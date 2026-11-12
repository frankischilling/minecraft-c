#ifndef MC_TEST_DEDICATED_PROBE_H
#define MC_TEST_DEDICATED_PROBE_H

#include "../../../../src/world/storage/metadata_internal.h"
#include "dedicated_state_probe.h"

/* The producer accepts constructor boundary inputs, never expected game state.
 * The original observation and its independent audit remain separate processes. */
typedef struct dedicated_run {
    mc_java_random_constructor_input *inputs;
    mc_world_store *preparing;
    mc_world_info *prepared_info, *readback_info;
    mc_world_info_record *record;
    natural_epoch preparation, construction;
    int64_t preparation_token;
    size_t guarded_blocks;
    bool reload_guard_complete;
} dedicated_run;

static bool dedicated_decimal(const uint8_t *text, size_t length, int64_t *out) {
    if (length == 0 || length > 20u)
        return false;
    const bool negative = text[0] == '-';
    size_t i = negative ? 1u : 0u;
    if (i == length || (text[i] == '0' && (length - i != 1u || negative)))
        return false;
    const uint64_t limit = negative ? (UINT64_C(1) << 63) : (uint64_t)INT64_MAX;
    uint64_t magnitude = 0;
    for (; i < length; ++i) {
        if (text[i] < '0' || text[i] > '9')
            return false;
        const uint64_t digit = (uint64_t)(text[i] - '0');
        if (magnitude > (limit - digit) / 10u)
            return false;
        magnitude = magnitude * 10u + digit;
    }
    *out = !negative                          ? (int64_t)magnitude
           : magnitude == (UINT64_C(1) << 63) ? INT64_MIN
                                              : -(int64_t)magnitude;
    return true;
}

static bool dedicated_read_inputs(natural_probe *probe, dedicated_run *run, const char *path) {
    static const char header[] = "minecraft-c-random-constructor-inputs-v1\n";
    mc_ws_file_bytes bytes = {0};
    NATURAL_REQUIRE(
        probe,
        mc_ws_read_file(path, sizeof(header) + 39u * MC_JAVA_RANDOM_PROFILE_MAX_CONSTRUCTOR_INPUTS,
                        &bytes, &probe->storage_error),
        MC_WORLD_STORAGE_OK, "storage", "Constructor input artifact read");
    bool ok =
        bytes.length >= sizeof(header) - 1u && memcmp(bytes.data, header, sizeof(header) - 1u) == 0;
    size_t count = 0;
    for (size_t i = sizeof(header) - 1u; ok && i < bytes.length; ++i)
        if (bytes.data[i] == '\n' && ++count > MC_JAVA_RANDOM_PROFILE_MAX_CONSTRUCTOR_INPUTS)
            ok = false;
    if (ok && count != 0) {
        run->inputs = calloc(count, sizeof(*run->inputs));
        ok = run->inputs != NULL;
    }
    size_t offset = sizeof(header) - 1u, row = 0;
    while (ok && offset < bytes.length) {
        size_t end = offset;
        while (end < bytes.length && bytes.data[end] != '\n')
            ++end;
        if (row >= count || end == bytes.length || end - offset < 18u || end - offset > 37u ||
            bytes.data[offset + 16u] != '\t') {
            ok = false;
            break;
        }
        uint64_t committed = 0;
        for (size_t i = 0; ok && i < 16u; ++i) {
            const uint8_t c = bytes.data[offset + i];
            const unsigned int digit = c >= '0' && c <= '9'   ? (unsigned int)(c - '0')
                                       : c >= 'a' && c <= 'f' ? (unsigned int)(c - 'a') + 10u
                                                              : 16u;
            if (digit > 15u)
                ok = false;
            else
                committed = (committed << 4) | digit;
        }
        int64_t sample = 0;
        ok = ok && dedicated_decimal(bytes.data + offset + 17u, end - offset - 17u, &sample);
        if (ok)
            run->inputs[row++] = (mc_java_random_constructor_input){committed, sample};
        offset = end + 1u;
    }
    mc_ws_free_file_bytes(&bytes);
    if (!ok || row != count)
        return problem(probe, "Constructor input artifact shape", "probe", 1u);
    probe->dedicated->inputs = run->inputs;
    probe->dedicated->input_count = count;
    return true;
}

