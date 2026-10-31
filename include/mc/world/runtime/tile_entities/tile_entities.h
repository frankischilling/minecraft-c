#ifndef MC_WORLD_RUNTIME_TILE_ENTITIES_TILES_H
#define MC_WORLD_RUNTIME_TILE_ENTITIES_TILES_H

#include "mc/item/stack/stack.h"
#include "mc/storage/nbt/tree.h"
#include "mc/world/chunk/object.h"
#include "mc/world/info/info.h"
#include "mc/world/runtime/tile_entities/errors.h"

#include <stdbool.h>

/* Concrete Chest/Spawner/Piston TileEntity owner. Caller-facing runtime and persistence
 * joins require the coordinated actual owners; reached missing descendants
 * remain explicit failures. Full gameplay/tick qualification is separate. */
typedef struct mc_world_runtime mc_world_runtime;
typedef struct mc_world_runtime_classes mc_world_runtime_classes;
typedef struct mc_world_reference_list mc_world_reference_list;
typedef struct mc_tile_entity mc_tile_entity;
typedef struct mc_tile_entity_name mc_tile_entity_name;
typedef struct mc_tile_entity_classes mc_tile_entity_classes;
typedef struct mc_tile_entity_class_literal mc_tile_entity_class_literal;
typedef struct mc_tile_entity_map_backing mc_tile_entity_map_backing;

typedef enum mc_tile_entity_kind {
    MC_TILE_ENTITY_CHEST = 0,
    MC_TILE_ENTITY_SPAWNER,
    MC_TILE_ENTITY_PISTON
} mc_tile_entity_kind;

typedef enum mc_tile_entity_class_phase {
    MC_TILE_ENTITY_CLASS_COLD = 0,
    MC_TILE_ENTITY_CLASS_INITIALIZING,
    MC_TILE_ENTITY_CLASS_READY,
    MC_TILE_ENTITY_CLASS_FAILED
} mc_tile_entity_class_phase;

/* Passive Class selectors, independent of instance kinds, private registration
 * IDs and constructor availability. All eight concrete classes directly extend
 * base TileEntity in the unchanged client/server class declarations. */
typedef enum mc_tile_entity_class_kind {
    MC_TILE_ENTITY_CLASS_KIND_TILE_ENTITY = 0,
    MC_TILE_ENTITY_CLASS_KIND_FURNACE,
    MC_TILE_ENTITY_CLASS_KIND_CHEST,
    MC_TILE_ENTITY_CLASS_KIND_JUKEBOX,
    MC_TILE_ENTITY_CLASS_KIND_DISPENSER,
    MC_TILE_ENTITY_CLASS_KIND_SIGN,
    MC_TILE_ENTITY_CLASS_KIND_SPAWNER,
    MC_TILE_ENTITY_CLASS_KIND_NOTE_BLOCK,
    MC_TILE_ENTITY_CLASS_KIND_PISTON,
    MC_TILE_ENTITY_CLASS_KIND_COUNT
} mc_tile_entity_class_kind;

typedef struct mc_tile_entity_class_literal_view {
    const mc_tile_entity_classes *owner;
    const mc_world_runtime_classes *world_classes;
    mc_world_info_side side;
    mc_tile_entity_class_kind kind;
    const char *client_name, *server_name; /* Diagnostic names, not source Strings. */
    const char *superclass_name; /* Actual side's declared name, including java.lang.Object. */
} mc_tile_entity_class_literal_view;

/* One counted native borrow of the SAME TileEntity class owner above these
 * Worlds. The first borrow may allocate its cold native header/canonical cells;
 * it initializes no source class, registration Map or Piston List, creates no
 * World/TileEntity, reads no resource and consumes no Random/profile state.
 * Later borrows return the identical owner; overflow is LIMIT. The borrow
 * blocks explicit TileEntity class retirement and World-class destruction.
 * release consumes one borrow and clears its slot; a NULL slot is invalid,
 * a NULL *slot is harmless, and an unbalanced release is INVALID_ARGUMENT.
 * Borrowing a namespace whose source initializer failed does not retry it. */
mc_tile_entity_status mc_tile_entity_classes_borrow(mc_world_runtime_classes *world_classes,
                                                    mc_tile_entity_classes **out);
mc_tile_entity_status mc_tile_entity_classes_release(mc_tile_entity_classes **slot);

