/* ═══════════════════════════════════════════════════════════════════════════════
 * VECTORD OS - Vector AI Integration Module
 * Bridges the C kernel with the Akasha AI system
 * Level: 1.67¹¹¹ | Hyperdimensional Computing Interface
 * ═══════════════════════════════════════════════════════════════════════════════ */

#include "../include/vecord.h"

/* External kernel functions */
extern u32 get_ticks(void);

/* ═══════════════════════════════════════════════════════════════════════════════
 * CONSTANTS
 * ═══════════════════════════════════════════════════════════════════════════════ */

#define AI_HD_DIM           4096    /* Hyperdimensional space dimension */
/* Paged concept storage: 0 static .bss consumption, grows on demand */
#define AI_CONCEPTS_PER_PAGE    64  /* Concepts per heap-allocated page */
#define AI_INITIAL_PAGE_CAP     2   /* Start with 2 pages (128 concepts) */
#define AI_MAX_PAGES            64  /* Cap: 64 pages * 64 = 4096 concepts */
#define AI_MAX_NAME_LEN     128     /* Maximum concept name length */
#define AI_MAX_TEXT_LEN     1024    /* Maximum concept text length */
#define AI_EMBEDDING_DIM    384     /* Input embedding dimension */

/* Concept types */
#define AI_TYPE_MATHEMATICS  1
#define AI_TYPE_COMPUTER_SCI 2
#define AI_TYPE_CLIFFORD     3
#define AI_TYPE_PHYSICS      4
#define AI_TYPE_PHILOSOPHY   5

/* ═══════════════════════════════════════════════════════════════════════════════
 * DATA STRUCTURES
 * ═══════════════════════════════════════════════════════════════════════════════ */

/* A knowledge concept in the Akasha memory */
typedef struct {
    u32 id;                         /* Unique identifier */
    char name[AI_MAX_NAME_LEN];     /* Concept name */
    char text[AI_MAX_TEXT_LEN];     /* Concept description */
    u8 type;                        /* Concept type */
    u32 timestamp;                  /* When ingested */
    float hd_vector[16];            /* Compressed hypervector (sample) */
} ai_concept_t;

/* Hyperdimensional vector (compressed representation) */
typedef struct {
    float data[AI_HD_DIM];         /* 4096D vector */
    u32 concept_count;             /* Number of bundled concepts */
} ai_hd_vector_t;

/* Page of concepts, heap-allocated */
typedef struct ai_concept_page {
    struct ai_concept_page *next;
    u32 count;                      /* Used slots in this page */
    ai_concept_t concepts[AI_CONCEPTS_PER_PAGE];
} ai_concept_page_t;

/* Akasha memory structure - paged, heap-backed */
typedef struct {
    ai_concept_page_t *first_page;  /* Head of page list */
    u32 page_count;                 /* Total pages allocated */
    u32 concept_count;              /* Total concepts across all pages */
    ai_hd_vector_t compressed_knowledge;  /* The "god vector" */
    u32 total_queries;
    u32 total_hits;
} akasha_memory_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * GLOBAL STATE
 * ═══════════════════════════════════════════════════════════════════════════════ */

static akasha_memory_t akasha_mem;
static int ai_initialized = 0;

/* ═══════════════════════════════════════════════════════════════════════════════
 * CONCEPT MANAGEMENT
 * ═══════════════════════════════════════════════════════════════════════════════ */

/* Initialize a hypervector to zero */
static void hd_zero(ai_hd_vector_t *vec) {
    for (int i = 0; i < AI_HD_DIM; i++) {
        vec->data[i] = 0.0f;
    }
    vec->concept_count = 0;
}

/* Bind two hypervectors (element-wise multiplication) */
static void hd_bind(const ai_hd_vector_t *a, const ai_hd_vector_t *b, ai_hd_vector_t *result) {
    for (int i = 0; i < AI_HD_DIM; i++) {
        result->data[i] = a->data[i] * b->data[i];
    }
    result->concept_count = a->concept_count;  /* Approximate */
}

/* Bundle (superpose) multiple hypervectors */
static void hd_bundle(const ai_hd_vector_t *vectors[], int count, ai_hd_vector_t *result) {
    hd_zero(result);
    
    for (int v = 0; v < count; v++) {
        for (int i = 0; i < AI_HD_DIM; i++) {
            result->data[i] += vectors[v]->data[i];
        }
    }
    
    /* Normalize */
    float norm = 0.0f;
    for (int i = 0; i < AI_HD_DIM; i++) {
        norm += result->data[i] * result->data[i];
    }
    norm = ({ float __s = norm; asm volatile ("fsqrt" : "=t"(__s) : "0"(__s)); __s; });
    
    if (norm > 0.0001f) {
        for (int i = 0; i < AI_HD_DIM; i++) {
            result->data[i] /= norm;
        }
    }
    
    result->concept_count = count;
}

