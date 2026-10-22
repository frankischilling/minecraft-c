#include "blocks_internal.h"

#include "../../../block/registry/registry_internal.h"
#include "../../../block/registry/runtime_internal.h"
#include "../../chunk/object_state_internal.h"
#include "../../storage/objects_internal.h"
#include "../construction_internal.h"
#include "../provider_internal.h"
#include "../tiles/piston_math_internal.h"
#include "mc/core/java/java.h"

#include <float.h>
#include <stdint.h>
#include <string.h>

#if !defined(__SSE2__) && !defined(_M_X64) && !(defined(_M_IX86_FP) && _M_IX86_FP >= 2)
#error "Block collision arithmetic requires SSE2"
#endif
#if defined(__FAST_MATH__) || (defined(__FINITE_MATH_ONLY__) && __FINITE_MATH_ONLY__ > 0) ||       \
    (defined(_M_FP_FAST) && _M_FP_FAST)
#error "Block collision arithmetic requires strict floating transformations"
#endif
#include <emmintrin.h>

_Static_assert(FLT_RADIX == 2 && FLT_MANT_DIG == 24 && DBL_MANT_DIG == 53 && FLT_EVAL_METHOD == 0,
               "Block collision requires strict binary32/binary64 evaluation");

typedef struct block_context {
    mc_world_runtime_classes *code_classes;
    mc_world_runtime *world;
    bool legacy_world;
} block_context;

static mc_world_storage_status block_box_core(mc_world_runtime_classes *code_classes,
                                              mc_world_runtime *world, bool legacy_world,
                                              const mc_block *block, int32_t x, int32_t y,
                                              int32_t z, mc_entity_bounds **out,
                                              mc_world_collision_error *error);
static mc_world_storage_status shifted_box_core(mc_world_runtime_classes *code_classes,
                                                mc_world_runtime *world, bool legacy_world,
                                                const mc_block *moving, int32_t x, int32_t y,
                                                int32_t z, int32_t carried_id, float progress,
                                                int32_t facing, mc_entity_bounds **out,
                                                mc_world_collision_error *error);
static mc_world_storage_status state_shape_core(mc_world_runtime_classes *code_classes,
                                                mc_world_runtime *world, bool legacy_world,
                                                const mc_block *block, int32_t x, int32_t y,
                                                int32_t z, mc_world_collision_error *error);

static mc_world_storage_status fail(mc_world_collision_error *error, mc_world_storage_status status,
                                    const char *operation) {
    error->nearby.query.status = status;
    error->nearby.query.operation = operation;
    error->nearby.query.cause = (mc_world_storage_error){status, operation, 0};
    return status;
}

static mc_world_storage_status cause(mc_world_collision_error *error,
                                     mc_world_storage_error child) {
    error->nearby.query.status = child.status;
    error->nearby.query.operation = child.operation;
    error->nearby.query.cause = child;
    return child.status;
}

static void stage(const block_context *context, mc_world_collision_error *error, const char *client,
                  const char *server, unsigned int pc) {
    error->block_class = context->code_classes->side == MC_WORLD_INFO_CLIENT ? client : server;
    error->block_bytecode = pc;
}

static bool floating(const block_context *context, mc_world_collision_error *error,
                     const char *client, const char *server, unsigned int pc) {
    stage(context, error, client, server, pc);
    if ((_mm_getcsr() & 0xffc0u) == 0x1f80u)
        return true;
    fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Block collision FP admission");
    return false;
}

static mc_world_storage_status bounds_status(mc_entity_status status) {
    switch (status) {
        case MC_ENTITY_LIMIT:
            return MC_WORLD_STORAGE_LIMIT;
        case MC_ENTITY_ALLOCATION_FAILED:
            return MC_WORLD_STORAGE_ALLOCATION_FAILED;
        case MC_ENTITY_BUSY:
            return MC_WORLD_STORAGE_BUSY;
        case MC_ENTITY_INVALID_ARGUMENT:
            return MC_WORLD_STORAGE_INVALID_ARGUMENT;
        default:
            return MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    }
}

/* A separate per-attempt record prevents a later getter from inheriting an
 * unrelated World diagnostic, including during synchronous acquisition. */
static bool acquire(mc_world_runtime *world, int32_t x, int32_t z, mc_chunk_state **out,
                    mc_world_collision_error *error) {
    mc_world_nearby_error *lower = &error->nearby;
    lower->query.provider_bytecode = 6;
    if (world->fields.v == NULL) {
        fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Collision provider receiver");
        return false;
    }
    mc_world_provider_fields provider;
    mc_world_storage_error native = {0};
    if (mc_world_provider_bind_fields(world->fields.v, &provider, &native) != MC_WORLD_STORAGE_OK) {
        cause(error, native);
        return false;
    }
    if (provider.world != world || provider.cache != world->acquisition.cache ||
        provider.pending != world->acquisition.pending || provider.store != world->options.store) {
        fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Collision replacement provider binding");
        return false;
    }
    mc_world_runtime_acquisition_error child;
    lower->query.acquisition = mc_world_runtime_get_detailed(world, mc_java_i32_shr(x, 4),
                                                             mc_java_i32_shr(z, 4), out, &child);
    if (lower->query.acquisition != MC_CHUNK_ACQUIRE_OK) {
        lower->query.registry = child.registry;
        lower->query.population = child.population;
        lower->tile = child.tile;
        lower->batch = child.batch;
        lower->load_entity = child.load_entity;
        lower->activation_bytecode = child.activation_bytecode;
        lower->activation_world_bytecode = child.activation_world_bytecode;
        lower->load_bytecode = child.load_bytecode;
        lower->load_builder_bytecode = child.load_builder_bytecode;
        if (child.cause.status != MC_WORLD_STORAGE_OK)
            cause(error, child.cause);
        else
            fail(error,
                 lower->query.acquisition == MC_CHUNK_ACQUIRE_LIMIT ||
                         lower->query.acquisition == MC_CHUNK_ACQUIRE_PARTIAL_LIMIT
                     ? MC_WORLD_STORAGE_LIMIT
                     : MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                 "Collision Chunk acquisition");
        return false;
    }
    lower->query.provider_bytecode = 0;
    return true;
}

