#ifndef MC_ENTITY_MINECART_MINECART_H
#define MC_ENTITY_MINECART_MINECART_H

#include "mc/entity/entity.h"
#include "mc/entity/minecart/classes.h"

typedef struct mc_minecart_stack_array mc_minecart_stack_array;

/* Own yl/pr fields, separate from shadowed Entity and Item/Falling fields.
 * The client-only tail represents source fields only for the client owner. */
typedef struct mc_minecart_entity_fields {
    mc_minecart_stack_array *h;
    int32_t a, b, c;
    bool private_i;
    int32_t d, e;
    double f, g;
    int32_t private_k;
    double private_l, private_m, private_n, private_o, private_p;

    struct {
        double private_q, private_r, private_s;
    } client_only;
} mc_minecart_entity_fields;

typedef struct mc_minecart_entity_snapshot {
    mc_entity_source source;
    const mc_entity_class_literal *class_literal;
    mc_entity_phase phase;
    uint16_t world_bytecode, bytecode;
    mc_minecart_entity_fields fields;
    size_t inventory_length;
    mc_item_stack *inventory[36]; /* Borrowed actual 27/36 entries; unused tail is NULL. */
} mc_minecart_entity_snapshot;

/* Allocate RAW with canonical MINECART identity from this exact owner. Active
 * use initializes Entity, then the complete Minecart j graph, before native
 * instance allocation. Allocation does not consume an Entity ID. */
mc_entity_status mc_minecart_entity_allocate(mc_entity_classes *classes, mc_entity **out,
                                             mc_entity_error *error);

/* Matching-kind RAW admission, nullable World, same-profile check and failure
 * lifetime follow the existing concrete Entity constructors. Caller holds the
 * Entity and any nonnull World; the reached base store takes its counted World
 * binding. Strict java.h floating arithmetic applies. No insertion occurs.
 * World-only returns after size/offset and preserves the base-created bounds.
 * Full construction positions the same bounds at x,y+(double)offset,z; stores
 * raw previous coordinates and unrestricted signed type. No Item Math draws,
 * Stack construction, inventory invocation or extra watcher record occurs. */
mc_entity_status mc_minecart_entity_construct_world(mc_entity *raw, mc_world_runtime *world,
                                                    mc_entity_error *error);
mc_entity_status mc_minecart_entity_construct(mc_entity *raw, mc_world_runtime *world, double x,
                                              double y, double z, int32_t type,
                                              mc_entity_error *error);

/* All phases, exact canonical Minecart kind. Get the existing generic Entity
 * snapshot separately for actual base/bounds/Random/watcher references.
 * Actual h may be NULL after a partial constructor, length36 after construction,
 * or length27 after semantic NBT replacement. No initialization, allocation,
 * source invocation or hold transfer. */
mc_entity_status mc_minecart_entity_get_snapshot(const mc_entity *entity,
                                                 mc_minecart_entity_snapshot *out);

/* Snapshots borrow live owners exclusively and expire on mutation/final
 * release. Outputs/errors/owners must be disjoint. Native rejection preserves
 * the existing phase and outputs; reached failure retains FAILED_CONSTRUCTION and its actual
 * prefix. release/classes_free remain the existing lifetime operations.
 * Inventory operations and their occurrence transfers are in inventory.h. */

#endif
