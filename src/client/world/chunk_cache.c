#include "chunk_cache_internal.h"

#include "../../block/registry/registry_internal.h"
#include "../../block/registry/runtime_internal.h"
#include "../../world/chunk/object_state_internal.h"
#include "../../world/runtime/chunk_provider_internal.h"
#include "../../world/runtime/construction_internal.h"
#include "../../world/runtime/runtime_internal.h"
#include "../../world/storage/objects_internal.h"
#include "mc/block/registry/rendering.h"
#include "mc/block/registry/runtime.h"
#include "mc/core/java/java.h"

#include <stdlib.h>
#include <string.h>

_Static_assert(MC_BLOCK_KIND_COUNT == 69, "audit ew normal-cube virtual descendants");

static mc_client_chunk_cache_error empty_error(const char *method, const char *descriptor) {
    return (mc_client_chunk_cache_error){.method = method,
                                         .descriptor = descriptor,
                                         .registry = {.initializer_pc = -1,
                                                      .block_id = -1,
                                                      .constructor_pc = -1,
                                                      .active_initializer_pc = -1,
                                                      .item_argument = -1}};
}

static bool fail(mc_client_chunk_cache_error *e, mc_client_chunk_cache_status status,
                 const char *operation, unsigned int pc, bool native) {
    e->status = status;
    e->operation = operation;
    e->bytecode = pc;
    e->native_policy = native;
    return false;
}

static mc_client_chunk_cache_status copy_error(mc_client_chunk_cache_error *out,
                                               const mc_client_chunk_cache_error *e) {
    if (out != NULL)
        *out = *e;
    return e->status;
}

static void wrap(mc_client_chunk_cache_error *e, const char *method, const char *descriptor,
                 unsigned int pc) {
    /* Maximum actual depth is c/a(IIII)->d(III)->a(IIIZ)->a(IIIZ)
     * or a(III). No open recursion or elided source frame is introduced. */
    if (e->nested_count < sizeof(e->nested) / sizeof(*e->nested)) {
        memmove(e->nested + 1, e->nested, e->nested_count * sizeof(*e->nested));
        e->nested[0] = (mc_client_chunk_cache_frame){e->method, e->descriptor, e->bytecode};
        ++e->nested_count;
    }
    e->method = method;
    e->descriptor = descriptor;
    e->bytecode = pc;
}

static bool storage(mc_client_chunk_cache_error *e, mc_world_storage_status status,
                    unsigned int pc) {
    return status == MC_WORLD_STORAGE_OK ||
           fail(e,
                status == MC_WORLD_STORAGE_LIMIT ? MC_CLIENT_CHUNK_CACHE_LIMIT
                                                 : MC_CLIENT_CHUNK_CACHE_CHILD_FAILED,
                e->storage.operation, pc, true);
}

static bool acquire(mc_client_chunk_cache *c, int32_t x, int32_t z,
                    mc_client_chunk_cache_occurrence *cell) {
    mc_client_chunk_cache_error *e = &c->failure;
    mc_world_runtime *world = c->state.d;
    if (world == NULL)
        return fail(e, MC_CLIENT_CHUNK_CACHE_SOURCE_NULL, "actual ew constructor World receiver",
                    112, false);
    e->child_bytecode = 6; /* fd.c(II)'s actual provider invocation. */
    if (world->fields.v == NULL)
        return fail(e, MC_CLIENT_CHUNK_CACHE_SOURCE_NULL, "actual World.c null provider", 112,
                    false);
    mc_chunk_provider_fields provider;
    if (!storage(e, mc_chunk_provider_bind_fields(world->fields.v, &provider, &e->storage), 112))
        return false;
    if (provider.world != world || provider.cache != world->acquisition.cache ||
        provider.pending != world->acquisition.pending || provider.store != world->options.store)
        return fail(e, MC_CLIENT_CHUNK_CACHE_CHILD_FAILED,
                    "ew actual World provider native binding", 112, true);
    mc_chunk_state *state = NULL;
    const mc_chunk_acquire_status status =
        mc_world_runtime_get_detailed(world, x, z, &state, &e->acquisition);
    if (status != MC_CHUNK_ACQUIRE_OK) {
        e->storage = e->acquisition.cause;
        return fail(e,
                    status == MC_CHUNK_ACQUIRE_LIMIT || status == MC_CHUNK_ACQUIRE_PARTIAL_LIMIT
                        ? MC_CLIENT_CHUNK_CACHE_LIMIT
                        : MC_CLIENT_CHUNK_CACHE_CHILD_FAILED,
                    e->storage.operation == NULL ? "ew actual World Chunk getter"
                                                 : e->storage.operation,
                    112, true);
    }
    e->child_bytecode = 0;
    /* Source may store NULL. A nonnull normal identity needs the same Store
     * lease before publication; actual blank fallback lives with held World. */
    if (state == NULL || state->blank) {
        cell->state = state;
        return true;
    }
    mc_world_store *store = world->fields.w;
    mc_world_chunk *object = mc_world_store_chunk_object(store, state);
    if (object == NULL)
        return fail(e, MC_CLIENT_CHUNK_CACHE_CHILD_FAILED,
                    "ew captured Chunk has no canonical object", 112, true);
    if (!storage(e, mc_world_store_lease_chunk(store, state, &e->storage), 112))
        return false;
    /* Accessors also use this occurrence ledger. Publish it only when the
     * source cell can be stored with its complete native lifetime right;
     * a failed getter/binding/lease leaves the retained source NULL prefix. */
    *cell = (mc_client_chunk_cache_occurrence){
        .state = state, .object = object, .store = store, .leased = true};
    ++c->state.leased;
    return true;
}

