/* Research replay of the selected original getter-only protocol. Build this
 * translation unit alone; the imported fixture main is never executed. The
 * live GAME constructors and production getter/pool are the measured objects.
 * Query/marker context is caller-owned and never passed to the getter. */
#define main mc_block_shape_publication_fixture_main
#include "../test_entity_publication.c"
#undef main

#include "../../../../src/entity/bounds/bounds_internal.h"
#include "../../../../src/world/runtime/collision/blocks_internal.h"

#include <inttypes.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

enum { SHAPE_LINE = 2048, SHAPE_SOURCE_CASES = 672, SHAPE_SELECTED_CASES = 623 };

typedef struct shape_case {
    char *name;
    size_t ordinal;
    int32_t id, x, y, z;
} shape_case;

static bool shape_unsigned(const char *text, uint32_t limit, uint32_t *out) {
    if (text == NULL || *text == '\0')
        return false;
    uint32_t value = 0;
    for (const char *p = text; *p != '\0'; ++p) {
        if (*p < '0' || *p > '9')
            return false;
        const uint32_t digit = (uint32_t)(*p - '0');
        if (value > (limit - digit) / 10u)
            return false;
        value = value * 10u + digit;
    }
    *out = value;
    return true;
}

static bool shape_signed(const char *text, int32_t *out) {
    if (text == NULL || *text == '\0')
        return false;
    const bool negative = text[0] == '-';
    uint32_t magnitude;
    if (!shape_unsigned(text + (negative ? 1 : 0),
                        negative ? UINT32_C(2147483648) : UINT32_C(2147483647), &magnitude))
        return false;
    if (negative && magnitude == UINT32_C(2147483648))
        *out = INT32_MIN;
    else
        *out = negative ? -(int32_t)magnitude : (int32_t)magnitude;
    return true;
}

static bool shape_source_unused_world(int32_t id) {
    /* These seven actual virtual bodies dereference their World. The original
     * prefix supplies null, so its49 failures are excluded from this replay. */
    return id >= 1 && id <= 96 && id != 36 && id != 64 && id != 65 && id != 71 && id != 78 &&
           id != 92 && id != 96;
}

static bool shape_parse(char *line, shape_case *out) {
    char *fields[14];
    fields[0] = line;
    for (size_t i = 1; i < COUNT(fields); ++i) {
        char *const separator = strchr(fields[i - 1u], '\t');
        if (separator == NULL)
            return false;
        *separator = '\0';
        fields[i] = separator + 1;
    }
    if (strchr(fields[13], '\t') != NULL || strlen(fields[0]) != 6 || fields[0][0] != 's' ||
        strcmp(fields[1], "get") != 0 || strcmp(fields[6], "ctor") != 0 ||
        strcmp(fields[7], "0000000000000000,0000000000000000,0000000000000000,"
                          "3ff0000000000000,3ff0000000000000,3ff0000000000000") != 0 ||
        strcmp(fields[8], "marker") != 0 || strcmp(fields[9], "reset") != 0 ||
        strcmp(fields[10], "1") != 0 || strcmp(fields[11], "0") != 0 ||
        strcmp(fields[12], "00000000") != 0 || strcmp(fields[13], "0") != 0)
        return false;
    uint32_t ordinal;
    if (!shape_unsigned(fields[0] + 1, SHAPE_SOURCE_CASES - 1u, &ordinal) ||
        !shape_signed(fields[2], &out->id) || !shape_source_unused_world(out->id) ||
        !shape_signed(fields[3], &out->x) || !shape_signed(fields[4], &out->y) ||
        !shape_signed(fields[5], &out->z) || out->id != (int32_t)(ordinal % 96u) + 1)
        return false;
    out->name = fields[0];
    out->ordinal = (size_t)ordinal;
    return true;
}

static mc_entity_bounds shape_block_bounds(const mc_block *block) {
    const mc_block_fields *const fields = mc_block_fields_get(block);
    return (mc_entity_bounds){fields->bs, fields->bt, fields->bu,
                              fields->bv, fields->bw, fields->bx};
}

