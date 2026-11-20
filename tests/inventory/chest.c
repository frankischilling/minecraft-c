#include "../../src/inventory/chest_internal.h"
#include "../../src/inventory/container/interaction_internal.h"
#include "../../src/inventory/player/source_internal.h"
#include "../reference/inventory/chest_container/native-observations.h"
#include "mc/resources/files/files.h"
#include "mc/world/runtime/construction.h"
#include "mc/world/runtime/tile_entities/tile_entities.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

static unsigned checks;
#define CHECK(v)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(v)) {                                                                                \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #v);                                \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

typedef struct leaf {
    int32_t size, second_size, limit, last_index;
    unsigned sizes, gets, sets, changes, usable_calls, releases;
    bool dynamic, usable, fail_change;
    mc_item_stack *slots[64];
} leaf;

static void leaf_release(void *context) {
    ++((leaf *)context)->releases;
}

static mc_inventory_status leaf_size(void *context, int32_t *out, mc_inventory_error *e) {
    leaf *o = context;
    (void)e;
    *out = o->dynamic && o->sizes != 0 ? o->second_size : o->size;
    ++o->sizes;
    return MC_INVENTORY_OK;
}

static mc_inventory_status leaf_name(void *context, const char **out, mc_inventory_error *e) {
    (void)context;
    (void)e;
    *out = "Chest";
    return MC_INVENTORY_OK;
}

static mc_inventory_status leaf_limit(void *context, int32_t *out, mc_inventory_error *e) {
    (void)e;
    *out = ((leaf *)context)->limit;
    return MC_INVENTORY_OK;
}

static mc_inventory_status leaf_get(void *context, int32_t index, const mc_item_stack **out,
                                    mc_inventory_error *e) {
    leaf *o = context;
    ++o->gets;
    o->last_index = index;
    if (index < 0 || index >= 64) {
        *e = (mc_inventory_error){.status = MC_INVENTORY_INDEX_OUT_OF_BOUNDS,
                                  .operation = "controlled LW signed index"};
        return e->status;
    }
    *out = o->slots[index];
    return MC_INVENTORY_OK;
}

static mc_inventory_status leaf_set(void *context, int32_t index, mc_item_stack *value,
                                    mc_inventory_error *e) {
    leaf *o = context;
    const mc_item_stack *old;
    if (leaf_get(context, index, &old, e) != MC_INVENTORY_OK)
        return e->status;
    if (inv_hold(value, e) != MC_INVENTORY_OK)
        return e->status;
    o->slots[index] = value;
    mc_item_stack_release((mc_item_stack *)old);
    ++o->sets;
    return MC_INVENTORY_OK;
}

static mc_inventory_status leaf_split(void *context, int32_t index, int32_t amount,
                                      mc_item_stack **out, mc_inventory_error *e) {
    const mc_item_stack *value;
    (void)amount;
    if (leaf_get(context, index, &value, e) != MC_INVENTORY_OK)
        return e->status;
    return inv_publish_hold(value, out, e); /* Controlled original LW protocol. */
}

static mc_inventory_status leaf_changed(void *context, mc_inventory_error *e) {
    leaf *o = context;
    ++o->changes;
    if (o->fail_change) {
        *e = (mc_inventory_error){.status = MC_INVENTORY_UNIMPLEMENTED_DEPENDENCY,
                                  .operation = "controlled LW changed failure"};
        return e->status;
    }
    return MC_INVENTORY_OK;
}

static mc_inventory_status leaf_usable(void *context, mc_entity *player, bool *out,
                                       mc_inventory_error *e) {
    leaf *o = context;
    (void)player;
    (void)e;
    ++o->usable_calls;
    *out = o->usable;
    return MC_INVENTORY_OK;
}

static const inv_chest_leaf leaves = {leaf_release, leaf_size,  leaf_name,    leaf_limit,  leaf_get,
                                      leaf_set,     leaf_split, leaf_changed, leaf_usable, NULL};

