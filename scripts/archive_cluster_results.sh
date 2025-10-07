#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<'USAGE'
Использование: archive_cluster_results.sh [-s каталог] [-o каталог] [-l метка]

Опции:
  -s каталог   Каталог с сырыми логами кластерных прогонов (по умолчанию logs/cluster/raw)
  -o каталог   Каталог для сохранения архива (по умолчанию logs/cluster/archives)
  -l метка     Дополнительная метка для имени архива
  -h           Показать эту справку

Скрипт создаёт архив tar.gz с именем cluster_<timestamp>[_<метка>].tar.gz в UTC.
USAGE
}

sanitize_tag() {
    printf '%s' "$1" | tr ' ' '_' | tr -cd '[:alnum:]_-' | sed 's/_\{2,\}/_/g'
}

root_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
istochnik="$root_dir/logs/cluster/raw"
priyomnik="$root_dir/logs/cluster/archives"
metka=""

while getopts "s:o:l:h" flag; do
    case "$flag" in
        s)
            istochnik="$OPTARG"
            ;;
        o)
            priyomnik="$OPTARG"
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

mkdir -p "$priyomnik"

stamp=$(date -u +"%Y%m%dT%H%M%SZ")
metka_sufiks=""
if [ -n "$metka" ]; then
    metka_sufiks="_${metka}"
fi

arkhiv_path="$priyomnik/cluster_${stamp}${metka_sufiks}.tar.gz"

tar -czf "$arkhiv_path" -C "$istochnik" .

if command -v sha256sum >/dev/null 2>&1; then
    sha256sum "$arkhiv_path" > "${arkhiv_path}.sha256"
fi

cat <<EOF2
[Готово] Архив создан: $arkhiv_path
EOF2
