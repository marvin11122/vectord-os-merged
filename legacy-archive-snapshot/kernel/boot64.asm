; VECTORD OS - 64-bit Bootloader
; Multiboot2 compliant, switches to Long Mode (x86-64)
; Compiled as 64-bit ELF but starts in 32-bit mode

[BITS 32]

; Multiboot2 header - must be in first 32KB and aligned to 8 bytes
section .multiboot2
    align 8
    dd 0xE8525036          ; Magic
    dd 0                   ; Flags
    dd -(0xE8525036)      ; Checksum
    ; End tag
    dw 0
    dw 8
    dd 0

; Stack for boot phase
section .bss
    align 16
    stack_bottom:
        resb 16384
    stack_top:

; Page tables
section .data
    align 4096
pml4:
    resq 512
pdpt:
    resq 512

section .text
global _start64
extern kernel64_main

_start64:
    ; Setup stack
    mov esp, stack_top
    cld
    
    ; Save multiboot info
    push ebx        ; boot_info
    push eax        ; boot_magic
    
    ; ============================================
    ; Enable Long Mode
    ; ============================================
    
    ; 1. Check CPU support
    mov eax, 0x80000000
    cpuid
    cmp eax, 0x80000001
    jb .no_longmode
    
    mov eax, 0x80000001
    cpuid
    test edx, (1<<29)
    jz .no_longmode
    
    ; 2. Enable PAE
    mov eax, cr4
    or eax, (1<<5)
    mov cr4, eax
    
    ; 3. Build page tables - identity map first 1GB
    lea eax, [pdpt]
    or eax, 0x03
    mov [pml4], eax
    
    lea eax, [pdpt + 4096]
    or eax, 0x03
    mov [pdpt], eax
    
    mov ecx, 512
    mov eax, 0x00000000
    lea ebx, [pdpt + 4096]
.setup_pd:
    mov [ebx], eax
    or eax, 0x83
    add ebx, 8
    add eax, 0x200000
    loop .setup_pd
    
    ; 4. Load CR3
    mov eax, pml4
    mov cr3, eax
    
    ; 5. Enable long mode via EFER
    mov ecx, 0xC0000080
    rdmsr
    or eax, (1<<8)
    wrmsr
    
    ; 6. Enable paging
    mov eax, cr0
    or eax, (1<<31)
    mov cr0, eax
    
    ; 7. Load GDT and jump to 64-bit
    lgdt [gdt64_ptr]
    jmp 0x08:.enter64

.no_longmode:
    mov esi, msg_no_lm
    call print32
    jmp .hang

.enter64:
    [BITS 64]
    
    ; Setup segment registers
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    
    ; Setup stack
    mov rsp, stack_top
    cld
    
    ; Restore multiboot info
    pop rdi
    pop rsi
    
    ; Call kernel main
    call kernel64_main
    
.hang:
    cli
    hlt
    jmp .hang

[BITS 32]
print32:
    mov ah, 0x0E
    xor bh, bh
.loop:
    lodsb
    or al, al
    jz .done
    int 0x10
    jmp .loop
.done:
    ret

msg_no_lm db "No Long Mode!", 0

; 64-bit GDT
[BITS 64]
gdt64:
    dq 0
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 0x9A
    db 0xAF
    db 0x00
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 0x92
    db 0xCF
    db 0x00

gdt64_ptr:
    dw $ - gdt64 - 1
    dq gdt64
