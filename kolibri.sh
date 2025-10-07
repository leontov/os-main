#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<USAGE
Usage: $0 <command>

Commands:
  up      Build and launch the Kolibri node executable.
  build   Configure and build the Kolibri binaries.
USAGE
}

root_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
build_dir="$root_dir/build"
hmac_key_path="$root_dir/root.key"
frontend_dir="$root_dir/frontend"

generate_hmac_key() {
    if command -v openssl >/dev/null 2>&1; then
        openssl rand -hex 32
        return
    fi
    if command -v hexdump >/dev/null 2>&1; then
        hexdump -ve '1/1 "%02x"' -n 32 /dev/urandom
        return
    fi
    python3 - <<'PY'
import secrets
print(secrets.token_hex(32))
PY
}

ensure_hmac_key() {
    if [ -f "$hmac_key_path" ]; then
        return
    fi
    echo "[Kolibri] создаю новый HMAC-ключ в $hmac_key_path"
    generate_hmac_key >"$hmac_key_path"
}

ensure_frontend_prerequisites() {
    if ! command -v npm >/dev/null 2>&1; then
        echo "[Kolibri] npm не найден. Установите Node.js и npm для сборки фронтенда." >&2
        exit 1
    fi
}

install_frontend_dependencies() {
    local lockfile="$frontend_dir/package-lock.json"
    local stamp="$frontend_dir/node_modules/.kolibri-ci-stamp"

    if [ -f "$stamp" ] && [ "$lockfile" -ot "$stamp" ]; then
        return
    fi

    echo "[Kolibri] устанавливаю зависимости фронтенда"
    npm --prefix "$frontend_dir" ci
    touch "$stamp"
}

ensure_frontend_wasm() {
    local wasm_path="$build_dir/wasm/kolibri.wasm"
    if [ -f "$wasm_path" ]; then
        return
    fi
    echo "[Kolibri] kolibri.wasm не найден, запускаю scripts/build_wasm.sh"
    "$root_dir/scripts/build_wasm.sh"
}

build_frontend() {
    ensure_frontend_prerequisites
    install_frontend_dependencies
    ensure_frontend_wasm
    echo "[Kolibri] собираю фронтенд"
    local wasm_info="$build_dir/wasm/kolibri.wasm.txt"
    if [ -f "$wasm_info" ] && grep -qi 'kolibri\.wasm: заглушка' "$wasm_info"; then
        echo "[Kolibri] kolibri.wasm собран как заглушка — включаю деградированный режим фронтенда."
        echo "[Kolibri] Установите Emscripten или Docker и пересоберите scripts/build_wasm.sh для полноценного режима."
        KOLIBRI_ALLOW_WASM_STUB=1 npm --prefix "$frontend_dir" run build
        return
    fi
    npm --prefix "$frontend_dir" run build
}

case "${1:-}" in
    up)
        cmake -S "$root_dir" -B "$build_dir" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
        cmake --build "$build_dir"
        build_frontend
        ensure_hmac_key
        if [ -z "${KNP_THETA_FILE:-}" ]; then
            export KNP_THETA_FILE="$root_dir/data/knp_theta.csv"
        fi
        "$build_dir/kolibri_node" --hmac-key "$hmac_key_path"
        ;;
    build)
        cmake -S "$root_dir" -B "$build_dir" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
        cmake --build "$build_dir"
        ;;
    ""|-h|--help)
        usage
        ;;
    *)
        echo "Unknown command: $1" >&2
        usage
        exit 1
        ;;
esac
