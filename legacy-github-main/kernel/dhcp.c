/* ═══════════════════════════════════════════════════════════════════════
 * VECTORD OS - DHCP Client
 * 
 * Dynamic Host Configuration Protocol - Auto-IP configuration
 * ═══════════════════════════════════════════════════════════════════════ */

#include "../include/vecord.h"

/* ═══════════════════════════════════════════════════════════════════════
 * DHCP CONSTANTS
 * ═══════════════════════════════════════════════════════════════════════ */

#define DHCP_SERVER_PORT   67
#define DHCP_CLIENT_PORT   68

#define DHCP_DISCOVER  1
#define DHCP_OFFER     2
#define DHCP_REQUEST   3
#define DHCP_ACK       5
#define DHCP_NAK       6

/* DHCP options */
#define DHCP_OPT_SUBNET_MASK   1
#define DHCP_OPT_ROUTER        3
#define DHCP_OPT_DNS           6
#define DHCP_OPT_REQUESTED_IP  50
#define DHCP_OPT_LEASE_TIME    51
#define DHCP_OPT_MSG_TYPE      53
#define DHCP_OPT_SERVER_ID     54
#define DHCP_OPT_END           255

/* ═══════════════════════════════════════════════════════════════════════
 * DHCP PACKET STRUCTURE
 * ═══════════════════════════════════════════════════════════════════════ */

typedef struct {
    u8  op;             /* Message op code / message type */
    u8  htype;          /* Hardware address type */
    u8  hlen;           /* Hardware address length */
    u8  hops;           /* Client sets to zero, used by relay agents */
    u32 xid;            /* Transaction ID */
    u16 secs;           /* Seconds elapsed since client started */
    u16 flags;          /* Flags */
    ip_addr_t ciaddr;   /* Client IP address */
    ip_addr_t yiaddr;   /* Your (client) IP address */
    ip_addr_t siaddr;   /* Next server IP address */
    ip_addr_t giaddr;   /* Relay agent IP address */
    u8  chaddr[16];     /* Client hardware address */
    u8  sname[64];      /* Server host name */
    u8  file[128];      /* Boot file name */
    u8  options[312];   /* Options */
} __attribute__((packed)) dhcp_packet_t;

/* ═══════════════════════════════════════════════════════════════════════
 * DHCP STATE
 * ═══════════════════════════════════════════════════════════════════════ */

static u32 dhcp_xid = 0x12345678;
static ip_addr_t dhcp_server_ip;
static u8 dhcp_state = 0; /* 0 = init, 1 = waiting, 2 = bound */

/* ═══════════════════════════════════════════════════════════════════════
 * DHCP INITIALIZATION
 * ═══════════════════════════════════════════════════════════════════════ */

void dhcp_init(void) {
    serial_puts("DHCP: DHCP client initialized\n");
    dhcp_state = 0;
}

/* ═══════════════════════════════════════════════════════════════════════
 * DHCP DISCOVER
 * ═══════════════════════════════════════════════════════════════════════ */

void dhcp_discover(void) {
    /* Build DHCP Discover packet */
    dhcp_packet_t pkt;
    
    /* Zero out packet */
    for (int i = 0; i < sizeof(pkt); i++) {
        ((u8 *)&pkt)[i] = 0;
    }
    
    pkt.op = 1; /* BOOTREQUEST */
    pkt.htype = 1; /* Ethernet */
    pkt.hlen = 6;
    pkt.xid = dhcp_xid;
    pkt.flags = htons(0x8000); /* Broadcast */
    
    /* Get MAC address from interface 0 */
    net_if_t *ifc = net_get_interface(0);
    if (!ifc) {
        return;
    }
    
    for (int i = 0; i < 6; i++) {
        pkt.chaddr[i] = ifc->mac.addr[i];
    }
    
    /* Set options */
    int opt_idx = 0;
    pkt.options[opt_idx++] = DHCP_OPT_MSG_TYPE;
    pkt.options[opt_idx++] = 1;
    pkt.options[opt_idx++] = DHCP_DISCOVER;
    
    /* Request parameters */
    pkt.options[opt_idx++] = 55; /* Parameter Request List */
    pkt.options[opt_idx++] = 4;
    pkt.options[opt_idx++] = DHCP_OPT_SUBNET_MASK;
    pkt.options[opt_idx++] = DHCP_OPT_ROUTER;
    pkt.options[opt_idx++] = DHCP_OPT_DNS;
    pkt.options[opt_idx++] = DHCP_OPT_SERVER_ID;
    
    pkt.options[opt_idx++] = DHCP_OPT_END;
    
    /* Send via UDP to broadcast address */
    ip_addr_t broadcast = {{255, 255, 255, 255}};
    udp_send(broadcast, DHCP_SERVER_PORT, DHCP_CLIENT_PORT, (u8 *)&pkt, sizeof(pkt));
    
    serial_puts("DHCP: Discover sent\n");
    dhcp_state = 1;
}

