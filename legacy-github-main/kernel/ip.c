/* ═══════════════════════════════════════════════════════════════════════
 * VECTORD OS - IPv4 Protocol
 * 
 * IPv4 addressing, routing, and packet handling
 * ═══════════════════════════════════════════════════════════════════════ */

#include "../include/vecord.h"

/* ═══════════════════════════════════════════════════════════════════════
 * IP CONSTANTS
 * ═══════════════════════════════════════════════════════════════════════ */

#define IP_VERSION      4
#define IP_HEADER_LEN   20

/* IP Protocol numbers */
#define IP_PROTO_ICMP   1
#define IP_PROTO_TCP    6
#define IP_PROTO_UDP    17

/* EtherTypes */
#define ETH_P_IP        0x0800

/* ═══════════════════════════════════════════════════════════════════════
 * IP HEADER STRUCTURE
 * ═══════════════════════════════════════════════════════════════════════ */

typedef struct {
    u8  version_ihl;    /* Version (4 bits) + IHL (4 bits) */
    u8  tos;            /* Type of Service */
    u16 total_len;      /* Total Length */
    u16 id;             /* Identification */
    u16 flags_frag;     /* Flags (3 bits) + Fragment Offset (13 bits) */
    u8  ttl;            /* Time to Live */
    u8  protocol;       /* Protocol */
    u16 checksum;       /* Header Checksum */
    ip_addr_t src;      /* Source IP Address */
    ip_addr_t dst;      /* Destination IP Address */
} __attribute__((packed)) ip_header_t;

/* ═══════════════════════════════════════════════════════════════════════
 * ROUTING TABLE
 * ═══════════════════════════════════════════════════════════════════════ */

#define ROUTING_TABLE_SIZE 8

typedef struct {
    ip_addr_t network;
    ip_addr_t netmask;
    ip_addr_t gateway;
    int interface;
} route_entry_t;

static route_entry_t routing_table[ROUTING_TABLE_SIZE];
static int route_count = 0;

/* ═══════════════════════════════════════════════════════════════════════
 * IP INITIALIZATION
 * ═══════════════════════════════════════════════════════════════════════ */

void ip_init(void) {
    serial_puts("IP: IPv4 stack initialized\n");
    
    /* Clear routing table */
    route_count = 0;
    
    /* TODO: Add default route when network interface is configured */
}

/* ═══════════════════════════════════════════════════════════════════════
 * ROUTING FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════ */

int ip_route_add(ip_addr_t network, ip_addr_t netmask, ip_addr_t gateway, int interface) {
    if (route_count >= ROUTING_TABLE_SIZE) {
        return -1;
    }
    
    routing_table[route_count].network = network;
    routing_table[route_count].netmask = netmask;
    routing_table[route_count].gateway = gateway;
    routing_table[route_count].interface = interface;
    
    route_count++;
    return 0;
}

int ip_route_lookup(ip_addr_t dest, ip_addr_t *next_hop, int *interface) {
    /* TODO: Implement proper routing lookup */
    /* For now, use default gateway if configured */
    
    for (int i = 0; i < route_count; i++) {
        /* Check if destination is in the network */
        int match = 1;
        for (int j = 0; j < 4; j++) {
            if ((dest.addr[j] & routing_table[i].netmask.addr[j]) !=
                (routing_table[i].network.addr[j] & routing_table[i].netmask.addr[j])) {
                match = 0;
                break;
            }
        }
        
        if (match) {
            *next_hop = routing_table[i].gateway;
            *interface = routing_table[i].interface;
            return 0;
        }
    }
    
    return -1; /* No route found */
}

/* ═══════════════════════════════════════════════════════════════════════
 * IP CHECKSUM
 * ═══════════════════════════════════════════════════════════════════════ */

static u16 ip_checksum(const u8 *header, u16 len) {
    u32 sum = 0;
    
    /* Sum 16-bit words */
    while (len > 1) {
        sum += (header[0] << 8) | header[1];
        header += 2;
        len -= 2;
    }
    
    /* Add left-over byte */
    if (len == 1) {
        sum += header[0] << 8;
    }
    
    /* Fold 32-bit sum to 16 bits */
    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    
    return ~sum;
}

/* ═══════════════════════════════════════════════════════════════════════
 * IP PACKET CONSTRUCTION
 * ═══════════════════════════════════════════════════════════════════════ */

