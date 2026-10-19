#include "mc/core/java/reader.h"
#include "mc/resources/translation/translation.h"
#include "mc/statistics/keys/keys.h"

#include "../../../src/block/registry/registry_internal.h"
#include "../../../src/core/java/random_profile_internal.h"

#include <stdio.h>
#include <string.h>

#define COUNT(array) (sizeof(array) / sizeof((array)[0]))
static unsigned int checks;
static bool retirement_failed;

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #condition);                \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

/* Synthetic ASCII-compatible bytes, never an original /achievement/map.txt.
 * The binary NUL is part of the final value, not this input's length marker. */
static const uint8_t valid_bytes[] = "7,old\n7,new\n"
                                     "-2147483648,min\r\n+2147483647,max,ignored\n"
                                     "-0,zero\r+0,zero-final\n1,,x\n2,,y\n3,same\n4,same\n"
                                     "5, value \n6,a,b,c\n0008,padded\n-1,neg\n9,nul\0value\n";
static const int32_t valid_ids[] = {7, INT32_MIN, INT32_MAX, 0, 1, 2, 3, 4, 5, 6, 8, -1, 9};
static const char *const valid_values[] = {"new",    "min",  "max",  "zero-final", "",
                                           "",       "same", "same", " value ",    "a",
                                           "padded", "neg",  NULL};
static const uint16_t nul_value[] = {'n', 'u', 'l', 0, 'v', 'a', 'l', 'u', 'e'};

static bool units_are(const mc_registry_string *string, const uint16_t *units, size_t length)
{
    mc_registry_string_view view;
    if (!mc_registry_string_read(string, &view) || view.length != length ||
        (length != 0 && view.units == NULL))
        return false;
    for (size_t i = 0; i < length; ++i)
        if (view.units[i] != units[i])
            return false;
    return true;
}

static bool text_is(const mc_registry_string *string, const char *text)
{
    mc_registry_string_view view;
    size_t length = strlen(text);
    if (!mc_registry_string_read(string, &view) || view.length != length ||
        (length != 0 && view.units == NULL))
        return false;
    for (size_t i = 0; i < length; ++i)
        if (view.units[i] != (unsigned char)text[i])
            return false;
    return true;
}

static bool same_view(const mc_registry_statistic_key_view *a,
                      const mc_registry_statistic_key_view *b)
{
    return a->phase == b->phase && a->initializer_pc == b->initializer_pc && a->a == b->a &&
           a->construction == b->construction && a->map_published == b->map_published &&
           a->constructor_pc == b->constructor_pc && a->caught_status == b->caught_status &&
           a->caught_pc == b->caught_pc;
}

static bool same_error(const mc_registry_error *a, const mc_registry_error *b)
{
    return a->status == b->status && a->message == b->message &&
           a->initializer_pc == b->initializer_pc && a->block_id == b->block_id &&
           a->constructor_class == b->constructor_class && a->constructor_pc == b->constructor_pc &&
           a->profile_status == b->profile_status &&
           a->active_initializer_class == b->active_initializer_class &&
           a->active_initializer_pc == b->active_initializer_pc &&
           a->item_argument == b->item_argument;
}

static const mc_registry_string *entry_at(const mc_statistic_keys *keys, int32_t wanted)
{
    for (size_t i = 0; i < mc_statistic_keys_size(keys); ++i) {
        int32_t id;
        const mc_registry_string *value = NULL;
        if (mc_statistic_keys_entry(keys, i, &id, &value) != MC_REGISTRY_OK)
            return NULL;
        if (id == wanted)
            return value;
    }
    return NULL;
}

typedef struct resource {
    const uint8_t *bytes;
    size_t length, offset, chunk, opens, reads, closes, ready_calls, fail_read;
    bool missing, advertised, closed, invalid, observe, reenter;
    mc_registry_status close_status;
    mc_registry_classes *owner;
    mc_registry_resources source;
    const mc_statistic_keys *construction;
    const mc_registry_string *old_value;
    const uint16_t *old_units;
} resource;

