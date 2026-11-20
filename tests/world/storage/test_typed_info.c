#include "mc/world/storage/store.h"

#include "files.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Constructed native NBT/gzip contracts, not original-game golden bytes.
 * The driver supplies one fresh owned UTF-8 root as lowercase hex. All files
 * remain beneath it for retained inspection; this test deletes no files. */
static unsigned int checks;
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #condition);                \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

static const uint16_t title[] = {'T', 0, 0xd800, 'n', 0xdc00, 0xffff};
static const uint16_t root_title[] = {'R', 'e', 't', 'a', 'i', 'n', 'e', 'd'};
static const uint16_t bad_seed[] = {'x'};
static const char *const keys[] = {"RandomSeed", "SpawnX",      "SpawnY",     "SpawnZ",  "Time",
                                   "LastPlayed", "SizeOnDisk",  "LevelName",  "version", "rainTime",
                                   "raining",    "thunderTime", "thundering", "Player"};
static const mc_nbt_type types[] = {
    MC_NBT_LONG,   MC_NBT_INT, MC_NBT_INT, MC_NBT_INT,  MC_NBT_LONG, MC_NBT_LONG, MC_NBT_LONG,
    MC_NBT_STRING, MC_NBT_INT, MC_NBT_INT, MC_NBT_BYTE, MC_NBT_INT,  MC_NBT_BYTE, MC_NBT_COMPOUND};

typedef enum fixture_kind {
    VALID_INFO,
    MISSING_DATA,
    WRONG_DATA,
    BAD_SEED_AND_DIMENSION,
    BAD_NAME_AND_DIMENSION,
    BAD_DIMENSION
} fixture_kind;

static mc_nbt_string ascii(const char *text, uint16_t units[16]) {
    const size_t length = strlen(text);
    for (size_t i = 0; i < length; ++i)
        units[i] = (uint16_t)(unsigned char)text[i];
    return (mc_nbt_string){units, length};
}

static const mc_nbt_tag *get(const mc_nbt_tag *root, const char *key) {
    uint16_t units[16];
    if (root == NULL || root->type != MC_NBT_COMPOUND)
        return NULL;
    const mc_nbt_string name = ascii(key, units);
    return mc_nbt_compound_get(root, name.units, name.length);
}

static bool int_is(const mc_nbt_tag *root, const char *key, int32_t expected) {
    const mc_nbt_tag *tag = get(root, key);
    return tag != NULL && tag->type == MC_NBT_INT && tag->value.int_value == expected;
}

static bool text_is(mc_nbt_string text, const uint16_t *units, size_t length) {
    return text.length == length &&
           (length == 0 ||
            (text.units != NULL && memcmp(text.units, units, length * sizeof(*units)) == 0));
}

static bool same_view(const mc_level_info *a, const mc_level_info *b) {
    return a->seed == b->seed && a->spawn_x == b->spawn_x && a->spawn_y == b->spawn_y &&
           a->spawn_z == b->spawn_z && a->time == b->time && a->last_played == b->last_played &&
           a->size_on_disk == b->size_on_disk && a->version == b->version &&
           a->rain_time == b->rain_time && a->thunder_time == b->thunder_time &&
           a->raining == b->raining && a->thundering == b->thundering &&
           a->player_dimension == b->player_dimension && a->player == b->player &&
           a->level_name.units == b->level_name.units &&
           a->level_name.length == b->level_name.length;
}

static bool fields_are(const mc_world_info *info, int64_t seed) {
    mc_world_info_fields f;
    if (mc_world_info_fields_get(info, &f) != MC_WORLD_INFO_OK)
        return false;
    const mc_nbt_tag *dimension = get(mc_world_info_player_root(f.player), "Dimension");
    return f.value.seed == seed && f.value.spawn_x == -17 && f.value.spawn_y == 80 &&
           f.value.spawn_z == 33 && f.value.time == 9090 && f.value.last_played == -555 &&
           f.value.size_on_disk == 100 && f.value.version == 17 && f.value.rain_time == -9 &&
           f.value.thunder_time == 11 && f.value.raining && !f.value.thundering &&
           f.value.player_dimension == -7 && f.name != NULL &&
           text_is(mc_world_info_name_text(f.name), title, sizeof(title) / sizeof(title[0])) &&
           dimension != NULL && dimension->type == MC_NBT_INT && dimension->value.int_value == -7;
}

