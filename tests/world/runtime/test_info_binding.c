#include "mc/world/runtime/runtime.h"

#include "mc/core/java/java.h"

#include <stdio.h>
#include <string.h>

/* Constructed native binding/file contracts. The driver supplies one fresh
 * owned UTF-8 root as hex; files remain available for its independent checks. */
static unsigned int checks;
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #condition);                \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

static const uint16_t title[] = {'I', 0, 0xd800, 'n', 0xdc00, 0xffff};
static const uint16_t before_name[] = {'B', 'e', 'f', 'o', 'r', 'e'};
static const char *const keys[] = {"RandomSeed", "SpawnX",      "SpawnY",     "SpawnZ",  "Time",
                                   "LastPlayed", "SizeOnDisk",  "LevelName",  "version", "rainTime",
                                   "raining",    "thunderTime", "thundering", "Player"};
static const mc_nbt_type types[] = {
    MC_NBT_LONG,   MC_NBT_INT, MC_NBT_INT, MC_NBT_INT,  MC_NBT_LONG, MC_NBT_LONG, MC_NBT_LONG,
    MC_NBT_STRING, MC_NBT_INT, MC_NBT_INT, MC_NBT_BYTE, MC_NBT_INT,  MC_NBT_BYTE, MC_NBT_COMPOUND};

static mc_nbt_string ascii(const char *text, uint16_t units[16]) {
    const size_t length = strlen(text);
    for (size_t i = 0; i < length; ++i)
        units[i] = (uint16_t)(unsigned char)text[i];
    return (mc_nbt_string){units, length};
}

static const mc_nbt_tag *get(const mc_nbt_tag *root, const char *key) {
    uint16_t units[16];
    if (root == NULL)
        return NULL;
    const mc_nbt_string name = ascii(key, units);
    return mc_nbt_compound_get(root, name.units, name.length);
}

static bool text_equal(mc_nbt_string left, mc_nbt_string right) {
    return left.length == right.length &&
           (left.length == 0 ||
            (left.units != NULL && right.units != NULL &&
             memcmp(left.units, right.units, left.length * sizeof(uint16_t)) == 0));
}

static bool tag_name_is(const mc_nbt_tag *tag, const char *text) {
    uint16_t units[16];
    return tag != NULL && text_equal(tag->name, ascii(text, units));
}

static bool same_values(const mc_level_info *a, const mc_level_info *b) {
    return a->seed == b->seed && a->spawn_x == b->spawn_x && a->spawn_y == b->spawn_y &&
           a->spawn_z == b->spawn_z && a->time == b->time && a->last_played == b->last_played &&
           a->size_on_disk == b->size_on_disk && a->player_dimension == b->player_dimension &&
           a->version == b->version && a->rain_time == b->rain_time &&
           a->thunder_time == b->thunder_time && a->raining == b->raining &&
           a->thundering == b->thundering && text_equal(a->level_name, b->level_name);
}

static bool matches_info(const mc_level_info *value, const mc_world_info_fields *fields) {
    const mc_world_info_scalars *v = &fields->value;
    const mc_level_info expected = {.seed = v->seed,
                                    .spawn_x = v->spawn_x,
                                    .spawn_y = v->spawn_y,
                                    .spawn_z = v->spawn_z,
                                    .time = v->time,
                                    .last_played = v->last_played,
                                    .size_on_disk = v->size_on_disk,
                                    .level_name = mc_world_info_name_text(fields->name),
                                    .version = v->version,
                                    .rain_time = v->rain_time,
                                    .thunder_time = v->thunder_time,
                                    .raining = v->raining,
                                    .thundering = v->thundering,
                                    .player_dimension = v->player_dimension};
    return same_values(value, &expected) &&
           value->player == mc_world_info_player_root(fields->player);
}

static bool make_info(mc_world_info **out) {
    mc_nbt_tag fields[14] = {0}, dimension = {.type = MC_NBT_INT}, data = {.type = MC_NBT_COMPOUND};
    uint16_t names[15][16];
    const mc_nbt_tag *items[14], *children[1] = {&dimension};
    mc_world_info_data *owned = NULL;
    mc_world_info *info = NULL;
    mc_world_info_error error = {0};
    bool ok = false;
    for (size_t i = 0; i < 14; ++i) {
        fields[i].type = types[i];
        fields[i].name = ascii(keys[i], names[i]);
        items[i] = &fields[i];
    }
    fields[0].value.long_value = -INT64_C(123456789012345);
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
    data.value.compound.items = items;
    data.value.compound.length = 14;
    if (mc_world_info_data_copy(&data, NULL, &owned, &error) != MC_WORLD_INFO_OK ||
        mc_world_info_allocate(MC_WORLD_INFO_CLIENT, &info) != MC_WORLD_INFO_OK ||
        mc_world_info_construct_nbt(info, owned, &error) != MC_WORLD_INFO_OK)
        goto cleanup;
    *out = info;
    info = NULL;
    ok = true;
cleanup:
    mc_world_info_release(info);
    mc_world_info_data_release(owned);
    return ok;
}

