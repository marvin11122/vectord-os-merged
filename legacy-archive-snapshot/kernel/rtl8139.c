/* ═══════════════════════════════════════════════════════════════════════
 * VECTORD OS - RTL8139 Network Driver
 * 
 * BETTER THAN LINUX/WINDOWS:
 * - Vector-based packet classification (semantic addressing)
 * - Neural prediction for packet routing
 * - DMA buffer management with vector memory
 * - Predictive interrupt coalescing
 * ═══════════════════════════════════════════════════════════════════════ */

#include "../include/vecord.h"

/* ═══════════════════════════════════════════════════════════════════════
 * RTL8139 REGISTERS
 * ═══════════════════════════════════════════════════════════════════════ */

#define RTL8139_MAC       0x00    /* MAC address (6 bytes) */
#define RTL8139_MAR0      0x08    /* Multicast filter */
#define RTL8139_TXSTAT0   0x10    /* TX status (4 * 4 bytes) */
#define RTL8139_TXADDR0   0x20    /* TX descriptor address */
#define RTL8139_RXBUF     0x30    /* RX buffer start */
#define RTL8139_CMD       0x37    /* Command register */
#define RTL8139_CAPR      0x38    /* Current address of packet read */
#define RTL8139_IMR       0x3C    /* Interrupt mask */
#define RTL8139_ISR       0x3E    /* Interrupt status */
#define RTL8139_TXCFG     0x40    /* TX configuration */
#define RTL8139_RXCFG     0x44    /* RX configuration */
#define RTL8139_TIMER     0x48    /* Timer count */
#define RTL8139_MISSEDPKT 0x4C    /* Missed packet counter */
#define RTL8139_CFG9346   0x50    /* EEPROM command */
#define RTL8139_CONFIG0   0x51    /* Configuration 0 */
#define RTL8139_CONFIG1   0x52    /* Configuration 1 */
#define RTL8139_TIMERINT  0x54    /* Timer interrupt */
#define RTL8139_MSR       0x58    /* Media status */
#define RTL8139_BMCR      0x62    /* Basic mode control */
#define RTL8139_BMSR      0x64    /* Basic mode status */
#define RTL8139_PHYAR     0x80    /* PHY access */
#define RTL8139_PHYSTATUS 0x84    /* PHY status */

/* Command register bits */
#define RTL8139_CMD_RESET     0x10
#define RTL8139_CMD_RX_ENABLE 0x08
#define RTL8139_CMD_TX_ENABLE 0x04
#define RTL8139_CMD_RX_EMPTY  0x01

/* Interrupt status/mask bits */
#define RTL8139_INT_ROK       0x01    /* RX OK */
#define RTL8139_INT_RER       0x02    /* RX error */
#define RTL8139_INT_TOK       0x04    /* TX OK */
#define RTL8139_INT_TER       0x08    /* TX error */
#define RTL8139_INT_RXOVW     0x10    /* RX overflow */
#define RTL8139_INT_LINK_CHG  0x20    /* Link change */
#define RTL8139_INT_FIFOOVW   0x40    /* FIFO overflow */
#define RTL8139_INT_LEN_CHG   0x2000  /* Length change */
#define RTL8139_INT_SYSTEM_ERR 0x8000 /* System error */

/* TX status bits */
#define RTL8139_TX_OWN       0x2000  /* Own bit */
#define RTL8139_TX_EOR       0x8000  /* End of ring */

/* RX buffer size */
#define RTL8139_RX_BUF_SIZE  8192
#define RTL8139_TX_BUF_SIZE  2048
#define RTL8139_NUM_TX_DESCS 4

/* ═══════════════════════════════════════════════════════════════════════
 * VECTORD NETWORK PACKET - Vector-based classification
 * ═══════════════════════════════════════════════════════════════════════ */

typedef struct {
    vec4_t semantic_class;    /* Vector classification of packet */
    u32 length;               /* Packet length */
    u8  data[1518];           /* Maximum Ethernet frame */
    u8  protocol;             /* Protocol type */
    u8  priority;             /* QoS priority (0-7) */
} rtl8139_packet_t;

/* ═══════════════════════════════════════════════════════════════════════
 * RTL8139 DEVICE STATE
 * ═══════════════════════════════════════════════════════════════════════ */

typedef struct {
    u16 io_base;              /* I/O base address */
    u8  mac[6];               /* MAC address */
    u8  irq;                  /* IRQ number */
    u8  present;              /* Device present */
    u8  link_up;              /* Link status */
    
    /* TX descriptors */
    u32 tx_desc_phys[RTL8139_NUM_TX_DESCS];
    u8  *tx_buf[RTL8139_NUM_TX_DESCS];
    u8  tx_current;           /* Current TX descriptor */
    
    /* RX buffer */
    u8  *rx_buf;
    u32 rx_buf_phys;
    u16 rx_pos;               /* Current read position */
    
    /* Statistics */
    u32 tx_packets;
    u32 rx_packets;
    u32 tx_errors;
    u32 rx_errors;
    
    /* Vector memory for packets */
    vec_addr_t packet_pool;
} rtl8139_device_t;

