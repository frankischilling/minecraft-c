#include "mesher.h"
#include "mesher_lighting.h"

#include "mc/block/registry/runtime.h"
#include "mc/core/java/java.h"
#include "mc/world/dimension/world_chunk_manager.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static const int32_t normal[6][3] = {{0, -1, 0}, {0, 1, 0},  {0, 0, -1},
                                     {0, 0, 1},  {-1, 0, 0}, {1, 0, 0}};
static const unsigned int triangle[6] = {0, 1, 2, 0, 2, 3};
static const float shade[6] = {.5f, 1.0f, .8f, .8f, .6f, .6f};

typedef struct mesh_context {
    mc_world_runtime *world;
    mc_world_runtime_fields fields;
    mc_world_provider_fields dimension;
    mc_registry_classes *registry;
    mc_registry_block_view blocks;
    mc_block_render_access access;
    mc_chunk_state *chunks[3][3];
    bool leased[3][3];
    int32_t neighbor_light_ids[4];
    int32_t chunk_x, chunk_z;
    mc_client_world_mesher_options options;
    mc_client_world_cpu_mesh mesh;
    size_t capacity[2];
    unsigned int pass;
    mc_client_world_mesher_error error;
} mesh_context;

static mc_client_world_mesher_status fail(mesh_context *c, mc_client_world_mesher_status status,
                                          const char *operation) {
    if (c->error.status == MC_CLIENT_WORLD_MESHER_OK) {
        c->error.status = status;
        c->error.operation = operation;
    }
    return c->error.status;
}

static bool registry_result(mesh_context *c, mc_registry_status status, const char *operation) {
    if (status == MC_REGISTRY_OK)
        return true;
    if (c->error.status == MC_CLIENT_WORLD_MESHER_OK) {
        c->error.registry = status;
        fail(c,
             status == MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY
                 ? MC_CLIENT_WORLD_MESHER_UNIMPLEMENTED_DEPENDENCY
                 : MC_CLIENT_WORLD_MESHER_REGISTRY_FAILED,
             operation);
    }
    return false;
}

mc_client_world_mesher_options mc_client_world_mesher_default_options(void) {
    return (mc_client_world_mesher_options){.ambient_occlusion = true,
                                            .fancy_leaves = true,
                                            .max_vertices = 16u * 16u * 16u * 6u * 6u * 2u};
}

void mc_client_world_cpu_mesh_free(mc_client_world_cpu_mesh *mesh) {
    if (mesh == NULL)
        return;
    free(mesh->vertices[0]);
    free(mesh->vertices[1]);
    *mesh = (mc_client_world_cpu_mesh){0};
}

static mc_chunk_state *cached_chunk(mesh_context *c, int32_t x, int32_t z) {
    int32_t cx = mc_java_i32_shr(x, 4) - c->chunk_x;
    int32_t cz = mc_java_i32_shr(z, 4) - c->chunk_z;
    if (cx < 0 || cx >= 3 || cz < 0 || cz >= 3)
        return NULL;
    return c->chunks[cx][cz];
}

static int32_t read_block(void *owner, int32_t x, int32_t y, int32_t z) {
    mesh_context *c = owner;
    if (c->error.status != MC_CLIENT_WORLD_MESHER_OK || y < 0 || y >= 128)
        return 0;
    mc_chunk_state *chunk = cached_chunk(c, x, z);
    if (chunk == NULL || chunk->blank)
        return 0; /* ew's actual bounds/blank guards. */
    const mc_chunk_data *d = chunk->data;
    if (d == NULL || d->blocks == NULL || d->blocks_capacity < MC_CHUNK_BLOCKS) {
        fail(c, MC_CLIENT_WORLD_MESHER_WORLD_FAILED, "ChunkCache block array");
        return 0;
    }
    size_t index = ((uint32_t)x & 15u) * 2048u + ((uint32_t)z & 15u) * 128u + (uint32_t)y;
    return d->blocks[index];
}

static int32_t read_metadata(void *owner, int32_t x, int32_t y, int32_t z) {
    mesh_context *c = owner;
    if (c->error.status != MC_CLIENT_WORLD_MESHER_OK || y < 0 || y >= 128)
        return 0;
    mc_chunk_state *chunk = cached_chunk(c, x, z);
    if (chunk == NULL) {
        fail(c, MC_CLIENT_WORLD_MESHER_WORLD_FAILED,
             "ChunkCache metadata outside acquired columns");
        return 0;
    }
    if (chunk->blank)
        return 0;
    uint8_t value = 0;
    if (chunk->data == NULL || !mc_chunk_nibbles_get(chunk->data->metadata, MC_CHUNK_NIBBLE_BYTES,
                                                     x & 15, y, z & 15, &value))
        fail(c, MC_CLIENT_WORLD_MESHER_WORLD_FAILED, "ChunkCache metadata nibble");
    return value;
}

static const mc_block *block_at(mesh_context *c, int32_t id) {
    if (id < 0 || id >= 256 || c->blocks.m == NULL) {
        fail(c, MC_CLIENT_WORLD_MESHER_REGISTRY_FAILED, "Block registry array");
        return NULL;
    }
    return c->blocks.m[id];
}

static mc_material *read_material(void *owner, int32_t x, int32_t y, int32_t z) {
    mesh_context *c = owner;
    int32_t id = read_block(c, x, y, z);
    if (id == 0) {
        mc_material *air = NULL;
        if (c->error.status == MC_CLIENT_WORLD_MESHER_OK)
            registry_result(c, mc_registry_material_get(c->registry, MC_MATERIAL_a, &air, NULL),
                            "IBlockAccess air Material");
        return air;
    }
    const mc_block *block = block_at(c, id);
    const mc_block_fields *f = mc_block_fields_get(block);
    if (f == NULL) {
        fail(c, MC_CLIENT_WORLD_MESHER_REGISTRY_FAILED, "IBlockAccess Block material");
        return NULL;
    }
    return f->bA;
}

static bool read_opaque(void *owner, int32_t x, int32_t y, int32_t z) {
    mesh_context *c = owner;
    return mc_block_render_opaque(block_at(c, read_block(c, x, y, z)));
}

