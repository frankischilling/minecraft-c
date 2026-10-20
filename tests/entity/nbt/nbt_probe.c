/* Genuine native owners, with the old constructor program renamed/unexecuted. */
#define main mc_entity_nbt_probe_constructor_fixture_main
#include "../test_entity.c"
#undef main

#include "mc/entity/nbt/factory.h"
#include <inttypes.h>
#include <stdlib.h>
#if defined(_WIN32)
#include <fcntl.h>
#include <io.h>
#endif

static const char *const np_cases[] = {
    "save_narrow",      "save_dead",         "factory",        "factory_missing_item",
    "read_good",        "read_missing_item", "motion_nan_ten", "motion_infinities",
    "motion_neighbors", "motion_short",      "motion_wrong",   "pos_wrong",
    "rotation_short",   "fire_wrong",        "count_wrong",    "getter_wrong",
    "null_compound",    "empty_vectors",     "angle_infinity", "missing_scalars"};

typedef struct np_record {
    mc_nbt_tag root, fields[11], vectors[8], item[3];
    const mc_nbt_tag *children[11], *pos[3], *motion[3], *rotation[2], *items[3];
    uint16_t names[14][16], item_name[4];
} np_record;

static bool np_is(const char *scenario, const char *name)
{
    return strcmp(scenario, name) == 0;
}

static mc_nbt_string np_ascii(const char *text, uint16_t *units)
{
    const size_t length = strlen(text);
    for (size_t i = 0; i < length; ++i)
        units[i] = (uint16_t)(unsigned char)text[i];
    return (mc_nbt_string){units, length};
}

