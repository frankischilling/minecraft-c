/* Reuse genuine GAME World/Store/Chunk construction and authored NBT transport.
 * The old main and its unrelated controls compile but do not run here. */
#define main mc_render_lists_publication_fixture_main
#include "../test_entity_publication.c"
#undef main

#include "mc/world/runtime/entity_tick.h"
#include "mc/world/runtime/render_entities.h"

/* Native contracts only. Source alias control uses two actual nonempty Lists;
 * both real ownership ledgers stay untouched and source b is restored before
 * assertions. No native count/phase/hold/readiness is assigned. */
int main(int argc, char **argv) {
    int result = 1;
    fixture *f = NULL;
    mc_world_runtime *raw = NULL, *world = NULL;
    mc_world_runtime *world_before = NULL;
    void **b_before = NULL, **e_before = NULL;
    mc_entity **b_holds_before = NULL, **e_holds_before = NULL;
    mc_entity *entity = NULL;
    mc_entity entity_before;
    const mc_world_reference_list *ordinary = NULL, *weather_list = NULL;
    char root[8192];
    const uint16_t title[] = {'R', 'e', 'n', 'd', 'e', 'r'};
    size_t leases_owned = 0;
    if (argc != 3 || strcmp(argv[1], "world-render-entities") != 0 ||
        !decode_path(argv[2], root, sizeof(root)) || !mc_math_trig_init(&trig))
        return 1;
    f = calloc(1, sizeof(*f));
    CHECK(f != NULL && mc_world_regions_create(NULL, &f->regions, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_materials_create(&f->materials));
    CHECK(mc_java_random_profile_create(4u * 1024u * 1024u, &f->profile) ==
          MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_world_info_name_copy((mc_nbt_string){title, COUNT(title)}, SIZE_MAX, &f->name) ==
          MC_WORLD_INFO_OK);
    CHECK(mc_item_stack_allocate(&f->stack) == MC_ITEM_STACK_OK &&
          mc_item_stack_construct(f->stack, 262, 5, 7) == MC_ITEM_STACK_OK);
    resources *resources0 = &f->sources[0];
    resources0->streams[0] = (stream){.bytes = language, .length = sizeof(language) - 1u};
    resources0->streams[1] = (stream){.bytes = language};
    resources0->streams[2] = (stream){.bytes = language};
    resources0->api =
        (mc_registry_resources){resources0, open_resource, close_resource, ready_resource};
    const mc_registry_options options = {MC_REGISTRY_CLIENT, f->profile, f->materials, 0,
                                         &resources0->api};
    CHECK(mc_registry_classes_allocate(&options, &f->registry[0], NULL) == MC_REGISTRY_OK);
    CHECK(mc_world_runtime_classes_create(MC_WORLD_INFO_CLIENT, f->profile, &trig, &f->classes[0],
                                          NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_classes_bind_registry(f->classes[0], f->registry[0], NULL) ==
          MC_REGISTRY_OK);
    CHECK(mc_entity_classes_create(MC_ENTITY_CLIENT, f->profile, NULL, &f->entity_classes[0]) ==
          MC_ENTITY_OK);
    CHECK(mc_world_runtime_classes_bind_entities(f->classes[0], f->entity_classes[0]) ==
          MC_ENTITY_OK);
    CHECK(mc_registry_classes_initialize_blocks(f->registry[0], NULL) == MC_REGISTRY_OK &&
          resources0->opens == 3 && resources0->closes == 1 && !resources0->bad);
    CHECK(open_store(f, 0, root) && create_world(f, 0));
    world = f->worlds[0];
    CHECK(mc_world_runtime_render_entities_list(world, &ordinary, NULL) == MC_WORLD_STORAGE_OK &&
          ordinary == world->fields.b);
    CHECK(mc_world_runtime_render_weather_list(world, &weather_list, NULL) == MC_WORLD_STORAGE_OK &&
          weather_list == world->fields.e && weather_list != ordinary);
    int32_t count = -1;
    CHECK(mc_world_runtime_render_entity_list_count(world, ordinary, &count, NULL) ==
              MC_WORLD_STORAGE_OK &&
          count == 0);
    CHECK(mc_world_runtime_render_entity_list_count(world, weather_list, &count, NULL) ==
              MC_WORLD_STORAGE_OK &&
          count == 0);
    CHECK(make_entity(f, 0, world, 2.0, 16.0, 3.0, &entity));
    bool returned = false;
    CHECK(mc_world_runtime_insert_entity(world, entity, &returned, NULL) == MC_WORLD_STORAGE_OK &&
          returned);
    CHECK(mc_world_runtime_insert_entity(world, entity, &returned, NULL) == MC_WORLD_STORAGE_OK &&
          returned);
    CHECK(mc_world_runtime_add_weather_entity(world, entity, &returned, NULL) ==
              MC_WORLD_STORAGE_OK &&
          returned);
    CHECK(mc_world_runtime_add_weather_entity(world, NULL, &returned, NULL) ==
              MC_WORLD_STORAGE_OK &&
          returned);
    CHECK(mc_world_runtime_add_weather_entity(world, entity, &returned, NULL) ==
              MC_WORLD_STORAGE_OK &&
          returned);

    world_before = malloc(sizeof(*world_before));
    b_before = malloc(ordinary->capacity * sizeof(*b_before));
    e_before = malloc(weather_list->capacity * sizeof(*e_before));
    b_holds_before = malloc(ordinary->capacity * sizeof(*b_holds_before));
    e_holds_before = malloc(weather_list->capacity * sizeof(*e_holds_before));
    CHECK(world_before != NULL && b_before != NULL && e_before != NULL && b_holds_before != NULL &&
          e_holds_before != NULL);
    memcpy(world_before, world, sizeof(*world_before));
    memcpy(&entity_before, entity, sizeof(entity_before));
    const mc_world_reference_list b_descriptor = *ordinary, e_descriptor = *weather_list;
    memcpy(b_before, ordinary->items, ordinary->capacity * sizeof(*b_before));
    memcpy(e_before, weather_list->items, weather_list->capacity * sizeof(*e_before));
    memcpy(b_holds_before, world->entity_membership.entries,
           ordinary->capacity * sizeof(*b_holds_before));
    memcpy(e_holds_before, world->weather_membership.entries,
           weather_list->capacity * sizeof(*e_holds_before));
    CHECK(mc_world_runtime_render_entity_list_count(world, ordinary, &count, NULL) ==
              MC_WORLD_STORAGE_OK &&
          count == 2);
    CHECK(mc_world_runtime_render_entity_list_count(world, weather_list, &count, NULL) ==
              MC_WORLD_STORAGE_OK &&
          count == 3);
    mc_entity *row = NULL;
    for (int32_t i = 0; i < 2; ++i)
        CHECK(mc_world_runtime_render_entity_list_get(world, ordinary, i, &row, NULL) ==
                  MC_WORLD_STORAGE_OK &&
              row == entity);
    for (int32_t i = 0; i < 3; ++i) {
        row = entity;
        CHECK(mc_world_runtime_render_entity_list_get(world, weather_list, i, &row, NULL) ==
                  MC_WORLD_STORAGE_OK &&
              row == (i == 1 ? NULL : entity));
    }
    /* Explicit source-field input, restored before any assertion can exit. */
    world->fields.b = world->fields.e;
    const mc_world_reference_list *fresh = NULL;
    const mc_world_storage_status alias_status =
        mc_world_runtime_render_entities_list(world, &fresh, NULL);
    int32_t old_count = -1, fresh_count = -1;
    const mc_world_storage_status old_status =
        mc_world_runtime_render_entity_list_count(world, ordinary, &old_count, NULL);
    const mc_world_storage_status fresh_status =
        mc_world_runtime_render_entity_list_count(world, fresh, &fresh_count, NULL);
    world->fields.b = (mc_world_reference_list *)ordinary;
    CHECK(alias_status == MC_WORLD_STORAGE_OK && fresh == weather_list &&
          old_status == MC_WORLD_STORAGE_OK && old_count == 2 &&
          fresh_status == MC_WORLD_STORAGE_OK && fresh_count == 3);

    mc_world_storage_error error = {0};
    count = INT32_MIN;
    CHECK(mc_world_runtime_render_entity_list_count(world, world->fields.d, &count, &error) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          count == INT32_MIN && error.status == MC_WORLD_STORAGE_UNSUPPORTED_STATE);
    CHECK(mc_world_runtime_render_entity_list_count(world, NULL, &count, NULL) ==
              MC_WORLD_STORAGE_INVALID_ARGUMENT &&
          count == INT32_MIN);
    row = entity;
    CHECK(mc_world_runtime_render_entity_list_get(world, ordinary, -1, &row, &error) ==
              MC_WORLD_STORAGE_INVALID_ARGUMENT &&
          row == entity);
    CHECK(mc_world_runtime_render_entity_list_get(world, ordinary, 2, &row, NULL) ==
              MC_WORLD_STORAGE_INVALID_ARGUMENT &&
          row == entity);
    fresh = ordinary;
    CHECK(mc_world_runtime_render_entities_list(NULL, &fresh, &error) ==
              MC_WORLD_STORAGE_INVALID_ARGUMENT &&
          fresh == ordinary);
    CHECK(mc_world_runtime_render_weather_list(world, NULL, &error) ==
          MC_WORLD_STORAGE_INVALID_ARGUMENT);
    CHECK(mc_world_runtime_render_entity_list_get(world, ordinary, 0, NULL, &error) ==
          MC_WORLD_STORAGE_INVALID_ARGUMENT);
    returned = false;
    CHECK(mc_world_runtime_render_block_present(world, 2, 0, 3, &returned, NULL) ==
              MC_WORLD_STORAGE_OK &&
          returned);
    CHECK(mc_world_runtime_render_block_present(world, 16, 64, 0, &returned, NULL) ==
              MC_WORLD_STORAGE_OK &&
          !returned);
    CHECK(mc_world_runtime_render_block_present(world, -1, 64, 0, &returned, NULL) ==
              MC_WORLD_STORAGE_OK &&
          !returned);
    CHECK(mc_world_runtime_render_block_present(world, INT32_MIN, -1, INT32_MAX, &returned, NULL) ==
              MC_WORLD_STORAGE_OK &&
          !returned);
    CHECK(mc_world_runtime_render_block_present(world, 0, 128, 0, &returned, NULL) ==
              MC_WORLD_STORAGE_OK &&
          !returned);
    CHECK(memcmp(world, world_before, sizeof(*world_before)) == 0 &&
          memcmp(entity, &entity_before, sizeof(entity_before)) == 0);
    CHECK(memcmp(ordinary, &b_descriptor, sizeof(b_descriptor)) == 0 &&
          memcmp(weather_list, &e_descriptor, sizeof(e_descriptor)) == 0);
    CHECK(memcmp(ordinary->items, b_before, ordinary->capacity * sizeof(*b_before)) == 0 &&
          memcmp(weather_list->items, e_before, weather_list->capacity * sizeof(*e_before)) == 0);
    CHECK(memcmp(world->entity_membership.entries, b_holds_before,
                 ordinary->capacity * sizeof(*b_holds_before)) == 0 &&
          memcmp(world->weather_membership.entries, e_holds_before,
                 weather_list->capacity * sizeof(*e_holds_before)) == 0);

    CHECK(mc_world_runtime_render_entities_lease(world, &error) == MC_WORLD_STORAGE_OK);
    ++leases_owned;
    CHECK(mc_world_runtime_render_entities_lease(world, NULL) == MC_WORLD_STORAGE_OK);
    ++leases_owned;
    CHECK(world->local_leases == world_before->local_leases + 2);
    memcpy(world_before, world, sizeof(*world_before));
    CHECK(mc_world_runtime_retire_entity_containers(world, &error) == MC_WORLD_STORAGE_BUSY &&
          memcmp(world, world_before, sizeof(*world_before)) == 0);
    CHECK(mc_world_runtime_render_entities_release_lease(world, &error) == MC_WORLD_STORAGE_OK);
    --leases_owned;
    memcpy(world_before, world, sizeof(*world_before));
    CHECK(mc_world_runtime_retire_entity_containers(world, NULL) == MC_WORLD_STORAGE_BUSY &&
          memcmp(world, world_before, sizeof(*world_before)) == 0);
    CHECK(mc_world_runtime_render_entities_release_lease(world, NULL) == MC_WORLD_STORAGE_OK);
    --leases_owned;
    CHECK(world->local_leases == 0);
    memcpy(world_before, world, sizeof(*world_before));
    CHECK(mc_world_runtime_render_entities_release_lease(world, &error) ==
              MC_WORLD_STORAGE_INVALID_ARGUMENT &&
          memcmp(world, world_before, sizeof(*world_before)) == 0);
    CHECK(mc_world_runtime_render_entities_lease(NULL, NULL) == MC_WORLD_STORAGE_INVALID_ARGUMENT &&
          mc_world_runtime_render_entities_release_lease(NULL, NULL) ==
              MC_WORLD_STORAGE_INVALID_ARGUMENT);

    CHECK(mc_world_runtime_allocate_world(MC_WORLD_RUNTIME_CLIENT_LOAD_OR_CREATE, f->classes[0],
                                          NULL, &raw, NULL) == MC_WORLD_STORAGE_OK);
    fresh = ordinary;
    CHECK(mc_world_runtime_render_entities_list(raw, &fresh, NULL) == MC_WORLD_STORAGE_OK &&
          fresh == NULL);
    returned = true;
    CHECK(mc_world_runtime_render_block_present(raw, INT32_MIN, -1, INT32_MAX, &returned, NULL) ==
              MC_WORLD_STORAGE_OK &&
          !returned);
    returned = true;
    CHECK(mc_world_runtime_render_block_present(raw, 0, 64, 0, &returned, &error) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          returned);
    CHECK(mc_world_runtime_free(raw, NULL) == MC_WORLD_STORAGE_OK);
    raw = NULL;
    CHECK(mc_world_runtime_retire_entity_containers(world, &error) == MC_WORLD_STORAGE_OK);
    f->terminal[0] = true;
    fresh = ordinary;
    CHECK(mc_world_runtime_render_entities_list(world, &fresh, &error) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          fresh == ordinary);
    count = INT32_MIN;
    CHECK(mc_world_runtime_render_entity_list_count(world, ordinary, &count, NULL) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          count == INT32_MIN);
    result = 0;
cleanup:
    while (leases_owned != 0) {
        if (mc_world_runtime_render_entities_release_lease(world, NULL) != MC_WORLD_STORAGE_OK)
            return 1;
        --leases_owned;
    }
    if (raw != NULL && mc_world_runtime_free(raw, NULL) != MC_WORLD_STORAGE_OK)
        return 1;
    free(world_before);
    free(b_before);
    free(e_before);
    free(b_holds_before);
    free(e_holds_before);
    if (!cleanup_fixture(f)) {
        fputs("render Entity List cleanup refused; surviving owners retained\n", stderr);
        return 1;
    }
    if (result == 0)
        printf("PASS: %u World render Entity List checks (world-render-entities)\n", checks);
    return result;
}