static bool observe(resource *r, int32_t pc)
{
    mc_registry_statistic_key_view view;
    mc_registry_error error;
    if (mc_registry_classes_inspect_statistic_keys(r->owner, &view, &error) != MC_REGISTRY_OK ||
        view.phase != MC_REGISTRY_INITIALIZING || view.initializer_pc != 4 || view.a != NULL ||
        view.construction == NULL || !view.map_published || view.constructor_pc != pc ||
        view.caught_status != MC_REGISTRY_OK || view.caught_pc != -1)
        return false;
    if (r->construction == NULL)
        r->construction = view.construction;
    if (view.construction != r->construction)
        return false;
    if (r->observe && r->old_value == NULL) {
        const mc_registry_string *value = entry_at(view.construction, 7);
        if (value != NULL && text_is(value, "old")) {
            mc_registry_string_view string;
            if (!mc_registry_string_read(value, &string))
                return false;
            r->old_value = value;
            r->old_units = string.units;
        }
    }
    if (r->old_value != NULL && !text_is(r->old_value, "old"))
        return false;
    if (r->reenter) {
        size_t bytes = r->owner->allocation_bytes;
        const mc_registry_error last = r->owner->last_error;
        const mc_registry_string *out = r->old_value;
        if (mc_registry_classes_initialize_statistic_keys(r->owner, &error) != MC_REGISTRY_BUSY ||
            mc_registry_classes_statistic_key(r->owner, 7, &out, &error) != MC_REGISTRY_BUSY ||
            out != r->old_value || mc_registry_classes_free(r->owner, &error) != MC_REGISTRY_BUSY)
            return false;
        mc_registry_statistic_key_view after;
        if (mc_registry_classes_inspect_statistic_keys(r->owner, &after, &error) !=
                MC_REGISTRY_OK ||
            !same_view(&view, &after) || r->owner->allocation_bytes != bytes ||
            !same_error(&last, &r->owner->last_error))
            return false;
    }
    return true;
}

static mc_java_properties_status read_bytes(void *context, uint8_t *out, size_t capacity,
                                            size_t *count)
{
    resource *r = context;
    ++r->reads;
    if (!observe(r, 38))
        r->invalid = true;
    if (out == NULL || count == NULL || capacity == 0 || capacity > 8192 || r->offset > r->length) {
        r->invalid = true;
        return MC_JAVA_PROPERTIES_IO_ERROR;
    }
    *count = 0;
    if (r->closed || (r->fail_read != 0 && r->reads == r->fail_read))
        return MC_JAVA_PROPERTIES_IO_ERROR;
    size_t take = r->length - r->offset;
    if (take > r->chunk)
        take = r->chunk;
    if (take > capacity)
        take = capacity;
    if (take != 0)
        memcpy(out, r->bytes + r->offset, take);
    r->offset += take;
    *count = take;
    return MC_JAVA_PROPERTIES_OK;
}

static mc_registry_status open_resource(void *context, const char *name,
                                        mc_java_properties_input *out)
{
    resource *r = context;
    ++r->opens;
    if (r->opens != 1 || name == NULL || strcmp(name, "/achievement/map.txt") != 0 || out == NULL ||
        !observe(r, 27)) {
        r->invalid = true;
        return MC_REGISTRY_INVALID_ARGUMENT;
    }
    if (mc_statistic_keys_size(r->construction) != 0)
        r->invalid = true;
    if (r->missing)
        return MC_REGISTRY_NULL_REFERENCE;
    *out = (mc_java_properties_input){r, read_bytes};
    return MC_REGISTRY_OK;
}

static mc_registry_status close_resource(void *context, const mc_java_properties_input *input)
{
    resource *r = context;
    ++r->closes;
    if (input == NULL || input->context != r || input->read != read_bytes || !observe(r, 83))
        r->invalid = true;
    r->closed = true;
    return r->close_status;
}

static bool ready_resource(void *context, const mc_java_properties_input *input)
{
    resource *r = context;
    ++r->ready_calls;
    if (input == NULL || input->context != r || input->read != read_bytes)
        r->invalid = true;
    return r->advertised && r->offset < r->length;
}

static void resource_init(resource *r, const uint8_t *bytes, size_t length, size_t chunk)
{
    *r = (resource){
        .bytes = bytes, .length = length, .chunk = chunk, .close_status = MC_REGISTRY_OK};
    r->source = (mc_registry_resources){r, open_resource, close_resource, ready_resource};
}

typedef struct material_snapshot {
    mc_material *identity;
    const mc_material_palette *palette;
    bool a, b, c, d, e, g, h, i;
    int32_t j;
} material_snapshot;

typedef struct dependencies {
    mc_materials *materials;
    mc_java_random_profile *profile;
    uint64_t uniquifier;
    size_t bytes;
    material_snapshot material[MC_MATERIAL_COUNT];
} dependencies;

