#ifndef MC_ENTITY_ITEM_TICK_INTERNAL_H
#define MC_ENTITY_ITEM_TICK_INTERNAL_H

#include "../living/travel_math_internal.h"
#include "../tick/tick_internal.h"
#include "mc/entity/item/tick.h"

typedef struct mc_item_tick_scope {
    mc_entity *entity;
    mc_registry_classes *registry;
    const mc_item_entity_tick_limits *limits;
    bool registry_held;
} mc_item_tick_scope;

bool mc_item_tick_fail(mc_item_entity_tick_error *, mc_entity_status, const char *);
bool mc_item_tick_fp(mc_item_entity_tick_error *);
bool mc_item_tick_world(mc_item_tick_scope *, mc_world_runtime *, mc_item_entity_tick_error *);
bool mc_item_tick_code(mc_item_tick_scope *, mc_item_entity_tick_error *);
bool mc_item_tick_registry(mc_item_entity_tick_error *, mc_registry_status);
bool mc_item_tick_world_result(mc_item_entity_tick_error *, mc_world_storage_status, const char *);
void mc_item_tick_child(mc_item_entity_tick_error *, mc_item_entity_tick_child);
bool mc_item_tick_pushout(mc_item_tick_scope *, double, double, double,
                          mc_item_entity_tick_error *);

static inline double mc_item_tick_widen(float f) {
    return _mm_cvtsd_f64(_mm_cvtss_sd(_mm_setzero_pd(), _mm_set_ss(f)));
}

static inline double mc_item_tick_div(double a, double b) {
    return _mm_cvtsd_f64(_mm_div_sd(_mm_set_sd(a), _mm_set_sd(b)));
}

static inline float mc_item_tick_negate(float f) {
    uint32_t bits;
    memcpy(&bits, &f, sizeof(bits));
    bits ^= UINT32_C(0x80000000);
    memcpy(&f, &bits, sizeof(f));
    return f;
}

#endif
