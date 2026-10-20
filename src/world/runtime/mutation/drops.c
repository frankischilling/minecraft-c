/* Caller holds the actual World and live Registry/Block owners. Reached Entity
 * publication uses the actual World/Chunk owners; TileEntity lookup remains a
 * distinct unfinished descendant. */
#include "mutation_internal.h"

#include "../../../block/registry/registry_internal.h"
#include "../../../entity/entity_internal.h"
#include "mc/block/registry/runtime.h"
#include "mc/core/java/java.h"
#include "mc/item/registry/registry.h"

#if !defined(_M_X64) && !defined(_M_AMD64) && !defined(__x86_64__)
#error "Block drop floating operations require x86-64/SSE2"
#endif
#include <emmintrin.h>

typedef struct drop_frame {
    mc_world_runtime *world;
    const mc_block *block;
    mc_world_mutation_error *error;
    const char *class_name, *method;
} drop_frame;

static const char *drop_probability_method(const mc_block *block)
{
    return block->classes->options.side == MC_REGISTRY_CLIENT ? "a(Lfd;IIIIF)V" : "a(Ldj;IIIIF)V";
}

static const char *drop_spawn_method(const mc_block *block)
{
    return block->classes->options.side == MC_REGISTRY_CLIENT ? "a(Lfd;IIILiz;)V"
                                                              : "a(Ldj;IIILfy;)V";
}

static const char *drop_damage_method(const mc_block *block)
{
    return block->classes->options.side == MC_REGISTRY_CLIENT ? "b_(I)I" : "a_(I)I";
}

static drop_frame drop_frame_make(mc_world_runtime *world, const mc_block *block,
                                  const char *client, const char *server, const char *method,
                                  mc_world_mutation_error *error)
{
    return (drop_frame){world, block, error,
                        block->classes->options.side == MC_REGISTRY_CLIENT ? client : server,
                        method};
}

static void drop_at(const drop_frame *frame, unsigned int pc)
{
    frame->error->drop_class = frame->class_name;
    frame->error->drop_method = frame->method;
    frame->error->drop_bytecode = pc;
}

static bool drop_fail(const drop_frame *frame, unsigned int pc, const char *operation)
{
    drop_at(frame, pc);
    return mc_world_mutation_fail(frame->world, frame->error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                  operation);
}

static bool drop_registry_fail(const drop_frame *frame, unsigned int pc,
                               const mc_registry_error *cause)
{
    drop_at(frame, pc);
    return mc_world_mutation_registry_fail(frame->world, frame->error, cause, cause->message);
}

static bool drop_null(const drop_frame *frame, unsigned int pc, const char *operation)
{
    mc_registry_error cause = mc_world_mutation_empty_error().registry;
    cause.status = MC_REGISTRY_NULL_REFERENCE;
    cause.message = operation;
    return drop_registry_fail(frame, pc, &cause);
}

static bool drop_kind(const drop_frame *frame)
{
    if ((unsigned int)frame->block->kind < (unsigned int)MC_BLOCK_KIND_COUNT)
        return true;
    drop_at(frame, 0);
    return mc_world_mutation_fail(frame->world, frame->error, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                                  "Block drop native concrete kind");
}

static bool drop_random_int(const drop_frame *frame, mc_java_random *random, int32_t bound,
                            unsigned int pc, int32_t *out)
{
    drop_at(frame, pc);
    if (random == NULL)
        return drop_fail(frame, pc, "Block drop Random receiver is null");
    if (!mc_java_random_next_bounded(random, bound, out))
        return drop_fail(frame, pc, "Block drop bounded Random argument");
    return true;
}

static bool drop_random_float(const drop_frame *frame, mc_java_random *random, unsigned int pc,
                              float *out)
{
    drop_at(frame, pc);
    if (random == NULL)
        return drop_fail(frame, pc, "Block drop Random receiver is null");
    *out = mc_java_random_next_float(random);
    return true;
}

static bool drop_remote(const drop_frame *frame, unsigned int pc, bool *out)
{
    drop_at(frame, pc);
    if (frame->world == NULL)
        return drop_fail(frame, pc, "Block drop World receiver is null");
    *out = frame->world->fields.B;
    return true;
}

static bool drop_block_id(const drop_frame *frame, const char *name, unsigned int capture_pc,
                          unsigned int field_pc, int32_t *out)
{
    const mc_block *value;
    mc_registry_error cause = mc_world_mutation_empty_error().registry;
    drop_at(frame, capture_pc);
    if (mc_registry_block_static_get(frame->block->classes, name, &value, &cause) != MC_REGISTRY_OK)
        return drop_registry_fail(frame, capture_pc, &cause);
    if (value == NULL)
        return drop_null(frame, field_pc, "Block drop static Block receiver is null");
    *out = value->fields.bn;
    return true;
}

