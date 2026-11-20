/* Real GAME/Registry/Store/Chunk constructors and native component inputs.
 * The imported program entry is renamed and never run. No World getter,
 * actor, collision producer, Random or returned placement value is replaced. */
#define MC_COLLISION_BOXES_MAIN mc_placement_unused_boxes_main
#include "../collision/test_boxes.c"
#undef MC_COLLISION_BOXES_MAIN

#include "mc/entity/movement/effects.h"
#include "mc/world/runtime/collision/spawning.h"
#include "mc/world/runtime/placement.h"
#include "mc/world/runtime/world_access.h"

static bool placement_leaf_controls(fixture *f) {
    bool ok = false, value = true;
    mc_world_placement_error e = {0};
    const mc_block *dirt = NULL, *glass = NULL, *log = NULL, *torch = NULL, *sapling = NULL;
    CHECK(mc_registry_block_get(f->registry[0], 3, &dirt, NULL) == MC_REGISTRY_OK && dirt != NULL &&
          mc_registry_block_get(f->registry[0], 20, &glass, NULL) == MC_REGISTRY_OK &&
          glass != NULL &&
          mc_registry_block_get(f->registry[0], 17, &log, NULL) == MC_REGISTRY_OK && log != NULL &&
          mc_registry_block_get(f->registry[0], 50, &torch, NULL) == MC_REGISTRY_OK &&
          torch != NULL &&
          mc_registry_block_get(f->registry[0], 6, &sapling, NULL) == MC_REGISTRY_OK &&
          sapling != NULL);
    const mc_block *base[] = {dirt, glass, log};
    for (size_t i = 0; i < COUNT(base); ++i) {
        value = true;
        CHECK(mc_world_block_activate(f->registry[0], base[i], NULL, INT32_MIN, INT32_MAX, 0, NULL,
                                      &value, &e) == MC_WORLD_STORAGE_OK &&
              e.returned && !value && strcmp(e.site.class_name, "uu") == 0 && e.site.pc == 1 &&
              e.acquisition == NULL && e.collision == NULL && mc_world_placement_error_retire(&e));
        CHECK(mc_world_block_placed_face(f->registry[0], base[i], NULL, 0, 0, 0, INT32_MIN, &e) ==
                  MC_WORLD_STORAGE_OK &&
              e.returned && e.site.pc == 0 && mc_world_placement_error_retire(&e));
        CHECK(mc_world_block_placed_by(f->registry[0], base[i], NULL, 0, 0, 0, NULL, &e) ==
                  MC_WORLD_STORAGE_OK &&
              e.returned && e.site.pc == 0 && mc_world_placement_error_retire(&e));
    }
    value = true;
    CHECK(mc_world_block_can_place(f->registry[0], dirt, NULL, 8, 64, 8, 0, &value, &e) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          !e.returned && value && e.source_exception && !e.native_policy && e.site.pc == 5 &&
          strcmp(e.site.descriptor, "(Lfd;III)Z") == 0 && e.wrapper.pc == 6 &&
          e.acquisition == NULL && mc_world_placement_error_retire(&e));
    CHECK(mc_world_block_activate(f->registry[0], NULL, NULL, 0, 0, 0, NULL, &value, &e) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          value && e.source_exception && e.site.pc == 0 && mc_world_placement_error_retire(&e));
    CHECK(mc_world_block_can_place(f->registry[0], sapling, NULL, 8, 64, 8, 1, &value, &e) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          e.source_exception && !e.native_policy && strcmp(e.site.class_name, "uu") == 0 &&
          e.wrapper.pc == 6 && e.site.pc == 5 && e.acquisition == NULL && e.base_entered &&
          !e.base_returned && strcmp(e.base.class_name, "wb") == 0 && e.base.pc == 6 &&
          !e.below_read && !e.substrate_entered && mc_world_placement_error_retire(&e));
    CHECK(mc_world_block_placed_face(f->registry[0], torch, NULL, 8, 64, 8, 1, &e) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          !e.returned && strcmp(e.site.class_name, "vm") == 0 && e.site.pc == 5 &&
          e.source_exception && !e.native_policy && e.acquisition == NULL &&
          mc_world_placement_error_retire(&e));
    ok = true;
cleanup:
    (void)mc_world_placement_error_retire(&e);
    return ok;
}

