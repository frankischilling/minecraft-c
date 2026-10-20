#ifndef MC_WORLD_RUNTIME_ENTITY_PUBLICATION_INTERNAL_H
#define MC_WORLD_RUNTIME_ENTITY_PUBLICATION_INTERNAL_H

#include "runtime_internal.h"

/* Validate native ownership against the actual constructor-created b List;
 * never reinterpret an unknown nonempty reference list as Entity ownership. */
bool mc_world_runtime_entity_list_valid(const mc_world_runtime *world);
/* Infallible after complete quiescent admission. No source callback, World
 * binding rewrite, serialization, dirty/q or Entity membership store occurs. */
void mc_world_runtime_entity_list_release(mc_world_runtime *world);

#endif