mc_client_chunk_cache_status mc_client_chunk_cache_create(mc_world_runtime *world, int32_t x0,
                                                          int32_t y0, int32_t z0, int32_t x1,
                                                          int32_t y1, int32_t z1,
                                                          mc_client_chunk_cache **out,
                                                          mc_client_chunk_cache_error *error) {
    (void)y0;
    (void)y1;
    mc_client_chunk_cache_error e = empty_error("<init>", "(Lfd;IIIIII)V");
    if (out == NULL || *out != NULL) {
        fail(&e, MC_CLIENT_CHUNK_CACHE_INVALID_ARGUMENT, "ew native output slot", 0, true);
        return copy_error(error, &e);
    }
    mc_client_chunk_cache *c = calloc(1, sizeof(*c));
    if (c == NULL) {
        fail(&e, MC_CLIENT_CHUNK_CACHE_ALLOCATION_FAILED, "ew native owner allocation", 0, true);
        return copy_error(error, &e);
    }
    c->failure = e;
    c->state.failure = &c->failure;
    c->state.cleanup = &c->cleanup;
    c->state.active = true;
    *out = c;
    if (world != NULL && !mc_world_runtime_binding_retain(world)) {
        c->failure.storage = *mc_world_runtime_last_error(world);
        fail(&c->failure, MC_CLIENT_CHUNK_CACHE_CHILD_FAILED, "ew native World occurrence", 0,
             true);
        goto done;
    }
    c->world_held = world != NULL;
    c->state.d = world;
    c->state.last_store_pc = 6;
    c->state.a = mc_java_i32_shr(x0, 4);
    c->state.last_store_pc = 13;
    c->state.b = mc_java_i32_shr(z0, 4);
    c->state.last_store_pc = 21;
    const int32_t end_x = mc_java_i32_shr(x1, 4), end_z = mc_java_i32_shr(z1, 4);
    c->state.rows = mc_java_i32_add(mc_java_i32_sub(end_x, c->state.a), 1);
    c->state.columns = mc_java_i32_add(mc_java_i32_sub(end_z, c->state.b), 1);
    if (c->state.rows < 0 || c->state.columns < 0) {
        fail(&c->failure, MC_CLIENT_CHUNK_CACHE_SOURCE_NEGATIVE_ARRAY,
             "actual ew negative rectangular array dimension", 55, false);
        goto done;
    }
    const size_t rows = (size_t)c->state.rows, columns = (size_t)c->state.columns;
    if (rows > 1048576u || columns > 1048576u || (columns != 0 && rows > 1048576u / columns)) {
        fail(&c->failure, MC_CLIENT_CHUNK_CACHE_LIMIT,
             "ew native rectangular array allocation bound", 55, true);
        goto done;
    }
    c->cell_count = rows * columns;
    /* Native coalesced backing and row handles reproduce distinct source
     * arrays/NULL cells, without claiming Java per-object allocation timing. */
    c->rows = calloc(rows == 0 ? 1 : rows, sizeof(*c->rows));
    c->cells =
        calloc(c->cell_count == 0 ? (rows == 0 ? 1 : rows) : c->cell_count, sizeof(*c->cells));
    c->occurrences = calloc(c->cell_count == 0 ? 1 : c->cell_count, sizeof(*c->occurrences));
    if (c->rows == NULL || c->cells == NULL || c->occurrences == NULL) {
        fail(&c->failure, MC_CLIENT_CHUNK_CACHE_ALLOCATION_FAILED,
             "ew native rectangular array allocation", 55, true);
        goto done;
    }
    for (size_t x = 0; x < rows; ++x)
        c->rows[x] = c->cells + x * (columns == 0 ? 1 : columns);
    c->state.c = (mc_chunk_state *const *const *)c->rows;
    c->state.last_store_pc = 59;
    for (size_t x = 0; x < rows; ++x)
        for (size_t z = 0; z < columns; ++z) {
            const size_t index = x * columns + z;
            if (!acquire(c, mc_java_i32_add(c->state.a, (int32_t)x),
                         mc_java_i32_add(c->state.b, (int32_t)z), &c->occurrences[index]))
                goto done;
            c->rows[x][z] = c->occurrences[index].state;
            ++c->state.acquired;
            c->state.last_store_pc = 115;
        }
    c->state.returned = true;
    c->state.source_pc = 128;
done:
    if (!c->state.returned)
        c->state.source_pc = c->failure.bytecode;
    c->state.active = false;
    return copy_error(error, &c->failure);
}

