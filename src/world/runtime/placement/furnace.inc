/* Actual TC activation, with one real SK query and the GS.a(SK) operand.
 * These native holds preserve that same Tile through nested display work. */
static bool furnace_activate(mc_world_placement_error *e, mc_world_runtime *world, int32_t x,
                             int32_t y, int32_t z, mc_entity *player,
                             const mc_world_block_activation_context *context) {
    e->site = site("tc", "a", "(Lfd;IIILgs;)Z", 1);
    if (!hold_world(e, world))
        return false;
    e->remote_read = true;
    e->remote = world->fields.B;
    if (e->remote) {
        e->site.pc = 8;
        return true;
    }
    e->site.pc = 14;
    furnace_activation_work *f = calloc(1, sizeof(*f));
    if (f == NULL) {
        fail(e, MC_WORLD_STORAGE_ALLOCATION_FAILED, "Furnace activation retained Tile child", true);
        return false;
    }
    e->work->furnace = f;
    e->furnace_tile = &f->tile_error;
    const mc_world_storage_status queried =
        mc_live_tile_entities_get(world, x, y, z, &f->tile, &f->tile_error);
    if (queried != MC_WORLD_STORAGE_OK) {
        fail(e, queried, f->tile_error.operation, true);
        e->cause = f->tile_error.cause;
        e->registry = f->tile_error.registry;
    }
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
        mc_tile_entity_snapshot state;
        if (mc_tile_entity_get_snapshot(f->tile, &state) != MC_TILE_ENTITY_OK) {
            fail(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Furnace activation actual cast storage",
                 true);
            return false;
        }
        if (state.kind != MC_TILE_ENTITY_FURNACE) {
            fail(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Furnace activation SK cast", false);
            e->source_exception = true;
            e->exception_class = "java/lang/ClassCastException";
            return false;
        }
        e->furnace = &f->inventory_error;
        const mc_inventory_status adapted = mc_inventory_furnace_from_tile(
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
    e->furnace_inventory = f->inventory;
    e->site.pc = 26;
    if (player == NULL) {
        null_reference(e, "TC actual Player Furnace invocation null receiver");
        return false;
    }
    if (context == NULL || context->furnace == NULL) {
        fail(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "TC reached Player Furnace display child",
             true);
        return false;
    }
    e->caller = e->site;
    e->furnace_entered = true;
    const mc_world_storage_status displayed =
        context->furnace(context->context, player, f->inventory, e);
    if (displayed != MC_WORLD_STORAGE_OK) {
        e->status = displayed;
        return false;
    }
    e->furnace_returned = true;
    e->site = site("tc", "a", "(Lfd;IIILgs;)Z", 30);
    return true;
}

static bool furnace_placed_by(mc_world_placement_error *e, mc_world_runtime *world, int32_t x,
                              int32_t y, int32_t z, mc_entity *living, mc_math_classes *math) {
    e->site = site("tc", "a", "(Lfd;IIILls;)V", 2);
    if (living == NULL) {
        null_reference(e, "TC actual null Living yaw field receiver");
        return false;
    }
    if (mc_entity_living_fields(living) == NULL) {
        fail(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "TC represented Living yaw storage", true);
        return false;
    }
    const float yaw = living->base.yaw;
    e->site.pc = 7;
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u) {
        fail(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "TC reached yaw floating environment", true);
        return false;
    }
    const float multiplied = _mm_cvtss_f32(_mm_mul_ss(_mm_set_ss(yaw), _mm_set_ss(4.0f)));
    const float divided = _mm_cvtss_f32(_mm_div_ss(_mm_set_ss(multiplied), _mm_set_ss(360.0f)));
    const double argument = _mm_cvtsd_f64(_mm_add_sd(_mm_set_sd((double)divided), _mm_set_sd(.5)));
    e->site.pc = 16;
    if (math == NULL) {
        fail(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "TC reached defining Math namespace", true);
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
        fail(e, MC_WORLD_STORAGE_ALLOCATION_FAILED, "Furnace placed-by metadata diagnostic", true);
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
            registry_failed_in(e, mc_world_runtime_registry(world));
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
