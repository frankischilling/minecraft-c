/* Reuse genuine constructor, Registry, Store and retirement fixture owners. */
#define main mc_world_tick_weather_fixture_main
#include "test_entity_publication.c"
#undef main
#include "../../../src/core/java/random_profile_internal.h"
#include "../../../src/world/info/tick_source_internal.h"
#include "../../../src/world/info/weather_source_internal.h"
#include "../../../src/world/runtime/entity_publication_internal.h"
#include "mc/entity/mob/construction.h"
#include "mc/entity/mob/spawning.h"
#include "mc/entity/player/player.h"
#include "mc/world/runtime/random_tick.h"
#include "mc/world/runtime/world_tick.h"
#include <inttypes.h>

static mc_math_classes *tick_math;

static bool spawn_chunk(fixture *f, size_t slot, int32_t cx, int32_t cz, bool flat,
                        uint8_t block_light) {
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
    r->fields[0].value.int_value = cx;
    r->fields[1].value.int_value = cz;
    r->fields[8].value.byte_value = 1;
    for (size_t i = 0; i < COUNT(lengths); ++i) {
        r->fields[i + 3].value.byte_array.data = r->arrays[i];
        r->fields[i + 3].value.byte_array.length = lengths[i];
    }
    if (flat) {
        for (size_t x = 0; x < 16; ++x)
            for (size_t z = 0; z < 16; ++z) {
                r->arrays[0][x * 2048 + z * 128 + 63] = 2;
                r->arrays[4][z * 16 + x] = 64;
            }
        memset(r->arrays[3], block_light, MC_CHUNK_NIBBLE_BYTES);
    } else {
        /* A genuine loaded solid Chunk rejects the initial source support
         * gate at every height. Only (-3,-3) enters the trial loop. */
        memset(r->arrays[0], 1, MC_CHUNK_BLOCKS);
        memset(r->arrays[4], 128, MC_CHUNK_HEIGHTMAP_BYTES);
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
    CHECK(mc_world_regions_write(f->regions, mc_world_store_root(f->stores[slot]), cx, cz, encoded,
                                 length, fixture_timestamp, &growth, NULL) == MC_WORLD_STORAGE_OK);
    ok = true;
cleanup:
    free(encoded);
    free(r);
    return ok;
}

static bool spawn_chunks(fixture *f, size_t slot, uint8_t light) {
    for (int32_t x = -10; x <= 8; ++x)
        for (int32_t z = -7; z <= 11; ++z)
            if (!spawn_chunk(f, slot, x, z, x == -3 && z == -3, light))
                return false;
    return true;
}

static bool spawn_seed(mc_world_runtime *world, int32_t category, const char *target, bool jockey,
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
        if (x >= (jockey ? 1 : 0) && x < (jockey ? 15 : 16) && z >= (jockey ? 1 : 0) &&
            z < (jockey ? 15 : 16)) {
            if (jockey) {
                (void)mc_java_random_next_float(&random);
                if (!mc_java_random_next_bounded(&random, 100, &a) || a != 0)
                    continue;
            }
            *seed = candidate;
            return true;
        }
    }
    return false;
}

