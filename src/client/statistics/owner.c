#include "internal.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

typedef union stats_allocation {
    struct {
        stats_memory *memory;
        size_t bytes;
    } data;

    long double alignment;
    void *pointer_alignment;
    uint64_t integer_alignment;
} stats_allocation;

void stats_error_clear(mc_client_stats_error *error) {
    if (error != NULL)
        *error = (mc_client_stats_error){.index = SIZE_MAX, .text_offset = SIZE_MAX};
}

mc_client_stats_status stats_fail(mc_client_stats_error *error, mc_client_stats_status status,
                                  const char *operation, const char *class_name, const char *method,
                                  const char *descriptor, unsigned int pc) {
    if (error != NULL) {
        error->status = status;
        error->operation = operation;
        error->class_name = class_name;
        error->method = method;
        error->descriptor = descriptor;
        error->bytecode = pc;
    }
    return status;
}

mc_client_stats_status stats_exception(mc_client_stats_error *error,
                                       mc_client_stats_exception exception, const char *operation,
                                       const char *class_name, const char *method,
                                       const char *descriptor, unsigned int pc) {
    if (error != NULL)
        error->exception = exception;
    return stats_fail(error, MC_CLIENT_STATS_SOURCE_EXCEPTION, operation, class_name, method,
                      descriptor, pc);
}

mc_client_stats_limits mc_client_stats_default_limits(void) {
    return (mc_client_stats_limits){64u * 1024u * 1024u, 16384u, 4u * 1024u * 1024u, 262144u, 128u};
}

mc_client_stats_options mc_client_stats_default_options(void) {
    return (mc_client_stats_options){.limits = mc_client_stats_default_limits(),
                                     .charset = MC_JAVA_READER_DEFAULT,
                                     .name_case = MC_CLIENT_STATS_NAME_ENGLISH_ASCII};
}

mc_client_stats_options stats_options(const mc_client_stats_options *input) {
    mc_client_stats_options value = input == NULL ? mc_client_stats_default_options() : *input;
    const mc_client_stats_limits defaults = mc_client_stats_default_limits();
    if (value.limits.max_allocation_bytes == 0)
        value.limits.max_allocation_bytes = defaults.max_allocation_bytes;
    if (value.limits.max_map_entries == 0)
        value.limits.max_map_entries = defaults.max_map_entries;
    if (value.limits.max_text_units == 0)
        value.limits.max_text_units = defaults.max_text_units;
    if (value.limits.max_json_nodes == 0)
        value.limits.max_json_nodes = defaults.max_json_nodes;
    if (value.limits.max_json_depth == 0)
        value.limits.max_json_depth = defaults.max_json_depth;
    return value;
}

stats_memory *stats_memory_create(size_t maximum) {
    stats_memory *memory = malloc(sizeof(*memory));
    if (memory == NULL)
        return NULL;
    *memory = (stats_memory){.holds = 1, .maximum = maximum};
    if (mtx_init(&memory->mutex, mtx_plain) != thrd_success) {
        free(memory);
        return NULL;
    }
    return memory;
}

bool stats_memory_retain(stats_memory *memory) {
    mtx_lock(&memory->mutex);
    const bool result = memory->holds != SIZE_MAX;
    if (result)
        ++memory->holds;
    mtx_unlock(&memory->mutex);
    return result;
}

void stats_memory_release(stats_memory *memory) {
    if (memory == NULL)
        return;
    mtx_lock(&memory->mutex);
    const bool last = --memory->holds == 0;
    mtx_unlock(&memory->mutex);
    if (last) {
        if (memory->registry != NULL)
            (void)mc_registry_runtime_borrow_release(memory->registry);
        mtx_destroy(&memory->mutex);
        free(memory);
    }
}

void *stats_alloc(stats_memory *memory, size_t bytes, mc_client_stats_error *error) {
    if (bytes > SIZE_MAX - sizeof(stats_allocation)) {
        stats_fail(error, MC_CLIENT_STATS_LIMIT, "Client stats native allocation extent", NULL,
                   NULL, NULL, 0);
        return NULL;
    }
    const size_t amount = sizeof(stats_allocation) + bytes;
    mtx_lock(&memory->mutex);
    if (amount > memory->maximum || memory->bytes > memory->maximum - amount) {
        mtx_unlock(&memory->mutex);
        stats_fail(error, MC_CLIENT_STATS_LIMIT, "Client stats native allocation budget", NULL,
                   NULL, NULL, 0);
        return NULL;
    }
    stats_allocation *block = malloc(amount);
    if (block == NULL) {
        mtx_unlock(&memory->mutex);
        stats_fail(error, MC_CLIENT_STATS_ALLOCATION_FAILED, "Client stats native allocation", NULL,
                   NULL, NULL, 0);
        return NULL;
    }
    block->data.memory = memory;
    block->data.bytes = amount;
    memory->bytes += amount;
    mtx_unlock(&memory->mutex);
    return block + 1;
}

void stats_free(void *storage) {
    if (storage == NULL)
        return;
    stats_allocation *block = (stats_allocation *)storage - 1;
    stats_memory *memory = block->data.memory;
    mtx_lock(&memory->mutex);
    memory->bytes -= block->data.bytes;
    mtx_unlock(&memory->mutex);
    free(block);
}

size_t stats_memory_bytes(stats_memory *memory) {
    mtx_lock(&memory->mutex);
    const size_t bytes = memory->bytes;
    mtx_unlock(&memory->mutex);
    return bytes;
}

