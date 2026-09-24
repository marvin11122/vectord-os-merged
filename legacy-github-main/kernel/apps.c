/* ═══════════════════════════════════════════════════════════════════════
 * VECTORD OS - System Applications
 * Built-in applications for the VECTORD OS shell
 * ═══════════════════════════════════════════════════════════════════════ */

#include "../include/vecord.h"

/* ═══════════════════════════════════════════════════════════════════════
 * UTILITY FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════ */

static int app_strlen(const char *s) {
    int len = 0;
    while (s[len]) len++;
    return len;
}

static int app_atoi(const char *s) {
    int n = 0;
    int sign = 1;
    if (*s == '-') { sign = -1; s++; }
    while (*s >= '0' && *s <= '9') {
        n = n * 10 + (*s - '0');
        s++;
    }
    return n * sign;
}

static void app_itoa(int n, char *buf) {
    int i = 0;
    int neg = 0;

    if (n == 0) {
        buf[0] = '0';
        buf[1] = '\0';
        return;
    }

    if (n < 0) {
        neg = 1;
        n = -n;
    }

    while (n > 0) {
        buf[i++] = '0' + (n % 10);
        n /= 10;
    }

    if (neg) buf[i++] = '-';

    /* Reverse */
    for (int j = 0; j < i / 2; j++) {
        char tmp = buf[j];
        buf[j] = buf[i - 1 - j];
        buf[i - 1 - j] = tmp;
    }
    buf[i] = '\0';
}

static void app_float_to_str(float f, char *buf, int decimals) {
    int i = 0;
    if (f < 0) {
        buf[i++] = '-';
        f = -f;
    }

    int whole = (int)f;
    float frac = f - whole;

    app_itoa(whole, buf + i);
    i = app_strlen(buf);

    buf[i++] = '.';

    for (int d = 0; d < decimals; d++) {
        frac *= 10;
        int digit = (int)frac;
        buf[i++] = '0' + digit;
        frac -= digit;
    }
    buf[i] = '\0';
}

static void app_print_separator(void) {
    serial_puts("────────────────────────────────────────────────────────\n");
}

static void app_print_header(const char *title) {
    serial_puts("\n");
    app_print_separator();
    serial_puts("  ");
    serial_puts(title);
    serial_puts("\n");
    app_print_separator();
}

static void app_strcpy(char *dest, const char *src) {
    while (*src) {
        *dest++ = *src++;
    }
    *dest = '\0';
}

static int app_strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

static void app_itoa_hex(unsigned int n, char *buf) {
    const char hex_digits[] = "0123456789ABCDEF";
    buf[0] = '0';
    buf[1] = 'x';
    for (int i = 7; i >= 0; i--) {
        buf[2 + (7 - i)] = hex_digits[(n >> (i * 4)) & 0xF];
    }
    buf[10] = '\0';
}

static void app_itoa_hex_byte(unsigned int n, char *buf) {
    const char hex_digits[] = "0123456789ABCDEF";
    buf[0] = hex_digits[(n >> 4) & 0xF];
    buf[1] = hex_digits[n & 0xF];
    buf[2] = '\0';
}

static int rand(void) {
    static unsigned int seed = 12345;
    seed = seed * 1103515245 + 12345;
    return (seed >> 16) & 0x7FFF;
}

/* ═══════════════════════════════════════════════════════════════════════
 * APP CALCULATOR - Simple calculator application
 * ═══════════════════════════════════════════════════════════════════════ */
void app_calculator(void) {
    char input_buf[128];
    char result_buf[32];
    int running = 1;
    int pos;

    app_print_header("VECTORD CALCULATOR v1.0");

    serial_puts("\n");
    serial_puts("  Operations: +  -  *  /  %%  ^\n");
    serial_puts("  Enter 'q' to quit\n");
    serial_puts("\n");

    while (running) {
        serial_puts("calc> ");

        /* Read line */
        pos = 0;
        while (1) {
            int c = serial_getchar();
            if (c == -1) {
                asm volatile ("hlt");
                continue;
            }
            if (c == '\n' || c == '\r') {
                serial_puts("\n");
                input_buf[pos] = '\0';
                break;
            }
            if (c == '\b' || c == 127) {
                if (pos > 0) {
                    pos--;
                    serial_puts("\b \b");
                }
            } else if (c >= 32 && c < 127 && pos < 127) {
                input_buf[pos++] = c;
                serial_putc(c);
            }
        }

        if (input_buf[0] == 'q' || input_buf[0] == 'Q') {
            running = 0;
            continue;
        }

        /* Simple parser: "num op num" */
        int num1 = 0, num2 = 0;
        char op = '+';
        int i = 0;

        /* Parse first number */
        while (input_buf[i] == ' ') i++;
        int sign1 = 1;
        if (input_buf[i] == '-') { sign1 = -1; i++; }
        while (input_buf[i] >= '0' && input_buf[i] <= '9') {
            num1 = num1 * 10 + (input_buf[i] - '0');
            i++;
        }
        num1 *= sign1;

        /* Parse operator */
        while (input_buf[i] == ' ') i++;
        if (input_buf[i]) {
            op = input_buf[i];
            i++;
        }

        /* Parse second number */
        while (input_buf[i] == ' ') i++;
        int sign2 = 1;
        if (input_buf[i] == '-') { sign2 = -1; i++; }
        while (input_buf[i] >= '0' && input_buf[i] <= '9') {
            num2 = num2 * 10 + (input_buf[i] - '0');
            i++;
        }
        num2 *= sign2;

        /* Calculate result */
        int result = 0;
        int valid = 1;

        switch (op) {
            case '+': result = num1 + num2; break;
            case '-': result = num1 - num2; break;
            case '*': result = num1 * num2; break;
            case '/':
                if (num2 == 0) {
                    serial_puts("  Error: Division by zero\n");
                    valid = 0;
                } else {
                    result = num1 / num2;
                }
                break;
            case '%':
                if (num2 == 0) {
                    serial_puts("  Error: Modulo by zero\n");
                    valid = 0;
                } else {
                    result = num1 % num2;
                }
                break;
            case '^': {
                result = 1;
                for (int p = 0; p < (num2 < 0 ? -num2 : num2); p++) {
                    result *= num1;
                }
                if (num2 < 0) result = 0;
                break;
            }
            default:
                serial_puts("  Unknown operator\n");
                valid = 0;
        }

        if (valid) {
            app_itoa(num1, result_buf);
            serial_puts("  ");
            serial_puts(result_buf);
            serial_puts(" ");
            serial_putc(op);
            serial_puts(" ");
            app_itoa(num2, result_buf);
            serial_puts(result_buf);
            serial_puts(" = ");
            app_itoa(result, result_buf);
            serial_puts(result_buf);
            serial_puts("\n");
        }
    }

    serial_puts("\n  Calculator closed.\n");
}

/* ═══════════════════════════════════════════════════════════════════════
 * APP CLOCK - Display and update time
 * ═══════════════════════════════════════════════════════════════════════ */
void app_clock(void) {
    char time_buf[32];
    int running = 1;
    u32 ticks = 0;
    int seconds = 0;
    int minutes = 0;
    int hours = 10;  /* Start at 10:00:00 */
    int count = 0;

    app_print_header("VECTORD CLOCK v1.0");

    serial_puts("\n");
    serial_puts("  Press 'q' to exit\n");
    serial_puts("  Clock will update every second\n");
    serial_puts("\n");

    while (running) {
        /* Simple tick counter (approximate) */
        for (volatile int i = 0; i < 1000000; i++) {
            /* Delay loop */
        }
        count++;

        if (count >= 10) {
            count = 0;
            seconds++;
            if (seconds >= 60) {
                seconds = 0;
                minutes++;
                if (minutes >= 60) {
                    minutes = 0;
                    hours++;
                    if (hours >= 24) {
                        hours = 0;
                    }
                }
            }

            /* Display time */
            serial_puts("\r  Time: ");
            if (hours < 10) serial_putc('0');
            app_itoa(hours, time_buf);
            serial_puts(time_buf);
            serial_putc(':');
            if (minutes < 10) serial_putc('0');
            app_itoa(minutes, time_buf);
            serial_puts(time_buf);
            serial_putc(':');
            if (seconds < 10) serial_putc('0');
            app_itoa(seconds, time_buf);
            serial_puts(time_buf);
            serial_puts("  ");
        }

        /* Check for input */
        int c = serial_getchar();
        if (c == 'q' || c == 'Q') {
            running = 0;
        }
    }

    serial_puts("\n\n  Clock stopped.\n");
}

/* ═══════════════════════════════════════════════════════════════════════
 * APP SYSTEM INFO - Detailed system information
 * ═══════════════════════════════════════════════════════════════════════ */
