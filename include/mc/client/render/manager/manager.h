#ifndef MC_CLIENT_RENDER_MANAGER_H
#define MC_CLIENT_RENDER_MANAGER_H
#include "mc/block/registry/runtime.h"
#include "mc/client/render/model.h"
#include "mc/client/render_engine/render_engine.h"
#include "mc/client/world/entity_renderer.h"
#include "mc/core/math/classes.h"
#include "mc/entity/player/player.h"

typedef struct mc_client_bitmap_font mc_client_bitmap_font;
typedef struct mc_client_render_engine mc_client_render_engine;
typedef struct mc_game_settings mc_game_settings;
typedef struct mc_client_render_manager_classes mc_client_render_manager_classes;
typedef struct mc_client_render_manager mc_client_render_manager;
typedef struct mc_client_managed_renderer mc_client_managed_renderer;
typedef struct mc_client_held_item mc_client_held_item;
typedef struct mc_client_source_ra mc_client_source_ra;
typedef struct mc_client_bitmap_font_error mc_client_bitmap_font_error;
typedef struct mc_world_runtime_acquisition_error mc_world_runtime_acquisition_error;

typedef enum mc_client_manager_status {
    MC_CLIENT_MANAGER_OK,
    MC_CLIENT_MANAGER_INVALID_ARGUMENT,
    MC_CLIENT_MANAGER_INVALID_PHASE,
    MC_CLIENT_MANAGER_BUSY,
    MC_CLIENT_MANAGER_LIMIT,
    MC_CLIENT_MANAGER_ALLOCATION_FAILED,
    MC_CLIENT_MANAGER_MODEL_FAILED,
    MC_CLIENT_MANAGER_REGISTRY_FAILED,
    MC_CLIENT_MANAGER_STRING_FAILED,
    MC_CLIENT_MANAGER_NULL_REFERENCE,
    MC_CLIENT_MANAGER_CLASS_ERRONEOUS,
    MC_CLIENT_MANAGER_HAND_FAILED,
    MC_CLIENT_MANAGER_WORLD_FAILED,
    MC_CLIENT_MANAGER_MATH_FAILED,
    MC_CLIENT_MANAGER_PLAYER_FAILED,
    MC_CLIENT_MANAGER_ENTITY_FAILED,
    MC_CLIENT_MANAGER_ENGINE_FAILED,
    MC_CLIENT_MANAGER_FONT_FAILED,
    MC_CLIENT_MANAGER_SETTINGS_FAILED,
    MC_CLIENT_MANAGER_UNSUPPORTED_FP,
    MC_CLIENT_MANAGER_UNSUPPORTED_NUMERIC,
    MC_CLIENT_MANAGER_UNIMPLEMENTED_DEPENDENCY
} mc_client_manager_status;

typedef enum mc_client_manager_phase {
    MC_CLIENT_MANAGER_COLD,
    MC_CLIENT_MANAGER_CONSTRUCTING,
    MC_CLIENT_MANAGER_READY,
    MC_CLIENT_MANAGER_FAILED,
    MC_CLIENT_MANAGER_RETIRING
} mc_client_manager_phase;

typedef struct mc_client_manager_error {
    mc_client_manager_status status, initial_status;
    const char *operation, *source_class, *source_method, *source_descriptor;
    int32_t pc, th_pc, renderer_pc, frame_pc;
    bool native_policy;
    /* Registry's existing typed channel can mix source/native failures.
     * When true, registry is authoritative; native_policy is not an inferred
     * Java Throwable classification for that child. */
    bool registry_delegated;
    mc_client_model_error model;
    mc_registry_error registry;
    mc_player_status string;
    /* Complete by-value child cause; all nested view lifetimes are audited. */
    mc_client_entity_renderer_error hand;
    mc_world_storage_error world;
    mc_math_class_error math;
    mc_player_error player;
    mc_entity_status entity_status;
    mc_client_render_engine_status engine_status;
    mc_game_settings_error settings;
    int font_status; /* Actual private BitmapFont status. */
    /* Complete large child diagnostics in this manager's quota-owned storage.
     * Borrowed until its next mutating operation or retirement. Any source
     * loader views also borrow the captured World through that interval. */
    const mc_client_bitmap_font_error *font;
    const mc_world_runtime_acquisition_error *acquisition;
} mc_client_manager_error;

