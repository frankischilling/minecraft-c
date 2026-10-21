/* Bounded research protocol over the real GAME fixture and production gather.
 * Build this translation unit alone. Limits below bound this observer's input
 * format, never production gameplay loops. No callback/backend is substituted. */
#define main mc_nearby_probe_publication_fixture_main
#include "../test_entity_publication.c"
#undef main

#include "../../../../src/world/chunk/object_state_internal.h"
#include "../../../../src/world/runtime/collision/entities_internal.h"
#include "mc/world/runtime/chunks.h"

enum { PROBE_ENTITIES = 16, PROBE_OCCURRENCES = 64, PROBE_LINE = 8192 };

typedef struct probe_case {
    char *id;
    bool blank, query_null, destination_null;
    int excluded, marker;
    char kinds[PROBE_ENTITIES];
    mc_entity_bounds boxes[PROBE_ENTITIES], query;
    size_t entity_count, counts[8];
    size_t indexes[8][PROBE_OCCURRENCES];
} probe_case;

static char *probe_token(char **cursor, char delimiter) {
    if (*cursor == NULL)
        return NULL;
    char *result = *cursor;
    char *end = strchr(result, delimiter);
    if (end == NULL)
        *cursor = NULL;
    else {
        *end = '\0';
        *cursor = end + 1;
    }
    return result;
}

static bool probe_index(const char *text, size_t limit, size_t *out) {
    if (text == NULL || *text == '\0')
        return false;
    size_t value = 0;
    for (const char *p = text; *p != '\0'; ++p) {
        if (*p < '0' || *p > '9' || value > (SIZE_MAX - 9u) / 10u)
            return false;
        value = value * 10u + (size_t)(*p - '0');
    }
    if (value >= limit)
        return false;
    *out = value;
    return true;
}

static bool probe_number(const char *text, double *out) {
    if (text == NULL || strlen(text) != 16)
        return false;
    uint64_t value = 0;
    for (size_t i = 0; i < 16; ++i) {
        const unsigned char c = (unsigned char)text[i];
        const unsigned int digit = c >= '0' && c <= '9'   ? (unsigned int)(c - '0')
                                   : c >= 'a' && c <= 'f' ? (unsigned int)(c - 'a') + 10u
                                                          : 16u;
        if (digit > 15u)
            return false;
        value = (value << 4u) | digit;
    }
    _Static_assert(sizeof(value) == sizeof(*out), "binary64 observer required");
    memcpy(out, &value, sizeof(value));
    return true;
}

static bool probe_box(char *text, mc_entity_bounds *out) {
    double values[6];
    char *cursor = text;
    for (size_t i = 0; i < 6; ++i)
        if (!probe_number(probe_token(&cursor, ','), &values[i]))
            return false;
    if (cursor != NULL)
        return false;
    *out = (mc_entity_bounds){values[0], values[1], values[2], values[3], values[4], values[5]};
    return true;
}

static bool probe_same_box(const mc_entity_bounds *a, const mc_entity_bounds *b) {
    const double av[] = {a->min_x, a->min_y, a->min_z, a->max_x, a->max_y, a->max_z};
    const double bv[] = {b->min_x, b->min_y, b->min_z, b->max_x, b->max_y, b->max_z};
    return memcmp(av, bv, sizeof(av)) == 0;
}

