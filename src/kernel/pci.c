#include "vectord/pci.h"
#include "vectord/io.h"
#include "vectord/serial.h"
#define ADDR 0xCF8
#define DATA 0xCFC
pci_device_t pci_devices[PCI_MAX_DEVICES];u32 pci_device_count;
u32 pci_config_read(u8 b,u8 s,u8 f,u8 o){u32 a=0x80000000u|((u32)b<<16)|((u32)s<<11)|((u32)f<<8)|(o&0xFC);outl(ADDR,a);return inl(DATA);}
static u16 rw(u8 b,u8 s,u8 f,u8 o){u32 v=pci_config_read(b,s,f,o);return(u16)(v>>((o&2)*8));}
static u8 rb(u8 b,u8 s,u8 f,u8 o){u32 v=pci_config_read(b,s,f,o);return(u8)(v>>((o&3)*8));}
static void add(u8 b,u8 s,u8 f){if(pci_device_count>=PCI_MAX_DEVICES||rw(b,s,f,0)==0xFFFF)return;pci_device_t*d=&pci_devices[pci_device_count++];d->vendor_id=rw(b,s,f,0);d->device_id=rw(b,s,f,2);d->class_code=rb(b,s,f,0x0B);d->subclass=rb(b,s,f,0x0A);d->prog_if=rb(b,s,f,9);d->revision=rb(b,s,f,8);d->header_type=rb(b,s,f,0x0E);d->bus=b;d->slot=s;d->func=f;for(int i=0;i<6;i++)d->bar[i]=pci_config_read(b,s,f,(u8)(0x10+i*4));d->interrupt_line=rb(b,s,f,0x3C);d->interrupt_pin=rb(b,s,f,0x3D);}
void pci_init(void){pci_device_count=0;for(u32 b=0;b<256;b++)for(u32 s=0;s<32;s++){if(rw((u8)b,(u8)s,0,0)==0xFFFF)continue;u8 h=rb((u8)b,(u8)s,0,0x0E);add((u8)b,(u8)s,0);if(h&0x80)for(u32 f=1;f<8;f++)add((u8)b,(u8)s,(u8)f);}serial_puts("PCI=OK devices=");serial_dec(pci_device_count);serial_puts("\r\n");for(u32 i=0;i<pci_device_count&&i<32;i++){pci_device_t*d=&pci_devices[i];serial_puts(" PCI ");serial_dec(d->bus);serial_putc(':');serial_dec(d->slot);serial_putc('.');serial_dec(d->func);serial_puts(" vid=");serial_hex64(d->vendor_id);serial_puts(" did=");serial_hex64(d->device_id);serial_puts(" class=");serial_hex64(((u64)d->class_code<<8)|d->subclass);serial_puts("\r\n");}}