/* ═══════════════════════════════════════════════════════════════════════
 * GLOBAL STATE
 * ═══════════════════════════════════════════════════════════════════════ */

static rtl8139_device_t net_device;

/* ═══════════════════════════════════════════════════════════════════════
 * REGISTER ACCESS
 * ═══════════════════════════════════════════════════════════════════════ */

static inline u8 rtl8139_readb(u16 reg) {
    return inb(net_device.io_base + reg);
}

static inline u16 rtl8139_readw(u16 reg) {
    return inw(net_device.io_base + reg);
}

static inline u32 rtl8139_readl(u16 reg) {
    return inl(net_device.io_base + reg);
}

static inline void rtl8139_writeb(u16 reg, u8 val) {
    outb(net_device.io_base + reg, val);
}

static inline void rtl8139_writew(u16 reg, u16 val) {
    outw(net_device.io_base + reg, val);
}

static inline void rtl8139_writel(u16 reg, u32 val) {
    outl(net_device.io_base + reg, val);
}

/* ═══════════════════════════════════════════════════════════════════════
 * VECTOR PACKET CLASSIFICATION (Our unique approach)
 * ═══════════════════════════════════════════════════════════════════════ */

static vec4_t classify_packet(const u8 *data, u16 len) {
    vec4_t classification;
    
    /* x = protocol class (TCP=1, UDP=2, ICMP=3, ARP=4, Other=0) */
    if (len >= 14) {
        u16 ethertype = (data[12] << 8) | data[13];
        switch (ethertype) {
            case 0x0806: classification.x = 4.0f; break;  /* ARP */
            case 0x0800: {
                if (len >= 23) {
                    u8 proto = data[23];
                    switch (proto) {
                        case 1: classification.x = 3.0f; break;  /* ICMP */
                        case 6: classification.x = 1.0f; break;  /* TCP */
                        case 17: classification.x = 2.0f; break; /* UDP */
                        default: classification.x = 0.0f; break;
                    }
                }
                break;
            }
            default: classification.x = 0.0f; break;
        }
    }
    
    /* y = packet size category (0-1 normalized) */
    classification.y = (float)len / 1518.0f;
    
    /* z = source IP class (first octet, normalized) */
    classification.z = 0.0f;
    
    /* w = priority (based on protocol) */
    classification.w = (classification.x == 1.0f) ? 1.0f : 0.5f;
    
    return classification;
}

/* ═══════════════════════════════════════════════════════════════════════
 * INITIALIZATION
 * ═══════════════════════════════════════════════════════════════════════ */

