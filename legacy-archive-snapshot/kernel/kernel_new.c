/* ═══════════════════════════════════════════════════════════════════════
 * VECTORD OS - Microkernel (Serial Mode)
 * Standalone. No Linux. No Unix. Pure vector mathematics.
 * ═══════════════════════════════════════════════════════════════════════ */

#include "../include/vecord.h"

/* RTC time structure (from rtc.c) */
typedef struct {
    u16 year;
    u8  month;
    u8  day;
    u8  hour;
    u8  minute;
    u8  second;
    u8  century;
} rtc_time_t;

extern rtc_time_t rtc_read_time(void);

/* ═══════════════════════════════════════════════════════════════════════
 * EXTERNAL SYMBOLS FROM BOOT.ASM
 * ═══════════════════════════════════════════════════════════════════════ */

extern void gdt_flush(u32 gdt_ptr);
extern void idt_load(u32 idt_ptr);
extern void syscall_stub(void);

/* ISR/IRQ stubs */
extern void isr0(void);  extern void isr1(void);  extern void isr2(void);
extern void isr3(void);  extern void isr4(void);  extern void isr5(void);
extern void isr6(void);  extern void isr7(void);  extern void isr8(void);
extern void isr9(void);  extern void isr10(void); extern void isr11(void);
extern void isr12(void); extern void isr13(void); extern void isr14(void);
extern void isr15(void); extern void isr16(void); extern void isr17(void);
extern void isr18(void); extern void isr19(void); extern void isr20(void);
extern void isr21(void); extern void isr22(void); extern void isr23(void);
extern void isr24(void); extern void isr25(void); extern void isr26(void);
extern void isr27(void); extern void isr28(void); extern void isr29(void);
extern void isr30(void); extern void isr31(void);

extern void irq0(void);  extern void irq1(void);  extern void irq2(void);
extern void irq3(void);  extern void irq4(void);  extern void irq5(void);
extern void irq6(void);  extern void irq7(void);  extern void irq8(void);
extern void irq9(void);  extern void irq10(void); extern void irq11(void);
extern void irq12(void); extern void irq13(void); extern void irq14(void);
extern void irq15(void);

/* ═══════════════════════════════════════════════════════════════════════
 * KERNEL LOG BUFFER (for dmesg)
 * ═══════════════════════════════════════════════════════════════════════ */

#define LOG_BUFFER_SIZE 4096
static char log_buffer[LOG_BUFFER_SIZE];
static int log_pos = 0;

static void log_msg(const char *msg) {
    /* Copy message to log buffer, wrap around if needed */
    while (*msg) {
        log_buffer[log_pos] = *msg++;
        log_pos = (log_pos + 1) % LOG_BUFFER_SIZE;
    }
    /* Ensure null terminator? Not needed for ring buffer */
}

/* Helper to print log buffer to serial */
static void dmesg_print(void) {
    serial_puts("=== Kernel Log ===\n");
    /* Simple linear scan, may not be perfect for wrap */
    for (int i = 0; i < LOG_BUFFER_SIZE; i++) {
        if (log_buffer[i] == '\0') break; /* We don't store nulls, but as sentinel */
        serial_putc(log_buffer[i]);
    }
    serial_puts("\n=== End Log ===\n");
}

/* ═══════════════════════════════════════════════════════════════════════
 * GDT
 * ═══════════════════════════════════════════════════════════════════════ */

static struct gdt_entry gdt[6];
static struct gdt_ptr gp;

static void gdt_set_gate(int num, u32 base, u32 limit, u8 access, u8 gran) {
    gdt[num].base_low = base & 0xFFFF;
    gdt[num].base_middle = (base >> 16) & 0xFF;
    gdt[num].base_high = (base >> 24) & 0xFF;
    gdt[num].limit_low = limit & 0xFFFF;
    gdt[num].granularity = ((limit >> 16) & 0x0F) | (gran & 0xF0);
    gdt[num].access = access;
}

void gdt_init(void) {
    gp.limit = sizeof(gdt) - 1;
    gp.base = (u32)&gdt;
    
    gdt_set_gate(0, 0, 0, 0, 0);               /* Null segment */
    gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF); /* Kernel code */
    gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF); /* Kernel data */
    gdt_set_gate(3, 0, 0xFFFFFFFF, 0xFA, 0xCF); /* User code */
    gdt_set_gate(4, 0, 0xFFFFFFFF, 0xF2, 0xCF); /* User data */
    gdt_set_gate(5, 0, 0, 0, 0);               /* TSS placeholder */
    
    gdt_flush((u32)&gp);
}

/* ═══════════════════════════════════════════════════════════════════════
 * IDT
 * ═══════════════════════════════════════════════════════════════════════ */

static struct idt_entry idt[256];
static struct idt_ptr idtp;
static u32 isr_routines[256];

void idt_set_gate(u8 num, u32 base, u16 sel, u8 flags) {
    idt[num].base_low = base & 0xFFFF;
    idt[num].base_high = (base >> 16) & 0xFFFF;
    idt[num].sel = sel;
    idt[num].always0 = 0;
    idt[num].flags = flags;
}

static void idt_install(void) {
    idtp.limit = sizeof(idt) - 1;
    idtp.base = (u32)&idt;
    idt_load((u32)&idtp);
}

/* Remap PIC */
static void pic_remap(void) {
    outb(0x20, 0x11); outb(0xA0, 0x11);
    outb(0x21, 0x20); outb(0xA1, 0x28);
    outb(0x21, 0x04); outb(0xA1, 0x02);
    outb(0x21, 0x01); outb(0xA1, 0x01);
    outb(0x21, 0x0);  outb(0xA1, 0x0);
}

