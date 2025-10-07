#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<USAGE
Использование: $0 [-n количество] [-b порт] [-d секунды] [-s зерно]

Опции:
  -n количество    Сколько узлов запустить (по умолчанию 3)
  -b порт          Стартовый порт прослушивания (по умолчанию 4100)
  -d секунды       Продолжительность работы (0 — до Ctrl+C, по умолчанию 60)
  -s зерно         Базовое зерно детерминизма (по умолчанию 20250923)
USAGE
}

kolichestvo=3
bazovyj_port=4100
prodolzhitelnost=60
bazovoe_zerno=20250923

while getopts "n:b:d:s:h" flag; do
    case "$flag" in
        n)
            kolichestvo="$OPTARG"
            ;;
        b)
            bazovyj_port="$OPTARG"
            ;;
        d)
            prodolzhitelnost="$OPTARG"
            ;;
        s)
            bazovoe_zerno="$OPTARG"
            ;;
        h|*)
            usage
            exit 0
            ;;
    esac
done

if ! [[ "$kolichestvo" =~ ^[0-9]+$ ]] || [ "$kolichestvo" -lt 1 ]; then
    echo "[Ошибка] Количество узлов должно быть натуральным" >&2
    exit 1
fi

if ! [[ "$bazovyj_port" =~ ^[0-9]+$ ]] || [ "$bazovyj_port" -lt 1 ] || [ "$bazovyj_port" -gt 65500 ]; then
    echo "[Ошибка] Неверный стартовый порт" >&2
    exit 1
fi

if ! [[ "$prodolzhitelnost" =~ ^[0-9]+$ ]]; then
    echo "[Ошибка] Некорректная продолжительность" >&2
    exit 1
fi

root_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
build_dir="$root_dir/build"
cluster_dir="$build_dir/cluster"
mkdir -p "$cluster_dir"

key_path="$cluster_dir/swarm.key"

sozdat_klyuch() {
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

obespechit_klyuch() {
    if [ -f "$key_path" ]; then
        return
    fi
    echo "[Рой] генерируем новый ключ кластера: $key_path"
    sozdat_klyuch >"$key_path"
}

cmake -S "$root_dir" -B "$build_dir" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON >/dev/null
cmake --build "$build_dir" >/dev/null

obespechit_klyuch

declare -a pids=()

otchistka() {
    for pid in "${pids[@]}"; do
        if kill -0 "$pid" 2>/dev/null; then
            kill "$pid" 2>/dev/null || true
        fi
    done
    wait >/dev/null 2>&1 || true
}

trap 'otchistka' INT TERM

echo "[Рой] Запуск $kolichestvo узлов с базовым портом $bazovyj_port"

for ((indeks = 0; indeks < kolichestvo; ++indeks)); do
    nomer=$((indeks + 1))
    port=$((bazovyj_port + indeks))
    sledujushchij=$(((indeks + 1) % kolichestvo))
    port_soseda=$((bazovyj_port + sledujushchij))
    geneticheskij="$cluster_dir/genome_${nomer}.dat"
    zhurnal="$cluster_dir/node_${nomer}.log"
    seed=$((bazovoe_zerno + nomer))
    komanda=("$build_dir/kolibri_node" "--node-id" "$nomer" "--listen" "$port" "--genome" "$geneticheskij" "--seed" "$seed" "--verify-genome" "--hmac-key" "$key_path")
    if [ "$kolichestvo" -gt 1 ]; then
        komanda+=("--peer" "127.0.0.1:${port_soseda}")
    fi
    echo "[Рой] узел $nomer слушает $port, лог: $zhurnal"
    "${komanda[@]}" >"$zhurnal" 2>&1 &
    pids+=($!)
    sleep 0.2
done

echo "[Рой] Все узлы запущены. Для остановки нажмите Ctrl+C."
if [ "$prodolzhitelnost" -gt 0 ]; then
    sleep "$prodolzhitelnost"
    echo "[Рой] Время истекло, останавливаем кластер"
    otchistka
else
    wait
fi