static material_snapshot material_read(mc_material *m)
{
    return (material_snapshot){m,
                               mc_material_palette_of(m),
                               mc_material_a(m),
                               mc_material_b(m),
                               mc_material_c(m),
                               mc_material_d(m),
                               mc_material_e(m),
                               mc_material_g(m),
                               mc_material_h(m),
                               mc_material_i(m),
                               mc_material_j(m)};
}

static bool dependencies_unchanged(const dependencies *d, const mc_registry_classes *owner)
{
    if (atomic_load_explicit(&d->profile->uniquifier, memory_order_relaxed) != d->uniquifier ||
        atomic_load_explicit(&d->profile->holder_phase, memory_order_relaxed) !=
            MC_JAVA_RANDOM_HOLDER_COLD ||
        d->profile->math != NULL || mc_java_random_profile_allocation_bytes(d->profile) != d->bytes)
        return false;
    for (size_t i = 0; i < MC_MATERIAL_COUNT; ++i) {
        material_snapshot a = d->material[i];
        material_snapshot b = material_read(mc_materials_get(d->materials, (mc_material_id)i));
        if (a.identity != b.identity || a.palette != b.palette || a.a != b.a || a.b != b.b ||
            a.c != b.c || a.d != b.d || a.e != b.e || a.g != b.g || a.h != b.h || a.i != b.i ||
            a.j != b.j)
            return false;
    }
    if (owner != NULL) {
        mc_registry_block_view blocks;
        mc_registry_item_view items;
        mc_registry_translation_view translation;
        if (mc_registry_classes_inspect_blocks(owner, &blocks, NULL) != MC_REGISTRY_OK ||
            mc_registry_classes_inspect_items(owner, &items, NULL) != MC_REGISTRY_OK ||
            mc_registry_classes_inspect_translation(owner, &translation, NULL) != MC_REGISTRY_OK ||
            blocks.phase != MC_REGISTRY_COLD || blocks.initializer_pc != -1 ||
            blocks.named_blocks != 0 || blocks.m != NULL || blocks.n != NULL || blocks.o != NULL ||
            blocks.p != NULL || blocks.q != NULL || blocks.r != NULL || blocks.s != NULL ||
            blocks.t != NULL || items.phase != MC_REGISTRY_COLD || items.initializer_pc != -1 ||
            items.named_items != 0 || items.b != NULL || items.c != NULL ||
            translation.singleton_phase != MC_REGISTRY_COLD ||
            translation.wrapper_phase != MC_REGISTRY_COLD || translation.singleton_a != NULL ||
            translation.wrapper_a != NULL)
            return false;
    }
    return true;
}

static mc_registry_status allocate(mc_registry_side side, const dependencies *d, resource *r,
                                   size_t limit, mc_registry_classes **out)
{
    const mc_registry_options options = {side, d->profile, d->materials, limit,
                                         r == NULL ? NULL : &r->source};
    mc_registry_status status = mc_registry_classes_allocate(&options, out, NULL);
    if (status == MC_REGISTRY_OK && r != NULL)
        r->owner = *out;
    return status;
}

static int retire(mc_registry_classes *owner)
{
    if (owner == NULL || mc_registry_classes_free(owner, NULL) == MC_REGISTRY_OK)
        return 0;
    retirement_failed = true;
    fprintf(stderr, "class retirement refused; preserving borrowed dependencies\n");
    return 1;
}

static int test_cold(mc_registry_side side, const dependencies *d)
{
    int result = 1;
    resource r;
    resource_init(&r, valid_bytes, sizeof(valid_bytes) - 1, 6);
    mc_registry_classes *owner = NULL;
    CHECK(allocate(side, d, &r, 0, &owner) == MC_REGISTRY_OK);
    mc_registry_statistic_key_view view;
    CHECK(mc_registry_classes_inspect_statistic_keys(owner, &view, NULL) == MC_REGISTRY_OK);
    CHECK(view.phase == MC_REGISTRY_COLD && view.initializer_pc == -1 && view.a == NULL &&
          view.construction == NULL && !view.map_published && view.constructor_pc == -1 &&
          view.caught_status == MC_REGISTRY_OK && view.caught_pc == -1);
    mc_registry_statistic_key_view before = view;
    CHECK(mc_registry_classes_inspect_statistic_keys(NULL, &view, NULL) ==
          MC_REGISTRY_INVALID_ARGUMENT);
    CHECK(same_view(&view, &before));
    CHECK(mc_registry_classes_inspect_statistic_keys(owner, NULL, NULL) ==
          MC_REGISTRY_INVALID_ARGUMENT);
    CHECK(mc_registry_classes_initialize_statistic_keys(NULL, NULL) ==
          MC_REGISTRY_INVALID_ARGUMENT);
    CHECK(mc_registry_classes_statistic_key(owner, 1, NULL, NULL) == MC_REGISTRY_INVALID_ARGUMENT);
    CHECK(r.opens == 0 && r.reads == 0 && r.closes == 0 && mc_statistic_keys_size(NULL) == 0);
    CHECK(dependencies_unchanged(d, owner));
    result = 0;
cleanup:
    if (retire(owner) != 0)
        result = 1;
    return result;
}

