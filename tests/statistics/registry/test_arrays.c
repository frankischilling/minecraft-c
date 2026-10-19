#include "mc/item/recipes/recipes.h"
#include "mc/item/registry/registry.h"
#include "mc/resources/translation/translation.h"
#include "mc/statistics/achievements/achievements.h"
#include "mc/statistics/registry/registry.h"

#include "../../../src/block/registry/registry_internal.h"
#include "../../../src/core/java/random_profile_internal.h"
#include "../../../src/resources/translation/translation_internal.h"

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

static const uint8_t language[] =
    "stat.mineBlock=Mine %1$s\nstat.useItem=Use %1$s\nstat.breakItem=Break %1$s\n"
    "stat.craftItem=Craft %1$s\n"
    "unit.empty=\nunit.named=Value %1$s\nunit.arg=Q\\u0000\\ud800\nunit.upper=%S\n"
    "unit.sharp=\\u00df\nunit.bad=%q\n";
static const uint8_t malformed[] = "stat.mineBlock=%q\nstat.useItem=%q\nstat.breakItem=%q\n";
static const uint8_t key_input[] = "1000,key.start\n";
static const int32_t common_ids[] = {1000, 1001, 1002, 1003, 1004, 1100, 2000, 2001,
                                     2002, 2003, 2004, 2005, 2006, 2007, 2008, 2010,
                                     2011, 2020, 2021, 2022, 2023, 2024, 2025};
/* Actual source pair order, including E->E. These are selectors of live
 * input Block fields, not an expected native final array. */
static const char *const firsts[] = {"C", "E", "bg", "aD", "aP", "bj", "aR", "ah", "ak", "v", "aB"};
static const char *const seconds[] = {"B", "E", "bb", "aC", "aO", "bi", "aQ", "ag", "al", "w", "w"};

/* Actual List b crafting-stat subsequence from both sides' ordinary completed
 * Stats/Block bootstrap fixtures, not native HashMap inspection order. The
 * recipe output traversal feeds an Integer HashSet before Stats.d iterates it. */
static const int32_t crafted_stat_ids[] = {
    16843008, 16843009, 16842753, 16843010, 16843011, 16843013, 16842757, 16843014, 16843015,
    16843016, 16843017, 16843018, 16843019, 16843020, 16843021, 16843022, 16843023, 16843024,
    16843025, 16843026, 16843027, 16843028, 16842772, 16843029, 16843030, 16842774, 16842775,
    16843031, 16842776, 16843032, 16842777, 16843033, 16843034, 16842779, 16843035, 16842780,
    16843036, 16843037, 16842781, 16843038, 16842785, 16843042, 16843043, 16842787, 16843044,
    16843045, 16843046, 16842793, 16843049, 16842794, 16843050, 16843051, 16843052, 16842796,
    16843053, 16842797, 16842798, 16843054, 16843055, 16842799, 16843056, 16843057, 16843058,
    16842802, 16843059, 16843060, 16842805, 16843061, 16842806, 16843062, 16843063, 16843064,
    16842809, 16843065, 16843066, 16842810, 16843067, 16843068, 16842813, 16843069, 16843072,
    16842817, 16843073, 16842818, 16843074, 16843075, 16842819, 16843076, 16843077, 16842821,
    16842822, 16843080, 16842824, 16843082, 16842828, 16843085, 16842829, 16842832, 16843088,
    16842834, 16843091, 16842836, 16843092, 16842837, 16843094, 16843095, 16843097, 16842841,
    16843098, 16843099, 16842843, 16843102, 16843103, 16842848, 16843105, 16843106, 16843107,
    16843108, 16843109, 16843110, 16843111};

typedef struct stream {
    const uint8_t *data;
    size_t size, offset, opens, reads, closes;
    bool closed;
} stream;
typedef struct resources {
    stream streams[3];
    mc_registry_resources source;
    bool invalid;
} resources;

