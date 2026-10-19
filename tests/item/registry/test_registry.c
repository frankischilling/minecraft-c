#include "mc/item/registry/registry.h"

#include "../../../src/block/registry/registry_internal.h"
#include "../../../src/core/java/random_profile_internal.h"

#include <stdio.h>
#include <string.h>

static unsigned int checks;
static bool retirement_failed;

#define COUNT(array) (sizeof(array) / sizeof((array)[0]))
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #condition);                \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

/* Independent declaration selectors and arguments from paired gm/ej clinit:
 * the first104 raw arguments are0..103; only the two records use2000/2001.
 * These are unit expectations, not a production registry or an original dump. */
static const char *const names[] = {
    "d",  "e",  "f",  "g",  "h",  "i",  "j",  "k",  "l",  "m",  "n",  "o",  "p",  "q",  "r",  "s",
    "t",  "u",  "v",  "w",  "x",  "y",  "z",  "A",  "B",  "C",  "D",  "E",  "F",  "G",  "H",  "I",
    "J",  "K",  "L",  "M",  "N",  "O",  "P",  "Q",  "R",  "S",  "T",  "U",  "V",  "W",  "X",  "Y",
    "Z",  "aa", "ab", "ac", "ad", "ae", "af", "ag", "ah", "ai", "aj", "ak", "al", "am", "an", "ao",
    "ap", "aq", "ar", "as", "at", "au", "av", "aw", "ax", "ay", "az", "aA", "aB", "aC", "aD", "aE",
    "aF", "aG", "aH", "aI", "aJ", "aK", "aL", "aM", "aN", "aO", "aP", "aQ", "aR", "aS", "aT", "aU",
    "aV", "aW", "aX", "aY", "aZ", "ba", "bb", "bc", "bd", "be"};
_Static_assert(COUNT(names) == 106, "actual named Item declaration count");

static uint64_t uniquifier(const mc_java_random_profile *profile)
{
    return atomic_load_explicit(&profile->uniquifier, memory_order_relaxed);
}

static uint64_t advanced(uint64_t value, size_t count)
{
    while (count-- != 0)
        value *= UINT64_C(1181783497276652981);
    return value;
}

static bool text_is(const mc_registry_string *value, const char *expected)
{
    mc_registry_string_view view;
    if (!mc_registry_string_read(value, &view) || view.length != strlen(expected) ||
        (view.length != 0 && view.units == NULL))
        return false;
    for (size_t i = 0; i < view.length; ++i)
        if (view.units[i] != (uint16_t)(unsigned char)expected[i])
            return false;
    return true;
}

static const mc_item *item_at(const mc_registry_classes *owner, const char *name)
{
    const mc_item *value = NULL;
    if (mc_registry_classes_inspect_item_static(owner, name, &value, NULL) != MC_REGISTRY_OK)
        return NULL;
    return value;
}

static bool same_view(const mc_registry_item_view *a, const mc_registry_item_view *b)
{
    return a->phase == b->phase && a->initializer_pc == b->initializer_pc &&
           a->named_items == b->named_items && a->b == b->b && a->c == b->c;
}

static int retire(mc_registry_classes *owner, mc_registry_error *error)
{
    if (owner == NULL || mc_registry_classes_free(owner, error) == MC_REGISTRY_OK)
        return 0;
    fprintf(stderr, "class owner retirement refused; preserving borrowed dependencies\n");
    retirement_failed = true;
    return 1;
}

static int profile_is(const mc_java_random_profile *profile, uint64_t expected)
{
    int result = 1;
    CHECK(uniquifier(profile) == expected);
    CHECK(profile->math == NULL);
    CHECK(atomic_load_explicit(&profile->holder_phase, memory_order_relaxed) ==
          MC_JAVA_RANDOM_HOLDER_COLD);
    result = 0;
cleanup:
    return result;
}

static int invalid_reads(const mc_registry_classes *owner)
{
    int result = 1;
    mc_item sentinel = {0};
    const mc_item *out = &sentinel;
    mc_registry_error error;
    mc_registry_item_view before = {MC_REGISTRY_READY, 12345, 321, NULL, NULL};
    mc_registry_item_view view = before;
    size_t bytes = owner->allocation_bytes;
    uint64_t u = uniquifier(owner->options.random_profile);
    CHECK(mc_registry_classes_inspect_items(NULL, &view, &error) == MC_REGISTRY_INVALID_ARGUMENT);
    CHECK(same_view(&view, &before));
    CHECK(mc_registry_classes_inspect_items(owner, NULL, &error) == MC_REGISTRY_INVALID_ARGUMENT);
    CHECK(mc_registry_classes_inspect_item_static(NULL, "d", &out, &error) ==
          MC_REGISTRY_INVALID_ARGUMENT);
    CHECK(out == &sentinel);
    CHECK(mc_registry_classes_inspect_item_static(owner, NULL, &out, &error) ==
          MC_REGISTRY_INVALID_ARGUMENT);
    CHECK(out == &sentinel);
    CHECK(mc_registry_classes_inspect_item_static(owner, "missing", &out, &error) ==
          MC_REGISTRY_INVALID_ARGUMENT);
    CHECK(out == &sentinel);
    CHECK(mc_registry_classes_inspect_item_static(owner, "d", NULL, &error) ==
          MC_REGISTRY_INVALID_ARGUMENT);
    CHECK(mc_registry_item_array_get(NULL, 0, &out, &error) == MC_REGISTRY_INVALID_ARGUMENT);
    CHECK(out == &sentinel);
    CHECK(mc_registry_item_array_length(NULL) == 0);
    CHECK(mc_item_fields_get(NULL) == NULL);
    CHECK(mc_registry_classes_initialize_items(NULL, &error) == MC_REGISTRY_INVALID_ARGUMENT);
    if (owner->items.c != NULL) {
        CHECK(mc_registry_item_array_get(owner->items.c, 0, NULL, &error) ==
              MC_REGISTRY_INVALID_ARGUMENT);
        CHECK(mc_registry_item_array_get(owner->items.c, 32000, &out, &error) ==
              MC_REGISTRY_INDEX_OUT_OF_RANGE);
        CHECK(out == &sentinel);
        CHECK(mc_registry_item_array_get(owner->items.c, SIZE_MAX, &out, &error) ==
              MC_REGISTRY_INDEX_OUT_OF_RANGE);
        CHECK(out == &sentinel);
    }
    CHECK(owner->allocation_bytes == bytes);
    CHECK(profile_is(owner->options.random_profile, u) == 0);
    result = 0;
cleanup:
    return result;
}