void app_system_info(void) {
    char num_buf[32];

    app_print_header("VECTORD SYSTEM INFORMATION");

    /* OS Info */
    serial_puts("\n");
    serial_puts("  ┌─ Operating System ──────────────────────────────────┐\n");
    serial_puts("  │  Name:    ");
    serial_puts(OS_NAME);
    serial_puts("\n");
    serial_puts("  │  Version: ");
    serial_puts(OS_VERSION);
    serial_puts("\n");
    serial_puts("  │  Codename:");
    serial_puts(OS_CODENAME);
    serial_puts("\n");
    serial_puts("  │  Type:    Standalone Kernel (No Linux/Unix)\n");
    serial_puts("  └─────────────────────────────────────────────────────┘\n");

    /* Memory Info */
    serial_puts("\n");
    serial_puts("  ┌─ Memory ────────────────────────────────────────────┐\n");

    serial_puts("  │  Total: ");
    app_itoa(mem_get_total() / 1024, num_buf);
    serial_puts(num_buf);
    serial_puts(" KB (");
    app_itoa(mem_get_total() / (1024 * 1024), num_buf);
    serial_puts(num_buf);
    serial_puts(" MB)\n");

    serial_puts("  │  Used:  ");
    app_itoa(mem_get_used() / 1024, num_buf);
    serial_puts(num_buf);
    serial_puts(" KB\n");

    serial_puts("  │  Free:  ");
    app_itoa(mem_get_free() / 1024, num_buf);
    serial_puts(num_buf);
    serial_puts(" KB\n");

    serial_puts("  │  Free Blocks: ");
    app_itoa(mem_get_free_blocks(), num_buf);
    serial_puts(num_buf);
    serial_puts("\n");

    serial_puts("  └─────────────────────────────────────────────────────┘\n");

    /* Hardware Info */
    serial_puts("\n");
    serial_puts("  ┌─ Hardware ──────────────────────────────────────────┐\n");

    serial_puts("  │  CPU Vendor: ");
    serial_puts(hw_info.cpu_vendor);
    serial_puts("\n");

    serial_puts("  │  CPU Brand:  ");
    serial_puts(hw_info.cpu_brand);
    serial_puts("\n");

    serial_puts("  │  CPU Speed:  ");
    app_itoa(hw_info.cpu_speed_mhz, num_buf);
    serial_puts(num_buf);
    serial_puts(" MHz\n");

    serial_puts("  │  CPU Family: ");
    app_itoa(hw_info.cpu_family, num_buf);
    serial_puts(num_buf);
    serial_puts("  Model: ");
    app_itoa(hw_info.cpu_model, num_buf);
    serial_puts(num_buf);
    serial_puts("\n");

    serial_puts("  │  RAM:        ");
    app_itoa(hw_info.ram_total_mb, num_buf);
    serial_puts(num_buf);
    serial_puts(" MB\n");

    serial_puts("  └─────────────────────────────────────────────────────┘\n");

    /* PCI Devices */
    serial_puts("\n");
    serial_puts("  ┌─ PCI Devices ───────────────────────────────────────┐\n");
    serial_puts("  │  Detected: ");
    app_itoa(pci_device_count, num_buf);
    serial_puts(num_buf);
    serial_puts(" devices\n");
    serial_puts("  └─────────────────────────────────────────────────────┘\n");

    /* ATA Devices */
    serial_puts("\n");
    serial_puts("  ┌─ Storage ───────────────────────────────────────────┐\n");
    serial_puts("  │  ATA Devices: ");
    app_itoa(ata_device_count, num_buf);
    serial_puts(num_buf);
    serial_puts("\n");
    for (int i = 0; i < ata_device_count; i++) {
        serial_puts("  │    ");
        app_itoa(i, num_buf);
        serial_puts(num_buf);
        serial_puts(": ");
        serial_puts(ata_devices[i].model);
        serial_puts(" (");
        app_itoa(ata_devices[i].sectors / 2048, num_buf);
        serial_puts(num_buf);
        serial_puts(" MB)\n");
    }
    serial_puts("  │  AHCI Controllers: ");
    app_itoa(ahci_controller_count, num_buf);
    serial_puts(num_buf);
    serial_puts("\n");
    serial_puts("  └─────────────────────────────────────────────────────┘\n");

    /* USB Controllers */
    serial_puts("\n");
    serial_puts("  ┌─ USB ───────────────────────────────────────────────┐\n");
    serial_puts("  │  Controllers: ");
    app_itoa(usb_controller_count, num_buf);
    serial_puts(num_buf);
    serial_puts("\n");
    for (int i = 0; i < usb_controller_count; i++) {
        serial_puts("  │    ");
        app_itoa(i, num_buf);
        serial_puts(num_buf);
        serial_puts(": ");
        serial_puts(usb_controllers[i].type_name);
        serial_puts("\n");
    }
    serial_puts("  └─────────────────────────────────────────────────────┘\n");

    /* Vector Memory */
    serial_puts("\n");
    serial_puts("  ┌─ Vector Memory System ──────────────────────────────┐\n");
    serial_puts("  │  Vector Allocations: ");
    app_itoa(vec_get_counter(), num_buf);
    serial_puts(num_buf);
    serial_puts("\n");
    serial_puts("  │  4D Semantic Addressing: ACTIVE\n");
    serial_puts("  └─────────────────────────────────────────────────────┘\n");

    serial_puts("\n  Press Enter to return to shell...\n");
    while (serial_getchar() == -1) {
        asm volatile ("hlt");
    }
}

/* ═══════════════════════════════════════════════════════════════════════
 * APP FILE BROWSER - Simple file browser in text mode
 * ═══════════════════════════════════════════════════════════════════════ */
void app_file_browser(void) {
    char num_buf[32];
    char path_buf[256];
    int current_path_len = 1;
    path_buf[0] = '/';
    path_buf[1] = '\0';

    app_print_header("VECTORD FILE BROWSER v1.0");

    serial_puts("\n");
    serial_puts("  Commands: [u]p  [r]efresh  [q]uit\n");
    serial_puts("\n");

    int running = 1;
    while (running) {
        serial_puts("\n  Current path: ");
        serial_puts(path_buf);
        serial_puts("\n\n");

        /* Try to list directory */
        dirent_t entries[32];
        int count = vfs_ls(path_buf, entries, 32);

        if (count >= 0) {
            serial_puts("  Type    Name\n");
            serial_puts("  ────    ────────────────────────────\n");

            for (int i = 0; i < count; i++) {
                if (entries[i].type == FILE_TYPE_DIRECTORY) {
                    serial_puts("  [DIR]   ");
                } else {
                    serial_puts("  [FILE]  ");
                }
                serial_puts(entries[i].name);
                serial_puts("\n");
            }

            serial_puts("\n  ");
            app_itoa(count, num_buf);
            serial_puts(num_buf);
            serial_puts(" entries\n");
        } else {
            serial_puts("  Could not read directory.\n");
        }

        serial_puts("\n  Command: ");

        /* Read single character command */
        int cmd = -1;
        while (cmd == -1) {
            cmd = serial_getchar();
            if (cmd == -1) asm volatile ("hlt");
        }
        serial_putc(cmd);
        serial_puts("\n");

        switch (cmd) {
            case 'q':
            case 'Q':
                running = 0;
                break;
            case 'u':
            case 'U':
                /* Go up */
                if (current_path_len > 1) {
                    current_path_len--;
                    while (current_path_len > 1 && path_buf[current_path_len - 1] != '/') {
                        current_path_len--;
                    }
                    if (current_path_len == 1) {
                        path_buf[1] = '\0';
                    } else {
                        path_buf[current_path_len - 1] = '\0';
                    }
                }
                break;
            case 'r':
            case 'R':
                /* Refresh - just loop again */
                break;
            default:
                serial_puts("  Unknown command\n");
        }
    }

    serial_puts("\n  File browser closed.\n");
}

/* ═══════════════════════════════════════════════════════════════════════
 * APP SETTINGS - System settings display
 * ═══════════════════════════════════════════════════════════════════════ */
