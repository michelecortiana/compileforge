import Fastify, { FastifyInstance } from 'fastify';
import { sql, eq } from 'drizzle-orm';
import { PassThrough } from 'stream';
import { db, pool } from './db';
import { redisClient } from './redis';
import { initBroker, publishJob, closeBroker, channel } from './broker';
import { jobsTable } from './schema';
import fastifyRateLimit from '@fastify/rate-limit';
import cors from '@fastify/cors';
import * as promClient from 'prom-client';
const collectDefaultMetrics = promClient.collectDefaultMetrics;
const Counter = promClient.Counter;
// Il magico "as any" fa chiudere la bocca a TypeScript una volta per tutte
const register = (promClient as any).register;
import path from 'path';
import fs from 'fs'; 
import crypto from 'crypto';



// === CONTROLLO DI SICUREZZA FAIL-FAST ===
const API_KEY = process.env.API_KEY;
if (!API_KEY) {
    throw new Error("ERRORE CRITICO: API_KEY non è definita nelle variabili d'ambiente.");
}

const METRICS_TOKEN = process.env.METRICS_TOKEN;
if (!METRICS_TOKEN) {
    throw new Error("ERRORE CRITICO: METRICS_TOKEN non è definita nelle variabili d'ambiente.");
}

// Abilita le metriche di default di NodeJS (uso RAM, CPU, event loop)
collectDefaultMetrics();

const jobsSubmittedTotal = new Counter({
    name: 'compileforge_jobs_submitted_total',
    help: 'Numero totale di richieste di compilazione ricevute'
});
const app = Fastify({
    logger: {
        serializers: {
            req(request) {
                return {
                    method: request.method,
                    url: request.url.replace(/([?&]key=)[^&]+/, '$1REDACTED'),
                    hostname: request.hostname,
                    remoteAddress: request.ip,
                    remotePort: request.socket.remotePort
                };
            }
        }
    },
    bodyLimit: 102400
});

// Regex semplice per validare il formato UUID nei parametri di route
const UUID_REGEX = /^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/i;

function isValidUuid(value: string): boolean {
    return UUID_REGEX.test(value);
}

