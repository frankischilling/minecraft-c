static bool origins(mc_client_render_global_entities_call *c) {
    mc_client_render_global *n = c->state.owner;
    n->fields.J = 0;
    c->state.last_store_pc = 85;
    n->fields.K = 0;
    c->state.last_store_pc = 90;
    n->fields.L = 0;
    c->state.last_store_pc = 95;
    c->state.counters_cleared = true;
    client_application *source_app = app(c, 102);
    if (source_app == NULL)
        return false;
    mc_entity *living = source_app->i;
    /* Each static component has its own source interpolation evaluation. */
    static const int32_t first[] = {109, 132, 155, 178, 201, 224};
    static const int32_t sub[] = {122, 145, 168, 191, 214, 237};
    static const int32_t widen[] = {124, 147, 170, 193, 216, 239};
    static const int32_t mul[] = {125, 148, 171, 194, 217, 240};
    static const int32_t add[] = {126, 149, 172, 195, 218, 241};
    static const unsigned int put[] = {127, 150, 173, 196, 219, 242};
    for (unsigned int i = 0; i < 6; ++i) {
        if (living == NULL)
            return fail(c, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE,
                        "actual N captured Living coordinate receiver", (unsigned int)first[i],
                        false);
        mc_entity_base_fields *f = mc_entity_runtime_fields(living);
        if (f == NULL)
            return fail(c, MC_CLIENT_RENDER_GLOBAL_INVALID_PHASE,
                        "N canonical Living coordinate fields", (unsigned int)first[i], true);
        const unsigned int axis = i % 3;
        const double previous = axis == 0 ? f->bl : axis == 1 ? f->bm : f->bn;
        const double current = axis == 0 ? f->x : axis == 1 ? f->y : f->z;
        mc_client_interpolation_error arithmetic;
        double value;
        if (!mc_client_interpolate_double(previous, current, c->state.partial, sub[i], widen[i],
                                          mul[i], add[i], &value, &arithmetic))
            return fail(c, MC_CLIENT_RENDER_GLOBAL_UNSUPPORTED_FP,
                        arithmetic.status == MC_CLIENT_INTERPOLATION_UNSUPPORTED_FP
                            ? "N origin strict floating controls"
                            : "N origin unmeasured two-NaN signaling operand",
                        (unsigned int)arithmetic.pc, true);
        c->state.source_pc = put[i];
        if (i < 3) {
            if (mc_client_render_manager_origin_store(n->entity_manager_consumer.classes, axis,
                                                      value, &c->manager) != MC_CLIENT_MANAGER_OK)
                return manager_failure(c, put[i]);
        } else if (mc_client_tile_renderer_origin_store(n->tile_consumer.classes, axis, value,
                                                        &c->tiles) != MC_CLIENT_TILE_RENDERER_OK) {
            return tile_failure(c, put[i]);
        }
        c->state.last_store_pc = put[i];
        ++c->state.origins_written;
    }
    c->state.origins_stored = true;
    return true;
}

static bool world_failure(mc_client_render_global_entities_call *c, mc_world_storage_status status,
                          unsigned int pc) {
    return fail(c,
                status == MC_WORLD_STORAGE_BUSY    ? MC_CLIENT_RENDER_GLOBAL_BUSY
                : status == MC_WORLD_STORAGE_LIMIT ? MC_CLIENT_RENDER_GLOBAL_LIMIT
                : status == MC_WORLD_STORAGE_ALLOCATION_FAILED
                    ? MC_CLIENT_RENDER_GLOBAL_ALLOCATION_FAILED
                    : MC_CLIENT_RENDER_GLOBAL_INVALID_PHASE,
                c->failure.world.operation, pc, true);
}

static bool lease_world(mc_client_render_global_entities_call *c, mc_world_runtime *world,
                        unsigned int pc) {
    for (size_t i = 0; i < c->world_count; ++i)
        if (c->worlds[i] == world)
            return true;
    if (c->world_count == sizeof(c->worlds) / sizeof(c->worlds[0]))
        return fail(c, MC_CLIENT_RENDER_GLOBAL_LIMIT, "N distinct reached World lease bound", pc,
                    true);
    const mc_world_storage_status status =
        mc_world_runtime_render_entities_lease(world, &c->failure.world);
    if (status != MC_WORLD_STORAGE_OK)
        return world_failure(c, status, pc);
    c->worlds[c->world_count++] = world;
    return true;
}

static mc_world_runtime *world_at(mc_client_render_global_entities_call *c, unsigned int pc) {
    mc_world_runtime *world = c->state.owner->fields.k;
    if (world == NULL)
        fail(c, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE, "actual N World receiver", pc, false);
    return world;
}

