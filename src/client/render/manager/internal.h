#ifndef MC_CLIENT_RENDER_MANAGER_INTERNAL_H
#define MC_CLIENT_RENDER_MANAGER_INTERNAL_H
#include "../model_graph_internal.h"
#include "mc/client/render/manager/manager.h"
#include "mc/item/registry/runtime.h"

/* Older MSVC diagnoses this C17 flexible array as extension C4200.
 * Limit the compatibility guard to this type; its layout stays unchanged. */
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4200)
#endif
typedef struct manager_part_array {
    int32_t length;
    mc_client_model_part *elements[];
} manager_part_array;
#ifdef _MSC_VER
#pragma warning(pop)
#endif

typedef struct manager_model {
    struct manager_model *next;
    const char *source_class, *descriptor;
    mc_client_model_phase phase;
    /* Concrete native model storage. These shared part slots do not imply FH
     * inheritance/construction for other KO subclasses. Only FH/LC/EJ run FH. */
    mc_client_biped_model fields;
    mc_client_model_part *extra_j, *extra_k;
    manager_part_array *array_a, *array_b;
} manager_model;

typedef struct manager_map_entry {
    const mc_entity_class_literal *key;
    const void *value; /* Actual Source renderer object, including real BB. */
    struct mc_client_managed_renderer *owner;
    struct manager_map_entry *next;
} manager_map_entry;

typedef struct manager_map {
    size_t size, capacity;
    manager_map_entry **buckets;
} manager_map;

typedef struct manager_displaced_hand {
    struct manager_displaced_hand *next;
    mc_client_held_item *owner;
} manager_displaced_hand;

typedef struct manager_engine_occurrence {
    struct manager_engine_occurrence *next;
    mc_client_render_engine *value;
} manager_engine_occurrence;

typedef struct manager_font_occurrence {
    struct manager_font_occurrence *next;
    mc_client_bitmap_font *value;
} manager_font_occurrence;

typedef struct manager_settings_occurrence {
    struct manager_settings_occurrence *next;
    mc_game_settings *value;
} manager_settings_occurrence;
typedef struct manager_frame_diagnostics manager_frame_diagnostics;

struct mc_client_managed_renderer {
    struct mc_client_render_manager *parent; /* Borrowed, not a counted cycle. */
    const char *source_class;
    size_t holds;
    bool active, models_held;
    mc_client_model_phase phase;
    const void *manager; /* Actual BW.b, same native field type as canonical BB. */
    mc_client_biped_model *bw_model;
    mc_client_render_blocks *bw_blocks;
    float shadow_size, shadow_opacity;
    manager_model *primary, *secondary, *own_model, *g, *h;
    manager_model *models; /* Native pending/failed/all source models. */
    mc_client_render_blocks *own_blocks;
    mc_java_random *random;
    float scale;
    int32_t icon;
    mc_client_item_renderer *bb; /* SAME complete legacy BB object when kind BB. */
    model_allocation *allocations;
    size_t bytes, max_bytes;
    mc_client_model_error failure;
};

struct mc_client_render_manager {
    mc_client_render_manager_classes *classes;
    mc_client_manager_fields fields;
    size_t holds;
    bool active;
    manager_map *map, *map_candidate;
    mc_client_managed_renderer *renderers[28]; /* Native pending/owned list. */
    size_t allocated_renderers, put_count, bw_count;
    int32_t last_put_pc, last_constructor_pc;
    mc_client_held_item *hand;
    manager_displaced_hand *displaced;
    size_t displaced_count;
    mc_world_runtime *world_occurrence; /* Independent native hold backing actual g. */
    mc_entity *living_occurrence;
    manager_engine_occurrence *engines;
    manager_font_occurrence *fonts;
    manager_settings_occurrence *settings;
    size_t retained_engines, retained_fonts, retained_settings;
    int32_t last_frame_pc;
    manager_frame_diagnostics *frame_diagnostics;
};

struct mc_client_render_manager_classes {
    mc_client_render_classes *models; /* Actual native association, no self hold. */
    mc_registry_classes *registry;
    mc_java_random_profile *profile;
    mc_client_manager_options options;
    mc_client_manager_phase phase;
    mc_client_render_manager *a, *candidate;
    double b, c, d;
    size_t consumers, bytes;
    bool active, registry_held, ds_ready, ds_erroneous;
    mc_player_string **ds_i, **ds_candidate;
    mc_client_manager_error failure, retirement;
};

mc_client_manager_status manager_fail(mc_client_manager_error *, mc_client_manager_status,
                                      const char *, const char *, const char *, const char *,
                                      int32_t, bool);
void manager_error_init(mc_client_manager_error *);
void *manager_allocate_native(mc_client_render_manager_classes *, size_t, const char *, int32_t,
                              mc_client_manager_error *);
void *manager_allocate(mc_client_render_manager_classes *, size_t, const char *, int32_t);
mc_client_model_graph manager_renderer_graph(mc_client_managed_renderer *);
manager_part_array *manager_part_array_new(mc_client_model_graph *, int32_t, const char *, int32_t);
manager_model *manager_model_allocate(mc_client_model_graph *, const char *, const char *,
                                      const char *, int32_t);
bool manager_model_construct(mc_client_model_graph *, manager_model *, float, int32_t);
mc_client_managed_renderer *manager_renderer_allocate(mc_client_render_manager *, const char *,
                                                      int32_t);
manager_model *manager_renderer_model(mc_client_managed_renderer *, const char *, const char *,
                                      const char *, int32_t, float, int32_t);
bool manager_ds_active_use(mc_client_render_manager_classes *);
bool manager_map_put(mc_client_render_manager *, const mc_entity_class_literal *,
                     mc_client_managed_renderer *, int32_t);
bool manager_renderer_construct(mc_client_managed_renderer *, manager_model *, manager_model *,
                                float, float, int32_t);
bool manager_renderer_retire(mc_client_managed_renderer **, mc_client_manager_error *);
bool manager_construct(mc_client_render_manager *);
bool manager_terminal_admit(mc_client_render_manager_classes *, mc_client_manager_error *);
bool manager_world_store_entered(mc_client_render_manager *, mc_world_runtime *,
                                 mc_client_manager_error *);
bool manager_world_ledger_admit(const mc_client_render_manager *, mc_client_manager_error *);
void manager_frame_invalidate(mc_client_render_manager *);
bool manager_frame_terminal_admit(mc_client_render_manager *, mc_client_manager_error *);
bool manager_frame_retire(mc_client_render_manager *, mc_client_manager_error *);
void manager_frame_destroy(mc_client_render_manager *);

/* One reached N putstatic into the SAME already-active TH class. Raw scalar
 * store only: no interpolation, FP admission, class use or frame operation.
 * Caller owns the class graph/consumer and records its own N source PC. */
mc_client_manager_status mc_client_render_manager_origin_store(mc_client_render_manager_classes *,
                                                               unsigned int axis, double value,
                                                               mc_client_manager_error *);
#endif
