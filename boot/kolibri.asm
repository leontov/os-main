; Kolibri OS загрузчик: читает ядро с диска в память, подготавливает
; конфигурацию и передаёт управление точке входа ядра в защищённом режиме.

BITS 16
ORG 0x7C00

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00

    mov [boot_drive], dl
    mov word [destination_segment], 0x1000
    mov word [destination_offset], 0

    mov si, welcome_msg
    call bios_print

    mov di, [kernel_sectors]
    mov si, 1               ; LBA = 1 (сектор после загрузчика)

load_loop:
    cmp di, 0
    je load_done
    push di
    push si
    call read_sector
    jc disk_error
    pop si
    inc si
    pop di
    dec di
    jmp load_loop

load_done:
    mov ax, 0x0040
    mov es, ax
    mov word [es:0x0072], 0 ; BIOS warm boot flag -> холодный старт после возврата

    mov ax, 0x0000
    mov es, ax
    mov dword [0x8000], 20250923
    mov dword [0x8004], 1
    mov word  [0x8008], 0x1F90
    mov word  [0x800A], 0

    call enable_a20
    call enter_protected_mode

disk_error:
    mov si, disk_msg
    call bios_print
hang:
    hlt
    jmp hang

; --- BIOS helper routines ---

bios_print:
    lodsb
    cmp al, 0
    je .done
    mov ah, 0x0E
    mov bh, 0
    mov bl, 0x07
    int 0x10
    jmp bios_print
.done:
    ret

read_sector:
    push ax
    push bx
    push cx
    push dx
    push si

    mov ax, si
    call lba_to_chs
    mov ah, 0x02
    mov al, 1
    mov ch, byte [chs_cylinder]
    mov cl, byte [chs_sector]
    mov dh, byte [chs_head]
    mov dl, [boot_drive]
    mov bx, [destination_offset]
    mov es, word [destination_segment]
    int 0x13
    jc .fail
    add word [destination_offset], 512
    cmp word [destination_offset], 0
    jne .ok
    add word [destination_segment], 0x20
.ok:
    clc
    jmp .done
.fail:
    stc
.done:
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret

lba_to_chs:
    xor dx, dx
    mov bx, 18               ; секторов на дорожке
    div bx
    mov byte [chs_sector], dl
    xor dx, dx
    mov bx, 2
    div bx
    mov byte [chs_head], dl
    mov byte [chs_cylinder], al
    inc byte [chs_sector]
    ret

enable_a20:
    in al, 0x64
.wait_input:
    test al, 0x02
    jnz .wait_input
    mov al, 0xD1
    out 0x64, al
.wait_ready:
    in al, 0x64
    test al, 0x02
    jnz .wait_ready
    mov al, 0xDF
    out 0x60, al
    ret

enter_protected_mode:
    cli
    lgdt [gdt_descriptor]
    mov eax, cr0
    or eax, 0x1
    mov cr0, eax
    jmp 0x08:protected_entry

; --- Protected mode section ---

BITS 32

protected_entry:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x00900000

    mov esi, 0x00010000
    mov edi, 0x00100000
    mov ecx, [kernel_bytes]
    mov ebx, ecx
    shr ecx, 2
    rep movsd
    mov ecx, ebx
    and ecx, 3
    rep movsb

    mov eax, 0x36D76289
    xor ebx, ebx
    mov esi, 0x00008000
    jmp 0x08:0x00100000

; --- Data ---

BITS 16

boot_drive:             db 0
destination_segment:    dw 0x1000
destination_offset:     dw 0x0000
chs_sector:             db 0
chs_head:               db 0
chs_cylinder:           db 0
kernel_marker:          db 'KSEC'
kernel_sectors:         dw 0
kernel_bytes_marker:    db 'KBYT'
kernel_bytes:           dd 0

welcome_msg: db "Kolibri loader", 0
disk_msg:    db "Disk error", 0

align 16
gdt_start:
    dq 0x0000000000000000
    dq 0x00CF9A000000FFFF
    dq 0x00CF92000000FFFF
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

times 510-($-$$) db 0
dw 0xAA55
