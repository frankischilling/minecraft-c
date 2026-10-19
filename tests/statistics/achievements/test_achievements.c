#include "mc/resources/translation/translation.h"
#include "mc/statistics/achievements/achievements.h"

#include "../../../src/block/registry/registry_internal.h"
#include "../../../src/core/java/random_profile_internal.h"
#include "../../../src/item/registry/stacks_internal.h"
#include "../../../src/statistics/achievements/achievements_internal.h"

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

/* Literal paired ep/cy construction table. Icon IDs are read from the actual
 * supplied Item/Block owners, never guessed from these field selectors. */
static const char *const suffixes[] = {
    "openInventory", "mineWood",           "buildWorkBench", "buildPickaxe",
    "buildFurnace",  "acquireIron",        "buildHoe",       "makeBread",
    "bakeCake",      "buildBetterPickaxe", "cookFish",       "onARail",
    "buildSword",    "killEnemy",          "killCow",        "flyPig"};
static const int32_t xs[] = {0, 2, 4, 4, 3, 1, 2, -1, 0, 6, 2, 2, 6, 8, 7, 8};
static const int32_t ys[] = {0, 1, -1, 2, 4, 4, -3, -3, -5, 2, 6, 3, -1, -1, -3, -4};
static const int parents[] = {-1, 0, 1, 2, 3, 4, 2, 6, 6, 3, 4, 5, 2, 12, 12, 14};
static const char *const icons[] = {"aJ", "K", "az", "r",  "aD", "m",  "L",  "S",
                                    "aX", "v", "aT", "aH", "p",  "aV", "aD", "ay"};
static const bool block_icons[] = {false, true,  true,  false, true,  false, false, false,
                                   false, false, false, true,  false, false, false, false};
static const uint8_t english[] =
    "stat.mineBlock=Mine %1$s\nstat.useItem=Use %1$s\nstat.breakItem=Break %1$s\n"
    "achievement.openInventory=Inventory\nachievement.openInventory.desc=Inventory details\n"
    "achievement.unit=Display %q\nachievement.unit.desc=Details %q\nunit.suffix=x\\u0000\\ud800\n";
static const uint8_t keys[] = "5242880,key.first\n5242917,key.unit\n";

typedef struct stream {
    const uint8_t *bytes;
    size_t length, offset, opens, reads, closes;
    bool closed;
} stream;
typedef struct resources {
    stream streams[3];
    mc_registry_resources source;
    mc_registry_classes *owner;
    const mc_statistic *watch;
    size_t observed;
    bool missing_key, invalid;
} resources;

static bool key_prefix(resources *r)
{
    if (r->watch == NULL)
        return true;
    mc_registry_statistic_view stats;
    mc_registry_achievement_view achievements;
    const mc_statistic_fields *base = mc_statistic_fields_get(r->watch);
    const mc_statistic *found = NULL;
    if (base == NULL ||
        mc_registry_classes_inspect_statistics(r->owner, &stats, NULL) != MC_REGISTRY_OK ||
        mc_registry_classes_inspect_achievements(r->owner, &achievements, NULL) != MC_REGISTRY_OK ||
        mc_registry_classes_find_statistic(r->owner, base->e, &found) != MC_REGISTRY_OK ||
        found != r->watch || stats.phase != MC_REGISTRY_INITIALIZING ||
        achievements.phase != MC_REGISTRY_INITIALIZING || stats.map_size != 1 ||
        mc_statistic_list_size(stats.b) != 1 || mc_statistic_list_size(stats.c) != 0 ||
        mc_statistic_list_size(achievements.e) != 0 || base->h != NULL)
        return false;
    size_t bytes = r->owner->allocation_bytes;
    if (mc_registry_classes_initialize_achievements(r->owner, NULL) != MC_REGISTRY_BUSY ||
        mc_registry_classes_initialize_statistics(r->owner, NULL) != MC_REGISTRY_BUSY ||
        mc_registry_classes_free(r->owner, NULL) != MC_REGISTRY_BUSY ||
        r->owner->allocation_bytes != bytes)
        return false;
    ++r->observed;
    return true;
}

static mc_java_properties_status read_bytes(void *context, uint8_t *out, size_t capacity,
                                            size_t *count)
{
    stream *s = context;
    ++s->reads;
    if (out == NULL || count == NULL || capacity == 0 || s->closed || s->offset > s->length)
        return MC_JAVA_PROPERTIES_IO_ERROR;
    size_t take = s->length - s->offset;
    if (take > 7)
        take = 7;
    if (take > capacity)
        take = capacity;
    if (take != 0)
        memcpy(out, s->bytes + s->offset, take);
    s->offset += take;
    *count = take;
    return MC_JAVA_PROPERTIES_OK;
}

static mc_registry_status open_resource(void *context, const char *name,
                                        mc_java_properties_input *out)
{
    resources *r = context;
    const char *const names[] = {"/lang/en_US.lang", "/lang/stats_US.lang", "/achievement/map.txt"};
    for (size_t i = 0; i < COUNT(names); ++i)
        if (name != NULL && strcmp(name, names[i]) == 0 && out != NULL) {
            if (++r->streams[i].opens != 1 || (i == 2 && !key_prefix(r)))
                r->invalid = true;
            if (i == 2 && r->missing_key)
                return MC_REGISTRY_NULL_REFERENCE;
            *out = (mc_java_properties_input){&r->streams[i], read_bytes};
            return MC_REGISTRY_OK;
        }
    r->invalid = true;
    return MC_REGISTRY_INVALID_ARGUMENT;
}

