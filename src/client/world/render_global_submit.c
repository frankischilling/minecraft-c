#include "source_terrain_internal.h"

#include "../../entity/player/tick_math_internal.h"
#include "../application_state.h"
#include "../render_engine/internal.h"
#include "../render_engine/lighting.h"
#include "mc/core/math/math.h"
#include "render_global_jo.h"
#include "render_global_recenter.h"

#include <limits.h>
#include <stdlib.h>

static bool length(mc_client_render_global_submit_call *c, bool sorted, unsigned int pc,
                   int32_t *out) {
    mc_client_render_global *n = c->state.owner;
    if ((sorted ? n->fields.n : n->fields.o) == NULL)
        return mc_terrain_fail(c, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE,
                               "actual N ChunkRenderer array length", pc, false);
    const size_t value = sorted ? n->n_length : n->o_length;
    if (value > INT32_MAX)
        return mc_terrain_fail(c, MC_CLIENT_RENDER_GLOBAL_LIMIT, "N ChunkRenderer native length",
                               pc, true);
    *out = (int32_t)value;
    return true;
}

static mc_client_render_list *dirty(mc_client_render_global_submit_call *c, unsigned int pc) {
    mc_client_render_list *m = c->state.owner->fields.m;
    if (m == NULL) {
        mc_terrain_fail(c, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE, "actual N dirty List receiver",
                        pc, false);
        return NULL;
    }
    if (m->count > m->capacity || m->capacity > INT32_MAX ||
        (m->capacity != 0 && m->entries == NULL)) {
        mc_terrain_fail(c, MC_CLIENT_RENDER_GLOBAL_INVALID_PHASE, "N dirty List native extent", pc,
                        true);
        return NULL;
    }
    return m;
}

static bool polls(mc_client_render_global_submit_call *c) {
    mc_client_render_global *n = c->state.owner;
    for (int i = 0; i < 10; ++i) {
        int32_t count, next;
        if (!length(c, false, 21, &count))
            return false;
        if (!mc_java_i32_rem(mc_java_i32_add(n->fields.R, 1), count, &next))
            return mc_terrain_fail(c, MC_CLIENT_RENDER_GLOBAL_SOURCE_ARITHMETIC,
                                   "actual N dirty-poll remainder by zero", 22, false);
        n->fields.R = next;
        mc_terrain_store(c, 23);
        ++c->state.dirty_polls;
        mc_client_source_dk *dk;
        if (!mc_terrain_array(c, false, n->fields.R, 34, &dk) || !mc_terrain_dk_receiver(c, dk, 39))
            return false;
        if (!dk->fields.u)
            continue;
        mc_client_render_list *m = dirty(c, 51);
        if (m == NULL)
            return false;
        bool contains = false;
        for (size_t j = 0; j < m->count; ++j)
            if (m->entries[j] == dk) {
                contains = true;
                break;
            }
        if (contains)
            continue;
        m = dirty(c, 65);
        if (m == NULL)
            return false;
        mc_terrain_pc(c, 65);
        if (!mc_terrain_child(
                c,
                mc_render_global_pointer_append(
                    n, m, dk, (mc_client_graphics_site){"n", "a", "(Lls;ID)I", 65}, &c->failure),
                65))
            return false;
        ++c->state.queued_chunks;
    }
    return true;
}

static bool coordinates(mc_client_render_global_submit_call *c, mc_entity_snapshot *v) {
    if (c->state.camera == NULL)
        return mc_terrain_fail(c, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE, "actual terrain camera",
                               128, false);
    c->failure.entity.status = mc_entity_get_snapshot(c->state.camera, v);
    if (c->failure.entity.status != MC_ENTITY_OK)
        return mc_terrain_fail(c, MC_CLIENT_RENDER_GLOBAL_INVALID_PHASE,
                               "N canonical camera storage", 128, true);
    if (!mc_terrain_fp(c, 139))
        return false;
    c->state.render_x = at_add(v->base.bl, at_mul(at_sub(v->base.x, v->base.bl), c->state.partial));
    c->state.render_y = at_add(v->base.bm, at_mul(at_sub(v->base.y, v->base.bm), c->state.partial));
    c->state.render_z = at_add(v->base.bn, at_mul(at_sub(v->base.z, v->base.bn), c->state.partial));
    return true;
}