static bool probe_parse(char *line, probe_case *out) {
    char *fields[7], *cursor = line;
    for (size_t i = 0; i < COUNT(fields); ++i)
        if ((fields[i] = probe_token(&cursor, '\t')) == NULL)
            return false;
    if (cursor != NULL || fields[0][0] == '\0' ||
        (strcmp(fields[1], "N") != 0 && strcmp(fields[1], "B") != 0))
        return false;
    *out =
        (probe_case){.id = fields[0], .blank = fields[1][0] == 'B', .excluded = -1, .marker = -1};
    out->query_null = strcmp(fields[4], "-") == 0;
    if (!out->query_null && !probe_box(fields[4], &out->query))
        return false;
    if (strcmp(fields[5], "_") != 0) {
        cursor = fields[5];
        char *spec;
        while ((spec = probe_token(&cursor, ';')) != NULL) {
            if (out->entity_count == PROBE_ENTITIES || strlen(spec) < 3 || spec[1] != ':' ||
                (spec[0] != 'I' && spec[0] != 'F'))
                return false;
            out->kinds[out->entity_count] = spec[0];
            if (!probe_box(spec + 2, &out->boxes[out->entity_count]))
                return false;
            ++out->entity_count;
        }
    }
    size_t index;
    if (strcmp(fields[2], "-") != 0) {
        if (!probe_index(fields[2], out->entity_count, &index))
            return false;
        out->excluded = (int)index;
    }
    out->destination_null = strcmp(fields[3], "-") == 0;
    if (!out->destination_null && strcmp(fields[3], "_") != 0) {
        if (!probe_index(fields[3], out->entity_count, &index))
            return false;
        out->marker = (int)index;
    }
    /* Actual native Blank has no normal header: none of m is reached.
     * Malformed normal arrays/entries are excluded explicitly by the driver. */
    if (out->blank)
        return true;
    cursor = fields[6];
    for (size_t slice = 0; slice < 8; ++slice) {
        char *list = probe_token(&cursor, '|');
        if (list == NULL)
            return false;
        if (strcmp(list, "_") == 0)
            continue;
        char *entries = list, *entry;
        while ((entry = probe_token(&entries, ',')) != NULL) {
            if (out->counts[slice] == PROBE_OCCURRENCES ||
                !probe_index(entry, out->entity_count, &index))
                return false;
            out->indexes[slice][out->counts[slice]++] = index;
        }
    }
    return cursor == NULL;
}

static bool probe_world(fixture *f, size_t side, const char *root) {
    bool ok = false;
    CHECK(open_store(f, side, root));
    CHECK(mc_world_dimension_create(0, NULL, &f->dimensions[side], NULL) == MC_WORLD_DIMENSION_OK);
    CHECK(mc_world_runtime_allocate_world(side == 0 ? MC_WORLD_RUNTIME_CLIENT_LOAD_OR_CREATE
                                                    : MC_WORLD_RUNTIME_SERVER_LOAD_OR_CREATE,
                                          f->classes[side], NULL, &f->worlds[side],
                                          NULL) == MC_WORLD_STORAGE_OK);
    const mc_world_runtime_world_arguments arguments = {
        .store = f->stores[side], .name = f->name, .seed = -99, .dimension = f->dimensions[side]};
    CHECK(mc_world_runtime_construct_world(f->worlds[side], &arguments, NULL) ==
          MC_WORLD_STORAGE_OK);
    CHECK(mc_world_provider_bind_fields(f->worlds[side]->fields.v, &f->providers[side], NULL) ==
          MC_WORLD_STORAGE_OK);
    f->bound[side] = true;
    CHECK(f->worlds[side]->fallback.blank && f->providers[side].cache->count == 0 &&
          mc_world_store_chunk_count(f->stores[side]) == 0);
    ok = true;
cleanup:
    return ok;
}

static bool probe_new_chunk(fixture *f, size_t side) {
    bool ok = false;
    uint8_t *blocks = calloc(MC_CHUNK_BLOCKS, 1);
    CHECK(blocks != NULL && f->states[side] == NULL &&
          mc_world_runtime_chunk_construct(f->worlds[side], 7, -9, &blocks, MC_CHUNK_BLOCKS,
                                           &f->chunks[side], NULL) == MC_WORLD_STORAGE_OK &&
          blocks == NULL);
    f->states[side] = mc_world_chunk_state(f->chunks[side]);
    CHECK(f->states[side] != NULL && !f->states[side]->active &&
          mc_world_store_chunk_count(f->stores[side]) == 1);
    ok = true;
cleanup:
    free(blocks);
    return ok;
}

