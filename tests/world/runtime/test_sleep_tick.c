/* Genuine World/Store/Player fixture; imported observation main is not run.
 * WorldAccess callbacks below are authored component controls, not Renderer
 * behavior or additional original-game observations. */
#define MC_WORLD_ENTITY_TICK_MAIN mc_sleep_tick_unused_tick_main
#include "test_entity_tick.c"
#undef MC_WORLD_ENTITY_TICK_MAIN

#include "../../../src/world/info/tick_source_internal.h"
#include "../../../src/world/info/weather_source_internal.h"
#include "../../../src/world/runtime/entity_tick_internal.h"
#include "../../../src/world/runtime/lighting_internal.h"
#include "mc/core/math/classes.h"
#include "mc/entity/player/coordinates.h"
#include "mc/entity/player/sleep.h"
#include "mc/world/chunk/nibbles.h"
#include "mc/world/runtime/random_tick.h"
#include "mc/world/runtime/world_tick.h"

static bool sleep_tick_reset(fixture *f) {
    mc_chunk_data *data = f->states[0]->data;
    memset(data->blocks, 0, MC_CHUNK_BLOCKS);
    memset(data->metadata, 0, MC_CHUNK_NIBBLE_BYTES);
    memset(data->heightmap, 0, MC_CHUNK_HEIGHTMAP_BYTES);
    memset(data->skylight, 0, MC_CHUNK_NIBBLE_BYTES);
    memset(data->block_light, 0, MC_CHUNK_NIBBLE_BYTES);
    f->worlds[0]->fields.B = false;
    f->worlds[0]->fields.f = 0;
    return mc_world_runtime_light_list_clear(f->worlds[0], NULL) == MC_WORLD_STORAGE_OK;
}

static bool sleep_tick_put(mc_chunk_data *data, int32_t x, int32_t y, int32_t z, int32_t id,
                           int32_t metadata) {
    data->blocks[boxes_index(x, y, z)] = (uint8_t)id;
    return mc_chunk_nibbles_set(data->metadata, MC_CHUNK_NIBBLE_BYTES, x, y, z, (uint8_t)metadata);
}

typedef struct sleep_tick_observer {
    mc_entity *other;
    mc_world_tick_call **parent;
    mc_world_reference_list *captured;
    unsigned int mode, calls;
    unsigned int bed_calls;
    size_t registry_borrows;
    size_t ranges;
    size_t world_bindings;
    mc_world_info *info;
    bool parent_busy;
} sleep_tick_observer;

static bool sleep_tick_player(fixture *f, mc_entity **out, mc_player_session_classes **classes) {
    bool ok = false;
    mc_player_session *session = NULL;
    mc_player_string *username = NULL, *token = NULL;
    const uint16_t user[] = {'S', 'l', 'e', 'e', 'p', 'T', 'i', 'c', 'k'}, sid[] = {'-'};
    CHECK(mc_player_session_classes_create(f->entity_classes[0], f->registry[0], classes, NULL) ==
              MC_PLAYER_OK &&
          mc_player_string_create(user, COUNT(user), &username) == MC_PLAYER_OK &&
          mc_player_string_create(sid, COUNT(sid), &token) == MC_PLAYER_OK &&
          mc_player_session_create(*classes, username, token, &session, NULL) == MC_PLAYER_OK);
    CHECK(mc_local_player_allocate(f->entity_classes[0], out, NULL) == MC_ENTITY_OK &&
          mc_local_player_construct(*out, f->worlds[0], NULL, session, 0, NULL, NULL) ==
              MC_PLAYER_OK);
    bool inserted = false;
    CHECK(mc_world_runtime_insert_entity(f->worlds[0], *out, &inserted, NULL) ==
              MC_WORLD_STORAGE_OK &&
          inserted);
    ok = true;
cleanup:
    mc_player_session_retire(&session);
    mc_player_string_release(username);
    mc_player_string_release(token);
    return ok;
}

