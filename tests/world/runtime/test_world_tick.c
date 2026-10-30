/* Reuse genuine constructor, Registry, Store and retirement fixture owners. */
#define main mc_world_tick_weather_fixture_main
#include "test_entity_publication.c"
#undef main
#include "../../../src/world/info/tick_source_internal.h"
#include "../../../src/world/info/weather_source_internal.h"
#include "../../../src/world/runtime/entity_publication_internal.h"
#include "mc/entity/mob/construction.h"
#include "mc/entity/player/player.h"
#include "mc/world/runtime/world_tick.h"
#include <inttypes.h>

static bool flat_chunk(fixture *f) {
    bool ok = false;
    fixture_record *r = calloc(1, sizeof(*r));
    uint8_t *encoded = NULL;
    size_t length = 0, written = 0;
    const char *const names[] = {
        "xPos",       "zPos",      "LastUpdate",       "Blocks",   "Data",        "SkyLight",
        "BlockLight", "HeightMap", "TerrainPopulated", "Entities", "TileEntities"};
    const mc_nbt_type types[] = {MC_NBT_INT,        MC_NBT_INT,        MC_NBT_LONG,
                                 MC_NBT_BYTE_ARRAY, MC_NBT_BYTE_ARRAY, MC_NBT_BYTE_ARRAY,
                                 MC_NBT_BYTE_ARRAY, MC_NBT_BYTE_ARRAY, MC_NBT_BYTE,
                                 MC_NBT_LIST,       MC_NBT_LIST};
    const size_t lengths[] = {MC_CHUNK_BLOCKS, MC_CHUNK_NIBBLE_BYTES, MC_CHUNK_NIBBLE_BYTES,
                              MC_CHUNK_NIBBLE_BYTES, MC_CHUNK_HEIGHTMAP_BYTES};
    CHECK(r != NULL);
    r->root.type = r->level.type = MC_NBT_COMPOUND;
    r->level.name = ascii("Level", r->names[11]);
    for (size_t i = 0; i < COUNT(names); ++i) {
        r->fields[i].type = types[i];
        r->fields[i].name = ascii(names[i], r->names[i]);
        r->level_fields[i] = &r->fields[i];
    }
    r->fields[0].value.int_value = r->fields[1].value.int_value = -3;
    r->fields[8].value.byte_value = 1;
    for (size_t i = 0; i < COUNT(lengths); ++i) {
        r->fields[i + 3].value.byte_array.data = r->arrays[i];
        r->fields[i + 3].value.byte_array.length = lengths[i];
    }
    for (size_t x = 0; x < 16; ++x)
        for (size_t z = 0; z < 16; ++z) {
            r->arrays[0][x * 2048 + z * 128 + 63] = 2; /* Actual grass, air above. */
            r->arrays[4][z * 16 + x] = 64;
        }
    r->fields[9].value.list.element_type = r->fields[10].value.list.element_type = MC_NBT_COMPOUND;
    r->level.value.compound.items = r->level_fields;
    r->level.value.compound.length = COUNT(r->level_fields);
    r->root_fields[0] = &r->level;
    r->root.value.compound.items = r->root_fields;
    r->root.value.compound.length = 1;
    CHECK(mc_nbt_measure(&r->root, NULL, &length, NULL) == MC_NBT_OK);
    encoded = malloc(length);
    CHECK(encoded != NULL &&
          mc_nbt_write(&r->root, NULL, encoded, length, &written, NULL) == MC_NBT_OK &&
          written == length);
    uint32_t growth;
    CHECK(mc_world_regions_write(f->regions, mc_world_store_root(f->stores[0]), -3, -3, encoded,
                                 length, fixture_timestamp, &growth, NULL) == MC_WORLD_STORAGE_OK);
    ok = true;
cleanup:
    free(encoded);
    free(r);
    return ok;
}

