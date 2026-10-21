#include "mc/block/registry/registry.h"
#include "mc/world/runtime/construction.h"
#include "mc/world/runtime/runtime.h"
#include "mc/world/runtime/tiles/tiles.h"

#include "../../../../src/block/registry/registry_internal.h"
#include "../../../../src/world/runtime/construction_internal.h"
#include "../../../../src/world/runtime/runtime_internal.h"
#include "../../../../src/world/runtime/tiles/tiles_internal.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

enum { FIRST_CASE = 25, CASE_COUNT = 18292 };

/* This probe holds real GAME namespace/Class/Tile owners. The namespace stays
 * RAW: no World constructor, Store, Chunk, geometry or gameplay is substituted.
 * Only the actual private k/l fields are controlled as raw source inputs. The
 * public progress getter computes every result; expected results are not input. */
static bool raw_hex(const char *text, size_t length, uint32_t *out) {
    uint32_t value = 0;
    for (size_t i = 0; i < length; ++i) {
        const unsigned char c = (unsigned char)text[i];
        const unsigned int digit = c >= '0' && c <= '9'   ? (unsigned int)(c - '0')
                                   : c >= 'a' && c <= 'f' ? (unsigned int)(c - 'a') + 10u
                                                          : 16u;
        if (digit > 15u)
            return false;
        value = value * 16u + digit;
    }
    *out = value;
    return true;
}

static bool input_row(size_t index, char name[8], uint32_t *current, uint32_t *previous,
                      uint32_t *factor) {
    char line[64], expected[8];
    if (fgets(line, sizeof(line), stdin) == NULL)
        return false;
    size_t length = strlen(line);
    if (length == 0 || line[length - 1] != '\n')
        return false;
    line[--length] = '\0';
    if (length != 0 && line[length - 1] == '\r')
        line[--length] = '\0';
    if (length != 34 || line[7] != '\t' || line[16] != '\t' || line[25] != '\t' ||
        snprintf(expected, sizeof(expected), "p%06u", (unsigned int)(FIRST_CASE + index)) != 7 ||
        memcmp(line, expected, 7) != 0 || !raw_hex(line + 8, 8, current) ||
        !raw_hex(line + 17, 8, previous) || !raw_hex(line + 26, 8, factor))
        return false;
    memcpy(name, line, 7);
    name[7] = '\0';
    return true;
}

static bool unchanged_namespace(const mc_world_runtime *world,
                                const mc_registry_classes *registry) {
    return world->construction.mode == MC_WORLD_RUNTIME_GAME &&
           world->construction.phase == MC_WORLD_RUNTIME_RAW && world->fields.w == NULL &&
           world->fields.x == NULL && world->fields.v == NULL && world->persistent_bindings == 0 &&
           world->active_calls == 0 && world->local_leases == 0 &&
           registry->blocks_phase == MC_REGISTRY_COLD && registry->blocks.m == NULL &&
           registry->direction_state.phase == MC_REGISTRY_COLD;
}

static bool unchanged_tile(const mc_tile *tile, uint32_t current, uint32_t previous) {
    mc_tile_snapshot view;
    return mc_tile_get_snapshot(tile, &view) == MC_TILE_OK && view.kind == MC_TILE_PISTON &&
           view.phase == MC_TILE_READY && view.world == NULL && view.x == 0 && view.y == 0 &&
           view.z == 0 && !view.invalid && view.piston_block_id == 36 &&
           view.piston_block_data == -1 && view.piston_facing == 5 && view.piston_extending &&
           view.piston_render_head && view.piston_progress_bits == current &&
           view.piston_previous_progress_bits == previous && tile->holds == 1 &&
           tile->source == NULL;
}

#define REQUIRE(condition)                                                                         \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "FAIL: piston interpolation; line=%d; rows=%zu\n", __LINE__, rows);    \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