bool mc_client_chunk_cache_inspect(const mc_client_chunk_cache *c,
                                   mc_client_chunk_cache_state *out) {
    if (c == NULL || out == NULL || c->state.active)
        return false;
    *out = c->state;
    return true;
}

mc_client_chunk_cache_status mc_client_chunk_cache_retire(mc_client_chunk_cache **slot,
                                                          mc_client_chunk_cache_error *error) {
    mc_client_chunk_cache_error e = empty_error(NULL, NULL);
    if (slot == NULL) {
        fail(&e, MC_CLIENT_CHUNK_CACHE_INVALID_ARGUMENT, "ew native retirement slot", 0, true);
        return copy_error(error, &e);
    }
    mc_client_chunk_cache *c = *slot;
    if (c == NULL)
        return copy_error(error, &e);
    if (c->state.active) {
        fail(&e, MC_CLIENT_CHUNK_CACHE_BUSY, "ew native active retirement", 0, true);
        return copy_error(error, &e);
    }
    c->state.retiring = true;
    c->state.c = NULL; /* End borrowed source views before any supplying lease. */
    for (size_t i = 0; c->occurrences != NULL && i < c->cell_count; ++i) {
        mc_client_chunk_cache_occurrence *cell = &c->occurrences[i];
        if (!cell->leased)
            continue;
        if (!storage(&e, mc_world_store_release_chunk(cell->store, cell->state, &e.storage), 0)) {
            c->cleanup = e;
            return copy_error(error, &e);
        }
        cell->leased = false;
        --c->state.leased;
    }
    free(c->occurrences);
    free(c->cells);
    free(c->rows);
    if (c->world_held)
        mc_world_runtime_binding_release(c->state.d);
    free(c);
    *slot = NULL;
    return copy_error(error, &e);
}

static bool enter(mc_client_chunk_cache *c, bool output, mc_client_chunk_cache_error *e) {
    if (c == NULL || !output)
        return fail(e, MC_CLIENT_CHUNK_CACHE_INVALID_ARGUMENT, "ew native accessor arguments", 0,
                    true);
    if (c->state.active || c->state.retiring)
        return fail(e, MC_CLIENT_CHUNK_CACHE_BUSY, "ew native active/retiring accessor", 0, true);
    c->state.active = true;
    return true;
}

static mc_client_chunk_cache_status finish(mc_client_chunk_cache *c, mc_client_chunk_cache_error *e,
                                           mc_client_chunk_cache_error *out) {
    c->state.active = false;
    return copy_error(out, e);
}

/* Check only when the actual source reaches the corresponding array load. */
static mc_client_chunk_cache_occurrence *cell_at(mc_client_chunk_cache *c, int32_t x, int32_t z,
                                                 unsigned int outer_pc, unsigned int inner_pc,
                                                 mc_client_chunk_cache_error *e) {
    const int32_t row = mc_java_i32_sub(mc_java_i32_shr(x, 4), c->state.a);
    const int32_t column = mc_java_i32_sub(mc_java_i32_shr(z, 4), c->state.b);
    if (c->state.c == NULL) {
        fail(e, MC_CLIENT_CHUNK_CACHE_SOURCE_NULL, "actual ew null outer array", outer_pc, false);
        return NULL;
    }
    if (row < 0 || row >= c->state.rows) {
        fail(e, MC_CLIENT_CHUNK_CACHE_SOURCE_INDEX, "actual ew outer array index", outer_pc, false);
        return NULL;
    }
    if (column < 0 || column >= c->state.columns) {
        fail(e, MC_CLIENT_CHUNK_CACHE_SOURCE_INDEX, "actual ew inner array index", inner_pc, false);
        return NULL;
    }
    return &c->occurrences[(size_t)row * (size_t)c->state.columns + (size_t)column];
}

