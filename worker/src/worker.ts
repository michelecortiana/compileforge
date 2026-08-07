// 1. Inizializzazione: Importa i moduli nativi
import fs from 'fs/promises';
import { exec } from 'child_process';
import util from 'util';
import path from 'path'; 
import { eq } from 'drizzle-orm'; 
import client from 'prom-client';
import express from 'express';

// 👇 MODIFICATO: Importiamo le nuove funzioni di spegnimento
import { initBroker, closeBroker, channel as rabbitChannel } from './rabbitmq';
import { db, pool } from './db'; 
import { jobsTable } from './schema';
import { redisClient } from './redis';

const execAsync = util.promisify(exec);

// === CONTROLLI DI SICUREZZA E VARIABILI GLOBALI ===
const METRICS_TOKEN = process.env.METRICS_TOKEN;
if (!METRICS_TOKEN) {
    throw new Error("ERRORE CRITICO: METRICS_TOKEN non è definita nelle variabili d'ambiente.");
}

const CONTAINER_TMP_BASE = '/tmp/compileforge';
const HOST_TMP_BASE = process.env.HOST_TMP_DIR; 
if (!HOST_TMP_BASE) {
    throw new Error("ERRORE CRITICO: HOST_TMP_DIR non definita — necessaria per Docker-outside-of-Docker.");
}

// 👇 NUOVO: Variabili per tracciare lo stato di spegnimento
let activeJobs = 0;
let isShuttingDown = false;

client.collectDefaultMetrics();

const jobResultsTotal = new client.Counter({
  name: 'compileforge_job_results_total',
  help: 'Totale dei job processati divisi per stato',
  labelNames: ['status'] 
});

const jobDurationHistogram = new client.Histogram({
  name: 'compileforge_job_duration_seconds',
  help: 'Durata dell esecuzione del container di compilazione in secondi',
  buckets: [0.1, 0.5, 1, 2, 5, 10]
});

// Mini-server per esporre le metriche
const app = express();
app.get('/metrics', async (req, res) => {
    const authHeader = req.headers.authorization;

    if (authHeader !== `Bearer ${METRICS_TOKEN}`) {
        console.warn('Tentativo di accesso non autorizzato a /metrics sul Worker');
        return res.status(401).send({ error: 'Unauthorized: Invalid metrics token' });
    }

    res.set('Content-Type', client.register.contentType);
    res.end(await client.register.metrics());
});

// 👇 MODIFICATO: Salviamo l'istanza del server per poterlo chiudere
const metricsServer = app.listen(9090, '0.0.0.0', () => console.log('Worker metrics esposte su porta 9090'));

