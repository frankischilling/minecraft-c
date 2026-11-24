#include "source_terrain_internal.h"

#include "../../entity/entity_internal.h"
#include "../../entity/player/tick_math_internal.h"
#include "../application_state.h"
#include "../render_engine/internal.h"
#include "render_batch_internal.h"

#include <limits.h>
#include <string.h>

static mc_client_render_list *selected(mc_client_render_global_submit_call *c, unsigned int pc) {
    mc_client_render_list *list = c->state.owner->fields.S;
    if (list == NULL) {
        mc_terrain_fail(c, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE, "actual N selected List", pc,
                        false);
        return NULL;
    }
    if (list->count > list->capacity || list->capacity > INT32_MAX ||
        (list->capacity != 0 && list->entries == NULL)) {
        mc_terrain_fail(c, MC_CLIENT_RENDER_GLOBAL_INVALID_PHASE, "N selected List native extent",
                        pc, true);
        return NULL;
    }
    return list;
}

static bool pass_empty(mc_client_render_global_submit_call *c, mc_client_source_dk *dk,
                       unsigned int field_pc, unsigned int index_pc, bool *out) {
    if (!mc_terrain_dk_receiver(c, dk, field_pc))
        return false;
    const bool *p = dk->fields.p;
    if (p == NULL)
        return mc_terrain_fail(c, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE, "actual DK pass array",
                               index_pc, false);
    if (c->state.pass < 0 || c->state.pass >= 2)
        return mc_terrain_fail(c, MC_CLIENT_RENDER_GLOBAL_SOURCE_INDEX,
                               "actual DK pass array index", index_pc, false);
    *out = p[c->state.pass];
    return true;
}

static bool batch(mc_client_render_global_submit_call *c, int32_t index, unsigned int pc,
                  mc_client_render_batch **out) {
    mc_client_render_global *n = c->state.owner;
    if (n->fields.T == NULL)
        return mc_terrain_fail(c, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE, "actual N TG array", pc,
                               false);
    if (n->t_length > INT32_MAX)
        return mc_terrain_fail(c, MC_CLIENT_RENDER_GLOBAL_LIMIT, "N TG native extent", pc, true);
    if (index < 0 || (size_t)index >= n->t_length)
        return mc_terrain_fail(c, MC_CLIENT_RENDER_GLOBAL_SOURCE_INDEX, "actual N TG array index",
                               pc, false);
    *out = n->fields.T[index];
    return true;
}

static bool batch_length(mc_client_render_global_submit_call *c, unsigned int pc, int32_t *out) {
    mc_client_render_global *n = c->state.owner;
    if (n->fields.T == NULL)
        return mc_terrain_fail(c, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE,
                               "actual N TG array length", pc, false);
    if (n->t_length > INT32_MAX)
        return mc_terrain_fail(c, MC_CLIENT_RENDER_GLOBAL_LIMIT, "N TG native extent", pc, true);
    *out = (int32_t)n->t_length;
    return true;
}

static void increment(mc_client_render_global_submit_call *c, int32_t *field, unsigned int pc) {
    const uint32_t next = (uint32_t)*field + 1u;
    memcpy(field, &next, sizeof(next));
    mc_terrain_store(c, pc);
}

