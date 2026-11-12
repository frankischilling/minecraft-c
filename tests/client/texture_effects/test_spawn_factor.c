/* The retained predecessor is project C, not Java/decompiled source. The
 * actual new production TU is compiled once here so its private wrapper is
 * exercised directly; a future link map must exclude archive construction.o. */
#define main mc_effect_spawn_fixture_unused_main
#include "../../entity/player/test_construction.c"
#undef main
#include "../../../src/entity/player/construction.c"
#include "../../../src/world/runtime/runtime_internal.h"
#include "fixtures.h"

static mc_player_status spawn_predecessor(mc_entity *entity, mc_world_runtime *world,
                                          mc_player_coordinates **out, mc_player_error *error) {
    mc_player_coordinates *coordinate = mc_entity_construction_allocate(
        entity, sizeof(*coordinate), &error->entity, "World spawn coordinate allocation", 0);
    if (coordinate == NULL)
        return mc_player_fail(error,
                              error->entity.status == MC_ENTITY_LIMIT ? MC_PLAYER_LIMIT
                                                                      : MC_PLAYER_ALLOCATION_FAILED,
                              "World spawn coordinate allocation", "fd", "u", "()Lbr;", 0);
    /* fd.u reloads x for each of the three actual Info getters. No provider,
     * terrain-top guess or cached diagnostic spawn replaces these cells. */
    mc_world_runtime_fields fields;
    mc_world_info_fields info;
    int32_t values[3];
    for (size_t i = 0; i < 3; ++i) {
        mc_player_status status = world_fields(world, &fields, 100, error);
        if (status != MC_PLAYER_OK)
            goto failed;
        error->info = mc_world_info_fields_get(fields.x, &info);
        if (error->info != MC_WORLD_INFO_OK) {
            mc_player_fail(
                error, fields.x == NULL ? MC_PLAYER_NULL_REFERENCE : MC_PLAYER_WORLD_FAILED,
                "World spawn Info getter", "fd", "u", "()Lbr;", 8u + (unsigned int)i * 7u);
            goto failed;
        }
        values[i] = i == 0 ? info.value.spawn_x : i == 1 ? info.value.spawn_y : info.value.spawn_z;
    }
    coordinate->x = values[0];
    coordinate->y = values[1];
    coordinate->z = values[2];
    *out = coordinate;
    return MC_PLAYER_OK;
failed:
    mc_entity_construction_retire(entity, coordinate, sizeof(*coordinate));
    return error->status;
}

static bool same_text(const char *a, const char *b) {
    return a == NULL ? b == NULL : b != NULL && strcmp(a, b) == 0;
}

static bool spawn_pair(fixture *f, mc_world_runtime *world) {
    mc_entity *a = NULL, *b = NULL;
    CHECK(mc_local_player_allocate(f->entities, &a, NULL) == MC_ENTITY_OK);
    CHECK(mc_local_player_allocate(f->entities, &b, NULL) == MC_ENTITY_OK);
    const size_t charge_a = a->direct_bytes, charge_b = b->direct_bytes;
    mc_player_error *ea = calloc(1, sizeof(*ea)), *eb = calloc(1, sizeof(*eb));
    CHECK(ea != NULL && eb != NULL);
    ea->info = MC_WORLD_INFO_CLOCK_FAILURE;
    eb->info = MC_WORLD_INFO_CLOCK_FAILURE;
    ea->world.status = MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    eb->world.status = MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    ea->world.operation = "caller diagnostic";
    eb->world.operation = "caller diagnostic";
    ea->world.system_error = 41;
    eb->world.system_error = 41;
    mc_player_coordinates *old_value = NULL, *new_value = NULL;
    const mc_player_status old_status = spawn_predecessor(a, world, &old_value, ea);
    const mc_player_status new_status = spawn(b, world, &new_value, eb);
    CHECK(old_status == new_status && ea->status == eb->status && ea->info == eb->info);
    CHECK(same_text(ea->operation, eb->operation));
    CHECK(ea->site.pc == eb->site.pc && same_text(ea->site.class_name, eb->site.class_name) &&
          same_text(ea->site.method, eb->site.method) &&
          same_text(ea->site.descriptor, eb->site.descriptor));
    CHECK(ea->world.status == eb->world.status &&
          ea->world.system_error == eb->world.system_error &&
          same_text(ea->world.operation, eb->world.operation));
    CHECK(ea->entity.status == eb->entity.status && ea->entity.bytecode == eb->entity.bytecode &&
          same_text(ea->entity.operation, eb->entity.operation));
    CHECK((old_value == NULL) == (new_value == NULL));
    if (old_status == MC_PLAYER_OK) {
        CHECK(old_value != NULL && new_value != NULL && old_value != new_value &&
              old_value->x == new_value->x && old_value->y == new_value->y &&
              old_value->z == new_value->z);
        CHECK(a->direct_bytes - charge_a == sizeof(*old_value) &&
              b->direct_bytes - charge_b == sizeof(*new_value));
        mc_entity_construction_retire(a, old_value, sizeof(*old_value));
        mc_entity_construction_retire(b, new_value, sizeof(*new_value));
    }
    CHECK(a->direct_bytes == charge_a && b->direct_bytes == charge_b);
    mc_entity_release(a);
    mc_entity_release(b);
    free(ea);
    free(eb);
    return true;
}

static bool paired_cases(fixture *f) {
    CHECK(spawn_pair(f, f->world));
    CHECK(spawn_pair(f, NULL));
    mc_world_info *saved = f->world->fields.x;
    f->world->fields.x = NULL;
    const bool null_info = spawn_pair(f, f->world);
    f->world->fields.x = saved;
    CHECK(null_info);
    /* Deliberately rejected native-mode perturbation on a genuine owner,
     * never fabricated positive World construction or source state. */
    const mc_world_runtime_mode mode = f->world->construction.mode;
    f->world->construction.mode = MC_WORLD_RUNTIME_COMPOSITION;
    const bool wrong_mode = spawn_pair(f, f->world);
    f->world->construction.mode = mode;
    CHECK(wrong_mode);
    return true;
}

int main(int argc, char **argv) {
    if (argc != 2 || !fx_make(argv[1]) || !mc_math_trig_init(&trig))
        return 2;
    char world[4096];
    if (!fx_path(world, sizeof(world), argv[1], "world"))
        return 2;
    fixture f = {0};
    if (!setup(&f, argv[1], world) || !paired_cases(&f) || !teardown(&f))
        return 1;
    printf("Actual spawn extraction controls: %u\n", checks);
    return 0;
}
