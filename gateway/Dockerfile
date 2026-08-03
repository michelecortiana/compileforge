FROM node:20-alpine

# Imposta la directory di lavoro dentro il container
WORKDIR /app

# Copia i file delle dipendenze e installale
COPY package*.json ./
RUN npm install

# Copia tutto il resto del codice
COPY . .

# Compila il codice TypeScript (assicurati di avere lo script "build" nel package.json)
RUN npm run build

# Esponi la porta usata dal Gateway
EXPOSE 8080

# Comando per avviare l'app
CMD ["npm", "start"]