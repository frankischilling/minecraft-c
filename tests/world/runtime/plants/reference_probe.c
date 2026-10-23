/* Prepared input regions and genuine GAME owners. The included fluid entry
 * is never executed. This probe emits only the declared plant projection;
 * it does not compare original World services or constructor entropy. */
#define main mc_plant_reference_fluid_fixture_main
#include "../test_fluids.c"
#undef main

#include "../../../../src/world/chunk/object_state_internal.h"
#include "../../../../src/world/runtime/mutation/plants/plants_internal.h"

#include <errno.h>
#include <inttypes.h>
#include <stdarg.h>

enum { REFERENCE_WIDTH = 32, REFERENCE_HEIGHT = 128, REFERENCE_REQUESTS = 64 };

#define REFERENCE_CELLS (32u * 32u * 128u)
#define REFERENCE_JSON_LIMIT (128u * 1024u * 1024u)

typedef struct reference_request {
    char name[129];

    enum { REFERENCE_TICK, REFERENCE_NEIGHBOR, REFERENCE_GROW } action;

    int32_t id, x, y, z, metadata, plant_light, growth_light;
    int64_t seed, world_seed;
    bool remote;
    input_cell *prepared;
} reference_request;

typedef struct reference_json {
    char *bytes;
    size_t length, capacity;
    bool failed;
} reference_json;

static const reference_request *reference_active;

static size_t reference_index(int32_t x, int32_t y, int32_t z) {
    return ((size_t)(uint32_t)x * REFERENCE_WIDTH + (uint32_t)z) * REFERENCE_HEIGHT + (uint32_t)y;
}

static input_cell reference_blocks(int32_t x, int32_t y, int32_t z) {
    return reference_active->prepared[reference_index(x, y, z)];
}

static input_cell reference_lights(int32_t x, int32_t y, int32_t z) {
    (void)x;
    (void)z;
    const int32_t light = y == reference_active->y + 1 ? reference_active->growth_light
                                                       : reference_active->plant_light;
    return (input_cell){0, (uint8_t)light};
}

static bool reference_number(const char *text, int64_t minimum, int64_t maximum, int64_t *out) {
    if (text == NULL || *text == '\0')
        return false;
    const char *digits = text;
    if (*digits == '-' || *digits == '+')
        ++digits;
    if (*digits == '\0')
        return false;
    for (const char *p = digits; *p != '\0'; ++p) {
        if (*p < '0' || *p > '9')
            return false;
    }
    errno = 0;
    char *end;
    const intmax_t value = strtoimax(text, &end, 10);
    if (errno != 0 || *end != '\0' || value < minimum || value > maximum)
        return false;
    *out = (int64_t)value;
    return true;
}

static bool reference_small(const char *text, int32_t minimum, int32_t maximum, int32_t *out) {
    int64_t value;
    if (!reference_number(text, minimum, maximum, &value))
        return false;
    *out = (int32_t)value;
    return true;
}

static bool reference_boolean(const char *text, bool *out) {
    if (strcmp(text, "true") == 0)
        *out = true;
    else if (strcmp(text, "false") == 0)
        *out = false;
    else
        return false;
    return true;
}

static bool reference_split(char *text, char delimiter, char **parts, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        parts[i] = text;
        char *end = strchr(text, delimiter);
        if (i + 1u == count)
            return end == NULL;
        if (end == NULL)
            return false;
        *end = '\0';
        text = end + 1;
    }
    return false;
}

