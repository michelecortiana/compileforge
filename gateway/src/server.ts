import Fastify, { FastifyInstance } from 'fastify';
import { sql, eq } from 'drizzle-orm';
import { PassThrough } from 'stream';
import { db } from './db';
import { redisClient } from './redis';
import { initBroker, publishJob } from './broker';
import { jobsTable } from './schema';
import fastifyRateLimit from '@fastify/rate-limit';
import client from 'prom-client';

// === CONTROLLO DI SICUREZZA FAIL-FAST ===
const API_KEY = process.env.API_KEY;
if (!API_KEY) {
    throw new Error("ERRORE CRITICO: API_KEY non è definita nelle variabili d'ambiente.");
}

// AGGIUNGI QUESTO BLOCCO:
const METRICS_TOKEN = process.env.METRICS_TOKEN;
if (!METRICS_TOKEN) {
    throw new Error("ERRORE CRITICO: METRICS_TOKEN non è definita nelle variabili d'ambiente.");
}

// Abilita le metriche di default di NodeJS (uso RAM, CPU, event loop)
client.collectDefaultMetrics();

// Metrica 1: Job Totali Ricevuti
const jobsSubmittedTotal = new client.Counter({
  name: 'compileforge_jobs_submitted_total',
  help: 'Numero totale di richieste di compilazione ricevute'
});

const app = Fastify({
    logger: true,
    bodyLimit: 102400
});

// Regex semplice per validare il formato UUID nei parametri di route
const UUID_REGEX = /^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/i;

function isValidUuid(value: string): boolean {
    return UUID_REGEX.test(value);
}

async function apiRoutes(fastify: FastifyInstance) {

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

    fastify.post('/compile', { schema: compileSchema }, async (request, reply) => {
        // La variabile API_KEY globale ora viene usata in modo sicuro
        const clientKey = request.headers['x-api-key'];

        if (!clientKey || clientKey !== API_KEY) {
            request.log.warn('Tentativo di accesso non autorizzato alla rotta /compile');
            return reply.status(401).send({ error: 'Unauthorized: Missing or invalid X-API-Key header' });
        }

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

    fastify.get('/status/:job_id', async (request, reply) => {
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

    fastify.get('/status/:job_id/stream', async (request, reply) => {
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

        request.raw.on('close', () => {
            cleanup();
        });

        return stream;
    });

    fastify.get('/metrics', async (request, reply) => {
        // HARDENING: Protezione dell'endpoint /metrics
        const authHeader = request.headers.authorization;
        
        // RIMOSSA LA RIGA CON IL FALLBACK. Usa direttamente la costante globale
        if (authHeader !== `Bearer ${METRICS_TOKEN}`) {
            request.log.warn('Tentativo di accesso non autorizzato a /metrics');
            return reply.status(401).send({ error: 'Unauthorized: Invalid metrics token' });
        }

        reply.header('Content-Type', client.register.contentType);
        return client.register.metrics();
    });
}

const start = async () => {
    try {
        console.log('⏳ Avvio dei controlli di sistema...');

        await db.execute(sql`SELECT 1`);
        console.log('Connessione a Postgres verificata');

        await redisClient.ping();
        console.log('Connessione a Redis verificata');

        // --- INIZIO LOGICA DI RETRY ---
        let retries = 5;
        while (retries > 0) {
            try {
                await initBroker();
                console.log('✅ Connessione a RabbitMQ verificata');
                break;
            } catch (error) {
                console.error(`⏳ Errore RabbitMQ nel Gateway. Tentativi rimasti: ${retries - 1}`);
                retries -= 1;
                if (retries === 0) throw error;
                await new Promise(res => setTimeout(res, 3000));
            }
        }
        // --- FINE LOGICA DI RETRY ---

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