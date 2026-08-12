# Fase 1: Costruzione (Builder)
FROM debian:bookworm-slim AS builder

RUN apt-get update && apt-get install -y gcc make libc6-dev

WORKDIR /build

COPY Makefile ./
COPY src/ ./src/
COPY include/ ./include/

RUN make

# Fase 2: Esecuzione
FROM debian:bookworm-slim

# Installiamo gcc e libc6-dev per permettere il linking a runtime
RUN apt-get update && apt-get install -y gcc libc6-dev && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY --from=builder /build/compilatore /usr/local/bin/compilatore
RUN chmod +x /usr/local/bin/compilatore

# ❌ RIMOSSE le righe groupadd, useradd e USER compiler_user
# La sicurezza e i permessi sono ora gestiti dinamicamente dal comando run del Worker

ENTRYPOINT ["compilatore"]