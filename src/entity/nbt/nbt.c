#include "mc/entity/nbt/nbt.h"

#include "../entity_internal.h"
#include "mc/core/java/java.h"

#include <float.h>
#include <limits.h>
#include <math.h>
#include <string.h>

#if !defined(_M_X64) && !defined(_M_AMD64) && !defined(__x86_64__)
#error "Entity NBT arithmetic requires x86-64/SSE2"
#endif
#if !defined(_MSC_VER) && !defined(__SSE2__)
#error "Entity NBT arithmetic requires SSE2"
#endif
#if defined(__FAST_MATH__) || (defined(__FINITE_MATH_ONLY__) && __FINITE_MATH_ONLY__ > 0) ||       \
    (defined(_M_FP_FAST) && _M_FP_FAST)
#error "Entity NBT arithmetic requires strict floating transformations"
#endif
#include <emmintrin.h>

_Static_assert(CHAR_BIT == 8 && sizeof(float) == sizeof(uint32_t) &&
                   sizeof(double) == sizeof(uint64_t),
               "binary32/binary64 storage required");
_Static_assert(FLT_RADIX == 2 && FLT_MANT_DIG == 24 && FLT_MAX_EXP == 128 && DBL_MANT_DIG == 53 &&
                   DBL_MAX_EXP == 1024 && FLT_EVAL_METHOD == 0,
               "strict binary32/binary64 evaluation required");

static const uint16_t pos_name[] = {'P', 'o', 's'};
static const uint16_t motion_name[] = {'M', 'o', 't', 'i', 'o', 'n'};
static const uint16_t rotation_name[] = {'R', 'o', 't', 'a', 't', 'i', 'o', 'n'};
static const uint16_t fall_name[] = {'F', 'a', 'l', 'l', 'D', 'i', 's', 't', 'a', 'n', 'c', 'e'};
static const uint16_t fire_name[] = {'F', 'i', 'r', 'e'};
static const uint16_t air_name[] = {'A', 'i', 'r'};
static const uint16_t ground_name[] = {'O', 'n', 'G', 'r', 'o', 'u', 'n', 'd'};
static const uint16_t health_name[] = {'H', 'e', 'a', 'l', 't', 'h'};
static const uint16_t age_name[] = {'A', 'g', 'e'};
static const uint16_t item_name[] = {'I', 't', 'e', 'm'};
static const uint16_t id_name[] = {'i', 'd'};
static const uint16_t count_name[] = {'C', 'o', 'u', 'n', 't'};
static const uint16_t damage_name[] = {'D', 'a', 'm', 'a', 'g', 'e'};
static const uint16_t tile_name[] = {'T', 'i', 'l', 'e'};

static bool fail(mc_entity_nbt_error *frame, mc_entity_nbt_status status, const char *operation)
{
    frame->status = status;
    frame->operation = operation;
    return false;
}

static mc_entity_nbt_status publish(const mc_entity_nbt_error *frame, mc_entity_nbt_error *error)
{
    if (error != NULL)
        *error = *frame;
    return frame->status;
}

static bool enter(mc_entity *entity, mc_entity_nbt_error *frame)
{
    if (entity == NULL)
        return fail(frame, MC_ENTITY_NBT_INVALID_ARGUMENT, "Entity NBT native owner");
    const mc_entity_status status = mc_entity_retain(entity);
    if (status != MC_ENTITY_OK) {
        frame->construction.status = status;
        frame->construction.operation = "Entity NBT native ownership";
        return fail(frame,
                    status == MC_ENTITY_LIMIT ? MC_ENTITY_NBT_LIMIT : MC_ENTITY_NBT_ENTITY_FAILED,
                    "Entity NBT native ownership");
    }
    return true;
}

static bool fp_admit(mc_entity_nbt_error *frame, const char *operation)
{
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u)
        return fail(frame, MC_ENTITY_NBT_UNSUPPORTED_FP, operation);
    return true;
}

/* Called only at the reached original Compound getter. Input structural spans
 * are caller-owned/readable; no whole-tree validation moves later failures. */