static void np_record_init(np_record *r, const char *scenario)
{
    const char *const names[] = {"id",   "Pos", "Motion",   "Rotation", "FallDistance",
                                 "Fire", "Air", "OnGround", "Health",   "Age",
                                 "Item", "id",  "Count",    "Damage"};
    const mc_nbt_type types[] = {MC_NBT_STRING, MC_NBT_LIST,  MC_NBT_LIST,    MC_NBT_LIST,
                                 MC_NBT_FLOAT,  MC_NBT_SHORT, MC_NBT_SHORT,   MC_NBT_BYTE,
                                 MC_NBT_SHORT,  MC_NBT_SHORT, MC_NBT_COMPOUND};
    const double doubles[] = {8, 16, 32, 20, 30, 40};
    memset(r, 0, sizeof(*r));
    r->root.type = MC_NBT_COMPOUND;
    r->root.value.compound.items = r->children;
    r->root.value.compound.length = 11;
    for (size_t i = 0; i < 11; ++i) {
        r->fields[i].name = np_ascii(names[i], r->names[i]);
        r->fields[i].type = types[i];
        r->children[i] = &r->fields[i];
    }
    r->fields[0].value.string = np_ascii("Item", r->item_name);
    for (size_t i = 0; i < 6; ++i) {
        r->vectors[i].type = MC_NBT_DOUBLE;
        r->vectors[i].value.double_bits = double_bits(doubles[i]);
    }
    r->vectors[6].type = r->vectors[7].type = MC_NBT_FLOAT;
    r->vectors[6].value.float_bits = float_bits(725.0f);
    r->vectors[7].value.float_bits = float_bits(-725.0f);
    for (size_t i = 0; i < 3; ++i) {
        r->pos[i] = &r->vectors[i];
        r->motion[i] = &r->vectors[i + 3];
        r->items[i] = &r->item[i];
        r->item[i].name = np_ascii(names[i + 11], r->names[i + 11]);
        r->item[i].type = i == 1 ? MC_NBT_BYTE : MC_NBT_SHORT;
    }
    r->rotation[0] = &r->vectors[6];
    r->rotation[1] = &r->vectors[7];
    r->fields[1].value.list.element_type = r->fields[2].value.list.element_type = MC_NBT_DOUBLE;
    r->fields[3].value.list.element_type = MC_NBT_FLOAT;
    r->fields[1].value.list.items = r->pos;
    r->fields[2].value.list.items = r->motion;
    r->fields[3].value.list.items = r->rotation;
    r->fields[1].value.list.length = r->fields[2].value.list.length = 3;
    r->fields[3].value.list.length = 2;
    r->fields[4].value.float_bits = float_bits(3.5f);
    r->fields[5].value.short_value = -19;
    r->fields[6].value.short_value = 298;
    r->fields[7].value.byte_value = -2;
    r->fields[8].value.short_value = np_is(scenario, "count_wrong") ? 128 : 511;
    r->fields[9].value.short_value = np_is(scenario, "count_wrong") ? -123 : -17;
    r->fields[10].value.compound.items = r->items;
    r->fields[10].value.compound.length = 3;
    r->item[0].value.short_value = -7;
    r->item[1].value.byte_value = -3;
    r->item[2].value.short_value = -11;
    if (np_is(scenario, "motion_nan_ten")) {
        r->vectors[3].value.double_bits = UINT64_C(0x7ff8000000000042);
        r->vectors[4].value.double_bits = double_bits(10.0);
        r->vectors[5].value.double_bits = double_bits(-10.0);
    }
    if (np_is(scenario, "motion_infinities")) {
        r->vectors[3].value.double_bits = UINT64_C(0x7ff0000000000000);
        r->vectors[4].value.double_bits = UINT64_C(0xfff0000000000000);
        r->vectors[5].value.double_bits = UINT64_C(0x8000000000000000);
    }
    if (np_is(scenario, "motion_neighbors")) {
        r->vectors[3].value.double_bits = UINT64_C(0x4024000000000001);
        r->vectors[4].value.double_bits = UINT64_C(0xc024000000000001);
        r->vectors[5].value.double_bits = UINT64_C(0x4023ffffffffffff);
    }
    if (np_is(scenario, "motion_short"))
        r->fields[2].value.list.length = 2;
    if (np_is(scenario, "rotation_short"))
        r->fields[3].value.list.length = 1;
    if (np_is(scenario, "motion_wrong")) {
        r->vectors[4].type = MC_NBT_INT;
        r->vectors[4].value.int_value = 9;
    }
    if (np_is(scenario, "pos_wrong")) {
        r->vectors[1].type = MC_NBT_INT;
        r->vectors[1].value.int_value = 9;
    }
    if (np_is(scenario, "fire_wrong")) {
        r->fields[5].type = MC_NBT_INT;
        r->fields[5].value.int_value = 9;
    }
    if (np_is(scenario, "count_wrong")) {
        r->item[1].type = MC_NBT_INT;
        r->item[1].value.int_value = 9;
    }
    if (np_is(scenario, "angle_infinity")) {
        r->vectors[6].value.float_bits = UINT32_C(0x7f800000);
        r->vectors[7].value.float_bits = UINT32_C(0x80000000);
    }
    if (np_is(scenario, "factory_missing_item") || np_is(scenario, "read_missing_item"))
        r->root.value.compound.length = 10;
    if (np_is(scenario, "missing_scalars"))
        r->root.value.compound.length = 4;
    if (np_is(scenario, "empty_vectors"))
        r->root.value.compound.length = 0;
    if (np_is(scenario, "getter_wrong")) {
        r->children[1] = &r->fields[2];
        for (size_t i = 2; i < 10; ++i)
            r->children[i] = &r->fields[i + 1];
        r->root.value.compound.length = 10;
        r->fields[2].type = MC_NBT_INT;
        r->fields[2].value.int_value = 9;
    }
}

static void np_prepare(mc_entity *entity, bool saving)
{
    /* Explicit actual instance scalar fixture inputs only. */
    mc_entity_base_fields *b = &entity->base;
    b->x = -101;
    b->y = saving ? 16777217 : -102;
    b->z = -103;
    b->aJ = -201;
    b->aK = -202;
    b->aL = -203;
    b->bl = -301;
    b->bm = -302;
    b->bn = -303;
    b->velocity_x = -1;
    b->velocity_y = -2;
    b->velocity_z = -3;
    b->yaw = saving ? 725.0f : 11.0f;
    b->aT = saving ? -725.0f : 12.0f;
    b->aU = 13;
    b->aV = 14;
    b->bk = 2.25f;
    b->y_size = 0.5f;
    b->bv = saving ? 65535 : 7;
    b->bz = saving ? 65537 : 8;
    b->aX = false;
    entity->item.f = saving ? 384 : 37;
    entity->item.b = saving ? 98305 : 41;
}

