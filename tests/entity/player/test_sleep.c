/* Genuine World/Registry/Player constructors; imported fixture mains are not run. */
#define MC_PLAYER_SPAWN_MAIN mc_sleep_spawn_fixture_main
#include "test_spawn.c"
#undef MC_PLAYER_SPAWN_MAIN
#include "../../../src/entity/player/sleep_internal.h"
#include "mc/entity/player/sleep.h"

static bool sleep_name(const mc_player_sleep_result *value, const char *expected, int ordinal) {
    mc_player_sleep_result_view view;
    if (!mc_player_sleep_result_inspect(value, &view) || view.phase != MC_ENTITY_CLASS_READY ||
        view.ordinal != ordinal || view.name == NULL || view.name_length != strlen(expected))
        return false;
    for (size_t i = 0; i < view.name_length; ++i)
        if (view.name_units[i] != (uint8_t)expected[i])
            return false;
    return true;
}

static bool sleep_class_cases(mc_java_random_profile *profile) {
    bool ok = false;
    mc_entity_classes *first = NULL, *second = NULL, *server = NULL;
    const mc_player_sleep_result *value = NULL, *other = NULL;
    mc_player_sleep_result_class_view view;
    mc_player_sleep_result_error error;
    CHECK(mc_entity_classes_create(MC_ENTITY_CLIENT, profile, NULL, &first) == MC_ENTITY_OK &&
          mc_entity_classes_create(MC_ENTITY_CLIENT, profile, NULL, &second) == MC_ENTITY_OK &&
          mc_entity_classes_create(MC_ENTITY_SERVER, profile, NULL, &server) == MC_ENTITY_OK);
    CHECK(mc_player_sleep_result_classes_inspect(first, &view) &&
          view.phase == MC_ENTITY_CLASS_COLD && view.a == NULL && view.values_identity == NULL);
    CHECK(mc_player_sleep_result_static_get(first, "f", &value, &error) ==
              MC_PLAYER_SLEEP_RESULT_ARGUMENT &&
          value == NULL && first->sleep_result_phase == MC_ENTITY_CLASS_COLD);
    CHECK(mc_player_sleep_result_static_get(server, "a", &value, &error) ==
              MC_PLAYER_SLEEP_RESULT_ARGUMENT &&
          server->sleep_result_phase == MC_ENTITY_CLASS_COLD);
    CHECK(mc_player_sleep_result_static_get(first, "c", &value, &error) ==
              MC_PLAYER_SLEEP_RESULT_OK &&
          mc_player_sleep_result_classes_inspect(first, &view) &&
          view.phase == MC_ENTITY_CLASS_READY && value == view.c && view.last_pc == 102 &&
          view.last_store_pc == 99 && view.values_length == 5 && view.values_identity != NULL);
    static const char *const names[] = {"OK", "NOT_POSSIBLE_HERE", "NOT_POSSIBLE_NOW",
                                        "TOO_FAR_AWAY", "OTHER_PROBLEM"};
    const mc_player_sleep_result *const fields[] = {view.a, view.b, view.c, view.d, view.e};
    for (size_t i = 0; i < 5; ++i) {
        CHECK(fields[i] == view.candidates[i] && fields[i] == view.values[i] &&
              sleep_name(fields[i], names[i], (int)i));
        for (size_t j = 0; j < i; ++j) {
            mc_player_sleep_result_view a, b;
            CHECK(fields[i] != fields[j] && mc_player_sleep_result_inspect(fields[i], &a) &&
                  mc_player_sleep_result_inspect(fields[j], &b) && a.name != b.name);
        }
    }
    CHECK(mc_player_sleep_result_static_get(second, "c", &other, &error) ==
              MC_PLAYER_SLEEP_RESULT_OK &&
          other != value && sleep_name(other, names[2], 2));
    /* Actual mutable getstatic cells, with the original values-array identities retained. */
    first->sleep_results->c = NULL;
    CHECK(mc_player_sleep_result_static_get(first, "c", &value, &error) ==
              MC_PLAYER_SLEEP_RESULT_OK &&
          value == NULL && first->sleep_results->values->elements[2] == fields[2]);
    first->sleep_results->c = first->sleep_results->e;
    CHECK(mc_player_sleep_result_static_get(first, "c", &value, &error) ==
              MC_PLAYER_SLEEP_RESULT_OK &&
          value == fields[4]);
    first->sleep_results->c = first->sleep_results->candidates[2];
    ok = true;
cleanup:
    if (mc_entity_classes_free(first) != MC_ENTITY_OK ||
        mc_entity_classes_free(second) != MC_ENTITY_OK ||
        mc_entity_classes_free(server) != MC_ENTITY_OK)
        return false;
    return ok;
}

