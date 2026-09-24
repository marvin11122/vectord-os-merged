/* ═══════════════════════════════════════════════════════════════════════
 * VECTORD OS - PCI Bus Driver
 * ═══════════════════════════════════════════════════════════════════════ */

#include "../include/vecord.h"

/* ═══════════════════════════════════════════════════════════════════════
 * PCI CONFIGURATION SPACE ACCESS
 * ═══════════════════════════════════════════════════════════════════════ */

#define PCI_CONFIG_ADDRESS  0xCF8
#define PCI_CONFIG_DATA     0xCFC

/* Known vendor IDs */
#define VENDOR_INTEL        0x8086
#define VENDOR_AMD          0x1022
#define VENDOR_NVIDIA       0x10DE
#define VENDOR_VIRTIO       0x1AF4
#define VENDOR_QEMU         0x1234
#define VENDOR_BOCHS        0x1234

/* PCI class codes */
#define PCI_CLASS_NONE              0x00
#define PCI_CLASS_STORAGE           0x01
#define PCI_CLASS_NETWORK           0x02
#define PCI_CLASS_DISPLAY           0x03
#define PCI_CLASS_MULTIMEDIA        0x04
#define PCI_CLASS_MEMORY            0x05
#define PCI_CLASS_BRIDGE            0x06
#define PCI_CLASS_COMMUNICATION     0x07
#define PCI_CLASS_SYSTEM            0x08
#define PCI_CLASS_INPUT             0x09
#define PCI_CLASS_DOCKING           0x0A
#define PCI_CLASS_PROCESSOR         0x0B
#define PCI_CLASS_SERIAL            0x0C

/* Storage subclasses */
#define PCI_SUBCLASS_SATA       0x06
#define PCI_SUBCLASS_IDE        0x01
#define PCI_SUBCLASS_SCSI       0x00

/* Display subclasses */
#define PCI_SUBCLASS_VGA        0x00

/* Serial subclasses */
#define PCI_SUBCLASS_USB        0x03

/* ═══════════════════════════════════════════════════════════════════════
 * GLOBAL STATE
 * ═══════════════════════════════════════════════════════════════════════ */

pci_device_t pci_devices[PCI_MAX_DEVICES];
int pci_device_count = 0;

/* ═══════════════════════════════════════════════════════════════════════
 * PCI CONFIGURATION READ/WRITE
 * ═══════════════════════════════════════════════════════════════════════ */

u32 pci_config_read(u8 bus, u8 slot, u8 func, u8 offset) {
    u32 address = (u32)((bus << 16) | (slot << 11) | (func << 8) | (offset & 0xFC) | 0x80000000);
    outl(PCI_CONFIG_ADDRESS, address);
    return inl(PCI_CONFIG_DATA);
}

static u16 pci_config_read_word(u8 bus, u8 slot, u8 func, u8 offset) {
    u32 val = pci_config_read(bus, slot, func, offset & 0xFC);
    return (u16)((val >> ((offset & 2) * 8)) & 0xFFFF);
}

static u8 pci_config_read_byte(u8 bus, u8 slot, u8 func, u8 offset) {
    u32 val = pci_config_read(bus, slot, func, offset & 0xFC);
    return (u8)((val >> ((offset & 3) * 8)) & 0xFF);
}

/* TODO: Implement PCI config write when needed
static void pci_config_write(u8 bus, u8 slot, u8 func, u8 offset, u32 value) {
    u32 address = (u32)((bus << 16) | (slot << 11) | (func << 8) | (offset & 0xFC) | 0x80000000);
    outl(PCI_CONFIG_ADDRESS, address);
    outl(PCI_CONFIG_DATA, value);
}
*/

/* ═══════════════════════════════════════════════════════════════════════
 * PCI DEVICE CHECK
 * ═══════════════════════════════════════════════════════════════════════ */

static int pci_device_exists(u8 bus, u8 slot, u8 func) {
    u16 vendor = pci_config_read_word(bus, slot, func, 0x00);
    return (vendor != 0xFFFF);
}

/* ═══════════════════════════════════════════════════════════════════════
 * PCI BAR READ
 * ═══════════════════════════════════════════════════════════════════════ */