function safeCompare(input: string | undefined, secret: string): boolean {
    if (!input) return false;
    
    const inputBuffer = Buffer.from(input);
    const secretBuffer = Buffer.from(secret);

    if (inputBuffer.length !== secretBuffer.length) {
        crypto.timingSafeEqual(secretBuffer, secretBuffer);
        return false;
    }

    return crypto.timingSafeEqual(inputBuffer, secretBuffer);
}
async function apiRoutes(fastify: FastifyInstance) {

    // === MIDDLEWARE DI AUTENTICAZIONE ===
    // Controlla l'header x-api-key OPPURE il query parameter ?key=
    const requireApiKey = async (request: any, reply: any) => {
        const headerKeyRaw = request.headers['x-api-key'];
        const headerKey = Array.isArray(headerKeyRaw) ? headerKeyRaw[0] : headerKeyRaw;
        const queryKey = (request.query as { key?: string }).key;
        const API_KEY = process.env.API_KEY as string;
        // 👇 4. USATO IL CONFRONTO A TEMPO COSTANTE
        const isValid = safeCompare(headerKey ?? '', API_KEY) || safeCompare(queryKey ?? '', API_KEY);

        if (!isValid) {
            request.log.warn(`Accesso negato alla rotta: ${request.url}`);
            return reply.status(401).send({ error: 'Unauthorized: API Key mancante o non valida' });
        }
    };

    // === HEALTH CHECK ENDPOINT (Rimane aperto per i sistemi di monitoraggio) ===
    fastify.get('/health', async (request, reply) => {
        try {
            // 1. Ping al Database (esegue una query vuota leggerissima)
            await db.execute(sql`SELECT 1`);
            
            // 2. Ping a Redis
            await redisClient.ping();
            
            // 3. Controllo RabbitMQ (CONTROLLO ATTIVO)
            if (!channel) {
                throw new Error('Variabile canale RabbitMQ non inizializzata');
            }
            // checkQueue verifica se la connessione è viva e se la coda esiste.
            // Se il canale si è chiuso o RabbitMQ è irraggiungibile, lancerà un'eccezione.
            await channel.checkQueue('compile_jobs');

            // Se arriviamo qui, l'infrastruttura sta benissimo!
            return reply.status(200).send({ 
                status: 'ok', 
                timestamp: new Date().toISOString(),
                services: { database: 'up', redis: 'up', rabbitmq: 'up' }
            });

        } catch (error: any) {
            request.log.error(`Health check fallito: ${error.message}`);
            // Ritorna 503 (Service Unavailable) se anche solo un servizio è giù
            return reply.status(503).send({ 
                status: 'error', 
                message: 'Infrastruttura degradata',
                details: error.message 
            });
        }
    });

    const compileSchema = {
        body: {
            type: 'object',
            required: ['source_code', 'language'],
            properties: {
                source_code: { type: 'string', minLength: 1 },
                language: { type: 'string', minLength: 1 }
            }
        }
    };

    // 👇 Rotta protetta dal middleware
    fastify.post('/compile', { schema: compileSchema, preHandler: requireApiKey }, async (request, reply) => {
        jobsSubmittedTotal.inc(); 

        const body = request.body as { source_code: string; language: string };
        let newJobId: string | null = null;

        try {
            const [newJob] = await db.insert(jobsTable)
                .values({ sourceCode: body.source_code, language: body.language })
                .returning({ id: jobsTable.id });

            newJobId = newJob.id;

            try {
                await publishJob(newJob.id, body.source_code, body.language);
            } catch (queueError: any) {
                fastify.log.error(queueError);
                await db.update(jobsTable)
                    .set({
                        status: 'failed',
                        errorMessage: 'Impossibile mettere in coda il job (RabbitMQ non raggiungibile)',
                        finishedAt: new Date()
                    })
                    .where(eq(jobsTable.id, newJob.id));
                throw queueError;
            }

            return reply.status(202).send({ job_id: newJob.id });

        } catch (error: any) {
            fastify.log.error(error);
            return reply.status(500).send({
                error: 'Errore interno del server',
                job_id: newJobId 
            });
        }
    });

    // 👇 Rotta protetta dal middleware
    fastify.get('/status/:job_id', { preHandler: requireApiKey }, async (request, reply) => {
        const { job_id } = request.params as { job_id: string };
        
        if (!isValidUuid(job_id)) {
            return reply.status(400).send({ error: 'L\'ID del job deve essere un UUID valido' });
        }

        try {
            const cacheKey = 'job_status:' + job_id;
            const cachedResult = await redisClient.get(cacheKey);

            if (cachedResult) {
                return reply.send(JSON.parse(cachedResult));
            }

            const [job] = await db.select().from(jobsTable).where(eq(jobsTable.id, job_id));

            if (!job) {
                return reply.status(404).send({ error: 'Job non trovato' });
            }

            await redisClient.set(cacheKey, JSON.stringify(job), 'EX', 15);
            return reply.send(job);

        } catch (error: any) {
            fastify.log.error(error);
            return reply.status(500).send({ error: 'Errore interno del server' });
        }
    });

    // 👇 Rotta protetta dal middleware
    // 👇 Rotta protetta dal middleware
    fastify.get('/status/:job_id/stream', { preHandler: requireApiKey }, async (request, reply) => {
        const { job_id } = request.params as { job_id: string };

        if (!isValidUuid(job_id)) {
            return reply.status(400).send({ error: 'L\'ID del job deve essere un UUID valido' });
        }

        const stream = new PassThrough();

        reply.header('Content-Type', 'text/event-stream');
        reply.header('Cache-Control', 'no-cache');
        reply.header('Connection', 'keep-alive');

        const [existingJob] = await db.select().from(jobsTable).where(eq(jobsTable.id, job_id));

        if (!existingJob) {
            stream.write(`data: ${JSON.stringify({ error: 'Job non trovato' })}\n\n`);
            stream.end();
            return stream;
        }

        if (existingJob.status === 'completed' || existingJob.status === 'failed') {
            stream.write(`data: ${JSON.stringify(existingJob)}\n\n`);
            stream.end();
            return stream;
        }

        stream.write(`data: ${JSON.stringify({ status: 'connected', message: 'In attesa di aggiornamenti...' })}\n\n`);

        const subscriber = redisClient.duplicate();
        const channelName = `job_updates:${job_id}`;

        let isCleanedUp = false;

        const cleanup = async () => {
            if (isCleanedUp) return;
            isCleanedUp = true;
            try {
                await subscriber.unsubscribe(channelName);
                await subscriber.quit();
                stream.end();
            } catch (err) {
                fastify.log.error('Errore chiusura stream SSE:', err);
            }
        };

        // 1. Ci iscriviamo prima a Redis per non perdere nulla da questo momento in poi
        await subscriber.subscribe(channelName);

        subscriber.on('message', (channel: string, message: string) => {
            if (channel === channelName) {
                stream.write(`data: ${message}\n\n`);
                try {
                    const parsedMessage = JSON.parse(message);
                    if (parsedMessage.status === 'completed' || parsedMessage.status === 'failed') {
                        cleanup();
                    }
                } catch (e) { /* Ignora */ }
            }
        });

        // 👇 FIX ANTI-RACE CONDITION: 
        // Dopo l'iscrizione, rifacciamo un controllo lampo sul DB. 
        // Se il worker ha finito il job proprio durante la fase di setup, lo becchiamo ora.
        const [latestJob] = await db.select().from(jobsTable).where(eq(jobsTable.id, job_id));
        if (latestJob && (latestJob.status === 'completed' || latestJob.status === 'failed')) {
            stream.write(`data: ${JSON.stringify(latestJob)}\n\n`);
            await cleanup();
            return stream;
        }

        request.raw.on('close', () => {
            cleanup();
        });

        return stream;
    });

    // Endpoint Metriche (Già protetto col suo token specifico)
    // Endpoint Metriche (Già protetto col suo token specifico)
    fastify.get('/metrics', async (request, reply) => {
        const authHeader = request.headers.authorization;
        
        if (authHeader !== `Bearer ${METRICS_TOKEN}`) {
            request.log.warn('Tentativo di accesso non autorizzato a /metrics');
            return reply.status(401).send({ error: 'Unauthorized: Invalid metrics token' });
        }

        // Usa register direttamente
        reply.header('Content-Type', register.contentType);
        
        // Usa register direttamente
        return reply.send(await register.metrics());
    });

    // 👇 Rotta protetta dal middleware
    fastify.get('/download/:job_id', { preHandler: requireApiKey }, async (request, reply) => {
        const { job_id } = request.params as { job_id: string };

        if (!isValidUuid(job_id)) {
            return reply.status(400).send({ error: 'L\'ID del job deve essere un UUID valido' });
        }

        // 👇 FIX 1: Cambiato in .out
        const filePath = path.join('/app/downloads', `${job_id}.out`);

        if (!fs.existsSync(filePath)) {
            request.log.error(`Tentativo di download fallito, file mancante: ${filePath}`);
            return reply.status(404).send({ error: 'File eseguibile non trovato o compilazione fallita' });
        }

        try {
            // 👇 FIX 2: Cambiato il nome suggerito al browser in output.out
            reply.header('Content-Disposition', 'attachment; filename="output.out"');
            reply.header('Content-Type', 'application/octet-stream'); 

            const stream = fs.createReadStream(filePath);
            return reply.send(stream);
            
        } catch (error: any) {
            request.log.error(`Errore durante lo stream del file: ${error.message}`);
            return reply.status(500).send({ error: 'Errore interno del server durante il download' });
        }
    });
}