/* ═══════════════════════════════════════════════════════════════════════
 * DHCP HANDLE OFFER
 * ═══════════════════════════════════════════════════════════════════════ */

static void dhcp_request(ip_addr_t requested_ip); /* forward declaration */

void dhcp_handle_offer(net_packet_t *pkt) {
    /* Parse DHCP Offer packet */
    dhcp_packet_t *offer = (dhcp_packet_t *)pkt->data;
    
    if (offer->op != 2 /* BOOTREPLY */ || offer->xid != dhcp_xid) {
        return;
    }
    
    serial_puts("DHCP: Offer received, IP: ");
    ip_addr_print(offer->yiaddr);
    serial_puts("\n");
    
    /* Extract options */
    int opt_idx = 0;
    while (opt_idx < 312) {
        u8 opt = offer->options[opt_idx++];
        if (opt == DHCP_OPT_END) break;
        if (opt == 0) continue; /* Padding */
        
        u8 len = offer->options[opt_idx++];
        
        switch (opt) {
            case DHCP_OPT_SUBNET_MASK:
                /* Store subnet mask */
                break;
            case DHCP_OPT_ROUTER:
                /* Store default gateway */
                break;
            case DHCP_OPT_SERVER_ID:
                for (int i = 0; i < 4; i++) {
                    dhcp_server_ip.addr[i] = offer->options[opt_idx + i];
                }
                break;
            case DHCP_OPT_MSG_TYPE:
                if (offer->options[opt_idx] == DHCP_OFFER) {
                    /* Ready to send request */
                }
                break;
        }
        opt_idx += len;
    }
    
    /* Send DHCP Request */
    dhcp_request(offer->yiaddr);
}

/* ═══════════════════════════════════════════════════════════════════════
 * DHCP REQUEST
 * ═══════════════════════════════════════════════════════════════════════ */

static void dhcp_request(ip_addr_t requested_ip) {
    dhcp_packet_t pkt;
    
    /* Zero out packet */
    for (int i = 0; i < sizeof(pkt); i++) {
        ((u8 *)&pkt)[i] = 0;
    }
    
    pkt.op = 1; /* BOOTREQUEST */
    pkt.htype = 1; /* Ethernet */
    pkt.hlen = 6;
    pkt.xid = dhcp_xid;
    pkt.flags = htons(0x8000); /* Broadcast */
    
    net_if_t *ifc = net_get_interface(0);
    if (!ifc) {
        return;
    }
    
    for (int i = 0; i < 6; i++) {
        pkt.chaddr[i] = ifc->mac.addr[i];
    }
    
    /* Set options */
    int opt_idx = 0;
    pkt.options[opt_idx++] = DHCP_OPT_MSG_TYPE;
    pkt.options[opt_idx++] = 1;
    pkt.options[opt_idx++] = DHCP_REQUEST;
    
    /* Requested IP */
    pkt.options[opt_idx++] = DHCP_OPT_REQUESTED_IP;
    pkt.options[opt_idx++] = 4;
    for (int i = 0; i < 4; i++) {
        pkt.options[opt_idx++] = requested_ip.addr[i];
    }
    
    /* Server ID */
    pkt.options[opt_idx++] = DHCP_OPT_SERVER_ID;
    pkt.options[opt_idx++] = 4;
    for (int i = 0; i < 4; i++) {
        pkt.options[opt_idx++] = dhcp_server_ip.addr[i];
    }
    
    pkt.options[opt_idx++] = DHCP_OPT_END;
    
    /* Send via UDP to broadcast address */
    ip_addr_t broadcast = {{255, 255, 255, 255}};
    udp_send(broadcast, DHCP_SERVER_PORT, DHCP_CLIENT_PORT, (u8 *)&pkt, sizeof(pkt));
    
    serial_puts("DHCP: Request sent\n");
}

/* ═══════════════════════════════════════════════════════════════════════
 * DHCP HANDLE ACK
 * ═══════════════════════════════════════════════════════════════════════ */

void dhcp_handle_ack(net_packet_t *pkt) {
    dhcp_packet_t *ack = (dhcp_packet_t *)pkt->data;
    
    if (ack->op != 2 /* BOOTREPLY */ || ack->xid != dhcp_xid) {
        return;
    }
    
    serial_puts("DHCP: ACK received, IP: ");
    ip_addr_print(ack->yiaddr);
    serial_puts("\n");
    
    /* Configure network interface with assigned IP */
    net_if_t *ifc = net_get_interface(0);
    if (ifc) {
        ifc->ip = ack->yiaddr;
        /* Extract subnet mask and gateway from options */
        /* For now, use defaults */
        ip_addr_t subnet = {{255, 255, 255, 0}};
        ip_addr_t gateway = {{0, 0, 0, 0}};
        ifc->netmask = subnet;
        ifc->gateway = gateway;
        
        serial_puts("DHCP: Network configured\n");
        dhcp_state = 2;
    }
}
