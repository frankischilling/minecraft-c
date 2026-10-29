#ifndef MC_BLOCK_REGISTRY_RENDERING_H
#define MC_BLOCK_REGISTRY_RENDERING_H

#include "mc/block/registry/registry.h"

/* Block renderer's IBlockAccess receiver. Reads refer to an already acquired
 * cache; these callbacks must not acquire a different Chunk. A callback owner
 * may retain a first native error and return a neutral local value afterward;
 * the caller must reject the entire mesh on that error, never publish it. */
typedef struct mc_block_render_access {
    void *owner;
    int32_t (*block)(void *owner, int32_t x, int32_t y, int32_t z);
    int32_t (*metadata)(void *owner, int32_t x, int32_t y, int32_t z);
    mc_material *(*material)(void *owner, int32_t x, int32_t y, int32_t z);
    bool (*opaque)(void *owner, int32_t x, int32_t y, int32_t z);
    mc_registry_status (*climate)(void *owner, int32_t x, int32_t z, double *temperature,
                                  double *rainfall);
    const uint32_t *grass_colors, *foliage_colors;
    size_t grass_color_count, foliage_color_count;
} mc_block_render_access;

typedef struct mc_block_render_description {
    int32_t type, pass;
    bool grass, tall_grass, fluid, cactus;
} mc_block_render_description;

/* Source virtual rendering dispatch on actual closed Registry descendants.
 * Exclusive access, same live Registry/Material dependencies throughout.
 * These operations do not initialize Block. Invoke after its ordinary active
 * use. Bounds/fancy-leaf operations mutate the actual shared Block objects;
 * do not restore detached snapshots over later source state. Negative render
 * types denote the original TileEntity mesh path, not a cube fallback. */
mc_registry_status mc_block_render_describe(const mc_block *block,
                                            mc_block_render_description *out);
mc_registry_status mc_block_render_update_bounds(mc_block *block,
                                                 const mc_block_render_access *access, int32_t x,
                                                 int32_t y, int32_t z);
mc_registry_status mc_block_render_fancy_leaves(mc_registry_classes *classes, bool fancy);
/* Exact bk.a(Z) on the already captured uu.L receiver; no static reread. */
mc_registry_status mc_block_render_fancy_leaves_receiver(mc_block *block, bool fancy);
mc_registry_status mc_block_render_finish_fluid(mc_block *block);
/* Actual uu.a(FFFFFF)V bound mutation used by composite render families. */
mc_registry_status mc_block_render_bounds(mc_block *block, float x0, float y0, float z0, float x1,
                                          float y1, float z1);
bool mc_block_render_opaque(const mc_block *block);
mc_registry_status mc_block_render_texture(const mc_block *block,
                                           const mc_block_render_access *access, int32_t x,
                                           int32_t y, int32_t z, int32_t side, int32_t *out);
mc_registry_status mc_block_render_texture_metadata(const mc_block *block, int32_t side,
                                                    int32_t metadata, int32_t *out);
mc_registry_status mc_block_render_color(const mc_block *block,
                                         const mc_block_render_access *access, int32_t x, int32_t y,
                                         int32_t z, uint32_t *out);
mc_registry_status mc_block_render_side(const mc_block *block, const mc_block_render_access *access,
                                        int32_t neighbor_x, int32_t neighbor_y, int32_t neighbor_z,
                                        int32_t side, bool *out);

#endif
