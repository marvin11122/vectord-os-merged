/* ═══════════════════════════════════════════════════════════════════════
 * VECTORD OS - ICMP Protocol
 * 
 * Internet Control Message Protocol - Ping support
 * ═══════════════════════════════════════════════════════════════════════ */

#include "../include/vecord.h"

/* ═══════════════════════════════════════════════════════════════════════
 * ICMP CONSTANTS
 * ═══════════════════════════════════════════════════════════════════════ */

#define ICMP_ECHO_REQUEST   8
#define ICMP_ECHO_REPLY     0

#define IP_PROTO_ICMP   1

/* ═══════════════════════════════════════════════════════════════════════
 * ICMP HEADER STRUCTURE
 * ═══════════════════════════════════════════════════════════════════════ */

typedef struct {
    u8  type;
    u8  code;
    u16 checksum;
    u16 id;
    u16 sequence;
} __attribute__((packed)) icmp_header_t;

/* ═══════════════════════════════════════════════════════════════════════
 * ICMP INITIALIZATION
 * ═══════════════════════════════════════════════════════════════════════ */

void icmp_init(void) {
    serial_puts("ICMP: ICMP protocol initialized\n");
}

/* ═══════════════════════════════════════════════════════════════════════
 * ICMP CHECKSUM
 * ═══════════════════════════════════════════════════════════════════════ */

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

/* ═══════════════════════════════════════════════════════════════════════
 * ICMP ECHO REQUEST (Ping)
 * ═══════════════════════════════════════════════════════════════════════ */

void icmp_echo_request(ip_addr_t dst, u16 id, u16 seq) {
    /* Build ICMP echo request packet */
    u8 packet[64];
    icmp_header_t *hdr = (icmp_header_t *)packet;
    
    hdr->type = ICMP_ECHO_REQUEST;
    hdr->code = 0;
    hdr->checksum = 0;
    hdr->id = htons(id);
    hdr->sequence = htons(seq);
    
    /* Fill payload with pattern */
    for (int i = sizeof(icmp_header_t); i < sizeof(packet); i++) {
        packet[i] = i;
    }
    
    /* Calculate checksum */
    hdr->checksum = icmp_checksum(packet, sizeof(packet));
    
    /* Send via IP */
    ip_send(dst, IP_PROTO_ICMP, packet, sizeof(packet));
    
    serial_puts("ICMP: Echo request to ");
    ip_addr_print(dst);
    serial_puts(", id=");
    serial_putdec(id);
    serial_puts(", seq=");
    serial_putdec(seq);
    serial_puts("\n");
}

/* ═══════════════════════════════════════════════════════════════════════
 * ICMP ECHO REPLY
 * ═══════════════════════════════════════════════════════════════════════ */

void icmp_echo_reply(ip_addr_t src, u16 id, u16 seq) {
    /* Build ICMP echo reply packet */
    u8 packet[64];
    icmp_header_t *hdr = (icmp_header_t *)packet;
    
    hdr->type = ICMP_ECHO_REPLY;
    hdr->code = 0;
    hdr->checksum = 0;
    hdr->id = htons(id);
    hdr->sequence = htons(seq);
    
    /* Fill payload with pattern */
    for (int i = sizeof(icmp_header_t); i < sizeof(packet); i++) {
        packet[i] = i;
    }
    
    /* Calculate checksum */
    hdr->checksum = icmp_checksum(packet, sizeof(packet));
    
    /* Send via IP */
    ip_send(src, IP_PROTO_ICMP, packet, sizeof(packet));
    
    serial_puts("ICMP: Echo reply to ");
    ip_addr_print(src);
    serial_puts(", id=");
    serial_putdec(id);
    serial_puts(", seq=");
    serial_putdec(seq);
    serial_puts("\n");
}

/* ═══════════════════════════════════════════════════════════════════════
 * ICMP PACKET HANDLING
 * ═══════════════════════════════════════════════════════════════════════ */

void icmp_handle(net_packet_t *pkt) {
    if (pkt->length < sizeof(icmp_header_t)) {
        return;
    }
    
    icmp_header_t *hdr = (icmp_header_t *)pkt->data;
    
    switch (hdr->type) {
        case ICMP_ECHO_REQUEST:
            /* Reply to ping */
            {
                u16 id = ntohs(hdr->id);
                u16 seq = ntohs(hdr->sequence);
                
                /* Get source IP from IP header (assuming pkt points to IP payload) */
                /* For simplicity, we'll need the source IP from the IP header */
                /* This is a simplification; in real code we'd parse the IP header */
                ip_addr_t src = {0}; /* TODO: extract from IP header */
                
                icmp_echo_reply(src, id, seq);
            }
            break;
        case ICMP_ECHO_REPLY:
            serial_puts("ICMP: Echo reply received\n");
            break;
        default:
            serial_puts("ICMP: Unknown type ");
            serial_putdec(hdr->type);
            serial_puts("\n");
            break;
    }
}
