CREATE TYPE "public"."status" AS ENUM('pending', 'processing', 'completed', 'failed');--> statement-breakpoint
CREATE TABLE "jobs" (
	"id" uuid PRIMARY KEY DEFAULT gen_random_uuid() NOT NULL,
	"status" "status" DEFAULT 'pending' NOT NULL,
	"source_code" text NOT NULL,
	"created_at" timestamp DEFAULT now() NOT NULL,
	"finished_at" timestamp,
	"output_ir" text,
	"output_asm" text,
	"error_message" text
);
