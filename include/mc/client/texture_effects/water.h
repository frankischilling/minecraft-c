#ifndef MC_CLIENT_TEXTURE_EFFECTS_WATER_H
#define MC_CLIENT_TEXTURE_EFFECTS_WATER_H

#include "mc/block/registry/registry.h"

typedef struct mc_client_water_texture_effect mc_client_water_texture_effect;

typedef enum mc_water_texture_phase {
    MC_WATER_TEXTURE_RAW,
    MC_WATER_TEXTURE_CONSTRUCTING,
    MC_WATER_TEXTURE_READY,
    MC_WATER_TEXTURE_FAILED
} mc_water_texture_phase;

typedef enum mc_water_texture_status {
    MC_WATER_TEXTURE_OK,
    MC_WATER_TEXTURE_INVALID_ARGUMENT,
    MC_WATER_TEXTURE_INVALID_PHASE,
    MC_WATER_TEXTURE_BUSY,
    MC_WATER_TEXTURE_ALLOCATION_FAILED,
    MC_WATER_TEXTURE_LIMIT,
    MC_WATER_TEXTURE_REGISTRY_FAILED,
    MC_WATER_TEXTURE_NULL_REFERENCE
} mc_water_texture_status;

typedef struct mc_water_texture_error {
    mc_water_texture_status status;
    const char *operation;
    const char *source_class;
    int32_t source_pc, vs_pc; /* aw child failure keeps the enclosing vs7. */
    mc_registry_error registry;
} mc_water_texture_error;

typedef struct mc_water_texture_options {
    /* Native array-byte quota. Zero selects the exact5120-byte normal graph.
     * Allocation records/owner itself are separate native storage. */
    size_t max_array_bytes;
} mc_water_texture_options;

typedef struct mc_water_texture_fields {
    const uint8_t *a;
    size_t a_length; /* actual aw.a byte[1024] */
    int32_t b;
    bool c;
    int32_t d, e, f; /* actual aw fields */
    const float *g, *h, *i, *j;
    size_t g_length, h_length, i_length, j_length;
    int32_t k; /* actual vs private counter */
} mc_water_texture_fields;

typedef struct mc_water_texture_state {
    mc_water_texture_phase phase;
    size_t array_bytes;
    mc_water_texture_fields fields;
} mc_water_texture_state;

/* Genuine client vs water texture constructor: actual B is flowing water8,
 * named water with texture205. Retain the actual Registry/profile namespace;
 * no World, entropy, clock, animation, GL upload or device operation occurs.
 * A failed constructor stays inspectable and owns every completed array.
 * All callers are exclusive; output/error storage must be owner-disjoint. */
mc_water_texture_status mc_client_water_texture_allocate(mc_registry_classes *,
                                                         mc_java_random_profile *,
                                                         const mc_water_texture_options *,
                                                         mc_client_water_texture_effect **,
                                                         mc_water_texture_error *);
mc_water_texture_status mc_client_water_texture_construct(mc_client_water_texture_effect *,
                                                          mc_water_texture_error *);
bool mc_client_water_texture_read(const mc_client_water_texture_effect *, mc_water_texture_state *);
mc_water_texture_status mc_client_water_texture_retire(mc_client_water_texture_effect **,
                                                       mc_water_texture_error *);
/* Animation vs.a()V and binding aw.a(ji)V are separate unfinished descendants;
 * this constructor API exposes neither as a successful empty operation. */
#endif
