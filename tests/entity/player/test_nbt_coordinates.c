/* Exercise the real active Player body using the existing complete App/World
 * fixture. The selected case replaces only the authored test sequence. */
#include "../../../src/client/application_state.h"
#include "mc/client/input/movement_input.h"
#include "mc/entity/entity.h"
#include "mc/entity/nbt/nbt.h"

typedef struct fixture fixture;
static bool nbt_coordinate_cases(fixture *, client_application *, mc_entity *, mc_movement_input *);
#define MC_PLAYER_ACTIVE_TICK_MAIN mc_nbt_coordinate_fixture_main
#define MC_PLAYER_ACTIVE_TICK_CASES nbt_coordinate_cases
#include "test_tick.c"
#undef MC_PLAYER_ACTIVE_TICK_CASES
#undef MC_PLAYER_ACTIVE_TICK_MAIN

static bool nbt_spawn(mc_entity *p, mc_registry_classes *registry, int32_t x, int32_t y,
                      int32_t z) {
    bool ok = false;
    mc_nbt_tree *tree = NULL;
    mc_entity_nbt_error error;
    CHECK(mc_entity_build_nbt(p, NULL, &tree, &error) == MC_ENTITY_NBT_OK);
    const mc_nbt_tag *root = mc_nbt_tree_root(tree);
    CHECK(root != NULL && root->type == MC_NBT_COMPOUND && root->value.compound.length <= 19);
    const uint16_t names[3][6] = {{'S', 'p', 'a', 'w', 'n', 'X'},
                                  {'S', 'p', 'a', 'w', 'n', 'Y'},
                                  {'S', 'p', 'a', 'w', 'n', 'Z'}};
    const int32_t values[] = {x, y, z};
    mc_nbt_tag fields[3];
    const mc_nbt_tag *children[22];
    size_t count = 0;
    for (size_t i = 0; i < root->value.compound.length; ++i) {
        const mc_nbt_tag *tag = root->value.compound.items[i];
        bool spawn = false;
        for (size_t j = 0; j < 3; ++j)
            if (tag->name.length == 6 && memcmp(tag->name.units, names[j], sizeof names[j]) == 0)
                spawn = true;
        if (!spawn)
            children[count++] = tag;
    }
    for (size_t i = 0; i < 3; ++i) {
        fields[i] =
            (mc_nbt_tag){.type = MC_NBT_INT, .name = {names[i], 6}, .value.int_value = values[i]};
        children[count++] = &fields[i];
    }
    const mc_nbt_tag input = {.type = MC_NBT_COMPOUND,
                              .value.compound = {.items = children, .length = count}};
    CHECK(mc_entity_read_nbt_in_context(p, registry, &input, NULL, &error) == MC_ENTITY_NBT_OK);
    ok = true;
cleanup:
    mc_nbt_tree_free(tree);
    return ok;
}

