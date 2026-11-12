#ifndef MC_ENTITY_MINECART_INVENTORY_INTERNAL_H
#define MC_ENTITY_MINECART_INVENTORY_INTERNAL_H

#include "../../item/stack/source_internal.h"
#include "mc/entity/minecart/inventory.h"
#include "mc/entity/minecart/minecart.h"

/* Caller already holds the concrete canonical cart/array exclusively. Raw
 * current/length/borrowed-at reads add no source phase, Type or World check.
 * Nonnull array and index<actual length are preconditions for element access.
 * NBT must reload current h at its source reads, then capture target array126
 * and index before Stack new131; store140 uses those captured operands. */
mc_minecart_stack_array *mc_minecart_inventory_array_current(const mc_entity *cart);
size_t mc_minecart_inventory_array_length(const mc_minecart_stack_array *array);
mc_item_stack *mc_minecart_inventory_array_borrowed_at(const mc_minecart_stack_array *array,
                                                       size_t index);

/* Exact class/Entity direct-byte ledger. Caller supplies its reached operation
 * and PC (constructor8 or NBT71). Allocate initializes a detached NULL array;
 * out is unchanged on failure. Full Entity error is separate for NBT's
 * existing .construction child, never embedded in public inventory context. */
mc_entity_status mc_minecart_inventory_array_allocate(mc_entity *cart, size_t length,
                                                      const char *operation, uint16_t bytecode,
                                                      mc_minecart_stack_array **out,
                                                      mc_entity_error *allocation);

/* Swap only the actual h field and return the detached old array, without
 * retirement. Keep old array/occurrences live through all borrowing NBT input
 * reads, then retire it on both success/failure. Both arrays count at peak. */
mc_minecart_stack_array *mc_minecart_inventory_array_swap(mc_entity *cart,
                                                          mc_minecart_stack_array *replacement);

/* Store a prepared independent hold, consuming that one hold. Publish identity
 * before releasing the old occurrence. take transfers the old occurrence and
 * clears it without a redundant retain/overflow point. Retire releases every
 * actual occurrence once, then the exact array request on its original cart. */
void mc_minecart_inventory_array_store_owned(mc_minecart_stack_array *array, size_t index,
                                             mc_item_stack *owned);
mc_item_stack *mc_minecart_inventory_array_take(mc_minecart_stack_array *array, size_t index);
void mc_minecart_inventory_array_retire(mc_entity *cart, mc_minecart_stack_array *detached);

/* Passive complete lower-context projection. No World, allocation, class
 * initialization or string-based source-PC inference. NBT carries the compact
 * context and copies the supplied lower->cause into its existing full .stack child. */
void mc_minecart_inventory_stack_context(mc_minecart_inventory_context *context,
                                         mc_entity_source source,
                                         mc_minecart_inventory_stack_cause selected,
                                         const mc_item_stack_source_error *lower);

/* Honest native refusal seam for a single operation. Public split always uses
 * mc_item_stack_allocate; no persistent/global test configuration exists. */
mc_minecart_inventory_status
mc_minecart_inventory_split_with_allocator(mc_entity *cart, int32_t slot, int32_t amount,
                                           mc_item_stack_source_allocator allocate,
                                           mc_item_stack **out, mc_minecart_inventory_error *error);

#endif