static bool tick_world_create(fixture *f, const char *root, size_t slot, size_t max_chunks) {
    bool ok = false;
    char path[8192];
    uint8_t map[256];
    CHECK(snprintf(path, sizeof path, "%s/world%zu", root, slot) > 0);
    for (size_t i = 0; i < COUNT(map); ++i)
        map[i] = (uint8_t)i;
    const mc_world_store_options options = {.session_token = 137,
                                            .block_map = map,
                                            .block_map_length = sizeof map,
                                            .limits = mc_world_store_default_limits()};
    CHECK(mc_world_store_open(path, f->regions, &options, &f->stores[slot], NULL) ==
          MC_WORLD_STORAGE_OK);
    const uint16_t title[] = {'W', 'e', 'a', 't', 'h', 'e', 'r'};
    const mc_level_info info = {.seed = 23,
                                .spawn_y = 64,
                                .level_name = {title, COUNT(title)},
                                .version = 19132,
                                .rain_time = 20,
                                .thunder_time = 30};
    CHECK(mc_world_store_save_info(f->stores[slot], &info, 123, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_provider_create(0, NULL, &f->dimensions[slot], NULL) == MC_WORLD_PROVIDER_OK);
    mc_world_runtime_construction_limits limits = mc_world_runtime_default_construction_limits();
    limits.max_provider_chunks = max_chunks;
    limits.max_scheduled_entries = 0;
    CHECK(mc_world_runtime_allocate_world(MC_WORLD_RUNTIME_CLIENT_LOAD_OR_CREATE, f->classes[0],
                                          &limits, &f->worlds[slot], NULL) == MC_WORLD_STORAGE_OK);
    const mc_world_runtime_world_arguments args = {
        .store = f->stores[slot], .name = f->name, .seed = -99, .dimension = f->dimensions[slot]};
    CHECK(mc_world_runtime_construct_world(f->worlds[slot], &args, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_chunk_provider_bind_fields(f->worlds[slot]->fields.v, &f->providers[slot], NULL) ==
          MC_WORLD_STORAGE_OK);
    f->bound[slot] = true;
    ok = true;
cleanup:
    return ok;
}

typedef struct spawn_oracle {
    mc_java_random random, first_random;
    size_t calls, constructors, rejections, publications, first_calls;
    int32_t group, total, last_x, last_y, last_z;
    float last_yaw;

    struct {
        int32_t x, z, color;
        float yaw;
    } published[4];
} spawn_oracle;

static bool oracle_draw(spawn_oracle *o, int32_t bound, int32_t *out) {
    if (!mc_java_random_next_bounded(&o->random, bound, out))
        return false;
    ++o->calls;
    return true;
}

static int32_t oracle_hash(int32_t x, int32_t z) {
    const uint32_t bits = (x < 0 ? UINT32_C(0x80000000) : 0) | (((uint32_t)x & 32767u) << 16) |
                          (z < 0 ? 32768u : 0) | ((uint32_t)z & 32767u);
    int32_t result;
    memcpy(&result, &bits, sizeof result);
    return result;
}

static bool oracle_weight(mc_world_runtime *world, int32_t category, int32_t x, int32_t z,
                          int32_t *out) {
    mc_world_provider_fields dimension;
    const mc_biome *biome = NULL;
    if (mc_world_provider_fields_get(world->fields.t, &dimension) != MC_WORLD_PROVIDER_OK ||
        mc_world_chunk_manager_chunk(dimension.b, x, z, &biome, NULL) != MC_WORLD_PROVIDER_OK)
        return false;
    const mc_biome_spawn_list *list = mc_biome_spawn_list_get(biome, category);
    *out = 0;
    for (size_t i = 0; i < mc_biome_spawn_list_size(list); ++i)
        *out += mc_biome_spawn_fields_get(mc_biome_spawn_list_at(list, i))->b;
    return true;
}

/* Independent call-sequence/geometry oracle over authored fixture inputs.
 * Core Random and HashSet have their own unchanged-original tests; this model
 * exercises CQ's use/order, rather than calling CQ or the predicate itself. */
static bool oracle_spawn(mc_world_runtime *world, int32_t seed, int32_t category, const char *name,
                         bool permit, bool jockey, spawn_oracle *o) {
    memset(o, 0, sizeof(*o));
    mc_java_random_seed(&o->random, seed);
    int32_t weight, ignored, x0, y0, z0;
    if (!oracle_weight(world, category, -3, -3, &weight) || !oracle_draw(o, weight, &ignored) ||
        !oracle_draw(o, 16, &x0) || !oracle_draw(o, 128, &y0) || !oracle_draw(o, 16, &z0))
        return false;
    x0 -= 48;
    z0 -= 48;
    for (int group = 0; group < 3 && o->group < 4; ++group) {
        int32_t x = x0, y = y0, z = z0;
        for (int attempt = 0; attempt < 4 && o->group < 4; ++attempt) {
            int32_t a, b;
            if (!oracle_draw(o, 6, &a) || !oracle_draw(o, 6, &b))
                return false;
            x += a - b;
            if (!oracle_draw(o, 1, &a) || !oracle_draw(o, 1, &b))
                return false;
            y += a - b;
            if (!oracle_draw(o, 6, &a) || !oracle_draw(o, 6, &b))
                return false;
            z += a - b;
            /* Every other Chunk is actual stone, so its current cell is
             * solid. In the flat Chunk only y64 has support and air above. */
            if (x < -48 || x >= -32 || z < -48 || z >= -32 || y != 64)
                continue;
            if (o->constructors == 0) {
                o->first_random = o->random;
                o->first_calls = o->calls;
            }
            ++o->constructors;
            o->last_x = x;
            o->last_y = y;
            o->last_z = z;
            o->last_yaw = mc_java_random_next_float(&o->random) * 360.0f;
            ++o->calls;
            bool eligible = permit;
            for (size_t i = 0; i < o->publications && i < COUNT(o->published); ++i)
                if (o->published[i].x == x && o->published[i].z == z)
                    eligible = false;
            if (!eligible)
                ++o->rejections;
            else {
                const size_t index = o->publications++;
                ++o->group;
                if (index < COUNT(o->published)) {
                    o->published[index].x = x;
                    o->published[index].z = z;
                    o->published[index].yaw = o->last_yaw;
                }
                if (jockey) {
                    if (!oracle_draw(o, 100, &ignored) || ignored != 0)
                        return false;
                    ++o->publications; /* Genuine extra Skeleton, before mount. */
                    return true;
                }
                if (strcmp(name, "dl") == 0) {
                    if (!oracle_draw(o, 100, &ignored))
                        return false;
                    int32_t color = ignored < 5    ? 15
                                    : ignored < 10 ? 7
                                    : ignored < 15 ? 8
                                    : ignored < 18 ? 12
                                                   : -1;
                    if (color == -1) {
                        if (!oracle_draw(o, 500, &ignored))
                            return false;
                        color = ignored == 0 ? 6 : 0;
                    }
                    o->published[index].color = color;
                }
            }
            if (o->group < 4)
                o->total += o->group; /* CQ858 skips this on the fourth success. */
        }
    }
    mc_java_int_set *order = NULL;
    if (mc_java_int_set_create(NULL, &order) != MC_JAVA_INT_SET_OK)
        return false;
    bool ok = false, added;
    int32_t keys[289];
    size_t count;
    for (int32_t dx = -8; dx <= 8; ++dx)
        for (int32_t dz = -8; dz <= 8; ++dz)
            if (mc_java_int_set_add(order, oracle_hash(-1 + dx, 2 + dz), &added) !=
                    MC_JAVA_INT_SET_OK ||
                !added)
                goto done;
    if (mc_java_int_set_copy_order(order, keys, COUNT(keys), &count) != MC_JAVA_INT_SET_OK ||
        count != COUNT(keys) || keys[0] != oracle_hash(-3, -3))
        goto done;
    for (int32_t kind = category; kind <= (category == 1 ? 2 : 0); ++kind)
        for (size_t i = kind == category ? 1 : 0; i < COUNT(keys); ++i) {
            const uint32_t bits = (uint32_t)keys[i];
            const int32_t cx =
                (int32_t)((bits >> 16) & 32767u) - ((bits & UINT32_C(0x80000000)) != 0 ? 32768 : 0);
            const int32_t cz = (int32_t)(bits & 32767u) - ((bits & 32768u) != 0 ? 32768 : 0);
            if (!oracle_weight(world, kind, cx, cz, &weight))
                goto done;
            if (weight == 0)
                continue;
            if (!oracle_draw(o, weight, &ignored) || !oracle_draw(o, 16, &ignored) ||
                !oracle_draw(o, 128, &ignored) || !oracle_draw(o, 16, &ignored))
                goto done;
            /* The solid initial-cell gate stops all other Chunks. The water
             * category's one flat air Chunk fails actual material identity. */
        }
    ok = true;
done:
    mc_java_int_set_free(order);
    return ok;
}

typedef struct random_tick_oracle {
    mc_java_random random;
    uint32_t g;
    int32_t cooldown;
    size_t calls, block_calls, lcg_updates, snow_attempts, ambient_attempts, block_ticks;
} random_tick_oracle;

static uint32_t oracle_lcg(random_tick_oracle *o) {
    o->g = o->g * UINT32_C(3) + UINT32_C(1013904223);
    ++o->lcg_updates;
    return o->g;
}

static bool random_tick_draw(random_tick_oracle *o, int32_t bound, int32_t *value) {
    if (!mc_java_random_next_bounded(&o->random, bound, value))
        return false;
    ++o->calls;
    return true;
}

/* Separate FD.n model over the authored 19x19 grid. Each nonflat Chunk is
 * stone at every height; the flat Chunk contains only Grass at63 and air.
 * Neither contains Dirt/water, so Grass draws cannot change Blocks and snow
 * or ice cannot be written. Weather scalars remain zero in these profiles.
 * This model never invokes FD.n, a Block tick, or a runtime light query. */
static bool oracle_random_tick(mc_world_runtime *world, const mc_java_random *after_spawn,
                               uint8_t block_light, random_tick_oracle *o) {
    memset(o, 0, sizeof(*o));
    if (world->fields.i != 0.0f || world->fields.j != 0.0f || world->fields.k != 0.0f ||
        world->fields.l != 0.0f)
        return false;
    o->random = *after_spawn;
    memcpy(&o->g, &world->fields.g, sizeof(o->g));
    o->cooldown = world->fields.Q > 0 ? world->fields.Q - 1 : world->fields.Q;
    mc_java_int_set *order = NULL;
    if (mc_java_int_set_create(NULL, &order) != MC_JAVA_INT_SET_OK)
        return false;
    bool ok = false, added;
    int32_t keys[361];
    size_t count = 0;
    for (int32_t x = -10; x <= 8; ++x)
        for (int32_t z = -7; z <= 11; ++z)
            if (mc_java_int_set_add(order, oracle_hash(x, z), &added) != MC_JAVA_INT_SET_OK ||
                !added)
                goto done;
    if (mc_java_int_set_copy_order(order, keys, COUNT(keys), &count) != MC_JAVA_INT_SET_OK ||
        count != COUNT(keys))
        goto done;
    for (size_t chunk = 0; chunk < COUNT(keys); ++chunk) {
        const bool flat = keys[chunk] == oracle_hash(-3, -3);
        int32_t value;
        if (o->cooldown == 0) {
            ++o->ambient_attempts;
            const uint32_t g = oracle_lcg(o);
            const uint32_t y = (g >> 18) & 127u;
            /* A flat air cell reaches nextInt8. Light15 rejects it; dark
             * air has no Player within8 of this far-away flat Chunk. */
            if (flat && y != 63 && !random_tick_draw(o, 8, &value))
                goto done;
        }
        if (!random_tick_draw(o, 100000, &value) || !random_tick_draw(o, 16, &value))
            goto done;
        if (value == 0) {
            ++o->snow_attempts;
            (void)oracle_lcg(o);
        }
        for (size_t sample = 0; sample < 80; ++sample) {
            const uint32_t g = oracle_lcg(o);
            if (flat && ((g >> 18) & 127u) == 63) {
                ++o->block_ticks;
                if (block_light >= 9) {
                    static const int32_t bounds[] = {3, 5, 3};
                    for (size_t i = 0; i < COUNT(bounds); ++i) {
                        if (!mc_java_random_next_bounded(&o->random, bounds[i], &value))
                            goto done;
                        ++o->block_calls;
                    }
                }
            }
        }
    }
    ok = true;
done:
    mc_java_int_set_free(order);
    return ok;
}

static bool random_tick_matches(const mc_world_runtime *world, const mc_world_tick_state *state,
                                const random_tick_oracle *o) {
    const mc_world_random_tick_state *random = state->random;
    uint32_t g;
    memcpy(&g, &world->fields.g, sizeof(g));
    return state->returned && state->source_pc == 198 && state->random_returned && random != NULL &&
           !random->active && random->returned && random->source_pc == 994 &&
           random->world == world && random->math == tick_math && random->candidates_returned &&
           random->chunk_count == 361 && random->chunks_started == 361 &&
           random->chunks_returned == 361 && random->samples_started == 361u * 80u &&
           random->samples_returned == 361u * 80u && random->block_ticks == o->block_ticks &&
           random->random_calls == o->calls && random->lcg_updates == o->lcg_updates &&
           random->ambient_attempts == o->ambient_attempts && random->ambient_sounds == 0 &&
           random->snow_attempts == o->snow_attempts && random->lightning_attempts == 0 &&
           random->weather_publications == 0 && random->snow_writes == 0 &&
           random->ice_writes == 0 && random->chunk.captured == NULL && !random->chunk.ready &&
           !random->chunk.world_held && !random->chunk.chunk_held && random->error != NULL &&
           random->error->status == MC_WORLD_STORAGE_OK &&
           state->error->status == MC_WORLD_STORAGE_OK && world->fields.P->count == 361 &&
           g == o->g && world->fields.Q == o->cooldown &&
           mc_test_java_random_equal(world->fields.r, &o->random);
}

static bool math_holds(const mc_math_class_state *before, size_t extra) {
    mc_math_class_state after = {0};
    return mc_math_classes_state(tick_math, &after, NULL) == MC_MATH_CLASS_OK &&
           after.holds == before->holds + extra && after.phase == MC_MATH_CLASS_READY &&
           after.source_a == before->source_a && after.last_store_pc == before->last_store_pc;
}

static void random_tick_row(const char *kind, const char *name, const random_tick_oracle *o) {
    printf("RANDOM_TICK %s %s chunks=361 samples=28880 lcg=%zu snow=%zu ambient=%zu "
           "world-draws=%zu block-draws=%zu block-ticks=%zu source=198:994 nested-holds=2\n",
           kind, name, o->lcg_updates, o->snow_attempts, o->ambient_attempts, o->calls,
           o->block_calls, o->block_ticks);
}

static bool retire_spawn_chunks(fixture *f, size_t slot) {
    if (!f->bound[slot] || f->world_retired[slot])
        return true;
    mc_chunk_cache *cache = f->providers[slot].cache;
    mc_chunk_state **chunks = malloc((cache->count + 1) * sizeof(*chunks));
    if (chunks == NULL)
        return false;
    const size_t count = cache->count;
    for (size_t i = 0; i < count; ++i)
        chunks[i] = cache->entries[i].chunk;
    const size_t random_coordinates = f->worlds[slot]->fields.P->count;
    bool ok =
        mc_world_runtime_retire_entity_containers(f->worlds[slot], NULL) == MC_WORLD_STORAGE_OK &&
        f->worlds[slot]->fields.P->count == random_coordinates;
    if (ok) {
        f->terminal[slot] = true;
        ok = retire_descriptors(f, slot) &&
             mc_world_runtime_retire_world(f->worlds[slot], NULL) == MC_WORLD_STORAGE_OK;
    }
    if (ok) {
        f->world_retired[slot] = true;
        for (size_t i = 0; i < count; ++i)
            if (mc_world_store_retire_chunk(f->stores[slot], chunks[i], NULL) !=
                MC_WORLD_STORAGE_OK)
                ok = false;
        f->states[slot] = NULL;
    }
    free(chunks);
    return ok;
}

static bool spawn_matches(mc_world_runtime *w, const mc_world_tick_state *s, const spawn_oracle *o,
                          const random_tick_oracle *random, const char *name, bool jockey) {
    mc_entity_class_literal_view selected;
    if (!s->first_constructor.captured || !s->first_constructor.random_present ||
        mc_entity_class_literal_inspect(s->first_constructor.class_literal, &selected) !=
            MC_ENTITY_OK ||
        strcmp(selected.client_name, name) != 0 || s->first_constructor.chunk_count != 289 ||
        s->first_constructor.chunk_x != -3 || s->first_constructor.chunk_z != -3 ||
        s->first_constructor.candidates_visited != 1 ||
        s->first_constructor.random_calls != o->first_calls ||
        !mc_test_java_random_equal(&s->first_constructor.random, &o->first_random) ||
        s->spawn_constructors != o->constructors || s->spawn_predicates != o->constructors ||
        s->spawn_rejections != o->rejections || s->spawn_publications != o->publications ||
        s->spawn_group_successes != o->group || s->spawn_total != o->total ||
        s->spawn_random_calls != o->calls ||
        (jockey && !mc_test_java_random_equal(w->fields.r, &o->random)) ||
        s->spawn_entity == NULL || w->local_leases != 0)
        return false;
    mc_entity_snapshot entity;
    if (mc_entity_get_snapshot(s->spawn_entity, &entity) != MC_ENTITY_OK ||
        entity.phase != MC_ENTITY_READY || entity.base.world != w ||
        entity.base.x != (double)((float)o->last_x + 0.5f) ||
        entity.base.y != (double)o->last_y + (double)entity.base.offset ||
        entity.base.z != (double)((float)o->last_z + 0.5f) || entity.base.yaw != o->last_yaw ||
        entity.base.aT != 0 || entity.base.aU != 0 || entity.base.aV != 0 ||
        entity.base.x != entity.base.aJ || entity.base.y != entity.base.aK ||
        entity.base.z != entity.base.aL ||
        entity.bounds->min_y != entity.base.y - (double)entity.base.offset)
        return false;
    if (jockey) {
        mc_entity_snapshot skeleton;
        return s->source_pc == 80 && !s->spawning_returned && !s->random_returned &&
               s->random == NULL && s->error->child_bytecode == 845 &&
               s->error->descendant_bytecode == 56 && s->spawn_postspawn_pc == 56 &&
               !s->spawn_postspawn_returned && s->spawn_jockey != NULL &&
               w->entity_membership.count == 2 &&
               w->entity_membership.entries[0] == s->spawn_entity &&
               w->entity_membership.entries[1] == s->spawn_jockey &&
               mc_entity_get_snapshot(s->spawn_jockey, &skeleton) == MC_ENTITY_OK &&
               mc_entity_class_literal_inspect(skeleton.class_literal, &selected) == MC_ENTITY_OK &&
               selected.kind == MC_ENTITY_CLASS_KIND_SKELETON && skeleton.base.x == entity.base.x &&
               skeleton.base.y == entity.base.y && skeleton.base.z == entity.base.z &&
               skeleton.base.yaw == entity.base.yaw && entity.base.aG == NULL &&
               entity.base.aH == NULL && skeleton.base.aG == NULL && skeleton.base.aH == NULL;
    }
    return random != NULL && random_tick_matches(w, s, random) && s->spawning_returned &&
           s->provider_returned && s->time_stored && s->scheduled_returned &&
           s->random_chunk_count == 361 &&
           s->spawn_candidates_visited == (s->first_constructor.category == 1 ? 578u : 289u);
}

static bool published_matches(mc_world_runtime *w, const spawn_oracle *o, const char *name) {
    if (w->entity_membership.count != o->publications)
        return false;
    for (size_t i = 0; i < o->publications; ++i) {
        mc_entity_snapshot fields;
        mc_entity_class_literal_view kind;
        mc_mob_state mob;
        mc_entity *entity = w->entity_membership.entries[i];
        if (mc_entity_get_snapshot(entity, &fields) != MC_ENTITY_OK ||
            mc_entity_class_literal_inspect(fields.class_literal, &kind) != MC_ENTITY_OK ||
            strcmp(kind.client_name, name) != 0 || fields.base.world != w ||
            fields.base.x != (double)o->published[i].x + 0.5 ||
            fields.base.z != (double)o->published[i].z + 0.5 ||
            fields.base.yaw != o->published[i].yaw || !fields.base.bF || fields.base.bG != -3 ||
            fields.base.bH != 4 || fields.base.bI != -3)
            return false;
        if (strcmp(name, "dl") == 0 && (!mc_mob_inspect(entity, &mob) || !mob.watcher_16_present ||
                                        (mob.watcher_16 & 15) != o->published[i].color))
            return false;
    }
    return true;
}

static bool spawn_add_player(fixture *f, size_t slot, mc_entity **out) {
    mc_entity *player = NULL;
    if (mc_local_player_allocate(f->entity_classes[0], &player, NULL) != MC_ENTITY_OK)
        return false;
    player->base.x = -0.25;
    player->base.z = 32;
    mc_world_runtime *w = f->worlds[slot];
    mc_world_entity_error error = {0};
    if (mc_world_runtime_call_enter(w, NULL) != MC_WORLD_STORAGE_OK) {
        mc_entity_release(player);
        return false;
    }
    const bool appended = mc_world_runtime_entity_append_owned(
        w, w->fields.d, &w->player_membership, player, 0, &error);
    mc_world_runtime_call_leave(w);
    if (!appended) {
        mc_entity_release(player);
        return false;
    }
    *out = player;
    return true;
}

static bool spawn_positive_case(fixture *f, const char *root, size_t slot, const char *name,
                                int32_t category, bool jockey) {
    bool ok = false;
    mc_entity *player = NULL, *external = NULL;
    mc_world_tick_call *call = NULL;
    mc_world_tick_state state = {0};
    spawn_oracle oracle = {0};
    random_tick_oracle random = {0};
    mc_math_class_state math_before = {0};
    int32_t seed;
    CHECK(tick_world_create(f, root, slot, jockey ? 1 : 512) &&
          (jockey ? spawn_chunk(f, slot, -3, -3, true, 0)
                  : spawn_chunks(f, slot, category == 1 ? 255 : 0)) &&
          spawn_add_player(f, slot, &player));
    mc_world_runtime *w = f->worlds[slot];
    w->fields.N = category == 0;
    w->fields.O = category == 1;
    CHECK(spawn_seed(w, category, name, jockey, &seed) &&
          oracle_spawn(w, seed, category, name, true, jockey, &oracle));
    CHECK((jockey || oracle_random_tick(w, &oracle.random, category == 1 ? 15 : 0, &random)) &&
          mc_math_classes_state(tick_math, &math_before, NULL) == MC_MATH_CLASS_OK);
    const size_t baseline_bindings = w->persistent_bindings;
    mc_java_random_reseed(w->fields.r, seed);
    CHECK(mc_world_runtime_tick_begin(w, tick_math, &call, NULL) ==
              (jockey ? MC_WORLD_STORAGE_UNSUPPORTED_STATE : MC_WORLD_STORAGE_OK) &&
          mc_world_runtime_tick_inspect(call, &state) &&
          spawn_matches(w, &state, &oracle, &random, name, jockey) &&
          math_holds(&math_before, jockey ? 1u : 2u) &&
          w->persistent_bindings == baseline_bindings + oracle.publications + (jockey ? 1u : 2u));
    if (jockey) {
        CHECK(oracle.constructors == 1 && oracle.publications == 2 && oracle.group == 1 &&
              oracle.total == 0 && oracle.calls == 12);
        CHECK(mc_entity_retain(state.spawn_jockey) == MC_ENTITY_OK);
        external = state.spawn_jockey;
        CHECK(mc_world_runtime_tick_retire(&call, NULL) == MC_WORLD_STORAGE_OK &&
              w->entity_membership.entries[1] == external);
        printf("MOB_LOOP jockey cn next=cq845:56 published=2 skeleton=fr random-checked=1\n");
    } else {
        CHECK(oracle.publications == 4 && oracle.group == 4 && oracle.total >= 6 &&
              published_matches(w, &oracle, name));
        printf("MOB_LOOP publish %s completed=1 group=4 membership=4 total=%d random-checked=1\n",
               name, oracle.total);
        random_tick_row("publish", name, &random);
    }
    CHECK(mc_world_runtime_tick_retire(&call, NULL) == MC_WORLD_STORAGE_OK &&
          math_holds(&math_before, 0) &&
          w->persistent_bindings == baseline_bindings + oracle.publications);
    ok = true;
cleanup:
    (void)mc_world_runtime_tick_retire(&call, NULL);
    mc_entity_release(external);
    mc_entity_release(player);
    if (f->worlds[slot] != NULL && !retire_spawn_chunks(f, slot))
        ok = false;
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
            mc_world_runtime_tick_begin(world, tick_math, &nested, NULL) == MC_WORLD_STORAGE_OK &&
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
    mc_math_class_state math_before = {0};
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
    CHECK(mc_math_classes_state(tick_math, &math_before, NULL) == MC_MATH_CLASS_OK);
    CHECK(mc_world_runtime_tick_begin(w, tick_math, &call, &admission) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_tick_inspect(call, &state) && state.returned && state.source_pc == 198 &&
          state.weather_returned && state.provider_returned && state.time_stored &&
          state.scheduled_returned && state.random_returned && state.random_chunk_count == 0 &&
          state.random != NULL && state.random->returned && state.random->source_pc == 994 &&
          state.random->samples_returned == 0 && w->fields.Q == 6 &&
          w->persistent_bindings == baseline_bindings + 2 && math_holds(&math_before, 2) &&
          w->active_calls == baseline_calls);
    CHECK(tick_row(w, "basic"));
    CHECK(mc_world_runtime_free(w, NULL) == MC_WORLD_STORAGE_BUSY);
    CHECK(mc_world_runtime_tick_retire(&call, NULL) == MC_WORLD_STORAGE_OK &&
          math_holds(&math_before, 0) && w->persistent_bindings == baseline_bindings);
    int64_t now;
    CHECK(mc_world_info_time_get(w->fields.x, &now) == MC_WORLD_INFO_OK && now == 1);
    w->fields.O = true;
    CHECK(mc_world_runtime_tick_begin(w, tick_math, &call, NULL) == MC_WORLD_STORAGE_OK &&
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
    CHECK(mc_world_runtime_tick_begin(w, tick_math, &call, NULL) == MC_WORLD_STORAGE_OK &&
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
    CHECK(mc_world_runtime_tick_begin(w, tick_math, &earlier, NULL) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          mc_world_runtime_tick_inspect(earlier, &prior) && prior.source_pc == 141 &&
          prior.sky_stored && !prior.time_stored &&
          prior.error->child == MC_WORLD_TICK_CHILD_ACCESS && prior.random == NULL &&
          math_holds(&math_before, 1) && w->persistent_bindings == baseline_bindings + 1);
    CHECK(mc_world_runtime_remove_world_access(w, missing, &removed, NULL) == MC_WORLD_STORAGE_OK &&
          removed && mc_world_access_free(missing, NULL) == MC_WORLD_STORAGE_OK);
    missing = NULL;
    CHECK(mc_world_info_source_time_set(w->fields.x, 1) == MC_WORLD_INFO_OK);
    w->fields.p = 0;
    CHECK(mc_world_info_time_get(w->fields.x, &now) == MC_WORLD_INFO_OK);
    CHECK(mc_world_runtime_tick_begin(w, tick_math, &call, NULL) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          mc_world_runtime_tick_inspect(call, &state) && state.source_pc == 168 &&
          state.error->exception_class != NULL && !state.time_stored &&
          strcmp(state.error->exception_class, "java/lang/ArithmeticException") == 0 &&
          mc_world_runtime_tick_inspect(earlier, &prior) && prior.source_pc == 141 &&
          state.random == NULL && math_holds(&math_before, 2) &&
          w->persistent_bindings == baseline_bindings + 2);
    int64_t unchanged;
    CHECK(mc_world_info_time_get(w->fields.x, &unchanged) == MC_WORLD_INFO_OK && unchanged == now);
    printf("TICK zero-period %lld %lld\n", (long long)now, (long long)unchanged);
    CHECK(mc_world_runtime_tick_retire(&call, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_tick_retire(&earlier, NULL) == MC_WORLD_STORAGE_OK &&
          math_holds(&math_before, 0) && w->persistent_bindings == baseline_bindings);
    w->fields.p = 40;
    w->fields.J = true; /* Empty actual d: A returns true vacuously. */
    w->fields.N = w->fields.O = false;
    w->fields.Q = 7;
    CHECK(mc_world_info_source_rain_time_set(w->fields.x, 20) == MC_WORLD_INFO_OK &&
          mc_world_info_source_thunder_time_set(w->fields.x, 30) == MC_WORLD_INFO_OK);
    CHECK(mc_world_info_source_time_set(w->fields.x, -24001) == MC_WORLD_INFO_OK);
    CHECK(mc_world_runtime_tick_begin(w, tick_math, &call, NULL) == MC_WORLD_STORAGE_OK &&
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
    spawn_oracle oracle = {0};
    random_tick_oracle random = {0};
    CHECK(spawn_chunks(f, 0, 255) && spawn_seed(w, 1, "wh", false, &selected_seed) &&
          oracle_spawn(w, selected_seed, 1, "wh", true, false, &oracle) &&
          oracle_random_tick(w, &oracle.random, 15, &random));
    CHECK(mc_ws_path_join(mc_world_store_root(f->stores[0]), "region/r.-1.-1.mcr", &flat_path,
                          NULL) == MC_WORLD_STORAGE_OK &&
          mc_ws_read_file(flat_path, 1024u * 1024u, &flat_before, NULL) == MC_WORLD_STORAGE_OK);
    mc_java_random_reseed(w->fields.r, selected_seed);
    CHECK(mc_world_info_source_rain_time_set(w->fields.x, 20) == MC_WORLD_INFO_OK &&
          mc_world_info_source_thunder_time_set(w->fields.x, 30) == MC_WORLD_INFO_OK);
    CHECK(mc_world_runtime_tick_begin(w, tick_math, &call, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_tick_inspect(call, &state) &&
          spawn_matches(w, &state, &oracle, &random, "wh", false) &&
          published_matches(w, &oracle, "wh") && oracle.publications == 4 && oracle.group == 4 &&
          math_holds(&math_before, 2) && w->persistent_bindings == baseline_bindings + 6);
    const mc_world_spawn_constructor_trace *trace = &state.first_constructor;
    mc_entity_class_literal_view selected_class = {0};
    CHECK(mc_entity_class_literal_inspect(trace->class_literal, &selected_class) == MC_ENTITY_OK &&
          selected_class.kind == MC_ENTITY_CLASS_KIND_PIG && trace->category == 1 &&
          trace->random_calls == 10 && f->providers[0].cache->count == 361 &&
          f->providers[0].cache->insertion_count == 361 &&
          mc_ws_read_file(flat_path, 1024u * 1024u, &flat_after, NULL) == MC_WORLD_STORAGE_OK &&
          flat_before.length == flat_after.length &&
          memcmp(flat_before.data, flat_after.data, flat_before.length) == 0 &&
          w->player_membership.count == 1 && w->local_leases == 0);
    printf("SPAWN prefix %zu %d %d\n", trace->chunk_count, trace->chunk_x, trace->chunk_z);
    printf("SPAWN constructor %d %d %d %s %d %d %d %d %d %d %zu %012" PRIx64 "\n", selected_seed,
           trace->total_weight, trace->ticket, selected_class.client_name, trace->initial_x,
           trace->initial_y, trace->initial_z, trace->trial_x, trace->trial_y, trace->trial_z,
           trace->random_calls, trace->random.state);
    printf("MOB_LOOP publish wh completed=1 group=4 membership=4 total=%d random-checked=1\n",
           oracle.total);
    random_tick_row("publish", "wh", &random);
    CHECK(mc_world_runtime_tick_retire(&call, NULL) == MC_WORLD_STORAGE_OK &&
          w->active_calls == baseline_calls && w->persistent_bindings == baseline_bindings + 4 &&
          math_holds(&math_before, 0));
    /* BlockLight15 rejects every GZ predicate after its two own draws; y64
     * rejects Slime. Actual CQ continues every remaining trial and candidate. */
    static const char *const hostile_names[] = {"cn", "uz", "fr", "gb", "uw"};
    w->fields.N = true;
    w->fields.O = false;
    for (size_t i = 0; i < COUNT(hostile_names); ++i) {
        CHECK(spawn_seed(w, 0, hostile_names[i], false, &selected_seed) &&
              oracle_spawn(w, selected_seed, 0, hostile_names[i], false, false, &oracle) &&
              oracle_random_tick(w, &oracle.random, 15, &random));
        mc_java_random_reseed(w->fields.r, selected_seed);
        CHECK(mc_world_info_source_rain_time_set(w->fields.x, 20) == MC_WORLD_INFO_OK &&
              mc_world_info_source_thunder_time_set(w->fields.x, 30) == MC_WORLD_INFO_OK &&
              mc_world_info_source_time_set(w->fields.x, 1) == MC_WORLD_INFO_OK);
        const size_t before_bindings = w->persistent_bindings;
        CHECK(mc_world_runtime_tick_begin(w, tick_math, &call, NULL) == MC_WORLD_STORAGE_OK &&
              mc_world_runtime_tick_inspect(call, &state) &&
              spawn_matches(w, &state, &oracle, &random, hostile_names[i], false) &&
              oracle.constructors > 1 && oracle.constructors == oracle.rejections &&
              oracle.publications == 0 && oracle.group == 0 && oracle.total == 0 &&
              w->entity_membership.count == 4 && w->player_membership.count == 1 &&
              w->persistent_bindings == before_bindings + 3 && math_holds(&math_before, 2));
        CHECK(mc_entity_retain(state.spawn_entity) == MC_ENTITY_OK);
        retained_mob = state.spawn_entity;
        CHECK(mc_world_runtime_tick_retire(&call, NULL) == MC_WORLD_STORAGE_OK &&
              w->persistent_bindings == before_bindings + 1 && math_holds(&math_before, 0));
        mc_entity_snapshot entity;
        CHECK(mc_entity_get_snapshot(retained_mob, &entity) == MC_ENTITY_OK &&
              entity.phase == MC_ENTITY_READY && entity.base.yaw == oracle.last_yaw);
        mc_entity_release(retained_mob);
        retained_mob = NULL;
        CHECK(w->persistent_bindings == before_bindings && w->active_calls == baseline_calls);
        printf("MOB_LOOP reject %s completed=1 constructors=%zu yaw-checked=1 random-checked=1\n",
               hostile_names[i], oracle.constructors);
        random_tick_row("reject", hostile_names[i], &random);
    }
    /* A real null World.r reaches FD.n's first mandatory draw after acquiring
     * its actual Chunk. Retain that exact typed nested prefix; source class
     * state and the reached time/cooldown stores survive terminal cleanup. */
    w->fields.N = w->fields.O = false;
    w->fields.Q = 7;
    CHECK(mc_world_info_source_rain_time_set(w->fields.x, 20) == MC_WORLD_INFO_OK &&
          mc_world_info_source_thunder_time_set(w->fields.x, 30) == MC_WORLD_INFO_OK &&
          mc_world_info_source_time_set(w->fields.x, 1) == MC_WORLD_INFO_OK);
    const size_t prefix_bindings = w->persistent_bindings;
    const int32_t prefix_g = w->fields.g;
    const int32_t registry_pc = f->registry[0]->block_initializer_pc;
    mc_java_random *const saved_random = w->fields.r;
    const mc_java_random prefix_random = *saved_random;
    w->fields.r = NULL;
    const mc_world_storage_status prefix_status =
        mc_world_runtime_tick_begin(w, tick_math, &call, NULL);
    w->fields.r = saved_random;
    CHECK(prefix_status == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          mc_world_runtime_tick_inspect(call, &state) && state.source_pc == 195 &&
          state.scheduled_returned && !state.returned && !state.random_returned &&
          state.error->child == MC_WORLD_TICK_CHILD_RANDOM && state.error->child_bytecode == 470 &&
          state.error->random != NULL && !state.error->native_policy &&
          state.error->exception_class != NULL &&
          strcmp(state.error->exception_class, "java/lang/NullPointerException") == 0 &&
          state.random != NULL && state.random->source_pc == 470 && !state.random->returned &&
          state.random->chunk_count == 361 && state.random->chunks_started == 1 &&
          state.random->chunks_returned == 0 && state.random->samples_started == 0 &&
          state.random->random_calls == 0 && state.random->lcg_updates == 0 &&
          state.random->chunk.ready && state.random->chunk.captured != NULL &&
          state.random->chunk.world_held && state.random->chunk.chunk_held &&
          w->persistent_bindings == prefix_bindings + 2 && w->active_calls == baseline_calls + 1 &&
          w->local_leases == 1 && math_holds(&math_before, 2) &&
          f->registry[0]->blocks_phase == MC_REGISTRY_READY &&
          f->registry[0]->block_initializer_pc == registry_pc && w->fields.g == prefix_g &&
          w->fields.Q == 6 && mc_test_java_random_equal(saved_random, &prefix_random) &&
          mc_world_info_time_get(w->fields.x, &now) == MC_WORLD_INFO_OK && now == 2);
    CHECK(mc_world_runtime_free(w, NULL) == MC_WORLD_STORAGE_BUSY);
    CHECK(mc_world_runtime_tick_retire(&call, NULL) == MC_WORLD_STORAGE_OK && call == NULL &&
          w->persistent_bindings == prefix_bindings && w->active_calls == baseline_calls &&
          w->local_leases == 0 && math_holds(&math_before, 0) &&
          f->registry[0]->blocks_phase == MC_REGISTRY_READY &&
          f->registry[0]->block_initializer_pc == registry_pc && w->fields.g == prefix_g &&
          w->fields.Q == 6 && w->fields.P->count == 361 &&
          mc_test_java_random_equal(saved_random, &prefix_random) &&
          mc_world_info_time_get(w->fields.x, &now) == MC_WORLD_INFO_OK && now == 2);
    puts("RANDOM_TICK prefix fd195:470 chunk-held=1 world-holds=2 math-holds=2 source-preserved=1");
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
    if (!retire_spawn_chunks(f, 0))
        ok = false;
    return ok;
}

static mc_registry_status missing_resource_stream(void *context, const char *name,
                                                  mc_java_properties_input *out) {
    resources *source = context;
    if (name == NULL || out == NULL || strcmp(name, "/lang/en_US.lang") != 0) {
        source->bad = true;
        return MC_REGISTRY_INVALID_ARGUMENT;
    }
    ++source->opens;
    /* Actual declared Class.getResourceAsStream result: a null InputStream.
     * The real Translation constructor then reaches Properties.load(null). */
    return MC_REGISTRY_NULL_REFERENCE;
}

static bool registry_tick_case(const char *root, unsigned int mode) {
    bool ok = false;
    fixture *f = calloc(1, sizeof(*f));
    mc_world_tick_call *call = NULL;
    mc_world_tick_state state = {0};
    mc_entity *player = NULL;
    char path[8192];
    CHECK(f != NULL && snprintf(path, sizeof(path), "%s/registry%u", root, mode) > 0 &&
          mc_materials_create(&f->materials) &&
          mc_world_regions_create(NULL, &f->regions, NULL) == MC_WORLD_STORAGE_OK &&
          mc_java_random_profile_create(16u * 1024u * 1024u, &f->profile) ==
              MC_JAVA_RANDOM_PROFILE_OK);
    resources *source = &f->sources[0];
    source->api = (mc_registry_resources){source, missing_resource_stream, NULL, NULL};
    static const uint16_t title[] = {'C', 'o', 'l', 'd'};
    CHECK(mc_world_info_name_copy((mc_nbt_string){title, COUNT(title)}, SIZE_MAX, &f->name) ==
          MC_WORLD_INFO_OK);
    const mc_registry_options options = {MC_REGISTRY_CLIENT, f->profile, f->materials,
                                         mode == 1 ? sizeof(mc_registry_classes) + 1024u : 0u,
                                         mode == 0 ? &source->api : NULL};
    CHECK(mc_registry_classes_allocate(&options, &f->registry[0], NULL) == MC_REGISTRY_OK &&
          mc_world_runtime_classes_create(MC_WORLD_INFO_CLIENT, f->profile,
                                          mc_math_classes_table(tick_math), &f->classes[0],
                                          NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_classes_bind_registry(f->classes[0], f->registry[0], NULL) ==
              MC_REGISTRY_OK &&
          mc_entity_classes_create(MC_ENTITY_CLIENT, f->profile, NULL, &f->entity_classes[0]) ==
              MC_ENTITY_OK &&
          mc_world_runtime_classes_bind_entities(f->classes[0], f->entity_classes[0]) ==
              MC_ENTITY_OK);
    CHECK(tick_world_create(f, path, 0, 512) && spawn_chunks(f, 0, 0) &&
          spawn_add_player(f, 0, &player) && f->registry[0]->blocks_phase == MC_REGISTRY_COLD);
    mc_world_runtime *world = f->worlds[0];
    world->fields.N = world->fields.O = false;
    world->fields.Q = 7;
    /* Source-set only World.r before entry. A nonzero first snow ticket
     * avoids Biome initialization before the genuine Block.n951 active use. */
    int32_t seed = 0;
    for (;; ++seed) {
        mc_java_random random;
        int32_t draw;
        mc_java_random_seed(&random, seed);
        CHECK(mc_java_random_next_bounded(&random, 100000, &draw) &&
              mc_java_random_next_bounded(&random, 16, &draw));
        if (draw != 0)
            break;
    }
    mc_java_random_reseed(world->fields.r, seed);
    const mc_world_storage_status initial =
        mc_world_runtime_tick_begin(world, tick_math, &call, NULL);
    const mc_registry_status original = mode == 0   ? MC_REGISTRY_NULL_REFERENCE
                                        : mode == 1 ? MC_REGISTRY_LIMIT
                                                    : MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY;
    CHECK(initial == (mode == 1 ? MC_WORLD_STORAGE_LIMIT : MC_WORLD_STORAGE_UNSUPPORTED_STATE) &&
          mc_world_runtime_tick_inspect(call, &state) && !state.returned &&
          state.source_pc == 195 && state.error->child == MC_WORLD_TICK_CHILD_RANDOM &&
          state.error->child_bytecode == 951 && state.random != NULL &&
          state.random->source_pc == 951 && state.random->samples_started == 1 &&
          state.random->samples_returned == 0 && state.error->random->registry != NULL &&
          state.error->random->registry->status == original &&
          state.error->native_policy == (mode != 0) &&
          (mode == 0 ? state.error->exception_class != NULL &&
                           strcmp(state.error->exception_class,
                                  "java/lang/ExceptionInInitializerError") == 0
                     : state.error->exception_class == NULL) &&
          f->registry[0]->blocks_phase == MC_REGISTRY_FAILED && !source->bad &&
          source->opens == (mode == 0 ? 1u : 0u));
    const mc_registry_error first = f->registry[0]->block_failure;
    const size_t allocation_bytes = f->registry[0]->allocation_bytes;
    const uint64_t uniquifier = atomic_load_explicit(&f->profile->uniquifier, memory_order_relaxed);
    CHECK(first.status == original &&
          mc_world_runtime_tick_retire(&call, NULL) == MC_WORLD_STORAGE_OK);
    mc_java_random_reseed(world->fields.r, seed);
    CHECK(
        mc_world_runtime_tick_begin(world, tick_math, &call, NULL) ==
            MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
        mc_world_runtime_tick_inspect(call, &state) && !state.returned && state.source_pc == 195 &&
        state.error->child_bytecode == 951 && state.error->random != NULL &&
        state.error->random->registry != NULL &&
        state.error->random->registry->status == MC_REGISTRY_FAILED_INITIALIZATION &&
        state.error->random->registry->initializer_pc == first.initializer_pc &&
        state.error->random->registry->active_initializer_class == first.active_initializer_class &&
        state.error->random->registry->active_initializer_pc == first.active_initializer_pc &&
        state.error->native_policy == (mode != 0) &&
        (mode == 0 ? state.error->exception_class != NULL &&
                         strcmp(state.error->exception_class, "java/lang/NoClassDefFoundError") == 0
                   : state.error->exception_class == NULL) &&
        f->registry[0]->block_failure.status == first.status &&
        f->registry[0]->block_failure.message == first.message &&
        f->registry[0]->block_failure.initializer_pc == first.initializer_pc &&
        f->registry[0]->block_failure.active_initializer_class == first.active_initializer_class &&
        f->registry[0]->block_failure.active_initializer_pc == first.active_initializer_pc &&
        f->registry[0]->allocation_bytes == allocation_bytes &&
        atomic_load_explicit(&f->profile->uniquifier, memory_order_relaxed) == uniquifier &&
        !source->bad && source->opens == (mode == 0 ? 1u : 0u));
    printf("REGISTRY_TICK %u first=%s cached=%s immutable=1 no-replay=1\n", mode,
           mode == 0 ? "EIIE" : "native", mode == 0 ? "NCDFE" : "native");
    ok = true;
cleanup:
    (void)mc_world_runtime_tick_retire(&call, NULL);
    mc_entity_release(player);
    if (f != NULL && f->worlds[0] != NULL && !retire_spawn_chunks(f, 0))
        ok = false;
    if (!cleanup_fixture(f))
        ok = false;
    return ok;
}

int main(int argc, char **argv) {
    int result = 1;
    fixture *f = NULL;
    char root[8192];
    if (argc != 3 || strcmp(argv[1], "world-tick") != 0 || !decode_path(argv[2], root, sizeof root))
        return 2;
    f = calloc(1, sizeof(*f));
    CHECK(f != NULL && mc_materials_create(&f->materials));
    CHECK(mc_math_classes_create(SIZE_MAX, &tick_math, NULL) == MC_MATH_CLASS_OK &&
          mc_math_classes_ensure(tick_math, NULL) == MC_MATH_CLASS_OK);
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
    CHECK(mc_world_runtime_classes_create(MC_WORLD_INFO_CLIENT, f->profile,
                                          mc_math_classes_table(tick_math), &f->classes[0],
                                          NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_classes_bind_registry(f->classes[0], f->registry[0], NULL) ==
              MC_REGISTRY_OK);
    CHECK(mc_entity_classes_create(MC_ENTITY_CLIENT, f->profile, NULL, &f->entity_classes[0]) ==
              MC_ENTITY_OK &&
          mc_world_runtime_classes_bind_entities(f->classes[0], f->entity_classes[0]) ==
              MC_ENTITY_OK);
    CHECK(mc_registry_classes_initialize_blocks(f->registry[0], NULL) == MC_REGISTRY_OK);
    CHECK(tick_world_create(f, root, 0, 512));
    CHECK(drain_cases(f) && tick_cases(f) && spawn_positive_case(f, root, 1, "uz", 0, false) &&
          spawn_positive_case(f, root, 2, "dl", 1, false) &&
          spawn_positive_case(f, root, 3, "cn", 0, true));
    for (unsigned int mode = 0; mode < 3; ++mode)
        CHECK(registry_tick_case(root, mode));
    result = 0;
cleanup:
    if (!cleanup_fixture(f))
        return 1;
    if (mc_math_classes_release(&tick_math, NULL) != MC_MATH_CLASS_OK)
        return 1;
    if (result == 0)
        printf("World scheduler: %u checks\n", checks);
    return result;
}
