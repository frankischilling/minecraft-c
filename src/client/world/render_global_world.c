#include "../../world/runtime/runtime_internal.h"
#include "../application_state.h"
#include "../render/model_graph_internal.h"
#include "../render_engine/internal.h"
#include "../startup/application_startup_internal.h"
#include "mc/block/registry/rendering.h"
#include "mc/block/registry/runtime.h"
#include "mc/world/runtime/entities.h"
#include "mc/world/runtime/runtime.h"
#include "render_global_access.h"
#include "render_global_jo.h"
#include "render_global_recenter.h"
#include "source_chunk_renderer_internal.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

struct mc_render_global_cv {
    mc_render_global_cv *next;
    mc_client_render_blocks fields;
    mc_world_runtime *world;
};

struct mc_client_render_global_world_call {
    mc_client_render_global *owner;
    mc_client_render_global_world_state state;
    mc_world_runtime *argument;
    bool argument_held, owner_held;
    mc_client_render_global_error failure, cleanup;
};

mc_client_render_global_status mc_render_global_source_fail(mc_client_render_global_error *e,
                                                            mc_client_render_global_status status,
                                                            const char *operation, const char *type,
                                                            const char *method, const char *desc,
                                                            unsigned int pc, bool native) {
    e->status = status;
    e->operation = operation;
    e->source_class = type;
    e->source_method = method;
    e->source_descriptor = desc;
    e->source_pc = pc;
    e->native_policy = native;
    return status;
}

void *mc_render_global_allocate(mc_client_render_global *o, size_t bytes,
                                mc_client_graphics_site site, mc_client_render_global_error *e) {
    void *out = mc_client_render_arena_allocate(&o->arena, bytes, site, &e->graphics);
    if (out == NULL) {
        e->child = e->graphics.site;
        mc_render_global_source_fail(e,
                                     e->graphics.status == MC_CLIENT_GRAPHICS_ALLOCATION_FAILED
                                         ? MC_CLIENT_RENDER_GLOBAL_ALLOCATION_FAILED
                                         : MC_CLIENT_RENDER_GLOBAL_LIMIT,
                                     e->graphics.operation, site.class_name, site.method,
                                     site.descriptor, (unsigned int)site.pc, true);
    }
    return out;
}

static bool list_shape(const mc_client_render_list *list) {
    return list != NULL && list->count <= list->capacity && list->capacity <= INT32_MAX &&
           (list->capacity == 0 || list->entries != NULL);
}

bool mc_render_global_pointer_append(mc_client_render_global *o, mc_client_render_list *list,
                                     void *value, mc_client_graphics_site site,
                                     mc_client_render_global_error *e) {
    if (!list_shape(list)) {
        mc_render_global_source_fail(e,
                                     list == NULL ? MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE
                                                  : MC_CLIENT_RENDER_GLOBAL_INVALID_PHASE,
                                     "actual RenderGlobal List.add receiver", site.class_name,
                                     site.method, site.descriptor, (unsigned int)site.pc,
                                     list != NULL);
        return false;
    }
    if (list->count == list->capacity) {
        size_t capacity = list->capacity == 0 ? 8u : list->capacity * 2u;
        if (capacity > INT32_MAX || capacity > SIZE_MAX / sizeof(void *)) {
            mc_render_global_source_fail(e, MC_CLIENT_RENDER_GLOBAL_LIMIT,
                                         "RenderGlobal pointer List native extent", NULL, NULL,
                                         NULL, 0, true);
            return false;
        }
        void **entries = mc_render_global_allocate(o, capacity * sizeof(*entries), site, e);
        if (entries == NULL)
            return false;
        if (list->count != 0)
            memcpy(entries, list->entries, list->count * sizeof(*entries));
        list->entries = entries;
        list->capacity = capacity;
    }
    list->entries[list->count++] = value;
    return true;
}

static bool failure(mc_client_render_global_world_call *c, mc_client_render_global_status status,
                    const char *op, unsigned int pc, bool native) {
    c->state.source_pc = pc;
    mc_render_global_source_fail(&c->failure, status, op, "n", "a", "(Lfd;)V", pc, native);
    return false;
}

typedef struct rebuild_context {
    mc_client_render_global *owner;
    mc_math_classes *math;
    mc_client_render_global_world_state *state;
    mc_client_render_global_error *failure;
} rebuild_context;

