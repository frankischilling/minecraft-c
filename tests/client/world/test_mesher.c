#include "../../../src/block/registry/registry_internal.h"
#include "../../../src/client/world/mesher.h"
#include "../../../src/client/world/mesher_lighting.h"

#include <stdio.h>
#include <string.h>

/* Authored receiver/light grids. These are native source-contract tests, not
 * a fabricated GAME World or a qualification of original rendered pixels. */
static unsigned int checks;
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #condition);                \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

static uint32_t bits(float value) {
    uint32_t result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

static bool faces(void) {
    mc_block_fields b = {.bv = 1, .bw = 1, .bx = 1};
    const uint32_t colors[4] = {0xff123456, 0xff789abc, 0xffdef012, 0xff345678};
    static const int positions[6][4][3] = {
        {{0, 0, 1}, {0, 0, 0}, {1, 0, 0}, {1, 0, 1}}, {{1, 1, 1}, {1, 1, 0}, {0, 1, 0}, {0, 1, 1}},
        {{0, 1, 0}, {1, 1, 0}, {1, 0, 0}, {0, 0, 0}}, {{0, 1, 1}, {0, 0, 1}, {1, 0, 1}, {1, 1, 1}},
        {{0, 1, 1}, {0, 1, 0}, {0, 0, 0}, {0, 0, 1}}, {{1, 0, 1}, {1, 0, 0}, {1, 1, 0}, {1, 1, 1}}};
    static const float normals[6][3] = {{0, -1, 0}, {0, 1, 0},  {0, 0, -1},
                                        {0, 0, 1},  {-1, 0, 0}, {1, 0, 0}};
    static const unsigned int order[6] = {0, 1, 2, 0, 2, 3};
    for (int side = 0; side < 6; ++side) {
        mc_client_world_vertex v[6];
        CHECK(mc_client_world_mesher_face(&b, -16, 48, 32, -3, 50, 39, side, 83, colors, v));
        for (size_t i = 0; i < 6; ++i) {
            unsigned int j = order[i];
            CHECK(v[i].x == 13 + (float)positions[side][j][0]);
            CHECK(v[i].y == 2 + (float)positions[side][j][1]);
            CHECK(v[i].z == 7 + (float)positions[side][j][2]);
            CHECK(v[i].argb == colors[j]);
        }
        float ax = v[1].x - v[0].x, ay = v[1].y - v[0].y, az = v[1].z - v[0].z;
        float bx = v[2].x - v[0].x, by = v[2].y - v[0].y, bz = v[2].z - v[0].z;
        CHECK((ay * bz - az * by) * normals[side][0] + (az * bx - ax * bz) * normals[side][1] +
                  (ax * by - ay * bx) * normals[side][2] >
              0);
    }
    mc_client_world_vertex v[6];
    CHECK(mc_client_world_mesher_face(&b, 0, 0, 0, 0, 0, 0, 0, 83, colors, v));
    CHECK(v[0].u == .1875f && bits(v[0].v) == bits((float).3749609375));
    CHECK(v[1].v == .3125f && bits(v[2].u) == bits((float).2499609375));
    b.bs = .125;
    b.bt = .25;
    b.bu = .375;
    b.bv = .75;
    b.bw = .875;
    CHECK(mc_client_world_mesher_face(&b, 16777216, 64, -16, 16777216, 64, -16, 0, 83, colors, v));
    CHECK(v[0].x == .125f && v[0].y == .25f && v[0].z == 1.0f);
    b.bs = -.125;
    b.bv = 1.125;
    b.bu = -.375;
    b.bx = 1.375;
    CHECK(mc_client_world_mesher_face(&b, 0, 0, 0, 0, 0, 0, 0, 83, colors, v));
    CHECK(v[0].u == .1875f && v[1].v == .3125f);
    CHECK(bits(v[2].u) == bits((48.0f + 15.99f) / 256.0f));
    mc_client_world_vertex before[6];
    memcpy(before, v, sizeof(v));
    CHECK(!mc_client_world_mesher_face(&b, 0, 0, 0, 0, 0, 0, 6, 83, colors, v));
    CHECK(memcmp(before, v, sizeof(v)) == 0);
    CHECK(!mc_client_world_mesher_face(&b, 0, 0, 0, 0, 0, 0, 0, 256, colors, v));
    CHECK(memcmp(before, v, sizeof(v)) == 0);
    return true;
}

typedef struct light_grid {
    bool transmit;
    int dark[3];
    unsigned int lights, flags;
    int first[7][3];
} light_grid;

static float grid_light(void *owner, int x, int y, int z) {
    light_grid *g = owner;
    if (g->lights < 7) {
        g->first[g->lights][0] = x;
        g->first[g->lights][1] = y;
        g->first[g->lights][2] = z;
    }
    ++g->lights;
    return x == g->dark[0] && y == g->dark[1] && z == g->dark[2] ? 0.0f : 1.0f;
}

static bool grid_flag(void *owner, int x, int y, int z) {
    light_grid *g = owner;
    (void)x;
    (void)y;
    (void)z;
    ++g->flags;
    return g->transmit;
}

static bool ambient_occlusion(void) {
    static const int corners[6][4][3] = {{{-1, -1, 1}, {-1, -1, -1}, {1, -1, -1}, {1, -1, 1}},
                                         {{1, 1, 1}, {1, 1, -1}, {-1, 1, -1}, {-1, 1, 1}},
                                         {{-1, 1, -1}, {1, 1, -1}, {1, -1, -1}, {-1, -1, -1}},
                                         {{-1, 1, 1}, {-1, -1, 1}, {1, -1, 1}, {1, 1, 1}},
                                         {{-1, 1, 1}, {-1, 1, -1}, {-1, -1, -1}, {-1, -1, 1}},
                                         {{1, -1, 1}, {1, -1, -1}, {1, 1, -1}, {1, 1, 1}}};
    static const int prefix[7][3] = {{0, 0, 0}, {-1, 0, 0}, {0, -1, 0}, {0, 0, -1},
                                     {1, 0, 0}, {0, 1, 0},  {0, 0, 1}};
    for (unsigned int side = 0; side < 6; ++side)
        for (unsigned int corner = 0; corner < 4; ++corner) {
            light_grid g = {.transmit = true,
                            .dark = {corners[side][corner][0], corners[side][corner][1],
                                     corners[side][corner][2]}};
            mc_client_world_ao a;
            mc_client_world_ao_prepare(&a, &g, grid_light, grid_flag);
            CHECK(g.lights == 7 && g.flags == 12);
            CHECK(memcmp(prefix, g.first, sizeof(prefix)) == 0);
            float light[4];
            mc_client_world_ao_face(&a, side, light);
            CHECK(g.lights == 15);
            for (unsigned int i = 0; i < 4; ++i)
                CHECK(light[i] == (i == corner ? .75f : 1.0f));
            g = (light_grid){.dark = {corners[side][corner][0], corners[side][corner][1],
                                      corners[side][corner][2]}};
            mc_client_world_ao_prepare(&a, &g, grid_light, grid_flag);
            mc_client_world_ao_face(&a, side, light);
            CHECK(g.lights == 11); /* Blocked diagonals reuse the prescribed edge. */
            for (unsigned int i = 0; i < 4; ++i)
                CHECK(light[i] == 1.0f);
        }
    return true;
}

typedef struct block_grid {
    int32_t id, metadata;
    bool opaque;
    unsigned int opacity_reads, climate_reads;
    mc_material *material;
} block_grid;

static int32_t grid_block(void *owner, int32_t x, int32_t y, int32_t z) {
    (void)x;
    (void)y;
    (void)z;
    return ((block_grid *)owner)->id;
}

static int32_t grid_metadata(void *owner, int32_t x, int32_t y, int32_t z) {
    (void)x;
    (void)y;
    (void)z;
    return ((block_grid *)owner)->metadata;
}

static mc_material *grid_material(void *owner, int32_t x, int32_t y, int32_t z) {
    (void)x;
    (void)y;
    (void)z;
    return ((block_grid *)owner)->material;
}

static bool grid_opaque(void *owner, int32_t x, int32_t y, int32_t z) {
    block_grid *g = owner;
    (void)x;
    (void)y;
    (void)z;
    ++g->opacity_reads;
    return g->opaque;
}

static mc_registry_status grid_climate(void *owner, int32_t x, int32_t z, double *t, double *r) {
    block_grid *g = owner;
    (void)x;
    (void)z;
    ++g->climate_reads;
    *t = 1;
    *r = 1;
    return MC_REGISTRY_OK;
}

static uint32_t grass[65536], foliage[65536];

typedef struct coordinate_trace {
    size_t count;
    int32_t positions[9][3], blocks[9];
    char operations[9];
    mc_material *material;
} coordinate_trace;

static size_t trace_coordinate(coordinate_trace *trace, char operation, int32_t x, int32_t y,
                               int32_t z) {
    const size_t index = trace->count++;
    if (index < 9) {
        trace->operations[index] = operation;
        trace->positions[index][0] = x;
        trace->positions[index][1] = y;
        trace->positions[index][2] = z;
    }
    return index;
}

static int32_t traced_block(void *owner, int32_t x, int32_t y, int32_t z) {
    coordinate_trace *trace = owner;
    const size_t index = trace_coordinate(trace, 'B', x, y, z);
    return index < 9 ? trace->blocks[index] : 0;
}

static int32_t traced_metadata(void *owner, int32_t x, int32_t y, int32_t z) {
    (void)trace_coordinate(owner, 'D', x, y, z);
    return 0;
}

static mc_material *traced_material(void *owner, int32_t x, int32_t y, int32_t z) {
    coordinate_trace *trace = owner;
    (void)trace_coordinate(trace, 'M', x, y, z);
    return trace->material;
}

static bool traced_opaque(void *owner, int32_t x, int32_t y, int32_t z) {
    (void)trace_coordinate(owner, 'O', x, y, z);
    return true;
}

static bool coordinate_trace_matches(const coordinate_trace *trace, char operation,
                                     const int32_t (*positions)[3], size_t count) {
    CHECK(trace->count == count);
    for (size_t i = 0; i < count; ++i)
        CHECK(trace->operations[i] == operation &&
              memcmp(trace->positions[i], positions[i], sizeof(trace->positions[i])) == 0);
    return true;
}

static bool wrapped_block_coordinates(mc_materials *materials) {
    static const struct {
        int32_t x, z, west[2], east[2], north[2], south[2];
    } edges[] = {{INT32_MIN,
                  INT32_MAX,
                  {INT32_MAX, INT32_MAX - 1},
                  {INT32_MIN + 1, INT32_MIN + 2},
                  {INT32_MAX - 1, INT32_MAX - 2},
                  {INT32_MIN, INT32_MIN + 1}},
                 {INT32_MAX,
                  INT32_MIN,
                  {INT32_MAX - 1, INT32_MAX - 2},
                  {INT32_MIN, INT32_MIN + 1},
                  {INT32_MAX, INT32_MAX - 1},
                  {INT32_MIN + 1, INT32_MIN + 2}}};

    mc_registry_classes classes = {0};
    classes.options.materials = materials;
    mc_block block = {.classes = &classes, .kind = MC_BLOCK_PORTAL, .fields = {.bn = 90}};
    coordinate_trace trace = {0};
    mc_block_render_access access = {.owner = &trace,
                                     .block = traced_block,
                                     .metadata = traced_metadata,
                                     .material = traced_material,
                                     .opaque = traced_opaque};
    for (size_t i = 0; i < sizeof(edges) / sizeof(*edges); ++i) {
        const int32_t x = edges[i].x, y = INT32_MAX, z = edges[i].z;
        const int32_t bounds_positions[2][3] = {{edges[i].west[0], y, z}, {edges[i].east[0], y, z}};
        for (int mode = 0; mode < 3; ++mode) {
            trace = (coordinate_trace){0};
            trace.blocks[mode == 1 ? 1 : 0] = mode == 2 ? 0 : 90;
            CHECK(mc_block_render_update_bounds(&block, &access, x, y, z) == MC_REGISTRY_OK);
            CHECK(coordinate_trace_matches(&trace, 'B', bounds_positions, mode == 0 ? 1 : 2));
            CHECK(block.fields.bs == (mode == 2 ? .375 : 0) &&
                  block.fields.bv == (mode == 2 ? .625 : 1) &&
                  block.fields.bu == (mode == 2 ? 0 : .375) &&
                  block.fields.bx == (mode == 2 ? 1 : .625));
        }
        const int32_t complete[9][3] = {{x, y, z},
                                        {edges[i].west[0], y, z},
                                        {edges[i].west[1], y, z},
                                        {edges[i].east[0], y, z},
                                        {edges[i].east[1], y, z},
                                        {x, y, edges[i].north[0]},
                                        {x, y, edges[i].north[1]},
                                        {x, y, edges[i].south[0]},
                                        {x, y, edges[i].south[1]}};
        const int32_t first_neighbors[5][3] = {{x, y, z},
                                               {edges[i].west[0], y, z},
                                               {edges[i].east[0], y, z},
                                               {x, y, edges[i].north[0]},
                                               {x, y, edges[i].south[0]}};
        for (int side = 0; side < 6; ++side) {
            trace = (coordinate_trace){.blocks = {0, 90, 0, 90, 0, 90, 0, 90, 0}};
            bool visible = true;
            CHECK(mc_block_render_side(&block, &access, x, y, z, side, &visible) ==
                      MC_REGISTRY_OK &&
                  visible == (side >= 2));
            CHECK(coordinate_trace_matches(&trace, 'B', complete, 9));
            trace = (coordinate_trace){0};
            CHECK(mc_block_render_side(&block, &access, x, y, z, side, &visible) ==
                      MC_REGISTRY_OK &&
                  !visible);
            CHECK(coordinate_trace_matches(&trace, 'B', first_neighbors, 5));
            trace = (coordinate_trace){.blocks = {90}};
            CHECK(mc_block_render_side(&block, &access, x, y, z, side, &visible) ==
                      MC_REGISTRY_OK &&
                  !visible);
            CHECK(coordinate_trace_matches(&trace, 'B', complete, 1));
        }
    }
    block.kind = MC_BLOCK_GRASS;
    static const int32_t heights[2][2] = {{INT32_MAX, INT32_MIN}, {INT32_MIN, INT32_MIN + 1}};
    const mc_material_id material_ids[3] = {MC_MATERIAL_t, MC_MATERIAL_u, MC_MATERIAL_e};
    for (size_t i = 0; i < sizeof(heights) / sizeof(*heights); ++i) {
        int32_t texture = -1;
        trace = (coordinate_trace){0};
        CHECK(mc_block_render_texture(&block, &access, INT32_MIN, heights[i][0], INT32_MAX, 0,
                                      &texture) == MC_REGISTRY_OK &&
              texture == 2 && trace.count == 0);
        CHECK(mc_block_render_texture(&block, &access, INT32_MIN, heights[i][0], INT32_MAX, 1,
                                      &texture) == MC_REGISTRY_OK &&
              texture == 0 && trace.count == 0);
        const int32_t above[1][3] = {{INT32_MIN, heights[i][1], INT32_MAX}};
        for (size_t material = 0; material < 3; ++material) {
            trace =
                (coordinate_trace){.material = mc_materials_get(materials, material_ids[material])};
            CHECK(mc_block_render_texture(&block, &access, INT32_MIN, heights[i][0], INT32_MAX, 2,
                                          &texture) == MC_REGISTRY_OK &&
                  texture == (material < 2 ? 68 : 3));
            CHECK(coordinate_trace_matches(&trace, 'M', above, 1));
        }
    }
    return true;
}

static bool block_predicates(mc_materials *materials) {
    /* Authored closed-kind receivers exercise pure virtual method contracts.
     * No source initializer or actual GAME owner is claimed by this fixture. */
    mc_registry_classes classes = {0};
    classes.options.materials = materials;
    block_grid grid = {.opaque = true, .material = mc_materials_get(materials, MC_MATERIAL_e)};
    mc_block_render_access access = {.owner = &grid,
                                     .block = grid_block,
                                     .metadata = grid_metadata,
                                     .material = grid_material,
                                     .opaque = grid_opaque,
                                     .climate = grid_climate,
                                     .grass_colors = grass,
                                     .foliage_colors = foliage,
                                     .grass_color_count = 65536,
                                     .foliage_color_count = 65536};
    mc_block b = {.classes = &classes,
                  .kind = MC_BLOCK_BASE,
                  .fields = {.bn = 1, .bm = 1, .bv = 1, .bw = 1, .bx = 1}};
    bool side = true;
    for (int s = 0; s < 6; ++s) {
        CHECK(mc_block_render_side(&b, &access, 0, 0, 0, s, &side) == MC_REGISTRY_OK);
        CHECK(!side);
    }
    grid.opacity_reads = 0;
    b.fields.bs = .125;
    CHECK(mc_block_render_side(&b, &access, 0, 0, 0, 4, &side) == MC_REGISTRY_OK && side);
    CHECK(grid.opacity_reads == 0);
    b.fields.bs = 0;
    grid.opaque = false;
    b.kind = MC_BLOCK_GLASS;
    grid.id = 1;
    b.nr_b = true;
    b.jp_a = false;
    CHECK(mc_block_render_side(&b, &access, 0, 0, 0, 2, &side) == MC_REGISTRY_OK && !side);
    b.jp_a = true;
    CHECK(mc_block_render_side(&b, &access, 0, 0, 0, 2, &side) == MC_REGISTRY_OK && side);
    b.kind = MC_BLOCK_SNOW_LAYER;
    grid.opaque = true;
    CHECK(mc_block_render_side(&b, &access, 0, 0, 0, 1, &side) == MC_REGISTRY_OK && side);
    grid.metadata = 0;
    CHECK(mc_block_render_update_bounds(&b, &access, 0, 0, 0) == MC_REGISTRY_OK &&
          b.fields.bw == .125);
    grid.metadata = 5;
    CHECK(mc_block_render_update_bounds(&b, &access, 0, 0, 0) == MC_REGISTRY_OK &&
          b.fields.bw == .75);
    b.kind = MC_BLOCK_ICE;
    grid.id = 0;
    b.jp_a = false;
    b.fields.bw = .5;
    CHECK(mc_block_render_side(&b, &access, 0, 0, 0, 0, &side) == MC_REGISTRY_OK && side);
    int32_t texture = -1;
    b.kind = MC_BLOCK_LOG;
    b.fields.bm = 20;
    CHECK(mc_block_render_texture_metadata(&b, 0, 2, &texture) == MC_REGISTRY_OK && texture == 21);
    CHECK(mc_block_render_texture_metadata(&b, 2, 2, &texture) == MC_REGISTRY_OK && texture == 117);
    b.kind = MC_BLOCK_WOOL;
    b.fields.bm = 64;
    CHECK(mc_block_render_texture_metadata(&b, 2, 0, &texture) == MC_REGISTRY_OK && texture == 64);
    CHECK(mc_block_render_texture_metadata(&b, 2, 15, &texture) == MC_REGISTRY_OK &&
          texture == 113);
    b.kind = MC_BLOCK_GRASS;
    grass[0] = 0xff112233;
    uint32_t color = 0;
    CHECK(mc_block_render_color(&b, &access, 0, 0, 0, &color) == MC_REGISTRY_OK &&
          color == 0x112233);
    CHECK(grid.climate_reads == 1);
    grid.material = mc_materials_get(materials, MC_MATERIAL_t);
    b.fields.bm = 3;
    CHECK(mc_block_render_texture(&b, &access, 0, 0, 0, 2, &texture) == MC_REGISTRY_OK &&
          texture == 68);
    CHECK(mc_block_render_texture(&b, &access, 0, 0, 0, 1, &texture) == MC_REGISTRY_OK &&
          texture == 0);
    b.kind = MC_BLOCK_LEAVES;
    grid.metadata = 1;
    CHECK(mc_block_render_color(&b, &access, 0, 0, 0, &color) == MC_REGISTRY_OK &&
          color == 0x619961);
    grid.metadata = 2;
    CHECK(mc_block_render_color(&b, &access, 0, 0, 0, &color) == MC_REGISTRY_OK &&
          color == 8431445);
    grid.metadata = 0;
    foliage[0] = 0xff2468ac;
    CHECK(mc_block_render_color(&b, &access, 0, 0, 0, &color) == MC_REGISTRY_OK &&
          color == 0x2468ac);
    classes.blocks_phase = MC_REGISTRY_READY;
    classes.blocks.named[17] = &b;
    b.subtype.bk.c = 52;
    CHECK(mc_block_render_fancy_leaves(&classes, true) == MC_REGISTRY_OK && b.nr_b &&
          b.fields.bm == 52);
    CHECK(!mc_block_render_opaque(&b));
    CHECK(mc_block_render_fancy_leaves(&classes, false) == MC_REGISTRY_OK && !b.nr_b &&
          b.fields.bm == 53);
    CHECK(mc_block_render_opaque(&b));
    mc_block_render_description description;
    b.kind = MC_BLOCK_CACTUS;
    CHECK(mc_block_render_describe(&b, &description) == MC_REGISTRY_OK && description.type == 13);
    b.kind = MC_BLOCK_PISTON_HEAD;
    CHECK(mc_block_render_describe(&b, &description) == MC_REGISTRY_OK && description.type == 17);
    return true;
}

int main(void) {
    mc_materials *materials = NULL;
    if (!mc_materials_create(&materials))
        return 1;
    bool ok = faces() && ambient_occlusion() && block_predicates(materials) &&
              wrapped_block_coordinates(materials);
    mc_materials_destroy(materials);
    if (!ok)
        return 1;
    printf("client World CPU mesher: %u checks passed\n", checks);
    return 0;
}
