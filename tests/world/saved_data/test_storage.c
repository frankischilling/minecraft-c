#include "mc/world/saved_data/storage.h"

#include "files.h"
#include "mc/world/storage/regions.h"
#include "mc/world/storage/store.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Constructed contracts from the paired hc/es ID methods. No clock, allocator,
 * session bypass hook, or original wire-order expectation is supplied here. */
static unsigned int checks;
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #condition);                \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

static const uint16_t key_a[] = {'a'};
static const uint16_t key_d[] = {'d'};
static const uint16_t unicode_key[] = {0, 0xd800, 'n', 0xdc00, 0xffff};
static const mc_nbt_string name_a = {key_a, 1};
static const mc_nbt_string empty_name = {NULL, 0};

/* Raw named Compound, not gzip or a Data wrapper. Duplicate d resolves to -2;
 * x's later Int replaces its earlier Short and is then ignored. Trailing bytes
 * belong to neither the root nor the ID map. Names use literal modified UTF-8. */
static const uint8_t fixture[] = {10,   0,    1,    'I',  2,    0,    1,    'a',  0x7f, 0xff, 1,
                                  0,    1,    'b',  0xff, 2,    0,    1,    'd',  0,    1,    2,
                                  0,    1,    'd',  0xff, 0xfe, 2,    0,    1,    'x',  0,    9,
                                  3,    0,    1,    'x',  0,    0,    0,    7,    2,    0,    0,
                                  0xff, 0xff, 2,    0,    12,   0xc0, 0x80, 0xed, 0xa0, 0x80, 'n',
                                  0xed, 0xb0, 0x80, 0xef, 0xbf, 0xbf, 0x80, 0,    0,    0xde, 0xad};

static bool text_is(mc_nbt_string actual, const uint16_t *units, size_t length)
{
    return actual.length == length &&
           (length == 0 ||
            (actual.units != NULL && memcmp(actual.units, units, length * sizeof(*units)) == 0));
}

static const mc_world_saved_id *find_id(const mc_world_saved_data *owner, bool null_name,
                                        const uint16_t *units, size_t length)
{
    for (size_t i = 0; i < mc_world_saved_data_count(owner); ++i) {
        const mc_world_saved_id *id = mc_world_saved_data_id_at(owner, i);
        if (id != NULL && id->null_name == null_name && text_is(id->name, units, length))
            return id;
    }
    return NULL;
}

static bool id_is(const mc_world_saved_data *owner, bool null_name, const uint16_t *units,
                  size_t length, int16_t value)
{
    const mc_world_saved_id *id = find_id(owner, null_name, units, length);
    return id != NULL && id->value == value;
}

static mc_world_storage_status open_store(const char *root, const char *leaf,
                                          mc_world_regions *regions, mc_world_store **out)
{
    char *path = NULL;
    uint8_t map[256];
    mc_world_storage_error error = {0};
    mc_world_store_options options = {0};
    for (size_t i = 0; i < 256; ++i)
        map[i] = (uint8_t)i;
    options.block_map = map;
    options.block_map_length = sizeof(map);
    options.limits = mc_world_store_default_limits();
    options.session_token = INT64_C(0x0123456789abcdef);
    mc_world_storage_status status = mc_ws_path_join(root, leaf, &path, &error);
    if (status == MC_WORLD_STORAGE_OK)
        status = mc_world_store_open(path, regions, &options, out, &error);
    free(path);
    return status;
}

static mc_world_storage_status id_path(const mc_world_store *store, char **out)
{
    return mc_ws_path_join(mc_world_store_root(store), "data/idcounts.dat", out, NULL);
}

static bool file_is(const char *path, const uint8_t *expected, size_t length)
{
    mc_ws_file_bytes bytes = {0};
    const mc_world_storage_status status = mc_ws_read_file(path, 1024 * 1024, &bytes, NULL);
    const bool equal = status == MC_WORLD_STORAGE_OK && bytes.length == length &&
                       (length == 0 || memcmp(bytes.data, expected, length) == 0);
    mc_ws_free_file_bytes(&bytes);
    return equal;
}