static bool shape_coordinates(const mc_entity_bounds *bounds) {
    if (bounds == NULL)
        return fputs("-", stdout) >= 0;
    const double values[] = {bounds->min_x, bounds->min_y, bounds->min_z,
                             bounds->max_x, bounds->max_y, bounds->max_z};
    for (size_t i = 0; i < COUNT(values); ++i) {
        uint64_t bits;
        _Static_assert(sizeof(bits) == sizeof(values[i]), "binary64 observer required");
        memcpy(&bits, &values[i], sizeof(bits));
        if (printf("%s%016" PRIx64, i == 0 ? "" : ",", bits) < 0)
            return false;
    }
    return true;
}

static bool shape_identity(const mc_entity_classes *classes, const mc_entity_bounds *bounds) {
    if (bounds == NULL)
        return fputs("-", stdout) >= 0;
    const mc_entity_bounds_pool *const pool = classes->bounds_pool;
    for (size_t i = 0; i < pool->length; ++i)
        if (pool->elements[i] == bounds)
            return printf("p%zu", i) >= 0;
    return false;
}

static bool shape_pool(const mc_entity_classes *classes) {
    const mc_entity_bounds_pool *const pool = classes->bounds_pool;
    if (pool->length == 0)
        return fputs("_", stdout) >= 0;
    for (size_t i = 0; i < pool->length; ++i)
        if (pool->elements[i] == NULL || printf("%sp%zu:", i == 0 ? "" : ";", i) < 0 ||
            !shape_coordinates(pool->elements[i]))
            return false;
    return true;
}

