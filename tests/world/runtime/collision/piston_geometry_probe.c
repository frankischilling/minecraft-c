/* Replay the actual direct shifted helper using genuine GAME owners. This
 * translation unit imports only the real constructor/cleanup fixture; its
 * renamed publication main is never called. No substitute World is used. */
#define main mc_piston_geometry_publication_fixture_main
#include "../test_entity_publication.c"
#undef main

#include "../../../../src/block/registry/runtime_internal.h"
#include "../../../../src/entity/bounds/bounds_internal.h"
#include "../../../../src/world/runtime/collision/blocks_internal.h"

#include <inttypes.h>
#include <stdarg.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

enum { PG_SOURCE_ROWS = 11026, PG_ROWS = 11001, PG_LINE = 2048, PG_TEXT = 2048 };

typedef struct pg_case {
    char *name, *bounds, *mode;
    size_t ordinal;
    int32_t carried, x, y, z, facing;
    uint32_t progress;
    unsigned int repeats;
} pg_case;

static bool pg_unsigned(const char *text, uint32_t limit, uint32_t *out) {
    if (text == NULL || *text == '\0')
        return false;
    uint32_t value = 0;
    for (const char *p = text; *p != '\0'; ++p) {
        if (*p < '0' || *p > '9')
            return false;
        const uint32_t digit = (uint32_t)(*p - '0');
        if (digit > limit || value > (limit - digit) / 10u)
            return false;
        value = value * 10u + digit;
    }
    *out = value;
    return true;
}

static bool pg_signed(const char *text, int32_t *out) {
    if (text == NULL || *text == '\0')
        return false;
    const bool negative = text[0] == '-';
    uint32_t magnitude;
    if (!pg_unsigned(text + (negative ? 1 : 0),
                     negative ? UINT32_C(2147483648) : UINT32_C(2147483647), &magnitude))
        return false;
    *out = negative && magnitude == UINT32_C(2147483648) ? INT32_MIN
           : negative                                    ? -(int32_t)magnitude
                                                         : (int32_t)magnitude;
    return true;
}

static bool pg_hex32(const char *text, uint32_t *out) {
    if (strlen(text) != 8)
        return false;
    uint32_t value = 0;
    for (size_t i = 0; i < 8; ++i) {
        const char c = text[i];
        const unsigned int digit = c >= '0' && c <= '9'   ? (unsigned int)(c - '0')
                                   : c >= 'a' && c <= 'f' ? (unsigned int)(c - 'a') + 10u
                                                          : 16u;
        if (digit == 16u)
            return false;
        value = (value << 4u) | digit;
    }
    *out = value;
    return true;
}

static bool pg_world_dependent(int32_t id) {
    return id == 64 || id == 65 || id == 71 || id == 78 || id == 92 || id == 96;
}

static bool pg_parse(char *line, pg_case *out) {
    char *fields[11];
    fields[0] = line;
    for (size_t i = 1; i < COUNT(fields); ++i) {
        char *const separator = strchr(fields[i - 1u], '\t');
        if (separator == NULL)
            return false;
        *separator = '\0';
        fields[i] = separator + 1;
    }
    uint32_t ordinal, repeats;
    if (strchr(fields[10], '\t') != NULL || strlen(fields[0]) != 7 || fields[0][0] != 'g' ||
        !pg_unsigned(fields[0] + 1, PG_SOURCE_ROWS - 1u, &ordinal) ||
        strcmp(fields[1], "moving") != 0 || !pg_signed(fields[2], &out->carried) ||
        pg_world_dependent(out->carried) || !pg_signed(fields[3], &out->x) ||
        !pg_signed(fields[4], &out->y) || !pg_signed(fields[5], &out->z) ||
        !pg_hex32(fields[7], &out->progress) || !pg_signed(fields[8], &out->facing) ||
        !pg_unsigned(fields[10], 2, &repeats) || repeats == 0 ||
        (strcmp(fields[9], "reset") != 0 && strcmp(fields[9], "used") != 0 &&
         strcmp(fields[9], "alias") != 0 && strcmp(fields[9], "between") != 0))
        return false;
    out->name = fields[0];
    out->ordinal = ordinal;
    out->bounds = fields[6];
    out->mode = fields[9];
    out->repeats = repeats;
    return true;
}

