#include "vectord/types.h"
#include "vectord/cpu.h"
#include "vectord/serial.h"
typedef struct __attribute__((packed)){u16 off0;u16 sel;u8 ist;u8 attr;u16 off1;u32 off2;u32 zero;} idt_gate_t;
typedef struct __attribute__((packed)){u16 limit;u64 base;} idtr_t;
static idt_gate_t idt[256];
#define D(n) extern void isr##n(void);
D(0)D(1)D(2)D(3)D(4)D(5)D(6)D(7)D(8)D(9)D(10)D(11)D(12)D(13)D(14)D(15)
D(16)D(17)D(18)D(19)D(20)D(21)D(22)D(23)D(24)D(25)D(26)D(27)D(28)D(29)D(30)D(31)
static void setgate(int n,void(*fn)(void)){u64 a=(u64)(uptr)fn;idt[n]=(idt_gate_t){(u16)a,0x08,0,0x8E,(u16)(a>>16),(u32)(a>>32),0};}
void idt_init(void){
 void(*v[32])(void)={isr0,isr1,isr2,isr3,isr4,isr5,isr6,isr7,isr8,isr9,isr10,isr11,isr12,isr13,isr14,isr15,isr16,isr17,isr18,isr19,isr20,isr21,isr22,isr23,isr24,isr25,isr26,isr27,isr28,isr29,isr30,isr31};
 for(int i=0;i<32;i++)setgate(i,v[i]);
 idtr_t r={(u16)(sizeof(idt)-1),(u64)(uptr)idt};__asm__ volatile("lidt %0"::"m"(r));serial_puts("IDT=OK\r\n");
}
typedef struct {u64 r15,r14,r13,r12,r11,r10,r9,r8,rdi,rsi,rbp,rdx,rcx,rbx,rax,vector,error;} frame_t;
void isr_dispatch(frame_t*f){serial_puts("EXCEPTION vector=");serial_dec(f->vector);serial_puts(" error=");serial_hex64(f->error);if(f->vector==14){serial_puts(" cr2=");serial_hex64(read_cr2());}serial_puts("\r\nHALT\r\n");for(;;)__asm__ volatile("cli; hlt");}
