/* Actual client VC.a(Stack,Player,World,IIII)Z. These are the two bare
 * World setters, not CK placement; ordinary false results are popped. */
static void sign_at(mc_item_use_call *c, unsigned int pc) {
    at(c, "vc", use_block_descriptor, pc);
    c->error.child = (mc_item_use_site){0};
}

static bool sign_next_world(mc_item_use_call *c) {
    if (!captured(c))
        return false;
    if (c->source_epoch == SIZE_MAX)
        return fail(c, MC_ITEM_USE_LIMIT, "Sign diagnostic invocation identity", true);
    ++c->source_epoch;
    return true;
}

static mc_world_runtime_acquisition_error *sign_acquisition(mc_item_use_call *c) {
    if (c->query_count == sizeof(c->queries) / sizeof(c->queries[0])) {
        fail(c, MC_ITEM_USE_LIMIT, "Sign reached query diagnostic extent", true);
        return NULL;
    }
    mc_world_runtime_acquisition_error *query = calloc(1, sizeof(*query));
    if (query == NULL) {
        fail(c, MC_ITEM_USE_ALLOCATION, "Sign reached query diagnostic", true);
        return NULL;
    }
    mc_live_tile_entities_error_init(&query->tile_entity);
    query->registry = query->tile_entity.registry;
    mc_world_entity_load_error_init(&query->load_entity);
    mc_world_entity_batch_error_init(&query->batch);
    c->queries[c->query_count++] = query;
    c->error.acquisition = query;
    return query;
}

static bool sign_material(mc_item_use_call *c) {
    mc_item_use_state *s = &c->state;
    sign_at(c, 14);
    if (!hold_world(c) || !sign_next_world(c))
        return false;
    mc_world_runtime_acquisition_error *query = sign_acquisition(c);
    if (query == NULL)
        return false;
    c->error.material = &c->sign_material;
    const mc_world_storage_status status = mc_world_runtime_material_get_source_detailed(
        s->world, s->x, s->y, s->z, &s->sign_material, &c->sign_material, query,
        &c->error.query_provider_null);
    if (status != MC_WORLD_STORAGE_OK) {
        c->error.world = c->sign_material.cause;
        c->error.registry = c->sign_material.registry;
        c->error.child = site("fd", "f", "(III)Lln;", c->sign_material.bytecode);
        if (c->error.query_provider_null) {
            c->error.child = site("fd", "c", "(II)Llm;", 6);
            source_null(c, c->sign_material.operation);
        } else {
            fail(c, MC_ITEM_USE_WORLD, c->sign_material.operation, true);
            if (c->error.registry.status != MC_REGISTRY_OK) {
                mc_item_render_throwable throwable;
                mc_item_render_registry_classification(mc_world_runtime_registry(s->world),
                                                       &c->error.registry, &throwable,
                                                       &c->error.exception_class);
                c->error.source_exception = throwable == MC_ITEM_RENDER_SOURCE_EXCEPTION;
                c->error.source_error = throwable == MC_ITEM_RENDER_SOURCE_ERROR;
                c->error.native_policy = throwable == MC_ITEM_RENDER_NO_THROWABLE;
            }
        }
    } else
        s->sign_material_returned = true;
    if (!captured(c) || status != MC_WORLD_STORAGE_OK)
        return false;
    sign_at(c, 17);
    if (s->sign_material == NULL)
        return source_null(c, "VC actual null Material solid invocation");
    s->sign_solid = mc_material_a(s->sign_material);
    return true;
}

static bool sign_math(mc_item_use_call *c) {
    mc_item_use_state *s = &c->state;
    sign_at(c, 108);
    if (s->player == NULL)
        return source_null(c, "VC actual null Player yaw field");
    if (!hold_player(c, "VC reached Player yaw occurrence"))
        return false;
    s->sign_yaw = s->player->base.yaw;
    sign_at(c, 113);
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u)
        return fail(c, MC_ITEM_USE_FLOATING, "VC reached yaw floating environment", true);
    const float shifted = add(s->sign_yaw, 180.0f);
    const float multiplied = multiply(shifted, 16.0f);
    const float divided = divide(multiplied, 360.0f);
    const double argument = _mm_cvtsd_f64(_mm_add_sd(_mm_set_sd((double)divided), _mm_set_sd(.5)));
    sign_at(c, 125);
    if (s->math == NULL)
        return fail(c, MC_ITEM_USE_MATH, "VC reached defining Math namespace", true);
    if (mc_math_classes_retain(s->math, &c->error.math) != MC_MATH_CLASS_OK)
        return fail(c, MC_ITEM_USE_MATH, c->error.math.operation, true);
    c->math_held = true;
    if (mc_math_classes_ensure(s->math, &c->error.math) != MC_MATH_CLASS_OK) {
        c->error.child =
            site("in", "<clinit>", "()V",
                 c->error.math.source_pc < 0 ? 0u : (unsigned int)c->error.math.source_pc);
        return fail(c, MC_ITEM_USE_MATH, c->error.math.operation, true);
    }
    s->metadata = (int32_t)((uint32_t)mc_math_floor_double(argument) & 15u);
    s->metadata_returned = true;
    return true;
}

