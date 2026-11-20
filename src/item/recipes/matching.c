#include "mc/item/recipes/matching.h"

#include "mc/core/java/java.h"

#include <stdlib.h>
#include <string.h>

typedef struct matching_work {
    size_t steps;
} matching_work;

static bool valid_side(mc_registry_side side) {
    return side == MC_REGISTRY_CLIENT || side == MC_REGISTRY_SERVER;
}

static mc_inventory_site site(mc_registry_side side, const char *client, const char *server,
                              const char *method, const char *client_descriptor,
                              const char *server_descriptor, int32_t pc) {
    return (mc_inventory_site){side == MC_REGISTRY_CLIENT ? client : server, method,
                               side == MC_REGISTRY_CLIENT ? client_descriptor : server_descriptor,
                               pc};
}

static mc_crafting_status fail(mc_crafting_error *error, mc_crafting_status status,
                               const char *operation, mc_inventory_site frame) {
    *error = (mc_crafting_error){
        .status = status, .operation = operation, .matching_status = (int32_t)status};
    if (frame.class_name != NULL) {
        error->frames[0] = frame;
        error->frame_count = 1;
    }
    return status;
}

static mc_crafting_status caller(mc_crafting_error *error, mc_inventory_site frame) {
    if (error->frame_count == MC_INVENTORY_ERROR_FRAMES) {
        error->status = MC_CRAFTING_LIMIT;
        error->operation = "Crafting source frame bound";
    } else {
        memmove(error->frames + 1, error->frames, error->frame_count * sizeof(*error->frames));
        error->frames[0] = frame;
        ++error->frame_count;
    }
    error->matching_status = (int32_t)error->status;
    return error->status;
}

static mc_crafting_status finish(mc_crafting_error *out, const mc_crafting_error *error,
                                 mc_crafting_status status) {
    if (out != NULL)
        *out = status == MC_CRAFTING_OK ? (mc_crafting_error){0} : *error;
    return status;
}

static mc_crafting_status stack_failure(mc_crafting_error *error, mc_item_stack_status status,
                                        const char *operation, mc_inventory_site frame) {
    const mc_crafting_status result = status == MC_ITEM_STACK_ALLOCATION_FAILED
                                          ? MC_CRAFTING_ALLOCATION_FAILED
                                      : status == MC_ITEM_STACK_LIMIT ? MC_CRAFTING_LIMIT
                                                                      : MC_CRAFTING_STACK_FAILED;
    fail(error, result, operation, frame);
    error->stack_status = status;
    error->stack.status = status;
    return result;
}

static mc_crafting_status step(matching_work *work, mc_crafting_error *error,
                               mc_inventory_site frame) {
    if (work->steps == MC_CRAFTING_MAX_WORK_STEPS)
        return fail(error, MC_CRAFTING_LIMIT, "Crafting native work bound", frame);
    ++work->steps;
    return MC_CRAFTING_OK;
}

static mc_crafting_status fields(const mc_item_stack *stack, mc_item_stack_fields *out,
                                 mc_crafting_error *error, mc_inventory_site frame) {
    mc_item_stack_phase phase = MC_ITEM_STACK_RAW;
    mc_item_stack_status status = mc_item_stack_phase_get(stack, &phase);
    if (status == MC_ITEM_STACK_OK && phase != MC_ITEM_STACK_READY)
        status = MC_ITEM_STACK_INVALID_PHASE;
    if (status == MC_ITEM_STACK_OK)
        status = mc_item_stack_fields_get(stack, out);
    return status == MC_ITEM_STACK_OK
               ? MC_CRAFTING_OK
               : stack_failure(error, status, "Crafting Stack field read", frame);
}