/* Compute cosine similarity between two hypervectors */
static float hd_similarity(const ai_hd_vector_t *a, const ai_hd_vector_t *b) {
    float dot = 0.0f;
    float norm_a = 0.0f;
    float norm_b = 0.0f;
    
    for (int i = 0; i < AI_HD_DIM; i++) {
        dot += a->data[i] * b->data[i];
        norm_a += a->data[i] * a->data[i];
        norm_b += b->data[i] * b->data[i];
    }
    
    /* Inline sqrt using x87 FPU */
    asm volatile ("fsqrt" : "=t"(norm_a) : "0"(norm_a));
    asm volatile ("fsqrt" : "=t"(norm_b) : "0"(norm_b));
    
    if (norm_a < 0.0001f || norm_b < 0.0001f) return 0.0f;
    
    return dot / (norm_a * norm_b);
}

/* Generate a deterministic hypervector from a hash */
static void hd_from_hash(u32 hash, ai_hd_vector_t *result) {
    /* Simple deterministic pseudo-random generation */
    u32 seed = hash;
    
    for (int i = 0; i < AI_HD_DIM; i++) {
        /* LCG pseudo-random */
        seed = seed * 1103515245 + 12345;
        
        /* Convert to float in [-1, 1] */
        result->data[i] = ((float)((seed >> 16) & 0x7FFF) / 16384.0f) - 1.0f;
    }
    
    /* Normalize */
    float sum = 0.0f;
    for (int i = 0; i < AI_HD_DIM; i++) {
        sum += result->data[i] * result->data[i];
    }
    
    if (sum > 0.0001f) {
        float norm = ({ float __s = sum; asm volatile ("fsqrt" : "=t"(__s) : "0"(__s)); __s; });
        for (int i = 0; i < AI_HD_DIM; i++) {
            result->data[i] /= norm;
        }
    }
    
    result->concept_count = 1;
}

