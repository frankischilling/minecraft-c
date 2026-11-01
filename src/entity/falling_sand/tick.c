#include "tick_internal.h"

#include <stdlib.h>

void mc_falling_entity_tick_error_init(mc_falling_entity_tick_error *e) {
    if (e != NULL)
        memset(e, 0, sizeof(*e));
}

static bool fail(mc_falling_entity_tick_error *e, mc_entity_status status, const char *operation) {
    e->status = status;
    e->operation = operation;
    return false;
}

static bool fp(mc_falling_entity_tick_error *e) {
    return (_mm_getcsr() & 0xffc0u) == 0x1f80u ||
           fail(e, MC_ENTITY_UNSUPPORTED_FP, "FallingSand strict FP admission");
}

static bool storage(mc_falling_entity_tick_error *e, mc_world_storage_status status,
                    const char *operation) {
    return status == MC_WORLD_STORAGE_OK ||
           fail(e, status == MC_WORLD_STORAGE_LIMIT ? MC_ENTITY_LIMIT : MC_ENTITY_WORLD_FAILED,
                operation);
}

static bool world(mc_falling_entity_tick_error *e, mc_world_runtime *w) {
    mc_falling_entity_tick_work *s = e->work;
    if (w == NULL)
        return fail(e, MC_ENTITY_STATE_ERROR, "FallingSand actual null World receiver");
    if (mc_world_runtime_entities(w) != s->entity->classes ||
        (s->registry != NULL && mc_world_runtime_registry(w) != s->registry))
        return fail(e, MC_ENTITY_STATE_ERROR, "FallingSand actual World namespace");
    for (size_t i = 0; i < s->world_count; ++i)
        if (s->worlds[i] == w)
            return true;
    if (s->world_count == sizeof(s->worlds) / sizeof(s->worlds[0]) ||
        !mc_world_runtime_binding_retain(w))
        return fail(e, MC_ENTITY_LIMIT, "FallingSand captured World occurrence");
    s->worlds[s->world_count++] = w;
    return true;
}

static bool receipt(mc_falling_entity_tick_error *e, const mc_world_access_failure *source,
                    mc_world_access_failure *out) {
    if (source->receipt == NULL || out->receipt != NULL)
        return true;
    const mc_world_storage_status status = mc_world_access_failure_retain(source, out);
    if (status == MC_WORLD_STORAGE_OK)
        return true;
    e->cleanup_status = MC_ENTITY_BUSY;
    e->cleanup_operation = "FallingSand actual WorldAccess failure receipt capture";
    return false;
}

bool mc_falling_entity_tick_error_retire(mc_falling_entity_tick_error *e) {
    if (e == NULL)
        return false;
    mc_falling_entity_tick_work *s = e->work;
    if (s == NULL) {
        mc_falling_entity_tick_error_init(e);
        return true;
    }
    if (s->active) {
        e->cleanup_status = MC_ENTITY_BUSY;
        e->cleanup_operation = "FallingSand active tick retirement";
        return false;
    }
    if (!receipt(e, &s->mutation.world_access_failure, &s->mutation_receipt) ||
        !receipt(e, &s->publication.world_access_failure, &s->publication_receipt))
        return false;
    if (!mc_world_placement_error_retire(&s->placement)) {
        e->cleanup_status = MC_ENTITY_BUSY;
        e->cleanup_operation = "FallingSand placement child retirement";
        return false;
    }
    if (s->registry_held && !mc_registry_runtime_borrow_release(s->registry)) {
        e->cleanup_status = MC_ENTITY_BUSY;
        e->cleanup_operation = "FallingSand Registry borrow retirement";
        return false;
    }
    s->registry_held = false;
    mc_world_access_failure_release(&s->mutation_receipt);
    mc_world_access_failure_release(&s->publication_receipt);
    mc_entity_release(s->item);
    mc_item_stack_release(s->stack);
    if (s->entity_held)
        mc_entity_release(s->entity);
    for (size_t i = 0; i < s->world_count; ++i)
        mc_world_runtime_binding_release(s->worlds[i]);
    free(s);
    mc_falling_entity_tick_error_init(e);
    return true;
}

