# CompileForge
Cloud-based microservices architecture (API Gateway, RabbitMQ, Worker) exposing a custom C compiler.

## 🔒 Configurazione Sicurezza e Avvio Rapido

Prima di avviare l'infrastruttura, devi generare i tuoi segreti locali. I file contenenti i segreti reali sono volutamente ignorati da Git per motivi di sicurezza.

**1. Configura le variabili d'ambiente:**
```bash
cp .env.example .env.docker
cp .env.example .env
(Apri i file appena copiati e inserisci le tue chiavi reali per API_KEY e METRICS_TOKEN)

2. Configura il token di Prometheus:

Bash
cp secrets/metrics_token.txt.example secrets/metrics_token.txt
(⚠️ IMPORTANTE: Inserisci dentro secrets/metrics_token.txt lo stesso identico valore che hai assegnato a METRICS_TOKEN nei file .env. Se i valori non combaciano, Prometheus non riuscirà a leggere le metriche e le dashboard di Grafana resteranno vuote).


Piazzalo nel `README.md` e hai un blocco note formattato da dio a prova di amnesia!