/* Actual canonical Class identity is pointer equality within one live owner;
 * distinct namespaces have distinct cells even with the same side/profile.
 * Hold a counted namespace borrow throughout every returned literal/view and
 * cached key use, including view.owner and view.world_classes. These passive
 * queries allocate/initialize nothing and work independently of source class
 * phases. Invalid arguments preserve out. get rejects an unknown selector with
 * INVALID_KIND. superclass returns the same owner's base OW/JH for each of the
 * eight concrete classes. The base's real java/lang/Object superclass is
 * outside this admitted namespace: superclass(base) returns INVALID_KIND and
 * preserves out, never a fabricated NULL Class. LL checks base before asking.
 * All operations require exclusive live owners and disjoint outputs/slots;
 * expired or noncanonical pointers are not admitted. */
mc_tile_entity_status mc_tile_entity_class_literal_get(const mc_tile_entity_classes *classes,
                                                       mc_tile_entity_class_kind kind,
                                                       const mc_tile_entity_class_literal **out);
mc_tile_entity_status
mc_tile_entity_class_literal_inspect(const mc_tile_entity_class_literal *literal,
                                     mc_tile_entity_class_literal_view *out);
mc_tile_entity_status
mc_tile_entity_class_literal_superclass(const mc_tile_entity_class_literal *literal,
                                        const mc_tile_entity_class_literal **out);
/* Actual Object.getClass for the fixed allocation type, in RAW, READY or failed
 * construction phases. Hold the instance during this query and the counted
 * class borrow for subsequent literal use. NULL instance is INVALID_ARGUMENT;
 * an unsupported native instance kind is INVALID_KIND. No active use occurs. */
mc_tile_entity_status mc_tile_entity_get_class(const mc_tile_entity *tile_entity,
                                               const mc_tile_entity_class_literal **out);

/* Passive class-owner view. No active use, initialization, allocation or holds.
 * List identity is borrowed until class retirement; it is not a mutable List
 * API. allocation_bytes counts only the Piston static List's requested native
 * bytes, independently of base TileEntity maps/literals and instance storage.
 * list_mod_count_bits is the raw wrapping Java int counter. Published backing
 * slots from count to capacity are NULL; a backing view expires on growth. */
typedef struct mc_tile_entity_piston_class_snapshot {
    mc_tile_entity_class_phase base_phase, piston_phase;
    unsigned int base_initializer_bytecode, piston_initializer_bytecode;
    mc_tile_entity_status base_failure, piston_failure;
    size_t base_name_count, base_class_count;
    const mc_world_reference_list *list_identity;
    size_t list_count, list_capacity, allocation_bytes, live_tile_entities;
    size_t max_allocation_bytes;
    size_t class_literal_borrows; /* Counted passive namespace users, not source fields. */
    uint32_t list_mod_count_bits;
} mc_tile_entity_piston_class_snapshot;

/* Configure one immutable native requested-byte ceiling before Piston active
 * use. This creates the native class owner without initializing base TileEntity or
 * Piston. Default is 1 MiB. The native ArrayList starts with one empty
 * descriptor (capacity0); later backing, owning ledger and temporary snapshots
 * share this peak requested-byte ceiling. This policy does not claim JDK
 * allocation/OOM timing.
 * A failed active use latches its first cause and cannot be configured/retried. */
mc_tile_entity_status mc_tile_entity_piston_class_configure(mc_world_runtime *world,
                                                            size_t max_allocation_bytes);
mc_tile_entity_status
mc_tile_entity_piston_class_get_snapshot(const mc_world_runtime_classes *classes,
                                         mc_tile_entity_piston_class_snapshot *out);

/* Explicit native shutdown only: after retiring/releasing ALL TileEntity owners,
 * discard the shared Piston List's owning Entity occurrences. This breaks a
 * failed push's m -> Entity -> World -> class native ownership cycle so the
 * Worlds can subsequently be freed. Requires exclusive quiescent classes and
 * no later gameplay/resumption; returns BUSY while any TileEntity owner remains.
 * Full owned-List admission precedes any release. Capacity/backing remain for
 * ordinary class destruction. Cold/absent TileEntity classes stay uninitialized;
 * this is neither a Java gameplay clear nor an active-class retry. No World
 * retirement or failed tick invokes it automatically. Failure preserves all
 * owners and replaces error; a nonnull classes owner is required. */
mc_world_storage_status
mc_tile_entity_piston_class_discard_entities(mc_world_runtime_classes *classes,
                                             mc_world_storage_error *error);

typedef enum mc_tile_entity_phase {
    MC_TILE_ENTITY_RAW = 0,
    MC_TILE_ENTITY_READY,
    MC_TILE_ENTITY_FAILED_CONSTRUCTION
} mc_tile_entity_phase;

