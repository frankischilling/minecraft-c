#ifndef MC_ENTITY_REGISTRY_REGISTRY_INTERNAL_H
#define MC_ENTITY_REGISTRY_REGISTRY_INTERNAL_H

#include "mc/entity/registry/registry.h"

typedef struct mc_entity_registry_record {
    mc_entity_class_kind kind;
    int32_t id;
    mc_nbt_string name;
    unsigned int client_pc, server_pc;
} mc_entity_registry_record;

typedef struct mc_entity_registry_node {
    const mc_entity_class_literal *type;
    const mc_entity_registry_record *record;
    struct mc_entity_registry_node *next;
} mc_entity_registry_node;

typedef struct mc_entity_registry_map {
    mc_entity_registry_node *head, *tail;
    size_t count;
} mc_entity_registry_map;

/* Infallible native class-owner destruction after all borrower/entity checks.
 * Frees actual allocated map headers/nodes and balances their direct ledger.
 * No source class reset, Entity release, Registry callback or serialization. */
void mc_entity_registry_dispose(mc_entity_classes *classes);

#endif
