#ifndef MC_WORLD_ENTITY_TICK_INTERNAL_H
#define MC_WORLD_ENTITY_TICK_INTERNAL_H
#include "../../entity/entity_internal.h"
#include "../../entity/player/player_internal.h"
#include "../chunk/object_internal.h"
#include "../storage/objects_internal.h"
#include "chunk_provider_internal.h"
#include "construction_internal.h"
#include "entity_tick_math_internal.h"
#include "mc/core/java/java.h"
#include "mc/core/math/math.h"
#include "mc/world/runtime/entities.h"
#include "mc/world/runtime/entity_tick.h"
#include "presence_internal.h"
#include "runtime_internal.h"
#include "world_access_internal.h"
#include <stdlib.h>

typedef struct wt_scope {
    mc_world_runtime *world;
    const mc_world_entity_tick_context *context;
    mc_world_entity_tick_error error;
    mc_entity **parents;
    size_t parent_count, parent_capacity, temporary_bytes;
} wt_scope;

bool wt_fail(wt_scope *, mc_world_storage_status, const char *);
bool wt_cause(wt_scope *, mc_world_storage_error);
void wt_at(wt_scope *, const char *, unsigned int);
bool wt_fp(wt_scope *, unsigned int);
bool wt_work(wt_scope *, unsigned int);
void *wt_allocate(wt_scope *, size_t, unsigned int);
void wt_free(wt_scope *, void *, size_t);
mc_world_storage_status wt_entity_status(mc_entity_status);
bool wt_hold(wt_scope *, mc_entity *, unsigned int);
bool wt_relationship(wt_scope *, mc_entity *, mc_entity *, unsigned int, mc_entity **);
bool wt_get(wt_scope *, int32_t, int32_t, unsigned int, mc_chunk_state **);
bool wt_present(wt_scope *, int32_t, int32_t, unsigned int, bool *);
bool wt_chunk_remove(wt_scope *, mc_chunk_state *, mc_entity *, bool, int32_t, unsigned int);
bool wt_chunk_add(wt_scope *, mc_chunk_state *, mc_entity *, unsigned int);
bool wt_tick(wt_scope *, mc_entity *, unsigned int);
bool wt_riding(wt_scope *, mc_entity *, unsigned int);
bool wt_update(wt_scope *, mc_entity *, bool, bool);
bool wt_loops(wt_scope *);
bool wt_observe_removed(wt_scope *, mc_entity *, unsigned int);

bool wt_list_size(wt_scope *, mc_world_reference_list *, unsigned int, size_t *);
bool wt_list_get(wt_scope *, mc_world_reference_list *, int32_t, unsigned int, unsigned int,
                 mc_entity **);
bool wt_list_remove(wt_scope *, mc_world_reference_list *, int32_t, unsigned int);
bool wt_list_clear(wt_scope *, mc_world_reference_list *, unsigned int);
bool wt_list_remove_all(wt_scope *, mc_world_reference_list *, mc_world_reference_list *,
                        unsigned int);
bool wt_tile_active(wt_scope *);
bool wt_spawner(wt_scope *, mc_tile_entity *);
bool wt_nearest(wt_scope *, mc_world_runtime *, double, double, double, double, mc_entity **);
#endif