static int test_cold(mc_registry_side side, mc_java_random_profile *profile,
                     mc_materials *materials)
{
    int result = 1;
    mc_registry_classes *owner = NULL;
    mc_registry_error error;
    const mc_registry_options options = {side, profile, materials, 0, NULL};
    uint64_t u = uniquifier(profile);
    CHECK(mc_registry_classes_allocate(&options, &owner, &error) == MC_REGISTRY_OK);
    mc_registry_item_view view;
    CHECK(mc_registry_classes_inspect_items(owner, &view, &error) == MC_REGISTRY_OK);
    CHECK(view.phase == MC_REGISTRY_COLD && view.initializer_pc == -1 && view.named_items == 0);
    CHECK(view.b == NULL && view.c == NULL);
    CHECK(owner->blocks_phase == MC_REGISTRY_COLD && owner->block_initializer_pc == -1);
    CHECK(owner->entered == 0 && owner->initializer == NULL);
    CHECK(owner->current_item == NULL && owner->current_block == NULL);
    CHECK(owner->item_tool_state.phase == MC_REGISTRY_COLD);
    CHECK(owner->items.tool_a == NULL && owner->items.tool_j == NULL);
    CHECK(owner->items.wc_bk == NULL && owner->items.au_bk == NULL && owner->items.ta_bk == NULL);
    for (size_t i = 0; i < COUNT(names); ++i) {
        const mc_item *value = NULL;
        CHECK(mc_registry_classes_inspect_item_static(owner, names[i], &value, &error) ==
              MC_REGISTRY_OK);
        CHECK(value == NULL);
    }
    for (size_t i = 0; i < MC_ITEM_KIND_COUNT; ++i)
        CHECK(owner->item_states[i].phase == MC_REGISTRY_COLD && owner->item_states[i].pc == -1);
    CHECK(invalid_reads(owner) == 0);
    CHECK(profile_is(profile, u) == 0);
    result = 0;
cleanup:
    if (retire(owner, &error) != 0)
        result = 1;
    return result;
}

static bool ordinary_randoms(const mc_registry_classes *owner, const mc_java_random *out[5])
{
    if (owner->blocks.m == NULL || owner->blocks.m[23] == NULL || owner->blocks.m[54] == NULL ||
        owner->blocks.m[61] == NULL || owner->blocks.m[62] == NULL)
        return false;
    if (owner->blocks.m[23]->kind != MC_BLOCK_XQ_PA || owner->blocks.m[54]->kind != MC_BLOCK_E_E ||
        owner->blocks.m[61]->kind != MC_BLOCK_TC_LZ || owner->blocks.m[62]->kind != MC_BLOCK_TC_LZ)
        return false;
    out[0] = owner->items.b;
    out[1] = owner->blocks.m[23]->subtype.xq.a;
    out[2] = owner->blocks.m[54]->subtype.e.a;
    out[3] = owner->blocks.m[61]->subtype.tc.a;
    out[4] = owner->blocks.m[62]->subtype.tc.a;
    for (size_t i = 0; i < 5; ++i)
        if (out[i] == NULL)
            return false;
    return true;
}

static int check_randoms(const mc_registry_classes *owner, uint64_t before)
{
    int result = 1;
    const mc_java_random *randoms[5];
    CHECK(ordinary_randoms(owner, randoms));
    CHECK(profile_is(owner->options.random_profile, advanced(before, 5)) == 0);
    for (size_t i = 0; i < COUNT(randoms); ++i) {
        uint64_t cache;
        CHECK(randoms[i] != NULL);
        CHECK(randoms[i]->state <= UINT64_C(0xffffffffffff));
        memcpy(&cache, &randoms[i]->next_next_gaussian, sizeof(cache));
        CHECK(cache == 0 && !randoms[i]->have_next_next_gaussian);
        for (size_t j = 0; j < i; ++j)
            CHECK(randoms[i] != randoms[j]);
    }
    result = 0;
cleanup:
    return result;
}

static int check_effective(const mc_registry_classes *owner, const mc_registry_block_array *array,
                           const char *const *fields, size_t count)
{
    int result = 1;
    CHECK(array != NULL && array->length == count && array->elements != NULL);
    for (size_t i = 0; i < count; ++i) {
        const mc_block *block = NULL;
        CHECK(mc_registry_classes_inspect_block_static(owner, fields[i], &block, NULL) ==
              MC_REGISTRY_OK);
        CHECK(block != NULL && array->elements[i] == block);
    }
    result = 0;
cleanup:
    return result;
}

static int check_tools(const mc_registry_classes *owner)
{
    int result = 1;
    static const char *const tool_names[] = {"WOOD", "STONE", "IRON", "EMERALD", "GOLD"};
    static const int32_t f[] = {0, 1, 2, 3, 0};
    static const int32_t g[] = {59, 131, 250, 1561, 32};
    static const float h[] = {2.0f, 4.0f, 6.0f, 8.0f, 12.0f};
    static const int32_t damage[] = {0, 1, 2, 3, 0};
    const mc_item_tool_material *tools[] = {owner->items.tool_a, owner->items.tool_b,
                                            owner->items.tool_c, owner->items.tool_d,
                                            owner->items.tool_e};
    static const char *const shovel_blocks[] = {"v", "w", "F", "G", "aT", "aV", "aX", "aB"};
    static const char *const pick_blocks[] = {"x",  "ak", "al", "u",  "R",  "ap", "I", "aj", "J",
                                              "ai", "H",  "ax", "ay", "aU", "bc", "O", "P"};
    static const char *const axe_blocks[] = {"y", "ao", "K", "av"};
    CHECK(owner->item_tool_state.phase == MC_REGISTRY_READY && owner->item_tool_state.pc == 134);
    CHECK(owner->items.tool_j != NULL);
    for (size_t i = 0; i < COUNT(tools); ++i) {
        CHECK(tools[i] != NULL && owner->items.tool_j[i] == tools[i]);
        CHECK(text_is(tools[i]->name, tool_names[i]) && tools[i]->ordinal == (int32_t)i);
        CHECK(tools[i]->f == f[i] && tools[i]->g == g[i] && tools[i]->h == h[i] &&
              tools[i]->i == damage[i]);
        for (size_t j = 0; j < i; ++j)
            CHECK(tools[i] != tools[j] && tools[i]->name != tools[j]->name);
    }
    CHECK(check_effective(owner, owner->items.wc_bk, shovel_blocks, COUNT(shovel_blocks)) == 0);
    CHECK(check_effective(owner, owner->items.au_bk, pick_blocks, COUNT(pick_blocks)) == 0);
    CHECK(check_effective(owner, owner->items.ta_bk, axe_blocks, COUNT(axe_blocks)) == 0);
    CHECK(owner->items.wc_bk != owner->items.au_bk && owner->items.wc_bk != owner->items.ta_bk &&
          owner->items.au_bk != owner->items.ta_bk);
    /* Iron and Wood shovels share the real class array, not tool identity. */
    const mc_item *iron = item_at(owner, "d"), *wood = item_at(owner, "q");
    const mc_item *pick = item_at(owner, "e"), *axe = item_at(owner, "f");
    CHECK(iron != NULL && wood != NULL && pick != NULL && axe != NULL);
    CHECK(iron->dq.bk == owner->items.wc_bk && wood->dq.bk == iron->dq.bk);
    CHECK(iron->dq.a == tools[2] && wood->dq.a == tools[0]);
    CHECK(iron->dq.bl == 6.0f && iron->dq.bm == 3 && iron->fields.a == 250);
    CHECK(wood->dq.bl == 2.0f && wood->dq.bm == 1 && wood->fields.a == 59);
    CHECK(pick->dq.bk == owner->items.au_bk && pick->dq.bm == 4);
    CHECK(axe->dq.bk == owner->items.ta_bk && axe->dq.bm == 5);
    CHECK(iron->fields.bl != NULL && iron->fields.bk == NULL);
    const mc_item *sword_iron = item_at(owner, "o"), *sword_wood = item_at(owner, "p");
    const mc_item *hoe_diamond = item_at(owner, "O"), *hoe_gold = item_at(owner, "P");
    CHECK(sword_iron != NULL && sword_wood != NULL && hoe_diamond != NULL && hoe_gold != NULL);
    CHECK(sword_iron->subtype.qd.a == 8 && sword_iron->fields.a == 250);
    CHECK(sword_wood->subtype.qd.a == 4 && sword_wood->fields.a == 59);
    CHECK(hoe_diamond->fields.a == 1561 && hoe_gold->fields.a == 32);
    result = 0;
cleanup:
    return result;
}

