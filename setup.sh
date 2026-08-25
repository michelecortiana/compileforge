#!/usr/bin/env bash
set -euo pipefail

# ─────────────────────────────────────────────────────────────
# CompileForge — Automated Setup Script
#
# Clones/downloads assumed already done. This script:
#   1. Verifies prerequisites (Docker, Docker Compose V2)
#   2. Generates secure secrets and writes all .env files
#   3. Builds the sandboxed compiler image
#   4. Launches the full microservices stack (Gateway, Worker,
#      Postgres, Redis, RabbitMQ, Prometheus, Grafana, and the
#      containerized UI served via Nginx)
#   5. Waits for the Gateway to be healthy
#   6. Syncs the database schema (Drizzle)
#
# When it's done, the entire application is running — including
# the UI — with no further manual steps required.
#
# Usage:
#   ./setup.sh          # normal run, skips files that already exist
#   ./setup.sh --reset  # force-regenerate all secrets, .env files,
#                        # and rebuild the UI with the new API key
# ─────────────────────────────────────────────────────────────

RESET=false
if [[ "${1:-}" == "--reset" ]]; then
  RESET=true
fi

# ── Colors for output ──────────────────────────────────────────
GREEN='\033[0;32m'; YELLOW='\033[1;33m'; RED='\033[0;31m'; NC='\033[0m'
info()  { echo -e "${GREEN}==>${NC} $1"; }
warn()  { echo -e "${YELLOW}!!${NC} $1"; }
fail()  { echo -e "${RED}xx${NC} $1"; exit 1; }

# ── 1. Prerequisite checks ─────────────────────────────────────
info "Checking prerequisites..."

command -v docker >/dev/null 2>&1 || fail "Docker is not installed. Install it from https://docs.docker.com/get-docker/"

if ! docker compose version >/dev/null 2>&1; then
  fail "Docker Compose V2 is required (the 'docker compose' plugin). The legacy standalone 'docker-compose' v1 binary is not supported (this project uses 'depends_on' health conditions)."
fi

command -v openssl >/dev/null 2>&1 || fail "openssl is required to generate secure secrets."
command -v curl >/dev/null 2>&1 || fail "curl is required to wait for the Gateway health check."

info "Prerequisites OK."

# ── 2. Generate / load secrets ─────────────────────────────────
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT_DIR"

if [[ "$RESET" == true ]]; then
  warn "Reset mode: removing existing .env files and secrets."
  rm -f .env .env.docker ui/.env.local secrets/metrics_token.txt
fi

mkdir -p worker-tmp secrets

write_env_files_needed=false
for f in .env .env.docker ui/.env.local secrets/metrics_token.txt; do
  [[ -f "$f" ]] || write_env_files_needed=true
done

if [[ "$write_env_files_needed" == true ]]; then
  info "Generating secure secrets..."

  API_KEY=$(openssl rand -hex 32)
  METRICS_TOKEN=$(openssl rand -hex 32)
  HOST_TMP_DIR="$ROOT_DIR/worker-tmp"

  # Root .env — used by Docker Compose for variable interpolation
  # (both HOST_TMP_DIR for the Worker bind mount, and API_KEY, which
  # Compose needs to pass as a build arg into the UI image).
  if [[ ! -f .env ]]; then
    cat > .env <<EOF
HOST_TMP_DIR=$HOST_TMP_DIR
API_KEY=$API_KEY
EOF
    info "Created .env"
  fi

  # gateway/worker container secrets
  if [[ ! -f .env.docker ]]; then
    cat > .env.docker <<EOF
PORT=8080
HOST=0.0.0.0
ALLOWED_ORIGINS=http://localhost:5173

API_KEY=$API_KEY
METRICS_TOKEN=$METRICS_TOKEN

DATABASE_URL=postgresql://admin:pass@my_postgres_db:5432/compileforge
REDIS_URL=redis://redis:6379
RABBITMQ_URL=amqp://admin:pass@rabbitmq:5672
HOST_TMP_DIR=$HOST_TMP_DIR
EOF
    info "Created .env.docker"
  fi

  # UI dev-mode config (only used if you run 'npm run dev' manually
  # for hot-reload development — the containerized UI gets its key
  # baked in at build time instead, via the Compose build arg).
  if [[ ! -f ui/.env.local ]]; then
    EXISTING_KEY=$(grep -E '^API_KEY=' .env.docker | cut -d= -f2)
    cat > ui/.env.local <<EOF
VITE_API_BASE_URL=http://localhost:8080
VITE_API_KEY=$EXISTING_KEY
EOF
    info "Created ui/.env.local"
  fi

  # Prometheus scrape token — must match METRICS_TOKEN in .env.docker
  if [[ ! -f secrets/metrics_token.txt ]]; then
    EXISTING_TOKEN=$(grep -E '^METRICS_TOKEN=' .env.docker | cut -d= -f2)
    echo -n "$EXISTING_TOKEN" > secrets/metrics_token.txt
    info "Created secrets/metrics_token.txt"
  fi

  warn "Secrets were generated automatically. Grafana still uses its docker-compose default (admin/admin) — change it after first login if you plan to keep this environment around."
else
  info "All .env files already exist, skipping secret generation (use --reset to regenerate)."
fi

# ── 3. Build the sandboxed compiler image ──────────────────────
info "Building compiler-image (this runs once, cached afterwards)..."
docker build -t compiler-image ./compiler-image

# ── 4. Launch the infrastructure (including the containerized UI) ─
info "Launching the full stack — Postgres, Redis, RabbitMQ, Gateway, Worker, Prometheus, Grafana, and the UI..."
docker compose up -d --build

# ── 5. Wait for the Gateway to become healthy ──────────────────
info "Waiting for the Gateway to become healthy..."
ATTEMPTS=0
MAX_ATTEMPTS=60
until curl --silent --fail http://127.0.0.1:8080/health > /dev/null 2>&1; do
  ATTEMPTS=$((ATTEMPTS + 1))
  if [[ $ATTEMPTS -ge $MAX_ATTEMPTS ]]; then
    fail "Gateway did not become healthy in time. Run 'docker compose logs gateway' to investigate."
  fi
  sleep 2
done
info "Gateway is healthy."

# ── 6. Sync the database schema ────────────────────────────────
info "Syncing database schema with Drizzle..."
docker exec gateway npx drizzle-kit push --force

# ── Done ─────────────────────────────────────────────────────
echo
echo -e "${GREEN}CompileForge is up and running — no further steps needed.${NC}"
echo
echo "  App (UI):            http://127.0.0.1:5173"
echo "  Gateway API:         http://127.0.0.1:8080"
echo "  Grafana dashboards:  http://127.0.0.1:3000  (login: admin / admin)"
echo "  RabbitMQ mgmt UI:    http://127.0.0.1:15672 (login: admin / pass)"
echo
echo "To stop everything:"
echo "  docker compose down"
echo
echo "For UI development with hot-reload instead of the built container:"
echo "  cd ui && npm install && npm run dev   (then open http://localhost:5173)"
echo