static bool choose(mc_client_render_global_submit_call *c, int32_t index, int32_t *count,
                   mc_client_source_dk *const *chunks, mc_client_render_list *s) {
    mc_client_render_global *n = c->state.owner;
    const bool record_statistics = c->state.pass == 0;
    mc_client_source_dk *dk;
    bool empty;
    if (record_statistics)
        increment(c, &n->fields.M, 32);
    dk = chunks == NULL ? NULL : chunks[index];
    if ((chunks == NULL || (dk != NULL && dk->owner != n)) &&
        !mc_terrain_array(c, true, index, record_statistics ? 41 : 138, &dk))
        return false;
    if (!pass_empty(c, dk, record_statistics ? 42 : 139, record_statistics ? 46 : 143, &empty))
        return false;
    /* No callback runs between classification and selection. */
    if (empty) {
        if (record_statistics)
            increment(c, &n->fields.Q, 57);
        return true;
    }
    if (!dk->fields.o) {
        if (record_statistics)
            increment(c, &n->fields.N, 83);
        return true;
    }
    if (n->fields.w && !dk->fields.x) {
        if (record_statistics)
            increment(c, &n->fields.O, 116);
        return true;
    }
    if (record_statistics)
        increment(c, &n->fields.P, 129);
    int32_t id;
    if (!mc_source_dk_render_list(dk, c->state.pass, &id, &c->failure))
        return mc_terrain_child(c, false, 188);
    if (id < 0)
        return true;
    mc_terrain_pc(c, 209);
    if (s->count < s->capacity) {
        ++s->mod_count;
        s->entries[s->count++] = dk;
    } else if (!mc_render_global_pointer_append(
                   n, s, dk, (mc_client_graphics_site){"n", "a", "(IIID)I", 209}, &c->failure)) {
        return mc_terrain_child(c, false, 209);
    }
    *count = mc_java_i32_add(*count, 1);
    ++c->state.selected_chunks;
    return true;
}

static bool camera(mc_client_render_global_submit_call *c, double *x, double *y, double *z) {
    client_application *app = c->state.owner->fields.t;
    if (app == NULL)
        return mc_terrain_fail(c, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE,
                               "actual N batching App.i receiver", 228, false);
    mc_entity *current = app->i;
    if (current != NULL && mc_entity_retain(current) != MC_ENTITY_OK)
        return mc_terrain_fail(c, MC_CLIENT_RENDER_GLOBAL_BUSY,
                               "N batching camera native occurrence", 235, true);
    if (c->batch_camera_held)
        mc_entity_release(c->state.batch_camera);
    c->state.batch_camera = current;
    c->batch_camera_held = current != NULL;
    if (current == NULL)
        return mc_terrain_fail(c, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE,
                               "actual N batching camera", 235, false);
    const mc_entity_base_fields *fields = mc_entity_runtime_fields(current);
    c->failure.entity.status = MC_ENTITY_OK;
    if (!mc_terrain_fp(c, 248))
        return false;
    *x = at_add(fields->bl, at_mul(at_sub(fields->x, fields->bl), c->state.partial));
    *y = at_add(fields->bm, at_mul(at_sub(fields->y, fields->bm), c->state.partial));
    *z = at_add(fields->bn, at_mul(at_sub(fields->z, fields->bn), c->state.partial));
    return true;
}

bool mc_terrain_replay(mc_client_render_global_submit_call *c) {
    mc_client_render_global *n = c->state.owner;
    if (!mc_terrain_push(c, "n", "a", "(ID)V"))
        return false;
    for (int32_t i = 0;; i = mc_java_i32_add(i, 1)) {
        int32_t length;
        if (!batch_length(c, 9, &length))
            return false;
        if (i >= length)
            break;
        mc_client_render_batch *b;
        if (!batch(c, i, 19, &b))
            return false;
        if (b == NULL)
            return mc_terrain_fail(c, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE,
                                   "actual TG draw receiver", 20, false);
        mc_terrain_pc(c, 20);
        if (!mc_terrain_child(c, mc_render_batch_draw(n, b, &c->failure), 20))
            return false;
        ++c->state.drawn_batches;
    }
    mc_terrain_pc(c, 29);
    mc_terrain_pop(c);
    return true;
}

static bool draw_all(mc_client_render_global_submit_call *c) {
    mc_terrain_pc(c, 484);
    return mc_terrain_replay(c);
}