static int check_armor_dye(const mc_registry_classes *owner)
{
    int result = 1;
    static const int32_t protection[] = {3, 8, 6, 3};
    static const int32_t durability[] = {11, 16, 15, 13};
    static const char *const armor_fields[][4] = {{"T", "U", "V", "W"},
                                                  {"X", "Y", "Z", "aa"},
                                                  {"ab", "ac", "ad", "ae"},
                                                  {"af", "ag", "ah", "ai"},
                                                  {"aj", "ak", "al", "am"}};
    static const int32_t armor_material[] = {0, 1, 2, 3, 1};
    static const int32_t maxima[][4] = {{33, 48, 45, 39},
                                        {66, 96, 90, 78},
                                        {132, 192, 180, 156},
                                        {264, 384, 360, 312},
                                        {66, 96, 90, 78}};
    static const char *const dyes[] = {"black",     "red",     "green",  "brown", "blue", "purple",
                                       "cyan",      "silver",  "gray",   "pink",  "lime", "yellow",
                                       "lightBlue", "magenta", "orange", "white"};
    static const int32_t rgb[] = {1973019, 11743532, 3887386,  5320730,  2437522, 8073150,
                                  2651799, 2651799,  4408131,  14188952, 4312372, 14602026,
                                  6719955, 12801229, 15435844, 15790320};
    CHECK(owner->items.wa_bn != NULL && owner->items.wa_bo != NULL &&
          owner->items.wa_bn != owner->items.wa_bo);
    for (size_t i = 0; i < 4; ++i)
        CHECK(owner->items.wa_bn[i] == protection[i] && owner->items.wa_bo[i] == durability[i]);
    for (size_t group = 0; group < COUNT(armor_fields); ++group)
        for (size_t slot = 0; slot < 4; ++slot) {
            const mc_item *item = item_at(owner, armor_fields[group][slot]);
            CHECK(item != NULL && item->kind == MC_ITEM_WA_NX);
            CHECK(item->subtype.wa.a == armor_material[group] &&
                  item->subtype.wa.bk == (int32_t)slot);
            CHECK(item->subtype.wa.bm == (int32_t)group && item->subtype.wa.bl == protection[slot]);
            CHECK(item->fields.bg == 1 && item->fields.a == maxima[group][slot]);
        }
    CHECK(owner->items.km_a != NULL && owner->items.km_bk != NULL);
    for (size_t i = 0; i < COUNT(dyes); ++i) {
        CHECK(text_is(owner->items.km_a[i], dyes[i]) && owner->items.km_bk[i] == rgb[i]);
        for (size_t j = 0; j < i; ++j)
            CHECK(owner->items.km_a[i] != owner->items.km_a[j]);
    }
    CHECK(owner->items.km_bk[6] == owner->items.km_bk[7]);
    CHECK(owner->items.km_a[6] != owner->items.km_a[7]);
    CHECK(owner->item_states[MC_ITEM_WA_NX].pc == 50 &&
          owner->item_states[MC_ITEM_KM_GU].pc == 195);
    result = 0;
cleanup:
    return result;
}

typedef struct base_case {
    const char *field, *text;
    int32_t count, maximum, icon;
    bool flag, subtypes;
} base_case;