static int test_memory(void)
{
    int result = 1;
    mc_world_saved_data *owner = NULL, *limited = NULL, *candidate = NULL;
    mc_world_saved_data_limits limits = mc_world_saved_data_default_limits();
    mc_world_storage_error error = {0};
    int32_t value = 91;
    uint16_t mutable_units[] = {0, 0xd800, 'n', 0xdc00, 0xffff};
    mc_nbt_string mutable_name = {mutable_units, 5};
    const mc_world_saved_id *held = NULL;
    CHECK(limits.max_ids == 4096 && limits.max_key_units == 65536 &&
          limits.max_allocation_bytes == 4u * 1024u * 1024u);
    CHECK(mc_world_saved_data_count(NULL) == 0 && mc_world_saved_data_id_at(NULL, 0) == NULL &&
          mc_world_saved_data_last_caught(NULL) == NULL);
    mc_world_saved_data_free(NULL);
    CHECK(mc_world_saved_data_create(NULL, NULL, &owner, &error) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_saved_data_count(owner) == 0 && mc_world_saved_data_id_at(owner, 0) == NULL);
    CHECK(mc_world_saved_data_last_caught(owner)->status == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_saved_data_next_id(owner, NULL, &value, &error) == MC_WORLD_STORAGE_OK &&
          value == 0);
    held = mc_world_saved_data_id_at(owner, 0);
    CHECK(held != NULL && held->null_name && held->name.units == NULL && held->name.length == 0);
    /* Exhaust the entire signed Short cycle, with an independent bounded int
     * expectation. No file exists in this NULL-store mode. */
    for (int32_t i = 1; i <= 65536; ++i) {
        const int32_t expected = i < 32768 ? i : i - 65536;
        CHECK(mc_world_saved_data_next_id(owner, NULL, &value, &error) == MC_WORLD_STORAGE_OK);
        CHECK(value == expected && held->value == expected);
        CHECK(mc_world_saved_data_id_at(owner, 0) == held);
    }
    CHECK(value == 0 && mc_world_saved_data_count(owner) == 1);
    CHECK(mc_world_saved_data_next_id(owner, &empty_name, &value, NULL) == MC_WORLD_STORAGE_OK &&
          value == 0);
    CHECK(mc_world_saved_data_count(owner) == 2 && mc_world_saved_data_id_at(owner, 0) == held);
    CHECK(id_is(owner, false, NULL, 0, 0) && id_is(owner, true, NULL, 0, 0));
    CHECK(mc_world_saved_data_next_id(owner, &mutable_name, &value, &error) ==
              MC_WORLD_STORAGE_OK &&
          value == 0);
    memset(mutable_units, 0x55, sizeof(mutable_units));
    CHECK(id_is(owner, false, unicode_key, 5, 0));
    mc_nbt_string equal_name = {unicode_key, 5};
    CHECK(mc_world_saved_data_next_id(owner, &equal_name, &value, &error) == MC_WORLD_STORAGE_OK &&
          value == 1);
    CHECK(mc_world_saved_data_count(owner) == 3 && held->value == 0);
    const mc_nbt_string bad = {NULL, 1}, huge = {key_a, SIZE_MAX};
    value = 91;
    CHECK(mc_world_saved_data_next_id(NULL, NULL, &value, &error) ==
              MC_WORLD_STORAGE_INVALID_ARGUMENT &&
          value == 91);
    CHECK(mc_world_saved_data_next_id(owner, &bad, &value, &error) ==
              MC_WORLD_STORAGE_INVALID_ARGUMENT &&
          value == 91);
    CHECK(mc_world_saved_data_next_id(owner, &huge, &value, &error) ==
              MC_WORLD_STORAGE_INVALID_ARGUMENT &&
          value == 91);
    CHECK(mc_world_saved_data_next_id(owner, NULL, NULL, &error) ==
              MC_WORLD_STORAGE_INVALID_ARGUMENT &&
          held->value == 0);
    CHECK(mc_world_saved_data_count(owner) == 3 &&
          mc_world_saved_data_id_at(owner, SIZE_MAX) == NULL);
    candidate = owner;
    limits.max_allocation_bytes = 0;
    CHECK(mc_world_saved_data_create(NULL, &limits, &candidate, &error) == MC_WORLD_STORAGE_LIMIT &&
          candidate == owner);
    limits = mc_world_saved_data_default_limits();
    limits.nbt.max_depth = 0;
    CHECK(mc_world_saved_data_create(NULL, &limits, &candidate, &error) ==
              MC_WORLD_STORAGE_INVALID_ARGUMENT &&
          candidate == owner);
    limits.nbt.max_depth = 129;
    CHECK(mc_world_saved_data_create(NULL, &limits, &candidate, &error) ==
              MC_WORLD_STORAGE_INVALID_ARGUMENT &&
          candidate == owner);
    limits = mc_world_saved_data_default_limits();
    limits.nbt.max_input_bytes = (size_t)MC_WS_FILE_MAX_BYTES + 1u;
    CHECK(mc_world_saved_data_create(NULL, &limits, &candidate, &error) ==
              MC_WORLD_STORAGE_INVALID_ARGUMENT &&
          candidate == owner);
    limits = mc_world_saved_data_default_limits();
    limits.nbt.max_output_bytes = (size_t)MC_WS_FILE_MAX_BYTES + 1u;
    CHECK(mc_world_saved_data_create(NULL, &limits, &candidate, &error) ==
              MC_WORLD_STORAGE_INVALID_ARGUMENT &&
          candidate == owner);
    CHECK(mc_world_saved_data_create(NULL, NULL, NULL, &error) ==
          MC_WORLD_STORAGE_INVALID_ARGUMENT);
    limits = mc_world_saved_data_default_limits();
    limits.max_ids = 0;
    CHECK(mc_world_saved_data_create(NULL, &limits, &limited, &error) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_saved_data_next_id(limited, NULL, &value, &error) == MC_WORLD_STORAGE_LIMIT &&
          value == 91);
    CHECK(mc_world_saved_data_count(limited) == 0);
    mc_world_saved_data_free(limited);
    limited = NULL;
    limits.max_ids = 2;
    limits.max_key_units = 0;
    CHECK(mc_world_saved_data_create(NULL, &limits, &limited, &error) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_saved_data_next_id(limited, &name_a, &value, &error) == MC_WORLD_STORAGE_LIMIT &&
          value == 91);
    CHECK(mc_world_saved_data_next_id(limited, NULL, &value, &error) == MC_WORLD_STORAGE_OK &&
          value == 0);
    CHECK(mc_world_saved_data_next_id(limited, &empty_name, &value, &error) ==
              MC_WORLD_STORAGE_OK &&
          value == 0);
    CHECK(mc_world_saved_data_next_id(limited, NULL, &value, &error) == MC_WORLD_STORAGE_OK &&
          value == 1);
    CHECK(mc_world_saved_data_count(limited) == 2);
    result = 0;
cleanup:
    if (candidate != owner)
        mc_world_saved_data_free(candidate);
    mc_world_saved_data_free(limited);
    mc_world_saved_data_free(owner);
    return result;
}