bool mc_terrain_subset(mc_client_render_global_submit_call *c, int32_t start, int32_t end,
                       unsigned int caller_pc, int32_t *result) {
    mc_client_render_global *n = c->state.owner;
    mc_terrain_pc(c, caller_pc);
    if (!mc_terrain_push(c, "n", "a", "(IIID)I"))
        return false;
    ++c->state.subsets;
    mc_client_render_list *s = selected(c, 4);
    if (s == NULL)
        return false;
    mc_client_render_list_clear(s);
    int32_t count = 0;
    /* Selection cannot replace S or call back, even if its backing grows.
     * Malformed chunk spans still report failures after the same counter stores. */
    mc_client_source_dk *const *chunks =
        start >= 0 && end >= start && n->n_length <= INT32_MAX && (size_t)end <= n->n_length
            ? n->fields.n
            : NULL;
    for (int32_t i = start; i < end; ++i) {
        c->state.index = i;
        if (!choose(c, i, &count, chunks, s))
            return false;
    }
    double x, y, z;
    if (!camera(c, &x, &y, &z))
        return false;
    for (int32_t i = 0;; i = mc_java_i32_add(i, 1)) {
        int32_t length;
        if (!batch_length(c, 311, &length))
            return false;
        if (i >= length)
            break;
        mc_client_render_batch *b;
        if (!batch(c, i, 321, &b))
            return false;
        if (b == NULL)
            return mc_terrain_fail(c, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE,
                                   "actual TG reset receiver", 322, false);
        mc_terrain_pc(c, 322);
        if (!mc_terrain_child(c, mc_render_batch_reset(n, b, &c->failure), 322))
            return false;
    }
    int32_t used = 0;
    for (size_t i = 0;; ++i) {
        /* A preceding append can draw. Recheck the live list before reading this entry. */
        s = selected(c, 340);
        if (s == NULL)
            return false;
        if (i >= s->count)
            break;
        mc_client_source_dk *dk = s->entries[i];
        int32_t chosen = -1;
        for (int32_t j = 0; j < used; ++j) {
            mc_client_render_batch *b;
            if (!batch(c, j, 383, &b))
                return false;
            if (dk == NULL)
                return mc_terrain_dk_receiver(c, dk, 386);
            if (b == NULL)
                return mc_terrain_fail(c, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE,
                                       "actual TG match receiver", 399, false);
            mc_terrain_pc(c, 399);
            if (mc_render_batch_match_fields(b, dk->fields.i, dk->fields.j, dk->fields.k))
                chosen = j; /* The original search deliberately has no break. */
        }
        if (chosen < 0) {
            chosen = used;
            used = mc_java_i32_add(used, 1);
            mc_client_render_batch *b;
            if (!batch(c, chosen, 433, &b) || !mc_terrain_dk_receiver(c, dk, 436))
                return false;
            if (b == NULL)
                return mc_terrain_fail(c, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE,
                                       "actual TG configure receiver", 455, false);
            mc_terrain_pc(c, 455);
            if (!mc_terrain_child(c,
                                  mc_render_batch_configure(n, b, dk->fields.i, dk->fields.j,
                                                            dk->fields.k, x, y, z, &c->failure),
                                  455))
                return false;
            ++c->state.configured_batches;
        }
        mc_client_render_batch *b;
        int32_t id;
        if (!batch(c, chosen, 464, &b) || !mc_terrain_dk_receiver(c, dk, 468))
            return false;
        mc_terrain_pc(c, 468);
        if (!mc_terrain_child(c, mc_source_dk_render_list(dk, c->state.pass, &id, &c->failure),
                              468))
            return false;
        if (b == NULL)
            return mc_terrain_fail(c, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE,
                                   "actual TG append receiver", 471, false);
        mc_terrain_pc(c, 471);
        if (!mc_terrain_child(c, mc_render_batch_append(n, b, id, &c->failure), 471))
            return false;
        ++c->state.appended_lists;
    }
    if (!draw_all(c))
        return false;
    mc_terrain_pc(c, 489);
    mc_terrain_pop(c);
    *result = count;
    return true;
}