static mc_registry_status read_climate(void *owner, int32_t x, int32_t z, double *temperature,
                                       double *rainfall) {
    mesh_context *c = owner;
    if (c->error.status != MC_CLIENT_WORLD_MESHER_OK)
        return MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY;
    mc_biome_reference_array *array = NULL;
    mc_world_chunk_manager_error error;
    mc_world_provider_status status =
        mc_world_chunk_manager_cached(c->dimension.b, x, z, 1, 1, &array, &error);
    if (status != MC_WORLD_PROVIDER_OK) {
        fail(c, MC_CLIENT_WORLD_MESHER_WORLD_FAILED, "IBlockAccess WorldChunkManager climate");
        return MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY;
    }
    mc_world_chunk_manager_fields fields;
    if (mc_world_chunk_manager_fields_get(c->dimension.b, &fields) != MC_WORLD_PROVIDER_OK ||
        fields.a == NULL || fields.b == NULL || fields.a->data == NULL || fields.b->data == NULL ||
        fields.a->length < 1 || fields.b->length < 1) {
        fail(c, MC_CLIENT_WORLD_MESHER_UNIMPLEMENTED_DEPENDENCY,
             "WorldChunkManager color channels");
        return MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY;
    }
    *temperature = fields.a->data[0];
    *rainfall = fields.b->data[0];
    return MC_REGISTRY_OK;
}