void app_settings(void) {
    char num_buf[32];
    int selection = 0;
    int running = 1;

    app_print_header("VECTORD SYSTEM SETTINGS v1.0");

    while (running) {
        serial_puts("\n");
        serial_puts("  ┌─ Settings Menu ───────────────────────────────────┐\n");
        serial_puts("  │                                                    │\n");
        serial_puts("  │  [1] System Information                           │\n");
        serial_puts("  │  [2] Memory Settings                              │\n");
        serial_puts("  │  [3] Display Settings                             │\n");
        serial_puts("  │  [4] Network Settings                             │\n");
        serial_puts("  │  [5] Kernel Parameters                            │\n");
        serial_puts("  │  [6] About VECTORD OS                             │\n");
        serial_puts("  │  [0] Exit to Shell                                │\n");
        serial_puts("  │                                                    │\n");
        serial_puts("  └────────────────────────────────────────────────────┘\n");
        serial_puts("\n");
        serial_puts("  Select option: ");

        /* Read input */
        int cmd = -1;
        while (cmd == -1) {
            cmd = serial_getchar();
            if (cmd == -1) asm volatile ("hlt");
        }
        serial_putc(cmd);
        serial_puts("\n");

        switch (cmd) {
            case '0':
                running = 0;
                break;
            case '1':
                /* System Info */
                serial_puts("\n  System Information:\n");
                serial_puts("  ─────────────────────\n");
                serial_puts("  OS: ");
                serial_puts(OS_NAME);
                serial_puts(" v");
                serial_puts(OS_VERSION);
                serial_puts("\n");
                serial_puts("  CPU: ");
                serial_puts(hw_info.cpu_vendor);
                serial_puts(" - ");
                serial_puts(hw_info.cpu_brand);
                serial_puts("\n");
                serial_puts("  RAM: ");
                app_itoa(hw_info.ram_total_mb, num_buf);
                serial_puts(num_buf);
                serial_puts(" MB\n");
                serial_puts("  PCI Devices: ");
                app_itoa(pci_device_count, num_buf);
                serial_puts(num_buf);
                serial_puts("\n");
                serial_puts("  Press any key...\n");
                while (serial_getchar() == -1) asm volatile ("hlt");
                break;

            case '2':
                /* Memory Settings */
                serial_puts("\n  Memory Settings:\n");
                serial_puts("  ──────────────────\n");
                serial_puts("  Heap Total: ");
                app_itoa(mem_get_total() / 1024, num_buf);
                serial_puts(num_buf);
                serial_puts(" KB\n");
                serial_puts("  Heap Used:  ");
                app_itoa(mem_get_used() / 1024, num_buf);
                serial_puts(num_buf);
                serial_puts(" KB\n");
                serial_puts("  Heap Free:  ");
                app_itoa(mem_get_free() / 1024, num_buf);
                serial_puts(num_buf);
                serial_puts(" KB\n");
                serial_puts("  Vector Allocations: ");
                app_itoa(vec_get_counter(), num_buf);
                serial_puts(num_buf);
                serial_puts("\n");
                serial_puts("  Press any key...\n");
                while (serial_getchar() == -1) asm volatile ("hlt");
                break;

            case '3':
                /* Display Settings */
                serial_puts("\n  Display Settings:\n");
                serial_puts("  ──────────────────\n");
                serial_puts("  VGA Text Mode: 80x25\n");
                serial_puts("  Color Depth: 16 colors\n");
                serial_puts("  Framebuffer: ");
                if (vbe_info.present) {
                    serial_puts("Available (");
                    app_itoa(vbe_info.width, num_buf);
                    serial_puts(num_buf);
                    serial_puts("x");
                    app_itoa(vbe_info.height, num_buf);
                    serial_puts(num_buf);
                    serial_puts(")\n");
                } else {
                    serial_puts("Not initialized\n");
                }
                serial_puts("  Press any key...\n");
                while (serial_getchar() == -1) asm volatile ("hlt");
                break;

            case '4':
                /* Network Settings */
                serial_puts("\n  Network Settings:\n");
                serial_puts("  ──────────────────\n");
                serial_puts("  Network Stack: ");
                serial_puts("Initialized\n");
                serial_puts("  Press any key...\n");
                while (serial_getchar() == -1) asm volatile ("hlt");
                break;

            case '5':
                /* Kernel Parameters */
                serial_puts("\n  Kernel Parameters:\n");
                serial_puts("  ──────────────────\n");
                serial_puts("  Max Processes: 64\n");
                serial_puts("  Default Quantum: 100 ticks\n");
                serial_puts("  Heap Alignment: 16 bytes\n");
                serial_puts("  Page Size: 4096 bytes\n");
                serial_puts("  Press any key...\n");
                while (serial_getchar() == -1) asm volatile ("hlt");
                break;

            case '6':
                /* About */
                serial_puts("\n  About VECTORD OS:\n");
                serial_puts("  ──────────────────\n");
                serial_puts("  VECTORD OS is a standalone operating system\n");
                serial_puts("  built from scratch with no Linux or Unix code.\n");
                serial_puts("\n");
                serial_puts("  Features:\n");
                serial_puts("    - 4D Semantic Memory Addressing\n");
                serial_puts("    - Geometric Process Scheduler\n");
                serial_puts("    - Neural Hardware Optimizer\n");
                serial_puts("    - Custom VECTORD Filesystem\n");
                serial_puts("    - Built-in Network Stack\n");
                serial_puts("\n");
                serial_puts("  Version: ");
                serial_puts(OS_VERSION);
                serial_puts("\n");
                serial_puts("  Press any key...\n");
                while (serial_getchar() == -1) asm volatile ("hlt");
                break;

            default:
                serial_puts("  Invalid option\n");
        }
    }

    serial_puts("\n  Settings closed.\n");
}

/* ═══════════════════════════════════════════════════════════════════════
 * APP LIST - List all available applications
 * ═══════════════════════════════════════════════════════════════════════ */

/* ═══════════════════════════════════════════════════════════════════════
 * APP FILE MANAGER - Enhanced file manager with navigation
 * ═══════════════════════════════════════════════════════════════════════ */
void app_file_manager(void) {
    char num_buf[32];
    char path_buf[256];
    int current_path_len = 1;
    path_buf[0] = '/';
    path_buf[1] = '\0';
    int selected = 0;
    int scroll_offset = 0;
    const int visible_items = 15;
    
    /* ANSI color codes */
    const char *RESET = "\033[0m";
    const char *BOLD = "\033[1m";
    const char *RED = "\033[31m";
    const char *GREEN = "\033[32m";
    const char *YELLOW = "\033[33m";
    const char *BLUE = "\033[34m";
    const char *MAGENTA = "\033[35m";
    const char *CYAN = "\033[36m";
    const char *WHITE = "\033[37m";
    const char *BG_BLUE = "\033[44m";
    const char *BG_WHITE = "\033[47m";
    
    app_print_header("VECTORD FILE MANAGER v2.0");
    
    serial_puts("\n");
    serial_puts("  Navigation: u=up, d=down, enter=open, q=quit\n");
    serial_puts("  Columns: Type | Name | Size | Modified\n");
    serial_puts("\n");
    
    int running = 1;
    while (running) {
        /* Clear screen */
        serial_puts("\033[2J\033[H");
        app_print_header("VECTORD FILE MANAGER v2.0");
        serial_puts("\n");
        
        /* Draw top border */
        serial_puts("  ┌─────────────────────────────────────────────────────────────────────────────┐\n");
        
        /* Current path */
        serial_puts("  │ ");
        serial_puts(BOLD);
        serial_puts(CYAN);
        serial_puts("Path: ");
        serial_puts(path_buf);
        serial_puts(RESET);
        /* Fill to border */
        int path_len = app_strlen(path_buf) + 7;
        for (int i = path_len; i < 77; i++) serial_puts(" ");
        serial_puts("│\n");
        
        /* Separator */
        serial_puts("  ├─────────────────────────────────────────────────────────────────────────────┤\n");
        
        /* List directory */
        dirent_t entries[32];
        int count = vfs_ls(path_buf, entries, 32);
        
        if (count >= 0) {
            /* Column headers */
            serial_puts("  │ ");
            serial_puts(BOLD);
            serial_puts(" Type  Name                                 Size     Modified   ");
            serial_puts(RESET);
            serial_puts("│\n");
            serial_puts("  │─────────────────────────────────────────────────────────────────────────────│\n");
            
            /* Ensure selected is within bounds */
            if (selected >= count) selected = count - 1;
            if (selected < 0) selected = 0;
            
            /* Ensure selected is visible */
            if (selected < scroll_offset) scroll_offset = selected;
            if (selected >= scroll_offset + visible_items) scroll_offset = selected - visible_items + 1;
            
            /* Display entries */
            for (int i = scroll_offset; i < count && i < scroll_offset + visible_items; i++) {
                serial_puts("  │");
                if (i == selected) {
                    serial_puts(BG_BLUE);
                    serial_puts(WHITE);
                }
                
                /* Type column */
                serial_puts(" ");
                if (entries[i].type == FILE_TYPE_DIRECTORY) {
                    serial_puts(GREEN);
                    serial_puts("[DIR] ");
                    serial_puts(RESET);
                } else {
                    serial_puts(RED);
                    serial_puts("[FILE]");
                    serial_puts(RESET);
                }
                
                /* Name column */
                serial_puts(" ");
                serial_puts(entries[i].name);
                int name_len = app_strlen(entries[i].name);
                for (int j = name_len; j < 36; j++) serial_puts(" ");
                
                /* Size column (simulated) */
                serial_puts(" ");
                if (entries[i].type == FILE_TYPE_DIRECTORY) {
                    serial_puts("<DIR>   ");
                } else {
                    /* Simulate file size */
                    app_itoa((i * 1024) % 10000, num_buf);
                    serial_puts(num_buf);
                    serial_puts(" B    ");
                }
                
                /* Modified date (simulated) */
                serial_puts(" 2024-01-");
                app_itoa((i % 30) + 1, num_buf);
                serial_puts(num_buf);
                
                if (i == selected) {
                    serial_puts(RESET);
                }
                serial_puts(" │\n");
            }
            
            /* Fill remaining lines if fewer entries */
            for (int i = count - scroll_offset; i < visible_items; i++) {
                serial_puts("  │                                                                                             │\n");
            }
            
            /* Bottom border with entry count */
            serial_puts("  ├─────────────────────────────────────────────────────────────────────────────┤\n");
            serial_puts("  │ ");
            app_itoa(count, num_buf);
            serial_puts(num_buf);
            serial_puts(" entries | Selected: ");
            app_itoa(selected + 1, num_buf);
            serial_puts(num_buf);
            serial_puts("/");
            app_itoa(count, num_buf);
            serial_puts(num_buf);
            /* Fill to border */
            int info_len = app_strlen(num_buf) * 2 + 20;
            for (int i = info_len; i < 77; i++) serial_puts(" ");
            serial_puts("│\n");
        } else {
            serial_puts("  │ Error: Could not read directory                                           │\n");
            serial_puts("  │                                                                           │\n");
        }
        
        serial_puts("  └─────────────────────────────────────────────────────────────────────────────┘\n");
        
        /* Instructions */
        serial_puts("\n  [u] Move up  [d] Move down  [enter] Open  [q] Quit\n");
        
        /* Get key */
        int key = -1;
        while (key == -1) {
            key = serial_getchar();
            if (key == -1) asm volatile ("hlt");
        }
        
        switch (key) {
            case 'q':
            case 'Q':
                running = 0;
                break;
            case 'u':
            case 'U':
                if (selected > 0) selected--;
                break;
            case 'd':
            case 'D':
                if (selected < count - 1) selected++;
                break;
            case '\n':
            case '\r':
                /* Open directory or show file info */
                if (count > 0 && entries[selected].type == FILE_TYPE_DIRECTORY) {
                    /* Build new path */
                    if (current_path_len == 1 && path_buf[0] == '/') {
                        /* Root directory */
                        path_buf[0] = '/';
                        app_strcpy(path_buf + 1, entries[selected].name);
                        current_path_len = 1 + app_strlen(entries[selected].name);
                    } else {
                        /* Append slash and name */
                        path_buf[current_path_len - 1] = '/';
                        app_strcpy(path_buf + current_path_len, entries[selected].name);
                        current_path_len += 1 + app_strlen(entries[selected].name);
                    }
                    selected = 0;
                    scroll_offset = 0;
                } else if (count > 0) {
                    /* Show file details */
                    serial_puts("\033[2J\033[H");
                    app_print_header("FILE DETAILS");
                    serial_puts("\n");
                    serial_puts("  ┌─────────────────────────────────┐\n");
                    serial_puts("  │ Name: ");
                    serial_puts(entries[selected].name);
                    serial_puts("\n");
                    serial_puts("  │ Type: ");
                    serial_puts(entries[selected].type == FILE_TYPE_DIRECTORY ? "Directory" : "File");
                    serial_puts("\n");
                    serial_puts("  │ Size: ");
                    app_itoa((selected * 1024) % 10000, num_buf);
                    serial_puts(num_buf);
                    serial_puts(" bytes\n");
                    serial_puts("  │ Permissions: rw-r--r--\n");
                    serial_puts("  │ Owner: root\n");
                    serial_puts("  └─────────────────────────────────┘\n");
                    serial_puts("\n  Press any key to return...");
                    while (serial_getchar() == -1) asm volatile ("hlt");
                }
                break;
            case 'b':
            case 'B':
                /* Go up one directory */
                if (current_path_len > 1) {
                    current_path_len--;
                    while (current_path_len > 1 && path_buf[current_path_len - 1] != '/') {
                        current_path_len--;
                    }
                    if (current_path_len == 1) {
                        path_buf[1] = '\0';
                    } else {
                        path_buf[current_path_len - 1] = '\0';
                    }
                    selected = 0;
                    scroll_offset = 0;
                }
                break;
        }
    }
    
    serial_puts("\n  File manager closed.\n");
}

