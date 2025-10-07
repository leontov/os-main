BITS 32

section .multiboot
align 8
    dd 0x1BADB002
    dd 0
    dd -(0x1BADB002)

section .text
align 4
extern kernel_main

GLOBAL _start
_start:
    cli
    mov esp, stack_top
    push eax        ; multiboot magic
    push ebx        ; multiboot info
    call kernel_main

.hang:
    hlt
    jmp .hang

section .bss
align 16
stack_bottom:
    resb 4096
stack_top:
