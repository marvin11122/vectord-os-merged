ENTRY(_start64)
BASE_ADDR = 0x100000;

SECTIONS {
    . = BASE_ADDR;

    .text BLOCK(4K) : ALIGN(4K) {
        KEEP(*(.multiboot2))
        *(.text)
    }

    .rodata BLOCK(4K) : ALIGN(4K) {
        *(.rodata)
    }

    .data BLOCK(4K) : ALIGN(4K) {
        *(.data)
    }

    .bss BLOCK(4K) : ALIGN(4K) {
        *(COMMON)
        *(.bss)
    }

    /DISCARD/ : {
        *(.note*)
        *(.comment*)
        *(.eh_frame*)
    }

    _kernel_start = .;
    _kernel_end = .;
}

