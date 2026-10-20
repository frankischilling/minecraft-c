#include "mc/world/runtime/population/trees.h"

/* Proposed location: src/world/runtime/population/trees.c. */
#include "../../../block/registry/runtime_internal.h"
#include "../construction_internal.h"
#include "../runtime_internal.h"
#include "mc/block/registry/runtime.h"
#include "mc/core/java/java.h"
#include "mc/core/math/math.h"
#include "mc/world/runtime/blocks.h"
#include "tree_internal.h"
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#if defined(__FAST_MATH__) || (defined(__FINITE_MATH_ONLY__) && __FINITE_MATH_ONLY__ > 0) ||       \
    (defined(_M_FP_FAST) && _M_FP_FAST)
#error "tree features require strict floating transformations"
#endif
#if !defined(_M_X64) && !defined(_M_AMD64) && !defined(__x86_64__)
#error "tree features require the selected x86-64/SSE2 profile"
#endif
#include <emmintrin.h>
#if defined(__clang__)
#pragma STDC FP_CONTRACT OFF
#endif
_Static_assert(FLT_RADIX == 2 && FLT_MANT_DIG == 24 && DBL_MANT_DIG == 53 && FLT_MIN_EXP == -125 &&
                   FLT_MAX_EXP == 128 && DBL_MIN_EXP == -1021 && DBL_MAX_EXP == 1024,
               "binary32/binary64 required");
_Static_assert(sizeof(float) == 4 && sizeof(double) == 8 && FLT_EVAL_METHOD == 0,
               "source-width floating evaluation required");
_Static_assert(FLT_HAS_SUBNORM == 1 && DBL_HAS_SUBNORM == 1, "subnormal support required");

typedef struct tree_pcs {
    unsigned int clearance, clearance_static, clearance_field;
    unsigned int soil, grass_static, grass_field, dirt_static, dirt_field;
    unsigned int soil_write_static, soil_write_field, soil_set;
    unsigned int opacity_capture, leaf_get, opacity_at, leaf_static, leaf_field, leaf_set;
    unsigned int trunk_get, trunk_leaf_static, trunk_leaf_field;
    unsigned int trunk_log_static, trunk_log_field, trunk_set;
} tree_pcs;
static const tree_pcs small_pcs[4] = {{141, 153, 156, 204, 211, 214, 222, 225, 254, 257, 260, 382,
                                       392, 395, 406, 409, 412, 453, 465, 468, 483, 486, 489},
                                      {141, 153, 156, 204, 211, 214, 222, 225, 254, 257, 260, 382,
                                       392, 395, 406, 409, 413, 454, 466, 469, 484, 487, 491},
                                      {171, 183, 186, 234, 241, 244, 252, 255, 284, 287, 290, 387,
                                       397, 400, 411, 414, 418, 495, 507, 510, 525, 528, 532},
                                      {165, 177, 180, 228, 235, 238, 246, 249, 278, 281, 284, 394,
                                       404, 407, 418, 421, 425, 513, 525, 528, 543, 546, 550}};
static const char *const client_classes[5] = {"yh", "k", "pw", "ws", "ih"};
static const char *const server_classes[5] = {"pn", "h", "jt", "on", "fn"};

typedef struct tree_context {
    mc_world_runtime *world;
    mc_java_random *random;
    mc_world_tree_error *error;
    const tree_pcs *pcs;
    bool big;
} tree_context;

