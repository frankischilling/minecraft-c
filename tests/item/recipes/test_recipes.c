#include "mc/item/recipes/recipes.h"
#include "mc/resources/translation/translation.h"
#include "mc/statistics/registry/registry.h"

#include "../../../src/block/registry/registry_internal.h"
#include "../../../src/core/java/random_profile_internal.h"
#include "../../../src/item/registry/stacks_internal.h"

#include <stdio.h>
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

static const uint8_t english[] =
    "stat.mineBlock=Mine %s\nstat.useItem=Use %1$s\nstat.breakItem=Break %s\n"
    "stat.craftItem=Craft %1$s\nrow.a=A\nrow.aa=AA\nrow.ab=AB\nrow.b=B\n"
    "row.empty=\nrow.unicode=\\u0000\\ud800A\\udc00\n";

typedef struct stream {
    const uint8_t *bytes;
    size_t length, offset, opens, reads, closes;
    bool closed;
} stream;

typedef struct fixture {
    mc_registry_classes *owner;
    mc_registry_resources source;
    stream streams[3];
    bool invalid;
} fixture;

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
    for (size_t i = 0; i < COUNT(names); ++i)
        if (name != NULL && strcmp(name, names[i]) == 0 && out != NULL) {
            if (++f->streams[i].opens != 1)
                f->invalid = true;
            if (f->owner == NULL ||
                mc_registry_classes_initialize_crafting(f->owner, NULL) != MC_REGISTRY_BUSY ||
                mc_registry_classes_initialize_smelting(f->owner, NULL) != MC_REGISTRY_BUSY ||
                mc_registry_classes_free(f->owner, NULL) != MC_REGISTRY_BUSY)
                f->invalid = true;
            *out = (mc_java_properties_input){&f->streams[i], read_bytes};
            return MC_REGISTRY_OK;
        }
    f->invalid = true;
    return MC_REGISTRY_INVALID_ARGUMENT;
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
    return false; /* Actual available0 profile, without consuming a byte. */
}

static mc_registry_status create(fixture *f, mc_registry_side side, mc_materials *materials,
                                 mc_java_random_profile *profile, size_t budget) {
    *f = (fixture){0};
    f->streams[0] = (stream){english, sizeof(english) - 1, 0, 0, 0, 0, false};
    f->streams[1] = (stream){english, 0, 0, 0, 0, 0, false};
    f->streams[2] = (stream){english, 0, 0, 0, 0, 0, false};
    f->source = (mc_registry_resources){f, open_resource, close_resource, ready_resource};
    const mc_registry_options options = {side, profile, materials, budget, &f->source};
    return mc_registry_classes_allocate(&options, &f->owner, NULL);
}

static bool retire(fixture *f) {
    if (f->owner == NULL)
        return true;
    f->owner->entered = 0;
    if (mc_registry_classes_free(f->owner, NULL) != MC_REGISTRY_OK) {
        retirement_failed = true;
        fputs("Recipe fixture retirement refused; dependencies retained\n", stderr);
        return false;
    }
    f->owner = NULL;
    return true;
}

static uint64_t uniquifier(const mc_java_random_profile *profile) {
    return atomic_load_explicit(&profile->uniquifier, memory_order_relaxed);
}

static uint64_t advance_five(uint64_t value) {
    for (int i = 0; i < 5; ++i)
        value *= UINT64_C(1181783497276652981);
    return value;
}

static bool fields_are(const mc_item_stack *stack, int32_t id, int32_t count, int32_t damage) {
    mc_item_stack_fields f;
    return mc_item_stack_fields_get(stack, &f) == MC_ITEM_STACK_OK && f.id == id &&
           f.count == count && f.damage == damage && f.animation == 0;
}

