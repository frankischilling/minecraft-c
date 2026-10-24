#include "mc/block/registry/runtime.h"
#include "mc/entity/movement/movement.h"

#include "../../../src/block/registry/registry_internal.h"
#include "../../../src/entity/bounds/bounds_internal.h"
#include "../../../src/entity/entity_internal.h"

#include <inttypes.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xmmintrin.h>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

/* Retained-observation transport, not an alternate movement engine. Each input
 * constructs a fresh actual Item/Falling owner with NULL World. Private scalar
 * and typed Byte0 inputs match the original reflection scope. Every measured
 * operation uses production; no expected result or pool coordinate is input.
 * Server gap rows execute normally to preserve the genuine class-pool history. */
enum {
    MP_CLIENT_ROWS = 59654,
    MP_SERVER_ROWS = 35748,
    MP_SERVER_SELECTED = 24109,
    MP_KIND_BOUNDARY = 29827,
    MP_INPUT = 1024,
    MP_SNAPSHOT = 512,
    MP_POOL = 1024,
    MP_CALLS = 8192,
    MP_SEQUENCE = 6
};

typedef struct mp_text {
    char *data;
    size_t length, capacity;
    bool valid;
} mp_text;

typedef enum mp_operation {
    MP_MOVE,
    MP_FLAG,
    MP_SNEAK,
    MP_FALL_STATE,
    MP_FALL,
    MP_DAMAGE,
    MP_FIRE,
    MP_WET,
    MP_ATTACKED,
    MP_DEAD,
    MP_PORTAL,
    MP_WALK,
    MP_CONTACT,
    MP_DAMAGE_SEQUENCE,
    MP_FIRE_SEQUENCE,
    MP_OPERATION_COUNT
} mp_operation;

static const char *const mp_operations[] = {
    "move",     "flag", "sneak",  "fall-state",   "fall",    "damage",     "fire",    "wet",
    "attacked", "dead", "portal", "walk-enabled", "contact", "damage-seq", "fire-seq"};
static const char *const mp_pool_modes[] = {"reset", "used", "alias"};

_Static_assert(sizeof(float) == sizeof(uint32_t) && sizeof(double) == sizeof(uint64_t),
               "movement probe requires binary32/binary64 storage");
_Static_assert(sizeof(mp_operations) / sizeof(mp_operations[0]) == MP_OPERATION_COUNT,
               "movement protocol operation table");

typedef struct mp_case {
    char line[MP_INPUT], name[8];
    size_t ordinal;
    bool item, flags[6], argument;
    mp_operation operation;
    int8_t watcher;
    uint64_t bounds[6], position[3], velocity[3], delta[3];
    uint32_t singles[5];
    int32_t integer, contact, sequence[MP_SEQUENCE];
    size_t repeat, pool_mode;
} mp_case;

typedef struct mp_stream {
    const uint8_t *bytes;
    size_t length, offset;
    bool closed;
} mp_stream;

typedef struct mp_resources {
    mp_stream streams[3];
    mc_registry_resources api;
    size_t opens, closes;
    bool bad;
} mp_resources;

typedef struct mp_probe {
    bool client, selected[MP_CLIENT_ROWS];
    size_t executed, emitted, prefixes, state_prefixes, entropy_rows;
    mc_materials *materials;
    mc_java_random_profile *profile;
    mc_registry_classes *registry;
    mc_entity_classes *classes;
    mc_entity_bounds_pool *pool;
    mc_block *const *blocks;
    mp_resources resources;
    FILE *entropy;
    bool row_cleanup_ok;
} mp_probe;

typedef struct mp_owners {
    mc_entity_bounds *bounds;
    mc_entity_watcher *watcher;
    mc_entity_watch_map *map;
    mc_entity_watch_node *node;
    mc_entity_watch_record *record;
    mc_java_random *random;
    const mc_entity_class_literal *literal;
    uint16_t base_pc, item_pc, falling_pc;
} mp_owners;

typedef struct mp_result {
    bool failed;
    const char *returned;
    mc_entity_move_error move;
    mc_entity_effect_error effect;
    mc_world_entity_contact_error contact;
} mp_result;

static void mp_init(mp_text *text, char *storage, size_t capacity) {
    *text = (mp_text){storage, 0, capacity, capacity != 0};
    if (capacity != 0)
        storage[0] = '\0';
}

static bool mp_append(mp_text *text, const char *format, ...) {
    if (!text->valid)
        return false;
    va_list arguments;
    va_start(arguments, format);
    const int written =
        vsnprintf(text->data + text->length, text->capacity - text->length, format, arguments);
    va_end(arguments);
    if (written < 0 || (size_t)written >= text->capacity - text->length) {
        text->valid = false;
        return false;
    }
    text->length += (size_t)written;
    return true;
}

/* Inspect storage bits directly; no float promotion, arithmetic or NaN codec. */
static uint64_t mp_raw64(const double *value) {
    uint64_t bits;
    memcpy(&bits, value, sizeof(bits));
    return bits;
}

static uint32_t mp_raw32(const float *value) {
    uint32_t bits;
    memcpy(&bits, value, sizeof(bits));
    return bits;
}

static bool mp_hex(const char *text, size_t width, uint64_t *out) {
    uint64_t value = 0;
    for (size_t i = 0; i < width; ++i) {
        const unsigned char c = (unsigned char)text[i];
        const unsigned int digit = c >= '0' && c <= '9'   ? (unsigned int)(c - '0')
                                   : c >= 'a' && c <= 'f' ? (unsigned int)(c - 'a') + 10u
                                                          : 16u;
        if (digit > 15u)
            return false;
        value = (value << 4u) | digit;
    }
    *out = value;
    return true;
}

static bool mp_raw_group(const char *text, size_t count, uint64_t *out) {
    if (strlen(text) != count * 17u - 1u)
        return false;
    for (size_t i = 0; i < count; ++i)
        if (!mp_hex(text + i * 17u, 16, &out[i]) || (i + 1u < count && text[i * 17u + 16u] != ','))
            return false;
    return true;
}

static bool mp_int(const char *text, int32_t *out) {
    const bool negative = *text == '-';
    const char *digits = text + (negative ? 1u : 0u);
    if (*digits == '\0' || (*digits == '0' && (digits[1] != '\0' || negative)))
        return false;
    const uint32_t limit = negative ? UINT32_C(2147483648) : UINT32_C(2147483647);
    uint32_t value = 0;
    for (const char *p = digits; *p != '\0'; ++p) {
        if (*p < '0' || *p > '9')
            return false;
        const uint32_t digit = (uint32_t)(*p - '0');
        if (value > (limit - digit) / 10u)
            return false;
        value = value * 10u + digit;
    }
    *out = negative ? value == UINT32_C(2147483648) ? INT32_MIN : -(int32_t)value : (int32_t)value;
    return true;
}

