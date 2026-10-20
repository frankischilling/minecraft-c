#ifndef MC_WORLD_RUNTIME_TILES_TILES_H
#define MC_WORLD_RUNTIME_TILES_TILES_H

#include "mc/item/stack/stack.h"
#include "mc/storage/nbt/tree.h"
#include "mc/world/chunk/object.h"
#include "mc/world/runtime/tiles/errors.h"

#include <stdbool.h>

/* Concrete chest/spawner Tile owner. Caller-facing runtime and persistence
 * joins require the coordinated actual owners; reached missing descendants
 * remain explicit failures. Full gameplay/tick qualification is separate. */
typedef struct mc_world_runtime mc_world_runtime;
typedef struct mc_world_runtime_classes mc_world_runtime_classes;
typedef struct mc_world_reference_list mc_world_reference_list;
typedef struct mc_tile mc_tile;
typedef struct mc_tile_name mc_tile_name;
typedef struct mc_tile_classes mc_tile_classes;
typedef struct mc_tile_map_backing mc_tile_map_backing;
typedef enum mc_tile_kind { MC_TILE_CHEST = 0, MC_TILE_SPAWNER } mc_tile_kind;

typedef enum mc_tile_phase {
    MC_TILE_RAW = 0,
    MC_TILE_READY,
    MC_TILE_FAILED_CONSTRUCTION
} mc_tile_phase;

typedef struct mc_tile_name_view {
    bool is_null;
    mc_nbt_string text;
} mc_tile_name_view;

/* Defined in every phase, including failed constructor prefixes. Unused
 * kind-specific fields are zero. The name/world views are borrowed until the
 * next corresponding assignment or final release; hold the name separately
 * across synchronous reentry. Rotation snapshots preserve representation bits. */
typedef struct mc_tile_snapshot {
    mc_tile_kind kind;
    mc_tile_phase phase;
    mc_world_runtime *world;
    int32_t x, y, z;
    bool invalid;
    size_t chest_capacity;
    int32_t spawner_delay;
    uint64_t spawner_rotation_bits, spawner_previous_rotation_bits;
    mc_tile_name *spawner_name;
} mc_tile_snapshot;

/* Concrete World dirty is implemented in this module. Existing persistent
 * counted World bindings prevent runtime_free
 * while a tile retains its world field, even after invalidation/detachment.
 * Retain failure leaves the binding count unchanged and sets runtime last_error.
 * Release requires a counted binding. Dirty enters/protects the real runtime,
 * executes actual acquiring dirty/observer behavior and retains partial effects. */

bool mc_world_runtime_tile_dirty(mc_world_runtime *runtime, int32_t x, int32_t y, int32_t z,
                                 mc_tile *tile);

/* Immutable nullable String owners. Create copies explicit UTF-16 units, with
 * no normalization; NULL units require length0. Empty and NULL are distinct.
 * retain(NULL) succeeds; release(NULL) is safe. View of NULL reports is_null.
 * Views borrow the held name. Interned Pig/empty handles can be immortal and
 * consume no requested heap bytes. No native object identity is copied by a
 * tile name setter; it retains that same handle. */
mc_tile_status mc_tile_name_create(const uint16_t *units, size_t length, mc_tile_name **out);
mc_tile_status mc_tile_name_retain(mc_tile_name *name);
void mc_tile_name_release(mc_tile_name *name);
mc_tile_status mc_tile_name_get_view(const mc_tile_name *name, mc_tile_name_view *out);
size_t mc_tile_name_native_bytes(const mc_tile_name *name);

/* Allocate raw zero fields before constructor work, with one native hold and
 * fixed concrete kind. Construct only RAW: chest allocates36 null physical
 * slots; spawner executes delay-1/previous0/Pig/delay20. Constructor failure
 * consumes RAW and remains FAILED_CONSTRUCTION; no transparent retry. Snapshot,
 * native-byte query and hold/release support every phase. All mutation/semantic
 * calls require READY and a caller hold stable throughout synchronous reentry.
 * Result/error storage is disjoint from owners, input and each other; failure
 * leaves outputs unchanged except explicit error records and actual prefixes.
 * All access is exclusive; no thread synchronization or arbitrary stale-pointer
 * detection is provided. */