/* The two existing operations implement actual Item getstatic evaluation:
 * initialize first, then read the current named cell. Inspection alone would
 * skip the real active use. No final numeric Item table is used. */
static bool drop_item_static(const drop_frame *frame, const char *name, unsigned int pc,
                             const mc_item **out)
{
    mc_registry_error cause = mc_world_mutation_empty_error().registry;
    drop_at(frame, pc);
    if (mc_registry_classes_initialize_items(frame->block->classes, &cause) != MC_REGISTRY_OK)
        return drop_registry_fail(frame, pc, &cause);
    if (mc_registry_classes_inspect_item_static(frame->block->classes, name, out, &cause) !=
        MC_REGISTRY_OK)
        return drop_registry_fail(frame, pc, &cause);
    return true;
}

static bool drop_item_id(const drop_frame *frame, const char *name, unsigned int capture_pc,
                         unsigned int field_pc, int32_t *out)
{
    const mc_item *value;
    if (!drop_item_static(frame, name, capture_pc, &value))
        return false;
    if (value == NULL)
        return drop_null(frame, field_pc, "Block drop static Item receiver is null");
    *out = mc_item_fields_get(value)->bf;
    return true;
}

static bool drop_quantity(mc_world_runtime *world, const mc_block *block, mc_java_random *random,
                          int32_t *out, mc_world_mutation_error *error)
{
    drop_frame frame = drop_frame_make(world, block, "uu", "na", "a(Ljava/util/Random;)I", error);
    if (!drop_kind(&frame))
        return false;
    int32_t draw, value;
    switch (block->kind) {
        /* Actual constant bodies, not a zero-drop fallback. */
        case MC_BLOCK_AH_T:
        case MC_BLOCK_AK_Z:
        case MC_BLOCK_DD_BZ:
        case MC_BLOCK_FK_DO:
        case MC_BLOCK_H_G:
        case MC_BLOCK_HB_ER:
        case MC_BLOCK_JR_GJ:
        case MC_BLOCK_NK_IK:
        case MC_BLOCK_OM_JC:
        case MC_BLOCK_NX_IS:
        case MC_BLOCK_YQ_PT:
        case MC_BLOCK_UM_MW:
            *out = 0;
            return true;
        case MC_BLOCK_AC_P:
        case MC_BLOCK_RZ_LF:
            *out = 4;
            return true;
        case MC_BLOCK_BK_AR:
            frame = drop_frame_make(world, block, "bk", "ar", "a(Ljava/util/Random;)I", error);
            if (!drop_random_int(&frame, random, 20, 3, &draw))
                return false;
            *out = draw == 0 ? 1 : 0;
            return true;
        case MC_BLOCK_BS_AZ:
            frame = drop_frame_make(world, block, "bs", "az", "a(Ljava/util/Random;)I", error);
            if (!drop_random_int(&frame, random, 2, 3, &draw))
                return false;
            *out = mc_java_i32_add(4, draw);
            return true;
        case MC_BLOCK_LY_HR:
            frame = drop_frame_make(world, block, "ly", "hr", "a(Ljava/util/Random;)I", error);
            if (!drop_random_int(&frame, random, 3, 3, &draw))
                return false;
            *out = mc_java_i32_add(2, draw);
            return true;
        case MC_BLOCK_MT_IC:
            frame = drop_frame_make(world, block, "mt", "ic", "a(Ljava/util/Random;)I", error);
            value = block->fields.bn; /* own read1 precedes active static4 */
            if (!drop_block_id(&frame, "O", 4, 7, &draw))
                return false;
            if (value == draw) {
                if (!drop_random_int(&frame, random, 5, 16, &draw))
                    return false;
                *out = mc_java_i32_add(4, draw);
            } else {
                *out = 1;
            }
            return true;
        case MC_BLOCK_YS_PW:
            *out = block->subtype.ys.b ? 2 : 1;
            return true;
        case MC_BLOCK_SS_LU:
            frame = drop_frame_make(world, block, "ss", "lu", "a(Ljava/util/Random;)I", error);
            drop_at(&frame, 5);
            if (block->subtype.ss.a == NULL)
                return drop_fail(&frame, 5, "Block delegated quantity receiver is null");
            error->drop_call_bytecode = 5;
            return drop_quantity(world, block->subtype.ss.a, random, out, error);
        /* dp/cg, fb/dh, ni/ij, pc/jj (also ph/jn), vg/nk and the
         * inherited uu/na bodies each actually return the constant1. */
        default:
            *out = 1;
            return true;
    }
}