static bool mp_id(const char *text, size_t *out) {
    if (strlen(text) != 7u || text[0] != 'e')
        return false;
    size_t value = 0;
    for (size_t i = 1; i < 7u; ++i) {
        if (text[i] < '0' || text[i] > '9')
            return false;
        value = value * 10u + (size_t)(text[i] - '0');
    }
    *out = value;
    return true;
}

/* Complete uniform LF/CRLF records only; no NUL/control/overlong/trailing row. */
static int mp_line(FILE *source, char *line, size_t capacity, int *ending) {
    size_t length = 0;
    int c;
    while ((c = fgetc(source)) != EOF) {
        if (c == '\n') {
            const int current = length != 0 && line[length - 1u] == '\r' ? 2 : 1;
            if (current == 2)
                --length;
            if (length == 0 || (*ending != 0 && *ending != current))
                return -1;
            *ending = current;
            line[length] = '\0';
            return 1;
        }
        if (length + 1u >= capacity || c > 126 || (c < 32 && c != '\t' && c != '\r') ||
            (length != 0 && line[length - 1u] == '\r'))
            return -1;
        line[length++] = (char)c;
    }
    return length == 0 && !ferror(source) ? 0 : -1;
}

static FILE *mp_open(const char *path, const char *mode) {
#ifdef _WIN32
    FILE *file = NULL;
    return fopen_s(&file, path, mode) == 0 ? file : NULL;
#else
    return fopen(path, mode);
#endif
}

static bool mp_expected_selection(bool client, size_t ordinal) {
    return client || ordinal <= 24072u || (ordinal >= 25412u && ordinal <= 25441u) ||
           (ordinal >= 35742u && ordinal <= 35747u);
}

static bool mp_selection(mp_probe *probe, const char *path) {
    FILE *source = mp_open(path, "rb");
    if (source == NULL)
        return false;
    char line[16];
    int ending = 0, read = 0;
    size_t count = 0, previous = 0;
    bool valid = true;
    const size_t executed = probe->client ? MP_CLIENT_ROWS : MP_SERVER_ROWS;
    while ((read = mp_line(source, line, sizeof(line), &ending)) == 1) {
        size_t ordinal;
        if (!mp_id(line, &ordinal) || ordinal >= executed || (count != 0 && ordinal <= previous) ||
            !mp_expected_selection(probe->client, ordinal)) {
            valid = false;
            break;
        }
        probe->selected[ordinal] = true;
        previous = ordinal;
        ++count;
    }
    if (read != 0 || count != (probe->client ? MP_CLIENT_ROWS : MP_SERVER_SELECTED))
        valid = false;
    for (size_t i = 0; i < executed; ++i)
        if (probe->selected[i] != mp_expected_selection(probe->client, i))
            valid = false;
    if (fclose(source) != 0)
        valid = false;
    return valid;
}

static bool mp_parse(const char *line, size_t ordinal, mp_case *row) {
    const size_t length = strlen(line);
    if (length >= sizeof(row->line))
        return false;
    *row = (mp_case){0};
    memcpy(row->line, line, length + 1u);
    char work[MP_INPUT];
    memcpy(work, line, length + 1u);
    char *parts[20];
    parts[0] = work;
    for (size_t i = 1; i < 20u; ++i) {
        char *tab = strchr(parts[i - 1u], '\t');
        if (tab == NULL)
            return false;
        *tab = '\0';
        parts[i] = tab + 1;
    }
    if (strchr(parts[19], '\t') != NULL || !mp_id(parts[0], &row->ordinal) ||
        row->ordinal != ordinal || strlen(parts[1]) != 1u ||
        parts[1][0] != (ordinal < MP_KIND_BOUNDARY ? 'I' : 'F'))
        return false;
    memcpy(row->name, parts[0], sizeof(row->name));
    row->item = parts[1][0] == 'I';
    size_t operation = 0;
    while (operation < MP_OPERATION_COUNT && strcmp(parts[2], mp_operations[operation]) != 0)
        ++operation;
    if (operation == MP_OPERATION_COUNT || strlen(parts[3]) != 11u)
        return false;
    row->operation = (mp_operation)operation;
    for (size_t i = 0; i < 6u; ++i) {
        const char bit = parts[3][i * 2u];
        if ((bit != '0' && bit != '1') || (i < 5u && parts[3][i * 2u + 1u] != ','))
            return false;
        row->flags[i] = bit == '1';
    }
    int32_t byte, repeat, bool_value;
    if (!mp_int(parts[4], &byte) || byte < INT8_MIN || byte > INT8_MAX ||
        !mp_raw_group(parts[5], 6, row->bounds) || !mp_raw_group(parts[6], 3, row->position) ||
        !mp_raw_group(parts[7], 3, row->velocity) || !mp_raw_group(parts[12], 3, row->delta) ||
        !mp_int(parts[13], &row->integer) || !mp_int(parts[14], &bool_value) ||
        (bool_value != 0 && bool_value != 1) || !mp_int(parts[17], &repeat) || repeat < 1 ||
        repeat > MP_SEQUENCE || !mp_int(parts[19], &row->contact))
        return false;
    row->watcher = (int8_t)byte;
    row->argument = bool_value != 0;
    row->repeat = (size_t)repeat;
    for (size_t i = 0; i < 5u; ++i) {
        const char *single = parts[i == 4u ? 15u : 8u + i];
        uint64_t bits;
        if (strlen(single) != 8u || !mp_hex(single, 8, &bits))
            return false;
        row->singles[i] = (uint32_t)bits;
    }
    for (; row->pool_mode < 3u; ++row->pool_mode)
        if (strcmp(parts[16], mp_pool_modes[row->pool_mode]) == 0)
            break;
    if (row->pool_mode == 3u)
        return false;
    const bool sequence =
        row->operation == MP_DAMAGE_SEQUENCE || row->operation == MP_FIRE_SEQUENCE;
    if (!sequence) {
        if (row->repeat != 1u || strcmp(parts[18], "-") != 0)
            return false;
    } else {
        char *next = parts[18];
        for (size_t i = 0; i < row->repeat; ++i) {
            char *comma = strchr(next, ',');
            if ((i + 1u < row->repeat) != (comma != NULL))
                return false;
            if (comma != NULL)
                *comma = '\0';
            if (!mp_int(next, &row->sequence[i]))
                return false;
            if (comma != NULL)
                next = comma + 1;
        }
    }
    if (row->operation == MP_CONTACT)
        return row->contact == 1 || row->contact == 28 || row->contact == 30 ||
               row->contact == 70 || row->contact == 72 || row->contact == 81 ||
               row->contact == 88 || row->contact == 90;
    return row->contact == 0;
}

