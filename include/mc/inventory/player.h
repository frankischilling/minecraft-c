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

#endif
