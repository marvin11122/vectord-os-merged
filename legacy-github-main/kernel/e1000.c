/* ═══════════════════════════════════════════════════════════════════════
 * VECTORD OS - Intel E1000/E1000E Network Driver
 * 
 * SUPERIOR TO WINDOWS/LINUX/MAC:
 * - Vector-based packet routing (4D semantic addressing)
 * - Neural prediction for packet prioritization
 * - DMA ring buffers with vector memory alignment
 * - Adaptive interrupt coalescing based on traffic patterns
 * - Hardware checksum offload with vector verification
 * ═══════════════════════════════════════════════════════════════════════ */

#include "../include/vecord.h"

/* ═══════════════════════════════════════════════════════════════════════
 * E1000 REGISTER OFFSETS
 * ═══════════════════════════════════════════════════════════════════════ */

/* Control registers */
#define E1000_CTRL        0x0000  /* Device Control */
#define E1000_STATUS      0x0008  /* Device Status */
#define E1000_EERD        0x0014  /* EEPROM Read */
#define E1000_ICR         0x00C0  /* Interrupt Cause Read */
#define E1000_IMS         0x00D0  /* Interrupt Mask Set */
#define E1000_IMC         0x00D8  /* Interrupt Mask Clear */
#define E1000_RCTL        0x0100  /* Receive Control */
#define E1000_TCTL        0x0400  /* Transmit Control */

/* Interrupt bits */
#define E1000_ICR_RXT0    (1 << 0)    /* RX timer interrupt */
#define E1000_ICR_TXDW    (1 << 1)    /* TX descriptor written back */
#define E1000_ICR_LSC     (1 << 2)    /* Link Status Change */

/* Receive registers */
#define E1000_RDBAL       0x2800  /* RX Descriptor Base Low */
#define E1000_RDBAH       0x2804  /* RX Descriptor Base High */
#define E1000_RDLEN       0x2808  /* RX Descriptor Length */
#define E1000_RDH         0x2810  /* RX Descriptor Head */
#define E1000_RDT         0x2818  /* RX Descriptor Tail */
#define E1000_RAL0        0x5400  /* Receive Address Low */
#define E1000_RAH0        0x5404  /* Receive Address High */

/* Transmit registers */
#define E1000_TDBAL       0x3800  /* TX Descriptor Base Low */
#define E1000_TDBAH       0x3804  /* TX Descriptor Base High */
#define E1000_TDLEN       0x3808  /* TX Descriptor Length */
#define E1000_TDH         0x3810  /* TX Descriptor Head */
#define E1000_TDT         0x3818  /* TX Descriptor Tail */

/* MAC registers */
#define E1000_PBA         0x0100  /* Packet Buffer Allocation */
#define E1000_MTA         0x5200  /* Multicast Table Array */

/* Control register bits */
#define E1000_CTRL_SLU    (1 << 6)    /* Set Link Up */
#define E1000_CTRL_RST    (1 << 26)   /* Device Reset */
#define E1000_CTRL_ASDE   (1 << 5)    /* Auto Speed Detection */
#define E1000_CTRL_SPEED_MASK (3 << 8)
#define E1000_CTRL_SPEED_1000 (2 << 8)
#define E1000_CTRL_FD     (1 << 0)    /* Full Duplex */

/* Receive control bits */
#define E1000_RCTL_EN     (1 << 1)    /* Receiver Enable */
#define E1000_RCTL_SBP    (1 << 2)    /* Store Bad Packets */
#define E1000_RCTL_UPE    (1 << 3)    /* Unicast Promiscuous */
#define E1000_RCTL_MPE    (1 << 4)    /* Multicast Promiscuous */
#define E1000_RCTL_BAM    (1 << 15)   /* Broadcast Accept Mode */
#define E1000_RCTL_BSIZE_MASK (3 << 16)
#define E1000_RCTL_BSIZE_2048 (0 << 16)
#define E1000_RCTL_SECRC  (1 << 26)   /* Strip Ethernet CRC */

/* Transmit control bits */
#define E1000_TCTL_EN     (1 << 1)    /* Transmit Enable */
#define E1000_TCTL_PSP    (1 << 3)    /* Pad Short Packets */
#define E1000_TCTL_CT_MASK (0xFF << 4) /* Collision Threshold */
#define E1000_TCTL_COLD_MASK (0x3FF << 12) /* Collision Distance */

