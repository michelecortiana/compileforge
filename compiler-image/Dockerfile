FROM debian:bookworm-slim AS builder

RUN apt-get update && apt-get install -y gcc make libc6-dev

WORKDIR /build

COPY Makefile ./
COPY src/ ./src/
COPY include/ ./include/

RUN make

FROM debian:bookworm-slim

RUN apt-get update && apt-get install -y gcc libc6-dev && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY --from=builder /build/compilatore /usr/local/bin/compilatore

RUN chmod +x /usr/local/bin/compilatore

ENTRYPOINT ["compilatore"]