#include "lever_controls.inc"
#include "torch_controls.inc"

static bool workbench_activation_controls(fixture *f) {
    bool ok = false;
    const mc_block *table = NULL;
    mc_world_placement_error e = {0};
    mc_world_runtime *world = f->worlds[0];
    const bool initial_remote = world->fields.B;
    const size_t bindings = world->persistent_bindings;
    CHECK(mc_registry_block_get(f->registry[0], 58, &table, NULL) == MC_REGISTRY_OK &&
          table != NULL);
    bool value = false;
    CHECK(mc_world_block_activate(f->registry[0], table, NULL, 8, 64, 8, NULL, &value, &e) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          !value && !e.returned && e.source_exception && !e.native_policy &&
          strcmp(e.site.class_name, "fi") == 0 && e.site.pc == 1 && !e.remote_read &&
          mc_world_placement_error_retire(&e));
    world->fields.B = true; /* Declared source input, before invocation. */
    const mc_world_storage_status remote = mc_world_block_activate(
        f->registry[0], table, world, INT32_MIN, INT32_MAX, -1, NULL, &value, &e);
    const bool remote_valid = remote == MC_WORLD_STORAGE_OK && value && e.returned &&
                              e.remote_read && e.remote && e.site.pc == 8 && !e.workbench_entered &&
                              world->persistent_bindings == bindings + 1;
    const bool remote_retired = mc_world_placement_error_retire(&e);
    world->fields.B = false; /* Next declared input, after checked retirement. */
    CHECK(remote_valid && remote_retired && world->persistent_bindings == bindings);
    value = false;
    const mc_world_storage_status local =
        mc_world_block_activate(f->registry[0], table, world, 8, 64, 8, NULL, &value, &e);
    const bool local_valid = local == MC_WORLD_STORAGE_UNSUPPORTED_STATE && !value && !e.returned &&
                             e.remote_read && !e.remote && e.site.pc == 15 && e.source_exception &&
                             !e.native_policy && !e.workbench_entered;
    const bool local_retired = mc_world_placement_error_retire(&e);
    world->fields.B = initial_remote;
    CHECK(local_valid && local_retired && world->persistent_bindings == bindings);
    ok = true;
cleanup:
    (void)mc_world_placement_error_retire(&e);
    world->fields.B = initial_remote;
    return ok;
}

