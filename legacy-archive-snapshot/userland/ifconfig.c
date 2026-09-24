/* ═══════════════════════════════════════════════════════════════════════
 * VECTORD OS - ifconfig - Network configuration
 * ═══════════════════════════════════════════════════════════════════════ */

#include "libc.h"

static void print_usage(void) {
    puts("Usage: ifconfig [INTERFACE]");
    puts("Configure network interfaces");
    puts("");
    puts("Options:");
    puts("  -a    Show all interfaces");
    puts("  -h    Display this help");
}

static void print_interface(const char *name, const char *ip, const char *netmask, 
                           const char *hwaddr, int up) {
    puts(name);
    puts(": flags=");
    
    if (up) {
        puts("4163<UP,BROADCAST,RUNNING,MULTICAST>");
    } else {
        puts("4163<BROADCAST,MULTICAST>");
    }
    
    puts("  mtu 1500");
    puts("");
    
    puts("        inet ");
    puts(ip);
    puts("  netmask ");
    puts(netmask);
    puts("");
    
    puts("        inet6 fe80::1  prefixlen 64  scopeid 0x20<link>");
    puts("");
    
    puts("        ether ");
    puts(hwaddr);
    puts("  txqueuelen 1000  (Ethernet)");
    puts("");
    
    puts("        RX packets 0  bytes 0 (0.0 B)");
    puts("        RX errors 0  dropped 0  overruns 0  frame 0");
    puts("");
    
    puts("        TX packets 0  bytes 0 (0.0 B)");
    puts("        TX errors 0  dropped 0 overruns 0  carrier 0  collisions 0");
    puts("");
}

static void print_loopback(void) {
    puts("lo: flags=73<UP,LOOPBACK,RUNNING>  mtu 65536");
    puts("");
    puts("        inet 127.0.0.1  netmask 255.0.0.0");
    puts("");
    puts("        inet6 ::1  prefixlen 128  scopeid 0x10<host>");
    puts("");
    puts("        loop  txqueuelen 1000  (Local Loopback)");
    puts("");
    puts("        RX packets 0  bytes 0 (0.0 B)");
    puts("        RX errors 0  dropped 0  overruns 0  frame 0");
    puts("");
    puts("        TX packets 0  bytes 0 (0.0 B)");
    puts("        TX errors 0  dropped 0 overruns 0  carrier 0  collisions 0");
    puts("");
}

int main(int argc, char *argv[]) {
    int show_all = 0;
    const char *interface = NULL;
    
    /* Parse arguments */
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            if (strcmp(argv[i], "-a") == 0) {
                show_all = 1;
            } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
                print_usage();
                return 0;
            }
        } else {
            interface = argv[i];
        }
    }
    
    /* Show loopback */
    if (!interface || show_all || strcmp(interface, "lo") == 0) {
        print_loopback();
    }
    
    /* Show ethernet interface */
    if (!interface || show_all || strcmp(interface, "eth0") == 0) {
        print_interface("eth0", "10.0.2.15", "255.255.255.0", "52:54:00:12:34:56", 1);
    }
    
    if (show_all) {
        puts("");
        puts("lo: lo");
        puts("eth0: eth0");
    }
    
    return 0;
}
