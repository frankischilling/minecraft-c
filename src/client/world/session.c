#include "session.h"

#include "../../world/runtime/chunk_provider_internal.h"
#include "../../world/storage/objects_internal.h"
#include "mc/world/biome/registry.h"
#include "mc/world/runtime/entities.h"
#include "millis.h"

#include <limits.h>
#include <stdlib.h>

struct mc_client_world_session {
    mc_world_runtime_classes *classes;
    mc_registry_classes *registry;
    mc_world_regions *regions;
    mc_world_store *store;
    mc_world_info_name *name;
    mc_world_runtime *world;
    bool attempted, ready, closing;
};

static bool result(mc_client_world_session_error *out, mc_client_world_session_error frame,
                   mc_world_storage_status status, const char *operation) {
    frame.storage = (mc_world_storage_error){status, operation, 0};
    if (out != NULL)
        *out = frame;
    return status == MC_WORLD_STORAGE_OK;
}

static bool storage_result(mc_client_world_session_error *out,
                           mc_client_world_session_error frame) {
    if (out != NULL)
        *out = frame;
    return frame.storage.status == MC_WORLD_STORAGE_OK;
}

bool mc_client_world_session_allocate(mc_world_runtime_classes *classes,
                                      mc_registry_classes *registry, mc_client_world_session **out,
                                      mc_client_world_session_error *error) {
    mc_client_world_session_error frame = {0};
    mc_world_runtime_class_state state;
    if (classes == NULL || registry == NULL || out == NULL ||
        mc_world_runtime_classes_get(classes, &state) != MC_WORLD_STORAGE_OK ||
        state.side != MC_WORLD_INFO_CLIENT)
        return result(error, frame, MC_WORLD_STORAGE_INVALID_ARGUMENT, "Client World classes");
    mc_client_world_session *session = calloc(1, sizeof(*session));
    if (session == NULL)
        return result(error, frame, MC_WORLD_STORAGE_ALLOCATION_FAILED, "Client World session");
    session->classes = classes;
    session->registry = registry;
    *out = session;
    return result(error, frame, MC_WORLD_STORAGE_OK, NULL);
}

mc_world_runtime *mc_client_world_session_world(const mc_client_world_session *session) {
    return session != NULL && session->ready && !session->closing ? session->world : NULL;
}

