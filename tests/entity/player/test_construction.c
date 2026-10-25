#include "../../../src/core/java/random_profile_internal.h"
#include "../../../src/entity/player/player_internal.h"
#include "mc/block/registry/runtime.h"
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

static bool watcher(const mc_entity *entity) {
    CHECK(entity->watcher != NULL && entity->watcher->map != NULL &&
          entity->watcher->map->size == 2);
    unsigned mask = 0;
    for (const mc_entity_watch_node *n = entity->watcher->map->head; n != NULL; n = n->next) {
        CHECK(n->record != NULL && n->record->type == 0 && n->record->byte_value == 0 &&
              n->record->dirty);
        CHECK(n->key == n->record->index && (n->key == 0 || n->key == 16));
        mask |= n->key == 0 ? 1u : 2u;
    }
    CHECK(mask == 3);
    mc_entity_watcher_snapshot view;
    CHECK(mc_entity_watcher_get_snapshot(entity->watcher, &view) == MC_ENTITY_OK &&
          view.size == 2 && view.index == 0 && view.byte_value == 0);
    return true;
}

static bool construct_case(fixture *f, const uint16_t *name, size_t length, bool null_name,
                           bool null_session, bool null_world, int32_t y) {
    mc_player_string *username = NULL, *token = NULL;
    if (!null_name)
        CHECK(mc_player_string_create(name, length, &username) == MC_PLAYER_OK);
    CHECK(mc_player_string_create(NULL, 0, &token) == MC_PLAYER_OK);
    mc_player_session *session = NULL;
    if (!null_session)
        CHECK(mc_player_session_create(f->sessions, username, token, &session, NULL) ==
              MC_PLAYER_OK);
    mc_player_session_class_state cl;
    CHECK(mc_player_session_classes_read(f->sessions, &cl));
    if (!null_session)
        CHECK(cl.phase == MC_PLAYER_SESSION_READY && cl.pc == 346 && cl.creative_count == 28);
    if (!null_session) {
        static const char *const names[] = {
            "u",  "x",  "am", "w", "y", "K",  "L", "ar", "al", "N",  "ap", "z",  "ae", "af",
            "ag", "ah", "F",  "G", "M", "ac", "J", "I",  "H",  "aj", "ai", "ao", "an", "aq"};
        for (size_t i = 0; i < 28; ++i) {
            const mc_block *block = NULL;
            CHECK(mc_registry_block_static_get(f->registry, names[i], &block, NULL) ==
                      MC_REGISTRY_OK &&
                  block == cl.creative_blocks[i]);
        }
        CHECK(mc_player_session_classes_free(&f->sessions, NULL) == MC_PLAYER_BUSY &&
              f->sessions != NULL);
    }
    mc_entity *entity = NULL;
    CHECK(mc_local_player_allocate(f->entities, &entity, NULL) == MC_ENTITY_OK);
    CHECK(entity->phase == MC_ENTITY_RAW && entity->local_player != NULL);
    const mc_entity_class_literal *local, *player, *living, *base;
    CHECK(mc_entity_class_literal_get(f->entities, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER, &local) ==
          MC_ENTITY_OK);
    CHECK(mc_entity_class_literal_get(f->entities, MC_ENTITY_CLASS_KIND_PLAYER, &player) ==
          MC_ENTITY_OK);
    CHECK(mc_entity_class_literal_get(f->entities, MC_ENTITY_CLASS_KIND_LIVING_ENTITY, &living) ==
          MC_ENTITY_OK);
    CHECK(mc_entity_class_literal_get(f->entities, MC_ENTITY_CLASS_KIND_ENTITY, &base) ==
          MC_ENTITY_OK);
    bool assign = false;
    CHECK(local == entity->class_literal);
    CHECK(mc_entity_class_literal_is_assignable_from(player, local, &assign) == MC_ENTITY_OK &&
          assign);
    CHECK(mc_entity_class_literal_is_assignable_from(living, local, &assign) == MC_ENTITY_OK &&
          assign);
    CHECK(mc_entity_class_literal_is_assignable_from(base, local, &assign) == MC_ENTITY_OK &&
          assign);
    CHECK(mc_world_info_spawn_set(mc_world_runtime_info(f->world), -7, y, 11) == MC_WORLD_INFO_OK);
    double discarded;
    CHECK(mc_java_math_random(f->profile, &discarded) == MC_JAVA_RANDOM_PROFILE_OK);
    mc_java_random oracle = {.state = atomic_load(&f->profile->math->state)};
    double draws[4];
    for (size_t i = 0; i < 4; ++i)
        draws[i] = mc_java_random_next_double(&oracle);
    mc_player_error error;
    const mc_player_status status = mc_local_player_construct(entity, null_world ? NULL : f->world,
                                                              NULL, session, -1, NULL, &error);
    CHECK(atomic_load(&f->profile->math->state) == oracle.state);
    mc_local_player_snapshot s;
    CHECK(mc_local_player_get_snapshot(entity, &s) == MC_ENTITY_OK);
    CHECK(watcher(entity));
    CHECK(s.watcher_hook_pc == 17 && s.living_pc == 217);
    CHECK(s.living.E == 20 && s.living.ai == -1 && s.living.N && s.living.P && s.living.S == 1.0f);
    CHECK(s.living.aj == mc_local_player_aj_from_draw(draws[0]));
    CHECK(s.living.G == mc_local_player_g_from_draw(draws[1]));
    CHECK(s.living.F == mc_local_player_f_from_draw(draws[2]));
    CHECK(s.living.J == 0 && s.living.K == 0 && s.living.private_a == 0 &&
          s.living.private_b == NULL);
    CHECK(s.player.c != NULL && s.player.z == 20 && s.player.D == NULL);
    mc_inventory_player_state inv;
    CHECK(mc_inventory_player_read_state(s.player.c, &inv));
    CHECK(inv.player == entity && inv.selected == 0 && inv.cursor == NULL && !inv.changed);
    for (size_t i = 0; i < 36; ++i)
        CHECK(inv.main_slots[i] == NULL);
    for (size_t i = 0; i < 4; ++i)
        CHECK(inv.armor_slots[i] == NULL);
    if (null_world) {
        CHECK(status == MC_PLAYER_NULL_REFERENCE && error.site.pc == 68 &&
              entity->phase == MC_ENTITY_FAILED_CONSTRUCTION);
        CHECK(s.player_pc == 55 && s.player.d == NULL && s.filters[0] == NULL && s.local_pc == 0);
        CHECK(entity->base.yaw == mc_local_player_yaw_from_draw(draws[3]) &&
              entity->base.offset == 0);
    } else {
        CHECK(s.player_pc == 165 && s.player.d == s.player.e && s.player.d != NULL &&
              inv.bound_containers == 1);
        mc_container_state container;
        CHECK(mc_container_read_state(s.player.d, &container));
        CHECK(container.slots == 45 && container.snapshots == 45 && container.local &&
              container.player_inventory == s.player.c);
        CHECK(container.crafting != NULL && container.result != NULL && container.listeners == 0 &&
              container.blocked_players == 0);
        CHECK(entity->base.x == -6.5 && entity->base.z == 11.5);
        CHECK(entity->base.y == (double)mc_java_i32_add(y, 1) + (double)1.62f);
        CHECK(entity->base.aJ == entity->base.x && entity->base.aK == entity->base.y &&
              entity->base.aL == entity->base.z);
        CHECK(entity->base.bl == entity->base.x && entity->base.bm == entity->base.y &&
              entity->base.bn == entity->base.z);
        CHECK(entity->base.yaw == 0 && entity->base.aT == 0 && entity->base.aU == 0 &&
              entity->base.aV == 0);
        CHECK(s.living.Y == 20 && s.living.Q == 180 && entity->base.bu == 20 &&
              entity->base.bp == 0.5f);
        CHECK(s.input == NULL && s.minecraft == NULL && s.player.m == -1);
        CHECK(s.filters[0] != s.filters[1] && s.filters[1] != s.filters[2] &&
              s.filters[0] != s.filters[2]);
        for (size_t i = 0; i < 3; ++i)
            CHECK(s.filters[i] != NULL && s.filters[i]->a == 0 && s.filters[i]->b == 0 &&
                  s.filters[i]->c == 0);
        if (null_session)
            CHECK(status == MC_PLAYER_NULL_REFERENCE && error.site.pc == 103 && s.local_pc == 46 &&
                  s.player.l == NULL);
        else {
            CHECK(status == MC_PLAYER_OK && entity->phase == MC_ENTITY_READY && s.local_pc == 109 &&
                  s.player.l == username);
            CHECK((s.skin_url != NULL) == (!null_name && length != 0));
            if (s.skin_url) {
                mc_player_string_view view;
                CHECK(mc_player_string_read(s.skin_url, &view) && view.length == length + 43);
                CHECK(memcmp(view.units + 39, name, length * sizeof(*name)) == 0 &&
                      entity->base.client_only.bA == s.skin_url);
            }
        }
    }
    mc_player_session_retire(&session);
    mc_player_string_release(username);
    mc_player_string_release(token);
    if (!null_world && !null_session && !null_name) {
        mc_player_string_view kept;
        CHECK(mc_player_string_read(s.player.l, &kept) && kept.length == length);
    }
    CHECK(mc_local_player_construct(entity, NULL, NULL, NULL, 0, NULL, NULL) ==
          MC_PLAYER_INVALID_PHASE);
    mc_entity_release(entity);
    return true;
}

