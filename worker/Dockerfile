FROM node:20-alpine
RUN apk add --no-cache docker-cli curl
WORKDIR /app
COPY package*.json ./
RUN npm install
COPY . .
RUN npm run build
EXPOSE 9090
CMD ["npm", "start"]