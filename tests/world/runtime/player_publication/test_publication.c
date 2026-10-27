#include "../../../../src/entity/player/player_internal.h"
#include "../../../../src/world/runtime/chunk_provider_internal.h"
#include "../../../../src/world/runtime/construction_internal.h"
#include "../../../../src/world/runtime/entity_publication_internal.h"
#include "../../../../src/world/runtime/runtime_internal.h"
#include "../../../../src/world/storage/objects_internal.h"
#include "mc/block/registry/runtime.h"
#include "mc/entity/player/player.h"
#include "mc/resources/files/files.h"
#include "mc/statistics/registry/registry.h"
#include "mc/world/dimension/world_provider.h"
#include "mc/world/runtime/entities.h"
#include "mc/world/runtime/players.h"
#include "mc/world/runtime/world_access.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;
static mc_math_trig trig;
#define CHECK(expression)                                                                          \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(expression)) {                                                                       \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expression);                       \
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

static bool setup(fixture *f, const char *resources, const char *directory, size_t capacity) {
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
    limits.max_provider_chunks = capacity;
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
    mc_chunk_provider_fields provider;
    CHECK(mc_chunk_provider_bind_fields(f->world->fields.v, &provider, NULL) ==
          MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_retire_entity_containers(f->world, NULL) == MC_WORLD_STORAGE_OK);
    /* Terminal native cache teardown, after every source operation and lease. */
    CHECK(mc_chunk_pending_clear(provider.pending) == MC_CHUNK_ACQUIRE_OK);
    CHECK(mc_chunk_cache_init(provider.cache, provider.cache->entries, provider.cache->capacity,
                              provider.cache->insertions, provider.cache->insertion_capacity));
    mc_world_provider_unbind(f->dimension);
    CHECK(mc_world_runtime_retire_world(f->world, NULL) == MC_WORLD_STORAGE_OK);
    while (mc_world_store_chunk_count(f->store) != 0) {
        mc_chunk_state *chunk;
        CHECK(mc_world_store_chunk_at(f->store, 0, &chunk, NULL) == MC_WORLD_STORAGE_OK);
        CHECK(mc_world_store_retire_chunk(f->store, chunk, NULL) == MC_WORLD_STORAGE_OK);
    }
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
    CHECK(mc_resource_files_free(f->files) == MC_REGISTRY_OK);
    return true;
}

static bool make_player(fixture *f, mc_entity **out) {
    static const uint16_t text[] = {'P', 'u', 'b', 'l', 'i', 's', 'h'};
    mc_player_string *name = NULL, *token = NULL;
    mc_player_session *session = NULL;
    CHECK(mc_player_string_create(text, 7, &name) == MC_PLAYER_OK);
    CHECK(mc_player_string_create(NULL, 0, &token) == MC_PLAYER_OK);
    CHECK(mc_player_session_create(f->sessions, name, token, &session, NULL) == MC_PLAYER_OK);
    CHECK(mc_world_info_spawn_set(mc_world_runtime_info(f->world), 2, 64, 2) == MC_WORLD_INFO_OK);
    CHECK(mc_local_player_allocate(f->entities, out, NULL) == MC_ENTITY_OK);
    CHECK(mc_local_player_construct(*out, f->world, NULL, session, 0, NULL, NULL) == MC_PLAYER_OK);
    mc_player_session_retire(&session);
    mc_player_string_release(name);
    mc_player_string_release(token);
    return true;
}

static bool slice_count(fixture *f, mc_entity *player, size_t *out) {
    mc_chunk_state *state = NULL;
    CHECK(mc_world_store_chunk_at(f->store, 0, &state, NULL) == MC_WORLD_STORAGE_OK);
    mc_world_chunk *chunk = mc_world_store_chunk_object(f->store, state);
    mc_world_chunk_fields fields;
    CHECK(mc_world_chunk_fields_get(chunk, &fields) == MC_WORLD_STORAGE_OK);
    const mc_world_chunk_entity_list *list;
    CHECK(mc_world_chunk_slice_at(fields.m, (size_t)player->base.bH, &list) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_chunk_entity_count(list, out) == MC_WORLD_STORAGE_OK);
    for (size_t i = 0; i < *out; ++i) {
        mc_entity *entry = NULL;
        CHECK(mc_world_chunk_entity_at(list, i, &entry) == MC_WORLD_STORAGE_OK && entry == player);
    }
    return true;
}