static bool dead(mc_falling_entity_tick_error *e, unsigned int pc) {
    e->bytecode = pc;
    e->child = MC_FALLING_TICK_CHILD_EFFECT;
    const mc_entity_status status = mc_entity_effect_dead(e->work->entity, &e->work->effect);
    if (status != MC_ENTITY_OK)
        return fail(e, status, "FallingSand inherited sn.K");
    e->child = MC_FALLING_TICK_CHILD_NONE;
    return true;
}

static bool set(mc_falling_entity_tick_error *e, unsigned int pc, int32_t x, int32_t y, int32_t z,
                int32_t id, bool *changed) {
    mc_falling_entity_tick_work *s = e->work;
    mc_world_runtime *w = s->entity->base.world;
    e->bytecode = pc;
    if (!world(e, w))
        return false;
    e->child = MC_FALLING_TICK_CHILD_MUTATION;
    const mc_world_storage_status status =
        mc_world_runtime_block_set(w, x, y, z, id, changed, &s->mutation);
    if (!storage(e, status, "FallingSand notifying World.f")) {
        (void)receipt(e, &s->mutation.world_access_failure, &s->mutation_receipt);
        return false;
    }
    e->child = MC_FALLING_TICK_CHILD_NONE;
    return true;
}

/* Exact inherited sn.b(II) -> sn.a(IIF) -> sn.a(Stack,F), with zero offset.
 * The actual canonical Item constructor owns its five Math/Random operations;
 * the caller adds no launch velocity, inventory statistic or remote check. */
static bool drop(mc_falling_entity_tick_error *e, unsigned int pc) {
    mc_falling_entity_tick_work *s = e->work;
    mc_entity *p = s->entity;
    const int32_t id = p->falling.a;
    e->bytecode = pc;
    e->drop_wrapper_bytecode = 4;
    e->drop_method = "a(IIF)Lhl;";
    e->drop_bytecode = 1;
    e->child = MC_FALLING_TICK_CHILD_STACK;
    e->stack_status = mc_item_stack_allocate(&s->stack);
    e->stack = s->stack;
    if (e->stack_status != MC_ITEM_STACK_OK)
        return fail(e,
                    e->stack_status == MC_ITEM_STACK_ALLOCATION_FAILED ? MC_ENTITY_ALLOCATION_FAILED
                                                                       : MC_ENTITY_LIMIT,
                    "FallingSand inherited drop Stack allocation");
    e->drop_bytecode = 8;
    e->stack_status = mc_item_stack_construct(s->stack, id, 1, 0);
    if (e->stack_status != MC_ITEM_STACK_OK)
        return fail(e, MC_ENTITY_STATE_ERROR, "FallingSand inherited drop Stack constructor");
    e->drop_method = "a(Liz;F)Lhl;";
    e->drop_bytecode = 0;
    e->child = MC_FALLING_TICK_CHILD_CONSTRUCTOR;
    mc_entity_status status = mc_item_entity_allocate(p->classes, &s->item, &s->constructor);
    e->item = s->item;
    if (status != MC_ENTITY_OK)
        return fail(e, status, "FallingSand inherited drop Item allocation");
    mc_world_runtime *w = p->base.world; /* sn4 before the coordinate arguments. */
    if (w != NULL && !world(e, w))
        return false;
    const double x = p->base.x;
    e->drop_bytecode = 18;
    if (!fp(e))
        return false;
    const double y = mc_travel_dadd(p->base.y, 0.0);
    const double z = p->base.z;
    e->drop_bytecode = 24;
    status = mc_item_entity_construct(s->item, w, x, y, z, s->stack, &s->constructor);
    if (status != MC_ENTITY_OK)
        return fail(e, status, "FallingSand inherited drop Item constructor");
    e->drop_bytecode = 31;
    status = mc_entity_item_pickup_delay_set(s->item, 10);
    if (status != MC_ENTITY_OK)
        return fail(e, status, "FallingSand inherited drop Item delay store");
    w = p->base.world; /* Independent sn35 read, after the constructor. */
    e->drop_bytecode = 39;
    if (!world(e, w))
        return false;
    e->child = MC_FALLING_TICK_CHILD_PUBLICATION;
    bool published = false;
    const mc_world_storage_status inserted =
        mc_world_runtime_insert_entity(w, s->item, &published, &s->publication);
    if (!storage(e, inserted, "FallingSand inherited drop World.b(Item)")) {
        (void)receipt(e, &s->publication.world_access_failure, &s->publication_receipt);
        return false;
    }
    e->child = MC_FALLING_TICK_CHILD_NONE;
    return true;
}