static mc_crafting_status registry_failure(mc_crafting_error *error, mc_registry_status status,
                                           const char *operation, mc_inventory_site frame) {
    mc_crafting_status result = MC_CRAFTING_REGISTRY_FAILED;
    if (status == MC_REGISTRY_NULL_REFERENCE)
        result = MC_CRAFTING_NULL_REFERENCE;
    else if (status == MC_REGISTRY_INDEX_OUT_OF_RANGE)
        result = MC_CRAFTING_INDEX_OUT_OF_BOUNDS;
    else if (status == MC_REGISTRY_LIMIT)
        result = MC_CRAFTING_LIMIT;
    else if (status == MC_REGISTRY_ALLOCATION_FAILED)
        result = MC_CRAFTING_ALLOCATION_FAILED;
    fail(error, result, operation, frame);
    error->registry_status = status;
    error->registry = (mc_registry_error){.status = status,
                                          .message = operation,
                                          .initializer_pc = -1,
                                          .block_id = -1,
                                          .constructor_pc = -1,
                                          .active_initializer_pc = -1,
                                          .item_argument = -1};
    return result;
}

static mc_inventory_site shaped_site(mc_registry_side side, int32_t pc) {
    return site(side, "is", "ft", "a", "(Lmq;IIZ)Z", "(Lhz;IIZ)Z", pc);
}

static mc_crafting_status shaped_at(mc_registry_side side, const mc_shaped_recipe_fields *shape,
                                    size_t length, const mc_inventory_crafting *grid,
                                    int32_t offset_x, int32_t offset_y, bool mirrored, bool *out,
                                    mc_crafting_error *error, matching_work *work) {
    for (int32_t x = 0; x < 3; ++x)
        for (int32_t y = 0; y < 3; ++y) {
            const mc_crafting_status progress = step(work, error, shaped_site(side, 18));
            if (progress != MC_CRAFTING_OK)
                return progress;
            const int32_t local_x = x - offset_x;
            const int32_t local_y = y - offset_y;
            const mc_item_stack *expected = NULL;
            if (local_x >= 0 && local_y >= 0 && local_x < shape->b && local_y < shape->c) {
                const int32_t index =
                    (mirrored ? shape->b - local_x - 1 : local_x) + local_y * shape->b;
                const mc_inventory_site load = shaped_site(side, mirrored ? 87 : 107);
                if (shape->d == NULL)
                    return fail(error, MC_CRAFTING_NULL_REFERENCE,
                                "Shaped recipe null ingredient array", load);
                if (index < 0 || (size_t)index >= length)
                    return fail(error, MC_CRAFTING_INDEX_OUT_OF_BOUNDS,
                                "Shaped recipe ingredient array index", load);
                expected = shape->d[(size_t)index];
            }
            const mc_item_stack *actual = NULL;
            const mc_inventory_status read =
                mc_inventory_crafting_borrow_xy(grid, x, y, &actual, error);
            if (read != MC_INVENTORY_OK)
                return caller(error, shaped_site(side, 115));
            if (actual == NULL && expected == NULL)
                continue;
            if (actual == NULL || expected == NULL) {
                *out = false;
                return MC_CRAFTING_OK;
            }
            mc_item_stack_fields wanted = {0}, found = {0};
            mc_crafting_status status = fields(expected, &wanted, error, shaped_site(side, 157));
            if (status == MC_CRAFTING_OK)
                status = fields(actual, &found, error, shaped_site(side, 162));
            if (status != MC_CRAFTING_OK)
                return status;
            if (wanted.id != found.id || (wanted.damage != -1 && wanted.damage != found.damage)) {
                *out = false;
                return MC_CRAFTING_OK;
            }
        }
    *out = true;
    return MC_CRAFTING_OK;
}

