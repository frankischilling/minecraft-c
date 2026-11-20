/* Actual client XQ.a(World,III,Living), Code83. These float operations are
 * deliberately separate; double yaw/90 would change the reached operands.
 * There is no World.B guard in this override. */
static bool dispenser_placed_by(mc_world_placement_error *e, mc_world_runtime *world, int32_t x,
                                int32_t y, int32_t z, mc_entity *living, mc_math_classes *math) {
    e->site = site("xq", "a", "(Lfd;IIILls;)V", 2);
    if (living == NULL) {
        null_reference(e, "XQ actual null Living yaw field receiver");
        return false;
    }
    if (mc_entity_living_fields(living) == NULL) {
        fail(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "XQ represented Living yaw storage", true);
        return false;
    }
    const mc_entity_status held = mc_entity_retain(living);
    if (held != MC_ENTITY_OK) {
        fail(e, held == MC_ENTITY_LIMIT ? MC_WORLD_STORAGE_LIMIT : MC_WORLD_STORAGE_BUSY,
             "XQ captured Living native occurrence", true);
        return false;
    }
    e->work->placed_by_living = living;
    const float yaw = living->base.yaw;
    e->site.pc = 7;
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u) {
        fail(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "XQ reached yaw floating environment", true);
        return false;
    }
    const float multiplied = _mm_cvtss_f32(_mm_mul_ss(_mm_set_ss(yaw), _mm_set_ss(4.0f)));
    const float divided = _mm_cvtss_f32(_mm_div_ss(_mm_set_ss(multiplied), _mm_set_ss(360.0f)));
    const double argument = _mm_cvtsd_f64(_mm_add_sd(_mm_set_sd((double)divided), _mm_set_sd(.5)));
    e->site.pc = 16;
    if (math == NULL) {
        fail(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "XQ reached defining Math namespace", true);
        return false;
    }
    if (mc_math_classes_retain(math, &e->math) != MC_MATH_CLASS_OK) {
        fail(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE, e->math.operation, true);
        return false;
    }
    e->work->math = math;
    if (mc_math_classes_ensure(math, &e->math) != MC_MATH_CLASS_OK) {
        e->caller = e->site;
        e->site = site("in", "<clinit>", "()V",
                       e->math.source_pc < 0 ? 0u : (unsigned int)e->math.source_pc);
        fail(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE, e->math.operation, true);
        return false;
    }
    const uint32_t direction = (uint32_t)mc_math_floor_double(argument) & 3u;
    static const int32_t metadata[] = {2, 5, 3, 4};
    static const unsigned int pcs[] = {34, 49, 64, 79};
    e->site.pc = pcs[direction];
    if (!hold_world(e, world))
        return false;
    e->work->mutation = malloc(sizeof(*e->work->mutation));
    if (e->work->mutation == NULL) {
        fail(e, MC_WORLD_STORAGE_ALLOCATION_FAILED, "Dispenser placed-by metadata diagnostic",
             true);
        return false;
    }
    e->mutation = e->work->mutation;
    const mc_world_storage_status changed =
        mc_world_runtime_metadata_set(world, x, y, z, metadata[direction], e->work->mutation);
    if (changed != MC_WORLD_STORAGE_OK) {
        fail(e, changed, e->mutation->operation, true);
        e->cause = e->mutation->cause;
        e->registry = e->mutation->registry;
        if (e->registry.status != MC_REGISTRY_OK)
            registry_value_failed(e, &e->mutation->registry_failure,
                                  mc_world_runtime_registry(world));
    }
    if (!capture(e)) {
        if (changed == MC_WORLD_STORAGE_OK)
            fail(e, e->cleanup.status, e->cleanup.operation, true);
        return false;
    }
    if (changed != MC_WORLD_STORAGE_OK) {
        classify_receipts(e);
        return false;
    }
    e->site.pc = 82;
    return true;
}

/* Actual client XQ.a(World,III,Player), Code31. The detailed facade performs
 * the same single ordinary getter and preserves its real acquisition cause. */
