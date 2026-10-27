/* Genuine constructed App/World/LocalPlayer fixture, with authored scalar/NBT
 * inputs corresponding to PlayerNbtObservation. No captured answer drives it. */
#include "../../../src/client/application_state.h"
#include "../../../src/entity/player/nbt_internal.h"
#include "../../../src/inventory/internal.h"
#include "mc/client/input/movement_input.h"
#include "mc/entity/nbt/factory.h"

typedef struct fixture fixture;
static bool nbt_cases(fixture *, client_application *, mc_entity *, mc_movement_input *);
#define MC_PLAYER_ACTIVE_TICK_MAIN mc_nbt_fixture_main
#define MC_PLAYER_ACTIVE_TICK_CASES nbt_cases
#include "test_tick.c"
#undef MC_PLAYER_ACTIVE_TICK_CASES
#undef MC_PLAYER_ACTIVE_TICK_MAIN

typedef struct nbt_input_node {
    mc_nbt_tag tag;
    uint16_t name[24];
    const mc_nbt_tag *children[40];
} nbt_input_node;

typedef struct nbt_input {
    nbt_input_node nodes[96];
    size_t count;
} nbt_input;

static nbt_input_node *nbt_node(nbt_input *a, const char *name, mc_nbt_type type) {
    if (a->count == COUNT(a->nodes) || strlen(name) > COUNT(a->nodes[0].name))
        return NULL;
    nbt_input_node *n = &a->nodes[a->count++];
    *n = (nbt_input_node){0};
    n->tag.type = type;
    n->tag.name = (mc_nbt_string){n->name, strlen(name)};
    for (size_t i = 0; i < n->tag.name.length; ++i)
        n->name[i] = (uint16_t)(unsigned char)name[i];
    if (type == MC_NBT_COMPOUND)
        n->tag.value.compound.items = n->children;
    if (type == MC_NBT_LIST)
        n->tag.value.list.items = n->children;
    return n;
}

static bool nbt_same_name(const mc_nbt_tag *x, const mc_nbt_tag *y) {
    return x->name.length == y->name.length &&
           memcmp(x->name.units, y->name.units, x->name.length * sizeof(uint16_t)) == 0;
}

static bool nbt_add(nbt_input_node *parent, nbt_input_node *child) {
    if (parent == NULL || child == NULL)
        return false;
    if (parent->tag.type == MC_NBT_COMPOUND) {
        size_t *count = &parent->tag.value.compound.length;
        for (size_t i = 0; i < *count; ++i)
            if (nbt_same_name(parent->children[i], &child->tag)) {
                parent->children[i] = &child->tag;
                return true;
            }
        if (*count == COUNT(parent->children))
            return false;
        parent->children[(*count)++] = &child->tag;
        return true;
    }
    if (parent->tag.type != MC_NBT_LIST || parent->tag.value.list.length == COUNT(parent->children))
        return false;
    parent->tag.value.list.element_type = (uint8_t)child->tag.type;
    parent->children[parent->tag.value.list.length++] = &child->tag;
    return true;
}

static bool nbt_scalar(nbt_input *a, nbt_input_node *parent, const char *name, mc_nbt_type type,
                       int32_t value) {
    nbt_input_node *n = nbt_node(a, name, type);
    if (n == NULL)
        return false;
    if (type == MC_NBT_BYTE)
        n->tag.value.byte_value = mc_java_i8_narrow(value);
    else if (type == MC_NBT_SHORT)
        n->tag.value.short_value = mc_java_i16_narrow(value);
    else
        n->tag.value.int_value = value;
    return nbt_add(parent, n);
}

