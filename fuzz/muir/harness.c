#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "muir.h"
int LLVMFuzzerTestOneInput(const uint8_t *data,size_t size){muir_vm_t vm;memset(&vm,0,sizeof(vm));(void)muir_execute(&vm,data,size);return 0;}