static int check_ready(mc_registry_classes *owner, const resource *r)
{
    int result = 1;
    mc_registry_statistic_key_view view;
    mc_registry_error error;
    CHECK(mc_registry_classes_inspect_statistic_keys(owner, &view, &error) == MC_REGISTRY_OK);
    CHECK(view.phase == MC_REGISTRY_READY && view.initializer_pc == 10 && view.a != NULL &&
          view.map_published && view.constructor_pc == 94 && view.caught_status == MC_REGISTRY_OK &&
          view.caught_pc == -1);
    CHECK(view.a == r->construction);
    CHECK(mc_statistic_keys_size(view.a) == COUNT(valid_ids));
    const mc_registry_string *values[COUNT(valid_ids)] = {0};
    size_t bytes = owner->allocation_bytes, reads = r->reads;
    for (size_t i = 0; i < COUNT(valid_ids); ++i) {
        int32_t id = 12345;
        CHECK(mc_statistic_keys_entry(view.a, i, &id, &values[i]) == MC_REGISTRY_OK &&
              id == valid_ids[i]);
        const mc_registry_string *value = NULL;
        CHECK(mc_registry_classes_statistic_key(owner, valid_ids[i], &value, &error) ==
                  MC_REGISTRY_OK &&
              value == values[i]);
        CHECK(valid_values[i] == NULL ? units_are(value, nul_value, COUNT(nul_value))
                                      : text_is(value, valid_values[i]));
    }
    CHECK(values[4] == values[5]); /* actual empty substring returns literal "" */
    CHECK(values[6] != values[7]); /* nonempty equal substrings are new Strings */
    if (r->observe) {
        CHECK(r->old_value != NULL && r->old_value != values[0] && text_is(r->old_value, "old"));
        mc_registry_string_view old;
        CHECK(mc_registry_string_read(r->old_value, &old) && old.units == r->old_units);
    }
    const mc_registry_string *out = values[0];
    CHECK(mc_registry_classes_statistic_key(owner, 42, &out, &error) == MC_REGISTRY_OK &&
          out == NULL);
    out = values[0];
    CHECK(mc_registry_classes_statistic_key(NULL, 7, &out, &error) ==
              MC_REGISTRY_INVALID_ARGUMENT &&
          out == values[0]);
    int32_t id = 45678;
    CHECK(mc_statistic_keys_entry(NULL, 0, &id, &out) == MC_REGISTRY_INVALID_ARGUMENT);
    CHECK(id == 45678 && out == values[0]);
    CHECK(mc_statistic_keys_entry(view.a, COUNT(valid_ids), &id, &out) ==
          MC_REGISTRY_INDEX_OUT_OF_RANGE);
    CHECK(id == 45678 && out == values[0]);
    CHECK(mc_statistic_keys_entry(view.a, SIZE_MAX, &id, &out) == MC_REGISTRY_INDEX_OUT_OF_RANGE);
    CHECK(id == 45678 && out == values[0]);
    CHECK(mc_statistic_keys_entry(view.a, 0, NULL, &out) == MC_REGISTRY_INVALID_ARGUMENT &&
          out == values[0]);
    CHECK(mc_statistic_keys_entry(view.a, 0, &id, NULL) == MC_REGISTRY_INVALID_ARGUMENT &&
          id == 45678);
    CHECK(mc_registry_classes_initialize_statistic_keys(owner, &error) == MC_REGISTRY_OK);
    CHECK(r->opens == 1 && r->closes == 1 && r->closed && !r->invalid && r->reads == reads);
    CHECK(owner->allocation_bytes == bytes);
    result = 0;
cleanup:
    return result;
}

