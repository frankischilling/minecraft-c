/* Direct FD.n contracts with genuine Worlds, Players, cached Chunks and Math. */
#include "../../../src/block/registry/registry_internal.h"
#include "../../../src/core/java/random_profile_internal.h"
#include "../../../src/entity/entity_internal.h"
#include "../../../src/world/chunk/object_state_internal.h"
#include "../../../src/world/runtime/chunk_provider_internal.h"
#include "../../../src/world/runtime/construction_internal.h"
#include "../../../src/world/runtime/entity_publication_internal.h"
#include "../../../src/world/runtime/runtime_internal.h"
#include "../../../src/world/runtime/world_tick_random_internal.h"
#include "../../../src/world/storage/files.h"
#include "../../../src/world/storage/objects_internal.h"
#include "mc/entity/lightning/construction.h"
#include "mc/entity/player/player.h"
#include "mc/resources/files/files.h"
#include "mc/statistics/registry/registry.h"
#include "mc/world/biome/registry.h"
#include "mc/world/dimension/world_chunk_manager.h"
#include "mc/world/dimension/world_provider.h"
#include "mc/world/runtime/random_tick.h"
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned int checks;
#define COUNT(a) (sizeof(a) / sizeof((a)[0]))
#define CHECK(c)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(c)) {                                                                                \
            fprintf(stderr, "Random tick check%u line%d: %s\n", checks, __LINE__, #c);             \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

typedef struct random_fixture {
    mc_resource_files *files;
    mc_materials *materials;
    mc_java_random_profile *profile;
    mc_entity_classes *entities;
    mc_registry_classes *registry;
    mc_math_classes *math;
    mc_world_runtime_classes *classes;
    mc_world_regions *regions;
    mc_world_store *store;
    mc_world_provider *dimension;
    mc_world_runtime *world;
    mc_world_info_name *name;
    mc_player_session_classes *sessions;
    mc_entity *player;
    mc_chunk_provider_fields provider;
    mc_chunk_state *owned[441];
    size_t owned_count;
    mc_world_random_tick_call *call;
    mc_world_access *access;
    mc_chunk_state *null_chunk;
    uint8_t *null_chunk_bytes;
    mc_java_random *owned_random;
    mc_block *stone;
    bool stone_tick, stone_control;
    unsigned int index;
    size_t sounds, cells, ranges;
    bool callback_bad;
    FILE *events;
} random_fixture;

static const char *const scenarios[] = {
    "empty_players", "air_qhigh",          "stone_q0",     "air_ambient",     "sound_failure",
    "P_mutation",    "blank_grass_remote", "raw_null",     "tick_entry_null", "snow_HD",
    "ice_HD",        "lightning_HD",       "normal_alias", "random_null",     "native_h_ignored"};

static FILE *open_file(const char *path, const char *mode) {
#ifdef _MSC_VER
    FILE *out = NULL;
    return fopen_s(&out, path, mode) == 0 ? out : NULL;
#else
    return fopen(path, mode);
#endif
}

static uint64_t double_bits(double value) {
    uint64_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static uint32_t float_bits(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static bool put_i32(FILE *out, int32_t value) {
    const uint32_t v = (uint32_t)value;
    const uint8_t bytes[4] = {(uint8_t)(v >> 24), (uint8_t)(v >> 16), (uint8_t)(v >> 8),
                              (uint8_t)v};
    return fwrite(bytes, 1, sizeof(bytes), out) == sizeof(bytes);
}

static bool put_array(FILE *out, const uint8_t *array, size_t length) {
    return array == NULL ? put_i32(out, -1)
                         : length <= INT32_MAX && put_i32(out, (int32_t)length) &&
                               fwrite(array, 1, length, out) == length;
}

static bool setup(random_fixture *f, const char *resources, const char *directory) {
    CHECK(mc_resource_files_create(resources, &f->files) == MC_REGISTRY_OK &&
          mc_materials_create(&f->materials));
    CHECK(mc_java_random_profile_create(SIZE_MAX, &f->profile) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_entity_classes_create(MC_ENTITY_CLIENT, f->profile, NULL, &f->entities) ==
          MC_ENTITY_OK);
    const mc_registry_options registry = {MC_REGISTRY_CLIENT, f->profile, f->materials, 0,
                                          mc_resource_files_source(f->files)};
    CHECK(mc_registry_classes_allocate(&registry, &f->registry, NULL) == MC_REGISTRY_OK &&
          mc_registry_classes_initialize_statistics(f->registry, NULL) == MC_REGISTRY_OK);
    CHECK(mc_math_classes_create(SIZE_MAX, &f->math, NULL) == MC_MATH_CLASS_OK &&
          mc_math_classes_ensure(f->math, NULL) == MC_MATH_CLASS_OK);
    CHECK(mc_world_runtime_classes_create(MC_WORLD_INFO_CLIENT, f->profile,
                                          mc_math_classes_table(f->math), &f->classes,
                                          NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_classes_bind_registry(f->classes, f->registry, NULL) == MC_REGISTRY_OK &&
          mc_world_runtime_classes_bind_entities(f->classes, f->entities) == MC_ENTITY_OK);
    CHECK(mc_world_regions_create(NULL, &f->regions, NULL) == MC_WORLD_STORAGE_OK);
    uint8_t map[256] = {0};
    mc_registry_block_view blocks;
    CHECK(mc_registry_classes_inspect_blocks(f->registry, &blocks, NULL) == MC_REGISTRY_OK);
    for (size_t i = 1; i < COUNT(map); ++i)
        map[i] = blocks.m[i] == NULL ? 0 : (uint8_t)i;
    const mc_world_store_options store = {.session_token = 761,
                                          .block_map = map,
                                          .block_map_length = sizeof(map),
                                          .limits = mc_world_store_default_limits()};
    CHECK(mc_world_store_open(directory, f->regions, &store, &f->store, NULL) ==
          MC_WORLD_STORAGE_OK);
    const uint16_t title[] = {'R', 'a', 'n', 'd', 'o', 'm'};
    CHECK(mc_world_info_name_copy((mc_nbt_string){title, COUNT(title)}, SIZE_MAX, &f->name) ==
          MC_WORLD_INFO_OK);
    CHECK(mc_world_provider_create(0, NULL, &f->dimension, NULL) == MC_WORLD_PROVIDER_OK);
    mc_world_runtime_construction_limits limits = mc_world_runtime_default_construction_limits();
    limits.max_provider_chunks = 512;
    CHECK(mc_world_runtime_allocate_world(MC_WORLD_RUNTIME_CLIENT_DIRECT_METADATA, f->classes,
                                          &limits, &f->world, NULL) == MC_WORLD_STORAGE_OK);
    const mc_world_runtime_world_arguments arguments = {
        .store = f->store, .name = f->name, .seed = 23, .dimension = f->dimension};
    CHECK(mc_world_runtime_construct_world(f->world, &arguments, NULL) == MC_WORLD_STORAGE_OK &&
          mc_chunk_provider_bind_fields(f->world->fields.v, &f->provider, NULL) ==
              MC_WORLD_STORAGE_OK);
    CHECK(mc_player_session_classes_create(f->entities, f->registry, &f->sessions, NULL) ==
          MC_PLAYER_OK);
    return true;
}

static bool load_chunk(random_fixture *f, int32_t x, int32_t z, mc_chunk_state **out) {
    mc_chunk_state *chunk = NULL;
    CHECK(f->owned_count < COUNT(f->owned));
    CHECK(mc_world_store_load_game_chunk(f->store, f->world, 0, x, z, &chunk, NULL) ==
              MC_WORLD_STORAGE_OK &&
          chunk && !chunk->blank);
    f->owned[f->owned_count++] = chunk;
    *out = chunk;
    return true;
}

static bool player(random_fixture *f) {
    mc_player_session *session = NULL;
    mc_player_error error;
    CHECK(mc_player_session_create(f->sessions, NULL, NULL, &session, &error) == MC_PLAYER_OK);
    CHECK(mc_local_player_allocate(f->entities, &f->player, NULL) == MC_ENTITY_OK);
    const mc_player_status status =
        mc_local_player_construct(f->player, f->world, NULL, session, 0, NULL, &error);
    mc_player_session_retire(&session);
    CHECK(status == MC_PLAYER_OK && f->player->phase == MC_ENTITY_READY);
    CHECK(mc_entity_set_position(f->player, .5, 64.5, 4.5) == MC_ENTITY_OK);
    mc_world_entity_error publication;
    CHECK(mc_world_runtime_call_enter(f->world, NULL) == MC_WORLD_STORAGE_OK);
    const bool appended = mc_world_runtime_entity_append_owned(
        f->world, f->world->fields.d, &f->world->player_membership, f->player, 0, &publication);
    mc_world_runtime_call_leave(f->world);
    CHECK(appended && f->world->player_membership.count == 1 &&
          f->world->entity_membership.count == 0);
    return true;
}

static bool prepare_chunks(random_fixture *f) {
    if (f->index == 0)
        return true;
    const bool hd = f->index >= 9 && f->index <= 11;
    const int32_t radius = hd ? 10 : 9;
    mc_chunk_state *shared = NULL;
    if (f->index == 6)
        shared = &f->world->fallback;
    else if (f->index == 12)
        CHECK(load_chunk(f, 0, 0, &shared));
    for (int32_t x = -radius; x <= radius; ++x)
        for (int32_t z = -radius; z <= radius; ++z) {
            mc_chunk_state *chunk = shared;
            if (!chunk)
                CHECK(load_chunk(f, x, z, &chunk));
            CHECK(mc_chunk_cache_publish(f->provider.cache, x, z, chunk));
        }
    if (f->index == 6)
        memset(shared->data->blocks, 2, MC_CHUNK_BLOCKS);
    CHECK(f->provider.cache->count == (hd ? 441u : 361u));
    return true;
}

static mc_world_storage_status cell(void *owner, mc_world_runtime *world, int32_t x, int32_t y,
                                    int32_t z, mc_world_storage_error *error) {
    random_fixture *f = owner;
    if (world != f->world)
        f->callback_bad = true;
    fprintf(f->events, "RANDOM_CELL %u %zu %d %d %d\n", f->index, f->cells++, x, y, z);
    *error = (mc_world_storage_error){0};
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status range(void *owner, mc_world_runtime *world, int32_t ax, int32_t ay,
                                     int32_t az, int32_t bx, int32_t by, int32_t bz,
                                     mc_world_storage_error *error) {
    random_fixture *f = owner;
    if (world != f->world)
        f->callback_bad = true;
    fprintf(f->events, "RANDOM_RANGE %u %zu %d %d %d %d %d %d\n", f->index, f->ranges++, ax, ay, az,
            bx, by, bz);
    *error = (mc_world_storage_error){0};
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status sound(void *owner, mc_world_runtime *world,
                                     const mc_registry_string *name, double x, double y, double z,
                                     float volume, float pitch, mc_world_storage_error *error) {
    random_fixture *f = owner;
    mc_registry_string_view text = {0};
    mc_world_random_tick_state state = {0};
    if (world != f->world || !mc_registry_string_read(name, &text) || text.length != 17 ||
        !mc_world_runtime_random_tick_inspect(f->call, &state) || !state.active ||
        state.chunk.captured == NULL || world->local_leases == 0 || world->active_calls < 2)
        f->callback_bad = true;
    static const char expected[] = "ambient.cave.cave";
    if (!f->callback_bad)
        for (size_t i = 0; i < text.length; ++i)
            if (text.units[i] != (uint16_t)(unsigned char)expected[i])
                f->callback_bad = true;
    mc_world_random_tick_error cleanup;
    if (mc_world_runtime_random_tick_retire(&f->call, &cleanup) != MC_WORLD_STORAGE_BUSY ||
        !f->call || mc_world_runtime_retire_world(world, NULL) != MC_WORLD_STORAGE_BUSY ||
        mc_world_store_entity_containers_validate(f->store, world, NULL) != MC_WORLD_STORAGE_BUSY)
        f->callback_bad = true;
    fprintf(f->events,
            "RANDOM_SOUND %u %zu %016" PRIx64 " %016" PRIx64 " %016" PRIx64 " %08" PRIx32
            " %08" PRIx32 "\n",
            f->index, f->sounds++, double_bits(x), double_bits(y), double_bits(z),
            float_bits(volume), float_bits(pitch));
    if (f->index == 5) {
        mc_world_random_tick_error cleared;
        if (mc_world_random_tick_active_set_clear(world, &cleared) != MC_WORLD_STORAGE_OK)
            f->callback_bad = true;
    }
    if (f->index == 4) {
        *error = (mc_world_storage_error){MC_WORLD_STORAGE_IO_ERROR,
                                          "authored native sound observer failure", 0};
        return error->status;
    }
    *error = (mc_world_storage_error){0};
    return MC_WORLD_STORAGE_OK;
}

static bool prepare_access(random_fixture *f) {
    const mc_world_access_callbacks callbacks = {.cell = cell, .range = range, .sound = sound};
    CHECK(mc_world_access_create(&callbacks, f, &f->access, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_add_world_access(f->world, f->access, NULL) == MC_WORLD_STORAGE_OK);
    return true;
}

typedef struct point {
    int32_t x, z;
} point;

static int32_t pair_hash(int32_t x, int32_t z) {
    const uint32_t word = (x < 0 ? UINT32_C(0x80000000) : 0) | (((uint32_t)x & 32767u) << 16) |
                          (z < 0 ? 32768u : 0) | ((uint32_t)z & 32767u);
    int32_t value;
    memcpy(&value, &word, sizeof(value));
    return value;
}

static bool make_order(point *points) {
    mc_java_int_set *set = NULL;
    CHECK(mc_java_int_set_create(NULL, &set) == MC_JAVA_INT_SET_OK);
    for (int32_t x = -9; x <= 9; ++x)
        for (int32_t z = -9; z <= 9; ++z) {
            bool added = false;
            CHECK(mc_java_int_set_add(set, pair_hash(x, z), &added) == MC_JAVA_INT_SET_OK && added);
        }
    int32_t keys[361];
    size_t written = 0;
    CHECK(mc_java_int_set_copy_order(set, keys, COUNT(keys), &written) == MC_JAVA_INT_SET_OK &&
          written == COUNT(keys));
    for (size_t i = 0; i < written; ++i) {
        bool found = false;
        for (int32_t x = -9; x <= 9; ++x)
            for (int32_t z = -9; z <= 9; ++z)
                if (pair_hash(x, z) == keys[i]) {
                    points[i] = (point){x, z};
                    found = true;
                }
        CHECK(found);
    }
    mc_java_int_set_free(set);
    CHECK(points[0].x == -2 && points[0].z == -2);
    return true;
}

static int32_t step(int32_t g) {
    return mc_java_i32_add(mc_java_i32_mul(g, 3), 1013904223);
}

/* The integration oracle consumes only the declared constant terrain/profile
 * operands and separately qualified Random/HashSet primitives. It never runs
 * FD.n or executes against copied Chunk buffers. Every actual draw/read/tick
 * under test still comes from the real retained FD.n entry below. */
typedef struct oracle {
    mc_java_random random;
    int32_t g, q;
    unsigned int stop;
    size_t chunks_started, chunks_returned, samples_started, samples_returned;
    size_t random_calls, updates, ambient_attempts, ambient_sounds, snow_attempts;
    size_t snow_writes, ice_writes, lightning, ticks, sound_events;
} oracle;

static bool oracle_draw(oracle *o, int32_t bound, int32_t *value) {
    ++o->random_calls;
    return mc_java_random_next_bounded(&o->random, bound, value);
}

static int32_t oracle_step(oracle *o) {
    o->g = step(o->g);
    ++o->updates;
    return mc_java_i32_shr(o->g, 2);
}

static bool expected(unsigned int index, const point *order, oracle *o) {
    memset(o, 0, sizeof(*o));
    o->g = index >= 3 && index <= 5 ? 10 : 17;
    o->q = index == 0 || (index >= 3 && index <= 5) ? 1 : index == 2 || index == 12 ? 0 : 6001;
    mc_java_random_seed(&o->random, index == 11 ? INT64_C(107038380838084) : 0);
    if (o->q > 0)
        --o->q;
    if (index == 0) {
        o->stop = 994;
        return true;
    }
    for (size_t chunk = 0; chunk < 361; ++chunk) {
        ++o->chunks_started;
        bool clear = false;
        if (o->q == 0) {
            ++o->ambient_attempts;
            const int32_t v = oracle_step(o);
            if (index != 2 && index != 12) {
                int32_t ignored;
                CHECK(oracle_draw(o, 8, &ignored));
                const double dx = (double)(order[chunk].x * 16 + (v & 15));
                const double dy = (double)((mc_java_i32_shr(v, 16) & 127) - 64);
                const double dz = (double)(order[chunk].z * 16 + (mc_java_i32_shr(v, 8) & 15) - 4);
                const double distance = dx * dx + dy * dy + dz * dz;
                if (distance > 4 && distance < 64) {
                    (void)mc_java_random_next_float(&o->random);
                    ++o->random_calls;
                    ++o->sound_events;
                    if (index == 4) {
                        o->stop = 443;
                        return true;
                    }
                    ++o->ambient_sounds;
                    CHECK(oracle_draw(o, 12000, &ignored));
                    o->q = ignored + 6000;
                    clear = index == 5;
                }
            }
        }
        if (index == 13) {
            o->stop = 470;
            return true;
        }
        int32_t ticket;
        CHECK(oracle_draw(o, 100000, &ticket));
        if (ticket == 0 && index == 11) {
            (void)oracle_step(o);
            ++o->lightning;
        }
        CHECK(oracle_draw(o, 16, &ticket));
        int32_t weather_x = -1, weather_z = -1;
        if (ticket == 0) {
            ++o->snow_attempts;
            const int32_t v = oracle_step(o);
            weather_x = v & 15;
            weather_z = mc_java_i32_shr(v, 8) & 15;
            if (index == 9)
                ++o->snow_writes;
            if (index == 10)
                ++o->ice_writes;
        }
        for (size_t sample = 0; sample < 80; ++sample) {
            const int32_t v = oracle_step(o);
            ++o->samples_started;
            if (index == 7 || index == 8) {
                o->stop = index == 7 ? 944 : 982;
                return true;
            }
            const int32_t x = v & 15, y = mc_java_i32_shr(v, 16) & 127,
                          z = mc_java_i32_shr(v, 8) & 15;
            if (index == 6 || ((index == 9 || index == 10) && x == weather_x && z == weather_z &&
                               y == (index == 9 ? 64 : 63)))
                ++o->ticks;
            ++o->samples_returned;
        }
        ++o->chunks_returned;
        if (clear) {
            o->stop = 172;
            return true;
        }
    }
    o->stop = 994;
    return true;
}

static bool prepare_ice(random_fixture *f, const point *order) {
    mc_java_random random;
    mc_java_random_seed(&random, 0);
    int32_t g = 17;
    for (size_t i = 0; i < 361; ++i) {
        int32_t ignored, ticket;
        CHECK(mc_java_random_next_bounded(&random, 100000, &ignored) &&
              mc_java_random_next_bounded(&random, 16, &ticket));
        if (ticket == 0) {
            g = step(g);
            const int32_t v = mc_java_i32_shr(g, 2);
            mc_chunk_state *chunk = NULL;
            CHECK(mc_chunk_cache_lookup(f->provider.cache, order[i].x, order[i].z, &chunk) &&
                  chunk);
            const size_t cell_index =
                (size_t)(((v & 15) << 11) | ((mc_java_i32_shr(v, 8) & 15) << 7) | 63);
            chunk->data->blocks[cell_index] = 9;
        }
        for (size_t sample = 0; sample < 80; ++sample)
            g = step(g);
    }
    return true;
}

static bool prepare_hd(random_fixture *f) {
    if (f->index < 9 || f->index > 11)
        return true;
    const mc_biome *biome = NULL;
    CHECK(mc_biome_static_get(f->registry, f->index == 11 ? 'i' : 'k', &biome, NULL) ==
              MC_REGISTRY_OK &&
          biome);
    const mc_world_provider_registry_binding binding = {f->registry, MC_REGISTRY_CLIENT,
                                                        f->profile};
    mc_world_chunk_manager *manager = NULL;
    mc_world_provider_error error;
    CHECK(mc_world_chunk_manager_fixed_create_game(f->dimension, &binding, biome, .5, .5, &manager,
                                                   &error) == MC_WORLD_PROVIDER_OK &&
          manager);
    const mc_world_provider_status status =
        mc_world_provider_assign_chunk_manager_game(f->dimension, manager, &error);
    mc_world_chunk_manager_release(manager);
    CHECK(status == MC_WORLD_PROVIDER_OK);
    return true;
}

static bool dump_streams(random_fixture *f, const char *directory, size_t *unique_count) {
    static const char *const names[] = {"raw", "nibbles", "height", "membership", "P"};
    FILE *streams[5] = {0};
    for (size_t i = 0; i < COUNT(streams); ++i) {
        char path[8192];
        const int n =
            snprintf(path, sizeof(path), "%s/case%u-%s.bin", directory, f->index, names[i]);
        CHECK(n > 0 && (size_t)n < sizeof(path) && (streams[i] = open_file(path, "wb")) != NULL);
    }
    mc_chunk_state *identities[441];
    size_t count = 0;
    const int32_t radius = f->index >= 9 && f->index <= 11 ? 10 : 9;
    if (f->provider.cache->count)
        for (int32_t x = -radius; x <= radius; ++x)
            for (int32_t z = -radius; z <= radius; ++z) {
                mc_chunk_state *chunk = NULL;
                CHECK(mc_chunk_cache_lookup(f->provider.cache, x, z, &chunk) && chunk);
                size_t ordinal = 0;
                while (ordinal < count && identities[ordinal] != chunk)
                    ++ordinal;
                if (ordinal == count) {
                    CHECK(count < COUNT(identities));
                    identities[count++] = chunk;
                    if (chunk == &f->world->fallback) {
                        CHECK(put_array(streams[0], chunk->data->blocks,
                                        chunk->data->blocks_capacity));
                        CHECK(
                            put_array(streams[1], chunk->data->metadata, MC_CHUNK_NIBBLE_BYTES) &&
                            put_array(streams[1], chunk->data->skylight, MC_CHUNK_NIBBLE_BYTES) &&
                            put_array(streams[1], chunk->data->block_light, MC_CHUNK_NIBBLE_BYTES));
                        CHECK(put_array(streams[2], chunk->data->heightmap,
                                        MC_CHUNK_HEIGHTMAP_BYTES));
                    } else {
                        mc_world_chunk *object = mc_world_store_chunk_object(f->store, chunk);
                        mc_world_chunk_fields fields;
                        CHECK(object &&
                              mc_world_chunk_fields_get(object, &fields) == MC_WORLD_STORAGE_OK);
                        CHECK(put_array(streams[0], fields.b, chunk->data->blocks_capacity));
                        const mc_world_chunk_nibble *arrays[3] = {fields.e, fields.f, fields.g};
                        for (size_t i = 0; i < COUNT(arrays); ++i) {
                            mc_world_chunk_nibble_fields words;
                            CHECK(mc_world_chunk_nibble_fields_get(arrays[i], &words) ==
                                      MC_WORLD_STORAGE_OK &&
                                  put_array(streams[1], words.a, words.length));
                        }
                        CHECK(put_array(streams[2], fields.h, MC_CHUNK_HEIGHTMAP_BYTES));
                    }
                }
                CHECK(put_i32(streams[3], x) && put_i32(streams[3], z) &&
                      put_i32(streams[3], chunk->blank ? 1 : 0) && put_i32(streams[3], chunk->x) &&
                      put_i32(streams[3], chunk->z) && put_i32(streams[3], (int32_t)ordinal + 1));
            }
    mc_world_active_chunks *set = f->world->fields.P;
    int32_t *keys = set->count ? malloc(set->count * sizeof(*keys)) : NULL;
    CHECK(!set->count || keys);
    size_t written = 0;
    CHECK(mc_java_int_set_copy_order(set->order, keys, set->count, &written) ==
              MC_JAVA_INT_SET_OK &&
          written == set->count);
    for (size_t i = 0; i < written; ++i) {
        const mc_world_chunk_candidate *entry = NULL;
        for (size_t j = 0; j < set->count; ++j)
            if (set->entries[j].hash == keys[i])
                entry = &set->entries[j];
        CHECK(entry && put_i32(streams[4], entry->x) && put_i32(streams[4], entry->z));
    }
    free(keys);
    for (size_t i = 0; i < COUNT(streams); ++i)
        CHECK(fclose(streams[i]) == 0);
    *unique_count = count;
    return true;
}

static bool observe(random_fixture *f, const char *directory,
                    const mc_world_random_tick_state *state, mc_world_storage_status status) {
    size_t unique = 0;
    CHECK(dump_streams(f, directory, &unique));
    fprintf(f->events, "RANDOM_FRAME %u %s %d %08" PRIx32 " %d ", f->index, scenarios[f->index],
            status == MC_WORLD_STORAGE_OK ? 1 : -1, (uint32_t)f->world->fields.g,
            f->world->fields.Q);
    if (f->world->fields.r)
        fprintf(f->events, "%012" PRIx64, f->world->fields.r->state);
    else
        fputc('-', f->events);
    fprintf(f->events, " %d %zu %zu %zu %zu %zu %zu %zu %zu\n", f->world->fields.m,
            f->world->fields.P->count, f->provider.cache->count, unique,
            f->world->entity_membership.count, f->world->weather_membership.count, f->sounds,
            f->cells, f->ranges);
    fprintf(
        f->events,
        "NATIVE_RANDOM_STATE %u %u %zu %zu %zu %zu %zu %zu %zu %zu %zu %zu %zu %zu %zu %zu %d\n",
        f->index, state->source_pc, state->chunk_count, state->chunks_started,
        state->chunks_returned, state->samples_started, state->samples_returned, state->block_ticks,
        state->random_calls, state->lcg_updates, state->ambient_attempts, state->ambient_sounds,
        state->snow_attempts, state->snow_writes, state->ice_writes, state->weather_publications,
        f->world->fields.h);
    if (status != MC_WORLD_STORAGE_OK)
        fprintf(f->events, "RANDOM_FAILURE %u %u %u %s\n", f->index, state->source_pc,
                state->error->native_policy ? 1u : 0u,
                state->error->exception_class ? state->error->exception_class : "native");
    CHECK(fflush(f->events) == 0);
    return true;
}

static bool failed_inputs_preserved(const random_fixture *f) {
    return (!f->null_chunk || f->null_chunk->data->blocks == NULL) &&
           (!f->owned_random || f->world->fields.r == (f->index == 13 ? NULL : f->owned_random)) &&
           (!f->stone_control || (f->registry->blocks.n[1] && f->registry->blocks.m[1] == NULL));
}

static bool run(random_fixture *f, const char *directory) {
    point order[361];
    CHECK(make_order(order));
    if (f->index != 0)
        CHECK(prepare_chunks(f) && player(f));
    CHECK(prepare_hd(f));
    if (f->index == 10)
        CHECK(prepare_ice(f, order));
    for (size_t i = 0; i < f->owned_count; ++i)
        f->owned[i]->data->minimum_height = f->index == 2 || f->index == 8 || f->index == 12 ? 128
                                            : f->index >= 9 && f->index <= 11                ? 64
                                                                                             : 0;
    if (f->index == 6)
        f->world->fallback.terrain_populated = true;
    mc_world_runtime *w = f->world;
    w->fields.B = f->index == 6;
    w->fields.q = 1;
    w->fields.f = w->fields.m = 0;
    w->fields.i = w->fields.j = f->index == 9 || f->index == 11 ? 1.f : 0.f;
    w->fields.k = w->fields.l = f->index == 11 ? 1.f : 0.f;
    w->fields.g = f->index >= 3 && f->index <= 5 ? 10 : 17;
    w->fields.Q = f->index == 0 || (f->index >= 3 && f->index <= 5) ? 1
                  : f->index == 2 || f->index == 12                 ? 0
                                                                    : 6001;
    CHECK(w->fields.h == 1013904223);
    if (f->index == 14)
        w->fields.h = INT32_MIN;
    const int32_t h = w->fields.h;
    mc_java_random_reseed(w->fields.r, f->index == 11 ? INT64_C(107038380838084) : 0);
    CHECK(prepare_access(f));
    CHECK(mc_entity_bounds_pool_reset(f->entities, NULL) == MC_ENTITY_OK &&
          mc_entity_vector_pool_reset(f->entities, NULL) == MC_ENTITY_OK);
    const uint64_t u = atomic_load(&f->profile->uniquifier);
    const unsigned int holder = atomic_load(&f->profile->holder_phase);
    const uint64_t math_random = f->profile->math ? atomic_load(&f->profile->math->state) : 0;
    const mc_java_random own = f->player ? *f->player->random : (mc_java_random){0};
    mc_entity_class_snapshot before_entities;
    mc_math_class_state before_math;
    CHECK(mc_entity_classes_get_snapshot(f->entities, &before_entities) == MC_ENTITY_OK &&
          mc_math_classes_state(f->math, &before_math, NULL) == MC_MATH_CLASS_OK);
    const size_t bindings = w->persistent_bindings;
    if (f->index == 7) {
        CHECK(mc_chunk_cache_lookup(f->provider.cache, order[0].x, order[0].z, &f->null_chunk) &&
              f->null_chunk);
        f->null_chunk_bytes = f->null_chunk->data->blocks;
        f->null_chunk->data->blocks = NULL;
    }
    if (f->index == 8) {
        f->stone = f->registry->blocks.m[1];
        f->stone_tick = f->registry->blocks.n[1];
        f->stone_control = true;
        f->registry->blocks.n[1] = true;
        f->registry->blocks.m[1] = NULL;
    }
    f->owned_random = w->fields.r;
    if (f->index == 13)
        w->fields.r = NULL;
    mc_world_random_tick_error admission;
    const mc_world_storage_status status =
        mc_world_runtime_random_tick_begin(w, f->math, &f->call, &admission);
    mc_world_random_tick_state state;
    const bool inspected = mc_world_runtime_random_tick_inspect(f->call, &state);
    const bool observed = inspected && observe(f, directory, &state, status);
    CHECK(inspected && observed && f->call && !state.active && state.world == w &&
          state.math == f->math && state.candidates_returned && !f->callback_bad);
    CHECK(failed_inputs_preserved(f));
    oracle expected_state;
    CHECK(expected(f->index, order, &expected_state));
    const bool returned = expected_state.stop == 994;
    if (status != MC_WORLD_STORAGE_OK && returned)
        fprintf(stderr, "Random profile%u expected994, reached%u: %s\n", f->index, state.source_pc,
                state.error->operation);
    CHECK(state.source_pc == expected_state.stop && state.returned == returned &&
          (status == MC_WORLD_STORAGE_OK) == returned && w->fields.g == expected_state.g &&
          w->fields.Q == expected_state.q &&
          f->owned_random->state == expected_state.random.state && w->fields.h == h);
    CHECK(state.chunk_count == (f->index ? 361u : 0u) &&
          state.chunks_started == expected_state.chunks_started &&
          state.chunks_returned == expected_state.chunks_returned &&
          state.samples_started == expected_state.samples_started &&
          state.samples_returned == expected_state.samples_returned &&
          state.random_calls == expected_state.random_calls &&
          state.lcg_updates == expected_state.updates);
    CHECK(state.ambient_attempts == expected_state.ambient_attempts &&
          state.ambient_sounds == expected_state.ambient_sounds &&
          state.snow_attempts == expected_state.snow_attempts &&
          state.snow_writes == expected_state.snow_writes &&
          state.ice_writes == expected_state.ice_writes &&
          state.weather_publications == expected_state.lightning &&
          state.block_ticks == expected_state.ticks && f->sounds == expected_state.sound_events);
    CHECK(f->cells == expected_state.snow_writes + expected_state.ice_writes && f->ranges == 0 &&
          w->weather_membership.count == expected_state.lightning &&
          w->entity_membership.count == 0 && w->fields.m == (expected_state.lightning ? 2 : 0));
    mc_entity_class_snapshot after_entities;
    mc_math_class_state after_math;
    CHECK(mc_entity_classes_get_snapshot(f->entities, &after_entities) == MC_ENTITY_OK &&
          after_entities.next_id == before_entities.next_id + (int32_t)expected_state.lightning &&
          mc_math_classes_state(f->math, &after_math, NULL) == MC_MATH_CLASS_OK &&
          after_math.holds == before_math.holds + 1);
    uint64_t expected_u = u;
    for (size_t i = 0; i < expected_state.lightning; ++i)
        expected_u *= UINT64_C(1181783497276652981);
    CHECK(atomic_load(&f->profile->uniquifier) == expected_u &&
          atomic_load(&f->profile->holder_phase) == holder &&
          (!f->profile->math || atomic_load(&f->profile->math->state) == math_random) &&
          (!f->player ||
           (f->player->random->state == own.state &&
            f->player->random->have_next_next_gaussian == own.have_next_next_gaussian)) &&
          w->persistent_bindings == bindings + 1 + expected_state.lightning);
    if (returned)
        CHECK(state.samples_returned == (f->index ? 28880u : 0u) && w->local_leases == 0 &&
              w->active_calls == 0 && !state.chunk.captured && !state.chunk.failure);
    else {
        CHECK(state.error && state.error->status == status);
        if (f->index == 5)
            CHECK(!state.chunk.captured && w->local_leases == 0 && w->active_calls == 0 &&
                  !strcmp(state.error->exception_class,
                          "java/util/ConcurrentModificationException") &&
                  w->fields.P->count == 0);
        else
            CHECK(state.chunk.captured && state.chunk.ready && w->local_leases == 1 &&
                  w->active_calls == 1 &&
                  mc_world_runtime_retire_world(w, NULL) == MC_WORLD_STORAGE_BUSY &&
                  mc_world_store_entity_containers_validate(f->store, w, NULL) ==
                      MC_WORLD_STORAGE_BUSY);
        if (f->index == 4)
            CHECK(state.nearest == f->player && state.error->native_policy &&
                  status == MC_WORLD_STORAGE_IO_ERROR && state.source_pc == 443);
        if (f->index == 7 || f->index == 8 || f->index == 13)
            CHECK(!state.error->native_policy && state.error->exception_class &&
                  !strcmp(state.error->exception_class, "java/lang/NullPointerException"));
    }
    CHECK(failed_inputs_preserved(f));
    CHECK(mc_world_runtime_random_tick_retire(&f->call, NULL) == MC_WORLD_STORAGE_OK && !f->call &&
          w->local_leases == 0 && w->active_calls == 0 &&
          w->persistent_bindings == bindings + expected_state.lightning);
    CHECK(failed_inputs_preserved(f));
    if (f->null_chunk) {
        /* Call/capture retirement needs no source repair. Whole-Store terminal
         * admission is a narrower existing contract: its owner validator
         * still requires a nonnull source b. This passive refusal consumes
         * nothing and is not credited as unrepaired whole-graph closure. */
        mc_world_storage_error terminal;
        const size_t chunks = mc_world_store_chunk_count(f->store);
        CHECK(mc_world_store_entity_containers_validate(f->store, w, &terminal) ==
                  MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
              terminal.operation &&
              !strcmp(terminal.operation, "Store entity retirement Chunk ownership") &&
              f->null_chunk->data->blocks == NULL && w->active_calls == 0 && w->local_leases == 0 &&
              mc_world_store_chunk_count(f->store) == chunks &&
              w->persistent_bindings == bindings && w->player_membership.count == 1);
    }
    if (f->index == 13) {
        /* Store admission does not read World.r. This proves no analogous
         * Store refusal for r=NULL; complete World disposal is kept separate
         * and restores the fixture's retained Random storage in teardown. */
        CHECK(w->fields.r == NULL &&
              mc_world_store_entity_containers_validate(f->store, w, NULL) == MC_WORLD_STORAGE_OK &&
              w->fields.r == NULL && w->active_calls == 0 && w->local_leases == 0);
    }
    CHECK(mc_math_classes_state(f->math, &after_math, NULL) == MC_MATH_CLASS_OK &&
          after_math.holds == before_math.holds);
    return true;
}

static bool teardown(random_fixture *f) {
    CHECK(failed_inputs_preserved(f));
    CHECK(mc_world_runtime_random_tick_retire(&f->call, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(failed_inputs_preserved(f));
    /* Restore declared fixture inputs only after call retirement has succeeded
     * against the actual failed prefix. This is terminal fixture ownership
     * cleanup, never a prerequisite or credit for the call's retirement. */
    if (f->null_chunk) {
        f->null_chunk->data->blocks = f->null_chunk_bytes;
        f->null_chunk = NULL;
    }
    if (f->owned_random) {
        f->world->fields.r = f->owned_random;
        f->owned_random = NULL;
    }
    if (f->stone_control) {
        f->registry->blocks.m[1] = f->stone;
        f->registry->blocks.n[1] = f->stone_tick;
        f->stone_control = false;
    }
    if (f->world) {
        CHECK(mc_world_runtime_retire_entity_containers(f->world, NULL) == MC_WORLD_STORAGE_OK);
        if (f->player) {
            mc_entity_release(f->player);
            f->player = NULL;
        }
        if (f->provider.cache) {
            CHECK(mc_chunk_pending_clear(f->provider.pending) == MC_CHUNK_ACQUIRE_OK);
            mc_chunk_cache *cache = f->provider.cache;
            CHECK(mc_chunk_cache_init(cache, cache->entries, cache->capacity, cache->insertions,
                                      cache->insertion_capacity));
        }
        if (f->dimension)
            mc_world_provider_unbind(f->dimension);
        CHECK(mc_world_runtime_retire_world(f->world, NULL) == MC_WORLD_STORAGE_OK);
        for (size_t i = 0; i < f->owned_count; ++i)
            CHECK(mc_world_store_retire_chunk(f->store, f->owned[i], NULL) == MC_WORLD_STORAGE_OK);
        CHECK(mc_world_runtime_free(f->world, NULL) == MC_WORLD_STORAGE_OK);
        f->world = NULL;
    }
    if (f->access)
        CHECK(mc_world_access_free(f->access, NULL) == MC_WORLD_STORAGE_OK);
    if (f->sessions)
        CHECK(mc_player_session_classes_free(&f->sessions, NULL) == MC_PLAYER_OK);
    if (f->dimension)
        mc_world_provider_release(f->dimension);
    if (f->store)
        CHECK(mc_world_store_free_checked(&f->store, NULL) == MC_WORLD_STORAGE_OK);
    mc_world_regions_free(f->regions);
    if (f->name)
        mc_world_info_name_release(f->name);
    if (f->classes)
        CHECK(mc_world_runtime_classes_free(f->classes, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_math_classes_release(&f->math, NULL) == MC_MATH_CLASS_OK);
    if (f->registry)
        CHECK(mc_registry_classes_free(f->registry, NULL) == MC_REGISTRY_OK);
    if (f->entities)
        CHECK(mc_entity_classes_free(f->entities) == MC_ENTITY_OK);
    mc_java_random_profile_free(f->profile);
    mc_materials_destroy(f->materials);
    if (f->files)
        CHECK(mc_resource_files_free(f->files) == MC_REGISTRY_OK);
    return true;
}

int main(int argc, char **argv) {
    if (argc != 4)
        return 2;
    char path[8192];
    int n = snprintf(path, sizeof(path), "%s/native.txt", argv[3]);
    if (n < 0 || (size_t)n >= sizeof(path))
        return 2;
    FILE *events = open_file(path, "wb");
    if (!events)
        return 2;
    for (unsigned int index = 0; index < COUNT(scenarios); ++index) {
        random_fixture *f = calloc(1, sizeof(*f));
        if (!f)
            return 2;
        f->events = events;
        f->index = index;
        n = snprintf(path, sizeof(path), "%s/case%u", argv[2], index);
        const bool passed =
            n > 0 && (size_t)n < sizeof(path) && setup(f, argv[1], path) && run(f, argv[3]);
        if (!teardown(f)) {
            fputs("Random tick cleanup refused; live dependencies retained\n", stderr);
            return 1;
        }
        free(f);
        if (!passed)
            return 1;
    }
    if (fclose(events))
        return 1;
    printf("World random tick: %u checks;14 paired profiles and native h-ignored control\n",
           checks);
    return 0;
}