void idt_init(void) {
    idtp.limit = sizeof(idt) - 1;
    idtp.base = 0;
    idt_load((u32)&idtp);
    
    pic_remap();
    
    /* Set ISR gates */
    idt_set_gate(0, (u32)isr0, 0x08, 0x8E);
    idt_set_gate(1, (u32)isr1, 0x08, 0x8E);
    idt_set_gate(2, (u32)isr2, 0x08, 0x8E);
    idt_set_gate(3, (u32)isr3, 0x08, 0x8E);
    idt_set_gate(4, (u32)isr4, 0x08, 0x8E);
    idt_set_gate(5, (u32)isr5, 0x08, 0x8E);
    idt_set_gate(6, (u32)isr6, 0x08, 0x8E);
    idt_set_gate(7, (u32)isr7, 0x08, 0x8E);
    idt_set_gate(8, (u32)isr8, 0x08, 0x8E);
    idt_set_gate(9, (u32)isr9, 0x08, 0x8E);
    idt_set_gate(10, (u32)isr10, 0x08, 0x8E);
    idt_set_gate(11, (u32)isr11, 0x08, 0x8E);
    idt_set_gate(12, (u32)isr12, 0x08, 0x8E);
    idt_set_gate(13, (u32)isr13, 0x08, 0x8E);
    idt_set_gate(14, (u32)isr14, 0x08, 0x8E);
    idt_set_gate(15, (u32)isr15, 0x08, 0x8E);
    idt_set_gate(16, (u32)isr16, 0x08, 0x8E);
    idt_set_gate(17, (u32)isr17, 0x08, 0x8E);
    idt_set_gate(18, (u32)isr18, 0x08, 0x8E);
    idt_set_gate(19, (u32)isr19, 0x08, 0x8E);
    idt_set_gate(20, (u32)isr20, 0x08, 0x8E);
    idt_set_gate(21, (u32)isr21, 0x08, 0x8E);
    idt_set_gate(22, (u32)isr22, 0x08, 0x8E);
    idt_set_gate(23, (u32)isr23, 0x08, 0x8E);
    idt_set_gate(24, (u32)isr24, 0x08, 0x8E);
    idt_set_gate(25, (u32)isr25, 0x08, 0x8E);
    idt_set_gate(26, (u32)isr26, 0x08, 0x8E);
    idt_set_gate(27, (u32)isr27, 0x08, 0x8E);
    idt_set_gate(28, (u32)isr28, 0x08, 0x8E);
    idt_set_gate(29, (u32)isr29, 0x08, 0x8E);
    idt_set_gate(30, (u32)isr30, 0x08, 0x8E);
    idt_set_gate(31, (u32)isr31, 0x08, 0x8E);
    
    /* Set IRQ gates */
    idt_set_gate(32, (u32)irq0, 0x08, 0x8E);
    idt_set_gate(33, (u32)irq1, 0x08, 0x8E);
    idt_set_gate(34, (u32)irq2, 0x08, 0x8E);
    idt_set_gate(35, (u32)irq3, 0x08, 0x8E);
    idt_set_gate(36, (u32)irq4, 0x08, 0x8E);
    idt_set_gate(37, (u32)irq5, 0x08, 0x8E);
    idt_set_gate(38, (u32)irq6, 0x08, 0x8E);
    idt_set_gate(39, (u32)irq7, 0x08, 0x8E);
    idt_set_gate(40, (u32)irq8, 0x08, 0x8E);
    idt_set_gate(41, (u32)irq9, 0x08, 0x8E);
    idt_set_gate(42, (u32)irq10, 0x08, 0x8E);
    idt_set_gate(43, (u32)irq11, 0x08, 0x8E);
    idt_set_gate(44, (u32)irq12, 0x08, 0x8E);
    idt_set_gate(45, (u32)irq13, 0x08, 0x8E);
    idt_set_gate(46, (u32)irq14, 0x08, 0x8E);
    idt_set_gate(47, (u32)irq15, 0x08, 0x8E);
    
    /* Syscall gate (user-accessible) */
    idt_set_gate(0x80, (u32)syscall_stub, 0x08, 0xEE);
    
    idt_install();
    log_msg("IDT initialized\n");
}

/* ═══════════════════════════════════════════════════════════════════════
 * PORT I/O
 * ═══════════════════════════════════════════════════════════════════════ */