static void clear(leaf *o) {
    for (size_t i = 0; i < 64; ++i) {
        mc_item_stack_release(o->slots[i]);
        o->slots[i] = NULL;
    }
}

static mc_item_stack *stack(int32_t count) {
    mc_item_stack *o = NULL;
    if (mc_item_stack_allocate(&o) != MC_ITEM_STACK_OK)
        return NULL;
    if (mc_item_stack_construct(o, 3, count, 0) != MC_ITEM_STACK_OK) {
        mc_item_stack_release(o);
        return NULL;
    }
    return o;
}

static bool count_is(const mc_item_stack *o, int32_t expected) {
    int32_t count;
    return o != NULL && mc_item_stack_source_count_get(o, &count) == MC_ITEM_STACK_OK &&
           count == expected;
}

static bool protocol(void) {
    bool ok = false, usable = true;
    mc_inventory_chest *a = NULL, *b = NULL, *pair = NULL, *raw = NULL;
    leaf left = {.size = 27, .limit = 17, .usable = false};
    leaf right = {.size = 27, .limit = 64, .usable = true};
    mc_inventory_error e = {0};
    CHECK(inv_chest_leaf_create(MC_REGISTRY_CLIENT, &left, NULL, &leaves, NULL, &a, &e) ==
          MC_INVENTORY_OK);
    CHECK(inv_chest_leaf_create(MC_REGISTRY_CLIENT, &right, NULL, &leaves, NULL, &b, &e) ==
          MC_INVENTORY_OK);
    CHECK(mc_inventory_chest_large_create(MC_REGISTRY_CLIENT, NULL, a, b, NULL, &pair, &e) ==
          MC_INVENTORY_OK);
    int32_t value = -1;
    CHECK(mc_inventory_chest_size(pair, &value, &e) == MC_INVENTORY_OK && value == 54);
    const char *name = "sentinel";
    CHECK(mc_inventory_chest_name(pair, &name, &e) == MC_INVENTORY_OK && name == NULL);
    CHECK(mc_inventory_chest_limit(pair, &value, &e) == MC_INVENTORY_OK && value == 17);
    left.size = INT_MAX;
    right.size = 2;
    CHECK(mc_inventory_chest_size(pair, &value, &e) == MC_INVENTORY_OK && value == INT_MIN + 1);
    left.size = right.size = 27;
    CHECK(mc_inventory_chest_usable(pair, NULL, &usable, &e) == MC_INVENTORY_OK && !usable &&
          left.usable_calls == 1 && right.usable_calls == 0);
    const mc_item_stack *borrowed = NULL;
    left.sizes = 0;
    left.dynamic = true;
    left.second_size = 28;
    CHECK(mc_inventory_chest_borrow(pair, 27, &borrowed, &e) == MC_INVENTORY_INDEX_OUT_OF_BOUNDS &&
          left.sizes == 2 && right.last_index == -1 && e.frames[0].pc == 28);
    left.sizes = 0;
    left.second_size = 26;
    CHECK(mc_inventory_chest_borrow(pair, 27, &borrowed, &e) == MC_INVENTORY_OK &&
          left.sizes == 2 && right.last_index == 1);
    left.dynamic = false;
    left.fail_change = true;
    CHECK(mc_inventory_chest_changed(pair, &e) == MC_INVENTORY_UNIMPLEMENTED_DEPENDENCY &&
          left.changes == 1 && right.changes == 0 && e.frames[0].pc == 4);
    CHECK(mc_inventory_chest_large_allocate(MC_REGISTRY_CLIENT, NULL, &raw, &e) == MC_INVENTORY_OK);
    value = 123;
    CHECK(mc_inventory_chest_size(raw, &value, &e) == MC_INVENTORY_NULL_REFERENCE && value == 123 &&
          e.frames[0].pc == 4);
    CHECK(mc_inventory_chest_large_construct(raw, "Large chest", a, a, &e) == MC_INVENTORY_OK);
    size_t paths;
    inv_chest_leaf_view first, second;
    CHECK(inv_chest_leaf_count(raw, &paths, &e) == MC_INVENTORY_OK && paths == 2 &&
          inv_chest_leaf_at(raw, 0, &first, &e) == MC_INVENTORY_OK &&
          inv_chest_leaf_at(raw, 1, &second, &e) == MC_INVENTORY_OK &&
          first.owner == second.owner && first.native_holds == 4);
    inv_chest_node_view node;
    CHECK(inv_chest_node_count(raw, &paths, &e) == MC_INVENTORY_OK && paths == 3 &&
          inv_chest_node_at(raw, 0, &node, &e) == MC_INVENTORY_OK && node.large &&
          node.left == node.right && node.phase == INV_CHEST_READY && !node.terminal_sealed);
    CHECK(inv_chest_terminal_admit(raw, false, &e) == MC_INVENTORY_OK);
    inv_chest_terminal_seal(raw);
    CHECK(inv_chest_terminal_admit(raw, true, &e) == MC_INVENTORY_OK &&
          mc_inventory_chest_retain(a, &e) == MC_INVENTORY_BUSY &&
          mc_inventory_chest_size(raw, &value, &e) == MC_INVENTORY_BUSY &&
          mc_inventory_chest_end_calls(raw, &e) == MC_INVENTORY_OK);
    CHECK(mc_inventory_chest_retire(&a, &e) == MC_INVENTORY_OK && a == NULL && left.releases == 0);
    CHECK(mc_inventory_chest_retire(&raw, &e) == MC_INVENTORY_OK && left.releases == 0);
    CHECK(mc_inventory_chest_retire(&pair, &e) == MC_INVENTORY_OK && left.releases == 1);
    CHECK(mc_inventory_chest_retire(&b, &e) == MC_INVENTORY_OK && right.releases == 1);
    ok = true;
cleanup:
    mc_inventory_chest_release(raw);
    mc_inventory_chest_release(pair);
    mc_inventory_chest_release(a);
    mc_inventory_chest_release(b);
    clear(&left);
    clear(&right);
    return ok;
}

