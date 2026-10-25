#include "../internal.h"

#include <limits.h>

static const char *grid_desc(mc_registry_side side, const char *client, const char *server) {
    return inv_class(side, client, server);
}

static void grid_frame(mc_inventory_error *error, const mc_inventory_crafting *grid,
                       const char *method, const char *client, const char *server, int32_t pc) {
    inv_frame(error, grid->side, "mq", "hz", method, grid_desc(grid->side, client, server), pc);
}

static mc_inventory_error grid_admit(const mc_inventory_crafting *grid, const void *out) {
    return inv_error(grid == NULL  ? MC_INVENTORY_NULL_REFERENCE
                     : out == NULL ? MC_INVENTORY_INVALID_ARGUMENT
                                   : MC_INVENTORY_OK,
                     grid == NULL  ? "Crafting inventory null receiver"
                     : out == NULL ? "Crafting inventory output"
                                   : NULL,
                     MC_REGISTRY_CLIENT, NULL, NULL, NULL, NULL, -1);
}

mc_inventory_status inv_crafting_new(mc_registry_side side, mc_container *handler, int32_t width,
                                     int32_t height, inv_budget *budget,
                                     mc_inventory_crafting **out, mc_inventory_error *error) {
    mc_inventory_crafting *grid = inv_alloc(budget, sizeof(*grid), error);
    if (grid == NULL)
        return error->status;
    grid->budget = budget;
    grid->side = side;
    const int32_t count = mc_java_i32_mul(width, height);
    if (count < 0) {
        *error = inv_error(MC_INVENTORY_NEGATIVE_ARRAY_SIZE, "Crafting negative array size", side,
                           "mq", "hz", "<init>", grid_desc(side, "(Ldw;II)V", "(Lcl;II)V"), 12);
        inv_crafting_free(grid);
        return error->status;
    }
    if ((size_t)count > budget->max_slots || (size_t)count > SIZE_MAX / sizeof(*grid->slots)) {
        *error = inv_error(MC_INVENTORY_LIMIT, "Crafting native slot limit", side, "mq", "hz",
                           "<init>", grid_desc(side, "(Ldw;II)V", "(Lcl;II)V"), 12);
        inv_crafting_free(grid);
        return error->status;
    }
    grid->slots = inv_alloc(budget, (size_t)count * sizeof(*grid->slots), error);
    if (grid->slots == NULL) {
        grid_frame(error, grid, "<init>", "(Ldw;II)V", "(Lcl;II)V", 12);
        inv_crafting_free(grid);
        return error->status;
    }
    grid->size = (size_t)count;
    grid->handler = handler;
    grid->width = width;
    *out = grid;
    return MC_INVENTORY_OK;
}

void inv_crafting_free(mc_inventory_crafting *grid) {
    if (grid == NULL)
        return;
    for (size_t i = 0; i < grid->size; ++i)
        mc_item_stack_release(grid->slots[i]);
    inv_budget *budget = grid->budget;
    inv_free(budget, grid->slots);
    inv_free(budget, grid);
}

mc_inventory_status mc_inventory_crafting_create(mc_registry_side side, mc_container *handler,
                                                 int32_t width, int32_t height,
                                                 const mc_inventory_limits *limits,
                                                 mc_inventory_crafting **out,
                                                 mc_inventory_error *result) {
    mc_inventory_error error = {0};
    inv_budget budget;
    if (!inv_side_valid(side) || out == NULL || !inv_limits(limits, &budget)) {
        error.status = MC_INVENTORY_INVALID_ARGUMENT;
        error.operation = "Crafting inventory construction admission";
    } else {
        mc_inventory_crafting *grid = NULL;
        if (inv_crafting_new(side, handler, width, height, &budget, &grid, &error) ==
            MC_INVENTORY_OK) {
            grid->local_budget = budget;
            grid->budget = &grid->local_budget;
            *out = grid;
        }
    }
    return inv_finish(result, &error);
}

mc_inventory_status mc_inventory_crafting_retire(mc_inventory_crafting **owner,
                                                 mc_inventory_error *result) {
    mc_inventory_error error = {0};
    if (owner == NULL)
        error.status = MC_INVENTORY_INVALID_ARGUMENT;
    else if (*owner != NULL && ((*owner)->active || (*owner)->container_owned)) {
        error.status = MC_INVENTORY_BUSY;
        error.operation = "Crafting inventory retained by concrete Container";
    } else {
        inv_crafting_free(*owner);
        *owner = NULL;
    }
    return inv_finish(result, &error);
}