mc_world_storage_status mc_world_collision_block_value_held(mc_world_runtime *world, int32_t x,
                                                            int32_t y, int32_t z, bool metadata,
                                                            int32_t *out,
                                                            mc_world_collision_error *error) {
    mc_world_collision_error frame = error == NULL ? mc_world_collision_empty_error() : *error;
    frame.nearby = mc_world_nearby_empty_error();
    frame.x = x;
    frame.y = y;
    frame.z = z;
    mc_world_storage_status status = MC_WORLD_STORAGE_OK;
    int32_t value = 0;
    if (world == NULL || out == NULL)
        status = fail(&frame, MC_WORLD_STORAGE_INVALID_ARGUMENT, "Collision block read arguments");
    else if (world->construction.mode != MC_WORLD_RUNTIME_GAME ||
             world->construction.phase == MC_WORLD_RUNTIME_RETIRED)
        status = fail(&frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Collision block read World");
    else if (x < -32000000 || z < -32000000 || x >= 32000000 || z > 32000000 || y < 0 || y >= 128) {
        /* Actual World guards return zero before reading the provider. */
    } else {
        frame.nearby.query.bytecode = 48;
        mc_chunk_state *state = NULL;
        if (!acquire(world, x, z, &state, &frame))
            status = frame.nearby.query.status;
        else {
            frame.nearby.query.bytecode = metadata ? 68u : 60u;
            if (state == NULL)
                status = fail(&frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                              "Collision acquired Chunk receiver");
            else if (!state->blank) {
                /* Concrete-kind dispatch precedes normal header resolution. */
                mc_world_store *const store = world->fields.w;
                mc_world_chunk *const chunk = mc_world_store_chunk_object(store, state);
                if (chunk == NULL || !mc_world_chunk_source_enabled(chunk))
                    status = fail(&frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                  "Collision concrete Chunk owner");
                else {
                    mc_world_storage_error child = {0};
                    status = mc_world_store_lease_chunk(store, state, &child);
                    if (status != MC_WORLD_STORAGE_OK)
                        cause(&frame, child);
                    else {
                        const int32_t local_x = (int32_t)((uint32_t)x & 15u);
                        const int32_t local_z = (int32_t)((uint32_t)z & 15u);
                        const int32_t cell =
                            mc_java_i32_shl(local_x, 11) | mc_java_i32_shl(local_z, 7) | y;
                        frame.nearby.query.chunk_bytecode = 1;
                        if (metadata) {
                            const mc_world_chunk_nibble *const nibble = chunk->e;
                            frame.nearby.query.chunk_bytecode = 7;
                            const int32_t index = mc_java_i32_shr(cell, 1);
                            if (nibble == NULL || nibble->a == NULL || index < 0 ||
                                (size_t)index >= nibble->length)
                                status = fail(&frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                              "Collision metadata Nibble backing/index");
                            else
                                value = (int32_t)(((uint32_t)nibble->a[(size_t)index] >>
                                                   (((uint32_t)cell & 1u) * 4u)) &
                                                  15u);
                        } else if (chunk->data == NULL)
                            status = fail(&frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                          "Collision Chunk block owner");
                        else {
                            frame.nearby.query.chunk_bytecode = 15;
                            if (chunk->data->blocks == NULL || cell < 0 ||
                                (size_t)cell >= chunk->data->blocks_capacity)
                                status = fail(&frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                              "Collision Chunk block backing/index");
                            else
                                value = (int32_t)chunk->data->blocks[(size_t)cell];
                        }
                        (void)mc_world_store_release_chunk(store, state, NULL);
                    }
                }
            }
        }
    }
    if (status == MC_WORLD_STORAGE_OK) {
        *out = value;
        frame.nearby = mc_world_nearby_empty_error();
    }
    if (error != NULL)
        *error = frame;
    return status;
}

static mc_world_storage_status pooled(const block_context *context, const char *client,
                                      const char *server, unsigned int pc, double x0, double y0,
                                      double z0, double x1, double y1, double z1,
                                      mc_entity_bounds **out, mc_world_collision_error *error) {
    stage(context, error, client, server, pc);
    mc_entity_classes *const classes = context->code_classes->entities;
    if (classes == NULL)
        return fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                    "Block collision bounds class binding");
    mc_entity_bounds_error child = {0};
    const mc_entity_status status =
        mc_entity_bounds_pool_get(classes, x0, y0, z0, x1, y1, z1, out, &child);
    if (status != MC_ENTITY_OK) {
        error->bounds = child;
        return fail(error, bounds_status(status), "Block collision pooled factory");
    }
    return MC_WORLD_STORAGE_OK;
}

static void shape(const block_context *context, mc_block *block, const char *client,
                  const char *server, unsigned int pc, float x0, float y0, float z0, float x1,
                  float y1, float z1, mc_world_collision_error *error) {
    stage(context, error, client, server, pc);
    /* This setter does not alter constructor progress or active-use state. */
    mc_registry_block_bounds(block, x0, y0, z0, x1, y1, z1);
}

static mc_world_storage_status base_box(const block_context *context, const mc_block *block,
                                        int32_t x, int32_t y, int32_t z, mc_entity_bounds **out,
                                        mc_world_collision_error *error) {
    if (!floating(context, error, "uu", "na", 6))
        return error->nearby.query.status;
    const double x0 = (double)x + block->fields.bs;
    const double y0 = (double)y + block->fields.bt;
    const double z0 = (double)z + block->fields.bu;
    const double x1 = (double)x + block->fields.bv;
    const double y1 = (double)y + block->fields.bw;
    const double z1 = (double)z + block->fields.bx;
    return pooled(context, "uu", "na", 44, x0, y0, z0, x1, y1, z1, out, error);
}

static bool block_value(const block_context *context, const char *client, const char *server,
                        unsigned int pc, int32_t x, int32_t y, int32_t z, bool is_metadata,
                        int32_t *out, mc_world_collision_error *error) {
    stage(context, error, client, server, pc);
    if (context->legacy_world || context->world == NULL)
        return mc_world_collision_block_value_held(context->world, x, y, z, is_metadata, out,
                                                   error) == MC_WORLD_STORAGE_OK;

    /* The explicit-code entry cannot enter a source-unused World up front.
     * Hold this actual receiver only at its reached metadata invocation,
     * including the read after acquiring/loading/populating its Chunk. */
    mc_world_storage_error child = {0};
    if (mc_world_runtime_call_enter(context->world, &child) != MC_WORLD_STORAGE_OK) {
        error->nearby = mc_world_nearby_empty_error();
        error->x = x;
        error->y = y;
        error->z = z;
        (void)cause(error, child);
        return false;
    }
    const mc_world_storage_status status =
        mc_world_collision_block_value_held(context->world, x, y, z, is_metadata, out, error);
    mc_world_runtime_call_leave(context->world);
    return status == MC_WORLD_STORAGE_OK;
}

static bool metadata(const block_context *context, const char *client, const char *server,
                     unsigned int pc, int32_t x, int32_t y, int32_t z, int32_t *out,
                     mc_world_collision_error *error) {
    return block_value(context, client, server, pc, x, y, z, true, out, error);
}

static bool door_shape(const block_context *context, mc_block *block, int32_t x, int32_t y,
                       int32_t z, mc_world_collision_error *error) {
    int32_t value;
    if (!metadata(context, "le", "hc", 7, x, y, z, &value, error))
        return false;
    const int32_t direction = (value & 4) == 0 ? mc_java_i32_sub(value, 1) & 3 : value & 3;
    const float width = 0.1875f;
    shape(context, block, "le", "hc", 10, 0.0f, 0.0f, 0.0f, 1.0f, 2.0f, 1.0f, error);
    switch (direction) {
        case 0:
            shape(context, block, "le", "hc", 24, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, width, error);
            break;
        case 1:
            shape(context, block, "le", "hc", 41, 1.0f - width, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,
                  error);
            break;
        case 2:
            shape(context, block, "le", "hc", 58, 0.0f, 0.0f, 1.0f - width, 1.0f, 1.0f, 1.0f,
                  error);
            break;
        case 3:
            shape(context, block, "le", "hc", 73, 0.0f, 0.0f, 0.0f, width, 1.0f, 1.0f, error);
            break;
    }
    return true;
}

static bool trapdoor_shape(const block_context *context, mc_block *block, int32_t x, int32_t y,
                           int32_t z, mc_world_collision_error *error) {
    int32_t value;
    if (!metadata(context, "oq", "jd", 6, x, y, z, &value, error))
        return false;
    const float width = 0.1875f;
    shape(context, block, "oq", "jd", 10, 0.0f, 0.0f, 0.0f, 1.0f, width, 1.0f, error);
    if ((value & 4) != 0) {
        switch (value & 3) {
            case 0:
                shape(context, block, "oq", "jd", 35, 0.0f, 0.0f, 1.0f - width, 1.0f, 1.0f, 1.0f,
                      error);
                break;
            case 1:
                shape(context, block, "oq", "jd", 52, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, width, error);
                break;
            case 2:
                shape(context, block, "oq", "jd", 71, 1.0f - width, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,
                      error);
                break;
            case 3:
                shape(context, block, "oq", "jd", 88, 0.0f, 0.0f, 0.0f, width, 1.0f, 1.0f, error);
                break;
        }
    }
    return true;
}

