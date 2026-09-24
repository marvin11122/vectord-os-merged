/* VECTORD OS - Kernel Linker Script */

ENTRY(_start)

SECTIONS {
    . = 1M;
    
    .text BLOCK(4K) : ALIGN(4K) {
        *(.multiboot)
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