static mc_entity_bounds pg_block_bounds(const mc_block *block) {
    const mc_block_fields *const f = mc_block_fields_get(block);
    return (mc_entity_bounds){f->bs, f->bt, f->bu, f->bv, f->bw, f->bx};
}

static bool pg_append(char text[PG_TEXT], size_t *length, const char *format, ...) {
    va_list arguments;
    va_start(arguments, format);
    const int n = vsnprintf(text + *length, PG_TEXT - *length, format, arguments);
    va_end(arguments);
    if (n < 0 || (size_t)n >= PG_TEXT - *length)
        return false;
    *length += (size_t)n;
    return true;
}

static bool pg_coordinates_text(char text[PG_TEXT], size_t *length,
                                const mc_entity_bounds *bounds) {
    if (bounds == NULL)
        return pg_append(text, length, "-");
    const double values[] = {bounds->min_x, bounds->min_y, bounds->min_z,
                             bounds->max_x, bounds->max_y, bounds->max_z};
    for (size_t i = 0; i < COUNT(values); ++i) {
        uint64_t raw;
        _Static_assert(sizeof(raw) == sizeof(values[i]), "binary64 required");
        memcpy(&raw, &values[i], sizeof(raw));
        if (!pg_append(text, length, "%s%016" PRIx64, i == 0 ? "" : ",", raw))
            return false;
    }
    return true;
}

static bool pg_coordinates(const mc_entity_bounds *bounds) {
    char text[PG_TEXT] = {0};
    size_t length = 0;
    return pg_coordinates_text(text, &length, bounds) && fputs(text, stdout) >= 0;
}

static bool pg_identity(const mc_entity_classes *classes, const mc_entity_bounds *bounds) {
    if (bounds == NULL)
        return fputs("-", stdout) >= 0;
    for (size_t i = 0; i < classes->bounds_pool->length; ++i)
        if (classes->bounds_pool->elements[i] == bounds)
            return printf("p%zu", i) >= 0;
    return false;
}

static bool pg_pool(char text[PG_TEXT], const mc_entity_classes *classes) {
    size_t length = 0;
    const mc_entity_bounds_pool *const pool = classes->bounds_pool;
    if (pool->length == 0)
        return pg_append(text, &length, "_");
    for (size_t i = 0; i < pool->length; ++i)
        if (pool->elements[i] == NULL ||
            !pg_append(text, &length, "%sp%zu:", i == 0 ? "" : ";", i) ||
            !pg_coordinates_text(text, &length, pool->elements[i]))
            return false;
    return true;
}

static bool pg_directions(mc_registry_classes *registry, bool reached,
                          const int32_t *identities[4]) {
    if (!reached)
        return fputs("unobserved", stdout) >= 0;
    for (size_t i = 0; i < 4; ++i) {
        mc_registry_direction_view view;
        if (mc_registry_direction_get(registry, (mc_registry_direction_field)i, &view, NULL) !=
                MC_REGISTRY_OK ||
            view.identity == NULL || view.length != 6)
            return false;
        if (identities[i] == NULL)
            identities[i] = view.identity;
        if (identities[i] != view.identity)
            return false;
        for (size_t j = 0; j < i; ++j)
            if (identities[j] == identities[i])
                return false;
        if (printf("%s%c=d%zu:", i == 0 ? "" : ";", (char)('a' + (int)i), i) < 0)
            return false;
        for (int32_t j = 0; j < 6; ++j) {
            int32_t value;
            if (mc_registry_direction_at(&view, j, &value, NULL) != MC_REGISTRY_OK ||
                printf("%s%" PRId32, j == 0 ? "" : ",", value) < 0)
                return false;
        }
    }
    return true;
}

