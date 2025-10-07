; Kolibri OS начальный код с Multiboot2 заголовком
SECTION .multiboot
align 8
MULTIBOOT_MAGIC    equ 0xE85250D6
MULTIBOOT_ARCH     equ 0
MULTIBOOT_HEADER_LEN equ multiboot_end - multiboot_header
MULTIBOOT_CHECKSUM equ 0x100000000 - (MULTIBOOT_MAGIC + MULTIBOOT_ARCH + MULTIBOOT_HEADER_LEN)

dd MULTIBOOT_MAGIC
dd MULTIBOOT_ARCH
dd MULTIBOOT_HEADER_LEN
dd MULTIBOOT_CHECKSUM

multiboot_header:
    ; Тег окончания списка
    dw 0                    ; тип = 0 (end)
    dw 0                    ; флаг
    dd 8                    ; размер
multiboot_end:

SECTION .text
bits 32
global kolibri_boot_entry
extern kolibri_kernel_main

kolibri_boot_entry:
    cli
    mov esp, stack_top
    push ebx                 ; multiboot_info
    push eax                 ; multiboot_magic
    call kolibri_kernel_main
petlya_zavison:
    hlt
    jmp petlya_zavison

SECTION .bss
align 16
stack_nachalo:
    resb 8192
stack_top:
