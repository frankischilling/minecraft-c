#ifndef MC_ENTITY_REGISTRY_REGISTRY_H
#define MC_ENTITY_REGISTRY_REGISTRY_H

#include "mc/entity/classes/classes.h"
#include "mc/storage/nbt/nbt.h"

typedef enum mc_entity_registry_status {
    MC_ENTITY_REGISTRY_OK = 0,
    MC_ENTITY_REGISTRY_INVALID_ARGUMENT,
    MC_ENTITY_REGISTRY_LIMIT,
    MC_ENTITY_REGISTRY_ALLOCATION_FAILED,
    MC_ENTITY_REGISTRY_BUSY,
    MC_ENTITY_REGISTRY_CLASS_FAILED,
    MC_ENTITY_REGISTRY_STATE_ERROR
} mc_entity_registry_status;

typedef struct mc_entity_registry_error {
    mc_entity_registry_status status, cause;
    const char *operation;
    unsigned int initializer_bytecode, registration_bytecode;
    mc_entity_status class_cause;
} mc_entity_registry_error;

typedef struct mc_entity_registry_view {
    mc_entity_class_phase phase;
    const void *maps[4];
    size_t counts[4];
    mc_entity_registry_error first_failure;
} mc_entity_registry_view;

/* The actual jc/ga four-map initialization on this existing class owner.
 * Canonical Class literals are passive and shared with an explicitly bound
 * Registry. Registration does not initialize/construct an Entity or draw RNG.
 * Each reached map publication/put survives a later native failure. Failure
 * latches once; a later active use returns CLASS_FAILED with the original
 * complete cause/positions, without retrying or resetting earlier classes.
 * Native linked maps and value-key UTF16 views are not Java HashMap layouts,
 * String/Integer object identities or allocation/OOME equivalence. */
mc_entity_registry_status mc_entity_classes_initialize_registry(mc_entity_classes *classes,
                                                                mc_entity_registry_error *error);
/* Read-only inspection performs no active use. Map identities and names are
 * borrowed until class-owner destruction; outputs must be disjoint from every
 * owner/input. Names are immutable UTF16 values, not fabricated String objects. */
mc_entity_registry_status mc_entity_registry_inspect(const mc_entity_classes *classes,
                                                     mc_entity_registry_view *out);
mc_entity_registry_status mc_entity_registry_named_class(mc_entity_classes *classes,
                                                         mc_nbt_string name,
                                                         const mc_entity_class_literal **out,
                                                         mc_entity_registry_error *error);
mc_entity_registry_status mc_entity_registry_class_name(mc_entity_classes *classes,
                                                        const mc_entity_class_literal *type,
                                                        mc_nbt_string *out,
                                                        mc_entity_registry_error *error);
/* Native metadata lookup of map c; this is not a server numeric factory. */
mc_entity_registry_status mc_entity_registry_numeric_class(mc_entity_classes *classes, int32_t id,
                                                           const mc_entity_class_literal **out,
                                                           mc_entity_registry_error *error);

#endif