static mc_world_storage_status piston_tile_failure(mc_world_collision_error *error,
                                                   mc_tile_status status,
                                                   const mc_tile_error *child) {
    error->nearby.tile.tile = *child;
    error->nearby.tile.tile.status = status;
    return fail(error,
                status == MC_TILE_LIMIT ? MC_WORLD_STORAGE_LIMIT
                                        : MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                child->operation == NULL ? "Moving-piston Tile getter" : child->operation);
}

static mc_world_storage_status moving_piston_tile(const block_context *context, int32_t x,
                                                  int32_t y, int32_t z, mc_tile **out,
                                                  mc_world_collision_error *error) {
    stage(context, error, "ut", "mz", 5);
    mc_tile *tile = NULL;
    mc_live_tile_error child = mc_live_tiles_empty_error();
    const mc_world_storage_status status =
        mc_live_tiles_get(context->world, x, y, z, &tile, &child);
    if (status != MC_WORLD_STORAGE_OK) {
        error->nearby.tile = child;
        error->nearby.query.registry = child.registry;
        error->nearby.query.bytecode = child.bytecode;
        error->nearby.query.provider_bytecode = child.provider_bytecode;
        error->nearby.query.chunk_bytecode = child.chunk_bytecode;
        error->nearby.batch = child.batch;
        error->nearby.load_entity = child.load_entity;
        error->nearby.activation_bytecode = child.activation_bytecode;
        error->nearby.activation_world_bytecode = child.activation_world_bytecode;
        error->nearby.load_bytecode = child.load_bytecode;
        error->nearby.load_builder_bytecode = child.load_builder_bytecode;
        if (child.cause.status != MC_WORLD_STORAGE_OK)
            return cause(error, child.cause);
        return fail(error, status, child.operation);
    }
    if (tile == NULL) {
        *out = NULL;
        return MC_WORLD_STORAGE_OK;
    }
    mc_tile_snapshot snapshot;
    const mc_tile_status inspected = mc_tile_get_snapshot(tile, &snapshot);
    stage(context, error, "ut", "mz", 19);
    if (inspected != MC_TILE_OK) {
        mc_tile_release(tile);
        error->nearby.tile.tile.status = inspected;
        return fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                    "Moving-piston Tile concrete identity");
    }
    if (snapshot.kind == MC_TILE_CHEST || snapshot.kind == MC_TILE_SPAWNER) {
        mc_tile_release(tile);
        *out = NULL; /* Actual instanceof uk/mu is false for these real types. */
        return MC_WORLD_STORAGE_OK;
    }
    if (snapshot.kind != MC_TILE_PISTON) {
        mc_tile_release(tile);
        return fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                    "Moving-piston Tile concrete identity");
    }
    *out = tile;
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status moving_piston_box(const block_context *context,
                                                 const mc_block *moving, int32_t x, int32_t y,
                                                 int32_t z, mc_entity_bounds **out,
                                                 mc_world_collision_error *error) {
    /* e invokes its actual private typed lookup6; the reached Tile query is5
     * inside that helper, before its null/instanceof tests. */
    error->piston_bytecode = 6;
    mc_tile *tile = NULL;
    const mc_world_storage_status status = moving_piston_tile(context, x, y, z, &tile, error);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    if (tile == NULL) {
        *out = NULL;
        return MC_WORLD_STORAGE_OK;
    }
    /* Retain the actual lookup result through every reached source getter.
     * The snapshot above only identifies the concrete native kind. */
    uint32_t progress_bits;
    float progress;
    bool extending;
    int32_t carried_id = 0, facing = 0;
    mc_tile_error lower = mc_tile_empty_error();
    error->piston_bytecode = 21;
    stage(context, error, "ut", "mz", 21);
    mc_tile_status tile_status = mc_tile_piston_progress(tile, 0, &progress_bits, &lower);
    if (tile_status != MC_TILE_OK) {
        mc_tile_release(tile);
        return piston_tile_failure(error, tile_status, &lower);
    }
    memcpy(&progress, &progress_bits, sizeof(progress));
    error->piston_bytecode = 28;
    stage(context, error, "ut", "mz", 28);
    tile_status = mc_tile_piston_extending(tile, &extending);
    if (tile_status == MC_TILE_OK && extending) {
        stage(context, error, "ut", "mz", 37);
        progress = mc_piston_float_subtract(1.0f, progress);
    }
    if (tile_status == MC_TILE_OK) {
        error->piston_bytecode = 48;
        stage(context, error, "ut", "mz", 48);
        tile_status = mc_tile_piston_block_id(tile, &carried_id);
    }
    if (tile_status == MC_TILE_OK) {
        error->piston_bytecode = 55;
        stage(context, error, "ut", "mz", 55);
        tile_status = mc_tile_piston_facing(tile, &facing);
    }
    mc_world_storage_status result;
    if (tile_status != MC_TILE_OK)
        result = piston_tile_failure(error, tile_status, &lower);
    else {
        error->piston_bytecode = 58;
        result = shifted_box_core(context->code_classes, context->world, context->legacy_world,
                                  moving, x, y, z, carried_id, progress, facing, out, error);
    }
    mc_tile_release(tile);
    return result;
}

