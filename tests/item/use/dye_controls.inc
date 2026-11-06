/* Declared flat terrain on actual owners. The public IZ wrapper is always
 * executed; NULL Player cases deliberately reach its late statistic NPE. */
typedef struct dye_observer {
    mc_registry_classes *registry;
    mc_item_stack *stack;
    size_t cells, fail_cell;
    int32_t expected_count;
    bool valid, replace_random, replace_count;
} dye_observer;

static mc_world_storage_status dye_cell(void *context, mc_world_runtime *world, int32_t x,
                                        int32_t y, int32_t z, mc_world_storage_error *error) {
    (void)x;
    (void)y;
    (void)z;
    dye_observer *o = context;
    int32_t count;
    o->valid = o->valid && mc_item_stack_source_count_get(o->stack, &count) == MC_ITEM_STACK_OK &&
               count == o->expected_count;
    ++o->cells;
    if (o->replace_random)
        o->registry->items.b = world->fields.r;
    if (o->replace_count) {
        o->valid = o->valid && mc_item_stack_source_count_set(o->stack, 99) == MC_ITEM_STACK_OK;
        o->expected_count = 99;
    }
    if (o->fail_cell != 0 && o->cells == o->fail_cell) {
        if (error != NULL)
            *error =
                (mc_world_storage_error){MC_WORLD_STORAGE_LIMIT, "declared Dye cell refusal", 0};
        return MC_WORLD_STORAGE_LIMIT;
    }
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status dye_range(void *context, mc_world_runtime *world, int32_t x,
                                         int32_t y, int32_t z, int32_t max_x, int32_t max_y,
                                         int32_t max_z, mc_world_storage_error *error) {
    (void)context;
    (void)world;
    (void)x;
    (void)y;
    (void)z;
    (void)max_x;
    (void)max_y;
    (void)max_z;
    (void)error;
    return MC_WORLD_STORAGE_OK;
}

static size_t dye_index(int32_t y) {
    return ((size_t)8 * 16u + 8u) * 128u + (size_t)y;
}

static void dye_terrain(mc_chunk_state *chunk, uint8_t target, int32_t y) {
    memset(chunk->data->blocks, 0, MC_CHUNK_BLOCKS);
    memset(chunk->data->metadata, 0, MC_CHUNK_NIBBLE_BYTES);
    memset(chunk->data->skylight, 0x0f, MC_CHUNK_NIBBLE_BYTES);
    memset(chunk->data->block_light, 0, MC_CHUNK_NIBBLE_BYTES);
    memset(chunk->data->heightmap, 64, MC_CHUNK_HEIGHTMAP_BYTES);
    for (size_t x = 0; x < 16; ++x)
        for (size_t z = 0; z < 16; ++z)
            chunk->data->blocks[(x * 16u + z) * 128u + 63u] = 2;
    chunk->data->blocks[dye_index(y)] = target;
}

static bool dye_prefix_controls(fixture *f, mc_chunk_state *chunk, dye_observer *observer) {
    static const int32_t metadata[] = {0, 14, 16, -1, INT32_MIN, INT32_MAX};
    for (size_t i = 0; i < sizeof metadata / sizeof *metadata; ++i) {
        mc_item_stack *stack = NULL;
        CHECK(use_stack(351, INT32_MIN, metadata[i], &stack));
        mc_item_use_call *call = NULL;
        mc_item_use_state state;
        CHECK(mc_item_use_on_block_begin(f->registry, stack, NULL, NULL, INT32_MIN, INT32_MAX,
                                         INT32_MIN, INT32_MAX, &call, NULL) == MC_ITEM_USE_OK);
        CHECK(mc_item_use_inspect(call, &state) && state.returned && !state.result &&
              !state.count_read && !state.count_stored && !state.statistic_entered &&
              use_retire(&call));
        mc_item_stack_release(stack);
    }
    static const uint8_t targets[] = {6, 59, 2};
    for (size_t i = 0; i < sizeof targets / sizeof *targets; ++i) {
        dye_terrain(chunk, targets[i], 64);
        mc_item_stack *stack = NULL;
        CHECK(use_stack(351, 2, 15, &stack));
        *observer = (dye_observer){.registry = f->registry, .stack = stack, .valid = true};
        f->world->fields.B = true;
        const mc_java_random before = *f->registry->items.b;
        const mc_java_random world_before = *f->world->fields.r;
        mc_item_use_call *call = NULL;
        mc_item_use_state state;
        mc_item_use_error error;
        const mc_item_use_status status = mc_item_use_on_block_begin(
            f->registry, stack, NULL, f->world, 8, 64, 8, INT32_MIN, &call, &error);
        f->world->fields.B = false;
        CHECK(status == MC_ITEM_USE_SOURCE_NULL && error.site.pc == 34 &&
              strcmp(error.site.class_name, "iz") == 0 && mc_item_use_inspect(call, &state) &&
              state.result && !state.count_read && !state.count_stored && !state.setter_returned &&
              observer->cells == 0 && f->registry->items.b->state == before.state &&
              f->world->fields.r->state == world_before.state && use_retire(&call));
        mc_item_stack_release(stack);
    }
    return true;
}

static bool dye_crop_controls(fixture *f, mc_chunk_state *chunk, dye_observer *observer) {
    static const int32_t counts[] = {2, 0, -7, INT32_MIN};
    for (size_t i = 0; i < sizeof counts / sizeof *counts; ++i) {
        dye_terrain(chunk, 59, 64);
        mc_item_stack *stack = NULL;
        CHECK(use_stack(351, counts[i], 15, &stack));
        *observer = (dye_observer){
            .registry = f->registry, .stack = stack, .expected_count = counts[i], .valid = true};
        mc_item_use_call *call = NULL;
        mc_item_use_state state;
        mc_item_use_error error;
        CHECK(mc_item_use_on_block_begin(f->registry, stack, NULL, f->world, 8, 64, 8, 0, &call,
                                         &error) == MC_ITEM_USE_SOURCE_NULL);
        int32_t count, metadata;
        CHECK(error.site.pc == 34 && mc_item_use_inspect(call, &state) && state.result &&
              state.count_stored && observer->valid && observer->cells == 1 &&
              mc_item_stack_source_count_get(stack, &count) == MC_ITEM_STACK_OK &&
              count == mc_java_i32_sub(counts[i], 1) &&
              mc_world_runtime_metadata_get(f->world, 8, 64, 8, &metadata, NULL) ==
                  MC_WORLD_STORAGE_OK &&
              metadata == 7 && use_retire(&call));
        mc_item_stack_release(stack);
    }
    for (size_t mode = 0; mode < 2; ++mode) {
        dye_terrain(chunk, 59, 64);
        mc_item_stack *stack = NULL;
        CHECK(use_stack(351, 2, 15, &stack));
        *observer = (dye_observer){.registry = f->registry,
                                   .stack = stack,
                                   .expected_count = 2,
                                   .valid = true,
                                   .fail_cell = mode == 0 ? 1u : 0u,
                                   .replace_count = mode == 1};
        mc_item_use_call *call = NULL;
        mc_item_use_error error;
        const mc_item_use_status status = mc_item_use_on_block_begin(
            f->registry, stack, NULL, f->world, 8, 64, 8, 1, &call, &error);
        int32_t count;
        CHECK(status == (mode == 0 ? MC_ITEM_USE_WORLD : MC_ITEM_USE_SOURCE_NULL) &&
              observer->valid && observer->cells == 1 &&
              mc_item_stack_source_count_get(stack, &count) == MC_ITEM_STACK_OK &&
              count == (mode == 0 ? 2 : 98) &&
              (mode != 0 ||
               (error.site.pc == 102 && error.mutation != NULL && error.native_policy)) &&
              use_retire(&call));
        mc_item_stack_release(stack);
    }
    return true;
}

static bool dye_grass_controls(fixture *f, mc_chunk_state *chunk, dye_observer *observer) {
    static const struct {
        size_t fail_cell;
        bool replace_random, replace_count;
        uint64_t item_after, world_after;
        size_t cells;
        int32_t count;
    } cases[] = {
        {0, false, false, UINT64_C(0x9c40009bfbc6), UINT64_C(0x0005deece624), 23, 1},
        {1, false, false, UINT64_C(0xd4d95138ab6f), UINT64_C(0x0005deece624), 1, 1},
        {3, false, false, UINT64_C(0x005ae0b4c479), UINT64_C(0x0005deece624), 3, 1},
        {0, true, false, UINT64_C(0xd4d95138ab6f), UINT64_C(0x68bef9983fd3), 20, 1},
        {0, false, true, UINT64_C(0x9c40009bfbc6), UINT64_C(0x0005deece624), 23, 99},
    };

    mc_java_random *const item_random = f->registry->items.b;
    for (size_t i = 0; i < sizeof cases / sizeof *cases; ++i) {
        dye_terrain(chunk, 2, 63);
        mc_java_random_reseed(item_random, 0);
        mc_java_random_reseed(f->world->fields.r, 73);
        mc_item_stack *stack = NULL;
        CHECK(use_stack(351, 2, 15, &stack));
        *observer = (dye_observer){.registry = f->registry,
                                   .stack = stack,
                                   .expected_count = 1,
                                   .valid = true,
                                   .fail_cell = cases[i].fail_cell,
                                   .replace_random = cases[i].replace_random,
                                   .replace_count = cases[i].replace_count};
        const size_t bindings = f->world->persistent_bindings;
        mc_item_use_call *call = NULL;
        mc_item_use_state state;
        mc_item_use_error error;
        const mc_item_use_status status = mc_item_use_on_block_begin(
            f->registry, stack, NULL, f->world, 8, 63, 8, INT32_MAX, &call, &error);
        f->registry->items.b = item_random;
        int32_t count;
        CHECK(status == (cases[i].fail_cell == 0 ? MC_ITEM_USE_SOURCE_NULL : MC_ITEM_USE_WORLD) &&
              mc_item_use_inspect(call, &state) && state.count_stored && observer->valid &&
              observer->cells == cases[i].cells &&
              mc_item_stack_source_count_get(stack, &count) == MC_ITEM_STACK_OK &&
              count == cases[i].count && item_random->state == cases[i].item_after &&
              f->world->fields.r->state == cases[i].world_after && error.acquisition != NULL &&
              (cases[i].fail_cell == 0 ? error.site.pc == 34 : error.mutation != NULL) &&
              use_retire(&call) && f->world->persistent_bindings == bindings &&
              f->world->active_calls == 0);
        mc_item_stack_release(stack);
    }
    dye_terrain(chunk, 2, 63);
    mc_item_stack *stack = NULL;
    CHECK(use_stack(351, 2, 15, &stack));
    *observer = (dye_observer){.registry = f->registry, .stack = stack, .valid = true};
    f->registry->items.b = NULL;
    mc_item_use_call *call = NULL;
    mc_item_use_error error;
    const mc_item_use_status status =
        mc_item_use_on_block_begin(f->registry, stack, NULL, f->world, 8, 63, 8, 1, &call, &error);
    f->registry->items.b = item_random;
    int32_t count;
    CHECK(status == MC_ITEM_USE_SOURCE_NULL && error.site.pc == 296 && error.source_exception &&
          !error.native_policy && observer->cells == 0 &&
          mc_item_stack_source_count_get(stack, &count) == MC_ITEM_STACK_OK && count == 1 &&
          use_retire(&call));
    mc_item_stack_release(stack);
    return true;
}

static bool dye_sapling_controls(fixture *f, mc_chunk_state *chunk, dye_observer *observer) {
    dye_terrain(chunk, 6, 64);
    chunk->data->blocks[dye_index(65)] = 1;
    const size_t index = dye_index(64);
    chunk->data->metadata[index / 2u] = 10;
    mc_java_random_reseed(f->registry->items.b, 0);
    mc_java_random_reseed(f->world->fields.r, 73);
    mc_item_stack *stack = NULL;
    CHECK(use_stack(351, 2, 15, &stack));
    *observer =
        (dye_observer){.registry = f->registry, .stack = stack, .expected_count = 2, .valid = true};
    mc_item_use_call *call = NULL;
    mc_item_use_state state;
    mc_item_use_error error;
    CHECK(mc_item_use_on_block_begin(f->registry, stack, NULL, f->world, 8, 64, 8, 4, &call,
                                     &error) == MC_ITEM_USE_SOURCE_NULL);
    int32_t metadata, count;
    CHECK(error.site.pc == 34 && mc_item_use_inspect(call, &state) && state.result &&
          state.count_stored && observer->cells == 0 && chunk->data->blocks[index] == 6 &&
          mc_world_runtime_metadata_get(f->world, 8, 64, 8, &metadata, NULL) ==
              MC_WORLD_STORAGE_OK &&
          metadata == 2 && mc_item_stack_source_count_get(stack, &count) == MC_ITEM_STACK_OK &&
          count == 1 && f->registry->items.b->state == UINT64_C(0x0005deece66d) &&
          f->world->fields.r->state == UINT64_C(0xb97422d2555f) && use_retire(&call));
    mc_item_stack_release(stack);
    return true;
}

static bool dye_cast_controls(fixture *f, mc_chunk_state *chunk) {
    size_t selected = 96;
    for (size_t i = 0; i < 96; ++i)
        if (strcmp(mc_registry_block_static_name(i), "z") == 0)
            selected = i;
    CHECK(selected < 96);
    mc_block *const sapling = f->registry->blocks.named[selected];
    f->registry->blocks.named[selected] = f->registry->blocks.m[2];
    dye_terrain(chunk, 2, 64);
    mc_item_stack *stack = NULL;
    CHECK(use_stack(351, 2, 15, &stack));
    mc_item_use_call *call = NULL;
    mc_item_use_error error;
    const mc_item_use_status status =
        mc_item_use_on_block_begin(f->registry, stack, NULL, f->world, 8, 64, 8, 1, &call, &error);
    f->registry->blocks.named[selected] = sapling;
    CHECK(status == MC_ITEM_USE_SOURCE_CAST && error.source_exception && !error.native_policy &&
          strcmp(error.exception_class, "java/lang/ClassCastException") == 0 &&
          error.site.pc == 42 && use_retire(&call));
    mc_item_stack_release(stack);
    return true;
}

#ifdef MC_PLACEMENT_REAL_RENDER_GLOBAL
static bool dye_producer_controls(fixture *f, mc_chunk_state *chunk) {
    dye_terrain(chunk, 2, 63);
    mc_java_random_reseed(f->registry->items.b, 0);
    mc_client_render_engine_classes *graphics = NULL;
    mc_client_render_global *owner = NULL;
    mc_client_render_global_error *detail = calloc(1, sizeof(*detail));
    CHECK(detail != NULL &&
          mc_client_render_engine_classes_create(NULL, &graphics) == MC_CLIENT_RENDER_ENGINE_OK &&
          mc_client_render_global_allocate(graphics, NULL, &owner, detail) ==
              MC_CLIENT_RENDER_GLOBAL_OK);
    owner->fields.p = owner->fields.q = owner->fields.r = 1;
    CHECK(mc_render_global_access_create(owner, &owner->access, detail) ==
              MC_CLIENT_RENDER_GLOBAL_OK &&
          mc_render_global_access_source_add(owner->access, f->world, detail) ==
              MC_CLIENT_RENDER_GLOBAL_OK);
    mc_item_stack *stack = NULL;
    CHECK(use_stack(351, 2, 15, &stack));
    mc_item_use_call *call = NULL;
    mc_item_use_error error;
    CHECK(mc_item_use_on_block_begin(f->registry, stack, NULL, f->world, 8, 63, 8, 1, &call,
                                     &error) == MC_ITEM_USE_WORLD);
    CHECK(error.retained_producers != 0 && error.source_exception && !error.native_policy &&
          error.mutation != NULL &&
          mc_render_global_access_detach_all(owner->access, detail) ==
              MC_CLIENT_RENDER_GLOBAL_BUSY);
    CHECK(use_retire(&call) &&
          mc_render_global_access_detach_all(owner->access, detail) == MC_CLIENT_RENDER_GLOBAL_OK &&
          mc_render_global_access_retire(&owner->access, detail) == MC_CLIENT_RENDER_GLOBAL_OK &&
          mc_client_render_global_release(&owner, detail) == MC_CLIENT_RENDER_GLOBAL_OK &&
          mc_client_render_engine_classes_release(graphics) == MC_CLIENT_RENDER_ENGINE_OK);
    mc_item_stack_release(stack);
    free(detail);
    return true;
}
#endif

static bool dye_controls(fixture *f, mc_chunk_state *chunk) {
    dye_observer observer = {.registry = f->registry, .valid = true};
    const mc_world_access_callbacks callbacks = {.cell = dye_cell, .range = dye_range};
    mc_world_access *access = NULL;
    CHECK(mc_world_access_create(&callbacks, &observer, &access, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_add_world_access(f->world, access, NULL) == MC_WORLD_STORAGE_OK);
    const mc_java_random item_before = *f->registry->items.b;
    const mc_java_random world_before = *f->world->fields.r;
    CHECK(dye_prefix_controls(f, chunk, &observer) && dye_crop_controls(f, chunk, &observer) &&
          dye_grass_controls(f, chunk, &observer) && dye_sapling_controls(f, chunk, &observer) &&
          dye_cast_controls(f, chunk));
    bool removed = false;
    CHECK(mc_world_runtime_remove_world_access(f->world, access, &removed, NULL) ==
              MC_WORLD_STORAGE_OK &&
          removed && mc_world_access_free(access, NULL) == MC_WORLD_STORAGE_OK);
#ifdef MC_PLACEMENT_REAL_RENDER_GLOBAL
    CHECK(dye_producer_controls(f, chunk));
#endif
    *f->registry->items.b = item_before;
    *f->world->fields.r = world_before;
    return true;
}
