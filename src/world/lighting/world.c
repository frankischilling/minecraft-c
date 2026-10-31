#include "mc/world/lighting/world.h"

#include "mc/core/java/java.h"

static bool queue_valid(const mc_light_queue *queue) {
    return queue != NULL && queue->capacity <= MC_LIGHT_QUEUE_LIMIT &&
           queue->count <= queue->capacity && (queue->capacity == 0 || queue->entries != NULL);
}

bool mc_world_lighting_validate(const mc_world_lighting *world) {
    if (world == NULL || world->enqueue_depth == NULL || !queue_valid(world->queue) ||
        !mc_chunk_cache_validate(world->cache)) {
        return false;
    }
    for (size_t i = 0; i < world->queue->count; ++i) {
        mc_light_kind kind = world->queue->entries[i].kind;
        if (kind != MC_LIGHT_SKY && kind != MC_LIGHT_BLOCK)
            return false;
    }
    return true;
}

bool mc_world_height(const mc_chunk_cache *cache, int32_t x, int32_t z, int32_t *height) {
    if (cache == NULL || height == NULL || cache->count > cache->capacity ||
        (cache->capacity > 0 && cache->entries == NULL)) {
        return false;
    }
    if (x < -32000000 || z < -32000000 || x >= 32000000 || z > 32000000) {
        *height = 0;
        return true;
    }
    mc_chunk_state *chunk;
    if (!mc_chunk_cache_lookup(cache, mc_java_i32_shr(x, 4), mc_java_i32_shr(z, 4), &chunk)) {
        *height = 0;
        return true;
    }
    if (chunk == NULL || (!chunk->blank && chunk->data == NULL))
        return false;
    size_t local_x = (uint32_t)x & 15u;
    size_t local_z = (uint32_t)z & 15u;
    *height = chunk->blank ? 0 : chunk->data->heightmap[local_z * 16u + local_x];
    return true;
}

mc_light_submit_result mc_world_light_submit(mc_world_lighting *world,
                                             const mc_light_region *region, bool coalesce) {
    if (world == NULL || world->cache == NULL || world->enqueue_depth == NULL ||
        !queue_valid(world->queue) || region == NULL ||
        (region->kind != MC_LIGHT_SKY && region->kind != MC_LIGHT_BLOCK)) {
        return MC_LIGHT_SUBMIT_INVALID;
    }
    if (world->no_sky && region->kind == MC_LIGHT_SKY)
        return MC_LIGHT_SUBMIT_SKIPPED;
    int32_t previous_depth = *world->enqueue_depth;
    *world->enqueue_depth = mc_java_i32_add(previous_depth, 1);
    mc_light_submit_result result = MC_LIGHT_SUBMIT_SKIPPED;
    if (*world->enqueue_depth != 50) {
        int32_t mid_x = mc_java_i32_add(region->bounds.max_x, region->bounds.min_x) / 2;
        int32_t mid_z = mc_java_i32_add(region->bounds.max_z, region->bounds.min_z) / 2;
        mc_chunk_state *chunk;
        if (mc_chunk_cache_lookup(world->cache, mc_java_i32_shr(mid_x, 4),
                                  mc_java_i32_shr(mid_z, 4), &chunk)) {
            if (chunk == NULL || (!chunk->blank && chunk->data == NULL)) {
                result = MC_LIGHT_SUBMIT_INVALID;
            } else if (!chunk->blank) {
                switch (mc_light_queue_enqueue(world->queue, region, coalesce)) {
                    case MC_LIGHT_QUEUE_MERGED:
                        result = MC_LIGHT_SUBMIT_MERGED;
                        break;
                    case MC_LIGHT_QUEUE_APPENDED:
                        result = MC_LIGHT_SUBMIT_APPENDED;
                        break;
                    case MC_LIGHT_QUEUE_CLEARED:
                        result = MC_LIGHT_SUBMIT_CLEARED;
                        break;
                    case MC_LIGHT_QUEUE_FULL:
                        result = MC_LIGHT_SUBMIT_FULL;
                        break;
                    default:
                        result = MC_LIGHT_SUBMIT_INVALID;
                        break;
                }
            }
        }
    }
    *world->enqueue_depth = previous_depth;
    return result;
}
