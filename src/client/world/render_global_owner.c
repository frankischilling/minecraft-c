#include "../display/source_surface.h"
#include "../render_engine/context_internal.h"
#include "../render_engine/internal.h"
#include "render_global_internal.h"

#include <emmintrin.h>
#include <stdlib.h>

mc_client_render_global_status mc_render_global_fail(mc_client_render_global_error *e,
                                                     mc_client_render_global_status status,
                                                     const char *operation, const char *method,
                                                     unsigned int pc, bool native_policy) {
    e->status = status;
    e->operation = operation;
    e->source_class = method == NULL ? NULL : "n";
    e->source_method = method;
    e->source_descriptor = method == NULL     ? NULL
                           : method[0] == 'g' ? "()V"
                                              : "(Lnet/minecraft/client/Minecraft;Lji;)V";
    e->source_pc = pc;
    e->native_policy = native_policy;
    return status;
}

bool mc_render_global_graphics(mc_client_render_global *o, unsigned int pc) {
    mc_client_graphics_error *g = &o->failure.graphics;
    mc_render_global_fail(&o->failure,
                          g->status == MC_CLIENT_GRAPHICS_LIMIT ? MC_CLIENT_RENDER_GLOBAL_LIMIT
                          : g->status == MC_CLIENT_GRAPHICS_ALLOCATION_FAILED
                              ? MC_CLIENT_RENDER_GLOBAL_ALLOCATION_FAILED
                              : MC_CLIENT_RENDER_GLOBAL_GRAPHICS_FAILED,
                          g->operation, o->failure.source_method, pc, g->native_policy);
    return false;
}

bool mc_render_global_display(mc_client_render_global *o, unsigned int pc) {
    mc_render_global_fail(&o->failure,
                          o->failure.display.status == MC_CLIENT_DISPLAY_BUSY
                              ? MC_CLIENT_RENDER_GLOBAL_BUSY
                              : MC_CLIENT_RENDER_GLOBAL_GRAPHICS_FAILED,
                          o->failure.display.operation, o->failure.source_method, pc, true);
    return false;
}

bool mc_render_global_fp(mc_client_render_global *o, const char *method, unsigned int pc) {
    if ((_mm_getcsr() & 0xffc0u) == 0x1f80u)
        return true;
    mc_render_global_fail(&o->failure, MC_CLIENT_RENDER_GLOBAL_UNSUPPORTED_FP,
                          "RenderGlobal reached strict FP prerequisite", method, pc, true);
    return false;
}

mc_client_render_global_status mc_client_render_global_allocate(
    mc_client_render_engine_classes *classes, const mc_client_render_engine_options *options,
    mc_client_render_global **out, mc_client_render_global_error *error) {
    mc_client_render_global_error e = {0};
    if (classes == NULL || out == NULL || classes->resource_retirement_started) {
        mc_render_global_fail(&e, MC_CLIENT_RENDER_GLOBAL_INVALID_ARGUMENT,
                              "native RenderGlobal cold context/output", NULL, 0, true);
        goto done;
    }
    mc_client_render_global *o = calloc(1, sizeof(*o));
    if (o == NULL) {
        mc_render_global_fail(&e, MC_CLIENT_RENDER_GLOBAL_ALLOCATION_FAILED,
                              "native actual RenderGlobal owner", NULL, 0, true);
        goto done;
    }
    e.classes = mc_client_render_engine_classes_retain(classes);
    if (e.classes != MC_CLIENT_RENDER_ENGINE_OK || classes->scene_lease_count == SIZE_MAX) {
        if (e.classes == MC_CLIENT_RENDER_ENGINE_OK)
            (void)mc_client_render_engine_classes_release(classes); /* Proven nonfinal hold. */
        free(o);
        mc_render_global_fail(&e, MC_CLIENT_RENDER_GLOBAL_LIMIT,
                              "native RenderGlobal context occurrence", NULL, 0, true);
        goto done;
    }
    ++classes->scene_lease_count;
    o->scene_guard = true;
    o->classes = classes;
    o->holds = 1;
    o->arena.options = options == NULL
                           ? (mc_client_render_engine_options){8u * 1024u * 1024u, SIZE_MAX}
                           : *options;
    *out = o;
done:
    if (error != NULL)
        *error = e;
    return e.status;
}

mc_client_render_global_status
mc_client_render_global_retain(mc_client_render_global *o, mc_client_render_global_error *error) {
    mc_client_render_global_error e = {0};
    if (o == NULL)
        mc_render_global_fail(&e, MC_CLIENT_RENDER_GLOBAL_INVALID_ARGUMENT,
                              "native RenderGlobal retain receiver", NULL, 0, true);
    else if (o->active || o->phase == MC_CLIENT_RENDER_GLOBAL_RETIRING)
        mc_render_global_fail(&e, MC_CLIENT_RENDER_GLOBAL_BUSY,
                              "native RenderGlobal entered/retiring occurrence", NULL, 0, true);
    else if (o->holds == SIZE_MAX)
        mc_render_global_fail(&e, MC_CLIENT_RENDER_GLOBAL_LIMIT, "native RenderGlobal hold limit",
                              NULL, 0, true);
    else
        ++o->holds;
    if (error != NULL)
        *error = e;
    return e.status;
}