static bool rebuild_failure(rebuild_context *c, mc_client_render_global_status status,
                            const char *op, unsigned int pc, bool native) {
    c->state->rebuild_pc = pc;
    mc_render_global_source_fail(c->failure, status, op, "n", "a", "()V", pc, native);
    return false;
}

static bool settings(rebuild_context *c, unsigned int app_pc, unsigned int settings_pc,
                     mc_game_settings_state *out) {
    client_application *app = c->owner->fields.t;
    if (app == NULL)
        return rebuild_failure(c, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE,
                               "actual n.t Minecraft receiver", app_pc, false);
    mc_game_settings *actual = app->settings;
    if (actual == NULL)
        return rebuild_failure(c, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE,
                               "actual Minecraft.z Settings receiver", settings_pc, false);
    if (!mc_game_settings_read_state(actual, out))
        return rebuild_failure(c, MC_CLIENT_RENDER_GLOBAL_BUSY,
                               "actual Settings native state admission", settings_pc, true);
    return true;
}

static bool rebuild(rebuild_context *c) {
    mc_client_render_global *o = c->owner;
    client_application *app = o->fields.t;
    const mc_block *leaves = NULL;
    c->state->rebuild_pc = 0;
    if (app == NULL || mc_registry_block_static_get(app->registry, "L", &leaves,
                                                    &c->failure->registry) != MC_REGISTRY_OK)
        return rebuild_failure(c, MC_CLIENT_RENDER_GLOBAL_GRAPHICS_FAILED, "actual uu.L active use",
                               0, true);
    mc_game_settings_state opts;
    if (!settings(c, 7, 10, &opts))
        return false;
    if (mc_block_render_fancy_leaves_receiver((mc_block *)leaves, opts.values.fancy_graphics) !=
        MC_REGISTRY_OK)
        return rebuild_failure(c, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE,
                               "actual bk.a captured receiver", 13, false);
    if (!settings(c, 21, 24, &opts))
        return false;
    o->fields.H = opts.values.render_distance;
    c->state->last_store_pc = 27;
    if (o->fields.o != NULL) {
        for (size_t index = 0; index < o->o_length; ++index) {
            mc_client_source_dk *old = o->fields.o[index];
            c->state->rebuild_pc = 54;
            if (old == NULL)
                return rebuild_failure(c, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE,
                                       "actual old DK.c receiver", 54, false);
            if (!mc_source_dk_close(old, c->failure))
                return false;
        }
    }
    int32_t distance = mc_java_i32_shl(64, mc_java_i32_sub(3, o->fields.H));
    if (distance > 400)
        distance = 400;
    c->state->distance = distance;
    int32_t sections;
    (void)mc_java_i32_div(distance, 16, &sections);
    o->fields.p = mc_java_i32_add(sections, 1);
    c->state->last_store_pc = 91;
    o->fields.q = 8;
    c->state->last_store_pc = 97;
    o->fields.r = mc_java_i32_add(sections, 1);
    c->state->last_store_pc = 107;
    const int32_t count = mc_java_i32_mul(mc_java_i32_mul(o->fields.p, o->fields.q), o->fields.r);
    if (count < 0 || (size_t)count > SIZE_MAX / sizeof(*o->fields.o))
        return rebuild_failure(c, MC_CLIENT_RENDER_GLOBAL_UNIMPLEMENTED_DEPENDENCY,
                               "actual renderer array length dependency", 125, false);
    const mc_client_graphics_site old_array = {"n", "a", "()V", 125};
    mc_client_source_dk **array =
        mc_render_global_allocate(o, (size_t)count * sizeof(*array), old_array, c->failure);
    if (array == NULL)
        return false;
    o->fields.o = array;
    o->o_length = (size_t)count;
    c->state->last_store_pc = 128;
    array = mc_render_global_allocate(o, (size_t)count * sizeof(*array),
                                      (mc_client_graphics_site){"n", "a", "()V", 146}, c->failure);
    if (array == NULL)
        return false;
    o->fields.n = array;
    o->n_length = (size_t)count;
    c->state->last_store_pc = 149;
    o->fields.B = 0;
    o->fields.C = 0;
    o->fields.D = 0;
    o->fields.E = o->fields.p;
    o->fields.F = o->fields.q;
    o->fields.G = o->fields.r;
    c->state->last_store_pc = 192;
    if (!list_shape(o->fields.m))
        return rebuild_failure(c,
                               o->fields.m == NULL ? MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE
                                                   : MC_CLIENT_RENDER_GLOBAL_INVALID_PHASE,
                               "actual dirty List.size receiver", 204, o->fields.m != NULL);
    for (size_t index = 0; index < o->fields.m->count; ++index) {
        mc_client_source_dk *chunk = o->fields.m->entries[index];
        if (chunk == NULL)
            return rebuild_failure(c, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE,
                                   "actual dirty DK.u receiver", 227, false);
        chunk->fields.u = false;
    }
    o->fields.m->count = 0;
    if (!list_shape(o->fields.a))
        return rebuild_failure(c,
                               o->fields.a == NULL ? MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE
                                                   : MC_CLIENT_RENDER_GLOBAL_INVALID_PHASE,
                               "actual TileEntity List.clear receiver", 249, o->fields.a != NULL);
    o->fields.a->count = 0;
    int32_t offset = 0, ordinal = 0;
    for (c->state->x = 0; c->state->x < o->fields.p; ++c->state->x)
        for (c->state->y = 0; c->state->y < o->fields.q; ++c->state->y)
            for (c->state->z = 0; c->state->z < o->fields.r; ++c->state->z) {
                const int32_t index =
                    ((c->state->z * o->fields.q) + c->state->y) * o->fields.p + c->state->x;
                c->state->rebuild_pc = 312;
                mc_client_source_dk *chunk = mc_source_dk_allocate(o, c->failure);
                c->state->pending_chunk = chunk;
                if (chunk == NULL)
                    return false;
                c->state->rebuild_pc = 347;
                if (!mc_source_dk_construct(
                        chunk, o->fields.k, o->fields.a, mc_java_i32_mul(c->state->x, 16),
                        mc_java_i32_mul(c->state->y, 16), mc_java_i32_mul(c->state->z, 16), 16,
                        mc_java_i32_add(o->fields.s, offset), c->failure))
                    return false;
                ++c->state->constructed_chunks;
                o->fields.o[index] = chunk;
                ++c->state->published_chunks;
                c->state->pending_chunk = NULL;
                if (o->fields.w) {
                    if (!mc_client_ge_int_get(o->fields.v, ordinal, &chunk->fields.z,
                                              (mc_client_graphics_site){"n", "a", "()V", 386},
                                              &c->failure->graphics)) {
                        c->failure->child = c->failure->graphics.site;
                        return rebuild_failure(c, MC_CLIENT_RENDER_GLOBAL_GRAPHICS_FAILED,
                                               c->failure->graphics.operation, 386,
                                               c->failure->graphics.native_policy);
                    }
                }
                chunk->fields.y = false;
                chunk->fields.x = true;
                chunk->fields.o = true;
                chunk->fields.w = ordinal;
                ordinal = mc_java_i32_add(ordinal, 1);
                mc_source_dk_dirty(chunk);
                o->fields.n[index] = chunk;
                c->state->rebuild_pc = 602;
                if (!mc_render_global_pointer_append(
                        o, o->fields.m, chunk, (mc_client_graphics_site){"n", "a", "()V", 602},
                        c->failure))
                    return false;
                ++c->state->queued_chunks;
                offset = mc_java_i32_add(offset, 3);
            }
    if (o->fields.k != NULL) {
        client_application *current = o->fields.t;
        if (current == NULL)
            return rebuild_failure(c, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE,
                                   "actual rebuild App.i receiver", 640, false);
        mc_entity *camera = current->i;
        if (camera != NULL) {
            mc_entity_snapshot fields;
            c->failure->entity.status = mc_entity_get_snapshot(camera, &fields);
            if (c->failure->entity.status != MC_ENTITY_OK)
                return rebuild_failure(c, MC_CLIENT_RENDER_GLOBAL_INVALID_PHASE,
                                       "actual rebuild camera native fields", 653, true);
            int32_t x, y, z;
            c->state->rebuild_pc = 656;
            if (mc_math_classes_ensure(c->math, &c->failure->math) != MC_MATH_CLASS_OK)
                return rebuild_failure(c, MC_CLIENT_RENDER_GLOBAL_GRAPHICS_FAILED,
                                       c->failure->math.operation, 656, true);
            x = mc_math_floor_double(fields.base.x);
            c->state->rebuild_pc = 664;
            if (mc_math_classes_ensure(c->math, &c->failure->math) != MC_MATH_CLASS_OK)
                return rebuild_failure(c, MC_CLIENT_RENDER_GLOBAL_GRAPHICS_FAILED,
                                       c->failure->math.operation, 664, true);
            y = mc_math_floor_double(fields.base.y);
            c->state->rebuild_pc = 672;
            if (mc_math_classes_ensure(c->math, &c->failure->math) != MC_MATH_CLASS_OK)
                return rebuild_failure(c, MC_CLIENT_RENDER_GLOBAL_GRAPHICS_FAILED,
                                       c->failure->math.operation, 672, true);
            z = mc_math_floor_double(fields.base.z);
            c->state->rebuild_pc = 675;
            if (!mc_render_global_recenter_entered(o, x, y, z, c->failure))
                return false;
            c->state->rebuild_pc = 691;
            if (!mc_render_global_sort_at_entered(
                    o, camera, (mc_client_graphics_site){"n", "a", "()V", 691}, c->failure))
                return false;
        }
    }
    o->fields.I = 2;
    c->state->last_store_pc = 696;
    c->state->rebuild_pc = 699;
    c->state->rebuild_returned = true;
    return true;
}