static bool projection(fixture *f, mc_entity *player, const char *phase) {
    size_t count;
    CHECK(slice_count(f, player, &count));
    printf("PUB\t%s\td=%zu\tb=%zu\tJ=%s\tslice=%zu\tmember=%s\tx=%d\ty=%d\tz=%d\n", phase,
           f->world->fields.d->count, f->world->fields.b->count,
           f->world->fields.J ? "true" : "false", count, player->base.bF ? "true" : "false",
           player->base.bG, player->base.bH, player->base.bI);
    return true;
}

typedef struct observer_state {
    fixture *fixture;
    unsigned calls;
    bool reenter, fail;
} observer_state;

static mc_world_storage_status observed(void *owner, mc_world_runtime *world, mc_entity *entity,
                                        mc_world_storage_error *error) {
    observer_state *state = owner;
    ++state->calls;
    size_t count;
    if (!slice_count(state->fixture, entity, &count) || count != world->fields.b->count ||
        count != world->fields.d->count || world->fields.J || !entity->base.bF ||
        mc_world_runtime_retire_entity_containers(world, NULL) != MC_WORLD_STORAGE_BUSY)
        return MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    if (state->reenter) {
        state->reenter = false;
        bool inserted = false;
        mc_world_entity_error nested;
        mc_world_storage_status status =
            mc_world_runtime_insert_entity(world, entity, &inserted, &nested);
        if (status != MC_WORLD_STORAGE_OK || !inserted)
            return MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    }
    if (state->fail) {
        *error = (mc_world_storage_error){MC_WORLD_STORAGE_IO_ERROR,
                                          "Authored Player observer failure", 1234};
        return error->status;
    }
    return MC_WORLD_STORAGE_OK;
}

