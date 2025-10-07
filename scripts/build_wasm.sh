#!/usr/bin/env bash
set -euo pipefail

# Скрипт компиляции ядра Kolibri в WebAssembly.
# По умолчанию собирает вычислительное ядро (десятичный слой,
# эволюцию формул и генератор случайных чисел) и проверяет,
# что итоговый модуль укладывается в бюджет < 1 МБ.

proekt_koren="$(cd "$(dirname "$0")/.." && pwd)"
vyhod_dir="$proekt_koren/build/wasm"
mkdir -p "$vyhod_dir"

vyhod_wasm="$vyhod_dir/kolibri.wasm"
vremennaja_map="$vyhod_dir/kolibri.map"
vremennaja_js="$vyhod_dir/kolibri.js"

EMCC="${EMCC:-emcc}"
sozdat_zaglushku=0

vychislit_sha256_stroku() {
    local file="$1"

    if command -v sha256sum >/dev/null 2>&1; then
        sha256sum "$file"
        return 0
    fi

    if command -v shasum >/dev/null 2>&1; then
        shasum -a 256 "$file"
        return 0
    fi

    if command -v python3 >/dev/null 2>&1; then
        python3 - "$file" <<'PY'
import hashlib
import os
import sys

path = sys.argv[1]
with open(path, "rb") as handle:
    digest = hashlib.sha256(handle.read()).hexdigest()

print(f"{digest}  {os.path.basename(path)}")
PY
        return 0
    fi

    return 1
}

zapisat_sha256() {
    local file="$1"
    local target="$2"

    if vychislit_sha256_stroku "$file" >"$target.tmp"; then
        mv "$target.tmp" "$target"
        return 0
    fi

    rm -f "$target.tmp"
    cat >"$target" <<EOF
sha256 недоступна: отсутствуют утилиты sha256sum/shasum и python3
EOF
    echo "[ПРЕДУПРЕЖДЕНИЕ] Не удалось вычислить SHA256 для $file: отсутствуют необходимые утилиты." >&2
    return 1
}

sozdat_stub_wasm() {
    printf '\x00asm\x01\x00\x00\x00' >"$vyhod_wasm"
    cat >"$vyhod_dir/kolibri.wasm.txt" <<'EOF_INFO'
kolibri.wasm: заглушка (WebAssembly ядро недоступно)
В окружении не найден компилятор Emscripten (emcc) и отсутствует Docker для
автосборки. Фронтенд Kolibri будет работать в деградированном режиме без WASM.
Установите Emscripten или Docker и повторно запустите scripts/build_wasm.sh,
чтобы получить полноценный модуль kolibri.wasm.
EOF_INFO
    zapisat_sha256 "$vyhod_wasm" "$vyhod_dir/kolibri.wasm.sha256"
    rm -f "$vremennaja_js" "$vremennaja_map"
    echo "[ПРЕДУПРЕЖДЕНИЕ] kolibri.wasm заменён заглушкой. Установите Emscripten или Docker для полноценной сборки." >&2
}

opredelit_razmer() {
    local file="$1"

    if stat -c '%s' "$file" >/dev/null 2>&1; then
        stat -c '%s' "$file"
        return 0
    fi

    if stat -f '%z' "$file" >/dev/null 2>&1; then
        stat -f '%z' "$file"
        return 0
    fi

    if command -v python3 >/dev/null 2>&1; then
        python3 - "$file" <<'PY'
import os
import sys

print(os.path.getsize(sys.argv[1]))
PY
        return 0
    fi

    local size
    size=$(wc -c <"$file")
    size=${size//[[:space:]]/}
    printf '%s\n' "$size"
}

if ! command -v "$EMCC" >/dev/null 2>&1; then
    if [[ "${KOLIBRI_WASM_INVOKED_VIA_DOCKER:-0}" == "1" ]]; then
        echo "[ОШИБКА] Не найден emcc внутри Docker-окружения. Проверьте образ ${KOLIBRI_WASM_DOCKER_IMAGE:-emscripten/emsdk:3.1.61}." >&2
        exit 1
    fi

    if command -v docker >/dev/null 2>&1; then
        docker_image="${KOLIBRI_WASM_DOCKER_IMAGE:-emscripten/emsdk:3.1.61}"
        echo "[Kolibri] emcc не найден. Пытаюсь собрать kolibri.wasm через Docker (${docker_image})."
        docker run --rm \
            -v "$proekt_koren":/project \
            -w /project/scripts \
            -e KOLIBRI_WASM_INVOKED_VIA_DOCKER=1 \
            -e KOLIBRI_WASM_INCLUDE_GENOME \
            -e KOLIBRI_WASM_GENERATE_MAP \
            "$docker_image" \
            bash -lc "./build_wasm.sh"
        exit $?
    fi

    sozdat_zaglushku=1
fi

if (( sozdat_zaglushku )); then
    sozdat_stub_wasm
    exit 0
fi

istochniki=(
    "$proekt_koren/backend/src/decimal.c"
    "$proekt_koren/backend/src/digits.c"
    "$proekt_koren/backend/src/formula.c"
    "$proekt_koren/backend/src/random.c"
    "$proekt_koren/backend/src/script.c"
    "$proekt_koren/backend/src/wasm_bridge.c"
)

if [[ "${KOLIBRI_WASM_INCLUDE_GENOME:-0}" == "1" ]]; then
    istochniki+=("$proekt_koren/backend/src/genome.c")
else
    istochniki+=("$proekt_koren/backend/src/wasm_genome_stub.c")
fi

flags=(
    -Os
    -std=gnu99
    -s STANDALONE_WASM=1
    -s SIDE_MODULE=0
    -s ALLOW_MEMORY_GROWTH=0
    -s EXPORTED_RUNTIME_METHODS='[]'
    -s EXPORTED_FUNCTIONS='["_kolibri_bridge_init","_kolibri_bridge_reset","_kolibri_bridge_execute","_malloc","_free"]'
    -s DEFAULT_LIBRARY_FUNCS_TO_INCLUDE='[]'
    --no-entry
    -I"$proekt_koren/backend/include"
    -o "$vyhod_wasm"
)

if [[ "${KOLIBRI_WASM_GENERATE_MAP:-0}" == "1" ]]; then
    flags+=(--emit-symbol-map)
fi

"$EMCC" "${istochniki[@]}" "${flags[@]}"

razmer=$(opredelit_razmer "$vyhod_wasm")
if (( razmer > 1024 * 1024 )); then
    printf '[ОШИБКА] kolibri.wasm превышает бюджет: %.2f МБ\n' "$(awk -v b="$razmer" 'BEGIN {printf "%.2f", b/1048576}')" >&2
    exit 1
fi

ekport_info="$vyhod_dir/kolibri.wasm.txt"
cat >"$ekport_info" <<EOF_INFO
kolibri.wasm: $(awk -v b="$razmer" 'BEGIN {printf "%.2f МБ", b/1048576}')
Эта сборка включает вычислительное ядро (десятичные трансдукции,
эволюцию формул и генератор случайных чисел). Для включения цифрового
генома запустите скрипт с KOLIBRI_WASM_INCLUDE_GENOME=1 и добавьте
поддержку HMAC в окружении.
EOF_INFO

zapisat_sha256 "$vyhod_wasm" "$vyhod_dir/kolibri.wasm.sha256"

rm -f "$vremennaja_js" "$vremennaja_map"

echo "[ГОТОВО] kolibri.wasm собрано: $vyhod_wasm"
