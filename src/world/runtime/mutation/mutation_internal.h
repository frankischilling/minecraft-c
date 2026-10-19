#ifndef MC_WORLD_RUNTIME_MUTATION_INTERNAL_H
#define MC_WORLD_RUNTIME_MUTATION_INTERNAL_H

#include "../runtime_internal.h"
#include "mc/world/chunk/object.h"
#include "mc/world/runtime/mutations.h"

mc_world_mutation_error mc_world_mutation_empty_error(void);
bool mc_world_mutation_fail(mc_world_runtime *world, mc_world_mutation_error *error,
                            mc_world_storage_status status, const char *operation);
bool mc_world_mutation_registry_fail(mc_world_runtime *world, mc_world_mutation_error *error,
                                     const mc_registry_error *cause, const char *operation);

/* Internal concrete bodies: enclosing caller holds actual Chunk.d and leases
 * its exact store state. No additional preflight, holds or owner copies here.
 * Source fields remain late; caller sets the enclosing invocation PC. */
bool mc_world_mutation_chunk_block(mc_world_chunk *chunk, int32_t x, int32_t y, int32_t z,
                                   int32_t id, bool *out, mc_world_mutation_error *error);
bool mc_world_mutation_chunk_block_metadata(mc_world_chunk *chunk, int32_t x, int32_t y, int32_t z,
                                            int32_t id, int32_t metadata, bool *out,
                                            mc_world_mutation_error *error);
bool mc_world_mutation_height(mc_world_chunk *chunk, int32_t x, int32_t y, int32_t z,
                              mc_world_mutation_error *error);
bool mc_world_mutation_column(mc_world_chunk *chunk, int32_t x, int32_t z,
                              mc_world_mutation_error *error);

/* Actual reached Block virtual calls. No ID palette or successful substitute
 * for a missing concrete override is permitted. block is nullable until invoke. */
bool mc_world_mutation_block_added(mc_world_runtime *world, const mc_block *block, int32_t x,
                                   int32_t y, int32_t z, mc_world_mutation_error *error);
bool mc_world_mutation_block_removed(mc_world_runtime *world, const mc_block *block, int32_t x,
                                     int32_t y, int32_t z, mc_world_mutation_error *error);
bool mc_world_mutation_block_neighbor(mc_world_runtime *world, const mc_block *block, int32_t x,
                                      int32_t y, int32_t z, int32_t source_id,
                                      mc_world_mutation_error *error);

/* Callers hold this actual World. u reload/size/get order is source behavior;
 * native empty-list support must validate the reached descriptor. Nonempty
 * unsupported observer calls fail at their reached cast/invocation, never succeed.
 * The sorted column wrapper receives source LOCAL x,z without correction. */
bool mc_world_mutation_observe_cell(mc_world_runtime *world, int32_t x, int32_t y, int32_t z,
                                    mc_world_mutation_error *error);
bool mc_world_mutation_observe_range(mc_world_runtime *world, int32_t min_x, int32_t min_y,
                                     int32_t min_z, int32_t max_x, int32_t max_y, int32_t max_z,
                                     mc_world_mutation_error *error);
bool mc_world_mutation_observe_column(mc_world_runtime *world, int32_t x, int32_t z, int32_t a,
                                      int32_t b, mc_world_mutation_error *error);
bool mc_world_mutation_neighbors(mc_world_runtime *world, int32_t x, int32_t y, int32_t z,
                                 int32_t id, mc_world_mutation_error *error);

/* Actual Nibble wrappers have an unchecked Java OR-composed cell index. These
 * helpers check the reached backing byte, not numerical XYZ admission. This
 * preserves valid aliasing when a retained height byte exceeds128. */
bool mc_world_mutation_nibble_get(const mc_world_chunk_nibble *nibble, int32_t x, int32_t y,
                                  int32_t z, int32_t *out, mc_world_mutation_error *error);
bool mc_world_mutation_nibble_set(const mc_world_chunk_nibble *nibble, int32_t x, int32_t y,
                                  int32_t z, int32_t value, mc_world_mutation_error *error);
bool mc_world_mutation_q(mc_world_runtime *world, int32_t id, unsigned int *bytecode,
                         unsigned int capture_pc, unsigned int element_pc, int32_t *out,
                         mc_world_mutation_error *error);
bool mc_world_mutation_submit(mc_world_runtime *world, mc_light_kind kind,
                              const mc_light_bounds *bounds, unsigned int *bytecode,
                              unsigned int type_pc, unsigned int call_pc,
                              mc_world_mutation_error *error);

#endif