void outb(u16 port, u8 val) {
    asm volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

/* Serial port output for debugging */
void serial_init(void) {
    outb(0x3F8 + 1, 0x00);    /* Disable interrupts */
    outb(0x3F8 + 3, 0x80);    /* Enable DLAB */
    outb(0x3F8 + 0, 0x03);    /* Set divisor to 3 (38400 baud) */
    outb(0x3F8 + 1, 0x00);
    outb(0x3F8 + 3, 0x03);    /* 8 bits, no parity, one stop bit */
    outb(0x3F8 + 2, 0xC7);    /* Enable FIFO */
    outb(0x3F8 + 4, 0x0B);    /* IRQs enabled, RTS/DSR set */
}

static int serial_transmit_empty(void) {
    return inb(0x3F8 + 5) & 0x20;
}

void serial_putc(char c) {
    while (!serial_transmit_empty());
    outb(0x3F8, c);
}

void serial_puts(const char *str) {
    for (int i = 0; str[i]; i++) {
        if (str[i] == '\n') serial_putc('\r');
        serial_putc(str[i]);
    }
    /* Also log to kernel log */
    log_msg(str);
}

void serial_puthex(u32 value) {
    const char hex_chars[] = "0123456789ABCDEF";
    serial_putc('0');
    serial_putc('x');
    for (int i = 7; i >= 0; i--) {
        serial_putc(hex_chars[(value >> (i * 4)) & 0xF]);
    }
}

void serial_putdec(u32 value) {
    char buf[12];
    int i = 0;
    
    if (value == 0) {
        serial_putc('0');
        return;
    }
    
    while (value > 0) {
        buf[i++] = '0' + (value % 10);
        value /= 10;
    }
    
    for (int j = i - 1; j >= 0; j--) {
        serial_putc(buf[j]);
    }
}

u8 inb(u16 port) {
    u8 ret;
    asm volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

void outw(u16 port, u16 val) {
    asm volatile ("outw %0, %1" : : "a"(val), "Nd"(port));
}

u16 inw(u16 port) {
    u16 ret;
    asm volatile ("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

void outl(u16 port, u32 val) {
    asm volatile ("outl %0, %1" : : "a"(val), "Nd"(port));
}

u32 inl(u16 port) {
    u32 ret;
    asm volatile ("inl %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

void io_wait(void) {
    outb(0x80, 0);
}
 
/* ═══════════════════════════════════════════════════════════════════════
 * MEMORY MANAGEMENT - First-fit free list allocator with coalescing
 * ═══════════════════════════════════════════════════════════════════════ */
 
extern u32 _kernel_end;
static u32 heap_start;
static u32 heap_end;
 
typedef struct block_header {
    u32 size;                /* Size of usable data (excludes header) */
    u8 free;                 /* 1 if free, 0 if allocated */
    struct block_header *next;
    struct block_header *prev;
} block_header_t;
 
#define BLOCK_HEADER_SIZE ((sizeof(block_header_t) + 15) & ~15)  /* Align to 16 bytes */
 
static block_header_t *first_block = NULL;
 
void mem_init(u32 mem_size) {
    heap_start = (u32)&_kernel_end;
    heap_start = (heap_start + 4095) & ~4095; /* Align to page */
    heap_end = heap_start + mem_size;
    
    /* Initialize heap with one large free block */
    first_block = (block_header_t *)heap_start;
    first_block->size = mem_size - BLOCK_HEADER_SIZE;
    first_block->free = 1;
    first_block->next = NULL;
    first_block->prev = NULL;
    log_msg("Memory heap initialized\n");
}
 
void *kmalloc(u32 size) {
    if (size == 0) return NULL;
    
    /* Align size to 16 bytes */
    size = (size + 15) & ~15;
    
    block_header_t *current = first_block;
    
    while (current) {
        if (current->free && current->size >= size) {
            /* Found a free block that fits */
            /* Split if remainder is large enough for another block */
            if (current->size >= size + BLOCK_HEADER_SIZE + 16) {
                block_header_t *new_block = (block_header_t *)((u8 *)current + BLOCK_HEADER_SIZE + size);
                new_block->size = current->size - size - BLOCK_HEADER_SIZE;
                new_block->free = 1;
                new_block->next = current->next;
                new_block->prev = current;
                if (current->next) {
                    current->next->prev = new_block;
                }
                current->next = new_block;
                current->size = size;
            }
            current->free = 0;
            return (void *)((u8 *)current + BLOCK_HEADER_SIZE);
        }
        current = current->next;
    }
    return NULL;  /* Out of memory */
}
 
void kfree(void *ptr) {
    if (!ptr) return;
    
    block_header_t *block = (block_header_t *)((u8 *)ptr - BLOCK_HEADER_SIZE);
    block->free = 1;
    
    /* Coalesce with next block if free */
    if (block->next && block->next->free) {
        block->size += BLOCK_HEADER_SIZE + block->next->size;
        block->next = block->next->next;
        if (block->next) {
            block->next->prev = block;
        }
    }
    
    /* Coalesce with previous block if free */
    if (block->prev && block->prev->free) {
        block->prev->size += BLOCK_HEADER_SIZE + block->size;
        block->prev->next = block->next;
        if (block->next) {
            block->next->prev = block->prev;
        }
    }
}
 
u32 mem_get_free(void) {
    u32 free_total = 0;
    block_header_t *current = first_block;
    while (current) {
        if (current->free) {
            free_total += current->size;
        }
        current = current->next;
    }
    return free_total;
}
 
u32 mem_get_used(void) {
    u32 used_total = 0;
    block_header_t *current = first_block;
    while (current) {
        if (!current->free) {
            used_total += current->size;
        }
        current = current->next;
    }
    return used_total;
}
 
u32 mem_get_total(void) {
    return heap_end - heap_start;
}

u32 mem_get_free_blocks(void) {
    u32 count = 0;
    block_header_t *current = first_block;
    while (current) {
        if (current->free) {
            count++;
        }
        current = current->next;
    }
    return count;
}

/* ═══════════════════════════════════════════════════════════════════════
 * VECTOR MEMORY - 4D semantic addressing
 * ═══════════════════════════════════════════════════════════════════════ */

static u32 vec_counter = 0;

vec_addr_t vec_alloc(u32 size, u32 semantic_class) {
    vec_addr_t addr;
    void *ptr = kmalloc(size);
    
    if (!ptr) {
        addr.position = VEC4_ZERO;
        addr.epoch = 0;
        addr.flags = 0;
        return addr;
    }
    
    /* Generate semantic position */
    addr.position.x = (float)(vec_counter % 100);
    addr.position.y = (float)(vec_counter / 100);
    addr.position.z = (float)(size / 4096);
    addr.position.w = (float)semantic_class;
    addr.epoch = (u32)ptr;  /* Store physical pointer for resolution */
    addr.flags = 0x03; /* Read | Write */
    
    vec_counter++;
    return addr;
}

void vec_free(vec_addr_t addr) {
    /* Free the underlying memory using stored pointer */
    if (addr.epoch != 0 && addr.epoch != 0xFFFFFFFF) {
        kfree((void *)addr.epoch);
    }
    /* Invalidate the address */
    addr.epoch = 0xFFFFFFFF;
}

void *vec_resolve(vec_addr_t addr) {
    /* Return the stored physical pointer */
    if (addr.epoch == 0 || addr.epoch == 0xFFFFFFFF) {
        return NULL;
    }
    return (void *)addr.epoch;
}

u32 vec_get_counter(void) {
    return vec_counter;
}

/* ═══════════════════════════════════════════════════════════════════════
 * PROCESS SCHEDULER - Geometric momentum-based
 * ═══════════════════════════════════════════════════════════════════════ */

#define MAX_PROCESSES 64
#define DEFAULT_QUANTUM 100

static process_t process_table[MAX_PROCESSES];
static process_t *current_proc = NULL;
static process_t *proc_queue = NULL;
static u32 next_pid = 1;

void proc_init(void) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        process_table[i].pid = 0;
        process_table[i].state = PROC_DORMANT;
        process_table[i].next = NULL;
        process_table[i].prev = NULL;
    }
    log_msg("Process table initialized\n");
}

process_t *proc_create(const char *name, void (*entry)(void)) {
    /* Find free slot */
    process_t *proc = NULL;
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i].state == PROC_DORMANT) {
            proc = &process_table[i];
            break;
        }
    }
    
    if (!proc) return NULL;
    
    proc->pid = next_pid++;
    
    /* Copy name */
    int i;
    for (i = 0; i < 31 && name[i]; i++) {
        proc->name[i] = name[i];
    }
    proc->name[i] = '\0';
    
    /* Geometric properties */
    proc->position = (vec4_t){{0, 0, 0, 1}};
    proc->velocity = VEC4_ZERO;
    proc->momentum = VEC4_ONE;
    proc->rotation = 0.0f;
    
    /* Stack allocation */
    proc->stack_size = 4096;
    proc->stack_base = (u32)kmalloc(proc->stack_size);
    proc->esp = proc->stack_base + proc->stack_size - 4;
    proc->ebp = proc->esp;
    proc->eip = (u32)entry;
    
    /* CPU state */
    proc->eax = 0; proc->ebx = 0; proc->ecx = 0; proc->edx = 0;
    proc->esi = 0; proc->edi = 0;
    proc->eflags = 0x202; /* IF set */
    
    /* State */
    proc->state = PROC_SPINNING;
    proc->quantum = DEFAULT_QUANTUM;
    proc->cpu_time = 0;
    
    /* Add to queue */
    proc->next = proc_queue;
    proc->prev = NULL;
    if (proc_queue) proc_queue->prev = proc;
    proc_queue = proc;
    
    return proc;
}

void proc_destroy(process_t *proc) {
    if (!proc) return;
    
    /* Remove from queue */
    if (proc->prev) proc->prev->next = proc->next;
    if (proc->next) proc->next->prev = proc->prev;
    if (proc_queue == proc) proc_queue = proc->next;
    
    /* Free stack */
    if (proc->stack_base) {
        kfree((void *)proc->stack_base);
    }
    
    proc->state = PROC_DORMANT;
    proc->pid = 0;
}

void proc_schedule(void) {
    if (!proc_queue) return;
    
    /* Find next runnable process */
    process_t *next = proc_queue;
    while (next && next->state != PROC_SPINNING && next->state != PROC_TRANSLATING) {
        next = next->next;
    }
    
    if (!next) return;
    
    /* Save current context if exists */
    if (current_proc) {
        current_proc->state = PROC_SPINNING;
        current_proc->cpu_time++;
    }
    
    /* Load new process */
    current_proc = next;
    current_proc->state = PROC_TRANSLATING;
    current_proc->quantum = DEFAULT_QUANTUM;
}

void proc_yield(void) {
    if (current_proc) {
        current_proc->state = PROC_SPINNING;
        current_proc->quantum = 0;
    }
    proc_schedule();
}

process_t *proc_get_current(void) {
    return current_proc;
}

/* ═══════════════════════════════════════════════════════════════════════
 * KEYBOARD DRIVER - PS/2 (unused in serial mode, but kept for compatibility)
 * ═══════════════════════════════════════════════════════════════════════ */

static volatile char key_buffer[256];
static volatile int key_head = 0;
static volatile int key_tail = 0;
static volatile int shift_pressed = 0;
static volatile int caps_lock = 0;

/* US keyboard layout */
static const char scancode_to_ascii[128] = {
    0, 27, '1','2','3','4','5','6','7','8','9','0','-','=','\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0, 'a','s','d','f','g','h','j','k','l',';','\'','`',
    0, '\\','z','x','c','v','b','n','m',',','.','/',0,
    '*', 0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static const char scancode_to_ascii_shift[128] = {
    0, 27, '!','@','#','$','%','^','&','*','(',')','_','+','\b',
    '\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n',
    0, 'A','S','D','F','G','H','J','K','L',':','"','~',
    0, '|','Z','X','C','V','B','N','M','<','>','?',0,
    '*', 0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

void keyboard_handler(u8 scancode) {
    if (scancode & 0x80) {
        /* Key release */
        scancode &= 0x7F;
        if (scancode == 0x2A || scancode == 0x36) { /* Shift keys */
            shift_pressed = 0;
        }
    } else {
        /* Key press */
        if (scancode == 0x2A || scancode == 0x36) { /* Shift keys */
            shift_pressed = 1;
            return;
        }
        if (scancode == 0x3A) { /* Caps Lock */
            caps_lock = !caps_lock;
            return;
        }
        
        char c;
        if (shift_pressed) {
            c = scancode_to_ascii_shift[scancode];
        } else {
            c = scancode_to_ascii[scancode];
            if (caps_lock && c >= 'a' && c <= 'z') {
                c = c - 'a' + 'A';
            }
        }
        
        if (c) {
            key_buffer[key_head] = c;
            key_head = (key_head + 1) % 256;
        }
    }
}

int keyboard_getchar(void) {
    if (key_head == key_tail) {
        return -1;
    }
    char c = key_buffer[key_tail];
    key_tail = (key_tail + 1) % 256;
    return c;
}

char keyboard_read(void) {
    while (key_head == key_tail) {
        asm volatile ("hlt");
    }
    char c = key_buffer[key_tail];
    key_tail = (key_tail + 1) % 256;
    return c;
}

void keyboard_init(void) {
    key_head = 0;
    key_tail = 0;
    shift_pressed = 0;
    caps_lock = 0;
    /* Enable keyboard IRQ */
    outb(0x21, inb(0x21) & ~0x02);
}

/* ═══════════════════════════════════════════════════════════════════════
 * STRING UTILITIES
 * ═══════════════════════════════════════════════════════════════════════ */

static int strlen(const char *s) {
    int len = 0;
    while (s[len]) len++;
    return len;
}

static int strcmp(const char *a, const char *b) {
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return *(unsigned char*)a - *(unsigned char*)b;
}

static int strncmp(const char *a, const char *b, int n) {
    while (n && *a && *a == *b) {
        a++;
        b++;
        n--;
    }
    if (n == 0) return 0;
    return *(unsigned char*)a - *(unsigned char*)b;
}

static char *strcpy(char *dest, const char *src) {
    char *d = dest;
    while ((*d++ = *src++));
    return dest;
}

static int atoi(const char *s) {
    int n = 0;
    while (*s >= '0' && *s <= '9') {
        n = n * 10 + (*s - '0');
        s++;
    }
    return n;
}

/* ═══════════════════════════════════════════════════════════════════════
 * SIMPLE VIRTUAL FILESYSTEM (in-memory demo)
 * ═══════════════════════════════════════════════════════════════════════ */

#define MAX_VFS_FILES 10
typedef struct {
    char name[32];
    char content[256];
    int size;
    int is_dir;
} vfs_file_t;

static vfs_file_t vfs_root[MAX_VFS_FILES];
static int vfs_file_count = 0;

static void vfs_add_file(const char *name, const char *content, int is_dir) {
    if (vfs_file_count >= MAX_VFS_FILES) return;
    strcpy(vfs_root[vfs_file_count].name, name);
    if (content) {
        strcpy(vfs_root[vfs_file_count].content, content);
        vfs_root[vfs_file_count].size = strlen(content);
    } else {
        vfs_root[vfs_file_count].content[0] = '\0';
        vfs_root[vfs_file_count].size = 0;
    }
    vfs_root[vfs_file_count].is_dir = is_dir;
    vfs_file_count++;
}

static void vfs_init_demo(void) {
    vfs_add_file("/", NULL, 1);
    vfs_add_file("readme.txt", "Welcome to VECTORD OS.\nThis is a demo filesystem.\n", 0);
    vfs_add_file("system.cfg", "kernel=vektrovian\nmode=serial\n", 0);
    vfs_add_file("logs", NULL, 1);
    vfs_add_file("logs/boot.log", "Boot sequence completed.\n", 0);
}

static vfs_file_t* vfs_find(const char *name) {
    for (int i = 0; i < vfs_file_count; i++) {
        if (strcmp(vfs_root[i].name, name) == 0) {
            return &vfs_root[i];
        }
    }
    return NULL;
}

/* ═══════════════════════════════════════════════════════════════════════
 * SHELL COMMANDS
 * ═══════════════════════════════════════════════════════════════════════ */

static void cmd_help(void) {
    serial_puts("VECTORD OS Commands:\n");
    serial_puts("  help              - Show this help\n");
    serial_puts("  ver               - Version info\n");
    serial_puts("  mem               - Memory info (total, used, free)\n");
    serial_puts("  ps                - Process list\n");
    serial_puts("  ls <dir>          - List directory\n");
    serial_puts("  cat <file>        - Display file\n");
    serial_puts("  echo <text>       - Echo text\n");
    serial_puts("  clear             - Clear screen\n");
    serial_puts("  reboot            - Reboot system\n");
    serial_puts("  neural status     - Neural network status\n");
    serial_puts("  netstat           - Network statistics\n");
    serial_puts("  uptime            - System uptime\n");
    serial_puts("  whoami            - Current user\n");
    serial_puts("  date              - Current date/time\n");
    serial_puts("  dmesg             - Kernel log buffer\n");
    serial_puts("  shutdown          - Halt system\n");
}

static void cmd_ver(void) {
    serial_puts("VECTORD OS version " OS_VERSION "\n");
    serial_puts("NEURAL VECTOR codename\n");
    serial_puts("Standalone Kernel - No Linux - No Unix\n");
}

static void cmd_mem(void) {
    serial_puts("Memory Information:\n");
    serial_puts("  Total: ");
    serial_putdec(mem_get_total() / 1024);
    serial_puts(" KB\n");
    serial_puts("  Used:  ");
    serial_putdec(mem_get_used() / 1024);
    serial_puts(" KB\n");
    serial_puts("  Free:  ");
    serial_putdec(mem_get_free() / 1024);
    serial_puts(" KB\n");
}

static void cmd_ps(void) {
    serial_puts("PID  NAME                 STATE\n");
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i].pid != 0) {
            serial_putdec(process_table[i].pid);
            serial_puts("    ");
            serial_puts(process_table[i].name);
            serial_puts("    ");
            switch (process_table[i].state) {
                case PROC_DORMANT: serial_puts("DORMANT"); break;
                case PROC_SPINNING: serial_puts("SPINNING"); break;
                case PROC_ORBITING: serial_puts("ORBITING"); break;
                case PROC_TRANSLATING: serial_puts("TRANSLATING"); break;
                case PROC_REFLECTING: serial_puts("REFLECTING"); break;
                default: serial_puts("UNKNOWN"); break;
            }
            serial_puts("\n");
        }
    }
}