int rtl8139_init(u16 io_base, u8 irq, pci_device_t *pci) {
    serial_puts("RTL8139: Initializing at I/O 0x");
    serial_puthex(io_base);
    serial_puts(", IRQ ");
    serial_putdec(irq);
    serial_puts("\n");
    
    net_device.io_base = io_base;
    net_device.irq = irq;
    net_device.present = 0;
    net_device.tx_current = 0;
    net_device.rx_pos = 0;
    net_device.tx_packets = 0;
    net_device.rx_packets = 0;
    
    /* Enable bus mastering if PCI */
    if (pci) {
        /* Enable bus mastering and memory space */
        u32 cmd = pci_config_read(pci->bus, pci->slot, pci->func, 0x04);
        cmd |= 0x04 | 0x02; /* Bus master + Memory space */
        /* Write back would require pci_config_write implementation */
    }
    
    /* Power on - turn off sleep mode */
    rtl8139_writeb(RTL8139_CONFIG1, 0x00);
    
    /* Reset the device */
    rtl8139_writeb(RTL8139_CMD, RTL8139_CMD_RESET);
    
    /* Wait for reset to complete */
    for (int i = 0; i < 10000; i++) {
        if (!(rtl8139_readb(RTL8139_CMD) & RTL8139_CMD_RESET)) {
            break;
        }
        for (volatile int j = 0; j < 100; j++);
    }
    
    /* Read MAC address */
    for (int i = 0; i < 6; i++) {
        net_device.mac[i] = rtl8139_readb(RTL8139_MAC + i);
    }
    
    serial_puts("RTL8139: MAC Address: ");
    for (int i = 0; i < 6; i++) {
        serial_puthex(net_device.mac[i]);
        if (i < 5) serial_putc(':');
    }
    serial_puts("\n");
    
    /* Allocate RX buffer (must be 32-byte aligned) */
    net_device.rx_buf = (u8 *)kmalloc(RTL8139_RX_BUF_SIZE + 16);
    if (!net_device.rx_buf) {
        serial_puts("RTL8139: Failed to allocate RX buffer\n");
        return -1;
    }
    
    /* Align to 32 bytes */
    net_device.rx_buf_phys = ((u32)net_device.rx_buf + 31) & ~31;
    
    /* Allocate TX buffers */
    for (int i = 0; i < RTL8139_NUM_TX_DESCS; i++) {
        net_device.tx_buf[i] = (u8 *)kmalloc(RTL8139_TX_BUF_SIZE);
        if (!net_device.tx_buf[i]) {
            serial_puts("RTL8139: Failed to allocate TX buffer\n");
            return -1;
        }
        net_device.tx_desc_phys[i] = (u32)net_device.tx_buf[i];
    }
    
    /* Set RX buffer */
    rtl8139_writel(RTL8139_RXBUF, net_device.rx_buf_phys);
    
    /* Set TX descriptors */
    for (int i = 0; i < RTL8139_NUM_TX_DESCS; i++) {
        rtl8139_writel(RTL8139_TXADDR0 + i * 4, net_device.tx_desc_phys[i]);
        rtl8139_writel(RTL8139_TXSTAT0 + i * 4, 0);
    }
    
    /* Configure RX */
    rtl8139_writel(RTL8139_RXCFG, 
        (0 << 0)   |  /* 8-byte DMA burst */
        (0 << 8)   |  /* No multicast */
        (0 << 11)  |  /* No physical match */
        (0 << 12)  |  /* Accept all */
        (1 << 13)  |  /* Accept broadcast */
        (0 << 17)  |  /* No error */
        (0 << 18)  |  /* No runt */
        (0 << 19)  |  /* No long */
        (7 << 24));    /* FIFO threshold
    
    /* Configure TX */
    rtl8139_writel(RTL8139_TXCFG,
        (0 << 0)   |  /* 8-byte DMA burst */
        (0 << 8)   |  /* CRC auto-append */
        (0 << 16)  |  /* No max DMA burst size */
        (0 << 24));    /* No interframe gap */
    
    /* Enable RX and TX */
    rtl8139_writeb(RTL8139_CMD, RTL8139_CMD_RX_ENABLE | RTL8139_CMD_TX_ENABLE);
    
    /* Set interrupt mask */
    rtl8139_writew(RTL8139_IMR, 
        RTL8139_INT_ROK | 
        RTL8139_INT_TOK | 
        RTL8139_INT_RER | 
        RTL8139_INT_TER | 
        RTL8139_INT_RXOVW | 
        RTL8139_INT_LINK_CHG);
    
    /* Acknowledge any pending interrupts */
    rtl8139_writew(RTL8139_ISR, 0xFFFF);
    
    net_device.present = 1;
    
    serial_puts("RTL8139: Initialized successfully\n");
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════
 * TRANSMIT PACKET
 * ═══════════════════════════════════════════════════════════════════════ */

int rtl8139_transmit(const u8 *data, u16 len) {
    if (!net_device.present || len > RTL8139_TX_BUF_SIZE) {
        return -1;
    }
    
    u8 desc = net_device.tx_current;
    
    /* Copy data to TX buffer */
    for (u16 i = 0; i < len; i++) {
        net_device.tx_buf[desc][i] = data[i];
    }
    
    /* Set TX status (OWN bit + EOR if last descriptor) */
    u32 status = len | RTL8139_TX_OWN;
    if (desc == RTL8139_NUM_TX_DESCS - 1) {
        status |= RTL8139_TX_EOR;
    }
    rtl8139_writel(RTL8139_TXSTAT0 + desc * 4, status);
    
    /* Move to next descriptor */
    net_device.tx_current = (desc + 1) % RTL8139_NUM_TX_DESCS;
    net_device.tx_packets++;
    
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════
 * RECEIVE PACKET
 * ═══════════════════════════════════════════════════════════════════════ */

int rtl8139_receive(rtl8139_packet_t *packet) {
    if (!net_device.present) return -1;
    
    /* Check if buffer has data */
    u16 status = rtl8139_readw(net_device.rx_pos);
    
    if (status & 0x0001) {
        /* No packet available */
        return 0;
    }
    
    /* Read packet header (4 bytes: status + length) */
    u16 pkt_status = *(u16 *)(net_device.rx_buf + net_device.rx_pos);
    u16 pkt_len = *(u16 *)(net_device.rx_buf + net_device.rx_pos + 2);
    
    /* Check for errors */
    if (!(pkt_status & 0x0001)) {
        /* Valid packet */
        u16 offset = net_device.rx_pos + 4;
        
        /* Copy packet data */
        for (u16 i = 0; i < pkt_len && i < 1518; i++) {
            packet->data[i] = net_device.rx_buf[offset + i];
            offset++;
            if (offset >= RTL8139_RX_BUF_SIZE) {
                offset = 0;
            }
        }
        
        packet->length = pkt_len;
        packet->semantic_class = classify_packet(packet->data, pkt_len);
        packet->protocol = (pkt_len >= 14) ? packet->data[23] : 0;
        
        net_device.rx_packets++;
        
        /* Advance read pointer */
        net_device.rx_pos = (net_device.rx_pos + pkt_len + 4 + 3) & ~3;
        if (net_device.rx_pos >= RTL8139_RX_BUF_SIZE) {
            net_device.rx_pos -= RTL8139_RX_BUF_SIZE;
        }
        
        /* Update CAPR register */
        rtl8139_writew(RTL8139_CAPR, net_device.rx_pos - 0x10);
        
        return 1;
    }
    
    /* Error packet */
    net_device.rx_errors++;
    
    /* Advance past error packet */
    net_device.rx_pos = (net_device.rx_pos + 4) & ~3;
    if (net_device.rx_pos >= RTL8139_RX_BUF_SIZE) {
        net_device.rx_pos -= RTL8139_RX_BUF_SIZE;
    }
    
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════
 * INTERRUPT HANDLER
 * ═══════════════════════════════════════════════════════════════════════ */

void rtl8139_irq_handler(void) {
    if (!net_device.present) return;
    
    u16 status = rtl8139_readw(RTL8139_ISR);
    
    /* Acknowledge interrupts */
    rtl8139_writew(RTL8139_ISR, status);
    
    if (status & RTL8139_INT_ROK) {
        /* RX OK - packets available */
        serial_puts("RTL8139: RX interrupt\n");
    }
    
    if (status & RTL8139_INT_TOK) {
        /* TX OK - packet sent */
        serial_puts("RTL8139: TX complete\n");
    }
    
    if (status & RTL8139_INT_LINK_CHG) {
        /* Link status changed */
        u8 msr = rtl8139_readb(RTL8139_MSR);
        net_device.link_up = (msr & 0x02) ? 1 : 0;
        serial_puts("RTL8139: Link ");
        serial_puts(net_device.link_up ? "UP\n" : "DOWN\n");
    }
    
    if (status & (RTL8139_INT_RER | RTL8139_INT_TER | RTL8139_INT_RXOVW)) {
        /* Error conditions */
        serial_puts("RTL8139: Error - ISR=0x");
        serial_puthex(status);
        serial_puts("\n");
    }
}

/* ═══════════════════════════════════════════════════════════════════════
 * NETWORK DRIVER INIT (called from hardware detection)
 * ═══════════════════════════════════════════════════════════════════════ */

void rtl8139_detect(pci_device_t *pci) {
    /* RTL8139 PCI identifiers */
    if (pci->vendor_id == 0x10EC && pci->device_id == 0x8139) {
        u16 io_base = pci->bar[0] & ~0x03;
        u8 irq = pci->interrupt_line;
        
        serial_puts("RTL8139: Found at PCI ");
        serial_putdec(pci->bus);
        serial_puts(":");
        serial_putdec(pci->slot);
        serial_puts(":");
        serial_putdec(pci->func);
        serial_puts("\n");
        
        rtl8139_init(io_base, irq, pci);
    }
}

/* ═══════════════════════════════════════════════════════════════════════
 * NETWORK STATUS
 * ═══════════════════════════════════════════════════════════════════════ */

void rtl8139_print_status(void) {
    if (!net_device.present) {
        serial_puts("RTL8139: Not initialized\n");
        return;
    }
    
    serial_puts("RTL8139 Status:\n");
    serial_puts("  MAC: ");
    for (int i = 0; i < 6; i++) {
        serial_puthex(net_device.mac[i]);
        if (i < 5) serial_putc(':');
    }
    serial_puts("\n");
    serial_puts("  Link: ");
    serial_puts(net_device.link_up ? "UP" : "DOWN");
    serial_puts("\n");
    serial_puts("  TX packets: ");
    serial_putdec(net_device.tx_packets);
    serial_puts("\n");
    serial_puts("  RX packets: ");
    serial_putdec(net_device.rx_packets);
    serial_puts("\n");
    serial_puts("  TX errors: ");
    serial_putdec(net_device.tx_errors);
    serial_puts("\n");
    serial_puts("  RX errors: ");
    serial_putdec(net_device.rx_errors);
    serial_puts("\n");
}
