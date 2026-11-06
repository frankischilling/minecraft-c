/* Native fault/ownership inputs complement the unchanged-original replay.
 * Restore nullable fields before any assertion can leave their scope. */
static bool torch_placement_controls(fixture *f) {
    bool ok = false, value = false;
    mc_world_placement_error e = {0};
    mc_world_runtime *world = f->worlds[0];
    const mc_block *torch = NULL;
    const size_t bindings = world->persistent_bindings;
    CHECK(mc_registry_block_get(f->registry[0], 50, &torch, NULL) == MC_REGISTRY_OK);
    boxes_air(f, 0);
    /* VM never calls UU's target replaceability predicate. */
    CHECK(boxes_cell(f, 0, 8, 64, 8, 1, 0) && boxes_cell(f, 0, 7, 64, 8, 1, 0));
    CHECK(mc_world_block_can_place_base(f->registry[0], torch, world, 8, 64, 8, &value, &e) ==
              MC_WORLD_STORAGE_OK &&
          value && e.base.pc == 14 && e.support_query_count == 1 && e.query_count == 0 &&
          world->persistent_bindings == bindings + 1 && mc_world_placement_error_retire(&e) &&
          world->persistent_bindings == bindings);
    boxes_air(f, 0);
    value = true;
    CHECK(mc_world_block_can_place_base(f->registry[0], torch, world, 8, 64, 8, &value, &e) ==
              MC_WORLD_STORAGE_OK &&
          !value && e.base.pc == 77 && e.support_query_count == 5 && e.query_count == 1 &&
          e.floor.pc == 33 && e.below_read && e.below_id == 0 &&
          mc_world_placement_error_retire(&e));
    CHECK(boxes_cell(f, 0, 8, 63, 8, 85, 0));
    CHECK(mc_world_block_can_place_base(f->registry[0], torch, world, 8, 64, 8, &value, &e) ==
              MC_WORLD_STORAGE_OK &&
          value && e.base.pc == 75 && e.support_query_count == 5 && e.query_count == 1 &&
          e.below_id == 85 && mc_world_placement_error_retire(&e));

    /* This independently constructed code namespace supplies UU.ba, while
     * every World.h and fresh ID still uses the actual World's namespace. */
    resources *r = &f->sources[1];
    r->streams[0] = (stream){.bytes = language, .length = sizeof(language) - 1u};
    r->streams[1] = r->streams[2] = (stream){.bytes = language};
    r->api = (mc_registry_resources){r, open_resource, close_resource, ready_resource};
    const mc_registry_options options = {MC_REGISTRY_CLIENT, f->profile, f->materials, 0, &r->api};
    CHECK(mc_registry_classes_allocate(&options, &f->registry[1], NULL) == MC_REGISTRY_OK &&
          mc_registry_classes_initialize_blocks(f->registry[1], NULL) == MC_REGISTRY_OK);
    const mc_block *code_torch = NULL, *fence = NULL;
    CHECK(mc_registry_block_get(f->registry[1], 50, &code_torch, NULL) == MC_REGISTRY_OK &&
          mc_registry_block_static_get(f->registry[1], "ba", &fence, NULL) == MC_REGISTRY_OK);
    mc_block *mutable_fence = (mc_block *)fence;
    const int32_t old_id = mutable_fence->fields.bn;
    mutable_fence->fields.bn = 84;
    const mc_world_storage_status changed =
        mc_world_block_can_place_base(f->registry[1], code_torch, world, 8, 64, 8, &value, &e);
    mutable_fence->fields.bn = old_id;
    CHECK(changed == MC_WORLD_STORAGE_OK && !value && e.below_id == 85 &&
          mc_world_placement_error_retire(&e));
    size_t fence_slot = SIZE_MAX;
    for (size_t i = 0; i < COUNT(f->registry[1]->blocks.named); ++i)
        if (f->registry[1]->blocks.named[i] == fence)
            fence_slot = i;
    CHECK(fence_slot != SIZE_MAX);
    f->registry[1]->blocks.named[fence_slot] = NULL;
    value = true;
    const mc_world_storage_status missing =
        mc_world_block_can_place_base(f->registry[1], code_torch, world, 8, 64, 8, &value, &e);
    f->registry[1]->blocks.named[fence_slot] = mutable_fence;
    CHECK(missing == MC_WORLD_STORAGE_UNSUPPORTED_STATE && value && !e.returned &&
          e.source_exception && !e.native_policy && e.site.pc == 22 && e.base.pc == 68 &&
          e.floor.pc == 22 && e.registry.status == MC_REGISTRY_NULL_REFERENCE &&
          e.support_query_count == 5 && e.query_count == 1 && mc_world_placement_error_retire(&e));

    boxes_air(f, 0);
    CHECK(boxes_cell(f, 0, 7, 64, 8, 1, 0));
    mc_block *stone = f->registry[0]->blocks.m[1];
    mc_material *saved_material = stone->fields.bA;
    stone->fields.bA = NULL;
    const mc_world_storage_status material =
        mc_world_block_can_place_base(f->registry[1], code_torch, world, 8, 64, 8, &value, &e);
    stone->fields.bA = saved_material;
    CHECK(material == MC_WORLD_STORAGE_UNSUPPORTED_STATE && !e.returned && e.source_exception &&
          !e.native_policy && e.site.pc == 7 && e.support_query_count == 1 && e.query_count == 0 &&
          e.registry.status == MC_REGISTRY_NULL_REFERENCE && mc_world_placement_error_retire(&e));
    CHECK(world->persistent_bindings == bindings && world->active_calls == 0);
    boxes_air(f, 0);
    ok = true;
cleanup:
    (void)mc_world_placement_error_retire(&e);
    return ok;
}
