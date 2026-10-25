#include "../internal.h"

#include "mc/item/recipes/matching.h"

#include <string.h>

static const char *container_class(const mc_container *container) {
    return container->kind == MC_CONTAINER_PLAYER ? inv_class(container->side, "aa", "o")
                                                  : inv_class(container->side, "it", "fu");
}

static const char *constructor_desc(const mc_container *container) {
    return container->kind == MC_CONTAINER_PLAYER
               ? inv_class(container->side, "(Lix;Z)V", "(Lfx;Z)V")
               : inv_class(container->side, "(Lix;Lfd;III)V", "(Lfx;Ldj;III)V");
}

static void constructor_frame(mc_inventory_error *error, const mc_container *container,
                              int32_t pc) {
    const char *name = container_class(container);
    inv_frame(error, container->side, name, name, "<init>", constructor_desc(container), pc);
}

static inv_list *list_new(mc_container *container, int32_t pc, mc_inventory_error *error) {
    inv_list *list = inv_alloc(&container->budget, sizeof(*list), error);
    if (list == NULL)
        inv_frame(error, container->side, "dw", "cl", "<init>", "()V", pc);
    return list;
}

static bool append(mc_container *container, inv_list *list, void *value,
                   mc_inventory_error *error) {
    if (list->size >= container->budget.max_slots) {
        error->status = MC_INVENTORY_LIMIT;
        error->operation = "Container List native count bound";
        return false;
    }
    if (list->size == list->capacity) {
        size_t capacity = list->capacity == 0 ? 10u : list->capacity + list->capacity / 2u;
        if (capacity > container->budget.max_slots)
            capacity = container->budget.max_slots;
        if (capacity > SIZE_MAX / sizeof(*list->values)) {
            error->status = MC_INVENTORY_LIMIT;
            error->operation = "Container List native byte bound";
            return false;
        }
        void **values = inv_alloc(&container->budget, capacity * sizeof(*values), error);
        if (values == NULL)
            return false;
        if (list->size != 0)
            memcpy(values, list->values, list->size * sizeof(*values));
        inv_free(&container->budget, list->values);
        list->values = values;
        list->capacity = capacity;
    }
    list->values[list->size++] = value;
    return true;
}

static bool slot_add(mc_container *container, mc_slot_kind kind,
                     mc_slot_inventory_kind inventory_kind, void *inventory, int32_t index,
                     int32_t x, int32_t y, int32_t armor, int32_t source_new, int32_t source_add,
                     mc_inventory_error *error) {
    inv_slot *slot = inv_alloc(&container->budget, sizeof(*slot), error);
    if (slot == NULL) {
        constructor_frame(error, container, source_new);
        return false;
    }
    slot->allocated_next = container->allocated_slots;
    container->allocated_slots = slot;
    slot->state.kind = kind;
    slot->state.armor_type = armor;
    slot->state.armor_container = kind == MC_SLOT_ARMOR ? container : NULL;
    slot->state.inventory_kind = inventory_kind;
    slot->state.inventory = inventory;
    slot->state.inventory_index = index;
    slot->state.x = x;
    slot->state.y = y;
    slot->state.player = kind == MC_SLOT_CRAFTING ? container->inventory->player : NULL;
    slot->state.crafting_input = kind == MC_SLOT_CRAFTING ? container->crafting : NULL;
    slot->state.container_index = (int32_t)container->slots->size;
    if (!append(container, container->slots, slot, error)) {
        inv_frame(error, container->side, "dw", "cl", "a",
                  inv_class(container->side, "(Lgp;)V", "(Lel;)V"), 18);
        constructor_frame(error, container, source_add);
        return false;
    }
    if (!append(container, container->snapshots, NULL, error)) {
        inv_frame(error, container->side, "dw", "cl", "a",
                  inv_class(container->side, "(Lgp;)V", "(Lel;)V"), 29);
        constructor_frame(error, container, source_add);
        return false;
    }
    return true;
}

static void list_free(mc_container *container, inv_list *list, bool stacks) {
    if (list == NULL)
        return;
    if (stacks)
        for (size_t i = 0; i < list->size; ++i)
            mc_item_stack_release(list->values[i]);
    inv_free(&container->budget, list->values);
    inv_free(&container->budget, list);
}

