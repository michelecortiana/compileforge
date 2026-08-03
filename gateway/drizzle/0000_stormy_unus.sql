CREATE TABLE "jobs" (
	"id" uuid PRIMARY KEY DEFAULT gen_random_uuid() NOT NULL,
	"source_code" text NOT NULL,
	"language" text NOT NULL,
	"status" text DEFAULT 'pending' NOT NULL,
	"output_ir" text,
	"output_asm" text,
	"error_message" text,
	"created_at" timestamp DEFAULT now() NOT NULL,
	"finished_at" timestamp
);
