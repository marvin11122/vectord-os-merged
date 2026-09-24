/* ═══════════════════════════════════════════════════════════════════════
 * VECTORD OS - Graphical Desktop (VGA Text Mode)
 * Provides a simple GUI with title bar, taskbar, icons, and clock
 * ═══════════════════════════════════════════════════════════════════════ */

#include "../include/vecord.h"

/* RTC time structure (same as kernel.c) */
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

/* Helper to make VGA color byte */
static u8 desktop_make_color(u8 fg, u8 bg) {
    return fg | (bg << 4);
}

/* ═══════════════════════════════════════════════════════════════════════
 * DESKTOP VARIABLES
 * ═══════════════════════════════════════════════════════════════════════ */

/* Title bar text */
#define TITLE_TEXT "VECTORD OS 3.0"

/* Taskbar button labels */
static const char *taskbar_buttons[] = {
    "Terminal",
    "Files",
    "System",
    "Calculator",
    NULL
};

/* Icon definitions */
typedef struct {
    int x;
    int y;
    const char *name;
    const char **ascii_art;
} icon_t;

/* Simple ASCII art for icons (3 lines each) */
static const char *icon_terminal[] = {
    " /\\ ",
    "/  \\",
    "\\__/"
};

static const char *icon_files[] = {
    " ____",
    "|____|",
    "|    |"
};

static const char *icon_system[] = {
    " ┌┐ ",
    " ││ ",
    " └┘ "
};

static const char *icon_calculator[] = {
    " ___ ",
    "|_ _|",
    "|___|"
};

static icon_t desktop_icons[] = {
    {2,  2, "Terminal",     icon_terminal},
    {12, 2, "Files",        icon_files},
    {22, 2, "System",       icon_system},
    {32, 2, "Calculator",   icon_calculator},
    {0,  0, NULL, NULL}
};

/* ═══════════════════════════════════════════════════════════════════════
 * HELPER FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════ */

static int desktop_strlen(const char *s) {
    int len = 0;
    while (s[len]) len++;
    return len;
}

static void desktop_itoa(int n, char *buf) {
    int i = 0;
    int neg = 0;
    if (n == 0) { buf[0] = '0'; buf[1] = '\0'; return; }
    if (n < 0) { neg = 1; n = -n; }
    while (n > 0) { buf[i++] = '0' + (n % 10); n /= 10; }
    if (neg) buf[i++] = '-';
    for (int j = 0; j < i / 2; j++) {
        char tmp = buf[j];
        buf[j] = buf[i - 1 - j];
        buf[i - 1 - j] = tmp;
    }
    buf[i] = '\0';
}

static void desktop_format_time(rtc_time_t *t, char *buf) {
    /* Format HH:MM:SS */
    int pos = 0;
    char tmp[4];
    
    desktop_itoa(t->hour, tmp);
    buf[pos++] = tmp[0]; buf[pos++] = tmp[1]; buf[pos++] = ':';
    desktop_itoa(t->minute, tmp);
    buf[pos++] = tmp[0]; buf[pos++] = tmp[1]; buf[pos++] = ':';
    desktop_itoa(t->second, tmp);
    buf[pos++] = tmp[0]; buf[pos++] = tmp[1];
    buf[pos] = '\0';
}

/* ═══════════════════════════════════════════════════════════════════════
 * CORE DESKTOP FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════ */

void desktop_clear_screen(u8 color) {
    vga_clear(color);
}

void desktop_draw_box(int x, int y, int w, int h, u8 color) {
    /* Draw top border */
    vga_putchar(x, y, '+', color);
    for (int i = 1; i < w - 1; i++) vga_putchar(x + i, y, '-', color);
    vga_putchar(x + w - 1, y, '+', color);
    
    /* Draw sides */
    for (int j = 1; j < h - 1; j++) {
        vga_putchar(x, y + j, '|', color);
        for (int i = 1; i < w - 1; i++) vga_putchar(x + i, y + j, ' ', color);
        vga_putchar(x + w - 1, y + j, '|', color);
    }
    
    /* Draw bottom border */
    vga_putchar(x, y + h - 1, '+', color);
    for (int i = 1; i < w - 1; i++) vga_putchar(x + i, y + h - 1, '-', color);
    vga_putchar(x + w - 1, y + h - 1, '+', color);
}

void desktop_draw_titlebar(void) {
    u8 color = desktop_make_color(VGA_WHITE, VGA_BLUE);
    
    /* Fill title bar row with blue background */
    for (int i = 0; i < VGA_WIDTH; i++) {
        vga_putchar(i, 0, ' ', color);
    }
    
    /* Draw title text */
    vga_print(1, 0, TITLE_TEXT, color);
    
    /* Draw time on right */
    rtc_time_t t = rtc_read_time();
    char time_str[9]; /* HH:MM:SS + null */
    desktop_format_time(&t, time_str);
    int time_len = desktop_strlen(time_str);
    vga_print(VGA_WIDTH - time_len - 1, 0, time_str, color);
}

