#include "blocks_internal.h"

#include "../../../block/registry/registry_internal.h"
#include "../../chunk/object_state_internal.h"
#include "../../storage/objects_internal.h"
#include "../construction_internal.h"
#include "../provider_internal.h"
#include "mc/core/java/java.h"

#include <float.h>
#include <stdint.h>

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

static void stage(mc_world_runtime *world, mc_world_collision_error *error, const char *client,
                  const char *server, unsigned int pc) {
    error->block_class = world->classes->side == MC_WORLD_INFO_CLIENT ? client : server;
    error->block_bytecode = pc;
}

static bool floating(mc_world_runtime *world, mc_world_collision_error *error, const char *client,
                     const char *server, unsigned int pc) {
    stage(world, error, client, server, pc);
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

static mc_world_storage_status pooled(mc_world_runtime *world, const char *client,
                                      const char *server, unsigned int pc, double x0, double y0,
                                      double z0, double x1, double y1, double z1,
                                      mc_entity_bounds **out, mc_world_collision_error *error) {
    stage(world, error, client, server, pc);
    mc_entity_classes *const classes = mc_world_runtime_entities(world);
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

static void shape(mc_world_runtime *world, mc_block *block, const char *client, const char *server,
                  unsigned int pc, float x0, float y0, float z0, float x1, float y1, float z1,
                  mc_world_collision_error *error) {
    stage(world, error, client, server, pc);
    /* This setter does not alter constructor progress or active-use state. */
    mc_registry_block_bounds(block, x0, y0, z0, x1, y1, z1);
}

static mc_world_storage_status base_box(mc_world_runtime *world, const mc_block *block, int32_t x,
                                        int32_t y, int32_t z, mc_entity_bounds **out,
                                        mc_world_collision_error *error) {
    if (!floating(world, error, "uu", "na", 6))
        return error->nearby.query.status;
    const double x0 = (double)x + block->fields.bs;
    const double y0 = (double)y + block->fields.bt;
    const double z0 = (double)z + block->fields.bu;
    const double x1 = (double)x + block->fields.bv;
    const double y1 = (double)y + block->fields.bw;
    const double z1 = (double)z + block->fields.bx;
    return pooled(world, "uu", "na", 44, x0, y0, z0, x1, y1, z1, out, error);
}

static bool metadata(mc_world_runtime *world, const char *client, const char *server,
                     unsigned int pc, int32_t x, int32_t y, int32_t z, int32_t *out,
                     mc_world_collision_error *error) {
    stage(world, error, client, server, pc);
    return mc_world_collision_block_value_held(world, x, y, z, true, out, error) ==
           MC_WORLD_STORAGE_OK;
}

static bool door_shape(mc_world_runtime *world, mc_block *block, int32_t x, int32_t y, int32_t z,
                       mc_world_collision_error *error) {
    int32_t value;
    if (!metadata(world, "le", "hc", 7, x, y, z, &value, error))
        return false;
    const int32_t direction = (value & 4) == 0 ? mc_java_i32_sub(value, 1) & 3 : value & 3;
    const float width = 0.1875f;
    shape(world, block, "le", "hc", 10, 0.0f, 0.0f, 0.0f, 1.0f, 2.0f, 1.0f, error);
    switch (direction) {
        case 0:
            shape(world, block, "le", "hc", 24, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, width, error);
            break;
        case 1:
            shape(world, block, "le", "hc", 41, 1.0f - width, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, error);
            break;
        case 2:
            shape(world, block, "le", "hc", 58, 0.0f, 0.0f, 1.0f - width, 1.0f, 1.0f, 1.0f, error);
            break;
        case 3:
            shape(world, block, "le", "hc", 73, 0.0f, 0.0f, 0.0f, width, 1.0f, 1.0f, error);
            break;
    }
    return true;
}

static bool trapdoor_shape(mc_world_runtime *world, mc_block *block, int32_t x, int32_t y,
                           int32_t z, mc_world_collision_error *error) {
    int32_t value;
    if (!metadata(world, "oq", "jd", 6, x, y, z, &value, error))
        return false;
    const float width = 0.1875f;
    shape(world, block, "oq", "jd", 10, 0.0f, 0.0f, 0.0f, 1.0f, width, 1.0f, error);
    if ((value & 4) != 0) {
        switch (value & 3) {
            case 0:
                shape(world, block, "oq", "jd", 35, 0.0f, 0.0f, 1.0f - width, 1.0f, 1.0f, 1.0f,
                      error);
                break;
            case 1:
                shape(world, block, "oq", "jd", 52, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, width, error);
                break;
            case 2:
                shape(world, block, "oq", "jd", 71, 1.0f - width, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,
                      error);
                break;
            case 3:
                shape(world, block, "oq", "jd", 88, 0.0f, 0.0f, 0.0f, width, 1.0f, 1.0f, error);
                break;
        }
    }
    return true;
}

static mc_world_storage_status moving_piston_box(mc_world_runtime *world, int32_t x, int32_t y,
                                                 int32_t z, mc_entity_bounds **out,
                                                 mc_world_collision_error *error) {
    /* e invokes its actual private typed lookup6; the reached Tile query is5
     * inside that helper, before its null/instanceof tests. */
    stage(world, error, "ut", "mz", 5);
    mc_tile *tile = NULL;
    mc_live_tile_error child = mc_live_tiles_empty_error();
    const mc_world_storage_status status = mc_live_tiles_get(world, x, y, z, &tile, &child);
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
    mc_tile_release(tile);
    stage(world, error, "ut", "mz", 19);
    if (inspected != MC_TILE_OK) {
        error->nearby.tile.tile.status = inspected;
        return fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                    "Moving-piston Tile concrete identity");
    }
    if (snapshot.kind == MC_TILE_CHEST || snapshot.kind == MC_TILE_SPAWNER) {
        *out = NULL; /* Actual instanceof uk/mu is false for these real types. */
        return MC_WORLD_STORAGE_OK;
    }
    return fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                "Moving-piston positive Tile geometry descendant");
}