static int test_roundtrip(const char *root, mc_world_regions *regions)
{
    int result = 1;
    mc_world_store *store = NULL;
    mc_world_saved_data *owner = NULL, *peer = NULL, *reload = NULL, *candidate = NULL;
    char *path = NULL, *lock = NULL;
    mc_ws_file_bytes session = {0}, after = {0};
    mc_world_storage_error error = {0};
    mc_world_saved_data_limits limits = mc_world_saved_data_default_limits();
    const mc_nbt_string unicode_name = {unicode_key, 5};
    int32_t value = 77;
    const mc_world_saved_id *held = NULL;
    CHECK(open_store(root, "roundtrip", regions, &store) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_store_info(store) == NULL && mc_world_store_metadata_root(store) == NULL);
    CHECK(id_path(store, &path) == MC_WORLD_STORAGE_OK);
    CHECK(mc_ws_path_join(mc_world_store_root(store), "session.lock", &lock, &error) ==
          MC_WORLD_STORAGE_OK);
    CHECK(mc_ws_read_file(lock, 64, &session, &error) == MC_WORLD_STORAGE_OK &&
          session.length == 8);
    CHECK(mc_ws_write_file(path, fixture, sizeof(fixture), false, &error) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_saved_data_create(store, NULL, &owner, &error) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_saved_data_last_caught(owner)->status == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_saved_data_count(owner) == 4);
    CHECK(id_is(owner, false, key_a, 1, 32767) && id_is(owner, false, key_d, 1, -2));
    CHECK(id_is(owner, false, NULL, 0, -1) && id_is(owner, false, unicode_key, 5, -32768));
    CHECK(find_id(owner, true, NULL, 0) == NULL);
    CHECK(mc_world_saved_data_create(store, NULL, &peer, &error) == MC_WORLD_STORAGE_OK &&
          peer != owner);
    held = find_id(owner, false, key_a, 1);
    CHECK(mc_world_saved_data_next_id(owner, &name_a, &value, &error) == MC_WORLD_STORAGE_OK &&
          value == -32768);
    CHECK(find_id(owner, false, key_a, 1) == held && held->value == -32768);
    CHECK(id_is(peer, false, key_a, 1, 32767));
    CHECK(mc_world_saved_data_next_id(owner, &unicode_name, &value, &error) ==
              MC_WORLD_STORAGE_OK &&
          value == -32767);
    CHECK(mc_world_saved_data_next_id(owner, &empty_name, &value, &error) == MC_WORLD_STORAGE_OK &&
          value == 0);
    CHECK(mc_world_saved_data_create(store, NULL, &reload, &error) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_saved_data_count(reload) == 4 && id_is(reload, false, key_a, 1, -32768) &&
          id_is(reload, false, unicode_key, 5, -32767) && id_is(reload, false, key_d, 1, -2));
    CHECK(file_is(lock, session.data, session.length));
    CHECK(mc_world_store_info(store) == NULL && mc_world_store_metadata_root(store) == NULL);
    CHECK(mc_world_store_check_session(store, &error) == MC_WORLD_STORAGE_OK);
    /* Decode-time resource restrictions are native errors, not caught corrupt
     * files. A successful existing owner is an actual unchanged-out sentinel. */
    candidate = peer;
    for (unsigned int mode = 0; mode < 6; ++mode) {
        limits = mc_world_saved_data_default_limits();
        if (mode == 0)
            limits.max_ids = 1;
        if (mode == 1)
            limits.max_key_units = 1;
        if (mode == 2)
            limits.nbt.max_allocation_bytes = 0;
        if (mode == 3)
            limits.nbt.max_input_bytes = 1;
        if (mode == 4)
            limits.nbt.max_tags = 1;
        if (mode == 5)
            limits.nbt.max_depth = 1;
        CHECK(mc_world_saved_data_create(store, &limits, &candidate, &error) ==
                  MC_WORLD_STORAGE_LIMIT &&
              candidate == peer);
    }
    const uint8_t stale[] = {9, 8, 7, 6, 5, 4, 3, 2};
    CHECK(mc_ws_write_file(lock, stale, sizeof(stale), false, &error) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_store_check_session(store, &error) == MC_WORLD_STORAGE_SESSION_MISMATCH);
    CHECK(mc_world_saved_data_next_id(owner, &name_a, &value, &error) == MC_WORLD_STORAGE_OK &&
          value == -32767);
    CHECK(file_is(lock, stale, sizeof(stale)) && mc_world_store_info(store) == NULL);
    CHECK(mc_world_saved_data_last_caught(owner)->status == MC_WORLD_STORAGE_OK);
    mc_world_store_free(store);
    store = NULL;
    CHECK(held->value == -32767 && id_is(peer, false, key_a, 1, 32767));
    CHECK(mc_world_saved_data_next_id(owner, &name_a, &value, &error) == MC_WORLD_STORAGE_OK &&
          value == -32766);
    CHECK(mc_ws_read_file(lock, 64, &after, &error) == MC_WORLD_STORAGE_OK &&
          after.length == sizeof(stale));
    CHECK(memcmp(after.data, stale, sizeof(stale)) == 0);
    result = 0;
cleanup:
    if (candidate != peer)
        mc_world_saved_data_free(candidate);
    mc_world_saved_data_free(owner);
    mc_world_saved_data_free(peer);
    mc_world_saved_data_free(reload);
    mc_ws_free_file_bytes(&session);
    mc_ws_free_file_bytes(&after);
    free(path);
    free(lock);
    mc_world_store_free(store);
    return result;
}

