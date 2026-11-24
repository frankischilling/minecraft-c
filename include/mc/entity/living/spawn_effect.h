#ifndef MC_ENTITY_LIVING_SPAWN_EFFECT_H
#define MC_ENTITY_LIVING_SPAWN_EFFECT_H

#include "mc/entity/entity.h"
#include "mc/item/registry/runtime.h"
#include "mc/world/runtime/presentation.h"

typedef struct mc_living_spawn_effect_site {
    const char *class_name, *method, *descriptor;
    unsigned int pc;
} mc_living_spawn_effect_site;

typedef struct mc_living_spawn_effect_limits {
    size_t max_iterations; /* Native work ceiling; the Source loop always requests twenty. */
} mc_living_spawn_effect_limits;

typedef struct mc_living_spawn_effect_state {
    bool returned;
    unsigned int source_pc, iteration;
    size_t iterations_started, gaussian_calls, float_calls, particles_returned;
} mc_living_spawn_effect_state;

typedef struct mc_living_spawn_effect_error {
    mc_world_storage_status status;
    const char *operation, *exception_class;
    mc_living_spawn_effect_site site, child;
    bool native_policy, source_exception, exception_borrowed;
    mc_entity_status entity;
    mc_registry_error registry;
    mc_world_storage_error world;
    mc_world_mutation_error particle;
    /* Independent capture of this actual failed WorldAccess invocation. A
     * capture refusal never replaces the original particle failure. */
    bool world_access_capture_attempted;
    mc_world_storage_status world_access_capture_status;
    mc_world_access_failure world_access;
    mc_world_runtime *world_occurrence; /* Owned captured World of the failing particle child. */
} mc_living_spawn_effect_error;

mc_living_spawn_effect_limits mc_living_spawn_effect_default_limits(void);

/* Actual client LS.V()/server HL.S(): twenty own-Random Gaussian/float
 * iterations, followed by each actual World's explode particle invocation.
 * This is distinct from Living death particles and natural-spawn postspawn.
 * Defining Registry supplies the reached literal at55 independently of the
 * nullable World captured at52; NULL World fails149 after argument draws.
 * DADD follows the existing captured x86-64 interpreter NaN profile; this
 * does not establish a new LS.V or default-JIT NaN-payload comparison.
 *
 * The caller holds the canonical Living Entity and its whole dependency graph
 * live/exclusive. No READY gate, World substitution, RNG/pool reset, Source
 * repair or rollback is added. State/error are required zero-initialized or
 * previously retired, disjoint from each other and all owners. The error owns
 * its independent WorldAccess capture and failing child's captured World; keep it stable and retire
 * it before reuse or destruction. Earlier source effects survive every failure. A still-owned
 * capture refuses reentry without changing state/error. */
mc_world_storage_status mc_living_spawn_effect(mc_entity *, mc_registry_classes *defining_registry,
                                               const mc_living_spawn_effect_limits *,
                                               mc_living_spawn_effect_state *,
                                               mc_living_spawn_effect_error *);

/* Native-only, idempotent end of this captured occurrence. Source/scalar
 * diagnostics remain; borrowed receipt/Throwable-name views expire here. */
void mc_living_spawn_effect_error_retire(mc_living_spawn_effect_error *);

#endif