static bool reference_parse(char *row, reference_request *out) {
    char *parts[16];
    if (!reference_split(row, '\t', parts, COUNT(parts)))
        return false;
    const size_t length = strlen(parts[0]);
    if (length == 0 || length >= sizeof(out->name))
        return false;
    for (size_t i = 0; i < length; ++i) {
        if ((unsigned char)parts[0][i] < 32u || (unsigned char)parts[0][i] > 126u)
            return false;
    }
    memcpy(out->name, parts[0], length + 1u);
    if (strcmp(parts[1], "tick") == 0)
        out->action = REFERENCE_TICK;
    else if (strcmp(parts[1], "neighbor") == 0)
        out->action = REFERENCE_NEIGHBOR;
    else if (strcmp(parts[1], "grow") == 0)
        out->action = REFERENCE_GROW;
    else
        return false;
    bool sky;
    int32_t soil, soil_metadata;
    if (!reference_small(parts[2], 1, 255, &out->id) ||
        !reference_small(parts[3], 0, 31, &out->x) || !reference_small(parts[4], 0, 127, &out->y) ||
        !reference_small(parts[5], 0, 31, &out->z) ||
        !reference_small(parts[6], 0, 15, &out->metadata) ||
        !reference_number(parts[7], INT64_MIN, INT64_MAX, &out->seed) ||
        !reference_number(parts[8], INT64_MIN, INT64_MAX, &out->world_seed) ||
        !reference_boolean(parts[9], &out->remote) ||
        !reference_small(parts[10], 0, 15, &out->plant_light) ||
        !reference_small(parts[11], 0, 15, &out->growth_light) ||
        !reference_boolean(parts[12], &sky) || !sky || !reference_small(parts[13], 0, 255, &soil) ||
        !reference_small(parts[14], 0, 15, &soil_metadata))
        return false;
    out->prepared = calloc(REFERENCE_CELLS, sizeof(*out->prepared));
    if (out->prepared == NULL)
        return false;
    if (out->y > 0)
        for (int32_t x = 0; x < REFERENCE_WIDTH; ++x)
            for (int32_t z = 0; z < REFERENCE_WIDTH; ++z)
                out->prepared[reference_index(x, out->y - 1, z)] =
                    (input_cell){(uint8_t)soil, (uint8_t)soil_metadata};
    out->prepared[reference_index(out->x, out->y, out->z)] =
        (input_cell){(uint8_t)out->id, (uint8_t)out->metadata};
    if (strcmp(parts[15], "-") == 0)
        return true;
    char *next = parts[15];
    size_t overrides = 0;
    for (;;) {
        if (++overrides > 4096u)
            return false;
        char *end = strchr(next, ';');
        if (end != NULL)
            *end = '\0';
        char *values[5];
        int32_t x, y, z, id, metadata;
        if (!reference_split(next, ',', values, COUNT(values)) ||
            !reference_small(values[0], 0, 31, &x) || !reference_small(values[1], 0, 127, &y) ||
            !reference_small(values[2], 0, 31, &z) || !reference_small(values[3], 0, 255, &id) ||
            !reference_small(values[4], 0, 15, &metadata))
            return false;
        out->prepared[reference_index(x, y, z)] = (input_cell){(uint8_t)id, (uint8_t)metadata};
        if (end == NULL)
            return true;
        next = end + 1;
    }
}

static bool reference_requests(const char *path, reference_request *rows, size_t *count) {
    mc_ws_file_bytes input = {0};
    char *text = NULL;
    bool ok = false;
    if (mc_ws_read_file(path, 8u * 1024u * 1024u, &input, NULL) != MC_WORLD_STORAGE_OK ||
        input.length == 0 || memchr(input.data, 0, input.length) != NULL)
        goto done;
    text = malloc(input.length + 1u);
    if (text == NULL)
        goto done;
    memcpy(text, input.data, input.length);
    text[input.length] = '\0';
    char *row = text;
    for (;;) {
        char *end = strchr(row, '\n');
        if (end != NULL)
            *end = '\0';
        size_t length = strlen(row);
        if (length != 0 && row[length - 1u] == '\r')
            row[--length] = '\0';
        bool blank = true;
        for (size_t i = 0; i < length; ++i) {
            if (row[i] != ' ' && row[i] != '\t' && row[i] != '\r')
                blank = false;
        }
        if (!blank && row[0] != '#') {
            if (*count == REFERENCE_REQUESTS || !reference_parse(row, &rows[*count]))
                goto done;
            for (size_t i = 0; i < *count; ++i) {
                if (strcmp(rows[i].name, rows[*count].name) == 0)
                    goto done;
            }
            ++*count;
        }
        if (end == NULL)
            break;
        row = end + 1;
    }
    ok = *count != 0;
done:
    free(text);
    mc_ws_free_file_bytes(&input);
    return ok;
}

static bool reference_reserve(reference_json *json, size_t length) {
    if (json->failed || length > REFERENCE_JSON_LIMIT - json->length) {
        json->failed = true;
        return false;
    }
    const size_t needed = json->length + length + 1u;
    if (needed <= json->capacity)
        return true;
    size_t capacity = json->capacity == 0 ? 4096u : json->capacity;
    while (capacity < needed) {
        if (capacity > REFERENCE_JSON_LIMIT / 2u) {
            capacity = REFERENCE_JSON_LIMIT + 1u;
            break;
        }
        capacity *= 2u;
    }
    char *next = realloc(json->bytes, capacity);
    if (next == NULL) {
        json->failed = true;
        return false;
    }
    json->bytes = next;
    json->capacity = capacity;
    return true;
}