static int test_success(mc_registry_side side, const dependencies *d, size_t chunk, bool advertised)
{
    int result = 1;
    mc_registry_classes *a = NULL, *b = NULL;
    resource x, y;
    resource_init(&x, valid_bytes, sizeof(valid_bytes) - 1, chunk);
    resource_init(&y, valid_bytes, sizeof(valid_bytes) - 1, chunk);
    x.observe = chunk == 6 && !advertised;
    x.reenter = true;
    x.advertised = y.advertised = advertised;
    CHECK(allocate(side, d, &x, 0, &a) == MC_REGISTRY_OK);
    CHECK(allocate(side, d, &y, 0, &b) == MC_REGISTRY_OK);
    const mc_registry_string *first = NULL;
    CHECK(mc_registry_classes_statistic_key(a, 7, &first, NULL) == MC_REGISTRY_OK &&
          text_is(first, "new"));
    CHECK(check_ready(a, &x) == 0);
    CHECK(y.opens == 0 && y.reads == 0 && y.closes == 0);
    CHECK(mc_registry_classes_initialize_statistic_keys(b, NULL) == MC_REGISTRY_OK);
    CHECK(check_ready(b, &y) == 0 && x.construction != y.construction);
    const mc_registry_string *other = NULL;
    CHECK(mc_registry_classes_statistic_key(b, 3, &other, NULL) == MC_REGISTRY_OK);
    CHECK(other != entry_at(x.construction, 3) && text_is(other, "same"));
    CHECK(dependencies_unchanged(d, a) && dependencies_unchanged(d, b));
    CHECK(retire(a) == 0);
    a = NULL;
    CHECK(text_is(other, "same") && check_ready(b, &y) == 0);
    result = 0;
cleanup:
    if (retire(a) != 0)
        result = 1;
    if (retire(b) != 0)
        result = 1;
    return result;
}

static int check_retry(mc_registry_classes *owner, const resource *r)
{
    int result = 1;
    mc_registry_statistic_key_view before, after;
    CHECK(mc_registry_classes_inspect_statistic_keys(owner, &before, NULL) == MC_REGISTRY_OK);
    CHECK(before.phase == MC_REGISTRY_FAILED && before.a == NULL);
    size_t count = mc_statistic_keys_size(before.construction), bytes = owner->allocation_bytes;
    size_t reads = r->reads, closes = r->closes, opens = r->opens;
    const mc_registry_string *held[32] = {0};
    int32_t ids[32] = {0};
    CHECK(count <= COUNT(held));
    for (size_t i = 0; i < count; ++i)
        CHECK(mc_statistic_keys_entry(before.construction, i, &ids[i], &held[i]) == MC_REGISTRY_OK);
    mc_registry_error error;
    CHECK(mc_registry_classes_inspect_statistic_keys(NULL, &after, &error) ==
          MC_REGISTRY_INVALID_ARGUMENT);
    CHECK(mc_registry_classes_initialize_statistic_keys(owner, &error) ==
          MC_REGISTRY_FAILED_INITIALIZATION);
    CHECK(error.status == MC_REGISTRY_FAILED_INITIALIZATION);
    const mc_registry_string *out = count == 0 ? NULL : held[0];
    const mc_registry_string *sentinel = out;
    CHECK(mc_registry_classes_statistic_key(owner, 1, &out, &error) ==
              MC_REGISTRY_FAILED_INITIALIZATION &&
          out == sentinel);
    CHECK(mc_registry_classes_inspect_statistic_keys(owner, &after, NULL) == MC_REGISTRY_OK &&
          same_view(&before, &after));
    CHECK(owner->allocation_bytes == bytes && r->reads == reads && r->opens == opens &&
          r->closes == closes && !r->invalid);
    for (size_t i = 0; i < count; ++i) {
        int32_t id;
        const mc_registry_string *value = NULL;
        CHECK(mc_statistic_keys_entry(after.construction, i, &id, &value) == MC_REGISTRY_OK);
        CHECK(id == ids[i] && value == held[i]);
    }
    CHECK(owner->entered == 0 && owner->initializer == NULL);
    result = 0;
cleanup:
    return result;
}

static int test_empty_resource(mc_registry_side side, const dependencies *d)
{
    int result = 1;
    mc_registry_classes *owner = NULL;
    resource r;
    resource_init(&r, NULL, 0, 1);
    CHECK(allocate(side, d, &r, 0, &owner) == MC_REGISTRY_OK);
    CHECK(mc_registry_classes_initialize_statistic_keys(owner, NULL) == MC_REGISTRY_OK);
    mc_registry_statistic_key_view view;
    CHECK(mc_registry_classes_inspect_statistic_keys(owner, &view, NULL) == MC_REGISTRY_OK);
    CHECK(view.phase == MC_REGISTRY_READY && view.a == r.construction && view.map_published);
    CHECK(mc_statistic_keys_size(view.a) == 0 && r.opens == 1 && r.reads == 1 && r.closes == 1);
    const mc_registry_string *out = NULL;
    CHECK(mc_registry_classes_statistic_key(owner, 0, &out, NULL) == MC_REGISTRY_OK && out == NULL);
    CHECK(!r.invalid && dependencies_unchanged(d, owner));
    result = 0;
cleanup:
    if (retire(owner) != 0)
        result = 1;
    return result;
}