static void cmd_ls(const char *arg) {
    serial_puts("Directory listing:\n");
    for (int i = 0; i < vfs_file_count; i++) {
        if (vfs_root[i].is_dir) {
            serial_puts("[DIR]  ");
        } else {
            serial_puts("[FILE] ");
        }
        serial_puts(vfs_root[i].name);
        serial_puts("\n");
    }
}

static void cmd_cat(const char *arg) {
    if (!arg || arg[0] == '\0') {
        serial_puts("Usage: cat <filename>\n");
        return;
    }
    vfs_file_t *file = vfs_find(arg);
    if (!file) {
        serial_puts("File not found: ");
        serial_puts(arg);
        serial_puts("\n");
        return;
    }
    if (file->is_dir) {
        serial_puts("Is a directory\n");
        return;
    }
    serial_puts(file->content);
}

static void cmd_echo(const char *arg) {
    if (arg) {
        serial_puts(arg);
    }
    serial_puts("\n");
}

static void cmd_clear(void) {
    /* Send ANSI clear screen sequence */
    serial_puts("\033[2J\033[H");
}

static void cmd_reboot(void) {
    serial_puts("Rebooting...\n");
    /* Triple fault to reboot */
    struct idt_ptr null_idt = {0, 0};
    idt_load((u32)&null_idt);
    asm volatile ("int $0x03");
    /* Should not reach */
    while(1);
}