static bool placement_world_controls(fixture *f) {
    bool ok = false, value = false;
    mc_world_placement_error e = {0};
    mc_world_runtime *world = f->worlds[0];
    boxes_air(f, 0);
    for (int32_t face = 0; face < 6; ++face) {
        const size_t bindings = world->persistent_bindings;
        CHECK(mc_world_runtime_can_place(world, 3, 8, 64, 8, false, face, &value, &e) ==
                  MC_WORLD_STORAGE_OK &&
              value && e.returned && e.value && e.site.pc == 137 && e.existing_read &&
              e.existing_id == 0 && e.candidate_read && e.candidate != NULL && e.box_returned &&
              !e.box_ignored && e.collision_checked && e.predicate_entered &&
              e.predicate_returned && e.fresh_id == 0 && e.acquisition != NULL &&
              e.entity_collision != NULL && e.box != NULL && e.box->min_x == 8 &&
              e.box->min_y == 64 && e.box->min_z == 8 && e.box->max_x == 9 && e.box->max_y == 65 &&
              e.box->max_z == 9 &&
              mc_entity_bounds_native_holds(f->entity_classes[0], e.box) == 1 &&
              world->persistent_bindings == bindings + 1 && world->active_calls == 0);
        /* A new attempt cannot overwrite/consume this exact pending receipt. */
        const mc_entity_bounds *box = e.box;
        const mc_world_placement_site first = e.site;
        CHECK(mc_world_runtime_can_place(world, 3, 8, 64, 8, false, 0, &value, &e) ==
                  MC_WORLD_STORAGE_BUSY &&
              e.box == box && e.site.pc == first.pc && e.returned &&
              mc_world_placement_error_retire(&e) && world->persistent_bindings == bindings &&
              mc_entity_bounds_native_holds(f->entity_classes[0], box) == 0);
    }
    CHECK(boxes_cell(f, 0, 8, 64, 8, 1, 0));
    value = true;
    CHECK(mc_world_runtime_can_place(world, 3, 8, 64, 8, false, 1, &value, &e) ==
              MC_WORLD_STORAGE_OK &&
          !value && e.returned && e.site.pc == 139 && e.existing_id == 1 && e.box_returned &&
          e.collision_checked && !e.predicate_entered && mc_world_placement_error_retire(&e));
    /* Candidate null still throws at collision32, even for occupied space and
     * ignore=true: neither occupancy nor ignore may skip the real getter. */
    value = true;
    CHECK(mc_world_runtime_can_place(world, 0, 8, 64, 8, true, 1, &value, &e) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          value && e.source_exception && !e.native_policy && e.site.pc == 32 &&
          e.existing_id == 1 && !e.box_returned && mc_world_placement_error_retire(&e));
    CHECK(mc_world_runtime_can_place(world, -1, 8, 64, 8, true, 1, &value, &e) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          value && e.source_exception && e.site.pc == 22 &&
          strcmp(e.exception_class, "java/lang/ArrayIndexOutOfBoundsException") == 0 &&
          mc_world_placement_error_retire(&e));
    static const uint8_t replacements[] = {8, 9, 10, 11, 51, 78};
    for (size_t i = 0; i < COUNT(replacements); ++i) {
        CHECK(boxes_cell(f, 0, 8, 64, 8, replacements[i], 0));
        value = false;
        CHECK(mc_world_runtime_can_place(world, 3, 8, 64, 8, false, 5, &value, &e) ==
                  MC_WORLD_STORAGE_OK &&
              value && e.existing_id == replacements[i] && e.existing_replaced &&
              e.predicate_returned && e.fresh_id == replacements[i] && e.caller.pc == 130 &&
              mc_world_placement_error_retire(&e));
    }
    /* FD has no Item y127 restriction; its real bounded query returns air
     * outside height, while the actual candidate box still uses supplied Y. */
    const int32_t ys[] = {127, 128, INT32_MIN, INT32_MAX};
    for (size_t i = 0; i < COUNT(ys); ++i) {
        CHECK(mc_world_runtime_can_place(world, 3, 8, ys[i], 8, false, INT32_MIN, &value, &e) ==
                  MC_WORLD_STORAGE_OK &&
              value && e.fresh_id == 0 && e.box != NULL && e.box->min_y == (double)ys[i] &&
              e.box->max_y == (double)ys[i] + 1.0 && mc_world_placement_error_retire(&e));
    }
    /* A nullable source projection uses the real getter and restores the
     * component input before assertion/teardown; no successful provider exists. */
    mc_chunk_provider *provider = world->fields.v;
    world->fields.v = NULL;
    value = true;
    const mc_world_storage_status failed =
        mc_world_runtime_can_place(world, 3, 8, 64, 8, false, 1, &value, &e);
    world->fields.v = provider;
    CHECK(failed != MC_WORLD_STORAGE_OK && value && !e.returned && e.site.pc == 5 &&
          e.acquisition != NULL && !e.candidate_read && e.box == NULL &&
          mc_world_placement_error_retire(&e));
    ok = true;
cleanup:
    (void)mc_world_placement_error_retire(&e);
    return ok;
}