static void container_free(mc_container *container) {
    if (container == NULL)
        return;
    inv_crafting_free(container->crafting);
    inv_result_free(container->result);
    while (container->allocated_slots != NULL) {
        inv_slot *slot = container->allocated_slots;
        container->allocated_slots = slot->allocated_next;
        inv_free(&container->budget, slot);
    }
    list_free(container, container->snapshots, true);
    list_free(container, container->slots, false);
    list_free(container, container->listeners, false);
    if (container->blocked_players != NULL) {
        inv_free(&container->budget, container->blocked_players->members);
        inv_free(&container->budget, container->blocked_players);
    }
    if (container->inventory_bound)
        --container->inventory->containers;
    inv_free(&container->budget, container);
}

mc_inventory_status inv_container_notify(mc_container *container, mc_inventory_error *error) {
    if (container == NULL) {
        *error = (mc_inventory_error){.status = MC_INVENTORY_NULL_REFERENCE,
                                      .operation = "Crafting null Container handler"};
        return error->status;
    }
    if (container->active && !container->constructing) {
        *error = (mc_inventory_error){.status = MC_INVENTORY_BUSY,
                                      .operation = "Container matching already active"};
        return error->status;
    }
    const bool prior_active = container->active;
    container->active = true;
    /* The result receiver is evaluated before class active-use and matching. */
    mc_inventory_result *result = container->result;
    mc_registry_error registry = {0};
    mc_registry_status status =
        mc_registry_classes_initialize_crafting(container->classes, &registry);
    if (status != MC_REGISTRY_OK) {
        *error = (mc_inventory_error){.status = MC_INVENTORY_REGISTRY_FAILED,
                                      .operation = "Container CraftingManager active use",
                                      .registry_status = status,
                                      .registry = registry};
        inv_frame(error, container->side, container_class(container), container_class(container),
                  "a", inv_class(container->side, "(Llw;)V", "(Lhp;)V"), 5);
        container->active = prior_active;
        return error->status;
    }
    mc_registry_crafting_view view;
    status = mc_registry_classes_inspect_crafting(container->classes, &view, &registry);
    if (status != MC_REGISTRY_OK) {
        *error = (mc_inventory_error){.status = MC_INVENTORY_REGISTRY_FAILED,
                                      .operation = "Container actual CraftingManager singleton",
                                      .registry_status = status,
                                      .registry = registry};
    } else {
        mc_item_stack *output = NULL;
        if (mc_crafting_manager_find_matching(container->side, view.a, container->crafting, &output,
                                              error) == MC_INVENTORY_OK) {
            mc_inventory_result_set(result, 0, output, error);
            mc_item_stack_release(output);
            if (error->status != MC_INVENTORY_OK)
                inv_frame(error, container->side, container_class(container),
                          container_class(container), "a",
                          inv_class(container->side, "(Llw;)V", "(Lhp;)V"), 15);
        } else
            inv_frame(error, container->side, container_class(container),
                      container_class(container), "a",
                      inv_class(container->side, "(Llw;)V", "(Lhp;)V"), 12);
    }
    container->active = prior_active;
    return error->status;
}

