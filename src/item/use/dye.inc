/* Actual KM.a(Liz;Lgs;Lfd;IIII)Z. Private reusable diagnostics describe the
 * latest child; exact returned producers remain owned through call retirement. */
static void dye_at(mc_item_use_call *c, unsigned int pc) {
    at(c, "km", use_block_descriptor, pc);
    c->error.child = (mc_item_use_site){0};
}

static bool dye_next_world(mc_item_use_call *c) {
    if (!captured(c))
        return false;
    if (c->source_epoch == SIZE_MAX)
        return fail(c, MC_ITEM_USE_LIMIT, "Dye diagnostic invocation identity", true);
    ++c->source_epoch;
    return true;
}

static bool dye_query(mc_item_use_call *c, unsigned int pc, int32_t x, int32_t y, int32_t z,
                      int32_t *out) {
    dye_at(c, pc);
    if (!hold_world(c) || !dye_next_world(c))
        return false;
    if (c->dye_query == NULL) {
        c->dye_query = malloc(sizeof(*c->dye_query));
        if (c->dye_query == NULL)
            return fail(c, MC_ITEM_USE_ALLOCATION, "Dye reached query diagnostic", true);
    }
    c->error.acquisition = c->dye_query;
    /* First detailed call installs canonical success sentinels. Every later
     * success preserves them; a failed child exits this source method. */
    const mc_world_storage_status status =
        c->dye_query_initialized ? mc_world_runtime_block_get_failure(c->state.world, x, y, z, out,
                                                                      &c->error.world, c->dye_query)
                                 : mc_world_runtime_block_get_detailed(
                                       c->state.world, x, y, z, out, &c->error.world, c->dye_query);
    c->dye_query_initialized = true;
    if (status != MC_WORLD_STORAGE_OK)
        fail(c, MC_ITEM_USE_WORLD, c->error.world.operation, true);
    return captured(c) && status == MC_WORLD_STORAGE_OK;
}

static bool dye_remote(mc_item_use_call *c, unsigned int pc, bool *remote) {
    dye_at(c, pc);
    mc_world_runtime_fields fields;
    const mc_world_storage_status status = mc_world_runtime_fields_get(c->state.world, &fields);
    if (status != MC_WORLD_STORAGE_OK) {
        c->error.world = (mc_world_storage_error){status, "Dye reached World remote field", 0};
        return fail(c, MC_ITEM_USE_WORLD, c->error.world.operation, true);
    }
    *remote = fields.B;
    return true;
}

static bool dye_consume(mc_item_use_call *c, unsigned int read_pc, unsigned int store_pc) {
    dye_at(c, read_pc);
    c->error.stack = mc_item_stack_source_count_get(c->state.stack, &c->state.count_before);
    if (c->error.stack != MC_ITEM_STACK_OK)
        return fail(c, MC_ITEM_USE_STACK, "Dye reached Stack count field", true);
    c->state.count_read = true;
    c->state.count_after = mc_java_i32_sub(c->state.count_before, 1);
    dye_at(c, store_pc);
    c->error.stack = mc_item_stack_source_count_set(c->state.stack, c->state.count_after);
    if (c->error.stack != MC_ITEM_STACK_OK)
        return fail(c, MC_ITEM_USE_STACK, "Dye actual Stack count store", true);
    c->state.count_stored = true;
    return true;
}

static bool dye_draw(mc_item_use_call *c, unsigned int static_pc, unsigned int draw_pc,
                     int32_t bound, int32_t *out) {
    mc_java_random *random = NULL;
    dye_at(c, static_pc);
    if (mc_registry_item_b_get(c->state.registry, &random, &c->error.registry) != MC_REGISTRY_OK)
        return registry_failed(c);
    dye_at(c, draw_pc);
    if (random == NULL)
        return source_null(c, "Dye actual GM.b Random invocation");
    if (!mc_java_random_next_bounded(random, bound, out))
        return fail(c, MC_ITEM_USE_ARGUMENT, "Dye reached bounded Random draw", true);
    return true;
}

static bool dye_mutation_prepare(mc_item_use_call *c) {
    if (!dye_next_world(c))
        return false;
    if (c->mutation == NULL) {
        c->mutation = malloc(sizeof(*c->mutation));
        if (c->mutation == NULL)
            return fail(c, MC_ITEM_USE_ALLOCATION, "Dye reached mutation diagnostic", true);
    }
    mc_world_mutation_error_init(c->mutation);
    c->error.mutation = c->mutation;
    c->state.setter_returned = false;
    return true;
}

