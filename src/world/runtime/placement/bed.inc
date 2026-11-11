/* Actual VE activation over the caller's World and the clicked Player's
 * independently reached World. Every child remains in the placement owner. */
static void bed_activation_at(mc_world_placement_error *e, unsigned int pc) {
    e->site = site("ve", "a", "(Lfd;IIILgs;)Z", pc);
}

static bool bed_activation_direction(mc_world_placement_error *e, int32_t direction, int32_t axis,
                                     unsigned int get_pc, unsigned int row_pc,
                                     unsigned int value_pc, int32_t *out) {
    mc_registry_bed_table_view table;
    mc_registry_bed_row_view row;
    bed_activation_at(e, get_pc);
    mc_registry_status status = mc_registry_bed_table_get(e->work->registry, &table, &e->registry);
    if (status == MC_REGISTRY_OK) {
        bed_activation_at(e, row_pc);
        status = mc_registry_bed_table_at(&table, direction, &row, &e->registry);
    }
    if (status == MC_REGISTRY_OK) {
        bed_activation_at(e, value_pc);
        status = mc_registry_bed_row_at(&row, axis, out, &e->registry);
    }
    if (status != MC_REGISTRY_OK) {
        registry_failed(e);
        return false;
    }
    return true;
}

static bool bed_activation_player(mc_world_placement_error *e, mc_entity *player) {
    if (player == NULL) {
        null_reference(e, "Bed actual Player invocation null receiver");
        return false;
    }
    if (e->work->bed_player == player)
        return true;
    if (e->work->bed_player != NULL ||
        !mc_entity_is_kind(player, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER)) {
        fail(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Bed actual concrete Player virtual", true);
        return false;
    }
    const mc_entity_status status = mc_entity_retain(player);
    if (status != MC_ENTITY_OK) {
        fail(e,
             status == MC_ENTITY_LIMIT ? MC_WORLD_STORAGE_LIMIT
                                       : MC_WORLD_STORAGE_UNSUPPORTED_STATE,
             "Bed actual Player occurrence", true);
        return false;
    }
    e->work->bed_player = player;
    return true;
}

static bool bed_activation_write(mc_world_placement_error *e, mc_world_runtime *world, int32_t x,
                                 int32_t y, int32_t z, int32_t metadata, bool remove,
                                 mc_world_bed_occupancy_execution *owned) {
    if (e->work->mutation == NULL) {
        e->work->mutation = malloc(sizeof(*e->work->mutation));
        if (e->work->mutation == NULL) {
            fail(e, MC_WORLD_STORAGE_ALLOCATION_FAILED, "Bed retained mutation diagnostic", true);
            return false;
        }
        e->mutation = e->work->mutation;
    }
    mc_world_mutation_error *child = e->work->mutation;
    mc_world_mutation_error_init(child);
    if (owned != NULL) {
        mc_world_storage_status raw = MC_WORLD_STORAGE_OK;
        if (mc_world_runtime_metadata_set_in_execution(owned->context, world, x, y, z, metadata,
                                                       owned->mutation, &raw, child))
            return true;
        const mc_world_runtime_execution_result *result = owned->context->result;
        fail(e, raw,
             child->operation != NULL ? child->operation : result->ownership_error.operation, true);
        e->cause = child->cause;
        e->registry = child->registry;
        if (e->registry.status != MC_REGISTRY_OK)
            registry_failed_in(e, mc_world_runtime_registry(world));
        if (child->bed_source_null)
            null_reference(e, child->operation);
        return false;
    }
    bool ignored;
    const mc_world_storage_status status =
        remove ? mc_world_runtime_block_set(world, x, y, z, 0, &ignored, child)
               : mc_world_runtime_metadata_set(world, x, y, z, metadata, child);
    if (status != MC_WORLD_STORAGE_OK) {
        fail(e, status, child->operation, true);
        e->cause = child->cause;
        e->registry = child->registry;
        if (e->registry.status != MC_REGISTRY_OK)
            registry_failed_in(e, mc_world_runtime_registry(world));
        if (child->bed_source_null)
            null_reference(e, child->operation);
    }
    if (!capture(e)) {
        if (status == MC_WORLD_STORAGE_OK)
            fail(e, e->cleanup.status, e->cleanup.operation, true);
        return false;
    }
    if (status != MC_WORLD_STORAGE_OK)
        classify_receipts(e);
    return status == MC_WORLD_STORAGE_OK;
}

static bool bed_set_occupied(mc_world_placement_error *e, mc_world_runtime *world, int32_t x,
                             int32_t y, int32_t z, bool occupied,
                             mc_world_bed_occupancy_execution *owned) {
    e->site = site("ve", "a", "(Lfd;IIIZ)V", 0);
    if (mc_registry_bed_active_use(e->work->registry, &e->registry) != MC_REGISTRY_OK) {
        registry_failed(e);
        return false;
    }
    e->site.pc = 4;
    int32_t metadata;
    if (owned == NULL) {
        if (!value_get(e, world, e->query_count, x, y, z, true, &metadata))
            return false;
    } else {
        if (world == NULL) {
            null_reference(e, "Bed occupancy actual World null receiver");
            return false;
        }
        const mc_world_mutation_execution_context *context = owned->context;
        mc_world_block_execution_init(owned->scalar, context->execution, context->code,
                                      (mc_entity_tick_execution_site){"ve", "a", "(Lfd;IIIZ)V", 4},
                                      context->result);
        mc_world_storage_error cause = {0};
        mc_world_storage_status raw = MC_WORLD_STORAGE_OK;
        if (!mc_world_runtime_metadata_get_in_execution(owned->scalar, world, x, y, z, &metadata,
                                                        &cause, &raw)) {
            fail(e, raw,
                 cause.operation != NULL ? cause.operation
                                         : context->result->ownership_error.operation,
                 true);
            e->cause = cause;
            if (owned->scalar->get_returned && owned->scalar->get_status != MC_CHUNK_ACQUIRE_OK)
                e->acquisition = &owned->scalar->acquisition;
            if (owned->scalar->source.source_exception) {
                e->source_exception = true;
                e->exception_class = owned->scalar->source.exception_class;
                e->native_policy = false;
            }
            return false;
        }
    }
    metadata = occupied ? (int32_t)((uint32_t)metadata | UINT32_C(4))
                        : (int32_t)((uint32_t)metadata & ~UINT32_C(4));
    e->site.pc = 36;
    if (!bed_activation_write(e, world, x, y, z, metadata, false, owned))
        return false;
    e->site.pc = 39;
    return true;
}

static bool bed_occupancy_clear(mc_world_placement_error *e, mc_world_runtime *world, int32_t x,
                                int32_t y, int32_t z) {
    e->caller = e->site;
    if (!bed_set_occupied(e, world, x, y, z, false, NULL))
        return false;
    e->bed_occupancy_cleared = true;
    return true;
}

static bool bed_occupant(mc_world_placement_error *e, mc_world_runtime *world, int32_t x, int32_t y,
                         int32_t z, bool *found) {
    bed_activation_at(e, 277);
    const mc_world_reference_list *list = world->fields.d;
    e->bed_player_list_identity = (uintptr_t)list;
    if (list == NULL) {
        null_reference(e, "Bed occupied actual Player List iterator receiver");
        return false;
    }
    const mc_world_entity_list_ledger *ledger = &world->player_membership;
    if (list != ledger->list || !mc_world_runtime_entity_list_valid(world)) {
        fail(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Bed occupied typed Player List iterator",
             true);
        return false;
    }
    *found = false;
    for (size_t index = 0;; ++index) {
        bed_activation_at(e, 286);
        if (index >= list->count)
            return true;
        e->bed_player_index = index;
        bed_activation_at(e, 296);
        if (index >= ledger->count || list->items[index] != ledger->entries[index]) {
            fail(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Bed occupied typed iterator next", true);
            return false;
        }
        mc_entity *player = ledger->entries[index];
        bed_activation_at(e, 301);
        if (player != NULL) {
            const mc_entity_class_literal *target = NULL;
            bool matches = false;
            mc_entity_status status = mc_entity_class_literal_get(
                e->work->registry->entities, MC_ENTITY_CLASS_KIND_PLAYER, &target);
            if (status == MC_ENTITY_OK)
                status = mc_entity_class_literal_is_assignable_from(target, player->class_literal,
                                                                    &matches);
            if (status != MC_ENTITY_OK) {
                fail(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                     "Bed occupied actual Player cast native Class identities", true);
                return false;
            }
            if (!matches) {
                e->source_exception = true;
                e->exception_class = "java/lang/ClassCastException";
                fail(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Bed occupied actual Player cast",
                     false);
                return false;
            }
        }
        bed_activation_at(e, 308);
        if (player == NULL) {
            null_reference(e, "Bed occupied actual Player.N receiver");
            return false;
        }
        if (!mc_entity_is_kind(player, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER)) {
            fail(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                 "Bed occupied actual Player.N concrete native body", true);
            return false;
        }
        if (e->work->bed_occupant == NULL) {
            e->work->bed_occupant = calloc(1, sizeof(*e->work->bed_occupant));
            if (e->work->bed_occupant == NULL) {
                fail(e, MC_WORLD_STORAGE_ALLOCATION_FAILED,
                     "Bed retained occupied Player diagnostic", true);
                return false;
            }
            e->bed_occupant = e->work->bed_occupant;
        }
        bool sleeping;
        const mc_player_status status =
            mc_player_is_sleeping(player, &sleeping, e->work->bed_occupant);
        if (status != MC_PLAYER_OK) {
            fail(e,
                 status == MC_PLAYER_LIMIT ||
                         e->work->bed_occupant->entity.status == MC_ENTITY_LIMIT
                     ? MC_WORLD_STORAGE_LIMIT
                 : status == MC_PLAYER_BUSY ? MC_WORLD_STORAGE_BUSY
                                            : MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                 e->work->bed_occupant->operation, true);
            return false;
        }
        if (!sleeping)
            continue;
        const mc_player_coordinates *coordinate = player->local_player->fields.player.v;
        bed_activation_at(e, 323);
        if (coordinate == NULL) {
            null_reference(e, "Bed occupied actual sleeping coordinate x receiver");
            return false;
        }
        struct mc_player_nbt_coordinate *node = player->local_player->nbt_coordinates;
        while (node != NULL && &node->value != coordinate)
            node = node->next;
        if (node == NULL) {
            fail(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                 "Bed occupied actual sleeping coordinate native owner", true);
            return false;
        }
        if (coordinate->x != x)
            continue;
        bed_activation_at(e, 332);
        if (coordinate->y != y)
            continue;
        bed_activation_at(e, 341);
        if (coordinate->z != z)
            continue;
        *found = true; /* Source retains the last match and continues. */
        e->bed_sleeper_identity = (uintptr_t)player;
    }
}

static bool bed_message(mc_world_placement_error *e, mc_entity *player,
                        const mc_world_block_activation_context *context, bool occupied) {
    bed_activation_at(e, occupied ? 375u : 422u);
    const mc_registry_string *key = NULL;
    if (mc_registry_runtime_literal_ascii(e->work->registry,
                                          occupied ? "tile.bed.occupied" : "tile.bed.noSleep", &key,
                                          &e->registry) != MC_REGISTRY_OK) {
        registry_failed(e);
        return false;
    }
    bed_activation_at(e, occupied ? 377u : 424u);
    if (!bed_activation_player(e, player))
        return false;
    if (context == NULL || context->player_message == NULL) {
        fail(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Bed reached actual Player message child",
             true);
        return false;
    }
    e->caller = e->site;
    e->player_message_entered = true;
    const mc_world_storage_status status =
        context->player_message(context->context, player, e->work->registry, key, e);
    if (status != MC_WORLD_STORAGE_OK) {
        e->status = status;
        return false;
    }
    e->player_message_returned = true;
    return true;
}

static bool bed_enum(mc_world_placement_error *e, const char *field, unsigned int pc,
                     const mc_player_sleep_result **out) {
    bed_activation_at(e, pc);
    if (e->work->sleep_result == NULL) {
        e->work->sleep_result = calloc(1, sizeof(*e->work->sleep_result));
        if (e->work->sleep_result == NULL) {
            fail(e, MC_WORLD_STORAGE_ALLOCATION_FAILED, "Bed retained CW diagnostic", true);
            return false;
        }
        e->sleep_result = e->work->sleep_result;
    }
    if (e->work->bed_enum_owner == NULL) {
        /* VE resolves CW in its actual invoking code namespace. This is
         * independent of GS's concrete Player namespace and happens only
         * after GS has returned, at the reached caller getstatic. */
        mc_entity_classes *owner = e->work->registry->entities;
        const mc_entity_status retained = mc_entity_classes_retain(
            owner, MC_ENTITY_CLIENT, e->work->registry->options.random_profile);
        if (retained != MC_ENTITY_OK) {
            mc_player_sleep_result_error *child = e->work->sleep_result;
            child->status = retained == MC_ENTITY_LIMIT ? MC_PLAYER_SLEEP_RESULT_LIMIT
                                                        : MC_PLAYER_SLEEP_RESULT_ARGUMENT;
            child->operation = "Bed caller CW actual Entity namespace occurrence";
            child->native_policy = true;
            child->native.status = retained;
            child->native.operation = child->operation;
            fail(e,
                 retained == MC_ENTITY_LIMIT ? MC_WORLD_STORAGE_LIMIT
                                             : MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                 child->operation, true);
            return false;
        }
        e->work->bed_enum_owner = owner;
    }
    const mc_player_sleep_result_status status = mc_player_sleep_result_static_get(
        e->work->bed_enum_owner, field, out, e->work->sleep_result);
    if (status == MC_PLAYER_SLEEP_RESULT_OK)
        return true;
    fail(e,
         status == MC_PLAYER_SLEEP_RESULT_LIMIT               ? MC_WORLD_STORAGE_LIMIT
         : status == MC_PLAYER_SLEEP_RESULT_BUSY              ? MC_WORLD_STORAGE_BUSY
         : status == MC_PLAYER_SLEEP_RESULT_ALLOCATION_FAILED ? MC_WORLD_STORAGE_ALLOCATION_FAILED
                                                              : MC_WORLD_STORAGE_UNSUPPORTED_STATE,
         e->work->sleep_result->operation, e->work->sleep_result->native_policy);
    return false;
}

static bool bed_activate(mc_world_placement_error *e, const mc_block *block,
                         mc_world_runtime *world, int32_t x, int32_t y, int32_t z,
                         mc_entity *player, const mc_world_block_activation_context *context) {
    bed_activation_at(e, 1);
    e->bed_entered = true;
    e->bed_player_index = SIZE_MAX;
    if (!hold_world(e, world))
        return false;
    e->remote_read = true;
    e->remote = world->fields.B;
    if (e->remote) {
        bed_activation_at(e, 8);
        return true;
    }
    bed_activation_at(e, 14);
    int32_t metadata;
    if (!value_get(e, world, e->query_count, x, y, z, true, &metadata))
        return false;
    e->bed_head = ((uint32_t)metadata & 8u) != 0;
    if (!e->bed_head) {
        const int32_t direction = (int32_t)((uint32_t)metadata & 3u);
        int32_t dx, dz;
        if (!bed_activation_direction(e, direction, 0, 35, 40, 42, &dx))
            return false;
        x = mc_java_i32_add(x, dx);
        if (!bed_activation_direction(e, direction, 1, 47, 52, 54, &dz))
            return false;
        z = mc_java_i32_add(z, dz);
        bed_activation_at(e, 63);
        int32_t partner;
        if (!block_get(e, world, e->query_count, x, y, z, &partner))
            return false;
        e->bed_partner_read = true;
        bed_activation_at(e, 67);
        if (partner != block->fields.bn) {
            bed_activation_at(e, 73);
            return true;
        }
        bed_activation_at(e, 80);
        if (!value_get(e, world, e->query_count, x, y, z, true, &metadata))
            return false;
    }
    e->bed_metadata = metadata;
    e->bed_direction = (int32_t)((uint32_t)metadata & 3u);
    e->bed_x = x;
    e->bed_y = y;
    e->bed_z = z;
    bed_activation_at(e, 89);
    if (world->fields.t == NULL) {
        null_reference(e, "Bed actual World provider can-respawn receiver");
        return false;
    }
    bool allowed;
    e->provider = &e->work->provider;
    if (mc_world_provider_can_respawn(world->fields.t, &allowed, &e->work->provider) !=
        MC_WORLD_PROVIDER_OK) {
        fail(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Bed actual provider can-respawn body", true);
        return false;
    }
    if (!allowed) {
        /* Source's local doubles are evaluated even though explosion256 uses
         * fresh float operands. Native FP admission precedes those operations. */
        if ((_mm_getcsr() & 0xffc0u) != 0x1f80u) {
            bed_activation_at(e, 100);
            fail(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Bed explosion operand FP environment",
                 true);
            return false;
        }
        volatile double center_x = (double)x + 0.5, center_y = (double)y + 0.5;
        volatile double center_z = (double)z + 0.5;
        bed_activation_at(e, 126);
        if (!bed_activation_write(e, world, x, y, z, 0, true, NULL))
            return false;
        int32_t dx, dz;
        if (!bed_activation_direction(e, e->bed_direction, 0, 138, 143, 145, &dx))
            return false;
        x = mc_java_i32_add(x, dx);
        if (!bed_activation_direction(e, e->bed_direction, 1, 150, 155, 157, &dz))
            return false;
        z = mc_java_i32_add(z, dz);
        bed_activation_at(e, 166);
        int32_t partner;
        if (!block_get(e, world, e->query_count, x, y, z, &partner))
            return false;
        bed_activation_at(e, 170);
        if (partner == block->fields.bn) {
            bed_activation_at(e, 182);
            if (!bed_activation_write(e, world, x, y, z, 0, true, NULL))
                return false;
            if ((_mm_getcsr() & 0xffc0u) != 0x1f80u) {
                bed_activation_at(e, 190);
                fail(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Bed center operand FP environment",
                     true);
                return false;
            }
            center_x = (center_x + (double)x + 0.5) / 2.0;
            center_y = (center_y + (double)y + 0.5) / 2.0;
            center_z = (center_z + (double)z + 0.5) / 2.0;
        }
        (void)center_x;
        (void)center_y;
        (void)center_z;
        bed_activation_at(e, 235);
        if ((_mm_getcsr() & 0xffc0u) != 0x1f80u) {
            fail(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Bed explosion float FP environment", true);
            return false;
        }
        volatile float fx = (float)x, fy = (float)y, fz = (float)z;
        volatile float placed_x = fx + 0.5f, placed_y = fy + 0.5f, placed_z = fz + 0.5f;
        e->bed_explosion_x = (double)placed_x;
        e->bed_explosion_y = (double)placed_y;
        e->bed_explosion_z = (double)placed_z;
        e->bed_explosion_pending = true;
        bed_activation_at(e, 256);
        fail(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Bed actual World explosion child", true);
        return false;
    }
    e->bed_occupied = ((uint32_t)metadata & 4u) != 0;
    if (e->bed_occupied) {
        bool found;
        if (!bed_occupant(e, world, x, y, z, &found))
            return false;
        if (found) {
            if (!bed_message(e, player, context, true))
                return false;
            bed_activation_at(e, 381);
            return true;
        }
        bed_activation_at(e, 367);
        if (!bed_occupancy_clear(e, world, x, y, z))
            return false;
    }
    bed_activation_at(e, 388);
    if (!bed_activation_player(e, player))
        return false;
    e->work->sleep = calloc(1, sizeof(*e->work->sleep));
    if (e->work->sleep == NULL) {
        fail(e, MC_WORLD_STORAGE_ALLOCATION_FAILED, "Bed retained Player sleep diagnostic", true);
        return false;
    }
    e->sleep = e->work->sleep;
    /* GS owns each freshly reached Player.aI occurrence and all failed
     * descendants. Its code Registry is explicit and independent of VE. */
    const mc_player_sleep_context sleep_context = {
        .code_registry = context == NULL ? NULL : context->player_code_registry};
    const mc_player_sleep_status status = mc_player_try_sleep_begin(
        player, x, y, z, &sleep_context, &e->work->sleep_call, e->work->sleep);
    if (status != MC_PLAYER_SLEEP_OK) {
        const mc_player_sleep_error *child = e->work->sleep;
        e->caller = e->site;
        e->site = site(child->site.class_name, child->site.method, child->site.descriptor,
                       child->site.pc);
        fail(e,
             status == MC_PLAYER_SLEEP_LIMIT               ? MC_WORLD_STORAGE_LIMIT
             : status == MC_PLAYER_SLEEP_BUSY              ? MC_WORLD_STORAGE_BUSY
             : status == MC_PLAYER_SLEEP_ALLOCATION_FAILED ? MC_WORLD_STORAGE_ALLOCATION_FAILED
                                                           : MC_WORLD_STORAGE_UNSUPPORTED_STATE,
             child->operation, child->native_policy);
        e->source_exception = child->source_exception;
        e->source_error = child->source_error;
        e->exception_class = child->exception_class;
        return false;
    }
    mc_player_sleep_state sleep_state;
    if (!mc_player_sleep_inspect(e->work->sleep_call, &sleep_state) || !sleep_state.returned) {
        fail(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Bed retained Player sleep return state", true);
        return false;
    }
    const mc_player_sleep_result *result = sleep_state.result;
    e->bed_sleep_returned = true;
    e->bed_sleep_result_identity = (uintptr_t)result;
    const mc_player_sleep_result *expected = NULL;
    if (!bed_enum(e, "a", 395, &expected))
        return false;
    if (result == expected) {
        /* A nullable/replaced actual CW.a can select this source arm. The
         * genuine occupancy setter still runs; no sleep result is invented. */
        e->caller = site("ve", "a", "(Lfd;IIILgs;)Z", 407);
        if (!bed_set_occupied(e, world, x, y, z, true, NULL))
            return false;
        bed_activation_at(e, 411);
        return true;
    }
    if (!bed_enum(e, "c", 414, &expected))
        return false;
    if (result == expected && !bed_message(e, player, context, false))
        return false;
    bed_activation_at(e, 428);
    return true;
}