static void cmd_neural_status(void) {
    serial_puts("Neural Network Hardware Optimizer Status:\n");
    serial_puts("Network: 8 inputs -> 12 hidden -> 4 outputs\n");
    serial_puts("Outputs: [0]=CPU  [1]=Memory  [2]=Disk  [3]=Network\n");
    for (int i = 0; i < 4; i++) {
        float pred = neural_hw_predict(i);
        serial_puts("  HW[");
        serial_putdec(i);
        serial_puts("]: ");
        serial_putdec((u32)(pred * 100));
        serial_puts("%\n");
    }
}

static void cmd_netstat(void) {
    serial_puts("Network Statistics:\n");
    /* Placeholder: call net_stats_print if available */
    /* net_stats_print(); */
    serial_puts("Network statistics not yet implemented.\n");
}

static void cmd_uptime(void) {
    /* Use PIT tick counter (need to implement) */
    /* For now, just print something */
    serial_puts("Uptime: 0 seconds (placeholder)\n");
}

static void cmd_whoami(void) {
    serial_puts("root\n");
}

static void cmd_date(void) {
    /* Use RTC to print date/time */
    rtc_time_t t = rtc_read_time();
    serial_puts("Date: ");
    serial_putdec(t.year);
    serial_puts("-");
    serial_putdec(t.month);
    serial_puts("-");
    serial_putdec(t.day);
    serial_puts(" Time: ");
    serial_putdec(t.hour);
    serial_puts(":");
    if (t.minute < 10) serial_putc('0');
    serial_putdec(t.minute);
    serial_puts(":");
    if (t.second < 10) serial_putc('0');
    serial_putdec(t.second);
    serial_puts("\n");
}

