/* ═══════════════════════════════════════════════════════════════════════
 * VECTORD OS - SSE2-Accelerated Vector Math
 * Fast-path implementations for vec2/vec3/vec4 operations
 * Requires: SSE2 (enabled early in kernel_main via CR0/CR4)
 * Uses GCC vector extensions (work in freestanding without headers).
 * ═══════════════════════════════════════════════════════════════════════ */

#include "../include/vecord.h"

/* ─── GCC vector extension types (map to XMM) ────────────────────────── */
typedef float v4sf __attribute__((vector_size(16)));  /* 4x float in XMM */

/* ─── helpers ────────────────────────────────────────────────────────── */

/* Horizontal sum of 4 floats in a v4sf register */
static inline float hsum4(v4sf v) {
    /* [a b c d] -> shuffle -> add pairs -> shuffle -> add scalar */
    v4sf shuf = __builtin_shufflevector(v, v, 2, 3, 0, 1); /* [c d a b] */
    v4sf sums = v + shuf;                                    /* [a+c b+d c+a d+b] */
    v4sf shuf2 = __builtin_shufflevector(sums, sums, 1, 0, 3, 2); /* [b+d a+c d+b c+a] */
    v4sf sums2 = sums + shuf2;                               /* [sum sum sum sum] */
    return sums2[0];
}

/* rsqrt refinement for v4sf (one Newton step) */
static inline v4sf rsqrt4(v4sf sq) {
    v4sf rsq = __builtin_ia32_rsqrtps(sq);
    /* Newton: rsq = rsq * (1.5 - 0.5 * sq * rsq * rsq) */
    v4sf half  = {0.5f, 0.5f, 0.5f, 0.5f};
    v4sf oneh  = {1.5f, 1.5f, 1.5f, 1.5f};
    v4sf mul   = sq * rsq * rsq;
    return rsq * (oneh - half * mul);
}

/* Scalar sqrt via SSE rsqrt refinement */
static inline float sse_sqrtf(float x) {
    if (x <= 0.0f) return 0.0f;
    v4sf vx = {x, 0.0f, 0.0f, 0.0f};
    v4sf rsq = __builtin_ia32_rsqrtps(vx);
    v4sf half  = {0.5f, 0.5f, 0.5f, 0.5f};
    v4sf oneh  = {1.5f, 1.5f, 1.5f, 1.5f};
    v4sf mul   = vx * rsq * rsq;
    v4sf r     = rsq * (oneh - half * mul);
    return r[0] * x;
}

/* ═══════════════════════════════════════════════════════════════════════
 * VEC2 BATCH OPERATIONS (SSE2)
 * ═══════════════════════════════════════════════════════════════════════ */

void v2_dot_batch(const vec2_t *a, const vec2_t *b, float *out, int n) {
    int i = 0;
    for (; i + 3 < n; i += 4) {
        v4sf ax = {a[i].x, a[i+1].x, a[i+2].x, a[i+3].x};
        v4sf ay = {a[i].y, a[i+1].y, a[i+2].y, a[i+3].y};
        v4sf bx = {b[i].x, b[i+1].x, b[i+2].x, b[i+3].x};
        v4sf by = {b[i].y, b[i+1].y, b[i+2].y, b[i+3].y};
        v4sf dot = ax * bx + ay * by;
        out[i+0] = dot[0]; out[i+1] = dot[1];
        out[i+2] = dot[2]; out[i+3] = dot[3];
    }
    for (; i < n; i++) out[i] = a[i].x * b[i].x + a[i].y * b[i].y;
}

void v2_length_batch(const vec2_t *v, float *out, int n) {
    int i = 0;
    for (; i + 3 < n; i += 4) {
        v4sf vx = {v[i].x, v[i+1].x, v[i+2].x, v[i+3].x};
        v4sf vy = {v[i].y, v[i+1].y, v[i+2].y, v[i+3].y};
        v4sf sq = vx * vx + vy * vy;
        v4sf len = sq * rsqrt4(sq);
        out[i+0] = len[0]; out[i+1] = len[1];
        out[i+2] = len[2]; out[i+3] = len[3];
    }
    for (; i < n; i++) {
        float d = v[i].x*v[i].x + v[i].y*v[i].y;
        out[i] = sse_sqrtf(d);
    }
}

/* ═══════════════════════════════════════════════════════════════════════
 * VEC3 SINGLE-ENTRY SSE2
 * ═══════════════════════════════════════════════════════════════════════ */

float v3_dot_sse(vec3_t a, vec3_t b) {
    v4sf va = {a.x, a.y, a.z, 0.0f};
    v4sf vb = {b.x, b.y, b.z, 0.0f};
    return hsum4(va * vb);
}

float v3_length_sse(vec3_t v) {
    return sse_sqrtf(v.x*v.x + v.y*v.y + v.z*v.z);
}

/* ═══════════════════════════════════════════════════════════════════════
 * VEC3 BATCH OPERATIONS (SSE2)
 * ═══════════════════════════════════════════════════════════════════════ */

void v3_dot_batch(const vec3_t *a, const vec3_t *b, float *out, int n) {
    int i = 0;
    for (; i + 3 < n; i += 4) {
        v4sf ax = {a[i].x, a[i+1].x, a[i+2].x, a[i+3].x};
        v4sf ay = {a[i].y, a[i+1].y, a[i+2].y, a[i+3].y};
        v4sf az = {a[i].z, a[i+1].z, a[i+2].z, a[i+3].z};
        v4sf bx = {b[i].x, b[i+1].x, b[i+2].x, b[i+3].x};
        v4sf by = {b[i].y, b[i+1].y, b[i+2].y, b[i+3].y};
        v4sf bz = {b[i].z, b[i+1].z, b[i+2].z, b[i+3].z};
        v4sf dot = ax * bx + ay * by + az * bz;
        out[i+0] = dot[0]; out[i+1] = dot[1];
        out[i+2] = dot[2]; out[i+3] = dot[3];
    }
    for (; i < n; i++) out[i] = a[i].x*b[i].x + a[i].y*b[i].y + a[i].z*b[i].z;
}

