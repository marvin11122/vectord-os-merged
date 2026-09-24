/* ═══════════════════════════════════════════════════════════════════════
 * VECTORD OS - ARP Protocol
 * 
 * Address Resolution Protocol - Maps IP addresses to MAC addresses
 * ═══════════════════════════════════════════════════════════════════════ */

#include "../include/vecord.h"

/* ═══════════════════════════════════════════════════════════════════════
 * ARP CONSTANTS
 * ═══════════════════════════════════════════════════════════════════════ */

#define ARP_REQUEST     1
#define ARP_REPLY       2

#define ARP_HW_ETHER    1
#define ARP_PROTO_IP    0x0800

/* ARP table size */
#define ARP_TABLE_SIZE  16

/* ═══════════════════════════════════════════════════════════════════════
 * ARP PACKET STRUCTURE
 * ═══════════════════════════════════════════════════════════════════════ */

typedef struct {
    u16 hardware_type;      /* Hardware type (Ethernet = 1) */
    u16 protocol_type;      /* Protocol type (IPv4 = 0x0800) */
    u8  hardware_len;       /* Hardware address length (6) */
    u8  protocol_len;       /* Protocol address length (4) */
    u16 operation;          /* Operation (Request/Reply) */
    mac_addr_t sender_mac;  /* Sender hardware address */
    ip_addr_t sender_ip;    /* Sender protocol address */
    mac_addr_t target_mac;  /* Target hardware address */
    ip_addr_t target_ip;    /* Target protocol address */
} __attribute__((packed)) arp_packet_t;

/* ═══════════════════════════════════════════════════════════════════════
 * ARP TABLE ENTRY
 * ═══════════════════════════════════════════════════════════════════════ */

typedef struct {
    ip_addr_t ip;
    mac_addr_t mac;
    u32 timestamp;      /* Time when entry was added */
    u8 valid;           /* Entry is valid */
} arp_entry_t;

/* ═══════════════════════════════════════════════════════════════════════
 * GLOBAL STATE
 * ═══════════════════════════════════════════════════════════════════════ */

static arp_entry_t arp_table[ARP_TABLE_SIZE];
static int arp_count = 0;

/* ═══════════════════════════════════════════════════════════════════════
 * ARP INITIALIZATION
 * ═══════════════════════════════════════════════════════════════════════ */

void arp_init(void) {
    serial_puts("ARP: Initializing ARP table\n");
    
    /* Clear ARP table */
    for (int i = 0; i < ARP_TABLE_SIZE; i++) {
        arp_table[i].valid = 0;
    }
    arp_count = 0;
}

/* ═══════════════════════════════════════════════════════════════════════
 * ARP TABLE LOOKUP
 * ═══════════════════════════════════════════════════════════════════════ */

int arp_table_lookup(ip_addr_t ip, mac_addr_t *mac) {
    for (int i = 0; i < ARP_TABLE_SIZE; i++) {
        if (arp_table[i].valid && ip_addr_equal(arp_table[i].ip, ip)) {
            *mac = arp_table[i].mac;
            return 0;
        }
    }
    return -1;
}

/* ═══════════════════════════════════════════════════════════════════════
 * ARP TABLE ADD/UPDATE
 * ═══════════════════════════════════════════════════════════════════════ */

void arp_table_add(ip_addr_t ip, mac_addr_t mac) {
    /* Check if entry already exists */
    for (int i = 0; i < ARP_TABLE_SIZE; i++) {
        if (arp_table[i].valid && ip_addr_equal(arp_table[i].ip, ip)) {
            /* Update existing entry */
            arp_table[i].mac = mac;
            arp_table[i].timestamp = 0; /* TODO: Use real timestamp */
            return;
        }
    }
    
    /* Find free slot */
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
    
    /* Table full, replace oldest entry (simple FIFO) */
    static int next_slot = 0;
    arp_table[next_slot].ip = ip;
    arp_table[next_slot].mac = mac;
    arp_table[next_slot].valid = 1;
    arp_table[next_slot].timestamp = 0;
    next_slot = (next_slot + 1) % ARP_TABLE_SIZE;
}

/* ═══════════════════════════════════════════════════════════════════════
 * ARP REQUEST
 * ═══════════════════════════════════════════════════════════════════════ */

