typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;
typedef int i32;

static volatile u16 *const vga_buffer = (volatile u16 *)0xB8000;
static u32 vga_x;
static u32 vga_y;

static void vga_scroll(void) {
    for (u32 row = 1; row < 24; row++) {
        for (u32 column = 0; column < 80; column++) {
            vga_buffer[(row - 1) * 80 + column] = vga_buffer[row * 80 + column];
        }
    }
    for (u32 column = 0; column < 80; column++) vga_buffer[23 * 80 + column] = 0x0F20;
    vga_y = 23;
}

static void vga_putc(char value) {
    if (value == '\n') {
        vga_x = 0;
        if (++vga_y >= 24) vga_scroll();
        return;
    }
    if (value == '\r') {
        vga_x = 0;
        return;
    }
    if (value == '\b') {
        if (vga_x > 0) vga_x--;
        vga_buffer[vga_y * 80 + vga_x] = 0x0F20;
        return;
    }
    if (value < 32 || value > 126) return;
    vga_buffer[vga_y * 80 + vga_x] = (u16)(0x0F00 | (u8)value);
    if (++vga_x >= 80) {
        vga_x = 0;
        if (++vga_y >= 24) vga_scroll();
    }
}

static void vga_clear(void) {
    vga_x = 0;
    vga_y = 0;
    for (u32 i = 0; i < 80 * 24; i++) vga_buffer[i] = 0x0F20;
}

static void kmain_outb(u16 port, u8 value) {
    __asm__ volatile ("outb %b0, %w1" : : "a"(value), "Nd"(port));
}

static u8 kmain_inb(u16 port) {
    u8 value;
    __asm__ volatile ("inb %w1, %b0" : "=a"(value) : "Nd"(port));
    return value;
}

static void kmain_putc(char value) {
    for (volatile u32 i = 0; i < 100000; i++) {
        if (kmain_inb(0x3F8 + 5) & 0x20) break;
    }
    kmain_outb(0x3F8, (u8)value);
    vga_putc(value);
}

static void kmain_puts(const char *text) {
    while (*text) kmain_putc(*text++);
}

static void kmain_puthex(u64 value) {
    const char digits[] = "0123456789ABCDEF";
    char buffer[19];
    buffer[0] = '0';
    buffer[1] = 'x';
    for (int i = 0; i < 16; i++) {
        buffer[2 + i] = digits[(value >> ((15 - i) * 4)) & 0xF];
    }
    for (int i = 0; i < 18; i++) kmain_putc(buffer[i]);
}

static void kmain_putdec(u32 value) {
    char buffer[12];
    int length = 0;
    if (value == 0) {
        kmain_putc('0');
        return;
    }
    while (value > 0 && length < 11) {
        buffer[length++] = (char)('0' + value % 10);
        value /= 10;
    }
    while (length > 0) kmain_putc(buffer[--length]);
}

static void kmain_putdec_signed(i32 value) {
    if (value < 0) {
        kmain_putc('-');
        kmain_putdec((u32)(-value));
        return;
    }
    kmain_putdec((u32)value);
}

static int kmain_getc(void) {
    if (kmain_inb(0x3F8 + 5) & 0x01) return kmain_inb(0x3F8);
    return -1;
}

static void serial_init(void) {
    kmain_outb(0x3F8 + 1, 0x00);
    kmain_outb(0x3F8 + 3, 0x80);
    kmain_outb(0x3F8 + 0, 0x01);
    kmain_outb(0x3F8 + 1, 0x00);
    kmain_outb(0x3F8 + 3, 0x03);
    kmain_outb(0x3F8 + 2, 0xC7);
    kmain_outb(0x3F8 + 4, 0x0B);
}

static int ps2_wait_output(void) {
    for (volatile u32 i = 0; i < 100000; i++) {
        if (!(kmain_inb(0x64) & 0x02)) return 1;
    }
    return 0;
}

static int ps2_wait_input(void) {
    for (volatile u32 i = 0; i < 100000; i++) {
        if (kmain_inb(0x64) & 0x01) return 1;
    }
    return 0;
}

static void ps2_command(u8 command) {
    ps2_wait_output();
    kmain_outb(0x64, command);
}

static u8 ps2_read_data(void) {
    if (!ps2_wait_input()) return 0xFF;
    return kmain_inb(0x60);
}

static void ps2_write_data(u8 data) {
    ps2_wait_output();
    kmain_outb(0x60, data);
}