static mc_world_storage_status box(mc_world_runtime *world, mc_block *block, int32_t x, int32_t y,
                                   int32_t z, mc_entity_bounds **out,
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
            stage(world, error, "wb", "nw", 1);
            break;
        case MC_BLOCK_OM_JC:
        case MC_BLOCK_NX_IS:
            stage(world, error, "rp", "kv", 1);
            break;
        case MC_BLOCK_PC_JJ:
        case MC_BLOCK_PH_JN:
            stage(world, error, "pc", "jj", 1);
            break;
        case MC_BLOCK_RN_KU:
            stage(world, error, "rn", "ku", 1);
            break;
        case MC_BLOCK_VM_NP:
        case MC_BLOCK_DB_BY:
            stage(world, error, "vm", "np", 1);
            break;
        case MC_BLOCK_YQ_PT:
            stage(world, error, "yq", "pt", 1);
            break;
        case MC_BLOCK_SM_LO:
            stage(world, error, "sm", "lo", 1);
            break;
        case MC_BLOCK_UJ_MT:
            stage(world, error, "uj", "mt", 1);
            break;
        case MC_BLOCK_XR_PD:
            stage(world, error, "xr", "pd", 1);
            break;
        case MC_BLOCK_BV_BC:
            stage(world, error, "bv", "bc", 1);
            break;
        case MC_BLOCK_OI_IZ:
            stage(world, error, "oi", "iz", 1);
            break;
        case MC_BLOCK_RI_KO:
            stage(world, error, "ri", "ko", 1);
            break;
        case MC_BLOCK_AK_Z:
            stage(world, error, "ak", "z", 1);
            break;
        case MC_BLOCK_DP_CG: {
            if (!metadata(world, "dp", "cg", 5, x, y, z, &value, error))
                return error->nearby.query.status;
            const float depth = 0.125f;
            if (value == 2) {
                if (!floating(world, error, "dp", "cg", 26))
                    return error->nearby.query.status;
                shape(world, block, "dp", "cg", 30, 0.0f, 0.0f, 1.0f - depth, 1.0f, 1.0f, 1.0f,
                      error);
            }
            if (value == 3)
                shape(world, block, "dp", "cg", 47, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, depth, error);
            if (value == 4) {
                if (!floating(world, error, "dp", "cg", 60))
                    return error->nearby.query.status;
                shape(world, block, "dp", "cg", 66, 1.0f - depth, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,
                      error);
            }
            if (value == 5)
                shape(world, block, "dp", "cg", 83, 0.0f, 0.0f, 0.0f, depth, 1.0f, 1.0f, error);
            return base_box(world, block, x, y, z, out, error);
        }
        case MC_BLOCK_JR_GJ: {
            if (!metadata(world, "jr", "gj", 5, x, y, z, &value, error))
                return error->nearby.query.status;
            if ((value & 7) < 3) {
                *out = NULL;
                return MC_WORLD_STORAGE_OK;
            }
            if (!floating(world, error, "jr", "gj", 25))
                return error->nearby.query.status;
            const double x0 = (double)x + block->fields.bs;
            const double y0 = (double)y + block->fields.bt;
            const double z0 = (double)z + block->fields.bu;
            const double x1 = (double)x + block->fields.bv;
            const float y1 = (float)y + 0.5f;
            const double z1 = (double)z + block->fields.bx;
            return pooled(world, "jr", "gj", 62, x0, y0, z0, x1, (double)y1, z1, out, error);
        }
        case MC_BLOCK_JW_GN: {
            if (!floating(world, error, "jw", "gn", 12))
                return error->nearby.query.status;
            const float y1 = (float)y + 1.5f;
            return pooled(world, "jw", "gn", 22, (double)x, (double)y, (double)z,
                          (double)mc_java_i32_add(x, 1), (double)y1, (double)mc_java_i32_add(z, 1),
                          out, error);
        }
        case MC_BLOCK_LE_HC:
            if (!door_shape(world, block, x, y, z, error))
                return error->nearby.query.status;
            return base_box(world, block, x, y, z, out, error);
        case MC_BLOCK_OQ_JD:
            if (!trapdoor_shape(world, block, x, y, z, error))
                return error->nearby.query.status;
            return base_box(world, block, x, y, z, out, error);
        case MC_BLOCK_OA_IU: {
            if (!floating(world, error, "oa", "iu", 18))
                return error->nearby.query.status;
            const float y1 = (float)mc_java_i32_add(y, 1) - 0.125f;
            return pooled(world, "oa", "iu", 28, (double)x, (double)y, (double)z,
                          (double)mc_java_i32_add(x, 1), (double)y1, (double)mc_java_i32_add(z, 1),
                          out, error);
        }
        case MC_BLOCK_OR_JE: {
            if (!floating(world, error, "or", "je", 5))
                return error->nearby.query.status;
            const float edge = 0.0625f;
            const float x0 = (float)x + edge;
            const float z0 = (float)z + edge;
            const float x1 = (float)mc_java_i32_add(x, 1) - edge;
            const float y1 = (float)mc_java_i32_add(y, 1) - edge;
            const float z1 = (float)mc_java_i32_add(z, 1) - edge;
            return pooled(world, "or", "je", 44, (double)x0, (double)y, (double)z0, (double)x1,
                          (double)y1, (double)z1, out, error);
        }
        case MC_BLOCK_UM_MW: {
            if (!metadata(world, "um", "mw", 5, x, y, z, &value, error))
                return error->nearby.query.status;
            if (!floating(world, error, "um", "mw", 20))
                return error->nearby.query.status;
            const float edge = 0.0625f;
            const float size = (float)mc_java_i32_add(1, mc_java_i32_mul(value, 2)) / 16.0f;
            const float x0 = (float)x + size;
            const float z0 = (float)z + edge;
            const float x1 = (float)mc_java_i32_add(x, 1) - edge;
            float y1 = (float)y + 0.5f;
            y1 = y1 - edge;
            const float z1 = (float)mc_java_i32_add(z, 1) - edge;
            return pooled(world, "um", "mw", 71, (double)x0, (double)y, (double)z0, (double)x1,
                          (double)y1, (double)z1, out, error);
        }
        case MC_BLOCK_UT_MZ:
            return moving_piston_box(world, x, y, z, out, error);
        case MC_BLOCK_VL_NO:
            return pooled(world, "vl", "no", 26, (double)x, (double)y, (double)z,
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
            return base_box(world, block, x, y, z, out, error);
        case MC_BLOCK_KIND_COUNT:
        default:
            return fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Collision Block concrete kind");
    }
    *out = NULL;
    return MC_WORLD_STORAGE_OK;
}

