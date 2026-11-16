#ifndef MC_INVENTORY_INTERNAL_H
#define MC_INVENTORY_INTERNAL_H

#include "mc/core/java/java.h"
#include "mc/inventory/container.h"

typedef struct inv_budget {
    size_t used, limit, max_slots;
} inv_budget;

typedef struct inv_list {
    void **values;
    size_t size, capacity;
} inv_list;

typedef struct inv_set {
    void **members;
    size_t size, capacity;
} inv_set;

typedef struct inv_slot {
    struct inv_slot *allocated_next;
    mc_container *owner; /* Native owner, independent of every source Slot field. */
    mc_container_slot_state state;
} inv_slot;

struct mc_inventory_crafting {
    inv_budget local_budget, *budget;
    mc_registry_side side;
    mc_container *handler;
    mc_item_stack **slots;
    int32_t width;
    size_t size;
    bool active, container_owned, terminal_sealed;
};

struct mc_inventory_result {
    inv_budget local_budget, *budget;
    mc_registry_side side;
    mc_item_stack **slots;
    bool container_owned, terminal_sealed;
};

struct mc_inventory_player {
    inv_budget local_budget;
    mc_registry_side side;
    mc_entity *player;
    mc_item_stack **main_slots, **armor_slots;
    mc_item_stack *cursor;
    int32_t selected;
    bool changed, active, terminal_sealed;
    size_t containers;
};

struct mc_container {
    inv_budget budget;
    mc_registry_side side;
    mc_container_kind kind;
    mc_registry_classes *classes;
    mc_inventory_player *inventory;
    mc_inventory_crafting *crafting;
    mc_inventory_result *result;
    mc_inventory_chest *chest;
    mc_inventory_furnace *furnace;
    int32_t furnace_cook, furnace_burn, furnace_fuel;
    int32_t chest_rows;
    mc_world_runtime *world;
    inv_list *snapshots, *slots, *listeners;
    inv_set *blocked_players;
    inv_slot *allocated_slots;
    bool active, constructing, constructed, local, inventory_bound;
    bool constructor_entered, constructor_failed;
    int32_t constructor_pc;
    bool interaction_active, terminal_sealed;
    size_t interaction_holds;
    int32_t window_id, x, y, z;
    int16_t transaction;
};

const char *inv_class(mc_registry_side side, const char *client, const char *server);
bool inv_side_valid(mc_registry_side side);
mc_inventory_error inv_error(mc_inventory_status status, const char *operation,
                             mc_registry_side side, const char *client, const char *server,
                             const char *method, const char *descriptor, int32_t pc);
void inv_frame(mc_inventory_error *error, mc_registry_side side, const char *client,
               const char *server, const char *method, const char *descriptor, int32_t pc);
mc_inventory_status inv_finish(mc_inventory_error *out, const mc_inventory_error *error);
mc_inventory_status inv_stack(mc_inventory_error *error, mc_item_stack_status status,
                              const char *operation);
bool inv_limits(const mc_inventory_limits *options, inv_budget *out);
void *inv_alloc(inv_budget *budget, size_t bytes, mc_inventory_error *error);
void inv_free(inv_budget *budget, void *data);
bool inv_allocation_bytes(const void *data, size_t payload, size_t *out);
void inv_player_free(mc_inventory_player *inventory);
void inv_container_free(mc_container *container);
mc_inventory_status inv_hold(mc_item_stack *value, mc_inventory_error *error);
mc_inventory_status inv_publish_hold(const mc_item_stack *value, mc_item_stack **out,
                                     mc_inventory_error *error);
mc_inventory_status inv_crafting_new(mc_registry_side side, mc_container *handler, int32_t width,
                                     int32_t height, inv_budget *budget,
                                     mc_inventory_crafting **out, mc_inventory_error *error);
void inv_crafting_free(mc_inventory_crafting *grid);
mc_inventory_status inv_result_new(mc_registry_side side, inv_budget *budget,
                                   mc_inventory_result **out, mc_inventory_error *error);
void inv_result_free(mc_inventory_result *result);
mc_inventory_status inv_container_notify(mc_container *container, mc_inventory_error *error);
mc_inventory_status inv_furnace_terminal_admit(const mc_inventory_furnace *, bool,
                                               mc_inventory_error *);
void inv_furnace_terminal_seal(mc_inventory_furnace *);

typedef struct inv_furnace_leaf {
    void (*release)(void *);
    void (*end_calls)(void *);
    mc_inventory_status (*size)(void *, int32_t *, mc_inventory_error *);
    mc_inventory_status (*borrow)(void *, int32_t, const mc_item_stack **, mc_inventory_error *);
    mc_inventory_status (*set)(void *, int32_t, mc_item_stack *, mc_inventory_error *);
    mc_inventory_status (*split)(void *, int32_t, int32_t, mc_item_stack **, mc_inventory_error *);
    mc_inventory_status (*changed)(void *, mc_inventory_error *);
    mc_inventory_status (*usable)(void *, mc_entity *, bool *, mc_inventory_error *);
    mc_inventory_status (*burning)(void *, bool *, mc_inventory_error *);
    mc_inventory_status (*cook_scale)(void *, int32_t, int32_t *, mc_inventory_error *);
    mc_inventory_status (*burn_scale)(void *, int32_t, int32_t *, mc_inventory_error *);
    mc_inventory_status (*counters)(void *, int32_t *, int32_t *, int32_t *, mc_inventory_error *);
    mc_inventory_status (*progress)(void *, int32_t, int32_t, mc_inventory_error *);
} inv_furnace_leaf;

mc_inventory_status inv_furnace_leaf_create(mc_registry_side, void *, mc_tile_entity *,
                                            const inv_furnace_leaf *, const mc_inventory_limits *,
                                            mc_inventory_furnace **, mc_inventory_error *);

typedef struct inv_furnace_node_view {
    const mc_inventory_furnace *owner;
    mc_tile_entity *tile;
    size_t native_holds, allocation_bytes;
    bool active, terminal_sealed;
} inv_furnace_node_view;

mc_inventory_status inv_furnace_node_get(const mc_inventory_furnace *, inv_furnace_node_view *,
                                         mc_inventory_error *);
mc_inventory_status inv_furnace_counters(mc_inventory_furnace *, int32_t *, int32_t *, int32_t *,
                                         mc_inventory_error *);
mc_inventory_status inv_furnace_progress(mc_inventory_furnace *, int32_t, int32_t,
                                         mc_inventory_error *);

#endif
