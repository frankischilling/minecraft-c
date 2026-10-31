#include "nbt_internal.h"

#include "../construction_internal.h"
#include "mc/core/java/java.h"
#include "mc/core/math/math.h"
#include "mc/inventory/runtime/nbt.h"

#include <string.h>
#include <xmmintrin.h>

static const uint16_t health[] = {'H', 'e', 'a', 'l', 't', 'h'};
static const uint16_t hurt[] = {'H', 'u', 'r', 't', 'T', 'i', 'm', 'e'};
static const uint16_t death[] = {'D', 'e', 'a', 't', 'h', 'T', 'i', 'm', 'e'};
static const uint16_t attack[] = {'A', 't', 't', 'a', 'c', 'k', 'T', 'i', 'm', 'e'};
static const uint16_t inventory[] = {'I', 'n', 'v', 'e', 'n', 't', 'o', 'r', 'y'};
static const uint16_t dimension[] = {'D', 'i', 'm', 'e', 'n', 's', 'i', 'o', 'n'};
static const uint16_t sleeping[] = {'S', 'l', 'e', 'e', 'p', 'i', 'n', 'g'};
static const uint16_t sleep_timer[] = {'S', 'l', 'e', 'e', 'p', 'T', 'i', 'm', 'e', 'r'};
static const uint16_t spawn_x[] = {'S', 'p', 'a', 'w', 'n', 'X'};
static const uint16_t spawn_y[] = {'S', 'p', 'a', 'w', 'n', 'Y'};
static const uint16_t spawn_z[] = {'S', 'p', 'a', 'w', 'n', 'Z'};
static const uint16_t score[] = {'S', 'c', 'o', 'r', 'e'};

typedef struct player_key {
    const uint16_t *name;
    size_t length;
    const char *field;
} player_key;

#define KEY(n, text) ((player_key){n, sizeof(n) / sizeof(*n), text})

static bool fail(mc_entity_nbt_error *e, mc_entity_nbt_status status, const char *operation) {
    e->status = status;
    e->operation = operation;
    return false;
}

static bool get(const mc_nbt_tag *compound, player_key key, mc_nbt_type type,
                const mc_nbt_tag **out, mc_entity_nbt_error *e) {
    e->field = key.field;
    if (compound == NULL)
        return fail(e, MC_ENTITY_NBT_NULL_REFERENCE, "Player NBT Compound receiver");
    if (compound->type != MC_NBT_COMPOUND)
        return fail(e, MC_ENTITY_NBT_WRONG_TAG, "Player NBT Compound view type");
    const mc_nbt_tag *tag = mc_nbt_compound_get(compound, key.name, key.length);
    if (tag != NULL && tag->type != type)
        return fail(e, MC_ENTITY_NBT_WRONG_TAG, "Player NBT named tag cast");
    *out = tag;
    return true;
}

static bool inventory_result(mc_inventory_nbt_status status, mc_entity_nbt_error *e) {
    if (status == MC_INVENTORY_NBT_OK)
        return true;
    e->index = e->player_inventory.index;
    if (e->player_inventory.field != NULL)
        e->field = e->player_inventory.field;
    return fail(e,
                status == MC_INVENTORY_NBT_LIMIT                 ? MC_ENTITY_NBT_LIMIT
                : status == MC_INVENTORY_NBT_ALLOCATION_FAILED   ? MC_ENTITY_NBT_ALLOCATION_FAILED
                : status == MC_INVENTORY_NBT_NULL_REFERENCE      ? MC_ENTITY_NBT_NULL_REFERENCE
                : status == MC_INVENTORY_NBT_WRONG_TAG           ? MC_ENTITY_NBT_WRONG_TAG
                : status == MC_INVENTORY_NBT_INDEX_OUT_OF_BOUNDS ? MC_ENTITY_NBT_INDEX_OUT_OF_BOUNDS
                : status == MC_INVENTORY_NBT_REGISTRY_FAILED     ? MC_ENTITY_NBT_REGISTRY_FAILED
                : status == MC_INVENTORY_NBT_STACK_FAILED        ? MC_ENTITY_NBT_STACK_FAILED
                                                                 : MC_ENTITY_NBT_NBT_FAILED,
                "Player NBT actual Inventory operation");
}

