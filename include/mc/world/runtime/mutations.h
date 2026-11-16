#ifndef MC_WORLD_RUNTIME_MUTATIONS_H
#define MC_WORLD_RUNTIME_MUTATIONS_H

#include "mc/entity/entity.h"
#include "mc/world/dimension/world_chunk_manager.h"
#include "mc/world/info/info.h"
#include "mc/world/runtime/collision/boxes.h"
#include "mc/world/runtime/collision/entities.h"
#include "mc/world/runtime/entities.h"
#include "mc/world/runtime/power/power.h"
#include "mc/world/runtime/rails/errors.h"
#include "mc/world/runtime/world_access_error.h"

#include "mc/world/runtime/lighting.h"
#include "mc/world/runtime/plants.h"
#include "mc/world/runtime/queries.h"
#include "mc/world/runtime/scheduling_context.h"
#include "mc/world/runtime/tile_entities/errors.h"

/* Current source sites for the synchronous redstone closure. Repeated recursive
 * methods retain the current caller here and the deepest hook below, rather
 * than owning a recursive diagnostic stack. Zero means no reached site. */
typedef struct mc_world_redstone_sites {
    unsigned int added, removed, neighbor, support, drain, update, notify, strength;
} mc_world_redstone_sites;

typedef struct mc_world_redstone_torch_sites {
    unsigned int added, removed, neighbor, tick, powered, burnout;
} mc_world_redstone_torch_sites;

typedef enum mc_world_redstone_torch_collection_failure {
    MC_WORLD_REDSTONE_TORCH_COLLECTION_NONE,
    MC_WORLD_REDSTONE_TORCH_COLLECTION_NULL,
    MC_WORLD_REDSTONE_TORCH_COLLECTION_INDEX,
    MC_WORLD_REDSTONE_TORCH_COLLECTION_NATIVE
} mc_world_redstone_torch_collection_failure;

/* Current Fire callers and their actual rain child. These are reached sites,
 * not a recursive source stack or an execution/work limit. */
typedef struct mc_world_fire_sites {
    unsigned int added, neighbor, tick, can_place, neighbors, flammable;
    unsigned int encouragement, spread, burn;
    unsigned int rain_bytecode, raining_bytecode, rain_strength_bytecode;
    unsigned int dimension_bytecode, biome_bytecode;
} mc_world_fire_sites;

/* Distinct actual JQ/H/FD/UT callers. This is a finite diagnostic projection,
 * not a recursion/work limit or an owning Source stack. */
typedef struct mc_world_piston_sites {
    unsigned int added, neighbor, update, power, can_push, scan, move, event;
    unsigned int world_event, factory, head_removed, head_neighbor;
} mc_world_piston_sites;