static u32 pci_read_bar(u8 bus, u8 slot, u8 func, int bar_num) {
    u8 offset = 0x10 + (bar_num * 4);
    return pci_config_read(bus, slot, func, offset);
}

/* ═══════════════════════════════════════════════════════════════════════
 * SCAN SINGLE DEVICE
 * ═══════════════════════════════════════════════════════════════════════ */

static void pci_scan_device(u8 bus, u8 slot, u8 func) {
    if (!pci_device_exists(bus, slot, func)) return;
    if (pci_device_count >= PCI_MAX_DEVICES) return;

    pci_device_t *dev = &pci_devices[pci_device_count];
    
    dev->vendor_id = pci_config_read_word(bus, slot, func, 0x00);
    dev->device_id = pci_config_read_word(bus, slot, func, 0x02);
    dev->class_code = pci_config_read_byte(bus, slot, func, 0x0B);
    dev->subclass = pci_config_read_byte(bus, slot, func, 0x0A);
    dev->prog_if = pci_config_read_byte(bus, slot, func, 0x09);
    dev->revision = pci_config_read_byte(bus, slot, func, 0x08);
    dev->header_type = pci_config_read_byte(bus, slot, func, 0x0E);
    dev->bus = bus;
    dev->slot = slot;
    dev->func = func;
    
    /* Read BARs */
    for (int i = 0; i < 6; i++) {
        dev->bar[i] = pci_read_bar(bus, slot, func, i);
    }
    
    dev->interrupt_line = pci_config_read_byte(bus, slot, func, 0x3C);
    dev->interrupt_pin = pci_config_read_byte(bus, slot, func, 0x3D);
    
    pci_device_count++;
    
    /* Scan functions on multi-function devices */
    if (func == 0 && (dev->header_type & 0x80)) {
        for (int f = 1; f < 8; f++) {
            if (pci_device_exists(bus, slot, f)) {
                pci_scan_device(bus, slot, f);
            }
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════════
 * SCAN PCI BUS
 * ═══════════════════════════════════════════════════════════════════════ */

void pci_scan_bus(void) {
    /* Check if PCI is available */
    if (!pci_device_exists(0, 0, 0)) {
        serial_puts("PCI: Not available\n");
        return;
    }
    
    /* Check header type for host bridge */
    u8 header = pci_config_read_byte(0, 0, 0, 0x0E);
    
    if ((header & 0x01) == 0) {
        /* Single PCI host controller */
        for (int bus_num = 0; bus_num < 256; bus_num++) {
            for (int slot = 0; slot < 32; slot++) {
                if (pci_device_exists(bus_num, slot, 0)) {
                    pci_scan_device(bus_num, slot, 0);
                }
            }
        }
    } else {
        /* PCI host bridge with multiple buses */
        for (int func = 0; func < 8; func++) {
            if (pci_device_exists(0, 0, func)) {
                u32 bus_range = pci_config_read(0, 0, func, 0x18);
                u8 secondary_bus = (bus_range >> 8) & 0xFF;
                u8 subordinate_bus = (bus_range >> 16) & 0xFF;
                
                for (int bus_num = secondary_bus; bus_num <= subordinate_bus; bus_num++) {
                    for (int slot = 0; slot < 32; slot++) {
                        if (pci_device_exists(bus_num, slot, 0)) {
                            pci_scan_device(bus_num, slot, 0);
                        }
                    }
                }
            }
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════════
 * PCI FIND FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════ */

pci_device_t *pci_find_class(u8 class, u8 subclass) {
    for (int i = 0; i < pci_device_count; i++) {
        if (pci_devices[i].class_code == class && pci_devices[i].subclass == subclass) {
            return &pci_devices[i];
        }
    }
    return NULL;
}

pci_device_t *pci_find_vendor_device(u16 vendor, u16 device) {
    for (int i = 0; i < pci_device_count; i++) {
        if (pci_devices[i].vendor_id == vendor && pci_devices[i].device_id == device) {
            return &pci_devices[i];
        }
    }
    return NULL;
}

/* ═══════════════════════════════════════════════════════════════════════
 * PCI INITIALIZATION
 * ═══════════════════════════════════════════════════════════════════════ */

void pci_init(void) {
    pci_device_count = 0;
    pci_scan_bus();
}