/* Native ownership nodes wrap real br values; they are not source fields or
 * an arena. Replacing a field immediately retires nodes no longer referenced
 * by any of this Player's three coordinate occurrences. */
struct mc_player_nbt_coordinate {
    mc_player_coordinates value;
    struct mc_player_nbt_coordinate *next;
};

static struct mc_player_nbt_coordinate *coordinate_new(mc_entity *p, mc_entity_nbt_error *e) {
    struct mc_player_nbt_coordinate *value = mc_entity_construction_allocate(
        p, sizeof(*value), &e->construction, "Player NBT actual br allocation", 0);
    if (value == NULL)
        fail(e,
             e->construction.status == MC_ENTITY_LIMIT ? MC_ENTITY_NBT_LIMIT
                                                       : MC_ENTITY_NBT_ALLOCATION_FAILED,
             "Player NBT actual br allocation");
    return value;
}

void mc_player_nbt_coordinates_collect_held(mc_entity *p) {
    mc_local_player_state *s = p->local_player;
    struct mc_player_nbt_coordinate **link = &s->nbt_coordinates;
    while (*link != NULL) {
        struct mc_player_nbt_coordinate *node = *link;
        const mc_player_coordinates *value = &node->value;
        if (value == s->fields.player.v || value == s->fields.player.private_b ||
            value == s->fields.player.private_bN)
            link = &node->next;
        else {
            *link = node->next;
            mc_entity_construction_retire(p, node, sizeof(*node));
        }
    }
}

static void coordinate_store(mc_entity *p, struct mc_player_nbt_coordinate *node,
                             const mc_player_coordinates **field) {
    node->next = p->local_player->nbt_coordinates;
    p->local_player->nbt_coordinates = node;
    *field = &node->value;
    mc_player_nbt_coordinates_collect_held(p);
}

void mc_player_nbt_coordinates_dispose(mc_entity *p) {
    struct mc_player_nbt_coordinate *node = p->local_player->nbt_coordinates;
    while (node != NULL) {
        struct mc_player_nbt_coordinate *next = node->next;
        mc_entity_construction_retire(p, node, sizeof(*node));
        node = next;
    }
    p->local_player->nbt_coordinates = NULL;
}

static bool living_read(mc_entity *p, const mc_nbt_tag *compound, mc_entity_nbt_error *e) {
    mc_living_entity_fields *v = &p->local_player->fields.living;
    const mc_nbt_tag *tag;
    e->living_bytecode = 4;
    if (!get(compound, KEY(health, "Health"), MC_NBT_SHORT, &tag, e))
        return false;
    v->Y = tag == NULL ? 0 : tag->value.short_value; /* source store7 */
    e->living_bytecode = 13;
    if (mc_nbt_compound_get(compound, health, sizeof health / sizeof *health) == NULL)
        v->Y = 10; /* Source default22 follows the first zero store. */
    e->living_bytecode = 29;
    if (!get(compound, KEY(hurt, "HurtTime"), MC_NBT_SHORT, &tag, e))
        return false;
    v->aa = tag == NULL ? 0 : tag->value.short_value;
    e->living_bytecode = 39;
    if (!get(compound, KEY(death, "DeathTime"), MC_NBT_SHORT, &tag, e))
        return false;
    v->ad = tag == NULL ? 0 : tag->value.short_value;
    e->living_bytecode = 49;
    if (!get(compound, KEY(attack, "AttackTime"), MC_NBT_SHORT, &tag, e))
        return false;
    v->ae = tag == NULL ? 0 : tag->value.short_value;
    e->living_bytecode = 55;
    return true;
}

