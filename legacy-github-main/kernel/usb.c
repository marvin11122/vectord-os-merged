/* ═══════════════════════════════════════════════════════════════════════
 * VECTORD OS - USB Controller Driver
 * Detects UHCI, OHCI, EHCI, and xHCI controllers
 * ═══════════════════════════════════════════════════════════════════════ */

#include "../include/vecord.h"

/* ═══════════════════════════════════════════════════════════════════════
 * USB PROG_IF VALUES (subclass = 0x03 for serial bus)
 * ═══════════════════════════════════════════════════════════════════════ */

#define USB_PROG_IF_UHCI     0x00
#define USB_PROG_IF_OHCI     0x10
#define USB_PROG_IF_EHCI     0x20
#define USB_PROG_IF_XHCI     0x30

/* ═══════════════════════════════════════════════════════════════════════
 * GLOBAL STATE
 * ═══════════════════════════════════════════════════════════════════════ */

usb_controller_t usb_controllers[USB_MAX_CONTROLLERS];
int usb_controller_count = 0;

/* ═══════════════════════════════════════════════════════════════════════
 * DETECT USB CONTROLLER FROM PCI
 * ═══════════════════════════════════════════════════════════════════════ */

void usb_detect_controller(u16 vendor_id, u16 device_id, u8 prog_if, u32 bar0, u8 irq) {
    (void)vendor_id; (void)device_id; /* Not used in current implementation */
    if (usb_controller_count >= USB_MAX_CONTROLLERS) return;
    
    usb_controller_t *ctrl = &usb_controllers[usb_controller_count];
    
    /* Determine USB type from prog_if */
    switch (prog_if & 0xF0) {
        case USB_PROG_IF_UHCI:
            ctrl->type = USB_UHCI;
            ctrl->type_name[0] = 'U'; ctrl->type_name[1] = 'H'; 
            ctrl->type_name[2] = 'C'; ctrl->type_name[3] = 'I'; ctrl->type_name[4] = '\0';
            break;
        case USB_PROG_IF_OHCI:
            ctrl->type = USB_OHCI;
            ctrl->type_name[0] = 'O'; ctrl->type_name[1] = 'H'; 
            ctrl->type_name[2] = 'C'; ctrl->type_name[3] = 'I'; ctrl->type_name[4] = '\0';
            break;
        case USB_PROG_IF_EHCI:
            ctrl->type = USB_EHCI;
            ctrl->type_name[0] = 'E'; ctrl->type_name[1] = 'H'; 
            ctrl->type_name[2] = 'C'; ctrl->type_name[3] = 'I'; ctrl->type_name[4] = '\0';
            break;
        case USB_PROG_IF_XHCI:
            ctrl->type = USB_XHCI;
            ctrl->type_name[0] = 'x'; ctrl->type_name[1] = 'H'; 
            ctrl->type_name[2] = 'C'; ctrl->type_name[3] = 'I'; ctrl->type_name[4] = '\0';
            break;
        default:
            return; /* Unknown USB type */
    }
    
    ctrl->base = bar0 & ~0xF; /* Clear lower bits */
    ctrl->irq = irq;
    ctrl->present = 1;
    
    serial_puts("USB: ");
    serial_puts(ctrl->type_name);
    serial_puts(" controller @ 0x");
    serial_puthex(ctrl->base);
    serial_puts(", IRQ ");
    serial_putdec(ctrl->irq);
    serial_puts("\n");
    
    usb_controller_count++;
}

/* ═══════════════════════════════════════════════════════════════════════
 * USB INITIALIZATION
 * ═══════════════════════════════════════════════════════════════════════ */

void usb_init(void) {
    usb_controller_count = 0;
    serial_puts("USB: Controller detection ready\n");
}

