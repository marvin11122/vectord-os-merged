/* ═══════════════════════════════════════════════════════════════════════
 * VECTORD OS - ELF32 Binary Loader
 * Loads ELF32 executables into memory and prepares for execution
 * ═══════════════════════════════════════════════════════════════════════ */

#include "../include/vecord.h"

/* Helper: memory copy */
static void elf_memcpy(void *dest, const void *src, u32 n) {
    u8 *d = (u8 *)dest;
    const u8 *s = (const u8 *)src;
    for (u32 i = 0; i < n; i++) {
        d[i] = s[i];
    }
}

/* Helper: memory set */
static void elf_memset(void *dest, u8 val, u32 n) {
    u8 *d = (u8 *)dest;
    for (u32 i = 0; i < n; i++) {
        d[i] = val;
    }
}

/* Validate ELF header */
static int elf_check_header(const elf32_header_t *header) {
    if (header->e_ident[0] != ELF_MAG0 ||
        header->e_ident[1] != ELF_MAG1 ||
        header->e_ident[2] != ELF_MAG2 ||
        header->e_ident[3] != ELF_MAG3) {
        serial_puts("elf: Invalid ELF magic\n");
        return -1;
    }
    
    if (header->e_ident[4] != ELF_CLASS32) {
        serial_puts("elf: Not 32-bit ELF\n");
        return -1;
    }
    
    if (header->e_ident[5] != ELF_DATA2LSB) {
        serial_puts("elf: Not little-endian\n");
        return -1;
    }
    
    if (header->e_machine != ELF_MACHINE_386) {
        serial_puts("elf: Not i386 machine\n");
        return -1;
    }
    
    if (header->e_type != ELF_TYPE_EXEC) {
        serial_puts("elf: Not executable type\n");
        return -1;
    }
    
    return 0;
}

