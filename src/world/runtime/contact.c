#include "mc/world/runtime/contact.h"

#include "../../block/registry/registry_internal.h"
#include "../../entity/entity_internal.h"
#include "collision/blocks_internal.h"
#include "mutation/mutation_internal.h"
#include "runtime_internal.h"

#include <float.h>

#if !defined(__SSE2__) && !defined(_M_X64) && !(defined(_M_IX86_FP) && _M_IX86_FP >= 2)
#error "Block contact velocity arithmetic requires SSE2"
#endif
#if defined(__FAST_MATH__) || (defined(__FINITE_MATH_ONLY__) && __FINITE_MATH_ONLY__ > 0) ||       \
    (defined(_M_FP_FAST) && _M_FP_FAST)
#error "Block contact requires strict floating transformations"
#endif
#include <emmintrin.h>

_Static_assert(FLT_RADIX == 2 && DBL_MANT_DIG == 53 && DBL_MAX_EXP == 1024 && FLT_EVAL_METHOD == 0,
               "Block contact requires strict binary64 evaluation");
/* Complete paired parent-chain inventory: no current native kind derives
 * below the six contact owners. The other63 resolve the literal base return.
 * A new canonical kind requires that inheritance audit before extending this. */
_Static_assert(MC_BLOCK_KIND_COUNT == 69, "audit new Block contact inheritance");

mc_world_entity_contact_error mc_world_entity_contact_empty_error(void) {
    const mc_world_collision_error metadata = mc_world_collision_empty_error();
    return (mc_world_entity_contact_error){.registry = metadata.nearby.query.registry,
                                           .query = metadata.nearby.query,
                                           .metadata = metadata,
                                           .mutation = mc_world_mutation_empty_error(),
                                           .effect = mc_entity_effect_empty_error()};
}

static mc_world_storage_status fail(mc_world_entity_contact_error *frame,
                                    mc_world_storage_status status, const char *operation) {
    frame->status = status;
    frame->operation = operation;
    frame->cause = (mc_world_storage_error){status, operation, 0};
    return status;
}

static mc_world_storage_status entity_status(mc_entity_status status) {
    switch (status) {
        case MC_ENTITY_OK:
            return MC_WORLD_STORAGE_OK;
        case MC_ENTITY_INVALID_ARGUMENT:
            return MC_WORLD_STORAGE_INVALID_ARGUMENT;
        case MC_ENTITY_LIMIT:
            return MC_WORLD_STORAGE_LIMIT;
        case MC_ENTITY_ALLOCATION_FAILED:
            return MC_WORLD_STORAGE_ALLOCATION_FAILED;
        case MC_ENTITY_BUSY:
            return MC_WORLD_STORAGE_BUSY;
        default:
            return MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    }
}

static void stage(const mc_block *block, mc_world_entity_contact_error *frame, const char *client,
                  const char *server, unsigned int pc) {
    frame->block_class = block->classes->options.side == MC_REGISTRY_CLIENT ? client : server;
    frame->bytecode = pc;
}

