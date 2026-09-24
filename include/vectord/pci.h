#pragma once
#include "types.h"
#define PCI_MAX_DEVICES 256
typedef struct{u16 vendor_id,device_id;u8 class_code,subclass,prog_if,revision,header_type,bus,slot,func,interrupt_line,interrupt_pin;u32 bar[6];} pci_device_t;
extern pci_device_t pci_devices[PCI_MAX_DEVICES]; extern u32 pci_device_count;
void pci_init(void); u32 pci_config_read(u8 bus,u8 slot,u8 func,u8 off);