bool mc_render_global_rebuild_entered(mc_client_render_global *owner, mc_math_classes *math,
                                      mc_client_render_global_world_state *state,
                                      mc_client_render_global_error *error) {
    if (error == NULL)
        return false;
    if (owner == NULL || state == NULL || !owner->active) {
        mc_render_global_source_fail(error, MC_CLIENT_RENDER_GLOBAL_INVALID_PHASE,
                                     "rebuild requires the admitted actual N guard", NULL, NULL,
                                     NULL, 0, true);
        return false;
    }
    state->owner = owner;
    state->failure = error;
    state->rebuild_returned = false;
    rebuild_context c = {owner, math, state, error};
    return rebuild(&c);
}

static bool run(mc_client_render_global_world_call *c) {
    mc_client_render_global *o = c->owner;
    c->state.old_world = o->fields.k;
    if (o->access == NULL &&
        mc_render_global_access_create(o, &o->access, &c->failure) != MC_CLIENT_RENDER_GLOBAL_OK)
        return false;
    if (o->fields.k != NULL) {
        c->state.source_pc = 12;
        if (mc_render_global_access_source_remove(o->access, o->fields.k, &c->failure) !=
            MC_CLIENT_RENDER_GLOBAL_OK)
            return false;
        c->state.old_removed = true;
    }
    const uint64_t bits = UINT64_C(0xc0c3878000000000);
    memcpy(&o->fields.f, &bits, sizeof(bits));
    c->state.last_store_pc = 19;
    memcpy(&o->fields.g, &bits, sizeof(bits));
    c->state.last_store_pc = 26;
    memcpy(&o->fields.h, &bits, sizeof(bits));
    c->state.last_store_pc = 33;
    client_application *app = o->fields.t;
    if (app == NULL || app->startup == NULL || app->startup->manager_classes == NULL)
        return failure(c, MC_CLIENT_RENDER_GLOBAL_UNIMPLEMENTED_DEPENDENCY,
                       "actual shared TH class namespace", 36, true);
    mc_client_render_manager *manager = NULL;
    if (mc_client_render_manager_static_get(app->startup->manager_classes, &manager,
                                            &c->failure.manager) != MC_CLIENT_MANAGER_OK)
        return failure(c, MC_CLIENT_RENDER_GLOBAL_GRAPHICS_FAILED, c->failure.manager.operation, 36,
                       true);
    if (o->world_manager == NULL && manager != NULL) {
        if (mc_client_render_manager_retain(manager, &c->failure.manager) != MC_CLIENT_MANAGER_OK)
            return failure(c, MC_CLIENT_RENDER_GLOBAL_BUSY, "captured TH native occurrence", 40,
                           true);
        o->world_manager = manager;
    }
    if (manager != NULL && manager != o->world_manager)
        return failure(c, MC_CLIENT_RENDER_GLOBAL_INVALID_PHASE,
                       "actual shared TH identity changed outside associated namespace", 40, true);
    c->state.source_pc = 40;
    if (mc_client_render_manager_change_world(manager, c->argument, &c->failure.manager) !=
        MC_CLIENT_MANAGER_OK)
        return failure(c,
                       manager == NULL ? MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE
                                       : MC_CLIENT_RENDER_GLOBAL_GRAPHICS_FAILED,
                       c->failure.manager.operation, 40, manager != NULL);
    c->state.manager_returned = true;
    /* Transfer this invocation's captured argument occurrence into the native
     * TH-publication receipt before k's independent hold can fail. */
    mc_world_runtime *old_manager_world = o->manager_world_occurrence;
    o->manager_world_occurrence = c->argument;
    c->argument_held = false;
    mc_world_runtime_binding_release(old_manager_world);
    c->state.source_pc = 45;
    if (c->argument != NULL && !mc_world_runtime_binding_retain(c->argument))
        return failure(c, MC_CLIENT_RENDER_GLOBAL_BUSY, "actual n.k native World occurrence", 45,
                       true);
    mc_world_runtime *previous = o->world_occurrence;
    o->fields.k = c->argument;
    o->world_occurrence = c->argument;
    mc_world_runtime_binding_release(previous);
    c->state.k_stored = true;
    c->state.last_store_pc = 45;
    c->state.source_pc = 49;
    if (!mc_client_render_blocks_active_use(app->render_classes, &c->failure.model))
        return failure(c, MC_CLIENT_RENDER_GLOBAL_GRAPHICS_FAILED, "actual CV active use", 49,
                       true);
    mc_render_global_cv *cv = mc_render_global_allocate(
        o, sizeof(*cv), (mc_client_graphics_site){"n", "a", "(Lfd;)V", 49}, &c->failure);
    if (cv == NULL)
        return false;
    cv->next = o->cv_owners;
    o->cv_owners = cv;
    c->state.source_pc = 54;
    if (c->argument != NULL && !mc_world_runtime_binding_retain(c->argument))
        return failure(c, MC_CLIENT_RENDER_GLOBAL_BUSY, "actual CV.c World native occurrence", 54,
                       true);
    cv->world = c->argument;
    mc_client_render_blocks_construct_fields(&cv->fields, c->argument, true);
    c->state.cv_constructed = true;
    mc_client_render_blocks *displaced = o->fields.u;
    o->fields.u = &cv->fields;
    c->state.u_stored = true;
    c->state.last_store_pc = 57;
    /* No source reference to the displaced CV survives u57. Its borrowed
     * inspected view expires on this mutation; keep only the arena record. */
    for (mc_render_global_cv *old = cv->next; old != NULL; old = old->next) {
        if (&old->fields == displaced && old->world != NULL) {
            mc_world_runtime_binding_release(old->world);
            old->world = NULL;
            old->fields.c = NULL;
            break;
        }
    }
    if (c->argument != NULL) {
        c->state.source_pc = 66;
        if (mc_render_global_access_source_add(o->access, c->argument, &c->failure) !=
            MC_CLIENT_RENDER_GLOBAL_OK)
            return false;
        c->state.registered = true;
        c->state.source_pc = 70;
        if (!mc_render_global_rebuild_entered(o, app->source_math_classes, &c->state,
                                              &c->failure)) {
            c->failure.parent = (mc_client_graphics_site){"n", "a", "(Lfd;)V", 70};
            return false;
        }
    }
    c->state.source_pc = 73;
    c->state.source_returned = true;
    c->failure.source_returned = true;
    return true;
}