static mc_world_storage_status box(const block_context *context, mc_block *block, int32_t x,
                                   int32_t y, int32_t z, mc_entity_bounds **out,
                                   mc_world_collision_error *error) {
    int32_t value;
    switch (block->kind) {
        /* Literal-null e declarations, with their real inherited dispatch. */
        case MC_BLOCK_HE_EU:
        case MC_BLOCK_RU_KZ:
        case MC_BLOCK_JB_FZ:
        case MC_BLOCK_WB_NW:
        case MC_BLOCK_TL_MD:
        case MC_BLOCK_NI_IJ:
            stage(context, error, "wb", "nw", 1);
            break;
        case MC_BLOCK_OM_JC:
        case MC_BLOCK_NX_IS:
            stage(context, error, "rp", "kv", 1);
            break;
        case MC_BLOCK_PC_JJ:
        case MC_BLOCK_PH_JN:
            stage(context, error, "pc", "jj", 1);
            break;
        case MC_BLOCK_RN_KU:
            stage(context, error, "rn", "ku", 1);
            break;
        case MC_BLOCK_VM_NP:
        case MC_BLOCK_DB_BY:
            stage(context, error, "vm", "np", 1);
            break;
        case MC_BLOCK_YQ_PT:
            stage(context, error, "yq", "pt", 1);
            break;
        case MC_BLOCK_SM_LO:
            stage(context, error, "sm", "lo", 1);
            break;
        case MC_BLOCK_UJ_MT:
            stage(context, error, "uj", "mt", 1);
            break;
        case MC_BLOCK_XR_PD:
            stage(context, error, "xr", "pd", 1);
            break;
        case MC_BLOCK_BV_BC:
            stage(context, error, "bv", "bc", 1);
            break;
        case MC_BLOCK_OI_IZ:
            stage(context, error, "oi", "iz", 1);
            break;
        case MC_BLOCK_RI_KO:
            stage(context, error, "ri", "ko", 1);
            break;
        case MC_BLOCK_AK_Z:
            stage(context, error, "ak", "z", 1);
            break;
        case MC_BLOCK_DP_CG: {
            if (!metadata(context, "dp", "cg", 5, x, y, z, &value, error))
                return error->nearby.query.status;
            const float depth = 0.125f;
            if (value == 2) {
                if (!floating(context, error, "dp", "cg", 26))
                    return error->nearby.query.status;
                shape(context, block, "dp", "cg", 30, 0.0f, 0.0f, 1.0f - depth, 1.0f, 1.0f, 1.0f,
                      error);
            }
            if (value == 3)
                shape(context, block, "dp", "cg", 47, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, depth, error);
            if (value == 4) {
                if (!floating(context, error, "dp", "cg", 60))
                    return error->nearby.query.status;
                shape(context, block, "dp", "cg", 66, 1.0f - depth, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,
                      error);
            }
            if (value == 5)
                shape(context, block, "dp", "cg", 83, 0.0f, 0.0f, 0.0f, depth, 1.0f, 1.0f, error);
            return base_box(context, block, x, y, z, out, error);
        }
        case MC_BLOCK_JR_GJ: {
            if (!metadata(context, "jr", "gj", 5, x, y, z, &value, error))
                return error->nearby.query.status;
            if ((value & 7) < 3) {
                *out = NULL;
                return MC_WORLD_STORAGE_OK;
            }
            if (!floating(context, error, "jr", "gj", 25))
                return error->nearby.query.status;
            const double x0 = (double)x + block->fields.bs;
            const double y0 = (double)y + block->fields.bt;
            const double z0 = (double)z + block->fields.bu;
            const double x1 = (double)x + block->fields.bv;
            const float y1 = (float)y + 0.5f;
            const double z1 = (double)z + block->fields.bx;
            return pooled(context, "jr", "gj", 62, x0, y0, z0, x1, (double)y1, z1, out, error);
        }
        case MC_BLOCK_JW_GN: {
            if (!floating(context, error, "jw", "gn", 12))
                return error->nearby.query.status;
            const float y1 = (float)y + 1.5f;
            return pooled(context, "jw", "gn", 22, (double)x, (double)y, (double)z,
                          (double)mc_java_i32_add(x, 1), (double)y1, (double)mc_java_i32_add(z, 1),
                          out, error);
        }
        case MC_BLOCK_LE_HC:
            if (!door_shape(context, block, x, y, z, error))
                return error->nearby.query.status;
            return base_box(context, block, x, y, z, out, error);
        case MC_BLOCK_OQ_JD:
            if (!trapdoor_shape(context, block, x, y, z, error))
                return error->nearby.query.status;
            return base_box(context, block, x, y, z, out, error);
        case MC_BLOCK_OA_IU: {
            if (!floating(context, error, "oa", "iu", 18))
                return error->nearby.query.status;
            const float y1 = (float)mc_java_i32_add(y, 1) - 0.125f;
            return pooled(context, "oa", "iu", 28, (double)x, (double)y, (double)z,
                          (double)mc_java_i32_add(x, 1), (double)y1, (double)mc_java_i32_add(z, 1),
                          out, error);
        }
        case MC_BLOCK_OR_JE: {
            if (!floating(context, error, "or", "je", 5))
                return error->nearby.query.status;
            const float edge = 0.0625f;
            const float x0 = (float)x + edge;
            const float z0 = (float)z + edge;
            const float x1 = (float)mc_java_i32_add(x, 1) - edge;
            const float y1 = (float)mc_java_i32_add(y, 1) - edge;
            const float z1 = (float)mc_java_i32_add(z, 1) - edge;
            return pooled(context, "or", "je", 44, (double)x0, (double)y, (double)z0, (double)x1,
                          (double)y1, (double)z1, out, error);
        }
        case MC_BLOCK_UM_MW: {
            if (!metadata(context, "um", "mw", 5, x, y, z, &value, error))
                return error->nearby.query.status;
            if (!floating(context, error, "um", "mw", 20))
                return error->nearby.query.status;
            const float edge = 0.0625f;
            const float size = (float)mc_java_i32_add(1, mc_java_i32_mul(value, 2)) / 16.0f;
            const float x0 = (float)x + size;
            const float z0 = (float)z + edge;
            const float x1 = (float)mc_java_i32_add(x, 1) - edge;
            float y1 = (float)y + 0.5f;
            y1 = y1 - edge;
            const float z1 = (float)mc_java_i32_add(z, 1) - edge;
            return pooled(context, "um", "mw", 71, (double)x0, (double)y, (double)z0, (double)x1,
                          (double)y1, (double)z1, out, error);
        }
        case MC_BLOCK_UT_MZ:
            return moving_piston_box(context, block, x, y, z, out, error);
        case MC_BLOCK_VL_NO:
            return pooled(context, "vl", "no", 26, (double)x, (double)y, (double)z,
                          (double)mc_java_i32_add(x, 1), (double)mc_java_i32_add(y, 1),
                          (double)mc_java_i32_add(z, 1), out, error);
        case MC_BLOCK_UU_NA:
        case MC_BLOCK_EO_CX:
        case MC_BLOCK_WP_OJ:
        case MC_BLOCK_OT_JF:
        case MC_BLOCK_GK_EI:
        case MC_BLOCK_NE_IG:
        case MC_BLOCK_MT_IC:
        case MC_BLOCK_VG_NK:
        case MC_BLOCK_BK_AR:
        case MC_BLOCK_XF_OV:
        case MC_BLOCK_FK_DO:
        case MC_BLOCK_XQ_PA:
        case MC_BLOCK_RD_KL:
        case MC_BLOCK_PT_JR:
        case MC_BLOCK_VE_NJ:
        case MC_BLOCK_JQ_GI:
        case MC_BLOCK_H_G:
        case MC_BLOCK_EE_CQ:
        case MC_BLOCK_L_J:
        case MC_BLOCK_YS_PW:
        case MC_BLOCK_AH_T:
        case MC_BLOCK_HB_ER:
        case MC_BLOCK_FB_DH:
        case MC_BLOCK_DD_BZ:
        case MC_BLOCK_SS_LU:
        case MC_BLOCK_E_E:
        case MC_BLOCK_FI_DN:
        case MC_BLOCK_TC_LZ:
        case MC_BLOCK_BS_AZ:
        case MC_BLOCK_NK_IK:
        case MC_BLOCK_AC_P:
        case MC_BLOCK_RZ_LF:
        case MC_BLOCK_FO_DT:
        case MC_BLOCK_FC_DI:
        case MC_BLOCK_YI_PO:
        case MC_BLOCK_LY_HR:
        case MC_BLOCK_WO_OI:
        case MC_BLOCK_KS_GV:
            return base_box(context, block, x, y, z, out, error);
        case MC_BLOCK_KIND_COUNT:
        default:
            return fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Collision Block concrete kind");
    }
    *out = NULL;
    return MC_WORLD_STORAGE_OK;
}

static bool block_admit(block_context *context, const mc_block *block,
                        mc_world_collision_error *error) {
    if (context->legacy_world) {
        if (context->world == NULL) {
            fail(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "Collision Block World");
            return false;
        }
        context->code_classes = context->world->classes;
    }
    if (block == NULL || context->code_classes == NULL ||
        block->classes != context->code_classes->registry ||
        (unsigned int)block->kind >= (unsigned int)MC_BLOCK_KIND_COUNT) {
        fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Collision Block receiver/owner");
        return false;
    }
    return true;
}

static mc_world_storage_status block_box_core(mc_world_runtime_classes *code_classes,
                                              mc_world_runtime *world, bool legacy_world,
                                              const mc_block *block, int32_t x, int32_t y,
                                              int32_t z, mc_entity_bounds **out,
                                              mc_world_collision_error *error) {
    block_context frame_context = {code_classes, world, legacy_world};
    block_context *const context = &frame_context;
    mc_world_collision_error frame = error == NULL ? mc_world_collision_empty_error() : *error;
    frame.nearby = mc_world_nearby_empty_error();
    frame.bounds = (mc_entity_bounds_error){0};
    frame.block_class = NULL;
    frame.block_bytecode = 0;
    frame.x = x;
    frame.y = y;
    frame.z = z;
    mc_world_storage_status status;
    if (out == NULL)
        status = fail(&frame, MC_WORLD_STORAGE_INVALID_ARGUMENT, "Collision Block box output");
    else if (!block_admit(context, block, &frame))
        status = frame.nearby.query.status;
    else
        status = box(context, (mc_block *)block, x, y, z, out, &frame);
    if (error != NULL)
        *error = frame;
    return status;
}

mc_world_storage_status mc_world_collision_block_box_held(mc_world_runtime *world,
                                                          const mc_block *block, int32_t x,
                                                          int32_t y, int32_t z,
                                                          mc_entity_bounds **out,
                                                          mc_world_collision_error *error) {
    return block_box_core(NULL, world, true, block, x, y, z, out, error);
}