static mc_java_properties_status mp_read_resource(void *context, uint8_t *out, size_t capacity,
                                                  size_t *count) {
    mp_stream *stream = context;
    if (stream == NULL || out == NULL || capacity == 0 || count == NULL || stream->closed ||
        stream->offset > stream->length)
        return MC_JAVA_PROPERTIES_IO_ERROR;
    size_t take = stream->length - stream->offset;
    if (take > 17u)
        take = 17u;
    if (take > capacity)
        take = capacity;
    if (take != 0)
        memcpy(out, stream->bytes + stream->offset, take);
    stream->offset += take;
    *count = take;
    return MC_JAVA_PROPERTIES_OK;
}

static mc_registry_status mp_open_resource(void *context, const char *name,
                                           mc_java_properties_input *out) {
    mp_resources *resources = context;
    const char *const names[] = {"/lang/en_US.lang", "/lang/stats_US.lang", "/achievement/map.txt"};
    if (name == NULL || out == NULL || resources->opens >= 3u ||
        strcmp(name, names[resources->opens]) != 0) {
        resources->bad = true;
        return MC_REGISTRY_INVALID_ARGUMENT;
    }
    *out = (mc_java_properties_input){&resources->streams[resources->opens++], mp_read_resource};
    return MC_REGISTRY_OK;
}

static mc_registry_status mp_close_resource(void *context, const mc_java_properties_input *input) {
    mp_resources *resources = context;
    if (input == NULL || input->context != &resources->streams[2] ||
        input->read != mp_read_resource) {
        resources->bad = true;
        return MC_REGISTRY_INVALID_ARGUMENT;
    }
    resources->streams[2].closed = true;
    ++resources->closes;
    return MC_REGISTRY_OK;
}

static bool mp_ready_resource(void *context, const mc_java_properties_input *input) {
    mp_resources *resources = context;
    if (input == NULL || input->read != mp_read_resource)
        resources->bad = true;
    return false;
}

static bool mp_pool_valid(const mp_probe *probe, const mc_entity_bounds *permanent) {
    const mc_entity_bounds_pool *pool = probe->classes->bounds_pool;
    /* This family reaches at most anchor + initial copy + first query argument.
     * An unexpected extent is an invariant failure, never a truncated result. */
    if (pool == NULL || pool != probe->pool || pool->length > 3u || pool->length > pool->capacity ||
        (pool->capacity != 0 && pool->elements == NULL) || probe->classes->bounds_cursor < 0 ||
        (size_t)probe->classes->bounds_cursor > pool->length)
        return false;
    for (size_t i = 0; i < pool->length; ++i) {
        const mc_entity_bounds *value = pool->elements[i];
        if (value == NULL || value == permanent)
            return false;
        const mc_entity_bounds_object *object = probe->classes->bounds_objects;
        while (object != NULL && &object->value != value)
            object = object->next;
        if (object == NULL || !object->pooled)
            return false;
        for (size_t j = 0; j < i; ++j)
            if (pool->elements[j] == value)
                return false;
    }
    size_t objects = 0, holds = 0;
    for (const mc_entity_bounds_object *object = probe->classes->bounds_objects; object != NULL;
         object = object->next) {
        if (++objects > 3u || !object->pooled || object->holds > SIZE_MAX - holds)
            return false;
        bool listed = false;
        for (size_t i = 0; i < pool->length; ++i)
            if (&object->value == pool->elements[i])
                listed = true;
        if (!listed)
            return false;
        holds += object->holds;
    }
    return objects == pool->length && holds == probe->classes->bounds_holds;
}

static bool mp_owner_valid(const mp_probe *probe, const mc_entity *entity, const mp_owners *owners,
                           bool item) {
    mc_entity_snapshot view;
    mc_entity_watcher_snapshot watcher;
    const mc_entity_class_literal *literal = NULL;
    if (entity->classes != probe->classes || entity->holds != 1u ||
        entity->phase != MC_ENTITY_READY || entity->bounds != owners->bounds ||
        entity->watcher != owners->watcher || entity->random != owners->random ||
        owners->bounds == NULL || owners->watcher == NULL || owners->random == NULL ||
        entity->watcher->map != owners->map || owners->map == NULL || owners->map->size != 1u ||
        owners->map->head != owners->node || owners->node == NULL || owners->node->key != 0 ||
        owners->node->next != NULL || owners->node->record != owners->record ||
        owners->record == NULL || owners->record->index != 0 || owners->record->type != 0 ||
        entity->base.world != NULL || entity->base.aG != NULL || entity->base.aH != NULL ||
        entity->stack != NULL || entity->storage_source != NULL ||
        entity->base_bytecode != owners->base_pc || entity->item_bytecode != owners->item_pc ||
        entity->falling_bytecode != owners->falling_pc ||
        mc_entity_class_literal_get(
            probe->classes, item ? MC_ENTITY_CLASS_KIND_ITEM : MC_ENTITY_CLASS_KIND_FALLING_SAND,
            &literal) != MC_ENTITY_OK ||
        literal != owners->literal || entity->class_literal != literal ||
        mc_entity_get_snapshot(entity, &view) != MC_ENTITY_OK || view.bounds != owners->bounds ||
        view.random != owners->random || view.watcher != owners->watcher ||
        mc_entity_watcher_get_snapshot(entity->watcher, &watcher) != MC_ENTITY_OK ||
        watcher.record != owners->record || watcher.size != 1u || watcher.index != 0 ||
        watcher.type != 0 || watcher.byte_value != owners->record->byte_value ||
        watcher.record_dirty != owners->record->dirty || watcher.dirty != owners->watcher->dirty)
        return false;
    return mp_pool_valid(probe, owners->bounds);
}

static bool mp_three(mp_text *text, const double *x, const double *y, const double *z) {
    return mp_append(text, "%016" PRIx64 ",%016" PRIx64 ",%016" PRIx64, mp_raw64(x), mp_raw64(y),
                     mp_raw64(z));
}

static bool mp_bounds(mp_text *text, const mc_entity_bounds *bounds) {
    return bounds == NULL ? mp_append(text, "-")
                          : mp_three(text, &bounds->min_x, &bounds->min_y, &bounds->min_z) &&
                                mp_append(text, ",") &&
                                mp_three(text, &bounds->max_x, &bounds->max_y, &bounds->max_z);
}

