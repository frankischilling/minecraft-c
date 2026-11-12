#include "internal.h"
#include "mc/core/java/java.h"
#include "nbt_internal.h"

typedef struct arrow_key {
    const uint16_t *units;
    size_t length;
    const char *field;
} arrow_key;

#define KEY(name, text, ...)                                                                       \
    static const uint16_t name##_units[] = {__VA_ARGS__};                                          \
    static const arrow_key name = {name##_units, sizeof(name##_units) / sizeof(*name##_units), text}
KEY(x_tile, "xTile", 'x', 'T', 'i', 'l', 'e');
KEY(y_tile, "yTile", 'y', 'T', 'i', 'l', 'e');
KEY(z_tile, "zTile", 'z', 'T', 'i', 'l', 'e');
KEY(in_tile, "inTile", 'i', 'n', 'T', 'i', 'l', 'e');
KEY(in_data, "inData", 'i', 'n', 'D', 'a', 't', 'a');
KEY(shake, "shake", 's', 'h', 'a', 'k', 'e');
KEY(in_ground, "inGround", 'i', 'n', 'G', 'r', 'o', 'u', 'n', 'd');
KEY(player, "player", 'p', 'l', 'a', 'y', 'e', 'r');
#undef KEY

static bool fail(mc_entity_nbt_error *e, mc_entity_nbt_status status, const char *operation) {
    e->status = status;
    e->operation = operation;
    return false;
}

static void at(mc_entity_nbt_error *e, bool write, int32_t pc, arrow_key key) {
    e->arrow.site = (mc_arrow_site){"sl", write ? "b" : "a", "(Lnu;)V", pc};
    e->arrow.child = (mc_arrow_site){.pc = -1};
    e->field = key.field;
    e->index = SIZE_MAX;
}

static bool get(const mc_nbt_tag *compound, arrow_key key, mc_nbt_type type, const mc_nbt_tag **out,
                mc_entity_nbt_error *e) {
    if (compound == NULL)
        return fail(e, MC_ENTITY_NBT_NULL_REFERENCE, "Arrow NBT Compound receiver");
    if (compound->type != MC_NBT_COMPOUND)
        return fail(e, MC_ENTITY_NBT_WRONG_TAG, "Arrow NBT Compound view type");
    const mc_nbt_tag *value = mc_nbt_compound_get(compound, key.units, key.length);
    if (value != NULL && value->type != type) {
        /* NU.m delegates to the actual Byte getter. Its failed cast is still
         * c25, independently of the SL95 Boolean invoke. */
        e->arrow.child = (mc_arrow_site){
            "nu", type == MC_NBT_SHORT ? "d" : "c",
            type == MC_NBT_SHORT ? "(Ljava/lang/String;)S" : "(Ljava/lang/String;)B", 25};
        return fail(e, MC_ENTITY_NBT_WRONG_TAG, "Arrow NBT named tag cast");
    }
    *out = value;
    return true;
}

bool mc_arrow_nbt_read_held(mc_entity *entity, const mc_nbt_tag *compound, mc_entity_nbt_error *e) {
    mc_entity_nbt_error scratch;
    if (e == NULL) {
        mc_entity_nbt_error_init(&scratch);
        e = &scratch;
    }
    if (!mc_entity_has_arrow(entity))
        return fail(e, MC_ENTITY_NBT_INVALID_ARGUMENT, "Arrow NBT actual owner");
    mc_entity_nbt_select_arrow_detail(e);
    e->arrow.returned = false;
    mc_arrow_fields *v = &entity->arrow->fields;
    const mc_nbt_tag *tag;
    at(e, false, 4, x_tile);
    if (!get(compound, x_tile, MC_NBT_SHORT, &tag, e))
        return false;
    v->d = tag == NULL ? 0 : tag->value.short_value;
    at(e, false, 14, y_tile);
    if (!get(compound, y_tile, MC_NBT_SHORT, &tag, e))
        return false;
    v->e = tag == NULL ? 0 : tag->value.short_value;
    at(e, false, 24, z_tile);
    if (!get(compound, z_tile, MC_NBT_SHORT, &tag, e))
        return false;
    v->f = tag == NULL ? 0 : tag->value.short_value;
    at(e, false, 34, in_tile);
    if (!get(compound, in_tile, MC_NBT_BYTE, &tag, e))
        return false;
    v->g = (tag == NULL ? 0 : (int32_t)tag->value.byte_value) & 255;
    at(e, false, 48, in_data);
    if (!get(compound, in_data, MC_NBT_BYTE, &tag, e))
        return false;
    v->h = (tag == NULL ? 0 : (int32_t)tag->value.byte_value) & 255;
    at(e, false, 62, shake);
    if (!get(compound, shake, MC_NBT_BYTE, &tag, e))
        return false;
    v->b = (tag == NULL ? 0 : (int32_t)tag->value.byte_value) & 255;
    at(e, false, 76, in_ground);
    if (!get(compound, in_ground, MC_NBT_BYTE, &tag, e))
        return false;
    v->i = tag != NULL && tag->value.byte_value == 1;
    at(e, false, 95, player);
    if (!get(compound, player, MC_NBT_BYTE, &tag, e))
        return false;
    v->a = tag != NULL && tag->value.byte_value != 0;
    e->arrow.site.pc = 101;
    e->arrow.returned = true;
    e->field = NULL;
    return true;
}

static mc_nbt_tag scalar(arrow_key key, mc_nbt_type type, int32_t value) {
    mc_nbt_tag tag = {.type = type, .name = {key.units, key.length}};
    if (type == MC_NBT_SHORT)
        tag.value.short_value = mc_java_i16_narrow(value);
    else
        tag.value.byte_value = mc_java_i8_narrow(value);
    return tag;
}

bool mc_arrow_nbt_build_fields_held(mc_entity *entity, const mc_nbt_limits *limits,
                                    mc_nbt_tree **out, mc_entity_nbt_error *e) {
    mc_entity_nbt_error scratch;
    if (e == NULL) {
        mc_entity_nbt_error_init(&scratch);
        e = &scratch;
    }
    if (!mc_entity_has_arrow(entity) || out == NULL)
        return fail(e, MC_ENTITY_NBT_INVALID_ARGUMENT, "Arrow NBT own writer arguments");
    mc_entity_nbt_select_arrow_detail(e);
    e->arrow.returned = false;
    const mc_arrow_fields *v = &entity->arrow->fields;
    mc_nbt_tag fields[8];
    const mc_nbt_tag *items[8];
    at(e, true, 8, x_tile);
    fields[0] = scalar(x_tile, MC_NBT_SHORT, v->d);
    at(e, true, 19, y_tile);
    fields[1] = scalar(y_tile, MC_NBT_SHORT, v->e);
    at(e, true, 30, z_tile);
    fields[2] = scalar(z_tile, MC_NBT_SHORT, v->f);
    at(e, true, 41, in_tile);
    fields[3] = scalar(in_tile, MC_NBT_BYTE, v->g);
    at(e, true, 52, in_data);
    fields[4] = scalar(in_data, MC_NBT_BYTE, v->h);
    at(e, true, 63, shake);
    fields[5] = scalar(shake, MC_NBT_BYTE, v->b);
    at(e, true, 82, in_ground);
    fields[6] = scalar(in_ground, MC_NBT_BYTE, v->i ? 1 : 0);
    at(e, true, 92, player);
    fields[7] = scalar(player, MC_NBT_BYTE, v->a ? 1 : 0);
    e->arrow.site.pc = 95;
    e->arrow.returned = true;
    e->field = NULL;
    for (size_t i = 0; i < 8; ++i)
        items[i] = &fields[i];
    const mc_nbt_tag root = {.type = MC_NBT_COMPOUND, .value.compound = {items, 8}};
    const mc_nbt_status copied = mc_nbt_tree_copy_compound(&root, limits, out, &e->nbt);
    if (copied != MC_NBT_OK)
        return fail(e,
                    copied == MC_NBT_LIMIT               ? MC_ENTITY_NBT_LIMIT
                    : copied == MC_NBT_ALLOCATION_FAILED ? MC_ENTITY_NBT_ALLOCATION_FAILED
                                                         : MC_ENTITY_NBT_NBT_FAILED,
                    "Arrow NBT independent own-field tree");
    return true;
}