mc_world_storage_status mc_world_collision_block_box_in_classes_held(
    mc_world_runtime_classes *code_classes, mc_world_runtime *world, const mc_block *block,
    int32_t x, int32_t y, int32_t z, mc_entity_bounds **out, mc_world_collision_error *error) {
    return block_box_core(code_classes, world, false, block, x, y, z, out, error);
}

static bool piston_registry_result(const block_context *context, mc_world_collision_error *error,
                                   mc_registry_status status, const mc_registry_error *child) {
    if (status == MC_REGISTRY_OK)
        return true;
    error->nearby.query.registry = *child;
    if (context->legacy_world)
        error->nearby.query.status = mc_world_runtime_registry_result(
            context->world, status, child, "Moving-piston geometry Registry",
            &error->nearby.query.cause);
    else {
        /* Code active use has no World receiver. In particular, a NULL or
         * source-unused World must not be inspected just to report failure. */
        error->nearby.query.status = mc_world_runtime_registry_status(status);
        error->nearby.query.cause = (mc_world_storage_error){error->nearby.query.status,
                                                             "Moving-piston geometry Registry", 0};
    }
    error->nearby.query.operation = error->nearby.query.cause.operation;
    return false;
}

static mc_world_storage_status shifted_box_core(mc_world_runtime_classes *code_classes,
                                                mc_world_runtime *world, bool legacy_world,
                                                const mc_block *moving, int32_t x, int32_t y,
                                                int32_t z, int32_t carried_id, float progress,
                                                int32_t facing, mc_entity_bounds **out,
                                                mc_world_collision_error *error) {
    block_context frame_context = {code_classes, world, legacy_world};
    block_context *const context = &frame_context;
    mc_world_collision_error frame = error == NULL ? mc_world_collision_empty_error() : *error;
    mc_world_storage_status status = MC_WORLD_STORAGE_OK;
    frame.nearby = mc_world_nearby_empty_error();
    frame.bounds = (mc_entity_bounds_error){0};
    frame.shifted_bytecode = 0;
    frame.block_class = NULL;
    frame.block_bytecode = 0;
    frame.x = x;
    frame.y = y;
    frame.z = z;
    mc_entity_bounds *bounds = NULL;
    if (out == NULL)
        status = fail(&frame, MC_WORLD_STORAGE_INVALID_ARGUMENT, "Piston shifted-box output");
    else if (!block_admit(context, moving, &frame))
        status = frame.nearby.query.status;
    else if (moving->kind != MC_BLOCK_UT_MZ)
        status = fail(&frame, MC_WORLD_STORAGE_INVALID_ARGUMENT, "Piston shifted-box receiver");
    else if (carried_id == 0 || carried_id == moving->fields.bn)
        *out = NULL;
    else {
        mc_registry_classes *const registry = context->code_classes->registry;
        mc_registry_block_m_view array;
        mc_registry_error child = frame.nearby.query.registry;
        frame.shifted_bytecode = 16;
        stage(context, &frame, "ut", "mz", 16);
        if (!piston_registry_result(context, &frame,
                                    mc_registry_block_m_get(registry, &array, &child), &child))
            status = frame.nearby.query.status;
        else {
            const mc_block *carrier = NULL;
            frame.shifted_bytecode = 21;
            stage(context, &frame, "ut", "mz", 21);
            if (!piston_registry_result(
                    context, &frame, mc_registry_block_m_at(&array, carried_id, &carrier, &child),
                    &child))
                status = frame.nearby.query.status;
            else {
                frame.shifted_bytecode = 27;
                stage(context, &frame, "ut", "mz", 27);
                if (carrier == NULL)
                    status = fail(&frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                  "Piston carried Block receiver");
                else
                    status =
                        block_box_core(context->code_classes, context->world, context->legacy_world,
                                       carrier, x, y, z, &bounds, &frame);
            }
        }
        if (status == MC_WORLD_STORAGE_OK && bounds != NULL) {
            static const mc_registry_direction_field fields[6] = {
                MC_REGISTRY_DIRECTION_B, MC_REGISTRY_DIRECTION_B, MC_REGISTRY_DIRECTION_C,
                MC_REGISTRY_DIRECTION_C, MC_REGISTRY_DIRECTION_D, MC_REGISTRY_DIRECTION_D};
            static const unsigned int get_pc[6] = {45, 66, 87, 108, 129, 150};
            double *const coordinates[6] = {&bounds->min_x, &bounds->max_x, &bounds->min_y,
                                            &bounds->max_y, &bounds->min_z, &bounds->max_z};
            for (size_t i = 0; i < 6; ++i) {
                /* Source getfield precedes each independent getstatic. */
                const double coordinate = *coordinates[i];
                mc_registry_direction_view direction;
                frame.shifted_bytecode = get_pc[i];
                stage(context, &frame, "ut", "mz", get_pc[i]);
                if (!piston_registry_result(
                        context, &frame,
                        mc_registry_direction_get(registry, fields[i], &direction, &child),
                        &child)) {
                    status = frame.nearby.query.status;
                    break;
                }
                int32_t component;
                frame.shifted_bytecode = get_pc[i] + 5u;
                stage(context, &frame, "ut", "mz", get_pc[i] + 5u);
                if (!piston_registry_result(
                        context, &frame,
                        mc_registry_direction_at(&direction, facing, &component, &child), &child)) {
                    status = frame.nearby.query.status;
                    break;
                }
                frame.shifted_bytecode = get_pc[i] + 9u;
                if (!floating(context, &frame, "ut", "mz", get_pc[i] + 9u)) {
                    status = frame.nearby.query.status;
                    break;
                }
                const float offset = mc_piston_float_multiply((float)component, progress);
                *coordinates[i] = mc_piston_coordinate_subtract(coordinate, offset);
            }
        }
        if (status == MC_WORLD_STORAGE_OK)
            *out = bounds;
        else if (bounds != NULL)
            (void)mc_entity_bounds_release(context->code_classes->entities, bounds);
    }
    if (error != NULL)
        *error = frame;
    return status;
}

mc_world_storage_status
mc_world_collision_piston_shifted_box_held(mc_world_runtime *world, const mc_block *moving,
                                           int32_t x, int32_t y, int32_t z, int32_t carried_id,
                                           float progress, int32_t facing, mc_entity_bounds **out,
                                           mc_world_collision_error *error) {
    return shifted_box_core(NULL, world, true, moving, x, y, z, carried_id, progress, facing, out,
                            error);
}

mc_world_storage_status mc_world_collision_piston_shifted_box_in_classes_held(
    mc_world_runtime_classes *code_classes, mc_world_runtime *world, const mc_block *moving,
    int32_t x, int32_t y, int32_t z, int32_t carried_id, float progress, int32_t facing,
    mc_entity_bounds **out, mc_world_collision_error *error) {
    return shifted_box_core(code_classes, world, false, moving, x, y, z, carried_id, progress,
                            facing, out, error);
}

/* Actual ut.a(xp,III)/mz.a(pb,III). This is a singleton Block-field update,
 * separate from e(World,III) and the pooled shifted-box helper. */