static bool nbt_vector(nbt_input *a, nbt_input_node *root, const char *name, mc_nbt_type type,
                       const double *values, size_t count) {
    nbt_input_node *list = nbt_node(a, name, MC_NBT_LIST);
    if (!nbt_add(root, list))
        return false;
    for (size_t i = 0; i < count; ++i) {
        nbt_input_node *v = nbt_node(a, "", type);
        if (v == NULL)
            return false;
        if (type == MC_NBT_DOUBLE)
            v->tag.value.double_bits = movement_double_bits(values[i]);
        else
            v->tag.value.float_bits = movement_float_bits((float)values[i]);
        if (!nbt_add(list, v))
            return false;
    }
    return true;
}

static nbt_input_node *nbt_stack(nbt_input *a, int slot, int id, int count, int damage) {
    nbt_input_node *s = nbt_node(a, "", MC_NBT_COMPOUND);
    if ((slot != -1 && !nbt_scalar(a, s, "Slot", MC_NBT_BYTE, slot)) ||
        !nbt_scalar(a, s, "id", MC_NBT_SHORT, id) ||
        !nbt_scalar(a, s, "Count", MC_NBT_BYTE, count) ||
        !nbt_scalar(a, s, "Damage", MC_NBT_SHORT, damage))
        return NULL;
    return s;
}