static mc_crafting_status shaped_matches(mc_registry_side side, const mc_recipe *recipe,
                                         const mc_inventory_crafting *grid, bool *out,
                                         mc_crafting_error *error, matching_work *work) {
    const mc_shaped_recipe_fields *shape = mc_recipe_shaped_fields_get(recipe);
    const mc_inventory_site frame = site(side, "is", "ft", "a", "(Lmq;)Z", "(Lhz;)Z", 8);
    if (shape == NULL)
        return fail(error, MC_CRAFTING_UNIMPLEMENTED_DEPENDENCY, "Shaped recipe concrete fields",
                    frame);
    const int32_t maximum_x = mc_java_i32_sub(3, shape->b);
    if (maximum_x < 0) {
        *out = false;
        return MC_CRAFTING_OK;
    }
    if (maximum_x > 3)
        return fail(error, MC_CRAFTING_LIMIT, "Shaped recipe native horizontal offset bound",
                    frame);
    const int32_t maximum_y = mc_java_i32_sub(3, shape->c);
    if (maximum_y < 0) {
        *out = false;
        return MC_CRAFTING_OK;
    }
    if (maximum_y > 3)
        return fail(error, MC_CRAFTING_LIMIT, "Shaped recipe native vertical offset bound",
                    site(side, "is", "ft", "a", "(Lmq;)Z", "(Lhz;)Z", 20));
    const size_t length = mc_recipe_shaped_array_length(recipe);
    for (int32_t x = 0; x <= maximum_x; ++x)
        for (int32_t y = 0; y <= maximum_y; ++y)
            for (unsigned attempt = 0; attempt < 2u; ++attempt) {
                bool matched = false;
                const mc_crafting_status status =
                    shaped_at(side, shape, length, grid, x, y, attempt == 0, &matched, error, work);
                if (status != MC_CRAFTING_OK)
                    return caller(error, site(side, "is", "ft", "a", "(Lmq;)Z", "(Lhz;)Z",
                                              attempt == 0 ? 29 : 42));
                if (matched) {
                    *out = true;
                    return MC_CRAFTING_OK;
                }
            }
    *out = false;
    return MC_CRAFTING_OK;
}

static mc_inventory_site shapeless_site(mc_registry_side side, int32_t pc) {
    return site(side, "tt", "ml", "a", "(Lmq;)Z", "(Lhz;)Z", pc);
}

static mc_crafting_status shapeless_matches(mc_registry_side side, const mc_recipe *recipe,
                                            const mc_inventory_crafting *grid, bool *out,
                                            mc_crafting_error *error, matching_work *work) {
    const mc_shapeless_recipe_fields *shape = mc_recipe_shapeless_fields_get(recipe);
    if (shape == NULL)
        return fail(error, MC_CRAFTING_UNIMPLEMENTED_DEPENDENCY, "Shapeless recipe concrete fields",
                    shapeless_site(side, 5));
    if (shape->b == NULL)
        return fail(error, MC_CRAFTING_NULL_REFERENCE, "Shapeless recipe null ingredient list",
                    shapeless_site(side, 8));
    size_t count = mc_recipe_ingredient_list_size(shape->b);
    if (count > MC_CRAFTING_MAX_INGREDIENTS)
        return fail(error, MC_CRAFTING_LIMIT, "Shapeless recipe native ingredient copy bound",
                    shapeless_site(side, 8));
    const mc_item_stack **remaining = malloc((count == 0 ? 1u : count) * sizeof(*remaining));
    if (remaining == NULL)
        return fail(error, MC_CRAFTING_ALLOCATION_FAILED, "Shapeless recipe ingredient copy",
                    shapeless_site(side, 0));
    mc_crafting_status status = MC_CRAFTING_OK;
    for (size_t i = 0; i < count; ++i) {
        status = step(work, error, shapeless_site(side, 8));
        if (status != MC_CRAFTING_OK)
            goto done;
        const mc_registry_status read = mc_recipe_ingredient_list_get(shape->b, i, &remaining[i]);
        if (read != MC_REGISTRY_OK) {
            status = registry_failure(error, read, "Shapeless recipe ingredient copy read",
                                      shapeless_site(side, 8));
            goto done;
        }
    }
    for (int32_t y = 0; y < 3; ++y)
        for (int32_t x = 0; x < 3; ++x) {
            const mc_item_stack *actual = NULL;
            const mc_inventory_status read =
                mc_inventory_crafting_borrow_xy(grid, x, y, &actual, error);
            if (read != MC_INVENTORY_OK) {
                status = caller(error, shapeless_site(side, 32));
                goto done;
            }
            if (actual == NULL)
                continue;
            mc_item_stack_fields found = {0};
            status = fields(actual, &found, error, shapeless_site(side, 77));
            if (status != MC_CRAFTING_OK)
                goto done;
            bool matched = false;
            for (size_t i = 0; i < count; ++i) {
                status = step(work, error, shapeless_site(side, 75));
                if (status != MC_CRAFTING_OK)
                    goto done;
                if (remaining[i] == NULL) {
                    status = fail(error, MC_CRAFTING_NULL_REFERENCE,
                                  "Shapeless recipe null ingredient", shapeless_site(side, 82));
                    goto done;
                }
                mc_item_stack_fields wanted = {0};
                status = fields(remaining[i], &wanted, error, shapeless_site(side, 82));
                if (status != MC_CRAFTING_OK)
                    goto done;
                if (found.id == wanted.id &&
                    (wanted.damage == -1 || found.damage == wanted.damage)) {
                    memmove(remaining + i, remaining + i + 1u,
                            (count - i - 1u) * sizeof(*remaining));
                    --count;
                    matched = true;
                    break;
                }
            }
            if (!matched) {
                *out = false;
                goto done;
            }
        }
    *out = count == 0;
done:
    free(remaining);
    return status;
}

