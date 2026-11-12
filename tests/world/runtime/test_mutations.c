#include "mc/resources/translation/translation.h"
#include "mc/world/runtime/chunks.h"
#include "mc/world/runtime/mutations.h"

#include "../../../src/block/registry/registry_internal.h"
#include "../../../src/block/registry/runtime_internal.h"
#include "../../../src/world/runtime/chunk_provider_internal.h"
#include "../../../src/world/runtime/mutation/mutation_internal.h"
#include "../../../src/world/storage/objects_internal.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define COUNT(a) (sizeof(a) / sizeof((a)[0]))
#define CHECK(c)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(c)) {                                                                                \
            fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #c);                        \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

/* Native source contracts, not a second original execution. These cases use
 * actual GAME owners. Only fresh Chunk array contents and one restored q
 * element are prepared explicitly; no owner, phase, cache count or publication
 * is fabricated. The test author also authored the incremental height body;
 * its expectations below are bound to the paired primary, not an independent
 * implementation review or original/native comparison. */
static unsigned int checks;
static mc_math_trig trig;
static const uint8_t language[] = "stat.mineBlock=Mine %1$s\nstat.useItem=Use %1$s\n"
                                  "stat.breakItem=Break %1$s\nstat.craftItem=Craft %1$s\n";

typedef struct byte_stream {
    const uint8_t *data;
    size_t length, offset;
    bool closed;
} byte_stream;

typedef struct resource_source {
    byte_stream streams[3];
    mc_registry_resources api;
    mc_registry_classes *registry;
    mc_world_store *store;
    mc_world_runtime *world;
    mc_chunk_state *chunk;
    size_t cell, opens, closes, guarded;
    uint8_t expected_block, expected_metadata;
    bool fail_first, bad;
} resource_source;

typedef struct chunk_slot {
    mc_world_chunk *object;
    mc_chunk_state *state;
    mc_world_store *store;
    mc_world_runtime *world;
} chunk_slot;

typedef struct fixture {
    mc_world_regions *regions;
    mc_world_store *stores[2];
    mc_world_info_name *name;
    mc_materials *materials;
    mc_java_random_profile *profile;
    resource_source resources[2];
    mc_registry_classes *registry[2];
    mc_world_runtime_classes *classes[2];
    mc_world_runtime *worlds[2];
    mc_world_provider *dimensions[2];
    mc_chunk_provider_fields provider[2];
    bool bound[2];
    chunk_slot chunks[24];
    mc_chunk_state *cached;
    mc_chunk_state *cached_owned[32];
    size_t cached_count;
    mc_chunk_state *loaded;
} fixture;

static size_t cell(int32_t x, int32_t y, int32_t z) {
    return ((size_t)x << 11u) | ((size_t)z << 7u) | (size_t)y;
}

static uint8_t nibble(const uint8_t *bytes, size_t index) {
    return (uint8_t)(((uint32_t)bytes[index / 2u] >> (4u * (unsigned int)(index & 1u))) & 15u);
}

static bool bytes_are(const uint8_t *bytes, size_t length, uint8_t value) {
    for (size_t i = 0; i < length; ++i)
        if (bytes[i] != value)
            return false;
    return true;
}

static bool same_storage(const mc_world_storage_error *a, const mc_world_storage_error *b) {
    return a != NULL && b != NULL && a->status == b->status && a->operation == b->operation &&
           a->system_error == b->system_error;
}

static mc_java_properties_status read_bytes(void *context, uint8_t *out, size_t capacity,
                                            size_t *count) {
    byte_stream *s = context;
    if (s == NULL || out == NULL || count == NULL || capacity == 0 || s->closed ||
        s->offset > s->length)
        return MC_JAVA_PROPERTIES_IO_ERROR;
    size_t take = s->length - s->offset;
    if (take > 17)
        take = 17;
    if (take > capacity)
        take = capacity;
    if (take != 0)
        memcpy(out, s->data + s->offset, take);
    s->offset += take;
    *count = take;
    return MC_JAVA_PROPERTIES_OK;
}

static mc_registry_status open_resource(void *context, const char *name,
                                        mc_java_properties_input *out) {
    resource_source *r = context;
    const char *const names[] = {"/lang/en_US.lang", "/lang/stats_US.lang", "/achievement/map.txt"};
    if (name == NULL || out == NULL || r->opens >= COUNT(names) ||
        strcmp(name, names[r->opens]) != 0) {
        r->bad = true;
        return MC_REGISTRY_INVALID_ARGUMENT;
    }
    const size_t index = r->opens++;
    if (r->fail_first) {
        /* The source has already stored the actual new byte and first metadata
         * before its first q active use. Exercise three REAL busy owners at
         * that reached resource callback, without releasing or replacing any. */
        if (index != 0 || r->store == NULL || r->world == NULL || r->chunk == NULL ||
            r->chunk->data->blocks[r->cell] != r->expected_block ||
            nibble(r->chunk->data->metadata, r->cell) != r->expected_metadata ||
            mc_world_store_retire_chunk(r->store, r->chunk, NULL) != MC_WORLD_STORAGE_BUSY ||
            mc_world_runtime_retire_world(r->world, NULL) != MC_WORLD_STORAGE_BUSY ||
            mc_world_runtime_free(r->world, NULL) != MC_WORLD_STORAGE_BUSY ||
            mc_registry_classes_free(r->registry, NULL) != MC_REGISTRY_BUSY)
            r->bad = true;
        ++r->guarded;
        return MC_REGISTRY_IO_FAILED;
    }
    *out = (mc_java_properties_input){&r->streams[index], read_bytes};
    return MC_REGISTRY_OK;
}

