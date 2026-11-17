#ifndef MC_INVENTORY_DISPENSER_H
#define MC_INVENTORY_DISPENSER_H

#include "mc/inventory/errors.h"

typedef struct mc_entity mc_entity;
typedef struct mc_tile_entity mc_tile_entity;
typedef struct mc_inventory_dispenser mc_inventory_dispenser;

typedef struct mc_inventory_dispenser_state {
    mc_registry_side side;
    mc_tile_entity *tile; /* Borrowed actual AZ; this adapter owns one Tile occurrence. */
    size_t native_holds, allocation_bytes;
    bool active, terminal_sealed;
    const mc_inventory_error *cleanup; /* Separate native capture/cleanup failure, borrowed. */
    uintptr_t diagnostic_owner_id;     /* Native value witness, never an ownership token. */
} mc_inventory_dispenser_state;

/* Fixed actual AZ LW receiver, client only. The supplied live owner graph stays
 * exclusive. Native retirement does not dirty, close or alter Source fields.
 * Failed reached World children and their producers remain held until end_calls
 * or final native release. Outputs are disjoint from all live owner storage. */
mc_inventory_status mc_inventory_dispenser_from_tile(mc_registry_side, mc_tile_entity *,
                                                     const mc_inventory_limits *,
                                                     mc_inventory_dispenser **,
                                                     mc_inventory_error *);
mc_inventory_status mc_inventory_dispenser_retain(mc_inventory_dispenser *, mc_inventory_error *);
void mc_inventory_dispenser_release(mc_inventory_dispenser *);
mc_inventory_status mc_inventory_dispenser_retire(mc_inventory_dispenser **, mc_inventory_error *);
mc_inventory_status mc_inventory_dispenser_end_calls(mc_inventory_dispenser *,
                                                     mc_inventory_error *);
bool mc_inventory_dispenser_read_state(const mc_inventory_dispenser *,
                                       mc_inventory_dispenser_state *);
mc_inventory_status mc_inventory_dispenser_tile_borrow(mc_inventory_dispenser *, mc_tile_entity **,
                                                       mc_inventory_error *);
mc_inventory_status mc_inventory_dispenser_size(mc_inventory_dispenser *, int32_t *,
                                                mc_inventory_error *);
mc_inventory_status mc_inventory_dispenser_name(mc_inventory_dispenser *, const char **,
                                                mc_inventory_error *);
mc_inventory_status mc_inventory_dispenser_limit(mc_inventory_dispenser *, int32_t *,
                                                 mc_inventory_error *);
mc_inventory_status mc_inventory_dispenser_borrow(mc_inventory_dispenser *, int32_t,
                                                  const mc_item_stack **, mc_inventory_error *);
mc_inventory_status mc_inventory_dispenser_get(mc_inventory_dispenser *, int32_t, mc_item_stack **,
                                               mc_inventory_error *);
/* AZ set dirties after store/clamp. A nonempty split dirties after the actual
 * slot/count mutation, even for a zero or negative amount; an empty split does
 * not dirty. A failed dirty child preserves completed mutations and leaves the
 * return output unchanged. GP changed is a separate additional dirty call. */
mc_inventory_status mc_inventory_dispenser_set(mc_inventory_dispenser *, int32_t, mc_item_stack *,
                                               mc_inventory_error *);
mc_inventory_status mc_inventory_dispenser_split(mc_inventory_dispenser *, int32_t, int32_t,
                                                 mc_item_stack **, mc_inventory_error *);
mc_inventory_status mc_inventory_dispenser_changed(mc_inventory_dispenser *, mc_inventory_error *);
mc_inventory_status mc_inventory_dispenser_usable(mc_inventory_dispenser *, mc_entity *, bool *,
                                                  mc_inventory_error *);

/* End matching diagnostic/name borrows after actual end_calls, final release,
 * or consumption of the owning occurrence through which they were borrowed.
 * Save the id while that occurrence is live. Copies need the same explicit
 * join before producer retirement. No Source call or release occurs here;
 * Source flags, sites and scalar causes remain. The saved id is local to this
 * cleanup join and must not authorize a later reused allocation. */
void mc_inventory_dispenser_error_expire(mc_inventory_error *, uintptr_t ended_owner_id);

#endif