static void np_double(const char *scenario, const char *stage, const char *name, double value)
{
    printf("FIELD\t%s\t%s\t%s\t%016" PRIx64 "\n", scenario, stage, name, double_bits(value));
}
static void np_float(const char *scenario, const char *stage, const char *name, float value)
{
    printf("FIELD\t%s\t%s\t%s\t%08" PRIx32 "\n", scenario, stage, name, float_bits(value));
}
static void np_int(const char *scenario, const char *stage, const char *name, int32_t value)
{
    printf("FIELD\t%s\t%s\t%s\t%08" PRIx32 "\n", scenario, stage, name, (uint32_t)value);
}

static bool np_snapshot(const char *scenario, const char *stage, mc_entity *entity)
{
    mc_entity_snapshot e;
    if (mc_entity_get_snapshot(entity, &e) != MC_ENTITY_OK || e.bounds == NULL ||
        e.base.world != NULL)
        return false;
#define NP_D(key, member) np_double(scenario, stage, key, e.base.member)
#define NP_F(key, member) np_float(scenario, stage, key, e.base.member)
    NP_D("x", x);
    NP_D("y", y);
    NP_D("z", z);
    NP_D("previous_x", aJ);
    NP_D("previous_y", aK);
    NP_D("previous_z", aL);
    NP_D("interpolation_x", bl);
    NP_D("interpolation_y", bm);
    NP_D("interpolation_z", bn);
    NP_D("velocity_x", velocity_x);
    NP_D("velocity_y", velocity_y);
    NP_D("velocity_z", velocity_z);
    NP_F("yaw", yaw);
    NP_F("pitch", aT);
    NP_F("previous_yaw", aU);
    NP_F("previous_pitch", aV);
    NP_F("fall", bk);
    NP_F("y_size", y_size);
    NP_F("offset", offset);
    NP_F("width", width);
    NP_F("height", height);
#undef NP_D
#undef NP_F
    np_int(scenario, stage, "fire", e.base.bv);
    np_int(scenario, stage, "air", e.base.bz);
    printf("FIELD\t%s\t%s\tground\t%d\n", scenario, stage, e.base.aX ? 1 : 0);
    np_int(scenario, stage, "health", e.item.f);
    np_int(scenario, stage, "age", e.item.b);
    np_double(scenario, stage, "bounds_min_x", e.bounds->min_x);
    np_double(scenario, stage, "bounds_min_y", e.bounds->min_y);
    np_double(scenario, stage, "bounds_min_z", e.bounds->min_z);
    np_double(scenario, stage, "bounds_max_x", e.bounds->max_x);
    np_double(scenario, stage, "bounds_max_y", e.bounds->max_y);
    np_double(scenario, stage, "bounds_max_z", e.bounds->max_z);
    printf("FIELD\t%s\t%s\tstack_present\t%d\n", scenario, stage, e.stack == NULL ? 0 : 1);
    if (e.stack != NULL) {
        mc_item_stack_fields s;
        if (mc_item_stack_fields_get(e.stack, &s) != MC_ITEM_STACK_OK)
            return false;
        np_int(scenario, stage, "stack_id", s.id);
        np_int(scenario, stage, "stack_count", s.count);
        np_int(scenario, stage, "stack_damage", s.damage);
        np_int(scenario, stage, "stack_animation", s.animation);
    }
    np_int(scenario, stage, "id", e.base.id);
    printf("FIELD\t%s\t%s\tdead\t%d\n", scenario, stage, e.base.be ? 1 : 0);
    printf("FIELD\t%s\t%s\tentity_membership\t%d\n", scenario, stage, e.base.bF ? 1 : 0);
    np_int(scenario, stage, "chunk_x", e.base.bG);
    np_int(scenario, stage, "chunk_y", e.base.bH);
    np_int(scenario, stage, "chunk_z", e.base.bI);
    np_int(scenario, stage, "pickup_delay", e.item.c);
    np_int(scenario, stage, "update_counter", e.item.e);
    printf("FIELD\t%s\t%s\tworld_null\t1\n", scenario, stage);
    mc_entity_watcher_snapshot watched;
    if (mc_entity_watcher_get_snapshot(e.watcher, &watched) != MC_ENTITY_OK ||
        watched.size > INT32_MAX)
        return false;
    np_int(scenario, stage, "watcher_size", (int32_t)watched.size);
    np_int(scenario, stage, "watcher_index", watched.index);
    np_int(scenario, stage, "watcher_type", watched.type);
    np_int(scenario, stage, "watcher_value", watched.byte_value);
    printf("FIELD\t%s\t%s\twatcher_record_dirty\t%d\n", scenario, stage,
           watched.record_dirty ? 1 : 0);
    printf("FIELD\t%s\t%s\twatcher_dirty\t%d\n", scenario, stage, watched.dirty ? 1 : 0);
    return true;
}