mc_inventory_status mc_inventory_crafting_size(const mc_inventory_crafting *grid, int32_t *out,
                                               mc_inventory_error *result) {
    mc_inventory_error error = grid_admit(grid, out);
    if (error.status == MC_INVENTORY_OK)
        *out = (int32_t)grid->size;
    return inv_finish(result, &error);
}

mc_inventory_status mc_inventory_crafting_width(const mc_inventory_crafting *grid, int32_t *out,
                                                mc_inventory_error *result) {
    mc_inventory_error error = grid_admit(grid, out);
    if (error.status == MC_INVENTORY_OK)
        *out = grid->width;
    return inv_finish(result, &error);
}

mc_inventory_status mc_inventory_crafting_borrow_slot(const mc_inventory_crafting *grid,
                                                      int32_t slot, const mc_item_stack **out,
                                                      mc_inventory_error *result) {
    mc_inventory_error error = grid_admit(grid, out);
    if (error.status == MC_INVENTORY_OK) {
        if (slot >= (int32_t)grid->size)
            *out = NULL;
        else if (slot < 0) {
            error.status = MC_INVENTORY_INDEX_OUT_OF_BOUNDS;
            error.operation = "Crafting negative linear index";
            grid_frame(&error, grid, inv_class(grid->side, "f_", "d_"), "(I)Liz;", "(I)Lfy;", 15);
        } else
            *out = grid->slots[(size_t)slot];
    }
    return inv_finish(result, &error);
}

mc_inventory_status mc_inventory_crafting_borrow_xy(const mc_inventory_crafting *grid, int32_t x,
                                                    int32_t y, const mc_item_stack **out,
                                                    mc_inventory_error *result) {
    mc_inventory_error error = grid_admit(grid, out);
    if (error.status == MC_INVENTORY_OK) {
        if (x < 0 || x >= grid->width)
            *out = NULL;
        else {
            const int32_t slot = mc_java_i32_add(x, mc_java_i32_mul(y, grid->width));
            mc_inventory_crafting_borrow_slot(grid, slot, out, &error);
            if (error.status != MC_INVENTORY_OK)
                grid_frame(&error, grid, "b", "(II)Liz;", "(II)Lfy;", 25);
        }
    }
    return inv_finish(result, &error);
}

mc_inventory_status mc_inventory_crafting_get(mc_inventory_crafting *grid, int32_t slot,
                                              mc_item_stack **out, mc_inventory_error *result) {
    mc_inventory_error error = grid_admit(grid, out);
    const mc_item_stack *stack = NULL;
    if (error.status == MC_INVENTORY_OK &&
        mc_inventory_crafting_borrow_slot(grid, slot, &stack, &error) == MC_INVENTORY_OK)
        inv_publish_hold(stack, out, &error);
    return inv_finish(result, &error);
}

mc_inventory_status mc_inventory_crafting_get_xy(mc_inventory_crafting *grid, int32_t x, int32_t y,
                                                 mc_item_stack **out, mc_inventory_error *result) {
    mc_inventory_error error = grid_admit(grid, out);
    const mc_item_stack *stack = NULL;
    if (error.status == MC_INVENTORY_OK &&
        mc_inventory_crafting_borrow_xy(grid, x, y, &stack, &error) == MC_INVENTORY_OK)
        inv_publish_hold(stack, out, &error);
    return inv_finish(result, &error);
}

static mc_inventory_status notify(mc_inventory_crafting *grid, int32_t pc, const char *client,
                                  const char *server, mc_inventory_error *error) {
    if (inv_container_notify(grid->handler, error) != MC_INVENTORY_OK) {
        grid_frame(error, grid, "a", client, server, pc);
    }
    error->mutation_reached = true;
    error->notification_reached = true;
    return error->status;
}

mc_inventory_status mc_inventory_crafting_set(mc_inventory_crafting *grid, int32_t slot,
                                              mc_item_stack *value, mc_inventory_error *result) {
    mc_inventory_error error = grid_admit(grid, grid);
    if (error.status == MC_INVENTORY_OK) {
        if (grid->active) {
            error.status = MC_INVENTORY_BUSY;
            error.operation = "Crafting mutation already active";
        } else if (slot < 0 || (size_t)slot >= grid->size) {
            error.status = MC_INVENTORY_INDEX_OUT_OF_BOUNDS;
            error.operation = "Crafting setter array index";
            grid_frame(&error, grid, "a", "(ILiz;)V", "(ILfy;)V", 6);
        } else if (inv_hold(value, &error) == MC_INVENTORY_OK) {
            grid->active = true;
            mc_item_stack *old = grid->slots[(size_t)slot];
            grid->slots[(size_t)slot] = value;
            mc_item_stack_release(old);
            notify(grid, 12, "(ILiz;)V", "(ILfy;)V", &error);
            grid->active = false;
        }
    }
    return inv_finish(result, &error);
}