static mc_registry_error tree_empty_registry(void)
{
    return (mc_registry_error){.initializer_pc = -1,
                               .block_id = -1,
                               .constructor_pc = -1,
                               .active_initializer_pc = -1,
                               .item_argument = -1};
}
static mc_world_tree_error tree_empty_error(void)
{
    const mc_registry_error registry = tree_empty_registry();
    mc_world_tree_error error = {0};
    error.registry = registry;
    error.mutation.registry = registry;
    return error;
}
static void tree_stage(tree_context *context, const char *method, unsigned int pc)
{
    context->error->method = method;
    context->error->bytecode = pc;
    if (!context->big)
        context->error->entry_bytecode = pc;
}
static void tree_pc(tree_context *context, unsigned int pc)
{
    context->error->bytecode = pc;
    if (!context->big)
        context->error->entry_bytecode = pc;
}
static bool tree_fail(tree_context *context, mc_world_storage_status status, const char *operation,
                      bool native_boundary)
{
    context->error->status = status;
    context->error->operation = operation;
    context->error->native_boundary = native_boundary;
    context->error->cause = (mc_world_storage_error){status, operation, 0};
    return false;
}
static bool tree_storage_fail(tree_context *context, const mc_world_storage_error *cause)
{
    context->error->status = cause->status;
    context->error->operation = cause->operation;
    context->error->cause = *cause;
    return false;
}
static bool tree_registry_fail(tree_context *context, const mc_registry_error *cause,
                               const char *operation)
{
    context->error->registry = *cause;
    context->error->status = mc_world_runtime_registry_result(context->world, cause->status, cause,
                                                              operation, &context->error->cause);
    context->error->operation = context->error->cause.operation;
    return false;
}
static bool tree_get(tree_context *context, unsigned int pc, int32_t x, int32_t y, int32_t z,
                     int32_t *out)
{
    tree_pc(context, pc);
    mc_world_storage_error cause = {0};
    if (mc_world_runtime_block_get(context->world, x, y, z, out, &cause) != MC_WORLD_STORAGE_OK)
        return tree_storage_fail(context, &cause);
    return true;
}
static bool tree_set(tree_context *context, unsigned int pc, int32_t x, int32_t y, int32_t z,
                     int32_t id, bool metadata_write, int32_t metadata)
{
    tree_pc(context, pc);
    bool changed;
    mc_world_mutation_error cause = {0};
    const mc_world_storage_status status =
        metadata_write
            ? mc_world_runtime_block_metadata_set_quiet(context->world, x, y, z, id, metadata,
                                                        &changed, &cause)
            : mc_world_runtime_block_set_quiet(context->world, x, y, z, id, &changed, &cause);
    if (status != MC_WORLD_STORAGE_OK) {
        context->error->mutation = cause;
        context->error->registry = cause.registry;
        return tree_storage_fail(context, &cause.cause);
    }
    return true; /* Source discards changed, including ordinary false. */
}
static bool tree_static_id(tree_context *context, const char *name, unsigned int load_pc,
                           unsigned int field_pc, int32_t *out)
{
    tree_pc(context, load_pc);
    const mc_block *block = NULL;
    mc_registry_error cause = tree_empty_registry();
    if (mc_registry_block_static_get(mc_world_runtime_registry(context->world), name, &block,
                                     &cause) != MC_REGISTRY_OK)
        return tree_registry_fail(context, &cause, "Tree Block static active use");
    tree_pc(context, field_pc);
    const mc_block_fields *fields = mc_block_fields_get(block);
    if (fields == NULL) {
        cause = tree_empty_registry();
        cause.status = MC_REGISTRY_NULL_REFERENCE;
        cause.message = "Tree Block static receiver";
        return tree_registry_fail(context, &cause, cause.message);
    }
    *out = fields->bn;
    return true;
}
static bool tree_air_or_leaf(tree_context *context, int32_t block, unsigned int load_pc,
                             unsigned int field_pc, bool *out)
{
    if (block == 0) {
        *out = true;
        return true;
    }
    int32_t leaf;
    if (!tree_static_id(context, "L", load_pc, field_pc, &leaf))
        return false;
    *out = block == leaf;
    return true;
}
static bool tree_bounded(tree_context *context, unsigned int pc, mc_java_random *random,
                         int32_t bound, int32_t *out)
{
    tree_pc(context, pc);
    if (random == NULL)
        return tree_fail(context, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Tree reached NULL Random",
                         false);
    if (!mc_java_random_next_bounded(random, bound, out))
        return tree_fail(context, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                         "Tree reached nonpositive Random bound", false);
    return true;
}
static bool tree_fp(tree_context *context, const char *method, unsigned int pc)
{
    tree_stage(context, method, pc);
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u)
        return tree_fail(context, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                         "Tree requires nearest masked SSE2 without FTZ/DAZ", true);
    return true;
}
static bool tree_enter(tree_context *context)
{
    if (context->world == NULL || context->world->construction.mode != MC_WORLD_RUNTIME_GAME)
        return tree_fail(context, MC_WORLD_STORAGE_INVALID_ARGUMENT, "Tree requires GAME World",
                         true);
    mc_world_storage_error cause = {0};
    if (mc_world_runtime_call_enter(context->world, &cause) != MC_WORLD_STORAGE_OK) {
        context->error->native_boundary = true;
        return tree_storage_fail(context, &cause);
    }
    return true;
}
static mc_world_storage_status tree_finish(tree_context *context, bool completed,
                                           mc_world_tree_error *out_error)
{
    if (completed)
        *context->error = tree_empty_error();
    else if (context->world != NULL && context->error->bytecode != 0)
        context->world->last_error = context->error->cause;
    if (out_error != NULL)
        *out_error = *context->error;
    return context->error->status;
}
static int32_t tree_abs(int32_t value)
{
    return value < 0 ? mc_java_i32_neg(value) : value;
}
static bool tree_soil(tree_context *context, int32_t x, int32_t y, int32_t z, int32_t height,
                      bool *accepted)
{
    const tree_pcs *pc = context->pcs;
    int32_t below, grass, dirt;
    if (!tree_get(context, pc->soil, x, mc_java_i32_sub(y, 1), z, &below) ||
        !tree_static_id(context, "v", pc->grass_static, pc->grass_field, &grass))
        return false;
    bool soil = below == grass;
    if (!soil) {
        if (!tree_static_id(context, "w", pc->dirt_static, pc->dirt_field, &dirt))
            return false;
        soil = below == dirt;
    }
    if (!soil || y >= mc_java_i32_sub(mc_java_i32_sub(128, height), 1)) {
        *accepted = false;
        return true;
    }
    if (!tree_static_id(context, "w", pc->soil_write_static, pc->soil_write_field, &dirt) ||
        !tree_set(context, pc->soil_set, x, mc_java_i32_sub(y, 1), z, dirt, false, 0))
        return false;
    *accepted = true;
    return true;
}
static bool tree_leaf(tree_context *context, int32_t x, int32_t y, int32_t z, bool metadata_write,
                      int32_t metadata)
{
    const tree_pcs *pc = context->pcs;
    mc_registry_block_o_view array;
    mc_registry_error cause = tree_empty_registry();
    tree_pc(context, pc->opacity_capture);
    if (mc_registry_block_o_get(mc_world_runtime_registry(context->world), &array, &cause) !=
        MC_REGISTRY_OK)
        return tree_registry_fail(context, &cause, "Tree Block.o capture");
    int32_t block;
    if (!tree_get(context, pc->leaf_get, x, y, z, &block))
        return false;
    bool opaque;
    tree_pc(context, pc->opacity_at);
    if (mc_registry_block_o_at(&array, block, &opaque, &cause) != MC_REGISTRY_OK)
        return tree_registry_fail(context, &cause, "Tree captured Block.o element");
    if (opaque)
        return true;
    int32_t leaf;
    return tree_static_id(context, "L", pc->leaf_static, pc->leaf_field, &leaf) &&
           tree_set(context, pc->leaf_set, x, y, z, leaf, metadata_write, metadata);
}
static bool tree_trunk(tree_context *context, int32_t x, int32_t y, int32_t z, int32_t height,
                       bool metadata_write, int32_t metadata)
{
    const tree_pcs *pc = context->pcs;
    for (int32_t layer = 0; layer < height; layer = mc_java_i32_add(layer, 1)) {
        const int32_t ty = mc_java_i32_add(y, layer);
        int32_t block, log;
        bool accepted;
        if (!tree_get(context, pc->trunk_get, x, ty, z, &block) ||
            !tree_air_or_leaf(context, block, pc->trunk_leaf_static, pc->trunk_leaf_field,
                              &accepted))
            return false;
        if (!accepted)
            continue;
        if (!tree_static_id(context, "K", pc->trunk_log_static, pc->trunk_log_field, &log) ||
            !tree_set(context, pc->trunk_set, x, ty, z, log, metadata_write, metadata))
            return false;
    }
    return true;
}

