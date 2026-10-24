#include "mc/resources/translation/translation.h"

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

/* Synthetic resource bytes, not English-game resources or original goldens.
 * Both real loads target one Properties owner. Equal value text is deliberate:
 * String equality must not merge separately allocated value identities. */
static const char english[] = "# first fixture\n"
                              "dup=first\n"
                              "only=base\n"
                              "same=equal\n"
                              "alias=equal\n"
                              "nul\\u0000key=latin\\u00e9\\u0000\\ud800\n"
                              "empty=\n";
static const char statistics[] = "dup=second\n"
                                 "same=equal\n"
                                 "alias=equal\n"
                                 "tail=stats\n"
                                 "astral=\\ud83d\\ude00\n";
static const uint16_t nul_key[] = {'n', 'u', 'l', 0, 'k', 'e', 'y'};
static const uint16_t nul_value[] = {'l', 'a', 't', 'i', 'n', 0x00e9, 0, 0xd800};
static const uint16_t astral_value[] = {0xd83d, 0xde00};

static bool same_error(const mc_registry_error *a, const mc_registry_error *b) {
    return a->status == b->status && a->message == b->message &&
           a->initializer_pc == b->initializer_pc && a->block_id == b->block_id &&
           a->constructor_class == b->constructor_class && a->constructor_pc == b->constructor_pc &&
           a->profile_status == b->profile_status &&
           a->active_initializer_class == b->active_initializer_class &&
           a->active_initializer_pc == b->active_initializer_pc &&
           a->item_argument == b->item_argument;
}

static bool same_view(const mc_registry_translation_view *a,
                      const mc_registry_translation_view *b) {
    return a->singleton_phase == b->singleton_phase && a->wrapper_phase == b->wrapper_phase &&
           a->singleton_initializer_pc == b->singleton_initializer_pc &&
           a->wrapper_initializer_pc == b->wrapper_initializer_pc &&
           a->singleton_a == b->singleton_a && a->wrapper_a == b->wrapper_a;
}

static bool core_units(const mc_java_properties_string *string, const uint16_t *units,
                       size_t length) {
    mc_java_properties_string_view view;
    if (!mc_java_properties_string_read(string, &view) || view.length != length ||
        (length != 0 && view.units == NULL))
        return false;
    for (size_t i = 0; i < length; ++i)
        if (view.units[i] != units[i])
            return false;
    return true;
}

static bool core_text(const mc_java_properties_string *string, const char *text) {
    uint16_t units[128];
    size_t length = strlen(text);
    if (length > COUNT(units))
        return false;
    for (size_t i = 0; i < length; ++i)
        units[i] = (unsigned char)text[i];
    return core_units(string, units, length);
}

static mc_java_properties_status core_lookup(const mc_java_properties *properties, const char *key,
                                             const mc_java_properties_string **out) {
    uint16_t units[128];
    size_t length = strlen(key);
    if (length > COUNT(units))
        return MC_JAVA_PROPERTIES_INVALID_ARGUMENT;
    for (size_t i = 0; i < length; ++i)
        units[i] = (unsigned char)key[i];
    return mc_java_properties_get(properties, units, length, out);
}

static mc_registry_status lookup(const mc_translation *translation, const char *key,
                                 const mc_registry_string **out, mc_registry_error *error) {
    uint16_t units[128];
    size_t length = strlen(key);
    if (length > COUNT(units))
        return MC_REGISTRY_INVALID_ARGUMENT;
    for (size_t i = 0; i < length; ++i)
        units[i] = (unsigned char)key[i];
    return mc_translation_property(translation, units, length, out, error);
}

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
    uint16_t units[128];
    size_t length = strlen(text);
    if (length > COUNT(units))
        return false;
    for (size_t i = 0; i < length; ++i)
        units[i] = (unsigned char)text[i];
    return units_are(string, units, length);
}

typedef struct memory_stream {
    const uint8_t *bytes;
    size_t length, offset, chunk, calls, fail_call;
    bool invalid;
} memory_stream;

static mc_java_properties_status read_bytes(void *context, uint8_t *destination, size_t capacity,
                                            size_t *count) {
    memory_stream *stream = context;
    ++stream->calls;
    if (destination == NULL || count == NULL || capacity == 0 || capacity > 8192 ||
        stream->offset > stream->length) {
        stream->invalid = true;
        return MC_JAVA_PROPERTIES_IO_ERROR;
    }
    *count = 0;
    if (stream->fail_call != 0 && stream->calls == stream->fail_call)
        return MC_JAVA_PROPERTIES_IO_ERROR;
    size_t take = stream->length - stream->offset;
    if (take > stream->chunk)
        take = stream->chunk;
    if (take > capacity)
        take = capacity;
    if (take != 0)
        memcpy(destination, stream->bytes + stream->offset, take);
    stream->offset += take;
    *count = take;
    return MC_JAVA_PROPERTIES_OK;
}

typedef struct resources {
    memory_stream streams[2];
    mc_registry_classes *owner;
    mc_registry_resources source;
    size_t opens, reentries;
    int missing;
    bool invalid, reenter, first_observed;
    const mc_java_properties_string *old_dup, *old_same, *old_alias, *first_dup_key;
    mc_java_properties_string_view old_same_view;
} resources;