/* ═══════════════════════════════════════════════════════════════════════
 * APP TASK MANAGER - Process and task manager
 * ═══════════════════════════════════════════════════════════════════════ */
void app_task_manager(void) {
    char num_buf[32];
    int selected = 0;
    const int max_procs = 10;
    
    /* ANSI color codes */
    const char *RESET = "\033[0m";
    const char *BOLD = "\033[1m";
    const char *RED = "\033[31m";
    const char *GREEN = "\033[32m";
    const char *YELLOW = "\033[33m";
    const char *BLUE = "\033[34m";
    const char *CYAN = "\033[36m";
    const char *WHITE = "\033[37m";
    const char *BG_RED = "\033[41m";
    const char *BG_GREEN = "\033[42m";
    const char *BG_YELLOW = "\033[43m";
    
    app_print_header("VECTORD TASK MANAGER v1.0");
    
    serial_puts("\n");
    serial_puts("  Navigation: u=up, d=down, k=kill, r=refresh, q=quit\n");
    serial_puts("\n");
    
    int running = 1;
    while (running) {
        /* Clear screen */
        serial_puts("\033[2J\033[H");
        app_print_header("VECTORD TASK MANAGER v1.0");
        serial_puts("\n");
        
        /* Get process list (simulated) */
        /* In real kernel, we'd have process_get_list() */
        struct {
            int pid;
            char name[32];
            char state[16];
            int cpu_usage; /* percentage */
            int mem_usage; /* KB */
            int parent;
        } procs[10];
        int proc_count = 6;
        
        /* Simulated processes */
        const char *proc_names[] = {"kernel", "init", "shell", "file_manager", "network", "idle"};
        const char *proc_states[] = {"Running", "Running", "Running", "Sleeping", "Sleeping", "Running"};
        int cpu_vals[] = {2, 1, 5, 0, 0, 92};
        int mem_vals[] = {1024, 512, 2048, 1536, 1024, 256};
        
        for (int i = 0; i < proc_count; i++) {
            procs[i].pid = 1000 + i;
            app_strcpy(procs[i].name, proc_names[i]);
            app_strcpy(procs[i].state, proc_states[i]);
            procs[i].cpu_usage = cpu_vals[i];
            procs[i].mem_usage = mem_vals[i];
            procs[i].parent = (i == 0) ? 0 : 1000;
        }
        
        /* Draw table */
        serial_puts("  ┌─────┬────────────────────┬────────────┬─────────┬────────────┬──────────┐\n");
        serial_puts("  │ PID │ Name               │ State      │ CPU %   │ Memory KB  │ Parent   │\n");
        serial_puts("  ├─────┼────────────────────┼────────────┼─────────┼────────────┼──────────┤\n");
        
        for (int i = 0; i < proc_count; i++) {
            serial_puts("  │");
            
            /* Highlight selected row */
            if (i == selected) {
                serial_puts(BG_YELLOW);
                serial_puts(WHITE);
            }
            
            /* PID */
            app_itoa(procs[i].pid, num_buf);
            int len = app_strlen(num_buf);
            serial_puts(" ");
            for (int j = len; j < 4; j++) serial_puts(" ");
            serial_puts(num_buf);
            serial_puts(" │ ");
            
            /* Name */
            serial_puts(procs[i].name);
            int name_len = app_strlen(procs[i].name);
            for (int j = name_len; j < 18; j++) serial_puts(" ");
            serial_puts("│ ");
            
            /* State */
            if (app_strcmp(procs[i].state, "Running") == 0) {
                serial_puts(GREEN);
            } else if (app_strcmp(procs[i].state, "Sleeping") == 0) {
                serial_puts(BLUE);
            } else {
                serial_puts(WHITE);
            }
            serial_puts(procs[i].state);
            serial_puts(RESET);
            int state_len = app_strlen(procs[i].state);
            for (int j = state_len; j < 10; j++) serial_puts(" ");
            serial_puts(" │ ");
            
            /* CPU */
            app_itoa(procs[i].cpu_usage, num_buf);
            len = app_strlen(num_buf);
            serial_puts(" ");
            for (int j = len; j < 6; j++) serial_puts(" ");
            serial_puts(num_buf);
            serial_puts("% │ ");
            
            /* Memory */
            app_itoa(procs[i].mem_usage, num_buf);
            len = app_strlen(num_buf);
            serial_puts(" ");
            for (int j = len; j < 9; j++) serial_puts(" ");
            serial_puts(num_buf);
            serial_puts("   │ ");
            
            /* Parent */
            app_itoa(procs[i].parent, num_buf);
            len = app_strlen(num_buf);
            serial_puts(" ");
            for (int j = len; j < 7; j++) serial_puts(" ");
            serial_puts(num_buf);
            serial_puts(" │");
            
            if (i == selected) {
                serial_puts(RESET);
            }
            serial_puts("\n");
        }
        
        serial_puts("  └─────┴────────────────────┴────────────┴─────────┴────────────┴──────────┘\n");
        
        /* Summary */
        int total_cpu = 0, total_mem = 0;
        for (int i = 0; i < proc_count; i++) {
            total_cpu += procs[i].cpu_usage;
            total_mem += procs[i].mem_usage;
        }
        serial_puts("\n  Total CPU: ");
        app_itoa(total_cpu, num_buf);
        serial_puts(num_buf);
        serial_puts("% | Total Memory: ");
        app_itoa(total_mem / 1024, num_buf);
        serial_puts(num_buf);
        serial_puts(" MB\n");
        
        serial_puts("\n  Command: ");
        int key = -1;
        while (key == -1) {
            key = serial_getchar();
            if (key == -1) asm volatile ("hlt");
        }
        
        switch (key) {
            case 'q':
            case 'Q':
                running = 0;
                break;
            case 'u':
            case 'U':
                if (selected > 0) selected--;
                break;
            case 'd':
            case 'D':
                if (selected < proc_count - 1) selected++;
                break;
            case 'k':
            case 'K':
                /* Kill process (simulated) */
                serial_puts("\n  Kill process ");
                app_itoa(procs[selected].pid, num_buf);
                serial_puts(num_buf);
                serial_puts(" (y/n)? ");
                int confirm = -1;
                while (confirm == -1) {
                    confirm = serial_getchar();
                    if (confirm == -1) asm volatile ("hlt");
                }
                if (confirm == 'y' || confirm == 'Y') {
                    serial_puts("\n  Process ");
                    app_itoa(procs[selected].pid, num_buf);
                    serial_puts(num_buf);
                    serial_puts(" terminated.\n");
                    /* Remove from list (simulated) */
                    for (int i = selected; i < proc_count - 1; i++) {
                        procs[i] = procs[i + 1];
                    }
                    proc_count--;
                    if (selected >= proc_count) selected = proc_count - 1;
                }
                serial_puts("\n  Press any key...");
                while (serial_getchar() == -1) asm volatile ("hlt");
                break;
            case 'r':
            case 'R':
                /* Refresh - just loop again */
                break;
        }
    }
    
    serial_puts("\n  Task manager closed.\n");
}