typedef struct host {
    mc_inventory_player *inventory;
    unsigned drops;
} host;

static mc_inventory_status player_inventory(void *context, mc_entity *player,
                                            mc_inventory_player **out,
                                            mc_container_interaction_error *e) {
    (void)player;
    (void)e;
    *out = ((host *)context)->inventory;
    return MC_INVENTORY_OK;
}

static mc_inventory_status drop(void *context, mc_entity *player, mc_item_stack *value,
                                mc_container_interaction_error *e) {
    (void)player;
    (void)value;
    (void)e;
    ++((host *)context)->drops;
    return MC_INVENTORY_OK;
}

static bool container_controls(mc_registry_classes *registry) {
    bool ok = false;
    leaf storage = {.size = 27, .limit = 64, .usable = true};
    mc_inventory_chest *chest = NULL;
    mc_inventory_player *inventory = NULL;
    mc_container *cd = NULL;
    mc_container_interaction_call *call = NULL;
    mc_inventory_error e = {0};
    mc_container_interaction_error ie;
    mc_item_stack *original = NULL, *returned = NULL, *destination = NULL;
    CHECK(inv_chest_leaf_create(MC_REGISTRY_CLIENT, &storage, NULL, &leaves, NULL, &chest, &e) ==
          MC_INVENTORY_OK);
    CHECK(mc_inventory_player_create(MC_REGISTRY_CLIENT, NULL, NULL, &inventory, &e) ==
          MC_INVENTORY_OK);
    host h = {inventory, 0};
    const mc_container_interaction_host api = {
        .context = &h, .inventory = player_inventory, .drop = drop};
    const int32_t sizes[] = {27, 54, 28, 0, -9, INT_MIN};
    for (size_t test = 0; test < sizeof sizes / sizeof *sizes; ++test) {
        storage.size = sizes[test];
        CHECK(mc_container_chest_allocate(MC_REGISTRY_CLIENT, registry, NULL, &cd, &e) ==
              MC_INVENTORY_OK);
        CHECK(mc_container_chest_construct(cd, inventory, chest, &e) == MC_INVENTORY_OK);
        mc_container_state state;
        CHECK(mc_container_read_state(cd, &state) && state.constructor_pc == 217 &&
              state.chest_rows == sizes[test] / 9 && state.crafting == NULL &&
              state.result == NULL);
        const int32_t rows = sizes[test] / 9;
        const size_t storage_slots = rows > 0 ? (size_t)rows * 9u : 0;
        CHECK(state.slots == storage_slots + 36 && state.snapshots == state.slots);
        for (size_t i = 0; i < state.slots; ++i) {
            mc_container_slot_state slot;
            CHECK(mc_container_slot_read(cd, (int32_t)i, &slot, &e) == MC_INVENTORY_OK);
            const size_t relative = i < storage_slots ? i : i - storage_slots;
            CHECK(slot.x == 8 + (int32_t)(relative % 9u) * 18);
            const int32_t offset = mc_java_i32_mul(mc_java_i32_sub(rows, 4), 18);
            CHECK(slot.y ==
                  (i < storage_slots
                       ? 18 + (int32_t)(i / 9u) * 18
                       : mc_java_i32_add(relative < 27 ? 103 + (int32_t)(relative / 9u) * 18 : 161,
                                         offset)));
        }
        CHECK(mc_container_retire(&cd, &e) == MC_INVENTORY_OK);
    }
    storage.size = 27;
    CHECK(mc_container_chest_create(MC_REGISTRY_CLIENT, registry, inventory, chest, NULL, &cd,
                                    &e) == MC_INVENTORY_OK);
    bool eligible = false;
    CHECK(mc_container_chest_usable(cd, NULL, &eligible, &e) == MC_INVENTORY_OK && eligible);
    original = stack(10);
    destination = stack(60);
    CHECK(original != NULL && destination != NULL &&
          leaf_set(&storage, 0, original, &e) == MC_INVENTORY_OK &&
          mc_inventory_player_source_set(inventory, 8, destination, &e) == MC_INVENTORY_OK);
    const mc_inventory_status clicked =
        mc_container_click_begin(cd, NULL, 0, 0, true, &api, &call, &ie);
    if (clicked != MC_INVENTORY_OK)
        fprintf(stderr, "Chest Shift status=%d operation=%s dependency=%s.%s:%d\n", clicked,
                ie.operation == NULL ? "?" : ie.operation,
                ie.dependency.class_name == NULL ? "?" : ie.dependency.class_name,
                ie.dependency.method == NULL ? "?" : ie.dependency.method, ie.dependency.pc);
    CHECK(clicked == MC_INVENTORY_OK);
    CHECK(mc_container_interaction_take_result(call, &returned) == MC_INVENTORY_OK &&
          returned != original && count_is(returned, 10) && storage.slots[0] == NULL &&
          count_is(inventory->main_slots[8], 64) && count_is(inventory->main_slots[7], 6));
    CHECK(mc_container_retire(&cd, &e) == MC_INVENTORY_BUSY);
    CHECK(mc_container_interaction_retire(&call, &ie) == MC_INVENTORY_OK);
    mc_item_stack_release(returned);
    returned = NULL;
    /* Original CD returns a clone even when all destinations are full. */
    for (int32_t i = 0; i < 36; ++i)
        CHECK(mc_inventory_player_source_set(inventory, i, destination, &e) == MC_INVENTORY_OK);
    CHECK(leaf_set(&storage, 0, original, &e) == MC_INVENTORY_OK);
    CHECK(mc_item_stack_source_count_set(original, 10) == MC_ITEM_STACK_OK);
    const unsigned changed_before = storage.changes;
    CHECK(mc_container_click_begin(cd, NULL, 0, 0, true, &api, &call, &ie) == MC_INVENTORY_OK);
    CHECK(mc_container_interaction_take_result(call, &returned) == MC_INVENTORY_OK &&
          count_is(returned, 10) && storage.slots[0] == original && count_is(original, 10) &&
          storage.changes > changed_before);
    CHECK(mc_container_interaction_retire(&call, &ie) == MC_INVENTORY_OK);
    mc_item_stack_release(returned);
    returned = NULL;
    CHECK(mc_container_close_begin(cd, NULL, &api, &call, &ie) == MC_INVENTORY_OK &&
          storage.slots[0] == original && h.drops == 0);
    CHECK(mc_container_interaction_retire(&call, &ie) == MC_INVENTORY_OK);
    CHECK(mc_container_retire(&cd, &e) == MC_INVENTORY_OK);
    CHECK(mc_container_chest_allocate(MC_REGISTRY_CLIENT, registry, NULL, &cd, &e) ==
              MC_INVENTORY_OK &&
          mc_container_chest_construct(cd, inventory, NULL, &e) == MC_INVENTORY_NULL_REFERENCE &&
          cd->constructor_failed && cd->constructor_pc == 11 && cd->slots->size == 0);
    CHECK(mc_container_retire(&cd, &e) == MC_INVENTORY_OK);
    CHECK(mc_container_chest_create(MC_REGISTRY_CLIENT, registry, NULL, chest, NULL, &cd, &e) ==
          MC_INVENTORY_OK);
    CHECK(mc_container_retire(&cd, &e) == MC_INVENTORY_OK);
    const mc_inventory_limits tight = {256, 64};
    CHECK(mc_container_chest_allocate(MC_REGISTRY_CLIENT, registry, &tight, &cd, &e) ==
          MC_INVENTORY_OK);
    CHECK(mc_container_chest_construct(cd, inventory, chest, &e) == MC_INVENTORY_LIMIT &&
          cd != NULL && cd->constructor_failed && !cd->constructed && cd->constructor_pc == 1);
    CHECK(mc_container_retire(&cd, &e) == MC_INVENTORY_OK);
    ok = true;
cleanup:
    (void)mc_container_interaction_retire(&call, NULL);
    (void)mc_container_retire(&cd, NULL);
    (void)mc_inventory_player_retire(&inventory, NULL);
    mc_inventory_chest_release(chest);
    mc_item_stack_release(original);
    mc_item_stack_release(destination);
    mc_item_stack_release(returned);
    clear(&storage);
    return ok;
}