/* The callback is an actual source opening, not a root initialization flow.
 * Public nested initialization/free must refuse it. Diagnostic inspection
 * reports the actual entered prefix without performing an active use. */
static bool check_reentry(resources *r) {
    mc_registry_classes *owner = r->owner;
    if (owner == NULL || owner->entered == 0)
        return false;
    const size_t bytes = owner->allocation_bytes;
    const unsigned int entered = owner->entered;
    mc_registry_initializer *frame = owner->initializer;
    mc_translation *pending = owner->translations.pending;
    const mc_registry_class_state singleton = owner->translation_state;
    const mc_registry_class_state wrapper = owner->translation_wrapper_state;
    const mc_registry_error last = owner->last_error;
    const mc_registry_translation_view canary = {
        MC_REGISTRY_READY, MC_REGISTRY_FAILED, 1234, 5678, NULL, NULL};
    mc_registry_translation_view view = canary;
    const mc_registry_translation_view expected = {singleton.phase,
                                                   wrapper.phase,
                                                   singleton.pc,
                                                   wrapper.pc,
                                                   owner->translations.singleton_a,
                                                   owner->translations.wrapper_a};
    mc_registry_error error;
    bool ok = mc_registry_classes_inspect_translation(owner, &view, &error) == MC_REGISTRY_OK;
    ok = ok && error.status == MC_REGISTRY_OK && same_view(&view, &expected);
    ok = ok && view.singleton_phase == MC_REGISTRY_INITIALIZING &&
         view.wrapper_phase == MC_REGISTRY_INITIALIZING && view.singleton_initializer_pc == 4 &&
         view.wrapper_initializer_pc == 0 && view.singleton_a == NULL && view.wrapper_a == NULL;
    ok = ok && mc_registry_classes_initialize_translation(owner, &error) == MC_REGISTRY_BUSY;
    ok = ok && error.status == MC_REGISTRY_BUSY;
    ok = ok && mc_registry_classes_initialize_blocks(owner, &error) == MC_REGISTRY_BUSY;
    ok = ok && mc_registry_classes_initialize_items(owner, &error) == MC_REGISTRY_BUSY;
    ok = ok && mc_registry_classes_free(owner, &error) == MC_REGISTRY_BUSY;
    ok = ok && error.status == MC_REGISTRY_BUSY;
    ok = ok && owner->allocation_bytes == bytes && owner->entered == entered &&
         owner->initializer == frame && owner->translations.pending == pending &&
         owner->translation_state.phase == singleton.phase &&
         owner->translation_state.pc == singleton.pc &&
         same_error(&owner->translation_state.failure, &singleton.failure) &&
         owner->translation_wrapper_state.phase == wrapper.phase &&
         owner->translation_wrapper_state.pc == wrapper.pc &&
         same_error(&owner->translation_wrapper_state.failure, &wrapper.failure) &&
         same_error(&owner->last_error, &last);
    ++r->reentries;
    return ok;
}

static bool observe_first(resources *r) {
    const mc_translation *pending = r->owner->translations.pending;
    const mc_java_properties_string *value = NULL;
    if (pending == NULL || pending->b == NULL || mc_java_properties_size(pending->b) != 6 ||
        core_lookup(pending->b, "dup", &r->old_dup) != MC_JAVA_PROPERTIES_OK ||
        core_lookup(pending->b, "same", &r->old_same) != MC_JAVA_PROPERTIES_OK ||
        core_lookup(pending->b, "alias", &r->old_alias) != MC_JAVA_PROPERTIES_OK ||
        mc_java_properties_entry(pending->b, 0, &r->first_dup_key, &value) !=
            MC_JAVA_PROPERTIES_OK ||
        value != r->old_dup || !core_text(r->first_dup_key, "dup") ||
        !core_text(r->old_dup, "first") || !core_text(r->old_same, "equal") ||
        !core_text(r->old_alias, "equal") || r->old_same == r->old_alias ||
        !mc_java_properties_string_read(r->old_same, &r->old_same_view))
        return false;
    r->first_observed = true;
    return true;
}

static mc_registry_status open_resource(void *context, const char *name,
                                        mc_java_properties_input *out) {
    resources *r = context;
    size_t index = r->opens++;
    if (index >= COUNT(r->streams) || name == NULL || out == NULL ||
        strcmp(name, index == 0 ? "/lang/en_US.lang" : "/lang/stats_US.lang") != 0) {
        r->invalid = true;
        return MC_REGISTRY_INVALID_ARGUMENT;
    }
    if (r->reenter && !check_reentry(r))
        r->invalid = true;
    /* This observation is between load26 and load40, with no parser active.
     * It uses only real retained owners and performs no mutation/allocation. */
    if (index == 1 && !observe_first(r))
        r->invalid = true;
    if ((int)index == r->missing)
        return MC_REGISTRY_NULL_REFERENCE;
    *out = (mc_java_properties_input){&r->streams[index], read_bytes};
    return MC_REGISTRY_OK;
}

