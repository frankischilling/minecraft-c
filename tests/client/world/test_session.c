#include "session.h"

#include "mc/resources/files/files.h"
#include "mc/world/runtime/entities.h"

#include <stdio.h>
#include <stdlib.h>

static unsigned checks;
#define CHECK(expression)                                                                          \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(expression)) {                                                                       \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expression);                       \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

int main(int argc, char **argv) {
    int result = 1;
    mc_resource_files *files = NULL;
    mc_materials *materials = NULL;
    mc_java_random_profile *profile = NULL;
    mc_registry_classes *registry = NULL;
    mc_entity_classes *entities = NULL;
    mc_world_runtime_classes *classes = NULL;
    mc_client_world_session *session = NULL;
    mc_world_runtime *held = NULL;
    mc_math_trig *trig = malloc(sizeof(*trig));
    mc_client_world_session_error error = {0};
    CHECK(argc == 3 && trig != NULL && mc_math_trig_init(trig));
    CHECK(mc_resource_files_create(argv[1], &files) == MC_REGISTRY_OK);
    CHECK(mc_materials_create(&materials));
    CHECK(mc_java_random_profile_create(SIZE_MAX, &profile) == MC_JAVA_RANDOM_PROFILE_OK);
    const mc_registry_options options = {.side = MC_REGISTRY_CLIENT,
                                         .random_profile = profile,
                                         .materials = materials,
                                         .resources = mc_resource_files_source(files)};
    CHECK(mc_registry_classes_allocate(&options, &registry, NULL) == MC_REGISTRY_OK);
    CHECK(mc_entity_classes_create(MC_ENTITY_CLIENT, profile, NULL, &entities) == MC_ENTITY_OK);
    CHECK(mc_world_runtime_classes_create(MC_WORLD_INFO_CLIENT, profile, trig, &classes, NULL) ==
          MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_classes_bind_registry(classes, registry, NULL) == MC_REGISTRY_OK);
    CHECK(mc_world_runtime_classes_bind_entities(classes, entities) == MC_ENTITY_OK);
    CHECK(!mc_client_world_session_allocate(NULL, registry, &session, &error));
    CHECK(session == NULL && error.storage.status == MC_WORLD_STORAGE_INVALID_ARGUMENT);
    CHECK(mc_client_world_session_allocate(classes, registry, &session, &error));
    CHECK(mc_client_world_session_world(session) == NULL);
    CHECK(!mc_client_world_session_open(session, "", (mc_nbt_string){0}, 23, 32, &error));
    const uint16_t name[] = {'T', 'e', 's', 't', 0, 0xd800u};
    CHECK(mc_client_world_session_open(session, argv[2], (mc_nbt_string){name, 6}, 23, 32, &error));
    mc_world_runtime *world = mc_client_world_session_world(session);
    CHECK(world != NULL && mc_world_runtime_registry(world) == registry);
    mc_world_runtime_construction construction;
    CHECK(mc_world_runtime_construction_get(world, &construction) == MC_WORLD_STORAGE_OK);
    CHECK(construction.mode == MC_WORLD_RUNTIME_GAME &&
          construction.phase == MC_WORLD_RUNTIME_READY &&
          construction.entry == MC_WORLD_RUNTIME_CLIENT_LOAD_OR_CREATE);
    mc_world_info_fields info;
    CHECK(mc_world_info_fields_get(mc_world_runtime_info(world), &info) == MC_WORLD_INFO_OK);
    CHECK(info.value.seed == 23);
    const mc_world_runtime_state *live = mc_world_runtime_live(world);
    CHECK(live != NULL && live->spawn_x != NULL && live->spawn_y != NULL && live->spawn_z != NULL);
    mc_chunk_state *chunk = NULL;
    CHECK(mc_world_runtime_acquire(world, 1, 0, &chunk) == MC_CHUNK_ACQUIRE_OK && chunk != NULL);
    CHECK(!mc_client_world_session_open(session, argv[2], (mc_nbt_string){0}, 24, 32, &error));
    CHECK(error.storage.status == MC_WORLD_STORAGE_UNSUPPORTED_STATE);
    CHECK(mc_world_runtime_binding_retain(world));
    held = world;
    CHECK(!mc_client_world_session_retire(&session, &error));
    CHECK(session != NULL && error.storage.status == MC_WORLD_STORAGE_BUSY);
    CHECK(mc_client_world_session_world(session) == NULL);
    mc_world_runtime_binding_release(held);
    held = NULL;
    CHECK(mc_client_world_session_retire(&session, &error) && session == NULL);
    CHECK(mc_client_world_session_retire(&session, &error));
    CHECK(mc_client_world_session_allocate(classes, registry, &session, &error));
    CHECK(!mc_client_world_session_open(session, argv[2], (mc_nbt_string){0}, 23, 0, &error));
    CHECK(mc_client_world_session_world(session) == NULL);
    CHECK(mc_client_world_session_retire(&session, &error));
    result = 0;
cleanup:
    if (result != 0)
        fprintf(stderr, "World session failure: %d %s; Registry %d %s at %s:%d; name %d clock %d\n",
                (int)error.storage.status,
                error.storage.operation == NULL ? "" : error.storage.operation,
                (int)error.registry.status,
                error.registry.message == NULL ? "" : error.registry.message,
                error.registry.active_initializer_class == NULL
                    ? ""
                    : error.registry.active_initializer_class,
                (int)error.registry.active_initializer_pc, (int)error.name_status,
                (int)error.clock_status);
    if (held != NULL)
        mc_world_runtime_binding_release(held);
    if (!mc_client_world_session_retire(&session, &error)) {
        fprintf(stderr, "Session retirement: %s\n", error.storage.operation);
        return 1;
    }
    if (mc_world_runtime_classes_free(classes, NULL) != MC_WORLD_STORAGE_OK ||
        mc_registry_classes_free(registry, NULL) != MC_REGISTRY_OK ||
        mc_entity_classes_free(entities) != MC_ENTITY_OK)
        return 1;
    mc_materials_destroy(materials);
    mc_java_random_profile_free(profile);
    if (mc_resource_files_free(files) != MC_REGISTRY_OK)
        result = 1;
    free(trig);
    if (result == 0)
        printf("World session: %u checks\n", checks);
    return result;
}