mc_inventory_status mc_inventory_crafting_split(mc_inventory_crafting *grid, int32_t slot,
                                                int32_t amount, mc_item_stack **out,
                                                mc_inventory_error *result) {
    mc_inventory_error error = grid_admit(grid, out);
    if (error.status != MC_INVENTORY_OK)
        return inv_finish(result, &error);
    if (grid->active) {
        error.status = MC_INVENTORY_BUSY;
        return inv_finish(result, &error);
    }
    if (slot < 0 || (size_t)slot >= grid->size) {
        error.status = MC_INVENTORY_INDEX_OUT_OF_BOUNDS;
        error.operation = "Crafting splitter array index";
        grid_frame(&error, grid, "a", "(II)Liz;", "(II)Lfy;", 5);
        return inv_finish(result, &error);
    }
    mc_item_stack *source = grid->slots[(size_t)slot];
    if (source == NULL) {
        *out = NULL;
        return inv_finish(result, &error);
    }
    grid->active = true;
    mc_item_stack_fields fields;
    mc_item_stack *removed = NULL;
    int32_t notification_pc = 41;
    inv_stack(&error, mc_item_stack_fields_get(source, &fields), "Crafting source Stack fields");
    if (error.status == MC_INVENTORY_OK && fields.count <= amount) {
        removed = source;
        grid->slots[(size_t)slot] = NULL;
    } else if (error.status == MC_INVENTORY_OK) {
        notification_pc = 81;
        inv_stack(&error, mc_item_stack_split(source, amount, &removed), "Crafting Stack.split");
        if (error.status == MC_INVENTORY_OK) {
            mc_item_stack_fields_get(source, &fields);
            if (fields.count == 0) {
                grid->slots[(size_t)slot] = NULL;
                mc_item_stack_release(source);
            }
        } else {
            /* Stack.split stores its wrapped subtraction before allocation. */
            error.mutation_reached = error.stack_status == MC_ITEM_STACK_ALLOCATION_FAILED ||
                                     error.stack_status == MC_ITEM_STACK_LIMIT;
            if (error.mutation_reached)
                inv_frame(&error, grid->side, "iz", "fy", "a",
                          inv_class(grid->side, "(I)Liz;", "(I)Lfy;"), 10);
            grid_frame(&error, grid, "a", "(II)Liz;", "(II)Lfy;", 53);
        }
    }
    if (error.status == MC_INVENTORY_OK)
        notify(grid, notification_pc, "(II)Liz;", "(II)Lfy;", &error);
    if (error.status == MC_INVENTORY_OK)
        *out = removed;
    else
        mc_item_stack_release(removed);
    grid->active = false;
    return inv_finish(result, &error);
}

mc_inventory_status mc_inventory_crafting_name(const mc_inventory_crafting *grid, const char **out,
                                               mc_inventory_error *result) {
    mc_inventory_error error = grid_admit(grid, out);
    if (error.status == MC_INVENTORY_OK)
        *out = "Crafting";
    return inv_finish(result, &error);
}

mc_inventory_status mc_inventory_crafting_limit(const mc_inventory_crafting *grid, int32_t *out,
                                                mc_inventory_error *result) {
    mc_inventory_error error = grid_admit(grid, out);
    if (error.status == MC_INVENTORY_OK)
        *out = 64;
    return inv_finish(result, &error);
}

mc_inventory_status mc_inventory_crafting_changed(mc_inventory_crafting *grid,
                                                  mc_inventory_error *result) {
    mc_inventory_error error = grid_admit(grid, grid);
    return inv_finish(result, &error);
}

mc_inventory_status mc_inventory_crafting_usable(const mc_inventory_crafting *grid,
                                                 const void *player, bool *out,
                                                 mc_inventory_error *result) {
    (void)player;
    mc_inventory_error error = grid_admit(grid, out);
    if (error.status == MC_INVENTORY_OK)
        *out = true;
    return inv_finish(result, &error);
}
