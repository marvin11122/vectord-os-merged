.section .multiboot2,"a"
.align 8
mb2_start:
.long 0xE85250D6
.long 0
.long mb2_end-mb2_start
.long -(0xE85250D6 + (mb2_end-mb2_start))
.short 0
.short 0
.long 8
mb2_end:

.section .bss
.align 16
stack_bottom: .skip 32768
stack_top:
.align 4096
pml4: .skip 4096
.align 4096
pdpt: .skip 4096
.align 4096
pd: .skip 4096
.align 8
boot_magic: .long 0
boot_info: .long 0

.section .text
.code32
.global _start
.type _start,@function
.extern kernel_main
_start:
    cli
    mov $stack_top, %esp
    mov %eax, boot_magic
    mov %ebx, boot_info

    mov $0x80000000, %eax
    cpuid
    cmp $0x80000001, %eax
    jb fail32
    mov $0x80000001, %eax
    cpuid
    test $(1<<29), %edx
    jz fail32

    mov %cr4, %eax
    or $(1<<5), %eax
    mov %eax, %cr4

    mov $pdpt, %eax
    or $3, %eax
    mov %eax, pml4
    movl $0, pml4+4
    mov $pd, %eax
    or $3, %eax
    mov %eax, pdpt
    movl $0, pdpt+4

    xor %ecx,%ecx
1:
    mov %ecx,%eax
    shl $21,%eax
    or $0x83,%eax
    mov %eax,pd(,%ecx,8)
    movl $0,pd+4(,%ecx,8)
    inc %ecx
    cmp $512,%ecx
    jne 1b

    mov $pml4,%eax
    mov %eax,%cr3
    mov $0xC0000080,%ecx
    rdmsr
    or $(1<<8),%eax
    wrmsr
    mov %cr0,%eax
    or $(1<<31),%eax
    mov %eax,%cr0
    lgdt gdt_ptr
    ljmp $0x08,$long_mode

fail32:
    cli
2:  hlt
    jmp 2b

.code64
long_mode:
    mov $0x10,%ax
    mov %ax,%ds
    mov %ax,%es
    mov %ax,%ss
    xor %eax,%eax
    mov %ax,%fs
    mov %ax,%gs
    mov $stack_top,%rsp
    and $-16,%rsp
    mov boot_magic(%rip),%edi
    mov boot_info(%rip),%esi
    call kernel_main
3:  cli
    hlt
    jmp 3b

.align 8
gdt:
.quad 0
.quad 0x00AF9A000000FFFF
.quad 0x00CF92000000FFFF
gdt_end:
gdt_ptr:
.word gdt_end-gdt-1
.quad gdt

.section .note.GNU-stack,"",@progbits
