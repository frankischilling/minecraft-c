#include "mc/entity/player/spawn.h"

#include "../position_internal.h"
#include "player_internal.h"
#include "tick_math_internal.h"

#include <stdlib.h>

void mc_player_spawn_error_init(mc_player_spawn_error *e) {
    if (e == NULL)
        return;
    memset(e, 0, sizeof(*e));
    mc_world_collision_error_init(&e->collision);
}

static bool fail(mc_player_spawn_error *e, mc_entity_status status, mc_player_spawn_cause cause,
                 const char *operation) {
    e->status = status;
    e->cause = cause;
    e->operation = operation;
    return false;
}

static void base_at(mc_player_spawn_error *e, unsigned int pc) {
    e->source_class = "sn";
    e->source_method = "t_()V";
    e->base_bytecode = pc;
}

static bool fp(mc_player_spawn_error *e) {
    return (_mm_getcsr() & 0xffc0u) == 0x1f80u ||
           fail(e, MC_ENTITY_UNSUPPORTED_FP, MC_PLAYER_SPAWN_CAUSE_FP,
                "Player spawn reset FP admission");
}

/* Actual inherited sn.e(DDD). Keep its coordinate stores before FP admission
 * and evaluate all six arguments before the nullable eq receiver invocation.
 * The existing public setter assumes nonnull bounds and cannot expose this
 * reached failure prefix. Canonical dc has no e(DDD) override. */
static bool position(mc_entity *p, double x, double y, double z, mc_player_spawn_error *e) {
    p->base.x = x;
    p->base.y = y;
    p->base.z = z;
    e->position_bytecode = 21;
    if (!fp(e))
        return false;
    const float half_width = at_fdiv(p->base.width, 2.0f);
    const float height = p->base.height;
    mc_entity_bounds *receiver = p->bounds; /* Actual aW read31, before arguments. */
    const mc_entity_bounds value = mc_entity_source_position_values(p, x, y, z, half_width, height);
    e->position_bytecode = 86;
    if (receiver == NULL)
        return fail(e, MC_ENTITY_STATE_ERROR, MC_PLAYER_SPAWN_CAUSE_NULL_REFERENCE,
                    "Player spawn reset null permanent bounds");
    /* Actual eq.c assigns its same receiver's six fields in this order. */
    receiver->min_x = value.min_x;
    receiver->min_y = value.min_y;
    receiver->min_z = value.min_z;
    receiver->max_x = value.max_x;
    receiver->max_y = value.max_y;
    receiver->max_z = value.max_z;
    e->position_bytecode = 90;
    return true;
}

static bool world_result(mc_player_spawn_error *e, mc_world_storage_status status,
                         const char *operation) {
    if (status == MC_WORLD_STORAGE_OK)
        return true;
    return fail(e,
                status == MC_WORLD_STORAGE_LIMIT               ? MC_ENTITY_LIMIT
                : status == MC_WORLD_STORAGE_ALLOCATION_FAILED ? MC_ENTITY_ALLOCATION_FAILED
                : status == MC_WORLD_STORAGE_BUSY              ? MC_ENTITY_BUSY
                                                               : MC_ENTITY_WORLD_FAILED,
                MC_PLAYER_SPAWN_CAUSE_WORLD, operation);
}

static bool base(mc_entity *p, mc_player_spawn_error *e) {
    base_at(e, 1);
    if (p->base.world == NULL)
        return true; /* Actual initial return7. Loop continuation targets8. */
    for (;;) {
        base_at(e, 13);
        if (!fp(e))
            return false;
        /* Actual dcmpl gives NaN -1, so unordered Y skips the loop. */
        if (!(p->base.y > 0.0))
            break;
        base_at(e, 17);
        if (e->search_work == e->max_search_work)
            return fail(e, MC_ENTITY_LIMIT, MC_PLAYER_SPAWN_CAUSE_LIMIT,
                        "Player spawn reset pending search ceiling");
        ++e->search_work;
        const double x = p->base.x, y = p->base.y, z = p->base.z;
        base_at(e, 30);
        if (!position(p, x, y, z, e))
            return false;
        mc_world_runtime *receiver = p->base.world; /* Reload34 before bounds39. */
        const mc_entity_bounds *bounds = p->bounds;
        base_at(e, 42);
        if (receiver == NULL)
            return fail(e, MC_ENTITY_STATE_ERROR, MC_PLAYER_SPAWN_CAUSE_NULL_REFERENCE,
                        "Player spawn reset null collision World");
        mc_world_collision_list_view list = {0};
        const mc_world_storage_status status =
            mc_world_runtime_collision_boxes(receiver, p, bounds, &list, &e->collision);
        if (status != MC_WORLD_STORAGE_OK) {
            e->collision_failed = true;
            return world_result(e, status, "Player spawn reset collision child");
        }
        base_at(e, 45);
        e->list_bytecode = 45;
        size_t count;
        e->list_status = mc_world_collision_list_view_count(&list, &count);
        mc_world_collision_list_view_release(&list);
        if (!world_result(e, e->list_status, "Player spawn reset collision List size"))
            return false;
        if (count == 0)
            break; /* Actual goto53->69, without reloading World/Y at loop top. */
        base_at(e, 62);
        if (!fp(e))
            return false;
        p->base.y = at_add(p->base.y, 1.0); /* Actual current Y reload58/store63. */
    }
    p->base.velocity_z = 0.0;
    p->base.velocity_y = 0.0;
    p->base.velocity_x = 0.0;
    p->base.aT = 0.0f;
    return true;
}