static mc_crafting_status match(mc_registry_side side, const mc_recipe *recipe,
                                const mc_inventory_crafting *grid, bool *out,
                                mc_crafting_error *error, matching_work *work) {
    if (recipe == NULL)
        return fail(error, MC_CRAFTING_NULL_REFERENCE, "Recipe matching null receiver",
                    (mc_inventory_site){0});
    else if (mc_recipe_kind_get(recipe) == MC_RECIPE_SHAPED)
        return shaped_matches(side, recipe, grid, out, error, work);
    else if (mc_recipe_kind_get(recipe) == MC_RECIPE_SHAPELESS)
        return shapeless_matches(side, recipe, grid, out, error, work);
    return fail(error, MC_CRAFTING_UNIMPLEMENTED_DEPENDENCY, "Recipe matching concrete recipe kind",
                (mc_inventory_site){0});
}

mc_crafting_status mc_recipe_matches(mc_registry_side side, const mc_recipe *recipe,
                                     const mc_inventory_crafting *grid, bool *out,
                                     mc_crafting_error *error) {
    mc_crafting_error report = {0};
    matching_work work = {0};
    const mc_crafting_status status =
        !valid_side(side) || out == NULL ? fail(&report, MC_CRAFTING_INVALID_ARGUMENT,
                                                "Recipe matching admission", (mc_inventory_site){0})
                                         : match(side, recipe, grid, out, &report, &work);
    return finish(error, &report, status);
}

mc_crafting_status mc_recipe_craft_result(mc_registry_side side, const mc_recipe *recipe,
                                          const mc_inventory_crafting *grid, mc_item_stack **out,
                                          mc_crafting_error *error) {
    (void)grid;
    mc_crafting_error report = {0};
    mc_crafting_status status = MC_CRAFTING_OK;
    if (!valid_side(side) || out == NULL)
        status = fail(&report, MC_CRAFTING_INVALID_ARGUMENT, "Recipe output admission",
                      (mc_inventory_site){0});
    else if (recipe == NULL)
        status = fail(&report, MC_CRAFTING_NULL_REFERENCE, "Recipe output null receiver",
                      (mc_inventory_site){0});
    else if (mc_recipe_kind_get(recipe) == MC_RECIPE_SHAPED) {
        const mc_inventory_site allocation =
            site(side, "is", "ft", "b", "(Lmq;)Liz;", "(Lhz;)Lfy;", 0);
        mc_item_stack *result = NULL;
        const mc_item_stack_status allocated = mc_item_stack_allocate(&result);
        if (allocated != MC_ITEM_STACK_OK)
            status =
                stack_failure(&report, allocated, "Shaped output Stack allocation", allocation);
        else {
            const mc_item_stack *output = mc_recipe_output(recipe);
            const mc_inventory_site read =
                site(side, "is", "ft", "b", "(Lmq;)Liz;", "(Lhz;)Lfy;", 8);
            mc_item_stack_fields snapshot = {0};
            if (output == NULL)
                status =
                    fail(&report, MC_CRAFTING_NULL_REFERENCE, "Shaped recipe null output", read);
            else
                status = fields(output, &snapshot, &report, read);
            if (status == MC_CRAFTING_OK) {
                const mc_item_stack_status built =
                    mc_item_stack_construct(result, snapshot.id, snapshot.count, snapshot.damage);
                if (built != MC_ITEM_STACK_OK)
                    status =
                        stack_failure(&report, built, "Shaped output Stack construction",
                                      site(side, "is", "ft", "b", "(Lmq;)Liz;", "(Lhz;)Lfy;", 25));
            }
            if (status == MC_CRAFTING_OK)
                *out = result;
            else
                mc_item_stack_release(result);
        }
    } else if (mc_recipe_kind_get(recipe) == MC_RECIPE_SHAPELESS) {
        const mc_inventory_site clone = site(side, "tt", "ml", "b", "(Lmq;)Liz;", "(Lhz;)Lfy;", 4);
        const mc_item_stack *output = mc_recipe_output(recipe);
        if (output == NULL)
            status =
                fail(&report, MC_CRAFTING_NULL_REFERENCE, "Shapeless recipe null output", clone);
        else {
            const mc_item_stack_status copied = mc_item_stack_clone(output, out);
            if (copied != MC_ITEM_STACK_OK) {
                status = stack_failure(
                    &report, copied, "Shapeless output Stack clone",
                    site(side, "iz", "fy", side == MC_REGISTRY_CLIENT ? "k" : "j", "()Liz;",
                         "()Lfy;", copied == MC_ITEM_STACK_ALLOCATION_FAILED ? 0 : -1));
                status = caller(&report, clone);
            }
        }
    } else
        status = fail(&report, MC_CRAFTING_UNIMPLEMENTED_DEPENDENCY,
                      "Recipe output concrete recipe kind", (mc_inventory_site){0});
    return finish(error, &report, status);
}