typedef struct mc_client_manager_options {
    size_t class_bytes, renderer_bytes, maximum_displaced_hands;
} mc_client_manager_options;

typedef struct mc_client_manager_consumer {
    mc_client_render_manager_classes *classes;
    bool attached;
} mc_client_manager_consumer;

typedef struct mc_client_manager_fields {
    const void *o; /* The actual identity-key Map, published at TH:12. */
    mc_client_bitmap_font *p;
    mc_client_render_engine *e;
    const mc_client_source_ra *f;
    const void *g, *h;
    float i, j;
    const mc_game_settings *k;
    double l, m, n;
} mc_client_manager_fields;

typedef struct mc_client_manager_state {
    mc_client_manager_phase phase;
    const mc_client_render_manager *a, *candidate;
    double b, c, d;
    size_t native_consumers, entries, completed_bw_stores, class_bytes;
    size_t manager_holds, displaced_hands;
    mc_client_held_item *hand_occurrence; /* Borrowed native occurrence; source F is fields.f. */
    int32_t last_put_pc, last_constructor_pc;
    int32_t last_frame_pc;
    size_t retained_engines, retained_fonts, retained_settings;
    const mc_entity *living_occurrence;
    bool ds_ready, source_views_available;
    mc_player_string *const *ds_i;
    mc_client_manager_fields fields;
    const mc_client_manager_error *failure, *retirement;
} mc_client_manager_state;

typedef struct mc_client_managed_renderer_state {
    const char *source_class;
    const void *identity;
    mc_client_model_phase phase;
    size_t holds, allocation_bytes;
    const mc_client_render_manager *manager;
    const mc_client_biped_model *bw_model;
    const mc_client_render_blocks *bw_blocks, *own_blocks;
    const void *primary, *secondary, *own_model, *g, *h;
    const mc_java_random *random;
    float shadow_size, shadow_opacity, scale;
    int32_t icon;
    const mc_client_model_error *failure;
} mc_client_managed_renderer_state;

/* One associated namespace in the SAME real RenderClasses. Bind is cold,
 * retains the real Registry and rejects a changed Registry/profile/options.
 * Outer runtime owns this association; no per-App replacement class exists.
 * All operations require exclusive access; outputs/errors/slots are disjoint
 * from every live owner/view. Native quotas are not JVM heap/OOME claims. */
mc_client_manager_options mc_client_render_manager_default_options(void);
mc_client_manager_status mc_client_render_manager_classes_bind(mc_client_render_classes *,
                                                               mc_registry_classes *,
                                                               mc_java_random_profile *,
                                                               const mc_client_manager_options *,
                                                               mc_client_render_manager_classes **,
                                                               mc_client_manager_error *);
/* Passive lookup of the actual associated namespace, preserving its immutable
 * options. Absence returns NULL; cold/failed/inactive retiring owners remain
 * discoverable. No source initialization, retain or diagnostic mutation runs.
 * Output is disjoint and changes only on success; error is optional. */
mc_client_manager_status mc_client_render_manager_classes_existing(
    const mc_client_render_classes *, const mc_registry_classes *, const mc_java_random_profile *,
    mc_client_render_manager_classes **nullable_out, mc_client_manager_error *);
mc_client_manager_status
mc_client_render_manager_consumer_attach(mc_client_render_manager_classes *,
                                         mc_client_manager_consumer *, mc_client_manager_error *);
/* Detach on App RETIRING BEFORE waiting for global F's captured Font/Engine.
 * This is a non-counted source-admission borrow, never a class ownership cycle. */
mc_client_manager_status mc_client_render_manager_consumer_detach(mc_client_manager_consumer *,
                                                                  mc_client_manager_error *);
mc_client_manager_status
mc_client_render_manager_static_get(mc_client_render_manager_classes *,
                                    mc_client_render_manager **nullable_actual_a,
                                    mc_client_manager_error *);
bool mc_client_render_manager_inspect(const mc_client_render_manager_classes *,
                                      mc_client_manager_state *);
