#!/usr/bin/env bash
set -euo pipefail


# CompileForge — Script di configurazione automatizzata
#
# Si presuppone che il clone/download sia già stato effettuato. Questo script:
#   1. Verifica i prerequisiti (Docker, Docker Compose V2)
#   2. Genera segreti sicuri e scrive tutti i file .env
#   3. Compila l'immagine del compilatore sandboxed
#   4. Avvia l'intero stack di microservizi (Gateway, Worker,
#      Postgres, Redis, RabbitMQ, Prometheus, Grafana e la
#      UI containerizzata servita tramite Nginx)
#   5. Attende che il Gateway sia in stato "healthy" (operativo)
#   6. Sincronizza lo schema del database (Drizzle)
#
# Al termine, l'intera applicazione sarà in esecuzione — inclusa
# la UI — senza richiedere ulteriori passaggi manuali.
#
# Utilizzo:
#   ./setup.sh          # esecuzione normale, ignora i file già esistenti
#   ./setup.sh --reset  # forza la rigenerazione di tutti i segreti, dei file .env,
#                        # e ricompila la UI con la nuova chiave API


RESET=false
if [[ "${1:-}" == "--reset" ]]; then
  RESET=true
fi

#Colori per l'output 
GREEN='\033[0;32m'; YELLOW='\033[1;33m'; RED='\033[0;31m'; NC='\033[0m'
info()  { echo -e "${GREEN}==>${NC} $1"; }
warn()  { echo -e "${YELLOW}!!${NC} $1"; }
fail()  { echo -e "${RED}xx${NC} $1"; exit 1; }

#1. Controllo dei prerequisiti
info "Controllo dei prerequisiti in corso..."

command -v docker >/dev/null 2>&1 || fail "Docker non è installato. Installalo da https://docs.docker.com/get-docker/"

if ! docker compose version >/dev/null 2>&1; then
  fail "È richiesto Docker Compose V2 (il plugin 'docker compose'). Il vecchio binario standalone 'docker-compose' v1 non è supportato (questo progetto usa le condizioni di salute 'depends_on')."
fi

command -v openssl >/dev/null 2>&1 || fail "openssl è richiesto per generare segreti sicuri."
command -v curl >/dev/null 2>&1 || fail "curl è richiesto per attendere l'health check del Gateway."

info "Prerequisiti OK."

#2. Generazione / caricamento dei segreti 
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT_DIR"

if [[ "$RESET" == true ]]; then
  warn "Modalità reset: rimozione dei file .env e dei segreti esistenti."
  rm -f .env .env.docker ui/.env.local secrets/metrics_token.txt
fi

mkdir -p worker-tmp secrets

write_env_files_needed=false
for f in .env .env.docker ui/.env.local secrets/metrics_token.txt; do
  [[ -f "$f" ]] || write_env_files_needed=true
done

if [[ "$write_env_files_needed" == true ]]; then
  info "Generazione dei segreti sicuri in corso..."

  API_KEY=$(openssl rand -hex 32)
  METRICS_TOKEN=$(openssl rand -hex 32)
  HOST_TMP_DIR="$ROOT_DIR/worker-tmp"

  #.env root — usato da Docker Compose per l'interpolazione delle variabili
  #(sia HOST_TMP_DIR per il bind mount del Worker, sia API_KEY, che
  #Compose deve passare come argomento di build all'immagine della UI).
  if [[ ! -f .env ]]; then
    cat > .env <<EOF
HOST_TMP_DIR=$HOST_TMP_DIR
API_KEY=$API_KEY
EOF
    info "Creato .env"
  fi

  #Segreti dei container gateway/worker
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
    info "Creato .env.docker"
  fi

  #Configurazione UI per la dev-mode (usata solo se si avvia 'npm run dev' manualmente
  #per lo sviluppo con hot-reload — la UI containerizzata riceve la sua chiave
  #integrata al momento della build, tramite l'argomento di build di Compose).
  if [[ ! -f ui/.env.local ]]; then
    EXISTING_KEY=$(grep -E '^API_KEY=' .env.docker | cut -d= -f2)
    cat > ui/.env.local <<EOF
VITE_API_BASE_URL=http://localhost:8080
VITE_API_KEY=$EXISTING_KEY
EOF
    info "Creato ui/.env.local"
  fi

  #Token per lo scraping di Prometheus — deve corrispondere a METRICS_TOKEN in .env.docker
  if [[ ! -f secrets/metrics_token.txt ]]; then
    EXISTING_TOKEN=$(grep -E '^METRICS_TOKEN=' .env.docker | cut -d= -f2)
    echo -n "$EXISTING_TOKEN" > secrets/metrics_token.txt
    info "Creato secrets/metrics_token.txt"
  fi

  warn "I segreti sono stati generati automaticamente. Grafana usa ancora i valori di default di docker-compose (admin/admin) — cambiali al primo accesso se prevedi di mantenere questo ambiente."
else
  info "Tutti i file .env esistono già, salto la generazione dei segreti (usa --reset per rigenerarli)."
fi

#3. Build dell'immagine del compilatore sandboxed 
info "Build di compiler-image (viene eseguito una volta sola, poi usa la cache)..."
docker build -t compiler-image ./compiler-image

#4. Avvio dell'infrastruttura (inclusa la UI containerizzata) 
info "Avvio dell'intero stack — Postgres, Redis, RabbitMQ, Gateway, Worker, Prometheus, Grafana e la UI..."
docker compose up -d --build

#5. Attesa che il Gateway diventi "healthy" 
info "In attesa che il Gateway diventi operativo (healthy)..."
ATTEMPTS=0
MAX_ATTEMPTS=60
until curl --silent --fail http://127.0.0.1:8080/health > /dev/null 2>&1; do
  ATTEMPTS=$((ATTEMPTS + 1))
  if [[ $ATTEMPTS -ge $MAX_ATTEMPTS ]]; then
    fail "Il Gateway non è diventato operativo in tempo. Esegui 'docker compose logs gateway' per indagare."
  fi
  sleep 2
done
info "Il Gateway è operativo."

#6. Sincronizzazione dello schema del database
info "Sincronizzazione dello schema del database con Drizzle in corso..."
docker exec gateway npx drizzle-kit push --force

echo
echo -e "${GREEN}CompileForge è in esecuzione — non servono altri passaggi.${NC}"
echo
echo "  App (UI):            http://127.0.0.1:5173"
echo "  API Gateway:         http://127.0.0.1:8080"
echo "  Dashboard Grafana:   http://127.0.0.1:3000  (login: admin / admin)"
echo "  UI admin RabbitMQ:   http://127.0.0.1:15672 (login: admin / pass)"
echo
echo "Per fermare tutto:"
echo "  docker compose down"
echo
echo "Per lo sviluppo della UI con hot-reload al posto del container pre-compilato:"
echo "  cd ui && npm install && npm run dev   (poi apri http://localhost:5173)"
echo