static bool recenter(mc_client_render_global_submit_call *c, const mc_entity_snapshot *v) {
    mc_client_render_global *n = c->state.owner;
    if (!mc_terrain_fp(c, 189))
        return false;
    const double dx = at_sub(v->base.x, n->fields.f);
    const double dy = at_sub(v->base.y, n->fields.g);
    const double dz = at_sub(v->base.z, n->fields.h);
    const double distance = at_add(at_add(at_mul(dx, dx), at_mul(dy, dy)), at_mul(dz, dz));
    if (!(distance > 16.0))
        return true;
    n->fields.f = v->base.x;
    mc_terrain_store(c, 243);
    n->fields.g = v->base.y;
    mc_terrain_store(c, 251);
    n->fields.h = v->base.z;
    mc_terrain_store(c, 259);
    if (!mc_terrain_math(c, 267))
        return false;
    const int32_t x = mc_math_floor_double(v->base.x);
    if (!mc_terrain_math(c, 274))
        return false;
    const int32_t y = mc_math_floor_double(v->base.y);
    if (!mc_terrain_math(c, 281))
        return false;
    const int32_t z = mc_math_floor_double(v->base.z);
    mc_terrain_pc(c, 284);
    if (!mc_terrain_child(c, mc_render_global_recenter_entered(n, x, y, z, &c->failure), 284))
        return false;
    mc_terrain_pc(c, 299);
    return mc_terrain_child(
        c,
        mc_render_global_sort_at_entered(
            n, c->state.camera, (mc_client_graphics_site){"n", "a", "(Lls;ID)I", 299}, &c->failure),
        299);
}

static bool body(mc_client_render_global_submit_call *c) {
    mc_client_render_global *n = c->state.owner;
    if (!mc_terrain_push(c, "n", "a", "(Lls;ID)I") || !polls(c))
        return false;
    mc_game_settings_state settings;
    if (!mc_terrain_settings(c, 78, 81, 84, &settings))
        return false;
    if (settings.values.render_distance != n->fields.H) {
        mc_terrain_pc(c, 95);
        c->state.rebuild = &c->rebuild;
        if (!mc_terrain_push(c, "n", "a", "()V"))
            return false;
        const bool rebuilt =
            mc_render_global_rebuild_entered(n, c->state.math, &c->rebuild, &c->failure);
        if (!mc_terrain_child(c, rebuilt, c->rebuild.rebuild_pc))
            return false;
        mc_terrain_pop(c);
    }
    if (c->state.pass == 0) {
        n->fields.M = 0;
        mc_terrain_store(c, 104);
        n->fields.N = 0;
        mc_terrain_store(c, 109);
        n->fields.O = 0;
        mc_terrain_store(c, 114);
        n->fields.P = 0;
        mc_terrain_store(c, 119);
        n->fields.Q = 0;
        mc_terrain_store(c, 124);
    }
    mc_entity_snapshot camera;
    if (!coordinates(c, &camera) || !recenter(c, &camera))
        return false;
    mc_terrain_pc(c, 302);
    if (!mc_terrain_graphics(
            c,
            mc_client_lighting_disable(n->classes, n->display,
                                       (mc_client_graphics_site){"n", "a", "(Lls;ID)I", 302},
                                       &c->failure.graphics),
            302))
        return false;
    bool occlusion = false;
    if (n->fields.w) {
        if (!mc_terrain_settings(c, 316, 319, 322, &settings))
            return false;
        if (settings.values.advanced_opengl) {
            if (!mc_terrain_settings(c, 329, 332, 335, &settings))
                return false;
            if (!settings.values.anaglyph && c->state.pass == 0)
                occlusion = true;
        }
    }
    c->state.occlusion = occlusion;
    int32_t result;
    if (occlusion) {
        if (!mc_terrain_occlusion(c, &result))
            return false;
    } else {
        int32_t count;
        if (!length(c, true, 885, &count) || !mc_terrain_subset(c, 0, count, 888, &result))
            return false;
    }
    c->state.result = result;
    c->state.returned = true;
    c->failure.source_returned = true;
    mc_terrain_pc(c, 896);
    mc_terrain_pop(c);
    return true;
}

