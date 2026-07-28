import { pgTable, uuid, text, timestamp, pgEnum } from "drizzle-orm/pg-core";

export const statusEnum = pgEnum('status', ['pending', 'processing', 'completed', 'failed']);

export const jobs = pgTable('jobs', {
    id: uuid('id').defaultRandom().primaryKey(),
    status: statusEnum('status').default('pending').notNull(),
    sourceCode: text('source_code').notNull(),
    createdAt: timestamp('created_at').defaultNow().notNull(),
    finishedAt: timestamp('finished_at'),
    outputIr: text('output_ir'),
    outputAsm: text('output_asm'),
    errorMessage: text('error_message')
});