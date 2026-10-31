#include "diagnostic_builder.h"

#include <stdlib.h>
#include <string.h>

struct mc_client_diagnostic_builder {
    uint16_t *units;
    size_t count, capacity;
    bool constructed, failed, active;
};

static mc_player_status builder_fail(mc_client_diagnostic_builder_error *e, mc_player_status status,
                                     const char *operation) {
    *e = (mc_client_diagnostic_builder_error){status, operation, true};
    return status;
}

static bool builder_enter(mc_client_diagnostic_builder *b, mc_client_diagnostic_builder_error *e) {
    if (b == NULL || b->active || !b->constructed || b->failed) {
        builder_fail(e, b != NULL && b->active ? MC_PLAYER_BUSY : MC_PLAYER_INVALID_ARGUMENT,
                     "StringBuilder native phase/active admission");
        return false;
    }
    b->active = true;
    return true;
}

static mc_player_status builder_leave(mc_client_diagnostic_builder *b,
                                      mc_client_diagnostic_builder_error *e) {
    b->active = false;
    if (e->status != MC_PLAYER_OK)
        b->failed = true;
    return e->status;
}

static bool reserve(mc_client_diagnostic_builder *b, size_t extra,
                    mc_client_diagnostic_builder_error *e) {
    if (extra > MC_PLAYER_MAX_TEXT_UNITS - b->count) {
        builder_fail(e, MC_PLAYER_LIMIT, "StringBuilder native text budget");
        return false;
    }
    const size_t wanted = b->count + extra;
    if (wanted <= b->capacity)
        return true;
    size_t capacity = b->capacity * 2u + 2u;
    if (capacity < wanted)
        capacity = wanted;
    uint16_t *next = malloc(capacity * sizeof(*next));
    if (next == NULL) {
        builder_fail(e, MC_PLAYER_ALLOCATION_FAILED, "StringBuilder native grow candidate");
        return false;
    }
    if (b->count != 0)
        memcpy(next, b->units, b->count * sizeof(*next));
    free(b->units);
    b->units = next;
    b->capacity = capacity;
    return true;
}

mc_player_status mc_client_diagnostic_builder_allocate(mc_client_diagnostic_builder **out,
                                                       mc_client_diagnostic_builder_error *e) {
    if (e == NULL)
        return MC_PLAYER_INVALID_ARGUMENT;
    *e = (mc_client_diagnostic_builder_error){0};
    if (out == NULL)
        return builder_fail(e, MC_PLAYER_INVALID_ARGUMENT, "StringBuilder native owner slot");
    mc_client_diagnostic_builder *b = calloc(1, sizeof(*b));
    if (b == NULL)
        return builder_fail(e, MC_PLAYER_ALLOCATION_FAILED, "StringBuilder native raw owner");
    *out = b;
    return MC_PLAYER_OK;
}

mc_player_status mc_client_diagnostic_builder_construct(mc_client_diagnostic_builder *b,
                                                        mc_client_diagnostic_builder_error *e) {
    if (e == NULL)
        return MC_PLAYER_INVALID_ARGUMENT;
    *e = (mc_client_diagnostic_builder_error){0};
    if (b == NULL || b->constructed || b->failed || b->active)
        return builder_fail(e, MC_PLAYER_INVALID_ARGUMENT, "StringBuilder constructor phase");
    b->active = true;
    b->units = malloc(16u * sizeof(*b->units));
    if (b->units == NULL)
        builder_fail(e, MC_PLAYER_ALLOCATION_FAILED, "StringBuilder capacity16 candidate");
    else {
        b->capacity = 16;
        b->constructed = true;
    }
    return builder_leave(b, e);
}

mc_player_status mc_client_diagnostic_builder_append(mc_client_diagnostic_builder *b,
                                                     const mc_player_string_view *view,
                                                     mc_client_diagnostic_builder_error *e) {
    static const uint16_t null_units[] = {'n', 'u', 'l', 'l'};
    if (e == NULL)
        return MC_PLAYER_INVALID_ARGUMENT;
    *e = (mc_client_diagnostic_builder_error){0};
    if (!builder_enter(b, e))
        return e->status;
    const size_t length = view == NULL ? sizeof(null_units) / sizeof(null_units[0]) : view->length;
    const uint16_t *units = view == NULL ? null_units : view->units;
    if (length != 0 && units == NULL)
        builder_fail(e, MC_PLAYER_INVALID_ARGUMENT, "StringBuilder counted String");
    else if (reserve(b, length, e)) {
        if (length != 0)
            memcpy(b->units + b->count, units, length * sizeof(*units));
        b->count += length;
    }
    return builder_leave(b, e);
}

mc_player_status mc_client_diagnostic_builder_append_int(mc_client_diagnostic_builder *b,
                                                         int32_t value,
                                                         mc_client_diagnostic_builder_error *e) {
    if (e == NULL)
        return MC_PLAYER_INVALID_ARGUMENT;
    *e = (mc_client_diagnostic_builder_error){0};
    if (!builder_enter(b, e))
        return e->status;
    uint16_t digits[11];
    size_t length = 0;
    /* Negative-domain division admits INT32_MIN without negating it. This is
     * the Integer.getChars value operation, not an intermediate String. */
    int32_t remaining = value > 0 ? -value : value;
    do {
        digits[length++] = (uint16_t)('0' - remaining % 10);
        remaining /= 10;
    } while (remaining != 0);
    if (value < 0)
        digits[length++] = '-';
    if (reserve(b, length, e)) {
        for (size_t i = 0; i < length; ++i)
            b->units[b->count + i] = digits[length - i - 1u];
        b->count += length;
    }
    return builder_leave(b, e);
}

mc_player_status mc_client_diagnostic_builder_string(mc_client_diagnostic_builder *b,
                                                     mc_player_string **out,
                                                     mc_client_diagnostic_builder_error *e) {
    if (e == NULL)
        return MC_PLAYER_INVALID_ARGUMENT;
    *e = (mc_client_diagnostic_builder_error){0};
    if (out == NULL)
        return builder_fail(e, MC_PLAYER_INVALID_ARGUMENT, "StringBuilder toString slot");
    if (!builder_enter(b, e))
        return e->status;
    if (b->count == 0)
        *out = (mc_player_string *)mc_player_empty_literal();
    else {
        const mc_player_status status = mc_player_string_create(b->units, b->count, out);
        if (status != MC_PLAYER_OK)
            builder_fail(e, status, "StringBuilder immutable toString copy");
    }
    return builder_leave(b, e);
}

bool mc_client_diagnostic_builder_inspect(const mc_client_diagnostic_builder *b,
                                          mc_client_diagnostic_builder_state *out) {
    if (b == NULL || out == NULL)
        return false;
    *out = (mc_client_diagnostic_builder_state){b->count, b->capacity, b->constructed, b->failed,
                                                b->active};
    return true;
}

mc_player_status mc_client_diagnostic_builder_retire(mc_client_diagnostic_builder **slot,
                                                     mc_client_diagnostic_builder_error *e) {
    if (e == NULL)
        return MC_PLAYER_INVALID_ARGUMENT;
    *e = (mc_client_diagnostic_builder_error){0};
    if (slot == NULL)
        return builder_fail(e, MC_PLAYER_INVALID_ARGUMENT, "StringBuilder retirement slot");
    mc_client_diagnostic_builder *b = *slot;
    if (b == NULL)
        return MC_PLAYER_OK;
    if (b->active)
        return builder_fail(e, MC_PLAYER_BUSY, "active StringBuilder native retirement");
    free(b->units);
    free(b);
    *slot = NULL;
    return MC_PLAYER_OK;
}
