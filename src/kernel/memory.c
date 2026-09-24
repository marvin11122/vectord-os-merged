#include "vectord/types.h"
#include "vectord/serial.h"
extern u8 _kernel_end;
typedef struct block{usize size;struct block*next,*prev;u8 free;} block_t;
static uptr hs,he;static block_t*first;
#define ALIGN16(x) (((x)+15UL)&~15UL)
#define HDR ALIGN16(sizeof(block_t))
void memory_init(usize bytes){hs=((uptr)&_kernel_end+4095UL)&~4095UL;he=hs+bytes;first=(block_t*)hs;first->size=bytes-HDR;first->next=first->prev=NULL;first->free=1;serial_puts("HEAP=OK start=");serial_hex64(hs);serial_puts(" size=");serial_dec(bytes);serial_puts("\r\n");}
void*kmalloc(usize n){if(!n)return NULL;n=ALIGN16(n);for(block_t*b=first;b;b=b->next){if(!b->free||b->size<n)continue;if(b->size>=n+HDR+16){block_t*x=(block_t*)((u8*)b+HDR+n);x->size=b->size-n-HDR;x->free=1;x->next=b->next;x->prev=b;if(x->next)x->next->prev=x;b->next=x;b->size=n;}b->free=0;return(u8*)b+HDR;}return NULL;}
void kfree(void*p){if(!p)return;block_t*b=(block_t*)((u8*)p-HDR);b->free=1;if(b->next&&b->next->free){b->size+=HDR+b->next->size;b->next=b->next->next;if(b->next)b->next->prev=b;}if(b->prev&&b->prev->free){b=b->prev;b->size+=HDR+b->next->size;b->next=b->next->next;if(b->next)b->next->prev=b;}}
usize heap_total(void){return he-hs;}usize heap_free(void){usize n=0;for(block_t*b=first;b;b=b->next)if(b->free)n+=b->size;return n;}
int paging_probe(void){u64 cr3;__asm__ volatile("mov %%cr3,%0":"=r"(cr3));serial_puts("PAGING=OK cr3=");serial_hex64(cr3);serial_puts("\r\n");return cr3!=0;}
