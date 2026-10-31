#ifndef MC_CLIENT_TEXTURE_EFFECTS_EFFECT_H
#define MC_CLIENT_TEXTURE_EFFECTS_EFFECT_H
#include "mc/client/texture_effects/lava.h"
#include "mc/client/texture_effects/water.h"
#include "mc/resources/files/files.h"

typedef struct mc_client_texture_effect mc_client_texture_effect;
typedef struct mc_texture_effect_error mc_texture_effect_error;
struct client_application;

typedef enum mc_texture_effect_kind {
    MC_TEXTURE_EFFECT_WATER,
    MC_TEXTURE_EFFECT_LAVA,
    MC_TEXTURE_EFFECT_PORTAL,
    MC_TEXTURE_EFFECT_COMPASS,
    MC_TEXTURE_EFFECT_WATCH,
    MC_TEXTURE_EFFECT_FLOWING_WATER,
    MC_TEXTURE_EFFECT_FLOWING_LAVA,
    MC_TEXTURE_EFFECT_FIRE
} mc_texture_effect_kind;

typedef enum mc_texture_effect_phase {
    MC_TEXTURE_EFFECT_RAW,
    MC_TEXTURE_EFFECT_CONSTRUCTING,
    MC_TEXTURE_EFFECT_READY,
    MC_TEXTURE_EFFECT_FAILED,
    MC_TEXTURE_EFFECT_RETIRING
} mc_texture_effect_phase;

typedef enum mc_texture_effect_status {
    MC_TEXTURE_EFFECT_OK,
    MC_TEXTURE_EFFECT_INVALID_ARGUMENT,
    MC_TEXTURE_EFFECT_INVALID_PHASE,
    MC_TEXTURE_EFFECT_BUSY,
    MC_TEXTURE_EFFECT_LIMIT,
    MC_TEXTURE_EFFECT_ALLOCATION_FAILED,
    MC_TEXTURE_EFFECT_NULL_REFERENCE,
    MC_TEXTURE_EFFECT_INDEX_OUT_OF_RANGE,
    MC_TEXTURE_EFFECT_REGISTRY_FAILED,
    MC_TEXTURE_EFFECT_RANDOM_FAILED,
    MC_TEXTURE_EFFECT_FP_UNSUPPORTED,
    MC_TEXTURE_EFFECT_IMAGE_FAILED,
    MC_TEXTURE_EFFECT_CHILD_FAILED,
    MC_TEXTURE_EFFECT_DIAGNOSTIC_DEPENDENCY,
    MC_TEXTURE_EFFECT_WORLD_FAILED,
    MC_TEXTURE_EFFECT_UNIMPLEMENTED,
    MC_TEXTURE_EFFECT_NATIVE_CLOSE_FAILED
} mc_texture_effect_status;

typedef struct mc_texture_effect_services {
    mc_registry_classes *registry;
    mc_java_random_profile *profile;
    const mc_math_trig *trig;
    /* Concrete Class URL namespace. It must be the same declared class source,
     * never the selected Pack or a replacement decoded-image provider. */
    mc_resource_files *files;
    const mc_registry_resources *resources;
} mc_texture_effect_services;

typedef struct mc_texture_effect_options {
    size_t max_array_bytes, max_encoded_bytes, max_decoded_bytes, max_inflater_bytes;
    size_t max_angle_iterations;
} mc_texture_effect_options;

typedef struct mc_texture_effect_fields {
    const uint8_t *a;
    size_t a_length;
    int32_t b;
    bool c;
    int32_t d, e, f;
} mc_texture_effect_fields;

typedef struct mc_texture_effect_state {
    mc_texture_effect_kind kind;
    mc_texture_effect_phase phase;
    size_t native_holds, array_bytes;
    mc_texture_effect_fields fields;
    const float *g, *h, *i, *j;
    size_t g_length, h_length, i_length, j_length;
    const uint8_t *const *frames;
    size_t frame_count;
    const uint32_t *icon, *dial;
    struct client_application *app;
    int32_t counter, last_store_pc;
    double angle, velocity;
    const mc_texture_effect_error *failure, *retirement;
} mc_texture_effect_state;

/* Exact new source owners hs/av/ku/oh/if/sd. RAW allocation is cold: no class
 * activation, floating arithmetic, clock, entropy, file or GL operation.
 * Public Water/Lava allocation remains its existing API. Services and the
 * actual App argument remain borrowed through all retained failed/list uses.
 * Registered Apps carry one native effect borrow until final successful
 * cleanup, including failed constructors and externally retained effects.
 * RAW prepared Apps remain explicitly caller-owned stable storage until all
 * compass/watch occurrences end. No App-to-Engine counted cycle is added.
 * Error layout is internal so source-call owners use bounded heap storage;
 * native consumers can read the retained complete record through inspect. */
mc_texture_effect_status mc_client_texture_effect_allocate(const mc_texture_effect_services *,
                                                           mc_texture_effect_kind,
                                                           const mc_texture_effect_options *,
                                                           mc_client_texture_effect **,
                                                           mc_texture_effect_error *);
mc_texture_effect_status mc_client_texture_effect_construct(mc_client_texture_effect *,
                                                            struct client_application *nullable_app,
                                                            int32_t argument,
                                                            mc_texture_effect_error *);
mc_texture_effect_status mc_client_texture_effect_tick(mc_client_texture_effect *,
                                                       mc_texture_effect_error *);
mc_texture_effect_status mc_client_texture_effect_retain(mc_client_texture_effect *,
                                                         mc_texture_effect_error *);
mc_texture_effect_status mc_client_texture_effect_release(mc_client_texture_effect **,
                                                          mc_texture_effect_error *);
mc_texture_effect_status mc_client_texture_effect_set_anaglyph(mc_client_texture_effect *, bool,
                                                               mc_texture_effect_error *);
bool mc_client_texture_effect_inspect(const mc_client_texture_effect *, mc_texture_effect_state *);
/* Borrowed views of the exact same genuine subtype allocation, no wrapper. */
mc_client_texture_effect *mc_client_water_texture_as_effect(mc_client_water_texture_effect *);
mc_client_texture_effect *mc_client_lava_texture_as_effect(mc_client_lava_texture_effect *);
#endif