static bool encode_fixture(fixture_kind kind, int64_t seed, mc_compression_buffer *out) {
    uint16_t names[18][16];
    mc_nbt_tag fields[14] = {0}, dimension = {.type = MC_NBT_INT};
    mc_nbt_tag root = {.type = MC_NBT_COMPOUND}, data = {.type = MC_NBT_COMPOUND};
    mc_nbt_tag root_extra = {.type = MC_NBT_INT}, data_extra = {.type = MC_NBT_INT};
    const mc_nbt_tag *root_items[2] = {&data, &root_extra};
    const mc_nbt_tag *items[15], *children[1] = {&dimension};
    uint8_t *raw = NULL;
    mc_compression_buffer encoded = {0};
    bool ok = false;
    size_t length = 0, written = 0;
    for (size_t i = 0; i < 14; ++i) {
        fields[i].type = types[i];
        fields[i].name = ascii(keys[i], names[i]);
    }
    fields[0].value.long_value = seed;
    fields[1].value.int_value = -17;
    fields[2].value.int_value = 80;
    fields[3].value.int_value = 33;
    fields[4].value.long_value = 9090;
    fields[5].value.long_value = -555;
    fields[6].value.long_value = 100;
    fields[7].value.string = (mc_nbt_string){title, sizeof(title) / sizeof(title[0])};
    fields[8].value.int_value = 17;
    fields[9].value.int_value = -9;
    fields[10].value.byte_value = -1;
    fields[11].value.int_value = 11;
    fields[12].value.byte_value = 0;
    dimension.name = ascii("Dimension", names[14]);
    dimension.value.int_value = -7;
    fields[13].value.compound.items = children;
    fields[13].value.compound.length = 1;
    /* Put Player first physically. Expected constructor error order must come
     * from the actual named Info reads, not this wire traversal order. */
    items[0] = &fields[13];
    for (size_t i = 0; i < 13; ++i)
        items[i + 1u] = &fields[i];
    items[14] = &data_extra;
    data.name = ascii("Data", names[15]);
    data.value.compound.items = items;
    data.value.compound.length = 15;
    root_extra.name = ascii("RootOnly", names[16]);
    root_extra.value.int_value = -101;
    data_extra.name = ascii("Keep", names[17]);
    data_extra.value.int_value = 4242;
    root.name = (mc_nbt_string){root_title, sizeof(root_title) / sizeof(root_title[0])};
    root.value.compound.items = root_items;
    root.value.compound.length = 2;
    if (kind == MISSING_DATA) {
        root_items[0] = &root_extra;
        root.value.compound.length = 1;
    } else if (kind == WRONG_DATA) {
        data.type = MC_NBT_INT;
        data.value.int_value = 7;
    } else if (kind != VALID_INFO) {
        dimension.type = MC_NBT_BYTE;
        dimension.value.byte_value = -7;
        if (kind == BAD_SEED_AND_DIMENSION) {
            fields[0].type = MC_NBT_STRING;
            fields[0].value.string = (mc_nbt_string){bad_seed, 1};
        } else if (kind == BAD_NAME_AND_DIMENSION) {
            fields[7].type = MC_NBT_LONG;
            fields[7].value.long_value = 88;
        }
    }
    if (mc_nbt_measure(&root, NULL, &length, NULL) != MC_NBT_OK)
        goto cleanup;
    raw = malloc(length);
    if (raw == NULL || mc_nbt_write(&root, NULL, raw, length, &written, NULL) != MC_NBT_OK ||
        written != length ||
        mc_compression_encode(MC_COMPRESSION_GZIP, raw, written, NULL, &encoded) !=
            MC_COMPRESSION_OK)
        goto cleanup;
    *out = encoded;
    encoded = (mc_compression_buffer){0};
    ok = true;
cleanup:
    mc_compression_buffer_free(&encoded);
    free(raw);
    return ok;
}

static bool open_store(const char *root, const char *leaf, mc_world_regions *regions,
                       const mc_world_store_limits *limits, mc_world_store **out) {
    char *path = NULL;
    uint8_t map[256];
    for (size_t i = 0; i < 256; ++i)
        map[i] = (uint8_t)i;
    const mc_world_store_options options = {
        .session_token = INT64_C(0x0123456789abcdef),
        .block_map = map,
        .block_map_length = sizeof(map),
        .limits = limits == NULL ? mc_world_store_default_limits() : *limits};
    bool ok = mc_ws_path_join(root, leaf, &path, NULL) == MC_WORLD_STORAGE_OK &&
              mc_world_store_open(path, regions, &options, out, NULL) == MC_WORLD_STORAGE_OK;
    free(path);
    return ok;
}