static mc_world_storage_status moving_piston_state_shape(const block_context *context,
                                                         mc_block *moving, int32_t x, int32_t y,
                                                         int32_t z,
                                                         mc_world_collision_error *error) {
    error->piston_bytecode = 6;
    mc_tile *tile = NULL;
    mc_world_storage_status status = moving_piston_tile(context, x, y, z, &tile, error);
    if (status != MC_WORLD_STORAGE_OK || tile == NULL)
        return status;

    mc_registry_classes *const registry = context->code_classes->registry;
    mc_registry_block_m_view array;
    mc_registry_error child = error->nearby.query.registry;
    const mc_block *carrier = NULL;
    int32_t carried_id;
    mc_tile_error lower = mc_tile_empty_error();
    error->piston_bytecode = 16;
    stage(context, error, "ut", "mz", 16);
    if (!piston_registry_result(context, error, mc_registry_block_m_get(registry, &array, &child),
                                &child)) {
        status = error->nearby.query.status;
        goto done;
    }
    error->piston_bytecode = 21;
    stage(context, error, "ut", "mz", 21);
    mc_tile_status tile_status = mc_tile_piston_block_id(tile, &carried_id);
    if (tile_status != MC_TILE_OK) {
        status = piston_tile_failure(error, tile_status, &lower);
        goto done;
    }
    error->piston_bytecode = 24;
    stage(context, error, "ut", "mz", 24);
    if (!piston_registry_result(
            context, error, mc_registry_block_m_at(&array, carried_id, &carrier, &child), &child)) {
        status = error->nearby.query.status;
        goto done;
    }
    /* The bytecode compares object identity, not carried ID or bn. Even a
     * nonnull m[0] is a reached carrier, and another Block with the same ID
     * still receives its real virtual state-shape call. */
    if (carrier == NULL || carrier == moving)
        goto done;
    error->piston_bytecode = 46;
    stage(context, error, "ut", "mz", 46);
    status = state_shape_core(context->code_classes, context->world, context->legacy_world, carrier,
                              x, y, z, error);
    if (status != MC_WORLD_STORAGE_OK)
        goto done;

    /* Keep the captured carrier and Tile identities; read progress, extending
     * and facing only after the carrier's complete virtual call. */
    uint32_t progress_bits;
    float progress;
    bool extending;
    int32_t facing;
    error->piston_bytecode = 52;
    stage(context, error, "ut", "mz", 52);
    tile_status = mc_tile_piston_progress(tile, 0, &progress_bits, &lower);
    if (tile_status != MC_TILE_OK) {
        status = piston_tile_failure(error, tile_status, &lower);
        goto done;
    }
    memcpy(&progress, &progress_bits, sizeof(progress));
    error->piston_bytecode = 59;
    stage(context, error, "ut", "mz", 59);
    tile_status = mc_tile_piston_extending(tile, &extending);
    if (tile_status != MC_TILE_OK) {
        status = piston_tile_failure(error, tile_status, &lower);
        goto done;
    }
    if (extending) {
        error->piston_bytecode = 68;
        stage(context, error, "ut", "mz", 68);
        progress = mc_piston_float_subtract(1.0f, progress);
    }
    error->piston_bytecode = 73;
    stage(context, error, "ut", "mz", 73);
    tile_status = mc_tile_piston_facing(tile, &facing);
    if (tile_status != MC_TILE_OK) {
        status = piston_tile_failure(error, tile_status, &lower);
        goto done;
    }
    const mc_registry_direction_field fields[6] = {
        MC_REGISTRY_DIRECTION_B, MC_REGISTRY_DIRECTION_C, MC_REGISTRY_DIRECTION_D,
        MC_REGISTRY_DIRECTION_B, MC_REGISTRY_DIRECTION_C, MC_REGISTRY_DIRECTION_D};
    static const unsigned int get_pc[6] = {84, 105, 126, 147, 168, 189};
    const double *const source[6] = {&carrier->fields.bs, &carrier->fields.bt, &carrier->fields.bu,
                                     &carrier->fields.bv, &carrier->fields.bw, &carrier->fields.bx};
    double *const destination[6] = {&moving->fields.bs, &moving->fields.bt, &moving->fields.bu,
                                    &moving->fields.bv, &moving->fields.bw, &moving->fields.bx};
    for (size_t i = 0; i < 6; ++i) {
        /* Each source getfield precedes its independent Direction getstatic.
         * A late failure leaves the earlier singleton stores in place. */
        const double coordinate = *source[i];
        mc_registry_direction_view direction;
        error->piston_bytecode = get_pc[i];
        stage(context, error, "ut", "mz", get_pc[i]);
        if (!piston_registry_result(
                context, error, mc_registry_direction_get(registry, fields[i], &direction, &child),
                &child)) {
            status = error->nearby.query.status;
            break;
        }
        int32_t component;
        error->piston_bytecode = get_pc[i] + 5u;
        stage(context, error, "ut", "mz", get_pc[i] + 5u);
        if (!piston_registry_result(
                context, error, mc_registry_direction_at(&direction, facing, &component, &child),
                &child)) {
            status = error->nearby.query.status;
            break;
        }
        error->piston_bytecode = get_pc[i] + 9u;
        if (!floating(context, error, "ut", "mz", get_pc[i] + 9u)) {
            status = error->nearby.query.status;
            break;
        }
        const float offset = mc_piston_float_multiply((float)component, progress);
        *destination[i] = mc_piston_coordinate_subtract(coordinate, offset);
    }
done:
    mc_tile_release(tile);
    return status;
}

/* All actual a(BlockAccess,III) declarations in the paired Block hierarchy.
 * Classes absent from this override list inherit uu/na's empty return body. */