static bool session_failure(const char *empty_resources) {
    mc_resource_files *files = NULL;
    mc_materials *materials = NULL;
    mc_java_random_profile *profile = NULL;
    mc_entity_classes *entities = NULL;
    mc_registry_classes *registry = NULL;
    mc_player_session_classes *sessions = NULL;
    CHECK(mc_resource_files_create(empty_resources, &files) == MC_REGISTRY_OK &&
          mc_materials_create(&materials));
    CHECK(mc_java_random_profile_create(SIZE_MAX, &profile) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_entity_classes_create(MC_ENTITY_CLIENT, profile, NULL, &entities) == MC_ENTITY_OK);
    const mc_registry_options options = {MC_REGISTRY_CLIENT, profile, materials, 0,
                                         mc_resource_files_source(files)};
    CHECK(mc_registry_classes_allocate(&options, &registry, NULL) == MC_REGISTRY_OK);
    CHECK(mc_player_session_classes_create(entities, registry, &sessions, NULL) == MC_PLAYER_OK);
    mc_player_session *session = NULL;
    mc_player_error error;
    CHECK(mc_player_session_create(sessions, NULL, NULL, &session, &error) ==
              MC_PLAYER_REGISTRY_FAILED &&
          session == NULL);
    mc_player_session_class_state state;
    CHECK(mc_player_session_classes_read(sessions, &state) &&
          state.phase == MC_PLAYER_SESSION_FAILED && state.pc == 7 && state.creative_list != NULL &&
          state.creative_count == 0);
    const void *list = state.creative_list;
    CHECK(mc_player_session_create(sessions, NULL, NULL, &session, &error) ==
              MC_PLAYER_REGISTRY_FAILED &&
          session == NULL);
    CHECK(mc_player_session_classes_read(sessions, &state) && state.creative_list == list &&
          state.creative_count == 0);
    CHECK(mc_player_session_classes_free(&sessions, NULL) == MC_PLAYER_OK);
    CHECK(mc_registry_classes_free(registry, NULL) == MC_REGISTRY_OK);
    CHECK(mc_entity_classes_free(entities) == MC_ENTITY_OK);
    mc_java_random_profile_free(profile);
    mc_materials_destroy(materials);
    CHECK(mc_resource_files_free(files) == MC_REGISTRY_OK);
    return true;
}

