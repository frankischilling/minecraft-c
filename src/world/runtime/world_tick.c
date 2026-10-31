#include "mc/world/runtime/world_tick.h"

#include "../../block/registry/registry_internal.h"
#include "../../entity/entity_internal.h"
#include "../../entity/player/player_internal.h"
#include "../info/tick_source_internal.h"
#include "../info/weather_source_internal.h"
#include "chunk_provider_internal.h"
#include "construction_internal.h"
#include "entity_tick_math_internal.h"
#include "mc/core/java/java.h"
#include "mc/core/math/math.h"
#include "mc/entity/mob/construction.h"
#include "mc/entity/mob/spawning.h"
#include "mc/world/runtime/entities.h"
#include "mc/world/runtime/environment.h"
#include "mc/world/runtime/random_tick.h"
#include "mc/world/runtime/saving/saving.h"
#include "runtime_internal.h"
#include "world_access_internal.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

typedef mc_world_chunk_candidate candidate;

struct mc_world_tick_call {
    mc_world_tick_state state;
    mc_world_tick_error error;
    mc_world_weather_error weather;
    mc_world_runtime_lifecycle_error lifecycle;
    mc_world_scheduled_drain_error scheduled;
    mc_world_chunk_manager_error biome;
    mc_world_spawn_pick_error spawning;
    mc_mob_error mob;
    mc_mob_spawn_state eligibility;
    mc_mob_spawn_error eligibility_error;
    mc_world_entity_error publication;
    mc_world_random_tick_call *random;
    mc_world_random_tick_state random_state;
    mc_world_random_tick_error random_admission, random_cleanup;
    bool world_held;
    bool math_held;
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

static bool floor_admit(mc_world_tick_call *c, unsigned int pc) {
    c->error.child_bytecode = pc;
    if (mc_math_classes_ensure(c->state.math, &c->error.math) != MC_MATH_CLASS_OK) {
        c->error.descendant_bytecode =
            c->error.math.source_pc < 0 ? 0 : (unsigned int)c->error.math.source_pc;
        return fail(c, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World tick MathHelper active use",
                    true);
    }
    if (mc_math_classes_table(c->state.math) != c->state.world->classes->trig)
        return fail(c, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                    "World tick actual MathHelper table binding", true);
    return fp(c, pc);
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
    ++set->version; /* HashMap.clear increments modCount even when empty. */
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
    ++set->version;
    return true;
}

static bool spawn_candidates(mc_world_tick_call *c, mc_world_active_chunks *set) {
    mc_world_runtime *w = c->state.world;
    for (size_t i = 0;; ++i) {
        size_t count;
        if (!list_size(c, w->fields.d, &w->player_membership, 25, &count))
            return false;
        if (i >= count)
            return true;
        mc_entity *player;
        if (!list_get(c, w->fields.d, &w->player_membership, i, 38, 43, true, &player))
            return false;
        c->error.child_bytecode = 50;
        if (player == NULL) {
            c->error.exception_class = "java/lang/NullPointerException";
            return fail(c, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World tick null Player position",
                        false);
        }
        if (!floor_admit(c, 56))
            return false;
        const int32_t x = mc_math_floor_double(wt_div(player->base.x, 16.0));
        const int32_t z = mc_math_floor_double(wt_div(player->base.z, 16.0));
        for (int32_t dx = -8; dx <= 8; ++dx)
            for (int32_t dz = -8; dz <= 8; ++dz)
                if (!set_add(c, set, mc_java_i32_add(x, dx), mc_java_i32_add(z, dz), 124))
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

static bool spawn_error(mc_world_tick_call *c, unsigned int pc, const char *operation, bool native,
                        const char *exception) {
    c->error.child_bytecode = pc;
    c->error.spawning = &c->spawning;
    c->spawning.status = MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    c->spawning.operation = operation;
    c->spawning.bytecode = pc;
    c->spawning.native_policy = native;
    c->spawning.exception_class = c->error.exception_class = exception;
    c->spawning.cause = (mc_world_storage_error){MC_WORLD_STORAGE_UNSUPPORTED_STATE, operation, 0};
    return fail(c, MC_WORLD_STORAGE_UNSUPPORTED_STATE, operation, native);
}

static bool spawn_draw(mc_world_tick_call *c, int32_t bound, unsigned int pc,
                       unsigned int helper_pc, int32_t *out) {
    c->error.child_bytecode = pc;
    c->error.descendant_bytecode = helper_pc;
    mc_java_random *random = c->state.world->fields.r;
    if (random == NULL)
        return spawn_error(c, pc, "World spawn null Random receiver", false,
                           "java/lang/NullPointerException");
    if (!mc_java_random_next_bounded(random, bound, out))
        return spawn_error(c, pc, "World spawn Random nonpositive bound", false,
                           "java/lang/IllegalArgumentException");
    ++c->state.spawn_random_calls;
    return true;
}

static bool spawn_solid(mc_world_tick_call *c, int32_t x, int32_t y, int32_t z, unsigned int pc,
                        unsigned int helper_pc, bool *out) {
    c->error.child_bytecode = pc;
    c->error.descendant_bytecode = helper_pc;
    if (mc_world_runtime_solid_support_get(c->state.world, x, y, z, out, &c->spawning.solid) ==
        MC_WORLD_STORAGE_OK)
        return true;
    c->error.spawning = &c->spawning;
    c->spawning.bytecode = pc;
    c->spawning.helper_bytecode = helper_pc;
    c->spawning.status = c->spawning.solid.status;
    c->spawning.cause = c->spawning.solid.cause;
    c->spawning.operation = c->spawning.solid.operation;
    c->error.native_policy = c->spawning.native_policy = true;
    return cause(c, c->spawning.cause);
}

static bool spawn_material(mc_world_tick_call *c, int32_t x, int32_t y, int32_t z, unsigned int pc,
                           unsigned int helper_pc, mc_material **out) {
    c->error.child_bytecode = pc;
    c->error.descendant_bytecode = helper_pc;
    if (mc_world_runtime_material_get_detailed(c->state.world, x, y, z, out,
                                               &c->spawning.material) == MC_WORLD_STORAGE_OK)
        return true;
    c->error.spawning = &c->spawning;
    c->spawning.bytecode = pc;
    c->spawning.helper_bytecode = helper_pc;
    c->spawning.status = c->spawning.material.status;
    c->spawning.cause = c->spawning.material.cause;
    c->spawning.operation = c->spawning.material.operation;
    c->error.native_policy = c->spawning.native_policy = true;
    return cause(c, c->spawning.cause);
}

static bool spawn_position(mc_world_tick_call *c, int32_t category, int32_t x, int32_t y, int32_t z,
                           bool *out) {
    bool solid;
    mc_material *material;
    *out = false;
    if (category == 2) {
        if (!spawn_material(c, x, y, z, 636, 15, &material))
            return false;
        if (material == NULL)
            return spawn_error(c, 636, "World water spawn null Material receiver", false,
                               "java/lang/NullPointerException");
        if (!mc_material_d(material))
            return true;
        if (!spawn_solid(c, x, mc_java_i32_add(y, 1), z, 636, 31, &solid))
            return false;
        *out = !solid;
        return true;
    }
    if (!spawn_solid(c, x, mc_java_i32_sub(y, 1), z, 636, 50, &solid))
        return false;
    if (!solid)
        return true;
    if (!spawn_solid(c, x, y, z, 636, 61, &solid))
        return false;
    if (solid)
        return true;
    if (!spawn_material(c, x, y, z, 636, 72, &material))
        return false;
    if (material == NULL)
        return spawn_error(c, 636, "World land spawn null Material receiver", false,
                           "java/lang/NullPointerException");
    if (mc_material_d(material))
        return true;
    if (!spawn_solid(c, x, mc_java_i32_add(y, 1), z, 636, 88, &solid))
        return false;
    *out = !solid;
    return true;
}

static bool spawn_player_near(mc_world_tick_call *c, double x, double y, double z, bool *out) {
    mc_world_runtime *w = c->state.world;
    double best = -1.0;
    bool found = false;
    for (size_t i = 0;; ++i) {
        size_t size;
        c->error.descendant_bytecode = 17;
        if (!list_size(c, w->fields.d, &w->player_membership, 676, &size))
            return false;
        if (i >= size) {
            *out = found;
            return true;
        }
        mc_entity *player;
        c->error.descendant_bytecode = 31;
        if (!list_get(c, w->fields.d, &w->player_membership, i, 676, 676, true, &player))
            return false;
        c->error.descendant_bytecode = 47;
        if (player == NULL)
            return spawn_error(c, 676, "World spawn nearest null Player receiver", false,
                               "java/lang/NullPointerException");
        if (!fp(c, 676))
            return false;
        const double dx = wt_sub(player->base.x, x), dy = wt_sub(player->base.y, y);
        const double dz = wt_sub(player->base.z, z);
        const double distance = wt_add(wt_add(wt_mul(dx, dx), wt_mul(dy, dy)), wt_mul(dz, dz));
        if (distance < 576.0 && (best == -1.0 || distance < best)) {
            found = true;
            best = distance;
        }
    }
}

static float spawn_fmul(float a, float b) {
    return _mm_cvtss_f32(_mm_mul_ss(_mm_set_ss(a), _mm_set_ss(b)));
}

static bool spawn_mob_failure(mc_world_tick_call *c, unsigned int pc) {
    const mc_mob_error *error = &c->mob;
    (void)spawn_error(c, pc, error->operation, error->native_policy, error->exception_class);
    c->error.mob = error;
    c->error.descendant_bytecode = error->site.pc < 0 ? 0 : (unsigned int)error->site.pc;
    const mc_world_storage_status status =
        error->status == MC_MOB_ALLOCATION ? MC_WORLD_STORAGE_ALLOCATION_FAILED
        : error->status == MC_MOB_LIMIT    ? MC_WORLD_STORAGE_LIMIT
                                           : MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    c->spawning.status = status;
    c->spawning.cause.status = status;
    return fail(c, status, error->operation, error->native_policy);
}

static void spawn_pose(mc_entity *entity, float x, float y, float z, float yaw) {
    /* Exact inherited sn.c(DDDFF), including the explicit e(DDD) call. */
    entity->base.x = wt_widen(x);
    entity->base.aJ = entity->base.x;
    entity->base.bl = entity->base.x;
    entity->base.y = wt_add(wt_widen(y), wt_widen(entity->base.offset));
    entity->base.aK = entity->base.y;
    entity->base.bm = entity->base.y;
    entity->base.z = wt_widen(z);
    entity->base.aL = entity->base.z;
    entity->base.bn = entity->base.z;
    entity->base.yaw = yaw;
    entity->base.aT = 0.0f;
    mc_entity_source_position(entity, entity->base.x, entity->base.y, entity->base.z);
}

static bool spawn_construct(mc_world_tick_call *c, mc_registry_classes *registry, float x, float y,
                            float z) {
    mc_entity_class_literal_view selected;
    if (mc_entity_class_literal_inspect(c->state.spawn_class, &selected) != MC_ENTITY_OK)
        return spawn_error(c, 773, "World spawn canonical Mob Class owner", true, NULL);
    if (!c->state.first_constructor.captured) {
        mc_world_spawn_constructor_trace *trace = &c->state.first_constructor;
        *trace = (mc_world_spawn_constructor_trace){
            .captured = true,
            .class_literal = c->state.spawn_class,
            .category = c->state.spawn_category,
            .chunk_x = c->state.spawn_chunk_x,
            .chunk_z = c->state.spawn_chunk_z,
            .initial_x = c->state.spawn_initial_x,
            .initial_y = c->state.spawn_initial_y,
            .initial_z = c->state.spawn_initial_z,
            .trial_x = c->state.spawn_trial_x,
            .trial_y = c->state.spawn_trial_y,
            .trial_z = c->state.spawn_trial_z,
            .group = c->state.spawn_group,
            .attempt = c->state.spawn_attempt,
            .total_weight = c->state.spawn_pick.total_weight,
            .ticket = c->state.spawn_pick.ticket,
            .chunk_count = c->state.spawn_chunk_count,
            .candidates_visited = c->state.spawn_candidates_visited,
            .random_calls = c->state.spawn_random_calls};
        mc_java_random *random = c->state.world->fields.r;
        if (random != NULL) {
            trace->random_present = true;
            trace->random = *random;
        }
    }
    switch (selected.kind) {
        case MC_ENTITY_CLASS_KIND_SPIDER:
        case MC_ENTITY_CLASS_KIND_ZOMBIE:
        case MC_ENTITY_CLASS_KIND_SKELETON:
        case MC_ENTITY_CLASS_KIND_CREEPER:
        case MC_ENTITY_CLASS_KIND_SLIME:
        case MC_ENTITY_CLASS_KIND_PIG:
        case MC_ENTITY_CLASS_KIND_SHEEP:
        case MC_ENTITY_CLASS_KIND_COW:
        case MC_ENTITY_CLASS_KIND_CHICKEN:
        case MC_ENTITY_CLASS_KIND_WOLF:
        case MC_ENTITY_CLASS_KIND_SQUID:
            break;
        default:
            return spawn_error(c, 773, "World spawn concrete Mob class initialization/constructor",
                               true, NULL);
    }
    /* This occurrence remains held through any reached child failure. It is
     * not published to the World before the source spawn predicate returns. */
    mc_entity_release(c->state.spawn_entity);
    c->state.spawn_entity = NULL;
    c->state.spawn_constructed = c->state.spawn_positioned = false;
    c->state.spawn_eligibility_returned = c->state.spawn_eligible = false;
    c->state.spawn_publication_returned = c->state.spawn_published = false;
    c->state.spawn_postspawn_returned = false;
    c->state.eligibility = NULL;
    c->error.child_bytecode = 773;
    if (mc_mob_allocate(c->state.world->classes->entities, registry, c->state.spawn_class,
                        &c->state.spawn_entity, &c->mob) != MC_MOB_OK ||
        mc_mob_construct(c->state.spawn_entity, c->state.world, &c->mob) != MC_MOB_OK)
        return spawn_mob_failure(c, 773);
    c->state.spawn_constructed = true;
    ++c->state.spawn_constructors;
    c->error.child_bytecode = 808;
    mc_java_random *random = c->state.world->fields.r;
    if (random == NULL)
        return spawn_error(c, 808, "World spawn null Random receiver", false,
                           "java/lang/NullPointerException");
    const float draw = mc_java_random_next_float(random);
    ++c->state.spawn_random_calls;
    if (!fp(c, 813))
        return false;
    const float yaw = spawn_fmul(draw, 360.0f);
    c->error.child_bytecode = 815;
    /* Exact inherited sn.c(DDDFF): only current yaw/pitch are assigned;
     * previous yaw is unchanged and vertical offset is added before e(DDD). */
    spawn_pose(c->state.spawn_entity, x, y, z, yaw);
    c->state.spawn_positioned = true;
    return true;
}

static bool spawn_publish(mc_world_tick_call *c, mc_entity *entity, unsigned int pc,
                          unsigned int helper_pc, bool *published) {
    c->error.child_bytecode = pc;
    c->error.descendant_bytecode = helper_pc;
    if (mc_world_runtime_insert_entity(c->state.world, entity, published, &c->publication) ==
        MC_WORLD_STORAGE_OK) {
        if (*published)
            ++c->state.spawn_publications;
        return true;
    }
    c->error.publication = &c->publication;
    return cause(c, c->publication.cause);
}

static bool spawn_postspawn(mc_world_tick_call *c, mc_registry_classes *registry, float x, float y,
                            float z) {
    mc_entity *entity = c->state.spawn_entity;
    if (mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_SPIDER)) {
        int32_t draw;
        c->state.spawn_postspawn_pc = 13;
        if (!spawn_draw(c, 100, 845, 13, &draw))
            return false;
        if (draw == 0) {
            const mc_entity_class_literal *skeleton = NULL;
            c->state.spawn_postspawn_pc = 24;
            c->error.child_bytecode = 845;
            c->error.descendant_bytecode = 24;
            if (mc_entity_class_literal_get(c->state.world->classes->entities,
                                            MC_ENTITY_CLASS_KIND_SKELETON,
                                            &skeleton) != MC_ENTITY_OK)
                return spawn_error(c, 845, "World spawn Skeleton canonical Class", true, NULL);
            if (mc_mob_allocate(c->state.world->classes->entities, registry, skeleton,
                                &c->state.spawn_jockey, &c->mob) != MC_MOB_OK ||
                mc_mob_construct(c->state.spawn_jockey, c->state.world, &c->mob) != MC_MOB_OK)
                return spawn_mob_failure(c, 845);
            if (!fp(c, 845))
                return false;
            c->state.spawn_postspawn_pc = 43;
            spawn_pose(c->state.spawn_jockey, x, y, z, entity->base.yaw);
            bool published;
            c->state.spawn_postspawn_pc = 49;
            if (!spawn_publish(c, c->state.spawn_jockey, 845, 49, &published))
                return false;
            c->state.spawn_postspawn_pc = 56;
            c->error.descendant_bytecode = 56;
            return spawn_error(c, 845, "World spawn Skeleton mount source body", true, NULL);
        }
    }
    if (mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_SHEEP)) {
        int32_t draw, color;
        c->state.spawn_postspawn_pc = 77;
        if (!spawn_draw(c, 100, 845, 77, &draw))
            return false;
        if (draw < 5)
            color = 15;
        else if (draw < 10)
            color = 7;
        else if (draw < 15)
            color = 8;
        else if (draw < 18)
            color = 12;
        else {
            if (!spawn_draw(c, 500, 845, 77, &draw))
                return false;
            color = draw == 0 ? 6 : 0;
        }
        c->state.spawn_postspawn_pc = 80;
        if (mc_mob_sheep_set_color(entity, color, &c->mob) != MC_MOB_OK)
            return spawn_mob_failure(c, 845);
    }
    c->state.spawn_postspawn_pc = 83;
    c->state.spawn_postspawn_returned = true;
    return true;
}

static bool spawn_trials(mc_world_tick_call *c, int32_t category, const int32_t spawn[3],
                         int32_t initial_x, int32_t initial_y, int32_t initial_z) {
    c->state.spawn_group_successes = 0; /* CQ520 is outside all three groups. */
    for (int32_t group = 0; group < 3; ++group) {
        c->state.spawn_group = group;
        int32_t x = initial_x, y = initial_y, z = initial_z;
        for (int32_t attempt = 0; attempt < 4; ++attempt) {
            c->state.spawn_attempt = attempt;
            int32_t a, b;
            if (!spawn_draw(c, 6, 565, 0, &a) || !spawn_draw(c, 6, 574, 0, &b))
                return false;
            x = mc_java_i32_add(x, mc_java_i32_sub(a, b));
            c->state.spawn_trial_x = x;
            if (!spawn_draw(c, 1, 588, 0, &a) || !spawn_draw(c, 1, 596, 0, &b))
                return false;
            y = mc_java_i32_add(y, mc_java_i32_sub(a, b));
            c->state.spawn_trial_y = y;
            if (!spawn_draw(c, 6, 611, 0, &a) || !spawn_draw(c, 6, 620, 0, &b))
                return false;
            z = mc_java_i32_add(z, mc_java_i32_sub(a, b));
            c->state.spawn_trial_z = z;
            bool permitted;
            if (!spawn_position(c, category, x, y, z, &permitted))
                return false;
            if (!permitted)
                continue;
            if (!fp(c, 647))
                return false;
            const float fx = wt_fadd(wt_i2f(x), 0.5f), fy = wt_i2f(y);
            const float fz = wt_fadd(wt_i2f(z), 0.5f);
            bool nearby;
            if (!spawn_player_near(c, wt_widen(fx), wt_widen(fy), wt_widen(fz), &nearby))
                return false;
            if (nearby)
                continue;
            const float dx = wt_fsub(fx, wt_i2f(spawn[0]));
            const float dy = wt_fsub(fy, wt_i2f(spawn[1]));
            const float dz = wt_fsub(fz, wt_i2f(spawn[2]));
            const float distance =
                wt_fadd(wt_fadd(spawn_fmul(dx, dx), spawn_fmul(dy, dy)), spawn_fmul(dz, dz));
            if (distance < 576.0f)
                continue;
            c->error.child_bytecode = 750;
            const mc_biome_spawn_fields *entry =
                mc_biome_spawn_fields_get(c->state.spawn_pick.entry);
            if (entry == NULL)
                return spawn_error(c, 750, "World spawn null chosen entry", false,
                                   "java/lang/NullPointerException");
            mc_registry_classes *registry = mc_world_runtime_registry(c->state.world);
            if (mc_biome_entity_class_canonical(registry, entry->a, &c->state.spawn_class,
                                                &c->spawning.registry) != MC_REGISTRY_OK) {
                c->error.spawning = &c->spawning;
                return spawn_error(c, 750, "World spawn canonical Mob Class binding", true, NULL);
            }
            if (!spawn_construct(c, registry, fx, fy, fz))
                return false;
            c->error.child_bytecode = 820;
            bool eligible;
            c->state.eligibility = &c->eligibility;
            if (mc_mob_spawn_eligible(c->state.spawn_entity, c->state.math, c->state.world->classes,
                                      &eligible, &c->eligibility,
                                      &c->eligibility_error) != MC_WORLD_STORAGE_OK) {
                const mc_mob_spawn_error *error = &c->eligibility_error;
                c->error.eligibility = error;
                c->error.descendant_bytecode =
                    error->site.pc < 0 ? 0 : (unsigned int)error->site.pc;
                c->error.exception_class = error->exception_class;
                return fail(c, error->status, error->operation, error->native_policy);
            }
            c->state.spawn_eligibility_returned = true;
            c->state.spawn_eligible = eligible;
            ++c->state.spawn_predicates;
            if (eligible) {
                c->state.spawn_group_successes = mc_java_i32_add(c->state.spawn_group_successes, 1);
                if (!spawn_publish(c, c->state.spawn_entity, 832, 0, &c->state.spawn_published))
                    return false;
                c->state.spawn_publication_returned = true;
                if (!spawn_postspawn(c, registry, fx, fy, fz))
                    return false;
                const int32_t limit =
                    mc_entity_is_kind(c->state.spawn_entity, MC_ENTITY_CLASS_KIND_WOLF) ? 8 : 4;
                if (c->state.spawn_group_successes >= limit)
                    return true; /* CQ858 bypasses total += groupSuccesses at864. */
            } else
                ++c->state.spawn_rejections;
            c->state.spawn_total =
                mc_java_i32_add(c->state.spawn_total, c->state.spawn_group_successes);
        }
    }
    return true;
}

static bool spawning(mc_world_tick_call *c, bool hostile, bool peaceful) {
    mc_world_runtime *w = c->state.world;
    mc_world_active_chunks *set = &w->classes->spawning_chunks;
    w->classes->spawning_initialized = true;
    if (!hostile && !peaceful)
        return true;
    if (!set_prepare(c, set, 13) || !spawn_candidates(c, set))
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
        /* A native iterator inventory preserves the already-captured Set order.
         * Each source next checks the captured Java-wrapping modCount first;
         * callbacks may alter the shared Set, but cannot silently retarget it. */
        const size_t length = set->count;
        const uint32_t version = set->version;
        int32_t *order = malloc(length * sizeof(*order));
        c->error.child_bytecode = 246;
        if (order == NULL)
            return fail(c, MC_WORLD_STORAGE_ALLOCATION_FAILED, "World spawn iterator storage",
                        true);
        size_t written;
        if (mc_java_int_set_copy_order(set->order, order, length, &written) != MC_JAVA_INT_SET_OK ||
            written != length) {
            free(order);
            return fail(c, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World spawn iterator inventory",
                        true);
        }
        bool ok = true;
        for (size_t index = 0; index < length; ++index) {
            if (set->version != version) {
                ok = spawn_error(c, 265, "World spawn candidate iterator modified", false,
                                 "java/util/ConcurrentModificationException");
                break;
            }
            candidate *entries = set->entries;
            candidate selected = {0};
            bool found = false;
            for (size_t i = 0; i < set->count; ++i)
                if (entries[i].hash == order[index]) {
                    selected = entries[i];
                    found = true;
                    break;
                }
            if (!found) {
                ok = spawn_error(c, 265, "World spawn candidate key ledger", true, NULL);
                break;
            }
            ++c->state.spawn_candidates_visited;
            c->state.spawn_chunk_x = selected.x;
            c->state.spawn_chunk_z = selected.z;
            c->error.child_bytecode = 276;
            c->error.descendant_bytecode = 4;
            mc_world_provider_fields dimension;
            if (mc_world_provider_fields_get(w->fields.t, &dimension) != MC_WORLD_PROVIDER_OK) {
                ok = spawn_error(c, 276, "World spawn WorldProvider receiver", false,
                                 "java/lang/NullPointerException");
                break;
            }
            c->error.child_bytecode = 281;
            c->error.biome = &c->biome;
            const mc_biome *biome = NULL;
            if (mc_world_chunk_manager_chunk(dimension.b, selected.x, selected.z, &biome,
                                             &c->biome) != MC_WORLD_PROVIDER_OK) {
                c->error.descendant_bytecode = c->biome.bytecode;
                const mc_world_storage_status status =
                    c->biome.status == MC_WORLD_PROVIDER_ALLOCATION_FAILED
                        ? MC_WORLD_STORAGE_ALLOCATION_FAILED
                    : c->biome.status == MC_WORLD_PROVIDER_LIMIT
                        ? MC_WORLD_STORAGE_LIMIT
                        : MC_WORLD_STORAGE_UNSUPPORTED_STATE;
                ok = fail(c, status, "World spawn actual biome chunk lookup", dimension.b != NULL);
                break;
            }
            c->error.biome = NULL;
            c->error.child_bytecode = 290;
            c->error.descendant_bytecode = 0;
            if (mc_world_runtime_pick_spawn_entry(w, biome, category, &c->state.spawn_pick,
                                                  &c->spawning) != MC_WORLD_STORAGE_OK) {
                c->error.spawning = &c->spawning;
                c->error.child_bytecode = c->spawning.bytecode;
                c->error.native_policy = c->spawning.native_policy;
                c->error.exception_class = c->spawning.exception_class;
                ok = cause(c, c->spawning.cause);
                break;
            }
            if (c->state.spawn_pick.entry == NULL)
                continue;
            ++c->state.spawn_random_calls;
            int32_t x, y, z, offset;
            if (!spawn_draw(c, 16, 457, 7, &offset)) {
                ok = false;
                break;
            }
            x = mc_java_i32_add(mc_java_i32_mul(selected.x, 16), offset);
            c->state.spawn_initial_x = x;
            if (!spawn_draw(c, 128, 457, 19, &y)) {
                ok = false;
                break;
            }
            c->state.spawn_initial_y = y;
            if (!spawn_draw(c, 16, 457, 31, &offset)) {
                ok = false;
                break;
            }
            z = mc_java_i32_add(mc_java_i32_mul(selected.z, 16), offset);
            c->state.spawn_initial_z = z;
            bool solid;
            if (!spawn_solid(c, x, y, z, 490, 0, &solid)) {
                ok = false;
                break;
            }
            if (solid)
                continue;
            mc_material *material;
            if (!spawn_material(c, x, y, z, 506, 0, &material)) {
                ok = false;
                break;
            }
            mc_registry_classes *registry = mc_world_runtime_registry(w);
            if (registry == NULL) {
                ok = spawn_error(c, 511, "World spawn category Material binding", true, NULL);
                break;
            }
            mc_material *expected = mc_materials_get(registry->options.materials,
                                                     category == 2 ? MC_MATERIAL_g : MC_MATERIAL_a);
            if (material != expected)
                continue;
            if (!spawn_trials(c, category, spawn, x, y, z)) {
                ok = false;
                break;
            }
        }
        free(order);
        if (!ok)
            return false;
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
    const mc_world_storage_status random_status =
        mc_world_runtime_random_tick_begin(w, c->state.math, &c->random, &c->random_admission);
    if (c->random != NULL) {
        (void)mc_world_runtime_random_tick_inspect(c->random, &c->random_state);
        c->state.random = &c->random_state;
        c->state.random_chunk_count = c->random_state.chunk_count;
    }
    if (random_status != MC_WORLD_STORAGE_OK) {
        const mc_world_random_tick_error *random_error =
            c->random == NULL ? &c->random_admission : c->random_state.error;
        c->error.random = random_error;
        c->error.child_bytecode = random_error->bytecode;
        c->error.descendant_bytecode = random_error->child_bytecode;
        c->error.native_policy = random_error->native_policy;
        c->error.exception_class = random_error->exception_class;
        return cause(c, random_error->cause);
    }
    c->state.random_returned = true;
    c->state.source_pc = 198;
    c->state.returned = true;
    c->error = (mc_world_tick_error){0};
    return true;
}

mc_world_storage_status mc_world_runtime_tick_begin(mc_world_runtime *w, mc_math_classes *math,
                                                    mc_world_tick_call **out,
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
    if (math != NULL && mc_math_classes_retain(math, &c->error.math) != MC_MATH_CLASS_OK) {
        mc_world_runtime_call_leave(w);
        free(c);
        return rejected(admission, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                        "World tick MathHelper namespace hold");
    }
    c->state.math = math;
    c->math_held = math != NULL;
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
    if (mc_world_runtime_random_tick_retire(&c->random, &c->random_cleanup) != MC_WORLD_STORAGE_OK)
        return rejected(cleanup, c->random_cleanup.status, c->random_cleanup.operation);
    if (c->math_held) {
        mc_math_classes *math = c->state.math;
        const mc_math_class_status status = mc_math_classes_release(&math, NULL);
        if (status != MC_MATH_CLASS_OK)
            return rejected(cleanup,
                            status == MC_MATH_CLASS_BUSY ? MC_WORLD_STORAGE_BUSY
                                                         : MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                            "World tick MathHelper namespace retirement");
        c->math_held = false;
        c->state.math = NULL;
    }
    mc_world_access_failure_release(&c->error.world_access);
    mc_world_access_failure_release(&c->publication.world_access_failure);
    mc_entity_release(c->state.spawn_entity);
    mc_entity_release(c->state.spawn_jockey);
    if (c->world_held)
        mc_world_runtime_binding_release(c->state.world);
    free(c);
    *slot = NULL;
    return rejected(cleanup, MC_WORLD_STORAGE_OK, NULL);
}