typedef struct mc_tile_entity_name_view {
    bool is_null;
    mc_nbt_string text;
} mc_tile_entity_name_view;

/* Defined in every phase, including failed constructor prefixes. Unused
 * kind-specific fields are zero. The name/world views are borrowed until the
 * next corresponding assignment or final release; hold the name separately
 * across synchronous reentry. Rotation snapshots preserve representation bits. */
typedef struct mc_tile_entity_snapshot {
    mc_tile_entity_kind kind;
    mc_tile_entity_phase phase;
    mc_world_runtime *world;
    int32_t x, y, z;
    bool invalid;
    size_t chest_capacity;
    int32_t spawner_delay;
    uint64_t spawner_rotation_bits, spawner_previous_rotation_bits;
    mc_tile_entity_name *spawner_name;
    int32_t piston_block_id, piston_block_data, piston_facing;
    bool piston_extending, piston_render_head;
    uint32_t piston_progress_bits, piston_previous_progress_bits;
} mc_tile_entity_snapshot;

/* Concrete World dirty is implemented in this module. Existing persistent
 * counted World bindings prevent runtime_free
 * while a TileEntity retains its world field, even after invalidation/detachment.
 * Retain failure leaves the binding count unchanged and sets runtime last_error.
 * Release requires a counted binding. Dirty enters/protects the real runtime,
 * executes actual acquiring dirty/WorldAccess behavior and retains partial effects. */

bool mc_world_runtime_tile_entity_dirty(mc_world_runtime *runtime, int32_t x, int32_t y, int32_t z,
                                        mc_tile_entity *tile_entity);

/* Immutable nullable String owners. Create copies explicit UTF-16 units, with
 * no normalization; NULL units require length0. Empty and NULL are distinct.
 * retain(NULL) succeeds; release(NULL) is safe. View of NULL reports is_null.
 * Views borrow the held name. Interned Pig/empty handles can be immortal and
 * consume no requested heap bytes. No native object identity is copied by a
 * TileEntity name setter; it retains that same handle. */
mc_tile_entity_status mc_tile_entity_name_create(const uint16_t *units, size_t length,
                                                 mc_tile_entity_name **out);
mc_tile_entity_status mc_tile_entity_name_retain(mc_tile_entity_name *name);
void mc_tile_entity_name_release(mc_tile_entity_name *name);
mc_tile_entity_status mc_tile_entity_name_get_view(const mc_tile_entity_name *name,
                                                   mc_tile_entity_name_view *out);
size_t mc_tile_entity_name_native_bytes(const mc_tile_entity_name *name);

/* Allocate raw zero fields before constructor work, with one native hold and
 * fixed concrete kind. Construct only RAW: chest allocates36 null physical
 * slots; spawner executes delay-1/previous0/Pig/delay20; Piston default invokes
 * only the empty base constructor and leaves every raw field zero. Piston's
 * separate static ArrayList initializes before any instance allocation.
 * Constructor failure
 * consumes RAW and remains FAILED_CONSTRUCTION; no transparent retry. Snapshot,
 * native-byte query and hold/release support every phase. All mutation/semantic
 * calls require READY and a caller hold stable throughout synchronous reentry.
 * Result/error storage is disjoint from owners, input and each other; failure
 * leaves outputs unchanged except explicit error records and actual prefixes.
 * All access is exclusive; no thread synchronization or arbitrary stale-pointer
 * detection is provided. */
mc_tile_entity_status mc_tile_entity_allocate(mc_world_runtime *world, mc_tile_entity_kind kind,
                                              mc_tile_entity **out);
/* Allocate in the actual defining class namespace before any World exists.
 * Bind its application Registry before active use and keep classes live and
 * exclusive. This uses the same lazy TileEntity/Piston initialization, failure latch
 * and instance ownership as the World entry above. It does not initialize the
 * World class, allocate/bind a World or warm Block/Direction/Entity classes.
 * Class destruction is BUSY while the returned TileEntity remains live. */
mc_tile_entity_status mc_tile_entity_allocate_in_classes(mc_world_runtime_classes *classes,
                                                         mc_tile_entity_kind kind,
                                                         mc_tile_entity **out);
mc_tile_entity_status mc_tile_entity_construct(mc_tile_entity *raw);
mc_tile_entity_status mc_tile_entity_construct_detailed(mc_tile_entity *raw,
                                                        mc_tile_entity_error *error);
/* Complete uk/mu(IIIZZ), only for a RAW Piston. Stores ID/data/facing/extending/
 * render flag in source order, leaves both progress fields +0, and performs no
 * World install, validation, clamp, dirty or RNG operation. */
