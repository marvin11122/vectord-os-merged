/* ═══════════════════════════════════════════════════════════════════════
 * VECTORD OS - Neural Network Hardware Optimizer
 * Uses a simple neural network to predict and optimize hardware usage
 * ═══════════════════════════════════════════════════════════════════════ */

#include "../include/vecord.h"

/* ═══════════════════════════════════════════════════════════════════════
 * NEURAL NETWORK CONFIGURATION
 * ═══════════════════════════════════════════════════════════════════════ */

#define NN_INPUT_NODES    10    /* Input features */
#define NN_HIDDEN_NODES   12    /* Hidden layer neurons */
#define NN_OUTPUT_NODES   4     /* Output predictions */
#define NN_LEARNING_RATE  0.01f

/* Hardware types for prediction */
#define HW_CPU_LOAD       0
#define HW_MEMORY_USAGE   1
#define HW_DISK_ACTIVITY  2
#define HW_NETWORK_IO     3

/* ═══════════════════════════════════════════════════════════════════════
 * NEURAL NETWORK STRUCTURE
 * ═══════════════════════════════════════════════════════════════════════ */

typedef struct {
    /* Weights */
    float w_ih[NN_INPUT_NODES][NN_HIDDEN_NODES];   /* Input to hidden */
    float w_ho[NN_HIDDEN_NODES][NN_OUTPUT_NODES];  /* Hidden to output */
    
    /* Biases */
    float b_h[NN_HIDDEN_NODES];
    float b_o[NN_OUTPUT_NODES];
    
    /* Activations */
    float input[NN_INPUT_NODES];
    float hidden[NN_HIDDEN_NODES];
    float output[NN_OUTPUT_NODES];
    
    /* Training data history */
    float history[256][NN_INPUT_NODES];
    int history_count;
    int history_index;
} neural_hw_t;

/* ═══════════════════════════════════════════════════════════════════════
 * GLOBAL STATE
 * ═══════════════════════════════════════════════════════════════════════ */

static neural_hw_t nn;

/* ═══════════════════════════════════════════════════════════════════════
 * ACTIVATION FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════ */

/* Simple exponential approximation for freestanding environment */
static float simple_exp(float x) {
    /* Clamp extreme values */
    if (x < -10.0f) return 0.000045f;  /* e^-10 */
    if (x > 10.0f) return 22026.0f;    /* e^10 */
    
    /* Taylor series approximation for e^x around 0 */
    /* e^x = 1 + x + x^2/2! + x^3/3! + x^4/4! + ... */
    float result = 1.0f;
    float term = 1.0f;
    
    for (int i = 1; i <= 10; i++) {
        term *= x / (float)i;
        result += term;
    }
    
    return result;
}

static float sigmoid(float x) {
    if (x < -10.0f) return 0.0f;
    if (x > 10.0f) return 1.0f;
    return 1.0f / (1.0f + simple_exp(-x));
}

static float sigmoid_derivative(float x) {
    float s = sigmoid(x);
    return s * (1.0f - s);
}

/* ═══════════════════════════════════════════════════════════════════════
 * INITIALIZATION
 * ═══════════════════════════════════════════════════════════════════════ */

static void nn_random_weights(void) {
    /* Simple pseudo-random initialization */
    u32 seed = 12345;
    
    for (int i = 0; i < NN_INPUT_NODES; i++) {
        for (int j = 0; j < NN_HIDDEN_NODES; j++) {
            seed = seed * 1103515245 + 12345;
            nn.w_ih[i][j] = ((float)(seed % 1000) / 500.0f) - 1.0f;
        }
    }
    
    for (int i = 0; i < NN_HIDDEN_NODES; i++) {
        for (int j = 0; j < NN_OUTPUT_NODES; j++) {
            seed = seed * 1103515245 + 12345;
            nn.w_ho[i][j] = ((float)(seed % 1000) / 500.0f) - 1.0f;
        }
        seed = seed * 1103515245 + 12345;
        nn.b_h[i] = ((float)(seed % 1000) / 500.0f) - 1.0f;
    }
    
    for (int i = 0; i < NN_OUTPUT_NODES; i++) {
        seed = seed * 1103515245 + 12345;
        nn.b_o[i] = ((float)(seed % 1000) / 500.0f) - 1.0f;
    }
    
    nn.history_count = 0;
    nn.history_index = 0;
}

/* ═══════════════════════════════════════════════════════════════════════
 * FORWARD PASS
 * ═══════════════════════════════════════════════════════════════════════ */

static void nn_forward(float *input, float *output) {
    float hidden[NN_HIDDEN_NODES];
    
    /* Input to hidden layer */
    for (int j = 0; j < NN_HIDDEN_NODES; j++) {
        float sum = nn.b_h[j];
        for (int i = 0; i < NN_INPUT_NODES; i++) {
            sum += input[i] * nn.w_ih[i][j];
        }
        hidden[j] = sigmoid(sum);
    }
    
    /* Hidden to output layer */
    for (int j = 0; j < NN_OUTPUT_NODES; j++) {
        float sum = nn.b_o[j];
        for (int i = 0; i < NN_HIDDEN_NODES; i++) {
            sum += hidden[i] * nn.w_ho[i][j];
        }
        output[j] = sigmoid(sum);
    }
}

/* ═══════════════════════════════════════════════════════════════════════
 * TRAINING (Backpropagation)
 * ═══════════════════════════════════════════════════════════════════════ */

