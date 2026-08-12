import Redis from 'ioredis';

const redisUrl = process.env.REDIS_URL;

if (!redisUrl) {
    throw new Error("ERRORE: REDIS_URL non è definita nelle variabili d'ambiente. Il server non può avviarsi senza Redis.");
}

const redisClient = new Redis(redisUrl);

redisClient.on('error', (err) => {
    console.error('Errore client Redis:', err);
});
  
redisClient.on('connect', () => {
    console.log('Client Redis connesso con successo');
});

export { redisClient };