static mc_world_storage_status state_shape(const block_context *context, mc_block *block, int32_t x,
                                           int32_t y, int32_t z, mc_world_collision_error *error) {
    int32_t value;
    switch (block->kind) {
        case MC_BLOCK_UT_MZ:
            return moving_piston_state_shape(context, block, x, y, z, error);
        case MC_BLOCK_AK_Z:
            if (!block_value(context, "ak", "z", 7, mc_java_i32_sub(x, 1), y, z, false, &value,
                             error))
                return error->nearby.query.status;
            if (value != block->fields.bn) {
                if (!block_value(context, "ak", "z", 26, mc_java_i32_add(x, 1), y, z, false, &value,
                                 error))
                    return error->nearby.query.status;
            }
            if (value == block->fields.bn)
                shape(context, block, "ak", "z", 69, 0.0f, 0.0f, 0.375f, 1.0f, 1.0f, 0.625f, error);
            else
                shape(context, block, "ak", "z", 106, 0.375f, 0.0f, 0.0f, 0.625f, 1.0f, 1.0f,
                      error);
            break;
        case MC_BLOCK_BV_BC:
            if (!metadata(context, "bv", "bc", 5, x, y, z, &value, error))
                return error->nearby.query.status;
            shape(context, block, "bv", "bc", value == 1 ? 46u : 68u, 0.0625f, 0.0f, 0.0625f,
                  0.9375f, value == 1 ? 0.03125f : 0.0625f, 0.9375f, error);
            break;
        case MC_BLOCK_H_G: {
            if (!metadata(context, "h", "g", 5, x, y, z, &value, error))
                return error->nearby.query.status;
            static const float bounds[6][6] = {{0, 0, 0, 1, 0.25f, 1}, {0, 0.75f, 0, 1, 1, 1},
                                               {0, 0, 0, 1, 1, 0.25f}, {0, 0, 0.75f, 1, 1, 1},
                                               {0, 0, 0, 0.25f, 1, 1}, {0.75f, 0, 0, 1, 1, 1}};
            static const unsigned int pc[6] = {64, 78, 92, 106, 120, 134};
            const int32_t direction = value & 7;
            if (direction < 6) {
                const float *b = bounds[(size_t)direction];
                shape(context, block, "h", "g", pc[(size_t)direction], b[0], b[1], b[2], b[3], b[4],
                      b[5], error);
            }
            break;
        }
        case MC_BLOCK_JQ_GI: {
            if (!metadata(context, "jq", "gi", 5, x, y, z, &value, error))
                return error->nearby.query.status;
            static const float bounds[6][6] = {{0, 0.25f, 0, 1, 1, 1}, {0, 0, 0, 1, 0.75f, 1},
                                               {0, 0, 0.25f, 1, 1, 1}, {0, 0, 0, 1, 1, 0.75f},
                                               {0.25f, 0, 0, 1, 1, 1}, {0, 0, 0, 0.75f, 1, 1}};
            static const unsigned int pc[6] = {72, 86, 100, 114, 128, 142};
            const int32_t direction = value & 7;
            if ((value & 8) == 0)
                shape(context, block, "jq", "gi", 155, 0, 0, 0, 1, 1, 1, error);
            else if (direction < 6) {
                const float *b = bounds[(size_t)direction];
                shape(context, block, "jq", "gi", pc[(size_t)direction], b[0], b[1], b[2], b[3],
                      b[4], b[5], error);
            }
            break;
        }
        case MC_BLOCK_JR_GJ: {
            if (!metadata(context, "jr", "gj", 5, x, y, z, &value, error))
                return error->nearby.query.status;
            if (!floating(context, error, "jr", "gj", 24))
                return error->nearby.query.status;
            const float height = (float)mc_java_i32_mul(2, mc_java_i32_add(1, value & 7)) / 16.0f;
            shape(context, block, "jr", "gj", 35, 0, 0, 0, 1, height, 1, error);
            break;
        }
        case MC_BLOCK_LE_HC:
            if (!door_shape(context, block, x, y, z, error))
                return error->nearby.query.status;
            break;
        case MC_BLOCK_OQ_JD:
            if (!trapdoor_shape(context, block, x, y, z, error))
                return error->nearby.query.status;
            break;
        case MC_BLOCK_OI_IZ: {
            if (!metadata(context, "oi", "iz", 5, x, y, z, &value, error))
                return error->nearby.query.status;
            const float depth = (value & 8) != 0 ? 0.0625f : 0.125f;
            switch (value & 7) {
                case 1:
                    shape(context, block, "oi", "iz", 83, 0, 0.375f, 0.3125f, depth, 0.625f,
                          0.6875f, error);
                    break;
                case 2:
                    shape(context, block, "oi", "iz", 115, 1.0f - depth, 0.375f, 0.3125f, 1, 0.625f,
                          0.6875f, error);
                    break;
                case 3:
                    shape(context, block, "oi", "iz", 145, 0.3125f, 0.375f, 0, 0.6875f, 0.625f,
                          depth, error);
                    break;
                case 4:
                    shape(context, block, "oi", "iz", 177, 0.3125f, 0.375f, 1.0f - depth, 0.6875f,
                          0.625f, 1, error);
                    break;
            }
            break;
        }
        case MC_BLOCK_PC_JJ:
        case MC_BLOCK_PH_JN:
            if (!metadata(context, "pc", "jj", 5, x, y, z, &value, error))
                return error->nearby.query.status;
            shape(context, block, "pc", "jj", value >= 2 && value <= 5 ? 32u : 46u, 0, 0, 0, 1,
                  value >= 2 && value <= 5 ? 0.625f : 0.125f, 1, error);
            break;
        case MC_BLOCK_SS_LU:
            shape(context, block, "ss", "lu", 7, 0, 0, 0, 1, 1, 1, error);
            break;
        case MC_BLOCK_UJ_MT:
            stage(context, error, "uj", "mt", 1);
            if (block->subtype.uj.b)
                break;
            if (!metadata(context, "uj", "mt", 13, x, y, z, &value, error))
                return error->nearby.query.status;
            shape(context, block, "uj", "mt", 45, 0, 0, 0, 1, 1, 1, error);
            if (value == 2)
                shape(context, block, "uj", "mt", 68, 0, 0.28125f, 0.875f, 1, 0.78125f, 1, error);
            if (value == 3)
                shape(context, block, "uj", "mt", 89, 0, 0.28125f, 0, 1, 0.78125f, 0.125f, error);
            if (value == 4)
                shape(context, block, "uj", "mt", 112, 0.875f, 0.28125f, 0, 1, 0.78125f, 1, error);
            if (value == 5)
                shape(context, block, "uj", "mt", 133, 0, 0.28125f, 0, 0.125f, 0.78125f, 1, error);
            break;
        case MC_BLOCK_UM_MW: {
            if (!metadata(context, "um", "mw", 5, x, y, z, &value, error))
                return error->nearby.query.status;
            if (!floating(context, error, "um", "mw", 25))
                return error->nearby.query.status;
            const float width = (float)mc_java_i32_add(1, mc_java_i32_mul(value, 2)) / 16.0f;
            shape(context, block, "um", "mw", 48, width, 0, 0.0625f, 0.9375f, 0.5f, 0.9375f, error);
            break;
        }
        case MC_BLOCK_VE_NJ:
            shape(context, block, "ve", "nj", 8, 0, 0, 0, 1, 0.5625f, 1, error);
            break;
        case MC_BLOCK_XR_PD:
            if (!metadata(context, "xr", "pd", 5, x, y, z, &value, error))
                return error->nearby.query.status;
            switch (value & 7) {
                case 1:
                    shape(context, block, "xr", "pd", 45, 0, 0.2f, 0.3125f, 0.375f, 0.8f, 0.6875f,
                          error);
                    break;
                case 2:
                    shape(context, block, "xr", "pd", 79, 0.625f, 0.2f, 0.3125f, 1, 0.8f, 0.6875f,
                          error);
                    break;
                case 3:
                    shape(context, block, "xr", "pd", 111, 0.3125f, 0.2f, 0, 0.6875f, 0.8f, 0.375f,
                          error);
                    break;
                case 4:
                    shape(context, block, "xr", "pd", 145, 0.3125f, 0.2f, 0.625f, 0.6875f, 0.8f, 1,
                          error);
                    break;
                default:
                    shape(context, block, "xr", "pd", 179, 0.25f, 0, 0.25f, 0.75f, 0.6f, 0.75f,
                          error);
                    break;
            }
            break;
        case MC_BLOCK_UU_NA:
        case MC_BLOCK_EO_CX:
        case MC_BLOCK_WP_OJ:
        case MC_BLOCK_OT_JF:
        case MC_BLOCK_HE_EU:
        case MC_BLOCK_OM_JC:
        case MC_BLOCK_NX_IS:
        case MC_BLOCK_GK_EI:
        case MC_BLOCK_NE_IG:
        case MC_BLOCK_MT_IC:
        case MC_BLOCK_VG_NK:
        case MC_BLOCK_BK_AR:
        case MC_BLOCK_XF_OV:
        case MC_BLOCK_FK_DO:
        case MC_BLOCK_XQ_PA:
        case MC_BLOCK_RD_KL:
        case MC_BLOCK_PT_JR:
        case MC_BLOCK_RN_KU:
        case MC_BLOCK_RU_KZ:
        case MC_BLOCK_JB_FZ:
        case MC_BLOCK_EE_CQ:
        case MC_BLOCK_WB_NW:
        case MC_BLOCK_TL_MD:
        case MC_BLOCK_L_J:
        case MC_BLOCK_YS_PW:
        case MC_BLOCK_AH_T:
        case MC_BLOCK_HB_ER:
        case MC_BLOCK_FB_DH:
        case MC_BLOCK_VM_NP:
        case MC_BLOCK_YQ_PT:
        case MC_BLOCK_DD_BZ:
        case MC_BLOCK_E_E:
        case MC_BLOCK_SM_LO:
        case MC_BLOCK_FI_DN:
        case MC_BLOCK_NI_IJ:
        case MC_BLOCK_VL_NO:
        case MC_BLOCK_TC_LZ:
        case MC_BLOCK_DP_CG:
        case MC_BLOCK_BS_AZ:
        case MC_BLOCK_DB_BY:
        case MC_BLOCK_NK_IK:
        case MC_BLOCK_AC_P:
        case MC_BLOCK_OR_JE:
        case MC_BLOCK_RZ_LF:
        case MC_BLOCK_RI_KO:
        case MC_BLOCK_FO_DT:
        case MC_BLOCK_JW_GN:
        case MC_BLOCK_FC_DI:
        case MC_BLOCK_YI_PO:
        case MC_BLOCK_OA_IU:
        case MC_BLOCK_LY_HR:
        case MC_BLOCK_WO_OI:
        case MC_BLOCK_KS_GV:
            stage(context, error, "uu", "na", 0);
            break;
        case MC_BLOCK_KIND_COUNT:
        default:
            return fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                        "State-shape Block concrete kind");
    }
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status state_shape_core(mc_world_runtime_classes *code_classes,
                                                mc_world_runtime *world, bool legacy_world,
                                                const mc_block *block, int32_t x, int32_t y,
                                                int32_t z, mc_world_collision_error *error) {
    block_context context = {code_classes, world, legacy_world};
    mc_world_collision_error frame = error == NULL ? mc_world_collision_empty_error() : *error;
    frame.nearby = mc_world_nearby_empty_error();
    frame.bounds = (mc_entity_bounds_error){0};
    frame.block_class = NULL;
    frame.block_bytecode = 0;
    frame.x = x;
    frame.y = y;
    frame.z = z;
    const mc_world_storage_status status =
        block_admit(&context, block, &frame)
            ? state_shape(&context, (mc_block *)block, x, y, z, &frame)
            : frame.nearby.query.status;
    if (error != NULL)
        *error = frame;
    return status;
}