static void resources_init(resources *r, size_t chunk) {
    *r = (resources){0};
    r->streams[0] =
        (memory_stream){(const uint8_t *)english, sizeof(english) - 1, 0, chunk, 0, 0, false};
    r->streams[1] =
        (memory_stream){(const uint8_t *)statistics, sizeof(statistics) - 1, 0, chunk, 0, 0, false};
    r->source = (mc_registry_resources){r, open_resource, NULL, NULL};
    r->missing = -1;
}

static void stream_text(memory_stream *stream, const char *text) {
    stream->bytes = (const uint8_t *)text;
    stream->length = strlen(text);
}

static bool resources_valid(const resources *r) {
    return !r->invalid && !r->streams[0].invalid && !r->streams[1].invalid && r->opens <= 2;
}

static int retire(mc_registry_classes *owner) {
    if (owner == NULL || mc_registry_classes_free(owner, NULL) == MC_REGISTRY_OK)
        return 0;
    retirement_failed = true;
    fprintf(stderr, "class retirement refused; preserving borrowed resources/dependencies\n");
    return 1;
}

static bool registry_untouched(const mc_registry_classes *owner) {
    mc_registry_block_view blocks;
    mc_registry_item_view items;
    if (mc_registry_classes_inspect_blocks(owner, &blocks, NULL) != MC_REGISTRY_OK ||
        mc_registry_classes_inspect_items(owner, &items, NULL) != MC_REGISTRY_OK ||
        blocks.phase != MC_REGISTRY_COLD || blocks.initializer_pc != -1 ||
        blocks.named_blocks != 0 || blocks.d != NULL || blocks.e != NULL || blocks.f != NULL ||
        blocks.g != NULL || blocks.h != NULL || blocks.i != NULL || blocks.j != NULL ||
        blocks.k != NULL || blocks.l != NULL || blocks.m != NULL || blocks.n != NULL ||
        blocks.o != NULL || blocks.p != NULL || blocks.q != NULL || blocks.r != NULL ||
        blocks.s != NULL || blocks.t != NULL || items.phase != MC_REGISTRY_COLD ||
        items.initializer_pc != -1 || items.named_items != 0 || items.b != NULL ||
        items.c != NULL || owner->item_tool_state.phase != MC_REGISTRY_COLD)
        return false;
    for (size_t i = 0; i < COUNT(owner->blocks.named); ++i)
        if (owner->blocks.named[i] != NULL)
            return false;
    for (size_t i = 0; i < COUNT(owner->items.named); ++i)
        if (owner->items.named[i] != NULL)
            return false;
    for (size_t i = 0; i < MC_BLOCK_KIND_COUNT; ++i)
        if (owner->block_states[i].phase != MC_REGISTRY_COLD || owner->block_states[i].pc != -1)
            return false;
    for (size_t i = 0; i < MC_ITEM_KIND_COUNT; ++i)
        if (owner->item_states[i].phase != MC_REGISTRY_COLD || owner->item_states[i].pc != -1)
            return false;
    return true;
}

typedef struct material_snapshot {
    mc_material *identity;
    const mc_material_palette *palette;
    int32_t palette_index, palette_rgb;
    bool a, b, c, d, e, g, h, i;
    int32_t j;
} material_snapshot;

typedef struct dependencies {
    mc_materials *materials;
    mc_java_random_profile *profile;
    material_snapshot material[MC_MATERIAL_COUNT];
    uint64_t uniquifier;
    unsigned int holder;
    mc_java_math_random_owner *math;
    size_t profile_bytes;
} dependencies;