typedef struct aliases {
    const char *names[8];
    const mc_item_stack *values[8];
    size_t count;
} aliases;

static const mc_item_stack *lookup(const aliases *a, const char *name) {
    if (name != NULL)
        for (size_t i = 0; i < a->count; ++i)
            if (strcmp(name, a->names[i]) == 0)
                return a->values[i];
    return NULL;
}

static bool observed(aliases *a, const mc_chest_stack_reference *expected,
                     const mc_item_stack *actual) {
    if (expected->identity == NULL)
        return actual == NULL;
    mc_item_stack_fields f;
    if (actual == NULL || mc_item_stack_fields_get(actual, &f) != MC_ITEM_STACK_OK ||
        f.id != expected->id || f.count != expected->count || f.damage != expected->damage ||
        f.animation != expected->animation)
        return false;
    const mc_item_stack *known = lookup(a, expected->identity);
    if (known != NULL)
        return known == actual;
    for (size_t i = 0; i < a->count; ++i)
        if (a->values[i] == actual)
            return false; /* Different original labels must remain different. */
    if (a->count == sizeof a->names / sizeof *a->names)
        return false;
    a->names[a->count] = expected->identity;
    a->values[a->count++] = actual;
    return true;
}

static bool reference_shift(mc_registry_classes *registry, const mc_chest_shift_reference *ref) {
    bool ok = false;
    leaf storage = {.size = ref->storage_size, .limit = 64, .usable = true};
    mc_inventory_chest *chest = NULL;
    mc_inventory_player *inventory = NULL;
    mc_container *cd = NULL;
    mc_container_interaction_call *call = NULL;
    mc_item_stack *inputs[2] = {0};
    mc_inventory_error e = {0};
    mc_container_interaction_error ie;
    aliases identities = {0};
    CHECK(inv_chest_leaf_create(MC_REGISTRY_CLIENT, &storage, NULL, &leaves, NULL, &chest, &e) ==
          MC_INVENTORY_OK);
    CHECK(mc_inventory_player_create(MC_REGISTRY_CLIENT, NULL, NULL, &inventory, &e) ==
          MC_INVENTORY_OK);
    for (size_t i = 0; i < ref->input_stack_count; ++i) {
        const mc_chest_stack_reference *s = &ref->input_stacks[i];
        CHECK(mc_item_stack_allocate(&inputs[i]) == MC_ITEM_STACK_OK &&
              mc_item_stack_construct(inputs[i], s->id, s->count, s->damage) == MC_ITEM_STACK_OK &&
              mc_item_stack_source_animation_set(inputs[i], s->animation) == MC_ITEM_STACK_OK &&
              observed(&identities, s, inputs[i]));
    }
    for (int32_t i = 0; i < 36; ++i)
        CHECK(mc_inventory_player_source_set(
                  inventory, i, (mc_item_stack *)lookup(&identities, ref->before_main[i].identity),
                  &e) == MC_INVENTORY_OK);
    for (int32_t i = 0; i < ref->storage_size; ++i)
        CHECK(leaf_set(&storage, i,
                       (mc_item_stack *)lookup(&identities, ref->before_storage[i].identity),
                       &e) == MC_INVENTORY_OK);
    CHECK(mc_container_chest_create(MC_REGISTRY_CLIENT, registry, inventory, chest, NULL, &cd,
                                    &e) == MC_INVENTORY_OK);
    host h = {inventory, 0};
    const mc_container_interaction_host api = {
        .context = &h, .inventory = player_inventory, .drop = drop};
    CHECK(mc_container_click_begin(cd, NULL, ref->source_slot, 0, true, &api, &call, &ie) ==
          MC_INVENTORY_OK);
    mc_container_interaction_state state;
    CHECK(mc_container_interaction_inspect(call, &state) && state.returned &&
          observed(&identities, &ref->result, state.result));
    for (size_t i = 0; i < 36; ++i)
        CHECK(observed(&identities, &ref->after_main[i], inventory->main_slots[i]));
    for (int32_t i = 0; i < ref->storage_size; ++i)
        CHECK(observed(&identities, &ref->after_storage[i], storage.slots[i]));
    for (size_t i = 0; i < ref->tracked_stack_count; ++i) {
        const mc_chest_stack_reference *s = &ref->tracked_stacks[i];
        CHECK(observed(&identities, s, lookup(&identities, s->identity)));
    }
    ok = true;
cleanup:
    if (!ok)
        fprintf(stderr, "Original Chest Shift profile failed: %s\n", ref->profile);
    (void)mc_container_interaction_retire(&call, NULL);
    (void)mc_container_retire(&cd, NULL);
    (void)mc_inventory_player_retire(&inventory, NULL);
    mc_inventory_chest_release(chest);
    for (size_t i = 0; i < 2; ++i)
        mc_item_stack_release(inputs[i]);
    clear(&storage);
    return ok;
}