static bool chunk_required(mc_client_chunk_cache_occurrence *cell, unsigned int pc,
                           mc_client_chunk_cache_error *e) {
    return cell != NULL &&
           (cell->state != NULL ||
            fail(e, MC_CLIENT_CHUNK_CACHE_SOURCE_NULL, "actual ew null Chunk receiver", pc, false));
}

static bool nibble(const mc_world_chunk_nibble *n, int32_t x, int32_t y, int32_t z, int32_t *out,
                   unsigned int child_pc, mc_client_chunk_cache_error *e) {
    e->child_bytecode = child_pc;
    if (n == NULL || n->a == NULL)
        return fail(e, MC_CLIENT_CHUNK_CACHE_SOURCE_NULL, "actual Chunk null Nibble backing",
                    e->bytecode, false);
    const int32_t cell = mc_java_i32_shl(x, 11) | mc_java_i32_shl(z, 7) | y;
    const int32_t index = mc_java_i32_shr(cell, 1);
    if (index < 0 || (size_t)index >= n->length)
        return fail(e, MC_CLIENT_CHUNK_CACHE_SOURCE_INDEX, "actual Chunk Nibble index", e->bytecode,
                    false);
    *out = (int32_t)(((uint32_t)n->a[index] >> (((uint32_t)y & 1u) * 4u)) & 15u);
    e->child_bytecode = 0;
    return true;
}

static bool block_value(mc_client_chunk_cache *c, int32_t x, int32_t y, int32_t z, int32_t *out,
                        mc_client_chunk_cache_error *e) {
    e->method = "a";
    e->descriptor = "(III)I";
    if (y < 0 || y >= 128) {
        *out = 0;
        return true;
    }
    const int32_t row = mc_java_i32_sub(mc_java_i32_shr(x, 4), c->state.a);
    const int32_t col = mc_java_i32_sub(mc_java_i32_shr(z, 4), c->state.b);
    if (row < 0) {
        *out = 0;
        return true;
    }
    if (c->state.c == NULL)
        return fail(e, MC_CLIENT_CHUNK_CACHE_SOURCE_NULL, "actual ew block null outer array", 46,
                    false);
    if (row >= c->state.rows || col < 0 || col >= c->state.columns) {
        *out = 0;
        return true;
    }
    mc_client_chunk_cache_occurrence *cell =
        &c->occurrences[(size_t)row * (size_t)c->state.columns + (size_t)col];
    if (cell->state == NULL || cell->state->blank) {
        *out = 0;
        return true;
    }
    e->bytecode = 100;
    e->child_bytecode = 15;
    const mc_world_chunk *chunk = cell->object;
    const size_t index = ((uint32_t)x & 15u) * 2048u + ((uint32_t)z & 15u) * 128u + (uint32_t)y;
    if (chunk->data == NULL || chunk->data->blocks == NULL)
        return fail(e, MC_CLIENT_CHUNK_CACHE_SOURCE_NULL, "actual Chunk null block array", 100,
                    false);
    if (index >= chunk->data->blocks_capacity)
        return fail(e, MC_CLIENT_CHUNK_CACHE_SOURCE_INDEX, "actual Chunk block index", 100, false);
    *out = chunk->data->blocks[index];
    e->child_bytecode = 0;
    return true;
}

static bool block_child(mc_client_chunk_cache *c, int32_t x, int32_t y, int32_t z, int32_t *out,
                        const char *method, const char *descriptor, unsigned int pc,
                        mc_client_chunk_cache_error *e) {
    if (!block_value(c, x, y, z, out, e)) {
        wrap(e, method, descriptor, pc);
        return false;
    }
    e->method = method;
    e->descriptor = descriptor;
    e->bytecode = pc;
    return true;
}

mc_client_chunk_cache_status mc_client_chunk_cache_block(mc_client_chunk_cache *c, int32_t x,
                                                         int32_t y, int32_t z, int32_t *out,
                                                         mc_client_chunk_cache_error *error) {
    mc_client_chunk_cache_error e = empty_error("a", "(III)I");
    if (!enter(c, out != NULL, &e))
        return copy_error(error, &e);
    int32_t value;
    if (block_value(c, x, y, z, &value, &e))
        *out = value;
    return finish(c, &e, error);
}