static mc_java_properties_status read_bytes(void *context, uint8_t *out, size_t capacity,
                                            size_t *count)
{
    stream *s = context;
    ++s->reads;
    if (out == NULL || count == NULL || capacity == 0 || s->closed || s->offset > s->size)
        return MC_JAVA_PROPERTIES_IO_ERROR;
    size_t take = s->size - s->offset;
    if (take > 7)
        take = 7;
    if (take > capacity)
        take = capacity;
    if (take != 0)
        memcpy(out, s->data + s->offset, take);
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
            if (++r->streams[i].opens != 1)
                r->invalid = true;
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

static void resources_init(resources *r, bool bad)
{
    *r = (resources){0};
    r->streams[0] = (stream){bad ? malformed : language,
                             bad ? sizeof(malformed) - 1 : sizeof(language) - 1,
                             0,
                             0,
                             0,
                             0,
                             false};
    r->streams[1] = (stream){language, 0, 0, 0, 0, 0, false};
    r->streams[2] = (stream){key_input, sizeof(key_input) - 1, 0, 0, 0, 0, false};
    r->source = (mc_registry_resources){r, open_resource, close_resource, ready_resource};
}

static uint64_t uniquifier(const mc_java_random_profile *p)
{
    return atomic_load_explicit(&p->uniquifier, memory_order_relaxed);
}

static uint64_t advanced(uint64_t value, size_t count)
{
    while (count-- != 0)
        value *= UINT64_C(1181783497276652981);
    return value;
}

static int retire(mc_registry_classes *owner)
{
    if (mc_registry_classes_free(owner, NULL) == MC_REGISTRY_OK)
        return 0;
    retirement_failed = true;
    fputs("class retirement refused; preserving dependencies\n", stderr);
    return 1;
}

static mc_registry_status initialize(mc_registry_classes *owner, int entry,
                                     mc_registry_error *error)
{
    return entry == 0   ? mc_registry_classes_initialize_statistics(owner, error)
           : entry == 1 ? mc_registry_classes_initialize_blocks(owner, error)
                        : mc_registry_classes_initialize_items(owner, error);
}

static mc_registry_status create(mc_registry_side side, mc_materials *materials,
                                 mc_java_random_profile *profile, resources *r, size_t bytes,
                                 mc_registry_classes **out)
{
    const mc_registry_options options = {side, profile, materials, bytes, &r->source};
    return mc_registry_classes_allocate(&options, out, NULL);
}

static const mc_statistic *find(const mc_registry_classes *owner, int32_t id)
{
    const mc_statistic *out = NULL;
    return mc_registry_classes_find_statistic(owner, id, &out) == MC_REGISTRY_OK ? out : NULL;
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

static bool text_is(const mc_registry_string *string, const char *expected)
{
    mc_registry_string_view view;
    if (!mc_registry_string_read(string, &view) || view.length != strlen(expected))
        return false;
    for (size_t i = 0; i < view.length; ++i)
        if (view.units[i] != (unsigned char)expected[i])
            return false;
    return true;
}

static const mc_registry_string *literal(mc_registry_classes *owner, const char *text)
{
    const mc_registry_string *value = NULL;
    return mc_registry_literal_ascii(owner, text, &value) == MC_REGISTRY_OK ? value : NULL;
}

static const mc_registry_string *property(const mc_translation *translation, const char *name)
{
    uint16_t key[80];
    size_t n = strlen(name);
    if (n > COUNT(key))
        return NULL;
    for (size_t i = 0; i < n; ++i)
        key[i] = (unsigned char)name[i];
    const mc_registry_string *out = NULL;
    return mc_translation_property(translation, key, n, &out, NULL) == MC_REGISTRY_OK ? out : NULL;
}

static int check_randoms(const mc_registry_classes *owner, uint64_t before)
{
    int result = 1;
    CHECK(uniquifier(owner->options.random_profile) == advanced(before, 5));
    CHECK(owner->options.random_profile->math == NULL &&
          atomic_load_explicit(&owner->options.random_profile->holder_phase,
                               memory_order_relaxed) == MC_JAVA_RANDOM_HOLDER_COLD);
    CHECK(owner->blocks.m != NULL && owner->blocks.m[23] != NULL && owner->blocks.m[54] != NULL &&
          owner->blocks.m[61] != NULL && owner->blocks.m[62] != NULL);
    const mc_java_random *randoms[] = {
        owner->items.b, owner->blocks.m[23]->subtype.xq.a, owner->blocks.m[54]->subtype.e.a,
        owner->blocks.m[61]->subtype.tc.a, owner->blocks.m[62]->subtype.tc.a};
    for (size_t i = 0; i < COUNT(randoms); ++i) {
        CHECK(randoms[i] != NULL && randoms[i]->state <= UINT64_C(0xffffffffffff));
        uint64_t cache;
        memcpy(&cache, &randoms[i]->next_next_gaussian, sizeof(cache));
        CHECK(cache == 0 && !randoms[i]->have_next_next_gaussian);
        for (size_t j = 0; j < i; ++j)
            CHECK(randoms[i] != randoms[j]);
    }
    result = 0;
cleanup:
    return result;
}

static int check_unification(mc_registry_classes *owner, mc_statistic *const *array, int32_t base,
                             const mc_statistic_list *all, const mc_statistic_list *mined)
{
    int result = 1;
    CHECK(array != NULL);
    for (size_t i = 0; i < COUNT(firsts); ++i) {
        const mc_block *first = NULL, *second = NULL;
        CHECK(mc_registry_classes_inspect_block_static(owner, firsts[i], &first, NULL) ==
                  MC_REGISTRY_OK &&
              first != NULL);
        CHECK(mc_registry_classes_inspect_block_static(owner, seconds[i], &second, NULL) ==
                  MC_REGISTRY_OK &&
              second != NULL);
        int32_t a = mc_block_fields_get(first)->bn, b = mc_block_fields_get(second)->bn;
        CHECK(a > 0 && a < 256 && b > 0 && b < 256);
        const mc_statistic *old = find(owner, base + a), *target = find(owner, base + b);
        if (old != NULL && target == NULL) {
            CHECK(array[a] == old && array[b] == old && contains(all, old));
            if (mined != NULL)
                CHECK(contains(mined, old));
        } else {
            CHECK(array[a] == array[b]);
            if (old != NULL) {
                CHECK(!contains(all, old));
                if (mined != NULL)
                    CHECK(!contains(mined, old));
                CHECK(find(owner, base + a) == old); /* removal never erases map */
            }
        }
    }
    result = 0;
cleanup:
    return result;
}

static int record_output(const mc_item_stack *stack, bool *produced)
{
    int result = 1;
    mc_item_stack_fields fields;
    CHECK(stack != NULL && mc_item_stack_fields_get(stack, &fields) == MC_ITEM_STACK_OK);
    CHECK(fields.id >= 0 && fields.id < 32000);
    produced[fields.id] = true;
    result = 0;
cleanup:
    return result;
}

static int check_recipe_array(mc_registry_classes *owner, const mc_registry_statistic_view *view,
                              size_t *expected)
{
    int result = 1;
    bool produced[32000] = {false};
    mc_registry_crafting_view crafting;
    mc_registry_smelting_view smelting;
    CHECK(mc_registry_classes_inspect_crafting(owner, &crafting, NULL) == MC_REGISTRY_OK &&
          crafting.phase == MC_REGISTRY_READY && crafting.initializer_pc == 10 &&
          crafting.a != NULL);
    CHECK(mc_registry_classes_inspect_smelting(owner, &smelting, NULL) == MC_REGISTRY_OK &&
          smelting.phase == MC_REGISTRY_READY && smelting.initializer_pc == 10 &&
          smelting.a != NULL);
    CHECK(mc_crafting_manager_size(crafting.a) == 151 &&
          mc_smelting_manager_size(smelting.a) == 10);
    /* Stats.d reads the actual output Stack.c, not a shaped recipe's cached
     * output ID, the ingredient IDs, or the smelting map's input keys. */
    for (size_t i = 0; i < mc_crafting_manager_size(crafting.a); ++i) {
        const mc_recipe *recipe = NULL;
        CHECK(mc_crafting_manager_get(crafting.a, i, &recipe) == MC_REGISTRY_OK && recipe != NULL);
        CHECK(record_output(mc_recipe_output(recipe), produced) == 0);
    }
    int32_t keys[10];
    size_t count = 0;
    CHECK(mc_smelting_manager_keys(smelting.a, keys, COUNT(keys), &count) == MC_REGISTRY_OK &&
          count == COUNT(keys));
    for (size_t i = 0; i < count; ++i) {
        const mc_item_stack *stack = NULL;
        CHECK(mc_smelting_manager_find(smelting.a, keys[i], &stack) == MC_REGISTRY_OK);
        CHECK(record_output(stack, produced) == 0);
    }
    size_t registered = 0, slots = 0;
    for (size_t i = 0; i < COUNT(produced); ++i) {
        const mc_item *item = NULL;
        CHECK(mc_registry_item_array_get(owner->items.c, i, &item, NULL) == MC_REGISTRY_OK);
        const mc_statistic *statistic = find(owner, 16842752 + (int32_t)i);
        CHECK((statistic != NULL) == (produced[i] && item != NULL));
        if (statistic != NULL) {
            int32_t associated = -1;
            CHECK(mc_statistic_kind_get(statistic) == MC_STATISTIC_ITEM &&
                  !mc_statistic_fields_get(statistic)->g);
            CHECK(mc_statistic_item_id(statistic, &associated) == MC_REGISTRY_OK &&
                  associated == (int32_t)i);
            ++registered;
        }
        if (view->D[i] != NULL)
            ++slots;
    }
    CHECK(registered == COUNT(crafted_stat_ids) && slots == 125);
    CHECK(check_unification(owner, view->D, 16842752, view->b, NULL) == 0);
    size_t ordinal = 0;
    for (size_t i = 0; i < mc_statistic_list_size(view->b); ++i) {
        const mc_statistic *statistic = NULL;
        CHECK(mc_statistic_list_get(view->b, i, &statistic) == MC_REGISTRY_OK && statistic != NULL);
        int32_t id = mc_statistic_fields_get(statistic)->e;
        if (id >= 16842752 && id < 16842752 + 32000) {
            CHECK(ordinal < COUNT(crafted_stat_ids) && id == crafted_stat_ids[ordinal]);
            CHECK(statistic == find(owner, id));
            ++ordinal;
        }
    }
    CHECK(ordinal == COUNT(crafted_stat_ids));
    *expected += registered;
    result = 0;
cleanup:
    return result;
}

static int check_graph(mc_registry_classes *owner, int entry)
{
    int result = 1;
    mc_registry_statistic_view view;
    CHECK(mc_registry_classes_inspect_statistics(owner, &view, NULL) == MC_REGISTRY_OK);
    CHECK(view.named != NULL && mc_statistic_list_size(view.c) == 23);
    for (size_t i = 0; i < COUNT(common_ids); ++i) {
        const mc_statistic *common = find(owner, common_ids[i]), *listed = NULL;
        CHECK(common != NULL && view.named[i] == common &&
              mc_statistic_kind_get(common) == MC_STATISTIC_GENERAL);
        CHECK(mc_statistic_list_get(view.c, i, &listed) == MC_REGISTRY_OK && listed == common);
    }
    size_t expected = 23, nonblock = 0;
    if (entry != 2) {
        CHECK(view.C != NULL && view.D != NULL && view.E != NULL && view.F != NULL &&
              view.G == (entry == 1) && view.H == (entry == 1));
        CHECK(view.C != view.D && view.C != view.E && view.C != view.F && view.D != view.E &&
              view.D != view.F && view.E != view.F);
        for (size_t i = 0; i < 32000; ++i) {
            const mc_item *item = NULL;
            CHECK(mc_registry_item_array_get(owner->items.c, i, &item, NULL) == MC_REGISTRY_OK);
            const mc_statistic *used = find(owner, 16908288 + (int32_t)i);
            const mc_statistic *broken = find(owner, 16973824 + (int32_t)i);
            const mc_item_fields *f = item == NULL ? NULL : mc_item_fields_get(item);
            CHECK((used != NULL) == (item != NULL));
            CHECK((broken != NULL) == (f != NULL && f->a > 0 && !f->bj));
            if (used != NULL) {
                int32_t associated = -1;
                CHECK(mc_statistic_kind_get(used) == MC_STATISTIC_ITEM &&
                      !mc_statistic_fields_get(used)->g);
                CHECK(mc_statistic_item_id(used, &associated) == MC_REGISTRY_OK &&
                      associated == (int32_t)i);
                ++expected;
                if (i >= 256) {
                    const mc_statistic *listed = NULL;
                    CHECK(view.E[i] == used &&
                          mc_statistic_list_get(view.d, nonblock, &listed) == MC_REGISTRY_OK &&
                          listed == used);
                    ++nonblock;
                }
            }
            if (broken != NULL) {
                int32_t associated = -1;
                CHECK(mc_statistic_item_id(broken, &associated) == MC_REGISTRY_OK &&
                      associated == (int32_t)i);
                CHECK(view.F[i] == broken && mc_statistic_kind_get(broken) == MC_STATISTIC_ITEM);
                ++expected;
            }
            if (i >= 256 && item == NULL)
                CHECK(view.E[i] == NULL && view.F[i] == NULL);
            if (i >= 256 && broken == NULL)
                CHECK(view.F[i] == NULL);
        }
        CHECK(nonblock == 106 && mc_statistic_list_size(view.d) == nonblock);
        CHECK(check_unification(owner, view.E, 16908288, view.b, NULL) == 0);
        const mc_block *self = NULL;
        CHECK(mc_registry_classes_inspect_block_static(owner, "E", &self, NULL) == MC_REGISTRY_OK &&
              self != NULL);
        int32_t id = mc_block_fields_get(self)->bn;
        CHECK(view.E[id] == find(owner, 16908288 + id) && !contains(view.b, view.E[id]));
    }
    {
        CHECK(view.C != NULL);
        if (entry == 2)
            CHECK(view.D == NULL && view.E == NULL && view.F == NULL && !view.G && !view.H &&
                  mc_statistic_list_size(view.d) == 0);
        for (size_t i = 0; i < 256; ++i) {
            const mc_block *block = owner->blocks.m[i];
            const mc_statistic *mined = find(owner, 16777216 + (int32_t)i);
            CHECK((mined != NULL) == (block != NULL && mc_block_fields_get(block)->br));
            if (mined != NULL) {
                int32_t associated = -1;
                CHECK(mc_statistic_item_id(mined, &associated) == MC_REGISTRY_OK &&
                      associated == (int32_t)i);
                CHECK(mc_statistic_kind_get(mined) == MC_STATISTIC_ITEM &&
                      !mc_statistic_fields_get(mined)->g);
                ++expected;
            }
        }
        CHECK(check_unification(owner, view.C, 16777216, view.b, view.e) == 0);
    }
    if (entry != 2) {
        mc_registry_achievement_view achievements;
        CHECK(mc_registry_classes_inspect_achievements(owner, &achievements, NULL) ==
              MC_REGISTRY_OK);
        CHECK(achievements.phase == MC_REGISTRY_READY &&
              mc_statistic_list_size(achievements.e) == 16);
        for (size_t i = 0; i < 16; ++i) {
            CHECK(achievements.named[i] == find(owner, 5242880 + (int32_t)i));
            CHECK(mc_statistic_kind_get(achievements.named[i]) == MC_STATISTIC_ACHIEVEMENT);
        }
        expected += 16;
        CHECK(check_recipe_array(owner, &view, &expected) == 0);
    }
    CHECK(view.map_size == expected && view.map_size == (entry == 2 ? 99u : 486u));
    CHECK(mc_statistic_list_size(view.b) == (entry == 2 ? 91u : 467u) &&
          mc_statistic_list_size(view.e) == 68);
    for (size_t i = 0; i < view.map_size; ++i) {
        int32_t id;
        const mc_statistic *object = NULL;
        CHECK(mc_registry_classes_statistic_entry(owner, i, &id, &object) == MC_REGISTRY_OK);
        CHECK(object != NULL && mc_statistic_fields_get(object)->e == id &&
              find(owner, id) == object);
        for (size_t j = 0; j < i; ++j) {
            int32_t old_id;
            const mc_statistic *old = NULL;
            CHECK(mc_registry_classes_statistic_entry(owner, j, &old_id, &old) == MC_REGISTRY_OK &&
                  old_id != id && old != object);
        }
    }
    int32_t id = 123;
    const mc_statistic *object = view.named[0];
    CHECK(mc_registry_classes_statistic_entry(owner, view.map_size, &id, &object) ==
          MC_REGISTRY_INDEX_OUT_OF_RANGE);
    CHECK(id == 123 && object == view.named[0]);
    CHECK(mc_registry_classes_statistic_entry(NULL, 0, &id, &object) ==
          MC_REGISTRY_INVALID_ARGUMENT);
    CHECK(id == 123 && object == view.named[0]);
    CHECK(mc_registry_classes_statistic_entry(owner, 0, NULL, &object) ==
          MC_REGISTRY_INVALID_ARGUMENT);
    CHECK(mc_registry_classes_statistic_entry(owner, 0, &id, NULL) == MC_REGISTRY_INVALID_ARGUMENT);
    result = 0;
cleanup:
    return result;
}

static int check_retry(mc_registry_classes *owner, resources *r, int entry,
                       mc_registry_status previous)
{
    int result = 1;
    mc_statistic **saved[4] = {0};
    const mc_statistic **list[4] = {0};
    mc_registry_statistic_view before, after;
    CHECK(mc_registry_classes_inspect_statistics(owner, &before, NULL) == MC_REGISTRY_OK);
    mc_statistic *const *arrays[] = {before.C, before.D, before.E, before.F};
    const mc_statistic_list *lists[] = {before.b, before.c, before.d, before.e};
    size_t lengths[4] = {0};
    for (size_t a = 0; a < 4; ++a) {
        size_t n = a == 0 ? 256 : 32000;
        if (arrays[a] != NULL) {
            saved[a] = malloc(n * sizeof(*saved[a]));
            CHECK(saved[a] != NULL);
            for (size_t i = 0; i < n; ++i)
                saved[a][i] = arrays[a][i];
        }
        lengths[a] = mc_statistic_list_size(lists[a]);
        if (lengths[a] != 0) {
            list[a] = malloc(lengths[a] * sizeof(*list[a]));
            CHECK(list[a] != NULL);
            for (size_t i = 0; i < lengths[a]; ++i)
                CHECK(mc_statistic_list_get(lists[a], i, &list[a][i]) == MC_REGISTRY_OK);
        }
    }
    const mc_registry_error failure = owner->statistics_state.failure;
    uint64_t u = uniquifier(owner->options.random_profile);
    size_t bytes = owner->allocation_bytes, reads[3], opens[3], closes[3];
    for (size_t i = 0; i < 3; ++i) {
        reads[i] = r->streams[i].reads;
        opens[i] = r->streams[i].opens;
        closes[i] = r->streams[i].closes;
    }
    CHECK(initialize(owner, entry, NULL) ==
          (previous == MC_REGISTRY_OK ? MC_REGISTRY_OK : MC_REGISTRY_FAILED_INITIALIZATION));
    CHECK(mc_registry_classes_inspect_statistics(owner, &after, NULL) == MC_REGISTRY_OK);
    CHECK(before.phase == after.phase && before.initializer_pc == after.initializer_pc &&
          before.map_size == after.map_size && before.b == after.b && before.c == after.c &&
          before.d == after.d && before.e == after.e && before.C == after.C &&
          before.D == after.D && before.E == after.E && before.F == after.F &&
          before.G == after.G && before.H == after.H);
    CHECK(owner->allocation_bytes == bytes && uniquifier(owner->options.random_profile) == u);
    CHECK(owner->statistics_state.failure.status == failure.status &&
          owner->statistics_state.failure.constructor_class == failure.constructor_class &&
          owner->statistics_state.failure.constructor_pc == failure.constructor_pc &&
          owner->statistics_state.failure.format_error.status == failure.format_error.status);
    for (size_t a = 0; a < 4; ++a) {
        size_t n = a == 0 ? 256 : 32000;
        if (saved[a] != NULL)
            for (size_t i = 0; i < n; ++i)
                CHECK(saved[a][i] == arrays[a][i]);
        CHECK(lengths[a] == mc_statistic_list_size(lists[a]));
        for (size_t i = 0; i < lengths[a]; ++i) {
            const mc_statistic *object = NULL;
            CHECK(mc_statistic_list_get(lists[a], i, &object) == MC_REGISTRY_OK &&
                  object == list[a][i]);
        }
    }
    for (size_t i = 0; i < 3; ++i)
        CHECK(reads[i] == r->streams[i].reads && opens[i] == r->streams[i].opens &&
              closes[i] == r->streams[i].closes);
    CHECK(!r->invalid && owner->entered == 0 && owner->initializer == NULL);
    result = 0;
cleanup:
    for (size_t a = 0; a < 4; ++a) {
        free(saved[a]);
        free(list[a]);
    }
    return result;
}

static int test_route(mc_registry_side side, mc_materials *materials,
                      mc_java_random_profile *profile, int entry, bool bad)
{
    int result = 1;
    mc_registry_classes *owner = NULL;
    resources r;
    resources_init(&r, bad);
    uint64_t u = uniquifier(profile);
    CHECK(create(side, materials, profile, &r, 0, &owner) == MC_REGISTRY_OK);
    mc_registry_error error;
    const mc_registry_status status = initialize(owner, entry, &error);
    CHECK(status == (bad          ? MC_REGISTRY_ILLEGAL_ARGUMENT
                     : entry == 2 ? MC_REGISTRY_NULL_REFERENCE
                                  : MC_REGISTRY_OK));
    mc_registry_statistic_view view;
    CHECK(mc_registry_classes_inspect_statistics(owner, &view, NULL) == MC_REGISTRY_OK &&
          view.phase == (!bad && entry != 2 ? MC_REGISTRY_READY : MC_REGISTRY_FAILED));
    CHECK(check_randoms(owner, u) == 0);
    if (bad) {
        CHECK(error.format_error.status == MC_JAVA_FORMAT_UNKNOWN_CONVERSION);
        CHECK(error.constructor_class != NULL &&
              strcmp(error.constructor_class, side == MC_REGISTRY_CLIENT ? "nh" : "ii") == 0 &&
              error.constructor_pc == 12);
        CHECK(view.C == NULL && view.D == NULL && view.E == NULL && view.F == NULL && !view.G &&
              !view.H && view.map_size == 23);
    } else {
        CHECK(view.initializer_pc == (entry == 2 ? 624 : 635));
        if (entry == 2)
            CHECK(error.constructor_pc == 2 && error.constructor_class != NULL &&
                  strcmp(error.constructor_class, side == MC_REGISTRY_CLIENT ? "iz" : "fy") == 0);
        else {
            mc_registry_block_view blocks;
            mc_registry_item_view items;
            CHECK(error.status == MC_REGISTRY_OK && error.constructor_class == NULL &&
                  error.constructor_pc == -1);
            CHECK(mc_registry_classes_inspect_blocks(owner, &blocks, NULL) == MC_REGISTRY_OK &&
                  blocks.phase == MC_REGISTRY_READY);
            CHECK(mc_registry_classes_inspect_items(owner, &items, NULL) == MC_REGISTRY_OK &&
                  items.phase == MC_REGISTRY_READY);
        }
        CHECK(check_graph(owner, entry) == 0);
    }
    for (size_t i = 0; i < 3; ++i)
        CHECK(r.streams[i].opens == 1);
    CHECK(r.streams[0].closes == 0 && r.streams[1].closes == 0 && r.streams[2].closes == 1);
    CHECK(check_retry(owner, &r, entry, status) == 0);
    result = 0;
cleanup:
    if (retire(owner) != 0)
        result = 1;
    return result;
}

static int test_formatted(mc_registry_side side, mc_materials *materials,
                          mc_java_random_profile *profile)
{
    int result = 1;
    mc_registry_classes *owner = NULL;
    resources r;
    resources_init(&r, false);
    uint64_t u = uniquifier(profile);
    CHECK(create(side, materials, profile, &r, 0, &owner) == MC_REGISTRY_OK);
    CHECK(mc_registry_classes_initialize_translation(owner, NULL) == MC_REGISTRY_OK);
    const mc_registry_string *empty_key = literal(owner, "unit.empty"),
                             *named_key = literal(owner, "unit.named"),
                             *bad_key = literal(owner, "unit.bad"),
                             *upper_key = literal(owner, "unit.upper"), *empty = literal(owner, "");
    CHECK(empty_key != NULL && named_key != NULL && bad_key != NULL && upper_key != NULL &&
          empty != NULL);
    const mc_translation *translation = owner->translations.singleton_a;
    const mc_registry_string *argument = property(translation, "unit.arg"),
                             *sharp = property(translation, "unit.sharp");
    CHECK(argument != NULL && sharp != NULL);
    const mc_registry_string *args[] = {argument}, *one = NULL, *two = NULL;
    owner->entered = 1; /* actual private call admission, no fabricated READY */
    CHECK(mc_registry_translate_formatted(owner, empty_key, args, 1, &one) == MC_REGISTRY_OK &&
          one == empty);
    CHECK(mc_registry_translate_formatted(owner, empty_key, args, 1, &two) == MC_REGISTRY_OK &&
          two == one);
    CHECK(mc_registry_translate_formatted(owner, named_key, args, 1, &one) == MC_REGISTRY_OK);
    CHECK(mc_registry_translate_formatted(owner, named_key, args, 1, &two) == MC_REGISTRY_OK &&
          two != one);
    mc_registry_string_view a, b;
    CHECK(mc_registry_string_read(one, &a) && mc_registry_string_read(two, &b) && a.length == 9 &&
          b.length == a.length);
    const uint16_t expected[] = {'V', 'a', 'l', 'u', 'e', ' ', 'Q', 0, 0xd800};
    for (size_t i = 0; i < COUNT(expected); ++i)
        CHECK(a.units[i] == expected[i] && b.units[i] == expected[i]);
    const mc_registry_string *saved = one;
    CHECK(mc_registry_translate_formatted(owner, bad_key, args, 1, &one) ==
              MC_REGISTRY_ILLEGAL_ARGUMENT &&
          one == saved);
    CHECK(owner->last_error.format_error.status == MC_JAVA_FORMAT_UNKNOWN_CONVERSION);
    CHECK(owner->translation_state.phase == MC_REGISTRY_READY &&
          owner->translation_wrapper_state.phase == MC_REGISTRY_READY);
    args[0] = NULL;
    CHECK(mc_registry_translate_formatted(owner, named_key, args, 1, &one) == MC_REGISTRY_OK &&
          text_is(one, "Value null"));
    args[0] = sharp;
    CHECK(mc_registry_translate_formatted(owner, upper_key, args, 1, &one) == MC_REGISTRY_OK &&
          text_is(one, "SS"));
    for (size_t i = 0; i < COUNT(expected); ++i)
        CHECK(a.units[i] == expected[i]);
    owner->entered = 0;
    mc_registry_item_view items;
    CHECK(mc_registry_classes_inspect_items(owner, &items, NULL) == MC_REGISTRY_OK);
    CHECK(uniquifier(profile) == u && owner->blocks_phase == MC_REGISTRY_COLD &&
          items.phase == MC_REGISTRY_COLD);
    result = 0;
cleanup:
    if (owner != NULL)
        owner->entered = 0;
    if (retire(owner) != 0)
        result = 1;
    return result;
}

static int budget_trial(mc_registry_side side, mc_materials *materials,
                        mc_java_random_profile *profile, int entry, size_t limit, bool *complete)
{
    int result = 1;
    mc_registry_classes *owner = NULL;
    resources r;
    resources_init(&r, false);
    *complete = false;
    uint64_t u = uniquifier(profile);
    mc_registry_status status = create(side, materials, profile, &r, limit, &owner);
    if (status != MC_REGISTRY_OK) {
        CHECK(status == MC_REGISTRY_LIMIT && owner == NULL && uniquifier(profile) == u);
    } else {
        status = initialize(owner, entry, NULL);
        CHECK(status == MC_REGISTRY_LIMIT ||
              status == (entry == 2 ? MC_REGISTRY_NULL_REFERENCE : MC_REGISTRY_OK));
        CHECK(owner->allocation_bytes <= limit);
        mc_registry_statistic_view view;
        CHECK(mc_registry_classes_inspect_statistics(owner, &view, NULL) == MC_REGISTRY_OK);
        CHECK(!(view.G || view.H) || (view.E != NULL && view.F != NULL));
        CHECK(view.D == NULL || (view.E != NULL && view.F != NULL));
        if (entry == 2)
            CHECK(view.D == NULL && view.E == NULL && view.F == NULL && !view.G && !view.H);
        if (status != MC_REGISTRY_LIMIT) {
            CHECK(view.phase == (entry == 2 ? MC_REGISTRY_FAILED : MC_REGISTRY_READY) &&
                  view.initializer_pc == (entry == 2 ? 624 : 635));
            CHECK(view.C != NULL);
            if (entry != 2)
                CHECK(view.D != NULL && view.E != NULL && view.F != NULL &&
                      view.G == (entry == 1) && view.H == (entry == 1));
            *complete = true;
        }
        bool matched = false;
        for (size_t n = 0; n <= 5; ++n)
            if (uniquifier(profile) == advanced(u, n))
                matched = true;
        CHECK(matched && check_retry(owner, &r, entry, status) == 0);
    }
    result = 0;
cleanup:
    if (retire(owner) != 0)
        result = 1;
    return result;
}

static int test_budgets(mc_registry_side side, mc_materials *materials,
                        mc_java_random_profile *profile, int entry)
{
    int result = 1;
    size_t low = sizeof(mc_registry_classes), high = 4u * 1024u * 1024u;
    bool complete;
    CHECK(budget_trial(side, materials, profile, entry, high, &complete) == 0 && complete);
    while (low < high) {
        size_t middle = low + (high - low) / 2;
        CHECK(budget_trial(side, materials, profile, entry, middle, &complete) == 0);
        if (complete)
            high = middle;
        else
            low = middle + 1;
    }
    CHECK(low > sizeof(mc_registry_classes));
    CHECK(budget_trial(side, materials, profile, entry, low - 1, &complete) == 0 && !complete);
    CHECK(budget_trial(side, materials, profile, entry, low, &complete) == 0 && complete);
    CHECK(budget_trial(side, materials, profile, entry, sizeof(mc_registry_classes) - 1,
                       &complete) == 0 &&
          !complete);
    result = 0;
cleanup:
    return result;
}

int main(void)
{
    int result = 1;
    mc_materials *materials = NULL;
    mc_java_random_profile *profile = NULL;
    CHECK(mc_materials_create(&materials));
    CHECK(mc_java_random_profile_create(SIZE_MAX, &profile) == MC_JAVA_RANDOM_PROFILE_OK);
    mc_material *identities[MC_MATERIAL_COUNT];
    for (size_t i = 0; i < COUNT(identities); ++i)
        identities[i] = mc_materials_get(materials, (mc_material_id)i);
    for (int side = MC_REGISTRY_CLIENT; side <= MC_REGISTRY_SERVER; ++side) {
        CHECK(test_formatted((mc_registry_side)side, materials, profile) == 0);
        for (int entry = 0; entry < 3; ++entry) {
            CHECK(test_route((mc_registry_side)side, materials, profile, entry, false) == 0);
            CHECK(test_route((mc_registry_side)side, materials, profile, entry, true) == 0);
            CHECK(test_budgets((mc_registry_side)side, materials, profile, entry) == 0);
        }
    }
    for (size_t i = 0; i < COUNT(identities); ++i)
        CHECK(identities[i] == mc_materials_get(materials, (mc_material_id)i));
    result = 0;
cleanup:
    if (!retirement_failed) {
        mc_java_random_profile_free(profile);
        mc_materials_destroy(materials);
    }
    return result == 0 && printf("Statistics array contracts: %u checks passed\n", checks) < 0
               ? 1
               : result;
}