static mc_registry_status close_resource(void *context, const mc_java_properties_input *input) {
    resource_source *r = context;
    if (input == NULL || input->context != &r->streams[2] || input->read != read_bytes) {
        r->bad = true;
        return MC_REGISTRY_INVALID_ARGUMENT;
    }
    r->streams[2].closed = true;
    ++r->closes;
    return MC_REGISTRY_OK;
}

static bool resource_ready(void *context, const mc_java_properties_input *input) {
    resource_source *r = context;
    if (input == NULL || input->read != read_bytes)
        r->bad = true;
    return false;
}

static bool open_store(fixture *f, size_t index, const char *root, const char *suffix) {
    bool ok = false;
    char path[8192];
    uint8_t map[256];
    const int n = snprintf(path, sizeof(path), "%s%s", root, suffix);
    CHECK(n > 0 && (size_t)n < sizeof(path));
    for (size_t i = 0; i < COUNT(map); ++i)
        map[i] = (uint8_t)i;
    mc_world_store_options options = {.session_token = 127,
                                      .block_map = map,
                                      .block_map_length = sizeof(map),
                                      .limits = mc_world_store_default_limits()};
    options.limits.max_loaded_chunks = 64;
    CHECK(mc_world_store_open(path, f->regions, &options, &f->stores[index], NULL) ==
          MC_WORLD_STORAGE_OK);
    ok = true;
cleanup:
    return ok;
}