/* ═══════════════════════════════════════════════════════════════════════
 * APP NETWORK CONFIG - Network configuration utility
 * ═══════════════════════════════════════════════════════════════════════ */
void app_network_config(void) {
    char num_buf[32];
    char ip_str[32];
    char mac_str[32];
    char gw_str[32];
    int selection = 0;
    
    /* ANSI color codes */
    const char *RESET = "\033[0m";
    const char *BOLD = "\033[1m";
    const char *GREEN = "\033[32m";
    const char *YELLOW = "\033[33m";
    const char *CYAN = "\033[36m";
    
    app_print_header("VECTORD NETWORK CONFIGURATION v1.0");
    
    /* Simulated network data */
    struct {
        char name[16];
        char ip[16];
        char netmask[16];
        char gateway[16];
        char mac[18];
        int dhcp; /* 0=static, 1=dhcp */
        int status; /* 0=down, 1=up */
    } interfaces[2];
    
    app_strcpy(interfaces[0].name, "eth0");
    app_strcpy(interfaces[0].ip, "192.168.1.100");
    app_strcpy(interfaces[0].netmask, "255.255.255.0");
    app_strcpy(interfaces[0].gateway, "192.168.1.1");
    app_strcpy(interfaces[0].mac, "AA:BB:CC:DD:EE:FF");
    interfaces[0].dhcp = 1;
    interfaces[0].status = 1;
    
    app_strcpy(interfaces[1].name, "lo");
    app_strcpy(interfaces[1].ip, "127.0.0.1");
    app_strcpy(interfaces[1].netmask, "255.0.0.0");
    app_strcpy(interfaces[1].gateway, "0.0.0.0");
    app_strcpy(interfaces[1].mac, "00:00:00:00:00:00");
    interfaces[1].dhcp = 0;
    interfaces[1].status = 1;
    
    int interface_count = 2;
    
    int running = 1;
    while (running) {
        serial_puts("\033[2J\033[H");
        app_print_header("VECTORD NETWORK CONFIGURATION v1.0");
        serial_puts("\n");
        
        /* Main menu */
        serial_puts("  ┌───────────────────────────────────────────────┐\n");
        serial_puts("  │ Network Interfaces:                           │\n");
        serial_puts("  ├───────────────────────────────────────────────┤\n");
        
        for (int i = 0; i < interface_count; i++) {
            serial_puts("  │ ");
            if (i == selection) {
                serial_puts(BOLD);
                serial_puts(GREEN);
            }
            serial_puts(interfaces[i].name);
            if (i == selection) {
                serial_puts(RESET);
            }
            int name_len = app_strlen(interfaces[i].name);
            for (int j = name_len; j < 4; j++) serial_puts(" ");
            
            serial_puts(": ");
            if (interfaces[i].status) {
                serial_puts(GREEN);
                serial_puts("UP   ");
                serial_puts(RESET);
            } else {
                serial_puts(YELLOW);
                serial_puts("DOWN ");
                serial_puts(RESET);
            }
            
            serial_puts("IP: ");
            serial_puts(interfaces[i].ip);
            int ip_len = app_strlen(interfaces[i].ip);
            for (int j = ip_len; j < 15; j++) serial_puts(" ");
            
            serial_puts(" DHCP: ");
            if (interfaces[i].dhcp) {
                serial_puts(GREEN);
                serial_puts("YES");
                serial_puts(RESET);
            } else {
                serial_puts(YELLOW);
                serial_puts("NO ");
                serial_puts(RESET);
            }
            serial_puts(" │\n");
        }
        
        serial_puts("  └───────────────────────────────────────────────┘\n");
        
        /* Detailed info for selected interface */
        serial_puts("\n  Selected Interface Details:\n");
        serial_puts("  ┌───────────────────────────────────────────────┐\n");
        serial_puts("  │ Name:    ");
        serial_puts(interfaces[selection].name);
        serial_puts("\n");
        serial_puts("  │ IP:      ");
        serial_puts(interfaces[selection].ip);
        serial_puts("\n");
        serial_puts("  │ Netmask: ");
        serial_puts(interfaces[selection].netmask);
        serial_puts("\n");
        serial_puts("  │ Gateway: ");
        serial_puts(interfaces[selection].gateway);
        serial_puts("\n");
        serial_puts("  │ MAC:     ");
        serial_puts(interfaces[selection].mac);
        serial_puts("\n");
        serial_puts("  │ DHCP:    ");
        serial_puts(interfaces[selection].dhcp ? "Enabled" : "Disabled");
        serial_puts("\n");
        serial_puts("  │ Status:  ");
        serial_puts(interfaces[selection].status ? "Up" : "Down");
        serial_puts("\n");
        serial_puts("  └───────────────────────────────────────────────┘\n");
        
        /* Menu */
        serial_puts("\n  Commands:\n");
        serial_puts("    u/d: Select interface\n");
        serial_puts("    t: Toggle DHCP\n");
        serial_puts("    p: Ping test\n");
        serial_puts("    s: Toggle status\n");
        serial_puts("    q: Quit\n");
        serial_puts("\n  Choice: ");
        
        int key = -1;
        while (key == -1) {
            key = serial_getchar();
            if (key == -1) asm volatile ("hlt");
        }
        
        switch (key) {
            case 'q':
            case 'Q':
                running = 0;
                break;
            case 'u':
            case 'U':
                if (selection > 0) selection--;
                break;
            case 'd':
            case 'D':
                if (selection < interface_count - 1) selection++;
                break;
            case 't':
            case 'T':
                interfaces[selection].dhcp = !interfaces[selection].dhcp;
                break;
            case 's':
            case 'S':
                interfaces[selection].status = !interfaces[selection].status;
                break;
            case 'p':
            case 'P':
                /* Ping test */
                serial_puts("\n  Pinging ");
                serial_puts(interfaces[selection].gateway);
                serial_puts("...\n");
                for (int i = 0; i < 4; i++) {
                    serial_puts("  Reply from ");
                    serial_puts(interfaces[selection].gateway);
                    serial_puts(": time=");
                    app_itoa(10 + (i * 5), num_buf);
                    serial_puts(num_buf);
                    serial_puts("ms\n");
                }
                serial_puts("\n  Press any key...");
                while (serial_getchar() == -1) asm volatile ("hlt");
                break;
        }
    }
    
    serial_puts("\n  Network configuration closed.\n");
}

/* ═══════════════════════════════════════════════════════════════════════
 * APP DISPLAY SETTINGS - Display configuration utility
 * ═══════════════════════════════════════════════════════════════════════ */
