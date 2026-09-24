/* ═══════════════════════════════════════════════════════════════════════
 * VECTORD OS - TCP Protocol
 * 
 * Transmission Control Protocol - Reliable, connection-oriented sockets
 * Simplified implementation with SYN/ACK/FIN support
 * ═══════════════════════════════════════════════════════════════════════ */

#include "../include/vecord.h"

/* ═══════════════════════════════════════════════════════════════════════
 * TCP CONSTANTS
 * ═══════════════════════════════════════════════════════════════════════ */

#define TCP_MAX_SOCKETS  16
#define IP_PROTO_TCP     6

/* TCP flags */
#define TCP_FIN  0x01
#define TCP_SYN  0x02
#define TCP_RST  0x04
#define TCP_PSH  0x08
#define TCP_ACK  0x10
#define TCP_URG  0x20

/* TCP states */
#define TCP_CLOSED       0
#define TCP_LISTEN       1
#define TCP_SYN_SENT     2
#define TCP_SYN_RECEIVED 3
#define TCP_ESTABLISHED  4
#define TCP_FIN_WAIT1    5
#define TCP_FIN_WAIT2    6
#define TCP_CLOSE_WAIT   7
#define TCP_CLOSING      8
#define TCP_LAST_ACK     9
#define TCP_TIME_WAIT   10

/* ═══════════════════════════════════════════════════════════════════════
 * TCP HEADER STRUCTURE
 * ═══════════════════════════════════════════════════════════════════════ */

typedef struct {
    u16 src_port;
    u16 dst_port;
    u32 seq;
    u32 ack;
    u8  data_offset;    /* Data offset (4 bits) + reserved (4 bits) */
    u8  flags;
    u16 window;
    u16 checksum;
    u16 urgent;
} __attribute__((packed)) tcp_header_t;

/* ═══════════════════════════════════════════════════════════════════════
 * TCP SOCKET
 * ═══════════════════════════════════════════════════════════════════════ */

typedef struct {
    u16 local_port;
    ip_addr_t remote_ip;
    u16 remote_port;
    u8 state;
    u32 seq;
    u32 ack;
    /* Simple send/receive buffers */
    u8 send_buf[1024];
    u16 send_len;
    u8 recv_buf[1024];
    u16 recv_len;
    u8 active;
} tcp_socket_t;

/* ═══════════════════════════════════════════════════════════════════════
 * GLOBAL STATE
 * ═══════════════════════════════════════════════════════════════════════ */

static tcp_socket_t tcp_sockets[TCP_MAX_SOCKETS];
static int tcp_socket_count = 0;

/* ═══════════════════════════════════════════════════════════════════════
 * TCP INITIALIZATION
 * ═══════════════════════════════════════════════════════════════════════ */

void tcp_init(void) {
    serial_puts("TCP: TCP stack initialized\n");
    
    for (int i = 0; i < TCP_MAX_SOCKETS; i++) {
        tcp_sockets[i].active = 0;
        tcp_sockets[i].state = TCP_CLOSED;
    }
    tcp_socket_count = 0;
}

/* ═══════════════════════════════════════════════════════════════════════
 * TCP CHECKSUM
 * ═══════════════════════════════════════════════════════════════════════ */

