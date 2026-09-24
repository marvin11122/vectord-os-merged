#ifndef VECOS_MUIR_H
#define VECOS_MUIR_H
#include <stddef.h>
#include <stdint.h>
#define MUIR_MAGIC 0x5249554Du
#define MUIR_VERSION 0u
#define MUIR_MAX_TENSORS 32u
#define MUIR_MAX_ELEMENTS 256u
#define MUIR_MAX_INSTRUCTIONS 4096u
enum { MUIR_OP_END=0x00, MUIR_OP_INPUT=0x01, MUIR_OP_CONST=0x02, MUIR_OP_DENSE=0x10, MUIR_OP_RELU=0x11, MUIR_OP_ADD=0x12, MUIR_OP_ASSERT_CLOSE=0x20 };
typedef enum { MUIR_OK=0, MUIR_E_MAGIC, MUIR_E_VERSION, MUIR_E_TRUNCATED, MUIR_E_OPCODE, MUIR_E_TENSOR, MUIR_E_SHAPE, MUIR_E_NUMERIC, MUIR_E_ASSERT, MUIR_E_BUDGET } muir_status_t;
typedef struct { uint16_t length; float data[MUIR_MAX_ELEMENTS]; } muir_tensor_t;
typedef struct { muir_tensor_t tensors[MUIR_MAX_TENSORS]; uint32_t instructions; } muir_vm_t;
float muir_relu(float x);
int muir_dense_ref(const float*,size_t,const float*,const float*,size_t,float*);
muir_status_t muir_execute(muir_vm_t*, const uint8_t*, size_t);
const char *muir_status_string(muir_status_t s);
#endif
