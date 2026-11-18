#include "mc/world/lighting/queue.h"

static uint32_t volume(const mc_light_bounds *bounds) {
    const uint32_t x = (uint32_t)bounds->max_x - (uint32_t)bounds->min_x;
    const uint32_t y = (uint32_t)bounds->max_y - (uint32_t)bounds->min_y;
    const uint32_t z = (uint32_t)bounds->max_z - (uint32_t)bounds->min_z;
    return x * y * z;
}

static int32_t lower_bound(int32_t bound) {
    return bound == INT32_MIN ? INT32_MAX : bound - 1;
}

static int32_t upper_bound(int32_t bound) {
    return bound == INT32_MAX ? INT32_MIN : bound + 1;
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
    if (next.min_x < lower_bound(stored->min_x) || next.min_y < lower_bound(stored->min_y) ||
        next.min_z < lower_bound(stored->min_z) || next.max_x > upper_bound(stored->max_x) ||
        next.max_y > upper_bound(stored->max_y) || next.max_z > upper_bound(stored->max_z)) {
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
    const uint32_t growth = volume(&next) - volume(stored);
    /* A signed Java int with its high bit set is negative, and passes this check. */
    if (growth > 2u && growth <= INT32_MAX) {
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