static bool body(mc_falling_entity_tick_error *e, const mc_entity_move_limits *limits) {
    mc_falling_entity_tick_work *s = e->work;
    mc_entity *p = s->entity;
    if (p->falling.a == 0)
        return dead(e, 8);
    p->base.aJ = p->base.x;
    p->base.aK = p->base.y;
    p->base.aL = p->base.z;
    p->falling.b = mc_java_i32_add(p->falling.b, 1);
    e->bytecode = 54;
    if (!fp(e))
        return false;
    p->base.velocity_y = mc_travel_dsub(p->base.velocity_y, .03999999910593033);
    e->bytecode = 71;
    e->child = MC_FALLING_TICK_CHILD_MOVEMENT;
    const mc_entity_status moved =
        mc_entity_move_in_context(p, s->registry, p->base.velocity_x, p->base.velocity_y,
                                  p->base.velocity_z, limits, &s->movement);
    if (moved != MC_ENTITY_OK)
        return fail(e, moved, "FallingSand inherited sn.b mover");
    e->child = MC_FALLING_TICK_CHILD_NONE;
    e->bytecode = 82;
    if (!fp(e))
        return false;
    p->base.velocity_x = mc_travel_dmul(p->base.velocity_x, .9800000190734863);
    p->base.velocity_y = mc_travel_dmul(p->base.velocity_y, .9800000190734863);
    p->base.velocity_z = mc_travel_dmul(p->base.velocity_z, .9800000190734863);
    const int32_t x = mc_math_floor_double(p->base.x), y = mc_math_floor_double(p->base.y),
                  z = mc_math_floor_double(p->base.z);
    mc_world_runtime *w = p->base.world;
    e->bytecode = 141;
    if (!world(e, w))
        return false;
    int32_t current;
    e->child = MC_FALLING_TICK_CHILD_BLOCK;
    if (!storage(
            e,
            mc_world_runtime_block_get_detailed(w, x, y, z, &current, &s->block, &s->acquisition),
            "FallingSand actual World.a block query"))
        return false;
    e->child = MC_FALLING_TICK_CHILD_NONE;
    bool changed = false;
    if (current == p->falling.a && !set(e, 159, x, y, z, 0, &changed))
        return false;
    if (p->base.aX) {
        e->bytecode = 178;
        if (!fp(e))
            return false;
        p->base.velocity_x = mc_travel_dmul(p->base.velocity_x, .699999988079071);
        p->base.velocity_z = mc_travel_dmul(p->base.velocity_z, .699999988079071);
        p->base.velocity_y = mc_travel_dmul(p->base.velocity_y, -.5);
        if (!dead(e, 207))
            return false;
        w = p->base.world;
        e->bytecode = 223;
        if (!world(e, w))
            return false;
        e->child = MC_FALLING_TICK_CHILD_PLACEMENT;
        bool can_place = false;
        if (!storage(e,
                     mc_world_runtime_can_place(w, p->falling.a, x, y, z, true, 1, &can_place,
                                                &s->placement),
                     "FallingSand actual World.a placement"))
            return false;
        e->child = MC_FALLING_TICK_CHILD_NONE;
        if (can_place) {
            w = p->base.world;
            e->bytecode = 238;
            if (!world(e, w))
                return false;
            if (s->registry == NULL || s->registry->entities != p->classes)
                return fail(e, MC_ENTITY_STATE_ERROR, "FallingSand actual gk code namespace");
            bool passes = false;
            e->child = MC_FALLING_TICK_CHILD_PREDICATE;
            if (!mc_world_mutation_falling_can_pass(w, s->registry, x, mc_java_i32_sub(y, 1), z,
                                                    &passes, &s->predicate))
                return storage(e, s->predicate.status, "FallingSand actual gk.c_ predicate");
            e->child = MC_FALLING_TICK_CHILD_NONE;
            if (!passes) {
                if (!set(e, 255, x, y, z, p->falling.a, &changed))
                    return false;
                if (changed)
                    return true;
            }
        }
        e->bytecode = 268;
        w = p->base.world;
        if (!world(e, w))
            return false;
        return w->fields.B || drop(e, 280);
    }
    if (p->falling.b > 100) {
        e->bytecode = 300;
        w = p->base.world;
        if (!world(e, w))
            return false;
        if (!w->fields.B && (!drop(e, 312) || !dead(e, 317)))
            return false;
    }
    return true;
}

