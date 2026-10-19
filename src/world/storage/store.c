#include "mc/world/storage/store.h"

#include "../chunk/object_internal.h"
#include "chunk_build_internal.h"
#include "files.h"
#include "generated_internal.h"
#include "mc/core/java/java.h"
#include "objects_internal.h"

#include <stdlib.h>
#include <string.h>

typedef enum store_chunk_kind { STORE_CHUNK_STORED, STORE_CHUNK_GENERATED } store_chunk_kind;

typedef struct store_chunk {
    store_chunk_kind kind;
    union {
        mc_stored_chunk *stored;
        mc_world_chunk *generated;
    } owner;
    mc_world_chunk *loaded_binding;
    int32_t dimension;
    size_t leases;
    struct store_chunk *next;
} store_chunk;

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

static mc_chunk_state *store_chunk_state(const store_chunk *entry)
{
    switch (entry->kind) {
        case STORE_CHUNK_STORED:
            return mc_stored_chunk_state(entry->owner.stored);
        case STORE_CHUNK_GENERATED:
            return mc_world_chunk_state(entry->owner.generated);
    }
    return NULL;
}

static const mc_nbt_tag *store_chunk_root(const store_chunk *entry)
{
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
                                                mc_stored_chunk_error *error)
{
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

static void store_chunk_free(store_chunk *entry)
{
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
                                           mc_world_storage_status status, const char *operation)
{
    if (error != NULL)
        *error = (mc_world_storage_error){status, operation, 0};
    return status;
}

static mc_world_storage_status store_nbt_error(mc_world_storage_error *error, mc_nbt_status status)
{
    mc_world_storage_status result =
        status == MC_NBT_LIMIT               ? MC_WORLD_STORAGE_LIMIT
        : status == MC_NBT_ALLOCATION_FAILED ? MC_WORLD_STORAGE_ALLOCATION_FAILED
        : status == MC_NBT_INVALID_ARGUMENT  ? MC_WORLD_STORAGE_INVALID_ARGUMENT
                                             : MC_WORLD_STORAGE_INVALID_DATA;
    return store_error(error, result, mc_nbt_status_string(status));
}

static mc_world_storage_status store_level_error(mc_world_storage_error *error,
                                                 mc_level_status status,
                                                 const mc_level_error *detail)
{
    mc_world_storage_status result =
        status == MC_LEVEL_LIMIT               ? MC_WORLD_STORAGE_LIMIT
        : status == MC_LEVEL_ALLOCATION_FAILED ? MC_WORLD_STORAGE_ALLOCATION_FAILED
        : status == MC_LEVEL_INVALID_ARGUMENT  ? MC_WORLD_STORAGE_INVALID_ARGUMENT
                                               : MC_WORLD_STORAGE_INVALID_DATA;
    return store_error(error, result,
                       detail->field == NULL ? mc_level_status_string(status) : detail->field);
}

static mc_world_storage_status store_compression_error(mc_world_storage_error *error,
                                                       mc_compression_status status)
{
    mc_world_storage_status result =
        status == MC_COMPRESSION_LIMIT               ? MC_WORLD_STORAGE_LIMIT
        : status == MC_COMPRESSION_ALLOCATION_FAILED ? MC_WORLD_STORAGE_ALLOCATION_FAILED
        : status == MC_COMPRESSION_INVALID_ARGUMENT  ? MC_WORLD_STORAGE_INVALID_ARGUMENT
                                                     : MC_WORLD_STORAGE_INVALID_DATA;
    return store_error(error, result, mc_compression_status_string(status));
}

static mc_world_storage_status store_chunk_error(mc_world_storage_error *error,
                                                 mc_stored_chunk_status status,
                                                 const mc_stored_chunk_error *detail)
{
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

mc_world_store_limits mc_world_store_default_limits(void)
{
    const mc_world_store_limits limits = {512, mc_nbt_default_limits(),
                                          mc_compression_default_limits()};
    return limits;
}

static void store_token(int64_t value, uint8_t bytes[8])
{
    uint64_t bits = (uint64_t)value;
    for (size_t i = 0; i < 8; ++i)
        bytes[i] = (uint8_t)(bits >> (56u - 8u * i));
}

mc_world_storage_status mc_world_store_open(const char *root, mc_world_regions *regions,
                                            const mc_world_store_options *options,
                                            mc_world_store **out, mc_world_storage_error *error)
{
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
                                                     mc_world_storage_error *error)
{
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
                                               mc_world_storage_error *error)
{
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

static bool store_fallback_status(mc_world_storage_status status)
{
    return status == MC_WORLD_STORAGE_MISSING || status == MC_WORLD_STORAGE_IO_ERROR ||
           status == MC_WORLD_STORAGE_INVALID_DATA;
}

mc_world_storage_status mc_world_store_load_info(mc_world_store *store,
                                                 mc_world_storage_error *error)
{
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
                                                      const mc_world_info_error *detail)
{
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
                      mc_level_info *out_view, mc_world_storage_error *error)
{
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
                                                       mc_world_storage_error *error)
{
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

const mc_level_info *mc_world_store_info(const mc_world_store *store)
{
    return store == NULL || !store->has_info ? NULL : &store->info;
}

const mc_nbt_tag *mc_world_store_metadata_root(const mc_world_store *store)
{
    return store == NULL ? NULL : mc_nbt_document_root(store->metadata);
}

const char *mc_world_store_root(const mc_world_store *store)
{
    return store == NULL ? NULL : store->root;
}

static mc_world_storage_status store_encode_nbt(const mc_nbt_tag *root, const mc_nbt_limits *limits,
                                                mc_ws_file_bytes *out,
                                                mc_world_storage_error *error)
{
    size_t length = 0;
    mc_nbt_status nbt = mc_nbt_measure(root, limits, &length, NULL);
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
    nbt = mc_nbt_write(root, &remaining, bytes, length, &written, NULL);
    if (nbt != MC_NBT_OK) {
        free(bytes);
        return store_nbt_error(error, nbt);
    }
    *out = (mc_ws_file_bytes){bytes, written};
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status store_existing_file(const char *path, bool *exists,
                                                   mc_world_storage_error *error)
{
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
                                            size_t length, mc_world_storage_error *error)
{
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
                                                    mc_world_storage_error *error)
{
    mc_ws_file_bytes raw = {0};
    mc_world_storage_status status = store_encode_nbt(root, &store->limits.nbt, &raw, error);
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
                                                 int64_t saved_at_ms, mc_world_storage_error *error)
{
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
                                                       mc_world_storage_error *error)
{
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
    mc_nbt_document *document = NULL;
    mc_level_error detail;
    mc_level_status built = mc_level_build_record(mc_world_info_record_root(record),
                                                  mc_world_store_metadata_root(store),
                                                  &store->limits.nbt, &document, &detail);
    mc_world_info_record_release(record);
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

static mc_world_storage_status store_dimension(const mc_world_store *store, int32_t dimension,
                                               const char **out, mc_world_storage_error *error)
{
    if (dimension == 0)
        *out = store->root;
    else if (dimension == -1)
        *out = store->nether;
    else
        return store_error(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "world dimension");
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status store_new_chunk(mc_world_store *store, store_chunk **out,
                                               mc_world_storage_error *error)
{
    if (store->chunk_count >= store->limits.max_loaded_chunks ||
        store->reservation_count >= store->limits.max_loaded_chunks - store->chunk_count)
        return store_error(error, MC_WORLD_STORAGE_LIMIT, "retained chunk count");
    store_chunk *entry = calloc(1, sizeof(*entry));
    if (entry == NULL)
        return store_error(error, MC_WORLD_STORAGE_ALLOCATION_FAILED, "retained chunk link");
    /* Reservations and old copy/load routes always begin as the stored variant. */
    *entry = (store_chunk){.kind = STORE_CHUNK_STORED, .owner.stored = NULL};
    *out = entry;
    return MC_WORLD_STORAGE_OK;
}

static store_chunk *store_find_chunk(const mc_world_store *store, const mc_chunk_state *chunk)
{
    if (store == NULL || chunk == NULL)
        return NULL;
    store_chunk *entry = store->chunks;
    while (entry != NULL && store_chunk_state(entry) != chunk)
        entry = entry->next;
    return entry;
}

static void store_publish_chunk(mc_world_store *store, store_chunk *entry, int32_t dimension)
{
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
                                                        mc_world_storage_error *error)
{
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
                                               const mc_chunk_state *state)
{
    const store_chunk *entry = store_find_chunk(store, state);
    return entry != NULL && entry->kind == STORE_CHUNK_GENERATED ? entry->owner.generated : NULL;
}

mc_world_chunk *mc_world_store_chunk_object(const mc_world_store *store,
                                            const mc_chunk_state *state)
{
    const store_chunk *entry = store_find_chunk(store, state);
    return entry == NULL                          ? NULL
           : entry->kind == STORE_CHUNK_GENERATED ? entry->owner.generated
                                                  : entry->loaded_binding;
}

bool mc_world_store_has_chunks_for_world(const mc_world_store *store, const mc_world_runtime *world)
{
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
                                            const mc_world_runtime *world)
{
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

static const mc_nbt_tag *store_get(const mc_nbt_tag *parent, const char *name)
{
    uint16_t units[16];
    size_t length = strlen(name);
    for (size_t i = 0; i < length; ++i)
        units[i] = (uint16_t)(unsigned char)name[i];
    return mc_nbt_compound_get(parent, units, length);
}

static bool store_opaque(const store_chunk *entry)
{
    const mc_nbt_tag *level = store_get(store_chunk_root(entry), "Level");
    const mc_nbt_tag *entities = store_get(level, "Entities");
    const mc_nbt_tag *tiles = store_get(level, "TileEntities");
    return (entities != NULL && entities->value.list.length != 0) ||
           (tiles != NULL && tiles->value.list.length != 0);
}

static mc_world_storage_status store_load_chunk(mc_world_store *store, int32_t dimension, int32_t x,
                                                int32_t z, bool activation, mc_world_runtime *world,
                                                mc_chunk_state **out, mc_world_storage_error *error)
{
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
                             "opaque entity or tile activation");
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
                                                  mc_world_storage_error *error)
{
    return store_load_chunk(store, dimension, x, z, false, NULL, out, error);
}

mc_world_storage_status mc_world_store_load_game_chunk(mc_world_store *store,
                                                       mc_world_runtime *world, int32_t dimension,
                                                       int32_t x, int32_t z, mc_chunk_state **out,
                                                       mc_world_storage_error *error)
{
    if (world == NULL)
        return store_error(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "GAME load World receiver");
    return store_load_chunk(store, dimension, x, z, true, world, out, error);
}

mc_world_storage_status mc_world_store_retain_chunk(mc_world_store *store, int32_t dimension,
                                                    const mc_chunk_state *chunk,
                                                    mc_chunk_state **out,
                                                    mc_world_storage_error *error)
{
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
                                                 mc_world_storage_error *error)
{
    if (store == NULL || chunk == NULL || growth == NULL)
        return store_error(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "chunk write arguments");
    store_chunk *entry = store_find_chunk(store, chunk);
    if (entry == NULL)
        return store_error(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "foreign chunk identity");
    if (account == NULL && !store->has_info)
        return store_error(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                           "chunk write requires metadata");
    mc_world_storage_status status = mc_world_store_check_session(store, error);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    if (runtime) {
        if (store_opaque(entry))
            return store_error(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                               "opaque entity or tile runtime save");
        chunk->has_entities = false;
    }
    mc_nbt_document *document = NULL;
    mc_stored_chunk_error detail;
    mc_stored_chunk_status built =
        store_chunk_build(entry, world_time, &store->limits.nbt, &document, &detail);
    if (built != MC_STORED_CHUNK_OK)
        return store_chunk_error(error, built, &detail);
    mc_ws_file_bytes raw = {0};
    status = store_encode_nbt(mc_nbt_document_root(document), &store->limits.nbt, &raw, error);
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
                                                   uint32_t *growth, mc_world_storage_error *error)
{
    return store_write_chunk(store, chunk, world_time, timestamp, growth, false, NULL, error);
}

mc_world_storage_status mc_world_store_write_runtime_chunk(mc_world_store *store,
                                                           mc_chunk_state *chunk,
                                                           int64_t world_time, uint32_t timestamp,
                                                           uint32_t *growth,
                                                           mc_world_storage_error *error)
{
    return store_write_chunk(store, chunk, world_time, timestamp, growth, true, NULL, error);
}

mc_world_storage_status mc_world_store_write_runtime_chunk_for_info(
    mc_world_store *store, mc_chunk_state *chunk, mc_world_info *info, int64_t world_time,
    uint32_t timestamp, uint32_t *growth, mc_world_storage_error *error)
{
    if (info == NULL)
        return store_error(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "chunk WorldInfo argument");
    if (mc_world_info_live_get(info) == NULL)
        return store_error(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                           "chunk requires READY WorldInfo");
    return store_write_chunk(store, chunk, world_time, timestamp, growth, true, info, error);
}

size_t mc_world_store_chunk_count(const mc_world_store *store)
{
    return store == NULL ? 0 : store->chunk_count;
}

bool mc_world_store_owns_chunk(const mc_world_store *store, const mc_chunk_state *chunk)
{
    return store_find_chunk(store, chunk) != NULL;
}

mc_world_storage_status mc_world_store_lease_chunk(mc_world_store *store,
                                                   const mc_chunk_state *chunk,
                                                   mc_world_storage_error *error)
{
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
                                                     mc_world_storage_error *error)
{
    store_chunk *entry = store_find_chunk(store, chunk);
    if (entry == NULL)
        return store_error(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "foreign chunk identity");
    if (entry->leases == 0)
        return store_error(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "unbalanced chunk lease");
    --entry->leases;
    return store_error(error, MC_WORLD_STORAGE_OK, NULL);
}

static bool store_cache_valid(const mc_chunk_cache *cache, mc_chunk_state *const *fallback)
{
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
                                                      mc_world_storage_error *error)
{
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
                                                        mc_world_storage_error *error)
{
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
                                                    mc_world_storage_error *error)
{
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
    if (entry->leases != 0 || chunk->active || referenced)
        return store_error(error, MC_WORLD_STORAGE_BUSY, "chunk identity still referenced");
    *link = entry->next;
    if (store->last_chunk == entry)
        store->last_chunk = previous;
    --store->chunk_count;
    store_chunk_free(entry);
    free(entry);
    return store_error(error, MC_WORLD_STORAGE_OK, NULL);
}

static mc_world_store_reservation **store_find_reservation(mc_world_store *store,
                                                           mc_world_store_reservation *reservation)
{
    mc_world_store_reservation **link = &store->reservations;
    while (*link != NULL && *link != reservation)
        link = &(*link)->next;
    return link;
}

mc_world_storage_status mc_world_store_reserve_chunk(mc_world_store *store, int32_t dimension,
                                                     mc_world_store_reservation **out,
                                                     mc_world_storage_error *error)
{
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
                                                          mc_world_storage_error *error)
{
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
                                                       mc_world_storage_error *error)
{
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

size_t mc_world_store_reservation_count(const mc_world_store *store)
{
    return store == NULL ? 0 : store->reservation_count;
}

mc_chunk_load_result mc_world_store_load_callback(void *loader,
                                                  const mc_chunk_acquire_context *context,
                                                  int32_t x, int32_t z, mc_chunk_state **out)
{
    mc_world_store_loader *binding = loader;
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

void mc_world_store_free(mc_world_store *store)
{
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
