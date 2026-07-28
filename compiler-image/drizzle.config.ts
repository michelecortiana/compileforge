import { defineConfig } from "drizzle-kit";

export default defineConfig({
  schema: "./app/db/schema.ts",
  out: "./app/db/migrations",
  dialect: "postgresql",
  dbCredentials: {
    url: "postgres://admin:pass@127.0.0.1:5432/compileforge?sslmode=disable",
  }
});