static bool dedicated_prepare(natural_probe *probe, dedicated_run *run) {
    mc_world_store_options options = {.block_map = probe->block_map,
                                      .block_map_length = sizeof(probe->block_map),
                                      .limits = mc_world_store_default_limits()};
    NATURAL_REQUIRE(probe, epoch_sample(probe, &run->preparation.low, &run->preparation.low_valid),
                    true, "clock", "Preparation epoch start");
    NATURAL_REQUIRE(probe, mc_platform_current_time_millis(&run->preparation_token),
                    MC_PLATFORM_MILLIS_OK, "clock", "Preparation Store session clock");
    options.session_token = run->preparation_token;
    NATURAL_REQUIRE(probe,
                    mc_world_store_open(probe->persistence->store_root, probe->regions, &options,
                                        &run->preparing, &probe->storage_error),
                    MC_WORLD_STORAGE_OK, "storage", "Actual preparation Store");
    NATURAL_REQUIRE(probe, mc_world_info_allocate(MC_WORLD_INFO_SERVER, &run->prepared_info),
                    MC_WORLD_INFO_OK, "info", "Preparation Info allocation");
    NATURAL_REQUIRE(probe, mc_world_info_construct(run->prepared_info, 23, probe->name),
                    MC_WORLD_INFO_OK, "info", "Preparation Info constructor");
    NATURAL_REQUIRE(probe, mc_world_info_spawn_set(run->prepared_info, 0, 64, 0), MC_WORLD_INFO_OK,
                    "info", "Preparation Info spawn API");
    /* The real empty-Player handler route first assigns this version, then
     * selects NULL and enters the common Info writer. No World is edited. */
    NATURAL_REQUIRE(probe, mc_world_info_version_set(run->prepared_info, 19132), MC_WORLD_INFO_OK,
                    "info", "Preparation handler version prefix");
    NATURAL_REQUIRE(probe, mc_world_store_check_session(run->preparing, &probe->storage_error),
                    MC_WORLD_STORAGE_OK, "storage", "Preparation native session admission");
    NATURAL_REQUIRE(
        probe,
        mc_world_info_record_create_with_player(run->prepared_info, NULL, &run->record, NULL),
        MC_WORLD_INFO_OK, "info", "Preparation actual empty-Player Info writer");
    NATURAL_REQUIRE(probe,
                    mc_world_store_write_info_record(run->preparing, run->prepared_info,
                                                     run->record, &probe->storage_error),
                    MC_WORLD_STORAGE_OK, "storage", "Preparation metadata transport");
    mc_world_info_record_release(run->record);
    run->record = NULL;
    NATURAL_REQUIRE(probe,
                    mc_world_store_load_world_info(run->preparing, MC_WORLD_INFO_SERVER,
                                                   &run->readback_info, &probe->storage_error),
                    MC_WORLD_STORAGE_OK, "storage", "Preparation actual Info readback");
    mc_world_info_fields readback;
    NATURAL_REQUIRE(probe, mc_world_info_fields_get(run->readback_info, &readback),
                    MC_WORLD_INFO_OK, "info", "Preparation readback fields");
    if (readback.value.seed != 23 || readback.value.spawn_x != 0 || readback.value.spawn_y != 64 ||
        readback.value.spawn_z != 0)
        return problem(probe, "Preparation metadata readback mismatch", "probe", 1u);
    NATURAL_REQUIRE(probe, mc_world_store_free_checked(&run->preparing, &probe->storage_error),
                    MC_WORLD_STORAGE_OK, "storage", "Preparation Store close");
    return epoch_sample(probe, &run->preparation.high, &run->preparation.high_valid);
}

