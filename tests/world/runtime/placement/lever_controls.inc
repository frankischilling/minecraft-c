typedef struct lever_observer {
    mc_world_placement_error *placement;
    unsigned int ranges, sounds;
    bool refuse_sound, busy, append_null_sound, null_added;
    float pitch;
} lever_observer;

static mc_world_storage_status lever_observe_cell(void *context, mc_world_runtime *world, int32_t x,
                                                  int32_t y, int32_t z,
                                                  mc_world_storage_error *error) {
    (void)context;
    (void)world;
    (void)x;
    (void)y;
    (void)z;
    (void)error;
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status lever_observe_range(void *context, mc_world_runtime *world,
                                                   int32_t x, int32_t y, int32_t z, int32_t max_x,
                                                   int32_t max_y, int32_t max_z,
                                                   mc_world_storage_error *error) {
    lever_observer *o = context;
    ++o->ranges;
    o->busy = !mc_world_placement_error_retire(o->placement) &&
              o->placement->cleanup.status == MC_WORLD_STORAGE_BUSY;
    int32_t metadata;
    if (x != 8 || y != 64 || z != 8 || max_x != x || max_y != y || max_z != z || !o->busy ||
        mc_world_runtime_metadata_get(world, x, y, z, &metadata, error) != MC_WORLD_STORAGE_OK ||
        metadata != 13) {
        *error = (mc_world_storage_error){MC_WORLD_STORAGE_INVALID_DATA,
                                          "Lever range after actual metadata mutation", 0};
        return error->status;
    }
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status lever_observe_sound(void *context, mc_world_runtime *world,
                                                   const mc_registry_string *name, double x,
                                                   double y, double z, float volume, float pitch,
                                                   mc_world_storage_error *error) {
    (void)world;
    lever_observer *o = context;
    ++o->sounds;
    o->pitch = pitch;
    if (o->ranges != 1 || name == NULL || x != 8.5 || y != 64.5 || z != 8.5 || volume != 0.3f ||
        pitch != 0.6f || o->refuse_sound) {
        *error = (mc_world_storage_error){MC_WORLD_STORAGE_INVALID_DATA,
                                          "Lever authored sound refusal after range", 0};
        return error->status;
    }
    if (o->append_null_sound) {
        const mc_world_storage_status status =
            mc_world_runtime_add_world_access(world, NULL, error);
        if (status != MC_WORLD_STORAGE_OK)
            return status;
        o->null_added = true;
    }
    return MC_WORLD_STORAGE_OK;
}

/* Genuine World/Chunk controls. These constructed inputs complement the
 * original method replay; they do not establish a loaded graphical scene. */
static bool lever_placement_controls(fixture *f) {
    bool ok = false, value = false;
    mc_world_placement_error e = {0};
    lever_observer observer = {.placement = &e};
    mc_world_runtime *world = f->worlds[0];
    const mc_block *lever = NULL;
    const size_t bindings = world->persistent_bindings;
    const bool remote = world->fields.B;
    mc_java_random *const random = world->fields.r;
    mc_world_access *access = NULL;
    bool registered = false;
    CHECK(mc_registry_block_get(f->registry[0], 69, &lever, NULL) == MC_REGISTRY_OK);
    const int32_t xs[] = {7, 9, 8, 8, 8};
    const int32_t ys[] = {64, 64, 64, 64, 63};
    const int32_t zs[] = {8, 8, 7, 9, 8};
    for (size_t i = 0; i < 5; ++i) {
        boxes_air(f, 0);
        CHECK(boxes_cell(f, 0, xs[i], ys[i], zs[i], 1, 0));
        CHECK(mc_world_block_can_place_base(f->registry[0], lever, world, 8, 64, 8, &value, &e) ==
                  MC_WORLD_STORAGE_OK &&
              value && e.query_count == 0 && e.support_query_count == i + 1 &&
              e.base.pc == 14u + 15u * (unsigned int)i && mc_world_placement_error_retire(&e));
    }
    boxes_air(f, 0);
    CHECK(boxes_cell(f, 0, 8, 63, 8, 85, 0));
    CHECK(mc_world_block_can_place_base(f->registry[0], lever, world, 8, 64, 8, &value, &e) ==
              MC_WORLD_STORAGE_OK &&
          !value && e.support_query_count == 5 && e.query_count == 0 && e.base.pc == 76 &&
          mc_world_placement_error_retire(&e)); /* Fence is not Lever support. */
    /* Invalid faces return false without dereferencing the actual null World. */
    const int32_t invalid_faces[] = {INT32_MIN, -1, 0, 6, INT32_MAX};
    for (size_t i = 0; i < COUNT(invalid_faces); ++i)
        CHECK(mc_world_block_can_place(f->registry[0], lever, NULL, 8, 64, 8, invalid_faces[i],
                                       &value, &e) == MC_WORLD_STORAGE_OK &&
              !value && e.returned && e.support_query_count == 0 && !e.base_entered &&
              e.site.pc == 106 && mc_world_placement_error_retire(&e));
    for (int32_t face = 1; face <= 5; ++face) {
        boxes_air(f, 0);
        const size_t i = face == 1 ? 4u : (size_t)(5 - face);
        CHECK(boxes_cell(f, 0, xs[i], ys[i], zs[i], 1, 0));
        CHECK(mc_world_block_can_place(f->registry[0], lever, world, 8, 64, 8, face, &value, &e) ==
                  MC_WORLD_STORAGE_OK &&
              value && e.support_query_count == 1 && e.query_count == 0 && !e.base_entered &&
              mc_world_placement_error_retire(&e));
    }
    boxes_air(f, 0);
    for (size_t i = 0; i < 5; ++i)
        CHECK(boxes_cell(f, 0, xs[i], ys[i], zs[i], 1, 0));
    for (int32_t face = 1; face <= 5; ++face) {
        CHECK(boxes_cell(f, 0, 8, 64, 8, 69, 8));
        mc_java_random expected = *random;
        int32_t choice = 0;
        if (face == 1)
            CHECK(mc_java_random_next_bounded(&expected, 2, &choice));
        const int32_t expected_metadata = face == 1 ? 13 + choice : 14 - face;
        CHECK(mc_world_block_placed_face(f->registry[0], lever, world, 8, 64, 8, face, &e) ==
                  MC_WORLD_STORAGE_OK &&
              e.returned && e.site.pc == 193 && e.support_query_count == 1 && e.query_count == 1 &&
              mc_world_placement_error_retire(&e));
        int32_t metadata;
        CHECK(mc_world_runtime_metadata_get(world, 8, 64, 8, &metadata, NULL) ==
                  MC_WORLD_STORAGE_OK &&
              metadata == expected_metadata && memcmp(random, &expected, sizeof(expected)) == 0);
    }
    world->fields.B = false;
    for (int32_t metadata = 0; metadata < 16; ++metadata) {
        CHECK(boxes_cell(f, 0, 8, 64, 8, 69, metadata));
        const mc_java_random expected = *random;
        CHECK(mc_world_block_activate(f->registry[0], lever, world, 8, 64, 8, NULL, &value, &e) ==
                  MC_WORLD_STORAGE_OK &&
              value && e.returned && e.site.pc == 219 && e.remote_read && !e.remote &&
              e.query_count == 1 && mc_world_placement_error_retire(&e));
        int32_t changed;
        CHECK(mc_world_runtime_metadata_get(world, 8, 64, 8, &changed, NULL) ==
                  MC_WORLD_STORAGE_OK &&
              changed == (metadata ^ 8) && memcmp(random, &expected, sizeof(expected)) == 0);
    }
    const mc_world_access_callbacks callbacks = {
        .cell = lever_observe_cell, .range = lever_observe_range, .sound = lever_observe_sound};
    CHECK(mc_world_access_create(&callbacks, &observer, &access, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_add_world_access(world, access, NULL) == MC_WORLD_STORAGE_OK);
    registered = true;
    for (unsigned int refusal = 0; refusal < 2; ++refusal) {
        CHECK(boxes_cell(f, 0, 8, 64, 8, 69, 5));
        observer.ranges = observer.sounds = 0;
        observer.refuse_sound = refusal != 0;
        value = false;
        const mc_world_storage_status status =
            mc_world_block_activate(f->registry[0], lever, world, 8, 64, 8, NULL, &value, &e);
        CHECK(status == (refusal ? MC_WORLD_STORAGE_INVALID_DATA : MC_WORLD_STORAGE_OK) &&
              value == !refusal && e.returned == !refusal && observer.ranges == 1 &&
              observer.sounds == 1 && observer.busy && observer.pitch == 0.6f &&
              e.site.pc == (refusal ? 97u : 219u) && !e.source_exception && !e.source_error &&
              (!refusal || e.native_policy) && e.retained_producers == 0);
        int32_t metadata;
        CHECK(mc_world_runtime_metadata_get(world, 8, 64, 8, &metadata, NULL) ==
                  MC_WORLD_STORAGE_OK &&
              metadata == 13 && mc_world_placement_error_retire(&e));
    }
    /* Append the nullable occurrence during the real sound callback: metadata
     * and range have returned before FD's next sound-list lookup invokes it. */
    CHECK(boxes_cell(f, 0, 8, 64, 8, 69, 5));
    observer.ranges = observer.sounds = 0;
    observer.refuse_sound = false;
    observer.append_null_sound = true;
    value = true;
    const mc_world_storage_status null_sound =
        mc_world_block_activate(f->registry[0], lever, world, 8, 64, 8, NULL, &value, &e);
    bool removed;
    CHECK(observer.null_added &&
          mc_world_runtime_remove_world_access(world, NULL, &removed, NULL) ==
              MC_WORLD_STORAGE_OK &&
          removed);
    observer.null_added = false;
    int32_t metadata;
    const char *exception = NULL;
    CHECK(null_sound == MC_WORLD_STORAGE_UNSUPPORTED_STATE && value && !e.returned &&
          e.site.pc == 97 && e.source_exception && !e.source_error && !e.native_policy &&
          e.exception_class != NULL &&
          strcmp(e.exception_class, "java/lang/NullPointerException") == 0 &&
          e.retained_producers == 0 && observer.ranges == 1 && observer.sounds == 1 &&
          observer.busy && e.mutation != NULL && e.mutation->world_access_bytecode == 41 &&
          e.mutation->world_access_failure.receiver == NULL &&
          e.mutation->world_access_failure.render_global == NULL &&
          e.mutation->world_access_failure.receipt == NULL &&
          mc_world_access_failure_source_exception(&e.mutation->world_access_failure, &exception) &&
          strcmp(exception, e.exception_class) == 0 &&
          mc_world_runtime_metadata_get(world, 8, 64, 8, &metadata, NULL) == MC_WORLD_STORAGE_OK &&
          metadata == 13 && world->active_calls == 0 && mc_world_placement_error_retire(&e));
    CHECK(mc_world_runtime_remove_world_access(world, access, &removed, NULL) ==
              MC_WORLD_STORAGE_OK &&
          removed);
    registered = false;
    CHECK(mc_world_access_free(access, NULL) == MC_WORLD_STORAGE_OK);
    access = NULL;
    world->fields.B = true;
    world->fields.r = NULL;
    CHECK(mc_world_block_activate(f->registry[0], lever, world, INT32_MIN, INT32_MAX, INT32_MIN,
                                  NULL, &value, &e) == MC_WORLD_STORAGE_OK &&
          value && e.returned && e.site.pc == 8 && e.query_count == 0 && e.mutation == NULL &&
          mc_world_placement_error_retire(&e));
    CHECK(mc_world_block_placed_face(f->registry[0], lever, world, 8, 64, 8, 1, &e) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          e.source_exception && !e.native_policy && !e.returned && e.site.pc == 52 &&
          mc_world_placement_error_retire(&e));
    CHECK(world->persistent_bindings == bindings && world->active_calls == 0);
    ok = true;
cleanup:
    world->fields.B = remote;
    world->fields.r = random;
    if (registered && observer.null_added) {
        bool ignored;
        (void)mc_world_runtime_remove_world_access(world, NULL, &ignored, NULL);
    }
    (void)mc_world_placement_error_retire(&e);
    if (registered) {
        bool ignored;
        (void)mc_world_runtime_remove_world_access(world, access, &ignored, NULL);
    }
    if (access != NULL && mc_world_access_free(access, NULL) != MC_WORLD_STORAGE_OK)
        return false;
    boxes_air(f, 0);
    return ok;
}