static bool mp_snapshot(const mc_entity *entity, bool item, mp_text *text) {
    mc_entity_snapshot view;
    if (mc_entity_get_snapshot(entity, &view) != MC_ENTITY_OK || view.phase != MC_ENTITY_READY ||
        view.bounds == NULL)
        return false;
    const mc_entity_base_fields *base = &view.base;
    if (!mp_three(text, &base->x, &base->y, &base->z) || !mp_append(text, "/") ||
        !mp_three(text, &base->velocity_x, &base->velocity_y, &base->velocity_z) ||
        !mp_append(text, "/"))
        return false;
    const float *const singles[] = {&base->offset, &base->y_size, &base->bk,    &base->bp,
                                    &base->bj,     &base->width,  &base->height};
    for (size_t i = 0; i < 7u; ++i)
        if (!mp_append(text, "%s%08" PRIx32, i == 0 ? "" : ",", mp_raw32(singles[i])))
            return false;
    const bool flags[] = {base->bq, base->bc, base->aX, base->bd, base->bC, base->bx,
                          base->aY, base->aZ, base->ba, base->bb, base->be};
    if (!mp_append(text, "/"))
        return false;
    for (size_t i = 0; i < 11u; ++i)
        if (!mp_append(text, "%s%u", i == 0 ? "" : ",", (unsigned int)flags[i]))
            return false;
    if (!mp_append(text, "/%" PRId32 ",%" PRId32 ",%" PRId32 "/", base->bv, base->bu,
                   base->private_b))
        return false;
    return (item ? mp_append(text, "%" PRId32, view.item.f) : mp_append(text, "-")) &&
           mp_append(text, "/") && mp_bounds(text, view.bounds);
}

static bool mp_watcher(const mc_entity *entity, mp_text *text) {
    mc_entity_watcher_snapshot view;
    return mc_entity_watcher_get_snapshot(entity->watcher, &view) == MC_ENTITY_OK &&
           view.size == 1u && view.record != NULL && view.index == 0 && view.type == 0 &&
           mp_append(text, "w0,wm0,wr0,java.lang.Byte,%d,%u,%u", (int)view.byte_value,
                     (unsigned int)view.record_dirty, (unsigned int)view.dirty);
}

static bool mp_pool_values(const mp_probe *probe, mp_text *text) {
    const mc_entity_bounds_pool *pool = probe->classes->bounds_pool;
    if (!mp_pool_valid(probe, NULL))
        return false;
    if (pool->length == 0)
        return mp_append(text, "_");
    for (size_t i = 0; i < pool->length; ++i)
        if (!mp_append(text, "%sp%zu:", i == 0 ? "" : ";", i) ||
            !mp_bounds(text, pool->elements[i]))
            return false;
    return true;
}

static bool mp_reread(const mp_case *row, mc_entity *entity, int8_t byte, const double delta[3],
                      const float *single, mp_text *text) {
    mc_entity_snapshot view;
    if (mc_entity_get_snapshot(entity, &view) != MC_ENTITY_OK)
        return false;
    const mc_entity_base_fields *base = &view.base;
    if (!mp_append(text, "%s|%c|%s|%u,%u,%u,%u,%u,%u|%d|", row->name, row->item ? 'I' : 'F',
                   mp_operations[row->operation], (unsigned int)base->bq, (unsigned int)base->bc,
                   (unsigned int)base->aX, (unsigned int)base->bd, (unsigned int)base->bC,
                   (unsigned int)base->bx, (int)byte) ||
        !mp_bounds(text, view.bounds) || !mp_append(text, "|") ||
        !mp_three(text, &base->x, &base->y, &base->z) || !mp_append(text, "|") ||
        !mp_three(text, &base->velocity_x, &base->velocity_y, &base->velocity_z) ||
        !mp_append(text, "|%08" PRIx32 "|%08" PRIx32 "|%08" PRIx32 "|%08" PRIx32 "|",
                   mp_raw32(&base->offset), mp_raw32(&base->y_size), mp_raw32(&base->bk),
                   mp_raw32(&base->bp)) ||
        !mp_three(text, &delta[0], &delta[1], &delta[2]) ||
        !mp_append(text, "|%" PRId32 "|%u|%08" PRIx32 "|%s|%zu|", row->integer,
                   (unsigned int)row->argument, mp_raw32(single), mp_pool_modes[row->pool_mode],
                   row->repeat))
        return false;
    const bool sequence =
        row->operation == MP_DAMAGE_SEQUENCE || row->operation == MP_FIRE_SEQUENCE;
    if (sequence) {
        for (size_t i = 0; i < row->repeat; ++i)
            if (!mp_append(text, "%s%" PRId32, i == 0 ? "" : ",", row->sequence[i]))
                return false;
    } else if (!mp_append(text, "-"))
        return false;
    if (!mp_append(text, "|%" PRId32, row->contact))
        return false;
    char expected[MP_INPUT];
    memcpy(expected, row->line, strlen(row->line) + 1u);
    for (char *p = expected; *p != '\0'; ++p)
        if (*p == '\t')
            *p = '|';
    return strcmp(text->data, expected) == 0;
}

static const char *mp_optional(const char *text) {
    return text == NULL ? "-" : text;
}

static bool mp_string(const char *text, const char *expected) {
    return text != NULL && strcmp(text, expected) == 0;
}