static bool pg_setter(mc_block *block, const mc_entity_bounds *initial, char *specification,
                      char reread[PG_TEXT]) {
    if (block == NULL) {
        if (strcmp(specification, "-") != 0)
            return false;
        memcpy(reread, "-", sizeof("-"));
        return true;
    }
    if (strcmp(specification, "keep") == 0) {
        memcpy(reread, "keep", sizeof("keep"));
        return true;
    }
    float values[6];
    if (strcmp(specification, "ctor") == 0) {
        const double coordinates[] = {initial->min_x, initial->min_y, initial->min_z,
                                      initial->max_x, initial->max_y, initial->max_z};
        for (size_t i = 0; i < 6; ++i)
            values[i] = (float)coordinates[i];
    } else {
        char *token = specification;
        for (size_t i = 0; i < 6; ++i) {
            char *const next = strchr(token, ',');
            if ((i < 5) != (next != NULL))
                return false;
            if (next != NULL)
                *next = '\0';
            uint32_t bits;
            if (!pg_hex32(token, &bits))
                return false;
            memcpy(&values[i], &bits, sizeof(bits));
            token = next == NULL ? NULL : next + 1;
        }
    }
    size_t length = 0;
    for (size_t i = 0; i < 6; ++i) {
        uint32_t bits;
        _Static_assert(sizeof(bits) == sizeof(values[i]), "binary32 required");
        memcpy(&bits, &values[i], sizeof(bits));
        if (!pg_append(reread, &length, "%s%08" PRIx32, i == 0 ? "" : ",", bits))
            return false;
    }
    mc_registry_block_bounds(block, values[0], values[1], values[2], values[3], values[4],
                             values[5]);
    return true;
}

static bool pg_world(fixture *f, size_t side, const char *root) {
    bool ok = false;
    CHECK(open_store(f, side, root));
    CHECK(mc_world_provider_create(0, NULL, &f->dimensions[side], NULL) == MC_WORLD_PROVIDER_OK);
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
    CHECK(mc_chunk_provider_bind_fields(f->worlds[side]->fields.v, &f->providers[side], NULL) ==
          MC_WORLD_STORAGE_OK);
    f->bound[side] = true;
    CHECK(f->worlds[side]->fallback.blank && f->providers[side].cache->count == 0 &&
          mc_world_store_chunk_count(f->stores[side]) == 0);
    ok = true;
cleanup:
    return ok;
}

