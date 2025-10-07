#!/usr/bin/env bash
set -euo pipefail

# Скрипт готовит релизный пакет Kolibri: собирает артефакты ядра,
# проверяет контрольные суммы и формирует tar-архив в каталоге build/release.

usage() {
    cat <<USAGE
Использование: $0 [опции]

Опции:
  --skip-iso       Не собирать и не включать kolibri.iso
  --skip-wasm      Не собирать и не включать kolibri.wasm
  --skip-cluster   Не запускать оркестровочный кластер в рамках подготовки
  --skip-docker    Пропустить сборку и публикацию Docker-образов
  --registry R     Контейнерный реестр (по умолчанию $KOLIBRI_DOCKER_REGISTRY или kolibri)
  --tag T          Тег образов (по умолчанию $KOLIBRI_DOCKER_TAG или текущий commit)
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
            if [ -z "${2:-}" ]; then
                echo "[Ошибка] Опция --registry требует аргумент" >&2
                exit 1
            fi
            docker_registry="$2"
            shift 2
            ;;
        --tag)
            if [ -z "${2:-}" ]; then
                echo "[Ошибка] Опция --tag требует аргумент" >&2
                exit 1
            fi
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

if [ -z "$docker_registry" ]; then
    docker_registry="kolibri"
fi

if [ -z "$docker_tag" ]; then
    docker_tag=$(git -C "$kornevaya" rev-parse --short HEAD)
fi

run_all_opts=()
if [ "$propustit_iso" -ne 0 ]; then
    run_all_opts+=("--skip-iso")
fi
if [ "$propustit_wasm" -ne 0 ]; then
    run_all_opts+=("--skip-wasm")
fi
if [ "$propustit_klaster" -ne 0 ]; then
    run_all_opts+=("--skip-cluster")
else
    run_all_opts+=("-n" "3" "-d" "10")
fi

# Запускаем основной конвейер сборки/тестов/артефактов.
"$kornevaya/scripts/run_all.sh" "${run_all_opts[@]}"

artifacts=()
if [ "$propustit_iso" -eq 0 ]; then
    iso_path="$postroika/kolibri.iso"
    if [ ! -f "$iso_path" ]; then
        echo "[Ошибка] Ожидалось найти $iso_path после сборки" >&2
        exit 1
    fi
    artifacts+=("$iso_path")
fi

if [ "$propustit_wasm" -eq 0 ]; then
    wasm_path="$postroika/wasm/kolibri.wasm"
    if [ ! -f "$wasm_path" ]; then
        echo "[Ошибка] Ожидалось найти $wasm_path после сборки" >&2
        exit 1
    fi
    artifacts+=("$wasm_path" "$postroika/wasm/kolibri.wasm.sha256")
fi

"$kornevaya/scripts/build_iso.sh" --kernel-only

kernel_bin="$postroika/kolibri.bin"
if [ ! -f "$kernel_bin" ]; then
    echo "[Ошибка] Не удалось найти $kernel_bin" >&2
    exit 1
fi

boot_bin="$postroika/kolibri_boot.bin"
nasm -f bin "$kornevaya/boot/kolibri.asm" -o "$boot_bin"

kernel_size=$(stat -c '%s' "$kernel_bin")
kernel_sectors=$(( (kernel_size + 511) / 512 ))

python3 - "$boot_bin" "$kernel_size" "$kernel_sectors" <<'PY'
import struct
import sys

boot_path, kernel_size, sectors = sys.argv[1:4]
kernel_size = int(kernel_size)
sectors = int(sectors)

with open(boot_path, 'r+b') as boot:
    data = boot.read()
    marker = data.find(b'KSEC')
    if marker == -1:
        raise SystemExit('не найден маркер KSEC')
    boot.seek(marker + 4)
    boot.write(struct.pack('<H', sectors))
    marker = data.find(b'KBYT')
    if marker == -1:
        raise SystemExit('не найден маркер KBYT')
    boot.seek(marker + 4)
    boot.write(struct.pack('<I', kernel_size))
PY

disk_image="$postroika/kolibri.img"
dd if=/dev/zero of="$disk_image" bs=512 count=$((kernel_sectors + 1)) status=none
dd if="$boot_bin" of="$disk_image" bs=512 count=1 conv=notrunc status=none
dd if="$kernel_bin" of="$disk_image" bs=512 seek=1 conv=notrunc status=none
artifacts+=("$disk_image")

metadannye="$reliz_dir/METADATA.txt"
commit=$(git -C "$kornevaya" rev-parse --verify HEAD)
cat >"$metadannye" <<META
Kolibri release snapshot
Коммит: $commit
Дата: $(date -u +"%Y-%m-%dT%H:%M:%SZ")
META

# Копируем ключевые документы в релиз.
cp "$kornevaya/README.md" "$reliz_dir/README.md"
cp "$kornevaya/LICENSE" "$reliz_dir/LICENSE"
cp "$kornevaya/docs/release_notes.md" "$reliz_dir/release_notes.md"
cp "$kornevaya/docs/kolibri_integrated_prototype.md" "$reliz_dir/kolibri_integrated_prototype.md"

# Подготовка каталога с артефактами.
reliz_payload="$reliz_dir/payload"
rm -rf "$reliz_payload"
mkdir -p "$reliz_payload"
for art in "${artifacts[@]}"; do
    cp "$art" "$reliz_payload/"
    if command -v sha256sum >/dev/null 2>&1; then
        (cd "$reliz_payload" && sha256sum "$(basename "$art")" > "$(basename "$art").sha256")
    fi
done
cp "$metadannye" "$reliz_payload/"

arhiv="$reliz_dir/kolibri-$(date -u +"%Y%m%dT%H%M%SZ").tar.gz"
tar -czf "$arhiv" -C "$reliz_payload" .

if [ "$propustit_docker" -eq 0 ]; then
    if ! command -v docker >/dev/null 2>&1; then
        echo "[Ошибка] Docker не найден в PATH, а публикация образов не отключена." >&2
        exit 1
    fi

    registry_trim="${docker_registry%/}"
    declare -a docker_components=(backend frontend training)

    for component in "${docker_components[@]}"; do
        dockerfile="$kornevaya/$component/Dockerfile"
        if [ ! -f "$dockerfile" ]; then
            echo "[Ошибка] Не найден Dockerfile: $dockerfile" >&2
            exit 1
        fi

        image="${registry_trim}/kolibri-${component}:${docker_tag}"
        echo "[Docker] Сборка образа $image"
        docker build -f "$dockerfile" -t "$image" "$kornevaya"
        echo "[Docker] Публикация образа $image"
        docker push "$image"
    done
else
    echo "[Docker] Сборка Docker-образов пропущена по флагу --skip-docker"
fi

echo "[Готово] Релизный архив создан: $arhiv"