static bool placement_overlap_controls(fixture *f) {
    bool ok = false, published = false, value = true;
    mc_world_placement_error e = {0};
    mc_entity *actor = NULL;
    mc_entity_effect_error *effect = calloc(1, sizeof(*effect));
    boxes_air(f, 0);
    CHECK(effect != NULL && f->entity_count < COUNT(f->entities) &&
          mc_falling_entity_allocate(f->entity_classes[0], &actor, NULL) == MC_ENTITY_OK);
    f->entities[f->entity_count++] = actor;
    CHECK(mc_falling_entity_construct(actor, f->worlds[0], 8.5, 64.5, 8.5, 3, NULL) ==
              MC_ENTITY_OK &&
          actor->base.aF && !actor->base.be &&
          mc_world_runtime_insert_entity(f->worlds[0], actor, &published, NULL) ==
              MC_WORLD_STORAGE_OK &&
          published);
    CHECK(mc_world_runtime_can_place(f->worlds[0], 3, 8, 64, 8, false, 1, &value, &e) ==
              MC_WORLD_STORAGE_OK &&
          !value && e.site.pc == 60 && e.collision_checked && !e.predicate_entered &&
          e.entity_collision->bytecode == 48 && mc_world_placement_error_retire(&e));
    CHECK(mc_world_runtime_can_place(f->worlds[0], 3, 8, 64, 8, true, 1, &value, &e) ==
              MC_WORLD_STORAGE_OK &&
          value && e.box_returned && e.box_ignored && e.box != NULL && !e.collision_checked &&
          e.entity_collision == NULL && mc_world_placement_error_retire(&e));
    CHECK(mc_entity_effect_dead(actor, effect) == MC_ENTITY_OK && actor->base.be);
    CHECK(mc_world_runtime_can_place(f->worlds[0], 3, 8, 64, 8, false, 1, &value, &e) ==
              MC_WORLD_STORAGE_OK &&
          value && e.collision_checked && e.predicate_returned &&
          mc_world_placement_error_retire(&e));
    ok = true;
cleanup:
    (void)mc_world_placement_error_retire(&e);
    free(effect);
    return ok;
}

static mc_registry_status placement_missing_stream(void *context, const char *name,
                                                   mc_java_properties_input *out) {
    resources *r = context;
    if (name == NULL || out == NULL || strcmp(name, "/lang/en_US.lang") != 0) {
        r->bad = true;
        return MC_REGISTRY_INVALID_ARGUMENT;
    }
    ++r->opens;
    /* Actual Class.getResourceAsStream null result, as in the existing World
     * tick initializer controls. The real Translation initializer owns failure. */
    return MC_REGISTRY_NULL_REFERENCE;
}