mc_crafting_status mc_crafting_manager_find_matching(mc_registry_side side,
                                                     const mc_crafting_manager *manager,
                                                     const mc_inventory_crafting *grid,
                                                     mc_item_stack **out,
                                                     mc_crafting_error *error) {
    mc_crafting_error report = {0};
    mc_crafting_status status = MC_CRAFTING_OK;
    if (!valid_side(side) || out == NULL)
        status = fail(&report, MC_CRAFTING_INVALID_ARGUMENT, "Crafting manager matching admission",
                      (mc_inventory_site){0});
    else if (manager == NULL)
        status = fail(&report, MC_CRAFTING_NULL_REFERENCE, "Crafting manager null receiver",
                      (mc_inventory_site){0});
    else {
        size_t index = 0;
        matching_work work = {0};
        for (;;) {
            const size_t count = mc_crafting_manager_size(manager);
            const mc_inventory_site size_site =
                site(side, "hk", "ey", "a", "(Lmq;)Liz;", "(Lhz;)Lfy;", 7);
            status = step(&work, &report, size_site);
            if (status != MC_CRAFTING_OK)
                break;
            if (count > MC_CRAFTING_MAX_RECIPES) {
                status = fail(&report, MC_CRAFTING_LIMIT, "Crafting manager native recipe bound",
                              size_site);
                break;
            }
            if (index >= count) {
                if (count == 0) {
                    const mc_recipe *probe = NULL;
                    const mc_registry_status read = mc_crafting_manager_get(manager, 0, &probe);
                    if (read == MC_REGISTRY_NULL_REFERENCE) {
                        status = registry_failure(&report, read,
                                                  "Crafting manager null recipe list", size_site);
                        break;
                    }
                }
                *out = NULL;
                break;
            }
            const mc_recipe *recipe = NULL;
            const mc_registry_status read = mc_crafting_manager_get(manager, index, &recipe);
            if (read != MC_REGISTRY_OK) {
                status =
                    registry_failure(&report, read, "Crafting manager recipe list read",
                                     site(side, "hk", "ey", "a", "(Lmq;)Liz;", "(Lhz;)Lfy;", 20));
                break;
            }
            bool matched = false;
            status = match(side, recipe, grid, &matched, &report, &work);
            if (status != MC_CRAFTING_OK) {
                status =
                    caller(&report, site(side, "hk", "ey", "a", "(Lmq;)Liz;", "(Lhz;)Lfy;", 31));
                break;
            }
            if (matched) {
                status = mc_recipe_craft_result(side, recipe, grid, out, &report);
                if (status != MC_CRAFTING_OK)
                    status = caller(&report,
                                    site(side, "hk", "ey", "a", "(Lmq;)Liz;", "(Lhz;)Lfy;", 41));
                break;
            }
            ++index;
        }
    }
    return finish(error, &report, status);
}
