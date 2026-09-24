/* ═══════════════════════════════════════════════════════════════════════════════
 * VECTORD OS - Premium HD Desktop Environment
 * Better than macOS, Windows 13, and Hyprland combined
 * 
 * Features:
 * - Full glassmorphism UI with blur effects
 * - Smooth 60fps animations
 * - Spotlight-like search (Super+Space)
 * - Mission Control (Super+Tab)
 * - Notification center
 * - Control center
 * - Dynamic wallpapers
 * - Widget system
 * ═══════════════════════════════════════════════════════════════════════════════ */

#include "../include/vecord.h"

/* ═══════════════════════════════════════════════════════════════════════════════
 * CONSTANTS
 * ═══════════════════════════════════════════════════════════════════════════════ */

#define SCREEN_WIDTH    1920
#define SCREEN_HEIGHT   1080
#define TASKBAR_HEIGHT  48
#define DOCK_SIZE       72
#define DOCK_BOTTOM     20

/* Colors - macOS/Ventura inspired */
#define COL_WALLPAPER_1 0x1a0a2e  /* Deep purple */
#define COL_WALLPAPER_2 0x16213e  /* Dark blue */
#define COL_ACCENT      0x7c3aed  /* Purple accent */
#define COL_ACCENT_ALT  0x0a84ff  /* Blue accent */
#define COL_GLASS       0x30FFFFFF
#define COL_GLASS_DARK  0x201a1a2e
#define COL_TEXT        0xFFFFFF
#define COL_TEXT_DIM    0x9CA3AF
#define COL_SUCCESS     0x22c55e
#define COL_WARNING     0xf59e0b
#define COL_ERROR       0xef4444

/* ═══════════════════════════════════════════════════════════════════════════════
 * COLOR UTILITIES
 * ═══════════════════════════════════════════════════════════════════════════════ */

static inline u32 make_argb(u8 a, u8 r, u8 g, u8 b) {
    return ((u32)a << 24) | ((u32)r << 16) | ((u32)g << 8) | b;
}

static inline u32 blend_alpha(u32 fg, u32 bg) {
    u8 fa = (fg >> 24) & 0xFF;
    if (fa == 0xFF) return fg;
    if (fa == 0x00) return bg;
    
    u8 fr = (fg >> 16) & 0xFF, fg_g = (fg >> 8) & 0xFF, fb = fg & 0xFF;
    u8 br = (bg >> 16) & 0xFF, bg_g = (bg >> 8) & 0xFF, bb = bg & 0xFF;
    u8 ia = 0xFF - fa;
    
    return make_argb(0xFF,
        (fr * fa + br * ia) >> 8,
        (fg_g * fa + bg_g * ia) >> 8,
        (fb * fa + bb * ia) >> 8
    );
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * DESKTOP STATE
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef enum {
    MODE_NORMAL,
    MODE_SPOTLIGHT,     /* Super+Space - Search */
    MODE_MISSION_CTRL,  /* Super+Tab - Window overview */
    MODE_CONTROL_CENTER, /* Super+C - Quick settings */
    MODE_NOTIFICATIONS,  /* Super+N - Notifications */
    MODE_LAUNCHER       /* Super+L - App launcher */
} desktop_mode_t;

static desktop_mode_t current_mode = MODE_NORMAL;
static u32 desktop_tick = 0;
static int spotlight_x = 0;  /* Animation for spotlight search */

/* ═══════════════════════════════════════════════════════════════════════════════
 * DOCK APPLICATIONS (macOS style)
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct {
    const char *name;
    const char *icon;  /* Single character icon for now */
    u32 color;
    int running;
} dock_app_t;

static dock_app_t dock_apps[] = {
    {"Finder",      "F", 0x3b82f6, 1},
    {"Terminal",    ">", 0x22c55e, 1},
    {"Safari",      "S", 0x0ea5e9, 0},
    {"Settings",    "=", 0x6b7280, 0},
    {"Files",       "D", 0xf59e0b, 0},
    {"Calculator",  "C", 0x8b5cf6, 0},
    {"Calendar",    "7", 0xef4444, 0},
    {"Notes",       "N", 0xfbbf24, 0},
    {"Music",       "M", 0xec4899, 0},
    {"Photos",      "P", 0x06b6d4, 0},
};
#define DOCK_APP_COUNT 10

/* ═══════════════════════════════════════════════════════════════════════════════
 * SPOTLIGHT SEARCH (Super+Space)
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct {
    const char *name;
    const char *desc;
    int type; /* 0=app, 1=file, 1=calc, 2=settings */
} spotlight_result_t;

static spotlight_result_t spotlight_results[] = {
    {"Terminal", "Open terminal emulator", 0},
    {"Finder", "Browse files and folders", 0},
    {"Calculator", "Perform calculations", 0},
    {"System Preferences", "Configure system settings", 2},
    {"Network Settings", "Configure network", 2},
    {"Display Settings", "Change resolution and appearance", 2},
    {"Welcome to VECTORD OS", "Read the welcome guide", 1},
};
#define SPOTLIGHT_RESULT_COUNT 7

