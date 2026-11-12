#ifndef MC_WORLD_RUNTIME_CONSTRUCTION_H
#define MC_WORLD_RUNTIME_CONSTRUCTION_H

#include "mc/block/registry/registry.h"
#include "mc/core/java/random_profile.h"
#include "mc/core/math/math.h"
#include "mc/server/runtime/construction.h"
#include "mc/world/info/info.h"
#include "mc/world/saved_data/storage.h"
#include "mc/world/schedule/membership.h"
#include "mc/world/schedule/ordered.h"

typedef struct mc_world_runtime mc_world_runtime;
typedef struct mc_world_runtime_classes mc_world_runtime_classes;
typedef struct mc_world_provider mc_world_provider;
typedef struct mc_chunk_provider mc_chunk_provider;
typedef struct mc_world_reference_list mc_world_reference_list;
typedef struct mc_world_active_chunks mc_world_active_chunks;
typedef struct mc_world_entity_index mc_world_entity_index;

typedef enum mc_world_runtime_mode {
    MC_WORLD_RUNTIME_COMPOSITION = 0,
    MC_WORLD_RUNTIME_GAME
} mc_world_runtime_mode;

typedef enum mc_world_runtime_phase {
    MC_WORLD_RUNTIME_RAW = 0,
    MC_WORLD_RUNTIME_CONSTRUCTING,
    MC_WORLD_RUNTIME_READY,
    MC_WORLD_RUNTIME_FAILED_CONSTRUCTION,
    MC_WORLD_RUNTIME_RETIRED
} mc_world_runtime_phase;

typedef enum mc_world_runtime_entry {
    MC_WORLD_RUNTIME_CLIENT_LOAD_OR_CREATE = 0,
    MC_WORLD_RUNTIME_CLIENT_DIRECT_METADATA,
    MC_WORLD_RUNTIME_CLIENT_COPY,
    MC_WORLD_RUNTIME_SERVER_LOAD_OR_CREATE,
    MC_WORLD_RUNTIME_SERVER_DEDICATED
} mc_world_runtime_entry;

/* These five dp fields are distinct from the base class's same-label fields.
 * C and base v publish the SAME concrete dedicated cache. E is the actual
 * saving-disable cell; base E remains the scheduled TreeSet. Native ownership
 * retains F and every typed G occurrence independently. */
typedef struct mc_world_runtime_dedicated_fields {
    mc_chunk_provider *C;
    bool D, E;
    mc_server_runtime *F;
    mc_world_entity_index *G;
} mc_world_runtime_dedicated_fields;

typedef struct mc_world_runtime_dedicated_construction {
    unsigned int last_store_pc, failed_call_pc;
} mc_world_runtime_dedicated_construction;

/* All 43 declaring-class World fields, preserving case and reference identity.
 * These are read-only snapshots of the actual owner, not a second live World.
 * The five dedicated-server and six network-client shadowed fields are not
 * base fields. Unimplemented operations on child collections are separate
 * from their real, distinct empty construction identities. */
typedef struct mc_world_runtime_fields {
    bool a;
    mc_world_reference_list *C, *b, *D;
    mc_scheduled_ordered *E;
    mc_scheduled_membership *F;
    mc_world_reference_list *c, *G, *d, *e;
    int64_t H;
    int32_t f, g, h;
    float i, j, k, l;
    int32_t m, n;
    bool o;
    int64_t I;
    int32_t p, q;
    mc_java_random *r;
    bool s;
    mc_world_provider *t;
    mc_world_reference_list *u;
    mc_chunk_provider *v;
    mc_world_store *w;
    mc_world_info *x;
    bool y, J;
    mc_world_saved_data *z;
    mc_world_reference_list *K;
    bool L;
    int32_t M;
    bool N, O;
    mc_world_active_chunks *P;
    int32_t Q;
    mc_world_reference_list *R;
    bool B;
} mc_world_runtime_fields;

typedef struct mc_world_runtime_construction {
    mc_world_runtime_mode mode;
    mc_world_runtime_phase phase;
    mc_world_runtime_entry entry;
    unsigned int last_store_pc, failed_call_pc;
    bool contexts_bound, cache_registered;
    size_t direct_allocation_bytes;
    mc_java_random_profile_status random_status;
    int32_t clock_status; /* Private millis backend status, zero means OK. */
} mc_world_runtime_construction;

/* Separate native budgets; see docs/world/construction.md for exclusions.
 * Defaults: 16MiB direct World storage, 65536 scheduled entries, 512 provider
 * cache/list/pending slots and 8MiB direct provider storage, plus SavedData's
 * own limits. The dedicated Entity index separately defaults to 65536 nodes,
 * 8MiB retained and 16MiB peak requests. Zero bounds are valid. Entry bounds
 * must fit a signed Java int.
 * Java per-object allocation failure timing is not reproduced. */
typedef struct mc_world_runtime_construction_limits {
    size_t max_direct_allocation_bytes;
    size_t max_scheduled_entries;
    size_t max_provider_chunks;
    size_t max_provider_allocation_bytes;
    size_t max_dedicated_entities;
    size_t max_dedicated_allocation_bytes;
    size_t max_dedicated_peak_bytes;
    mc_world_saved_data_limits saved_data;
} mc_world_runtime_construction_limits;

typedef struct mc_world_runtime_class_state {
    mc_world_info_side side;
    bool initialized;
    int32_t enqueue_depth;
    size_t live_worlds;
} mc_world_runtime_class_state;

typedef struct mc_world_runtime_world_arguments {
    mc_world_store *store;
    mc_world_info_name *name;
    int64_t seed;
    mc_world_provider *dimension;
    mc_world_runtime *parent;
    mc_server_runtime *server;
    int32_t dimension_id;
} mc_world_runtime_world_arguments;

