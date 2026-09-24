#include "vectord/types.h"
#include "vectord/serial.h"
#include "vectord/cpu.h"
#include "vectord/idt.h"
#include "vectord/apic.h"
#include "vectord/memory.h"
#include "vectord/pci.h"
static void line(const char*n,u64 v){serial_puts(n);serial_putc('=');serial_hex64(v);serial_puts("\r\n");}
void kernel_main(u32 magic,u32 mbi){
 serial_init();serial_puts("\r\n=== VECTORD OS stage2 x86_64 ===\r\n");
 line("multiboot_magic",magic);line("multiboot_info",mbi);line("cs",read_cs());line("cpl",read_cs()&3);line("cr0",read_cr0());line("cr3",read_cr3());line("cr4",read_cr4());line("efer",rdmsr(0xC0000080));
 idt_init();
 paging_probe();
 memory_init(16UL*1024*1024);
 void*a=kmalloc(64),*b=kmalloc(4096);serial_puts((a&&b)?"HEAP_SELFTEST=OK\r\n":"HEAP_SELFTEST=FAIL\r\n");kfree(a);kfree(b);
 apic_init();
 pci_init();
 serial_puts("STATUS=BOOT_OK_STAGE2\r\n");
 for(;;)__asm__ volatile("hlt");
}
