# Kolibri OS Prototype (Bare Metal)

Минимальный каркас собственной операционной системы Kolibri.
Работает в QEMU на x86_64 и выводит баннер/шелл в текстовом режиме VGA.

## Требования

- кросс-компилятор `x86_64-elf-gcc` и `x86_64-elf-ld`
- `nasm`
- `grub-mkrescue`
- `qemu-system-x86_64`

На macOS можно установить через `brew` (некоторые пакеты доступны в tap `nativeos/i386-elf-toolchain`).

## Сборка и запуск

```bash
cd kolibri_os
make            # собирает ISO (если доступен grub-mkrescue)
make run        # запускает ELF напрямую через QEMU (-kernel)
make iso-run    # загрузка через GRUB ISO
```

В консоли (serial stdio) появится приглашение `kolibri>`. На текущем этапе ввод команд не обрабатывается — CPU уходит в `hlt`, дальнейшие слои (chain, сеть, память) будут добавлены в следующих итерациях.

## Структура

- `boot/boot.asm` — multiboot-загрузчик
- `kernel/kernel.c` — ядро и примитивный shell (пока без парсера)
- `kernel/link.ld` — линкерный скрипт
- `Makefile` — сборка и запуск через QEMU

## Следующие шаги

1. Ввод/вывод через serial (обработка клавиатуры).
2. Интеграция χ→Φ→S и Kolibri Memory внутри ядра.
3. Реализация Kolibri Chain и файлового журнала.
4. Минимальный сетевой стек (e1000/virtio-net) для ΔΘ обмена.
