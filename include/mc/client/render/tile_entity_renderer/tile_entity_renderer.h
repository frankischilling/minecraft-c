#ifndef MC_CLIENT_RENDER_TILE_ENTITY_RENDERER_H
#define MC_CLIENT_RENDER_TILE_ENTITY_RENDERER_H

#include "mc/client/render/model.h"
#include "mc/client/render_engine/render_engine.h"
#include "mc/world/runtime/tile_entities/tile_entities.h"

typedef struct mc_client_tile_renderer_classes mc_client_tile_renderer_classes;
typedef struct mc_client_tile_renderer_dispatcher mc_client_tile_renderer_dispatcher;
typedef struct mc_client_tile_renderer mc_client_tile_renderer;
typedef struct mc_client_tile_renderer_cv mc_client_tile_renderer_cv;
typedef struct mc_client_render_engine mc_client_render_engine;
typedef struct mc_client_bitmap_font mc_client_bitmap_font;

typedef enum mc_client_tile_renderer_status {
    MC_CLIENT_TILE_RENDERER_OK = 0,
    MC_CLIENT_TILE_RENDERER_INVALID_ARGUMENT,
    MC_CLIENT_TILE_RENDERER_INVALID_PHASE,
    MC_CLIENT_TILE_RENDERER_BUSY,
    MC_CLIENT_TILE_RENDERER_LIMIT,
    MC_CLIENT_TILE_RENDERER_ALLOCATION_FAILED,
    MC_CLIENT_TILE_RENDERER_MODEL_FAILED,
    MC_CLIENT_TILE_RENDERER_TILE_ENTITY_FAILED,
    MC_CLIENT_TILE_RENDERER_NULL_REFERENCE,
    MC_CLIENT_TILE_RENDERER_CLASS_ERRONEOUS,
    MC_CLIENT_TILE_RENDERER_WORLD_FAILED,
    MC_CLIENT_TILE_RENDERER_ENGINE_FAILED,
    MC_CLIENT_TILE_RENDERER_FONT_FAILED,
    MC_CLIENT_TILE_RENDERER_ENTITY_FAILED,
    MC_CLIENT_TILE_RENDERER_UNSUPPORTED_FP,
    MC_CLIENT_TILE_RENDERER_UNSUPPORTED_NUMERIC
} mc_client_tile_renderer_status;

typedef enum mc_client_tile_renderer_phase {
    MC_CLIENT_TILE_RENDERER_COLD = 0,
    MC_CLIENT_TILE_RENDERER_CONSTRUCTING,
    MC_CLIENT_TILE_RENDERER_READY,
    MC_CLIENT_TILE_RENDERER_FAILED,
    MC_CLIENT_TILE_RENDERER_RETIRING
} mc_client_tile_renderer_phase;

typedef struct mc_client_tile_renderer_error {
    mc_client_tile_renderer_status status, initial_status;
    const char *operation, *source_class, *source_method, *source_descriptor;
    int32_t pc, ll_pc, renderer_pc, rf_pc, cv_pc, frame_pc;
    bool native_policy;
    mc_client_model_error model;
    mc_tile_entity_status tile_entity;
    mc_world_storage_error world;
    mc_client_render_engine_status engine_status;
    mc_entity_status entity_status;
    int font_status; /* Actual private BitmapFont status; no copied Font error layout. */
} mc_client_tile_renderer_error;

typedef struct mc_client_tile_renderer_options {
    size_t class_bytes, renderer_bytes;
} mc_client_tile_renderer_options;

typedef struct mc_client_tile_renderer_consumer {
    mc_client_tile_renderer_classes *classes;
    bool attached;
} mc_client_tile_renderer_consumer;

/* Actual RF storage: two PS fields, with no KO/FH inheritance or copied model. */
typedef struct mc_client_sign_model {
    mc_client_model_part *a, *b;
} mc_client_sign_model;

typedef struct mc_client_tile_renderer_fields {
    const void *m; /* Actual Class-key Map identity, published at LL constructor12. */
    mc_client_bitmap_font *n;
    mc_client_render_engine *e;
    mc_world_runtime *f;
    mc_entity *g;
    float h, i;
    double j, k, l;
} mc_client_tile_renderer_fields;

typedef struct mc_client_tile_renderer_map_entry {
    bool present;
    const void *identity; /* Same node on a replacement put of an existing key. */
    const mc_tile_entity_class_literal *key;
    mc_client_tile_renderer *value; /* An actual nullable JE renderer. */
} mc_client_tile_renderer_map_entry;

