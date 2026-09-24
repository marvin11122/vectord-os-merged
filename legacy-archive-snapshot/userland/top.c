/* ═══════════════════════════════════════════════════════════════════════
 * VECTORD OS - top - Process monitor
 * ═══════════════════════════════════════════════════════════════════════ */

#include "libc.h"

#define UPDATE_INTERVAL 3  /* seconds */

static int iterations = 0;
static int running = 1;

static void print_header(void) {
    puts("");
    puts("VECTORD OS Process Monitor - Press Ctrl+C to exit");
    puts("══════════════════════════════════════════════════════════════");
    puts("");
}

static void print_top_line(void) {
    u32 time = get_time();
    u32 hours = (time / 3600) % 24;
    u32 mins = (time / 60) % 60;
    u32 secs = time % 60;
    
    char buf[16];
    
    puts("  PID USER       PR  NI VIRT   RES   SHR S  %CPU %MEM  COMMAND");
    
    /* Time display */
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
    puts(" up 0:00, 1 user, load average: 0.00, 0.00, 0.00");
    puts("");
}

static void print_processes(void) {
    /* Simulated process list */
    struct {
        u32 pid;
        u32 ppid;
        const char *user;
        u32 virt;
        u32 res;
        u32 shr;
        char state;
        float cpu;
        float mem;
        const char *cmd;
    } procs[] = {
        {1,  0, "root", 1024, 512,  256, 'S', 0.0f, 0.1f, "init"},
        {2,  1, "root", 1024, 512,  256, 'S', 0.0f, 0.1f, "sh"},
        {0, 0, NULL, 0, 0, 0, 0, 0.0f, 0.0f, NULL}
    };
    
    for (int i = 0; procs[i].cmd; i++) {
        char buf[16];
        
        /* PID */
        itoa((int)procs[i].pid, buf, 10);
        int len = strlen(buf);
        for (int j = len; j < 5; j++) putc(' ');
        puts(buf);
        
        puts(" root     ");
        
        /* PRI */
        puts(" 20   ");
        
        /* NI */
        puts(" 0    ");
        
        /* VIRT */
        itoa((int)procs[i].virt, buf, 10);
        len = strlen(buf);
        for (int j = len; j < 5; j++) putc(' ');
        puts(buf);
        
        putc(' ');
        
        /* RES */
        itoa((int)procs[i].res, buf, 10);
        len = strlen(buf);
        for (int j = len; j < 5; j++) putc(' ');
        puts(buf);
        
        putc(' ');
        
        /* SHR */
        itoa((int)procs[i].shr, buf, 10);
        len = strlen(buf);
        for (int j = len; j < 5; j++) putc(' ');
        puts(buf);
        
        putc(' ');
        
        /* State */
        putc(procs[i].state);
        putc(' ');
        
        /* CPU */
        /* Simplified - just print 0.0 */
        puts("  0.0  ");
        
        /* MEM */
        puts("  0.1  ");
        
        /* Command */
        puts(procs[i].cmd);
        puts("");
    }
}

static void print_summary(void) {
    puts("");
    puts("Tasks:   2 total,   2 running,   0 sleeping,   0 stopped,   0 zombie");
    puts("CPU:   0.0%us,  0.0%sy,  0.0%ni, 99.9%id,  0.0%wa,  0.0%hi,  0.0%si");
    puts("Mem:   4096k total,   512k used,  3584k free,     0k buffers");
    puts("");
    puts("  PID USER      PR  NI VIRT   RES   SHR S %CPU %MEM  COMMAND");
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    
    puts("VECTORD OS Process Monitor");
    puts("Press 'q' to quit");
    puts("");
    
    while (running) {
        /* Clear and redraw (simplified - just print new content) */
        print_header();
        print_top_line();
        print_processes();
        print_summary();
        
        iterations++;
        
        /* Wait for update interval */
        sys_sleep(UPDATE_INTERVAL);
        
        /* Check for input (non-blocking) */
        /* Would use syscalls to check for keypress */
    }
    
    return 0;
}