int main(int argc, char **argv) {
    bool success = false, cleanup_ok = true;
    size_t rows = 0;
    mc_materials *materials = NULL;
    mc_java_random_profile *profile = NULL;
    mc_registry_classes *registry = NULL;
    mc_world_runtime_classes *classes = NULL;
    mc_world_runtime *world = NULL;
    mc_tile *tile = NULL;
    mc_math_trig trig;
    mc_tile_piston_class_snapshot class_view;
    const mc_world_reference_list *list = NULL;
    if (argc != 3 || strcmp(argv[1], "piston-interpolation") != 0 ||
        (strcmp(argv[2], "CLIENT") != 0 && strcmp(argv[2], "SERVER") != 0)) {
        fputs("usage: piston_float_probe piston-interpolation CLIENT|SERVER\n", stderr);
        return EXIT_FAILURE;
    }
#ifdef _WIN32
    if (_setmode(_fileno(stdin), _O_BINARY) == -1 || _setmode(_fileno(stdout), _O_BINARY) == -1 ||
        _setmode(_fileno(stderr), _O_BINARY) == -1)
        return EXIT_FAILURE;
#endif
    const bool client = strcmp(argv[2], "CLIENT") == 0;
    REQUIRE(mc_math_trig_init(&trig) && mc_materials_create(&materials));
    REQUIRE(mc_java_random_profile_create(4u * 1024u * 1024u, &profile) ==
            MC_JAVA_RANDOM_PROFILE_OK);
    const mc_registry_options options = {client ? MC_REGISTRY_CLIENT : MC_REGISTRY_SERVER, profile,
                                         materials, 0, NULL};
    REQUIRE(mc_registry_classes_allocate(&options, &registry, NULL) == MC_REGISTRY_OK);
    REQUIRE(mc_world_runtime_classes_create(client ? MC_WORLD_INFO_CLIENT : MC_WORLD_INFO_SERVER,
                                            profile, &trig, &classes, NULL) == MC_WORLD_STORAGE_OK);
    REQUIRE(mc_world_runtime_classes_bind_registry(classes, registry, NULL) == MC_REGISTRY_OK);
    REQUIRE(mc_world_runtime_allocate_world(client ? MC_WORLD_RUNTIME_CLIENT_DIRECT_METADATA
                                                   : MC_WORLD_RUNTIME_SERVER_LOAD_OR_CREATE,
                                            classes, NULL, &world, NULL) == MC_WORLD_STORAGE_OK);
    REQUIRE(mc_tile_allocate(world, MC_TILE_PISTON, &tile) == MC_TILE_OK);
    REQUIRE(mc_tile_piston_construct(tile, 36, -1, 5, true, true, NULL) == MC_TILE_OK);
    REQUIRE(unchanged_namespace(world, registry) && unchanged_tile(tile, 0, 0));
    REQUIRE(mc_tile_piston_class_get_snapshot(classes, &class_view) == MC_TILE_OK &&
            class_view.base_phase == MC_TILE_CLASS_READY &&
            class_view.piston_phase == MC_TILE_CLASS_READY && class_view.base_name_count == 8 &&
            class_view.base_class_count == 8 && class_view.live_tiles == 1 &&
            class_view.list_identity != NULL && class_view.list_count == 0 &&
            class_view.list_capacity == 0);
    list = class_view.list_identity;
    REQUIRE(fprintf(stdout, "PISTON_INTERPOLATION_V1\t%s\n", argv[2]) > 0);
    for (; rows < CASE_COUNT; ++rows) {
        char name[8];
        uint32_t current, previous, factor, actual = 0, read_current = 0, read_previous = 0;
        REQUIRE(input_row(rows, name, &current, &previous, &factor));
        /* Actual private source fields, immediately reread through production.
         * No owner, class phase, World binding or hold is manufactured. */
        tile->state.piston.progress_bits = current;
        tile->state.piston.previous_progress_bits = previous;
        const mc_tile_status read_status =
            mc_tile_piston_progress_bits(tile, &read_current, &read_previous);
        const bool input_ok = read_status == MC_TILE_OK && read_current == current &&
                              read_previous == previous && unchanged_tile(tile, current, previous);
        const mc_tile_status result = mc_tile_piston_progress(tile, factor, &actual, NULL);
        const bool fields_unchanged = unchanged_tile(tile, current, previous);
        tile->state.piston.progress_bits = 0;
        tile->state.piston.previous_progress_bits = 0;
        REQUIRE(input_ok && result == MC_TILE_OK && fields_unchanged && unchanged_tile(tile, 0, 0));
        REQUIRE(fprintf(stdout, "%s\t%08" PRIx32 "\t%08" PRIx32 "\t%08" PRIx32 "\t%08" PRIx32 "\n",
                        name, current, previous, factor, actual) > 0);
    }
    REQUIRE(fgetc(stdin) == EOF && !ferror(stdin));
    REQUIRE(unchanged_namespace(world, registry));
    REQUIRE(mc_tile_piston_class_get_snapshot(classes, &class_view) == MC_TILE_OK &&
            class_view.list_identity == list && class_view.list_count == 0 &&
            class_view.list_capacity == 0 && class_view.live_tiles == 1);
    success = true;
cleanup:
    mc_tile_release(tile);
    tile = NULL;
    if (classes != NULL && (mc_tile_piston_class_get_snapshot(classes, &class_view) != MC_TILE_OK ||
                            class_view.live_tiles != 0))
        cleanup_ok = false;
    if (world != NULL && mc_world_runtime_free(world, NULL) != MC_WORLD_STORAGE_OK)
        cleanup_ok = false;
    else
        world = NULL;
    if (world == NULL && classes != NULL &&
        mc_world_runtime_classes_free(classes, NULL) != MC_WORLD_STORAGE_OK)
        cleanup_ok = false;
    else if (world == NULL)
        classes = NULL;
    if (classes == NULL && registry != NULL &&
        mc_registry_classes_free(registry, NULL) != MC_REGISTRY_OK)
        cleanup_ok = false;
    else if (classes == NULL)
        registry = NULL;
    if (registry == NULL) {
        mc_java_random_profile_free(profile);
        mc_materials_destroy(materials);
    }
    if (fflush(stdout) != 0 || ferror(stdout))
        cleanup_ok = false;
    if (!success || !cleanup_ok) {
        fprintf(stderr, "FAIL: piston interpolation; rows=%zu; cleanup=%s\n", rows,
                cleanup_ok ? "OK" : "FAILED");
        return EXIT_FAILURE;
    }
    if (fprintf(stderr, "PASS: piston interpolation; side=%s; rows=18292; cleanup=OK\n", argv[2]) <
            0 ||
        fflush(stderr) != 0)
        return EXIT_FAILURE;
    return EXIT_SUCCESS;
}