static const mc_nbt_tag *nbt_make_input(nbt_input *a, const char *name) {
    memset(a, 0, sizeof(*a));
    nbt_input_node *root = nbt_node(a, "", MC_NBT_COMPOUND);
    const double pos[] = {8.5, 80, 8.5}, motion[] = {.1, .2, .3}, rotation[] = {450, -450};
    if (!nbt_vector(a, root, "Pos", MC_NBT_DOUBLE, pos, COUNT(pos)) ||
        !nbt_vector(a, root, "Motion", MC_NBT_DOUBLE, motion, COUNT(motion)) ||
        !nbt_vector(a, root, "Rotation", MC_NBT_FLOAT, rotation, COUNT(rotation)))
        return NULL;
    if (strcmp(name, "base_only") == 0)
        return &root->tag;
    const char *keys[] = {"Fire",      "Air",        "OnGround",  "Health",   "HurtTime",
                          "DeathTime", "AttackTime", "Dimension", "Sleeping", "SleepTimer",
                          "Score",     "SpawnX",     "SpawnY",    "SpawnZ"};
    const mc_nbt_type types[] = {MC_NBT_SHORT, MC_NBT_SHORT, MC_NBT_BYTE,  MC_NBT_SHORT,
                                 MC_NBT_SHORT, MC_NBT_SHORT, MC_NBT_SHORT, MC_NBT_INT,
                                 MC_NBT_BYTE,  MC_NBT_SHORT, MC_NBT_INT,   MC_NBT_INT,
                                 MC_NBT_INT,   MC_NBT_INT};
    const int32_t values[] = {-19, 298, 1, 17, 2, 3, 4, -1, 0, 23, 123, 5, 66, -7};
    nbt_input_node *fall = nbt_node(a, "FallDistance", MC_NBT_FLOAT);
    if (fall == NULL)
        return NULL;
    fall->tag.value.float_bits = movement_float_bits(3.5f);
    if (!nbt_add(root, fall))
        return NULL;
    for (size_t i = 0; i < COUNT(keys); ++i)
        if (!(strcmp(name, "spawn_partial") == 0 && i == 13) &&
            !(strcmp(name, "score_absent") == 0 && i == 10) &&
            !nbt_scalar(a, root, keys[i], types[i], values[i]))
            return NULL;
    nbt_input_node *list = nbt_node(a, "Inventory", MC_NBT_LIST);
    nbt_input_node *stack = nbt_stack(a, 0, 1, 3, 4);
    if (!nbt_add(root, list) || !nbt_add(list, stack))
        return NULL;
    const char *wrong_cases[] = {"health_wrong",   "hurt_wrong",      "death_wrong",
                                 "attack_wrong",   "inventory_wrong", "dimension_wrong",
                                 "sleeping_wrong", "timer_wrong",     "spawn_x_wrong",
                                 "spawn_y_wrong",  "spawn_z_wrong",   "score_wrong"};
    const char *wrong_keys[] = {"Health",    "HurtTime",  "DeathTime", "AttackTime",
                                "Inventory", "Dimension", "Sleeping",  "SleepTimer",
                                "SpawnX",    "SpawnY",    "SpawnZ",    "Score"};
    for (size_t i = 0; i < COUNT(wrong_cases); ++i)
        if (strcmp(name, wrong_cases[i]) == 0 &&
            !nbt_scalar(a, root, wrong_keys[i], i < 5 || i == 7 ? MC_NBT_INT : MC_NBT_SHORT,
                        i < 5               ? 99
                        : i == 5 || i == 11 ? 3
                                            : 1))
            return NULL;
    if (strcmp(name, "entry_wrong") == 0 || strcmp(name, "partial_list") == 0) {
        if (strcmp(name, "entry_wrong") == 0)
            list->tag.value.list.length = 0;
        if (!nbt_scalar(a, list, "", MC_NBT_INT, 7))
            return NULL;
    }
    if (strcmp(name, "entry_null") == 0)
        list->children[0] = NULL; /* Deliberate reached invalid child; never encoded. */
    if (strcmp(name, "slot_wrong") == 0 && !nbt_scalar(a, stack, "Slot", MC_NBT_SHORT, 7))
        return NULL;
    if (strcmp(name, "id_wrong") == 0 && !nbt_scalar(a, stack, "id", MC_NBT_INT, 7))
        return NULL;
    if (strcmp(name, "count_wrong") == 0 && !nbt_scalar(a, stack, "Count", MC_NBT_INT, 7))
        return NULL;
    if (strcmp(name, "damage_wrong") == 0 && !nbt_scalar(a, stack, "Damage", MC_NBT_INT, 7))
        return NULL;
    if (strcmp(name, "id_negative") == 0 && !nbt_scalar(a, stack, "id", MC_NBT_SHORT, -1))
        return NULL;
    if (strcmp(name, "id_null") == 0 && !nbt_scalar(a, stack, "id", MC_NBT_SHORT, 3200))
        return NULL;
    if (strcmp(name, "outside_slot") == 0 && !nbt_scalar(a, stack, "Slot", MC_NBT_BYTE, 50))
        return NULL;
    if (strcmp(name, "slot_255") == 0 && !nbt_scalar(a, stack, "Slot", MC_NBT_BYTE, 255))
        return NULL;
    if (strcmp(name, "slot_missing") == 0) {
        list->tag.value.list.length = 0;
        if (!nbt_add(list, nbt_stack(a, -1, 1, 3, 4)))
            return NULL;
    }
    if (strcmp(name, "duplicate_slot") == 0 && !nbt_add(list, nbt_stack(a, 0, 3, -2, -5)))
        return NULL;
    if (strcmp(name, "armor_slots") == 0) {
        list->tag.value.list.length = 0;
        if (!nbt_add(list, nbt_stack(a, 100, 1, 3, 4)) ||
            !nbt_add(list, nbt_stack(a, 103, 3, -2, -5)))
            return NULL;
    }
    if (strcmp(name, "spawn_good") == 0 && (!nbt_scalar(a, root, "SpawnX", MC_NBT_INT, INT32_MIN) ||
                                            !nbt_scalar(a, root, "SpawnY", MC_NBT_INT, INT32_MAX) ||
                                            !nbt_scalar(a, root, "SpawnZ", MC_NBT_INT, 0)))
        return NULL;
    if (strcmp(name, "sleep") == 0 && !nbt_scalar(a, root, "Sleeping", MC_NBT_BYTE, 1))
        return NULL;
    return &root->tag;
}

static int nbt_name_order(const mc_nbt_tag *a, const mc_nbt_tag *b) {
    const size_t count = a->name.length < b->name.length ? a->name.length : b->name.length;
    for (size_t i = 0; i < count; ++i)
        if (a->name.units[i] != b->name.units[i])
            return a->name.units[i] < b->name.units[i] ? -1 : 1;
    return a->name.length < b->name.length ? -1 : a->name.length > b->name.length ? 1 : 0;
}

