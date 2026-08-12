FROM node:20-alpine

# Imposta la directory di lavoro
WORKDIR /app

# Copia e installa le dipendenze
COPY package*.json ./
RUN npm install

# Copia il resto del codice
COPY . .

# Compila il codice TypeScript in JavaScript
RUN npm run build

# Esponi la porta usata dal Gateway
EXPOSE 8080

# Avvia usando Node puro sul file compilato
CMD ["npm", "start"]