static bool make_wrong_dimension_player(mc_world_info_player **out) {
    uint16_t units[16];
    mc_nbt_tag dimension = {.type = MC_NBT_BYTE}, root = {.type = MC_NBT_COMPOUND};
    const mc_nbt_tag *items[1] = {&dimension};
    dimension.name = ascii("Dimension", units);
    dimension.value.byte_value = -12;
    root.name = (mc_nbt_string){before_name, sizeof(before_name) / sizeof(before_name[0])};
    root.value.compound.items = items;
    root.value.compound.length = 1;
    return mc_world_info_player_copy(&root, NULL, out, NULL) == MC_WORLD_INFO_OK;
}

typedef struct context {
    mc_world_runtime *runtime;
    mc_world_runtime_options options;
    mc_world_info *info; /* Borrowed through caller/runtime holds. */
    const mc_world_info_live *cells;
    mc_chunk_cache cache;
    mc_chunk_cache_entry entries[4];
    mc_chunk_state *list[4];
    mc_chunk_pending pending;
    int32_t keys[4];
    mc_java_int_set *history;
    unsigned int region_calls, metadata_calls, auxiliary_calls, provider_calls;
    bool fail_provider, callback_error;
} context;

static uint32_t region_timestamp(void *owner) {
    context *c = owner;
    ++c->region_calls;
    return UINT32_C(0xfedcba98); /* Explicit existing region-transport fixture input. */
}

static int64_t forbidden_metadata_timestamp(void *owner) {
    context *c = owner;
    ++c->metadata_calls;
    return 0; /* Any invocation fails the Info-bound test; never an Info clock sample. */
}

static mc_chunk_callback_result auxiliary(void *owner, const mc_world_save_context *save) {
    context *c = owner;
    ++c->auxiliary_calls;
    if (save != mc_world_runtime_saving(c->runtime) || save->world != c->runtime)
        c->callback_error = true;
    return MC_CHUNK_CALLBACK_OK;
}

static mc_chunk_callback_result provider(void *owner, const mc_chunk_lifecycle_context *life,
                                         bool *out) {
    context *c = owner;
    ++c->provider_calls;
    if (life != mc_world_runtime_lifecycle(c->runtime) || life->world != c->runtime ||
        mc_world_runtime_free(c->runtime, NULL) != MC_WORLD_STORAGE_BUSY)
        c->callback_error = true;
    if (c->fail_provider) {
        mc_level_info view = {0};
        if (mc_world_runtime_info(c->runtime) != c->info ||
            mc_world_info_live_get(c->info) != c->cells || life->world_time != c->cells->time ||
            *life->world_time != 7001 ||
            mc_world_runtime_acquisition(c->runtime)->spawn_x != c->cells->spawn_x ||
            mc_world_info_time_set(c->info, 7002) != MC_WORLD_INFO_OK ||
            mc_world_info_spawn_set(c->info, 101, -7, 103) != MC_WORLD_INFO_OK ||
            *mc_world_runtime_live(c->runtime)->time != 7002 ||
            *mc_world_runtime_acquisition(c->runtime)->spawn_x != 101 ||
            *mc_world_runtime_acquisition(c->runtime)->spawn_z != 103 ||
            mc_world_runtime_read_info(c->runtime, &view, NULL) != MC_WORLD_STORAGE_OK ||
            view.time != 7002 || view.spawn_y != -7)
            c->callback_error = true;
        return MC_CHUNK_CALLBACK_FATAL;
    }
    *out = false;
    return MC_CHUNK_CALLBACK_OK;
}

static bool context_init(context *c, mc_world_store *store, mc_world_info *info,
                         bool omit_metadata) {
    if (!mc_chunk_cache_init(&c->cache, c->entries, 4, c->list, 4) ||
        mc_java_int_set_create(NULL, &c->history) != MC_JAVA_INT_SET_OK ||
        !mc_chunk_pending_init_ordered(&c->pending, c->keys, 4, c->history))
        return false;
    c->info = info;
    c->options = (mc_world_runtime_options){.store = store,
                                            .cache = &c->cache,
                                            .pending = &c->pending,
                                            .dimension = 0,
                                            .policy = MC_CHUNK_ACQUIRE_DEDICATED,
                                            .provider = c,
                                            .provider_maintenance = provider,
                                            .auxiliary = c,
                                            .save_auxiliary = auxiliary,
                                            .clock = c,
                                            .region_timestamp = region_timestamp,
                                            .metadata_timestamp =
                                                omit_metadata ? NULL : forbidden_metadata_timestamp,
                                            .world_info = info};
    return true;
}

static bool context_close(context *c) {
    /* Only caller-published fixture entries exist; retire their references.
     * No acquired/active chunk, lease or pending key is hidden by this reset. */
    c->cache.count = 0;
    c->cache.insertion_count = 0;
    if (c->history != NULL && mc_chunk_pending_clear(&c->pending) != MC_CHUNK_ACQUIRE_OK)
        return false;
    if (c->runtime != NULL && mc_world_runtime_free(c->runtime, NULL) != MC_WORLD_STORAGE_OK)
        return false;
    c->runtime = NULL;
    c->info = NULL;
    mc_java_int_set_free(c->history);
    c->history = NULL;
    return true;
}

