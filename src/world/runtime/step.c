#include "mc/world/runtime/step.h"

#include "../../block/registry/registry_internal.h"
#include "../../block/registry/runtime_internal.h"
#include "collision/blocks_internal.h"
#include "mc/core/java/java.h"
#include "mutation/plants/plants_internal.h"

#include <float.h>
#include <string.h>

#if !defined(__SSE2__) && !defined(_M_X64)
#error "Block step arithmetic requires x86-64/SSE2"
#endif
#if defined(__FAST_MATH__) || (defined(__FINITE_MATH_ONLY__) && __FINITE_MATH_ONLY__ > 0) ||       \
    (defined(_M_FP_FAST) && _M_FP_FAST)
#error "Block step requires strict floating transformations"
#endif
#include <emmintrin.h>
_Static_assert(FLT_RADIX == 2 && FLT_MANT_DIG == 24 && DBL_MANT_DIG == 53 && FLT_EVAL_METHOD == 0,
               "Block step requires binary32/binary64 evaluation");
/* All 73 paired source Block classes were audited through their parent chains.
 * Only these four concrete families declare this exact overload. */
_Static_assert(MC_BLOCK_KIND_COUNT == 69, "audit new Block step inheritance");

void mc_world_step_error_init(mc_world_step_error *error) {
    if (error == NULL)
        return;
    memset(error, 0, sizeof(*error));
    error->particle_face = -1;
    mc_world_collision_error_init(&error->block_read);
    error->registry = error->block_read.nearby.query.registry;
    mc_world_mutation_error_init(&error->mutation);
}

static bool fail(mc_world_step_error *error, mc_world_storage_status status,
                 const char *operation) {
    error->status = status;
    error->operation = operation;
    return false;
}

static bool registry(mc_world_step_error *error, mc_registry_status status, const char *operation) {
    return status == MC_REGISTRY_OK ||
           fail(error,
                status == MC_REGISTRY_LIMIT               ? MC_WORLD_STORAGE_LIMIT
                : status == MC_REGISTRY_ALLOCATION_FAILED ? MC_WORLD_STORAGE_ALLOCATION_FAILED
                                                          : MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                operation);
}

/* Exact fd.g/dj.d: capture live m BEFORE the acquiring World block read,
 * index the retained array, then virtual opacity only for a nonnull element. */
static bool opaque(mc_world_runtime *world, int32_t x, int32_t y, int32_t z, bool *out,
                   mc_world_step_error *error) {
    mc_registry_block_m_view array;
    error->opaque_bytecode = 0;
    if (!registry(
            error,
            mc_registry_block_m_get(mc_world_runtime_registry(world), &array, &error->registry),
            "Step opaque Block.m"))
        return false;
    int32_t id;
    error->opaque_bytecode = 7;
    const mc_world_storage_status status =
        mc_world_collision_block_value_held(world, x, y, z, false, &id, &error->block_read);
    if (status != MC_WORLD_STORAGE_OK)
        return fail(error, status, "Step opaque actual World block read");
    const mc_block *block;
    error->opaque_bytecode = 10;
    if (!registry(error, mc_registry_block_m_at(&array, id, &block, &error->registry),
                  "Step opaque retained m element"))
        return false;
    error->opaque_bytecode = 22;
    *out = block != NULL && mc_registry_block_opaque(block);
    return true;
}

static double random_coordinate(int32_t coordinate, mc_java_random *random) {
    const float integer = _mm_cvtss_f32(_mm_cvtsi32_ss(_mm_setzero_ps(), coordinate));
    const float draw = mc_java_random_next_float(random);
    const float summed = _mm_cvtss_f32(_mm_add_ss(_mm_set_ss(integer), _mm_set_ss(draw)));
    return _mm_cvtsd_f64(_mm_cvtss_sd(_mm_setzero_pd(), _mm_set_ss(summed)));
}

static bool particles(mc_world_runtime *world, const mc_block *block, int32_t x, int32_t y,
                      int32_t z, mc_world_step_error *error) {
    error->particle_bytecode = 1;
    if (world == NULL)
        return fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                    "Redstone Ore particles null World Random receiver");
    mc_java_random *random = world->fields.r; /* One actual source receiver capture. */
    if (mc_world_runtime_call_enter(world, &error->cause) != MC_WORLD_STORAGE_OK)
        return fail(error, error->cause.status, "Redstone Ore actual GAME World");
    bool okay = false;
    if (mc_world_runtime_registry(world) != block->classes) {
        fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
             "Redstone Ore World/code Registry namespace");
        goto done;
    }
    for (int32_t face = 0; face < 6; ++face) {
        error->particle_face = face;
        error->particle_bytecode = 25;
        if (random == NULL) {
            fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Redstone Ore null captured Random");
            goto done;
        }
        if ((_mm_getcsr() & 0xffc0u) != 0x1f80u) {
            fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Redstone Ore particle FP admission");
            goto done;
        }
        double px = random_coordinate(x, random);
        double py = random_coordinate(y, random);
        double pz = random_coordinate(z, random);
        int32_t nx = x, ny = y, nz = z;
        static const unsigned int calls[6] = {67, 95, 123, 152, 181, 209};
        if (face == 0)
            ny = mc_java_i32_add(y, 1);
        else if (face == 1)
            ny = mc_java_i32_sub(y, 1);
        else if (face == 2)
            nz = mc_java_i32_add(z, 1);
        else if (face == 3)
            nz = mc_java_i32_sub(z, 1);
        else if (face == 4)
            nx = mc_java_i32_add(x, 1);
        else
            nx = mc_java_i32_sub(x, 1);
        bool closed;
        error->particle_bytecode = calls[face];
        if (!opaque(world, nx, ny, nz, &closed, error))
            goto done;
        if (!closed) {
            if (face == 0)
                py = (double)mc_java_i32_add(y, 1) + 0.0625;
            else if (face == 1)
                py = (double)y - 0.0625;
            else if (face == 2)
                pz = (double)mc_java_i32_add(z, 1) + 0.0625;
            else if (face == 3)
                pz = (double)z - 0.0625;
            else if (face == 4)
                px = (double)mc_java_i32_add(x, 1) + 0.0625;
            else
                px = (double)x - 0.0625;
        }
        /* Source compares py against ZERO, not y. Keep its wrapped +1 ints. */
        if (px < (double)x || px > (double)mc_java_i32_add(x, 1) || py < 0.0 ||
            py > (double)mc_java_i32_add(y, 1) || pz < (double)z ||
            pz > (double)mc_java_i32_add(z, 1)) {
            const mc_registry_string *name = NULL;
            error->particle_bytecode = 280;
            if (!registry(error,
                          mc_registry_runtime_literal_ascii(block->classes, "reddust", &name,
                                                            &error->registry),
                          "Redstone Ore particle code literal"))
                goto done;
            error->particle_bytecode = 291;
            if (!mc_world_mutation_particle(world, name, px, py, pz, 0.0, 0.0, 0.0,
                                            &error->mutation)) {
                fail(error, error->mutation.status, "Redstone Ore actual World particle dispatch");
                goto done;
            }
        }
    }
    okay = true;
