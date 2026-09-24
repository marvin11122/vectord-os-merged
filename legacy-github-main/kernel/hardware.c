/* ═══════════════════════════════════════════════════════════════════════
 * VECTORD OS - Hardware Detection & Reporting
 * Collects and displays hardware information
 * ═══════════════════════════════════════════════════════════════════════ */

#include "../include/vecord.h"

/* ═══════════════════════════════════════════════════════════════════════
 * CPUID INSTRUCTION
 * ═══════════════════════════════════════════════════════════════════════ */

static void cpuid(u32 leaf, u32 *eax, u32 *ebx, u32 *ecx, u32 *edx) {
    asm volatile ("cpuid"
        : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx)
        : "a"(leaf), "c"(0)
    );
}

/* ═══════════════════════════════════════════════════════════════════════
 * CPU DETECTION
 * ═══════════════════════════════════════════════════════════════════════ */

hardware_info_t hw_info;

static void detect_cpu(void) {
    u32 eax, ebx, ecx, edx;
    
    /* Get vendor string (CPUID leaf 0) */
    cpuid(0, &eax, &ebx, &ecx, &edx);
    
    u32 max_leaf = eax;
    
    /* Store vendor string */
    *(u32*)&hw_info.cpu_vendor[0] = ebx;
    *(u32*)&hw_info.cpu_vendor[4] = edx;
    *(u32*)&hw_info.cpu_vendor[8] = ecx;
    hw_info.cpu_vendor[12] = '\0';
    
    serial_puts("CPU: Vendor: ");
    serial_puts(hw_info.cpu_vendor);
    serial_puts("\n");
    
    /* Get processor info (CPUID leaf 1) */
    if (max_leaf >= 1) {
        cpuid(1, &eax, &ebx, &ecx, &edx);
        
        hw_info.cpu_family = ((eax >> 8) & 0x0F) | ((eax >> 20) & 0xFF);
        hw_info.cpu_model = ((eax >> 4) & 0x0F) | ((eax >> 12) & 0xF0);
        hw_info.cpu_stepping = eax & 0x0F;
        
        serial_puts("CPU: Family ");
        serial_putdec(hw_info.cpu_family);
        serial_puts(", Model ");
        serial_putdec(hw_info.cpu_model);
        serial_puts(", Stepping ");
        serial_putdec(hw_info.cpu_stepping);
        serial_puts("\n");
    }
    
    /* Get processor brand string (CPUID leaf 0x80000002-0x80000004) */
    cpuid(0x80000000, &eax, &ebx, &ecx, &edx);
    
    if (eax >= 0x80000004) {
        u32 *brand = (u32*)hw_info.cpu_brand;
        
        cpuid(0x80000002, &brand[0], &brand[1], &brand[2], &brand[3]);
        cpuid(0x80000003, &brand[4], &brand[5], &brand[6], &brand[7]);
        cpuid(0x80000004, &brand[8], &brand[9], &brand[10], &brand[11]);
        
        hw_info.cpu_brand[48] = '\0';
        
        /* Trim whitespace */
        char *p = hw_info.cpu_brand + 47;
        while (p > hw_info.cpu_brand && (*p == ' ' || *p == '\0')) *p-- = '\0';
        
        serial_puts("CPU: ");
        serial_puts(hw_info.cpu_brand);
        serial_puts("\n");
    }
}

/* ═══════════════════════════════════════════════════════════════════════
 * MEMORY DETECTION (via BIOS int 0x15, EAX=0xE820)
 * ═══════════════════════════════════════════════════════════════════════ */

#define E820_MAP_ADDR   0x8000  /* Where bootloader stores E820 map */

typedef struct {
    u64 base;
    u64 length;
    u32 type;
    u32 acpi_ext;
} __attribute__((packed)) e820_entry_t;

static void detect_memory(void) {
    /* Memory map is provided by bootloader at E820_MAP_ADDR */
    /* For now, use a simple estimate based on QEMU default */
    hw_info.ram_total_mb = 64; /* Default QEMU memory */
    
    /* If we have access to the bootloader info, we could parse this */
    serial_puts("RAM: ~");
    serial_putdec(hw_info.ram_total_mb);
    serial_puts(" MB detected\n");
}

