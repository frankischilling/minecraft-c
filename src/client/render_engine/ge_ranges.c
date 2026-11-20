#include "../display/source_surface.h"
#include "context_internal.h"
#include "ge.h"
#include "internal.h"
#include <stdlib.h>

struct mc_client_command_range {
    mc_client_command_range *next;
    mc_client_display_list_range *resource;
    int32_t base, count;
};

static mc_client_graphics_site range_site(int32_t pc) {
    return (mc_client_graphics_site){"ge", "a", "(I)I", pc};
}

static bool entered(mc_client_render_engine_classes *c, mc_client_graphics_site caller,
                    mc_client_graphics_error *e) {
    *e = (mc_client_graphics_error){0};
    if (c == NULL || c->resource_retirement_started) {
        mc_client_graphics_fail(
            e, c == NULL ? MC_CLIENT_GRAPHICS_INVALID_ARGUMENT : MC_CLIENT_GRAPHICS_INVALID_PHASE,
            "native ge source namespace admission", caller, true);
        return false;
    }
    if (c->active_source_calls != 0 || c->active_resource_calls != 0) {
        mc_client_graphics_fail(e, MC_CLIENT_GRAPHICS_BUSY, "ge source/resource operation active",
                                caller, true);
        return false;
    }
    if (mc_client_render_engine_classes_retain(c) != MC_CLIENT_RENDER_ENGINE_OK) {
        mc_client_graphics_fail(e, MC_CLIENT_GRAPHICS_LIMIT, "ge per-call class hold", caller,
                                true);
        return false;
    }
    ++c->active_source_calls;
    e->entry = e->caller = caller;
    return true;
}

static bool leave(mc_client_render_engine_classes *c, bool ok, mc_client_graphics_error *e) {
    mc_client_graphics_error cleanup = {0};
    const bool released = mc_client_render_context_unbind_empty(c, &cleanup);
    --c->active_source_calls;
    (void)mc_client_render_engine_classes_release(c);
    if (!released) {
        if (ok)
            *e = cleanup;
        else {
            e->cleanup_display = cleanup.display;
            e->cleanup_failed = true;
        }
    }
    return ok && released;
}

bool mc_client_ge_generate_lists(mc_client_render_engine_classes *c, mc_client_display *display,
                                 int32_t count, mc_client_graphics_site caller, int32_t *out,
                                 mc_client_graphics_error *e) {
    if (out == NULL) {
        mc_client_graphics_fail(e, MC_CLIENT_GRAPHICS_INVALID_ARGUMENT, "native ge range output",
                                caller, true);
        return false;
    }
    if (!entered(c, caller, e))
        return false;
    bool ok = false;
    if (!mc_client_ge_initialize(c, caller, e))
        goto done;
    if (c->command_range_count >= c->limits.max_command_ranges) {
        mc_client_graphics_fail(e, MC_CLIENT_GRAPHICS_LIMIT, "native ge retained range bound",
                                range_site(1), true);
        goto done;
    }
    if (!mc_client_render_context_bind(c, display, range_site(1), e))
        goto done;
    mc_client_command_range *range = calloc(1, sizeof(*range));
    if (range == NULL) {
        mc_client_graphics_fail(e, MC_CLIENT_GRAPHICS_ALLOCATION_FAILED, "native ge range record",
                                range_site(1), true);
        goto done;
    }
    if (!mc_client_display_lists_generate(display, count, c->limits.max_list_commands,
                                          &range->resource, &range->base, &e->display)) {
        free(range);
        mc_client_graphics_fail(e,
                                e->display.status == MC_CLIENT_DISPLAY_UNSUPPORTED_FP
                                    ? MC_CLIENT_GRAPHICS_UNSUPPORTED_FP
                                : e->display.status == MC_CLIENT_DISPLAY_UNSUPPORTED_GL
                                    ? MC_CLIENT_GRAPHICS_UNIMPLEMENTED_DEPENDENCY
                                : e->display.status == MC_CLIENT_DISPLAY_LIMIT
                                    ? MC_CLIENT_GRAPHICS_LIMIT
                                : e->display.status == MC_CLIENT_DISPLAY_ALLOCATION_FAILED
                                    ? MC_CLIENT_GRAPHICS_ALLOCATION_FAILED
                                    : MC_CLIENT_GRAPHICS_DISPLAY_FAILED,
                                e->display.operation, range_site(1), true);
        goto done;
    }
    range->count = count;
    range->next = c->command_ranges;
    c->command_ranges = range;
    ++c->command_range_count;
    e->generated = true;
    e->actual_index = range->base;
    e->retained_count = c->command_range_count;
    if (!mc_client_ge_append(c->a, range->base, c->limits.max_source_list_entries, range_site(12),
                             e))
        goto done;
    e->first_append = true;
    if (!mc_client_ge_append(c->a, count, c->limits.max_source_list_entries, range_site(25), e))
        goto done;
    e->second_append = true;
    *out = range->base;
    ok = true;
done:
    return leave(c, ok, e);
}

bool mc_client_ge_call_lists(mc_client_render_engine_classes *c, mc_client_display *display,
                             const mc_client_direct_int_buffer *buffer,
                             mc_client_graphics_site caller, mc_client_graphics_error *e) {
    if (!entered(c, caller, e))
        return false;
    bool ok = false;
    if (buffer == NULL) {
        mc_client_graphics_fail(e, MC_CLIENT_GRAPHICS_NULL_REFERENCE,
                                "actual glCallLists IntBuffer", caller, false);
        goto done;
    }
    if (!mc_client_render_context_bind(c, display, caller, e))
        goto done;
    /* Actual LWJGL cursor semantics: only address/remaining are observed.
     * No relative get, position increment, flip or clear occurs here. */
    const int32_t *ids =
        (const int32_t *)(const void *)(buffer->bytes->data + buffer->position * sizeof(int32_t));
    if (!mc_client_display_lists_call(display, ids, buffer->limit - buffer->position,
                                      &e->display)) {
        mc_client_graphics_fail(e,
                                e->display.status == MC_CLIENT_DISPLAY_UNSUPPORTED_FP
                                    ? MC_CLIENT_GRAPHICS_UNSUPPORTED_FP
                                : e->display.status == MC_CLIENT_DISPLAY_UNSUPPORTED_GL
                                    ? MC_CLIENT_GRAPHICS_UNIMPLEMENTED_DEPENDENCY
                                    : MC_CLIENT_GRAPHICS_DISPLAY_FAILED,
                                e->display.operation, caller, true);
        goto done;
    }
    ok = true;
done:
    return leave(c, ok, e);
}

bool mc_client_ge_retire_ranges(mc_client_render_engine_classes *c, mc_client_graphics_error *e) {
    bool ok = true;
    mc_client_command_range **link = &c->command_ranges;
    while (*link != NULL) {
        mc_client_command_range *range = *link;
        mc_client_display_error cause = {0};
        const bool released = mc_client_display_lists_retire(&range->resource, &cause);
        if (!released && ok) {
            e->display = cause;
            mc_client_graphics_fail(e, MC_CLIENT_GRAPHICS_BUSY, "native command range retirement",
                                    (mc_client_graphics_site){0}, true);
            ok = false;
        }
        if (range->resource == NULL) {
            *link = range->next;
            --c->command_range_count;
            free(range);
        } else
            link = &range->next;
    }
    return ok;
}