typedef struct failure_case {
    const char *line;
    mc_registry_status caught;
    int32_t pc;
} failure_case;

static const failure_case bad_lines[] = {{"nodelimiter", MC_REGISTRY_ILLEGAL_ARGUMENT, 56},
                                         {"12", MC_REGISTRY_INDEX_OUT_OF_RANGE, 72},
                                         {"", MC_REGISTRY_ILLEGAL_ARGUMENT, 56},
                                         {",", MC_REGISTRY_INDEX_OUT_OF_RANGE, 55},
                                         {",,", MC_REGISTRY_INDEX_OUT_OF_RANGE, 55},
                                         {"12,", MC_REGISTRY_INDEX_OUT_OF_RANGE, 72},
                                         {"12,,,", MC_REGISTRY_INDEX_OUT_OF_RANGE, 72},
                                         {",x", MC_REGISTRY_ILLEGAL_ARGUMENT, 56},
                                         {" 1,x", MC_REGISTRY_ILLEGAL_ARGUMENT, 56},
                                         {"1 ,x", MC_REGISTRY_ILLEGAL_ARGUMENT, 56},
                                         {"2147483648,x", MC_REGISTRY_ILLEGAL_ARGUMENT, 56},
                                         {"-2147483649,x", MC_REGISTRY_ILLEGAL_ARGUMENT, 56},
                                         {"+,x", MC_REGISTRY_ILLEGAL_ARGUMENT, 56},
                                         {"-,x", MC_REGISTRY_ILLEGAL_ARGUMENT, 56},
                                         {"++1,x", MC_REGISTRY_ILLEGAL_ARGUMENT, 56},
                                         {"0x10,x", MC_REGISTRY_ILLEGAL_ARGUMENT, 56}};

static int check_caught(mc_registry_classes *owner, const resource *r, mc_registry_status caught,
                        int32_t pc, size_t expected_entries, size_t expected_closes)
{
    int result = 1;
    mc_registry_statistic_key_view view;
    CHECK(mc_registry_classes_inspect_statistic_keys(owner, &view, NULL) == MC_REGISTRY_OK);
    CHECK(view.phase == MC_REGISTRY_FAILED && view.initializer_pc == 4 && view.a == NULL &&
          view.construction == r->construction && view.map_published && view.constructor_pc == 91 &&
          view.caught_status == caught && view.caught_pc == pc);
    CHECK(mc_statistic_keys_size(view.construction) == expected_entries);
    if (expected_entries != 0)
        CHECK(text_is(entry_at(view.construction, 10), "kept"));
    CHECK(r->opens == 1 && r->closes == expected_closes && !r->invalid);
    CHECK(check_retry(owner, r) == 0);
    result = 0;
cleanup:
    return result;
}

static int test_bad_line(mc_registry_side side, const dependencies *d, size_t index)
{
    int result = 1;
    mc_registry_classes *owner = NULL;
    uint8_t bytes[160];
    const char *prefix = "10,kept\n";
    size_t length = strlen(prefix), bad_length = strlen(bad_lines[index].line);
    memcpy(bytes, prefix, length);
    memcpy(bytes + length, bad_lines[index].line, bad_length);
    length += bad_length;
    memcpy(bytes + length, "\n11,notreached\n", 15);
    length += 15;
    resource r;
    resource_init(&r, bytes, length, 1);
    CHECK(allocate(side, d, &r, 0, &owner) == MC_REGISTRY_OK);
    mc_registry_error error;
    CHECK(mc_registry_classes_initialize_statistic_keys(owner, &error) ==
          MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY);
    CHECK(error.status == MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY && error.constructor_pc == 91);
    CHECK(error.initializer_pc == -1 && error.active_initializer_pc == 4 &&
          error.active_initializer_class != NULL &&
          strcmp(error.active_initializer_class, "b") == 0);
    CHECK(check_caught(owner, &r, bad_lines[index].caught, bad_lines[index].pc, 1, 0) == 0);
    CHECK(entry_at(r.construction, 11) == NULL && dependencies_unchanged(d, owner));
    result = 0;
cleanup:
    if (retire(owner) != 0)
        result = 1;
    return result;
}