static bool dispenser_activate(mc_world_placement_error *e, mc_world_runtime *world, int32_t x,
                               int32_t y, int32_t z, mc_entity *player,
                               const mc_world_block_activation_context *context) {
    e->site = site("xq", "a", "(Lfd;IIILgs;)Z", 1);
    if (!hold_world(e, world))
        return false;
    e->remote_read = true;
    e->remote = world->fields.B;
    if (e->remote) {
        e->site.pc = 8;
        return true;
    }
    e->site.pc = 14;
    dispenser_activation_work *f = calloc(1, sizeof(*f));
    if (f == NULL) {
        fail(e, MC_WORLD_STORAGE_ALLOCATION_FAILED, "Dispenser activation retained Tile child",
             true);
        return false;
    }
    e->work->dispenser = f;
    mc_live_tile_entities_error_init(&f->tile_error);
    e->dispenser_tile = &f->tile_error;
    mc_world_runtime_acquisition_error *query = calloc(1, sizeof(*query));
    if (query == NULL) {
        fail(e, MC_WORLD_STORAGE_ALLOCATION_FAILED, "Dispenser reached getter diagnostic", true);
        return false;
    }
    mc_live_tile_entities_error_init(&query->tile_entity);
    query->registry = query->tile_entity.registry;
    mc_world_entity_load_error_init(&query->load_entity);
    mc_world_entity_batch_error_init(&query->batch);
    e->work->queries[0] = query;
    e->queries[0] = e->acquisition = query;
    e->query_count = 1;
    e->dispenser_tile_entered = true;
    const mc_world_storage_status queried = mc_live_tile_entities_get_source_detailed(
        world, x, y, z, &f->tile, &f->tile_error, query, &e->dispenser_provider_null);
    if (queried != MC_WORLD_STORAGE_OK) {
        fail(e, queried, f->tile_error.operation, true);
        e->cause = f->tile_error.cause;
        e->registry = f->tile_error.registry;
        e->dispenser_getter_child = site("fd", "b", "(III)Low;", f->tile_error.bytecode);
        if (e->dispenser_provider_null) {
            null_reference(e, f->tile_error.operation);
            e->dispenser_getter_child = site("fd", "c", "(II)Llm;", 6);
        } else if (e->registry.status != MC_REGISTRY_OK)
            (void)registry_value_failed(e, &f->tile_error.registry_failure,
                                        mc_world_runtime_registry(world));
    } else
        e->dispenser_tile_returned = true;
    if (!capture(e)) {
        if (queried == MC_WORLD_STORAGE_OK)
            fail(e, e->cleanup.status, e->cleanup.operation, true);
        return false;
    }
    if (queried != MC_WORLD_STORAGE_OK) {
        classify_receipts(e);
        return false;
    }
    e->site.pc = 17;
    if (f->tile != NULL) {
        mc_world_runtime_classes *code = context == NULL ? NULL : context->block_code_classes;
        if (code == NULL || code->side != MC_WORLD_INFO_CLIENT ||
            code->registry != e->work->registry) {
            fail(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "XQ reached defining AZ Class namespace",
                 true);
            return false;
        }
        const mc_tile_entity_status borrowed = mc_tile_entity_classes_borrow(code, &f->classes);
        if (borrowed != MC_TILE_ENTITY_OK) {
            fail(e,
                 borrowed == MC_TILE_ENTITY_ALLOCATION_FAILED ? MC_WORLD_STORAGE_ALLOCATION_FAILED
                 : borrowed == MC_TILE_ENTITY_LIMIT           ? MC_WORLD_STORAGE_LIMIT
                                                              : MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                 "XQ target Class native occurrence", true);
            return false;
        }
        const mc_tile_entity_class_literal *expected = NULL, *actual = NULL;
        mc_tile_entity_class_literal_view view = {0};
        if (mc_tile_entity_class_literal_get(f->classes, MC_TILE_ENTITY_CLASS_KIND_DISPENSER,
                                             &expected) != MC_TILE_ENTITY_OK ||
            mc_tile_entity_get_class(f->tile, &actual) != MC_TILE_ENTITY_OK ||
            mc_tile_entity_class_literal_inspect(actual, &view) != MC_TILE_ENTITY_OK) {
            fail(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                 "XQ actual TileEntity canonical Class association", true);
            return false;
        }
        if (actual != expected) {
            fail(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "XQ actual AZ checkcast", false);
            e->source_exception = true;
            e->exception_class = "java/lang/ClassCastException";
            return false;
        }
        e->dispenser = &f->inventory_error;
        const mc_inventory_status adapted = mc_inventory_dispenser_from_tile(
            MC_REGISTRY_CLIENT, f->tile, NULL, &f->inventory, &f->inventory_error);
        if (adapted != MC_INVENTORY_OK) {
            fail(e,
                 adapted == MC_INVENTORY_ALLOCATION_FAILED ? MC_WORLD_STORAGE_ALLOCATION_FAILED
                 : adapted == MC_INVENTORY_LIMIT           ? MC_WORLD_STORAGE_LIMIT
                 : adapted == MC_INVENTORY_BUSY            ? MC_WORLD_STORAGE_BUSY
                                                           : MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                 f->inventory_error.operation, true);
            return false;
        }
    }
    e->dispenser_cast_returned = true;
    e->dispenser_inventory = f->inventory;
    e->site.pc = 26;
    if (player == NULL) {
        null_reference(e, "XQ actual Player Dispenser invocation null receiver");
        return false;
    }
    /* GS is abstract/query-only in the native namespace. No unknown concrete
     * subclass is silently replaced by GS's inherited literal return. */
    if (!mc_entity_is_kind(player, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER) ||
        player->classes->source != MC_ENTITY_CLIENT) {
        fail(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "XQ actual Player Dispenser virtual dispatch",
             true);
        return false;
    }
    if (context == NULL || context->dispenser == NULL) {
        fail(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "XQ reached DC Dispenser display child", true);
        return false;
    }
    if (mc_entity_retain(player) != MC_ENTITY_OK) {
        fail(e, MC_WORLD_STORAGE_BUSY, "XQ invocation Player native occurrence", true);
        return false;
    }
    f->player = player;
    e->caller = e->site;
    e->dispenser_entered = true;
    const mc_world_storage_status displayed =
        context->dispenser(context->context, player, f->inventory, e);
    if (displayed != MC_WORLD_STORAGE_OK) {
        e->status = displayed;
        return false;
    }
    e->dispenser_returned = true;
    e->site = site("xq", "a", "(Lfd;IIILgs;)Z", 30);
    return true;
}
