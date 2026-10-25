#ifndef MC_CLIENT_WORLD_MESHER_H
#define MC_CLIENT_WORLD_MESHER_H

#include "mc/block/registry/rendering.h"
#include "mc/core/math/math.h"
#include "mc/world/runtime/runtime.h"
#include "vertex.h"

typedef enum mc_client_world_mesher_status {
    MC_CLIENT_WORLD_MESHER_OK,
    MC_CLIENT_WORLD_MESHER_INVALID_ARGUMENT,
    MC_CLIENT_WORLD_MESHER_ALLOCATION_FAILED,
    MC_CLIENT_WORLD_MESHER_LIMIT,
    MC_CLIENT_WORLD_MESHER_WORLD_FAILED,
    MC_CLIENT_WORLD_MESHER_REGISTRY_FAILED,
    MC_CLIENT_WORLD_MESHER_UNIMPLEMENTED_DEPENDENCY
} mc_client_world_mesher_status;

typedef struct mc_client_world_mesher_error {
    mc_client_world_mesher_status status;
    const char *operation;
    int32_t x, y, z, block_id, render_type;
    mc_world_storage_error world;
    mc_registry_status registry;
} mc_client_world_mesher_error;

typedef struct mc_client_world_mesher_options {
    bool ambient_occlusion, anaglyph, fancy_leaves;
    const uint32_t *grass_colors, *foliage_colors;
    size_t grass_color_count, foliage_color_count;
    const mc_math_trig *trig;
    size_t max_vertices;
} mc_client_world_mesher_options;

typedef struct mc_client_world_cpu_mesh {
    int32_t origin_x, origin_y, origin_z;
    mc_client_world_vertex *vertices[2];
    size_t vertex_count[2];
} mc_client_world_cpu_mesh;

mc_client_world_mesher_options mc_client_world_mesher_default_options(void);

/* Build one aligned 16-cube section from the actual READY GAME World. Acquire
 * the original ew padded nine-column rectangle X outer/Z inner and hold every
 * actual identity throughout mesh/climate work. A failed acquisition or shape
 * dependency stops explicitly; output is unchanged and no partial mesh is
 * published. Prior World acquisition/population and actual Block bound changes
 * remain. Vertex positions narrow only after the double section translation.
 * Caller owns output arrays; they retain no World, Chunk or Registry pointer.
 * All World/profile/resources/Registry access stays live and exclusive. */
mc_client_world_mesher_status
mc_client_world_mesher_build(mc_world_runtime *world, int32_t origin_x, int32_t origin_y,
                             int32_t origin_z, const mc_client_world_mesher_options *options,
                             mc_client_world_cpu_mesh *out, mc_client_world_mesher_error *error);
void mc_client_world_cpu_mesh_free(mc_client_world_cpu_mesh *mesh);

/* Source six-face primitive, also usable with authored geometry. Bounds are
 * ordered doubles from the actual Block. Colors are per-quad-vertex ARGB.
 * Emit the original nw diagonal [0,1,2,0,2,3]. No Registry/World/GL operation. */
bool mc_client_world_mesher_face(const mc_block_fields *bounds, int32_t origin_x, int32_t origin_y,
                                 int32_t origin_z, double x, double y, double z, int32_t side,
                                 int32_t texture, const uint32_t colors[4],
                                 mc_client_world_vertex out[6]);

#endif
