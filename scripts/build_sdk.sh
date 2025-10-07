#!/usr/bin/env bash
set -euo pipefail

# Aggregated build helper for Kolibri SDK artifacts.
ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)

usage() {
    cat <<USAGE
Usage: $0 <component>

Components:
  python   Build Python wheel (sdk/python)
  js       Compile TypeScript bundle (sdk/js)
  all      Build all SDK artefacts
USAGE
}

run_python() {
    local sdk_dir="$ROOT_DIR/sdk/python"
    if ! command -v python3 >/dev/null 2>&1; then
        echo "python3 not found" >&2
        exit 1
    fi
    python3 -m pip install --upgrade build >/dev/null 2>&1 || true
    python3 -m build "$sdk_dir"
}

run_js() {
    local sdk_dir="$ROOT_DIR/sdk/js"
    if ! command -v npm >/dev/null 2>&1; then
        echo "npm not found" >&2
        exit 1
    fi
    (cd "$sdk_dir" && npm install && npm run build)
}

case "${1:-}" in
    python)
        run_python
        ;;
    js)
        run_js
        ;;
    all)
        run_python
        run_js
        ;;
    *)
        usage
        exit 1
        ;;
esac