static int np_name_compare(const void *left, const void *right)
{
    const mc_nbt_tag *a = *(const mc_nbt_tag *const *)left, *b = *(const mc_nbt_tag *const *)right;
    const size_t common = a->name.length < b->name.length ? a->name.length : b->name.length;
    for (size_t i = 0; i < common; ++i)
        if (a->name.units[i] != b->name.units[i])
            return a->name.units[i] < b->name.units[i] ? -1 : 1;
    return a->name.length < b->name.length ? -1 : a->name.length != b->name.length;
}

static bool np_tags(const char *scenario, const char *stage, const char *path,
                    const mc_nbt_tag *tag)
{
    if (tag == NULL) {
        printf("TAG\t%s\t%s\t%s\tNULL\t-\n", scenario, stage, path);
        return true;
    }
    printf("TAG\t%s\t%s\t%s\t%d\t", scenario, stage, path, (int)tag->type);
    switch (tag->type) {
        case MC_NBT_COMPOUND:
            printf("%zu", tag->value.compound.length);
            break;
        case MC_NBT_LIST:
            printf("%u,%zu", (unsigned int)tag->value.list.element_type, tag->value.list.length);
            break;
        case MC_NBT_DOUBLE:
            printf("%016" PRIx64, tag->value.double_bits);
            break;
        case MC_NBT_FLOAT:
            printf("%08" PRIx32, tag->value.float_bits);
            break;
        case MC_NBT_INT:
            printf("%08" PRIx32, (uint32_t)tag->value.int_value);
            break;
        case MC_NBT_SHORT:
            printf("%08" PRIx32, (uint32_t)(int32_t)tag->value.short_value);
            break;
        case MC_NBT_BYTE:
            printf("%08" PRIx32, (uint32_t)(int32_t)tag->value.byte_value);
            break;
        case MC_NBT_STRING:
            if (tag->value.string.length == 0)
                putchar('-');
            for (size_t i = 0; i < tag->value.string.length; ++i)
                printf("%04x", (unsigned int)tag->value.string.units[i]);
            break;
        default:
            return false;
    }
    putchar('\n');
    if (tag->type == MC_NBT_COMPOUND) {
        const mc_nbt_tag *ordered[16];
        if (tag->value.compound.length > 16)
            return false;
        for (size_t i = 0; i < tag->value.compound.length; ++i)
            ordered[i] = tag->value.compound.items[i];
        /* Only this local array of pointers is writable; borrowed tags remain
         * const. Make that storage conversion explicit for MSVC's qualifier check. */
        qsort((void *)ordered, tag->value.compound.length, sizeof(ordered[0]), np_name_compare);
        for (size_t i = 0; i < tag->value.compound.length; ++i) {
            char next[256];
            const size_t length = strlen(path), units = ordered[i]->name.length;
            if (length + units + 2 > sizeof(next))
                return false;
            memcpy(next, path, length);
            next[length] = '/';
            for (size_t j = 0; j < units; ++j) {
                if (ordered[i]->name.units[j] > 127)
                    return false;
                next[length + 1 + j] = (char)ordered[i]->name.units[j];
            }
            next[length + 1 + units] = '\0';
            if (!np_tags(scenario, stage, next, ordered[i]))
                return false;
        }
    } else if (tag->type == MC_NBT_LIST) {
        for (size_t i = 0; i < tag->value.list.length; ++i) {
            char next[256];
            const int written = snprintf(next, sizeof(next), "%s/%zu", path, i);
            if (written < 0 || (size_t)written >= sizeof(next) ||
                !np_tags(scenario, stage, next, tag->value.list.items[i]))
                return false;
        }
    }
    return !ferror(stdout);
}

