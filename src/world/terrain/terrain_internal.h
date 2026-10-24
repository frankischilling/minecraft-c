#ifndef MC_WORLD_TERRAIN_INTERNAL_H
#define MC_WORLD_TERRAIN_INTERNAL_H

#include "mc/world/terrain/terrain.h"

/* Fixed normal-density requests, in original g/h/d/e/f call order. This
 * private arithmetic interface knows no World, Registry or climate owner.
 * Each request validates only its own output span, then clears its complete
 * span and samples the fixed shape. Keep initialized state/storage exclusive
 * and disjoint as in terrain.h; no future channel/climate preflight occurs. */
typedef enum mc_terrain_density_channel {
    MC_TERRAIN_DENSITY_SCALE = 0,
    MC_TERRAIN_DENSITY_BIAS,
    MC_TERRAIN_DENSITY_MIX,
    MC_TERRAIN_DENSITY_LOW,
    MC_TERRAIN_DENSITY_HIGH
} mc_terrain_density_channel;

bool mc_terrain_overworld_density_channel(const mc_terrain_overworld_state *state,
                                          mc_terrain_density_channel channel, int32_t chunk_x,
                                          int32_t chunk_z, mc_terrain_double_span out);

/* Compose exactly one seventeen-value column from ALREADY reached climate
 * scalars. Require column<25, valid fixed workspace spans and density[425].
 * The caller owns temperature/rainfall access order and any reached errors.
 * Preserve arithmetic association, top binary32 fade and all other columns. */
void mc_terrain_overworld_density_column(const mc_terrain_overworld_workspace *workspace,
                                         size_t column, double temperature, double rainfall,
                                         double *density);

typedef struct mc_terrain_base_voxel {
    size_t index, temperature_index;
    int32_t y;
    double density;
} mc_terrain_base_voxel;

/* Fixed interpolation locals, not another generator or source array owner.
 * Callers use only init/get/advance and never modify this private state. The
 * same initialized density[425] allocation must stay live and stable through
 * the cursor. Init loads the first cell; advance performs the source repeated
 * additions and later cell loads ONLY AFTER the current actual byte store.
 * No advance after a failed climate/Block read or byte store. No allocation,
 * Random, World, Registry, validation of future climate, or callbacks occur. */
typedef struct mc_terrain_base_cursor {
    const double *density;
    size_t coarse_x, coarse_z, coarse_y, sub_x, sub_y, sub_z;
    double corner00, corner01, corner10, corner11;
    double step00, step01, step10, step11;
    double edge0, edge1, edge_step0, edge_step1, value, step;
    bool done;
} mc_terrain_base_cursor;

void mc_terrain_overworld_base_cursor_init(mc_terrain_base_cursor *cursor, const double *density);
/* Passive get, false after all32768 stores; failure preserves out. Nonnull
 * valid cursor/output and disjoint storage are private caller preconditions. */
bool mc_terrain_overworld_base_cursor_get(const mc_terrain_base_cursor *cursor,
                                          mc_terrain_base_voxel *out);
void mc_terrain_overworld_base_cursor_advance(mc_terrain_base_cursor *cursor);

#endif
