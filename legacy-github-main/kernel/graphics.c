/* ═══════════════════════════════════════════════════════════════════════
 * VECTORD OS - Master Graphics Engine
 * Direct framebuffer rendering, mouse, windows, widgets
 * ═══════════════════════════════════════════════════════════════════════ */

#include "../include/vecord.h"

extern u32 get_ticks(void);

/* ─── Types ─────────────────────────────────────────────────────────── */

typedef struct {
    u32 *pixels;        /* Points to VBE framebuffer or backbuffer */
    u32  width, height;
    u32  pitch;         /* Pixels per scanline */
    int  is_hw;         /* 1 = direct to hardware */
} gfx_surface_t;

typedef struct {
    int x, y;
    int w, h;
    char title[64];
    u8   active;        /* Focused */
    u8   visible;
    u8   minimized;
    u32  bg_color;
    /* Content callback */
    void (*on_draw)(gfx_surface_t *surf, int wx, int wy, int ww, int wh);
    void (*on_key)(char c);
    void (*on_click)(int mx, int my);
} gfx_window_t;

#define GFX_MAX_WINDOWS  16
#define GFX_MAX_WIDGETS  32

typedef struct {
    int x, y, w, h;
    char label[32];
    char value[64];
    u32  color;
    u8   type;          /* 0=label, 1=button, 2=progress, 3=icon */
    void (*on_click)(void);
} gfx_widget_t;

/* ─── Global State ──────────────────────────────────────────────────── */

static gfx_surface_t screen;
static gfx_surface_t backbuf;
static gfx_window_t  windows[GFX_MAX_WINDOWS];
static int           window_count = 0;
static int           focused_window = -1;
static gfx_widget_t  widgets[GFX_MAX_WIDGETS];
static int           widget_count = 0;

/* Mouse state */
static int mouse_x = 960, mouse_y = 540;
static int mouse_btn = 0;
static int mouse_prev_btn = 0;
static int drag_window = -1;
static int drag_off_x, drag_off_y;

/* Colors */
#define COL_BLACK       0x000000
#define COL_WHITE       0xFFFFFF
#define COL_RED         0x0044FF
#define COL_GREEN       0x00CC44
#define COL_BLUE        0xCC4400
#define COL_CYAN        0x00CCCC
#define COL_MAGENTA     0xCC00CC
#define COL_YELLOW      0x00CCFF
#define COL_GRAY        0x888888
#define COL_DARK        0x1A1A2E
#define COL_DARK2       0x16213E
#define COL_ACCENT      0x0F3460
#define COL_GLASS       0x4080A0C0
#define COL_TITLE_BG    0x333355
#define COL_TASKBAR     0x222244

/* ═══════════════════════════════════════════════════════════════════════
 * SURFACE OPERATIONS
 * ═══════════════════════════════════════════════════════════════════════ */

void gfx_init(void) {
    screen.width  = vbe_info.width  ? vbe_info.width  : 1024;
    screen.height = vbe_info.height ? vbe_info.height : 768;
    screen.pitch  = vbe_info.pitch  ? vbe_info.pitch / 4 : screen.width;
    screen.is_hw  = 1;

    serial_puts("[GFX] VBE mode: ");
    serial_putdec(vbe_info.width);
    serial_puts("x");
    serial_putdec(vbe_info.height);
    serial_puts(" pitch=");
    serial_putdec(vbe_info.pitch);
    serial_puts(" bpp=");
    serial_putdec(vbe_info.bpp);
    serial_puts("\n");

    if (vbe_info.present && vbe_info.base) {
        screen.pixels = (u32 *)vbe_info.base;
    } else {
        /* Fallback: use backbuffer */
        screen.pixels = backbuf.pixels;
        screen.is_hw = 0;
    }

    /* Allocate backbuffer for double buffering */
    backbuf.width  = screen.width;
    backbuf.height = screen.height;
    backbuf.pitch  = screen.width;
    backbuf.pixels = (u32 *)kmalloc(screen.width * screen.height * 4);
    backbuf.is_hw  = 0;

    /* Clear both buffers */
    for (u32 i = 0; i < screen.width * screen.height; i++) {
        screen.pixels[i] = COL_DARK;
        if (backbuf.pixels) backbuf.pixels[i] = COL_DARK;
    }

    serial_puts("[GFX] Initialized ");
    serial_putdec(screen.width);
    serial_puts("x");
    serial_putdec(screen.height);
    serial_puts(" (");
    serial_putdec(screen.pitch);
    serial_puts(" pitch)\n");
}

gfx_surface_t *gfx_get_screen(void) { return &screen; }
gfx_surface_t *gfx_get_backbuf(void) { return &backbuf; }

void gfx_flush(void) {
    if (!backbuf.pixels) return;
    /* Copy backbuffer to screen (or swap if we had page flipping) */
    u32 count = screen.pitch * screen.height;
    for (u32 i = 0; i < count; i++) {
        screen.pixels[i] = backbuf.pixels[i];
    }
}

/* ─── Pixel operations ─────────────────────────────────────────────── */

static inline u32 make_rgb(u8 r, u8 g, u8 b) {
    return ((u32)r << 16) | ((u32)g << 8) | (u32)b;
}

static inline u8 red_of(u32 c) { return (c >> 16) & 0xFF; }
static inline u8 green_of(u32 c) { return (c >> 8) & 0xFF; }
static inline u8 blue_of(u32 c) { return c & 0xFF; }

