#include "../../block/registry/registry_internal.h"
#include "../../world/runtime/construction_internal.h"
#include "../../world/runtime/presence_internal.h"
#include "../../world/runtime/runtime_internal.h"
#include "../classes/memory_internal.h"
#include "../construction_internal.h"
#include "../player/tick_math_internal.h"
#include "internal.h"
#include "mc/block/registry/runtime.h"
#include "mc/core/math/math.h"
#include "mc/world/runtime/entities.h"
#include <string.h>

static mc_lightning_site site(const char *cl, const char *method, const char *descriptor,
                              unsigned int pc) {
    return (mc_lightning_site){cl, method, descriptor, pc};
}

static mc_entity_status result(mc_lightning_error *e, mc_entity_status status,
                               const char *operation) {
    if (e != NULL) {
        memset(e, 0, sizeof(*e));
        e->status = status;
        e->operation = operation;
        e->native_policy = status != MC_ENTITY_OK;
    }
    return status;
}

static bool fail(mc_lightning_error *e, mc_entity_status status, const char *operation,
                 unsigned int pc) {
    e->status = status;
    e->operation = operation;
    e->site = site("c", "<init>", "(Lfd;DDD)V", pc);
    e->native_policy = !e->source_exception;
    return false;
}

static mc_entity_status world_status(mc_world_storage_status status) {
    return status == MC_WORLD_STORAGE_LIMIT               ? MC_ENTITY_LIMIT
           : status == MC_WORLD_STORAGE_ALLOCATION_FAILED ? MC_ENTITY_ALLOCATION_FAILED
           : status == MC_WORLD_STORAGE_BUSY              ? MC_ENTITY_BUSY
                                                          : MC_ENTITY_WORLD_FAILED;
}

static bool world_fail(mc_lightning_error *e, unsigned int pc) {
    return fail(e, world_status(e->world.status), e->world.operation, pc);
}

static bool registry_fire(mc_entity *entity, mc_world_runtime *world, unsigned int pc,
                          const mc_block **out) {
    mc_lightning_error *e = &entity->lightning->failure;
    mc_registry_classes *classes = world->classes == NULL ? NULL : world->classes->registry;
    e->child = MC_LIGHTNING_CHILD_REGISTRY;
    if (classes == NULL)
        return fail(e, MC_ENTITY_STATE_ERROR, "Lightning actual Fire Registry binding", pc);
    const mc_registry_status status =
        mc_registry_block_static_get(classes, "as", out, &e->detail.registry);
    if (status != MC_REGISTRY_OK) {
        mc_registry_status original = status;
        const bool previous = status == MC_REGISTRY_FAILED_INITIALIZATION;
        if (previous)
            original = classes->block_states[MC_BLOCK_BASE].failure.status;
        const bool source =
            original == MC_REGISTRY_NULL_REFERENCE || original == MC_REGISTRY_INDEX_OUT_OF_RANGE ||
            original == MC_REGISTRY_ILLEGAL_ARGUMENT || original == MC_REGISTRY_DUPLICATE_BLOCK ||
            original == MC_REGISTRY_DUPLICATE_STATISTIC;
        if (source) {
            e->source_exception = true;
            e->exception_class = previous ? "java/lang/NoClassDefFoundError"
                                          : "java/lang/ExceptionInInitializerError";
        }
        /* The complete Registry child retains its signed unknown-PC sentinel.
         * An unknown site is not a source PC0 or an unsigned UINT_MAX site. */
        if (e->detail.registry.active_initializer_class != NULL &&
            e->detail.registry.active_initializer_pc >= 0)
            e->child_site = site(e->detail.registry.active_initializer_class, "<clinit>", "()V",
                                 (unsigned int)e->detail.registry.active_initializer_pc);
        const mc_entity_status mapped = status == MC_REGISTRY_LIMIT ? MC_ENTITY_LIMIT
                                        : status == MC_REGISTRY_ALLOCATION_FAILED
                                            ? MC_ENTITY_ALLOCATION_FAILED
                                            : MC_ENTITY_STATE_ERROR;
        return fail(e, mapped, "Lightning actual Fire static active use", pc);
    }
    e->child = MC_LIGHTNING_CHILD_NONE;
    entity->lightning->constructor_pc = pc;
    return true;
}