static bool probe_retire_chunk(fixture *f, size_t side) {
    if (f->states[side] == NULL)
        return true;
    if (mc_world_store_retire_chunk(f->stores[side], f->states[side], NULL) != MC_WORLD_STORAGE_OK)
        return false;
    f->states[side] = NULL;
    f->chunks[side] = NULL;
    return mc_world_store_chunk_count(f->stores[side]) == 0;
}

static mc_world_storage_status probe_gather(mc_world_runtime *world, mc_chunk_state *state,
                                            mc_entity *excluded, const mc_entity_bounds *query,
                                            mc_world_reference_list *destination,
                                            mc_world_nearby_error *error) {
    const mc_world_storage_status entered = mc_world_runtime_call_enter(world, NULL);
    if (entered != MC_WORLD_STORAGE_OK)
        return entered;
    const mc_world_storage_status result =
        mc_world_nearby_gather_held(world, state, excluded, query, destination, error);
    mc_world_runtime_call_leave(world);
    return result;
}

static bool probe_clear(mc_world_runtime *world) {
    /* Inverted X bounds produce the ordinary empty source loop, after actual
     * R.clear. This is not terminal container retirement or a hidden list reset. */
    const mc_entity_bounds empty = {64, 0, 0, -64, 1, 1};
    mc_world_entity_list_view view = {0};
    size_t count = SIZE_MAX;
    const mc_world_storage_status status =
        mc_world_runtime_nearby_entities(world, NULL, &empty, &view, NULL);
    const bool ok = status == MC_WORLD_STORAGE_OK &&
                    mc_world_entity_list_view_count(&view, &count) == MC_WORLD_STORAGE_OK &&
                    count == 0;
    mc_world_entity_list_view_release(&view);
    return ok;
}

static bool probe_unchanged(fixture *f, size_t side, const probe_case *row,
                            const mc_chunk_state *blank_before, bool dirty_before) {
    for (size_t i = 0; i < row->entity_count; ++i)
        if (!probe_same_box(f->entities[i]->bounds, &row->boxes[i]))
            return false;
    if (row->blank)
        return memcmp(&f->worlds[side]->fallback, blank_before, sizeof(*blank_before)) == 0;
    mc_world_chunk_fields fields = {0};
    if (mc_world_chunk_fields_get(f->chunks[side], &fields) != MC_WORLD_STORAGE_OK ||
        fields.q != dirty_before || fields.m != &f->chunks[side]->slices)
        return false;
    for (size_t slice = 0; slice < 8; ++slice) {
        const mc_world_chunk_entity_list *list = NULL;
        size_t count = SIZE_MAX;
        if (mc_world_chunk_slice_at(fields.m, slice, &list) != MC_WORLD_STORAGE_OK ||
            mc_world_chunk_entity_count(list, &count) != MC_WORLD_STORAGE_OK ||
            count != row->counts[slice])
            return false;
        for (size_t index = 0; index < count; ++index) {
            mc_entity *entity = NULL;
            if (mc_world_chunk_entity_at(list, index, &entity) != MC_WORLD_STORAGE_OK ||
                entity != f->entities[row->indexes[slice][index]])
                return false;
        }
    }
    return true;
}

