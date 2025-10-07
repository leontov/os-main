#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<'USAGE'
Использование: export_soak_results.sh [-s каталог] [-l метка]

Опции:
  -s каталог   Каталог с результатами soak-прогона (по умолчанию build/soak)
  -l метка     Дополнительная метка для директории назначения
  -h           Показать эту справку

Скрипт копирует указанный каталог в logs/soak/raw/<timestamp>[_<метка>] с таймстампом в UTC.
USAGE
}

sanitize_tag() {
    printf '%s' "$1" | tr ' ' '_' | tr -cd '[:alnum:]_-' | sed 's/_\{2,\}/_/g'
}

root_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
istochnik="$root_dir/build/soak"
metka=""

while getopts "s:l:h" flag; do
    case "$flag" in
        s)
            istochnik="$OPTARG"
            ;;
        l)
            metka=$(sanitize_tag "$OPTARG")
            ;;
        h)
            usage
            exit 0
            ;;
        *)
            usage
            exit 1
            ;;
    esac
done

if [ ! -d "$istochnik" ]; then
    echo "[Ошибка] Источник '$istochnik' не найден" >&2
    exit 1
fi

logs_dir="$root_dir/logs/soak/raw"
mkdir -p "$logs_dir"

metka_sufiks=""
if [ -n "$metka" ]; then
    metka_sufiks="_${metka}"
fi

stamp=$(date -u +"%Y%m%dT%H%M%SZ")
priyomnik="$logs_dir/${stamp}${metka_sufiks}"
mkdir -p "$priyomnik"

cp -a "$istochnik"/. "$priyomnik"/

cat <<EOF2
[Готово] Результаты soak-прогона скопированы в: $priyomnik
EOF2
