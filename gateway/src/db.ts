import { drizzle } from 'drizzle-orm/node-postgres'; 
import { Pool } from 'pg'; 
import * as schema from './schema';


const databaseUrl = process.env.DATABASE_URL;

if (!databaseUrl) {
    throw new Error("ERRORE: DATABASE_URL non è definita nelle variabili d'ambiente. Il server non può avviarsi senza un database.");
}
  const pool = new Pool({ connectionString: databaseUrl });

export const db = drizzle(pool, { schema });
export { pool };