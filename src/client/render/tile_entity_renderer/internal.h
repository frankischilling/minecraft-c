#ifndef MC_CLIENT_RENDER_TILE_ENTITY_RENDERER_INTERNAL_H
#define MC_CLIENT_RENDER_TILE_ENTITY_RENDERER_INTERNAL_H

#include "../model_graph_internal.h"
#include "mc/client/render/tile_entity_renderer/tile_entity_renderer.h"

typedef struct tile_renderer_map_node {
    const mc_tile_entity_class_literal *key;
    mc_client_tile_renderer *value;
    struct tile_renderer_map_node *next;
} tile_renderer_map_node;

typedef struct tile_renderer_map {
    tile_renderer_map_node **buckets;
    size_t size, capacity, threshold;
    uint32_t mod_count;
} tile_renderer_map;

typedef struct tile_renderer_engine_occurrence {
    struct tile_renderer_engine_occurrence *next;
    mc_client_render_engine *value;
} tile_renderer_engine_occurrence;

typedef struct tile_renderer_font_occurrence {
    struct tile_renderer_font_occurrence *next;
    mc_client_bitmap_font *value;
} tile_renderer_font_occurrence;

/* AG.b is a distinct real empty String->Entity Map. No rendering/factory body
 * is supplied by its constructor. These are its own source storage cells. */
typedef struct tile_renderer_spawner_node {
    mc_tile_entity_name *key;
    mc_entity *value;
    struct tile_renderer_spawner_node *next;
} tile_renderer_spawner_node;

typedef struct tile_renderer_spawner_map {
    tile_renderer_spawner_node **buckets;
    size_t size, capacity, threshold;
    uint32_t mod_count;
} tile_renderer_spawner_map;

/* Native lifetime header around the SAME actual CV fields. The list is an
 * ownership ledger, not a source collection or a second CV representation. */
struct mc_client_tile_renderer_cv {
    mc_client_tile_renderer *owner;
    struct mc_client_tile_renderer_cv *next;
    mc_client_render_blocks fields;
    mc_world_runtime *world_occurrence;
    size_t external_holds;
    bool current;
};

struct mc_client_tile_renderer {
    mc_client_tile_renderer_dispatcher *parent; /* Native owner, no counted backedge. */
    const char *source_class;
    size_t holds;
    bool active, models_held;
    mc_client_model_phase phase;
    mc_client_tile_renderer_dispatcher *manager; /* Actual JE.a after setter2. */
    mc_client_sign_model *sign_model, *sign_candidate;
    mc_client_model_phase sign_phase;
    tile_renderer_spawner_map *spawner_map, *spawner_map_candidate;
    mc_client_render_blocks *piston_blocks;
    mc_client_tile_renderer_cv *current_cv, *cvs;
    size_t live_cvs, retained_cv_holds;
    model_allocation *allocations;
    size_t bytes, max_bytes;
    int32_t rf_pc;
    mc_client_model_error failure;
};

struct mc_client_tile_renderer_dispatcher {
    mc_client_tile_renderer_classes *classes;
    mc_client_tile_renderer_fields fields;
    size_t holds;
    bool active;
    mc_world_runtime *world_occurrence; /* Independent native occurrence backing LL.f. */
    tile_renderer_engine_occurrence *engines;
    tile_renderer_font_occurrence *fonts;
    mc_entity *living_occurrence;
    size_t retained_engines, retained_fonts;
    tile_renderer_map *map, *map_candidate;
    mc_client_tile_renderer *renderers[3]; /* Includes failed/unpublished children. */
    size_t allocated_renderers, put_calls, backpointers;
    int32_t last_put_pc, last_constructor_pc, last_frame_pc;
};

struct mc_client_tile_renderer_classes {
    mc_client_render_classes *models;        /* Association prevents model-class free. */
    mc_world_runtime_classes *world_classes; /* Protected by tile_classes borrow. */
    mc_tile_entity_classes *tile_classes;
    const mc_tile_entity_class_literal *base;
    mc_java_random_profile *profile;
    mc_client_tile_renderer_options options;
    mc_client_tile_renderer_phase phase;
    mc_client_tile_renderer_dispatcher *a, *candidate;
    double b, c, d;
    size_t consumers, bytes;
    bool active;
    mc_client_tile_renderer_error failure, retirement;
};

/* Passive exact-provenance lookup for outer terminal ownership. All expected
 * owners are required/live. Absence (including a consumed association) yields
 * NULL without creating/binding/initializing anything. Existing cold, failed
 * and inactive RETIRING owners remain discoverable for checked retirement.
 * Active owners refuse; output changes only on success. No native hold added. */
mc_client_tile_renderer_status mc_client_tile_entity_renderer_classes_existing(
    const mc_client_render_classes *models, const mc_registry_classes *expected_registry,
    const mc_world_runtime_classes *expected_world_classes,
    mc_client_tile_renderer_classes **nullable_out, mc_client_tile_renderer_error *error);

void tile_renderer_error_init(mc_client_tile_renderer_error *);
mc_client_tile_renderer_status tile_renderer_fail(mc_client_tile_renderer_error *,
                                                  mc_client_tile_renderer_status, const char *,
                                                  const char *, const char *, const char *, int32_t,
                                                  bool);
void *tile_renderer_allocate(mc_client_tile_renderer_classes *, size_t, const char *, const char *,
                             const char *, int32_t, mc_client_tile_renderer_error *);
mc_client_model_graph tile_renderer_graph(mc_client_tile_renderer *);
bool tile_renderer_construct(mc_client_tile_renderer_dispatcher *);
bool tile_renderer_object_retire(mc_client_tile_renderer **, mc_client_tile_renderer_error *);

tile_renderer_map_node *tile_renderer_map_node_find(const tile_renderer_map *,
                                                    const mc_tile_entity_class_literal *);
bool tile_renderer_map_put(mc_client_tile_renderer_dispatcher *,
                           const mc_tile_entity_class_literal *, mc_client_tile_renderer *, int32_t,
                           bool, mc_client_tile_renderer_error *);
void tile_renderer_map_retire(mc_client_tile_renderer_classes *, tile_renderer_map **);
bool tile_renderer_passive_admit(mc_client_tile_renderer_dispatcher *,
                                 mc_client_tile_renderer_error *);
bool tile_renderer_world_change_admit(mc_client_tile_renderer_dispatcher *, mc_world_runtime *,
                                      mc_client_tile_renderer_error *);
bool tile_renderer_world_change_entered(mc_client_tile_renderer_dispatcher *, mc_world_runtime *,
                                        mc_client_tile_renderer_error *);
bool tile_renderer_frame_terminal_admit(mc_client_tile_renderer_dispatcher *,
                                        mc_client_tile_renderer_error *);
bool tile_renderer_frame_retire(mc_client_tile_renderer_dispatcher *,
                                mc_client_tile_renderer_error *);
bool tile_renderer_world_terminal_admit(mc_client_tile_renderer_dispatcher *,
                                        mc_client_tile_renderer_error *);
bool tile_renderer_world_retire(mc_client_tile_renderer_dispatcher *,
                                mc_client_tile_renderer_error *);

#endif
