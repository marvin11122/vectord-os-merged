#pragma once
#include "types.h"
void serial_init(void); void serial_putc(char c); void serial_puts(const char*s); void serial_hex64(u64 v); void serial_dec(u64 v);
