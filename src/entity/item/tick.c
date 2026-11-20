#include "tick_internal.h"

void mc_item_entity_tick_error_init(mc_item_entity_tick_error *e) {
    if (e != NULL)
        memset(e, 0, sizeof(*e));
}

void mc_item_tick_child(mc_item_entity_tick_error *e, mc_item_entity_tick_child child) {
    e->child = child;
    memset(&e->child_error, 0, sizeof(e->child_error));
    switch (child) {
        case MC_ITEM_TICK_CHILD_BASE:
            mc_entity_tick_error_init(&e->child_error.base);
            break;
        case MC_ITEM_TICK_CHILD_MOVEMENT:
            mc_entity_move_error_init(&e->child_error.movement);
            break;
        case MC_ITEM_TICK_CHILD_SOUND:
            mc_world_movement_error_init(&e->child_error.sound);
            break;
        case MC_ITEM_TICK_CHILD_EFFECT:
            mc_entity_effect_error_init(&e->child_error.effect);
            break;
        default:
            break; /* The reached API supplies its complete typed diagnostic. */
    }
}

bool mc_item_tick_fail(mc_item_entity_tick_error *e, mc_entity_status status, const char *op) {
    e->status = status;
    e->operation = op;
    return false;
}

bool mc_item_tick_fp(mc_item_entity_tick_error *e) {
    return (_mm_getcsr() & 0xffc0u) == 0x1f80u ||
           mc_item_tick_fail(e, MC_ENTITY_UNSUPPORTED_FP, "Item tick strict FP admission");
}

bool mc_item_tick_world(mc_item_tick_scope *s, mc_world_runtime *world,
                        mc_item_entity_tick_error *e) {
    if (world == NULL)
        return mc_item_tick_fail(e, MC_ENTITY_STATE_ERROR, "Item tick null World receiver");
    return (mc_world_runtime_entities(world) == s->entity->classes &&
            (s->registry == NULL || mc_world_runtime_registry(world) == s->registry)) ||
           mc_item_tick_fail(e, MC_ENTITY_STATE_ERROR, "Item tick actual World namespace");
}

bool mc_item_tick_code(mc_item_tick_scope *s, mc_item_entity_tick_error *e) {
    return (s->registry != NULL && s->registry->entities == s->entity->classes) ||
           mc_item_tick_fail(e, MC_ENTITY_STATE_ERROR, "Item tick actual code Registry namespace");
}

bool mc_item_tick_registry(mc_item_entity_tick_error *e, mc_registry_status status) {
    if (status == MC_REGISTRY_OK) {
        e->child = MC_ITEM_TICK_CHILD_NONE;
        return true;
    }
    e->child = MC_ITEM_TICK_CHILD_REGISTRY;
    return mc_item_tick_fail(e,
                             status == MC_REGISTRY_LIMIT               ? MC_ENTITY_LIMIT
                             : status == MC_REGISTRY_ALLOCATION_FAILED ? MC_ENTITY_ALLOCATION_FAILED
                             : status == MC_REGISTRY_BUSY              ? MC_ENTITY_BUSY
                                                                       : MC_ENTITY_STATE_ERROR,
                             "Item tick actual Registry operation");
}

bool mc_item_tick_world_result(mc_item_entity_tick_error *e, mc_world_storage_status status,
                               const char *operation) {
    return status == MC_WORLD_STORAGE_OK ||
           mc_item_tick_fail(
               e, status == MC_WORLD_STORAGE_LIMIT ? MC_ENTITY_LIMIT : MC_ENTITY_WORLD_FAILED,
               operation);
}

static bool draw(mc_entity *p, float *out, mc_item_entity_tick_error *e) {
    if (p->random == NULL)
        return mc_item_tick_fail(e, MC_ENTITY_STATE_ERROR, "Item tick null Random receiver");
    *out = mc_java_random_next_float(p->random);
    return true;
}

static bool lava_bounce(mc_item_tick_scope *s, mc_item_entity_tick_error *e) {
    mc_entity *p = s->entity;
    p->base.velocity_y = 0.20000000298023224;
    float a, b;
    e->bytecode = 103;
    if (!draw(p, &a, e))
        return false;
    e->bytecode = 110;
    if (!draw(p, &b, e))
        return false;
    e->bytecode = 113;
    if (!mc_item_tick_fp(e))
        return false;
    p->base.velocity_x = mc_item_tick_widen(mc_travel_fmul(mc_travel_fsub(a, b), .2f));
    e->bytecode = 126;
    if (!draw(p, &a, e))
        return false;
    e->bytecode = 133;
    if (!draw(p, &b, e))
        return false;
    p->base.velocity_z = mc_item_tick_widen(mc_travel_fmul(mc_travel_fsub(a, b), .2f));
    mc_world_runtime *world = p->base.world; /* hl145 captured before literal/args. */
    const mc_registry_string *name = NULL;
    e->bytecode = 149;
    if (!mc_item_tick_code(s, e))
        return false;
    mc_item_tick_child(e, MC_ITEM_TICK_CHILD_REGISTRY);
    if (!mc_item_tick_registry(e, mc_registry_runtime_literal_ascii(
                                      s->registry, "random.fizz", &name, &e->child_error.registry)))
        return false;
    e->bytecode = 158;
    if (!draw(p, &a, e))
        return false;
    /* Source consumes the fifth draw before its fmul/fadd and invocation. */
    if (!mc_item_tick_fp(e))
        return false;
    const float pitch = mc_travel_fadd(2.0f, mc_travel_fmul(a, .4f));
    e->bytecode = 165;
    if (!mc_item_tick_world(s, world, e))
        return false;
    mc_item_tick_child(e, MC_ITEM_TICK_CHILD_SOUND);
    return mc_item_tick_world_result(
        e, mc_world_runtime_entity_sound(world, p, name, .4f, pitch, &e->child_error.sound),
        "Item tick actual fizz sound");
}