static bool pg_replay(fixture *f, size_t side, pg_case *row, const mc_block *moving,
                      const mc_entity_bounds initial[97], bool *direction_reached,
                      const int32_t *direction_identities[4], size_t *failure_count) {
    bool ok = false;
    mc_entity_bounds *anchor = NULL, *held[2] = {NULL, NULL};
    mc_entity_bounds *result = NULL, *first = NULL;
    mc_world_runtime *const world = f->worlds[side];
    mc_entity_classes *const classes = f->entity_classes[side];
    const mc_entity_bounds_pool *const pool_identity = classes->bounds_pool;
    const mc_block *selected = NULL;
    if (row->carried > 0 && row->carried <= 96)
        CHECK(mc_registry_block_get(f->registry[side], row->carried, &selected, NULL) ==
                  MC_REGISTRY_OK &&
              selected != NULL);
    char setter_inputs[PG_TEXT] = {0};
    CHECK(pg_setter((mc_block *)selected, selected == NULL ? NULL : &initial[row->carried],
                    row->bounds, setter_inputs));
    mc_entity_bounds before = {0}, after = {0};
    if (selected != NULL)
        before = pg_block_bounds(selected);
    CHECK(mc_entity_bounds_pool_reset(classes, NULL) == MC_ENTITY_OK);
    if (strcmp(row->mode, "used") == 0 || strcmp(row->mode, "alias") == 0) {
        CHECK(mc_entity_bounds_pool_get(classes, 101, 102, 103, 104, 105, 106, &anchor, NULL) ==
              MC_ENTITY_OK);
        if (strcmp(row->mode, "alias") == 0)
            CHECK(mc_entity_bounds_pool_reset(classes, NULL) == MC_ENTITY_OK);
    }
    const int32_t cursor_before = classes->bounds_cursor;
    const size_t size_before = classes->bounds_pool->length;
    char pool_before[PG_TEXT] = {0}, pool_after[PG_TEXT] = {0};
    CHECK(pg_pool(pool_before, classes));
    float progress;
    memcpy(&progress, &row->progress, sizeof(progress));
    uint32_t progress_reread;
    memcpy(&progress_reread, &progress, sizeof(progress_reread));
    CHECK(progress_reread == row->progress);
    unsigned int completed = 0;
    mc_world_storage_status status = MC_WORLD_STORAGE_OK;
    mc_world_collision_error error = mc_world_collision_empty_error();
    for (unsigned int call = 0; call < row->repeats; ++call) {
        if (call != 0 && strcmp(row->mode, "between") == 0)
            CHECK(mc_entity_bounds_pool_reset(classes, NULL) == MC_ENTITY_OK);
        mc_entity_bounds untouched = {-7, -6, -5, -4, -3, -2};
        mc_entity_bounds *candidate = &untouched;
        CHECK(mc_world_runtime_call_enter(world, NULL) == MC_WORLD_STORAGE_OK);
        error = mc_world_collision_empty_error();
        status = mc_world_collision_piston_shifted_box_held(world, moving, row->x, row->y, row->z,
                                                            row->carried, progress, row->facing,
                                                            &candidate, &error);
        mc_world_runtime_call_leave(world);
        if (status != MC_WORLD_STORAGE_OK) {
            CHECK(candidate == &untouched);
            break;
        }
        held[call] = candidate;
        result = candidate;
        if (call == 0)
            first = candidate;
        ++completed;
        if (candidate != NULL)
            *direction_reached = true;
    }
    const bool array_failure = row->carried < 0 || row->carried >= 256;
    const bool null_carrier = row->carried >= 97 && row->carried < 256;
    /* Only cube1's invalid-facing rows reach a nonnull carried Block in this corpus.
     * ID0/self and genuine null shapes complete before Direction indexing. */
    const bool facing_failure = row->carried == 1 && (row->facing < 0 || row->facing >= 6);
    const bool expected_failure = array_failure || null_carrier || facing_failure;
    CHECK(expected_failure == (status != MC_WORLD_STORAGE_OK));
    if (expected_failure) {
        const unsigned int pc = array_failure ? 21u : null_carrier ? 27u : 50u;
        const mc_registry_status registry_status =
            null_carrier ? MC_REGISTRY_OK : MC_REGISTRY_INDEX_OUT_OF_RANGE;
        CHECK(status == MC_WORLD_STORAGE_UNSUPPORTED_STATE && error.shifted_bytecode == pc &&
              error.block_bytecode == pc && error.block_class != NULL &&
              strcmp(error.block_class, side == 0 ? "ut" : "mz") == 0 &&
              error.nearby.query.registry.status == registry_status && completed == 0);
        if (facing_failure)
            *direction_reached = true;
        CHECK(fprintf(
                  stderr, "PREFIX\t%s\t%d\t%u\t%s\t%u\t%d\t%s\t%s\n", row->name, (int)status,
                  error.shifted_bytecode, error.block_class, error.block_bytecode,
                  (int)error.nearby.query.registry.status,
                  error.nearby.query.registry.message == NULL ? "-"
                                                              : error.nearby.query.registry.message,
                  error.nearby.query.operation == NULL ? "-" : error.nearby.query.operation) >= 0);
        ++*failure_count;
    }
    CHECK(pool_identity == classes->bounds_pool && classes->bounds_pool->length <= 3 &&
          world->active_calls == 0 && world->local_leases == 0 &&
          f->providers[side].cache->count == 0 && mc_world_store_chunk_count(f->stores[side]) == 0);
    if (selected != NULL)
        after = pg_block_bounds(selected);
    CHECK(pg_pool(pool_after, classes));
    CHECK(printf("%s\t%s\t%s\t%08" PRIx32 "\t%" PRId32 "\t%" PRId32 "\t%zu\t%zu\t", row->name,
                 expected_failure ? "NATIVE_FAILURE" : "OK",
                 expected_failure ? "native-body-prefix" : "none", progress_reread, cursor_before,
                 classes->bounds_cursor, size_before, classes->bounds_pool->length) >= 0 &&
          pg_identity(classes, result) && fputc('\t', stdout) != EOF && pg_coordinates(result) &&
          fputc('\t', stdout) != EOF && pg_coordinates(selected == NULL ? NULL : &before) &&
          fputc('\t', stdout) != EOF && pg_coordinates(selected == NULL ? NULL : &after) &&
          fputc('\t', stdout) != EOF && pg_identity(classes, anchor) &&
          fputc('\t', stdout) != EOF && pg_coordinates(anchor) &&
          printf("\t%s\t%s\t", pool_before, pool_after) >= 0 && pg_identity(classes, first) &&
          fputc('\t', stdout) != EOF && pg_coordinates(first) && fputc('\t', stdout) != EOF &&
          pg_directions(f->registry[side], *direction_reached, direction_identities) &&
          printf("\t%s\t%u\tb36\tg0,stable\n", setter_inputs, completed) >= 0);
    ok = true;
cleanup:
    for (size_t i = 0; i < COUNT(held); ++i)
        if (held[i] != NULL && mc_entity_bounds_release(classes, held[i]) != MC_ENTITY_OK)
            ok = false;
    if (anchor != NULL && mc_entity_bounds_release(classes, anchor) != MC_ENTITY_OK)
        ok = false;
    if (classes->bounds_holds != 0)
        ok = false;
    return ok;
}

