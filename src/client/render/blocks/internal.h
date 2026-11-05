#ifndef MC_CLIENT_RENDER_BLOCKS_INTERNAL_H
#define MC_CLIENT_RENDER_BLOCKS_INTERNAL_H

#include "../../render_engine/colorizer.h"
#include "../../startup/application_queries.h"
#include "../../world/render_global_internal.h"
#include "mc/block/registry/rendering.h"
#include "mc/client/world/chunk_cache.h"
#include "mc/entity/vector/vector.h"
#include "mc/world/dimension/world_chunk_manager.h"
#include "mc/world/runtime/blocks.h"
#include "mc/world/runtime/brightness.h"

typedef struct client_application client_application;

typedef struct mc_cv_frame {
    const char *class_name;
    const char *method, *descriptor;
    unsigned int pc;
} mc_cv_frame;

typedef enum mc_cv_world_child_kind {
    MC_CV_WORLD_NONE,
    MC_CV_WORLD_BLOCK,
    MC_CV_WORLD_METADATA,
    MC_CV_WORLD_MATERIAL,
    MC_CV_WORLD_BRIGHTNESS,
    MC_CV_WORLD_MIN_BRIGHTNESS,
    MC_CV_WORLD_OPAQUE,
    MC_CV_WORLD_MANAGER,
    MC_CV_WORLD_SOLID
} mc_cv_world_child_kind;

typedef struct mc_cv_world_error {
    mc_cv_world_child_kind kind;
    bool failed;

    union {
        struct {
            mc_world_storage_error cause;
            mc_world_runtime_acquisition_error acquisition;
        } query;

        mc_world_material_error material;
        mc_world_brightness_error brightness;
        mc_world_opaque_error opaque;
        mc_world_manager_error manager;
        mc_world_solid_support_error solid;
    } value;
} mc_cv_world_error;

/* Synchronous borrowed child of the admitted DK compile. The actual CV, EW,
 * Block and class owners remain live through that parent's retirement. A
 * reached failure retains every source store and the complete CV frame chain.
 * This object neither begins nor finishes the parent's NW batch. */
typedef struct mc_cv_context {
    mc_client_render_blocks *fields;
    mc_client_chunk_cache *cache;
    /* Exact native occurrence backing the actual CV.c World implementation.
     * The parent holds it through this context's retirement. NULL is also a
     * genuine source binding; xp rejects a null c before selecting a body.
     * This is not an alternative source field or an EW adapter. */
    mc_world_runtime *world;
    mc_registry_classes *registry;
    mc_client_render_classes *models;
    mc_client_render_engine_classes *graphics;
    mc_client_display *display; /* Actual inventory-call Display; World paths need none here. */
    client_application *application;
    mc_client_tessellator *tessellator;
    mc_cv_frame frames[16];
    size_t depth;
    unsigned int source_pc, last_store_pc;
    bool failed, returned;
    size_t inventory_faces;
    mc_client_render_global_error failure;
    mc_client_chunk_cache_error cache_error;
    mc_client_colorizer_error colorizer_error;
    mc_world_chunk_manager_error climate_error;
    mc_client_application_query_error application_query_error;
    mc_entity_vector_error fluid_vector_error;
    mc_cv_world_error world_error;
} mc_cv_context;

bool mc_cv_fail(mc_cv_context *, mc_client_render_global_status, const char *, unsigned int,
                bool native_policy);
bool mc_cv_fp(mc_cv_context *, unsigned int);
void mc_cv_store(mc_cv_context *, unsigned int);
bool mc_cv_push(mc_cv_context *, const char *method, const char *descriptor);
bool mc_cv_push_source(mc_cv_context *, const char *class_name, const char *method,
                       const char *descriptor);
void mc_cv_pop(mc_cv_context *);
void mc_cv_pc(mc_cv_context *, unsigned int);
bool mc_cv_nw(mc_cv_context *, unsigned int);
bool mc_cv_color(mc_cv_context *, float, float, float, unsigned int);
bool mc_cv_translation_add(mc_cv_context *, float, float, float, unsigned int);
bool mc_cv_vertex(mc_cv_context *, double, double, double, double, double, unsigned int);
bool mc_cv_fields(mc_cv_context *, const mc_block *, unsigned int, const mc_block_fields **);
bool mc_cv_block(mc_cv_context *, int32_t, int32_t, int32_t, unsigned int, int32_t *);
bool mc_cv_metadata(mc_cv_context *, int32_t, int32_t, int32_t, unsigned int, int32_t *);
/* Use the captured source XP argument rather than rereading the live CV.c. */
bool mc_cv_block_bound(mc_cv_context *, const void *, int32_t, int32_t, int32_t, unsigned int,
                       int32_t *);
bool mc_cv_metadata_bound(mc_cv_context *, const void *, int32_t, int32_t, int32_t, unsigned int,
                          int32_t *);
bool mc_cv_material(mc_cv_context *, int32_t, int32_t, int32_t, unsigned int, mc_material **);
bool mc_cv_manager(mc_cv_context *, unsigned int, mc_world_chunk_manager **);
bool mc_cv_cache_brightness(mc_cv_context *, int32_t, int32_t, int32_t, unsigned int, float *);
bool mc_cv_min_brightness(mc_cv_context *, int32_t, int32_t, int32_t, int32_t, unsigned int,
                          float *);
