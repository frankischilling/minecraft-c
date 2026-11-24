#include "lighting.h"

#include "../display/fog_surface.h"
#include "../display/source_surface.h"
#include "../display/startup_surface.h"
#include "internal.h"

bool mc_client_lighting_active_use(mc_client_render_engine_classes *c,
                                   mc_client_graphics_site caller, mc_client_graphics_error *e) {
    if (e == NULL)
        return false;
    *e = (mc_client_graphics_error){.entry = caller, .caller = caller};
    if (c == NULL || c->resource_retirement_started) {
        mc_client_graphics_fail(
            e, c == NULL ? MC_CLIENT_GRAPHICS_INVALID_ARGUMENT : MC_CLIENT_GRAPHICS_INVALID_PHASE,
            "u actual class namespace native admission", caller, true);
        return false;
    }
    if (c->active_source_calls != 0 || c->active_resource_calls != 0 ||
        c->u_phase == MC_CLIENT_RENDER_ENGINE_CONSTRUCTING) {
        mc_client_graphics_fail(e, MC_CLIENT_GRAPHICS_BUSY, "u actual class operation active",
                                caller, true);
        return false;
    }
    if (c->u_phase == MC_CLIENT_RENDER_ENGINE_READY)
        return true;
    if (c->u_phase == MC_CLIENT_RENDER_ENGINE_FAILED) {
        *e = c->u_failure;
        e->entry = e->caller = caller;
        e->dependency_class_cause = e->class_cause;
        e->class_cause = (mc_client_graphics_cause){c->u_failure.status, c->u_failure.operation,
                                                    c->u_failure.caller, c->u_failure.site,
                                                    c->u_failure.native_policy};
        mc_client_graphics_fail(e, MC_CLIENT_GRAPHICS_CLASS_FAILED,
                                "previous actual u class initialization failure", caller, false);
        return false;
    }
    c->u_phase = MC_CLIENT_RENDER_ENGINE_CONSTRUCTING;
    const mc_client_graphics_site child = {"u", "<clinit>", "()V", 2};
    mc_client_direct_float_buffer *buffer = NULL;
    if (!mc_client_ge_e(c, &c->arena, 16, child, &buffer, e)) {
        e->entry = caller;
        e->parent = child;
        c->u_phase = MC_CLIENT_RENDER_ENGINE_FAILED;
        c->u_failure = *e;
        return false;
    }
    c->u_a = buffer; /* Actual u.<clinit> putstatic5, borrowed class-arena storage. */
    c->u_phase = MC_CLIENT_RENDER_ENGINE_READY;
    *e = (mc_client_graphics_error){.entry = caller, .caller = caller};
    return true;
}

bool mc_client_lighting_inspect(const mc_client_render_engine_classes *c,
                                mc_client_lighting_state *out) {
    if (c == NULL || out == NULL || c->u_phase == MC_CLIENT_RENDER_ENGINE_CONSTRUCTING)
        return false;
    *out = (mc_client_lighting_state){c->u_phase, c->u_a, &c->u_failure};
    return true;
}

static bool display_result(mc_client_graphics_error *e, bool ok, int32_t pc) {
    if (ok) {
        ++e->retained_count;
        return true;
    }
    mc_client_graphics_fail(
        e,
        e->display.status == MC_CLIENT_DISPLAY_BUSY             ? MC_CLIENT_GRAPHICS_BUSY
        : e->display.status == MC_CLIENT_DISPLAY_UNSUPPORTED_FP ? MC_CLIENT_GRAPHICS_UNSUPPORTED_FP
                                                                : MC_CLIENT_GRAPHICS_DISPLAY_FAILED,
        e->display.operation, (mc_client_graphics_site){"u", "a", "()V", pc}, true);
    return false;
}

bool mc_client_lighting_disable(mc_client_render_engine_classes *c, mc_client_display *display,
                                mc_client_graphics_site caller, mc_client_graphics_error *e) {
    if (!mc_client_lighting_active_use(c, caller, e))
        return false;
    if (mc_client_render_engine_classes_retain(c) != MC_CLIENT_RENDER_ENGINE_OK) {
        mc_client_graphics_fail(e, MC_CLIENT_GRAPHICS_LIMIT, "u call native class occurrence",
                                caller, true);
        return false;
    }
    ++c->active_source_calls;
    const bool ok =
        display_result(e,
                       mc_client_display_source_disable_unlit_feature(
                           display, MC_CLIENT_SOURCE_LIGHTING, &e->display),
                       3) &&
        display_result(e, mc_client_display_source_light_enable(display, 0, false, &e->display),
                       9) &&
        display_result(e, mc_client_display_source_light_enable(display, 1, false, &e->display),
                       15) &&
        display_result(
            e, mc_client_display_source_color_material_enable(display, false, &e->display), 21);
    --c->active_source_calls;
    (void)mc_client_render_engine_classes_release(c);
    return ok;
}