static bool shape_world(fixture *f, size_t side, const char *root) {
    bool ok = false;
    CHECK(open_store(f, side, root));
    CHECK(mc_world_dimension_create(0, NULL, &f->dimensions[side], NULL) == MC_WORLD_DIMENSION_OK);
    mc_world_runtime_construction_limits limits = mc_world_runtime_default_construction_limits();
    limits.max_provider_chunks = 1;
    limits.max_scheduled_entries = 0;
    CHECK(mc_world_runtime_allocate_world(side == 0 ? MC_WORLD_RUNTIME_CLIENT_LOAD_OR_CREATE
                                                    : MC_WORLD_RUNTIME_SERVER_LOAD_OR_CREATE,
                                          f->classes[side], &limits, &f->worlds[side],
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

static bool shape_replay(fixture *f, size_t side, const shape_case *row,
                         const mc_entity_bounds initial[97]) {
    bool ok = false;
    mc_entity_bounds *result = NULL;
    mc_world_runtime *const world = f->worlds[side];
    mc_entity_classes *const classes = f->entity_classes[side];
    const mc_block *selected = NULL;
    CHECK(mc_registry_block_get(f->registry[side], row->id, &selected, NULL) == MC_REGISTRY_OK &&
          selected != NULL);
    mc_block *const block = (mc_block *)selected;
    const mc_entity_bounds *const constructor = &initial[row->id];
    /* Original protocol explicitly calls the real six-float setter with its
     * captured constructor values before every case. This is input setup,
     * not a detached expected geometry or a replacement collision engine. */
    mc_registry_block_bounds(block, (float)constructor->min_x, (float)constructor->min_y,
                             (float)constructor->min_z, (float)constructor->max_x,
                             (float)constructor->max_y, (float)constructor->max_z);
    mc_entity_bounds query = {0, 0, 0, 1, 1, 1};
    mc_entity_bounds marker = {-9, -8, -7, -6, -5, -4};
    const mc_entity_bounds query_before = query, marker_before = marker;
    void *items[] = {&marker};
    mc_world_reference_list destination = {items, COUNT(items), COUNT(items)};
    CHECK(mc_entity_bounds_pool_reset(classes, NULL) == MC_ENTITY_OK &&
          classes->bounds_pool != NULL);
    const int32_t cursor_before = classes->bounds_cursor;
    const size_t size_before = classes->bounds_pool->length;
    const mc_entity_bounds before = shape_block_bounds(block);
    CHECK(mc_world_runtime_call_enter(world, NULL) == MC_WORLD_STORAGE_OK);
    mc_world_collision_error error = mc_world_collision_empty_error();
    const mc_world_storage_status status =
        mc_world_collision_block_box_held(world, block, row->x, row->y, row->z, &result, &error);
    mc_world_runtime_call_leave(world);
    if (status != MC_WORLD_STORAGE_OK)
        (void)fprintf(stderr, "GETTER_FAILURE\t%s\t%d\t%s\t%u\n", row->name, (int)status,
                      error.block_class == NULL ? "-" : error.block_class, error.block_bytecode);
    CHECK(status == MC_WORLD_STORAGE_OK && classes->bounds_pool != NULL &&
          classes->bounds_pool->length <= 1 && world->active_calls == 0 &&
          world->local_leases == 0 && f->providers[side].cache->count == 0 &&
          mc_world_store_chunk_count(f->stores[side]) == 0 && destination.items == items &&
          destination.count == 1 && destination.capacity == 1 && destination.items[0] == &marker &&
          memcmp(&query, &query_before, sizeof(query)) == 0 &&
          memcmp(&marker, &marker_before, sizeof(marker)) == 0);
    const mc_entity_bounds after = shape_block_bounds(block);
    CHECK(printf("%s\tget\t%" PRId32 "\tOK\t%" PRId32 "\t%" PRId32 "\t%zu\t%zu\t", row->name,
                 row->id, cursor_before, classes->bounds_cursor, size_before,
                 classes->bounds_pool->length) >= 0 &&
          shape_identity(classes, result) && fputc('\t', stdout) != EOF &&
          shape_coordinates(result) && printf("\t%zu\tm:", destination.count) >= 0 &&
          shape_coordinates(destination.items[0]) && fputc('\t', stdout) != EOF &&
          shape_coordinates(&before) && fputc('\t', stdout) != EOF && shape_coordinates(&after) &&
          fputs("\t-\t-\t", stdout) >= 0 && shape_pool(classes) && fputc('\t', stdout) != EOF &&
          shape_identity(classes, result) && fputc('\t', stdout) != EOF &&
          shape_coordinates(result) && fputc('\n', stdout) != EOF);
    ok = true;
cleanup:
    /* The actual factory hold survives through every serialized reference and
     * coordinate. Releasing it leaves the source pool identity/contents alive. */
    if (result != NULL && mc_entity_bounds_release(classes, result) != MC_ENTITY_OK)
        ok = false;
    return ok;
}

int main(int argc, char **argv) {
    int result = 1;
    fixture *f = NULL;
    char root[8192], line[SHAPE_LINE];
    size_t rows = 0, previous = 0;
    bool seen[SHAPE_SOURCE_CASES] = {false};
    size_t counts[97] = {0};
    mc_entity_bounds initial[97] = {0};
    const uint16_t title[] = {'P', 'u', 'b', 'l', 'i', 's', 'h'};
    if (argc != 4 || strcmp(argv[1], "block-getters") != 0 ||
        (strcmp(argv[2], "CLIENT") != 0 && strcmp(argv[2], "SERVER") != 0) ||
        !decode_path(argv[3], root, sizeof(root)) || !mc_math_trig_init(&trig))
        return 1;
    const size_t side = strcmp(argv[2], "CLIENT") == 0 ? 0u : 1u;
    f = calloc(1, sizeof(*f));
    CHECK(f != NULL && mc_world_regions_create(NULL, &f->regions, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_materials_create(&f->materials));
    CHECK(mc_java_random_profile_create(4u * 1024u * 1024u, &f->profile) ==
          MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_world_info_name_copy((mc_nbt_string){title, COUNT(title)}, SIZE_MAX, &f->name) ==
          MC_WORLD_INFO_OK);
    resources *const r = &f->sources[side];
    r->streams[0] = (stream){.bytes = language, .length = sizeof(language) - 1u};
    r->streams[1] = (stream){.bytes = language};
    r->streams[2] = (stream){.bytes = language};
    r->api = (mc_registry_resources){r, open_resource, close_resource, ready_resource};
    const mc_registry_options options = {side == 0 ? MC_REGISTRY_CLIENT : MC_REGISTRY_SERVER,
                                         f->profile, f->materials, 0, &r->api};
    CHECK(mc_registry_classes_allocate(&options, &f->registry[side], NULL) == MC_REGISTRY_OK);
    CHECK(mc_world_runtime_classes_create(side == 0 ? MC_WORLD_INFO_CLIENT : MC_WORLD_INFO_SERVER,
                                          f->profile, &trig, &f->classes[side],
                                          NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_classes_bind_registry(f->classes[side], f->registry[side], NULL) ==
          MC_REGISTRY_OK);
    CHECK(mc_entity_classes_create(side == 0 ? MC_ENTITY_CLIENT : MC_ENTITY_SERVER, f->profile,
                                   NULL, &f->entity_classes[side]) == MC_ENTITY_OK);
    CHECK(mc_world_runtime_classes_bind_entities(f->classes[side], f->entity_classes[side]) ==
          MC_ENTITY_OK);
    CHECK(mc_registry_classes_initialize_blocks(f->registry[side], NULL) == MC_REGISTRY_OK &&
          r->opens == 3 && r->closes == 1 && !r->bad);
    for (int32_t id = 1; id <= 96; ++id) {
        const mc_block *block = NULL;
        CHECK(mc_registry_block_get(f->registry[side], id, &block, NULL) == MC_REGISTRY_OK &&
              block != NULL && block->fields.bn == id);
        initial[id] = shape_block_bounds(block);
    }
    CHECK(shape_world(f, side, root));
    /* The original observer loads the actual empty AABB class before its
     * corpus. Use real reset/initialization, then keep this pool across cases. */
    CHECK(mc_entity_bounds_pool_reset(f->entity_classes[side], NULL) == MC_ENTITY_OK &&
          f->entity_classes[side]->bounds_pool != NULL &&
          f->entity_classes[side]->bounds_pool->length == 0 &&
          f->entity_classes[side]->bounds_cursor == 0);
    /* Keep native protocol framing uniformly LF. The real bootstrap above is
     * preserved with its platform line endings; a replay controller retains
     * both raw streams and parses LF/CRLF framing without changing TSV fields. */
    CHECK(fflush(stdout) == 0);
#ifdef _WIN32
    CHECK(_setmode(_fileno(stdout), _O_BINARY) != -1);
#endif
    CHECK(printf("BLOCK_SHAPES_V1\t%s\n", argv[2]) >= 0);
    while (fgets(line, sizeof(line), stdin) != NULL) {
        const size_t length = strlen(line);
        CHECK(length != 0 && line[length - 1u] == '\n' && rows < SHAPE_SELECTED_CASES);
        line[length - 1u] = '\0';
        if (length > 1 && line[length - 2u] == '\r')
            line[length - 2u] = '\0';
        shape_case row;
        CHECK(shape_parse(line, &row) && !seen[row.ordinal] &&
              (rows == 0 || row.ordinal > previous));
        seen[row.ordinal] = true;
        previous = row.ordinal;
        ++counts[row.id];
        CHECK(shape_replay(f, side, &row, initial));
        ++rows;
    }
    CHECK(!ferror(stdin) && rows == SHAPE_SELECTED_CASES && fflush(stdout) == 0);
    for (int32_t id = 1; id <= 96; ++id)
        CHECK(counts[id] == (shape_source_unused_world(id) ? 7u : 0u));
    CHECK(f->entity_classes[side]->bounds_holds == 0 && f->worlds[side]->active_calls == 0 &&
          f->worlds[side]->local_leases == 0 && f->providers[side].cache->count == 0 &&
          mc_world_store_chunk_count(f->stores[side]) == 0);
    result = 0;
cleanup:
    if (!cleanup_fixture(f)) {
        fputs("block getter probe cleanup refused; surviving owners retained\n", stderr);
        return 1;
    }
    if (result == 0 &&
        (fprintf(stderr, "PASS: block getter replay; side=%s; rows=%zu; checks=%u; cleanup=OK\n",
                 argv[2], rows, checks) < 0 ||
         fflush(stderr) != 0))
        result = 1;
    return result;
}
