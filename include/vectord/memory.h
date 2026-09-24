#pragma once
#include "types.h"
void memory_init(usize bytes); void *kmalloc(usize n); void kfree(void*p); usize heap_total(void); usize heap_free(void); int paging_probe(void);