static mc_registry_status close_resource(void *context, const mc_java_properties_input *input)
{
    resources *r = context;
    if (input == NULL || input->context != &r->streams[2] || input->read != read_bytes) {
        r->invalid = true;
        return MC_REGISTRY_INVALID_ARGUMENT;
    }
    ++r->streams[2].closes;
    r->streams[2].closed = true;
    return MC_REGISTRY_OK;
}

static bool ready_resource(void *context, const mc_java_properties_input *input)
{
    resources *r = context;
    if (input == NULL || input->read != read_bytes)
        r->invalid = true;
    return false;
}

static void resources_init(resources *r)
{
    *r = (resources){0};
    r->streams[0] = (stream){english, sizeof(english) - 1, 0, 0, 0, 0, false};
    r->streams[1] = (stream){english, 0, 0, 0, 0, 0, false};
    r->streams[2] = (stream){keys, sizeof(keys) - 1, 0, 0, 0, 0, false};
    r->source = (mc_registry_resources){r, open_resource, close_resource, ready_resource};
}

static mc_registry_status create(mc_registry_side side, mc_materials *materials,
                                 mc_java_random_profile *profile, size_t limit, resources *r,
                                 mc_registry_classes **out)
{
    const mc_registry_options options = {side, profile, materials, limit, &r->source};
    mc_registry_status status = mc_registry_classes_allocate(&options, out, NULL);
    if (status == MC_REGISTRY_OK)
        r->owner = *out;
    return status;
}

static int retire(mc_registry_classes *owner, bool admitted)
{
    if (owner != NULL && admitted)
        owner->entered = 0;
    if (mc_registry_classes_free(owner, NULL) == MC_REGISTRY_OK)
        return 0;
    retirement_failed = true;
    fputs("class retirement refused; preserving borrowed dependencies\n", stderr);
    return 1;
}

static uint64_t uniquifier(const mc_java_random_profile *p)
{
    return atomic_load_explicit(&p->uniquifier, memory_order_relaxed);
}

static uint64_t advanced(uint64_t u, size_t n)
{
    while (n-- != 0)
        u *= UINT64_C(1181783497276652981);
    return u;
}

static bool text_is(const mc_registry_string *value, const char *expected)
{
    mc_registry_string_view view;
    if (!mc_registry_string_read(value, &view) || view.length != strlen(expected))
        return false;
    for (size_t i = 0; i < view.length; ++i)
        if (view.units[i] != (unsigned char)expected[i])
            return false;
    return true;
}

static const mc_registry_string *literal(mc_registry_classes *owner, const char *value)
{
    const mc_registry_string *out = NULL;
    return mc_registry_literal_ascii(owner, value, &out) == MC_REGISTRY_OK ? out : NULL;
}

static const mc_registry_string *property(const mc_translation *translation, const char *name)
{
    uint16_t units[128];
    size_t n = strlen(name);
    if (n > COUNT(units))
        return NULL;
    for (size_t i = 0; i < n; ++i)
        units[i] = (unsigned char)name[i];
    const mc_registry_string *out = NULL;
    return mc_translation_property(translation, units, n, &out, NULL) == MC_REGISTRY_OK ? out
                                                                                        : NULL;
}

static bool same_fields(const mc_item_stack_fields *a, const mc_item_stack_fields *b)
{
    return a->id == b->id && a->count == b->count && a->animation == b->animation &&
           a->damage == b->damage;
}

static bool same_achievement(const mc_achievement_fields *a, const mc_achievement_fields *b)
{
    return a->a == b->a && a->b == b->b && a->c == b->c && a->d == b->d && a->l == b->l &&
           a->special == b->special && a->client_callback == b->client_callback;
}

static bool same_base(const mc_statistic_fields *a, const mc_statistic_fields *b)
{
    return a->e == b->e && a->f == b->f && a->g == b->g && a->h == b->h && a->a == b->a;
}

