#include "mc/world/storage/store.h"

#include "../chunk/object_internal.h"
#include "../runtime/source_terminal_internal.h"
#include "../runtime/terminal_inventory_internal.h"
#include "chunk_build_internal.h"
#include "files.h"
#include "generated_internal.h"
#include "mc/core/java/java.h"
#include "metadata_internal.h"
#include "objects_internal.h"
#include "source_handler_internal.h"

#include <stdlib.h>
#include <string.h>

typedef enum store_chunk_kind { STORE_CHUNK_STORED, STORE_CHUNK_GENERATED } store_chunk_kind;

typedef struct store_chunk store_chunk;

struct mc_world_store_game_stage {
    store_chunk *entry;
};

struct mc_world_store_game_save {
    store_chunk *entry;
    mc_world_runtime *world;
    mc_world_info *account;
};

struct store_chunk {
    store_chunk_kind kind;

    union {
        mc_stored_chunk *stored;
        mc_world_chunk *generated;
    } owner;

    mc_world_chunk *loaded_binding;
    int32_t dimension;
    size_t leases;
    mc_world_store_game_stage_phase phase;
    bool game_stage, imports_started, coordinator_lease;
    bool source_header_failed;
    mc_world_store_game_stage stage;
    /* Native occurrence provenance over the exact immutable decoded owner.
     * No Entity/World holds or replacement source objects live here. */
    const mc_nbt_tag *entity_source_root, *entity_source_list;
    bool *entity_consumed;
    size_t entity_source_count, entity_processed;
    bool game_save_open;
    mc_world_store_game_save save;
    struct store_chunk *next;
};

typedef struct store_cache {
    const mc_chunk_cache *cache;
    mc_chunk_state *const *fallback;
    struct store_cache *next;
} store_cache;

struct mc_world_store_reservation {
    store_chunk *entry;
    struct mc_world_store_reservation *next;
};

struct mc_world_store {
    mc_world_store_source *source; /* NULL for unchanged legacy owners. */
    mc_world_regions *regions;
    mc_world_store_limits limits;
    char *root;
    char *nether;
    char *session;
    char *metadata_paths[3]; /* current, old, new */
    int64_t session_token;
    uint8_t block_map[256];
    mc_nbt_document *metadata;
    mc_level_info info;
    bool has_info;
    store_chunk *chunks, *last_chunk;
    size_t chunk_count;
    store_cache *caches;
    size_t cache_count;
    mc_world_store_reservation *reservations;
    size_t reservation_count;
};

static mc_chunk_state *store_chunk_state(const store_chunk *entry) {
    switch (entry->kind) {
        case STORE_CHUNK_STORED:
            return mc_stored_chunk_state(entry->owner.stored);
        case STORE_CHUNK_GENERATED:
            return mc_world_chunk_state(entry->owner.generated);
    }
    return NULL;
}

static const mc_nbt_tag *store_chunk_root(const store_chunk *entry) {
    switch (entry->kind) {
        case STORE_CHUNK_STORED:
            return mc_stored_chunk_root(entry->owner.stored);
        case STORE_CHUNK_GENERATED:
            return NULL;
    }
    return NULL;
}

static mc_stored_chunk_status store_chunk_build(const store_chunk *entry, int64_t world_time,
                                                const mc_nbt_limits *limits, mc_nbt_document **out,
                                                mc_stored_chunk_error *error) {
    if (entry->phase != MC_WORLD_STORE_GAME_COMMITTED) {
        if (error != NULL)
            *error =
                (mc_stored_chunk_error){MC_STORED_CHUNK_UNSUPPORTED_EDIT, NULL, {MC_NBT_OK, 0}};
        return MC_STORED_CHUNK_UNSUPPORTED_EDIT;
    }
    if (entry->kind == STORE_CHUNK_STORED &&
        (entry->loaded_binding == NULL || mc_world_chunk_storage_empty(entry->loaded_binding)))
        return mc_stored_chunk_build(entry->owner.stored, world_time, limits, out, error);
    if (entry->kind == STORE_CHUNK_GENERATED &&
        mc_world_chunk_storage_empty(entry->owner.generated))
        return mc_stored_chunk_build_fresh(mc_world_chunk_state(entry->owner.generated), world_time,
                                           limits, out, error);
    if (error != NULL)
        *error = (mc_stored_chunk_error){MC_STORED_CHUNK_UNSUPPORTED_EDIT, NULL, {MC_NBT_OK, 0}};
    return MC_STORED_CHUNK_UNSUPPORTED_EDIT;
}

static void store_chunk_free(store_chunk *entry) {
    free(entry->entity_consumed);
    switch (entry->kind) {
        case STORE_CHUNK_STORED:
            mc_world_chunk_storage_free(entry->loaded_binding);
            mc_stored_chunk_free(entry->owner.stored);
            break;
        case STORE_CHUNK_GENERATED:
            mc_world_chunk_storage_free(entry->owner.generated);
            break;
    }
}

static mc_world_storage_status store_error(mc_world_storage_error *error,
                                           mc_world_storage_status status, const char *operation) {
    if (error != NULL)
        *error = (mc_world_storage_error){status, operation, 0};
    return status;
}

static mc_world_storage_status store_nbt_error(mc_world_storage_error *error,
                                               mc_nbt_status status) {
    mc_world_storage_status result =
        status == MC_NBT_LIMIT               ? MC_WORLD_STORAGE_LIMIT
        : status == MC_NBT_ALLOCATION_FAILED ? MC_WORLD_STORAGE_ALLOCATION_FAILED
        : status == MC_NBT_INVALID_ARGUMENT  ? MC_WORLD_STORAGE_INVALID_ARGUMENT
                                             : MC_WORLD_STORAGE_INVALID_DATA;
    return store_error(error, result, mc_nbt_status_string(status));
}

static mc_world_storage_status store_level_error(mc_world_storage_error *error,
                                                 mc_level_status status,
                                                 const mc_level_error *detail) {
    mc_world_storage_status result =
        status == MC_LEVEL_LIMIT               ? MC_WORLD_STORAGE_LIMIT
        : status == MC_LEVEL_ALLOCATION_FAILED ? MC_WORLD_STORAGE_ALLOCATION_FAILED
        : status == MC_LEVEL_INVALID_ARGUMENT  ? MC_WORLD_STORAGE_INVALID_ARGUMENT
                                               : MC_WORLD_STORAGE_INVALID_DATA;
    return store_error(error, result,
                       detail->field == NULL ? mc_level_status_string(status) : detail->field);
}

static mc_world_storage_status store_compression_error(mc_world_storage_error *error,
                                                       mc_compression_status status) {
    mc_world_storage_status result =
        status == MC_COMPRESSION_LIMIT               ? MC_WORLD_STORAGE_LIMIT
        : status == MC_COMPRESSION_ALLOCATION_FAILED ? MC_WORLD_STORAGE_ALLOCATION_FAILED
        : status == MC_COMPRESSION_INVALID_ARGUMENT  ? MC_WORLD_STORAGE_INVALID_ARGUMENT
                                                     : MC_WORLD_STORAGE_INVALID_DATA;
    return store_error(error, result, mc_compression_status_string(status));
}

static mc_world_storage_status store_chunk_error(mc_world_storage_error *error,
                                                 mc_stored_chunk_status status,
                                                 const mc_stored_chunk_error *detail) {
    mc_world_storage_status result =
        status == MC_STORED_CHUNK_LIMIT               ? MC_WORLD_STORAGE_LIMIT
        : status == MC_STORED_CHUNK_ALLOCATION_FAILED ? MC_WORLD_STORAGE_ALLOCATION_FAILED
        : status == MC_STORED_CHUNK_INVALID_ARGUMENT  ? MC_WORLD_STORAGE_INVALID_ARGUMENT
        : status == MC_STORED_CHUNK_UNSUPPORTED_EDIT  ? MC_WORLD_STORAGE_UNSUPPORTED_STATE
                                                      : MC_WORLD_STORAGE_INVALID_DATA;
    return store_error(error, result,
                       detail->field == NULL ? mc_stored_chunk_status_string(status)
                                             : detail->field);
}

mc_world_store_limits mc_world_store_default_limits(void) {
    const mc_world_store_limits limits = {512, mc_nbt_default_limits(),
                                          mc_compression_default_limits()};
    return limits;
}

static void store_token(int64_t value, uint8_t bytes[8]) {
    uint64_t bits = (uint64_t)value;
    for (size_t i = 0; i < 8; ++i)
        bytes[i] = (uint8_t)(bits >> (56u - 8u * i));
}

mc_world_storage_status mc_world_store_open(const char *root, mc_world_regions *regions,
                                            const mc_world_store_options *options,
                                            mc_world_store **out, mc_world_storage_error *error) {
    if (regions == NULL || options == NULL || out == NULL || options->block_map == NULL ||
        options->block_map_length < 256 || options->limits.nbt.max_depth == 0 ||
        options->limits.nbt.max_depth > 128 || options->limits.compression.max_members == 0)
        return store_error(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "world store arguments");
    for (size_t i = 0; i < 256; ++i) {
        if (options->block_map[i] != 0 && options->block_map[i] != (uint8_t)i)
            return store_error(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "frozen block map");
    }
    char *data = NULL, *players = NULL;
    mc_world_storage_status status = mc_ws_path_join(root, "data", &data, error);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    mc_world_store *store = calloc(1, sizeof(*store));
    if (store == NULL) {
        free(data);
        return store_error(error, MC_WORLD_STORAGE_ALLOCATION_FAILED, "world store owner");
    }
    store->regions = regions;
    store->limits = options->limits;
    store->session_token = options->session_token;
    memcpy(store->block_map, options->block_map, sizeof(store->block_map));
    size_t root_length = strlen(root); /* path_join validated the bounded root. */
    store->root = malloc(root_length + 1u);
    if (store->root == NULL)
        status = store_error(error, MC_WORLD_STORAGE_ALLOCATION_FAILED, "world root path");
    else
        memcpy(store->root, root, root_length + 1u);
    if (status == MC_WORLD_STORAGE_OK)
        status = mc_ws_path_join(root, "DIM-1", &store->nether, error);
    if (status == MC_WORLD_STORAGE_OK)
        status = mc_ws_path_join(root, "session.lock", &store->session, error);
    static const char *const names[] = {"level.dat", "level.dat_old", "level.dat_new"};
    for (size_t i = 0; status == MC_WORLD_STORAGE_OK && i < 3; ++i)
        status = mc_ws_path_join(root, names[i], &store->metadata_paths[i], error);
    if (status == MC_WORLD_STORAGE_OK && options->create_players)
        status = mc_ws_path_join(root, "players", &players, error);
    if (status == MC_WORLD_STORAGE_OK)
        status = mc_ws_mkdirs(data, error);
    if (status == MC_WORLD_STORAGE_OK && players != NULL)
        status = mc_ws_mkdirs(players, error);
    if (status == MC_WORLD_STORAGE_OK) {
        uint8_t token[8];
        store_token(store->session_token, token);
        status = mc_ws_write_file(store->session, token, sizeof(token), false, error);
    }
    free(data);
    free(players);
    if (status != MC_WORLD_STORAGE_OK) {
        mc_world_store_free(store);
        return status;
    }
    *out = store;
    return store_error(error, MC_WORLD_STORAGE_OK, NULL);
}

mc_world_storage_status mc_world_store_check_session(const mc_world_store *store,
                                                     mc_world_storage_error *error) {
    if (store != NULL && store->source != NULL)
        return store_source_legacy(error);
    if (store == NULL)
        return store_error(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "session store");
    uint8_t actual[8], expected[8];
    mc_world_storage_error detail;
    mc_world_storage_status status =
        mc_ws_read_prefix(store->session, actual, sizeof(actual), &detail);
    if (status != MC_WORLD_STORAGE_OK) {
        if (status != MC_WORLD_STORAGE_INVALID_ARGUMENT && status != MC_WORLD_STORAGE_LIMIT &&
            status != MC_WORLD_STORAGE_ALLOCATION_FAILED) {
            status = MC_WORLD_STORAGE_SESSION_MISMATCH;
            detail.status = status;
        }
        if (error != NULL)
            *error = detail;
        return status;
    }
    store_token(store->session_token, expected);
    if (memcmp(actual, expected, sizeof(actual)) != 0)
        return store_error(error, MC_WORLD_STORAGE_SESSION_MISMATCH, "session token");
    return store_error(error, MC_WORLD_STORAGE_OK, NULL);
}

static mc_world_storage_status store_read_info(const mc_world_store *store, const char *path,
                                               mc_nbt_document **out, mc_level_info *info,
                                               mc_world_storage_error *error) {
    size_t max_bytes = store->limits.compression.max_input_bytes;
    if (max_bytes > MC_WS_FILE_MAX_BYTES)
        max_bytes = MC_WS_FILE_MAX_BYTES;
    mc_ws_file_bytes file = {0};
    mc_world_storage_status status = mc_ws_read_file(path, max_bytes, &file, error);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    mc_compression_buffer decoded = {0};
    mc_compression_status compression = mc_compression_decode(
        MC_COMPRESSION_GZIP, file.data, file.length, &store->limits.compression, &decoded, NULL);
    mc_ws_free_file_bytes(&file);
    if (compression != MC_COMPRESSION_OK)
        return store_compression_error(error, compression);
    mc_nbt_document *document = NULL;
    mc_nbt_status nbt = mc_nbt_read_compound(decoded.data, decoded.length, &store->limits.nbt,
                                             &document, NULL, NULL);
    mc_compression_buffer_free(&decoded);
    if (nbt != MC_NBT_OK)
        return store_nbt_error(error, nbt);
    mc_level_error detail;
    mc_level_status level = mc_level_decode(mc_nbt_document_root(document), info, &detail);
    if (level != MC_LEVEL_OK) {
        mc_nbt_document_free(document);
        return store_level_error(error, level, &detail);
    }
    *out = document;
    return store_error(error, MC_WORLD_STORAGE_OK, NULL);
}