static int test_nullable_file(const char *root, mc_world_regions *regions)
{
    int result = 1;
    mc_world_store *store = NULL;
    mc_world_saved_data *owner = NULL, *reload = NULL;
    mc_world_storage_error error = {0};
    char *path = NULL;
    int32_t value = 33;
    bool exists = true;
    const mc_world_saved_id *held = NULL;
    const uint8_t expected[] = {10, 0, 0, 2, 0, 0, 0, 1, 0};
    /* Either key alone writes an empty tag name. Coexistence currently refuses
     * before opening: the actual original collection winner is still open. */
    for (unsigned int mode = 0; mode < 2; ++mode) {
        const mc_nbt_string *first = mode == 0 ? NULL : &empty_name;
        const mc_nbt_string *second = mode == 0 ? &empty_name : NULL;
        CHECK(open_store(root, mode == 0 ? "nullable-null-first" : "nullable-empty-first", regions,
                         &store) == MC_WORLD_STORAGE_OK);
        CHECK(id_path(store, &path) == MC_WORLD_STORAGE_OK);
        CHECK(mc_world_saved_data_create(store, NULL, &owner, &error) == MC_WORLD_STORAGE_OK);
        CHECK(mc_world_saved_data_count(owner) == 0 &&
              mc_world_saved_data_last_caught(owner)->status == MC_WORLD_STORAGE_OK);
        CHECK(mc_ws_path_exists(path, &exists, &error) == MC_WORLD_STORAGE_OK && !exists);
        CHECK(mc_world_saved_data_next_id(owner, first, &value, &error) == MC_WORLD_STORAGE_OK &&
              value == 0);
        held = mc_world_saved_data_id_at(owner, 0);
        CHECK(held != NULL && held->null_name == (mode == 0));
        CHECK(mc_world_saved_data_next_id(owner, first, &value, &error) == MC_WORLD_STORAGE_OK &&
              value == 1);
        CHECK(file_is(path, expected, sizeof(expected)));
        CHECK(mc_world_saved_data_create(store, NULL, &reload, &error) == MC_WORLD_STORAGE_OK);
        CHECK(mc_world_saved_data_count(reload) == 1 && find_id(reload, true, NULL, 0) == NULL &&
              id_is(reload, false, NULL, 0, 1));
        value = 33;
        CHECK(mc_world_saved_data_next_id(owner, second, &value, &error) ==
                  MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
              value == 33);
        CHECK(mc_world_saved_data_count(owner) == 2 &&
              held == mc_world_saved_data_id_at(owner, 0) && held->value == 1);
        CHECK(id_is(owner, mode != 0, NULL, 0, 0) && file_is(path, expected, sizeof(expected)));
        CHECK(mc_world_saved_data_last_caught(owner)->status == MC_WORLD_STORAGE_OK);
        CHECK(mc_world_saved_data_next_id(owner, second, &value, &error) ==
                  MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
              value == 33);
        CHECK(id_is(owner, true, NULL, 0, 1) && id_is(owner, false, NULL, 0, 1));
        CHECK(file_is(path, expected, sizeof(expected)));
        mc_world_saved_data_free(owner);
        owner = NULL;
        mc_world_saved_data_free(reload);
        reload = NULL;
        mc_world_store_free(store);
        store = NULL;
        free(path);
        path = NULL;
    }
    result = 0;
cleanup:
    mc_world_saved_data_free(owner);
    mc_world_saved_data_free(reload);
    free(path);
    mc_world_store_free(store);
    return result;
}