static bool get(const mc_nbt_tag *compound, const uint16_t *name, size_t length, const char *field,
                mc_nbt_type type, const mc_nbt_tag **out, mc_entity_nbt_error *frame)
{
    frame->field = field;
    frame->index = SIZE_MAX;
    if (compound == NULL)
        return fail(frame, MC_ENTITY_NBT_NULL_REFERENCE, "Entity NBT Compound receiver");
    if (compound->type != MC_NBT_COMPOUND)
        return fail(frame, MC_ENTITY_NBT_WRONG_TAG, "Entity NBT Compound type");
    const mc_nbt_tag *tag = mc_nbt_compound_get(compound, name, length);
    if (tag != NULL && tag->type != type)
        return fail(frame, MC_ENTITY_NBT_WRONG_TAG, "Entity NBT named tag cast");
    *out = tag;
    return true;
}

static const mc_nbt_tag *element(const mc_nbt_tag *list, const char *field, size_t index,
                                 mc_nbt_type type, unsigned int get_pc, unsigned int cast_pc,
                                 unsigned int value_pc, mc_entity_nbt_error *frame)
{
    frame->field = field;
    frame->index = index;
    frame->base_bytecode = get_pc;
    /* NULL here represents the actual fresh empty List returned for an absent
     * named List. Explicit source NULL elements are checked after get/cast. */
    if (list == NULL || index >= list->value.list.length) {
        fail(frame, MC_ENTITY_NBT_INDEX_OUT_OF_BOUNDS, "Entity NBT List.get index");
        return NULL;
    }
    const mc_nbt_tag *tag = list->value.list.items[index];
    frame->base_bytecode = cast_pc;
    if (tag != NULL && tag->type != type) {
        fail(frame, MC_ENTITY_NBT_WRONG_TAG, "Entity NBT vector element cast");
        return NULL;
    }
    frame->base_bytecode = value_pc;
    if (tag == NULL) {
        fail(frame, MC_ENTITY_NBT_NULL_REFERENCE, "Entity NBT vector scalar receiver");
        return NULL;
    }
    return tag;
}

static void load_double(double *destination, const mc_nbt_tag *tag)
{
    memcpy(destination, &tag->value.double_bits, sizeof(*destination));
}

static void load_float(float *destination, const mc_nbt_tag *tag)
{
    const uint32_t bits = tag == NULL ? 0 : tag->value.float_bits;
    memcpy(destination, &bits, sizeof(*destination));
}

static void clamp_motion(double *value)
{
    uint64_t bits;
    memcpy(&bits, value, sizeof(bits));
    const uint64_t magnitude = bits & UINT64_C(0x7fffffffffffffff);
    /* abs(v)>10 with dcmpl/ifle: NaN skips, infinity clamps, -0 survives.
     * Integer comparison leaves the already stored NaN representation intact. */
    if (magnitude > UINT64_C(0x4024000000000000) && magnitude <= UINT64_C(0x7ff0000000000000))
        *value = 0.0;
}

static mc_entity_nbt_status stack_status(mc_item_stack_status status)
{
    if (status == MC_ITEM_STACK_LIMIT)
        return MC_ENTITY_NBT_LIMIT;
    if (status == MC_ITEM_STACK_ALLOCATION_FAILED)
        return MC_ENTITY_NBT_ALLOCATION_FAILED;
    return MC_ENTITY_NBT_STACK_FAILED;
}

