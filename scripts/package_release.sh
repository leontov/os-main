#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<USAGE
Использование: $0 [опции]

Опции:
  --skip-iso       Не собирать и не включать kolibri.iso / kolibri.img
  --skip-wasm      Не собирать и не включать kolibri.wasm
  --skip-cluster   Не запускать оркестровочный кластер при подготовке
  --skip-docker    Не собирать и не публиковать Docker-образы
  --registry R     Контейнерный реестр (по умолчанию $KOLIBRI_DOCKER_REGISTRY или kolibri)
  --tag T          Тег Docker-образов (по умолчанию $KOLIBRI_DOCKER_TAG или текущий коммит)
  -h, --help       Показать эту справку
USAGE
}

propustit_iso=0
propustit_wasm=0
propustit_klaster=0
propustit_docker=0
docker_registry="${KOLIBRI_DOCKER_REGISTRY:-}"
docker_tag="${KOLIBRI_DOCKER_TAG:-}"

while (("$#" > 0)); do
    case "$1" in
        --skip-iso)
            propustit_iso=1
            shift
            ;;
        --skip-wasm)
            propustit_wasm=1
            shift
            ;;
        --skip-cluster)
            propustit_klaster=1
            shift
            ;;
        --skip-docker)
            propustit_docker=1
            shift
            ;;
        --registry)
            docker_registry="$2"
            shift 2
            ;;
        --tag)
            docker_tag="$2"
            shift 2
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

kornevaya=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
postroika="$kornevaya/build"
reliz_dir="$postroika/release"
mkdir -p "$reliz_dir"

ensure() {
    local path="$1"
    local message="$2"
    if [ ! -f "$path" ]; then
        echo "[Ошибка] $message ($path)" >&2
        exit 1
    fi
}

docker_registry="${docker_registry:-kolibri}"
if [ -z "$docker_tag" ]; then
    docker_tag=$(git -C "$kornevaya" rev-parse --short HEAD)
fi

# 1. Полный цикл сборки / тестов / артефактов.
run_all_opts=()
if [ "$propustit_iso" -eq 1 ]; then run_all_opts+=("--skip-iso"); fi
if [ "$propustit_wasm" -eq 1 ]; then run_all_opts+=("--skip-wasm"); fi
if [ "$propustit_klaster" -eq 1 ]; then
    run_all_opts+=("--skip-cluster")
else
    run_all_opts+=("-n" "3" "-d" "10")
fi

"$kornevaya/scripts/run_all.sh" "${run_all_opts[@]}"

# 2. Сборка SDK (python + js)
"$kornevaya/scripts/build_sdk.sh" all

if [ "$propustit_iso" -eq 0 ]; then
    ensure "$postroika/kolibri.iso" "Не найден kolibri.iso"
    ensure "$postroika/kolibri.bin" "Не найден kolibri.bin"
fi
if [ "$propustit_wasm" -eq 0 ]; then
    ensure "$postroika/wasm/kolibri.wasm" "Не найден kolibri.wasm"
fi

create_disk_image() {
    local kernel_bin="$1"
    local boot_src="$2"
    local out_img="$3"
    local tmp_boot="$postроika/kolibri_boot.bin"

    nasm -f bin "$boot_src" -o "$tmp_boot"
    local kernel_size kernel_sectors
    kernel_size=$(stat -c '%s' "$kernel_bin")
    kernel_sectors=$(( (kernel_size + 511) / 512 ))

    python3 - "$tmp_boot" "$kernel_size" "$kernel_sectors" <<'PY'
import struct
import sys
boot_path, kernel_size, sectors = sys.argv[1:4]
kernel_size = int(kernel_size)
sectors = int(sectors)
with open(boot_path, 'r+b') as boot:
    data = boot.read()
    marker = data.find(b'KSEC')
    if marker == -1:
        raise SystemExit('маркер KSEC не найден')
    boot.seek(marker + 4)
    boot.write(struct.pack('<H', sectors))
    marker = data.find(b'KBYT')
    if marker == -1:
        raise SystemExit('маркер KBYT не найден')
    boot.seek(marker + 4)
    boot.write(struct.pack('<I', kernel_size))
PY

    dd if=/dev/zero of="$out_img" bs=512 count=$((kernel_sectors + 1)) status=none
    dd if="$tmp_boot" of="$out_img" bs=512 count=1 conv=notrunc status=none
    dd if="$kernel_bin" of="$out_img" bs=512 seek=1 conv=notrunc status=none
}

if [ "$propustit_iso" -eq 0 ]; then
    create_disk_image "$postroika/kolibri.bin" "$kornevaya/boot/kolibri.asm" "$postroika/kolibri.img"
fi

# 3. Подготовка релизного payload
payload="$reliz_dir/payload"
rm -rf "$payload"
mkdir -p "$payload"/{os,wasm,cli,sdk/python,sdk/js,docs,metadata}