static u8 ps2_configure(void) {
    ps2_command(0xAE);
    ps2_command(0xA8);
    ps2_command(0x20);
    u8 config = ps2_read_data();
    config |= 0x03;
    config |= 0x20;
    ps2_command(0x60);
    ps2_write_data(config);
    for (volatile u32 i = 0; i < 10000 && (kmain_inb(0x64) & 0x01); i++) {
        (void)kmain_inb(0x60);
    }
    return config;
}

static int mouse_present;
static u8 mouse_packet[3];
static u8 mouse_packet_size;
static i32 mouse_x;
static i32 mouse_y;
static u8 mouse_buttons;
static u32 mouse_bytes;

static int mouse_init(void) {
    mouse_present = 0;
    ps2_command(0xD4);
    ps2_write_data(0xFF);
    u8 ack = ps2_read_data();
    if (ack != 0xFA) {
        kmain_puts("Mouse reset ACK: ");
        kmain_puthex(ack);
        kmain_puts("\r\n");
        return -1;
    }
    (void)ps2_read_data();
    (void)ps2_read_data();
    ps2_command(0xD4);
    ps2_write_data(0xF3);
    (void)ps2_read_data();
    ps2_write_data(100);
    (void)ps2_read_data();
    ps2_command(0xD4);
    ps2_write_data(0xF4);
    (void)ps2_read_data();
    kmain_outb(0xA1, kmain_inb(0xA1) & (u8)~0x02);
    mouse_packet_size = 0;
    mouse_present = 1;
    return 0;
}

static u8 shift_down;
static u8 caps_lock;

static char keyboard_map(u8 scancode) {
    const char normal[] = "\0\0331234567890-=\b\tqwertyuiop[]\n\0asdfghjkl;'`\0\\zxcvbnm,./\0*0 \0";
    const char shifted[] = "\0\033!@#$%^&*()_+\b\tQWERTYUIOP{}\n\0ASDFGHJKL:\"~\0|ZXCVBNM<>?\0*0 \0";
    if (scancode >= sizeof(normal) - 1) return 0;
    char value = shift_down ? shifted[scancode] : normal[scancode];
    if (caps_lock && value >= 'a' && value <= 'z') value = (char)(value - 'a' + 'A');
    return value;
}

static char keyboard_buffer[256];
static u8 keyboard_head;
static u8 keyboard_tail;

static void keyboard_handle(u8 scancode) {
    if (scancode == 0xE0) return;
    if (scancode & 0x80) {
        u8 released = scancode & 0x7F;
        if (released == 0x2A || released == 0x36) shift_down = 0;
        return;
    }
    if (scancode == 0x2A || scancode == 0x36) {
        shift_down = 1;
        return;
    }
    if (scancode == 0x3A) {
        caps_lock = (u8)!caps_lock;
        return;
    }
    char value = keyboard_map(scancode);
    if (!value) return;
    u8 next = (u8)((keyboard_head + 1) & 255);
    if (next != keyboard_tail) {
        keyboard_buffer[keyboard_head] = value;
        keyboard_head = next;
    }
}

static int keyboard_getchar(void) {
    if (keyboard_head == keyboard_tail) return -1;
    char value = keyboard_buffer[keyboard_tail];
    keyboard_tail = (u8)((keyboard_tail + 1) & 255);
    return value;
}

static void mouse_handle(u8 byte) {
    if (!mouse_present) return;
    mouse_bytes++;
    mouse_packet[mouse_packet_size++] = byte;
    if (mouse_packet_size != 3) return;
    mouse_packet_size = 0;
    u8 status = mouse_packet[0];
    if (status & 0xC0) return;
    i32 dx = (i32)(signed char)mouse_packet[1];
    i32 dy = -(i32)(signed char)mouse_packet[2];
    mouse_x += dx;
    mouse_y += dy;
    mouse_buttons = status & 0x07;
}

static void ps2_poll(void) {
    u8 status = kmain_inb(0x64);
    if (!(status & 0x01)) return;
    u8 data = kmain_inb(0x60);
    if (status & 0x20) mouse_handle(data);
    else keyboard_handle(data);
}

static char line_buffer[128];
static u8 line_length;

static int line_equals(const char *line, const char *command) {
    while (*command) {
        if (*line++ != *command++) return 0;
    }
    return *line == 0;
}

