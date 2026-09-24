/* ═══════════════════════════════════════════════════════════════════════
 * VECTORD OS - AHCI/SATA Driver
 * Minimal AHCI controller detection and basic port info
 * ═══════════════════════════════════════════════════════════════════════ */

#include "../include/vecord.h"

/* ═══════════════════════════════════════════════════════════════════════
 * AHCI REGISTERS (HBA Memory Map)
 * ═══════════════════════════════════════════════════════════════════════ */

#define AHCI_GHC       0x04   /* Global Host Control */
#define AHCI_IS        0x08   /* Interrupt Status */
#define AHCI_PI        0x0C   /* Ports Implemented */
#define AHCI_VS        0x10   /* Version */
#define AHCI_CAP       0x00   /* Host Capabilities */

/* Port registers (per port, stride 0x80) */
#define AHCI_PORT_CLB   0x00   /* Command List Base Address */
#define AHCI_PORT_CLBU  0x04   /* Command List Base Address Upper */
#define AHCI_PORTxFB    0x08   /* FIS Base Address */
#define AHCI_PORTxFBU   0x0C   /* FIS Base Address Upper */
#define AHCI_PORT_IS    0x10   /* Port Interrupt Status */
#define AHCI_PORT_IE    0x14   /* Port Interrupt Enable */
#define AHCI_PORT_CMD   0x18   /* Port Command and Status */
#define AHCI_PORT_TFD   0x20   /* Task File Data */
#define AHCI_PORT_SIG   0x24   /* Signature */
#define AHCI_PORT_SSTS  0x28   /* Serial ATA Status */
#define AHCI_PORT_SCTL  0x2C   /* Serial ATA Control */
#define AHCI_PORT_SERR  0x30   /* Serial ATA Error */
#define AHCI_PORT_SACT  0x34   /* Serial ATA Active */

/* AHCI Global Host Control bits */
#define AHCI_GHC_HBA_RESET  (1 << 0)
#define AHCI_GHC_AHCI_ENABLE (1 << 31)

/* Port Command bits */
#define AHCI_CMD_ST    (1 << 0)  /* Start */
#define AHCI_CMD_FRE   (1 << 4)  /* FIS Receive Enable */

/* Port Signature values */
#define AHCI_SIG_ATA   0x00000101   /* ATA device */
#define AHCI_SIG_ATAPI 0xEB140101   /* ATAPI device */
#define AHCI_SIG_SEMB  0xC33C0101   /* Enclosure management bridge */
#define AHCI_SIG_PORT_MULTIPLIER 0x96690101 /* Port multiplier */

/* ═══════════════════════════════════════════════════════════════════════
 * GLOBAL STATE
 * ═══════════════════════════════════════════════════════════════════════ */

ahci_controller_t ahci_controllers[AHCI_MAX_CONTROLLERS];
int ahci_controller_count = 0;

/* ═══════════════════════════════════════════════════════════════════════
 * AHCI REGISTER ACCESS
 * ═══════════════════════════════════════════════════════════════════════ */

static u32 ahci_read(ahci_controller_t *ctrl, u32 offset) {
    volatile u32 *reg = (volatile u32 *)(ctrl->base + offset);
    return *reg;
}

static void ahci_write(ahci_controller_t *ctrl, u32 offset, u32 value) {
    volatile u32 *reg = (volatile u32 *)(ctrl->base + offset);
    *reg = value;
}

/* ═══════════════════════════════════════════════════════════════════════
 * DETECT PORT TYPE
 * ═══════════════════════════════════════════════════════════════════════ */

static const char* ahci_port_type(u32 signature) {
    switch (signature) {
        case AHCI_SIG_ATA: return "ATA";
        case AHCI_SIG_ATAPI: return "ATAPI";
        case AHCI_SIG_SEMB: return "SEMB";
        case AHCI_SIG_PORT_MULTIPLIER: return "PM";
        default: return "Unknown";
    }
}

/* ═══════════════════════════════════════════════════════════════════════
 * SCAN AHCI PORTS
 * ═══════════════════════════════════════════════════════════════════════ */

static void ahci_scan_ports(ahci_controller_t *ctrl) {
    u32 ports_impl = ahci_read(ctrl, AHCI_PI);
    ctrl->ports_implemented = ports_impl;
    ctrl->port_count = 0;
    
    serial_puts("AHCI: Ports implemented: 0x");
    serial_puthex(ports_impl);
    serial_puts("\n");
    
    for (int i = 0; i < 32; i++) {
        if (ports_impl & (1 << i)) {
            /* Read port signature */
            u32 sig = ahci_read(ctrl, 0x100 + i * 0x80 + AHCI_PORT_SIG);
            u32 ssts = ahci_read(ctrl, 0x100 + i * 0x80 + AHCI_PORT_SSTS);
            
            /* Check if device present (Interface Power Management Status) */
            u8 det = ssts & 0x0F;  /* Device Detection */
            u8 ipm = (ssts >> 8) & 0x0F; /* Interface Power Management */
            
            if (det == 0x3 && ipm == 0x1) {
                ctrl->port_count++;
                
                serial_puts("AHCI: Port ");
                serial_putdec(i);
                serial_puts(": ");
                serial_puts(ahci_port_type(sig));
                serial_puts(" (SIG=0x");
                serial_puthex(sig);
                serial_puts(")\n");
            }
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════════
 * DETECT AHCI CONTROLLER
 * ═══════════════════════════════════════════════════════════════════════ */

void ahci_detect(u32 base, u8 irq) {
    if (ahci_controller_count >= AHCI_MAX_CONTROLLERS) return;
    
    ahci_controller_t *ctrl = &ahci_controllers[ahci_controller_count];
    ctrl->base = base;
    ctrl->irq = irq;
    ctrl->present = 1;
    
    /* Read version */
    u32 version = ahci_read(ctrl, AHCI_VS);
    serial_puts("AHCI: Controller @ 0x");
    serial_puthex(base);
    serial_puts(" v");
    serial_putdec((version >> 16) & 0xFF);
    serial_puts(".");
    serial_putdec(version & 0xFF);
    serial_puts("\n");
    
    /* Enable AHCI mode */
    u32 ghc = ahci_read(ctrl, AHCI_GHC);
    if (!(ghc & AHCI_GHC_AHCI_ENABLE)) {
        ahci_write(ctrl, AHCI_GHC, ghc | AHCI_GHC_AHCI_ENABLE);
    }
    
    /* Scan ports */
    ahci_scan_ports(ctrl);
    
    ahci_controller_count++;
}

/* ═══════════════════════════════════════════════════════════════════════
 * AHCI INITIALIZATION
 * ═══════════════════════════════════════════════════════════════════════ */

void ahci_init(void) {
    ahci_controller_count = 0;
    serial_puts("AHCI: SATA controller detection ready\n");
}

