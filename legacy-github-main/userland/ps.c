/* ═══════════════════════════════════════════════════════════════════════
 * VECTORD OS - ps - List processes
 * ═══════════════════════════════════════════════════════════════════════ */

#include "libc.h"

static int show_all = 0;
static int show_full = 0;
static int show_threads = 0;

static void print_usage(void) {
    puts("Usage: ps [OPTIONS]");
    puts("Display running processes");
    puts("");
    puts("Options:");
    puts("  -a    Show all processes (including others)");
    puts("  -f    Full-format listing");
    puts("  -e    Select all processes");
    puts("  -h    Display this help");
}

static void print_header(void) {
    if (show_full) {
        puts("  UID   PID  PPID  C STIME TTY          TIME CMD");
    } else {
        puts("  PID TTY          TIME CMD");
    }
}

static void print_process(u32 pid, u32 ppid, const char *name, u32 cpu_time) {
    char buf[16];
    
    if (show_full) {
        puts("  root ");
        
        itoa((int)pid, buf, 10);
        /* Pad */
        int len = strlen(buf);
        for (int i = len; i < 5; i++) putc(' ');
        puts(buf);
        putc(' ');
        
        itoa((int)ppid, buf, 10);
        len = strlen(buf);
        for (int i = len; i < 5; i++) putc(' ');
        puts(buf);
        putc(' ');
        
        puts(" 1  ");
        puts("00:00  ");
        puts("?        ");
        
        /* Time */
        u32 hours = cpu_time / 3600;
        u32 mins = (cpu_time % 3600) / 60;
        u32 secs = cpu_time % 60;
        
        itoa((int)hours, buf, 10);
        if (hours < 10) putc('0');
        puts(buf);
        putc(':');
        itoa((int)mins, buf, 10);
        if (mins < 10) putc('0');
        puts(buf);
        putc(':');
        itoa((int)secs, buf, 10);
        if (secs < 10) putc('0');
        puts(buf);
        
        putc(' ');
        puts(name);
        puts("");
    } else {
        /* PID */
        itoa((int)pid, buf, 10);
        int len = strlen(buf);
        for (int i = len; i < 5; i++) putc(' ');
        puts(buf);
        
        puts(" ?        ");
        
        /* Time */
        u32 hours = cpu_time / 3600;
        u32 mins = (cpu_time % 3600) / 60;
        u32 secs = cpu_time % 60;
        
        itoa((int)hours, buf, 10);
        if (hours < 10) putc('0');
        puts(buf);
        putc(':');
        itoa((int)mins, buf, 10);
        if (mins < 10) putc('0');
        puts(buf);
        putc(':');
        itoa((int)secs, buf, 10);
        if (secs < 10) putc('0');
        puts(buf);
        
        putc(' ');
        puts(name);
        puts("");
    }
}

int main(int argc, char *argv[]) {
    /* Parse options */
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            if (strcmp(argv[i], "-a") == 0) {
                show_all = 1;
            } else if (strcmp(argv[i], "-f") == 0) {
                show_full = 1;
            } else if (strcmp(argv[i], "-e") == 0) {
                show_all = 1;
            } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
                print_usage();
                return 0;
            }
        }
    }
    
    print_header();
    
    /* Display hardcoded process list for now */
    print_process(1, 0, "init", 0);
    print_process(2, 1, "sh", 0);
    
    /* Would query kernel for actual process list */
    
    return 0;
}