if [ "$propustit_iso" -eq 0 ]; then
    cp "$postroika/kolibri.iso" "$payload/os/"
    cp "$postroika/kolibri.img" "$payload/os/"
fi
if [ "$propustit_wasm" -eq 0 ]; then
    cp "$postroika/wasm/kolibri.wasm" "$payload/wasm/"
fi

# CLI бинарник (kolibri_node)
shopt -s nullglob
cli_candidates=(
    "$postroika/kolibri_node"
    "$postroika/apps/kolibri_node"
    "$postroika/apps/kolibri_node/kolibri_node"
    "$postroika/apps/Debug/kolibri_node"
    "$postroika/apps/Release/kolibri_node"
)
cli_found=0
for candidate in "${cli_candidates[@]}"; do
    if [ -f "$candidate" ]; then
        cp "$candidate" "$payload/cli/"
        chmod +x "$payload/cli/$(basename "$candidate")"
        cli_found=1
        break
    fi
done
shopt -u nullglob
if [ $cli_found -eq 0 ]; then
    echo "[Предупреждение] CLI (kolibri_node) не найден, он не будет включён в релиз" >&2
fi

# Python SDK (wheel)
python_dist="$kornevaya/sdk/python/dist"
if [ -d "$python_dist" ]; then
    shopt -s nullglob
    py_files=("$python_dist"/*.whl)
    shopt -u nullglob
    if [ ${#py_files[@]} -eq 0 ]; then
        echo "[Ошибка] Сборка Python SDK не создала wheel" >&2
        exit 1
    fi
    cp "${py_files[@]}" "$payload/sdk/python/"
else
    echo "[Ошибка] Каталог $python_dist не найден" >&2
    exit 1
fi

# JS SDK (tar.gz из dist)
js_dist="$kornevaya/sdk/js/dist"
if [ -d "$js_dist" ]; then
    tar -czf "$payload/sdk/js/kolibri-sdk-js.tar.gz" -C "$js_dist" .
else
    echo "[Ошибка] Каталог $js_dist не найден (npm run build)" >&2
    exit 1
fi

# Документация и метаданные
cp "$kornevaya/README.md" "$payload/docs/"
cp "$kornevaya/LICENSE" "$payload/docs/"
cp "$kornevaya/docs/release_notes.md" "$payload/docs/"
cp "$kornevaya/docs/kolibri_integrated_prototype.md" "$payload/docs/"

metadata_file="$payload/metadata/METADATA.txt"
commit=$(git -C "$kornevaya" rev-parse --verify HEAD)
{
    echo "Kolibri Platform release"
    echo "Commit: $commit"
    echo "Timestamp: $(date -u +"%Y-%m-%dT%H:%M:%SZ")"
    echo "Skip ISO: $propustit_iso"
    echo "Skip WASM: $propustit_wasm"
} >"$metadata_file"

if command -v sha256sum >/dev/null 2>&1; then
    checksum_file="$payload/metadata/checksums.txt"
    : >"$checksum_file"
    (cd "$payload" && find . -type f ! -name 'checksums.txt' -print0 | sort -z | while IFS= read -r -d '' file; do
        sha256sum "$file" >> "$checksum_file"
    done)
fi

# 4. Архив
timestamp=$(date -u +"%Y%m%dT%H%M%SZ")
arhiv="$reliz_dir/kolibri-platform-${timestamp}.tar.gz"
tar -czf "$arhiv" -C "$payload" .

# 5. Docker (опционально)
if [ "$propustit_docker" -eq 0 ]; then
    if ! command -v docker >/dev/null 2>&1; then
        echo "[Ошибка] Docker не найден в PATH, а публикация образов не отключена." >&2
        exit 1
    fi

    registry_trim="${docker_registry%/}"
    docker_components=(backend frontend training)
    for component in "${docker_components[@]}"; do
        dockerfile="$kornevaya/$component/Dockerfile"
        if [ ! -f "$dockerfile" ]; then
            echo "[Ошибка] Не найден Dockerfile: $dockerfile" >&2
            exit 1
        fi
        image="${registry_trim}/kolibri-${component}:${docker_tag}"
        docker build -f "$dockerfile" -t "$image" "$kornevaya"
        docker push "$image"
    done
else
    echo "[Docker] Сборка Docker-образов пропущена по флагу --skip-docker"
fi

# 6. Финальное сообщение
cat <<EOF
[Готово] Релизный архив создан: $arhiv
Структура:
  os/        — kolibri.iso, kolibri.img
  wasm/      — kolibri.wasm
  cli/       — kolibri_node (если собран)
  sdk/python — Python wheel
  sdk/js     — kolibri-sdk-js.tar.gz
  docs/      — ключевые документы
  metadata/  — METADATA.txt, checksums.txt
EOF