static bool placement_initializer_controls(const char *root, unsigned int mode) {
    bool ok = false, value = true;
    fixture *f = calloc(1, sizeof(*f));
    mc_world_placement_error e = {0};
    const size_t slot = mode == 0 ? 2u : 6u;
    const uint16_t title[] = {'C', 'o', 'l', 'd'};
    CHECK(f != NULL && mc_world_regions_create(NULL, &f->regions, NULL) == MC_WORLD_STORAGE_OK &&
          mc_materials_create(&f->materials) &&
          mc_java_random_profile_create(4u * 1024u * 1024u, &f->profile) ==
              MC_JAVA_RANDOM_PROFILE_OK &&
          mc_world_info_name_copy((mc_nbt_string){title, COUNT(title)}, SIZE_MAX, &f->name) ==
              MC_WORLD_INFO_OK);
    resources *r = &f->sources[0];
    r->api = (mc_registry_resources){r, placement_missing_stream, NULL, NULL};
    const mc_registry_options options = {MC_REGISTRY_CLIENT, f->profile, f->materials, 0,
                                         mode == 0 ? &r->api : NULL};
    CHECK(mc_registry_classes_allocate(&options, &f->registry[0], NULL) == MC_REGISTRY_OK &&
          mc_world_runtime_classes_create(MC_WORLD_INFO_CLIENT, f->profile, &trig, &f->classes[0],
                                          NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_classes_bind_registry(f->classes[0], f->registry[0], NULL) ==
              MC_REGISTRY_OK &&
          mc_entity_classes_create(MC_ENTITY_CLIENT, f->profile, NULL, &f->entity_classes[0]) ==
              MC_ENTITY_OK &&
          mc_world_runtime_classes_bind_entities(f->classes[0], f->entity_classes[0]) ==
              MC_ENTITY_OK &&
          boxes_open_store(f, slot, root) && boxes_create_world(f, slot) &&
          f->registry[0]->blocks_phase == MC_REGISTRY_COLD);
    for (unsigned int attempt = 0; attempt < 2; ++attempt) {
        CHECK(mc_world_runtime_can_place(f->worlds[slot], 3, 8, 64, 8, false, 1, &value, &e) ==
                  MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
              value && !e.returned && e.site.pc == 10 && e.existing_read && !e.candidate_read &&
              !e.source_exception && e.source_error == (mode == 0) &&
              e.native_policy == (mode != 0) &&
              f->registry[0]->blocks_phase == MC_REGISTRY_FAILED && !r->bad &&
              r->opens == (mode == 0 ? 1u : 0u));
        if (mode == 0)
            CHECK(e.exception_class != NULL &&
                  strcmp(e.exception_class, attempt == 0 ? "java/lang/ExceptionInInitializerError"
                                                         : "java/lang/NoClassDefFoundError") == 0);
        else
            CHECK(e.exception_class == NULL);
        CHECK(e.registry.status == (attempt != 0 ? MC_REGISTRY_FAILED_INITIALIZATION
                                    : mode == 0  ? MC_REGISTRY_NULL_REFERENCE
                                                 : MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY) &&
              mc_world_placement_error_retire(&e));
    }
    ok = true;
cleanup:
    (void)mc_world_placement_error_retire(&e);
    if (!cleanup_fixture(f)) {
        fputs("placement initializer fixture cleanup refused; owners retained\n", stderr);
        return false;
    }
    return ok;
}

int main(int argc, char **argv) {
    int result = 1;
    fixture *f = NULL;
    char root[8192];
    const uint16_t title[] = {'P', 'l', 'a', 'c', 'e'};
    if (argc != 3 || strcmp(argv[1], "world-placement") != 0 ||
        !decode_path(argv[2], root, sizeof(root)) || !mc_math_trig_init(&trig))
        return 1;
    f = calloc(1, sizeof(*f));
    CHECK(f != NULL && mc_world_regions_create(NULL, &f->regions, NULL) == MC_WORLD_STORAGE_OK &&
          mc_materials_create(&f->materials) &&
          mc_java_random_profile_create(4u * 1024u * 1024u, &f->profile) ==
              MC_JAVA_RANDOM_PROFILE_OK &&
          mc_world_info_name_copy((mc_nbt_string){title, COUNT(title)}, SIZE_MAX, &f->name) ==
              MC_WORLD_INFO_OK);
    resources *r = &f->sources[0];
    r->streams[0] = (stream){.bytes = language, .length = sizeof(language) - 1u};
    r->streams[1] = r->streams[2] = (stream){.bytes = language};
    r->api = (mc_registry_resources){r, open_resource, close_resource, ready_resource};
    const mc_registry_options options = {MC_REGISTRY_CLIENT, f->profile, f->materials, 0, &r->api};
    CHECK(mc_registry_classes_allocate(&options, &f->registry[0], NULL) == MC_REGISTRY_OK &&
          mc_world_runtime_classes_create(MC_WORLD_INFO_CLIENT, f->profile, &trig, &f->classes[0],
                                          NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_classes_bind_registry(f->classes[0], f->registry[0], NULL) ==
              MC_REGISTRY_OK &&
          mc_entity_classes_create(MC_ENTITY_CLIENT, f->profile, NULL, &f->entity_classes[0]) ==
              MC_ENTITY_OK &&
          mc_world_runtime_classes_bind_entities(f->classes[0], f->entity_classes[0]) ==
              MC_ENTITY_OK &&
          mc_registry_classes_initialize_blocks(f->registry[0], NULL) == MC_REGISTRY_OK);
    CHECK(boxes_open_store(f, 0, root) && boxes_create_world(f, 0) && placement_leaf_controls(f) &&
          placement_world_controls(f) && torch_placement_controls(f) &&
          lever_placement_controls(f) && workbench_activation_controls(f) &&
          placement_overlap_controls(f) && placement_initializer_controls(root, 0) &&
          placement_initializer_controls(root, 1));
    result = 0;
cleanup:
    if (!cleanup_fixture(f)) {
        fputs("placement fixture cleanup refused; surviving owners retained\n", stderr);
        return 1;
    }
    if (result == 0)
        printf("PASS: %u World placement source-order/ownership checks\n", checks);
    return result;
}