static mc_world_storage_status sleep_tick_sky(void *context, mc_world_runtime *world,
                                              mc_world_storage_error *error) {
    (void)context;
    (void)world;
    (void)error;
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status sleep_tick_range(void *context, mc_world_runtime *world, int32_t x0,
                                                int32_t y0, int32_t z0, int32_t x1, int32_t y1,
                                                int32_t z1, mc_world_storage_error *error) {
    (void)world;
    (void)x0;
    (void)y0;
    (void)z0;
    (void)x1;
    (void)y1;
    (void)z1;
    (void)error;
    ++((sleep_tick_observer *)context)->ranges;
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status sleep_tick_cell(void *context, mc_world_runtime *world, int32_t x,
                                               int32_t y, int32_t z,
                                               mc_world_storage_error *error) {
    sleep_tick_observer *o = context;
    ++o->calls;
    mc_world_tick_error cleanup = {0};
    o->parent_busy = mc_world_runtime_tick_retire(o->parent, &cleanup) == MC_WORLD_STORAGE_BUSY &&
                     *o->parent != NULL;
    if (!o->parent_busy)
        return MC_WORLD_STORAGE_INVALID_DATA;
    if (x != 8 || y != 64 || z != 8)
        return MC_WORLD_STORAGE_OK;
    ++o->bed_calls;
    /* FD.e returns true on repeated in-domain metadata writes, so both wakes
     * notify. Apply each authored mutation once while observing every call. */
    if (o->bed_calls != 1)
        return MC_WORLD_STORAGE_OK;
    if (o->mode == 2 || o->mode == 4) {
        wt_scope scope = {0};
        scope.world = world;
        mc_world_entity_tick_error_init(&scope.error);
        if (o->mode == 2) {
            mc_world_entity_error append = {0};
            if (!mc_world_runtime_entity_append_owned(world, o->captured, &world->player_membership,
                                                      o->other, 45, &append)) {
                *error = append.cause;
                return append.status;
            }
        }
        const int32_t index = (int32_t)(o->captured->count - 1);
        if (!wt_list_remove(&scope, o->captured, index, 45)) {
            *error = scope.error.cause;
            return scope.error.status;
        }
    } else if (o->mode == 3) {
        o->registry_borrows = world->classes->registry->runtime_borrows;
        world->classes->registry->runtime_borrows = 0;
    } else if (o->mode == 5) {
        world->fields.d = NULL;
    } else if (o->mode == 6) {
        o->world_bindings = world->persistent_bindings;
        world->persistent_bindings = 0;
        *error = (mc_world_storage_error){MC_WORLD_STORAGE_IO_ERROR,
                                          "authored wake occupancy refusal", 0};
        return MC_WORLD_STORAGE_IO_ERROR;
    } else if (o->mode == 7) {
        o->info = world->fields.x;
        world->fields.x = NULL;
    }
    return MC_WORLD_STORAGE_OK;
}

static bool sleep_tick_weather_info(const mc_world_info *info, bool reset) {
    int32_t rain = -1, thunder = -1;
    bool raining = true, thundering = true;
    return mc_world_info_source_rain_time_get(info, &rain) == MC_WORLD_INFO_OK &&
           mc_world_info_source_thunder_time_get(info, &thunder) == MC_WORLD_INFO_OK &&
           mc_world_info_source_raining_get(info, &raining) == MC_WORLD_INFO_OK &&
           mc_world_info_source_thundering_get(info, &thundering) == MC_WORLD_INFO_OK &&
           (reset ? rain == 0 && thunder == 0 && !raining && !thundering
                  : rain != 0 && thunder != 0 && raining && thundering);
}

static bool sleep_tick_weather(mc_world_runtime *world, bool reset) {
    return sleep_tick_weather_info(world->fields.x, reset);
}

/* Preserve every actual cache-inserted Chunk owner across World retirement.
 * The general tick fixture keeps its existing bounded64 cleanup unchanged. */
static bool sleep_tick_cleanup(fixture *f) {
    mc_chunk_state **states = NULL;
    if (f != NULL && f->worlds[0] != NULL && f->bound[0]) {
        mc_chunk_cache *cache = f->providers[0].cache;
        const size_t count = cache->insertion_count;
        if (count > 512)
            return false;
        if (count != 0) {
            states = malloc(count * sizeof(*states));
            if (states == NULL)
                return false;
            memcpy(states, cache->insertions, count * sizeof(*states));
        }
        if (mc_world_runtime_retire_entity_containers(f->worlds[0], NULL) != MC_WORLD_STORAGE_OK) {
            free(states);
            return false;
        }
        f->terminal[0] = true;
        if (!retire_descriptors(f, 0) ||
            mc_world_runtime_retire_world(f->worlds[0], NULL) != MC_WORLD_STORAGE_OK) {
            free(states);
            return false;
        }
        f->world_retired[0] = true;
        for (size_t i = 0; i < count; ++i)
            if (mc_world_store_retire_chunk(f->stores[0], states[i], NULL) != MC_WORLD_STORAGE_OK) {
                free(states);
                return false;
            }
        f->states[0] = NULL;
        f->chunks[0] = NULL;
    }
    free(states);
    return cleanup_fixture(f);
}

static bool sleep_tick_case(const char *root, unsigned int mode, mc_math_classes *math) {
    bool ok = false, registered = false, observation_saved = false, restore_entered = false;
    unsigned int saved_entered = 0;
    fixture *f = calloc(1, sizeof(*f));
    mc_entity *players[2] = {NULL, NULL};
    mc_player_session_classes *sessions[2] = {NULL, NULL};
    mc_world_tick_call *call = NULL;
    mc_world_access *access = NULL;
    sleep_tick_observer observer = {.mode = mode, .parent = &call};
    bool observation = false;
    char directory[8192];
    CHECK(f != NULL && snprintf(directory, sizeof(directory), "%s/mode%u", root, mode) > 0 &&
          tick_setup_configured(f, directory, 512, mc_math_classes_table(math)));
    mc_world_runtime *world = f->worlds[0];
    /* The complete361 candidate domain is ordinary authored stored air NBT.
     * Seed it before observer registration; selected FD.n still loads every
     * real missing cache entry and runs all80 samples in every selected Chunk.
     * This fixture does not qualify terrain generation or elapsed sleep ticks. */
    for (int32_t x = -9; x <= 9; ++x)
        for (int32_t z = -9; z <= 9; ++z)
            if (x != 0 || z != 0)
                CHECK(tick_seed(f, 0, x, z));
    CHECK(sleep_tick_reset(f));
    world->fields.d->mod_count = INT32_MAX;
    for (size_t i = 0; i < COUNT(players); ++i) {
        CHECK(sleep_tick_player(f, &players[i], &sessions[i]));
        const mc_player_coordinates bed = {8, 64, 8};
        CHECK(mc_player_saved_spawn_set(players[i], &bed, NULL) == MC_PLAYER_OK);
        players[i]->local_player->fields.player.v =
            players[i]->local_player->fields.player.private_b;
        players[i]->local_player->fields.player.u = true;
        players[i]->local_player->fields.player.private_a = 100;
    }
    CHECK(world->fields.d->count == 2 && world->fields.d->mod_count == INT32_MIN + 1);
    observer.captured = world->fields.d;
    observer.other = players[1];
    for (int32_t x = 6; x <= 10; ++x)
        for (int32_t z = 6; z <= 10; ++z)
            CHECK(sleep_tick_put(f->states[0]->data, x, 63, z, 1, 0));
    CHECK(sleep_tick_put(f->states[0]->data, 8, 64, 8, 26, 12));
    world->fields.J = true;
    world->fields.N = world->fields.O = false;
    world->fields.p = 40;
    CHECK(mc_world_info_source_time_set(world->fields.x, 15000) == MC_WORLD_INFO_OK &&
          mc_world_info_source_rain_time_set(world->fields.x, 1000) == MC_WORLD_INFO_OK &&
          mc_world_info_source_raining_set(world->fields.x, true) == MC_WORLD_INFO_OK &&
          mc_world_info_source_thunder_time_set(world->fields.x, 1000) == MC_WORLD_INFO_OK &&
          mc_world_info_source_thundering_set(world->fields.x, true) == MC_WORLD_INFO_OK);
    const mc_world_access_callbacks callbacks = {
        .cell = sleep_tick_cell, .range = sleep_tick_range, .sky_changed = sleep_tick_sky};
    CHECK(mc_world_access_create(&callbacks, &observer, &access, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_add_world_access(world, access, NULL) == MC_WORLD_STORAGE_OK);
    registered = true;
    observation = f->registry[0]->blocks.t[26];
    f->registry[0]->blocks.t[26] = true;
    observation_saved = true;
    const size_t bindings = world->persistent_bindings;
    const size_t borrows = f->registry[0]->runtime_borrows;
    const mc_player_coordinates *first_v = players[0]->local_player->fields.player.v;
    if (mode == 1)
        players[0]->base.world = NULL;
    const mc_world_storage_status status = mc_world_runtime_tick_begin(world, math, &call, NULL);
    mc_world_tick_state state = {0};
    CHECK(call != NULL && mc_world_runtime_tick_inspect(call, &state) && !state.active &&
          state.sleeping && !world->fields.J && state.wake_calls >= 1);
    int64_t now = 0;
    const mc_world_info *observed_info = observer.info == NULL ? world->fields.x : observer.info;
    CHECK(mc_world_info_time_get(observed_info, &now) == MC_WORLD_INFO_OK && now >= 24000);
    if (mode == 1) {
        CHECK(status == MC_WORLD_STORAGE_UNSUPPORTED_STATE && !state.returned &&
              state.wake_calls == 1 && state.wake_returns == 0 && state.sleep != NULL &&
              state.error->child == MC_WORLD_TICK_CHILD_SLEEP && state.error->bytecode == 68 &&
              state.error->child_bytecode == 45 && state.error->descendant_bytecode == 48 &&
              state.error->sleep != NULL && state.error->sleep->source_exception &&
              !state.error->native_policy &&
              strcmp(state.error->exception_class, "java/lang/NullPointerException") == 0 &&
              players[0]->local_player->fields.player.u && now == 24000 &&
              sleep_tick_weather(world, false));
    } else if (mode == 2) {
        CHECK(status == MC_WORLD_STORAGE_UNSUPPORTED_STATE && !state.returned &&
              state.wake_calls == 1 && state.wake_returns == 1 && state.sleep == NULL &&
              state.error->sleep == NULL && state.error->bytecode == 68 &&
              state.error->child_bytecode == 25 && !state.error->native_policy &&
              strcmp(state.error->exception_class, "java/util/ConcurrentModificationException") ==
                  0 &&
              observer.captured->count == 2 && observer.captured->mod_count == INT32_MIN + 3 &&
              !players[0]->local_player->fields.player.u &&
              players[1]->local_player->fields.player.u && now == 24000 &&
              sleep_tick_weather(world, false));
    } else if (mode == 3) {
        CHECK(status != MC_WORLD_STORAGE_OK && !state.returned && state.wake_returns == 1 &&
              state.sleep != NULL && state.sleep->returned && state.error->sleep_cleanup != NULL &&
              state.error->native_policy && state.error->child_bytecode == 45 &&
              !players[0]->local_player->fields.player.u && now == 24000 &&
              sleep_tick_weather(world, false));
        f->registry[0]->runtime_borrows = observer.registry_borrows;
        observer.registry_borrows = 0;
    } else if (mode == 6) {
        CHECK(status == MC_WORLD_STORAGE_IO_ERROR && !state.returned && state.wake_calls == 1 &&
              state.wake_returns == 0 && state.error->sleep != NULL &&
              state.error->child_bytecode == 45 && players[0]->local_player->fields.player.u &&
              now == 24000 && sleep_tick_weather(world, false));
        mc_world_tick_error first = {0}, second = {0};
        CHECK(mc_world_runtime_tick_retire(&call, &first) == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
              call != NULL && first.sleep_cleanup != NULL &&
              first.sleep_cleanup->child == MC_PLAYER_SLEEP_CLEANUP_WORLD);
        world->persistent_bindings = observer.world_bindings;
        observer.world_bindings = 0;
        saved_entered = f->registry[0]->entered;
        f->registry[0]->entered = 1;
        restore_entered = true;
        const mc_world_storage_status second_status = mc_world_runtime_tick_retire(&call, &second);
        f->registry[0]->entered = saved_entered;
        restore_entered = false;
        CHECK(second_status == MC_WORLD_STORAGE_BUSY && call != NULL &&
              second.sleep_cleanup != NULL &&
              second.sleep_cleanup->child == MC_PLAYER_SLEEP_CLEANUP_REGISTRY &&
              second.sleep_cleanup->registry.status == MC_REGISTRY_BUSY &&
              second.sleep_cleanup->world.status == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
              mc_world_runtime_tick_inspect(call, &state) &&
              state.error->status == MC_WORLD_STORAGE_IO_ERROR &&
              state.error->child_bytecode == 45 && observer.calls == 1 &&
              sleep_tick_weather(world, false));
    } else if (mode == 7) {
        CHECK(status == MC_WORLD_STORAGE_UNSUPPORTED_STATE && !state.returned &&
              state.wake_calls == 2 && state.wake_returns == 2 && state.sleep == NULL &&
              state.error->bytecode == 68 && state.error->child_bytecode == 52 &&
              state.error->descendant_bytecode == 5 && !state.error->native_policy &&
              strcmp(state.error->source_class, "fd") == 0 &&
              strcmp(state.error->source_method, "z()V") == 0 &&
              strcmp(state.error->exception_class, "java/lang/NullPointerException") == 0 &&
              !players[0]->local_player->fields.player.u &&
              !players[1]->local_player->fields.player.u && world->fields.x == NULL &&
              now == 24000 && sleep_tick_weather_info(observed_info, false));
    } else {
        const bool completed =
            (mode == 5 ? status != MC_WORLD_STORAGE_OK : status == MC_WORLD_STORAGE_OK) &&
            state.wake_calls == (mode == 4 ? 1u : 2u) && state.wake_returns == state.wake_calls &&
            state.sleep == NULL && state.error->sleep == NULL && sleep_tick_weather(world, true) &&
            now == 24001 && !players[0]->local_player->fields.player.u &&
            players[0]->local_player->fields.player.v == first_v &&
            players[0]->local_player->fields.player.private_b != first_v &&
            players[0]->local_player->fields.player.private_b->x == 8 &&
            players[0]->local_player->fields.player.private_b->y == 64 &&
            players[0]->local_player->fields.player.private_b->z == 8;
        if (!completed)
            fprintf(stderr,
                    "World sleep fixture failure: mode=%u status=%d source=%u child=%u "
                    "operation=%s wakes=%zu/%zu candidates=%zu cells=%u bed_cells=%u ranges=%zu\n",
                    mode, (int)status, state.source_pc, state.error->child_bytecode,
                    state.error->operation == NULL ? "-" : state.error->operation, state.wake_calls,
                    state.wake_returns, state.random_chunk_count, observer.calls,
                    observer.bed_calls, observer.ranges);
        CHECK(completed);
        if (mode == 0 || mode == 4) {
            /* Stored air does not guarantee a range notification. Count every
             * actual call, while requiring the complete source work itself. */
            CHECK(state.returned && state.source_pc == 198 && state.random != NULL &&
                  state.random->returned && state.random_chunk_count == 361 &&
                  state.random->samples_returned == 361u * 80u);
        }
        if (mode == 5)
            CHECK(world->fields.d == NULL && !players[1]->local_player->fields.player.u);
        if (mode == 4)
            CHECK(observer.captured->count == 1 && players[1]->local_player->fields.player.u);
    }
    if (mode != 1)
        CHECK(observer.bed_calls == (mode == 0 || mode == 5 || mode == 7 ? 2u : 1u) &&
              observer.calls >= observer.bed_calls && observer.parent_busy);
    const unsigned int calls = observer.calls;
    players[0]->base.world = world;
    if (observer.info != NULL)
        world->fields.x = observer.info;
    CHECK(mc_world_runtime_tick_retire(&call, NULL) == MC_WORLD_STORAGE_OK && call == NULL &&
          observer.calls == calls && f->registry[0]->runtime_borrows == borrows &&
          world->persistent_bindings == bindings);
    CHECK(printf("SLEEP_RANGE %u %zu %u %u\n", mode, observer.ranges, observer.calls,
                 observer.bed_calls) > 0);
    ok = true;
cleanup:
    if (f != NULL && f->worlds[0] != NULL) {
        if (players[0] != NULL)
            players[0]->base.world = f->worlds[0];
        if (observer.registry_borrows != 0)
            f->registry[0]->runtime_borrows = observer.registry_borrows;
        if (observer.world_bindings != 0)
            f->worlds[0]->persistent_bindings = observer.world_bindings;
        if (restore_entered)
            f->registry[0]->entered = saved_entered;
        if (observer.info != NULL)
            f->worlds[0]->fields.x = observer.info;
        if (observer.captured != NULL)
            f->worlds[0]->fields.d = observer.captured;
        if (observation_saved)
            f->registry[0]->blocks.t[26] = observation;
    }
    if (mc_world_runtime_tick_retire(&call, NULL) != MC_WORLD_STORAGE_OK)
        ok = false;
    if (registered) {
        bool ignored;
        if (mc_world_runtime_remove_world_access(f->worlds[0], access, &ignored, NULL) !=
            MC_WORLD_STORAGE_OK)
            ok = false;
    }
    if (mc_world_access_free(access, NULL) != MC_WORLD_STORAGE_OK)
        ok = false;
    for (size_t i = 0; i < COUNT(players); ++i) {
        mc_entity_release(players[i]);
        if (mc_player_session_classes_free(&sessions[i], NULL) != MC_PLAYER_OK)
            ok = false;
    }
    if (!sleep_tick_cleanup(f))
        ok = false;
    return ok;
}

int main(int argc, char **argv) {
    int result = 1;
    mc_resource_files *files = NULL;
    mc_math_classes *math = NULL;
    char root[8192], resources_path[8192];
    if (argc != 4 || strcmp(argv[1], "sleep-tick") != 0 ||
        !decode_path(argv[2], root, sizeof(root)) ||
        !decode_path(argv[3], resources_path, sizeof(resources_path)) || !mc_math_trig_init(&trig))
        return 2;
    CHECK(mc_resource_files_create(resources_path, &files) == MC_REGISTRY_OK &&
          mc_math_classes_create(SIZE_MAX, &math, NULL) == MC_MATH_CLASS_OK &&
          mc_math_classes_ensure(math, NULL) == MC_MATH_CLASS_OK);
    tick_resources = mc_resource_files_source(files);
    for (unsigned int mode = 0; mode < 8; ++mode)
        CHECK(sleep_tick_case(root, mode, math));
    result = 0;
cleanup:
    if (mc_math_classes_release(&math, NULL) != MC_MATH_CLASS_OK)
        return 1;
    mc_resource_files_free(files);
    if (result == 0 && printf("World sleep tick: %u checks\n", checks) < 0)
        return 1;
    return result;
}