mc_tile_entity_status mc_tile_entity_piston_construct(mc_tile_entity *raw, int32_t block_id,
                                                      int32_t block_data, int32_t facing,
                                                      bool extending, bool render_head,
                                                      mc_tile_entity_error *error);
mc_tile_entity_status mc_tile_entity_retain(mc_tile_entity *tile_entity);
void mc_tile_entity_release(mc_tile_entity *tile_entity);
mc_tile_entity_status mc_tile_entity_get_snapshot(const mc_tile_entity *tile_entity,
                                                  mc_tile_entity_snapshot *out);

/* Exact direct requested heap bytes: common owner plus its physical slot array.
 * Name handles and ItemStack owners are independent held identities, excluded
 * here and exposed separately; callers count shared owners once. Whole TileEntity
 * provenance is also excluded here and exposed below. This is not a total
 * world/process memory query. */
size_t mc_tile_entity_native_bytes(const mc_tile_entity *tile_entity);

/* bind_world acquires a new persistent runtime binding before pointer store,
 * then releases the previous binding; same-pointer store does not recount.
 * NULL clears only through this explicit native operation. Invalidation and
 * detachment never clear world or coordinates. These separate operations let
 * actual chunk install retain original world->coords->admission/map order. */
mc_tile_entity_status mc_tile_entity_bind_world(mc_tile_entity *tile_entity,
                                                mc_world_runtime *world);
mc_tile_entity_status mc_tile_entity_set_coordinates(mc_tile_entity *tile_entity, int32_t x,
                                                     int32_t y, int32_t z);
mc_tile_entity_status mc_tile_entity_set_invalid(mc_tile_entity *tile_entity, bool invalid);
mc_tile_entity_status mc_tile_entity_dirty(mc_tile_entity *tile_entity);
mc_tile_entity_status mc_tile_entity_dirty_detailed(mc_tile_entity *tile_entity,
                                                    mc_live_tile_entity_error *error);

/* Chest logical size27 and limit64 differ from physical capacity36 before load,
 * 27 after its reached NBT array replacement. Slot access checks PHYSICAL bounds.
 * A successful getter publishes a hold on the SAME nullable stack identity. */
mc_tile_entity_status mc_tile_entity_chest_size(const mc_tile_entity *tile_entity, int32_t *out);
mc_tile_entity_status mc_tile_entity_chest_limit(const mc_tile_entity *tile_entity, int32_t *out);
mc_tile_entity_status mc_tile_entity_chest_capacity(const mc_tile_entity *tile_entity, size_t *out);
mc_tile_entity_status mc_tile_entity_chest_get(mc_tile_entity *tile_entity, int32_t slot,
                                               mc_item_stack **out);

/* Retain/install the same supplied stack, release only the previous slot hold,
 * then clamp count>64 and perform real dirty. Null is accepted; other counts
 * remain unchanged. No copy/item-max query. Later failure keeps install/clamp.
 * Remove returns same owner when count<=amount, else actual ItemStack split;
 * exact-zero source count clears slot. Dirty follows mutation. An empty slot
 * returns successful NULL without dirty. On failure result remains unchanged;
 * earlier slot/count/world effects remain and only private result holds release.
 * A stack installed by this API must be READY. No amount/ID/damage range clamp. */
mc_tile_entity_status mc_tile_entity_chest_set(mc_tile_entity *tile_entity, int32_t slot,
                                               mc_item_stack *stack);
mc_tile_entity_status mc_tile_entity_chest_set_detailed(mc_tile_entity *tile_entity, int32_t slot,
                                                        mc_item_stack *stack,
                                                        mc_live_tile_entity_error *error);
mc_tile_entity_status mc_tile_entity_chest_remove(mc_tile_entity *tile_entity, int32_t slot,
                                                  int32_t amount, mc_item_stack **out);
mc_tile_entity_status mc_tile_entity_chest_remove_detailed(mc_tile_entity *tile_entity,
                                                           int32_t slot, int32_t amount,
                                                           mc_item_stack **out,
                                                           mc_live_tile_entity_error *error);
/* Detailed variants publish fresh errors and preserve the actual nested dirty
 * status/operation/World/WorldAccess frame through late failures after install,
 * clamp, clear or split. They preserve the old status-only wrappers for native
 * callers; dungeon/mutation integration uses the detailed entries explicitly. */

/* Field-only name assignment, no dirty, update or RNG. Getter publishes a hold
 * on the same nullable name handle. Delay/rotation inspection uses snapshot. */