static bool nbt_tags(const char *name, const char *stage, const char *path, const mc_nbt_tag *tag,
                     size_t depth) {
    if (tag == NULL || depth > 8)
        return false;
    printf("TAG %s %s %s %d ", name, stage, path, (int)tag->type);
    switch (tag->type) {
        case MC_NBT_COMPOUND:
            printf("%zu", tag->value.compound.length);
            break;
        case MC_NBT_LIST:
            printf("%u,%zu", tag->value.list.element_type, tag->value.list.length);
            break;
        case MC_NBT_BYTE:
            printf("%d", tag->value.byte_value);
            break;
        case MC_NBT_SHORT:
            printf("%d", tag->value.short_value);
            break;
        case MC_NBT_INT:
            printf("%" PRId32, tag->value.int_value);
            break;
        case MC_NBT_LONG:
            printf("%016" PRIx64, (uint64_t)tag->value.long_value);
            break;
        case MC_NBT_FLOAT:
            printf("%08" PRIx32, tag->value.float_bits);
            break;
        case MC_NBT_DOUBLE:
            printf("%016" PRIx64, tag->value.double_bits);
            break;
        case MC_NBT_STRING:
            if (tag->value.string.length == 0)
                putchar('-');
            for (size_t i = 0; i < tag->value.string.length; ++i)
                printf("%04x", tag->value.string.units[i]);
            break;
        default:
            return false;
    }
    putchar('\n');
    if (tag->type == MC_NBT_COMPOUND) {
        const mc_nbt_tag *ordered[40];
        if (tag->value.compound.length > COUNT(ordered))
            return false;
        for (size_t i = 0; i < tag->value.compound.length; ++i) {
            const mc_nbt_tag *value = tag->value.compound.items[i];
            size_t j = i;
            while (j != 0 && nbt_name_order(ordered[j - 1], value) > 0) {
                ordered[j] = ordered[j - 1];
                --j;
            }
            ordered[j] = value;
        }
        for (size_t i = 0; i < tag->value.compound.length; ++i) {
            char next[256], key[32];
            if (ordered[i]->name.length >= sizeof key)
                return false;
            for (size_t j = 0; j < ordered[i]->name.length; ++j) {
                if (ordered[i]->name.units[j] > 127)
                    return false;
                key[j] = (char)ordered[i]->name.units[j];
            }
            key[ordered[i]->name.length] = 0;
            const int length = snprintf(next, sizeof next, "%s/%s", path, key);
            if (length < 0 || (size_t)length >= sizeof next ||
                !nbt_tags(name, stage, next, ordered[i], depth + 1))
                return false;
        }
    } else if (tag->type == MC_NBT_LIST) {
        for (size_t i = 0; i < tag->value.list.length; ++i) {
            char next[256];
            const int length = snprintf(next, sizeof next, "%s/%zu", path, i);
            if (length < 0 || (size_t)length >= sizeof next ||
                !nbt_tags(name, stage, next, tag->value.list.items[i], depth + 1))
                return false;
        }
    }
    return !ferror(stdout);
}

static void nbt_coordinate(const mc_player_coordinates *c) {
    if (c == NULL)
        putchar('-');
    else
        printf("%" PRId32 ",%" PRId32 ",%" PRId32, c->x, c->y, c->z);
}

