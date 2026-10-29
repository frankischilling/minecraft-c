#ifndef MC_CLIENT_RENDER_ENGINE_EFFECTS_H
#define MC_CLIENT_RENDER_ENGINE_EFFECTS_H
#include "../texture_effects/effect_internal.h"
#include "internal.h"

typedef struct mc_client_effect_registration_error {
    mc_texture_effect_status status;
    const char *operation;
    int32_t ji_pc;
    mc_client_render_engine_status engine;
    mc_texture_effect_error child;
    bool appended, child_returned, source_returned;
} mc_client_effect_registration_error;

/* Genuine ji.a(aw), no GL/Settings/bind/upload. maximum zero selects4096.
 * Callers own Engine/effect and heap diagnostic output exclusively. NULL is
 * a real List occurrence; duplicates are separate native holds. Failed child
 * calls preserve the actual append. All child services/App stay live. */
bool mc_client_render_engine_register_effect(mc_client_render_engine *,
                                             mc_client_texture_effect *nullable_effect,
                                             size_t maximum, mc_client_effect_registration_error *);
/* Explicit terminal native container cleanup, no Java List.clear or tick.
 * Progress counts released source occurrences without dereferencing their
 * historical identities again; the actual g table/count is not replaced.
 * Later registrations/ticks reject this stopped container. Allocation-free
 * retries preserve the current unconsumed occurrence on child BUSY/error. */
bool mc_client_render_engine_retire_effects(mc_client_render_engine *,
                                            mc_client_effect_registration_error *);
bool mc_client_render_engine_effect_at(const mc_client_render_engine *, size_t,
                                       const mc_client_texture_effect **);
#endif