static bool spawn_seed(mc_world_runtime *world, int32_t category, const char *target,
                       int32_t *seed) {
    mc_world_provider_fields dimension;
    const mc_biome *biome;
    if (mc_world_provider_fields_get(world->fields.t, &dimension) != MC_WORLD_PROVIDER_OK ||
        mc_world_chunk_manager_chunk(dimension.b, -3, -3, &biome, NULL) != MC_WORLD_PROVIDER_OK)
        return false;
    const mc_biome_spawn_list *list = mc_biome_spawn_list_get(biome, category);
    for (int32_t candidate = 0; candidate < 1000000; ++candidate) {
        mc_java_random random;
        mc_java_random_seed(&random, candidate);
        int32_t sum = 0, ticket;
        for (size_t i = 0; i < mc_biome_spawn_list_size(list); ++i)
            sum += mc_biome_spawn_fields_get(mc_biome_spawn_list_at(list, i))->b;
        if (!mc_java_random_next_bounded(&random, sum, &ticket))
            return false;
        const mc_biome_spawn_fields *selected = NULL;
        for (size_t i = 0; i < mc_biome_spawn_list_size(list); ++i) {
            selected = mc_biome_spawn_fields_get(mc_biome_spawn_list_at(list, i));
            ticket -= selected->b;
            if (ticket < 0)
                break;
        }
        if (strcmp(mc_biome_entity_class_name(selected->a, MC_REGISTRY_CLIENT), target) != 0)
            continue;
        int32_t x, y, z, a, b;
        (void)mc_java_random_next_bounded(&random, 16, &x);
        (void)mc_java_random_next_bounded(&random, 128, &y);
        (void)mc_java_random_next_bounded(&random, 16, &z);
        if (y != 64)
            continue;
        (void)mc_java_random_next_bounded(&random, 6, &a);
        (void)mc_java_random_next_bounded(&random, 6, &b);
        x += a - b;
        (void)mc_java_random_next_bounded(&random, 1, &a);
        (void)mc_java_random_next_bounded(&random, 1, &b);
        (void)mc_java_random_next_bounded(&random, 6, &a);
        (void)mc_java_random_next_bounded(&random, 6, &b);
        z += a - b;
        if (x >= 0 && x < 16 && z >= 0 && z < 16) {
            *seed = candidate;
            return true;
        }
    }
    return false;
}