mc_tile_entity_status mc_tile_entity_spawner_get_name(mc_tile_entity *tile_entity,
                                                      mc_tile_entity_name **out);
mc_tile_entity_status mc_tile_entity_spawner_set_name(mc_tile_entity *tile_entity,
                                                      mc_tile_entity_name *name);

/* Actual field-only Piston getters, requiring a held READY Piston. No World,
 * dirty, class initialization or mutation. Metadata is the stored uk/mu.e(),
 * rather than the inherited live World metadata getter. Progress bits preserve
 * both raw binary32 representations; no interpolation is done by that getter.
 * The render flag is stored on both sides; its original public getter k()Z
 * exists only on the client. This native field view also inspects server state.
 * All outputs are required, disjoint, and unchanged on failure. */
mc_tile_entity_status mc_tile_entity_piston_block_id(const mc_tile_entity *tile_entity,
                                                     int32_t *out);
mc_tile_entity_status mc_tile_entity_piston_block_data(const mc_tile_entity *tile_entity,
                                                       int32_t *out);
mc_tile_entity_status mc_tile_entity_piston_facing(const mc_tile_entity *tile_entity, int32_t *out);
mc_tile_entity_status mc_tile_entity_piston_extending(const mc_tile_entity *tile_entity, bool *out);
mc_tile_entity_status mc_tile_entity_piston_render_head(const mc_tile_entity *tile_entity,
                                                        bool *out);
mc_tile_entity_status mc_tile_entity_piston_progress_bits(const mc_tile_entity *tile_entity,
                                                          uint32_t *current, uint32_t *previous);
/* Actual uk/mu.a(F) interpolation with a raw factor. Only factor>1 clamps.
 * Unsupported native floating environment fails at getter PC20, with no result
 * or TileEntity field change. Separate binary32 operations preserve observed NaNs. */
mc_tile_entity_status mc_tile_entity_piston_progress(const mc_tile_entity *tile_entity,
                                                     uint32_t factor_bits, uint32_t *out_bits,
                                                     mc_tile_entity_error *error);

/* Piston push, tick and finalize/moving-block replacement have unreached
 * descendants and are not provided as inherited base no-op successes. */

/* Ordered semantic read on READY: base Int x/y/z, then subtype fields. Missing
 * exact-typed fields use original defaults; wrong present types fail at their
 * own reached lookup. Failure keeps READY and completed field/array/slot prefix.
 * Base read does not touch world/invalid. Chest queries Items BEFORE allocating
 * and replacing physical storage with27 null slots; then installs each valid
 * Slot Byte&255 record in list order, last duplicate wins. No clamp/dirty. New
 * stacks are constructed before their actual slot assignment. Spawner owns its
 * new EntityId value before reading Short Delay, retaining coordinate/name prefix
 * on later failure. Piston reads Int blockId/blockData/facing, Float progress
 * into current k then previous l, and Byte extending in that order. It leaves
 * the render/source flag j unchanged. Input is an immutable valid NBT tree.
 * The caller must keep
 * every input owner and span alive throughout the call and all partial prefixes,
 * including replacement releasing old slot holds and their stack/source owners.
 * Input borrowing such a source requires an independent stack/source hold;
 * ownership solely through the old chest slot is insufficient. Independently
 * owned decoded NBT already meets this rule. Input may be released after return:
 * strings/stack fields do not borrow it.
 *
 * This semantic operation does not attach or replace provenance. The explicit
 * storage import below composes it with EXTRA independent source ownership. */
mc_tile_entity_status mc_tile_entity_read_base(mc_tile_entity *tile_entity,
                                               const mc_nbt_tag *compound,
                                               mc_tile_entity_error *error);
mc_tile_entity_status mc_tile_entity_read_nbt(mc_tile_entity *tile_entity,
                                              const mc_nbt_tag *compound,
                                              mc_tile_entity_error *error);

/* Explicit storage composition, not an alternate TileEntity registry. resolved_kind
 * must come from the actual concrete factory decision. Construct/read a new
 * unpublished TileEntity, capture each final stack/Items-index association DURING
 * assignment, then copy the whole TileEntity and independently attach each winning
 * copied child to that SAME stack identity. Whole copy first canonicalizes
 * ignored list-child names. Only complete success publishes a READY, unbound
 * owner. Failure releases every unpublished prefix; earlier outer import/world
 * effects are not rolled back. No dirty/tick/registration occurs here.
 *
 * Input is a readable immutable valid NBT compound/tree; keep every supplying
 * owner/span held until return. No input memory is retained. limits NULL selects
 * defaults. max_input_bytes is unused for compound import, which parses no
 * encoded bytes. Other normal tree limits apply at EXTRA source validation.
 * max_allocation_bytes covers all simultaneous new TileEntity/name/array/stack/tree
 * requests, including old36/new27 array overlap and completed source copies.
 * Caller input owners, other worlds, allocator overhead and bounded C stack are
 * excluded. Every request is admitted before allocation at its reached phase.
 * STATE_ERROR means a native invariant/witness mismatch; never retry or guess.
 * out is required and unchanged on failure. All result/error/limit storage is
 * mutually disjoint and disjoint from inputs and their supplying owners. */