static int check_base_and_subtypes(const mc_registry_classes *owner, mc_materials *materials)
{
    int result = 1;
    /* Literal constructor/fluent outcomes, independently resolved through the
     * primary gm/ej base, selected descendants and registration instructions. */
    static const base_case cases[] = {{"d", "item.shovelIron", 1, 250, 82, false, false},
                                      {"e", "item.pickaxeIron", 1, 250, 98, false, false},
                                      {"f", "item.hatchetIron", 1, 250, 114, false, false},
                                      {"g", "item.flintAndSteel", 1, 64, 5, false, false},
                                      {"h", "item.apple", 1, 0, 10, false, false},
                                      {"i", "item.bow", 1, 0, 21, false, false},
                                      {"j", "item.arrow", 64, 0, 37, false, false},
                                      {"k", "item.coal", 64, 0, 7, false, true},
                                      {"m", "item.ingotIron", 64, 0, 23, false, false},
                                      {"B", "item.stick", 64, 0, 53, true, false},
                                      {"D", "item.mushroomStew", 1, 0, 72, false, false},
                                      {"Q", "item.seeds", 64, 0, 9, false, false},
                                      {"S", "item.bread", 1, 0, 41, false, false},
                                      {"aq", "item.painting", 64, 0, 26, false, false},
                                      {"ar", "item.appleGold", 1, 0, 11, false, false},
                                      {"as", "item.sign", 1, 0, 42, false, false},
                                      {"au", "item.bucket", 1, 0, 74, false, false},
                                      {"av", "item.bucketWater", 1, 0, 75, false, false},
                                      {"aw", "item.bucketLava", 1, 0, 76, false, false},
                                      {"ay", "item.saddle", 1, 0, 104, false, false},
                                      {"aA", "item.redstone", 64, 0, 56, false, false},
                                      {"aB", "item.snowball", 16, 0, 14, false, false},
                                      {"aC", "item.boat", 1, 0, 136, false, false},
                                      {"aE", "item.milk", 1, 0, 77, false, false},
                                      {"aN", "item.egg", 16, 0, 12, false, false},
                                      {"aP", "item.fishingRod", 1, 64, 69, false, false},
                                      {"aU", "item.dyePowder", 64, 0, 78, false, true},
                                      {"aV", "item.bone", 64, 0, 28, true, false},
                                      {"aW", "item.sugar", 64, 0, 13, true, false},
                                      {"aX", "item.cake", 1, 0, 29, false, false},
                                      {"aY", "item.bed", 1, 0, 45, false, false},
                                      {"aZ", "item.diode", 64, 0, 86, false, false},
                                      {"ba", "item.cookie", 8, 0, 92, false, false},
                                      {"bb", "item.map", 1, 0, 60, false, false},
                                      {"bc", "item.shears", 1, 238, 93, false, false},
                                      {"bd", "item.record", 1, 0, 240, false, false},
                                      {"be", "item.record", 1, 0, 241, false, false}};
    for (size_t i = 0; i < COUNT(cases); ++i) {
        const mc_item *item = item_at(owner, cases[i].field);
        CHECK(item != NULL);
        const mc_item_fields *f = mc_item_fields_get(item);
        CHECK(f == &item->fields);
        CHECK(f->bg == cases[i].count && f->a == cases[i].maximum && f->bh == cases[i].icon);
        CHECK(f->bi == cases[i].flag && f->bj == cases[i].subtypes);
        CHECK(text_is(f->bl, cases[i].text));
    }
    CHECK(item_at(owner, "h")->yw.a == 4 && !item_at(owner, "h")->yw.bk);
    CHECK(item_at(owner, "S")->yw.a == 5 && item_at(owner, "S")->fields.a == 0);
    CHECK(item_at(owner, "D")->yw.a == 10 && !item_at(owner, "D")->yw.bk);
    CHECK(item_at(owner, "ao")->yw.a == 3 && item_at(owner, "ao")->yw.bk);
    CHECK(item_at(owner, "ap")->yw.a == 8 && item_at(owner, "ap")->yw.bk);
    CHECK(item_at(owner, "ar")->yw.a == 42 && !item_at(owner, "ar")->yw.bk);
    CHECK(item_at(owner, "ba")->yw.a == 1 && !item_at(owner, "ba")->yw.bk);
    CHECK(item_at(owner, "at")->subtype.hu.a == mc_materials_get(materials, MC_MATERIAL_d));
    CHECK(item_at(owner, "az")->subtype.hu.a == mc_materials_get(materials, MC_MATERIAL_f));
    CHECK(item_at(owner, "au")->subtype.bc.a == 0 && item_at(owner, "au")->fields.bk == NULL);
    CHECK(item_at(owner, "av")->subtype.bc.a == 8 && item_at(owner, "aw")->subtype.bc.a == 10);
    CHECK(item_at(owner, "aE")->subtype.bc.a == -1);
    CHECK(item_at(owner, "av")->fields.bk == item_at(owner, "au"));
    CHECK(item_at(owner, "aw")->fields.bk == item_at(owner, "au"));
    CHECK(item_at(owner, "aE")->fields.bk == item_at(owner, "au"));
    CHECK(item_at(owner, "ax")->subtype.rl.a == 0 && item_at(owner, "aL")->subtype.rl.a == 1 &&
          item_at(owner, "aM")->subtype.rl.a == 2);
    CHECK(item_at(owner, "Q")->subtype.rk.a == 59 && item_at(owner, "Q")->fields.a == 0);
    CHECK(item_at(owner, "aH")->subtype.lv.a == 83 && item_at(owner, "aX")->subtype.lv.a == 92 &&
          item_at(owner, "aZ")->subtype.lv.a == 93);
    const mc_item *first = item_at(owner, "bd"), *second = item_at(owner, "be");
    CHECK(first != second && first->kind == MC_ITEM_TR_MJ && second->kind == MC_ITEM_TR_MJ);
    CHECK(text_is(first->subtype.tr.a, "13") && text_is(second->subtype.tr.a, "cat"));
    CHECK(first->subtype.tr.a != second->subtype.tr.a);
    CHECK(first->fields.bl != second->fields.bl && text_is(first->fields.bl, "item.record") &&
          text_is(second->fields.bl, "item.record"));
    CHECK(first->subtype.tr.a != first->fields.bl && second->subtype.tr.a != second->fields.bl);
    result = 0;
cleanup:
    return result;
}

static int check_statistics_prefix(const mc_registry_classes *owner, mc_registry_side side,
                                   const mc_registry_error *failure)
{
    int result = 1;
    mc_registry_statistic_base_view base;
    mc_registry_statistic_view statistics;
    mc_registry_translation_view translation;
    mc_registry_statistic_key_view keys;
    const char *singleton_class = side == MC_REGISTRY_CLIENT ? "nh" : "ii";
    CHECK(owner->options.resources == NULL);
    CHECK(failure->status == MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY);
    CHECK(failure->active_initializer_class != NULL &&
          strcmp(failure->active_initializer_class, singleton_class) == 0 &&
          failure->active_initializer_pc == 4);
    CHECK(failure->constructor_class != NULL &&
          strcmp(failure->constructor_class, singleton_class) == 0 &&
          failure->constructor_pc == 23 && failure->profile_status == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_registry_classes_inspect_statistic_base(owner, &base, NULL) == MC_REGISTRY_OK);
    CHECK(base.phase == MC_REGISTRY_READY && base.initializer_pc == 51);
    CHECK(base.b != NULL && base.c != NULL && base.b != base.c);
    CHECK(base.i != NULL && base.j != NULL && base.k != NULL && base.i != base.j &&
          base.i != base.k && base.j != base.k);
    CHECK(mc_registry_classes_inspect_statistics(owner, &statistics, NULL) == MC_REGISTRY_OK);
    CHECK(statistics.phase == MC_REGISTRY_FAILED && statistics.initializer_pc == 59);
    CHECK(owner->statistics.a != NULL && statistics.map_size == 0);
    const mc_statistic_list *lists[] = {statistics.b, statistics.c, statistics.d, statistics.e};
    for (size_t i = 0; i < COUNT(lists); ++i) {
        CHECK(lists[i] != NULL && mc_statistic_list_size(lists[i]) == 0);
        for (size_t j = 0; j < i; ++j)
            CHECK(lists[i] != lists[j]);
    }
    CHECK(statistics.named != NULL);
    for (size_t i = 0; i < 23; ++i)
        CHECK(statistics.named[i] == NULL);
    CHECK(statistics.C == NULL && statistics.D == NULL && statistics.E == NULL &&
          statistics.F == NULL);
    CHECK(!statistics.G && !statistics.H && statistics.duplicate_message == NULL);
    CHECK(mc_registry_classes_inspect_translation(owner, &translation, NULL) == MC_REGISTRY_OK);
    CHECK(translation.singleton_phase == MC_REGISTRY_FAILED &&
          translation.singleton_initializer_pc == 4);
    CHECK(translation.wrapper_phase == MC_REGISTRY_FAILED &&
          translation.wrapper_initializer_pc == 0);
    CHECK(translation.singleton_a == NULL && translation.wrapper_a == NULL);
    CHECK(owner->translations.pending != NULL && owner->translations.pending->constructor_pc == 23);
    CHECK(mc_registry_classes_inspect_statistic_keys(owner, &keys, NULL) == MC_REGISTRY_OK);
    CHECK(keys.phase == MC_REGISTRY_COLD && keys.initializer_pc == -1);
    CHECK(keys.a == NULL && keys.construction == NULL && !keys.map_published);
    CHECK(keys.constructor_pc == -1 && keys.caught_status == MC_REGISTRY_OK &&
          keys.caught_pc == -1);
    result = 0;
cleanup:
    return result;
}

