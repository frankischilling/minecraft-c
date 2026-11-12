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
    bool ok = faces() && ambient_occlusion() && block_predicates(materials);
    mc_materials_destroy(materials);
    if (!ok)
        return 1;
    printf("client World CPU mesher: %u checks passed\n", checks);
    return 0;
}
