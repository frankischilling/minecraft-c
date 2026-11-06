#include "inventory_internal.h"

const char *mc_minecart_inventory_status_string(mc_minecart_inventory_status status) {
    switch (status) {
        case MC_MINECART_INVENTORY_OK:
            return "ok";
        case MC_MINECART_INVENTORY_INVALID_ARGUMENT:
            return "invalid argument";
        case MC_MINECART_INVENTORY_NULL_REFERENCE:
            return "null inventory array";
        case MC_MINECART_INVENTORY_INDEX_OUT_OF_BOUNDS:
            return "inventory index out of bounds";
        case MC_MINECART_INVENTORY_LIMIT:
            return "resource limit";
        case MC_MINECART_INVENTORY_ALLOCATION_FAILED:
            return "allocation failed";
        case MC_MINECART_INVENTORY_STACK_FAILED:
            return "Stack operation failed";
    }
    return "unknown Minecart inventory status";
}

static mc_minecart_site stack_site(mc_item_stack_source_site site, bool client,
                                   const char *method) {
    if (!site.selected)
        return (mc_minecart_site){0};
    return (mc_minecart_site){true, site.entered, client ? "iz" : "fy", method, site.bytecode};
}

void mc_minecart_inventory_stack_context(mc_minecart_inventory_context *context,
                                         mc_entity_source source,
                                         mc_minecart_inventory_stack_cause selected,
                                         const mc_item_stack_source_error *lower) {
    const bool client = source == MC_ENTITY_CLIENT;
    context->status = lower->cause.status == MC_ITEM_STACK_OK      ? MC_MINECART_INVENTORY_OK
                      : lower->cause.status == MC_ITEM_STACK_LIMIT ? MC_MINECART_INVENTORY_LIMIT
                      : lower->cause.status == MC_ITEM_STACK_ALLOCATION_FAILED
                          ? MC_MINECART_INVENTORY_ALLOCATION_FAILED
                          : MC_MINECART_INVENTORY_STACK_FAILED;
    context->stack_cause = selected;
    context->stack_split =
        stack_site(lower->context.split, client, client ? "a(I)Liz;" : "a(I)Lfy;");
    const bool nbt = lower->context.constructor_kind == MC_ITEM_STACK_SOURCE_CONSTRUCTOR_NBT;
    context->stack_constructor =
        stack_site(lower->context.constructor, client,
                   nbt ? (client ? "<init>(Lnu;)V" : "<init>(Liq;)V") : "<init>(III)V");
    context->stack_read = stack_site(lower->context.read, client, client ? "b(Lnu;)V" : "b(Liq;)V");
}
