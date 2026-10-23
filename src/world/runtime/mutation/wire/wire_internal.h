#ifndef MC_WORLD_RUNTIME_MUTATION_WIRE_INTERNAL_H
#define MC_WORLD_RUNTIME_MUTATION_WIRE_INTERNAL_H

#include "../../../../block/registry/registry_internal.h"
#include "../mutation_internal.h"
#include "mc/core/java/java.h"

typedef enum mc_wire_method {
    MC_WIRE_ADDED,
    MC_WIRE_REMOVED,
    MC_WIRE_NEIGHBOR,
    MC_WIRE_SUPPORT,
    MC_WIRE_DRAIN,
    MC_WIRE_UPDATE,
    MC_WIRE_NOTIFY,
    MC_WIRE_STRENGTH
} mc_wire_method;

typedef struct mc_wire_frame {
    mc_world_runtime *world;
    mc_block *block;
    mc_world_mutation_error *error;
    mc_wire_method method;
} mc_wire_frame;

mc_wire_frame mc_wire_frame_make(mc_world_runtime *, const mc_block *, mc_world_mutation_error *,
                                 mc_wire_method);
void mc_wire_at(const mc_wire_frame *, unsigned int pc);
bool mc_wire_done(const mc_wire_frame *);
bool mc_wire_fail(const mc_wire_frame *, unsigned int pc, mc_world_storage_status,
                  const char *operation);
bool mc_wire_nested(const mc_wire_frame *, unsigned int pc, const mc_world_mutation_error *);
bool mc_wire_collection(const mc_wire_frame *, unsigned int pc, mc_registry_status);
mc_world_mutation_error *mc_wire_workspace(const mc_wire_frame *, unsigned int pc);
bool mc_wire_child(const mc_wire_frame *, unsigned int pc, mc_world_mutation_error *, bool);
bool mc_wire_remote(const mc_wire_frame *, unsigned int pc, bool *out);
bool mc_wire_value(const mc_wire_frame *, unsigned int pc, int32_t x, int32_t y, int32_t z,
                   bool metadata, int32_t *out);
bool mc_wire_solid(const mc_wire_frame *, unsigned int pc, int32_t x, int32_t y, int32_t z,
                   bool *out);
bool mc_wire_neighbors(const mc_wire_frame *, unsigned int pc, int32_t x, int32_t y, int32_t z);
bool mc_wire_strength(const mc_wire_frame *, unsigned int pc, int32_t x, int32_t y, int32_t z,
                      int32_t maximum, int32_t *out);
bool mc_wire_notify(const mc_wire_frame *, unsigned int pc, int32_t x, int32_t y, int32_t z);
bool mc_wire_drain(const mc_wire_frame *, unsigned int pc, int32_t x, int32_t y, int32_t z);
bool mc_wire_update(const mc_wire_frame *, unsigned int pc, int32_t x, int32_t y, int32_t z,
                    int32_t incoming_x, int32_t incoming_y, int32_t incoming_z);

/* Source horizontal loop order: -X, +X, -Z, +Z; all arithmetic wraps. */
void mc_wire_horizontal(int32_t x, int32_t z, unsigned int side, int32_t *out_x, int32_t *out_z);

#endif
