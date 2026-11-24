#include "mc/core/java/reader.h"
#include "mc/resources/translation/translation.h"
#include "mc/statistics/keys/keys.h"
#include "mc/statistics/registry/registry.h"

#include "../../../src/block/registry/registry_internal.h"
#include "../../../src/core/java/random_profile_internal.h"
#include "../../../src/platform/text/locale.h"
#include "../../../src/statistics/registry/registry_internal.h"

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

/* Literal jl/gg f..B declaration order, not values read from production. */
static const int32_t ids[] = {1000, 1001, 1002, 1003, 1004, 1100, 2000, 2001,
                              2002, 2003, 2004, 2005, 2006, 2007, 2008, 2010,
                              2011, 2020, 2021, 2022, 2023, 2024, 2025};
static const char *const keys[] = {
    "stat.startGame",     "stat.createWorld",   "stat.loadWorld",   "stat.joinMultiplayer",
    "stat.leaveGame",     "stat.playOneMinute", "stat.walkOneCm",   "stat.swimOneCm",
    "stat.fallOneCm",     "stat.climbOneCm",    "stat.flyOneCm",    "stat.diveOneCm",
    "stat.minecartOneCm", "stat.boatOneCm",     "stat.pigOneCm",    "stat.jump",
    "stat.drop",          "stat.damageDealt",   "stat.damageTaken", "stat.deaths",
    "stat.mobKills",      "stat.playerKills",   "stat.fishCaught"};
static const uint8_t english[] =
    "stat.startGame=Start\nstat.createWorld=Same\nstat.loadWorld=Same\n"
    "stat.playOneMinute=Clock\nstat.walkOneCm=Steps\nstat.jump=\nunit.name=Q\\u0000\\ud800\n";
static const uint8_t statistics[] = "stat.startGame=Started\nstat.damageDealt=Damage\n";
static const uint8_t key_bytes[] = "1000,key.start\n1001,key.world\n2025,key.fish\n";

static bool units_are(const mc_registry_string *string, const uint16_t *units, size_t length) {
    mc_registry_string_view view;
    if (!mc_registry_string_read(string, &view) || view.length != length ||
        (length != 0 && view.units == NULL))
        return false;
    for (size_t i = 0; i < length; ++i)
        if (view.units[i] != units[i])
            return false;
    return true;
}