static int test_caught(const char *root, mc_world_regions *regions)
{
    int result = 1;
    mc_world_store *store = NULL;
    mc_world_saved_data *owner = NULL;
    char *path = NULL;
    mc_world_storage_error error = {0};
    int32_t value = 55;
    const uint8_t broken[] = {10, 0, 0, 2, 0, 1, 'a'};
    const uint8_t empty[] = {10, 0, 0, 0};
    CHECK(open_store(root, "corrupt", regions, &store) == MC_WORLD_STORAGE_OK);
    CHECK(id_path(store, &path) == MC_WORLD_STORAGE_OK);
    CHECK(mc_ws_write_file(path, broken, sizeof(broken), false, &error) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_saved_data_create(store, NULL, &owner, &error) == MC_WORLD_STORAGE_OK &&
          error.status == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_saved_data_count(owner) == 0 &&
          mc_world_saved_data_last_caught(owner)->status == MC_WORLD_STORAGE_INVALID_DATA);
    CHECK(mc_world_saved_data_last_caught(owner)->operation != NULL);
    CHECK(mc_world_saved_data_next_id(owner, NULL, &value, &error) == MC_WORLD_STORAGE_OK &&
          value == 0);
    CHECK(mc_world_saved_data_last_caught(owner)->status == MC_WORLD_STORAGE_OK);
    mc_world_saved_data_free(owner);
    owner = NULL;
    mc_world_store_free(store);
    store = NULL;
    free(path);
    path = NULL;
    CHECK(open_store(root, "directory", regions, &store) == MC_WORLD_STORAGE_OK);
    CHECK(id_path(store, &path) == MC_WORLD_STORAGE_OK &&
          mc_ws_mkdirs(path, &error) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_saved_data_create(store, NULL, &owner, &error) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_saved_data_count(owner) == 0 &&
          mc_world_saved_data_last_caught(owner)->status == MC_WORLD_STORAGE_IO_ERROR);
    CHECK(mc_world_saved_data_next_id(owner, &name_a, &value, &error) == MC_WORLD_STORAGE_OK &&
          value == 0);
    CHECK(mc_world_saved_data_last_caught(owner)->status == MC_WORLD_STORAGE_IO_ERROR &&
          error.status == MC_WORLD_STORAGE_OK);
    CHECK(id_is(owner, false, key_a, 1, 0));
    CHECK(mc_world_saved_data_next_id(owner, &name_a, &value, NULL) == MC_WORLD_STORAGE_OK &&
          value == 1);
    CHECK(id_is(owner, false, key_a, 1, 1));
    mc_world_saved_data_free(owner);
    owner = NULL;
    mc_world_store_free(store);
    store = NULL;
    free(path);
    path = NULL;
    CHECK(open_store(root, "empty-root", regions, &store) == MC_WORLD_STORAGE_OK);
    CHECK(id_path(store, &path) == MC_WORLD_STORAGE_OK);
    CHECK(mc_ws_write_file(path, empty, sizeof(empty), false, &error) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_saved_data_create(store, NULL, &owner, &error) == MC_WORLD_STORAGE_OK &&
          mc_world_saved_data_count(owner) == 0);
    CHECK(mc_world_saved_data_last_caught(owner)->status == MC_WORLD_STORAGE_OK);
    result = 0;
cleanup:
    mc_world_saved_data_free(owner);
    free(path);
    mc_world_store_free(store);
    return result;
}

