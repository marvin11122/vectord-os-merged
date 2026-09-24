#include "vectord/types.h"
#include "vectord/cpu.h"
#include "vectord/serial.h"
#define APIC_BASE_MSR 0x1B
#define APIC_ENABLE (1ULL<<11)
static volatile u32 *lapic;
u32 apic_id(void);
int apic_init(void){
 u32 a,b,c,d;cpuid(1,0,&a,&b,&c,&d);
 if(!(d&(1u<<9))){serial_puts("APIC=UNAVAILABLE\r\n");return 0;}
 u64 base=rdmsr(APIC_BASE_MSR);
 if(!(base&APIC_ENABLE)){serial_puts("APIC=DISABLED_BY_FIRMWARE\r\n");return 0;}
 u64 pa=base&0xFFFFF000ULL;
 /* Clean baseline identity-maps the first 1 GiB, including the usual LAPIC MMIO page. */
 lapic=(volatile u32*)(uptr)pa;
 serial_puts("APIC=OK base=");serial_hex64(pa);serial_puts(" id=");serial_dec(apic_id());serial_puts("\r\n");
 return 1;
}
u32 apic_id(void){return lapic ? (lapic[0x20/4]>>24) : 0;}
