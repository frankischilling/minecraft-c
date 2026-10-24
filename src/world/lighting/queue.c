#include "mc/world/lighting/queue.h"

#include "mc/core/java/java.h"

static int32_t volume(const mc_light_bounds *bounds) {
    int32_t x = mc_java_i32_sub(bounds->max_x, bounds->min_x);
    int32_t y = mc_java_i32_sub(bounds->max_y, bounds->min_y);
    int32_t z = mc_java_i32_sub(bounds->max_z, bounds->min_z);
    return mc_java_i32_mul(mc_java_i32_mul(x, y), z);
}

bool mc_light_bounds_merge(mc_light_bounds *stored, const mc_light_bounds *requested) {
    if (stored == NULL || requested == NULL) {
        return false;
    }
    mc_light_bounds next = *requested;
    if (next.min_x >= stored->min_x && next.min_y >= stored->min_y && next.min_z >= stored->min_z &&
        next.max_x <= stored->max_x && next.max_y <= stored->max_y && next.max_z <= stored->max_z) {
        return true;
    }
    if (next.min_x < mc_java_i32_sub(stored->min_x, 1) ||
        next.min_y < mc_java_i32_sub(stored->min_y, 1) ||
        next.min_z < mc_java_i32_sub(stored->min_z, 1) ||
        next.max_x > mc_java_i32_add(stored->max_x, 1) ||
        next.max_y > mc_java_i32_add(stored->max_y, 1) ||
        next.max_z > mc_java_i32_add(stored->max_z, 1)) {
        return false;
    }

    if (next.min_x > stored->min_x)
        next.min_x = stored->min_x;
    if (next.min_y > stored->min_y)
        next.min_y = stored->min_y;
    if (next.min_z > stored->min_z)
        next.min_z = stored->min_z;
    if (next.max_x < stored->max_x)
        next.max_x = stored->max_x;
    if (next.max_y < stored->max_y)
        next.max_y = stored->max_y;
    if (next.max_z < stored->max_z)
        next.max_z = stored->max_z;
    if (mc_java_i32_sub(volume(&next), volume(stored)) > 2) {
        return false;
    }
    *stored = next;
    return true;
}

bool mc_light_queue_init(mc_light_queue *queue, mc_light_region *storage, size_t capacity) {
    if (queue == NULL || capacity > MC_LIGHT_QUEUE_LIMIT || (capacity > 0 && storage == NULL)) {
        return false;
    }
    *queue = (mc_light_queue){storage, capacity, 0};
    return true;
}

mc_light_queue_result mc_light_queue_enqueue(mc_light_queue *queue,
                                             const mc_light_region *requested, bool coalesce) {
    if (queue == NULL || requested == NULL || queue->capacity > MC_LIGHT_QUEUE_LIMIT ||
        queue->count > queue->capacity || (queue->capacity > 0 && queue->entries == NULL) ||
        (requested->kind != MC_LIGHT_SKY && requested->kind != MC_LIGHT_BLOCK)) {
        return MC_LIGHT_QUEUE_INVALID;
    }
    mc_light_region next = *requested;
    if (coalesce) {
        size_t examined = queue->count < 5u ? queue->count : 5u;
        for (size_t i = 0; i < examined; ++i) {
            mc_light_region *entry = &queue->entries[queue->count - i - 1u];
            if (entry->kind == next.kind && mc_light_bounds_merge(&entry->bounds, &next.bounds)) {
                return MC_LIGHT_QUEUE_MERGED;
            }
        }
    }
    if (queue->count == MC_LIGHT_QUEUE_LIMIT) {
        /* The original appends, then clears. The transient extra entry is
         * unobservable through the live queue, so no extra slot is needed. */
        queue->count = 0;
        return MC_LIGHT_QUEUE_CLEARED;
    }
    if (queue->count == queue->capacity) {
        return MC_LIGHT_QUEUE_FULL;
    }
    queue->entries[queue->count++] = next;
    return MC_LIGHT_QUEUE_APPENDED;
}
