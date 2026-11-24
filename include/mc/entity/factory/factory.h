#ifndef MC_ENTITY_FACTORY_FACTORY_H
#define MC_ENTITY_FACTORY_FACTORY_H

#include "mc/entity/arrow/construction.h"
#include "mc/entity/mob/construction.h"
#include "mc/entity/registry/registry.h"

typedef enum mc_entity_factory_status {
    MC_ENTITY_FACTORY_OK,
    MC_ENTITY_FACTORY_ARGUMENT,
    MC_ENTITY_FACTORY_LIMIT,
    MC_ENTITY_FACTORY_ALLOCATION_FAILED,
    MC_ENTITY_FACTORY_REGISTRY_FAILED,
    MC_ENTITY_FACTORY_CONSTRUCTOR_FAILED,
    MC_ENTITY_FACTORY_UNSUPPORTED_FP,
    MC_ENTITY_FACTORY_UNIMPLEMENTED
} mc_entity_factory_status;

typedef struct mc_entity_factory_error {
    mc_entity_factory_status status;
    const char *operation;
    unsigned int bytecode;
    bool native_policy, source_exception;
    const char *exception_class;
    /* Known reflection catch entering printStackTrace52, whose diagnostic body
     * remains an explicit frontier. This is not an escaping Throwable, printed
     * stack, or successfully returned NULL. The original child stays below. */
    const char *caught_exception_class;
    const mc_entity_class_literal *selected_class;
    mc_entity_registry_error registry;
    mc_entity_error construction;
    bool mob_selected, arrow_selected;
    mc_mob_error mob_call;
    mc_arrow_error arrow_call;
} mc_entity_factory_error;

/* Actual jc/ga.a(String,World), independent of the NBT overload. Caller keeps
 * the actual defining Entity/Registry namespaces and nullable World live and
 * exclusive. Registry is read only by a reached concrete dependency (not from
 * World); unsupported known constructors stop at40. Nullable UTF16 name is a
 * borrowed value span: {NULL,0}, empty and unknown keys all silently return
 * NULL. Equal-valued names select the same Class, never a cached Entity.
 *
 * EntityList initialization precedes the protected body; its real native
 * failure/latch is preserved. Supported actual constructors publish one held
 * READY result, without NBT reading or World-list insertion. Native failures
 * preserve *out and release only the factory's temporary Entity occurrence.
 * The abstract Mob reflection catch reaches the honest print52 frontier;
 * native limits and class-init Errors never become ordinary NULL success.
 * Error is a value; selected_class/child diagnostic borrows require the same
 * defining namespaces to remain live. No callback, hidden World inference or
 * asynchronous owner is introduced. NULL error is permitted; outputs/errors
 * must be disjoint from every input and live owner. */
mc_entity_factory_status mc_entity_factory_named(mc_entity_classes *,
                                                 mc_registry_classes *defining_registry,
                                                 mc_nbt_string nullable_name,
                                                 mc_world_runtime *nullable_world, mc_entity **out,
                                                 mc_entity_factory_error *);

#endif
