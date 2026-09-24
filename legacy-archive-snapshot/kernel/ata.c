/* ═══════════════════════════════════════════════════════════════════════
 * VECTORD OS - ATA/IDE Disk Driver
 * Supports PIO mode for reading/writing disk sectors
 * ═══════════════════════════════════════════════════════════════════════ */

#include "../include/vecord.h"

/* ═══════════════════════════════════════════════════════════════════════
 * ATA REGISTERS (PIO Mode)
 * ═══════════════════════════════════════════════════════════════════════ */

/* Primary channel */
#define ATA_PRI_BASE        0x1F0
#define ATA_PRI_CTRL        0x3F6

/* Secondary channel */
#define ATA_SEC_BASE        0x170
#define ATA_SEC_CTRL        0x376

/* Register offsets from base */
#define ATA_REG_DATA        0   /* Data (R/W) */
#define ATA_REG_ERROR       1   /* Error (R) */
#define ATA_REG_NSECT       2   /* Sector count (R/W) */
#define ATA_REG_LBAL        3   /* LBA low (R/W) */
#define ATA_REG_LBAM        4   /* LBA mid (R/W) */
#define ATA_REG_LBAH        5   /* LBA high (R/W) */
#define ATA_REG_DEVICE      6   /* Device (R/W) */
#define ATA_REG_STATUS      7   /* Status (R) */
#define ATA_REG_COMMAND     7   /* Command (W) */

/* Control register offsets */
#define ATA_REG_ALTSTATUS   0   /* Alternate status (R) */
#define ATA_REG_CONTROL     0   /* Device control (W) */

/* Status register bits */
#define ATA_SR_BSY          0x80    /* Busy */
#define ATA_SR_DRDY         0x40    /* Drive ready */
#define ATA_SR_DF           0x20    /* Device fault */
#define ATA_SR_DSC          0x10    /* Device seek complete */
#define ATA_SR_DRQ          0x08    /* Data request */
#define ATA_SR_CORR         0x04    /* Corrected data */
#define ATA_SR_IDX          0x02    /* Index */
#define ATA_SR_ERR          0x01    /* Error */

/* Error register bits */
#define ATA_ER_BBK          0x80    /* Bad block */
#define ATA_ER_UNC          0x40    /* Uncorrectable */
#define ATA_ER_MC           0x20    /* Media changed */
#define ATA_ER_IDNF         0x10    /* ID not found */
#define ATA_ER_MCR          0x08    /* Media change request */
#define ATA_ER_ABRT         0x04    /* Command aborted */
#define ATA_ER_TK0NF        0x02    /* Track 0 not found */
#define ATA_ER_AMNF         0x01    /* Address mark not found */

/* Commands */
#define ATA_CMD_READ_PIO        0x20    /* Read sectors (PIO) */
#define ATA_CMD_READ_PIO_EXT    0x24    /* Read sectors (PIO, LBA48) */
#define ATA_CMD_WRITE_PIO       0x30    /* Write sectors (PIO) */
#define ATA_CMD_WRITE_PIO_EXT   0x34    /* Write sectors (PIO, LBA48) */
#define ATA_CMD_IDENTIFY        0xEC    /* Identify drive */

/* Device/Head register */
#define ATA_DEV_MASTER      0xA0
#define ATA_DEV_SLAVE       0xB0
#define ATA_DEV_LBA         0x40    /* LBA mode */

/* Identify data offsets */
#define ATA_IDENT_SERIAL    20      /* Serial number (20 bytes) */
#define ATA_IDENT_MODEL     54      /* Model string (40 bytes) */
#define ATA_IDENT_CAPS      49      /* Capabilities */
#define ATA_IDENT_LBA_SECTORS 60    /* LBA sectors (4 bytes) */

/* ═══════════════════════════════════════════════════════════════════════
 * GLOBAL STATE
 * ═══════════════════════════════════════════════════════════════════════ */

ata_device_t ata_devices[ATA_MAX_DEVICES];
int ata_device_count = 0;

/* ═══════════════════════════════════════════════════════════════════════
 * HELPERS
 * ═══════════════════════════════════════════════════════════════════════ */

static void ata_delay(u16 ctrl) {
    /* Read alternate status 4 times for 400ns delay */
    inb(ctrl + ATA_REG_ALTSTATUS);
    inb(ctrl + ATA_REG_ALTSTATUS);
    inb(ctrl + ATA_REG_ALTSTATUS);
    inb(ctrl + ATA_REG_ALTSTATUS);
}

