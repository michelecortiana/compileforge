FROM node:20-alpine

# Installa il client Docker per permettere al Worker di lanciare i container di compilazione
RUN apk add --no-cache docker-cli

WORKDIR /app

COPY package*.json ./
RUN npm install

COPY . .

RUN npm run build

# Esponi la porta usata dal Worker per le metriche Prometheus
EXPOSE 9090

CMD ["npm", "start"]