static int test_write_limits(const char *root, mc_world_regions *regions)
{
    int result = 1;
    mc_world_store *store = NULL;
    mc_world_saved_data *owner = NULL;
    char *path = NULL;
    mc_world_storage_error error = {0};
    mc_world_saved_data_limits limits = mc_world_saved_data_default_limits();
    int32_t value = 1234;
    bool exists = true;
    const uint8_t prefix[] = {10, 0, 0, 2};
    for (unsigned int mode = 0; mode < 5; ++mode) {
        const char *leaf = mode == 0   ? "limit-tags"
                           : mode == 1 ? "limit-depth"
                           : mode == 2 ? "limit-elements"
                           : mode == 3 ? "limit-output"
                                       : "limit-zero-output";
        CHECK(open_store(root, leaf, regions, &store) == MC_WORLD_STORAGE_OK);
        CHECK(id_path(store, &path) == MC_WORLD_STORAGE_OK);
        limits = mc_world_saved_data_default_limits();
        if (mode == 0)
            limits.nbt.max_tags = 1;
        if (mode == 1)
            limits.nbt.max_depth = 1;
        if (mode == 2)
            limits.nbt.max_elements = 0;
        if (mode == 3)
            limits.nbt.max_output_bytes = 4;
        if (mode == 4)
            limits.nbt.max_output_bytes = 0;
        CHECK(mc_world_saved_data_create(store, &limits, &owner, &error) == MC_WORLD_STORAGE_OK);
        value = 1234;
        CHECK(mc_world_saved_data_next_id(owner, &name_a, &value, &error) ==
                  MC_WORLD_STORAGE_LIMIT &&
              value == 1234);
        CHECK(mc_world_saved_data_count(owner) == 1 && id_is(owner, false, key_a, 1, 0));
        CHECK(mc_world_saved_data_last_caught(owner)->status == MC_WORLD_STORAGE_OK);
        CHECK(mc_world_saved_data_next_id(owner, &name_a, &value, &error) ==
                  MC_WORLD_STORAGE_LIMIT &&
              value == 1234);
        CHECK(id_is(owner, false, key_a, 1, 1));
        CHECK(mc_ws_path_exists(path, &exists, &error) == MC_WORLD_STORAGE_OK &&
              exists == (mode >= 3));
        if (mode == 3)
            CHECK(file_is(path, prefix, sizeof(prefix)));
        if (mode == 4)
            CHECK(file_is(path, NULL, 0));
        mc_world_saved_data_free(owner);
        owner = NULL;
        mc_world_store_free(store);
        store = NULL;
        free(path);
        path = NULL;
    }
    result = 0;
cleanup:
    mc_world_saved_data_free(owner);
    free(path);
    mc_world_store_free(store);
    return result;
}