typedef struct mc_client_tile_renderer_state {
    mc_client_tile_renderer_phase phase;
    const mc_client_tile_renderer_dispatcher *a, *candidate;
    const mc_tile_entity_classes *tile_classes;
    const mc_world_runtime_classes *world_classes;
    double b, c, d;
    size_t native_consumers, class_bytes, dispatcher_holds;
    size_t entries, capacity, threshold, null_values, allocated_renderers;
    size_t put_calls, completed_backpointers;
    uint32_t map_mod_count;
    int32_t last_put_pc, last_constructor_pc, last_frame_pc;
    size_t retained_engines, retained_fonts;
    bool living_retained;
    bool source_views_available;
    mc_client_tile_renderer_fields fields;
    const mc_client_tile_renderer_error *failure, *retirement;
} mc_client_tile_renderer_state;

typedef struct mc_client_tile_renderer_object_state {
    const char *source_class;
    const void *identity;
    mc_client_model_phase phase;
    size_t holds, allocation_bytes, graph_allocations, vectors;
    const mc_client_tile_renderer_dispatcher *manager; /* Actual JE.a, borrowed. */
    const mc_client_sign_model *sign_model, *sign_candidate;
    mc_client_model_phase sign_phase;
    const void *spawner_map, *spawner_map_candidate;
    size_t spawner_entries, spawner_capacity, spawner_threshold;
    const mc_client_render_blocks *piston_blocks;
    const mc_client_tile_renderer_cv *piston_cv;
    size_t live_cvs, retained_cv_holds;
    const mc_client_model_error *failure;
} mc_client_tile_renderer_object_state;

typedef struct mc_client_tile_renderer_cv_state {
    const mc_client_tile_renderer *owner;
    const mc_client_render_blocks *fields; /* The actual embedded CV, not a snapshot. */
    const void *identity;                  /* Same actual CV identity as fields. */
    mc_world_runtime *world_occurrence;    /* Independent native occurrence backing c. */
    size_t external_holds, allocation_bytes;
    bool current, static_ready, static_a; /* a is the SAME shared CV Class field. */
} mc_client_tile_renderer_cv_state;

/* One LL namespace associated with the SAME RenderClasses, matching CLIENT
 * World/Entity/Profile owners. Bind retains one counted borrow of the existing
 * TileEntity namespace; it initializes no source class, Map, model, Vec3, World,
 * Registry, Random or CV. Registry and Entity bindings must already be present
 * on World classes and match the model owner's actual Entity/profile pair.
 * Rebinding the exact dependencies/options is idempotent; replacement or a
 * terminally retired association is rejected. No counted self-reference is
 * added to RenderClasses; its association prevents destruction until retire.
 * Class bytes charge native namespace/dispatcher/Map/renderer storage;
 * renderer bytes separately bound each renderer header plus graph records.
 * Canonical Vec3 allocations use the existing separate Entity ledger. Native
 * limits/allocation timing do not claim JVM heap or Java OOME equivalence. */
mc_client_tile_renderer_options mc_client_tile_renderer_default_options(void);
mc_client_tile_renderer_status mc_client_tile_renderer_classes_bind(
    mc_client_render_classes *models, mc_world_runtime_classes *world_classes,
    mc_java_random_profile *profile, const mc_client_tile_renderer_options *options,
    mc_client_tile_renderer_classes **out, mc_client_tile_renderer_error *error);
mc_client_tile_renderer_status
mc_client_tile_renderer_consumer_attach(mc_client_tile_renderer_classes *,
                                        mc_client_tile_renderer_consumer *,
                                        mc_client_tile_renderer_error *);
/* Native source-admission association only, not a counted ownership cycle. */
mc_client_tile_renderer_status
mc_client_tile_renderer_consumer_detach(mc_client_tile_renderer_consumer *,
                                        mc_client_tile_renderer_error *);

/* Actual LL active use: complete all three JE constructors and real RF geometry,
 * then JE.a(same LL) for every Map value, before publishing LL.a at clinit7.
 * One attempt; failed candidate/children/partial publications remain owned and
 * inspectable. Later active use returns CLASS_ERRONEOUS with the first cause.
 * The returned singleton is borrowed; retain before storing another occurrence. */
mc_client_tile_renderer_status
mc_client_tile_renderer_static_get(mc_client_tile_renderer_classes *,
                                   mc_client_tile_renderer_dispatcher **out,
                                   mc_client_tile_renderer_error *);
mc_client_tile_renderer_status
mc_client_tile_renderer_dispatcher_retain(mc_client_tile_renderer_dispatcher *,
                                          mc_client_tile_renderer_error *);