static int test_runtime_binding(const char *root) {
    int result = 1;
    mc_world_regions *regions = NULL;
    mc_world_store *store = NULL, *observer = NULL, *conflict = NULL, *limited = NULL;
    mc_world_info *a = NULL, *b = NULL, *raw = NULL;
    mc_world_info_player *replacement = NULL, *held_player = NULL;
    mc_world_info_name *before = NULL, *held_name = NULL;
    mc_world_info_fields original = {0}, fields = {0}, b_fields = {0};
    mc_world_storage_error error = {0};
    context c[3] = {0};
    uint8_t map[256], blocks[MC_CHUNK_BLOCKS] = {0};
    mc_chunk_data data;
    mc_chunk_state source = {0}, *chunks[3] = {NULL, NULL, NULL};
    mc_world_runtime *rejected = NULL;
    mc_world_store_options options = {.session_token = INT64_C(0x123456789abcdef),
                                      .block_map = map,
                                      .block_map_length = sizeof(map),
                                      .limits = mc_world_store_default_limits()};
    mc_level_info view = {0}, expected = {0}, other = {0};
    const mc_world_runtime_state *live_a = NULL, *live_b = NULL;
    const mc_world_info_live *cells_a = NULL, *cells_b = NULL;
    const mc_nbt_tag *metadata = NULL, *player = NULL;
    uint32_t growth = 777;
    int64_t size_before = 0, b_size = 0, legacy_size = 0, saved_at = 0;
    bool completed = false, provider_result = true, closed = true;
    for (size_t i = 0; i < sizeof(map); ++i)
        map[i] = (uint8_t)i;
    CHECK(mc_world_regions_create(NULL, &regions, &error) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_store_open(root, regions, &options, &store, &error) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_store_info(store) == NULL && mc_world_store_metadata_root(store) == NULL);
    CHECK(make_info(&a));
    CHECK(mc_world_info_fields_get(a, &original) == MC_WORLD_INFO_OK);
    CHECK(mc_world_info_allocate(MC_WORLD_INFO_CLIENT, &b) == MC_WORLD_INFO_OK &&
          mc_world_info_construct_copy(b, a) == MC_WORLD_INFO_OK);
    CHECK(mc_world_info_fields_get(b, &b_fields) == MC_WORLD_INFO_OK &&
          b_fields.name == original.name && b_fields.player == original.player);
    CHECK(mc_world_info_allocate(MC_WORLD_INFO_CLIENT, &raw) == MC_WORLD_INFO_OK &&
          mc_world_info_live_get(raw) == NULL && mc_world_info_live_get(NULL) == NULL);
    CHECK(context_init(&c[0], store, a, false) && context_init(&c[1], store, b, true) &&
          context_init(&c[2], store, raw, true));
    CHECK(mc_world_runtime_create(&c[2].options, &rejected, &error) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          rejected == NULL);
    CHECK(mc_world_info_construct_nbt(raw, NULL, NULL) == MC_WORLD_INFO_INVALID_ARGUMENT &&
          mc_world_info_live_get(raw) == NULL);
    mc_world_info_release(raw);
    raw = NULL;
    c[2].info = NULL;
    c[2].options.world_info = NULL;
    c[2].options.metadata_timestamp = forbidden_metadata_timestamp;
    CHECK(mc_world_runtime_create(&c[0].options, &c[0].runtime, &error) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_create(&c[1].options, &c[1].runtime, &error) == MC_WORLD_STORAGE_OK);
    cells_a = c[0].cells = mc_world_info_live_get(a);
    cells_b = c[1].cells = mc_world_info_live_get(b);
    live_a = mc_world_runtime_live(c[0].runtime);
    live_b = mc_world_runtime_live(c[1].runtime);
    CHECK(cells_a != NULL && cells_b != NULL && cells_a != cells_b &&
          cells_a->time != cells_b->time);
    CHECK(live_a != NULL && live_b != NULL && live_a != live_b &&
          mc_world_runtime_live(c[0].runtime) == live_a && mc_world_info_live_get(a) == cells_a);
    CHECK(mc_world_runtime_info(c[0].runtime) == a && mc_world_runtime_info(c[1].runtime) == b &&
          mc_world_runtime_info(NULL) == NULL);
    CHECK(live_a->time == cells_a->time && live_a->spawn_x == cells_a->spawn_x &&
          live_a->spawn_y == cells_a->spawn_y && live_a->spawn_z == cells_a->spawn_z);
    CHECK(live_b->time == cells_b->time && live_b->spawn_x == cells_b->spawn_x &&
          live_b->spawn_y == cells_b->spawn_y && live_b->spawn_z == cells_b->spawn_z);
    CHECK(live_a->world_force_load != live_b->world_force_load &&
          live_a->provider_force_load != live_b->provider_force_load &&
          live_a->saving_disabled != live_b->saving_disabled);
    CHECK(!*live_a->world_force_load && !*live_a->provider_force_load && !*live_a->saving_disabled);
    CHECK(mc_world_runtime_acquisition(c[0].runtime)->world == c[0].runtime &&
          mc_world_runtime_lifecycle(c[0].runtime)->world == c[0].runtime);
    CHECK(mc_world_runtime_acquisition(c[0].runtime)->world_time == cells_a->time &&
          mc_world_runtime_lifecycle(c[0].runtime)->world_time == cells_a->time &&
          mc_world_runtime_saving(c[0].runtime)->chunks ==
              mc_world_runtime_lifecycle(c[0].runtime));
    CHECK(mc_world_runtime_acquisition(c[0].runtime)->spawn_x == cells_a->spawn_x &&
          mc_world_runtime_acquisition(c[0].runtime)->spawn_z == cells_a->spawn_z);
    mc_world_info_release(a);
    a = NULL; /* Runtime now supplies A's sole Info hold. */
    *live_a->time = 6001;
    *live_a->spawn_x = 71;
    *live_a->spawn_y = 72;
    *live_a->spawn_z = 73;
    CHECK(mc_world_info_fields_get(c[0].info, &fields) == MC_WORLD_INFO_OK &&
          fields.value.time == 6001 && fields.value.spawn_y == 72);
    CHECK(mc_world_info_time_set(c[0].info, 6002) == MC_WORLD_INFO_OK &&
          mc_world_info_spawn_set(c[0].info, -1, -2, -3) == MC_WORLD_INFO_OK);
    CHECK(*live_a->time == 6002 && *live_a->spawn_x == -1 && *live_a->spawn_y == -2 &&
          *live_a->spawn_z == -3);
    CHECK(mc_world_runtime_read_info(c[0].runtime, &view, &error) == MC_WORLD_STORAGE_OK &&
          mc_world_info_fields_get(c[0].info, &fields) == MC_WORLD_INFO_OK &&
          matches_info(&view, &fields));
    CHECK(mc_world_runtime_read_info(c[1].runtime, &other, &error) == MC_WORLD_STORAGE_OK &&
          matches_info(&other, &b_fields));
    CHECK(mc_world_store_info(store) == NULL);
    CHECK(mc_world_info_time_set(c[0].info, 7001) == MC_WORLD_INFO_OK);
    c[0].fail_provider = true;
    CHECK(mc_world_runtime_maintain(c[0].runtime, &provider_result) ==
              MC_CHUNK_LIFECYCLE_CALLBACK_FAILED &&
          provider_result);
    c[0].fail_provider = false;
    CHECK(!c[0].callback_error && c[0].provider_calls == 1 && *live_a->time == 7002 &&
          *live_a->spawn_y == -7 && *live_b->time == 9090);
    CHECK(mc_world_runtime_save(c[0].runtime, true) == MC_CHUNK_LIFECYCLE_CALLBACK_FAILED);
    CHECK(mc_world_runtime_last_error(c[0].runtime)->status == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          strcmp(mc_world_runtime_last_error(c[0].runtime)->operation,
                 "live-player metadata serialization") == 0);
    CHECK(mc_world_runtime_save(c[1].runtime, true) == MC_CHUNK_LIFECYCLE_CALLBACK_FAILED);
    CHECK(c[0].metadata_calls == 0 && c[1].metadata_calls == 0 && c[0].auxiliary_calls == 0 &&
          c[1].auxiliary_calls == 0 && mc_world_store_info(store) == NULL);
    CHECK(mc_chunk_data_init(&data, blocks, sizeof(blocks)));
    source = (mc_chunk_state){.data = &data, .dirty = true, .terrain_populated = true};
    for (size_t i = 0; i < 3; ++i) {
        source.x = (int32_t)i * 32;
        CHECK(mc_world_store_retain_chunk(store, 0, &source, &chunks[i], &error) ==
              MC_WORLD_STORAGE_OK);
    }
    CHECK(mc_world_info_size_set(c[0].info, INT64_MAX - 5) == MC_WORLD_INFO_OK);
    size_before = INT64_MAX - 5;
    CHECK(mc_world_store_write_runtime_chunk_for_info(store, chunks[0], c[0].info, *cells_a->time,
                                                      UINT32_C(0xfedcba98), &growth,
                                                      &error) == MC_WORLD_STORAGE_OK &&
          growth > 5);
    CHECK(mc_world_info_fields_get(c[0].info, &fields) == MC_WORLD_INFO_OK &&
          fields.value.size_on_disk ==
              mc_java_i64_add(size_before, (int64_t)mc_java_i32_narrow((int64_t)growth)));
    CHECK(mc_world_info_fields_get(b, &b_fields) == MC_WORLD_INFO_OK &&
          b_fields.value.size_on_disk == 100 && mc_world_store_info(store) == NULL);
    CHECK(mc_world_info_size_set(b, 7) == MC_WORLD_INFO_OK);
    size_before = fields.value.size_on_disk;
    CHECK(mc_world_store_write_runtime_chunk_for_info(store, chunks[1], b, *cells_b->time,
                                                      UINT32_C(0xfedcba98), &growth,
                                                      &error) == MC_WORLD_STORAGE_OK &&
          growth > 0);
    CHECK(mc_world_info_fields_get(b, &b_fields) == MC_WORLD_INFO_OK &&
          b_fields.value.size_on_disk ==
              mc_java_i64_add(7, (int64_t)mc_java_i32_narrow((int64_t)growth)));
    b_size = b_fields.value.size_on_disk;
    CHECK(mc_world_info_fields_get(c[0].info, &fields) == MC_WORLD_INFO_OK &&
          fields.value.size_on_disk == size_before && mc_world_store_info(store) == NULL);

    CHECK(mc_world_info_name_copy(
              (mc_nbt_string){before_name, sizeof(before_name) / sizeof(before_name[0])}, SIZE_MAX,
              &before) == MC_WORLD_INFO_OK);
    player = mc_world_info_player_root(fields.player);
    CHECK(mc_world_info_player_name_set(fields.player, before) == MC_WORLD_INFO_OK);
    CHECK(mc_world_store_save_world_info(store, c[0].info, &error) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_info_player_root(b_fields.player) == player && tag_name_is(player, "Player") &&
          text_equal(player->name,
                     mc_world_info_name_text(mc_world_info_player_name(fields.player))) &&
          mc_world_info_player_name(fields.player) != before);
    CHECK(mc_world_runtime_read_info(c[0].runtime, &expected, &error) == MC_WORLD_STORAGE_OK &&
          expected.last_played == -555 && expected.version == 17);
    CHECK(mc_world_store_info(store) != NULL && same_values(mc_world_store_info(store), &expected));
    metadata = mc_world_store_metadata_root(store);
    const mc_nbt_tag *saved_data = get(metadata, "Data");
    CHECK(saved_data != NULL && get(saved_data, "LastPlayed") != NULL &&
          get(saved_data, "LastPlayed")->type == MC_NBT_LONG);
    saved_at = get(saved_data, "LastPlayed")->value.long_value;
    CHECK(get(saved_data, "version") != NULL && get(saved_data, "version")->value.int_value == 17);
    CHECK(mc_world_store_open(root, regions, &options, &observer, &error) == MC_WORLD_STORAGE_OK &&
          mc_world_store_load_info(observer, &error) == MC_WORLD_STORAGE_OK);
    expected.last_played = saved_at;
    CHECK(same_values(mc_world_store_info(observer), &expected));
    CHECK(mc_world_runtime_read_info(c[0].runtime, &view, &error) == MC_WORLD_STORAGE_OK &&
          view.last_played == -555 && view.version == 17);
    CHECK(mc_world_store_load_info(store, &error) == MC_WORLD_STORAGE_OK &&
          same_values(mc_world_store_info(store), &expected));
    CHECK(mc_world_runtime_read_info(c[0].runtime, &view, &error) == MC_WORLD_STORAGE_OK &&
          mc_world_store_info(store)->player != view.player);
    metadata = mc_world_store_metadata_root(store);
    CHECK(mc_world_store_save_info(store, &view, 123, &error) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          mc_world_store_metadata_root(store) == metadata);
    CHECK(mc_world_runtime_create(&c[2].options, &c[2].runtime, &error) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_info(c[2].runtime) == NULL);
    const mc_world_runtime_state *legacy = mc_world_runtime_live(c[2].runtime);
    CHECK(legacy->time != cells_a->time && legacy->time != cells_b->time);
    *legacy->time = 8888;
    *legacy->spawn_y = 88;
    CHECK(mc_world_runtime_read_info(c[2].runtime, &view, &error) == MC_WORLD_STORAGE_OK &&
          view.time == 8888 && view.spawn_y == 88 && view.last_played == saved_at);
    CHECK(mc_world_store_info(store)->time != 8888 && *cells_a->time == 7002);
    legacy_size = mc_world_store_info(store)->size_on_disk;
    CHECK(mc_world_info_size_set(c[0].info, 0) == MC_WORLD_INFO_OK &&
          mc_chunk_cache_publish(&c[0].cache, 64, 0, chunks[2]));
    chunks[2]->has_entities = true;
    CHECK(mc_world_runtime_save_chunks(c[0].runtime, false, &completed) == MC_CHUNK_LIFECYCLE_OK &&
          completed);
    CHECK(!chunks[2]->dirty && !chunks[2]->has_entities &&
          chunks[2]->last_save_time == *cells_a->time && c[0].region_calls == 1);
    CHECK(mc_world_info_fields_get(c[0].info, &fields) == MC_WORLD_INFO_OK &&
          fields.value.size_on_disk > 0);
    CHECK(mc_world_info_fields_get(b, &b_fields) == MC_WORLD_INFO_OK &&
          b_fields.value.size_on_disk == b_size &&
          mc_world_store_info(store)->size_on_disk == legacy_size);
    CHECK(make_wrong_dimension_player(&replacement) &&
          mc_world_info_player_set(c[0].info, replacement) == MC_WORLD_INFO_OK);
    CHECK(mc_world_store_save_world_info(store, c[0].info, &error) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_info_fields_get(c[0].info, &fields) == MC_WORLD_INFO_OK &&
          fields.value.player_dimension == -7 && fields.value.last_played == -555);
    CHECK(mc_world_store_info(store)->player_dimension == -7 &&
          mc_world_store_info(store)->last_played == -555 &&
          mc_world_store_info(store)->version == 17);
    saved_data = get(mc_world_store_metadata_root(store), "Data");
    CHECK(get(get(saved_data, "Player"), "Dimension") != NULL &&
          get(get(saved_data, "Player"), "Dimension")->type == MC_NBT_BYTE);
    CHECK(mc_world_info_player_name_set(replacement, before) == MC_WORLD_INFO_OK &&
          mc_world_info_name_set(c[0].info, NULL) == MC_WORLD_INFO_OK);
    metadata = mc_world_store_metadata_root(store);
    CHECK(mc_world_store_save_world_info(store, c[0].info, &error) ==
              MC_WORLD_STORAGE_INVALID_DATA &&
          mc_world_store_metadata_root(store) == metadata);
    CHECK(mc_world_info_player_name(replacement) == before &&
          mc_world_info_fields_get(c[0].info, &fields) == MC_WORLD_INFO_OK && fields.name == NULL &&
          fields.value.last_played == -555);
    CHECK(mc_world_info_name_set(c[0].info, b_fields.name) == MC_WORLD_INFO_OK);
    CHECK(mc_world_runtime_read_info(c[0].runtime, &other, &error) == MC_WORLD_STORAGE_OK);
    mc_world_store_options limited_options = options;
    limited_options.limits.nbt.max_allocation_bytes = 0;
    CHECK(mc_world_store_open(root, regions, &limited_options, &limited, &error) ==
          MC_WORLD_STORAGE_OK);
    CHECK(mc_world_store_info(limited) == NULL && mc_world_store_metadata_root(limited) == NULL);
    CHECK(mc_world_store_save_world_info(limited, c[0].info, &error) == MC_WORLD_STORAGE_LIMIT);
    CHECK(mc_world_store_info(limited) == NULL && mc_world_store_metadata_root(limited) == NULL &&
          mc_world_store_metadata_root(store) == metadata);
    CHECK(mc_world_info_fields_get(c[0].info, &fields) == MC_WORLD_INFO_OK &&
          matches_info(&other, &fields) && fields.name == b_fields.name &&
          fields.player == replacement);
    CHECK(mc_world_info_player_name(replacement) != before &&
          tag_name_is(mc_world_info_player_root(replacement), "Player"));
    CHECK(mc_world_info_player_name_set(replacement, before) == MC_WORLD_INFO_OK);
    mc_world_store_free(limited);
    limited = NULL;
    mc_world_store_options conflict_options = options;
    ++conflict_options.session_token;
    CHECK(mc_world_store_open(root, regions, &conflict_options, &conflict, &error) ==
          MC_WORLD_STORAGE_OK);
    growth = 777;
    size_before = fields.value.size_on_disk;
    CHECK(mc_world_store_write_runtime_chunk_for_info(store, chunks[0], c[0].info, 1, 1, &growth,
                                                      &error) ==
              MC_WORLD_STORAGE_SESSION_MISMATCH &&
          growth == 777);
    CHECK(mc_world_info_fields_get(c[0].info, &fields) == MC_WORLD_INFO_OK &&
          fields.value.size_on_disk == size_before);
    CHECK(mc_world_store_save_world_info(store, c[0].info, &error) ==
              MC_WORLD_STORAGE_SESSION_MISMATCH &&
          mc_world_info_player_name(replacement) == before);
    CHECK(mc_world_info_name_set(c[0].info, NULL) == MC_WORLD_INFO_OK &&
          mc_world_store_save_world_info(store, c[0].info, &error) ==
              MC_WORLD_STORAGE_SESSION_MISMATCH);
    CHECK(mc_world_info_name_set(c[0].info, b_fields.name) == MC_WORLD_INFO_OK);
    CHECK(mc_world_info_retain(c[0].info) == MC_WORLD_INFO_OK);
    a = c[0].info;
    CHECK(mc_world_info_name_retain(b_fields.name) == MC_WORLD_INFO_OK);
    held_name = b_fields.name;
    CHECK(mc_world_info_player_retain(b_fields.player) == MC_WORLD_INFO_OK);
    held_player = b_fields.player;
    for (size_t i = 0; i < 3; ++i)
        CHECK(context_close(&c[i]));
    mc_world_store_free(observer);
    observer = NULL;
    mc_world_store_free(conflict);
    conflict = NULL;
    mc_world_store_free(store);
    store = NULL;
    mc_world_regions_free(regions);
    regions = NULL;
    CHECK(mc_world_info_live_get(a) == cells_a && mc_world_info_live_get(b) == cells_b);
    CHECK(mc_world_info_fields_get(a, &fields) == MC_WORLD_INFO_OK &&
          fields.name == b_fields.name && fields.player == replacement &&
          fields.value.last_played == -555);
    CHECK(mc_world_info_fields_get(b, &original) == MC_WORLD_INFO_OK &&
          original.player == held_player && original.value.time == 9090 &&
          original.value.size_on_disk == b_size);
    CHECK(mc_world_info_source(a) == mc_world_info_source(b));
    mc_world_info_release(a);
    a = NULL;
    mc_world_info_release(b);
    b = NULL;
    CHECK(text_equal(mc_world_info_name_text(held_name),
                     (mc_nbt_string){title, sizeof(title) / sizeof(title[0])}) &&
          get(mc_world_info_player_root(held_player), "Dimension") != NULL &&
          get(mc_world_info_player_root(held_player), "Dimension")->value.int_value == -7);
    result = 0;
cleanup:
    if (rejected != NULL && mc_world_runtime_free(rejected, NULL) != MC_WORLD_STORAGE_OK)
        closed = false;
    for (size_t i = 0; i < 3; ++i)
        if (!context_close(&c[i]))
            closed = false;
    if (closed) {
        mc_world_store_free(observer);
        mc_world_store_free(conflict);
        mc_world_store_free(limited);
        mc_world_store_free(store);
        mc_world_regions_free(regions);
    } else {
        fprintf(stderr, "runtime Info cleanup retained live dependencies\n");
        result = 1;
    }
    mc_world_info_release(a);
    mc_world_info_release(b);
    mc_world_info_release(raw);
    mc_world_info_name_release(before);
    mc_world_info_name_release(held_name);
    mc_world_info_player_release(replacement);
    mc_world_info_player_release(held_player);
    return result;
}

