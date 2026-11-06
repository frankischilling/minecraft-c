#ifndef MC_CLIENT_TEXTURE_EFFECTS_LAVA_H
#define MC_CLIENT_TEXTURE_EFFECTS_LAVA_H

#include "mc/block/registry/registry.h"
#include "mc/core/math/math.h"

typedef struct mc_client_lava_texture_effect mc_client_lava_texture_effect;

typedef enum mc_lava_texture_phase {
    MC_LAVA_TEXTURE_RAW,
    MC_LAVA_TEXTURE_CONSTRUCTING,
    MC_LAVA_TEXTURE_READY,
    MC_LAVA_TEXTURE_FAILED
} mc_lava_texture_phase;

typedef enum mc_lava_texture_status {
    MC_LAVA_TEXTURE_OK,
    MC_LAVA_TEXTURE_INVALID_ARGUMENT,
    MC_LAVA_TEXTURE_INVALID_PHASE,
    MC_LAVA_TEXTURE_BUSY,
    MC_LAVA_TEXTURE_ALLOCATION_FAILED,
    MC_LAVA_TEXTURE_LIMIT,
    MC_LAVA_TEXTURE_REGISTRY_FAILED,
    MC_LAVA_TEXTURE_NULL_REFERENCE,
    MC_LAVA_TEXTURE_INDEX_OUT_OF_RANGE,
    MC_LAVA_TEXTURE_RANDOM_FAILED
} mc_lava_texture_status;

typedef struct mc_lava_texture_error {
    mc_lava_texture_status status;
    const char *operation;
    const char *source_class;
    int32_t source_pc, cg_pc; /* aw child failures retain enclosing cg7. */
    mc_registry_error registry;
    mc_java_random_profile_status random;
} mc_lava_texture_error;

typedef struct mc_lava_texture_options {
    /* Native array quota. Zero selects the exact5120-byte constructor graph. */
    size_t max_array_bytes;
} mc_lava_texture_options;

typedef struct mc_lava_texture_fields {
    const uint8_t *a;
    size_t a_length;
    int32_t b;
    bool c;
    int32_t d, e, f; /* Actual inherited aw fields. */
    const float *g, *h, *i, *j;
    size_t g_length, h_length, i_length, j_length;
} mc_lava_texture_fields;

typedef struct mc_lava_texture_state {
    mc_lava_texture_phase phase;
    size_t array_bytes;
    mc_lava_texture_fields fields;
} mc_lava_texture_state;

/* Actual client cg/aw constructor: uu.D is flowing Lava10, texture237.
 * Allocation retains the actual client Registry and borrows its same Random
 * profile and the application's initialized, immutable in lookup table.
 * Both borrowed owners must outlive the effect. Constructor performs no trig,
 * Math.random, World, clock or GL operation beyond reached Registry active use.
 * A failed constructor retains every completed array and cannot retry.
 * Callers are exclusive; outputs/errors must be disjoint from owner storage. */
mc_lava_texture_status
mc_client_lava_texture_allocate(mc_registry_classes *, mc_java_random_profile *,
                                const mc_math_trig *, const mc_lava_texture_options *,
                                mc_client_lava_texture_effect **, mc_lava_texture_error *);
mc_lava_texture_status mc_client_lava_texture_construct(mc_client_lava_texture_effect *,
                                                        mc_lava_texture_error *);

/* Actual cg.a()V on that READY owner. Each cell performs nine g reads, four
 * live i reads, h/i/j stores and the shared Math.random call at311 in source
 * order. Earlier cells' i updates are visible to later cells. Failures keep
 * all completed stores and Random advances, leave READY and skip unreached
 * swaps/pixels. No per-frame allocation, local Random or rollback is supplied.
 * Captured strict FP environment must remain stable throughout the call. */
mc_lava_texture_status mc_client_lava_texture_animate(mc_client_lava_texture_effect *,
                                                      mc_lava_texture_error *);
/* Actual aw.c field write; native READY/exclusive admission only. */
mc_lava_texture_status mc_client_lava_texture_set_anaglyph(mc_client_lava_texture_effect *, bool,
                                                           mc_lava_texture_error *);
bool mc_client_lava_texture_read(const mc_client_lava_texture_effect *, mc_lava_texture_state *);
mc_lava_texture_status mc_client_lava_texture_retire(mc_client_lava_texture_effect **,
                                                     mc_lava_texture_error *);

/* App.ae publication, aw.a(ji), registration and GPU upload are separate bodies. */
#endif