/* ═══════════════════════════════════════════════════════════════════════
 * HARDWARE DETECTION ORCHESTRATION
 * ═══════════════════════════════════════════════════════════════════════ */

void hardware_detect(void) {
    serial_puts("\n=== Hardware Detection ===\n");
    
    /* CPU */
    detect_cpu();
    
    /* Memory */
    detect_memory();
    
    /* PCI and associated devices */
    serial_puts("\n--- PCI Devices ---\n");
    pci_init();
    
    /* Find and initialize specific devices found via PCI */
    for (int i = 0; i < pci_device_count; i++) {
        pci_device_t *dev = &pci_devices[i];
        
        /* SATA/AHCI controller (class 0x01, subclass 0x06) */
        if (dev->class_code == 0x01 && dev->subclass == 0x06) {
            serial_puts("Found AHCI/SATA controller\n");
            ahci_detect(dev->bar[5] & ~0xF, dev->interrupt_line);
        }
        
        /* IDE controller (class 0x01, subclass 0x01) */
        if (dev->class_code == 0x01 && dev->subclass == 0x01) {
            serial_puts("Found IDE controller\n");
        }
        
        /* USB controller (class 0x0C, subclass 0x03) */
        if (dev->class_code == 0x0C && dev->subclass == 0x03) {
            usb_detect_controller(dev->vendor_id, dev->device_id, dev->prog_if, 
                                 dev->bar[0], dev->interrupt_line);
        }
        
        /* VGA/Display (class 0x03, subclass 0x00) */
        if (dev->class_code == 0x03 && dev->subclass == 0x00) {
            serial_puts("VGA:xx ");
            serial_puts("xx");
            serial_puts(":");
            serial_puts("yy");
            serial_puts("\n");
        }
        
        /* Network controllers (class 0x02, subclass 0x00) */
        if (dev->class_code == 0x02 && dev->subclass == 0x00) {
            serial_puts("NET:xx ");
            serial_puts("xx");
            serial_puts(":");
            serial_puts("yy");
            serial_puts("\n");
            
            /* RTL8139 */
            if (dev->vendor_id == 0x10EC && dev->device_id == 0x8139) {
                rtl8139_detect(dev);
            }
            
            /* Intel E1000/E1000E */
            if (dev->vendor_id == 0x8086) {
                e1000_detect(dev);
            }
        }
    }
    
    /* ATA/IDE (legacy) */
    serial_puts("\n--- Legacy ATA/IDE ---\n");
    ata_init();
    
    /* VBE/VESA */
    serial_puts("\n--- Graphics ---\n");
    vbe_init();
}

/* ═══════════════════════════════════════════════════════════════════════
 * PRINT HARDWARE INFO (for shell command)
 * ═══════════════════════════════════════════════════════════════════════ */