static bool reference_printf(reference_json *json, const char *format, ...) {
    if (json->failed)
        return false;
    va_list args, copy;
    va_start(args, format);
    va_copy(copy, args);
    const int n = vsnprintf(NULL, 0, format, copy);
    va_end(copy);
    if (n < 0 || !reference_reserve(json, (size_t)n)) {
        va_end(args);
        json->failed = true;
        return false;
    }
    const int written =
        vsnprintf(json->bytes + json->length, json->capacity - json->length, format, args);
    va_end(args);
    if (written != n) {
        json->failed = true;
        return false;
    }
    json->length += (size_t)n;
    return true;
}

static bool reference_string(reference_json *json, const char *text) {
    if (text == NULL)
        return reference_printf(json, "null");
    if (!reference_printf(json, "\""))
        return false;
    for (const unsigned char *p = (const unsigned char *)text; *p != 0; ++p) {
        if (*p == '\\' || *p == '"') {
            if (!reference_printf(json, "\\%c", (int)*p))
                return false;
        } else if (*p < 32u || *p > 126u) {
            if (!reference_printf(json, "\\u%04x", (unsigned int)*p))
                return false;
        } else if (!reference_printf(json, "%c", (int)*p)) {
            return false;
        }
    }
    return reference_printf(json, "\"");
}