static bool drop_item(mc_world_runtime *world, const mc_block *block, int32_t metadata,
                      mc_java_random *random, int32_t *out, mc_world_mutation_error *error)
{
    drop_frame frame = drop_frame_make(world, block, "uu", "na", "a(ILjava/util/Random;)I", error);
    if (!drop_kind(&frame))
        return false;
    int32_t own, value, draw;
    const mc_block *delegate;
    mc_registry_error cause;
    switch (block->kind) {
        case MC_BLOCK_AC_P:
            frame = drop_frame_make(world, block, "ac", "p", "a(ILjava/util/Random;)I", error);
            return drop_item_id(&frame, "aB", 0, 3, out);
        case MC_BLOCK_BK_AR:
            frame = drop_frame_make(world, block, "bk", "ar", "a(ILjava/util/Random;)I", error);
            return drop_block_id(&frame, "z", 0, 3, out);
        case MC_BLOCK_BS_AZ:
            frame = drop_frame_make(world, block, "bs", "az", "a(ILjava/util/Random;)I", error);
            return drop_item_id(&frame, "aA", 0, 3, out);
        case MC_BLOCK_DB_BY:
            frame = drop_frame_make(world, block, "db", "by", "a(ILjava/util/Random;)I", error);
            return drop_block_id(&frame, "aR", 0, 3, out);
        case MC_BLOCK_DD_BZ:
        case MC_BLOCK_OM_JC:
        case MC_BLOCK_NX_IS:
        case MC_BLOCK_UM_MW:
        case MC_BLOCK_UT_MZ:
            *out = 0;
            return true;
        case MC_BLOCK_EO_CX:
            frame = drop_frame_make(world, block, "eo", "cx", "a(ILjava/util/Random;)I", error);
            return drop_block_id(&frame, "x", 0, 3, out);
        case MC_BLOCK_FB_DH:
            frame = drop_frame_make(world, block, "fb", "dh", "a(ILjava/util/Random;)I", error);
            return drop_block_id(&frame, "aq", 0, 3, out);
        case MC_BLOCK_JB_FZ:
            *out = -1;
            return true;
        case MC_BLOCK_JR_GJ:
            frame = drop_frame_make(world, block, "jr", "gj", "a(ILjava/util/Random;)I", error);
            return drop_item_id(&frame, "aB", 0, 3, out);
        case MC_BLOCK_LE_HC: {
            frame = drop_frame_make(world, block, "le", "hc", "a(ILjava/util/Random;)I", error);
            if (((uint32_t)metadata & 8u) != 0) {
                *out = 0;
                return true;
            }
            mc_material *captured = block->fields.bA;
            mc_material *material;
            cause = mc_world_mutation_empty_error().registry;
            drop_at(&frame, 13);
            if (mc_registry_material_get(block->classes, MC_MATERIAL_f, &material, &cause) !=
                MC_REGISTRY_OK)
                return drop_registry_fail(&frame, 13, &cause);
            return captured == material ? drop_item_id(&frame, "az", 19, 22, out)
                                        : drop_item_id(&frame, "at", 26, 29, out);
        }
        case MC_BLOCK_LY_HR:
            frame = drop_frame_make(world, block, "ly", "hr", "a(ILjava/util/Random;)I", error);
            return drop_item_id(&frame, "aR", 0, 3, out);
        case MC_BLOCK_MT_IC:
            frame = drop_frame_make(world, block, "mt", "ic", "a(ILjava/util/Random;)I", error);
            own = block->fields.bn;
            if (!drop_block_id(&frame, "J", 4, 7, &value))
                return false;
            if (own == value)
                return drop_item_id(&frame, "k", 13, 16, out);
            own = block->fields.bn;
            if (!drop_block_id(&frame, "ax", 24, 27, &value))
                return false;
            if (own == value)
                return drop_item_id(&frame, "l", 33, 36, out);
            own = block->fields.bn;
            if (!drop_block_id(&frame, "O", 44, 47, &value))
                return false;
            if (own == value)
                return drop_item_id(&frame, "aU", 53, 56, out);
            *out = block->fields.bn;
            return true;
        case MC_BLOCK_NE_IG:
            frame = drop_frame_make(world, block, "ne", "ig", "a(ILjava/util/Random;)I", error);
            if (!drop_random_int(&frame, random, 10, 3, &draw))
                return false;
            if (draw == 0)
                return drop_item_id(&frame, "an", 9, 12, out);
            *out = block->fields.bn;
            return true;
        case MC_BLOCK_NI_IJ:
            frame = drop_frame_make(world, block, "ni", "ij", "a(ILjava/util/Random;)I", error);
            if (metadata == 7)
                return drop_item_id(&frame, "R", 6, 9, out);
            *out = -1;
            return true;
        case MC_BLOCK_RI_KO:
            frame = drop_frame_make(world, block, "ri", "ko", "a(ILjava/util/Random;)I", error);
            return drop_item_id(&frame, "aH", 0, 3, out);
        case MC_BLOCK_RN_KU:
            frame = drop_frame_make(world, block, "rn", "ku", "a(ILjava/util/Random;)I", error);
            return drop_item_id(&frame, "I", 0, 3, out);
        case MC_BLOCK_RU_KZ:
            frame = drop_frame_make(world, block, "ru", "kz", "a(ILjava/util/Random;)I", error);
            if (!drop_random_int(&frame, random, 8, 3, &draw))
                return false;
            if (draw == 0)
                return drop_item_id(&frame, "Q", 9, 12, out);
            *out = -1;
            return true;
        case MC_BLOCK_RZ_LF:
            frame = drop_frame_make(world, block, "rz", "lf", "a(ILjava/util/Random;)I", error);
            return drop_item_id(&frame, "aG", 0, 3, out);
        case MC_BLOCK_SM_LO:
            frame = drop_frame_make(world, block, "sm", "lo", "a(ILjava/util/Random;)I", error);
            return drop_item_id(&frame, "aA", 0, 3, out);
        case MC_BLOCK_SS_LU:
            frame = drop_frame_make(world, block, "ss", "lu", "a(ILjava/util/Random;)I", error);
            drop_at(&frame, 6);
            if (block->subtype.ss.a == NULL)
                return drop_fail(&frame, 6, "Block delegated item receiver is null");
            error->drop_call_bytecode = 6;
            return drop_item(world, block->subtype.ss.a, metadata, random, out, error);
        case MC_BLOCK_TC_LZ:
            frame = drop_frame_make(world, block, "tc", "lz", "a(ILjava/util/Random;)I", error);
            return drop_block_id(&frame, "aC", 0, 3, out);
        case MC_BLOCK_UJ_MT:
            frame = drop_frame_make(world, block, "uj", "mt", "a(ILjava/util/Random;)I", error);
            return drop_item_id(&frame, "as", 0, 3, out);
        case MC_BLOCK_VE_NJ:
            frame = drop_frame_make(world, block, "ve", "nj", "a(ILjava/util/Random;)I", error);
            if (((uint32_t)metadata & 8u) != 0) {
                *out = 0;
                return true;
            }
            return drop_item_id(&frame, "aY", 9, 12, out);
        case MC_BLOCK_VG_NK:
            frame = drop_frame_make(world, block, "vg", "nk", "a(ILjava/util/Random;)I", error);
            return drop_block_id(&frame, "K", 0, 3, out);
        case MC_BLOCK_VL_NO:
        case MC_BLOCK_WP_OJ:
            frame = drop_frame_make(world, block, block->kind == MC_BLOCK_VL_NO ? "vl" : "wp",
                                    block->kind == MC_BLOCK_VL_NO ? "no" : "oj",
                                    "a(ILjava/util/Random;)I", error);
            cause = mc_world_mutation_empty_error().registry;
            drop_at(&frame, 0);
            if (mc_registry_block_static_get(block->classes, "w", &delegate, &cause) !=
                MC_REGISTRY_OK)
                return drop_registry_fail(&frame, 0, &cause);
            if (delegate == NULL)
                return drop_fail(&frame, 5, "Block static delegated item receiver is null");
            error->drop_call_bytecode = 5;
            return drop_item(world, delegate, 0, random, out, error);
        case MC_BLOCK_WO_OI:
            frame = drop_frame_make(world, block, "wo", "oi", "a(ILjava/util/Random;)I", error);
            return drop_item_id(&frame, "aZ", 0, 3, out);
        case MC_BLOCK_XQ_PA:
            frame = drop_frame_make(world, block, "xq", "pa", "a(ILjava/util/Random;)I", error);
            return drop_block_id(&frame, "Q", 0, 3, out);
        case MC_BLOCK_YS_PW:
            frame = drop_frame_make(world, block, "ys", "pw", "a(ILjava/util/Random;)I", error);
            return drop_block_id(&frame, "al", 0, 3, out);
        default:
            *out = block->fields.bn; /* selected inherited uu/na body */
            return true;
    }
}

