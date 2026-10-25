#include "../../../src/client/input/movement_input_internal.h"
#include "../../../src/core/java/random_profile_internal.h"
#include "../../../src/entity/player/player_internal.h"
#include "mc/block/registry/runtime.h"
#include "mc/entity/player/input.h"
#include "mc/entity/player/player.h"
#include "mc/resources/files/files.h"
#include "mc/statistics/registry/registry.h"
#include "mc/world/dimension/world_provider.h"
#include "mc/world/runtime/entities.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned int checks;
static mc_math_trig trig;
#define CHECK(c)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(c)) {                                                                                \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c);                                \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

typedef struct fixture {
    mc_resource_files *files;
    mc_materials *materials;
    mc_java_random_profile *profile;
    mc_entity_classes *entities;
    mc_registry_classes *registry;
    mc_world_runtime_classes *world_classes;
    mc_world_regions *regions;
    mc_world_store *store;
    mc_world_provider *dimension;
    mc_world_runtime *world;
    mc_world_info_name *name;
    mc_player_session_classes *sessions;
} fixture;

static bool setup(fixture *f, const char *resources, const char *directory) {
    CHECK(mc_resource_files_create(resources, &f->files) == MC_REGISTRY_OK);
    CHECK(mc_materials_create(&f->materials));
    CHECK(mc_java_random_profile_create(SIZE_MAX, &f->profile) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_entity_classes_create(MC_ENTITY_CLIENT, f->profile, NULL, &f->entities) ==
          MC_ENTITY_OK);
    const mc_registry_options options = {MC_REGISTRY_CLIENT, f->profile, f->materials, 0,
                                         mc_resource_files_source(f->files)};
    CHECK(mc_registry_classes_allocate(&options, &f->registry, NULL) == MC_REGISTRY_OK);
    CHECK(mc_registry_classes_initialize_statistics(f->registry, NULL) == MC_REGISTRY_OK);
    CHECK(mc_world_runtime_classes_create(MC_WORLD_INFO_CLIENT, f->profile, &trig,
                                          &f->world_classes, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_classes_bind_registry(f->world_classes, f->registry, NULL) ==
          MC_REGISTRY_OK);
    CHECK(mc_world_runtime_classes_bind_entities(f->world_classes, f->entities) == MC_ENTITY_OK);
    CHECK(mc_world_regions_create(NULL, &f->regions, NULL) == MC_WORLD_STORAGE_OK);
    uint8_t map[256] = {0};
    mc_registry_block_view blocks;
    CHECK(mc_registry_classes_inspect_blocks(f->registry, &blocks, NULL) == MC_REGISTRY_OK);
    for (size_t i = 1; i < 256; ++i)
        map[i] = blocks.m[i] == NULL ? 0 : (uint8_t)i;
    const mc_world_store_options store = {.session_token = 6381,
                                          .block_map = map,
                                          .block_map_length = 256,
                                          .limits = mc_world_store_default_limits()};
    CHECK(mc_world_store_open(directory, f->regions, &store, &f->store, NULL) ==
          MC_WORLD_STORAGE_OK);
    static const uint16_t name[] = {'P', 'l', 'a', 'y', 'e', 'r'};
    CHECK(mc_world_info_name_copy((mc_nbt_string){name, 6}, SIZE_MAX, &f->name) ==
          MC_WORLD_INFO_OK);
    CHECK(mc_world_provider_create(0, NULL, &f->dimension, NULL) == MC_WORLD_PROVIDER_OK);
    mc_world_runtime_construction_limits limits = mc_world_runtime_default_construction_limits();
    limits.max_provider_chunks = 2;
    CHECK(mc_world_runtime_allocate_world(MC_WORLD_RUNTIME_CLIENT_DIRECT_METADATA, f->world_classes,
                                          &limits, &f->world, NULL) == MC_WORLD_STORAGE_OK);
    const mc_world_runtime_world_arguments args = {
        .store = f->store, .name = f->name, .seed = 5143, .dimension = f->dimension};
    mc_world_storage_error error;
    const mc_world_storage_status status =
        mc_world_runtime_construct_world(f->world, &args, &error);
    if (status != MC_WORLD_STORAGE_OK)
        fprintf(stderr, "World: %d %s\n", (int)status, error.operation);
    CHECK(status == MC_WORLD_STORAGE_OK);
    CHECK(mc_player_session_classes_create(f->entities, f->registry, &f->sessions, NULL) ==
          MC_PLAYER_OK);
    return true;
}

static bool teardown(fixture *f) {
    CHECK(mc_player_session_classes_free(&f->sessions, NULL) == MC_PLAYER_OK);
    CHECK(mc_world_runtime_retire_entity_containers(f->world, NULL) == MC_WORLD_STORAGE_OK);
    mc_world_provider_unbind(f->dimension);
    CHECK(mc_world_runtime_retire_world(f->world, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_store_chunk_count(f->store) == 0);
    CHECK(mc_world_runtime_free(f->world, NULL) == MC_WORLD_STORAGE_OK);
    mc_world_provider_release(f->dimension);
    CHECK(mc_world_store_free_checked(&f->store, NULL) == MC_WORLD_STORAGE_OK);
    mc_world_regions_free(f->regions);
    mc_world_info_name_release(f->name);
    CHECK(mc_world_runtime_classes_free(f->world_classes, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_registry_classes_free(f->registry, NULL) == MC_REGISTRY_OK);
    CHECK(mc_entity_classes_free(f->entities) == MC_ENTITY_OK);
    mc_java_random_profile_free(f->profile);
    mc_materials_destroy(f->materials);
    mc_resource_files_free(f->files);
    return true;
}

static uint32_t bits(float value) {
    uint32_t result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

static unsigned int mask(const mc_movement_input_state *state) {
    unsigned int value = 0;
    for (size_t i = 0; i < state->key_count; ++i)
        if (state->keys[i])
            value |= 1u << i;
    return value;
}

static bool row(mc_entity *player, const char *label, const char *status) {
    mc_local_player_snapshot p;
    mc_movement_input_state input = {0};
    CHECK(mc_local_player_get_snapshot(player, &p) == MC_ENTITY_OK);
    const bool exists = mc_movement_input_read_state(p.input, &input);
    bool sneaking = true;
    const mc_player_status result = mc_local_player_is_sneaking(player, &sneaking, NULL);
    CHECK(result == (exists ? MC_PLAYER_OK : MC_PLAYER_NULL_REFERENCE));
    printf("INPUT\t%s\t%s\t%d\t%d\t%08x\t%08x\t%d\t%d\t%d\t%u\t%d\t%d\n", label, status,
           exists ? 1 : 0, exists ? (int)input.kind : -1, (unsigned int)bits(input.fields.strafe),
           (unsigned int)bits(input.fields.forward), input.fields.source_c ? 1 : 0,
           input.fields.jump ? 1 : 0, input.fields.sneak ? 1 : 0, mask(&input), p.player.u ? 1 : 0,
           exists ? (sneaking ? 1 : 0) : -1);
    return true;
}

static bool make_player(fixture *f, mc_entity **out) {
    static const uint16_t name[] = {'I', 'n', 'p', 'u', 't'};
    mc_player_string *username = NULL;
    mc_player_session *session = NULL;
    CHECK(mc_player_string_create(name, 5, &username) == MC_PLAYER_OK);
    CHECK(mc_player_session_create(f->sessions, username, NULL, &session, NULL) == MC_PLAYER_OK);
    CHECK(mc_local_player_allocate(f->entities, out, NULL) == MC_ENTITY_OK);
    CHECK(mc_local_player_construct(*out, f->world, NULL, session, 0, NULL, NULL) == MC_PLAYER_OK);
    mc_player_session_retire(&session);
    CHECK(session == NULL);
    mc_player_string_release(username);
    return true;
}

static bool code_set(mc_game_settings *settings, size_t named, int32_t code) {
    mc_game_settings_state state;
    mc_game_settings_text name;
    int32_t previous;
    CHECK(mc_game_settings_read_state(settings, &state));
    CHECK(mc_key_binding_read(state.named[named], &name, &previous));
    CHECK(mc_game_settings_binding_set(settings, state.named[named], name, code, NULL) ==
          MC_GAME_SETTINGS_OK);
    return true;
}

static bool input_cases(fixture *f) {
    mc_entity *p = NULL, *q = NULL, *raw = NULL, *item = NULL;
    mc_movement_input *base = NULL, *options = NULL, *null_options = NULL, *later = NULL;
    mc_game_settings *settings = NULL;
    mc_game_settings_state bindings;
    mc_movement_input_state input;
    mc_player_input_error error;
    mc_local_player_snapshot snapshot;
    bool value = true;
    CHECK(mc_world_info_spawn_set(mc_world_runtime_info(f->world), 2, 64, 2) == MC_WORLD_INFO_OK);
    CHECK(make_player(f, &p) && make_player(f, &q));
    const double x = p->base.x, y = p->base.y, z = p->base.z;
    CHECK(mc_local_player_input_key(p, 17, true, &error) == MC_PLAYER_NULL_REFERENCE);
    CHECK(error.site.pc == 6 && strcmp(error.site.method, "a") == 0 &&
          error.input.status == MC_MOVEMENT_INPUT_OK && p->holds == 1);
    CHECK(mc_local_player_input_reset(p, &error) == MC_PLAYER_NULL_REFERENCE && error.site.pc == 4);
    CHECK(mc_local_player_is_sneaking(p, &value, &error) == MC_PLAYER_NULL_REFERENCE &&
          error.site.pc == 4 && value);
    CHECK(row(p, "fresh", "NPE"));
    CHECK(mc_movement_input_create(&base, NULL) == MC_MOVEMENT_INPUT_OK);
    const mc_movement_input_fields custom = {-0.0f, 1.0f, true, true, true};
    CHECK(mc_movement_input_set_fields(base, &custom, NULL) == MC_MOVEMENT_INPUT_OK);
    CHECK(mc_local_player_set_movement_input(p, base, NULL) == MC_PLAYER_OK);
    CHECK(mc_movement_input_read_state(base, &input) && input.native_holds == 2);
    CHECK(mc_local_player_set_movement_input(p, base, NULL) == MC_PLAYER_OK);
    CHECK(mc_movement_input_read_state(base, &input) && input.native_holds == 2);
    CHECK(mc_local_player_input_key(p, INT32_MIN, true, NULL) == MC_PLAYER_OK);
    CHECK(mc_local_player_input_reset(p, NULL) == MC_PLAYER_OK);
    CHECK(row(p, "base", "OK"));
    /* Authored source field control, not original bed-entry gameplay. */
    p->local_player->fields.player.u = true;
    CHECK(row(p, "base-asleep", "OK"));
    p->local_player->fields.player.u = false;
    CHECK(mc_game_settings_create_defaults(NULL, &settings, NULL) == MC_GAME_SETTINGS_OK);
    CHECK(mc_game_settings_read_state(settings, &bindings));
    CHECK(mc_movement_input_create_from_options(settings, &options, NULL) == MC_MOVEMENT_INPUT_OK);
    CHECK(mc_local_player_set_movement_input(p, options, NULL) == MC_PLAYER_OK);
    CHECK(mc_movement_input_read_state(base, &input) && input.native_holds == 1);
    CHECK(row(p, "options-fresh", "OK"));
    const int32_t codes[] = {17, 30, 57, 42};
    for (size_t i = 0; i < sizeof(codes) / sizeof(codes[0]); ++i)
        CHECK(mc_local_player_input_key(p, codes[i], true, NULL) == MC_PLAYER_OK);
    CHECK(mc_movement_input_read_state(options, &input) && mask(&input) == 53 &&
          !input.fields.sneak);
    CHECK(row(p, "options-keys", "OK"));
    /* Direct actual input.a(Player), not an incomplete dc.o tick wrapper. */
    CHECK(mc_movement_input_update(options, p, NULL) == MC_MOVEMENT_INPUT_OK);
    CHECK(mc_movement_input_read_state(options, &input) &&
          bits(input.fields.strafe) == 0x3e99999a && bits(input.fields.forward) == 0x3e99999a &&
          input.fields.jump && input.fields.sneak);
    CHECK(row(p, "options-updated", "OK"));
    CHECK(mc_local_player_set_movement_input(q, options, NULL) == MC_PLAYER_OK);
    CHECK(mc_movement_input_read_state(options, &input) && input.native_holds == 3);
    CHECK(mc_movement_input_retire(&options, NULL) == MC_MOVEMENT_INPUT_OK && options == NULL);
    CHECK(mc_local_player_get_snapshot(q, &snapshot) == MC_ENTITY_OK);
    CHECK(mc_movement_input_read_state(snapshot.input, &input) && input.native_holds == 2 &&
          input.settings == settings);
    CHECK(row(q, "shared", "OK"));
    CHECK(mc_local_player_input_reset(q, NULL) == MC_PLAYER_OK);
    CHECK(mc_movement_input_read_state(snapshot.input, &input) && mask(&input) == 0 &&
          input.fields.sneak && input.fields.jump);
    CHECK(row(p, "shared-reset", "OK"));
    CHECK(mc_movement_input_update(snapshot.input, q, NULL) == MC_MOVEMENT_INPUT_OK);
    CHECK(row(p, "shared-updated", "OK"));
    CHECK(mc_local_player_set_movement_input(p, base, NULL) == MC_PLAYER_OK);
    CHECK(mc_movement_input_read_state(snapshot.input, &input) && input.native_holds == 1);
    CHECK(row(p, "replacement-base", "OK"));
    CHECK(mc_local_player_input_key(q, 17, true, NULL) == MC_PLAYER_OK);
    CHECK(mc_movement_input_update(snapshot.input, q, NULL) == MC_MOVEMENT_INPUT_OK);
    CHECK(row(q, "remaining-options", "OK"));
    CHECK(mc_local_player_set_movement_input(p, NULL, NULL) == MC_PLAYER_OK);
    CHECK(row(p, "cleared", "NPE"));
    CHECK(mc_movement_input_create_from_options(NULL, &null_options, NULL) == MC_MOVEMENT_INPUT_OK);
    CHECK(mc_local_player_set_movement_input(q, null_options, NULL) == MC_PLAYER_OK);
    CHECK(row(q, "null-settings-fresh", "OK"));
    CHECK(mc_local_player_input_key(q, 17, true, &error) == MC_PLAYER_NULL_REFERENCE);
    CHECK(error.site.pc == 6 && error.input.status == MC_MOVEMENT_INPUT_SOURCE_EXCEPTION &&
          error.input.exception == MC_MOVEMENT_INPUT_NULL_SETTINGS && error.input.bytecode == 7 &&
          error.input.named_index == 0);
    CHECK(row(q, "null-settings-key", "NPE"));
    CHECK(mc_local_player_input_reset(q, NULL) == MC_PLAYER_OK);
    CHECK(row(q, "null-settings-reset", "OK"));
    CHECK(mc_movement_input_update(null_options, q, NULL) == MC_MOVEMENT_INPUT_OK);
    CHECK(row(q, "null-settings-update", "OK"));
    CHECK(mc_movement_input_create_from_options(settings, &later, NULL) == MC_MOVEMENT_INPUT_OK);
    CHECK(mc_local_player_set_movement_input(q, later, NULL) == MC_PLAYER_OK);
    CHECK(mc_game_settings_set_named_binding(settings, 0, NULL, NULL) == MC_GAME_SETTINGS_OK);
    CHECK(mc_local_player_input_key(q, 17, true, &error) == MC_PLAYER_NULL_REFERENCE);
    CHECK(error.site.pc == 6 && error.input.exception == MC_MOVEMENT_INPUT_NULL_BINDING &&
          error.input.bytecode == 10 && error.input.named_index == 0);
    CHECK(row(q, "null-binding", "NPE"));
    CHECK(mc_game_settings_set_named_binding(settings, 0, bindings.named[0], NULL) ==
          MC_GAME_SETTINGS_OK);
    CHECK(mc_local_player_input_key(q, 17, true, NULL) == MC_PLAYER_OK);
    CHECK(mc_movement_input_update(later, q, NULL) == MC_MOVEMENT_INPUT_OK);
    CHECK(row(q, "restored", "OK"));
    CHECK(code_set(settings, 0, INT32_MIN));
    CHECK(mc_local_player_input_key(q, 17, false, NULL) == MC_PLAYER_OK);
    CHECK(mc_movement_input_read_state(later, &input) && mask(&input) == 1);
    CHECK(row(q, "rebind-stale", "OK"));
    CHECK(mc_local_player_input_key(q, INT32_MIN, false, NULL) == MC_PLAYER_OK);
    CHECK(mc_movement_input_read_state(later, &input) && mask(&input) == 0);
    CHECK(row(q, "rebind-event", "OK"));
    CHECK(mc_movement_input_update(later, q, NULL) == MC_MOVEMENT_INPUT_OK);
    CHECK(row(q, "rebind-updated", "OK"));
    CHECK(p->base.x == x && p->base.y == y && p->base.z == z &&
          p->local_player->fields.living.aw == 0 && p->local_player->fields.living.ax == 0 &&
          !p->local_player->fields.living.az && p->phase == MC_ENTITY_READY && p->holds == 1);
    CHECK(mc_local_player_set_movement_input(NULL, base, &error) == MC_PLAYER_NULL_REFERENCE &&
          error.site.pc == 205);
    CHECK(mc_local_player_input_key(NULL, 0, false, &error) == MC_PLAYER_NULL_REFERENCE &&
          error.site.pc == 1);
    CHECK(mc_local_player_input_reset(NULL, &error) == MC_PLAYER_NULL_REFERENCE &&
          error.site.pc == 1);
    CHECK(mc_local_player_is_sneaking(NULL, &value, &error) == MC_PLAYER_NULL_REFERENCE &&
          error.site.pc == 1 && value);
    CHECK(mc_local_player_is_sneaking(p, NULL, &error) == MC_PLAYER_INVALID_ARGUMENT &&
          error.site.pc == 0);
    CHECK(mc_item_entity_allocate(f->entities, &item, NULL) == MC_ENTITY_OK);
    CHECK(mc_local_player_set_movement_input(item, base, &error) == MC_PLAYER_INVALID_ARGUMENT);
    CHECK(mc_local_player_input_key(item, 17, true, NULL) == MC_PLAYER_INVALID_ARGUMENT);
    CHECK(mc_local_player_input_reset(item, NULL) == MC_PLAYER_INVALID_ARGUMENT);
    CHECK(mc_local_player_is_sneaking(item, &value, NULL) == MC_PLAYER_INVALID_ARGUMENT);
    mc_entity_release(item);
    CHECK(mc_local_player_set_movement_input(p, base, NULL) == MC_PLAYER_OK);
    /* Native counter-bound controls; do not stand in for JVM allocation timing. */
    later->holds = SIZE_MAX;
    CHECK(mc_local_player_set_movement_input(p, later, &error) == MC_PLAYER_LIMIT &&
          error.site.pc == 0 && error.input.status == MC_MOVEMENT_INPUT_LIMIT &&
          p->local_player->fields.input == base && later->holds == SIZE_MAX && base->holds == 2);
    later->holds = 2; /* Caller plus actual q field. */
    p->holds = SIZE_MAX;
    CHECK(mc_local_player_input_key(p, 17, true, &error) == MC_PLAYER_ENTITY_FAILED &&
          error.entity == MC_ENTITY_LIMIT && error.site.pc == 0 && p->holds == SIZE_MAX);
    p->holds = 1;
    CHECK(mc_local_player_allocate(f->entities, &raw, NULL) == MC_ENTITY_OK);
    CHECK(mc_local_player_set_movement_input(raw, base, NULL) == MC_PLAYER_OK);
    CHECK(mc_local_player_is_sneaking(raw, &value, NULL) == MC_PLAYER_OK && value);
    CHECK(mc_local_player_input_key(raw, 17, true, NULL) == MC_PLAYER_OK &&
          raw->phase == MC_ENTITY_RAW);
    mc_player_error constructor;
    CHECK(mc_local_player_construct(raw, NULL, NULL, NULL, 0, NULL, &constructor) ==
          MC_PLAYER_NULL_REFERENCE);
    CHECK(constructor.site.pc == 68 && raw->phase == MC_ENTITY_FAILED_CONSTRUCTION &&
          raw->local_player->fields.input == base);
    CHECK(mc_local_player_input_reset(raw, NULL) == MC_PLAYER_OK);
    CHECK(mc_local_player_is_sneaking(raw, &value, NULL) == MC_PLAYER_OK && value);
    mc_entity_release(raw);
    CHECK(base->holds == 2);
    CHECK(mc_local_player_set_movement_input(q, base, NULL) == MC_PLAYER_OK);
    CHECK(base->holds == 3 && later->holds == 1);
    CHECK(mc_movement_input_retire(&later, NULL) == MC_MOVEMENT_INPUT_OK && later == NULL);
    CHECK(mc_movement_input_retire(&null_options, NULL) == MC_MOVEMENT_INPUT_OK &&
          null_options == NULL);
    CHECK(mc_movement_input_retire(&base, NULL) == MC_MOVEMENT_INPUT_OK && base == NULL);
    mc_entity_release(p);
    CHECK(mc_local_player_get_snapshot(q, &snapshot) == MC_ENTITY_OK);
    CHECK(mc_movement_input_read_state(snapshot.input, &input) && input.native_holds == 1 &&
          bits(input.fields.strafe) == 0x80000000 && input.fields.sneak);
    CHECK(mc_local_player_input_reset(q, NULL) == MC_PLAYER_OK);
    mc_entity_release(q); /* Last input hold frees its exact owner, no source reset. */
    CHECK(mc_game_settings_retire(&settings, NULL) == MC_GAME_SETTINGS_OK && settings == NULL);
    return true;
}

int main(int argc, char **argv) {
    (void)setvbuf(stdout, NULL, _IONBF, 0);
    if (argc != 3 || !mc_math_trig_init(&trig))
        return 2;
    fixture f = {0};
    if (!setup(&f, argv[1], argv[2]) || !input_cases(&f) || !teardown(&f))
        return 1;
    printf("LocalPlayer input attachment: %u checks\n", checks);
    return 0;
}