void desktop_draw_taskbar(void) {
    u8 color = desktop_make_color(VGA_BLACK, VGA_LIGHT_GRAY);
    int taskbar_y = VGA_HEIGHT - 1; /* Last row */
    
    /* Fill taskbar row */
    for (int i = 0; i < VGA_WIDTH; i++) {
        vga_putchar(i, taskbar_y, ' ', color);
    }
    
    /* Start menu button */
    vga_print(0, taskbar_y, "[Start]", desktop_make_color(VGA_WHITE, VGA_BLUE));
    
    /* Application buttons */
    int x_pos = 10;
    for (int i = 0; taskbar_buttons[i] != NULL; i++) {
        vga_putchar(x_pos, taskbar_y, '[', color);
        vga_print(x_pos + 1, taskbar_y, taskbar_buttons[i], color);
        int len = desktop_strlen(taskbar_buttons[i]);
        vga_putchar(x_pos + len + 1, taskbar_y, ']', color);
        x_pos += len + 3;
    }
}

void desktop_draw_icon(int x, int y, const char *name) {
    u8 color = desktop_make_color(VGA_WHITE, VGA_BLACK);
    
    /* Find icon by name (simple linear search) */
    const char **art = NULL;
    for (int i = 0; desktop_icons[i].name != NULL; i++) {
        if (strcmp(desktop_icons[i].name, name) == 0) {
            art = desktop_icons[i].ascii_art;
            break;
        }
    }
    
    if (!art) return;
    
    /* Draw 3 lines of ASCII art */
    for (int i = 0; i < 3; i++) {
        vga_print(x, y + i, art[i], color);
    }
    
    /* Draw label below */
    vga_print(x, y + 3, name, desktop_make_color(VGA_YELLOW, VGA_BLACK));
}

void desktop_init(void) {
    /* Clear screen with black */
    desktop_clear_screen(VGA_BLACK);
    
    /* Draw static elements */
    desktop_draw_titlebar();
    desktop_draw_taskbar();
    
    /* Draw desktop icons */
    for (int i = 0; desktop_icons[i].name != NULL; i++) {
        desktop_draw_icon(desktop_icons[i].x, desktop_icons[i].y, desktop_icons[i].name);
    }
    
    /* Hide cursor */
    vga_set_cursor(0, 0);
}

/* ═══════════════════════════════════════════════════════════════════════
 * INTERACTIVE DESKTOP MAIN LOOP
 * ═══════════════════════════════════════════════════════════════════════ */

void desktop_main_loop(void) {
    char key;
    
    while (1) {
        /* Update clock each iteration */
        desktop_draw_titlebar();
        
        /* Check for serial input */
        int c = serial_getchar();
        if (c == -1) {
            /* No input, short delay */
            asm volatile ("hlt");
            continue;
        }
        
        key = (char)c;
        
        if (key == '1') {
            /* Open Terminal */
            vga_clear(VGA_BLACK);
            vga_print(0, 0, "Terminal Application (simulated)", desktop_make_color(VGA_WHITE, VGA_BLACK));
            vga_print(0, 1, "Press any key to return...", desktop_make_color(VGA_LIGHT_GRAY, VGA_BLACK));
            while (serial_getchar() == -1) asm volatile ("hlt");
            desktop_init();
        }
        else if (key == '2') {
            /* Open File Manager */
            vga_clear(VGA_BLACK);
            vga_print(0, 0, "File Manager (simulated)", desktop_make_color(VGA_WHITE, VGA_BLACK));
            vga_print(0, 1, "Press any key to return...", desktop_make_color(VGA_LIGHT_GRAY, VGA_BLACK));
            while (serial_getchar() == -1) asm volatile ("hlt");
            desktop_init();
        }
        else if (key == '3') {
            /* Open System Info */
            vga_clear(VGA_BLACK);
            vga_print(0, 0, "System Information (simulated)", desktop_make_color(VGA_WHITE, VGA_BLACK));
            vga_print(0, 1, "VECTORD OS 3.0 - VGA Text Mode 80x25", desktop_make_color(VGA_LIGHT_GRAY, VGA_BLACK));
            vga_print(0, 2, "Press any key to return...", desktop_make_color(VGA_LIGHT_GRAY, VGA_BLACK));
            while (serial_getchar() == -1) asm volatile ("hlt");
            desktop_init();
        }
        else if (key == '4') {
            /* Open Calculator */
            vga_clear(VGA_BLACK);
            vga_print(0, 0, "Calculator (simulated)", desktop_make_color(VGA_WHITE, VGA_BLACK));
            vga_print(0, 1, "Press any key to return...", desktop_make_color(VGA_LIGHT_GRAY, VGA_BLACK));
            while (serial_getchar() == -1) asm volatile ("hlt");
            desktop_init();
        }
        else if (key == 'q' || key == 'Q') {
            /* Quit to shell */
            break;
        }
    }
}