static bool nbt_state(const char *name, const char *outcome, mc_entity *p, uintptr_t old_main,
                      uintptr_t old_armor) {
    mc_living_entity_fields *l = &p->local_player->fields.living;
    mc_player_fields *s = &p->local_player->fields.player;
    mc_inventory_player *inv = s->c;
    mc_item_stack_fields main = {0}, armor = {0};
    if ((inv->main_slots[0] != NULL &&
         mc_item_stack_fields_get(inv->main_slots[0], &main) != MC_ITEM_STACK_OK) ||
        (inv->armor_slots[0] != NULL &&
         mc_item_stack_fields_get(inv->armor_slots[0], &armor) != MC_ITEM_STACK_OK))
        return false;
    printf("STATE %s %s %" PRId32 " %" PRId32 " %" PRId32 " %" PRId32 " %" PRId32 " %d %" PRId32
           " %" PRId32 " ",
           name, outcome, l->Y, l->aa, l->ad, l->ae, s->m, s->u ? 1 : 0, s->private_a, s->g);
    nbt_coordinate(s->v);
    putchar(' ');
    nbt_coordinate(s->private_b);
    printf(" %d %d %" PRId32 " %d %d ", (uintptr_t)inv->main_slots != old_main ? 1 : 0,
           (uintptr_t)inv->armor_slots != old_armor ? 1 : 0, inv->selected, inv->changed ? 1 : 0,
           inv->main_slots[0] != NULL && inv->main_slots[0] == inv->armor_slots[0] ? 1 : 0);
    if (inv->main_slots[0] == NULL)
        putchar('-');
    else
        printf("%" PRId32, main.animation);
    putchar(' ');
    if (inv->armor_slots[0] == NULL)
        putchar('-');
    else
        printf("%" PRId32, armor.animation);
    const double doubles[] = {p->base.x,          p->base.y,          p->base.z,
                              p->base.velocity_x, p->base.velocity_y, p->base.velocity_z};
    for (size_t i = 0; i < COUNT(doubles); ++i)
        printf(" %016" PRIx64, movement_double_bits(doubles[i]));
    const float floats[] = {p->base.yaw, p->base.aT, p->base.aU, p->base.aV, p->base.bk};
    for (size_t i = 0; i < COUNT(floats); ++i)
        printf(" %08" PRIx32, movement_float_bits(floats[i]));
    printf(" %" PRId32 " %" PRId32 " %d %08" PRIx32 "\n", p->base.bv, p->base.bz,
           p->base.aX ? 1 : 0, movement_float_bits(p->base.y_size));
    return !ferror(stdout);
}

static bool nbt_prepare(client_application *app, mc_entity *p, const char *name, nbt_input *a,
                        mc_entity_nbt_error *error) {
    bool ok = false;
    mc_item_stack *common = NULL;
    memset(a, 0, sizeof(*a));
    nbt_input_node *own = nbt_node(a, "", MC_NBT_COMPOUND);
    if (strcmp(name, "write_fresh") != 0)
        CHECK(nbt_scalar(a, own, "SpawnX", MC_NBT_INT, -7) &&
              nbt_scalar(a, own, "SpawnY", MC_NBT_INT, 65) &&
              nbt_scalar(a, own, "SpawnZ", MC_NBT_INT, 9));
    mc_entity_nbt_error_init(error);
    /* Exact inherited dc.a own-data body on the genuinely constructed held
     * receiver. This is preparation, not an alternate production reader. */
    CHECK(mc_player_nbt_read_held(p, app->registry, &own->tag, NULL, error));
    p->base.be = false;
    /* The imported movement fixture intentionally changes size/offset. Restore
     * the actual constructor inputs before this observer's explicit pose. */
    CHECK(mc_entity_set_size(p, .6f, 1.8f) == MC_ENTITY_OK);
    p->base.offset = 1.62f;
    p->base.y_size = .25f;
    CHECK(mc_entity_set_position(p, 8.5, 80.25, 8.5) == MC_ENTITY_OK);
    p->base.velocity_x = .25;
    p->base.velocity_y = -.5;
    p->base.velocity_z = .75;
    p->base.yaw = 450;
    p->base.aT = -450;
    p->base.aU = 12.5f;
    p->base.aV = -3;
    p->base.bk = 2.5f;
    p->base.bv = 65535;
    p->base.bz = 65537;
    p->base.aX = true;
    mc_living_entity_fields *l = &p->local_player->fields.living;
    mc_player_fields *s = &p->local_player->fields.player;
    l->Y = 20;
    l->aa = 1;
    l->ad = 2;
    l->ae = 3;
    s->m = 0;
    s->u = false;
    s->private_a = 4;
    s->g = 900;
    const bool narrow = strcmp(name, "write_alias_narrow") == 0;
    /* Numeric construction supplies the same authored live scalar fields.
     * Native Stack has no public id mutation; neither identity is an answer. */
    CHECK(mc_item_stack_allocate(&common) == MC_ITEM_STACK_OK &&
          mc_item_stack_construct(common, narrow ? 65537 : 2, narrow ? 511 : 5,
                                  narrow ? 65538 : 9) == MC_ITEM_STACK_OK &&
          mc_item_stack_source_animation_set(common, 4) == MC_ITEM_STACK_OK &&
          mc_inventory_player_set(s->c, 0, common, NULL) == MC_INVENTORY_OK &&
          mc_inventory_player_set(s->c, 36, common, NULL) == MC_INVENTORY_OK);
    s->c->selected = 7;
    s->c->changed = true;
    if (narrow) {
        l->Y = 40000;
        l->aa = 32768;
        l->ad = -40000;
        l->ae = 65535;
        s->g = INT32_MIN;
        p->base.be = true;
    }
    mc_java_random_reseed(p->random, 38);
    mc_java_random_reseed(p->base.world->fields.r, 38);
    ok = true;
cleanup:
    mc_item_stack_release(common);
    return ok;
}