static bool mp_dispatch(mp_probe *probe, const mp_case *row, mc_entity *entity,
                        const double delta[3], float single, size_t call, mp_result *result) {
    *result = (mp_result){.returned = "-"};
    bool returned = false;
    mc_entity_status status = MC_ENTITY_OK;
    mc_world_storage_status world_status = MC_WORLD_STORAGE_OK;
    const int32_t amount =
        row->operation == MP_DAMAGE_SEQUENCE || row->operation == MP_FIRE_SEQUENCE
            ? row->sequence[call]
            : row->integer;
    switch (row->operation) {
        case MP_MOVE:
            status = mc_entity_move(entity, delta[0], delta[1], delta[2], NULL, &result->move);
            break;
        case MP_FLAG:
            status = mc_entity_effect_flag(entity, row->integer, &returned, &result->effect);
            break;
        case MP_SNEAK:
            status = mc_entity_effect_sneaking(entity, &returned, &result->effect);
            break;
        case MP_FALL_STATE:
            status = mc_entity_effect_fall_state(entity, delta[1], row->argument, &result->effect);
            break;
        case MP_FALL:
            status = mc_entity_effect_fall(entity, single, &result->effect);
            break;
        case MP_DAMAGE:
        case MP_DAMAGE_SEQUENCE:
            status = mc_entity_effect_damage(entity, NULL, amount, &returned, &result->effect);
            break;
        case MP_FIRE:
        case MP_FIRE_SEQUENCE:
            status = mc_entity_effect_fire_damage(entity, amount, &result->effect);
            break;
        case MP_WET:
            status = mc_entity_effect_wet(entity, &returned, &result->effect);
            break;
        case MP_ATTACKED:
            status = mc_entity_effect_attacked(entity, &result->effect);
            break;
        case MP_DEAD:
            status = mc_entity_effect_dead(entity, &result->effect);
            break;
        case MP_PORTAL:
            status = mc_entity_effect_contact_portal(entity, &result->effect);
            break;
        case MP_WALK:
            status = mc_entity_effect_walks(entity, &returned, &result->effect);
            break;
        case MP_CONTACT: {
            const mc_block *block = NULL;
            if (mc_registry_block_get(probe->registry, row->contact, &block, NULL) !=
                    MC_REGISTRY_OK ||
                block == NULL || block != probe->blocks[row->contact] ||
                block->classes != probe->registry || block->fields.bn != row->contact)
                return false;
            world_status =
                mc_world_runtime_entity_contact(NULL, block, 7, -9, 13, entity, &result->contact);
            break;
        }
        default:
            return false;
    }
    result->failed = status != MC_ENTITY_OK || world_status != MC_WORLD_STORAGE_OK;
    const bool expected_failure =
        (row->operation == MP_MOVE && !row->flags[0]) ||
        (row->operation == MP_WET && !row->flags[5]) ||
        (row->operation == MP_CONTACT &&
         (row->contact == 28 || row->contact == 70 || row->contact == 72));
    if (result->failed != expected_failure)
        return false;
    if (!result->failed) {
        if (row->operation == MP_FLAG || row->operation == MP_SNEAK ||
            row->operation == MP_DAMAGE || row->operation == MP_DAMAGE_SEQUENCE ||
            row->operation == MP_WET || row->operation == MP_WALK)
            result->returned = returned ? "1" : "0";
        return status == MC_ENTITY_OK && world_status == MC_WORLD_STORAGE_OK;
    }
    if (row->operation == MP_MOVE) {
        const mc_entity_move_error *error = &result->move;
        const mc_world_query_error *child = &error->collision.nearby.query;
        return status == MC_ENTITY_WORLD_FAILED && error->status == status &&
               (error->bytecode == 232u || error->bytecode == 313u || error->bytecode == 394u) &&
               error->loop_work == (error->bytecode == 394u ? 0u : 1u) &&
               error->max_loop_work == MC_ENTITY_MOVE_DEFAULT_LOOP_WORK &&
               child->status == MC_WORLD_STORAGE_INVALID_ARGUMENT &&
               child->cause.status == child->status && child->bytecode == 0 &&
               child->provider_bytecode == 0 && error->collision.world_bytecode == 0 &&
               mp_string(error->operation, "Entity movement collision query") &&
               mp_string(child->operation, "Collision arguments");
    }
    if (row->operation == MP_WET) {
        const mc_entity_effect_error *error = &result->effect;
        return status == MC_ENTITY_WORLD_FAILED && error->status == status &&
               error->cause == MC_ENTITY_EFFECT_CAUSE_NULL_REFERENCE &&
               error->source_bytecode == 32u && error->entity.status == status &&
               error->entity.bytecode == 32u &&
               error->world.status == MC_WORLD_STORAGE_INVALID_ARGUMENT &&
               error->world.bytecode == 0 && error->world.provider_bytecode == 0 &&
               mp_string(error->operation, "Entity wet World rain-at call") &&
               mp_string(error->world.operation, "World movement arguments");
    }
    const mc_world_entity_contact_error *error = &result->contact;
    const char *block_class = row->contact == 28 ? probe->client ? "ph" : "jn"
                              : probe->client    ? "bv"
                                                 : "bc";
    return world_status == MC_WORLD_STORAGE_UNSUPPORTED_STATE && error->status == world_status &&
           error->bytecode == 1u && error->helper_bytecode == 0 &&
           error->cause.status == world_status &&
           error->metadata.nearby.query.status == MC_WORLD_STORAGE_OK &&
           mp_string(error->block_class, block_class) &&
           mp_string(error->operation, "Block contact World B receiver") &&
           mp_string(error->cause.operation, "Block contact World B receiver");
}

/* Fixed sixteen-field native diagnostics. Java exception names/traces are never
 * synthesized; the controller joins genuine original traces independently. */
static bool mp_prefix(const mp_case *row, const mp_result *result) {
    const char *family, *operation, *child_operation, *block = "-";
    int status, cause = 0, child_status, world_status;
    unsigned int pc, child_pc, world_pc, provider_pc;
    size_t work = 0;
    if (row->operation == MP_MOVE) {
        const mc_entity_move_error *error = &result->move;
        const mc_world_query_error *child = &error->collision.nearby.query;
        family = "MOVE";
        status = (int)error->status;
        pc = error->bytecode;
        child_status = (int)child->status;
        child_pc = error->collision.world_bytecode;
        world_status = (int)child->cause.status;
        world_pc = child->bytecode;
        provider_pc = child->provider_bytecode;
        operation = error->operation;
        child_operation = child->operation;
        work = error->loop_work;
    } else if (row->operation == MP_WET) {
        const mc_entity_effect_error *error = &result->effect;
        family = "EFFECT";
        status = (int)error->status;
        pc = error->source_bytecode;
        cause = (int)error->cause;
        child_status = (int)error->entity.status;
        child_pc = error->entity.bytecode;
        world_status = (int)error->world.status;
        world_pc = error->world.bytecode;
        provider_pc = error->world.provider_bytecode;
        operation = error->operation;
        child_operation = error->world.operation;
    } else {
        const mc_world_entity_contact_error *error = &result->contact;
        family = "CONTACT";
        status = (int)error->status;
        pc = error->bytecode;
        child_status = (int)error->cause.status;
        child_pc = error->helper_bytecode;
        world_status = (int)error->metadata.nearby.query.status;
        world_pc = error->metadata.nearby.query.bytecode;
        provider_pc = error->metadata.nearby.query.provider_bytecode;
        block = mp_optional(error->block_class);
        operation = error->operation;
        child_operation = error->cause.operation;
    }
    return fprintf(stderr, "PREFIX\t%s\t%s\t%s\t%d\t%u\t%d\t%d\t%u\t%d\t%u\t%u\t%s\t%s\t%s\t%zu\n",
                   row->name, mp_operations[row->operation], family, status, pc, cause,
                   child_status, child_pc, world_status, world_pc, provider_pc, block,
                   mp_optional(operation), mp_optional(child_operation), work) >= 0;
}