static void mouse_status(void) {
    kmain_puts("Mouse status:\n  Present: ");
    kmain_puts(mouse_present ? "YES\n" : "NO\n");
    kmain_puts("  Position: (");
    kmain_putdec_signed(mouse_x);
    kmain_puts(", ");
    kmain_putdec_signed(mouse_y);
    kmain_puts(")\n  Buttons: ");
    kmain_putdec(mouse_buttons & 1);
    kmain_puts(" ");
    kmain_putdec((mouse_buttons >> 1) & 1);
    kmain_puts(" ");
    kmain_putdec((mouse_buttons >> 2) & 1);
    kmain_puts("\n  Bytes: ");
    kmain_putdec(mouse_bytes);
    kmain_puts("\n");
}

static void akasha_status(void) {
    kmain_puts("AKASHA x64 bridge: READY\n");
    kmain_puts("Seed concepts: 3\n");
    kmain_puts("Hypervector core: staged for native port\n");
}

static void execute_line(char *line) {
    if (line_equals(line, "help")) {
        kmain_puts("Commands: help ver ai stats mouse status clear halt reboot\n");
    } else if (line_equals(line, "ver")) {
        kmain_puts("VECTORD OS x86-64 bridge 2026.1\n");
    } else if (line_equals(line, "ai stats") || line_equals(line, "ai")) {
        akasha_status();
    } else if (line_equals(line, "mouse status") || line_equals(line, "mouse")) {
        mouse_status();
    } else if (line_equals(line, "clear")) {
        kmain_puts("\033[2J\033[H");
    } else if (line_equals(line, "halt")) {
        for (;;) __asm__ volatile ("hlt");
    } else if (line_equals(line, "reboot")) {
        kmain_outb(0x64, 0xFE);
        for (;;) __asm__ volatile ("hlt");
    } else if (line[0]) {
        kmain_puts("Unknown command: ");
        kmain_puts(line);
        kmain_puts("\n");
    }
}

static void process_input(int value) {
    if (value == '\r' || value == '\n') {
        line_buffer[line_length] = 0;
        kmain_putc('\n');
        execute_line(line_buffer);
        line_length = 0;
        kmain_puts("vectord64> ");
        return;
    }
    if (value == 8 || value == 127) {
        if (line_length > 0) {
            line_length--;
            kmain_puts("\b \b");
        }
        return;
    }
    if (value >= 32 && value < 127 && line_length < sizeof(line_buffer) - 1) {
        line_buffer[line_length++] = (char)value;
        kmain_putc((char)value);
    }
}

void kernel64_main(u32 boot_magic, u32 boot_info) {
    serial_init();
    vga_clear();
    kmain_puts("\r\n\r\n=== VECTORD OS 64-bit Mode ===\r\n");
    kmain_puts("\r\n        .-^\"-.\r\n       /  o o  \\\r\n      |    ^    |\r\n       \\  '-'  /\r\n        '-----'\r\n");
    kmain_puts("FREE INFORMATION FOR ALL SEXY GS\r\n");
    kmain_puts("The map is not the territory, but curiosity still redraws it.\r\n");
    kmain_puts("\r\n    .-\"\"\"-.        .-\"\"\"-.\r\n   /  o   o  \\      /  o   o  \\\r\n  |    _    |    |    _    |\r\n   \\  '-'  /      \\  '-'  /\r\n    '-----'        '-----'\r\n");
    kmain_puts("SOSSAGE: SIGNAL LOST // KEEP THE SIGNAL ALIVE\r\n");
    kmain_puts("When the map ends, the signal begins.\r\n");
    kmain_puts("Boot magic: ");
    kmain_puthex(boot_magic);
    kmain_puts("\r\nBoot info:  ");
    kmain_puthex(boot_info);
    kmain_puts("\r\n");
    ps2_configure();
    kmain_puts("PS/2 keyboard: polling ready\r\n");
    kmain_puts("PS/2 mouse: ");
    kmain_puts(mouse_init() == 0 ? "ready\r\n" : "not detected\r\n");
    akasha_status();
    kmain_puts("Type help. ESC is not required in serial mode.\r\n\r\nvectord64> ");
    for (;;) {
        ps2_poll();
        int keyboard = keyboard_getchar();
        if (keyboard >= 0) process_input(keyboard);
        int serial = kmain_getc();
        if (serial >= 0) process_input(serial);
        __asm__ volatile ("pause");
    }
}

