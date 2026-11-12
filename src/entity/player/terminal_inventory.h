#ifndef MC_ENTITY_PLAYER_TERMINAL_INVENTORY_H
#define MC_ENTITY_PLAYER_TERMINAL_INVENTORY_H

#include "mc/entity/entity.h"
#include "mc/inventory/container.h"

typedef struct mc_inventory_player_terminal mc_inventory_player_terminal;

/* Native-only ownership diagnostics. No Source method is entered, no Source
 * exception is thrown, and the nested Inventory frames remain empty. */
typedef struct mc_player_terminal_inventory_error {
    mc_entity_status status;
    const char *operation;
    mc_inventory_error inventory;
} mc_player_terminal_inventory_error;

/* Trusted selected-root terminal adapter only. The caller has ended actual
 * root Source admission, checked-retired its reached children, and proved the
 * exact Load/Screen constructor and publication Player/Inventory/World ledger.
 * This module proves the local canonical Player and Inventory owner graph;
 * it does not manufacture authority to collect an arbitrary live graph.
 *
 * owned_it is the actual OO native owning slot, not a Source j/e alias or a
 * copied pointer. All local checks precede the Inventory's atomic seal. On
 * success that SAME IT moves into the Player's native sidecar and *owned_it
 * becomes NULL; Source d/e/c/j/r, Stack fields, entropy and refcounts do not
 * change. No allocation, constructor, clone, close, drop or callback occurs.
 * Failure preserves the owning slot, sidecar and Source graph. A second owner
 * cannot be adopted into an occupied sidecar. error is optional/disjoint.
 * Actual owners and dependencies remain live/exclusive throughout. */
mc_entity_status mc_player_terminal_inventory_adopt(mc_entity *player,
                                                    mc_registry_classes *registry,
                                                    mc_world_runtime *world,
                                                    mc_container **owned_it,
                                                    mc_player_terminal_inventory_error *error);

/* Fresh passive proof for the same admitted World/Registry and sealed local
 * owner graph. This is not a cached whole-World admission permit. A Player
 * without an adopted terminal owner is left to its existing owner protocol.
 * No Source field, native hold, seal, diagnostic owner or caller output changes
 * except the optional value error. */
mc_entity_status mc_player_terminal_inventory_admit(const mc_entity *player,
                                                    mc_registry_classes *registry,
                                                    mc_world_runtime *world,
                                                    mc_player_terminal_inventory_error *error);

/* Passive copy of native state, including an empty unsealed sidecar. All
 * pointers in out borrow the actual live Player/Inventory owners. Invalid
 * represented storage or NULL out preserves *out and returns false. */
bool mc_player_terminal_inventory_inspect(const mc_entity *player,
                                          mc_inventory_player_terminal *out);

/* Central native new-Player-right admission predicate. It performs no Source
 * read, retain or callback; RAW/unrepresented/non-Player receivers are false.
 * Public Entity retain and the separate proven terminal-pin path are owned by
 * Entity/World, not this module. Passive snapshots remain phase-transparent. */
bool mc_local_player_terminal_sealed(const mc_entity *player);

/* Infallible native commit at final zero-hold Player destruction only. The
 * selected terminal protocol has freshly admitted this sealed owner graph and
 * prevented new rights. Consume actual IT -> AA -> IX through the Inventory
 * owner, before the Entity's SN World binding ends. Never invoke Source close
 * or write Source d/e/c. An empty unsealed sidecar is a no-op. */
void mc_player_terminal_inventory_dispose(mc_entity *player);

#endif
