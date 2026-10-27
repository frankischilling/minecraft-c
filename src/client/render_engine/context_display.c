#include "../display/source_surface.h"
#include "context_internal.h"
#include "internal.h"

bool mc_client_render_context_bind(mc_client_render_engine_classes *c, mc_client_display *display,
                                   mc_client_graphics_site site, mc_client_graphics_error *e) {
    if (display == NULL || (c->bound_display != NULL && c->bound_display != display)) {
        mc_client_graphics_fail(e, MC_CLIENT_GRAPHICS_INVALID_ARGUMENT,
                                "actual graphics Display context admission", site, true);
        return false;
    }
    if (c->context_lease != NULL)
        return true;
    if (!mc_client_display_context_acquire(display, &c->context_lease, &e->display)) {
        mc_client_graphics_fail(e,
                                e->display.status == MC_CLIENT_DISPLAY_BUSY
                                    ? MC_CLIENT_GRAPHICS_BUSY
                                    : MC_CLIENT_GRAPHICS_DISPLAY_FAILED,
                                "actual graphics context native hold", site, true);
        return false;
    }
    c->bound_display = display;
    return true;
}

bool mc_client_render_context_unbind_empty(mc_client_render_engine_classes *c,
                                           mc_client_graphics_error *e) {
    if (c->native_resource_count != 0 || c->command_range_count != 0 || c->font_lease_count != 0)
        return true;
    if (!mc_client_display_context_release(&c->context_lease, &e->display)) {
        mc_client_graphics_fail(e, MC_CLIENT_GRAPHICS_BUSY, "graphics empty-context hold release",
                                (mc_client_graphics_site){0}, true);
        return false;
    }
    c->bound_display = NULL;
    return true;
}
