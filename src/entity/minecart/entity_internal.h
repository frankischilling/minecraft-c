#ifndef MC_ENTITY_MINECART_ENTITY_INTERNAL_H
#define MC_ENTITY_MINECART_ENTITY_INTERNAL_H

#include "../entity_internal.h"
#include "mc/entity/minecart/minecart.h"

struct mc_minecart_entity_state {
    mc_minecart_entity_fields fields;
    uint16_t world_bytecode, bytecode;
};

/* One actual native allocation. The shared Entity header gains only a
 * pointer to this genuine embedded state; Item/Falling never allocate it.
 * This complete object is zero/default initialized before RAW is published. */
typedef struct mc_minecart_entity_object {
    mc_entity entity;
    struct mc_minecart_entity_state state;
} mc_minecart_entity_object;

struct mc_minecart_stack_array {
    size_t length;
    mc_item_stack *elements[];
};

/* Final native Entity release only. Retires the actual h owner when present;
 * these constructors never acquire any Stack-element hold. It does
 * not clear/reset source fields during a live constructor failure. The shared
 * release then retires sizeof(mc_minecart_entity_object) at the same first-
 * member address once, not a separate state allocation or sizeof(mc_entity). */
void mc_minecart_entity_dispose(mc_entity *entity);

#endif
