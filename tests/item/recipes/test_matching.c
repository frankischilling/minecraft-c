#include "mc/item/recipes/matching.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition);                        \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

static mc_item_stack *stack(int32_t id, int32_t count, int32_t damage) {
    mc_item_stack *value = NULL;
    if (mc_item_stack_allocate(&value) != MC_ITEM_STACK_OK)
        return NULL;
    if (mc_item_stack_construct(value, id, count, damage) != MC_ITEM_STACK_OK) {
        mc_item_stack_release(value);
        return NULL;
    }
    return value;
}

static bool same_fields(const mc_item_stack *value, int32_t id, int32_t count, int32_t damage) {
    mc_item_stack_fields fields;
    return mc_item_stack_fields_get(value, &fields) == MC_ITEM_STACK_OK && fields.id == id &&
           fields.count == count && fields.damage == damage && fields.animation == 0;
}

static const mc_recipe *recipe(const mc_crafting_manager *manager, mc_recipe_kind kind, int32_t id,
                               int32_t count, int32_t damage) {
    for (size_t i = 0; i < mc_crafting_manager_size(manager); ++i) {
        const mc_recipe *current = NULL;
        if (mc_crafting_manager_get(manager, i, &current) != MC_REGISTRY_OK)
            return NULL;
        if (mc_recipe_kind_get(current) == kind &&
            same_fields(mc_recipe_output(current), id, count, damage))
            return current;
    }
    return NULL;
}

/* NULL handler is an actual source constructor input, not a container stub.
 * The real inventory setter commits its slot then fails its notification; the
 * matching tests retain and verify that source prefix. Container integration
 * with a nonnull actual handler is covered by the separate container tests. */
static bool put(mc_inventory_crafting *grid, int32_t index, mc_item_stack *value) {
    mc_inventory_error error;
    return mc_inventory_crafting_set(grid, index, value, &error) == MC_INVENTORY_NULL_REFERENCE &&
           error.mutation_reached;
}

static bool clear(mc_inventory_crafting *grid) {
    int32_t size = 0;
    if (mc_inventory_crafting_size(grid, &size, NULL) != MC_INVENTORY_OK)
        return false;
    for (int32_t i = 0; i < size; ++i)
        if (!put(grid, i, NULL))
            return false;
    return true;
}

static bool put_ingredient(mc_inventory_crafting *grid, int32_t index,
                           const mc_item_stack *ingredient, size_t salt) {
    if (ingredient == NULL)
        return true;
    mc_item_stack_fields original;
    if (mc_item_stack_fields_get(ingredient, &original) != MC_ITEM_STACK_OK)
        return false;
    mc_item_stack *value = stack(original.id, (salt & 1u) == 0 ? 0 : -3,
                                 original.damage == -1 ? (int32_t)(salt % 16u) : original.damage);
    if (value == NULL)
        return false;
    const bool stored = put(grid, index, value);
    mc_item_stack_release(value);
    return stored;
}

static bool catalog_inputs(const mc_recipe *entry, mc_inventory_crafting *grid, unsigned variant) {
    if (!clear(grid))
        return false;
    if (mc_recipe_kind_get(entry) == MC_RECIPE_SHAPED) {
        const mc_shaped_recipe_fields *shape = mc_recipe_shaped_fields_get(entry);
        if (shape == NULL || shape->b < 1 || shape->b > 3 || shape->c < 1 || shape->c > 3 ||
            (size_t)(shape->b * shape->c) != mc_recipe_shaped_array_length(entry))
            return false;
        const int32_t offset_x = variant == 2u ? 3 - shape->b : 0;
        const int32_t offset_y = variant == 2u ? 3 - shape->c : 0;
        for (int32_t y = 0; y < shape->c; ++y)
            for (int32_t x = 0; x < shape->b; ++x) {
                const int32_t source_x = variant == 1u ? shape->b - x - 1 : x;
                const size_t source = (size_t)(source_x + y * shape->b);
                if (!put_ingredient(grid, x + offset_x + (y + offset_y) * 3, shape->d[source],
                                    source + variant))
                    return false;
            }
        return true;
    }
    if (mc_recipe_kind_get(entry) == MC_RECIPE_SHAPELESS) {
        const mc_shapeless_recipe_fields *shape = mc_recipe_shapeless_fields_get(entry);
        const size_t count = shape == NULL ? 0 : mc_recipe_ingredient_list_size(shape->b);
        if (count == 0 || count > 9u)
            return false;
        for (size_t i = 0; i < count; ++i) {
            const mc_item_stack *ingredient = NULL;
            if (mc_recipe_ingredient_list_get(shape->b, i, &ingredient) != MC_REGISTRY_OK ||
                ingredient == NULL)
                return false;
            const int32_t slot = variant == 0u   ? (int32_t)i
                                 : variant == 1u ? 8 - (int32_t)i
                                                 : (int32_t)((i * 2u + 1u) % 9u);
            if (!put_ingredient(grid, slot, ingredient, i + variant))
                return false;
        }
        return true;
    }
    return false;
}