static bool text_is(const mc_registry_string *string, const char *text) {
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

static mc_registry_status property(const mc_translation *translation, const char *key,
                                   const mc_registry_string **out) {
    uint16_t units[80];
    size_t length = strlen(key);
    if (length > COUNT(units))
        return MC_REGISTRY_INVALID_ARGUMENT;
    for (size_t i = 0; i < length; ++i)
        units[i] = (unsigned char)key[i];
    return mc_translation_property(translation, units, length, out, NULL);
}

static bool same_error(const mc_registry_error *a, const mc_registry_error *b) {
    return a->status == b->status && a->message == b->message &&
           a->initializer_pc == b->initializer_pc && a->block_id == b->block_id &&
           a->constructor_class == b->constructor_class && a->constructor_pc == b->constructor_pc &&
           a->profile_status == b->profile_status &&
           a->active_initializer_class == b->active_initializer_class &&
           a->active_initializer_pc == b->active_initializer_pc &&
           a->item_argument == b->item_argument &&
           a->format_error.status == b->format_error.status &&
           a->format_error.offset == b->format_error.offset &&
           a->format_error.length == b->format_error.length &&
           a->format_error.value == b->format_error.value &&
           a->format_error.flags == b->format_error.flags &&
           a->format_error.conversion == b->format_error.conversion &&
           a->format_error.date_time == b->format_error.date_time;
}

static bool same_base(const mc_registry_statistic_base_view *a,
                      const mc_registry_statistic_base_view *b) {
    return a->phase == b->phase && a->initializer_pc == b->initializer_pc && a->b == b->b &&
           a->c == b->c && a->i == b->i && a->j == b->j && a->k == b->k;
}

typedef struct byte_stream {
    const uint8_t *bytes;
    size_t length, offset, chunk, opens, reads, closes, fail_read;
    bool closed;
} byte_stream;

typedef struct resources {
    byte_stream stream[3];
    mc_registry_resources source;
    mc_registry_classes *owner;
    int missing;
    bool invalid, observe_first;
    size_t first_observations;
    const mc_statistic *first;
} resources;

static bool first_registration(resources *r) {
    mc_registry_statistic_view view;
    mc_registry_statistic_key_view key;
    mc_registry_error error;
    if (mc_registry_classes_inspect_statistics(r->owner, &view, &error) != MC_REGISTRY_OK ||
        mc_registry_classes_inspect_statistic_keys(r->owner, &key, &error) != MC_REGISTRY_OK ||
        view.phase != MC_REGISTRY_INITIALIZING || view.initializer_pc != 68 || view.map_size != 1 ||
        view.named == NULL || view.named[0] != NULL || mc_statistic_list_size(view.b) != 1 ||
        mc_statistic_list_size(view.c) != 0 || key.phase != MC_REGISTRY_INITIALIZING ||
        key.a != NULL)
        return false;
    const mc_statistic *found = NULL, *listed = NULL;
    if (mc_registry_classes_find_statistic(r->owner, 1000, &found) != MC_REGISTRY_OK ||
        found == NULL || mc_statistic_list_get(view.b, 0, &listed) != MC_REGISTRY_OK ||
        listed != found)
        return false;
    const mc_statistic_fields *fields = mc_statistic_fields_get(found);
    if (fields == NULL || fields->e != 1000 || !fields->g || fields->h != NULL ||
        !text_is(fields->f, "Started") || mc_statistic_kind_get(found) != MC_STATISTIC_GENERAL)
        return false;
    if (r->first != NULL && r->first != found)
        return false;
    r->first = found;
    size_t bytes = r->owner->allocation_bytes;
    if (mc_registry_classes_initialize_statistics(r->owner, &error) != MC_REGISTRY_BUSY ||
        mc_registry_classes_free(r->owner, &error) != MC_REGISTRY_BUSY ||
        r->owner->allocation_bytes != bytes)
        return false;
    ++r->first_observations;
    return true;
}

static mc_java_properties_status read_bytes(void *context, uint8_t *out, size_t capacity,
                                            size_t *count) {
    byte_stream *s = context;
    ++s->reads;
    if (out == NULL || count == NULL || capacity == 0 || capacity > 8192 || s->offset > s->length)
        return MC_JAVA_PROPERTIES_IO_ERROR;
    *count = 0;
    if (s->closed || (s->fail_read != 0 && s->reads == s->fail_read))
        return MC_JAVA_PROPERTIES_IO_ERROR;
    size_t take = s->length - s->offset;
    if (take > s->chunk)
        take = s->chunk;
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
    resources *r = context;
    static const char *const names[] = {"/lang/en_US.lang", "/lang/stats_US.lang",
                                        "/achievement/map.txt"};
    size_t index = COUNT(names);
    if (name != NULL)
        for (size_t i = 0; i < COUNT(names); ++i)
            if (strcmp(name, names[i]) == 0)
                index = i;
    if (index == COUNT(names) || out == NULL) {
        r->invalid = true;
        return MC_REGISTRY_INVALID_ARGUMENT;
    }
    byte_stream *s = &r->stream[index];
    if (++s->opens != 1 || (index == 2 && r->observe_first && !first_registration(r)))
        r->invalid = true;
    if ((int)index == r->missing)
        return MC_REGISTRY_NULL_REFERENCE;
    *out = (mc_java_properties_input){s, read_bytes};
    return MC_REGISTRY_OK;
}

static mc_registry_status close_resource(void *context, const mc_java_properties_input *input) {
    resources *r = context;
    if (input == NULL || input->context != &r->stream[2] || input->read != read_bytes) {
        r->invalid = true;
        return MC_REGISTRY_INVALID_ARGUMENT;
    }
    ++r->stream[2].closes;
    r->stream[2].closed = true;
    return MC_REGISTRY_OK;
}

static bool ready_resource(void *context, const mc_java_properties_input *input) {
    resources *r = context;
    if (input == NULL || input->read != read_bytes)
        r->invalid = true;
    return false;
}

static void resources_init(resources *r) {
    *r = (resources){.missing = -1};
    r->stream[0] = (byte_stream){.bytes = english, .length = sizeof(english) - 1, .chunk = 7};
    r->stream[1] = (byte_stream){.bytes = statistics, .length = sizeof(statistics) - 1, .chunk = 7};
    r->stream[2] = (byte_stream){.bytes = key_bytes, .length = sizeof(key_bytes) - 1, .chunk = 15};
    r->source = (mc_registry_resources){r, open_resource, close_resource, ready_resource};
}

typedef struct dependencies {
    mc_materials *materials;
    mc_java_random_profile *profile;
    uint64_t uniquifier;
    size_t bytes;
    mc_material *material[MC_MATERIAL_COUNT];
    bool material_e[MC_MATERIAL_COUNT], material_i[MC_MATERIAL_COUNT];
    mc_text_number_locale locale;
} dependencies;

static uint64_t advanced(uint64_t value, size_t count) {
    while (count-- != 0)
        value *= UINT64_C(1181783497276652981);
    return value;
}

static mc_java_random *random_at(const mc_registry_classes *owner, size_t id) {
    if (owner->blocks.m == NULL || id >= owner->blocks.m_length || owner->blocks.m[id] == NULL)
        return NULL;
    if (id == 23)
        return owner->blocks.m[id]->subtype.xq.a;
    if (id == 54)
        return owner->blocks.m[id]->subtype.e.a;
    return owner->blocks.m[id]->subtype.tc.a;
}

static size_t random_owners(const mc_registry_classes *owner, mc_java_random **out) {
    size_t count = 0;
    if (owner != NULL) {
        static const size_t block_ids[] = {23, 54, 61, 62};
        for (size_t i = 0; i < COUNT(block_ids); ++i) {
            mc_java_random *random = random_at(owner, block_ids[i]);
            if (random != NULL)
                out[count++] = random;
        }
        if (owner->items.b != NULL)
            out[count++] = owner->items.b;
    }
    return count;
}

static bool same_random(const mc_java_random *a, const mc_java_random *b) {
    uint64_t left, right;
    memcpy(&left, &a->next_next_gaussian, sizeof(left));
    memcpy(&right, &b->next_next_gaussian, sizeof(right));
    return a->state == b->state && left == right &&
           a->have_next_next_gaussian == b->have_next_next_gaussian;
}

static bool dependencies_cold(const mc_registry_classes *owner) {
    mc_registry_block_view block;
    mc_registry_item_view item;
    return mc_registry_classes_inspect_blocks(owner, &block, NULL) == MC_REGISTRY_OK &&
           mc_registry_classes_inspect_items(owner, &item, NULL) == MC_REGISTRY_OK &&
           block.phase == MC_REGISTRY_COLD && block.named_blocks == 0 && block.m == NULL &&
           owner->blocks.m_length == 0 && item.phase == MC_REGISTRY_COLD && item.named_items == 0 &&
           item.b == NULL && item.c == NULL;
}

static bool dependencies_unchanged(const dependencies *d, const mc_registry_classes *owner) {
    mc_java_random *randoms[5] = {0};
    size_t count = random_owners(owner, randoms);
    if (atomic_load_explicit(&d->profile->uniquifier, memory_order_relaxed) !=
            advanced(d->uniquifier, count) ||
        atomic_load_explicit(&d->profile->holder_phase, memory_order_relaxed) !=
            MC_JAVA_RANDOM_HOLDER_COLD ||
        d->profile->math != NULL || mc_java_random_profile_allocation_bytes(d->profile) != d->bytes)
        return false;
    for (size_t i = 0; i < count; ++i) {
        uint64_t cache;
        memcpy(&cache, &randoms[i]->next_next_gaussian, sizeof(cache));
        if (randoms[i]->state > UINT64_C(0xffffffffffff) || cache != 0 ||
            randoms[i]->have_next_next_gaussian)
            return false;
        for (size_t j = 0; j < i; ++j)
            if (randoms[i] == randoms[j])
                return false;
    }
    for (size_t i = 0; i < MC_MATERIAL_COUNT; ++i) {
        mc_material *m = mc_materials_get(d->materials, (mc_material_id)i);
        if (m != d->material[i] || mc_material_e(m) != d->material_e[i] ||
            mc_material_i(m) != d->material_i[i])
            return false;
    }
    if (owner != NULL) {
        mc_registry_block_view block;
        mc_registry_item_view item;
        if (mc_registry_classes_inspect_blocks(owner, &block, NULL) != MC_REGISTRY_OK ||
            mc_registry_classes_inspect_items(owner, &item, NULL) != MC_REGISTRY_OK ||
            block.phase == MC_REGISTRY_INITIALIZING || item.phase == MC_REGISTRY_INITIALIZING)
            return false;
        if (block.phase == MC_REGISTRY_COLD &&
            (block.named_blocks != 0 || block.m != NULL || owner->blocks.m_length != 0))
            return false;
        if (item.phase == MC_REGISTRY_COLD &&
            (item.named_items != 0 || item.b != NULL || item.c != NULL))
            return false;
    }
    return true;
}

/* Move the next owner's baseline only after checking the real publications,
 * exact wrapped U advance and untouched Math/material dependencies. */
static bool accept_dependencies(dependencies *d, const mc_registry_classes *owner) {
    if (!dependencies_unchanged(d, owner))
        return false;
    d->uniquifier = atomic_load_explicit(&d->profile->uniquifier, memory_order_relaxed);
    return true;
}

static mc_registry_status allocate(mc_registry_side side, const dependencies *d, resources *r,
                                   size_t limit, mc_registry_classes **out) {
    const mc_registry_options options = {side, d->profile, d->materials, limit,
                                         r == NULL ? NULL : &r->source};
    mc_registry_status status = mc_registry_classes_allocate(&options, out, NULL);
    if (status == MC_REGISTRY_OK && r != NULL)
        r->owner = *out;
    return status;
}

static int retire(mc_registry_classes *owner) {
    if (owner == NULL || mc_registry_classes_free(owner, NULL) == MC_REGISTRY_OK)
        return 0;
    retirement_failed = true;
    fprintf(stderr, "class retirement refused; preserving borrowed dependencies\n");
    return 1;
}

static int check_number(const mc_statistic_number_format *format, bool integer_us,
                        mc_text_number_locale locale) {
    int result = 1;
    const mc_statistic_number_format_fields *f = mc_statistic_number_format_fields_get(format);
    CHECK(f != NULL);
    CHECK(f->locale == (integer_us || locale == MC_TEXT_NUMBER_LOCALE_EN_US
                            ? MC_STATISTIC_NUMBER_LOCALE_EN_US
                            : MC_STATISTIC_NUMBER_LOCALE_EN));
    CHECK(f->grouping_used == integer_us && f->parse_integer_only == integer_us);
    CHECK(f->maximum_integer_digits == INT32_MAX && f->minimum_integer_digits == 1);
    CHECK(f->maximum_fraction_digits == (integer_us ? 0 : 2) &&
          f->minimum_fraction_digits == (integer_us ? 0 : 2));
    CHECK(f->grouping_size == (integer_us ? 3 : 0) && f->multiplier == 1 && f->half_even);
    CHECK(!f->decimal_separator_always_shown && !f->parse_big_decimal);
    CHECK(f->zero_digit == 0x30 && f->decimal_separator == 0x2e && f->grouping_separator == 0x2c &&
          f->minus_sign == 0x2d);
    CHECK(text_is(f->pattern, integer_us ? "#,##0" : "#0.00"));
    CHECK(text_is(f->positive_prefix, "") && text_is(f->positive_suffix, "") &&
          text_is(f->negative_prefix, "-") && text_is(f->negative_suffix, "") &&
          text_is(f->nan, "NaN"));
    const uint16_t infinity[] = {0x221e};
    CHECK(units_are(f->infinity, infinity, COUNT(infinity)));
    result = 0;
cleanup:
    return result;
}

static int check_base(mc_registry_classes *owner, const dependencies *d) {
    int result = 1;
    mc_registry_statistic_base_view view;
    CHECK(mc_registry_classes_inspect_statistic_base(owner, &view, NULL) == MC_REGISTRY_OK);
    CHECK(view.phase == MC_REGISTRY_READY && view.initializer_pc == 51);
    CHECK(view.b != NULL && view.c != NULL && view.b != view.c && view.i != NULL &&
          view.j != NULL && view.k != NULL);
    CHECK(view.i != view.j && view.i != view.k && view.j != view.k);
    const mc_statistic_formatter *formatters[] = {view.i, view.j, view.k};
    for (size_t i = 0; i < COUNT(formatters); ++i) {
        mc_statistic_formatter_kind kind = MC_STATISTIC_FORMAT_DISTANCE;
        CHECK(mc_statistic_formatter_kind_get(formatters[i], &kind) == MC_REGISTRY_OK &&
              kind == (mc_statistic_formatter_kind)i);
    }
    CHECK(check_number(view.b, true, d->locale) == 0 &&
          check_number(view.c, false, d->locale) == 0);
    size_t bytes = owner->allocation_bytes;
    CHECK(mc_registry_classes_initialize_statistic_base(owner, NULL) == MC_REGISTRY_OK);
    mc_registry_statistic_base_view after;
    CHECK(mc_registry_classes_inspect_statistic_base(owner, &after, NULL) == MC_REGISTRY_OK &&
          same_base(&view, &after));
    CHECK(owner->allocation_bytes == bytes && dependencies_unchanged(d, owner));
    result = 0;
cleanup:
    return result;
}

static int test_base(mc_registry_side side, const dependencies *d) {
    int result = 1;
    mc_registry_classes *owner = NULL;
    resources r;
    resources_init(&r);
    CHECK(allocate(side, d, &r, 0, &owner) == MC_REGISTRY_OK);
    mc_registry_statistic_base_view view;
    CHECK(mc_registry_classes_inspect_statistic_base(owner, &view, NULL) == MC_REGISTRY_OK);
    CHECK(view.phase == MC_REGISTRY_COLD && view.initializer_pc == -1 && view.b == NULL &&
          view.c == NULL && view.i == NULL && view.j == NULL && view.k == NULL);
    mc_registry_statistic_base_view saved = view;
    CHECK(mc_registry_classes_inspect_statistic_base(NULL, &view, NULL) ==
              MC_REGISTRY_INVALID_ARGUMENT &&
          same_base(&saved, &view));
    CHECK(mc_registry_classes_initialize_statistic_base(NULL, NULL) ==
          MC_REGISTRY_INVALID_ARGUMENT);
    CHECK(mc_registry_classes_initialize_statistic_base(owner, NULL) == MC_REGISTRY_OK &&
          check_base(owner, d) == 0 && dependencies_cold(owner));
    mc_registry_statistic_view stats;
    CHECK(mc_registry_classes_inspect_statistics(owner, &stats, NULL) == MC_REGISTRY_OK);
    CHECK(stats.phase == MC_REGISTRY_COLD && stats.initializer_pc == -1 && stats.map_size == 0 &&
          stats.b == NULL && stats.c == NULL && stats.d == NULL && stats.e == NULL);
    for (size_t i = 0; i < COUNT(r.stream); ++i)
        CHECK(r.stream[i].opens == 0 && r.stream[i].reads == 0 && r.stream[i].closes == 0);
    mc_statistic_formatter_kind kind = MC_STATISTIC_FORMAT_DISTANCE;
    CHECK(mc_statistic_formatter_kind_get(NULL, &kind) == MC_REGISTRY_INVALID_ARGUMENT &&
          kind == MC_STATISTIC_FORMAT_DISTANCE);
    CHECK(mc_statistic_formatter_kind_get(owner->statistic_base.i, NULL) ==
          MC_REGISTRY_INVALID_ARGUMENT);
    CHECK(mc_statistic_number_format_fields_get(NULL) == NULL &&
          mc_statistic_fields_get(NULL) == NULL);
    result = 0;
cleanup:
    if (retire(owner) != 0)
        result = 1;
    return result;
}

static int check_retry(mc_registry_classes *owner, const resources *r) {
    int result = 1;
    mc_registry_statistic_view before, after;
    CHECK(mc_registry_classes_inspect_statistics(owner, &before, NULL) == MC_REGISTRY_OK &&
          (before.phase == MC_REGISTRY_FAILED || before.phase == MC_REGISTRY_READY));
    const bool failed = before.phase == MC_REGISTRY_FAILED;
    const mc_registry_error failure = owner->statistics_state.failure;
    size_t bytes = owner->allocation_bytes;
    const uint64_t u =
        atomic_load_explicit(&owner->options.random_profile->uniquifier, memory_order_relaxed);
    const size_t profile_bytes =
        mc_java_random_profile_allocation_bytes(owner->options.random_profile);
    mc_java_random *randoms[5] = {0}, *after_randoms[5] = {0};
    mc_java_random saved_randoms[5];
    const size_t random_count = random_owners(owner, randoms);
    for (size_t i = 0; i < random_count; ++i)
        saved_randoms[i] = *randoms[i];
    const size_t sizes[] = {mc_statistic_list_size(before.b), mc_statistic_list_size(before.c),
                            mc_statistic_list_size(before.d), mc_statistic_list_size(before.e)};
    size_t reads[3], opens[3], closes[3];
    const mc_statistic *named[23] = {0};
    for (size_t i = 0; i < COUNT(named); ++i)
        named[i] = before.named[i];
    for (size_t i = 0; i < COUNT(reads); ++i) {
        reads[i] = r->stream[i].reads;
        opens[i] = r->stream[i].opens;
        closes[i] = r->stream[i].closes;
    }
    mc_registry_error error;
    CHECK(mc_registry_classes_inspect_statistics(NULL, &after, &error) ==
          MC_REGISTRY_INVALID_ARGUMENT);
    CHECK(mc_registry_classes_initialize_statistics(owner, &error) ==
          (failed ? MC_REGISTRY_FAILED_INITIALIZATION : MC_REGISTRY_OK));
    if (failed) {
        mc_registry_error expected_error = failure;
        expected_error.status = MC_REGISTRY_FAILED_INITIALIZATION;
        CHECK(same_error(&error, &expected_error));
    } else {
        CHECK(error.status == MC_REGISTRY_OK);
    }
    CHECK(mc_registry_classes_inspect_statistics(owner, &after, NULL) == MC_REGISTRY_OK);
    CHECK(after.phase == before.phase && after.initializer_pc == before.initializer_pc &&
          after.map_size == before.map_size && after.b == before.b && after.c == before.c &&
          after.d == before.d && after.e == before.e && after.named == before.named &&
          after.C == before.C && after.D == before.D && after.E == before.E &&
          after.F == before.F && after.G == before.G && after.H == before.H);
    CHECK(mc_statistic_list_size(after.b) == sizes[0] &&
          mc_statistic_list_size(after.c) == sizes[1] &&
          mc_statistic_list_size(after.d) == sizes[2] &&
          mc_statistic_list_size(after.e) == sizes[3]);
    CHECK(same_error(&failure, &owner->statistics_state.failure) &&
          owner->allocation_bytes == bytes);
    CHECK(atomic_load_explicit(&owner->options.random_profile->uniquifier, memory_order_relaxed) ==
              u &&
          mc_java_random_profile_allocation_bytes(owner->options.random_profile) == profile_bytes);
    CHECK(random_owners(owner, after_randoms) == random_count);
    for (size_t i = 0; i < random_count; ++i)
        CHECK(after_randoms[i] == randoms[i] && same_random(&saved_randoms[i], after_randoms[i]));
    for (size_t i = 0; i < COUNT(named); ++i)
        CHECK(after.named[i] == named[i]);
    for (size_t i = 0; i < COUNT(reads); ++i)
        CHECK(r->stream[i].reads == reads[i] && r->stream[i].opens == opens[i] &&
              r->stream[i].closes == closes[i]);
    CHECK(!r->invalid && owner->entered == 0 && owner->initializer == NULL);
    result = 0;
cleanup:
    return result;
}

static int check_prefix(mc_registry_classes *owner, size_t *published) {
    int result = 1;
    mc_registry_statistic_view view;
    CHECK(mc_registry_classes_inspect_statistics(owner, &view, NULL) == MC_REGISTRY_OK);
    size_t b = mc_statistic_list_size(view.b), c = mc_statistic_list_size(view.c), n = 0;
    for (size_t i = 0; i < COUNT(ids); ++i) {
        if (view.named[i] != NULL) {
            CHECK(i == n);
            ++n;
        }
    }
    if (n < COUNT(ids)) {
        /* Before all common publications, exactly the old constructor/register
         * prefix is reached; no Block/Item array helper has run. */
        CHECK(dependencies_cold(owner));
        CHECK(b <= COUNT(ids) && c <= b && view.map_size <= b && c <= view.map_size);
        CHECK(mc_statistic_list_size(view.d) == 0 && mc_statistic_list_size(view.e) == 0);
        CHECK(view.C == NULL && view.D == NULL && view.E == NULL && view.F == NULL && !view.G &&
              !view.H);
        CHECK(n <= c && b <= n + 1);
    } else {
        /* The real nested helpers can append many ITEM stats and remove list-b
         * aliases. They never remove the first23 GENERAL objects or ID cells. */
        CHECK(c == COUNT(ids) && b >= COUNT(ids) && view.map_size >= COUNT(ids));
        CHECK(b <= view.map_size + 1 && mc_statistic_list_size(view.d) <= view.map_size &&
              mc_statistic_list_size(view.e) <= view.map_size);
        /* For this real Stats-first path, nested Item/Block d constructs D
         * before Block returns and the outer helper can publish C. */
        if (view.D != NULL)
            CHECK(view.E != NULL && view.F != NULL);
        if (view.C != NULL)
            CHECK(view.D != NULL && view.E != NULL && view.F != NULL);
        if (view.G || view.H)
            CHECK(view.E != NULL && view.F != NULL);
    }
    const size_t common_b = b < COUNT(ids) ? b : COUNT(ids);
    for (size_t i = 0; i < common_b; ++i) {
        const mc_statistic *statistic = NULL, *found = NULL;
        CHECK(mc_statistic_list_get(view.b, i, &statistic) == MC_REGISTRY_OK && statistic != NULL);
        const mc_statistic_fields *fields = mc_statistic_fields_get(statistic);
        CHECK(fields != NULL && fields->e == ids[i] && fields->g == (i < 17));
        CHECK(mc_statistic_kind_get(statistic) == MC_STATISTIC_GENERAL);
        CHECK(mc_registry_classes_find_statistic(owner, ids[i], &found) == MC_REGISTRY_OK);
        CHECK(found == (i < view.map_size ? statistic : NULL));
        if (i < c) {
            const mc_statistic *general = NULL;
            CHECK(mc_statistic_list_get(view.c, i, &general) == MC_REGISTRY_OK &&
                  general == statistic);
        }
        if (i < n)
            CHECK(view.named[i] == statistic);
    }
    *published = n;
    result = 0;
cleanup:
    return result;
}

static bool completed_initialization(mc_registry_classes *owner) {
    mc_registry_statistic_view view;
    mc_registry_block_view block;
    mc_registry_item_view item;
    mc_java_random *randoms[5] = {0};
    if (mc_registry_classes_inspect_statistics(owner, &view, NULL) != MC_REGISTRY_OK ||
        mc_registry_classes_inspect_blocks(owner, &block, NULL) != MC_REGISTRY_OK ||
        mc_registry_classes_inspect_items(owner, &item, NULL) != MC_REGISTRY_OK)
        return false;
    /* Numeric sizes independently match both untouched-original Stats-first
     * bootstrap fixtures; synthetic display text does not choose registered IDs. */
    return view.phase == MC_REGISTRY_READY && view.initializer_pc == 635 && view.C != NULL &&
           view.D != NULL && view.E != NULL && view.F != NULL && !view.G && !view.H &&
           view.map_size == 486 && mc_statistic_list_size(view.b) == 467 &&
           mc_statistic_list_size(view.c) == COUNT(ids) && mc_statistic_list_size(view.d) == 106 &&
           mc_statistic_list_size(view.e) == 68 && view.duplicate_message == NULL &&
           block.phase == MC_REGISTRY_READY && block.initializer_pc == 3677 &&
           item.phase == MC_REGISTRY_READY && item.initializer_pc == 2667 &&
           owner->achievements_state.phase == MC_REGISTRY_READY &&
           owner->achievements_state.pc == 454 &&
           owner->crafting_state.phase == MC_REGISTRY_READY &&
           owner->smelting_state.phase == MC_REGISTRY_READY && owner->recipes.crafting != NULL &&
           owner->recipes.smelting != NULL && random_owners(owner, randoms) == 5;
}

static int test_common(mc_registry_side side, dependencies *d, int mode) {
    int result = 1;
    mc_registry_classes *owner = NULL;
    resources r;
    resources_init(&r);
    r.observe_first = mode != 1;
    r.missing = mode == 1 ? 0 : mode == 2 ? 2 : -1;
    if (mode == 3)
        r.stream[2].fail_read = 2;
    CHECK(allocate(side, d, &r, 0, &owner) == MC_REGISTRY_OK);
    mc_registry_error error;
    mc_registry_status expected = mode == 0   ? MC_REGISTRY_OK
                                  : mode == 1 ? MC_REGISTRY_NULL_REFERENCE
                                              : MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY;
    CHECK(mc_registry_classes_initialize_statistics(owner, &error) == expected);
    CHECK(error.status == expected && check_base(owner, d) == 0);
    size_t n = 0;
    CHECK(check_prefix(owner, &n) == 0);
    mc_registry_statistic_view view;
    CHECK(mc_registry_classes_inspect_statistics(owner, &view, NULL) == MC_REGISTRY_OK &&
          view.phase == (mode == 0 ? MC_REGISTRY_READY : MC_REGISTRY_FAILED));
    if (mode == 1) {
        CHECK(n == 0 && view.map_size == 0 && mc_statistic_list_size(view.b) == 0);
        CHECK(r.stream[0].opens == 1 && r.stream[1].opens == 0 && r.stream[2].opens == 0);
    } else {
        CHECK(r.first_observations != 0 && r.first != NULL && r.stream[0].opens == 1 &&
              r.stream[1].opens == 1 && r.stream[2].opens == 1);
        if (mode == 0) {
            CHECK(completed_initialization(owner));
            CHECK(n == COUNT(ids) && mc_statistic_list_size(view.b) >= COUNT(ids) &&
                  mc_statistic_list_size(view.c) == COUNT(ids));
            CHECK(view.b != view.c && view.b != view.d && view.b != view.e && view.c != view.d &&
                  view.c != view.e && view.d != view.e);
            mc_registry_statistic_base_view base;
            mc_registry_translation_view translation;
            mc_registry_statistic_key_view key;
            CHECK(mc_registry_classes_inspect_statistic_base(owner, &base, NULL) == MC_REGISTRY_OK);
            CHECK(mc_registry_classes_inspect_translation(owner, &translation, NULL) ==
                      MC_REGISTRY_OK &&
                  translation.wrapper_phase == MC_REGISTRY_READY);
            CHECK(mc_registry_classes_inspect_statistic_keys(owner, &key, NULL) == MC_REGISTRY_OK &&
                  key.phase == MC_REGISTRY_READY);
            for (size_t i = 0; i < COUNT(ids); ++i) {
                const mc_statistic_fields *fields = mc_statistic_fields_get(view.named[i]);
                CHECK(fields->a == (i == 5 ? base.j : i >= 6 && i <= 14 ? base.k : base.i));
                const mc_registry_string *display = NULL, *key_name = NULL;
                CHECK(property(translation.singleton_a, keys[i], &display) == MC_REGISTRY_OK);
                if (display == NULL)
                    CHECK(mc_registry_literal_ascii(owner, keys[i], &display) == MC_REGISTRY_OK);
                CHECK(fields->f == display);
                CHECK(mc_registry_classes_statistic_key(owner, ids[i], &key_name, NULL) ==
                          MC_REGISTRY_OK &&
                      fields->h == key_name);
            }
            CHECK(text_is(mc_statistic_fields_get(view.named[0])->f, "Started"));
            CHECK(mc_statistic_fields_get(view.named[1])->f !=
                  mc_statistic_fields_get(view.named[2])->f);
            CHECK(text_is(mc_statistic_fields_get(view.named[1])->f, "Same") &&
                  text_is(mc_statistic_fields_get(view.named[2])->f, "Same"));
            CHECK(mc_statistic_fields_get(view.named[3])->h == NULL &&
                  text_is(mc_statistic_fields_get(view.named[0])->h, "key.start"));
            CHECK(r.stream[2].closes == 1);
            const mc_registry_string *mine = NULL;
            size_t bytes = owner->allocation_bytes;
            CHECK(mc_registry_literal_ascii(owner, "stat.mineBlock", &mine) == MC_REGISTRY_OK &&
                  owner->allocation_bytes == bytes);
        } else {
            CHECK(n == 0 && view.map_size == 1 && mc_statistic_list_size(view.b) == 1 &&
                  mc_statistic_list_size(view.c) == 0);
            CHECK(mc_statistic_fields_get(r.first)->h == NULL && r.stream[2].closes == 0);
            CHECK(error.active_initializer_class != NULL &&
                  strcmp(error.active_initializer_class, "b") == 0 &&
                  error.constructor_class != NULL && strcmp(error.constructor_class, "b") == 0 &&
                  error.constructor_pc == 91);
            const mc_registry_error *saved = &owner->statistics_state.failure;
            CHECK(saved->active_initializer_class != NULL &&
                  strcmp(saved->active_initializer_class, "b") == 0 &&
                  saved->constructor_class != NULL && strcmp(saved->constructor_class, "b") == 0 &&
                  saved->constructor_pc == 91);
            mc_registry_statistic_key_view key;
            CHECK(mc_registry_classes_inspect_statistic_keys(owner, &key, NULL) == MC_REGISTRY_OK &&
                  key.phase == MC_REGISTRY_FAILED && key.a == NULL);
            CHECK(key.caught_status ==
                  (mode == 2 ? MC_REGISTRY_NULL_REFERENCE : MC_REGISTRY_IO_FAILED));
            CHECK(key.caught_pc == (mode == 2 ? 30 : 38));
            CHECK(mc_statistic_keys_size(key.construction) == (mode == 2 ? 0 : 1));
        }
    }
    CHECK(r.stream[0].closes == 0 && r.stream[1].closes == 0 && !r.invalid);
    CHECK(check_retry(owner, &r) == 0 && accept_dependencies(d, owner));
    result = 0;
cleanup:
    if (retire(owner) != 0)
        result = 1;
    return result;
}

/* Exact UTF16 duplicate message with a nonempty name containing NUL/surrogate.
 * Suffix trailing space comes from original CONSTANT_String, not javap text. */
static bool duplicate_units(const mc_registry_string *message) {
    const char *prefix = "Duplicate stat id: \"null\" and \"";
    const char *suffix = "\" at id -5";
    uint16_t expected[96];
    size_t length = 0;
    for (size_t i = 0; prefix[i] != 0; ++i)
        expected[length++] = (unsigned char)prefix[i];
    expected[length++] = 'Q';
    expected[length++] = 0;
    expected[length++] = 0xd800;
    for (size_t i = 0; suffix[i] != 0; ++i)
        expected[length++] = (unsigned char)suffix[i];
    return units_are(message, expected, length);
}

static int test_methods(mc_registry_side side, const dependencies *d, bool missing_key) {
    int result = 1;
    mc_registry_classes *owner = NULL;
    resources r;
    resources_init(&r);
    r.missing = missing_key ? 2 : -1;
    CHECK(allocate(side, d, &r, 0, &owner) == MC_REGISTRY_OK);
    CHECK(mc_registry_classes_initialize_statistic_base(owner, NULL) == MC_REGISTRY_OK);
    CHECK(mc_registry_classes_initialize_translation(owner, NULL) == MC_REGISTRY_OK);
    mc_registry_translation_view translation;
    CHECK(mc_registry_classes_inspect_translation(owner, &translation, NULL) == MC_REGISTRY_OK);
    const mc_registry_string *name = NULL;
    CHECK(property(translation.singleton_a, "unit.name", &name) == MC_REGISTRY_OK);
    const uint16_t name_units[] = {'Q', 0, 0xd800};
    CHECK(units_are(name, name_units, COUNT(name_units)));

    /* Isolated method precondition only: same-flow jl/gg INITIALIZING with
     * actual collections. Never claim a completed original class or READY. */
    owner->entered = 1;
    owner->statistics_state.phase = MC_REGISTRY_INITIALIZING;
    CHECK(mc_registry_statistic_collections(owner) == MC_REGISTRY_OK);
    mc_statistic *base = NULL, *general = NULL, *item = NULL;
    CHECK(mc_registry_statistic_allocate(owner, MC_STATISTIC_BASE, &base) == MC_REGISTRY_OK);
    CHECK(mc_registry_statistic_construct(base, -5, NULL, NULL, 0) == MC_REGISTRY_OK);
    const mc_statistic_fields *fields = mc_statistic_fields_get(base);
    CHECK(fields->e == -5 && fields->f == NULL && fields->a == NULL && fields->h == NULL &&
          !fields->g);
    CHECK(mc_statistic_kind_get(base) == MC_STATISTIC_BASE);
    int32_t associated = 123;
    CHECK(mc_statistic_item_id(base, &associated) == MC_REGISTRY_INVALID_ARGUMENT &&
          associated == 123);
    if (missing_key) {
        CHECK(mc_registry_statistic_allocate(owner, MC_STATISTIC_GENERAL, &general) ==
              MC_REGISTRY_OK);
        CHECK(mc_registry_statistic_construct(general, 1000, name, owner->statistic_base.i, 0) ==
              MC_REGISTRY_OK);
        CHECK(mc_registry_statistic_register(general) == MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY);
        mc_registry_statistic_view view;
        CHECK(mc_registry_classes_inspect_statistics(owner, &view, NULL) == MC_REGISTRY_OK);
        CHECK(view.map_size == 1 && mc_statistic_list_size(view.b) == 1 &&
              mc_statistic_list_size(view.c) == 0);
        CHECK(mc_statistic_fields_get(general)->h == NULL);
    } else {
        CHECK(mc_registry_statistic_flag(base) == MC_REGISTRY_OK &&
              mc_registry_statistic_flag(base) == MC_REGISTRY_OK);
        CHECK(fields->g && fields->e == -5 && fields->f == NULL && fields->a == NULL);
        CHECK(mc_registry_statistic_register(base) == MC_REGISTRY_OK && fields->h == NULL);
        CHECK(mc_registry_statistic_register(base) == MC_REGISTRY_DUPLICATE_STATISTIC);
        const mc_registry_string *first_message = owner->statistics.duplicate_message;
        CHECK(text_is(first_message, "Duplicate stat id: \"null\" and \"null\" at id -5"));
        CHECK(mc_registry_statistic_allocate(owner, MC_STATISTIC_GENERAL, &general) ==
              MC_REGISTRY_OK);
        CHECK(mc_registry_statistic_construct(general, -5, name, owner->statistic_base.j, 0) ==
              MC_REGISTRY_OK);
        CHECK(mc_registry_statistic_register(general) == MC_REGISTRY_DUPLICATE_STATISTIC);
        CHECK(owner->statistics.duplicate_message != first_message &&
              duplicate_units(owner->statistics.duplicate_message));
        CHECK(text_is(first_message, "Duplicate stat id: \"null\" and \"null\" at id -5"));
        mc_statistic *different = NULL;
        CHECK(mc_registry_statistic_allocate(owner, MC_STATISTIC_GENERAL, &different) ==
              MC_REGISTRY_OK);
        CHECK(mc_registry_statistic_construct(different, -6, name, owner->statistic_base.j, 0) ==
              MC_REGISTRY_OK);
        CHECK(mc_registry_statistic_register(different) == MC_REGISTRY_OK);
        CHECK(mc_statistic_fields_get(different)->f == name &&
              mc_statistic_fields_get(different)->a == owner->statistic_base.j);
        CHECK(mc_registry_statistic_allocate(owner, MC_STATISTIC_ITEM, &item) == MC_REGISTRY_OK);
        CHECK(mc_registry_statistic_construct(item, INT32_MIN, name, owner->statistic_base.i,
                                              -42) == MC_REGISTRY_OK);
        CHECK(mc_statistic_item_id(item, &associated) == MC_REGISTRY_OK && associated == -42);
        CHECK(mc_statistic_kind_get(item) == MC_STATISTIC_ITEM &&
              mc_statistic_fields_get(item)->e == INT32_MIN);
        CHECK(mc_registry_statistic_register(item) == MC_REGISTRY_OK);
        mc_registry_statistic_view view;
        CHECK(mc_registry_classes_inspect_statistics(owner, &view, NULL) == MC_REGISTRY_OK);
        CHECK(view.phase == MC_REGISTRY_INITIALIZING && view.map_size == 3 &&
              mc_statistic_list_size(view.b) == 3 && mc_statistic_list_size(view.c) == 1);
        const mc_statistic *out = NULL;
        CHECK(mc_statistic_list_get(view.b, 0, &out) == MC_REGISTRY_OK && out == base);
        CHECK(mc_statistic_list_get(view.b, 1, &out) == MC_REGISTRY_OK && out == different);
        CHECK(mc_statistic_list_get(view.b, 2, &out) == MC_REGISTRY_OK && out == item);
        CHECK(mc_statistic_list_get(view.c, 0, &out) == MC_REGISTRY_OK && out == different);
        CHECK(mc_registry_classes_find_statistic(owner, -5, &out) == MC_REGISTRY_OK && out == base);
        CHECK(mc_registry_classes_find_statistic(owner, 42, &out) == MC_REGISTRY_OK && out == NULL);
        out = base;
        CHECK(mc_registry_classes_find_statistic(NULL, -5, &out) == MC_REGISTRY_INVALID_ARGUMENT &&
              out == base);
        CHECK(mc_statistic_list_get(view.b, 3, &out) == MC_REGISTRY_INDEX_OUT_OF_RANGE &&
              out == base);
        CHECK(mc_statistic_list_get(NULL, 0, &out) == MC_REGISTRY_INVALID_ARGUMENT && out == base);
        CHECK(mc_statistic_list_get(view.b, 0, NULL) == MC_REGISTRY_INVALID_ARGUMENT);
        CHECK(mc_statistic_item_id(item, NULL) == MC_REGISTRY_INVALID_ARGUMENT);
        CHECK(mc_statistic_list_size(view.d) == 0 && mc_statistic_list_size(view.e) == 0);
    }
    owner->entered = 0;
    CHECK(!r.invalid && dependencies_cold(owner) && dependencies_unchanged(d, owner));
    result = 0;
cleanup:
    if (owner != NULL)
        owner->entered = 0; /* retire only the explicit isolated-method admission */
    if (retire(owner) != 0)
        result = 1;
    return result;
}

typedef struct budget_result {
    size_t bytes, named, map, b, c;
    bool end;
} budget_result;

static int budget_trial(mc_registry_side side, dependencies *d, size_t maximum,
                        budget_result *out) {
    int result = 1;
    mc_registry_classes *owner = NULL;
    resources r;
    resources_init(&r);
    r.observe_first = true;
    *out = (budget_result){0};
    mc_registry_status status = allocate(side, d, &r, maximum, &owner);
    if (status != MC_REGISTRY_OK) {
        CHECK(status == MC_REGISTRY_LIMIT && maximum < sizeof(mc_registry_classes));
    } else {
        status = mc_registry_classes_initialize_statistics(owner, NULL);
        CHECK(status == MC_REGISTRY_LIMIT || status == MC_REGISTRY_OK);
        mc_registry_statistic_view view;
        CHECK(mc_registry_classes_inspect_statistics(owner, &view, NULL) == MC_REGISTRY_OK);
        CHECK(view.phase == (status == MC_REGISTRY_OK ? MC_REGISTRY_READY : MC_REGISTRY_FAILED) &&
              check_prefix(owner, &out->named) == 0);
        out->bytes = owner->allocation_bytes;
        out->map = view.map_size;
        out->b = mc_statistic_list_size(view.b);
        out->c = mc_statistic_list_size(view.c);
        out->end = status == MC_REGISTRY_OK;
        CHECK(out->bytes <= maximum);
        if (out->end)
            CHECK(completed_initialization(owner) && out->named == COUNT(ids));
        CHECK(check_retry(owner, &r) == 0 && accept_dependencies(d, owner));
    }
    result = 0;
cleanup:
    if (retire(owner) != 0)
        result = 1;
    return result;
}

static int test_budgets(mc_registry_side side, dependencies *d) {
    int result = 1;
    size_t low = sizeof(mc_registry_classes), high = 4u * 1024u * 1024u;
    budget_result current;
    CHECK(budget_trial(side, d, high, &current) == 0 && current.end);
    while (low < high) {
        size_t middle = low + (high - low) / 2;
        CHECK(budget_trial(side, d, middle, &current) == 0);
        if (current.end)
            high = middle;
        else
            low = middle + 1;
    }
    const size_t limits[] = {sizeof(mc_registry_classes) - 1,
                             sizeof(mc_registry_classes),
                             sizeof(mc_registry_classes) + (low - sizeof(mc_registry_classes)) / 2,
                             low - 1,
                             low,
                             low + 1};
    budget_result previous = {0};
    for (size_t i = 0; i < COUNT(limits); ++i) {
        CHECK(budget_trial(side, d, limits[i], &current) == 0);
        CHECK(current.end == (limits[i] >= low));
        CHECK(current.bytes >= previous.bytes && current.named >= previous.named &&
              current.map >= previous.map && current.c >= previous.c);
        /* List-b aliases are removed by real unification. Registration may
         * append one owner before its map publication, but never two. */
        CHECK(current.b <= current.map + 1 && current.c <= current.b);
        previous = current;
    }
    result = 0;
cleanup:
    return result;
}

int main(int argc, char **argv) {
    int result = 1;
    dependencies d = {0};
    CHECK(argc == 1 ||
          (argc == 2 && (strcmp(argv[1], "en_US") == 0 || strcmp(argv[1], "en") == 0)));
    CHECK(mc_text_number_locale_query(&d.locale) == MC_TEXT_LOCALE_OK);
    CHECK(d.locale == MC_TEXT_NUMBER_LOCALE_EN_US || d.locale == MC_TEXT_NUMBER_LOCALE_EN);
    if (argc == 2)
        CHECK(d.locale == (strcmp(argv[1], "en_US") == 0 ? MC_TEXT_NUMBER_LOCALE_EN_US
                                                         : MC_TEXT_NUMBER_LOCALE_EN));
    mc_java_reader_charset charset;
    CHECK(mc_java_reader_default_charset(&charset) == MC_JAVA_READER_OK);
    CHECK(charset >= MC_JAVA_READER_UTF8 && charset <= MC_JAVA_READER_US_ASCII);
    CHECK(mc_materials_create(&d.materials));
    CHECK(mc_java_random_profile_create(1024u * 1024u, &d.profile) == MC_JAVA_RANDOM_PROFILE_OK);
    d.uniquifier = atomic_load_explicit(&d.profile->uniquifier, memory_order_relaxed);
    d.bytes = mc_java_random_profile_allocation_bytes(d.profile);
    for (size_t i = 0; i < MC_MATERIAL_COUNT; ++i) {
        d.material[i] = mc_materials_get(d.materials, (mc_material_id)i);
        d.material_e[i] = mc_material_e(d.material[i]);
        d.material_i[i] = mc_material_i(d.material[i]);
    }
    for (int side = MC_REGISTRY_CLIENT; side <= MC_REGISTRY_SERVER; ++side) {
        CHECK(test_base((mc_registry_side)side, &d) == 0);
        for (int mode = 0; mode < 4; ++mode)
            CHECK(test_common((mc_registry_side)side, &d, mode) == 0);
        CHECK(test_methods((mc_registry_side)side, &d, false) == 0);
        CHECK(test_methods((mc_registry_side)side, &d, true) == 0);
        CHECK(test_budgets((mc_registry_side)side, &d) == 0);
    }
    CHECK(!retirement_failed && dependencies_unchanged(&d, NULL));
    result = 0;
cleanup:
    if (!retirement_failed) {
        mc_java_random_profile_free(d.profile);
        mc_materials_destroy(d.materials);
    }
    if (result == 0 && printf("Statistics native contract: %u checks passed\n", checks) < 0)
        result = 1;
    return result;
}