static bool drop_damage(mc_world_runtime *world, const mc_block *block, int32_t metadata,
                        int32_t *out, mc_world_mutation_error *error)
{
    drop_frame frame = drop_frame_make(world, block, "uu", "na", drop_damage_method(block), error);
    if (!drop_kind(&frame))
        return false;
    switch (block->kind) {
        case MC_BLOCK_BK_AR:
        case MC_BLOCK_HE_EU:
            *out = (int32_t)((uint32_t)metadata & 3u);
            return true;
        case MC_BLOCK_EE_CQ:
        case MC_BLOCK_VG_NK:
        case MC_BLOCK_YS_PW:
            *out = metadata;
            return true;
        case MC_BLOCK_MT_IC: {
            frame = drop_frame_make(world, block, "mt", "ic", drop_damage_method(block), error);
            const int32_t own = block->fields.bn;
            int32_t value;
            if (!drop_block_id(&frame, "O", 4, 7, &value))
                return false;
            *out = own == value ? 4 : 0;
            return true;
        }
        default:
            *out = 0;
            return true;
    }
}

static mc_world_storage_status drop_item_status(mc_item_stack_status status)
{
    switch (status) {
        case MC_ITEM_STACK_LIMIT:
            return MC_WORLD_STORAGE_LIMIT;
        case MC_ITEM_STACK_ALLOCATION_FAILED:
            return MC_WORLD_STORAGE_ALLOCATION_FAILED;
        case MC_ITEM_STACK_INVALID_ARGUMENT:
            return MC_WORLD_STORAGE_INVALID_ARGUMENT;
        default:
            return MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    }
}