static bool same_cause(const mc_registry_error *a, const mc_registry_error *b) {
    return a->initializer_pc == b->initializer_pc && a->block_id == b->block_id &&
           a->constructor_class == b->constructor_class && a->constructor_pc == b->constructor_pc &&
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

static bool same_error(const mc_registry_error *a, const mc_registry_error *b) {
    return a->status == b->status && a->message == b->message && same_cause(a, b);
}

static const mc_registry_string *property(mc_registry_classes *owner, const char *key) {
    uint16_t units[64];
    size_t length = strlen(key);
    if (length > COUNT(units))
        return NULL;
    for (size_t i = 0; i < length; ++i)
        units[i] = (unsigned char)key[i];
    const mc_registry_string *out = NULL;
    return mc_translation_property(owner->translations.singleton_a, units, length, &out, NULL) ==
                   MC_REGISTRY_OK
               ? out
               : NULL;
}

static bool starts_with(const mc_registry_string *string, const char *prefix) {
    mc_registry_string_view view;
    size_t n = strlen(prefix);
    if (!mc_registry_string_read(string, &view) || view.length < n)
        return false;
    for (size_t i = 0; i < n; ++i)
        if (view.units[i] != (unsigned char)prefix[i])
            return false;
    return true;
}

static const mc_recipe *at(const mc_crafting_manager *manager, size_t index) {
    const mc_recipe *out = NULL;
    return mc_crafting_manager_get(manager, index, &out) == MC_REGISTRY_OK ? out : NULL;
}

static mc_item_stack *new_stack(mc_registry_classes *owner, int32_t id, int32_t count,
                                int32_t damage) {
    mc_item_stack *out = NULL;
    if (mc_registry_stack_allocate(owner, &out) != MC_REGISTRY_OK ||
        mc_registry_stack_construct_numeric(owner, out, id, count, damage) != MC_REGISTRY_OK)
        return NULL;
    return out; /* Actual class ledger owns the hold through every failure. */
}

/* No expected numeric catalog. Enroll actual owners while checking distinct
 * allocations and the independent source invariant that ingredients are clones. */
typedef struct identity_set {
    const mc_item_stack *objects[2048];
    size_t count;
} identity_set;

static bool enroll(identity_set *set, const mc_item_stack *stack) {
    if (stack == NULL || set->count == COUNT(set->objects))
        return false;
    mc_item_stack_fields fields;
    mc_item_stack_phase phase;
    if (mc_item_stack_fields_get(stack, &fields) != MC_ITEM_STACK_OK || fields.animation != 0 ||
        mc_item_stack_phase_get(stack, &phase) != MC_ITEM_STACK_OK ||
        phase != MC_ITEM_STACK_READY || mc_item_stack_source(stack) != NULL)
        return false;
    for (size_t i = 0; i < set->count; ++i)
        if (set->objects[i] == stack)
            return false;
    set->objects[set->count++] = stack;
    return true;
}

static bool inspect_recipe(const mc_recipe *recipe, identity_set *identities, bool *ids,
                           int32_t *size) {
    mc_item_stack_fields output;
    const mc_item_stack *stack = mc_recipe_output(recipe);
    if (!enroll(identities, stack) ||
        mc_item_stack_fields_get(stack, &output) != MC_ITEM_STACK_OK || output.id < 0 ||
        output.id >= 32000 || output.animation != 0 || mc_item_stack_source(stack) != NULL ||
        mc_recipe_size(recipe, size) != MC_REGISTRY_OK)
        return false;
    ids[(size_t)output.id] = true;
    if (mc_recipe_kind_get(recipe) == MC_RECIPE_SHAPED) {
        const mc_shaped_recipe_fields *s = mc_recipe_shaped_fields_get(recipe);
        if (s == NULL || mc_recipe_shapeless_fields_get(recipe) != NULL || s->a != output.id ||
            s->e != stack || s->b < 0 || s->c < 0 || (int64_t)s->b * (int64_t)s->c > INT32_MAX ||
            (int64_t)*size != (int64_t)s->b * (int64_t)s->c ||
            mc_recipe_shaped_array_length(recipe) != (size_t)*size)
            return false;
        for (size_t i = 0; i < mc_recipe_shaped_array_length(recipe); ++i)
            if (s->d[i] != NULL && !enroll(identities, s->d[i]))
                return false;
    } else {
        const mc_shapeless_recipe_fields *s = mc_recipe_shapeless_fields_get(recipe);
        if (s == NULL || mc_recipe_shaped_fields_get(recipe) != NULL || s->a != stack ||
            *size < 0 || mc_recipe_ingredient_list_size(s->b) != (size_t)*size)
            return false;
        for (size_t i = 0; i < mc_recipe_ingredient_list_size(s->b); ++i) {
            const mc_item_stack *ingredient = NULL;
            if (mc_recipe_ingredient_list_get(s->b, i, &ingredient) != MC_REGISTRY_OK ||
                !enroll(identities, ingredient))
                return false;
        }
    }
    return true;
}

/* Literal paired ey/de constructor selectors. IDs come from live native
 * Item/Block objects; no familiar Minecraft numeric ID is an expectation. */
static const struct smelt_case {
    bool key_block;
    const char *key;
    bool output_block;
    const char *output;
    int32_t damage;
} smelts[] = {{true, "I", false, "m", 0},    {true, "H", false, "n", 0},
              {true, "ax", false, "l", 0},   {true, "F", true, "N", 0},
              {false, "ao", false, "ap", 0}, {false, "aS", false, "aT", 0},
              {true, "x", true, "u", 0},     {false, "aG", false, "aF", 0},
              {true, "aW", false, "aU", 2},  {true, "K", false, "k", 1}};

static bool selected_id(mc_registry_classes *owner, bool block, const char *name, int32_t *id) {
    if (block) {
        const mc_block *object = NULL;
        if (mc_registry_classes_inspect_block_static(owner, name, &object, NULL) !=
                MC_REGISTRY_OK ||
            object == NULL)
            return false;
        *id = mc_block_fields_get(object)->bn;
    } else {
        const mc_item *object = NULL;
        if (mc_registry_classes_inspect_item_static(owner, name, &object, NULL) != MC_REGISTRY_OK ||
            object == NULL)
            return false;
        *id = mc_item_fields_get(object)->bf;
    }
    return true;
}

static int bootstrap(mc_registry_side side, mc_materials *materials,
                     mc_java_random_profile *profile) {
    int result = 1;
    fixture f = {0};
    identity_set identities = {0};
    bool ids[32000] = {false};
    mc_registry_crafting_view craft;
    mc_registry_smelting_view smelt;
    mc_registry_statistic_view stats;
    mc_registry_statistic_base_view base;
    uint64_t initial = uniquifier(profile);
    CHECK(create(&f, side, materials, profile, 0) == MC_REGISTRY_OK);
    CHECK(mc_registry_classes_initialize_blocks(f.owner, NULL) == MC_REGISTRY_OK);
    CHECK(mc_registry_classes_inspect_crafting(f.owner, &craft, NULL) == MC_REGISTRY_OK &&
          craft.phase == MC_REGISTRY_READY && craft.a != NULL);
    CHECK(mc_registry_classes_inspect_smelting(f.owner, &smelt, NULL) == MC_REGISTRY_OK &&
          smelt.phase == MC_REGISTRY_READY && smelt.a != NULL);
    CHECK(mc_crafting_manager_size(craft.a) == 151 && mc_smelting_manager_size(smelt.a) == 10);
    int32_t previous_size = INT32_MAX;
    for (size_t i = 0; i < 151; ++i) {
        const mc_recipe *recipe = at(craft.a, i);
        int32_t size;
        CHECK(recipe != NULL && inspect_recipe(recipe, &identities, ids, &size));
        CHECK(mc_recipe_kind_get(recipe) == (i < 120 ? MC_RECIPE_SHAPED : MC_RECIPE_SHAPELESS));
        if (i == 120)
            previous_size = INT32_MAX;
        CHECK(size <= previous_size);
        previous_size = size;
    }
    int32_t keys[10];
    size_t key_count = 0;
    CHECK(mc_smelting_manager_keys(smelt.a, keys, COUNT(keys), &key_count) == MC_REGISTRY_OK &&
          key_count == 10);
    for (size_t i = 0; i < COUNT(smelts); ++i) {
        int32_t key, output;
        const mc_item_stack *value = NULL;
        CHECK(selected_id(f.owner, smelts[i].key_block, smelts[i].key, &key) &&
              selected_id(f.owner, smelts[i].output_block, smelts[i].output, &output));
        CHECK(mc_smelting_manager_find(smelt.a, key, &value) == MC_REGISTRY_OK &&
              fields_are(value, output, 1, smelts[i].damage) && enroll(&identities, value));
        bool found = false;
        for (size_t j = 0; j < key_count; ++j)
            if (keys[j] == key)
                found = true;
        CHECK(found && output >= 0 && output < 32000);
        ids[(size_t)output] = true;
    }
    CHECK(mc_registry_classes_inspect_statistics(f.owner, &stats, NULL) == MC_REGISTRY_OK &&
          stats.phase == MC_REGISTRY_READY && stats.G && stats.H && stats.D != NULL);
    CHECK(mc_registry_classes_inspect_statistic_base(f.owner, &base, NULL) == MC_REGISTRY_OK &&
          base.phase == MC_REGISTRY_READY);
    size_t covered = 0;
    for (size_t id = 0; id < COUNT(ids); ++id) {
        if (ids[id]) {
            const mc_item *item = NULL;
            mc_registry_item_view view;
            CHECK(mc_registry_classes_inspect_items(f.owner, &view, NULL) == MC_REGISTRY_OK &&
                  mc_registry_item_array_get(view.c, id, &item, NULL) == MC_REGISTRY_OK);
            if (item != NULL)
                CHECK(stats.D[id] != NULL);
        }
        if (stats.D[id] != NULL) {
            int32_t associated;
            const mc_statistic *found = NULL;
            const mc_statistic_fields *fields = mc_statistic_fields_get(stats.D[id]);
            CHECK(mc_statistic_kind_get(stats.D[id]) == MC_STATISTIC_ITEM && fields != NULL &&
                  mc_statistic_item_id(stats.D[id], &associated) == MC_REGISTRY_OK &&
                  associated >= 0 && associated < 32000 && fields->e == 16842752 + associated &&
                  !fields->g && fields->a == base.i && starts_with(fields->f, "Craft "));
            CHECK(mc_registry_classes_find_statistic(f.owner, fields->e, &found) ==
                      MC_REGISTRY_OK &&
                  found == stats.D[id]);
            ++covered;
        }
    }
    CHECK(covered != 0 && !f.invalid && f.streams[2].closes == 1 &&
          uniquifier(profile) == advance_five(initial) && profile->math == NULL);
    size_t bytes = f.owner->allocation_bytes;
    CHECK(mc_registry_classes_initialize_crafting(f.owner, NULL) == MC_REGISTRY_OK &&
          mc_registry_classes_initialize_smelting(f.owner, NULL) == MC_REGISTRY_OK &&
          f.owner->allocation_bytes == bytes && uniquifier(profile) == advance_five(initial));
    result = 0;
cleanup:
    if (!retire(&f))
        result = 1;
    return result;
}

static mc_registry_status enter_route(fixture *f, int route, mc_registry_error *error) {
    return route == 0   ? mc_registry_classes_initialize_items(f->owner, error)
           : route == 1 ? mc_registry_classes_initialize_crafting(f->owner, error)
                        : mc_registry_classes_initialize_smelting(f->owner, error);
}

static int entry_failures(mc_registry_side side, mc_materials *materials,
                          mc_java_random_profile *profile, int route) {
    int result = 1;
    fixture f = {0};
    mc_registry_error first, retry;
    mc_registry_crafting_view craft;
    mc_registry_smelting_view smelt;
    CHECK(create(&f, side, materials, profile, 0) == MC_REGISTRY_OK);
    CHECK(enter_route(&f, route, &first) == MC_REGISTRY_NULL_REFERENCE);
    CHECK(first.constructor_pc == (route == 0 ? 2 : route == 1 ? 24 : 75));
    const char *class_name = route == 0 ? (side == MC_REGISTRY_CLIENT ? "iz" : "fy")
                                        : (side == MC_REGISTRY_CLIENT ? "jl" : "gg");
    CHECK(first.constructor_class != NULL && strcmp(first.constructor_class, class_name) == 0);
    CHECK(mc_registry_classes_inspect_crafting(f.owner, &craft, NULL) == MC_REGISTRY_OK &&
          mc_registry_classes_inspect_smelting(f.owner, &smelt, NULL) == MC_REGISTRY_OK);
    if (route == 1)
        CHECK(craft.phase == MC_REGISTRY_FAILED && craft.a == NULL);
    if (route == 2)
        CHECK(smelt.phase == MC_REGISTRY_FAILED && smelt.a == NULL);
    if (route == 0) {
        mc_registry_item_view item;
        CHECK(mc_registry_classes_inspect_items(f.owner, &item, NULL) == MC_REGISTRY_OK &&
              item.phase == MC_REGISTRY_FAILED);
    }
    const mc_registry_class_state *state = route == 0   ? &f.owner->item_states[MC_ITEM_BASE]
                                           : route == 1 ? &f.owner->crafting_state
                                                        : &f.owner->smelting_state;
    const mc_registry_error cached = state->failure;
    int32_t failed_pc = state->pc;
    CHECK(state->phase == MC_REGISTRY_FAILED && cached.status == MC_REGISTRY_NULL_REFERENCE &&
          same_error(&first, &cached));
    size_t bytes = f.owner->allocation_bytes, stacks = mc_registry_stack_count(f.owner);
    stream streams[3];
    memcpy(streams, f.streams, sizeof(streams));
    uint64_t u = uniquifier(profile);
    CHECK(enter_route(&f, route, &retry) == MC_REGISTRY_FAILED_INITIALIZATION &&
          retry.status == MC_REGISTRY_FAILED_INITIALIZATION && same_cause(&cached, &retry));
    if (route == 0)
        CHECK(retry.message != NULL &&
              strcmp(retry.message, "Item initialization previously failed") == 0);
    else
        CHECK(retry.message == cached.message);
    CHECK(state->phase == MC_REGISTRY_FAILED && state->pc == failed_pc &&
          same_error(&cached, &state->failure));
    CHECK(bytes == f.owner->allocation_bytes && stacks == mc_registry_stack_count(f.owner) &&
          uniquifier(profile) == u && !f.invalid);
    for (size_t i = 0; i < COUNT(streams); ++i)
        CHECK(streams[i].opens == f.streams[i].opens && streams[i].reads == f.streams[i].reads &&
              streams[i].closes == f.streams[i].closes);
    mc_registry_crafting_view after_craft;
    mc_registry_smelting_view after_smelt;
    CHECK(mc_registry_classes_inspect_crafting(f.owner, &after_craft, NULL) == MC_REGISTRY_OK &&
          craft.phase == after_craft.phase && craft.a == after_craft.a &&
          mc_registry_classes_inspect_smelting(f.owner, &after_smelt, NULL) == MC_REGISTRY_OK &&
          smelt.phase == after_smelt.phase && smelt.a == after_smelt.a);
    result = 0;
cleanup:
    if (!retire(&f))
        result = 1;
    return result;
}

static mc_recipe_argument text(const mc_registry_string *string) {
    return (mc_recipe_argument){.kind = MC_RECIPE_ARGUMENT_STRING, .value.string = string};
}

static mc_recipe_argument character(uint16_t value) {
    return (mc_recipe_argument){.kind = MC_RECIPE_ARGUMENT_CHARACTER, .value.character = value};
}

static mc_recipe_argument stack_arg(mc_item_stack *stack) {
    return (mc_recipe_argument){.kind = MC_RECIPE_ARGUMENT_STACK, .value.stack = stack};
}

static mc_recipe_argument rows(const mc_registry_string *const *values, size_t count) {
    return (mc_recipe_argument){.kind = MC_RECIPE_ARGUMENT_STRINGS,
                                .value.strings = {values, count}};
}

static bool shape(const mc_recipe *recipe, mc_item_stack *output, int32_t width, int32_t height) {
    const mc_shaped_recipe_fields *s = mc_recipe_shaped_fields_get(recipe);
    mc_item_stack_fields fields;
    return s != NULL && mc_item_stack_fields_get(output, &fields) == MC_ITEM_STACK_OK &&
           s->a == fields.id && s->b == width && s->c == height && s->e == output &&
           mc_recipe_output(recipe) == output &&
           mc_recipe_shaped_array_length(recipe) == (size_t)width * (size_t)height;
}

static bool failure(mc_crafting_manager *manager, bool shapeless, mc_item_stack *output,
                    const mc_recipe_argument *args, size_t count, mc_registry_status status,
                    int32_t pc) {
    size_t size = mc_crafting_manager_size(manager);
    const mc_recipe *first = at(manager, 0);
    mc_registry_status actual = shapeless
                                    ? mc_registry_recipe_shapeless(manager, output, args, count)
                                    : mc_registry_recipe_shaped(manager, output, args, count);
    return actual == status && manager->classes->last_error.constructor_pc == pc &&
           mc_crafting_manager_size(manager) == size && at(manager, 0) == first;
}

static int parser_contracts(mc_registry_side side, mc_materials *materials,
                            mc_java_random_profile *profile) {
    int result = 1;
    fixture f = {0};
    mc_item_stack *held_output = NULL, *held_ingredient = NULL;
    bool output_hold = false, ingredient_hold = false;
    CHECK(create(&f, side, materials, profile, 0) == MC_REGISTRY_OK &&
          mc_registry_classes_initialize_blocks(f.owner, NULL) == MC_REGISTRY_OK);
    mc_crafting_manager *manager = f.owner->recipes.crafting;
    mc_smelting_manager *smelter = f.owner->recipes.smelting;
    CHECK(manager != NULL && smelter != NULL);
    uint64_t u = uniquifier(profile);
    f.owner->entered = 1; /* Real READY managers; isolated actual method calls. */
    mc_item_stack *output = new_stack(f.owner, INT32_MAX, -2, INT32_MIN);
    mc_item_stack *source = new_stack(f.owner, -987654321, 7, -3);
    mc_item_stack *replacement = new_stack(f.owner, 99, 2, 4);
    CHECK(output != NULL && source != NULL && replacement != NULL);
    mc_nbt_tag provenance = {.type = MC_NBT_COMPOUND};
    mc_item_stack_error nbt_error;
    CHECK(mc_item_stack_preserve_source(source, &provenance, NULL, &nbt_error) ==
              MC_ITEM_STACK_OK &&
          mc_item_stack_source(source) != NULL);
    held_output = output;
    CHECK(mc_item_stack_retain(held_output) == MC_ITEM_STACK_OK);
    output_hold = true;
    const mc_registry_string *a = property(f.owner, "row.a"), *aa = property(f.owner, "row.aa");
    const mc_registry_string *ab = property(f.owner, "row.ab"), *b = property(f.owner, "row.b");
    const mc_registry_string *empty = property(f.owner, "row.empty");
    const mc_registry_string *unicode = property(f.owner, "row.unicode");
    CHECK(a != NULL && aa != NULL && ab != NULL && b != NULL && empty != NULL && unicode != NULL);

    mc_recipe_argument repeated[] = {text(aa), character('A'), stack_arg(source)};
    size_t n = mc_crafting_manager_size(manager);
    CHECK(mc_registry_recipe_shaped(manager, output, repeated, COUNT(repeated)) == MC_REGISTRY_OK);
    const mc_recipe *recipe = at(manager, n);
    CHECK(shape(recipe, output, 2, 1));
    const mc_shaped_recipe_fields *s = mc_recipe_shaped_fields_get(recipe);
    CHECK(s->d[0] != source && s->d[1] != source && s->d[0] != s->d[1] &&
          fields_are(s->d[0], -987654321, 7, -3) && fields_are(s->d[1], -987654321, 7, -3) &&
          mc_item_stack_source(s->d[0]) == NULL && mc_item_stack_source(s->d[1]) == NULL);
    held_ingredient = s->d[0];
    CHECK(mc_item_stack_retain(held_ingredient) == MC_ITEM_STACK_OK);
    ingredient_hold = true;
    CHECK(mc_item_stack_count_set(source, 81) == MC_ITEM_STACK_OK &&
          fields_are(s->d[0], -987654321, 7, -3));
    CHECK(mc_item_stack_count_set(source, 7) == MC_ITEM_STACK_OK);
    CHECK(mc_item_stack_count_set(output, -44) == MC_ITEM_STACK_OK &&
          fields_are(mc_recipe_output(recipe), INT32_MAX, -44, INT32_MIN) && s->a == INT32_MAX);

    const mc_registry_string *unequal_rows[] = {aa, a};
    mc_recipe_argument array_rows[] = {rows(unequal_rows, COUNT(unequal_rows)), character('A'),
                                       stack_arg(source)};
    n = mc_crafting_manager_size(manager);
    CHECK(mc_registry_recipe_shaped(manager, output, array_rows, COUNT(array_rows)) ==
          MC_REGISTRY_OK);
    CHECK(shape(at(manager, n), output, 1, 2)); /* Last row width, no rectangularity check. */
    mc_recipe_argument consecutive[] = {text(aa), text(a), character('A'), stack_arg(source)};
    n = mc_crafting_manager_size(manager);
    CHECK(mc_registry_recipe_shaped(manager, output, consecutive, COUNT(consecutive)) ==
          MC_REGISTRY_OK);
    CHECK(shape(at(manager, n), output, 1, 2));
    mc_recipe_argument overwrite[] = {text(a), character('A'), stack_arg(source), character('A'),
                                      stack_arg(replacement)};
    n = mc_crafting_manager_size(manager);
    CHECK(mc_registry_recipe_shaped(manager, output, overwrite, COUNT(overwrite)) ==
          MC_REGISTRY_OK);
    s = mc_recipe_shaped_fields_get(at(manager, n));
    CHECK(s != NULL && fields_are(s->d[0], 99, 2, 4) && s->d[0] != replacement);
    mc_recipe_argument absent[] = {text(ab), character('A'), stack_arg(source)};
    n = mc_crafting_manager_size(manager);
    CHECK(mc_registry_recipe_shaped(manager, output, absent, COUNT(absent)) == MC_REGISTRY_OK);
    s = mc_recipe_shaped_fields_get(at(manager, n));
    CHECK(s != NULL && s->d[0] != NULL && s->d[1] == NULL);

    /* A real Properties UTF-16 value, including NUL and separated surrogate units. */
    mc_registry_string_view view;
    CHECK(mc_registry_string_read(unicode, &view) && view.length == 4 && view.units[0] == 0 &&
          view.units[1] == 0xd800 && view.units[2] == 'A' && view.units[3] == 0xdc00);
    const mc_item *item = NULL;
    const mc_block *block = NULL;
    CHECK(mc_registry_classes_inspect_item_static(f.owner, "d", &item, NULL) == MC_REGISTRY_OK &&
          item != NULL &&
          mc_registry_classes_inspect_block_static(f.owner, "u", &block, NULL) == MC_REGISTRY_OK &&
          block != NULL);
    mc_recipe_argument units[] = {text(unicode),
                                  character(0),
                                  stack_arg(source),
                                  character(0xd800),
                                  stack_arg(replacement),
                                  character('A'),
                                  {.kind = MC_RECIPE_ARGUMENT_ITEM, .value.item = item},
                                  character(0xdc00),
                                  {.kind = MC_RECIPE_ARGUMENT_BLOCK, .value.block = block}};
    n = mc_crafting_manager_size(manager);
    CHECK(mc_registry_recipe_shaped(manager, output, units, COUNT(units)) == MC_REGISTRY_OK);
    s = mc_recipe_shaped_fields_get(at(manager, n));
    CHECK(shape(at(manager, n), output, 4, 1) && fields_are(s->d[0], -987654321, 7, -3) &&
          fields_are(s->d[1], 99, 2, 4) &&
          fields_are(s->d[2], mc_item_fields_get(item)->bf, 1, 0) &&
          fields_are(s->d[3], mc_block_fields_get(block)->bn, 1, -1));

    const mc_registry_string *empty_rows[] = {NULL};
    mc_recipe_argument no_rows[] = {rows(empty_rows, 0)};
    n = mc_crafting_manager_size(manager);
    CHECK(mc_registry_recipe_shaped(manager, output, no_rows, COUNT(no_rows)) == MC_REGISTRY_OK &&
          shape(at(manager, n), output, 0, 0));
    mc_recipe_argument zero_width[] = {text(empty), character('A'), stack_arg(source)};
    n = mc_crafting_manager_size(manager);
    CHECK(mc_registry_recipe_shaped(manager, output, zero_width, COUNT(zero_width)) ==
              MC_REGISTRY_OK &&
          shape(at(manager, n), output, 0, 1));
    mc_recipe_argument null_key[] = {text(a), {.kind = MC_RECIPE_ARGUMENT_NULL}, stack_arg(source)};
    n = mc_crafting_manager_size(manager);
    CHECK(mc_registry_recipe_shaped(manager, output, null_key, COUNT(null_key)) == MC_REGISTRY_OK);
    s = mc_recipe_shaped_fields_get(at(manager, n));
    CHECK(s != NULL && s->d[0] == NULL);

    mc_recipe_argument mapped_null[] = {text(a), character('A'), stack_arg(NULL)};
    CHECK(failure(manager, false, output, mapped_null, COUNT(mapped_null),
                  MC_REGISTRY_NULL_REFERENCE, 351));
    mapped_null[2] = (mc_recipe_argument){.kind = MC_RECIPE_ARGUMENT_NULL};
    CHECK(failure(manager, false, output, mapped_null, COUNT(mapped_null),
                  MC_REGISTRY_NULL_REFERENCE, 351));
    mc_recipe_argument short_pair[] = {text(a), character('A')};
    CHECK(failure(manager, false, output, short_pair, COUNT(short_pair),
                  MC_REGISTRY_INDEX_OUT_OF_RANGE, 182));
    mc_recipe_argument bad_key[] = {text(a), stack_arg(source), stack_arg(source)};
    CHECK(failure(manager, false, output, bad_key, COUNT(bad_key), MC_REGISTRY_ILLEGAL_ARGUMENT,
                  169));
    const mc_registry_string *null_row[] = {NULL};
    mc_recipe_argument null_row_arg[] = {rows(null_row, 1)};
    CHECK(failure(manager, false, output, null_row_arg, COUNT(null_row_arg),
                  MC_REGISTRY_NULL_REFERENCE, 60));
    mc_recipe_argument too_short[] = {text(a), text(aa), character('A'), stack_arg(source)};
    CHECK(failure(manager, false, output, too_short, COUNT(too_short),
                  MC_REGISTRY_INDEX_OUT_OF_RANGE, 312));
    CHECK(failure(manager, false, NULL, no_rows, COUNT(no_rows), MC_REGISTRY_NULL_REFERENCE, 7));
    mc_recipe_argument dummy = {0};
    CHECK(failure(manager, false, output, &dummy, 0, MC_REGISTRY_INDEX_OUT_OF_RANGE, 15));
    size_t null_array_stacks = mc_registry_stack_count(f.owner);
    CHECK(failure(manager, false, output, NULL, 0, MC_REGISTRY_NULL_REFERENCE, 15) &&
          mc_registry_stack_count(f.owner) == null_array_stacks);

    mc_recipe_argument ingredients[] = {stack_arg(source),
                                        {.kind = MC_RECIPE_ARGUMENT_ITEM, .value.item = item},
                                        {.kind = MC_RECIPE_ARGUMENT_BLOCK, .value.block = block}};
    n = mc_crafting_manager_size(manager);
    CHECK(mc_registry_recipe_shapeless(manager, output, ingredients, COUNT(ingredients)) ==
          MC_REGISTRY_OK);
    const mc_shapeless_recipe_fields *sl = mc_recipe_shapeless_fields_get(at(manager, n));
    const mc_item_stack *values[3] = {NULL};
    CHECK(sl != NULL && sl->a == output && mc_recipe_ingredient_list_size(sl->b) == 3);
    for (size_t i = 0; i < COUNT(values); ++i)
        CHECK(mc_recipe_ingredient_list_get(sl->b, i, &values[i]) == MC_REGISTRY_OK &&
              values[i] != NULL);
    CHECK(values[0] != source && fields_are(values[0], -987654321, 7, -3) &&
          fields_are(values[1], mc_item_fields_get(item)->bf, 1, 0) &&
          fields_are(values[2], mc_block_fields_get(block)->bn, 1, 0));
    size_t null_array_bytes = f.owner->allocation_bytes;
    null_array_stacks = mc_registry_stack_count(f.owner);
    CHECK(failure(manager, true, NULL, NULL, 0, MC_REGISTRY_NULL_REFERENCE, 13) &&
          f.owner->allocation_bytes > null_array_bytes &&
          mc_registry_stack_count(f.owner) == null_array_stacks);
    n = mc_crafting_manager_size(manager);
    CHECK(mc_registry_recipe_shapeless(manager, NULL, &dummy, 0) == MC_REGISTRY_OK);
    sl = mc_recipe_shapeless_fields_get(at(manager, n));
    CHECK(sl != NULL && sl->a == NULL && mc_recipe_output(at(manager, n)) == NULL &&
          mc_recipe_ingredient_list_size(sl->b) == 0 &&
          mc_crafting_manager_size(manager) == n + 1 &&
          mc_registry_stack_count(f.owner) == null_array_stacks);
    mc_recipe_argument invalid[] = {stack_arg(source), character('Z')};
    size_t stack_count = mc_registry_stack_count(f.owner);
    CHECK(failure(manager, true, output, invalid, COUNT(invalid), MC_REGISTRY_ILLEGAL_ARGUMENT,
                  128) &&
          mc_registry_stack_count(f.owner) == stack_count + 1);
    const mc_recipe_argument nulls[] = {{.kind = MC_RECIPE_ARGUMENT_NULL},
                                        {.kind = MC_RECIPE_ARGUMENT_STACK, .value.stack = NULL},
                                        {.kind = MC_RECIPE_ARGUMENT_ITEM, .value.item = NULL},
                                        {.kind = MC_RECIPE_ARGUMENT_BLOCK, .value.block = NULL}};
    for (size_t i = 0; i < COUNT(nulls); ++i)
        CHECK(failure(manager, true, output, &nulls[i], 1, MC_REGISTRY_ILLEGAL_ARGUMENT, 128));

    /* Live Map replacement includes a real null value, not key removal. No
     * native insertion sequence is asserted to be Java HashMap traversal. */
    const mc_item_stack *found = output;
    CHECK(mc_smelting_manager_find(smelter, INT32_MIN, &found) == MC_REGISTRY_OK && found == NULL);
    size_t smelt_count = mc_smelting_manager_size(smelter);
    CHECK(mc_registry_smelting_put(smelter, INT32_MIN, source) == MC_REGISTRY_OK &&
          mc_smelting_manager_size(smelter) == smelt_count + 1);
    CHECK(mc_registry_smelting_put(smelter, INT32_MIN, replacement) == MC_REGISTRY_OK &&
          mc_smelting_manager_find(smelter, INT32_MIN, &found) == MC_REGISTRY_OK &&
          found == replacement);
    CHECK(mc_registry_smelting_put(smelter, INT32_MIN, NULL) == MC_REGISTRY_OK &&
          mc_smelting_manager_find(smelter, INT32_MIN, &found) == MC_REGISTRY_OK && found == NULL &&
          mc_smelting_manager_size(smelter) == smelt_count + 1);
    int32_t keys[11];
    size_t count = 0;
    CHECK(mc_smelting_manager_keys(smelter, keys, COUNT(keys), &count) == MC_REGISTRY_OK &&
          count == 11);
    bool key_present = false;
    for (size_t i = 0; i < count; ++i)
        if (keys[i] == INT32_MIN)
            key_present = true;
    CHECK(key_present);
    int32_t canary = 7654321;
    count = SIZE_MAX;
    CHECK(mc_smelting_manager_keys(smelter, &canary, 1, &count) == MC_REGISTRY_LIMIT &&
          canary == 7654321 && count == SIZE_MAX);
    const mc_recipe *recipe_canary = at(manager, 0);
    CHECK(mc_crafting_manager_get(manager, mc_crafting_manager_size(manager), &recipe_canary) ==
              MC_REGISTRY_INDEX_OUT_OF_RANGE &&
          recipe_canary == at(manager, 0));
    size_t previous_count = mc_crafting_manager_size(manager);
    CHECK(mc_registry_recipe_shaped(NULL, output, repeated, COUNT(repeated)) ==
          MC_REGISTRY_INVALID_ARGUMENT);
    CHECK(mc_registry_recipe_shapeless(NULL, output, &dummy, 0) == MC_REGISTRY_INVALID_ARGUMENT);
    CHECK(mc_registry_smelting_put(NULL, 0, source) == MC_REGISTRY_INVALID_ARGUMENT);
    CHECK(mc_crafting_manager_get(NULL, 0, &recipe_canary) == MC_REGISTRY_INVALID_ARGUMENT &&
          recipe_canary == at(manager, 0));
    f.owner->entered = 0;
    CHECK(mc_registry_recipe_shapeless(manager, output, &dummy, 0) ==
              MC_REGISTRY_INVALID_ARGUMENT &&
          mc_crafting_manager_size(manager) == previous_count);
    f.owner->entered = 1;
    CHECK(uniquifier(profile) == u && !f.invalid);
    CHECK(retire(&f));
    CHECK(fields_are(held_output, INT32_MAX, -44, INT32_MIN) &&
          fields_are(held_ingredient, -987654321, 7, -3));
    result = 0;
cleanup:
    if (!retire(&f))
        result = 1;
    if (output_hold)
        mc_item_stack_release(held_output);
    if (ingredient_hold)
        mc_item_stack_release(held_ingredient);
    return result;
}

static int repeated_statistics(mc_registry_side side, mc_materials *materials,
                               mc_java_random_profile *profile) {
    int result = 1;
    fixture f = {0};
    CHECK(create(&f, side, materials, profile, 0) == MC_REGISTRY_OK &&
          mc_registry_classes_initialize_blocks(f.owner, NULL) == MC_REGISTRY_OK);
    mc_statistic **old = f.owner->statistics.D;
    CHECK(old != NULL && f.owner->statistics.G && f.owner->statistics.H);
    const mc_statistic *old_first = NULL;
    size_t old_index = 0;
    while (old_index < 32000 && old[old_index] == NULL)
        ++old_index;
    CHECK(old_index < 32000);
    old_first = old[old_index];
    size_t bytes = f.owner->allocation_bytes;
    CHECK(mc_registry_statistic_recipes(f.owner) == MC_REGISTRY_INVALID_ARGUMENT &&
          f.owner->statistics.D == old && f.owner->allocation_bytes == bytes);
    f.owner->entered = 1; /* Actual READY owner; explicit mutable G/H inputs. */
    uint64_t u = uniquifier(profile);
    f.owner->statistics.G = false;
    CHECK(mc_registry_statistic_recipes(f.owner) == MC_REGISTRY_OK &&
          f.owner->statistics.D == old && f.owner->allocation_bytes == bytes &&
          !f.owner->statistics.G && f.owner->statistics.H);
    f.owner->statistics.G = true;
    f.owner->statistics.H = false;
    CHECK(mc_registry_statistic_recipes(f.owner) == MC_REGISTRY_OK &&
          f.owner->statistics.D == old && f.owner->allocation_bytes == bytes &&
          f.owner->statistics.G && !f.owner->statistics.H);
    f.owner->statistics.H = true;
    mc_registry_statistic_view before, after;
    CHECK(mc_registry_classes_inspect_statistics(f.owner, &before, NULL) == MC_REGISTRY_OK);
    const size_t counts[] = {mc_statistic_list_size(before.b), mc_statistic_list_size(before.c),
                             mc_statistic_list_size(before.d), mc_statistic_list_size(before.e)};
    CHECK(mc_registry_statistic_recipes(f.owner) == MC_REGISTRY_DUPLICATE_STATISTIC);
    CHECK(mc_registry_classes_inspect_statistics(f.owner, &after, NULL) == MC_REGISTRY_OK &&
          after.phase == MC_REGISTRY_READY && after.D != NULL && after.D != old && after.G &&
          after.H && after.map_size == before.map_size &&
          mc_statistic_list_size(after.b) == counts[0] &&
          mc_statistic_list_size(after.c) == counts[1] &&
          mc_statistic_list_size(after.d) == counts[2] &&
          mc_statistic_list_size(after.e) == counts[3]);
    bool empty = true;
    for (size_t i = 0; i < 32000; ++i)
        if (after.D[i] != NULL)
            empty = false;
    CHECK(empty && old[old_index] == old_first && f.owner->allocation_bytes > bytes &&
          uniquifier(profile) == u && f.owner->last_error.constructor_pc == 88);
    result = 0;
cleanup:
    if (!retire(&f))
        result = 1;
    return result;
}

static int natural_limit(mc_registry_side side, mc_materials *materials,
                         mc_java_random_profile *profile) {
    int result = 1;
    fixture f = {0};
    mc_item_stack *held = NULL;
    bool retained = false;
    CHECK(create(&f, side, materials, profile, 0) == MC_REGISTRY_OK &&
          mc_registry_classes_initialize_blocks(f.owner, NULL) == MC_REGISTRY_OK);
    size_t baseline = f.owner->allocation_bytes;
    CHECK(baseline <= SIZE_MAX - 1024 && retire(&f));
    CHECK(create(&f, side, materials, profile, baseline + 1024) == MC_REGISTRY_OK &&
          mc_registry_classes_initialize_blocks(f.owner, NULL) == MC_REGISTRY_OK);
    mc_crafting_manager *manager = f.owner->recipes.crafting;
    const mc_recipe *original[151];
    CHECK(mc_crafting_manager_size(manager) == COUNT(original));
    for (size_t i = 0; i < COUNT(original); ++i) {
        original[i] = at(manager, i);
        CHECK(original[i] != NULL);
    }
    held = (mc_item_stack *)mc_recipe_output(original[0]);
    CHECK(mc_item_stack_retain(held) == MC_ITEM_STACK_OK);
    retained = true;
    mc_item_stack_fields fields, after;
    CHECK(mc_item_stack_fields_get(held, &fields) == MC_ITEM_STACK_OK);
    uint64_t u = uniquifier(profile);
    f.owner->entered = 1;
    bool exhausted = false;
    mc_recipe_argument empty_arguments = {0};
    for (size_t i = 0; i < 64; ++i) {
        size_t count = mc_crafting_manager_size(manager);
        mc_registry_status status =
            mc_registry_recipe_shapeless(manager, held, &empty_arguments, 0);
        if (status == MC_REGISTRY_LIMIT) {
            CHECK(mc_crafting_manager_size(manager) == count &&
                  f.owner->last_error.status == MC_REGISTRY_LIMIT);
            exhausted = true;
            break;
        }
        CHECK(status == MC_REGISTRY_OK && mc_crafting_manager_size(manager) == count + 1 &&
              mc_recipe_output(at(manager, count)) == held);
    }
    CHECK(exhausted && f.owner->allocation_bytes <= baseline + 1024 && uniquifier(profile) == u);
    for (size_t i = 0; i < COUNT(original); ++i)
        CHECK(at(manager, i) == original[i]);
    CHECK(retire(&f));
    CHECK(mc_item_stack_fields_get(held, &after) == MC_ITEM_STACK_OK && fields.id == after.id &&
          fields.count == after.count && fields.damage == after.damage &&
          fields.animation == after.animation);
    result = 0;
cleanup:
    if (!retire(&f))
        result = 1;
    if (retained)
        mc_item_stack_release(held);
    return result;
}

int main(void) {
    int result = 1;
    mc_materials *materials = NULL;
    mc_java_random_profile *profile = NULL;
    CHECK(mc_materials_create(&materials));
    CHECK(mc_java_random_profile_create(SIZE_MAX, &profile) == MC_JAVA_RANDOM_PROFILE_OK);
    for (int side = MC_REGISTRY_CLIENT; side <= MC_REGISTRY_SERVER; ++side) {
        CHECK(bootstrap((mc_registry_side)side, materials, profile) == 0);
        for (int route = 0; route < 3; ++route)
            CHECK(entry_failures((mc_registry_side)side, materials, profile, route) == 0);
        CHECK(parser_contracts((mc_registry_side)side, materials, profile) == 0);
        CHECK(repeated_statistics((mc_registry_side)side, materials, profile) == 0);
        CHECK(natural_limit((mc_registry_side)side, materials, profile) == 0);
    }
    result = 0;
cleanup:
    if (!retirement_failed) {
        mc_java_random_profile_free(profile);
        mc_materials_destroy(materials);
    }
    if (result == 0 && printf("Recipe native contracts: %u checks passed\n", checks) < 0)
        result = 1;
    return result;
}
