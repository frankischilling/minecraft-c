#include "mc/world/schedule/entry.h"

#include "mc/core/java/java.h"

#include <stddef.h>
#include <stdlib.h>

/* Original record fields and native reference ownership are independent. */
struct mc_scheduled_tick {
    mc_scheduled_tick_fields fields;
    size_t holds;
};

mc_scheduled_tick_status mc_scheduled_tick_create(mc_scheduled_tick_sequence *sequence, int32_t x,
                                                  int32_t y, int32_t z, int32_t block,
                                                  mc_scheduled_tick **out) {
    if (sequence == NULL || out == NULL) {
        return MC_SCHEDULED_TICK_INVALID_ARGUMENT;
    }
    /* Java allocation supplies zero defaults, including due time, before the
     * constructor's global-sequence operation. No fallible step follows it. */
    mc_scheduled_tick *tick = calloc(1, sizeof(*tick));
    if (tick == NULL) {
        return MC_SCHEDULED_TICK_ALLOCATION_FAILED;
    }
    tick->holds = 1;
    const int64_t old_sequence = sequence->next;
    sequence->next = mc_java_i64_add(old_sequence, 1);
    tick->fields.sequence = old_sequence;
    tick->fields.x = x;
    tick->fields.y = y;
    tick->fields.z = z;
    tick->fields.block = block;
    *out = tick;
    return MC_SCHEDULED_TICK_OK;
}

mc_scheduled_tick_status mc_scheduled_tick_retain(mc_scheduled_tick *tick) {
    if (tick == NULL) {
        return MC_SCHEDULED_TICK_INVALID_ARGUMENT;
    }
    if (tick->holds == SIZE_MAX) {
        return MC_SCHEDULED_TICK_LIMIT;
    }
    tick->holds += 1;
    return MC_SCHEDULED_TICK_OK;
}

void mc_scheduled_tick_release(mc_scheduled_tick *tick) {
    if (tick == NULL) {
        return;
    }
    tick->holds -= 1;
    if (tick->holds == 0) {
        free(tick);
    }
}

mc_scheduled_tick_status mc_scheduled_tick_fields_get(const mc_scheduled_tick *tick,
                                                      mc_scheduled_tick_fields *out) {
    if (tick == NULL || out == NULL) {
        return MC_SCHEDULED_TICK_INVALID_ARGUMENT;
    }
    *out = tick->fields;
    return MC_SCHEDULED_TICK_OK;
}

mc_scheduled_tick_status mc_scheduled_tick_time_set(mc_scheduled_tick *tick, int64_t time) {
    if (tick == NULL) {
        return MC_SCHEDULED_TICK_INVALID_ARGUMENT;
    }
    tick->fields.due_time = time;
    return MC_SCHEDULED_TICK_OK;
}

mc_scheduled_tick_status mc_scheduled_tick_time_shift(mc_scheduled_tick *tick, int64_t delta) {
    if (tick == NULL) {
        return MC_SCHEDULED_TICK_INVALID_ARGUMENT;
    }
    tick->fields.due_time = mc_java_i64_add(tick->fields.due_time, delta);
    return MC_SCHEDULED_TICK_OK;
}

mc_scheduled_tick_status mc_scheduled_tick_equals(const mc_scheduled_tick *tick,
                                                  const mc_scheduled_tick *other, bool *out) {
    if (tick == NULL || out == NULL) {
        return MC_SCHEDULED_TICK_INVALID_ARGUMENT;
    }
    if (other == NULL) {
        *out = false;
        return MC_SCHEDULED_TICK_OK;
    }
    *out = tick->fields.x == other->fields.x && tick->fields.y == other->fields.y &&
           tick->fields.z == other->fields.z && tick->fields.block == other->fields.block;
    return MC_SCHEDULED_TICK_OK;
}

mc_scheduled_tick_status mc_scheduled_tick_hash(const mc_scheduled_tick *tick, int32_t *out) {
    if (tick == NULL || out == NULL) {
        return MC_SCHEDULED_TICK_INVALID_ARGUMENT;
    }
    const int32_t scaled_x = mc_java_i32_mul(tick->fields.x, 128);
    const int32_t scaled_x_again = mc_java_i32_mul(scaled_x, 1024);
    const int32_t scaled_z = mc_java_i32_mul(tick->fields.z, 128);
    const int32_t combined_x_z = mc_java_i32_add(scaled_x_again, scaled_z);
    const int32_t combined_x_y_z = mc_java_i32_add(combined_x_z, tick->fields.y);
    const int32_t scaled_coordinates = mc_java_i32_mul(combined_x_y_z, 256);
    const int32_t hash = mc_java_i32_add(scaled_coordinates, tick->fields.block);
    *out = hash;
    return MC_SCHEDULED_TICK_OK;
}

mc_scheduled_tick_status mc_scheduled_tick_compare(const mc_scheduled_tick *tick,
                                                   const mc_scheduled_tick *other, int32_t *out) {
    if (tick == NULL || other == NULL || out == NULL) {
        return MC_SCHEDULED_TICK_INVALID_ARGUMENT;
    }
    int32_t comparison;
    if (tick->fields.due_time < other->fields.due_time) {
        comparison = -1;
    } else if (tick->fields.due_time > other->fields.due_time) {
        comparison = 1;
    } else if (tick->fields.sequence < other->fields.sequence) {
        comparison = -1;
    } else if (tick->fields.sequence > other->fields.sequence) {
        comparison = 1;
    } else {
        comparison = 0;
    }
    *out = comparison;
    return MC_SCHEDULED_TICK_OK;
}
