#define MC_PLAYER_SPAWN_MAIN mc_position_spawn_fixture_main
#include "test_spawn.c"
#undef MC_PLAYER_SPAWN_MAIN

typedef struct position_vector {
    uint64_t xyz[3];
    uint32_t shape[4];
} position_vector;

static const position_vector position_vectors[] = {
    {{UINT64_C(0x4021000000000000), UINT64_C(0x4020000000000000), UINT64_C(0xc021000000000000)},
     {UINT32_C(0x3f19999a), UINT32_C(0x3fe66666), UINT32_C(0x3fcf5c29), UINT32_C(0x3e000000)}},
    {{UINT64_C(0x0000000000000000), UINT64_C(0x0000000000000000), UINT64_C(0x0000000000000000)},
     {UINT32_C(0x00000000), UINT32_C(0x00000000), UINT32_C(0x00000000), UINT32_C(0x00000000)}},
    {{UINT64_C(0x8000000000000000), UINT64_C(0x8000000000000000), UINT64_C(0x8000000000000000)},
     {UINT32_C(0x80000000), UINT32_C(0x80000000), UINT32_C(0x80000000), UINT32_C(0x80000000)}},
    {{UINT64_C(0x0000000000000000), UINT64_C(0x8000000000000000), UINT64_C(0x0000000000000000)},
     {UINT32_C(0x00000000), UINT32_C(0x80000000), UINT32_C(0x00000000), UINT32_C(0x80000000)}},
    {{UINT64_C(0x0000000000000001), UINT64_C(0x0000000000000001), UINT64_C(0x0000000000000001)},
     {UINT32_C(0x00000001), UINT32_C(0x00000001), UINT32_C(0x00000001), UINT32_C(0x00000001)}},
    {{UINT64_C(0x8000000000000001), UINT64_C(0x8000000000000001), UINT64_C(0x8000000000000001)},
     {UINT32_C(0x80000001), UINT32_C(0x80000001), UINT32_C(0x80000001), UINT32_C(0x80000001)}},
    {{UINT64_C(0x7ff8123456789abc), UINT64_C(0x4020000000000000), UINT64_C(0xc021000000000000)},
     {UINT32_C(0x3f19999a), UINT32_C(0x3fe66666), UINT32_C(0x3fcf5c29), UINT32_C(0x3e000000)}},
    {{UINT64_C(0x4021000000000000), UINT64_C(0x7ff823456789abcd), UINT64_C(0xc021000000000000)},
     {UINT32_C(0x3f19999a), UINT32_C(0x3fe66666), UINT32_C(0x3fcf5c29), UINT32_C(0x3e000000)}},
    {{UINT64_C(0x4021000000000000), UINT64_C(0x4020000000000000), UINT64_C(0xfff83456789abcde)},
     {UINT32_C(0x3f19999a), UINT32_C(0x3fe66666), UINT32_C(0x3fcf5c29), UINT32_C(0x3e000000)}},
    {{UINT64_C(0x4021000000000000), UINT64_C(0x4020000000000000), UINT64_C(0xc021000000000000)},
     {UINT32_C(0x7fc12345), UINT32_C(0x3fe66666), UINT32_C(0x3fcf5c29), UINT32_C(0x3e000000)}},
    {{UINT64_C(0x4021000000000000), UINT64_C(0x4020000000000000), UINT64_C(0xc021000000000000)},
     {UINT32_C(0x3f19999a), UINT32_C(0xffc54321), UINT32_C(0x3fcf5c29), UINT32_C(0x3e000000)}},
    {{UINT64_C(0x4021000000000000), UINT64_C(0x4020000000000000), UINT64_C(0xc021000000000000)},
     {UINT32_C(0x3f19999a), UINT32_C(0x3fe66666), UINT32_C(0x7fc23456), UINT32_C(0x3e000000)}},
    {{UINT64_C(0x4021000000000000), UINT64_C(0x4020000000000000), UINT64_C(0xc021000000000000)},
     {UINT32_C(0x3f19999a), UINT32_C(0x3fe66666), UINT32_C(0x3fcf5c29), UINT32_C(0xffc34567)}},
    {{UINT64_C(0x4021000000000000), UINT64_C(0x7ff823456789abcd), UINT64_C(0xc021000000000000)},
     {UINT32_C(0x3f19999a), UINT32_C(0x3fe66666), UINT32_C(0x7fc23456), UINT32_C(0xffc34567)}},
    {{UINT64_C(0x7ff8123456789abc), UINT64_C(0x4020000000000000), UINT64_C(0xc021000000000000)},
     {UINT32_C(0xffc12345), UINT32_C(0x3fe66666), UINT32_C(0x3fcf5c29), UINT32_C(0x3e000000)}},
    {{UINT64_C(0x4021000000000000), UINT64_C(0x4020000000000000), UINT64_C(0xfff83456789abcde)},
     {UINT32_C(0x7fc12345), UINT32_C(0x3fe66666), UINT32_C(0x3fcf5c29), UINT32_C(0x3e000000)}},
    {{UINT64_C(0x4021000000000000), UINT64_C(0x4020000000000000), UINT64_C(0xc021000000000000)},
     {UINT32_C(0x3f19999a), UINT32_C(0x3fe66666), UINT32_C(0x7fc23456), UINT32_C(0xffc34567)}},
    {{UINT64_C(0x4021000000000000), UINT64_C(0x7ff823456789abcd), UINT64_C(0xc021000000000000)},
     {UINT32_C(0x3f19999a), UINT32_C(0xffc54321), UINT32_C(0x3fcf5c29), UINT32_C(0x3e000000)}},
    {{UINT64_C(0x7ff8123456789abc), UINT64_C(0x7ff823456789abcd), UINT64_C(0xfff83456789abcde)},
     {UINT32_C(0x7fc12345), UINT32_C(0xffc54321), UINT32_C(0x7fc23456), UINT32_C(0xffc34567)}},
    {{UINT64_C(0x7fefffffffffffff), UINT64_C(0x7fefffffffffffff), UINT64_C(0xffefffffffffffff)},
     {UINT32_C(0x7f7fffff), UINT32_C(0x7f7fffff), UINT32_C(0xff7fffff), UINT32_C(0x7f7fffff)}},
    {{UINT64_C(0x7ff0000000000000), UINT64_C(0x7ff0000000000000), UINT64_C(0x7ff0000000000000)},
     {UINT32_C(0x3f19999a), UINT32_C(0x3fe66666), UINT32_C(0x3fcf5c29), UINT32_C(0x3e000000)}},
    {{UINT64_C(0xfff0000000000000), UINT64_C(0xfff0000000000000), UINT64_C(0xfff0000000000000)},
     {UINT32_C(0x3f19999a), UINT32_C(0x3fe66666), UINT32_C(0x3fcf5c29), UINT32_C(0x3e000000)}},
    {{UINT64_C(0x4021000000000000), UINT64_C(0x4020000000000000), UINT64_C(0xc021000000000000)},
     {UINT32_C(0x7f800000), UINT32_C(0x3fe66666), UINT32_C(0x3fcf5c29), UINT32_C(0x3e000000)}},
    {{UINT64_C(0x4021000000000000), UINT64_C(0x4020000000000000), UINT64_C(0xc021000000000000)},
     {UINT32_C(0xff800000), UINT32_C(0x3fe66666), UINT32_C(0x3fcf5c29), UINT32_C(0x3e000000)}},
    {{UINT64_C(0x4021000000000000), UINT64_C(0x4020000000000000), UINT64_C(0xc021000000000000)},
     {UINT32_C(0x3f19999a), UINT32_C(0x7f800000), UINT32_C(0x3fcf5c29), UINT32_C(0x3e000000)}},
    {{UINT64_C(0x4021000000000000), UINT64_C(0x4020000000000000), UINT64_C(0xc021000000000000)},
     {UINT32_C(0x3f19999a), UINT32_C(0xff800000), UINT32_C(0x3fcf5c29), UINT32_C(0x3e000000)}},
    {{UINT64_C(0x4021000000000000), UINT64_C(0x4020000000000000), UINT64_C(0xc021000000000000)},
     {UINT32_C(0x3f19999a), UINT32_C(0x3fe66666), UINT32_C(0x7f800000), UINT32_C(0x3e000000)}},
    {{UINT64_C(0x4021000000000000), UINT64_C(0x4020000000000000), UINT64_C(0xc021000000000000)},
     {UINT32_C(0x3f19999a), UINT32_C(0x3fe66666), UINT32_C(0xff800000), UINT32_C(0x3e000000)}},
    {{UINT64_C(0x4021000000000000), UINT64_C(0x4020000000000000), UINT64_C(0xc021000000000000)},
     {UINT32_C(0x3f19999a), UINT32_C(0x3fe66666), UINT32_C(0x3fcf5c29), UINT32_C(0x7f800000)}},
    {{UINT64_C(0x4021000000000000), UINT64_C(0x4020000000000000), UINT64_C(0xc021000000000000)},
     {UINT32_C(0x3f19999a), UINT32_C(0x3fe66666), UINT32_C(0x3fcf5c29), UINT32_C(0xff800000)}},
    {{UINT64_C(0x4021000000000000), UINT64_C(0x7ff0000000000000), UINT64_C(0xc021000000000000)},
     {UINT32_C(0x3f19999a), UINT32_C(0x3fe66666), UINT32_C(0x7f800000), UINT32_C(0x00000000)}},
    {{UINT64_C(0x4021000000000000), UINT64_C(0xfff0000000000000), UINT64_C(0xc021000000000000)},
     {UINT32_C(0x3f19999a), UINT32_C(0x3fe66666), UINT32_C(0xff800000), UINT32_C(0x7f800000)}},
    {{UINT64_C(0x7ff0000000000000), UINT64_C(0x4020000000000000), UINT64_C(0xc021000000000000)},
     {UINT32_C(0x7f800000), UINT32_C(0x3fe66666), UINT32_C(0x3fcf5c29), UINT32_C(0x3e000000)}},
    {{UINT64_C(0x4021000000000000), UINT64_C(0x7ff023456789abcd), UINT64_C(0xc021000000000000)},
     {UINT32_C(0x3f19999a), UINT32_C(0x3fe66666), UINT32_C(0x7f823456), UINT32_C(0xff834567)}},
    {{UINT64_C(0x7ff0123456789abc), UINT64_C(0x4020000000000000), UINT64_C(0xc021000000000000)},
     {UINT32_C(0xff812345), UINT32_C(0x3fe66666), UINT32_C(0x3fcf5c29), UINT32_C(0x3e000000)}},
    {{UINT64_C(0x4021000000000000), UINT64_C(0x4170000000000000), UINT64_C(0xc021000000000000)},
     {UINT32_C(0x3f19999a), UINT32_C(0xcb800000), UINT32_C(0x3e800000), UINT32_C(0x3e000000)}},
};