static int test_record_builder(void) {
    int result = 1;
    mc_world_info *info = NULL;
    mc_world_info_player *player = NULL;
    mc_world_info_record *record = NULL, *empty = NULL;
    mc_nbt_document *baseline = NULL, *built = NULL, *without = NULL, *candidate = NULL;
    mc_world_info_fields fields = {0};
    mc_world_info_error info_error = {0};
    mc_level_error error = {0};
    mc_nbt_error nbt = {0};
    mc_nbt_limits limits = mc_nbt_default_limits();
    uint8_t encoded[4096];
    size_t written = 0, consumed = 0;
    uint16_t names[8][16];
    static const uint16_t root_name[] = {'R', 0, 0xdfff};
    static const uint8_t opaque[] = {0, 0xff, 0x80};
    mc_nbt_tag root = {.type = MC_NBT_COMPOUND}, data = {.type = MC_NBT_COMPOUND};
    mc_nbt_tag root_extra = {.type = MC_NBT_BYTE_ARRAY}, data_extra = {.type = MC_NBT_FLOAT};
    mc_nbt_tag bad_time = {.type = MC_NBT_STRING}, bad_version = {.type = MC_NBT_BYTE},
               old_player = {.type = MC_NBT_BYTE};
    const mc_nbt_tag *root_items[2] = {&root_extra, &data};
    const mc_nbt_tag *data_items[4] = {&bad_time, &old_player, &data_extra, &bad_version};
    const mc_nbt_tag *record_root = NULL, *output = NULL, *output_data = NULL;
    int64_t stamp = 0;
    root.name = (mc_nbt_string){root_name, sizeof(root_name) / sizeof(root_name[0])};
    root.value.compound.items = root_items;
    root.value.compound.length = 2;
    data.name = ascii("Data", names[0]);
    data.value.compound.items = data_items;
    data.value.compound.length = 4;
    root_extra.name = ascii("RootUnknown", names[1]);
    root_extra.value.byte_array.data = opaque;
    root_extra.value.byte_array.length = sizeof(opaque);
    data_extra.name = ascii("DataUnknown", names[2]);
    data_extra.value.float_bits = UINT32_C(0x80000000);
    bad_time.name = ascii("Time", names[3]);
    bad_time.value.string = (mc_nbt_string){title, sizeof(title) / sizeof(title[0])};
    bad_version.name = ascii("version", names[4]);
    bad_version.value.byte_value = -1;
    old_player.name = ascii("Player", names[5]);
    old_player.value.byte_value = -1;
    /* The baseline contract requires a successful compound read, not merely a
     * caller's syntactically similar tree. Encode/read only in owned memory. */
    CHECK(mc_nbt_write(&root, NULL, encoded, sizeof(encoded), &written, &nbt) == MC_NBT_OK);
    CHECK(mc_nbt_read_compound(encoded, written, NULL, &baseline, &consumed, &nbt) == MC_NBT_OK &&
          consumed == written);
    CHECK(make_info(&info) && make_wrong_dimension_player(&player));
    CHECK(mc_world_info_player_set(info, player) == MC_WORLD_INFO_OK &&
          mc_world_info_record_create(info, &record, &info_error) == MC_WORLD_INFO_OK);
    record_root = mc_world_info_record_root(record);
    const mc_nbt_tag *clock_field = get(record_root, "LastPlayed");
    CHECK(clock_field != NULL && clock_field->type == MC_NBT_LONG);
    stamp = clock_field->value.long_value;
    CHECK(mc_level_build_record(record_root, mc_nbt_document_root(baseline), NULL, &built,
                                &error) == MC_LEVEL_OK);
    output = mc_nbt_document_root(built);
    output_data = get(output, "Data");
    CHECK(output != NULL && text_equal(output->name, root.name) && output_data != NULL);
    CHECK(get(output, "RootUnknown") != NULL &&
          get(output, "RootUnknown")->type == MC_NBT_BYTE_ARRAY &&
          get(output, "RootUnknown")->value.byte_array.length == sizeof(opaque) &&
          memcmp(get(output, "RootUnknown")->value.byte_array.data, opaque, sizeof(opaque)) == 0);
    CHECK(get(output_data, "DataUnknown") != NULL &&
          get(output_data, "DataUnknown")->type == MC_NBT_FLOAT &&
          get(output_data, "DataUnknown")->value.float_bits == UINT32_C(0x80000000));
    CHECK(get(output_data, "Time") != NULL && get(output_data, "Time")->type == MC_NBT_LONG &&
          get(output_data, "Time")->value.long_value == 9090);
    CHECK(get(output_data, "version") != NULL && get(output_data, "version")->type == MC_NBT_INT &&
          get(output_data, "version")->value.int_value == 17);
    CHECK(get(output_data, "LastPlayed") != NULL &&
          get(output_data, "LastPlayed")->value.long_value == stamp);
    CHECK(get(get(output_data, "Player"), "Dimension") != NULL &&
          get(get(output_data, "Player"), "Dimension")->type == MC_NBT_BYTE);
    const mc_nbt_tag *history = get(mc_nbt_document_root(baseline), "Data");
    CHECK(get(history, "Time")->type == MC_NBT_STRING &&
          get(history, "version")->type == MC_NBT_BYTE &&
          get(history, "Player")->type == MC_NBT_BYTE);
    CHECK(mc_world_info_fields_get(info, &fields) == MC_WORLD_INFO_OK &&
          fields.value.last_played == -555 && fields.value.version == 17 &&
          fields.value.player_dimension == -7);
    CHECK(mc_world_info_player_set(info, NULL) == MC_WORLD_INFO_OK &&
          mc_world_info_record_create(info, &empty, &info_error) == MC_WORLD_INFO_OK);
    const mc_nbt_tag *empty_root = mc_world_info_record_root(empty);
    CHECK(empty_root != NULL && empty_root->value.compound.length == 13);
    CHECK(mc_level_build_record(empty_root, mc_nbt_document_root(baseline), NULL, &without,
                                &error) == MC_LEVEL_OK);
    CHECK(get(get(mc_nbt_document_root(without), "Data"), "Player") == NULL &&
          get(history, "Player")->type == MC_NBT_BYTE);
    CHECK(get(get(mc_nbt_document_root(without), "Data"), "DataUnknown")->value.float_bits ==
          UINT32_C(0x80000000));
    candidate = built;
    for (size_t i = 0; i < 13; ++i) {
        const mc_nbt_tag *children[14];
        for (size_t j = 0; j < 13; ++j)
            children[j] = empty_root->value.compound.items[j];
        mc_nbt_tag wrong = *children[i], edited = *empty_root;
        wrong.type = wrong.type == MC_NBT_BYTE ? MC_NBT_INT : MC_NBT_BYTE;
        memset(&wrong.value, 0, sizeof(wrong.value));
        children[i] = &wrong;
        edited.value.compound.items = children;
        CHECK(mc_level_build_record(&edited, NULL, NULL, &candidate, &error) ==
                  MC_LEVEL_INVALID_FIELD &&
              candidate == built);
    }
    for (size_t i = 0; i < 13; ++i) {
        const mc_nbt_tag *children[13];
        uint16_t extra_key[16];
        for (size_t j = 0; j < 13; ++j)
            children[j] = empty_root->value.compound.items[j];
        mc_nbt_tag renamed = *children[i], edited = *empty_root;
        renamed.name = ascii("Extra", extra_key);
        children[i] = &renamed;
        edited.value.compound.items = children;
        CHECK(mc_level_build_record(&edited, NULL, NULL, &candidate, &error) ==
                  MC_LEVEL_INVALID_FIELD &&
              candidate == built);
        CHECK(error.field != NULL && tag_name_is(empty_root->value.compound.items[i], error.field));
    }
    const mc_nbt_tag *children[14];
    for (size_t i = 0; i < 13; ++i)
        children[i] = empty_root->value.compound.items[i];
    mc_nbt_tag edited = *empty_root, extra = {.type = MC_NBT_BYTE};
    edited.value.compound.items = children;
    edited.value.compound.length = 12;
    CHECK(mc_level_build_record(&edited, NULL, NULL, &candidate, &error) ==
              MC_LEVEL_INVALID_FIELD &&
          candidate == built);
    extra.name = ascii("Extra", names[6]);
    children[13] = &extra;
    edited.value.compound.length = 14;
    CHECK(mc_level_build_record(&edited, NULL, NULL, &candidate, &error) ==
              MC_LEVEL_INVALID_FIELD &&
          candidate == built);
    extra.name = ascii("Player", names[7]);
    CHECK(mc_level_build_record(&edited, NULL, NULL, &candidate, &error) ==
              MC_LEVEL_INVALID_FIELD &&
          candidate == built);
    limits.max_allocation_bytes = 0;
    CHECK(mc_level_build_record(empty_root, mc_nbt_document_root(baseline), &limits, &candidate,
                                &error) == MC_LEVEL_LIMIT &&
          candidate == built);
    limits = mc_nbt_default_limits();
    limits.max_output_bytes = 0;
    CHECK(mc_level_build_record(empty_root, NULL, &limits, &candidate, &error) == MC_LEVEL_LIMIT &&
          candidate == built);
    CHECK(mc_level_build_record(NULL, NULL, NULL, &candidate, &error) ==
              MC_LEVEL_INVALID_ARGUMENT &&
          candidate == built);
    CHECK(mc_level_build_record(empty_root, NULL, NULL, NULL, &error) == MC_LEVEL_INVALID_ARGUMENT);
    mc_world_info_record_release(empty);
    empty = NULL;
    mc_world_info_record_release(record);
    record = NULL;
    mc_world_info_release(info);
    info = NULL;
    mc_world_info_player_release(player);
    player = NULL;
    mc_nbt_document_free(baseline);
    baseline = NULL;
    CHECK(get(get(mc_nbt_document_root(built), "Data"), "LastPlayed")->value.long_value == stamp &&
          get(get(mc_nbt_document_root(built), "Data"), "version")->value.int_value == 17);
    CHECK(get(get(mc_nbt_document_root(without), "Data"), "Player") == NULL);
    result = 0;
cleanup:
    if (candidate != built)
        mc_nbt_document_free(candidate);
    mc_nbt_document_free(built);
    mc_nbt_document_free(without);
    mc_nbt_document_free(baseline);
    mc_world_info_record_release(record);
    mc_world_info_record_release(empty);
    mc_world_info_release(info);
    mc_world_info_player_release(player);
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
    if (argc != 3 || strcmp(argv[1], "world-info-bindings") != 0 ||
        !decode_path(argv[2], root, sizeof(root)))
        return 1;
    if (test_record_builder() != 0 || test_runtime_binding(root) != 0)
        return 1;
    printf("PASS: %u constructed runtime/owner checks (world-info-bindings)\n", checks);
    return 0;
}
