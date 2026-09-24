/* VECTORD OS - 64-bit Kernel Main
 * Minimal stub that verifies Long Mode and halts
 */

/* Basic types */
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;

/* Simple 64-bit serial output using IO ports */
static void kmain_outb(u16 port, u8 val) {
    __asm__ volatile ("outb %b0, %w1" : : "a"(val), "Nd"(port));
}

static void kmain_putc(char c) {
    /* Wait for TX buffer empty */
    for (volatile int i = 0; i < 10000; i++) {
        u8 status;
        __asm__ volatile ("inb %w1, %b0" : "=a"(status) : "Nd"(0x3F8 + 5));
        if (status & 0x20) break;
    }
    kmain_outb(0x3F8, c);
}

static void kmain_puts(const char *str) {
    while (*str) {
        kmain_putc(*str++);
    }
}

static void kmain_puthex(u64 value) {
    char hex[] = "0123456789ABCDEF";
    char buf[18];
    buf[0] = '0';
    buf[1] = 'x';
    
    for (int i = 15; i >= 0; i--) {
        buf[2 + (15 - i)] = hex[(value >> (i * 4)) & 0xF];
    }
    buf[18] = '\0';
    
    for (int i = 0; i < 18; i++) {
        kmain_putc(buf[i]);
    }
}

void kernel64_main(u32 boot_magic, u32 boot_info) {
    /* Initialize serial port */
    kmain_outb(0x3F8 + 1, 0);    /* Disable interrupts */
    kmain_outb(0x3F8 + 3, 0x80); /* Enable DLAB */
    kmain_outb(0x3F8 + 0, 0x03); /* Set divisor: 38400 baud */
    kmain_outb(0x3F8 + 1, 0x00);
    kmain_outb(0x3F8 + 3, 0x03); /* 8N1 */
    kmain_outb(0x3F8 + 2, 0xC7); /* Enable FIFO */
    kmain_outb(0x3F8 + 4, 0x0B); /* IRQs enabled, RTS/DSR set */
    
    kmain_puts("\r\n\r\n=== VECTORD OS 64-bit Mode ===\r\n");
    kmain_puts("Boot magic: ");
    kmain_puthex(boot_magic);
    kmain_puts("\r\n");
    kmain_puts("Boot info:  ");
    kmain_puthex(boot_info);
    kmain_puts("\r\n\r\n");
    
    kmain_puts("64-bit Long Mode is working!\r\n");
    kmain_puts("\r\nSystem halted.\r\n");
    
    /* Halt */
    for (;;) {
        __asm__ volatile ("hlt");
    }
}