int main(int argc, char **argv) {
    int result = 1;
    fixture *f = NULL;
    char root[8192], line[PG_LINE];
    size_t rows = 0, previous = 0, failures = 0;
    bool seen[PG_SOURCE_ROWS] = {false}, direction_reached = false;
    const int32_t *direction_identities[4] = {NULL, NULL, NULL, NULL};
    mc_entity_bounds initial[97] = {0};
    const uint16_t title[] = {'P', 'u', 'b', 'l', 'i', 's', 'h'};
    if (argc != 4 || strcmp(argv[1], "piston-geometry") != 0 ||
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
        initial[id] = pg_block_bounds(block);
    }
    const mc_block *moving = NULL;
    CHECK(mc_registry_block_get(f->registry[side], 36, &moving, NULL) == MC_REGISTRY_OK &&
          moving != NULL);
    CHECK(pg_world(f, side, root));
    CHECK(mc_entity_bounds_pool_reset(f->entity_classes[side], NULL) == MC_ENTITY_OK &&
          f->entity_classes[side]->bounds_pool != NULL &&
          f->entity_classes[side]->bounds_pool->length == 0 &&
          f->entity_classes[side]->bounds_cursor == 0);
    CHECK(fflush(stdout) == 0);
#ifdef _WIN32
    CHECK(_setmode(_fileno(stdout), _O_BINARY) != -1);
#endif
    CHECK(printf("PISTON_GEOMETRY_V1\t%s\n", argv[2]) >= 0);
    while (fgets(line, sizeof(line), stdin) != NULL) {
        const size_t length = strlen(line);
        CHECK(length != 0 && line[length - 1u] == '\n' && rows < PG_ROWS);
        line[length - 1u] = '\0';
        if (length > 1 && line[length - 2u] == '\r')
            line[length - 2u] = '\0';
        pg_case row;
        CHECK(pg_parse(line, &row) && !seen[row.ordinal] && (rows == 0 || row.ordinal > previous));
        seen[row.ordinal] = true;
        previous = row.ordinal;
        CHECK(pg_replay(f, side, &row, moving, initial, &direction_reached, direction_identities,
                        &failures));
        ++rows;
    }
    CHECK(!ferror(stdin) && rows == PG_ROWS && failures == 15 && direction_reached &&
          fflush(stdout) == 0);
    CHECK(f->entity_classes[side]->bounds_holds == 0 && f->worlds[side]->active_calls == 0 &&
          f->worlds[side]->local_leases == 0 && f->providers[side].cache->count == 0 &&
          mc_world_store_chunk_count(f->stores[side]) == 0);
    result = 0;
cleanup:
    if (!cleanup_fixture(f)) {
        fputs("piston geometry probe cleanup refused; surviving owners retained\n", stderr);
        return 1;
    }
    if (result == 0 && (fprintf(stderr,
                                "PASS: piston geometry replay; side=%s; rows=%zu; prefixes=%zu; "
                                "checks=%u; cleanup=OK\n",
                                argv[2], rows, failures, checks) < 0 ||
                        fflush(stderr) != 0))
        result = 1;
    return result;
}