static bool np_bytes(const char *scenario, const mc_nbt_tag *root)
{
    size_t length = 0, written = 0;
    if (mc_nbt_measure(root, NULL, &length, NULL) != MC_NBT_OK || length > 65536)
        return false;
    uint8_t *bytes = malloc(length);
    if (bytes == NULL)
        return false;
    const bool ok =
        mc_nbt_write(root, NULL, bytes, length, &written, NULL) == MC_NBT_OK && written == length;
    if (ok) {
        printf("BYTES\t%s\t", scenario);
        for (size_t i = 0; i < length; ++i)
            printf("%02x", (unsigned int)bytes[i]);
        putchar('\n');
    }
    free(bytes);
    return ok;
}

static const char *np_outcome(mc_entity_nbt_status status, const mc_entity_nbt_error *error)
{
    if (status == MC_ENTITY_NBT_OK)
        return "OK";
    if (status == MC_ENTITY_NBT_WRONG_TAG || (status == MC_ENTITY_NBT_STACK_FAILED &&
                                              error->stack.status == MC_ITEM_STACK_INVALID_FIELD))
        return "CAST";
    if (status == MC_ENTITY_NBT_INDEX_OUT_OF_BOUNDS)
        return "INDEX";
    return status == MC_ENTITY_NBT_NULL_REFERENCE ? "NULL" : NULL;
}

static bool np_case(mc_entity_classes *classes, const char *scenario)
{
    bool ok = false;
    const bool saving = strncmp(scenario, "save_", 5) == 0,
               factory = strncmp(scenario, "factory", 7) == 0;
    mc_entity *entity = NULL;
    mc_item_stack *supplied = NULL, *old = NULL;
    mc_nbt_tree *tree = NULL;
    mc_entity_nbt_error error;
    np_record input, initial;
    np_record_init(&input, scenario);
    np_record_init(&initial, "read_good");
    const mc_nbt_tag *root = np_is(scenario, "null_compound") ? NULL : &input.root;
    printf("CASE\t%s\t%s\n", scenario, saving ? "SAVE" : factory ? "FACTORY" : "READ");
    if (!saving && !np_tags(scenario, "input", "$", root))
        goto cleanup;
    const mc_entity_bounds *bounds = NULL;
    if (!factory) {
        if (saving && (mc_item_stack_allocate(&supplied) != MC_ITEM_STACK_OK ||
                       mc_item_stack_construct(supplied, 98305, 511, 65538) != MC_ITEM_STACK_OK))
            goto cleanup;
        if (mc_item_entity_allocate(classes, &entity, NULL) != MC_ENTITY_OK)
            goto cleanup;
        if (saving) {
            if (mc_item_entity_construct(entity, NULL, 8, 16, 32, supplied, NULL) != MC_ENTITY_OK)
                goto cleanup;
        } else if (mc_item_entity_construct_world(entity, NULL, NULL) != MC_ENTITY_OK ||
                   mc_entity_read_nbt(entity, &initial.root, NULL) != MC_ENTITY_NBT_OK)
            goto cleanup;
        np_prepare(entity, saving);
        if (np_is(scenario, "save_dead"))
            entity->base.be = true; /* Labeled source dead flag only. */
        if (!np_snapshot(scenario, "before", entity) ||
            mc_item_entity_stack_get(entity, &old) != MC_ENTITY_OK)
            goto cleanup;
        bounds = entity->bounds;
    }
    const char *outcome;
    mc_entity_nbt_status status;
    if (saving) {
        mc_nbt_tag marker = {.type = MC_NBT_INT};
        const mc_nbt_tag *child = &marker;
        uint16_t marker_units[6];
        marker.name = np_ascii("Marker", marker_units);
        marker.value.int_value = 123;
        const mc_nbt_tag marker_root = {.type = MC_NBT_COMPOUND, .value.compound = {&child, 1}};
        if (np_is(scenario, "save_dead") &&
            mc_nbt_tree_copy_compound(&marker_root, NULL, &tree, NULL) != MC_NBT_OK)
            goto cleanup;
        bool serialized = false;
        status = mc_entity_save_nbt(entity, NULL, &serialized, &tree, &error);
        if (status != MC_ENTITY_NBT_OK)
            goto cleanup;
        outcome = serialized ? "TRUE" : "FALSE";
    } else if (factory) {
        status = mc_entity_factory_nbt(classes, NULL, root, &entity, &error);
        if (status != MC_ENTITY_NBT_OK || entity == NULL)
            goto cleanup;
        outcome = "OK";
    } else {
        status = mc_entity_read_nbt(entity, root, &error);
        outcome = np_outcome(status, &error);
        if (outcome == NULL)
            goto cleanup;
    }
    if (status != MC_ENTITY_NBT_OK)
        printf("DETAIL\t%s\t%d\t%u\t%u\t%u\t%zu\t%s\n", scenario, (int)status, error.base_bytecode,
               error.item_bytecode, error.stack_bytecode, error.index,
               error.field == NULL ? "-" : error.field);
    printf("RESULT\t%s\t%s\n", scenario, outcome);
    if (!np_snapshot(scenario, "after", entity))
        goto cleanup;
    if (factory)
        printf("REF\t%s\tNA\tNA\n", scenario);
    else
        printf("REF\t%s\t%d\t%d\n", scenario, bounds == entity->bounds ? 1 : 0,
               old == entity->stack ? 1 : 0);
    if (old != NULL) {
        mc_item_stack_fields fields;
        if (mc_item_stack_fields_get(old, &fields) != MC_ITEM_STACK_OK)
            goto cleanup;
        printf("HELD\t%s\t%08" PRIx32 "\t%08" PRIx32 "\t%08" PRIx32 "\t%08" PRIx32 "\n", scenario,
               (uint32_t)fields.id, (uint32_t)fields.count, (uint32_t)fields.damage,
               (uint32_t)fields.animation);
    }
    if (saving && (!np_tags(scenario, "output", "$", mc_nbt_tree_root(tree)) ||
                   !np_bytes(scenario, mc_nbt_tree_root(tree))))
        goto cleanup;
    ok = !ferror(stdout);
cleanup:
    mc_nbt_tree_free(tree);
    mc_item_stack_release(old);
    mc_item_stack_release(supplied);
    mc_entity_release(entity);
    return ok;
}