static bool registered_catalog(mc_registry_side side, const mc_crafting_manager *manager,
                               mc_inventory_crafting *grid) {
    bool ok = false;
    mc_item_stack *output = NULL, *other = NULL, *intruder = NULL;
    mc_crafting_error error;
    intruder = stack(32767, 1, 0);
    CHECK(intruder != NULL);
    for (size_t index = 0; index < 151u; ++index) {
        const mc_recipe *entry = NULL;
        CHECK(mc_crafting_manager_get(manager, index, &entry) == MC_REGISTRY_OK && entry != NULL);
        mc_item_stack_fields expected;
        CHECK(mc_item_stack_fields_get(mc_recipe_output(entry), &expected) == MC_ITEM_STACK_OK);
        for (unsigned variant = 0; variant < 3u; ++variant) {
            CHECK(catalog_inputs(entry, grid, variant));
            mc_item_stack_fields before[9] = {0};
            bool present[9] = {0};
            for (int32_t slot = 0; slot < 9; ++slot) {
                const mc_item_stack *value = NULL;
                CHECK(mc_inventory_crafting_borrow_slot(grid, slot, &value, NULL) ==
                      MC_INVENTORY_OK);
                present[slot] = value != NULL;
                if (value != NULL)
                    CHECK(mc_item_stack_fields_get(value, &before[slot]) == MC_ITEM_STACK_OK);
            }
            bool matched = false;
            CHECK(mc_recipe_matches(side, entry, grid, &matched, &error) == MC_CRAFTING_OK &&
                  matched);
            for (int32_t slot = 0; slot < 9; ++slot) {
                const mc_item_stack *value = NULL;
                CHECK(mc_inventory_crafting_borrow_slot(grid, slot, &value, NULL) ==
                      MC_INVENTORY_OK);
                CHECK((value != NULL) == present[slot]);
                if (value != NULL)
                    CHECK(same_fields(value, before[slot].id, before[slot].count,
                                      before[slot].damage));
            }
            CHECK(mc_recipe_craft_result(side, entry, grid, &output, &error) == MC_CRAFTING_OK);
            CHECK(mc_recipe_craft_result(side, entry, NULL, &other, &error) == MC_CRAFTING_OK);
            CHECK(output != NULL && other != NULL && output != other &&
                  output != mc_recipe_output(entry) && other != mc_recipe_output(entry) &&
                  same_fields(output, expected.id, expected.count, expected.damage) &&
                  same_fields(other, expected.id, expected.count, expected.damage) &&
                  mc_item_stack_source(output) == NULL && mc_item_stack_source(other) == NULL);
            mc_item_stack_release(output);
            output = NULL;
            mc_item_stack_release(other);
            other = NULL;
            int32_t extra = 0;
            while (extra < 9 && present[extra])
                ++extra;
            CHECK(put(grid, extra < 9 ? extra : 0, intruder));
            CHECK(mc_recipe_matches(side, entry, grid, &matched, &error) == MC_CRAFTING_OK &&
                  !matched);
        }
    }
    ok = true;
cleanup:
    mc_item_stack_release(output);
    mc_item_stack_release(other);
    mc_item_stack_release(intruder);
    return ok;
}