void mc_terrain_freeze_failure(mc_client_render_global_submit_call *c) {
    if (c->depth == 0)
        return;
    const mc_client_graphics_site outer = c->frames[0];
    c->failure.source_class = outer.class_name;
    c->failure.source_method = outer.method;
    c->failure.source_descriptor = outer.descriptor;
    c->failure.source_pc = (unsigned int)outer.pc;
    c->failure.source_returned = false;
    c->state.source_pc = (unsigned int)outer.pc;
    if (c->depth > 1)
        c->failure.child = c->frames[1];
}

mc_client_render_global_status
mc_client_render_global_submit_begin(mc_client_render_global *n, mc_entity *camera, int32_t pass,
                                     double partial, mc_math_classes *math,
                                     mc_client_render_global_submit_call **out,
                                     mc_client_render_global_error *error) {
    mc_client_render_global_error local = {0};
    if (error == NULL)
        error = &local;
    *error = (mc_client_render_global_error){0};
    if (n == NULL || out == NULL || *out != NULL)
        return mc_render_global_source_fail(error, MC_CLIENT_RENDER_GLOBAL_INVALID_ARGUMENT,
                                            "N submission native owner/output", NULL, NULL, NULL, 0,
                                            true);
    mc_client_render_global_world_state attachment;
    const bool blocked = n->world_call != NULL &&
                         (!mc_client_render_global_world_inspect(n->world_call, &attachment) ||
                          attachment.active || !attachment.source_returned);
    if (n->active || n->world_retiring || blocked || n->sky_call != NULL || n->cull_call != NULL ||
        n->update_call != NULL || n->submit_call != NULL ||
        n->phase == MC_CLIENT_RENDER_GLOBAL_CONSTRUCTING ||
        n->phase == MC_CLIENT_RENDER_GLOBAL_RETIRING)
        return mc_render_global_source_fail(error, MC_CLIENT_RENDER_GLOBAL_BUSY,
                                            "N submission entered/pending/retiring owner", NULL,
                                            NULL, NULL, 0, true);
    mc_client_render_global_submit_call *c = calloc(1, sizeof(*c));
    if (c == NULL)
        return mc_render_global_source_fail(error, MC_CLIENT_RENDER_GLOBAL_ALLOCATION_FAILED,
                                            "N submission native call allocation", NULL, NULL, NULL,
                                            0, true);
    if (camera != NULL && mc_entity_retain(camera) != MC_ENTITY_OK) {
        free(c);
        return mc_render_global_source_fail(error, MC_CLIENT_RENDER_GLOBAL_BUSY,
                                            "N submission camera occurrence", NULL, NULL, NULL, 0,
                                            true);
    }
    c->camera_held = camera != NULL;
    if (math != NULL && mc_math_classes_retain(math, &c->math_error) != MC_MATH_CLASS_OK) {
        if (c->camera_held)
            mc_entity_release(camera);
        *error = (mc_client_render_global_error){.math = c->math_error};
        free(c);
        return mc_render_global_source_fail(error, MC_CLIENT_RENDER_GLOBAL_BUSY,
                                            "N submission Math namespace occurrence", NULL, NULL,
                                            NULL, 0, true);
    }
    c->math_held = math != NULL;
    if (mc_client_render_global_retain(n, error) != MC_CLIENT_RENDER_GLOBAL_OK) {
        if (c->math_held)
            (void)mc_math_classes_release(&math, NULL);
        if (c->camera_held)
            mc_entity_release(camera);
        free(c);
        return error->status;
    }
    c->state = (mc_client_render_global_submit_state){.owner = n,
                                                      .camera = camera,
                                                      .math = math,
                                                      .pass = pass,
                                                      .partial = partial,
                                                      .failure = &c->failure,
                                                      .cleanup = &c->cleanup,
                                                      .frames = c->frames};
    n->submit_call = c;
    *out = c;
    c->state.active = n->active = true;
    const bool ok = body(c);
    if (!ok && c->failure.status == MC_CLIENT_RENDER_GLOBAL_OK)
        mc_terrain_fail(c, MC_CLIENT_RENDER_GLOBAL_INVALID_PHASE,
                        "N source child stopped without a failure", c->state.source_pc, true);
    if (!ok)
        mc_terrain_freeze_failure(c);
    c->state.active = n->active = false;
    *error = c->failure;
    return error->status;
}

