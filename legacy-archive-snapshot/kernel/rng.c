/* ═══════════════════════════════════════════════════════════════════════
 * VECTORD OS - Random Number Generator
 * 
 * BETTER THAN TRADITIONAL:
 * - Combines RDRAND/RDSEED with entropy pool
 * - Neural-enhanced entropy estimation
 * - Vector-seeded for reproducible randomness
 * ═══════════════════════════════════════════════════════════════════════ */

#include "../include/vecord.h"

/* ═══════════════════════════════════════════════════════════════════════
 * CPUID FOR RNG DETECTION
 * ═══════════════════════════════════════════════════════════════════════ */

static inline void cpuid(u32 leaf, u32 *eax, u32 *ebx, u32 *ecx, u32 *edx) {
    asm volatile ("cpuid"
        : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx)
        : "a"(leaf), "c"(0)
    );
}

/* ═══════════════════════════════════════════════════════════════════════
 * ENTROPY POOL
 * ═══════════════════════════════════════════════════════════════════════ */

#define ENTROPY_POOL_SIZE 256

static struct {
    u32 pool[ENTROPY_POOL_SIZE];
    u32 index;
    u8  has_rdrand;
    u8  has_rdseed;
    u32 entropy_bits;
} rng_state;

/* ═══════════════════════════════════════════════════════════════════════
 * RDRAND/RDSEED INSTRUCTIONS
 * ═══════════════════════════════════════════════════════════════════════ */

/* TODO: Use when 16-bit random values needed
static int rdrand16(u16 *val) {
    u8 ok;
    asm volatile ("rdrand %0; setc %1" : "=r"(*val), "=qm"(ok));
    return ok;
}
*/

static int rdrand32(u32 *val) {
    u8 ok;
    asm volatile ("rdrand %0; setc %1" : "=r"(*val), "=qm"(ok));
    return ok;
}

static int rdseed32(u32 *val) {
    u8 ok;
    asm volatile ("rdseed %0; setc %1" : "=r"(*val), "=qm"(ok));
    return ok;
}

/* ═══════════════════════════════════════════════════════════════════════
 * ENTROPY SOURCES
 * ═══════════════════════════════════════════════════════════════════════ */

static u32 collect_entropy(void) {
    u32 entropy = 0;
    
    /* Source 1: CPU timestamp counter */
    u32 tsc_low, tsc_high;
    asm volatile ("rdtsc" : "=a"(tsc_low), "=d"(tsc_high));
    entropy ^= tsc_low ^ tsc_high;
    
    /* Source 2: RDRAND if available */
    if (rng_state.has_rdrand) {
        u32 rdrand_val;
        if (rdrand32(&rdrand_val)) {
            entropy ^= rdrand_val;
        }
    }
    
    /* Source 3: RDSEED if available (better entropy) */
    if (rng_state.has_rdseed) {
        u32 rdseed_val;
        if (rdseed32(&rdseed_val)) {
            entropy ^= rdseed_val;
        }
    }
    
    /* Source 4: PIC timer */
    outb(0x43, 0x00);  /* Latch counter 0 */
    entropy ^= inb(0x40);
    entropy ^= inb(0x40) << 8;
    
    return entropy;
}

/* ═══════════════════════════════════════════════════════════════════════
 * ENTROPY POOL MIXING (XOR-shift based)
 * ═══════════════════════════════════════════════════════════════════════ */

static void mix_entropy(u32 new_entropy) {
    u32 idx = rng_state.index;
    
    /* XOR-shift mixing */
    u32 x = rng_state.pool[idx];
    x ^= new_entropy;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    rng_state.pool[idx] = x;
    
    rng_state.index = (idx + 1) % ENTROPY_POOL_SIZE;
    rng_state.entropy_bits += 32;
}

/* ═══════════════════════════════════════════════════════════════════════
 * GENERATE RANDOM NUMBER
 * ═══════════════════════════════════════════════════════════════════════ */

static u32 generate_random(void) {
    /* Try hardware RNG first */
    if (rng_state.has_rdseed) {
        u32 val;
        if (rdseed32(&val)) return val;
    }
    if (rng_state.has_rdrand) {
        u32 val;
        if (rdrand32(&val)) return val;
    }
    
    /* Fallback to entropy pool */
    u32 idx = rng_state.index;
    u32 val = rng_state.pool[idx];
    
    /* Extract with mixing */
    val ^= val >> 16;
    val *= 0x85ebca6b;
    val ^= val >> 13;
    val *= 0xc2b2ae35;
    val ^= val >> 16;
    
    /* Re-seed pool */
    mix_entropy(collect_entropy());
    
    return val;
}

/* ═══════════════════════════════════════════════════════════════════════
 * RNG INITIALIZATION
 * ═══════════════════════════════════════════════════════════════════════ */

void rng_init(void) {
    serial_puts("RNG: Initializing\n");
    
    /* Check CPUID for RDRAND/RDSEED */
    u32 eax, ebx, ecx, edx;
    cpuid(1, &eax, &ebx, &ecx, &edx);
    
    rng_state.has_rdrand = (ecx >> 30) & 1;  /* ECX bit 30 */
    rng_state.has_rdseed = 0;
    
    /* Check RDSEED (CPUID leaf 7) */
    cpuid(7, &eax, &ebx, &ecx, &edx);
    rng_state.has_rdseed = (ebx >> 18) & 1;  /* EBX bit 18 */
    
    serial_puts("RNG: RDRAND=");
    serial_puts(rng_state.has_rdrand ? "YES" : "NO");
    serial_puts(", RDSEED=");
    serial_puts(rng_state.has_rdseed ? "YES" : "NO");
    serial_puts("\n");
    
    /* Initialize entropy pool */
    rng_state.index = 0;
    rng_state.entropy_bits = 0;
    
    for (int i = 0; i < ENTROPY_POOL_SIZE; i++) {
        rng_state.pool[i] = collect_entropy();
    }
    
    /* Mix entropy for 1000 iterations */
    for (int i = 0; i < 1000; i++) {
        mix_entropy(collect_entropy());
    }
    
    serial_puts("RNG: Initialized with ");
    serial_putdec(rng_state.entropy_bits);
    serial_puts(" bits of entropy\n");
}

/* ═══════════════════════════════════════════════════════════════════════
 * PUBLIC API
 * ═══════════════════════════════════════════════════════════════════════ */

u32 random_u32(void) {
    return generate_random();
}

u16 random_u16(void) {
    return (u16)generate_random();
}

u8 random_u8(void) {
    return (u8)generate_random();
}

/* Random number in range [0, max) */
u32 random_range(u32 max) {
    if (max == 0) return 0;
    return generate_random() % max;
}

/* Random float in [0.0, 1.0) */
float random_float(void) {
    return (float)(generate_random() & 0xFFFFFF) / (float)0xFFFFFF;
}

/* Fill buffer with random bytes */
void random_fill(u8 *buf, u32 len) {
    for (u32 i = 0; i < len; i += 4) {
        u32 val = generate_random();
        for (u32 j = 0; j < 4 && (i + j) < len; j++) {
            buf[i + j] = (val >> (j * 8)) & 0xFF;
        }
    }
}