static bool store_fallback_status(mc_world_storage_status status) {
    return status == MC_WORLD_STORAGE_MISSING || status == MC_WORLD_STORAGE_IO_ERROR ||
           status == MC_WORLD_STORAGE_INVALID_DATA;
}

mc_world_storage_status mc_world_store_load_info(mc_world_store *store,
                                                 mc_world_storage_error *error) {
    if (store != NULL && store->source != NULL)
        return store_source_legacy(error);
    if (store == NULL)
        return store_error(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "metadata store");
    mc_nbt_document *document = NULL;
    mc_level_info info;
    mc_world_storage_error detail, primary;
    mc_world_storage_status status =
        store_read_info(store, store->metadata_paths[0], &document, &info, &primary);
    detail = primary;
    if (store_fallback_status(status) && status != MC_WORLD_STORAGE_OK) {
        status = store_read_info(store, store->metadata_paths[1], &document, &info, &detail);
        if (status == MC_WORLD_STORAGE_MISSING && primary.status != MC_WORLD_STORAGE_MISSING) {
            status = primary.status;
            detail = primary;
        }
    }
    if (status != MC_WORLD_STORAGE_OK) {
        if (error != NULL)
            *error = detail;
        return status;
    }
    mc_nbt_document *previous = store->metadata;
    store->metadata = document;
    store->info = info;
    store->has_info = true;
    mc_nbt_document_free(previous);
    return store_error(error, MC_WORLD_STORAGE_OK, NULL);
}

static mc_world_storage_status store_world_info_error(mc_world_storage_error *error,
                                                      mc_world_info_status status,
                                                      const mc_world_info_error *detail) {
    if (status == MC_WORLD_INFO_NBT_ERROR && detail != NULL)
        return store_nbt_error(error, detail->nbt.status);
    mc_world_storage_status mapped =
        status == MC_WORLD_INFO_LIMIT               ? MC_WORLD_STORAGE_LIMIT
        : status == MC_WORLD_INFO_ALLOCATION_FAILED ? MC_WORLD_STORAGE_ALLOCATION_FAILED
        : status == MC_WORLD_INFO_INVALID_ARGUMENT  ? MC_WORLD_STORAGE_INVALID_ARGUMENT
        : status == MC_WORLD_INFO_INVALID_FIELD     ? MC_WORLD_STORAGE_INVALID_DATA
                                                    : MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    return store_error(error, mapped,
                       detail != NULL && detail->field != NULL
                           ? detail->field
                           : mc_world_info_status_string(status));
}

static mc_world_storage_status
store_read_world_info(const mc_world_store *store, const char *path, mc_world_info_side side,
                      mc_nbt_document **out_document, mc_world_info **out_info,
                      mc_level_info *out_view, mc_world_storage_error *error) {
    size_t max_bytes = store->limits.compression.max_input_bytes;
    if (max_bytes > MC_WS_FILE_MAX_BYTES)
        max_bytes = MC_WS_FILE_MAX_BYTES;
    mc_ws_file_bytes file = {0};
    mc_world_storage_status status = mc_ws_read_file(path, max_bytes, &file, error);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    mc_compression_buffer decoded = {0};
    mc_compression_status compression = mc_compression_decode(
        MC_COMPRESSION_GZIP, file.data, file.length, &store->limits.compression, &decoded, NULL);
    mc_ws_free_file_bytes(&file);
    if (compression != MC_COMPRESSION_OK)
        return store_compression_error(error, compression);
    mc_nbt_document *document = NULL;
    mc_nbt_status nbt = mc_nbt_read_compound(decoded.data, decoded.length, &store->limits.nbt,
                                             &document, NULL, NULL);
    mc_compression_buffer_free(&decoded);
    if (nbt != MC_NBT_OK)
        return store_nbt_error(error, nbt);

    static const uint16_t data_key[] = {'D', 'a', 't', 'a'};
    static const uint16_t name_key[] = {'L', 'e', 'v', 'e', 'l', 'N', 'a', 'm', 'e'};
    static const uint16_t player_key[] = {'P', 'l', 'a', 'y', 'e', 'r'};
    const mc_nbt_tag empty_data = {.type = MC_NBT_COMPOUND};
    const mc_nbt_tag *data = mc_nbt_compound_get(mc_nbt_document_root(document), data_key, 4);
    if (data == NULL)
        data = &empty_data;
    else if (data->type != MC_NBT_COMPOUND) {
        /* Original getCompound defaults only on absence; its present-value
         * checkcast failure is caught by the handler's candidate fallback. */
        mc_nbt_document_free(document);
        return store_error(error, MC_WORLD_STORAGE_INVALID_DATA, "Data");
    }

    mc_world_info *info = NULL;
    mc_world_info_data *owned_data = NULL;
    mc_world_info_error detail = {0};
    mc_world_info_status constructed = mc_world_info_allocate(side, &info);
    if (constructed == MC_WORLD_INFO_OK)
        constructed = mc_world_info_data_copy(data, &store->limits.nbt, &owned_data, &detail);
    if (constructed == MC_WORLD_INFO_OK)
        constructed = mc_world_info_construct_nbt(info, owned_data, &detail);
    mc_world_info_data_release(owned_data);
    if (constructed != MC_WORLD_INFO_OK) {
        mc_world_info_release(info);
        mc_nbt_document_free(document);
        return store_world_info_error(error, constructed, &detail);
    }

    mc_world_info_fields fields;
    (void)mc_world_info_fields_get(info, &fields);
    const mc_nbt_tag *name = mc_nbt_compound_get(data, name_key, 9);
    const mc_nbt_tag *player = mc_nbt_compound_get(data, player_key, 6);
    /* Scalars come from the completed real constructor. Reference views must
     * borrow the retained document, never the independently held Info copy. */
    *out_view =
        (mc_level_info){.seed = fields.value.seed,
                        .spawn_x = fields.value.spawn_x,
                        .spawn_y = fields.value.spawn_y,
                        .spawn_z = fields.value.spawn_z,
                        .time = fields.value.time,
                        .last_played = fields.value.last_played,
                        .size_on_disk = fields.value.size_on_disk,
                        .level_name = name == NULL ? (mc_nbt_string){0} : name->value.string,
                        .version = fields.value.version,
                        .rain_time = fields.value.rain_time,
                        .thunder_time = fields.value.thunder_time,
                        .raining = fields.value.raining,
                        .thundering = fields.value.thundering,
                        .player = player,
                        .player_dimension = fields.value.player_dimension};
    *out_document = document;
    *out_info = info;
    return store_error(error, MC_WORLD_STORAGE_OK, NULL);
}

mc_world_storage_status mc_world_store_load_world_info(mc_world_store *store,
                                                       mc_world_info_side side, mc_world_info **out,
                                                       mc_world_storage_error *error) {
    if (store != NULL && store->source != NULL)
        return store_source_legacy(error);
    if (store == NULL || out == NULL ||
        (side != MC_WORLD_INFO_CLIENT && side != MC_WORLD_INFO_SERVER))
        return store_error(error, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                           "typed metadata load arguments");
    mc_nbt_document *document = NULL;
    mc_world_info *info = NULL;
    mc_level_info view;
    mc_world_storage_error detail, primary;
    mc_world_storage_status status = store_read_world_info(store, store->metadata_paths[0], side,
                                                           &document, &info, &view, &primary);
    detail = primary;
    if (store_fallback_status(status) && status != MC_WORLD_STORAGE_OK) {
        status = store_read_world_info(store, store->metadata_paths[1], side, &document, &info,
                                       &view, &detail);
        if (status == MC_WORLD_STORAGE_MISSING && primary.status != MC_WORLD_STORAGE_MISSING) {
            status = primary.status;
            detail = primary;
        }
    }
    if (status != MC_WORLD_STORAGE_OK) {
        if (error != NULL)
            *error = detail;
        return status;
    }
    mc_nbt_document *previous = store->metadata;
    store->metadata = document;
    store->info = view;
    store->has_info = true;
    mc_nbt_document_free(previous);
    *out = info;
    return store_error(error, MC_WORLD_STORAGE_OK, NULL);
}

const mc_level_info *mc_world_store_info(const mc_world_store *store) {
    return store == NULL || !store->has_info ? NULL : &store->info;
}

const mc_nbt_tag *mc_world_store_metadata_root(const mc_world_store *store) {
    return store == NULL ? NULL : mc_nbt_document_root(store->metadata);
}

const char *mc_world_store_root(const mc_world_store *store) {
    return store == NULL ? NULL : store->root;
}

