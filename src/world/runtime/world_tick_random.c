#include "mc/world/runtime/random_tick.h"

#include "../../block/registry/registry_internal.h"
#include "../../block/registry/runtime_internal.h"
#include "../../entity/entity_internal.h"
#include "chunk_provider_internal.h"
#include "construction_internal.h"
#include "entity_tick_math_internal.h"
#include "mc/block/registry/runtime.h"
#include "mc/core/java/java.h"
#include "mc/core/math/math.h"
#include "mc/entity/lightning/construction.h"
#include "mc/world/runtime/entity_tick.h"
#include "mc/world/runtime/environment.h"
#include "mutation/mutation_internal.h"
#include "runtime_internal.h"
#include "world_tick_random_internal.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

struct mc_world_random_tick_call {
    mc_world_random_tick_state state;
    mc_world_random_tick_error error;
    mc_world_runtime_acquisition_error acquisition;
    mc_world_chunk_tick_error chunk_error;
    mc_world_chunk_tick_error chunk_cleanup;
    mc_world_query_error query;
    mc_registry_error registry;
    mc_world_chunk_manager_error biome;
    mc_world_movement_error rain;
    mc_world_mutation_error mutation;
    mc_lightning_error lightning_error;
    mc_world_entity_error publication;
    mc_entity_error allocation;
    mc_world_chunk_tick_capture *chunk;
    bool world_held, math_held;
};

static mc_world_storage_status rejected(mc_world_random_tick_error *error,
                                        mc_world_storage_status status, const char *operation) {
    if (error != NULL) {
        memset(error, 0, sizeof(*error));
        error->status = status;
        error->operation = operation;
        error->native_policy = status != MC_WORLD_STORAGE_OK;
        error->cause = (mc_world_storage_error){status, operation, 0};
    }
    return status;
}

static void at(mc_world_random_tick_call *c, unsigned int pc) {
    c->state.source_pc = c->error.bytecode = pc;
    c->error.child = MC_WORLD_RANDOM_TICK_NONE;
    c->error.child_class = c->error.child_method = NULL;
    c->error.child_bytecode = 0;
    c->error.acquisition = NULL;
    c->error.chunk = NULL;
    c->error.query = NULL;
    c->error.registry = NULL;
    c->error.biome = NULL;
    c->error.rain = NULL;
    c->error.lightning = NULL;
    c->error.publication = NULL;
    c->error.mutation = NULL;
}

static bool fail(mc_world_random_tick_call *c, mc_world_storage_status status,
                 const char *operation, bool native, const char *exception) {
    c->error.status = status;
    c->error.operation = operation;
    c->error.native_policy = native;
    c->error.exception_class = exception;
    c->error.cause = (mc_world_storage_error){status, operation, 0};
    return false;
}

static bool source_null(mc_world_random_tick_call *c, const char *operation) {
    return fail(c, MC_WORLD_STORAGE_UNSUPPORTED_STATE, operation, false,
                "java/lang/NullPointerException");
}

static bool storage_fail(mc_world_random_tick_call *c, mc_world_storage_error cause) {
    (void)fail(c, cause.status, cause.operation, true, NULL);
    c->error.cause = cause;
    return false;
}

static bool fp(mc_world_random_tick_call *c) {
    return (_mm_getcsr() & 0xffc0u) == 0x1f80u ||
           fail(c, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World random tick strict FP admission",
                true, NULL);
}

static bool math_use(mc_world_random_tick_call *c) {
    c->error.child = MC_WORLD_RANDOM_TICK_MATH;
    if (mc_math_classes_ensure(c->state.math, &c->error.math) != MC_MATH_CLASS_OK)
        return fail(c, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World random tick Math active use",
                    true, NULL);
    if (mc_math_classes_table(c->state.math) != c->state.world->classes->trig)
        return fail(c, MC_WORLD_STORAGE_INVALID_ARGUMENT, "World random tick Math table identity",
                    true, NULL);
    return fp(c);
}

static bool player_size(mc_world_random_tick_call *c, unsigned int pc, size_t *out) {
    at(c, pc);
    const mc_world_runtime *w = c->state.world;
    const mc_world_reference_list *list = w->fields.d;
    const mc_world_entity_list_ledger *ledger = &w->player_membership;
    if (list == NULL)
        return source_null(c, "World random tick null Player List");
    if (list != ledger->list || list->count != ledger->count ||
        list->capacity != ledger->capacity || list->count > list->capacity ||
        list->capacity > (size_t)INT32_MAX || ((list->capacity == 0) != (list->items == NULL)) ||
        ((ledger->capacity == 0) != (ledger->entries == NULL)))
        return fail(c, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World random tick Player List owner",
                    true, NULL);
    *out = list->count;
    return true;
}

static bool player_get(mc_world_random_tick_call *c, size_t index, unsigned int get_pc,
                       unsigned int cast_pc, mc_entity **out) {
    size_t count;
    if (!player_size(c, get_pc, &count))
        return false;
    if (index >= count)
        return fail(c, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World random tick Player List index",
                    false, "java/lang/IndexOutOfBoundsException");
    at(c, cast_pc);
    mc_world_runtime *w = c->state.world;
    mc_entity *entity = w->player_membership.entries[index];
    if (w->fields.d->items[index] != entity)
        return fail(c, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                    "World random tick Player occurrence provenance", true, NULL);
    if (entity != NULL && (entity->classes != w->classes->entities ||
                           !mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER)))
        return fail(c, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World random tick Player cast", false,
                    "java/lang/ClassCastException");
    *out = entity;
    return true;
}

