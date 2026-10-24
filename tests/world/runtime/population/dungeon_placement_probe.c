/* One real prepared GAME call per process. The included contract main and
 * original TileEntity main are compiled under other names and never executed here. */
#define MC_DUNGEON_TEST_MAIN mc_dungeon_placement_contract_fixture_main
#include "test_dungeon.c"
#undef MC_DUNGEON_TEST_MAIN

#include "mc/world/runtime/scheduling.h"

#include <inttypes.h>
#include <stdarg.h>

enum {
    DP_TILES = 16,
    DP_SLOTS = 36,
    DP_STACKS = DP_TILES * DP_SLOTS,
    DP_LIGHTS = 4096,
    DP_JSON_BYTES = 1024 * 1024
};

typedef struct dp_stack {
    size_t identity;
    mc_item_stack_fields fields;
} dp_stack;

typedef struct dp_tile {
    size_t identity;
    mc_tile_entity_snapshot fields;
    bool world_binding, name_null;
    size_t name_length;
    uint16_t name[65535];
    dp_stack slots[DP_SLOTS];
} dp_tile;

typedef struct dp_map_row {
    size_t identity;
    int32_t x, y, z;
    int32_t key_x, key_y, key_z;
} dp_map_row;

typedef struct dp_light {
    mc_world_light_type_fields kind;
    mc_light_bounds bounds;
} dp_light;

typedef struct dp_image {
    uint8_t blocks[MC_CHUNK_BLOCKS];
    uint8_t metadata[MC_CHUNK_NIBBLE_BYTES], sky[MC_CHUNK_NIBBLE_BYTES];
    uint8_t light[MC_CHUNK_NIBBLE_BYTES], height[MC_CHUNK_HEIGHTMAP_BYTES];
    mc_world_chunk_fields fields;
    bool world_binding, world_a, world_o, world_L;
    bool map_valid, lists_valid, containers_retired;
    int64_t world_time;
    int32_t E_count, F_count, enqueue_depth, drain_depth;
    bool chunk_static_a;
    mc_world_schedule_class_state scheduling;
    size_t light_count;
    dp_light lights[DP_LIGHTS];
    const mc_world_reference_list *c, *G;
    size_t map_count, c_count, G_count, tile_entity_count;
    dp_map_row map[DP_TILES];
    size_t active[DP_TILES], deferred[DP_TILES], entity_counts[8];
    dp_tile tile_entities[DP_TILES];
} dp_image;

typedef struct dp_capture {
    size_t case_index, side, openings;
    int reject;
    int32_t rx, rz, prefix_x, prefix_z;
    uint64_t seed_bits, priming_bits;
    mc_java_random before_random, after_random, continuation_random;
    mc_world_storage_status status;
    bool returned;
    mc_population_dungeon_error error;
    dp_image before, after;
    mc_tile_entity *held_tiles[DP_TILES];
    mc_item_stack *held_stacks[DP_STACKS];
    size_t held_tile_count, held_stack_count;
    bool repeated_tile_same, repeated_stack_same;
    bool chunk_same, map_same, wrappers_same, blocks_same, height_same, c_same, G_same;
    uint64_t gaussian_bits, long_bits, double_bits;
    int32_t bounded, integer;
} dp_capture;

typedef struct dp_json {
    char *bytes;
    size_t length;
    bool ok;
} dp_json;