static bool nbt_prefixes(fixture *f, mc_entity *player) {
    mc_world_info *info = mc_world_runtime_info(f->world);
    mc_world_info_player *owner = NULL, *current = NULL;
    mc_world_player_attachment_error error;
    const mc_nbt_tag empty = {.type = MC_NBT_COMPOUND};
    CHECK(mc_world_info_player_copy(&empty, NULL, &owner, NULL) == MC_WORLD_INFO_OK);
    CHECK(mc_world_info_player_set(info, owner) == MC_WORLD_INFO_OK);
    CHECK(mc_world_runtime_attach_player(f->world, NULL, &error) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          error.bytecode == 14 && error.nbt.base_bytecode == 0);
    const size_t before = f->world->fields.d->count;
    CHECK(mc_world_runtime_attach_player(f->world, player, &error) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          error.bytecode == 14 && error.nbt.base_bytecode == 25 && error.nbt.index == 0 &&
          error.nbt.status == MC_ENTITY_NBT_INDEX_OUT_OF_BOUNDS);
    CHECK(mc_world_info_player_get(info, &current) == MC_WORLD_INFO_OK && current == owner &&
          f->world->fields.d->count == before);
    mc_world_info_player_release(owner);

    static const uint16_t pn[] = {'P', 'o', 's'}, mn[] = {'M', 'o', 't', 'i', 'o', 'n'},
                          rn[] = {'R', 'o', 't', 'a', 't', 'i', 'o', 'n'};
    mc_nbt_tag vectors[8] = {0}, lists[3] = {0};
    const double coordinates[] = {8.5, 80.0, 8.5, .1, .2, .3};
    for (size_t i = 0; i < 6; ++i) {
        vectors[i].type = MC_NBT_DOUBLE;
        memcpy(&vectors[i].value.double_bits, &coordinates[i], sizeof(double));
    }
    const float rotation[] = {450.0f, -450.0f};
    for (size_t i = 0; i < 2; ++i) {
        vectors[6 + i].type = MC_NBT_FLOAT;
        memcpy(&vectors[6 + i].value.float_bits, &rotation[i], sizeof(float));
    }
    const mc_nbt_tag *pos[] = {vectors, vectors + 1, vectors + 2},
                     *motion[] = {vectors + 3, vectors + 4, vectors + 5},
                     *rot[] = {vectors + 6, vectors + 7};
    lists[0] =
        (mc_nbt_tag){.type = MC_NBT_LIST,
                     .name = {pn, 3},
                     .value.list = {.element_type = MC_NBT_DOUBLE, .length = 3, .items = pos}};
    lists[1] =
        (mc_nbt_tag){.type = MC_NBT_LIST,
                     .name = {mn, 6},
                     .value.list = {.element_type = MC_NBT_DOUBLE, .length = 3, .items = motion}};
    lists[2] =
        (mc_nbt_tag){.type = MC_NBT_LIST,
                     .name = {rn, 8},
                     .value.list = {.element_type = MC_NBT_FLOAT, .length = 2, .items = rot}};
    const mc_nbt_tag *items[] = {lists, lists + 1, lists + 2};
    const mc_nbt_tag root = {.type = MC_NBT_COMPOUND,
                             .value.compound = {.length = 3, .items = items}};
    owner = NULL;
    CHECK(mc_world_info_player_copy(&root, NULL, &owner, NULL) == MC_WORLD_INFO_OK);
    CHECK(mc_world_info_player_set(info, owner) == MC_WORLD_INFO_OK);
    /* Preserve the original base-only input: ordinary source defaults now
     * complete through the genuine Player bodies and publication. */
    mc_inventory_player_state old_inventory, new_inventory;
    CHECK(mc_inventory_player_read_state(player->local_player->fields.player.c, &old_inventory));
    const uintptr_t old_main = (uintptr_t)old_inventory.main_slots;
    const uintptr_t old_armor = (uintptr_t)old_inventory.armor_slots;
    CHECK(mc_world_runtime_attach_player(f->world, player, &error) == MC_WORLD_STORAGE_OK &&
          error.bytecode == 0 && error.nbt.status == MC_ENTITY_NBT_OK);
    CHECK(mc_inventory_player_read_state(player->local_player->fields.player.c, &new_inventory));
    CHECK(player->base.x == 8.5 && player->base.y == 80.0 && player->base.velocity_z == .3 &&
          player->base.yaw == 90.0f && player->base.aU == 450.0f &&
          player->local_player->fields.living.Y == 10 &&
          player->local_player->fields.player.g == 0 && !player->local_player->fields.player.u &&
          (uintptr_t)new_inventory.main_slots != old_main &&
          (uintptr_t)new_inventory.armor_slots != old_armor);
    CHECK(mc_world_info_player_get(info, &current) == MC_WORLD_INFO_OK && current == NULL &&
          f->world->fields.d->count == before + 1);
    mc_world_info_player_release(owner);
    owner = NULL;

    /* A distinct sleeping input reaches the real, unfinished wake invocation.
     * No source read/clear/publication success is substituted for that body. */
    const uint16_t sleeping_name[] = {'S', 'l', 'e', 'e', 'p', 'i', 'n', 'g'},
                   timer_name[] = {'S', 'l', 'e', 'e', 'p', 'T', 'i', 'm', 'e', 'r'},
                   score_name[] = {'S', 'c', 'o', 'r', 'e'};
    const mc_nbt_tag sleeping = {.type = MC_NBT_BYTE,
                                 .name = {sleeping_name, 8},
                                 .value.byte_value = 1},
                     timer = {.type = MC_NBT_SHORT,
                              .name = {timer_name, 10},
                              .value.short_value = 23},
                     score = {.type = MC_NBT_INT, .name = {score_name, 5}, .value.int_value = 1234};
    const mc_nbt_tag *sleep_items[] = {lists, lists + 1, lists + 2, &sleeping, &timer, &score};
    const mc_nbt_tag sleep_root = {.type = MC_NBT_COMPOUND,
                                   .value.compound = {.items = sleep_items, .length = 6}};
    const size_t sleeping_before = f->world->fields.d->count;
    CHECK(mc_world_info_player_copy(&sleep_root, NULL, &owner, NULL) == MC_WORLD_INFO_OK &&
          mc_world_info_player_set(info, owner) == MC_WORLD_INFO_OK);
    CHECK(mc_world_runtime_attach_player(f->world, player, &error) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          error.bytecode == 14 && error.nbt.base_bytecode == 311 &&
          error.nbt.local_player_bytecode == 2 && error.nbt.player_bytecode == 93 &&
          error.nbt.late_hook == MC_ENTITY_NBT_LATE_PLAYER && error.nbt.living_bytecode == 55 &&
          error.nbt.status == MC_ENTITY_NBT_UNSUPPORTED_TYPE);
    const mc_player_fields *state = &player->local_player->fields.player;
    CHECK(state->u && state->private_a == 23 && state->g == 0 && state->v != NULL &&
          state->v->x == 8 && state->v->y == 80 && state->v->z == 8 &&
          player->local_player->fields.living.Y == 10);
    CHECK(mc_world_info_player_get(info, &current) == MC_WORLD_INFO_OK && current == owner &&
          f->world->fields.d->count == sleeping_before);
    CHECK(mc_world_info_player_set(info, NULL) == MC_WORLD_INFO_OK);
    mc_world_info_player_release(owner);
    return true;
}