static bool reference_layouts(mc_registry_classes *registry) {
    bool ok = false;
    leaf storage = {.limit = 64, .usable = true};
    mc_inventory_chest *chest = NULL;
    mc_inventory_player *inventory = NULL;
    mc_container *cd = NULL;
    mc_inventory_error e = {0};
    CHECK(inv_chest_leaf_create(MC_REGISTRY_CLIENT, &storage, NULL, &leaves, NULL, &chest, &e) ==
          MC_INVENTORY_OK);
    CHECK(mc_inventory_player_create(MC_REGISTRY_CLIENT, NULL, NULL, &inventory, &e) ==
          MC_INVENTORY_OK);
    for (size_t p = 0; p < sizeof mc_chest_layout_references / sizeof *mc_chest_layout_references;
         ++p) {
        const mc_chest_layout_reference *r = &mc_chest_layout_references[p];
        storage.size = r->size;
        CHECK(mc_container_chest_create(MC_REGISTRY_CLIENT, registry, inventory, chest, NULL, &cd,
                                        &e) == MC_INVENTORY_OK);
        mc_container_state state;
        CHECK(mc_container_read_state(cd, &state) && state.chest_rows == r->rows &&
              state.slots == r->slot_count);
        for (size_t i = 0; i < r->slot_count; ++i) {
            mc_container_slot_state s;
            CHECK(mc_container_slot_read(cd, (int32_t)i, &s, &e) == MC_INVENTORY_OK &&
                  s.container_index == r->slots[i][0] &&
                  s.inventory_kind ==
                      (r->slots[i][1] == 1 ? MC_SLOT_INVENTORY_PLAYER : MC_SLOT_INVENTORY_CHEST) &&
                  s.inventory_index == r->slots[i][2] && s.x == r->slots[i][3] &&
                  s.y == r->slots[i][4]);
        }
        CHECK(mc_container_retire(&cd, &e) == MC_INVENTORY_OK);
    }
    for (size_t i = 0; i < sizeof mc_chest_shift_references / sizeof *mc_chest_shift_references;
         ++i)
        CHECK(reference_shift(registry, &mc_chest_shift_references[i]));
    printf("Original Chest projection: %zu layouts, %zu Shift profiles\n",
           sizeof mc_chest_layout_references / sizeof *mc_chest_layout_references,
           sizeof mc_chest_shift_references / sizeof *mc_chest_shift_references);
    ok = true;
cleanup:
    (void)mc_container_retire(&cd, NULL);
    (void)mc_inventory_player_retire(&inventory, NULL);
    mc_inventory_chest_release(chest);
    clear(&storage);
    return ok;
}

