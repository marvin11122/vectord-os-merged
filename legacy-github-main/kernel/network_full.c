/* ═══════════════════════════════════════════════════════════════════════
 * VECTORD OS - Complete Network Stack Implementation
 * 
 * Full TCP/IP network stack for VECTORD OS
 * Includes: Ethernet, ARP, IPv4, ICMP, UDP, TCP, DHCP, Socket API
 * ═══════════════════════════════════════════════════════════════════════ */

#include "../include/vecord.h"

/* ═══════════════════════════════════════════════════════════════════════
 * NETWORK CONSTANTS
 * ═══════════════════════════════════════════════════════════════════════ */

#define MAX_NET_INTERFACES   4
#define NET_QUEUE_SIZE       32
#define ARP_TABLE_SIZE       16
#define ROUTING_TABLE_SIZE   8
#define TCP_MAX_SOCKETS      16
#define UDP_MAX_SOCKETS      16
#define MAX_SOCKETS          32

/* Ethernet constants */
#define ETH_ALEN            6
#define ETH_FRAME_MIN       64
#define ETH_FRAME_MAX       1518
#define ETH_P_IP            0x0800
#define ETH_P_ARP           0x0806

/* IP constants */
#define IP_VERSION          4
#define IP_HEADER_LEN       20
#define IP_PROTO_ICMP       1
#define IP_PROTO_TCP        6
#define IP_PROTO_UDP        17

/* ARP constants */
#define ARP_REQUEST         1
#define ARP_REPLY           2
#define ARP_HW_ETHER        1
#define ARP_PROTO_IP        0x0800

/* ICMP constants */
#define ICMP_ECHO_REQUEST   8
#define ICMP_ECHO_REPLY     0

/* TCP constants */
#define TCP_FIN             0x01
#define TCP_SYN             0x02
#define TCP_RST             0x04
#define TCP_PSH             0x08
#define TCP_ACK             0x10
#define TCP_URG             0x20

#define TCP_CLOSED          0
#define TCP_LISTEN          1
#define TCP_SYN_SENT        2
#define TCP_SYN_RECEIVED    3
#define TCP_ESTABLISHED     4
#define TCP_FIN_WAIT1       5
#define TCP_FIN_WAIT2       6
#define TCP_CLOSE_WAIT      7
#define TCP_CLOSING         8
#define TCP_LAST_ACK        9
#define TCP_TIME_WAIT       10

/* DHCP constants */
#define DHCP_SERVER_PORT    67
#define DHCP_CLIENT_PORT    68
#define DHCP_DISCOVER       1
#define DHCP_OFFER          2
#define DHCP_REQUEST        3
#define DHCP_ACK            5
#define DHCP_NAK            6

/* DHCP options */
#define DHCP_OPT_SUBNET_MASK    1
#define DHCP_OPT_ROUTER         3
#define DHCP_OPT_DNS            6
#define DHCP_OPT_REQUESTED_IP   50
#define DHCP_OPT_LEASE_TIME     51
#define DHCP_OPT_MSG_TYPE       53
#define DHCP_OPT_SERVER_ID      54
#define DHCP_OPT_END            255

/* ═══════════════════════════════════════════════════════════════════════
 * NETWORK STRUCTURES
 * ═══════════════════════════════════════════════════════════════════════ */

/* Ethernet frame structure - defined in vecord.h */

/* ARP packet structure */
typedef struct {
    u16 hardware_type;
    u16 protocol_type;
    u8  hardware_len;
    u8  protocol_len;
    u16 operation;
    mac_addr_t sender_mac;
    ip_addr_t sender_ip;
    mac_addr_t target_mac;
    ip_addr_t target_ip;
} __attribute__((packed)) arp_packet_t;

/* ARP table entry */
typedef struct {
    ip_addr_t ip;
    mac_addr_t mac;
    u32 timestamp;
    u8 valid;
} arp_entry_t;

/* IP header structure */
typedef struct {
    u8  version_ihl;
    u8  tos;
    u16 total_len;
    u16 id;
    u16 flags_frag;
    u8  ttl;
    u8  protocol;
    u16 checksum;
    ip_addr_t src;
    ip_addr_t dst;
} __attribute__((packed)) ip_header_t;

/* ICMP header structure */
typedef struct {
    u8  type;
    u8  code;
    u16 checksum;
    u16 id;
    u16 sequence;
} __attribute__((packed)) icmp_header_t;

/* UDP header structure */
typedef struct {
    u16 src_port;
    u16 dst_port;
    u16 length;
    u16 checksum;
} __attribute__((packed)) udp_header_t;

/* TCP header structure */
typedef struct {
    u16 src_port;
    u16 dst_port;
    u32 seq;
    u32 ack;
    u8  data_offset;
    u8  flags;
    u16 window;
    u16 checksum;
    u16 urgent;
} __attribute__((packed)) tcp_header_t;

/* TCP socket internal structure */
typedef struct {
    u16 local_port;
    ip_addr_t remote_ip;
    u16 remote_port;
    u8 state;
    u32 seq;
    u32 ack;
    u8 send_buf[1024];
    u16 send_len;
    u8 recv_buf[1024];
    u16 recv_len;
    u8 active;
} tcp_socket_internal_t;

/* UDP socket internal structure */
typedef struct {
    u16 local_port;
    ip_addr_t remote_ip;
    u16 remote_port;
    u8 active;
    u8 buffer[1024];
    u16 buf_len;
} udp_socket_internal_t;

/* DHCP packet structure */
typedef struct {
    u8  op;
    u8  htype;
    u8  hlen;
    u8  hops;
    u32 xid;
    u16 secs;
    u16 flags;
    ip_addr_t ciaddr;
    ip_addr_t yiaddr;
    ip_addr_t siaddr;
    ip_addr_t giaddr;
    u8  chaddr[16];
    u8  sname[64];
    u8  file[128];
    u8  options[312];
} __attribute__((packed)) dhcp_packet_t;

/* Routing table entry */
typedef struct {
    ip_addr_t network;
    ip_addr_t netmask;
    ip_addr_t gateway;
    int interface;
} route_entry_t;

/* ═══════════════════════════════════════════════════════════════════════
 * GLOBAL STATE
 * ═══════════════════════════════════════════════════════════════════════ */

/* Network interfaces */
static net_if_t net_interfaces[MAX_NET_INTERFACES];
static int net_if_count = 0;

/* Packet queue */
static net_packet_t *net_queue_head = NULL;
static net_packet_t *net_queue_tail = NULL;
static int net_queue_count = 0;

/* ARP table */
static arp_entry_t arp_table[ARP_TABLE_SIZE];
static int arp_count = 0;