static bool normal_case(fixture *f) {
    mc_entity *player = NULL;
    CHECK(make_player(f, &player));
    mc_world_info_player *tag = (mc_world_info_player *)player;
    CHECK(mc_world_info_player_get(mc_world_runtime_info(f->world), &tag) == MC_WORLD_INFO_OK &&
          tag == NULL);
    CHECK(f->world->fields.b->count == 0 && f->world->fields.d->count == 0 &&
          mc_world_store_chunk_count(f->store) == 0);
    mc_world_player_sleep_error sleep;
    CHECK(mc_world_runtime_update_player_sleeping(f->world, &sleep) == MC_WORLD_STORAGE_OK &&
          !f->world->fields.J);
    mc_world_player_attachment_error attachment;
    CHECK(mc_world_runtime_attach_player(f->world, player, &attachment) == MC_WORLD_STORAGE_OK);
    CHECK(player->holds == 4 && mc_world_store_chunk_count(f->store) == 1);
    CHECK(projection(f, player, "attached"));
    bool inserted = false;
    mc_world_entity_error error;
    CHECK(mc_world_runtime_insert_entity(f->world, player, &inserted, &error) ==
              MC_WORLD_STORAGE_OK &&
          inserted);
    CHECK(player->holds == 7 && projection(f, player, "duplicate"));
    CHECK(mc_world_runtime_add_world_access(f->world, NULL, NULL) == MC_WORLD_STORAGE_OK);
    inserted = false;
    CHECK(mc_world_runtime_insert_entity(f->world, player, &inserted, &error) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          !inserted && error.bytecode == 103 && error.world_access_bytecode == 29 &&
          player->holds == 10);
    CHECK(projection(f, player, "null-access"));
    bool removed = false;
    CHECK(mc_world_runtime_remove_world_access(f->world, NULL, &removed, NULL) ==
              MC_WORLD_STORAGE_OK &&
          removed);
    observer_state state = {.fixture = f, .reenter = true};
    const mc_world_access_callbacks callbacks = {.entity_added = observed};
    mc_world_access *observer = NULL;
    CHECK(mc_world_access_create(&callbacks, &state, &observer, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_add_world_access(f->world, observer, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_insert_entity(f->world, player, &inserted, &error) ==
              MC_WORLD_STORAGE_OK &&
          inserted && state.calls == 2 && player->holds == 16 && f->world->fields.d->count == 5);
    state.fail = true;
    inserted = false;
    CHECK(mc_world_runtime_insert_entity(f->world, player, &inserted, &error) ==
              MC_WORLD_STORAGE_IO_ERROR &&
          !inserted && error.bytecode == 103 && error.world_access_bytecode == 29 &&
          error.cause.system_error == 1234 && player->holds == 19 &&
          f->world->fields.b->count == 6);
    CHECK(mc_world_runtime_remove_world_access(f->world, observer, &removed, NULL) ==
              MC_WORLD_STORAGE_OK &&
          removed);
    CHECK(mc_world_access_free(observer, NULL) == MC_WORLD_STORAGE_OK);

    /* Authored actual-cell controls, not bed-entry/gameplay observations. */
    player->local_player->fields.player.u = true;
    CHECK(mc_world_runtime_update_player_sleeping(f->world, &sleep) == MC_WORLD_STORAGE_OK &&
          f->world->fields.J);
    player->local_player->fields.player.u = false;
    CHECK(mc_world_runtime_update_player_sleeping(f->world, &sleep) == MC_WORLD_STORAGE_OK &&
          !f->world->fields.J);
    mc_world_reference_list *saved = f->world->fields.d;
    f->world->fields.d = NULL;
    f->world->fields.J = true;
    CHECK(mc_world_runtime_update_player_sleeping(f->world, &sleep) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          sleep.bytecode == 5 && f->world->fields.J);
    f->world->fields.d = saved;
    CHECK(mc_world_runtime_update_player_sleeping(f->world, &sleep) == MC_WORLD_STORAGE_OK &&
          !f->world->fields.J);
    CHECK(nbt_prefixes(f, player));
    CHECK(mc_world_runtime_attach_player(f->world, NULL, &attachment) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          attachment.bytecode == 79 && attachment.publication.bytecode == 1);
    CHECK(mc_world_runtime_entity_list_valid(f->world));
    CHECK(mc_world_runtime_retire_entity_containers(f->world, NULL) == MC_WORLD_STORAGE_OK &&
          player->holds == 1 && player->base.world == f->world && player->base.bF &&
          f->world->fields.d->count == 0 && f->world->fields.b->count == 0);
    CHECK(mc_world_runtime_free(f->world, NULL) == MC_WORLD_STORAGE_BUSY);
    mc_entity_release(player);
    return true;
}

static bool acquisition_failure(fixture *f) {
    mc_entity *player = NULL;
    CHECK(make_player(f, &player));
    bool inserted = true;
    mc_world_entity_error error;
    CHECK(mc_world_runtime_insert_entity(f->world, player, &inserted, &error) ==
              MC_WORLD_STORAGE_LIMIT &&
          inserted && error.bytecode == 83 && error.presence_bytecode == 0 &&
          f->world->fields.d->count == 1 && f->world->fields.b->count == 0 && !f->world->fields.J &&
          player->holds == 2 && mc_world_store_chunk_count(f->store) == 0);
    CHECK(f->world->active_calls == 0 && f->world->local_leases == 0);
    CHECK(mc_world_runtime_retire_entity_containers(f->world, NULL) == MC_WORLD_STORAGE_OK &&
          player->holds == 1);
    mc_entity_release(player);
    return true;
}

int main(int argc, char **argv) {
    if (argc != 4 || !mc_math_trig_init(&trig))
        return 2;
    (void)setvbuf(stdout, NULL, _IONBF, 0);
    fixture normal = {0}, failed = {0};
    if (!setup(&normal, argv[1], argv[2], 2) || !normal_case(&normal) || !teardown(&normal) ||
        !setup(&failed, argv[1], argv[3], 0) || !acquisition_failure(&failed) || !teardown(&failed))
        return 1;
    printf("World Player publication: %u checks\n", checks);
    return 0;
}