mc_tile_entity_status
mc_tile_entity_import_storage(mc_world_runtime *world, mc_tile_entity_kind resolved_kind,
                              const mc_nbt_tag *compound, const mc_nbt_limits *limits,
                              mc_tile_entity **out, mc_tile_entity_error *error);

/* Source root is immutable/borrowed until final TileEntity release; NULL is safe.
 * source_bytes includes that one exact tree owner/alignment/payload allocation.
 * storage_bytes supports every phase and checked-adds direct TileEntity/array/source,
 * current name, and each UNIQUE installed stack owner/source once. It excludes
 * runtime/world/input/other-container owners. Summing several TileEntity totals is not
 * a global deduplicated accounting scheme. Required out is unchanged on error. */
const mc_nbt_tag *mc_tile_entity_source(const mc_tile_entity *tile_entity);
size_t mc_tile_entity_source_bytes(const mc_tile_entity *tile_entity);
mc_tile_entity_status mc_tile_entity_storage_bytes(const mc_tile_entity *tile_entity, size_t *out);

/* READY only. Build one independent document from the physical slots ascending
 * (including fresh27..35 and nonpositive counts), current base fields or current
 * spawner name/Short Delay, or the nine Piston base/ID/data/facing/previous
 * progress/extending fields. Piston never writes current k or flag j. Null
 * spawner name fails before Delay preparation. Known
 * fields replace retained positions; unknowns/names stay preserved. Current
 * container Slot Byte replaces a moved stack's old Slot. Earlier discarded
 * duplicates/out-of-range records never reappear; no extension keys are added.
 *
 * Hold TileEntity and all input views stable with exclusive access. Shared stack
 * identities count once in the live graph, but each slot gets its own record.
 * max_allocation_bytes charges storage_bytes + pointer scratch + encoded bytes
 * + current codec validation/new-document allocations, which run in sequential
 * phases. max_input_bytes applies to the FINAL encoded-buffer read only.
 * Other owners/worlds/input documents/allocator overhead/C stack are excluded.
 * Normal writer canonicalization applies; mutable supplied-compound partial
 * writes, HashMap wire order and raw wire parity are not claimed. No nested
 * per-item documents, source mutation, callbacks, ticks or registration occur.
 * Result/error/limit storage obeys the same disjointness as import; out is
 * required, unchanged on failure, and freed with mc_nbt_document_free on success.
 * The output slot must not already own an unreleased owner/document. */
mc_tile_entity_status mc_tile_entity_build(const mc_tile_entity *tile_entity,
                                           const mc_nbt_limits *limits, mc_nbt_document **out,
                                           mc_tile_entity_error *error);

/* Native integration contract for the existing actual owners:
 * mc_world_runtime_classes: mc_tile_entity_classes *TileEntities;
 * mc_world_runtime: mc_tile_entity_list_ledger tile_entity_membership[2];
 *                   bool tile_entity_containers_retired;
 * mc_world_chunk_tile_entity_map: mc_tile_entity_map_backing *backing;
 *     void (*release_backing)(struct mc_world_chunk_tile_entity_map *);
 *     bool (*backing_bytes)(const mc_world_chunk *, size_t *);
 * Both new markers/pointers start zero. Capture c/G at their actual constructor
 * field stores with mc_live_tile_entities_capture_list, not after constructor success.
 * The map's existing count remains the source Map size and is validated against
 * real backing nodes. No second World or requested-coordinate dictionary exists.
 */
typedef struct mc_tile_entity_list_ledger {
    mc_world_reference_list *identity;
    mc_tile_entity **holds;
    size_t count, capacity;
    int32_t mod_count;
} mc_tile_entity_list_ledger;

/* Detailed concrete World dirty entry. The legacy bool/status-only wrappers
 * remain available but source-facing dungeon/mutation callers use the detailed
 * variants; no operation-string or ambient diagnostic reconstruction occurs.
 * Presence guards only Chunk dirty; each live typed WorldAccess still receives
 * the same nullable TileEntity. A reached callback temporarily holds a nonnull TileEntity;
 * callers keep its owner live through synchronous reentry. Empty lists do not
 * preflight TileEntity arguments. Failures retain the earlier dirty/callback effects. */
