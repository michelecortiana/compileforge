FROM node:20-alpine

# 👇 MODIFICA QUESTA RIGA: Aggiungi curl dopo docker-cli
RUN apk add --no-cache docker-cli curl

WORKDIR /app

COPY package*.json ./
RUN npm install

COPY . .

# 👇 Compila il codice TypeScript in JavaScript nella cartella /dist
RUN npm run build

# Esponi la porta usata dal Worker per le metriche Prometheus
EXPOSE 9090

# 👇 Avvia usando Node puro sul file compilato
CMD ["npm", "start"]