mc_client_tile_renderer_status
mc_client_tile_renderer_dispatcher_release(mc_client_tile_renderer_dispatcher **,
                                           mc_client_tile_renderer_error *);
mc_client_tile_renderer_status mc_client_tile_renderer_retain(mc_client_tile_renderer *,
                                                              mc_client_tile_renderer_error *);
mc_client_tile_renderer_status mc_client_tile_renderer_release(mc_client_tile_renderer **,
                                                               mc_client_tile_renderer_error *);

/* Exact LL Class lookup: Map.get5, OW comparison21, real superclass26/recursive
 * call29, Map.put39 even for a nullable result. Cached null values are looked up
 * recursively again; replacement preserves the original Map node identity.
 * NULL Class reaches source NULL_REFERENCE at26 without changing the Map.
 * Nonnull keys must belong to this exact held TileEntity namespace; a foreign
 * namespace is a native INVALID_ARGUMENT, not an invented inherited result.
 * lookup_tile(NULL) returns NULL and has_renderer(NULL) returns false, matching
 * LL.b(OW)'s early return5. Other instances use their actual fixed Class cell,
 * with no initialization/construction or instance-kind ordinal conversion.
 * Failed ordinary lookups retain already completed Map effects and do not
 * poison the successfully initialized LL class. Outputs preserve on failure. */
mc_client_tile_renderer_status mc_client_tile_renderer_lookup_class(
    mc_client_tile_renderer_dispatcher *, const mc_tile_entity_class_literal *nullable_class,
    mc_client_tile_renderer **nullable_renderer, mc_client_tile_renderer_error *);
mc_client_tile_renderer_status mc_client_tile_renderer_lookup_tile(
    mc_client_tile_renderer_dispatcher *, const mc_tile_entity *nullable_tile,
    mc_client_tile_renderer **nullable_renderer, mc_client_tile_renderer_error *);
mc_client_tile_renderer_status
mc_client_tile_renderer_has_renderer(mc_client_tile_renderer_dispatcher *,
                                     const mc_tile_entity *nullable_tile, bool *out,
                                     mc_client_tile_renderer_error *);

/* Exact ll.a(fd): store f2, then iterate THIS Map's values and invoke each
 * nonnull JE World callback45. PO/AG inherit JE's return-only body; HY performs
 * genuine CV active use/new1, constructor6 with the supplied nullable World as
 * XP, then b9. This direct operation has no same-World/null identity shortcut:
 * every reached HY occurrence creates another actual CV. The frame method's
 * separate identity guard is not supplied by this API.
 * Nonnull inputs must be actual GAME Worlds of this exact World-class owner;
 * RAW/failed construction phases may be stored if native binding retain admits
 * them. One persistent native occurrence backs f and one backs each live CV.c.
 * A successful new b store ends the old field occurrence; an unretained old CV
 * is freed immediately, while a retained displaced CV keeps its World alive.
 * Native CV quota admission charges renderer_bytes BEFORE dropping the old b.
 * A callback failure preserves f2 and earlier callback effects, leaves LL READY,
 * and returns its typed child cause without rolling back source stores. */
mc_client_tile_renderer_status
mc_client_tile_renderer_change_world(mc_client_tile_renderer_dispatcher *,
                                     mc_world_runtime *nullable_world,
                                     mc_client_tile_renderer_error *);

/* Actual ll.a(fd,ji,sj,ls,F): compare f first and call the real World body10
 * only when different, then store e15/g21/n26 before the first Living read32.
 * A null Living therefore preserves all three reference stores before source
 * NULL_REFERENCE32. Nonnull Living must be from this exact Entity namespace
 * and assignable to the actual Living Class; no READY-only restriction is added.
 * Engine/Font inputs are actual nullable owners, never constructed by this call.
 * Native retention/quota failures occur at each reached reference store.
 * Distinct Engine/Font owners remain retained until outer LL CPU retirement;
 * replacing a source field performs no native Font/resource cleanup. Living
 * uses one current counted occurrence, retained before its old occurrence ends.
 *
 * Strict x86 floating admission begins at fsub45, after all reference stores
 * and the null read. h50/i74 use ordered binary32 subtract/multiply/add; j99,
 * k124/l149 use ordered binary64 operations with the source float widening.
 * Each component publishes separately. The measured x86-64 JDK17 profile
 * preserves NaN sign/payload, signed zero, subnormals and intermediate overflow.
 * An unmeasured two-NaN operation with at least one signaling operand returns
 * native UNSUPPORTED_NUMERIC at that operation, preserving earlier components.
 * Floating exception flags are not a value parity claim. No partial0/1 shortcut,
 * Math/Vec3 allocation, draw, source class retry or World reset occurs.
 * frame_pc records this method's failing operation (parent10 for a World child);
 * the existing child fields retain their original source site. */
