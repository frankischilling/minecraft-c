#include "mc/world/chunk/finish.h"

#include "mc/core/java/java.h"

bool mc_chunk_finish_normal(mc_chunk_state *chunk, mc_world_lighting *world, const int32_t *opacity,
                            size_t opacity_length, mc_chunk_finish_trace *trace,
                            size_t *queue_aborts)
{
    if (chunk == NULL || chunk->blank || chunk->data == NULL || chunk->data->blocks == NULL ||
        chunk->data->blocks_capacity < MC_CHUNK_BLOCKS || opacity == NULL ||
        opacity_length < 256u || !mc_world_lighting_validate(world) ||
        (trace != NULL && (trace->entries == NULL || trace->capacity < MC_CHUNK_FINISH_QUERIES))) {
        return false;
    }
    for (size_t i = 0; i < 256u; ++i) {
        if (opacity[i] < 0)
            return false;
    }
    mc_light_queue *queue = world->queue;
    if (queue->capacity != MC_LIGHT_QUEUE_LIMIT &&
        queue->capacity - queue->count < MC_CHUNK_FINISH_QUERIES) {
        return false;
    }
    /* Preflight above covers the local rebuild and every later admission/append.
     * A valid nonnull cache cannot load or change during these height queries. */
    (void)mc_chunk_data_rebuild_height_skylight(chunk->data, opacity, opacity_length,
                                                !world->no_sky);
    size_t index = 0, aborts = 0;
    static const int32_t dx[] = {-1, 1, 0, 0};
    static const int32_t dz[] = {0, 0, -1, 1};
    for (int32_t x = 0; x < 16; ++x) {
        for (int32_t z = 0; z < 16; ++z) {
            int32_t local = chunk->data->heightmap[(size_t)z * 16u + (size_t)x];
            int32_t world_x = mc_java_i32_add(mc_java_i32_mul(chunk->x, 16), x);
            int32_t world_z = mc_java_i32_add(mc_java_i32_mul(chunk->z, 16), z);
            for (size_t direction = 0; direction < 4u; ++direction) {
                int32_t qx = mc_java_i32_add(world_x, dx[direction]);
                int32_t qz = mc_java_i32_add(world_z, dz[direction]);
                int32_t height = 0;
                (void)mc_world_height(world->cache, qx, qz, &height);
                if (trace != NULL)
                    trace->entries[index] = (mc_chunk_height_query){qx, qz, local, height};
                ++index;
                if (height != local) {
                    mc_light_region region = {MC_LIGHT_SKY,
                                              {qx, height < local ? height : local, qz, qx,
                                               height > local ? height : local, qz}};
                    mc_light_submit_result result = mc_world_light_submit(world, &region, true);
                    if (result == MC_LIGHT_SUBMIT_CLEARED)
                        ++aborts;
                    chunk->dirty = true;
                }
            }
        }
    }
    chunk->dirty = true;
    if (trace != NULL)
        trace->count = index;
    if (queue_aborts != NULL)
        *queue_aborts = aborts;
    return true;
}
