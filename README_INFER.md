
# Kolibri Nano — усиленное ядро (χ/Φ/S) и инференс без хранилищ

## Сборка
```bash
make
```

## CLI-инференс
```bash
./apps/kolibri_infer --q 123456 --beam 8 --depth 6
KNP_THETA="1.0,0.3,-0.2,0.12" ./apps/kolibri_infer --q 42
# stdout: "<best_id> <value> <score>"
```

## HTTP API
```bash
export KNP_INFER_BIN="$(pwd)/apps/kolibri_infer"
uvicorn backend.infer_api.main:app --host 0.0.0.0 --port 8010
curl -s http://127.0.0.1:8010/api/infer -H 'content-type: application/json' \
  -d '{"q":123456,"beam":8,"depth":6,"theta":[1.0,0.3,-0.2,0.12]}'
```

Инварианты:
- Конвейер: ID → χ → Φ → S → EMIT.
- Никаких БД/словари/кэш — вычисления «на лету».
- θ — фиксированный малый вектор (до 32 чисел).
