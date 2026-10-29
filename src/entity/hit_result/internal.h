#ifndef MC_ENTITY_HIT_RESULT_INTERNAL_H
#define MC_ENTITY_HIT_RESULT_INTERNAL_H

#include "../classes/memory_internal.h"
#include "../entity_internal.h"
#include "mc/entity/hit_result.h"

struct mc_entity_hit_name {
    uint16_t units[6];
    size_t length;
};

struct mc_entity_hit_kind {
    mc_entity_classes *classes;
    mc_entity_hit_phase phase;
    const mc_entity_hit_name *name;
    int32_t ordinal;
};

typedef struct mc_entity_hit_kind_array {
    size_t length;
    const mc_entity_hit_kind *elements[2];
} mc_entity_hit_kind_array;

typedef struct mc_entity_hit_kind_state {
    mc_entity_hit_name names[2];
    mc_entity_hit_kind *candidates[2], *a, *b;
    mc_entity_hit_kind_array *pending_values, *values;
} mc_entity_hit_kind_state;

struct mc_entity_hit_result {
    mc_entity_classes *classes;
    mc_entity_hit_phase phase;
    size_t holds;
    bool active, retiring;
    mc_entity_hit_fields fields;
    mc_entity_hit_error construction_error;
};

void mc_entity_hit_error_clear(mc_entity_hit_error *error);
mc_entity_hit_status mc_entity_hit_fail(mc_entity_hit_error *error, mc_entity_hit_status status,
                                        const char *operation, bool native_policy,
                                        mc_entity_classes *classes, const char *method,
                                        const char *descriptor, bool entered, uint16_t pc);
mc_entity_hit_status mc_entity_hit_kind_ensure(mc_entity_classes *classes,
                                               mc_entity_hit_kind_error *error);
void mc_entity_hit_kind_dispose(mc_entity_classes *classes);

#endif
