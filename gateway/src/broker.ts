import * as amqp from 'amqplib';

const rabbitmqUrl = process.env.RABBITMQ_URL;

if (!rabbitmqUrl) {
    throw new Error("ERRORE: RABBITMQ_URL non è definita nelle variabili d'ambiente. Il server non può avviarsi senza RabbitMQ.");
}

// Usiamo 'any' per evitare i conflitti di TypeScript
export let channel: any;
export let connection: any;

export const initBroker = async () => {
    let retries = 5;
    
    while (retries > 0) {
        try {
            connection = await amqp.connect(rabbitmqUrl);
            channel = await connection.createChannel();
            
            // 1. Dichiara la DLQ
            await channel.assertQueue('compile_jobs_dlq', { durable: true });

            // 2. Dichiara la coda principale con i parametri per la DLQ
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
            console.error(`⏳ Errore di connessione a RabbitMQ. Tentativi rimasti: ${retries - 1}`);
            retries -= 1;
            if (retries === 0) throw error;
            await new Promise(res => setTimeout(res, 3000)); // Aspetta 3 secondi prima di riprovare
        }
    }
};

export const publishJob = async (jobId: string, sourceCode: string, language: string) => {
    if (!channel) {
        throw new Error('Canale RabbitMQ non inizializzato');
    }
    const payload = JSON.stringify({ jobId, sourceCode, language });
    channel.sendToQueue('compile_jobs', Buffer.from(payload), { persistent: true });
};

// NUOVO: Funzione per chiudere tutto pulitamente
export const closeBroker = async () => {
    try {
        if (channel) await channel.close();
        if (connection) await connection.close();
        console.log('🔌 Connessione RabbitMQ chiusa.');
    } catch (err) {
        console.error('Errore durante la chiusura di RabbitMQ:', err);
    }
};