static bool mp_entropy(mp_probe *probe, const mp_case *row, const mc_entity *entity) {
    const uint64_t seed = UINT64_C(0x173e0000) + row->ordinal;
    if (fprintf(probe->entropy, "%s\t%c\t%" PRId32 "\t", row->name, row->item ? 'I' : 'F',
                entity->base.id) < 0 ||
        (row->item ? fprintf(probe->entropy, "%08" PRIx32, mp_raw32(&entity->item.d))
                   : fputs("-", probe->entropy)) < 0 ||
        fprintf(probe->entropy, "\tmc_java_random\t%016" PRIx64 "\n", seed) < 0)
        return false;
    ++probe->entropy_rows;
    return true;
}

static bool mp_replay(mp_probe *probe, const mp_case *row) {
    bool success = false;
    mc_entity *entity = NULL;
    mc_entity_bounds *anchor = NULL;
    mp_result result = {0};
    mp_owners owners = {0};
    char constructor_storage[MP_SNAPSHOT], before_storage[MP_SNAPSHOT], after_storage[MP_SNAPSHOT];
    char reread_storage[MP_INPUT], watcher_before_storage[128], watcher_after_storage[128];
    char pool_before_storage[MP_POOL], pool_after_storage[MP_POOL], calls_storage[MP_CALLS];
    char setup_storage[256], anchor_storage[128];
    mp_text constructor, before, after, reread, watcher_before, watcher_after;
    mp_text pool_before, pool_after, calls, setup, anchor_values;
    mp_init(&constructor, constructor_storage, sizeof(constructor_storage));
    mp_init(&before, before_storage, sizeof(before_storage));
    mp_init(&after, after_storage, sizeof(after_storage));
    mp_init(&reread, reread_storage, sizeof(reread_storage));
    mp_init(&watcher_before, watcher_before_storage, sizeof(watcher_before_storage));
    mp_init(&watcher_after, watcher_after_storage, sizeof(watcher_after_storage));
    mp_init(&pool_before, pool_before_storage, sizeof(pool_before_storage));
    mp_init(&pool_after, pool_after_storage, sizeof(pool_after_storage));
    mp_init(&calls, calls_storage, sizeof(calls_storage));
    mp_init(&setup, setup_storage, sizeof(setup_storage));
    mp_init(&anchor_values, anchor_storage, sizeof(anchor_storage));
#define MP_ROW(condition)                                                                          \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "FAIL: Entity movement row; id=%s; line=%d\n", row->name, __LINE__);   \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)
    MP_ROW(probe->classes->live_entities == 0 && probe->classes->bounds_holds == 0);
    MP_ROW((row->item ? mc_item_entity_allocate(probe->classes, &entity, NULL)
                      : mc_falling_entity_allocate(probe->classes, &entity, NULL)) == MC_ENTITY_OK);
    MP_ROW(entity != NULL);
    MP_ROW((row->item ? mc_item_entity_construct_world(entity, NULL, NULL)
                      : mc_falling_entity_construct_world(entity, NULL, NULL)) == MC_ENTITY_OK);
    MP_ROW(entity->watcher != NULL && entity->watcher->map != NULL &&
           entity->watcher->map->head != NULL);
    owners = (mp_owners){entity->bounds,
                         entity->watcher,
                         entity->watcher->map,
                         entity->watcher->map->head,
                         entity->watcher->map->head->record,
                         entity->random,
                         entity->class_literal,
                         entity->base_bytecode,
                         entity->item_bytecode,
                         entity->falling_bytecode};
    if (probe->pool == NULL)
        probe->pool = probe->classes->bounds_pool;
    MP_ROW(entity->base.id == (int32_t)row->ordinal &&
           probe->classes->next_id == (int32_t)(row->ordinal + 1u));
    MP_ROW(mp_owner_valid(probe, entity, &owners, row->item) && owners.record->byte_value == 0 &&
           owners.record->dirty && !owners.watcher->dirty);
    MP_ROW(mp_snapshot(entity, row->item, &constructor) && mp_entropy(probe, row, entity));
    mc_entity_bounds supplied;
    memcpy(&supplied.min_x, &row->bounds[0], sizeof(double));
    memcpy(&supplied.min_y, &row->bounds[1], sizeof(double));
    memcpy(&supplied.min_z, &row->bounds[2], sizeof(double));
    memcpy(&supplied.max_x, &row->bounds[3], sizeof(double));
    memcpy(&supplied.max_y, &row->bounds[4], sizeof(double));
    memcpy(&supplied.max_z, &row->bounds[5], sizeof(double));
    MP_ROW(mc_entity_bounds_assign(entity->bounds, &supplied) == entity->bounds);
    double *const positions[] = {&entity->base.x, &entity->base.y, &entity->base.z};
    double *const velocities[] = {&entity->base.velocity_x, &entity->base.velocity_y,
                                  &entity->base.velocity_z};
    for (size_t i = 0; i < 3u; ++i) {
        memcpy(positions[i], &row->position[i], sizeof(double));
        memcpy(velocities[i], &row->velocity[i], sizeof(double));
    }
    bool *const flags[] = {&entity->base.bq, &entity->base.bc, &entity->base.aX,
                           &entity->base.bd, &entity->base.bC, &entity->base.bx};
    for (size_t i = 0; i < 6u; ++i)
        *flags[i] = row->flags[i];
    float *const singles[] = {&entity->base.offset, &entity->base.y_size, &entity->base.bk,
                              &entity->base.bp};
    for (size_t i = 0; i < 4u; ++i)
        memcpy(singles[i], &row->singles[i], sizeof(float));
    /* Explicit private source input, not a claimed production watcher setter.
     * Keep the real record/map/owner and source dirty-store behavior intact. */
    if (owners.record->byte_value != row->watcher) {
        owners.record->byte_value = row->watcher;
        owners.record->dirty = true;
        owners.watcher->dirty = true;
    }
    int8_t reread_byte = 0;
    MP_ROW(mc_entity_effect_watcher_byte(entity, 0, &reread_byte, NULL) == MC_ENTITY_OK &&
           reread_byte == row->watcher);
    const uint64_t seed = UINT64_C(0x173e0000) + row->ordinal;
    mc_java_random_reseed(entity->random, (int64_t)seed);
    double delta[3];
    for (size_t i = 0; i < 3u; ++i)
        memcpy(&delta[i], &row->delta[i], sizeof(double));
    float single;
    memcpy(&single, &row->singles[4], sizeof(single));
    MP_ROW(mp_reread(row, entity, reread_byte, delta, &single, &reread));
    MP_ROW(mc_entity_bounds_pool_reset(probe->classes, NULL) == MC_ENTITY_OK);
    MP_ROW(mp_append(&setup,
                     "AABB-assign;private-scalar-fields;private-typed-Byte0;watcher-byte-read;"
                     "Random.setSeed:%016" PRIx64 ";pool-reset",
                     seed));
    if (row->pool_mode != 0) {
        MP_ROW(mc_entity_bounds_pool_get(probe->classes, 101, 102, 103, 104, 105, 106, &anchor,
                                         NULL) == MC_ENTITY_OK);
        MP_ROW(anchor != NULL && mp_append(&setup, ";pooled-anchor"));
        if (row->pool_mode == 2u) {
            MP_ROW(mc_entity_bounds_pool_reset(probe->classes, NULL) == MC_ENTITY_OK);
            MP_ROW(mp_append(&setup, ";pool-reset"));
        }
    }
    MP_ROW(mp_owner_valid(probe, entity, &owners, row->item));
    const int32_t cursor_before = probe->classes->bounds_cursor;
    const size_t size_before = probe->pool->length;
    MP_ROW(mp_snapshot(entity, row->item, &before) && mp_watcher(entity, &watcher_before) &&
           mp_pool_values(probe, &pool_before));
    const mc_java_random seeded_random = *entity->random;
    for (size_t call = 0; call < row->repeat; ++call) {
        MP_ROW(mp_dispatch(probe, row, entity, delta, single, call, &result));
        if (row->repeat != 1u) {
            char step_storage[MP_SNAPSHOT];
            mp_text step;
            mp_init(&step, step_storage, sizeof(step_storage));
            MP_ROW(mp_snapshot(entity, row->item, &step));
            MP_ROW(mp_append(&calls, "%s%zu:%s:%s:", call == 0 ? "" : ";", call,
                             result.failed ? "NATIVE_FAILURE" : "OK", result.returned));
            for (size_t i = 0; i < step.length; ++i)
                MP_ROW(mp_append(&calls, "%02x", (unsigned int)(unsigned char)step.data[i]));
        }
        if (result.failed)
            break;
    }
    MP_ROW(mp_owner_valid(probe, entity, &owners, row->item));
    MP_ROW(entity->random->state == seeded_random.state &&
           entity->random->have_next_next_gaussian == seeded_random.have_next_next_gaussian &&
           mp_raw64(&entity->random->next_next_gaussian) ==
               mp_raw64(&seeded_random.next_next_gaussian));
    MP_ROW(probe->classes->bounds_holds == (anchor == NULL ? 0u : 1u));
    if (anchor != NULL)
        MP_ROW(probe->pool->length != 0 && probe->pool->elements[0] == anchor &&
               mc_entity_bounds_native_holds(probe->classes, anchor) == 1u);
    MP_ROW(mp_snapshot(entity, row->item, &after) && mp_watcher(entity, &watcher_after) &&
           mp_pool_values(probe, &pool_after) && mp_bounds(&anchor_values, anchor));
    if (row->repeat == 1u)
        MP_ROW(mp_append(&calls, "-"));
    if (probe->selected[row->ordinal]) {
        if (result.failed) {
            MP_ROW(mp_prefix(row, &result));
            ++probe->prefixes;
        }
        MP_ROW(fprintf(stdout,
                       "%s\t%c\t%s\t%s\t%s\t%s\t%s\t%s\t%s\te0\te0\t%s\t%s\tg0,stable\t%" PRId32
                       "\t%" PRId32 "\t%zu\t%zu\t%s\t%s\t%s\t%s\t%s\tbox=e0,watcher=w0,random=r0,"
                       "World=null,passenger=null,riding=null\t%s\n",
                       row->name, row->item ? 'I' : 'F', mp_operations[row->operation],
                       result.failed ? "NATIVE_FAILURE" : "OK", result.returned, constructor.data,
                       reread.data, before.data, after.data, watcher_before.data,
                       watcher_after.data, cursor_before, probe->classes->bounds_cursor,
                       size_before, probe->pool->length, pool_before.data, pool_after.data,
                       anchor == NULL ? "-" : "p0", anchor_values.data, calls.data,
                       setup.data) >= 0);
        ++probe->emitted;
    } else if (result.failed)
        ++probe->state_prefixes;
    success = true;
cleanup:
    if (anchor != NULL && mc_entity_bounds_release(probe->classes, anchor) != MC_ENTITY_OK) {
        probe->row_cleanup_ok = false;
        success = false;
    }
    mc_entity_release(entity);
    if (probe->classes->live_entities != 0 || probe->classes->bounds_holds != 0) {
        probe->row_cleanup_ok = false;
        success = false;
    }
    return success;
#undef MP_ROW
}