static void nn_train(float *input, float *target) {
    float hidden[NN_HIDDEN_NODES];
    float output[NN_OUTPUT_NODES];
    float hidden_delta[NN_HIDDEN_NODES];
    float output_delta[NN_OUTPUT_NODES];
    
    /* Forward pass */
    for (int j = 0; j < NN_HIDDEN_NODES; j++) {
        float sum = nn.b_h[j];
        for (int i = 0; i < NN_INPUT_NODES; i++) {
            sum += input[i] * nn.w_ih[i][j];
        }
        hidden[j] = sigmoid(sum);
    }
    
    for (int j = 0; j < NN_OUTPUT_NODES; j++) {
        float sum = nn.b_o[j];
        for (int i = 0; i < NN_HIDDEN_NODES; i++) {
            sum += hidden[i] * nn.w_ho[i][j];
        }
        output[j] = sigmoid(sum);
    }
    
    /* Calculate output deltas */
    for (int j = 0; j < NN_OUTPUT_NODES; j++) {
        float error = target[j] - output[j];
        output_delta[j] = error * sigmoid_derivative(output[j]);
    }
    
    /* Calculate hidden deltas */
    for (int j = 0; j < NN_HIDDEN_NODES; j++) {
        float error = 0.0f;
        for (int k = 0; k < NN_OUTPUT_NODES; k++) {
            error += output_delta[k] * nn.w_ho[j][k];
        }
        hidden_delta[j] = error * sigmoid_derivative(hidden[j]);
    }
    
    /* Update hidden-to-output weights */
    for (int i = 0; i < NN_HIDDEN_NODES; i++) {
        for (int j = 0; j < NN_OUTPUT_NODES; j++) {
            nn.w_ho[i][j] += NN_LEARNING_RATE * output_delta[j] * hidden[i];
        }
    }
    
    /* Update output biases */
    for (int j = 0; j < NN_OUTPUT_NODES; j++) {
        nn.b_o[j] += NN_LEARNING_RATE * output_delta[j];
    }
    
    /* Update input-to-hidden weights */
    for (int i = 0; i < NN_INPUT_NODES; i++) {
        for (int j = 0; j < NN_HIDDEN_NODES; j++) {
            nn.w_ih[i][j] += NN_LEARNING_RATE * hidden_delta[j] * input[i];
        }
    }
    
    /* Update hidden biases */
    for (int j = 0; j < NN_HIDDEN_NODES; j++) {
        nn.b_h[j] += NN_LEARNING_RATE * hidden_delta[j];
    }
}

/* ═══════════════════════════════════════════════════════════════════════
 * PUBLIC API
 * ═══════════════════════════════════════════════════════════════════════ */

void neural_hw_init(void) {
    serial_puts("NN: Initializing hardware optimizer\n");
    nn_random_weights();
    serial_puts("NN: ");
    serial_putdec(NN_INPUT_NODES);
    serial_puts(" inputs -> ");
    serial_putdec(NN_HIDDEN_NODES);
    serial_puts(" hidden -> ");
    serial_putdec(NN_OUTPUT_NODES);
    serial_puts(" outputs\n");
    serial_puts("NN: ");
    serial_putdec(sizeof(neural_hw_t));
    serial_puts(" bytes allocated\n");
}

float neural_hw_predict(u32 hw_type) {
    if (hw_type >= NN_OUTPUT_NODES) return 0.0f;
    
    /* Create input from recent history */
    float input[NN_INPUT_NODES] = {0};
    
    if (nn.history_count > 0) {
        /* Use most recent data point */
        int idx = (nn.history_index - 1 + 256) % 256;
        for (int i = 0; i < NN_INPUT_NODES && i < NN_INPUT_NODES; i++) {
            input[i] = nn.history[idx][i];
        }
    }
    
    float output[NN_OUTPUT_NODES];
    nn_forward(input, output);
    
    return output[hw_type];
}

void neural_hw_learn(u32 hw_type, float performance) {
    /* Store input pattern */
    float input[NN_INPUT_NODES];
    
    /* Fill with current system state (simplified) */
    input[0] = (float)mem_get_used() / (float)mem_get_total();
    input[1] = (float)ata_device_count / 4.0f;
    input[2] = (float)pci_device_count / (float)PCI_MAX_DEVICES;
    input[3] = (float)usb_controller_count / (float)USB_MAX_CONTROLLERS;
    input[4] = performance;
    input[5] = (float)hw_type / (float)NN_OUTPUT_NODES;
    input[6] = 0.5f; /* Placeholder */
    input[7] = 0.5f; /* Placeholder */
    input[8] = (float)vec_get_counter() / 1000.0f;  /* Vector allocations normalized */
    input[9] = (float)mem_get_free_blocks() / 64.0f; /* Free blocks normalized */
    
    /* Store in history */
    for (int i = 0; i < NN_INPUT_NODES; i++) {
        nn.history[nn.history_index][i] = input[i];
    }
    nn.history_index = (nn.history_index + 1) % 256;
    if (nn.history_count < 256) nn.history_count++;
    
    /* Create target output */
    float target[NN_OUTPUT_NODES] = {0};
    if (hw_type < NN_OUTPUT_NODES) {
        target[hw_type] = performance;
    }
    
    /* Train the network */
    nn_train(input, target);
}

void neural_hw_optimize(void) {
    serial_puts("NN: Running hardware optimization\n");
    
    /* Predict optimal settings for each hardware type */
    for (int i = 0; i < NN_OUTPUT_NODES; i++) {
        float prediction = neural_hw_predict(i);
        
        serial_puts("NN: HW[");
        serial_putdec(i);
        serial_puts("] prediction: ");
        serial_putdec((u32)(prediction * 100));
        serial_puts("%\n");
    }
}