mc_tile_status mc_tile_allocate(mc_world_runtime *world, mc_tile_kind kind, mc_tile **out);
mc_tile_status mc_tile_construct(mc_tile *raw);
mc_tile_status mc_tile_construct_detailed(mc_tile *raw, mc_tile_error *error);
mc_tile_status mc_tile_retain(mc_tile *tile);
void mc_tile_release(mc_tile *tile);
mc_tile_status mc_tile_get_snapshot(const mc_tile *tile, mc_tile_snapshot *out);

/* Exact direct requested heap bytes: common owner plus its physical slot array.
 * Name handles and ItemStack owners are independent held identities, excluded
 * here and exposed separately; callers count shared owners once. Whole tile
 * provenance is also excluded here and exposed below. This is not a total
 * world/process memory query. */
size_t mc_tile_native_bytes(const mc_tile *tile);

/* bind_world acquires a new persistent runtime binding before pointer store,
 * then releases the previous binding; same-pointer store does not recount.
 * NULL clears only through this explicit native operation. Invalidation and
 * detachment never clear world or coordinates. These separate operations let
 * actual chunk install retain original world->coords->admission/map order. */
mc_tile_status mc_tile_bind_world(mc_tile *tile, mc_world_runtime *world);
mc_tile_status mc_tile_set_coordinates(mc_tile *tile, int32_t x, int32_t y, int32_t z);
mc_tile_status mc_tile_set_invalid(mc_tile *tile, bool invalid);
mc_tile_status mc_tile_dirty(mc_tile *tile);
mc_tile_status mc_tile_dirty_detailed(mc_tile *tile, mc_live_tile_error *error);

/* Chest logical size27 and limit64 differ from physical capacity36 before load,
 * 27 after its reached NBT array replacement. Slot access checks PHYSICAL bounds.
 * A successful getter publishes a hold on the SAME nullable stack identity. */
mc_tile_status mc_tile_chest_size(const mc_tile *tile, int32_t *out);
mc_tile_status mc_tile_chest_limit(const mc_tile *tile, int32_t *out);
mc_tile_status mc_tile_chest_capacity(const mc_tile *tile, size_t *out);
mc_tile_status mc_tile_chest_get(mc_tile *tile, int32_t slot, mc_item_stack **out);

/* Retain/install the same supplied stack, release only the previous slot hold,
 * then clamp count>64 and perform real dirty. Null is accepted; other counts
 * remain unchanged. No copy/item-max query. Later failure keeps install/clamp.
 * Remove returns same owner when count<=amount, else actual ItemStack split;
 * exact-zero source count clears slot. Dirty follows mutation. An empty slot
 * returns successful NULL without dirty. On failure result remains unchanged;
 * earlier slot/count/world effects remain and only private result holds release.
 * A stack installed by this API must be READY. No amount/ID/damage range clamp. */
mc_tile_status mc_tile_chest_set(mc_tile *tile, int32_t slot, mc_item_stack *stack);
mc_tile_status mc_tile_chest_set_detailed(mc_tile *tile, int32_t slot, mc_item_stack *stack,
                                          mc_live_tile_error *error);
mc_tile_status mc_tile_chest_remove(mc_tile *tile, int32_t slot, int32_t amount,
                                    mc_item_stack **out);
mc_tile_status mc_tile_chest_remove_detailed(mc_tile *tile, int32_t slot, int32_t amount,
                                             mc_item_stack **out, mc_live_tile_error *error);
/* Detailed variants publish fresh errors and preserve the actual nested dirty
 * status/operation/World/observer frame through late failures after install,
 * clamp, clear or split. They preserve the old status-only wrappers for native
 * callers; dungeon/mutation integration uses the detailed entries explicitly. */

/* Field-only name assignment, no dirty, update or RNG. Getter publishes a hold
 * on the same nullable name handle. Delay/rotation inspection uses snapshot. */
mc_tile_status mc_tile_spawner_get_name(mc_tile *tile, mc_tile_name **out);
mc_tile_status mc_tile_spawner_set_name(mc_tile *tile, mc_tile_name *name);