mc_client_render_global_status
mc_client_render_global_release(mc_client_render_global **slot,
                                mc_client_render_global_error *error) {
    mc_client_render_global_error e = {0};
    if (slot == NULL) {
        mc_render_global_fail(&e, MC_CLIENT_RENDER_GLOBAL_INVALID_ARGUMENT,
                              "native RenderGlobal release output", NULL, 0, true);
        goto done;
    }
    mc_client_render_global *o = *slot;
    if (o == NULL)
        goto done;
    if (o->active) {
        mc_render_global_fail(&e, MC_CLIENT_RENDER_GLOBAL_BUSY,
                              "native RenderGlobal source operation active", NULL, 0, true);
        goto done;
    }
    if (o->holds > 1) {
        --o->holds;
        *slot = NULL;
        goto done;
    }
    o->phase = MC_CLIENT_RENDER_GLOBAL_RETIRING;
    o->active = true;
    if (!mc_client_display_query_names_retire(&o->queries, &e.cleanup_display)) {
        mc_render_global_fail(&e,
                              e.cleanup_display.status == MC_CLIENT_DISPLAY_BUSY
                                  ? MC_CLIENT_RENDER_GLOBAL_BUSY
                                  : MC_CLIENT_RENDER_GLOBAL_GRAPHICS_FAILED,
                              "native actual query-name retirement", NULL, 0, true);
        goto retained;
    }
    if (o->open_list != 0) {
        if (!mc_client_display_lists_abort(o->display, o->open_list, &e.cleanup_display)) {
            mc_render_global_fail(&e,
                                  e.cleanup_display.status == MC_CLIENT_DISPLAY_BUSY
                                      ? MC_CLIENT_RENDER_GLOBAL_BUSY
                                      : MC_CLIENT_RENDER_GLOBAL_GRAPHICS_FAILED,
                                  "native actual partial-list retirement", NULL, 0, true);
            goto retained;
        }
        o->open_list = 0;
    }
    if (o->scene_guard) {
        --o->classes->scene_lease_count;
        o->scene_guard = false;
    }
    if (!mc_client_render_context_unbind_empty(o->classes, &e.cleanup_graphics)) {
        mc_render_global_fail(&e, MC_CLIENT_RENDER_GLOBAL_BUSY,
                              "native scene empty-context retirement", NULL, 0, true);
        goto retained;
    }
    e.classes = mc_client_render_engine_classes_release(o->classes);
    if (e.classes != MC_CLIENT_RENDER_ENGINE_OK) {
        mc_render_global_fail(&e, MC_CLIENT_RENDER_GLOBAL_BUSY,
                              "native scene final class occurrence", NULL, 0, true);
        goto retained;
    }
    o->classes = NULL;
    mc_client_render_arena_free(&o->arena);
    free(o);
    *slot = NULL;
    goto done;
retained:
    o->retirement = e;
    o->active = false;
done:
    if (error != NULL)
        *error = e;
    return e.status;
}

bool mc_client_render_global_inspect(const mc_client_render_global *o,
                                     mc_client_render_global_state *out) {
    if (o == NULL || out == NULL)
        return false;
    *out = (mc_client_render_global_state){
        .phase = o->phase,
        .fields = o->fields,
        .classes = o->classes,
        .holds = o->holds,
        .arena_bytes = o->arena.bytes,
        .arena_allocations = o->arena.allocations,
        .last_store_pc = o->last_store_pc,
        .last_constructor_pc = o->last_constructor_pc,
        .star_pc = o->star_pc,
        .pending_T = o->pending_T,
        .pending_batch = o->pending_batch,
        .constructed_batches = o->constructed_batches,
        .published_batches = o->published_batches,
        .source_returned = o->source_returned,
        .queries_issued = o->queries_issued,
        .list_open = o->open_list != 0,
        .matrix_pushed = o->matrix_pushed,
        .star_random_created = o->star_random != NULL,
        .star_random = o->star_random == NULL ? (mc_java_random){0} : *o->star_random,
        .star_attempts = o->star_attempts,
        .accepted_stars = o->accepted_stars,
        .star_vertices = o->star_vertices,
        .top_quads = o->top_quads,
        .bottom_quads = o->bottom_quads,
        .failure = &o->failure,
        .retirement = &o->retirement};
    return true;
}

bool mc_client_render_batch_inspect(const mc_client_render_batch *batch,
                                    mc_client_render_batch_state *out) {
    if (batch == NULL || out == NULL)
        return false;
    *out = batch->state;
    return true;
}
