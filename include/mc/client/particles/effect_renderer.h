#ifndef MC_CLIENT_PARTICLES_EFFECT_RENDERER_H
#define MC_CLIENT_PARTICLES_EFFECT_RENDERER_H

#include "mc/entity/particles/particle.h"

typedef struct mc_client_effect_renderer mc_client_effect_renderer;
typedef struct mc_client_render_engine mc_client_render_engine;

typedef enum mc_effect_renderer_phase {
    MC_EFFECT_RENDERER_RAW,
    MC_EFFECT_RENDERER_CONSTRUCTING,
    MC_EFFECT_RENDERER_READY,
    MC_EFFECT_RENDERER_FAILED_CONSTRUCTION
} mc_effect_renderer_phase;

typedef enum mc_effect_renderer_status {
    MC_EFFECT_RENDERER_OK,
    MC_EFFECT_RENDERER_INVALID_ARGUMENT,
    MC_EFFECT_RENDERER_INVALID_PHASE,
    MC_EFFECT_RENDERER_BUSY,
    MC_EFFECT_RENDERER_LIMIT,
    MC_EFFECT_RENDERER_ALLOCATION_FAILED,
    MC_EFFECT_RENDERER_ENTITY_FAILED,
    MC_EFFECT_RENDERER_RANDOM_FAILED,
    MC_EFFECT_RENDERER_WORLD_FAILED,
    MC_EFFECT_RENDERER_NULL_REFERENCE,
    MC_EFFECT_RENDERER_UNSUPPORTED_PARTICLE
} mc_effect_renderer_status;

typedef struct mc_effect_renderer_error {
    mc_effect_renderer_status status;
    const char *operation, *source_method, *source_descriptor;
    unsigned int source_pc;
    size_t layer, index;
    mc_entity_status entity;
    mc_java_random_profile_status random;
    mc_particle_error particle;
} mc_effect_renderer_error;

typedef struct mc_effect_renderer_options {
    size_t max_allocation_bytes;
} mc_effect_renderer_options;

typedef struct mc_effect_renderer_snapshot {
    mc_effect_renderer_phase phase;
    unsigned int constructor_pc;
    mc_world_runtime *world;
    mc_client_render_engine *render_engine;
    const void *layers, *lists[4];
    size_t sizes[4], capacities[4], allocation_bytes;
    const mc_java_random *random;
} mc_effect_renderer_snapshot;

/* Actual client dn CPU owner: four distinct ArrayLists, its ordinary Random,
 * nullable World and nullable borrowed ji. Allocate RAW before evaluating the
 * actual constructor arguments. Classes/profile outlive this owner; allocation
 * retains the exact client class namespace without Entity active use. A supplied
 * World uses that same profile and is retained only at the reached dn29 store.
 * The typed ji is borrowed, never synthesized or dereferenced by these methods.
 * Its actual resource construction and particle drawing are separate work.
 *
 * Default allocation budget is 16 MiB. It charges this object, its four-cell
 * List array, separate Random, four List owners and native element arrays.
 * Each reached failure keeps its completed prefix, including entropy. Failed
 * construction cannot retry. No source list/Random is prebuilt at allocation.
 * All access is exclusive, and arguments/errors/outputs are mutually disjoint.
 * The strict FP profile environment remains stable through construction. */
mc_effect_renderer_status mc_client_effect_renderer_allocate(mc_entity_classes *,
                                                             const mc_effect_renderer_options *,
                                                             mc_client_effect_renderer **,
                                                             mc_effect_renderer_error *);
mc_effect_renderer_status mc_client_effect_renderer_construct(mc_client_effect_renderer *,
                                                              mc_world_runtime *,
                                                              mc_client_render_engine *,
                                                              mc_effect_renderer_error *);

/* Actual dn.a(xw), dn.a() and dn.a(fd), respectively. The current concrete
 * virtual closure admits real READY em only; other particle kinds report their
 * missing body. Each List occurrence has its own native Entity hold. Adding
 * at 4000 removes the first occurrence before appending, including on a later
 * native resource failure. Keep the supplied Entity held until add returns.
 * Tick reloads list size, invokes the concrete em override, reads be and removes
 * dead occurrences in source order. World change stores its new World first,
 * then clears all four lists without replacing their identities/capacities.
 * Native admission/ownership failures preserve reached prefixes. No callback,
 * frame scheduling, texture upload or rendering is supplied by this interface. */
mc_effect_renderer_status mc_client_effect_renderer_add(mc_client_effect_renderer *, mc_entity *,
                                                        mc_effect_renderer_error *);
mc_effect_renderer_status mc_client_effect_renderer_tick(mc_client_effect_renderer *,
                                                         mc_effect_renderer_error *);
mc_effect_renderer_status mc_client_effect_renderer_change_world(mc_client_effect_renderer *,
                                                                 mc_world_runtime *,
                                                                 mc_effect_renderer_error *);

/* Native terminal World coordination before its containers/free. Requires the
 * sole manager occurrence and SAME expected World. Releases every actual List
 * occurrence, then this manager's World binding, without invoking source world
 * change/tick callbacks. Failed admission leaves all fields and holds intact.
 * Source operations must not resume while this enclosing graph retires. */
mc_effect_renderer_status mc_client_effect_renderer_retire_world(mc_client_effect_renderer *,
                                                                 mc_world_runtime *expected_world,
                                                                 mc_effect_renderer_error *);

bool mc_client_effect_renderer_read(const mc_client_effect_renderer *,
                                    mc_effect_renderer_snapshot *);
/* Borrowed occurrence; caller holds this exclusive manager through use. */
mc_entity *mc_client_effect_renderer_particle(const mc_client_effect_renderer *, size_t, size_t);

/* Native capture holds keep the exact old manager alive across argument
 * evaluation. Release balances a hold; final retirement is explicit. Free is
 * BUSY with additional captures or an entered operation, and leaves *owner
 * unchanged then. Successful free detaches occurrences before releasing them,
 * then retires World, Random, Lists and the class binding. free(NULL) is safe. */
mc_effect_renderer_status mc_client_effect_renderer_retain(mc_client_effect_renderer *);
mc_effect_renderer_status mc_client_effect_renderer_release(mc_client_effect_renderer *);
mc_effect_renderer_status mc_client_effect_renderer_free(mc_client_effect_renderer **);

#endif
