#ifndef MC_CLIENT_RENDER_MODEL_RUNTIME_INTERNAL_H
#define MC_CLIENT_RENDER_MODEL_RUNTIME_INTERNAL_H

#include "manager/internal.h"
#include "mc/client/render/model_draw.h"
#include <stdint.h>

typedef struct mc_client_model_runtime_context {
    mc_math_classes *math;
    mc_client_render_engine_classes *graphics;
    mc_entity_classes *entities;
    mc_client_display *display;
} mc_client_model_runtime_context;

typedef enum mc_client_model_runtime_status {
    MC_CLIENT_MODEL_RUNTIME_OK,
    MC_CLIENT_MODEL_RUNTIME_ARGUMENT,
    MC_CLIENT_MODEL_RUNTIME_STATE,
    MC_CLIENT_MODEL_RUNTIME_BUSY,
    MC_CLIENT_MODEL_RUNTIME_LIMIT,
    MC_CLIENT_MODEL_RUNTIME_ALLOCATION,
    MC_CLIENT_MODEL_RUNTIME_MATH_FAILED,
    MC_CLIENT_MODEL_RUNTIME_PART_FAILED,
    MC_CLIENT_MODEL_RUNTIME_SOURCE_EXCEPTION,
    MC_CLIENT_MODEL_RUNTIME_UNIMPLEMENTED
} mc_client_model_runtime_status;

typedef enum mc_client_model_runtime_child {
    MC_CLIENT_MODEL_RUNTIME_CHILD_NONE,
    MC_CLIENT_MODEL_RUNTIME_CHILD_MATH,
    MC_CLIENT_MODEL_RUNTIME_CHILD_PART
} mc_client_model_runtime_child;

/* Borrowed identities belong to the parent's retained renderer graph. These
 * are observations of the actual selected model/part, never replacement data. */
typedef struct mc_client_model_runtime_state {
    const manager_model *model;
    const mc_client_model_part *part;
    const char *runtime_class;
    mc_client_graphics_site site, event_site, pose_site, render_site;
    float arguments[6];
    uintptr_t actor_identity;
    unsigned int argument_count, pose_stores, math_calls, part_calls, part_returns;
    unsigned int faces, vertices, compiled_parts;
    bool returned, event_returned, pose_returned;
    /* Actual superclass bodies may have returned before an enclosing HH
     * override fails. These do not credit the selected pose/render return. */
    mc_client_graphics_site nn_pose_site, nn_render_site;
    bool nn_pose_returned, nn_render_returned;
} mc_client_model_runtime_state;

typedef struct mc_client_model_runtime_error {
    mc_client_model_runtime_status status;
    const char *operation, *exception_class;
    mc_client_graphics_site caller, parent, site, child;
    bool native_policy, source_exception;
    mc_client_model_draw_throwable throwable;
    mc_client_model_runtime_child selected_child;
    mc_math_class_error math;
    mc_client_model_draw_error part;
    mc_client_model_runtime_state state;
    /* Active model call sites, outermost first. The supplied caller remains
     * separate; HH's maximum is HH.a10 -> NN.a10 -> HH.b10. */
    mc_client_graphics_site ancestors[3];
    unsigned int ancestor_count;
} mc_client_model_runtime_error;

void mc_client_model_runtime_error_init(mc_client_model_runtime_error *);

/* Entered synchronous KO/NN/HH/TD dispatch on the SAME manager_model allocation.
 * The parent holds/guards the complete manager/renderer graph and actual
 * service namespaces until all borrowed diagnostics/pending PS rights retire.
 * An empty KO event reads no actor or service. Six-float methods read no
 * Entity. Source fields, lists, pools and GL effects are never repaired. */
bool mc_client_model_event_entered(mc_client_managed_renderer *, manager_model *,
                                   mc_entity *nullable_actor, float limb_swing, float limb_amount,
                                   float partial, const mc_client_model_runtime_context *,
                                   mc_client_graphics_site caller, mc_client_model_runtime_error *);
bool mc_client_model_pose_entered(mc_client_managed_renderer *, manager_model *, float limb_swing,
                                  float limb_amount, float animation_age, float head_yaw,
                                  float head_pitch, float scale,
                                  const mc_client_model_runtime_context *,
                                  mc_client_graphics_site caller, mc_client_model_runtime_error *);
bool mc_client_model_render_entered(mc_client_managed_renderer *, manager_model *, float limb_swing,
                                    float limb_amount, float animation_age, float head_yaw,
                                    float head_pitch, float scale,
                                    const mc_client_model_runtime_context *,
                                    mc_client_graphics_site caller,
                                    mc_client_model_runtime_error *);

#endif