static bool dye_set(mc_item_use_call *c, unsigned int pc, int32_t id, bool metadata) {
    dye_at(c, pc);
    if (!dye_mutation_prepare(c))
        return false;
    c->state.block_id = id;
    c->state.metadata = metadata ? 1 : 0;
    mc_world_storage_status status =
        metadata ? mc_world_runtime_block_metadata_set(c->state.world, c->state.target_x,
                                                       c->state.target_y, c->state.target_z, id, 1,
                                                       &c->state.setter_value, c->mutation)
                 : mc_world_runtime_block_set(c->state.world, c->state.target_x, c->state.target_y,
                                              c->state.target_z, id, &c->state.setter_value,
                                              c->mutation);
    if (status != MC_WORLD_STORAGE_OK) {
        world_failed(c, c->mutation, metadata ? "b" : "f", metadata ? "(IIIII)Z" : "(IIII)Z");
        c->error.registry = c->mutation->registry;
        if (c->error.registry.status != MC_REGISTRY_OK)
            registry_failed(c);
    } else
        c->state.setter_returned = true;
    /* Source pops the ordinary Boolean, including false. */
    return captured(c) && status == MC_WORLD_STORAGE_OK;
}

static bool dye_solid(mc_item_use_call *c, bool *out) {
    dye_at(c, 263);
    if (!dye_next_world(c))
        return false;
    if (c->dye_support == NULL) {
        c->dye_support = calloc(1, sizeof(*c->dye_support));
        if (c->dye_support == NULL)
            return fail(c, MC_ITEM_USE_ALLOCATION, "Dye reached solid-support diagnostic", true);
    }
    c->error.support = &c->dye_support->error;
    c->error.support_access = &c->dye_support->access;
    mc_world_storage_status status = mc_world_runtime_call_enter(c->state.world, &c->error.world);
    if (status != MC_WORLD_STORAGE_OK)
        return fail(c, MC_ITEM_USE_WORLD, c->error.world.operation, true);
    status =
        mc_world_solid_support_detailed_held(c->state.world, c->state.target_x, c->state.target_y,
                                             c->state.target_z, out, c->dye_support);
    mc_world_runtime_call_leave(c->state.world);
    if (status != MC_WORLD_STORAGE_OK) {
        c->error.world = c->dye_support->error.cause;
        c->error.registry = c->dye_support->error.registry;
        c->error.child = site("fd", "h", "(III)Z", c->dye_support->error.bytecode);
        fail(c, MC_ITEM_USE_WORLD, c->dye_support->error.operation, true);
        if (c->error.registry.status != MC_REGISTRY_OK)
            registry_failed(c);
    }
    return captured(c) && status == MC_WORLD_STORAGE_OK;
}

static bool dye_cast(mc_item_use_call *c, const mc_block *block, mc_block_kind kind,
                     unsigned int pc) {
    dye_at(c, pc);
    if (block == NULL)
        return true;
    if (block->classes != c->state.registry || (unsigned int)block->kind >= MC_BLOCK_KIND_COUNT)
        return fail(c, MC_ITEM_USE_ARGUMENT, "Dye actual Block code owner", true);
    if (block->kind == kind)
        return true;
    c->error.source_exception = true;
    c->error.exception_class = "java/lang/ClassCastException";
    return fail(c, MC_ITEM_USE_SOURCE_CAST, "Dye actual concrete Block checkcast", false);
}

static bool dye_sapling(mc_item_use_call *c) {
    const mc_block *block = NULL;
    dye_at(c, 39);
    if (mc_registry_block_static_get(c->state.registry, "z", &block, &c->error.registry) !=
        MC_REGISTRY_OK)
        return registry_failed(c);
    if (!dye_cast(c, block, MC_BLOCK_SAPLING, 42))
        return false;
    /* Java evaluates World.r before invoking even a null Sapling receiver. */
    dye_at(c, 53);
    mc_world_runtime_fields fields;
    const mc_world_storage_status field_status =
        mc_world_runtime_fields_get(c->state.world, &fields);
    if (field_status != MC_WORLD_STORAGE_OK) {
        c->error.world =
            (mc_world_storage_error){field_status, "Dye reached World Random field", 0};
        return fail(c, MC_ITEM_USE_WORLD, c->error.world.operation, true);
    }
    dye_at(c, 56);
    if (block == NULL)
        return source_null(c, "Dye actual Sapling growth receiver");
    c->state.captured_block = block;
    if (!dye_mutation_prepare(c))
        return false;
    mc_world_storage_status status = mc_world_runtime_call_enter(c->state.world, &c->error.world);
    if (status != MC_WORLD_STORAGE_OK)
        return fail(c, MC_ITEM_USE_WORLD, c->error.world.operation, true);
    const bool returned = mc_plant_sapling_grow(c->state.world, block, c->state.x, c->state.y,
                                                c->state.z, fields.r, c->mutation);
    mc_world_runtime_call_leave(c->state.world);
    if (!returned) {
        c->error.world = c->mutation->cause;
        c->error.registry = c->mutation->registry;
        c->error.child =
            site("he", "c", "(Lfd;IIILjava/util/Random;)V", c->mutation->hook_bytecode);
        fail(c, MC_ITEM_USE_WORLD, c->mutation->operation, true);
        if (c->error.registry.status != MC_REGISTRY_OK)
            registry_failed(c);
    }
    return captured(c) && returned && dye_consume(c, 61, 66);
}