static bool body(mc_item_tick_scope *s, mc_item_entity_tick_error *e) {
    mc_entity *p = s->entity;
    e->bytecode = 1;
    e->super_bytecode = 1; /* sn.w_ virtual U resolves to inherited sn.U on hl. */
    mc_item_tick_child(e, MC_ITEM_TICK_CHILD_BASE);
    if (!mc_tick_item_base_body(p, s->registry, s->limits == NULL ? NULL : &s->limits->environment,
                                &e->child_error.base))
        return mc_item_tick_fail(e, e->child_error.base.status, "Item tick actual sn.w_ -> sn.U");
    e->child = MC_ITEM_TICK_CHILD_NONE;
    if (p->item.c > 0)
        p->item.c = mc_java_i32_sub(p->item.c, 1);
    p->base.aJ = p->base.x;
    p->base.aK = p->base.y;
    p->base.aL = p->base.z;
    e->bytecode = 53;
    if (!mc_item_tick_fp(e))
        return false;
    p->base.velocity_y = mc_travel_dsub(p->base.velocity_y, 0.03999999910593033);
    mc_world_runtime *world = p->base.world; /* hl58 precedes floors and invoke82. */
    const int32_t x = mc_math_floor_double(p->base.x), y = mc_math_floor_double(p->base.y),
                  z = mc_math_floor_double(p->base.z);
    e->bytecode = 82;
    if (!mc_item_tick_world(s, world, e))
        return false;
    mc_material *material = NULL, *lava = NULL;
    mc_item_tick_child(e, MC_ITEM_TICK_CHILD_MATERIAL);
    if (!mc_item_tick_world_result(e,
                                   mc_world_runtime_material_get_detailed(world, x, y, z, &material,
                                                                          &e->child_error.material),
                                   "Item tick actual World.f material"))
        return false;
    e->bytecode = 85;
    if (!mc_item_tick_code(s, e))
        return false;
    mc_item_tick_child(e, MC_ITEM_TICK_CHILD_REGISTRY);
    if (!mc_item_tick_registry(e, mc_registry_material_get(s->registry, MC_MATERIAL_h, &lava,
                                                           &e->child_error.registry)))
        return false;
    if (material == lava && !lava_bounce(s, e))
        return false;
    const double push_x = p->base.x;
    e->bytecode = 177;
    if (p->bounds == NULL)
        return mc_item_tick_fail(e, MC_ENTITY_STATE_ERROR, "Item tick push-out null bounds");
    const double low = p->bounds->min_y, high = p->bounds->max_y;
    if (!mc_item_tick_fp(e))
        return false;
    const double center_y = mc_item_tick_div(mc_travel_dadd(low, high), 2.0);
    const double push_z = p->base.z;
    e->bytecode = 196;
    if (!mc_item_tick_pushout(s, push_x, center_y, push_z, e))
        return false;
    e->bytecode = 213;
    mc_item_tick_child(e, MC_ITEM_TICK_CHILD_MOVEMENT);
    const mc_entity_status moved = mc_entity_move_in_context(
        p, s->registry, p->base.velocity_x, p->base.velocity_y, p->base.velocity_z,
        s->limits == NULL ? NULL : &s->limits->movement, &e->child_error.movement);
    if (moved != MC_ENTITY_OK)
        return mc_item_tick_fail(e, moved, "Item tick actual inherited mover");
    e->child = MC_ITEM_TICK_CHILD_NONE;
    float friction = .98f;
    if (p->base.aX) {
        friction = 0.58800006f;
        world = p->base.world;
        e->bytecode = 237;
        if (!mc_item_tick_fp(e))
            return false;
        const int32_t bx = mc_math_floor_double(p->base.x);
        e->bytecode = 244;
        if (p->bounds == NULL)
            return mc_item_tick_fail(e, MC_ENTITY_STATE_ERROR, "Item tick friction null bounds");
        const int32_t by = mc_java_i32_sub(mc_math_floor_double(p->bounds->min_y), 1),
                      bz = mc_math_floor_double(p->base.z);
        int32_t id;
        e->bytecode = 259;
        if (!mc_item_tick_world(s, world, e))
            return false;
        mc_item_tick_child(e, MC_ITEM_TICK_CHILD_BLOCK);
        if (!mc_item_tick_world_result(
                e, mc_world_runtime_block_get(world, bx, by, bz, &id, &e->child_error.block),
                "Item tick actual ground Block query"))
            return false;
        if (id > 0) {
            e->bytecode = 267;
            if (!mc_item_tick_code(s, e))
                return false;
            mc_item_tick_child(e, MC_ITEM_TICK_CHILD_REGISTRY);
            mc_registry_block_m_view array;
            if (!mc_item_tick_registry(
                    e, mc_registry_block_m_get(s->registry, &array, &e->child_error.registry)))
                return false;
            const mc_block *block = NULL;
            e->bytecode = 271;
            if (!mc_item_tick_registry(
                    e, mc_registry_block_m_at(&array, id, &block, &e->child_error.registry)))
                return false;
            const mc_block_fields *fields = mc_block_fields_get(block);
            e->bytecode = 272;
            if (fields == NULL) {
                e->child = MC_ITEM_TICK_CHILD_REGISTRY;
                e->child_error.registry.status = MC_REGISTRY_NULL_REFERENCE;
                e->child_error.registry.message = "Item tick ground Block.bB null receiver";
                return mc_item_tick_fail(e, MC_ENTITY_STATE_ERROR,
                                         "Item tick ground Block.bB null receiver");
            }
            e->bytecode = 277;
            if (!mc_item_tick_fp(e))
                return false;
            friction = mc_travel_fmul(fields->bB, .98f);
        }
    }
    e->bytecode = 286;
    if (!mc_item_tick_fp(e))
        return false;
    p->base.velocity_x = mc_travel_dmul(p->base.velocity_x, mc_item_tick_widen(friction));
    p->base.velocity_y = mc_travel_dmul(p->base.velocity_y, 0.9800000190734863);
    p->base.velocity_z = mc_travel_dmul(p->base.velocity_z, mc_item_tick_widen(friction));
    if (p->base.aX)
        p->base.velocity_y = mc_travel_dmul(p->base.velocity_y, -.5);
    p->item.e = mc_java_i32_add(p->item.e, 1);
    p->item.b = mc_java_i32_add(p->item.b, 1);
    if (p->item.b >= 6000) {
        e->bytecode = 363;
        mc_item_tick_child(e, MC_ITEM_TICK_CHILD_EFFECT);
        const mc_entity_status dead = mc_entity_effect_dead(p, &e->child_error.effect);
        if (dead != MC_ENTITY_OK)
            return mc_item_tick_fail(e, dead, "Item tick actual inherited K");
    }
    return true;
}