static int check_retry(mc_registry_classes *owner)
{
    int result = 1;
    const mc_item *entries[32000];
    const mc_item *named[106];
    mc_registry_item_view before, after;
    mc_registry_error error;
    const mc_java_random *randoms[5];
    struct {
        const mc_java_random *owner;
        uint64_t state, cache_bits;
        bool have_cache;
    } snapshots[5];
    CHECK(ordinary_randoms(owner, randoms));
    for (size_t i = 0; i < COUNT(snapshots); ++i) {
        snapshots[i].owner = randoms[i];
        snapshots[i].state = randoms[i]->state;
        memcpy(&snapshots[i].cache_bits, &randoms[i]->next_next_gaussian,
               sizeof(snapshots[i].cache_bits));
        snapshots[i].have_cache = randoms[i]->have_next_next_gaussian;
    }
    CHECK(mc_registry_classes_inspect_items(owner, &before, &error) == MC_REGISTRY_OK);
    CHECK(before.c != NULL && before.c->length == COUNT(entries));
    for (size_t i = 0; i < COUNT(entries); ++i)
        entries[i] = before.c->elements[i];
    for (size_t i = 0; i < COUNT(named); ++i)
        named[i] = owner->items.named[i];
    size_t bytes = owner->allocation_bytes;
    mc_registry_allocation *allocations = owner->allocations;
    uint64_t u = uniquifier(owner->options.random_profile);
    int32_t block_pc = owner->block_initializer_pc;
    int32_t item_pc = owner->item_states[MC_ITEM_GM_EJ].pc;
    mc_registry_error item_failure = owner->item_states[MC_ITEM_GM_EJ].failure;
    mc_registry_error block_failure = owner->block_failure;
    CHECK(mc_registry_classes_initialize_items(owner, &error) == MC_REGISTRY_FAILED_INITIALIZATION);
    CHECK(error.status == MC_REGISTRY_FAILED_INITIALIZATION);
    CHECK(error.initializer_pc == item_failure.initializer_pc &&
          error.active_initializer_pc == item_failure.active_initializer_pc &&
          error.active_initializer_class == item_failure.active_initializer_class &&
          error.constructor_class == item_failure.constructor_class &&
          error.constructor_pc == item_failure.constructor_pc &&
          error.item_argument == item_failure.item_argument &&
          error.profile_status == item_failure.profile_status);
    CHECK(mc_registry_classes_initialize_blocks(owner, &error) ==
          MC_REGISTRY_FAILED_INITIALIZATION);
    CHECK(error.status == MC_REGISTRY_FAILED_INITIALIZATION);
    CHECK(error.initializer_pc == block_failure.initializer_pc &&
          error.active_initializer_pc == block_failure.active_initializer_pc &&
          error.active_initializer_class == block_failure.active_initializer_class &&
          error.constructor_class == block_failure.constructor_class &&
          error.constructor_pc == block_failure.constructor_pc &&
          error.item_argument == block_failure.item_argument &&
          error.profile_status == block_failure.profile_status);
    CHECK(mc_registry_classes_inspect_items(owner, &after, &error) == MC_REGISTRY_OK);
    CHECK(same_view(&before, &after));
    CHECK(owner->allocation_bytes == bytes && owner->allocations == allocations);
    CHECK(owner->block_initializer_pc == block_pc &&
          owner->item_states[MC_ITEM_GM_EJ].pc == item_pc);
    CHECK(owner->entered == 0 && owner->initializer == NULL && owner->current_item == NULL &&
          owner->current_block == NULL);
    for (size_t i = 0; i < COUNT(entries); ++i)
        CHECK(before.c->elements[i] == entries[i]);
    for (size_t i = 0; i < COUNT(named); ++i)
        CHECK(owner->items.named[i] == named[i]);
    CHECK(ordinary_randoms(owner, randoms));
    for (size_t i = 0; i < COUNT(snapshots); ++i) {
        uint64_t cache_bits;
        memcpy(&cache_bits, &randoms[i]->next_next_gaussian, sizeof(cache_bits));
        CHECK(randoms[i] == snapshots[i].owner && randoms[i]->state == snapshots[i].state);
        CHECK(cache_bits == snapshots[i].cache_bits &&
              randoms[i]->have_next_next_gaussian == snapshots[i].have_cache);
    }
    CHECK(profile_is(owner->options.random_profile, u) == 0);
    CHECK(check_statistics_prefix(owner, owner->options.side, &item_failure) == 0);
    result = 0;
cleanup:
    return result;
}

static int check_fire(const mc_registry_classes *owner, bool hook_reached)
{
    int result = 1;
    static const struct {
        size_t id;
        int32_t encouragement, flammability;
    } entries[] = {{5, 5, 20},   {85, 5, 20},   {53, 5, 20},   {17, 5, 5},  {18, 30, 60},
                   {47, 30, 20}, {46, 15, 100}, {31, 60, 100}, {35, 30, 60}};
    CHECK(owner->blocks.m != NULL && owner->blocks.m[51] != NULL);
    const mc_block *fire = owner->blocks.m[51];
    CHECK(fire->kind == MC_BLOCK_YQ_PT);
    CHECK(fire->subtype.yq.a != NULL && fire->subtype.yq.b != NULL &&
          fire->subtype.yq.a != fire->subtype.yq.b);
    for (size_t id = 0; id < 256; ++id) {
        int32_t encouragement = 0, flammability = 0;
        if (hook_reached)
            for (size_t i = 0; i < COUNT(entries); ++i)
                if (entries[i].id == id) {
                    encouragement = entries[i].encouragement;
                    flammability = entries[i].flammability;
                }
        CHECK(fire->subtype.yq.a[id] == encouragement && fire->subtype.yq.b[id] == flammability);
    }
    result = 0;
cleanup:
    return result;
}

