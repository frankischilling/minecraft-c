#ifndef MC_CLIENT_TEXTURE_EFFECTS_EFFECT_INTERNAL_H
#define MC_CLIENT_TEXTURE_EFFECTS_EFFECT_INTERNAL_H
#include "../../entity/player/tick_math_internal.h"
#include "../../world/runtime/spawn_internal.h"
#include "../images/class_url.h"
#include "base.h"
#include "mc/block/registry/runtime.h"
#include "mc/core/java/java.h"
#include "mc/core/math/trig_double.h"
#include "mc/item/registry/runtime.h"
#include "mc/world/dimension/world_provider.h"
#include "mc/world/runtime/environment.h"
#include <float.h>
#include <string.h>

struct mc_texture_effect_error {
    mc_texture_effect_status status;
    const char *operation, *source_class, *source_method, *source_descriptor;
    int32_t source_pc, enclosing_pc, cause_pc;
    const char *dependency_class, *dependency_method, *dependency_descriptor;
    mc_registry_error registry;
    mc_java_random_profile_status random;
    mc_world_storage_error world;
    mc_world_provider_status provider;
    mc_world_spawn_error spawn;
    mc_client_class_url_error image;
    mc_water_texture_error water;
    mc_lava_texture_error lava;
};

typedef struct mc_texture_effect_owner {
    mc_client_texture_effect base;
    mc_texture_effect_phase phase;
    mc_texture_effect_services services;
    mc_texture_effect_options options;
    mc_texture_effect_fields fields;
    size_t array_bytes;
    float *g, *h, *i, *j;
    size_t g_length, h_length, i_length, j_length;
    uint8_t **frames;
    size_t frame_count;
    uint8_t **pending_frames;
    size_t pending_frame_count;
    uint32_t *icon, *dial;
    struct client_application *app;
    bool app_borrow_counted; /* Native App occurrence, not a source field or Engine hold. */
    int32_t counter, last_store_pc;
    double angle, velocity;
    mc_java_random *random; /* Only hs's actual explicitly seeded local Random. */
    bool random_constructed;
    mc_client_class_url *urls[2];
    mc_client_argb_image *images[2];
    /* Pending source new br at actual fd.u0, not a cached spawn value. */
    mc_player_coordinates *coordinate;
    mc_texture_effect_error failure, retirement;
} mc_texture_effect_owner;

const char *mc_texture_effect_class(mc_texture_effect_kind);
mc_texture_effect_status mc_texture_effect_fail(mc_texture_effect_error *, mc_texture_effect_status,
                                                const char *, const char *, const char *, int32_t);
void *mc_texture_effect_array(mc_texture_effect_owner *, size_t, int32_t,
                              mc_texture_effect_error *);
bool mc_texture_effect_fp(mc_texture_effect_error *, const char *, int32_t);
bool mc_texture_effect_random(mc_texture_effect_owner *, int32_t, double *,
                              mc_texture_effect_error *);
bool mc_texture_effect_pixel(mc_texture_effect_owner *, size_t, int32_t, int32_t, int32_t, int32_t,
                             const int32_t[4], mc_texture_effect_error *);
bool mc_texture_effect_load(mc_texture_effect_owner *, const float *, size_t, size_t, int32_t,
                            float *, mc_texture_effect_error *);
bool mc_texture_effect_store(mc_texture_effect_owner *, float *, size_t, size_t, float, int32_t,
                             mc_texture_effect_error *);
bool mc_texture_effect_constructor(mc_texture_effect_owner *, struct client_application *, int32_t,
                                   mc_texture_effect_error *);
bool mc_texture_portal_construct(mc_texture_effect_owner *, mc_texture_effect_error *);
bool mc_texture_portal_tick(mc_texture_effect_owner *, mc_texture_effect_error *);
bool mc_texture_flowing_water_tick(mc_texture_effect_owner *, mc_texture_effect_error *);
bool mc_texture_flowing_lava_tick(mc_texture_effect_owner *, mc_texture_effect_error *);
bool mc_texture_fire_tick(mc_texture_effect_owner *, mc_texture_effect_error *);
bool mc_texture_compass_tick(mc_texture_effect_owner *, mc_texture_effect_error *);
bool mc_texture_watch_tick(mc_texture_effect_owner *, mc_texture_effect_error *);
bool mc_texture_effect_resource(mc_texture_effect_owner *, unsigned, const char *, int32_t, int32_t,
                                int32_t, int32_t, bool, uint32_t *, mc_texture_effect_error *);

/* tick_math_internal.h supplies the existing qualified fdlibm leaf; no copy. */

static inline float ef_add(float a, float b) {
    volatile float v = a + b;
    return v;
}

static inline float ef_sub(float a, float b) {
    volatile float v = a - b;
    return v;
}

static inline float ef_mul(float a, float b) {
    volatile float v = a * b;
    return v;
}

static inline float ef_div(float a, float b) {
    volatile float v = a / b;
    return v;
}

static inline double ed_add(double a, double b) {
    return at_add(a, b);
}

static inline double ed_sub(double a, double b) {
    return at_sub(a, b);
}

static inline double ed_mul(double a, double b) {
    return at_mul(a, b);
}

static inline double ed_div(double a, double b) {
    return at_div(a, b);
}
#endif