static bool sleep_class_limits(mc_java_random_profile *profile) {
    bool ok = false;
    mc_entity_classes *classes = NULL;
    for (size_t failure = 0; failure < 7; ++failure) {
        CHECK(mc_entity_classes_create(MC_ENTITY_CLIENT, profile, NULL, &classes) == MC_ENTITY_OK);
        const size_t baseline = classes->allocated_bytes;
        const size_t completed = failure == 0 ? 0 : failure - 1;
        classes->limits.max_allocation_bytes =
            baseline + (failure == 0 ? 0
                                     : sizeof(mc_player_sleep_result_class) +
                                           completed * sizeof(mc_player_sleep_result));
        const mc_player_sleep_result *sentinel = (const void *)classes;
        mc_player_sleep_result_error error, cached;
        CHECK(mc_player_sleep_result_static_get(classes, "e", &sentinel, &error) ==
                  MC_PLAYER_SLEEP_RESULT_LIMIT &&
              sentinel == (const void *)classes && error.native_policy &&
              error.native.status == MC_ENTITY_LIMIT && strcmp(error.site.class_name, "cw") == 0 &&
              strcmp(error.site.method, "<clinit>") == 0 &&
              error.site.pc == (failure == 0   ? 0u
                                : failure == 6 ? 66u
                                               : (unsigned int)completed * 13u));
        mc_player_sleep_result_class_view view;
        CHECK(mc_player_sleep_result_classes_inspect(classes, &view) &&
              view.phase == MC_ENTITY_CLASS_FAILED && view.values_identity == NULL);
        for (size_t i = 0; i < 5; ++i)
            CHECK((view.candidates[i] != NULL) == (failure != 0 && i < completed));
        const size_t reached = classes->allocated_bytes;
        classes->limits.max_allocation_bytes = SIZE_MAX;
        CHECK(mc_player_sleep_result_static_get(classes, "a", &sentinel, &cached) ==
                  MC_PLAYER_SLEEP_RESULT_CLASS_FAILED &&
              sentinel == (const void *)classes && cached.site.pc == error.site.pc &&
              cached.operation == error.operation && cached.native.status == MC_ENTITY_LIMIT &&
              classes->allocated_bytes == reached &&
              classes->sleep_result_failure.status == MC_PLAYER_SLEEP_RESULT_LIMIT);
        CHECK(mc_entity_classes_free(classes) == MC_ENTITY_OK);
        classes = NULL;
    }
    ok = true;
cleanup:
    if (mc_entity_classes_free(classes) != MC_ENTITY_OK)
        return false;
    return ok;
}