static bool read_item(mc_entity *entity, const mc_nbt_tag *compound, mc_entity_nbt_error *frame)
{
    const mc_nbt_tag *tag;
    frame->item_bytecode = 4;
    if (!get(compound, health_name, 6, "Health", MC_NBT_SHORT, &tag, frame))
        return false;
    entity->item.f = (tag == NULL ? 0 : (int32_t)tag->value.short_value) & 255;
    frame->item_bytecode = 18;
    if (!get(compound, age_name, 3, "Age", MC_NBT_SHORT, &tag, frame))
        return false;
    entity->item.b = tag == NULL ? 0 : tag->value.short_value;
    frame->item_bytecode = 27;
    if (!get(compound, item_name, 4, "Item", MC_NBT_COMPOUND, &tag, frame))
        return false;
    const mc_nbt_tag empty = {.type = MC_NBT_COMPOUND};
    const mc_nbt_tag *input = tag == NULL ? &empty : tag;
    mc_item_stack *replacement = NULL;
    frame->item_bytecode = 32;
    mc_item_stack_status status = mc_item_stack_allocate(&replacement);
    if (status != MC_ITEM_STACK_OK) {
        frame->stack.status = status;
        return fail(frame, stack_status(status), "ItemEntity NBT new Stack");
    }
    frame->item_bytecode = 37;
    status = mc_item_stack_construct_nbt(replacement, input, &frame->stack);
    if (status != MC_ITEM_STACK_OK) {
        if (frame->stack.field != NULL) {
            frame->field = frame->stack.field;
            if (strcmp(frame->stack.field, "id") == 0)
                frame->stack_bytecode = 4;
            else if (strcmp(frame->stack.field, "Count") == 0)
                frame->stack_bytecode = 14;
            else if (strcmp(frame->stack.field, "Damage") == 0)
                frame->stack_bytecode = 24;
        }
        mc_item_stack_release(replacement);
        return fail(frame, stack_status(status), "ItemEntity NBT Stack constructor");
    }
    /* Transfer the new constructor's real hold at the original reference store.
     * Every input read has finished before the old Stack can be released. */
    mc_item_stack *old = entity->stack;
    entity->stack = replacement;
    mc_item_stack_release(old);
    frame->item_bytecode = 43;
    return true;
}

static bool read_subtype(mc_entity *entity, const mc_nbt_tag *compound, mc_entity_nbt_error *frame)
{
    if (mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_ITEM))
        return read_item(entity, compound, frame);
    if (mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_FALLING_SAND)) {
        const mc_nbt_tag *tag;
        frame->falling_bytecode = 4;
        if (!get(compound, tile_name, 4, "Tile", MC_NBT_BYTE, &tag, frame))
            return false;
        /* Actual Byte getter, unsigned mask10, then own field store11. Age is
         * not read here; the actual current b remains unchanged. */
        frame->falling_bytecode = 11;
        entity->falling.a = (tag == NULL ? 0 : (int32_t)tag->value.byte_value) & 255;
        frame->falling_bytecode = 14;
        return true;
    }
    return fail(frame, MC_ENTITY_NBT_UNSUPPORTED_TYPE, "Entity NBT concrete read hook");
}

