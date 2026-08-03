import amqp from 'amqplib';
import dotenv from 'dotenv';

dotenv.config();
const rabbitmqUrl = process.env.RABBITMQ_URL;

if (!rabbitmqUrl) {
    throw new Error("ERRORE: RABBITMQ_URL non è definita nelle variabili d'ambiente. Il server non può avviarsi senza RabbitMQ.");
}

export let channel: amqp.Channel;

export const initBroker = async () => {
    try {
        const connection = await amqp.connect(rabbitmqUrl);
        channel = await connection.createChannel();
        
        // 1. Dichiara la DLQ (esattamente come nel worker)
        await channel.assertQueue('compile_jobs_dlq', { durable: true });

        // 2. Dichiara la coda principale con i parametri per la DLQ (esattamente come nel worker)
        await channel.assertQueue('compile_jobs', { 
            durable: true,
            arguments: {
                'x-dead-letter-exchange': '',
                'x-dead-letter-routing-key': 'compile_jobs_dlq'
            }
        });

        console.log('RabbitMQ connesso e coda "compile_jobs" pronta (con DLQ allineata)');
        return channel;
    } catch (error) {
        console.error('Errore di connessione a RabbitMQ:', error);
        throw error;
    }
};

// jobId ora è una stringa (uuid), non più un numero
export const publishJob = async (jobId: string, sourceCode: string, language: string) => {
    if (!channel) {
        throw new Error('Canale RabbitMQ non inizializzato');
    }
    const payload = JSON.stringify({ jobId, sourceCode, language });
    channel.sendToQueue('compile_jobs', Buffer.from(payload), { persistent: true });
};