static bool tile_controls(mc_registry_classes *registry, mc_java_random_profile *profile) {
    bool ok = false;
    mc_world_runtime_classes *world = NULL;
    mc_tile_entity *tile = NULL;
    mc_inventory_chest *chest = NULL;
    mc_item_stack *value = NULL;
    mc_item_stack *removed = NULL;
    mc_inventory_error e = {0};
    static mc_math_trig trig;
    CHECK(mc_math_trig_init(&trig));
    CHECK(mc_world_runtime_classes_create(MC_WORLD_INFO_CLIENT, profile, &trig, &world, NULL) ==
          MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_classes_bind_registry(world, registry, NULL) == MC_REGISTRY_OK);
    CHECK(mc_tile_entity_allocate_in_classes(world, MC_TILE_ENTITY_CHEST, &tile) ==
              MC_TILE_ENTITY_OK &&
          mc_tile_entity_construct(tile) == MC_TILE_ENTITY_OK);
    CHECK(mc_inventory_chest_from_tile(MC_REGISTRY_CLIENT, tile, NULL, &chest, &e) ==
          MC_INVENTORY_OK);
    int32_t size;
    CHECK(mc_inventory_chest_size(chest, &size, &e) == MC_INVENTORY_OK && size == 27);
    const mc_item_stack *borrowed = NULL;
    CHECK(mc_inventory_chest_borrow(chest, 35, &borrowed, &e) == MC_INVENTORY_OK &&
          borrowed == NULL);
    CHECK(mc_inventory_chest_borrow(chest, 36, &borrowed, &e) == MC_INVENTORY_INDEX_OUT_OF_BOUNDS);
    value = stack(90);
    CHECK(value != NULL && mc_inventory_chest_set(chest, 0, value, &e) == MC_INVENTORY_OK &&
          e.chest == NULL && count_is(value, 64)); /* OW.y_ skips null World. */
    CHECK(mc_inventory_chest_set(chest, 36, NULL, &e) == MC_INVENTORY_INDEX_OUT_OF_BOUNDS &&
          e.chest != NULL && e.source_exception);
    const mc_live_tile_entity_error *failure = e.chest;
    CHECK(mc_inventory_chest_borrow(chest, 0, &borrowed, &e) == MC_INVENTORY_OK &&
          borrowed == value);
    CHECK(failure->status !=
          MC_WORLD_STORAGE_OK); /* Prior retained receipt survives a later call. */
    CHECK(mc_inventory_chest_end_calls(chest, &e) == MC_INVENTORY_OK);
    CHECK(mc_inventory_chest_split(chest, 0, -1, &removed, &e) == MC_INVENTORY_OK &&
          removed != value && count_is(removed, -1) && count_is(value, 65));
    mc_item_stack_release(removed);
    removed = NULL;
    CHECK(mc_inventory_chest_split(chest, 0, 100, &removed, &e) == MC_INVENTORY_OK &&
          removed == value && count_is(removed, 65));
    mc_item_stack_release(removed);
    removed = NULL;
    CHECK(mc_inventory_chest_borrow(chest, 0, &borrowed, &e) == MC_INVENTORY_OK &&
          borrowed == NULL);
    bool usable = true;
    CHECK(mc_inventory_chest_usable(chest, NULL, &usable, &e) == MC_INVENTORY_NULL_REFERENCE &&
          usable);
    CHECK(mc_inventory_chest_retire(&chest, &e) == MC_INVENTORY_OK);
    mc_tile_entity_release(tile);
    tile = NULL;
    CHECK(mc_world_runtime_classes_free(world, NULL) == MC_WORLD_STORAGE_OK);
    world = NULL;
    ok = true;
cleanup:
    mc_inventory_chest_release(chest);
    mc_tile_entity_release(tile);
    mc_item_stack_release(value);
    mc_item_stack_release(removed);
    if (world != NULL)
        (void)mc_world_runtime_classes_free(world, NULL);
    return ok;
}