done:
    mc_world_runtime_call_leave(world);
    return okay;
}

static bool redstone_ore(mc_world_runtime *world, const mc_block *block, int32_t x, int32_t y,
                         int32_t z, mc_world_step_error *error) {
    error->bytecode = 6;
    error->activation_bytecode = 6;
    if (!particles(world, block, x, y, z, error))
        return false;
    const mc_block *normal = NULL;
    error->activation_bytecode = 13;
    if (!registry(error,
                  mc_registry_block_static_get(block->classes, "aO", &normal, &error->registry),
                  "Redstone Ore inactive Block getstatic"))
        return false;
    error->activation_bytecode = 16;
    if (normal == NULL)
        return fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                    "Redstone Ore inactive static null bn receiver");
    if (block->fields.bn == normal->fields.bn) {
        const mc_block *active = NULL;
        error->activation_bytecode = 27;
        if (!registry(error,
                      mc_registry_block_static_get(block->classes, "aP", &active, &error->registry),
                      "Redstone Ore active Block getstatic"))
            return false;
        error->activation_bytecode = 30;
        if (active == NULL)
            return fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                        "Redstone Ore active static null bn receiver");
        bool ordinary;
        error->activation_bytecode = 33;
        if (mc_world_runtime_block_set(world, x, y, z, active->fields.bn, &ordinary,
                                       &error->mutation) != MC_WORLD_STORAGE_OK)
            return fail(error, error->mutation.status, "Redstone Ore notifying activation setter");
    }
    return true;
}

mc_world_storage_status mc_world_runtime_entity_step(mc_world_runtime *world, const mc_block *block,
                                                     int32_t x, int32_t y, int32_t z,
                                                     mc_entity *entity,
                                                     const mc_world_step_limits *limits,
                                                     mc_world_step_error *out) {
    mc_world_step_error frame;
    mc_world_step_error_init(&frame);
    const size_t maximum =
        limits == NULL ? MC_WORLD_STEP_DEFAULT_WRAPPED_CALLS : limits->max_wrapped_calls;
    for (;;) {
        frame.block = block;
        if (block == NULL || block->classes == NULL ||
            (unsigned int)block->kind >= (unsigned int)MC_BLOCK_KIND_COUNT) {
            fail(&frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                 "Block step null/unsupported concrete receiver");
            break;
        }
        const bool client = block->classes->options.side == MC_REGISTRY_CLIENT;
        if (block->kind == MC_BLOCK_STAIRS) {
            frame.block_class = client ? "ss" : "lu";
            frame.bytecode = 11;
            if (frame.wrapped_calls == maximum) {
                fail(&frame, MC_WORLD_STORAGE_LIMIT, "Stairs step pending wrapped-call ceiling");
                break;
            }
            ++frame.wrapped_calls;
            block = block->subtype.ss.a;
            continue;
        }
        if (block->kind == MC_BLOCK_REDSTONE_ORE) {
            frame.block_class = client ? "bs" : "az";
            (void)redstone_ore(world, block, x, y, z, &frame);
        } else if (block->kind == MC_BLOCK_FARMLAND) {
            frame.block_class = client ? "vl" : "no";
            frame.bytecode = 1;
            if (world == NULL)
                fail(&frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Farmland step null World");
            else if (mc_world_runtime_call_enter(world, &frame.cause) != MC_WORLD_STORAGE_OK)
                fail(&frame, frame.cause.status, "Farmland step actual GAME World");
            else {
                if (mc_world_runtime_registry(world) != block->classes)
                    fail(&frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                         "Farmland step World/code Registry namespace");
                else {
                    frame.bytecode = 5;
                    if (!mc_plant_farmland_contact(world, block, x, y, z, entity, &frame.mutation))
                        fail(&frame, frame.mutation.status, "Farmland actual step body");
                }
                mc_world_runtime_call_leave(world);
            }
        } else {
            frame.block_class = client ? "uu" : "na";
            frame.bytecode = 0; /* Leaves invokespecial at8 resolves this empty body. */
        }
        break;
    }
    const mc_world_storage_status status = frame.status;
    if (status == MC_WORLD_STORAGE_OK)
        mc_world_step_error_init(&frame);
    if (out != NULL)
        *out = frame;
    return status;
}