static bool nbt_random_equal(const mc_java_random *x, const mc_java_random *y) {
    return x->state == y->state && x->have_next_next_gaussian == y->have_next_next_gaussian &&
           movement_double_bits(x->next_next_gaussian) ==
               movement_double_bits(y->next_next_gaussian);
}

static void nbt_random(const mc_java_random *r) {
    printf("%012" PRIx64 ",%d,%016" PRIx64, r->state, r->have_next_next_gaussian ? 1 : 0,
           movement_double_bits(r->next_next_gaussian));
}

static const char *nbt_outcome(mc_entity_nbt_status status, const mc_entity_nbt_error *e) {
    if (status == MC_ENTITY_NBT_WRONG_TAG ||
        (status == MC_ENTITY_NBT_STACK_FAILED && e->late_hook == MC_ENTITY_NBT_LATE_PLAYER &&
         e->player_inventory.status == MC_INVENTORY_NBT_STACK_FAILED &&
         e->player_inventory.stack.status == MC_ITEM_STACK_INVALID_FIELD))
        return "CAST";
    if (status == MC_ENTITY_NBT_NULL_REFERENCE)
        return "NULL";
    if (status == MC_ENTITY_NBT_INDEX_OUT_OF_BOUNDS ||
        (status == MC_ENTITY_NBT_REGISTRY_FAILED && e->late_hook == MC_ENTITY_NBT_LATE_PLAYER &&
         e->player_inventory.registry.status == MC_REGISTRY_INDEX_OUT_OF_RANGE))
        return "INDEX";
    return NULL;
}

#include "nbt_metadata_cases.h"