mc_world_storage_status mc_world_collision_block_state_shape_held(mc_world_runtime *world,
                                                                  const mc_block *block, int32_t x,
                                                                  int32_t y, int32_t z,
                                                                  mc_world_collision_error *error) {
    return state_shape_core(NULL, world, true, block, x, y, z, error);
}

mc_world_storage_status mc_world_collision_block_state_shape_in_classes_held(
    mc_world_runtime_classes *code_classes, mc_world_runtime *world, const mc_block *block,
    int32_t x, int32_t y, int32_t z, mc_world_collision_error *error) {
    return state_shape_core(code_classes, world, false, block, x, y, z, error);
}

static mc_world_storage_status base_contribute(const block_context *context, mc_block *block,
                                               int32_t x, int32_t y, int32_t z,
                                               const mc_entity_bounds *query,
                                               mc_world_reference_list *destination,
                                               mc_world_collision_error *error) {
    mc_entity_bounds *bounds = NULL;
    mc_world_storage_status status = box(context, block, x, y, z, &bounds, error);
    if (status != MC_WORLD_STORAGE_OK || bounds == NULL)
        return status;
    stage(context, error, "uu", "na", 20);
    if (query == NULL)
        status = fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Block collision query bounds");
    else if (mc_entity_bounds_intersects(query, bounds)) {
        stage(context, error, "uu", "na", 30);
        status = mc_world_collision_append_pool_held(context->world, destination, bounds, error);
    }
    (void)mc_entity_bounds_release(context->code_classes->entities, bounds);
    return status;
}

/* Each row is the actual two setter argument tuples, not an envelope. Head
 * directions2/4/5 deliberately retain the original asymmetric rod values. */
typedef struct contribution_part {
    float bounds[6];
    unsigned int setter, call;
} contribution_part;

static const contribution_part head_parts[6][2] = {
    {{{0.0f, 0.0f, 0.0f, 1.0f, 0.25f, 1.0f}, 60, 73},
     {{0.375f, 0.25f, 0.375f, 0.625f, 1.0f, 0.625f}, 88, 101}},
    {{{0.0f, 0.75f, 0.0f, 1.0f, 1.0f, 1.0f}, 115, 128},
     {{0.375f, 0.0f, 0.375f, 0.625f, 0.75f, 0.625f}, 143, 156}},
    {{{0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.25f}, 170, 183},
     {{0.25f, 0.375f, 0.25f, 0.75f, 0.625f, 1.0f}, 198, 211}},
    {{{0.0f, 0.0f, 0.75f, 1.0f, 1.0f, 1.0f}, 225, 238},
     {{0.25f, 0.375f, 0.0f, 0.75f, 0.625f, 0.75f}, 253, 266}},
    {{{0.0f, 0.0f, 0.0f, 0.25f, 1.0f, 1.0f}, 280, 293},
     {{0.375f, 0.25f, 0.25f, 0.625f, 0.75f, 1.0f}, 308, 321}},
    {{{0.75f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f}, 335, 348},
     {{0.0f, 0.375f, 0.25f, 0.75f, 0.625f, 0.75f}, 363, 376}}};

static const contribution_part stair_parts[4][2] = {
    {{{0.0f, 0.0f, 0.0f, 0.5f, 0.5f, 1.0f}, 24, 37},
     {{0.5f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f}, 48, 61}},
    {{{0.0f, 0.0f, 0.0f, 0.5f, 1.0f, 1.0f}, 81, 94},
     {{0.5f, 0.0f, 0.0f, 1.0f, 0.5f, 1.0f}, 106, 119}},
    {{{0.0f, 0.0f, 0.0f, 1.0f, 0.5f, 0.5f}, 140, 153},
     {{0.0f, 0.0f, 0.5f, 1.0f, 1.0f, 1.0f}, 164, 177}},
    {{{0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.5f}, 197, 210},
     {{0.0f, 0.0f, 0.5f, 1.0f, 0.5f, 1.0f}, 221, 235}}};

static mc_world_storage_status contribute(const block_context *context, mc_block *block, int32_t x,
                                          int32_t y, int32_t z, const mc_entity_bounds *query,
                                          mc_world_reference_list *destination,
                                          mc_world_collision_error *error) {
    if (block->kind == MC_BLOCK_JQ_GI) {
        shape(context, block, "jq", "gi", 7, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, error);
        return base_contribute(context, block, x, y, z, query, destination, error);
    }
    if (block->kind != MC_BLOCK_H_G && block->kind != MC_BLOCK_SS_LU)
        return base_contribute(context, block, x, y, z, query, destination, error);
    const bool head = block->kind == MC_BLOCK_H_G;
    const char *const client = head ? "h" : "ss";
    const char *const server = head ? "g" : "lu";
    int32_t value;
    if (!metadata(context, client, server, 5, x, y, z, &value, error))
        return error->nearby.query.status;
    if (head)
        value &= 7;
    if (value >= 0 && value < (head ? 6 : 4)) {
        const contribution_part *const parts = head ? head_parts[value] : stair_parts[value];
        for (size_t i = 0; i < 2; ++i) {
            const contribution_part *const part = &parts[i];
            shape(context, block, client, server, part->setter, part->bounds[0], part->bounds[1],
                  part->bounds[2], part->bounds[3], part->bounds[4], part->bounds[5], error);
            stage(context, error, client, server, part->call);
            const mc_world_storage_status status =
                base_contribute(context, block, x, y, z, query, destination, error);
            if (status != MC_WORLD_STORAGE_OK)
                return status; /* The original final reset is not a finally. */
        }
    }
    shape(context, block, client, server, head ? 386u : 245u, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,
          error);
    return MC_WORLD_STORAGE_OK;
}

mc_world_storage_status
mc_world_collision_block_contribute_held(mc_world_runtime *world, const mc_block *block, int32_t x,
                                         int32_t y, int32_t z, const mc_entity_bounds *query,
                                         mc_world_reference_list *destination,
                                         mc_world_collision_error *error) {
    block_context frame_context = {NULL, world, true};
    block_context *const context = &frame_context;
    mc_world_collision_error frame = error == NULL ? mc_world_collision_empty_error() : *error;
    frame.nearby = mc_world_nearby_empty_error();
    frame.bounds = (mc_entity_bounds_error){0};
    frame.block_class = NULL;
    frame.block_bytecode = 0;
    frame.x = x;
    frame.y = y;
    frame.z = z;
    mc_world_storage_status status;
    if (!block_admit(context, block, &frame))
        status = frame.nearby.query.status;
    else
        status = contribute(context, (mc_block *)block, x, y, z, query, destination, &frame);
    if (error != NULL)
        *error = frame;
    return status;
}