static bool dye_crop(mc_item_use_call *c) {
    const mc_block *block = NULL;
    dye_at(c, 89);
    if (mc_registry_block_static_get(c->state.registry, "aA", &block, &c->error.registry) !=
        MC_REGISTRY_OK)
        return registry_failed(c);
    if (!dye_cast(c, block, MC_BLOCK_CROPS, 92))
        return false;
    dye_at(c, 102);
    if (block == NULL)
        return source_null(c, "Dye actual Crop maturity receiver");
    c->state.captured_block = block;
    if (!dye_mutation_prepare(c))
        return false;
    /* Actual NI.d_(World,III)V invokes notifying FD.d(IIII)V at7. */
    const mc_world_storage_status status = mc_world_runtime_metadata_set(
        c->state.world, c->state.x, c->state.y, c->state.z, 7, c->mutation);
    if (status != MC_WORLD_STORAGE_OK) {
        c->error.child = site("ni", "d_", "(Lfd;III)V", 7);
        c->error.world = c->mutation->cause;
        c->error.registry = c->mutation->registry;
        fail(c, MC_ITEM_USE_WORLD, c->mutation->operation, true);
        if (c->error.registry.status != MC_REGISTRY_OK)
            registry_failed(c);
    }
    return captured(c) && status == MC_WORLD_STORAGE_OK && dye_consume(c, 107, 112);
}

static bool dye_grass(mc_item_use_call *c) {
    if (!dye_consume(c, 137, 142))
        return false;
    for (int32_t attempt = 0; attempt < 128; ++attempt) {
        c->state.target_x = c->state.x;
        c->state.target_y = mc_java_i32_add(c->state.y, 1);
        c->state.target_z = c->state.z;
        bool rejected = false;
        for (int32_t step = 0; step < attempt / 16; ++step) {
            int32_t x, y, multiplier, z;
            if (!dye_draw(c, 185, 189, 3, &x))
                return false;
            c->state.target_x = mc_java_i32_add(c->state.target_x, mc_java_i32_sub(x, 1));
            if (!dye_draw(c, 199, 203, 3, &y) || !dye_draw(c, 208, 212, 3, &multiplier))
                return false;
            const int32_t delta = mc_java_i32_mul(mc_java_i32_sub(y, 1), multiplier) / 2;
            c->state.target_y = mc_java_i32_add(c->state.target_y, delta);
            if (!dye_draw(c, 223, 227, 3, &z))
                return false;
            c->state.target_z = mc_java_i32_add(c->state.target_z, mc_java_i32_sub(z, 1));
            int32_t below, grass;
            if (!dye_query(c, 244, c->state.target_x, mc_java_i32_sub(c->state.target_y, 1),
                           c->state.target_z, &below) ||
                !singleton_id(c, "km", "v", 247, 250, &grass))
                return false;
            if (below != grass) {
                rejected = true;
                break;
            }
            bool solid;
            if (!dye_solid(c, &solid))
                return false;
            if (solid) {
                rejected = true;
                break;
            }
        }
        if (rejected)
            continue;
        int32_t id;
        if (!dye_query(c, 285, c->state.target_x, c->state.target_y, c->state.target_z, &id))
            return false;
        if (id != 0)
            continue;
        int32_t draw;
        if (!dye_draw(c, 291, 296, 10, &draw))
            return false;
        if (draw != 0) {
            if (!singleton_id(c, "km", "Y", 309, 312, &id) || !dye_set(c, 316, id, true))
                return false;
        } else {
            if (!dye_draw(c, 323, 327, 3, &draw))
                return false;
            if (draw != 0) {
                if (!singleton_id(c, "km", "ae", 340, 343, &id) || !dye_set(c, 346, id, false))
                    return false;
            } else if (!singleton_id(c, "km", "af", 360, 363, &id) || !dye_set(c, 366, id, false))
                return false;
        }
    }
    return true;
}

static bool dye(mc_item_use_call *c) {
    dye_at(c, 1);
    c->error.stack = mc_item_stack_source_damage_get(c->state.stack, &c->state.damage);
    if (c->error.stack != MC_ITEM_STACK_OK)
        return fail(c, MC_ITEM_USE_STACK, "Dye actual Stack damage getter", true);
    if (c->state.damage != 15) {
        dye_at(c, 379);
        c->state.result = false;
        return true;
    }
    int32_t id, selected;
    if (!query_at(c, "km", 16, c->state.y, &id) || !singleton_id(c, "km", "z", 23, 26, &selected))
        return false;
    bool remote;
    if (id == selected) {
        if (!dye_remote(c, 33, &remote) || (!remote && !dye_sapling(c)))
            return false;
        dye_at(c, 70);
    } else {
        if (!singleton_id(c, "km", "aA", 73, 76, &selected))
            return false;
        if (id == selected) {
            if (!dye_remote(c, 83, &remote) || (!remote && !dye_crop(c)))
                return false;
            dye_at(c, 116);
        } else {
            if (!singleton_id(c, "km", "v", 119, 122, &selected))
                return false;
            if (id != selected) {
                dye_at(c, 379);
                c->state.result = false;
                return true;
            }
            if (!dye_remote(c, 129, &remote) || (!remote && !dye_grass(c)))
                return false;
            dye_at(c, 377);
        }
    }
    c->state.result = true;
    return true;
}
