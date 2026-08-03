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
        
        // 1. Dichiariamo PRIMA la Dead Letter Queue (dove finiscono i job falliti)
        await channel.assertQueue('compile_jobs_dlq', { durable: true });

        // 2. Dichiariamo la coda principale dicendole di scartare i messaggi nella DLQ
        await channel.assertQueue('compile_jobs', { 
            durable: true,
            arguments: {
                'x-dead-letter-exchange': '', // Usiamo l'exchange di default
                'x-dead-letter-routing-key': 'compile_jobs_dlq' // La coda di destinazione per gli scarti
            }
        });
        
        console.log('RabbitMQ connesso: coda "compile_jobs" e relativa DLQ pronte');
        return channel;
    } catch (error) {
        console.error('Errore di connessione a RabbitMQ:', error);
        throw error;
    }
};