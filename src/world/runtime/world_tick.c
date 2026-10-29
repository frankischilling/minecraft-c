#include "mc/world/runtime/world_tick.h"

#include "../../entity/entity_internal.h"
#include "../../entity/player/player_internal.h"
#include "../info/tick_source_internal.h"
#include "../info/weather_source_internal.h"
#include "chunk_provider_internal.h"
#include "construction_internal.h"
#include "entity_tick_math_internal.h"
#include "mc/core/java/java.h"
#include "mc/core/math/math.h"
#include "mc/world/runtime/environment.h"
#include "mc/world/runtime/saving/saving.h"
#include "runtime_internal.h"
#include "world_access_internal.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

typedef struct candidate {
    int32_t x, z, hash;
} candidate;

struct mc_world_tick_call {
    mc_world_tick_state state;
    mc_world_tick_error error;
    mc_world_weather_error weather;
    mc_world_runtime_lifecycle_error lifecycle;
    mc_world_scheduled_drain_error scheduled;
    mc_world_chunk_manager_error biome;
    bool world_held;
};

static mc_world_storage_status rejected(mc_world_tick_error *e, mc_world_storage_status status,
                                        const char *operation) {
    if (e != NULL)
        *e = (mc_world_tick_error){.status = status,
                                   .operation = operation,
                                   .native_policy = status != MC_WORLD_STORAGE_OK,
                                   .cause = {status, operation, 0}};
    return status;
}

static void at(mc_world_tick_call *c, unsigned int pc, mc_world_tick_child child,
               const char *source_class, const char *method, unsigned int child_pc) {
    c->state.source_pc = c->error.bytecode = pc;
    c->error.child = child;
    c->error.source_class = source_class;
    c->error.source_method = method;
    c->error.child_bytecode = child_pc;
    c->error.descendant_bytecode = 0;
    c->error.native_policy = false;
}

static bool fail(mc_world_tick_call *c, mc_world_storage_status status, const char *operation,
                 bool native) {
    c->error.status = status;
    c->error.operation = operation;
    c->error.native_policy = native;
    c->error.cause = (mc_world_storage_error){status, operation, 0};
    return false;
}

static bool cause(mc_world_tick_call *c, mc_world_storage_error error) {
    const bool native = c->error.native_policy;
    (void)fail(c, error.status, error.operation, native);
    c->error.cause = error;
    return false;
}

static bool fp(mc_world_tick_call *c, unsigned int pc) {
    c->error.child_bytecode = pc;
    return (_mm_getcsr() & 0xffc0u) == 0x1f80u ||
           fail(c, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World tick reached strict FP admission",
                true);
}

/* Read only the reached List metadata/occurrence. Nonnull values are proved
 * against the separately captured native owner before any Entity dereference.
 * No callback occurs while these borrowed references are used. */
static bool list_size(mc_world_tick_call *c, mc_world_reference_list *list,
                      mc_world_entity_list_ledger *ledger, unsigned int pc, size_t *out) {
    c->error.child_bytecode = pc;
    if (list == NULL || list != ledger->list || list->count != ledger->count ||
        list->capacity != ledger->capacity || list->count > list->capacity ||
        list->capacity > (size_t)INT32_MAX || ((list->capacity == 0) != (list->items == NULL)) ||
        ((ledger->capacity == 0) != (ledger->entries == NULL)))
        return fail(c, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World tick reached Entity List owner",
                    list != NULL);
    *out = list->count;
    return true;
}