static mc_entity_base_fields *fields(mc_entity *entity, mc_world_entity_contact_error *frame) {
    mc_entity_base_fields *const base = mc_entity_runtime_fields(entity);
    if (base == NULL) {
        frame->entity = (mc_entity_error){.status = MC_ENTITY_STATE_ERROR,
                                          .operation = "Block contact Entity receiver"};
        fail(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Block contact Entity receiver");
    }
    return base;
}

static mc_world_storage_status effect_result(mc_world_entity_contact_error *frame,
                                             mc_entity_status status, const char *operation) {
    return status == MC_ENTITY_OK ? MC_WORLD_STORAGE_OK
                                  : fail(frame, entity_status(status), operation);
}

static mc_world_storage_status portal(const mc_block *block, mc_entity *entity,
                                      mc_world_entity_contact_error *frame) {
    stage(block, frame, "ak", "z", 2);
    mc_entity_base_fields *base = fields(entity, frame);
    if (base == NULL)
        return frame->status;
    if (base->aH != NULL)
        return MC_WORLD_STORAGE_OK;
    frame->bytecode = 10;
    base = fields(entity, frame);
    if (base == NULL)
        return frame->status;
    if (base->aG != NULL)
        return MC_WORLD_STORAGE_OK;
    frame->bytecode = 18;
    return effect_result(frame, mc_entity_effect_contact_portal(entity, &frame->effect),
                         "Block portal Entity S/P");
}

static mc_world_storage_status soul_sand(const mc_block *block, mc_entity *entity,
                                         mc_world_entity_contact_error *frame) {
    stage(block, frame, "oa", "iu", 3);
    mc_entity_base_fields *base = fields(entity, frame);
    if (base == NULL)
        return frame->status;
    const double x = base->velocity_x;
    frame->bytecode = 9;
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u)
        return fail(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Block soul-sand FP admission");
    base->velocity_x = x * 0.4; /* dmul9, store10; never widened float0.4. */
    frame->bytecode = 16;
    base = fields(entity, frame);
    if (base == NULL)
        return frame->status;
    const double z = base->velocity_z;
    frame->bytecode = 22;
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u)
        return fail(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Block soul-sand FP admission");
    base->velocity_z = z * 0.4; /* dmul22, store23 follows the completed X store. */
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status guarded_plate(mc_world_runtime *world, const mc_block *block,
                                             int32_t x, int32_t y, int32_t z, bool detector,
                                             mc_world_entity_contact_error *frame) {
    stage(block, frame, detector ? "ph" : "bv", detector ? "jn" : "bc", 1);
    if (world == NULL)
        return fail(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Block contact World B receiver");
    if (world->construction.mode != MC_WORLD_RUNTIME_GAME)
        return fail(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Block contact GAME World");
    mc_world_storage_error child = {0};
    mc_world_storage_status status = mc_world_runtime_call_enter(world, &child);
    if (status != MC_WORLD_STORAGE_OK) {
        frame->status = status;
        frame->operation = child.operation;
        frame->cause = child;
        return status;
    }
    if (!world->fields.B) {
        frame->bytecode = 13;
        int32_t metadata;
        status =
            mc_world_collision_block_value_held(world, x, y, z, true, &metadata, &frame->metadata);
        if (status != MC_WORLD_STORAGE_OK) {
            frame->query = frame->metadata.nearby.query;
            frame->registry = frame->query.registry;
            frame->status = status;
            frame->operation = frame->query.operation;
            frame->cause = frame->query.cause;
        } else if (detector ? ((uint32_t)metadata & 8u) == 0u : metadata != 1) {
            frame->bytecode = detector ? 35u : 27u;
            if (detector) {
                status = fail(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                              "Detector contact positive helper f is not implemented");
            } else if (!mc_world_mutation_pressure_update(world, block, x, y, z,
                                                          &frame->mutation)) {
                frame->mutation.pressure_contact_bytecode = 27;
                frame->helper_bytecode = frame->mutation.pressure_bytecode;
                status = frame->status = frame->mutation.status;
                frame->operation = frame->mutation.operation;
                frame->cause = frame->mutation.cause;
                frame->query = frame->mutation.query;
                frame->registry = frame->mutation.registry;
            }
        }
    }
    mc_world_runtime_call_leave(world);
    return status;
}

mc_world_storage_status mc_world_runtime_entity_contact(mc_world_runtime *world,
                                                        const mc_block *block, int32_t x, int32_t y,
                                                        int32_t z, mc_entity *entity,
                                                        mc_world_entity_contact_error *error) {
    mc_world_entity_contact_error frame = mc_world_entity_contact_empty_error();
    frame.block = block;
    frame.x = x;
    frame.y = y;
    frame.z = z;
    mc_world_storage_status status;
    if (block == NULL || block->classes == NULL ||
        (unsigned int)block->kind >= (unsigned int)MC_BLOCK_KIND_COUNT) {
        status = fail(&frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Block contact receiver/kind");
    } else {
        switch (block->kind) {
            case MC_BLOCK_AK_Z:
                status = portal(block, entity, &frame);
                break;
            case MC_BLOCK_BV_BC:
                status = guarded_plate(world, block, x, y, z, false, &frame);
                break;
            case MC_BLOCK_OA_IU:
                status = soul_sand(block, entity, &frame);
                break;
            case MC_BLOCK_OR_JE: {
                stage(block, &frame, "or", "je", 4);
                bool ignored;
                status = effect_result(
                    &frame, mc_entity_effect_damage(entity, NULL, 1, &ignored, &frame.effect),
                    "Block cactus Entity damage");
                break;
            }
            case MC_BLOCK_PH_JN:
                status = guarded_plate(world, block, x, y, z, true, &frame);
                break;
            case MC_BLOCK_RN_KU: {
                stage(block, &frame, "rn", "ku", 3);
                mc_entity_base_fields *const base = fields(entity, &frame);
                if (base == NULL)
                    status = frame.status;
                else {
                    base->bc = true; /* sn.bc/lq.bf3, slowdown belongs to the next move. */
                    status = MC_WORLD_STORAGE_OK;
                }
                break;
            }
            default:
                stage(block, &frame, "uu", "na", 0);
                status = MC_WORLD_STORAGE_OK; /* Actual inherited base return0. */
                break;
        }
    }
    if (status == MC_WORLD_STORAGE_OK)
        frame = mc_world_entity_contact_empty_error();
    if (error != NULL)
        *error = frame;
    return status;
}
