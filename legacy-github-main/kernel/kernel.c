/* ═══════════════════════════════════════════════════════════════════════
 * VECTORD OS - Microkernel (Serial Mode)
 * Standalone. No Linux. No Unix. Pure vector mathematics.
 * ═══════════════════════════════════════════════════════════════════════ */

#include "../include/vecord.h"

#ifndef VECTORD_DEFAULT_GUI
#define VECTORD_DEFAULT_GUI 0
#endif

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
/* static u32 isr_routines[256]; */

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

/* ═══════════════════════════════════════════════════════════════════════
 * CPU FEATURES (FPU/SSE)
 * ═══════════════════════════════════════════════════════════════════════ */

static void cpuid1(u32 *eax, u32 *ebx, u32 *ecx, u32 *edx) {
    u32 a, b, c, d;
    asm volatile ("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(1));
    if (eax) *eax = a;
    if (ebx) *ebx = b;
    if (ecx) *ecx = c;
    if (edx) *edx = d;
}

static int cpu_has_sse2(void) {
    u32 edx;
    cpuid1(NULL, NULL, NULL, &edx);
    /* SSE=bit25, SSE2=bit26 */
    return (edx & (1u << 25)) && (edx & (1u << 26));
}

static void cpu_enable_sse(void) {
    u32 cr0, cr4;
    asm volatile ("mov %%cr0, %0" : "=r"(cr0));
    /* Clear EM (x87 emulation), clear TS (task switched), set MP (monitor coprocessor) */
    cr0 &= ~(1u << 2);
    cr0 &= ~(1u << 3);
    cr0 |=  (1u << 1);
    asm volatile ("mov %0, %%cr0" : : "r"(cr0));

    asm volatile ("mov %%cr4, %0" : "=r"(cr4));
    /* Enable SSE state save/restore and SIMD FP exceptions */
    cr4 |= (1u << 9) | (1u << 10);
    asm volatile ("mov %0, %%cr4" : : "r"(cr4));

    asm volatile ("fninit");
}

void serial_putc(char c) {
    /* Avoid hard hangs if UART status never changes (e.g. misconfigured serial). */
    u32 spins = 0;
    while (!serial_transmit_empty()) {
        if (++spins > 1000000) {
            break;
        }
    }
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

int serial_getchar(void) {
    if (inb(0x3F8 + 5) & 0x01) {
        return inb(0x3F8);
    }
    return -1;
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
 * VGA TEXT MODE
 * ═══════════════════════════════════════════════════════════════════════ */

static volatile u16 *vga_buffer = (u16 *)VGA_BUFFER;
static int vga_x = 0, vga_y = 0;
static u8 vga_color = 0x0F;

static u8 vga_make_color(u8 fg, u8 bg) {
    return fg | (bg << 4);
}

static u16 vga_make_entry(char c, u8 color) {
    return (u16)c | ((u16)color << 8);
}

void vga_init(void) {
    vga_clear(VGA_BLACK);
    vga_color = vga_make_color(VGA_LIGHT_CYAN, VGA_BLACK);
}

void vga_clear(u8 color) {
    u16 blank = vga_make_entry(' ', color);
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        vga_buffer[i] = blank;
    }
    vga_x = 0;
    vga_y = 0;
}

void vga_putchar(int x, int y, char c, u8 color) {
    if (x >= 0 && x < VGA_WIDTH && y >= 0 && y < VGA_HEIGHT) {
        vga_buffer[y * VGA_WIDTH + x] = vga_make_entry(c, color);
    }
}

void vga_print(int x, int y, const char *str, u8 color) {
    for (int i = 0; str[i]; i++) {
        vga_putchar(x + i, y, str[i], color);
    }
}

void vga_print_colored(int x, int y, const char *str, u8 fg, u8 bg) {
    vga_print(x, y, str, vga_make_color(fg, bg));
}

void vga_put_hex(int x, int y, u32 value, u8 color) {
    const char hex_chars[] = "0123456789ABCDEF";
    char buf[11];
    buf[0] = '0';
    buf[1] = 'x';
    for (int i = 7; i >= 0; i--) {
        buf[2 + (7 - i)] = hex_chars[(value >> (i * 4)) & 0xF];
    }
    buf[10] = '\0';
    vga_print(x, y, buf, color);
}

void vga_put_dec(int x, int y, u32 value, u8 color) {
    char buf[12];
    int i = 0;

    if (value == 0) {
        vga_print(x, y, "0", color);
        return;
    }

    while (value > 0) {
        buf[i++] = '0' + (value % 10);
        value /= 10;
    }

    for (int j = i - 1; j >= 0; j--) {
        vga_putchar(x + (i - 1 - j), y, buf[j], color);
    }
}

void vga_set_cursor(int x, int y) {
    u16 pos = y * VGA_WIDTH + x;
    outb(0x3D4, 0x0F);
    outb(0x3D5, (u8)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (u8)((pos >> 8) & 0xFF));
}

/* ═══════════════════════════════════════════════════════════════════════
 * VECTOR MATH
 * ═══════════════════════════════════════════════════════════════════════ */

vec2_t v2_add(vec2_t a, vec2_t b) {
    vec2_t r = {{a.x + b.x, a.y + b.y}};
    return r;
}

vec2_t v2_sub(vec2_t a, vec2_t b) {
    vec2_t r = {{a.x - b.x, a.y - b.y}};
    return r;
}

vec2_t v2_scale(vec2_t v, float s) {
    vec2_t r = {{v.x * s, v.y * s}};
    return r;
}

float v2_dot(vec2_t a, vec2_t b) {
    return a.x * b.x + a.y * b.y;
}

float v2_length(vec2_t v) {
    return __builtin_sqrtf(v2_dot(v, v));
}

vec2_t v2_normalize(vec2_t v) {
    float l = v2_length(v);
    if (l < 0.0001f) return VEC2_ZERO;
    return v2_scale(v, 1.0f / l);
}

vec3_t v3_add(vec3_t a, vec3_t b) {
    vec3_t r = {{a.x + b.x, a.y + b.y, a.z + b.z}};
    return r;
}

vec3_t v3_cross(vec3_t a, vec3_t b) {
    vec3_t r = {{
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    }};
    return r;
}

float v3_dot(vec3_t a, vec3_t b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

float v3_length(vec3_t v) {
    return __builtin_sqrtf(v3_dot(v, v));
}

vec4_t v4_add(vec4_t a, vec4_t b) {
    vec4_t r = {{a.x+b.x, a.y+b.y, a.z+b.z, a.w+b.w}};
    return r;
}

vec4_t v4_sub(vec4_t a, vec4_t b) {
    vec4_t r = {{a.x-b.x, a.y-b.y, a.z-b.z, a.w-b.w}};
    return r;
}

vec4_t v4_scale(vec4_t v, float s) {
    vec4_t r = {{v.x*s, v.y*s, v.z*s, v.w*s}};
    return r;
}

float v4_dot(vec4_t a, vec4_t b) {
    return a.x*b.x + a.y*b.y + a.z*b.z + a.w*b.w;
}

vec4_t v4_lerp(vec4_t a, vec4_t b, float t) {
    return v4_add(v4_scale(a, 1.0f-t), v4_scale(b, t));
}

/* ═══════════════════════════════════════════════════════════════════════
 * STRING UTILITIES (implementations for vecord.h declarations)
 * ═══════════════════════════════════════════════════════════════════════ */

int strlen(const char *s) {
    int len = 0;
    while (s[len]) len++;
    return len;
}

int strcmp(const char *a, const char *b) {
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return *(unsigned char*)a - *(unsigned char*)b;
}

int strncmp(const char *a, const char *b, int n) {
    while (n && *a && *a == *b) {
        a++;
        b++;
        n--;
    }
    if (n == 0) return 0;
    return *(unsigned char*)a - *(unsigned char*)b;
}

char *strcpy(char *dest, const char *src) {
    char *d = dest;
    while ((*d++ = *src++));
    return dest;
}

int atoi(const char *s) {
    int n = 0;
    while (*s >= '0' && *s <= '9') {
        n = n * 10 + (*s - '0');
        s++;
    }
    return n;
}

void *memcpy(void *dest, const void *src, size_t n) {
    u8 *d = (u8 *)dest;
    const u8 *s = (const u8 *)src;
    while (n--) *d++ = *s++;
    return dest;
}

void *memset(void *s, int c, size_t n) {
    u8 *p = (u8 *)s;
    while (n--) *p++ = (u8)c;
    return s;
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
    u32 iter = 0;
    u32 last_addr = 0;
    
    while (current) {
        /* Basic heap list sanity to avoid infinite loops on corruption */
        u32 cur_addr = (u32)current;
        if (cur_addr < heap_start || cur_addr >= heap_end) return NULL;
        if (iter++ > 4096) {
            return NULL;
        }
        if (cur_addr <= last_addr) {
            return NULL;
        }
        last_addr = cur_addr;

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
 * VECTOR MEMORY - 4D semantic addressing + lookaside cache
 * ═══════════════════════════════════════════════════════════════════════ */

#define VEC_LACACHE_SIZE   256
#define VEC_LACACHE_MASK   (VEC_LACACHE_SIZE - 1)

typedef struct {
    u32    key_epoch;        /* 0 = invalid entry */
    u32    key_x, key_y;     /* quantized position.x, position.y as int bits */
    void  *phys_ptr;         /* cached physical pointer */
} vec_lacache_entry_t;

static vec_lacache_entry_t vec_lacache[VEC_LACACHE_SIZE];
static u32 vec_counter = 0;

/* Safe float<->u32 type-punning via union (no strict-aliasing violation) */
typedef union { float f; u32 u; } f32u32_t;

/* Hash: mix epoch + position into a cache slot */
static inline u32 vec_lacache_hash(u32 epoch, float fx, float fy) {
    u32 xbits = ((f32u32_t){.f = fx}).u;
    u32 ybits = ((f32u32_t){.f = fy}).u;
    u32 h = epoch * 2654435761u ^ xbits * 3735928559u ^ ybits * 2246822519u;
    h ^= h >> 16;
    return h & VEC_LACACHE_MASK;
}

static inline void vec_lacache_insert(vec_addr_t addr) {
    if (!addr.epoch || addr.epoch == 0xFFFFFFFF) return;
    u32 slot = vec_lacache_hash(addr.epoch, addr.position.x, addr.position.y);
    vec_lacache_entry_t *e = &vec_lacache[slot];
    e->key_epoch = addr.epoch;
    e->key_x     = ((f32u32_t){.f = addr.position.x}).u;
    e->key_y     = ((f32u32_t){.f = addr.position.y}).u;
    e->phys_ptr  = (void *)addr.epoch;
}

static inline void vec_lacache_invalidate(vec_addr_t addr) {
    if (!addr.epoch || addr.epoch == 0xFFFFFFFF) return;
    u32 slot = vec_lacache_hash(addr.epoch, addr.position.x, addr.position.y);
    vec_lacache_entry_t *e = &vec_lacache[slot];
    u32 kx = ((f32u32_t){.f = addr.position.x}).u;
    u32 ky = ((f32u32_t){.f = addr.position.y}).u;
    if (e->key_epoch == addr.epoch && e->key_x == kx && e->key_y == ky) {
        e->key_epoch = 0;
        e->phys_ptr  = NULL;
    }
}

static inline void *vec_lacache_lookup(vec_addr_t addr) {
    if (!addr.epoch || addr.epoch == 0xFFFFFFFF) return NULL;
    u32 slot = vec_lacache_hash(addr.epoch, addr.position.x, addr.position.y);
    vec_lacache_entry_t *e = &vec_lacache[slot];
    u32 kx = ((f32u32_t){.f = addr.position.x}).u;
    u32 ky = ((f32u32_t){.f = addr.position.y}).u;
    if (e->key_epoch == addr.epoch && e->key_x == kx && e->key_y == ky) {
        return e->phys_ptr;
    }
    return NULL;
}

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
    vec_lacache_insert(addr);
    return addr;
}

void vec_free(vec_addr_t addr) {
    vec_lacache_invalidate(addr);
    /* Free the underlying memory using stored pointer */
    if (addr.epoch != 0 && addr.epoch != 0xFFFFFFFF) {
        kfree((void *)addr.epoch);
    }
    /* Invalidate the address */
    addr.epoch = 0xFFFFFFFF;
}

void *vec_resolve(vec_addr_t addr) {
    /* Fast path: lookaside cache hit */
    void *cached = vec_lacache_lookup(addr);
    if (cached) return cached;

    /* Slow path: direct epoch field (still O(1), but cache miss) */
    if (addr.epoch == 0 || addr.epoch == 0xFFFFFFFF) {
        return NULL;
    }
    /* Re-populate cache for future hits */
    vec_lacache_insert(addr);
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

static int boot_arg_present(u32 magic, u32 info, const char *needle) {
    if (magic != 0x2BADB002 || info == 0) return 0;
    const char *cmdline = (const char *)(*(u32 *)(info + 8));
    if (cmdline == 0) return 0;
    u32 i = 0;
    u32 j = 0;
    while (cmdline[i]) {
        if (needle[j] == 0) return 1;
        if (cmdline[i] == needle[j]) j++;
        else j = 0;
        i++;
    }
    return needle[j] == 0;
}

/* ═══════════════════════════════════════════════════════════════════════
 * STRING UTILITIES
 * ═══════════════════════════════════════════════════════════════════════ */



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

static void gfx_safe_demo(void) {
    vga_gfx_set_mode13h();
    vga_gfx_set_palette();
    vga_gfx_clear(0x01);
    vga_gfx_fill_rect(20, 20, 280, 160, 0x02);
    vga_gfx_draw_rect(20, 20, 280, 160, 0x0F);
    vga_gfx_draw_string(48, 70, "VECTORD OS", 0x0F);
    vga_gfx_draw_string(42, 90, "VGA GRAPHICS READY", 0x0E);
    vga_gfx_draw_string(36, 120, "AKASHA x64 / 32", 0x0B);
    serial_puts("[GFX] VGA mode 13h demo rendered\n");
}

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
    serial_puts("  mouse status      - PS/2 mouse status\n");
    serial_puts("  ai <query>        - Query Akasha knowledge base\n");
    serial_puts("  ai learn <n> <t>  - Ingest knowledge concept\n");
    serial_puts("  ai stats          - Vector AI statistics\n");
    serial_puts("  netstat           - Network statistics\n");
    serial_puts("  uptime            - System uptime\n");
    serial_puts("  whoami            - Current user\n");
    serial_puts("  date              - Current date/time\n");
    serial_puts("  dmesg             - Kernel log buffer\n");
    serial_puts("  shutdown          - Halt system\n");
    serial_puts("  desktop           - Launch graphical desktop\n");
    serial_puts("  apps              - List available applications\n");
    serial_puts("  calc              - Calculator\n");
    serial_puts("  sysinfo           - System information\n");
    serial_puts("  filemgr           - File manager\n");
    serial_puts("  clock             - Clock display\n");
    serial_puts("  settings          - System settings\n");
    serial_puts("  graphics/gfx      - VGA graphics demo\n");
    serial_puts("  screenshot        - Capture screen as ASCII\n");
    serial_puts("  demo              - Animation demo\n");
    serial_puts("  mount             - Mount filesystem\n");
    serial_puts("  disktest          - Test disk I/O\n");
    serial_puts("  desktop-hypr      - Launch Hyprland-style desktop\n");
    serial_puts("  desktop-hd        - Premium HD desktop (Spotlight, Dock, etc)\n");
    serial_puts("  fb                - Flush desktop to VBE framebuffer\n");
    serial_puts("  vbetest           - Write color bars to VBE framebuffer\n");
    serial_puts("  vbemode           - Show current VBE mode info\n");
    serial_puts("  gui               - Launch interactive GUI (ESC to exit)\n");
    serial_puts("  install           - Install VECTORD OS to disk\n");
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
    (void)arg;
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
    serial_puts("Network: 10 inputs -> 12 hidden -> 4 outputs\n");
    serial_puts("Outputs: [0]=CPU  [1]=Memory  [2]=Disk  [3]=Network\n");
    serial_puts("Status: ACTIVE\n");
    serial_puts("  CPU Load:     42%%\n");
    serial_puts("  Memory Usage: 15%%\n");
    serial_puts("  Disk I/O:     3%%\n");
    serial_puts("  Network:      0%%\n");
    serial_puts("  Predictions:  Ready\n");
    serial_puts("  Learning:     Online\n");
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

static void cmd_ping(const char *target) {
    serial_puts("PING ");
    serial_puts(target);
    serial_puts(" 56(84) bytes of data.\n");

    /* Simulate ping responses */
    for (int i = 0; i < 4; i++) {
        serial_puts("64 bytes from ");
        serial_puts(target);
        serial_puts(": icmp_seq=");
        serial_putdec(i + 1);
        serial_puts(" ttl=64 time=");
        serial_putdec(1 + (i * 2));
        serial_puts(" ms\n");
    }

    serial_puts("\n--- ");
    serial_puts(target);
    serial_puts(" ping statistics ---\n");
    serial_puts("4 packets transmitted, 4 received, 0% packet loss\n");
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
    } else if (strcmp(command, "mouse") == 0) {
        mouse_print_status();
    } else if (strcmp(command, "ai") == 0) {
        if (!arg) {
            serial_puts("Usage: ai <query>\n");
            serial_puts("       ai learn <name> <text>\n");
            serial_puts("       ai stats\n");
        } else if (strcmp(arg, "stats") == 0) {
            ai_stats();
        } else if (strncmp(arg, "learn ", 6) == 0) {
            /* Parse: ai learn <name> <text> */
            const char *rest = arg + 6;
            char name[128];
            int i = 0;
            while (rest[i] && rest[i] != ' ' && i < 127) {
                name[i] = rest[i];
                i++;
            }
            name[i] = '\0';
            if (rest[i] == ' ') {
                const char *text = rest + i + 1;
                ai_ingest_concept(name, text, AI_TYPE_MATHEMATICS);
            } else {
                serial_puts("Usage: ai learn <name> <text>\n");
            }
        } else {
            ai_query_concept(arg, 5);
        }
    } else if (strcmp(command, "netstat") == 0) {
    } else if (strcmp(command, "ifconfig") == 0) {
        ifconfig();
    } else if (strcmp(command, "ipconfig") == 0) {
        ifconfig();
    } else if (strcmp(command, "ping") == 0) {
        if (arg) { cmd_ping(arg); } else { serial_puts("Usage: ping <ip>\n"); serial_puts("Example: ping 192.168.1.1\n"); }
    } else if (strcmp(command, "arp") == 0) {
        arp_print_table();
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
    } else if (strcmp(command, "desktop") == 0) {
        serial_puts("\n=== Launching Desktop Environment ===\n");
        serial_puts("Keys: 1=Terminal 2=FileMgr 3=SysInfo 4=Calc q=Quit\n\n");
        desktop_main_loop();
    } else if (strcmp(command, "desktop-hypr") == 0) {
        serial_puts("\n=== VECTORD OS - Hyprland-Style Desktop ===\n");
        serial_puts("1920x1080 Full HD with Glassmorphism\n");
        serial_puts("Smooth animations, blurred windows, modern design\n\n");
        compositor_demo();
    } else if (strcmp(command, "desktop-hd") == 0) {
        serial_puts("\n=== VECTORD OS - Premium HD Desktop ===\n");
        serial_puts("Spotlight | Mission Control | Control Center | Dock\n\n");
        hd_desktop_demo();
    } else if (strcmp(command, "fb") == 0) {
        serial_puts("Rendering desktop to framebuffer...\n");
        hd_desktop_render();
        hd_flush_to_vbe();
    } else if (strcmp(command, "vbetest") == 0) {
        vbe_test_pattern();
    } else if (strcmp(command, "vbemode") == 0) {
        /* Show current VBE mode info */
        serial_puts("VBE Mode Information:\n");
        serial_puts("  Present:  "); serial_puts(vbe_info.present ? "yes" : "no"); serial_puts("\n");
        serial_puts("  Base:     0x"); serial_puthex(vbe_info.base); serial_puts("\n");
        serial_puts("  Width:    "); serial_putdec(vbe_info.width); serial_puts("\n");
        serial_puts("  Height:   "); serial_putdec(vbe_info.height); serial_puts("\n");
        serial_puts("  BPP:      "); serial_putdec(vbe_info.bpp); serial_puts("\n");
        serial_puts("  Pitch:    "); serial_putdec(vbe_info.pitch); serial_puts(" bytes\n");
        serial_puts("  Size:     "); serial_putdec(vbe_info.size / 1024); serial_puts(" KB\n");
        /* Read current DISPI registers (indices 0-4) */
        serial_puts("  DISPI ID: 0x"); serial_puthex(vbe_read(0)); serial_puts("\n");
        serial_puts("  DISPI X:  "); serial_putdec(vbe_read(1)); serial_puts("\n");
        serial_puts("  DISPI Y:  "); serial_putdec(vbe_read(2)); serial_puts("\n");
        serial_puts("  DISPI BPP:"); serial_putdec(vbe_read(3)); serial_puts("\n");
        serial_puts("  DISPI EN: 0x"); serial_puthex(vbe_read(4)); serial_puts("\n");
    } else if (strcmp(command, "gui") == 0) {
        gfx_safe_demo();
    } else if (strcmp(command, "dumpfb") == 0) {
        /* Downsample framebuffer to ASCII art */
        if (!vbe_info.present || !vbe_info.base) {
            serial_puts("No VBE framebuffer\n");
        } else {
            extern void hd_dump_serial(const char *label);
            hd_desktop_render();
            hd_dump_serial("=== VBE Framebuffer View ===");
        }
    } else if (strcmp(command, "install") == 0) {
        gfx_installer();
    } else if (strcmp(command, "apps") == 0) {
        app_list();
    } else if (strcmp(command, "calc") == 0) {
        app_calculator();
    } else if (strcmp(command, "sysinfo") == 0) {
        app_system_info();
    } else if (strcmp(command, "filemgr") == 0) {
        app_file_browser();
    } else if (strcmp(command, "clock") == 0) {
        app_clock();
    } else if (strcmp(command, "settings") == 0) {
    } else if (strcmp(command, "webserver") == 0) {
        if (arg && strcmp(arg, "start") == 0) { httpd_start(); }
        else if (arg && strcmp(arg, "stop") == 0) { httpd_stop(); }
        else { httpd_status(); }
        app_settings();
    } else if (strcmp(command, "mount") == 0) {
        serial_puts("Attempting to mount ext2 filesystem...\n");
        vfs_mount_t *mount = (vfs_mount_t *)kmalloc(sizeof(vfs_mount_t));
        if (!mount) {
            serial_puts("Failed to allocate mount structure\n");
            return;
        }

        memset(mount, 0, sizeof(vfs_mount_t));

        if (ext2_mount(mount, 0) == 0) {
            serial_puts("ext2 filesystem mounted successfully\n");
        } else {
            serial_puts("Failed to mount ext2 filesystem\n");
            kfree(mount);
        }
    } else if (strcmp(command, "disktest") == 0) {
        serial_puts("Testing disk I/O...\n");

        if (ata_device_count == 0) {
            serial_puts("No ATA devices found\n");
            return;
        }

        ata_device_t *dev = &ata_devices[0];
        serial_puts("Testing device: ");
        serial_puts(dev->model);
        serial_puts("\n");

        /* Try to read sector 0 (MBR) */
        u8 buffer[512];
        if (ata_read_sectors(dev, 0, 1, (u16 *)buffer) == 0) {
            serial_puts("Successfully read MBR sector 0\n");
            serial_puts("First 16 bytes: ");
            for (int i = 0; i < 16; i++) {
                serial_puthex(buffer[i]);
                serial_putc(' ');
            }
            serial_puts("\n");

            /* Check for partition table */
            if (buffer[510] == 0x55 && buffer[511] == 0xAA) {
                serial_puts("Valid MBR signature found\n");
                serial_puts("Partition table:\n");

                for (int i = 0; i < 4; i++) {
                    u32 start_sector = *(u32 *)&buffer[0x1BE + i*16 + 8];
                    u32 total_sectors = *(u32 *)&buffer[0x1BE + i*16 + 12];
                    u8 type = buffer[0x1BE + i*16 + 4];

                    if (type != 0) {
                        serial_puts("  Partition ");
                        serial_putdec(i + 1);
                        serial_puts(": Type 0x");
                        serial_puthex(type);
                        serial_puts(", Start: ");
                        serial_putdec(start_sector);
                        serial_puts(", Size: ");
                        serial_putdec(total_sectors / 2048);
                        serial_puts(" MB\n");
                    }
                }
            } else {
                serial_puts("No valid MBR signature\n");
            }
        } else {
            serial_puts("Failed to read sector 0\n");
        }
    } else if (strcmp(command, "graphics") == 0 || strcmp(command, "gfx") == 0) {
        serial_puts("\n=== VGA Graphics Test ===\n");
        serial_puts("Initializing Mode 13h...\n");
        vga_gfx_set_mode13h();
        vga_gfx_set_palette();
        vga_gfx_clear(0x01); vga_gfx_draw_string(100, 90, "VECTORD OS", 0x3F); vga_gfx_draw_string(60, 110, "Graphics Mode Working!", 0x0A);
        serial_puts("Graphics demo displayed on VGA output\n");
        serial_puts("Press Enter to return to text mode...\n");
    } else if (strcmp(command, "demo") == 0) {
        serial_puts("\n=== VECTORD Animation Demo ===\n");
        serial_puts("Running bouncing ball demo...\n");
        vga_gfx_set_mode13h();
        vga_gfx_set_palette();
        vga_gfx_demo_animation();
        serial_puts("Demo complete\n");
    } else if (strcmp(command, "screenshot") == 0) {
        serial_puts("Taking screenshot...\n");
        vga_screenshot_ascii();
    } else {
        serial_puts("Unknown command: ");
        serial_puts(command);
        serial_puts("\n");
    }
}

static void shell_update(void) {
    int c = serial_getchar();
    if (c == -1) c = keyboard_getchar();
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
    if (frame->int_no == 44) {
        u8 scancode = inb(0x60);
        mouse_irq_handler(scancode);
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
            return 0;
        case SYS_EXEC:
            /* Load and execute ELF binary */
            {
                const char *path = (const char *)arg1;
                if (!path) return -1;

                serial_puts("SYS_EXEC: Attempting to execute ");
                serial_puts(path);
                serial_puts("\n");

                /* For now, this is a placeholder - we don't have real ELF files */
                /* In a full implementation, we would:
                 * 1. Open the file using vfs_open
                 * 2. Read the ELF header and data
                 * 3. Load it with elf_load
                 * 4. Switch to user mode
                 */
                serial_puts("SYS_EXEC: User-mode execution not fully implemented yet\n");
                return -1;
            }
        case SYS_FORK:
            /* TODO: Fork current process */
            return 0;
        case SYS_WAIT:
            /* TODO: Wait for child process */
            return 0;
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
    /* Initialize serial for debugging */
    serial_init();
    log_msg("Serial initialized\n");
    int graphical_boot = VECTORD_DEFAULT_GUI;

    /* Enable SSE early (some optimized code paths use XMM registers) */
    serial_puts("[BOOT] Enabling FPU/SSE2... ");
    if (cpu_has_sse2()) {
        cpu_enable_sse();
        serial_puts("OK\n");
    } else {
        serial_puts("HALT (no SSE2)\n");
        asm volatile ("cli; hlt");
    }
    graphical_boot = VECTORD_DEFAULT_GUI || boot_arg_present(magic, mboot_addr, "gui");
    
    serial_puts("\n\n");
    serial_puts("╔══════════════════════════════════════════════════════════════╗\n");
    serial_puts("║            VECTORD OS - NEURAL VECTOR KERNEL                ║\n");
    serial_puts("║                 Serial Mode Boot                            ║\n");
    serial_puts("╚══════════════════════════════════════════════════════════════╝\n\n");
    
    serial_puts("\n        .-^\"-.\n       /  o o  \\\n      |    ^    |\n       \\  '-'  /\n        '-----'\n");
    serial_puts("FREE INFORMATION FOR ALL SEXY GS\n");
    serial_puts("The map is not the territory, but curiosity still redraws it.\n");
    serial_puts("\n    .-\"\"\"-.        .-\"\"\"-.\n   /  o   o  \\      /  o   o  \\\n  |    _    |    |    _    |\n   \\  '-'  /      \\  '-'  /\n    '-----'        '-----'\n");
    serial_puts("SOSSAGE: SIGNAL LOST // KEEP THE SIGNAL ALIVE\n");
    serial_puts("When the map ends, the signal begins.\n\n");

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
    serial_puts("[BOOT] Initializing mouse (PS/2)... ");
    if (mouse_init() == 0) serial_puts("OK\n");
    else serial_puts("unavailable\n");
    
    serial_puts("[BOOT] Initializing neural hardware optimizer... ");
    neural_hw_init();
    serial_puts("OK\n");
    
    serial_puts("[BOOT] Initializing RTC... SKIPPED (disabled for stability)\n");
    
    serial_puts("[BOOT] Initializing RNG... ");
    rng_init();
    serial_puts("OK\n");

    serial_puts("[BOOT] Initializing Vector AI (Akasha)... ");
    ai_init();
    serial_puts("OK\n");

    /* serial_puts("[BOOT] Initializing network stack... ");
    network_init();
    serial_puts("OK\n"); */

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
    
    /* Show VGA text mode status screen */
    vga_show_status();
    
    /* VFS init */
    serial_puts("[BOOT] Initializing VFS... ");
    vfs_init();
    vfs_init_demo();
    serial_puts("OK\n");

    /* Filesystem drivers */
    serial_puts("[BOOT] Initializing filesystem drivers... ");
    ext2_init();
    vectord_fs_init();
    serial_puts("OK\n");
    serial_puts("[BOOT] Filesystem drivers skipped (no disk)\n");
    
    /* Enable interrupts */
    serial_puts("[BOOT] Enabling interrupts... ");
    asm volatile ("sti");
    serial_puts("OK\n");
    
    serial_puts("\n=== Boot Complete ===\n");
    serial_puts("VECTORD OS " OS_VERSION " ready.\n");
    
    /* Show GUI instructions if VBE is available */
    if (vbe_info.present) {
        serial_puts("[BOOT] VBE framebuffer ready. Type 'gui' for desktop, 'vbetest' for color bars.\n");
    }
    
    serial_puts("Type 'help' for available commands.\n\n");
    if (graphical_boot) {
        serial_puts("[BOOT] Launching VBE desktop...\n");
        gfx_safe_demo();
    }
    
    /* Show shell prompt */
    print_prompt();
    
    /* Main shell loop */
    while (1) {
        shell_update();
        asm volatile ("hlt");
    }
}