mc_entity_status mc_item_entity_tick_in_context(mc_entity *p, mc_registry_classes *registry,
                                                const mc_item_entity_tick_limits *limits,
                                                mc_item_entity_tick_error *out) {
    mc_item_entity_tick_error frame;
    mc_item_entity_tick_error_init(&frame);
    mc_item_tick_scope scope = {NULL, registry, limits, false};
    if (p == NULL)
        mc_item_tick_fail(&frame, MC_ENTITY_INVALID_ARGUMENT, "Item tick null owner");
    else if (p->phase != MC_ENTITY_READY)
        mc_item_tick_fail(&frame, MC_ENTITY_INVALID_PHASE, "Item tick READY owner");
    else if (p->classes->source != MC_ENTITY_CLIENT ||
             !mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_ITEM))
        mc_item_tick_fail(&frame, MC_ENTITY_STATE_ERROR, "Item tick actual canonical client hl");
    else {
        const mc_entity_status held = mc_entity_retain(p);
        if (held != MC_ENTITY_OK)
            mc_item_tick_fail(&frame, held, "Item tick lifetime hold");
        else {
            scope.entity = p;
            if (registry != NULL) {
                mc_item_tick_child(&frame, MC_ITEM_TICK_CHILD_REGISTRY);
                scope.registry_held = mc_item_tick_registry(
                    &frame,
                    mc_registry_classes_retain(registry, MC_REGISTRY_CLIENT, p->classes->profile,
                                               &frame.child_error.registry));
            }
            if (frame.status == MC_ENTITY_OK)
                (void)body(&scope, &frame);
            if (scope.registry_held && !mc_registry_runtime_borrow_release(registry))
                mc_item_tick_fail(&frame, MC_ENTITY_STATE_ERROR,
                                  "Item tick Registry borrow retirement");
            mc_entity_release(p);
        }
    }
    if (frame.status == MC_ENTITY_OK)
        mc_item_entity_tick_error_init(&frame);
    if (out != NULL)
        *out = frame;
    return frame.status;
}
