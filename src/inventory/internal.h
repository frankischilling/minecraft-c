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
    mc_container_slot_state state;
} inv_slot;

struct mc_inventory_crafting {
    inv_budget local_budget, *budget;
    mc_registry_side side;
    mc_container *handler;
    mc_item_stack **slots;
    int32_t width;
    size_t size;
    bool active, container_owned;
};

struct mc_inventory_result {
    inv_budget local_budget, *budget;
    mc_registry_side side;
    mc_item_stack **slots;
    bool container_owned;
};

struct mc_inventory_player {
    inv_budget local_budget;
    mc_registry_side side;
    mc_entity *player;
    mc_item_stack **main_slots, **armor_slots;
    mc_item_stack *cursor;
    int32_t selected;
    bool changed, active;
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
    mc_world_runtime *world;
    inv_list *snapshots, *slots, *listeners;
    inv_set *blocked_players;
    inv_slot *allocated_slots;
    bool active, constructing, constructed, local, inventory_bound;
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

#endif