static bool tree_broadleaf_space(tree_context *context, int32_t x, int32_t y, int32_t z,
                                 int32_t height, bool *clear)
{
    bool available = true;
    const int32_t ceiling = mc_java_i32_add(mc_java_i32_add(y, 1), height);
    for (int32_t scan_y = y; scan_y <= ceiling; scan_y = mc_java_i32_add(scan_y, 1)) {
        int32_t radius = 1;
        if (scan_y == y) {
            radius = 0;
        }
        if (scan_y >= mc_java_i32_sub(ceiling, 2)) {
            radius = 2;
        }
        for (int32_t scan_x = mc_java_i32_sub(x, radius);
             scan_x <= mc_java_i32_add(x, radius) && available;
             scan_x = mc_java_i32_add(scan_x, 1)) {
            for (int32_t scan_z = mc_java_i32_sub(z, radius);
                 scan_z <= mc_java_i32_add(z, radius) && available;
                 scan_z = mc_java_i32_add(scan_z, 1)) {
                if (scan_y < 0 || scan_y >= 128) {
                    available = false;
                    continue;
                }
                int32_t block;
                if (!tree_get(context, context->pcs->clearance, scan_x, scan_y, scan_z, &block)) {
                    return false;
                }
                bool accepted;
                if (!tree_air_or_leaf(context, block, context->pcs->clearance_static,
                                      context->pcs->clearance_field, &accepted))
                    return false;
                if (!accepted) {
                    available = false;
                }
            }
        }
        /* This outer loop has no available guard in yh/pn or k/h. */
    }
    *clear = available;
    return true;
}

static bool tree_broadleaf(tree_context *context, int32_t x, int32_t y, int32_t z, bool birch,
                           bool *result)
{
    int32_t draw;
    if (!tree_bounded(context, 2, context->random, 3, &draw)) {
        return false;
    }
    const int32_t height = mc_java_i32_add(draw, birch ? 5 : 4);
    if (y < 1 || mc_java_i32_add(mc_java_i32_add(y, height), 1) > 128) {
        *result = false;
        return true;
    }
    bool clear;
    if (!tree_broadleaf_space(context, x, y, z, height, &clear)) {
        return false;
    }
    if (!clear) {
        *result = false;
        return true;
    }
    bool soil_accepted;
    if (!tree_soil(context, x, y, z, height, &soil_accepted)) {
        return false;
    }
    if (!soil_accepted) {
        *result = false;
        return true;
    }
    const int32_t top = mc_java_i32_add(y, height);
    for (int32_t leaf_y = mc_java_i32_add(mc_java_i32_sub(y, 3), height); leaf_y <= top;
         leaf_y = mc_java_i32_add(leaf_y, 1)) {
        const int32_t from_top = mc_java_i32_sub(leaf_y, top);
        /* Successful admission proves from_top in [-3, 0]; C99 division has
         * the same truncation toward zero as this original idiv by two. */
        const int32_t radius = 1 - from_top / 2;
        for (int32_t leaf_x = mc_java_i32_sub(x, radius); leaf_x <= mc_java_i32_add(x, radius);
             leaf_x = mc_java_i32_add(leaf_x, 1)) {
            const int32_t offset_x = mc_java_i32_sub(leaf_x, x);
            for (int32_t leaf_z = mc_java_i32_sub(z, radius); leaf_z <= mc_java_i32_add(z, radius);
                 leaf_z = mc_java_i32_add(leaf_z, 1)) {
                const int32_t offset_z = mc_java_i32_sub(leaf_z, z);
                if (tree_abs(offset_x) == radius && tree_abs(offset_z) == radius) {
                    int32_t corner;
                    if (!tree_bounded(context, 368, context->random, 2, &corner)) {
                        return false;
                    }
                    /* Even a top-layer corner consumes its draw before the
                     * from_top check rejects it. */
                    if (corner == 0 || from_top == 0) {
                        continue;
                    }
                }
                if (!tree_leaf(context, leaf_x, leaf_y, leaf_z, birch, 2)) {
                    return false;
                }
            }
        }
    }
    if (!tree_trunk(context, x, y, z, height, birch, 2)) {
        return false;
    }
    *result = true;
    return true;
}

static bool tree_oak_body(tree_context *context, int32_t x, int32_t y, int32_t z, bool *result)
{
    return tree_broadleaf(context, x, y, z, false, result);
}

static bool tree_birch_body(tree_context *context, int32_t x, int32_t y, int32_t z, bool *result)
{
    return tree_broadleaf(context, x, y, z, true, result);
}

static bool tree_conifer_space(tree_context *context, int32_t x, int32_t y, int32_t z,
                               int32_t height, int32_t foliage_start, int32_t maximum_radius,
                               bool *clear)
{
    bool available = true;
    const int32_t ceiling = mc_java_i32_add(mc_java_i32_add(y, 1), height);
    for (int32_t scan_y = y; scan_y <= ceiling && available; scan_y = mc_java_i32_add(scan_y, 1)) {
        const int32_t radius = mc_java_i32_sub(scan_y, y) < foliage_start ? 0 : maximum_radius;
        for (int32_t scan_x = mc_java_i32_sub(x, radius);
             scan_x <= mc_java_i32_add(x, radius) && available;
             scan_x = mc_java_i32_add(scan_x, 1)) {
            for (int32_t scan_z = mc_java_i32_sub(z, radius);
                 scan_z <= mc_java_i32_add(z, radius) && available;
                 scan_z = mc_java_i32_add(scan_z, 1)) {
                if (scan_y < 0 || scan_y >= 128) {
                    available = false;
                    continue;
                }
                int32_t block;
                if (!tree_get(context, context->pcs->clearance, scan_x, scan_y, scan_z, &block)) {
                    return false;
                }
                bool accepted;
                if (!tree_air_or_leaf(context, block, context->pcs->clearance_static,
                                      context->pcs->clearance_field, &accepted))
                    return false;
                if (!accepted) {
                    available = false;
                }
            }
        }
    }
    *clear = available;
    return true;
}

static bool tree_conifer_layer(tree_context *context, int32_t x, int32_t y, int32_t z,
                               int32_t radius)
{
    for (int32_t leaf_x = mc_java_i32_sub(x, radius); leaf_x <= mc_java_i32_add(x, radius);
         leaf_x = mc_java_i32_add(leaf_x, 1)) {
        const int32_t offset_x = mc_java_i32_sub(leaf_x, x);
        for (int32_t leaf_z = mc_java_i32_sub(z, radius); leaf_z <= mc_java_i32_add(z, radius);
             leaf_z = mc_java_i32_add(leaf_z, 1)) {
            const int32_t offset_z = mc_java_i32_sub(leaf_z, z);
            if (tree_abs(offset_x) == radius && tree_abs(offset_z) == radius && radius > 0) {
                continue;
            }
            if (!tree_leaf(context, leaf_x, y, leaf_z, true, 1)) {
                return false;
            }
        }
    }
    return true;
}