static bool write_bytes(const mc_world_store *store, const char *leaf, const uint8_t *bytes,
                        size_t length) {
    char *path = NULL;
    const bool ok =
        mc_ws_path_join(mc_world_store_root(store), leaf, &path, NULL) == MC_WORLD_STORAGE_OK &&
        mc_ws_write_file(path, bytes, length, false, NULL) == MC_WORLD_STORAGE_OK;
    free(path);
    return ok;
}

static bool file_is(const mc_world_store *store, const char *leaf, const uint8_t *expected,
                    size_t length) {
    char *path = NULL;
    mc_ws_file_bytes bytes = {0};
    const bool ok =
        mc_ws_path_join(mc_world_store_root(store), leaf, &path, NULL) == MC_WORLD_STORAGE_OK &&
        mc_ws_read_file(path, 1024u * 1024u, &bytes, NULL) == MC_WORLD_STORAGE_OK &&
        bytes.length == length && (length == 0 || memcmp(bytes.data, expected, length) == 0);
    mc_ws_free_file_bytes(&bytes);
    free(path);
    return ok;
}

static bool file_missing(const mc_world_store *store, const char *leaf) {
    char *path = NULL;
    bool exists = true;
    const bool ok =
        mc_ws_path_join(mc_world_store_root(store), leaf, &path, NULL) == MC_WORLD_STORAGE_OK &&
        mc_ws_path_exists(path, &exists, NULL) == MC_WORLD_STORAGE_OK && !exists;
    free(path);
    return ok;
}

static bool expect_failure(mc_world_store *store, mc_world_info_side side, mc_world_info *sentinel,
                           mc_world_storage_status expected, const char *operation) {
    mc_world_info *out = sentinel;
    mc_world_storage_error error = {0};
    const mc_nbt_tag *root = mc_world_store_metadata_root(store);
    const mc_level_info *view = mc_world_store_info(store);
    const mc_level_info saved = view == NULL ? (mc_level_info){0} : *view;
    uint8_t before[4096], after[4096];
    size_t before_length = 0, after_length = 0;
    bool ok = false;
    if (root != NULL)
        CHECK(mc_nbt_write(root, NULL, before, sizeof(before), &before_length, NULL) == MC_NBT_OK);
    CHECK(mc_world_store_load_world_info(store, side, &out, &error) == expected);
    CHECK(out == sentinel && error.status == expected);
    CHECK(operation == NULL ||
          (error.operation != NULL && strcmp(error.operation, operation) == 0));
    CHECK(mc_world_store_metadata_root(store) == root);
    CHECK((view == NULL && mc_world_store_info(store) == NULL) ||
          (view != NULL && mc_world_store_info(store) == view && same_view(view, &saved)));
    if (root != NULL) {
        CHECK(mc_nbt_write(root, NULL, after, sizeof(after), &after_length, NULL) == MC_NBT_OK);
        CHECK(before_length == after_length && memcmp(before, after, before_length) == 0);
    }
    ok = true;
cleanup:
    if (out != sentinel)
        mc_world_info_release(out);
    return ok;
}

