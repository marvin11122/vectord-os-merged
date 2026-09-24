/* VECTORD OS - 64-bit Kernel Linker Script
 * Two-stage layout: 32-bit boot + 64-bit kernel
 */

ENTRY(_start64_trampoline)

/* Base address for 64-bit kernel (trampoline + main) */
BASE_ADDR = 0x200000;

SECTIONS {
    . = BASE_ADDR;
    
    .text BLOCK(4K) : ALIGN(4K) {
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
