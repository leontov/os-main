#!/usr/bin/env bash
set -euo pipefail

# Простая обёртка для запуска самодостаточного колибри-пакета из dist/kolibri_selfcontained.tar.gz
# Распаковывает архив в временную директорию и запускает нативный узел + статический http для фронтенда.

here="$(cd "$(dirname "$0")" && pwd)"
root="${here}/.."
dist="${root}/dist/kolibri_selfcontained.tar.gz"

if [[ ! -f "$dist" ]]; then
  echo "Не найден пакет: $dist" >&2
  echo "Сначала соберите пакет: see scripts/build_wasm.sh and frontend build" >&2
  exit 2
fi

tmpdir="$(mktemp -d /tmp/kolibri-local-XXXX)"
echo "Распаковка в: $tmpdir"
tar -xzf "$dist" -C "$tmpdir"

cp -R "$tmpdir/kolibri_package/frontend" "$tmpdir/kolibri_frontend"
cp -R "$tmpdir/kolibri_package/wasm" "$tmpdir/kolibri_frontend/public/" 2>/dev/null || true

if [[ -x "$root/build/kolibri_node" ]]; then
  echo "Запускаю нативный узел (в фоне)..."
  (cd "$root/build" && ./kolibri_node) &
else
  echo "Нативный бинарник build/kolibri_node не найден или не исполняемый. Продолжу только с фронтендом." >&2
fi

echo "Запускаю статический сервер для фронтенда на http://localhost:8000"
cd "$tmpdir/kolibri_frontend"
python3 -m http.server 8000