static bool drop_stack_failed(const drop_frame *frame, unsigned int pc, mc_item_stack_status status)
{
    drop_at(frame, pc);
    frame->error->item = status;
    return mc_world_mutation_fail(frame->world, frame->error, drop_item_status(status),
                                  "Block drop ItemStack operation");
}

static bool drop_entity_failed(const drop_frame *frame, unsigned int pc, mc_entity_status status)
{
    mc_world_storage_status mapped;
    switch (status) {
        case MC_ENTITY_LIMIT:
            mapped = MC_WORLD_STORAGE_LIMIT;
            break;
        case MC_ENTITY_ALLOCATION_FAILED:
            mapped = MC_WORLD_STORAGE_ALLOCATION_FAILED;
            break;
        case MC_ENTITY_BUSY:
            mapped = MC_WORLD_STORAGE_BUSY;
            break;
        case MC_ENTITY_INVALID_ARGUMENT:
            mapped = MC_WORLD_STORAGE_INVALID_ARGUMENT;
            break;
        case MC_ENTITY_WORLD_FAILED:
            mapped = frame->error->entity.world == MC_WORLD_STORAGE_OK
                         ? MC_WORLD_STORAGE_UNSUPPORTED_STATE
                         : frame->error->entity.world;
            break;
        case MC_ENTITY_ITEM_FAILED:
            mapped = drop_item_status(frame->error->entity.item);
            break;
        default:
            mapped = MC_WORLD_STORAGE_UNSUPPORTED_STATE;
            break;
    }
    drop_at(frame, pc);
    return mc_world_mutation_fail(frame->world, frame->error, mapped,
                                  frame->error->entity.operation == NULL
                                      ? "Block drop Entity operation"
                                      : frame->error->entity.operation);
}

static bool drop_entity_allocate(const drop_frame *frame, unsigned int pc, mc_entity **out)
{
    drop_at(frame, pc);
    mc_entity_classes *classes = mc_world_runtime_entities(frame->world);
    if (classes == NULL)
        return drop_fail(frame, pc, "Block drop EntityClasses binding is unavailable");
    const mc_entity_status status = mc_item_entity_allocate(classes, out, &frame->error->entity);
    return status == MC_ENTITY_OK || drop_entity_failed(frame, pc, status);
}

static bool drop_pickup_delay(const drop_frame *frame, unsigned int pc, mc_entity *entity)
{
    drop_at(frame, pc);
    const mc_entity_status status = mc_entity_item_pickup_delay_set(entity, 10);
    if (status == MC_ENTITY_OK)
        return true;
    frame->error->entity =
        (mc_entity_error){.status = status, .operation = "Block ItemEntity pickup delay field"};
    return drop_entity_failed(frame, pc, status);
}

static bool drop_fp(const drop_frame *frame, unsigned int pc)
{
    if ((_mm_getcsr() & 0xffc0u) == 0x1f80u)
        return true;
    return drop_fail(frame, pc, "Block drop floating-point admission");
}

static bool drop_done(mc_world_mutation_error *error)
{
    error->drop_class = NULL;
    error->drop_method = NULL;
    error->drop_bytecode = 0;
    error->drop_call_bytecode = 0;
    error->drop_spawn_bytecode = 0;
    error->publication = (mc_world_entity_error){0};
    return true;
}

