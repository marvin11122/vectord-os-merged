/* ═══════════════════════════════════════════════════════════════════════
 * VECTORD OS - Network Core
 * 
 * Network packet queueing, interface management, and protocol dispatch
 * ═══════════════════════════════════════════════════════════════════════ */

#include "../include/vecord.h"

/* ═══════════════════════════════════════════════════════════════════════
 * NETWORK QUEUE
 * ═══════════════════════════════════════════════════════════════════════ */

#define NET_QUEUE_SIZE 32

static net_packet_t *net_queue_head = NULL;
static net_packet_t *net_queue_tail = NULL;
static int net_queue_count = 0;

/* ═══════════════════════════════════════════════════════════════════════
 * NETWORK INTERFACES
 * ═══════════════════════════════════════════════════════════════════════ */

#define MAX_NET_INTERFACES 4

static net_if_t net_interfaces[MAX_NET_INTERFACES];
static int net_if_count = 0;

/* ═══════════════════════════════════════════════════════════════════════
 * UTILITY FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════ */

u16 htons(u16 val) {
    return (val << 8) | (val >> 8);
}

u16 ntohs(u16 val) {
    return (val << 8) | (val >> 8);
}

u32 htonl(u32 val) {
    return ((val & 0xFF) << 24) |
           ((val & 0xFF00) << 8) |
           ((val & 0xFF0000) >> 8) |
           ((val & 0xFF000000) >> 24);
}

u32 ntohl(u32 val) {
    return ((val & 0xFF) << 24) |
           ((val & 0xFF00) << 8) |
           ((val & 0xFF0000) >> 8) |
           ((val & 0xFF000000) >> 24);
}

u16 checksum(const u8 *data, u16 len) {
    u32 sum = 0;
    
    while (len > 1) {
        sum += (data[0] << 8) | data[1];
        data += 2;
        len -= 2;
    }
    
    if (len == 1) {
        sum += data[0] << 8;
    }
    
    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    
    return ~sum;
}

/* ═══════════════════════════════════════════════════════════════════════
 * PACKET QUEUE OPERATIONS
 * ═══════════════════════════════════════════════════════════════════════ */

static int net_queue_packet(net_packet_t *pkt) {
    if (net_queue_count >= NET_QUEUE_SIZE) {
        return -1; /* Queue full */
    }
    
    pkt->next = NULL;
    
    if (!net_queue_head) {
        net_queue_head = pkt;
        net_queue_tail = pkt;
    } else {
        net_queue_tail->next = pkt;
        net_queue_tail = pkt;
    }
    
    net_queue_count++;
    return 0;
}

static net_packet_t *net_dequeue_packet(void) {
    if (!net_queue_head) {
        return NULL;
    }
    
    net_packet_t *pkt = net_queue_head;
    net_queue_head = pkt->next;
    
    if (!net_queue_head) {
        net_queue_tail = NULL;
    }
    
    net_queue_count--;
    pkt->next = NULL;
    return pkt;
}

/* ═══════════════════════════════════════════════════════════════════════
 * NETWORK INTERFACE MANAGEMENT
 * ═══════════════════════════════════════════════════════════════════════ */

int net_add_interface(mac_addr_t mac, ip_addr_t ip, ip_addr_t netmask, ip_addr_t gateway) {
    if (net_if_count >= MAX_NET_INTERFACES) {
        return -1;
    }
    
    net_if_t *ifc = &net_interfaces[net_if_count];
    ifc->index = net_if_count;
    ifc->mac = mac;
    ifc->ip = ip;
    ifc->netmask = netmask;
    ifc->gateway = gateway;
    ifc->present = 1;
    
    net_if_count++;
    return ifc->index;
}

net_if_t *net_get_interface(int index) {
    if (index < 0 || index >= net_if_count) {
        return NULL;
    }
    return &net_interfaces[index];
}