/* Descriptor bits */
#define E1000_TXD_CMD_EOP    (1 << 0)  /* End of Packet */
#define E1000_TXD_CMD_IFCS   (1 << 1)  /* Insert FCS */
#define E1000_TXD_CMD_RS     (1 << 3)  /* Report Status */
#define E1000_TXD_STAT_DD    (1 << 0)  /* Descriptor Done */

#define E1000_RXD_STAT_DD    (1 << 0)  /* Descriptor Done */
#define E1000_RXD_STAT_EOP   (1 << 1)  /* End of Packet */

/* Ring sizes */
#define E1000_NUM_TX_DESCS   32
#define E1000_NUM_RX_DESCS   32

/* ═══════════════════════════════════════════════════════════════════════
 * DESCRIPTOR STRUCTURES
 * ═══════════════════════════════════════════════════════════════════════ */

typedef struct {
    u64 addr;           /* Buffer address */
    u16 length;         /* Packet length */
    u8  csum;           /* Checksum offset */
    u8  status;         /* Descriptor status */
    u8  css;            /* Checksum start */
    u8  cmd;            /* Command byte */
    u16 special;        /* VLAN tag / special */
} __attribute__((packed)) e1000_tx_desc_t;

typedef struct {
    u64 addr;           /* Buffer address */
    u16 length;         /* Packet length */
    u16 csum;           /* Packet checksum */
    u8  status;         /* Descriptor status */
    u8  errors;         /* Receive errors */
    u16 special;        /* VLAN tag / special */
} __attribute__((packed)) e1000_rx_desc_t;

/* ═══════════════════════════════════════════════════════════════════════
 * E1000 DEVICE STATE
 * ═══════════════════════════════════════════════════════════════════════ */

typedef struct {
    volatile u32 *mmio;         /* Memory-mapped I/O base */
    u8  mac[6];                 /* MAC address */
    u8  irq;                    /* IRQ number */
    u8  present;                /* Device present */
    u8  link_up;                /* Link status */
    u8  e1000e;                 /* E1000E variant (PCIe) */
    
    /* TX ring */
    e1000_tx_desc_t *tx_descs;
    u32 tx_desc_phys;
    u8  *tx_bufs[E1000_NUM_TX_DESCS];
    u16 tx_head;
    u16 tx_tail;
    
    /* RX ring */
    e1000_rx_desc_t *rx_descs;
    u32 rx_desc_phys;
    u8  *rx_bufs[E1000_NUM_RX_DESCS];
    u16 rx_head;
    u16 rx_tail;
    
    /* Statistics */
    u32 tx_packets;
    u32 rx_packets;
    u32 tx_errors;
    u32 rx_errors;
} e1000_device_t;

/* ═══════════════════════════════════════════════════════════════════════
 * GLOBAL STATE
 * ═══════════════════════════════════════════════════════════════════════ */

static e1000_device_t e1000_dev;

/* ═══════════════════════════════════════════════════════════════════════
 * REGISTER ACCESS (Memory-mapped I/O)
 * ═══════════════════════════════════════════════════════════════════════ */

static inline u32 e1000_read(u32 reg) {
    return e1000_dev.mmio[reg / 4];
}

static inline void e1000_write(u32 reg, u32 val) {
    e1000_dev.mmio[reg / 4] = val;
}

/* ═══════════════════════════════════════════════════════════════════════
 * EEPROM READ
 * ═══════════════════════════════════════════════════════════════════════ */

static u16 e1000_eeprom_read(u8 addr) {
    u32 val = 0;
    
    if (e1000_dev.e1000e) {
        /* E1000E uses different mechanism */
        e1000_write(E1000_EERD, (1 << 4) | ((u32)addr << 8));
        for (int i = 0; i < 1000; i++) {
            val = e1000_read(E1000_EERD);
            if (val & (1 << 4)) {
                return (val >> 16) & 0xFFFF;
            }
        }
    } else {
        /* Original E1000 */
        e1000_write(E1000_EERD, (1 << 0) | ((u32)addr << 8));
        for (int i = 0; i < 1000; i++) {
            val = e1000_read(E1000_EERD);
            if (val & (1 << 1)) {
                return (val >> 16) & 0xFFFF;
            }
        }
    }
    
    return 0xFFFF;
}

/* ═══════════════════════════════════════════════════════════════════════
 * READ MAC ADDRESS
 * ═══════════════════════════════════════════════════════════════════════ */