static bool read_base(mc_entity *entity, const mc_nbt_tag *compound, mc_entity_nbt_error *frame)
{
    const mc_nbt_tag *pos, *motion, *rotation, *tag;
    frame->base_bytecode = 3;
    if (!get(compound, pos_name, 3, "Pos", MC_NBT_LIST, &pos, frame))
        return false;
    frame->base_bytecode = 10;
    if (!get(compound, motion_name, 6, "Motion", MC_NBT_LIST, &motion, frame))
        return false;
    frame->base_bytecode = 17;
    if (!get(compound, rotation_name, 8, "Rotation", MC_NBT_LIST, &rotation, frame))
        return false;

    tag = element(motion, "Motion", 0, MC_NBT_DOUBLE, 25, 28, 31, frame);
    if (tag == NULL)
        return false;
    load_double(&entity->base.velocity_x, tag);
    tag = element(motion, "Motion", 1, MC_NBT_DOUBLE, 40, 43, 46, frame);
    if (tag == NULL)
        return false;
    load_double(&entity->base.velocity_y, tag);
    tag = element(motion, "Motion", 2, MC_NBT_DOUBLE, 55, 58, 61, frame);
    if (tag == NULL)
        return false;
    load_double(&entity->base.velocity_z, tag);
    clamp_motion(&entity->base.velocity_x);
    clamp_motion(&entity->base.velocity_y);
    clamp_motion(&entity->base.velocity_z);

    tag = element(pos, "Pos", 0, MC_NBT_DOUBLE, 129, 132, 135, frame);
    if (tag == NULL)
        return false;
    load_double(&entity->base.x, tag);
    load_double(&entity->base.bl, tag);
    load_double(&entity->base.aJ, tag);
    tag = element(pos, "Pos", 1, MC_NBT_DOUBLE, 154, 157, 160, frame);
    if (tag == NULL)
        return false;
    load_double(&entity->base.y, tag);
    load_double(&entity->base.bm, tag);
    load_double(&entity->base.aK, tag);
    tag = element(pos, "Pos", 2, MC_NBT_DOUBLE, 179, 182, 185, frame);
    if (tag == NULL)
        return false;
    load_double(&entity->base.z, tag);
    load_double(&entity->base.bn, tag);
    load_double(&entity->base.aL, tag);

    tag = element(rotation, "Rotation", 0, MC_NBT_FLOAT, 204, 207, 210, frame);
    if (tag == NULL)
        return false;
    load_float(&entity->base.yaw, tag);
    load_float(&entity->base.aU, tag);
    tag = element(rotation, "Rotation", 1, MC_NBT_FLOAT, 225, 228, 231, frame);
    if (tag == NULL)
        return false;
    load_float(&entity->base.aT, tag);
    load_float(&entity->base.aV, tag);

    frame->base_bytecode = 245;
    if (!get(compound, fall_name, 12, "FallDistance", MC_NBT_FLOAT, &tag, frame))
        return false;
    load_float(&entity->base.bk, tag);
    frame->base_bytecode = 255;
    if (!get(compound, fire_name, 4, "Fire", MC_NBT_SHORT, &tag, frame))
        return false;
    entity->base.bv = tag == NULL ? 0 : tag->value.short_value;
    frame->base_bytecode = 265;
    if (!get(compound, air_name, 3, "Air", MC_NBT_SHORT, &tag, frame))
        return false;
    entity->base.bz = tag == NULL ? 0 : tag->value.short_value;
    frame->base_bytecode = 275;
    if (!get(compound, ground_name, 8, "OnGround", MC_NBT_BYTE, &tag, frame))
        return false;
    entity->base.aX = tag != NULL && tag->value.byte_value != 0;

    frame->base_bytecode = 294;
    frame->field = "Pos";
    if (!fp_admit(frame, "Entity NBT position floating environment"))
        return false;
    /* Explicit reached native bounds frontier; do not dereference a partial
     * constructor's NULL child, or invent an earlier READY/constructor gate. */
    if (entity->bounds == NULL)
        return fail(frame, MC_ENTITY_NBT_NULL_REFERENCE, "Entity NBT position bounds receiver");
    mc_entity_source_position(entity, entity->base.x, entity->base.y, entity->base.z);
    frame->base_bytecode = 306;
    frame->field = "Rotation";
    entity->base.yaw = fmodf(entity->base.yaw, 360.0f);
    entity->base.aT = fmodf(entity->base.aT, 360.0f);
    frame->base_bytecode = 311;
    return read_subtype(entity, compound, frame);
}

mc_entity_nbt_status mc_entity_read_nbt(mc_entity *entity, const mc_nbt_tag *compound,
                                        mc_entity_nbt_error *error)
{
    mc_entity_nbt_error frame = mc_entity_nbt_empty_error();
    if (!enter(entity, &frame))
        return publish(&frame, error);
    const bool completed = read_base(entity, compound, &frame);
    mc_entity_release(entity);
    if (completed)
        frame = mc_entity_nbt_empty_error();
    return publish(&frame, error);
}

static mc_nbt_string name(const uint16_t *units, size_t length)
{
    return (mc_nbt_string){units, length};
}

static void store_double(mc_nbt_tag *tag, const double *value)
{
    tag->type = MC_NBT_DOUBLE;
    memcpy(&tag->value.double_bits, value, sizeof(*value));
}