static bool dedicated_initialize(natural_probe *probe, dedicated_run *run, const char *resources,
                                 const char *root, const mc_math_trig *trig) {
    NATURAL_REQUIRE(probe, mc_resource_files_create(resources, &probe->resources), MC_REGISTRY_OK,
                    "registry", "Resource source");
    NATURAL_REQUIRE(probe, mc_materials_create(&probe->materials), true, "probe", "Materials");
    NATURAL_REQUIRE(probe, mc_java_random_profile_create(SIZE_MAX, &probe->profile),
                    MC_JAVA_RANDOM_PROFILE_OK, "random_profile", "Application Random profile");
    NATURAL_REQUIRE(probe,
                    mc_java_random_profile_bind_constructor_inputs(probe->profile, run->inputs,
                                                                   probe->dedicated->input_count),
                    MC_JAVA_RANDOM_PROFILE_OK, "random_profile", "Measured constructor inputs");
    const mc_registry_options registry = {.side = MC_REGISTRY_SERVER,
                                          .random_profile = probe->profile,
                                          .materials = probe->materials,
                                          .resources = mc_resource_files_source(probe->resources)};
    NATURAL_REQUIRE(
        probe, mc_registry_classes_allocate(&registry, &probe->registry, &probe->registry_error),
        MC_REGISTRY_OK, "registry", "Cold registry allocation");
    NATURAL_REQUIRE(
        probe, mc_entity_classes_create(MC_ENTITY_SERVER, probe->profile, NULL, &probe->entities),
        MC_ENTITY_OK, "entity", "Cold Entity class binding");
    NATURAL_REQUIRE(probe,
                    mc_world_runtime_classes_create(MC_WORLD_INFO_SERVER, probe->profile, trig,
                                                    &probe->classes, &probe->storage_error),
                    MC_WORLD_STORAGE_OK, "storage", "World class binding");
    NATURAL_REQUIRE(probe,
                    mc_world_runtime_classes_bind_registry(probe->classes, probe->registry,
                                                           &probe->registry_error),
                    MC_REGISTRY_OK, "registry", "Cold World Registry binding");
    NATURAL_REQUIRE(probe, mc_world_runtime_classes_bind_entities(probe->classes, probe->entities),
                    MC_ENTITY_OK, "entity", "Cold World Entity binding");
    bool exists = true;
    NATURAL_REQUIRE(probe, mc_ws_path_exists(root, &exists, &probe->storage_error),
                    MC_WORLD_STORAGE_OK, "storage", "Fresh dedicated root admission");
    if (exists)
        return problem(probe, "Dedicated root already exists", "probe", 1u);
    /* A transport identity table permits cold source class order. Before the
     * one reload, an independent bounded read verifies every saved Blocks byte
     * in this finite profile against the then-READY actual registry. */
    for (size_t i = 0; i < sizeof(probe->block_map); ++i)
        probe->block_map[i] = (uint8_t)i;
    NATURAL_REQUIRE(
        probe,
        mc_ws_path_join(root, "world", &probe->persistence->store_root, &probe->storage_error),
        MC_WORLD_STORAGE_OK, "storage", "Dedicated Store root binding");
    NATURAL_REQUIRE(probe, mc_world_regions_create(NULL, &probe->regions, &probe->storage_error),
                    MC_WORLD_STORAGE_OK, "storage", "Region owner");
    static const char title[] = "dedicated-population-reference";
    uint16_t units[sizeof(title) - 1u];
    for (size_t i = 0; i < sizeof(units) / sizeof(units[0]); ++i)
        units[i] = (uint16_t)(unsigned char)title[i];
    NATURAL_REQUIRE(
        probe,
        mc_world_info_name_copy((mc_nbt_string){units, sizeof(units) / sizeof(units[0])}, SIZE_MAX,
                                &probe->name),
        MC_WORLD_INFO_OK, "info", "Dedicated World title");
    if (!dedicated_prepare(probe, run) ||
        !epoch_sample(probe, &run->construction.low, &run->construction.low_valid))
        return false;
    NATURAL_REQUIRE(probe, mc_platform_current_time_millis(&probe->persistence->session_token),
                    MC_PLATFORM_MILLIS_OK, "clock", "New live Store session clock");
    mc_world_store_options store = {.session_token = probe->persistence->session_token,
                                    .block_map = probe->block_map,
                                    .block_map_length = sizeof(probe->block_map),
                                    .limits = mc_world_store_default_limits()};
    store.limits.max_loaded_chunks = NATURAL_CAPACITY;
    NATURAL_REQUIRE(probe,
                    mc_world_store_open(probe->persistence->store_root, probe->regions, &store,
                                        &probe->store, &probe->storage_error),
                    MC_WORLD_STORAGE_OK, "storage", "New live dedicated Store");
    NATURAL_REQUIRE(probe, mc_server_runtime_allocate(SIZE_MAX, &probe->dedicated->server, NULL),
                    MC_SERVER_OK, "server", "Actual Server allocation");
    NATURAL_REQUIRE(probe, mc_server_runtime_construct(probe->dedicated->server, NULL),
                    MC_SERVER_OK, "server", "Actual Server default constructor");
    mc_world_runtime_construction_limits limits = mc_world_runtime_default_construction_limits();
    limits.max_provider_chunks = NATURAL_CAPACITY;
    NATURAL_REQUIRE(probe,
                    mc_world_runtime_allocate_world(MC_WORLD_RUNTIME_SERVER_DEDICATED,
                                                    probe->classes, &limits, &probe->world,
                                                    &probe->storage_error),
                    MC_WORLD_STORAGE_OK, "storage", "Actual dedicated World allocation");
    const mc_world_runtime_world_arguments arguments = {.store = probe->store,
                                                        .name = probe->name,
                                                        .seed = 23,
                                                        .server = probe->dedicated->server,
                                                        .dimension_id = 0};
    probe->constructor_called = true;
    probe->constructor_status =
        mc_world_runtime_construct_world(probe->world, &arguments, &probe->storage_error);
    (void)epoch_sample(probe, &run->construction.high, &run->construction.high_valid);
    if (probe->constructor_status != MC_WORLD_STORAGE_OK)
        return problem(probe, "Actual dedicated World constructor", "storage",
                       (unsigned int)probe->constructor_status);
    mc_world_runtime_fields fields;
    NATURAL_REQUIRE(probe, mc_world_runtime_fields_get(probe->world, &fields), MC_WORLD_STORAGE_OK,
                    "storage", "Actual loaded dedicated World fields");
    if (fields.s || fields.x == NULL)
        return problem(probe, "Dedicated constructor did not select existing metadata", "probe",
                       1u);
    NATURAL_REQUIRE(probe, mc_world_info_retain(fields.x), MC_WORLD_INFO_OK, "info",
                    "Actual World Info observation hold");
    probe->persistence->info = fields.x;
    return true;
}