static bool create_world(fixture *f, size_t index) {
    bool ok = false;
    mc_world_runtime_fields fields;
    mc_world_runtime_construction state;
    mc_world_runtime_construction_limits limits = mc_world_runtime_default_construction_limits();
    /* A coherent real one-entry provider also exercises capacity failure on an
     * absent key after the first genuine generation, without changing metadata. */
    limits.max_provider_chunks = 1;
    CHECK(mc_world_provider_create(0, NULL, &f->dimensions[index], NULL) == MC_WORLD_PROVIDER_OK);
    CHECK(mc_world_runtime_allocate_world(MC_WORLD_RUNTIME_CLIENT_DIRECT_METADATA,
                                          f->classes[index], &limits, &f->worlds[index],
                                          NULL) == MC_WORLD_STORAGE_OK);
    const mc_world_runtime_world_arguments args = {
        .store = f->stores[index], .name = f->name, .seed = 23, .dimension = f->dimensions[index]};
    CHECK(mc_world_runtime_construct_world(f->worlds[index], &args, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_construction_get(f->worlds[index], &state) == MC_WORLD_STORAGE_OK &&
          state.mode == MC_WORLD_RUNTIME_GAME && state.phase == MC_WORLD_RUNTIME_READY &&
          state.contexts_bound && state.cache_registered);
    CHECK(mc_world_runtime_fields_get(f->worlds[index], &fields) == MC_WORLD_STORAGE_OK &&
          fields.v != NULL && fields.w == f->stores[index] && fields.t == f->dimensions[index]);
    CHECK(mc_chunk_provider_bind_fields(fields.v, &f->provider[index], NULL) ==
          MC_WORLD_STORAGE_OK);
    f->bound[index] = true;
    CHECK(f->provider[index].world == f->worlds[index] &&
          f->provider[index].store == f->stores[index] &&
          mc_chunk_cache_validate(f->provider[index].cache) &&
          f->provider[index].cache->count == 0);
    ok = true;
cleanup:
    return ok;
}

static bool new_chunk(fixture *f, size_t slot, size_t world) {
    bool ok = false;
    uint8_t *blocks = calloc(MC_CHUNK_BLOCKS, 1);
    CHECK(blocks != NULL && slot < COUNT(f->chunks) && f->chunks[slot].state == NULL);
    uint8_t *const supplied = blocks;
    mc_world_chunk_error error;
    CHECK(mc_world_runtime_chunk_construct(f->worlds[world], -(int32_t)slot - 20, -20, &blocks,
                                           MC_CHUNK_BLOCKS, &f->chunks[slot].object,
                                           &error) == MC_WORLD_STORAGE_OK);
    chunk_slot *s = &f->chunks[slot];
    s->state = mc_world_chunk_state(s->object);
    s->store = f->stores[world];
    s->world = f->worlds[world];
    CHECK(blocks == NULL && s->state != NULL && s->state->data->blocks == supplied &&
          mc_world_store_owns_chunk(s->store, s->state) && !s->state->active && !s->state->dirty &&
          f->provider[world].cache->count == 0 && bytes_are(supplied, MC_CHUNK_BLOCKS, 0));
    ok = true;
cleanup:
    free(blocks);
    return ok;
}

/* No private body is entered without both genuine enclosing holds. Every
 * success/failure leaves those holds in reverse order and preserves its first
 * source diagnostic; this is not a substitute fake Chunk/world call context. */
static bool direct_set(chunk_slot *s, bool with_metadata, int32_t x, int32_t y, int32_t z,
                       int32_t id, int32_t metadata, bool *out, mc_world_mutation_error *error) {
    *error = mc_world_mutation_empty_error();
    error->bytecode = with_metadata ? 68u : 66u;
    if (mc_world_runtime_call_enter(s->world, NULL) != MC_WORLD_STORAGE_OK)
        return false;
    if (mc_world_store_lease_chunk(s->store, s->state, NULL) != MC_WORLD_STORAGE_OK) {
        mc_world_runtime_call_leave(s->world);
        return false;
    }
    const bool result =
        with_metadata
            ? mc_world_mutation_chunk_block_metadata(s->object, x, y, z, id, metadata, out, error)
            : mc_world_mutation_chunk_block(s->object, x, y, z, id, out, error);
    const bool released =
        mc_world_store_release_chunk(s->store, s->state, NULL) == MC_WORLD_STORAGE_OK;
    mc_world_runtime_call_leave(s->world);
    return result && released;
}

static bool direct_height(chunk_slot *s, int32_t x, int32_t y, int32_t z,
                          mc_world_mutation_error *error) {
    *error = mc_world_mutation_empty_error();
    /* Test-supplied enclosing-PC token. This direct helper call does not
     * execute the setter; it verifies that the helper retains its caller frame. */
    error->chunk_bytecode = 156;
    if (mc_world_runtime_call_enter(s->world, NULL) != MC_WORLD_STORAGE_OK)
        return false;
    if (mc_world_store_lease_chunk(s->store, s->state, NULL) != MC_WORLD_STORAGE_OK) {
        mc_world_runtime_call_leave(s->world);
        return false;
    }
    const bool result = mc_world_mutation_height(s->object, x, y, z, error);
    const bool released =
        mc_world_store_release_chunk(s->store, s->state, NULL) == MC_WORLD_STORAGE_OK;
    mc_world_runtime_call_leave(s->world);
    return result && released;
}

static bool cold_failure(fixture *f) {
    bool ok = false, out = true;
    mc_world_mutation_error first, again;
    mc_registry_block_view view;
    CHECK(new_chunk(f, 0, 1));
    chunk_slot *s = &f->chunks[0];
    mc_chunk_data *d = s->state->data;
    const size_t index = cell(8, 7, 8);
    memset(d->metadata, 0xa5, sizeof(d->metadata));
    memset(d->skylight, 0xb6, sizeof(d->skylight));
    resource_source *r = &f->resources[1];
    r->store = s->store;
    r->world = s->world;
    r->chunk = s->state;
    r->cell = index;
    r->expected_block = 1;
    r->expected_metadata = 6;
    out = true;
    CHECK(mc_world_runtime_block_set_quiet(s->world, 0, 128, 0, INT32_MAX, &out, &again) ==
              MC_WORLD_STORAGE_OK &&
          !out && r->opens == 0 && f->provider[1].cache->count == 0);
    CHECK(mc_registry_classes_inspect_blocks(f->registry[1], &view, NULL) == MC_REGISTRY_OK &&
          view.phase == MC_REGISTRY_COLD);
    out = true;
    CHECK(!direct_set(s, true, 8, 7, 8, 1, 6, &out, &first) && out);
    CHECK(first.bytecode == 68 && first.chunk_bytecode == 170 && first.height_bytecode == 0 &&
          first.registry.status == MC_REGISTRY_IO_FAILED && first.registry.constructor_pc == 23 &&
          first.registry.active_initializer_pc == 4 &&
          first.registry.active_initializer_class != NULL &&
          strcmp(first.registry.active_initializer_class, "nh") == 0 &&
          same_storage(&first.cause, mc_world_runtime_last_error(s->world)));
    CHECK(r->opens == 1 && r->guarded == 1 && !r->bad && d->blocks[index] == 1 &&
          nibble(d->metadata, index) == 6 && nibble(d->metadata, index ^ 1u) == 5 &&
          bytes_are(d->skylight, sizeof(d->skylight), 0xb6) &&
          bytes_are(d->heightmap, sizeof(d->heightmap), 0) && d->minimum_height == 0 &&
          !s->state->dirty);
    CHECK(mc_registry_classes_inspect_blocks(f->registry[1], &view, NULL) == MC_REGISTRY_OK &&
          view.phase == MC_REGISTRY_FAILED);
    const mc_world_storage_error saved = *mc_world_runtime_last_error(s->world);
    out = true;
    CHECK(direct_set(s, true, 8, 7, 8, 1, 6, &out, &again) && !out && r->opens == 1 &&
          same_storage(&saved, mc_world_runtime_last_error(s->world)));
    out = true;
    CHECK(direct_set(s, false, 8, 7, 8, 1, 0, &out, &again) && !out && r->opens == 1 &&
          !s->state->dirty);
    ok = true;
cleanup:
    return ok;
}

static bool ordinary_overloads(fixture *f) {
    bool ok = false;
    for (size_t mode = 0; mode < 2; ++mode) {
        const bool with_metadata = mode != 0;
        CHECK(new_chunk(f, mode + 1u, 0));
        chunk_slot *s = &f->chunks[mode + 1u];
        mc_chunk_data *d = s->state->data;
        mc_world_mutation_error error;
        bool out = true;
        CHECK(direct_set(s, with_metadata, 8, 10, 8, 0, 0, &out, &error) && !out &&
              !s->state->dirty && bytes_are(d->blocks, MC_CHUNK_BLOCKS, 0));
        out = false;
        CHECK(direct_set(s, with_metadata, 8, 10, 8, 1, 3, &out, &error) && out &&
              d->blocks[cell(8, 10, 8)] == 1 && d->heightmap[136] == 11 && d->minimum_height == 0 &&
              s->state->dirty && nibble(d->metadata, cell(8, 10, 8)) == (with_metadata ? 3 : 0) &&
              nibble(d->skylight, cell(8, 10, 8)) == 0);
        out = true;
        CHECK(direct_set(s, with_metadata, 8, 10, 8, 1, 3, &out, &error) && !out &&
              d->heightmap[136] == 11);
        out = false;
        CHECK(direct_set(s, with_metadata, 8, 10, 8, 0, 0, &out, &error) && out &&
              d->blocks[cell(8, 10, 8)] == 0 && d->heightmap[136] == 0 && d->minimum_height == 0 &&
              nibble(d->metadata, cell(8, 10, 8)) == 0 &&
              nibble(d->skylight, cell(8, 10, 8)) == 15);
    }
    ok = true;
cleanup:
    return ok;
}

static bool top_and_full_metadata(fixture *f) {
    bool ok = false, out = false;
    mc_world_mutation_error error;
    CHECK(new_chunk(f, 3, 0));
    chunk_slot *s = &f->chunks[3];
    mc_chunk_data *d = s->state->data;
    CHECK(direct_set(s, false, 8, 127, 8, 1, 0, &out, &error) && out && d->heightmap[136] == 128 &&
          d->blocks[cell(8, 127, 8)] == 1 && nibble(d->skylight, cell(8, 127, 8)) == 0);
    out = false;
    CHECK(direct_set(s, false, 8, 127, 8, 0, 0, &out, &error) && out && d->heightmap[136] == 0 &&
          nibble(d->skylight, cell(8, 127, 8)) == 15);
    CHECK(new_chunk(f, 4, 0));
    s = &f->chunks[4];
    d = s->state->data;
    /* Actual retained h bytes are caller-prepared Chunk inputs. The original
     * full rescan seed is127, even when every resulting unsigned height is128. */
    memset(d->heightmap, 128, sizeof(d->heightmap));
    d->heightmap[136] = 0;
    out = false;
    CHECK(direct_set(s, true, 8, 127, 8, 1, 0, &out, &error) && out &&
          bytes_are(d->heightmap, sizeof(d->heightmap), 128) && d->minimum_height == 127);
    CHECK(new_chunk(f, 5, 0));
    s = &f->chunks[5];
    d = s->state->data;
    d->blocks[cell(8, 10, 8)] = 1;
    d->heightmap[136] = 11;
    for (unsigned int i = 0; i < 2; ++i) {
        out = false;
        CHECK(direct_set(s, true, 8, 10, 8, 1, 16, &out, &error) && out &&
              nibble(d->metadata, cell(8, 10, 8)) == 0 && d->heightmap[136] == 11);
    }
    out = true;
    CHECK(direct_set(s, true, 8, 10, 8, 1, 0, &out, &error) && !out);
    ok = true;
cleanup:
    return ok;
}

static bool full_ids_and_null_old(fixture *f) {
    bool ok = false;
    for (size_t mode = 0; mode < 2; ++mode) {
        for (size_t id_case = 0; id_case < 2; ++id_case) {
            CHECK(new_chunk(f, 6u + mode * 2u + id_case, 0));
            chunk_slot *s = &f->chunks[6u + mode * 2u + id_case];
            mc_chunk_data *d = s->state->data;
            mc_world_mutation_error error;
            bool out = true;
            const int32_t id = id_case == 0 ? 257 : 256;
            /* A retained opaque block below the target keeps height6 in the
             * narrowed-air case. Real absent-neighbor interval calls then set
             * dirty BEFORE the late full-ID add lookup escapes. */
            d->blocks[cell(8, 5, 8)] = 1;
            d->blocks[cell(8, 10, 8)] = 1;
            d->heightmap[136] = 11;
            memset(d->metadata, 0xa5, sizeof(d->metadata));
            CHECK(!direct_set(s, mode != 0, 8, 10, 8, id, 6, &out, &error) && out);
            CHECK(error.chunk_bytecode == (mode ? 284u : 243u) &&
                  error.registry.status == MC_REGISTRY_INDEX_OUT_OF_RANGE &&
                  error.registry.block_id == -1 && error.registry.initializer_pc == -1 &&
                  error.registry.constructor_pc == -1 && error.registry.constructor_class == NULL &&
                  error.registry.active_initializer_class == NULL && error.height_bytecode == 0 &&
                  d->blocks[cell(8, 10, 8)] == (uint8_t)id &&
                  nibble(d->metadata, cell(8, 10, 8)) == (mode ? 6 : 0) &&
                  d->heightmap[136] == (id_case ? 6 : 11) && s->state->dirty &&
                  same_storage(&error.cause, mc_world_runtime_last_error(s->world)));
        }
        CHECK(new_chunk(f, 10u + mode, 0));
        chunk_slot *s = &f->chunks[10u + mode];
        mc_chunk_data *d = s->state->data;
        mc_world_mutation_error error;
        const size_t index = cell(8, 10, 8);
        d->blocks[index] = 255;
        memset(d->metadata, 0xa5, sizeof(d->metadata));
        bool out = true;
        CHECK(!direct_set(s, mode != 0, 8, 10, 8, 0, 6, &out, &error) && out &&
              error.chunk_bytecode == (mode ? 142u : 117u) &&
              error.status == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
              error.registry.status == MC_REGISTRY_OK && error.hook_class == NULL &&
              error.hook_bytecode == 0 &&
              same_storage(&error.cause, mc_world_runtime_last_error(s->world)) &&
              d->blocks[index] == 0 && nibble(d->metadata, index) == 5 && !s->state->dirty &&
              bytes_are(d->heightmap, sizeof(d->heightmap), 0));
    }
    ok = true;
cleanup:
    return ok;
}

static bool retained_heights(fixture *f) {
    bool ok = false;
    mc_world_mutation_error error;
    /* Both valid arrays can contain retained unsigned height255. For z8 the
     * additive scan and later OR getter alias a cell in the neighboring local
     * column. For z15 the two literal index forms are DIFFERENT cells. */
    for (size_t i = 0; i < 2; ++i) {
        CHECK(new_chunk(f, 12u + i, 0));
        chunk_slot *s = &f->chunks[12u + i];
        mc_chunk_data *d = s->state->data;
        const int32_t z = i == 0 ? 8 : 15;
        const size_t column = (size_t)z * 16u + 8u;
        const size_t base = cell(8, 0, z);
        d->heightmap[column] = 255;
        memset(d->skylight, 0x99, sizeof(d->skylight));
        d->blocks[base + 253u] = 1;
        CHECK(((base + 253u) == (base | 253u)) == (i == 0) &&
              d->blocks[base | 253u] == (i == 0 ? 1 : 0));
        CHECK(direct_height(s, 8, 0, z, &error) && d->heightmap[column] == 254 &&
              d->minimum_height == 0 && s->state->dirty && nibble(d->skylight, base | 254u) == 15 &&
              nibble(d->skylight, base | 253u) == (i == 0 ? 0 : 14) && d->blocks[base + 253u] == 1);
        CHECK(nibble(d->skylight, base | 127u) == 9);
    }
    CHECK(new_chunk(f, 14, 0));
    chunk_slot *s = &f->chunks[14];
    mc_chunk_data *d = s->state->data;
    d->heightmap[255] = 255;
    memset(d->skylight, 0x99, sizeof(d->skylight));
    CHECK(!direct_height(s, 15, 0, 15, &error) && error.chunk_bytecode == 156 &&
          error.height_bytecode == 59 && error.registry.status == MC_REGISTRY_OK &&
          d->heightmap[255] == 255 && d->minimum_height == 0 && !s->state->dirty &&
          bytes_are(d->skylight, sizeof(d->skylight), 0x99));
    ok = true;
cleanup:
    return ok;
}

static bool negative_opacity(fixture *f) {
    bool ok = false, altered = false;
    int32_t saved = 0;
    mc_world_mutation_error error;
    CHECK(new_chunk(f, 15, 0));
    chunk_slot *s = &f->chunks[15];
    mc_chunk_data *d = s->state->data;
    CHECK(f->registry[0]->blocks.q != NULL);
    saved = f->registry[0]->blocks.q[1];
    CHECK(saved == 255);
    /* Explicit native/source input control: mutate the ACTUAL retained q
     * element, not an observer palette, and restore it on every cleanup path. */
    f->registry[0]->blocks.q[1] = -1;
    altered = true;
    d->blocks[cell(8, 1, 8)] = 1;
    CHECK(direct_height(s, 8, 2, 8, &error) && d->heightmap[136] == 2 &&
          nibble(d->skylight, cell(8, 1, 8)) == 0 && nibble(d->skylight, cell(8, 0, 8)) == 15 &&
          s->state->dirty);
    f->registry[0]->blocks.q[1] = saved;
    altered = false;
    CHECK(f->registry[0]->blocks.q[1] == 255);
    ok = true;
cleanup:
    if (altered)
        f->registry[0]->blocks.q[1] = saved;
    return ok;
}

static bool captured_view_guards(fixture *f) {
    bool ok = false;
    mc_registry_error error;
    mc_registry_block_m_view actual_m;
    mc_registry_block_t_view actual_t;
    CHECK(mc_registry_block_m_get(f->registry[0], &actual_m, &error) == MC_REGISTRY_OK &&
          actual_m.identity != NULL && actual_m.length == 256);
    CHECK(mc_registry_block_t_get(f->registry[0], &actual_t, &error) == MC_REGISTRY_OK &&
          actual_t.identity != NULL && actual_t.length == 256);
    const mc_block *sentinel = NULL;
    CHECK(mc_registry_block_m_at(&actual_m, 1, &sentinel, &error) == MC_REGISTRY_OK &&
          sentinel != NULL);
    const mc_block *block = sentinel;
    CHECK(mc_registry_block_m_at(&actual_m, 255, &block, &error) == MC_REGISTRY_OK &&
          block == NULL);
    /* SYNTHETIC NONOWNING views exercise null-before-index in the accessor
     * contract. No actual static field, registry phase or owning array changes. */
    const mc_registry_block_m_view null_m = {f->registry[0], NULL, 256};
    const mc_registry_block_t_view null_t = {f->registry[0], NULL, 256};
    const int32_t indices[] = {-1, 256, INT32_MIN, INT32_MAX};
    for (size_t i = 0; i < COUNT(indices); ++i) {
        block = sentinel;
        CHECK(mc_registry_block_m_at(&null_m, indices[i], &block, &error) ==
                  MC_REGISTRY_NULL_REFERENCE &&
              block == sentinel && error.constructor_class == NULL &&
              error.active_initializer_class == NULL);
        bool flag = true;
        CHECK(mc_registry_block_t_at(&null_t, indices[i], &flag, &error) ==
                  MC_REGISTRY_NULL_REFERENCE &&
              flag);
        block = sentinel;
        CHECK(mc_registry_block_m_at(&actual_m, indices[i], &block, &error) ==
                  MC_REGISTRY_INDEX_OUT_OF_RANGE &&
              block == sentinel);
        CHECK(mc_registry_block_t_at(&actual_t, indices[i], &flag, &error) ==
                  MC_REGISTRY_INDEX_OUT_OF_RANGE &&
              flag);
    }
    ok = true;
cleanup:
    return ok;
}

static bool public_cached(fixture *f) {
    bool ok = false, out = true;
    mc_world_mutation_error error;
    mc_chunk_provider_fields *p = &f->provider[0];
    const size_t before = mc_world_store_chunk_count(f->stores[0]);
    const int32_t bad[][3] = {{-32000001, 1, 0}, {32000000, 1, 0}, {0, 1, -32000001},
                              {0, 1, 32000001},  {0, -1, 0},       {0, 128, 0}};
    for (size_t i = 0; i < COUNT(bad); ++i) {
        out = true;
        CHECK(mc_world_runtime_block_set_quiet(f->worlds[0], bad[i][0], bad[i][1], bad[i][2],
                                               INT32_MAX, &out, &error) == MC_WORLD_STORAGE_OK &&
              !out);
        out = true;
        CHECK(mc_world_runtime_block_metadata_set(f->worlds[0], bad[i][0], bad[i][1], bad[i][2],
                                                  INT32_MAX, INT32_MAX, &out,
                                                  &error) == MC_WORLD_STORAGE_OK &&
              !out);
        CHECK(mc_world_runtime_metadata_set(f->worlds[0], bad[i][0], bad[i][1], bad[i][2],
                                            INT32_MAX, &error) == MC_WORLD_STORAGE_OK &&
              p->cache->count == 0 && mc_world_store_chunk_count(f->stores[0]) == before);
    }
    /* The ordinary provider generates/publishes this one far Chunk; its seven
     * caller-selected cells are prepared through REAL quiet setters. The six
     * neighbors fit inside that same Chunk, avoiding accidental population. */
    CHECK(mc_world_runtime_get(f->worlds[0], 8, 0, &f->cached) == MC_CHUNK_ACQUIRE_OK &&
          f->cached != NULL && f->cached->active && f->cached->x == 8 && f->cached->z == 0 &&
          p->cache->count == 1 && p->cache->insertion_count == 1 &&
          p->cache->insertions[0] == f->cached &&
          mc_world_store_chunk_count(f->stores[0]) == before + 1);
    const int32_t cells[][3] = {{136, 120, 8}, {135, 120, 8}, {137, 120, 8}, {136, 119, 8},
                                {136, 121, 8}, {136, 120, 7}, {136, 120, 9}};
    for (size_t i = 0; i < COUNT(cells); ++i) {
        out = false;
        CHECK(mc_world_runtime_block_set_quiet(f->worlds[0], cells[i][0], cells[i][1], cells[i][2],
                                               0, &out, &error) == MC_WORLD_STORAGE_OK);
    }
    out = false;
    CHECK(mc_world_runtime_block_set(f->worlds[0], 136, 120, 8, 1, &out, &error) ==
              MC_WORLD_STORAGE_OK &&
          out && f->cached->data->blocks[cell(8, 120, 8)] == 1);
    /* Seed a genuine earlier World diagnostic through the late full-ID m
     * failure, then reach a DIFFERENT coherent cache-capacity failure at48.
     * Neither source failure may be replaced by the older retained cause. */
    out = true;
    CHECK(mc_world_runtime_block_set_quiet(f->worlds[0], 136, 120, 8, 257, &out, &error) !=
              MC_WORLD_STORAGE_OK &&
          out && error.bytecode == 66 && error.chunk_bytecode == 243 &&
          error.registry.status == MC_REGISTRY_INDEX_OUT_OF_RANGE &&
          error.cause.status != MC_WORLD_STORAGE_LIMIT &&
          same_storage(&error.cause, mc_world_runtime_last_error(f->worlds[0])));
    const mc_world_storage_error old_cause = *mc_world_runtime_last_error(f->worlds[0]);
    out = true;
    CHECK(mc_world_runtime_block_set_quiet(f->worlds[0], 152, 120, 8, 1, &out, &error) ==
              MC_WORLD_STORAGE_LIMIT &&
          out && error.bytecode == 48 && error.chunk_bytecode == 0 &&
          error.cause.status == MC_WORLD_STORAGE_LIMIT && old_cause.status != error.cause.status &&
          same_storage(&error.cause, mc_world_runtime_last_error(f->worlds[0])) &&
          p->cache->count == 1 && p->cache->insertion_count == 1 &&
          p->cache->insertions[0] == f->cached &&
          mc_world_store_chunk_count(f->stores[0]) == before + 1);
    mc_registry_block_t_view t;
    bool flag = true;
    CHECK(mc_registry_block_t_get(f->registry[0], &t, NULL) == MC_REGISTRY_OK &&
          mc_registry_block_t_at(&t, 1, &flag, NULL) == MC_REGISTRY_OK && !flag);
    CHECK(mc_world_runtime_metadata_set(f->worlds[0], 136, 120, 8, 16, &error) ==
              MC_WORLD_STORAGE_OK &&
          nibble(f->cached->data->metadata, cell(8, 120, 8)) == 0);
    CHECK(mc_world_runtime_metadata_set(f->worlds[0], 136, 120, 8, 6, &error) ==
              MC_WORLD_STORAGE_OK &&
          nibble(f->cached->data->metadata, cell(8, 120, 8)) == 6);
    for (size_t i = 1; i < COUNT(cells); ++i)
        CHECK(f->cached->data->blocks[cell(cells[i][0] & 15, cells[i][1], cells[i][2] & 15)] == 0);
    CHECK(p->cache->count == 1 && p->cache->insertion_count == 1 && f->cached->dirty &&
          mc_world_store_retire_chunk(f->stores[0], f->cached, NULL) == MC_WORLD_STORAGE_BUSY);
    ok = true;
cleanup:
    return ok;
}

static bool drain_world(fixture *f, size_t index) {
    if (!f->bound[index])
        return true;
    mc_chunk_cache *cache = f->provider[index].cache;
    mc_chunk_pending *pending = f->provider[index].pending;
    if (!mc_chunk_cache_validate(cache) || !mc_chunk_pending_validate(pending))
        return false;
    /* Retain every actual published identity BEFORE maintenance unpublishes it,
     * including a completed publication whose later caller failed. These are
     * borrowed tracking slots, never cache writes or ownership fabrication. */
    for (size_t i = 0; i < cache->insertion_count; ++i) {
        mc_chunk_state *const state = cache->insertions[i];
        bool found = false;
        if (index != 0 || state == NULL || !mc_world_store_owns_chunk(f->stores[0], state))
            return false;
        for (size_t j = 0; j < f->cached_count; ++j)
            if (f->cached_owned[j] == state)
                found = true;
        if (!found) {
            if (f->cached_count == COUNT(f->cached_owned))
                return false;
            f->cached_owned[f->cached_count++] = state;
        }
    }
    for (size_t i = 0; i < cache->count; ++i) {
        bool changed = false;
        if (mc_chunk_pending_add(pending, cache->entries[i].key, &changed) != MC_CHUNK_ACQUIRE_OK)
            return false;
    }
    while (pending->count != 0) {
        const size_t before = pending->count;
        bool result = true;
        if (mc_world_runtime_maintain(f->worlds[index], &result) != MC_CHUNK_LIFECYCLE_OK ||
            result || pending->count >= before)
            return false;
    }
    return cache->count == 0 && cache->insertion_count == 0;
}

static bool saved_and_unpublished(fixture *f) {
    bool ok = false;
    mc_world_info_fields before, after;
    CHECK(f->cached != NULL && f->cached->dirty &&
          mc_world_info_fields_get(mc_world_runtime_info(f->worlds[0]), &before) ==
              MC_WORLD_INFO_OK &&
          before.value.size_on_disk == 0 && mc_world_store_info(f->stores[0]) == NULL);
    CHECK(drain_world(f, 0));
    /* Unload saves and unpublishes; it does not clear the dirty field. */
    CHECK(!f->cached->active && f->cached->dirty);
    CHECK(mc_world_info_fields_get(mc_world_runtime_info(f->worlds[0]), &after) ==
              MC_WORLD_INFO_OK &&
          after.value.size_on_disk > before.value.size_on_disk &&
          mc_world_store_info(f->stores[0]) == NULL);
    CHECK(mc_world_store_load_chunk(f->stores[0], 0, 8, 0, &f->loaded, NULL) ==
          MC_WORLD_STORAGE_OK);
    CHECK(f->loaded != NULL && f->loaded != f->cached &&
          f->loaded->data->blocks[cell(8, 120, 8)] == 1 &&
          nibble(f->loaded->data->metadata, cell(8, 120, 8)) == 6);
    CHECK(mc_world_store_retire_chunk(f->stores[0], f->loaded, NULL) == MC_WORLD_STORAGE_OK);
    f->loaded = NULL;
    ok = true;
cleanup:
    return ok;
}

static bool cleanup_fixture(fixture *f) {
    if (f == NULL)
        return true;
    for (size_t i = 0; i < COUNT(f->worlds); ++i)
        if (f->worlds[i] != NULL && !drain_world(f, i))
            return false;
    if (f->loaded != NULL) {
        if (mc_world_store_retire_chunk(f->stores[0], f->loaded, NULL) != MC_WORLD_STORAGE_OK)
            return false;
        f->loaded = NULL;
    }
    for (size_t i = 0; i < f->cached_count; ++i)
        if (mc_world_store_retire_chunk(f->stores[0], f->cached_owned[i], NULL) !=
            MC_WORLD_STORAGE_OK)
            return false;
    f->cached = NULL;
    for (size_t i = 0; i < COUNT(f->chunks); ++i) {
        if (f->chunks[i].state != NULL &&
            mc_world_store_retire_chunk(f->chunks[i].store, f->chunks[i].state, NULL) !=
                MC_WORLD_STORAGE_OK)
            return false;
        f->chunks[i] = (chunk_slot){0};
    }
    for (size_t i = 0; i < COUNT(f->worlds); ++i) {
        if (f->worlds[i] != NULL &&
            (mc_world_runtime_retire_world(f->worlds[i], NULL) != MC_WORLD_STORAGE_OK ||
             mc_world_runtime_free(f->worlds[i], NULL) != MC_WORLD_STORAGE_OK))
            return false;
        f->worlds[i] = NULL;
    }
    for (size_t i = 0; i < COUNT(f->dimensions); ++i)
        mc_world_provider_release(f->dimensions[i]);
    for (size_t i = 0; i < COUNT(f->classes); ++i)
        if (mc_world_runtime_classes_free(f->classes[i], NULL) != MC_WORLD_STORAGE_OK)
            return false;
    for (size_t i = 0; i < COUNT(f->registry); ++i)
        if (mc_registry_classes_free(f->registry[i], NULL) != MC_REGISTRY_OK)
            return false;
    mc_world_info_name_release(f->name);
    for (size_t i = 0; i < COUNT(f->stores); ++i)
        mc_world_store_free(f->stores[i]);
    mc_world_regions_free(f->regions);
    mc_java_random_profile_free(f->profile);
    mc_materials_destroy(f->materials);
    free(f);
    return true;
}

static bool decode_path(const char *hex, char *out, size_t capacity) {
    const size_t n = strlen(hex);
    if (n == 0 || n % 2u != 0 || n / 2u >= capacity)
        return false;
    for (size_t i = 0; i < n / 2u; ++i) {
        unsigned int value = 0;
        for (size_t j = 0; j < 2; ++j) {
            const unsigned char c = (unsigned char)hex[2 * i + j];
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
    out[n / 2u] = '\0';
    return true;
}

int main(int argc, char **argv) {
    int result = 1;
    fixture *f = NULL;
    char root[8192];
    const uint16_t title[] = {'M', 'u', 't'};
    if (argc != 3 || strcmp(argv[1], "world-mutations") != 0 ||
        !decode_path(argv[2], root, sizeof(root)) || !mc_math_trig_init(&trig))
        return 1;
    f = calloc(1, sizeof(*f));
    CHECK(f != NULL);
    CHECK(mc_world_regions_create(NULL, &f->regions, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(open_store(f, 0, root, "/mutations") && open_store(f, 1, root, "/cold"));
    CHECK(mc_materials_create(&f->materials));
    CHECK(mc_java_random_profile_create(4u * 1024u * 1024u, &f->profile) ==
          MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_world_info_name_copy((mc_nbt_string){title, COUNT(title)}, SIZE_MAX, &f->name) ==
          MC_WORLD_INFO_OK);
    for (size_t i = 0; i < COUNT(f->registry); ++i) {
        resource_source *r = &f->resources[i];
        r->streams[0] = (byte_stream){.data = language, .length = sizeof(language) - 1u};
        r->streams[1] = (byte_stream){.data = language};
        r->streams[2] = (byte_stream){.data = language};
        r->fail_first = i == 1;
        r->api = (mc_registry_resources){r, open_resource, close_resource, resource_ready};
        const mc_registry_options options = {MC_REGISTRY_CLIENT, f->profile, f->materials, 0,
                                             &r->api};
        CHECK(mc_registry_classes_allocate(&options, &f->registry[i], NULL) == MC_REGISTRY_OK);
        r->registry = f->registry[i];
        CHECK(mc_world_runtime_classes_create(MC_WORLD_INFO_CLIENT, f->profile, &trig,
                                              &f->classes[i], NULL) == MC_WORLD_STORAGE_OK);
        CHECK(mc_world_runtime_classes_bind_registry(f->classes[i], f->registry[i], NULL) ==
              MC_REGISTRY_OK);
        CHECK(create_world(f, i));
    }
    CHECK(cold_failure(f));
    CHECK(mc_registry_classes_initialize_blocks(f->registry[0], NULL) == MC_REGISTRY_OK &&
          f->resources[0].opens == 3 && f->resources[0].closes == 1);
    CHECK(ordinary_overloads(f));
    CHECK(top_and_full_metadata(f));
    CHECK(full_ids_and_null_old(f));
    CHECK(retained_heights(f));
    CHECK(negative_opacity(f));
    CHECK(captured_view_guards(f));
    CHECK(public_cached(f));
    CHECK(saved_and_unpublished(f));
    CHECK(!f->resources[0].bad && !f->resources[1].bad);
    for (size_t i = 0; i < COUNT(f->classes); ++i) {
        mc_world_runtime_class_state state;
        CHECK(mc_world_runtime_classes_get(f->classes[i], &state) == MC_WORLD_STORAGE_OK &&
              state.enqueue_depth == 0);
    }
    result = 0;
cleanup:
    if (!cleanup_fixture(f)) {
        fputs("mutation fixture cleanup refused; live dependencies retained\n", stderr);
        return 1;
    }
    if (result == 0 && printf("PASS: %u World mutation checks (world-mutations)\n", checks) < 0)
        result = 1;
    return result;
}