bool mc_client_world_session_open(mc_client_world_session *session, const char *directory,
                                  mc_nbt_string name, int64_t seed, size_t max_chunks,
                                  mc_client_world_session_error *error) {
    mc_client_world_session_error frame = {0};
    if (session == NULL || directory == NULL || directory[0] == '\0' ||
        (name.length != 0 && name.units == NULL) || max_chunks > INT32_MAX)
        return result(error, frame, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                      "Client World open arguments");
    if (session->attempted || session->closing)
        return result(error, frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Client World open phase");
    session->attempted = true;
    frame.registry.status =
        mc_registry_classes_initialize_blocks(session->registry, &frame.registry);
    if (frame.registry.status == MC_REGISTRY_OK)
        frame.registry.status =
            mc_registry_classes_initialize_biomes(session->registry, &frame.registry);
    if (frame.registry.status != MC_REGISTRY_OK)
        return result(error, frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                      "Client World Registry initialization");
    mc_registry_block_view blocks;
    if (mc_registry_classes_inspect_blocks(session->registry, &blocks, &frame.registry) !=
            MC_REGISTRY_OK ||
        blocks.phase != MC_REGISTRY_READY || blocks.m == NULL)
        return result(error, frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Client World block map");
    uint8_t block_map[256] = {0};
    for (size_t i = 1; i < sizeof(block_map); ++i)
        block_map[i] = blocks.m[i] == NULL ? 0 : (uint8_t)i;
    if (mc_world_regions_create(NULL, &session->regions, &frame.storage) != MC_WORLD_STORAGE_OK)
        return storage_result(error, frame);
    int64_t token;
    frame.clock_status = (int32_t)mc_platform_current_time_millis(&token);
    if (frame.clock_status != MC_PLATFORM_MILLIS_OK)
        return result(error, frame, MC_WORLD_STORAGE_IO_ERROR, "Client World session clock");
    mc_world_store_options options = {.session_token = token,
                                      .block_map = block_map,
                                      .block_map_length = sizeof(block_map),
                                      .limits = mc_world_store_default_limits()};
    options.limits.max_loaded_chunks = max_chunks;
    if (mc_world_store_open(directory, session->regions, &options, &session->store,
                            &frame.storage) != MC_WORLD_STORAGE_OK)
        return storage_result(error, frame);
    frame.name_status = mc_world_info_name_copy(name, SIZE_MAX, &session->name);
    if (frame.name_status != MC_WORLD_INFO_OK)
        return result(error, frame,
                      frame.name_status == MC_WORLD_INFO_ALLOCATION_FAILED
                          ? MC_WORLD_STORAGE_ALLOCATION_FAILED
                          : MC_WORLD_STORAGE_INVALID_ARGUMENT,
                      "Client World name");
    mc_world_runtime_construction_limits limits = mc_world_runtime_default_construction_limits();
    limits.max_provider_chunks = max_chunks;
    if (mc_world_runtime_allocate_world(MC_WORLD_RUNTIME_CLIENT_LOAD_OR_CREATE, session->classes,
                                        &limits, &session->world,
                                        &frame.storage) != MC_WORLD_STORAGE_OK)
        return storage_result(error, frame);
    if (mc_world_runtime_registry(session->world) != session->registry)
        return result(error, frame, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                      "Client World Registry binding");
    const mc_world_runtime_world_arguments arguments = {
        .store = session->store, .name = session->name, .seed = seed};
    if (mc_world_runtime_construct_world(session->world, &arguments, &frame.storage) !=
        MC_WORLD_STORAGE_OK)
        return storage_result(error, frame);
    session->ready = true;
    return result(error, frame, MC_WORLD_STORAGE_OK, NULL);
}

bool mc_client_world_session_retire(mc_client_world_session **owner,
                                    mc_client_world_session_error *error) {
    mc_client_world_session_error frame = {0};
    if (owner == NULL)
        return result(error, frame, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                      "Client World retirement arguments");
    mc_client_world_session *session = *owner;
    if (session == NULL)
        return result(error, frame, MC_WORLD_STORAGE_OK, NULL);
    session->closing = true;
    if (session->world != NULL) {
        if (mc_world_runtime_retire_entity_containers(session->world, &frame.storage) !=
            MC_WORLD_STORAGE_OK)
            return storage_result(error, frame);
        mc_world_runtime_fields fields;
        if (mc_world_runtime_fields_get(session->world, &fields) != MC_WORLD_STORAGE_OK)
            return result(error, frame, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                          "Client World retirement fields");
        if (fields.v != NULL) {
            mc_chunk_provider_fields provider;
            if (mc_chunk_provider_bind_fields(fields.v, &provider, &frame.storage) !=
                MC_WORLD_STORAGE_OK)
                return storage_result(error, frame);
            if (mc_chunk_pending_clear(provider.pending) != MC_CHUNK_ACQUIRE_OK ||
                !mc_chunk_cache_init(provider.cache, provider.cache->entries,
                                     provider.cache->capacity, provider.cache->insertions,
                                     provider.cache->insertion_capacity))
                return result(error, frame, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                              "Client World terminal cache release");
        }
        if (mc_world_runtime_retire_world(session->world, &frame.storage) != MC_WORLD_STORAGE_OK)
            return storage_result(error, frame);
    }
    while (mc_world_store_chunk_count(session->store) != 0) {
        mc_chunk_state *chunk = NULL;
        if (mc_world_store_chunk_at(session->store, 0, &chunk, &frame.storage) !=
                MC_WORLD_STORAGE_OK ||
            mc_world_store_retire_chunk(session->store, chunk, &frame.storage) !=
                MC_WORLD_STORAGE_OK)
            return storage_result(error, frame);
    }
    if (mc_world_runtime_free(session->world, &frame.storage) != MC_WORLD_STORAGE_OK)
        return storage_result(error, frame);
    session->world = NULL;
    if (mc_world_store_free_checked(&session->store, &frame.storage) != MC_WORLD_STORAGE_OK)
        return storage_result(error, frame);
    mc_world_info_name_release(session->name);
    mc_world_regions_free(session->regions);
    free(session);
    *owner = NULL;
    return result(error, frame, MC_WORLD_STORAGE_OK, NULL);
}