static mc_inventory_status construct(mc_registry_side side, mc_registry_classes *classes,
                                     mc_inventory_player *inventory, mc_container_kind kind,
                                     bool local, mc_world_runtime *world, int32_t x, int32_t y,
                                     int32_t z, const mc_inventory_limits *limits,
                                     mc_container **out, mc_inventory_error *result) {
    mc_inventory_error error = {0};
    inv_budget budget;
    if (!inv_side_valid(side) || classes == NULL || out == NULL || !inv_limits(limits, &budget) ||
        (inventory != NULL && inventory->side != side)) {
        error.status = MC_INVENTORY_INVALID_ARGUMENT;
        error.operation = "Container constructor native admission";
        return inv_finish(result, &error);
    }
    mc_container *container = inv_alloc(&budget, sizeof(*container), &error);
    if (container == NULL)
        return inv_finish(result, &error);
    container->budget = budget;
    container->side = side;
    container->classes = classes;
    container->kind = kind;
    container->constructing = container->active = true;
    container->snapshots = list_new(container, 5, &error);
    if (container->snapshots == NULL)
        goto base_failed;
    container->slots = list_new(container, 16, &error);
    if (container->slots == NULL)
        goto base_failed;
    container->window_id = 0;
    container->transaction = 0;
    container->listeners = list_new(container, 37, &error);
    if (container->listeners == NULL)
        goto base_failed;
    container->blocked_players =
        inv_alloc(&container->budget, sizeof(*container->blocked_players), &error);
    if (container->blocked_players == NULL) {
        inv_frame(&error, side, "dw", "cl", "<init>", "()V", 48);
        goto base_failed;
    }
    const int32_t width = kind == MC_CONTAINER_PLAYER ? 2 : 3;
    if (inv_crafting_new(side, container, width, width, &container->budget, &container->crafting,
                         &error) != MC_INVENTORY_OK) {
        constructor_frame(&error, container, 12);
        goto failed;
    }
    container->crafting->container_owned = true;
    if (inv_result_new(side, &container->budget, &container->result, &error) != MC_INVENTORY_OK) {
        constructor_frame(&error, container, 23);
        goto failed;
    }
    container->result->container_owned = true;
    if (kind == MC_CONTAINER_PLAYER)
        container->local = local;
    else {
        container->world = world;
        container->x = x;
        container->y = y;
        container->z = z;
    }
    if (inventory == NULL) {
        error.status = MC_INVENTORY_NULL_REFERENCE;
        error.operation = "Container InventoryPlayer Player field";
        constructor_frame(&error, container, kind == MC_CONTAINER_PLAYER ? 45 : 57);
        goto failed;
    }
    if (inventory->containers == SIZE_MAX) {
        error.status = MC_INVENTORY_LIMIT;
        error.operation = "Container native inventory lifetime bound";
        goto failed;
    }
    container->inventory = inventory;
    ++inventory->containers;
    container->inventory_bound = true;
    if (!slot_add(container, MC_SLOT_CRAFTING, MC_SLOT_INVENTORY_RESULT, container->result, 0,
                  kind == MC_CONTAINER_PLAYER ? 144 : 124, kind == MC_CONTAINER_PLAYER ? 36 : 35,
                  -1, kind == MC_CONTAINER_PLAYER ? 40 : 52, kind == MC_CONTAINER_PLAYER ? 65 : 76,
                  &error))
        goto failed;
    for (int32_t row = 0; row < width; ++row)
        for (int32_t column = 0; column < width; ++column)
            if (!slot_add(container, MC_SLOT_NORMAL, MC_SLOT_INVENTORY_CRAFTING,
                          container->crafting, column + row * width,
                          (kind == MC_CONTAINER_PLAYER ? 88 : 30) + column * 18,
                          (kind == MC_CONTAINER_PLAYER ? 26 : 17) + row * 18, -1,
                          kind == MC_CONTAINER_PLAYER ? 85 : 98,
                          kind == MC_CONTAINER_PLAYER ? 117 : 132, &error))
                goto failed;
    if (kind == MC_CONTAINER_PLAYER)
        for (int32_t armor = 0; armor < 4; ++armor)
            if (!slot_add(container, MC_SLOT_ARMOR, MC_SLOT_INVENTORY_PLAYER, inventory, 39 - armor,
                          8, 8 + armor * 18, armor, 143, 171, &error))
                goto failed;
    for (int32_t row = 0; row < 3; ++row)
        for (int32_t column = 0; column < 9; ++column)
            if (!slot_add(container, MC_SLOT_NORMAL, MC_SLOT_INVENTORY_PLAYER, inventory,
                          column + (row + 1) * 9, 8 + column * 18, 84 + row * 18, -1,
                          kind == MC_CONTAINER_PLAYER ? 198 : 167,
                          kind == MC_CONTAINER_PLAYER ? 230 : 202, &error))
                goto failed;
    for (int32_t column = 0; column < 9; ++column)
        if (!slot_add(container, MC_SLOT_NORMAL, MC_SLOT_INVENTORY_PLAYER, inventory, column,
                      8 + column * 18, 142, -1, kind == MC_CONTAINER_PLAYER ? 254 : 228,
                      kind == MC_CONTAINER_PLAYER ? 273 : 249, &error))
            goto failed;
    if (inv_container_notify(container, &error) != MC_INVENTORY_OK) {
        constructor_frame(&error, container, kind == MC_CONTAINER_PLAYER ? 287 : 263);
        goto failed;
    }
    container->constructing = container->active = false;
    container->constructed = true;
    *out = container;
    return inv_finish(result, &error);
base_failed:
    constructor_frame(&error, container, 1);
failed:
    container_free(container);
    return inv_finish(result, &error);
}