static bool same_error(const mc_registry_error *a, const mc_registry_error *b)
{
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

static bool contains(const mc_statistic_list *list, const mc_statistic *object)
{
    for (size_t i = 0; i < mc_statistic_list_size(list); ++i) {
        const mc_statistic *out = NULL;
        if (mc_statistic_list_get(list, i, &out) != MC_REGISTRY_OK)
            return false;
        if (out == object)
            return true;
    }
    return false;
}

static int published(mc_registry_classes *owner, const mc_registry_achievement_view *view, size_t n)
{
    int result = 1;
    CHECK(view->named != NULL && n <= 16 && mc_statistic_list_size(view->e) == n);
    for (size_t i = 0; i < 16; ++i) {
        if (i >= n) {
            CHECK(view->named[i] == NULL);
            continue;
        }
        const mc_statistic *object = view->named[i], *found = NULL, *listed = NULL;
        const mc_achievement_fields *f = mc_statistic_achievement_fields_get(object);
        const mc_statistic_fields *base = mc_statistic_fields_get(object);
        CHECK(f != NULL && base != NULL &&
              mc_statistic_kind_get(object) == MC_STATISTIC_ACHIEVEMENT);
        CHECK(base->e == 5242880 + (int32_t)i && base->g == (i == 0) &&
              base->a == owner->statistic_base.i);
        CHECK(f->a == xs[i] && f->b == ys[i] &&
              f->c == (parents[i] < 0 ? NULL : view->named[parents[i]]));
        CHECK(f->special == (i == 11 || i == 15) && f->client_callback == NULL);
        CHECK(mc_registry_classes_find_statistic(owner, base->e, &found) == MC_REGISTRY_OK &&
              found == object);
        CHECK(mc_statistic_list_get(view->e, i, &listed) == MC_REGISTRY_OK && listed == object);
        CHECK(contains(owner->statistics.b, object) && !contains(owner->statistics.c, object));
        mc_item_stack_fields fields;
        mc_item_stack_phase phase;
        CHECK(f->d != NULL && mc_item_stack_fields_get(f->d, &fields) == MC_ITEM_STACK_OK &&
              mc_item_stack_phase_get(f->d, &phase) == MC_ITEM_STACK_OK &&
              phase == MC_ITEM_STACK_READY);
        CHECK(fields.count == 1 && fields.animation == 0 && fields.damage == 0 &&
              mc_item_stack_source(f->d) == NULL);
        if (block_icons[i]) {
            const mc_block *icon = NULL;
            CHECK(mc_registry_classes_inspect_block_static(owner, icons[i], &icon, NULL) ==
                      MC_REGISTRY_OK &&
                  icon != NULL);
            CHECK(fields.id == mc_block_fields_get(icon)->bn);
        } else {
            const mc_item *icon = NULL;
            CHECK(mc_registry_classes_inspect_item_static(owner, icons[i], &icon, NULL) ==
                      MC_REGISTRY_OK &&
                  icon != NULL);
            CHECK(fields.id == mc_item_fields_get(icon)->bf);
        }
        char key[96], desc[104];
        CHECK(snprintf(key, sizeof(key), "achievement.%s", suffixes[i]) > 0);
        CHECK(snprintf(desc, sizeof(desc), "%s.desc", key) > 0);
        const mc_registry_string *value = property(owner->translations.singleton_a, key),
                                 *description = property(owner->translations.singleton_a, desc);
        if (value != NULL)
            CHECK(base->f == value);
        else
            CHECK(text_is(base->f, key));
        if (description != NULL)
            CHECK(f->l == description);
        else
            CHECK(text_is(f->l, desc));
        CHECK(base->f != f->l);
        for (size_t j = 0; j < i; ++j)
            CHECK(view->named[j] != object &&
                  mc_statistic_achievement_fields_get(view->named[j])->d != f->d);
    }
    result = 0;
cleanup:
    return result;
}

static int test_bootstrap(mc_registry_side side, mc_materials *materials,
                          mc_java_random_profile *profile, bool item_first)
{
    int result = 1;
    resources r;
    resources_init(&r);
    mc_registry_classes *owner = NULL;
    mc_item_stack *held = NULL;
    bool retained = false;
    mc_item_stack_fields held_fields = {0};
    uint64_t u = uniquifier(profile);
    CHECK(create(side, materials, profile, 0, &r, &owner) == MC_REGISTRY_OK);
    mc_registry_achievement_view cold;
    CHECK(mc_registry_classes_inspect_achievements(owner, &cold, NULL) == MC_REGISTRY_OK &&
          cold.phase == MC_REGISTRY_COLD && cold.initializer_pc == -1);
    CHECK(cold.a == 0 && cold.b == 0 && cold.c == 0 && cold.d == 0 && cold.e == NULL);
    CHECK(mc_statistic_achievement_fields_get(NULL) == NULL && mc_registry_stack_count(NULL) == 0);
    mc_registry_error error;
    CHECK((item_first ? mc_registry_classes_initialize_items(owner, &error)
                      : mc_registry_classes_initialize_blocks(owner, &error)) ==
          (item_first ? MC_REGISTRY_NULL_REFERENCE : MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY));
    CHECK(uniquifier(profile) == advanced(u, 5) && profile->math == NULL);
    mc_registry_achievement_view view;
    CHECK(mc_registry_classes_inspect_achievements(owner, &view, NULL) == MC_REGISTRY_OK);
    if (item_first) {
        CHECK(view.phase == MC_REGISTRY_FAILED && view.initializer_pc == 23 &&
              mc_statistic_list_size(view.e) == 0);
        CHECK(view.a == 0 && view.b == 0 && view.c == 0 && view.d == 0 &&
              mc_registry_stack_count(owner) == 1);
        CHECK(error.constructor_class != NULL &&
              strcmp(error.constructor_class, side == MC_REGISTRY_CLIENT ? "iz" : "fy") == 0 &&
              error.constructor_pc == 2);
        for (size_t i = 0; i < 16; ++i)
            CHECK(view.named[i] == NULL);
        const mc_item_stack *raw = NULL;
        CHECK(mc_registry_stack_get(owner, 0, &raw) == MC_REGISTRY_OK && raw != NULL);
        held = (mc_item_stack *)raw;
        mc_item_stack_phase phase;
        CHECK(mc_item_stack_phase_get(raw, &phase) == MC_ITEM_STACK_OK &&
              phase == MC_ITEM_STACK_RAW);
        CHECK(mc_item_stack_fields_get(raw, &held_fields) == MC_ITEM_STACK_OK &&
              held_fields.count == 0 && held_fields.animation == 0 && held_fields.id == 0 &&
              held_fields.damage == 0);
    } else {
        CHECK(view.phase == MC_REGISTRY_READY && view.initializer_pc == 454 && view.a == -1 &&
              view.b == -5 && view.c == 8 && view.d == 6);
        CHECK(published(owner, &view, 16) == 0 && mc_registry_stack_count(owner) == 16);
        CHECK(view.e != owner->statistics.b && view.e != owner->statistics.c &&
              view.e != owner->statistics.d && view.e != owner->statistics.e);
        CHECK(error.constructor_pc == 21 && error.constructor_class != NULL &&
              strcmp(error.constructor_class, side == MC_REGISTRY_CLIENT ? "jl" : "gg") == 0);
        size_t bytes = owner->allocation_bytes;
        CHECK(mc_registry_classes_initialize_achievements(owner, NULL) == MC_REGISTRY_OK &&
              owner->allocation_bytes == bytes);
        held = (mc_item_stack *)mc_statistic_achievement_fields_get(view.named[0])->d;
        CHECK(mc_item_stack_fields_get(held, &held_fields) == MC_ITEM_STACK_OK);
    }
    CHECK(mc_item_stack_retain(held) == MC_ITEM_STACK_OK);
    retained = true;
    const mc_item_stack *sentinel = held;
    CHECK(mc_registry_stack_get(owner, mc_registry_stack_count(owner), &sentinel) ==
              MC_REGISTRY_INDEX_OUT_OF_RANGE &&
          sentinel == held);
    CHECK(mc_registry_stack_get(NULL, 0, &sentinel) == MC_REGISTRY_INVALID_ARGUMENT &&
          sentinel == held);
    CHECK(mc_registry_stack_get(owner, 0, NULL) == MC_REGISTRY_INVALID_ARGUMENT);
    CHECK(retire(owner, false) == 0);
    owner = NULL;
    mc_item_stack_fields after;
    CHECK(mc_item_stack_fields_get(held, &after) == MC_ITEM_STACK_OK &&
          same_fields(&after, &held_fields));
    CHECK(!r.invalid);
    result = 0;
cleanup:
    if (retire(owner, false) != 0)
        result = 1;
    if (retained)
        mc_item_stack_release(held);
    return result;
}

/* Retain values, not padded byte copies. Include registered-but-not-named
 * Achievement prefixes and every actual RAW/READY stack in the real ledger. */
static int retry_snapshot(mc_registry_classes *owner, resources *r)
{
    int result = 1;
    const mc_statistic *objects[16] = {0}, *named[16] = {0}, *listed[16] = {0};
    mc_statistic_fields bases[16];
    mc_achievement_fields typed[16];
    const mc_item_stack *stacks[16] = {0};
    mc_item_stack_fields fields[16];
    mc_item_stack_phase phases[16];
    mc_registry_achievement_view before, after;
    mc_registry_statistic_view stats_before, stats_after;
    CHECK(mc_registry_classes_inspect_achievements(owner, &before, NULL) == MC_REGISTRY_OK);
    CHECK(mc_registry_classes_inspect_statistics(owner, &stats_before, NULL) == MC_REGISTRY_OK);
    size_t object_count = 0, list_count = mc_statistic_list_size(before.e);
    size_t stack_count = mc_registry_stack_count(owner);
    CHECK(list_count <= COUNT(listed) && stack_count <= COUNT(stacks));
    for (size_t i = 0; i < 16; ++i)
        named[i] = before.named[i];
    for (size_t i = 0; i < list_count; ++i)
        CHECK(mc_statistic_list_get(before.e, i, &listed[i]) == MC_REGISTRY_OK);
    for (size_t i = 0; i < stats_before.map_size; ++i) {
        const mc_statistic *object = NULL;
        int32_t id;
        CHECK(mc_registry_classes_statistic_entry(owner, i, &id, &object) == MC_REGISTRY_OK);
        if (mc_statistic_kind_get(object) != MC_STATISTIC_ACHIEVEMENT)
            continue;
        CHECK(object_count < COUNT(objects));
        objects[object_count] = object;
        bases[object_count] = *mc_statistic_fields_get(object);
        typed[object_count] = *mc_statistic_achievement_fields_get(object);
        CHECK(id == bases[object_count].e);
        ++object_count;
    }
    for (size_t i = 0; i < stack_count; ++i) {
        CHECK(mc_registry_stack_get(owner, i, &stacks[i]) == MC_REGISTRY_OK);
        CHECK(mc_item_stack_fields_get(stacks[i], &fields[i]) == MC_ITEM_STACK_OK);
        CHECK(mc_item_stack_phase_get(stacks[i], &phases[i]) == MC_ITEM_STACK_OK);
    }
    const mc_registry_error block_failure = owner->block_failure;
    const mc_registry_error achievement_failure = owner->achievements_state.failure;
    size_t bytes = owner->allocation_bytes, opens[3], reads[3], closes[3];
    uint64_t u = uniquifier(owner->options.random_profile);
    for (size_t i = 0; i < 3; ++i) {
        opens[i] = r->streams[i].opens;
        reads[i] = r->streams[i].reads;
        closes[i] = r->streams[i].closes;
    }
    CHECK(mc_registry_classes_initialize_blocks(owner, NULL) == MC_REGISTRY_FAILED_INITIALIZATION);
    CHECK(mc_registry_classes_inspect_achievements(owner, &after, NULL) == MC_REGISTRY_OK);
    CHECK(mc_registry_classes_inspect_statistics(owner, &stats_after, NULL) == MC_REGISTRY_OK);
    CHECK(before.phase == after.phase && before.initializer_pc == after.initializer_pc &&
          before.a == after.a && before.b == after.b && before.c == after.c &&
          before.d == after.d && before.e == after.e && before.named == after.named);
    CHECK(stats_before.phase == stats_after.phase &&
          stats_before.initializer_pc == stats_after.initializer_pc &&
          stats_before.map_size == stats_after.map_size && stats_before.G == stats_after.G &&
          stats_before.H == stats_after.H);
    CHECK(same_error(&block_failure, &owner->block_failure) &&
          same_error(&achievement_failure, &owner->achievements_state.failure));
    CHECK(owner->allocation_bytes == bytes && uniquifier(owner->options.random_profile) == u &&
          mc_registry_stack_count(owner) == stack_count);
    for (size_t i = 0; i < 16; ++i)
        CHECK(named[i] == after.named[i]);
    CHECK(mc_statistic_list_size(after.e) == list_count);
    for (size_t i = 0; i < list_count; ++i) {
        const mc_statistic *object = NULL;
        CHECK(mc_statistic_list_get(after.e, i, &object) == MC_REGISTRY_OK && object == listed[i]);
    }
    for (size_t i = 0; i < object_count; ++i) {
        const mc_statistic *found = NULL;
        CHECK(mc_registry_classes_find_statistic(owner, bases[i].e, &found) == MC_REGISTRY_OK &&
              found == objects[i]);
        CHECK(same_base(&bases[i], mc_statistic_fields_get(objects[i])) &&
              same_achievement(&typed[i], mc_statistic_achievement_fields_get(objects[i])));
    }
    for (size_t i = 0; i < stack_count; ++i) {
        const mc_item_stack *stack = NULL;
        mc_item_stack_fields current;
        mc_item_stack_phase phase;
        CHECK(mc_registry_stack_get(owner, i, &stack) == MC_REGISTRY_OK && stack == stacks[i]);
        CHECK(mc_item_stack_fields_get(stack, &current) == MC_ITEM_STACK_OK &&
              same_fields(&current, &fields[i]));
        CHECK(mc_item_stack_phase_get(stack, &phase) == MC_ITEM_STACK_OK && phase == phases[i]);
    }
    for (size_t i = 0; i < 3; ++i)
        CHECK(opens[i] == r->streams[i].opens && reads[i] == r->streams[i].reads &&
              closes[i] == r->streams[i].closes);
    CHECK(!r->invalid && owner->entered == 0 && owner->initializer == NULL);
    result = 0;
cleanup:
    return result;
}

static int bootstrap_budget(mc_registry_side side, mc_materials *materials,
                            mc_java_random_profile *profile, size_t budget)
{
    int result = 1;
    resources r;
    resources_init(&r);
    mc_registry_classes *owner = NULL;
    uint64_t u = uniquifier(profile);
    CHECK(create(side, materials, profile, budget, &r, &owner) == MC_REGISTRY_OK);
    mc_registry_status status = mc_registry_classes_initialize_blocks(owner, NULL);
    CHECK(status == MC_REGISTRY_LIMIT || status == MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY);
    bool matched = false;
    for (size_t i = 0; i <= 5; ++i)
        if (uniquifier(profile) == advanced(u, i))
            matched = true;
    CHECK(matched && profile->math == NULL);
    mc_registry_achievement_view view;
    CHECK(mc_registry_classes_inspect_achievements(owner, &view, NULL) == MC_REGISTRY_OK);
    size_t n = 0;
    while (n < 16 && view.named[n] != NULL)
        ++n;
    for (size_t i = n; i < 16; ++i)
        CHECK(view.named[i] == NULL);
    if (status == MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY || budget == 4u * 1024u * 1024u) {
        CHECK(status == MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY && n == 16 &&
              view.phase == MC_REGISTRY_READY);
        CHECK(published(owner, &view, 16) == 0 && uniquifier(profile) == advanced(u, 5));
    }
    CHECK(retry_snapshot(owner, &r) == 0);
    result = 0;
cleanup:
    if (retire(owner, false) != 0)
        result = 1;
    return result;
}

static int test_bootstrap_budgets(mc_registry_side side, mc_materials *materials,
                                  mc_java_random_profile *profile)
{
    int result = 1;
    const size_t extras[] = {0, 32768, 65536, 131072, 262144, 524288, 1048576, 2097152};
    for (size_t i = 0; i < COUNT(extras); ++i)
        CHECK(bootstrap_budget(side, materials, profile, sizeof(mc_registry_classes) + extras[i]) ==
              0);
    CHECK(bootstrap_budget(side, materials, profile, 4u * 1024u * 1024u) == 0);
    result = 0;
cleanup:
    return result;
}

/* Explicit isolated method admission, never a successful original class.
 * The two actual INITIALIZING contexts own real collection storage. */
static mc_registry_status isolated(mc_registry_side side, mc_materials *materials,
                                   mc_java_random_profile *profile, size_t limit, resources *r,
                                   mc_registry_classes **out)
{
    mc_registry_status status = create(side, materials, profile, limit, r, out);
    if (status != MC_REGISTRY_OK)
        return status;
    mc_registry_classes *owner = *out;
    status = mc_registry_classes_initialize_statistic_base(owner, NULL);
    if (status != MC_REGISTRY_OK)
        return status;
    status = mc_registry_classes_initialize_translation(owner, NULL);
    if (status != MC_REGISTRY_OK)
        return status;
    owner->entered = 1;
    owner->statistics_state.phase = MC_REGISTRY_INITIALIZING;
    owner->achievements_state.phase = MC_REGISTRY_INITIALIZING;
    status = mc_registry_statistic_collections(owner);
    if (status != MC_REGISTRY_OK)
        return status;
    mc_registry_list *list = mc_registry_allocate(owner, sizeof(*list));
    if (list == NULL)
        return owner->last_error.status;
    *list = (mc_registry_list){0};
    owner->achievements.e = list;
    return MC_REGISTRY_OK;
}

static bool unicode_key(const mc_registry_string *value, bool description)
{
    const char *prefix = "achievement.";
    mc_registry_string_view view;
    if (!mc_registry_string_read(value, &view) ||
        view.length != strlen(prefix) + 3 + (description ? 5 : 0))
        return false;
    for (size_t i = 0; i < strlen(prefix); ++i)
        if (view.units[i] != (unsigned char)prefix[i])
            return false;
    size_t n = strlen(prefix);
    if (view.units[n] != 'x' || view.units[n + 1] != 0 || view.units[n + 2] != 0xd800)
        return false;
    if (description)
        for (size_t i = 0; i < 5; ++i)
            if (view.units[n + 3 + i] != (unsigned char)".desc"[i])
                return false;
    return true;
}

static int test_methods(mc_registry_side side, mc_materials *materials,
                        mc_java_random_profile *profile, bool missing)
{
    int result = 1;
    resources r;
    resources_init(&r);
    r.missing_key = missing;
    mc_registry_classes *owner = NULL;
    mc_item_stack *borrowed = NULL;
    uint64_t u = uniquifier(profile);
    CHECK(mc_item_stack_allocate(&borrowed) == MC_ITEM_STACK_OK &&
          mc_item_stack_construct(borrowed, 777, 2, 3) == MC_ITEM_STACK_OK);
    CHECK(isolated(side, materials, profile, 0, &r, &owner) == MC_REGISTRY_OK);
    const mc_registry_string *suffix = literal(owner, "unit");
    CHECK(suffix != NULL);
    mc_statistic *first = NULL;
    CHECK(mc_registry_statistic_allocate(owner, MC_STATISTIC_ACHIEVEMENT, &first) ==
          MC_REGISTRY_OK);
    CHECK(mc_registry_achievement_construct_stack(first, 37, suffix, -2, 3, NULL, NULL) ==
          MC_REGISTRY_OK);
    const mc_achievement_fields *f = mc_statistic_achievement_fields_get(first);
    const mc_statistic_fields *base = mc_statistic_fields_get(first);
    CHECK(f != NULL && f->a == -2 && f->b == 3 && f->c == NULL && f->d == NULL && !f->special &&
          f->client_callback == NULL);
    CHECK(base->e == 5242917 && base->a == owner->statistic_base.i && !base->g && base->h == NULL);
    CHECK(text_is(base->f, "Display %q") && text_is(f->l, "Details %q")); /* plain lookup */
    CHECK(base->f == property(owner->translations.singleton_a, "achievement.unit"));
    CHECK(mc_registry_statistic_flag(first) == MC_REGISTRY_OK && base->g && !f->special);
    CHECK(mc_registry_achievement_special(first) == MC_REGISTRY_OK && f->special && base->g);
    r.watch = first;
    CHECK(mc_registry_statistic_register(first) ==
          (missing ? MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY : MC_REGISTRY_OK));
    CHECK(r.observed == 1 && !r.invalid && owner->statistics.a != NULL &&
          mc_statistic_list_size(owner->statistics.b) == 1 &&
          mc_statistic_list_size(owner->statistics.c) == 0 &&
          mc_statistic_list_size(owner->achievements.e) == (missing ? 0 : 1));
    CHECK(missing ? base->h == NULL : text_is(base->h, "key.unit"));
    size_t reads = r.streams[2].reads;
    CHECK(mc_registry_statistic_register(first) == MC_REGISTRY_DUPLICATE_STATISTIC &&
          r.streams[2].reads == reads);
    CHECK(text_is(owner->statistics.duplicate_message,
                  "Duplicate stat id: \"Display %q\" and \"Display %q\" at id 5242917"));
    if (!missing) {
        mc_statistic *duplicate = NULL, *null_name = NULL, *same_name = NULL, *wide = NULL;
        CHECK(mc_registry_statistic_allocate(owner, MC_STATISTIC_ACHIEVEMENT, &duplicate) ==
              MC_REGISTRY_OK);
        CHECK(mc_registry_achievement_construct_stack(duplicate, 37, suffix, -9, 12, borrowed,
                                                      first) == MC_REGISTRY_OK);
        CHECK(mc_registry_achievement_register(duplicate) == MC_REGISTRY_DUPLICATE_STATISTIC);
        CHECK(owner->achievements.a == -9 && owner->achievements.d == 12 &&
              mc_statistic_achievement_fields_get(duplicate)->c == first &&
              mc_statistic_achievement_fields_get(duplicate)->d == borrowed);
        CHECK(mc_statistic_list_size(owner->achievements.e) == 1 &&
              mc_statistic_list_size(owner->statistics.c) == 0);
        CHECK(mc_registry_statistic_allocate(owner, MC_STATISTIC_ACHIEVEMENT, &null_name) ==
              MC_REGISTRY_OK);
        CHECK(mc_registry_achievement_construct_stack(null_name, 38, NULL, 2, -3, borrowed,
                                                      first) == MC_REGISTRY_OK);
        CHECK(text_is(mc_statistic_fields_get(null_name)->f, "achievement.null") &&
              text_is(mc_statistic_achievement_fields_get(null_name)->l, "achievement.null.desc"));
        CHECK(mc_registry_achievement_register(null_name) == MC_REGISTRY_OK);
        CHECK(mc_registry_statistic_allocate(owner, MC_STATISTIC_ACHIEVEMENT, &same_name) ==
              MC_REGISTRY_OK);
        CHECK(mc_registry_achievement_construct_stack(same_name, 39, NULL, 0, 0, NULL, first) ==
              MC_REGISTRY_OK);
        CHECK(mc_statistic_fields_get(same_name)->f != mc_statistic_fields_get(null_name)->f &&
              mc_statistic_achievement_fields_get(same_name)->l !=
                  mc_statistic_achievement_fields_get(null_name)->l);
        CHECK(text_is(mc_statistic_fields_get(same_name)->f, "achievement.null"));
        CHECK(mc_registry_statistic_allocate(owner, MC_STATISTIC_ACHIEVEMENT, &wide) ==
              MC_REGISTRY_OK);
        const mc_registry_string *unicode =
            property(owner->translations.singleton_a, "unit.suffix");
        CHECK(unicode != NULL && mc_registry_achievement_construct_stack(
                                     wide, INT32_MAX, unicode, INT32_MIN, INT32_MAX, borrowed,
                                     first) == MC_REGISTRY_OK);
        CHECK(mc_statistic_fields_get(wide)->e == -2142240769 &&
              unicode_key(mc_statistic_fields_get(wide)->f, false) &&
              unicode_key(mc_statistic_achievement_fields_get(wide)->l, true));
        CHECK(owner->achievements.a == INT32_MIN && owner->achievements.d == INT32_MAX &&
              mc_statistic_achievement_fields_get(wide)->c == first &&
              mc_statistic_achievement_fields_get(wide)->d == borrowed);
    }
    CHECK(uniquifier(profile) == u && !r.invalid);
    result = 0;
cleanup:
    if (retire(owner, true) != 0)
        result = 1;
    if (!retirement_failed)
        mc_item_stack_release(borrowed);
    return result;
}

static int test_null_adapters(mc_registry_side side, mc_materials *materials,
                              mc_java_random_profile *profile, bool block)
{
    int result = 1;
    resources r;
    resources_init(&r);
    mc_registry_classes *owner = NULL;
    mc_item_stack *held = NULL;
    bool retained = false;
    CHECK(isolated(side, materials, profile, 0, &r, &owner) == MC_REGISTRY_OK);
    mc_statistic *object = NULL;
    CHECK(mc_registry_statistic_allocate(owner, MC_STATISTIC_ACHIEVEMENT, &object) ==
          MC_REGISTRY_OK);
    size_t count = mc_registry_stack_count(owner), bytes = owner->allocation_bytes;
    CHECK((block ? mc_registry_achievement_construct_block(object, 7, NULL, -8, 9, NULL, NULL)
                 : mc_registry_achievement_construct_item(object, 7, NULL, -8, 9, NULL, NULL)) ==
          MC_REGISTRY_NULL_REFERENCE);
    CHECK(mc_registry_stack_count(owner) == count + 1 &&
          owner->allocation_bytes >= bytes + mc_item_stack_object_bytes());
    CHECK(owner->last_error.constructor_class != NULL &&
          strcmp(owner->last_error.constructor_class, side == MC_REGISTRY_CLIENT ? "iz" : "fy") ==
              0 &&
          owner->last_error.constructor_pc == 2);
    const mc_statistic_fields *base = mc_statistic_fields_get(object);
    const mc_achievement_fields *f = mc_statistic_achievement_fields_get(object);
    CHECK(base->e == 0 && base->f == NULL && base->a == NULL && !base->g && base->h == NULL);
    CHECK(f->a == 0 && f->b == 0 && f->c == NULL && f->d == NULL && f->l == NULL && !f->special &&
          f->client_callback == NULL);
    const mc_item_stack *raw = NULL;
    CHECK(mc_registry_stack_get(owner, 0, &raw) == MC_REGISTRY_OK && raw != NULL);
    mc_item_stack_phase phase;
    mc_item_stack_fields fields;
    CHECK(mc_item_stack_phase_get(raw, &phase) == MC_ITEM_STACK_OK && phase == MC_ITEM_STACK_RAW);
    CHECK(mc_item_stack_fields_get(raw, &fields) == MC_ITEM_STACK_OK && fields.count == 0 &&
          fields.animation == 0 && fields.id == 0 && fields.damage == 0);
    held = (mc_item_stack *)raw;
    CHECK(mc_item_stack_retain(held) == MC_ITEM_STACK_OK);
    retained = true;
    CHECK(retire(owner, true) == 0);
    owner = NULL;
    CHECK(mc_item_stack_phase_get(held, &phase) == MC_ITEM_STACK_OK && phase == MC_ITEM_STACK_RAW);
    result = 0;
cleanup:
    if (retire(owner, true) != 0)
        result = 1;
    if (retained)
        mc_item_stack_release(held);
    return result;
}

static int constructor_trial(mc_registry_side side, mc_materials *materials,
                             mc_java_random_profile *profile, mc_item_stack *icon, size_t budget,
                             bool require_prefix, bool *complete)
{
    int result = 1;
    resources r;
    resources_init(&r);
    mc_registry_classes *owner = NULL;
    *complete = false;
    mc_registry_status status = isolated(side, materials, profile, budget, &r, &owner);
    mc_statistic *parent = NULL, *object = NULL;
    if (status == MC_REGISTRY_OK)
        status = mc_registry_statistic_allocate(owner, MC_STATISTIC_ACHIEVEMENT, &parent);
    if (status == MC_REGISTRY_OK)
        status = mc_registry_statistic_allocate(owner, MC_STATISTIC_ACHIEVEMENT, &object);
    if (status == MC_REGISTRY_OK)
        status = mc_registry_achievement_construct_stack(object, 77, NULL, -2, 3, icon, parent);
    CHECK(status == MC_REGISTRY_OK || status == MC_REGISTRY_LIMIT);
    if (status == MC_REGISTRY_OK) {
        *complete = true;
        const mc_achievement_fields *f = mc_statistic_achievement_fields_get(object);
        CHECK(f->c == parent && f->d == icon && f->a == -2 && f->b == 3 && f->l != NULL);
        CHECK(owner->achievements.a == -2 && owner->achievements.b == 0 &&
              owner->achievements.c == 0 && owner->achievements.d == 3);
    }
    if (require_prefix) {
        CHECK(status == MC_REGISTRY_LIMIT && object != NULL);
        const mc_statistic_fields *base = mc_statistic_fields_get(object);
        const mc_achievement_fields *f = mc_statistic_achievement_fields_get(object);
        CHECK(base->e == 5242957 && base->f != NULL && base->a == owner->statistic_base.i &&
              !base->g && base->h == NULL);
        CHECK(f->d == icon && f->l == NULL && f->a == 0 && f->b == 0 && f->c == NULL);
        CHECK(owner->achievements.a == 0 && owner->achievements.b == 0 &&
              owner->achievements.c == 0 && owner->achievements.d == 0);
    }
    result = 0;
cleanup:
    if (retire(owner, true) != 0)
        result = 1;
    return result;
}

static int test_constructor_limit(mc_registry_side side, mc_materials *materials,
                                  mc_java_random_profile *profile)
{
    int result = 1;
    mc_item_stack *icon = NULL;
    CHECK(mc_item_stack_allocate(&icon) == MC_ITEM_STACK_OK &&
          mc_item_stack_construct(icon, 999, 1, 0) == MC_ITEM_STACK_OK);
    size_t low = sizeof(mc_registry_classes), high = 1024u * 1024u;
    bool complete;
    CHECK(constructor_trial(side, materials, profile, icon, high, false, &complete) == 0 &&
          complete);
    while (low < high) {
        size_t middle = low + (high - low) / 2;
        CHECK(constructor_trial(side, materials, profile, icon, middle, false, &complete) == 0);
        if (complete)
            high = middle;
        else
            low = middle + 1;
    }
    CHECK(low > sizeof(mc_registry_classes));
    CHECK(constructor_trial(side, materials, profile, icon, low - 1, true, &complete) == 0 &&
          !complete);
    CHECK(constructor_trial(side, materials, profile, icon, low, false, &complete) == 0 &&
          complete);
    result = 0;
cleanup:
    if (!retirement_failed)
        mc_item_stack_release(icon);
    return result;
}

int main(void)
{
    int result = 1;
    mc_materials *materials = NULL;
    mc_java_random_profile *profile = NULL;
    CHECK(mc_materials_create(&materials));
    CHECK(mc_java_random_profile_create(SIZE_MAX, &profile) == MC_JAVA_RANDOM_PROFILE_OK);
    for (int side = MC_REGISTRY_CLIENT; side <= MC_REGISTRY_SERVER; ++side) {
        CHECK(test_bootstrap((mc_registry_side)side, materials, profile, false) == 0);
        CHECK(test_bootstrap((mc_registry_side)side, materials, profile, true) == 0);
        CHECK(test_bootstrap_budgets((mc_registry_side)side, materials, profile) == 0);
        CHECK(test_methods((mc_registry_side)side, materials, profile, false) == 0);
        CHECK(test_methods((mc_registry_side)side, materials, profile, true) == 0);
        CHECK(test_null_adapters((mc_registry_side)side, materials, profile, false) == 0);
        CHECK(test_null_adapters((mc_registry_side)side, materials, profile, true) == 0);
        CHECK(test_constructor_limit((mc_registry_side)side, materials, profile) == 0);
    }
    result = 0;
cleanup:
    if (!retirement_failed) {
        mc_java_random_profile_free(profile);
        mc_materials_destroy(materials);
    }
    return result == 0 && printf("Achievement native contracts: %u checks passed\n", checks) < 0
               ? 1
               : result;
}
