#ifndef MC_WORLD_RUNTIME_NIGHTMARE_CLASSES_INTERNAL_H
#define MC_WORLD_RUNTIME_NIGHTMARE_CLASSES_INTERNAL_H

#include "mc/core/java/int_set.h"
#include "mc/world/storage/status.h"

typedef struct mc_entity_class_literal mc_entity_class_literal;
typedef struct mc_world_runtime_classes mc_world_runtime_classes;

typedef enum mc_world_nightmare_class_kind {
    MC_WORLD_NIGHTMARE_CLASS_NULL,
    MC_WORLD_NIGHTMARE_CLASS_ENTITY,
    MC_WORLD_NIGHTMARE_CLASS_WORLD
} mc_world_nightmare_class_kind;

typedef struct mc_world_nightmare_class_value {
    mc_world_nightmare_class_kind kind;

    union {
        const mc_entity_class_literal *entity;
        const mc_world_runtime_classes *world;
    } value;
} mc_world_nightmare_class_value;

typedef struct mc_world_nightmare_class_array {
    size_t length;
    mc_world_nightmare_class_value *items;
} mc_world_nightmare_class_array;

typedef enum mc_world_nightmare_class_phase {
    MC_WORLD_NIGHTMARE_CLASS_COLD,
    MC_WORLD_NIGHTMARE_CLASS_ENTERED,
    MC_WORLD_NIGHTMARE_CLASS_READY,
    MC_WORLD_NIGHTMARE_CLASS_FAILED
} mc_world_nightmare_class_phase;

typedef struct mc_world_nightmare_class_error {
    mc_world_storage_status status;
    const char *operation;
    unsigned int pc;
    int entity; /* Exact mc_entity_status value, without an Entity header dependency. */
    mc_java_int_set_status set;
} mc_world_nightmare_class_error;

typedef struct mc_world_nightmare_namespace {
    mc_world_nightmare_class_phase phase;
    unsigned int pc;
    mc_world_nightmare_class_array *a, *candidate;
    mc_world_nightmare_class_array default_array;
    mc_world_nightmare_class_value defaults[3];
    mc_world_nightmare_class_error failure;
} mc_world_nightmare_namespace;

/* The WorldClasses owner embeds one `mc_world_nightmare_namespace nightmare`.
 * CQ.b uses its existing spawning_chunks Set. Reached static initialization
 * constructs b before publishing the separately built three-cell Class[] a.
 * No target Mob initializer, default constructor or Random call is performed.
 * Failed/entered prefixes retain actual cells; ensure never replays them. */
mc_world_storage_status mc_world_nightmare_classes_ensure(mc_world_runtime_classes *,
                                                          mc_world_nightmare_class_error *);

#endif