static int32_t light_level(mesh_context *c, int32_t x, int32_t y, int32_t z, bool neighbors) {
    if (c->error.status != MC_CLIENT_WORLD_MESHER_OK)
        return 0;
    if (x < -32000000 || z < -32000000 || x >= 32000000 || z > 32000000)
        return 15;
    if (neighbors) {
        int32_t id = read_block(c, x, y, z);
        if (id == c->neighbor_light_ids[0] || id == c->neighbor_light_ids[1] ||
            id == c->neighbor_light_ids[2] || id == c->neighbor_light_ids[3]) {
            int32_t result = light_level(c, x, y + 1, z, false);
            static const int32_t offset[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
            for (size_t i = 0; i < 4; ++i) {
                int32_t n = light_level(c, x + offset[i][0], y, z + offset[i][1], false);
                if (n > result)
                    result = n;
            }
            return result;
        }
    }
    if (y < 0)
        return 0;
    if (y >= 128) {
        int32_t result = 15 - c->fields.f;
        return result < 0 ? 0 : result;
    }
    mc_chunk_state *chunk = cached_chunk(c, x, z);
    if (chunk == NULL) {
        fail(c, MC_CLIENT_WORLD_MESHER_WORLD_FAILED, "ChunkCache light outside acquired columns");
        return 0;
    }
    if (chunk->blank)
        return 0; /* lm blank subclass combined-light returns0. */
    uint8_t sky, block;
    if (chunk->data == NULL ||
        !mc_chunk_nibbles_get(chunk->data->skylight, MC_CHUNK_NIBBLE_BYTES, x & 15, y, z & 15,
                              &sky) ||
        !mc_chunk_nibbles_get(chunk->data->block_light, MC_CHUNK_NIBBLE_BYTES, x & 15, y, z & 15,
                              &block)) {
        fail(c, MC_CLIENT_WORLD_MESHER_WORLD_FAILED, "ChunkCache light nibble");
        return 0;
    }
    int32_t result = (int32_t)sky - c->fields.f;
    return block > result ? block : result;
}

static float brightness(mesh_context *c, const mc_block *block, int32_t x, int32_t y, int32_t z) {
    const mc_block_fields *b = mc_block_fields_get(block);
    mc_block_render_description description;
    if (b == NULL || !registry_result(c, mc_block_render_describe(block, &description),
                                      "Block brightness receiver"))
        return 0;
    int32_t level = light_level(c, x, y, z, true);
    if (description.fluid) {
        int32_t above = light_level(c, x, y + 1, z, true);
        if (above > level)
            level = above;
    } else if (c->blocks.s != NULL && c->blocks.s[b->bn] > level)
        level = c->blocks.s[b->bn];
    if (level < 0 || level >= 16 || c->dimension.f == NULL) {
        fail(c, MC_CLIENT_WORLD_MESHER_WORLD_FAILED, "Dimension brightness table");
        return 0;
    }
    return c->dimension.f[level];
}

static uint32_t rgb(float r, float g, float b) {
    int32_t rr = mc_java_float_to_i32(r * 255.0f);
    int32_t gg = mc_java_float_to_i32(g * 255.0f);
    int32_t bb = mc_java_float_to_i32(b * 255.0f);
    rr = rr < 0 ? 0 : rr > 255 ? 255 : rr;
    gg = gg < 0 ? 0 : gg > 255 ? 255 : gg;
    bb = bb < 0 ? 0 : bb > 255 ? 255 : bb;
    return UINT32_C(0xff000000) | ((uint32_t)rr << 16) | ((uint32_t)gg << 8) | (uint32_t)bb;
}

static bool reserve(mesh_context *c, size_t count) {
    size_t total = c->mesh.vertex_count[0] + c->mesh.vertex_count[1];
    if (count > c->options.max_vertices || total > c->options.max_vertices - count) {
        fail(c, MC_CLIENT_WORLD_MESHER_LIMIT, "terrain vertex budget");
        return false;
    }
    size_t needed = c->mesh.vertex_count[c->pass] + count;
    if (needed <= c->capacity[c->pass])
        return true;
    size_t cap = c->capacity[c->pass] == 0 ? 4096 : c->capacity[c->pass];
    while (cap < needed && cap <= SIZE_MAX / 2)
        cap *= 2;
    if (cap > c->options.max_vertices)
        cap = c->options.max_vertices;
    if (cap < needed || cap > SIZE_MAX / sizeof(mc_client_world_vertex)) {
        fail(c, MC_CLIENT_WORLD_MESHER_LIMIT, "terrain vertex allocation extent");
        return false;
    }
    void *memory = realloc(c->mesh.vertices[c->pass], cap * sizeof(mc_client_world_vertex));
    if (memory == NULL) {
        fail(c, MC_CLIENT_WORLD_MESHER_ALLOCATION_FAILED, "terrain vertex allocation");
        return false;
    }
    c->mesh.vertices[c->pass] = memory;
    c->capacity[c->pass] = cap;
    return true;
}

static bool append_quad(mesh_context *c, const double positions[4][5], const uint32_t colors[4]) {
    if (c->error.status != MC_CLIENT_WORLD_MESHER_OK || !reserve(c, 6))
        return false;
    for (size_t i = 0; i < 6; ++i) {
        unsigned int j = triangle[i];
        c->mesh.vertices[c->pass][c->mesh.vertex_count[c->pass]++] =
            (mc_client_world_vertex){(float)(positions[j][0] + -(double)c->mesh.origin_x),
                                     (float)(positions[j][1] + -(double)c->mesh.origin_y),
                                     (float)(positions[j][2] + -(double)c->mesh.origin_z),
                                     (float)positions[j][3],
                                     (float)positions[j][4],
                                     colors[j]};
    }
    return true;
}

static bool append_face(mesh_context *c, const mc_block_fields *b, double x, double y, double z,
                        int32_t side, int32_t texture, const uint32_t colors[4]) {
    if (c->error.status != MC_CLIENT_WORLD_MESHER_OK || !reserve(c, 6))
        return false;
    if (!mc_client_world_mesher_face(b, c->mesh.origin_x, c->mesh.origin_y, c->mesh.origin_z, x, y,
                                     z, side, texture, colors,
                                     c->mesh.vertices[c->pass] + c->mesh.vertex_count[c->pass])) {
        fail(c, MC_CLIENT_WORLD_MESHER_REGISTRY_FAILED, "terrain face texture/bounds");
        return false;
    }
    c->mesh.vertex_count[c->pass] += 6;
    return true;
}

static bool visible(mesh_context *c, const mc_block *block, int32_t x, int32_t y, int32_t z,
                    int32_t side) {
    bool result = false;
    registry_result(c,
                    mc_block_render_side(block, &c->access, x + normal[side][0],
                                         y + normal[side][1], z + normal[side][2], side, &result),
                    "Block shouldSideBeRendered");
    return result;
}

static int32_t texture(mesh_context *c, const mc_block *block, int32_t x, int32_t y, int32_t z,
                       int32_t side) {
    int32_t result = 0;
    registry_result(c, mc_block_render_texture(block, &c->access, x, y, z, side, &result),
                    "Block side texture");
    return result;
}

static bool tint(mesh_context *c, const mc_block *block, int32_t x, int32_t y, int32_t z,
                 bool anaglyph, float color[3]) {
    uint32_t value = 0;
    if (!registry_result(c, mc_block_render_color(block, &c->access, x, y, z, &value),
                         "Block biome color multiplier / grasscolor or foliagecolor"))
        return false;
    float r = (float)((value >> 16) & 255) / 255.0f;
    float g = (float)((value >> 8) & 255) / 255.0f;
    float b = (float)(value & 255) / 255.0f;
    if (anaglyph) {
        color[0] = (r * 30.0f + g * 59.0f + b * 11.0f) / 100.0f;
        color[1] = (r * 30.0f + g * 70.0f) / 100.0f;
        color[2] = (r * 30.0f + b * 70.0f) / 100.0f;
    } else {
        color[0] = r;
        color[1] = g;
        color[2] = b;
    }
    return c->error.status == MC_CLIENT_WORLD_MESHER_OK;
}

typedef struct ao_receiver {
    mesh_context *context;
    const mc_block *block;
    int32_t x, y, z;
} ao_receiver;

static float ao_brightness(void *owner, int x, int y, int z) {
    const ao_receiver *a = owner;
    return brightness(a->context, a->block, a->x + x, a->y + y, a->z + z);
}

static bool ao_transmit(void *owner, int x, int y, int z) {
    const ao_receiver *a = owner;
    mesh_context *c = a->context;
    int32_t id = read_block(c, a->x + x, a->y + y, a->z + z);
    if (c->blocks.r == NULL || id < 0 || id >= 256) {
        fail(c, MC_CLIENT_WORLD_MESHER_REGISTRY_FAILED, "Block AO material flags");
        return false;
    }
    return c->blocks.r[id];
}

static bool common(mesh_context *c, const mc_block *block, int32_t x, int32_t y, int32_t z,
                   const mc_block_render_description *description) {
    const mc_block_fields *b = mc_block_fields_get(block);
    float color[3];
    if (!tint(c, block, x, y, z, c->options.anaglyph, color))
        return false;
    mc_client_world_ao ao;
    ao_receiver receiver = {c, block, x, y, z};
    bool smooth = c->options.ambient_occlusion && !description->cactus;
    float center;
    if (smooth) {
        mc_client_world_ao_prepare(&ao, &receiver, ao_brightness, ao_transmit);
        center = ao.n;
    } else
        center = brightness(c, block, x, y, z);
    for (int32_t side = 0; side < 6; ++side) {
        if (c->error.status != MC_CLIENT_WORLD_MESHER_OK)
            return false;
        if (!visible(c, block, x, y, z, side))
            continue;
        float corner[4];
        if (smooth)
            mc_client_world_ao_face(&ao, (unsigned int)side, corner);
        else {
            float light =
                brightness(c, block, x + normal[side][0], y + normal[side][1], z + normal[side][2]);
            if ((side == 1 && b->bw != 1.0 && !mc_material_d(b->bA)) || (side == 2 && b->bu > 0) ||
                (side == 3 && b->bx < 1) || (side == 4 && b->bs > 0) || (side == 5 && b->bv < 1))
                light = center;
            for (size_t i = 0; i < 4; ++i)
                corner[i] = light;
        }
        /* AO uses the actual bm==3 predicate; flat uses actual grass identity. */
        bool multiply = side == 1 || (smooth ? b->bm != 3 : !description->grass);
        uint32_t colors[4];
        float channels[4][3];
        for (size_t i = 0; i < 4; ++i) {
            for (size_t j = 0; j < 3; ++j)
                channels[i][j] = ((multiply ? color[j] : 1.0f) * shade[side]) * corner[i];
            colors[i] = rgb(channels[i][0], channels[i][1], channels[i][2]);
        }
        int32_t tex = texture(c, block, x, y, z, side);
        double dx = x, dz = z;
        if (description->cactus) {
            if (side == 2)
                dz += .0625f;
            if (side == 3)
                dz += -.0625f;
            if (side == 4)
                dx += .0625f;
            if (side == 5)
                dx += -.0625f;
        }
        if (!append_face(c, b, dx, y, dz, side, tex, colors))
            return false;
        if (!description->cactus && side >= 2 && tex == 3) {
            for (size_t i = 0; i < 4; ++i)
                colors[i] = rgb(channels[i][0] * color[0], channels[i][1] * color[1],
                                channels[i][2] * color[2]);
            if (!append_face(c, b, x, y, z, side, 38, colors))
                return false;
        }
    }
    return c->error.status == MC_CLIENT_WORLD_MESHER_OK;
}

static bool plants(mesh_context *c, const mc_block *block, int32_t x, int32_t y, int32_t z,
                   const mc_block_render_description *description) {
    float light = brightness(c, block, x, y, z), color[3];
    if (!tint(c, block, x, y, z, c->options.anaglyph, color))
        return false;
    uint32_t packed = rgb(light * color[0], light * color[1], light * color[2]);
    uint32_t colors[4] = {packed, packed, packed, packed};
    double xx = x, yy = y, zz = z;
    if (description->tall_grass) {
        int64_t n = (int64_t)mc_java_i32_mul(x, 3129871) ^ mc_java_i64_mul(z, 116129781) ^ y;
        n = mc_java_i64_add(mc_java_i64_mul(mc_java_i64_mul(n, n), 42317861),
                            mc_java_i64_mul(n, 11));
        xx += ((double)((float)(mc_java_i64_shr(n, 16) & 15) / 15.0f) - .5) * .5;
        yy += ((double)((float)(mc_java_i64_shr(n, 20) & 15) / 15.0f) - 1.0) * .2;
        zz += ((double)((float)(mc_java_i64_shr(n, 24) & 15) / 15.0f) - .5) * .5;
    }
    int32_t tex;
    if (!registry_result(
            c, mc_block_render_texture_metadata(block, 0, read_metadata(c, x, y, z), &tex),
            "crossed plant metadata texture"))
        return false;
    int32_t tu = (tex & 15) * 16, tv = tex & 240;
    double u0 = (float)tu / 256.0f, u1 = ((float)tu + 15.99f) / 256.0f;
    double v0 = (float)tv / 256.0f, v1 = ((float)tv + 15.99f) / 256.0f;
    double x0 = xx + .5 - (double).45f, x1 = xx + .5 + (double).45f;
    double z0 = zz + .5 - (double).45f, z1 = zz + .5 + (double).45f;
    const double quads[4][4][5] = {{{x0, yy + 1, z0, u0, v0},
                                    {x0, yy, z0, u0, v1},
                                    {x1, yy, z1, u1, v1},
                                    {x1, yy + 1, z1, u1, v0}},
                                   {{x1, yy + 1, z1, u0, v0},
                                    {x1, yy, z1, u0, v1},
                                    {x0, yy, z0, u1, v1},
                                    {x0, yy + 1, z0, u1, v0}},
                                   {{x0, yy + 1, z1, u0, v0},
                                    {x0, yy, z1, u0, v1},
                                    {x1, yy, z0, u1, v1},
                                    {x1, yy + 1, z0, u1, v0}},
                                   {{x1, yy + 1, z0, u0, v0},
                                    {x1, yy, z0, u0, v1},
                                    {x0, yy, z1, u1, v1},
                                    {x0, yy + 1, z1, u1, v0}}};
    for (size_t i = 0; i < 4; ++i)
        if (!append_quad(c, quads[i], colors))
            return false;
    return true;
}

static float fluid_depth(int32_t metadata) {
    if (metadata >= 8)
        metadata = 0;
    return (float)(metadata + 1) / 9.0f;
}

static float fluid_height(mesh_context *c, int32_t x, int32_t y, int32_t z, mc_material *material) {
    int32_t weight = 0;
    float sum = 0;
    for (int32_t i = 0; i < 4; ++i) {
        int32_t xx = x - (i & 1), zz = z - ((i >> 1) & 1);
        if (read_material(c, xx, y + 1, zz) == material)
            return 1.0f;
        mc_material *current = read_material(c, xx, y, zz);
        if (current == material) {
            int32_t m = read_metadata(c, xx, y, zz);
            if (m >= 8 || m == 0) {
                sum += fluid_depth(m) * 10.0f;
                weight += 10;
            }
            sum += fluid_depth(m);
            ++weight;
        } else if (current != NULL && !mc_material_a(current)) {
            sum += 1.0f;
            ++weight;
        }
    }
    return 1.0f - sum / (float)weight;
}

static int32_t fluid_decay(mesh_context *c, int32_t x, int32_t y, int32_t z,
                           mc_material *material) {
    if (read_material(c, x, y, z) != material)
        return -1;
    int32_t value = read_metadata(c, x, y, z);
    return value >= 8 ? 0 : value;
}

static void normalize(double vector[3]) {
    double length =
        (float)sqrt(vector[0] * vector[0] + vector[1] * vector[1] + vector[2] * vector[2]);
    if (length < 1.0e-4)
        vector[0] = vector[1] = vector[2] = 0.0;
    else
        for (size_t i = 0; i < 3; ++i)
            vector[i] /= length;
}

static float fluid_angle(mesh_context *c, int32_t x, int32_t y, int32_t z, mc_material *material) {
    static const int32_t offset[4][2] = {{-1, 0}, {0, -1}, {1, 0}, {0, 1}};
    int32_t center = fluid_decay(c, x, y, z, material);
    double vector[3] = {0};
    for (size_t i = 0; i < 4; ++i) {
        int32_t xx = x + offset[i][0], zz = z + offset[i][1];
        int32_t neighbor = fluid_decay(c, xx, y, zz, material), difference;
        if (neighbor < 0) {
            mc_material *m = read_material(c, xx, y, zz);
            if (m == NULL || mc_material_c(m) ||
                (neighbor = fluid_decay(c, xx, y - 1, zz, material)) < 0)
                continue;
            difference = neighbor - (center - 8);
        } else
            difference = neighbor - center;
        vector[0] += (double)(offset[i][0] * difference);
        vector[2] += (double)(offset[i][1] * difference);
    }
    if (read_metadata(c, x, y, z) >= 8) {
        bool obstruction = false;
        /* rp.d's Material solidity (not shouldSideBeRendered) is used here.
         * Same material and ice returnfalse before the base Material.a call. */
        mc_material *ice = NULL;
        registry_result(c, mc_registry_material_get(c->registry, MC_MATERIAL_s, &ice, NULL),
                        "flow ice Material");
        static const int32_t sides[4][2] = {{0, -1}, {0, 1}, {-1, 0}, {1, 0}};
        for (int32_t yy = 0; yy < 2 && !obstruction; ++yy)
            for (size_t i = 0; i < 4 && !obstruction; ++i) {
                mc_material *neighbor = read_material(c, x + sides[i][0], y + yy, z + sides[i][1]);
                obstruction = neighbor != NULL && neighbor != material && neighbor != ice &&
                              mc_material_a(neighbor);
            }
        if (obstruction) {
            normalize(vector);
            vector[1] += -6.0;
        }
    }
    normalize(vector);
    if (vector[0] == 0.0 && vector[2] == 0.0)
        return -1000.0f;
    return (float)(atan2(vector[2], vector[0]) - 1.5707963267948966);
}

static bool fluids(mesh_context *c, mc_block *block, int32_t x, int32_t y, int32_t z) {
    const mc_block_fields *b = mc_block_fields_get(block);
    float color[3];
    if (!tint(c, block, x, y, z, false, color))
        return false; /* cv.k has no anaglyph branch. */
    bool faces[6];
    faces[1] = visible(c, block, x, y, z, 1);
    faces[0] = visible(c, block, x, y, z, 0);
    bool any = faces[1] || faces[0];
    for (int32_t side = 2; side < 6; ++side) {
        faces[side] = visible(c, block, x, y, z, side);
        any |= faces[side];
    }
    if (!any)
        return c->error.status == MC_CLIENT_WORLD_MESHER_OK;
    if (c->options.trig == NULL) {
        fail(c, MC_CLIENT_WORLD_MESHER_UNIMPLEMENTED_DEPENDENCY, "fluid Minecraft trig table");
        return false;
    }
    int32_t m = read_metadata(c, x, y, z);
    float h[4];
    h[0] = fluid_height(c, x, y, z, b->bA);
    h[1] = fluid_height(c, x, y, z + 1, b->bA);
    h[2] = fluid_height(c, x + 1, y, z + 1, b->bA);
    h[3] = fluid_height(c, x + 1, y, z, b->bA);
    uint32_t colors[4];
    if (faces[1]) {
        int32_t tex;
        if (!registry_result(c, mc_block_render_texture_metadata(block, 1, m, &tex),
                             "fluid top texture"))
            return false;
        float angle = fluid_angle(c, x, y, z, b->bA);
        if (angle > -999.0f &&
            !registry_result(c, mc_block_render_texture_metadata(block, 2, m, &tex),
                             "fluid flowing top texture"))
            return false;
        int32_t tu = (tex & 15) * 16, tv = tex & 240;
        double u = ((double)tu + 8.0) / 256.0, v = ((double)tv + 8.0) / 256.0;
        if (angle < -999.0f)
            angle = 0;
        else {
            u = (float)(tu + 16) / 256.0f;
            v = (float)(tv + 16) / 256.0f;
        }
        float ss = mc_math_sin(c->options.trig, angle) * 8.0f / 256.0f;
        float cc = mc_math_cos(c->options.trig, angle) * 8.0f / 256.0f;
        float light = brightness(c, block, x, y, z);
        uint32_t packed =
            rgb((1.0f * light) * color[0], (1.0f * light) * color[1], (1.0f * light) * color[2]);
        for (size_t i = 0; i < 4; ++i)
            colors[i] = packed;
        const double quad[4][5] = {
            {x, (float)y + h[0], z, u - (double)cc - (double)ss, v - (double)cc + (double)ss},
            {x, (float)y + h[1], z + 1, u - (double)cc + (double)ss, v + (double)cc + (double)ss},
            {x + 1, (float)y + h[2], z + 1, u + (double)cc + (double)ss,
             v + (double)cc - (double)ss},
            {x + 1, (float)y + h[3], z, u + (double)cc - (double)ss, v - (double)cc - (double)ss}};
        if (!append_quad(c, quad, colors))
            return false;
    }
    if (faces[0]) {
        float light = brightness(c, block, x, y - 1, z);
        uint32_t packed = rgb(.5f * light, .5f * light, .5f * light);
        for (size_t i = 0; i < 4; ++i)
            colors[i] = packed;
        int32_t tex;
        if (!registry_result(c, mc_block_render_texture_metadata(block, 0, 0, &tex),
                             "fluid bottom texture") ||
            !append_face(c, b, x, y, z, 0, tex, colors))
            return false;
    }
    for (int32_t side = 2; side < 6; ++side) {
        int32_t tex;
        /* The original calls a(side,meta) before the visibility branch. */
        if (!registry_result(c, mc_block_render_texture_metadata(block, side, m, &tex),
                             "fluid side texture"))
            return false;
        if (!faces[side])
            continue;
        float first, second, ax, az, bx, bz;
        if (side == 2) {
            first = h[0];
            second = h[3];
            ax = (float)x;
            bx = (float)(x + 1);
            az = bz = (float)z;
        } else if (side == 3) {
            first = h[2];
            second = h[1];
            ax = (float)(x + 1);
            bx = (float)x;
            az = bz = (float)(z + 1);
        } else if (side == 4) {
            first = h[1];
            second = h[0];
            ax = bx = (float)x;
            az = (float)(z + 1);
            bz = (float)z;
        } else {
            first = h[3];
            second = h[2];
            ax = bx = (float)(x + 1);
            az = (float)z;
            bz = (float)(z + 1);
        }
        int32_t tu = (tex & 15) * 16, tv = tex & 240;
        double u0 = (float)(tu + 0) / 256.0f, u1 = ((double)(tu + 16) - .01) / 256.0;
        double v0 = ((float)tv + (1.0f - first) * 16.0f) / 256.0f;
        double v1 = ((float)tv + (1.0f - second) * 16.0f) / 256.0f;
        double vb = ((double)(tv + 16) - .01) / 256.0;
        float light = brightness(c, block, x + normal[side][0], y, z + normal[side][2]);
        light *= shade[side];
        uint32_t packed =
            rgb((1.0f * light) * color[0], (1.0f * light) * color[1], (1.0f * light) * color[2]);
        for (size_t i = 0; i < 4; ++i)
            colors[i] = packed;
        const double quad[4][5] = {{ax, (float)y + first, az, u0, v0},
                                   {bx, (float)y + second, bz, u1, v1},
                                   {bx, y, bz, u1, vb},
                                   {ax, y, az, u0, vb}};
        if (!append_quad(c, quad, colors))
            return false;
    }
    /* cv.k's final direct bt/bw stores restore precisely these fields. */
    registry_result(c, mc_block_render_finish_fluid(block), "fluid final bounds stores");
    return c->error.status == MC_CLIENT_WORLD_MESHER_OK;
}

static bool crops(mesh_context *c, const mc_block *block, int32_t x, int32_t y, int32_t z) {
    float light = brightness(c, block, x, y, z);
    uint32_t packed = rgb(light, light, light), colors[4] = {packed, packed, packed, packed};
    int32_t tex;
    if (!registry_result(
            c, mc_block_render_texture_metadata(block, 0, read_metadata(c, x, y, z), &tex),
            "crop growth texture"))
        return false;
    int32_t tu = (tex & 15) * 16, tv = tex & 240;
    double u0 = (float)tu / 256.0f, u1 = ((float)tu + 15.99f) / 256.0f;
    double v0 = (float)tv / 256.0f, v1 = ((float)tv + 15.99f) / 256.0f;
    double yy = (float)y - .0625f;
    static const double planes[8][4] = {{.25, 0, .25, 1}, {.25, 1, .25, 0}, {.75, 1, .75, 0},
                                        {.75, 0, .75, 1}, {0, .25, 1, .25}, {1, .25, 0, .25},
                                        {1, .75, 0, .75}, {0, .75, 1, .75}};
    for (size_t i = 0; i < 8; ++i) {
        double ax = (double)x + planes[i][0], az = (double)z + planes[i][1];
        double bx = (double)x + planes[i][2], bz = (double)z + planes[i][3];
        const double quad[4][5] = {{ax, yy + 1, az, u0, v0},
                                   {ax, yy, az, u0, v1},
                                   {bx, yy, bz, u1, v1},
                                   {bx, yy + 1, bz, u1, v0}};
        if (!append_quad(c, quad, colors))
            return false;
    }
    return true;
}

static bool bound_box(mesh_context *c, mc_block *block, int32_t x, int32_t y, int32_t z,
                      const mc_block_render_description *description, float x0, float y0, float z0,
                      float x1, float y1, float z1) {
    return registry_result(c, mc_block_render_bounds(block, x0, y0, z0, x1, y1, z1),
                           "composite Block bounds") &&
           common(c, block, x, y, z, description);
}

static bool stairs(mesh_context *c, mc_block *block, int32_t x, int32_t y, int32_t z,
                   const mc_block_render_description *description) {
    static const float boxes[4][2][6] = {{{0, 0, 0, .5f, .5f, 1}, {.5f, 0, 0, 1, 1, 1}},
                                         {{0, 0, 0, .5f, 1, 1}, {.5f, 0, 0, 1, .5f, 1}},
                                         {{0, 0, 0, 1, .5f, .5f}, {0, 0, .5f, 1, 1, 1}},
                                         {{0, 0, 0, 1, 1, .5f}, {0, 0, .5f, 1, .5f, 1}}};
    int32_t m = read_metadata(c, x, y, z);
    if (m >= 0 && m < 4)
        for (size_t i = 0; i < 2; ++i) {
            const float *b = boxes[m][i];
            if (!bound_box(c, block, x, y, z, description, b[0], b[1], b[2], b[3], b[4], b[5]))
                return false;
        }
    return registry_result(c, mc_block_render_bounds(block, 0, 0, 0, 1, 1, 1),
                           "stairs final bounds");
}

static bool fence(mesh_context *c, mc_block *block, int32_t x, int32_t y, int32_t z,
                  const mc_block_render_description *description) {
    if (!bound_box(c, block, x, y, z, description, .375f, 0, .375f, .625f, 1, .625f))
        return false;
    int32_t id = mc_block_fields_get(block)->bn;
    bool along_x = read_block(c, x - 1, y, z) == id || read_block(c, x + 1, y, z) == id;
    bool along_z = read_block(c, x, y, z - 1) == id || read_block(c, x, y, z + 1) == id;
    bool west = read_block(c, x - 1, y, z) == id, east = read_block(c, x + 1, y, z) == id;
    bool north = read_block(c, x, y, z - 1) == id, south = read_block(c, x, y, z + 1) == id;
    if (!along_x && !along_z)
        along_x = true;
    float x0 = west ? 0 : .4375f, x1 = east ? 1 : .5625f, z0 = north ? 0 : .4375f,
          z1 = south ? 1 : .5625f;
    for (size_t layer = 0; layer < 2; ++layer) {
        float y0 = layer == 0 ? .75f : .375f, y1 = layer == 0 ? .9375f : .5625f;
        if (along_x && !bound_box(c, block, x, y, z, description, x0, y0, .4375f, x1, y1, .5625f))
            return false;
        if (along_z && !bound_box(c, block, x, y, z, description, .4375f, y0, z0, .5625f, y1, z1))
            return false;
    }
    return registry_result(c, mc_block_render_bounds(block, 0, 0, 0, 1, 1, 1),
                           "fence final bounds");
}

static bool ladder(mesh_context *c, const mc_block *block, int32_t x, int32_t y, int32_t z) {
    int32_t tex;
    if (!registry_result(c, mc_block_render_texture_metadata(block, 0, 0, &tex), "ladder texture"))
        return false;
    float light = brightness(c, block, x, y, z);
    uint32_t packed = rgb(light, light, light), colors[4] = {packed, packed, packed, packed};
    int32_t tu = (tex & 15) * 16, tv = tex & 240;
    double u0 = (float)tu / 256.0f, u1 = ((float)tu + 15.99f) / 256.0f;
    double v0 = (float)tv / 256.0f, v1 = ((float)tv + 15.99f) / 256.0f;
    int32_t m = read_metadata(c, x, y, z);
    if (m < 2 || m > 5)
        return true; /* Original invalid metadata emits no vertices. */
    float x0 = (float)x, x1 = (float)(x + 1), y0 = (float)y, y1 = (float)(y + 1), z0 = (float)z,
          z1 = (float)(z + 1);
    double quad[4][5];
    if (m == 5) {
        const double q[4][5] = {{x0 + .05f, y1, z1, u0, v0},
                                {x0 + .05f, y0, z1, u0, v1},
                                {x0 + .05f, y0, z0, u1, v1},
                                {x0 + .05f, y1, z0, u1, v0}};
        memcpy(quad, q, sizeof(q));
    } else if (m == 4) {
        const double q[4][5] = {{x1 - .05f, y0, z1, u1, v1},
                                {x1 - .05f, y1, z1, u1, v0},
                                {x1 - .05f, y1, z0, u0, v0},
                                {x1 - .05f, y0, z0, u0, v1}};
        memcpy(quad, q, sizeof(q));
    } else if (m == 3) {
        const double q[4][5] = {{x1, y0, z0 + .05f, u1, v1},
                                {x1, y1, z0 + .05f, u1, v0},
                                {x0, y1, z0 + .05f, u0, v0},
                                {x0, y0, z0 + .05f, u0, v1}};
        memcpy(quad, q, sizeof(q));
    } else {
        const double q[4][5] = {{x1, y1, z1 - .05f, u0, v0},
                                {x1, y0, z1 - .05f, u0, v1},
                                {x0, y0, z1 - .05f, u1, v1},
                                {x0, y1, z1 - .05f, u1, v0}};
        memcpy(quad, q, sizeof(q));
    }
    return append_quad(c, (const double(*)[5])quad, colors);
}

static const char *family_dependency(int32_t type) {
    switch (type) {
        case -1:
            return "TileEntity renderer geometry";
        case 2:
            return "torch metadata orientation and tilted geometry";
        case 3:
            return "fire flammability, neighbors and planes";
        case 5:
            return "redstone power and connection geometry";
        case 6:
            return "crop growth and four crossed-plane pairs";
        case 7:
            return "door metadata, bounds and mirrored UV";
        case 8:
            return "ladder metadata attachment plane";
        case 9:
            return "rail slope, power and UV orientation";
        case 10:
            return "stairs oriented repeated bounds";
        case 11:
            return "fence connected repeated bounds";
        case 12:
            return "lever mount and handle geometry";
        case 14:
            return "bed head and direction geometry";
        case 15:
            return "repeater delay and oriented torch pieces";
        case 16:
            return "piston base state and rotated UV";
        case 17:
            return "piston extension shaft and rotated UV";
        default:
            return "unrecognized Block render type";
    }
}

mc_client_world_mesher_status
mc_client_world_mesher_build(mc_world_runtime *world, int32_t ox, int32_t oy, int32_t oz,
                             const mc_client_world_mesher_options *options,
                             mc_client_world_cpu_mesh *out, mc_client_world_mesher_error *error) {
    mesh_context c = {0};
    c.world = world;
    c.options = options == NULL ? mc_client_world_mesher_default_options() : *options;
    c.mesh.origin_x = ox;
    c.mesh.origin_y = oy;
    c.mesh.origin_z = oz;
    c.error.x = ox;
    c.error.y = oy;
    c.error.z = oz;
    c.error.block_id = c.error.render_type = -1;
    bool held_world = false, held_dimension = false, held_manager = false;
    mc_world_runtime_construction construction;
    if (world == NULL || out == NULL || ox < -32000000 || ox > 31999984 || oz < -32000000 ||
        oz > 31999984 || oy < 0 || oy > 112 ||
        (((uint32_t)ox | (uint32_t)oy | (uint32_t)oz) & 15u) != 0 ||
        (c.options.grass_color_count != 0 && c.options.grass_colors == NULL) ||
        (c.options.foliage_color_count != 0 && c.options.foliage_colors == NULL) ||
        mc_world_runtime_construction_get(world, &construction) != MC_WORLD_STORAGE_OK ||
        construction.mode != MC_WORLD_RUNTIME_GAME ||
        construction.phase != MC_WORLD_RUNTIME_READY) {
        fail(&c, MC_CLIENT_WORLD_MESHER_INVALID_ARGUMENT, "READY GAME World aligned section");
        goto finish;
    }
    if (!(held_world = mc_world_runtime_binding_retain(world)) ||
        mc_world_runtime_fields_get(world, &c.fields) != MC_WORLD_STORAGE_OK ||
        c.fields.t == NULL || c.fields.w == NULL ||
        mc_world_provider_retain(c.fields.t) != MC_WORLD_PROVIDER_OK) {
        fail(&c, MC_CLIENT_WORLD_MESHER_WORLD_FAILED, "World renderer dependency holds");
        goto finish;
    }
    held_dimension = true;
    if (mc_world_provider_fields_get(c.fields.t, &c.dimension) != MC_WORLD_PROVIDER_OK ||
        c.dimension.f == NULL || c.dimension.b == NULL ||
        mc_world_chunk_manager_retain(c.dimension.b) != MC_WORLD_PROVIDER_OK) {
        fail(&c, MC_CLIENT_WORLD_MESHER_WORLD_FAILED, "World renderer Dimension/manager");
        goto finish;
    }
    held_manager = true;
    c.registry = mc_world_runtime_registry(world);
    if (c.registry == NULL ||
        !registry_result(&c, mc_registry_classes_initialize_blocks(c.registry, NULL),
                         "Block active use") ||
        !registry_result(&c, mc_registry_classes_inspect_blocks(c.registry, &c.blocks, NULL),
                         "Block renderer view") ||
        c.blocks.phase != MC_REGISTRY_READY || c.blocks.m == NULL || c.blocks.r == NULL ||
        c.blocks.s == NULL ||
        !registry_result(&c, mc_block_render_fancy_leaves(c.registry, c.options.fancy_leaves),
                         "RenderGlobal leaves graphics setting")) {
        if (c.error.status == MC_CLIENT_WORLD_MESHER_OK)
            fail(&c, MC_CLIENT_WORLD_MESHER_REGISTRY_FAILED, "Block renderer READY arrays");
        goto finish;
    }
    static const char *const light_fields[4] = {"al", "aB", "au", "aI"};
    for (size_t i = 0; i < 4; ++i) {
        const mc_block *source = NULL;
        if (!registry_result(&c,
                             mc_registry_classes_inspect_block_static(c.registry, light_fields[i],
                                                                      &source, NULL),
                             "ChunkCache neighbor-light Block identity"))
            goto finish;
        const mc_block_fields *fields = mc_block_fields_get(source);
        if (fields == NULL) {
            fail(&c, MC_CLIENT_WORLD_MESHER_REGISTRY_FAILED,
                 "ChunkCache neighbor-light static receiver");
            goto finish;
        }
        c.neighbor_light_ids[i] = fields->bn;
    }
    c.chunk_x = mc_java_i32_shr(ox - 1, 4);
    c.chunk_z = mc_java_i32_shr(oz - 1, 4);
    for (int32_t x = 0; x < 3; ++x) {
        for (int32_t z = 0; z < 3; ++z) {
            mc_chunk_acquire_status status =
                mc_world_runtime_get(world, c.chunk_x + x, c.chunk_z + z, &c.chunks[x][z]);
            if (status != MC_CHUNK_ACQUIRE_OK || c.chunks[x][z] == NULL) {
                const mc_world_storage_error *cause = mc_world_runtime_last_error(world);
                if (cause != NULL)
                    c.error.world = *cause;
                fail(&c, MC_CLIENT_WORLD_MESHER_WORLD_FAILED,
                     "ew source-ordered padded Chunk acquisition");
                goto finish;
            }
            if (mc_world_store_owns_chunk(c.fields.w, c.chunks[x][z])) {
                if (mc_world_store_lease_chunk(c.fields.w, c.chunks[x][z], &c.error.world) !=
                    MC_WORLD_STORAGE_OK) {
                    fail(&c, MC_CLIENT_WORLD_MESHER_WORLD_FAILED, "ew Chunk identity lease");
                    goto finish;
                }
                c.leased[x][z] = true;
            } else if (!c.chunks[x][z]->blank) {
                fail(&c, MC_CLIENT_WORLD_MESHER_WORLD_FAILED, "ew nonowned normal Chunk identity");
                goto finish;
            }
        }
    }
    c.access = (mc_block_render_access){.owner = &c,
                                        .block = read_block,
                                        .metadata = read_metadata,
                                        .material = read_material,
                                        .opaque = read_opaque,
                                        .climate = read_climate,
                                        .grass_colors = c.options.grass_colors,
                                        .foliage_colors = c.options.foliage_colors,
                                        .grass_color_count = c.options.grass_color_count,
                                        .foliage_color_count = c.options.foliage_color_count};
    for (c.pass = 0; c.pass < 2; ++c.pass) {
        for (int32_t y = oy; y < oy + 16; ++y) {
            for (int32_t z = oz; z < oz + 16; ++z) {
                for (int32_t x = ox; x < ox + 16; ++x) {
                    c.error.x = x;
                    c.error.y = y;
                    c.error.z = z;
                    int32_t id = read_block(&c, x, y, z);
                    if (c.error.status != MC_CLIENT_WORLD_MESHER_OK)
                        goto finish;
                    if (id == 0)
                        continue;
                    c.error.block_id = id;
                    mc_block *block = c.blocks.m[id];
                    mc_block_render_description description;
                    if (!registry_result(&c, mc_block_render_describe(block, &description),
                                         "Block render dispatcher"))
                        goto finish;
                    c.error.render_type = description.type;
                    if (description.pass != (int32_t)c.pass)
                        continue;
                    if (description.type != 0 && description.type != 1 && description.type != 4 &&
                        description.type != 6 && description.type != 8 && description.type != 10 &&
                        description.type != 11 && description.type != 13) {
                        fail(&c, MC_CLIENT_WORLD_MESHER_UNIMPLEMENTED_DEPENDENCY,
                             family_dependency(description.type));
                        goto finish;
                    }
                    if (!registry_result(&c,
                                         mc_block_render_update_bounds(block, &c.access, x, y, z),
                                         "Block IBlockAccess bounds callback"))
                        goto finish;
                    bool success;
                    switch (description.type) {
                        case 1:
                            success = plants(&c, block, x, y, z, &description);
                            break;
                        case 4:
                            success = fluids(&c, block, x, y, z);
                            break;
                        case 6:
                            success = crops(&c, block, x, y, z);
                            break;
                        case 8:
                            success = ladder(&c, block, x, y, z);
                            break;
                        case 10:
                            success = stairs(&c, block, x, y, z, &description);
                            break;
                        case 11:
                            success = fence(&c, block, x, y, z, &description);
                            break;
                        default:
                            success = common(&c, block, x, y, z, &description);
                            break;
                    }
                    if (!success || c.error.status != MC_CLIENT_WORLD_MESHER_OK)
                        goto finish;
                }
            }
        }
    }
finish:
    for (size_t x = 0; x < 3; ++x)
        for (size_t z = 0; z < 3; ++z)
            if (c.leased[x][z]) {
                mc_world_storage_error cause;
                if (mc_world_store_release_chunk(c.fields.w, c.chunks[x][z], &cause) !=
                        MC_WORLD_STORAGE_OK &&
                    c.error.status == MC_CLIENT_WORLD_MESHER_OK) {
                    c.error.world = cause;
                    fail(&c, MC_CLIENT_WORLD_MESHER_WORLD_FAILED, "ew Chunk lease release");
                }
            }
    if (held_manager)
        mc_world_chunk_manager_release(c.dimension.b);
    if (held_dimension)
        mc_world_provider_release(c.fields.t);
    if (held_world)
        mc_world_runtime_binding_release(world);
    if (c.error.status == MC_CLIENT_WORLD_MESHER_OK) {
        *out = c.mesh;
        if (error != NULL)
            *error = (mc_client_world_mesher_error){0};
    } else {
        mc_client_world_cpu_mesh_free(&c.mesh);
        if (error != NULL)
            *error = c.error;
    }
    return c.error.status;
}