static int test_block_first(mc_registry_side side, mc_java_random_profile *profile,
                            mc_materials *materials)
{
    int result = 1;
    mc_registry_classes *owner = NULL;
    mc_registry_error error;
    const mc_registry_options options = {side, profile, materials, 0, NULL};
    uint64_t before = uniquifier(profile);
    CHECK(mc_registry_classes_allocate(&options, &owner, &error) == MC_REGISTRY_OK);
    CHECK(mc_registry_classes_initialize_blocks(owner, &error) ==
          MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY);
    CHECK(error.status == MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY && error.initializer_pc == 3398);
    CHECK(check_statistics_prefix(owner, side, &error) == 0);
    mc_registry_item_view view;
    mc_registry_block_view blocks;
    CHECK(mc_registry_classes_inspect_items(owner, &view, &error) == MC_REGISTRY_OK);
    CHECK(mc_registry_classes_inspect_blocks(owner, &blocks, &error) == MC_REGISTRY_OK);
    CHECK(view.phase == MC_REGISTRY_FAILED && view.initializer_pc == 2664 &&
          view.named_items == 106);
    CHECK(blocks.phase == MC_REGISTRY_FAILED && blocks.initializer_pc == 3398 &&
          blocks.named_blocks == 96);
    CHECK(view.b != NULL && view.c != NULL && mc_registry_item_array_length(view.c) == 32000);
    CHECK(blocks.m != NULL && blocks.r != NULL);
    CHECK(!blocks.r[0]);
    for (size_t i = 0; i < COUNT(names); ++i) {
        const mc_item *named = NULL, *element = NULL;
        int32_t raw = i < 104 ? (int32_t)i : i == 104 ? 2000 : 2001;
        CHECK(mc_registry_classes_inspect_item_static(owner, names[i], &named, &error) ==
              MC_REGISTRY_OK);
        CHECK(named != NULL && named->classes == owner && named->phase == MC_ITEM_CONSTRUCTED);
        CHECK(named->requested_argument == raw && named->fields.bf == raw + 256);
        CHECK(mc_registry_item_array_get(view.c, (size_t)(raw + 256), &element, &error) ==
              MC_REGISTRY_OK);
        CHECK(element == named && owner->items.named[i] == named);
        for (size_t j = 0; j < i; ++j)
            CHECK(named != owner->items.named[j]);
    }
    for (size_t i = 0; i < 32000; ++i) {
        const mc_item *element = NULL;
        CHECK(mc_registry_item_array_get(view.c, i, &element, &error) == MC_REGISTRY_OK);
        CHECK((element != NULL) == ((i >= 256 && i <= 359) || i == 2256 || i == 2257));
    }
    CHECK(check_randoms(owner, before) == 0);
    CHECK(check_fire(owner, false) == 0);
    CHECK(check_tools(owner) == 0);
    CHECK(check_armor_dye(owner) == 0);
    CHECK(check_base_and_subtypes(owner, materials) == 0);
    CHECK(invalid_reads(owner) == 0);
    CHECK(check_retry(owner) == 0);
    result = 0;
cleanup:
    if (retire(owner, &error) != 0)
        result = 1;
    return result;
}

static int test_item_first(mc_registry_side side, mc_java_random_profile *profile,
                           mc_materials *materials)
{
    int result = 1;
    mc_registry_classes *owner = NULL;
    mc_registry_error error;
    const mc_registry_options options = {side, profile, materials, 0, NULL};
    static const struct {
        size_t id;
        mc_item_kind kind;
        const char *name;
        int32_t icon;
    } specials[] = {{35, MC_ITEM_BI_AP, "item.cloth", 64},
                    {17, MC_ITEM_GO_EK, "item.log", 20},
                    {44, MC_ITEM_EN_CW, "item.stoneSlab", 5},
                    {6, MC_ITEM_MR_IA, "item.sapling", 15},
                    {18, MC_ITEM_UY_ND, "item.leaves", 52},
                    {33, MC_ITEM_QR_KC, NULL, 107},
                    {29, MC_ITEM_QR_KC, NULL, 106}};
    /* Side2 dispatch literals, including fluid increments, delegated stairs,
     * workbench, pumpkin, cake and repeater metadata-zero branches. */
    static const struct {
        size_t id;
        int32_t icon;
    } ordinary_textures[] = {{8, 206}, {9, 206}, {10, 238}, {11, 238}, {23, 45},
                             {43, 5},  {53, 4},  {54, 26},  {58, 60},  {61, 45},
                             {62, 45}, {67, 16}, {86, 118}, {91, 118}, {92, 122},
                             {93, 5},  {94, 5},  {95, 26},  {96, 84}};
    uint64_t before = uniquifier(profile);
    CHECK(mc_registry_classes_allocate(&options, &owner, &error) == MC_REGISTRY_OK);
    CHECK(mc_registry_classes_initialize_items(owner, &error) ==
          MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY);
    CHECK(error.status == MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY && error.initializer_pc == 3674);
    CHECK(check_statistics_prefix(owner, side, &error) == 0);
    mc_registry_item_view view;
    mc_registry_block_view blocks;
    CHECK(mc_registry_classes_inspect_items(owner, &view, &error) == MC_REGISTRY_OK);
    CHECK(mc_registry_classes_inspect_blocks(owner, &blocks, &error) == MC_REGISTRY_OK);
    CHECK(view.phase == MC_REGISTRY_FAILED && view.initializer_pc == 19 && view.named_items == 0);
    CHECK(blocks.phase == MC_REGISTRY_FAILED && blocks.initializer_pc == 3674 &&
          blocks.named_blocks == 96);
    CHECK(view.c != NULL && view.b != NULL && mc_registry_item_array_length(view.c) == 32000);
    CHECK(blocks.m != NULL && blocks.r != NULL);
    CHECK(blocks.r[0]);
    CHECK(owner->item_states[MC_ITEM_WC_NY].phase == MC_REGISTRY_FAILED &&
          owner->item_states[MC_ITEM_WC_NY].pc == 7);
    CHECK(owner->items.wc_bk == NULL && owner->items.au_bk == NULL && owner->items.ta_bk == NULL);
    CHECK(owner->item_tool_state.phase == MC_REGISTRY_COLD && owner->item_tool_state.pc == -1);
    CHECK(owner->items.tool_a == NULL && owner->items.tool_b == NULL &&
          owner->items.tool_c == NULL && owner->items.tool_d == NULL &&
          owner->items.tool_e == NULL && owner->items.tool_j == NULL);
    CHECK(owner->items.wa_bn == NULL && owner->items.wa_bo == NULL && owner->items.km_a == NULL &&
          owner->items.km_bk == NULL);
    for (size_t i = 0; i < COUNT(names); ++i) {
        const mc_item *named = NULL;
        CHECK(mc_registry_classes_inspect_item_static(owner, names[i], &named, &error) ==
              MC_REGISTRY_OK);
        CHECK(named == NULL);
    }
    for (size_t i = 0; i < 32000; ++i) {
        const mc_item *element = NULL;
        CHECK(mc_registry_item_array_get(view.c, i, &element, &error) == MC_REGISTRY_OK);
        CHECK((element != NULL) == (i >= 1 && i <= 96));
        if (element == NULL)
            continue;
        CHECK(element->classes == owner && element->phase == MC_ITEM_CONSTRUCTED);
        CHECK(element->fields.bf == (int32_t)i && element->ck_a == (int32_t)i);
        CHECK(element->requested_argument == (int32_t)i - 256);
        CHECK(element->fields.bg == 64 && element->fields.a == 0 && !element->fields.bi &&
              element->fields.bk == NULL);
        bool special = false;
        for (size_t j = 0; j < COUNT(specials); ++j) {
            if (specials[j].id != i)
                continue;
            special = true;
            CHECK(element->kind == specials[j].kind && element->fields.bh == specials[j].icon);
            CHECK(element->fields.bj == (j < 5));
            if (specials[j].name != NULL)
                CHECK(text_is(element->fields.bl, specials[j].name));
            else
                CHECK(element->fields.bl == NULL);
        }
        if (!special)
            CHECK(element->kind == MC_ITEM_CK_BK && !element->fields.bj &&
                  element->fields.bl == NULL);
    }
    for (size_t i = 0; i < COUNT(ordinary_textures); ++i) {
        const mc_item *item = view.c->elements[ordinary_textures[i].id];
        CHECK(item != NULL && item->kind == MC_ITEM_CK_BK);
        CHECK(item->fields.bh == ordinary_textures[i].icon);
    }
    /* Fire's real conditional post-Item hook runs during the default loop. */
    CHECK(check_fire(owner, true) == 0);
    CHECK(check_randoms(owner, before) == 0);
    CHECK(invalid_reads(owner) == 0);
    CHECK(check_retry(owner) == 0);
    result = 0;
cleanup:
    if (retire(owner, &error) != 0)
        result = 1;
    return result;
}