static bool dedicated_reload_guard(natural_probe *probe, dedicated_run *run) {
    mc_registry_block_view blocks;
    NATURAL_REQUIRE(
        probe, mc_registry_classes_inspect_blocks(probe->registry, &blocks, &probe->registry_error),
        MC_REGISTRY_OK, "registry", "Reload actual block-registry view");
    if (blocks.phase != MC_REGISTRY_READY || blocks.m == NULL)
        return problem(probe, "Reload block registry is not READY", "probe", 1u);
    static const int32_t slots[][2] = {{0, 0}, {1, 0}, {0, 1}, {1, 1}, {16, 0}};
    mc_world_regions *reader = NULL;
    NATURAL_REQUIRE(probe, mc_world_regions_create(NULL, &reader, &probe->inspection_error),
                    MC_WORLD_STORAGE_OK, "storage", "Reload guard independent reader");
    bool ok = true;
    for (size_t i = 0; ok && i < sizeof(slots) / sizeof(slots[0]); ++i) {
        mc_compression_buffer bytes = {0};
        mc_stored_chunk *stored = NULL;
        size_t consumed = 0;
        ok = mc_world_regions_read(reader, probe->persistence->store_root, slots[i][0], slots[i][1],
                                   &bytes, &probe->inspection_error) == MC_WORLD_STORAGE_OK &&
             mc_stored_chunk_decode(bytes.data, bytes.length, NULL, &stored, &consumed, NULL) ==
                 MC_STORED_CHUNK_OK &&
             consumed == bytes.length;
        const mc_chunk_state *state = mc_stored_chunk_state(stored);
        if (ok)
            ok = state != NULL && state->data != NULL && state->data->blocks != NULL &&
                 state->data->blocks_capacity == MC_CHUNK_BLOCKS;
        if (ok)
            for (size_t j = 0; j < MC_CHUNK_BLOCKS; ++j) {
                const uint8_t id = state->data->blocks[j];
                if (id != 0 && blocks.m[id] == NULL) {
                    ok = false;
                    break;
                }
                ++run->guarded_blocks;
            }
        mc_stored_chunk_free(stored);
        mc_compression_buffer_free(&bytes);
    }
    mc_world_regions_free(reader);
    run->reload_guard_complete = ok;
    return ok || problem(probe, "Reload finite saved-ID identity-map guard", "probe", 1u);
}