static uint64_t dp_double_bits(double value) {
    uint64_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static const char *dp_bool(bool value) {
    return value ? "true" : "false";
}

static bool dp_seed(const char *text, uint64_t *bits, int64_t *seed) {
    if (strlen(text) != 16u)
        return false;
    uint64_t value = 0;
    for (size_t i = 0; i < 16u; ++i) {
        const unsigned char c = (unsigned char)text[i];
        unsigned int digit;
        if (c >= '0' && c <= '9')
            digit = (unsigned int)(c - '0');
        else if (c >= 'a' && c <= 'f')
            digit = (unsigned int)(c - 'a') + 10u;
        else
            return false;
        value = (value << 4u) | digit;
    }
    *bits = value;
    *seed =
        value <= INT64_MAX ? (int64_t)value : INT64_MIN + (int64_t)(value - (UINT64_C(1) << 63u));
    return true;
}

static bool dp_index(const char *text, size_t *out) {
    const size_t length = strlen(text);
    if (length == 0 || length > 2u)
        return false;
    size_t value = 0;
    for (size_t i = 0; i < length; ++i) {
        if (text[i] < '0' || text[i] > '9')
            return false;
        value = value * 10u + (size_t)(text[i] - '0');
    }
    if (value >= 24u)
        return false;
    *out = value;
    return true;
}

/* IDs are only same-process identity joins. Each new actual identity receives
 * an ordinary hold until all before/after values have been copied. */
static bool dp_tile_identity(dp_capture *capture, mc_tile_entity *tile_entity, size_t *out) {
    if (tile_entity == NULL) {
        *out = 0;
        return true;
    }
    for (size_t i = 0; i < capture->held_tile_count; ++i)
        if (capture->held_tiles[i] == tile_entity) {
            *out = i + 1u;
            return true;
        }
    if (capture->held_tile_count == DP_TILES ||
        mc_tile_entity_retain(tile_entity) != MC_TILE_ENTITY_OK)
        return false;
    capture->held_tiles[capture->held_tile_count++] = tile_entity;
    *out = capture->held_tile_count;
    return true;
}

static bool dp_stack_identity(dp_capture *capture, mc_item_stack *stack, size_t *out) {
    if (stack == NULL) {
        *out = 0;
        return true;
    }
    for (size_t i = 0; i < capture->held_stack_count; ++i)
        if (capture->held_stacks[i] == stack) {
            *out = i + 1u;
            return true;
        }
    if (capture->held_stack_count == DP_STACKS || mc_item_stack_retain(stack) != MC_ITEM_STACK_OK)
        return false;
    capture->held_stacks[capture->held_stack_count++] = stack;
    *out = capture->held_stack_count;
    return true;
}

static void dp_release(dp_capture *capture) {
    if (capture == NULL)
        return;
    for (size_t i = 0; i < capture->held_stack_count; ++i)
        mc_item_stack_release(capture->held_stacks[i]);
    for (size_t i = 0; i < capture->held_tile_count; ++i)
        mc_tile_entity_release(capture->held_tiles[i]);
    capture->held_stack_count = capture->held_tile_count = 0;
}

static bool dp_copy_tile(dp_capture *capture, mc_world_runtime *world, size_t identity,
                         dp_tile *out) {
    mc_tile_entity *tile_entity = capture->held_tiles[identity - 1u];
    out->identity = identity;
    if (mc_tile_entity_get_snapshot(tile_entity, &out->fields) != MC_TILE_ENTITY_OK)
        return false;
    out->world_binding = out->fields.world == world;
    if (out->fields.kind == MC_TILE_ENTITY_CHEST) {
        if (out->fields.phase != MC_TILE_ENTITY_READY || out->fields.chest_capacity > DP_SLOTS)
            return false;
        for (size_t slot = 0; slot < out->fields.chest_capacity; ++slot) {
            mc_item_stack *first = NULL, *second = NULL;
            bool ok =
                mc_tile_entity_chest_get(tile_entity, (int32_t)slot, &first) == MC_TILE_ENTITY_OK;
            if (ok)
                ok = mc_tile_entity_chest_get(tile_entity, (int32_t)slot, &second) ==
                     MC_TILE_ENTITY_OK;
            if (ok) {
                capture->repeated_stack_same = capture->repeated_stack_same && first == second;
                ok = first == second &&
                     dp_stack_identity(capture, first, &out->slots[slot].identity);
                if (ok && first != NULL)
                    ok = mc_item_stack_fields_get(first, &out->slots[slot].fields) ==
                         MC_ITEM_STACK_OK;
            }
            mc_item_stack_release(second);
            mc_item_stack_release(first);
            if (!ok)
                return false;
        }
    } else if (out->fields.kind == MC_TILE_ENTITY_SPAWNER) {
        mc_tile_entity_name *name = out->fields.spawner_name;
        if (mc_tile_entity_name_retain(name) != MC_TILE_ENTITY_OK)
            return false;
        mc_tile_entity_name_view view;
        const mc_tile_entity_status status = mc_tile_entity_name_get_view(name, &view);
        const bool ok = status == MC_TILE_ENTITY_OK && view.text.length <= COUNT(out->name) &&
                        (view.text.length == 0 || view.text.units != NULL);
        if (ok) {
            out->name_null = view.is_null;
            out->name_length = view.text.length;
            if (view.text.length != 0)
                memcpy(out->name, view.text.units, view.text.length * sizeof(*out->name));
        }
        mc_tile_entity_name_release(name);
        if (!ok)
            return false;
    } else {
        return false;
    }
    return true;
}

static bool dp_collect_list(dp_capture *capture, const mc_world_reference_list *list, size_t *count,
                            size_t identities[DP_TILES]) {
    if (list == NULL || list->count > DP_TILES || list->count > list->capacity ||
        (list->count != 0 && list->items == NULL))
        return false;
    *count = list->count;
    for (size_t i = 0; i < list->count; ++i)
        if (!dp_tile_identity(capture, (mc_tile_entity *)list->items[i], &identities[i]))
            return false;
    return true;
}

static bool dp_snapshot(dp_capture *capture, fixture *f, dp_image *out) {
    mc_world_chunk *chunk = f->chunks[capture->side];
    mc_world_runtime *world = f->worlds[capture->side];
    if (chunk == NULL || mc_world_chunk_fields_get(chunk, &out->fields) != MC_WORLD_STORAGE_OK ||
        out->fields.b == NULL || out->fields.h == NULL || chunk->data == NULL ||
        chunk->data->blocks_capacity != MC_CHUNK_BLOCKS)
        return false;
    memcpy(out->blocks, out->fields.b, sizeof(out->blocks));
    memcpy(out->height, out->fields.h, sizeof(out->height));
    const mc_world_chunk_nibble *wrappers[] = {out->fields.e, out->fields.f, out->fields.g};
    uint8_t *arrays[] = {out->metadata, out->sky, out->light};
    for (size_t i = 0; i < COUNT(wrappers); ++i) {
        mc_world_chunk_nibble_fields fields;
        if (mc_world_chunk_nibble_fields_get(wrappers[i], &fields) != MC_WORLD_STORAGE_OK ||
            fields.a == NULL || fields.length != MC_CHUNK_NIBBLE_BYTES)
            return false;
        memcpy(arrays[i], fields.a, MC_CHUNK_NIBBLE_BYTES);
    }
    out->world_binding = out->fields.d == world;
    out->world_a = world->fields.a;
    out->world_o = world->fields.o;
    out->world_L = world->fields.L;
    out->containers_retired = world->tile_entity_containers_retired;
    out->map_valid = mc_live_tile_entities_map_valid(chunk);
    out->lists_valid = mc_live_tile_entities_lists_valid(world);
    out->c = world->fields.c;
    out->G = world->fields.G;
    out->enqueue_depth = world->classes->enqueue_depth;
    out->drain_depth = world->fields.M;
    out->chunk_static_a = world->classes->chunk_a;
    if (mc_world_info_time_get(world->fields.x, &out->world_time) != MC_WORLD_INFO_OK ||
        mc_scheduled_ordered_size(world->fields.E, &out->E_count) != MC_SCHEDULED_ORDERED_OK ||
        mc_scheduled_membership_size(world->fields.F, &out->F_count) !=
            MC_SCHEDULED_MEMBERSHIP_OK ||
        out->E_count != 0 || out->F_count != 0 ||
        mc_world_runtime_schedule_class_get(world->classes, &out->scheduling) !=
            MC_WORLD_STORAGE_OK)
        return false;
    size_t written = SIZE_MAX;
    if (mc_scheduled_ordered_snapshot(world->fields.E, NULL, 0, &written) !=
            MC_SCHEDULED_ORDERED_OK ||
        written != 0)
        return false;
    written = SIZE_MAX;
    if (mc_scheduled_membership_snapshot(world->fields.F, NULL, 0, &written) !=
            MC_SCHEDULED_MEMBERSHIP_OK ||
        written != 0)
        return false;
    mc_world_light_list_view queue;
    if (mc_world_runtime_light_list_get(world, &queue, NULL) != MC_WORLD_STORAGE_OK ||
        queue.count > DP_LIGHTS)
        return false;
    out->light_count = queue.count;
    for (size_t i = 0; i < queue.count; ++i) {
        const mc_world_light_region *region = NULL;
        if (mc_world_runtime_light_at(world, i, &region, NULL) != MC_WORLD_STORAGE_OK)
            return false;
        const mc_world_light_region_fields *fields =
            mc_world_runtime_light_region_fields_get(region);
        const mc_world_light_type_fields *kind =
            fields == NULL ? NULL : mc_world_runtime_light_type_fields_get(fields->a);
        if (kind == NULL || (kind->kind != MC_LIGHT_SKY && kind->kind != MC_LIGHT_BLOCK) ||
            fields->a != world->classes->light_statics[(size_t)kind->kind])
            return false;
        out->lights[i].kind = *kind;
        out->lights[i].bounds = fields->bounds;
    }
    if (!out->map_valid || !out->lists_valid || out->fields.m == NULL ||
        out->fields.m->length != COUNT(out->entity_counts) ||
        mc_world_chunk_tile_entity_count(out->fields.l, &out->map_count) != MC_WORLD_STORAGE_OK ||
        out->map_count > DP_TILES)
        return false;
    for (size_t i = 0; i < COUNT(out->entity_counts); ++i) {
        const mc_world_chunk_entity_list *slice = NULL;
        if (mc_world_chunk_slice_at(out->fields.m, i, &slice) != MC_WORLD_STORAGE_OK ||
            mc_world_chunk_entity_count(slice, &out->entity_counts[i]) != MC_WORLD_STORAGE_OK ||
            out->entity_counts[i] != 0)
            return false; /* Declared input is genuinely known-empty, not omitted live Entities. */
    }
    for (size_t i = 0; i < out->map_count; ++i) {
        mc_tile_entity *first = NULL, *second = NULL;
        mc_live_tile_entity_map_entry entry;
        if (mc_live_tile_entities_map_entry_at(chunk, i, &entry) != MC_WORLD_STORAGE_OK ||
            mc_live_tile_entities_map_at(chunk, i, &first) != MC_WORLD_STORAGE_OK ||
            mc_live_tile_entities_map_at(chunk, i, &second) != MC_WORLD_STORAGE_OK || first == NULL)
            return false;
        capture->repeated_tile_same = capture->repeated_tile_same && first == second;
        mc_tile_entity_snapshot tile_entity;
        if (entry.tile_entity != first || first != second ||
            !dp_tile_identity(capture, first, &out->map[i].identity) ||
            mc_tile_entity_get_snapshot(first, &tile_entity) != MC_TILE_ENTITY_OK)
            return false;
        out->map[i].x = tile_entity.x;
        out->map[i].y = tile_entity.y;
        out->map[i].z = tile_entity.z;
        out->map[i].key_x = entry.x;
        out->map[i].key_y = entry.y;
        out->map[i].key_z = entry.z;
    }
    if (!dp_collect_list(capture, out->c, &out->c_count, out->active) ||
        !dp_collect_list(capture, out->G, &out->G_count, out->deferred))
        return false;
    bool seen[DP_TILES + 1] = {false};
    for (size_t group = 0; group < 3; ++group) {
        const size_t count = group == 0 ? out->map_count : group == 1 ? out->c_count : out->G_count;
        for (size_t i = 0; i < count; ++i) {
            const size_t identity = group == 0   ? out->map[i].identity
                                    : group == 1 ? out->active[i]
                                                 : out->deferred[i];
            if (identity != 0 && !seen[identity]) {
                if (out->tile_entity_count == DP_TILES ||
                    !dp_copy_tile(capture, world, identity,
                                  &out->tile_entities[out->tile_entity_count]))
                    return false;
                ++out->tile_entity_count;
                seen[identity] = true;
            }
        }
    }
    return true;
}

/* Fixed-capacity construction keeps one bounded complete JSON document in
 * memory; no capture bytes are emitted before real checked teardown succeeds. */
static void dp_write(dp_json *json, const char *format, ...) {
    if (!json->ok)
        return;
    va_list arguments;
    va_start(arguments, format);
    const int length =
        vsnprintf(json->bytes + json->length, DP_JSON_BYTES - json->length, format, arguments);
    va_end(arguments);
    if (length < 0 || (size_t)length >= (size_t)DP_JSON_BYTES - json->length)
        json->ok = false;
    else
        json->length += (size_t)length;
}

static void dp_string(dp_json *json, const char *text) {
    if (text == NULL) {
        dp_write(json, "null");
        return;
    }
    dp_write(json, "\"");
    for (const unsigned char *p = (const unsigned char *)text; *p != 0; ++p) {
        if (*p == '"' || *p == '\\')
            dp_write(json, "\\%c", (int)*p);
        else if (*p < 32u || *p >= 127u)
            dp_write(json, "\\u%04x", (unsigned int)*p);
        else
            dp_write(json, "%c", (int)*p);
    }
    dp_write(json, "\"");
}

static void dp_hex(dp_json *json, const uint8_t *bytes, size_t length) {
    static const char digits[] = "0123456789abcdef";
    if (!json->ok || length > ((size_t)DP_JSON_BYTES - json->length - 1u) / 2u ||
        (size_t)DP_JSON_BYTES - json->length - 1u - 2u * length < 2u) {
        json->ok = false;
        return;
    }
    json->bytes[json->length++] = '"';
    for (size_t i = 0; i < length; ++i) {
        json->bytes[json->length++] = digits[bytes[i] >> 4u];
        json->bytes[json->length++] = digits[bytes[i] & 15u];
    }
    json->bytes[json->length++] = '"';
    json->bytes[json->length] = '\0';
}

static void dp_random(dp_json *json, const mc_java_random *random) {
    dp_write(json,
             "{\"state_hex\":\"%016" PRIx64 "\",\"gaussian_valid\":%s,"
             "\"stored_gaussian_bits\":\"%016" PRIx64 "\"}",
             random->state, dp_bool(random->have_next_next_gaussian),
             dp_double_bits(random->next_next_gaussian));
}

static void dp_ids(dp_json *json, const size_t *identities, size_t count) {
    dp_write(json, "[");
    for (size_t i = 0; i < count; ++i)
        dp_write(json, "%s%zu", i == 0 ? "" : ",", identities[i]);
    dp_write(json, "]");
}

static void dp_tile_json(dp_json *json, const dp_tile *tile_entity) {
    const mc_tile_entity_snapshot *s = &tile_entity->fields;
    dp_write(json,
             "{\"tile_id\":%zu,\"kind\":\"%s\",\"phase\":%d,"
             "\"position\":[%" PRId32 ",%" PRId32 ",%" PRId32 "],"
             "\"invalid\":%s,\"world_binding\":%s",
             tile_entity->identity, s->kind == MC_TILE_ENTITY_CHEST ? "chest" : "spawner",
             (int)s->phase, s->x, s->y, s->z, dp_bool(s->invalid),
             dp_bool(tile_entity->world_binding));
    if (s->kind == MC_TILE_ENTITY_CHEST) {
        dp_write(json, ",\"chest_capacity\":%zu,\"slots\":[", s->chest_capacity);
        for (size_t i = 0; i < s->chest_capacity; ++i) {
            const dp_stack *stack = &tile_entity->slots[i];
            dp_write(json, "%s", i == 0 ? "" : ",");
            if (stack->identity == 0)
                dp_write(json, "null");
            else
                dp_write(json,
                         "{\"stack_id\":%zu,\"id\":%" PRId32 ",\"count\":%" PRId32
                         ",\"damage\":%" PRId32 ",\"animation\":%" PRId32 "}",
                         stack->identity, stack->fields.id, stack->fields.count,
                         stack->fields.damage, stack->fields.animation);
        }
        dp_write(json, "]");
    } else {
        dp_write(json, ",\"spawner_name_utf16\":");
        if (tile_entity->name_null)
            dp_write(json, "null");
        else {
            dp_write(json, "\"");
            for (size_t i = 0; i < tile_entity->name_length; ++i)
                dp_write(json, "%04x", (unsigned int)tile_entity->name[i]);
            dp_write(json, "\"");
        }
        dp_write(json,
                 ",\"spawner_delay\":%" PRId32 ",\"rotation_bits\":\"%016" PRIx64
                 "\",\"previous_rotation_bits\":\"%016" PRIx64 "\"",
                 s->spawner_delay, s->spawner_rotation_bits, s->spawner_previous_rotation_bits);
    }
    dp_write(json, "}");
}

static void dp_image_json(dp_json *json, const dp_image *image) {
    const mc_world_chunk_fields *s = &image->fields;
    dp_write(json, "{\"blocks_hex\":");
    dp_hex(json, image->blocks, sizeof(image->blocks));
    dp_write(json, ",\"metadata_hex\":");
    dp_hex(json, image->metadata, sizeof(image->metadata));
    dp_write(json, ",\"skylight_hex\":");
    dp_hex(json, image->sky, sizeof(image->sky));
    dp_write(json, ",\"block_light_hex\":");
    dp_hex(json, image->light, sizeof(image->light));
    dp_write(json, ",\"heightmap_hex\":");
    dp_hex(json, image->height, sizeof(image->height));
    dp_write(json,
             ",\"minimum_height\":%" PRId32 ",\"coordinates\":[%" PRId32 ",%" PRId32
             "],\"flags\":{\"c\":%s,\"n\":%s,\"o\":%s,\"p\":%s,\"q\":%s},"
             "\"last_save_time_hex\":\"%016" PRIx64 "\",\"world_binding\":%s,"
             "\"World_flags\":{\"a\":%s,\"o\":%s,\"L\":%s},"
             "\"Tile_Map_valid\":%s,\"Tile_Map\":[",
             s->i, s->j, s->k, dp_bool(s->c), dp_bool(s->n), dp_bool(s->o), dp_bool(s->p),
             dp_bool(s->q), (uint64_t)s->r, dp_bool(image->world_binding), dp_bool(image->world_a),
             dp_bool(image->world_o), dp_bool(image->world_L), dp_bool(image->map_valid));
    for (size_t i = 0; i < image->map_count; ++i) {
        const dp_map_row *row = &image->map[i];
        dp_write(json,
                 "%s{\"ordinal\":%zu,\"tile_id\":%zu,\"position\":[%" PRId32 ",%" PRId32 ",%" PRId32
                 "],\"key\":[%" PRId32 ",%" PRId32 ",%" PRId32 "]}",
                 i == 0 ? "" : ",", i, row->identity, row->x, row->y, row->z, row->key_x,
                 row->key_y, row->key_z);
    }
    dp_write(json, "],\"World_Tile_lists\":{\"c\":");
    dp_ids(json, image->active, image->c_count);
    dp_write(json, ",\"G\":");
    dp_ids(json, image->deferred, image->G_count);
    dp_write(json, ",\"L\":%s,\"valid\":%s,\"containers_retired\":%s},\"Tiles\":[",
             dp_bool(image->world_L), dp_bool(image->lists_valid),
             dp_bool(image->containers_retired));
    for (size_t i = 0; i < image->tile_entity_count; ++i) {
        dp_write(json, "%s", i == 0 ? "" : ",");
        dp_tile_json(json, &image->tile_entities[i]);
    }
    dp_write(json, "],\"Entity_slices\":");
    dp_ids(json, image->entity_counts, COUNT(image->entity_counts));
    dp_write(json,
             ",\"world_time\":%" PRId64 ",\"enqueue_depth\":%" PRId32 ",\"drain_depth\":%" PRId32
             ",\"chunk_static_a\":%s,\"Light_queue\":[",
             image->world_time, image->enqueue_depth, image->drain_depth,
             dp_bool(image->chunk_static_a));
    for (size_t i = 0; i < image->light_count; ++i) {
        const dp_light *light = &image->lights[i];
        const mc_light_bounds *b = &light->bounds;
        dp_write(json,
                 "%s{\"ordinal\":%zu,\"kind\":{\"ordinal\":%d,\"identity\":\"%s\","
                 "\"name\":\"%s\",\"default\":%" PRId32 "},\"bounds\":[%" PRId32 ",%" PRId32
                 ",%" PRId32 ",%" PRId32 ",%" PRId32 ",%" PRId32 "]}",
                 i == 0 ? "" : ",", i, (int)light->kind.kind,
                 light->kind.kind == MC_LIGHT_SKY ? "a" : "b",
                 light->kind.kind == MC_LIGHT_SKY ? "Sky" : "Block", light->kind.c, b->min_x,
                 b->min_y, b->min_z, b->max_x, b->max_y, b->max_z);
    }
    dp_write(json,
             "],\"World_scheduling\":{\"E_count\":%" PRId32 ",\"F_count\":%" PRId32
             ",\"E\":[],\"F\":[],\"class_initialized\":%s,"
             "\"next_sequence_hex\":\"%016" PRIx64 "\"}}",
             image->E_count, image->F_count, dp_bool(image->scheduling.initialized),
             (uint64_t)image->scheduling.next);
}

static bool dp_document(dp_json *json, const dp_capture *capture) {
    const mc_population_dungeon_error *error = &capture->error;
    dp_write(json,
             "{\"schema\":\"dungeon-placement-native-a\",\"side\":\"%s\","
             "\"case_index\":%zu,\"seed_hex\":\"%016" PRIx64 "\","
             "\"center\":[8,64,8],\"radius_x\":%" PRId32 ",\"radius_z\":%" PRId32
             ",\"openings\":%zu,\"reject\":%d,\"priming_gaussian_bits\":\"%016" PRIx64
             "\",\"independent_prefix\":{\"nextInt2_x\":%" PRId32 ",\"nextInt2_z\":%" PRId32
             "},\"placement\":{\"status\":%d,\"boolean\":%s,"
             "\"bytecode\":%u,\"loot_bytecode\":%u,\"mob_bytecode\":%u,"
             "\"air_bytecode\":%u,\"allocation_bytecode\":%u,"
             "\"stack_constructor_bytecode\":%u,\"operation\":",
             capture->side == 0 ? "client" : "server", capture->case_index, capture->seed_bits,
             capture->rx, capture->rz, capture->openings, capture->reject, capture->priming_bits,
             capture->prefix_x, capture->prefix_z, (int)capture->status,
             capture->status == MC_WORLD_STORAGE_OK ? dp_bool(capture->returned) : "null",
             error->bytecode, error->loot_bytecode, error->mob_bytecode, error->air_bytecode,
             error->allocation_bytecode, error->stack_constructor_bytecode);
    dp_string(json, error->operation);
    dp_write(json,
             ",\"registry_status\":%d,\"material_status\":%d,\"mutation_status\":%d,"
             "\"tile_status\":%d,\"item_status\":%d},\"before\":",
             (int)error->registry.status, (int)error->material.status, (int)error->mutation.status,
             (int)error->tile_entity.status, (int)error->item.status);
    dp_image_json(json, &capture->before);
    dp_write(json, ",\"after\":");
    dp_image_json(json, &capture->after);
    dp_write(json, ",\"supplied_Random\":{\"before\":");
    dp_random(json, &capture->before_random);
    dp_write(json, ",\"after\":");
    dp_random(json, &capture->after_random);
    dp_write(json, ",\"after_continuation\":");
    dp_random(json, &capture->continuation_random);
    dp_write(json,
             "},\"continuation\":{\"gaussian_bits\":\"%016" PRIx64 "\",\"bounded11\":%" PRId32
             ",\"integer\":%" PRId32 ",\"long_bits\":\"%016" PRIx64
             "\",\"double_bits\":\"%016" PRIx64
             "\"},\"identity_checks\":{\"chunk_same\":%s,\"map_descriptor_same\":%s,"
             "\"nibble_wrappers_same\":%s,\"blocks_backing_same\":%s,"
             "\"heightmap_backing_same\":%s,\"World_c_descriptor_same\":%s,"
             "\"World_G_descriptor_same\":%s,\"repeated_Map_value_same\":%s,"
             "\"repeated_Stack_get_same\":%s},\"cleanup_passed\":true}\n",
             capture->gaussian_bits, capture->bounded, capture->integer, capture->long_bits,
             capture->double_bits, dp_bool(capture->chunk_same), dp_bool(capture->map_same),
             dp_bool(capture->wrappers_same), dp_bool(capture->blocks_same),
             dp_bool(capture->height_same), dp_bool(capture->c_same), dp_bool(capture->G_same),
             dp_bool(capture->repeated_tile_same), dp_bool(capture->repeated_stack_same));
    return json->ok;
}

int main(int argc, char **argv) {
    int result = 1;
    fixture *f = NULL;
    dp_capture *capture = NULL;
    dp_json json = {0};
    bool captured = false;
    char root[8192];
    int64_t seed;
    if (argc != 6 || strcmp(argv[1], "dungeon-placement") != 0 ||
        (strcmp(argv[2], "client") != 0 && strcmp(argv[2], "server") != 0)) {
        fputs("Usage: dungeon-placement client|server case_index0..23 seed_hex16 root_path_hex\n",
              stderr);
        goto cleanup;
    }
    capture = calloc(1, sizeof(*capture));
    if (capture == NULL || !dp_index(argv[3], &capture->case_index) ||
        !dp_seed(argv[4], &capture->seed_bits, &seed) ||
        !decode_path(argv[5], root, sizeof(root))) {
        fputs("Dungeon placement probe input/allocation failure\n", stderr);
        goto cleanup;
    }
    capture->side = strcmp(argv[2], "client") == 0 ? 0u : 1u;
    capture->rx = 2 + (int32_t)(capture->case_index / 12u);
    capture->rz = 2 + (int32_t)((capture->case_index / 6u) % 2u);
    const size_t variant = capture->case_index % 6u;
    capture->openings = variant == 1u ? 5u : variant == 2u ? 0u : variant == 3u ? 6u : 1u;
    capture->reject = variant == 4u ? 1 : variant == 5u ? 2 : 0;
    capture->repeated_tile_same = capture->repeated_stack_same = true;
    const dg_plan room = {.rx = capture->rx, .rz = capture->rz};
    if (!mc_math_trig_init(&trig) || !dg_setup(&f, root) ||
        mc_world_info_time_set(f->worlds[capture->side]->fields.x, 1234) != MC_WORLD_INFO_OK ||
        !dg_room(f, capture->side, &room, capture->openings, capture->reject)) {
        fputs("Dungeon placement genuine setup/acquisition failed\n", stderr);
        goto cleanup;
    }
    mc_java_random random;
    /* Exactly dg_start's ordinary seed + ONE actual nextGaussian, retaining its
     * actual return value rather than reading/replacing the cache fields. */
    mc_java_random_seed(&random, seed);
    capture->priming_bits = dp_double_bits(mc_java_random_next_gaussian(&random));
    capture->before_random = random;
    mc_java_random independent = random; /* Radius selection only. */
    if (!mc_java_random_next_bounded(&independent, 2, &capture->prefix_x) ||
        !mc_java_random_next_bounded(&independent, 2, &capture->prefix_z) ||
        capture->prefix_x + 2 != capture->rx || capture->prefix_z + 2 != capture->rz) {
        fputs("Supplied primed seed does not select the declared radius pair\n", stderr);
        goto cleanup;
    }
    if (!dp_snapshot(capture, f, &capture->before)) {
        fputs("Dungeon placement before-image capture failed\n", stderr);
        goto cleanup;
    }
    mc_world_chunk *held_chunk = f->chunks[capture->side];
    fprintf(stderr, "DUNGEON_NATIVE_PLACE_BEGIN %s case=%zu\n", argv[2], capture->case_index);
    capture->status = mc_population_dungeon_place(f->worlds[capture->side], &random, NULL, DG_X,
                                                  DG_Y, DG_Z, &capture->returned, &capture->error);
    capture->after_random = random;
    fprintf(stderr, "DUNGEON_NATIVE_PLACE_END status=%d boolean=%s pc=%u operation=%s\n",
            (int)capture->status,
            capture->status == MC_WORLD_STORAGE_OK ? dp_bool(capture->returned) : "unpublished",
            capture->error.bytecode,
            capture->error.operation == NULL ? "null" : capture->error.operation);
    capture->chunk_same = held_chunk == f->chunks[capture->side] &&
                          mc_world_store_chunk_object(f->stores[capture->side],
                                                      f->states[capture->side]) == held_chunk;
    if (!capture->chunk_same || !dp_snapshot(capture, f, &capture->after)) {
        fputs("Dungeon placement after-image capture failed\n", stderr);
        goto cleanup;
    }
    capture->map_same = capture->before.fields.l == capture->after.fields.l;
    capture->wrappers_same = capture->before.fields.e == capture->after.fields.e &&
                             capture->before.fields.f == capture->after.fields.f &&
                             capture->before.fields.g == capture->after.fields.g;
    capture->blocks_same = capture->before.fields.b == capture->after.fields.b;
    capture->height_same = capture->before.fields.h == capture->after.fields.h;
    capture->c_same = capture->before.c == capture->after.c;
    capture->G_same = capture->before.G == capture->after.G;
    /* These consume the SAME supplied owner, in the original observer's order. */
    capture->gaussian_bits = dp_double_bits(mc_java_random_next_gaussian(&random));
    if (!mc_java_random_next_bounded(&random, 11, &capture->bounded))
        goto cleanup;
    capture->integer = mc_java_random_next_i32(&random);
    capture->long_bits = (uint64_t)mc_java_random_next_i64(&random);
    capture->double_bits = dp_double_bits(mc_java_random_next_double(&random));
    capture->continuation_random = random;
    captured = true;
cleanup:
    dp_release(capture);
    if (!cleanup_fixture(f)) {
        fputs("Dungeon placement cleanup refused; surviving actual owners retained\n", stderr);
        captured = false;
    }
    if (captured) {
        json.bytes = malloc(DP_JSON_BYTES);
        json.ok = json.bytes != NULL;
        if (json.ok && dp_document(&json, capture) &&
            fwrite(json.bytes, 1u, json.length, stdout) == json.length && fflush(stdout) == 0)
            result = 0;
        else
            fputs("Dungeon placement complete bounded JSON output failed\n", stderr);
    }
    free(json.bytes);
    free(capture);
    return result;
}