#ifdef MC_PLAYER_SPAWN_TEST_ALLOCATOR
extern void *mc_player_spawn_test_allocate(size_t bytes);
extern void mc_player_spawn_test_free(void *memory);
#define diagnostic_allocate mc_player_spawn_test_allocate
#define diagnostic_free mc_player_spawn_test_free
#else
#define diagnostic_allocate malloc
#define diagnostic_free free
#endif

static mc_entity_status rejected(mc_player_spawn_error *out, mc_entity_status status,
                                 const char *operation) {
    if (out != NULL) {
        mc_player_spawn_error_init(out);
        out->status = status;
        out->cause = MC_PLAYER_SPAWN_CAUSE_OWNER;
        out->operation = operation;
    }
    return status;
}

static mc_entity_status enter(mc_entity *p, const mc_player_spawn_limits *limits, bool wrapper,
                              mc_player_spawn_error *out) {
    if (p == NULL)
        return rejected(out, MC_ENTITY_INVALID_ARGUMENT, "Player spawn reset native receiver");
    if (p->phase != MC_ENTITY_READY)
        return rejected(out, MC_ENTITY_INVALID_PHASE, "Player spawn reset READY receiver");
    if (!mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER) || p->local_player == NULL ||
        p->classes->source != MC_ENTITY_CLIENT)
        return rejected(out, MC_ENTITY_INVALID_ARGUMENT, "Player spawn reset actual client dc");
    const mc_entity_status held = mc_entity_retain(p);
    if (held != MC_ENTITY_OK)
        return rejected(out, held, "Player spawn reset native hold");
    mc_player_spawn_error *e = diagnostic_allocate(sizeof(*e));
    if (e == NULL) {
        mc_entity_release(p);
        return rejected(out, MC_ENTITY_ALLOCATION_FAILED, "Player spawn reset diagnostic scratch");
    }
    mc_player_spawn_error_init(e);
    e->max_search_work =
        limits == NULL ? MC_PLAYER_SPAWN_DEFAULT_SEARCH_WORK : limits->max_search_work;
    if (wrapper) {
        e->source_class = "gs";
        e->source_method = "t_()V";
        p->base.offset = 1.62f; /* putfield3 */
        e->player_bytecode = 11;
        /* Actual virtual ls.b(FF) invokespecial3 to sn.b(FF) on canonical dc. */
        e->size_living_bytecode = 3;
        p->base.width = 0.6f;
        p->base.height = 1.8f;
        e->player_bytecode = 15;
    }
    if (base(p, e) && wrapper) {
        p->local_player->fields.living.Y = 20;
        p->local_player->fields.living.ad = 0;
    }
    mc_entity_release(p);
    const mc_entity_status result = e->status;
    if (result == MC_ENTITY_OK)
        mc_player_spawn_error_init(e);
    if (out != NULL)
        *out = *e;
    diagnostic_free(e);
    return result;
}

mc_entity_status mc_player_spawn_reset(mc_entity *p, const mc_player_spawn_limits *limits,
                                       mc_player_spawn_error *e) {
    return enter(p, limits, true, e);
}

mc_entity_status mc_player_spawn_reset_body(mc_entity *p, const mc_player_spawn_limits *limits,
                                            mc_player_spawn_error *e) {
    return enter(p, limits, false, e);
}
