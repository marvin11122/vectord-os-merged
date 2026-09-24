#pragma once
#include "types.h"
static inline u64 rdmsr(u32 m){u32 lo,hi;__asm__ volatile("rdmsr":"=a"(lo),"=d"(hi):"c"(m));return ((u64)hi<<32)|lo;}
static inline u64 read_cr0(void){u64 v;__asm__ volatile("mov %%cr0,%0":"=r"(v));return v;}
static inline u64 read_cr2(void){u64 v;__asm__ volatile("mov %%cr2,%0":"=r"(v));return v;}
static inline u64 read_cr3(void){u64 v;__asm__ volatile("mov %%cr3,%0":"=r"(v));return v;}
static inline u64 read_cr4(void){u64 v;__asm__ volatile("mov %%cr4,%0":"=r"(v));return v;}
static inline u16 read_cs(void){u16 v;__asm__ volatile("mov %%cs,%0":"=r"(v));return v;}
static inline void cpuid(u32 leaf,u32 sub,u32*a,u32*b,u32*c,u32*d){__asm__ volatile("cpuid":"=a"(*a),"=b"(*b),"=c"(*c),"=d"(*d):"a"(leaf),"c"(sub));}
