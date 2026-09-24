#include "muir.h"

static void *muir_memcpy(void *dst, const void *src, size_t n) {
    uint8_t *d = dst;
    const uint8_t *s = src;
    while (n--) *d++ = *s++;
    return dst;
}

static int take(const uint8_t **p, size_t *n, void *out, size_t size) {
    if (*n < size) return 0;
    muir_memcpy(out, *p, size);
    *p += size;
    *n -= size;
    return 1;
}

static int tensor_id_valid(uint8_t id) {
    return id < MUIR_MAX_TENSORS;
}

static int finite_f32(float x) {
    return x == x && x <= 3.402823466e38F && x >= -3.402823466e38F;
}

const char *muir_status_string(muir_status_t status) {
    static const char *const names[] = {
        "OK", "MAGIC", "VERSION", "TRUNCATED", "OPCODE",
        "TENSOR", "SHAPE", "NUMERIC", "ASSERT", "BUDGET"
    };
    return (unsigned)status < (sizeof(names) / sizeof(names[0]))
        ? names[status] : "?";
}

muir_status_t muir_execute(muir_vm_t *vm, const uint8_t *program, size_t size) {
    uint32_t magic;
    uint16_t version, flags;
    uint32_t payload_size;

    if (!vm || !program ||
        !take(&program, &size, &magic, sizeof(magic)) ||
        !take(&program, &size, &version, sizeof(version)) ||
        !take(&program, &size, &flags, sizeof(flags)) ||
        !take(&program, &size, &payload_size, sizeof(payload_size))) {
        return MUIR_E_TRUNCATED;
    }
    if (magic != MUIR_MAGIC) return MUIR_E_MAGIC;
    if (version != MUIR_VERSION || flags != 0) return MUIR_E_VERSION;
    if (payload_size != size) return MUIR_E_TRUNCATED;

    vm->instructions = 0;
    for (unsigned i = 0; i < MUIR_MAX_TENSORS; i++) {
        vm->tensors[i].length = 0;
    }

    while (size) {
        uint8_t opcode = *program++;
        size--;
        if (++vm->instructions > MUIR_MAX_INSTRUCTIONS) {
            return MUIR_E_BUDGET;
        }

        if (opcode == MUIR_OP_END) {
            return size == 0 ? MUIR_OK : MUIR_E_OPCODE;
        }

        if (opcode == MUIR_OP_INPUT || opcode == MUIR_OP_CONST) {
            uint8_t dst;
            uint16_t length;
            if (!take(&program, &size, &dst, sizeof(dst)) ||
                !take(&program, &size, &length, sizeof(length))) {
                return MUIR_E_TRUNCATED;
            }
            if (!tensor_id_valid(dst)) return MUIR_E_TENSOR;
            if (!length || length > MUIR_MAX_ELEMENTS) return MUIR_E_SHAPE;
            size_t bytes = (size_t)length * sizeof(float);
            if (size < bytes) return MUIR_E_TRUNCATED;
            vm->tensors[dst].length = length;
            muir_memcpy(vm->tensors[dst].data, program, bytes);
            for (unsigned i = 0; i < length; i++) {
                if (!finite_f32(vm->tensors[dst].data[i])) return MUIR_E_NUMERIC;
            }
            program += bytes;
            size -= bytes;
            continue;
        }

        if (opcode == MUIR_OP_DENSE) {
            uint8_t dst, input, weights, bias;
            uint16_t input_count, output_count;
            if (!take(&program, &size, &dst, 1) ||
                !take(&program, &size, &input, 1) ||
                !take(&program, &size, &weights, 1) ||
                !take(&program, &size, &bias, 1) ||
                !take(&program, &size, &input_count, 2) ||
                !take(&program, &size, &output_count, 2)) {
                return MUIR_E_TRUNCATED;
            }
            if (!tensor_id_valid(dst) || !tensor_id_valid(input) ||
                !tensor_id_valid(weights) || !tensor_id_valid(bias)) {
                return MUIR_E_TENSOR;
            }
            if (!input_count || !output_count ||
                output_count > MUIR_MAX_ELEMENTS ||
                (size_t)input_count * output_count > MUIR_MAX_ELEMENTS) {
                return MUIR_E_SHAPE;
            }
            if (vm->tensors[input].length != input_count ||
                vm->tensors[weights].length != (size_t)input_count * output_count ||
                vm->tensors[bias].length != output_count) {
                return MUIR_E_SHAPE;
            }
            vm->tensors[dst].length = output_count;
            if (muir_dense_ref(vm->tensors[input].data, input_count,
                               vm->tensors[weights].data, vm->tensors[bias].data,
                               output_count, vm->tensors[dst].data) != 0) {
                return MUIR_E_NUMERIC;
            }
            continue;
        }

        if (opcode == MUIR_OP_RELU) {
            uint8_t dst, input;
            if (!take(&program, &size, &dst, 1) ||
                !take(&program, &size, &input, 1)) {
                return MUIR_E_TRUNCATED;
            }
            if (!tensor_id_valid(dst) || !tensor_id_valid(input)) {
                return MUIR_E_TENSOR;
            }
            vm->tensors[dst].length = vm->tensors[input].length;
            for (unsigned i = 0; i < vm->tensors[input].length; i++) {
                vm->tensors[dst].data[i] = muir_relu(vm->tensors[input].data[i]);
            }
            continue;
        }

        if (opcode == MUIR_OP_ADD) {
            uint8_t dst, lhs, rhs;
            if (!take(&program, &size, &dst, 1) ||
                !take(&program, &size, &lhs, 1) ||
                !take(&program, &size, &rhs, 1)) {
                return MUIR_E_TRUNCATED;
            }
            if (!tensor_id_valid(dst) || !tensor_id_valid(lhs) ||
                !tensor_id_valid(rhs)) {
                return MUIR_E_TENSOR;
            }
            if (vm->tensors[lhs].length != vm->tensors[rhs].length) {
                return MUIR_E_SHAPE;
            }
            vm->tensors[dst].length = vm->tensors[lhs].length;
            for (unsigned i = 0; i < vm->tensors[dst].length; i++) {
                float value = vm->tensors[lhs].data[i] + vm->tensors[rhs].data[i];
                if (!finite_f32(value)) return MUIR_E_NUMERIC;
                vm->tensors[dst].data[i] = value;
            }
            continue;
        }

        if (opcode == MUIR_OP_ASSERT_CLOSE) {
            uint8_t input;
            uint16_t length;
            float epsilon;
            if (!take(&program, &size, &input, 1) ||
                !take(&program, &size, &length, 2) ||
                !take(&program, &size, &epsilon, sizeof(epsilon))) {
                return MUIR_E_TRUNCATED;
            }
            if (!tensor_id_valid(input)) return MUIR_E_TENSOR;
            if (!finite_f32(epsilon) || epsilon < 0.0f) return MUIR_E_NUMERIC;
            if (vm->tensors[input].length != length ||
                length > MUIR_MAX_ELEMENTS) {
                return MUIR_E_SHAPE;
            }
            for (unsigned i = 0; i < length; i++) {
                float expected;
                if (!take(&program, &size, &expected, sizeof(expected))) {
                    return MUIR_E_TRUNCATED;
                }
                if (!finite_f32(expected) ||
                    !finite_f32(vm->tensors[input].data[i])) {
                    return MUIR_E_NUMERIC;
                }
                float delta = vm->tensors[input].data[i] - expected;
                if (!finite_f32(delta) || delta > epsilon || delta < -epsilon) {
                    return MUIR_E_ASSERT;
                }
            }
            continue;
        }

        return MUIR_E_OPCODE;
    }

    return MUIR_E_TRUNCATED;
}
