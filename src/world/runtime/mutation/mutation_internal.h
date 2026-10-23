#ifndef MC_WORLD_RUNTIME_MUTATION_INTERNAL_H
#define MC_WORLD_RUNTIME_MUTATION_INTERNAL_H

#include "../runtime_internal.h"
#include "mc/world/chunk/object.h"
#include "mc/world/runtime/mutations.h"

/* Fresh diagnostic defaults only; these operations do not read game owners.
 * Registry-only callers avoid constructing the full mutation error. The
 * in-place initializer accepts NULL and uses the same child defaults. */
mc_registry_error mc_world_mutation_empty_registry(void);
void mc_world_mutation_error_init(mc_world_mutation_error *error);
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
bool mc_world_mutation_block_tick(mc_world_runtime *world, const mc_block *block, int32_t x,
                                  int32_t y, int32_t z, mc_java_random *random,
                                  mc_world_mutation_error *error);

/* Concrete plant neighbors and ticks. The enclosing dispatch holds the actual
 * World and Block. Survival, drops and notifying writes keep their source order;
 * no replacement receiver lookup, rollback or future tick is inserted. */
bool mc_world_mutation_plant_neighbor(mc_world_runtime *world, const mc_block *block, int32_t x,
                                      int32_t y, int32_t z, int32_t source_id,
                                      mc_world_mutation_error *error);
bool mc_world_mutation_plant_tick(mc_world_runtime *world, const mc_block *block, int32_t x,
                                  int32_t y, int32_t z, mc_java_random *random,
                                  mc_world_mutation_error *error);

/* Actual ut/mz.b(World,III). Caller holds the real World and nullable Block
 * receiver has already been checked by virtual dispatch. The fetched Tile
 * owns a temporary hold through finalization or the actual rw/lb fallback. */
bool mc_world_mutation_piston_removed(mc_world_runtime *world, const mc_block *block, int32_t x,
                                      int32_t y, int32_t z, mc_world_mutation_error *error);

/* Complete ut/mz.b(World,IIII). Caller holds the actual World/Block; NULL World
 * is reached at B1, and B=true skips the acquiring Tile getter12 entirely.
 * The nullable getter result is discarded only after releasing its native hold. */
bool mc_world_mutation_piston_neighbor(mc_world_runtime *world, const mc_block *block, int32_t x,
                                       int32_t y, int32_t z, int32_t source_id,
                                       mc_world_mutation_error *error);

/* Field-only private join to the actual admitted, held Tile's class Registry.
 * No World lookup, allocation, initialization, or alternate class owner. The
 * finalizer reaches this only at its getstatic61, after the actual ID query. */
mc_registry_classes *mc_tile_piston_registry_held(const mc_tile *tile);

/* Concrete flowing/still fluid bodies share the actual receiver scratch.
 * Internal callers already hold World and preserve the supplied Random. */
bool mc_world_mutation_fluid_tick(mc_world_runtime *world, const mc_block *block, int32_t x,
                                  int32_t y, int32_t z, mc_java_random *random,
                                  mc_world_mutation_error *error);
bool mc_world_mutation_fluid_added(mc_world_runtime *world, const mc_block *block, int32_t x,
                                   int32_t y, int32_t z, mc_world_mutation_error *error);
bool mc_world_mutation_fluid_neighbor(mc_world_runtime *world, const mc_block *block, int32_t x,
                                      int32_t y, int32_t z, int32_t source_id,
                                      mc_world_mutation_error *error);

/* Actual gk/ei bodies, inherited unchanged by ne/ig. The enclosing source flow
 * holds the same live World/Registry/Block and selected Chunk leases. Added and
 * neighbor evaluate bn and virtual delay before scheduling; tick ignores its
 * supplied Random and executes the complete immediate or real Entity branch.
 * No extra source flag, remote guard, work cap or rollback is introduced. */
bool mc_world_mutation_falling_added(mc_world_runtime *world, const mc_block *block, int32_t x,
                                     int32_t y, int32_t z, mc_world_mutation_error *error);
bool mc_world_mutation_falling_neighbor(mc_world_runtime *world, const mc_block *block, int32_t x,
                                        int32_t y, int32_t z, int32_t source_id,
                                        mc_world_mutation_error *error);
bool mc_world_mutation_falling_tick(mc_world_runtime *world, const mc_block *block, int32_t x,
                                    int32_t y, int32_t z, mc_java_random *random,
                                    mc_world_mutation_error *error);