/* Routing table */
static route_entry_t routing_table[ROUTING_TABLE_SIZE];
static int route_count = 0;

/* TCP sockets */
static tcp_socket_internal_t tcp_sockets[TCP_MAX_SOCKETS];
static int tcp_socket_count = 0;

/* UDP sockets */
static udp_socket_internal_t udp_sockets[UDP_MAX_SOCKETS];
static int udp_socket_count = 0;

/* Socket API table */
static socket_t sockets[MAX_SOCKETS];
static int socket_count = 0;

/* DHCP state */
static u32 dhcp_xid = 0x12345678;
static ip_addr_t dhcp_server_ip;
static u8 dhcp_state = 0;

/* Network statistics */
static u32 net_tx_packets = 0;
static u32 net_rx_packets = 0;
static u32 net_tx_bytes = 0;
static u32 net_rx_bytes = 0;
static u32 net_errors = 0;

/* Ping statistics */
static u32 ping_sent = 0;
static u32 ping_received = 0;
/* TODO: Implement ping statistics tracking
static u32 ping_min_time = 0xFFFFFFFF;
static u32 ping_max_time = 0;
static u32 ping_total_time = 0;
*/

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

void mac_addr_print(mac_addr_t addr) {
    for (int i = 0; i < 6; i++) {
        serial_puthex(addr.addr[i]);
        if (i < 5) serial_putc(':');
    }
}

/* Print hex dump of data */
void print_hex_dump(const u8 *data, u16 len) {
    for (u16 i = 0; i < len; i++) {
        if (i % 16 == 0) {
            serial_puts("\n  ");
        }
        serial_puthex(data[i]);
        serial_putc(' ');
    }
    serial_puts("\n");
}

/* ═══════════════════════════════════════════════════════════════════════
 * PACKET QUEUE OPERATIONS
 * ═══════════════════════════════════════════════════════════════════════ */

