FROM node:20-alpine

# 👇 AGGIUNGI QUESTA RIGA: Installa curl per l'healthcheck
RUN apk add --no-cache curl

# Imposta la directory di lavoro
WORKDIR /app

# Copia e installa le dipendenze
COPY package*.json ./
RUN npm install

# Copia il resto del codice
COPY . .

# Compila il codice TypeScript in JavaScript
RUN npm run build

# 👇 NUOVE RIGHE: Assegna i permessi e abbassa i privilegi
RUN chown -R node:node /app
USER node

# Esponi la porta usata dal Gateway
EXPOSE 8080

# Avvia usando Node puro sul file compilato
CMD ["npm", "start"]