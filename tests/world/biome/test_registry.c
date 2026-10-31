#include "mc/item/registry/registry.h"
#include "mc/resources/translation/translation.h"
#include "mc/world/biome/registry.h"

#include "../../../src/block/registry/registry_internal.h"
#include "../../../src/core/java/random_profile_internal.h"
#include "../../../src/world/biome/registry_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define COUNT(a) (sizeof(a) / sizeof((a)[0]))
static unsigned int checks;
static bool retirement_failed;
#define CHECK(c)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(c)) {                                                                                \
            fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #c);                        \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)
#define VERIFY(c)                                                                                  \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(c)) {                                                                                \
            fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #c);                        \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

/* Synthetic inputs exercise real dependencies. No original resource text or
 * expected registry objects are provided by these byte streams. */
static const uint8_t language[] = "stat.mineBlock=Mine %1$s\nstat.useItem=Use %1$s\n"
                                  "stat.breakItem=Break %1$s\nstat.craftItem=Craft %1$s\n";

typedef struct stream {
    const uint8_t *bytes;
    size_t length, offset, opens, reads, closes;
    bool closed;
} stream;

typedef struct fixture {
    mc_registry_classes *owner;
    mc_registry_resources resources;
    stream streams[3];
    bool missing, invalid, method_entered;
    size_t observed_callbacks;
} fixture;

static void named_cells(const mc_biome_registry_view *v, const mc_biome **out) {
    const mc_biome *const values[] = {v->a, v->b, v->c, v->d, v->e, v->f, v->g,
                                      v->h, v->i, v->j, v->k, v->l, v->m};
    memcpy(out, values, sizeof(values));
}

static bool same_view(const mc_biome_registry_view *a, const mc_biome_registry_view *b) {
    const mc_biome *left[13], *right[13];
    named_cells(a, left);
    named_cells(b, right);
    if (a->phase != b->phase || a->initializer_pc != b->initializer_pc || a->x != b->x ||
        a->x_length != b->x_length)
        return false;
    for (size_t i = 0; i < COUNT(left); ++i)
        if (left[i] != right[i])
            return false;
    return true;
}

static bool no_named(const mc_biome_registry_view *v) {
    const mc_biome *values[13];
    named_cells(v, values);
    for (size_t i = 0; i < COUNT(values); ++i)
        if (values[i] != NULL)
            return false;
    return true;
}

static uint64_t uniquifier(const mc_java_random_profile *profile) {
    return atomic_load_explicit(&profile->uniquifier, memory_order_relaxed);
}

static uint64_t advance_five(uint64_t value) {
    for (unsigned int i = 0; i < 5; ++i)
        value *= UINT64_C(1181783497276652981);
    return value;
}

static bool same_cause(const mc_registry_error *a, const mc_registry_error *b) {
    return a->message == b->message && a->initializer_pc == b->initializer_pc &&
           a->block_id == b->block_id && a->constructor_class == b->constructor_class &&
           a->constructor_pc == b->constructor_pc &&
           a->active_initializer_class == b->active_initializer_class &&
           a->active_initializer_pc == b->active_initializer_pc &&
           a->item_argument == b->item_argument && a->profile_status == b->profile_status &&
           a->format_error.status == b->format_error.status &&
           a->format_error.offset == b->format_error.offset &&
           a->format_error.length == b->format_error.length &&
           a->format_error.value == b->format_error.value &&
           a->format_error.flags == b->format_error.flags &&
           a->format_error.conversion == b->format_error.conversion &&
           a->format_error.date_time == b->format_error.date_time;
}

static mc_java_properties_status read_bytes(void *context, uint8_t *out, size_t capacity,
                                            size_t *count) {
    stream *s = context;
    ++s->reads;
    if (out == NULL || count == NULL || capacity == 0 || s->closed || s->offset > s->length)
        return MC_JAVA_PROPERTIES_IO_ERROR;
    size_t take = s->length - s->offset;
    if (take > 17)
        take = 17;
    if (take > capacity)
        take = capacity;
    if (take != 0)
        memcpy(out, s->bytes + s->offset, take);
    s->offset += take;
    *count = take;
    return MC_JAVA_PROPERTIES_OK;
}

static mc_registry_status open_resource(void *context, const char *name,
                                        mc_java_properties_input *out) {
    fixture *f = context;
    const char *const names[] = {"/lang/en_US.lang", "/lang/stats_US.lang", "/achievement/map.txt"};
    size_t index = COUNT(names);
    for (size_t i = 0; i < COUNT(names); ++i)
        if (name != NULL && strcmp(name, names[i]) == 0)
            index = i;
    if (index == COUNT(names) || out == NULL) {
        f->invalid = true;
        return MC_REGISTRY_INVALID_ARGUMENT;
    }
    if (++f->streams[index].opens != 1)
        f->invalid = true;
    if (index != f->observed_callbacks)
        f->invalid = true;

    /* The first Rainforest base constructor is suspended at real Block use5.
     * Diagnostic inspection is allowed; ordinary public entry is BUSY. */
    mc_biome_registry_view before, after;
    mc_registry_error error;
    const mc_biome *sentinel = (const mc_biome *)f;
    const mc_biome *value = sentinel;
    size_t bytes = f->owner->allocation_bytes;
    if (mc_biome_registry_inspect(f->owner, &before, &error) != MC_REGISTRY_OK ||
        before.phase != MC_REGISTRY_INITIALIZING || before.initializer_pc != 4 ||
        !no_named(&before) || before.x != NULL || before.x_length != 0 ||
        mc_registry_classes_initialize_biomes(f->owner, &error) != MC_REGISTRY_BUSY ||
        mc_biome_static_get(f->owner, 'a', &value, &error) != MC_REGISTRY_BUSY ||
        value != sentinel ||
        mc_biome_registry_classify_float(f->owner, 0.0f, 0.0f, &value, &error) !=
            MC_REGISTRY_BUSY ||
        value != sentinel ||
        mc_biome_registry_lookup(f->owner, 0.0, 0.0, &value, &error) != MC_REGISTRY_BUSY ||
        value != sentinel || mc_biome_table_rebuild(f->owner, &error) != MC_REGISTRY_BUSY ||
        mc_registry_classes_free(f->owner, &error) != MC_REGISTRY_BUSY ||
        mc_biome_registry_inspect(f->owner, &after, &error) != MC_REGISTRY_OK ||
        !same_view(&before, &after) || f->owner->allocation_bytes != bytes)
        f->invalid = true;
    ++f->observed_callbacks;
    if (f->missing && index == 0)
        return MC_REGISTRY_NULL_REFERENCE;
    *out = (mc_java_properties_input){&f->streams[index], read_bytes};
    return MC_REGISTRY_OK;
}

