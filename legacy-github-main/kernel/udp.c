/* ═══════════════════════════════════════════════════════════════════════
 * VECTORD OS - UDP Protocol
 * 
 * User Datagram Protocol - Connectionless sockets
 * ═══════════════════════════════════════════════════════════════════════ */

#include "../include/vecord.h"

/* ═══════════════════════════════════════════════════════════════════════
 * UDP CONSTANTS
 * ═══════════════════════════════════════════════════════════════════════ */

#define UDP_MAX_SOCKETS  16
#define IP_PROTO_UDP     17

/* ═══════════════════════════════════════════════════════════════════════
 * UDP HEADER STRUCTURE
 * ═══════════════════════════════════════════════════════════════════════ */

typedef struct {
    u16 src_port;
    u16 dst_port;
    u16 length;
    u16 checksum;
} __attribute__((packed)) udp_header_t;

/* ═══════════════════════════════════════════════════════════════════════
 * UDP SOCKET (internal)
 * ═══════════════════════════════════════════════════════════════════════ */

typedef struct {
    u16 local_port;
    ip_addr_t remote_ip;
    u16 remote_port;
    u8 active;
    /* Simple receive buffer */
    u8 buffer[1024];
    u16 buf_len;
} udp_socket_internal_t;

/* ═══════════════════════════════════════════════════════════════════════
 * GLOBAL STATE
 * ═══════════════════════════════════════════════════════════════════════ */

static udp_socket_internal_t udp_sockets[UDP_MAX_SOCKETS];
static int udp_socket_count = 0;

/* ═══════════════════════════════════════════════════════════════════════
 * UDP INITIALIZATION
 * ═══════════════════════════════════════════════════════════════════════ */

void udp_init(void) {
    serial_puts("UDP: UDP stack initialized\n");
    
    for (int i = 0; i < UDP_MAX_SOCKETS; i++) {
        udp_sockets[i].active = 0;
    }
    udp_socket_count = 0;
}

/* ═══════════════════════════════════════════════════════════════════════
 * UDP CHECKSUM (optional, often left to upper layers)
 * ═══════════════════════════════════════════════════════════════════════ */

static u16 udp_checksum(const u8 *data, u16 len, ip_addr_t src, ip_addr_t dst) {
    /* Pseudo header */
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
    
    /* Sum pseudo header */
    for (int i = 0; i < 12; i += 2) {
        sum += (pseudo[i] << 8) | pseudo[i + 1];
    }
    
    /* Sum UDP packet */
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
 * UDP SEND (as per vecord.h)
 * ═══════════════════════════════════════════════════════════════════════ */

int udp_send(ip_addr_t dst, u16 dst_port, u16 src_port, const u8 *data, u16 len) {
    /* Build UDP packet */
    u8 packet[1500];
    udp_header_t *hdr = (udp_header_t *)packet;
    
    if (len > sizeof(packet) - sizeof(udp_header_t)) {
        return -1;
    }
    
    hdr->src_port = htons(src_port);
    hdr->dst_port = htons(dst_port);
    hdr->length = htons(sizeof(udp_header_t) + len);
    hdr->checksum = 0;
    
    /* Copy payload */
    for (u16 i = 0; i < len; i++) {
        packet[sizeof(udp_header_t) + i] = data[i];
    }
    
    /* Calculate checksum (optional) */
    net_if_t *ifc = net_get_interface(0);
    if (ifc) {
        hdr->checksum = udp_checksum(packet, sizeof(udp_header_t) + len, ifc->ip, dst);
    }
    
    /* Send via IP */
    return ip_send(dst, IP_PROTO_UDP, packet, sizeof(udp_header_t) + len);
}

/* ═══════════════════════════════════════════════════════════════════════
 * UDP RECEIVE (as per vecord.h) - read from socket buffer
 * ═══════════════════════════════════════════════════════════════════════ */

int udp_receive(socket_t *sock, u8 *buf, u16 *len) {
    /* This is a socket API function; we need to map socket_t to internal socket */
    /* For simplicity, assume sock->local_port maps to internal socket */
    /* We'll need a mapping mechanism; for now, we'll use the socket's local_port */
    
    /* Find internal socket by local port */
    for (int i = 0; i < UDP_MAX_SOCKETS; i++) {
        if (udp_sockets[i].active && udp_sockets[i].local_port == sock->local_port) {
            if (udp_sockets[i].buf_len == 0) {
                return 0; /* No data */
            }
            
            u16 copy_len = udp_sockets[i].buf_len;
            if (copy_len > *len) {
                copy_len = *len;
            }
            
            for (u16 j = 0; j < copy_len; j++) {
                buf[j] = udp_sockets[i].buffer[j];
            }
            *len = copy_len;
            
            /* Clear buffer */
            udp_sockets[i].buf_len = 0;
            
            return copy_len;
        }
    }
    
    return -1;
}

/* ═══════════════════════════════════════════════════════════════════════
 * UDP PROCESS PACKET (called from IP layer)
 * ═══════════════════════════════════════════════════════════════════════ */

void udp_process_packet(net_packet_t *pkt, ip_addr_t src) {
    if (pkt->length < sizeof(udp_header_t)) {
        return;
    }
    
    udp_header_t *hdr = (udp_header_t *)pkt->data;
    
    u16 dst_port = ntohs(hdr->dst_port);
    u16 src_port = ntohs(hdr->src_port);
    u16 length = ntohs(hdr->length);
    
    /* Find socket bound to this port */
    for (int i = 0; i < UDP_MAX_SOCKETS; i++) {
        if (udp_sockets[i].active && udp_sockets[i].local_port == dst_port) {
            /* Store packet in socket buffer */
            u16 copy_len = length - sizeof(udp_header_t);
            if (copy_len > sizeof(udp_sockets[i].buffer)) {
                copy_len = sizeof(udp_sockets[i].buffer);
            }
            
            for (u16 j = 0; j < copy_len; j++) {
                udp_sockets[i].buffer[j] = pkt->data[sizeof(udp_header_t) + j];
            }
            udp_sockets[i].buf_len = copy_len;
            
            /* Store remote info */
            udp_sockets[i].remote_ip = src;
            udp_sockets[i].remote_port = src_port;
            
            serial_puts("UDP: Received ");
            serial_putdec(copy_len);
            serial_puts(" bytes on port ");
            serial_putdec(dst_port);
            serial_puts("\n");
            return;
        }
    }
    
    serial_puts("UDP: No socket listening on port ");
    serial_putdec(dst_port);
    serial_puts("\n");
}

/* ═══════════════════════════════════════════════════════════════════════
 * UDP SOCKET MANAGEMENT (for socket API)
 * ═══════════════════════════════════════════════════════════════════════ */

int udp_socket_create(u16 port) {
    if (udp_socket_count >= UDP_MAX_SOCKETS) {
        return -1;
    }
    
    /* Find free slot */
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
