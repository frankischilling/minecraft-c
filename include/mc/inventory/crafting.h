#ifndef MC_INVENTORY_CRAFTING_H
#define MC_INVENTORY_CRAFTING_H

#include "mc/inventory/errors.h"

typedef struct mc_inventory_crafting mc_inventory_crafting;
typedef struct mc_container mc_container;

/* A NULL handler is a real source constructor input. Mutation stores first,
 * then raises its source null failure at notification. This is not a working
 * container substitute. A bound handler must outlive its grid. */
mc_inventory_status mc_inventory_crafting_create(mc_registry_side side, mc_container *handler,
                                                 int32_t width, int32_t height,
                                                 const mc_inventory_limits *limits,
                                                 mc_inventory_crafting **out,
                                                 mc_inventory_error *error);
mc_inventory_status mc_inventory_crafting_retire(mc_inventory_crafting **grid,
                                                 mc_inventory_error *error);
mc_inventory_status mc_inventory_crafting_size(const mc_inventory_crafting *grid, int32_t *out,
                                               mc_inventory_error *error);
mc_inventory_status mc_inventory_crafting_width(const mc_inventory_crafting *grid, int32_t *out,
                                                mc_inventory_error *error);
mc_inventory_status mc_inventory_crafting_borrow_slot(const mc_inventory_crafting *grid,
                                                      int32_t slot, const mc_item_stack **out,
                                                      mc_inventory_error *error);
mc_inventory_status mc_inventory_crafting_borrow_xy(const mc_inventory_crafting *grid, int32_t x,
                                                    int32_t y, const mc_item_stack **out,
                                                    mc_inventory_error *error);
/* Nonnull public get results add an independent native hold. */
mc_inventory_status mc_inventory_crafting_get(mc_inventory_crafting *grid, int32_t slot,
                                              mc_item_stack **out, mc_inventory_error *error);
mc_inventory_status mc_inventory_crafting_get_xy(mc_inventory_crafting *grid, int32_t x, int32_t y,
                                                 mc_item_stack **out, mc_inventory_error *error);
mc_inventory_status mc_inventory_crafting_set(mc_inventory_crafting *grid, int32_t slot,
                                              mc_item_stack *value, mc_inventory_error *error);
mc_inventory_status mc_inventory_crafting_split(mc_inventory_crafting *grid, int32_t slot,
                                                int32_t amount, mc_item_stack **out,
                                                mc_inventory_error *error);
mc_inventory_status mc_inventory_crafting_name(const mc_inventory_crafting *grid, const char **out,
                                               mc_inventory_error *error);
mc_inventory_status mc_inventory_crafting_limit(const mc_inventory_crafting *grid, int32_t *out,
                                                mc_inventory_error *error);
mc_inventory_status mc_inventory_crafting_changed(mc_inventory_crafting *grid,
                                                  mc_inventory_error *error);
mc_inventory_status mc_inventory_crafting_usable(const mc_inventory_crafting *grid,
                                                 const void *player, bool *out,
                                                 mc_inventory_error *error);

#endif