static bool dedicated_state(natural_probe *probe) {
    const size_t start = probe->json.length;
    if (dedicated_boundary(probe))
        return true;
    problem(probe, "Dedicated boundary observation", "probe", 1u);
    if (probe->json.failed)
        return false;
    probe->json.length = start;
    if (probe->json.bytes != NULL)
        probe->json.bytes[start] = '\0';
    return emit(&probe->json, "null");
}

static bool dedicated_preparation_info(natural_probe *probe, const mc_world_info *info) {
    if (info == NULL)
        return emit(&probe->json, "null");
    mc_world_info_fields fields;
    if (mc_world_info_fields_get(info, &fields) != MC_WORLD_INFO_OK || fields.player != NULL)
        return problem(probe, "Preparation held Info observation", "probe", 1u);
    const mc_world_info_scalars *value = &fields.value;
    natural_json *json = &probe->json;
    return emit(json, "{\"owner\":") && dedicated_ref(probe, info, D_INFO) &&
           emit(json,
                ",\"seed\":\"%016" PRIx64 "\",\"spawn_x\":%" PRId32 ",\"spawn_y\":%" PRId32
                ",\"spawn_z\":%" PRId32 ",\"time\":\"%016" PRIx64 "\",\"last_played\":\"%016" PRIx64
                "\",\"size_on_disk\":\"%016" PRIx64
                "\",\"player\":null,\"player_dimension\":%" PRId32 ",\"level_name\":",
                (uint64_t)value->seed, value->spawn_x, value->spawn_y, value->spawn_z,
                (uint64_t)value->time, (uint64_t)value->last_played, (uint64_t)value->size_on_disk,
                value->player_dimension) &&
           dedicated_name(probe, fields.name) &&
           emit(json,
                ",\"version\":%" PRId32 ",\"raining\":%s,\"rain_time\":%" PRId32
                ",\"thundering\":%s,\"thunder_time\":%" PRId32 "}",
                value->version, boolean(value->raining), value->rain_time,
                boolean(value->thundering), value->thunder_time);
}

