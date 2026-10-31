#ifndef MC_ENTITY_PLAYER_MOUSE_FILTER_H
#define MC_ENTITY_PLAYER_MOUSE_FILTER_H

#include "mc/entity/player/player.h"

/* Actual client cu.a(FF)F on the canonical a/b/c fields. Each operation rounds
 * separately to binary32; Java comparisons leave NaN branches untaken. No
 * reset, normalization, clamp beyond the source branch, or settings copy is
 * added. Strict x86-64 floating admission precedes the first a store; native
 * argument/FP failure preserves all fields and out. Field/out storage must be
 * live, exclusive and disjoint. The owning Player/PX lifetime stays with its
 * existing caller; this direct source body owns no allocation or native hold. */
mc_entity_status mc_player_mouse_filter_apply(mc_player_mouse_filter_fields *fields, float input,
                                              float factor, float *out);

#endif