static bool nbt_native_limits(client_application *app, mc_entity *p, nbt_input *a,
                              mc_entity_nbt_error *error) {
    bool ok = false;
    mc_nbt_tree *tree = NULL, *output = NULL;
    CHECK(nbt_prepare(app, p, "limits", a, error));
    const mc_nbt_tag *root = nbt_make_input(a, "outside_slot");
    const mc_nbt_tag *list = nbt_named(root, "Inventory");
    CHECK(list != NULL);
    mc_inventory_player *inv = p->local_player->fields.player.c;
    const uintptr_t first_main = (uintptr_t)inv->main_slots,
                    first_armor = (uintptr_t)inv->armor_slots;
    const size_t old_limit = inv->local_budget.limit;
    inv->local_budget.limit = inv->local_budget.used;
    mc_entity_nbt_select_player_detail(error);
    const mc_inventory_nbt_status no_main =
        mc_inventory_player_read_nbt(inv, app->registry, list, NULL, &error->player_inventory);
    inv->local_budget.limit = old_limit;
    CHECK(no_main == MC_INVENTORY_NBT_LIMIT && error->player_inventory.bytecode == 3 &&
          (uintptr_t)inv->main_slots == first_main && (uintptr_t)inv->armor_slots == first_armor &&
          inv->main_slots[0] != NULL && inv->main_slots[0] == inv->armor_slots[0]);
    mc_nbt_limits limits = mc_nbt_default_limits();
    limits.max_elements = 0;
    CHECK(mc_inventory_player_read_nbt(inv, app->registry, list, &limits,
                                       &error->player_inventory) == MC_INVENTORY_NBT_LIMIT &&
          error->player_inventory.bytecode == 29 && error->player_inventory.index == 0 &&
          error->player_inventory.slot == -1 && (uintptr_t)inv->main_slots != first_main &&
          (uintptr_t)inv->armor_slots != first_armor && inv->main_slots[0] == NULL &&
          inv->armor_slots[0] == NULL && inv->selected == 7 && inv->changed);
    CHECK(mc_entity_build_nbt(p, NULL, &tree, error) == MC_ENTITY_NBT_OK);
    mc_entity_nbt_select_player_detail(error);
    output = tree;
    limits = mc_nbt_default_limits();
    limits.max_allocation_bytes = 0;
    CHECK(mc_inventory_player_build_nbt(inv, &limits, &output, &error->player_inventory) ==
              MC_INVENTORY_NBT_LIMIT &&
          output == tree);
    CHECK(mc_inventory_player_read_nbt(inv, app->registry, NULL, NULL, &error->player_inventory) ==
              MC_INVENTORY_NBT_NULL_REFERENCE &&
          error->player_inventory.bytecode == 21 && inv->main_slots[0] == NULL &&
          inv->armor_slots[0] == NULL);
    ok = true;
cleanup:
    mc_nbt_tree_free(tree);
    return ok;
}

