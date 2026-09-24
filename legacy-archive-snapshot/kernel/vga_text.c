/* ═══════════════════════════════════════════════════════════════════════
 * VECTORD OS - VGA Text Mode Boot Screen
 * ═══════════════════════════════════════════════════════════════════════ */

#include "../include/vecord.h"

#define VGA_TXT ((volatile u16 *)0xB8000)

static void vga_putc(int x, int y, char c, u8 color) {
    if (x >= 0 && x < 80 && y >= 0 && y < 25)
        VGA_TXT[y * 80 + x] = (u16)c | ((u16)color << 8);
}

static void vga_str(int x, int y, const char *s, u8 color) {
    while (*s && x < 80) vga_putc(x++, y, *s++, color);
}

static void vga_cls(void) {
    for (int i = 0; i < 80 * 25; i++)
        VGA_TXT[i] = (u16)' ' | ((u16)0x07 << 8);
}

static void vga_bar(int y, u8 color) {
    for (int x = 0; x < 80; x++) vga_putc(x, y, ' ', color);
}

static void u32_to_str(u32 val, char *buf) {
    if (val == 0) { buf[0] = '0'; buf[1] = 0; return; }
    char t[12]; int i = 0;
    while (val) { t[i++] = '0' + (val % 10); val /= 10; }
    int j = 0; while (i) buf[j++] = t[--i]; buf[j] = 0;
}

static void u32_to_hex(u32 val, char *buf) {
    buf[0] = '0'; buf[1] = 'x';
    for (int j = 0; j < 8; j++) {
        u8 n = (val >> (28 - j * 4)) & 0xF;
        buf[2 + j] = n < 10 ? '0' + n : 'A' + n - 10;
    }
    buf[10] = 0;
}

void vga_show_status(void) {
    /* 0=black 1=blue 2=green 3=cyan 4=red 7=lgray 8=dgray 9=lblue
     * 10=lgreen 11=lcyan 12=lred 14=yellow 15=white */
    vga_cls();
    
    /* Blue header */
    vga_bar(0, 0x1F);
    vga_str(2, 0, "VECTORD OS v2.0.1 - Neural Vector Kernel", 0x1F);
    
    int r = 2;
    
    vga_putc(0, r, '[', 0x07); vga_str(1, r, "OK", 0x0A);
    vga_str(3, r, "] Kernel booted successfully", 0x07); r++;
    
    vga_putc(0, r, '[', 0x07); vga_str(1, r, "OK", 0x0A);
    vga_str(3, r, "] CPU: SSE2 enabled", 0x07); r++;
    
    vga_putc(0, r, '[', 0x07); vga_str(1, r, "OK", 0x0A);
    vga_str(3, r, "] PS/2 keyboard + mouse", 0x07); r++;
    
    /* VBE */
    vga_putc(0, r, '[', 0x07);
    if (vbe_info.present) {
        vga_str(1, r, "OK", 0x0A);
        vga_str(3, r, "] VBE: ", 0x07);
        char buf[12];
        u32_to_str(vbe_info.width, buf);
        vga_str(10, r, buf, 0x0B);
        vga_str(10 + 4, r, "x", 0x0B);
        u32_to_str(vbe_info.height, buf);
        vga_str(10 + 5, r, buf, 0x0B);
        vga_str(10 + 10, r, "@32bpp", 0x0B);
    } else {
        vga_str(1, r, "!!", 0x0C);
        vga_str(3, r, "] VBE: DETECTING...", 0x0C);
    }
    r++;
    
    /* FB address */
    vga_str(0, r, "      FB: ", 0x07);
    char hex[12];
    u32_to_hex(vbe_info.base, hex);
    vga_str(10, r, hex, 0x0B); r++;
    r++;
    
    /* Color bars preview */
    vga_str(0, r, "Color bar test preview:", 0x0E); r++;
    u8 cols[] = {0, 4, 2, 6, 1, 5, 3, 7};
    for (int x = 0; x < 40; x++)
        vga_putc(x, r, ' ', (cols[(x / 5) % 8] << 4) | 15);
    r += 2;
    
    vga_str(0, r++, "Type 'vbetest' then check display", 0x0F);
    vga_str(0, r++, "Type 'vbemode' for detailed info", 0x07);
    vga_str(0, r++, "Type 'gui' for graphical desktop", 0x07);
    vga_str(0, r++, "Serial console works in background", 0x08);
    
    /* Show cursor */
    u16 pos = r * 80;
    outb(0x3D4, 0x0F); outb(0x3D5, pos & 0xFF);
    outb(0x3D4, 0x0E); outb(0x3D5, (pos >> 8) & 0xFF);
}