static bool probe_run_case(fixture *f, size_t side, const probe_case *row) {
    bool ok = false;
    mc_world_runtime *world = f->worlds[side];
    CHECK(f->entity_count == 0 && world->fields.R->count == 0 && f->states[side] == NULL);
    for (size_t i = 0; i < row->entity_count; ++i) {
        mc_entity *entity = NULL;
        const mc_entity_status allocated =
            row->kinds[i] == 'I'
                ? mc_item_entity_allocate(f->entity_classes[side], &entity, NULL)
                : mc_falling_entity_allocate(f->entity_classes[side], &entity, NULL);
        CHECK(allocated == MC_ENTITY_OK && entity != NULL);
        f->entities[f->entity_count++] = entity;
        const mc_entity_status constructed =
            row->kinds[i] == 'I' ? mc_item_entity_construct_world(entity, NULL, NULL)
                                 : mc_falling_entity_construct_world(entity, NULL, NULL);
        CHECK(constructed == MC_ENTITY_OK && entity->bounds != NULL);
    }
    if (row->marker >= 0) {
        CHECK(probe_new_chunk(f, side));
        mc_entity *marker = f->entities[(size_t)row->marker];
        CHECK(mc_entity_set_position(marker, 112.5, 0.5, -143.5) == MC_ENTITY_OK &&
              mc_world_chunk_insert_entity(f->chunks[side], marker, NULL) == MC_WORLD_STORAGE_OK);
        const mc_entity_bounds unit = {0, 0, 0, 1, 1, 1};
        *marker->bounds = unit;
        mc_world_nearby_error seed_error = mc_world_nearby_empty_error();
        CHECK(probe_gather(world, f->states[side], NULL, &unit, world->fields.R, &seed_error) ==
                  MC_WORLD_STORAGE_OK &&
              world->fields.R->count == 1 && world->fields.R->items[0] == marker &&
              probe_retire_chunk(f, side));
    }
    if (!row->blank) {
        CHECK(probe_new_chunk(f, side));
        for (size_t slice = 0; slice < 8; ++slice)
            for (size_t index = 0; index < row->counts[slice]; ++index) {
                mc_entity *entity = f->entities[row->indexes[slice][index]];
                CHECK(mc_entity_set_position(entity, 112.5, (double)slice * 16.0 + 0.5, -143.5) ==
                          MC_ENTITY_OK &&
                      mc_world_chunk_insert_entity(f->chunks[side], entity, NULL) ==
                          MC_WORLD_STORAGE_OK);
            }
    }
    /* Explicit research input setup, same six original bounds cells as the
     * reflection observer. Never replace the actual permanent bounds object. */
    for (size_t i = 0; i < row->entity_count; ++i)
        *f->entities[i]->bounds = row->boxes[i];
    const mc_chunk_state blank_before = world->fallback;
    const bool dirty_before = !row->blank && f->states[side]->has_entities;
    mc_world_reference_list *destination = row->destination_null ? NULL : world->fields.R;
    const int before = destination == NULL ? -1 : (int)destination->count;
    mc_world_nearby_error error = mc_world_nearby_empty_error();
    const mc_world_storage_status status =
        probe_gather(world, row->blank ? &world->fallback : f->states[side],
                     row->excluded < 0 ? NULL : f->entities[(size_t)row->excluded],
                     row->query_null ? NULL : &row->query, destination, &error);
    const bool unchanged = probe_unchanged(f, side, row, &blank_before, dirty_before);
    CHECK(mc_world_nearby_owned_valid(world));
    CHECK(printf("%s\t%c\t%s\t%d\t%d\t", row->id, row->blank ? 'B' : 'N',
                 status == MC_WORLD_STORAGE_OK ? "OK" : "FAIL", before,
                 destination == NULL ? -1 : (int)destination->count) >= 0);
    if (destination == NULL || destination->count == 0)
        CHECK(fputs(destination == NULL ? "-" : "_", stdout) >= 0);
    else
        for (size_t index = 0; index < destination->count; ++index) {
            size_t identity = 0;
            while (identity < row->entity_count &&
                   destination->items[index] != f->entities[identity])
                ++identity;
            CHECK(identity < row->entity_count &&
                  printf("%s%zu", index == 0 ? "" : ",", identity) >= 0);
        }
    CHECK(printf("\t%d\t%d\t%u\n", unchanged ? 1 : 0, (int)status, error.query.chunk_bytecode) >=
              0 &&
          unchanged);
    CHECK(probe_clear(world) && probe_retire_chunk(f, side));
    for (size_t i = 0; i < f->entity_count; ++i) {
        CHECK(f->entities[i]->holds == 1);
        mc_entity_release(f->entities[i]);
        f->entities[i] = NULL;
    }
    f->entity_count = 0;
    CHECK(world->active_calls == 0 && world->local_leases == 0 && world->fields.R->count == 0 &&
          f->providers[side].cache->count == 0);
    ok = true;
cleanup:
    return ok;
}

