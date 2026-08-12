FROM node:20-alpine

# Installa il client Docker per permettere al Worker di lanciare i container
RUN apk add --no-cache docker-cli

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