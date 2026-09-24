/* ═══════════════════════════════════════════════════════════════════════
 * VECTORD OS - Ethernet Frame Handling
 * 
 * Ethernet frame encoding and decoding
 * ═══════════════════════════════════════════════════════════════════════ */

#include "../include/vecord.h"

/* ═══════════════════════════════════════════════════════════════════════
 * ETHERNET CONSTANTS
 * ═══════════════════════════════════════════════════════════════════════ */

#define ETH_ALEN       6
#define ETH_FRAME_MIN  64
#define ETH_FRAME_MAX  1518

/* EtherTypes */
#define ETH_P_IP       0x0800
#define ETH_P_ARP      0x0806
#define ETH_P_IPV6     0x86DD

/* ═══════════════════════════════════════════════════════════════════════
 * ETHERNET FRAME STRUCTURE
 * ═══════════════════════════════════════════════════════════════════════ */

typedef struct {
    mac_addr_t dst;
    mac_addr_t src;
    u16 ethertype;
    u8 data[ETH_FRAME_MAX - 14]; /* Payload */
} __attribute__((packed)) ethernet_frame_t;

/* ═══════════════════════════════════════════════════════════════════════
 * ETHERNET FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════ */

void ethernet_init(void) {
    serial_puts("ETH: Ethernet layer initialized\n");
}

/* Parse Ethernet frame from raw data */
int ethernet_parse(const u8 *raw, u16 len, ethernet_frame_t *frame) {
    if (len < 14) {
        return -1; /* Too short */
    }
    
    /* Extract destination MAC */
    for (int i = 0; i < ETH_ALEN; i++) {
        frame->dst.addr[i] = raw[i];
    }
    
    /* Extract source MAC */
    for (int i = 0; i < ETH_ALEN; i++) {
        frame->src.addr[i] = raw[ETH_ALEN + i];
    }
    
    /* Extract EtherType */
    frame->ethertype = (raw[12] << 8) | raw[13];
    
    /* Copy payload */
    u16 payload_len = len - 14;
    if (payload_len > sizeof(frame->data)) {
        payload_len = sizeof(frame->data);
    }
    
    for (u16 i = 0; i < payload_len; i++) {
        frame->data[i] = raw[14 + i];
    }
    
    return 0;
}

/* Build Ethernet frame into raw buffer */
int ethernet_build(const ethernet_frame_t *frame, u8 *raw, u16 *len) {
    /* Minimum payload size for Ethernet (padding if needed) */
    u16 payload_len = *len - 14;
    if (payload_len < (ETH_FRAME_MIN - 14)) {
        payload_len = ETH_FRAME_MIN - 14;
    }
    
    *len = 14 + payload_len;
    
    /* Destination MAC */
    for (int i = 0; i < ETH_ALEN; i++) {
        raw[i] = frame->dst.addr[i];
    }
    
    /* Source MAC */
    for (int i = 0; i < ETH_ALEN; i++) {
        raw[ETH_ALEN + i] = frame->src.addr[i];
    }
    
    /* EtherType */
    raw[12] = (frame->ethertype >> 8) & 0xFF;
    raw[13] = frame->ethertype & 0xFF;
    
    /* Payload */
    for (u16 i = 0; i < payload_len; i++) {
        raw[14 + i] = frame->data[i];
    }
    
    /* Pad with zeros if needed */
    for (u16 i = 14 + payload_len; i < *len; i++) {
        raw[i] = 0;
    }
    
    return 0;
}

/* Calculate Ethernet CRC (optional, hardware usually does this) */
u32 ethernet_crc(const u8 *data, u16 len) {
    u32 crc = 0xFFFFFFFF;
    
    for (u16 i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xEDB88320;
            } else {
                crc >>= 1;
            }
        }
    }
    
    return ~crc;
}

/* Print Ethernet frame info */
void ethernet_print_frame(const ethernet_frame_t *frame) {
    serial_puts("ETH Frame:\n");
    serial_puts("  DST: ");
    for (int i = 0; i < ETH_ALEN; i++) {
        serial_puthex(frame->dst.addr[i]);
        if (i < ETH_ALEN - 1) serial_putc(':');
    }
    serial_puts("\n");
    serial_puts("  SRC: ");
    for (int i = 0; i < ETH_ALEN; i++) {
        serial_puthex(frame->src.addr[i]);
        if (i < ETH_ALEN - 1) serial_putc(':');
    }
    serial_puts("\n");
    serial_puts("  EtherType: 0x");
    serial_puthex(frame->ethertype);
    serial_puts("\n");
}
