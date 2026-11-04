OUTPUT_FORMAT(elf64-x86-64)
ENTRY(_start)

PHDRS
{
    text    PT_LOAD FLAGS(5);   /* r-x */
    rodata  PT_LOAD FLAGS(4);   /* r-- */
    data    PT_LOAD FLAGS(6);   /* rw- */
}

SECTIONS {
    . = 0xFFFFFFFF80000000;

    .limine_requests : {
        . = ALIGN(8);
        KEEP(*(.limine_requests_start))
        . = ALIGN(8);
        KEEP(*(.limine_requests))
        . = ALIGN(8);
        KEEP(*(.limine_requests_end))
    } :text

    .text : {
        *(.text*)
    } :text

    . = ALIGN(0x1000); 

    .rodata : {
        *(.rodata*)
    } :rodata

    . = ALIGN(0x1000); 
    
    .data : {
        *(.data*)
    } :data

    .bss : {
        *(COMMON)
        *(.bss*)
    } :data
}