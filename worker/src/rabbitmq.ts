import amqp from 'amqplib';
import dotenv from 'dotenv';

dotenv.config();

const rabbitmqUrl = process.env.RABBITMQ_URL;

if (!rabbitmqUrl) {
    throw new Error("ERRORE: RABBITMQ_URL non è definita nelle variabili d'ambiente. Il server non può avviarsi senza RabbitMQ.");
}

export let channel: any;       // Ora TypeScript lo capisce
export let connection: any; // Ora TypeScript lo capisce

export const initBroker = async () => {
    try {
        connection = await amqp.connect(rabbitmqUrl); // 👈 MODIFICATO
        channel = await connection.createChannel();
        
        // 1. Dichiariamo PRIMA la Dead Letter Queue
        await channel.assertQueue('compile_jobs_dlq', { durable: true });

        // 2. Dichiariamo la coda principale
        await channel.assertQueue('compile_jobs', { 
            durable: true,
            arguments: {
                'x-dead-letter-exchange': '', 
                'x-dead-letter-routing-key': 'compile_jobs_dlq' 
            }
        });
        
        console.log('RabbitMQ connesso: coda "compile_jobs" e relativa DLQ pronte');
        return channel;
    } catch (error) {
        console.error('Errore di connessione a RabbitMQ:', error);
        throw error;
    }
};

// 👇 NUOVO: Funzione per chiudere tutto pulitamente
export const closeBroker = async () => {
    try {
        if (channel) await channel.close();
        if (connection) await connection.close();
        console.log('🔌 Connessione RabbitMQ chiusa.');
    } catch (err) {
        console.error('Errore durante la chiusura di RabbitMQ:', err);
    }
};