static bool position_cases(mc_entity *p) {
    bool ok = false;
    for (size_t scenario = 0; scenario < COUNT(position_vectors); ++scenario) {
        const position_vector *v = &position_vectors[scenario];
        double xyz[3];
        memcpy(xyz, v->xyz, sizeof(xyz));
        memcpy(&p->base.width, &v->shape[0], sizeof(float));
        memcpy(&p->base.height, &v->shape[1], sizeof(float));
        memcpy(&p->base.offset, &v->shape[2], sizeof(float));
        memcpy(&p->base.y_size, &v->shape[3], sizeof(float));
        CHECK(mc_entity_set_position(p, xyz[0], xyz[1], xyz[2]) == MC_ENTITY_OK);
        const mc_entity_bounds *b = p->bounds;
        const double row[] = {p->base.x, p->base.y, p->base.z, b->min_x, b->min_y,
                              b->min_z,  b->max_x,  b->max_y,  b->max_z};
        printf("POSITION %zu", scenario);
        for (size_t i = 0; i < COUNT(row); ++i)
            printf(" %016" PRIx64, movement_double_bits(row[i]));
        printf("\n");
    }
    ok = !ferror(stdout);
cleanup:
    return ok;
}

int main(int argc, char **argv) {
    int result = 1;
    fixture *f = NULL;
    mc_player_session_classes *sessions = NULL;
    mc_player_session *session = NULL;
    mc_player_string *username = NULL;
    mc_entity *player = NULL;
    char root[8192];
    const uint16_t title[] = {'S', 'p', 'a', 'w', 'n'},
                   name[] = {'S', 'p', 'a', 'w', 'n', 'P', 'l', 'a', 'y', 'e', 'r'};
    if (argc != 3 || !decode_path(argv[2], root, sizeof(root)) || !mc_math_trig_init(&trig))
        return 1;
    CHECK(strcmp(argv[1], "position-probe") == 0);
    f = calloc(1, sizeof(*f));
    CHECK(f != NULL && mc_world_regions_create(NULL, &f->regions, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_materials_create(&f->materials));
    CHECK(mc_java_random_profile_create(8u * 1024u * 1024u, &f->profile) ==
          MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_world_info_name_copy((mc_nbt_string){title, COUNT(title)}, SIZE_MAX, &f->name) ==
          MC_WORLD_INFO_OK);
    resources *r = &f->sources[0];
    r->streams[0] = (stream){.bytes = language, .length = sizeof(language) - 1u};
    r->streams[1] = r->streams[2] = (stream){.bytes = language};
    r->api = (mc_registry_resources){r, open_resource, close_resource, ready_resource};
    const mc_registry_options options = {MC_REGISTRY_CLIENT, f->profile, f->materials, 0, &r->api};
    CHECK(mc_registry_classes_allocate(&options, &f->registry[0], NULL) == MC_REGISTRY_OK);
    CHECK(mc_world_runtime_classes_create(MC_WORLD_INFO_CLIENT, f->profile, &trig, &f->classes[0],
                                          NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_classes_bind_registry(f->classes[0], f->registry[0], NULL) ==
          MC_REGISTRY_OK);
    CHECK(mc_entity_classes_create(MC_ENTITY_CLIENT, f->profile, NULL, &f->entity_classes[0]) ==
          MC_ENTITY_OK);
    CHECK(mc_world_runtime_classes_bind_entities(f->classes[0], f->entity_classes[0]) ==
          MC_ENTITY_OK);
    CHECK(mc_registry_classes_initialize_blocks(f->registry[0], NULL) == MC_REGISTRY_OK);
    CHECK(boxes_open_store(f, 0, root) && boxes_create_world(f, 0));
    CHECK(mc_registry_classes_initialize_statistics(f->registry[0], NULL) == MC_REGISTRY_OK);
    CHECK(mc_player_session_classes_create(f->entity_classes[0], f->registry[0], &sessions, NULL) ==
          MC_PLAYER_OK);
    CHECK(mc_player_string_create(name, COUNT(name), &username) == MC_PLAYER_OK);
    CHECK(mc_player_session_create(sessions, username, NULL, &session, NULL) == MC_PLAYER_OK);
    CHECK(mc_local_player_allocate(f->entity_classes[0], &player, NULL) == MC_ENTITY_OK);
    CHECK(mc_local_player_construct(player, f->worlds[0], NULL, session, 0, NULL, NULL) ==
          MC_PLAYER_OK);
    CHECK(position_cases(player));
    result = 0;
cleanup:
    mc_entity_release(player);
    mc_player_session_retire(&session);
    mc_player_string_release(username);
    if (mc_player_session_classes_free(&sessions, NULL) != MC_PLAYER_OK || !cleanup_fixture(f))
        return 1;
    if (result == 0 && printf("PASS: %u position vector checks\n", checks) < 0)
        return 1;
    return result;
}