typedef struct mc_world_mutation_error {
    mc_world_storage_status status;
    const char *operation;
    /* Separate reached source frames; zero is native admission or unreached.
     * wrapper is the notifying entry, bytecode its quiet World callee. */
    unsigned int wrapper_bytecode, bytecode, chunk_bytecode, height_bytecode;
    unsigned int column_bytecode, interval_bytecode, world_access_bytecode;
    unsigned int neighbor_call_bytecode, neighbor_bytecode, hook_bytecode;
    unsigned int presence_bytecode, provider_bytecode;
    const char *hook_class;
    const char *hook_method;
    const char *drop_class, *drop_method;
    unsigned int drop_bytecode, drop_call_bytecode, drop_spawn_bytecode;
    mc_item_stack_status item;
    mc_entity_error entity;
    mc_world_entity_error publication;
    mc_world_entity_batch_error batch;
    mc_live_tile_entity_error tile_entity;
    unsigned int activation_bytecode, activation_world_bytecode;
    unsigned int load_bytecode, load_builder_bytecode;
    mc_world_entity_load_error load_entity;
    /* Flat nested diagnostics retain the current concrete caller and deepest
     * reached concrete hook origin, rather than a recursive call-stack owner.
     * No gameplay recursion or work limit follows from this representation. */
    const char *cause_hook_class, *cause_hook_method;
    unsigned int cause_hook_bytecode;
    unsigned int schedule_bytecode, support_bytecode;
    /* Separate falling tick/private/predicate sites, alongside current/deepest
     * concrete hooks. These are explicit reached call frames, not a stack. */
    unsigned int falling_tick_bytecode, falling_bytecode, falling_predicate_bytecode;
    /* Snow's neighbor, private survival, support and tick are distinct source
     * frames. Detailed material/light queries retain their own nested sites. */
    unsigned int snow_neighbor_bytecode, snow_bytecode, snow_support_bytecode, snow_tick_bytecode;
    /* VM's six enclosing bodies retain their own reached sites. A null source
     * receiver is explicit evidence, separate from native admission failure. */
    unsigned int torch_can_place_bytecode, torch_floor_bytecode, torch_survive_bytecode;
    unsigned int torch_added_bytecode, torch_neighbor_bytecode, torch_tick_bytecode;
    bool torch_source_null;
    bool lever_source_null;
    bool ladder_source_null;
    bool bed_source_null;
    bool fire_source_null, fire_array_index, stairs_source_null;
    bool furnace_source_null;
    unsigned int redstone_ore_tick_bytecode;
    bool redstone_ore_source_null;
    mc_world_fire_sites fire;
    mc_world_provider_status fire_dimension;
    mc_world_chunk_manager_error fire_biome;
    /* Pressure contact/tick, private update and lifecycle calls are separate
     * source frames. Bounds and nearby retain the actual reached children. */
    unsigned int pressure_contact_bytecode, pressure_tick_bytecode, pressure_bytecode;
    unsigned int pressure_neighbor_bytecode, pressure_removed_bytecode;
    mc_entity_bounds_error bounds;
    mc_world_nearby_error nearby;
    /* uk.l/mu.k and ut/mz.b(World,III) are distinct source frames. Preserve
     * their current caller and deepest nested occurrence when a notifying
     * setter reenters removal/finalization; this is not a recursive stack. */
    unsigned int piston_finalize_bytecode, piston_removed_bytecode;
    unsigned int cause_piston_finalize_bytecode, cause_piston_removed_bytecode;
    mc_world_piston_sites piston;
    bool piston_source_null, piston_class_cast;
    mc_world_material_error material;
    mc_world_query_error query;
    mc_world_plant_error plant;
    mc_world_plant_tree_error plant_tree;
    mc_world_schedule_prefix schedule;
    mc_scheduled_tick_status record;
    mc_scheduled_membership_status membership;
    mc_scheduled_ordered_status ordered;
    mc_world_storage_error cause;
    mc_registry_error registry;
    mc_world_light_error light;
    mc_world_population_context population;
    /* Explicit selected bits distinguish neutral from selected-but-unentered
     * rail callees. All sites are a finite current/deepest projection, never a
     * source stack, callback owner, recursion bound or gameplay work limit. */
    uint32_t rail_sites_selected;
    mc_world_detector_site rail_detector_contact, rail_detector_tick, rail_detector_update;
    mc_world_detector_site rail_detector_cause;
    mc_rail_lifecycle_site rail_added, rail_shape, rail_neighbor, rail_lifecycle_cause;
    mc_rail_connection_site rail_connection, rail_connection_cause;
    mc_rail_connection_site rail_predicate, rail_predicate_cause;
    mc_rail_connection_site rail_accessor, rail_accessor_cause;
    mc_rail_power_search_site rail_search, rail_verify, rail_search_cause;
    /* Exact actually failed children are copied at reached callsites. No
     * apparent PC/status/sentinel is used to discover which child was reached.
     * Scheduler prefix above stays nonrecursive; a full schedule error owns
     * a mutation tick child and cannot itself be embedded in this record. */
    mc_world_power_error power;
    mc_world_collision_error rail_access;
    mc_world_solid_support_error rail_support;
    mc_world_redstone_sites redstone;
    /* DB owns six separate bodies and a class-static ArrayList. These flat
     * values record actual failed operations, without owning List storage or
     * imposing a source recursion/work bound. */
    mc_world_redstone_torch_sites redstone_torch;
    bool redstone_torch_source_null, redstone_torch_mod_count_stored;
    mc_registry_status redstone_torch_collection_status;
    mc_world_redstone_torch_collection_failure redstone_torch_collection_failure;
    mc_world_info_status redstone_torch_info;
    /* Only a failed held value read selects these coordinates. Its complete
     * acquisition/query child already occupies nearby/query above. Avoid
     * duplicating that large graph in every enclosing mutation diagnostic. */
    bool redstone_value_failed;
    int32_t redstone_x, redstone_y, redstone_z;
    /* Actual List.size/get null witness, independent of an interface failure
     * receipt. The detailed iterator records this at the reached operation. */
    mc_world_access_source_fault world_access_source_fault;
    mc_world_access_failure world_access_failure;
} mc_world_mutation_error;

/* Actual GAME World setters. Bounds and acquisition precede the selected
 * concrete Chunk call. ID and metadata remain full Java ints until each source
 * narrowing/index operation. Equal results are ordinary booleans, independent
 * of native status. Blank overrides return true without changing their bytes.
 * The two Chunk overloads retain their distinct hook/remote/no-sky ordering.
 *
 * Hold the World and exact selected Chunk through synchronous calls. Failure
 * preserves out and every reached byte, metadata, height, queue, hook and dirty
 * effect; no rollback/retry occurs. Missing reached descendants fail explicitly.
 * Require live exclusive owners and disjoint arguments/results/errors. GAME
 * construction/failed prefixes are admitted; RETIRED/COMPOSITION are rejected.
 * No whole-ID-table or future-array preflight replaces the source accesses. */
mc_world_storage_status mc_world_runtime_block_set_quiet(mc_world_runtime *world, int32_t x,
                                                         int32_t y, int32_t z, int32_t id,
                                                         bool *out, mc_world_mutation_error *error);
mc_world_storage_status mc_world_runtime_block_metadata_set_quiet(mc_world_runtime *world,
                                                                  int32_t x, int32_t y, int32_t z,
                                                                  int32_t id, int32_t metadata,
                                                                  bool *out,
                                                                  mc_world_mutation_error *error);

/* Notifying variants call the corresponding quiet operation, then WorldAccess receivers
 * and six live neighbors only on ordinary true. They pass the original full
 * requested ID. WorldAccess and neighbor failures preserve prior effects/out.
 * These operations do not enable a future lighting drain or later tick. */
mc_world_storage_status mc_world_runtime_block_set(mc_world_runtime *world, int32_t x, int32_t y,
                                                   int32_t z, int32_t id, bool *out,
                                                   mc_world_mutation_error *error);
mc_world_storage_status mc_world_runtime_block_metadata_set(mc_world_runtime *world, int32_t x,
                                                            int32_t y, int32_t z, int32_t id,
                                                            int32_t metadata, bool *out,
                                                            mc_world_mutation_error *error);

/* Original notifying metadata operation is void. After quiet true, acquire the
 * current ID again, then read live Block.t[id&255]. That flag selects WorldAccess
 * plus neighbors or neighbors alone. Repeated/equal nibble writes still run it.
 * Native status is the only ordinary return channel of this C operation. */
mc_world_storage_status mc_world_runtime_metadata_set(mc_world_runtime *world, int32_t x, int32_t y,
                                                      int32_t z, int32_t metadata,
                                                      mc_world_mutation_error *error);

#endif
