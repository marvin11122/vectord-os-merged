; VECTORD OS - 64-bit Trampoline (Stage 2)
; This is pure 64-bit code called from boot64.asm after enabling long mode
; Linked to address 0x200000 by linker64.ld

[BITS 64]

section .text
global _start64_trampoline
extern kernel64_main

_start64_trampoline:
    ; Setup segment registers
    mov ax, 0x10        ; Data segment (0x10)
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    
    ; Setup stack (in low memory, below 1MB)
    mov rsp, 0x100000   ; 1MB stack
    
    ; Clear direction flag
    cld
    
    ; Restore multiboot info (pushed by boot64.asm)
    pop rdi             ; boot_info -> rdi (first arg)
    pop rsi             ; boot_magic -> rsi (second arg)
    
    ; Call 64-bit kernel main
    call kernel64_main
    
.hang:
    cli
    hlt
    jmp .hang