static bool sign_set(mc_item_use_call *c) {
    mc_item_use_state *s = &c->state;
    sign_at(c, s->sign_standing ? 131u : 153u);
    if (!sign_next_world(c))
        return false;
    c->mutation = malloc(sizeof(*c->mutation));
    if (c->mutation == NULL)
        return fail(c, MC_ITEM_USE_ALLOCATION, "Sign reached setter diagnostic", true);
    mc_world_mutation_error_init(c->mutation);
    c->error.mutation = c->mutation;
    const mc_world_storage_status status = mc_world_runtime_block_metadata_set(
        s->world, s->target_x, s->target_y, s->target_z, s->block_id, s->metadata, &s->setter_value,
        c->mutation);
    if (status != MC_WORLD_STORAGE_OK)
        world_failed(c, c->mutation, "b", "(IIIII)Z");
    else
        s->setter_returned = true;
    return captured(c) && status == MC_WORLD_STORAGE_OK;
}

static bool sign_tile(mc_item_use_call *c) {
    mc_item_use_state *s = &c->state;
    sign_at(c, 174);
    if (!hold_world(c) || !sign_next_world(c))
        return false;
    c->sign_tile = malloc(sizeof(*c->sign_tile));
    if (c->sign_tile == NULL)
        return fail(c, MC_ITEM_USE_ALLOCATION, "Sign reached TileEntity diagnostic", true);
    mc_live_tile_entities_error_init(c->sign_tile);
    c->error.tile = c->sign_tile;
    mc_world_runtime_acquisition_error *query = sign_acquisition(c);
    if (query == NULL)
        return false;
    s->sign_tile_entered = true;
    const mc_world_storage_status status = mc_live_tile_entities_get_source_detailed(
        s->world, s->target_x, s->target_y, s->target_z, &s->sign_tile, c->sign_tile, query,
        &c->error.query_provider_null);
    if (status != MC_WORLD_STORAGE_OK) {
        c->error.world = c->sign_tile->cause;
        c->error.registry = c->sign_tile->registry;
        c->error.child = site("fd", "b", "(III)Low;", c->sign_tile->bytecode);
        if (c->error.query_provider_null) {
            c->error.child = site("fd", "c", "(II)Llm;", 6);
            source_null(c, c->sign_tile->operation);
        } else {
            fail(c, MC_ITEM_USE_WORLD, c->sign_tile->operation, true);
            if (c->error.registry.status != MC_REGISTRY_OK)
                registry_failed(c);
        }
    } else
        s->sign_tile_returned = true;
    if (!captured(c) || status != MC_WORLD_STORAGE_OK)
        return false;
    sign_at(c, 177);
    if (s->sign_tile == NULL) {
        s->sign_cast_returned = true;
        return true;
    }
    mc_world_runtime_classes *code = c->context.world_code_classes;
    if (code == NULL || code->side != MC_WORLD_INFO_CLIENT || code->registry != s->registry)
        return fail(c, MC_ITEM_USE_UNSUPPORTED, "VC reached defining TileEntity namespace", true);
    if (mc_tile_entity_classes_borrow(code, &s->sign_classes) != MC_TILE_ENTITY_OK)
        return fail(c, MC_ITEM_USE_LIMIT, "VC reached TileEntity code occurrence", true);
    if (mc_tile_entity_class_literal_get(s->sign_classes, MC_TILE_ENTITY_CLASS_KIND_SIGN,
                                         &s->sign_expected_class) != MC_TILE_ENTITY_OK ||
        mc_tile_entity_get_class(s->sign_tile, &s->sign_actual_class) != MC_TILE_ENTITY_OK)
        return fail(c, MC_ITEM_USE_UNSUPPORTED, "VC actual canonical TileEntity Class", true);
    mc_tile_entity_class_literal_view actual = {0};
    if (mc_tile_entity_class_literal_inspect(s->sign_actual_class, &actual) != MC_TILE_ENTITY_OK ||
        actual.owner != s->sign_classes)
        return fail(c, MC_ITEM_USE_UNSUPPORTED, "VC actual TileEntity code association", true);
    if (s->sign_actual_class != s->sign_expected_class) {
        c->error.source_exception = true;
        c->error.exception_class = "java/lang/ClassCastException";
        return fail(c, MC_ITEM_USE_SOURCE_CAST, "VC actual Sign checkcast", false);
    }
    s->sign_cast_returned = true;
    return true;
}