static mc_registry_status close_resource(void *context, const mc_java_properties_input *input) {
    fixture *f = context;
    if (input == NULL || input->context != &f->streams[2] || input->read != read_bytes) {
        f->invalid = true;
        return MC_REGISTRY_INVALID_ARGUMENT;
    }
    ++f->streams[2].closes;
    f->streams[2].closed = true;
    return MC_REGISTRY_OK;
}

static bool ready_resource(void *context, const mc_java_properties_input *input) {
    fixture *f = context;
    if (input == NULL || input->read != read_bytes)
        f->invalid = true;
    return false; /* Actual finite adapter advertises no immediately available bytes. */
}

static fixture *new_fixture(mc_registry_side side, mc_materials *materials,
                            mc_java_random_profile *profile, size_t budget, bool missing) {
    fixture *f = calloc(1, sizeof(*f));
    if (f == NULL)
        return NULL;
    f->missing = missing;
    f->streams[0] = (stream){language, sizeof(language) - 1, 0, 0, 0, 0, false};
    f->streams[1] = (stream){language, 0, 0, 0, 0, 0, false};
    f->streams[2] = (stream){language, 0, 0, 0, 0, 0, false};
    f->resources = (mc_registry_resources){f, open_resource, close_resource, ready_resource};
    const mc_registry_options options = {side, profile, materials, budget, &f->resources};
    if (mc_registry_classes_allocate(&options, &f->owner, NULL) != MC_REGISTRY_OK) {
        free(f);
        return NULL;
    }
    return f;
}

static void leave_method(fixture *f) {
    if (f != NULL && f->method_entered) {
        f->owner->entered = 0;
        f->method_entered = false;
    }
}

static bool destroy_fixture(fixture *f) {
    if (f == NULL)
        return true;
    leave_method(f); /* Only our explicitly admitted ordinary-method fixture. */
    if (mc_registry_classes_free(f->owner, NULL) != MC_REGISTRY_OK) {
        retirement_failed = true;
        fputs("Biome fixture retirement refused; fixture and dependencies retained\n", stderr);
        return false;
    }
    free(f);
    return true;
}

static size_t opens(const fixture *f) {
    return f->streams[0].opens + f->streams[1].opens + f->streams[2].opens;
}

static size_t reads(const fixture *f) {
    return f->streams[0].reads + f->streams[1].reads + f->streams[2].reads;
}

static bool ascii_is(const mc_registry_string *string, const char *expected) {
    mc_registry_string_view view;
    if (!mc_registry_string_read(string, &view) || view.length != strlen(expected))
        return false;
    for (size_t i = 0; i < view.length; ++i)
        if (view.units[i] != (unsigned char)expected[i])
            return false;
    return true;
}

/* Literal chains from paired kd/gs clinit, not modern names or a C dump. */
static const struct expected_biome {
    mc_biome_kind kind;
    const char *name;
    int32_t o, r;
    bool v, w;
} expected[] = {{MC_BIOME_KIND_YJ_PP, "Rainforest", 588342, 2094168, false, true},
                {MC_BIOME_KIND_TF_MA, "Swampland", 522674, 9154376, false, true},
                {MC_BIOME_KIND_KD_GS, "Seasonal Forest", 10215459, 5169201, false, true},
                {MC_BIOME_KIND_RB_KK, "Forest", 353825, 5159473, false, true},
                {MC_BIOME_KIND_FS_DW, "Savanna", 14278691, 5169201, false, true},
                {MC_BIOME_KIND_KD_GS, "Shrubland", 10595616, 5169201, false, true},
                {MC_BIOME_KIND_G_F, "Taiga", 3060051, 8107825, true, true},
                {MC_BIOME_KIND_FS_DW, "Desert", 16421912, 5169201, false, false},
                {MC_BIOME_KIND_FS_DW, "Plains", 16767248, 5169201, false, true},
                {MC_BIOME_KIND_FS_DW, "Ice Desert", 16772499, 12899129, true, false},
                {MC_BIOME_KIND_KD_GS, "Tundra", 5762041, 12899129, true, true},
                {MC_BIOME_KIND_T_M, "Hell", 16711680, 5169201, false, false},
                {MC_BIOME_KIND_RY_LE, "Sky", 8421631, 5169201, false, false}};

