#include "muir.h"

static int finite_f32(float x) {
    return x == x && x <= 3.402823466e38F && x >= -3.402823466e38F;
}

float muir_relu(float x) {
    return x > 0.0f ? x : 0.0f;
}

int muir_dense_ref(const float *input, size_t input_count,
                   const float *weights, const float *bias,
                   size_t output_count, float *output) {
    if (!input || !weights || !bias || !output ||
        !input_count || !output_count) {
        return -1;
    }

    for (size_t i = 0; i < input_count; i++) {
        if (!finite_f32(input[i])) return -1;
    }
    for (size_t j = 0; j < output_count; j++) {
        if (!finite_f32(bias[j])) return -1;
        float acc = bias[j];
        for (size_t i = 0; i < input_count; i++) {
            float weight = weights[i * output_count + j];
            if (!finite_f32(weight)) return -1;
            acc += input[i] * weight;
            if (!finite_f32(acc)) return -1;
        }
        output[j] = acc;
    }
    return 0;
}
