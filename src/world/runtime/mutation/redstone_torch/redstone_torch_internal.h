#ifndef MC_WORLD_MUTATION_REDSTONE_TORCH_INTERNAL_H
#define MC_WORLD_MUTATION_REDSTONE_TORCH_INTERNAL_H

#include "../../../../block/registry/redstone_torch_list_internal.h"
#include "../../../../block/registry/registry_internal.h"
#include "../mutation_internal.h"
#include "mc/core/java/java.h"

typedef enum mc_redstone_torch_method {
    MC_REDSTONE_TORCH_ADDED,
    MC_REDSTONE_TORCH_REMOVED,
    MC_REDSTONE_TORCH_NEIGHBOR,
    MC_REDSTONE_TORCH_TICK,
    MC_REDSTONE_TORCH_POWERED,
    MC_REDSTONE_TORCH_BURNOUT
} mc_redstone_torch_method;

typedef struct mc_redstone_torch_frame {
    mc_world_runtime *world;
    const mc_block *block;
    mc_world_mutation_error *error;
    mc_redstone_torch_method method;
} mc_redstone_torch_frame;

void mc_redstone_torch_at(const mc_redstone_torch_frame *, unsigned int);
bool mc_redstone_torch_done(const mc_redstone_torch_frame *);
bool mc_redstone_torch_fail(const mc_redstone_torch_frame *, unsigned int, mc_world_storage_status,
                            const char *);
bool mc_redstone_torch_null(const mc_redstone_torch_frame *, unsigned int, const char *);
bool mc_redstone_torch_storage(const mc_redstone_torch_frame *, unsigned int,
                               const mc_world_storage_error *);
bool mc_redstone_torch_nested(const mc_redstone_torch_frame *, unsigned int,
                              const mc_world_mutation_error *);
mc_world_mutation_error *mc_redstone_torch_workspace(const mc_redstone_torch_frame *, unsigned int);
bool mc_redstone_torch_child(const mc_redstone_torch_frame *, unsigned int,
                             mc_world_mutation_error *, bool);
bool mc_redstone_torch_metadata(const mc_redstone_torch_frame *, unsigned int, int32_t, int32_t,
                                int32_t, int32_t *);
bool mc_redstone_torch_power_at(const mc_redstone_torch_frame *, unsigned int, int32_t, int32_t,
                                int32_t, int32_t, bool *);
bool mc_redstone_torch_notify(const mc_redstone_torch_frame *, unsigned int, unsigned int, int32_t,
                              int32_t, int32_t);
bool mc_redstone_torch_registry(const mc_redstone_torch_frame *, unsigned int, mc_registry_status,
                                const mc_registry_error *, const char *);
bool mc_redstone_torch_collection(const mc_redstone_torch_frame *, unsigned int,
                                  const mc_registry_redstone_torch_error *, const char *);
bool mc_redstone_torch_list(const mc_redstone_torch_frame *, unsigned int,
                            mc_registry_redstone_torch_list_ref *);
bool mc_redstone_torch_time(const mc_redstone_torch_frame *, unsigned int, int64_t *);

/* Private DB.h is distinct from private VM.h. No virtual name lookup selects
 * between them. These entries share the enclosing real World/Registry hold. */
bool mc_world_mutation_redstone_torch_powered(mc_world_runtime *, const mc_block *, int32_t,
                                              int32_t, int32_t, bool *, mc_world_mutation_error *);
bool mc_world_mutation_redstone_torch_added(mc_world_runtime *, const mc_block *, int32_t, int32_t,
                                            int32_t, mc_world_mutation_error *);
bool mc_world_mutation_redstone_torch_removed(mc_world_runtime *, const mc_block *, int32_t,
                                              int32_t, int32_t, mc_world_mutation_error *);
bool mc_world_mutation_redstone_torch_neighbor(mc_world_runtime *, const mc_block *, int32_t,
                                               int32_t, int32_t, int32_t,
                                               mc_world_mutation_error *);
bool mc_world_mutation_redstone_torch_burnout(mc_world_runtime *, const mc_block *, int32_t,
                                              int32_t, int32_t, bool, bool *,
                                              mc_world_mutation_error *);
bool mc_world_mutation_redstone_torch_tick(mc_world_runtime *, const mc_block *, int32_t, int32_t,
                                           int32_t, mc_java_random *, mc_world_mutation_error *);

#endif
