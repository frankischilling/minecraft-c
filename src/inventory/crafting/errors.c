#include "../internal.h"

#include <stdlib.h>
#include <string.h>

typedef union inv_header {
    long double alignment;
    void *pointer;
    uint64_t integer;
    size_t bytes;
} inv_header;

mc_inventory_limits mc_inventory_default_limits(void) {
    return (mc_inventory_limits){16777216u, 65536u};
}

const char *inv_class(mc_registry_side side, const char *client, const char *server) {
    return side == MC_REGISTRY_CLIENT ? client : server;
}

bool inv_side_valid(mc_registry_side side) {
    return side == MC_REGISTRY_CLIENT || side == MC_REGISTRY_SERVER;
}

mc_inventory_error inv_error(mc_inventory_status status, const char *operation,
                             mc_registry_side side, const char *client, const char *server,
                             const char *method, const char *descriptor, int32_t pc) {
    mc_inventory_error error = {.status = status, .operation = operation};
    if (client != NULL)
        inv_frame(&error, side, client, server, method, descriptor, pc);
    return error;
}

void inv_frame(mc_inventory_error *error, mc_registry_side side, const char *client,
               const char *server, const char *method, const char *descriptor, int32_t pc) {
    if (error->frame_count < MC_INVENTORY_ERROR_FRAMES) {
        memmove(error->frames + 1, error->frames, error->frame_count * sizeof(*error->frames));
        ++error->frame_count;
        error->frames[0] =
            (mc_inventory_site){inv_class(side, client, server), method, descriptor, pc};
    } else {
        error->status = MC_INVENTORY_LIMIT;
        error->operation = "Inventory native source frame bound";
    }
}

mc_inventory_status inv_finish(mc_inventory_error *out, const mc_inventory_error *error) {
    if (out != NULL)
        *out = *error;
    return error->status;
}

mc_inventory_status inv_stack(mc_inventory_error *error, mc_item_stack_status status,
                              const char *operation) {
    if (status != MC_ITEM_STACK_OK) {
        error->status = status == MC_ITEM_STACK_ALLOCATION_FAILED ? MC_INVENTORY_ALLOCATION_FAILED
                        : status == MC_ITEM_STACK_LIMIT           ? MC_INVENTORY_LIMIT
                                                                  : MC_INVENTORY_STACK_FAILED;
        error->operation = operation;
        error->stack_status = status;
        error->stack.status = status;
    }
    return error->status;
}

bool inv_limits(const mc_inventory_limits *options, inv_budget *out) {
    const mc_inventory_limits selected = options == NULL ? mc_inventory_default_limits() : *options;
    if (selected.allocation_bytes < 256u || selected.allocation_bytes > 1073741824u ||
        selected.max_slots == 0 || selected.max_slots > 1048576u)
        return false;
    *out = (inv_budget){0, selected.allocation_bytes, selected.max_slots};
    return true;
}

void *inv_alloc(inv_budget *budget, size_t bytes, mc_inventory_error *error) {
    if (bytes > SIZE_MAX - sizeof(inv_header) || budget->used > budget->limit ||
        bytes + sizeof(inv_header) > budget->limit - budget->used) {
        error->status = MC_INVENTORY_LIMIT;
        error->operation = "Inventory allocation budget";
        return NULL;
    }
    inv_header *allocation = calloc(1, sizeof(*allocation) + bytes);
    if (allocation == NULL) {
        error->status = MC_INVENTORY_ALLOCATION_FAILED;
        error->operation = "Inventory native allocation";
        return NULL;
    }
    allocation->bytes = sizeof(*allocation) + bytes;
    budget->used += allocation->bytes;
    return allocation + 1;
}

void inv_free(inv_budget *budget, void *data) {
    if (data == NULL)
        return;
    inv_header *header = (inv_header *)data - 1;
    budget->used -= header->bytes;
    free(header);
}

mc_inventory_status inv_hold(mc_item_stack *value, mc_inventory_error *error) {
    return value == NULL
               ? MC_INVENTORY_OK
               : inv_stack(error, mc_item_stack_retain(value), "Inventory occurrence hold");
}

mc_inventory_status inv_publish_hold(const mc_item_stack *value, mc_item_stack **out,
                                     mc_inventory_error *error) {
    const mc_inventory_status status = inv_hold((mc_item_stack *)value, error);
    if (status == MC_INVENTORY_OK)
        *out = (mc_item_stack *)value;
    return status;
}