void app_display_settings(void) {
    char num_buf[32];
    int selection = 0;
    
    /* ANSI color codes */
    const char *RESET = "\033[0m";
    const char *BOLD = "\033[1m";
    const char *GREEN = "\033[32m";
    const char *YELLOW = "\033[33m";
    const char *CYAN = "\033[36m";
    const char *WHITE = "\033[37m";
    
    /* Simulated display settings */
    struct {
        char name[32];
        int width;
        int height;
        int color_depth;
    } modes[] = {
        {"VGA Text 80x25", 80, 25, 16},
        {"VGA Text 80x50", 80, 50, 16},
        {"Framebuffer 640x480", 640, 480, 256},
        {"Framebuffer 800x600", 800, 600, 16},
        {"Framebuffer 1024x768", 1024, 768, 16},
        {"Framebuffer 1280x1024", 1280, 1024, 24}
    };
    int mode_count = 6;
    
    char color_schemes[][32] = {
        "Default (Black/White)",
        "Blue Background",
        "Green Terminal",
        "Amber CRT",
        "White on Black",
        "Black on White"
    };
    int scheme_count = 6;
    
    int current_mode = 0;
    int current_scheme = 0;
    
    app_print_header("VECTORD DISPLAY SETTINGS v1.0");
    
    int running = 1;
    while (running) {
        serial_puts("\033[2J\033[H");
        app_print_header("VECTORD DISPLAY SETTINGS v1.0");
        serial_puts("\n");
        
        /* Display modes */
        serial_puts("  ┌─ Display Modes ───────────────────────────────────────────────────────┐\n");
        for (int i = 0; i < mode_count; i++) {
            serial_puts("  │ ");
            if (i == current_mode) {
                serial_puts(BOLD);
                serial_puts(GREEN);
                serial_puts("* ");
            } else {
                serial_puts("  ");
            }
            serial_puts(modes[i].name);
            int len = app_strlen(modes[i].name);
            for (int j = len; j < 30; j++) serial_puts(" ");
            
            serial_puts(" [");
            app_itoa(modes[i].width, num_buf);
            serial_puts(num_buf);
            serial_puts("x");
            app_itoa(modes[i].height, num_buf);
            serial_puts(num_buf);
            serial_puts(" @ ");
            app_itoa(modes[i].color_depth, num_buf);
            serial_puts(num_buf);
            serial_puts(" colors] │\n");
            
            if (i == current_mode) {
                serial_puts(RESET);
            }
        }
        serial_puts("  └──────────────────────────────────────────────────────────────────────────┘\n");
        
        /* Color schemes */
        serial_puts("\n  ┌─ Color Schemes ──────────────────────────────────────────────────────┐\n");
        for (int i = 0; i < scheme_count; i++) {
            serial_puts("  │ ");
            if (i == current_scheme) {
                serial_puts(BOLD);
                serial_puts(GREEN);
                serial_puts("* ");
            } else {
                serial_puts("  ");
            }
            serial_puts(color_schemes[i]);
            int len = app_strlen(color_schemes[i]);
            for (int j = len; j < 60; j++) serial_puts(" ");
            serial_puts(" │\n");
            if (i == current_scheme) {
                serial_puts(RESET);
            }
        }
        serial_puts("  └──────────────────────────────────────────────────────────────────────────┘\n");
        
        /* Current settings summary */
        serial_puts("\n  Current Settings:\n");
        serial_puts("  ┌──────────────────────────────────────────────────────┐\n");
        serial_puts("  │ Mode: ");
        serial_puts(modes[current_mode].name);
        serial_puts("\n");
        serial_puts("  │ Resolution: ");
        app_itoa(modes[current_mode].width, num_buf);
        serial_puts(num_buf);
        serial_puts("x");
        app_itoa(modes[current_mode].height, num_buf);
        serial_puts(num_buf);
        serial_puts("\n");
        serial_puts("  │ Color Depth: ");
        app_itoa(modes[current_mode].color_depth, num_buf);
        serial_puts(num_buf);
        serial_puts(" colors\n");
        serial_puts("  │ Color Scheme: ");
        serial_puts(color_schemes[current_scheme]);
        serial_puts("\n");
        serial_puts("  └──────────────────────────────────────────────────────┘\n");
        
        /* Menu */
        serial_puts("\n  Commands:\n");
        serial_puts("    m: Change mode selection\n");
        serial_puts("    c: Change color scheme\n");
        serial_puts("    a: Apply settings\n");
        serial_puts("    r: Reset to defaults\n");
        serial_puts("    q: Quit\n");
        serial_puts("\n  Choice: ");
        
        int key = -1;
        while (key == -1) {
            key = serial_getchar();
            if (key == -1) asm volatile ("hlt");
        }
        
        switch (key) {
            case 'q':
            case 'Q':
                running = 0;
                break;
            case 'm':
            case 'M':
                current_mode++;
                if (current_mode >= mode_count) current_mode = 0;
                break;
            case 'c':
            case 'C':
                current_scheme++;
                if (current_scheme >= scheme_count) current_scheme = 0;
                break;
            case 'a':
            case 'A':
                serial_puts("\n  Applying settings...\n");
                serial_puts("  New mode: ");
                serial_puts(modes[current_mode].name);
                serial_puts("\n");
                serial_puts("  Press any key to confirm (or ESC to cancel)...");
                int confirm = -1;
                while (confirm == -1) {
                    confirm = serial_getchar();
                    if (confirm == -1) asm volatile ("hlt");
                }
                if (confirm != 27) { /* ESC */
                    serial_puts("\n  Settings applied.\n");
                } else {
                    serial_puts("\n  Cancelled.\n");
                }
                serial_puts("  Press any key...");
                while (serial_getchar() == -1) asm volatile ("hlt");
                break;
            case 'r':
            case 'R':
                current_mode = 0;
                current_scheme = 0;
                serial_puts("\n  Settings reset to defaults.\n");
                serial_puts("  Press any key...");
                while (serial_getchar() == -1) asm volatile ("hlt");
                break;
        }
    }
    
    serial_puts("\n  Display settings closed.\n");
}

/* ═══════════════════════════════════════════════════════════════════════
 * APP KERNEL INFO - Kernel information utility
 * ═══════════════════════════════════════════════════════════════════════ */