/* ═══════════════════════════════════════════════════════════════════════
 * NETWORK INITIALIZATION
 * ═══════════════════════════════════════════════════════════════════════ */

void net_init(void) {
    serial_puts("NET: Initializing network stack\n");
    
    /* Clear interfaces */
    for (int i = 0; i < MAX_NET_INTERFACES; i++) {
        net_interfaces[i].present = 0;
    }
    net_if_count = 0;
    
    /* Initialize subsystems */
    arp_init();
    ip_init();
    icmp_init();
    udp_init();
    tcp_init();
    
    serial_puts("NET: Network stack initialized\n");
}

/* ═══════════════════════════════════════════════════════════════════════
 * PACKET TRANSMISSION
 * ═══════════════════════════════════════════════════════════════════════ */

int net_send_packet(net_if_t *ifc, mac_addr_t dst, u16 ethertype, const u8 *data, u16 len) {
    /* Build Ethernet frame */
    u8 frame[1514]; /* Max Ethernet frame size */
    u16 frame_len = 14 + len; /* Ethernet header + payload */
    
    if (frame_len > 1514) {
        return -1; /* Packet too large */
    }
    
    /* Ethernet header */
    for (int i = 0; i < 6; i++) {
        frame[i] = dst.addr[i];     /* Destination MAC */
        frame[i + 6] = ifc->mac.addr[i]; /* Source MAC */
    }
    frame[12] = (ethertype >> 8) & 0xFF;
    frame[13] = ethertype & 0xFF;
    
    /* Copy payload */
    for (u16 i = 0; i < len; i++) {
        frame[14 + i] = data[i];
    }
    
    /* TODO: Call driver transmit function */
    /* For now, just print */
    serial_puts("NET: Sending packet, ethertype=0x");
    serial_puthex(ethertype);
    serial_puts(", len=");
    serial_putdec(len);
    serial_puts("\n");
    
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════
 * PACKET RECEPTION (called from driver)
 * ═══════════════════════════════════════════════════════════════════════ */

int net_receive_packet(net_if_t *ifc, net_packet_t *pkt) {
    /* Queue packet for processing */
    return net_queue_packet(pkt);
}

/* ═══════════════════════════════════════════════════════════════════════
 * PACKET PROCESSING
 * ═══════════════════════════════════════════════════════════════════════ */

void net_process_packet(net_packet_t *pkt) {
    if (!pkt || pkt->length < 14) {
        return;
    }
    
    /* Extract Ethernet header */
    u16 ethertype = (pkt->data[12] << 8) | pkt->data[13];
    
    switch (ethertype) {
        case 0x0806: /* ARP */
            arp_handle(pkt);
            break;
        case 0x0800: /* IPv4 */
            ip_receive(pkt);
            break;
        default:
            serial_puts("NET: Unknown ethertype 0x");
            serial_puthex(ethertype);
            serial_puts("\n");
            break;
    }
}

/* ═══════════════════════════════════════════════════════════════════════
 * NETWORK MAIN (called from kernel main)
 * ═══════════════════════════════════════════════════════════════════════ */

void network_main(void) {
    serial_puts("NET: Starting network processing\n");
    
    /* Main network loop */
    while (1) {
        /* Process queued packets */
        net_packet_t *pkt = net_dequeue_packet();
        if (pkt) {
            net_process_packet(pkt);
        }
        
        /* TODO: Add timer-based tasks (ARP timeouts, TCP retransmission, etc.) */
        /* For now, just yield */
        proc_yield();
    }
}

/* ═══════════════════════════════════════════════════════════════════════
 * NETWORK STATUS
 * ═══════════════════════════════════════════════════════════════════════ */

void net_print_status(void) {
    serial_puts("NET: Network Status\n");
    serial_puts("  Interfaces: ");
    serial_putdec(net_if_count);
    serial_puts("\n");
    serial_puts("  Queued packets: ");
    serial_putdec(net_queue_count);
    serial_puts("\n");
}
