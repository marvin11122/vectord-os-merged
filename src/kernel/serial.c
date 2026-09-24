#include "vectord/io.h"
#include "vectord/serial.h"
void serial_init(void){outb(0x3F9,0);outb(0x3FB,0x80);outb(0x3F8,3);outb(0x3F9,0);outb(0x3FB,3);outb(0x3FA,0xC7);outb(0x3FC,0x0B);}
void serial_putc(char c){while(!(inb(0x3FD)&0x20)){} outb(0x3F8,(u8)c);}
void serial_puts(const char*s){while(*s)serial_putc(*s++);}
void serial_hex64(u64 v){static const char h[]="0123456789ABCDEF";serial_puts("0x");for(int i=15;i>=0;i--)serial_putc(h[(v>>(i*4))&15]);}
void serial_dec(u64 v){char b[24];int n=0;if(!v){serial_putc('0');return;}while(v){b[n++]=(char)('0'+v%10);v/=10;}while(n)serial_putc(b[--n]);}