static u16 tcp_checksum(const u8 *data, u16 len, ip_addr_t src, ip_addr_t dst) {
    /* Pseudo header */
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
    
    /* Sum pseudo header */
    for (int i = 0; i < 12; i += 2) {
        sum += (pseudo[i] << 8) | pseudo[i + 1];
    }
    
    /* Sum TCP packet */
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
 * TCP SEND PACKET
 * ═══════════════════════════════════════════════════════════════════════ */

static int tcp_send_packet(tcp_socket_t *sock, u8 flags, const u8 *data, u16 data_len) {
    /* Build TCP packet */
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
    hdr->window = htons(1024); /* Fixed window size */
    hdr->checksum = 0;
    hdr->urgent = 0;
    
    /* Copy data */
    if (data && data_len > 0) {
        for (u16 i = 0; i < data_len; i++) {
            packet[sizeof(tcp_header_t) + i] = data[i];
        }
    }
    
    /* Calculate checksum */
    net_if_t *ifc = net_get_interface(0);
    if (ifc) {
        hdr->checksum = tcp_checksum(packet, sizeof(tcp_header_t) + data_len, ifc->ip, sock->remote_ip);
    }
    
    /* Send via IP */
    return ip_send(sock->remote_ip, IP_PROTO_TCP, packet, sizeof(tcp_header_t) + data_len);
}

/* ═══════════════════════════════════════════════════════════════════════
 * TCP CONNECT (as per vecord.h)
 * ═══════════════════════════════════════════════════════════════════════ */

int tcp_connect(ip_addr_t dst, u16 dst_port) {
    /* Find free socket slot */
    for (int i = 0; i < TCP_MAX_SOCKETS; i++) {
        if (!tcp_sockets[i].active) {
            tcp_sockets[i].local_port = 1024 + i; /* Simple port assignment */
            tcp_sockets[i].remote_ip = dst;
            tcp_sockets[i].remote_port = dst_port;
            tcp_sockets[i].state = TCP_CLOSED;
            tcp_sockets[i].seq = 0; /* Should be random */
            tcp_sockets[i].ack = 0;
            tcp_sockets[i].send_len = 0;
            tcp_sockets[i].recv_len = 0;
            tcp_sockets[i].active = 1;
            tcp_socket_count++;
            
            /* Send SYN */
            tcp_sockets[i].state = TCP_SYN_SENT;
            tcp_send_packet(&tcp_sockets[i], TCP_SYN, NULL, 0);
            
            serial_puts("TCP: Connecting to ");
            ip_addr_print(dst);
            serial_puts(":");
            serial_putdec(dst_port);
            serial_puts("\n");
            
            return i;
        }
    }
    return -1;
}

/* ═══════════════════════════════════════════════════════════════════════
 * TCP LISTEN (as per vecord.h)
 * ═══════════════════════════════════════════════════════════════════════ */

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
            
            serial_puts("TCP: Listening on port ");
            serial_putdec(port);
            serial_puts("\n");
            
            return i;
        }
    }
    return -1;
}

/* ═══════════════════════════════════════════════════════════════════════
 * TCP SEND DATA (as per vecord.h)
 * ═══════════════════════════════════════════════════════════════════════ */