static const char *const class_names[2][13] = {
    {"cn", "uz", "fr", "gb", "uw", "dl", "wh", "ww", "bx", "xt", "gi", "bp", "ya"},
    {"bn", "ne", "dv", "ec", "nb", "ce", "oc", "or", "bd", "pf", "eh", "aw", "pi"}};
static const int32_t weights[] = {10, 10, 10, 10, 10, 12, 10, 10, 8, 10, 2, 10, 10};

static bool check_graph(const mc_biome_registry_view *view, bool sand_tail,
                        const mc_biome_entity_class **classes_out) {
    const mc_biome *biomes[13];
    const mc_biome_spawn_list *lists[39];
    const mc_biome_spawn *records[115];
    const mc_biome_entity_class *entities[13] = {0};
    size_t list_count = 0, record_count = 0;
    named_cells(view, biomes);
    for (size_t i = 0; i < COUNT(biomes); ++i) {
        const mc_biome_fields *f = mc_biome_fields_get(biomes[i]);
        VERIFY(biomes[i] != NULL && f != NULL);
        for (size_t j = 0; j < i; ++j) {
            VERIFY(biomes[i] != biomes[j]);
            VERIFY(f->n != mc_biome_fields_get(biomes[j])->n);
        }
        VERIFY(mc_biome_kind_get(biomes[i]) == expected[i].kind);
        VERIFY(ascii_is(f->n, expected[i].name));
        VERIFY(f->o == expected[i].o && f->r == expected[i].r);
        VERIFY(f->v == expected[i].v && f->w == expected[i].w);
        const bool sand = sand_tail && (i == 7 || i == 9);
        VERIFY(f->p == (sand ? 12 : 2) && f->q == (sand ? 12 : 3));
        const mc_biome_spawn_list *const current[] = {f->s, f->t, f->u};
        for (size_t list = 0; list < COUNT(current); ++list) {
            VERIFY(current[list] != NULL && list_count < COUNT(lists));
            for (size_t old = 0; old < list_count; ++old)
                VERIFY(current[list] != lists[old]);
            lists[list_count++] = current[list];
            size_t length = list == 0 ? 5 : list == 1 ? 4 : 1;
            if ((i == 3 || i == 6) && list == 1)
                ++length;
            if (i == 11)
                length = list == 0 ? 2 : 0;
            if (i == 12)
                length = list == 1 ? 1 : 0;
            VERIFY(mc_biome_spawn_list_size(current[list]) == length);
            VERIFY(mc_biome_spawn_list_at(current[list], length) == NULL);
            VERIFY(mc_biome_spawn_list_at(current[list], SIZE_MAX) == NULL);
            for (size_t ordinal = 0; ordinal < length; ++ordinal) {
                size_t id = list == 0 ? ordinal : list == 1 ? ordinal + 5 : 9;
                if ((i == 3 || i == 6) && list == 1 && ordinal == 4)
                    id = 10;
                if (i == 11)
                    id = 11 + ordinal;
                if (i == 12)
                    id = 7;
                const mc_biome_spawn *record = mc_biome_spawn_list_at(current[list], ordinal);
                const mc_biome_spawn_fields *r = mc_biome_spawn_fields_get(record);
                VERIFY(record != NULL && r != NULL && r->a != NULL && r->b == weights[id]);
                VERIFY(record_count < COUNT(records));
                for (size_t old = 0; old < record_count; ++old)
                    VERIFY(record != records[old]);
                records[record_count++] = record;
                if (entities[id] == NULL)
                    entities[id] = r->a;
                VERIFY(r->a == entities[id]);
                const char *client_name = mc_biome_entity_class_name(r->a, MC_REGISTRY_CLIENT);
                const char *server_name = mc_biome_entity_class_name(r->a, MC_REGISTRY_SERVER);
                VERIFY(client_name != NULL && server_name != NULL);
                VERIFY(strcmp(client_name, class_names[0][id]) == 0);
                VERIFY(strcmp(server_name, class_names[1][id]) == 0);
            }
        }
    }
    VERIFY(list_count == 39 && record_count == 115);
    for (size_t i = 0; i < COUNT(entities); ++i) {
        VERIFY(entities[i] != NULL);
        for (size_t j = 0; j < i; ++j)
            VERIFY(entities[i] != entities[j]);
        if (classes_out != NULL)
            classes_out[i] = entities[i];
    }
    return true;
}