mc_world_storage_status
mc_world_runtime_tile_entity_dirty_detailed(mc_world_runtime *world, int32_t x, int32_t y,
                                            int32_t z, mc_tile_entity *tile_entity,
                                            mc_live_tile_entity_error *error);

/* Source-facing constructors/factory. new performs actual base TileEntity class
 * active use before allocation and the selected complete constructor. Factory
 * performs real registered ID lookup; an unknown ASCII ID diagnostic uses the
 * existing default stdout owner. Missing registered constructors, non-ASCII or
 * over64-byte diagnostics remain explicit reached unsupported operations.
 * preserve=true attaches independent source ownership only after semantic read.
 * No failed call changes out; every completed shared/world prefix is retained.
 */
mc_world_storage_status mc_live_tile_entities_new(mc_world_runtime *world, mc_tile_entity_kind kind,
                                                  mc_tile_entity **out,
                                                  mc_live_tile_entity_error *error);
mc_world_storage_status mc_live_tile_entities_factory(mc_world_runtime *world,
                                                      const mc_nbt_tag *compound,
                                                      const mc_nbt_limits *limits, bool preserve,
                                                      mc_tile_entity **out,
                                                      mc_live_tile_entity_error *error);
/* Resolve a reached code literal without initializing ow/jh. The returned
 * handle retains the actual shared Registry literal identity; even a nullable
 * spawner receiver loads its selected mob literal before setter failure. */
mc_world_storage_status mc_live_tile_entities_literal(mc_world_runtime *world, const char *ascii,
                                                      mc_tile_entity_name **out,
                                                      mc_live_tile_entity_error *error);
const void *mc_live_tile_entities_name_identity(const mc_tile_entity_name *name);

/* These use actual acquiring provider b via runtime_get, normal/blank dispatch,
 * exact Store identity leases, real typed Map values and actual c/G Lists.
 * Nullable get publishes a hold or NULL. set/remove preserve partial publication
 * on later failure; set accepts NULL until its reached invalid getter.
 */
mc_world_storage_status mc_live_tile_entities_get(mc_world_runtime *world, int32_t x, int32_t y,
                                                  int32_t z, mc_tile_entity **out,
                                                  mc_live_tile_entity_error *error);
/* Actual normal lm.d(III) on this exact held canonical Chunk. Coordinates are
 * already local X/Z operands; no World getter, masking or Y gate is inserted.
 * Executes the SAME Map/Block/factory path as the acquiring World route.
 * A nullable result owns one independent TileEntity hold. Keep supplying
 * World/Store/Chunk live and exclusive; native entry adds its own temporary
 * World call/Chunk lease and preserves every reached effect on failure. */
mc_world_storage_status mc_live_tile_entities_chunk_get(mc_world_chunk *chunk, int32_t x, int32_t y,
                                                        int32_t z, mc_tile_entity **out,
                                                        mc_live_tile_entity_error *error);
mc_world_storage_status mc_live_tile_entities_set(mc_world_runtime *world, int32_t x, int32_t y,
                                                  int32_t z, mc_tile_entity *tile_entity,
                                                  mc_live_tile_entity_error *error);
mc_world_storage_status mc_live_tile_entities_remove(mc_world_runtime *world, int32_t x, int32_t y,
                                                     int32_t z, mc_live_tile_entity_error *error);
mc_world_storage_status mc_live_tile_entities_chunk_import(mc_world_chunk *chunk,
                                                           mc_tile_entity *tile_entity,
                                                           mc_live_tile_entity_error *error);
mc_world_storage_status mc_live_tile_entities_chunk_register(mc_world_chunk *chunk,
                                                             mc_live_tile_entity_error *error);
/* Caller holds the actual Chunk and all owned Map occurrences exclusively.
 * Invalidation traverses that Map without reading a source World receiver. */
mc_world_storage_status mc_live_tile_entities_chunk_invalidate(mc_world_chunk *chunk,
                                                               mc_live_tile_entity_error *error);

/* Concrete source TileEntity-block added hook: base return, actual virtual subtype
 * factory, World install. Other registered TileEntity block factories remain explicit
 * reached dependencies, never stand-in Chest or Spawner allocations. */
mc_world_storage_status mc_live_tile_entities_block_added(mc_world_runtime *world,
                                                          const mc_block *block, int32_t x,
                                                          int32_t y, int32_t z,
                                                          mc_live_tile_entity_error *error);