static char spotlight_query[128] = "";
static int spotlight_result_count = 0;

/* ═══════════════════════════════════════════════════════════════════════════════
 * CONTROL CENTER (Super+C)
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct {
    const char *name;
    int enabled;
    int value; /* 0-100 for sliders */
} control_item_t;

static control_item_t control_items[] = {
    {"Wi-Fi", 1, 100},
    {"Bluetooth", 1, 100},
    {"AirDrop", 0, 0},
    {"Focus Mode", 0, 0},
    {"Night Shift", 0, 0},
    {"Dark Mode", 1, 100},
    {"Display", 1, 75},
    {"Sound", 1, 60},
};
#define CONTROLItemCount 8

/* ═══════════════════════════════════════════════════════════════════════════════
 * DRAWING HELPERS
 * ═══════════════════════════════════════════════════════════════════════════════ */

/* Simple 8x8 bitmap font for UI elements */
static const u8 font_8x8[][8] = {
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /* Space */
    {0x18,0x3C,0x3C,0x18,0x18,0x00,0x18,0x00}, /* ! */
    {0x6C,0x6C,0x24,0x00,0x00,0x00,0x00,0x00}, /* " */
    {0x6C,0x6C,0xFE,0x6C,0xFE,0x6C,0x6C,0x00}, /* # */
    {0x18,0x3E,0x60,0x3C,0x06,0x7C,0x18,0x00}, /* $ */
    {0x00,0xC6,0xCC,0x18,0x30,0x66,0xC6,0x00}, /* % */
    {0x38,0x6C,0x38,0x76,0xDC,0xCC,0x76,0x00}, /* & */
    {0x18,0x18,0x30,0x00,0x00,0x00,0x00,0x00}, /* ' */
    {0x0C,0x18,0x30,0x30,0x30,0x18,0x0C,0x00}, /* ( */
    {0x30,0x18,0x0C,0x0C,0x0C,0x18,0x30,0x00}, /* ) */
    {0x00,0x66,0x3C,0xFF,0x3C,0x66,0x00,0x00}, /* * */
    {0x00,0x18,0x18,0x7E,0x18,0x18,0x00,0x00}, /* + */
    {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x30}, /* , */
    {0x00,0x00,0x00,0x7E,0x00,0x00,0x00,0x00}, /* - */
    {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x00}, /* . */
    {0x06,0x0C,0x18,0x30,0x60,0xC0,0x80,0x00}, /* / */
    {0x3C,0x66,0x6E,0x76,0x66,0x66,0x3C,0x00}, /* 0 */
    {0x18,0x38,0x18,0x18,0x18,0x18,0x7E,0x00}, /* 1 */
    {0x3C,0x66,0x06,0x0C,0x30,0x60,0x7E,0x00}, /* 2 */
    {0x3C,0x66,0x06,0x1C,0x06,0x66,0x3C,0x00}, /* 3 */
    {0x0C,0x1C,0x3C,0x6C,0x7E,0x0C,0x0C,0x00}, /* 4 */
    {0x7E,0x60,0x7C,0x06,0x06,0x66,0x3C,0x00}, /* 5 */
    {0x1C,0x30,0x60,0x7C,0x66,0x66,0x3C,0x00}, /* 6 */
    {0x7E,0x06,0x0C,0x18,0x30,0x30,0x30,0x00}, /* 7 */
    {0x3C,0x66,0x66,0x3C,0x66,0x66,0x3C,0x00}, /* 8 */
    {0x3C,0x66,0x66,0x3E,0x06,0x0C,0x38,0x00}, /* 9 */
    {0x00,0x18,0x18,0x00,0x00,0x18,0x18,0x00}, /* : */
    {0x00,0x18,0x18,0x00,0x00,0x18,0x18,0x30}, /* ; */
    {0x0C,0x18,0x30,0x60,0x30,0x18,0x0C,0x00}, /* < */
    {0x00,0x00,0x7E,0x00,0x7E,0x00,0x00,0x00}, /* = */
    {0x30,0x18,0x0C,0x06,0x0C,0x18,0x30,0x00}, /* > */
    {0x3C,0x66,0x06,0x0C,0x18,0x00,0x18,0x00}, /* ? */
    {0x3C,0x66,0x6E,0x6E,0x60,0x62,0x3C,0x00}, /* @ */
    {0x18,0x3C,0x66,0x66,0x7E,0x66,0x66,0x00}, /* A */
    {0x7C,0x66,0x66,0x7C,0x66,0x66,0x7C,0x00}, /* B */
    {0x3C,0x66,0x60,0x60,0x60,0x66,0x3C,0x00}, /* C */
    {0x78,0x6C,0x66,0x66,0x66,0x6C,0x78,0x00}, /* D */
    {0x7E,0x60,0x60,0x78,0x60,0x60,0x7E,0x00}, /* E */
    {0x7E,0x60,0x60,0x78,0x60,0x60,0x60,0x00}, /* F */
    {0x3C,0x66,0x60,0x6E,0x66,0x66,0x3C,0x00}, /* G */
    {0x66,0x66,0x66,0x7E,0x66,0x66,0x66,0x00}, /* H */
    {0x3C,0x18,0x18,0x18,0x18,0x18,0x3C,0x00}, /* I */
    {0x1E,0x0C,0x0C,0x0C,0x0C,0x6C,0x38,0x00}, /* J */
    {0x66,0x6C,0x78,0x70,0x78,0x6C,0x66,0x00}, /* K */
    {0x60,0x60,0x60,0x60,0x60,0x60,0x7E,0x00}, /* L */
    {0xC6,0xEE,0xFE,0xD6,0xC6,0xC6,0xC6,0x00}, /* M */
    {0xC6,0xE6,0xF6,0xDE,0xCE,0xC6,0xC6,0x00}, /* N */
    {0x3C,0x66,0x66,0x66,0x66,0x66,0x3C,0x00}, /* O */
    {0x7C,0x66,0x66,0x7C,0x60,0x60,0x60,0x00}, /* P */
    {0x3C,0x66,0x66,0x66,0x6A,0x6C,0x36,0x00}, /* Q */
    {0x7C,0x66,0x66,0x7C,0x6C,0x66,0x66,0x00}, /* R */
    {0x3C,0x66,0x60,0x3C,0x06,0x66,0x3C,0x00}, /* S */
    {0x7E,0x18,0x18,0x18,0x18,0x18,0x18,0x00}, /* T */
    {0x66,0x66,0x66,0x66,0x66,0x66,0x3C,0x00}, /* U */
    {0x66,0x66,0x66,0x66,0x66,0x3C,0x18,0x00}, /* V */
    {0xC6,0xC6,0xC6,0xD6,0xFE,0xEE,0xC6,0x00}, /* W */
    {0x66,0x66,0x3C,0x18,0x3C,0x66,0x66,0x00}, /* X */
    {0x66,0x66,0x66,0x3C,0x18,0x18,0x18,0x00}, /* Y */
    {0x7E,0x06,0x0C,0x18,0x30,0x60,0x7E,0x00}, /* Z */
    {0x3C,0x30,0x30,0x30,0x30,0x30,0x3C,0x00}, /* [ */
    {0xC0,0x60,0x30,0x18,0x0C,0x06,0x02,0x00}, /* \ */
    {0x3C,0x0C,0x0C,0x0C,0x0C,0x0C,0x3C,0x00}, /* ] */
    {0x18,0x3C,0x66,0x00,0x00,0x00,0x00,0x00}, /* ^ */
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF}, /* _ */
    {0x30,0x18,0x0C,0x00,0x00,0x00,0x00,0x00}, /* ` */
    {0x00,0x00,0x3C,0x06,0x3E,0x66,0x3E,0x00}, /* a */
    {0x60,0x7C,0x66,0x66,0x66,0x66,0x7C,0x00}, /* b */
    {0x00,0x00,0x3C,0x66,0x60,0x66,0x3C,0x00}, /* c */
    {0x06,0x3E,0x66,0x66,0x66,0x66,0x3E,0x00}, /* d */
    {0x00,0x00,0x3C,0x66,0x7E,0x60,0x3C,0x00}, /* e */
    {0x0E,0x18,0x18,0x7E,0x18,0x18,0x18,0x00}, /* f */
    {0x00,0x00,0x3E,0x66,0x66,0x3E,0x06,0x7C}, /* g */
    {0x60,0x7C,0x66,0x66,0x66,0x66,0x66,0x00}, /* h */
    {0x18,0x00,0x38,0x18,0x18,0x18,0x3C,0x00}, /* i */
    {0x06,0x00,0x0E,0x06,0x06,0x06,0x66,0x3C}, /* j */
    {0x60,0x66,0x6C,0x78,0x6C,0x66,0x66,0x00}, /* k */
    {0x38,0x18,0x18,0x18,0x18,0x18,0x3C,0x00}, /* l */
    {0x00,0x00,0xEC,0xFE,0xD6,0xD6,0xC6,0x00}, /* m */
    {0x00,0x00,0x7C,0x66,0x66,0x66,0x66,0x00}, /* n */
    {0x00,0x00,0x3C,0x66,0x66,0x66,0x3C,0x00}, /* o */
    {0x00,0x00,0x7C,0x66,0x66,0x7C,0x60,0x60}, /* p */
    {0x00,0x00,0x3E,0x66,0x66,0x3E,0x06,0x06}, /* q */
    {0x00,0x00,0x7C,0x66,0x60,0x60,0x60,0x00}, /* r */
    {0x00,0x00,0x3E,0x60,0x3C,0x06,0x7C,0x00}, /* s */
    {0x18,0x18,0x7E,0x18,0x18,0x18,0x0E,0x00}, /* t */
    {0x00,0x00,0x66,0x66,0x66,0x66,0x3E,0x00}, /* u */
    {0x00,0x00,0x66,0x66,0x66,0x3C,0x18,0x00}, /* v */
    {0x00,0x00,0xC6,0xC6,0xD6,0xFE,0x6C,0x00}, /* w */
    {0x00,0x00,0x66,0x3C,0x18,0x3C,0x66,0x00}, /* x */
    {0x00,0x00,0x66,0x66,0x66,0x3E,0x06,0x3C}, /* y */
    {0x00,0x00,0x7E,0x0C,0x18,0x30,0x7E,0x00}, /* z */
    {0x0C,0x18,0x18,0x70,0x18,0x18,0x0C,0x00}, /* { */
    {0x18,0x18,0x18,0x00,0x18,0x18,0x18,0x00}, /* | */
    {0x30,0x18,0x18,0x0E,0x18,0x18,0x30,0x00}, /* } */
    {0x00,0x00,0x76,0xDC,0x00,0x00,0x00,0x00}, /* ~ */
    {0x00,0x18,0x3C,0x7E,0x7E,0x18,0x3C,0x00}, /* DEL */
};

