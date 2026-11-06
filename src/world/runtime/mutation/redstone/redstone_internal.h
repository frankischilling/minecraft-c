#ifndef MC_WORLD_RUNTIME_MUTATION_REDSTONE_INTERNAL_H
#define MC_WORLD_RUNTIME_MUTATION_REDSTONE_INTERNAL_H

#include "../../../../block/registry/registry_internal.h"
#include "../mutation_internal.h"
#include "mc/core/java/java.h"

typedef enum mc_redstone_method {
    MC_REDSTONE_ADDED,
    MC_REDSTONE_REMOVED,
    MC_REDSTONE_NEIGHBOR,
    MC_REDSTONE_SUPPORT,
    MC_REDSTONE_DRAIN,
    MC_REDSTONE_UPDATE,
    MC_REDSTONE_NOTIFY,
    MC_REDSTONE_STRENGTH
} mc_redstone_method;

typedef struct mc_redstone_frame {
    mc_world_runtime *world;
    mc_block *block;
    mc_world_mutation_error *error;
    mc_redstone_method method;
} mc_redstone_frame;

mc_redstone_frame mc_redstone_frame_make(mc_world_runtime *, const mc_block *,
                                         mc_world_mutation_error *, mc_redstone_method);
void mc_redstone_at(const mc_redstone_frame *, unsigned int pc);
bool mc_redstone_done(const mc_redstone_frame *);
bool mc_redstone_fail(const mc_redstone_frame *, unsigned int pc, mc_world_storage_status,
                      const char *operation);
bool mc_redstone_nested(const mc_redstone_frame *, unsigned int pc,
                        const mc_world_mutation_error *);
bool mc_redstone_collection(const mc_redstone_frame *, unsigned int pc, mc_registry_status);
mc_world_mutation_error *mc_redstone_workspace(const mc_redstone_frame *, unsigned int pc);
bool mc_redstone_child(const mc_redstone_frame *, unsigned int pc, mc_world_mutation_error *, bool);
bool mc_redstone_remote(const mc_redstone_frame *, unsigned int pc, bool *out);
bool mc_redstone_value(const mc_redstone_frame *, unsigned int pc, int32_t x, int32_t y, int32_t z,
                       bool metadata, int32_t *out);
bool mc_redstone_solid(const mc_redstone_frame *, unsigned int pc, int32_t x, int32_t y, int32_t z,
                       bool *out);
bool mc_redstone_neighbors(const mc_redstone_frame *, unsigned int pc, int32_t x, int32_t y,
                           int32_t z);
bool mc_redstone_strength(const mc_redstone_frame *, unsigned int pc, int32_t x, int32_t y,
                          int32_t z, int32_t maximum, int32_t *out);
bool mc_redstone_notify(const mc_redstone_frame *, unsigned int pc, int32_t x, int32_t y,
                        int32_t z);
bool mc_redstone_drain(const mc_redstone_frame *, unsigned int pc, int32_t x, int32_t y, int32_t z);
bool mc_redstone_update(const mc_redstone_frame *, unsigned int pc, int32_t x, int32_t y, int32_t z,
                        int32_t incoming_x, int32_t incoming_y, int32_t incoming_z);

/* Source horizontal loop order: -X, +X, -Z, +Z; all arithmetic wraps. */
void mc_redstone_horizontal(int32_t x, int32_t z, unsigned int side, int32_t *out_x,
                            int32_t *out_z);

#endif