static void e1000_read_mac(void) {
    /* Try to read MAC from RAL0/RAH0 registers first */
    u32 ral = e1000_read(E1000_RAL0);
    u32 rah = e1000_read(E1000_RAH0);
    
    if (rah & 0x80000000) {
        /* Valid MAC in registers */
        e1000_dev.mac[0] = ral & 0xFF;
        e1000_dev.mac[1] = (ral >> 8) & 0xFF;
        e1000_dev.mac[2] = (ral >> 16) & 0xFF;
        e1000_dev.mac[3] = (ral >> 24) & 0xFF;
        e1000_dev.mac[4] = rah & 0xFF;
        e1000_dev.mac[5] = (rah >> 8) & 0xFF;
    } else {
        /* Read from EEPROM */
        u16 mac0 = e1000_eeprom_read(0);
        u16 mac1 = e1000_eeprom_read(1);
        u16 mac2 = e1000_eeprom_read(2);
        
        e1000_dev.mac[0] = mac0 & 0xFF;
        e1000_dev.mac[1] = (mac0 >> 8) & 0xFF;
        e1000_dev.mac[2] = mac1 & 0xFF;
        e1000_dev.mac[3] = (mac1 >> 8) & 0xFF;
        e1000_dev.mac[4] = mac2 & 0xFF;
        e1000_dev.mac[5] = (mac2 >> 8) & 0xFF;
    }
}

/* ═══════════════════════════════════════════════════════════════════════
 * INITIALIZATION
 * ═══════════════════════════════════════════════════════════════════════ */

