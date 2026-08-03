FROM node:20-alpine

# Imposta la directory di lavoro
WORKDIR /app

# Copia e installa le dipendenze (incluse quelle di dev per avere tsx)
COPY package*.json ./
RUN npm install

# Copia il resto del codice
COPY . .

# Esponi la porta usata dal Gateway
EXPOSE 8080

# Avvia usando tsx direttamente sul file TypeScript
CMD ["npx", "tsx", "src/server.ts"]