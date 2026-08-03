// 1. Inizializzazione: Importa i moduli nativi
import fs from 'fs/promises';
import { exec } from 'child_process';
import util from 'util';
import path from 'path'; 
import { eq } from 'drizzle-orm'; 
import client from 'prom-client';
import express from 'express';

// 1. Inizializzazione: Importa i tuoi moduli locali
import { initBroker } from './rabbitmq';
import { db } from './db';
import { jobsTable } from './schema';
import { redisClient } from './redis';

const execAsync = util.promisify(exec);

// === CONTROLLO DI SICUREZZA FAIL-FAST ===
const METRICS_TOKEN = process.env.METRICS_TOKEN;
if (!METRICS_TOKEN) {
    throw new Error("ERRORE CRITICO: METRICS_TOKEN non è definita nelle variabili d'ambiente.");
}

client.collectDefaultMetrics();

// Metrica 2: Esito delle compilazioni (Successo/Fallimento)
const jobResultsTotal = new client.Counter({
  name: 'compileforge_job_results_total',
  help: 'Totale dei job processati divisi per stato',
  labelNames: ['status'] // 'success' o 'failed'
});

// Metrica 3: Durata della compilazione
const jobDurationHistogram = new client.Histogram({
  name: 'compileforge_job_duration_seconds',
  help: 'Durata dell esecuzione del container di compilazione in secondi',
  buckets: [0.1, 0.5, 1, 2, 5, 10] // Fasce di tempo
});

// Mini-server per esporre le metriche sulla porta 9090
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
app.listen(9090, '0.0.0.0', () => console.log('Worker metrics esposte su porta 9090'));


async function startWorker() {
    try {
        const channel = await initBroker();
        
        await channel.prefetch(1);
        console.log('Worker avviato. In attesa di job nella coda "compile_jobs"...');

        channel.consume('compile_jobs', async (msg) => {
            if (!msg) return;

            try {
                const jobData = JSON.parse(msg.content.toString());
                const { jobId, sourceCode } = jobData;
                
                console.log(`Elaborazione Job ID: ${jobId} iniziata...`);
                const endTimer = jobDurationHistogram.startTimer();

                // 5. Creazione file e cartelle temporanee
                const filePath = path.join('/tmp', `${jobId}.c`);
                const outputDir = path.join('/tmp', `output-${jobId}`);

                await fs.writeFile(filePath, sourceCode);
                await fs.mkdir(outputDir, { recursive: true });
                // Diamo i permessi di scrittura completi alla cartella condivisa per evitare blocchi Docker
                await execAsync(`chmod 777 ${outputDir}`);
                console.log(`Ambiente preparato in /tmp per il job ${jobId}`);

                // 6. Esecuzione Docker (aggiunto --read-only e volume output)
                // 6. Esecuzione Docker (aggiunto --read-only, --tmpfs e volume output)
                const timeoutMs = 10000;
                const containerName = `runner-${jobId}`;
                
                // NOTA LO SPAZIO FINALE AGGIUNTO DOPO 10m
                const command = `docker run --rm --name ${containerName} --network none --memory=256m --cpus=0.5 --pids-limit=64 --cap-drop ALL --read-only --tmpfs /tmp:size=10m ` +
                `-v ${filePath}:/input/code.c:ro ` +   // sorgente in una cartella separata, sola lettura
                `-v ${outputDir}:/app ` +              // l'intera /app (dove il compilatore scrive) è ora il volume scrivibile
                `compiler-image /input/code.c --no-link`;

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

                // Leggiamo i file reali generati dal compilatore
                let outputIr = null;
                let outputAsm = null;

                if (finalStatus === 'completed') {
                    outputIr = await fs.readFile(path.join(outputDir, 'output.ir'), 'utf-8').catch(() => null);
                    outputAsm = await fs.readFile(path.join(outputDir, 'output.s'), 'utf-8').catch(() => null);
                    
                    if (!outputIr || !outputAsm) {
                        finalStatus = 'failed';
                        finalOutput += '\nErrore: Il compilatore non ha prodotto i file attesi.';
                    }
                }

                // 📊 2. REGISTRA I RISULTATI E FERMA IL TIMER QUI
                // Usiamo finalStatus per capire se è andato tutto a buon fine
                if (finalStatus === 'completed') {
                    jobResultsTotal.inc({ status: 'success' });
                } else {
                    jobResultsTotal.inc({ status: 'failed' });
                }
                endTimer(); // Ferma il timer e registra la durata nell'istogramma

                // 7. Aggiornamento DB
                await db.update(jobsTable)
                    .set({ 
                        status: finalStatus, 
                        errorMessage: finalStatus === 'failed' ? finalOutput : null,
                        outputIr: outputIr,
                        outputAsm: outputAsm,
                        finishedAt: new Date() 
                    })
                    .where(eq(jobsTable.id, jobId));

                // 8. Notifica Redis per il Gateway
                const updatePayload = JSON.stringify({
                    jobId,
                    status: finalStatus,
                    output: finalOutput,
                    outputIr: finalStatus === 'completed' ? outputIr : null,
                    outputAsm: finalStatus === 'completed' ? outputAsm : null,
                    errorMessage: finalStatus === 'failed' ? finalOutput : null
                });
                await redisClient.publish(`job_updates:${jobId}`, updatePayload);

                // Pulizia dei file temporanei
                await fs.unlink(filePath).catch(() => console.warn(`Impossibile eliminare ${filePath}`));
                await fs.rm(outputDir, { recursive: true, force: true }).catch(() => console.warn(`Impossibile eliminare ${outputDir}`));

                // 9. Conferma Finale (ACK)
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
            }
        }, { noAck: false });

    } catch (error) {
        console.error("Impossibile avviare il Worker:", error);
        process.exit(1);
    }
}

startWorker();