static bool sign_player(mc_item_use_call *c) {
    mc_item_use_state *s = &c->state;
    sign_at(c, 190);
    if (s->player == NULL)
        return source_null(c, "VC actual null Player Sign invocation");
    if (!hold_player(c, "VC reached Player Sign occurrence"))
        return false;
    /* GS is an abstract, query-only selector here. No unknown subclass is
     * silently admitted as its inherited no-op body. DC is concrete storage. */
    if (!mc_entity_is_kind(s->player, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER) ||
        s->player->classes->source != MC_ENTITY_CLIENT)
        return fail(c, MC_ITEM_USE_UNSUPPORTED, "VC actual Player Sign virtual dispatch", true);
    if (c->context.sign == NULL)
        return fail(c, MC_ITEM_USE_UNSUPPORTED, "VC reached DC Sign callback", true);
    c->error.sign = &c->sign_error;
    s->sign_callback_entered = true;
    const mc_item_use_status status =
        c->context.sign(c->context.sign_context, s->player, s->sign_tile, &c->sign_error);
    if (status != MC_ITEM_USE_OK) {
        c->error.child = c->sign_error.site;
        c->error.source_exception = c->sign_error.source_exception;
        c->error.source_error = c->sign_error.source_error;
        c->error.exception_class = c->sign_error.exception_class;
        return fail(c, MC_ITEM_USE_PLAYER, c->sign_error.operation, c->sign_error.native_policy);
    }
    s->sign_callback_returned = true;
    return true;
}

static bool sign(mc_item_use_call *c) {
    mc_item_use_state *s = &c->state;
    if (s->face == 0) {
        sign_at(c, 6);
        s->result = false;
        return true;
    }
    if (!sign_material(c))
        return false;
    if (!s->sign_solid) {
        sign_at(c, 24);
        s->result = false;
        return true;
    }
    if (s->face == 1)
        s->target_y = mc_java_i32_add(s->target_y, 1);
    if (s->face == 2)
        s->target_z = mc_java_i32_sub(s->target_z, 1);
    if (s->face == 3)
        s->target_z = mc_java_i32_add(s->target_z, 1);
    if (s->face == 4)
        s->target_x = mc_java_i32_sub(s->target_x, 1);
    if (s->face == 5)
        s->target_x = mc_java_i32_add(s->target_x, 1);
    sign_at(c, 70);
    if (mc_registry_block_static_get(s->registry, "aE", &s->captured_block, &c->error.registry) !=
        MC_REGISTRY_OK)
        return registry_failed(c);
    sign_at(c, 80);
    if (s->captured_block == NULL)
        return source_null(c, "VC actual null Block can-place invocation");
    c->error.placement = &c->predicate;
    s->predicate_entered = true;
    if (mc_world_block_can_place_base(s->registry, s->captured_block, s->world, s->target_x,
                                      s->target_y, s->target_z, &s->can_place,
                                      &c->predicate) != MC_WORLD_STORAGE_OK)
        return placement_failed(c, &c->predicate);
    s->predicate_returned = true;
    if (!s->can_place) {
        sign_at(c, 87);
        s->result = false;
        return true;
    }
    s->sign_standing = s->face == 1;
    if (s->sign_standing) {
        if (!singleton_id(c, "vc", "aE", 101, 104, &s->block_id) || !sign_math(c))
            return false;
    } else {
        if (!singleton_id(c, "vc", "aJ", 145, 148, &s->block_id))
            return false;
        s->metadata = s->face;
        s->metadata_returned = true;
    }
    if (!sign_set(c))
        return false;
    sign_at(c, 159);
    if (s->stack == NULL)
        return source_null(c, "VC actual null Stack count field");
    c->error.stack = mc_item_stack_source_count_get(s->stack, &s->count_before);
    if (c->error.stack != MC_ITEM_STACK_OK)
        return fail(c, MC_ITEM_USE_STACK, "VC reached actual Stack count field", true);
    s->count_read = true;
    s->count_after = mc_java_i32_sub(s->count_before, 1);
    sign_at(c, 164);
    c->error.stack = mc_item_stack_source_count_set(s->stack, s->count_after);
    if (c->error.stack != MC_ITEM_STACK_OK)
        return fail(c, MC_ITEM_USE_STACK, "VC actual Stack count store", true);
    s->count_stored = true;
    if (!sign_tile(c) || (s->sign_tile != NULL && !sign_player(c)))
        return false;
    sign_at(c, 194);
    s->result = true;
    return true;
}