static bool drop_publish(const drop_frame *frame, unsigned int pc, mc_entity *entity)
{
    drop_at(frame, pc);
    bool inserted;
    mc_world_mutation_error *error = frame->error;
    const mc_world_storage_status status =
        mc_world_runtime_insert_entity(frame->world, entity, &inserted, &error->publication);
    if (status == MC_WORLD_STORAGE_OK)
        return true; /* Original pop discards ordinary false as well as true. */
    error->status = status;
    error->operation = error->publication.operation;
    error->cause = error->publication.cause;
    frame->world->last_error = error->cause;
    return false;
}

static void drop_spawn_at(const drop_frame *frame, unsigned int pc)
{
    frame->error->drop_spawn_bytecode = pc;
    drop_at(frame, pc);
}

/* All 69 current concrete kinds select this actual uu/na protected spawn
 * body. new Entity precedes evaluation of its constructor coordinates; the
 * three offset calculations precede new Entity, as in the source. */
static bool drop_spawn(mc_world_runtime *world, const mc_block *block, int32_t x, int32_t y,
                       int32_t z, mc_item_stack *stack, mc_world_mutation_error *error)
{
    const drop_frame frame =
        drop_frame_make(world, block, "uu", "na", drop_spawn_method(block), error);
    bool remote;
    drop_spawn_at(&frame, 1);
    if (!drop_remote(&frame, 1, &remote))
        return false;
    if (remote)
        return true;
    float draw;
    double offset_x, offset_y, offset_z;
    drop_spawn_at(&frame, 16);
    if (!drop_random_float(&frame, world->fields.r, 16, &draw))
        return false;
    drop_spawn_at(&frame, 21);
    if (!drop_fp(&frame, 21))
        return false;
    const float spread = 0.7f;
    const float scaled_x = draw * spread;
    const float edge_x = 1.0f - spread;
    const double margin_x = (double)edge_x * 0.5;
    offset_x = (double)scaled_x + margin_x;
    drop_spawn_at(&frame, 39);
    if (!drop_random_float(&frame, world->fields.r, 39, &draw))
        return false;
    const float scaled_y = draw * spread;
    const float edge_y = 1.0f - spread;
    const double margin_y = (double)edge_y * 0.5;
    offset_y = (double)scaled_y + margin_y;
    drop_spawn_at(&frame, 62);
    if (!drop_random_float(&frame, world->fields.r, 62, &draw))
        return false;
    const float scaled_z = draw * spread;
    const float edge_z = 1.0f - spread;
    const double margin_z = (double)edge_z * 0.5;
    offset_z = (double)scaled_z + margin_z;
    mc_entity *entity = NULL;
    drop_spawn_at(&frame, 81);
    if (!drop_entity_allocate(&frame, 81, &entity))
        return false;
    const double position_x = (double)x + offset_x;
    const double position_y = (double)y + offset_y;
    const double position_z = (double)z + offset_z;
    drop_spawn_at(&frame, 104);
    mc_entity_status status = mc_item_entity_construct(entity, world, position_x, position_y,
                                                       position_z, stack, &error->entity);
    if (status != MC_ENTITY_OK) {
        (void)drop_entity_failed(&frame, 104, status);
        mc_entity_release(entity);
        return false;
    }
    drop_spawn_at(&frame, 113);
    if (!drop_pickup_delay(&frame, 113, entity)) {
        mc_entity_release(entity);
        return false;
    }
    drop_spawn_at(&frame, 119);
    const bool completed = drop_publish(&frame, 119, entity);
    mc_entity_release(entity);
    return completed;
}

static bool drop_probability(mc_world_runtime *world, const mc_block *block, int32_t x, int32_t y,
                             int32_t z, int32_t metadata, float chance,
                             mc_world_mutation_error *error);

/* invokespecial paths below enter this base body directly while its quantity,
 * item, damage and spawn calls remain virtual on the original actual Block. */