static int test_long_name(const char *root, mc_world_regions *regions)
{
    int result = 1;
    mc_world_store *store = NULL;
    mc_world_saved_data *owner = NULL;
    char *path = NULL;
    uint16_t *units = NULL;
    mc_world_storage_error error = {0};
    int32_t value = 71;
    const uint8_t prefix[] = {10, 0, 0, 2};
    CHECK(open_store(root, "overlong-name", regions, &store) == MC_WORLD_STORAGE_OK);
    CHECK(id_path(store, &path) == MC_WORLD_STORAGE_OK);
    units = malloc(65536u * sizeof(*units));
    CHECK(units != NULL);
    for (size_t i = 0; i < 65536; ++i)
        units[i] = 'q';
    const mc_nbt_string key = {units, 65536};
    CHECK(mc_world_saved_data_create(store, NULL, &owner, &error) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_saved_data_next_id(owner, &key, &value, &error) == MC_WORLD_STORAGE_OK &&
          value == 0);
    CHECK(error.status == MC_WORLD_STORAGE_OK && mc_world_saved_data_count(owner) == 1);
    CHECK(mc_world_saved_data_last_caught(owner)->status == MC_WORLD_STORAGE_INVALID_DATA);
    CHECK(strcmp(mc_world_saved_data_last_caught(owner)->operation, "saved data name writeUTF") ==
          0);
    CHECK(id_is(owner, false, units, 65536, 0) && file_is(path, prefix, sizeof(prefix)));
    CHECK(mc_world_saved_data_next_id(owner, &key, &value, NULL) == MC_WORLD_STORAGE_OK &&
          value == 1);
    CHECK(id_is(owner, false, units, 65536, 1) && file_is(path, prefix, sizeof(prefix)));
    result = 0;
cleanup:
    mc_world_saved_data_free(owner);
    free(units);
    free(path);
    mc_world_store_free(store);
    return result;
}