mc_world_storage_status mc_world_store_session_token_get(const mc_world_store *store,
                                                         int64_t *out) {
    if (store == NULL || out == NULL)
        return MC_WORLD_STORAGE_INVALID_ARGUMENT;
    *out = store->session_token;
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status store_encode_nbt(const mc_nbt_tag *root, const mc_nbt_limits *limits,
                                                mc_ws_file_bytes *out,
                                                mc_world_storage_error *error,
                                                mc_nbt_error *detail) {
    if (detail != NULL)
        *detail = (mc_nbt_error){MC_NBT_OK, 0};
    size_t length = 0;
    mc_nbt_status nbt = mc_nbt_measure(root, limits, &length, detail);
    if (nbt != MC_NBT_OK)
        return store_nbt_error(error, nbt);
    if (length > limits->max_allocation_bytes)
        return store_error(error, MC_WORLD_STORAGE_LIMIT, "NBT serialization buffer");
    uint8_t *bytes = malloc(length);
    if (bytes == NULL)
        return store_error(error, MC_WORLD_STORAGE_ALLOCATION_FAILED, "NBT serialization buffer");
    mc_nbt_limits remaining = *limits;
    remaining.max_allocation_bytes -= length;
    size_t written = 0;
    nbt = mc_nbt_write(root, &remaining, bytes, length, &written, detail);
    if (nbt != MC_NBT_OK) {
        free(bytes);
        return store_nbt_error(error, nbt);
    }
    *out = (mc_ws_file_bytes){bytes, written};
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status store_existing_file(const char *path, bool *exists,
                                                   mc_world_storage_error *error) {
    mc_world_storage_status status = mc_ws_read_prefix(path, NULL, 0, error);
    if (status == MC_WORLD_STORAGE_MISSING) {
        *exists = false;
        return MC_WORLD_STORAGE_OK;
    }
    if (status == MC_WORLD_STORAGE_OK)
        *exists = true;
    return status;
}

static mc_world_storage_status store_rotate(mc_world_store *store, const uint8_t *bytes,
                                            size_t length, mc_world_storage_error *error) {
    mc_world_storage_status status =
        mc_ws_write_file(store->metadata_paths[2], bytes, length, false, error);
    bool current = false, old = false;
    if (status == MC_WORLD_STORAGE_OK)
        status = store_existing_file(store->metadata_paths[0], &current, error);
    if (status == MC_WORLD_STORAGE_OK)
        status = store_existing_file(store->metadata_paths[1], &old, error);
    if (status == MC_WORLD_STORAGE_OK && old)
        status = mc_ws_remove_file(store->metadata_paths[1], false, error);
    if (status == MC_WORLD_STORAGE_OK && current)
        status = mc_ws_replace_file(store->metadata_paths[0], store->metadata_paths[1], error);
    if (status == MC_WORLD_STORAGE_OK)
        status = mc_ws_replace_file(store->metadata_paths[2], store->metadata_paths[0], error);
    return status;
}

static mc_world_storage_status store_write_metadata(mc_world_store *store, const mc_nbt_tag *root,
                                                    mc_world_storage_error *error) {
    mc_ws_file_bytes raw = {0};
    mc_world_storage_status status = store_encode_nbt(root, &store->limits.nbt, &raw, error, NULL);
    mc_compression_buffer compressed = {0};
    if (status == MC_WORLD_STORAGE_OK) {
        mc_compression_status encoded = mc_compression_encode(
            MC_COMPRESSION_GZIP, raw.data, raw.length, &store->limits.compression, &compressed);
        if (encoded != MC_COMPRESSION_OK)
            status = store_compression_error(error, encoded);
    }
    mc_ws_free_file_bytes(&raw);
    if (status == MC_WORLD_STORAGE_OK)
        status = store_rotate(store, compressed.data, compressed.length, error);
    mc_compression_buffer_free(&compressed);
    return status;
}

mc_world_storage_status mc_world_store_save_info(mc_world_store *store, const mc_level_info *info,
                                                 int64_t saved_at_ms,
                                                 mc_world_storage_error *error) {
    if (store != NULL && store->source != NULL)
        return store_source_legacy(error);
    if (store == NULL || info == NULL)
        return store_error(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "metadata save arguments");
    if (info->player != (store->has_info ? store->info.player : NULL))
        return store_error(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "opaque Player replacement");
    mc_world_storage_status status = mc_world_store_check_session(store, error);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    mc_level_info requested = *info;
    requested.version = 19132;
    mc_nbt_document *document = NULL;
    mc_level_error detail;
    mc_level_status level = mc_level_build(&requested, mc_world_store_metadata_root(store),
                                           saved_at_ms, &store->limits.nbt, &document, &detail);
    if (level != MC_LEVEL_OK)
        return store_level_error(error, level, &detail);
    mc_level_info rebound;
    level = mc_level_decode(mc_nbt_document_root(document), &rebound, &detail);
    if (level != MC_LEVEL_OK) {
        mc_nbt_document_free(document);
        return store_level_error(error, level, &detail);
    }
    rebound.last_played = requested.last_played;
    status = store_write_metadata(store, mc_nbt_document_root(document), error);
    if (status != MC_WORLD_STORAGE_OK) {
        mc_nbt_document_free(document);
        return status;
    }
    mc_nbt_document *previous = store->metadata;
    store->metadata = document;
    store->info = rebound;
    store->has_info = true;
    mc_nbt_document_free(previous);
    return store_error(error, MC_WORLD_STORAGE_OK, NULL);
}

mc_world_storage_status mc_world_store_save_world_info(mc_world_store *store, mc_world_info *info,
                                                       mc_world_storage_error *error) {
    if (store != NULL && store->source != NULL)
        return store_source_legacy(error);
    if (store == NULL || info == NULL)
        return store_error(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "typed metadata arguments");
    if (mc_world_info_live_get(info) == NULL)
        return store_error(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                           "typed metadata requires READY WorldInfo");
    mc_world_storage_status status = mc_world_store_check_session(store, error);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    mc_world_info_record *record = NULL;
    mc_world_info_error info_error;
    mc_world_info_status serialized = mc_world_info_record_create(info, &record, &info_error);
    if (serialized != MC_WORLD_INFO_OK) {
        mc_world_storage_status mapped =
            serialized == MC_WORLD_INFO_LIMIT               ? MC_WORLD_STORAGE_LIMIT
            : serialized == MC_WORLD_INFO_ALLOCATION_FAILED ? MC_WORLD_STORAGE_ALLOCATION_FAILED
            : serialized == MC_WORLD_INFO_INVALID_ARGUMENT  ? MC_WORLD_STORAGE_INVALID_ARGUMENT
            : serialized == MC_WORLD_INFO_CLOCK_FAILURE     ? MC_WORLD_STORAGE_IO_ERROR
            : serialized == MC_WORLD_INFO_INVALID_PHASE     ? MC_WORLD_STORAGE_UNSUPPORTED_STATE
                                                            : MC_WORLD_STORAGE_INVALID_DATA;
        return store_error(error, mapped,
                           info_error.field == NULL ? mc_world_info_status_string(serialized)
                                                    : info_error.field);
    }
    status = mc_world_store_write_info_record(store, info, record, error);
    mc_world_info_record_release(record);
    return status;
}

mc_world_storage_status mc_world_store_write_info_record(mc_world_store *store, mc_world_info *info,
                                                         const mc_world_info_record *record,
                                                         mc_world_storage_error *error) {
    if (store != NULL && store->source != NULL)
        return store_source_legacy(error);
    if (store == NULL || mc_world_info_live_get(info) == NULL ||
        mc_world_info_record_root(record) == NULL)
        return store_error(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "metadata record transport");
    mc_nbt_document *document = NULL;
    mc_level_error detail;
    mc_level_status built = mc_level_build_record(mc_world_info_record_root(record),
                                                  mc_world_store_metadata_root(store),
                                                  &store->limits.nbt, &document, &detail);
    if (built != MC_LEVEL_OK)
        return store_level_error(error, built, &detail);
    /* Rebind only the persistence projection. Reading Dimension here would add
     * a schema failure absent from the actual serializer. */
    static const uint16_t data_key[] = {'D', 'a', 't', 'a'};
    static const uint16_t name_key[] = {'L', 'e', 'v', 'e', 'l', 'N', 'a', 'm', 'e'};
    static const uint16_t player_key[] = {'P', 'l', 'a', 'y', 'e', 'r'};
    const mc_nbt_tag *data = mc_nbt_compound_get(mc_nbt_document_root(document), data_key, 4);
    const mc_nbt_tag *name = mc_nbt_compound_get(data, name_key, 9);
    mc_world_info_fields fields;
    (void)mc_world_info_fields_get(info, &fields);
    mc_level_info rebound = {.seed = fields.value.seed,
                             .spawn_x = fields.value.spawn_x,
                             .spawn_y = fields.value.spawn_y,
                             .spawn_z = fields.value.spawn_z,
                             .time = fields.value.time,
                             .last_played = fields.value.last_played,
                             .size_on_disk = fields.value.size_on_disk,
                             .level_name = name->value.string,
                             .version = fields.value.version,
                             .rain_time = fields.value.rain_time,
                             .thunder_time = fields.value.thunder_time,
                             .raining = fields.value.raining,
                             .thundering = fields.value.thundering,
                             .player = mc_nbt_compound_get(data, player_key, 6),
                             .player_dimension = fields.value.player_dimension};
    mc_world_storage_status status =
        store_write_metadata(store, mc_nbt_document_root(document), error);
    if (status != MC_WORLD_STORAGE_OK) {
        mc_nbt_document_free(document);
        return status;
    }
    mc_nbt_document *previous = store->metadata;
    store->metadata = document;
    store->info = rebound;
    store->has_info = true;
    mc_nbt_document_free(previous);
    return store_error(error, MC_WORLD_STORAGE_OK, NULL);
}

static mc_world_storage_status store_dimension(const mc_world_store *store, int32_t dimension,
                                               const char **out, mc_world_storage_error *error) {
    if (store->source != NULL) {
        if (dimension != 0 && dimension != -1)
            return store_error(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                               "source native chunk dimension");
        *out = NULL; /* Native bookkeeping never invents a legacy path. */
        return MC_WORLD_STORAGE_OK;
    }
    if (dimension == 0)
        *out = store->root;
    else if (dimension == -1)
        *out = store->nether;
    else
        return store_error(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "world dimension");
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status store_new_chunk(mc_world_store *store, store_chunk **out,
                                               mc_world_storage_error *error) {
    if (store->chunk_count >= store->limits.max_loaded_chunks ||
        store->reservation_count >= store->limits.max_loaded_chunks - store->chunk_count)
        return store_error(error, MC_WORLD_STORAGE_LIMIT, "retained chunk count");
    store_chunk *entry = calloc(1, sizeof(*entry));
    if (entry == NULL)
        return store_error(error, MC_WORLD_STORAGE_ALLOCATION_FAILED, "retained chunk link");
    /* Reservations and old copy/load routes always begin as the stored variant. */
    *entry = (store_chunk){
        .kind = STORE_CHUNK_STORED, .owner.stored = NULL, .phase = MC_WORLD_STORE_GAME_COMMITTED};
    *out = entry;
    return MC_WORLD_STORAGE_OK;
}

static store_chunk *store_find_chunk(const mc_world_store *store, const mc_chunk_state *chunk) {
    if (store == NULL || chunk == NULL)
        return NULL;
    store_chunk *entry = store->chunks;
    while (entry != NULL && store_chunk_state(entry) != chunk)
        entry = entry->next;
    return entry;
}

static void store_publish_chunk(mc_world_store *store, store_chunk *entry, int32_t dimension) {
    /* Canonical native ownership attachment, including pending/failed stages.
     * This is not source cache/list publication, activation or loader success. */
    entry->dimension = dimension;
    if (store->last_chunk == NULL)
        store->chunks = entry;
    else
        store->last_chunk->next = entry;
    store->last_chunk = entry;
    ++store->chunk_count;
}

mc_world_storage_status mc_world_store_create_generated(mc_world_store *store,
                                                        mc_world_runtime *world, int32_t dimension,
                                                        int32_t x, int32_t z, uint8_t **blocks,
                                                        size_t length, mc_world_chunk **out,
                                                        mc_world_storage_error *error) {
    if (store != NULL && store->source != NULL) {
        mc_world_storage_status admission = store_source_native_admit(store, error);
        if (admission != MC_WORLD_STORAGE_OK)
            return admission;
    }
    if (store == NULL || world == NULL || blocks == NULL || *blocks == NULL ||
        length != MC_CHUNK_BLOCKS || out == NULL)
        return store_error(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "generated chunk arguments");
    const char *root = NULL;
    mc_world_storage_status status = store_dimension(store, dimension, &root, error);
    (void)root;
    store_chunk *entry = NULL;
    if (status == MC_WORLD_STORAGE_OK)
        status = store_new_chunk(store, &entry, error);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    entry->kind = STORE_CHUNK_GENERATED;
    entry->owner.generated = NULL;
    status = mc_world_chunk_storage_create(world, x, z, blocks, length,
                                           store->limits.nbt.max_allocation_bytes,
                                           &entry->owner.generated, error);
    if (status != MC_WORLD_STORAGE_OK) {
        store_chunk_free(entry);
        free(entry);
        return status;
    }
    /* No fallible step remains after the lower constructor adopts the blocks. */
    store_publish_chunk(store, entry, dimension);
    *out = entry->owner.generated;
    return store_error(error, MC_WORLD_STORAGE_OK, NULL);
}

mc_world_chunk *mc_world_store_generated_chunk(const mc_world_store *store,
                                               const mc_chunk_state *state) {
    const store_chunk *entry = store_find_chunk(store, state);
    return entry != NULL && entry->kind == STORE_CHUNK_GENERATED ? entry->owner.generated : NULL;
}

mc_world_chunk *mc_world_store_chunk_object(const mc_world_store *store,
                                            const mc_chunk_state *state) {
    const store_chunk *entry = store_find_chunk(store, state);
    return entry == NULL                          ? NULL
           : entry->kind == STORE_CHUNK_GENERATED ? entry->owner.generated
                                                  : entry->loaded_binding;
}

bool mc_world_store_has_chunks_for_world(const mc_world_store *store,
                                         const mc_world_runtime *world) {
    if (store == NULL || world == NULL)
        return false;
    for (const store_chunk *entry = store->chunks; entry != NULL; entry = entry->next) {
        mc_world_chunk *chunk =
            entry->kind == STORE_CHUNK_GENERATED ? entry->owner.generated : entry->loaded_binding;
        mc_world_chunk_fields fields;
        if (mc_world_chunk_fields_get(chunk, &fields) == MC_WORLD_STORAGE_OK && fields.d == world)
            return true;
    }
    return false;
}

bool mc_world_store_has_generated_for_world(const mc_world_store *store,
                                            const mc_world_runtime *world) {
    if (store == NULL || world == NULL)
        return false;
    for (const store_chunk *entry = store->chunks; entry != NULL; entry = entry->next) {
        if (entry->kind == STORE_CHUNK_GENERATED) {
            mc_world_chunk_fields fields;
            if (mc_world_chunk_fields_get(entry->owner.generated, &fields) == MC_WORLD_STORAGE_OK &&
                fields.d == world)
                return true;
        }
    }
    return false;
}

static const mc_nbt_tag *store_get(const mc_nbt_tag *parent, const char *name) {
    uint16_t units[16];
    size_t length = strlen(name);
    for (size_t i = 0; i < length; ++i)
        units[i] = (uint16_t)(unsigned char)name[i];
    return mc_nbt_compound_get(parent, units, length);
}

static bool store_opaque(const store_chunk *entry) {
    const mc_nbt_tag *level = store_get(store_chunk_root(entry), "Level");
    const mc_nbt_tag *entities = store_get(level, "Entities");
    const mc_nbt_tag *tile_entities = store_get(level, "TileEntities");
    return (entities != NULL && entities->value.list.length != 0) ||
           (tile_entities != NULL && tile_entities->value.list.length != 0);
}

static mc_world_storage_status store_load_chunk(mc_world_store *store, int32_t dimension, int32_t x,
                                                int32_t z, bool activation, mc_world_runtime *world,
                                                mc_chunk_state **out,
                                                mc_world_storage_error *error) {
    if (store == NULL || out == NULL)
        return store_error(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "chunk load arguments");
    const char *root = NULL;
    mc_world_storage_status status = store_dimension(store, dimension, &root, error);
    store_chunk *entry = NULL;
    if (status == MC_WORLD_STORAGE_OK)
        status = store_new_chunk(store, &entry, error);
    mc_compression_buffer raw = {0};
    if (status == MC_WORLD_STORAGE_OK)
        status = mc_world_regions_read(store->regions, root, x, z, &raw, error);
    if (status == MC_WORLD_STORAGE_OK) {
        mc_stored_chunk_error detail;
        mc_stored_chunk_status decoded = mc_stored_chunk_decode(
            raw.data, raw.length, &store->limits.nbt, &entry->owner.stored, NULL, &detail);
        if (decoded != MC_STORED_CHUNK_OK)
            status = store_chunk_error(error, decoded, &detail);
    }
    mc_compression_buffer_free(&raw);
    if (status == MC_WORLD_STORAGE_OK && activation && store_opaque(entry))
        status = store_error(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                             "opaque entity or TileEntity activation");
    if (status == MC_WORLD_STORAGE_OK) {
        mc_stored_chunk_status normalized = mc_stored_chunk_normalize(
            entry->owner.stored, store->block_map, sizeof(store->block_map));
        if (normalized != MC_STORED_CHUNK_OK)
            status = store_error(error, MC_WORLD_STORAGE_INVALID_DATA, "postload normalization");
    }
    if (status == MC_WORLD_STORAGE_OK && world != NULL)
        status = mc_world_chunk_storage_bind_loaded(world, store_chunk_state(entry),
                                                    store->limits.nbt.max_allocation_bytes,
                                                    &entry->loaded_binding, error);
    if (status != MC_WORLD_STORAGE_OK) {
        if (entry != NULL)
            store_chunk_free(entry);
        free(entry);
        return status;
    }
    store_publish_chunk(store, entry, dimension);
    *out = store_chunk_state(entry);
    return store_error(error, MC_WORLD_STORAGE_OK, NULL);
}

mc_world_storage_status mc_world_store_load_chunk(mc_world_store *store, int32_t dimension,
                                                  int32_t x, int32_t z, mc_chunk_state **out,
                                                  mc_world_storage_error *error) {
    if (store != NULL && store->source != NULL)
        return store_source_legacy(error);
    return store_load_chunk(store, dimension, x, z, false, NULL, out, error);
}

mc_world_storage_status mc_world_store_load_game_chunk(mc_world_store *store,
                                                       mc_world_runtime *world, int32_t dimension,
                                                       int32_t x, int32_t z, mc_chunk_state **out,
                                                       mc_world_storage_error *error) {
    if (store != NULL && store->source != NULL)
        return store_source_legacy(error);
    if (world == NULL)
        return store_error(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "GAME load World receiver");
    return store_load_chunk(store, dimension, x, z, true, world, out, error);
}

mc_world_storage_status mc_world_store_retain_chunk(mc_world_store *store, int32_t dimension,
                                                    const mc_chunk_state *chunk,
                                                    mc_chunk_state **out,
                                                    mc_world_storage_error *error) {
    if (store != NULL && store->source != NULL) {
        mc_world_storage_status admission = store_source_native_admit(store, error);
        if (admission != MC_WORLD_STORAGE_OK)
            return admission;
    }
    if (store == NULL || chunk == NULL || out == NULL)
        return store_error(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "chunk retain arguments");
    const char *root = NULL;
    mc_world_storage_status status = store_dimension(store, dimension, &root, error);
    (void)root;
    store_chunk *entry = NULL;
    if (status == MC_WORLD_STORAGE_OK)
        status = store_new_chunk(store, &entry, error);
    if (status == MC_WORLD_STORAGE_OK) {
        mc_stored_chunk_error detail;
        mc_stored_chunk_status copied =
            mc_stored_chunk_create(chunk, &store->limits.nbt, &entry->owner.stored, &detail);
        if (copied != MC_STORED_CHUNK_OK)
            status = store_chunk_error(error, copied, &detail);
    }
    if (status != MC_WORLD_STORAGE_OK) {
        if (entry != NULL)
            store_chunk_free(entry);
        free(entry);
        return status;
    }
    store_publish_chunk(store, entry, dimension);
    *out = store_chunk_state(entry);
    return store_error(error, MC_WORLD_STORAGE_OK, NULL);
}

static mc_world_storage_status store_write_chunk(mc_world_store *store, mc_chunk_state *chunk,
                                                 int64_t world_time, uint32_t timestamp,
                                                 uint32_t *growth, bool runtime,
                                                 mc_world_info *account,
                                                 mc_world_storage_error *error) {
    if (store == NULL || chunk == NULL || growth == NULL)
        return store_error(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "chunk write arguments");
    store_chunk *entry = store_find_chunk(store, chunk);
    if (entry == NULL)
        return store_error(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "foreign chunk identity");
    if (entry->game_save_open)
        return store_error(error, MC_WORLD_STORAGE_BUSY, "GAME save already open");
    if (entry->phase != MC_WORLD_STORE_GAME_COMMITTED)
        return store_error(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                           "uncommitted GAME stage cannot serialize");
    mc_world_chunk *runtime_chunk =
        entry->kind == STORE_CHUNK_GENERATED ? entry->owner.generated : entry->loaded_binding;
    if (mc_world_chunk_entity_containers_retired(runtime_chunk))
        return store_error(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                           "retired Chunk containers cannot serialize");
    if (account == NULL && !store->has_info)
        return store_error(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                           "chunk write requires metadata");
    mc_world_storage_status status = mc_world_store_check_session(store, error);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    if (runtime) {
        if (store_opaque(entry) ||
            (runtime_chunk != NULL && !mc_world_chunk_storage_empty(runtime_chunk)))
            return store_error(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                               "opaque entity or TileEntity runtime save");
        chunk->has_entities = false;
    }
    mc_nbt_document *document = NULL;
    mc_stored_chunk_error detail;
    mc_stored_chunk_status built =
        store_chunk_build(entry, world_time, &store->limits.nbt, &document, &detail);
    if (built != MC_STORED_CHUNK_OK)
        return store_chunk_error(error, built, &detail);
    mc_ws_file_bytes raw = {0};
    status =
        store_encode_nbt(mc_nbt_document_root(document), &store->limits.nbt, &raw, error, NULL);
    uint32_t appended = 0;
    const char *root = entry->dimension == 0 ? store->root : store->nether;
    if (status == MC_WORLD_STORAGE_OK)
        status = mc_world_regions_write(store->regions, root, chunk->x, chunk->z, raw.data,
                                        raw.length, timestamp, &appended, error);
    mc_ws_free_file_bytes(&raw);
    mc_nbt_document_free(document);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    int64_t delta = (int64_t)mc_java_i32_narrow((int64_t)appended);
    if (account == NULL) {
        store->info.size_on_disk = mc_java_i64_add(store->info.size_on_disk, delta);
    } else {
        mc_world_info_fields fields;
        (void)mc_world_info_fields_get(account, &fields);
        (void)mc_world_info_size_set(account, mc_java_i64_add(fields.value.size_on_disk, delta));
    }
    *growth = appended;
    return store_error(error, MC_WORLD_STORAGE_OK, NULL);
}

mc_world_storage_status mc_world_store_write_chunk(mc_world_store *store, mc_chunk_state *chunk,
                                                   int64_t world_time, uint32_t timestamp,
                                                   uint32_t *growth,
                                                   mc_world_storage_error *error) {
    if (store != NULL && store->source != NULL)
        return store_source_legacy(error);
    return store_write_chunk(store, chunk, world_time, timestamp, growth, false, NULL, error);
}

mc_world_storage_status mc_world_store_write_runtime_chunk(mc_world_store *store,
                                                           mc_chunk_state *chunk,
                                                           int64_t world_time, uint32_t timestamp,
                                                           uint32_t *growth,
                                                           mc_world_storage_error *error) {
    if (store != NULL && store->source != NULL)
        return store_source_legacy(error);
    return store_write_chunk(store, chunk, world_time, timestamp, growth, true, NULL, error);
}

mc_world_storage_status mc_world_store_write_runtime_chunk_for_info(
    mc_world_store *store, mc_chunk_state *chunk, mc_world_info *info, int64_t world_time,
    uint32_t timestamp, uint32_t *growth, mc_world_storage_error *error) {
    if (store != NULL && store->source != NULL)
        return store_source_legacy(error);
    if (info == NULL)
        return store_error(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "chunk WorldInfo argument");
    if (mc_world_info_live_get(info) == NULL)
        return store_error(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                           "chunk requires READY WorldInfo");
    return store_write_chunk(store, chunk, world_time, timestamp, growth, true, info, error);
}

size_t mc_world_store_chunk_count(const mc_world_store *store) {
    return store == NULL ? 0 : store->chunk_count;
}

mc_world_storage_status mc_world_store_chunk_at(const mc_world_store *store, size_t ordinal,
                                                mc_chunk_state **out,
                                                mc_world_storage_error *error) {
    if (store != NULL && store->source != NULL) {
        mc_world_storage_status admission = store_source_native_admit(store, error);
        if (admission != MC_WORLD_STORAGE_OK)
            return admission;
    }
    if (store == NULL || out == NULL)
        return store_error(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "chunk ordinal arguments");
    for (const store_chunk *entry = store->chunks; entry != NULL; entry = entry->next) {
        if (ordinal != 0) {
            --ordinal;
            continue;
        }
        mc_chunk_state *state = store_chunk_state(entry);
        if (state == NULL)
            return store_error(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "chunk ownership");
        *out = state;
        return store_error(error, MC_WORLD_STORAGE_OK, NULL);
    }
    return store_error(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "chunk ordinal");
}

bool mc_world_store_owns_chunk(const mc_world_store *store, const mc_chunk_state *chunk) {
    return store_find_chunk(store, chunk) != NULL;
}

mc_world_storage_status mc_world_store_lease_chunk(mc_world_store *store,
                                                   const mc_chunk_state *chunk,
                                                   mc_world_storage_error *error) {
    if (store != NULL && store->source != NULL) {
        mc_world_storage_status admission = store_source_native_admit(store, error);
        if (admission != MC_WORLD_STORAGE_OK)
            return admission;
    }
    store_chunk *entry = store_find_chunk(store, chunk);
    if (entry == NULL)
        return store_error(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "foreign chunk identity");
    if (entry->leases == SIZE_MAX)
        return store_error(error, MC_WORLD_STORAGE_LIMIT, "chunk lease count");
    ++entry->leases;
    return store_error(error, MC_WORLD_STORAGE_OK, NULL);
}

mc_world_storage_status mc_world_store_release_chunk(mc_world_store *store,
                                                     const mc_chunk_state *chunk,
                                                     mc_world_storage_error *error) {
    if (store != NULL && store->source != NULL) {
        mc_world_storage_status admission = store_source_native_admit(store, error);
        if (admission != MC_WORLD_STORAGE_OK)
            return admission;
    }
    store_chunk *entry = store_find_chunk(store, chunk);
    if (entry == NULL)
        return store_error(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "foreign chunk identity");
    if (entry->leases == 0)
        return store_error(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "unbalanced chunk lease");
    --entry->leases;
    return store_error(error, MC_WORLD_STORAGE_OK, NULL);
}

static bool store_cache_valid(const mc_chunk_cache *cache, mc_chunk_state *const *fallback) {
    if (cache == NULL || cache->capacity > SIZE_MAX / sizeof(*cache->entries) ||
        cache->insertion_capacity > SIZE_MAX / sizeof(*cache->insertions) ||
        !mc_chunk_cache_validate(cache))
        return false;
    return fallback == NULL ||
           (*fallback != NULL && ((*fallback)->blank || (*fallback)->data != NULL));
}

mc_world_storage_status mc_world_store_register_cache(mc_world_store *store,
                                                      const mc_chunk_cache *cache,
                                                      mc_chunk_state *const *fallback,
                                                      mc_world_storage_error *error) {
    if (store != NULL && store->source != NULL) {
        mc_world_storage_status admission = store_source_native_admit(store, error);
        if (admission != MC_WORLD_STORAGE_OK)
            return admission;
    }
    if (store == NULL || !store_cache_valid(cache, fallback))
        return store_error(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "registered cache metadata");
    for (store_cache *entry = store->caches; entry != NULL; entry = entry->next) {
        if (entry->cache == cache) {
            if (entry->fallback != fallback)
                return store_error(error, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                                   "different registered fallback slot");
            return store_error(error, MC_WORLD_STORAGE_OK, NULL);
        }
    }
    if (store->cache_count == MC_WORLD_STORE_MAX_REGISTERED_CACHES)
        return store_error(error, MC_WORLD_STORAGE_LIMIT, "registered cache count");
    store_cache *entry = malloc(sizeof(*entry));
    if (entry == NULL)
        return store_error(error, MC_WORLD_STORAGE_ALLOCATION_FAILED, "registered cache link");
    *entry = (store_cache){cache, fallback, store->caches};
    store->caches = entry;
    ++store->cache_count;
    return store_error(error, MC_WORLD_STORAGE_OK, NULL);
}

mc_world_storage_status mc_world_store_unregister_cache(mc_world_store *store,
                                                        const mc_chunk_cache *cache,
                                                        mc_world_storage_error *error) {
    if (store != NULL && store->source != NULL) {
        mc_world_storage_status admission = store_source_native_admit(store, error);
        if (admission != MC_WORLD_STORAGE_OK)
            return admission;
    }
    if (store == NULL || cache == NULL)
        return store_error(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "cache unregister arguments");
    store_cache **link = &store->caches;
    while (*link != NULL && (*link)->cache != cache)
        link = &(*link)->next;
    if (*link == NULL)
        return store_error(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "unregistered cache");
    store_cache *entry = *link;
    *link = entry->next;
    --store->cache_count;
    free(entry);
    return store_error(error, MC_WORLD_STORAGE_OK, NULL);
}

mc_world_storage_status mc_world_store_retire_chunk(mc_world_store *store, mc_chunk_state *chunk,
                                                    mc_world_storage_error *error) {
    if (store != NULL && store->source != NULL) {
        mc_world_storage_status admission = store_source_native_admit(store, error);
        if (admission != MC_WORLD_STORAGE_OK)
            return admission;
    }
    if (store == NULL || chunk == NULL)
        return store_error(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "chunk retire arguments");
    store_chunk **link = &store->chunks;
    store_chunk *previous = NULL;
    while (*link != NULL && store_chunk_state(*link) != chunk) {
        previous = *link;
        link = &(*link)->next;
    }
    if (*link == NULL)
        return store_error(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "foreign chunk identity");
    bool referenced = false;
    for (store_cache *entry = store->caches; entry != NULL; entry = entry->next) {
        if (!store_cache_valid(entry->cache, entry->fallback))
            return store_error(error, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                               "registered cache metadata");
        for (size_t i = 0; i < entry->cache->count; ++i)
            referenced = referenced || entry->cache->entries[i].chunk == chunk;
        for (size_t i = 0; i < entry->cache->insertion_count; ++i)
            referenced = referenced || entry->cache->insertions[i] == chunk;
        if (entry->fallback != NULL)
            referenced = referenced || *entry->fallback == chunk;
    }
    store_chunk *entry = *link;
    const mc_world_chunk *runtime_chunk =
        entry->kind == STORE_CHUNK_GENERATED ? entry->owner.generated : entry->loaded_binding;
    if (entry->leases != 0 || entry->coordinator_lease || entry->game_save_open || referenced ||
        (entry->imports_started && entry->phase != MC_WORLD_STORE_GAME_COMMITTED &&
         !mc_world_chunk_entity_containers_retired(runtime_chunk)) ||
        (chunk->active && !mc_world_chunk_entity_containers_retired(runtime_chunk)))
        return store_error(error, MC_WORLD_STORAGE_BUSY, "chunk identity still referenced");
    *link = entry->next;
    if (store->last_chunk == entry)
        store->last_chunk = previous;
    --store->chunk_count;
    store_chunk_free(entry);
    free(entry);
    return store_error(error, MC_WORLD_STORAGE_OK, NULL);
}

static mc_world_store_reservation **
store_find_reservation(mc_world_store *store, mc_world_store_reservation *reservation) {
    mc_world_store_reservation **link = &store->reservations;
    while (*link != NULL && *link != reservation)
        link = &(*link)->next;
    return link;
}

mc_world_storage_status mc_world_store_reserve_chunk(mc_world_store *store, int32_t dimension,
                                                     mc_world_store_reservation **out,
                                                     mc_world_storage_error *error) {
    if (store != NULL && store->source != NULL) {
        mc_world_storage_status admission = store_source_native_admit(store, error);
        if (admission != MC_WORLD_STORAGE_OK)
            return admission;
    }
    if (store == NULL || out == NULL)
        return store_error(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "chunk reservation arguments");
    const char *root = NULL;
    mc_world_storage_status status = store_dimension(store, dimension, &root, error);
    (void)root;
    store_chunk *entry = NULL;
    if (status == MC_WORLD_STORAGE_OK)
        status = store_new_chunk(store, &entry, error);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    mc_world_store_reservation *reservation = malloc(sizeof(*reservation));
    if (reservation == NULL) {
        store_chunk_free(entry);
        free(entry);
        return store_error(error, MC_WORLD_STORAGE_ALLOCATION_FAILED, "chunk reservation token");
    }
    entry->dimension = dimension;
    *reservation = (mc_world_store_reservation){entry, store->reservations};
    store->reservations = reservation;
    ++store->reservation_count;
    *out = reservation;
    return store_error(error, MC_WORLD_STORAGE_OK, NULL);
}

mc_world_storage_status mc_world_store_cancel_reservation(mc_world_store *store,
                                                          mc_world_store_reservation *reservation,
                                                          mc_world_storage_error *error) {
    if (store != NULL && store->source != NULL) {
        mc_world_storage_status admission = store_source_native_admit(store, error);
        if (admission != MC_WORLD_STORAGE_OK)
            return admission;
    }
    if (store == NULL || reservation == NULL)
        return store_error(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "chunk reservation arguments");
    mc_world_store_reservation **link = store_find_reservation(store, reservation);
    if (*link == NULL)
        return store_error(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "foreign chunk reservation");
    *link = reservation->next;
    --store->reservation_count;
    store_chunk_free(reservation->entry);
    free(reservation->entry);
    free(reservation);
    return store_error(error, MC_WORLD_STORAGE_OK, NULL);
}

mc_world_storage_status mc_world_store_retain_reserved(mc_world_store *store,
                                                       mc_world_store_reservation *reservation,
                                                       const mc_chunk_state *chunk,
                                                       mc_chunk_state **out,
                                                       mc_world_storage_error *error) {
    if (store != NULL && store->source != NULL) {
        mc_world_storage_status admission = store_source_native_admit(store, error);
        if (admission != MC_WORLD_STORAGE_OK)
            return admission;
    }
    if (store == NULL || reservation == NULL || chunk == NULL || out == NULL)
        return store_error(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "reserved retain arguments");
    mc_world_store_reservation **link = store_find_reservation(store, reservation);
    if (*link == NULL)
        return store_error(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "foreign chunk reservation");
    store_chunk *entry = reservation->entry;
    mc_stored_chunk_error detail;
    mc_stored_chunk_status copied =
        mc_stored_chunk_create(chunk, &store->limits.nbt, &entry->owner.stored, &detail);
    if (copied != MC_STORED_CHUNK_OK)
        return store_chunk_error(error, copied, &detail);
    *link = reservation->next;
    --store->reservation_count;
    store_publish_chunk(store, entry, entry->dimension);
    *out = store_chunk_state(entry);
    free(reservation);
    return store_error(error, MC_WORLD_STORAGE_OK, NULL);
}

size_t mc_world_store_reservation_count(const mc_world_store *store) {
    return store == NULL ? 0 : store->reservation_count;
}

static store_chunk *store_find_game_stage(const mc_world_store *store,
                                          const mc_world_store_game_stage *stage) {
    if (store == NULL || stage == NULL)
        return NULL;
    for (store_chunk *entry = store->chunks; entry != NULL; entry = entry->next)
        if (entry->game_stage && &entry->stage == stage && entry->stage.entry == entry)
            return entry;
    return NULL;
}

static bool store_chunk_entity_journal_valid(const store_chunk *entry) {
    if (entry->entity_consumed == NULL)
        return entry->entity_source_root == NULL && entry->entity_source_list == NULL &&
               entry->entity_source_count == 0 && entry->entity_processed == 0;
    if (!entry->game_stage || !entry->imports_started || entry->kind != STORE_CHUNK_STORED ||
        entry->owner.stored == NULL || entry->entity_source_root == NULL ||
        entry->entity_source_list == NULL || entry->entity_source_count == 0 ||
        entry->entity_source_count > SIZE_MAX / sizeof(*entry->entity_consumed) ||
        entry->entity_processed == 0 || entry->entity_processed > entry->entity_source_count ||
        entry->entity_source_root != store_chunk_root(entry))
        return false;
    const mc_nbt_tag *level = store_get(entry->entity_source_root, "Level");
    const mc_nbt_tag *records = store_get(level, "Entities");
    return level != NULL && level->type == MC_NBT_COMPOUND &&
           records == entry->entity_source_list && records->type == MC_NBT_LIST &&
           records->value.list.items != NULL &&
           records->value.list.length == entry->entity_source_count;
}

static bool store_chunk_entity_journal_complete(const store_chunk *entry) {
    if (!store_chunk_entity_journal_valid(entry))
        return false;
    if (entry->entity_consumed != NULL)
        return entry->entity_processed == entry->entity_source_count;
    /* No source method, allocation, q write or hidden empty factory occurs.
     * Inspect only the immutable document already owned by this entry. */
    const mc_nbt_tag *records = store_get(store_get(store_chunk_root(entry), "Level"), "Entities");
    return records == NULL || (records->type == MC_NBT_LIST && records->value.list.length == 0);
}

static bool store_chunk_phase_valid(const store_chunk *entry) {
    if (!store_chunk_entity_journal_valid(entry))
        return false;
    if (!entry->game_stage)
        return entry->phase == MC_WORLD_STORE_GAME_COMMITTED && !entry->imports_started &&
               !entry->coordinator_lease;
    if (entry->source_header_failed)
        return entry->kind == STORE_CHUNK_STORED && entry->owner.stored != NULL &&
               entry->loaded_binding == NULL && entry->stage.entry == entry &&
               entry->phase == MC_WORLD_STORE_GAME_FAILED && !entry->imports_started &&
               !entry->coordinator_lease && entry->leases == 0;
    if (entry->kind != STORE_CHUNK_STORED || entry->owner.stored == NULL ||
        entry->loaded_binding == NULL || entry->stage.entry != entry)
        return false;
    switch (entry->phase) {
        case MC_WORLD_STORE_GAME_PREPARED:
            return !entry->imports_started && entry->coordinator_lease && entry->leases != 0 &&
                   entry->entity_consumed == NULL;
        case MC_WORLD_STORE_GAME_IMPORTING:
        case MC_WORLD_STORE_GAME_NORMALIZED:
            return entry->imports_started && entry->coordinator_lease && entry->leases != 0;
        case MC_WORLD_STORE_GAME_COMMITTED:
            return entry->imports_started && !entry->coordinator_lease &&
                   store_chunk_entity_journal_complete(entry);
        case MC_WORLD_STORE_GAME_FAILED:
            return !entry->coordinator_lease;
    }
    return false;
}

static mc_world_chunk *store_runtime_chunk(const store_chunk *entry) {
    return entry->kind == STORE_CHUNK_GENERATED ? entry->owner.generated : entry->loaded_binding;
}

static bool store_chunk_save_valid(const store_chunk *entry) {
    if (!entry->game_save_open)
        return entry->save.entry == NULL && entry->save.world == NULL &&
               entry->save.account == NULL;
    return entry->phase == MC_WORLD_STORE_GAME_COMMITTED && !entry->coordinator_lease &&
           entry->leases != 0 && entry->save.entry == entry && entry->save.world != NULL &&
           (entry->save.account == NULL || mc_world_info_live_get(entry->save.account) != NULL);
}

static bool store_chunk_owner_valid(const store_chunk *entry) {
    if (entry->kind == STORE_CHUNK_STORED) {
        if (!mc_stored_chunk_storage_valid(entry->owner.stored))
            return false;
    } else if (entry->kind != STORE_CHUNK_GENERATED || entry->owner.generated == NULL) {
        return false;
    }
    mc_chunk_state *state = store_chunk_state(entry);
    if (state == NULL || state->blank || state->data == NULL || state->data->blocks == NULL ||
        state->data->blocks_capacity != (size_t)MC_CHUNK_BLOCKS ||
        !store_chunk_phase_valid(entry) || !store_chunk_save_valid(entry))
        return false;
    mc_world_chunk *chunk = store_runtime_chunk(entry);
    if (chunk == NULL)
        return entry->kind == STORE_CHUNK_STORED &&
               (!entry->game_stage || entry->source_header_failed) && !entry->game_save_open;
    if (mc_world_chunk_state(chunk) != state || !mc_world_chunk_entities_validate(chunk))
        return false;
    mc_world_chunk_fields fields;
    return mc_world_chunk_fields_get(chunk, &fields) == MC_WORLD_STORAGE_OK &&
           fields.b == state->data->blocks &&
           (!entry->game_save_open ||
            (fields.d == entry->save.world && mc_world_chunk_source_enabled(chunk)));
}

static store_chunk *store_find_game_save(const mc_world_store *store,
                                         const mc_world_store_game_save *save) {
    if (store == NULL || save == NULL)
        return NULL;
    for (store_chunk *entry = store->chunks; entry != NULL; entry = entry->next)
        if (entry->game_save_open && &entry->save == save && entry->save.entry == entry)
            return entry;
    return NULL;
}

mc_world_storage_status
mc_world_store_game_save_begin(mc_world_store *store, mc_world_runtime *world,
                               mc_chunk_state *state, mc_world_info *account,
                               mc_world_store_game_save **out, mc_world_store_game_save_view *view,
                               mc_world_storage_error *error) {
    if (store != NULL && store->source != NULL)
        return store_source_save_frontier(store, error);
    if (store == NULL || world == NULL || state == NULL || out == NULL || view == NULL)
        return store_error(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "GAME save arguments");
    store_chunk *entry = store_find_chunk(store, state);
    if (entry == NULL)
        return store_error(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "foreign chunk identity");
    if (entry->game_save_open)
        return store_error(error, MC_WORLD_STORAGE_BUSY, "GAME save already open");
    if (!store_chunk_owner_valid(entry))
        return store_error(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                           "GAME save Chunk ownership metadata");
    mc_world_chunk *chunk = store_runtime_chunk(entry);
    if (entry->phase != MC_WORLD_STORE_GAME_COMMITTED || chunk == NULL ||
        !mc_world_chunk_source_enabled(chunk))
        return store_error(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                           "GAME save requires committed source owner");
    mc_world_chunk_fields fields;
    (void)mc_world_chunk_fields_get(chunk, &fields);
    if (fields.d != world)
        return store_error(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "GAME save foreign World");
    if ((account == NULL && !store->has_info) ||
        (account != NULL && mc_world_info_live_get(account) == NULL))
        return store_error(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "GAME save metadata owner");
    mc_world_storage_status status = mc_world_store_lease_chunk(store, state, error);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    status = mc_world_store_check_session(store, error);
    if (status != MC_WORLD_STORAGE_OK) {
        (void)mc_world_store_release_chunk(store, state, NULL);
        return status;
    }
    entry->save = (mc_world_store_game_save){entry, world, account};
    entry->game_save_open = true;
    const mc_world_store_game_save_view snapshot = {chunk, state, store->limits.nbt};
    *out = &entry->save;
    *view = snapshot;
    return store_error(error, MC_WORLD_STORAGE_OK, NULL);
}

mc_world_storage_status mc_world_store_game_save_write(
    mc_world_store *store, mc_world_store_game_save *save, int64_t world_time, uint32_t timestamp,
    const mc_nbt_tree *entity_records, const mc_nbt_tree *tile_entity_records, uint32_t *growth,
    mc_world_storage_error *error, mc_stored_chunk_error *composition_error,
    mc_nbt_error *encoding_error) {
    if (store != NULL && store->source != NULL)
        return store_source_legacy(error);
    if (composition_error != NULL)
        *composition_error = (mc_stored_chunk_error){MC_STORED_CHUNK_OK, NULL, {MC_NBT_OK, 0}};
    if (encoding_error != NULL)
        *encoding_error = (mc_nbt_error){MC_NBT_OK, 0};
    if (growth == NULL || entity_records == NULL || tile_entity_records == NULL)
        return store_error(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "GAME save write arguments");
    store_chunk *entry = store_find_game_save(store, save);
    if (entry == NULL)
        return store_error(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "foreign GAME save");
    if (!store_chunk_owner_valid(entry))
        return store_error(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                           "GAME save Chunk ownership metadata");
    mc_chunk_state *state = store_chunk_state(entry);
    if (!store_chunk_entity_journal_complete(entry))
        return store_error(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                           "GAME save incomplete Entity occurrence journal");
    const mc_nbt_tag *entity_root = mc_nbt_tree_root(entity_records);
    if (entity_root == NULL || entity_root->type != MC_NBT_COMPOUND ||
        entity_root->value.compound.length != 1)
        return store_error(error, MC_WORLD_STORAGE_INVALID_DATA, "GAME save Entity records root");
    const mc_nbt_tag *entities = store_get(entity_root, "Entities");
    if (entities == NULL || entities->type != MC_NBT_LIST ||
        entities->value.list.element_type != MC_NBT_COMPOUND)
        return store_error(error, MC_WORLD_STORAGE_INVALID_DATA, "GAME save Entities List");
    const mc_nbt_tag *records_root = mc_nbt_tree_root(tile_entity_records);
    if (records_root == NULL || records_root->type != MC_NBT_COMPOUND ||
        records_root->value.compound.length != 1)
        return store_error(error, MC_WORLD_STORAGE_INVALID_DATA,
                           "GAME save TileEntity records root");
    const mc_nbt_tag *records = store_get(records_root, "TileEntities");
    if (records == NULL || records->type != MC_NBT_LIST ||
        records->value.list.element_type != MC_NBT_COMPOUND)
        return store_error(error, MC_WORLD_STORAGE_INVALID_DATA, "GAME save TileEntities List");
    mc_nbt_limits remaining = store->limits.nbt;
    size_t input_bytes = mc_nbt_tree_allocation_bytes(entity_records);
    if (input_bytes > remaining.max_allocation_bytes)
        return store_error(error, MC_WORLD_STORAGE_LIMIT, "GAME save Entity input tree budget");
    remaining.max_allocation_bytes -= input_bytes;
    input_bytes = mc_nbt_tree_allocation_bytes(tile_entity_records);
    if (input_bytes > remaining.max_allocation_bytes)
        return store_error(error, MC_WORLD_STORAGE_LIMIT, "GAME save TileEntity input tree budget");
    remaining.max_allocation_bytes -= input_bytes;
    mc_nbt_tree *document = NULL;
    mc_stored_chunk_error detail;
    mc_stored_chunk_status built = mc_stored_chunk_build_game_records(
        entry->kind == STORE_CHUNK_STORED ? entry->owner.stored : NULL, state,
        store_runtime_chunk(entry), world_time, entities, records, entry->entity_source_list,
        entry->entity_consumed, entry->entity_source_count, &remaining, &document, &detail);
    if (composition_error != NULL)
        *composition_error = detail;
    if (built != MC_STORED_CHUNK_OK)
        return store_chunk_error(error, built, &detail);
    size_t document_bytes = mc_nbt_tree_allocation_bytes(document);
    if (document_bytes > remaining.max_allocation_bytes) {
        mc_nbt_tree_free(document);
        return store_error(error, MC_WORLD_STORAGE_LIMIT, "GAME save output tree budget");
    }
    remaining.max_allocation_bytes -= document_bytes;
    mc_ws_file_bytes raw = {0};
    mc_world_storage_status status =
        store_encode_nbt(mc_nbt_tree_root(document), &remaining, &raw, error, encoding_error);
    uint32_t appended = 0;
    const char *root = entry->dimension == 0 ? store->root : store->nether;
    if (status == MC_WORLD_STORAGE_OK)
        status = mc_world_regions_write(store->regions, root, state->x, state->z, raw.data,
                                        raw.length, timestamp, &appended, error);
    mc_ws_free_file_bytes(&raw);
    mc_nbt_tree_free(document);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    int64_t delta = (int64_t)mc_java_i32_narrow((int64_t)appended);
    if (save->account == NULL) {
        store->info.size_on_disk = mc_java_i64_add(store->info.size_on_disk, delta);
    } else {
        mc_world_info_fields fields;
        (void)mc_world_info_fields_get(save->account, &fields);
        (void)mc_world_info_size_set(save->account,
                                     mc_java_i64_add(fields.value.size_on_disk, delta));
    }
    *growth = appended;
    return store_error(error, MC_WORLD_STORAGE_OK, NULL);
}

mc_world_storage_status mc_world_store_game_save_end(mc_world_store *store,
                                                     mc_world_store_game_save *save,
                                                     mc_world_storage_error *error) {
    if (store != NULL && store->source != NULL)
        return store_source_legacy(error);
    store_chunk *entry = store_find_game_save(store, save);
    if (entry == NULL)
        return store_error(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "foreign GAME save");
    if (entry->leases == 0)
        return store_error(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "GAME save lease metadata");
    --entry->leases;
    entry->game_save_open = false;
    entry->save = (mc_world_store_game_save){0};
    return store_error(error, MC_WORLD_STORAGE_OK, NULL);
}

static mc_world_store_game_stage_view store_game_stage_view(const mc_world_store *store,
                                                            const store_chunk *entry) {
    return (mc_world_store_game_stage_view){.chunk = entry->loaded_binding,
                                            .state = store_chunk_state(entry),
                                            .level = store_get(store_chunk_root(entry), "Level"),
                                            .phase = entry->phase,
                                            .limits = store->limits.nbt};
}

mc_world_storage_status mc_world_store_game_stage_begin(mc_world_store *store,
                                                        mc_world_runtime *world, int32_t dimension,
                                                        int32_t x, int32_t z,
                                                        mc_world_store_game_stage **out,
                                                        mc_world_storage_error *error) {
    if (store != NULL && store->source != NULL)
        return store_source_legacy(error);
    if (store == NULL || world == NULL || out == NULL)
        return store_error(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "GAME stage arguments");
    const char *root = NULL;
    mc_world_storage_status status = store_dimension(store, dimension, &root, error);
    mc_world_store_reservation *reservation = NULL;
    if (status == MC_WORLD_STORAGE_OK)
        status = mc_world_store_reserve_chunk(store, dimension, &reservation, error);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    store_chunk *entry = reservation->entry;
    mc_compression_buffer raw = {0};
    status = mc_world_regions_read(store->regions, root, x, z, &raw, error);
    if (status == MC_WORLD_STORAGE_OK) {
        mc_stored_chunk_error detail;
        const mc_stored_chunk_status decoded = mc_stored_chunk_decode_game(
            raw.data, raw.length, &store->limits.nbt, &entry->owner.stored, NULL, &detail);
        if (decoded != MC_STORED_CHUNK_OK)
            status = store_chunk_error(error, decoded, &detail);
    }
    mc_compression_buffer_free(&raw);
    if (status == MC_WORLD_STORAGE_OK)
        status = mc_world_chunk_storage_bind_loaded(world, store_chunk_state(entry),
                                                    store->limits.nbt.max_allocation_bytes,
                                                    &entry->loaded_binding, error);
    if (status != MC_WORLD_STORAGE_OK) {
        /* No source import, callback, public handle or canonical attachment has
         * occurred. This private preparation alone is cancellable. */
        (void)mc_world_store_cancel_reservation(store, reservation, NULL);
        return status;
    }

    mc_world_store_reservation **link = store_find_reservation(store, reservation);
    *link = reservation->next;
    --store->reservation_count;
    free(reservation);
    entry->game_stage = true;
    entry->phase = MC_WORLD_STORE_GAME_PREPARED;
    entry->stage.entry = entry;
    store_publish_chunk(store, entry, dimension);
    status = mc_world_store_lease_chunk(store, store_chunk_state(entry), error);
    if (status != MC_WORLD_STORAGE_OK) {
        /* New entry/zero leases makes this unreachable under exclusive valid
         * ownership. Even an invariant failure keeps the attached owner. */
        entry->phase = MC_WORLD_STORE_GAME_FAILED;
        mc_world_chunk_source_disable(entry->loaded_binding);
        return status;
    }
    entry->coordinator_lease = true;
    *out = &entry->stage;
    return store_error(error, MC_WORLD_STORAGE_OK, NULL);
}

mc_world_storage_status mc_world_store_game_stage_view_get(const mc_world_store *store,
                                                           const mc_world_store_game_stage *stage,
                                                           mc_world_store_game_stage_view *out,
                                                           mc_world_storage_error *error) {
    if (store != NULL && store->source != NULL) {
        mc_world_storage_status admission = store_source_native_admit(store, error);
        if (admission != MC_WORLD_STORAGE_OK)
            return admission;
    }
    if (out == NULL)
        return store_error(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "GAME stage view output");
    const store_chunk *entry = store_find_game_stage(store, stage);
    if (entry == NULL)
        return store_error(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "foreign GAME stage");
    if (!store_chunk_phase_valid(entry))
        return store_error(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                           "GAME stage ownership metadata");
    *out = store_game_stage_view(store, entry);
    return store_error(error, MC_WORLD_STORAGE_OK, NULL);
}

mc_world_storage_status mc_world_store_game_stage_imports_begin(mc_world_store *store,
                                                                mc_world_store_game_stage *stage,
                                                                mc_world_storage_error *error) {
    if (store != NULL && store->source != NULL) {
        mc_world_storage_status admission = store_source_native_admit(store, error);
        if (admission != MC_WORLD_STORAGE_OK)
            return admission;
    }
    store_chunk *entry = store_find_game_stage(store, stage);
    if (entry == NULL)
        return store_error(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "foreign GAME stage");
    if (!store_chunk_phase_valid(entry) || entry->phase != MC_WORLD_STORE_GAME_PREPARED ||
        !mc_world_chunk_source_enabled(entry->loaded_binding))
        return store_error(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "GAME stage import phase");
    entry->imports_started = true;
    entry->phase = MC_WORLD_STORE_GAME_IMPORTING;
    return store_error(error, MC_WORLD_STORAGE_OK, NULL);
}

mc_world_storage_status
mc_world_store_game_stage_record_entity_result(mc_world_store *store,
                                               mc_world_store_game_stage *stage, size_t index,
                                               bool consumed, mc_world_storage_error *error) {
    if (store != NULL && store->source != NULL) {
        mc_world_storage_status admission = store_source_native_admit(store, error);
        if (admission != MC_WORLD_STORAGE_OK)
            return admission;
    }
    store_chunk *entry = store_find_game_stage(store, stage);
    if (entry == NULL)
        return store_error(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "foreign GAME stage");
    if (!store_chunk_owner_valid(entry) || entry->phase != MC_WORLD_STORE_GAME_IMPORTING ||
        !mc_world_chunk_source_enabled(entry->loaded_binding))
        return store_error(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                           "GAME Entity result recording phase");
    const mc_nbt_tag *root = store_chunk_root(entry);
    const mc_nbt_tag *records = store_get(store_get(root, "Level"), "Entities");
    if (records == NULL || records->type != MC_NBT_LIST || records->value.list.length == 0 ||
        records->value.list.items == NULL || index != entry->entity_processed ||
        index >= records->value.list.length)
        return store_error(error, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                           "GAME Entity result occurrence order");
    const mc_nbt_tag *record = records->value.list.items[index];
    if (record == NULL || record->type != MC_NBT_COMPOUND)
        return store_error(error, MC_WORLD_STORAGE_INVALID_DATA,
                           "GAME Entity result source Compound");
    if (entry->entity_consumed == NULL) {
        const size_t count = records->value.list.length;
        if (count > store->limits.nbt.max_elements ||
            count > SIZE_MAX / sizeof(*entry->entity_consumed) ||
            count * sizeof(*entry->entity_consumed) > store->limits.nbt.max_allocation_bytes)
            return store_error(error, MC_WORLD_STORAGE_LIMIT,
                               "GAME Entity occurrence journal budget");
        bool *results = malloc(count * sizeof(*results));
        if (results == NULL)
            return store_error(error, MC_WORLD_STORAGE_ALLOCATION_FAILED,
                               "GAME Entity occurrence journal");
        entry->entity_source_root = root;
        entry->entity_source_list = records;
        entry->entity_source_count = count;
        entry->entity_consumed = results;
    }
    entry->entity_consumed[index] = consumed;
    ++entry->entity_processed;
    return store_error(error, MC_WORLD_STORAGE_OK, NULL);
}

mc_world_storage_status mc_world_store_game_stage_finish(mc_world_store *store,
                                                         mc_world_store_game_stage *stage,
                                                         mc_chunk_state **out,
                                                         mc_world_storage_error *error) {
    if (store != NULL && store->source != NULL) {
        mc_world_storage_status admission = store_source_native_admit(store, error);
        if (admission != MC_WORLD_STORAGE_OK)
            return admission;
    }
    if (out == NULL)
        return store_error(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "GAME stage result output");
    store_chunk *entry = store_find_game_stage(store, stage);
    if (entry == NULL)
        return store_error(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "foreign GAME stage");
    if (!store_chunk_owner_valid(entry) || entry->phase != MC_WORLD_STORE_GAME_IMPORTING ||
        !mc_world_chunk_source_enabled(entry->loaded_binding))
        return store_error(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                           "GAME stage completion phase");
    if (!store_chunk_entity_journal_complete(entry))
        return store_error(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                           "GAME stage incomplete Entity occurrence journal");
    const mc_stored_chunk_status normalized =
        store->source != NULL ? store_source_normalize(store, entry, error)
                              : mc_stored_chunk_normalize(entry->owner.stored, store->block_map,
                                                          sizeof(store->block_map));
    if (normalized != MC_STORED_CHUNK_OK)
        return store_error(error, MC_WORLD_STORAGE_INVALID_DATA,
                           "GAME stage postload normalization");
    entry->phase = MC_WORLD_STORE_GAME_NORMALIZED;
    /* All admission precedes normalization. The actual lease is known held;
     * these exclusive owner updates cannot fail or call source code. */
    (void)mc_world_store_release_chunk(store, store_chunk_state(entry), NULL);
    entry->coordinator_lease = false;
    entry->phase = MC_WORLD_STORE_GAME_COMMITTED;
    *out = store_chunk_state(entry);
    return store_error(error, MC_WORLD_STORAGE_OK, NULL);
}

mc_world_storage_status mc_world_store_game_stage_fail(mc_world_store *store,
                                                       mc_world_store_game_stage *stage,
                                                       mc_world_storage_status original_status,
                                                       mc_world_storage_error *error) {
    if (store != NULL && store->source != NULL) {
        mc_world_storage_status admission = store_source_native_admit(store, error);
        if (admission != MC_WORLD_STORAGE_OK)
            return admission;
    }
    if (original_status == MC_WORLD_STORAGE_OK)
        return store_error(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "GAME stage failure status");
    store_chunk *entry = store_find_game_stage(store, stage);
    if (entry == NULL)
        return store_error(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "foreign GAME stage");
    if (!store_chunk_phase_valid(entry) || entry->phase == MC_WORLD_STORE_GAME_COMMITTED)
        return store_error(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "GAME stage failure phase");
    mc_world_chunk_source_disable(entry->loaded_binding);
    if (entry->phase != MC_WORLD_STORE_GAME_FAILED) {
        (void)mc_world_store_release_chunk(store, store_chunk_state(entry), NULL);
        entry->coordinator_lease = false;
        entry->phase = MC_WORLD_STORE_GAME_FAILED;
    }
    return store_error(error, original_status, "failed GAME stage retained");
}

size_t mc_world_store_game_stage_count(const mc_world_store *store) {
    size_t count = 0;
    if (store != NULL)
        for (const store_chunk *entry = store->chunks; entry != NULL; entry = entry->next)
            if (entry->game_stage && entry->phase != MC_WORLD_STORE_GAME_COMMITTED)
                ++count;
    return count;
}

mc_world_storage_status mc_world_store_game_stage_at(const mc_world_store *store, size_t ordinal,
                                                     mc_world_store_game_stage **out,
                                                     mc_world_store_game_stage_view *view,
                                                     mc_world_storage_error *error) {
    if (store != NULL && store->source != NULL) {
        mc_world_storage_status admission = store_source_native_admit(store, error);
        if (admission != MC_WORLD_STORAGE_OK)
            return admission;
    }
    if (store == NULL || out == NULL || view == NULL)
        return store_error(error, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                           "GAME stage ordinal arguments");
    for (store_chunk *entry = store->chunks; entry != NULL; entry = entry->next) {
        if (!entry->game_stage || entry->phase == MC_WORLD_STORE_GAME_COMMITTED)
            continue;
        if (ordinal != 0) {
            --ordinal;
            continue;
        }
        if (!store_chunk_phase_valid(entry))
            return store_error(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                               "GAME stage ownership metadata");
        const mc_world_store_game_stage_view snapshot = store_game_stage_view(store, entry);
        *out = &entry->stage;
        *view = snapshot;
        return store_error(error, MC_WORLD_STORAGE_OK, NULL);
    }
    return store_error(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "GAME stage ordinal");
}

bool mc_world_store_chunk_source_enabled(const mc_world_store *store, const mc_chunk_state *state) {
    const store_chunk *entry = store_find_chunk(store, state);
    if (entry == NULL || !store_chunk_phase_valid(entry) ||
        entry->phase == MC_WORLD_STORE_GAME_FAILED)
        return false;
    const mc_world_chunk *chunk = store_runtime_chunk(entry);
    return chunk == NULL || mc_world_chunk_source_enabled(chunk);
}

mc_chunk_load_result mc_world_store_load_callback(void *loader,
                                                  const mc_chunk_acquire_context *context,
                                                  int32_t x, int32_t z, mc_chunk_state **out) {
    mc_world_store_loader *binding = loader;
    if (binding != NULL && binding->store != NULL && binding->store->source != NULL) {
        (void)store_source_legacy(&binding->last_error);
        return MC_CHUNK_LOAD_FATAL;
    }
    if (binding == NULL)
        return MC_CHUNK_LOAD_FATAL;
    if (binding->store == NULL || context == NULL || out == NULL) {
        (void)store_error(&binding->last_error, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                          "store loader arguments");
        return MC_CHUNK_LOAD_FATAL;
    }
    mc_world_storage_status status = store_load_chunk(binding->store, binding->dimension, x, z,
                                                      true, NULL, out, &binding->last_error);
    return status == MC_WORLD_STORAGE_OK        ? MC_CHUNK_LOAD_LOADED
           : status == MC_WORLD_STORAGE_MISSING ? MC_CHUNK_LOAD_MISS
                                                : MC_CHUNK_LOAD_FATAL;
}

void mc_world_store_free(mc_world_store *store) {
    if (store != NULL && store->source != NULL)
        return; /* Counted source owners require their explicit occurrence API. */
    if (store == NULL)
        return;
    store_chunk *entry = store->chunks;
    while (entry != NULL) {
        store_chunk *next = entry->next;
        store_chunk_free(entry);
        free(entry);
        entry = next;
    }
    store_cache *registration = store->caches;
    while (registration != NULL) {
        store_cache *next = registration->next;
        free(registration);
        registration = next;
    }
    mc_world_store_reservation *reservation = store->reservations;
    while (reservation != NULL) {
        mc_world_store_reservation *next = reservation->next;
        store_chunk_free(reservation->entry);
        free(reservation->entry);
        free(reservation);
        reservation = next;
    }
    mc_nbt_document_free(store->metadata);
    free(store->root);
    free(store->nether);
    free(store->session);
    for (size_t i = 0; i < 3; ++i)
        free(store->metadata_paths[i]);
    free(store);
}

static mc_world_storage_status store_free_admit(const mc_world_store *store,
                                                mc_world_storage_error *error) {
    if (store->chunk_count > store->limits.max_loaded_chunks ||
        store->reservation_count > store->limits.max_loaded_chunks - store->chunk_count ||
        store->cache_count > MC_WORLD_STORE_MAX_REGISTERED_CACHES)
        return store_error(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Store ownership counts");

    size_t registrations = 0;
    for (const store_cache *entry = store->caches; entry != NULL; entry = entry->next) {
        if (registrations == store->cache_count ||
            !store_cache_valid(entry->cache, entry->fallback))
            return store_error(error, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                               "Store close registered cache metadata");
        ++registrations;
    }
    if (registrations != store->cache_count)
        return store_error(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                           "Store registered cache count");

    size_t reservations = 0;
    for (const mc_world_store_reservation *entry = store->reservations; entry != NULL;
         entry = entry->next) {
        if (reservations == store->reservation_count || entry->entry == NULL)
            return store_error(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                               "Store reservation ownership metadata");
        ++reservations;
    }
    if (reservations != store->reservation_count)
        return store_error(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Store reservation count");

    size_t chunks = 0;
    const store_chunk *last = NULL;
    bool busy = registrations != 0 || reservations != 0;
    for (const store_chunk *entry = store->chunks; entry != NULL; entry = entry->next) {
        if (chunks == store->chunk_count || !store_chunk_owner_valid(entry))
            return store_error(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                               "Store close Chunk ownership metadata");
        ++chunks;
        last = entry;
        const mc_world_chunk *chunk = store_runtime_chunk(entry);
        busy = busy || entry->leases != 0 || entry->coordinator_lease || entry->game_save_open ||
               (chunk != NULL && !mc_world_chunk_entity_containers_retired(chunk)) ||
               (chunk == NULL && store_chunk_state(entry)->active);
    }
    if (chunks != store->chunk_count || last != store->last_chunk)
        return store_error(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Store Chunk chain metadata");
    if (busy)
        return store_error(error, MC_WORLD_STORAGE_BUSY, "Store owners not quiescent");

    return MC_WORLD_STORAGE_OK;
}

mc_world_storage_status mc_world_store_terminal_validate(const mc_world_store *store,
                                                         const mc_world_runtime *world,
                                                         const mc_chunk_cache *owned_cache,
                                                         mc_chunk_state *const *owned_fallback,
                                                         mc_world_storage_error *error) {
    if (store == NULL || world == NULL || owned_cache == NULL || owned_fallback == NULL)
        return store_error(error, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                           "Store terminal validation arguments");
    if (store->source != NULL) {
        const mc_world_store_source *source = store->source;
        if (source->classes == NULL || source->state.holds == 0 || source->classes->holds == 0 ||
            source->classes->handlers == 0)
            return store_error(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                               "Store terminal source ownership metadata");
        const mc_world_storage_status admission = store_source_native_admit(store, error);
        if (admission != MC_WORLD_STORAGE_OK)
            return admission;
    }
    if (store->chunk_count > store->limits.max_loaded_chunks ||
        store->reservation_count > store->limits.max_loaded_chunks - store->chunk_count ||
        store->cache_count > MC_WORLD_STORE_MAX_REGISTERED_CACHES)
        return store_error(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                           "Store terminal ownership counts");

    size_t registrations = 0, owned_registrations = 0;
    bool foreign_registration = false;
    for (const store_cache *entry = store->caches; entry != NULL; entry = entry->next) {
        if (registrations == store->cache_count ||
            !store_cache_valid(entry->cache, entry->fallback))
            return store_error(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                               "Store terminal registered cache metadata");
        ++registrations;
        if (entry->cache == owned_cache) {
            if (entry->fallback != owned_fallback || ++owned_registrations != 1)
                return store_error(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                   "Store terminal owned cache registration identity");
        } else
            foreign_registration = true;
    }
    if (registrations != store->cache_count || owned_registrations != 1)
        return store_error(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                           "Store terminal registered cache count");

    size_t reservations = 0;
    for (const mc_world_store_reservation *entry = store->reservations; entry != NULL;
         entry = entry->next) {
        if (reservations == store->reservation_count || entry->entry == NULL)
            return store_error(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                               "Store terminal reservation ownership metadata");
        ++reservations;
    }
    if (reservations != store->reservation_count)
        return store_error(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                           "Store terminal reservation count");

    size_t chunks = 0;
    const store_chunk *last = NULL;
    bool busy = foreign_registration || reservations != 0;
    for (const store_chunk *entry = store->chunks; entry != NULL; entry = entry->next) {
        if (chunks == store->chunk_count || !store_chunk_owner_valid(entry))
            return store_error(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                               "Store terminal Chunk ownership metadata");
        ++chunks;
        last = entry;
        busy = busy || entry->leases != 0 || entry->coordinator_lease || entry->game_save_open ||
               (entry->imports_started && entry->phase != MC_WORLD_STORE_GAME_COMMITTED);
        const mc_world_chunk *chunk = store_runtime_chunk(entry);
        if (chunk != NULL) {
            mc_world_chunk_fields fields;
            if (mc_world_chunk_fields_get(chunk, &fields) != MC_WORLD_STORAGE_OK ||
                fields.d != world)
                return store_error(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                   "Store terminal actual Chunk World identity");
        } else
            busy = busy || store_chunk_state(entry)->active;
    }
    if (chunks != store->chunk_count || last != store->last_chunk)
        return store_error(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                           "Store terminal Chunk chain metadata");
    if (busy)
        return store_error(error, MC_WORLD_STORAGE_BUSY,
                           "Store terminal outside ownership or work");
    return store_error(error, MC_WORLD_STORAGE_OK, NULL);
}

mc_world_storage_status mc_world_store_free_checked(mc_world_store **owner,
                                                    mc_world_storage_error *error) {
    if (owner != NULL && *owner != NULL && (*owner)->source != NULL)
        return store_source_checked_free(owner, error);
    if (owner == NULL)
        return store_error(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "Store close owner output");
    mc_world_store *store = *owner;
    if (store == NULL)
        return store_error(error, MC_WORLD_STORAGE_OK, NULL);
    mc_world_storage_status admission = store_free_admit(store, error);
    if (admission != MC_WORLD_STORAGE_OK)
        return admission;
    /* Complete admission above; no allocation, source call or later owner
     * validation can be introduced after the first release. The real lower
     * finalizers accept already-retired coherent empty containers. */
    mc_world_store_free(store);
    *owner = NULL;
    return store_error(error, MC_WORLD_STORAGE_OK, NULL);
}

mc_world_storage_status mc_world_store_entity_containers_validate(const mc_world_store *store,
                                                                  const mc_world_runtime *world,
                                                                  mc_world_storage_error *error) {
    if (store != NULL && store->source != NULL) {
        mc_world_storage_status admission = store_source_native_admit(store, error);
        if (admission != MC_WORLD_STORAGE_OK)
            return admission;
    }
    if (store == NULL || world == NULL)
        return store_error(error, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                           "Store entity retirement arguments");
    for (const store_chunk *entry = store->chunks; entry != NULL; entry = entry->next) {
        mc_world_chunk *chunk =
            entry->kind == STORE_CHUNK_GENERATED ? entry->owner.generated : entry->loaded_binding;
        if (chunk == NULL)
            continue; /* Generic stored/opaque NBT has no runtime owner to clear. */
        mc_world_chunk_fields fields;
        const mc_world_storage_status inspected = mc_world_chunk_fields_get(chunk, &fields);
        if (inspected != MC_WORLD_STORAGE_OK)
            return store_error(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                               "Store entity retirement Chunk identity");
        if (fields.d != world)
            continue;
        if (entry->leases != 0 || entry->coordinator_lease || entry->game_save_open)
            return store_error(error, MC_WORLD_STORAGE_BUSY,
                               "Store Chunk lease during entity retirement");
        if (!store_chunk_owner_valid(entry))
            return store_error(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                               "Store entity retirement Chunk ownership");
    }
    return store_error(error, MC_WORLD_STORAGE_OK, NULL);
}

void mc_world_store_entity_containers_release(mc_world_store *store,
                                              const mc_world_runtime *world) {
    /* Requires whole admission above and unchanged exclusive owners. No lease
     * is acquired/released, no entry is removed, and no cache/active flag changes.
     * fields_get cannot fail for those same already-admitted live identities. */
    for (store_chunk *entry = store->chunks; entry != NULL; entry = entry->next) {
        mc_world_chunk *chunk =
            entry->kind == STORE_CHUNK_GENERATED ? entry->owner.generated : entry->loaded_binding;
        if (chunk != NULL) {
            mc_world_chunk_fields fields;
            (void)mc_world_chunk_fields_get(chunk, &fields);
            if (fields.d == world)
                mc_world_chunk_containers_release(chunk);
        }
    }
}

mc_world_storage_status
mc_world_store_entity_containers_visit_chunks(mc_world_store *store, const mc_world_runtime *world,
                                              mc_world_terminal_chunk_visitor visit, void *context,
                                              mc_world_storage_error *error) {
    if (visit == NULL)
        return store_error(error, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                           "Store terminal Chunk visitor");
    const mc_world_storage_status admitted =
        mc_world_store_entity_containers_validate(store, world, error);
    if (admitted != MC_WORLD_STORAGE_OK)
        return admitted;
    for (const store_chunk *entry = store->chunks; entry != NULL; entry = entry->next) {
        const mc_world_chunk *chunk =
            entry->kind == STORE_CHUNK_GENERATED ? entry->owner.generated : entry->loaded_binding;
        if (chunk == NULL)
            continue;
        mc_world_chunk_fields fields;
        (void)mc_world_chunk_fields_get(chunk, &fields); /* Same exclusive admitted identity. */
        if (fields.d == world) {
            const mc_world_storage_status status = visit(context, chunk, error);
            if (status != MC_WORLD_STORAGE_OK)
                return status;
        }
    }
    return store_error(error, MC_WORLD_STORAGE_OK, NULL);
}

mc_world_storage_status mc_world_store_terminal_visit_chunks(mc_world_store *store,
                                                             const mc_world_runtime *world,
                                                             mc_world_terminal_chunk_visitor visit,
                                                             void *context,
                                                             mc_world_storage_error *error) {
    if (visit == NULL)
        return store_error(error, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                           "Store terminal Chunk visitor");
    const mc_world_storage_status admitted =
        mc_world_store_entity_containers_validate(store, world, error);
    if (admitted != MC_WORLD_STORAGE_OK)
        return admitted;
    /* First scope has one World/handler graph. Refuse a mixed actual handler
     * before visiting any child; source cache membership is not ownership. */
    for (const store_chunk *entry = store->chunks; entry != NULL; entry = entry->next) {
        const mc_world_chunk *chunk =
            entry->kind == STORE_CHUNK_GENERATED ? entry->owner.generated : entry->loaded_binding;
        if (chunk == NULL)
            continue;
        mc_world_chunk_fields fields;
        if (mc_world_chunk_fields_get(chunk, &fields) != MC_WORLD_STORAGE_OK)
            return store_error(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                               "Store terminal Chunk identity");
        if (fields.d != NULL && fields.d != world)
            return store_error(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                               "Store terminal mixed World ownership");
    }
    return mc_world_store_entity_containers_visit_chunks(store, world, visit, context, error);
}

static mc_world_storage_status store_terminal_source_file(const mc_java_file *file,
                                                          const mc_java_files *files,
                                                          mc_world_storage_error *error) {
    mc_java_file_state state;
    if (file == NULL || mc_java_file_namespace(file) != files ||
        mc_java_file_read_state(file, &state, NULL) != MC_JAVA_FILE_OK)
        return store_error(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                           "Store terminal source File provenance");
    if (state.active || state.phase == MC_JAVA_FILE_CONSTRUCTING ||
        state.phase == MC_JAVA_FILE_RETIRING)
        return store_error(error, MC_WORLD_STORAGE_BUSY, "Store terminal source active File");
    if (state.phase != MC_JAVA_FILE_READY || state.holds == 0 || state.path == NULL ||
        state.has_failed_array)
        return store_error(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                           "Store terminal source incomplete File child");
    /* Independent counted Files remain valid owners. This does not authorize
     * final File/namespace closure or impose a guessed File hold allowance. */
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status store_terminal_source_input(const mc_world_store_source *source,
                                                           mc_world_storage_error *error) {
    const mc_world_source_input_state *input = source->input;
    if (input == NULL)
        return MC_WORLD_STORAGE_OK; /* Actual prefix already cleared by its owner. */
    if (input->candidate != NULL)
        return store_error(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                           "Store terminal source pending input File");
    mc_world_storage_status status =
        store_terminal_source_file(input->file, source->classes->files, error);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    if (input->stream != NULL) {
        mc_java_file_stream_state stream;
        if (mc_java_file_stream_read_state(input->stream, &stream, NULL) != MC_JAVA_FILE_OK)
            return store_error(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                               "Store terminal source FileInputStream owner");
        if (stream.active || stream.phase == MC_JAVA_FILE_CONSTRUCTING ||
            stream.phase == MC_JAVA_FILE_RETIRING)
            return store_error(error, MC_WORLD_STORAGE_BUSY,
                               "Store terminal source active FileInputStream");
        if (stream.phase != MC_JAVA_FILE_READY || stream.holds == 0 || stream.native_open ||
            !stream.source_close_reached || !stream.source_close_succeeded)
            return store_error(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                               "Store terminal source unclosed FileInputStream");
    }
    mc_nbt_stream_state reader = {0};
    if (input->reader != NULL) {
        if (mc_nbt_stream_state_read(input->reader, &reader, NULL) != MC_NBT_OK)
            return store_error(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                               "Store terminal source DataInput owner");
        if (reader.active || reader.phase == MC_NBT_STREAM_CONSTRUCTING ||
            reader.phase == MC_NBT_STREAM_PARSING || reader.phase == MC_NBT_STREAM_RETIRING)
            return store_error(error, MC_WORLD_STORAGE_BUSY,
                               "Store terminal source active DataInput");
    }
    if (input->gzip != NULL) {
        mc_gzip_state gzip;
        if (mc_gzip_input_state_read(input->gzip, &gzip, NULL) != MC_GZIP_OK)
            return store_error(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                               "Store terminal source GZIP owner");
        if (gzip.active || gzip.retiring || gzip.phase == MC_GZIP_CONSTRUCTING ||
            gzip.phase == MC_GZIP_RETIRING)
            return store_error(error, MC_WORLD_STORAGE_BUSY, "Store terminal source active GZIP");
        if (gzip.phase != MC_GZIP_READY || gzip.holds == 0 || !gzip.source_closed ||
            !gzip.super_closed || gzip.source_in != input->stream || input->stream == NULL ||
            gzip.pending_input != NULL || gzip.pending_inflater != NULL || gzip.source_inf == NULL)
            return store_error(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                               "Store terminal source incomplete GZIP child");
        mc_raw_inflater_state inflater;
        if (mc_raw_inflater_state_read(gzip.source_inf, &inflater, NULL) != MC_RAW_INFLATER_OK)
            return store_error(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                               "Store terminal source Inflater owner");
        if (inflater.active || inflater.phase == MC_RAW_INFLATER_RETIRING)
            return store_error(error, MC_WORLD_STORAGE_BUSY,
                               "Store terminal source active Inflater");
        if (inflater.phase != MC_RAW_INFLATER_READY || inflater.armed)
            return store_error(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                               "Store terminal source unclosed Inflater");
    }
    if (!input->source_returned || input->info != NULL || input->document != NULL)
        return store_error(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                           "Store terminal source incomplete input prefix");
    if (input->call == MC_WORLD_SOURCE_INPUT_SESSION) {
        if (!input->input_constructed || !input->token_read || !input->token_matches ||
            !input->closed || input->reader == NULL || input->stream == NULL ||
            input->gzip != NULL || input->data != NULL || reader.phase != MC_NBT_STREAM_READY ||
            reader.source_in != input->stream)
            return store_error(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                               "Store terminal source incomplete session input");
    } else if (input->call == MC_WORLD_SOURCE_INPUT_METADATA) {
        if (!input->output_transferred ||
            (input->exists &&
             (!input->input_constructed || !input->parsed || !input->closed ||
              input->reader == NULL || input->gzip == NULL || input->stream == NULL ||
              input->data == NULL || reader.phase != MC_NBT_STREAM_COMPLETE ||
              reader.source_in != input->gzip)) ||
            (!input->exists && (input->reader != NULL || input->gzip != NULL ||
                                input->stream != NULL || input->data != NULL)))
            return store_error(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                               "Store terminal source incomplete metadata input");
    } else {
        return store_error(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                           "Store terminal source input call identity");
    }
    /* Only compare the reader's untyped context with the actual typed child
     * above. Independent Info/Data/document occurrences are not Handler holds. */
    return MC_WORLD_STORAGE_OK;
}

mc_world_storage_status
mc_world_store_terminal_source_admit(const mc_world_store *store,
                                     const mc_world_terminal_store_anchor *anchors, size_t count,
                                     mc_world_storage_error *error) {
    if (store == NULL || anchors == NULL || count == 0)
        return store_error(error, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                           "Store terminal source owning slots");
    if (count > 16)
        return store_error(error, MC_WORLD_STORAGE_LIMIT, "Store terminal source anchor bound");
    for (size_t i = 0; i < count; ++i) {
        if (anchors[i].slot == NULL || *anchors[i].slot != store)
            return store_error(error, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                               "Store terminal source owning slot identity");
        for (size_t j = 0; j < i; ++j)
            if (anchors[i].slot == anchors[j].slot)
                return store_error(error, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                                   "Store terminal source repeated owning slot");
    }
    const mc_world_store_source *source = store->source;
    if (source == NULL || source->classes == NULL)
        return store_error(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                           "Store terminal source Handler owner");
    const mc_world_source_state *state = &source->state;
    const mc_world_store_source_classes *classes = source->classes;
    if (state->active || classes->active || state->phase == MC_WORLD_SOURCE_CONSTRUCTING ||
        state->phase == MC_WORLD_SOURCE_RETIRING ||
        classes->phase == MC_WORLD_SOURCE_CONSTRUCTING ||
        classes->phase == MC_WORLD_SOURCE_RETIRING)
        return store_error(error, MC_WORLD_STORAGE_BUSY, "Store terminal source active Handler");
    if (state->phase != MC_WORLD_SOURCE_READY || classes->phase != MC_WORLD_SOURCE_READY ||
        classes->holds == 0 || classes->handlers == 0 || classes->holds < classes->handlers ||
        classes->files == NULL || classes->logging == NULL)
        return store_error(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                           "Store terminal source Handler class provenance");
    if (state->holds > count)
        return store_error(error, MC_WORLD_STORAGE_BUSY,
                           "Store terminal source external Handler occurrence");
    if (state->holds < count)
        return store_error(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                           "Store terminal source Handler occurrence deficit");
    if (!state->e_stored || !state->session_entered || !state->session_file_stored ||
        !state->output_stored || !state->write_returned || !state->close_returned ||
        !state->d_returned || !state->fm_returned || !state->mx_returned ||
        state->pending_file != NULL || source->data_file != NULL || source->data_name != NULL)
        return store_error(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                           "Store terminal source incomplete Handler prefix");
    const mc_java_file *files[] = {state->source_b, state->source_c, state->source_d,
                                   state->session_file};
    for (size_t i = 0; i < sizeof(files) / sizeof(*files); ++i) {
        mc_world_storage_status status =
            store_terminal_source_file(files[i], classes->files, error);
        if (status != MC_WORLD_STORAGE_OK)
            return status;
    }
    mc_java_file_output_state fos;
    mc_java_data_output_state dos;
    if (!mc_java_file_output_inspect(state->pending_fos, &fos) ||
        !mc_java_data_output_inspect(state->pending_dos, &dos))
        return store_error(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                           "Store terminal source session output owners");
    if (fos.active || dos.active || fos.phase == MC_JAVA_OUTPUT_CONSTRUCTING ||
        dos.phase == MC_JAVA_OUTPUT_CONSTRUCTING || fos.phase == MC_JAVA_OUTPUT_RETIRING ||
        dos.phase == MC_JAVA_OUTPUT_RETIRING)
        return store_error(error, MC_WORLD_STORAGE_BUSY,
                           "Store terminal source active session output");
    if (fos.phase != MC_JAVA_OUTPUT_READY || dos.phase != MC_JAVA_OUTPUT_READY || fos.holds == 0 ||
        dos.holds == 0 || fos.native_open || !fos.source_closed || !fos.source_close_reached ||
        !fos.source_close_succeeded || !dos.closed || fos.pending_close_lock != NULL ||
        dos.pending_close_lock != NULL || fos.argument != state->session_file ||
        fos.descriptor_parent != state->pending_fos || dos.out != state->pending_fos)
        return store_error(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                           "Store terminal source incomplete session output");
    mc_world_storage_status status = store_terminal_source_input(source, error);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    /* No shared File/logging/region namespace is closed, retained or activated.
     * This admission is neither a reservation nor a cached cleanup permit. */
    return store_error(error, MC_WORLD_STORAGE_OK, NULL);
}

#include "source_game_stage.inc"
#include "store_source.inc"
