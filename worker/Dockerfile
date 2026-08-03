FROM node:20-alpine

# Installa il client Docker per permettere al Worker di lanciare i container
RUN apk add --no-cache docker-cli

WORKDIR /app

COPY package*.json ./
RUN npm install

COPY . .

# Esponi la porta usata dal Worker per le metriche Prometheus
EXPOSE 9090

# Avvia usando tsx direttamente sul file TypeScript del worker
CMD ["npx", "tsx", "src/worker.ts"]