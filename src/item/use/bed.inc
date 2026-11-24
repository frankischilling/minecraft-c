/* Actual client LP.a(Stack,Player,World,IIII)Z. Its two World.b boolean
 * results are popped; only a failed invocation stops the remaining body. */
static void bed_at(mc_item_use_call *c, unsigned int pc) {
    at(c, "lp", use_block_descriptor, pc);
    c->error.child = (mc_item_use_site){0};
}

static bool bed_air(mc_item_use_call *c, unsigned int pc, int32_t x, int32_t z, bool *out) {
    bed_at(c, pc);
    if (!hold_world(c))
        return false;
    c->error.child = site("fd", "d", "(III)Z", 4);
    int32_t id;
    if (!query_xyz_at(c, "lp", pc, x, c->state.target_y, z, &id))
        return false;
    c->error.child = (mc_item_use_site){0};
    *out = id == 0;
    return true;
}

static bool bed_support(mc_item_use_call *c, size_t index, unsigned int pc, int32_t x, int32_t z,
                        bool *out) {
    bed_at(c, pc);
    if (!hold_world(c) || !captured(c))
        return false;
    if (c->source_epoch == SIZE_MAX)
        return fail(c, MC_ITEM_USE_LIMIT, "Bed support invocation identity", true);
    ++c->source_epoch;
    c->bed_support[index] = calloc(1, sizeof(*c->bed_support[index]));
    if (c->bed_support[index] == NULL)
        return fail(c, MC_ITEM_USE_ALLOCATION, "Bed reached support diagnostic", true);
    mc_world_solid_support_detail *detail = c->bed_support[index];
    c->error.support = &detail->error;
    c->error.support_access = &detail->access;
    mc_world_storage_status status = mc_world_runtime_call_enter(c->state.world, &c->error.world);
    if (status != MC_WORLD_STORAGE_OK)
        return fail(c, MC_ITEM_USE_WORLD, c->error.world.operation, true);
    status = mc_world_solid_support_detailed_held(
        c->state.world, x, mc_java_i32_sub(c->state.target_y, 1), z, out, detail);
    mc_world_runtime_call_leave(c->state.world);
    if (status != MC_WORLD_STORAGE_OK) {
        c->error.world = detail->error.cause;
        c->error.registry = detail->error.registry;
        c->error.child = site("fd", "h", "(III)Z", detail->error.bytecode);
        fail(c, MC_ITEM_USE_WORLD, detail->error.operation, true);
        if (c->error.registry.status != MC_REGISTRY_OK)
            registry_failed(c);
    }
    return captured(c) && status == MC_WORLD_STORAGE_OK;
}

static bool bed_math(mc_item_use_call *c, double argument, int32_t *direction) {
    bed_at(c, 37);
    if (c->state.math == NULL)
        return fail(c, MC_ITEM_USE_MATH, "LP reached defining Math namespace", true);
    if (mc_math_classes_retain(c->state.math, &c->error.math) != MC_MATH_CLASS_OK)
        return fail(c, MC_ITEM_USE_MATH, c->error.math.operation, true);
    c->math_held = true;
    if (mc_math_classes_ensure(c->state.math, &c->error.math) != MC_MATH_CLASS_OK) {
        c->error.child =
            site("in", "<clinit>", "()V",
                 c->error.math.source_pc < 0 ? 0u : (unsigned int)c->error.math.source_pc);
        return fail(c, MC_ITEM_USE_MATH, c->error.math.operation, true);
    }
    *direction = (int32_t)((uint32_t)mc_math_floor_double(argument) & 3u);
    return true;
}

static bool bed_set(mc_item_use_call *c, bool head) {
    mc_item_use_state *s = &c->state;
    bed_at(c, head ? 186u : 162u);
    if (s->captured_block == NULL)
        return source_null(c, "LP actual captured null Bed id field");
    const int32_t id = s->captured_block->fields.bn;
    s->block_id = id;
    bed_at(c, head ? 194u : 167u);
    if (!captured(c))
        return false;
    if (c->source_epoch == SIZE_MAX)
        return fail(c, MC_ITEM_USE_LIMIT, "Bed setter invocation identity", true);
    ++c->source_epoch;
    if (c->mutation == NULL) {
        c->mutation = malloc(sizeof(*c->mutation));
        if (c->mutation == NULL)
            return fail(c, MC_ITEM_USE_ALLOCATION, "Bed reached setter diagnostic", true);
    }
    mc_world_mutation_error_init(c->mutation);
    c->error.mutation = c->mutation;
    s->setter_returned = false;
    const mc_world_storage_status status = mc_world_runtime_block_metadata_set(
        s->world, head ? s->bed_head_x : s->target_x, s->target_y,
        head ? s->bed_head_z : s->target_z, id,
        head ? mc_java_i32_add(s->bed_direction, 8) : s->bed_direction, &s->setter_value,
        c->mutation);
    if (status != MC_WORLD_STORAGE_OK)
        world_failed(c, c->mutation, "b", "(IIIII)Z");
    else {
        s->setter_returned = true;
        if (head) {
            s->bed_head_setter_returned = true;
            s->bed_head_setter_value = s->setter_value;
        } else {
            s->bed_foot_setter_returned = true;
            s->bed_foot_setter_value = s->setter_value;
        }
    }
    return captured(c) && status == MC_WORLD_STORAGE_OK;
}

