[BITS 32]

section .multiboot2
    align 8
    dd 0xE85250D6
    dd 0
    dd 16
    dd -(0xE85250D6 + 16)
    dw 0
    dw 8
    dd 0

section .bss
    align 16
stack_bottom:
    resb 16384
stack_top:
    align 4096
pml4:
    resb 4096
pdpt:
    resb 4096
pd0:
    resb 4096
pd1:
    resb 4096
pd2:
    resb 4096
pd3:
    resb 4096

section .text
    global _start64
    extern kernel64_main

_start64:
    cli
    mov esp, stack_top
    cld
    mov edi, eax
    mov esi, ebx

    mov eax, 0x80000000
    cpuid
    cmp eax, 0x80000001
    jb .no_long_mode

    mov eax, 0x80000001
    cpuid
    test edx, 1 << 29
    jz .no_long_mode

    mov eax, cr4
    or eax, 1 << 5
    mov cr4, eax

    lea eax, [pdpt]
    or eax, 3
    mov [pml4], eax

    lea eax, [pd0]
    or eax, 3
    mov [pdpt], eax
    lea eax, [pd1]
    or eax, 3
    mov [pdpt + 8], eax
    lea eax, [pd2]
    or eax, 3
    mov [pdpt + 16], eax
    lea eax, [pd3]
    or eax, 3
    mov [pdpt + 24], eax

    lea edi, [pd0]
    mov eax, 0x83
    mov ecx, 512
.pd_loop:
    mov [edi], eax
    add edi, 8
    add eax, 0x200000
    loop .pd_loop

    mov eax, pml4
    mov cr3, eax

    mov ecx, 0xC0000080
    rdmsr
    or eax, 1 << 8
    wrmsr

    lgdt [gdt64_ptr]
    mov eax, cr0
    or eax, 1 << 31
    mov cr0, eax
    push dword 0x10
    push dword .enter64
    retf

.no_long_mode:
    mov esi, msg_no_long_mode
    call print32
.hang:
    cli
    hlt
    jmp .hang

    [BITS 64]
.enter64:
    mov ax, 0x18
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov rsp, stack_top
    cld
    mov rax, cr0
    and eax, 0xFFFFFFFB
    or eax, 0x2
    mov cr0, rax
    mov rax, cr4
    or eax, 0x600
    mov cr4, rax
    call kernel64_main
.hang64:
    cli
    hlt
    jmp .hang64

    [BITS 32]
print32:
    mov ah, 0x0E
    xor bh, bh
.print_loop:
    lodsb
    or al, al
    jz .print_done
    int 0x10
    jmp .print_loop
.print_done:
    ret

msg_no_long_mode db "VECTORD x64: long mode unavailable", 0

    [BITS 64]
    align 8
gdt64:
    dq 0
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 0x9A
    db 0xCF
    db 0x00
    dw 0x0000
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

