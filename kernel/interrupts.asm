; Обработчики аппаратных прерываний Kolibri OS
SECTION .text
bits 32
global isr_timer
global isr_keyboard
extern obrabotat_tajmer
extern obrabotat_klaviaturu

isr_timer:
    pusha
    call obrabotat_tajmer
    popa
    iretd

isr_keyboard:
    pusha
    call obrabotat_klaviaturu
    popa
    iretd
