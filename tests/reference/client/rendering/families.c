/* Authored IBlockAccess fixtures call the current CPU family functions.
 * No GAME World is allocated, injected or represented by these grids. */
#include "../../../../src/block/registry/registry_internal.h"
#include "../../../../src/client/world/mesher.c"
#include <inttypes.h>
#include <stdio.h>
static mc_registry_classes classes;
static mc_block blocks[256];
static mc_block *cells[256];
static bool flags[256];
static int32_t emission[256];
static mc_chunk_state chunks[3][3];
static mc_chunk_data data[3][3];
static uint8_t bytes[3][3][32768];
static mc_math_trig trig;

static uint32_t bits(float f) {
    uint32_t n;
    memcpy(&n, &f, 4);
    return n;
}

static void put(mesh_context *c, int x, int y, int z, int id, int m) {
    mc_chunk_state *ch = cached_chunk(c, x, z);
    if (ch == NULL) {
        fputs("authored voxel outside fixture cache\n", stderr);
        exit(2);
    }
    ch->data->blocks[(x & 15) * 2048 + (z & 15) * 128 + y] = (uint8_t)id;
    if (!mc_chunk_nibbles_set(ch->data->metadata, 16384, x & 15, y, z & 15, m)) {
        fputs("authored metadata outside fixture array\n", stderr);
        exit(2);
    }
}

int main(void) {
    mc_materials *materials = NULL;
    if (!mc_materials_create(&materials) || !mc_math_trig_init(&trig))
        return 2;
    classes.options.materials = materials;
    classes.blocks_phase = MC_REGISTRY_READY;
    classes.blocks.m = cells;
    const int ids[16] = {8, 8, 8, 8, 37, 31, 6, 59, 81, 53, 53, 85, 65, 65, 65, 65};
    const int initialized[11] = {1, 5, 8, 37, 31, 6, 59, 81, 53, 85, 65};
    const mc_block_kind kinds[11] = {MC_BLOCK_BASE,   MC_BLOCK_BASE,       MC_BLOCK_FLOWING_FLUID,
                                     MC_BLOCK_FLOWER, MC_BLOCK_TALL_GRASS, MC_BLOCK_SAPLING,
                                     MC_BLOCK_CROPS,  MC_BLOCK_CACTUS,     MC_BLOCK_STAIRS,
                                     MC_BLOCK_FENCE,  MC_BLOCK_LADDER};
    const int textures[11] = {1, 4, 205, 13, 39, 15, 88, 70, 4, 4, 83};
    const mc_material_id mids[11] = {MC_MATERIAL_e, MC_MATERIAL_d, MC_MATERIAL_g, MC_MATERIAL_j,
                                     MC_MATERIAL_j, MC_MATERIAL_j, MC_MATERIAL_j, MC_MATERIAL_v,
                                     MC_MATERIAL_d, MC_MATERIAL_d, MC_MATERIAL_o};
    flags[0] = true;
    for (int i = 0; i < 11; ++i) {
        int id = initialized[i];
        blocks[id] = (mc_block){.classes = &classes,
                                .kind = kinds[i],
                                .fields = {.bn = id,
                                           .bm = textures[i],
                                           .bv = 1,
                                           .bw = 1,
                                           .bx = 1,
                                           .bA = mc_materials_get(materials, mids[i])}};
        cells[id] = &blocks[id];
        flags[id] = !mc_material_b(blocks[id].fields.bA);
    }
    blocks[53].subtype.ss.a = &blocks[5];
    for (int test = 0; test < 16; ++test) {
        mesh_context c = {0};
        c.registry = &classes;
        c.chunk_x = -1;
        c.chunk_z = -3;
        c.neighbor_light_ids[0] = 44;
        c.neighbor_light_ids[1] = 60;
        c.neighbor_light_ids[2] = 53;
        c.neighbor_light_ids[3] = 67;
        c.options = mc_client_world_mesher_default_options();
        c.options.ambient_occlusion = false;
        c.options.trig = &trig;
        c.mesh.origin_y = 64;
        c.mesh.origin_z = -32;
        c.blocks.m = cells;
        c.blocks.r = flags;
        c.blocks.s = emission;
        float brightness_table[16];
        for (int i = 0; i < 16; ++i)
            brightness_table[i] = .75f;
        c.dimension.f = brightness_table;
        for (int x = 0; x < 3; ++x)
            for (int z = 0; z < 3; ++z) {
                memset(&data[x][z], 0, sizeof(data[x][z]));
                memset(bytes[x][z], 0, 32768);
                data[x][z].blocks = bytes[x][z];
                data[x][z].blocks_capacity = 32768;
                memset(data[x][z].skylight, 255, 16384);
                chunks[x][z] = (mc_chunk_state){.data = &data[x][z]};
                c.chunks[x][z] = &chunks[x][z];
            }
        c.access = (mc_block_render_access){.owner = &c,
                                            .block = read_block,
                                            .metadata = read_metadata,
                                            .material = read_material,
                                            .opaque = read_opaque};
        int id = ids[test], m = test == 1    ? 4
                                : test == 2  ? 8
                                : test == 6  ? 1
                                : test == 7  ? 5
                                : test == 10 ? 1
                                : test >= 12 ? 17 - test
                                             : 0;
        put(&c, 13, 64, -17, id, m);
        if (test == 1) {
            put(&c, 12, 64, -17, 8, 1);
            put(&c, 13, 64, -18, 8, 2);
        }
        if (test == 2) {
            put(&c, 14, 64, -17, 1, 0);
            put(&c, 14, 65, -17, 1, 0);
        }
        if (test == 3)
            for (int x = 12; x <= 14; ++x)
                for (int z = -18; z <= -16; ++z)
                    put(&c, x, 64, z, 8, 0);
        mc_block *block = &blocks[id];
        mc_block_render_description desc;
        if (mc_block_render_describe(block, &desc) != MC_REGISTRY_OK ||
            mc_block_render_update_bounds(block, &c.access, 13, 64, -17) != MC_REGISTRY_OK)
            return 3;
        bool ok = desc.type == 4    ? fluids(&c, block, 13, 64, -17)
                  : desc.type == 1  ? plants(&c, block, 13, 64, -17, &desc)
                  : desc.type == 6  ? crops(&c, block, 13, 64, -17)
                  : desc.type == 8  ? ladder(&c, block, 13, 64, -17)
                  : desc.type == 10 ? stairs(&c, block, 13, 64, -17, &desc)
                  : desc.type == 11 ? fence(&c, block, 13, 64, -17, &desc)
                                    : common(&c, block, 13, 64, -17, &desc);
        if (!ok || c.error.status != MC_CLIENT_WORLD_MESHER_OK) {
            fprintf(stderr, "failure %d %s\n", test, c.error.operation);
            return 4;
        }
        printf("case %d %zu\n", test, c.mesh.vertex_count[0]);
        for (size_t i = 0; i < c.mesh.vertex_count[0]; ++i) {
            mc_client_world_vertex v = c.mesh.vertices[0][i];
            printf("%d %zu %08" PRIx32 " %08" PRIx32 " %08" PRIx32 " %08" PRIx32 " %08" PRIx32
                   " %08" PRIx32 "\n",
                   test, i, bits(v.x), bits(v.y), bits(v.z), bits(v.u), bits(v.v), v.argb);
        }
        mc_client_world_cpu_mesh_free(&c.mesh);
    }
    mc_materials_destroy(materials);
    return 0;
}