static bool block_get(mc_entity *entity, mc_world_runtime *world, int32_t x, int32_t y, int32_t z,
                      unsigned int pc, int32_t *out) {
    mc_lightning_error *e = &entity->lightning->failure;
    e->child = MC_LIGHTNING_CHILD_BLOCK;
    if (mc_world_runtime_block_get_detailed(world, x, y, z, out, &e->world,
                                            &e->detail.acquisition) != MC_WORLD_STORAGE_OK)
        return world_fail(e, pc);
    e->child = MC_LIGHTNING_CHILD_NONE;
    return true;
}

/* Actual yq.a(World,III), including the private h and c(BlockAccess,III)
 * source order. The exact Fire object and its captured a array remain live
 * across acquiring World reads. This is not a generic Block placement API. */
static bool fire_can_place(mc_entity *entity, mc_world_runtime *world, const mc_block *fire,
                           int32_t x, int32_t y, int32_t z, unsigned int pc, bool *out) {
    mc_lightning_error *e = &entity->lightning->failure;
    e->fire_pc = e->fire_neighbor_pc = 0;
    if (fire == NULL) {
        e->source_exception = true;
        e->exception_class = "java/lang/NullPointerException";
        return fail(e, MC_ENTITY_STATE_ERROR, "Lightning null Fire can-place receiver", pc);
    }
    if (fire->kind != MC_BLOCK_FIRE)
        return fail(e, MC_ENTITY_STATE_ERROR, "Lightning actual Fire concrete receiver", pc);
    bool supported;
    e->fire_pc = 7;
    e->child = MC_LIGHTNING_CHILD_SUPPORT;
    if (mc_world_runtime_solid_support_get(world, x, mc_java_i32_sub(y, 1), z, &supported,
                                           &e->detail.support) != MC_WORLD_STORAGE_OK) {
        e->world = e->detail.support.cause;
        e->child_site = site("yq", "a", "(Lfd;III)Z", 7);
        return world_fail(e, pc);
    }
    e->child = MC_LIGHTNING_CHILD_NONE;
    if (supported) {
        *out = true;
        return true;
    }
    static const int32_t offsets[6][3] = {{1, 0, 0}, {-1, 0, 0}, {0, -1, 0},
                                          {0, 1, 0}, {0, 0, -1}, {0, 0, 1}};
    static const unsigned int calls[6] = {8, 24, 40, 56, 72, 88};
    e->fire_pc = 19;
    for (size_t i = 0; i < 6; ++i) {
        e->fire_neighbor_pc = calls[i];
        const int32_t *encouragement = fire->subtype.yq.a; /* YQ.c getfield1. */
        int32_t id;
        if (!block_get(entity, world, mc_java_i32_add(x, offsets[i][0]),
                       mc_java_i32_add(y, offsets[i][1]), mc_java_i32_add(z, offsets[i][2]), pc,
                       &id)) {
            e->child_site = site("yq", "c", "(Lxp;III)Z", 9);
            return false;
        }
        if (encouragement == NULL || id < 0 || id >= 256) {
            e->source_exception = true;
            e->exception_class = encouragement == NULL ? "java/lang/NullPointerException"
                                                       : "java/lang/ArrayIndexOutOfBoundsException";
            e->child_site = site("yq", "c", "(Lxp;III)Z", 14);
            return fail(e, MC_ENTITY_STATE_ERROR, "Lightning actual Fire encouragement array", pc);
        }
        if (encouragement[id] > 0) {
            *out = true;
            return true;
        }
    }
    *out = false;
    return true;
}

