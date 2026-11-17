/* TC.b drop loop. Its false-guard body matches E.b with source sites +6;
 * TC owns its independent Random and actual SK cells. */
static bool live_furnace_drops_held(mc_world_runtime *world, const mc_block *block, int32_t x,
                                    int32_t y, int32_t z, mc_live_tile_entity_error *error) {
    mc_tile_entity *furnace = NULL;
    error->bytecode = 11;
    if (!live_world_get_held(world, x, y, z, &furnace, error, NULL, NULL, NULL))
        return false;
    error->bytecode = 14;
    if (furnace != NULL && furnace->kind != MC_TILE_ENTITY_FURNACE) {
        mc_tile_entity_release(furnace);
        live_fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                  "Furnace removal TileEntity checkcast");
        return false;
    }
    bool completed = true;
    for (int32_t slot = 0; completed; slot = mc_java_i32_add(slot, 1)) {
        error->bytecode = 26;
        int32_t size;
        mc_tile_entity_status tile_entity_status_code = mc_tile_entity_furnace_size(furnace, &size);
        if (tile_entity_status_code != MC_TILE_ENTITY_OK) {
            live_tile_entity_error(error, tile_entity_status_code, "Furnace removal size receiver");
            completed = false;
            break;
        }
        if (slot >= size)
            break;
        error->bytecode = 38;
        mc_item_stack *stack = NULL;
        tile_entity_status_code = mc_tile_entity_furnace_get(furnace, slot, &stack);
        if (tile_entity_status_code != MC_TILE_ENTITY_OK) {
            live_tile_entity_error(error, tile_entity_status_code, "Furnace removal slot getter");
            completed = false;
            break;
        }
        if (stack == NULL)
            continue;
        error->bytecode = 54;
        mc_java_random *random = block->subtype.tc.a;
        if (random == NULL || (_mm_getcsr() & 0xffc0u) != 0x1f80u) {
            live_fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                      "Furnace private Random/strict float call");
            mc_item_stack_release(stack);
            completed = false;
            break;
        }
        volatile float offset_x = mc_java_random_next_float(random) * 0.8f + 0.1f;
        error->bytecode = 69;
        random = block->subtype.tc.a;
        if (random == NULL) {
            live_fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Furnace second private Random");
            mc_item_stack_release(stack);
            completed = false;
            break;
        }
        volatile float offset_y = mc_java_random_next_float(random) * 0.8f + 0.1f;
        error->bytecode = 84;
        random = block->subtype.tc.a;
        if (random == NULL) {
            live_fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Furnace third private Random");
            mc_item_stack_release(stack);
            completed = false;
            break;
        }
        volatile float offset_z = mc_java_random_next_float(random) * 0.8f + 0.1f;
        while (completed) {
            error->bytecode = 97;
            mc_item_stack_fields fields;
            mc_item_stack_status item = mc_item_stack_fields_get(stack, &fields);
            if (item != MC_ITEM_STACK_OK) {
                live_fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Furnace Stack count field");
                completed = false;
                break;
            }
            if (fields.count <= 0)
                break;
            error->bytecode = 109;
            random = block->subtype.tc.a;
            int32_t count;
            if (random == NULL || !mc_java_random_next_bounded(random, 21, &count)) {
                live_fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                          "Furnace private count Random");
                completed = false;
                break;
            }
            count = mc_java_i32_add(count, 10);
            item = mc_item_stack_fields_get(stack, &fields);
            if (item != MC_ITEM_STACK_OK) {
                live_fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Furnace cap count field");
                completed = false;
                break;
            }
            if (count > fields.count)
                count = fields.count;
            error->bytecode = 143;
            item = mc_item_stack_count_set(stack, mc_java_i32_sub(fields.count, count));
            if (item != MC_ITEM_STACK_OK) {
                live_fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                          "Furnace drop count decrement");
                completed = false;
                break;
            }
            error->bytecode = 146;
            mc_entity *entity = NULL;
            mc_entity_status entity_status =
                mc_item_entity_allocate(mc_world_runtime_entities(world), &entity, &error->entity);
            if (entity_status != MC_ENTITY_OK) {
                completed = live_drop_entity_error(error, entity_status, "Furnace raw ItemEntity");
                break;
            }
            /* Source i2f and fadd precede widening. The ItemEntity allocation
             * already happened before these constructor argument operations. */
            volatile float px = (float)x + offset_x;
            volatile float py = (float)y + offset_y;
            volatile float pz = (float)z + offset_z;
            error->bytecode = 170;
            mc_item_stack *drop = NULL;
            item = mc_item_stack_allocate(&drop);
            if (item == MC_ITEM_STACK_OK) {
                error->bytecode = 176;
                item = mc_item_stack_fields_get(stack, &fields);
                if (item == MC_ITEM_STACK_OK) {
                    error->bytecode = 186;
                    item = mc_item_stack_construct(drop, fields.id, count, fields.damage);
                }
            }
            if (item != MC_ITEM_STACK_OK) {
                error->tile_entity.item.status = item;
                live_fail(error,
                          item == MC_ITEM_STACK_LIMIT ? MC_WORLD_STORAGE_LIMIT
                          : item == MC_ITEM_STACK_ALLOCATION_FAILED
                              ? MC_WORLD_STORAGE_ALLOCATION_FAILED
                              : MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                          "Furnace numeric drop Stack");
                mc_item_stack_release(drop);
                mc_entity_release(entity);
                completed = false;
                break;
            }
            error->bytecode = 189;
            entity_status = mc_item_entity_construct(entity, world, (double)px, (double)py,
                                                     (double)pz, drop, &error->entity);
            mc_item_stack_release(drop);
            if (entity_status != MC_ENTITY_OK) {
                completed =
                    live_drop_entity_error(error, entity_status, "Furnace ItemEntity constructor");
                mc_entity_release(entity);
                break;
            }
            static const unsigned int gaussian_pc[3] = {204, 221, 241};
            static const unsigned int store_pc[3] = {212, 232, 249};
            for (unsigned int axis = 0; axis < 3; ++axis) {
                error->bytecode = gaussian_pc[axis];
                random = block->subtype.tc.a;
                if (random == NULL || (_mm_getcsr() & 0xffc0u) != 0x1f80u) {
                    live_fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                              "Furnace private Gaussian/strict double call");
                    completed = false;
                    break;
                }
                volatile float velocity = (float)mc_java_random_next_gaussian(random) * 0.05f;
                if (axis == 1u)
                    velocity = velocity + 0.2f;
                error->bytecode = store_pc[axis];
                entity_status = mc_entity_set_velocity(entity, axis, (double)velocity);
                if (entity_status != MC_ENTITY_OK) {
                    completed =
                        live_drop_entity_error(error, entity_status, "Furnace velocity store");
                    break;
                }
            }
            if (completed) {
                error->bytecode = 255;
                bool ordinary;
                if (mc_world_runtime_insert_entity(world, entity, &ordinary, &error->publication) !=
                    MC_WORLD_STORAGE_OK) {
                    live_cause(error, error->publication.cause);
                    completed = false;
                }
                /* Source pop258 discards both ordinary false and true. */
            }
            mc_entity_release(entity);
        }
        mc_item_stack_release(stack);
    }
    mc_tile_entity_release(furnace);
    return completed;
}