static uint64_t reference_bits(double value) {
    uint64_t bits;
    _Static_assert(sizeof(bits) == sizeof(value), "binary64 projection");
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static bool reference_tail(reference_json *json, const mc_java_random *source) {
    mc_java_random random = *source;
    const uint64_t first = (uint64_t)mc_java_random_next_i64(&random);
    int32_t bounded;
    if (!mc_java_random_next_bounded(&random, 31, &bounded))
        return false;
    const uint64_t second = reference_bits(mc_java_random_next_double(&random));
    const uint64_t gaussian = reference_bits(mc_java_random_next_gaussian(&random));
    return reference_printf(json,
                            "{\"next_long_hex\":\"%016" PRIx64 "\",\"next_int31\":%" PRId32
                            ",\"next_double_bits\":\"%016" PRIx64
                            "\",\"next_gaussian_bits\":\"%016" PRIx64 "\"}",
                            first, bounded, second, gaussian);
}

static bool reference_snapshot(reference_json *json, mc_world_chunk *chunks[2][2]) {
    bool comma = false;
    if (!reference_printf(json, "["))
        return false;
    for (int32_t x = 0; x < REFERENCE_WIDTH; ++x) {
        for (int32_t z = 0; z < REFERENCE_WIDTH; ++z) {
            mc_world_chunk *chunk = chunks[x / 16][z / 16];
            if (chunk == NULL || chunk->data == NULL || chunk->data->blocks == NULL ||
                chunk->data->blocks_capacity < MC_CHUNK_BLOCKS || chunk->e == NULL ||
                chunk->e->a == NULL || chunk->e->length < MC_CHUNK_NIBBLE_BYTES)
                return false;
            for (int32_t y = 0; y < REFERENCE_HEIGHT; ++y) {
                const size_t index = cell(x, y, z);
                const uint8_t id = chunk->data->blocks[index];
                const unsigned int shift = ((uint32_t)y & 1u) * 4u;
                const uint8_t metadata =
                    (uint8_t)(((uint32_t)chunk->e->a[index / 2u] >> shift) & 15u);
                if (id == 0 && metadata == 0)
                    continue;
                if (!reference_printf(json, "%s[%" PRId32 ",%" PRId32 ",%" PRId32 ",%u,%u]",
                                      comma ? "," : "", x, y, z, (unsigned int)id,
                                      (unsigned int)metadata))
                    return false;
                comma = true;
            }
        }
    }
    return reference_printf(json, "]");
}

static bool reference_drops(reference_json *json, mc_world_runtime *world) {
    const mc_world_reference_list *list = world->fields.b;
    if (list == NULL || list->count > list->capacity || list->count > 4096u ||
        (list->count != 0 && list->items == NULL) || !reference_printf(json, "["))
        return false;
    const mc_entity_class_literal *item = NULL;
    if (mc_entity_class_literal_get(world->classes->entities, MC_ENTITY_CLASS_KIND_ITEM, &item) !=
        MC_ENTITY_OK)
        return false;
    for (size_t i = 0; i < list->count; ++i) {
        mc_entity_snapshot entity;
        mc_item_stack_fields stack;
        if (mc_entity_get_snapshot(list->items[i], &entity) != MC_ENTITY_OK ||
            entity.class_literal != item || entity.stack == NULL ||
            mc_item_stack_fields_get(entity.stack, &stack) != MC_ITEM_STACK_OK)
            return false;
        if (!reference_printf(json,
                              "%s{\"stack\":[%" PRId32 ",%" PRId32 ",%" PRId32 ",%" PRId32
                              "],\"pickup_delay\":%" PRId32 ",\"position_bits\":[\"%016" PRIx64
                              "\",\"%016" PRIx64 "\",\"%016" PRIx64 "\"]}",
                              i == 0 ? "" : ",", stack.count, stack.animation, stack.id,
                              stack.damage, entity.item.c, reference_bits(entity.base.x),
                              reference_bits(entity.base.y), reference_bits(entity.base.z)))
            return false;
    }
    return reference_printf(json, "]");
}

static bool reference_initialize(fixture *f, size_t side, const char *root) {
    const uint16_t title[] = {'P', 'l', 'a', 'n', 't'};
    if (mc_world_regions_create(NULL, &f->regions, NULL) != MC_WORLD_STORAGE_OK ||
        !mc_materials_create(&f->materials) ||
        mc_java_random_profile_create(4u * 1024u * 1024u, &f->profile) !=
            MC_JAVA_RANDOM_PROFILE_OK ||
        mc_world_info_name_copy((mc_nbt_string){title, COUNT(title)}, SIZE_MAX, &f->name) !=
            MC_WORLD_INFO_OK)
        return false;
    for (size_t i = 0; i < 2; ++i) {
        resources *r = &f->sources[i];
        r->streams[0] = (stream){.bytes = language, .length = sizeof(language) - 1u};
        r->streams[1] = (stream){.bytes = language};
        r->streams[2] = (stream){.bytes = language};
        r->api = (mc_registry_resources){r, open_resource, close_resource, ready_resource};
        const mc_registry_options options = {i == 0 ? MC_REGISTRY_CLIENT : MC_REGISTRY_SERVER,
                                             f->profile, f->materials, 0, &r->api};
        if (mc_registry_classes_allocate(&options, &f->registry[i], NULL) != MC_REGISTRY_OK ||
            mc_entity_classes_create(i == 0 ? MC_ENTITY_CLIENT : MC_ENTITY_SERVER, f->profile, NULL,
                                     &f->entities[i]) != MC_ENTITY_OK ||
            mc_world_runtime_classes_create(i == 0 ? MC_WORLD_INFO_CLIENT : MC_WORLD_INFO_SERVER,
                                            f->profile, &trig, &f->classes[i],
                                            NULL) != MC_WORLD_STORAGE_OK ||
            mc_world_runtime_classes_bind_registry(f->classes[i], f->registry[i], NULL) !=
                MC_REGISTRY_OK ||
            mc_world_runtime_classes_bind_entities(f->classes[i], f->entities[i]) != MC_ENTITY_OK ||
            mc_registry_classes_initialize_blocks(f->registry[i], NULL) != MC_REGISTRY_OK ||
            r->opens != 3 || r->closes != 1 || r->bad)
            return false;
    }
    if (!open_store(f, side, root) || !create_world_with_scheduled_capacity(f, side, 512))
        return false;
    for (int32_t x = 0; x < 2; ++x)
        for (int32_t z = 0; z < 2; ++z)
            if (!seed_record_from(f, side, x, z, reference_blocks, reference_lights))
                return false;
    return load_seeded_square(f, side);
}

static bool reference_destroy(fixture *f) {
    if (f == NULL)
        return true;
    for (size_t i = 0; i < COUNT(f->worlds); ++i) {
        if (f->worlds[i] == NULL)
            continue;
        if (!remember(f, i) ||
            mc_world_runtime_retire_entity_containers(f->worlds[i], NULL) != MC_WORLD_STORAGE_OK)
            return false;
        if (f->bound[i]) {
            mc_chunk_cache *cache = f->providers[i].cache;
            if (mc_chunk_pending_clear(f->providers[i].pending) != MC_CHUNK_ACQUIRE_OK ||
                !mc_chunk_cache_init(cache, cache->entries, cache->capacity, cache->insertions,
                                     cache->insertion_capacity))
                return false;
        }
        if (mc_world_runtime_retire_world(f->worlds[i], NULL) != MC_WORLD_STORAGE_OK)
            return false;
    }
    for (size_t i = 0; i < COUNT(f->children); ++i) {
        if (f->children[i].state != NULL &&
            mc_world_store_retire_chunk(f->children[i].store, f->children[i].state, NULL) !=
                MC_WORLD_STORAGE_OK)
            return false;
    }
    for (size_t i = 0; i < COUNT(f->worlds); ++i) {
        if (f->worlds[i] != NULL &&
            mc_world_runtime_free(f->worlds[i], NULL) != MC_WORLD_STORAGE_OK)
            return false;
    }
    for (size_t i = 0; i < COUNT(f->dimensions); ++i)
        mc_world_dimension_release(f->dimensions[i]);
    for (size_t i = 0; i < COUNT(f->classes); ++i) {
        if (f->classes[i] != NULL &&
            mc_world_runtime_classes_free(f->classes[i], NULL) != MC_WORLD_STORAGE_OK)
            return false;
    }
    for (size_t i = 0; i < COUNT(f->registry); ++i) {
        if (f->registry[i] != NULL &&
            mc_registry_classes_free(f->registry[i], NULL) != MC_REGISTRY_OK)
            return false;
    }
    for (size_t i = 0; i < COUNT(f->entities); ++i) {
        if (f->entities[i] != NULL && mc_entity_classes_free(f->entities[i]) != MC_ENTITY_OK)
            return false;
    }
    mc_world_info_name_release(f->name);
    for (size_t i = 0; i < COUNT(f->stores); ++i)
        mc_world_store_free(f->stores[i]);
    mc_world_regions_free(f->regions);
    mc_java_random_profile_free(f->profile);
    mc_materials_destroy(f->materials);
    free(f);
    return true;
}

static bool reference_failure(reference_json *json, const mc_world_mutation_error *error) {
    if (!reference_printf(
            json, ",\"failure\":{\"status\":%u,\"operation\":", (unsigned int)error->status) ||
        !reference_string(json, error->operation) || !reference_printf(json, ",\"hook_class\":") ||
        !reference_string(json, error->hook_class) ||
        !reference_printf(json, ",\"hook_method\":") ||
        !reference_string(json, error->hook_method) ||
        !reference_printf(json,
                          ",\"hook_bytecode\":%u,\"world_bytecode\":%u,\"chunk_bytecode\":%u,"
                          "\"query_bytecode\":%u,\"registry_status\":%u,\"cause_operation\":",
                          error->hook_bytecode, error->bytecode, error->chunk_bytecode,
                          error->query.bytecode, (unsigned int)error->registry.status) ||
        !reference_string(json, error->cause.operation) ||
        !reference_printf(json, ",\"cause_hook_class\":") ||
        !reference_string(json, error->cause_hook_class) ||
        !reference_printf(json, ",\"cause_hook_method\":") ||
        !reference_string(json, error->cause_hook_method))
        return false;
    return reference_printf(json, ",\"cause_hook_bytecode\":%u}", error->cause_hook_bytecode);
}

static bool reference_observe(reference_json *json, const reference_request *request, size_t side,
                              const char *root) {
    bool ok = false, entered = false;
    bool leased[2][2] = {{false}};
    mc_world_chunk *chunks[2][2] = {{NULL}};
    fixture *f = calloc(1, sizeof(*f));
    reference_active = request;
    if (f == NULL || !reference_initialize(f, side, root))
        goto done;
    mc_world_runtime *world = f->worlds[side];
    world->fields.B = request->remote;
    if (world->fields.r == NULL)
        goto done;
    mc_java_random_reseed(world->fields.r, request->world_seed);
    mc_java_random supplied;
    mc_java_random_seed(&supplied, request->seed);
    const mc_block *block;
    if (mc_registry_block_get(f->registry[side], request->id, &block, NULL) != MC_REGISTRY_OK ||
        block == NULL || (request->action == REFERENCE_GROW && block->kind != MC_BLOCK_HE_EU) ||
        mc_world_runtime_call_enter(world, NULL) != MC_WORLD_STORAGE_OK)
        goto done;
    entered = true;
    for (size_t x = 0; x < 2; ++x) {
        for (size_t z = 0; z < 2; ++z) {
            if (mc_world_store_lease_chunk(f->stores[side], f->chunks[side][x][z], NULL) !=
                MC_WORLD_STORAGE_OK)
                goto done;
            leased[x][z] = true;
            chunks[x][z] = mc_world_store_chunk_object(f->stores[side], f->chunks[side][x][z]);
        }
    }
    if (!reference_printf(json, "{\"name\":") || !reference_string(json, request->name) ||
        !reference_printf(json, ",\"before\":") || !reference_snapshot(json, chunks))
        goto done;
    mc_world_mutation_error error = mc_world_mutation_empty_error();
    bool returned;
    if (request->action == REFERENCE_NEIGHBOR)
        returned = mc_world_mutation_block_neighbor(world, block, request->x, request->y,
                                                    request->z, 0, &error);
    else if (request->action == REFERENCE_GROW)
        returned = mc_plant_sapling_grow(world, block, request->x, request->y, request->z,
                                         &supplied, &error);
    else
        returned = mc_world_mutation_block_tick(world, block, request->x, request->y, request->z,
                                                &supplied, &error);
    if (!reference_printf(json, ",\"outcome\":{\"status\":\"%s\"},\"after\":",
                          returned ? "returned" : "native_failed") ||
        !reference_snapshot(json, chunks) ||
        !reference_printf(json, ",\"supplied_random_tail\":") || !reference_tail(json, &supplied) ||
        !reference_printf(json, ",\"world_random_tail\":") ||
        !reference_tail(json, world->fields.r) || !reference_printf(json, ",\"drops\":") ||
        !reference_drops(json, world) || (!returned && !reference_failure(json, &error)) ||
        !reference_printf(json, "}"))
        goto done;
    ok = true;
done:
    if (f != NULL) {
        for (size_t x = 0; x < 2; ++x) {
            for (size_t z = 0; z < 2; ++z) {
                if (leased[x][z] &&
                    mc_world_store_release_chunk(f->stores[side], f->chunks[side][x][z], NULL) !=
                        MC_WORLD_STORAGE_OK)
                    ok = false;
            }
        }
        if (entered)
            mc_world_runtime_call_leave(f->worlds[side]);
        if (!reference_destroy(f)) {
            fputs("plant reference cleanup refused; live dependencies retained\n", stderr);
            ok = false;
        }
    }
    reference_active = NULL;
    return ok;
}

int main(int argc, char **argv) {
    int result = 1;
    size_t count = 0;
    reference_request *requests = calloc(REFERENCE_REQUESTS, sizeof(*requests));
    reference_json json = {0};
    char root[8192], path[8192];
    const size_t side = argc > 1 && strcmp(argv[1], "server") == 0 ? 1u : 0u;
    if (argc != 5 || (strcmp(argv[1], "client") != 0 && strcmp(argv[1], "server") != 0) ||
        requests == NULL || !decode_path(argv[3], root, sizeof(root)) ||
        !mc_math_trig_init(&trig) || !reference_requests(argv[2], requests, &count) ||
        mc_ws_mkdirs(root, NULL) != MC_WORLD_STORAGE_OK ||
        !reference_printf(&json,
                          "{\"schema\":\"native-plant-lifecycle-v1\",\"side\":\"%s\","
                          "\"extent\":[0,31,0,127,0,31],\"sparse_default\":[0,0],\"cases\":[",
                          argv[1]))
        goto done;
    for (size_t i = 0; i < count; ++i) {
        const int n = snprintf(path, sizeof(path), "%s/case-%02zu", root, i);
        bool exists = true;
        if (n < 0 || (size_t)n >= sizeof(path) ||
            mc_ws_path_exists(path, &exists, NULL) != MC_WORLD_STORAGE_OK || exists ||
            mc_ws_mkdirs(path, NULL) != MC_WORLD_STORAGE_OK ||
            (i != 0 && !reference_printf(&json, ",")) ||
            !reference_observe(&json, &requests[i], side, path)) {
            fprintf(stderr, "plant reference case %zu failed before projection completion\n", i);
            goto done;
        }
    }
    if (!reference_printf(&json, "]}\n") ||
        mc_ws_write_file(argv[4], (const uint8_t *)json.bytes, json.length, true, NULL) !=
            MC_WORLD_STORAGE_OK)
        goto done;
    result = 0;
done:
    if (requests != NULL) {
        for (size_t i = 0; i < REFERENCE_REQUESTS; ++i)
            free(requests[i].prepared);
    }
    free(requests);
    free(json.bytes);
    if (result != 0)
        fputs("plant reference probe failed; no complete transcript written\n", stderr);
    return result;
}
