#!/bin/bash
: "${API_KEY:?Devi esportare API_KEY prima di lanciare lo script, es: export API_KEY=xxxx}"

echo "Lancio 50 job di compilazione in parallelo..."
> stress_results.log

for i in {1..50}; do
  (
    STATUS=$(curl -s -o /dev/null -w "%{http_code}" -X POST http://localhost:8080/compile \
      -H "Content-Type: application/json" \
      -H "X-API-Key: $API_KEY" \
      -d @test_payload_small.json)
    echo "Richiesta $i -> HTTP $STATUS"
  ) >> stress_results.log &
done

wait
FAILED=$(grep -cv "202" stress_results.log)
echo "Fatto. Richieste non accettate (diverse da 202): $FAILED / 50"