static bool tree_taiga_tall_body(tree_context *context, int32_t x, int32_t y, int32_t z,
                                 bool *result)
{
    int32_t height_draw;
    if (!tree_bounded(context, 2, context->random, 5, &height_draw)) {
        return false;
    }
    const int32_t height = mc_java_i32_add(height_draw, 7);
    int32_t start_draw;
    if (!tree_bounded(context, 14, context->random, 2, &start_draw)) {
        return false;
    }
    const int32_t foliage_start = mc_java_i32_sub(mc_java_i32_sub(height, start_draw), 3);
    const int32_t foliage_height = mc_java_i32_sub(height, foliage_start);
    int32_t radius_draw;
    if (!tree_bounded(context, 35, context->random, mc_java_i32_add(foliage_height, 1),
                      &radius_draw)) {
        return false;
    }
    const int32_t maximum_radius = mc_java_i32_add(1, radius_draw);
    if (y < 1 || mc_java_i32_add(mc_java_i32_add(y, height), 1) > 128) {
        *result = false;
        return true;
    }
    bool clear;
    if (!tree_conifer_space(context, x, y, z, height, foliage_start, maximum_radius, &clear)) {
        return false;
    }
    if (!clear) {
        *result = false;
        return true;
    }
    bool soil_accepted;
    if (!tree_soil(context, x, y, z, height, &soil_accepted)) {
        return false;
    }
    if (!soil_accepted) {
        *result = false;
        return true;
    }
    int32_t radius = 0;
    const int32_t foliage_bottom = mc_java_i32_add(y, foliage_start);
    for (int32_t leaf_y = mc_java_i32_add(y, height); leaf_y >= foliage_bottom;
         leaf_y = mc_java_i32_sub(leaf_y, 1)) {
        if (!tree_conifer_layer(context, x, leaf_y, z, radius)) {
            return false;
        }
        if (radius >= 1 && leaf_y == mc_java_i32_add(foliage_bottom, 1)) {
            radius = mc_java_i32_sub(radius, 1);
        } else if (radius < maximum_radius) {
            radius = mc_java_i32_add(radius, 1);
        }
    }
    if (!tree_trunk(context, x, y, z, mc_java_i32_sub(height, 1), true, 1)) {
        return false;
    }
    *result = true;
    return true;
}

static bool tree_taiga_dense_body(tree_context *context, int32_t x, int32_t y, int32_t z,
                                  bool *result)
{
    int32_t height_draw;
    if (!tree_bounded(context, 2, context->random, 4, &height_draw)) {
        return false;
    }
    const int32_t height = mc_java_i32_add(height_draw, 6);
    int32_t start_draw;
    if (!tree_bounded(context, 13, context->random, 2, &start_draw)) {
        return false;
    }
    const int32_t foliage_start = mc_java_i32_add(1, start_draw);
    const int32_t foliage_height = mc_java_i32_sub(height, foliage_start);
    int32_t radius_draw;
    if (!tree_bounded(context, 29, context->random, 2, &radius_draw)) {
        return false;
    }
    const int32_t maximum_radius = mc_java_i32_add(2, radius_draw);
    if (y < 1 || mc_java_i32_add(mc_java_i32_add(y, height), 1) > 128) {
        *result = false;
        return true;
    }
    bool clear;
    if (!tree_conifer_space(context, x, y, z, height, foliage_start, maximum_radius, &clear)) {
        return false;
    }
    if (!clear) {
        *result = false;
        return true;
    }
    bool soil_accepted;
    if (!tree_soil(context, x, y, z, height, &soil_accepted)) {
        return false;
    }
    if (!soil_accepted) {
        *result = false;
        return true;
    }
    int32_t radius;
    if (!tree_bounded(context, 290, context->random, 2, &radius)) {
        return false;
    }
    int32_t threshold = 1;
    int32_t reset_radius = 0;
    for (int32_t layer = 0; layer <= foliage_height; layer = mc_java_i32_add(layer, 1)) {
        const int32_t leaf_y = mc_java_i32_sub(mc_java_i32_add(y, height), layer);
        if (!tree_conifer_layer(context, x, leaf_y, z, radius)) {
            return false;
        }
        if (radius >= threshold) {
            radius = reset_radius;
            reset_radius = 1;
            threshold = mc_java_i32_add(threshold, 1);
            if (threshold > maximum_radius) {
                threshold = maximum_radius;
            }
        } else {
            radius = mc_java_i32_add(radius, 1);
        }
    }
    int32_t shortening;
    if (!tree_bounded(context, 486, context->random, 3, &shortening)) {
        return false;
    }
    if (!tree_trunk(context, x, y, z, mc_java_i32_sub(height, shortening), true, 1)) {
        return false;
    }
    *result = true;
    return true;
}