static int test_loads(const char *root, const char *leaf, mc_world_regions *regions,
                      mc_world_info_side side) {
    int result = 1;
    mc_world_store *store = NULL;
    mc_world_info *first = NULL, *second = NULL;
    mc_compression_buffer valid = {0}, backup = {0}, missing = {0}, bad = {0};
    mc_world_info_fields fields;
    mc_world_info_state state;
    mc_world_storage_error error = {0};
    const uint8_t stale[] = {0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10};
    const uint8_t corrupt[] = {0x1f, 0x8b, 0};
    CHECK(open_store(root, leaf, regions, NULL, &store));
    int64_t held_token = -7;
    CHECK(mc_world_store_session_token_get(NULL, &held_token) ==
              MC_WORLD_STORAGE_INVALID_ARGUMENT &&
          held_token == -7);
    CHECK(mc_world_store_session_token_get(store, NULL) == MC_WORLD_STORAGE_INVALID_ARGUMENT);
    CHECK(mc_world_store_session_token_get(store, &held_token) == MC_WORLD_STORAGE_OK &&
          held_token == INT64_C(0x0123456789abcdef));
    CHECK(mc_world_store_info(store) == NULL && mc_world_store_metadata_root(store) == NULL);
    CHECK(expect_failure(store, side, NULL, MC_WORLD_STORAGE_MISSING, NULL));
    CHECK(encode_fixture(VALID_INFO, 111, &valid) && encode_fixture(VALID_INFO, 222, &backup) &&
          encode_fixture(MISSING_DATA, 0, &missing));
    CHECK(write_bytes(store, "level.dat", valid.data, valid.length));
    CHECK(mc_world_store_load_world_info(store, side, &first, &error) == MC_WORLD_STORAGE_OK);
    CHECK(fields_are(first, 111));
    CHECK(mc_world_info_state_get(first, &state) == MC_WORLD_INFO_OK && state.side == side &&
          state.phase == MC_WORLD_INFO_READY);
    CHECK(expect_failure(NULL, side, first, MC_WORLD_STORAGE_INVALID_ARGUMENT, NULL));
    CHECK(expect_failure(store, (mc_world_info_side)99, first, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                         NULL));
    CHECK(mc_world_store_load_world_info(store, side, NULL, &error) ==
          MC_WORLD_STORAGE_INVALID_ARGUMENT);
    CHECK(fields_are(first, 111));
    /* Both invalid fields are present and Player occurs first on the wire.
     * Each operation diagnostic must identify the earlier actual Info read. */
    const fixture_kind failures[] = {BAD_SEED_AND_DIMENSION, BAD_NAME_AND_DIMENSION, BAD_DIMENSION,
                                     WRONG_DATA};
    const char *const operations[] = {"RandomSeed", "LevelName", "Dimension", "Data"};
    for (size_t i = 0; i < sizeof(failures) / sizeof(failures[0]); ++i) {
        CHECK(encode_fixture(failures[i], 333, &bad));
        CHECK(write_bytes(store, "level.dat", bad.data, bad.length));
        CHECK(expect_failure(store, side, first, MC_WORLD_STORAGE_INVALID_DATA, operations[i]));
        CHECK(file_is(store, "level.dat", bad.data, bad.length) && fields_are(first, 111));
        mc_compression_buffer_free(&bad);
    }
    CHECK(write_bytes(store, "level.dat", corrupt, sizeof(corrupt)));
    CHECK(expect_failure(store, side, first, MC_WORLD_STORAGE_INVALID_DATA, NULL));
    CHECK(file_is(store, "level.dat", corrupt, sizeof(corrupt)));
    CHECK(write_bytes(store, "level.dat_old", backup.data, backup.length));
    CHECK(encode_fixture(BAD_NAME_AND_DIMENSION, 333, &bad));
    CHECK(write_bytes(store, "level.dat", bad.data, bad.length));
    CHECK(mc_world_store_load_world_info(store, side, &second, &error) == MC_WORLD_STORAGE_OK);
    CHECK(fields_are(second, 222) && fields_are(first, 111));
    CHECK(file_is(store, "level.dat", bad.data, bad.length) &&
          file_is(store, "level.dat_old", backup.data, backup.length));
    mc_world_info_release(second);
    second = NULL;
    mc_compression_buffer_free(&bad);
    CHECK(encode_fixture(WRONG_DATA, 333, &bad));
    CHECK(write_bytes(store, "level.dat", bad.data, bad.length));
    CHECK(mc_world_store_load_world_info(store, side, &second, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(fields_are(second, 222));
    mc_world_info_release(second);
    second = NULL;
    CHECK(write_bytes(store, "level.dat", missing.data, missing.length));
    CHECK(write_bytes(store, "session.lock", stale, sizeof(stale)));
    CHECK(mc_world_store_check_session(store, &error) == MC_WORLD_STORAGE_SESSION_MISMATCH);
    held_token = -7;
    CHECK(mc_world_store_session_token_get(store, &held_token) == MC_WORLD_STORAGE_OK &&
          held_token == INT64_C(0x0123456789abcdef) &&
          file_is(store, "session.lock", stale, sizeof(stale)));
    CHECK(mc_world_store_load_world_info(store, side, &second, &error) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_info_fields_get(second, &fields) == MC_WORLD_INFO_OK);
    CHECK(fields.value.seed == 0 && fields.value.spawn_x == 0 && fields.value.spawn_y == 0 &&
          fields.value.spawn_z == 0 && fields.value.time == 0 && fields.value.last_played == 0 &&
          fields.value.size_on_disk == 0 && fields.value.version == 0 &&
          fields.value.rain_time == 0 && fields.value.thunder_time == 0 && !fields.value.raining &&
          !fields.value.thundering && fields.value.player_dimension == 0);
    CHECK(fields.name != NULL && mc_world_info_name_text(fields.name).length == 0 &&
          fields.player == NULL);
    CHECK(mc_world_store_metadata_root(store) != NULL && mc_world_store_info(store) != NULL);
    CHECK(get(mc_world_store_metadata_root(store), "Data") == NULL &&
          mc_world_store_info(store)->seed == 0 && mc_world_store_info(store)->player == NULL);
    CHECK(mc_world_store_metadata_root(store) != NULL &&
          text_is(mc_world_store_metadata_root(store)->name, root_title,
                  sizeof(root_title) / sizeof(root_title[0])));
    CHECK(file_is(store, "level.dat", missing.data, missing.length) &&
          file_is(store, "level.dat_old", backup.data, backup.length) &&
          file_is(store, "session.lock", stale, sizeof(stale)));
    CHECK(file_missing(store, "level.dat_new"));
    result = 0;
cleanup:
    mc_world_info_release(second);
    mc_world_info_release(first);
    mc_world_store_free(store);
    mc_compression_buffer_free(&valid);
    mc_compression_buffer_free(&backup);
    mc_compression_buffer_free(&missing);
    mc_compression_buffer_free(&bad);
    return result;
}

static int test_lifetimes(const char *root, const char *leaf, mc_world_regions *regions,
                          mc_world_info_side side) {
    int result = 1;
    mc_world_store *store = NULL;
    mc_world_info *held = NULL, *next = NULL;
    mc_compression_buffer bytes = {0};
    mc_world_info_fields fields;
    const mc_level_info *view = NULL;
    const mc_nbt_tag *document = NULL, *player = NULL, *dimension = NULL;
    CHECK(open_store(root, leaf, regions, NULL, &store));
    CHECK(encode_fixture(VALID_INFO, 444, &bytes));
    CHECK(write_bytes(store, "level.dat", bytes.data, bytes.length));
    CHECK(mc_world_store_load_world_info(store, side, &held, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(fields_are(held, 444));
    view = mc_world_store_info(store);
    document = mc_world_store_metadata_root(store);
    CHECK(view != NULL && document != NULL);
    CHECK(mc_world_info_fields_get(held, &fields) == MC_WORLD_INFO_OK);
    CHECK(view->level_name.units != mc_world_info_name_text(fields.name).units &&
          view->player != mc_world_info_player_root(fields.player));
    const mc_nbt_tag *source_name = get(get(document, "Data"), "LevelName");
    CHECK(source_name != NULL && source_name->type == MC_NBT_STRING &&
          view->level_name.units == source_name->value.string.units &&
          view->player == get(get(document, "Data"), "Player"));
    mc_world_info_release(held);
    held = NULL;
    CHECK(mc_world_store_info(store) == view && mc_world_store_metadata_root(store) == document);
    CHECK(text_is(view->level_name, title, sizeof(title) / sizeof(title[0])));
    dimension = get(view->player, "Dimension");
    CHECK(dimension != NULL && dimension->type == MC_NBT_INT && dimension->value.int_value == -7);
    CHECK(int_is(document, "RootOnly", -101) && int_is(get(document, "Data"), "Keep", 4242));
    CHECK(mc_world_store_load_world_info(store, side, &held, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_info_time_set(held, 12345) == MC_WORLD_INFO_OK &&
          mc_world_info_spawn_set(held, 3, 4, 5) == MC_WORLD_INFO_OK);
    CHECK(mc_world_store_info(store)->time == 9090 && mc_world_store_info(store)->spawn_y == 80);
    mc_compression_buffer_free(&bytes);
    CHECK(encode_fixture(VALID_INFO, 555, &bytes));
    CHECK(write_bytes(store, "level.dat", bytes.data, bytes.length));
    CHECK(mc_world_store_load_world_info(store, side, &next, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(fields_are(next, 555));
    CHECK(mc_world_info_fields_get(held, &fields) == MC_WORLD_INFO_OK && fields.value.seed == 444 &&
          fields.value.time == 12345 && fields.value.spawn_y == 4);
    player = mc_world_info_player_root(fields.player);
    CHECK(text_is(mc_world_info_name_text(fields.name), title, sizeof(title) / sizeof(title[0])) &&
          int_is(player, "Dimension", -7));
    mc_world_info_release(next);
    next = NULL;
    CHECK(mc_world_store_info(store)->seed == 555 &&
          text_is(mc_world_store_info(store)->level_name, title, sizeof(title) / sizeof(title[0])));
    mc_world_store_free(store);
    store = NULL;
    CHECK(mc_world_info_fields_get(held, &fields) == MC_WORLD_INFO_OK && fields.value.seed == 444 &&
          fields.value.time == 12345 && fields.value.spawn_x == 3 && fields.value.spawn_y == 4 &&
          fields.value.spawn_z == 5);
    CHECK(mc_world_info_player_root(fields.player) == player && int_is(player, "Dimension", -7) &&
          text_is(mc_world_info_name_text(fields.name), title, sizeof(title) / sizeof(title[0])));
    CHECK(int_is(mc_world_info_source(held), "Keep", 4242));
    result = 0;
cleanup:
    mc_world_info_release(next);
    mc_world_info_release(held);
    mc_world_store_free(store);
    mc_compression_buffer_free(&bytes);
    return result;
}

static int test_limits(const char *root, const char *leaf, mc_world_regions *regions,
                       mc_world_info_side side, bool file_limit) {
    int result = 1;
    mc_world_store *store = NULL;
    mc_world_info *held = NULL;
    mc_compression_buffer primary = {0}, backup = {0};
    mc_world_store_limits limits = mc_world_store_default_limits();
    CHECK(encode_fixture(VALID_INFO, 666, &primary) && encode_fixture(MISSING_DATA, 0, &backup));
    CHECK(primary.length > backup.length);
    if (file_limit)
        limits.compression.max_input_bytes = backup.length;
    else
        limits.nbt.max_tags = 2; /* Backup is root plus RootOnly; primary exceeds two tags. */
    CHECK(open_store(root, leaf, regions, &limits, &store));
    CHECK(write_bytes(store, "level.dat_old", backup.data, backup.length));
    /* Prove this actual backup is admissible under the SAME fixed limits. */
    CHECK(mc_world_store_load_world_info(store, side, &held, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(write_bytes(store, "level.dat", primary.data, primary.length));
    CHECK(expect_failure(store, side, held, MC_WORLD_STORAGE_LIMIT, NULL));
    CHECK(file_is(store, "level.dat", primary.data, primary.length) &&
          file_is(store, "level.dat_old", backup.data, backup.length));
    result = 0;
cleanup:
    mc_world_info_release(held);
    mc_world_store_free(store);
    mc_compression_buffer_free(&primary);
    mc_compression_buffer_free(&backup);
    return result;
}

static bool decode_path(const char *hex, char *out, size_t capacity) {
    const size_t length = strlen(hex);
    if (length == 0 || length % 2u != 0 || length / 2u >= capacity)
        return false;
    for (size_t i = 0; i < length / 2u; ++i) {
        unsigned int value = 0;
        for (size_t j = 0; j < 2u; ++j) {
            const unsigned char c = (unsigned char)hex[i * 2u + j];
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

int main(int argc, char **argv) {
    char root[8192];
    mc_world_regions *regions = NULL;
    bool exists = false;
    int result = 1;
    if (argc != 3 || strcmp(argv[1], "typed-world-info") != 0 ||
        !decode_path(argv[2], root, sizeof(root)))
        return 1;
    CHECK(mc_ws_path_exists(root, &exists, NULL) == MC_WORLD_STORAGE_OK && !exists);
    CHECK(mc_world_regions_create(NULL, &regions, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(test_loads(root, "client-loads", regions, MC_WORLD_INFO_CLIENT) == 0);
    CHECK(test_loads(root, "server-loads", regions, MC_WORLD_INFO_SERVER) == 0);
    CHECK(test_lifetimes(root, "client-lifetimes", regions, MC_WORLD_INFO_CLIENT) == 0);
    CHECK(test_lifetimes(root, "server-lifetimes", regions, MC_WORLD_INFO_SERVER) == 0);
    CHECK(test_limits(root, "client-file-limit", regions, MC_WORLD_INFO_CLIENT, true) == 0);
    CHECK(test_limits(root, "server-file-limit", regions, MC_WORLD_INFO_SERVER, true) == 0);
    CHECK(test_limits(root, "client-tag-limit", regions, MC_WORLD_INFO_CLIENT, false) == 0);
    CHECK(test_limits(root, "server-tag-limit", regions, MC_WORLD_INFO_SERVER, false) == 0);
    printf("PASS: %u constructed typed WorldInfo load checks (typed-world-info)\n", checks);
    result = 0;
cleanup:
    mc_world_regions_free(regions);
    return result;
}
