#include "mc/world/runtime/respawn.h"

#include "../info/tick_source_internal.h"
#include "entity_tick_internal.h"
#include "mc/world/runtime/blocks.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

void mc_world_respawn_error_init(mc_world_respawn_error *e) {
    if (e != NULL)
        memset(e, 0, offsetof(mc_world_respawn_error, child));
}

bool mc_world_respawn_error_retire(mc_world_respawn_error *e) {
    if (e == NULL)
        return false;
    if (e->child_kind == MC_WORLD_RESPAWN_CHILD_DEAD &&
        !mc_entity_effect_error_retire(&e->child.dead))
        return false;
    if (e->child_kind == MC_WORLD_RESPAWN_CHILD_LIST &&
        !mc_world_entity_tick_error_retire(&e->child.list))
        return false;
    mc_world_respawn_error_init(e);
    return true;
}

static bool failed(mc_world_respawn_error *e, mc_world_storage_status status, const char *operation,
                   unsigned int pc) {
    e->status = status;
    e->operation = operation;
    e->bytecode = pc;
    e->cause = (mc_world_storage_error){status, operation, 0};
    return false;
}

static bool entered(mc_world_runtime *w, mc_world_respawn_error *e) {
    if (w == NULL)
        return failed(e, MC_WORLD_STORAGE_INVALID_ARGUMENT, "Respawn null World receiver", 0);
    if (w->construction.mode != MC_WORLD_RUNTIME_GAME ||
        w->construction.phase == MC_WORLD_RUNTIME_RETIRED || w->classes == NULL ||
        w->classes->side != MC_WORLD_INFO_CLIENT)
        return failed(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Respawn actual client GAME World", 0);
    const mc_world_storage_status status = mc_world_runtime_call_enter(w, &e->cause);
    if (status != MC_WORLD_STORAGE_OK) {
        e->status = status;
        e->operation = e->cause.operation;
        return false;
    }
    return true;
}

static mc_world_storage_status finished(mc_world_runtime *w, bool held, mc_world_respawn_error *e,
                                        bool private_error) {
    const mc_world_storage_status status = e->status;
    if (held) {
        w->last_error = status == MC_WORLD_STORAGE_OK ? (mc_world_storage_error){0} : e->cause;
        mc_world_runtime_call_leave(w);
    }
    /* Actual failed K work belongs to its Entity, including with NULL output. */
    if (private_error)
        free(e);
    return status;
}

static bool info_get(mc_world_runtime *w, unsigned int axis, int32_t *out,
                     mc_world_respawn_error *e, unsigned int pc) {
    e->bytecode = pc;
    mc_world_info *info = w->fields.x;
    e->info = mc_world_info_source_spawn_get(info, axis, out);
    if (e->info == MC_WORLD_INFO_OK)
        return true;
    e->source_null = info == NULL;
    return failed(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Respawn current Info getter receiver",
                  pc);
}

static bool info_set(mc_world_runtime *w, unsigned int axis, int32_t value,
                     mc_world_respawn_error *e, unsigned int pc) {
    e->bytecode = pc;
    mc_world_info *info = w->fields.x;
    e->info = mc_world_info_source_spawn_axis_set(info, axis, value);
    if (e->info == MC_WORLD_INFO_OK)
        return true;
    e->source_null = info == NULL;
    return failed(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Respawn current Info setter receiver",
                  pc);
}

mc_world_storage_status mc_world_runtime_respawn_spawn_new(mc_world_runtime *w,
                                                           mc_world_respawn_position **out,
                                                           mc_world_respawn_error *output) {
    mc_world_respawn_error *e = output == NULL ? malloc(sizeof(*e)) : output;
    if (e == NULL)
        return MC_WORLD_STORAGE_ALLOCATION_FAILED;
    mc_world_respawn_error_init(e);
    e->method = "u()Lbr;";
    bool held = false;
    if (out == NULL)
        (void)failed(e, MC_WORLD_STORAGE_INVALID_ARGUMENT, "Respawn position output", 0);
    else if ((held = entered(w, e))) {
        mc_world_respawn_position *position = malloc(sizeof(*position));
        if (position == NULL)
            (void)failed(e, MC_WORLD_STORAGE_ALLOCATION_FAILED, "Respawn NEW BlockPos owner", 0);
        else {
            int32_t x, y, z;
            if (info_get(w, 0, &x, e, 8) && info_get(w, 1, &y, e, 15) &&
                info_get(w, 2, &z, e, 22)) {
                *position = (mc_world_respawn_position){x, y, z};
                *out = position;
                e->bytecode = 28;
            } else
                free(position);
        }
    }
    return finished(w, held, e, output == NULL);
}

void mc_world_respawn_position_free(mc_world_respawn_position *position) {
    free(position);
}

static bool draw(mc_world_runtime *w, int32_t *out, mc_world_respawn_error *e, unsigned int pc) {
    e->bytecode = pc;
    mc_java_random *random = w->fields.r;
    if (random == NULL) {
        e->source_null = true;
        return failed(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Respawn null current Random", pc);
    }
    (void)mc_java_random_next_bounded(random, 8, out);
    ++e->random_draws;
    return true;
}

static bool repair(mc_world_runtime *w, size_t max_queries, mc_world_respawn_error *e) {
    int32_t y;
    if (!info_get(w, 1, &y, e, 4) || (y <= 0 && !info_set(w, 1, 64, e, 16)) ||
        !info_get(w, 0, &e->x, e, 23) || !info_get(w, 2, &e->z, e, 31))
        return false;
    for (;;) {
        if (e->queries == max_queries)
            return failed(e, MC_WORLD_STORAGE_LIMIT, "Respawn pending spawn query ceiling", 38);
        ++e->queries;
        int32_t block;
        e->bytecode = 38;
        const mc_world_storage_status status =
            mc_world_runtime_top_block(w, e->x, e->z, &block, &e->cause);
        if (status != MC_WORLD_STORAGE_OK) {
            e->status = status;
            e->operation = e->cause.operation;
            return false;
        }
        if (block != 0)
            break;
        int32_t a, b;
        if (!draw(w, &a, e, 51) || !draw(w, &b, e, 60))
            return false;
        e->x = mc_java_i32_add(e->x, mc_java_i32_sub(a, b));
        if (!draw(w, &a, e, 73) || !draw(w, &b, e, 82))
            return false;
        e->z = mc_java_i32_add(e->z, mc_java_i32_sub(a, b));
    }
    if (!info_set(w, 0, e->x, e, 96) || !info_set(w, 2, e->z, e, 104))
        return false;
    e->bytecode = 107;
    return true;
}

mc_world_storage_status mc_world_runtime_respawn_repair_spawn(mc_world_runtime *w,
                                                              size_t max_queries,
                                                              mc_world_respawn_error *output) {
    mc_world_respawn_error *e = output == NULL ? malloc(sizeof(*e)) : output;
    if (e == NULL)
        return MC_WORLD_STORAGE_ALLOCATION_FAILED;
    mc_world_respawn_error_init(e);
    e->method = "d()V";
    const bool held = entered(w, e);
    if (held)
        (void)repair(w, max_queries, e);
    return finished(w, held, e, output == NULL);
}

static void at(wt_scope *s, unsigned int pc) {
    wt_at(s, "v()V", pc);
    s->error.loop_bytecode = pc;
}

static bool old_chunk(wt_scope *s, mc_entity *p, unsigned int read_pc, unsigned int present_pc,
                      unsigned int get_pc, unsigned int remove_pc) {
    at(s, read_pc);
    if (p == NULL)
        return wt_fail(s, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Respawn null removed Entity");
    const int32_t x = p->base.bG, z = p->base.bI;
    if (!p->base.bF)
        return true;
    bool present;
    at(s, present_pc);
    if (!wt_present(s, x, z, present_pc, &present))
        return false;
    if (!present)
        return true;
    mc_chunk_state *state;
    at(s, get_pc);
    if (!wt_get(s, x, z, get_pc, &state))
        return false;
    at(s, remove_pc);
    return wt_chunk_remove(s, state, p, true, 0, remove_pc);
}

static bool cleanup(wt_scope *s) {
    size_t size;
    mc_entity *p = NULL;
    at(s, 8);
    if (!wt_list_remove_all(s, s->world->fields.b, s->world->fields.D, 8))
        return false;
    for (int32_t i = 0;; i = mc_java_i32_add(i, 1)) {
        s->error.deferred_index = i;
        at(s, 21);
        if (!wt_list_size(s, s->world->fields.D, 21, &size))
            return false;
        if ((size_t)i >= size)
            break;
        if (!wt_work(s, 34) || !wt_list_get(s, s->world->fields.D, i, 34, 39, &p))
            return false;
        const bool ok = old_chunk(s, p, 44, 65, 75, 79);
        mc_entity_release(p);
        p = NULL;
        if (!ok)
            return false;
    }
    for (int32_t i = 0;; i = mc_java_i32_add(i, 1)) {
        s->error.deferred_index = i;
        at(s, 95);
        if (!wt_list_size(s, s->world->fields.D, 95, &size))
            return false;
        if ((size_t)i >= size)
            break;
        if (!wt_work(s, 109) || !wt_list_get(s, s->world->fields.D, i, 109, 114, &p))
            return false;
        at(s, 117);
        const bool ok = wt_observe_removed(s, p, 117);
        mc_entity_release(p);
        p = NULL;
        if (!ok)
            return false;
    }
    at(s, 130);
    if (!wt_list_clear(s, s->world->fields.D, 130))
        return false;
    int32_t i = 0;
    for (;;) {
        s->error.loaded_index = i;
        at(s, 142);
        if (!wt_list_size(s, s->world->fields.b, 142, &size))
            return false;
        if ((size_t)i >= size)
            break;
        if (!wt_work(s, 155) || !wt_list_get(s, s->world->fields.b, i, 155, 160, &p))
            return false;
        at(s, 165);
        bool ok = true, skip = false;
        if (p == NULL)
            ok = wt_fail(s, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Respawn loaded null Entity");
        else if (p->base.aH != NULL) {
            mc_entity *mount = NULL;
            if (!wt_relationship(s, p->base.aH, p, 175, &mount))
                ok = false;
            else {
                skip = !mount->base.be && mount->base.aG == p;
                if (!skip) {
                    at(s, 197);
                    (void)mc_entity_mount_passenger_store(mount, NULL);
                    at(s, 202);
                    (void)mc_entity_mount_vehicle_store(p, NULL);
                }
                mc_entity_release(mount);
            }
        }
        if (ok && !skip && p->base.be) {
            ok = old_chunk(s, p, 213, 234, 244, 248);
            if (ok) {
                mc_world_reference_list *list = s->world->fields.b;
                const int32_t index = i;
                i = mc_java_i32_sub(i, 1);
                s->error.loaded_index = i;
                at(s, 259);
                ok = wt_list_remove(s, list, index, 259);
                if (ok) {
                    at(s, 267);
                    ok = wt_observe_removed(s, p, 267);
                }
            }
        }
        mc_entity_release(p);
        p = NULL;
        if (!ok)
            return false;
        i = mc_java_i32_add(i, 1);
    }
    return true;
}

static wt_scope *scope_new(mc_world_runtime *w, const mc_world_entity_tick_context *context,
                           const mc_world_entity_tick_limits *limits,
                           mc_world_entity_tick_error *e) {
    mc_world_entity_tick_error_init(e);
    if (w == NULL || w->construction.mode != MC_WORLD_RUNTIME_GAME ||
        w->construction.phase == MC_WORLD_RUNTIME_RETIRED || w->entity_containers_retired ||
        w->classes == NULL || w->classes->side != MC_WORLD_INFO_CLIENT) {
        e->status = MC_WORLD_STORAGE_UNSUPPORTED_STATE;
        e->operation = "Respawn cleanup actual live client GAME World";
        goto reject;
    }
    if (context != NULL &&
        (context->relationship_actor_count > (size_t)INT32_MAX ||
         (context->relationship_actor_count != 0 && context->relationship_actors == NULL))) {
        e->status = MC_WORLD_STORAGE_INVALID_ARGUMENT;
        e->operation = "Respawn relationship scope extent";
        goto reject;
    }
    e->status = mc_world_runtime_call_enter(w, &e->cause);
    if (e->status != MC_WORLD_STORAGE_OK) {
        e->operation = e->cause.operation;
        return NULL;
    }
    wt_scope *s = calloc(1, sizeof(*s));
    if (s == NULL) {
        mc_world_runtime_call_leave(w);
        e->status = MC_WORLD_STORAGE_ALLOCATION_FAILED;
        e->operation = "Respawn cleanup native scratch allocation";
        goto reject;
    }
    s->world = w;
    s->context = context;
    s->has_error_output = true;
    mc_world_entity_tick_error_init(&s->error);
    s->error.max_loop_work =
        limits == NULL ? MC_WORLD_ENTITY_TICK_DEFAULT_LOOP_WORK : limits->max_loop_work;
    s->error.max_passenger_calls =
        limits == NULL ? MC_WORLD_ENTITY_TICK_DEFAULT_PASSENGER_CALLS : limits->max_passenger_calls;
    s->error.max_temporary_bytes =
        limits == NULL ? MC_WORLD_ENTITY_TICK_DEFAULT_TEMPORARY_BYTES : limits->max_temporary_bytes;
    return s;
reject:
    e->cause = (mc_world_storage_error){e->status, e->operation, 0};
    return NULL;
}

static mc_world_storage_status scope_finish(wt_scope *s, mc_world_entity_tick_error *e) {
    while (s->parent_count != 0)
        mc_entity_release(s->parents[--s->parent_count]);
    wt_free(s, s->parents, s->parent_capacity * sizeof(*s->parents));
    const mc_world_storage_status status = s->error.status;
    s->world->last_error =
        status == MC_WORLD_STORAGE_OK ? (mc_world_storage_error){0} : s->error.cause;
    if (status == MC_WORLD_STORAGE_OK)
        mc_world_entity_tick_error_init(&s->error);
    *e = s->error;
    mc_world_runtime_call_leave(s->world);
    free(s);
    return status;
}

mc_world_storage_status
mc_world_runtime_respawn_cleanup(mc_world_runtime *w, const mc_world_entity_tick_context *context,
                                 const mc_world_entity_tick_limits *limits,
                                 mc_world_entity_tick_error *output) {
    mc_world_entity_tick_error *e = output == NULL ? malloc(sizeof(*e)) : output;
    if (e == NULL)
        return MC_WORLD_STORAGE_ALLOCATION_FAILED;
    wt_scope *s = scope_new(w, context, limits, e);
    mc_world_storage_status status = e->status;
    if (s != NULL) {
        (void)cleanup(s);
        status = scope_finish(s, e);
    }
    if (output == NULL)
        free(e);
    return status;
}

static bool remove_player(wt_scope *s, mc_entity *p, mc_world_respawn_error *e) {
    mc_world_reference_list *list = s->world->fields.d;
    size_t size;
    wt_at(s, "e(Lsn;)V", 46);
    if (!wt_list_size(s, list, 46, &size))
        return false;
    for (size_t i = 0; i < size; ++i) {
        mc_entity *candidate = NULL;
        if (!wt_work(s, 46) || !wt_list_get(s, list, (int32_t)i, 46, 46, &candidate))
            return false;
        const bool equal = candidate != NULL && candidate->base.id == p->base.id;
        mc_entity_release(candidate);
        if (equal)
            return wt_list_remove(s, list, (int32_t)i, 46);
    }
    e->bytecode = 51;
    return true;
}

mc_world_storage_status mc_world_runtime_respawn_remove(mc_world_runtime *w, mc_entity *p,
                                                        mc_registry_classes *registry,
                                                        mc_math_classes *math,
                                                        mc_entity *const *actors, size_t count,
                                                        mc_world_respawn_error *output) {
    mc_world_respawn_error *e = output == NULL ? malloc(sizeof(*e)) : output;
    if (e == NULL)
        return MC_WORLD_STORAGE_ALLOCATION_FAILED;
    mc_world_respawn_error_init(e);
    e->method = "e(Lsn;)V";
    mc_world_entity_tick_context context = {.relationship_actors = actors,
                                            .relationship_actor_count = count};
    e->child_kind = MC_WORLD_RESPAWN_CHILD_LIST;
    wt_scope *s = scope_new(w, &context, NULL, &e->child.list);
    if (s == NULL) {
        (void)failed(e, e->child.list.status, e->child.list.operation, 0);
        goto done;
    }
    if (p == NULL) {
        e->source_null = true;
        (void)failed(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Respawn null Entity aG receiver", 1);
        goto finish;
    }
    if (p->classes != w->classes->entities) {
        (void)failed(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Respawn actual Entity owner", 1);
        goto finish;
    }
    if (!wt_hold(s, p, 1)) {
        (void)failed(e, s->error.status, s->error.operation, 1);
        goto finish;
    }
    if (p->base.aG != NULL) {
        mc_entity *passenger = NULL;
        if (!wt_relationship(s, p->base.aG, p, 11, &passenger)) {
            e->child_kind = MC_WORLD_RESPAWN_CHILD_LIST;
            e->child.list = s->error;
            (void)failed(e, s->error.status, s->error.operation, 12);
            goto release;
        }
        e->child_kind = MC_WORLD_RESPAWN_CHILD_MOUNT;
        const mc_entity_status status =
            mc_entity_mount(passenger, NULL, actors, count, &e->child.mount);
        mc_entity_release(passenger);
        if (status != MC_ENTITY_OK) {
            (void)failed(e, wt_entity_status(status), e->child.mount.operation, 12);
            goto release;
        }
    }
    if (p->base.aH != NULL) {
        e->child_kind = MC_WORLD_RESPAWN_CHILD_MOUNT;
        const mc_entity_status status = mc_entity_mount(p, NULL, actors, count, &e->child.mount);
        if (status != MC_ENTITY_OK) {
            (void)failed(e, wt_entity_status(status), e->child.mount.operation, 24);
            goto release;
        }
    }
    e->child_kind = MC_WORLD_RESPAWN_CHILD_DEAD;
    if (mc_entity_effect_dead_with_context(p, registry, math, &e->child.dead) != MC_ENTITY_OK) {
        (void)failed(e, wt_entity_status(e->child.dead.status), e->child.dead.operation, 28);
        goto release;
    }
    e->child_kind = MC_WORLD_RESPAWN_CHILD_NONE;
    if (mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER) ||
        mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_PLAYER)) {
        if (!remove_player(s, p, e)) {
            e->child_kind = MC_WORLD_RESPAWN_CHILD_LIST;
            e->child.list = s->error;
            (void)failed(e, s->error.status, s->error.operation, 46);
            goto release;
        }
        e->child_kind = MC_WORLD_RESPAWN_CHILD_SLEEP;
        if (mc_world_runtime_update_player_sleeping(w, &e->child.sleep) != MC_WORLD_STORAGE_OK) {
            (void)failed(e, e->child.sleep.status, e->child.sleep.operation, 53);
            goto release;
        }
        e->child_kind = MC_WORLD_RESPAWN_CHILD_NONE;
    }
    e->bytecode = 56;
release:
    mc_entity_release(p);
finish:
    while (s->parent_count != 0)
        mc_entity_release(s->parents[--s->parent_count]);
    wt_free(s, s->parents, s->parent_capacity * sizeof(*s->parents));
    mc_world_runtime_call_leave(w);
    free(s);
done:
    if (w != NULL)
        w->last_error = e->status == MC_WORLD_STORAGE_OK ? (mc_world_storage_error){0} : e->cause;
    const mc_world_storage_status result = e->status;
    if (output == NULL)
        free(e);
    return result;
}