bool mc_client_render_global_submit_inspect(const mc_client_render_global_submit_call *c,
                                            mc_client_render_global_submit_state *out) {
    if (c == NULL || out == NULL || c->state.active)
        return false;
    *out = c->state;
    out->frames = c->frames;
    out->frame_count = c->depth;
    return true;
}

mc_client_render_global_status
mc_client_render_global_submit_retire(mc_client_render_global_submit_call **slot,
                                      mc_client_render_global_error *error) {
    mc_client_render_global_error local = {0};
    if (error == NULL)
        error = &local;
    *error = (mc_client_render_global_error){0};
    if (slot == NULL)
        return mc_render_global_source_fail(error, MC_CLIENT_RENDER_GLOBAL_INVALID_ARGUMENT,
                                            "N submission native retirement slot", NULL, NULL, NULL,
                                            0, true);
    mc_client_render_global_submit_call *c = *slot;
    if (c == NULL)
        return MC_CLIENT_RENDER_GLOBAL_OK;
    if (c->state.active || c->state.owner->active || c->state.owner->submit_call != c)
        return mc_render_global_source_fail(error, MC_CLIENT_RENDER_GLOBAL_BUSY,
                                            "N submission active/changed retirement", NULL, NULL,
                                            NULL, 0, true);
    c->state.retiring = true;
    if (!mc_client_display_active_query_retire(&c->query, &c->cleanup.cleanup_display)) {
        mc_render_global_source_fail(&c->cleanup, MC_CLIENT_RENDER_GLOBAL_CLEANUP_FAILED,
                                     c->cleanup.cleanup_display.operation, NULL, NULL, NULL, 0,
                                     true);
        *error = c->cleanup;
        return error->status;
    }
    c->state.query_active = false;
    if (c->math_held) {
        if (mc_math_classes_release(&c->state.math, &c->math_cleanup) != MC_MATH_CLASS_OK) {
            c->cleanup.math = c->math_cleanup;
            mc_render_global_source_fail(&c->cleanup, MC_CLIENT_RENDER_GLOBAL_CLEANUP_FAILED,
                                         "N submission Math occurrence retirement", NULL, NULL,
                                         NULL, 0, true);
            *error = c->cleanup;
            return error->status;
        }
        c->math_held = false;
    }
    mc_client_render_global *owner = c->state.owner;
    owner->submit_call = NULL;
    if (mc_client_render_global_release(&c->state.owner, &c->cleanup) !=
        MC_CLIENT_RENDER_GLOBAL_OK) {
        owner->submit_call = c;
        *error = c->cleanup;
        return error->status;
    }
    if (c->batch_camera_held)
        mc_entity_release(c->state.batch_camera);
    if (c->camera_held)
        mc_entity_release(c->state.camera);
    free(c);
    *slot = NULL;
    return MC_CLIENT_RENDER_GLOBAL_OK;
}