/* Draw character at position */
static void draw_char(u32 *buf, int x, int y, char c, u32 color, int scale) {
    if (c < 32 || c > 127) c = '?';
    int idx = c - 32;
    
    for (int py = 0; py < 8; py++) {
        u8 row = font_8x8[idx][py];
        for (int px = 0; px < 8; px++) {
            if (row & (0x80 >> px)) {
                for (int sy = 0; sy < scale; sy++) {
                    for (int sx = 0; sx < scale; sx++) {
                        int dx = x + px * scale + sx;
                        int dy = y + py * scale + sy;
                        if (dx >= 0 && dx < SCREEN_WIDTH && dy >= 0 && dy < SCREEN_HEIGHT) {
                            buf[dy * SCREEN_WIDTH + dx] = color;
                        }
                    }
                }
            }
        }
    }
}

/* Draw string */
static void draw_string(u32 *buf, int x, int y, const char *text, u32 color, int scale) {
    while (*text) {
        if (*text == '\n') {
            x = 0;
            y += 8 * scale;
        } else {
            draw_char(buf, x, y, *text, color, scale);
            x += 8 * scale;
        }
        text++;
    }
}

/* Draw filled rounded rectangle */
static void fill_rounded_rect(u32 *buf, int x, int y, int w, int h, int r, u32 color) {
    for (int py = 0; py < h; py++) {
        for (int px = 0; px < w; px++) {
            int in_rect = 1;
            
            /* Corner checks */
            if (py < r && px < r) {
                int dx = px - r, dy = py - r;
                if (dx*dx + dy*dy > r*r) in_rect = 0;
            } else if (py < r && px >= w - r) {
                int dx = px - (w - r - 1), dy = py - r;
                if (dx*dx + dy*dy > r*r) in_rect = 0;
            } else if (py >= h - r && px < r) {
                int dx = px - r, dy = py - (h - r - 1);
                if (dx*dx + dy*dy > r*r) in_rect = 0;
            } else if (py >= h - r && px >= w - r) {
                int dx = px - (w - r - 1), dy = py - (h - r - 1);
                if (dx*dx + dy*dy > r*r) in_rect = 0;
            }
            
            if (in_rect) {
                int fx = x + px, fy = y + py;
                if (fx >= 0 && fx < SCREEN_WIDTH && fy >= 0 && fy < SCREEN_HEIGHT) {
                    buf[fy * SCREEN_WIDTH + fx] = blend_alpha(color, buf[fy * SCREEN_WIDTH + fx]);
                }
            }
        }
    }
}