int main(int argc, char **argv) {
    int result = 1;
    mc_resource_files *files = NULL;
    mc_materials *materials = NULL;
    mc_java_random_profile *profile = NULL;
    mc_registry_classes *registry = NULL;
    if (argc != 2 || mc_resource_files_create(argv[1], &files) != MC_REGISTRY_OK ||
        !mc_materials_create(&materials) ||
        mc_java_random_profile_create(SIZE_MAX, &profile) != MC_JAVA_RANDOM_PROFILE_OK)
        goto cleanup;
    const mc_registry_options options = {MC_REGISTRY_CLIENT, profile, materials, 0,
                                         mc_resource_files_source(files)};
    if (mc_registry_classes_allocate(&options, &registry, NULL) != MC_REGISTRY_OK ||
        mc_registry_classes_initialize_statistics(registry, NULL) != MC_REGISTRY_OK ||
        !protocol() || !container_controls(registry) || !reference_layouts(registry) ||
        !tile_controls(registry, profile))
        goto cleanup;
    printf("Chest backend controls passed: %u\n", checks);
    result = 0;
cleanup:
    if (mc_registry_classes_free(registry, NULL) != MC_REGISTRY_OK)
        result = 1;
    if (files != NULL && mc_resource_files_free(files) != MC_REGISTRY_OK)
        result = 1;
    mc_materials_destroy(materials);
    mc_java_random_profile_free(profile);
    return result;
}