static bool drop_base_probability(mc_world_runtime *world, const mc_block *block, int32_t x,
                                  int32_t y, int32_t z, int32_t metadata, float chance,
                                  mc_world_mutation_error *error)
{
    const drop_frame frame =
        drop_frame_make(world, block, "uu", "na", drop_probability_method(block), error);
    bool remote;
    if (!drop_remote(&frame, 1, &remote))
        return false;
    if (remote)
        return true;
    int32_t quantity;
    drop_at(&frame, 13);
    error->drop_call_bytecode = 13;
    if (!drop_quantity(world, block, world->fields.r, &quantity, error))
        return false;
    for (int32_t index = 0; index < quantity; index = mc_java_i32_add(index, 1)) {
        float draw;
        if (!drop_random_float(&frame, world->fields.r, 32, &draw))
            return false;
        /* Source fcmpl37/ifle38 accepts unless draw is greater. This also
         * preserves the unordered case if a future actual caller supplies NaN. */
        if (draw > chance)
            continue;
        int32_t item;
        drop_at(&frame, 51);
        error->drop_call_bytecode = 51;
        if (!drop_item(world, block, metadata, world->fields.r, &item, error))
            return false;
        if (item <= 0)
            continue;
        mc_item_stack *stack = NULL;
        drop_at(&frame, 70);
        mc_item_stack_status status = mc_item_stack_allocate(&stack);
        if (status != MC_ITEM_STACK_OK)
            return drop_stack_failed(&frame, 70, status);
        int32_t damage;
        drop_at(&frame, 80);
        error->drop_call_bytecode = 80;
        if (!drop_damage(world, block, metadata, &damage, error)) {
            mc_item_stack_release(stack);
            return false;
        }
        drop_at(&frame, 83);
        status = mc_item_stack_construct(stack, item, 1, damage);
        if (status != MC_ITEM_STACK_OK) {
            (void)drop_stack_failed(&frame, 83, status);
            mc_item_stack_release(stack);
            return false;
        }
        drop_at(&frame, 86);
        error->drop_call_bytecode = 86;
        const bool completed = drop_spawn(world, block, x, y, z, stack, error);
        mc_item_stack_release(stack);
        if (!completed)
            return false;
    }
    return true;
}

/* ni/ij has a second, different spawn sequence. Its positions are computed
 * entirely in binary32 (including int->float) before widening, and new Entity
 * 111 precedes new Stack135 and the actual Item.Q active use139. */
static bool drop_crop_probability(mc_world_runtime *world, const mc_block *block, int32_t x,
                                  int32_t y, int32_t z, int32_t metadata, float chance,
                                  mc_world_mutation_error *error)
{
    const drop_frame frame =
        drop_frame_make(world, block, "ni", "ij", drop_probability_method(block), error);
    drop_at(&frame, 10);
    error->drop_call_bytecode = 10;
    if (!drop_base_probability(world, block, x, y, z, metadata, chance, error))
        return false;
    bool remote;
    if (!drop_remote(&frame, 14, &remote))
        return false;
    if (remote)
        return true;
    for (int32_t index = 0; index < 3; index = mc_java_i32_add(index, 1)) {
        int32_t draw_int;
        if (!drop_random_int(&frame, world->fields.r, 15, 36, &draw_int))
            return false;
        if (draw_int > metadata)
            continue;
        float draw;
        if (!drop_random_float(&frame, world->fields.r, 55, &draw))
            return false;
        if (!drop_fp(&frame, 60))
            return false;
        const float spread = 0.7f;
        const float scaled_x = draw * spread;
        const float edge_x = 1.0f - spread;
        const float margin_x = edge_x * 0.5f;
        const float offset_x = scaled_x + margin_x;
        if (!drop_random_float(&frame, world->fields.r, 75, &draw))
            return false;
        const float scaled_y = draw * spread;
        const float edge_y = 1.0f - spread;
        const float margin_y = edge_y * 0.5f;
        const float offset_y = scaled_y + margin_y;
        if (!drop_random_float(&frame, world->fields.r, 95, &draw))
            return false;
        const float scaled_z = draw * spread;
        const float edge_z = 1.0f - spread;
        const float margin_z = edge_z * 0.5f;
        const float offset_z = scaled_z + margin_z;
        mc_entity *entity = NULL;
        if (!drop_entity_allocate(&frame, 111, &entity))
            return false;
        const float float_x = (float)x + offset_x;
        const float float_y = (float)y + offset_y;
        const float float_z = (float)z + offset_z;
        mc_item_stack *stack = NULL;
        drop_at(&frame, 135);
        mc_item_stack_status item_status = mc_item_stack_allocate(&stack);
        if (item_status != MC_ITEM_STACK_OK) {
            (void)drop_stack_failed(&frame, 135, item_status);
            mc_entity_release(entity);
            return false;
        }
        const mc_item *item_value;
        if (!drop_item_static(&frame, "Q", 139, &item_value)) {
            mc_item_stack_release(stack);
            mc_entity_release(entity);
            return false;
        }
        drop_at(&frame, 142);
        if (item_value == NULL) {
            /* Actual Stack(Item) constructor dereferences Item.bf at2,
             * before invoking its numeric constructor7. The enclosing crop
             * frame stays at its real invokespecial142, not crop PC2. */
            mc_registry_error cause = mc_world_mutation_empty_error().registry;
            cause.status = MC_REGISTRY_NULL_REFERENCE;
            cause.message = "Block crop Stack Item receiver is null";
            cause.constructor_class =
                block->classes->options.side == MC_REGISTRY_CLIENT ? "iz" : "fy";
            cause.constructor_pc = 2;
            error->drop_call_bytecode = 142;
            (void)drop_registry_fail(&frame, 142, &cause);
            mc_item_stack_release(stack);
            mc_entity_release(entity);
            return false;
        }
        const int32_t item = mc_item_fields_get(item_value)->bf;
        item_status = mc_item_stack_construct(stack, item, 1, 0);
        if (item_status != MC_ITEM_STACK_OK) {
            (void)drop_stack_failed(&frame, 142, item_status);
            mc_item_stack_release(stack);
            mc_entity_release(entity);
            return false;
        }
        drop_at(&frame, 145);
        const mc_entity_status entity_status =
            mc_item_entity_construct(entity, world, (double)float_x, (double)float_y,
                                     (double)float_z, stack, &error->entity);
        mc_item_stack_release(stack);
        if (entity_status != MC_ENTITY_OK) {
            (void)drop_entity_failed(&frame, 145, entity_status);
            mc_entity_release(entity);
            return false;
        }
        if (!drop_pickup_delay(&frame, 154, entity)) {
            mc_entity_release(entity);
            return false;
        }
        const bool completed = drop_publish(&frame, 160, entity);
        mc_entity_release(entity);
        if (!completed)
            return false;
    }
    return true;
}