void *stats_arena_allocate(void *context, size_t bytes) {
    stats_arena *arena = context;
    if (bytes > SIZE_MAX - sizeof(stats_arena_block)) {
        arena->failure = MC_CLIENT_STATS_LIMIT;
        return NULL;
    }
    mc_client_stats_error error;
    stats_error_clear(&error);
    stats_arena_block *block = stats_alloc(arena->memory, sizeof(*block) + bytes, &error);
    if (block == NULL) {
        arena->failure = error.status;
        return NULL;
    }
    block->data.next = arena->blocks;
    arena->blocks = block;
    return block + 1;
}

void stats_arena_clear(stats_arena *arena) {
    while (arena->blocks != NULL) {
        stats_arena_block *block = arena->blocks;
        arena->blocks = block->data.next;
        stats_free(block);
    }
}

mc_client_stats_status stats_append(stats_builder *builder, const uint16_t *units, size_t length,
                                    mc_client_stats_error *error) {
    if (length > builder->maximum || builder->length > builder->maximum - length ||
        length > (size_t)INT32_MAX || builder->length > (size_t)INT32_MAX - length)
        return stats_fail(error, MC_CLIENT_STATS_LIMIT, "Client stats text extent", NULL, NULL,
                          NULL, 0);
    const size_t target = builder->length + length;
    if (target > builder->capacity) {
        size_t capacity = builder->capacity == 0 ? 64u : builder->capacity;
        if (capacity > builder->maximum)
            capacity = builder->maximum;
        while (capacity < target) {
            if (capacity > builder->maximum / 2u) {
                capacity = builder->maximum;
                break;
            }
            capacity *= 2u;
        }
        if (capacity > SIZE_MAX / sizeof(uint16_t))
            return stats_fail(error, MC_CLIENT_STATS_LIMIT, "Client stats text allocation extent",
                              NULL, NULL, NULL, 0);
        uint16_t *storage = stats_alloc(builder->memory, capacity * sizeof(*storage), error);
        if (storage == NULL)
            return error->status;
        if (builder->length != 0)
            memcpy(storage, builder->units, builder->length * sizeof(*storage));
        stats_free(builder->units);
        builder->units = storage;
        builder->capacity = capacity;
    }
    if (length != 0)
        memcpy(builder->units + builder->length, units, length * sizeof(*units));
    builder->length = target;
    return MC_CLIENT_STATS_OK;
}

mc_client_stats_status stats_ascii(stats_builder *builder, const char *text,
                                   mc_client_stats_error *error) {
    while (*text != '\0') {
        const uint16_t unit = (unsigned char)*text++;
        const mc_client_stats_status status = stats_append(builder, &unit, 1, error);
        if (status != MC_CLIENT_STATS_OK)
            return status;
    }
    return MC_CLIENT_STATS_OK;
}

mc_client_stats_status stats_string(stats_builder *builder, mc_client_stats_string text,
                                    mc_client_stats_error *error) {
    if (text.is_null)
        return stats_ascii(builder, "null", error);
    return stats_append(builder, text.units, text.length, error);
}

mc_client_stats_status stats_decimal(stats_builder *builder, int32_t value,
                                     mc_client_stats_error *error) {
    char digits[12];
    size_t length = 0;
    uint32_t magnitude = value < 0 ? UINT32_C(0) - (uint32_t)value : (uint32_t)value;
    do {
        digits[length++] = (char)('0' + magnitude % 10u);
        magnitude /= 10u;
    } while (magnitude != 0);
    if (value < 0)
        digits[length++] = '-';
    while (length != 0) {
        const uint16_t unit = (uint16_t)(unsigned char)digits[--length];
        const mc_client_stats_status status = stats_append(builder, &unit, 1, error);
        if (status != MC_CLIENT_STATS_OK)
            return status;
    }
    return MC_CLIENT_STATS_OK;
}

mc_client_stats_status stats_text_finish(stats_builder *builder, mc_client_stats_text **out,
                                         mc_client_stats_error *error) {
    mc_client_stats_text *text = stats_alloc(builder->memory, sizeof(*text), error);
    if (text == NULL)
        return error->status;
    if (!stats_memory_retain(builder->memory)) {
        stats_free(text);
        return stats_fail(error, MC_CLIENT_STATS_LIMIT, "Client stats text memory hold", NULL, NULL,
                          NULL, 0);
    }
    *text = (mc_client_stats_text){builder->memory, builder->units, builder->length};
    builder->units = NULL;
    builder->length = builder->capacity = 0;
    *out = text;
    return MC_CLIENT_STATS_OK;
}

void stats_builder_clear(stats_builder *builder) {
    stats_free(builder->units);
    builder->units = NULL;
    builder->length = builder->capacity = 0;
}

bool mc_client_stats_text_read(const mc_client_stats_text *text, mc_client_stats_string *out) {
    if (text == NULL || out == NULL)
        return false;
    *out = (mc_client_stats_string){text->units, text->length, false};
    return true;
}

void mc_client_stats_text_retire(mc_client_stats_text **owner) {
    if (owner == NULL || *owner == NULL)
        return;
    mc_client_stats_text *text = *owner;
    stats_memory *memory = text->memory;
    stats_free(text->units);
    stats_free(text);
    *owner = NULL;
    stats_memory_release(memory);
}
