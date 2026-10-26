#ifndef MC_BLOCK_REGISTRY_STEP_SOUND_H
#define MC_BLOCK_REGISTRY_STEP_SOUND_H
#include "mc/block/registry/runtime.h"

typedef struct mc_step_sound_name_owner mc_step_sound_name_owner;
#define MC_STEP_SOUND_MAX_TEXT_UNITS 1048576u

typedef struct mc_step_sound_name {
    const mc_registry_string *string;
    mc_registry_classes *classes;
    mc_step_sound_name_owner *owner;
} mc_step_sound_name;

/* Actual ct/bu methods b,c,a,d for the complete base/Glass/Sand class family.
 * No Block/class initialization occurs. a's Glass/Sand overrides return their
 * actual interned literals; base a and every d create a fresh String identity.
 * Nullable a field appends literal null. Null sound fails at invocation.
 * sound belongs to the supplied defining Registry, whose dependencies remain
 * live/exclusive. Dynamic names are bounded to MAX_TEXT_UNITS; a native limit
 * failure preserves out and releases this call's temporary class borrow. */
mc_registry_status mc_registry_step_sound_volume(const mc_block_sound *, float *,
                                                 mc_registry_error *);
mc_registry_status mc_registry_step_sound_pitch(const mc_block_sound *, float *,
                                                mc_registry_error *);
mc_registry_status mc_registry_step_sound_name(mc_registry_classes *, const mc_block_sound *,
                                               bool method_a, mc_step_sound_name *,
                                               mc_registry_error *);

/* Each noncopyable result owns one class borrow. Base names additionally own
 * one managed String hold; payload and ledger record retire after the last
 * hold. Retain from the actual returned String permits a WorldAccess receiver
 * to retain that same identity beyond the synchronous sound call. Only names
 * created by this exact Registry/method family are admitted. Consume/free the
 * caller's previous result before successful replacement. Fresh result/error
 * storage must be disjoint from owners and inputs. No callbacks or class use. */
mc_registry_status mc_registry_step_sound_name_retain(mc_registry_classes *,
                                                      const mc_registry_string *,
                                                      mc_step_sound_name *, mc_registry_error *);
mc_registry_status mc_registry_step_sound_name_release(mc_step_sound_name *, mc_registry_error *);
#endif