bool mc_player_nbt_read_held(mc_entity *p, mc_registry_classes *classes, const mc_nbt_tag *compound,
                             const mc_nbt_limits *limits, mc_entity_nbt_error *e) {
    mc_entity_nbt_select_player_detail(e);
    e->local_player_bytecode = 2;
    e->player_bytecode = 2;
    if (!living_read(p, compound, e))
        return false;
    mc_player_fields *v = &p->local_player->fields.player;
    const mc_nbt_tag *tag;
    e->player_bytecode = 8;
    if (!get(compound, KEY(inventory, "Inventory"), MC_NBT_LIST, &tag, e))
        return false;
    const mc_nbt_tag empty = {.type = MC_NBT_LIST};
    e->player_bytecode = 17;
    if (!inventory_result(mc_inventory_player_read_nbt(v->c, classes, tag == NULL ? &empty : tag,
                                                       limits, &e->player_inventory),
                          e))
        return false;
    e->player_bytecode = 24;
    if (!get(compound, KEY(dimension, "Dimension"), MC_NBT_INT, &tag, e))
        return false;
    v->m = tag == NULL ? 0 : tag->value.int_value;
    e->player_bytecode = 34;
    if (!get(compound, KEY(sleeping, "Sleeping"), MC_NBT_BYTE, &tag, e))
        return false;
    v->u = tag != NULL && tag->value.byte_value != 0;
    e->player_bytecode = 44;
    if (!get(compound, KEY(sleep_timer, "SleepTimer"), MC_NBT_SHORT, &tag, e))
        return false;
    v->private_a = tag == NULL ? 0 : tag->value.short_value;
    if (v->u) {
        e->player_bytecode = 58;
        struct mc_player_nbt_coordinate *node = coordinate_new(p, e);
        if (node == NULL)
            return false;
        e->player_bytecode = 66;
        /* A direct dc.a caller need not have entered the base pose reader.
         * Inventory class work may also precede this first reached floor.
         * Keep allocation58 and all earlier scalar/array stores, but never
         * execute the source compare with native DAZ/rounding controls. */
        if ((_mm_getcsr() & 0xffc0u) != 0x1f80u) {
            mc_entity_construction_retire(p, node, sizeof(*node));
            return fail(e, MC_ENTITY_NBT_UNSUPPORTED_FP,
                        "Player NBT reached bed-coordinate floor floating environment");
        }
        node->value.x = mc_math_floor_double(p->base.x);
        e->player_bytecode = 73;
        node->value.y = mc_math_floor_double(p->base.y);
        e->player_bytecode = 80;
        node->value.z = mc_math_floor_double(p->base.z);
        e->player_bytecode = 86;
        coordinate_store(p, node, &v->v);
        e->player_bytecode = 93;
        return fail(e, MC_ENTITY_NBT_UNSUPPORTED_TYPE,
                    "Player NBT actual wake(true,true,false) dependency");
    }
    if (mc_nbt_compound_get(compound, spawn_x, 6) != NULL &&
        mc_nbt_compound_get(compound, spawn_y, 6) != NULL &&
        mc_nbt_compound_get(compound, spawn_z, 6) != NULL) {
        e->player_bytecode = 124;
        struct mc_player_nbt_coordinate *node = coordinate_new(p, e);
        if (node == NULL)
            return false;
        const player_key keys[] = {KEY(spawn_x, "SpawnX"), KEY(spawn_y, "SpawnY"),
                                   KEY(spawn_z, "SpawnZ")};
        int32_t *values[] = {&node->value.x, &node->value.y, &node->value.z};
        for (size_t i = 0; i < 3; ++i) {
            e->player_bytecode = 131u + (unsigned int)i * 6u;
            if (!get(compound, keys[i], MC_NBT_INT, &tag, e)) {
                mc_entity_construction_retire(p, node, sizeof(*node));
                return false;
            }
            *values[i] = tag == NULL ? 0 : tag->value.int_value;
        }
        e->player_bytecode = 149;
        coordinate_store(p, node, &v->private_b);
    }
    e->player_bytecode = 152;
    e->local_player_bytecode = 9;
    if (!get(compound, KEY(score, "Score"), MC_NBT_INT, &tag, e))
        return false;
    v->g = tag == NULL ? 0 : tag->value.int_value;
    e->local_player_bytecode = 15;
    return true;
}

static mc_nbt_tag scalar(player_key key, mc_nbt_type type, int32_t value) {
    mc_nbt_tag tag = {.type = type, .name = {key.name, key.length}};
    if (type == MC_NBT_SHORT)
        tag.value.short_value = mc_java_i16_narrow(value);
    else if (type == MC_NBT_BYTE)
        tag.value.byte_value = mc_java_i8_narrow(value);
    else
        tag.value.int_value = value;
    return tag;
}