static bool tick_world_create(fixture *f, const char *root) {
    bool ok = false;
    char path[8192];
    uint8_t map[256];
    CHECK(snprintf(path, sizeof path, "%s/world0", root) > 0);
    for (size_t i = 0; i < COUNT(map); ++i)
        map[i] = (uint8_t)i;
    const mc_world_store_options options = {.session_token = 137,
                                            .block_map = map,
                                            .block_map_length = sizeof map,
                                            .limits = mc_world_store_default_limits()};
    CHECK(mc_world_store_open(path, f->regions, &options, &f->stores[0], NULL) ==
          MC_WORLD_STORAGE_OK);
    const uint16_t title[] = {'W', 'e', 'a', 't', 'h', 'e', 'r'};
    const mc_level_info info = {.seed = 23,
                                .spawn_y = 64,
                                .level_name = {title, COUNT(title)},
                                .version = 19132,
                                .rain_time = 20,
                                .thunder_time = 30};
    CHECK(mc_world_store_save_info(f->stores[0], &info, 123, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_provider_create(0, NULL, &f->dimensions[0], NULL) == MC_WORLD_PROVIDER_OK);
    mc_world_runtime_construction_limits limits = mc_world_runtime_default_construction_limits();
    limits.max_provider_chunks = 1;
    limits.max_scheduled_entries = 0;
    CHECK(mc_world_runtime_allocate_world(MC_WORLD_RUNTIME_CLIENT_LOAD_OR_CREATE, f->classes[0],
                                          &limits, &f->worlds[0], NULL) == MC_WORLD_STORAGE_OK);
    const mc_world_runtime_world_arguments args = {
        .store = f->stores[0], .name = f->name, .seed = -99, .dimension = f->dimensions[0]};
    CHECK(mc_world_runtime_construct_world(f->worlds[0], &args, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_chunk_provider_bind_fields(f->worlds[0]->fields.v, &f->providers[0], NULL) ==
          MC_WORLD_STORAGE_OK);
    f->bound[0] = true;
    ok = true;
cleanup:
    return ok;
}

static bool drain_row(mc_world_runtime *world, const char *name, bool has_result, bool pending,
                      const mc_scheduled_tick_fields *head) {
    int32_t esize, fsize;
    if (mc_scheduled_ordered_size(world->fields.E, &esize) != MC_SCHEDULED_ORDERED_OK ||
        mc_scheduled_membership_size(world->fields.F, &fsize) != MC_SCHEDULED_MEMBERSHIP_OK)
        return false;
    if (printf("DRAIN %s ", name) < 0 || (has_result && printf("%d ", pending) < 0) ||
        printf("%d %d", esize, fsize) < 0 || (head != NULL && printf(" %d", head->x) < 0))
        return false;
    return putchar('\n') != EOF;
}

static bool drain_cases(fixture *f) {
    bool ok = false, pending = true, added;
    mc_world_runtime *raw = NULL;
    mc_world_info *info = NULL;
    mc_scheduled_tick *record = NULL;
    mc_world_scheduled_drain_error *error = calloc(1, sizeof(*error));
    mc_scheduled_tick_sequence sequence = {0};
    CHECK(error != NULL);
    CHECK(mc_world_runtime_allocate_world(MC_WORLD_RUNTIME_CLIENT_LOAD_OR_CREATE, f->classes[0],
                                          NULL, &raw, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_info_allocate(MC_WORLD_INFO_CLIENT, &info) == MC_WORLD_INFO_OK);
    raw->fields.x = info;
    info = NULL;
    CHECK(mc_scheduled_ordered_create(2000, 1024u * 1024u, &raw->fields.E) ==
              MC_SCHEDULED_ORDERED_OK &&
          mc_scheduled_membership_create(2000, 1024u * 1024u, &raw->fields.F) ==
              MC_SCHEDULED_MEMBERSHIP_OK);
    CHECK(mc_world_runtime_drain_scheduled(raw, false, &pending, error) == MC_WORLD_STORAGE_OK &&
          !pending && error->initial_count == 0 && error->call.bytecode == 251);
    CHECK(mc_scheduled_tick_create(&sequence, 1, -100, 2, 1, &record) == MC_SCHEDULED_TICK_OK &&
          mc_scheduled_tick_time_set(record, 5) == MC_SCHEDULED_TICK_OK &&
          mc_scheduled_membership_add(raw->fields.F, record, &added) ==
              MC_SCHEDULED_MEMBERSHIP_OK &&
          added &&
          mc_scheduled_ordered_add(raw->fields.E, record, &added) == MC_SCHEDULED_ORDERED_OK &&
          added);
    CHECK(mc_world_runtime_drain_scheduled(raw, false, &pending, error) == MC_WORLD_STORAGE_OK &&
          pending && error->iterations == 1 && error->removed == 0);
    CHECK(drain_row(raw, "future", true, pending, NULL));
    CHECK(mc_world_runtime_drain_scheduled(raw, true, &pending, error) == MC_WORLD_STORAGE_OK &&
          !pending && error->removed == 1);
    CHECK(drain_row(raw, "force", true, pending, NULL));
    mc_scheduled_tick_release(record);
    record = NULL;
    for (int32_t i = 0; i < 1001; ++i) {
        CHECK(mc_scheduled_tick_create(&sequence, i, -100, 0, 1, &record) == MC_SCHEDULED_TICK_OK &&
              mc_scheduled_membership_add(raw->fields.F, record, &added) ==
                  MC_SCHEDULED_MEMBERSHIP_OK &&
              added &&
              mc_scheduled_ordered_add(raw->fields.E, record, &added) == MC_SCHEDULED_ORDERED_OK &&
              added);
        mc_scheduled_tick_release(record);
        record = NULL;
    }
    CHECK(mc_world_runtime_drain_scheduled(raw, false, &pending, error) == MC_WORLD_STORAGE_OK &&
          pending && error->initial_count == 1001 && error->removed == 1000 &&
          error->iterations == 1000);
    CHECK(mc_scheduled_ordered_first(raw->fields.E, &record) == MC_SCHEDULED_ORDERED_OK);
    mc_scheduled_tick_fields head;
    CHECK(mc_scheduled_tick_fields_get(record, &head) == MC_SCHEDULED_TICK_OK && head.x == 1000);
    CHECK(drain_row(raw, "cap", true, pending, &head));
    mc_scheduled_tick_release(record);
    record = NULL;
    CHECK(mc_world_runtime_drain_scheduled(raw, false, &pending, error) == MC_WORLD_STORAGE_OK &&
          !pending && error->removed == 1);
    CHECK(drain_row(raw, "rest", true, pending, NULL));

    CHECK(mc_scheduled_tick_create(&sequence, 0, 64, 0, 1, &record) == MC_SCHEDULED_TICK_OK &&
          mc_scheduled_ordered_add(raw->fields.E, record, &added) == MC_SCHEDULED_ORDERED_OK);
    pending = true;
    CHECK(mc_world_runtime_drain_scheduled(raw, false, &pending, error) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          pending && error->call.bytecode == 30 && error->exception_class != NULL &&
          strcmp(error->call.operation, "TickNextTick list out of synch") == 0);
    CHECK(drain_row(raw, "mismatch", false, pending, NULL));
    CHECK(mc_scheduled_membership_add(raw->fields.F, record, &added) ==
              MC_SCHEDULED_MEMBERSHIP_OK &&
          added);
    CHECK(mc_world_runtime_drain_scheduled(raw, false, &pending, error) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          error->call.bytecode == 159 && error->removed == 1 && pending);
    int32_t count;
    CHECK(mc_scheduled_ordered_size(raw->fields.E, &count) == MC_SCHEDULED_ORDERED_OK &&
          count == 0 &&
          mc_scheduled_membership_size(raw->fields.F, &count) == MC_SCHEDULED_MEMBERSHIP_OK &&
          count == 0 && raw->active_calls == 0);
    CHECK(drain_row(raw, "absent-provider", false, pending, NULL));
    mc_scheduled_tick_fields still_live;
    CHECK(mc_scheduled_tick_fields_get(record, &still_live) == MC_SCHEDULED_TICK_OK &&
          still_live.y == 64);
    ok = true;
cleanup:
    free(error);
    mc_scheduled_tick_release(record);
    mc_world_info_release(info);
    if (mc_world_runtime_free(raw, NULL) != MC_WORLD_STORAGE_OK)
        ok = false;
    return ok;
}

typedef struct sky_control {
    mc_world_access *self;
    size_t calls;
    bool remove_self;
    mc_world_tick_call **active;
    bool reentered, guarded;
} sky_control;

static mc_world_storage_status changed(void *context, mc_world_runtime *world,
                                       mc_world_storage_error *error) {
    sky_control *control = context;
    ++control->calls;
    if (control->active != NULL) {
        mc_world_tick_state active;
        mc_world_tick_call *same = *control->active, *nested = NULL;
        control->guarded =
            mc_world_runtime_tick_inspect(same, &active) && active.active &&
            mc_world_runtime_tick_retire(control->active, NULL) == MC_WORLD_STORAGE_BUSY &&
            *control->active == same && mc_world_runtime_free(world, NULL) == MC_WORLD_STORAGE_BUSY;
        control->reentered =
            mc_world_runtime_tick_begin(world, &nested, NULL) == MC_WORLD_STORAGE_OK &&
            mc_world_runtime_tick_inspect(nested, &active) && active.returned;
        (void)mc_world_runtime_tick_retire(&nested, NULL);
    }
    if (control->remove_self) {
        bool removed;
        return mc_world_runtime_remove_world_access(world, control->self, &removed, error);
    }
    return MC_WORLD_STORAGE_OK;
}

static bool tick_row(mc_world_runtime *world, const char *name) {
    mc_world_info_fields info;
    if (mc_world_info_fields_get(world->fields.x, &info) != MC_WORLD_INFO_OK)
        return false;
    return printf("TICK %s %lld %d %d %d %d %d\n", name, (long long)info.value.time,
                  world->fields.f, world->fields.J, world->fields.Q, info.value.rain_time,
                  info.value.thunder_time) > 0;
}

static bool tick_cases(fixture *f) {
    bool ok = false, removed;
    mc_world_runtime *w = f->worlds[0];
    mc_world_tick_call *call = NULL, *earlier = NULL;
    mc_world_tick_state state = {0}, prior = {0};
    mc_world_tick_error admission;
    const size_t baseline_bindings = w->persistent_bindings;
    const size_t baseline_calls = w->active_calls;
    mc_world_access *a = NULL, *b = NULL, *missing = NULL;
    char *flat_path = NULL;
    mc_ws_file_bytes flat_before = {0}, flat_after = {0};
    mc_entity *player = NULL;
    mc_entity *retained_mob = NULL;
    mc_world_entity_error publication;
    const mc_world_access_callbacks callbacks = {.sky_changed = changed};
    sky_control first = {.remove_self = true, .active = &call}, second = {0};
    w->fields.N = w->fields.O = false;
    w->fields.p = 40;
    w->fields.Q = 7;
    CHECK(mc_world_runtime_tick_begin(w, &call, &admission) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_tick_inspect(call, &state) && state.returned && state.source_pc == 198 &&
          state.weather_returned && state.provider_returned && state.time_stored &&
          state.scheduled_returned && state.random_chunk_count == 0 && w->fields.Q == 6 &&
          w->persistent_bindings == baseline_bindings + 1 && w->active_calls == baseline_calls);
    CHECK(tick_row(w, "basic"));
    CHECK(mc_world_runtime_free(w, NULL) == MC_WORLD_STORAGE_BUSY);
    CHECK(mc_world_runtime_tick_retire(&call, NULL) == MC_WORLD_STORAGE_OK);
    int64_t now;
    CHECK(mc_world_info_time_get(w->fields.x, &now) == MC_WORLD_INFO_OK && now == 1);
    w->fields.O = true;
    CHECK(mc_world_runtime_tick_begin(w, &call, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_tick_inspect(call, &state) && state.returned &&
          state.spawn_chunk_count == 0 && state.spawning_returned);
    CHECK(mc_world_runtime_tick_retire(&call, NULL) == MC_WORLD_STORAGE_OK);

    CHECK(mc_world_access_create(&callbacks, &first, &a, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_access_create(&callbacks, &second, &b, NULL) == MC_WORLD_STORAGE_OK);
    first.self = a;
    second.self = b;
    CHECK(mc_world_runtime_add_world_access(w, a, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_add_world_access(w, b, NULL) == MC_WORLD_STORAGE_OK);
    w->fields.f = INT32_MIN;
    CHECK(mc_world_runtime_tick_begin(w, &call, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_tick_inspect(call, &state) && state.sky_stored &&
          state.sky_callbacks == 1 && first.calls == 1 && second.calls == 0 && first.guarded &&
          first.reentered && mc_world_info_time_get(w->fields.x, &now) == MC_WORLD_INFO_OK &&
          now == 4);
    CHECK(mc_world_runtime_tick_retire(&call, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_remove_world_access(w, b, &removed, NULL) == MC_WORLD_STORAGE_OK &&
          removed && mc_world_access_free(a, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_access_free(b, NULL) == MC_WORLD_STORAGE_OK);
    a = b = NULL;

    const mc_world_access_callbacks empty = {0};
    CHECK(mc_world_access_create(&empty, NULL, &missing, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_add_world_access(w, missing, NULL) == MC_WORLD_STORAGE_OK);
    w->fields.f = INT32_MIN;
    CHECK(mc_world_runtime_tick_begin(w, &earlier, NULL) == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          mc_world_runtime_tick_inspect(earlier, &prior) && prior.source_pc == 141 &&
          prior.sky_stored && !prior.time_stored &&
          prior.error->child == MC_WORLD_TICK_CHILD_ACCESS);
    CHECK(mc_world_runtime_remove_world_access(w, missing, &removed, NULL) == MC_WORLD_STORAGE_OK &&
          removed && mc_world_access_free(missing, NULL) == MC_WORLD_STORAGE_OK);
    missing = NULL;
    CHECK(mc_world_info_source_time_set(w->fields.x, 1) == MC_WORLD_INFO_OK);
    w->fields.p = 0;
    CHECK(mc_world_info_time_get(w->fields.x, &now) == MC_WORLD_INFO_OK);
    CHECK(mc_world_runtime_tick_begin(w, &call, NULL) == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          mc_world_runtime_tick_inspect(call, &state) && state.source_pc == 168 &&
          state.error->exception_class != NULL && !state.time_stored &&
          strcmp(state.error->exception_class, "java/lang/ArithmeticException") == 0 &&
          mc_world_runtime_tick_inspect(earlier, &prior) && prior.source_pc == 141);
    int64_t unchanged;
    CHECK(mc_world_info_time_get(w->fields.x, &unchanged) == MC_WORLD_INFO_OK && unchanged == now);
    printf("TICK zero-period %lld %lld\n", (long long)now, (long long)unchanged);
    CHECK(mc_world_runtime_tick_retire(&call, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_tick_retire(&earlier, NULL) == MC_WORLD_STORAGE_OK);
    w->fields.p = 40;
    w->fields.J = true; /* Empty actual d: A returns true vacuously. */
    w->fields.N = w->fields.O = false;
    w->fields.Q = 7;
    CHECK(mc_world_info_source_rain_time_set(w->fields.x, 20) == MC_WORLD_INFO_OK &&
          mc_world_info_source_thunder_time_set(w->fields.x, 30) == MC_WORLD_INFO_OK);
    CHECK(mc_world_info_source_time_set(w->fields.x, -24001) == MC_WORLD_INFO_OK);
    CHECK(mc_world_runtime_tick_begin(w, &call, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_tick_inspect(call, &state) && state.sleeping && state.returned &&
          !w->fields.J && state.next_time == 1);
    CHECK(tick_row(w, "negative-sleep"));
    CHECK(mc_world_runtime_tick_retire(&call, NULL) == MC_WORLD_STORAGE_OK);

    CHECK(mc_local_player_allocate(f->entity_classes[0], &player, NULL) == MC_ENTITY_OK);
    player->base.x = -0.25;
    player->base.z = 32;
    CHECK(mc_world_runtime_call_enter(w, NULL) == MC_WORLD_STORAGE_OK);
    const bool appended = mc_world_runtime_entity_append_owned(
        w, w->fields.d, &w->player_membership, player, 0, &publication);
    mc_world_runtime_call_leave(w);
    CHECK(appended);
    w->fields.N = false;
    w->fields.O = true;
    int32_t selected_seed = 0;
    CHECK(flat_chunk(f) && spawn_seed(w, 1, "wh", &selected_seed));
    CHECK(mc_ws_path_join(mc_world_store_root(f->stores[0]), "region/r.-1.-1.mcr", &flat_path,
                          NULL) == MC_WORLD_STORAGE_OK &&
          mc_ws_read_file(flat_path, 1024u * 1024u, &flat_before, NULL) == MC_WORLD_STORAGE_OK);
    mc_java_random_reseed(w->fields.r, selected_seed);
    CHECK(mc_world_info_source_rain_time_set(w->fields.x, 20) == MC_WORLD_INFO_OK &&
          mc_world_info_source_thunder_time_set(w->fields.x, 30) == MC_WORLD_INFO_OK);
    CHECK(mc_world_runtime_tick_begin(w, &call, NULL) == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          mc_world_runtime_tick_inspect(call, &state) && state.source_pc == 80 &&
          state.weather_returned && !state.spawning_returned && !state.provider_returned &&
          state.spawn_chunk_count == 289 && state.spawn_category == 1 &&
          state.error->child_bytecode == 773 && state.error->native_policy &&
          state.spawn_chunk_x == -3 && state.spawn_chunk_z == -3 &&
          state.spawn_candidates_visited == 1 && state.spawn_random_calls == 10 &&
          state.spawn_class != NULL &&
          strcmp(state.error->operation,
                 "World spawn concrete Mob class initialization/constructor") == 0);
    mc_entity_class_literal_view selected_class = {0};
    CHECK(mc_entity_class_literal_inspect(state.spawn_class, &selected_class) == MC_ENTITY_OK &&
          selected_class.kind == MC_ENTITY_CLASS_KIND_PIG && f->providers[0].cache->count == 1 &&
          f->providers[0].cache->insertion_count == 1);
    CHECK(mc_ws_read_file(flat_path, 1024u * 1024u, &flat_after, NULL) == MC_WORLD_STORAGE_OK &&
          flat_before.length == flat_after.length &&
          memcmp(flat_before.data, flat_after.data, flat_before.length) == 0 &&
          w->entity_membership.count == 0 && w->player_membership.count == 1 &&
          w->local_leases == 0);
    f->states[0] = f->providers[0].cache->entries[0].chunk;
    printf("SPAWN prefix %zu %d %d\n", state.spawn_chunk_count, state.spawn_chunk_x,
           state.spawn_chunk_z);
    printf("SPAWN constructor %d %d %d %s %d %d %d %d %d %d %zu %012" PRIx64 "\n", selected_seed,
           state.spawn_pick.total_weight, state.spawn_pick.ticket, selected_class.client_name,
           state.spawn_initial_x, state.spawn_initial_y, state.spawn_initial_z, state.spawn_trial_x,
           state.spawn_trial_y, state.spawn_trial_z, state.spawn_random_calls, w->fields.r->state);
    CHECK(mc_world_runtime_tick_retire(&call, NULL) == MC_WORLD_STORAGE_OK &&
          w->active_calls == baseline_calls && w->persistent_bindings == baseline_bindings);
    /* The same real saved Chunk and candidate Set now exercise all five actual
     * default hostile constructors. The independent World Random oracle stops
     * after the ten selection/trial draws and the fresh postconstructor yaw. */
    static const char *const hostile_names[] = {"cn", "uz", "fr", "gb", "uw"};
    w->fields.N = true;
    w->fields.O = false;
    for (size_t i = 0; i < COUNT(hostile_names); ++i) {
        CHECK(spawn_seed(w, 0, hostile_names[i], &selected_seed));
        mc_java_random oracle;
        mc_java_random_seed(&oracle, selected_seed);
        static const int32_t bounds[] = {50, 16, 128, 16, 6, 6, 1, 1, 6, 6};
        int32_t ignored;
        for (size_t j = 0; j < COUNT(bounds); ++j)
            CHECK(mc_java_random_next_bounded(&oracle, bounds[j], &ignored));
        const float expected_yaw = mc_java_random_next_float(&oracle) * 360.0f;
        mc_java_random_reseed(w->fields.r, selected_seed);
        CHECK(mc_world_runtime_tick_begin(w, &call, NULL) == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
              mc_world_runtime_tick_inspect(call, &state) && state.source_pc == 80 &&
              state.weather_returned && !state.spawning_returned && !state.provider_returned &&
              state.error->child_bytecode == 820 && state.error->native_policy &&
              state.error->mob == NULL && state.spawn_constructed && state.spawn_positioned &&
              state.spawn_entity != NULL && state.spawn_category == 0 &&
              state.spawn_candidates_visited == 1 && state.spawn_random_calls == 11 &&
              w->fields.r->state == oracle.state &&
              w->persistent_bindings == baseline_bindings + 2 && w->entity_membership.count == 0 &&
              w->player_membership.count == 1 && w->local_leases == 0);
        mc_entity_snapshot entity;
        mc_mob_state mob;
        CHECK(mc_entity_get_snapshot(state.spawn_entity, &entity) == MC_ENTITY_OK &&
              mc_mob_inspect(state.spawn_entity, &mob) && entity.phase == MC_ENTITY_READY &&
              entity.class_literal == state.spawn_class && entity.base.world == w &&
              mc_entity_class_literal_inspect(entity.class_literal, &selected_class) ==
                  MC_ENTITY_OK &&
              strcmp(selected_class.client_name, hostile_names[i]) == 0 &&
              entity.base.x == (double)((float)state.spawn_trial_x + 0.5f) &&
              entity.base.y == (double)state.spawn_trial_y + (double)entity.base.offset &&
              entity.base.z == (double)((float)state.spawn_trial_z + 0.5f) &&
              entity.base.x == entity.base.aJ && entity.base.x == entity.base.bl &&
              entity.base.y == entity.base.aK && entity.base.y == entity.base.bm &&
              entity.base.z == entity.base.aL && entity.base.z == entity.base.bn &&
              entity.base.yaw == expected_yaw && entity.base.aT == 0.0f && entity.base.aU == 0.0f &&
              entity.base.aV == 0.0f &&
              entity.bounds->min_x == entity.base.x - (double)(entity.base.width / 2.0f) &&
              entity.bounds->max_x == entity.base.x + (double)(entity.base.width / 2.0f) &&
              entity.bounds->min_y == entity.base.y - (double)entity.base.offset &&
              entity.bounds->max_y == entity.bounds->min_y + (double)entity.base.height);
        CHECK(mc_entity_retain(state.spawn_entity) == MC_ENTITY_OK);
        retained_mob = state.spawn_entity;
        CHECK(mc_world_runtime_tick_retire(&call, NULL) == MC_WORLD_STORAGE_OK &&
              w->persistent_bindings == baseline_bindings + 1 &&
              mc_entity_get_snapshot(retained_mob, &entity) == MC_ENTITY_OK &&
              entity.phase == MC_ENTITY_READY);
        mc_entity_release(retained_mob);
        retained_mob = NULL;
        CHECK(w->persistent_bindings == baseline_bindings && w->active_calls == baseline_calls);
        printf("MOB_SPAWN %s yaw-positioned=1 next=820 world-random-calls=11\n", hostile_names[i]);
    }
    ok = true;
cleanup:
    free(flat_path);
    mc_ws_free_file_bytes(&flat_before);
    mc_ws_free_file_bytes(&flat_after);
    (void)mc_world_runtime_tick_retire(&call, NULL);
    (void)mc_world_runtime_tick_retire(&earlier, NULL);
    mc_entity_release(retained_mob);
    if (a != NULL)
        (void)mc_world_runtime_remove_world_access(w, a, &removed, NULL);
    if (b != NULL)
        (void)mc_world_runtime_remove_world_access(w, b, &removed, NULL);
    if (missing != NULL)
        (void)mc_world_runtime_remove_world_access(w, missing, &removed, NULL);
    (void)mc_world_access_free(a, NULL);
    (void)mc_world_access_free(b, NULL);
    (void)mc_world_access_free(missing, NULL);
    mc_entity_release(player);
    return ok;
}

int main(int argc, char **argv) {
    int result = 1;
    fixture *f = NULL;
    char root[8192];
    if (argc != 3 || strcmp(argv[1], "world-tick") != 0 ||
        !decode_path(argv[2], root, sizeof root) || !mc_math_trig_init(&trig))
        return 2;
    f = calloc(1, sizeof(*f));
    CHECK(f != NULL && mc_materials_create(&f->materials));
    CHECK(mc_world_regions_create(NULL, &f->regions, NULL) == MC_WORLD_STORAGE_OK &&
          mc_java_random_profile_create(16u * 1024u * 1024u, &f->profile) ==
              MC_JAVA_RANDOM_PROFILE_OK);
    static const uint16_t title[] = {'W', 'e', 'a', 't', 'h', 'e', 'r'};
    CHECK(mc_world_info_name_copy((mc_nbt_string){title, COUNT(title)}, SIZE_MAX, &f->name) ==
          MC_WORLD_INFO_OK);
    resources *r = &f->sources[0];
    r->streams[0] = (stream){.bytes = language, .length = sizeof language - 1};
    r->streams[1] = r->streams[2] = (stream){.bytes = language};
    r->api = (mc_registry_resources){r, open_resource, close_resource, ready_resource};
    const mc_registry_options options = {MC_REGISTRY_CLIENT, f->profile, f->materials, 0, &r->api};
    CHECK(mc_registry_classes_allocate(&options, &f->registry[0], NULL) == MC_REGISTRY_OK);
    CHECK(mc_world_runtime_classes_create(MC_WORLD_INFO_CLIENT, f->profile, &trig, &f->classes[0],
                                          NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_classes_bind_registry(f->classes[0], f->registry[0], NULL) ==
              MC_REGISTRY_OK);
    CHECK(mc_entity_classes_create(MC_ENTITY_CLIENT, f->profile, NULL, &f->entity_classes[0]) ==
              MC_ENTITY_OK &&
          mc_world_runtime_classes_bind_entities(f->classes[0], f->entity_classes[0]) ==
              MC_ENTITY_OK);
    CHECK(mc_registry_classes_initialize_blocks(f->registry[0], NULL) == MC_REGISTRY_OK);
    CHECK(tick_world_create(f, root));
    CHECK(drain_cases(f) && tick_cases(f));
    result = 0;
cleanup:
    if (!cleanup_fixture(f))
        return 1;
    if (result == 0)
        printf("World scheduler: %u checks\n", checks);
    return result;
}