static bool dedicated_operation(natural_probe *probe, dedicated_run *run, size_t index) {
    static const char *const actions[] = {"origin_c",
                                          "origin_c",
                                          "origin_c",
                                          "origin_c",
                                          "origin_c",
                                          "world_time_1234",
                                          "full_world_save_0",
                                          "full_world_save_1",
                                          "protected_drop_0_0",
                                          "far_c",
                                          "far_c",
                                          "far_c",
                                          "far_c",
                                          "drop_16_0",
                                          "cache_maintenance",
                                          "reload_c_16_0",
                                          "repeated_c_16_0"};
    static const int32_t coordinates[][2] = {{0, 0},  {1, 0},  {0, 1}, {1, 1},  {1, 1},  {0, 0},
                                             {0, 0},  {0, 0},  {0, 0}, {16, 0}, {17, 0}, {16, 1},
                                             {17, 1}, {16, 0}, {0, 0}, {16, 0}, {16, 0}};
    natural_epoch epoch = {0};
    mc_chunk_state *result = NULL;
    bool provider_result = false;
    unsigned int status = 0;
    const char *domain = NULL;
    if (index == 15u && !dedicated_reload_guard(probe, run))
        return false;
    if (!epoch_sample(probe, &epoch.low, &epoch.low_valid))
        return false;
    if (index == 5u) {
        domain = "info";
        status = (unsigned int)mc_world_info_time_set(probe->persistence->info, 1234);
    } else if (index == 6u || index == 7u) {
        domain = "lifecycle";
        status = (unsigned int)mc_world_runtime_save_detailed(probe->world, true,
                                                              &probe->persistence->save_error);
    } else if (index == 8u || index == 13u) {
        domain = "acquisition";
        status = (unsigned int)mc_world_runtime_mark_drop(probe->world, coordinates[index][0],
                                                          coordinates[index][1]);
    } else if (index == 14u) {
        domain = "lifecycle";
        status = (unsigned int)mc_world_runtime_maintain_detailed(probe->world, &provider_result,
                                                                  &probe->persistence->save_error);
    } else {
        domain = "acquisition";
        status = (unsigned int)mc_world_runtime_acquire(probe->world, coordinates[index][0],
                                                        coordinates[index][1], &result);
        if (status == 0u && index == 9u)
            probe->dedicated->old_far = result;
        if (status == 0u && index == 15u)
            probe->dedicated->reloaded_far = result;
    }
    (void)epoch_sample(probe, &epoch.high, &epoch.high_valid);
    if (status != 0u)
        problem(probe, actions[index], domain, status);
    natural_json *json = &probe->json;
    if (!emit(json, "%s{\"index\":%zu,\"action\":", index == 0u ? "" : ",", index) ||
        !string(json, actions[index]) || !emit(json, ",\"epoch_ms\":") ||
        !epoch_projection(json, &epoch) || !emit(json, ",\"native_domain\":") ||
        !string(json, domain) || !emit(json, ",\"native_status\":%u,\"result\":", status))
        return false;
    if (result != NULL) {
        if (!dedicated_ref(probe, result, D_CHUNK))
            return false;
    } else if (index == 14u && status == 0u) {
        if (!emit(json, "%s", boolean(provider_result)))
            return false;
    } else if (!emit(json, "null"))
        return false;
    if (!emit(json, ",\"error\":"))
        return false;
    if (status != 0u && strcmp(domain, "lifecycle") == 0) {
        if (!save_diagnostic(json, &probe->persistence->save_error))
            return false;
    } else if (status != 0u) {
        if (!diagnostic(probe))
            return false;
    } else if (!emit(json, "null"))
        return false;
    return emit(json, ",\"after\":") && dedicated_state(probe) && emit(json, "}");
}

static bool dedicated_observe(natural_probe *probe, dedicated_run *run, bool initialized) {
    natural_json *json = &probe->json;
    if (!emit(json, "{\"schema\":2,\"scope\":\"native-game-dedicated-natural-population-unload\","
                    "\"profile\":\"dedicated-natural-unload\",\"side\":\"server\","
                    "\"seed_hex\":\"0000000000000017\",\"construction_epoch_ms\":") ||
        !epoch_projection(json, &run->construction) || !emit(json, ",\"preparation_epoch_ms\":") ||
        !epoch_projection(json, &run->preparation) ||
        !emit(json,
              ",\"preparation_session_token\":\"%016" PRIx64 "\","
              "\"live_session_token\":\"%016" PRIx64 "\",\"prepared_info\":",
              (uint64_t)run->preparation_token, (uint64_t)probe->persistence->session_token) ||
        !dedicated_preparation_info(probe, run->prepared_info) ||
        !emit(json, ",\"preparation_readback_info\":") ||
        !dedicated_preparation_info(probe, run->readback_info) || !emit(json, ",\"initial\":") ||
        !(initialized ? dedicated_state(probe) : emit(json, "null")) ||
        !emit(json, ",\"operations\": ["))
        return false;
    for (size_t i = 0; initialized && probe->failed_stage == NULL && i < 17u; ++i)
        if (!dedicated_operation(probe, run, i))
            return false;
    if (initialized && probe->failed_stage == NULL) {
        const mc_java_random_profile_status completed =
            mc_java_random_profile_constructor_inputs_complete(probe->profile);
        if (completed != MC_JAVA_RANDOM_PROFILE_OK)
            problem(probe, "Terminal constructor input completeness", "random_profile",
                    (unsigned int)completed);
    }
    mc_java_random_constructor_inputs_view inputs = {0};
    const bool input_view =
        probe->profile != NULL && mc_java_random_profile_constructor_inputs_get(
                                      probe->profile, &inputs) == MC_JAVA_RANDOM_PROFILE_OK;
    return emit(json,
                "],\"reload_transport_guard\":{\"identity_map\":true,"
                "\"complete\":%s,\"saved_block_bytes_checked\":%zu},\"constructor_inputs\":",
                boolean(run->reload_guard_complete), run->guarded_blocks) &&
           (input_view ? emit(json, "{\"bound\":%s,\"count\":%zu,\"consumed\":%zu,\"failure\":%u}",
                              boolean(inputs.bound), inputs.count, inputs.consumed,
                              (unsigned int)inputs.failure)
                       : emit(json, "null"));
}