/* Simple sqrtf implementation for freestanding */
static float ai_sqrtf(float x) {
    float result;
    asm volatile ("sqrtss %1, %0" : "=x"(result) : "x"(x));
    return result;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * CONCEPT MANAGEMENT
 * ═══════════════════════════════════════════════════════════════════════════════ */

/* Compute hash of a string */
static u32 hash_string(const char *str) {
    u32 hash = 5381;
    while (*str) {
        hash = ((hash << 5) + hash) + *str++;
    }
    return hash;
}

/* Allocate a new concept page from heap */
static ai_concept_page_t *ai_alloc_page(void) {
    ai_concept_page_t *page = (ai_concept_page_t *)kmalloc(sizeof(ai_concept_page_t));
    if (!page) return NULL;
    page->next = NULL;
    page->count = 0;
    return page;
}

/* Get the last page in the list (or allocate first if empty) */
static ai_concept_page_t *ai_ensure_page(void) {
    if (!akasha_mem.first_page) {
        ai_concept_page_t *p = ai_alloc_page();
        if (!p) return NULL;
        akasha_mem.first_page = p;
        akasha_mem.page_count = 1;
        return p;
    }
    /* Find last page with space, or allocate new */
    ai_concept_page_t *p = akasha_mem.first_page;
    while (p) {
        if (p->count < AI_CONCEPTS_PER_PAGE) return p;
        if (!p->next) break;
        p = p->next;
    }
    /* Need new page */
    if (akasha_mem.page_count >= AI_MAX_PAGES) return NULL;
    ai_concept_page_t *np = ai_alloc_page();
    if (!np) return NULL;
    p->next = np;
    akasha_mem.page_count++;
    return np;
}

/* Get concept by index (across pages) */
static ai_concept_t *ai_get_concept(u32 index) {
    ai_concept_page_t *p = akasha_mem.first_page;
    while (p) {
        if (index < p->count) return &p->concepts[index];
        index -= p->count;
        p = p->next;
    }
    return NULL;
}

/* Add a concept to Akasha memory */
int ai_ingest_concept(const char *name, const char *text, u8 type) {
    if (!ai_initialized) {
        serial_puts("[AI] Error: Vector AI not initialized\n");
        return -1;
    }
    
    ai_concept_page_t *page = ai_ensure_page();
    if (!page) {
        serial_puts("[AI] Error: Akasha memory full (no heap/pages)\n");
        return -1;
    }
    
    ai_concept_t *concept = &page->concepts[page->count];
    
    /* Set ID from hash */
    concept->id = hash_string(name);
    
    /* Copy name */
    int i;
    for (i = 0; i < AI_MAX_NAME_LEN - 1 && name[i]; i++) {
        concept->name[i] = name[i];
    }
    concept->name[i] = '\0';
    
    /* Copy text */
    for (i = 0; i < AI_MAX_TEXT_LEN - 1 && text[i]; i++) {
        concept->text[i] = text[i];
    }
    concept->text[i] = '\0';
    
    concept->type = type;
    concept->timestamp = get_ticks();
    
    page->count++;
    akasha_mem.concept_count++;
    
    serial_puts("[AI] Ingested: ");
    serial_puts(concept->name);
    serial_puts("\n");
    
    return 0;
}

/* Query Akasha memory for related concepts */
int ai_query_concept(const char *query, int top_k) {
    if (!ai_initialized) {
        serial_puts("[AI] Error: Vector AI not initialized\n");
        return -1;
    }
    
    akasha_mem.total_queries++;
    
    serial_puts("\n┌─────────────────────────────────────────────┐\n");
    serial_puts("│  AKASHA QUERY RESULTS                       │\n");
    serial_puts("├─────────────────────────────────────────────┤\n");
    
    /* Simple string matching for now */
    int found = 0;
    for (u32 i = 0; i < akasha_mem.concept_count && found < top_k; i++) {
        ai_concept_t *concept = ai_get_concept(i);
        if (!concept) break;
        
        /* Check if query matches concept name (simple substring) */
        int match = 0;
        for (int j = 0; concept->name[j]; j++) {
            if (concept->name[j] == query[0]) {
                match = 1;
                break;
            }
        }
        
        if (match) {
            serial_puts("│ [1.00] ");
            
            /* Print concept name */
            int name_len = 0;
            while (concept->name[name_len] && name_len < 35) {
                serial_putc(concept->name[name_len]);
                name_len++;
            }
            while (name_len < 35) {
                serial_putc(' ');
                name_len++;
            }
            serial_puts(" │\n");
            
            found++;
        }
    }
    
    /* Always show available concepts */
    serial_puts("│                                             │\n");
    serial_puts("│ Available concepts:                         │\n");
    for (u32 i = 0; i < akasha_mem.concept_count; i++) {
        ai_concept_t *concept = ai_get_concept(i);
        if (!concept) break;
        serial_puts("│   - ");
        serial_puts(concept->name);
        serial_puts("\n");
    }
    
    serial_puts("└─────────────────────────────────────────────┘\n\n");
    
    akasha_mem.total_hits += found;
    return found;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * INITIALIZATION
 * ═══════════════════════════════════════════════════════════════════════════════ */

void ai_init(void) {
    serial_puts("[AI] Initializing Vector AI Module...\n");
    
    /* Clear state */
    akasha_mem.first_page = NULL;
    akasha_mem.page_count = 0;
    akasha_mem.concept_count = 0;
    akasha_mem.total_queries = 0;
    akasha_mem.total_hits = 0;

    /* Load fundamental concepts */
    serial_puts("[AI] Loading fundamental axioms...\n");
    
    /* Allocate first page and load 3 seed concepts */
    ai_concept_page_t *p = ai_alloc_page();
    if (!p) {
        serial_puts("[AI] Warning: no heap available, Akasha disabled\n");
        ai_initialized = 1;
        return;
    }
    akasha_mem.first_page = p;
    akasha_mem.page_count = 1;

    /* Concept 1: Turing machines */
    ai_concept_t *c = &p->concepts[p->count++];
    c->id = 1;
    memcpy(c->name, "turing", 7);
    memcpy(c->text, "Turing machines", 16);
    c->type = AI_TYPE_COMPUTER_SCI;
    akasha_mem.concept_count++;
    
    /* Concept 2: Clifford algebra */
    c = &p->concepts[p->count++];
    c->id = 2;
    memcpy(c->name, "clifford", 9);
    memcpy(c->text, "Clifford algebra", 17);
    c->type = AI_TYPE_CLIFFORD;
    akasha_mem.concept_count++;
    
    /* Concept 3: Lambda calculus */
    c = &p->concepts[p->count++];
    c->id = 3;
    memcpy(c->name, "lambda", 7);
    memcpy(c->text, "Lambda calculus", 16);
    c->type = AI_TYPE_COMPUTER_SCI;
    akasha_mem.concept_count++;
    
    ai_initialized = 1;
    
    serial_puts("[AI] Vector AI Module initialized\n");
    serial_puts("[AI] Concepts loaded: ");
    serial_putdec(akasha_mem.concept_count);
    serial_puts(" (");
    serial_putdec(akasha_mem.page_count);
    serial_puts(" page(s), ");
    serial_putdec(sizeof(ai_concept_page_t));
    serial_puts(" bytes/page)\n");
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * STATISTICS
 * ═══════════════════════════════════════════════════════════════════════════════ */

void ai_stats(void) {
    serial_puts("\n┌─────────────────────────────────────────────┐\n");
    serial_puts("│  VECTOR AI STATISTICS                       │\n");
    serial_puts("├─────────────────────────────────────────────┤\n");
    
    serial_puts("│  Total concepts:      ");
    serial_putdec(akasha_mem.concept_count);
    serial_puts("                  │\n");
    
    serial_puts("│  Pages allocated:     ");
    serial_putdec(akasha_mem.page_count);
    serial_puts("                  │\n");
    
    serial_puts("│  Page capacity:       ");
    serial_putdec(akasha_mem.page_count * AI_CONCEPTS_PER_PAGE);
    serial_puts("                  │\n");
    
    serial_puts("│  Total queries:       ");
    serial_putdec(akasha_mem.total_queries);
    serial_puts("                  │\n");
    
    serial_puts("│  Successful hits:     ");
    serial_putdec(akasha_mem.total_hits);
    serial_puts("                  │\n");
    
    serial_puts("│  Dimensions:          ");
    serial_putdec(AI_HD_DIM);
    serial_puts("                 │\n");
    serial_puts("└─────────────────────────────────────────────┘\n\n");
}