async function startWorker() {
    try {
        let channel;
        let retries = 5;
        
        while (retries > 0) {
            try {
                channel = await initBroker();
                console.log('✅ Connesso a RabbitMQ (Worker)!');
                break;
            } catch (error) {
                console.error(`⏳ Errore RabbitMQ nel Worker. Tentativi rimasti: ${retries - 1}`);
                retries -= 1;
                if (retries === 0) throw error;
                await new Promise(res => setTimeout(res, 3000)); 
            }
        }

        if (!channel) throw new Error("Canale RabbitMQ non inizializzato");

        await channel.prefetch(1);
        console.log('Worker avviato. In attesa di job nella coda "compile_jobs"...');

        // 👇 MODIFICATO: Estraiamo il consumerTag per poter cancellare l'iscrizione
        const { consumerTag } = await channel.consume('compile_jobs', async (msg: any) => {
            if (!msg) return;

            // 👇 NUOVO: Se stiamo spegnendo, rimettiamo in coda il job per un altro worker
            if (isShuttingDown) {
                channel.nack(msg, false, true); 
                return;
            }

            activeJobs++; // Registriamo che un job è in corso

            let filePath = '';
            let outputDir = '';

            try {
                const jobData = JSON.parse(msg.content.toString());
                const { jobId, sourceCode } = jobData;
                
                console.log(`Elaborazione Job ID: ${jobId} iniziata...`);
                
                const endTimer = jobDurationHistogram.startTimer();
                const createdAt = new Date().toISOString(); 

                filePath = path.join(CONTAINER_TMP_BASE, `${jobId}.c`);
                outputDir = path.join(CONTAINER_TMP_BASE, `output-${jobId}`);
                const downloadDir = '/app/downloads'; 

                await fs.mkdir(CONTAINER_TMP_BASE, { recursive: true }); 
                await fs.writeFile(filePath, sourceCode);
                await fs.mkdir(outputDir, { recursive: true });
                await fs.mkdir(downloadDir, { recursive: true }); 
                await execAsync(`chmod 777 ${outputDir}`);

                const timeoutMs = 10000;
                const containerName = `runner-${jobId}`;
                
                const hostFilePath = path.join(HOST_TMP_BASE as string, `${jobId}.c`);
                const hostOutputDir = path.join(HOST_TMP_BASE as string, `output-${jobId}`);

                const command = `docker run --rm --name ${containerName} --memory=256m --cpus=0.5 ` +
                    `-v ${hostFilePath}:/input/code.c:ro ` +
                    `-v ${hostOutputDir}:/app ` +
                    `-w /app ` +  
                    `compiler-image /input/code.c`;

                let finalStatus: 'completed' | 'failed' = 'completed';
                let finalOutput = '';

                try {
                    const { stdout, stderr } = await execAsync(command, { timeout: timeoutMs });
                    finalOutput = stdout || stderr;
                } catch (dockerError: any) {
                    finalStatus = 'failed';
                    finalOutput = dockerError.stderr || dockerError.stdout || dockerError.message;
                    if (dockerError.killed) {
                        finalOutput = `Errore: Tempo limite di esecuzione (${timeoutMs}ms) superato.`;
                    }
                }

                let outputIr = null;
                let outputAsm = null;
                let downloadUrl = null;

                if (finalStatus === 'completed') {
                    outputIr = await fs.readFile(path.join(outputDir, 'output.ir'), 'utf-8').catch(() => null);
                    outputAsm = await fs.readFile(path.join(outputDir, 'output.s'), 'utf-8').catch(() => null);
                    
                    if (!outputIr || !outputAsm) {
                        finalStatus = 'failed';
                        finalOutput += '\nErrore: Il compilatore non ha prodotto i file attesi.';
                    } else {
                        try {
                            const binPath = path.join(outputDir, 'output.exe'); 
                            const destPath = path.join(downloadDir, `${jobId}.exe`); 
                            await fs.copyFile(binPath, destPath);
                            downloadUrl = `http://localhost:8080/downloads/${jobId}.exe`; 
                        } catch (e) {
                            console.warn("Eseguibile output.exe non trovato, errore nel linking.");
                        }
                    }
                }

                const finishedAt = new Date().toISOString(); 

                if (finalStatus === 'completed') {
                    jobResultsTotal.inc({ status: 'success' });
                } else {
                    jobResultsTotal.inc({ status: 'failed' });
                }
                endTimer(); 

                await db.update(jobsTable)
                    .set({ 
                        status: finalStatus, 
                        errorMessage: finalStatus === 'failed' ? finalOutput : null,
                        outputIr: outputIr,
                        outputAsm: outputAsm,
                        finishedAt: new Date(finishedAt) 
                    })
                    .where(eq(jobsTable.id, jobId));

                const updatePayload = JSON.stringify({
                    jobId,
                    status: finalStatus,
                    output: finalOutput,
                    irCode: outputIr,      
                    assembly: outputAsm,   
                    createdAt,
                    finishedAt,
                    downloadUrl,
                    errorMessage: finalStatus === 'failed' ? finalOutput : null
                });
                await redisClient.publish(`job_updates:${jobId}`, updatePayload);

                channel.ack(msg);
                console.log(`Job ID: ${jobId} completato [${finalStatus}] e notificato.`);

            } catch (error) {
                console.error('Errore critico durante l\'elaborazione del job:', error);
                
                let failedJobId = 'Sconosciuto';
                try {
                    failedJobId = JSON.parse(msg.content.toString()).jobId;
                } catch (e) {}

                const headers = msg.properties.headers || {};
                const retryCount = (headers['x-retry-count'] as number) || 0;

                if (retryCount < 3) {
                    console.log(`Tentativo ${retryCount + 1}/3 fallito per Job ID: ${failedJobId}. Rimetto in coda...`);
                    channel.publish('', 'compile_jobs', msg.content, {
                        headers: { ...headers, 'x-retry-count': retryCount + 1 }
                    });
                    channel.ack(msg);
                } else {
                    console.error(`Job ID: ${failedJobId} fallito per 3 volte. Spostamento definitivo in DLQ.`);
                    channel.nack(msg, false, false); 
                }
            } finally {
                activeJobs--; // 👇 NUOVO: Rimuoviamo il job dai processi in corso
                if (filePath) {
                    await fs.unlink(filePath).catch(() => console.warn(`Nessun file ${filePath} da pulire.`));
                }
                if (outputDir) {
                    await fs.rm(outputDir, { recursive: true, force: true }).catch(() => console.warn(`Nessuna cartella ${outputDir} da pulire.`));
                }
            }
        }, { noAck: false });

        // === 👇 INIZIO LOGICA GRACEFUL SHUTDOWN 👇 ===
        const shutdown = async (signal: string) => {
            console.log(`\n🛑 Ricevuto segnale ${signal}. Avvio spegnimento pulito (Graceful Shutdown)...`);
            isShuttingDown = true;

            // 1. Chiudi il server delle metriche
            if (metricsServer) {
                metricsServer.close(() => console.log('🔌 Server metriche chiuso.'));
            }

            // 2. Smetti di accettare nuovi job
            if (rabbitChannel && consumerTag) {
                await rabbitChannel.cancel(consumerTag);
                console.log('🛑 Stop ricezione nuovi job da RabbitMQ.');
            }

            // 3. Aspetta la fine del job in corso (max 15 secondi)
            const timeout = 15000; 
            const startWait = Date.now();
            while (activeJobs > 0 && (Date.now() - startWait) < timeout) {
                console.log(`⏳ Attesa fine elaborazione... (${activeJobs} job in corso)`);
                await new Promise(resolve => setTimeout(resolve, 1000));
            }

            if (activeJobs > 0) {
                console.warn(`⚠️ Spegnimento forzato: ${activeJobs} job non terminati in tempo.`);
            } else {
                console.log('✅ Tutti i job in corso sono terminati correttamente.');
            }

            // 4. Disconnetti in modo sicuro i database
            await closeBroker();
            await pool.end();
            console.log('🔌 Connessione Postgres chiusa.');
            await redisClient.quit();
            console.log('🔌 Connessione Redis chiusa.');

            console.log('👋 Worker spento con successo.');
            process.exit(0);
        };

        // Aggancia la funzione di spegnimento ai comandi di arresto del sistema
        process.on('SIGINT', () => shutdown('SIGINT'));   // CTRL+C
        process.on('SIGTERM', () => shutdown('SIGTERM')); // Docker stop
        // === 👆 FINE LOGICA GRACEFUL SHUTDOWN 👆 ===

    } catch (error) {
        console.error("Impossibile avviare il Worker:", error);
        process.exit(1);
    }
}

startWorker();