static int ata_wait_busy(u16 base, u16 ctrl, int timeout_ms) {
    (void)ctrl; /* Control register not used in this implementation */
    for (int i = 0; i < timeout_ms * 100; i++) {
        u8 status = inb(base + ATA_REG_STATUS);
        if (!(status & ATA_SR_BSY)) {
            return (status & ATA_SR_ERR) ? -1 : 0;
        }
        /* Small delay */
        for (volatile int j = 0; j < 100; j++);
    }
    return -2; /* Timeout */
}

static int ata_wait_drq(u16 base, u16 ctrl, int timeout_ms) {
    (void)ctrl; /* Control register not used in this implementation */
    for (int i = 0; i < timeout_ms * 100; i++) {
        u8 status = inb(base + ATA_REG_STATUS);
        if (status & ATA_SR_ERR) return -1;
        if (status & ATA_SR_DRQ) return 0;
        if (!(status & ATA_SR_BSY)) return -2;
        for (volatile int j = 0; j < 100; j++);
    }
    return -2; /* Timeout */
}

/* ═══════════════════════════════════════════════════════════════════════
 * IDENTIFY DRIVE
 * ═══════════════════════════════════════════════════════════════════════ */

static int ata_identify(u16 base, u16 ctrl, u8 slave, ata_device_t *dev) {
    /* Select device */
    outb(base + ATA_REG_DEVICE, slave ? ATA_DEV_SLAVE : ATA_DEV_MASTER);
    ata_delay(ctrl);
    
    /* Clear registers */
    outb(base + ATA_REG_NSECT, 0);
    outb(base + ATA_REG_LBAL, 0);
    outb(base + ATA_REG_LBAM, 0);
    outb(base + ATA_REG_LBAH, 0);
    
    /* Send IDENTIFY command */
    outb(base + ATA_REG_COMMAND, ATA_CMD_IDENTIFY);
    ata_delay(ctrl);
    
    /* Check if drive exists */
    u8 status = inb(base + ATA_REG_STATUS);
    if (status == 0 || (status & ATA_SR_ERR)) {
        return -1; /* No drive */
    }
    
    /* Wait for data */
    if (ata_wait_drq(base, ctrl, 1000) != 0) {
        return -2;
    }
    
    /* Read identify data */
    u16 identify[256];
    for (int i = 0; i < 256; i++) {
        identify[i] = inw(base + ATA_REG_DATA);
    }
    
    /* Fill device structure */
    dev->base_port = base;
    dev->ctrl_port = ctrl;
    dev->slave = slave;
    dev->present = 1;
    
    /* Get total sectors (LBA28) */
    dev->sectors = identify[ATA_IDENT_LBA_SECTORS] | 
                   ((u32)identify[ATA_IDENT_LBA_SECTORS + 1] << 16);
    
    /* Copy model string (swap bytes) */
    for (int i = 0; i < 40; i += 2) {
        dev->model[i] = (char)(identify[ATA_IDENT_MODEL + i/2] >> 8);
        dev->model[i + 1] = (char)(identify[ATA_IDENT_MODEL + i/2] & 0xFF);
    }
    dev->model[40] = '\0';
    
    /* Copy serial string */
    for (int i = 0; i < 20; i += 2) {
        dev->serial[i] = (char)(identify[ATA_IDENT_SERIAL + i/2] >> 8);
        dev->serial[i + 1] = (char)(identify[ATA_IDENT_SERIAL + i/2] & 0xFF);
    }
    dev->serial[20] = '\0';
    
    /* Trim whitespace from strings */
    char *p;
    for (p = dev->model + 39; p > dev->model && (*p == ' ' || *p == '\0'); p--) *p = '\0';
    for (p = dev->serial + 19; p > dev->serial && (*p == ' ' || *p == '\0'); p--) *p = '\0';
    
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════
 * READ SECTORS (PIO)
 * ═══════════════════════════════════════════════════════════════════════ */

int ata_read_sectors(ata_device_t *dev, u32 lba, u8 count, u16 *buffer) {
    if (!dev->present) return -1;
    
    u16 base = dev->base_port;
    u16 ctrl = dev->ctrl_port;
    
    /* Wait for drive ready */
    if (ata_wait_busy(base, ctrl, 1000) != 0) return -1;
    
    /* Select device */
    outb(base + ATA_REG_DEVICE, (dev->slave ? ATA_DEV_SLAVE : ATA_DEV_MASTER) | 
          ATA_DEV_LBA | ((lba >> 24) & 0x0F));
    
    /* Set registers */
    outb(base + ATA_REG_NSECT, count);
    outb(base + ATA_REG_LBAL, (u8)(lba & 0xFF));
    outb(base + ATA_REG_LBAM, (u8)((lba >> 8) & 0xFF));
    outb(base + ATA_REG_LBAH, (u8)((lba >> 16) & 0xFF));
    
    /* Send command */
    outb(base + ATA_REG_COMMAND, ATA_CMD_READ_PIO);
    
    /* Read sectors */
    for (int s = 0; s < count; s++) {
        if (ata_wait_drq(base, ctrl, 3000) != 0) return -1;
        
        for (int i = 0; i < 256; i++) {
            buffer[s * 256 + i] = inw(base + ATA_REG_DATA);
        }
    }
    
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════
 * WRITE SECTORS (PIO)
 * ═══════════════════════════════════════════════════════════════════════ */

int ata_write_sectors(ata_device_t *dev, u32 lba, u8 count, const u16 *buffer) {
    if (!dev->present) return -1;
    
    u16 base = dev->base_port;
    u16 ctrl = dev->ctrl_port;
    
    /* Wait for drive ready */
    if (ata_wait_busy(base, ctrl, 1000) != 0) return -1;
    
    /* Select device */
    outb(base + ATA_REG_DEVICE, (dev->slave ? ATA_DEV_SLAVE : ATA_DEV_MASTER) | 
          ATA_DEV_LBA | ((lba >> 24) & 0x0F));
    
    /* Set registers */
    outb(base + ATA_REG_NSECT, count);
    outb(base + ATA_REG_LBAL, (u8)(lba & 0xFF));
    outb(base + ATA_REG_LBAM, (u8)((lba >> 8) & 0xFF));
    outb(base + ATA_REG_LBAH, (u8)((lba >> 16) & 0xFF));
    
    /* Send command */
    outb(base + ATA_REG_COMMAND, ATA_CMD_WRITE_PIO);
    
    /* Write sectors */
    for (int s = 0; s < count; s++) {
        if (ata_wait_drq(base, ctrl, 3000) != 0) return -1;
        
        for (int i = 0; i < 256; i++) {
            outw(base + ATA_REG_DATA, buffer[s * 256 + i]);
        }
    }
    
    /* Wait for completion */
    ata_wait_busy(base, ctrl, 1000);
    
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════
 * DETECT ATA DEVICES
 * ═══════════════════════════════════════════════════════════════════════ */

void ata_detect(u16 base, u16 ctrl, u8 slave) {
    if (ata_device_count >= ATA_MAX_DEVICES) return;
    
    ata_device_t *dev = &ata_devices[ata_device_count];
    
    if (ata_identify(base, ctrl, slave, dev) == 0) {
        /* Calculate size in MB */
        u32 size_mb = (u32)(dev->sectors / 2048);
        
        serial_puts("ATA: ");
        serial_puts(slave ? "Slave" : "Master");
        serial_puts(" @ 0x");
        serial_puthex(base);
        serial_puts(" - ");
        serial_puts(dev->model);
        serial_puts(" (");
        serial_putdec(size_mb);
        serial_puts(" MB)\n");
        
        ata_device_count++;
    }
}

/* ═══════════════════════════════════════════════════════════════════════
 * ATA INITIALIZATION
 * ═══════════════════════════════════════════════════════════════════════ */

void ata_init(void) {
    ata_device_count = 0;
    
    serial_puts("ATA: Detecting drives...\n");
    
    /* Scan primary channel */
    ata_detect(ATA_PRI_BASE, ATA_PRI_CTRL, 0); /* Master */
    ata_detect(ATA_PRI_BASE, ATA_PRI_CTRL, 1); /* Slave */
    
    /* Scan secondary channel */
    ata_detect(ATA_SEC_BASE, ATA_SEC_CTRL, 0); /* Master */
    ata_detect(ATA_SEC_BASE, ATA_SEC_CTRL, 1); /* Slave */
    
    serial_puts("ATA: ");
    serial_putdec(ata_device_count);
    serial_puts(" device(s) found\n");
}
