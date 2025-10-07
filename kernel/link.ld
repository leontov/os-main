ENTRY(kolibri_boot_entry)

SECTIONS
{
    . = 1M;
    .text :
    {
        *(.multiboot)
        *(.text*)
    }
    .rodata : { *(.rodata*) }
    .data : { *(.data*) }
    .bss :
    {
        *(.bss*)
        *(COMMON)
    }
}