static int dedicated_main(char **argv) {
    char *resources = decode_path(argv[2]), *root = decode_path(argv[3]),
         *input_path = decode_path(argv[6]);
    natural_probe *probe = calloc(1, sizeof(*probe));
    natural_persistence *persistence = calloc(1, sizeof(*persistence));
    dedicated_observation *observation = calloc(1, sizeof(*observation));
    dedicated_run *run = calloc(1, sizeof(*run));
    mc_math_trig *trig = calloc(1, sizeof(*trig));
    if (resources == NULL || root == NULL || input_path == NULL || probe == NULL ||
        persistence == NULL || observation == NULL || run == NULL || trig == NULL) {
        free(resources);
        free(root);
        free(input_path);
        free(probe);
        free(persistence);
        free(observation);
        free(run);
        free(trig);
        return 1;
    }
    probe->persistence = persistence;
    probe->dedicated = observation;
    persistence->root = root;
    persistence->full_world_save = true;
    const bool initialized = mc_math_trig_init(trig) &&
                             dedicated_read_inputs(probe, run, input_path) &&
                             dedicated_initialize(probe, run, resources, root, trig);
    if (!initialized && probe->failed_stage == NULL)
        problem(probe, "Dedicated observation initialization", "probe", 1u);
    bool encoded = dedicated_observe(probe, run, initialized);
    const bool game_passed = initialized && encoded && probe->failed_stage == NULL &&
                             observation->boundary_ordinal == 18u;
    /* Every source operation and state has finished before native retirement. */
    mc_world_info_record_release(run->record);
    run->record = NULL;
    mc_world_info_release(run->prepared_info);
    mc_world_info_release(run->readback_info);
    run->prepared_info = run->readback_info = NULL;
    const bool preparation_closed =
        mc_world_store_free_checked(&run->preparing, &probe->inspection_error) ==
        MC_WORLD_STORAGE_OK;
    const bool released = preparation_closed && cleanup(probe);
    const bool server_released =
        released && mc_server_runtime_release(observation->server) == MC_SERVER_OK;
    if (server_released)
        observation->server = NULL;
    if (encoded)
        encoded = emit(&probe->json,
                       ",\"region_close_completed\":%s,\"disk_after_close\":", boolean(released)) &&
                  (released ? disk_inventory(probe) : emit(&probe->json, "null")) &&
                  emit(&probe->json, ",\"failure\":") && diagnostic(probe) &&
                  emit(&probe->json, ",\"passed\":%s}\n",
                       boolean(game_passed && released && server_released));
    const bool written =
        encoded && !probe->json.failed &&
        mc_ws_write_file(argv[4], (const uint8_t *)probe->json.bytes, probe->json.length, true,
                         &probe->inspection_error) == MC_WORLD_STORAGE_OK;
    const bool passed = game_passed && released && server_released && encoded && written;
    if (!passed)
        fprintf(stderr,
                "dedicated observation failed: %s (%s %u); encoded=%d released=%d server=%d\n",
                probe->failed_stage == NULL ? "output or finalization" : probe->failed_stage,
                probe->failed_domain == NULL ? "probe" : probe->failed_domain, probe->failed_status,
                (int)encoded, (int)released, (int)server_released);
    free(resources);
    free(root);
    free(input_path);
    free(probe->json.bytes);
    if (released && server_released) {
        free(persistence->store_root);
        free(persistence);
        free(run->inputs);
        free(run);
        free(observation);
        free(probe);
        free(trig);
    }
    return passed ? 0 : 1;
}

#endif