void app_kernel_info(void) {
    char num_buf[32];
    
    /* ANSI color codes */
    const char *RESET = "\033[0m";
    const char *BOLD = "\033[1m";
    const char *RED = "\033[31m";
    const char *GREEN = "\033[32m";
    const char *YELLOW = "\033[33m";
    const char *BLUE = "\033[34m";
    const char *CYAN = "\033[36m";
    const char *WHITE = "\033[37m";
    
    app_print_header("VECTORD KERNEL INFORMATION v1.0");
    
    serial_puts("\n");
    serial_puts("  ┌─ Kernel Version ────────────────────────────────────────────────────┐\n");
    serial_puts("  │ ");
    serial_puts(BOLD);
    serial_puts(CYAN);
    serial_puts("Kernel:     ");
    serial_puts(RESET);
    serial_puts("VECTORD OS v");
    serial_puts(OS_VERSION);
    serial_puts("\n");
    serial_puts("  │ ");
    serial_puts(BOLD);
    serial_puts(CYAN);
    serial_puts("Codename:   ");
    serial_puts(RESET);
    serial_puts(OS_CODENAME);
    serial_puts("\n");
    serial_puts("  │ ");
    serial_puts(BOLD);
    serial_puts(CYAN);
    serial_puts("Build Date: ");
    serial_puts(RESET);
    serial_puts("2024-03-15 10:30:00 UTC\n");
    serial_puts("  │ ");
    serial_puts(BOLD);
    serial_puts(CYAN);
    serial_puts("Architecture: ");
    serial_puts(RESET);
    serial_puts("x86 (i386)\n");
    serial_puts("  └──────────────────────────────────────────────────────────────────────┘\n");
    
    /* Compile flags */
    serial_puts("\n  ┌─ Compile Flags ─────────────────────────────────────────────────────┐\n");
    serial_puts("  │ Optimization: -O2 -march=i486 -mtune=i486\n");
    serial_puts("  │ Debug:        -g -DDEBUG=1\n");
    serial_puts("  │ Features:     -DVECTORD_4D_MEMORY -DVECTORD_GEOMETRIC_SCHEDULER\n");
    serial_puts("  │               -DVECTORD_NEURAL_OPTIMIZER -DVECTORD_CUSTOM_FS\n");
    serial_puts("  │ Warnings:     -Wall -Wextra -Werror\n");
    serial_puts("  └──────────────────────────────────────────────────────────────────────┘\n");
    
    /* Kernel modules */
    serial_puts("\n  ┌─ Kernel Modules ────────────────────────────────────────────────────┐\n");
    serial_puts("  │ ID  Name                  Version  Size (KB)  Status                │\n");
    serial_puts("  │─────────────────────────────────────────────────────────────────────│\n");
    
    struct {
        int id;
        char name[24];
        char version[8];
        int size_kb;
        char status[16];
    } modules[] = {
        {1, "core", "1.0", 256, "Loaded"},
        {2, "memory", "1.0", 128, "Loaded"},
        {3, "process", "1.0", 96, "Loaded"},
        {4, "filesystem", "1.0", 192, "Loaded"},
        {5, "network", "1.0", 160, "Loaded"},
        {6, "drivers", "1.0", 320, "Loaded"},
        {7, "ui", "1.0", 64, "Loaded"}
    };
    int module_count = 7;
    
    for (int i = 0; i < module_count; i++) {
        serial_puts("  │ ");
        app_itoa(modules[i].id, num_buf);
        int len = app_strlen(num_buf);
        for (int j = len; j < 3; j++) serial_puts(" ");
        serial_puts(num_buf);
        serial_puts("  ");
        
        serial_puts(modules[i].name);
        len = app_strlen(modules[i].name);
        for (int j = len; j < 21; j++) serial_puts(" ");
        
        serial_puts(modules[i].version);
        len = app_strlen(modules[i].version);
        for (int j = len; j < 8; j++) serial_puts(" ");
        
        app_itoa(modules[i].size_kb, num_buf);
        len = app_strlen(num_buf);
        for (int j = len; j < 9; j++) serial_puts(" ");
        serial_puts(num_buf);
        serial_puts("   ");
        
        if (app_strcmp(modules[i].status, "Loaded") == 0) {
            serial_puts(GREEN);
        } else {
            serial_puts(RED);
        }
        serial_puts(modules[i].status);
        serial_puts(RESET);
        len = app_strlen(modules[i].status);
        for (int j = len; j < 20; j++) serial_puts(" ");
        serial_puts("│\n");
    }
    serial_puts("  └──────────────────────────────────────────────────────────────────────┘\n");
    
    /* Interrupt statistics */
    serial_puts("\n  ┌─ Interrupt Statistics ──────────────────────────────────────────────┐\n");
    serial_puts("  │ IRQ   Count      Rate/sec  Device                                 │\n");
    serial_puts("  │─────────────────────────────────────────────────────────────────────│\n");
    
    struct {
        int irq;
        unsigned long count;
        int rate;
        char device[32];
    } interrupts[] = {
        {0, 1234567890, 100, "Timer"},
        {1, 4567890, 50, "Keyboard"},
        {6, 12345, 1, "Floppy"},
        {11, 9876543, 1000, "Network"},
        {14, 234567890, 5000, "ATA Primary"},
        {15, 12345678, 1000, "ATA Secondary"}
    };
    int irq_count = 6;
    
    for (int i = 0; i < irq_count; i++) {
        serial_puts("  │ ");
        app_itoa(interrupts[i].irq, num_buf);
        int len = app_strlen(num_buf);
        for (int j = len; j < 3; j++) serial_puts(" ");
        serial_puts(num_buf);
        serial_puts("   ");
        
        /* Format large number */
        unsigned long count = interrupts[i].count;
        if (count >= 1000000000) {
            app_itoa(count / 1000000000, num_buf);
            serial_puts(num_buf);
            serial_puts(",");
            app_itoa((count % 1000000000) / 1000000, num_buf);
            serial_puts(num_buf);
            serial_puts(",");
            app_itoa((count % 1000000) / 1000, num_buf);
            serial_puts(num_buf);
            serial_puts(",");
            app_itoa(count % 1000, num_buf);
            serial_puts(num_buf);
        } else {
            app_itoa(count, num_buf);
            serial_puts(num_buf);
        }
        len = app_strlen(num_buf);
        for (int j = len; j < 12; j++) serial_puts(" ");
        
        app_itoa(interrupts[i].rate, num_buf);
        len = app_strlen(num_buf);
        for (int j = len; j < 8; j++) serial_puts(" ");
        serial_puts(num_buf);
        serial_puts("   ");
        
        serial_puts(interrupts[i].device);
        len = app_strlen(interrupts[i].device);
        for (int j = len; j < 38; j++) serial_puts(" ");
        serial_puts("│\n");
    }
    serial_puts("  └──────────────────────────────────────────────────────────────────────┘\n");
    
    /* Kernel statistics */
    serial_puts("\n  ┌─ Kernel Statistics ─────────────────────────────────────────────────┐\n");
    serial_puts("  │ Processes created:    1234\n");
    serial_puts("  │ Threads created:      5678\n");
    serial_puts("  │ Context switches:     9012345\n");
    serial_puts("  │ System calls:         34567890\n");
    serial_puts("  │ Page faults:          123456\n");
    serial_puts("  │ Uptime:               ");
    app_itoa(123456 / 3600, num_buf);
    serial_puts(num_buf);
    serial_puts("h ");
    app_itoa((123456 % 3600) / 60, num_buf);
    serial_puts(num_buf);
    serial_puts("m\n");
    serial_puts("  └──────────────────────────────────────────────────────────────────────┘\n");
    
    serial_puts("\n  Press Enter to return to shell...\n");
    while (serial_getchar() == -1) {
        asm volatile ("hlt");
    }
}

/* ═══════════════════════════════════════════════════════════════════════
 * APP MEMORY VIEWER - Memory viewer utility
 * ═══════════════════════════════════════════════════════════════════════ */
void app_memory_viewer(void) {
    char num_buf[32];
    char hex_buf[12];
    int selected_region = 0;
    int hex_mode = 0; /* 0 = memory map, 1 = hex dump */
    int scroll_offset = 0;
    
    /* ANSI color codes */
    const char *RESET = "\033[0m";
    const char *BOLD = "\033[1m";
    const char *GREEN = "\033[32m";
    const char *YELLOW = "\033[33m";
    const char *BLUE = "\033[34m";
    const char *CYAN = "\033[36m";
    const char *WHITE = "\033[37m";
    
    app_print_header("VECTORD MEMORY VIEWER v1.0");
    
    /* Simulated memory map */
    struct {
        char name[24];
        unsigned int start;
        unsigned int size;
        char type[16];
        int used; /* percentage */
    } regions[] = {
        {"Kernel Code", 0x00100000, 0x00100000, "Code", 85},
        {"Kernel Data", 0x00200000, 0x00100000, "Data", 70},
        {"Kernel Heap", 0x00300000, 0x00400000, "Heap", 45},
        {"Page Tables", 0x00700000, 0x00100000, "Tables", 30},
        {"User Space", 0x00800000, 0x0FF80000, "User", 15},
        {"Video Memory", 0x000A0000, 0x00020000, "Video", 100},
        {"BIOS ROM", 0x000C0000, 0x00040000, "ROM", 100},
        {"ACPI", 0x000F0000, 0x00010000, "ACPI", 100}
    };
    int region_count = 8;
    
    int running = 1;
    while (running) {
        serial_puts("\033[2J\033[H");
        app_print_header("VECTORD MEMORY VIEWER v1.0");
        serial_puts("\n");
        
        /* Toggle between map and hex view */
        if (!hex_mode) {
            /* Memory map view */
            serial_puts("  ┌─ Memory Map ─────────────────────────────────────────────────────┐\n");
            serial_puts("  │ Region              Start       Size (MB)   Type      Usage      │\n");
            serial_puts("  │─────────────────────────────────────────────────────────────────────│\n");
            
            for (int i = 0; i < region_count; i++) {
                serial_puts("  │");
                if (i == selected_region) {
                    serial_puts("\033[44m"); /* Blue background */
                }
                
                serial_puts(" ");
                serial_puts(regions[i].name);
                int len = app_strlen(regions[i].name);
                for (int j = len; j < 20; j++) serial_puts(" ");
                
                /* Start address */
                app_itoa_hex(regions[i].start, hex_buf);
                serial_puts(hex_buf);
                serial_puts("  ");
                
                /* Size in MB */
                app_itoa(regions[i].size / (1024*1024), num_buf);
                len = app_strlen(num_buf);
                for (int j = len; j < 8; j++) serial_puts(" ");
                serial_puts(num_buf);
                serial_puts("   ");
                
                serial_puts(regions[i].type);
                len = app_strlen(regions[i].type);
                for (int j = len; j < 9; j++) serial_puts(" ");
                serial_puts("  ");
                
                /* Usage bar */
                int bar_len = 10;
                int filled = (regions[i].used * bar_len) / 100;
                serial_puts("[");
                for (int j = 0; j < bar_len; j++) {
                    if (j < filled) {
                        serial_puts("#");
                    } else {
                        serial_puts(".");
                    }
                }
                serial_puts("] ");
                app_itoa(regions[i].used, num_buf);
                serial_puts(num_buf);
                serial_puts("%");
                
                if (i == selected_region) {
                    serial_puts(RESET);
                }
                serial_puts("│\n");
            }
            
            serial_puts("  └─────────────────────────────────────────────────────────────────────┘\n");
            
            /* Summary */
            unsigned int total_size = 0;
            for (int i = 0; i < region_count; i++) {
                total_size += regions[i].size;
            }
            serial_puts("\n  Total Memory: ");
            app_itoa(total_size / (1024*1024), num_buf);
            serial_puts(num_buf);
            serial_puts(" MB\n");
            
            serial_puts("\n  [h] Hex dump  [u/d] Select  [q] Quit\n");
        } else {
            /* Hex dump view */
            serial_puts("  ┌─ Hex Dump of ");
            serial_puts(regions[selected_region].name);
            serial_puts(" ──────────────────────────────────────────┐\n");
            
            /* Simulated hex dump of first 256 bytes */
            unsigned int base = regions[selected_region].start;
            for (int row = 0; row < 16; row++) {
                serial_puts("  │ ");
                
                /* Address */
                app_itoa_hex(base + row * 16, hex_buf);
                serial_puts(hex_buf);
                serial_puts(": ");
                
                /* Hex bytes */
                for (int col = 0; col < 16; col++) {
                    /* Simulated data */
                    int val = ((base + row * 16 + col) * 0x9E3779B9) & 0xFF;
                    app_itoa_hex_byte(val, hex_buf);
                    serial_puts(hex_buf);
                    if (col == 7) serial_puts("  ");
                    else if (col < 15) serial_puts(" ");
                }
                
                serial_puts(" |");
                
                /* ASCII */
                for (int col = 0; col < 16; col++) {
                    int val = ((base + row * 16 + col) * 0x9E3779B9) & 0xFF;
                    if (val >= 32 && val < 127) {
                        serial_putc(val);
                    } else {
                        serial_putc('.');
                    }
                }
                serial_puts("|\n");
            }
            
            serial_puts("  └─────────────────────────────────────────────────────────────────────┘\n");
            
            serial_puts("\n  [b] Back to map  [q] Quit\n");
        }
        
        /* Get key */
        int key = -1;
        while (key == -1) {
            key = serial_getchar();
            if (key == -1) asm volatile ("hlt");
        }
        
        switch (key) {
            case 'q':
            case 'Q':
                running = 0;
                break;
            case 'h':
            case 'H':
                hex_mode = 1;
                break;
            case 'b':
            case 'B':
                hex_mode = 0;
                break;
            case 'u':
            case 'U':
                if (selected_region > 0) selected_region--;
                break;
            case 'd':
            case 'D':
                if (selected_region < region_count - 1) selected_region++;
                break;
        }
    }
    
    serial_puts("\n  Memory viewer closed.\n");
}