static int test_output_api(const char *root)
{
    int result = 1;
    char *path = NULL, *directory = NULL;
    mc_ws_output output = {NULL, 777};
    mc_world_storage_error error = {0}, first_error = {0};
    const uint8_t expected[] = {10, 0, 0, 2};
    CHECK(mc_ws_path_join(root, "output-api.dat", &path, &error) == MC_WORLD_STORAGE_OK);
    CHECK(mc_ws_path_join(root, "output-directory", &directory, &error) == MC_WORLD_STORAGE_OK);
    CHECK(mc_ws_mkdirs(directory, &error) == MC_WORLD_STORAGE_OK);
    CHECK(mc_ws_output_open(path, (size_t)MC_WS_FILE_MAX_BYTES + 1u, &output, &error) ==
          MC_WORLD_STORAGE_LIMIT);
    CHECK(output.file == NULL && output.remaining == 777);
    CHECK(mc_ws_output_open(directory, 4, &output, &error) == MC_WORLD_STORAGE_IO_ERROR);
    CHECK(output.file == NULL && output.remaining == 777);
    CHECK(mc_ws_output_open(NULL, 4, &output, &error) == MC_WORLD_STORAGE_INVALID_ARGUMENT);
    CHECK(output.file == NULL && output.remaining == 777);
    CHECK(mc_ws_output_open(path, 4, NULL, &error) == MC_WORLD_STORAGE_INVALID_ARGUMENT);
    CHECK(mc_ws_output_write(NULL, expected, 1, &error) == MC_WORLD_STORAGE_INVALID_ARGUMENT);
    CHECK(mc_ws_output_close(NULL, &error) == MC_WORLD_STORAGE_INVALID_ARGUMENT);
    CHECK(mc_ws_output_open(path, 4, &output, &error) == MC_WORLD_STORAGE_OK &&
          output.file != NULL && output.remaining == 4);
    CHECK(mc_ws_output_write(&output, NULL, 1, &error) == MC_WORLD_STORAGE_INVALID_ARGUMENT &&
          output.remaining == 4);
    CHECK(mc_ws_output_write(&output, NULL, 0, &error) == MC_WORLD_STORAGE_OK &&
          output.remaining == 4);
    CHECK(mc_ws_output_write(&output, expected, 3, &error) == MC_WORLD_STORAGE_OK &&
          output.remaining == 1);
    CHECK(mc_ws_output_write(&output, expected, 2, &first_error) == MC_WORLD_STORAGE_LIMIT &&
          output.remaining == 1);
    CHECK(mc_ws_output_write(&output, expected + 3, 1, &error) == MC_WORLD_STORAGE_OK &&
          output.remaining == 0);
    CHECK(mc_ws_output_close(&output, &error) == MC_WORLD_STORAGE_OK && output.file == NULL &&
          output.remaining == 0);
    CHECK(first_error.status == MC_WORLD_STORAGE_LIMIT &&
          file_is(path, expected, sizeof(expected)));
    CHECK(mc_ws_output_close(&output, &error) == MC_WORLD_STORAGE_INVALID_ARGUMENT);
    CHECK(mc_ws_output_write(&output, expected, 1, &error) == MC_WORLD_STORAGE_INVALID_ARGUMENT);
    CHECK(mc_ws_output_open(path, 0, &output, &error) == MC_WORLD_STORAGE_OK);
    CHECK(mc_ws_output_write(&output, expected, 1, &first_error) == MC_WORLD_STORAGE_LIMIT);
    CHECK(mc_ws_output_close(&output, &error) == MC_WORLD_STORAGE_OK &&
          first_error.status == MC_WORLD_STORAGE_LIMIT);
    CHECK(file_is(path, NULL, 0));
    result = 0;
cleanup:
    if (output.file != NULL)
        (void)mc_ws_output_close(&output, NULL);
    free(path);
    free(directory);
    return result;
}

static bool decode_path(const char *hex, char *out, size_t capacity)
{
    const size_t length = strlen(hex);
    if (length == 0 || length % 2u != 0 || length / 2u >= capacity)
        return false;
    for (size_t i = 0; i < length / 2u; ++i) {
        unsigned int value = 0;
        for (size_t j = 0; j < 2; ++j) {
            const unsigned char c = (unsigned char)hex[2u * i + j];
            const unsigned int digit = c >= '0' && c <= '9'   ? (unsigned int)(c - '0')
                                       : c >= 'a' && c <= 'f' ? (unsigned int)(c - 'a') + 10u
                                                              : 16u;
            if (digit > 15u)
                return false;
            value = value * 16u + digit;
        }
        if (value == 0)
            return false;
        out[i] = (char)(unsigned char)value;
    }
    out[length / 2u] = '\0';
    return true;
}

int main(int argc, char **argv)
{
    char root[8192];
    mc_world_regions *regions = NULL;
    int result = 1;
    if (argc != 3 || strcmp(argv[1], "saved-data-ids") != 0 ||
        !decode_path(argv[2], root, sizeof(root)))
        return 1;
    if (test_memory() != 0 || mc_world_regions_create(NULL, &regions, NULL) != MC_WORLD_STORAGE_OK)
        goto cleanup;
    if (test_roundtrip(root, regions) != 0 || test_nullable_file(root, regions) != 0 ||
        test_caught(root, regions) != 0 || test_write_limits(root, regions) != 0 ||
        test_long_name(root, regions) != 0 || test_output_api(root) != 0)
        goto cleanup;
    printf("PASS: %u constructed saved-data ID checks (saved-data-ids)\n", checks);
    result = 0;
cleanup:
    mc_world_regions_free(regions);
    return result;
}
