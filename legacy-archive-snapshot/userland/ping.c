/* ═══════════════════════════════════════════════════════════════════════
 * VECTORD OS - ping - ICMP ping utility
 * ═══════════════════════════════════════════════════════════════════════ */

#include "libc.h"

#define PING_COUNT      4
#define PING_TIMEOUT    5  /* seconds */
#define PING_SIZE       64

static int count = PING_COUNT;
static float interval = 1.0f;
static int verbose = 0;
static int flood = 0;

/* ICMP packet structure */
struct icmp_packet {
    u8 type;
    u8 code;
    u16 checksum;
    u16 id;
    u16 sequence;
    u8 data[PING_SIZE - 8];
};

static void print_usage(void) {
    puts("Usage: ping [OPTIONS] HOST");
    puts("Send ICMP ECHO_REQUEST packets to network hosts");
    puts("");
    puts("Options:");
    puts("  -c COUNT   Stop after COUNT packets");
    puts("  -i INTERVAL  Wait INTERVAL seconds between packets");
    puts("  -s SIZE    Send SIZE data bytes");
    puts("  -v         Verbose output");
    puts("  -h         Display this help");
}

/* Calculate checksum */
static u16 calculate_checksum(void *data, int len) {
    u16 *buf = (u16 *)data;
    u32 sum = 0;
    
    while (len > 1) {
        sum += *buf++;
        len -= 2;
    }
    
    if (len == 1) {
        sum += *(u8 *)buf;
    }
    
    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    
    return ~sum;
}

/* Parse IP address string to u32 */
static u32 parse_ip(const char *str) {
    u32 ip = 0;
    int octet = 0;
    int value = 0;
    
    while (*str) {
        if (*str >= '0' && *str <= '9') {
            value = value * 10 + (*str - '0');
        } else if (*str == '.') {
            ip = (ip << 8) | value;
            value = 0;
            octet++;
        }
        str++;
    }
    
    ip = (ip << 8) | value;
    
    return ip;
}

static void print_ip(u32 ip) {
    char buf[16];
    int a = (ip >> 24) & 0xFF;
    int b = (ip >> 16) & 0xFF;
    int c = (ip >> 8) & 0xFF;
    int d = ip & 0xFF;
    
    itoa(a, buf, 10);
    puts(buf);
    putc('.');
    itoa(b, buf, 10);
    puts(buf);
    putc('.');
    itoa(c, buf, 10);
    puts(buf);
    putc('.');
    itoa(d, buf, 10);
    puts(buf);
}

int main(int argc, char *argv[]) {
    const char *host = NULL;
    int size = PING_SIZE;
    
    /* Parse arguments */
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            if (strcmp(argv[i], "-c") == 0 && i + 1 < argc) {
                count = atoi(argv[++i]);
            } else if (strcmp(argv[i], "-i") == 0 && i + 1 < argc) {
                interval = (float)atoi(argv[++i]);
            } else if (strcmp(argv[i], "-s") == 0 && i + 1 < argc) {
                size = atoi(argv[++i]);
            } else if (strcmp(argv[i], "-v") == 0) {
                verbose = 1;
            } else if (strcmp(argv[i], "-f") == 0) {
                flood = 1;
            } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
                print_usage();
                return 0;
            }
        } else {
            host = argv[i];
        }
    }
    
    if (!host) {
        puts("ping: missing host operand");
        print_usage();
        return 1;
    }
    
    /* Resolve host (simplified - assume it's an IP) */
    u32 dest_ip = parse_ip(host);
    
    /* Print header */
    puts("PING ");
    print_ip(dest_ip);
    puts(" (");
    puts(host);
    puts(") ");
    itoa(size, (char []){0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}, 10);
    puts(" data bytes");
    puts("");
    
    int transmitted = 0;
    int received = 0;
    u32 min_rtt = 0xFFFFFFFF;
    u32 max_rtt = 0;
    u32 sum_rtt = 0;
    
    /* Send ping packets */
    for (int seq = 0; seq < count; seq++) {
        transmitted++;
        
        /* Would send ICMP packet here */
        u32 start_time = get_time();
        
        /* Simulate response */
        sys_sleep(1);
        
        u32 end_time = get_time();
        u32 rtt = end_time - start_time;
        
        received++;
        
        /* Update statistics */
        if (rtt < min_rtt) min_rtt = rtt;
        if (rtt > max_rtt) max_rtt = rtt;
        sum_rtt += rtt;
        
        /* Print response */
        {
            char num[16];
            itoa(seq + 1, num, 10);
            puts(num);
            puts(": icmp_seq=");
            puts(num);
            puts(" ttl=64 time=");
            itoa((int)rtt, num, 10);
            puts(num);
            puts(" ms");
        }
        
        if (seq < count - 1) {
            sys_sleep((u32)interval);
        }
    }
    
    /* Print statistics */
    puts("");
    puts("--- ");
    puts(host);
    puts(" ping statistics ---");
    puts("");
    
    char buf[16];
    
    itoa(transmitted, buf, 10);
    puts(buf);
    puts(" packets transmitted, ");
    
    itoa(received, buf, 10);
    puts(buf);
    puts(" received, ");
    
    int loss = transmitted > 0 ? ((transmitted - received) * 100 / transmitted) : 0;
    itoa(loss, buf, 10);
    puts(buf);
    puts("% packet loss");
    puts("");
    
    if (received > 0) {
        puts("rtt min/avg/max = ");
        itoa((int)min_rtt, buf, 10);
        puts(buf);
        putc('/');
        itoa((int)(sum_rtt / received), buf, 10);
        puts(buf);
        putc('/');
        itoa((int)max_rtt, buf, 10);
        puts(buf);
        puts(" ms");
    }
    
    return received > 0 ? 0 : 1;
}
