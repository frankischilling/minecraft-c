#ifndef MC_ITEM_REGISTRY_STACKS_INTERNAL_H
#define MC_ITEM_REGISTRY_STACKS_INTERNAL_H

#include "mc/item/registry/registry.h"
#include "mc/item/stack/stack.h"

struct mc_registry_stack_allocation;

/* Private admitted construction flow only; out storage must not overlap class
 * or stack storage. Allocate before evaluating the source constructor arguments.
 * The class owner acquires the real stack's initial hold, retains every RAW or
 * constructed prefix, and charges its exact object bytes plus ledger storage.
 * out is borrowed from that owner and unchanged on failure; do not release its
 * class hold. Retain independently to keep it beyond class retirement.
 * Allocation errors keep the caller's diagnostic operation, rather than
 * inventing an iz/fy body. */
mc_registry_status mc_registry_stack_allocate(mc_registry_classes *classes, mc_item_stack **out);

/* raw must be the same class owner's allocated RAW stack; the native identity
 * and phase guard precedes source argument reads. No registry/ID/phase/max-count
 * validation is performed on the supplied Item or Block. A NULL supplied icon
 * fails at the reached iz/fy getfield2 and leaves the allocated stack RAW.
 * Block's one-argument constructor forwards count1 to its block/count overload.
 * Successful construction uses actual bf/bn, count1, damage0 and animation0.
 * There is no new hold, clone, provenance copy or provider lookup. */
mc_registry_status mc_registry_stack_construct_item(mc_registry_classes *classes,
                                                    mc_item_stack *raw, const mc_item *item);
mc_registry_status mc_registry_stack_construct_block(mc_registry_classes *classes,
                                                     mc_item_stack *raw, const mc_block *block);

/* Quiescent final class retirement only, before freeing the class arena.
 * Drop the one real hold per linked owner and detach the ledger; repeated calls
 * while the arena is still live are harmless. Ledger storage/charges remain in
 * the retiring arena, rather than reopening a live class's allocation budget.
 * Independently retained stack holds remain valid after class retirement.
 * NULL is accepted. */
void mc_registry_stacks_retire(mc_registry_classes *classes);

/* Native retained-hold diagnostics, newest allocation first, not Java heap
 * order. No allocation or class initialization occurs. Outputs are unchanged
 * on error; a returned stack borrows the class hold until retirement. */
size_t mc_registry_stack_count(const mc_registry_classes *classes);
mc_registry_status mc_registry_stack_get(const mc_registry_classes *classes, size_t index,
                                         const mc_item_stack **out);

#endif