mc_entity_status mc_falling_entity_tick_in_context(mc_entity *p, mc_registry_classes *registry,
                                                   const mc_entity_move_limits *limits,
                                                   mc_falling_entity_tick_error *e) {
    if (e == NULL)
        return MC_ENTITY_INVALID_ARGUMENT;
    if (e->work != NULL)
        return MC_ENTITY_BUSY;
    mc_falling_entity_tick_error_init(e);
    if (p == NULL)
        return fail(e, MC_ENTITY_INVALID_ARGUMENT, "FallingSand null owner"), e->status;
    if (p->phase != MC_ENTITY_READY)
        return fail(e, MC_ENTITY_INVALID_PHASE, "FallingSand READY owner"), e->status;
    if (p->classes == NULL || p->classes->source != MC_ENTITY_CLIENT ||
        !mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_FALLING_SAND))
        return fail(e, MC_ENTITY_STATE_ERROR, "FallingSand canonical client ju"), e->status;
    mc_falling_entity_tick_work *s = calloc(1, sizeof(*s));
    if (s == NULL)
        return fail(e, MC_ENTITY_ALLOCATION_FAILED, "FallingSand retained tick work"), e->status;
    e->work = s;
    s->entity = p;
    s->registry = registry;
    e->movement = &s->movement;
    e->block = &s->block;
    e->acquisition = &s->acquisition;
    e->mutation = &s->mutation;
    e->predicate = &s->predicate;
    e->placement = &s->placement;
    e->effect = &s->effect;
    e->constructor = &s->constructor;
    e->publication = &s->publication;
    e->registry = &s->registry_error;
    mc_entity_move_error_init(&s->movement);
    mc_world_mutation_error_init(&s->mutation);
    mc_world_mutation_error_init(&s->predicate);
    mc_entity_effect_error_init(&s->effect);
    const mc_entity_status held = mc_entity_retain(p);
    if (held != MC_ENTITY_OK)
        return fail(e, held, "FallingSand native actor occurrence"), e->status;
    s->entity_held = true;
    if (registry != NULL) {
        e->child = MC_FALLING_TICK_CHILD_REGISTRY;
        const mc_registry_status status = mc_registry_classes_retain(
            registry, MC_REGISTRY_CLIENT, p->classes->profile, &s->registry_error);
        if (status != MC_REGISTRY_OK)
            return fail(e, status == MC_REGISTRY_LIMIT ? MC_ENTITY_LIMIT : MC_ENTITY_STATE_ERROR,
                        "FallingSand Registry occurrence"),
                   e->status;
        s->registry_held = true;
    }
    e->child = MC_FALLING_TICK_CHILD_NONE;
    s->active = true;
    const bool complete = body(e, limits);
    s->active = false;
    if (!complete)
        return e->status;
    if (!mc_falling_entity_tick_error_retire(e))
        return fail(e, e->cleanup_status, e->cleanup_operation), e->status;
    return MC_ENTITY_OK;
}