static int32_t pair_hash(int32_t x, int32_t z) {
    const uint32_t bits = (x < 0 ? UINT32_C(0x80000000) : 0) |
                          (((uint32_t)x & UINT32_C(0x7fff)) << 16) |
                          (z < 0 ? UINT32_C(0x8000) : 0) | ((uint32_t)z & UINT32_C(0x7fff));
    int32_t value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static bool set_valid(mc_world_random_tick_call *c, const mc_world_active_chunks *set) {
    if (set == NULL)
        return source_null(c, "World random tick null candidate Set");
    if (set != c->state.world->active_chunks || set->count > set->capacity ||
        set->capacity > 65536 || ((set->capacity == 0) != (set->entries == NULL)) ||
        mc_java_int_set_size(set->order) != set->count)
        return fail(c, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World random tick candidate Set owner",
                    true, NULL);
    return true;
}

static bool set_add(mc_world_random_tick_call *c, mc_world_active_chunks *set, int32_t x,
                    int32_t z) {
    at(c, 111);
    if (!set_valid(c, set))
        return false;
    const int32_t hash = pair_hash(x, z);
    for (size_t i = 0; i < set->count; ++i) {
        if (set->entries[i].hash == hash) {
            if (set->entries[i].x == x && set->entries[i].z == z)
                return true;
            return fail(c, MC_WORLD_STORAGE_LIMIT, "World random tick same-hash yy boundary", true,
                        NULL);
        }
    }
    if (set->count == 65536)
        return fail(c, MC_WORLD_STORAGE_LIMIT, "World random tick candidate ceiling", true, NULL);
    if (set->count == set->capacity) {
        const size_t capacity = set->capacity == 0 ? 512 : set->capacity * 2;
        mc_world_chunk_candidate *entries = realloc(set->entries, capacity * sizeof(*entries));
        if (entries == NULL)
            return fail(c, MC_WORLD_STORAGE_ALLOCATION_FAILED,
                        "World random tick candidate allocation", true, NULL);
        set->entries = entries;
        set->capacity = capacity;
    }
    bool changed;
    const mc_java_int_set_status status = mc_java_int_set_add(set->order, hash, &changed);
    if (status != MC_JAVA_INT_SET_OK || !changed)
        return fail(c,
                    status == MC_JAVA_INT_SET_ALLOCATION_FAILED ? MC_WORLD_STORAGE_ALLOCATION_FAILED
                                                                : MC_WORLD_STORAGE_LIMIT,
                    "World random tick HashSet add", true, NULL);
    set->entries[set->count++] = (mc_world_chunk_candidate){x, z, hash};
    ++set->version;
    return true;
}

mc_world_storage_status mc_world_random_tick_active_set_clear(mc_world_runtime *world,
                                                              mc_world_random_tick_error *error) {
    mc_world_random_tick_error local;
    mc_world_random_tick_error *e = error == NULL ? &local : error;
    if (world == NULL || world->classes == NULL)
        return rejected(e, MC_WORLD_STORAGE_INVALID_ARGUMENT, "World active Set clear arguments");
    if (world->construction.mode != MC_WORLD_RUNTIME_GAME ||
        world->construction.phase == MC_WORLD_RUNTIME_RETIRED ||
        world->classes->side != MC_WORLD_INFO_CLIENT)
        return rejected(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World active Set clear phase");
    mc_world_storage_error cause = {0};
    mc_world_storage_status status = mc_world_runtime_call_enter(world, &cause);
    if (status != MC_WORLD_STORAGE_OK)
        return rejected(e, status, cause.operation);
    mc_world_active_chunks *set = world->fields.P;
    if (set == NULL) {
        status =
            rejected(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World random tick null candidate Set");
        e->native_policy = false;
        e->exception_class = "java/lang/NullPointerException";
    } else if (set != world->active_chunks || set->count > set->capacity || set->capacity > 65536 ||
               ((set->capacity == 0) != (set->entries == NULL)) ||
               mc_java_int_set_size(set->order) != set->count) {
        status = rejected(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                          "World random tick candidate Set owner");
    } else {
        if (set->order == NULL) {
            const mc_java_int_set_status made = mc_java_int_set_create(NULL, &set->order);
            if (made != MC_JAVA_INT_SET_OK)
                status = rejected(e,
                                  made == MC_JAVA_INT_SET_ALLOCATION_FAILED
                                      ? MC_WORLD_STORAGE_ALLOCATION_FAILED
                                      : MC_WORLD_STORAGE_LIMIT,
                                  "World random tick HashSet allocation");
        }
        if (status == MC_WORLD_STORAGE_OK) {
            mc_java_int_set_clear(set->order);
            set->count = 0;
            ++set->version;
            (void)rejected(e, MC_WORLD_STORAGE_OK, NULL);
        }
    }
    e->bytecode = 4;
    mc_world_runtime_call_leave(world);
    return status;
}

static bool gather(mc_world_random_tick_call *c) {
    mc_world_runtime *w = c->state.world;
    at(c, 4);
    if (mc_world_random_tick_active_set_clear(w, &c->error) != MC_WORLD_STORAGE_OK)
        return false;
    for (size_t i = 0;; ++i) {
        size_t count;
        if (!player_size(c, 16, &count))
            return false;
        if (i >= count)
            break;
        mc_entity *player;
        if (!player_get(c, i, 29, 34, &player))
            return false;
        at(c, 39);
        if (player == NULL)
            return source_null(c, "World random tick null Player x");
        const double source_x = player->base.x;
        at(c, 45);
        if (!fp(c))
            return false;
        const double divided_x = wt_div(source_x, 16.0);
        at(c, 46);
        if (!math_use(c))
            return false;
        const int32_t x = mc_math_floor_double(divided_x);
        const double source_z = player->base.z;
        at(c, 57);
        if (!fp(c))
            return false;
        const double divided_z = wt_div(source_z, 16.0);
        at(c, 58);
        if (!math_use(c))
            return false;
        const int32_t z = mc_math_floor_double(divided_z);
        for (int32_t dx = -9; dx <= 9; ++dx)
            for (int32_t dz = -9; dz <= 9; ++dz)
                if (!set_add(c, w->fields.P, mc_java_i32_add(x, dx), mc_java_i32_add(z, dz)))
                    return false;
    }
    c->state.candidates_returned = true;
    at(c, 136);
    if (w->fields.Q > 0) {
        w->fields.Q = mc_java_i32_sub(w->fields.Q, 1);
        c->state.cooldown_decremented = true;
    }
    return true;
}

static bool draw(mc_world_random_tick_call *c, unsigned int pc, int32_t bound, int32_t *out) {
    at(c, pc);
    mc_java_random *random = c->state.world->fields.r;
    if (random == NULL)
        return source_null(c, "World random tick null Random receiver");
    if (!mc_java_random_next_bounded(random, bound, out))
        return fail(c, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World random tick Random bound", false,
                    "java/lang/IllegalArgumentException");
    ++c->state.random_calls;
    c->state.draw = *out;
    return true;
}

static void lcg(mc_world_random_tick_call *c, unsigned int pc, bool height) {
    at(c, pc);
    mc_world_runtime *w = c->state.world;
    w->fields.g = mc_java_i32_add(mc_java_i32_mul(w->fields.g, 3), 1013904223);
    c->state.last_g = w->fields.g;
    ++c->state.lcg_updates;
    const int32_t v = mc_java_i32_shr(w->fields.g, 2);
    c->state.local_x = v & 15;
    c->state.local_z = mc_java_i32_shr(v, 8) & 15;
    if (height)
        c->state.local_y = mc_java_i32_shr(v, 16) & 127;
    c->state.x = mc_java_i32_add(c->state.offset_x, c->state.local_x);
    c->state.z = mc_java_i32_add(c->state.offset_z, c->state.local_z);
    if (height)
        c->state.y = c->state.local_y;
}

static const char *registry_source_exception(mc_registry_status status) {
    switch (status) {
        case MC_REGISTRY_NULL_REFERENCE:
            return "java/lang/NullPointerException";
        case MC_REGISTRY_INDEX_OUT_OF_RANGE:
            return "java/lang/ArrayIndexOutOfBoundsException";
        case MC_REGISTRY_ILLEGAL_ARGUMENT:
        case MC_REGISTRY_DUPLICATE_BLOCK:
        case MC_REGISTRY_DUPLICATE_STATISTIC:
            return "java/lang/IllegalArgumentException";
        default:
            return NULL;
    }
}

static bool same_registry_cause(const mc_registry_error *a, const mc_registry_error *b,
                                bool cached) {
    return (cached ? a->status == MC_REGISTRY_FAILED_INITIALIZATION : a->status == b->status) &&
           a->message == b->message && a->initializer_pc == b->initializer_pc &&
           a->block_id == b->block_id && a->constructor_class == b->constructor_class &&
           a->constructor_pc == b->constructor_pc && a->profile_status == b->profile_status &&
           a->active_initializer_class == b->active_initializer_class &&
           a->active_initializer_pc == b->active_initializer_pc &&
           a->item_argument == b->item_argument &&
           a->format_error.status == b->format_error.status &&
           a->format_error.offset == b->format_error.offset &&
           a->format_error.length == b->format_error.length &&
           a->format_error.value == b->format_error.value &&
           a->format_error.flags == b->format_error.flags &&
           a->format_error.conversion == b->format_error.conversion &&
           a->format_error.date_time == b->format_error.date_time;
}

static void registry_exception(mc_world_random_tick_call *c, const mc_registry_classes *classes,
                               const mc_registry_error *error) {
    const bool cached = error->status == MC_REGISTRY_FAILED_INITIALIZATION;
    const mc_registry_error *first = classes == NULL ? NULL : &classes->block_failure;
    const bool block_failure =
        first != NULL && classes->blocks_phase == MC_REGISTRY_FAILED &&
        classes->block_states[MC_BLOCK_BASE].phase == MC_REGISTRY_FAILED &&
        same_registry_cause(first, &classes->block_states[MC_BLOCK_BASE].failure, false) &&
        same_registry_cause(error, first, cached);
    const char *exception = NULL;
    if (block_failure) {
        /* Native limits/frontiers do not become Throwables when latched.
         * Only this actual Block owner's immutable, matching source cause
         * proves either first initialization or subsequent erroneous use. */
        if (registry_source_exception(first->status) != NULL)
            exception =
                cached ? "java/lang/NoClassDefFoundError" : "java/lang/ExceptionInInitializerError";
    } else if (!cached && error->active_initializer_class == NULL) {
        exception = registry_source_exception(error->status);
    }
    /* Other declaring-class initialization remains a native boundary until
     * its own immutable source cause can be identified. Never infer it from
     * FAILED_INITIALIZATION alone or rewrite the retained Registry record. */
    if (exception != NULL) {
        c->error.native_policy = false;
        c->error.exception_class = exception;
    }
}

static bool registry_fail(mc_world_random_tick_call *c) {
    c->error.child = MC_WORLD_RANDOM_TICK_REGISTRY;
    c->error.registry = &c->registry;
    (void)fail(c, mc_world_runtime_registry_status(c->registry.status), c->registry.message, true,
               NULL);
    registry_exception(c, c->state.world->classes->registry, &c->registry);
    return false;
}

static bool chunk_fail(mc_world_random_tick_call *c) {
    c->error.child = MC_WORLD_RANDOM_TICK_CHUNK;
    c->error.chunk = &c->chunk_error;
    c->error.child_class = c->chunk_error.site.class_name;
    c->error.child_method = c->chunk_error.site.method;
    c->error.child_bytecode = c->chunk_error.site.pc < 0 ? 0 : (unsigned int)c->chunk_error.site.pc;
    return fail(c, c->chunk_error.status, c->chunk_error.operation, c->chunk_error.native_policy,
                c->chunk_error.exception_class);
}

static bool query_fail(mc_world_random_tick_call *c) {
    c->error.child = MC_WORLD_RANDOM_TICK_QUERY;
    c->error.query = &c->query;
    c->error.child_bytecode = c->query.bytecode;
    (void)storage_fail(c, c->query.cause);
    registry_exception(c, c->state.world->classes->registry, &c->query.registry);
    return false;
}

static bool mutation_fail(mc_world_random_tick_call *c) {
    c->error.child = MC_WORLD_RANDOM_TICK_MUTATION;
    c->error.mutation = &c->mutation;
    c->error.child_class = c->mutation.hook_class;
    c->error.child_method = c->mutation.hook_method;
    c->error.child_bytecode = c->mutation.hook_bytecode;
    if (c->mutation.world_access_failure.receipt != NULL &&
        mc_world_access_failure_retain(&c->mutation.world_access_failure, &c->error.world_access) !=
            MC_WORLD_STORAGE_OK)
        return fail(c, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                    "World random tick WorldAccess failure capture", true, NULL);
    (void)storage_fail(c, c->mutation.cause);
    registry_exception(c, c->state.world->classes->registry, &c->mutation.registry);
    const char *exception = NULL;
    if (mc_world_access_failure_source_exception(&c->mutation.world_access_failure, &exception)) {
        c->error.native_policy = false;
        c->error.exception_class = exception;
    }
    return false;
}

static bool acquire(mc_world_random_tick_call *c) {
    at(c, 207);
    mc_world_runtime *w = c->state.world;
    c->error.child = MC_WORLD_RANDOM_TICK_ACQUISITION;
    c->error.acquisition = &c->acquisition;
    c->error.child_class = "fd";
    c->error.child_method = "c(II)Llm;";
    c->error.child_bytecode = 6;
    if (w->fields.v == NULL)
        return source_null(c, "World random tick null ChunkProvider");
    mc_chunk_provider_fields provider;
    mc_world_storage_error error = {0};
    if (mc_chunk_provider_bind_fields(w->fields.v, &provider, &error) != MC_WORLD_STORAGE_OK)
        return storage_fail(c, error);
    if (provider.world != w || provider.cache != w->acquisition.cache ||
        provider.pending != w->acquisition.pending || provider.store != w->options.store)
        return fail(c, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                    "World random tick actual ChunkProvider binding", true, NULL);
    mc_chunk_state *chunk = NULL;
    const mc_chunk_acquire_status status = mc_world_runtime_get_detailed(
        w, c->state.chunk_x, c->state.chunk_z, &chunk, &c->acquisition);
    if (status != MC_CHUNK_ACQUIRE_OK) {
        mc_world_storage_error cause = c->acquisition.cause;
        if (cause.status == MC_WORLD_STORAGE_OK)
            cause = (mc_world_storage_error){status == MC_CHUNK_ACQUIRE_LIMIT ||
                                                     status == MC_CHUNK_ACQUIRE_PARTIAL_LIMIT
                                                 ? MC_WORLD_STORAGE_LIMIT
                                                 : MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                             "World random tick Chunk acquisition", 0};
        return storage_fail(c, cause);
    }
    if (mc_world_chunk_tick_capture_begin(w, chunk, &c->chunk, &c->chunk_error) !=
        MC_WORLD_STORAGE_OK)
        return chunk_fail(c);
    (void)mc_world_chunk_tick_capture_inspect(c->chunk, &c->state.chunk);
    return true;
}

static double distance(mc_entity *entity, double x, double y, double z) {
    const double dx = wt_sub(entity->base.x, x), dy = wt_sub(entity->base.y, y);
    const double dz = wt_sub(entity->base.z, z);
    return wt_add(wt_add(wt_mul(dx, dx), wt_mul(dy, dy)), wt_mul(dz, dz));
}

static bool nearest(mc_world_random_tick_call *c, double x, double y, double z) {
    double best_distance = -1.0;
    for (size_t i = 0;; ++i) {
        size_t count;
        if (!player_size(c, 361, &count)) {
            c->error.child = MC_WORLD_RANDOM_TICK_NEAREST;
            c->error.child_class = "fd";
            c->error.child_method = "a(DDDD)Lgs;";
            c->error.child_bytecode = 17;
            return false;
        }
        if (i >= count)
            break;
        mc_entity *player;
        if (!player_get(c, i, 361, 361, &player)) {
            c->error.child = MC_WORLD_RANDOM_TICK_NEAREST;
            c->error.child_class = "fd";
            c->error.child_method = "a(DDDD)Lgs;";
            c->error.child_bytecode =
                c->error.exception_class != NULL &&
                        strcmp(c->error.exception_class, "java/lang/ClassCastException") == 0
                    ? 36
                    : 31;
            return false;
        }
        c->error.child = MC_WORLD_RANDOM_TICK_NEAREST;
        c->error.child_class = "fd";
        c->error.child_method = "a(DDDD)Lgs;";
        c->error.child_bytecode = 47;
        if (player == NULL)
            return source_null(c, "World ambient nearest null Player");
        if (!fp(c))
            return false;
        const double value = distance(player, x, y, z);
        if (value < 64.0 && (best_distance == -1.0 || value < best_distance)) {
            const mc_entity_status held = mc_entity_retain(player);
            if (held != MC_ENTITY_OK)
                return fail(c,
                            held == MC_ENTITY_LIMIT ? MC_WORLD_STORAGE_LIMIT
                                                    : MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                            "World ambient nearest Player hold", true, NULL);
            mc_entity_release(c->state.nearest);
            c->state.nearest = player;
            best_distance = value;
        }
    }
    return true;
}

static bool ambient(mc_world_random_tick_call *c) {
    if (c->state.world->fields.Q != 0)
        return true;
    ++c->state.ambient_attempts;
    lcg(c, 229, true);
    at(c, 275);
    if (mc_world_chunk_tick_block(c->chunk, c->state.local_x, c->state.local_y, c->state.local_z,
                                  &c->state.block_id, &c->chunk_error) != MC_WORLD_STORAGE_OK)
        return chunk_fail(c);
    if (c->state.block_id != 0)
        return true;
    at(c, 305);
    if (mc_world_runtime_plant_light(c->state.world, c->state.x, c->state.y, c->state.z,
                                     &c->state.light, &c->query) != MC_WORLD_STORAGE_OK)
        return query_fail(c);
    int32_t value;
    if (!draw(c, 314, 8, &value))
        return false;
    if (c->state.light > value)
        return true;
    const mc_world_light_type *sky = NULL;
    mc_world_storage_error error = {0};
    at(c, 321);
    if (mc_world_runtime_light_type_get(c->state.world->classes, MC_LIGHT_SKY, &sky, &error) !=
        MC_WORLD_STORAGE_OK)
        return storage_fail(c, error);
    at(c, 330);
    if (mc_world_runtime_saved_light(c->state.world, sky, c->state.x, c->state.y, c->state.z,
                                     &c->state.light, &c->query) != MC_WORLD_STORAGE_OK)
        return query_fail(c);
    if (c->state.light > 0)
        return true;
    at(c, 339);
    if (!fp(c))
        return false;
    const double x = wt_add((double)c->state.x, .5), y = wt_add((double)c->state.y, .5);
    const double z = wt_add((double)c->state.z, .5);
    if (!nearest(c, x, y, z))
        return false;
    if (c->state.nearest == NULL)
        return true;
    at(c, 375);
    if (!fp(c))
        return false;
    if (!(distance(c->state.nearest, wt_add((double)c->state.x, .5), wt_add((double)c->state.y, .5),
                   wt_add((double)c->state.z, .5)) > 4.0)) {
        mc_entity_release(c->state.nearest);
        c->state.nearest = NULL;
        return true;
    }
    at(c, 407);
    if (!fp(c))
        return false;
    const double sound_x = wt_add((double)c->state.x, .5);
    const double sound_y = wt_add((double)c->state.y, .5);
    const double sound_z = wt_add((double)c->state.z, .5);
    const mc_registry_string *name = NULL;
    at(c, 426);
    if (mc_registry_runtime_literal_ascii(c->state.world->classes->registry, "ambient.cave.cave",
                                          &name, &c->registry) != MC_REGISTRY_OK)
        return registry_fail(c);
    at(c, 436);
    mc_java_random *random = c->state.world->fields.r;
    if (random == NULL)
        return source_null(c, "World ambient pitch null Random");
    const float value_f = mc_java_random_next_float(random);
    ++c->state.random_calls;
    at(c, 441);
    if (!fp(c))
        return false;
    const float product = _mm_cvtss_f32(_mm_mul_ss(_mm_set_ss(value_f), _mm_set_ss(.2f)));
    c->state.pitch = wt_fadd(.8f, product);
    at(c, 443);
    mc_world_mutation_error_init(&c->mutation);
    if (mc_world_runtime_sound(c->state.world, sound_x, sound_y, sound_z, name, .7f, c->state.pitch,
                               &c->mutation) != MC_WORLD_STORAGE_OK) {
        (void)mutation_fail(c);
        c->error.child = MC_WORLD_RANDOM_TICK_SOUND;
        return false;
    }
    ++c->state.ambient_sounds;
    if (!draw(c, 454, 12000, &value))
        return false;
    at(c, 461);
    c->state.world->fields.Q = mc_java_i32_add(value, 6000);
    mc_entity_release(c->state.nearest);
    c->state.nearest = NULL;
    return true;
}

static bool rain_predicate(mc_world_random_tick_call *c, unsigned int pc, bool thunder, bool *out) {
    at(c, pc);
    mc_world_storage_error error = {0};
    float *value = thunder ? &c->state.thunder_strength : &c->state.rain_strength;
    const mc_world_storage_status status =
        thunder ? mc_world_runtime_thunder_strength(c->state.world, 1.0f, value, &error)
                : mc_world_runtime_rain_strength(c->state.world, 1.0f, value, &error);
    if (status != MC_WORLD_STORAGE_OK)
        return storage_fail(c, error);
    *out = wt_widen(*value) > (thunder ? .9 : .2);
    return true;
}

static bool lightning(mc_world_random_tick_call *c) {
    int32_t ticket;
    if (!draw(c, 470, 100000, &ticket))
        return false;
    if (ticket != 0)
        return true;
    if (!rain_predicate(c, 477, false, &c->state.raining))
        return false;
    if (!c->state.raining)
        return true;
    if (!rain_predicate(c, 484, true, &c->state.thundering))
        return false;
    if (!c->state.thundering)
        return true;
    ++c->state.lightning_attempts;
    lcg(c, 500, false);
    at(c, 538);
    if (mc_world_runtime_top_solid_liquid(c->state.world, c->state.x, c->state.z, &c->state.top_y,
                                          &c->query) != MC_WORLD_STORAGE_OK)
        return query_fail(c);
    at(c, 550);
    bool allowed;
    mc_world_movement_error_init(&c->rain);
    if (mc_world_runtime_rain_at(c->state.world, c->state.x, c->state.top_y, c->state.z, &allowed,
                                 &c->rain) != MC_WORLD_STORAGE_OK) {
        c->error.child = MC_WORLD_RANDOM_TICK_RAIN;
        c->error.rain = &c->rain;
        (void)storage_fail(c, c->rain.cause);
        registry_exception(c, c->state.world->classes->registry, &c->rain.registry);
        return false;
    }
    if (!allowed)
        return true;
    at(c, 557);
    mc_entity_release(c->state.lightning);
    c->state.lightning = NULL;
    const mc_entity_status allocated = mc_lightning_allocate(c->state.world->classes->entities,
                                                             &c->state.lightning, &c->allocation);
    if (allocated != MC_ENTITY_OK)
        return fail(c,
                    allocated == MC_ENTITY_ALLOCATION_FAILED ? MC_WORLD_STORAGE_ALLOCATION_FAILED
                    : allocated == MC_ENTITY_LIMIT           ? MC_WORLD_STORAGE_LIMIT
                                                             : MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                    c->allocation.operation, true, NULL);
    at(c, 564);
    if (!fp(c))
        return false;
    at(c, 571);
    if (mc_lightning_construct(c->state.lightning, c->state.world, (double)c->state.x,
                               (double)c->state.top_y, (double)c->state.z,
                               &c->lightning_error) != MC_ENTITY_OK) {
        c->error.child = MC_WORLD_RANDOM_TICK_LIGHTNING;
        c->error.lightning = &c->lightning_error;
        c->error.child_class = c->lightning_error.site.class_name;
        c->error.child_method = c->lightning_error.site.method;
        c->error.child_bytecode = c->lightning_error.site.pc;
        if (c->lightning_error.child == MC_LIGHTNING_CHILD_MUTATION &&
            c->lightning_error.detail.mutation.world_access_failure.receipt != NULL &&
            mc_world_access_failure_retain(&c->lightning_error.detail.mutation.world_access_failure,
                                           &c->error.world_access) != MC_WORLD_STORAGE_OK)
            return fail(c, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                        "World random tick Lightning failure capture", true, NULL);
        return fail(c,
                    c->lightning_error.status == MC_ENTITY_ALLOCATION_FAILED
                        ? MC_WORLD_STORAGE_ALLOCATION_FAILED
                    : c->lightning_error.status == MC_ENTITY_LIMIT
                        ? MC_WORLD_STORAGE_LIMIT
                        : MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                    c->lightning_error.operation, c->lightning_error.native_policy,
                    c->lightning_error.exception_class);
    }
    at(c, 574);
    if (mc_world_runtime_add_weather_entity(c->state.world, c->state.lightning,
                                            &c->state.publication_result,
                                            &c->publication) != MC_WORLD_STORAGE_OK) {
        c->error.child = MC_WORLD_RANDOM_TICK_PUBLICATION;
        c->error.publication = &c->publication;
        return storage_fail(c, c->publication.cause);
    }
    ++c->state.weather_publications;
    at(c, 580);
    c->state.world->fields.m = 2;
    return true;
}

static bool static_block(mc_world_random_tick_call *c, unsigned int pc, const char *name,
                         const mc_block **out) {
    at(c, pc);
    if (mc_registry_block_static_get(c->state.world->classes->registry, name, out, &c->registry) !=
        MC_REGISTRY_OK)
        return registry_fail(c);
    return true;
}

static bool block_id(mc_world_random_tick_call *c, unsigned int pc, const mc_block *block,
                     int32_t *out) {
    at(c, pc);
    if (block == NULL)
        return source_null(c, "World weather null Block ID receiver");
    *out = block->fields.bn;
    return true;
}

static bool snow_ice(mc_world_random_tick_call *c) {
    int32_t ticket;
    if (!draw(c, 589, 16, &ticket))
        return false;
    if (ticket != 0)
        return true;
    ++c->state.snow_attempts;
    lcg(c, 605, false);
    at(c, 643);
    if (mc_world_runtime_top_solid_liquid(c->state.world, c->state.x, c->state.z, &c->state.top_y,
                                          &c->query) != MC_WORLD_STORAGE_OK)
        return query_fail(c);
    at(c, 649);
    mc_world_provider_fields dimension;
    if (mc_world_provider_fields_get(c->state.world->fields.t, &dimension) != MC_WORLD_PROVIDER_OK)
        return source_null(c, "World weather null WorldProvider");
    at(c, 661);
    const mc_biome *biome = NULL;
    if (mc_world_chunk_manager_point(dimension.b, c->state.x, c->state.z, &biome, &c->biome) !=
        MC_WORLD_PROVIDER_OK) {
        c->error.child = MC_WORLD_RANDOM_TICK_BIOME;
        c->error.biome = &c->biome;
        return fail(c,
                    c->biome.status == MC_WORLD_PROVIDER_ALLOCATION_FAILED
                        ? MC_WORLD_STORAGE_ALLOCATION_FAILED
                    : c->biome.status == MC_WORLD_PROVIDER_LIMIT
                        ? MC_WORLD_STORAGE_LIMIT
                        : MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                    c->biome.operation, dimension.b != NULL,
                    dimension.b == NULL ? "java/lang/NullPointerException" : NULL);
    }
    at(c, 664);
    const mc_biome_fields *fields = mc_biome_fields_get(biome);
    if (fields == NULL)
        return source_null(c, "World weather null Biome snow receiver");
    c->state.snowy = fields->v;
    if (!c->state.snowy || c->state.top_y < 0 || c->state.top_y >= 128)
        return true;
    const mc_world_light_type *type = NULL;
    mc_world_storage_error error = {0};
    at(c, 685);
    if (mc_world_runtime_light_type_get(c->state.world->classes, MC_LIGHT_BLOCK, &type, &error) !=
        MC_WORLD_STORAGE_OK)
        return storage_fail(c, error);
    at(c, 694);
    if (mc_world_chunk_tick_saved_light(c->chunk, type, c->state.local_x, c->state.top_y,
                                        c->state.local_z, &c->state.light,
                                        &c->chunk_error) != MC_WORLD_STORAGE_OK)
        return chunk_fail(c);
    if (c->state.light >= 10)
        return true;
    const int32_t below_y = mc_java_i32_sub(c->state.top_y, 1);
    at(c, 712);
    if (mc_world_chunk_tick_block(c->chunk, c->state.local_x, below_y, c->state.local_z,
                                  &c->state.below_id, &c->chunk_error) != MC_WORLD_STORAGE_OK)
        return chunk_fail(c);
    at(c, 725);
    if (mc_world_chunk_tick_block(c->chunk, c->state.local_x, c->state.top_y, c->state.local_z,
                                  &c->state.current_id, &c->chunk_error) != MC_WORLD_STORAGE_OK)
        return chunk_fail(c);
    if (!rain_predicate(c, 731, false, &c->state.raining))
        return false;
    if (c->state.raining && c->state.current_id == 0) {
        const mc_block *snow = NULL;
        if (!static_block(c, 742, "aT", &snow))
            return false;
        at(c, 757);
        if (snow == NULL)
            return source_null(c, "World snow null can-place receiver");
        if (snow->kind != MC_BLOCK_SNOW_LAYER)
            return fail(c, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                        "World snow actual can-place dispatch", true, NULL);
        mc_world_mutation_error_init(&c->mutation);
        bool allowed;
        if (!mc_world_mutation_snow_can_place(c->state.world, snow, c->state.x, c->state.top_y,
                                              c->state.z, &allowed, &c->mutation))
            return mutation_fail(c);
        if (allowed && c->state.below_id != 0) {
            const mc_block *ice = NULL;
            int32_t ice_id;
            if (!static_block(c, 770, "aU", &ice) || !block_id(c, 773, ice, &ice_id))
                return false;
            if (c->state.below_id != ice_id) {
                mc_registry_block_m_view array;
                at(c, 779);
                if (mc_registry_block_m_get(c->state.world->classes->registry, &array,
                                            &c->registry) != MC_REGISTRY_OK)
                    return registry_fail(c);
                const mc_block *support = NULL;
                at(c, 784);
                if (mc_registry_block_m_at(&array, c->state.below_id, &support, &c->registry) !=
                    MC_REGISTRY_OK)
                    return registry_fail(c);
                at(c, 785);
                if (support == NULL)
                    return source_null(c, "World snow null supporting Block");
                mc_material *material = support->fields.bA;
                at(c, 788);
                if (material == NULL)
                    return source_null(c, "World snow null Material");
                if (mc_material_c(material)) {
                    /* The source reloads aT after the support queries. */
                    int32_t snow_id;
                    if (!static_block(c, 806, "aT", &snow) || !block_id(c, 809, snow, &snow_id))
                        return false;
                    at(c, 812);
                    bool changed;
                    mc_world_mutation_error_init(&c->mutation);
                    if (mc_world_runtime_block_set(c->state.world, c->state.x, c->state.top_y,
                                                   c->state.z, snow_id, &changed,
                                                   &c->mutation) != MC_WORLD_STORAGE_OK)
                        return mutation_fail(c);
                    ++c->state.snow_writes;
                }
            }
        }
    }
    const mc_block *water = NULL;
    int32_t water_id;
    if (!static_block(c, 818, "C", &water) || !block_id(c, 821, water, &water_id))
        return false;
    if (c->state.below_id != water_id)
        return true;
    int32_t metadata;
    at(c, 837);
    if (mc_world_chunk_tick_metadata(c->chunk, c->state.local_x, below_y, c->state.local_z,
                                     &metadata, &c->chunk_error) != MC_WORLD_STORAGE_OK)
        return chunk_fail(c);
    if (metadata != 0)
        return true;
    const mc_block *ice = NULL;
    int32_t ice_id;
    if (!static_block(c, 857, "aU", &ice) || !block_id(c, 860, ice, &ice_id))
        return false;
    at(c, 863);
    bool changed;
    mc_world_mutation_error_init(&c->mutation);
    if (mc_world_runtime_block_set(c->state.world, c->state.x, below_y, c->state.z, ice_id,
                                   &changed, &c->mutation) != MC_WORLD_STORAGE_OK)
        return mutation_fail(c);
    ++c->state.ice_writes;
    return true;
}

static bool samples(mc_world_random_tick_call *c) {
    for (c->state.sample = 0; c->state.sample < 80; ++c->state.sample) {
        lcg(c, 887, true);
        ++c->state.samples_started;
        at(c, 944);
        if (mc_world_chunk_tick_raw_block(c->chunk, c->state.local_x, c->state.local_y,
                                          c->state.local_z, &c->state.block_id,
                                          &c->chunk_error) != MC_WORLD_STORAGE_OK)
            return chunk_fail(c);
        mc_registry_block_n_view active;
        at(c, 951);
        if (mc_registry_block_n_get(c->state.world->classes->registry, &active, &c->registry) !=
            MC_REGISTRY_OK)
            return registry_fail(c);
        at(c, 956);
        bool enabled;
        if (mc_registry_block_n_at(&active, c->state.block_id, &enabled, &c->registry) !=
            MC_REGISTRY_OK)
            return registry_fail(c);
        if (enabled) {
            mc_registry_block_m_view blocks;
            at(c, 960);
            if (mc_registry_block_m_get(c->state.world->classes->registry, &blocks, &c->registry) !=
                MC_REGISTRY_OK)
                return registry_fail(c);
            const mc_block *block = NULL;
            at(c, 965);
            if (mc_registry_block_m_at(&blocks, c->state.block_id, &block, &c->registry) !=
                MC_REGISTRY_OK)
                return registry_fail(c);
            mc_java_random *random = c->state.world->fields.r; /* Late source getfield979. */
            at(c, 982);
            if (block == NULL)
                return source_null(c, "World random tick null Block receiver");
            mc_world_mutation_error_init(&c->mutation);
            if (!mc_world_mutation_block_tick(c->state.world, block, c->state.x, c->state.y,
                                              c->state.z, random, &c->mutation))
                return mutation_fail(c);
            ++c->state.block_ticks;
        }
        at(c, 985);
        ++c->state.samples_returned;
    }
    return true;
}

static bool body(mc_world_random_tick_call *c) {
    if (!gather(c))
        return false;
    at(c, 156);
    mc_world_active_chunks *set = c->state.world->fields.P;
    if (!set_valid(c, set))
        return false;
    c->state.iterator_set = set;
    const size_t count = set->count;
    const uint32_t version = set->version;
    c->state.chunk_count = count;
    int32_t *order = count == 0 ? NULL : malloc(count * sizeof(*order));
    if (count != 0 && order == NULL)
        return fail(c, MC_WORLD_STORAGE_ALLOCATION_FAILED, "World random tick iterator inventory",
                    true, NULL);
    size_t written;
    if (mc_java_int_set_copy_order(set->order, order, count, &written) != MC_JAVA_INT_SET_OK ||
        written != count) {
        free(order);
        return fail(c, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World random tick iterator owner", true,
                    NULL);
    }
    bool ok = true;
    for (size_t index = 0; index < count; ++index) {
        at(c, 172); /* hasNext does not perform the source modCount comparison. */
        if (set->version != version) {
            ok = fail(c, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                      "World random tick candidate iterator modified", false,
                      "java/util/ConcurrentModificationException");
            break;
        }
        if (!set_valid(c, set)) {
            ok = false;
            break;
        }
        const mc_world_chunk_candidate *entry = NULL;
        for (size_t i = 0; i < set->count; ++i)
            if (set->entries[i].hash == order[index]) {
                entry = &set->entries[i];
                break;
            }
        if (entry == NULL) {
            ok = fail(c, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                      "World random tick candidate coordinate ledger", true, NULL);
            break;
        }
        c->state.chunk_x = entry->x;
        c->state.chunk_z = entry->z;
        c->state.offset_x = mc_java_i32_mul(entry->x, 16);
        c->state.offset_z = mc_java_i32_mul(entry->z, 16);
        ++c->state.chunks_started;
        if (!acquire(c) || !ambient(c) || !lightning(c) || !snow_ice(c) || !samples(c)) {
            ok = false;
            break;
        }
        (void)mc_world_chunk_tick_capture_inspect(c->chunk, &c->state.chunk);
        at(c, 991);
        if (mc_world_chunk_tick_capture_retire(&c->chunk, &c->chunk_cleanup) !=
            MC_WORLD_STORAGE_OK) {
            c->chunk_error = c->chunk_cleanup;
            ok = chunk_fail(c);
            break;
        }
        /* The completed local's native lease has ended. Keep scalar read
         * diagnostics, but never expose an unleased Chunk identity as live. */
        c->state.chunk.captured = NULL;
        c->state.chunk.failure = NULL;
        c->state.chunk.world_held = c->state.chunk.chunk_held = false;
        c->state.chunk.ready = false;
        ++c->state.chunks_returned;
    }
    free(order);
    if (!ok)
        return false;
    at(c, 994);
    c->state.returned = true;
    memset(&c->error, 0, sizeof(c->error));
    return true;
}

mc_world_storage_status mc_world_runtime_random_tick_begin(mc_world_runtime *world,
                                                           mc_math_classes *math,
                                                           mc_world_random_tick_call **out,
                                                           mc_world_random_tick_error *admission) {
    if (world == NULL || out == NULL || *out != NULL)
        return rejected(admission, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                        "World random tick arguments");
    if (world->construction.mode != MC_WORLD_RUNTIME_GAME ||
        world->construction.phase == MC_WORLD_RUNTIME_RETIRED || world->classes == NULL ||
        world->classes->side != MC_WORLD_INFO_CLIENT)
        return rejected(admission, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                        "World random tick actual client GAME owner");
    if (world->persistent_bindings == SIZE_MAX)
        return rejected(admission, MC_WORLD_STORAGE_LIMIT, "World random tick binding ceiling");
    mc_world_random_tick_call *c = calloc(1, sizeof(*c));
    if (c == NULL)
        return rejected(admission, MC_WORLD_STORAGE_ALLOCATION_FAILED,
                        "World random tick call allocation");
    mc_world_storage_error error = {0};
    if (mc_world_runtime_call_enter(world, &error) != MC_WORLD_STORAGE_OK) {
        free(c);
        return rejected(admission, error.status, error.operation);
    }
    if (math != NULL && mc_math_classes_retain(math, NULL) != MC_MATH_CLASS_OK) {
        mc_world_runtime_call_leave(world);
        free(c);
        return rejected(admission, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                        "World random tick Math namespace hold");
    }
    c->math_held = math != NULL;
    c->state.math = math;
    (void)mc_world_runtime_binding_retain(world);
    c->world_held = true;
    c->state.world = world;
    c->state.error = &c->error;
    c->state.active = true;
    *out = c;
    (void)rejected(admission, MC_WORLD_STORAGE_OK, NULL);
    const bool returned = body(c);
    if (!returned && c->error.status == MC_WORLD_STORAGE_OK)
        (void)fail(c, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                   "World random tick child failed without a cause", true, NULL);
    c->state.active = false;
    if (c->chunk != NULL)
        (void)mc_world_chunk_tick_capture_inspect(c->chunk, &c->state.chunk);
    world->last_error = c->error.cause;
    mc_world_runtime_call_leave(world);
    return c->error.status;
}

bool mc_world_runtime_random_tick_inspect(const mc_world_random_tick_call *c,
                                          mc_world_random_tick_state *out) {
    if (c == NULL || out == NULL)
        return false;
    *out = c->state;
    return true;
}

mc_world_storage_status mc_world_runtime_random_tick_retire(mc_world_random_tick_call **slot,
                                                            mc_world_random_tick_error *cleanup) {
    if (slot == NULL)
        return rejected(cleanup, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                        "World random tick retire slot");
    mc_world_random_tick_call *c = *slot;
    if (c == NULL)
        return rejected(cleanup, MC_WORLD_STORAGE_OK, NULL);
    if (c->state.active)
        return rejected(cleanup, MC_WORLD_STORAGE_BUSY, "World random tick remains active");
    if (mc_world_chunk_tick_capture_retire(&c->chunk, &c->chunk_cleanup) != MC_WORLD_STORAGE_OK)
        return rejected(cleanup, c->chunk_cleanup.status, c->chunk_cleanup.operation);
    if (c->math_held) {
        mc_math_classes *math = c->state.math;
        const mc_math_class_status status = mc_math_classes_release(&math, NULL);
        if (status != MC_MATH_CLASS_OK)
            return rejected(cleanup,
                            status == MC_MATH_CLASS_BUSY ? MC_WORLD_STORAGE_BUSY
                                                         : MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                            "World random tick Math retirement");
        c->math_held = false;
    }
    mc_world_access_failure_release(&c->error.world_access);
    mc_entity_release(c->state.nearest);
    mc_entity_release(c->state.lightning);
    if (c->world_held)
        mc_world_runtime_binding_release(c->state.world);
    free(c);
    *slot = NULL;
    return rejected(cleanup, MC_WORLD_STORAGE_OK, NULL);
}
