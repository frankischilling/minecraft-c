#include "mc/world/storage/regions.h"

#include "files.h"
#include "mc/core/java/java.h"
#include "mc/storage/region/region.h"

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define REGIONS_PATH_BYTES 32768u

typedef struct regions_entry {
    char *path;
    uint8_t *image;
    size_t length;
    uint32_t growth;
} regions_entry;

struct mc_world_regions {
    mc_world_regions_limits limits;
    regions_entry entries[MC_WORLD_REGIONS_CACHE_SIZE];
    size_t count, bytes;
    uint64_t temporary_sequence;
};

static mc_world_storage_status regions_error(mc_world_storage_error *error,
                                             mc_world_storage_status status,
                                             const char *operation) {
    if (error != NULL)
        *error = (mc_world_storage_error){status, operation, 0};
    return status;
}

static mc_world_storage_status regions_image_error(mc_world_storage_error *error,
                                                   mc_region_writer_status status) {
    mc_world_storage_status result =
        status == MC_REGION_WRITER_LIMIT || status == MC_REGION_WRITER_RECORD_TOO_LARGE
            ? MC_WORLD_STORAGE_LIMIT
        : status == MC_REGION_WRITER_ALLOCATION_FAILED ? MC_WORLD_STORAGE_ALLOCATION_FAILED
        : status == MC_REGION_WRITER_INVALID_IMAGE     ? MC_WORLD_STORAGE_INVALID_DATA
                                                       : MC_WORLD_STORAGE_INVALID_ARGUMENT;
    return regions_error(error, result, mc_region_writer_status_string(status));
}

static mc_world_storage_status regions_compression_error(mc_world_storage_error *error,
                                                         mc_compression_status status) {
    mc_world_storage_status result =
        status == MC_COMPRESSION_LIMIT               ? MC_WORLD_STORAGE_LIMIT
        : status == MC_COMPRESSION_ALLOCATION_FAILED ? MC_WORLD_STORAGE_ALLOCATION_FAILED
        : status == MC_COMPRESSION_INVALID_ARGUMENT  ? MC_WORLD_STORAGE_INVALID_ARGUMENT
                                                     : MC_WORLD_STORAGE_INVALID_DATA;
    return regions_error(error, result, mc_compression_status_string(status));
}

mc_world_regions_limits mc_world_regions_default_limits(void) {
    const mc_world_regions_limits limits = {128u * 1024u * 1024u, mc_region_writer_default_limits(),
                                            mc_compression_default_limits()};
    return limits;
}

mc_world_storage_status mc_world_regions_create(const mc_world_regions_limits *requested,
                                                mc_world_regions **out,
                                                mc_world_storage_error *error) {
    mc_world_regions_limits limits =
        requested == NULL ? mc_world_regions_default_limits() : *requested;
    if (out == NULL || limits.image.max_image_bytes > MC_REGION_WRITER_MAX_IMAGE_BYTES ||
        limits.compression.max_members == 0)
        return regions_error(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "region cache limits");
    if (limits.max_cached_bytes < sizeof(mc_world_regions))
        return regions_error(error, MC_WORLD_STORAGE_LIMIT, "region cache owner");
    mc_world_regions *regions = calloc(1, sizeof(*regions));
    if (regions == NULL)
        return regions_error(error, MC_WORLD_STORAGE_ALLOCATION_FAILED, "region cache owner");
    regions->limits = limits;
    regions->bytes = sizeof(*regions);
    *out = regions;
    return regions_error(error, MC_WORLD_STORAGE_OK, NULL);
}

void mc_world_regions_clear(mc_world_regions *regions) {
    if (regions == NULL)
        return;
    for (size_t i = 0; i < regions->count; ++i) {
        free(regions->entries[i].path);
        free(regions->entries[i].image);
        regions->entries[i] = (regions_entry){0};
    }
    regions->count = 0;
    regions->bytes = sizeof(*regions);
}

mc_world_regions_usage mc_world_regions_get_usage(const mc_world_regions *regions) {
    const mc_world_regions_usage usage = {regions == NULL ? 0 : regions->count,
                                          regions == NULL ? 0 : regions->bytes};
    return usage;
}