mc_world_storage_status mc_live_tile_entities_block_removed(mc_world_runtime *world,
                                                            const mc_block *block, int32_t x,
                                                            int32_t y, int32_t z,
                                                            mc_live_tile_entity_error *error);

/* Executes the deferred-G phase only, at its actual World call site after
 * successful active TileEntity iteration/L=false. No invented tick or L reset occurs.
 * Known missing spawner/player/mob tick closure is documented separately. */
mc_world_storage_status mc_live_tile_entities_drain_deferred(mc_world_runtime *world,
                                                             mc_live_tile_entity_error *error);

/* Native lower-owner hooks. Validate the COMPLETE graph and Store leases before
 * calling any release. map_release is terminal and installs no gameplay/save
 * success. Its concrete finalizer owns real backing storage and passive source
 * provenance; every published TileEntity value first acquires an actual hold.
 * Classes can retire only after Worlds/TileEntities and external literal handles retire.
 * Class native budget is explicit and separate from World/Chunk budgets.
 */
void mc_live_tile_entities_capture_list(mc_world_runtime *world, bool deferred,
                                        mc_world_reference_list *identity);
bool mc_live_tile_entities_lists_valid(const mc_world_runtime *world);
void mc_live_tile_entities_lists_release(mc_world_runtime *world);
bool mc_live_tile_entities_map_valid(const mc_world_chunk *chunk);
/* Embedded native owner inspection, independent of mutable source l. This
 * neither calls the common Chunk ledger nor performs gameplay/class use. */
bool mc_live_tile_entities_owned_map_bytes(const mc_world_chunk *chunk, size_t *out);
void mc_live_tile_entities_map_release(mc_world_chunk_tile_entity_map *map);
mc_world_storage_status mc_live_tile_entities_classes_retire(mc_world_runtime_classes *classes,
                                                             mc_world_storage_error *error);
mc_world_storage_status mc_live_tile_entities_map_at(const mc_world_chunk *chunk, size_t ordinal,
                                                     mc_tile_entity **out);

typedef struct mc_live_tile_entity_map_entry {
    int32_t x, y, z;             /* Actual immutable local-coordinate Map key. */
    mc_tile_entity *tile_entity; /* Borrowed same value; its coordinates are independent. */
} mc_live_tile_entity_map_entry;

/* Passive entry inspection in actual bucket/chain iteration order. Keep the
 * Chunk and Map unchanged while borrowing the returned TileEntity. This does not
 * retain, initialize classes, acquire chunks or derive keys from TileEntity fields.
 * Failure preserves out; keep output storage disjoint from the owners. */
mc_world_storage_status mc_live_tile_entities_map_entry_at(const mc_world_chunk *chunk,
                                                           size_t ordinal,
                                                           mc_live_tile_entity_map_entry *out);

/* Load the real TileEntities list into this actual held Chunk, preserving
 * successful earlier source imports on failure. A native pending marker blocks
 * serialization after an incomplete attempt. Only exact witnessed successful
 * supported Map publication consumes a retained-record placeholder. Unknown
 * records remain passive provenance; registered unsupported constructors fail.
 * Input is immutable/readable through the full call; no input span is borrowed
 * after success. Complete provenance is copied after semantic imports.
 */
mc_world_storage_status mc_live_tile_entities_load_records(mc_world_chunk *chunk,
                                                           const mc_nbt_tag *records,
                                                           const mc_nbt_limits *limits,
                                                           mc_live_tile_entity_error *error);

/* One independent semantic NBT tree: root Compound with TileEntities List.
 * Rebuild actual Map iteration using live typed fields and Stack Slot overlays.
 * Approved native preservation merge keeps opaque records in relative order,
 * consumes recognized placeholders with the next actual Map record, drops
 * surplus placeholders, then appends remaining Map records. No matching by
 * later coordinates/type/payload and no inactive record resurrection.
 * Exact peak budget includes Chunk direct storage, unique TileEntity/Stack/name/source
 * graph, accounting/views/pointer scratch and new tree validation/allocation.
 * Other Store documents, Worlds, Entity graphs and allocator overhead are
 * excluded. The outer Store composition must deduct this new owned tree.
 * This is not original mutable-compound partial-update or unknown-record order.
 */
mc_world_storage_status mc_live_tile_entities_build_records(const mc_world_chunk *chunk,
                                                            const mc_nbt_limits *limits,
                                                            mc_nbt_tree **out,
                                                            mc_live_tile_entity_error *error);

#endif