static bool ignite(mc_entity *entity, mc_world_runtime *world, int32_t x, int32_t y, int32_t z,
                   unsigned int query_pc, unsigned int static_pc, unsigned int place_pc,
                   unsigned int id_pc, unsigned int set_pc) {
    mc_lightning_error *e = &entity->lightning->failure;
    int32_t id;
    if (!block_get(entity, world, x, y, z, query_pc, &id))
        return false;
    entity->lightning->constructor_pc = query_pc;
    if (id != 0)
        return true;
    const mc_block *fire;
    if (!registry_fire(entity, world, static_pc, &fire))
        return false;
    bool supported;
    if (!fire_can_place(entity, world, fire, x, y, z, place_pc, &supported))
        return false;
    entity->lightning->constructor_pc = place_pc;
    if (!supported)
        return true;
    if (!registry_fire(entity, world, id_pc - 3u, &fire))
        return false;
    if (fire == NULL) {
        e->source_exception = true;
        e->exception_class = "java/lang/NullPointerException";
        return fail(e, MC_ENTITY_STATE_ERROR, "Lightning actual Fire ID receiver", id_pc);
    }
    const int32_t fire_id = fire->fields.bn;
    entity->lightning->constructor_pc = id_pc;
    bool changed;
    e->child = MC_LIGHTNING_CHILD_MUTATION;
    if (mc_world_runtime_block_set(world, x, y, z, fire_id, &changed, &e->detail.mutation) !=
        MC_WORLD_STORAGE_OK) {
        e->world = e->detail.mutation.cause;
        e->child_site = site("fd", "f", "(IIII)Z", e->detail.mutation.wrapper_bytecode);
        return world_fail(e, set_pc);
    }
    e->child = MC_LIGHTNING_CHILD_NONE;
    entity->lightning->constructor_pc = set_pc;
    return true;
}