void arp_request(ip_addr_t ip) {
    /* Build ARP request packet */
    arp_packet_t pkt;
    
    pkt.hardware_type = htons(ARP_HW_ETHER);
    pkt.protocol_type = htons(ARP_PROTO_IP);
    pkt.hardware_len = 6;
    pkt.protocol_len = 4;
    pkt.operation = htons(ARP_REQUEST);
    
    /* Get our MAC and IP from interface 0 (TODO: determine interface) */
    net_if_t *ifc = net_get_interface(0);
    if (!ifc) {
        return;
    }
    
    pkt.sender_mac = ifc->mac;
    pkt.sender_ip = ifc->ip;
    
    /* Target MAC is broadcast */
    for (int i = 0; i < 6; i++) {
        pkt.target_mac.addr[i] = 0xFF;
    }
    pkt.target_ip = ip;
    
    /* Send ARP packet as broadcast Ethernet frame */
    mac_addr_t broadcast;
    for (int i = 0; i < 6; i++) {
        broadcast.addr[i] = 0xFF;
    }
    
    net_send_packet(ifc, broadcast, 0x0806, (u8 *)&pkt, sizeof(pkt));
    
    serial_puts("ARP: Request for ");
    ip_addr_print(ip);
    serial_puts("\n");
}

/* ═══════════════════════════════════════════════════════════════════════
 * ARP REPLY
 * ═══════════════════════════════════════════════════════════════════════ */

void arp_reply(mac_addr_t src_mac, ip_addr_t src_ip, mac_addr_t dst_mac, ip_addr_t dst_ip) {
    /* Build ARP reply packet */
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
    
    /* Send ARP reply to requesting host */
    net_if_t *ifc = net_get_interface(0);
    if (!ifc) {
        return;
    }
    
    net_send_packet(ifc, dst_mac, 0x0806, (u8 *)&pkt, sizeof(pkt));
    
    serial_puts("ARP: Reply to ");
    ip_addr_print(dst_ip);
    serial_puts("\n");
}

/* ═══════════════════════════════════════════════════════════════════════
 * ARP RESOLVE (blocking)
 * ═══════════════════════════════════════════════════════════════════════ */

int arp_resolve(ip_addr_t ip, mac_addr_t *mac) {
    /* Check if already in table */
    if (arp_table_lookup(ip, mac) == 0) {
        return 0;
    }
    
    /* Send ARP request */
    arp_request(ip);
    
    /* Wait for reply (simplified) */
    /* In a real implementation, we would queue the packet and wait */
    /* For now, return -1 to indicate ARP pending */
    return -1;
}

/* ═══════════════════════════════════════════════════════════════════════
 * ARP PACKET HANDLING
 * ═══════════════════════════════════════════════════════════════════════ */

void arp_handle(net_packet_t *pkt) {
    /* Ethernet header is 14 bytes */
    if (pkt->length < 14 + sizeof(arp_packet_t)) {
        return;
    }
    
    arp_packet_t *arp = (arp_packet_t *)(pkt->data + 14);
    
    /* Convert fields to host byte order */
    u16 hardware_type = ntohs(arp->hardware_type);
    u16 protocol_type = ntohs(arp->protocol_type);
    u16 operation = ntohs(arp->operation);
    
    /* Check if it's an ARP packet for IPv4 over Ethernet */
    if (hardware_type != ARP_HW_ETHER || protocol_type != ARP_PROTO_IP) {
        return;
    }
    
    /* Get our interface */
    net_if_t *ifc = net_get_interface(0);
    if (!ifc) {
        return;
    }
    
    /* Check if this ARP is for us */
    int for_us = ip_addr_equal(arp->target_ip, ifc->ip);
    
    /* Add sender to ARP table */
    arp_table_add(arp->sender_ip, arp->sender_mac);
    
    if (operation == ARP_REQUEST && for_us) {
        /* Reply to ARP request */
        arp_reply(ifc->mac, ifc->ip, arp->sender_mac, arp->sender_ip);
    }
}

/* ═══════════════════════════════════════════════════════════════════════
 * ARP TABLE PRINT
 * ═══════════════════════════════════════════════════════════════════════ */

void arp_print_table(void) {
    serial_puts("ARP Table:\n");
    for (int i = 0; i < ARP_TABLE_SIZE; i++) {
        if (arp_table[i].valid) {
            serial_puts("  ");
            ip_addr_print(arp_table[i].ip);
            serial_puts(" -> ");
            for (int j = 0; j < 6; j++) {
                serial_puthex(arp_table[i].mac.addr[j]);
                if (j < 5) serial_putc(':');
            }
            serial_puts("\n");
        }
    }
}