void hardware_print_info(void) {
    int row = 3;
    u8 title_color = VGA_YELLOW | (VGA_BLACK << 4);
    u8 label_color = VGA_WHITE | (VGA_BLACK << 4);
    u8 value_color = VGA_LIGHT_GREEN | (VGA_BLACK << 4);
    
    vga_print(0, row, "═══════════════════════════════════════════════════════════════════════", title_color);
    row++;
    vga_print(0, row, "  HARDWARE INFORMATION", title_color);
    row++;
    vga_print(0, row, "═══════════════════════════════════════════════════════════════════════", title_color);
    row++;
    row++;
    
    /* CPU */
    vga_print(2, row, "CPU:", label_color);
    row++;
    vga_print(4, row, "Vendor: ", label_color);
    vga_print(12, row, hw_info.cpu_vendor, value_color);
    row++;
    if (hw_info.cpu_brand[0]) {
        vga_print(4, row, "Model:  ", label_color);
        vga_print(12, row, hw_info.cpu_brand, value_color);
        row++;
    }
    vga_print(4, row, "Family: ", label_color);
    vga_put_dec(12, row, hw_info.cpu_family, value_color);
    row++;
    row++;
    
    /* Memory */
    vga_print(2, row, "Memory:", label_color);
    row++;
    vga_print(4, row, "Total:  ", label_color);
    vga_put_dec(12, row, hw_info.ram_total_mb, value_color);
    vga_print(15, row, "MB", value_color);
    row++;
    vga_print(4, row, "Kernel: ", label_color);
    vga_put_dec(12, row, mem_get_total() / 1024, value_color);
    vga_print(18, row, "KB heap", value_color);
    row++;
    row++;
    
    /* PCI Devices */
    vga_print(2, row, "PCI Devices:", label_color);
    row++;
    vga_print(4, row, "Found:  ", label_color);
    vga_put_dec(12, row, pci_device_count, value_color);
    row++;
    
    /* Show some devices */
    int shown = 0;
    for (int i = 0; i < pci_device_count && shown < 5; i++) {
        pci_device_t *dev = &pci_devices[i];
        
        /* Skip empty devices */
        if (dev->class_code == 0x00 && dev->vendor_id == 0xFFFF) continue;
        
        vga_print(4, row, "  ", label_color);
        vga_put_hex(6, row, dev->vendor_id, value_color);
        vga_print(12, row, ":", label_color);
        vga_put_hex(13, row, dev->device_id, value_color);
        vga_print(20, row, "Class:", label_color);
        vga_put_hex(27, row, dev->class_code, value_color);
        vga_print(29, row, ".", label_color);
        vga_put_hex(30, row, dev->subclass, value_color);
        row++;
        shown++;
    }
    if (pci_device_count > 5) {
        vga_print(4, row, "  ... and ", label_color);
        vga_put_dec(14, row, pci_device_count - 5, value_color);
        vga_print(17, row, " more", label_color);
        row++;
    }
    row++;
    
    /* ATA Devices */
    vga_print(2, row, "ATA/IDE:", label_color);
    row++;
    vga_print(4, row, "Found:  ", label_color);
    vga_put_dec(12, row, ata_device_count, value_color);
    row++;
    for (int i = 0; i < ata_device_count; i++) {
        ata_device_t *dev = &ata_devices[i];
        vga_print(4, row, "  ", label_color);
        vga_print(6, row, dev->slave ? "Slave " : "Master", value_color);
        vga_print(14, row, dev->model, value_color);
        u32 size_mb = (u32)(dev->sectors / 2048);
        vga_print(55, row, "(", label_color);
        vga_put_dec(56, row, size_mb, value_color);
        vga_print(62, row, "MB)", label_color);
        row++;
    }
    row++;
    
    /* USB Controllers */
    vga_print(2, row, "USB:", label_color);
    row++;
    vga_print(4, row, "Found:  ", label_color);
    vga_put_dec(12, row, usb_controller_count, value_color);
    row++;
    for (int i = 0; i < usb_controller_count; i++) {
        usb_controller_t *ctrl = &usb_controllers[i];
        vga_print(4, row, "  ", label_color);
        vga_print(6, row, ctrl->type_name, value_color);
        vga_print(14, row, "@ 0x", label_color);
        vga_put_hex(18, row, ctrl->base, value_color);
        vga_print(28, row, "IRQ ", label_color);
        vga_put_dec(32, row, ctrl->irq, value_color);
        row++;
    }
    row++;
    
    /* Graphics */
    vga_print(2, row, "Graphics:", label_color);
    row++;
    vga_print(4, row, "VBE:    ", label_color);
    if (vbe_info.present) {
        vga_print(12, row, "Available", value_color);
        vga_print(22, row, "(", label_color);
        vga_put_dec(23, row, vbe_info.width, value_color);
        vga_print(28, row, "x", label_color);
        vga_put_dec(29, row, vbe_info.height, value_color);
        vga_print(33, row, "@", label_color);
        vga_put_dec(34, row, vbe_info.bpp, value_color);
        vga_print(37, row, "bpp)", value_color);
    } else {
        vga_print(12, row, "Not available", VGA_RED | (VGA_BLACK << 4));
    }
    row++;
}