static void store_float(mc_nbt_tag *tag, const float *value)
{
    tag->type = MC_NBT_FLOAT;
    memcpy(&tag->value.float_bits, value, sizeof(*value));
}

static void list(mc_nbt_tag *tag, const uint16_t *units, size_t length, mc_nbt_type type,
                 const mc_nbt_tag *const *items, size_t count)
{
    tag->type = MC_NBT_LIST;
    tag->name = name(units, length);
    tag->value.list.element_type = (uint8_t)type;
    tag->value.list.items = items;
    tag->value.list.length = count;
}

static mc_entity_nbt_status nbt_status(mc_nbt_status status)
{
    if (status == MC_NBT_LIMIT)
        return MC_ENTITY_NBT_LIMIT;
    if (status == MC_NBT_ALLOCATION_FAILED)
        return MC_ENTITY_NBT_ALLOCATION_FAILED;
    return MC_ENTITY_NBT_NBT_FAILED;
}

mc_entity_nbt_status mc_entity_build_nbt(mc_entity *entity, const mc_nbt_limits *limits,
                                         mc_nbt_tree **out, mc_entity_nbt_error *error)
{
    mc_entity_nbt_error frame = mc_entity_nbt_empty_error();
    if (out == NULL) {
        fail(&frame, MC_ENTITY_NBT_INVALID_ARGUMENT, "Entity NBT tree output");
        return publish(&frame, error);
    }
    if (!enter(entity, &frame))
        return publish(&frame, error);
    mc_item_stack *held_stack = NULL;
    mc_nbt_tree *tree = NULL;
    mc_nbt_tag fields[10] = {0};
    mc_nbt_tag vector[8] = {0};
    mc_nbt_tag stack_fields[3] = {0};
    const mc_nbt_tag *pos_items[3] = {&vector[0], &vector[1], &vector[2]};
    const mc_nbt_tag *motion_items[3] = {&vector[3], &vector[4], &vector[5]};
    const mc_nbt_tag *rotation_items[2] = {&vector[6], &vector[7]};
    const mc_nbt_tag *item_items[3] = {&stack_fields[0], &stack_fields[1], &stack_fields[2]};
    const mc_nbt_tag *root_items[10];
    for (size_t index = 0; index < 10; ++index)
        root_items[index] = &fields[index];
    mc_nbt_tag root = {.type = MC_NBT_COMPOUND,
                       .value.compound = {.items = root_items, .length = 10}};

    store_double(&vector[0], &entity->base.x);
    frame.base_bytecode = 25;
    frame.field = "Pos";
    frame.index = 1;
    if (!fp_admit(&frame, "Entity NBT saved Y floating environment"))
        goto done;
    const double saved_y = entity->base.y + (double)entity->base.y_size;
    store_double(&vector[1], &saved_y);
    store_double(&vector[2], &entity->base.z);
    list(&fields[0], pos_name, 3, MC_NBT_DOUBLE, pos_items, 3);
    frame.base_bytecode = 71;
    store_double(&vector[3], &entity->base.velocity_x);
    store_double(&vector[4], &entity->base.velocity_y);
    store_double(&vector[5], &entity->base.velocity_z);
    list(&fields[1], motion_name, 6, MC_NBT_DOUBLE, motion_items, 3);
    frame.base_bytecode = 98;
    store_float(&vector[6], &entity->base.yaw);
    store_float(&vector[7], &entity->base.aT);
    list(&fields[2], rotation_name, 8, MC_NBT_FLOAT, rotation_items, 2);
    frame.index = SIZE_MAX;
    frame.base_bytecode = 108;
    store_float(&fields[3], &entity->base.bk);
    fields[3].name = name(fall_name, 12);
    frame.base_bytecode = 119;
    fields[4] = (mc_nbt_tag){.type = MC_NBT_SHORT,
                             .name = name(fire_name, 4),
                             .value.short_value = mc_java_i16_narrow(entity->base.bv)};
    frame.base_bytecode = 130;
    fields[5] = (mc_nbt_tag){.type = MC_NBT_SHORT,
                             .name = name(air_name, 3),
                             .value.short_value = mc_java_i16_narrow(entity->base.bz)};
    frame.base_bytecode = 140;
    fields[6] = (mc_nbt_tag){.type = MC_NBT_BYTE,
                             .name = name(ground_name, 8),
                             .value.byte_value = (int8_t)(entity->base.aX ? 1 : 0)};
    frame.base_bytecode = 145;
    if (mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_ITEM)) {
        frame.item_bytecode = 9;
        fields[7] = (mc_nbt_tag){.type = MC_NBT_SHORT,
                                 .name = name(health_name, 6),
                                 .value.short_value = mc_java_i8_narrow(entity->item.f)};
        frame.item_bytecode = 20;
        fields[8] = (mc_nbt_tag){.type = MC_NBT_SHORT,
                                 .name = name(age_name, 3),
                                 .value.short_value = mc_java_i16_narrow(entity->item.b)};
        mc_item_stack *stack = entity->stack; /* Actual captured receiver27. */
        fields[9] = (mc_nbt_tag){.type = MC_NBT_COMPOUND,
                                 .name = name(item_name, 4),
                                 .value.compound = {.items = item_items, .length = 3}};
        frame.item_bytecode = 37;
        frame.field = "Item";
        if (stack == NULL) {
            fail(&frame, MC_ENTITY_NBT_NULL_REFERENCE, "ItemEntity NBT Stack writer receiver");
            goto done;
        }
        mc_item_stack_status status = mc_item_stack_retain(stack);
        if (status != MC_ITEM_STACK_OK) {
            frame.stack.status = status;
            fail(&frame, stack_status(status), "ItemEntity NBT Stack writer ownership");
            goto done;
        }
        held_stack = stack;
        mc_item_stack_fields values;
        status = mc_item_stack_fields_get(stack, &values);
        if (status != MC_ITEM_STACK_OK) {
            frame.stack.status = status;
            fail(&frame, stack_status(status), "ItemEntity NBT Stack fields");
            goto done;
        }
        frame.stack_bytecode = 8;
        stack_fields[0] = (mc_nbt_tag){.type = MC_NBT_SHORT,
                                       .name = name(id_name, 2),
                                       .value.short_value = mc_java_i16_narrow(values.id)};
        frame.stack_bytecode = 19;
        stack_fields[1] = (mc_nbt_tag){.type = MC_NBT_BYTE,
                                       .name = name(count_name, 5),
                                       .value.byte_value = mc_java_i8_narrow(values.count)};
        frame.stack_bytecode = 30;
        stack_fields[2] = (mc_nbt_tag){.type = MC_NBT_SHORT,
                                       .name = name(damage_name, 6),
                                       .value.short_value = mc_java_i16_narrow(values.damage)};
        frame.stack_bytecode = 34;
        frame.item_bytecode = 43;
    } else if (mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_FALLING_SAND)) {
        frame.falling_bytecode = 8;
        frame.field = "Tile";
        fields[7] = (mc_nbt_tag){.type = MC_NBT_BYTE,
                                 .name = name(tile_name, 4),
                                 .value.byte_value = mc_java_i8_narrow(entity->falling.a)};
        frame.falling_bytecode = 11;
        root.value.compound.length = 8;
    } else {
        fail(&frame, MC_ENTITY_NBT_UNSUPPORTED_TYPE, "Entity NBT concrete write hook");
        goto done;
    }
    frame.base_bytecode = 148;
    frame.field = NULL;
    const mc_nbt_status copied = mc_nbt_tree_copy_compound(&root, limits, &tree, &frame.nbt);
    if (copied != MC_NBT_OK) {
        fail(&frame, nbt_status(copied), "Entity NBT independent tree build");
        goto done;
    }
    *out = tree;
    frame = mc_entity_nbt_empty_error();
done:
    mc_item_stack_release(held_stack);
    mc_entity_release(entity);
    return publish(&frame, error);
}
