import amqp from 'amqplib';

const rabbitmqUrl = process.env.RABBITMQ_URL;

if (!rabbitmqUrl) {
    throw new Error("ERRORE: RABBITMQ_URL non è definita nelle variabili d'ambiente. Il server non può avviarsi senza RabbitMQ.");
}

export let channel: any;      
export let connection: any; 

export const initBroker = async () => {
    try {
        connection = await amqp.connect(rabbitmqUrl);
        channel = await connection.createChannel();
        
        await channel.assertQueue('compile_jobs_dlq', { durable: true });

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

export const closeBroker = async () => {
    try {
        if (channel) await channel.close();
        if (connection) await connection.close();
        console.log(' Connessione RabbitMQ chiusa.');
    } catch (err) {
        console.error('Errore durante la chiusura di RabbitMQ:', err);
    }
};