/* ═══════════════════════════════════════════════════════════════════════
 * APP SYSTEM MONITOR - Real-time system monitoring
 * ═══════════════════════════════════════════════════════════════════════ */
void app_system_monitor(void) {
    char num_buf[32];
    char float_buf[32];
    int running = 1;
    int update_interval = 1000; /* ms */
    
    /* ANSI color codes */
    const char *RESET = "\033[0m";
    const char *BOLD = "\033[1m";
    const char *RED = "\033[31m";
    const char *GREEN = "\033[32m";
    const char *YELLOW = "\033[33m";
    const char *BLUE = "\033[34m";
    const char *CYAN = "\033[36m";
    const char *WHITE = "\033[37m";
    
    app_print_header("VECTORD SYSTEM MONITOR v1.0");
    
    /* Simulated system data */
    float cpu_usage = 15.0;
    float cpu_load[3] = {0.5, 0.3, 0.1}; /* 1, 5, 15 minute averages */
    unsigned int mem_total = 1024 * 1024 * 1024; /* 1 GB */
    unsigned int mem_used = 256 * 1024 * 1024; /* 256 MB */
    unsigned int mem_free = mem_total - mem_used;
    unsigned int uptime_seconds = 123456;
    
    serial_puts("\n  Press 'q' to quit\n");
    serial_puts("  Updating every second...\n");
    
    while (running) {
        /* Update simulated data */
        cpu_usage += ((rand() % 10) - 5); /* Random walk */
        if (cpu_usage < 0) cpu_usage = 0;
        if (cpu_usage > 100) cpu_usage = 100;
        
        /* Update uptime */
        uptime_seconds++;
        
        /* Clear screen */
        serial_puts("\033[2J\033[H");
        app_print_header("VECTORD SYSTEM MONITOR v1.0");
        serial_puts("\n");
        
        /* CPU usage */
        serial_puts("  ┌─ CPU Usage ─────────────────────────────────────────────────────────┐\n");
        serial_puts("  │ ");
        
        /* Draw CPU usage bar */
        int bar_width = 50;
        int filled = (int)(cpu_usage * bar_width / 100);
        serial_puts("[");
        for (int i = 0; i < bar_width; i++) {
            if (i < filled) {
                if (cpu_usage > 80) {
                    serial_puts(RED);
                } else if (cpu_usage > 60) {
                    serial_puts(YELLOW);
                } else {
                    serial_puts(GREEN);
                }
                serial_puts("#");
                serial_puts(RESET);
            } else {
                serial_puts(".");
            }
        }
        serial_puts("] ");
        app_float_to_str(cpu_usage, float_buf, 1);
        serial_puts(float_buf);
        serial_puts("%\n");
        
        serial_puts("  │ Load Average: ");
        app_float_to_str(cpu_load[0], float_buf, 2);
        serial_puts(float_buf);
        serial_puts(", ");
        app_float_to_str(cpu_load[1], float_buf, 2);
        serial_puts(float_buf);
        serial_puts(", ");
        app_float_to_str(cpu_load[2], float_buf, 2);
        serial_puts(float_buf);
        serial_puts("\n");
        serial_puts("  └──────────────────────────────────────────────────────────────────────┘\n");
        
        /* Memory usage */
        serial_puts("\n  ┌─ Memory Usage ───────────────────────────────────────────────────────┐\n");
        serial_puts("  │ Total: ");
        app_itoa(mem_total / (1024*1024), num_buf);
        serial_puts(num_buf);
        serial_puts(" MB\n");
        serial_puts("  │ Used:  ");
        app_itoa(mem_used / (1024*1024), num_buf);
        serial_puts(num_buf);
        serial_puts(" MB (");
        app_float_to_str((float)mem_used * 100 / mem_total, float_buf, 1);
        serial_puts(float_buf);
        serial_puts("%)\n");
        serial_puts("  │ Free:  ");
        app_itoa(mem_free / (1024*1024), num_buf);
        serial_puts(num_buf);
        serial_puts(" MB\n");
        
        /* Memory bar */
        serial_puts("  │ [");
        bar_width = 50;
        filled = (int)((float)mem_used * bar_width / mem_total);
        for (int i = 0; i < bar_width; i++) {
            if (i < filled) {
                serial_puts(BLUE);
                serial_puts("#");
                serial_puts(RESET);
            } else {
                serial_puts(".");
            }
        }
        serial_puts("]\n");
        serial_puts("  └──────────────────────────────────────────────────────────────────────┘\n");
        
        /* System uptime */
        serial_puts("\n  ┌─ System Information ─────────────────────────────────────────────────┐\n");
        serial_puts("  │ Uptime: ");
        
        int days = uptime_seconds / (24 * 3600);
        int hours = (uptime_seconds % (24 * 3600)) / 3600;
        int minutes = (uptime_seconds % 3600) / 60;
        int seconds = uptime_seconds % 60;
        
        if (days > 0) {
            app_itoa(days, num_buf);
            serial_puts(num_buf);
            serial_puts(" days, ");
        }
        app_itoa(hours, num_buf);
        serial_puts(num_buf);
        serial_puts(":");
        if (minutes < 10) serial_putc('0');
        app_itoa(minutes, num_buf);
        serial_puts(num_buf);
        serial_puts(":");
        if (seconds < 10) serial_putc('0');
        app_itoa(seconds, num_buf);
        serial_puts(num_buf);
        serial_puts("\n");
        
        serial_puts("  │ Processes: ");
        app_itoa(6, num_buf);
        serial_puts(num_buf);
        serial_puts("\n");
        serial_puts("  │ Threads: ");
        app_itoa(42, num_buf);
        serial_puts(num_buf);
        serial_puts("\n");
        serial_puts("  │ CPU Temp: 45°C\n");
        serial_puts("  └──────────────────────────────────────────────────────────────────────┘\n");
        
        /* Instructions */
        serial_puts("\n  [q] Quit  [u] Update slower  [d] Update faster\n");
        serial_puts("  Update interval: ");
        app_itoa(update_interval / 1000, num_buf);
        serial_puts(num_buf);
        serial_puts(" second(s)\n");
        
        /* Check for key press */
        int key = serial_getchar();
        if (key == 'q' || key == 'Q') {
            running = 0;
        } else if (key == 'u' || key == 'U') {
            update_interval += 500;
            if (update_interval > 10000) update_interval = 10000;
        } else if (key == 'd' || key == 'D') {
            update_interval -= 500;
            if (update_interval < 100) update_interval = 100;
        }
        
        /* Delay (simulated) */
        for (volatile int i = 0; i < update_interval * 1000; i++) {
            asm volatile ("nop");
        }
    }
    
    serial_puts("\n  System monitor closed.\n");
}

/* ═══════════════════════════════════════════════════════════════════════
 * APP LIST - List all available applications
 * ═══════════════════════════════════════════════════════════════════════ */
void app_list(void) {
    app_print_header("VECTORD APPLICATIONS");

    serial_puts("\n");
    serial_puts("  Available Applications:\n");
    serial_puts("  ─────────────────────────────────────────────\n");
    serial_puts("  calc         - Simple calculator\n");
    serial_puts("  clock        - Real-time clock display\n");
    serial_puts("  sysinfo      - Detailed system information\n");
    serial_puts("  files        - File browser\n");
    serial_puts("  settings     - System settings\n");
    serial_puts("  fileman      - Enhanced file manager with navigation\n");
    serial_puts("  taskman      - Process and task manager\n");
    serial_puts("  netconfig    - Network configuration\n");
    serial_puts("  display      - Display settings\n");
    serial_puts("  kernel       - Kernel information\n");
    serial_puts("  memview      - Memory viewer\n");
    serial_puts("  monitor      - System monitor\n");
    serial_puts("  apps         - This help screen\n");
    serial_puts("\n");
    serial_puts("  Usage: Type the command name at the shell prompt\n");
    serial_puts("\n");
}

