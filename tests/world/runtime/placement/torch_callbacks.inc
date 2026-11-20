static mc_world_storage_status torch_metadata_prefix(placement_observer *o, mc_world_runtime *world,
                                                     mc_world_storage_error *error) {
    ++o->calls;
    o->refused = !mc_world_placement_error_retire(o->placement) &&
                 o->placement->cleanup.status == MC_WORLD_STORAGE_BUSY &&
                 o->placement->site.pc == 128 && o->placement->work != NULL;
    int32_t metadata = -1;
    o->changed =
        mc_world_runtime_metadata_get(world, 8, 64, 8, &metadata, error) == MC_WORLD_STORAGE_OK &&
        metadata == 5;
    if (!o->refused || !o->changed) {
        *error = (mc_world_storage_error){MC_WORLD_STORAGE_INVALID_DATA,
                                          "Torch notifying metadata callback prefix", 0};
        return error->status;
    }
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status torch_metadata_native(void *context, mc_world_runtime *world,
                                                     int32_t x, int32_t y, int32_t z,
                                                     mc_world_storage_error *error) {
    (void)x;
    (void)y;
    (void)z;
    const mc_world_storage_status status = torch_metadata_prefix(context, world, error);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    *error = (mc_world_storage_error){MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                      "Authored Torch metadata notification refusal", 0};
    return error->status;
}

static mc_world_storage_status torch_metadata_source(void *context, mc_world_runtime *world,
                                                     int32_t x, int32_t y, int32_t z,
                                                     mc_world_storage_error *error,
                                                     mc_world_access_failure *failure) {
    (void)x;
    (void)y;
    (void)z;
    const mc_world_storage_status status = torch_metadata_prefix(context, world, error);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    return placement_source_failure(context, world, error, failure);
}

static int torch_metadata_callback(source_save_fixture *s, bool source_exception) {
    fixture *f = &s->game;
    VERIFY(ss_open(s) == 0);
    mc_world_placement_error e = {0};
    placement_observer observer = {.placement = &e};
    if (source_exception) {
        VERIFY(placement_real_producer(&observer, f->worlds[0]) == 0 &&
               mc_render_global_access_source_remove(
                   observer.bridge, f->worlds[0], observer.n_error) == MC_CLIENT_RENDER_GLOBAL_OK);
        const mc_world_access_source_callbacks callbacks = {.cell = torch_metadata_source};
        VERIFY(mc_world_access_source_create(&callbacks, &observer, &observer.access, NULL) ==
               MC_WORLD_STORAGE_OK);
    } else {
        const mc_world_access_callbacks callbacks = {.cell = torch_metadata_native};
        VERIFY(mc_world_access_create(&callbacks, &observer, &observer.access, NULL) ==
               MC_WORLD_STORAGE_OK);
    }
    const size_t cell = ((8u << 11u) | (8u << 7u) | 64u);
    /* Declared component inputs: Torch over Stone, metadata0 and Block.t50=true
     * select the actual notifying metadata callback. No code body is replaced. */
    f->states[0]->data->blocks[cell] = 50;
    f->states[0]->data->blocks[cell - 1] = 1;
    bool changed = false;
    VERIFY(mc_world_runtime_metadata_set_quiet(f->worlds[0], 8, 64, 8, 0, &changed, NULL) ==
               MC_WORLD_STORAGE_OK &&
           changed);
    const bool saved_notify = f->registry[0]->blocks.t[50];
    f->registry[0]->blocks.t[50] = true;
    VERIFY(mc_world_runtime_add_world_access(f->worlds[0], observer.access, NULL) ==
           MC_WORLD_STORAGE_OK);
    const mc_block *block = NULL;
    VERIFY(mc_registry_block_get(f->registry[0], 50, &block, NULL) == MC_REGISTRY_OK);
    const mc_world_storage_status status =
        mc_world_block_placed_face(f->registry[0], block, f->worlds[0], 8, 64, 8, 1, &e);
    f->registry[0]->blocks.t[50] = saved_notify;
    VERIFY(status == MC_WORLD_STORAGE_UNSUPPORTED_STATE && !e.returned && e.site.pc == 128 &&
           e.mutation != NULL && e.mutation->wrapper_bytecode == 40 && observer.calls == 1 &&
           observer.refused && observer.changed && e.source_exception == source_exception &&
           e.native_policy != source_exception && !e.source_error &&
           e.retained_producers == (source_exception ? 1u : 0u));
    if (source_exception)
        VERIFY(e.exception_class != NULL &&
               strcmp(e.exception_class, "java/lang/NullPointerException") == 0 &&
               observer.receipt != NULL && observer.receipt->holds == 2 &&
               mc_world_access_receipt_retire(&observer.receipt, NULL) == MC_WORLD_STORAGE_BUSY &&
               mc_render_global_access_retire(&observer.bridge, observer.n_error) ==
                   MC_CLIENT_RENDER_GLOBAL_BUSY);
    int32_t metadata = -1;
    VERIFY(mc_world_placement_error_retire(&e) && observer.calls == 1 &&
           mc_world_runtime_metadata_get(f->worlds[0], 8, 64, 8, &metadata, NULL) ==
               MC_WORLD_STORAGE_OK &&
           metadata == 5 && f->states[0]->data->blocks[cell] == 50);
    VERIFY(mc_world_access_receipt_retire(&observer.receipt, NULL) == MC_WORLD_STORAGE_OK);
    mc_world_access_failure_release(&observer.child);
    if (source_exception) {
        VERIFY(mc_render_global_access_retire(&observer.bridge, observer.n_error) ==
                   MC_CLIENT_RENDER_GLOBAL_OK &&
               mc_client_render_global_release(&observer.renderer, observer.n_error) ==
                   MC_CLIENT_RENDER_GLOBAL_OK &&
               mc_client_render_engine_classes_release(observer.graphics) ==
                   MC_CLIENT_RENDER_ENGINE_OK);
        free(observer.n_error);
    }
    bool removed = false;
    VERIFY(mc_world_runtime_remove_world_access(f->worlds[0], observer.access, &removed, NULL) ==
               MC_WORLD_STORAGE_OK &&
           removed && mc_world_access_free(observer.access, NULL) == MC_WORLD_STORAGE_OK &&
           ss_stop(s) == 0 && ss_finish(s) == 0);
    return 0;
}