static void cmd_dmesg(void) {
    dmesg_print();
}

static void cmd_shutdown(void) {
    serial_puts("Halting system.\n");
    asm volatile ("cli; hlt");
}

/* ═══════════════════════════════════════════════════════════════════════
 * SHELL
 * ═══════════════════════════════════════════════════════════════════════ */

#define MAX_CMD_LEN 128
static char cmd_buffer[MAX_CMD_LEN];
static int cmd_pos = 0;

static void print_prompt(void) {
    serial_puts("vectord> ");
}

static void process_command(const char *cmd) {
    /* Skip leading spaces */
    while (*cmd == ' ') cmd++;
    if (cmd[0] == '\0') return;
    
    /* Parse command and arguments */
    char command[32];
    const char *arg = NULL;
    int i = 0;
    while (cmd[i] && cmd[i] != ' ' && i < 31) {
        command[i] = cmd[i];
        i++;
    }
    command[i] = '\0';
    if (cmd[i] == ' ') {
        arg = &cmd[i+1];
    }
    
    if (strcmp(command, "help") == 0) {
        cmd_help();
    } else if (strcmp(command, "ver") == 0) {
        cmd_ver();
    } else if (strcmp(command, "mem") == 0) {
        cmd_mem();
    } else if (strcmp(command, "ps") == 0) {
        cmd_ps();
    } else if (strcmp(command, "ls") == 0) {
        cmd_ls(arg);
    } else if (strcmp(command, "cat") == 0) {
        cmd_cat(arg);
    } else if (strcmp(command, "echo") == 0) {
        cmd_echo(arg);
    } else if (strcmp(command, "clear") == 0) {
        cmd_clear();
    } else if (strcmp(command, "reboot") == 0) {
        cmd_reboot();
    } else if (strcmp(command, "neural") == 0) {
        if (arg && strcmp(arg, "status") == 0) {
            cmd_neural_status();
        } else {
            serial_puts("Usage: neural status\n");
        }
    } else if (strcmp(command, "netstat") == 0) {
        cmd_netstat();
    } else if (strcmp(command, "uptime") == 0) {
        cmd_uptime();
    } else if (strcmp(command, "whoami") == 0) {
        cmd_whoami();
    } else if (strcmp(command, "date") == 0) {
        cmd_date();
    } else if (strcmp(command, "dmesg") == 0) {
        cmd_dmesg();
    } else if (strcmp(command, "shutdown") == 0) {
        cmd_shutdown();
    } else {
        serial_puts("Unknown command: ");
        serial_puts(command);
        serial_puts("\n");
    }
}

