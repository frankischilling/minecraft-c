#ifndef MC_ENTITY_PLAYER_SLEEP_INTERNAL_H
#define MC_ENTITY_PLAYER_SLEEP_INTERNAL_H

#include "../classes/memory_internal.h"
#include "mc/entity/player/sleep.h"

struct mc_player_sleep_name {
    uint16_t units[17];
    size_t length;
};

struct mc_player_sleep_result {
    mc_entity_classes *classes;
    mc_entity_class_phase phase;
    const mc_player_sleep_name *name;
    int32_t ordinal;
};

typedef struct mc_player_sleep_values {
    size_t length;
    const mc_player_sleep_result *elements[5];
} mc_player_sleep_values;

typedef struct mc_player_sleep_result_class {
    mc_player_sleep_name names[5];
    mc_player_sleep_result *candidates[5], *a, *b, *c, *d, *e;
    mc_player_sleep_values *values;
    unsigned int last_pc, last_store_pc;
} mc_player_sleep_result_class;

void mc_player_sleep_result_dispose(mc_entity_classes *);

#endif