int main(int argc, char **argv) {
    int result = 1;
    fixture *f = NULL;
    char root[8192], line[PROBE_LINE];
    size_t rows = 0;
    const uint16_t title[] = {'P', 'u', 'b', 'l', 'i', 's', 'h'};
    if (argc != 4 || strcmp(argv[1], "nearby-gather") != 0 ||
        (strcmp(argv[2], "CLIENT") != 0 && strcmp(argv[2], "SERVER") != 0) ||
        !decode_path(argv[3], root, sizeof(root)) || !mc_math_trig_init(&trig))
        return 1;
    const size_t selected = strcmp(argv[2], "CLIENT") == 0 ? 0u : 1u;
    f = calloc(1, sizeof(*f));
    CHECK(f != NULL && mc_world_regions_create(NULL, &f->regions, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_materials_create(&f->materials));
    CHECK(mc_java_random_profile_create(4u * 1024u * 1024u, &f->profile) ==
          MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_world_info_name_copy((mc_nbt_string){title, COUNT(title)}, SIZE_MAX, &f->name) ==
          MC_WORLD_INFO_OK);
    for (size_t side = 0; side < 2; ++side) {
        resources *r = &f->sources[side];
        r->streams[0] = (stream){.bytes = language, .length = sizeof(language) - 1u};
        r->streams[1] = (stream){.bytes = language};
        r->streams[2] = (stream){.bytes = language};
        r->api = (mc_registry_resources){r, open_resource, close_resource, ready_resource};
        const mc_registry_options options = {side == 0 ? MC_REGISTRY_CLIENT : MC_REGISTRY_SERVER,
                                             f->profile, f->materials, 0, &r->api};
        CHECK(mc_registry_classes_allocate(&options, &f->registry[side], NULL) == MC_REGISTRY_OK);
        CHECK(mc_world_runtime_classes_create(
                  side == 0 ? MC_WORLD_INFO_CLIENT : MC_WORLD_INFO_SERVER, f->profile, &trig,
                  &f->classes[side], NULL) == MC_WORLD_STORAGE_OK);
        CHECK(mc_world_runtime_classes_bind_registry(f->classes[side], f->registry[side], NULL) ==
              MC_REGISTRY_OK);
        CHECK(mc_entity_classes_create(side == 0 ? MC_ENTITY_CLIENT : MC_ENTITY_SERVER, f->profile,
                                       NULL, &f->entity_classes[side]) == MC_ENTITY_OK);
        CHECK(mc_world_runtime_classes_bind_entities(f->classes[side], f->entity_classes[side]) ==
              MC_ENTITY_OK);
        CHECK(mc_registry_classes_initialize_blocks(f->registry[side], NULL) == MC_REGISTRY_OK &&
              r->opens == 3 && r->closes == 1 && !r->bad);
    }
    CHECK(probe_world(f, selected, root));
    while (fgets(line, sizeof(line), stdin) != NULL) {
        const size_t length = strlen(line);
        CHECK(length != 0 && line[length - 1u] == '\n');
        line[length - 1u] = '\0';
        if (length > 1 && line[length - 2u] == '\r')
            line[length - 2u] = '\0';
        probe_case row;
        CHECK(probe_parse(line, &row) && probe_run_case(f, selected, &row));
        ++rows;
    }
    CHECK(!ferror(stdin) && rows != 0 && fflush(stdout) == 0);
    result = 0;
cleanup:
    if (f != NULL && f->worlds[selected] != NULL && !probe_clear(f->worlds[selected]))
        result = 1;
    if (!cleanup_fixture(f)) {
        fputs("nearby probe cleanup refused; surviving owners retained\n", stderr);
        return 1;
    }
    if (result == 0 &&
        fprintf(stderr, "PASS: %u nearby gather replay checks; %zu rows\n", checks, rows) < 0)
        result = 1;
    return result;
}
