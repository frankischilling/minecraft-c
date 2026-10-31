#ifndef MC_ENTITY_MINECART_ENTITY_INTERNAL_H
#define MC_ENTITY_MINECART_ENTITY_INTERNAL_H

#include "../entity_internal.h"
#include "mc/entity/minecart/minecart.h"

struct mc_minecart_entity_state {
    mc_minecart_entity_fields fields;
    uint16_t world_bytecode, bytecode;
    /* Native root provenance owner; Entity.storage_source is a borrowed mirror. */
    struct mc_minecart_storage_generation *root_generation;
};

/* One actual native allocation. The shared Entity header gains only a
 * pointer to this genuine embedded state; Item/Falling never allocate it.
 * This complete object is zero/default initialized before RAW is published. */
typedef struct mc_minecart_entity_object {
    mc_entity entity;
    struct mc_minecart_entity_state state;
} mc_minecart_entity_object;

/* Older MSVC diagnoses this standard C17 flexible array as extension C4200.
 * Scope the compatibility guard to this native array declaration. */
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4200)
#endif
struct mc_minecart_stack_array {
    size_t length;
    /* Optional native occurrence provenance, independent of Stack identity. */
    struct mc_minecart_storage_sidecar *storage;
    mc_item_stack *elements[];
};
#ifdef _MSC_VER
#pragma warning(pop)
#endif

/* Final native Entity release only. Retires the actual h owner and every
 * retained Stack occurrence when present. It does
 * not clear/reset source fields during a live constructor failure. The shared
 * release then retires sizeof(mc_minecart_entity_object) at the same first-
 * member address once, not a separate state allocation or sizeof(mc_entity). */
void mc_minecart_entity_dispose(mc_entity *entity);

#endif