int tcp_send(socket_t *sock, const u8 *data, u16 len) {
    /* Find socket by local port (simplified mapping) */
    for (int i = 0; i < TCP_MAX_SOCKETS; i++) {
        if (tcp_sockets[i].active && tcp_sockets[i].local_port == sock->local_port) {
            if (tcp_sockets[i].state != TCP_ESTABLISHED) {
                return -1;
            }
            
            /* Send data with PSH/ACK flags */
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

/* ═══════════════════════════════════════════════════════════════════════
 * TCP RECEIVE DATA (as per vecord.h)
 * ═══════════════════════════════════════════════════════════════════════ */

int tcp_receive(socket_t *sock, u8 *buf, u16 *len) {
    for (int i = 0; i < TCP_MAX_SOCKETS; i++) {
        if (tcp_sockets[i].active && tcp_sockets[i].local_port == sock->local_port) {
            if (tcp_sockets[i].recv_len == 0) {
                return 0; /* No data */
            }
            
            u16 copy_len = tcp_sockets[i].recv_len;
            if (copy_len > *len) {
                copy_len = *len;
            }
            
            for (u16 j = 0; j < copy_len; j++) {
                buf[j] = tcp_sockets[i].recv_buf[j];
            }
            *len = copy_len;
            
            /* Shift remaining data */
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

/* ═══════════════════════════════════════════════════════════════════════
 * TCP CLOSE (as per vecord.h)
 * ═══════════════════════════════════════════════════════════════════════ */

void tcp_close(socket_t *sock) {
    for (int i = 0; i < TCP_MAX_SOCKETS; i++) {
        if (tcp_sockets[i].active && tcp_sockets[i].local_port == sock->local_port) {
            /* Send FIN */
            tcp_send_packet(&tcp_sockets[i], TCP_FIN | TCP_ACK, NULL, 0);
            tcp_sockets[i].state = TCP_FIN_WAIT1;
            serial_puts("TCP: Closing connection on port ");
            serial_putdec(sock->local_port);
            serial_puts("\n");
            return;
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════════
 * TCP PROCESS PACKET (called from IP layer)
 * ═══════════════════════════════════════════════════════════════════════ */

void tcp_process_packet(net_packet_t *pkt, ip_addr_t src) {
    if (pkt->length < sizeof(tcp_header_t)) {
        return;
    }
    
    tcp_header_t *hdr = (tcp_header_t *)pkt->data;
    
    u16 dst_port = ntohs(hdr->dst_port);
    u16 src_port = ntohs(hdr->src_port);
    u8 flags = hdr->flags;
    
    /* Find socket for this destination port */
    for (int i = 0; i < TCP_MAX_SOCKETS; i++) {
        if (tcp_sockets[i].active && tcp_sockets[i].local_port == dst_port) {
            /* Update remote info */
            tcp_sockets[i].remote_ip = src;
            tcp_sockets[i].remote_port = src_port;
            
            /* Process based on state and flags */
            switch (tcp_sockets[i].state) {
                case TCP_LISTEN:
                    if (flags & TCP_SYN) {
                        /* Respond with SYN-ACK */
                        tcp_sockets[i].ack = ntohl(hdr->seq) + 1;
                        tcp_send_packet(&tcp_sockets[i], TCP_SYN | TCP_ACK, NULL, 0);
                        tcp_sockets[i].state = TCP_SYN_RECEIVED;
                        serial_puts("TCP: SYN received on listening port\n");
                    }
                    break;
                    
                case TCP_SYN_SENT:
                    if ((flags & (TCP_SYN | TCP_ACK)) == (TCP_SYN | TCP_ACK)) {
                        /* Connection established */
                        tcp_sockets[i].ack = ntohl(hdr->seq) + 1;
                        tcp_sockets[i].seq++; /* Our SYN counted as one */
                        tcp_send_packet(&tcp_sockets[i], TCP_ACK, NULL, 0);
                        tcp_sockets[i].state = TCP_ESTABLISHED;
                        serial_puts("TCP: Connection established\n");
                    }
                    break;
                    
                case TCP_SYN_RECEIVED:
                    if (flags & TCP_ACK) {
                        tcp_sockets[i].state = TCP_ESTABLISHED;
                        serial_puts("TCP: Connection fully established\n");
                    }
                    break;
                    
                case TCP_ESTABLISHED:
                    if (flags & TCP_FIN) {
                        /* Remote wants to close */
                        tcp_sockets[i].ack = ntohl(hdr->seq) + 1;
                        tcp_send_packet(&tcp_sockets[i], TCP_ACK, NULL, 0);
                        tcp_sockets[i].state = TCP_CLOSE_WAIT;
                        serial_puts("TCP: Received FIN\n");
                    } else if (pkt->length > sizeof(tcp_header_t)) {
                        /* Data received */
                        u16 data_len = pkt->length - sizeof(tcp_header_t);
                        if (data_len > sizeof(tcp_sockets[i].recv_buf) - tcp_sockets[i].recv_len) {
                            data_len = sizeof(tcp_sockets[i].recv_buf) - tcp_sockets[i].recv_len;
                        }
                        
                        for (u16 j = 0; j < data_len; j++) {
                            tcp_sockets[i].recv_buf[tcp_sockets[i].recv_len + j] = pkt->data[sizeof(tcp_header_t) + j];
                        }
                        tcp_sockets[i].recv_len += data_len;
                        
                        /* Update ACK */
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
                        serial_puts("TCP: Connection closed\n");
                    }
                    break;
                    
                case TCP_CLOSE_WAIT:
                    /* Waiting for application to close */
                    break;
                    
                default:
                    break;
            }
            return;
        }
    }
    
    serial_puts("TCP: No socket listening on port ");
    serial_putdec(dst_port);
    serial_puts("\n");
}