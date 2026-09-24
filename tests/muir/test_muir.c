#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "muir.h"

typedef struct {
    uint8_t bytes[16384];
    size_t length;
} program_t;

static int failures;

static void put_u8(program_t *p, uint8_t value) {
    p->bytes[p->length++] = value;
}

static void put_u16(program_t *p, uint16_t value) {
    put_u8(p, (uint8_t)value);
    put_u8(p, (uint8_t)(value >> 8));
}

static void put_u32(program_t *p, uint32_t value) {
    put_u16(p, (uint16_t)value);
    put_u16(p, (uint16_t)(value >> 16));
}

static void put_f32(program_t *p, float value) {
    uint8_t bytes[sizeof(value)];
    memcpy(bytes, &value, sizeof(value));
    for (size_t i = 0; i < sizeof(value); i++) put_u8(p, bytes[i]);
}

static void begin(program_t *p) {
    p->length = 0;
    put_u32(p, MUIR_MAGIC);
    put_u16(p, MUIR_VERSION);
    put_u16(p, 0);
    put_u32(p, 0);
}

static void finish(program_t *p) {
    uint32_t payload = (uint32_t)(p->length - 12);
    p->bytes[8] = (uint8_t)payload;
    p->bytes[9] = (uint8_t)(payload >> 8);
    p->bytes[10] = (uint8_t)(payload >> 16);
    p->bytes[11] = (uint8_t)(payload >> 24);
}

static void expect_status(const char *name, const program_t *p,
                          muir_status_t expected) {
    muir_vm_t vm = {0};
    muir_status_t actual = muir_execute(&vm, p->bytes, p->length);
    if (actual != expected) {
        fprintf(stderr, "%s: expected %s, got %s\n", name,
                muir_status_string(expected), muir_status_string(actual));
        failures++;
    }
}

static void write_tensor(program_t *p, uint8_t opcode, uint8_t id,
                         const float *values, uint16_t count) {
    put_u8(p, opcode);
    put_u8(p, id);
    put_u16(p, count);
    for (uint16_t i = 0; i < count; i++) put_f32(p, values[i]);
}

static void test_dense_relu_program(void) {
    program_t p;
    begin(&p);
    const float input[] = {1.0f, 0.5f, -0.2f, 0.8f};
    const float weights[] = {0.0f, 0.1f, 0.2f, 0.3f,
                             0.4f, 0.0f, 0.1f, 0.2f};
    const float bias[] = {0.01f, 0.01f};

    write_tensor(&p, MUIR_OP_INPUT, 0, input, 4);
    write_tensor(&p, MUIR_OP_CONST, 1, weights, 8);
    write_tensor(&p, MUIR_OP_CONST, 2, bias, 2);
    put_u8(&p, MUIR_OP_DENSE);
    put_u8(&p, 3);
    put_u8(&p, 0);
    put_u8(&p, 1);
    put_u8(&p, 2);
    put_u16(&p, 4);
    put_u16(&p, 2);
    put_u8(&p, MUIR_OP_RELU);
    put_u8(&p, 4);
    put_u8(&p, 3);
    put_u8(&p, MUIR_OP_ASSERT_CLOSE);
    put_u8(&p, 4);
    put_u16(&p, 2);
    put_f32(&p, 0.00001f);
    put_f32(&p, 0.11f);
    put_f32(&p, 0.42f);
    put_u8(&p, MUIR_OP_ADD);
    put_u8(&p, 5);
    put_u8(&p, 4);
    put_u8(&p, 4);
    put_u8(&p, MUIR_OP_ASSERT_CLOSE);
    put_u8(&p, 5);
    put_u16(&p, 2);
    put_f32(&p, 0.00001f);
    put_f32(&p, 0.22f);
    put_f32(&p, 0.84f);
    put_u8(&p, MUIR_OP_END);
    finish(&p);

    muir_vm_t vm = {0};
    muir_status_t status = muir_execute(&vm, p.bytes, p.length);
    if (status != MUIR_OK || vm.tensors[4].length != 2 ||
        fabsf(vm.tensors[4].data[0] - 0.11f) > 0.00001f ||
        fabsf(vm.tensors[4].data[1] - 0.42f) > 0.00001f) {
        fprintf(stderr, "dense/relu program produced an unexpected result\n");
        failures++;
    }
}

static void test_invalid_streams(void) {
    program_t p;
    begin(&p);
    put_u8(&p, MUIR_OP_END);
    finish(&p);
    p.length = 3;
    expect_status("short header", &p, MUIR_E_TRUNCATED);

    begin(&p);
    p.bytes[0] ^= 0x01;
    put_u8(&p, MUIR_OP_END);
    finish(&p);
    expect_status("bad magic", &p, MUIR_E_MAGIC);

    begin(&p);
    p.bytes[4] = 1;
    put_u8(&p, MUIR_OP_END);
    finish(&p);
    expect_status("unsupported version", &p, MUIR_E_VERSION);

    begin(&p);
    put_u8(&p, 0xFE);
    finish(&p);
    expect_status("unknown opcode", &p, MUIR_E_OPCODE);

    begin(&p);
    put_u8(&p, MUIR_OP_INPUT);
    put_u8(&p, MUIR_MAX_TENSORS);
    put_u16(&p, 1);
    put_f32(&p, 1.0f);
    put_u8(&p, MUIR_OP_END);
    finish(&p);
    expect_status("invalid tensor id", &p, MUIR_E_TENSOR);

    begin(&p);
    put_u8(&p, MUIR_OP_INPUT);
    put_u8(&p, 0);
    put_u16(&p, 0);
    put_u8(&p, MUIR_OP_END);
    finish(&p);
    expect_status("empty tensor", &p, MUIR_E_SHAPE);

    begin(&p);
    put_u8(&p, MUIR_OP_END);
    put_u8(&p, 0xFE);
    finish(&p);
    expect_status("trailing opcode", &p, MUIR_E_OPCODE);

    begin(&p);
    put_u8(&p, MUIR_OP_END);
    finish(&p);
    p.bytes[p.length++] = 0xAA;
    expect_status("trailing bytes outside payload", &p, MUIR_E_TRUNCATED);

    begin(&p);
    put_u8(&p, MUIR_OP_INPUT);
    put_u8(&p, 0);
    put_u16(&p, 1);
    put_u32(&p, 0x7FC00000u);
    put_u8(&p, MUIR_OP_END);
    finish(&p);
    expect_status("NaN tensor value", &p, MUIR_E_NUMERIC);

    begin(&p);
    for (unsigned i = 0; i < MUIR_MAX_INSTRUCTIONS + 1; i++) {
        put_u8(&p, MUIR_OP_RELU);
        put_u8(&p, 0);
        put_u8(&p, 0);
    }
    finish(&p);
    expect_status("instruction budget", &p, MUIR_E_BUDGET);
}

int main(void) {
    test_dense_relu_program();
    test_invalid_streams();
    if (failures) {
        fprintf(stderr, "%d µIR test(s) failed\n", failures);
        return 1;
    }
    puts("µIR tests: PASS (dense/ReLU/add inference and 10 malformed/boundary cases)");
    return 0;
}
