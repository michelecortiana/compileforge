import { pgTable, uuid, text, timestamp, index } from 'drizzle-orm/pg-core';

export const jobsTable = pgTable('jobs', {
    id: uuid('id').defaultRandom().primaryKey(),
    sourceCode: text('source_code').notNull(),
    language: text('language').notNull(),
    status: text('status').default('pending').notNull(),
    output: text('output'),
    outputIr: text('output_ir'),
    outputAsm: text('output_asm'),
    errorMessage: text('error_message'),
    createdAt: timestamp('created_at').defaultNow().notNull(),
    finishedAt: timestamp('finished_at'),
}, (table) => {
    // 👇 2. Aggiungi questo blocco per creare gli indici sulle colonne più interrogate
    return {
        createdAtIndex: index('created_at_idx').on(table.createdAt),
        statusIndex: index('status_idx').on(table.status),
        // Se in futuro farai query che filtrano per status E ordinano per data, un indice composito è ancora meglio:
        // statusCreatedAtIndex: index('status_created_at_idx').on(table.status, table.createdAt)
    };
});