mc_client_chunk_cache_status mc_client_chunk_cache_metadata(mc_client_chunk_cache *c, int32_t x,
                                                            int32_t y, int32_t z, int32_t *out,
                                                            mc_client_chunk_cache_error *error) {
    mc_client_chunk_cache_error e = empty_error("e", "(III)I");
    if (!enter(c, out != NULL, &e))
        return copy_error(error, &e);
    if (y < 0 || y >= 128)
        *out = 0;
    else {
        mc_client_chunk_cache_occurrence *cell = cell_at(c, x, z, 41, 44, &e);
        if (chunk_required(cell, 54, &e)) {
            int32_t value = 0;
            e.bytecode = 54;
            if (cell->state->blank || nibble(cell->object->e, x & 15, y, z & 15, &value, 10, &e))
                *out = value;
        }
    }
    return finish(c, &e, error);
}

mc_client_chunk_cache_status mc_client_chunk_cache_tile_entity(mc_client_chunk_cache *c, int32_t x,
                                                               int32_t y, int32_t z,
                                                               mc_tile_entity **out,
                                                               mc_client_chunk_cache_error *error) {
    mc_client_chunk_cache_error e = empty_error("b", "(III)Low;");
    if (!enter(c, out != NULL, &e))
        return copy_error(error, &e);
    mc_client_chunk_cache_occurrence *cell = cell_at(c, x, z, 26, 29, &e);
    if (chunk_required(cell, 39, &e)) {
        if (cell->state->blank)
            *out = NULL;
        else if (mc_live_tile_entities_chunk_get(cell->object, x & 15, y, z & 15, out,
                                                 &e.tile_entity) != MC_WORLD_STORAGE_OK) {
            e.child_bytecode = e.tile_entity.chunk_bytecode;
            mc_client_chunk_cache_status cause = MC_CLIENT_CHUNK_CACHE_CHILD_FAILED;
            /* The shared held-Chunk body has already performed Map.get before
             * its actual lm.a block lookup. Classify only that reached array
             * failure, never an unconditional Y preflight of lm.d. */
            if (e.tile_entity.chunk_bytecode == 37 && e.tile_entity.operation != NULL &&
                strcmp(e.tile_entity.operation, "TileEntity Chunk block element") == 0) {
                const int32_t index = mc_java_i32_shl(x & 15, 11) | mc_java_i32_shl(z & 15, 7) | y;
                if (index < 0 || (uint32_t)index >= MC_CHUNK_BLOCKS)
                    cause = MC_CLIENT_CHUNK_CACHE_SOURCE_INDEX;
            }
            fail(&e, cause, e.tile_entity.operation, 39,
                 cause == MC_CLIENT_CHUNK_CACHE_CHILD_FAILED);
        }
    }
    return finish(c, &e, error);
}

static mc_registry_classes *registry(mc_client_chunk_cache *c) {
    return mc_world_runtime_registry(c->state.d);
}

static bool registry_result(mc_client_chunk_cache_error *e, mc_registry_status status,
                            unsigned int pc) {
    return status == MC_REGISTRY_OK ||
           fail(e,
                status == MC_REGISTRY_NULL_REFERENCE && e->registry.initializer_pc < 0
                    ? MC_CLIENT_CHUNK_CACHE_SOURCE_NULL
                : status == MC_REGISTRY_INDEX_OUT_OF_RANGE && e->registry.initializer_pc < 0
                    ? MC_CLIENT_CHUNK_CACHE_SOURCE_INDEX
                    : MC_CLIENT_CHUNK_CACHE_CHILD_FAILED,
                e->registry.message == NULL ? "ew canonical Registry binding" : e->registry.message,
                pc,
                !(e->registry.initializer_pc < 0 && (status == MC_REGISTRY_NULL_REFERENCE ||
                                                     status == MC_REGISTRY_INDEX_OUT_OF_RANGE)));
}

static bool light_static(mc_client_chunk_cache *c, const char *name, unsigned int load_pc,
                         unsigned int field_pc, int32_t *id, mc_client_chunk_cache_error *e) {
    const mc_block *block = NULL;
    if (!registry_result(e, mc_registry_block_static_get(registry(c), name, &block, &e->registry),
                         load_pc))
        return false;
    const mc_block_fields *f = mc_block_fields_get(block);
    if (f == NULL)
        return fail(e, MC_CLIENT_CHUNK_CACHE_SOURCE_NULL, "actual ew light static Block receiver",
                    field_pc, false);
    *id = f->bn;
    return true;
}