int ip_send(ip_addr_t dst, u8 protocol, const u8 *data, u16 len) {
    /* Find route to destination */
    ip_addr_t next_hop;
    int interface;
    
    if (ip_route_lookup(dst, &next_hop, &interface) != 0) {
        serial_puts("IP: No route to ");
        for (int i = 0; i < 4; i++) {
            serial_putdec(dst.addr[i]);
            if (i < 3) serial_putc('.');
        }
        serial_puts("\n");
        return -1;
    }
    
    /* Build IP packet */
    u8 packet[1500];
    ip_header_t *hdr = (ip_header_t *)packet;
    
    /* Fill header */
    hdr->version_ihl = (IP_VERSION << 4) | (IP_HEADER_LEN / 4);
    hdr->tos = 0;
    hdr->total_len = htons(IP_HEADER_LEN + len);
    hdr->id = 0;
    hdr->flags_frag = htons(0x4000); /* Don't fragment */
    hdr->ttl = 64;
    hdr->protocol = protocol;
    hdr->checksum = 0;
    
    /* Get source IP from interface */
    net_if_t *ifc = net_get_interface(interface);
    if (!ifc) {
        return -1;
    }
    
    hdr->src = ifc->ip;
    hdr->dst = dst;
    
    /* Calculate checksum */
    hdr->checksum = ip_checksum((u8 *)hdr, IP_HEADER_LEN);
    
    /* Copy payload */
    for (u16 i = 0; i < len; i++) {
        packet[IP_HEADER_LEN + i] = data[i];
    }
    
    /* Resolve next hop MAC via ARP */
    mac_addr_t dst_mac;
    if (arp_resolve(next_hop, &dst_mac) != 0) {
        /* Send ARP request and queue packet */
        arp_request(next_hop);
        return -2; /* ARP pending */
    }
    
    /* Send via Ethernet */
    return net_send_packet(ifc, dst_mac, ETH_P_IP, packet, IP_HEADER_LEN + len);
}

/* ═══════════════════════════════════════════════════════════════════════
 * IP PACKET RECEIVING
 * ═══════════════════════════════════════════════════════════════════════ */

int ip_receive(net_packet_t *pkt) {
    /* Ethernet header is 14 bytes */
    if (pkt->length < 14 + IP_HEADER_LEN) {
        return -1; /* Too short */
    }
    
    /* Skip Ethernet header */
    u8 *ip_start = pkt->data + 14;
    ip_header_t *hdr = (ip_header_t *)ip_start;
    
    /* Check version */
    if ((hdr->version_ihl >> 4) != IP_VERSION) {
        return -1; /* Not IPv4 */
    }
    
    /* Check header length */
    u8 ihl = (hdr->version_ihl & 0x0F) * 4;
    if (ihl < IP_HEADER_LEN) {
        return -1; /* Invalid IHL */
    }
    
    /* Verify checksum */
    u16 saved_checksum = hdr->checksum;
    hdr->checksum = 0;
    u16 calc_checksum = ip_checksum((u8 *)hdr, ihl);
    hdr->checksum = saved_checksum;
    
    if (saved_checksum != calc_checksum) {
        serial_puts("IP: Checksum error\n");
        return -1;
    }
    
    /* Get total length */
    u16 total_len = ntohs(hdr->total_len);
    if (total_len < ihl) {
        return -1;
    }
    
    /* Check if packet is for us */
    net_if_t *ifc = net_get_interface(0); /* TODO: Determine interface */
    if (!ifc) {
        return -1;
    }
    
    int for_us = 0;
    for (int i = 0; i < 4; i++) {
        if (hdr->dst.addr[i] != ifc->ip.addr[i]) {
            break;
        }
        if (i == 3) {
            for_us = 1;
        }
    }
    
    if (!for_us) {
        /* Not for us, check if we should forward */
        return 0;
    }
    
    /* Payload after IP header */
    u8 *payload = ip_start + ihl;
    u16 payload_len = total_len - ihl;
    
    /* Create a sub-packet for protocol handling (data points to protocol header) */
    net_packet_t sub_pkt;
    sub_pkt.data = payload;
    sub_pkt.length = payload_len;
    /* MAC addresses not used by protocols */
    
    switch (hdr->protocol) {
        case IP_PROTO_ICMP:
            icmp_handle(&sub_pkt);
            break;
        case IP_PROTO_UDP:
            udp_process_packet(&sub_pkt, hdr->src);
            break;
        case IP_PROTO_TCP:
            tcp_process_packet(&sub_pkt, hdr->src);
            break;
        default:
            serial_puts("IP: Unknown protocol ");
            serial_putdec(hdr->protocol);
            serial_puts("\n");
            break;
    }
    
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════
 * IP UTILITY FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════ */

int ip_addr_equal(ip_addr_t a, ip_addr_t b) {
    for (int i = 0; i < 4; i++) {
        if (a.addr[i] != b.addr[i]) {
            return 0;
        }
    }
    return 1;
}

void ip_addr_print(ip_addr_t addr) {
    for (int i = 0; i < 4; i++) {
        serial_putdec(addr.addr[i]);
        if (i < 3) serial_putc('.');
    }
}

int ip_addr_parse(const char *str, ip_addr_t *addr) {
    /* Simple IP address parser */
    int octets[4];
    int count = 0;
    
    while (*str && count < 4) {
        int val = 0;
        while (*str >= '0' && *str <= '9') {
            val = val * 10 + (*str - '0');
            str++;
        }
        
        if (val > 255) {
            return -1;
        }
        
        octets[count++] = val;
        
        if (*str == '.') {
            str++;
        } else if (*str == '\0') {
            break;
        } else {
            return -1;
        }
    }
    
    if (count != 4) {
        return -1;
    }
    
    for (int i = 0; i < 4; i++) {
        addr->addr[i] = octets[i];
    }
    
    return 0;
}