static bool nbt_cases(fixture *f, client_application *app, mc_entity *p, mc_movement_input *input) {
    bool ok = false;
    (void)f;
    (void)input;
    (void)tick_cases;
    nbt_input *a = calloc(1, sizeof(*a));
    mc_entity_nbt_error *error = calloc(1, sizeof(*error));
    mc_nbt_tree *tree = NULL, *read_tree = NULL;
    const char *names[] = {
        "write_fresh",     "write_alias_narrow", "roundtrip",      "base_only",
        "health_wrong",    "hurt_wrong",         "death_wrong",    "attack_wrong",
        "inventory_wrong", "entry_wrong",        "entry_null",     "slot_wrong",
        "id_wrong",        "count_wrong",        "damage_wrong",   "id_negative",
        "id_null",         "outside_slot",       "duplicate_slot", "armor_slots",
        "slot_255",        "slot_missing",       "partial_list",   "dimension_wrong",
        "sleeping_wrong",  "timer_wrong",        "spawn_partial",  "spawn_x_wrong",
        "spawn_y_wrong",   "spawn_z_wrong",      "spawn_good",     "score_wrong",
        "score_absent"};
    size_t complete = 0, interrupted = 0;
    CHECK(a != NULL && error != NULL);
    for (size_t i = 0; i < COUNT(names); ++i) {
        const char *name = names[i], *outcome = "OK";
        CHECK(nbt_prepare(app, p, name, a, error));
        mc_inventory_player *inv = p->local_player->fields.player.c;
        const uintptr_t main_before = (uintptr_t)inv->main_slots,
                        armor_before = (uintptr_t)inv->armor_slots;
        const mc_java_random er = *p->random, wr = *p->base.world->fields.r;
        if (strncmp(name, "write_", 6) == 0) {
            CHECK(mc_entity_build_nbt(p, NULL, &tree, error) == MC_ENTITY_NBT_OK &&
                  nbt_tags(name, "direct", "$", mc_nbt_tree_root(tree), 0));
            mc_nbt_tree *sentinel = tree;
            bool emitted = true;
            CHECK(mc_entity_save_nbt(p, NULL, &emitted, &sentinel, error) == MC_ENTITY_NBT_OK &&
                  !emitted && sentinel == tree);
            printf("NAMED %s 0 1\n", name);
            mc_nbt_tree_free(tree);
            tree = NULL;
        } else {
            const mc_nbt_tag *tag;
            if (strcmp(name, "roundtrip") == 0) {
                CHECK(mc_entity_build_nbt(p, NULL, &read_tree, error) == MC_ENTITY_NBT_OK);
                tag = mc_nbt_tree_root(read_tree);
                CHECK(nbt_tags(name, "input", "$", tag, 0));
            } else {
                tag = nbt_make_input(a, name);
                CHECK(tag != NULL);
            }
            const mc_entity_nbt_status status =
                mc_entity_read_nbt_in_context(p, app->registry, tag, NULL, error);
            if (status == MC_ENTITY_NBT_OK)
                ++complete;
            else {
                outcome = nbt_outcome(status, error);
                CHECK(outcome != NULL && error->late_hook == MC_ENTITY_NBT_LATE_PLAYER);

                /* ls.a(Lnu;)V fails in the Short getter, before its field store. */
                static const struct {
                    const char *name, *field;
                    unsigned int getter_pc;
                } living_sites[] = {{"health_wrong", "Health", 4},
                                    {"hurt_wrong", "HurtTime", 29},
                                    {"death_wrong", "DeathTime", 39},
                                    {"attack_wrong", "AttackTime", 49}};

                for (size_t site = 0; site < COUNT(living_sites); ++site)
                    if (strcmp(name, living_sites[site].name) == 0)
                        CHECK(status == MC_ENTITY_NBT_WRONG_TAG && error->field != NULL &&
                              strcmp(error->field, living_sites[site].field) == 0 &&
                              error->base_bytecode == 311 && error->player_bytecode == 2 &&
                              error->local_player_bytecode == 2 &&
                              error->living_bytecode == living_sites[site].getter_pc);
                ++interrupted;
                printf("PREFIX %s %u %u %u %u %u %u %zu\n", name, error->base_bytecode,
                       error->living_bytecode, error->player_bytecode, error->local_player_bytecode,
                       error->player_inventory.bytecode, error->player_inventory.stack_bytecode,
                       error->index);
            }
            mc_nbt_tree_free(read_tree);
            read_tree = NULL;
        }
        CHECK(nbt_state(name, outcome, p, main_before, armor_before) &&
              mc_entity_build_nbt(p, NULL, &tree, error) == MC_ENTITY_NBT_OK &&
              nbt_tags(name, "after", "$", mc_nbt_tree_root(tree), 0));
        mc_nbt_tree_free(tree);
        tree = NULL;
        CHECK(nbt_random_equal(&er, p->random) && nbt_random_equal(&wr, p->base.world->fields.r));
        printf("RANDOM %s ", name);
        nbt_random(p->random);
        putchar(' ');
        nbt_random(p->base.world->fields.r);
        printf("\nEND %s %s\n", name, outcome);
    }
    printf("Native Player NBT observations: %zu cases, %zu complete reads, %zu interrupted reads\n",
           COUNT(names), complete, interrupted);
    CHECK(nbt_composition(f, app, a, error));
    CHECK(nbt_native_limits(app, p, a, error));
    ok = !ferror(stdout);
cleanup:
    mc_nbt_tree_free(tree);
    mc_nbt_tree_free(read_tree);
    free(error);
    free(a);
    return ok;
}

int main(int argc, char **argv) {
    if (argc == 4 && strcmp(argv[1], "player-nbt") == 0)
        argv[1] = "player-active";
    const int result = mc_nbt_fixture_main(argc, argv);
    if (result == 0 && printf("Player NBT: %u checks\n", checks) < 0)
        return 1;
    return result;
}
