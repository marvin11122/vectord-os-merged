/* ═══════════════════════════════════════════════════════════════════════
 * VECTORD OS - netstat - Network status
 * ═══════════════════════════════════════════════════════════════════════ */

#include "libc.h"

static int show_all = 0;
static int show_numeric = 0;
static int show_program = 0;
static int show_internet = 1;
static int show_unix = 0;

static void print_usage(void) {
    puts("Usage: netstat [OPTIONS]");
    puts("Display network connections and statistics");
    puts("");
    puts("Options:");
    puts("  -a    Show all sockets");
    puts("  -n    Show numeric addresses");
    puts("  -p    Show process using socket");
    puts("  -t    Show TCP connections");
    puts("  -u    Show UDP connections");
    puts("  -l    Show listening sockets");
    puts("  -h    Display this help");
}

static void print_tcp_header(void) {
    puts("Active Internet connections (w/o servers)");
    puts("Proto Recv-Q Send-Q Local Address           Foreign Address         State");
}

static void print_tcp_entry(const char *local, const char *foreign, const char *state) {
    puts("tcp        0      0 ");
    puts(local);
    
    /* Pad to column */
    int len = strlen(local);
    for (int i = len; i < 23; i++) putc(' ');
    
    puts(foreign);
    
    len = strlen(foreign);
    for (int i = len; i < 21; i++) putc(' ');
    
    puts(state);
    puts("");
}

static void print_udp_header(void) {
    puts("Active Internet connections (w/o servers)");
    puts("Proto Recv-Q Send-Q Local Address           Foreign Address");
}

static void print_udp_entry(const char *local, const char *foreign) {
    puts("udp        0      0 ");
    puts(local);
    
    int len = strlen(local);
    for (int i = len; i < 23; i++) putc(' ');
    
    puts(foreign);
    puts("");
}

static void print_unix_header(void) {
    puts("Active UNIX domain sockets (w/o servers)");
    puts("Proto RefCnt Flags       Type       State         I-Node Path");
}

static void print_stats(void) {
    puts("");
    puts("Tcp:");
    puts("  1 active connections openings");
    puts("  1 established connections");
    puts("");
    puts("Udp:");
    puts("  0 packets received");
    puts("  0 packets to unknown port received");
    puts("  0 packet receive errors");
    puts("  0 packets sent");
    puts("");
}

int main(int argc, char *argv[]) {
    /* Parse options */
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            if (strchr(argv[i], 'a')) show_all = 1;
            if (strchr(argv[i], 'n')) show_numeric = 1;
            if (strchr(argv[i], 'p')) show_program = 1;
            if (strchr(argv[i], 't')) show_internet = 1;
            if (strchr(argv[i], 'u')) show_unix = 1;
            if (strchr(argv[i], 'l')) show_all = 1;
            if (strchr(argv[i], 'h') || strcmp(argv[i], "--help") == 0) {
                print_usage();
                return 0;
            }
        }
    }
    
    /* Print TCP connections */
    print_tcp_header();
    
    /* Would query kernel network state */
    print_tcp_entry("127.0.0.1:22", "127.0.0.1:12345", "ESTABLISHED");
    
    if (show_unix) {
        print_unix_header();
    }
    
    print_stats();
    
    return 0;
}
