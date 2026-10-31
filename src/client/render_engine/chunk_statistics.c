#include "chunk_statistics.h"
#include "internal.h"
#include "mc/core/java/java.h"

static bool active_use(mc_client_render_engine_classes *c, mc_client_graphics_site caller,
                       mc_client_graphics_error *e) {
    *e = (mc_client_graphics_error){.entry = caller, .caller = caller};
    if (c == NULL) {
        mc_client_graphics_fail(e, MC_CLIENT_GRAPHICS_INVALID_ARGUMENT, "dk native class namespace",
                                caller, true);
        return false;
    }
    if (c->resource_retirement_started || c->active_source_calls != 0 ||
        c->active_resource_calls != 0 || c->dk_phase == MC_CLIENT_RENDER_ENGINE_CONSTRUCTING) {
        mc_client_graphics_fail(e, MC_CLIENT_GRAPHICS_BUSY, "dk class active/retiring", caller,
                                true);
        return false;
    }
    if (c->dk_phase == MC_CLIENT_RENDER_ENGINE_READY)
        return true;
    if (c->dk_phase == MC_CLIENT_RENDER_ENGINE_FAILED) {
        *e = c->dk_failure;
        e->entry = e->caller = caller;
        e->dependency_class_cause = e->class_cause;
        e->class_cause = (mc_client_graphics_cause){c->dk_failure.status, c->dk_failure.operation,
                                                    c->dk_failure.entry, c->dk_failure.site,
                                                    c->dk_failure.native_policy};
        mc_client_graphics_fail(e, MC_CLIENT_GRAPHICS_CLASS_FAILED,
                                "previous actual dk class initialization failure", caller, false);
        return false;
    }
    c->dk_phase = MC_CLIENT_RENDER_ENGINE_CONSTRUCTING;
    mc_client_tessellator *tessellator = NULL;
    const mc_client_graphics_site dependency = {"dk", "<clinit>", "()V", 0};
    if (!mc_client_nw_active_use(c, dependency, &tessellator, e)) {
        e->entry = caller;
        e->parent = dependency;
        c->dk_phase = MC_CLIENT_RENDER_ENGINE_FAILED;
        c->dk_failure = *e;
        return false;
    }
    c->dk_D = tessellator; /* Actual putstatic3, same genuine nw singleton. */
    c->dk_b = 0;           /* Actual putstatic7. */
    c->dk_phase = MC_CLIENT_RENDER_ENGINE_READY;
    *e = (mc_client_graphics_error){.entry = caller, .caller = caller, .site = caller};
    return true;
}

bool mc_client_chunk_statistics_get(mc_client_render_engine_classes *c,
                                    mc_client_graphics_site caller, int32_t *out,
                                    mc_client_graphics_error *e) {
    if (e == NULL)
        return false;
    if (out == NULL) {
        *e = (mc_client_graphics_error){.entry = caller, .caller = caller};
        mc_client_graphics_fail(e, MC_CLIENT_GRAPHICS_INVALID_ARGUMENT, "dk.b getstatic output",
                                caller, true);
        return false;
    }
    if (!active_use(c, caller, e))
        return false;
    *out = c->dk_b;
    return true;
}

bool mc_client_chunk_renderer_active_use(mc_client_render_engine_classes *c,
                                         mc_client_graphics_site caller,
                                         mc_client_graphics_error *e) {
    return e != NULL && active_use(c, caller, e);
}

bool mc_client_chunk_statistics_reset(mc_client_render_engine_classes *c,
                                      mc_client_graphics_site caller, mc_client_graphics_error *e) {
    if (e == NULL || !active_use(c, caller, e))
        return false;
    c->dk_b = 0;
    return true;
}

bool mc_client_chunk_statistics_increment(mc_client_render_engine_classes *c,
                                          mc_client_graphics_site caller,
                                          mc_client_graphics_error *e) {
    if (e == NULL || !active_use(c, caller, e))
        return false;
    c->dk_b = mc_java_i32_add(c->dk_b, 1);
    return true;
}

bool mc_client_chunk_statistics_inspect(const mc_client_render_engine_classes *c,
                                        mc_client_render_engine_phase *phase,
                                        const mc_client_tessellator **D, int32_t *updates) {
    if (c == NULL || phase == NULL || D == NULL || updates == NULL)
        return false;
    *phase = c->dk_phase;
    *D = c->dk_D;
    *updates = c->dk_b;
    return true;
}