static bool light_value(mc_client_chunk_cache *c, int32_t x, int32_t y, int32_t z, bool neighbors,
                        int32_t *out, mc_client_chunk_cache_error *e) {
    e->method = "a";
    e->descriptor = "(IIIZ)I";
    if (x < -32000000 || z < -32000000 || x >= 32000000 || z > 32000000) {
        *out = 15;
        return true;
    }
    if (neighbors) {
        int32_t block;
        if (!block_child(c, x, y, z, &block, "a", "(IIIZ)I", 36, e))
            return false;
        static const char *const names[] = {"al", "aB", "au", "aI"};
        static const unsigned int load[] = {43, 54, 65, 76}, field[] = {46, 57, 68, 79};
        bool sample = false;
        for (size_t i = 0; i < 4; ++i) {
            int32_t id;
            if (!light_static(c, names[i], load[i], field[i], &id, e))
                return false;
            if (id == block) {
                sample = true;
                break;
            }
        }
        if (sample) {
            const int32_t xyz[5][3] = {{x, mc_java_i32_add(y, 1), z},
                                       {mc_java_i32_add(x, 1), y, z},
                                       {mc_java_i32_sub(x, 1), y, z},
                                       {x, y, mc_java_i32_add(z, 1)},
                                       {x, y, mc_java_i32_sub(z, 1)}};
            int32_t maximum = 0;
            static const unsigned int call_pc[5] = {92, 107, 119, 131, 143};
            for (size_t i = 0; i < 5; ++i) {
                int32_t value;
                if (!light_value(c, xyz[i][0], xyz[i][1], xyz[i][2], false, &value, e)) {
                    wrap(e, "a", "(IIIZ)I", call_pc[i]);
                    return false;
                }
                if (i == 0 || value > maximum)
                    maximum = value;
            }
            *out = maximum;
            return true;
        }
    }
    if (y < 0) {
        *out = 0;
        return true;
    }
    if (y >= 128) {
        if (c->state.d == NULL)
            return fail(e, MC_CLIENT_CHUNK_CACHE_SOURCE_NULL, "actual ew light null World receiver",
                        211, false);
        const int32_t value = mc_java_i32_sub(15, c->state.d->fields.f);
        *out = value < 0 ? 0 : value;
        return true;
    }
    mc_client_chunk_cache_occurrence *cell = cell_at(c, x, z, 254, 257, e);
    if (cell == NULL)
        return false;
    /* Source loads ew.d.f as the last argument before invoking the previously
     * captured (possibly NULL) Chunk receiver. */
    if (c->state.d == NULL)
        return fail(e, MC_CLIENT_CHUNK_CACHE_SOURCE_NULL, "actual ew light null World receiver",
                    271, false);
    const int32_t subtraction = c->state.d->fields.f;
    if (!chunk_required(cell, 274, e))
        return false;
    if (cell->state->blank) {
        *out = 0;
        return true;
    }
    int32_t sky, block;
    e->bytecode = 274;
    if (!nibble(cell->object->f, x & 15, y, z & 15, &sky, 7, e))
        return false;
    if (sky > 0)
        c->state.d->classes->chunk_a = true;
    sky = mc_java_i32_sub(sky, subtraction);
    if (!nibble(cell->object->g, x & 15, y, z & 15, &block, 35, e))
        return false;
    *out = sky > block ? sky : block;
    return true;
}

mc_client_chunk_cache_status mc_client_chunk_cache_light(mc_client_chunk_cache *c, int32_t x,
                                                         int32_t y, int32_t z, bool neighbors,
                                                         int32_t *out,
                                                         mc_client_chunk_cache_error *error) {
    mc_client_chunk_cache_error e = empty_error("a", "(IIIZ)I");
    if (!enter(c, out != NULL, &e))
        return copy_error(error, &e);
    int32_t value;
    if (light_value(c, x, y, z, neighbors, &value, &e))
        *out = value;
    return finish(c, &e, error);
}

static bool table_capture(mc_client_chunk_cache *c, unsigned int world_pc,
                          unsigned int dimension_pc, mc_world_provider **dimension,
                          const float **table, mc_client_chunk_cache_error *e) {
    if (c->state.d == NULL)
        return fail(e, MC_CLIENT_CHUNK_CACHE_SOURCE_NULL, "actual ew brightness null World",
                    world_pc, false);
    *dimension = c->state.d->fields.t;
    if (*dimension == NULL)
        return fail(e, MC_CLIENT_CHUNK_CACHE_SOURCE_NULL, "actual ew brightness null Dimension",
                    dimension_pc, false);
    if (mc_world_provider_retain(*dimension) != MC_WORLD_PROVIDER_OK) {
        *dimension = NULL;
        return fail(e, MC_CLIENT_CHUNK_CACHE_BUSY, "ew brightness Dimension native hold", 0, true);
    }
    mc_world_provider_fields fields;
    if (mc_world_provider_fields_get(*dimension, &fields) != MC_WORLD_PROVIDER_OK) {
        mc_world_provider_release(*dimension);
        *dimension = NULL;
        return fail(e, MC_CLIENT_CHUNK_CACHE_CHILD_FAILED, "ew brightness canonical Dimension", 0,
                    true);
    }
    *table = fields.f;
    return true;
}

