#!/bin/bash

echo "🚀 Lancio 50 job di compilazione in parallelo..."

for i in {1..50}; do
  STATUS=$(curl -s -o /dev/null -w "%{http_code}" -X POST http://localhost:8080/compile \
    -H "Content-Type: application/json" \
    -H "X-API-Key: 6a00637b22602b2512343b00d95a8327ab2f0bd0d57987e392503e85e0219cb7" \
    -d @test_payload_small.json)
  echo "Richiesta $i -> HTTP $STATUS"
done | tee stress_results.log

wait
FAILED=$(grep -cv "202" stress_results.log)
echo "✅ Fatto. Richieste non accettate (diverse da 202): $FAILED / 50"