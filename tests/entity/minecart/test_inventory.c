/* Source-derived inventory contracts on real native owners. No original
 * observations, Player substitutes, GAME storage or constructor oracle. */
#include "../../../src/entity/minecart/entity_internal.h"
#include "../../../src/entity/minecart/inventory_internal.h"
#include "mc/entity/minecart/inventory.h"

#include <emmintrin.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

static unsigned int checks, cases;
#define COUNT(a) (sizeof(a) / sizeof((a)[0]))
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        const bool ok_ = !!(condition);                                                            \
        if (!ok_) {                                                                                \
            fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #condition);                \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

typedef struct fixture {
    mc_java_random_profile *profile;
    mc_entity_classes *classes;
    mc_entity *cart;
} fixture;

static bool open_classes(fixture *f, mc_entity_source source) {
    return mc_java_random_profile_create(SIZE_MAX, &f->profile) == MC_JAVA_RANDOM_PROFILE_OK &&
           mc_entity_classes_create(source, f->profile, NULL, &f->classes) == MC_ENTITY_OK;
}

static bool open_cart(fixture *f, mc_entity_source source, int32_t type) {
    return open_classes(f, source) &&
           mc_minecart_entity_allocate(f->classes, &f->cart, NULL) == MC_ENTITY_OK &&
           mc_minecart_entity_construct(f->cart, NULL, 3.0, 5.0, 7.0, type, NULL) == MC_ENTITY_OK;
}

static bool close_fixture(fixture *f) {
    mc_entity_release(f->cart);
    f->cart = NULL;
    if (mc_entity_classes_free(f->classes) != MC_ENTITY_OK)
        return false;
    f->classes = NULL;
    mc_java_random_profile_free(f->profile);
    f->profile = NULL;
    return true;
}

static bool make_stack(mc_item_stack **out, int32_t id, int32_t count, int32_t damage) {
    return mc_item_stack_allocate(out) == MC_ITEM_STACK_OK &&
           mc_item_stack_construct(*out, id, count, damage) == MC_ITEM_STACK_OK;
}

static bool stack_is(const mc_item_stack *stack, int32_t id, int32_t count, int32_t damage) {
    mc_item_stack_fields fields;
    return mc_item_stack_fields_get(stack, &fields) == MC_ITEM_STACK_OK && fields.id == id &&
           fields.count == count && fields.damage == damage;
}

static bool phase_is(const mc_item_stack *stack, mc_item_stack_phase phase) {
    mc_item_stack_phase actual;
    return mc_item_stack_phase_get(stack, &actual) == MC_ITEM_STACK_OK && actual == phase;
}

static bool site_is(const mc_minecart_site *site, mc_entity_source source, const char *method,
                    uint16_t pc) {
    return site->selected && site->entered && site->declaring_class != NULL &&
           strcmp(site->declaring_class, source == MC_ENTITY_CLIENT ? "yl" : "pr") == 0 &&
           site->method != NULL && strcmp(site->method, method) == 0 && site->bytecode == pc;
}

static bool neutral_site(const mc_minecart_site *site) {
    return !site->selected && !site->entered && site->declaring_class == NULL &&
           site->method == NULL && site->bytecode == 0;
}

static bool fresh(const mc_minecart_inventory_error *error) {
    return error->context.status == MC_MINECART_INVENTORY_OK && error->context.operation == NULL &&
           !error->context.slot_selected && error->context.slot == 0 &&
           neutral_site(&error->context.current) &&
           error->context.stack_cause == MC_MINECART_INVENTORY_STACK_NONE &&
           neutral_site(&error->context.stack_split) &&
           neutral_site(&error->context.stack_constructor) &&
           neutral_site(&error->context.stack_read) && error->stack.status == MC_ITEM_STACK_OK &&
           error->stack.field == NULL && error->stack.nbt.status == MC_NBT_OK &&
           error->stack.nbt.offset == 0;
}

static bool class_unchanged(const mc_entity_class_snapshot *a, const mc_entity_class_snapshot *b) {
    return a->source == b->source && a->entity_class == b->entity_class &&
           a->bounds_class == b->bounds_class && a->watcher_class == b->watcher_class &&
           a->next_id == b->next_id && a->bounds_pool_cursor == b->bounds_pool_cursor &&
           a->bounds_pool_identity == b->bounds_pool_identity &&
           a->watcher_types_identity == b->watcher_types_identity &&
           a->watcher_type_count == b->watcher_type_count && a->live_entities == b->live_entities &&
           a->allocation_bytes == b->allocation_bytes;
}

static bool name_is_minecart(mc_nbt_string name) {
    static const uint16_t expected[] = {'M', 'i', 'n', 'e', 'c', 'a', 'r', 't'};
    return name.length == COUNT(expected) && name.units != NULL &&
           memcmp(name.units, expected, sizeof(expected)) == 0;
}

static bool failed_stack(mc_item_stack **out) {
    static const uint16_t id[] = {'i', 'd'}, count[] = {'C', 'o', 'u', 'n', 't'},
                          damage[] = {'D', 'a', 'm', 'a', 'g', 'e'};
    const mc_nbt_tag fields[] = {
        {.type = MC_NBT_SHORT, .name = {id, COUNT(id)}, .value.short_value = -12},
        {.type = MC_NBT_BYTE, .name = {count, COUNT(count)}, .value.byte_value = 100},
        {.type = MC_NBT_BYTE, .name = {damage, COUNT(damage)}, .value.byte_value = 7}};
    const mc_nbt_tag *items[] = {&fields[0], &fields[1], &fields[2]};
    const mc_nbt_tag root = {.type = MC_NBT_COMPOUND,
                             .value.compound = {.items = items, .length = COUNT(items)}};
    mc_item_stack_error error;
    return mc_item_stack_allocate(out) == MC_ITEM_STACK_OK &&
           mc_item_stack_construct_nbt(*out, &root, &error) == MC_ITEM_STACK_INVALID_FIELD &&
           error.field != NULL && strcmp(error.field, "Damage") == 0 &&
           phase_is(*out, MC_ITEM_STACK_FAILED_CONSTRUCTION);
}

static int raw_constants(mc_entity_source source) {
    int result = 1;
    fixture f = {0};
    mc_entity *other = NULL;
    mc_entity_snapshot before, after;
    mc_entity_class_snapshot class_before, class_after;
    mc_minecart_inventory_error error;
    mc_nbt_string name = {0};
    mc_item_stack *out = NULL;
    int32_t size = -1, limit = -1;
    ++cases;
    CHECK(open_classes(&f, source));
    CHECK(mc_minecart_entity_allocate(f.classes, &f.cart, NULL) == MC_ENTITY_OK);
    CHECK(mc_entity_get_snapshot(f.cart, &before) == MC_ENTITY_OK);
    CHECK(before.phase == MC_ENTITY_RAW);
    CHECK(mc_entity_classes_get_snapshot(f.classes, &class_before) == MC_ENTITY_OK);
    memset(&error, 0x5a, sizeof(error));
    CHECK(mc_minecart_inventory_size(f.cart, &size, &error) == MC_MINECART_INVENTORY_OK);
    CHECK(size == 27);
    CHECK(fresh(&error));
    CHECK(mc_minecart_inventory_limit(f.cart, &limit, &error) == MC_MINECART_INVENTORY_OK);
    CHECK(limit == 64);
    CHECK(mc_minecart_inventory_name(f.cart, &name, &error) == MC_MINECART_INVENTORY_OK);
    CHECK(name_is_minecart(name));
    CHECK(mc_minecart_inventory_changed(f.cart, &error) == MC_MINECART_INVENTORY_OK);
    CHECK(fresh(&error));
    CHECK(mc_entity_get_snapshot(f.cart, &after) == MC_ENTITY_OK);
    CHECK(after.phase == before.phase && after.bounds == before.bounds &&
          after.random == before.random && after.watcher == before.watcher);
    CHECK(mc_entity_classes_get_snapshot(f.classes, &class_after) == MC_ENTITY_OK);
    CHECK(class_unchanged(&class_before, &class_after));
    CHECK(mc_minecart_inventory_get(f.cart, 0, &out, &error) ==
          MC_MINECART_INVENTORY_NULL_REFERENCE);
    CHECK(out == NULL);
    CHECK(site_is(&error.context.current, source,
                  source == MC_ENTITY_CLIENT ? "f_(I)Liz;" : "d_(I)Lfy;", 5));
    CHECK(mc_minecart_inventory_set(f.cart, 0, NULL, &error) ==
          MC_MINECART_INVENTORY_NULL_REFERENCE);
    CHECK(site_is(&error.context.current, source,
                  source == MC_ENTITY_CLIENT ? "a(ILiz;)V" : "a(ILfy;)V", 6));
    CHECK(mc_falling_entity_allocate(f.classes, &other, NULL) == MC_ENTITY_OK);
    size = -91;
    CHECK(mc_minecart_inventory_size(other, &size, &error) ==
          MC_MINECART_INVENTORY_INVALID_ARGUMENT);
    CHECK(size == -91);
    CHECK(!error.context.current.selected && !error.context.current.entered);
    result = 0;
cleanup:
    mc_entity_release(other);
    if (!close_fixture(&f))
        result = 1;
    return result;
}

static int types_and_bounds(mc_entity_source source) {
    static const int32_t types[] = {0, 1, 2, -1, INT32_MIN, INT32_MAX};
    for (size_t n = 0; n < COUNT(types); ++n) {
        int result = 1;
        fixture f = {0};
        mc_item_stack *stack = NULL, *held = NULL;
        mc_minecart_entity_snapshot before, after;
        mc_minecart_inventory_error error;
        int32_t size = -1;
        ++cases;
        CHECK(open_cart(&f, source, types[n]));
        CHECK(make_stack(&stack, -32769, 65, 65537));
        CHECK(mc_minecart_entity_get_snapshot(f.cart, &before) == MC_ENTITY_OK);
        CHECK(before.fields.d == types[n] && before.inventory_length == 36);
        CHECK(mc_minecart_inventory_size(f.cart, &size, &error) == MC_MINECART_INVENTORY_OK);
        CHECK(size == 27);
        CHECK(mc_minecart_inventory_set(f.cart, 35, stack, &error) == MC_MINECART_INVENTORY_OK);
        CHECK(stack_is(stack, -32769, 64, 65537));
        CHECK(mc_minecart_inventory_get(f.cart, 35, &held, &error) == MC_MINECART_INVENTORY_OK);
        CHECK(held == stack);
        mc_item_stack_release(held);
        held = NULL;
        CHECK(mc_minecart_inventory_set(f.cart, 27, stack, &error) == MC_MINECART_INVENTORY_OK);
        CHECK(mc_minecart_entity_get_snapshot(f.cart, &after) == MC_ENTITY_OK);
        CHECK(after.fields.h == before.fields.h && after.inventory_length == 36 &&
              after.inventory[27] == stack && after.inventory[35] == stack);
        CHECK(mc_minecart_inventory_get(f.cart, -1, &held, &error) ==
              MC_MINECART_INVENTORY_INDEX_OUT_OF_BOUNDS);
        CHECK(held == NULL);
        CHECK(mc_minecart_inventory_get(f.cart, 36, &held, &error) ==
              MC_MINECART_INVENTORY_INDEX_OUT_OF_BOUNDS);
        CHECK(mc_minecart_inventory_set(f.cart, INT32_MAX, NULL, &error) ==
              MC_MINECART_INVENTORY_INDEX_OUT_OF_BOUNDS);
        CHECK(mc_minecart_inventory_split(f.cart, INT32_MIN, 1, &held, &error) ==
              MC_MINECART_INVENTORY_INDEX_OUT_OF_BOUNDS);
        CHECK(held == NULL);
        result = 0;
    cleanup:
        mc_item_stack_release(held);
        mc_item_stack_release(stack);
        if (!close_fixture(&f))
            result = 1;
        if (result != 0)
            return result;
    }
    return 0;
}

static int occurrence_lifetimes(mc_entity_source source) {
    int result = 1;
    fixture f = {0};
    mc_entity *second = NULL;
    mc_item_stack *stack = NULL, *get = NULL, *removed = NULL;
    mc_item_stack *identity = NULL;
    mc_minecart_inventory_error error;
    ++cases;
    CHECK(open_cart(&f, source, INT32_MIN));
    CHECK(mc_minecart_entity_allocate(f.classes, &second, NULL) == MC_ENTITY_OK);
    CHECK(mc_minecart_entity_construct_world(second, NULL, NULL) == MC_ENTITY_OK);
    CHECK(make_stack(&stack, 4, 10, 8));
    identity = stack;
    CHECK(mc_minecart_inventory_set(f.cart, 0, stack, &error) == MC_MINECART_INVENTORY_OK);
    CHECK(mc_minecart_inventory_set(f.cart, 1, stack, &error) == MC_MINECART_INVENTORY_OK);
    CHECK(mc_minecart_inventory_set(second, 35, stack, &error) == MC_MINECART_INVENTORY_OK);
    CHECK(mc_item_stack_count_set(stack, 99) == MC_ITEM_STACK_OK);
    /* Self-assignment must store the same owner safely, then mutate that shared
     * identity. No copy or item-specific maximum is consulted. */
    CHECK(mc_minecart_inventory_set(f.cart, 0, stack, &error) == MC_MINECART_INVENTORY_OK);
    CHECK(stack_is(stack, 4, 64, 8));
    CHECK(mc_minecart_inventory_get(f.cart, 1, &get, &error) == MC_MINECART_INVENTORY_OK);
    CHECK(get == identity);
    mc_item_stack_release(stack);
    stack = NULL;
    CHECK(mc_minecart_inventory_set(f.cart, 0, NULL, &error) == MC_MINECART_INVENTORY_OK);
    CHECK(mc_minecart_inventory_split(f.cart, 1, 64, &removed, &error) == MC_MINECART_INVENTORY_OK);
    CHECK(removed == identity && removed == get);
    mc_entity_release(f.cart);
    f.cart = NULL;
    CHECK(stack_is(removed, 4, 64, 8));
    mc_item_stack_release(get);
    get = NULL;
    CHECK(mc_minecart_inventory_set(second, 35, NULL, &error) == MC_MINECART_INVENTORY_OK);
    mc_entity_release(second);
    second = NULL;
    CHECK(stack_is(removed, 4, 64, 8));
    result = 0;
cleanup:
    mc_item_stack_release(removed);
    mc_item_stack_release(get);
    mc_item_stack_release(stack);
    mc_entity_release(second);
    if (!close_fixture(&f))
        result = 1;
    return result;
}

static int split_vectors(mc_entity_source source) {
    static const struct {
        int32_t count, amount, remainder, returned;
        bool whole;
    } vectors[] = {{7, 3, 4, 3, false},
                   {7, 7, 0, 7, true},
                   {7, 8, 0, 7, true},
                   {7, 0, 7, 0, false},
                   {7, -2, 9, -2, false},
                   {0, -1, 1, -1, false},
                   {-7, -8, 1, -8, false},
                   {INT32_MIN, INT32_MIN, 0, INT32_MIN, true},
                   {1, INT32_MIN, INT32_MIN + 1, INT32_MIN, false},
                   {INT32_MAX, -1, INT32_MIN, -1, false}};

    for (size_t n = 0; n < COUNT(vectors); ++n) {
        int result = 1;
        fixture f = {0};
        mc_item_stack *stack = NULL, *out = NULL, *slot = NULL;
        mc_minecart_inventory_error error;
        ++cases;
        CHECK(open_cart(&f, source, -42));
        CHECK(make_stack(&stack, -80, vectors[n].count, -21));
        CHECK(mc_minecart_inventory_set(f.cart, 4, stack, &error) == MC_MINECART_INVENTORY_OK);
        /* Public, real Stack mutation supplies a count above64 after insertion;
         * the split method itself must not silently reapply setter's clamp. */
        CHECK(mc_item_stack_count_set(stack, vectors[n].count) == MC_ITEM_STACK_OK);
        CHECK(mc_minecart_inventory_split(f.cart, 4, vectors[n].amount, &out, &error) ==
              MC_MINECART_INVENTORY_OK);
        CHECK(out != NULL && stack_is(out, -80, vectors[n].returned, -21));
        CHECK((out == stack) == vectors[n].whole);
        CHECK(mc_minecart_inventory_get(f.cart, 4, &slot, &error) == MC_MINECART_INVENTORY_OK);
        if (vectors[n].whole)
            CHECK(slot == NULL);
        else {
            CHECK(slot == stack && stack_is(slot, -80, vectors[n].remainder, -21));
            CHECK(phase_is(out, MC_ITEM_STACK_READY));
            CHECK(mc_item_stack_source(out) == NULL);
        }
        result = 0;
    cleanup:
        mc_item_stack_release(slot);
        mc_item_stack_release(out);
        mc_item_stack_release(stack);
        if (!close_fixture(&f))
            result = 1;
        if (result != 0)
            return result;
    }
    return 0;
}

static int all_stack_phases(mc_entity_source source) {
    int result = 1;
    fixture f = {0};
    mc_item_stack *raw = NULL, *failed = NULL, *a = NULL, *b = NULL, *get = NULL;
    mc_minecart_inventory_error error;
    ++cases;
    CHECK(open_cart(&f, source, 2));
    CHECK(mc_item_stack_allocate(&raw) == MC_ITEM_STACK_OK);
    CHECK(failed_stack(&failed));
    CHECK(stack_is(raw, 0, 0, 0) && phase_is(raw, MC_ITEM_STACK_RAW));
    CHECK(stack_is(failed, -12, 100, 0));
    CHECK(mc_minecart_inventory_set(f.cart, 0, raw, &error) == MC_MINECART_INVENTORY_OK);
    CHECK(mc_minecart_inventory_set(f.cart, 1, failed, &error) == MC_MINECART_INVENTORY_OK);
    CHECK(stack_is(failed, -12, 64, 0));
    CHECK(phase_is(failed, MC_ITEM_STACK_FAILED_CONSTRUCTION));
    CHECK(mc_minecart_inventory_get(f.cart, 0, &get, &error) == MC_MINECART_INVENTORY_OK);
    CHECK(get == raw);
    mc_item_stack_release(get);
    get = NULL;
    CHECK(mc_minecart_inventory_split(f.cart, 0, -1, &a, &error) == MC_MINECART_INVENTORY_OK);
    CHECK(a != raw && stack_is(a, 0, -1, 0) && stack_is(raw, 0, 1, 0));
    CHECK(phase_is(raw, MC_ITEM_STACK_RAW) && phase_is(a, MC_ITEM_STACK_READY));
    CHECK(mc_minecart_inventory_split(f.cart, 1, 2, &b, &error) == MC_MINECART_INVENTORY_OK);
    CHECK(b != failed && stack_is(b, -12, 2, 0) && stack_is(failed, -12, 62, 0));
    CHECK(phase_is(failed, MC_ITEM_STACK_FAILED_CONSTRUCTION));
    CHECK(mc_minecart_inventory_split(f.cart, 35, INT32_MIN, &get, &error) ==
          MC_MINECART_INVENTORY_OK);
    CHECK(get == NULL);
    result = 0;
cleanup:
    mc_item_stack_release(get);
    mc_item_stack_release(a);
    mc_item_stack_release(b);
    mc_item_stack_release(raw);
    mc_item_stack_release(failed);
    if (!close_fixture(&f))
        result = 1;
    return result;
}

/* Only this explicit call uses a refusing allocator. These fixture globals
 * observe callback ordering; they do not configure production or any later
 * operation. No native owner is fabricated or published by refusal. */
static mc_item_stack *refusal_source;
static unsigned int refusal_calls;
static int32_t refusal_count;
static bool refusal_observed;

static mc_item_stack_status refuse_stack_allocation(mc_item_stack **out) {
    mc_item_stack_fields fields;
    (void)out;
    ++refusal_calls;
    refusal_observed = mc_item_stack_fields_get(refusal_source, &fields) == MC_ITEM_STACK_OK;
    if (refusal_observed)
        refusal_count = fields.count;
    return MC_ITEM_STACK_ALLOCATION_FAILED;
}

static int native_refusal_prefix(mc_entity_source source) {
    int result = 1;
    fixture f = {0};
    mc_item_stack *stack = NULL, *out = NULL, *held = NULL;
    mc_minecart_inventory_error error;
    ++cases;
    CHECK(open_cart(&f, source, 0));
    CHECK(make_stack(&stack, -91, 9, 17));
    CHECK(mc_minecart_inventory_set(f.cart, 2, stack, &error) == MC_MINECART_INVENTORY_OK);
    refusal_source = stack;
    refusal_calls = 0;
    refusal_observed = false;
    const mc_minecart_inventory_status status = mc_minecart_inventory_split_with_allocator(
        f.cart, 2, 3, refuse_stack_allocation, &out, &error);
    refusal_source = NULL;
    CHECK(status == MC_MINECART_INVENTORY_ALLOCATION_FAILED);
    CHECK(out == NULL && refusal_calls == 1 && refusal_observed && refusal_count == 6);
    CHECK(stack_is(stack, -91, 6, 17));
    CHECK(site_is(&error.context.current, source,
                  source == MC_ENTITY_CLIENT ? "a(II)Liz;" : "a(II)Lfy;", 45));
    CHECK(error.context.stack_cause == MC_MINECART_INVENTORY_STACK_SPLIT);
    CHECK(error.context.stack_split.selected && error.context.stack_split.entered &&
          error.context.stack_split.bytecode == 10);
    CHECK(neutral_site(&error.context.stack_constructor) &&
          neutral_site(&error.context.stack_read));
    CHECK(error.stack.status == MC_ITEM_STACK_ALLOCATION_FAILED);
    CHECK(mc_minecart_inventory_get(f.cart, 2, &held, &error) == MC_MINECART_INVENTORY_OK);
    CHECK(held == stack && fresh(&error));
    mc_item_stack_release(held);
    held = NULL;
    refusal_calls = 0;
    CHECK(mc_minecart_inventory_split_with_allocator(f.cart, 2, 6, refuse_stack_allocation, &out,
                                                     &error) == MC_MINECART_INVENTORY_OK);
    CHECK(out == stack && refusal_calls == 0 && fresh(&error));
    mc_item_stack_release(out);
    out = NULL;
    CHECK(mc_minecart_inventory_split_with_allocator(f.cart, 35, INT32_MIN, refuse_stack_allocation,
                                                     &out, &error) == MC_MINECART_INVENTORY_OK);
    CHECK(out == NULL && refusal_calls == 0);
    result = 0;
cleanup:
    refusal_source = NULL;
    mc_item_stack_release(held);
    mc_item_stack_release(out);
    mc_item_stack_release(stack);
    if (!close_fixture(&f))
        result = 1;
    return result;
}

int main(void) {
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u) {
        fputs("Minecart inventory contracts require strict SSE arithmetic\n", stderr);
        return 1;
    }
    for (mc_entity_source source = MC_ENTITY_CLIENT; source <= MC_ENTITY_SERVER; ++source)
        if (raw_constants(source) || types_and_bounds(source) || occurrence_lifetimes(source) ||
            split_vectors(source) || all_stack_phases(source) || native_refusal_prefix(source))
            return 1;
    if (cases != 40u) {
        fprintf(stderr, "unexpected source-case census: %u\n", cases);
        return 1;
    }
    printf("PASS: %u Minecart inventory checks in %u source cases\n", checks, cases);
    return 0;
}