static bool list_get(mc_world_tick_call *c, mc_world_reference_list *list,
                     mc_world_entity_list_ledger *ledger, size_t index, unsigned int get_pc,
                     unsigned int cast_pc, bool player, mc_entity **out) {
    size_t count;
    if (!list_size(c, list, ledger, get_pc, &count))
        return false;
    if (index >= count)
        return fail(c, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World tick reached List index", false);
    c->error.child_bytecode = cast_pc;
    if (list->items[index] != ledger->entries[index])
        return fail(c, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                    "World tick reached Entity occurrence provenance", true);
    mc_entity *entity = ledger->entries[index];
    if (entity != NULL &&
        (entity->classes != c->state.world->classes->entities ||
         (player && !mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER)))) {
        c->error.exception_class = "java/lang/ClassCastException";
        return fail(c, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World tick reached Player cast", false);
    }
    *out = entity;
    return true;
}

static bool set_valid(mc_world_tick_call *c, mc_world_active_chunks *set, unsigned int pc) {
    c->error.child_bytecode = pc;
    if (set == NULL)
        return fail(c, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World tick null candidate Set", false);
    if (set->count > set->capacity || set->capacity > 65536 ||
        ((set->capacity == 0) != (set->entries == NULL)) ||
        mc_java_int_set_size(set->order) != set->count)
        return fail(c, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World tick candidate Set ownership",
                    true);
    return true;
}

static bool set_prepare(mc_world_tick_call *c, mc_world_active_chunks *set, unsigned int pc) {
    if (!set_valid(c, set, pc))
        return false;
    if (set->order == NULL) {
        const mc_java_int_set_status status = mc_java_int_set_create(NULL, &set->order);
        if (status != MC_JAVA_INT_SET_OK)
            return fail(c,
                        status == MC_JAVA_INT_SET_ALLOCATION_FAILED
                            ? MC_WORLD_STORAGE_ALLOCATION_FAILED
                            : MC_WORLD_STORAGE_LIMIT,
                        "World tick candidate Set allocation", true);
    }
    mc_java_int_set_clear(set->order);
    set->count = 0;
    return true;
}

static int32_t pair_hash(int32_t x, int32_t z) {
    const uint32_t bits = (x < 0 ? UINT32_C(0x80000000) : 0) |
                          (((uint32_t)x & UINT32_C(0x7fff)) << 16) |
                          (z < 0 ? UINT32_C(0x8000) : 0) | ((uint32_t)z & UINT32_C(0x7fff));
    int32_t result;
    memcpy(&result, &bits, sizeof result);
    return result;
}

static bool set_add(mc_world_tick_call *c, mc_world_active_chunks *set, int32_t x, int32_t z,
                    unsigned int pc) {
    c->error.child_bytecode = pc;
    const int32_t hash = pair_hash(x, z);
    candidate *entries = set->entries;
    for (size_t i = 0; i < set->count; ++i) {
        if (entries[i].hash != hash)
            continue;
        if (entries[i].x == x && entries[i].z == z)
            return true;
        return fail(c, MC_WORLD_STORAGE_LIMIT,
                    "World tick distinct yy keys with equal hash native boundary", true);
    }
    if (set->count == 65536)
        return fail(c, MC_WORLD_STORAGE_LIMIT, "World tick candidate Set native ceiling", true);
    if (set->count == set->capacity) {
        const size_t next = set->capacity == 0 ? 512 : set->capacity * 2;
        candidate *grown = realloc(entries, next * sizeof(*grown));
        if (grown == NULL)
            return fail(c, MC_WORLD_STORAGE_ALLOCATION_FAILED,
                        "World tick candidate coordinate allocation", true);
        set->entries = entries = grown;
        set->capacity = next;
    }
    bool added;
    const mc_java_int_set_status status = mc_java_int_set_add(set->order, hash, &added);
    if (status != MC_JAVA_INT_SET_OK)
        return fail(c,
                    status == MC_JAVA_INT_SET_ALLOCATION_FAILED ? MC_WORLD_STORAGE_ALLOCATION_FAILED
                                                                : MC_WORLD_STORAGE_LIMIT,
                    "World tick candidate HashSet add", true);
    if (!added)
        return fail(c, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World tick candidate Set key ledger",
                    true);
    entries[set->count++] = (candidate){x, z, hash};
    return true;
}

static bool candidates(mc_world_tick_call *c, mc_world_active_chunks *set, bool spawning) {
    mc_world_runtime *w = c->state.world;
    const int32_t radius = spawning ? 8 : 9;
    const unsigned int size_pc = spawning ? 25 : 16;
    for (size_t i = 0;; ++i) {
        size_t count;
        if (!list_size(c, w->fields.d, &w->player_membership, size_pc, &count))
            return false;
        if (i >= count)
            return true;
        mc_entity *player;
        if (!list_get(c, w->fields.d, &w->player_membership, i, spawning ? 38 : 29,
                      spawning ? 43 : 34, true, &player))
            return false;
        c->error.child_bytecode = spawning ? 50 : 39;
        if (player == NULL) {
            c->error.exception_class = "java/lang/NullPointerException";
            return fail(c, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World tick null Player position",
                        false);
        }
        if (!fp(c, spawning ? 56 : 45))
            return false;
        const int32_t x = mc_math_floor_double(wt_div(player->base.x, 16.0));
        const int32_t z = mc_math_floor_double(wt_div(player->base.z, 16.0));
        for (int32_t dx = -radius; dx <= radius; ++dx)
            for (int32_t dz = -radius; dz <= radius; ++dz)
                if (!set_add(c, set, mc_java_i32_add(x, dx), mc_java_i32_add(z, dz),
                             spawning ? 124 : 111))
                    return false;
    }
}

static bool sleep_predicate(mc_world_tick_call *c, bool *sleeping) {
    mc_world_runtime *w = c->state.world;
    *sleeping = false;
    if (!w->fields.J || w->fields.B)
        return true;
    mc_world_reference_list *captured = w->fields.d;
    size_t count;
    if (!list_size(c, captured, &w->player_membership, 18, &count))
        return false;
    for (size_t i = 0; i < count; ++i) {
        mc_entity *player;
        if (!list_get(c, captured, &w->player_membership, i, 34, 39, true, &player))
            return false;
        c->error.child_bytecode = 44;
        if (player == NULL)
            return fail(c, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World sleeping null Player", false);
        const mc_local_player_object *local = (const mc_local_player_object *)player;
        if (!local->state.fields.player.u || local->state.fields.player.private_a < 100)
            return true;
    }
    *sleeping = true;
    return true;
}

static bool advance_sleep(mc_world_tick_call *c) {
    mc_world_runtime *w = c->state.world;
    if (w->fields.N && w->fields.q >= 1) {
        at(c, 33, MC_WORLD_TICK_CHILD_SLEEP, "cq", "a(World,List)Z", 7);
        return fail(c, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                    "World sleep hostile-spawn pathfinder constructor", true);
    }
    int64_t now;
    at(c, 45, MC_WORLD_TICK_CHILD_SLEEP, "ei", "f()J", 1);
    if (mc_world_info_time_get(w->fields.x, &now) != MC_WORLD_INFO_OK)
        return fail(c, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World sleep Info time receiver", false);
    const int64_t next = mc_java_i64_add(now, 24000);
    int64_t remainder;
    (void)mc_java_i64_rem(next, 24000, &remainder);
    at(c, 64, MC_WORLD_TICK_CHILD_SLEEP, "ei", "a(J)V", 2);
    if (mc_world_info_source_time_set(w->fields.x, mc_java_i64_sub(next, remainder)) !=
        MC_WORLD_INFO_OK)
        return fail(c, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World sleep Info time store", false);
    at(c, 68, MC_WORLD_TICK_CHILD_SLEEP, "fd", "z()V", 2);
    w->fields.J = false;
    mc_world_reference_list *list = w->fields.d;
    size_t count;
    if (!list_size(c, list, &w->player_membership, 9, &count))
        return false;
    for (size_t i = 0; i < count; ++i) {
        mc_entity *player;
        if (!list_get(c, list, &w->player_membership, i, 25, 30, true, &player))
            return false;
        c->error.child_bytecode = 35;
        if (player == NULL)
            return fail(c, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World wake null Player", false);
        if (((const mc_local_player_object *)player)->state.fields.player.u) {
            c->error.child_bytecode = 45;
            return fail(c, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World wake actual Player.a(ZZZ)V",
                        true);
        }
    }
    c->error.child_bytecode = 52;
    /* Actual F: fresh Info receiver at each operation, in source order. */
    if (mc_world_info_source_rain_time_set(w->fields.x, 0) != MC_WORLD_INFO_OK ||
        mc_world_info_source_raining_set(w->fields.x, false) != MC_WORLD_INFO_OK ||
        mc_world_info_source_thunder_time_set(w->fields.x, 0) != MC_WORLD_INFO_OK ||
        mc_world_info_source_thundering_set(w->fields.x, false) != MC_WORLD_INFO_OK)
        return fail(c, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World wake F Info receiver", false);
    return true;
}

/* ff is an interface, not the gz Monster class. bg/ar are omitted intermediate
 * classes; this exhaustive concrete namespace mapping preserves their actual
 * assignability, including uw Slime and bp Ghast implementing ff. */
static bool category_matches(mc_entity_class_kind kind, int32_t category) {
    if (category == 0)
        return (kind >= MC_ENTITY_CLASS_KIND_MONSTER && kind <= MC_ENTITY_CLASS_KIND_PIG_ZOMBIE);
    if (category == 1)
        return (kind >= MC_ENTITY_CLASS_KIND_PIG && kind <= MC_ENTITY_CLASS_KIND_CHICKEN) ||
               kind == MC_ENTITY_CLASS_KIND_WOLF;
    return kind == MC_ENTITY_CLASS_KIND_SQUID;
}

static bool entity_count(mc_world_tick_call *c, int32_t category, int32_t *out) {
    mc_world_runtime *w = c->state.world;
    int32_t total = 0;
    for (size_t i = 0;; ++i) {
        size_t count;
        c->error.child_bytecode = 216;
        c->error.descendant_bytecode = 9;
        if (!list_size(c, w->fields.b, &w->entity_membership, 216, &count))
            return false;
        if (i >= count) {
            *out = total;
            return true;
        }
        mc_entity *entity;
        if (!list_get(c, w->fields.b, &w->entity_membership, i, 216, 216, false, &entity))
            return false;
        c->error.descendant_bytecode = 35;
        if (entity == NULL)
            return fail(c, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World spawn count null Entity",
                        false);
        mc_entity_class_literal_view view;
        if (mc_entity_class_literal_inspect(entity->class_literal, &view) != MC_ENTITY_OK)
            return fail(c, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World spawn count Entity class",
                        true);
        if (category_matches(view.kind, category))
            total = mc_java_i32_add(total, 1);
    }
}

static bool spawning(mc_world_tick_call *c, bool hostile, bool peaceful) {
    mc_world_runtime *w = c->state.world;
    mc_world_active_chunks *set = &w->classes->spawning_chunks;
    w->classes->spawning_initialized = true; /* cq.<clinit>: Set and class literals. */
    if (!hostile && !peaceful)
        return true;
    if (!set_prepare(c, set, 13) || !candidates(c, set, true))
        return false;
    c->state.spawn_chunk_count = set->count;
    int32_t spawn[3];
    for (unsigned int axis = 0; axis < 3; ++axis) {
        c->error.child_bytecode = 151;
        c->error.descendant_bytecode = 8 + axis * 7;
        if (mc_world_info_source_spawn_get(w->fields.x, axis, &spawn[axis]) != MC_WORLD_INFO_OK)
            return fail(c, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World spawn Info point receiver",
                        false);
    }
    (void)spawn;
    static const int32_t caps[] = {70, 15, 5};
    for (int32_t category = 0; category < 3; ++category) {
        if ((category != 0 && !peaceful) || (category == 0 && !hostile))
            continue;
        c->state.spawn_category = category;
        int32_t count;
        if (!entity_count(c, category, &count))
            return false;
        const int32_t cap = mc_java_i32_mul(caps[category], (int32_t)set->count) / 256;
        if (count > cap || set->count == 0)
            continue;
        int32_t first;
        c->error.child_bytecode = 265;
        if (mc_java_int_set_first(set->order, &first) != MC_JAVA_INT_SET_OK)
            return fail(c, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World spawn candidate iterator",
                        true);
        candidate *entries = set->entries;
        const candidate *selected = NULL;
        for (size_t i = 0; i < set->count; ++i)
            if (entries[i].hash == first) {
                selected = &entries[i];
                break;
            }
        if (selected == NULL)
            return fail(c, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World spawn candidate key ledger",
                        true);
        c->state.spawn_chunk_x = selected->x;
        c->state.spawn_chunk_z = selected->z;
        c->error.child_bytecode = 276;
        c->error.descendant_bytecode = 4;
        mc_world_provider_fields dimension;
        if (mc_world_provider_fields_get(w->fields.t, &dimension) != MC_WORLD_PROVIDER_OK)
            return fail(c, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World spawn WorldProvider receiver",
                        false);
        c->error.child_bytecode = 281;
        c->error.biome = &c->biome;
        const mc_biome *biome = NULL;
        if (mc_world_chunk_manager_chunk(dimension.b, selected->x, selected->z, &biome,
                                         &c->biome) != MC_WORLD_PROVIDER_OK) {
            c->error.descendant_bytecode = c->biome.bytecode;
            const mc_world_storage_status status =
                c->biome.status == MC_WORLD_PROVIDER_ALLOCATION_FAILED
                    ? MC_WORLD_STORAGE_ALLOCATION_FAILED
                : c->biome.status == MC_WORLD_PROVIDER_LIMIT ? MC_WORLD_STORAGE_LIMIT
                                                             : MC_WORLD_STORAGE_UNSUPPORTED_STATE;
            return fail(c, status, "World spawn actual biome chunk lookup", dimension.b != NULL);
        }
        c->error.biome = NULL;
        c->error.child_bytecode = 290;
        c->error.descendant_bytecode = 0;
        return fail(c, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                    biome == NULL ? "World spawn null Biome receiver"
                                  : "World spawn actual Biome spawn-list selection",
                    biome != NULL);
    }
    c->error.child_bytecode = 888;
    return true;
}

static bool sky_callbacks(mc_world_tick_call *c) {
    mc_world_runtime *w = c->state.world;
    for (size_t i = 0;; ++i) {
        mc_world_access *access;
        bool has;
        unsigned int pc;
        mc_world_storage_error error = {0};
        if (!mc_world_access_next(w, i, 120, 133, 138, &access, &has, &pc, &error)) {
            c->state.source_pc = c->error.bytecode = pc;
            c->error.native_policy = w->fields.u != NULL;
            return cause(c, error);
        }
        if (!has)
            return true;
        at(c, 141, MC_WORLD_TICK_CHILD_ACCESS, "pm", "e()V", 0);
        mc_world_access_failure failure = {0};
        const mc_world_storage_status status =
            mc_world_access_call_sky_changed(access, w, &error, &failure);
        if (status != MC_WORLD_STORAGE_OK) {
            c->error.native_policy = true;
            const char *exception;
            if (mc_world_access_failure_source_exception(&failure, &exception)) {
                c->error.exception_class = exception;
                c->error.native_policy = false;
            }
            if (failure.receipt != NULL) {
                const mc_world_storage_status held =
                    mc_world_access_failure_retain(&failure, &c->error.world_access);
                if (held != MC_WORLD_STORAGE_OK) {
                    mc_world_access_release_held(access);
                    return fail(c, held, "World tick sky failure receipt retain", true);
                }
            }
            mc_world_access_release_held(access);
            return cause(c, mc_world_access_callback_error(status, error));
        }
        mc_world_access_release_held(access);
        ++c->state.sky_callbacks;
    }
}

static bool scheduler(mc_world_tick_call *c) {
    mc_world_runtime *w = c->state.world;
    at(c, 1, MC_WORLD_TICK_CHILD_WEATHER, "fd", "m()V", 0);
    if (mc_world_runtime_tick_weather(w, &c->weather) != MC_WORLD_STORAGE_OK) {
        c->error.weather = &c->weather;
        c->error.child_bytecode = c->weather.bytecode;
        c->error.native_policy = c->weather.bytecode == 0 ||
                                 (c->weather.bytecode != 4 && c->weather.info == MC_WORLD_INFO_OK &&
                                  w->fields.r != NULL);
        return cause(c, c->weather.cause);
    }
    c->state.weather_returned = true;
    at(c, 5, MC_WORLD_TICK_CHILD_SLEEP, "fd", "A()Z", 0);
    if (!sleep_predicate(c, &c->state.sleeping) || (c->state.sleeping && !advance_sleep(c)))
        return false;
    at(c, 80, MC_WORLD_TICK_CHILD_SPAWNING, "cq", "a(World,ZZ)I", 0);
    if (!spawning(c, w->fields.N, w->fields.O))
        return false;
    c->state.spawning_returned = true;

    at(c, 88, MC_WORLD_TICK_CHILD_PROVIDER, "ok", "a()Z", 0);
    mc_chunk_provider_fields provider;
    mc_world_storage_error error = {0};
    if (w->fields.v == NULL ||
        mc_chunk_provider_bind_fields(w->fields.v, &provider, &error) != MC_WORLD_STORAGE_OK)
        return fail(c, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World tick actual provider receiver",
                    w->fields.v != NULL);
    if (provider.world != w || provider.cache != w->lifecycle.cache ||
        provider.pending != w->lifecycle.pending || provider.store != w->options.store)
        return fail(c, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World tick actual provider binding",
                    true);
    bool provider_result;
    if (mc_world_runtime_maintain_detailed(w, &provider_result, &c->lifecycle) !=
        MC_CHUNK_LIFECYCLE_OK) {
        c->error.lifecycle = &c->lifecycle;
        c->error.native_policy = true;
        return cause(c, c->lifecycle.cause);
    }
    c->state.provider_returned = true;
    at(c, 96, MC_WORLD_TICK_CHILD_SKY, "fd", "a(F)I", 0);
    int32_t sky;
    if (mc_world_runtime_sky_subtraction(w, 1.0f, &sky, &error) != MC_WORLD_STORAGE_OK) {
        c->error.native_policy = w->fields.t != NULL && w->fields.x != NULL;
        return cause(c, error);
    }
    if (sky != w->fields.f) {
        w->fields.f = sky;
        c->state.sky_stored = true;
        if (!sky_callbacks(c))
            return false;
    }
    at(c, 156, MC_WORLD_TICK_CHILD_NONE, "ei", "f()J", 1);
    int64_t now;
    if (mc_world_info_time_get(w->fields.x, &now) != MC_WORLD_INFO_OK)
        return fail(c, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World tick Info time receiver", false);
    c->state.next_time = mc_java_i64_add(now, 1);
    int64_t remainder;
    at(c, 168, MC_WORLD_TICK_CHILD_NONE, "fd", "l()V", 168);
    if (!mc_java_i64_rem(c->state.next_time, (int64_t)w->fields.p, &remainder)) {
        c->error.exception_class = "java/lang/ArithmeticException";
        return fail(c, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World tick long remainder by zero",
                    false);
    }
    if (remainder == 0) {
        at(c, 177, MC_WORLD_TICK_CHILD_SAVE, "fd", "a(Z,yb)V", 0);
        if (mc_world_runtime_save_detailed(w, false, &c->lifecycle) != MC_CHUNK_LIFECYCLE_OK) {
            c->error.lifecycle = &c->lifecycle;
            c->error.native_policy = true;
            return cause(c, c->lifecycle.cause);
        }
    }
    at(c, 185, MC_WORLD_TICK_CHILD_NONE, "ei", "a(J)V", 2);
    if (mc_world_info_source_time_set(w->fields.x, c->state.next_time) != MC_WORLD_INFO_OK)
        return fail(c, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World tick Info time store", false);
    c->state.time_stored = true;
    at(c, 190, MC_WORLD_TICK_CHILD_SCHEDULED, "fd", "a(Z)Z", 0);
    bool pending;
    if (mc_world_runtime_drain_scheduled(w, false, &pending, &c->scheduled) !=
        MC_WORLD_STORAGE_OK) {
        c->error.scheduled = &c->scheduled;
        c->error.child_bytecode = c->scheduled.call.bytecode;
        c->error.native_policy = c->scheduled.native_policy;
        c->error.exception_class = c->scheduled.exception_class;
        return cause(c, c->scheduled.call.cause);
    }
    c->state.scheduled_returned = true;
    at(c, 195, MC_WORLD_TICK_CHILD_RANDOM, "fd", "n()V", 4);
    if (w->fields.P != NULL && w->fields.P != w->active_chunks)
        return fail(c, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                    "World random replaced candidate Set owner", true);
    if (!set_prepare(c, w->fields.P, 4) || !candidates(c, w->fields.P, false))
        return false;
    c->state.random_chunk_count = w->fields.P->count;
    if (w->fields.Q > 0)
        w->fields.Q = mc_java_i32_sub(w->fields.Q, 1);
    if (w->fields.P->count != 0) {
        c->error.child_bytecode = 207;
        return fail(c, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World random active-Chunk tick body",
                    true);
    }
    c->state.source_pc = 198;
    c->state.returned = true;
    c->error = (mc_world_tick_error){0};
    return true;
}

mc_world_storage_status mc_world_runtime_tick_begin(mc_world_runtime *w, mc_world_tick_call **out,
                                                    mc_world_tick_error *admission) {
    if (w == NULL || out == NULL || *out != NULL)
        return rejected(admission, MC_WORLD_STORAGE_INVALID_ARGUMENT, "World tick arguments");
    if (w->construction.mode != MC_WORLD_RUNTIME_GAME || w->classes == NULL ||
        w->classes->side != MC_WORLD_INFO_CLIENT ||
        w->construction.phase == MC_WORLD_RUNTIME_RETIRED)
        return rejected(admission, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                        "World tick actual client GAME owner");
    if (w->persistent_bindings == SIZE_MAX)
        return rejected(admission, MC_WORLD_STORAGE_LIMIT, "World tick binding ceiling");
    mc_world_tick_call *c = calloc(1, sizeof(*c));
    if (c == NULL)
        return rejected(admission, MC_WORLD_STORAGE_ALLOCATION_FAILED,
                        "World tick call allocation");
    mc_world_storage_error error = {0};
    if (mc_world_runtime_call_enter(w, &error) != MC_WORLD_STORAGE_OK) {
        free(c);
        return rejected(admission, error.status, error.operation);
    }
    (void)mc_world_runtime_binding_retain(w);
    c->world_held = true;
    c->state.world = w;
    c->state.active = true;
    c->state.error = &c->error;
    c->state.spawn_category = -1;
    *out = c;
    (void)rejected(admission, MC_WORLD_STORAGE_OK, NULL);
    (void)scheduler(c);
    c->state.active = false;
    w->last_error = c->error.cause;
    mc_world_runtime_call_leave(w);
    return c->error.status;
}

bool mc_world_runtime_tick_inspect(const mc_world_tick_call *c, mc_world_tick_state *out) {
    if (c == NULL || out == NULL)
        return false;
    *out = c->state;
    return true;
}

mc_world_storage_status mc_world_runtime_tick_retire(mc_world_tick_call **slot,
                                                     mc_world_tick_error *cleanup) {
    if (slot == NULL)
        return rejected(cleanup, MC_WORLD_STORAGE_INVALID_ARGUMENT, "World tick retirement slot");
    mc_world_tick_call *c = *slot;
    if (c == NULL)
        return rejected(cleanup, MC_WORLD_STORAGE_OK, NULL);
    if (c->state.active)
        return rejected(cleanup, MC_WORLD_STORAGE_BUSY, "World tick call active");
    mc_world_access_failure_release(&c->error.world_access);
    if (c->world_held)
        mc_world_runtime_binding_release(c->state.world);
    free(c);
    *slot = NULL;
    return rejected(cleanup, MC_WORLD_STORAGE_OK, NULL);
}