typedef bool (*tree_small_body)(tree_context *, int32_t, int32_t, int32_t, bool *);
static mc_world_storage_status tree_small_entry(unsigned int shape, tree_small_body body,
                                                mc_world_runtime *world, mc_java_random *random,
                                                int32_t x, int32_t y, int32_t z, bool *out,
                                                mc_world_tree_error *error)
{
    mc_world_tree_error local = tree_empty_error();
    tree_context context = {world, random, &local, &small_pcs[shape], false};
    if (out == NULL) {
        tree_fail(&context, MC_WORLD_STORAGE_INVALID_ARGUMENT, "Tree requires result", true);
        return tree_finish(&context, false, error);
    }
    if (!tree_enter(&context))
        return tree_finish(&context, false, error);
    const bool client = world->classes->side == MC_WORLD_INFO_CLIENT;
    local.feature_class = client ? client_classes[shape] : server_classes[shape];
    tree_stage(&context, client ? "a(Lfd;Ljava/util/Random;III)Z" : "a(Ldj;Ljava/util/Random;III)Z",
               0);
    bool result;
    const bool completed = body(&context, x, y, z, &result);
    if (completed)
        *out = result;
    mc_world_runtime_call_leave(world);
    return tree_finish(&context, completed, error);
}
#define TREE_SMALL_ENTRY(name, index)                                                              \
    mc_world_storage_status mc_world_tree_##name(mc_world_runtime *world, mc_java_random *random,  \
                                                 int32_t x, int32_t y, int32_t z, bool *out,       \
                                                 mc_world_tree_error *error)                       \
    {                                                                                              \
        return tree_small_entry(index, tree_##name##_body, world, random, x, y, z, out, error);    \
    }
TREE_SMALL_ENTRY(oak, 0)
TREE_SMALL_ENTRY(birch, 1)
TREE_SMALL_ENTRY(taiga_tall, 2)
TREE_SMALL_ENTRY(taiga_dense, 3)
#undef TREE_SMALL_ENTRY

static const int32_t big_axes[6] = {2, 0, 0, 1, 2, 1};

void mc_world_tree_limits_default(mc_world_tree_limits *out)
{
    if (out != NULL)
        *out = (mc_world_tree_limits){1024u * 1024u};
}
static bool big_rows_allocate(mc_world_tree_big_oak *state, int32_t count, unsigned int pc,
                              int32_t (**out)[4])
{
    tree_context *context = state->context;
    tree_stage(context, "a()V", pc);
    if (count < 0)
        return tree_fail(context, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                         "Big oak negative array dimension", false);
    if (count == 0) {
        *out = NULL;
        return true;
    }
    if ((size_t)count > SIZE_MAX / sizeof(int32_t[4]))
        return tree_fail(context, MC_WORLD_STORAGE_LIMIT, "Big oak coordinate size overflow", true);
    const size_t bytes = (size_t)count * sizeof(int32_t[4]);
    if (state->allocation_bytes > state->max_allocation_bytes ||
        bytes > state->max_allocation_bytes - state->allocation_bytes)
        return tree_fail(context, MC_WORLD_STORAGE_LIMIT, "Big oak coordinate allocation limit",
                         true);
    int32_t(*rows)[4] = calloc((size_t)count, sizeof(*rows));
    if (rows == NULL)
        return tree_fail(context, MC_WORLD_STORAGE_ALLOCATION_FAILED,
                         "Big oak coordinate allocation", true);
    state->allocation_bytes += bytes;
    *out = rows;
    return true;
}
static void big_rows_free(mc_world_tree_big_oak *state, int32_t (*rows)[4], int32_t count)
{
    if (rows != NULL) {
        state->allocation_bytes -= (size_t)count * sizeof(*rows);
        free(rows);
    }
}
static int32_t big_major_axis(const int32_t start[3], const int32_t end[3], int32_t delta[3])
{
    int32_t major = 0;
    for (int32_t axis = 0; axis < 3; axis = (int32_t)mc_java_i8_narrow(mc_java_i32_add(axis, 1))) {
        delta[axis] = mc_java_i32_sub(end[axis], start[axis]);
        if (tree_abs(delta[axis]) > tree_abs(delta[major])) {
            major = axis;
        }
    }
    return major;
}

/* Original a([I[I)I. Clearance floors only the two secondary coordinates;
 * its primary coordinate is wrapped integer addition without half rounding. */
static bool big_check_line(mc_world_tree_big_oak *state, const int32_t start[3],
                           const int32_t end[3], int32_t *obstruction)
{
    int32_t delta[3] = {0, 0, 0};
    const int32_t major = big_major_axis(start, end, delta);
    if (delta[major] == 0) {
        *obstruction = -1;
        return true;
    }
    if (!tree_fp(state->context, "a([I[I)I", 125))
        return false;
    const int32_t side_a = big_axes[major];
    const int32_t side_b = big_axes[major + 3];
    const int32_t direction = delta[major] > 0 ? 1 : -1;
    const double slope_a = (double)delta[side_a] / (double)delta[major];
    const double slope_b = (double)delta[side_b] / (double)delta[major];
    int32_t point[3] = {0, 0, 0};
    int32_t offset = 0;
    const int32_t stop = mc_java_i32_add(delta[major], direction);
    while (offset != stop) {
        point[major] = mc_java_i32_add(start[major], offset);
        const double displacement_a = (double)offset * slope_a;
        const double coordinate_a = (double)start[side_a] + displacement_a;
        point[side_a] = mc_math_floor_double(coordinate_a);
        const double displacement_b = (double)offset * slope_b;
        const double coordinate_b = (double)start[side_b] + displacement_b;
        point[side_b] = mc_math_floor_double(coordinate_b);
        int32_t block;
        if (!tree_get(state->context, 245, point[0], point[1], point[2], &block)) {
            return false;
        }
        if (block != 0 && block != 18) {
            break;
        }
        offset = mc_java_i32_add(offset, direction);
    }
    *obstruction = offset == stop ? -1 : tree_abs(offset);
    return true;
}

/* Original a([I[II)V. Logs are written without a preceding block query.
 * All three coordinates use the original floor(value + 0.5) sequence. */
static bool big_write_line(mc_world_tree_big_oak *state, const int32_t start[3],
                           const int32_t end[3], int32_t block)
{
    int32_t delta[3] = {0, 0, 0};
    const int32_t major = big_major_axis(start, end, delta);
    if (delta[major] == 0) {
        return true;
    }
    if (!tree_fp(state->context, "a([I[II)V", 132))
        return false;
    const int32_t side_a = big_axes[major];
    const int32_t side_b = big_axes[major + 3];
    const int32_t direction = delta[major] > 0 ? 1 : -1;
    const double slope_a = (double)delta[side_a] / (double)delta[major];
    const double slope_b = (double)delta[side_b] / (double)delta[major];
    int32_t point[3] = {0, 0, 0};
    const int32_t stop = mc_java_i32_add(delta[major], direction);
    for (int32_t offset = 0; offset != stop; offset = mc_java_i32_add(offset, direction)) {
        const int32_t coordinate_major = mc_java_i32_add(start[major], offset);
        const double rounded_major = (double)coordinate_major + 0.5;
        point[major] = mc_math_floor_double(rounded_major);
        const double displacement_a = (double)offset * slope_a;
        const double coordinate_a = (double)start[side_a] + displacement_a;
        const double rounded_a = coordinate_a + 0.5;
        point[side_a] = mc_math_floor_double(rounded_a);
        const double displacement_b = (double)offset * slope_b;
        const double coordinate_b = (double)start[side_b] + displacement_b;
        const double rounded_b = coordinate_b + 0.5;
        point[side_b] = mc_math_floor_double(rounded_b);
        if (!tree_set(state->context, 272, point[0], point[1], point[2], block, false, 0)) {
            return false;
        }
    }
    return true;
}

static float big_layer_size(mc_world_tree_big_oak *state, int32_t layer)
{
    const float float_height = (float)state->height_limit;
    const double lower_boundary = (double)float_height * 0.3;
    if ((double)layer < lower_boundary) {
        return -1.618f;
    }
    const float half_height = (float)state->height_limit / 2.0f;
    const float repeated_half_height = (float)state->height_limit / 2.0f;
    const float offset = repeated_half_height - (float)layer;
    float radius;
    if (offset == 0.0f) {
        radius = half_height;
    } else if (fabsf(offset) >= half_height) {
        radius = 0.0f;
    } else {
        const double half_squared = pow((double)fabsf(half_height), 2.0);
        const double offset_squared = pow((double)fabsf(offset), 2.0);
        const double difference = half_squared - offset_squared;
        const double root = sqrt(difference);
        radius = (float)root;
    }
    const float scaled_radius = radius * 0.5f;
    return scaled_radius;
}

static float big_leaf_size(mc_world_tree_big_oak *state, int32_t layer)
{
    if (layer < 0 || layer >= state->leaf_distance) {
        return -1.0f;
    }
    if (layer == 0 || layer == mc_java_i32_sub(state->leaf_distance, 1)) {
        return 2.0f;
    }
    return 3.0f;
}

static bool big_leaf_disc(mc_world_tree_big_oak *state, int32_t x, int32_t y, int32_t z,
                          float radius, int32_t axis, int32_t block)
{
    if (!tree_fp(state->context, "a(IIIFBI)V", 6))
        return false;
    const double expanded_radius = (double)radius + 0.618;
    const int32_t bound = mc_java_double_to_i32(expanded_radius);
    const int32_t side_a = big_axes[axis];
    const int32_t side_b = big_axes[axis + 3];
    const int32_t center[3] = {x, y, z};
    int32_t point[3] = {0, 0, 0};
    point[axis] = center[axis];
    for (int32_t a = mc_java_i32_neg(bound); a <= bound; a = mc_java_i32_add(a, 1)) {
        point[side_a] = mc_java_i32_add(center[side_a], a);
        for (int32_t b = mc_java_i32_neg(bound); b <= bound; b = mc_java_i32_add(b, 1)) {
            const double centered_a = (double)tree_abs(a) + 0.5;
            const double squared_a = pow(centered_a, 2.0);
            const double centered_b = (double)tree_abs(b) + 0.5;
            const double squared_b = pow(centered_b, 2.0);
            const double squared_distance = squared_a + squared_b;
            const double distance = sqrt(squared_distance);
            /* dcmpl/ifle admits unordered comparison; skip only strict >. */
            if (distance > (double)radius) {
                continue;
            }
            point[side_b] = mc_java_i32_add(center[side_b], b);
            int32_t current;
            if (!tree_get(state->context, 196, point[0], point[1], point[2], &current)) {
                return false;
            }
            if (current != 0 && current != 18) {
                continue;
            }
            if (!tree_set(state->context, 237, point[0], point[1], point[2], block, false, 0)) {
                return false;
            }
        }
    }
    return true;
}

static bool big_leaf_node(mc_world_tree_big_oak *state, int32_t x, int32_t y, int32_t z)
{
    const int32_t stop = mc_java_i32_add(y, state->leaf_distance);
    for (int32_t layer_y = y; layer_y < stop; layer_y = mc_java_i32_add(layer_y, 1)) {
        const float radius = big_leaf_size(state, mc_java_i32_sub(layer_y, y));
        if (!big_leaf_disc(state, x, layer_y, z, radius, 1, 18)) {
            return false;
        }
    }
    return true;
}

static bool big_plan(mc_world_tree_big_oak *state)
{
    if (!tree_fp(state->context, "a()V", 10))
        return false;
    const double scaled_trunk = (double)state->height_limit * state->height_attenuation;
    state->trunk_height = mc_java_double_to_i32(scaled_trunk);
    if (state->trunk_height >= state->height_limit) {
        state->trunk_height = mc_java_i32_sub(state->height_limit, 1);
    }
    const double scaled_density = state->leaf_density * (double)state->height_limit;
    const double density_fraction = scaled_density / 13.0;
    const double density_square = pow(density_fraction, 2.0);
    const double density_count = 1.382 + density_square;
    int32_t per_layer = mc_java_double_to_i32(density_count);
    if (per_layer < 1) {
        per_layer = 1;
    }
    const int32_t capacity = mc_java_i32_mul(per_layer, state->height_limit);
    int32_t(*candidate)[4] = NULL;
    if (!big_rows_allocate(state, capacity, 76, &candidate))
        return false;
    int32_t sample_y =
        mc_java_i32_sub(mc_java_i32_add(state->base[1], state->height_limit), state->leaf_distance);
    int32_t used = 1;
    const int32_t trunk_top = mc_java_i32_add(state->base[1], state->trunk_height);
    int32_t layer = mc_java_i32_sub(sample_y, state->base[1]);
    if (capacity == 0) {
        tree_stage(state->context, "a()V", 126);
        tree_fail(state->context, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                  "Big oak first row outside zero-length array", false);
        return false;
    }
    candidate[0][0] = state->base[0];
    candidate[0][1] = sample_y;
    candidate[0][2] = state->base[2];
    candidate[0][3] = trunk_top;
    sample_y = mc_java_i32_sub(sample_y, 1);
    while (layer >= 0) {
        const float profile = big_layer_size(state, layer);
        if (profile < 0.0f) {
            sample_y = mc_java_i32_sub(sample_y, 1);
            layer = mc_java_i32_sub(layer, 1);
            continue;
        }
        const double center_offset = 0.5;
        for (int32_t attempt = 0; attempt < per_layer; attempt = mc_java_i32_add(attempt, 1)) {
            const float radial_draw = mc_java_random_next_float(&state->random);
            const double radial_fraction = (double)radial_draw + 0.328;
            const double profile_radius = (double)profile * radial_fraction;
            const double radius = state->width_scale * profile_radius;
            const float angle_draw = mc_java_random_next_float(&state->random);
            const double turns = (double)angle_draw * 2.0;
            const double angle = turns * 3.14159;
            const double sine = sin(angle);
            const double offset_x = radius * sine;
            const double position_x = offset_x + (double)state->base[0];
            const double rounded_x = position_x + center_offset;
            const int32_t node_x = mc_math_floor_double(rounded_x);
            const double cosine = cos(angle);
            const double offset_z = radius * cosine;
            const double position_z = offset_z + (double)state->base[2];
            const double rounded_z = position_z + center_offset;
            const int32_t node_z = mc_math_floor_double(rounded_z);
            const int32_t node[3] = {node_x, sample_y, node_z};
            const int32_t leaf_top[3] = {node_x, mc_java_i32_add(sample_y, state->leaf_distance),
                                         node_z};
            int32_t obstruction;
            if (!big_check_line(state, node, leaf_top, &obstruction)) {
                big_rows_free(state, candidate, capacity);
                return false;
            }
            if (obstruction != -1) {
                continue;
            }
            int32_t branch_start[3] = {state->base[0], state->base[1], state->base[2]};
            const int32_t dx = mc_java_i32_sub(state->base[0], node[0]);
            const double squared_dx = pow((double)tree_abs(dx), 2.0);
            const int32_t dz = mc_java_i32_sub(state->base[2], node[2]);
            const double squared_dz = pow((double)tree_abs(dz), 2.0);
            const double squared_distance = squared_dx + squared_dz;
            const double distance = sqrt(squared_distance);
            const double branch_drop = distance * state->branch_slope;
            const double branch_y = (double)node[1] - branch_drop;
            if (branch_y > (double)trunk_top) {
                branch_start[1] = trunk_top;
            } else {
                branch_start[1] = mc_java_double_to_i32(branch_y);
            }
            if (!big_check_line(state, branch_start, node, &obstruction)) {
                big_rows_free(state, candidate, capacity);
                return false;
            }
            if (obstruction != -1) {
                continue;
            }
            if (used < 0 || used >= capacity) {
                tree_stage(state->context, "a()V", 490);
                tree_fail(state->context, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                          "Big oak candidate row outside array", false);
                big_rows_free(state, candidate, capacity);
                return false;
            }
            candidate[used][0] = node_x;
            candidate[used][1] = sample_y;
            candidate[used][2] = node_z;
            candidate[used][3] = branch_start[1];
            used = mc_java_i32_add(used, 1);
        }
        sample_y = mc_java_i32_sub(sample_y, 1);
        layer = mc_java_i32_sub(layer, 1);
    }
    int32_t(*retained)[4] = NULL;
    if (!big_rows_allocate(state, used, 542, &retained)) {
        big_rows_free(state, candidate, capacity);
        return false;
    }
    /* Neither candidate nor retained is world storage. They own only the
     * original feature's ordered branch-coordinate scratch. */
    memcpy(retained, candidate, (size_t)used * sizeof(*retained));
    big_rows_free(state, candidate, capacity);
    big_rows_free(state, state->nodes, state->node_count);
    state->nodes = retained;
    state->node_count = used;
    return true;
}

static bool big_leaves(mc_world_tree_big_oak *state)
{
    const int32_t count = state->node_count;
    for (int32_t index = 0; index < count; index = mc_java_i32_add(index, 1)) {
        const int32_t x = state->nodes[index][0];
        const int32_t y = state->nodes[index][1];
        const int32_t z = state->nodes[index][2];
        if (!big_leaf_node(state, x, y, z)) {
            return false;
        }
    }
    return true;
}

static bool big_trunk(mc_world_tree_big_oak *state)
{
    const int32_t x = state->base[0];
    const int32_t y = state->base[1];
    const int32_t top = mc_java_i32_add(state->base[1], state->trunk_height);
    const int32_t z = state->base[2];
    int32_t start[3] = {x, y, z};
    int32_t end[3] = {x, top, z};
    if (!big_write_line(state, start, end, 17)) {
        return false;
    }
    if (state->trunk_size == 2) {
        start[0] = mc_java_i32_add(start[0], 1);
        end[0] = mc_java_i32_add(end[0], 1);
        if (!big_write_line(state, start, end, 17)) {
            return false;
        }
        start[2] = mc_java_i32_add(start[2], 1);
        end[2] = mc_java_i32_add(end[2], 1);
        if (!big_write_line(state, start, end, 17)) {
            return false;
        }
        start[0] = mc_java_i32_add(start[0], -1);
        end[0] = mc_java_i32_add(end[0], -1);
        if (!big_write_line(state, start, end, 17)) {
            return false;
        }
    }
    return true;
}

static bool big_branch_needed(mc_world_tree_big_oak *state, int32_t height)
{
    const double threshold = (double)state->height_limit * 0.2;
    return !((double)height < threshold);
}

static bool big_branches(mc_world_tree_big_oak *state)
{
    const int32_t count = state->node_count;
    int32_t start[3] = {state->base[0], state->base[1], state->base[2]};
    for (int32_t index = 0; index < count; index = mc_java_i32_add(index, 1)) {
        const int32_t *node = state->nodes[index];
        const int32_t end[3] = {node[0], node[1], node[2]};
        start[1] = node[3];
        const int32_t height = mc_java_i32_sub(start[1], state->base[1]);
        if (big_branch_needed(state, height)) {
            if (!big_write_line(state, start, end, 17)) {
                return false;
            }
        }
    }
    return true;
}

static bool big_location(mc_world_tree_big_oak *state, bool *accepted)
{
    tree_stage(state->context, "e()Z", 93);
    const int32_t start[3] = {state->base[0], state->base[1], state->base[2]};
    const int32_t end[3] = {
        state->base[0], mc_java_i32_sub(mc_java_i32_add(state->base[1], state->height_limit), 1),
        state->base[2]};
    int32_t below;
    if (!tree_get(state->context, 93, state->base[0], mc_java_i32_sub(state->base[1], 1),
                  state->base[2], &below)) {
        return false;
    }
    if (below != 2 && below != 3) {
        *accepted = false;
        return true;
    }
    int32_t obstruction;
    if (!big_check_line(state, start, end, &obstruction)) {
        return false;
    }
    if (obstruction == -1) {
        *accepted = true;
        return true;
    }
    if (obstruction < 6) {
        *accepted = false;
        return true;
    }
    state->height_limit = obstruction;
    *accepted = true;
    return true;
}

mc_world_storage_status mc_world_tree_big_oak_create(mc_world_runtime *world,
                                                     const mc_world_tree_limits *limits,
                                                     mc_world_tree_big_oak **out,
                                                     mc_world_tree_error *error)
{
    mc_world_tree_error local = tree_empty_error();
    tree_context context = {world, NULL, &local, NULL, true};
    if (out == NULL) {
        tree_fail(&context, MC_WORLD_STORAGE_INVALID_ARGUMENT, "Big oak requires owner result",
                  true);
        return tree_finish(&context, false, error);
    }
    if (!tree_enter(&context))
        return tree_finish(&context, false, error);
    local.feature_class = world->classes->side == MC_WORLD_INFO_CLIENT ? "ih" : "fn";
    tree_stage(&context, "<init>()V", 0);
    const size_t limit = limits == NULL ? 1024u * 1024u : limits->max_allocation_bytes;
    if (sizeof(mc_world_tree_big_oak) > limit) {
        tree_fail(&context, MC_WORLD_STORAGE_LIMIT, "Big oak owner allocation limit", true);
        mc_world_runtime_call_leave(world);
        return tree_finish(&context, false, error);
    }
    mc_world_tree_big_oak *state = calloc(1, sizeof(*state));
    if (state == NULL) {
        tree_fail(&context, MC_WORLD_STORAGE_ALLOCATION_FAILED, "Big oak owner allocation", true);
        mc_world_runtime_call_leave(world);
        return tree_finish(&context, false, error);
    }
    tree_stage(&context, "<init>()V", 9);
    local.profile_status = mc_world_runtime_construct_default_random(world, &state->random);
    if (local.profile_status != MC_JAVA_RANDOM_PROFILE_OK) {
        const mc_world_storage_error cause = *mc_world_runtime_last_error(world);
        tree_storage_fail(&context, &cause);
        free(state);
        mc_world_runtime_call_leave(world);
        return tree_finish(&context, false, error);
    }
    state->classes = world->classes;
    state->height_attenuation = 0.618;
    state->unused_h = 1.0;
    state->branch_slope = 0.381;
    state->width_scale = 1.0;
    state->leaf_density = 1.0;
    state->trunk_size = 1;
    state->height_random_limit = 12;
    state->leaf_distance = 4;
    state->allocation_bytes = sizeof(*state);
    state->max_allocation_bytes = limit;
    *out = state;
    mc_world_runtime_call_leave(world);
    return tree_finish(&context, true, error);
}
mc_world_storage_status mc_world_tree_big_oak_scale(mc_world_tree_big_oak *state, double height,
                                                    double width, double density,
                                                    mc_world_tree_error *error)
{
    mc_world_tree_error local = tree_empty_error();
    tree_context context = {NULL, NULL, &local, NULL, true};
    if (state == NULL || state->entered) {
        tree_fail(&context, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                  "Big oak scale requires quiescent owner", true);
        return tree_finish(&context, false, error);
    }
    local.feature_class = state->classes->side == MC_WORLD_INFO_CLIENT ? "ih" : "fn";
    if (!tree_fp(&context, "a(DDD)V", 5))
        return tree_finish(&context, false, error);
    const double scaled_height = height * 12.0;
    state->height_random_limit = mc_java_double_to_i32(scaled_height);
    if (height > 0.5)
        state->leaf_distance = 5;
    state->width_scale = width;
    state->leaf_density = density;
    return tree_finish(&context, true, error);
}
void mc_world_tree_big_oak_dispose(mc_world_tree_big_oak *state)
{
    if (state == NULL || state->entered)
        return; /* Requires quiescent owner. */
    big_rows_free(state, state->nodes, state->node_count);
    mc_world_runtime_binding_release(state->world);
    free(state);
}
static bool big_place_body(mc_world_tree_big_oak *state, tree_context *context, int32_t x,
                           int32_t y, int32_t z, bool *result)
{
    mc_world_runtime *world = context->world;
    const char *entry = world->classes->side == MC_WORLD_INFO_CLIENT
                            ? "a(Lfd;Ljava/util/Random;III)Z"
                            : "a(Ldj;Ljava/util/Random;III)Z";
    context->error->entry_bytecode = 2;
    tree_stage(context, entry, 2);
    if (state->world != world) {
        if (!mc_world_runtime_binding_retain(world)) {
            const mc_world_storage_error cause = *mc_world_runtime_last_error(world);
            context->error->native_boundary = true;
            return tree_storage_fail(context, &cause);
        }
        mc_world_runtime *old = state->world;
        state->world = world;
        mc_world_runtime_binding_release(old);
    }
    context->error->entry_bytecode = 6;
    tree_stage(context, entry, 6);
    if (context->random == NULL)
        return tree_fail(context, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                         "Big oak reached NULL provider Random", false);
    const int64_t seed = mc_java_random_next_i64(context->random);
    mc_java_random_reseed(&state->random, seed);
    state->base[0] = x;
    state->base[1] = y;
    state->base[2] = z;
    if (state->height_limit == 0) {
        int32_t draw;
        context->error->entry_bytecode = 60;
        if (!tree_bounded(context, 60, &state->random, state->height_random_limit, &draw))
            return false;
        state->height_limit = mc_java_i32_add(5, draw);
    }
    bool accepted;
    context->error->entry_bytecode = 68;
    if (!big_location(state, &accepted))
        return false;
    if (!accepted) {
        *result = false;
        return true;
    }
    context->error->entry_bytecode = 77;
    if (!big_plan(state))
        return false;
    context->error->entry_bytecode = 81;
    if (!big_leaves(state))
        return false;
    context->error->entry_bytecode = 85;
    if (!big_trunk(state))
        return false;
    context->error->entry_bytecode = 89;
    if (!big_branches(state))
        return false;
    *result = true;
    return true;
}
mc_world_storage_status mc_world_tree_big_oak_place(mc_world_tree_big_oak *state,
                                                    mc_world_runtime *world, mc_java_random *random,
                                                    int32_t x, int32_t y, int32_t z, bool *out,
                                                    mc_world_tree_error *error)
{
    mc_world_tree_error local = tree_empty_error();
    tree_context context = {world, random, &local, NULL, true};
    if (state == NULL || out == NULL || state->entered) {
        tree_fail(&context, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                  "Big oak requires quiescent owner and result", true);
        return tree_finish(&context, false, error);
    }
    if (!tree_enter(&context))
        return tree_finish(&context, false, error);
    if (world->classes != state->classes) {
        tree_fail(&context, MC_WORLD_STORAGE_SESSION_MISMATCH,
                  "Big oak World class context mismatch", true);
        mc_world_runtime_call_leave(world);
        return tree_finish(&context, false, error);
    }
    local.feature_class = state->classes->side == MC_WORLD_INFO_CLIENT ? "ih" : "fn";
    state->entered = true;
    state->context = &context;
    bool result;
    const bool completed = big_place_body(state, &context, x, y, z, &result);
    if (completed)
        *out = result;
    state->context = NULL;
    state->entered = false;
    mc_world_runtime_call_leave(world);
    return tree_finish(&context, completed, error);
}