/* Serial input check */
int serial_getchar(void) {
    if (inb(0x3F8 + 5) & 0x01) {
        return inb(0x3F8);
    }
    return -1;
}

static void shell_update(void) {
    int c = serial_getchar();
    if (c == -1) return;
    if (c == 10 || c == 13) {
        cmd_buffer[cmd_pos] = 0;
        serial_puts("\n");
        if (cmd_pos > 0) {
            process_command(cmd_buffer);
        }
        cmd_pos = 0;
        print_prompt();
    } else if (c == 8 || c == 127) {
        if (cmd_pos > 0) {
            cmd_pos--;
            serial_puts("\b \b");
        }
    } else if (c >= 32 && c < 127 && cmd_pos < MAX_CMD_LEN - 1) {
        cmd_buffer[cmd_pos] = c;
        cmd_pos++;
        serial_putc(c);
    }
}

/* ═══════════════════════════════════════════════════════════════════════
 * INTERRUPT HANDLERS
 * ═══════════════════════════════════════════════════════════════════════ */

static const char *exception_names[] = {
    "Division By Zero", "Debug", "Non Maskable Interrupt", "Breakpoint",
    "Into Detected Overflow", "Out of Bounds", "Invalid Opcode", "No Coprocessor",
    "Double Fault", "Coprocessor Segment Overrun", "Bad TSS", "Segment Not Present",
    "Stack Fault", "General Protection Fault", "Page Fault", "Unknown Interrupt",
    "Coprocessor Fault", "Alignment Check", "Machine Check", "Reserved",
    "Reserved", "Reserved", "Reserved", "Reserved", "Reserved", "Reserved",
    "Reserved", "Reserved", "Reserved", "Reserved", "Reserved", "Reserved"
};

void isr_handler(struct interrupt_frame *frame) {
    if (frame->int_no < 32) {
        /* CPU Exception */
        serial_puts("\n*** KERNEL PANIC ***\n");
        serial_puts("Exception: ");
        serial_puts(exception_names[frame->int_no]);
        serial_puts("\nError Code: ");
        serial_puthex(frame->err_code);
        serial_puts("\nEIP: ");
        serial_puthex(frame->eip);
        serial_puts("\nCS: ");
        serial_puthex(frame->cs);
        serial_puts("\nRegisters:\n");
        serial_puts("EAX="); serial_puthex(frame->eax);
        serial_puts(" EBX="); serial_puthex(frame->ebx);
        serial_puts("\n");
        serial_puts("ECX="); serial_puthex(frame->ecx);
        serial_puts(" EDX="); serial_puthex(frame->edx);
        serial_puts("\n");
        serial_puts("ESI="); serial_puthex(frame->esi);
        serial_puts(" EDI="); serial_puthex(frame->edi);
        serial_puts("\n");
        serial_puts("ESP="); serial_puthex(frame->esp);
        serial_puts(" EBP="); serial_puthex(frame->ebp);
        serial_puts("\n");
        /* Halt */
        asm volatile ("cli; hlt");
    }
}