static bool mp_initialize(mp_probe *probe) {
    static const uint8_t language[] = "stat.mineBlock=Mine %1$s\nstat.useItem=Use %1$s\n"
                                      "stat.breakItem=Break %1$s\nstat.craftItem=Craft %1$s\n";
    if (!mc_materials_create(&probe->materials) ||
        mc_java_random_profile_create(4u * 1024u * 1024u, &probe->profile) !=
            MC_JAVA_RANDOM_PROFILE_OK)
        return false;
    mp_resources *resources = &probe->resources;
    resources->streams[0] = (mp_stream){.bytes = language, .length = sizeof(language) - 1u};
    resources->streams[1] = resources->streams[2] = (mp_stream){.bytes = language};
    resources->api =
        (mc_registry_resources){resources, mp_open_resource, mp_close_resource, mp_ready_resource};
    const mc_registry_options options = {probe->client ? MC_REGISTRY_CLIENT : MC_REGISTRY_SERVER,
                                         probe->profile, probe->materials, 0, &resources->api};
    if (mc_registry_classes_allocate(&options, &probe->registry, NULL) != MC_REGISTRY_OK ||
        mc_entity_classes_create(probe->client ? MC_ENTITY_CLIENT : MC_ENTITY_SERVER,
                                 probe->profile, NULL, &probe->classes) != MC_ENTITY_OK ||
        mc_registry_classes_bind_entities(probe->registry, probe->classes, NULL) !=
            MC_REGISTRY_OK ||
        mc_registry_classes_initialize_blocks(probe->registry, NULL) != MC_REGISTRY_OK ||
        resources->opens != 3u || resources->closes != 1u || resources->bad)
        return false;
    mc_registry_block_view view;
    if (mc_registry_classes_inspect_blocks(probe->registry, &view, NULL) != MC_REGISTRY_OK ||
        view.phase != MC_REGISTRY_READY || view.m == NULL || view.m[0] != NULL)
        return false;
    probe->blocks = view.m;
    const int32_t ids[] = {1, 28, 30, 70, 72, 81, 88, 90};
    const mc_block_kind kinds[] = {MC_BLOCK_STONE,          MC_BLOCK_DETECTOR_RAIL,
                                   MC_BLOCK_COBWEB,         MC_BLOCK_PRESSURE_PLATE,
                                   MC_BLOCK_PRESSURE_PLATE, MC_BLOCK_CACTUS,
                                   MC_BLOCK_SOUL_SAND,      MC_BLOCK_PORTAL};
    for (size_t i = 0; i < sizeof(ids) / sizeof(ids[0]); ++i) {
        const mc_block *block = NULL;
        if (mc_registry_block_get(probe->registry, ids[i], &block, NULL) != MC_REGISTRY_OK ||
            block == NULL || block != probe->blocks[ids[i]] || block->classes != probe->registry ||
            block->fields.bn != ids[i] || block->kind != kinds[i] ||
            block->phase != MC_BLOCK_CONSTRUCTED)
            return false;
    }
    return probe->classes->next_id == 0 && probe->classes->live_entities == 0 &&
           probe->classes->bounds_holds == 0;
}