static bool body(mc_entity *entity, mc_world_runtime *world, double x, double y, double z) {
    mc_lightning_entity_state *s = entity->lightning;
    mc_lightning_error *e = &s->failure;
    e->child = MC_LIGHTNING_CHILD_ENTITY;
    if (mc_entity_construct_base(entity, world, &e->entity) != MC_ENTITY_OK) {
        e->child_site = site("sn", "<init>", "(Lfd;)V", e->entity.bytecode);
        e->site = site("yu", "<init>", "(Lfd;)V", 2);
        e->status = e->entity.status;
        e->operation = e->entity.operation;
        e->native_policy = true;
        return false;
    }
    e->child = MC_LIGHTNING_CHILD_NONE;
    s->ancestor_pc = 5;
    s->a = 0;
    s->constructor_pc = 7;
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u)
        return fail(e, MC_ENTITY_UNSUPPORTED_FP, "Lightning reached pose strict FP controls", 18);
    entity->base.x = x;
    entity->base.aJ = entity->base.x;
    entity->base.bl = entity->base.x;
    entity->base.y = at_add(y, at_widen(entity->base.offset));
    entity->base.aK = entity->base.y;
    entity->base.bm = entity->base.y;
    entity->base.z = z;
    entity->base.aL = entity->base.z;
    entity->base.bn = entity->base.z;
    entity->base.yaw = 0.0f;
    entity->base.aT = 0.0f;
    mc_entity_source_position(entity, entity->base.x, entity->base.y, entity->base.z);
    s->constructor_pc = 18;
    s->b = 2;
    s->constructor_pc = 23;
    s->a = mc_java_random_next_i64(entity->random);
    ++s->own_random_calls;
    s->constructor_pc = 34;
    int32_t draw;
    if (!mc_java_random_next_bounded(entity->random, 3, &draw))
        return fail(e, MC_ENTITY_STATE_ERROR, "Lightning actual own Random nextInt3", 43);
    ++s->own_random_calls;
    s->c = mc_java_i32_add(draw, 1);
    s->constructor_pc = 48;
    if (world == NULL) {
        e->source_exception = true;
        e->exception_class = "java/lang/NullPointerException";
        return fail(e, MC_ENTITY_STATE_ERROR, "Lightning null World difficulty receiver", 52);
    }
    if (world->fields.q < 2) {
        s->constructor_pc = 262;
        return true;
    }
    const int32_t ix = mc_math_floor_double(x), iy = mc_math_floor_double(y),
                  iz = mc_math_floor_double(z);
    s->area_reached = true;
    mc_world_presence_error presence = {0};
    e->child = MC_LIGHTNING_CHILD_PRESENCE;
    if (mc_world_runtime_region_present_held(
            world, mc_java_i32_sub(ix, 10), mc_java_i32_sub(iy, 10), mc_java_i32_sub(iz, 10),
            mc_java_i32_add(ix, 10), mc_java_i32_add(iy, 10), mc_java_i32_add(iz, 10),
            &s->area_present, &presence) != MC_WORLD_STORAGE_OK) {
        e->presence_pc = presence.bytecode;
        e->provider_pc = presence.provider_bytecode;
        e->world = presence.cause;
        e->child_site = site("fd", "b", "(IIII)Z", 25);
        return world_fail(e, 76);
    }
    e->child = MC_LIGHTNING_CHILD_NONE;
    s->constructor_pc = 76;
    if (!s->area_present) {
        s->constructor_pc = 262;
        return true;
    }
    s->sampled_x = ix;
    s->sampled_y = iy;
    s->sampled_z = iz;
    if (!ignite(entity, world, ix, iy, iz, 109, 115, 125, 141, 144))
        return false;
    for (size_t attempt = 0; attempt < 4; ++attempt) {
        /* Each original floor precedes its independent own Random call. */
        int32_t coordinate = mc_math_floor_double(x);
        if (!mc_java_random_next_bounded(entity->random, 3, &draw))
            return fail(e, MC_ENTITY_STATE_ERROR, "Lightning actual neighbor X nextInt3", 166);
        ++s->own_random_calls;
        s->sampled_x = mc_java_i32_sub(mc_java_i32_add(coordinate, draw), 1);
        s->constructor_pc = 172;
        coordinate = mc_math_floor_double(y);
        if (!mc_java_random_next_bounded(entity->random, 3, &draw))
            return fail(e, MC_ENTITY_STATE_ERROR, "Lightning actual neighbor Y nextInt3", 184);
        ++s->own_random_calls;
        s->sampled_y = mc_java_i32_sub(mc_java_i32_add(coordinate, draw), 1);
        s->constructor_pc = 190;
        coordinate = mc_math_floor_double(z);
        if (!mc_java_random_next_bounded(entity->random, 3, &draw))
            return fail(e, MC_ENTITY_STATE_ERROR, "Lightning actual neighbor Z nextInt3", 202);
        ++s->own_random_calls;
        s->sampled_z = mc_java_i32_sub(mc_java_i32_add(coordinate, draw), 1);
        s->constructor_pc = 208;
        ++s->neighbor_attempts;
        if (!ignite(entity, world, s->sampled_x, s->sampled_y, s->sampled_z, 217, 223, 233, 249,
                    252))
            return false;
        s->constructor_pc = 256;
    }
    s->constructor_pc = 262;
    return true;
}

mc_entity_status mc_lightning_capture_failure(mc_entity *entity) {
    mc_lightning_entity_state *s = entity->lightning;
    mc_lightning_error *e = &s->failure;
    if (e->child != MC_LIGHTNING_CHILD_MUTATION ||
        e->detail.mutation.world_access_failure.receipt == NULL)
        return e->status;
    const mc_world_storage_status held = mc_world_access_failure_retain(
        &e->detail.mutation.world_access_failure, &s->callback_failure);
    if (held != MC_WORLD_STORAGE_OK) {
        e->source_exception = false;
        e->exception_class = NULL;
        e->native_policy = true;
        e->status = world_status(held);
        e->operation = "Lightning failed callback receipt capture";
        return e->status;
    }
    const char *exception = NULL;
    if (mc_world_access_failure_source_exception(&s->callback_failure, &exception)) {
        e->source_exception = true;
        e->exception_class = exception;
        e->native_policy = false;
    }
    return e->status;
}

void mc_lightning_dispose(mc_entity *entity) {
    mc_world_access_failure_release(&entity->lightning->callback_failure);
}

