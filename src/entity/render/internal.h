#ifndef MC_ENTITY_RENDER_INTERNAL_H
#define MC_ENTITY_RENDER_INTERNAL_H
#include "mc/core/math/classes.h"
#include "mc/entity/player/player.h"
#include "mc/world/runtime/brightness.h"
#include "mc/world/runtime/movement.h"

typedef struct mc_entity_render_site {
    const char *class_name, *method, *descriptor;
    int32_t pc;
} mc_entity_render_site;

typedef enum mc_entity_render_throwable {
    MC_ENTITY_RENDER_NO_THROWABLE,
    MC_ENTITY_RENDER_SOURCE_EXCEPTION,
    MC_ENTITY_RENDER_SOURCE_ERROR,
    MC_ENTITY_RENDER_UNCLASSIFIED_THROWABLE
} mc_entity_render_throwable;

typedef enum mc_entity_render_child {
    MC_ENTITY_RENDER_CHILD_NONE,
    MC_ENTITY_RENDER_CHILD_MATH,
    MC_ENTITY_RENDER_CHILD_AREA,
    MC_ENTITY_RENDER_CHILD_BRIGHTNESS
} mc_entity_render_child;

typedef enum mc_entity_render_query_kind {
    MC_ENTITY_RENDER_QUERY_BOOLEAN,
    MC_ENTITY_RENDER_QUERY_FLOAT_BITS
} mc_entity_render_query_kind;

typedef struct mc_entity_render_query {
    uintptr_t receiver;
    mc_entity_render_site method, caller;
    int32_t args[6];
    size_t nargs;
    mc_entity_render_query_kind kind;

    union {
        bool boolean;
        uint32_t float_bits;
    } value;

    bool invoked, returned, failed, native_policy, source_exception;
    const char *exception_class;
} mc_entity_render_query;

typedef struct mc_entity_render_error {
    mc_entity_status status;
    const char *operation, *exception_class;
    mc_entity_render_site site, child;
    bool native_policy, source_exception;
    mc_entity_render_throwable throwable;
    mc_entity_render_child selected_child;
    int32_t watcher_index;
    unsigned int watcher_pc;
    mc_math_class_error math;
    mc_world_movement_error area;
    mc_world_brightness_error brightness;
    /* Fixed owned native World holds, acquired BEFORE each reached query.
     * Parent adopts these slots or releases them with worlds_release before
     * reusing this error. Identities remain scalar observations after release. */
    mc_world_runtime *world_reads[2];
    uintptr_t world_identities[2];
    size_t world_read_count;
    mc_entity_render_query queries[2];
    size_t query_count, published_query_count;
} mc_entity_render_error;

/* Initialize fresh/discharged storage only; never discard owning slots. */
void mc_entity_render_error_init(mc_entity_render_error *);
/* Passive classification against the same Registry namespace's immutable first cause. */
void mc_entity_render_registry_classification(mc_registry_classes *, const mc_registry_error *,
                                              mc_entity_render_throwable *, const char **);
/* Native-only checked discharge. Returns OK after all slots are NULL. A
 * broken native binding invariant preserves that and later slots for retry;
 * previously discharged slots stay NULL. Never overwrites source diagnostics. */
mc_entity_status mc_entity_render_worlds_release(mc_entity_render_error *);
/* Entered source leaves on actual held canonical Entity storage. No phase,
 * allocation, constructor, source field or Random mutation is introduced.
 * Output/error are required/disjoint; parent owns nullable invoke ordering. */
bool mc_entity_render_brightness(mc_entity *, mc_math_classes *, float partial, float *,
                                 mc_entity_render_error *);
bool mc_entity_render_swing(mc_entity *, float partial, float *, mc_entity_render_error *);
bool mc_entity_render_riding(mc_entity *, bool *, mc_entity_render_error *);
bool mc_entity_render_burning(mc_entity *, bool *, mc_entity_render_error *);
bool mc_entity_render_half_height(mc_entity *, float *, mc_entity_render_error *);
bool mc_entity_render_texture(mc_entity *, const mc_player_string **, mc_entity_render_error *);
bool mc_entity_render_sheep_sheared(mc_entity *, bool *, mc_entity_render_error *);
bool mc_entity_render_sheep_color(mc_entity *, int32_t *, mc_entity_render_error *);
bool mc_entity_render_pig_saddled(mc_entity *, bool *, mc_entity_render_error *);
#endif