enum transport_case { MISSING, READ_IO, CLOSE_IO, NO_CLOSE, NO_SOURCE };

static int test_transport(mc_registry_side side, const dependencies *d, enum transport_case which)
{
    int result = 1;
    mc_registry_classes *owner = NULL;
    static const uint8_t bytes[] = "10,kept\n11,notreached\n";
    resource r;
    resource_init(&r, bytes, sizeof(bytes) - 1, 8);
    r.missing = which == MISSING;
    r.fail_read = which == READ_IO ? 2 : 0;
    r.close_status = which == CLOSE_IO ? MC_REGISTRY_IO_FAILED : MC_REGISTRY_OK;
    if (which == NO_CLOSE)
        r.source.close = NULL;
    CHECK(allocate(side, d, which == NO_SOURCE ? NULL : &r, 0, &owner) == MC_REGISTRY_OK);
    CHECK(mc_registry_classes_initialize_statistic_keys(owner, NULL) ==
          MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY);
    if (which == MISSING) {
        CHECK(check_caught(owner, &r, MC_REGISTRY_NULL_REFERENCE, 30, 0, 0) == 0 && r.reads == 0);
    } else if (which == READ_IO) {
        CHECK(check_caught(owner, &r, MC_REGISTRY_IO_FAILED, 38, 1, 0) == 0 && r.reads == 2);
    } else if (which == CLOSE_IO) {
        CHECK(check_caught(owner, &r, MC_REGISTRY_IO_FAILED, 83, 2, 1) == 0 && r.closed);
    } else {
        mc_registry_statistic_key_view view;
        CHECK(mc_registry_classes_inspect_statistic_keys(owner, &view, NULL) == MC_REGISTRY_OK);
        CHECK(view.phase == MC_REGISTRY_FAILED && view.a == NULL && view.map_published &&
              view.constructor_pc == (which == NO_CLOSE ? 83 : 27) &&
              view.caught_status == MC_REGISTRY_OK && view.caught_pc == -1);
        CHECK(mc_statistic_keys_size(view.construction) == (which == NO_CLOSE ? 2 : 0));
        CHECK(r.closes == 0 && !r.closed && (which == NO_CLOSE ? r.opens == 1 : r.opens == 0));
        CHECK(check_retry(owner, &r) == 0);
    }
    CHECK(dependencies_unchanged(d, owner));
    result = 0;
cleanup:
    if (retire(owner) != 0)
        result = 1;
    return result;
}

typedef struct budget_view {
    bool allocated, construction, map, ready;
    size_t opens, entries, bytes;
} budget_view;

static int budget_trial(mc_registry_side side, const dependencies *d, size_t maximum,
                        budget_view *out)
{
    int result = 1;
    mc_registry_classes *owner = NULL;
    static const uint8_t bytes[] = "1,a\n2,b\n3,c\n";
    resource r;
    resource_init(&r, bytes, sizeof(bytes) - 1, 1);
    *out = (budget_view){0};
    mc_registry_status status = allocate(side, d, &r, maximum, &owner);
    if (status != MC_REGISTRY_OK) {
        CHECK(status == MC_REGISTRY_LIMIT && maximum < sizeof(mc_registry_classes) &&
              owner == NULL);
        CHECK(r.opens == 0 && r.reads == 0 && r.closes == 0);
    } else {
        out->allocated = true;
        status = mc_registry_classes_initialize_statistic_keys(owner, NULL);
        CHECK(status == MC_REGISTRY_OK || status == MC_REGISTRY_LIMIT);
        mc_registry_statistic_key_view view;
        CHECK(mc_registry_classes_inspect_statistic_keys(owner, &view, NULL) == MC_REGISTRY_OK);
        const mc_statistic_keys *keys = view.a == NULL ? view.construction : view.a;
        out->construction = keys != NULL;
        out->map = view.map_published;
        out->ready = status == MC_REGISTRY_OK;
        out->opens = r.opens;
        out->entries = mc_statistic_keys_size(keys);
        out->bytes = owner->allocation_bytes;
        CHECK(out->bytes <= maximum && out->entries <= 3 && !r.invalid);
        for (size_t i = 0; i < out->entries; ++i) {
            int32_t id = 0;
            const mc_registry_string *value = NULL;
            static const char *const values[] = {"a", "b", "c"};
            CHECK(mc_statistic_keys_entry(keys, i, &id, &value) == MC_REGISTRY_OK &&
                  id == (int32_t)i + 1);
            CHECK(text_is(value, values[i]));
        }
        if (status == MC_REGISTRY_OK) {
            CHECK(view.phase == MC_REGISTRY_READY && out->entries == 3 && r.closes == 1);
        } else {
            CHECK(view.phase == MC_REGISTRY_FAILED && view.a == NULL &&
                  view.caught_status == MC_REGISTRY_OK);
            CHECK(r.closes == 0 && check_retry(owner, &r) == 0);
        }
        CHECK(dependencies_unchanged(d, owner));
    }
    result = 0;
cleanup:
    if (retire(owner) != 0)
        result = 1;
    return result;
}