mc_world_runtime_construction_limits mc_world_runtime_default_construction_limits(void);

/* One concrete base-World class lifetime above its instances. Borrow the same
 * application Random profile and immutable initialized lookup table throughout
 * that lifetime. Creation initializes native storage only. First admitted World
 * allocation executes the original class's sole static A=0 store once; later
 * instances share that actual lighting-depth cell. Free is BUSY while any
 * allocated RAW, failed or ready World exists. NULL free is harmless. */
mc_world_storage_status mc_world_runtime_classes_create(mc_world_info_side side,
                                                        mc_java_random_profile *profile,
                                                        const mc_math_trig *trig,
                                                        mc_world_runtime_classes **out,
                                                        mc_world_storage_error *error);
mc_world_storage_status mc_world_runtime_classes_get(const mc_world_runtime_classes *classes,
                                                     mc_world_runtime_class_state *out);
mc_world_storage_status mc_world_runtime_classes_free(mc_world_runtime_classes *classes,
                                                      mc_world_storage_error *error);

/* Bind the same application registry above every World of this class. Match
 * side and Random-profile identity, retain one native registry borrow, and
 * execute no initializer, resource read, clock or Random operation. Bind once,
 * before allocating any World; replacing a binding or binding with live Worlds
 * is BUSY. An absent binding leaves reached registry operations unavailable.
 * Class destruction releases the borrow only after every World has been freed.
 * Materials and profile must outlive the registry, including borrowed views. */
mc_registry_status mc_world_runtime_classes_bind_registry(mc_world_runtime_classes *classes,
                                                          mc_registry_classes *registry,
                                                          mc_registry_error *error);

/* Native identity/diagnostic views; no game active use. The registry is NULL
 * for composition or an unbound class. A GAME World keeps its same binding
 * through RAW, failed and retired phases until actual free. The diagnostic is
 * replaced only by a reached registry operation, including its full nested
 * initialization cause. Both views expire when the World is freed. */
mc_registry_classes *mc_world_runtime_registry(const mc_world_runtime *runtime);
const mc_registry_error *mc_world_runtime_last_registry_error(const mc_world_runtime *runtime);

/* Allocate the SAME runtime identity before its game constructor. No game
 * instance fields, clocks, profile constructors or file operations run here.
 * Classes are borrowed above Worlds and protected by their live count. NULL
 * limits selects the defaults. Native allocation failure preserves out.
 * Allocation does not construct a dimension/provider or load metadata. */
mc_world_storage_status
mc_world_runtime_allocate_world(mc_world_runtime_entry entry, mc_world_runtime_classes *classes,
                                const mc_world_runtime_construction_limits *limits,
                                mc_world_runtime **out, mc_world_storage_error *error);

/* One reached attempt on a RAW game owner. Original arguments remain borrowed
 * for the call. NULL argument references reach their original dereference
 * points after completed prefix effects; a NULL arguments descriptor is a
 * native admission error. Failed construction cannot be retried. This entry
 * completes normal direct/copy/already-loaded base and dedicated construction.
 * The dedicated entry evaluates dimension_id through the actual factory before
 * the base instance prefix; its dimension argument is not used. It constructs
 * the dedicated cache during the base virtual call, then G and F in source
 * order after the base returns. A nullable server is stored without an invented
 * dereference. Server startup is a separate operation.
 * Missing concrete descendants stop explicitly and never publish READY for a
 * completed prefix. Unsupported Dimension providers, network-client bootstrap,
 * server startup and general failure parity remain unfinished. See
 * docs/world/construction.md. */
mc_world_storage_status
mc_world_runtime_construct_world(mc_world_runtime *runtime,
                                 const mc_world_runtime_world_arguments *arguments,
                                 mc_world_storage_error *error);

/* Explicit native retirement, separate from original gameplay construction.
 * Require no entered call/local lease and empty owned registrations. Detach
 * actual contexts and this dimension's binding to this exact World without
 * running game biome setup. External Entity/TileEntity/feature bindings still keep the same
 * retired owner alive and free BUSY until released; no aggregate count is reset.
 * No implicit save, file rollback, profile reset or game constructor retry.
 * Repeated retirement succeeds. Existing Info/field inspection stays valid;
 * typed lighting records in C and scheduled records in E/F remain owned until
 * final native free, including F-only partial insertion state. The dedicated
 * Entity index is released and G becomes NULL during terminal retirement;
 * no source clear/removal hook is run. The server hold remains until free.
 * The retained scheduled/light records need no
 * propagation, queue repair or implicit gameplay clear to retire the World.
 * ordinary gameplay/new persistent bindings are unavailable after retirement. */
mc_world_storage_status mc_world_runtime_retire_world(mc_world_runtime *runtime,
                                                      mc_world_storage_error *error);

/* Inspect the actual phase/prefix without entering a gameplay operation.
 * Composition READY describes the existing already-loaded foundation only.
 * fields_get requires GAME mode; raw and failed field prefixes are inspectable.
 * Neither query allocates, retains references or changes diagnostics. */
mc_world_storage_status mc_world_runtime_construction_get(const mc_world_runtime *runtime,
                                                          mc_world_runtime_construction *out);
mc_world_storage_status mc_world_runtime_fields_get(const mc_world_runtime *runtime,
                                                    mc_world_runtime_fields *out);

/* Passive dedicated-owner inspection, including RAW and failed prefixes.
 * Rejects base/composition Worlds without changing outputs. This does not
 * initialize the Entity index or manufacture a server/cache identity. */
mc_world_storage_status
mc_world_runtime_dedicated_get(const mc_world_runtime *runtime,
                               mc_world_runtime_dedicated_fields *fields,
                               mc_world_runtime_dedicated_construction *construction);

#endif