static mc_world_storage_status regions_path(const char *root, int32_t x, int32_t z,
                                            char **directory, char **path,
                                            mc_world_storage_error *error) {
    char filename[64];
    int written = snprintf(filename, sizeof(filename), "r.%" PRId32 ".%" PRId32 ".mcr",
                           mc_java_i32_shr(x, 5), mc_java_i32_shr(z, 5));
    if (written < 0 || (size_t)written >= sizeof(filename))
        return regions_error(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "region coordinates");
    char *parent = NULL;
    mc_world_storage_status status = mc_ws_path_join(root, "region", &parent, error);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    char *complete = NULL;
    status = mc_ws_path_join(parent, filename, &complete, error);
    if (status != MC_WORLD_STORAGE_OK) {
        free(parent);
        return status;
    }
    *directory = parent;
    *path = complete;
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status regions_open(mc_world_regions *regions, const char *root, int32_t x,
                                            int32_t z, regions_entry **out,
                                            mc_world_storage_error *error) {
    char *directory = NULL, *path = NULL;
    mc_world_storage_status status = regions_path(root, x, z, &directory, &path, error);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    for (size_t i = 0; i < regions->count; ++i) {
        if (strcmp(regions->entries[i].path, path) == 0) {
            free(directory);
            free(path);
            *out = &regions->entries[i];
            return MC_WORLD_STORAGE_OK;
        }
    }
    if (regions->count == MC_WORLD_REGIONS_CACHE_SIZE)
        mc_world_regions_clear(regions);
    size_t path_bytes = strlen(path) + 1u;
    if (path_bytes > regions->limits.max_cached_bytes - regions->bytes) {
        free(directory);
        free(path);
        return regions_error(error, MC_WORLD_STORAGE_LIMIT, "region cache path");
    }
    size_t available = regions->limits.max_cached_bytes - regions->bytes - path_bytes;
    size_t read_limit = regions->limits.image.max_image_bytes;
    if (read_limit > available)
        read_limit = available;
    status = mc_ws_mkdirs(directory, error);
    free(directory);
    mc_ws_file_bytes bytes = {0};
    uint32_t growth = 0;
    if (status == MC_WORLD_STORAGE_OK) {
        status = mc_ws_read_file(path, read_limit, &bytes, error);
        if (status == MC_WORLD_STORAGE_MISSING) {
            mc_region_writer *fresh = NULL;
            mc_region_writer_status created =
                mc_region_writer_create(&regions->limits.image, &fresh);
            if (created != MC_REGION_WRITER_OK) {
                status = regions_image_error(error, created);
            } else if (MC_REGION_HEADER_BYTES > available) {
                status = regions_error(error, MC_WORLD_STORAGE_LIMIT, "region cache header");
            } else {
                mc_region_writer_bytes image = mc_region_writer_get_bytes(fresh);
                bytes.data = malloc(image.length);
                if (bytes.data == NULL) {
                    status = regions_error(error, MC_WORLD_STORAGE_ALLOCATION_FAILED,
                                           "region header copy");
                } else {
                    memcpy(bytes.data, image.data, image.length);
                    bytes.length = image.length;
                    status = mc_ws_write_file(path, bytes.data, bytes.length, true, error);
                    if (status == MC_WORLD_STORAGE_OK)
                        growth = mc_region_writer_consume_growth(fresh);
                }
            }
            mc_region_writer_free(fresh);
        }
    }
    if (status != MC_WORLD_STORAGE_OK) {
        mc_ws_free_file_bytes(&bytes);
        free(path);
        return status;
    }
    regions_entry *entry = &regions->entries[regions->count++];
    *entry = (regions_entry){path, bytes.data, bytes.length, growth};
    regions->bytes += path_bytes + bytes.length;
    *out = entry;
    return MC_WORLD_STORAGE_OK;
}

mc_world_storage_status mc_world_regions_read(mc_world_regions *regions, const char *root,
                                              int32_t x, int32_t z, mc_compression_buffer *out,
                                              mc_world_storage_error *error) {
    if (regions == NULL || root == NULL || out == NULL)
        return regions_error(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "region read arguments");
    regions_entry *entry;
    mc_world_storage_status status = regions_open(regions, root, x, z, &entry, error);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    mc_region_view view;
    mc_region_limits limits = {regions->limits.image.max_image_bytes};
    mc_region_status opened = mc_region_open(entry->image, entry->length, &limits, &view);
    if (opened != MC_REGION_OK)
        return regions_error(error, MC_WORLD_STORAGE_INVALID_DATA, mc_region_status_string(opened));
    mc_region_payload payload;
    opened = mc_region_record(&view, (int32_t)((uint32_t)x & UINT32_C(31)),
                              (int32_t)((uint32_t)z & UINT32_C(31)), &payload);
    if (opened != MC_REGION_OK)
        return regions_error(error,
                             opened == MC_REGION_MISSING ? MC_WORLD_STORAGE_MISSING
                                                         : MC_WORLD_STORAGE_INVALID_DATA,
                             mc_region_status_string(opened));
    mc_compression_buffer decoded = {0};
    mc_compression_status decompressed = mc_compression_decode(
        payload.selector == 1 ? MC_COMPRESSION_GZIP : MC_COMPRESSION_ZLIB, payload.data,
        payload.length, &regions->limits.compression, &decoded, NULL);
    if (decompressed != MC_COMPRESSION_OK)
        return regions_compression_error(error, decompressed);
    *out = decoded;
    return regions_error(error, MC_WORLD_STORAGE_OK, NULL);
}

static mc_world_storage_status regions_replace(mc_world_regions *regions, const char *path,
                                               const uint8_t *bytes, size_t length,
                                               mc_world_storage_error *error) {
    size_t path_length = strlen(path);
    if (path_length > REGIONS_PATH_BYTES)
        return regions_error(error, MC_WORLD_STORAGE_LIMIT, "region temporary path");
    char suffix[40];
    for (size_t attempt = 0; attempt < 64; ++attempt) {
        uint64_t sequence = regions->temporary_sequence++;
        int count = snprintf(suffix, sizeof(suffix), ".mc-%016" PRIx64 ".tmp", sequence);
        if (count < 0 || (size_t)count >= sizeof(suffix) ||
            (size_t)count > REGIONS_PATH_BYTES - path_length)
            return regions_error(error, MC_WORLD_STORAGE_LIMIT, "region temporary path");
        size_t total = path_length + (size_t)count;
        char *temporary = malloc(total + 1u);
        if (temporary == NULL)
            return regions_error(error, MC_WORLD_STORAGE_ALLOCATION_FAILED,
                                 "region temporary path");
        memcpy(temporary, path, path_length);
        memcpy(temporary + path_length, suffix, (size_t)count + 1u);
        mc_world_storage_error detail;
        mc_world_storage_status status = mc_ws_write_file(temporary, bytes, length, true, &detail);
        if (status == MC_WORLD_STORAGE_IO_ERROR && detail.system_error == (uint32_t)EEXIST) {
            free(temporary);
            continue;
        }
        if (status == MC_WORLD_STORAGE_OK) {
            status = mc_ws_replace_file(temporary, path, &detail);
            if (status != MC_WORLD_STORAGE_OK) {
                mc_world_storage_error cleanup;
                mc_world_storage_status removed = mc_ws_remove_file(temporary, false, &cleanup);
                if (removed != MC_WORLD_STORAGE_OK) {
                    detail = cleanup;
                    status = removed;
                }
            }
        }
        free(temporary);
        if (error != NULL)
            *error = detail;
        return status;
    }
    return regions_error(error, MC_WORLD_STORAGE_IO_ERROR, "region temporary file collisions");
}

mc_world_storage_status mc_world_regions_write(mc_world_regions *regions, const char *root,
                                               int32_t x, int32_t z, const uint8_t *input,
                                               size_t length, uint32_t timestamp, uint32_t *growth,
                                               mc_world_storage_error *error) {
    if (regions == NULL || root == NULL || growth == NULL || (input == NULL && length != 0))
        return regions_error(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "region write arguments");
    mc_compression_buffer compressed = {0};
    mc_compression_status encoded = mc_compression_encode(
        MC_COMPRESSION_ZLIB, input, length, &regions->limits.compression, &compressed);
    if (encoded != MC_COMPRESSION_OK)
        return regions_compression_error(error, encoded);
    regions_entry *entry;
    mc_world_storage_status status = regions_open(regions, root, x, z, &entry, error);
    mc_region_writer *writer = NULL;
    if (status == MC_WORLD_STORAGE_OK) {
        mc_region_writer_status created =
            mc_region_writer_import(entry->image, entry->length, &regions->limits.image, &writer);
        if (created != MC_REGION_WRITER_OK) {
            status = regions_image_error(error, created);
        } else {
            created = mc_region_writer_write_zlib(writer, (int32_t)((uint32_t)x & UINT32_C(31)),
                                                  (int32_t)((uint32_t)z & UINT32_C(31)),
                                                  compressed.data, compressed.length, timestamp);
            if (created != MC_REGION_WRITER_OK)
                status = regions_image_error(error, created);
        }
    }
    mc_compression_buffer_free(&compressed);
    uint8_t *copy = NULL;
    mc_region_writer_bytes bytes = mc_region_writer_get_bytes(writer);
    if (status == MC_WORLD_STORAGE_OK) {
        size_t retained = regions->bytes - entry->length;
        if (bytes.length > regions->limits.max_cached_bytes - retained) {
            status = regions_error(error, MC_WORLD_STORAGE_LIMIT, "region cache replacement");
        } else {
            copy = malloc(bytes.length);
            if (copy == NULL) {
                status = regions_error(error, MC_WORLD_STORAGE_ALLOCATION_FAILED,
                                       "region cache replacement");
            } else {
                memcpy(copy, bytes.data, bytes.length);
                status = regions_replace(regions, entry->path, copy, bytes.length, error);
            }
        }
    }
    if (status == MC_WORLD_STORAGE_OK) {
        regions->bytes = regions->bytes - entry->length + bytes.length;
        free(entry->image);
        entry->image = copy;
        entry->length = bytes.length;
        *growth = entry->growth + mc_region_writer_consume_growth(writer);
        entry->growth = 0;
        copy = NULL;
    }
    free(copy);
    mc_region_writer_free(writer);
    if (status == MC_WORLD_STORAGE_OK)
        return regions_error(error, status, NULL);
    return status;
}

void mc_world_regions_free(mc_world_regions *regions) {
    if (regions != NULL) {
        mc_world_regions_clear(regions);
        free(regions);
    }
}