static int test_budgets(mc_registry_side side, const dependencies *d)
{
    int result = 1;
    size_t low = sizeof(mc_registry_classes), high = 4u * 1024u * 1024u;
    budget_view current;
    CHECK(budget_trial(side, d, high, &current) == 0 && current.ready);
    while (low < high) {
        size_t middle = low + (high - low) / 2;
        CHECK(budget_trial(side, d, middle, &current) == 0);
        if (current.ready)
            high = middle;
        else
            low = middle + 1;
    }
    CHECK(low > sizeof(mc_registry_classes));
    const size_t limits[] = {sizeof(mc_registry_classes) - 1,
                             sizeof(mc_registry_classes),
                             sizeof(mc_registry_classes) + (low - sizeof(mc_registry_classes)) / 2,
                             low - 1,
                             low,
                             low + 1};
    budget_view previous = {0};
    for (size_t i = 0; i < COUNT(limits); ++i) {
        CHECK(budget_trial(side, d, limits[i], &current) == 0);
        CHECK(current.ready == (limits[i] >= low));
        CHECK(current.allocated >= previous.allocated &&
              current.construction >= previous.construction && current.map >= previous.map &&
              current.opens >= previous.opens && current.entries >= previous.entries &&
              current.bytes >= previous.bytes);
        previous = current;
    }
    result = 0;
cleanup:
    return result;
}

static int test_side(mc_registry_side side, const dependencies *d)
{
    int result = 1;
    CHECK(test_cold(side, d) == 0 && test_empty_resource(side, d) == 0);
    const size_t chunks[] = {SIZE_MAX, 1, 6, 7, 8192};
    for (size_t i = 0; i < COUNT(chunks); ++i)
        CHECK(test_success(side, d, chunks[i], false) == 0);
    CHECK(test_success(side, d, 7, true) == 0);
    for (size_t i = 0; i < COUNT(bad_lines); ++i)
        CHECK(test_bad_line(side, d, i) == 0);
    for (int i = MISSING; i <= NO_SOURCE; ++i)
        CHECK(test_transport(side, d, (enum transport_case)i) == 0);
    CHECK(test_budgets(side, d) == 0);
    result = 0;
cleanup:
    return result;
}

int main(void)
{
    int result = 1;
    dependencies d = {0};
    mc_java_reader_charset charset;
    mc_java_reader_status status = mc_java_reader_default_charset(&charset);
    if (status != MC_JAVA_READER_OK)
        fprintf(stderr, "actual default charset unavailable: status %d; key tests cannot qualify\n",
                (int)status);
    CHECK(status == MC_JAVA_READER_OK);
    CHECK(charset == MC_JAVA_READER_UTF8 || charset == MC_JAVA_READER_WINDOWS1252 ||
          charset == MC_JAVA_READER_ISO8859_1 || charset == MC_JAVA_READER_US_ASCII);
    CHECK(mc_materials_create(&d.materials));
    CHECK(mc_java_random_profile_create(1024u * 1024u, &d.profile) == MC_JAVA_RANDOM_PROFILE_OK);
    d.uniquifier = atomic_load_explicit(&d.profile->uniquifier, memory_order_relaxed);
    d.bytes = mc_java_random_profile_allocation_bytes(d.profile);
    for (size_t i = 0; i < MC_MATERIAL_COUNT; ++i)
        d.material[i] = material_read(mc_materials_get(d.materials, (mc_material_id)i));
    CHECK(test_side(MC_REGISTRY_CLIENT, &d) == 0);
    CHECK(test_side(MC_REGISTRY_SERVER, &d) == 0);
    CHECK(dependencies_unchanged(&d, NULL) && !retirement_failed);
    result = 0;
cleanup:
    if (!retirement_failed) {
        mc_java_random_profile_free(d.profile);
        mc_materials_destroy(d.materials);
    }
    if (result == 0 && printf("Statistic keys native contract: %u checks passed\n", checks) < 0)
        result = 1;
    return result;
}
