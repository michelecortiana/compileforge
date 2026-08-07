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

# 👇 CORRETTO: Installiamo gcc e libc6-dev per permettere il linking a runtime
RUN apt-get update && apt-get install -y gcc libc6-dev && rm -rf /var/lib/apt/lists/*

# NUOVO: Creiamo il gruppo e l'utente di sistema (non privilegiato)
RUN groupadd -r compiler_group && useradd -r -g compiler_group compiler_user

WORKDIR /app

COPY --from=builder /build/compilatore /usr/local/bin/compilatore

RUN chmod +x /usr/local/bin/compilatore

# NUOVO: Diciamo a Docker di eseguire da qui in poi come 'compiler_user'
USER compiler_user

ENTRYPOINT ["compilatore"]