static bool drop_probability(mc_world_runtime *world, const mc_block *block, int32_t x, int32_t y,
                             int32_t z, int32_t metadata, float chance,
                             mc_world_mutation_error *error)
{
    drop_frame frame =
        drop_frame_make(world, block, "uu", "na", drop_probability_method(block), error);
    if (!drop_kind(&frame))
        return false;
    bool remote;
    switch (block->kind) {
        case MC_BLOCK_FO_DT:
            frame =
                drop_frame_make(world, block, "fo", "dt", drop_probability_method(block), error);
            if (!drop_remote(&frame, 1, &remote))
                return false;
            if (remote)
                return true;
            drop_at(&frame, 18);
            error->drop_call_bytecode = 18;
            return drop_base_probability(world, block, x, y, z, metadata, chance, error);
        case MC_BLOCK_NI_IJ:
            return drop_crop_probability(world, block, x, y, z, metadata, chance, error);
        case MC_BLOCK_SS_LU:
            frame =
                drop_frame_make(world, block, "ss", "lu", drop_probability_method(block), error);
            drop_at(&frame, 13);
            if (block->subtype.ss.a == NULL)
                return drop_fail(&frame, 13, "Block delegated probability receiver is null");
            error->drop_call_bytecode = 13;
            return drop_probability(world, block->subtype.ss.a, x, y, z, metadata, chance, error);
        case MC_BLOCK_UT_MZ:
            frame =
                drop_frame_make(world, block, "ut", "mz", drop_probability_method(block), error);
            if (!drop_remote(&frame, 1, &remote))
                return false;
            if (remote)
                return true;
            /* Actual TileEntity lookup c14 is a missing concrete body. It
             * cannot be replaced by NULL (which would fabricate return24). */
            return drop_fail(&frame, 14, "Moving-piston drop TileEntity lookup is not implemented");
        case MC_BLOCK_VE_NJ:
            if (((uint32_t)metadata & 8u) != 0)
                return true;
            frame =
                drop_frame_make(world, block, "ve", "nj", drop_probability_method(block), error);
            drop_at(&frame, 18);
            error->drop_call_bytecode = 18;
            return drop_base_probability(world, block, x, y, z, metadata, chance, error);
        default:
            return drop_base_probability(world, block, x, y, z, metadata, chance, error);
    }
}

bool mc_world_mutation_drop(mc_world_runtime *world, const mc_block *block, int32_t x, int32_t y,
                            int32_t z, int32_t metadata, mc_world_mutation_error *error)
{
    error->publication = (mc_world_entity_error){0};
    error->drop_class = NULL;
    error->drop_method = NULL;
    error->drop_bytecode = 0;
    error->drop_spawn_bytecode = 0;
    error->drop_call_bytecode = 0;
    error->item = MC_ITEM_STACK_OK;
    error->entity = (mc_entity_error){0};
    if (block == NULL)
        return mc_world_mutation_fail(world, error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                      "Block final drop receiver is null");
    error->drop_call_bytecode = 9; /* actual final g invokes virtual probability */
    if (!drop_probability(world, block, x, y, z, metadata, 1.0f, error))
        return false;
    return drop_done(error);
}
