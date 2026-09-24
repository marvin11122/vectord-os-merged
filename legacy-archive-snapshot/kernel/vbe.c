/* ═══════════════════════════════════════════════════════════════════════
 * VECTORD OS - VESA/VBE Framebuffer Driver
 * Safe mode setting with timeouts and fallbacks
 * ═══════════════════════════════════════════════════════════════════════ */

#include "../include/vecord.h"

/* Bochs/QEMU VBE DISPI registers */
#define VBE_DISPI_IOPORT_INDEX  0x1CE
#define VBE_DISPI_IOPORT_DATA   0x1CF
#define VBE_DISPI_INDEX_ID      0x0
#define VBE_DISPI_INDEX_XRES    0x1
#define VBE_DISPI_INDEX_YRES    0x2
#define VBE_DISPI_INDEX_BPP     0x3
#define VBE_DISPI_INDEX_ENABLE  0x4

vbe_mode_info_t vbe_info;

/* Safe register access with timeout */
static int vbe_write_safe(u16 index, u16 value) {
    u32 timeout = 10000;
    while (timeout--) {
        /* Check if controller is ready by reading ID */
        outw(VBE_DISPI_IOPORT_INDEX, VBE_DISPI_INDEX_ID);
        u16 id = inw(VBE_DISPI_IOPORT_DATA);
        /* Accept any valid Bochs ID or 0 (not initialized) */
        if ((id & 0xFFF0) == 0xB0C0 || id == 0) break;
    }
    outw(VBE_DISPI_IOPORT_INDEX, index);
    outw(VBE_DISPI_IOPORT_DATA, value);
    return 0;
}

static u16 vbe_read_safe(u16 index) {
    outw(VBE_DISPI_IOPORT_INDEX, index);
    return inw(VBE_DISPI_IOPORT_DATA);
}

void vbe_init(void) {
    vbe_info.present = 0;
    vbe_info.base = 0;

    serial_puts("VBE: Initializing...\n");

    /* Step 1: Get framebuffer address from PCI BAR2 */
    u32 bar2 = pci_config_read(0, 2, 0, 0x18);
    if (bar2 != 0 && bar2 != 0xFFFFFFFF && !(bar2 & 1)) {
        vbe_info.base = bar2 & 0xFFFFFFF0;
    }
    if (vbe_info.base == 0) {
        vbe_info.base = 0xE0000000;
    }
    serial_puts("VBE: FB at 0x");
    serial_puthex(vbe_info.base);
    serial_puts("\n");

    /* Step 2: Check if Bochs VBE is present */
    outw(VBE_DISPI_IOPORT_INDEX, 0);  /* ID register */
    u16 id = inw(VBE_DISPI_IOPORT_DATA);
    serial_puts("VBE: DISPI ID=0x");
    serial_puthex(id);
    serial_puts("\n");

    /* Step 3: Try to set mode */
    if ((id & 0xFFF0) == 0xB0C0) {
        /* Bochs VBE present - use DISPI */
        serial_puts("VBE: Using DISPI registers\n");

        /* Disable first */
        vbe_write_safe(VBE_DISPI_INDEX_ENABLE, 0);

        /* Set 1024x768@32 */
        vbe_write_safe(VBE_DISPI_INDEX_XRES, 1024);
        vbe_write_safe(VBE_DISPI_INDEX_YRES, 768);
        vbe_write_safe(VBE_DISPI_INDEX_BPP, 32);

        /* Enable with LFB */
        vbe_write_safe(VBE_DISPI_INDEX_ENABLE, 0x41);

        /* Read back */
        u16 rx = vbe_read_safe(VBE_DISPI_INDEX_XRES);
        u16 ry = vbe_read_safe(VBE_DISPI_INDEX_YRES);
        u16 rb = vbe_read_safe(VBE_DISPI_INDEX_BPP);
        u16 re = vbe_read_safe(VBE_DISPI_INDEX_ENABLE);

        serial_puts("VBE: Mode set: ");
        serial_putdec(rx); serial_puts("x");
        serial_putdec(ry); serial_puts(" @ ");
        serial_putdec(rb); serial_puts("bpp, en=0x");
        serial_puthex(re);
        serial_puts("\n");

        vbe_info.width = rx;
        vbe_info.height = ry;
        vbe_info.bpp = rb;
    } else {
        /* No DISPI - use VGA default */
        serial_puts("VBE: No DISPI, using VGA default 640x480\n");
        vbe_info.width = 640;
        vbe_info.height = 480;
        vbe_info.bpp = 32;
    }

    vbe_info.pitch = vbe_info.width * (vbe_info.bpp / 8);
    vbe_info.size = vbe_info.pitch * vbe_info.height;
    vbe_info.present = 1;

    serial_puts("VBE: Ready - ");
    serial_putdec(vbe_info.width);
    serial_puts("x");
    serial_putdec(vbe_info.height);
    serial_puts("@");
    serial_putdec(vbe_info.bpp);
    serial_puts(" pitch=");
    serial_putdec(vbe_info.pitch);
    serial_puts("\n");
}

void vbe_test_pattern(void) {
    if (!vbe_info.present || !vbe_info.base) {
        serial_puts("VBE: Not ready\n");
        return;
    }

    u32 *fb = (u32 *)vbe_info.base;
    u32 w = vbe_info.width;
    u32 h = vbe_info.height;
    u32 pitch = vbe_info.pitch / 4;

    serial_puts("VBE: Color bars ");
    serial_putdec(w); serial_puts("x"); serial_putdec(h);
    serial_puts(" at 0x"); serial_puthex(vbe_info.base);
    serial_puts("\n");

    u32 colors[] = {
        0xFF000000, 0xFF0000FF, 0xFF00FF00, 0xFF00FFFF,
        0xFFFF0000, 0xFFFF00FF, 0xFFFFFF00, 0xFFFFFFFF
    };

    for (u32 y = 0; y < h; y++) {
        for (u32 x = 0; x < w; x++) {
            fb[y * pitch + x] = colors[(x * 8) / w & 7];
        }
    }

    /* White horizontal bars at 25%, 50%, 75% height */
    for (u32 y = 0; y < h; y += h/4) {
        if (y < h) {
            for (u32 x = 0; x < w; x++) {
                fb[y * pitch + x] = 0xFFFFFFFF;
            }
        }
    }

    serial_puts("VBE: Test pattern done\n");
}

u16 vbe_read(u16 index) {
    return vbe_read_safe(index);
}
