#!/usr/bin/env bash
set -euo pipefail

# Скрипт сборки загрузочного ISO с микроядром Kolibri OS.
# Поддерживает кросс-компилятор i686-elf-* и падение обратно
# на системные gcc/ld с поддержкой -m32. Все сообщения — на русском,
# чтобы облегчить диагностику в CI и локально.

kernel_only=0
while [[ $# -gt 0 ]]; do
    case "$1" in
        --kernel-only)
            kernel_only=1
            shift
            ;;
        *)
            echo "Неизвестный параметр: $1" >&2
            exit 1
            ;;
    esac
done

proekt_koren="$(cd "$(dirname "$0")/.." && pwd)"
postroika_dir="$proekt_koren/build"
yadro_dir="$postroika_dir/kernel"
iso_dir="$postroika_dir/iso"

mkdir -p "$yadro_dir" "$iso_dir/boot/grub"

vybrat_instr() {
    local predpochtenie="$1"
    local zamenitel="$2"
    if command -v "$predpochtenie" >/dev/null 2>&1; then
        echo "$predpochtenie"
        return 0
    fi
    if [[ -n "$zamenitel" ]] && command -v "$zamenitel" >/dev/null 2>&1; then
        echo "$zamenitel"
        return 0
    fi
    return 1
}

CROSS_PREFIX="${CROSS_PREFIX:-i686-elf-}"
CC="${CC:-${CROSS_PREFIX}gcc}"
LD="${LD:-${CROSS_PREFIX}ld}"
AS="${AS:-nasm}"
GRUB_MKRESCUE="${GRUB_MKRESCUE:-grub-mkrescue}"

if ! command -v "$CC" >/dev/null 2>&1; then
    CC="$(vybrat_instr "${CROSS_PREFIX}gcc" gcc || true)"
fi
if ! command -v "$LD" >/dev/null 2>&1; then
    LD="$(vybrat_instr "${CROSS_PREFIX}ld" ld || true)"
fi

# Проверяем доступность обязательных инструментов.
otsutstvuet=()
for instrument in "$CC" "$LD" "$AS"; do
    if [[ -z "$instrument" ]] || ! command -v "$instrument" >/dev/null 2>&1; then
        otsutstvuet+=("$instrument")
    fi
done

if [[ $kernel_only -eq 0 ]]; then
    for instrument in "$GRUB_MKRESCUE"; do
        if [[ -z "$instrument" ]] || ! command -v "$instrument" >/dev/null 2>&1; then
            otsutstvuet+=("$instrument")
        fi
    done
fi

if (( ${#otsutstvuet[@]} > 0 )); then
    echo "[ОШИБКА] Не найдены инструменты: ${otsutstvuet[*]}" >&2
    echo "Установите пакет i686-elf-toolchain или gcc-multilib, nasm, grub-mkrescue, xorriso." >&2
    exit 1
fi

# Проверяем, поддерживает ли компилятор целевую архитектуру.
dopolnitelnye_flagi=("-ffreestanding" "-fno-stack-protector" "-fno-pic" "-Wall" "-Wextra" "-O2")
if "$CC" --version | grep -qi "gcc"; then
    dopolnitelnye_flagi+=("-m32")
fi

cc_proverka_obj="$(mktemp "$yadro_dir/cc_check.XXXXXX.o")"
if ! "$CC" "${dopolnitelnye_flagi[@]}" -xc -c -o "$cc_proverka_obj" - <<<'int main(void){return 0;}' 2>/tmp/kolibri_iso_cc.log; then
    cat /tmp/kolibri_iso_cc.log >&2
    echo "[ОШИБКА] Компилятор не поддерживает требуемый режим. Проверьте наличие gcc-multilib или cross-toolchain." >&2
    rm -f "$cc_proverka_obj"
    exit 1
fi
rm -f "$cc_proverka_obj"

if [[ "$LD" == *ld ]]; then
    if ! "$LD" -V 2>&1 | grep -q 'elf_i386'; then
        echo "[ОШИБКА] Компоновщик не поддерживает формат elf_i386" >&2
        exit 1
    fi
fi

set -x
"$AS" -f elf32 "$proekt_koren/kernel/entry.asm" -o "$yadro_dir/entry.o"
"$AS" -f elf32 "$proekt_koren/kernel/interrupts.asm" -o "$yadro_dir/interrupts.o"
kernel_sources=(
    "$proekt_koren/kernel/support.c"
    "$proekt_koren/kernel/random.c"
    "$proekt_koren/kernel/formula.c"
    "$proekt_koren/kernel/genome.c"
    "$proekt_koren/kernel/net.c"
    "$proekt_koren/kernel/ramdisk.c"
    "$proekt_koren/kernel/serial.c"
    "$proekt_koren/kernel/main.c"
)

kernel_objects=()
for src in "${kernel_sources[@]}"; do
    obj="$yadro_dir/$(basename "${src%.c}").o"
    "$CC" "${dopolnitelnye_flagi[@]}" -std=gnu99 \
        -I"$proekt_koren/kernel" \
        -I"$proekt_koren/backend/include" \
        -c "$src" -o "$obj"
    kernel_objects+=("$obj")
done

"$LD" -m elf_i386 -T "$proekt_koren/kernel/link.ld" -o "$postroika_dir/kolibri.bin" \
    "$yadro_dir/entry.o" \
    "$yadro_dir/interrupts.o" \
    "${kernel_objects[@]}"
set +x

if [[ $kernel_only -eq 0 ]]; then
    cp "$postroika_dir/kolibri.bin" "$iso_dir/boot/kolibri.bin"
    cp "$proekt_koren/boot/grub/grub.cfg" "$iso_dir/boot/grub/grub.cfg"

    set -x
    "$GRUB_MKRESCUE" -o "$postroika_dir/kolibri.iso" "$iso_dir"
    set +x

    iso_razmer=$(stat -c '%s' "$postroika_dir/kolibri.iso")
    iso_mb=$(awk -v bytes="$iso_razmer" 'BEGIN {printf "%.2f", bytes/1048576}')
    printf '[ГОТОВО] Образ готов: %s (%s МБ)\n' "$postroika_dir/kolibri.iso" "$iso_mb"
else
    printf '[ГОТОВО] Бинарник ядра собран: %s\n' "$postroika_dir/kolibri.bin"
fi