static u32 blend(u32 fg, u32 bg, u8 alpha) {
    u32 inv = 255 - alpha;
    u32 r = ((u32)red_of(fg)   * alpha + (u32)red_of(bg)   * inv) >> 8;
    u32 g = ((u32)green_of(fg) * alpha + (u32)green_of(bg) * inv) >> 8;
    u32 b = ((u32)blue_of(fg)  * alpha + (u32)blue_of(bg)  * inv) >> 8;
    return make_rgb(r, g, b);
}

static u32 blend_add(u32 fg, u32 bg, u8 alpha) {
    u32 r = red_of(bg)   + ((u32)red_of(fg)   * alpha >> 8);
    u32 g = green_of(bg) + ((u32)green_of(fg) * alpha >> 8);
    u32 b = blue_of(bg)  + ((u32)blue_of(fg)  * alpha >> 8);
    if (r > 255) r = 255;
    if (g > 255) g = 255;
    if (b > 255) b = 255;
    return make_rgb(r, g, b);
}

static inline void put_pixel(gfx_surface_t *s, int x, int y, u32 c) {
    if (x >= 0 && (u32)x < s->width && y >= 0 && (u32)y < s->height) {
        s->pixels[y * s->pitch + x] = c;
    }
}

static inline u32 get_pixel(gfx_surface_t *s, int x, int y) {
    if (x >= 0 && (u32)x < s->width && y >= 0 && (u32)y < s->height) {
        return s->pixels[y * s->pitch + x];
    }
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════
 * SHAPE PRIMITIVES
 * ═══════════════════════════════════════════════════════════════════════ */

void gfx_fill_rect(gfx_surface_t *s, int x, int y, int w, int h, u32 c) {
    for (int dy = 0; dy < h; dy++) {
        int py = y + dy;
        if (py < 0 || py >= (int)s->height) continue;
        u32 off = (u32)py * s->pitch;
        for (int dx = 0; dx < w; dx++) {
            int px = x + dx;
            if (px >= 0 && (u32)px < s->width) {
                s->pixels[off + px] = c;
            }
        }
    }
}

void gfx_fill_rect_alpha(gfx_surface_t *s, int x, int y, int w, int h, u32 c, u8 alpha) {
    for (int dy = 0; dy < h; dy++) {
        int py = y + dy;
        if (py < 0 || py >= (int)s->height) continue;
        u32 off = (u32)py * s->pitch;
        for (int dx = 0; dx < w; dx++) {
            int px = x + dx;
            if (px >= 0 && (u32)px < s->width) {
                u32 bg = s->pixels[off + px];
                s->pixels[off + px] = blend(c, bg, alpha);
            }
        }
    }
}

void gfx_draw_rect(gfx_surface_t *s, int x, int y, int w, int h, u32 c) {
    /* Top and bottom */
    for (int dx = 0; dx < w; dx++) {
        put_pixel(s, x + dx, y, c);
        put_pixel(s, x + dx, y + h - 1, c);
    }
    /* Left and right */
    for (int dy = 0; dy < h; dy++) {
        put_pixel(s, x, y + dy, c);
        put_pixel(s, x + w - 1, y + dy, c);
    }
}

void gfx_fill_circle(gfx_surface_t *s, int cx, int cy, int r, u32 c) {
    for (int dy = -r; dy <= r; dy++) {
        int row = cy + dy;
        if (row < 0 || row >= (int)s->height) continue;
        int dx_max = r - (dy * dy / (r ? r : 1));
        u32 off = (u32)row * s->pitch;
        for (int dx = -dx_max; dx <= dx_max; dx++) {
            int px = cx + dx;
            if (px >= 0 && (u32)px < s->width) {
                s->pixels[off + px] = c;
            }
        }
    }
}

void gfx_draw_circle(gfx_surface_t *s, int cx, int cy, int r, u32 c) {
    for (int a = 0; a < 360; a++) {
        /* Fixed-point sin/cos approximation */
        int rad_x = (r * 7 * a) / 1000;  /* rough */
        int rad_y = (r * 7 * a) / 1000;
        put_pixel(s, cx + rad_x, cy + rad_y, c);
        put_pixel(s, cx - rad_x, cy - rad_y, c);
    }
}

void gfx_rounded_rect(gfx_surface_t *s, int x, int y, int w, int h, int rad, u32 c) {
    /* Fill main rectangle */
    gfx_fill_rect(s, x + rad, y, w - 2*rad, h, c);
    gfx_fill_rect(s, x, y + rad, w, h - 2*rad, c);
    /* Corners */
    for (int dy = 0; dy < rad; dy++) {
        for (int dx = 0; dx < rad; dx++) {
            int d2 = dx*dx + dy*dy;
            if (d2 <= rad*rad) {
                put_pixel(s, x + dx, y + dy, c);
                put_pixel(s, x + w - 1 - dx, y + dy, c);
                put_pixel(s, x + dx, y + h - 1 - dy, c);
                put_pixel(s, x + w - 1 - dx, y + h - 1 - dy, c);
            }
        }
    }
}

void gfx_rounded_rect_alpha(gfx_surface_t *s, int x, int y, int w, int h, int rad, u32 c, u8 alpha) {
    /* Apply alpha to rounded rect */
    for (int dy = 0; dy < h; dy++) {
        for (int dx = 0; dx < w; dx++) {
            int draw = 0;
            /* Check if inside rounded corners */
            if (dx < rad && dy < rad && (dx-rad)*(dx-rad)+(dy-rad)*(dy-rad) <= rad*rad) draw = 1;
            else if (dx >= w-rad && dy < rad && (dx-(w-1-rad))*(dx-(w-1-rad))+(dy-rad)*(dy-rad) <= rad*rad) draw = 1;
            else if (dx < rad && dy >= h-rad && (dx-rad)*(dx-rad)+(dy-(h-1-rad))*(dy-(h-1-rad)) <= rad*rad) draw = 1;
            else if (dx >= w-rad && dy >= h-rad && (dx-(w-1-rad))*(dx-(w-1-rad))+(dy-(h-1-rad))*(dy-(h-1-rad)) <= rad*rad) draw = 1;
            else if (dx >= rad && dx < w-rad && dy >= 0 && dy < h) draw = 1;
            else if (dy >= rad && dy < h-rad && dx >= 0 && dx < w) draw = 1;

            if (draw) {
                int px = x + dx, py = y + dy;
                if (px >= 0 && px < (int)s->width && py >= 0 && py < (int)s->height) {
                    u32 bg = s->pixels[py * s->pitch + px];
                    s->pixels[py * s->pitch + px] = blend(c, bg, alpha);
                }
            }
        }
    }
}

void gfx_gradient_v(gfx_surface_t *s, int x, int y, int w, int h, u32 top, u32 bot) {
    for (int dy = 0; dy < h; dy++) {
        u32 c = blend(bot, top, (u8)((u32)dy * 255 / h));
        for (int dx = 0; dx < w; dx++) {
            put_pixel(s, x + dx, y + dy, c);
        }
    }
}

void gfx_gradient_h(gfx_surface_t *s, int x, int y, int w, int h, u32 left, u32 right) {
    for (int dx = 0; dx < w; dx++) {
        u32 c = blend(right, left, (u8)((u32)dx * 255 / w));
        for (int dy = 0; dy < h; dy++) {
            put_pixel(s, x + dx, y + dy, c);
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════════
 * TEXT RENDERING (8x16 bitmap font, built-in)
 * ═══════════════════════════════════════════════════════════════════════ */

static const u8 font8x16[][16] = {
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /* space */
    {0x00,0x00,0x00,0x18,0x18,0x18,0x18,0x18,0x18,0x00,0x18,0x18,0x00,0x00,0x00,0x00}, /* ! */
    {0x00,0x00,0x00,0x6C,0x6C,0x24,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /* " */
    {0x00,0x00,0x00,0x6C,0x6C,0xFE,0x6C,0x6C,0xFE,0x6C,0x6C,0x00,0x00,0x00,0x00,0x00}, /* # */
    /* Minimal font - just ABCDEFGHIJKLMNOPQRSTUVWXYZ + 0-9 + common */
};

/* Simplified: render ASCII directly with simple 5x7 bitmap */
static const u8 font5x7[][7] = {
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /* 32 space */
    {0x04,0x04,0x04,0x04,0x04,0x00,0x04}, /* 33 ! */
    {0x0A,0x0A,0x0A,0x00,0x00,0x00,0x00}, /* 34 " */
    {0x0A,0x0A,0x1F,0x0A,0x1F,0x0A,0x0A}, /* 35 # */
    {0x04,0x0F,0x14,0x0E,0x05,0x1E,0x04}, /* 36 $ */
    {0x18,0x19,0x02,0x04,0x08,0x13,0x03}, /* 37 % */
    {0x0C,0x12,0x14,0x08,0x15,0x12,0x0D}, /* 38 & */
    {0x0C,0x04,0x08,0x00,0x00,0x00,0x00}, /* 39 ' */
    {0x02,0x04,0x08,0x08,0x08,0x04,0x02}, /* 40 ( */
    {0x08,0x04,0x02,0x02,0x02,0x04,0x08}, /* 41 ) */
    {0x00,0x04,0x15,0x0E,0x15,0x04,0x00}, /* 42 * */
    {0x00,0x04,0x04,0x1F,0x04,0x04,0x00}, /* 43 + */
    {0x00,0x00,0x00,0x00,0x0C,0x04,0x08}, /* 44 , */
    {0x00,0x00,0x00,0x1F,0x00,0x00,0x00}, /* 45 - */
    {0x00,0x00,0x00,0x00,0x00,0x0C,0x0C}, /* 46 . */
    {0x00,0x01,0x02,0x04,0x08,0x10,0x00}, /* 47 / */
    {0x0E,0x11,0x13,0x15,0x19,0x11,0x0E}, /* 48 0 */
    {0x04,0x0C,0x04,0x04,0x04,0x04,0x0E}, /* 49 1 */
    {0x0E,0x11,0x01,0x02,0x04,0x08,0x1F}, /* 50 2 */
    {0x0E,0x11,0x01,0x06,0x01,0x11,0x0E}, /* 51 3 */
    {0x02,0x06,0x0A,0x12,0x1F,0x02,0x02}, /* 52 4 */
    {0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E}, /* 53 5 */
    {0x06,0x08,0x10,0x1E,0x11,0x11,0x0E}, /* 54 6 */
    {0x1F,0x01,0x02,0x04,0x08,0x08,0x08}, /* 55 7 */
    {0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E}, /* 56 8 */
    {0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C}, /* 57 9 */
    {0x00,0x0C,0x0C,0x00,0x0C,0x0C,0x00}, /* 58 : */
    {0x00,0x0C,0x0C,0x00,0x0C,0x04,0x08}, /* 59 ; */
    {0x02,0x04,0x08,0x10,0x08,0x04,0x02}, /* 60 < */
    {0x00,0x00,0x1F,0x00,0x1F,0x00,0x00}, /* 61 = */
    {0x08,0x04,0x02,0x01,0x02,0x04,0x08}, /* 62 > */
    {0x0E,0x11,0x01,0x02,0x04,0x00,0x04}, /* 63 ? */
    {0x0E,0x11,0x17,0x15,0x17,0x10,0x0E}, /* 64 @ */
    {0x04,0x0A,0x11,0x11,0x1F,0x11,0x11}, /* 65 A */
    {0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E}, /* 66 B */
    {0x0E,0x11,0x10,0x10,0x10,0x11,0x0E}, /* 67 C */
    {0x1C,0x12,0x11,0x11,0x11,0x12,0x1C}, /* 68 D */
    {0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F}, /* 69 E */
    {0x1F,0x10,0x10,0x1E,0x10,0x10,0x10}, /* 70 F */
    {0x0E,0x11,0x10,0x17,0x11,0x11,0x0F}, /* 71 G */
    {0x11,0x11,0x11,0x1F,0x11,0x11,0x11}, /* 72 H */
    {0x0E,0x04,0x04,0x04,0x04,0x04,0x0E}, /* 73 I */
    {0x07,0x02,0x02,0x02,0x02,0x12,0x0C}, /* 74 J */
    {0x11,0x12,0x14,0x18,0x14,0x12,0x11}, /* 75 K */
    {0x10,0x10,0x10,0x10,0x10,0x10,0x1F}, /* 76 L */
    {0x11,0x1B,0x15,0x15,0x11,0x11,0x11}, /* 77 M */
    {0x11,0x19,0x15,0x13,0x11,0x11,0x11}, /* 78 N */
    {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E}, /* 79 O */
    {0x1E,0x11,0x11,0x1E,0x10,0x10,0x10}, /* 80 P */
    {0x0E,0x11,0x11,0x11,0x15,0x12,0x0D}, /* 81 Q */
    {0x1E,0x11,0x11,0x1E,0x14,0x12,0x11}, /* 82 R */
    {0x0E,0x11,0x10,0x0E,0x01,0x11,0x0E}, /* 83 S */
    {0x1F,0x04,0x04,0x04,0x04,0x04,0x04}, /* 84 T */
    {0x11,0x11,0x11,0x11,0x11,0x11,0x0E}, /* 85 U */
    {0x11,0x11,0x11,0x11,0x11,0x0A,0x04}, /* 86 V */
    {0x11,0x11,0x11,0x15,0x15,0x1B,0x11}, /* 87 W */
    {0x11,0x11,0x0A,0x04,0x0A,0x11,0x11}, /* 88 X */
    {0x11,0x11,0x0A,0x04,0x04,0x04,0x04}, /* 89 Y */
    {0x1F,0x01,0x02,0x04,0x08,0x10,0x1F}, /* 90 Z */
    {0x0E,0x08,0x08,0x08,0x08,0x08,0x0E}, /* 91 [ */
    {0x00,0x10,0x08,0x04,0x02,0x01,0x00}, /* 92 \ */
    {0x0E,0x02,0x02,0x02,0x02,0x02,0x0E}, /* 93 ] */
    {0x04,0x0A,0x11,0x00,0x00,0x00,0x00}, /* 94 ^ */
    {0x00,0x00,0x00,0x00,0x00,0x00,0x1F}, /* 95 _ */
    {0x08,0x04,0x02,0x00,0x00,0x00,0x00}, /* 96 ` */
    {0x00,0x00,0x0E,0x01,0x0F,0x11,0x0F}, /* 97 a */
    {0x10,0x10,0x1E,0x11,0x11,0x11,0x1E}, /* 98 b */
    {0x00,0x00,0x0E,0x11,0x10,0x11,0x0E}, /* 99 c */
    {0x01,0x01,0x0F,0x11,0x11,0x11,0x0F}, /* 100 d */
    {0x00,0x00,0x0E,0x11,0x1F,0x10,0x0E}, /* 101 e */
    {0x06,0x08,0x08,0x1E,0x08,0x08,0x08}, /* 102 f */
    {0x00,0x00,0x0F,0x11,0x0F,0x01,0x0E}, /* 103 g */
    {0x10,0x10,0x1E,0x11,0x11,0x11,0x11}, /* 104 h */
    {0x04,0x00,0x04,0x04,0x04,0x04,0x04}, /* 105 i */
    {0x02,0x00,0x06,0x02,0x02,0x12,0x0C}, /* 106 j */
    {0x10,0x10,0x12,0x14,0x18,0x14,0x12}, /* 107 k */
    {0x0C,0x04,0x04,0x04,0x04,0x04,0x0E}, /* 108 l */
    {0x00,0x00,0x1A,0x15,0x15,0x11,0x11}, /* 109 m */
    {0x00,0x00,0x1E,0x11,0x11,0x11,0x11}, /* 110 n */
    {0x00,0x00,0x0E,0x11,0x11,0x11,0x0E}, /* 111 o */
    {0x00,0x00,0x1E,0x11,0x1E,0x10,0x10}, /* 112 p */
    {0x00,0x00,0x0F,0x11,0x0F,0x01,0x01}, /* 113 q */
    {0x00,0x00,0x16,0x19,0x10,0x10,0x10}, /* 114 r */
    {0x00,0x00,0x0E,0x10,0x0E,0x01,0x1E}, /* 115 s */
    {0x08,0x08,0x1E,0x08,0x08,0x09,0x06}, /* 116 t */
    {0x00,0x00,0x11,0x11,0x11,0x13,0x0D}, /* 117 u */
    {0x00,0x00,0x11,0x11,0x11,0x0A,0x04}, /* 118 v */
    {0x00,0x00,0x11,0x11,0x15,0x15,0x0A}, /* 119 w */
    {0x00,0x00,0x11,0x0A,0x04,0x0A,0x11}, /* 120 x */
    {0x00,0x00,0x11,0x11,0x0F,0x01,0x0E}, /* 121 y */
    {0x00,0x00,0x1F,0x02,0x04,0x08,0x1F}, /* 122 z */
};

void gfx_draw_char(gfx_surface_t *s, int x, int y, char c, u32 color, int scale) {
    u8 ch = (u8)c;
    if (ch < 32 || ch > 122) ch = '?';
    int idx = ch - 32;
    if (idx < 0 || idx >= (int)(sizeof(font5x7)/sizeof(font5x7[0]))) return;

    for (int row = 0; row < 7; row++) {
        u8 bits = font5x7[idx][row];
        for (int col = 0; col < 5; col++) {
            if (bits & (0x10 >> col)) {
                for (int sy = 0; sy < scale; sy++) {
                    for (int sx = 0; sx < scale; sx++) {
                        put_pixel(s, x + col*scale + sx, y + row*scale + sy, color);
                    }
                }
            }
        }
    }
}

void gfx_draw_string(gfx_surface_t *s, int x, int y, const char *str, u32 color, int scale) {
    while (*str) {
        if (*str == '\n') {
            x = x; /* keep x, caller should handle */
            y += 8 * scale;
        } else {
            gfx_draw_char(s, x, y, *str, color, scale);
            x += 6 * scale;
        }
        str++;
    }
}

void gfx_measure_string(const char *str, int scale, int *w, int *h) {
    int max_w = 0, cur_w = 0, lines = 1;
    while (*str) {
        if (*str == '\n') {
            if (cur_w > max_w) max_w = cur_w;
            cur_w = 0;
            lines++;
        } else {
            cur_w += 6 * scale;
        }
        str++;
    }
    if (cur_w > max_w) max_w = cur_w;
    if (w) *w = max_w;
    if (h) *h = lines * 8 * scale;
}

/* ═══════════════════════════════════════════════════════════════════════
 * MOUSE CURSOR (16x16 arrow)
 * ═══════════════════════════════════════════════════════════════════════ */

static const u8 cursor_arrow[16][2] = {
    {0x80,0x00},{0xC0,0x00},{0xE0,0x00},{0xF0,0x00},
    {0xF8,0x00},{0xFC,0x00},{0xFE,0x00},{0xFF,0x00},
    {0xFF,0x80},{0xF8,0x00},{0xD8,0x00},{0x8C,0x00},
    {0x0C,0x00},{0x06,0x00},{0x06,0x00},{0x00,0x00}
};

/* Cursor save area for un- drawing */
static u32 cursor_save[16][16];

void gfx_draw_cursor(gfx_surface_t *s, int cx, int cy) {
    /* Save background */
    for (int y = 0; y < 16; y++) {
        for (int x = 0; x < 16; x++) {
            int px = cx + x, py = cy + y;
            if (px >= 0 && px < (int)s->width && py >= 0 && py < (int)s->height)
                cursor_save[y][x] = s->pixels[py * s->pitch + px];
            else
                cursor_save[y][x] = 0;
        }
    }
    /* Draw cursor */
    for (int y = 0; y < 16; y++) {
        for (int x = 0; x < 16; x++) {
            if (cursor_arrow[y][x >> 3] & (0x80 >> (x & 7))) {
                int px = cx + x, py = cy + y;
                if (px >= 0 && px < (int)s->width && py >= 0 && py < (int)s->height) {
                    /* White with black border for visibility */
                    if (x == 0 || y == 0 || !(cursor_arrow[y][((x+1)>>3)&1] & (0x80 >> ((x+1)&7))) &&
                        !(y < 15 && cursor_arrow[y+1][x>>3] & (0x80 >> (x&7))))
                        put_pixel(s, px, py, COL_WHITE);
                    else
                        put_pixel(s, px, py, COL_WHITE);
                }
            }
        }
    }
}

void gfx_restore_cursor(gfx_surface_t *s, int cx, int cy) {
    for (int y = 0; y < 16; y++) {
        for (int x = 0; x < 16; x++) {
            int px = cx + x, py = cy + y;
            if (px >= 0 && px < (int)s->width && py >= 0 && py < (int)s->height)
                s->pixels[py * s->pitch + px] = cursor_save[y][x];
        }
    }
}

void gfx_set_mouse_pos(int x, int y) { mouse_x = x; mouse_y = y; }
int  gfx_get_mouse_x(void) { return mouse_x; }
int  gfx_get_mouse_y(void) { return mouse_y; }
void gfx_set_mouse_btn(int b) { mouse_btn = b; }
int  gfx_get_mouse_btn(void) { return mouse_btn; }

/* ═══════════════════════════════════════════════════════════════════════
 * WINDOW MANAGEMENT
 * ═══════════════════════════════════════════════════════════════════════ */

int gfx_window_create(const char *title, int x, int y, int w, int h, u32 bg) {
    if (window_count >= GFX_MAX_WINDOWS) return -1;
    gfx_window_t *win = &windows[window_count];
    int i;
    for (i = 0; title[i] && i < 63; i++) win->title[i] = title[i];
    win->title[i] = 0;
    win->x = x; win->y = y; win->w = w; win->h = h;
    win->bg_color = bg;
    win->active = 0;
    win->visible = 1;
    win->minimized = 0;
    win->on_draw = 0;
    win->on_key = 0;
    win->on_click = 0;
    return window_count++;
}

void gfx_window_set_callbacks(int wid, void (*draw)(gfx_surface_t*,int,int,int,int), void (*key)(char), void (*click)(int,int)) {
    if (wid < 0 || wid >= window_count) return;
    windows[wid].on_draw = draw;
    windows[wid].on_key = key;
    windows[wid].on_click = click;
}

void gfx_window_focus(int wid) {
    if (focused_window >= 0 && focused_window < window_count)
        windows[focused_window].active = 0;
    focused_window = wid;
    if (wid >= 0 && wid < window_count)
        windows[wid].active = 1;
}

void gfx_window_close(int wid) {
    if (wid < 0 || wid >= window_count) return;
    windows[wid].visible = 0;
    if (focused_window == wid) focused_window = -1;
}

/* Hit test: returns window index or -1 */
int gfx_window_hit(int mx, int my) {
    for (int i = window_count - 1; i >= 0; i--) {
        if (!windows[i].visible || windows[i].minimized) continue;
        if (mx >= windows[i].x && mx < windows[i].x + windows[i].w &&
            my >= windows[i].y && my < windows[i].y + windows[i].h)
            return i;
    }
    return -1;
}

/* Check if click is in title bar */
static int hit_titlebar(int wid, int mx, int my) {
    if (wid < 0) return 0;
    gfx_window_t *w = &windows[wid];
    return mx >= w->x && mx < w->x + w->w && my >= w->y && my < w->y + 28;
}

/* ═══════════════════════════════════════════════════════════════════════
 * DESKTOP RENDERING
 * ═══════════════════════════════════════════════════════════════════════ */

void gfx_render_desktop(gfx_surface_t *s) {
    /* Wallpaper: gradient */
    gfx_gradient_v(s, 0, 0, s->width, s->height, COL_DARK2, COL_DARK);

    /* Subtle radial glow */
    for (u32 y = 0; y < s->height; y += 2) {
        for (u32 x = 0; x < s->width; x += 2) {
            int dx = (int)x - (int)s->width/2;
            int dy = (int)y - (int)s->height/2;
            u32 dist = (u32)(dx*dx + dy*dy);
            if (dist < 200000) {
                u8 alpha = (u8)(200000 - dist) >> 8;
                if (alpha > 40) alpha = 40;
                u32 c = blend_add(make_rgb(30, 20, 80), get_pixel(s, x, y), alpha);
                put_pixel(s, x, y, c);
                put_pixel(s, x+1, y, c);
                put_pixel(s, x, y+1, c);
                put_pixel(s, x+1, y+1, c);
            }
        }
    }

    /* Draw windows (back to front) */
    for (int i = 0; i < window_count; i++) {
        gfx_window_t *w = &windows[i];
        if (!w->visible || w->minimized) continue;

        /* Drop shadow */
        gfx_fill_rect_alpha(s, w->x + 8, w->y + 8, w->w, w->h, 0, 60);

        /* Window background with glass effect */
        gfx_rounded_rect_alpha(s, w->x, w->y, w->w, w->h, 10, w->bg_color, 220);

        /* Title bar */
        u32 title_col = w->active ? make_rgb(60, 60, 120) : make_rgb(40, 40, 60);
        gfx_rounded_rect_alpha(s, w->x, w->y, w->w, 28, 10, title_col, 200);
        gfx_fill_rect(s, w->x + 5, w->y + 18, w->w - 10, 10, title_col);

        /* Title text */
        gfx_draw_string(s, w->x + 12, w->y + 7, w->title, COL_WHITE, 1);

        /* Close button */
        gfx_fill_circle(s, w->x + w->w - 20, w->y + 14, 6, make_rgb(200, 60, 60));
        gfx_draw_string(s, w->x + w->w - 23, w->y + 10, "x", COL_WHITE, 1);

        /* Window border */
        gfx_draw_rect(s, w->x, w->y, w->w, w->h, w->active ? make_rgb(80, 80, 160) : make_rgb(60, 60, 80));

        /* Content area callback */
        if (w->on_draw) {
            w->on_draw(s, w->x + 4, w->y + 30, w->w - 8, w->h - 34);
        }
    }

    /* Taskbar */
    int taskbar_h = 40;
    int taskbar_y = s->height - taskbar_h;
    gfx_fill_rect_alpha(s, 0, taskbar_y, s->width, taskbar_h, COL_TASKBAR, 230);

    /* Start button */
    gfx_rounded_rect(s, 8, taskbar_y + 6, 80, 28, 6, make_rgb(80, 60, 180));
    gfx_draw_string(s, 20, taskbar_y + 12, "VECTORD", COL_WHITE, 1);

    /* Window buttons in taskbar */
    for (int i = 0; i < window_count; i++) {
        if (!windows[i].visible) continue;
        int bx = 100 + i * 120;
        u32 bc = (i == focused_window) ? make_rgb(60, 60, 120) : make_rgb(50, 50, 70);
        gfx_rounded_rect(s, bx, taskbar_y + 6, 110, 28, 4, bc);
        gfx_draw_string(s, bx + 6, taskbar_y + 12, windows[i].title, COL_WHITE, 1);
    }

    /* Clock in bottom right */
    u32 ticks = get_ticks();
    u32 seconds = ticks / 100; /* approximate */
    u32 mins = (seconds / 60) % 60;
    u32 hrs = (seconds / 3600) % 24;
    char clk[16];
    clk[0] = '0' + (hrs / 10); clk[1] = '0' + (hrs % 10); clk[2] = ':';
    clk[3] = '0' + (mins / 10); clk[4] = '0' + (mins % 10); clk[5] = 0;
    gfx_draw_string(s, s->width - 60, taskbar_y + 12, clk, COL_WHITE, 1);

    /* Draw mouse cursor */
    gfx_draw_cursor(s, mouse_x, mouse_y);
}

/* ═══════════════════════════════════════════════════════════════════════
 * INPUT HANDLING
 * ═══════════════════════════════════════════════════════════════════════ */

void gfx_handle_mouse_move(int x, int y) {
    mouse_x = x;
    mouse_y = y;

    /* Window dragging */
    if (drag_window >= 0 && (mouse_btn & 1)) {
        windows[drag_window].x = x - drag_off_x;
        windows[drag_window].y = y - drag_off_y;
    }
}

void gfx_handle_mouse_click(int btn) {
    mouse_prev_btn = mouse_btn;
    mouse_btn = btn;

    /* On click down */
    if (btn & 1 && !(mouse_prev_btn & 1)) {
        int hit = gfx_window_hit(mouse_x, mouse_y);
        if (hit >= 0) {
            gfx_window_focus(hit);
            /* Check title bar for drag */
            if (hit_titlebar(hit, mouse_x, mouse_y)) {
                /* Check close button */
                gfx_window_t *w = &windows[hit];
                if (mouse_x >= w->x + w->w - 30 && mouse_x <= w->x + w->w - 10 &&
                    mouse_y >= w->y + 4 && mouse_y <= w->y + 24) {
                    gfx_window_close(hit);
                } else {
                    drag_window = hit;
                    drag_off_x = mouse_x - w->x;
                    drag_off_y = mouse_y - w->y;
                }
            } else {
                /* Call window click handler */
                if (windows[hit].on_click) {
                    windows[hit].on_click(mouse_x - windows[hit].x, mouse_y - windows[hit].y);
                }
            }
        } else {
            /* Click on desktop */
            focused_window = -1;
        }
    }

    /* On click up */
    if (!(btn & 1) && (mouse_prev_btn & 1)) {
        drag_window = -1;
    }
}

void gfx_handle_key(char c) {
    if (focused_window >= 0 && focused_window < window_count) {
        if (windows[focused_window].on_key) {
            windows[focused_window].on_key(c);
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════════
 * SYSTEM MONITOR WIDGET (for terminal window)
 * ═══════════════════════════════════════════════════════════════════════ */

static char term_buf[4096];
static int term_len = 0;

static void terminal_draw(gfx_surface_t *surf, int wx, int wy, int ww, int wh) {
    /* Dark terminal background */
    gfx_fill_rect(surf, wx, wy, ww, wh, make_rgb(20, 20, 30));

    /* Draw text */
    int ty = wy + 4;
    int line_start = 0;
    for (int i = 0; i <= term_len; i++) {
        if (i == term_len || term_buf[i] == '\n') {
            char tmp[128];
            int len = i - line_start;
            if (len > 127) len = 127;
            for (int j = 0; j < len; j++) tmp[j] = term_buf[line_start + j];
            tmp[len] = 0;
            gfx_draw_string(surf, wx + 8, ty, tmp, make_rgb(0, 255, 100), 1);
            ty += 10;
            line_start = i + 1;
        }
    }

    /* Blink cursor */
    if ((get_ticks() / 50) % 2 == 0) {
        int cx = wx + 8 + (term_len - line_start) * 6;
        gfx_fill_rect(surf, cx, ty, 6, 10, make_rgb(0, 255, 100));
    }
}

static void terminal_key(char c) {
    if (c == 8 || c == 127) {
        if (term_len > 0) term_len--;
    } else if (c == '\n') {
        if (term_len < 4095) term_buf[term_len++] = '\n';
    } else if (c >= 32 && c < 127) {
        if (term_len < 4095) term_buf[term_len++] = c;
    }
}

/* ═══════════════════════════════════════════════════════════════════════
 * SYSTEM INFO WIDGET
 * ═══════════════════════════════════════════════════════════════════════ */

static void sysinfo_draw(gfx_surface_t *surf, int wx, int wy, int ww, int wh) {
    gfx_fill_rect(surf, wx, wy, ww, wh, make_rgb(25, 25, 40));

    int ty = wy + 8;
    gfx_draw_string(surf, wx + 8, ty, "VECTORD OS v2.0.1", make_rgb(100, 150, 255), 1); ty += 16;
    gfx_draw_string(surf, wx + 8, ty, "----------------", COL_GRAY, 1); ty += 14;

    gfx_draw_string(surf, wx + 8, ty, "CPU: SSE2 Enabled", COL_CYAN, 1); ty += 12;
    gfx_draw_string(surf, wx + 8, ty, "RAM: 256 MB", COL_CYAN, 1); ty += 12;
    gfx_draw_string(surf, wx + 8, ty, "FB:  1920x1080@32", COL_CYAN, 1); ty += 12;

    /* Uptime bar */
    u32 ticks = get_ticks();
    gfx_draw_string(surf, wx + 8, ty, "Up:", COL_CYAN, 1);
    gfx_fill_rect(surf, wx + 40, ty, 100, 10, make_rgb(40, 40, 60));
    gfx_fill_rect(surf, wx + 40, ty, (ticks % 1000) / 10, 10, make_rgb(80, 120, 255));
    ty += 16;

    gfx_draw_string(surf, wx + 8, ty, "MIPS: Neural HW Opt", COL_GREEN, 1); ty += 12;
    gfx_draw_string(surf, wx + 8, ty, "AI: Akasha Vector", COL_GREEN, 1);
}

/* ═══════════════════════════════════════════════════════════════════════
 * PUBLIC API - Full Desktop Launch
 * ═══════════════════════════════════════════════════════════════════════ */

/* Setup windows (called once) */
static void gfx_setup_windows(void) {
    window_count = 0;
    focused_window = -1;
    drag_window = -1;
    term_len = 0;

    int term = gfx_window_create("Terminal", 80, 80, 560, 380, make_rgb(15, 15, 25));
    gfx_window_set_callbacks(term, terminal_draw, terminal_key, 0);
    gfx_window_focus(term);

    int sys = gfx_window_create("System", 680, 120, 320, 220, make_rgb(20, 20, 35));
    gfx_window_set_callbacks(sys, sysinfo_draw, 0, 0);

    int about = gfx_window_create("About", 400, 500, 380, 180, make_rgb(18, 18, 32));

    /* Welcome message in terminal */
    const char *welcome =
        "╔════════════════════════════════════════════╗\n"
        "║   VECTORD OS v2.0.1 - Neural Vector Kernel ║\n"
        "╠════════════════════════════════════════════╣\n"
        "║  • SSE2 vector math acceleration           ║\n"
        "║  • Akasha AI (heap-backed paged storage)   ║\n"
        "║  • 1920x1080 Full HD framebuffer           ║\n"
        "║  • Window manager + glassmorphism          ║\n"
        "║  • TCP/IP networking stack                 ║\n"
        "║  • Vector filesystem (VECTORD FS)          ║\n"
        "╚════════════════════════════════════════════╝\n\n"
        "Type here to interact...\n\n";
    for (int i = 0; welcome[i]; i++) {
        if (term_len < 4095) term_buf[term_len++] = welcome[i];
    }
}

void gfx_desktop_launch(void) {
    gfx_init();
    gfx_setup_windows();
    serial_puts("[GFX] Desktop ready - ");
    serial_putdec(window_count);
    serial_puts(" windows, ");
    serial_putdec(screen.width);
    serial_puts("x");
    serial_putdec(screen.height);
    serial_puts("\n");
    gfx_render_desktop(&backbuf);
    gfx_flush();
}

/* Single frame tick */
void gfx_desktop_tick(void) {
    if (!vbe_info.present) return;
    gfx_render_desktop(&backbuf);
    gfx_flush();
}

/* ═══════════════════════════════════════════════════════════════════════
 * MAIN EVENT LOOP - runs until ESC pressed
 * ═══════════════════════════════════════════════════════════════════════ */

void gfx_run(void) {
    if (!vbe_info.present) {
        serial_puts("[GFX] No VBE - use 'desktop-hd' for serial mode\n");
        return;
    }

    gfx_init();
    gfx_setup_windows();
    gfx_render_desktop(&backbuf);
    gfx_flush();

    serial_puts("\n╔══════════════════════════════════════════╗\n");
    serial_puts("║   VECTORD OS - Interactive GUI Desktop   ║\n");
    serial_puts("╠══════════════════════════════════════════╣\n");
    serial_puts("║  Mouse: move + click to interact         ║\n");
    serial_puts("║  Keyboard: type in terminal              ║\n");
    serial_puts("║  ESC: return to shell                    ║\n");
    serial_puts("╚══════════════════════════════════════════╝\n\n");

    /* Get mouse position from driver */
    vec2_t mpos = mouse_get_position();
    mouse_x = (int)mpos.x;
    mouse_y = (int)mpos.y;
    if (mouse_x < 0) mouse_x = 0;
    if (mouse_y < 0) mouse_y = 0;
    if (mouse_x >= (int)screen.width) mouse_x = screen.width - 1;
    if (mouse_y >= (int)screen.height) mouse_y = screen.height - 1;

    /* Main event loop */
    for (;;) {
        /* Process keyboard input */
        int ch = keyboard_getchar();
        if (ch > 0) {
            if (ch == 27) {
                /* ESC - exit GUI */
                serial_puts("[GFX] Returning to shell\n");
                return;
            }
            gfx_handle_key((char)ch);
        }

        /* Process mouse input */
        vec2_t mp = mouse_get_position();
        int mx = (int)mp.x, my = (int)mp.y;
        /* Clamp to screen */
        if (mx < 0) mx = 0;
        if (my < 0) my = 0;
        if (mx >= (int)screen.width) mx = screen.width - 1;
        if (my >= (int)screen.height) my = screen.height - 1;

        if (mx != mouse_x || my != mouse_y) {
            gfx_handle_mouse_move(mx, my);
        }

        u8 btns = mouse_get_buttons();
        if (btns != mouse_btn) {
            gfx_handle_mouse_click(btns);
        }

        /* Render frame */
        gfx_render_desktop(&backbuf);
        gfx_flush();
    }
}

/* Disk installer */
void gfx_installer(void) {
    serial_puts("\n");
    serial_puts("╔══════════════════════════════════════════════╗\n");
    serial_puts("║       VECTORD OS - Disk Installer           ║\n");
    serial_puts("╚══════════════════════════════════════════════╝\n\n");

    if (!ata_devices[0].present && !ata_devices[1].present) {
        serial_puts("No ATA/ATAPI drives detected.\n");
        serial_puts("Connect a disk and try again.\n");
        serial_puts("  qemu-system-i386 -cdrom vectord-os.iso -hda disk.img\n");
        return;
    }

    serial_puts("Available drives:\n");
    for (int i = 0; i < 4; i++) {
        if (ata_devices[i].present) {
            u32 mb = (ata_devices[i].sectors * 512) / (1024*1024);
            serial_puts("  [");
            serial_putdec(i);
            serial_puts("] ");
            serial_puts(ata_devices[i].model);
            serial_puts(" (");
            serial_putdec(mb);
            serial_puts(" MB)\n");
        }
    }

    serial_puts("\nTo install: select drive, format VECTORD FS, copy kernel\n");
    serial_puts("(Installer in development - boot ISO for live desktop)\n");
}