mc_inventory_status mc_container_player_create(mc_registry_side side, mc_registry_classes *classes,
                                               mc_inventory_player *inventory, bool local,
                                               const mc_inventory_limits *limits,
                                               mc_container **out, mc_inventory_error *error) {
    return construct(side, classes, inventory, MC_CONTAINER_PLAYER, local, NULL, 0, 0, 0, limits,
                     out, error);
}

mc_inventory_status mc_container_workbench_create(mc_registry_side side,
                                                  mc_registry_classes *classes,
                                                  mc_inventory_player *inventory,
                                                  mc_world_runtime *world, int32_t x, int32_t y,
                                                  int32_t z, const mc_inventory_limits *limits,
                                                  mc_container **out, mc_inventory_error *error) {
    return construct(side, classes, inventory, MC_CONTAINER_WORKBENCH, false, world, x, y, z,
                     limits, out, error);
}

mc_inventory_status mc_container_retire(mc_container **owner, mc_inventory_error *result) {
    mc_inventory_error error = {0};
    if (owner == NULL)
        error.status = MC_INVENTORY_INVALID_ARGUMENT;
    else if (*owner != NULL && (*owner)->active) {
        error.status = MC_INVENTORY_BUSY;
        error.operation = "Container operation active";
    } else {
        container_free(*owner);
        *owner = NULL;
    }
    return inv_finish(result, &error);
}

mc_inventory_status mc_container_player_create_default(
    mc_registry_side side, mc_registry_classes *classes, mc_inventory_player *inventory,
    const mc_inventory_limits *limits, mc_container **out, mc_inventory_error *result) {
    mc_inventory_error error = {0};
    mc_container_player_create(side, classes, inventory, true, limits, out, &error);
    if (error.status != MC_INVENTORY_OK && error.frame_count != 0)
        inv_frame(&error, side, "aa", "o", "<init>", inv_class(side, "(Lix;)V", "(Lfx;)V"), 3);
    return inv_finish(result, &error);
}

bool mc_container_read_state(const mc_container *container, mc_container_state *out) {
    if (container == NULL || out == NULL)
        return false;
    *out = (mc_container_state){.side = container->side,
                                .kind = container->kind,
                                .constructed = container->constructed,
                                .active = container->active,
                                .local = container->local,
                                .window_id = container->window_id,
                                .transaction = container->transaction,
                                .slots = container->slots->size,
                                .snapshots = container->snapshots->size,
                                .listeners = container->listeners->size,
                                .blocked_players = container->blocked_players->size,
                                .allocation_bytes = container->budget.used,
                                .slot_list = container->slots,
                                .snapshot_list = container->snapshots,
                                .listener_list = container->listeners,
                                .blocked_player_set = container->blocked_players,
                                .crafting = container->crafting,
                                .result = container->result,
                                .player_inventory = container->inventory,
                                .world = container->world,
                                .x = container->x,
                                .y = container->y,
                                .z = container->z};
    return true;
}

mc_inventory_status mc_container_slot_read(const mc_container *container, int32_t slot,
                                           mc_container_slot_state *out,
                                           mc_inventory_error *result) {
    mc_inventory_error error = {0};
    if (container == NULL)
        error.status = MC_INVENTORY_NULL_REFERENCE;
    else if (out == NULL)
        error.status = MC_INVENTORY_INVALID_ARGUMENT;
    else if (slot < 0 || (size_t)slot >= container->slots->size)
        error.status = MC_INVENTORY_INDEX_OUT_OF_BOUNDS;
    else
        *out = ((inv_slot *)container->slots->values[(size_t)slot])->state;
    return inv_finish(result, &error);
}

mc_inventory_status mc_container_inventory_changed(mc_container *container,
                                                   mc_inventory_error *result) {
    mc_inventory_error error = {0};
    inv_container_notify(container, &error);
    return inv_finish(result, &error);
}