void v3_length_batch(const vec3_t *v, float *out, int n) {
    int i = 0;
    for (; i + 3 < n; i += 4) {
        v4sf vx = {v[i].x, v[i+1].x, v[i+2].x, v[i+3].x};
        v4sf vy = {v[i].y, v[i+1].y, v[i+2].y, v[i+3].y};
        v4sf vz = {v[i].z, v[i+1].z, v[i+2].z, v[i+3].z};
        v4sf sq = vx * vx + vy * vy + vz * vz;
        v4sf len = sq * rsqrt4(sq);
        out[i+0] = len[0]; out[i+1] = len[1];
        out[i+2] = len[2]; out[i+3] = len[3];
    }
    for (; i < n; i++) {
        float d = v[i].x*v[i].x + v[i].y*v[i].y + v[i].z*v[i].z;
        out[i] = sse_sqrtf(d);
    }
}

/* ═══════════════════════════════════════════════════════════════════════
 * VEC4 SINGLE-ENTRY SSE2 (perfect XMM fit: 4 floats)
 * ═══════════════════════════════════════════════════════════════════════ */

static inline v4sf load4sf(const vec4_t *v) {
    v4sf r;
    ((float*)&r)[0] = v->x; ((float*)&r)[1] = v->y;
    ((float*)&r)[2] = v->z; ((float*)&r)[3] = v->w;
    return r;
}

static inline vec4_t store4sf(v4sf v) {
    vec4_t r;
    r.x = ((float*)&v)[0]; r.y = ((float*)&v)[1];
    r.z = ((float*)&v)[2]; r.w = ((float*)&v)[3];
    return r;
}

vec4_t v4_add_sse(vec4_t a, vec4_t b) {
    return store4sf(load4sf(&a) + load4sf(&b));
}

vec4_t v4_sub_sse(vec4_t a, vec4_t b) {
    return store4sf(load4sf(&a) - load4sf(&b));
}

vec4_t v4_scale_sse(vec4_t v, float s) {
    v4sf sv = {s, s, s, s};
    return store4sf(load4sf(&v) * sv);
}

float v4_dot_sse(vec4_t a, vec4_t b) {
    return hsum4(load4sf(&a) * load4sf(&b));
}

vec4_t v4_lerp_sse(vec4_t a, vec4_t b, float t) {
    v4sf tv = {t, t, t, t};
    v4sf ov = {1.0f, 1.0f, 1.0f, 1.0f};
    return store4sf(load4sf(&a) * (ov - tv) + load4sf(&b) * tv);
}

/* ═══════════════════════════════════════════════════════════════════════
 * VEC4 BATCH OPERATIONS (SSE2) - 4-wide
 * ═══════════════════════════════════════════════════════════════════════ */

void v4_dot_batch(const vec4_t *a, const vec4_t *b, float *out, int n) {
    int i = 0;
    for (; i + 3 < n; i += 4) {
        /* Pack 4 component-wise: each register holds i..i+3 of one component */
        v4sf ax = {a[i].x, a[i+1].x, a[i+2].x, a[i+3].x};
        v4sf ay = {a[i].y, a[i+1].y, a[i+2].y, a[i+3].y};
        v4sf az = {a[i].z, a[i+1].z, a[i+2].z, a[i+3].z};
        v4sf aw = {a[i].w, a[i+1].w, a[i+2].w, a[i+3].w};
        v4sf bx = {b[i].x, b[i+1].x, b[i+2].x, b[i+3].x};
        v4sf by = {b[i].y, b[i+1].y, b[i+2].y, b[i+3].y};
        v4sf bz = {b[i].z, b[i+1].z, b[i+2].z, b[i+3].z};
        v4sf bw = {b[i].w, b[i+1].w, b[i+2].w, b[i+3].w};
        v4sf dot = ax*bx + ay*by + az*bz + aw*bw;
        out[i+0] = dot[0]; out[i+1] = dot[1];
        out[i+2] = dot[2]; out[i+3] = dot[3];
    }
    for (; i < n; i++) out[i] = a[i].x*b[i].x + a[i].y*b[i].y + a[i].z*b[i].z + a[i].w*b[i].w;
}

void v4_length_batch(const vec4_t *v, float *out, int n) {
    int i = 0;
    for (; i + 3 < n; i += 4) {
        v4sf vx = {v[i].x, v[i+1].x, v[i+2].x, v[i+3].x};
        v4sf vy = {v[i].y, v[i+1].y, v[i+2].y, v[i+3].y};
        v4sf vz = {v[i].z, v[i+1].z, v[i+2].z, v[i+3].z};
        v4sf vw = {v[i].w, v[i+1].w, v[i+2].w, v[i+3].w};
        v4sf sq = vx*vx + vy*vy + vz*vz + vw*vw;
        v4sf len = sq * rsqrt4(sq);
        out[i+0] = len[0]; out[i+1] = len[1];
        out[i+2] = len[2]; out[i+3] = len[3];
    }
    for (; i < n; i++) {
        float d = v[i].x*v[i].x + v[i].y*v[i].y + v[i].z*v[i].z + v[i].w*v[i].w;
        out[i] = sse_sqrtf(d);
    }
}