void irq_handler(struct interrupt_frame *frame) {
    /* Keyboard (IRQ 1) - handle BEFORE EOI */
    if (frame->int_no == 33) {
        u8 scancode = inb(0x60);
        keyboard_handler(scancode);
    }
    
    /* Send EOI */
    if (frame->int_no >= 40) {
        outb(0xA0, 0x20);
    }
    outb(0x20, 0x20);
    
    /* Timer (IRQ 0) */
    if (frame->int_no == 32) {
        if (current_proc) {
            current_proc->quantum--;
            if (current_proc->quantum <= 0) {
                proc_schedule();
            }
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════════
 * SYSCALL HANDLER
 * ═══════════════════════════════════════════════════════════════════════ */

u32 syscall_handler(u32 num, u32 arg1, u32 arg2, u32 arg3) {
    switch (num) {
        case SYS_PUTS:
            /* Print string from userspace */
            {
                const char *str = (const char *)arg1;
                serial_puts(str ? str : "(null)");
            }
            return 0;
        case SYS_EXIT:
            if (current_proc) {
                proc_destroy(current_proc);
                proc_schedule();
            }
            return 0;
        case SYS_PID:
            if (current_proc) {
                /* Return PID in eax */
                asm volatile ("mov %0, %%eax" : : "r"(current_proc->pid));
            }
            break;
        case SYS_EXEC:
            /* TODO: Load and execute ELF binary */
            break;
        case SYS_FORK:
            /* TODO: Fork current process */
            break;
        case SYS_WAIT:
            /* TODO: Wait for child process */
            break;
        case SYS_READ:
            /* TODO: Read from file descriptor */
            return -1;
        case SYS_WRITE:
            /* Write to file descriptor */
            {
                u32 fd = arg1;
                const char *buf = (const char *)arg2;
                u32 count = arg3;
                if (fd == 1 || fd == 2) {
                    /* stdout/stderr - write to serial */
                    for (u32 i = 0; i < count; i++) {
                        serial_putc(buf[i]);
                    }
                } else {
                    /* TODO: write to file */
                }
                return count;
            }
        case SYS_OPEN:
            /* TODO: Open file */
            return -1;
        case SYS_CLOSE:
            /* TODO: Close file descriptor */
            return -1;
        case SYS_MALLOC:
            /* TODO: Allocate memory in userspace */
            return -1;
        case SYS_FREE:
            /* TODO: Free userspace memory */
            return -1;
        case SYS_SLEEP:
            /* TODO: Sleep for milliseconds */
            return -1;
        case SYS_MOUNT:
            /* TODO: Mount filesystem */
            return -1;
        case SYS_STAT:
            /* TODO: Get file status */
            return -1;
        case SYS_MKDIR:
            /* TODO: Create directory */
            return -1;
        case SYS_LS:
            /* TODO: List directory */
            return -1;
        default:
            return 0;
    }
}

/* ═══════════════════════════════════════════════════════════════════════
 * PANIC
 * ═══════════════════════════════════════════════════════════════════════ */

void panic(const char *msg) {
    serial_puts("\n*** KERNEL PANIC: ");
    serial_puts(msg);
    serial_puts(" ***\n");
    asm volatile ("cli; hlt");
}

/* ═══════════════════════════════════════════════════════════════════════
 * UTILITY: uptime tick counter (simple)
 * ═══════════════════════════════════════════════════════════════════════ */

static volatile u32 tick_count = 0;

void timer_handler(void) {
    tick_count++;
}

u32 get_ticks(void) {
    return tick_count;
}

/* ═══════════════════════════════════════════════════════════════════════
 * MAIN KERNEL ENTRY POINT
 * ═══════════════════════════════════════════════════════════════════════ */

void kernel_main(u32 magic, u32 mboot_addr) {
    (void)magic;
    (void)mboot_addr;
    
    /* Initialize serial for debugging */
    serial_init();
    log_msg("Serial initialized\n");
    
    serial_puts("\n\n");
    serial_puts("╔══════════════════════════════════════════════════════════════╗\n");
    serial_puts("║            VECTORD OS - NEURAL VECTOR KERNEL                ║\n");
    serial_puts("║                 Serial Mode Boot                            ║\n");
    serial_puts("╚══════════════════════════════════════════════════════════════╝\n\n");
    
    /* Boot sequence */
    serial_puts("[BOOT] Initializing GDT... ");
    gdt_init();
    serial_puts("OK\n");
    
    serial_puts("[BOOT] Initializing IDT... ");
    idt_init();
    serial_puts("OK\n");
    
    serial_puts("[BOOT] Initializing memory manager... ");
    mem_init(4 * 1024 * 1024); /* 4MB heap */
    serial_puts("OK\n");
    
    serial_puts("[BOOT] Initializing process scheduler... ");
    proc_init();
    serial_puts("OK\n");
    
    serial_puts("[BOOT] Initializing keyboard (PS/2)... ");
    keyboard_init();
    serial_puts("OK\n");
    
    serial_puts("[BOOT] Initializing neural hardware optimizer... ");
    neural_hw_init();
    serial_puts("OK\n");
    
    serial_puts("[BOOT] Initializing RTC... ");
    rtc_init();
    serial_puts("OK\n");
    
    serial_puts("[BOOT] Initializing RNG... ");
    rng_init();
    serial_puts("OK\n");
    
    /* Hardware detection with timeouts (skip problematic) */
    serial_puts("[BOOT] Detecting hardware... \n");
    hardware_detect();
    serial_puts("[BOOT] PCI devices: ");
    serial_putdec(pci_device_count);
    serial_puts("\n[BOOT] ATA devices: ");
    serial_putdec(ata_device_count);
    serial_puts("\n[BOOT] USB controllers: ");
    serial_putdec(usb_controller_count);
    serial_puts("\n");
    
    /* VFS init (demo) */
    serial_puts("[BOOT] Initializing VFS... ");
    vfs_init_demo();
    serial_puts("OK\n");
    
    /* Filesystem drivers (ext2, vectord) - skip if not needed */
    serial_puts("[BOOT] Initializing filesystem drivers... \n");
    /* ext2_init(); */  /* May hang without disk */
    /* vectord_fs_init(); */
    serial_puts("[BOOT] Filesystem drivers skipped (no disk)\n");
    
    /* Enable interrupts */
    serial_puts("[BOOT] Enabling interrupts... ");
    asm volatile ("sti");
    serial_puts("OK\n");
    
    serial_puts("\n=== Boot Complete ===\n");
    serial_puts("VECTORD OS " OS_VERSION " ready.\n");
    serial_puts("Type 'help' for available commands.\n\n");
    
    /* Show shell prompt */
    print_prompt();
    
    /* Main shell loop */
    while (1) {
        shell_update();
        asm volatile ("hlt");
    }
}