static bool mp_cleanup(mp_probe *probe) {
    bool valid = probe->row_cleanup_ok;
    if (probe->entropy != NULL) {
        if (fflush(probe->entropy) != 0 || ferror(probe->entropy))
            valid = false;
        if (fclose(probe->entropy) != 0)
            valid = false;
        probe->entropy = NULL;
    }
    if (probe->registry != NULL) {
        if (mc_registry_classes_free(probe->registry, NULL) != MC_REGISTRY_OK)
            valid = false;
        else
            probe->registry = NULL;
    }
    if (probe->registry == NULL && probe->classes != NULL) {
        if (mc_entity_classes_free(probe->classes) != MC_ENTITY_OK)
            valid = false;
        else
            probe->classes = NULL;
    }
    if (probe->registry == NULL && probe->classes == NULL) {
        mc_java_random_profile_free(probe->profile);
        probe->profile = NULL;
        mc_materials_destroy(probe->materials);
        probe->materials = NULL;
    }
    if (probe->resources.bad)
        valid = false;
    return valid;
}

int main(int argc, char **argv) {
    bool success = false;
    mp_probe *probe = NULL;
    if (argc != 5 || strcmp(argv[1], "entity-movement") != 0 ||
        (strcmp(argv[2], "CLIENT") != 0 && strcmp(argv[2], "SERVER") != 0)) {
        fputs(
            "usage: movement_probe entity-movement CLIENT|SERVER selection.ids fresh-entropy.tsv\n",
            stderr);
        return EXIT_FAILURE;
    }
#ifdef _WIN32
    if (_setmode(_fileno(stdin), _O_BINARY) == -1 || _setmode(_fileno(stdout), _O_BINARY) == -1 ||
        _setmode(_fileno(stderr), _O_BINARY) == -1)
        return EXIT_FAILURE;
#endif
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u) {
        fputs("FAIL: Entity movement replay FP admission\n", stderr);
        return EXIT_FAILURE;
    }
    probe = calloc(1, sizeof(*probe));
    if (probe == NULL)
        return EXIT_FAILURE;
    probe->client = strcmp(argv[2], "CLIENT") == 0;
    probe->row_cleanup_ok = true;
    if (!mp_selection(probe, argv[3]))
        goto cleanup;
    /* ISO C exclusive create; an existing entropy file is never truncated. */
    probe->entropy = mp_open(argv[4], "wbx");
    if (probe->entropy == NULL ||
        fputs("ENTITY_CONSTRUCTOR_ENTROPY_NATIVE_V1\n", probe->entropy) < 0 ||
        !mp_initialize(probe) || fprintf(stdout, "ENTITY_MOVEMENT_NATIVE_V1\t%s\n", argv[2]) < 0)
        goto cleanup;
    const size_t expected = probe->client ? MP_CLIENT_ROWS : MP_SERVER_ROWS;
    int ending = 0;
    for (; probe->executed < expected; ++probe->executed) {
        char line[MP_INPUT];
        mp_case row;
        if (mp_line(stdin, line, sizeof(line), &ending) != 1 ||
            !mp_parse(line, probe->executed, &row) || !mp_replay(probe, &row))
            goto cleanup;
    }
    char extra[MP_INPUT];
    if (mp_line(stdin, extra, sizeof(extra), &ending) != 0 ||
        probe->emitted != (probe->client ? MP_CLIENT_ROWS : MP_SERVER_SELECTED) ||
        probe->prefixes != (probe->client ? 10874u : 4741u) ||
        probe->state_prefixes != (probe->client ? 0u : 696u) || probe->entropy_rows != expected ||
        probe->classes->live_entities != 0 || probe->classes->bounds_holds != 0 ||
        probe->classes->next_id != (int32_t)expected || !mp_pool_valid(probe, NULL))
        goto cleanup;
    success = true;
cleanup:
    if (!mp_cleanup(probe))
        success = false;
    if (fflush(stdout) != 0 || ferror(stdout))
        success = false;
    if (success) {
        if (fprintf(
                stderr,
                "PASS: Entity movement replay; side=%s; executed=%zu; selected=%zu; "
                "state_only=%zu; prefixes=%zu; state_only_prefixes=%zu; entropy=%zu; cleanup=OK\n",
                argv[2], probe->executed, probe->emitted, probe->executed - probe->emitted,
                probe->prefixes, probe->state_prefixes, probe->entropy_rows) < 0 ||
            fflush(stderr) != 0)
            success = false;
    } else {
        fprintf(stderr,
                "FAIL: Entity movement replay; side=%s; executed=%zu; selected=%zu; entropy=%zu; "
                "owners_retained=%u\n",
                argv[2], probe->executed, probe->emitted, probe->entropy_rows,
                (unsigned int)(probe->registry != NULL || probe->classes != NULL));
        (void)fflush(stderr);
    }
    /* Resource callbacks borrow storage in this owner. A failed checked free
     * keeps that storage and all surviving dependencies live until process exit. */
    if (probe->registry == NULL && probe->classes == NULL)
        free(probe);
    return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