static int test_metadata_budget(mc_registry_side side, mc_java_random_profile *profile,
                                mc_materials *materials)
{
    int result = 1;
    mc_registry_classes *owner = NULL;
    mc_registry_error error;
    /* An actual public owner budget; no allocator hook or state mutation. */
    const mc_registry_options options = {side, profile, materials, sizeof(mc_registry_classes),
                                         NULL};
    uint64_t u = uniquifier(profile);
    CHECK(mc_registry_classes_allocate(&options, &owner, &error) == MC_REGISTRY_OK);
    CHECK(owner->allocation_bytes == sizeof(mc_registry_classes));
    CHECK(mc_registry_classes_initialize_items(owner, &error) == MC_REGISTRY_LIMIT);
    CHECK(error.status == MC_REGISTRY_LIMIT && error.active_initializer_pc == 0);
    CHECK(owner->item_states[MC_ITEM_GM_EJ].phase == MC_REGISTRY_FAILED &&
          owner->item_states[MC_ITEM_GM_EJ].pc == 0);
    CHECK(owner->blocks_phase == MC_REGISTRY_COLD && owner->items.b == NULL &&
          owner->items.c == NULL);
    CHECK(owner->allocation_bytes == sizeof(mc_registry_classes));
    CHECK(profile_is(profile, u) == 0);
    CHECK(mc_registry_classes_initialize_items(owner, &error) == MC_REGISTRY_FAILED_INITIALIZATION);
    CHECK(profile_is(profile, u) == 0);
    result = 0;
cleanup:
    if (retire(owner, &error) != 0)
        result = 1;
    return result;
}

static int published_random_count(const mc_registry_classes *owner, size_t *out)
{
    int result = 1;
    size_t count = owner->items.b != NULL ? 1u : 0u;
    static const size_t ids[] = {23, 54, 61, 62};
    static const mc_block_kind kinds[] = {MC_BLOCK_XQ_PA, MC_BLOCK_E_E, MC_BLOCK_TC_LZ,
                                          MC_BLOCK_TC_LZ};
    if (owner->blocks.m != NULL)
        for (size_t i = 0; i < COUNT(ids); ++i) {
            const mc_block *block = owner->blocks.m[ids[i]];
            if (block == NULL)
                continue;
            CHECK(block->kind == kinds[i]);
            const mc_java_random *random = i == 0   ? block->subtype.xq.a
                                           : i == 1 ? block->subtype.e.a
                                                    : block->subtype.tc.a;
            if (random != NULL)
                ++count;
        }
    *out = count;
    result = 0;
cleanup:
    return result;
}

/* Every trial really initializes its own bounded class owner on the SAME live
 * profile. Account genuine successful constructor publications, including any
 * Block recursion reached with a larger budget; never restore/reseed U. */
static int budget_trial(mc_registry_side side, mc_java_random_profile *profile,
                        mc_materials *materials, size_t budget, bool *published)
{
    int result = 1;
    mc_registry_classes *owner = NULL;
    mc_registry_error error;
    const mc_registry_options options = {side, profile, materials, budget, NULL};
    uint64_t before = uniquifier(profile);
    mc_registry_status status = mc_registry_classes_allocate(&options, &owner, &error);
    if (status == MC_REGISTRY_LIMIT) {
        CHECK(owner == NULL);
        CHECK(profile_is(profile, before) == 0);
        *published = false;
    } else {
        CHECK(status == MC_REGISTRY_OK && owner != NULL);
        status = mc_registry_classes_initialize_items(owner, &error);
        /* Unexpected clock/profile/system-allocation failures are not inferred
         * to have a count from static pointers and cannot qualify this trial. */
        CHECK(status == MC_REGISTRY_LIMIT || status == MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY);
        if (status == MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY) {
            CHECK(error.initializer_pc == 3674);
            CHECK(check_statistics_prefix(owner, side, &error) == 0);
        } else {
            CHECK(error.status == MC_REGISTRY_LIMIT);
        }
        CHECK(error.profile_status == MC_JAVA_RANDOM_PROFILE_OK);
        size_t constructors = 0;
        CHECK(published_random_count(owner, &constructors) == 0);
        CHECK(profile_is(profile, advanced(before, constructors)) == 0);
        *published = owner->items.c != NULL;
        if (*published)
            CHECK(mc_registry_item_array_length(owner->items.c) == 32000);
        CHECK(owner->entered == 0 && owner->initializer == NULL);
    }
    result = 0;
cleanup:
    if (retire(owner, &error) != 0)
        result = 1;
    return result;
}

