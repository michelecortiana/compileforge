#!/bin/bash

echo "🚀 Lancio 50 job di compilazione in parallelo..."

# Avvia 50 richieste in background usando &
for i in {1..50}; do
  curl -s -X POST http://localhost:8080/compile \
    -H "Content-Type: application/json" \
    -H "X-API-Key: super-secret-key-123" \
    -d @test_payload_small.json > /dev/null &
done

# Aspetta che tutti i processi in background finiscano
wait

echo "✅ Tutte le 50 richieste sono state inviate al Gateway!"