/* Ordered semantic read on READY: base Int x/y/z, then subtype fields. Missing
 * exact-typed fields use original defaults; wrong present types fail at their
 * own reached lookup. Failure keeps READY and completed field/array/slot prefix.
 * Base read does not touch world/invalid. Chest queries Items BEFORE allocating
 * and replacing physical storage with27 null slots; then installs each valid
 * Slot Byte&255 record in list order, last duplicate wins. No clamp/dirty. New
 * stacks are constructed before their actual slot assignment. Spawner owns its
 * new EntityId value before reading Short Delay, retaining coordinate/name prefix
 * on later failure. Input is an immutable valid NBT tree. The caller must keep
 * every input owner and span alive throughout the call and all partial prefixes,
 * including replacement releasing old slot holds and their stack/source owners.
 * Input borrowing such a source requires an independent stack/source hold;
 * ownership solely through the old chest slot is insufficient. Independently
 * owned decoded NBT already meets this rule. Input may be released after return:
 * strings/stack fields do not borrow it.
 *
 * This semantic operation does not attach or replace provenance. The explicit
 * storage import below composes it with EXTRA independent source ownership. */
mc_tile_status mc_tile_read_base(mc_tile *tile, const mc_nbt_tag *compound, mc_tile_error *error);
mc_tile_status mc_tile_read_nbt(mc_tile *tile, const mc_nbt_tag *compound, mc_tile_error *error);

/* Explicit storage composition, not an alternate tile registry. resolved_kind
 * must come from the actual concrete factory decision. Construct/read a new
 * unpublished tile, capture each final stack/Items-index association DURING
 * assignment, then copy the whole tile and independently attach each winning
 * copied child to that SAME stack identity. Whole copy first canonicalizes
 * ignored list-child names. Only complete success publishes a READY, unbound
 * owner. Failure releases every unpublished prefix; earlier outer import/world
 * effects are not rolled back. No dirty/tick/registration occurs here.
 *
 * Input is a readable immutable valid NBT compound/tree; keep every supplying
 * owner/span held until return. No input memory is retained. limits NULL selects
 * defaults. max_input_bytes is unused for compound import, which parses no
 * encoded bytes. Other normal tree limits apply at EXTRA source validation.
 * max_allocation_bytes covers all simultaneous new tile/name/array/stack/tree
 * requests, including old36/new27 array overlap and completed source copies.
 * Caller input owners, other worlds, allocator overhead and bounded C stack are
 * excluded. Every request is admitted before allocation at its reached phase.
 * STATE_ERROR means a native invariant/witness mismatch; never retry or guess.
 * out is required and unchanged on failure. All result/error/limit storage is
 * mutually disjoint and disjoint from inputs and their supplying owners. */
mc_tile_status mc_tile_import_storage(mc_world_runtime *world, mc_tile_kind resolved_kind,
                                      const mc_nbt_tag *compound, const mc_nbt_limits *limits,
                                      mc_tile **out, mc_tile_error *error);

/* Source root is immutable/borrowed until final tile release; NULL is safe.
 * source_bytes includes that one exact tree owner/alignment/payload allocation.
 * storage_bytes supports every phase and checked-adds direct tile/array/source,
 * current name, and each UNIQUE installed stack owner/source once. It excludes
 * runtime/world/input/other-container owners. Summing several tile totals is not
 * a global deduplicated accounting scheme. Required out is unchanged on error. */
const mc_nbt_tag *mc_tile_source(const mc_tile *tile);
size_t mc_tile_source_bytes(const mc_tile *tile);
mc_tile_status mc_tile_storage_bytes(const mc_tile *tile, size_t *out);

/* READY only. Build one independent document from the physical slots ascending
 * (including fresh27..35 and nonpositive counts), current base fields or current
 * spawner name/Short Delay. Null name fails before Delay preparation. Known
 * fields replace retained positions; unknowns/names stay preserved. Current
 * container Slot Byte replaces a moved stack's old Slot. Earlier discarded
 * duplicates/out-of-range records never reappear; no extension keys are added.
 *
 * Hold tile and all input views stable with exclusive access. Shared stack
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
mc_tile_status mc_tile_build(const mc_tile *tile, const mc_nbt_limits *limits,
                             mc_nbt_document **out, mc_tile_error *error);

/* Native integration contract for the existing actual owners:
 * mc_world_runtime_classes: mc_tile_classes *tiles;
 * mc_world_runtime: mc_tile_list_ledger tile_membership[2];
 *                   bool tile_containers_retired;
 * mc_world_chunk_tile_map: mc_tile_map_backing *backing;
 *     void (*release_backing)(struct mc_world_chunk_tile_map *);
 *     bool (*backing_bytes)(const mc_world_chunk *, size_t *);
 * Both new markers/pointers start zero. Capture c/G at their actual constructor
 * field stores with mc_live_tiles_capture_list, not after constructor success.
 * The map's existing count remains the source Map size and is validated against
 * real backing nodes. No second World or requested-coordinate dictionary exists.
 */
