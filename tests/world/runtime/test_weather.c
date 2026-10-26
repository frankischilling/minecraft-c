/* Reuse genuine Registry/World/Store fixture owners and their teardown.
 * The imported main is retained, never executed. Scenario field controls are
 * authored inputs and are restored before owner retirement. */
#define main mc_weather_publication_fixture_main
#include "test_entity_publication.c"
#undef main
#include "../../../src/world/info/weather_source_internal.h"
#include "../../../src/world/runtime/weather_math_internal.h"
#include "mc/world/runtime/weather.h"
#include <inttypes.h>

static bool weather_reference;

static uint32_t fbits(float value) {
    uint32_t b;
    memcpy(&b, &value, sizeof b);
    return b;
}

static uint64_t dbits(double value) {
    uint64_t b;
    memcpy(&b, &value, sizeof b);
    return b;
}

static float from_bits(uint32_t bits) {
    float value;
    memcpy(&value, &bits, sizeof value);
    return value;
}

static bool weather_create(fixture *f, size_t slot, const char *root, size_t side,
                           int32_t dimension, bool raining, bool thundering) {
    bool ok = false;
    char path[8192];
    uint8_t map[256];
    const uint16_t title[] = {'W', 'e', 'a', 't', 'h', 'e', 'r'};
    CHECK(snprintf(path, sizeof path, "%s/world%zu", root, slot) > 0);
    for (size_t i = 0; i < COUNT(map); ++i)
        map[i] = (uint8_t)i;
    const mc_world_store_options options = {.session_token = 137,
                                            .block_map = map,
                                            .block_map_length = sizeof map,
                                            .limits = mc_world_store_default_limits()};
    CHECK(mc_world_store_open(path, f->regions, &options, &f->stores[slot], NULL) ==
          MC_WORLD_STORAGE_OK);
    const mc_level_info info = {.seed = 23,
                                .spawn_y = 64,
                                .level_name = {title, COUNT(title)},
                                .version = 19132,
                                .rain_time = 20,
                                .thunder_time = 30,
                                .raining = raining,
                                .thundering = thundering};
    CHECK(mc_world_store_save_info(f->stores[slot], &info, 123, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_provider_create(dimension, NULL, &f->dimensions[slot], NULL) ==
          MC_WORLD_PROVIDER_OK);
    mc_world_runtime_construction_limits limits = mc_world_runtime_default_construction_limits();
    limits.max_provider_chunks = 1;
    limits.max_scheduled_entries = 0;
    CHECK(mc_world_runtime_allocate_world(side == 0 ? MC_WORLD_RUNTIME_CLIENT_LOAD_OR_CREATE
                                                    : MC_WORLD_RUNTIME_SERVER_LOAD_OR_CREATE,
                                          f->classes[side], &limits, &f->worlds[slot],
                                          NULL) == MC_WORLD_STORAGE_OK);
    const mc_world_runtime_world_arguments args = {
        .store = f->stores[slot], .name = f->name, .seed = -99, .dimension = f->dimensions[slot]};
    mc_world_storage_error constructor_error = {0};
    const mc_world_storage_status constructed =
        mc_world_runtime_construct_world(f->worlds[slot], &args, &constructor_error);
    if (slot == 4) {
        mc_world_runtime_construction prefix;
        mc_world_provider_fields fields;
        CHECK(mc_world_runtime_construction_get(f->worlds[slot], &prefix) == MC_WORLD_STORAGE_OK);
        CHECK(constructed == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
              prefix.phase == MC_WORLD_RUNTIME_FAILED_CONSTRUCTION &&
              prefix.failed_call_pc == 403 &&
              strcmp(constructor_error.operation, "provider Dimension kind") == 0);
        CHECK(mc_world_provider_fields_get(f->worlds[slot]->fields.t, &fields) ==
                  MC_WORLD_PROVIDER_OK &&
              fields.e && fields.a == f->worlds[slot] && f->worlds[slot]->fields.r != NULL &&
              f->worlds[slot]->fields.x != NULL && f->worlds[slot]->fields.v == NULL);
        /* Real failed constructor prefix; never fabricate READY or a generator. */
        printf("PREFIX Nether constructor UNSUPPORTED pc403 phaseFAILED; weather return10 only\n");
        ok = true;
        goto cleanup;
    }
    CHECK(constructed == MC_WORLD_STORAGE_OK);
    CHECK(mc_chunk_provider_bind_fields(f->worlds[slot]->fields.v, &f->providers[slot], NULL) ==
          MC_WORLD_STORAGE_OK);
    f->bound[slot] = true;
    CHECK(f->worlds[slot]->fields.i == 0 && f->worlds[slot]->fields.k == 0 &&
          f->worlds[slot]->fields.j == (raining ? 1.0f : 0.0f) &&
          f->worlds[slot]->fields.l == (raining && thundering ? 1.0f : 0.0f));
    if (slot < 4 && weather_reference)
        printf("INIT %zu %d %d %08" PRIx32 " %08" PRIx32 " %08" PRIx32 " %08" PRIx32 "\n", slot,
               raining, thundering, fbits(f->worlds[slot]->fields.i),
               fbits(f->worlds[slot]->fields.j), fbits(f->worlds[slot]->fields.k),
               fbits(f->worlds[slot]->fields.l));
    ok = true;
cleanup:
    return ok;
}

static bool weather_inputs(mc_world_runtime *w, int scene) {
    bool ok = false;
    const bool sequence = scene >= 45;
    if (sequence && scene != 45) {
        ok = true;
        goto cleanup;
    }
    w->fields.m = scene % 3 == 0 ? 3 : scene % 3 == 1 ? 0 : INT32_MAX;
    w->fields.n = 123;
    w->fields.B = (scene & 1) != 0;
    w->fields.i = -11;
    w->fields.k = -13;
    w->fields.j = .25f;
    w->fields.l = .75f;
    bool raining = (scene & 1) != 0, thundering = (scene & 2) != 0;
    int32_t rain = 20, thunder = 20;
    if (scene < 16) {
        switch (scene / 4) {
            case 0:
                rain = thunder = 0;
                break;
            case 1:
                rain = thunder = 1;
                break;
            case 2:
                rain = thunder = 2;
                break;
            default:
                rain = INT32_MIN;
                thunder = INT32_MAX;
                break;
        }
    }
    static const uint32_t edges[] = {0,
                                     UINT32_C(0x80000000),
                                     1,
                                     UINT32_C(0x80000001),
                                     UINT32_C(0x00800000),
                                     UINT32_C(0x3f7fffff),
                                     UINT32_C(0x3f800000),
                                     UINT32_C(0x3fc00000),
                                     UINT32_C(0xbf000000),
                                     UINT32_C(0x7f800000),
                                     UINT32_C(0xff800000),
                                     UINT32_C(0x7fc12345),
                                     UINT32_C(0xffc54321),
                                     UINT32_C(0x7f812345),
                                     UINT32_C(0xff854321),
                                     UINT32_C(0x7f7fffff)};
    if (scene >= 16 && scene < 32) {
        size_t i = (size_t)(scene - 16);
        w->fields.j = from_bits(edges[i]);
        w->fields.l = from_bits(edges[(i + 5) % COUNT(edges)]);
        w->fields.m = INT32_MIN;
    }
    if (scene >= 32 && scene < 36) {
        w->fields.j = from_bits(scene < 34 ? UINT32_C(0x7fc12345) : UINT32_C(0x7f812345));
        w->fields.l = from_bits(scene < 34 ? UINT32_C(0xffc54321) : UINT32_C(0xff854321));
    }
    if (scene == 32) {
        w->fields.j = from_bits(UINT32_C(0x3a0003e5));
        w->fields.l = from_bits(UINT32_C(0x3c300000));
        raining = true;
        thundering = false;
    }
    if (scene == 33) {
        w->fields.j = from_bits(UINT32_C(0x3c300000));
        w->fields.l = from_bits(UINT32_C(0x3a0003e5));
        raining = false;
        thundering = true;
    }
    if (scene >= 39 && scene <= 44) {
        rain = 0;
        thunder = scene >= 43 ? 2 : 0;
        raining = scene == 44;
        thundering = scene == 42;
        w->fields.m = 3;
    }
    if (scene == 45) {
        rain = thunder = 2;
        raining = thundering = false;
        w->fields.m = 2;
    }
    CHECK(mc_world_info_rain_time_set(w->fields.x, rain) == MC_WORLD_INFO_OK &&
          mc_world_info_thunder_time_set(w->fields.x, thunder) == MC_WORLD_INFO_OK &&
          mc_world_info_raining_set(w->fields.x, raining) == MC_WORLD_INFO_OK &&
          mc_world_info_thundering_set(w->fields.x, thundering) == MC_WORLD_INFO_OK);
    mc_java_random_reseed(w->fields.r, 38);
    if (scene == 35)
        (void)mc_java_random_next_gaussian(w->fields.r);
    ok = true;
cleanup:
    return ok;
}

static bool weather_row(mc_world_runtime *w, int scene, bool complete) {
    bool ok = false;
    mc_world_info_fields info;
    mc_world_info_state state;
    CHECK(mc_world_info_fields_get(w->fields.x, &info) == MC_WORLD_INFO_OK &&
          mc_world_info_state_get(w->fields.x, &state) == MC_WORLD_INFO_OK);
    const uint64_t random = w->fields.r->state;
    const int32_t continuation = mc_java_random_next_i32(w->fields.r);
    if (weather_reference)
        printf("WEATHER %d %d %" PRId32 " %" PRId32 " %" PRId32 " %" PRId32 " %d %d %08" PRIx32
               " %08" PRIx32 " %08" PRIx32 " %08" PRIx32 " %012" PRIx64 " %d %016" PRIx64
               " %" PRId32 "\n",
               scene, complete, w->fields.m, w->fields.n, info.value.thunder_time,
               info.value.rain_time, info.value.thundering, info.value.raining, fbits(w->fields.i),
               fbits(w->fields.j), fbits(w->fields.k), fbits(w->fields.l), random,
               w->fields.r->have_next_next_gaussian, dbits(w->fields.r->next_next_gaussian),
               continuation);
    ok = true;
cleanup:
    return ok;
}

static bool weather_cases(fixture *f) {
    bool ok = false;
    mc_world_weather_error error;
    for (int scene = 0; scene < 64; ++scene) {
        mc_world_runtime *w = f->worlds[scene >= 36 && scene <= 38 ? 4 : 0];
        CHECK(weather_inputs(w, scene));
        mc_world_provider *provider = w->fields.t;
        mc_world_info *info = w->fields.x;
        mc_java_random *random = w->fields.r;
        if (scene == 36 || scene == 38 || scene == 40)
            w->fields.x = NULL;
        if (scene == 37 || scene == 38 || (scene >= 41 && scene <= 44))
            w->fields.r = NULL;
        if (scene == 39)
            w->fields.t = NULL;
        const mc_world_storage_status status = mc_world_runtime_tick_weather(w, &error);
        w->fields.t = provider;
        w->fields.x = info;
        w->fields.r = random;
        const bool complete = scene < 39 || scene >= 45;
        CHECK(complete ? status == MC_WORLD_STORAGE_OK
                       : status == MC_WORLD_STORAGE_UNSUPPORTED_STATE);
        if (!complete) {
            const unsigned int expected = scene == 39   ? 4u
                                          : scene == 40 ? 32u
                                          : scene == 41 ? 84u
                                          : scene == 42 ? 61u
                                          : scene == 43 ? 190u
                                                        : 167u;
            CHECK(error.bytecode == expected);
        } else
            CHECK(error.operation == NULL && error.bytecode == 0 &&
                  error.cause.status == MC_WORLD_STORAGE_OK);
        if (scene >= 36 && scene <= 38) {
            mc_world_runtime_construction prefix;
            CHECK(mc_world_runtime_construction_get(w, &prefix) == MC_WORLD_STORAGE_OK &&
                  prefix.phase == MC_WORLD_RUNTIME_FAILED_CONSTRUCTION &&
                  prefix.failed_call_pc == 403);
        }
        CHECK(weather_row(w, scene, complete));
    }
    ok = true;
cleanup:
    return ok;
}

static bool weather_native(fixture *f) {
    bool ok = false;
    mc_world_runtime *w = f->worlds[0];
    mc_world_weather_error error;
    mc_world_info *raw = NULL;
    CHECK(mc_world_runtime_tick_weather(NULL, &error) == MC_WORLD_STORAGE_INVALID_ARGUMENT &&
          error.bytecode == 0);
    CHECK(mc_world_runtime_tick_weather(f->worlds[5], &error) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          error.bytecode == 0);
    /* Native admission leaves actual server source fields/diagnostics intact. */
    mc_world_runtime *server = f->worlds[5];
    const mc_world_storage_error old_server_error = server->last_error;
    const int32_t old_server_m = server->fields.m;
    const uint64_t old_server_random = server->fields.r->state;
    CHECK(mc_world_runtime_tick_weather(server, NULL) == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          server->fields.m == old_server_m && server->fields.r->state == old_server_random &&
          server->last_error.status == old_server_error.status &&
          server->last_error.operation == old_server_error.operation);
    CHECK(weather_inputs(w, 0));
    const mc_world_storage_error old_error = w->last_error;
    const uint64_t old_random = w->fields.r->state;
    const size_t old_calls = w->active_calls;
    w->active_calls = SIZE_MAX;
    const mc_world_storage_status count_status = mc_world_runtime_tick_weather(w, &error);
    const size_t reached_calls = w->active_calls;
    w->active_calls = old_calls;
    CHECK(count_status != MC_WORLD_STORAGE_OK && reached_calls == SIZE_MAX && w->fields.m == 3 &&
          w->fields.r->state == old_random && w->last_error.status == old_error.status &&
          w->last_error.operation == old_error.operation);
    /* Wrong-side actual Info is rejected only after the preceding m store. */
    mc_world_info *client_info = w->fields.x;
    w->fields.x = server->fields.x;
    const mc_world_storage_status side_status = mc_world_runtime_tick_weather(w, &error);
    w->fields.x = client_info;
    CHECK(side_status == MC_WORLD_STORAGE_UNSUPPORTED_STATE && error.bytecode == 32 &&
          error.info == MC_WORLD_INFO_UNSUPPORTED_OPERATION && w->fields.m == 2 &&
          w->fields.r->state == old_random);
    CHECK(mc_world_runtime_tick_weather(w, NULL) == MC_WORLD_STORAGE_OK &&
          w->active_calls == old_calls);
    CHECK(mc_world_runtime_tick_weather(w, &error) == MC_WORLD_STORAGE_OK &&
          error.operation == NULL && error.bytecode == 0 && error.info_method == NULL &&
          error.cause.operation == NULL && w->last_error.status == MC_WORLD_STORAGE_OK);
    mc_world_weather_error_init(NULL);
    /* Bad FP is admitted until timers/draws and previous-rain store245. */
    for (int rain = 0; rain < 2; ++rain) {
        CHECK(weather_inputs(w, 0));
        CHECK(mc_world_info_raining_set(w->fields.x, rain != 0) == MC_WORLD_INFO_OK);
        const unsigned int csr = _mm_getcsr();
        _mm_setcsr((csr & ~0x6000u) | 0x6000u);
        const mc_world_storage_status status = mc_world_runtime_tick_weather(w, &error);
        _mm_setcsr(csr);
        mc_world_info_fields fields;
        CHECK(mc_world_info_fields_get(w->fields.x, &fields) == MC_WORLD_INFO_OK);
        CHECK(status == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
              error.bytecode == (rain ? 263u : 280u));
        CHECK(w->fields.m == 2 && w->fields.i == .25f && w->fields.j == .25f &&
              w->fields.k == -13 && w->fields.l == .75f && fields.value.rain_time > 0 &&
              fields.value.thunder_time > 0);
    }
    mc_world_runtime *nether = f->worlds[4];
    CHECK(weather_inputs(nether, 36));
    const unsigned int csr = _mm_getcsr();
    _mm_setcsr((csr & ~0x6000u) | 0x6000u);
    const mc_world_storage_status skipped = mc_world_runtime_tick_weather(nether, &error);
    _mm_setcsr(csr);
    CHECK(skipped == MC_WORLD_STORAGE_OK && nether->fields.m == 3);
    /* Private source kernels, public READY admission and constructor state. */
    CHECK(mc_world_info_allocate(MC_WORLD_INFO_CLIENT, &raw) == MC_WORLD_INFO_OK);
    mc_world_info_state before, after;
    CHECK(mc_world_info_state_get(raw, &before) == MC_WORLD_INFO_OK);
    CHECK(mc_world_info_source_rain_time_set(raw, 2) == MC_WORLD_INFO_OK &&
          mc_world_info_source_thunder_time_set(raw, 2) == MC_WORLD_INFO_OK &&
          mc_world_info_source_raining_set(raw, true) == MC_WORLD_INFO_OK &&
          mc_world_info_source_thundering_set(raw, true) == MC_WORLD_INFO_OK);
    bool flag = false;
    int32_t value = -1;
    CHECK(mc_world_info_raining_get(raw, &flag) == MC_WORLD_INFO_INVALID_PHASE && !flag);
    CHECK(mc_world_info_thundering_get(raw, &flag) == MC_WORLD_INFO_INVALID_PHASE && !flag);
    CHECK(mc_world_info_rain_time_set(raw, 9) == MC_WORLD_INFO_INVALID_PHASE &&
          mc_world_info_thunder_time_set(raw, 9) == MC_WORLD_INFO_INVALID_PHASE &&
          mc_world_info_raining_set(raw, false) == MC_WORLD_INFO_INVALID_PHASE &&
          mc_world_info_thundering_set(raw, false) == MC_WORLD_INFO_INVALID_PHASE);
    CHECK(mc_world_info_source_rain_time_get(raw, &value) == MC_WORLD_INFO_OK && value == 2);
    CHECK(mc_world_info_source_thunder_time_get(raw, &value) == MC_WORLD_INFO_OK && value == 2);
    CHECK(mc_world_info_source_raining_get(raw, &flag) == MC_WORLD_INFO_OK && flag);
    CHECK(mc_world_info_source_thundering_get(raw, &flag) == MC_WORLD_INFO_OK && flag);
    mc_world_info *saved = w->fields.x;
    w->fields.x = raw;
    const mc_world_storage_status raw_status = mc_world_runtime_tick_weather(w, &error);
    w->fields.x = saved;
    CHECK(raw_status == MC_WORLD_STORAGE_OK &&
          mc_world_info_state_get(raw, &after) == MC_WORLD_INFO_OK && after.phase == before.phase &&
          after.last_store_pc == before.last_store_pc);
    mc_world_info_release(raw);
    raw = NULL;
    ok = true;
cleanup:
    mc_world_info_release(raw);
    return ok;
}

int main(int argc, char **argv) {
    int result = 1;
    fixture *f = NULL;
    char root[8192];
    if (argc != 3 ||
        (strcmp(argv[1], "world-weather") != 0 &&
         strcmp(argv[1], "world-weather-reference") != 0) ||
        !decode_path(argv[2], root, sizeof root) || !mc_math_trig_init(&trig))
        return 2;
    weather_reference = strcmp(argv[1], "world-weather-reference") == 0;
    f = calloc(1, sizeof *f);
    CHECK(f != NULL && mc_materials_create(&f->materials));
    CHECK(mc_world_regions_create(NULL, &f->regions, NULL) == MC_WORLD_STORAGE_OK &&
          mc_java_random_profile_create(16u * 1024u * 1024u, &f->profile) ==
              MC_JAVA_RANDOM_PROFILE_OK);
    static const uint16_t title[] = {'W', 'e', 'a', 't', 'h', 'e', 'r'};
    CHECK(mc_world_info_name_copy((mc_nbt_string){title, COUNT(title)}, SIZE_MAX, &f->name) ==
          MC_WORLD_INFO_OK);
    for (size_t side = 0; side < 2; ++side) {
        resources *r = &f->sources[side];
        r->streams[0] = (stream){.bytes = language, .length = sizeof language - 1};
        r->streams[1] = r->streams[2] = (stream){.bytes = language};
        r->api = (mc_registry_resources){r, open_resource, close_resource, ready_resource};
        const mc_registry_options options = {side == 0 ? MC_REGISTRY_CLIENT : MC_REGISTRY_SERVER,
                                             f->profile, f->materials, 0, &r->api};
        CHECK(mc_registry_classes_allocate(&options, &f->registry[side], NULL) == MC_REGISTRY_OK);
        CHECK(mc_world_runtime_classes_create(
                  side == 0 ? MC_WORLD_INFO_CLIENT : MC_WORLD_INFO_SERVER, f->profile, &trig,
                  &f->classes[side], NULL) == MC_WORLD_STORAGE_OK);
        CHECK(mc_world_runtime_classes_bind_registry(f->classes[side], f->registry[side], NULL) ==
              MC_REGISTRY_OK);
        CHECK(mc_entity_classes_create(side == 0 ? MC_ENTITY_CLIENT : MC_ENTITY_SERVER, f->profile,
                                       NULL, &f->entity_classes[side]) == MC_ENTITY_OK &&
              mc_world_runtime_classes_bind_entities(f->classes[side], f->entity_classes[side]) ==
                  MC_ENTITY_OK);
        CHECK(mc_registry_classes_initialize_blocks(f->registry[side], NULL) == MC_REGISTRY_OK);
    }
    for (size_t i = 0; i < 6; ++i)
        CHECK(weather_create(f, i, root, i == 5 ? 1u : 0u, i == 4 ? -1 : 0, (i & 1u) != 0,
                             (i & 2u) != 0));
    CHECK(weather_cases(f));
    CHECK(weather_native(f));
    result = 0;
cleanup:
    if (!cleanup_fixture(f))
        return 1;
    if (result == 0 && printf("World weather: %u checks\n", checks) < 0)
        return 1;
    return result;
}