mc_client_render_global_status
mc_client_render_global_world_begin(mc_client_render_global *o, mc_world_runtime *world,
                                    mc_client_render_global_world_call **out) {
    if (o == NULL || out == NULL || *out != NULL)
        return MC_CLIENT_RENDER_GLOBAL_INVALID_ARGUMENT;
    if (o->active || o->world_retiring || o->world_call != NULL || o->sky_call != NULL ||
        o->cull_call != NULL || o->update_call != NULL || o->submit_call != NULL ||
        o->entities_call != NULL || o->phase != MC_CLIENT_RENDER_GLOBAL_READY)
        return MC_CLIENT_RENDER_GLOBAL_BUSY;
    if (world != NULL) {
        mc_world_runtime_construction construction;
        client_application *app = o->fields.t;
        if (app == NULL ||
            mc_world_runtime_construction_get(world, &construction) != MC_WORLD_STORAGE_OK ||
            construction.mode != MC_WORLD_RUNTIME_GAME ||
            construction.phase == MC_WORLD_RUNTIME_RETIRED ||
            mc_world_runtime_registry(world) != app->registry ||
            mc_world_runtime_entities(world) != app->entities ||
            mc_world_runtime_random_profile(world) != app->profile)
            return MC_CLIENT_RENDER_GLOBAL_INVALID_ARGUMENT;
    }
    mc_client_render_global_world_call *c = calloc(1, sizeof(*c));
    if (c == NULL)
        return MC_CLIENT_RENDER_GLOBAL_ALLOCATION_FAILED;
    c->owner = o;
    c->argument = world;
    c->state.active = true;
    *out = c;
    o->world_call = c;
    if (mc_client_render_global_retain(o, &c->failure) != MC_CLIENT_RENDER_GLOBAL_OK)
        goto done;
    c->owner_held = true;
    if (world != NULL && !mc_world_runtime_binding_retain(world)) {
        (void)failure(c, MC_CLIENT_RENDER_GLOBAL_BUSY, "native captured World argument", 0, true);
        goto done;
    }
    c->argument_held = world != NULL;
    o->active = true;
    (void)run(c);
    o->active = false;
done:
    c->state.active = false;
    if (c->failure.status != MC_CLIENT_RENDER_GLOBAL_OK)
        o->failure = c->failure;
    return c->failure.status;
}