typedef struct mc_tile_list_ledger {
    mc_world_reference_list *identity;
    mc_tile **holds;
    size_t count, capacity;
    int32_t mod_count;
} mc_tile_list_ledger;

/* Detailed concrete World dirty entry. The legacy bool/status-only wrappers
 * remain available but source-facing dungeon/mutation callers use the detailed
 * variants; no operation-string or ambient diagnostic reconstruction occurs. */
mc_world_storage_status mc_world_runtime_tile_dirty_detailed(mc_world_runtime *world, int32_t x,
                                                             int32_t y, int32_t z, mc_tile *tile,
                                                             mc_live_tile_error *error);

/* Source-facing constructors/factory. new performs actual base Tile class
 * active use before allocation and the selected complete constructor. Factory
 * performs real registered ID lookup; an unknown ASCII ID diagnostic uses the
 * existing default stdout owner. Missing registered constructors, non-ASCII or
 * over64-byte diagnostics remain explicit reached unsupported operations.
 * preserve=true attaches independent source ownership only after semantic read.
 * No failed call changes out; every completed shared/world prefix is retained.
 */
mc_world_storage_status mc_live_tiles_new(mc_world_runtime *world, mc_tile_kind kind, mc_tile **out,
                                          mc_live_tile_error *error);
mc_world_storage_status mc_live_tiles_factory(mc_world_runtime *world, const mc_nbt_tag *compound,
                                              const mc_nbt_limits *limits, bool preserve,
                                              mc_tile **out, mc_live_tile_error *error);
/* Resolve a reached code literal without initializing ow/jh. The returned
 * handle retains the actual shared Registry literal identity; even a nullable
 * spawner receiver loads its selected mob literal before setter failure. */
mc_world_storage_status mc_live_tiles_literal(mc_world_runtime *world, const char *ascii,
                                              mc_tile_name **out, mc_live_tile_error *error);
const void *mc_live_tiles_name_identity(const mc_tile_name *name);

/* These use actual acquiring provider b via runtime_get, normal/blank dispatch,
 * exact Store identity leases, real typed Map values and actual c/G Lists.
 * Nullable get publishes a hold or NULL. set/remove preserve partial publication
 * on later failure; set accepts NULL until its reached invalid getter.
 */
mc_world_storage_status mc_live_tiles_get(mc_world_runtime *world, int32_t x, int32_t y, int32_t z,
                                          mc_tile **out, mc_live_tile_error *error);
mc_world_storage_status mc_live_tiles_set(mc_world_runtime *world, int32_t x, int32_t y, int32_t z,
                                          mc_tile *tile, mc_live_tile_error *error);
mc_world_storage_status mc_live_tiles_remove(mc_world_runtime *world, int32_t x, int32_t y,
                                             int32_t z, mc_live_tile_error *error);
mc_world_storage_status mc_live_tiles_chunk_import(mc_world_chunk *chunk, mc_tile *tile,
                                                   mc_live_tile_error *error);
mc_world_storage_status mc_live_tiles_chunk_register(mc_world_chunk *chunk,
                                                     mc_live_tile_error *error);
/* Caller holds the actual Chunk and all owned Map occurrences exclusively.
 * Invalidation traverses that Map without reading a source World receiver. */
mc_world_storage_status mc_live_tiles_chunk_invalidate(mc_world_chunk *chunk,
                                                       mc_live_tile_error *error);

/* Concrete source tile-block added hook: base return, actual virtual subtype
 * factory, World install. Other registered tile block factories remain explicit
 * reached dependencies, never stand-in Chest or Spawner allocations. */
mc_world_storage_status mc_live_tiles_block_added(mc_world_runtime *world, const mc_block *block,
                                                  int32_t x, int32_t y, int32_t z,
                                                  mc_live_tile_error *error);