mc_client_manager_status mc_client_render_manager_retain(mc_client_render_manager *,
                                                         mc_client_manager_error *);
mc_client_manager_status mc_client_render_manager_release(mc_client_render_manager **,
                                                          mc_client_manager_error *);
/* Exact th.a(fd): source g2 store and return5. Holds the SAME nullable World
 * independently until replacement or checked terminal detach. No World query,
 * renderer lookup or other source field is read. */
mc_client_manager_status mc_client_render_manager_change_world(mc_client_render_manager *,
                                                               mc_world_runtime *,
                                                               mc_client_manager_error *);
/* Native terminal occurrence ending only if this manager still holds the
 * specified actual World. A different current World is left untouched. */
mc_client_manager_status mc_client_render_manager_detach_world(mc_client_render_manager *,
                                                               mc_world_runtime *,
                                                               mc_client_manager_error *);
/* Complete th.a(fd,ji,sj,ls,kv,float), including virtual Living.N, real sleeping
 * World/Bed/metadata reads and ordered arithmetic/stores. Ordinary failure
 * preserves completed effects and leaves TH READY. Nonnull Worlds must be real
 * GAME Worlds using this Registry/Entity/profile namespace. Nullable services
 * are stored at their reached PCs; NULL Living fails at29 after all five stores.
 * The caller holds the SAME Math namespace through the call; it is used only
 * at reached sleeping floors, with no eager initialization or table copy.
 * Distinct captured Engines, Fonts and Settings remain held until TH retirement;
 * current Living and World occurrences end on replacement. Settings' source
 * App.x stays borrowed: its real App must outlive every captured Settings owner.
 * No App/class hold or automatic Settings final free is introduced. */
mc_client_manager_status
mc_client_render_manager_bind_frame(mc_client_render_manager *, mc_math_classes *borrowed_math,
                                    mc_world_runtime *, mc_client_render_engine *,
                                    mc_client_bitmap_font *, mc_entity *, mc_game_settings *, float,
                                    mc_client_manager_error *);
mc_client_manager_status mc_client_render_manager_entry(mc_client_render_manager *, size_t,
                                                        const mc_entity_class_literal **,
                                                        mc_client_managed_renderer **,
                                                        mc_client_manager_error *);
mc_client_manager_status mc_client_managed_renderer_retain(mc_client_managed_renderer *,
                                                           mc_client_manager_error *);
mc_client_manager_status mc_client_managed_renderer_release(mc_client_managed_renderer **,
                                                            mc_client_manager_error *);
bool mc_client_managed_renderer_inspect(const mc_client_managed_renderer *,
                                        mc_client_managed_renderer_state *);
/* CPU terminal body requires actual outer authority, zero consumers, no
 * external manager/renderer operations/holds, and no current/displaced hands.
 * Captured Fonts and Settings are released before captured Engines, with
 * both TH active guards held. Failed release preserves remaining occurrences
 * for checked retry, including a Settings owner active during native cleanup.
 * No Source a/f=NULL store occurs. It clears only consumed native ownership.
 * ModelClasses final free is BUSY until this explicit association is consumed. */
mc_client_manager_status
mc_client_render_manager_classes_retire(mc_client_render_manager_classes **,
                                        mc_client_manager_error *);
/* Concrete GPU-side integration lives separately from the CPU constructor:
 * completed new RA must precede this actual captured-receiver f321 store.
 * NULL captured A fails at321, after the real RA constructor, preserving it.
 * Successful publication consumes the candidate occurrence, retaining the SAME
 * source object and displaced old ownership until actual outer cleanup. */
mc_client_manager_status mc_client_render_manager_publish_hand(
    mc_client_render_manager_classes *, mc_client_render_manager *captured_nullable_a,
    mc_client_held_item **completed_candidate, mc_client_manager_error *);
/* Explicit outer terminal coordination with real App/Settings/Display alive.
 * Never called by per-App retirement and never inferred from a hold count. */
mc_client_manager_status mc_client_render_manager_retire_hands(mc_client_render_manager_classes *,
                                                               mc_client_manager_error *);
#endif