/* Called by the isolated integration main after constructing the real complete
 * registry. No expected recipe catalog, alternate manager or slot callback is
 * supplied here. Both registry sides use this same authored input suite. */
bool mc_test_registered_matching(mc_registry_side side, const mc_crafting_manager *manager) {
    bool ok = false;
    mc_inventory_crafting *small = NULL, *large = NULL;
    mc_item_stack *log = NULL, *plank = NULL, *stick = NULL;
    mc_item_stack *black = NULL, *white = NULL, *wrong = NULL;
    mc_item_stack *output = NULL, *other = NULL;
    mc_crafting_error error;
    bool matched = false;
    CHECK(manager != NULL && mc_crafting_manager_size(manager) == 151u);
    CHECK(mc_inventory_crafting_create(side, NULL, 2, 2, NULL, &small, NULL) == MC_INVENTORY_OK);
    CHECK(mc_inventory_crafting_create(side, NULL, 3, 3, NULL, &large, NULL) == MC_INVENTORY_OK);
    CHECK(registered_catalog(side, manager, large) && clear(large));
    CHECK(mc_crafting_manager_find_matching(side, manager, small, &output, &error) ==
              MC_CRAFTING_OK &&
          output == NULL);
    CHECK(mc_crafting_manager_find_matching(side, manager, large, &output, &error) ==
              MC_CRAFTING_OK &&
          output == NULL);
    log = stack(17, 0, 2);
    plank = stack(5, 1, 0);
    stick = stack(280, 1, 0);
    black = stack(351, 1, 0);
    white = stack(351, -9, 15);
    wrong = stack(351, 1, 14);
    CHECK(log != NULL && plank != NULL && stick != NULL && black != NULL && white != NULL &&
          wrong != NULL);
    const mc_recipe *planks = recipe(manager, MC_RECIPE_SHAPED, 5, 4, 0);
    const mc_recipe *sticks = recipe(manager, MC_RECIPE_SHAPED, 280, 4, 0);
    const mc_recipe *hoe = recipe(manager, MC_RECIPE_SHAPED, 290, 1, 0);
    const mc_recipe *gray = recipe(manager, MC_RECIPE_SHAPELESS, 351, 3, 7);
    CHECK(planks != NULL && sticks != NULL && hoe != NULL && gray != NULL);

    /* A zero-count log with arbitrary metadata still matches its wildcard;
     * translated1x1 matching uses the real2x2 virtual-empty accessors. */
    CHECK(put(small, 3, log));
    CHECK(mc_recipe_matches(side, planks, small, &matched, &error) == MC_CRAFTING_OK && matched);
    CHECK(mc_crafting_manager_find_matching(side, manager, small, &output, &error) ==
          MC_CRAFTING_OK);
    CHECK(output != NULL && output != mc_recipe_output(planks) && same_fields(output, 5, 4, 0) &&
          mc_item_stack_source(output) == NULL && same_fields(log, 17, 0, 2));
    CHECK(mc_recipe_craft_result(side, planks, NULL, &other, &error) == MC_CRAFTING_OK);
    CHECK(other != NULL && other != output && other != mc_recipe_output(planks));
    CHECK(mc_item_stack_count_set(output, 99) == MC_ITEM_STACK_OK && same_fields(other, 5, 4, 0) &&
          same_fields(mc_recipe_output(planks), 5, 4, 0));
    mc_item_stack_release(output);
    output = NULL;
    mc_item_stack_release(other);
    other = NULL;

    CHECK(clear(small) && put(small, 1, plank) && put(small, 3, plank));
    CHECK(mc_recipe_matches(side, sticks, small, &matched, &error) == MC_CRAFTING_OK && matched);
    CHECK(mc_crafting_manager_find_matching(side, manager, small, &output, &error) ==
              MC_CRAFTING_OK &&
          same_fields(output, 280, 4, 0));
    mc_item_stack_release(output);
    output = NULL;
    CHECK(put(small, 0, plank));
    CHECK(mc_recipe_matches(side, sticks, small, &matched, &error) == MC_CRAFTING_OK && !matched);
    CHECK(clear(small) && put(small, 0, plank) && put(small, 1, plank));
    CHECK(mc_recipe_matches(side, sticks, small, &matched, &error) == MC_CRAFTING_OK && !matched);

    /* Actual wooden hoe definition: XX / space# / space#. Both horizontal
     * reflections and a right translation fit3x3; the2x2 grid cannot fit it. */
    CHECK(put(large, 0, plank) && put(large, 1, plank) && put(large, 4, stick) &&
          put(large, 7, stick));
    CHECK(mc_recipe_matches(side, hoe, large, &matched, &error) == MC_CRAFTING_OK && matched);
    CHECK(clear(large) && put(large, 0, plank) && put(large, 1, plank) && put(large, 3, stick) &&
          put(large, 6, stick));
    CHECK(mc_recipe_matches(side, hoe, large, &matched, &error) == MC_CRAFTING_OK && matched);
    CHECK(clear(large) && put(large, 1, plank) && put(large, 2, plank) && put(large, 5, stick) &&
          put(large, 8, stick));
    CHECK(mc_recipe_matches(side, hoe, large, &matched, &error) == MC_CRAFTING_OK && matched);
    CHECK(mc_recipe_matches(side, hoe, small, &matched, &error) == MC_CRAFTING_OK && !matched);
    CHECK(put(large, 0, log));
    CHECK(mc_recipe_matches(side, hoe, large, &matched, &error) == MC_CRAFTING_OK && !matched);

    /* Three-slot gray dye uses two distinct ingredient entries for white.
     * Aliased input identity and negative count remain three visited slots. */
    CHECK(clear(large) && put(large, 0, white) && put(large, 4, black) && put(large, 8, white));
    CHECK(mc_recipe_matches(side, gray, large, &matched, &error) == MC_CRAFTING_OK && matched);
    CHECK(mc_crafting_manager_find_matching(side, manager, large, &output, &error) ==
              MC_CRAFTING_OK &&
          same_fields(output, 351, 3, 7) && same_fields(white, 351, -9, 15));
    CHECK(mc_recipe_craft_result(side, gray, NULL, &other, &error) == MC_CRAFTING_OK);
    CHECK(other != NULL && other != output && other != mc_recipe_output(gray) &&
          same_fields(other, 351, 3, 7));
    CHECK(put(large, 8, NULL));
    CHECK(mc_recipe_matches(side, gray, large, &matched, &error) == MC_CRAFTING_OK && !matched);
    CHECK(put(large, 8, wrong));
    CHECK(mc_recipe_matches(side, gray, large, &matched, &error) == MC_CRAFTING_OK && !matched);

    /* No input mutation or result publication occurs when a descendant grid
     * read fails. The actual caller and shaped-helper frames are retained. */
    mc_item_stack *sentinel = output;
    CHECK(mc_crafting_manager_find_matching(side, manager, NULL, &output, &error) ==
              MC_CRAFTING_NULL_REFERENCE &&
          output == sentinel && error.frame_count >= 3u);
    CHECK(strcmp(error.frames[0].class_name, side == MC_REGISTRY_CLIENT ? "hk" : "ey") == 0 &&
          error.frames[0].pc == 31 && error.frames[1].pc == 29 && error.frames[2].pc == 115);
    CHECK(mc_inventory_crafting_retire(&large, NULL) == MC_INVENTORY_OK);
    CHECK(same_fields(output, 351, 3, 7) && same_fields(other, 351, 3, 7));
    ok = true;
cleanup:
    mc_inventory_crafting_retire(&small, NULL);
    mc_inventory_crafting_retire(&large, NULL);
    mc_item_stack_release(log);
    mc_item_stack_release(plank);
    mc_item_stack_release(stick);
    mc_item_stack_release(black);
    mc_item_stack_release(white);
    mc_item_stack_release(wrong);
    mc_item_stack_release(output);
    mc_item_stack_release(other);
    return ok;
}