static bool table_read(const float *table, int32_t level, unsigned int pc, float *out,
                       mc_client_chunk_cache_error *e) {
    if (table == NULL)
        return fail(e, MC_CLIENT_CHUNK_CACHE_SOURCE_NULL, "actual ew brightness null array", pc,
                    false);
    if (level < 0 || level >= 16)
        return fail(e, MC_CLIENT_CHUNK_CACHE_SOURCE_INDEX, "actual ew brightness array index", pc,
                    false);
    memcpy(out, table + level, sizeof(*out));
    return true;
}

static mc_client_chunk_cache_status brightness(mc_client_chunk_cache *c, int32_t x, int32_t y,
                                               int32_t z, bool minimum, int32_t lower, float *out,
                                               mc_client_chunk_cache_error *error) {
    mc_client_chunk_cache_error e =
        empty_error(minimum ? "a" : "c", minimum ? "(IIII)F" : "(III)F");
    if (!enter(c, out != NULL, &e))
        return copy_error(error, &e);
    mc_world_provider *dimension = NULL;
    const float *table = NULL;
    int32_t light = 0;
    /* c captures d.t.f BEFORE d(III); a(IIII) performs d(III) first. */
    bool ok = minimum || table_capture(c, 4, 7, &dimension, &table, &e);
    if (ok) {
        ok = light_value(c, x, y, z, true, &light, &e);
        if (!ok) {
            wrap(&e, "d", "(III)I", 5);
            wrap(&e, minimum ? "a" : "c", minimum ? "(IIII)F" : "(III)F", minimum ? 4u : 14u);
        } else {
            e.method = minimum ? "a" : "c";
            e.descriptor = minimum ? "(IIII)F" : "(III)F";
        }
    }
    if (ok && minimum) {
        if (light < lower)
            light = lower;
        ok = table_capture(c, 24, 27, &dimension, &table, &e);
    }
    if (ok)
        (void)table_read(table, light, minimum ? 32u : 17u, out, &e);
    if (dimension != NULL)
        mc_world_provider_release(dimension);
    return finish(c, &e, error);
}

mc_client_chunk_cache_status mc_client_chunk_cache_brightness(mc_client_chunk_cache *c, int32_t x,
                                                              int32_t y, int32_t z, float *out,
                                                              mc_client_chunk_cache_error *error) {
    return brightness(c, x, y, z, false, 0, out, error);
}

mc_client_chunk_cache_status
mc_client_chunk_cache_min_brightness(mc_client_chunk_cache *c, int32_t x, int32_t y, int32_t z,
                                     int32_t minimum, float *out,
                                     mc_client_chunk_cache_error *error) {
    return brightness(c, x, y, z, true, minimum, out, error);
}

mc_client_chunk_cache_status mc_client_chunk_cache_material(mc_client_chunk_cache *c, int32_t x,
                                                            int32_t y, int32_t z, mc_material **out,
                                                            mc_client_chunk_cache_error *error) {
    mc_client_chunk_cache_error e = empty_error("f", "(III)Lln;");
    if (!enter(c, out != NULL, &e))
        return copy_error(error, &e);
    int32_t id;
    if (block_child(c, x, y, z, &id, "f", "(III)Lln;", 4, &e)) {
        if (id == 0) {
            (void)registry_result(
                &e, mc_registry_material_get(registry(c), MC_MATERIAL_a, out, &e.registry), 14);
        } else {
            const mc_block *block = NULL;
            if (registry_result(&e, mc_registry_block_get(registry(c), id, &block, &e.registry),
                                18)) {
                const mc_block_fields *f = mc_block_fields_get(block);
                if (f == NULL)
                    fail(&e, MC_CLIENT_CHUNK_CACHE_SOURCE_NULL,
                         "actual ew material null Block receiver", 24, false);
                else
                    *out = f->bA;
            }
        }
    }
    return finish(c, &e, error);
}

