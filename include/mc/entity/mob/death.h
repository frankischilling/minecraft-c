#ifndef MC_ENTITY_MOB_DEATH_H
#define MC_ENTITY_MOB_DEATH_H

#include "mc/block/registry/registry.h"
#include "mc/entity/classes/classes.h"
#include "mc/entity/errors.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct mc_entity_drop_error mc_entity_drop_error;
struct mc_entity;

typedef struct mc_mob_death_site {
    const char *class_name, *method, *descriptor;
    int32_t pc;
} mc_mob_death_site;

/* Passive reached-source projection, never a native owning occurrence.
 * Integer identities are comparison-only. drop_error borrows the failed
 * generic drop call/admission record owned by the SAME actor's pending death
 * work; it expires when checked effect cleanup consumes that call. receiver
 * borrows the work's actual self-held actor only while pending. Cleanup
 * invalidates these borrows in every earlier value copy. Completed source
 * returns clear them while retaining scalar/identity observations.
 * Registry Strings and diagnostic names keep their existing owner lifetimes. */
typedef struct mc_mob_death_state {
    mc_entity_class_kind kind;
    bool entered, pending, active, returned;
    mc_entity_status status, cleanup_status;
    const char *operation, *cleanup_operation;
    bool native_policy, source_exception;
    const char *exception_class;
    mc_mob_death_site site, child;
    bool receiver_held, source_hold_acquired, source_hold_released, registry_held;
    struct mc_entity *receiver; /* Borrowed only while pending; never an owning occurrence. */
    uintptr_t receiver_identity;
    uintptr_t source_identity, gate_world_identity, status_world_identity;
    bool score_callback_entered, score_callback_returned;
    bool kill_callback_entered, kill_callback_returned;
    int32_t score;
    bool ah_stored, world_b_read, world_remote;
    bool q_entered, q_returned, status_entered, status_returned;
    uint16_t death_pc, q_pc, j_pc, drop_wrapper_pc, watcher_pc;
    bool sheared_read, sheared, color_read;
    int32_t color, drop_id, quantity, index;
    bool quantity_attempted, quantity_returned;
    bool stack_allocated, stack_constructed;
    uintptr_t sheep_stack_identity;
    mc_item_stack_status stack_status;
    uint8_t drops_attempted, drops_returned, drops_retired;
    uintptr_t items[2], stacks[2], constructor_worlds[2], publication_worlds[2];
    bool inserted[2];
    mc_registry_error registry;
    const mc_entity_drop_error *drop_error;
} mc_mob_death_state;

#endif