static bool nbt_coordinate_cases(fixture *f, client_application *app, mc_entity *p,
                                 mc_movement_input *input) {
    bool ok = false;
    (void)tick_cases; /* Retain the imported baseline function without executing it. */
    mc_client_stats_writer *limited = NULL;
    mc_client_game_directory *directory = NULL;
    mc_client_stats_writer *original = app->stats_writer;
    mc_player_tick_error *error = calloc(1, sizeof(*error));
    CHECK(error != NULL && reset_tick(f, p, input));
    const size_t baseline = mc_entity_native_bytes(p);
    CHECK(nbt_spawn(p, app->registry, 4, 5, 6));
    mc_player_fields *fields = &p->local_player->fields.player;
    const mc_player_coordinates *first = fields->private_b;
    const size_t one_node = mc_entity_native_bytes(p);
    CHECK(first != NULL && first->x == 4 && first->y == 5 && first->z == 6 && one_node > baseline);
    /* Authored alias input references the genuine object created by gs.a149.
     * No coordinate object/phase/ledger node is fabricated by the test. */
    fields->private_bN = first;
    CHECK(nbt_spawn(p, app->registry, 7, 8, 9));
    const mc_player_coordinates *second = fields->private_b;
    CHECK(second != NULL && second != first && fields->private_bN == first && second->x == 7 &&
          second->y == 8 && second->z == 9 &&
          mc_entity_native_bytes(p) - one_node == one_node - baseline);
    const size_t two_nodes = mc_entity_native_bytes(p);

    /* A real separately constructed xi with a one-entry caller policy lets
     * tutorial contains succeed, then rejects the reached new play-time key.
     * It is temporarily the actual App.I input, not a callback/stat substitute. */
    char path[8192];
    const int written = snprintf(path, sizeof path, "%s/coordinate-limit",
                                 mc_client_game_directory_path(app->game_directory));
    CHECK(written > 0 && (size_t)written < sizeof path);
    CHECK(mc_client_game_directory_create(path, NULL, &directory, NULL, NULL) ==
          MC_CLIENT_GAME_DIRECTORY_OK);
    mc_client_stats_options options = mc_client_stats_default_options();
    options.limits.max_map_entries = 1;
    options.charset = MC_JAVA_READER_UTF8;
    options.diagnostics.emit = active_quiet;
    CHECK(mc_client_stats_writer_allocate(app->registry, &options, &limited, NULL) ==
              MC_CLIENT_STATS_OK &&
          mc_client_stats_writer_construct(limited, app->session,
                                           mc_client_game_directory_path(directory), NULL,
                                           NULL) == MC_CLIENT_STATS_OK &&
          mc_client_stats_writer_join(limited, NULL) == MC_CLIENT_STATS_OK);
    mc_registry_achievement_view achievements;
    CHECK(mc_registry_classes_inspect_achievements(app->registry, &achievements, NULL) ==
              MC_REGISTRY_OK &&
          mc_client_stats_writer_increment(limited, achievements.named[0], 1, NULL) ==
              MC_CLIENT_STATS_OK);
    const mc_player_action_context context = {
        .travel = {.code_registry = app->registry, .trig = &trig}};
    app->stats_writer = limited;
    const mc_entity_status failed = mc_player_tick(p, &context, NULL, error);
    app->stats_writer = original;
    CHECK(failed == MC_ENTITY_LIMIT && error->player_bytecode == 387 &&
          error->statistic.writer.status == MC_CLIENT_STATS_LIMIT && fields->private_bN == first &&
          fields->private_b == second && first->x == 4 && first->y == 5 && first->z == 6 &&
          mc_entity_native_bytes(p) == two_nodes);
    CHECK(mc_client_stats_writer_retire(&limited, NULL) == MC_CLIENT_STATS_OK);

    CHECK(mc_player_tick(p, &context, NULL, error) == MC_ENTITY_OK && fields->private_bN == NULL &&
          fields->private_b == second && second->x == 7 && second->y == 8 && second->z == 9 &&
          mc_entity_native_bytes(p) == one_node);
    /* The same clear399 must retain a node still referenced by b. */
    fields->private_bN = second;
    CHECK(mc_player_tick(p, &context, NULL, error) == MC_ENTITY_OK && fields->private_bN == NULL &&
          fields->private_b == second && second->x == 7 && second->y == 8 && second->z == 9 &&
          mc_entity_native_bytes(p) == one_node);
    ok = true;
cleanup:
    app->stats_writer = original;
    if (limited != NULL && mc_client_stats_writer_retire(&limited, NULL) != MC_CLIENT_STATS_OK)
        ok = false;
    mc_client_game_directory_free(directory);
    free(error);
    return ok;
}

int main(int argc, char **argv) {
    const int result = mc_nbt_coordinate_fixture_main(argc, argv);
    if (result == 0 && printf("Player NBT coordinates: %u checks\n", checks) < 0)
        return 1;
    return result;
}
