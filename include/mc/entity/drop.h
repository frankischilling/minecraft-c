#ifndef MC_ENTITY_DROP_H
#define MC_ENTITY_DROP_H

#include "mc/block/registry/registry.h"
#include "mc/entity/entity.h"

typedef struct mc_entity_drop_call mc_entity_drop_call;
typedef struct mc_world_entity_error mc_world_entity_error;

typedef struct mc_entity_drop_site {
    const char *class_name, *method, *descriptor;
    unsigned int pc;
} mc_entity_drop_site;

/* Value receipt only. Publication and all other borrowed views expire when
 * the retained call successfully retires. Copying an error acquires no owner. */
typedef struct mc_entity_drop_error {
    mc_entity_status status;
    const char *operation, *exception_class;
    mc_entity_drop_site site, child;
    unsigned int numeric_pc, stack_pc;
    bool native_policy, source_exception;
    mc_item_stack_status stack;
    mc_entity_error entity;
    mc_registry_error registry;
    const mc_world_entity_error *publication;
} mc_entity_drop_error;

typedef struct mc_entity_drop_state {
    mc_entity *actor, *item;
    mc_item_stack *stack;
    mc_world_runtime *constructor_world, *publication_world;
    int32_t id, count;
    float offset;
    double x, y, z;
    bool numeric, active, returned;
    bool stack_allocated, stack_constructed, item_allocated, item_constructed;
    bool pickup_delay_stored, publication_entered, publication_returned, inserted;
    unsigned int numeric_pc, stack_pc;
    const mc_entity_drop_error *error, *cleanup;
} mc_entity_drop_state;

/* Actual client SN.a(int,int,float) and SN.a(Stack,float). The numeric entry
 * allocates RAW Stack before constructing id/count/metadata0; callers of the
 * allocation-free SN.b(int,int) wrapper record its invoke4 separately and pass
 * offset+0. The Stack entry preserves its exact nullable RAW/READY/failed owner
 * without reading fields. A NULL invocation receiver fails before source work.
 * The current closed receiver set is client FallingSand/Sheep/Pig/Cow/Chicken;
 * other virtual dispatch is refused as native admission, never approximated.
 *
 * Output is mandatory, starts NULL and stays live/disjoint through synchronous
 * callbacks. Admission failure preserves it. Once admitted, the published call
 * owns the exact actor, Registry, Stack, RAW/constructed Item, independently
 * captured constructor/publication Worlds and reached producer receipt. Source
 * failures keep that same call. No inventory, remote gate, statistic or launch
 * velocity rewrite is introduced; ordinary false publication still returns
 * the Item. Constructor arithmetic includes Y+offset even when offset is +0.
 *
 * Keep input owners and their dependency graphs live/exclusive. Read-only
 * inspect is permitted during a callback; retire returns BUSY while active.
 * Checked retirement consumes only this call's native occurrences, never
 * removes published actors, restores fields or replays a source operation.
 * A refused retirement preserves the call and remaining slots for retry. */
mc_entity_status mc_entity_drop_numeric_begin(mc_registry_classes *, mc_entity *actor, int32_t id,
                                              int32_t count, float offset,
                                              mc_entity_drop_call **out, mc_entity_drop_error *);
mc_entity_status mc_entity_drop_stack_begin(mc_registry_classes *, mc_entity *actor,
                                            mc_item_stack *, float offset,
                                            mc_entity_drop_call **out, mc_entity_drop_error *);
bool mc_entity_drop_inspect(const mc_entity_drop_call *, mc_entity_drop_state *);
mc_entity_status mc_entity_drop_retire(mc_entity_drop_call **, mc_entity_drop_error *);

#endif