static bool block_admit(mc_world_runtime *world, const mc_block *block,
                        mc_world_collision_error *error) {
    if (world == NULL) {
        fail(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "Collision Block World");
        return false;
    }
    if (block == NULL || world->classes == NULL ||
        block->classes != mc_world_runtime_registry(world) ||
        (unsigned int)block->kind >= (unsigned int)MC_BLOCK_KIND_COUNT) {
        fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Collision Block receiver/owner");
        return false;
    }
    return true;
}

mc_world_storage_status mc_world_collision_block_box_held(mc_world_runtime *world,
                                                          const mc_block *block, int32_t x,
                                                          int32_t y, int32_t z,
                                                          mc_entity_bounds **out,
                                                          mc_world_collision_error *error) {
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
    else if (!block_admit(world, block, &frame))
        status = frame.nearby.query.status;
    else
        status = box(world, (mc_block *)block, x, y, z, out, &frame);
    if (error != NULL)
        *error = frame;
    return status;
}

static mc_world_storage_status base_contribute(mc_world_runtime *world, mc_block *block, int32_t x,
                                               int32_t y, int32_t z, const mc_entity_bounds *query,
                                               mc_world_reference_list *destination,
                                               mc_world_collision_error *error) {
    mc_entity_bounds *bounds = NULL;
    mc_world_storage_status status = box(world, block, x, y, z, &bounds, error);
    if (status != MC_WORLD_STORAGE_OK || bounds == NULL)
        return status;
    stage(world, error, "uu", "na", 20);
    if (query == NULL)
        status = fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Block collision query bounds");
    else if (mc_entity_bounds_intersects(query, bounds)) {
        stage(world, error, "uu", "na", 30);
        status = mc_world_collision_append_pool_held(world, destination, bounds, error);
    }
    (void)mc_entity_bounds_release(mc_world_runtime_entities(world), bounds);
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

static mc_world_storage_status contribute(mc_world_runtime *world, mc_block *block, int32_t x,
                                          int32_t y, int32_t z, const mc_entity_bounds *query,
                                          mc_world_reference_list *destination,
                                          mc_world_collision_error *error) {
    if (block->kind == MC_BLOCK_JQ_GI) {
        shape(world, block, "jq", "gi", 7, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, error);
        return base_contribute(world, block, x, y, z, query, destination, error);
    }
    if (block->kind != MC_BLOCK_H_G && block->kind != MC_BLOCK_SS_LU)
        return base_contribute(world, block, x, y, z, query, destination, error);
    const bool head = block->kind == MC_BLOCK_H_G;
    const char *const client = head ? "h" : "ss";
    const char *const server = head ? "g" : "lu";
    int32_t value;
    if (!metadata(world, client, server, 5, x, y, z, &value, error))
        return error->nearby.query.status;
    if (head)
        value &= 7;
    if (value >= 0 && value < (head ? 6 : 4)) {
        const contribution_part *const parts = head ? head_parts[value] : stair_parts[value];
        for (size_t i = 0; i < 2; ++i) {
            const contribution_part *const part = &parts[i];
            shape(world, block, client, server, part->setter, part->bounds[0], part->bounds[1],
                  part->bounds[2], part->bounds[3], part->bounds[4], part->bounds[5], error);
            stage(world, error, client, server, part->call);
            const mc_world_storage_status status =
                base_contribute(world, block, x, y, z, query, destination, error);
            if (status != MC_WORLD_STORAGE_OK)
                return status; /* The original final reset is not a finally. */
        }
    }
    shape(world, block, client, server, head ? 386u : 245u, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,
          error);
    return MC_WORLD_STORAGE_OK;
}

mc_world_storage_status
mc_world_collision_block_contribute_held(mc_world_runtime *world, const mc_block *block, int32_t x,
                                         int32_t y, int32_t z, const mc_entity_bounds *query,
                                         mc_world_reference_list *destination,
                                         mc_world_collision_error *error) {
    mc_world_collision_error frame = error == NULL ? mc_world_collision_empty_error() : *error;
    frame.nearby = mc_world_nearby_empty_error();
    frame.bounds = (mc_entity_bounds_error){0};
    frame.block_class = NULL;
    frame.block_bytecode = 0;
    frame.x = x;
    frame.y = y;
    frame.z = z;
    mc_world_storage_status status;
    if (!block_admit(world, block, &frame))
        status = frame.nearby.query.status;
    else
        status = contribute(world, (mc_block *)block, x, y, z, query, destination, &frame);
    if (error != NULL)
        *error = frame;
    return status;
}
