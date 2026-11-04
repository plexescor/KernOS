OUTPUT_FORMAT(elf64-x86-64)
ENTRY(_start)

SECTIONS {
    . = 0xFFFFFFFF80000000;

    .limine_requests : {
        . = ALIGN(8);
        KEEP(*(.limine_requests_start))
        . = ALIGN(8);
        KEEP(*(.limine_requests))
        . = ALIGN(8);
        KEEP(*(.limine_requests_end))
    }

    .text : {
        *(.text*)
    }

    .rodata : {
        *(.rodata*)
    }

    .data : {
        *(.data*)
    }

    .bss : {
        *(COMMON)
        *(.bss*)
    }
}