static bool bed(mc_item_use_call *c) {
    mc_item_use_state *s = &c->state;
    if (s->face != 1) {
        bed_at(c, 7);
        s->result = false;
        return true;
    }
    s->target_y = mc_java_i32_add(s->y, 1);
    bed_at(c, 14);
    if (mc_registry_block_static_get(s->registry, "T", &s->captured_block, &c->error.registry) !=
        MC_REGISTRY_OK)
        return registry_failed(c);
    bed_at(c, 17);
    if (s->captured_block != NULL && s->captured_block->classes != s->registry)
        return fail(c, MC_ITEM_USE_ARGUMENT, "LP actual captured Bed code owner", true);
    if (s->captured_block != NULL && s->captured_block->kind != MC_BLOCK_BED) {
        c->error.source_exception = true;
        c->error.exception_class = "java/lang/ClassCastException";
        return fail(c, MC_ITEM_USE_SOURCE_CAST, "LP actual Bed checkcast", false);
    }
    bed_at(c, 23);
    if (s->player == NULL)
        return source_null(c, "LP actual null Player yaw field");
    if (!hold_player(c, "LP reached Player yaw occurrence"))
        return false;
    const float yaw = s->player->base.yaw;
    bed_at(c, 28);
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u)
        return fail(c, MC_ITEM_USE_FLOATING, "LP reached yaw floating environment", true);
    const float multiplied = _mm_cvtss_f32(_mm_mul_ss(_mm_set_ss(yaw), _mm_set_ss(4.0f)));
    const float divided = divide(multiplied, 360.0f);
    const double argument = _mm_cvtsd_f64(_mm_add_sd(_mm_set_sd((double)divided), _mm_set_sd(.5)));
    if (!bed_math(c, argument, &s->bed_direction))
        return false;
    s->bed_head_x = mc_java_i32_add(s->target_x, s->bed_direction == 1   ? -1
                                                 : s->bed_direction == 3 ? 1
                                                                         : 0);
    s->bed_head_z = mc_java_i32_add(s->target_z, s->bed_direction == 0   ? 1
                                                 : s->bed_direction == 2 ? -1
                                                                         : 0);
    bool value;
    if (!bed_air(c, 92, s->target_x, s->target_z, &value))
        return false;
    if (!value)
        goto rejected;
    if (!bed_air(c, 111, s->bed_head_x, s->bed_head_z, &value))
        return false;
    if (!value)
        goto rejected;
    if (!bed_support(c, 0, 126, s->target_x, s->target_z, &value))
        return false;
    if (!value)
        goto rejected;
    if (!bed_support(c, 1, 147, s->bed_head_x, s->bed_head_z, &value))
        return false;
    if (!value)
        goto rejected;
    if (!bed_set(c, false) || !bed_set(c, true))
        return false;
    bed_at(c, 200);
    if (s->stack == NULL)
        return source_null(c, "LP actual null Stack count field");
    c->error.stack = mc_item_stack_source_count_get(s->stack, &s->count_before);
    if (c->error.stack != MC_ITEM_STACK_OK)
        return fail(c, MC_ITEM_USE_STACK, "LP reached actual Stack count field", true);
    s->count_read = true;
    s->count_after = mc_java_i32_sub(s->count_before, 1);
    bed_at(c, 205);
    c->error.stack = mc_item_stack_source_count_set(s->stack, s->count_after);
    if (c->error.stack != MC_ITEM_STACK_OK)
        return fail(c, MC_ITEM_USE_STACK, "LP actual Stack count store", true);
    s->count_stored = true;
    s->result = true;
    bed_at(c, 209);
    return true;
rejected:
    bed_at(c, 211);
    s->result = false;
    return true;
}