static bool list_count(mc_client_render_global_entities_call *c, mc_world_runtime *world,
                       const mc_world_reference_list *list, unsigned int pc, int32_t *out) {
    if (list == NULL)
        return fail(c, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE, "actual N List.size receiver", pc,
                    false);
    const mc_world_storage_status status =
        mc_world_runtime_render_entity_list_count(world, list, out, &c->failure.world);
    return status == MC_WORLD_STORAGE_OK || world_failure(c, status, pc);
}

static void release_current(mc_client_render_global_entities_call *c) {
    if (c->current != NULL)
        mc_entity_release(c->current);
    c->state.current = c->current = NULL;
}

static bool list_get(mc_client_render_global_entities_call *c, mc_world_runtime *world,
                     const mc_world_reference_list *list, unsigned int pc) {
    if (list == NULL)
        return fail(c, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE, "actual N List.get receiver", pc,
                    false);
    mc_entity *entity = NULL;
    const mc_world_storage_status status = mc_world_runtime_render_entity_list_get(
        world, list, c->state.index, &entity, &c->failure.world);
    if (status != MC_WORLD_STORAGE_OK) {
        if (status == MC_WORLD_STORAGE_INVALID_ARGUMENT)
            return fail(c, MC_CLIENT_RENDER_GLOBAL_SOURCE_INDEX, c->failure.world.operation, pc,
                        false);
        return world_failure(c, status, pc);
    }
    if (entity != NULL && mc_entity_retain(entity) != MC_ENTITY_OK)
        return fail(c, MC_CLIENT_RENDER_GLOBAL_BUSY, "N reached Entity occurrence", pc, true);
    c->state.current = c->current = entity;
    return true;
}

static bool entity_distance(mc_client_render_global_entities_call *c, unsigned int pc, bool *out) {
    if (c->current == NULL)
        return fail(c, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE,
                    "actual N Entity render-distance receiver", pc, false);
    ++c->state.distance_checks;
    const mc_entity_render_distance_status status =
        mc_entity_render_distance(c->current, c->state.eye, out, &c->distance);
    if (status == MC_ENTITY_RENDER_DISTANCE_OK)
        return true;
    fail(c,
         status == MC_ENTITY_RENDER_DISTANCE_SOURCE_NULL ? MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE
         : status == MC_ENTITY_RENDER_DISTANCE_UNSUPPORTED_FP ||
                 status == MC_ENTITY_RENDER_DISTANCE_UNSUPPORTED_NUMERIC
             ? MC_CLIENT_RENDER_GLOBAL_UNSUPPORTED_FP
         : status == MC_ENTITY_RENDER_DISTANCE_UNIMPLEMENTED_DEPENDENCY
             ? MC_CLIENT_RENDER_GLOBAL_UNIMPLEMENTED_DEPENDENCY
             : MC_CLIENT_RENDER_GLOBAL_INVALID_PHASE,
         c->distance.operation, pc, c->distance.native_policy);
    c->failure.render_distance = &c->distance;
    c->failure.parent = (mc_client_graphics_site){"n", "a", entities_descriptor, (int32_t)pc};
    c->failure.child =
        (mc_client_graphics_site){c->distance.source_class, c->distance.source_method,
                                  c->distance.source_descriptor, c->distance.pc};
    return false;
}

static bool draw_entity(mc_client_render_global_entities_call *c, unsigned int get_pc,
                        unsigned int invoke_pc) {
    c->state.source_pc = get_pc;
    mc_client_render_manager *receiver = NULL;
    if (mc_client_render_manager_static_get(c->state.owner->entity_manager_consumer.classes,
                                            &receiver, &c->manager) != MC_CLIENT_MANAGER_OK)
        return manager_failure(c, get_pc);
    if (receiver != NULL &&
        mc_client_render_manager_retain(receiver, &c->manager) != MC_CLIENT_MANAGER_OK)
        return manager_failure(c, get_pc);
    c->draw_th = receiver;
    if (receiver == NULL)
        return fail(c, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE, "actual N TH draw receiver",
                    invoke_pc, false);
    fail(c, MC_CLIENT_RENDER_GLOBAL_UNIMPLEMENTED_DEPENDENCY,
         "actual TH Entity render child is not connected", invoke_pc, true);
    c->failure.child = (mc_client_graphics_site){"th", "a", "(Lsn;F)V", 0};
    return false;
}