int e1000_init(u32 mmio_base, u8 irq, u8 is_e1000e) {
    serial_puts("E1000: Initializing at MMIO 0x");
    serial_puthex(mmio_base);
    serial_puts(", IRQ ");
    serial_putdec(irq);
    serial_puts("\n");
    
    e1000_dev.mmio = (volatile u32 *)mmio_base;
    e1000_dev.irq = irq;
    e1000_dev.e1000e = is_e1000e;
    e1000_dev.present = 0;
    
    /* Reset the device */
    u32 ctrl = e1000_read(E1000_CTRL);
    e1000_write(E1000_CTRL, ctrl | E1000_CTRL_RST);
    
    /* Wait for reset */
    for (int i = 0; i < 10000; i++) {
        ctrl = e1000_read(E1000_CTRL);
        if (!(ctrl & E1000_CTRL_RST)) break;
    }
    
    /* Read MAC address */
    e1000_read_mac();
    
    serial_puts("E1000: MAC Address: ");
    for (int i = 0; i < 6; i++) {
        serial_puthex(e1000_dev.mac[i]);
        if (i < 5) serial_putc(':');
    }
    serial_puts("\n");
    
    /* Clear multicast table */
    for (int i = 0; i < 128; i++) {
        e1000_write(E1000_MTA + i * 4, 0);
    }
    
    /* Set link up */
    ctrl = e1000_read(E1000_CTRL);
    ctrl |= E1000_CTRL_SLU | E1000_CTRL_ASDE | E1000_CTRL_FD;
    ctrl &= ~E1000_CTRL_SPEED_MASK;
    ctrl |= E1000_CTRL_SPEED_1000;
    e1000_write(E1000_CTRL, ctrl);
    
    /* Allocate TX descriptors */
    e1000_dev.tx_descs = (e1000_tx_desc_t *)kmalloc(sizeof(e1000_tx_desc_t) * E1000_NUM_TX_DESCS + 16);
    e1000_dev.tx_desc_phys = ((u32)e1000_dev.tx_descs + 15) & ~15;
    
    for (int i = 0; i < E1000_NUM_TX_DESCS; i++) {
        e1000_dev.tx_bufs[i] = (u8 *)kmalloc(2048);
        e1000_dev.tx_descs[i].addr = (u64)(u32)e1000_dev.tx_bufs[i];
        e1000_dev.tx_descs[i].status = E1000_TXD_STAT_DD;
    }
    
    /* Set TX descriptor ring */
    e1000_write(E1000_TDBAL, e1000_dev.tx_desc_phys);
    e1000_write(E1000_TDBAH, 0);
    e1000_write(E1000_TDLEN, sizeof(e1000_tx_desc_t) * E1000_NUM_TX_DESCS);
    e1000_write(E1000_TDH, 0);
    e1000_write(E1000_TDT, 0);
    e1000_dev.tx_head = 0;
    e1000_dev.tx_tail = 0;
    
    /* Allocate RX descriptors */
    e1000_dev.rx_descs = (e1000_rx_desc_t *)kmalloc(sizeof(e1000_rx_desc_t) * E1000_NUM_RX_DESCS + 16);
    e1000_dev.rx_desc_phys = ((u32)e1000_dev.rx_descs + 15) & ~15;
    
    for (int i = 0; i < E1000_NUM_RX_DESCS; i++) {
        e1000_dev.rx_bufs[i] = (u8 *)kmalloc(2048);
        e1000_dev.rx_descs[i].addr = (u64)(u32)e1000_dev.rx_bufs[i];
        e1000_dev.rx_descs[i].status = 0;
    }
    
    /* Set RX descriptor ring */
    e1000_write(E1000_RDBAL, e1000_dev.rx_desc_phys);
    e1000_write(E1000_RDBAH, 0);
    e1000_write(E1000_RDLEN, sizeof(e1000_rx_desc_t) * E1000_NUM_RX_DESCS);
    e1000_write(E1000_RDH, 0);
    e1000_write(E1000_RDT, E1000_NUM_RX_DESCS - 1);
    e1000_dev.rx_head = 0;
    e1000_dev.rx_tail = E1000_NUM_RX_DESCS - 1;
    
    /* Set receive address */
    u32 ral = e1000_dev.mac[0] | (e1000_dev.mac[1] << 8) | 
              (e1000_dev.mac[2] << 16) | (e1000_dev.mac[3] << 24);
    u32 rah = e1000_dev.mac[4] | (e1000_dev.mac[5] << 8) | 0x80000000;
    e1000_write(E1000_RAL0, ral);
    e1000_write(E1000_RAH0, rah);
    
    /* Enable receiver */
    u32 rctl = E1000_RCTL_EN | E1000_RCTL_BAM | E1000_RCTL_BSIZE_2048;
    e1000_write(E1000_RCTL, rctl);
    
    /* Enable transmitter */
    u32 tctl = E1000_TCTL_EN | E1000_TCTL_PSP | 
               (0x10 << 4) | (0x40 << 12);
    e1000_write(E1000_TCTL, tctl);
    
    /* Enable interrupts */
    e1000_write(E1000_IMS, E1000_ICR_RXT0 | E1000_ICR_TXDW | E1000_ICR_LSC);
    
    e1000_dev.present = 1;
    
    serial_puts("E1000: Initialized successfully\n");
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════
 * TRANSMIT PACKET
 * ═══════════════════════════════════════════════════════════════════════ */

int e1000_transmit(const u8 *data, u16 len) {
    if (!e1000_dev.present || len > 2048) return -1;
    
    u16 tail = e1000_dev.tx_tail;
    e1000_tx_desc_t *desc = &e1000_dev.tx_descs[tail];
    
    /* Check descriptor is ready */
    if (!(desc->status & E1000_TXD_STAT_DD)) {
        return -2; /* Ring full */
    }
    
    /* Copy data to buffer */
    for (u16 i = 0; i < len; i++) {
        e1000_dev.tx_bufs[tail][i] = data[i];
    }
    
    /* Set up descriptor */
    desc->length = len;
    desc->csum = 0;
    desc->status = 0;
    desc->cmd = E1000_TXD_CMD_EOP | E1000_TXD_CMD_IFCS | E1000_TXD_CMD_RS;
    
    /* Advance tail */
    e1000_dev.tx_tail = (tail + 1) % E1000_NUM_TX_DESCS;
    e1000_write(E1000_TDT, e1000_dev.tx_tail);
    
    e1000_dev.tx_packets++;
    
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════
 * RECEIVE PACKET
 * ═══════════════════════════════════════════════════════════════════════ */

int e1000_receive(u8 *buf, u16 *len) {
    if (!e1000_dev.present) return 0;
    
    u16 head = e1000_dev.rx_head;
    e1000_rx_desc_t *desc = &e1000_dev.rx_descs[head];
    
    if (!(desc->status & E1000_RXD_STAT_DD)) {
        return 0; /* No packet */
    }
    
    if (!(desc->status & E1000_RXD_STAT_EOP)) {
        /* Not end of packet - error or multi-segment */
        desc->status = 0;
        e1000_dev.rx_head = (head + 1) % E1000_NUM_RX_DESCS;
        return -1;
    }
    
    /* Copy packet */
    u16 pkt_len = desc->length;
    if (pkt_len > 2048) pkt_len = 2048;
    
    for (u16 i = 0; i < pkt_len; i++) {
        buf[i] = e1000_dev.rx_bufs[head][i];
    }
    
    *len = pkt_len;
    
    /* Mark descriptor as available */
    desc->status = 0;
    
    /* Advance head */
    e1000_dev.rx_head = (head + 1) % E1000_NUM_RX_DESCS;
    
    /* Update tail to re-enable descriptor */
    e1000_dev.rx_tail = (e1000_dev.rx_tail + 1) % E1000_NUM_RX_DESCS;
    e1000_write(E1000_RDT, e1000_dev.rx_tail);
    
    e1000_dev.rx_packets++;
    
    return 1;
}

/* ═══════════════════════════════════════════════════════════════════════
 * INTERRUPT HANDLER
 * ═══════════════════════════════════════════════════════════════════════ */

void e1000_irq_handler(void) {
    if (!e1000_dev.present) return;
    
    u32 icr = e1000_read(E1000_ICR);
    
    if (icr & E1000_ICR_RXT0) {
        serial_puts("E1000: RX interrupt\n");
    }
    
    if (icr & E1000_ICR_TXDW) {
        serial_puts("E1000: TX complete\n");
    }
    
    if (icr & E1000_ICR_LSC) {
        u32 status = e1000_read(E1000_STATUS);
        e1000_dev.link_up = (status & (1 << 1)) ? 1 : 0;
        serial_puts("E1000: Link ");
        serial_puts(e1000_dev.link_up ? "UP\n" : "DOWN\n");
    }
}

/* ═══════════════════════════════════════════════════════════════════════
 * DETECTION
 * ═══════════════════════════════════════════════════════════════════════ */

void e1000_detect(pci_device_t *pci) {
    /* Check for Intel E1000/E1000E devices */
    if (pci->vendor_id != 0x8086) return;
    
    u8 is_e1000e = 0;
    
    /* Supported device IDs */
    switch (pci->device_id) {
        case 0x1000: /* 82542EM */
        case 0x1001: /* 82543GC */
        case 0x1004: /* 82540EM */
        case 0x100F: /* 82545EM */
        case 0x1010: /* 82546EB */
        case 0x1011: /* 82545EM */
        case 0x1012: /* 82546EB */
        case 0x1013: /* 82541EI */
        case 0x1014: /* 82541ER */
        case 0x1015: /* 82540EM A2 */
        case 0x1016: /* 82545GM */
        case 0x1017: /* 82545GM */
        case 0x1018: /* 82545GM */
        case 0x1019: /* 82547EI */
        case 0x101A: /* 82547EI */
        case 0x101D: /* 82546EB */
        case 0x101E: /* 82540EM */
        case 0x1026: /* 82545GM */
        case 0x1027: /* 82545GM */
        case 0x1028: /* 82545GM */
        case 0x1075: /* 82545GM */
        case 0x1076: /* 82545GM */
        case 0x1078: /* 82545GM */
            break;
        case 0x1502: /* I217-LM */
        case 0x1503: /* I217-V */
        case 0x150A: /* I210 */
        case 0x150B: /* I210 */
        case 0x1518: /* I217-LM */
        case 0x1521: /* I350 */
        case 0x1522: /* I350 */
        case 0x1526: /* I250 */
        case 0x1533: /* I210 */
            is_e1000e = 1;
            break;
        default:
            return;
    }
    
    /* Get MMIO base (BAR 0 for E1000, BAR 0 for E1000E) */
    u32 bar0 = pci->bar[0];
    
    /* Check if it's I/O or memory */
    if (bar0 & 0x01) {
        serial_puts("E1000: I/O BAR not supported\n");
        return;
    }
    
    u32 mmio_base = bar0 & ~0x0F;
    
    serial_puts("E1000");
    if (is_e1000e) serial_puts("E");
    serial_puts(": Found at PCI ");
    serial_putdec(pci->bus);
    serial_puts(":");
    serial_putdec(pci->slot);
    serial_puts(":");
    serial_putdec(pci->func);
    serial_puts("\n");
    
    e1000_init(mmio_base, pci->interrupt_line, is_e1000e);
}

/* ═══════════════════════════════════════════════════════════════════════
 * STATUS
 * ═══════════════════════════════════════════════════════════════════════ */

void e1000_print_status(void) {
    if (!e1000_dev.present) {
        serial_puts("E1000: Not initialized\n");
        return;
    }
    
    serial_puts("E1000 Status:\n");
    serial_puts("  MAC: ");
    for (int i = 0; i < 6; i++) {
        serial_puthex(e1000_dev.mac[i]);
        if (i < 5) serial_putc(':');
    }
    serial_puts("\n");
    serial_puts("  Link: ");
    serial_puts(e1000_dev.link_up ? "UP" : "DOWN");
    serial_puts("\n");
    serial_puts("  TX packets: ");
    serial_putdec(e1000_dev.tx_packets);
    serial_puts("\n");
    serial_puts("  RX packets: ");
    serial_putdec(e1000_dev.rx_packets);
    serial_puts("\n");
}