bool mc_cv_cache_brightness_bound(mc_cv_context *, const void *, int32_t, int32_t, int32_t,
                                  unsigned int, float *);
bool mc_cv_min_brightness_bound(mc_cv_context *, const void *, int32_t, int32_t, int32_t, int32_t,
                                unsigned int, float *);
bool mc_cv_opaque(mc_cv_context *, int32_t, int32_t, int32_t, unsigned int, bool *);
bool mc_cv_solid(mc_cv_context *, int32_t, int32_t, int32_t, unsigned int, bool *);
bool mc_cv_registry(mc_cv_context *, unsigned int, mc_registry_block_view *);
bool mc_cv_brightness(mc_cv_context *, const mc_block *, int32_t, int32_t, int32_t, unsigned int,
                      float *);
bool mc_cv_brightness_bound(mc_cv_context *, const mc_block *, const void *, int32_t, int32_t,
                            int32_t, unsigned int, float *);
bool mc_cv_side(mc_cv_context *, const mc_block *, int32_t, int32_t, int32_t, int32_t, unsigned int,
                bool *);
bool mc_cv_texture(mc_cv_context *, const mc_block *, int32_t, int32_t, int32_t, int32_t,
                   unsigned int, int32_t *);
bool mc_cv_side_texture(mc_cv_context *, const mc_block *, int32_t, unsigned int, int32_t *);
bool mc_cv_update_bounds(mc_cv_context *, const mc_block *, int32_t, int32_t, int32_t,
                         unsigned int);
bool mc_cv_grass(mc_cv_context *, unsigned int, const mc_block **);
bool mc_cv_ao_flag(mc_cv_context *, int32_t, int32_t, int32_t, unsigned int static_pc,
                   unsigned int query_pc, unsigned int index_pc, bool *);
bool mc_cv_block_color(mc_cv_context *, const mc_block *, int32_t, int32_t, int32_t, unsigned int,
                       int32_t *);

/* The wrappers record the caller PC and enter the actual CV method. Bodies
 * operate on the SAME mutable fields and emit four vertices per source face. */
bool mc_cv_face(mc_cv_context *, int32_t side, const mc_block *, double, double, double,
                int32_t texture, unsigned int caller_pc);
bool mc_cv_face_body(mc_cv_context *, int32_t, const mc_block *, double, double, double, int32_t);
bool mc_cv_flat(mc_cv_context *, const mc_block *, int32_t, int32_t, int32_t, float, float, float,
                bool *);
bool mc_source_cv_ambient_occlusion(mc_cv_context *, const mc_block *, int32_t, int32_t, int32_t,
                                    float, float, float, bool *);
bool mc_cv_render(mc_cv_context *, const mc_block *, int32_t, int32_t, int32_t, bool *);
bool mc_cv_crossed_plant(mc_cv_context *, const mc_block *, int32_t, int32_t, int32_t, bool *);
bool mc_cv_crossed_plant_geometry(mc_cv_context *, const mc_block *, int32_t, double, double,
                                  double);
bool mc_cv_fluid(mc_cv_context *, const mc_block *, int32_t, int32_t, int32_t, bool *);
bool mc_cv_ladder(mc_cv_context *, const mc_block *, int32_t, int32_t, int32_t, bool *);
bool mc_cv_crops(mc_cv_context *, const mc_block *, int32_t, int32_t, int32_t, bool *);
bool mc_cv_crops_geometry(mc_cv_context *, const mc_block *, int32_t, double, double, double);
/* Actual Cactus World bodies. The dispatcher owns m's frame; c is entered by
 * its caller. Neither body begins/draws the enclosing NW batch or resets m. */
bool mc_cv_cactus(mc_cv_context *, const mc_block *, int32_t, int32_t, int32_t, bool *);
bool mc_cv_cactus_geometry(mc_cv_context *, const mc_block *, int32_t, int32_t, int32_t, float,
                           float, float, bool *);
/* Actual type2 World wrapper and shared five-quad Torch primitive. The caller
 * owns the existing NW batch; the primitive owns its nested source frame. */
bool mc_cv_torch(mc_cv_context *, const mc_block *, int32_t, int32_t, int32_t, bool *);
bool mc_cv_torch_geometry(mc_cv_context *, const mc_block *, double, double, double, double,
                          double);
bool mc_cv_redstone(mc_cv_context *, const mc_block *, int32_t, int32_t, int32_t, bool *);
/* Actual cv.a(Block,III,texture): d3, child b12, d=-1 at18, return21.
 * Failure retains d and the complete nested source/CV/World diagnostics. */
bool mc_cv_texture_override(mc_cv_context *, const mc_block *, int32_t, int32_t, int32_t, int32_t);
/* Actual static cv.a(I)Z and instance cv.a(Block,int,float). The enclosing
 * owner retains the same CV, Block, graphics namespace and Display. Each face
 * uses an independent genuine NW begin/normal/face/draw sequence. */
bool mc_cv_inventory_type_3d(int32_t);
bool mc_cv_inventory(mc_cv_context *, const mc_block *, int32_t, float);

#endif