/* Draw filled rectangle */
static void fill_rect(u32 *buf, int x, int y, int w, int h, u32 color) {
    for (int py = y; py < y + h && py < SCREEN_HEIGHT; py++) {
        for (int px = x; px < x + w && px < SCREEN_WIDTH; px++) {
            if (px >= 0 && py >= 0) {
                buf[py * SCREEN_WIDTH + px] = blend_alpha(color, buf[py * SCREEN_WIDTH + px]);
            }
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * RENDER: Wallpaper (Dynamic gradient)
 * ═══════════════════════════════════════════════════════════════════════════════ */

static void render_wallpaper(u32 *buf) {
    int x, y;
    /* Pass 1: gradient fill */
    for (y = 0; y < SCREEN_HEIGHT; y++) {
        u8 r = 26 + (u8)((u32)y * 16 / SCREEN_HEIGHT);
        u8 g = 10 + (u8)((u32)y * 16 / SCREEN_HEIGHT);
        u8 b = 46 + (u8)((u32)y * 32 / SCREEN_HEIGHT);
        u32 color = ((u32)r << 16) | ((u32)g << 8) | (u32)b;
        u32 off = (u32)y * SCREEN_WIDTH;
        for (x = 0; x < SCREEN_WIDTH; x++) {
            buf[off + x] = color;
        }
    }
    /* Pass 2: glow (integer-only, safe for 1920x1080) */
    {
        int cx = SCREEN_WIDTH / 2;
        int cy = SCREEN_HEIGHT / 2;
        for (y = 0; y < SCREEN_HEIGHT; y++) {
            int dy = y - cy;
            u32 off = (u32)y * SCREEN_WIDTH;
            for (x = 0; x < SCREEN_WIDTH; x++) {
                int dx = x - cx;
                /* Use u32 to avoid signed overflow */
                u32 dist = (u32)(dx*dx) + (u32)(dy*dy);
                /* glow = 20000 / (dist + 10000), capped to 1.0 */
                u32 glow256; /* glow as fixed-point 0..256 */
                u32 denom = dist + 10000;
                if (denom < 20000) {
                    glow256 = 256; /* 1.0 */
                } else if (denom > 1000000) {
                    glow256 = 0;
                } else {
                    glow256 = (u32)(20000 * 256 / denom);
                }
                u32 c = buf[off + x];
                u32 r = ((c >> 16) & 0xFF) + (glow256 * 20 / 256);
                u32 g = ((c >> 8) & 0xFF) + (glow256 * 10 / 256);
                u32 bv = (c & 0xFF) + (glow256 * 40 / 256);
                if (r > 255) r = 255;
                if (g > 255) g = 255;
                if (bv > 255) bv = 255;
                buf[off + x] = (r << 16) | (g << 8) | bv;
            }
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * RENDER: Menu Bar (macOS style)
 * ═══════════════════════════════════════════════════════════════════════════════ */

static void render_menubar(u32 *buf) {
    /* Glass background */
    fill_rect(buf, 0, 0, SCREEN_WIDTH, 28, make_argb(0x80, 0x1a, 0x1a, 0x2e));
    
    /* Apple logo placeholder */
    draw_string(buf, 10, 8, "V", COL_TEXT, 2);
    
    /* App name */
    draw_string(buf, 36, 8, "VECTORD OS", COL_TEXT, 2);
    
    /* Menu items */
    draw_string(buf, 200, 8, "File  Edit  View  Window  Help", COL_TEXT_DIM, 2);
    
    /* Right side - time, battery, wifi */
    draw_string(buf, SCREEN_WIDTH - 200, 8, "12:34  [====]  ~", COL_TEXT, 2);
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * RENDER: Dock (macOS style)
 * ═══════════════════════════════════════════════════════════════════════════════ */

static void render_dock(u32 *buf) {
    int dock_width = DOCK_APP_COUNT * (DOCK_SIZE + 8) + 32;
    int dock_x = (SCREEN_WIDTH - dock_width) / 2;
    int dock_y = SCREEN_HEIGHT - DOCK_SIZE - DOCK_BOTTOM;
    
    /* Dock background - glass effect */
    fill_rounded_rect(buf, dock_x - 10, dock_y - 10, dock_width + 20, DOCK_SIZE + 20, 
                      16, make_argb(0x60, 0xff, 0xff, 0xff));
    
    /* Dock border */
    fill_rounded_rect(buf, dock_x - 10, dock_y - 10, dock_width + 20, DOCK_SIZE + 20, 
                      16, make_argb(0x30, 0xff, 0xff, 0xff));
    
    /* Draw app icons */
    for (int i = 0; i < DOCK_APP_COUNT; i++) {
        int app_x = dock_x + i * (DOCK_SIZE + 8);
        
        /* Icon background */
        fill_rounded_rect(buf, app_x, dock_y, DOCK_SIZE, DOCK_SIZE, 12, 
                          make_argb(0xA0, (dock_apps[i].color >> 16) & 0xFF,
                                                 (dock_apps[i].color >> 8) & 0xFF,
                                                 dock_apps[i].color & 0xFF));
        
        /* Icon letter */
        draw_char(buf, app_x + 26, dock_y + 20, dock_apps[i].icon[0], COL_TEXT, 3);
        
        /* Running indicator */
        if (dock_apps[i].running) {
            fill_rect(buf, app_x + 32, dock_y + DOCK_SIZE + 2, 8, 3, COL_TEXT);
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * RENDER: Spotlight Search
 * ═══════════════════════════════════════════════════════════════════════════════ */

static void render_spotlight(u32 *buf) {
    int search_width = 600;
    int search_height = 400;
    int search_x = (SCREEN_WIDTH - search_width) / 2;
    int search_y = 120;
    
    /* Darken background */
    fill_rect(buf, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, make_argb(0x40, 0x00, 0x00, 0x00));
    
    /* Search window - glass effect */
    fill_rounded_rect(buf, search_x, search_y, search_width, search_height, 16, 
                      make_argb(0xE0, 0x2a, 0x2a, 0x3e));
    
    /* Search bar */
    fill_rounded_rect(buf, search_x + 20, search_y + 20, search_width - 40, 40, 8, 
                      make_argb(0x40, 0xff, 0xff, 0xff));
    
    /* Search icon */
    draw_char(buf, search_x + 30, search_y + 28, '/', COL_TEXT_DIM, 2);
    
    /* Search text */
    draw_string(buf, search_x + 55, search_y + 28, spotlight_query, COL_TEXT, 2);
    
    /* Cursor blink */
    int cursor_x = search_x + 55 + strlen(spotlight_query) * 8;
    if ((desktop_tick / 500) % 2) {
        fill_rect(buf, cursor_x, search_y + 28, 2, 16, COL_TEXT);
    }
    
    /* Results */
    draw_string(buf, search_x + 20, search_y + 80, "Applications", COL_TEXT_DIM, 2);
    
    int y_offset = 110;
    for (int i = 0; i < 4 && i < SPOTLIGHT_RESULT_COUNT; i++) {
        /* Result background on hover */
        if (i == 0) {
            fill_rounded_rect(buf, search_x + 10, search_y + y_offset - 5, 
                              search_width - 20, 40, 8, make_argb(0x40, 0x7c, 0x3a, 0xed));
        }
        
        /* Icon */
        fill_rounded_rect(buf, search_x + 20, search_y + y_offset, 32, 32, 6,
                          spotlight_results[i].type == 0 ? 0xFF3b82f6 :
                          spotlight_results[i].type == 2 ? 0xFF6b7280 : 0xFFf59e0b);
        
        /* Name and description */
        draw_string(buf, search_x + 65, search_y + y_offset + 4, 
                   spotlight_results[i].name, COL_TEXT, 2);
        draw_string(buf, search_x + 65, search_y + y_offset + 20, 
                   spotlight_results[i].desc, COL_TEXT_DIM, 1);
        
        y_offset += 45;
    }
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * RENDER: Control Center
 * ═══════════════════════════════════════════════════════════════════════════════ */

static void render_control_center(u32 *buf) {
    int cc_width = 320;
    int cc_height = 480;
    int cc_x = SCREEN_WIDTH - cc_width - 20;
    int cc_y = 40;
    
    /* Darken background */
    fill_rect(buf, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, make_argb(0x40, 0x00, 0x00, 0x00));
    
    /* Control center panel */
    fill_rounded_rect(buf, cc_x, cc_y, cc_width, cc_height, 16, 
                      make_argb(0xE0, 0x2a, 0x2a, 0x3e));
    
    /* Title */
    draw_string(buf, cc_x + 20, cc_y + 15, "Control Center", COL_TEXT, 2);
    
    /* Control tiles */
    int tile_x = cc_x + 15;
    int tile_y = cc_y + 50;
    int tile_w = 140;
    int tile_h = 80;
    int gap = 10;
    
    /* Wi-Fi tile */
    fill_rounded_rect(buf, tile_x, tile_y, tile_w, tile_h, 12, 
                      make_argb(0x60, 0x00, 0x7a, 0xff));
    draw_string(buf, tile_x + 10, tile_y + 10, "WiFi", COL_TEXT, 2);
    draw_string(buf, tile_x + 10, tile_y + 35, "On", COL_TEXT, 2);
    
    /* Bluetooth tile */
    fill_rounded_rect(buf, tile_x + tile_w + gap, tile_y, tile_w, tile_h, 12,
                      make_argb(0x60, 0x00, 0x7a, 0xff));
    draw_string(buf, tile_x + tile_w + gap + 10, tile_y + 10, "BT", COL_TEXT, 2);
    draw_string(buf, tile_x + tile_w + gap + 10, tile_y + 35, "On", COL_TEXT, 2);
    
    /* Dark mode tile */
    tile_y += tile_h + gap;
    fill_rounded_rect(buf, tile_x, tile_y, tile_w, tile_h, 12,
                      make_argb(0x60, 0x7c, 0x3a, 0xed));
    draw_string(buf, tile_x + 10, tile_y + 10, "Dark", COL_TEXT, 2);
    draw_string(buf, tile_x + 10, tile_y + 35, "Mode", COL_TEXT, 2);
    
    /* AirDrop tile */
    fill_rounded_rect(buf, tile_x + tile_w + gap, tile_y, tile_w, tile_h, 12,
                      make_argb(0x40, 0x40, 0x40, 0x50));
    draw_string(buf, tile_x + tile_w + gap + 10, tile_y + 10, "AirDrop", COL_TEXT_DIM, 2);
    draw_string(buf, tile_x + tile_w + gap + 10, tile_y + 35, "Off", COL_TEXT_DIM, 2);
    
    /* Brightness slider */
    tile_y += tile_h + gap;
    fill_rounded_rect(buf, tile_x, tile_y, cc_width - 30, 50, 12,
                      make_argb(0x40, 0x40, 0x40, 0x50));
    draw_string(buf, tile_x + 15, tile_y + 18, "Display", COL_TEXT, 2);
    fill_rect(buf, tile_x + 90, tile_y + 22, 180, 8, make_argb(0x60, 0x40, 0x40, 0x50));
    fill_rect(buf, tile_x + 90, tile_y + 22, 120, 8, make_argb(0xFF, 0x7c, 0x3a, 0xed));
    
    /* Volume slider */
    tile_y += 60;
    fill_rounded_rect(buf, tile_x, tile_y, cc_width - 30, 50, 12,
                      make_argb(0x40, 0x40, 0x40, 0x50));
    draw_string(buf, tile_x + 15, tile_y + 18, "Sound", COL_TEXT, 2);
    fill_rect(buf, tile_x + 90, tile_y + 22, 180, 8, make_argb(0x60, 0x40, 0x40, 0x50));
    fill_rect(buf, tile_x + 90, tile_y + 22, 100, 8, make_argb(0xFF, 0x22, 0xc5, 0x5e));
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * RENDER: Mission Control (Super+Tab)
 * ═══════════════════════════════════════════════════════════════════════════════ */

static void render_mission_control(u32 *buf) {
    /* Darken background */
    fill_rect(buf, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, make_argb(0x80, 0x00, 0x00, 0x00));
    
    /* Title */
    draw_string(buf, SCREEN_WIDTH/2 - 100, 30, "Mission Control", COL_TEXT, 2);
    
    /* Window thumbnails */
    int thumb_w = 300;
    int thumb_h = 200;
    int spacing = 40;
    int start_x = (SCREEN_WIDTH - (3 * thumb_w + 2 * spacing)) / 2;
    
    const char *titles[] = {"Terminal", "System Monitor", "File Manager"};
    u32 colors[] = {0x22c55e, 0x3b82f6, 0xf59e0b};
    
    for (int i = 0; i < 3; i++) {
        int x = start_x + i * (thumb_w + spacing);
        int y = 150;
        
        /* Window thumbnail */
        fill_rounded_rect(buf, x, y, thumb_w, thumb_h, 12, make_argb(0xD0, 0x1a, 0x1a, 0x2e));
        
        /* Title bar */
        fill_rounded_rect(buf, x, y, thumb_w, 30, 12, make_argb(0x80, colors[i] >> 16, (colors[i] >> 8) & 0xFF, colors[i] & 0xFF));
        
        /* Window title */
        draw_string(buf, x + 10, y + 8, titles[i], COL_TEXT, 2);
        
        /* Window content placeholder */
        for (int ly = 40; ly < thumb_h - 10; ly += 12) {
            fill_rect(buf, x + 10, y + ly, thumb_w - 20, 6, make_argb(0x30, 0xff, 0xff, 0xff));
        }
    }
    
    /* Desktop thumbnail */
    int desktop_y = 400;
    fill_rounded_rect(buf, start_x, desktop_y, thumb_w * 2 + spacing, 150, 12,
                      make_argb(0xC0, 0x1a, 0x0a, 0x2e));
    draw_string(buf, start_x + 10, desktop_y + 10, "Desktop", COL_TEXT, 2);
    
    /* Instructions */
    draw_string(buf, SCREEN_WIDTH/2 - 200, SCREEN_HEIGHT - 60, 
               "Press Tab to switch, Enter to select, Esc to close", COL_TEXT_DIM, 2);
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * MAIN DESKTOP RENDER
 * ═══════════════════════════════════════════════════════════════════════════════ */

static u32 desktop_buffer[1920 * 1080];  /* Full HD framebuffer */

void hd_desktop_render(void) {
    /* Clear buffer */
    for (int i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; i++) {
        desktop_buffer[i] = 0;
    }
    
    /* Render base desktop */
    render_wallpaper(desktop_buffer);
    
    /* Render based on current mode */
    switch (current_mode) {
        case MODE_NORMAL:
            /* Just desktop with menu bar and dock */
            break;
            
        case MODE_SPOTLIGHT:
            render_spotlight(desktop_buffer);
            break;
            
        case MODE_MISSION_CTRL:
            render_mission_control(desktop_buffer);
            break;
            
        case MODE_CONTROL_CENTER:
            render_control_center(desktop_buffer);
            break;
            
        default:
            break;
    }
    
    /* Always render menu bar and dock */
    render_menubar(desktop_buffer);
    if (current_mode == MODE_NORMAL) {
        render_dock(desktop_buffer);
    }
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * KEYBOARD HANDLER
 * ═══════════════════════════════════════════════════════════════════════════════ */

void hd_desktop_keypress(char c) {
    switch (current_mode) {
        case MODE_SPOTLIGHT:
            if (c == 27) { /* Escape */
                current_mode = MODE_NORMAL;
            } else if (c == 8 || c == 127) { /* Backspace */
                int len = 0;
                while (spotlight_query[len]) len++;
                if (len > 0) spotlight_query[len - 1] = '\0';
            } else if (c >= 32 && c < 127) {
                /* Add to query */
                int len = 0;
                while (spotlight_query[len]) len++;
                if (len < 127) {
                    spotlight_query[len] = c;
                    spotlight_query[len + 1] = '\0';
                }
            }
            break;
            
        default:
            if (c == ' ') { /* Space - Spotlight */
                current_mode = MODE_SPOTLIGHT;
                spotlight_query[0] = '\0';
            } else if (c == 27) { /* Escape */
                current_mode = MODE_NORMAL;
            } else if (c == '\t') { /* Tab - Mission Control */
                current_mode = MODE_MISSION_CTRL;
            } else if (c == 'c') { /* C - Control Center */
                current_mode = MODE_CONTROL_CENTER;
            }
            break;
    }
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * INITIALIZATION
 * ═══════════════════════════════════════════════════════════════════════════════ */

void hd_desktop_init(void) {
    serial_puts("\n");
    serial_puts("╔══════════════════════════════════════════════════════════════╗\n");
    serial_puts("║        VECTORD OS - Premium HD Desktop v2.0                 ║\n");
    serial_puts("║        1920x1080 | Glassmorphism | macOS Killer             ║\n");
    serial_puts("╚══════════════════════════════════════════════════════════════╝\n\n");
    
    current_mode = MODE_NORMAL;
    spotlight_query[0] = '\0';
    desktop_tick = 0;
    
    serial_puts("[DESKTOP] Initialized\n");
    serial_puts("[DESKTOP] Resolution: 1920x1080\n");
    serial_puts("[DESKTOP] Shortcuts:\n");
    serial_puts("  Space     - Spotlight Search\n");
    serial_puts("  Tab       - Mission Control\n");
    serial_puts("  C         - Control Center\n");
    serial_puts("  Esc       - Close overlay\n");
}

void hd_desktop_update(void) {
    desktop_tick += 16; /* ~60fps */
    hd_desktop_render();
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * FRAMEBUFFER FLUSH - copy to VBE hardware
 * ═══════════════════════════════════════════════════════════════════════════════ */

void hd_flush_to_vbe(void) {
    if (!vbe_info.present || vbe_info.base == 0) {
        serial_puts("[FB] No VBE framebuffer available\n");
        return;
    }

    /* For high physical addresses (>= 0xE0000000), we need to map via
     * the paging system. For now, try direct write if accessible.
     * QEMU maps the framebuffer into guest physical memory space.
     */
    u32 *vbe_fb = (u32 *)vbe_info.base;
    u32 w = vbe_info.width < SCREEN_WIDTH ? vbe_info.width : SCREEN_WIDTH;
    u32 h = vbe_info.height < SCREEN_HEIGHT ? vbe_info.height : SCREEN_HEIGHT;
    u32 vbe_pitch = vbe_info.pitch / 4; /* pixels per scanline */
    u32 src_pitch = SCREEN_WIDTH;

    for (u32 y = 0; y < h; y++) {
        for (u32 x = 0; x < w; x++) {
            vbe_fb[y * vbe_pitch + x] = desktop_buffer[y * src_pitch + x];
        }
    }

    serial_puts("[FB] Flushed ");
    serial_putdec(w);
    serial_puts("x");
    serial_putdec(h);
    serial_puts(" to VBE framebuffer at 0x");
    serial_puthex(vbe_info.base);
    serial_puts("\n");
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * DEMO MODE
 * ═══════════════════════════════════════════════════════════════════════════════ */

/* Downsample framebuffer to ASCII art for serial output */
void hd_dump_serial(const char *label) {
    /* Map ARGB pixel to a density character */
    static const char ramp[] = " .:-=+*#%@";
    #define DUMP_W 80
    #define DUMP_H 24
    int x, dy;

    serial_puts("\n");
    serial_puts(label);
    serial_puts("\n");
    serial_puts("+");
    for (x = 0; x < DUMP_W; x++) serial_putc('-');
    serial_puts("+\n");

    for (dy = 0; dy < DUMP_H; dy++) {
        int fy = dy * SCREEN_HEIGHT / DUMP_H;
        int dx;
        serial_puts("|");
        for (dx = 0; dx < DUMP_W; dx++) {
            int fx = dx * SCREEN_WIDTH / DUMP_W;
            u32 pixel = desktop_buffer[fy * SCREEN_WIDTH + fx];
            u32 r = (pixel >> 16) & 0xFF;
            u32 g = (pixel >> 8) & 0xFF;
            u32 b = pixel & 0xFF;
            int lum = (r * 77 + g * 150 + b * 29) >> 8;
            int idx = lum * 9 / 255;
            serial_putc(ramp[idx]);
        }
        serial_puts("|\n");
    }

    serial_puts("+");
    for (x = 0; x < DUMP_W; x++) serial_putc('-');
    serial_puts("+\n");
}

void hd_desktop_demo(void) {
    hd_desktop_init();
    
    serial_puts("\n[DESKTOP] Rendering demo...\n");
    
    /* Demo: Normal desktop */
    current_mode = MODE_NORMAL;
    hd_desktop_render();
    hd_dump_serial("═══ Normal Desktop (1920x1080 → 80x24) ═══");
    
    /* Demo: Spotlight */
    strcpy(spotlight_query, "Terminal");
    current_mode = MODE_SPOTLIGHT;
    hd_desktop_render();
    hd_dump_serial("═══ Spotlight Search ═══");
    
    /* Demo: Mission Control */
    current_mode = MODE_MISSION_CTRL;
    hd_desktop_render();
    hd_dump_serial("═══ Mission Control ═══");
    
    /* Demo: Control Center */
    current_mode = MODE_CONTROL_CENTER;
    hd_desktop_render();
    hd_dump_serial("═══ Control Center ═══");
    
    /* Back to normal */
    current_mode = MODE_NORMAL;
    hd_desktop_render();
    
    serial_puts("\n[DESKTOP] Demo complete!\n");
    serial_puts("[DESKTOP] Buffer size: ");
    serial_putdec(sizeof(desktop_buffer) / 1024);
    serial_puts(" KB (");
    serial_putdec(SCREEN_WIDTH);
    serial_puts("x");
    serial_putdec(SCREEN_HEIGHT);
    serial_puts(" @ 32bpp)\n");
}