static bool failures(void) {
    mc_java_random_profile *profile = NULL;
    mc_entity_classes *entities = NULL;
    mc_entity *entity = NULL;
    CHECK(mc_java_random_profile_create(sizeof(*profile), &profile) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_entity_classes_create(MC_ENTITY_CLIENT, profile, NULL, &entities) == MC_ENTITY_OK);
    CHECK(mc_local_player_allocate(entities, &entity, NULL) == MC_ENTITY_OK);
    mc_player_error error;
    CHECK(mc_local_player_construct(entity, NULL, NULL, NULL, 0, NULL, &error) ==
          MC_PLAYER_PROFILE_FAILED);
    mc_local_player_snapshot s;
    CHECK(mc_local_player_get_snapshot(entity, &s) == MC_ENTITY_OK);
    CHECK(watcher(entity));
    CHECK(error.site.pc == 93 && s.living_pc == 89 && s.watcher_hook_pc == 17 && s.living.E == 20 &&
          s.living.ai == -1);
    CHECK(s.living.aj == 0 && s.living.aB == 0 && s.player.c == NULL && !entity->base.aF);
    mc_entity_class_snapshot reached;
    CHECK(mc_entity_classes_get_snapshot(entities, &reached) == MC_ENTITY_OK);
    const size_t only_base =
        reached.allocation_bytes - sizeof(mc_entity_watch_record) - sizeof(mc_entity_watch_node);
    mc_entity_release(entity);
    CHECK(mc_entity_classes_free(entities) == MC_ENTITY_OK);
    mc_java_random_profile_free(profile);
    profile = NULL;
    entities = NULL;
    entity = NULL;
    CHECK(mc_java_random_profile_create(SIZE_MAX, &profile) == MC_JAVA_RANDOM_PROFILE_OK);
    const mc_entity_limits budget = {only_base, SIZE_MAX};
    CHECK(mc_entity_classes_create(MC_ENTITY_CLIENT, profile, &budget, &entities) == MC_ENTITY_OK);
    CHECK(mc_local_player_allocate(entities, &entity, NULL) == MC_ENTITY_OK);
    CHECK(mc_local_player_construct(entity, NULL, NULL, NULL, 0, NULL, &error) ==
          MC_PLAYER_ENTITY_FAILED);
    CHECK(mc_local_player_get_snapshot(entity, &s) == MC_ENTITY_OK);
    CHECK(entity->phase == MC_ENTITY_FAILED_CONSTRUCTION && entity->base_bytecode == 224);
    CHECK(s.watcher_hook_pc == 1 && s.living_pc == 0 && s.living.E == 0 && s.living.ai == 0 &&
          s.player.c == NULL);
    CHECK(entity->watcher->map->size == 1 && entity->watcher->map->head->record->index == 0);
    CHECK(atomic_load(&profile->holder_phase) == MC_JAVA_RANDOM_HOLDER_COLD &&
          profile->math == NULL);
    mc_entity_release(entity);
    CHECK(mc_entity_classes_free(entities) == MC_ENTITY_OK);
    mc_java_random_profile_free(profile);
    profile = NULL;
    entities = NULL;
    CHECK(mc_java_random_profile_create(SIZE_MAX, &profile) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_entity_classes_create(MC_ENTITY_SERVER, profile, NULL, &entities) == MC_ENTITY_OK);
    const mc_entity_class_literal *literal = NULL;
    CHECK(mc_entity_class_literal_get(entities, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER, &literal) ==
              MC_ENTITY_INVALID_ARGUMENT &&
          literal == NULL);
    entity = NULL;
    CHECK(mc_local_player_allocate(entities, &entity, NULL) == MC_ENTITY_INVALID_ARGUMENT &&
          entity == NULL);
    CHECK(mc_entity_classes_free(entities) == MC_ENTITY_OK);
    mc_java_random_profile_free(profile);
    return true;
}

int main(int argc, char **argv) {
    if (argc != 3 || !mc_math_trig_init(&trig))
        return 2;
    fixture f = {0};
    if (!setup(&f, argv[1], argv[2]))
        return 1;
    static const uint16_t ascii[] = {'S', 't', 'e', 'v', 'e'};
    static const uint16_t unicode[] = {0, 0xd800, 0x00e9, 0xdc00};
    if (!construct_case(&f, ascii, 5, false, false, false, 64) ||
        !construct_case(&f, NULL, 0, false, false, false, INT32_MAX) ||
        !construct_case(&f, NULL, 0, true, false, false, INT32_MIN) ||
        !construct_case(&f, unicode, 4, false, false, false, -2) ||
        !construct_case(&f, NULL, 0, true, true, false, 0) ||
        !construct_case(&f, NULL, 0, true, true, true, 0) || !failures() ||
        !session_failure(argv[2]) || !teardown(&f))
        return 1;
    printf("Concrete local Player construction checks passed: %u\n", checks);
    return 0;
}
