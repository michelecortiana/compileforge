import { drizzle } from 'drizzle-orm/node-postgres'; // Usa l'adattatore per pg
import { Pool } from 'pg'; // Importa il Pool direttamente da qui
import * as schema from './schema';


const databaseUrl = process.env.DATABASE_URL;

if (!databaseUrl) {
    throw new Error("ERRORE: DATABASE_URL non è definita nelle variabili d'ambiente. Il server non può avviarsi senza un database.");
}

// Ora puoi inizializzare direttamente il Pool
const pool = new Pool({ connectionString: databaseUrl });

export const db = drizzle(pool, { schema });
export { pool };