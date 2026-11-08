#include "../player/player_internal.h"
#include "../watcher_payload_internal.h"
#include "internal.h"
#include "mc/core/java/java.h"
#include "nbt_internal.h"
#include <string.h>

typedef struct mob_key {
    const uint16_t *units;
    size_t length;
    const char *field;
} mob_key;

#define KEY(name, text, ...)                                                                       \
    static const uint16_t name##_units[] = {__VA_ARGS__};                                          \
    static const mob_key name = {name##_units, sizeof(name##_units) / sizeof(*name##_units), text}
KEY(health, "Health", 'H', 'e', 'a', 'l', 't', 'h');
KEY(hurt, "HurtTime", 'H', 'u', 'r', 't', 'T', 'i', 'm', 'e');
KEY(death, "DeathTime", 'D', 'e', 'a', 't', 'h', 'T', 'i', 'm', 'e');
KEY(attack, "AttackTime", 'A', 't', 't', 'a', 'c', 'k', 'T', 'i', 'm', 'e');
KEY(powered, "powered", 'p', 'o', 'w', 'e', 'r', 'e', 'd');
KEY(size, "Size", 'S', 'i', 'z', 'e');
KEY(saddle, "Saddle", 'S', 'a', 'd', 'd', 'l', 'e');
KEY(sheared, "Sheared", 'S', 'h', 'e', 'a', 'r', 'e', 'd');
KEY(color, "Color", 'C', 'o', 'l', 'o', 'r');
KEY(angry, "Angry", 'A', 'n', 'g', 'r', 'y');
KEY(sitting, "Sitting", 'S', 'i', 't', 't', 'i', 'n', 'g');
KEY(owner, "Owner", 'O', 'w', 'n', 'e', 'r');
#undef KEY

static mc_mob_site site(const char *cl, const char *method, const char *descriptor, int pc) {
    return (mc_mob_site){cl, method, descriptor, pc};
}

static bool fail(mc_entity_nbt_error *e, mc_entity_nbt_status status, const char *operation,
                 const char *exception) {
    e->status = status;
    e->operation = operation;
    e->mob.source_exception = exception != NULL;
    e->mob.native_policy = exception == NULL;
    e->mob.exception_class = exception;
    return false;
}

static void hook(mc_entity *entity, bool write, mc_entity_nbt_error *e) {
    mc_entity_nbt_select_mob_detail(e);
    const char *cl = entity->class_literal->client_name;
    const char *parent = NULL;
    switch (entity->class_literal->kind) {
        case MC_ENTITY_CLASS_KIND_GHAST:
            cl = "ls";
            break; /* BP/WQ inherit the actual Living NBT hooks. */
        case MC_ENTITY_CLASS_KIND_ZOMBIE:
            cl = "gz";
            break; /* inherited actual hook */
        case MC_ENTITY_CLASS_KIND_SPIDER:
        case MC_ENTITY_CLASS_KIND_SKELETON:
        case MC_ENTITY_CLASS_KIND_CREEPER:
            parent = "gz";
            break;
        case MC_ENTITY_CLASS_KIND_PIG:
        case MC_ENTITY_CLASS_KIND_SHEEP:
        case MC_ENTITY_CLASS_KIND_COW:
        case MC_ENTITY_CLASS_KIND_CHICKEN:
        case MC_ENTITY_CLASS_KIND_WOLF:
            parent = "bg";
            break;
        case MC_ENTITY_CLASS_KIND_SQUID:
            parent = "ar";
            break;
        default:
            break;
    }
    e->mob.concrete = site(cl, write ? "b" : "a", "(Lnu;)V", 2);
    if (parent != NULL)
        e->mob.parent = site(parent, write ? "b" : "a", "(Lnu;)V", 2);
}

static void concrete_at(mc_entity_nbt_error *e, int pc) {
    e->mob.concrete.pc = pc;
    e->mob.child = (mc_mob_site){.pc = -1};
    e->mob.watcher = (mc_mob_site){.pc = -1};
}

static bool get(const mc_nbt_tag *compound, mob_key key, mc_nbt_type type, const mc_nbt_tag **out,
                mc_entity_nbt_error *e) {
    e->field = key.field;
    e->index = SIZE_MAX;
    if (compound == NULL)
        return fail(e, MC_ENTITY_NBT_NULL_REFERENCE, "Mob NBT Compound receiver",
                    "java/lang/NullPointerException");
    if (compound->type != MC_NBT_COMPOUND)
        return fail(e, MC_ENTITY_NBT_WRONG_TAG, "Mob NBT Compound view type", NULL);
    const mc_nbt_tag *value = mc_nbt_compound_get(compound, key.units, key.length);
    if (value != NULL && value->type != type)
        return fail(e, MC_ENTITY_NBT_WRONG_TAG, "Mob NBT named tag cast",
                    "java/lang/ClassCastException");
    *out = value;
    return true;
}

/* These are the existing actual UD/MA owners. The type projection represents
 * canonical boxed Byte/String values. A noncanonical setter old-type mismatch
 * is an explicit native representation policy, not a fabricated Java cast. */
static mc_entity_watch_record *watch(mc_entity *entity, int index, int type, bool setter,
                                     mc_entity_nbt_error *e) {
    if (entity->watcher == NULL) {
        fail(e, MC_ENTITY_NBT_NULL_REFERENCE, "Mob NBT null watcher invoke receiver",
             "java/lang/NullPointerException");
        return NULL;
    }
    e->mob.watcher = site("ud",
                          setter      ? "b"
                          : type == 4 ? "c"
                                      : "a",
                          setter      ? "(ILjava/lang/Object;)V"
                          : type == 4 ? "(I)Ljava/lang/String;"
                                      : "(I)B",
                          8);
    mc_entity_watch_map *map = entity->watcher->map;
    if (map == NULL) {
        fail(e, MC_ENTITY_NBT_NULL_REFERENCE, "Mob NBT watcher null Map receiver",
             "java/lang/NullPointerException");
        return NULL;
    }
    size_t seen = 0;
    mc_entity_watch_record *record = NULL;
    for (mc_entity_watch_node *node = map->head; node != NULL; node = node->next) {
        if (seen++ >= map->size) {
            fail(e, MC_ENTITY_NBT_UNSUPPORTED_TYPE, "Mob NBT watcher Map native ownership", NULL);
            return NULL;
        }
        if (node->key == index) {
            record = node->record;
            break;
        }
    }
    e->mob.watcher.pc = setter ? 19 : 16;
    if (record == NULL) {
        fail(e, MC_ENTITY_NBT_NULL_REFERENCE, "Mob NBT watcher null record receiver",
             "java/lang/NullPointerException");
        return NULL;
    }
    e->mob.watcher.pc = setter ? 22 : 19;
    const mc_entity_watch_payload_result payload =
        mc_entity_watch_payload_check(record, (mc_entity_watch_payload_kind)type);
    if (setter ? !mc_entity_watch_payload_known(record) : payload != MC_ENTITY_WATCH_PAYLOAD_OK) {
        if (setter)
            fail(e, MC_ENTITY_NBT_UNSUPPORTED_TYPE, "Mob NBT setter boxed native representation",
                 NULL);
        else if (payload == MC_ENTITY_WATCH_PAYLOAD_NULL_UNBOX) {
            e->mob.watcher.pc = 22;
            fail(e, MC_ENTITY_NBT_NULL_REFERENCE, "Mob NBT null Byte unbox receiver",
                 "java/lang/NullPointerException");
        } else if (payload == MC_ENTITY_WATCH_PAYLOAD_WRONG_CLASS)
            fail(e, MC_ENTITY_NBT_WRONG_TAG, "Mob NBT watcher actual boxed cast",
                 "java/lang/ClassCastException");
        else
            fail(e, MC_ENTITY_NBT_UNSUPPORTED_TYPE, "Mob NBT getter boxed native representation",
                 NULL);
        return NULL;
    }
    return record;
}

static bool read_byte(mc_entity *entity, int index, int8_t *out, mc_entity_nbt_error *e) {
    mc_entity_watch_record *record = watch(entity, index, 0, false, e);
    if (record == NULL)
        return false;
    *out = record->byte_value;
    return true;
}

static bool store_byte(mc_entity *entity, int index, int8_t value, mc_entity_nbt_error *e) {
    mc_entity_watch_record *record = watch(entity, index, 0, true, e);
    if (record == NULL)
        return false;
    if (mc_entity_watch_byte_replace(record, value))
        entity->watcher->dirty = true;
    return true;
}

static bool bits(mc_entity *entity, const char *cl, const char *method, int mask, bool value,
                 int yes_pc, int no_pc, mc_entity_nbt_error *e) {
    e->mob.child = site(cl, method, "(Z)V", 6);
    int8_t previous;
    if (!read_byte(entity, 16, &previous, e))
        return false;
    const int8_t next = mc_java_i8_narrow(value ? previous | mask : previous & ~mask);
    e->mob.child.pc = value ? yes_pc : no_pc;
    return store_byte(entity, 16, next, e);
}

static bool living_read(mc_entity *entity, const mc_nbt_tag *compound, mc_entity_nbt_error *e) {
    mc_living_entity_fields *v = &entity->mob->living;
    const mc_nbt_tag *tag;
    e->mob.living_bytecode = 4;
    if (!get(compound, health, MC_NBT_SHORT, &tag, e))
        return false;
    v->Y = tag == NULL ? 0 : tag->value.short_value;
    e->mob.living_bytecode = 13;
    if (mc_nbt_compound_get(compound, health.units, health.length) == NULL)
        v->Y = 10;
    e->mob.living_bytecode = 29;
    if (!get(compound, hurt, MC_NBT_SHORT, &tag, e))
        return false;
    v->aa = tag == NULL ? 0 : tag->value.short_value;
    e->mob.living_bytecode = 39;
    if (!get(compound, death, MC_NBT_SHORT, &tag, e))
        return false;
    v->ad = tag == NULL ? 0 : tag->value.short_value;
    e->mob.living_bytecode = 49;
    if (!get(compound, attack, MC_NBT_SHORT, &tag, e))
        return false;
    v->ae = tag == NULL ? 0 : tag->value.short_value;
    e->mob.living_bytecode = 55;
    return true;
}

static bool slime_size(mc_entity *entity, int32_t requested, mc_entity_nbt_error *e) {
    mc_mob_error call = {.site = {.pc = -1}, .child = {.pc = -1}};
    if (mc_mob_slime_size_held(entity, requested, &call))
        return true;
    e->mob.call_kind = MC_ENTITY_MOB_NBT_SLIME_SIZE;
    e->mob.call_status = call.status;
    e->mob.child = call.site;
    e->mob.watcher = call.child;
    e->construction = call.entity;
    e->stack.status = call.stack;
    fail(e,
         call.status == MC_MOB_UNSUPPORTED_FP ? MC_ENTITY_NBT_UNSUPPORTED_FP
                                              : MC_ENTITY_NBT_ENTITY_FAILED,
         call.operation, call.exception_class);
    e->mob.native_policy = call.native_policy;
    e->mob.source_exception = call.source_exception;
    return false;
}

static bool owner_store(mc_entity *entity, mc_nbt_string input, const mc_nbt_limits *requested,
                        mc_entity_nbt_error *e) {
    e->mob.child = site("gi", "a", "(Ljava/lang/String;)V", 7);
    mc_entity_watch_record *record = watch(entity, 17, 4, true, e);
    if (record == NULL)
        return false;
    if (input.length != 0 && input.units == NULL)
        return fail(e, MC_ENTITY_NBT_INVALID_ARGUMENT, "Wolf NBT source String span", NULL);
    mc_player_string_view old = {0};
    const bool present = mc_player_string_read(mc_entity_watch_string_value(record), &old);
    /* The actual new String.equals(old) controls both field and dirty stores. */
    if (present && old.length == input.length &&
        (input.length == 0 || memcmp(old.units, input.units, input.length * sizeof(uint16_t)) == 0))
        return true;
    const mc_nbt_limits limits = requested == NULL ? mc_nbt_default_limits() : *requested;
    if (input.length > (SIZE_MAX - sizeof(mc_player_string)) / sizeof(uint16_t))
        return fail(e, MC_ENTITY_NBT_LIMIT, "Wolf NBT native owner String extent", NULL);
    const size_t bytes = sizeof(mc_player_string) + input.length * sizeof(uint16_t);
    if (entity->mob->nbt_owner_bytes > limits.max_allocation_bytes ||
        bytes > limits.max_allocation_bytes - entity->mob->nbt_owner_bytes)
        return fail(e, MC_ENTITY_NBT_LIMIT, "Wolf NBT overlapping native String owner bound", NULL);
    mc_player_string *replacement = NULL;
    e->mob.string_status = mc_player_string_create(input.units, input.length, &replacement);
    if (e->mob.string_status != MC_PLAYER_OK)
        return fail(e,
                    e->mob.string_status == MC_PLAYER_LIMIT ? MC_ENTITY_NBT_LIMIT
                    : e->mob.string_status == MC_PLAYER_ALLOCATION_FAILED
                        ? MC_ENTITY_NBT_ALLOCATION_FAILED
                        : MC_ENTITY_NBT_INVALID_ARGUMENT,
                    "Wolf NBT native owner String copy", NULL);
    mc_player_string *previous = entity->mob->nbt_owner_hold;
    record->payload_kind = MC_ENTITY_WATCH_PAYLOAD_STRING;
    record->string_value = replacement;
    entity->mob->nbt_owner_hold = replacement;
    entity->mob->nbt_owner_bytes = bytes;
    record->dirty = true;
    entity->watcher->dirty = true;
    mc_player_string_release(previous);
    return true;
}

bool mc_mob_nbt_read_held(mc_entity *entity, const mc_nbt_tag *compound,
                          const mc_nbt_limits *limits, mc_entity_nbt_error *e) {
    hook(entity, false, e);
    if (!living_read(entity, compound, e))
        return false;
    const mc_nbt_tag *tag;
    const mc_entity_class_kind kind = entity->class_literal->kind;
    if (kind == MC_ENTITY_CLASS_KIND_CREEPER) {
        concrete_at(e, 14);
        if (!get(compound, powered, MC_NBT_BYTE, &tag, e))
            return false;
        concrete_at(e, 29);
        if (!store_byte(entity, 17, tag != NULL && tag->value.byte_value != 0 ? 1 : 0, e))
            return false;
    } else if (kind == MC_ENTITY_CLASS_KIND_SLIME) {
        concrete_at(e, 9);
        if (!get(compound, size, MC_NBT_INT, &tag, e))
            return false;
        const int32_t requested = mc_java_i32_add(tag == NULL ? 0 : tag->value.int_value, 1);
        concrete_at(e, 14);
        if (!slime_size(entity, requested, e))
            return false;
    } else if (kind == MC_ENTITY_CLASS_KIND_PIG) {
        concrete_at(e, 9);
        if (!get(compound, saddle, MC_NBT_BYTE, &tag, e))
            return false;
        const bool value = tag != NULL && tag->value.byte_value != 0;
        concrete_at(e, 12);
        e->mob.child = site("wh", "a", "(Z)V", value ? 14 : 30);
        if (!store_byte(entity, 16, value ? 1 : 0, e))
            return false;
    } else if (kind == MC_ENTITY_CLASS_KIND_SHEEP) {
        concrete_at(e, 9);
        if (!get(compound, sheared, MC_NBT_BYTE, &tag, e))
            return false;
        concrete_at(e, 12);
        if (!bits(entity, "dl", "a", 16, tag != NULL && tag->value.byte_value != 0, 28, 48, e))
            return false;
        concrete_at(e, 19);
        if (!get(compound, color, MC_NBT_BYTE, &tag, e))
            return false;
        const int32_t value = tag == NULL ? 0 : tag->value.byte_value;
        concrete_at(e, 22);
        e->mob.child = site("dl", "e_", "(I)V", 6);
        int8_t previous;
        if (!read_byte(entity, 16, &previous, e))
            return false;
        e->mob.child.pc = 30;
        if (!store_byte(entity, 16, mc_java_i8_narrow((previous & 240) | (value & 15)), e))
            return false;
    } else if (kind == MC_ENTITY_CLASS_KIND_WOLF) {
        concrete_at(e, 9);
        if (!get(compound, angry, MC_NBT_BYTE, &tag, e))
            return false;
        concrete_at(e, 12);
        if (!bits(entity, "gi", "c", 2, tag != NULL && tag->value.byte_value != 0, 27, 47, e))
            return false;
        concrete_at(e, 19);
        if (!get(compound, sitting, MC_NBT_BYTE, &tag, e))
            return false;
        concrete_at(e, 22);
        if (!bits(entity, "gi", "b", 1, tag != NULL && tag->value.byte_value != 0, 27, 47, e))
            return false;
        concrete_at(e, 28);
        if (!get(compound, owner, MC_NBT_STRING, &tag, e))
            return false;
        const mc_nbt_string value = tag == NULL ? (mc_nbt_string){0} : tag->value.string;
        concrete_at(e, 33);
        if (value.length != 0) {
            concrete_at(e, 41);
            if (!owner_store(entity, value, limits, e))
                return false;
            concrete_at(e, 46);
            if (!bits(entity, "gi", "d", 4, true, 27, 47, e))
                return false;
        }
        /* Missing/empty Owner does not clear the old owner or tame flag. */
    }
    return true;
}

static mc_nbt_tag scalar(mob_key key, mc_nbt_type type, int32_t value) {
    mc_nbt_tag tag = {.type = type, .name = {key.units, key.length}};
    if (type == MC_NBT_SHORT)
        tag.value.short_value = mc_java_i16_narrow(value);
    else if (type == MC_NBT_BYTE)
        tag.value.byte_value = mc_java_i8_narrow(value);
    else
        tag.value.int_value = value;
    return tag;
}

bool mc_mob_nbt_build_fields_held(mc_entity *entity, const mc_nbt_limits *limits, mc_nbt_tree **out,
                                  mc_entity_nbt_error *e) {
    hook(entity, true, e);
    const mc_living_entity_fields *v = &entity->mob->living;
    mc_nbt_tag fields[7];
    const mc_nbt_tag *items[7];
    size_t count = 4;
    e->field = health.field;
    e->mob.living_bytecode = 8;
    fields[0] = scalar(health, MC_NBT_SHORT, v->Y);
    e->field = hurt.field;
    e->mob.living_bytecode = 19;
    fields[1] = scalar(hurt, MC_NBT_SHORT, v->aa);
    e->field = death.field;
    e->mob.living_bytecode = 30;
    fields[2] = scalar(death, MC_NBT_SHORT, v->ad);
    e->field = attack.field;
    e->mob.living_bytecode = 41;
    fields[3] = scalar(attack, MC_NBT_SHORT, v->ae);
    e->mob.living_bytecode = 44;
    int8_t value;
    const mc_entity_class_kind kind = entity->class_literal->kind;
    if (kind == MC_ENTITY_CLASS_KIND_CREEPER) {
        e->field = powered.field;
        concrete_at(e, 11);
        if (!read_byte(entity, 17, &value, e))
            return false;
        if (value == 1) {
            concrete_at(e, 22);
            fields[count++] = scalar(powered, MC_NBT_BYTE, 1);
        }
    } else if (kind == MC_ENTITY_CLASS_KIND_SLIME) {
        e->field = size.field;
        concrete_at(e, 9);
        e->mob.child = site("uw", "v", "()I", 6);
        if (!read_byte(entity, 16, &value, e))
            return false;
        concrete_at(e, 14);
        fields[count++] = scalar(size, MC_NBT_INT, mc_java_i32_sub((int32_t)value, 1));
    } else if (kind == MC_ENTITY_CLASS_KIND_PIG) {
        e->field = saddle.field;
        concrete_at(e, 9);
        e->mob.child = site("wh", "v", "()Z", 6);
        if (!read_byte(entity, 16, &value, e))
            return false;
        concrete_at(e, 12);
        fields[count++] = scalar(saddle, MC_NBT_BYTE, (value & 1) != 0);
    } else if (kind == MC_ENTITY_CLASS_KIND_SHEEP) {
        e->field = sheared.field;
        concrete_at(e, 9);
        e->mob.child = site("dl", "s", "()Z", 6);
        if (!read_byte(entity, 16, &value, e))
            return false;
        concrete_at(e, 12);
        fields[count++] = scalar(sheared, MC_NBT_BYTE, (value & 16) != 0);
        e->field = color.field;
        concrete_at(e, 19);
        e->mob.child = site("dl", "r", "()I", 6);
        if (!read_byte(entity, 16, &value, e))
            return false;
        concrete_at(e, 23);
        fields[count++] = scalar(color, MC_NBT_BYTE, value & 15);
    } else if (kind == MC_ENTITY_CLASS_KIND_WOLF) {
        e->field = angry.field;
        concrete_at(e, 9);
        e->mob.child = site("gi", "C", "()Z", 6);
        if (!read_byte(entity, 16, &value, e))
            return false;
        concrete_at(e, 12);
        fields[count++] = scalar(angry, MC_NBT_BYTE, (value & 2) != 0);
        e->field = sitting.field;
        concrete_at(e, 19);
        e->mob.child = site("gi", "B", "()Z", 6);
        if (!read_byte(entity, 16, &value, e))
            return false;
        concrete_at(e, 22);
        fields[count++] = scalar(sitting, MC_NBT_BYTE, (value & 1) != 0);
        e->field = owner.field;
        concrete_at(e, 26);
        e->mob.child = site("gi", "A", "()Ljava/lang/String;", 6);
        mc_entity_watch_record *record = watch(entity, 17, 4, false, e);
        if (record == NULL)
            return false;
        mc_player_string_view text = {0};
        if (mc_entity_watch_string_value(record) == NULL)
            concrete_at(e, 37);
        else {
            concrete_at(e, 47);
            e->mob.child = site("gi", "A", "()Ljava/lang/String;", 6);
            record = watch(entity, 17, 4, false, e); /* Actual second A() read. */
            if (record == NULL)
                return false;
            if (!mc_player_string_read(mc_entity_watch_string_value(record), &text))
                return fail(e, MC_ENTITY_NBT_NULL_REFERENCE,
                            "Wolf NBT actual String setter null value",
                            "java/lang/NullPointerException");
            concrete_at(e, 50);
        }
        fields[count++] = (mc_nbt_tag){.type = MC_NBT_STRING,
                                       .name = {owner.units, owner.length},
                                       .value.string = {text.units, text.length}};
    }
    for (size_t i = 0; i < count; ++i)
        items[i] = &fields[i];
    const mc_nbt_tag root = {.type = MC_NBT_COMPOUND, .value.compound = {items, count}};
    e->field = NULL;
    const mc_nbt_status copied = mc_nbt_tree_copy_compound(&root, limits, out, &e->nbt);
    if (copied != MC_NBT_OK)
        return fail(e,
                    copied == MC_NBT_LIMIT               ? MC_ENTITY_NBT_LIMIT
                    : copied == MC_NBT_ALLOCATION_FAILED ? MC_ENTITY_NBT_ALLOCATION_FAILED
                                                         : MC_ENTITY_NBT_NBT_FAILED,
                    "Mob NBT independent own-field tree", NULL);
    return true;
}

size_t mc_mob_nbt_owner_bytes(const mc_entity *entity) {
    return mc_entity_has_mob(entity) ? entity->mob->nbt_owner_bytes : 0;
}

void mc_mob_nbt_dispose(mc_entity *entity) {
    mc_player_string_release(entity->mob->nbt_owner_hold);
    entity->mob->nbt_owner_hold = NULL;
    entity->mob->nbt_owner_bytes = 0;
}