/* Actual jr/gj snow support, neighbor/private survival and light-melt bodies.
 * The caller holds the same live GAME World and actual snow Block. Support
 * reads the below ID before Block.m, invokes virtual opacity, then separately
 * reacquires the below Material. Neighbor ignores source_id; tick ignores
 * Random, uses actual saved Block light, and does not run survival first.
 * Failed calls preserve ordinary out and every reached synchronous effect.
 * Native arguments/outputs/errors are disjoint from each other and owners. */
bool mc_world_mutation_snow_can_place(mc_world_runtime *world, const mc_block *block, int32_t x,
                                      int32_t y, int32_t z, bool *out,
                                      mc_world_mutation_error *error);
bool mc_world_mutation_snow_neighbor(mc_world_runtime *world, const mc_block *block, int32_t x,
                                     int32_t y, int32_t z, int32_t source_id,
                                     mc_world_mutation_error *error);
bool mc_world_mutation_snow_tick(mc_world_runtime *world, const mc_block *block, int32_t x,
                                 int32_t y, int32_t z, mc_java_random *random,
                                 mc_world_mutation_error *error);

/* Actual bv/bc private update and lifecycle bodies. The enclosing caller holds
 * the same World and Block. The update owns its pooled bounds/query List lease
 * until the reached source flow ends; no implicit pool reset or work cap. */
bool mc_world_mutation_pressure_update(mc_world_runtime *world, const mc_block *block, int32_t x,
                                       int32_t y, int32_t z, mc_world_mutation_error *error);
bool mc_world_mutation_pressure_tick(mc_world_runtime *world, const mc_block *block, int32_t x,
                                     int32_t y, int32_t z, mc_java_random *random,
                                     mc_world_mutation_error *error);
bool mc_world_mutation_pressure_neighbor(mc_world_runtime *world, const mc_block *block, int32_t x,
                                         int32_t y, int32_t z, int32_t source_id,
                                         mc_world_mutation_error *error);
bool mc_world_mutation_pressure_removed(mc_world_runtime *world, const mc_block *block, int32_t x,
                                        int32_t y, int32_t z, mc_world_mutation_error *error);

/* Actual invokestatic c_(World,III)Z: perform gk/ei declaring active use at the
 * caller's reached invoke, then source body query4/fire18/m29/material42/52.
 * classes is that same application's actual Registry, not an ID palette.
 * Keep it and World/dependencies live/exclusive; no new borrow is acquired.
 * World may be NULL until query4. A nullable Material is an identity value.
 * The ordinary boolean out is unchanged on failure. Arguments/results/errors
 * are disjoint from each other and owners; error is required by internal use.
 * Whole native/Registry causes and separate predicate/private/tick PCs survive
 * failure; the finite current/deepest hook projection is not a source stack. */
bool mc_world_mutation_falling_can_pass(mc_world_runtime *world, mc_registry_classes *classes,
                                        int32_t x, int32_t y, int32_t z, bool *out,
                                        mc_world_mutation_error *error);
bool mc_world_mutation_drop(mc_world_runtime *world, const mc_block *block, int32_t x, int32_t y,
                            int32_t z, int32_t metadata, mc_world_mutation_error *error);

/* Caller already holds the exact World; preserve its enclosing source frames. */
bool mc_world_mutation_sound(mc_world_runtime *world, double x, double y, double z,
                             const mc_registry_string *name, float volume, float pitch,
                             mc_world_mutation_error *error);
bool mc_world_mutation_particle(mc_world_runtime *world, const mc_registry_string *name, double x,
                                double y, double z, double velocity_x, double velocity_y,
                                double velocity_z, mc_world_mutation_error *error);

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

/* Concrete inherited rail and detector tick dispatch. The enclosing caller
 * keeps the actual World entry and receiver alive across all synchronous
 * effects; these adapters add no getter, class use or persistent binding. */
bool mc_world_mutation_rail_added_hook(mc_world_runtime *world, const mc_block *block, int32_t x,
                                       int32_t y, int32_t z, mc_world_mutation_error *error);
bool mc_world_mutation_rail_neighbor_hook(mc_world_runtime *world, const mc_block *block, int32_t x,
                                          int32_t y, int32_t z, int32_t source_id,
                                          mc_world_mutation_error *error);
bool mc_world_mutation_detector_tick_hook(mc_world_runtime *world, const mc_block *block, int32_t x,
                                          int32_t y, int32_t z, mc_java_random *random,
                                          mc_world_mutation_error *error);

#endif