const start = async () => {
    try {
        console.log('⏳ Avvio dei controlli di sistema...');

        let dbRetries = 5;
        while (dbRetries > 0) {
            try {
                await db.execute(sql`SELECT 1`);
                console.log('✅ Connessione a Postgres verificata');
                break;
            } catch (error) {
                console.error(`⏳ Errore connessione Postgres. Tentativi rimasti: ${dbRetries - 1}`);
                dbRetries -= 1;
                if (dbRetries === 0) throw error;
                await new Promise(res => setTimeout(res, 3000));
            }
        }

        let redisRetries = 5;
        while (redisRetries > 0) {
            try {
                await redisClient.ping();
                console.log('✅ Connessione a Redis verificata');
                break;
            } catch (error) {
                console.error(`⏳ Errore connessione Redis. Tentativi rimasti: ${redisRetries - 1}`);
                redisRetries -= 1;
                if (redisRetries === 0) throw error;
                await new Promise(res => setTimeout(res, 3000));
            }
        }

        console.log('⏳ Connessione a RabbitMQ in corso...');
        await initBroker(); 
        console.log('✅ Connessione a RabbitMQ verificata');
        
        await app.register(cors, {
            origin: '*', 
            methods: ['GET', 'POST', 'OPTIONS'],
            allowedHeaders: ['Content-Type', 'x-api-key', 'Authorization']
        });
        console.log('🌐 CORS abilitato per le comunicazioni cross-origin');

        await app.register(fastifyRateLimit, {
            max: 100,
            timeWindow: '1 minute',
            redis: redisClient
        });
        console.log('🛡️ Rate Limiter attivato e agganciato a Redis');

        await app.register(apiRoutes);

        const PORT = process.env.PORT ? Number(process.env.PORT) : 8080;
        const HOST = process.env.HOST || '0.0.0.0';

        await app.listen({ port: PORT, host: HOST });
        app.log.info(`Gateway in ascolto su http://${HOST}:${PORT}`);
    } catch (err) {
        console.error('ERRORE CRITICO DURANTE L\'AVVIO:', err);
        process.exit(1);
    }
};

start();

const shutdown = async (signal: string) => {
    console.log(`\nRicevuto segnale ${signal}. Avvio Graceful Shutdown del Gateway...`);
    
    try {
        await app.close();
        console.log('Fastify chiuso: nessuna nuova richiesta HTTP accettata. Connessioni attive terminate.');

        await closeBroker();

        await pool.end();
        console.log('🔌 Connessione Postgres chiusa.');

        await redisClient.quit();
        console.log('🔌 Connessione Redis chiusa.');

        console.log('👋 Gateway spento con successo.');
        process.exit(0);
    } catch (err) {
        console.error('Errore durante lo spegnimento del Gateway:', err);
        process.exit(1);
    }
};

process.on('SIGINT', () => shutdown('SIGINT'));
process.on('SIGTERM', () => shutdown('SIGTERM'));