static bool weather(mc_client_render_global_entities_call *c) {
    for (c->state.index = 0;; c->state.index = mc_java_i32_add(c->state.index, 1)) {
        mc_world_runtime *world = world_at(c, 274);
        if (world == NULL || !lease_world(c, world, 274))
            return false;
        const mc_world_reference_list *list = NULL;
        mc_world_storage_status status =
            mc_world_runtime_render_weather_list(world, &list, &c->failure.world);
        if (status != MC_WORLD_STORAGE_OK)
            return world_failure(c, status, 274);
        int32_t count;
        if (!list_count(c, world, list, 277, &count))
            return false;
        if (c->state.index >= count)
            return true;
        world = world_at(c, 289);
        if (world == NULL || !lease_world(c, world, 289))
            return false;
        status = mc_world_runtime_render_weather_list(world, &list, &c->failure.world);
        if (status != MC_WORLD_STORAGE_OK)
            return world_failure(c, status, 289);
        if (!list_get(c, world, list, 294))
            return false;
        ++c->state.weather_visited;
        c->state.owner->fields.K = mc_java_i32_add(c->state.owner->fields.K, 1);
        c->state.last_store_pc = 311;
        bool visible;
        if (!entity_distance(c, 317, &visible))
            return false;
        if (visible)
            return draw_entity(c, 323, 329);
        release_current(c);
    }
}

static bool entity_frustum(mc_client_render_global_entities_call *c, bool *out) {
    mc_entity_snapshot entity;
    c->failure.entity.status = mc_entity_get_snapshot(c->current, &entity);
    if (c->failure.entity.status != MC_ENTITY_OK)
        return fail(c, MC_CLIENT_RENDER_GLOBAL_INVALID_PHASE, "N actual Entity bounds field", 387,
                    true);
    if (c->state.frustum == NULL)
        return fail(c, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE, "actual N YN receiver", 390, false);
    ++c->state.frustum_checks;
    const mc_client_frustum_status status = mc_client_source_frustum_bounds(
        (mc_client_source_frustum *)c->state.frustum, entity.bounds, out, &c->culling);
    if (status == MC_CLIENT_FRUSTUM_OK)
        return true;
    fail(c,
         status == MC_CLIENT_FRUSTUM_SOURCE_NULL      ? MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE
         : status == MC_CLIENT_FRUSTUM_UNSUPPORTED_FP ? MC_CLIENT_RENDER_GLOBAL_UNSUPPORTED_FP
         : status == MC_CLIENT_FRUSTUM_BUSY           ? MC_CLIENT_RENDER_GLOBAL_BUSY
                                                      : MC_CLIENT_RENDER_GLOBAL_INVALID_PHASE,
         c->culling.operation, 390, c->culling.native_policy);
    c->failure.entity_culling = &c->culling;
    c->failure.parent = (mc_client_graphics_site){"n", "a", entities_descriptor, 390};
    c->failure.child = c->culling.site;
    return false;
}

static bool camera_visible(mc_client_render_global_entities_call *c, bool *out) {
    client_application *source_app = app(c, 404);
    if (source_app == NULL)
        return false;
    if (c->current != source_app->i) {
        *out = true;
        return true;
    }
    source_app = app(c, 414);
    if (source_app == NULL)
        return false;
    mc_game_settings *settings = source_app->settings;
    if (settings == NULL)
        return fail(c, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE, "actual N Settings receiver", 417,
                    false);
    mc_game_settings_state state;
    if (!mc_game_settings_read_state(settings, &state))
        return fail(c, MC_CLIENT_RENDER_GLOBAL_INVALID_PHASE, "N actual Settings fields", 417,
                    true);
    if (state.values.third_person_view) {
        *out = true;
        return true;
    }
    source_app = app(c, 427);
    if (source_app == NULL)
        return false;
    mc_entity *living = source_app->i;
    if (living == NULL)
        return fail(c, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE, "actual N Living.N receiver", 430,
                    false);
    const mc_entity_sleeping_status status =
        mc_entity_living_is_sleeping(living, out, &c->sleeping);
    if (status != MC_ENTITY_SLEEPING_OK) {
        c->failure.entity.status = c->sleeping.entity;
        c->failure.player = c->sleeping.player;
        return fail(c,
                    status == MC_ENTITY_SLEEPING_UNIMPLEMENTED_DEPENDENCY
                        ? MC_CLIENT_RENDER_GLOBAL_UNIMPLEMENTED_DEPENDENCY
                        : MC_CLIENT_RENDER_GLOBAL_INVALID_PHASE,
                    "N actual Living.N concrete dispatch", 430, true);
    }
    if (!*out)
        ++c->state.camera_skips;
    return true;
}