static bool sleep_prefix_cases(fixture *f, mc_entity *player) {
    bool ok = false;
    mc_world_runtime *world = f->worlds[0];
    const mc_world_runtime_fields original_world = world->fields;
    const mc_entity_base_fields original_base = player->base;
    const mc_local_player_snapshot original_player = player->local_player->fields;
    const mc_entity_bounds *const bounds = player->bounds;
    const mc_entity_bounds original_bounds = *bounds;
    const size_t holds = player->holds, world_holds = world->persistent_bindings;
    const unsigned int controls = _mm_getcsr();
    mc_world_provider *nether = NULL;
    CHECK(mc_world_provider_create(-1, NULL, &nether, NULL) == MC_WORLD_PROVIDER_OK &&
          mc_world_provider_bind(nether, NULL, NULL) == MC_WORLD_PROVIDER_OK);
    for (unsigned int profile = 0; profile < 27; ++profile) {
        player->base = original_base;
        player->local_player->fields = original_player;
        world->fields = original_world;
        player->base.world = world;
        player->base.be = false;
        player->local_player->fields.player.u = false;
        player->local_player->fields.living.Y = 20;
        world->fields.B = false;
        world->fields.f = 4;
        player->base.x = 11;
        player->base.y = 10;
        player->base.z = 11;
        const char *field = NULL;
        unsigned int pc = 119;
        mc_player_sleep_status expected = MC_PLAYER_SLEEP_UNIMPLEMENTED;
        if (profile == 0 || profile == 25) {
            player->base.world = NULL;
            player->local_player->fields.player.u = profile == 25;
            expected = MC_PLAYER_SLEEP_SOURCE_NULL;
            pc = 4;
        } else if (profile == 1 || profile == 20) {
            world->fields.B = true;
            world->fields.t = NULL;
            player->local_player->fields.player.u = true;
            player->base.be = true;
        } else if (profile == 3) {
            player->base.x = player->base.y = player->base.z = NAN;
        } else if (profile == 4) {
            player->base.y = NAN;
            player->base.z = INFINITY;
            field = "d";
        } else if (profile == 5) {
            world->fields.t = NULL;
            world->fields.f = 0;
            expected = MC_PLAYER_SLEEP_SOURCE_NULL;
            pc = 35;
        } else if (profile == 6 || profile == 19 || profile == 22) {
            world->fields.f = profile == 22 ? 3 : 0;
            player->base.x = NAN;
            player->base.y = INFINITY;
            field = "c";
        } else if (profile == 7 || profile == 8 || profile == 9 || profile == 21 || profile == 24) {
            world->fields.t = NULL;
            player->local_player->fields.player.u = profile == 7;
            player->base.be = profile == 8;
            player->local_player->fields.living.Y = profile == 21                   ? INT32_MIN
                                                    : profile == 9 || profile == 24 ? 0
                                                                                    : 20;
            field = "e";
        } else if (profile == 10) {
            world->fields.t = nether;
            world->fields.f = 0;
            field = "b";
        } else if (profile == 12) {
            player->base.x = 11.000000000000002;
            field = "d";
        } else if (profile == 14) {
            player->base.y = 10.000000000000002;
            field = "d";
        } else if (profile == 16) {
            player->base.z = 11.000000000000002;
            field = "d";
        } else if (profile == 17) {
            player->base.x = -INFINITY;
            field = "d";
        } else if (profile == 18) {
            expected = MC_PLAYER_SLEEP_CHILD_FAILED;
            pc = 65;
        } else if (profile == 26) {
            expected = MC_PLAYER_SLEEP_SOURCE_NULL;
            pc = 0;
        }
        if (field != NULL) {
            expected = MC_PLAYER_SLEEP_OK;
            pc = field[0] == 'e' ? 27 : field[0] == 'b' ? 44 : field[0] == 'c' ? 58 : 113;
        }
        const mc_entity_base_fields before_base = player->base;
        const mc_local_player_snapshot before_player = player->local_player->fields;
        const mc_world_runtime_fields before_world = world->fields;
        const mc_player_sleep_result *value = (const void *)player;
        mc_player_sleep_error error;
        if (profile == 18 || profile == 19 || profile == 20)
            _mm_setcsr(controls | 0x8000u);
        const mc_player_sleep_status status =
            mc_player_try_sleep(profile == 26 ? NULL : player, 8, 8, 8, &value, &error);
        _mm_setcsr(controls);
        CHECK(status == expected && error.status == status && error.site.pc == pc &&
              strcmp(error.site.class_name, "gs") == 0 && error.returned == (field != NULL));
        CHECK(memcmp(&before_base, &player->base, sizeof(before_base)) == 0 &&
              memcmp(&before_player, &player->local_player->fields, sizeof(before_player)) == 0 &&
              memcmp(&before_world, &world->fields, sizeof(before_world)) == 0 &&
              player->bounds == bounds && memcmp(bounds, &original_bounds, sizeof(*bounds)) == 0 &&
              player->holds == holds && world->persistent_bindings == world_holds &&
              world->active_calls == 0 && world->local_leases == 0);
        if (field != NULL) {
            const mc_player_sleep_result *actual = NULL;
            CHECK(mc_player_sleep_result_static_get(player->classes, field, &actual, NULL) ==
                      MC_PLAYER_SLEEP_RESULT_OK &&
                  value == actual && !error.native_policy);
        } else {
            CHECK(value == (const void *)player &&
                  error.native_policy == (expected != MC_PLAYER_SLEEP_SOURCE_NULL));
        }
        if (profile < 4)
            CHECK(player->classes->sleep_result_phase == MC_ENTITY_CLASS_COLD);
    }
    /* GS returns the actual nullable/replaced rejection cell without inventing a reason. */
    world->fields.B = false;
    world->fields.t = original_world.t;
    world->fields.f = 0;
    player->base.world = world;
    player->base.be = false;
    player->local_player->fields.player.u = false;
    player->local_player->fields.living.Y = 20;
    mc_player_sleep_result *saved = player->classes->sleep_results->c;
    player->classes->sleep_results->c = NULL;
    const mc_player_sleep_result *value = (const void *)player;
    mc_player_sleep_error error;
    const mc_player_sleep_status nullable = mc_player_try_sleep(player, 8, 8, 8, &value, &error);
    player->classes->sleep_results->c = saved;
    CHECK(nullable == MC_PLAYER_SLEEP_OK && value == NULL && error.returned && error.site.pc == 58);
    ok = true;
cleanup:
    _mm_setcsr(controls);
    player->base = original_base;
    player->local_player->fields = original_player;
    world->fields = original_world;
    mc_world_provider_release(nether);
    return ok;
}