static material_snapshot material_read(mc_material *m) {
    const mc_material_palette *palette = mc_material_palette_of(m);
    return (material_snapshot){m,
                               palette,
                               mc_material_palette_index(palette),
                               mc_material_palette_rgb(palette),
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

static void capture_dependencies(dependencies *d) {
    d->uniquifier = atomic_load_explicit(&d->profile->uniquifier, memory_order_relaxed);
    d->holder = atomic_load_explicit(&d->profile->holder_phase, memory_order_relaxed);
    d->math = d->profile->math;
    d->profile_bytes = mc_java_random_profile_allocation_bytes(d->profile);
    for (size_t i = 0; i < MC_MATERIAL_COUNT; ++i)
        d->material[i] = material_read(mc_materials_get(d->materials, (mc_material_id)i));
}

static bool dependencies_unchanged(const dependencies *d) {
    if (atomic_load_explicit(&d->profile->uniquifier, memory_order_relaxed) != d->uniquifier ||
        atomic_load_explicit(&d->profile->holder_phase, memory_order_relaxed) != d->holder ||
        d->profile->math != d->math ||
        mc_java_random_profile_allocation_bytes(d->profile) != d->profile_bytes)
        return false;
    for (size_t i = 0; i < MC_MATERIAL_COUNT; ++i) {
        material_snapshot a = d->material[i];
        material_snapshot b = material_read(mc_materials_get(d->materials, (mc_material_id)i));
        if (a.identity != b.identity || a.palette != b.palette ||
            a.palette_index != b.palette_index || a.palette_rgb != b.palette_rgb || a.a != b.a ||
            a.b != b.b || a.c != b.c || a.d != b.d || a.e != b.e || a.g != b.g || a.h != b.h ||
            a.i != b.i || a.j != b.j)
            return false;
    }
    return true;
}

static mc_registry_status allocate(mc_registry_side side, const dependencies *d, resources *r,
                                   size_t maximum, mc_registry_classes **out,
                                   mc_registry_error *error) {
    const mc_registry_options options = {side, d->profile, d->materials, maximum,
                                         r == NULL ? NULL : &r->source};
    mc_registry_status status = mc_registry_classes_allocate(&options, out, error);
    if (status == MC_REGISTRY_OK && r != NULL)
        r->owner = *out;
    return status;
}

static int test_cold(mc_registry_side side, const dependencies *d) {
    int result = 1;
    mc_registry_classes *owner = NULL;
    resources r;
    resources_init(&r, 7);
    mc_registry_error error;
    mc_registry_translation_view view;
    CHECK(allocate(side, d, &r, 0, &owner, &error) == MC_REGISTRY_OK);
    size_t bytes = owner->allocation_bytes;
    CHECK(mc_registry_classes_inspect_translation(owner, &view, &error) == MC_REGISTRY_OK);
    CHECK(view.singleton_phase == MC_REGISTRY_COLD && view.wrapper_phase == MC_REGISTRY_COLD);
    CHECK(view.singleton_initializer_pc == -1 && view.wrapper_initializer_pc == -1);
    CHECK(view.singleton_a == NULL && view.wrapper_a == NULL &&
          owner->translations.pending == NULL);
    CHECK(r.opens == 0 && r.streams[0].calls == 0 && r.streams[1].calls == 0);
    CHECK(mc_translation_property_count(NULL) == 0);
    CHECK(mc_registry_classes_initialize_translation(NULL, &error) == MC_REGISTRY_INVALID_ARGUMENT);
    mc_registry_translation_view old = view;
    CHECK(mc_registry_classes_inspect_translation(NULL, &view, &error) ==
          MC_REGISTRY_INVALID_ARGUMENT);
    CHECK(same_view(&view, &old));
    CHECK(mc_registry_classes_inspect_translation(owner, NULL, &error) ==
          MC_REGISTRY_INVALID_ARGUMENT);
    CHECK(owner->allocation_bytes == bytes && registry_untouched(owner) &&
          dependencies_unchanged(d));
    CHECK(resources_valid(&r));
    result = 0;
cleanup:
    if (retire(owner) != 0)
        result = 1;
    return result;
}

static int check_loaded(mc_registry_classes *owner, const resources *r) {
    int result = 1;
    mc_registry_translation_view view;
    mc_registry_error error;
    CHECK(mc_registry_classes_inspect_translation(owner, &view, &error) == MC_REGISTRY_OK);
    CHECK(view.singleton_phase == MC_REGISTRY_READY && view.wrapper_phase == MC_REGISTRY_READY);
    CHECK(view.singleton_initializer_pc == 10 && view.wrapper_initializer_pc == 6);
    CHECK(view.singleton_a != NULL && view.singleton_a == view.wrapper_a);
    const mc_translation *translation = view.singleton_a;
    CHECK(translation->b != NULL && mc_translation_property_count(translation) == 8);
    CHECK(r->opens == 2 && r->first_observed && resources_valid(r));
    const mc_registry_string *dup = NULL, *same = NULL, *alias = NULL, *value = NULL;
    CHECK(lookup(translation, "dup", &dup, &error) == MC_REGISTRY_OK && text_is(dup, "second"));
    CHECK(lookup(translation, "only", &value, &error) == MC_REGISTRY_OK && text_is(value, "base"));
    CHECK(lookup(translation, "same", &same, &error) == MC_REGISTRY_OK && text_is(same, "equal"));
    CHECK(lookup(translation, "alias", &alias, &error) == MC_REGISTRY_OK &&
          text_is(alias, "equal"));
    CHECK(same != alias && dup != same && dup != alias);
    CHECK(lookup(translation, "tail", &value, &error) == MC_REGISTRY_OK && text_is(value, "stats"));
    CHECK(lookup(translation, "empty", &value, &error) == MC_REGISTRY_OK && text_is(value, ""));
    CHECK(mc_translation_property(translation, nul_key, COUNT(nul_key), &value, &error) ==
          MC_REGISTRY_OK);
    CHECK(units_are(value, nul_value, COUNT(nul_value)));
    CHECK(lookup(translation, "astral", &value, &error) == MC_REGISTRY_OK);
    CHECK(units_are(value, astral_value, COUNT(astral_value)));
    value = dup;
    CHECK(lookup(translation, "missing", &value, &error) == MC_REGISTRY_OK && value == NULL);
    value = dup;
    CHECK(mc_translation_property(translation, NULL, 0, &value, &error) == MC_REGISTRY_OK &&
          value == NULL);
    value = dup;
    CHECK(mc_translation_property(NULL, nul_key, COUNT(nul_key), &value, &error) ==
          MC_REGISTRY_INVALID_ARGUMENT);
    CHECK(value == dup);
    CHECK(mc_translation_property(translation, NULL, 1, &value, &error) ==
          MC_REGISTRY_INVALID_ARGUMENT);
    CHECK(value == dup);
    CHECK(mc_translation_property(translation, nul_key, COUNT(nul_key), NULL, &error) ==
          MC_REGISTRY_INVALID_ARGUMENT);
    CHECK(core_text(r->old_dup, "first") && core_text(r->old_same, "equal") &&
          core_text(r->old_alias, "equal"));
    mc_java_properties_string_view old_view;
    CHECK(mc_java_properties_string_read(r->old_same, &old_view));
    CHECK(old_view.units == r->old_same_view.units && old_view.length == r->old_same_view.length);
    const mc_java_properties_string *core_dup = NULL, *core_same = NULL, *core_alias = NULL;
    const mc_java_properties_string *key = NULL, *core_value = NULL;
    CHECK(core_lookup(translation->b, "dup", &core_dup) == MC_JAVA_PROPERTIES_OK);
    CHECK(core_lookup(translation->b, "same", &core_same) == MC_JAVA_PROPERTIES_OK);
    CHECK(core_lookup(translation->b, "alias", &core_alias) == MC_JAVA_PROPERTIES_OK);
    CHECK(core_dup != r->old_dup && core_same != r->old_same && core_alias != r->old_alias);
    CHECK(core_same != core_alias && core_text(core_dup, "second"));
    CHECK(mc_java_properties_entry(translation->b, 0, &key, &core_value) == MC_JAVA_PROPERTIES_OK);
    CHECK(key == r->first_dup_key && core_value == core_dup);
    size_t bytes = owner->allocation_bytes;
    size_t reads[2] = {r->streams[0].calls, r->streams[1].calls};
    for (size_t i = 0; i < 3; ++i) {
        value = NULL;
        CHECK(lookup(translation, "dup", &value, &error) == MC_REGISTRY_OK && value == dup);
        CHECK(mc_registry_classes_initialize_translation(owner, &error) == MC_REGISTRY_OK);
        CHECK(error.status == MC_REGISTRY_OK);
    }
    CHECK(owner->allocation_bytes == bytes && r->opens == 2);
    CHECK(r->streams[0].calls == reads[0] && r->streams[1].calls == reads[1]);
    CHECK(registry_untouched(owner));
    CHECK(owner->entered == 0 && owner->initializer == NULL && owner->current_item == NULL &&
          owner->current_block == NULL);
    result = 0;
cleanup:
    return result;
}

static int test_success(mc_registry_side side, const dependencies *d, size_t chunk) {
    int result = 1;
    mc_registry_classes *first = NULL, *second = NULL;
    resources a, b;
    resources_init(&a, chunk);
    resources_init(&b, chunk);
    a.reenter = true;
    CHECK(allocate(side, d, &a, 0, &first, NULL) == MC_REGISTRY_OK);
    CHECK(allocate(side, d, &b, 0, &second, NULL) == MC_REGISTRY_OK && second != first);
    CHECK(mc_registry_classes_initialize_translation(first, NULL) == MC_REGISTRY_OK);
    CHECK(a.reentries == 2 && check_loaded(first, &a) == 0);
    CHECK(b.opens == 0 && b.streams[0].calls == 0 && b.streams[1].calls == 0);
    CHECK(second->translation_state.phase == MC_REGISTRY_COLD &&
          second->translations.pending == NULL);
    CHECK(mc_registry_classes_initialize_translation(second, NULL) == MC_REGISTRY_OK);
    CHECK(check_loaded(second, &b) == 0);
    CHECK(first->translations.singleton_a != second->translations.singleton_a);
    CHECK(first->translations.singleton_a->b != second->translations.singleton_a->b);
    const mc_registry_string *x = NULL, *y = NULL;
    CHECK(lookup(first->translations.singleton_a, "same", &x, NULL) == MC_REGISTRY_OK);
    CHECK(lookup(second->translations.singleton_a, "same", &y, NULL) == MC_REGISTRY_OK);
    CHECK(x != y && text_is(x, "equal") && text_is(y, "equal"));
    CHECK(a.old_same != b.old_same && dependencies_unchanged(d));
    CHECK(retire(first) == 0);
    first = NULL;
    CHECK(text_is(y, "equal") && check_loaded(second, &b) == 0);
    result = 0;
cleanup:
    if (retire(first) != 0)
        result = 1;
    if (retire(second) != 0)
        result = 1;
    return result;
}

static int check_failed_retry(mc_registry_classes *owner, const resources *r) {
    int result = 1;
    mc_registry_translation_view before, after;
    mc_registry_error error;
    CHECK(mc_registry_classes_inspect_translation(owner, &before, &error) == MC_REGISTRY_OK);
    CHECK(before.singleton_phase == MC_REGISTRY_FAILED &&
          before.wrapper_phase == MC_REGISTRY_FAILED);
    CHECK(before.singleton_a == NULL && before.wrapper_a == NULL);
    mc_registry_class_state singleton = owner->translation_state;
    mc_registry_class_state wrapper = owner->translation_wrapper_state;
    mc_translation *pending = owner->translations.pending;
    mc_java_properties *properties = pending == NULL ? NULL : pending->b;
    size_t count = mc_java_properties_size(properties), bytes = owner->allocation_bytes;
    mc_registry_allocation *allocations = owner->allocations;
    size_t opens = r->opens, reads[2] = {r->streams[0].calls, r->streams[1].calls};
    CHECK(registry_untouched(owner));
    CHECK(mc_registry_classes_inspect_translation(NULL, &after, &error) ==
          MC_REGISTRY_INVALID_ARGUMENT);
    CHECK(mc_registry_classes_initialize_translation(owner, &error) ==
          MC_REGISTRY_FAILED_INITIALIZATION);
    CHECK(error.status == MC_REGISTRY_FAILED_INITIALIZATION);
    CHECK(error.active_initializer_class == wrapper.failure.active_initializer_class &&
          error.active_initializer_pc == wrapper.failure.active_initializer_pc &&
          error.constructor_pc == wrapper.failure.constructor_pc &&
          error.constructor_class == wrapper.failure.constructor_class);
    CHECK(mc_registry_classes_initialize_translation(owner, NULL) ==
          MC_REGISTRY_FAILED_INITIALIZATION);
    CHECK(mc_registry_classes_inspect_translation(owner, &after, &error) == MC_REGISTRY_OK);
    CHECK(same_view(&before, &after));
    CHECK(same_error(&singleton.failure, &owner->translation_state.failure));
    CHECK(same_error(&wrapper.failure, &owner->translation_wrapper_state.failure));
    CHECK(owner->translation_state.pc == singleton.pc &&
          owner->translation_wrapper_state.pc == wrapper.pc);
    CHECK(owner->allocation_bytes == bytes && owner->allocations == allocations);
    CHECK(owner->translations.pending == pending && (pending == NULL || pending->b == properties));
    CHECK(mc_java_properties_size(properties) == count);
    CHECK(r->opens == opens && r->streams[0].calls == reads[0] && r->streams[1].calls == reads[1]);
    CHECK(owner->entered == 0 && owner->initializer == NULL && owner->current_item == NULL &&
          owner->current_block == NULL);
    CHECK(resources_valid(r));
    result = 0;
cleanup:
    return result;
}

enum failure_case {
    NO_SOURCE,
    MISSING_FIRST,
    MISSING_SECOND,
    IO_FIRST,
    IO_SECOND,
    BAD_FIRST,
    BAD_SECOND
};

static int test_failure(mc_registry_side side, const dependencies *d, enum failure_case which) {
    int result = 1;
    mc_registry_classes *owner = NULL;
    resources r;
    resources_init(&r, SIZE_MAX);
    r.reenter = true;
    size_t expected_opens = 1, expected_count = 0;
    int32_t constructor_pc = 26;
    mc_registry_status expected = MC_REGISTRY_NULL_REFERENCE;
    if (which == NO_SOURCE) {
        expected = MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY;
        expected_opens = 0;
        constructor_pc = 23;
    } else if (which == MISSING_FIRST) {
        r.missing = 0;
    } else if (which == MISSING_SECOND) {
        r.missing = 1;
        expected_opens = 2;
        expected_count = 6;
        constructor_pc = 40;
    } else if (which == IO_FIRST || which == IO_SECOND) {
        size_t index = which == IO_FIRST ? 0 : 1;
        stream_text(&r.streams[index], index == 0 ? "early=one\nkept=two\nlast=notcommitted\n"
                                                  : "added=yes\ndup=changed\nlast=notcommitted\n");
        r.streams[index].fail_call = 2;
        expected = MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY;
        constructor_pc = 48;
        expected_opens = index + 1;
        expected_count = index == 0 ? 2 : 7;
    } else {
        size_t index = which == BAD_FIRST ? 0 : 1;
        stream_text(&r.streams[index], index == 0 ? "before=one\nbad=\\u00Q0\nlate=no\n"
                                                  : "added=yes\nbad=\\u00Q0\nlate=no\n");
        expected = MC_REGISTRY_ILLEGAL_ARGUMENT;
        constructor_pc = index == 0 ? 26 : 40;
        expected_opens = index + 1;
        expected_count = index == 0 ? 1 : 7;
    }
    CHECK(allocate(side, d, which == NO_SOURCE ? NULL : &r, 0, &owner, NULL) == MC_REGISTRY_OK);
    mc_registry_error error;
    CHECK(mc_registry_classes_initialize_translation(owner, &error) == expected);
    CHECK(error.status == expected && error.initializer_pc == -1);
    CHECK(error.active_initializer_class != NULL &&
          strcmp(error.active_initializer_class, side == MC_REGISTRY_CLIENT ? "nh" : "ii") == 0);
    CHECK(error.active_initializer_pc == 4 && error.constructor_pc == constructor_pc);
    CHECK(owner->translation_state.pc == 4 && owner->translation_wrapper_state.pc == 0);
    CHECK(owner->translation_state.failure.status == expected &&
          owner->translation_wrapper_state.failure.status == expected);
    CHECK(owner->translations.singleton_a == NULL && owner->translations.wrapper_a == NULL);
    const mc_translation *pending = owner->translations.pending;
    CHECK(pending != NULL && pending->b != NULL && pending->constructor_pc == constructor_pc);
    CHECK(mc_java_properties_size(pending->b) == expected_count);
    CHECK(r.opens == expected_opens && r.reentries == expected_opens && resources_valid(&r));
    const mc_java_properties_string *value = NULL;
    if (which == IO_FIRST) {
        CHECK(core_lookup(pending->b, "early", &value) == MC_JAVA_PROPERTIES_OK &&
              core_text(value, "one"));
        CHECK(core_lookup(pending->b, "kept", &value) == MC_JAVA_PROPERTIES_OK &&
              core_text(value, "two"));
    } else if (which == BAD_FIRST) {
        CHECK(core_lookup(pending->b, "before", &value) == MC_JAVA_PROPERTIES_OK &&
              core_text(value, "one"));
    } else if (expected_opens == 2) {
        CHECK(r.first_observed && core_text(r.old_dup, "first"));
        CHECK(core_lookup(pending->b, "only", &value) == MC_JAVA_PROPERTIES_OK &&
              core_text(value, "base"));
        CHECK(core_lookup(pending->b, "dup", &value) == MC_JAVA_PROPERTIES_OK);
        CHECK(core_text(value, which == IO_SECOND ? "changed" : "first"));
        if (which != MISSING_SECOND)
            CHECK(core_lookup(pending->b, "added", &value) == MC_JAVA_PROPERTIES_OK &&
                  core_text(value, "yes"));
    }
    CHECK(core_lookup(pending->b, "last", &value) == MC_JAVA_PROPERTIES_OK && value == NULL);
    CHECK(core_lookup(pending->b, "bad", &value) == MC_JAVA_PROPERTIES_OK && value == NULL);
    CHECK(core_lookup(pending->b, "late", &value) == MC_JAVA_PROPERTIES_OK && value == NULL);
    CHECK(r.streams[1].calls == 0 || expected_opens == 2);
    if (which == NO_SOURCE || which == MISSING_FIRST)
        CHECK(r.streams[0].calls == 0 && r.streams[1].calls == 0);
    if (which == MISSING_SECOND)
        CHECK(r.streams[1].calls == 0);
    if (which == IO_FIRST || which == IO_SECOND)
        CHECK(r.streams[which == IO_FIRST ? 0 : 1].calls == 2);
    CHECK(check_failed_retry(owner, &r) == 0 && dependencies_unchanged(d));
    result = 0;
cleanup:
    if (retire(owner) != 0)
        result = 1;
    return result;
}

typedef struct budget_observation {
    bool allocated, ready, pending, properties;
    size_t bytes, opens, entries;
} budget_observation;

/* Check a complete source-order prefix, rather than accepting an arbitrary
 * map of the right size. Native first-insertion enumeration is explicitly a
 * Properties API contract, not a Java Hashtable iteration-order assertion. */
static bool budget_prefix(const mc_java_properties *properties, size_t opens) {
    static const char *const keys[] = {"dup", "only",  "same", "alias",
                                       NULL,  "empty", "tail", "astral"};
    static const char *const values[] = {NULL, "base", "equal", "equal", NULL, "", "stats", NULL};
    size_t count = mc_java_properties_size(properties);
    if (count > COUNT(keys) || (count != 0 && opens == 0) || (count > 6 && opens != 2))
        return false;
    for (size_t i = 0; i < count; ++i) {
        const mc_java_properties_string *key = NULL, *value = NULL;
        if (mc_java_properties_entry(properties, i, &key, &value) != MC_JAVA_PROPERTIES_OK)
            return false;
        if (i == 4) {
            if (!core_units(key, nul_key, COUNT(nul_key)) ||
                !core_units(value, nul_value, COUNT(nul_value)))
                return false;
        } else if (!core_text(key, keys[i])) {
            return false;
        } else if (i == 0) {
            if (opens < 2 || count < 6) {
                if (!core_text(value, "first"))
                    return false;
            } else if (count > 6) {
                if (!core_text(value, "second"))
                    return false;
            } else if (!core_text(value, "first") && !core_text(value, "second")) {
                return false;
            }
        } else if (i == 7) {
            if (!core_units(value, astral_value, COUNT(astral_value)))
                return false;
        } else if (!core_text(value, values[i])) {
            return false;
        }
    }
    return true;
}

/* Each trial is a real fresh owner with an ordinary options byte bound. No
 * allocator hook, private mutation, clock substitution or allocation fault is
 * injected. The shared dependencies remain untouched between all trials. */
static int budget_trial(mc_registry_side side, const dependencies *d, size_t maximum,
                        budget_observation *observation) {
    int result = 1;
    mc_registry_classes *owner = NULL;
    resources r;
    resources_init(&r, 7);
    mc_registry_error error;
    mc_registry_status status = allocate(side, d, &r, maximum, &owner, &error);
    *observation = (budget_observation){0};
    if (status != MC_REGISTRY_OK) {
        CHECK(status == MC_REGISTRY_LIMIT && error.status == MC_REGISTRY_LIMIT);
        CHECK(owner == NULL && r.opens == 0 && maximum < sizeof(mc_registry_classes));
    } else {
        observation->allocated = true;
        status = mc_registry_classes_initialize_translation(owner, &error);
        CHECK(status == MC_REGISTRY_OK || status == MC_REGISTRY_LIMIT);
        observation->ready = status == MC_REGISTRY_OK;
        const mc_translation *translation = owner->translations.singleton_a != NULL
                                                ? owner->translations.singleton_a
                                                : owner->translations.pending;
        observation->pending = translation != NULL;
        observation->properties = translation != NULL && translation->b != NULL;
        observation->entries = translation == NULL ? 0 : mc_java_properties_size(translation->b);
        observation->bytes = owner->allocation_bytes;
        observation->opens = r.opens;
        CHECK(observation->bytes <= maximum && observation->entries <= 8 && r.opens <= 2);
        CHECK(budget_prefix(translation == NULL ? NULL : translation->b, r.opens));
        if (status == MC_REGISTRY_OK) {
            CHECK(check_loaded(owner, &r) == 0);
        } else {
            CHECK(error.status == MC_REGISTRY_LIMIT);
            CHECK(owner->translations.singleton_a == NULL && owner->translations.wrapper_a == NULL);
            CHECK(check_failed_retry(owner, &r) == 0);
        }
        CHECK(registry_untouched(owner) && resources_valid(&r));
    }
    CHECK(dependencies_unchanged(d));
    result = 0;
cleanup:
    if (retire(owner) != 0)
        result = 1;
    return result;
}

static int test_budgets(mc_registry_side side, const dependencies *d) {
    int result = 1;
    const size_t upper = 4u * 1024u * 1024u;
    size_t low = sizeof(mc_registry_classes), high = upper;
    budget_observation observed;
    CHECK(budget_trial(side, d, upper, &observed) == 0 && observed.ready);
    while (low < high) {
        size_t middle = low + (high - low) / 2;
        CHECK(budget_trial(side, d, middle, &observed) == 0);
        if (observed.ready)
            high = middle;
        else
            low = middle + 1;
    }
    CHECK(low > sizeof(mc_registry_classes) && low < upper);
    const size_t budgets[] = {sizeof(mc_registry_classes) - 1,
                              sizeof(mc_registry_classes),
                              sizeof(mc_registry_classes) + 1,
                              sizeof(mc_registry_classes) + (low - sizeof(mc_registry_classes)) / 2,
                              low - 1,
                              low,
                              low + 1,
                              upper};
    budget_observation previous = {0};
    for (size_t i = 0; i < COUNT(budgets); ++i) {
        budget_observation current;
        CHECK(budget_trial(side, d, budgets[i], &current) == 0);
        CHECK(current.ready == (budgets[i] >= low));
        CHECK(current.allocated >= previous.allocated && current.pending >= previous.pending &&
              current.properties >= previous.properties && current.opens >= previous.opens &&
              current.entries >= previous.entries && current.bytes >= previous.bytes);
        previous = current;
        if (budgets[i] == low - 1) {
            CHECK(current.allocated && !current.ready && current.pending && current.properties);
            CHECK(current.opens == 2 && current.entries == 8);
        }
    }
    result = 0;
cleanup:
    return result;
}

static int test_invalid_source(mc_registry_side side, const dependencies *d) {
    int result = 1;
    resources r;
    resources_init(&r, 7);
    mc_registry_classes *owner = NULL;
    CHECK(allocate(side, d, &r, 0, &owner, NULL) == MC_REGISTRY_OK);
    mc_registry_classes *out = owner;
    const mc_registry_resources invalid = {&r, NULL, NULL, NULL};
    const mc_registry_options options = {side, d->profile, d->materials, 0, &invalid};
    CHECK(mc_registry_classes_allocate(&options, &out, NULL) == MC_REGISTRY_INVALID_ARGUMENT);
    CHECK(out == owner && r.opens == 0 && registry_untouched(owner) && dependencies_unchanged(d));
    result = 0;
cleanup:
    if (retire(owner) != 0)
        result = 1;
    return result;
}

static int test_side(mc_registry_side side, const dependencies *d) {
    int result = 1;
    CHECK(test_cold(side, d) == 0 && test_invalid_source(side, d) == 0);
    const size_t chunks[] = {SIZE_MAX, 1, 7, 8192};
    for (size_t i = 0; i < COUNT(chunks); ++i)
        CHECK(test_success(side, d, chunks[i]) == 0);
    for (int i = NO_SOURCE; i <= BAD_SECOND; ++i)
        CHECK(test_failure(side, d, (enum failure_case)i) == 0);
    CHECK(test_budgets(side, d) == 0 && dependencies_unchanged(d));
    result = 0;
cleanup:
    return result;
}

int main(void) {
    int result = 1;
    dependencies d = {0};
    CHECK(mc_materials_create(&d.materials));
    CHECK(mc_java_random_profile_create(1024u * 1024u, &d.profile) == MC_JAVA_RANDOM_PROFILE_OK);
    capture_dependencies(&d);
    CHECK(d.holder == MC_JAVA_RANDOM_HOLDER_COLD && d.math == NULL);
    CHECK(test_side(MC_REGISTRY_CLIENT, &d) == 0);
    CHECK(test_side(MC_REGISTRY_SERVER, &d) == 0);
    CHECK(dependencies_unchanged(&d) && !retirement_failed);
    result = 0;
cleanup:
    /* Every class owner retires inside its source-context scope. If a broken
     * lifetime contract refuses retirement, do not free borrowed dependencies. */
    if (!retirement_failed) {
        mc_java_random_profile_free(d.profile);
        mc_materials_destroy(d.materials);
    }
    if (result == 0 && printf("Translation native contract: %u checks passed\n", checks) < 0)
        result = 1;
    return result;
}