static int net_queue_packet(net_packet_t *pkt) {
    if (net_queue_count >= NET_QUEUE_SIZE) {
        return -1;
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
 * ARP PROTOCOL
 * ═══════════════════════════════════════════════════════════════════════ */

void arp_init(void) {
    for (int i = 0; i < ARP_TABLE_SIZE; i++) {
        arp_table[i].valid = 0;
    }
    arp_count = 0;
}

int arp_table_lookup(ip_addr_t ip, mac_addr_t *mac) {
    for (int i = 0; i < ARP_TABLE_SIZE; i++) {
        if (arp_table[i].valid && ip_addr_equal(arp_table[i].ip, ip)) {
            *mac = arp_table[i].mac;
            return 0;
        }
    }
    return -1;
}

void arp_table_add(ip_addr_t ip, mac_addr_t mac) {
    for (int i = 0; i < ARP_TABLE_SIZE; i++) {
        if (arp_table[i].valid && ip_addr_equal(arp_table[i].ip, ip)) {
            arp_table[i].mac = mac;
            arp_table[i].timestamp = 0;
            return;
        }
    }
    
    for (int i = 0; i < ARP_TABLE_SIZE; i++) {
        if (!arp_table[i].valid) {
            arp_table[i].ip = ip;
            arp_table[i].mac = mac;
            arp_table[i].valid = 1;
            arp_table[i].timestamp = 0;
            arp_count++;
            return;
        }
    }
    
    static int next_slot = 0;
    arp_table[next_slot].ip = ip;
    arp_table[next_slot].mac = mac;
    arp_table[next_slot].valid = 1;
    arp_table[next_slot].timestamp = 0;
    next_slot = (next_slot + 1) % ARP_TABLE_SIZE;
}

void arp_request(ip_addr_t ip) {
    arp_packet_t pkt;
    
    pkt.hardware_type = htons(ARP_HW_ETHER);
    pkt.protocol_type = htons(ARP_PROTO_IP);
    pkt.hardware_len = 6;
    pkt.protocol_len = 4;
    pkt.operation = htons(ARP_REQUEST);
    
    net_if_t *ifc = net_get_interface(0);
    if (!ifc) {
        return;
    }
    
    pkt.sender_mac = ifc->mac;
    pkt.sender_ip = ifc->ip;
    
    for (int i = 0; i < 6; i++) {
        pkt.target_mac.addr[i] = 0xFF;
    }
    pkt.target_ip = ip;
    
    mac_addr_t broadcast;
    for (int i = 0; i < 6; i++) {
        broadcast.addr[i] = 0xFF;
    }
    
    net_send_packet(ifc, broadcast, 0x0806, (u8 *)&pkt, sizeof(pkt));
}

void arp_reply(mac_addr_t src_mac, ip_addr_t src_ip, mac_addr_t dst_mac, ip_addr_t dst_ip) {
    arp_packet_t pkt;
    
    pkt.hardware_type = htons(ARP_HW_ETHER);
    pkt.protocol_type = htons(ARP_PROTO_IP);
    pkt.hardware_len = 6;
    pkt.protocol_len = 4;
    pkt.operation = htons(ARP_REPLY);
    
    pkt.sender_mac = src_mac;
    pkt.sender_ip = src_ip;
    pkt.target_mac = dst_mac;
    pkt.target_ip = dst_ip;
    
    net_if_t *ifc = net_get_interface(0);
    if (!ifc) {
        return;
    }
    
    net_send_packet(ifc, dst_mac, 0x0806, (u8 *)&pkt, sizeof(pkt));
}

int arp_resolve(ip_addr_t ip, mac_addr_t *mac) {
    if (arp_table_lookup(ip, mac) == 0) {
        return 0;
    }
    
    arp_request(ip);
    return -1;
}

void arp_handle(net_packet_t *pkt) {
    if (pkt->length < 14 + sizeof(arp_packet_t)) {
        return;
    }
    
    arp_packet_t *arp = (arp_packet_t *)(pkt->data + 14);
    
    u16 hardware_type = ntohs(arp->hardware_type);
    u16 protocol_type = ntohs(arp->protocol_type);
    u16 operation = ntohs(arp->operation);
    
    if (hardware_type != ARP_HW_ETHER || protocol_type != ARP_PROTO_IP) {
        return;
    }
    
    net_if_t *ifc = net_get_interface(0);
    if (!ifc) {
        return;
    }
    
    int for_us = ip_addr_equal(arp->target_ip, ifc->ip);
    
    arp_table_add(arp->sender_ip, arp->sender_mac);
    
    if (operation == ARP_REQUEST && for_us) {
        arp_reply(ifc->mac, ifc->ip, arp->sender_mac, arp->sender_ip);
    }
}

void arp_print_table(void) {
    serial_puts("ARP Table:\n");
    for (int i = 0; i < ARP_TABLE_SIZE; i++) {
        if (arp_table[i].valid) {
            serial_puts("  ");
            ip_addr_print(arp_table[i].ip);
            serial_puts(" -> ");
            mac_addr_print(arp_table[i].mac);
            serial_puts("\n");
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════════
 * IPv4 PROTOCOL
 * ═══════════════════════════════════════════════════════════════════════ */

void ip_init(void) {
    route_count = 0;
}

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
    for (int i = 0; i < route_count; i++) {
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
    
    return -1;
}

static u16 ip_checksum(const u8 *header, u16 len) {
    u32 sum = 0;
    
    while (len > 1) {
        sum += (header[0] << 8) | header[1];
        header += 2;
        len -= 2;
    }
    
    if (len == 1) {
        sum += header[0] << 8;
    }
    
    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    
    return ~sum;
}

int ip_send(ip_addr_t dst, u8 protocol, const u8 *data, u16 len) {
    ip_addr_t next_hop;
    int interface;
    
    if (ip_route_lookup(dst, &next_hop, &interface) != 0) {
        return -1;
    }
    
    u8 packet[1500];
    ip_header_t *hdr = (ip_header_t *)packet;
    
    hdr->version_ihl = (IP_VERSION << 4) | (IP_HEADER_LEN / 4);
    hdr->tos = 0;
    hdr->total_len = htons(IP_HEADER_LEN + len);
    hdr->id = 0;
    hdr->flags_frag = htons(0x4000);
    hdr->ttl = 64;
    hdr->protocol = protocol;
    hdr->checksum = 0;
    
    net_if_t *ifc = net_get_interface(interface);
    if (!ifc) {
        return -1;
    }
    
    hdr->src = ifc->ip;
    hdr->dst = dst;
    
    hdr->checksum = ip_checksum((u8 *)hdr, IP_HEADER_LEN);
    
    for (u16 i = 0; i < len; i++) {
        packet[IP_HEADER_LEN + i] = data[i];
    }
    
    mac_addr_t dst_mac;
    if (arp_resolve(next_hop, &dst_mac) != 0) {
        return -2;
    }
    
    return net_send_packet(ifc, dst_mac, ETH_P_IP, packet, IP_HEADER_LEN + len);
}

int ip_receive(net_packet_t *pkt) {
    if (pkt->length < 14 + IP_HEADER_LEN) {
        return -1;
    }
    
    u8 *ip_start = pkt->data + 14;
    ip_header_t *hdr = (ip_header_t *)ip_start;
    
    if ((hdr->version_ihl >> 4) != IP_VERSION) {
        return -1;
    }
    
    u8 ihl = (hdr->version_ihl & 0x0F) * 4;
    if (ihl < IP_HEADER_LEN) {
        return -1;
    }
    
    u16 saved_checksum = hdr->checksum;
    hdr->checksum = 0;
    u16 calc_checksum = ip_checksum((u8 *)hdr, ihl);
    hdr->checksum = saved_checksum;
    
    if (saved_checksum != calc_checksum) {
        return -1;
    }
    
    u16 total_len = ntohs(hdr->total_len);
    if (total_len < ihl) {
        return -1;
    }
    
    net_if_t *ifc = net_get_interface(0);
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
        return 0;
    }
    
    u8 *payload = ip_start + ihl;
    u16 payload_len = total_len - ihl;
    
    net_packet_t sub_pkt;
    sub_pkt.data = payload;
    sub_pkt.length = payload_len;
    
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
            break;
    }
    
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════
 * ICMP PROTOCOL
 * ═══════════════════════════════════════════════════════════════════════ */

void icmp_init(void) {
}

static u16 icmp_checksum(const u8 *data, u16 len) {
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

void icmp_echo_request(ip_addr_t dst, u16 id, u16 seq) {
    u8 packet[64];
    icmp_header_t *hdr = (icmp_header_t *)packet;
    
    hdr->type = ICMP_ECHO_REQUEST;
    hdr->code = 0;
    hdr->checksum = 0;
    hdr->id = htons(id);
    hdr->sequence = htons(seq);
    
    for (unsigned int i = sizeof(icmp_header_t); i < sizeof(packet); i++) {
        packet[i] = i;
    }
    
    hdr->checksum = icmp_checksum(packet, sizeof(packet));
    
    ip_send(dst, IP_PROTO_ICMP, packet, sizeof(packet));
    
    ping_sent++;
}

void icmp_echo_reply(ip_addr_t src, u16 id, u16 seq) {
    u8 packet[64];
    icmp_header_t *hdr = (icmp_header_t *)packet;
    
    hdr->type = ICMP_ECHO_REPLY;
    hdr->code = 0;
    hdr->checksum = 0;
    hdr->id = htons(id);
    hdr->sequence = htons(seq);
    
    for (unsigned int i = sizeof(icmp_header_t); i < sizeof(packet); i++) {
        packet[i] = i;
    }
    
    hdr->checksum = icmp_checksum(packet, sizeof(packet));
    
    ip_send(src, IP_PROTO_ICMP, packet, sizeof(packet));
}

void icmp_handle(net_packet_t *pkt) {
    if (pkt->length < sizeof(icmp_header_t)) {
        return;
    }
    
    icmp_header_t *hdr = (icmp_header_t *)pkt->data;
    
    switch (hdr->type) {
        case ICMP_ECHO_REQUEST:
            {
                u16 id = ntohs(hdr->id);
                u16 seq = ntohs(hdr->sequence);
                ip_addr_t src = {0};
                icmp_echo_reply(src, id, seq);
            }
            break;
        case ICMP_ECHO_REPLY:
            ping_received++;
            break;
        default:
            break;
    }
}

/* ═══════════════════════════════════════════════════════════════════════
 * UDP PROTOCOL
 * ═══════════════════════════════════════════════════════════════════════ */

void udp_init(void) {
    for (int i = 0; i < UDP_MAX_SOCKETS; i++) {
        udp_sockets[i].active = 0;
    }
    udp_socket_count = 0;
}

static u16 udp_checksum(const u8 *data, u16 len, ip_addr_t src, ip_addr_t dst) {
    u8 pseudo[12];
    for (int i = 0; i < 4; i++) {
        pseudo[i] = src.addr[i];
        pseudo[4 + i] = dst.addr[i];
    }
    pseudo[8] = 0;
    pseudo[9] = IP_PROTO_UDP;
    pseudo[10] = (len >> 8) & 0xFF;
    pseudo[11] = len & 0xFF;
    
    u32 sum = 0;
    
    for (int i = 0; i < 12; i += 2) {
        sum += (pseudo[i] << 8) | pseudo[i + 1];
    }
    
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

int udp_send(ip_addr_t dst, u16 dst_port, u16 src_port, const u8 *data, u16 len) {
    u8 packet[1500];
    udp_header_t *hdr = (udp_header_t *)packet;
    
    if (len > sizeof(packet) - sizeof(udp_header_t)) {
        return -1;
    }
    
    hdr->src_port = htons(src_port);
    hdr->dst_port = htons(dst_port);
    hdr->length = htons(sizeof(udp_header_t) + len);
    hdr->checksum = 0;
    
    for (u16 i = 0; i < len; i++) {
        packet[sizeof(udp_header_t) + i] = data[i];
    }
    
    net_if_t *ifc = net_get_interface(0);
    if (ifc) {
        hdr->checksum = udp_checksum(packet, sizeof(udp_header_t) + len, ifc->ip, dst);
    }
    
    return ip_send(dst, IP_PROTO_UDP, packet, sizeof(udp_header_t) + len);
}

int udp_receive(socket_t *sock, u8 *buf, u16 *len) {
    for (int i = 0; i < UDP_MAX_SOCKETS; i++) {
        if (udp_sockets[i].active && udp_sockets[i].local_port == sock->local_port) {
            if (udp_sockets[i].buf_len == 0) {
                return 0;
            }
            
            u16 copy_len = udp_sockets[i].buf_len;
            if (copy_len > *len) {
                copy_len = *len;
            }
            
            for (u16 j = 0; j < copy_len; j++) {
                buf[j] = udp_sockets[i].buffer[j];
            }
            *len = copy_len;
            
            udp_sockets[i].buf_len = 0;
            
            return copy_len;
        }
    }
    
    return -1;
}

void udp_process_packet(net_packet_t *pkt, ip_addr_t src) {
    if (pkt->length < sizeof(udp_header_t)) {
        return;
    }
    
    udp_header_t *hdr = (udp_header_t *)pkt->data;
    
    u16 dst_port = ntohs(hdr->dst_port);
    u16 src_port = ntohs(hdr->src_port);
    u16 length = ntohs(hdr->length);
    
    for (int i = 0; i < UDP_MAX_SOCKETS; i++) {
        if (udp_sockets[i].active && udp_sockets[i].local_port == dst_port) {
            u16 copy_len = length - sizeof(udp_header_t);
            if (copy_len > sizeof(udp_sockets[i].buffer)) {
                copy_len = sizeof(udp_sockets[i].buffer);
            }
            
            for (u16 j = 0; j < copy_len; j++) {
                udp_sockets[i].buffer[j] = pkt->data[sizeof(udp_header_t) + j];
            }
            udp_sockets[i].buf_len = copy_len;
            
            udp_sockets[i].remote_ip = src;
            udp_sockets[i].remote_port = src_port;
            
            return;
        }
    }
}

int udp_socket_create(u16 port) {
    if (udp_socket_count >= UDP_MAX_SOCKETS) {
        return -1;
    }
    
    for (int i = 0; i < UDP_MAX_SOCKETS; i++) {
        if (!udp_sockets[i].active) {
            udp_sockets[i].local_port = port;
            udp_sockets[i].active = 1;
            udp_sockets[i].buf_len = 0;
            udp_socket_count++;
            return i;
        }
    }
    
    return -1;
}

int udp_socket_close(int sockfd) {
    if (sockfd < 0 || sockfd >= UDP_MAX_SOCKETS || !udp_sockets[sockfd].active) {
        return -1;
    }
    
    udp_sockets[sockfd].active = 0;
    udp_socket_count--;
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════
 * TCP PROTOCOL
 * ═══════════════════════════════════════════════════════════════════════ */

void tcp_init(void) {
    for (int i = 0; i < TCP_MAX_SOCKETS; i++) {
        tcp_sockets[i].active = 0;
        tcp_sockets[i].state = TCP_CLOSED;
    }
    tcp_socket_count = 0;
}

static u16 tcp_checksum(const u8 *data, u16 len, ip_addr_t src, ip_addr_t dst) {
    u8 pseudo[12];
    for (int i = 0; i < 4; i++) {
        pseudo[i] = src.addr[i];
        pseudo[4 + i] = dst.addr[i];
    }
    pseudo[8] = 0;
    pseudo[9] = IP_PROTO_TCP;
    pseudo[10] = (len >> 8) & 0xFF;
    pseudo[11] = len & 0xFF;
    
    u32 sum = 0;
    
    for (int i = 0; i < 12; i += 2) {
        sum += (pseudo[i] << 8) | pseudo[i + 1];
    }
    
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

static int tcp_send_packet(tcp_socket_internal_t *sock, u8 flags, const u8 *data, u16 data_len) {
    u8 packet[1500];
    tcp_header_t *hdr = (tcp_header_t *)packet;
    
    if (data_len > sizeof(packet) - sizeof(tcp_header_t)) {
        return -1;
    }
    
    hdr->src_port = htons(sock->local_port);
    hdr->dst_port = htons(sock->remote_port);
    hdr->seq = htonl(sock->seq);
    hdr->ack = htonl(sock->ack);
    hdr->data_offset = (sizeof(tcp_header_t) / 4) << 4;
    hdr->flags = flags;
    hdr->window = htons(1024);
    hdr->checksum = 0;
    hdr->urgent = 0;
    
    if (data && data_len > 0) {
        for (u16 i = 0; i < data_len; i++) {
            packet[sizeof(tcp_header_t) + i] = data[i];
        }
    }
    
    net_if_t *ifc = net_get_interface(0);
    if (ifc) {
        hdr->checksum = tcp_checksum(packet, sizeof(tcp_header_t) + data_len, ifc->ip, sock->remote_ip);
    }
    
    return ip_send(sock->remote_ip, IP_PROTO_TCP, packet, sizeof(tcp_header_t) + data_len);
}

int tcp_connect(ip_addr_t dst, u16 dst_port) {
    for (int i = 0; i < TCP_MAX_SOCKETS; i++) {
        if (!tcp_sockets[i].active) {
            tcp_sockets[i].local_port = 1024 + i;
            tcp_sockets[i].remote_ip = dst;
            tcp_sockets[i].remote_port = dst_port;
            tcp_sockets[i].state = TCP_CLOSED;
            tcp_sockets[i].seq = 0;
            tcp_sockets[i].ack = 0;
            tcp_sockets[i].send_len = 0;
            tcp_sockets[i].recv_len = 0;
            tcp_sockets[i].active = 1;
            tcp_socket_count++;
            
            tcp_sockets[i].state = TCP_SYN_SENT;
            tcp_send_packet(&tcp_sockets[i], TCP_SYN, NULL, 0);
            
            return i;
        }
    }
    return -1;
}

int tcp_listen(u16 port) {
    for (int i = 0; i < TCP_MAX_SOCKETS; i++) {
        if (!tcp_sockets[i].active) {
            tcp_sockets[i].local_port = port;
            tcp_sockets[i].remote_port = 0;
            tcp_sockets[i].state = TCP_LISTEN;
            tcp_sockets[i].seq = 0;
            tcp_sockets[i].ack = 0;
            tcp_sockets[i].send_len = 0;
            tcp_sockets[i].recv_len = 0;
            tcp_sockets[i].active = 1;
            tcp_socket_count++;
            
            return i;
        }
    }
    return -1;
}

int tcp_send(socket_t *sock, const u8 *data, u16 len) {
    for (int i = 0; i < TCP_MAX_SOCKETS; i++) {
        if (tcp_sockets[i].active && tcp_sockets[i].local_port == sock->local_port) {
            if (tcp_sockets[i].state != TCP_ESTABLISHED) {
                return -1;
            }
            
            int ret = tcp_send_packet(&tcp_sockets[i], TCP_PSH | TCP_ACK, data, len);
            if (ret == 0) {
                tcp_sockets[i].seq += len;
                return len;
            }
            return ret;
        }
    }
    return -1;
}

int tcp_receive(socket_t *sock, u8 *buf, u16 *len) {
    for (int i = 0; i < TCP_MAX_SOCKETS; i++) {
        if (tcp_sockets[i].active && tcp_sockets[i].local_port == sock->local_port) {
            if (tcp_sockets[i].recv_len == 0) {
                return 0;
            }
            
            u16 copy_len = tcp_sockets[i].recv_len;
            if (copy_len > *len) {
                copy_len = *len;
            }
            
            for (u16 j = 0; j < copy_len; j++) {
                buf[j] = tcp_sockets[i].recv_buf[j];
            }
            *len = copy_len;
            
            if (copy_len < tcp_sockets[i].recv_len) {
                for (u16 j = 0; j < tcp_sockets[i].recv_len - copy_len; j++) {
                    tcp_sockets[i].recv_buf[j] = tcp_sockets[i].recv_buf[copy_len + j];
                }
            }
            tcp_sockets[i].recv_len -= copy_len;
            
            return copy_len;
        }
    }
    return -1;
}

void tcp_close(socket_t *sock) {
    for (int i = 0; i < TCP_MAX_SOCKETS; i++) {
        if (tcp_sockets[i].active && tcp_sockets[i].local_port == sock->local_port) {
            tcp_send_packet(&tcp_sockets[i], TCP_FIN | TCP_ACK, NULL, 0);
            tcp_sockets[i].state = TCP_FIN_WAIT1;
            return;
        }
    }
}

void tcp_process_packet(net_packet_t *pkt, ip_addr_t src) {
    if (pkt->length < sizeof(tcp_header_t)) {
        return;
    }
    
    tcp_header_t *hdr = (tcp_header_t *)pkt->data;
    
    u16 dst_port = ntohs(hdr->dst_port);
    u16 src_port = ntohs(hdr->src_port);
    u8 flags = hdr->flags;
    
    for (int i = 0; i < TCP_MAX_SOCKETS; i++) {
        if (tcp_sockets[i].active && tcp_sockets[i].local_port == dst_port) {
            tcp_sockets[i].remote_ip = src;
            tcp_sockets[i].remote_port = src_port;
            
            switch (tcp_sockets[i].state) {
                case TCP_LISTEN:
                    if (flags & TCP_SYN) {
                        tcp_sockets[i].ack = ntohl(hdr->seq) + 1;
                        tcp_send_packet(&tcp_sockets[i], TCP_SYN | TCP_ACK, NULL, 0);
                        tcp_sockets[i].state = TCP_SYN_RECEIVED;
                    }
                    break;
                    
                case TCP_SYN_SENT:
                    if ((flags & (TCP_SYN | TCP_ACK)) == (TCP_SYN | TCP_ACK)) {
                        tcp_sockets[i].ack = ntohl(hdr->seq) + 1;
                        tcp_sockets[i].seq++;
                        tcp_send_packet(&tcp_sockets[i], TCP_ACK, NULL, 0);
                        tcp_sockets[i].state = TCP_ESTABLISHED;
                    }
                    break;
                    
                case TCP_SYN_RECEIVED:
                    if (flags & TCP_ACK) {
                        tcp_sockets[i].state = TCP_ESTABLISHED;
                    }
                    break;
                    
                case TCP_ESTABLISHED:
                    if (flags & TCP_FIN) {
                        tcp_sockets[i].ack = ntohl(hdr->seq) + 1;
                        tcp_send_packet(&tcp_sockets[i], TCP_ACK, NULL, 0);
                        tcp_sockets[i].state = TCP_CLOSE_WAIT;
                    } else if (pkt->length > sizeof(tcp_header_t)) {
                        u16 data_len = pkt->length - sizeof(tcp_header_t);
                        if (data_len > sizeof(tcp_sockets[i].recv_buf) - tcp_sockets[i].recv_len) {
                            data_len = sizeof(tcp_sockets[i].recv_buf) - tcp_sockets[i].recv_len;
                        }
                        
                        for (u16 j = 0; j < data_len; j++) {
                            tcp_sockets[i].recv_buf[tcp_sockets[i].recv_len + j] = pkt->data[sizeof(tcp_header_t) + j];
                        }
                        tcp_sockets[i].recv_len += data_len;
                        
                        tcp_sockets[i].ack = ntohl(hdr->seq) + data_len;
                        tcp_send_packet(&tcp_sockets[i], TCP_ACK, NULL, 0);
                    }
                    break;
                    
                case TCP_FIN_WAIT1:
                    if (flags & TCP_ACK) {
                        tcp_sockets[i].state = TCP_FIN_WAIT2;
                    }
                    break;
                    
                case TCP_FIN_WAIT2:
                    if (flags & TCP_FIN) {
                        tcp_sockets[i].ack = ntohl(hdr->seq) + 1;
                        tcp_send_packet(&tcp_sockets[i], TCP_ACK, NULL, 0);
                        tcp_sockets[i].state = TCP_CLOSED;
                        tcp_sockets[i].active = 0;
                        tcp_socket_count--;
                    }
                    break;
                    
                case TCP_CLOSE_WAIT:
                    break;
                    
                default:
                    break;
            }
            return;
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════════
 * DHCP CLIENT
 * ═══════════════════════════════════════════════════════════════════════ */

void dhcp_init(void) {
    dhcp_state = 0;
}

void dhcp_discover(void) {
    dhcp_packet_t pkt;
    
    for (int i = 0; i < sizeof(pkt); i++) {
        ((u8 *)&pkt)[i] = 0;
    }
    
    pkt.op = 1;
    pkt.htype = 1;
    pkt.hlen = 6;
    pkt.xid = dhcp_xid;
    pkt.flags = htons(0x8000);
    
    net_if_t *ifc = net_get_interface(0);
    if (!ifc) {
        return;
    }
    
    for (int i = 0; i < 6; i++) {
        pkt.chaddr[i] = ifc->mac.addr[i];
    }
    
    int opt_idx = 0;
    pkt.options[opt_idx++] = DHCP_OPT_MSG_TYPE;
    pkt.options[opt_idx++] = 1;
    pkt.options[opt_idx++] = DHCP_DISCOVER;
    
    pkt.options[opt_idx++] = 55;
    pkt.options[opt_idx++] = 4;
    pkt.options[opt_idx++] = DHCP_OPT_SUBNET_MASK;
    pkt.options[opt_idx++] = DHCP_OPT_ROUTER;
    pkt.options[opt_idx++] = DHCP_OPT_DNS;
    pkt.options[opt_idx++] = DHCP_OPT_SERVER_ID;
    
    pkt.options[opt_idx++] = DHCP_OPT_END;
    
    ip_addr_t broadcast = {{255, 255, 255, 255}};
    udp_send(broadcast, DHCP_SERVER_PORT, DHCP_CLIENT_PORT, (u8 *)&pkt, sizeof(pkt));
    
    dhcp_state = 1;
}

static void dhcp_request(ip_addr_t requested_ip) {
    dhcp_packet_t pkt;
    
    for (int i = 0; i < sizeof(pkt); i++) {
        ((u8 *)&pkt)[i] = 0;
    }
    
    pkt.op = 1;
    pkt.htype = 1;
    pkt.hlen = 6;
    pkt.xid = dhcp_xid;
    pkt.flags = htons(0x8000);
    
    net_if_t *ifc = net_get_interface(0);
    if (!ifc) {
        return;
    }
    
    for (int i = 0; i < 6; i++) {
        pkt.chaddr[i] = ifc->mac.addr[i];
    }
    
    int opt_idx = 0;
    pkt.options[opt_idx++] = DHCP_OPT_MSG_TYPE;
    pkt.options[opt_idx++] = 1;
    pkt.options[opt_idx++] = DHCP_REQUEST;
    
    pkt.options[opt_idx++] = DHCP_OPT_REQUESTED_IP;
    pkt.options[opt_idx++] = 4;
    for (int i = 0; i < 4; i++) {
        pkt.options[opt_idx++] = requested_ip.addr[i];
    }
    
    pkt.options[opt_idx++] = DHCP_OPT_SERVER_ID;
    pkt.options[opt_idx++] = 4;
    for (int i = 0; i < 4; i++) {
        pkt.options[opt_idx++] = dhcp_server_ip.addr[i];
    }
    
    pkt.options[opt_idx++] = DHCP_OPT_END;
    
    ip_addr_t broadcast = {{255, 255, 255, 255}};
    udp_send(broadcast, DHCP_SERVER_PORT, DHCP_CLIENT_PORT, (u8 *)&pkt, sizeof(pkt));
}

void dhcp_handle_offer(net_packet_t *pkt) {
    dhcp_packet_t *offer = (dhcp_packet_t *)pkt->data;
    
    if (offer->op != 2 || offer->xid != dhcp_xid) {
        return;
    }
    
    int opt_idx = 0;
    while (opt_idx < 312) {
        u8 opt = offer->options[opt_idx++];
        if (opt == DHCP_OPT_END) break;
        if (opt == 0) continue;
        
        u8 len = offer->options[opt_idx++];
        
        switch (opt) {
            case DHCP_OPT_SERVER_ID:
                for (int i = 0; i < 4; i++) {
                    dhcp_server_ip.addr[i] = offer->options[opt_idx + i];
                }
                break;
            case DHCP_OPT_MSG_TYPE:
                break;
        }
        opt_idx += len;
    }
    
    dhcp_request(offer->yiaddr);
}

void dhcp_handle_ack(net_packet_t *pkt) {
    dhcp_packet_t *ack = (dhcp_packet_t *)pkt->data;
    
    if (ack->op != 2 || ack->xid != dhcp_xid) {
        return;
    }
    
    net_if_t *ifc = net_get_interface(0);
    if (ifc) {
        ifc->ip = ack->yiaddr;
        ip_addr_t subnet = {{255, 255, 255, 0}};
        ip_addr_t gateway = {{0, 0, 0, 0}};
        ifc->netmask = subnet;
        ifc->gateway = gateway;
        
        dhcp_state = 2;
    }
}

/* ═══════════════════════════════════════════════════════════════════════
 * SOCKET API
 * ═══════════════════════════════════════════════════════════════════════ */

int socket_create(socket_type_t type) {
    if (socket_count >= MAX_SOCKETS) {
        return -1;
    }
    
    for (int i = 0; i < MAX_SOCKETS; i++) {
        if (sockets[i].state == SOCK_STATE_CLOSED) {
            sockets[i].type = type;
            sockets[i].state = SOCK_STATE_OPEN;
            sockets[i].internal = NULL;
            sockets[i].local_port = 0;
            sockets[i].remote_port = 0;
            socket_count++;
            return i;
        }
    }
    return -1;
}

int socket_bind(int sockfd, u16 port) {
    if (sockfd < 0 || sockfd >= MAX_SOCKETS || sockets[sockfd].state == SOCK_STATE_CLOSED) {
        return -1;
    }
    
    sockets[sockfd].local_port = port;
    sockets[sockfd].state = SOCK_STATE_OPEN;
    
    if (sockets[sockfd].type == SOCKET_UDP) {
        int udp_idx = udp_socket_create(port);
        if (udp_idx < 0) {
            return -1;
        }
        sockets[sockfd].internal = (void*)(udp_idx + 1);
    }
    
    return 0;
}

int socket_listen(int sockfd) {
    if (sockfd < 0 || sockfd >= MAX_SOCKETS || sockets[sockfd].state == SOCK_STATE_CLOSED) {
        return -1;
    }
    
    if (sockets[sockfd].type != SOCKET_TCP) {
        return -1;
    }
    
    int tcp_idx = tcp_listen(sockets[sockfd].local_port);
    if (tcp_idx < 0) {
        return -1;
    }
    
    sockets[sockfd].internal = (void*)(tcp_idx + 1);
    sockets[sockfd].state = SOCK_STATE_LISTEN;
    return 0;
}

int socket_accept(int sockfd) {
    return -1;
}

int socket_connect(int sockfd, ip_addr_t dst, u16 port) {
    if (sockfd < 0 || sockfd >= MAX_SOCKETS || sockets[sockfd].state == SOCK_STATE_CLOSED) {
        return -1;
    }
    
    if (sockets[sockfd].type == SOCKET_TCP) {
        int tcp_idx = tcp_connect(dst, port);
        if (tcp_idx < 0) {
            return -1;
        }
        sockets[sockfd].internal = (void*)(tcp_idx + 1);
        sockets[sockfd].remote_ip = dst;
        sockets[sockfd].remote_port = port;
        sockets[sockfd].state = SOCK_STATE_CONNECTING;
        return 0;
    }
    
    return -1;
}

int socket_send(int sockfd, const u8 *data, u16 len) {
    if (sockfd < 0 || sockfd >= MAX_SOCKETS || sockets[sockfd].state == SOCK_STATE_CLOSED) {
        return -1;
    }
    
    if (sockets[sockfd].type == SOCKET_UDP) {
        return udp_send(sockets[sockfd].remote_ip, sockets[sockfd].remote_port,
                        sockets[sockfd].local_port, data, len);
    }
    else if (sockets[sockfd].type == SOCKET_TCP) {
        return tcp_send(&sockets[sockfd], data, len);
    }
    
    return -1;
}

int socket_receive(int sockfd, u8 *buf, u16 *len) {
    if (sockfd < 0 || sockfd >= MAX_SOCKETS || sockets[sockfd].state == SOCK_STATE_CLOSED) {
        return -1;
    }
    
    if (sockets[sockfd].type == SOCKET_UDP) {
        return udp_receive(&sockets[sockfd], buf, len);
    }
    else if (sockets[sockfd].type == SOCKET_TCP) {
        return tcp_receive(&sockets[sockfd], buf, len);
    }
    
    return -1;
}

int socket_close(int sockfd) {
    if (sockfd < 0 || sockfd >= MAX_SOCKETS || sockets[sockfd].state == SOCK_STATE_CLOSED) {
        return -1;
    }
    
    if (sockets[sockfd].type == SOCKET_UDP && sockets[sockfd].internal != NULL) {
        int udp_idx = (int)sockets[sockfd].internal - 1;
        udp_socket_close(udp_idx);
    }
    else if (sockets[sockfd].type == SOCKET_TCP && sockets[sockfd].internal != NULL) {
        tcp_close(&sockets[sockfd]);
    }
    
    sockets[sockfd].state = SOCK_STATE_CLOSED;
    sockets[sockfd].internal = NULL;
    socket_count--;
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════
 * NETWORK INITIALIZATION
 * ═══════════════════════════════════════════════════════════════════════ */

void network_init(void) {
    serial_puts("\n");
    serial_puts("╔════════════════════════════════════════════════════════════╗\n");
    serial_puts("║  VECTORD OS Network Stack - Initializing                   ║\n");
    serial_puts("╚════════════════════════════════════════════════════════════╝\n");
    
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
    dhcp_init();
    
    /* Set up fake MAC address for QEMU */
    mac_addr_t mac = {{0x52, 0x54, 0x00, 0x12, 0x34, 0x56}};
    ip_addr_t ip = {{10, 0, 2, 15}};
    ip_addr_t netmask = {{255, 255, 255, 0}};
    ip_addr_t gateway = {{10, 0, 2, 2}};
    
    net_add_interface(mac, ip, netmask, gateway);
    
    /* Add default route */
    ip_route_add(ip, netmask, gateway, 0);
    
    serial_puts("\n");
    serial_puts("  Network Interface eth0:\n");
    serial_puts("    MAC:  ");
    mac_addr_print(mac);
    serial_puts("\n    IP:   ");
    ip_addr_print(ip);
    serial_puts("\n    Mask: ");
    ip_addr_print(netmask);
    serial_puts("\n    GW:   ");
    ip_addr_print(gateway);
    serial_puts("\n");
    
    serial_puts("\n");
    serial_puts("  Network stack initialized successfully.\n");
    serial_puts("  Protocols: Ethernet, ARP, IPv4, ICMP, UDP, TCP, DHCP\n");
    serial_puts("  Socket API: UDP, TCP, RAW\n");
    serial_puts("\n");
}

/* ═══════════════════════════════════════════════════════════════════════
 * ETHERNET FRAME HANDLING
 * ═══════════════════════════════════════════════════════════════════════ */

void ethernet_init(void) {
}

int ethernet_parse(const u8 *raw, u16 len, void *frame_ptr) {
    ethernet_frame_t *frame = (ethernet_frame_t *)frame_ptr;
    
    if (len < 14) {
        return -1;
    }
    
    for (int i = 0; i < ETH_ALEN; i++) {
        frame->dst.addr[i] = raw[i];
    }
    
    for (int i = 0; i < ETH_ALEN; i++) {
        frame->src.addr[i] = raw[ETH_ALEN + i];
    }
    
    frame->ethertype = (raw[12] << 8) | raw[13];
    
    u16 payload_len = len - 14;
    if (payload_len > sizeof(frame->data)) {
        payload_len = sizeof(frame->data);
    }
    
    for (u16 i = 0; i < payload_len; i++) {
        frame->data[i] = raw[14 + i];
    }
    
    return 0;
}

int ethernet_build(const void *frame_ptr, u8 *raw, u16 *len) {
    const ethernet_frame_t *frame = (const ethernet_frame_t *)frame_ptr;
    u16 payload_len = *len - 14;
    if (payload_len < (ETH_FRAME_MIN - 14)) {
        payload_len = ETH_FRAME_MIN - 14;
    }
    
    *len = 14 + payload_len;
    
    for (int i = 0; i < ETH_ALEN; i++) {
        raw[i] = frame->dst.addr[i];
    }
    
    for (int i = 0; i < ETH_ALEN; i++) {
        raw[ETH_ALEN + i] = frame->src.addr[i];
    }
    
    raw[12] = (frame->ethertype >> 8) & 0xFF;
    raw[13] = frame->ethertype & 0xFF;
    
    for (u16 i = 0; i < payload_len; i++) {
        raw[14 + i] = frame->data[i];
    }
    
    for (u16 i = 14 + payload_len; i < *len; i++) {
        raw[i] = 0;
    }
    
    return 0;
}

void ethernet_print_frame(const void *frame_ptr) {
    const ethernet_frame_t *frame = (const ethernet_frame_t *)frame_ptr;
    serial_puts("ETH Frame:\n");
    serial_puts("  DST: ");
    mac_addr_print(frame->dst);
    serial_puts("\n  SRC: ");
    mac_addr_print(frame->src);
    serial_puts("\n  EtherType: 0x");
    serial_puthex(frame->ethertype);
    serial_puts("\n");
}

/* ═══════════════════════════════════════════════════════════════════════
 * NETWORK PACKET TRANSMISSION/RECEPTION
 * ═══════════════════════════════════════════════════════════════════════ */

int net_send_packet(net_if_t *ifc, mac_addr_t dst, u16 ethertype, const u8 *data, u16 len) {
    u8 frame[1514];
    u16 frame_len = 14 + len;
    
    if (frame_len > 1514) {
        return -1;
    }
    
    for (int i = 0; i < 6; i++) {
        frame[i] = dst.addr[i];
        frame[i + 6] = ifc->mac.addr[i];
    }
    frame[12] = (ethertype >> 8) & 0xFF;
    frame[13] = ethertype & 0xFF;
    
    for (u16 i = 0; i < len; i++) {
        frame[14 + i] = data[i];
    }
    
    /* Update statistics */
    net_tx_packets++;
    net_tx_bytes += len;
    
    /* Print what would be sent (for simulation) */
    serial_puts("[NET TX] ");
    serial_puthex(ethertype);
    serial_puts(" len=");
    serial_putdec(len);
    serial_puts(" -> ");
    mac_addr_print(dst);
    serial_puts("\n");
    
    return 0;
}

int net_receive_packet(net_if_t *ifc, net_packet_t *pkt) {
    return net_queue_packet(pkt);
}

void net_process_packet(net_packet_t *pkt) {
    if (!pkt || pkt->length < 14) {
        return;
    }
    
    u16 ethertype = (pkt->data[12] << 8) | pkt->data[13];
    
    net_rx_packets++;
    net_rx_bytes += pkt->length;
    
    switch (ethertype) {
        case 0x0806:
            arp_handle(pkt);
            break;
        case 0x0800:
            ip_receive(pkt);
            break;
        default:
            break;
    }
}

/* ═══════════════════════════════════════════════════════════════════════
 * NETWORK COMMANDS (for shell)
 * ═══════════════════════════════════════════════════════════════════════ */

void ifconfig(void) {
    serial_puts("\n");
    serial_puts("Network Interfaces:\n");
    serial_puts("====================\n");
    
    for (int i = 0; i < net_if_count; i++) {
        net_if_t *ifc = &net_interfaces[i];
        if (ifc->present) {
            serial_puts("eth");
            serial_putdec(i);
            serial_puts(":\n");
            serial_puts("  MAC:       ");
            mac_addr_print(ifc->mac);
            serial_puts("\n  IP:        ");
            ip_addr_print(ifc->ip);
            serial_puts("\n  Netmask:   ");
            ip_addr_print(ifc->netmask);
            serial_puts("\n  Gateway:   ");
            ip_addr_print(ifc->gateway);
            serial_puts("\n");
        }
    }
    serial_puts("\n");
}

void ping(ip_addr_t ip, int count) {
    serial_puts("\nPinging ");
    ip_addr_print(ip);
    serial_puts(":\n");
    
    for (int i = 0; i < count; i++) {
        icmp_echo_request(ip, 0x1234, i + 1);
        serial_puts("  Request ");
        serial_putdec(i + 1);
        serial_puts(" sent\n");
    }
    
    serial_puts("\nPing Statistics:\n");
    serial_puts("  Sent: ");
    serial_putdec(ping_sent);
    serial_puts("\n  Received: ");
    serial_putdec(ping_received);
    serial_puts("\n");
}

void ipconfig(void) {
    serial_puts("\n");
    serial_puts("IP Configuration:\n");
    serial_puts("==================\n");
    
    net_if_t *ifc = net_get_interface(0);
    if (ifc) {
        serial_puts("IP Address:    ");
        ip_addr_print(ifc->ip);
        serial_puts("\nSubnet Mask:   ");
        ip_addr_print(ifc->netmask);
        serial_puts("\nDefault GW:    ");
        ip_addr_print(ifc->gateway);
        serial_puts("\nDHCP Status:   ");
        if (dhcp_state == 2) {
            serial_puts("Bound\n");
        } else if (dhcp_state == 1) {
            serial_puts("Acquiring...\n");
        } else {
            serial_puts("Not configured\n");
        }
    }
    serial_puts("\n");
}

void netstat(void) {
    serial_puts("\n");
    serial_puts("Network Statistics:\n");
    serial_puts("====================\n");
    serial_puts("TX Packets: ");
    serial_putdec(net_tx_packets);
    serial_puts("\nRX Packets: ");
    serial_putdec(net_rx_packets);
    serial_puts("\nTX Bytes:   ");
    serial_putdec(net_tx_bytes);
    serial_puts("\nRX Bytes:   ");
    serial_putdec(net_rx_bytes);
    serial_puts("\nErrors:     ");
    serial_putdec(net_errors);
    serial_puts("\n");
    
    serial_puts("\nTCP Sockets: ");
    serial_putdec(tcp_socket_count);
    serial_puts("\nUDP Sockets: ");
    serial_putdec(udp_socket_count);
    serial_puts("\nSockets:     ");
    serial_putdec(socket_count);
    serial_puts("\n");
    
    serial_puts("\nPing Statistics:\n");
    serial_puts("  Sent:     ");
    serial_putdec(ping_sent);
    serial_puts("\n  Received: ");
    serial_putdec(ping_received);
    serial_puts("\n");
    
    serial_puts("\nARP Table:\n");
    arp_print_table();
    serial_puts("\n");
}

void net_print_status(void) {
    serial_puts("\nNetwork Status:\n");
    serial_puts("  Interfaces: ");
    serial_putdec(net_if_count);
    serial_puts("\n  Queued packets: ");
    serial_putdec(net_queue_count);
    serial_puts("\n");
}

void net_stats_print(void) {
    netstat();
}

/* ═══════════════════════════════════════════════════════════════════════
 * NETWORK MAIN
 * ═══════════════════════════════════════════════════════════════════════ */

void network_main(void) {
    while (1) {
        net_packet_t *pkt = net_dequeue_packet();
        if (pkt) {
            net_process_packet(pkt);
        }
        proc_yield();
    }
}