/* Load ELF binary into process memory */
int elf_load(const u8 *buffer, u32 size, process_t *proc) {
    if (!buffer || size < sizeof(elf32_header_t) || !proc) {
        serial_puts("elf: Invalid parameters\n");
        return -1;
    }
    
    const elf32_header_t *header = (const elf32_header_t *)buffer;
    
    /* Validate header */
    if (elf_check_header(header) != 0) {
        return -1;
    }
    
    /* Ensure we have program headers */
    if (header->e_phoff == 0 || header->e_phnum == 0) {
        serial_puts("elf: No program headers\n");
        return -1;
    }
    
    /* Check buffer size for program headers */
    if (header->e_phoff + header->e_phnum * header->e_phentsize > size) {
        serial_puts("elf: Program headers out of bounds\n");
        return -1;
    }
    
    const elf32_phdr_t *phdr = (const elf32_phdr_t *)(buffer + header->e_phoff);
    
    /* Calculate total memory needed for PT_LOAD segments */
    u32 total_mem = 0;
    u32 min_vaddr = 0xFFFFFFFF;
    u32 max_vaddr = 0;
    
    for (int i = 0; i < header->e_phnum; i++) {
        if (phdr[i].p_type == PT_LOAD) {
            u32 seg_start = phdr[i].p_vaddr;
            u32 seg_end = seg_start + phdr[i].p_memsz;
            if (seg_start < min_vaddr) min_vaddr = seg_start;
            if (seg_end > max_vaddr) max_vaddr = seg_end;
        }
    }
    
    if (max_vaddr <= min_vaddr) {
        serial_puts("elf: No loadable segments\n");
        return -1;
    }
    
    total_mem = max_vaddr - min_vaddr;
    
    /* Allocate memory for the process image */
    /* For simplicity, we allocate from kernel heap and assume identity mapping */
    u8 *load_base = (u8 *)kmalloc(total_mem);
    if (!load_base) {
        serial_puts("elf: Failed to allocate memory for ELF\n");
        return -1;
    }
    
    /* Zero out the memory */
    elf_memset(load_base, 0, total_mem);
    
    /* Load each PT_LOAD segment */
    for (int i = 0; i < header->e_phnum; i++) {
        if (phdr[i].p_type != PT_LOAD) continue;
        
        u32 virt_addr = phdr[i].p_vaddr;
        u32 file_offset = phdr[i].p_offset;
        u32 file_size = phdr[i].p_filesz;
        u32 mem_size = phdr[i].p_memsz;
        
        /* Sanity check */
        if (file_offset + file_size > size) {
            serial_puts("elf: Segment data out of file bounds\n");
            kfree(load_base);
            return -1;
        }
        
        /* Calculate destination address relative to load_base */
        u8 *dest = load_base + (virt_addr - min_vaddr);
        
        /* Copy file data */
        if (file_size > 0) {
            elf_memcpy(dest, buffer + file_offset, file_size);
        }
        
        /* Zero out BSS (if mem_size > file_size) */
        if (mem_size > file_size) {
            elf_memset(dest + file_size, 0, mem_size - file_size);
        }
        
        serial_puts("elf: Loaded segment at ");
        serial_puthex(virt_addr);
        serial_puts(" size ");
        serial_putdec(mem_size);
        serial_puts("\n");
    }
    
    /* Set up process memory addresses */
    proc->code_addr = vec_alloc(total_mem, MEM_USER);
    proc->stack_addr = vec_alloc(4096, MEM_USER);

    /* Set entry point - adjust for load address */
    /* Assume the program is loaded at address 0 for simplicity */
    proc->eip = (u32)load_base + (header->e_entry - min_vaddr);
    
    /* Set up a user stack (allocate separate stack memory) */
    u32 stack_size = 4096;
    u8 *stack = (u8 *)kmalloc(stack_size);
    if (!stack) {
        serial_puts("elf: Failed to allocate stack\n");
        kfree(load_base);
        return -1;
    }
    
    /* Initialize stack with zeros */
    elf_memset(stack, 0, stack_size);
    
    /* Set process stack pointers */
    proc->stack_base = (u32)stack;
    proc->stack_size = stack_size;
    proc->esp = proc->stack_base + proc->stack_size - 4;
    proc->ebp = proc->esp;
    
    /* For now, we keep load_base somewhere? We'll store it in proc->eax as a hack */
    proc->eax = (u32)load_base;
    
    serial_puts("elf: Entry point at ");
    serial_puthex(proc->eip);
    serial_puts("\n");
    serial_puts("elf: Stack at ");
    serial_puthex(proc->stack_base);
    serial_puts(" size ");
    serial_putdec(proc->stack_size);
    serial_puts("\n");
    
    return 0;
}

/* Jump to ELF entry point (user mode) */
void elf_jump_to_entry(u32 entry, u32 user_stack) {
    serial_puts("elf_jump_to_entry: Switching to user mode at ");
    serial_puthex(entry);
    serial_puts(" with stack ");
    serial_puthex(user_stack);
    serial_puts("\n");

    /* Switch to user mode (ring 3) and jump to entry */
    /* This sets up the stack for iret with user segments */
    asm volatile (
        "cli\n"                    /* Disable interrupts during switch */
        "mov %0, %%eax\n"         /* Load entry point */
        "mov %1, %%esp\n"         /* Set user stack pointer */
        "pushl $0x23\n"           /* Push user data segment (SS) */
        "pushl %%esp\n"           /* Push user stack pointer */
        "pushf\n"                 /* Push EFLAGS */
        "popl %%ebx\n"            /* Get EFLAGS */
        "orl $0x200, %%ebx\n"     /* Set IF flag (enable interrupts) */
        "pushl %%ebx\n"           /* Push modified EFLAGS */
        "pushl $0x1B\n"           /* Push user code segment (CS) */
        "pushl %%eax\n"           /* Push entry point (EIP) */
        "iret\n"                  /* Return to user mode */
        : : "r"(entry), "r"(user_stack) : "eax", "ebx"
    );

    /* This code should never be reached */
    serial_puts("ERROR: Returned from user mode switch!\n");
}