static float float_bits(uint32_t bits) {
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static double double_bits(uint64_t bits) {
    double value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static bool check_queries(fixture *f, const mc_biome_registry_view *view) {
    const mc_biome *biomes[13];
    named_cells(view, biomes);

    /* Every branch and comparison boundary uses independently literal inputs.
     * Rainfall is multiplied by temperature in binary32 before branching. */
    const struct {
        float t, r;
        char field;
    } points[] = {{0.0f, 1.0f, 'k'},
                  {-1.0f, 1.0f, 'k'},
                  {0.1f, 1.0f, 'k'},
                  {0.1f, 2.0f, 'g'},
                  {0.4f, 0.5f, 'g'},
                  {0.5f, 0.0f, 'e'},
                  {0.95f, 0.0f, 'h'},
                  {0.6f, 1.0f, 'b'},
                  {0.7f, 1.0f, 'd'},
                  {0.3f, 1.0f, 'g'},
                  {0.6f, 0.5f, 'f'},
                  {0.6f, 0.6f, 'd'},
                  {0.5f, 0.4f, 'f'},
                  {0.5f, 0.7f, 'd'},
                  {0.97f, 0.3f, 'i'},
                  {1.0f, 0.45f, 'c'},
                  {1.0f, 0.9f, 'a'},
                  {2.0f, 0.0f, 'h'},
                  {1.0f, -1.0f, 'h'},
                  {float_bits(UINT32_C(0x3dcccccc)), 2.0f, 'k'},
                  {float_bits(UINT32_C(0x3effffff)), 1.0f, 'g'},
                  {float_bits(UINT32_C(0x3f333332)), 1.0f, 'b'},
                  {float_bits(UINT32_C(0x3f7851eb)), 0.5f, 'd'},
                  {1.0f, float_bits(UINT32_C(0x3ee66665)), 'i'},
                  {1.0f, float_bits(UINT32_C(0x3f666665)), 'c'},
                  {float_bits(UINT32_C(0x7fc00001)), 1.0f, 'a'},
                  {0.3f, float_bits(UINT32_C(0x7fc00001)), 'g'},
                  {0.6f, float_bits(UINT32_C(0x7fc00001)), 'd'},
                  {1.0f, float_bits(UINT32_C(0x7fc00001)), 'a'},
                  {float_bits(UINT32_C(0x7f800000)), 0.0f, 'a'},
                  {float_bits(UINT32_C(0x7f800000)), -1.0f, 'h'},
                  {float_bits(UINT32_C(0xff800000)), 1.0f, 'k'}};

    const mc_biome *result = NULL;
    for (size_t i = 0; i < COUNT(points); ++i) {
        VERIFY(mc_biome_registry_classify_float(f->owner, points[i].t, points[i].r, &result,
                                                NULL) == MC_REGISTRY_OK);
        VERIFY(result == biomes[(size_t)(points[i].field - 'a')]);
    }
    bool selected[13] = {false};
    for (size_t rain = 0; rain < 64; ++rain)
        for (size_t temperature = 0; temperature < 64; ++temperature) {
            size_t index = temperature + rain * 64;
            VERIFY(mc_biome_registry_classify_float(f->owner, (float)temperature / 63.0f,
                                                    (float)rain / 63.0f, &result,
                                                    NULL) == MC_REGISTRY_OK);
            VERIFY(view->x[index] == result);
            size_t named = 0;
            while (named < COUNT(biomes) && result != biomes[named])
                ++named;
            VERIFY(named < COUNT(biomes) && named != 9 && named != 11 && named != 12);
            selected[named] = true;
        }
    for (size_t i = 0; i < COUNT(selected); ++i)
        VERIFY(selected[i] == (i != 9 && i != 11 && i != 12));

    const double inf = double_bits(UINT64_C(0x7ff0000000000000));
    const double nan = double_bits(UINT64_C(0x7ff8000000000001));

    /* Literal expected wrapped indices: neither floor nor unit clamping. */
    const struct {
        double t, r;
        int32_t index;
    } lookups[] = {{0.0, 0.0, 0},          {-0.0, -0.0, 0},
                   {-0.001, 0.0, 0},       {1.0, 0.0, 63},
                   {1.01, 0.0, 63},        {1.02, 0.0, 64},
                   {0.0, 1.0, 4032},       {1.0, 1.0, 4095},
                   {0.5, 0.5, 2015},       {64.0, 0.0, 4032},
                   {-64.0, 1.0, 0},        {128.0, -1.0, 4032},
                   {nan, nan, 0},          {0.0, -inf, 0},
                   {1.0, -inf, 63},        {-1.0, 0.0, -63},
                   {0.0, 2.0, 8064},       {0.0, inf, -64},
                   {1.0, inf, -1},         {inf, 0.0, INT32_MAX},
                   {-inf, 0.0, INT32_MIN}, {inf, inf, INT32_MAX - 64}};

    for (size_t i = 0; i < COUNT(lookups); ++i) {
        result = biomes[0];
        mc_registry_error error;
        mc_registry_status status =
            mc_biome_registry_lookup(f->owner, lookups[i].t, lookups[i].r, &result, &error);
        if (lookups[i].index >= 0 && lookups[i].index < 4096) {
            VERIFY(status == MC_REGISTRY_OK && result == view->x[(size_t)lookups[i].index]);
        } else {
            VERIFY(status == MC_REGISTRY_INDEX_OUT_OF_RANGE);
            VERIFY(error.status == status && result == biomes[0]);
        }
    }
    result = biomes[0];
    VERIFY(mc_biome_static_get(f->owner, 'z', &result, NULL) == MC_REGISTRY_INVALID_ARGUMENT);
    VERIFY(result == biomes[0]);
    VERIFY(mc_biome_static_get(f->owner, 'a', NULL, NULL) == MC_REGISTRY_INVALID_ARGUMENT);
    VERIFY(mc_biome_static_get(NULL, 'a', &result, NULL) == MC_REGISTRY_INVALID_ARGUMENT);
    VERIFY(result == biomes[0]);
    VERIFY(mc_biome_registry_classify_float(f->owner, 0.0f, 0.0f, NULL, NULL) ==
           MC_REGISTRY_INVALID_ARGUMENT);
    VERIFY(mc_biome_registry_classify_float(NULL, 0.0f, 0.0f, &result, NULL) ==
           MC_REGISTRY_INVALID_ARGUMENT);
    VERIFY(result == biomes[0]);
    VERIFY(mc_biome_registry_lookup(f->owner, 0.0, 0.0, NULL, NULL) ==
           MC_REGISTRY_INVALID_ARGUMENT);
    VERIFY(mc_biome_registry_lookup(NULL, 0.0, 0.0, &result, NULL) == MC_REGISTRY_INVALID_ARGUMENT);
    VERIFY(result == biomes[0]);
    return true;
}

static bool test_success(mc_registry_side side, mc_materials *materials,
                         mc_java_random_profile *profile, size_t *complete_bytes, size_t *raw_cost,
                         size_t *cold_bytes) {
    bool ok = false;
    fixture *f = NULL;
    fixture *other = NULL;
    mc_biome_registry_view cold, view, after;
    mc_registry_block_view block;
    mc_registry_item_view item;
    const mc_biome_entity_class *entities[13], *other_entities[13];
    uint64_t start = uniquifier(profile);
    size_t profile_bytes = mc_java_random_profile_allocation_bytes(profile);
    f = new_fixture(side, materials, profile, 0, false);
    CHECK(f != NULL);
    CHECK(mc_biome_registry_inspect(f->owner, &cold, NULL) == MC_REGISTRY_OK);
    CHECK(cold.phase == MC_REGISTRY_COLD && cold.initializer_pc == -1 && no_named(&cold));
    CHECK(cold.x == NULL && cold.x_length == 0);
    CHECK(mc_registry_classes_inspect_blocks(f->owner, &block, NULL) == MC_REGISTRY_OK);
    CHECK(mc_registry_classes_inspect_items(f->owner, &item, NULL) == MC_REGISTRY_OK);
    CHECK(block.phase == MC_REGISTRY_COLD && block.m == NULL && item.phase == MC_REGISTRY_COLD);
    CHECK(item.b == NULL && item.c == NULL && opens(f) == 0 && reads(f) == 0);
    CHECK(uniquifier(profile) == start && profile->math == NULL);
    CHECK(mc_java_random_profile_allocation_bytes(profile) == profile_bytes);
    *cold_bytes = block.allocation_bytes;
    after = cold;
    CHECK(mc_biome_registry_inspect(NULL, &after, NULL) == MC_REGISTRY_INVALID_ARGUMENT);
    CHECK(same_view(&after, &cold));
    CHECK(mc_biome_registry_inspect(f->owner, NULL, NULL) == MC_REGISTRY_INVALID_ARGUMENT);
    CHECK(mc_registry_classes_initialize_biomes(NULL, NULL) == MC_REGISTRY_INVALID_ARGUMENT);
    CHECK(mc_biome_table_rebuild(NULL, NULL) == MC_REGISTRY_INVALID_ARGUMENT);
    CHECK(mc_biome_kind_get(NULL) == MC_BIOME_KIND_NONE && mc_biome_fields_get(NULL) == NULL);
    CHECK(mc_biome_spawn_list_size(NULL) == 0 && mc_biome_spawn_list_at(NULL, 0) == NULL);
    CHECK(mc_biome_spawn_fields_get(NULL) == NULL &&
          mc_biome_entity_class_name(NULL, side) == NULL);

    CHECK(mc_registry_classes_initialize_biomes(f->owner, NULL) == MC_REGISTRY_OK);
    CHECK(mc_biome_registry_inspect(f->owner, &view, NULL) == MC_REGISTRY_OK);
    CHECK(view.phase == MC_REGISTRY_READY && view.initializer_pc == 323);
    CHECK(view.x != NULL && view.x_length == 4096 && check_graph(&view, true, entities));
    CHECK(mc_registry_classes_inspect_blocks(f->owner, &block, NULL) == MC_REGISTRY_OK);
    CHECK(mc_registry_classes_inspect_items(f->owner, &item, NULL) == MC_REGISTRY_OK);
    CHECK(block.phase == MC_REGISTRY_READY && block.initializer_pc == 3677);
    CHECK(item.phase == MC_REGISTRY_READY && item.initializer_pc == 2667);
    CHECK(uniquifier(profile) == advance_five(start) && profile->math == NULL);
    CHECK(opens(f) == 3 && f->observed_callbacks == 3 && !f->invalid);
    CHECK(f->streams[0].closes == 0 && f->streams[1].closes == 0 && f->streams[2].closes == 1);
    *complete_bytes = block.allocation_bytes;
    size_t byte_snapshot = block.allocation_bytes, read_snapshot = reads(f);
    mc_biome_fields field_snapshot[13];
    const mc_biome *values[13], *table_snapshot[4096];
    named_cells(&view, values);
    for (size_t i = 0; i < COUNT(field_snapshot); ++i)
        field_snapshot[i] = *mc_biome_fields_get(values[i]);
    for (size_t i = 0; i < COUNT(table_snapshot); ++i)
        table_snapshot[i] = view.x[i];
    CHECK(check_queries(f, &view));
    for (char field = 'a'; field <= 'm'; ++field) {
        const mc_biome *value = NULL;
        CHECK(mc_biome_static_get(f->owner, field, &value, NULL) == MC_REGISTRY_OK);
        CHECK(value == values[(size_t)(field - 'a')]);
    }
    CHECK(mc_registry_classes_initialize_biomes(f->owner, NULL) == MC_REGISTRY_OK);
    CHECK(mc_biome_table_rebuild(f->owner, NULL) == MC_REGISTRY_OK);
    CHECK(mc_biome_registry_inspect(f->owner, &after, NULL) == MC_REGISTRY_OK &&
          same_view(&view, &after));
    CHECK(check_graph(&after, true, NULL));
    for (size_t i = 0; i < COUNT(table_snapshot); ++i)
        CHECK(after.x[i] == table_snapshot[i]);
    for (size_t i = 0; i < COUNT(field_snapshot); ++i) {
        const mc_biome_fields *current = mc_biome_fields_get(values[i]);
        const mc_biome_fields *old = &field_snapshot[i];
        CHECK(current->n == old->n && current->o == old->o && current->p == old->p &&
              current->q == old->q);
        CHECK(current->r == old->r && current->s == old->s && current->t == old->t &&
              current->u == old->u);
        CHECK(current->v == old->v && current->w == old->w);
    }
    CHECK(f->owner->allocation_bytes == byte_snapshot && reads(f) == read_snapshot &&
          opens(f) == 3);
    CHECK(uniquifier(profile) == advance_five(start) && profile->math == NULL);

    /* Real ordinary method admission after actual class READY; no phase edit.
     * The returned RAW object is public-field diagnosed before construction. */
    f->owner->entered = 1;
    f->method_entered = true;
    mc_biome *raw = NULL;
    CHECK(mc_registry_biome_allocate(f->owner, MC_BIOME_KIND_NONE, &raw) ==
          MC_REGISTRY_INVALID_ARGUMENT);
    CHECK(raw == NULL && f->owner->allocation_bytes == byte_snapshot);
    CHECK(mc_registry_biome_allocate(f->owner, MC_BIOME_KIND_COUNT, &raw) ==
          MC_REGISTRY_INVALID_ARGUMENT);
    CHECK(raw == NULL && f->owner->allocation_bytes == byte_snapshot);
    CHECK(mc_registry_biome_allocate(f->owner, MC_BIOME_KIND_KD_GS, NULL) ==
          MC_REGISTRY_INVALID_ARGUMENT);
    CHECK(f->owner->allocation_bytes == byte_snapshot);
    CHECK(mc_registry_biome_allocate(f->owner, MC_BIOME_KIND_KD_GS, &raw) == MC_REGISTRY_OK);
    *raw_cost = f->owner->allocation_bytes - byte_snapshot;
    const mc_biome_fields *fields = mc_biome_fields_get(raw);
    CHECK(fields != NULL && fields->n == NULL && fields->o == 0 && fields->p == 0 &&
          fields->q == 0);
    CHECK(fields->r == 0 && fields->s == NULL && fields->t == NULL && fields->u == NULL);
    CHECK(!fields->v && !fields->w && *raw_cost != 0);
    CHECK(mc_registry_biome_construct(raw) == MC_REGISTRY_OK);
    CHECK(fields->n == NULL && fields->o == 0 && fields->p == 2 && fields->q == 3 &&
          fields->r == 5169201);
    CHECK(!fields->v && fields->w && mc_biome_spawn_list_size(fields->s) == 5);
    CHECK(mc_biome_spawn_list_size(fields->t) == 4 && mc_biome_spawn_list_size(fields->u) == 1);
    size_t ordinary_bytes = f->owner->allocation_bytes;
    CHECK(mc_registry_biome_construct(raw) == MC_REGISTRY_INVALID_ARGUMENT);
    CHECK(f->owner->allocation_bytes == ordinary_bytes);
    mc_biome *ordinary_desert_kind = NULL;
    CHECK(mc_registry_biome_allocate(f->owner, MC_BIOME_KIND_FS_DW, &ordinary_desert_kind) ==
          MC_REGISTRY_OK);
    CHECK(mc_registry_biome_construct(ordinary_desert_kind) == MC_REGISTRY_OK);
    const mc_biome_fields *ordinary_fields = mc_biome_fields_get(ordinary_desert_kind);
    CHECK(ordinary_desert_kind != view.h && ordinary_desert_kind != view.j);
    CHECK(ordinary_fields != NULL && ordinary_fields->n == NULL && ordinary_fields->o == 0);
    CHECK(ordinary_fields->p == 2 && ordinary_fields->q == 3 && ordinary_fields->r == 5169201);
    CHECK(!ordinary_fields->v && ordinary_fields->w);
    CHECK(mc_biome_spawn_list_size(ordinary_fields->s) == 5);
    CHECK(mc_biome_spawn_list_size(ordinary_fields->t) == 4);
    CHECK(mc_biome_spawn_list_size(ordinary_fields->u) == 1);
    leave_method(f);
    CHECK(mc_biome_registry_inspect(f->owner, &after, NULL) == MC_REGISTRY_OK &&
          same_view(&view, &after));
    CHECK(uniquifier(profile) == advance_five(start) && opens(f) == 3);

    /* A separate real class universe has disjoint object/list/Class owners.
     * Cold rebuild enters real clinit and its requested additional body. */
    other = new_fixture(side, materials, profile, 0, false);
    CHECK(other != NULL && mc_biome_table_rebuild(other->owner, NULL) == MC_REGISTRY_OK);
    CHECK(mc_biome_registry_inspect(other->owner, &after, NULL) == MC_REGISTRY_OK);
    CHECK(after.phase == MC_REGISTRY_READY && after.initializer_pc == 323 &&
          after.x_length == 4096);
    CHECK(check_graph(&after, true, other_entities));
    CHECK(after.x != view.x && after.a != view.a);
    CHECK(mc_biome_fields_get(after.a)->s != mc_biome_fields_get(view.a)->s);
    for (size_t i = 0; i < COUNT(entities); ++i)
        CHECK(entities[i] != other_entities[i]);
    CHECK(!other->invalid && other->observed_callbacks == 3);
    CHECK(uniquifier(profile) == advance_five(advance_five(start)) && profile->math == NULL);
    CHECK(mc_java_random_profile_allocation_bytes(profile) == profile_bytes);
    ok = true;
cleanup:
    if (!destroy_fixture(other))
        ok = false;
    if (!destroy_fixture(f))
        ok = false;
    return ok;
}

static bool test_missing(mc_registry_side side, mc_materials *materials,
                         mc_java_random_profile *profile) {
    bool ok = false;
    fixture *f = new_fixture(side, materials, profile, 0, true);
    mc_registry_error first, retry;
    mc_biome_registry_view view, after;
    mc_registry_block_view block;
    CHECK(f != NULL);
    uint64_t start = uniquifier(profile);
    const mc_biome *value = (const mc_biome *)f;
    CHECK(mc_biome_static_get(f->owner, 'a', &value, &first) == MC_REGISTRY_NULL_REFERENCE);
    CHECK(value == (const mc_biome *)f && first.status == MC_REGISTRY_NULL_REFERENCE);
    CHECK(first.active_initializer_class != NULL && first.constructor_class != NULL);
    CHECK(strcmp(first.active_initializer_class, side == MC_REGISTRY_CLIENT ? "nh" : "ii") == 0);
    CHECK(first.active_initializer_pc == 4 && first.constructor_pc == 26);
    CHECK(mc_biome_registry_inspect(f->owner, &view, NULL) == MC_REGISTRY_OK);
    CHECK(view.phase == MC_REGISTRY_FAILED && view.initializer_pc == 4);
    CHECK(no_named(&view) && view.x == NULL && view.x_length == 0);
    CHECK(mc_registry_classes_inspect_blocks(f->owner, &block, NULL) == MC_REGISTRY_OK);
    CHECK(block.phase == MC_REGISTRY_FAILED && block.m != NULL && block.named_blocks != 0);
    CHECK(uniquifier(profile) == advance_five(start) && opens(f) == 1 && reads(f) == 0 &&
          !f->invalid);
    size_t bytes = f->owner->allocation_bytes;
    CHECK(mc_registry_classes_initialize_biomes(f->owner, &retry) ==
          MC_REGISTRY_FAILED_INITIALIZATION);
    CHECK(retry.status == MC_REGISTRY_FAILED_INITIALIZATION && same_cause(&first, &retry));
    CHECK(mc_biome_registry_classify_float(f->owner, 0.0f, 0.0f, &value, &retry) ==
          MC_REGISTRY_FAILED_INITIALIZATION);
    CHECK(value == (const mc_biome *)f && same_cause(&first, &retry));
    CHECK(mc_biome_registry_lookup(f->owner, 0.0, 0.0, &value, &retry) ==
          MC_REGISTRY_FAILED_INITIALIZATION);
    CHECK(value == (const mc_biome *)f && same_cause(&first, &retry));
    CHECK(mc_biome_table_rebuild(f->owner, &retry) == MC_REGISTRY_FAILED_INITIALIZATION);
    CHECK(same_cause(&first, &retry));
    CHECK(mc_biome_registry_inspect(f->owner, &after, NULL) == MC_REGISTRY_OK &&
          same_view(&view, &after));
    CHECK(f->owner->allocation_bytes == bytes && opens(f) == 1 && reads(f) == 0);
    CHECK(uniquifier(profile) == advance_five(start) && profile->math == NULL);
    ok = true;
cleanup:
    if (!destroy_fixture(f))
        ok = false;
    return ok;
}

static bool test_budgets(mc_registry_side side, mc_materials *materials,
                         mc_java_random_profile *profile, size_t complete_bytes, size_t raw_cost,
                         size_t cold_bytes) {
    bool ok = false;
    fixture *early = NULL, *late = NULL, *ordinary = NULL;
    mc_biome_registry_view view, after;
    mc_registry_error first, retry;
    uint64_t start = uniquifier(profile);
    early = new_fixture(side, materials, profile, cold_bytes, false);
    CHECK(early != NULL);
    CHECK(mc_registry_classes_initialize_biomes(early->owner, &first) == MC_REGISTRY_LIMIT);
    CHECK(mc_biome_registry_inspect(early->owner, &view, NULL) == MC_REGISTRY_OK);
    CHECK(view.phase == MC_REGISTRY_FAILED && no_named(&view) && view.x == NULL);
    CHECK(opens(early) == 0 && uniquifier(profile) == start);
    size_t bytes = early->owner->allocation_bytes;
    CHECK(mc_registry_classes_initialize_biomes(early->owner, &retry) ==
          MC_REGISTRY_FAILED_INITIALIZATION);
    CHECK(same_cause(&first, &retry) && early->owner->allocation_bytes == bytes);
    CHECK(uniquifier(profile) == start && opens(early) == 0);

    /* The final source allocation is x[4096] at314. One byte less than the
     * actual successful ledger retains all named children, before sand tail. */
    CHECK(complete_bytes > cold_bytes && raw_cost != 0);
    late = new_fixture(side, materials, profile, complete_bytes - 1, false);
    CHECK(late != NULL);
    CHECK(mc_registry_classes_initialize_biomes(late->owner, &first) == MC_REGISTRY_LIMIT);
    CHECK(mc_biome_registry_inspect(late->owner, &view, NULL) == MC_REGISTRY_OK);
    CHECK(view.phase == MC_REGISTRY_FAILED && view.initializer_pc == 314);
    CHECK(view.x == NULL && view.x_length == 0 && check_graph(&view, false, NULL));
    CHECK(uniquifier(profile) == advance_five(start) && opens(late) == 3 && !late->invalid);
    bytes = late->owner->allocation_bytes;
    size_t read_snapshot = reads(late);
    CHECK(mc_registry_classes_initialize_biomes(late->owner, &retry) ==
          MC_REGISTRY_FAILED_INITIALIZATION);
    CHECK(same_cause(&first, &retry) && late->owner->allocation_bytes == bytes);
    CHECK(reads(late) == read_snapshot && opens(late) == 3 &&
          uniquifier(profile) == advance_five(start));
    CHECK(mc_biome_registry_inspect(late->owner, &after, NULL) == MC_REGISTRY_OK &&
          same_view(&view, &after));

    CHECK(raw_cost <= SIZE_MAX - complete_bytes);
    ordinary = new_fixture(side, materials, profile, complete_bytes + raw_cost, false);
    CHECK(ordinary != NULL &&
          mc_registry_classes_initialize_biomes(ordinary->owner, NULL) == MC_REGISTRY_OK);
    CHECK(ordinary->owner->allocation_bytes == complete_bytes);
    CHECK(mc_biome_registry_inspect(ordinary->owner, &view, NULL) == MC_REGISTRY_OK);
    ordinary->owner->entered = 1;
    ordinary->method_entered = true;
    mc_biome *raw = NULL;
    CHECK(mc_registry_biome_allocate(ordinary->owner, MC_BIOME_KIND_KD_GS, &raw) == MC_REGISTRY_OK);
    CHECK(raw != NULL && ordinary->owner->allocation_bytes == complete_bytes + raw_cost);
    CHECK(mc_registry_biome_construct(raw) == MC_REGISTRY_LIMIT);
    const mc_biome_fields *fields = mc_biome_fields_get(raw);
    CHECK(fields != NULL && fields->n == NULL && fields->o == 0);
    CHECK(fields->p == 2 && fields->q == 3 && fields->r == 5169201);
    CHECK(fields->s == NULL && fields->t == NULL && fields->u == NULL && !fields->v && !fields->w);
    bytes = ordinary->owner->allocation_bytes;
    CHECK(mc_registry_biome_construct(raw) == MC_REGISTRY_INVALID_ARGUMENT);
    CHECK(ordinary->owner->allocation_bytes == bytes);
    leave_method(ordinary);
    CHECK(mc_biome_registry_inspect(ordinary->owner, &after, NULL) == MC_REGISTRY_OK);
    CHECK(after.phase == MC_REGISTRY_READY && same_view(&view, &after));
    const mc_biome *value = NULL;
    CHECK(mc_biome_registry_lookup(ordinary->owner, 0.0, 0.0, &value, NULL) == MC_REGISTRY_OK &&
          value == view.x[0]);
    CHECK(opens(ordinary) == 3 && !ordinary->invalid);
    CHECK(uniquifier(profile) == advance_five(advance_five(start)) && profile->math == NULL);
    ok = true;
cleanup:
    if (!destroy_fixture(ordinary))
        ok = false;
    if (!destroy_fixture(late))
        ok = false;
    if (!destroy_fixture(early))
        ok = false;
    return ok;
}

int main(void) {
    int result = 1;
    mc_materials *materials = NULL;
    mc_java_random_profile *profile = NULL;
    CHECK(mc_materials_create(&materials));
    CHECK(mc_java_random_profile_create(
              sizeof(mc_java_random_profile) + sizeof(mc_java_math_random_owner), &profile) ==
          MC_JAVA_RANDOM_PROFILE_OK);
    mc_material *material_ids[MC_MATERIAL_COUNT];

    struct material_snapshot {
        bool a, b, c, d, e, g, h, i;
        int32_t j;
    } material_fields[MC_MATERIAL_COUNT];

    for (int i = 0; i < MC_MATERIAL_COUNT; ++i)
        material_ids[i] = mc_materials_get(materials, (mc_material_id)i);
    for (int i = 0; i < MC_MATERIAL_COUNT; ++i) {
        const mc_material *m = material_ids[i];
        material_fields[i] =
            (struct material_snapshot){mc_material_a(m), mc_material_b(m), mc_material_c(m),
                                       mc_material_d(m), mc_material_e(m), mc_material_g(m),
                                       mc_material_h(m), mc_material_i(m), mc_material_j(m)};
    }
    for (int side = MC_REGISTRY_CLIENT; side <= MC_REGISTRY_SERVER; ++side) {
        size_t complete_bytes = 0, raw_cost = 0, cold_bytes = 0;
        CHECK(test_success((mc_registry_side)side, materials, profile, &complete_bytes, &raw_cost,
                           &cold_bytes));
        CHECK(test_missing((mc_registry_side)side, materials, profile));
        CHECK(test_budgets((mc_registry_side)side, materials, profile, complete_bytes, raw_cost,
                           cold_bytes));
        for (int i = 0; i < MC_MATERIAL_COUNT; ++i) {
            CHECK(mc_materials_get(materials, (mc_material_id)i) == material_ids[i]);
            const mc_material *m = material_ids[i];
            const struct material_snapshot *old = &material_fields[i];
            CHECK(mc_material_a(m) == old->a && mc_material_b(m) == old->b &&
                  mc_material_c(m) == old->c);
            CHECK(mc_material_d(m) == old->d && mc_material_e(m) == old->e &&
                  mc_material_g(m) == old->g);
            CHECK(mc_material_h(m) == old->h && mc_material_i(m) == old->i &&
                  mc_material_j(m) == old->j);
        }
    }
    result = 0;
cleanup:
    if (!retirement_failed) {
        mc_java_random_profile_free(profile);
        mc_materials_destroy(materials);
    } else {
        result = 1;
    }
    if (result == 0)
        printf("PASS: %u biome registry checks\n", checks);
    return result;
}