static int test_array_publication_budget(mc_registry_side side, mc_java_random_profile *profile,
                                         mc_materials *materials)
{
    int result = 1;
    mc_registry_classes *owner = NULL;
    mc_registry_error error;
    bool published = false;
    size_t low = 1, high = 4u * 1024u * 1024u;
    CHECK(budget_trial(side, profile, materials, high, &published) == 0);
    CHECK(published);
    while (low < high) {
        size_t middle = low + (high - low) / 2;
        CHECK(budget_trial(side, profile, materials, middle, &published) == 0);
        if (published)
            high = middle;
        else
            low = middle + 1;
    }
    CHECK(low > sizeof(mc_registry_classes));
    uint64_t before = uniquifier(profile);
    mc_registry_options options = {side, profile, materials, low - 1, NULL};
    CHECK(mc_registry_classes_allocate(&options, &owner, &error) == MC_REGISTRY_OK);
    CHECK(mc_registry_classes_initialize_items(owner, &error) == MC_REGISTRY_LIMIT);
    CHECK(error.active_initializer_pc == 13 && error.profile_status == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(owner->item_states[MC_ITEM_GM_EJ].phase == MC_REGISTRY_FAILED &&
          owner->item_states[MC_ITEM_GM_EJ].pc == 13);
    CHECK(owner->items.b != NULL && owner->items.c == NULL && owner->items.named_items == 0);
    CHECK(owner->blocks_phase == MC_REGISTRY_COLD && owner->blocks.m == NULL);
    CHECK(owner->item_states[MC_ITEM_WC_NY].phase == MC_REGISTRY_COLD);
    CHECK(profile_is(profile, advanced(before, 1)) == 0);
    if (retire(owner, &error) != 0)
        goto cleanup;
    owner = NULL;

    before = uniquifier(profile);
    options.max_allocation_bytes = low;
    CHECK(mc_registry_classes_allocate(&options, &owner, &error) == MC_REGISTRY_OK);
    CHECK(mc_registry_classes_initialize_items(owner, &error) == MC_REGISTRY_LIMIT);
    CHECK(error.active_initializer_pc == 2 && error.active_initializer_class != NULL &&
          strcmp(error.active_initializer_class, side == MC_REGISTRY_CLIENT ? "wc" : "ny") == 0);
    CHECK(owner->allocation_bytes == low);
    CHECK(owner->item_states[MC_ITEM_GM_EJ].phase == MC_REGISTRY_FAILED &&
          owner->item_states[MC_ITEM_GM_EJ].pc == 19);
    CHECK(owner->item_states[MC_ITEM_WC_NY].phase == MC_REGISTRY_FAILED &&
          owner->item_states[MC_ITEM_WC_NY].pc == 2);
    CHECK(owner->items.b != NULL && owner->items.c != NULL && owner->items.wc_bk == NULL);
    CHECK(owner->items.named_items == 0 && owner->items.c->length == 32000);
    CHECK(owner->blocks_phase == MC_REGISTRY_COLD && owner->block_initializer_pc == -1 &&
          owner->blocks.m == NULL && owner->item_tool_state.phase == MC_REGISTRY_COLD);
    CHECK(profile_is(profile, advanced(before, 1)) == 0);
    for (size_t i = 0; i < 32000; ++i) {
        const mc_item *item = NULL;
        CHECK(mc_registry_item_array_get(owner->items.c, i, &item, &error) == MC_REGISTRY_OK);
        CHECK(item == NULL);
    }
    for (size_t i = 0; i < COUNT(names); ++i) {
        const mc_item *item = NULL;
        CHECK(mc_registry_classes_inspect_item_static(owner, names[i], &item, &error) ==
              MC_REGISTRY_OK);
        CHECK(item == NULL);
    }
    mc_registry_item_array *captured = owner->items.c;
    CHECK(mc_registry_classes_initialize_items(owner, &error) == MC_REGISTRY_FAILED_INITIALIZATION);
    CHECK(owner->items.c == captured && owner->allocation_bytes == low);
    CHECK(profile_is(profile, advanced(before, 1)) == 0);
    result = 0;
cleanup:
    if (retire(owner, &error) != 0)
        result = 1;
    return result;
}

static int test_side(mc_registry_side side, mc_java_random_profile *profile,
                     mc_materials *materials)
{
    int result = 1;
    uint64_t before = uniquifier(profile);
    CHECK(test_cold(side, profile, materials) == 0);
    CHECK(test_metadata_budget(side, profile, materials) == 0);
    CHECK(profile_is(profile, before) == 0);
    CHECK(test_block_first(side, profile, materials) == 0);
    CHECK(profile_is(profile, advanced(before, 5)) == 0);
    CHECK(test_item_first(side, profile, materials) == 0);
    CHECK(profile_is(profile, advanced(before, 10)) == 0);
    CHECK(test_array_publication_budget(side, profile, materials) == 0);
    result = 0;
cleanup:
    return result;
}

int main(int argc, char **argv)
{
    int result = 1;
    mc_java_random_profile *profile = NULL;
    mc_materials *materials = NULL;
    bool client = argc == 1 || (argc == 2 && strcmp(argv[1], "client") == 0);
    bool server = argc == 1 || (argc == 2 && strcmp(argv[1], "server") == 0);
    if (!client && !server) {
        fprintf(stderr, "usage: %s [client|server]\n", argv[0]);
        goto cleanup;
    }
    CHECK(mc_java_random_profile_create(SIZE_MAX, &profile) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_materials_create(&materials));
    size_t profile_bytes = mc_java_random_profile_allocation_bytes(profile);
    if (client)
        CHECK(test_side(MC_REGISTRY_CLIENT, profile, materials) == 0);
    if (server)
        CHECK(test_side(MC_REGISTRY_SERVER, profile, materials) == 0);
    CHECK(mc_java_random_profile_allocation_bytes(profile) == profile_bytes);
    printf("PASS: %u Item/Block initialization-prefix checks\n", checks);
    result = 0;
cleanup:
    if (!retirement_failed) {
        mc_materials_destroy(materials);
        mc_java_random_profile_free(profile);
    }
    return result;
}