mc_world_storage_status mc_live_tiles_block_removed(mc_world_runtime *world, const mc_block *block,
                                                    int32_t x, int32_t y, int32_t z,
                                                    mc_live_tile_error *error);

/* Executes the deferred-G phase only, at its actual World call site after
 * successful active tile iteration/L=false. No invented tick or L reset occurs.
 * Known missing spawner/player/mob tick closure is documented separately. */
mc_world_storage_status mc_live_tiles_drain_deferred(mc_world_runtime *world,
                                                     mc_live_tile_error *error);

/* Native lower-owner hooks. Validate the COMPLETE graph and Store leases before
 * calling any release. map_release is terminal and installs no gameplay/save
 * success. Its concrete finalizer owns real backing storage and passive source
 * provenance; every published Tile value first acquires an actual hold.
 * Classes can retire only after Worlds/Tiles and external literal handles retire.
 * Class native budget is explicit and separate from World/Chunk budgets.
 */
void mc_live_tiles_capture_list(mc_world_runtime *world, bool deferred,
                                mc_world_reference_list *identity);
bool mc_live_tiles_lists_valid(const mc_world_runtime *world);
void mc_live_tiles_lists_release(mc_world_runtime *world);
bool mc_live_tiles_map_valid(const mc_world_chunk *chunk);
/* Embedded native owner inspection, independent of mutable source l. This
 * neither calls the common Chunk ledger nor performs gameplay/class use. */
bool mc_live_tiles_owned_map_bytes(const mc_world_chunk *chunk, size_t *out);
void mc_live_tiles_map_release(mc_world_chunk_tile_map *map);
mc_world_storage_status mc_live_tiles_classes_retire(mc_world_runtime_classes *classes,
                                                     mc_world_storage_error *error);
mc_world_storage_status mc_live_tiles_map_at(const mc_world_chunk *chunk, size_t ordinal,
                                             mc_tile **out);

typedef struct mc_live_tile_map_entry {
    int32_t x, y, z; /* Actual immutable local-coordinate Map key. */
    mc_tile *tile;   /* Borrowed same value; its coordinates are independent. */
} mc_live_tile_map_entry;

/* Passive entry inspection in actual bucket/chain iteration order. Keep the
 * Chunk and Map unchanged while borrowing the returned Tile. This does not
 * retain, initialize classes, acquire chunks or derive keys from Tile fields.
 * Failure preserves out; keep output storage disjoint from the owners. */
mc_world_storage_status mc_live_tiles_map_entry_at(const mc_world_chunk *chunk, size_t ordinal,
                                                   mc_live_tile_map_entry *out);

/* Load the real TileEntities list into this actual held Chunk, preserving
 * successful earlier source imports on failure. A native pending marker blocks
 * serialization after an incomplete attempt. Only exact witnessed successful
 * supported Map publication consumes a retained-record placeholder. Unknown
 * records remain passive provenance; registered unsupported constructors fail.
 * Input is immutable/readable through the full call; no input span is borrowed
 * after success. Complete provenance is copied after semantic imports.
 */
mc_world_storage_status mc_live_tiles_load_records(mc_world_chunk *chunk, const mc_nbt_tag *records,
                                                   const mc_nbt_limits *limits,
                                                   mc_live_tile_error *error);

/* One independent semantic NBT tree: root Compound with TileEntities List.
 * Rebuild actual Map iteration using live typed fields and Stack Slot overlays.
 * Approved native preservation merge keeps opaque records in relative order,
 * consumes recognized placeholders with the next actual Map record, drops
 * surplus placeholders, then appends remaining Map records. No matching by
 * later coordinates/type/payload and no inactive record resurrection.
 * Exact peak budget includes Chunk direct storage, unique Tile/Stack/name/source
 * graph, accounting/views/pointer scratch and new tree validation/allocation.
 * Other Store documents, Worlds, Entity graphs and allocator overhead are
 * excluded. The outer Store composition must deduct this new owned tree.
 * This is not original mutable-compound partial-update or unknown-record order.
 */
mc_world_storage_status mc_live_tiles_build_records(const mc_world_chunk *chunk,
                                                    const mc_nbt_limits *limits, mc_nbt_tree **out,
                                                    mc_live_tile_error *error);

#endif