mc_client_tile_renderer_status mc_client_tile_renderer_bind_frame(
    mc_client_tile_renderer_dispatcher *, mc_world_runtime *nullable_world,
    mc_client_render_engine *nullable_engine, mc_client_bitmap_font *nullable_font,
    mc_entity *nullable_living, float partial, mc_client_tile_renderer_error *);

/* Native observation holds on current HY.b. A missing current b returns NULL
 * successfully; PO/AG are INVALID_ARGUMENT. Each nonnull result owns one hold
 * and must be released exactly once. Handles preserve the actual CV object and
 * World lifetime across replacement; they do not freeze its field contents.
 * Any external CV hold blocks LL class retirement BEFORE graph mutation.
 * release consumes/nulls one occurrence; a final displaced hold frees its CV
 * and releases its World, while a current CV retains its source-field owner.
 * Inspection fields/identity point into the real owned wrapper; borrowed
 * current views expire on replacement and retained views require that handle
 * to remain held. All calls require exclusive quiescent live owners. */
mc_client_tile_renderer_status
mc_client_tile_renderer_cv_retain_current(mc_client_tile_renderer *,
                                          mc_client_tile_renderer_cv **nullable_out,
                                          mc_client_tile_renderer_error *);
mc_client_tile_renderer_status mc_client_tile_renderer_cv_release(mc_client_tile_renderer_cv **,
                                                                  mc_client_tile_renderer_error *);
bool mc_client_tile_renderer_cv_inspect(const mc_client_tile_renderer_cv *,
                                        mc_client_tile_renderer_cv_state *);

/* Passive native inspection, with no source Map lookup/cache writes or active
 * use. Entry enumeration is native bucket order; JDK identity hashes/iteration
 * order are not claimed. map_find distinguishes missing keys from cached null.
 * allocated selects the pending/failed/complete renderer allocation ledger in
 * constructor allocation order; its result is borrowed, like Map entry values.
 * Views require exclusive quiescent live owners and expire on relevant mutation
 * or terminal retirement. Retain the renderer/dispatcher before persistent use.
 * Inspection of a failed initializer never makes it READY. */
bool mc_client_tile_renderer_inspect(const mc_client_tile_renderer_classes *,
                                     mc_client_tile_renderer_state *);
bool mc_client_tile_renderer_object_inspect(const mc_client_tile_renderer *,
                                            mc_client_tile_renderer_object_state *);
mc_client_tile_renderer_status mc_client_tile_renderer_entry(mc_client_tile_renderer_dispatcher *,
                                                             size_t index,
                                                             mc_client_tile_renderer_map_entry *out,
                                                             mc_client_tile_renderer_error *);
mc_client_tile_renderer_status mc_client_tile_renderer_map_find(
    mc_client_tile_renderer_dispatcher *, const mc_tile_entity_class_literal *nullable_class,
    mc_client_tile_renderer_map_entry *out, mc_client_tile_renderer_error *);
mc_client_tile_renderer_status
mc_client_tile_renderer_allocated(mc_client_tile_renderer_dispatcher *, size_t index,
                                  mc_client_tile_renderer **out, mc_client_tile_renderer_error *);

/* Explicit native CPU retirement. Requires no consumers/operations/external
 * dispatcher or renderer holds. Admission precedes releasing graph children;
 * once RETIRING begins source views end, cleanup is allocation-free/retryable.
 * The canonical TileEntity borrow ends only after the entire cached/pending
 * graph is gone. Retained Font occurrences end before Engine occurrences,
 * under the LL active guard; failed cleanup preserves its exact pending slots.
 * Keep the actual Display/GUI/graphics owners alive through this operation.
 * Retire LL before outer strict Font/Engine retirement and Display destruction;
 * unrelated graphics resource boundaries remain the actual owner's policy.
 * No source globals are reset and no failed active use retried.
 * All APIs require exclusive live dependencies and disjoint outputs/errors/
 * slots. NULL *slot is harmless; success consumes/nulls the caller's slot. */
mc_client_tile_renderer_status
mc_client_tile_renderer_classes_retire(mc_client_tile_renderer_classes **,
                                       mc_client_tile_renderer_error *);

#endif
