/* VGA Screenshot - dump framebuffer to serial as PPM image */
#include "../include/vecord.h"

#define VGA_FB      0xA0000
#define VGA_WIDTH   320
#define VGA_HEIGHT  200

/* Simple base64 encoding */
static const char b64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static void serial_puts_raw(const char *s) {
    while (*s) {
        while (!(inb(0x3F8 + 5) & 0x20));
        outb(0x3F8, *s++);
    }
}

void vga_screenshot_ppm(void) {
    u8 *fb = (u8*)VGA_FB;
    
    /* Send PPM header via serial */
    serial_puts_raw("P6\n320 200\n255\n");
    
    /* Read palette and send pixel data */
    for (int y = 0; y < VGA_HEIGHT; y++) {
        for (int x = 0; x < VGA_WIDTH; x++) {
            u8 color = fb[y * VGA_WIDTH + x];
            
            /* Read palette color */
            outb(0x3C7, color);
            u8 r = inb(0x3C9) << 2;  /* VGA uses 6-bit color (0-63) */
            u8 g = inb(0x3C9) << 2;
            u8 b = inb(0x3C9) << 2;
            
            /* Send RGB */
            while (!(inb(0x3F8 + 5) & 0x20));
            outb(0x3F8, r);
            while (!(inb(0x3F8 + 5) & 0x20));
            outb(0x3F8, g);
            while (!(inb(0x3F8 + 5) & 0x20));
            outb(0x3F8, b);
        }
    }
    
    serial_puts_raw("\n");
    serial_puts_raw("Screenshot complete\n");
}

/* Simple ASCII art representation of framebuffer */
void vga_screenshot_ascii(void) {
    u8 *fb = (u8*)VGA_FB;
    
    serial_puts_raw("\n=== VGA Screenshot (ASCII) ===\n");
    
    for (int y = 0; y < 50; y++) {  /* Show half the screen */
        for (int x = 0; x < 80; x++) {  /* 320/4 = 80 chars wide */
            u8 color = fb[(y * 4) * VGA_WIDTH + (x * 4)];
            char c;
            if (color == 0x01) c = '.';
            else if (color == 0x0F) c = '#';
            else if (color == 0x09) c = '=';
            else if (color == 0x07) c = '-';
            else if (color == 0x0C) c = 'O';
            else if (color == 0x0A) c = 'X';
            else c = '@';
            while (!(inb(0x3F8 + 5) & 0x20));
            outb(0x3F8, c);
        }
        serial_puts_raw("\n");
    }
    serial_puts_raw("=== End Screenshot ===\n");
}
