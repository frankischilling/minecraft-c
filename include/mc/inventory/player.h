#ifndef MC_INVENTORY_PLAYER_H
#define MC_INVENTORY_PLAYER_H

#include "mc/inventory/errors.h"

typedef struct mc_entity mc_entity;
typedef struct mc_inventory_player mc_inventory_player;

typedef struct mc_inventory_player_state {
    mc_entity *player; /* same actual Player identity; nullable constructor input */
    int32_t selected;
    bool changed;
    const mc_item_stack *cursor;
    mc_item_stack *const *main_slots;
    mc_item_stack *const *armor_slots;
    size_t bound_containers, allocation_bytes;
} mc_inventory_player_state;

/* Construction/read/ordinary IInventory methods only. Player gameplay,
 * armor calculations, insertion, NBT, cursor hooks and drops remain separate
 * concrete dependencies. A nonnull player must be an actual Player owner. */
mc_inventory_status mc_inventory_player_create(mc_registry_side side, mc_entity *player,
                                               const mc_inventory_limits *limits,
                                               mc_inventory_player **out,
                                               mc_inventory_error *error);
mc_inventory_status mc_inventory_player_retire(mc_inventory_player **inventory,
                                               mc_inventory_error *error);
bool mc_inventory_player_read_state(const mc_inventory_player *inventory,
                                    mc_inventory_player_state *out);
mc_inventory_status mc_inventory_player_size(const mc_inventory_player *inventory, int32_t *out,
                                             mc_inventory_error *error);
mc_inventory_status mc_inventory_player_borrow(const mc_inventory_player *inventory, int32_t slot,
                                               const mc_item_stack **out,
                                               mc_inventory_error *error);
mc_inventory_status mc_inventory_player_get(mc_inventory_player *inventory, int32_t slot,
                                            mc_item_stack **out, mc_inventory_error *error);
mc_inventory_status mc_inventory_player_set(mc_inventory_player *inventory, int32_t slot,
                                            mc_item_stack *value, mc_inventory_error *error);
mc_inventory_status mc_inventory_player_split(mc_inventory_player *inventory, int32_t slot,
                                              int32_t amount, mc_item_stack **out,
                                              mc_inventory_error *error);
mc_inventory_status mc_inventory_player_changed(mc_inventory_player *inventory,
                                                mc_inventory_error *error);
mc_inventory_status mc_inventory_player_name(const mc_inventory_player *inventory, const char **out,
                                             mc_inventory_error *error);
mc_inventory_status mc_inventory_player_limit(const mc_inventory_player *inventory, int32_t *out,
                                              mc_inventory_error *error);

/* Compact receipts for the actual client selection leaves. These errors are
 * native admission/policy failures; a null receiver's Java PC belongs to the
 * Minecraft caller, not an invented Inventory method invocation. No Stack,
 * notification, changed flag, allocation or Player/World call is performed.
 * The actual owner must stay live/exclusive; container occurrences are allowed
 * and remain intact. An active owner rejects entry without a source store.
 * Server FX has no paired b(I)V and is an explicit unsupported dependency.
 * Error storage is optional and disjoint from all owner fields. */
typedef struct mc_inventory_selection_error {
    mc_inventory_status status;
    const char *operation;
    bool native_policy, source_entered, mutation_reached;
    mc_inventory_site site;
    size_t loop_iterations;
} mc_inventory_selection_error;

/* Actual ix.b(I)V, including sign normalization, Java-wrap subtraction19 and
 * the original repeated +/-9 stores until selected is0..8, return66. Zero is
 * admitted by the method and still normalizes an authored out-of-range field.
 * No native iteration truncation or alternate modulo formula is used. */
mc_inventory_status mc_inventory_player_scroll(mc_inventory_player *, int32_t wheel,
                                               mc_inventory_selection_error *);
/* Native access to the actual ix.c putfield in Minecraft.k at874. No new Java
 * method is claimed. The source caller supplies0..8; other native values are
 * rejected as explicit index policy before the field changes. */
mc_inventory_status mc_inventory_player_select_hotbar(mc_inventory_player *, int32_t slot,
                                                      mc_inventory_selection_error *);

#endif