static bool normal_block(const mc_block *block) {
    /* Complete client uu.d()Z dispatch: constant-false declaring bodies and
     * their descendants, exact ys.b, otherwise inherited uu literal true. */
    switch (block->kind) {
        case MC_BLOCK_SLAB:
            return block->subtype.ys.b;
        case MC_BLOCK_SAPLING:
        case MC_BLOCK_FLOWING_FLUID:
        case MC_BLOCK_STILL_FLUID:
        case MC_BLOCK_BED:
        case MC_BLOCK_RAIL:
        case MC_BLOCK_DETECTOR_RAIL:
        case MC_BLOCK_PISTON_BASE:
        case MC_BLOCK_COBWEB:
        case MC_BLOCK_TALL_GRASS:
        case MC_BLOCK_DEAD_BUSH:
        case MC_BLOCK_PISTON_HEAD:
        case MC_BLOCK_MOVING_PISTON:
        case MC_BLOCK_FLOWER:
        case MC_BLOCK_MUSHROOM:
        case MC_BLOCK_TORCH:
        case MC_BLOCK_FIRE:
        case MC_BLOCK_STAIRS:
        case MC_BLOCK_REDSTONE:
        case MC_BLOCK_CROPS:
        case MC_BLOCK_FARMLAND:
        case MC_BLOCK_SIGN:
        case MC_BLOCK_DOOR:
        case MC_BLOCK_LADDER:
        case MC_BLOCK_LEVER:
        case MC_BLOCK_PRESSURE_PLATE:
        case MC_BLOCK_REDSTONE_TORCH:
        case MC_BLOCK_BUTTON:
        case MC_BLOCK_SNOW_LAYER:
        case MC_BLOCK_CACTUS:
        case MC_BLOCK_SUGAR_CANE:
        case MC_BLOCK_FENCE:
        case MC_BLOCK_PORTAL:
        case MC_BLOCK_CAKE:
        case MC_BLOCK_REPEATER:
        case MC_BLOCK_TRAPDOOR:
            return false;
        default:
            return true;
    }
}

static mc_client_chunk_cache_status predicate(mc_client_chunk_cache *c, int32_t x, int32_t y,
                                              int32_t z, bool normal, bool *out,
                                              mc_client_chunk_cache_error *error) {
    mc_client_chunk_cache_error e = empty_error(normal ? "h" : "g", "(III)Z");
    if (!enter(c, out != NULL, &e))
        return copy_error(error, &e);
    mc_registry_block_m_view array;
    if (registry_result(&e, mc_registry_block_m_get(registry(c), &array, &e.registry), 0)) {
        int32_t id;
        const mc_block *block;
        if (block_child(c, x, y, z, &id, normal ? "h" : "g", "(III)Z", 7, &e) &&
            registry_result(&e, mc_registry_block_m_at(&array, id, &block, &e.registry), 10)) {
            if (block == NULL)
                *out = false;
            else if (!normal)
                *out = mc_block_render_opaque(block);
            else {
                mc_material *material = block->fields.bA;
                if (material == NULL)
                    fail(&e, MC_CLIENT_CHUNK_CACHE_SOURCE_NULL,
                         "actual ew normal cube null Material receiver", 25, false);
                else
                    *out = mc_material_c(material) && normal_block(block);
            }
        }
    }
    return finish(c, &e, error);
}

mc_client_chunk_cache_status mc_client_chunk_cache_opaque(mc_client_chunk_cache *c, int32_t x,
                                                          int32_t y, int32_t z, bool *out,
                                                          mc_client_chunk_cache_error *error) {
    return predicate(c, x, y, z, false, out, error);
}

mc_client_chunk_cache_status mc_client_chunk_cache_normal_cube(mc_client_chunk_cache *c, int32_t x,
                                                               int32_t y, int32_t z, bool *out,
                                                               mc_client_chunk_cache_error *error) {
    return predicate(c, x, y, z, true, out, error);
}

mc_client_chunk_cache_status mc_client_chunk_cache_manager(mc_client_chunk_cache *c,
                                                           mc_world_chunk_manager **out,
                                                           mc_client_chunk_cache_error *error) {
    mc_client_chunk_cache_error e = empty_error("a", "()Lxv;");
    if (!enter(c, out != NULL, &e))
        return copy_error(error, &e);
    if (c->state.d == NULL)
        fail(&e, MC_CLIENT_CHUNK_CACHE_SOURCE_NULL, "actual ew manager null World receiver", 4,
             false);
    else {
        mc_world_provider *dimension = c->state.d->fields.t;
        if (dimension == NULL) {
            e.child_bytecode = 4;
            fail(&e, MC_CLIENT_CHUNK_CACHE_SOURCE_NULL, "actual fd.a null Dimension receiver", 4,
                 false);
        } else {
            mc_world_provider_fields fields;
            if (mc_world_provider_fields_get(dimension, &fields) != MC_WORLD_PROVIDER_OK)
                fail(&e, MC_CLIENT_CHUNK_CACHE_CHILD_FAILED, "ew canonical manager fields", 4,
                     true);
            else
                *out = fields.b;
        }
    }
    return finish(c, &e, error);
}