bool mc_client_render_global_world_inspect(const mc_client_render_global_world_call *c,
                                           mc_client_render_global_world_state *out) {
    if (c == NULL || out == NULL || c->state.active)
        return false;
    *out = c->state;
    out->owner = c->owner;
    out->argument = c->argument;
    out->failure = &c->failure;
    out->cleanup = &c->cleanup;
    return true;
}

mc_client_render_global_status
mc_client_render_global_world_retire(mc_client_render_global_world_call **slot) {
    if (slot == NULL)
        return MC_CLIENT_RENDER_GLOBAL_INVALID_ARGUMENT;
    mc_client_render_global_world_call *c = *slot;
    if (c == NULL)
        return MC_CLIENT_RENDER_GLOBAL_OK;
    if (c->state.active || c->owner->active)
        return MC_CLIENT_RENDER_GLOBAL_BUSY;
    c->state.retiring = true;
    if (c->argument_held) {
        mc_world_runtime_binding_release(c->argument);
        c->argument_held = false;
    }
    mc_client_render_global *o = c->owner;
    o->world_call = NULL;
    if (c->owner_held) {
        mc_client_render_global *held = o;
        if (mc_client_render_global_release(&held, &c->cleanup) != MC_CLIENT_RENDER_GLOBAL_OK) {
            o->world_call = c;
            return c->cleanup.status;
        }
    }
    free(c);
    *slot = NULL;
    return MC_CLIENT_RENDER_GLOBAL_OK;
}