bool mc_player_nbt_build_fields_held(mc_entity *p, const mc_nbt_limits *requested,
                                     mc_nbt_tree **out, mc_entity_nbt_error *e) {
    mc_entity_nbt_select_player_detail(e);
    mc_local_player_snapshot *s = &p->local_player->fields;
    mc_nbt_tag fields[12];
    const mc_nbt_tag *children[12];
    for (size_t i = 0; i < 12; ++i)
        children[i] = &fields[i];
    e->local_player_bytecode = 2;
    e->player_bytecode = 2;
    e->living_bytecode = 8;
    fields[0] = scalar(KEY(health, "Health"), MC_NBT_SHORT, s->living.Y);
    e->living_bytecode = 19;
    fields[1] = scalar(KEY(hurt, "HurtTime"), MC_NBT_SHORT, s->living.aa);
    e->living_bytecode = 30;
    fields[2] = scalar(KEY(death, "DeathTime"), MC_NBT_SHORT, s->living.ad);
    e->living_bytecode = 41;
    fields[3] = scalar(KEY(attack, "AttackTime"), MC_NBT_SHORT, s->living.ae);
    e->living_bytecode = 44;
    mc_nbt_tree *inventory_tree = NULL;
    e->player_bytecode = 19;
    if (!inventory_result(mc_inventory_player_build_nbt(s->player.c, requested, &inventory_tree,
                                                        &e->player_inventory),
                          e))
        return false;
    const mc_nbt_tag *root = mc_nbt_tree_root(inventory_tree);
    const mc_nbt_tag *list = mc_nbt_compound_get(root, inventory, 9);
    fields[4] = *list;
    e->player_bytecode = 32;
    fields[5] = scalar(KEY(dimension, "Dimension"), MC_NBT_INT, s->player.m);
    e->player_bytecode = 42;
    fields[6] = scalar(KEY(sleeping, "Sleeping"), MC_NBT_BYTE, s->player.u ? 1 : 0);
    e->player_bytecode = 53;
    fields[7] = scalar(KEY(sleep_timer, "SleepTimer"), MC_NBT_SHORT, s->player.private_a);
    size_t count = 8;
    if (s->player.private_b != NULL) {
        e->player_bytecode = 73;
        fields[count++] = scalar(KEY(spawn_x, "SpawnX"), MC_NBT_INT, s->player.private_b->x);
        e->player_bytecode = 86;
        fields[count++] = scalar(KEY(spawn_y, "SpawnY"), MC_NBT_INT, s->player.private_b->y);
        e->player_bytecode = 99;
        fields[count++] = scalar(KEY(spawn_z, "SpawnZ"), MC_NBT_INT, s->player.private_b->z);
    }
    e->player_bytecode = 102;
    e->local_player_bytecode = 12;
    fields[count++] = scalar(KEY(score, "Score"), MC_NBT_INT, s->player.g);
    const mc_nbt_tag result = {.type = MC_NBT_COMPOUND, .value.compound = {children, count}};
    mc_nbt_limits remaining = requested == NULL ? mc_nbt_default_limits() : *requested;
    const size_t retained = mc_nbt_tree_allocation_bytes(inventory_tree);
    bool completed = false;
    if (retained > remaining.max_allocation_bytes)
        fail(e, MC_ENTITY_NBT_LIMIT, "Player NBT retained Inventory tree budget");
    else {
        remaining.max_allocation_bytes -= retained;
        const mc_nbt_status copied = mc_nbt_tree_copy_compound(&result, &remaining, out, &e->nbt);
        if (copied == MC_NBT_OK)
            completed = true;
        else
            fail(e,
                 copied == MC_NBT_LIMIT               ? MC_ENTITY_NBT_LIMIT
                 : copied == MC_NBT_ALLOCATION_FAILED ? MC_ENTITY_NBT_ALLOCATION_FAILED
                                                      : MC_ENTITY_NBT_NBT_FAILED,
                 "Player NBT independent own-field tree");
    }
    mc_nbt_tree_free(inventory_tree);
    return completed;
}