static bool floor_coordinate(mc_client_render_global_entities_call *c, double value,
                             unsigned int pc, int32_t *out) {
    if (c->services.math == NULL)
        return fail(c, MC_CLIENT_RENDER_GLOBAL_UNIMPLEMENTED_DEPENDENCY,
                    "N reached canonical Math namespace", pc, true);
    if (mc_math_classes_ensure(c->services.math, &c->failure.math) != MC_MATH_CLASS_OK)
        return fail(c, MC_CLIENT_RENDER_GLOBAL_INVALID_PHASE, "N actual Math floor active use", pc,
                    true);
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u)
        return fail(c, MC_CLIENT_RENDER_GLOBAL_UNSUPPORTED_FP, "N floor strict floating controls",
                    pc, true);
    *out = mc_math_floor_double(value);
    return true;
}

static bool present(mc_client_render_global_entities_call *c, bool *out) {
    mc_entity_base_fields *f = mc_entity_runtime_fields(c->current);
    int32_t x, y, z;
    if (!floor_coordinate(c, f->y, 444, &y))
        return false;
    if (y < 0)
        y = 0;
    if (y >= 128)
        y = 127;
    mc_world_runtime *world = c->state.owner->fields.k; /* Capture470 before the floor children. */
    f = mc_entity_runtime_fields(c->current);
    if (!floor_coordinate(c, f->x, 478, &x))
        return false;
    f = mc_entity_runtime_fields(c->current);
    if (!floor_coordinate(c, f->z, 488, &z))
        return false;
    if (world == NULL)
        return fail(c, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE, "actual N presence World receiver",
                    491, false);
    if (!lease_world(c, world, 491))
        return false;
    ++c->state.presence_checks;
    const mc_world_storage_status status =
        mc_world_runtime_render_block_present(world, x, y, z, out, &c->failure.world);
    return status == MC_WORLD_STORAGE_OK || world_failure(c, status, 491);
}

static bool entities(mc_client_render_global_entities_call *c) {
    mc_world_runtime *world = world_at(c, 249);
    if (world == NULL || !lease_world(c, world, 249))
        return false;
    c->list_world = world;
    mc_world_storage_status status =
        mc_world_runtime_render_entities_list(world, &c->state.entities_list, &c->failure.world);
    if (status != MC_WORLD_STORAGE_OK)
        return world_failure(c, status, 249);
    int32_t count;
    if (!list_count(c, world, c->state.entities_list, 257, &count))
        return false;
    c->state.owner->fields.J = count;
    c->state.last_store_pc = 262;
    if (!weather(c))
        return false;
    for (c->state.index = 0;; c->state.index = mc_java_i32_add(c->state.index, 1)) {
        if (!list_count(c, c->list_world, c->state.entities_list, 345, &count))
            return false;
        if (c->state.index >= count)
            return true;
        if (!list_get(c, c->list_world, c->state.entities_list, 357))
            return false;
        ++c->state.entities_visited;
        bool visible;
        if (!entity_distance(c, 370, &visible))
            return false;
        if (visible) {
            mc_entity_base_fields *f = mc_entity_runtime_fields(c->current);
            if (!f->bM && !entity_frustum(c, &visible))
                return false;
        }
        if (visible && !camera_visible(c, &visible))
            return false;
        if (visible && !present(c, &visible))
            return false;
        if (visible) {
            c->state.owner->fields.K = mc_java_i32_add(c->state.owner->fields.K, 1);
            c->state.last_store_pc = 507;
            return draw_entity(c, 510, 516);
        }
        release_current(c);
    }
}

static bool tiles(mc_client_render_global_entities_call *c) {
    c->state.index = 0;
    mc_client_render_list *list = c->state.owner->fields.a;
    if (list == NULL)
        return fail(c, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE, "actual N Tile List.size", 534,
                    false);
    if (list->count > list->capacity || list->capacity > INT32_MAX ||
        (list->capacity != 0 && list->entries == NULL))
        return fail(c, MC_CLIENT_RENDER_GLOBAL_INVALID_PHASE, "N Tile List native extent", 534,
                    true);
    if (list->count == 0)
        return true;
    mc_client_tile_renderer_dispatcher *receiver = NULL;
    if (mc_client_tile_renderer_static_get(c->state.owner->tile_consumer.classes, &receiver,
                                           &c->tiles) != MC_CLIENT_TILE_RENDERER_OK)
        return tile_failure(c, 542);
    if (receiver != NULL && mc_client_tile_renderer_dispatcher_retain(receiver, &c->tiles) !=
                                MC_CLIENT_TILE_RENDERER_OK)
        return tile_failure(c, 542);
    c->draw_ll = receiver;
    /* The first nonempty row needs the actual DK/JE ownership ledger before
     * interpreting its void*. Its draw and later loop iterations are unreached. */
    return fail(c, MC_CLIENT_RENDER_GLOBAL_UNIMPLEMENTED_DEPENDENCY,
                "N reached rendered Tile occurrence ownership", 551, true);
}
