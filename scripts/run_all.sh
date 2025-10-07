#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<USAGE
Использование: $0 [опции]

Опции:
  -n число        Сколько узлов запускать в кластере (по умолчанию 3)
  -d секунды      Сколько секунд держать кластер активным (по умолчанию 30)
  --skip-cluster  Пропустить запуск кластера
  --skip-iso      Пропустить сборку ISO-образа
  --skip-wasm     Пропустить сборку kolibri.wasm
  -h              Показать эту справку
USAGE
}

uzly=3
prodolzhitelnost=30
propustit_klaster=0
propustit_iso=0
propustit_wasm=0

while (("$#" > 0)); do
    case "$1" in
        -n)
            uzly="$2"
            shift 2
            ;;
        -d)
            prodolzhitelnost="$2"
            shift 2
            ;;
        --skip-cluster)
            propustit_klaster=1
            shift
            ;;
        --skip-iso)
            propustit_iso=1
            shift
            ;;
        --skip-wasm)
            propustit_wasm=1
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "[Ошибка] Неизвестная опция: $1" >&2
            usage
            exit 1
            ;;
    esac
done

if ! [[ "$uzly" =~ ^[0-9]+$ ]] || [ "$uzly" -lt 1 ]; then
    echo "[Ошибка] Количество узлов должно быть натуральным" >&2
    exit 1
fi

if ! [[ "$prodolzhitelnost" =~ ^[0-9]+$ ]] || [ "$prodolzhitelnost" -lt 0 ]; then
    echo "[Ошибка] Продолжительность должна быть неотрицательной" >&2
    exit 1
fi

kornevaya=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
postroika="$kornevaya/build"

zapisat_shag() {
    echo "[Колибри] $1"
}

zapisat_shag "Подготовка каталога build: $postroika"
cmake -S "$kornevaya" -B "$postroika" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

zapisat_shag "Сборка ядра"
cmake --build "$postroika"

zapisat_shag "Запуск модульных тестов"
ctest --test-dir "$postroika" --output-on-failure

if [ "$propustit_wasm" -eq 0 ]; then
    zapisat_shag "Сборка kolibri.wasm"
    "$kornevaya/scripts/build_wasm.sh"
else
    zapisat_shag "Пропускаем сборку kolibri.wasm по требованию"
fi

if [ "$propustit_iso" -eq 0 ]; then
    zapisat_shag "Сборка загрузочного образа kolibri.iso"
    "$kornevaya/scripts/build_iso.sh"
else
    zapisat_shag "Пропускаем сборку kolibri.iso по требованию"
fi

if [ "$propustit_klaster" -eq 0 ]; then
    zapisat_shag "Запуск локального роя на $uzly узлах"
    "$kornevaya/scripts/run_cluster.sh" -n "$uzly" -d "$prodolzhitelnost"
else
    zapisat_shag "Пропускаем запуск кластера по требованию"
fi

zapisat_shag "Полный цикл выполнен"