/* Independent prepared-owner projection: each input gets a fresh actual RAW
 * World/DC namespace. No READY constructor or late source effect is fabricated. */
static bool sleep_reference_cases(fixture *f, const char *path) {
    bool ok = false;
    FILE *input = fopen(path, "rb");
    mc_entity_classes *entities = NULL;
    mc_world_runtime_classes *world_classes = NULL;
    mc_world_runtime *world = NULL;
    mc_entity *player = NULL;
    mc_world_provider *normal = NULL, *hell = NULL;
    CHECK(input != NULL &&
          mc_world_provider_create(0, NULL, &normal, NULL) == MC_WORLD_PROVIDER_OK &&
          mc_world_provider_create(-1, NULL, &hell, NULL) == MC_WORLD_PROVIDER_OK &&
          mc_world_provider_bind(hell, NULL, NULL) == MC_WORLD_PROVIDER_OK);
    for (unsigned int index = 0; index < 48; ++index) {
        unsigned int profile;
        int world_null, remote, sleeping, dead, provider_null, nether;
        int32_t health, darkness, x, y, z;
        uint64_t x_word, y_word, z_word;
        CHECK(fscanf(input,
                     "%u %d %d %d %d %" SCNd32 " %d %d %" SCNd32 " %" SCNx64 " %" SCNx64 " %" SCNx64
                     " %" SCNd32 " %" SCNd32 " %" SCNd32,
                     &profile, &world_null, &remote, &sleeping, &dead, &health, &provider_null,
                     &nether, &darkness, &x_word, &y_word, &z_word, &x, &y, &z) == 15 &&
              profile == index);
        CHECK(mc_entity_classes_create(MC_ENTITY_CLIENT, f->profile, NULL, &entities) ==
                  MC_ENTITY_OK &&
              mc_world_runtime_classes_create(MC_WORLD_INFO_CLIENT, f->profile, &trig,
                                              &world_classes, NULL) == MC_WORLD_STORAGE_OK &&
              mc_world_runtime_classes_bind_entities(world_classes, entities) == MC_ENTITY_OK &&
              mc_world_runtime_allocate_world(MC_WORLD_RUNTIME_CLIENT_DIRECT_METADATA,
                                              world_classes, NULL, &world,
                                              NULL) == MC_WORLD_STORAGE_OK &&
              mc_local_player_allocate(entities, &player, NULL) == MC_ENTITY_OK);
        player->base.world = world_null ? NULL : world;
        player->base.be = dead != 0;
        player->local_player->fields.player.u = sleeping != 0;
        player->local_player->fields.living.Y = health;
        memcpy(&player->base.x, &x_word, sizeof(x_word));
        memcpy(&player->base.y, &y_word, sizeof(y_word));
        memcpy(&player->base.z, &z_word, sizeof(z_word));
        world->fields.B = remote != 0;
        world->fields.f = darkness;
        world->fields.t = provider_null ? NULL : nether ? hell : normal;
        const mc_entity_base_fields before_base = player->base;
        const mc_local_player_snapshot before_player = player->local_player->fields;
        const mc_world_runtime_fields before_world = world->fields;
        mc_player_sleep_result_class_view before = {0}, after = {0};
        CHECK(mc_player_sleep_result_classes_inspect(entities, &before) &&
              before.phase == MC_ENTITY_CLASS_COLD);
        const mc_player_sleep_result *value = NULL;
        mc_player_sleep_error error;
        const mc_player_sleep_status status = mc_player_try_sleep(player, x, y, z, &value, &error);
        CHECK(status == MC_PLAYER_SLEEP_OK || status == MC_PLAYER_SLEEP_SOURCE_NULL ||
              status == MC_PLAYER_SLEEP_UNIMPLEMENTED);
        const bool preserved =
            memcmp(&before_base, &player->base, sizeof(before_base)) == 0 &&
            memcmp(&before_player, &player->local_player->fields, sizeof(before_player)) == 0 &&
            memcmp(&before_world, &world->fields, sizeof(before_world)) == 0 && player->holds == 1;
        CHECK(preserved && mc_player_sleep_result_classes_inspect(entities, &after));
        const mc_player_sleep_result *fields[] = {after.a, after.b, after.c, after.d, after.e};
        char result_field = '-';
        if (status == MC_PLAYER_SLEEP_OK)
            for (size_t i = 0; i < 5; ++i)
                if (value == fields[i])
                    result_field = (char)('a' + i);
        const char *outcome = status == MC_PLAYER_SLEEP_OK            ? "RETURN"
                              : status == MC_PLAYER_SLEEP_SOURCE_NULL ? "SOURCE_NULL"
                                                                      : "FRONTIER119";
        printf("SLEEP_CASE %u %s %u %c %d %d %zu\n", profile, outcome, error.site.pc, result_field,
               preserved ? 1 : 0, after.phase == MC_ENTITY_CLASS_READY ? 1 : 0,
               after.values_length);
        printf("SLEEP_IDENT %u ", profile);
        for (size_t i = 0; i < 5; ++i)
            for (size_t j = 0; j < 5; ++j)
                putchar(fields[i] == fields[j] ? '1' : '0');
        putchar('\n');
        for (size_t i = 0; i < 5; ++i) {
            mc_player_sleep_result_view result;
            printf("SLEEP_STATIC %u %zu ", profile, i);
            if (fields[i] == NULL)
                printf("- -1 0 0\n");
            else {
                CHECK(mc_player_sleep_result_inspect(fields[i], &result));
                for (size_t unit = 0; unit < result.name_length; ++unit)
                    printf("%04x", (unsigned int)result.name_units[unit]);
                printf(" %" PRId32 " %d %d\n", result.ordinal,
                       fields[i] == after.candidates[i] ? 1 : 0,
                       fields[i] == after.values[i] ? 1 : 0);
            }
        }
        mc_entity_release(player);
        player = NULL;
        world->fields.t = NULL; /* Restore the borrowed prepared operand before native cleanup. */
        CHECK(mc_world_runtime_retire_world(world, NULL) == MC_WORLD_STORAGE_OK &&
              mc_world_runtime_free(world, NULL) == MC_WORLD_STORAGE_OK);
        world = NULL;
        CHECK(mc_world_runtime_classes_free(world_classes, NULL) == MC_WORLD_STORAGE_OK);
        world_classes = NULL;
        CHECK(mc_entity_classes_free(entities) == MC_ENTITY_OK);
        entities = NULL;
    }
    int trailing;
    do {
        trailing = fgetc(input);
    } while (trailing == ' ' || trailing == '\r' || trailing == '\n' || trailing == '\t');
    CHECK(trailing == EOF && !ferror(input));
    ok = true;
cleanup:
    if (input != NULL)
        fclose(input);
    mc_entity_release(player);
    if (world != NULL) {
        world->fields.t = NULL;
        if (mc_world_runtime_retire_world(world, NULL) != MC_WORLD_STORAGE_OK ||
            mc_world_runtime_free(world, NULL) != MC_WORLD_STORAGE_OK)
            return false;
    }
    if (mc_world_runtime_classes_free(world_classes, NULL) != MC_WORLD_STORAGE_OK ||
        mc_entity_classes_free(entities) != MC_ENTITY_OK)
        return false;
    mc_world_provider_release(normal);
    mc_world_provider_release(hell);
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
    const uint16_t title[] = {'S', 'l', 'e', 'e', 'p'},
                   name[] = {'S', 'l', 'e', 'e', 'p', 'e', 'r'};
    const bool reference = argc == 4 && strcmp(argv[1], "sleep-reference") == 0;
    if ((!reference && (argc != 3 || strcmp(argv[1], "sleep") != 0)) ||
        !decode_path(argv[2], root, sizeof(root)) || !mc_math_trig_init(&trig))
        return 1;
    f = calloc(1, sizeof(*f));
    CHECK(f != NULL && mc_world_regions_create(NULL, &f->regions, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_materials_create(&f->materials));
    CHECK(mc_java_random_profile_create(8u * 1024u * 1024u, &f->profile) ==
          MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(sleep_class_cases(f->profile) && sleep_class_limits(f->profile));
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
    CHECK(sleep_prefix_cases(f, player));
    if (reference)
        CHECK(sleep_reference_cases(f, argv[3]));
    result = 0;
cleanup:
    mc_entity_release(player);
    mc_player_session_retire(&session);
    mc_player_string_release(username);
    if (mc_player_session_classes_free(&sessions, NULL) != MC_PLAYER_OK || !cleanup_fixture(f))
        return 1;
    if (result == 0 && printf("PASS: %u Player sleep-prefix/CW checks\n", checks) < 0)
        return 1;
    return result;
}
