import { pgTable, uuid, text, timestamp } from 'drizzle-orm/pg-core';

export const jobsTable = pgTable('jobs', {
    id: uuid('id').defaultRandom().primaryKey(),
    sourceCode: text('source_code').notNull(),
    language: text('language').notNull(),
    status: text('status').default('pending').notNull(),
    output: text('output'),          // 👈 mancava questa
    outputIr: text('output_ir'),
    outputAsm: text('output_asm'),
    errorMessage: text('error_message'),
    createdAt: timestamp('created_at').defaultNow().notNull(),
    finishedAt: timestamp('finished_at'),
});