static mc_client_render_global_status world_detach(mc_client_render_global *o,
                                                   mc_client_render_global_error *e,
                                                   bool preserve_manager_world) {
    if (o == NULL || e == NULL)
        return MC_CLIENT_RENDER_GLOBAL_INVALID_ARGUMENT;
    memset(e, 0, sizeof(*e));
    if (preserve_manager_world && o->world_retiring && !o->terminal_preserve_manager_world)
        return mc_render_global_source_fail(e, MC_CLIENT_RENDER_GLOBAL_BUSY,
                                            "RenderGlobal legacy World detach already entered",
                                            NULL, NULL, NULL, 0, true);
    if (o->active || o->holds != 1 || o->world_call != NULL || o->sky_call != NULL ||
        o->cull_call != NULL || o->update_call != NULL || o->submit_call != NULL ||
        o->entities_call != NULL)
        return mc_render_global_source_fail(e, MC_CLIENT_RENDER_GLOBAL_BUSY,
                                            "RenderGlobal World terminal admission", NULL, NULL,
                                            NULL, 0, true);
    mc_render_global_access_state access_state;
    if (o->access != NULL && mc_render_global_access_inspect(o->access, &access_state) &&
        access_state.active_callbacks != 0)
        return mc_render_global_source_fail(e, MC_CLIENT_RENDER_GLOBAL_BUSY,
                                            "RenderGlobal active callback retirement admission",
                                            NULL, NULL, NULL, 0, true);
    if (preserve_manager_world) {
        o->terminal_preserve_manager_world = true;
        o->phase = MC_CLIENT_RENDER_GLOBAL_RETIRING;
    }
    o->world_retiring = true;
    if (preserve_manager_world) {
        const mc_client_render_global_status ended = mc_render_global_entities_end_consumers(o, e);
        if (ended != MC_CLIENT_RENDER_GLOBAL_OK)
            return ended;
    }
    if (o->access != NULL &&
        mc_render_global_access_detach_all(o->access, e) != MC_CLIENT_RENDER_GLOBAL_OK)
        return e->status;
    if (o->manager_world_occurrence != NULL) {
        if (!o->terminal_preserve_manager_world && o->world_manager != NULL &&
            mc_client_render_manager_detach_world(o->world_manager, o->manager_world_occurrence,
                                                  &e->manager) != MC_CLIENT_MANAGER_OK)
            return mc_render_global_source_fail(e, MC_CLIENT_RENDER_GLOBAL_BUSY,
                                                e->manager.operation, NULL, NULL, NULL, 0, true);
        mc_world_runtime_binding_release(o->manager_world_occurrence);
        o->manager_world_occurrence = NULL;
    }
    for (mc_client_source_dk *chunk = o->chunk_owners; chunk != NULL; chunk = chunk->next) {
        if (!o->terminal_preserve_manager_world && o->world_manager != NULL && chunk->world_held &&
            mc_client_render_manager_detach_world(o->world_manager, chunk->fields.a, &e->manager) !=
                MC_CLIENT_MANAGER_OK)
            return mc_render_global_source_fail(e, MC_CLIENT_RENDER_GLOBAL_BUSY,
                                                e->manager.operation, NULL, NULL, NULL, 0, true);
        if (mc_source_dk_native_retire(chunk, e) != MC_CLIENT_RENDER_GLOBAL_OK)
            return e->status == MC_CLIENT_RENDER_GLOBAL_OK ? MC_CLIENT_RENDER_GLOBAL_BUSY
                                                           : e->status;
    }
    for (mc_render_global_cv *cv = o->cv_owners; cv != NULL; cv = cv->next) {
        if (cv->world != NULL) {
            if (!o->terminal_preserve_manager_world && o->world_manager != NULL &&
                mc_client_render_manager_detach_world(o->world_manager, cv->world, &e->manager) !=
                    MC_CLIENT_MANAGER_OK)
                return mc_render_global_source_fail(e, MC_CLIENT_RENDER_GLOBAL_BUSY,
                                                    e->manager.operation, NULL, NULL, NULL, 0,
                                                    true);
            mc_world_runtime_binding_release(cv->world);
            cv->world = NULL;
            cv->fields.c = NULL;
        }
    }
    if (o->world_occurrence != NULL) {
        if (!o->terminal_preserve_manager_world && o->world_manager != NULL &&
            mc_client_render_manager_detach_world(o->world_manager, o->world_occurrence,
                                                  &e->manager) != MC_CLIENT_MANAGER_OK)
            return mc_render_global_source_fail(e, MC_CLIENT_RENDER_GLOBAL_BUSY,
                                                e->manager.operation, NULL, NULL, NULL, 0, true);
        mc_world_runtime_binding_release(o->world_occurrence);
        o->world_occurrence = NULL;
        o->fields.k = NULL;
    }
    if (o->world_manager != NULL &&
        mc_client_render_manager_release(&o->world_manager, &e->manager) != MC_CLIENT_MANAGER_OK)
        return mc_render_global_source_fail(e, MC_CLIENT_RENDER_GLOBAL_BUSY, e->manager.operation,
                                            NULL, NULL, NULL, 0, true);
    return MC_CLIENT_RENDER_GLOBAL_OK;
}

mc_client_render_global_status
mc_client_render_global_world_detach(mc_client_render_global *o, mc_client_render_global_error *e) {
    return world_detach(o, e, false);
}

mc_client_render_global_status
mc_client_render_global_terminal_detach(mc_client_render_global *o,
                                        mc_client_render_global_error *e) {
    return world_detach(o, e, true);
}
