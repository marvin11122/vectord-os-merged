/* ═══════════════════════════════════════════════════════════════════════
 * VECTORD OS - VGA Graphics Mode 13h Driver
 * 320x200 resolution, 256 colors
 * ═══════════════════════════════════════════════════════════════════════ */

#include "../include/vecord.h"

/* VGA registers */
#define VGA_SEQ_INDEX   0x3C4
#define VGA_SEQ_DATA    0x3C5
#define VGA_GC_INDEX    0x3CE
#define VGA_GC_DATA     0x3CF
#define VGA_CRTC_INDEX  0x3D4
#define VGA_CRTC_DATA   0x3D5
#define VGA_MISC_WRITE  0x3C2
#define VGA_AC_INDEX    0x3C0
#define VGA_AC_WRITE    0x3C0
#define VGA_AC_READ     0x3C1
#define VGA_INPUT_STATUS 0x3DA

/* Framebuffer */
#define VGA_FB          0xA0000
#define VGA_WIDTH       320
#define VGA_HEIGHT      200
#define VGA_SIZE        (VGA_WIDTH * VGA_HEIGHT)

/* VGA I/O port access */
static inline void outb_vga(u16 port, u8 val) {
    asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline u8 inb_vga(u16 port) {
    u8 ret;
    asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/* ═══════════════════════════════════════════════════════════════════════
 * SET VGA MODE 13h
 * ═══════════════════════════════════════════════════════════════════════ */
void vga_gfx_set_mode13h(void) {
    serial_puts("VGA: Setting Mode 13h (320x200x256)...\n");
    
    /* Write sequencer registers */
    outb_vga(VGA_SEQ_INDEX, 0x00); outb_vga(VGA_SEQ_DATA, 0x03); /* Reset */
    outb_vga(VGA_SEQ_INDEX, 0x01); outb_vga(VGA_SEQ_DATA, 0x01); /* Clocking mode */
    outb_vga(VGA_SEQ_INDEX, 0x02); outb_vga(VGA_SEQ_DATA, 0x0F); /* Map mask: all planes */
    outb_vga(VGA_SEQ_INDEX, 0x04); outb_vga(VGA_SEQ_DATA, 0x0E); /* Memory mode: chain-4 */
    
    /* Write miscellaneous output */
    outb_vga(VGA_MISC_WRITE, 0x63);
    
    /* Unlock CRTC */
    outb_vga(VGA_CRTC_INDEX, 0x11);
    outb_vga(VGA_CRTC_DATA, inb_vga(VGA_CRTC_DATA) & 0x7F);
    
    /* CRTC timing for 320x200 */
    outb_vga(VGA_CRTC_INDEX, 0x00); outb_vga(VGA_CRTC_DATA, 0x5F); /* Horizontal total */
    outb_vga(VGA_CRTC_INDEX, 0x01); outb_vga(VGA_CRTC_DATA, 0x4F); /* H disp end */
    outb_vga(VGA_CRTC_INDEX, 0x02); outb_vga(VGA_CRTC_DATA, 0x50); /* H blank start */
    outb_vga(VGA_CRTC_INDEX, 0x03); outb_vga(VGA_CRTC_DATA, 0x82); /* H blank end */
    outb_vga(VGA_CRTC_INDEX, 0x04); outb_vga(VGA_CRTC_DATA, 0x54); /* H retrace start */
    outb_vga(VGA_CRTC_INDEX, 0x05); outb_vga(VGA_CRTC_DATA, 0x80); /* H retrace end */
    outb_vga(VGA_CRTC_INDEX, 0x06); outb_vga(VGA_CRTC_DATA, 0xBF); /* Vertical total */
    outb_vga(VGA_CRTC_INDEX, 0x07); outb_vga(VGA_CRTC_DATA, 0x1F); /* Overflow */
    outb_vga(VGA_CRTC_INDEX, 0x09); outb_vga(VGA_CRTC_DATA, 0x41); /* Max scan line */
    outb_vga(VGA_CRTC_INDEX, 0x10); outb_vga(VGA_CRTC_DATA, 0x9C); /* V retrace start */
    outb_vga(VGA_CRTC_INDEX, 0x11); outb_vga(VGA_CRTC_DATA, 0x8E); /* V retrace end */
    outb_vga(VGA_CRTC_INDEX, 0x12); outb_vga(VGA_CRTC_DATA, 0xBF); /* V display end */
    outb_vga(VGA_CRTC_INDEX, 0x13); outb_vga(VGA_CRTC_DATA, 0x20); /* Offset */
    
    /* Graphics controller */
    outb_vga(VGA_GC_INDEX, 0x05); outb_vga(VGA_GC_DATA, 0x40); /* Mode */
    outb_vga(VGA_GC_INDEX, 0x06); outb_vga(VGA_GC_DATA, 0x05); /* Misc: chain-4, 256 color */
    outb_vga(VGA_GC_INDEX, 0x01); outb_vga(VGA_GC_DATA, 0x00); /* Set/reset */
    outb_vga(VGA_GC_INDEX, 0x08); outb_vga(VGA_GC_DATA, 0xFF); /* Bit mask */
    
    /* Enable video */
    inb_vga(VGA_INPUT_STATUS);
    outb_vga(VGA_AC_INDEX, 0x20);
    
    serial_puts("VGA: Mode 13h set successfully\n");
}

/* ═══════════════════════════════════════════════════════════════════════
 * VGA PALETTE - Beautiful 256 color palette
 * ═══════════════════════════════════════════════════════════════════════ */
void vga_gfx_set_palette(void) {
    serial_puts("VGA: Setting palette...\n");
    
    for (int i = 0; i < 256; i++) {
        outb_vga(0x3C8, i);
        
        u8 r, g, b;
        if (i < 16) {
            /* First 16: standard colors */
            static const u8 std_pal[16][3] = {
                {0,0,0}, {0,0,42}, {0,42,0}, {0,42,42},
                {42,0,0}, {42,0,42}, {42,21,0}, {42,42,42},
                {21,21,21}, {21,21,63}, {21,63,21}, {21,63,63},
                {63,21,21}, {63,21,63}, {63,63,21}, {63,63,63}
            };
            r = std_pal[i][0]; g = std_pal[i][1]; b = std_pal[i][2];
        } else if (i < 32) {
            /* Second 16: bright colors */
            r = (i - 16) * 4; g = 0; b = 63;
        } else if (i < 64) {
            /* Blues */
            r = 0; g = (i - 32) * 2; b = 63;
        } else if (i < 96) {
            /* Greens */
            r = 0; g = 63; b = (i - 64) * 2;
        } else if (i < 128) {
            /* Cyans */
            r = (i - 96) * 2; g = 63; b = 63;
        } else if (i < 160) {
            /* Reds */
            r = 63; g = (i - 128) * 2; b = 0;
        } else if (i < 192) {
            /* Yellows */
            r = 63; g = 63; b = (i - 160) * 2;
        } else if (i < 224) {
            /* Magentas */
            r = 63; g = (i - 192) * 2; b = 63;
        } else {
            /* Grays */
            u8 v = (i - 224) * 3;
            r = v; g = v; b = v;
        }
        
        outb_vga(0x3C9, r);
        outb_vga(0x3C9, g);
        outb_vga(0x3C9, b);
    }
    serial_puts("VGA: Palette set\n");
}

/* ═══════════════════════════════════════════════════════════════════════
 * PIXEL OPERATIONS
 * ═══════════════════════════════════════════════════════════════════════ */
void vga_gfx_put_pixel(int x, int y, u8 color) {
    if (x >= 0 && x < VGA_WIDTH && y >= 0 && y < VGA_HEIGHT) {
        u8 *fb = (u8*)VGA_FB;
        fb[y * VGA_WIDTH + x] = color;
    }
}

void vga_gfx_clear(u8 color) {
    u8 *fb = (u8*)VGA_FB;
    for (int i = 0; i < VGA_SIZE; i++) {
        fb[i] = color;
    }
}

/* ═══════════════════════════════════════════════════════════════════════
 * SHAPE DRAWING
 * ═══════════════════════════════════════════════════════════════════════ */
void vga_gfx_fill_rect(int x, int y, int w, int h, u8 color) {
    for (int dy = 0; dy < h; dy++) {
        for (int dx = 0; dx < w; dx++) {
            vga_gfx_put_pixel(x + dx, y + dy, color);
        }
    }
}

void vga_gfx_draw_rect(int x, int y, int w, int h, u8 color) {
    for (int i = 0; i < w; i++) {
        vga_gfx_put_pixel(x + i, y, color);
        vga_gfx_put_pixel(x + i, y + h - 1, color);
    }
    for (int i = 0; i < h; i++) {
        vga_gfx_put_pixel(x, y + i, color);
        vga_gfx_put_pixel(x + w - 1, y + i, color);
    }
}

void vga_gfx_draw_line(int x1, int y1, int x2, int y2, u8 color) {
    int dx = x2 - x1;
    int dy = y2 - y1;
    int sx = (dx > 0) ? 1 : -1;
    int sy = (dy > 0) ? 1 : -1;
    dx = (dx < 0) ? -dx : dx;
    dy = (dy < 0) ? -dy : dy;
    
    int err = dx - dy;
    while (1) {
        vga_gfx_put_pixel(x1, y1, color);
        if (x1 == x2 && y1 == y2) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x1 += sx; }
        if (e2 < dx) { err += dx; y1 += sy; }
    }
}

void vga_gfx_draw_circle(int cx, int cy, int r, u8 color) {
    int x = r, y = 0;
    int err = 0;
    while (x >= y) {
        vga_gfx_put_pixel(cx + x, cy + y, color);
        vga_gfx_put_pixel(cx + y, cy + x, color);
        vga_gfx_put_pixel(cx - y, cy + x, color);
        vga_gfx_put_pixel(cx - x, cy + y, color);
        vga_gfx_put_pixel(cx - x, cy - y, color);
        vga_gfx_put_pixel(cx - y, cy - x, color);
        vga_gfx_put_pixel(cx + y, cy - x, color);
        vga_gfx_put_pixel(cx + x, cy - y, color);
        y++;
        if (err <= 0) err += 2*y + 1;
        if (err > 0) { x--; err -= 2*x + 1; }
    }
}

void vga_gfx_fill_circle(int cx, int cy, int r, u8 color) {
    for (int y = -r; y <= r; y++) {
        for (int x = -r; x <= r; x++) {
            if (x*x + y*y <= r*r) {
                vga_gfx_put_pixel(cx + x, cy + y, color);
            }
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════════
 * 8x8 BITMAP FONT
 * ═══════════════════════════════════════════════════════════════════════ */
static const u8 font_8x8[128][8] = {
    [0x20] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /* space */
    [0x30] = {0x3C,0x66,0x6E,0x76,0x66,0x66,0x3C,0x00}, /* 0 */
    [0x31] = {0x18,0x38,0x18,0x18,0x18,0x18,0x7E,0x00}, /* 1 */
    [0x32] = {0x3C,0x66,0x06,0x1C,0x30,0x66,0x7E,0x00}, /* 2 */
    [0x33] = {0x3C,0x66,0x06,0x1C,0x06,0x66,0x3C,0x00}, /* 3 */
    [0x34] = {0x0E,0x1E,0x36,0x66,0x7F,0x06,0x06,0x00}, /* 4 */
    [0x35] = {0x7E,0x60,0x7C,0x06,0x06,0x66,0x3C,0x00}, /* 5 */
    [0x36] = {0x1C,0x30,0x60,0x7C,0x66,0x66,0x3C,0x00}, /* 6 */
    [0x37] = {0x7E,0x66,0x06,0x0C,0x18,0x18,0x18,0x00}, /* 7 */
    [0x38] = {0x3C,0x66,0x66,0x3C,0x66,0x66,0x3C,0x00}, /* 8 */
    [0x39] = {0x3C,0x66,0x66,0x3E,0x06,0x0C,0x38,0x00}, /* 9 */
    [0x41] = {0x18,0x3C,0x66,0x7E,0x66,0x66,0x66,0x00}, /* A */
    [0x42] = {0x7C,0x66,0x66,0x7C,0x66,0x66,0x7C,0x00}, /* B */
    [0x43] = {0x3C,0x66,0x60,0x60,0x60,0x66,0x3C,0x00}, /* C */
    [0x44] = {0x78,0x6C,0x66,0x66,0x66,0x6C,0x78,0x00}, /* D */
    [0x45] = {0x7E,0x60,0x60,0x7C,0x60,0x60,0x7E,0x00}, /* E */
    [0x46] = {0x7E,0x60,0x60,0x7C,0x60,0x60,0x60,0x00}, /* F */
    [0x47] = {0x3C,0x66,0x60,0x6E,0x66,0x66,0x3E,0x00}, /* G */
    [0x48] = {0x66,0x66,0x66,0x7E,0x66,0x66,0x66,0x00}, /* H */
    [0x49] = {0x7E,0x18,0x18,0x18,0x18,0x18,0x7E,0x00}, /* I */
    [0x4C] = {0x60,0x60,0x60,0x60,0x60,0x66,0x3C,0x00}, /* L */
    [0x4E] = {0x63,0x73,0x7B,0x6F,0x67,0x63,0x63,0x00}, /* N */
    [0x4F] = {0x3C,0x66,0x66,0x66,0x66,0x66,0x3C,0x00}, /* O */
    [0x50] = {0x7C,0x66,0x66,0x7C,0x60,0x60,0x60,0x00}, /* P */
    [0x52] = {0x7C,0x66,0x66,0x7C,0x6C,0x66,0x66,0x00}, /* R */
    [0x53] = {0x3C,0x66,0x60,0x3C,0x06,0x66,0x3C,0x00}, /* S */
    [0x54] = {0x7E,0x18,0x18,0x18,0x18,0x18,0x18,0x00}, /* T */
    [0x55] = {0x66,0x66,0x66,0x66,0x66,0x66,0x3C,0x00}, /* U */
    [0x56] = {0x66,0x66,0x66,0x66,0x66,0x3C,0x18,0x00}, /* V */
    [0x57] = {0x63,0x63,0x63,0x6F,0x7B,0x73,0x63,0x00}, /* W */
    [0x58] = {0x66,0x66,0x3C,0x18,0x3C,0x66,0x66,0x00}, /* X */
    [0x59] = {0x66,0x66,0x66,0x3C,0x18,0x18,0x18,0x00}, /* Y */
    [0x61] = {0x00,0x00,0x3C,0x06,0x3E,0x66,0x3E,0x00}, /* a */
    [0x63] = {0x00,0x60,0x7C,0x66,0x66,0x66,0x7C,0x00}, /* c */
    [0x64] = {0x06,0x06,0x3E,0x66,0x66,0x66,0x3E,0x00}, /* d */
    [0x65] = {0x00,0x00,0x3C,0x66,0x7E,0x60,0x3C,0x00}, /* e */
    [0x66] = {0x0E,0x18,0x18,0x7E,0x18,0x18,0x18,0x00}, /* f */
    [0x67] = {0x00,0x00,0x3E,0x66,0x3E,0x06,0x3C,0x00}, /* g */
    [0x68] = {0x60,0x6C,0x76,0x66,0x66,0x66,0x66,0x00}, /* h */
    [0x69] = {0x18,0x00,0x38,0x18,0x18,0x18,0x3C,0x00}, /* i */
    [0x6B] = {0x60,0x60,0x66,0x6C,0x78,0x6C,0x66,0x00}, /* k */
    [0x6C] = {0x38,0x18,0x18,0x18,0x18,0x18,0x3C,0x00}, /* l */
    [0x6D] = {0x00,0x00,0x66,0x7F,0x7F,0x6B,0x63,0x00}, /* m */
    [0x6E] = {0x00,0x00,0x7C,0x66,0x66,0x66,0x66,0x00}, /* n */
    [0x6F] = {0x00,0x00,0x3C,0x66,0x66,0x66,0x3C,0x00}, /* o */
    [0x70] = {0x00,0x00,0x7C,0x66,0x7C,0x60,0x60,0x00}, /* p */
    [0x72] = {0x00,0x00,0x76,0x6C,0x60,0x60,0x60,0x00}, /* r */
    [0x73] = {0x00,0x00,0x3E,0x60,0x3C,0x06,0x7C,0x00}, /* s */
    [0x74] = {0x18,0x18,0x7E,0x18,0x18,0x18,0x0E,0x00}, /* t */
    [0x75] = {0x00,0x00,0x66,0x66,0x66,0x66,0x3E,0x00}, /* u */
    [0x76] = {0x00,0x00,0x66,0x66,0x66,0x3C,0x18,0x00}, /* v */
    [0x77] = {0x00,0x00,0x63,0x6B,0x7F,0x7F,0x36,0x00}, /* w */
    [0x78] = {0x00,0x00,0x66,0x3C,0x18,0x3C,0x66,0x00}, /* x */
    [0x79] = {0x00,0x00,0x66,0x66,0x3E,0x06,0x3C,0x00}, /* y */
    [0x7A] = {0x00,0x00,0x7E,0x0C,0x18,0x30,0x7E,0x00}, /* z */
};

void vga_gfx_draw_char(int x, int y, char c, u8 color) {
    if (c < 0 || c > 127) return;
    const u8 *glyph = font_8x8[(int)c];
    for (int row = 0; row < 8; row++) {
        u8 bits = glyph[row];
        for (int col = 0; col < 8; col++) {
            if (bits & (0x80 >> col)) {
                vga_gfx_put_pixel(x + col, y + row, color);
            }
        }
    }
}

void vga_gfx_draw_string(int x, int y, const char *str, u8 color) {
    int ox = x;
    while (*str) {
        if (*str == '\n') {
            x = ox;
            y += 8;
        } else {
            vga_gfx_draw_char(x, y, *str, color);
            x += 8;
        }
        str++;
    }
}

/* ═══════════════════════════════════════════════════════════════════════
 * DEMO - Draw a beautiful desktop (simplified, no division)
 * ═══════════════════════════════════════════════════════════════════════ */
void vga_gfx_demo_desktop(void) {
    /* Dark blue background */
    vga_gfx_clear(0x01);
    
    /* Draw title bar */
    vga_gfx_fill_rect(0, 0, 320, 14, 0x09);  /* Blue */
    
    /* Draw taskbar */
    vga_gfx_fill_rect(0, 186, 320, 14, 0x07);  /* Gray */
    
    /* Draw window box */
    vga_gfx_fill_rect(30, 20, 260, 160, 0x0F);  /* White */
    vga_gfx_draw_rect(30, 20, 260, 160, 0x01);  /* Border */
    
    /* Title bar of window */
    vga_gfx_fill_rect(30, 20, 260, 14, 0x09);  /* Blue */
    
    /* Draw text */
    vga_gfx_draw_string(6, 2, "VECTORD OS v3.0 NEURAL VECTOR", 0x3F);
    vga_gfx_draw_string(40, 36, "Welcome to VECTORD OS!", 0x01);
    vga_gfx_draw_string(40, 50, "Vector Neural Operating System", 0x02);
    vga_gfx_draw_string(40, 64, "Better than Windows and macOS", 0x04);
    vga_gfx_draw_string(40, 78, "AI integrated at kernel level", 0x01);
    vga_gfx_draw_string(40, 92, "4D vector memory addressing", 0x02);
    vga_gfx_draw_string(40, 106, "Neural hardware optimizer", 0x04);
    vga_gfx_draw_string(40, 120, "Real-time scheduling", 0x01);
    
    /* Start button */
    vga_gfx_fill_rect(2, 188, 50, 10, 0x3F);
    vga_gfx_draw_string(4, 189, "Start", 0x01);
    
    /* Taskbar icons */
    vga_gfx_fill_rect(60, 188, 80, 10, 0x07);
    vga_gfx_draw_string(64, 189, "Terminal", 0x01);
    vga_gfx_fill_rect(150, 188, 60, 10, 0x07);
    vga_gfx_draw_string(154, 189, "Files", 0x01);
    
    /* Draw some decorative rectangles instead of circles */
    vga_gfx_fill_rect(270, 80, 30, 30, 0x0C);  /* Red square */
    vga_gfx_fill_rect(270, 130, 20, 20, 0x0A);  /* Green square */
    vga_gfx_draw_line(30, 140, 200, 140, 0x04); /* Red line */
}

/* ═══════════════════════════════════════════════════════════════════════
 * DEMO ANIMATION - Bouncing square
 * ═══════════════════════════════════════════════════════════════════════ */
void vga_gfx_demo_animation(void) {
    /* Simple bouncing square demo */
    vga_gfx_clear(0x01);  /* Dark blue */
    
    int bx = 160, by = 100;
    int dx = 3, dy = 2;
    int size = 20;
    
    for (int frame = 0; frame < 300; frame++) {
        /* Clear old position */
        vga_gfx_fill_rect(bx - size/2, by - size/2, size, size, 0x01);
        
        /* Move */
        bx += dx;
        by += dy;
        
        /* Bounce */
        if (bx <= size/2 || bx >= 319 - size/2) dx = -dx;
        if (by <= size/2 || by >= 199 - size/2) dy = -dy;
        
        /* Draw with color cycling */
        u8 color = 0x20 + (frame % 32);
        vga_gfx_fill_rect(bx - size/2, by - size/2, size, size, color);
        vga_gfx_draw_rect(bx - size/2, by - size/2, size, size, 0x3F);
        
        /* Draw info */
        vga_gfx_draw_string(5, 5, "VECTORD OS Graphics Demo", 0x3F);
        
        /* Draw border frame */
        vga_gfx_draw_rect(0, 0, 320, 200, 0x3F);
        
        /* Small delay */
        for (volatile int d = 0; d < 10000; d++);
    }
}