mc_entity_status mc_lightning_allocate(mc_entity_classes *classes, mc_entity **out,
                                       mc_entity_error *error) {
    return mc_entity_allocate_canonical(classes, MC_ENTITY_CLASS_KIND_C, "Lightning allocation",
                                        out, error);
}

mc_entity_status mc_lightning_construct(mc_entity *entity, mc_world_runtime *world, double x,
                                        double y, double z, mc_lightning_error *out) {
    if (!mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_C) || entity->lightning == NULL)
        return result(out, MC_ENTITY_INVALID_ARGUMENT, "Lightning actual concrete receiver");
    if (entity->phase != MC_ENTITY_RAW)
        return result(
            out, entity->phase == MC_ENTITY_CONSTRUCTING ? MC_ENTITY_BUSY : MC_ENTITY_INVALID_PHASE,
            "Lightning one-shot RAW receiver");
    if (world != NULL && mc_world_runtime_entities(world) != entity->classes)
        return result(out, MC_ENTITY_INVALID_ARGUMENT, "Lightning exact World Entity namespace");
    mc_world_storage_error admission = {0};
    if (world != NULL && mc_world_runtime_call_enter(world, &admission) != MC_WORLD_STORAGE_OK) {
        const mc_entity_status status = world_status(admission.status);
        result(out, status, admission.operation);
        if (out != NULL)
            out->world = admission;
        return status;
    }
    mc_entity_error begin = {0};
    mc_entity_status status =
        mc_entity_constructor_begin(entity, MC_ENTITY_CLASS_KIND_C, world, "Lightning constructor",
                                    "Lightning World profile", &begin);
    if (status != MC_ENTITY_OK) {
        if (world != NULL)
            mc_world_runtime_call_leave(world);
        result(out, status, begin.operation);
        if (out != NULL)
            out->entity = begin;
        return status;
    }
    mc_lightning_error *e = &entity->lightning->failure;
    result(e, MC_ENTITY_OK, NULL);
    if (status == MC_ENTITY_OK) {
        const bool returned = body(entity, world, x, y, z);
        if (!returned)
            (void)mc_lightning_capture_failure(entity);
        entity->phase = returned ? MC_ENTITY_READY : MC_ENTITY_FAILED_CONSTRUCTION;
        if (returned)
            result(e, MC_ENTITY_OK, NULL);
        status = e->status;
    }
    if (world != NULL)
        mc_world_runtime_call_leave(world);
    if (out != NULL)
        *out = *e;
    return status;
}

bool mc_lightning_inspect(const mc_entity *entity, mc_lightning_state *out) {
    if (!mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_C) || entity->lightning == NULL ||
        out == NULL)
        return false;
    const mc_lightning_entity_state *s = entity->lightning;
    *out = (mc_lightning_state){entity->phase,
                                s->a,
                                s->b,
                                s->c,
                                s->ancestor_pc,
                                s->constructor_pc,
                                s->neighbor_attempts,
                                s->own_random_calls,
                                s->sampled_x,
                                s->sampled_y,
                                s->sampled_z,
                                s->area_reached,
                                s->area_present,
                                &s->failure};
    return true;
}

mc_entity_status mc_lightning_retire(mc_entity **slot, mc_entity_error *error) {
    if (slot == NULL)
        return mc_entity_error_set(error, MC_ENTITY_INVALID_ARGUMENT, "Lightning terminal slot", 0);
    if (*slot == NULL)
        return mc_entity_error_set(error, MC_ENTITY_OK, NULL, 0);
    mc_entity *entity = *slot;
    if (!mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_C) || entity->lightning == NULL)
        return mc_entity_error_set(error, MC_ENTITY_INVALID_ARGUMENT, "Lightning terminal receiver",
                                   0);
    if (entity->phase == MC_ENTITY_CONSTRUCTING || entity->holds != 1)
        return mc_entity_error_set(error, MC_ENTITY_BUSY, "Lightning active/external occurrence",
                                   0);
    mc_entity_release(entity);
    *slot = NULL;
    return mc_entity_error_set(error, MC_ENTITY_OK, NULL, 0);
}