int main(int argc, char **argv)
{
    int result = 1;
    mc_java_random_profile *profile = NULL;
    mc_entity_classes *classes = NULL;
    if (argc != 2 || (strcmp(argv[1], "client") != 0 && strcmp(argv[1], "server") != 0) ||
        (_mm_getcsr() & 0xffc0u) != 0x1f80u)
        return 1;
#if defined(_WIN32)
    if (_setmode(_fileno(stdout), _O_BINARY) == -1)
        return 1;
#endif
    const mc_entity_source source =
        strcmp(argv[1], "client") == 0 ? MC_ENTITY_CLIENT : MC_ENTITY_SERVER;
    if (mc_java_random_profile_create(profile_budget(), &profile) != MC_JAVA_RANDOM_PROFILE_OK ||
        mc_entity_classes_create(source, profile, NULL, &classes) != MC_ENTITY_OK)
        goto cleanup;
    printf("ENTITY_NBT_V1\t%s\n", argv[1]);
    puts("SCOPE\tPREPARED_SCALARS\tEXCLUDE_ONLY_RANDOM_INTERNALS_AND_HOVER_FROM_SELECTED_OWNER_"
         "FIELDS");
    for (size_t i = 0; i < sizeof(np_cases) / sizeof(np_cases[0]); ++i) {
        if (!np_case(classes, np_cases[i])) {
            fprintf(stderr, "actual native NBT probe failed: %s\n", np_cases[i]);
            goto cleanup;
        }
    }
    printf("END\t%zu\n", sizeof(np_cases) / sizeof(np_cases[0]));
    if (fflush(stdout) == 0 && !ferror(stdout))
        result = 0;
cleanup:
    if (retire_classes(&classes) != 0)
        result = 1;
    if (classes == NULL)
        mc_java_random_profile_free(profile);
    return result;
}
