#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#define MC_PICKING_BODY_MAIN_NAME mc_bed_picking_fixture_main
int mc_bed_picking_fixture_main(int argc, char **argv);
#include "../world/test_picking.c"
#undef main

#include "block/registry/bed_render_tables_internal.h"
#include "block/registry/registry_internal.h"
#include "client/render/blocks/internal.h"
#include "client/render/model_graph_internal.h"
#include "client/render_engine/nw.h"

static uint64_t bed_bits(double value) {
    uint64_t bits;
    memcpy(&bits, &value, sizeof bits);
    return bits;
}

static bool bed_prepare(picking_fixture *f, int32_t metadata, unsigned int profile) {
    mc_chunk_state *chunk = f->chunks[0];
    memset(chunk->data->blocks, 0, MC_CHUNK_BLOCKS);
    memset(chunk->data->metadata, 0, MC_CHUNK_NIBBLE_BYTES);
    memset(chunk->data->skylight, 0xff, MC_CHUNK_NIBBLE_BYTES);
    memset(chunk->data->block_light, 0, MC_CHUNK_NIBBLE_BYTES);
    chunk->data->blocks[(8u << 11u) | (8u << 7u) | 64u] = 26;
    CHECK(mc_chunk_nibbles_set(chunk->data->metadata, MC_CHUNK_NIBBLE_BYTES, 8, 64, 8,
                               (uint8_t)metadata));
    if (profile == 1 || profile == 2) {
        static const unsigned int neighbors[4][2] = {{8, 7}, {8, 9}, {7, 8}, {9, 8}};
        for (size_t i = 0; i < 4; ++i)
            chunk->data->blocks[(neighbors[i][0] << 11u) | (neighbors[i][1] << 7u) | 64u] = 1;
    }
    return true;
}

static bool bed_tables_limit(picking_fixture *f) {
    mc_registry_classes *classes = NULL;
    mc_registry_options options = f->base.registry->options;
    options.max_allocation_bytes = 0;
    CHECK(mc_registry_classes_allocate(&options, &classes, NULL) == MC_REGISTRY_OK);
    const size_t before = classes->allocation_bytes;
    CHECK(mc_registry_allocate(classes, 4u * sizeof(int32_t)) != NULL);
    const size_t one_array = classes->allocation_bytes - before;
    classes->options.max_allocation_bytes = classes->allocation_bytes + one_array;
    mc_registry_bed_render_array view = {0};
    mc_registry_error error;
    CHECK(mc_registry_bed_render_array_get(classes, false, &view, &error) == MC_REGISTRY_LIMIT &&
          error.active_initializer_class != NULL &&
          strcmp(error.active_initializer_class, "jj") == 0 && error.active_initializer_pc == 23 &&
          error.initializer_pc == -1 && classes->bed_render_state.phase == MC_REGISTRY_FAILED &&
          classes->bed_render_a != NULL && classes->bed_render_a[0] == 3 &&
          classes->bed_render_a[1] == 4 && classes->bed_render_a[2] == 2 &&
          classes->bed_render_a[3] == 5 && classes->bed_render_b == NULL &&
          classes->bed_render_c == NULL && view.identity == NULL && classes->entered == 0);
    int32_t *prefix = classes->bed_render_a;
    const size_t failed_bytes = classes->allocation_bytes;
    classes->options.max_allocation_bytes = SIZE_MAX;
    CHECK(mc_registry_bed_render_array_get(classes, false, &view, &error) ==
              MC_REGISTRY_FAILED_INITIALIZATION &&
          classes->bed_render_a == prefix && classes->allocation_bytes == failed_bytes &&
          error.active_initializer_pc == 23 && classes->entered == 0);
    CHECK(mc_registry_classes_free(classes, NULL) == MC_REGISTRY_OK);
    return true;
}

static bool bed_controls(picking_fixture *f) {
    mc_client_chunk_cache *cache = NULL;
    mc_client_chunk_cache_error cache_error;
    CHECK(mc_client_chunk_cache_create(f->base.world, 0, 0, 0, 31, 127, 31, &cache, &cache_error) ==
          MC_CLIENT_CHUNK_CACHE_OK);
    CHECK(mc_client_render_blocks_active_use(f->models, NULL));
    const mc_block *block = NULL;
    CHECK(mc_registry_block_get(f->base.registry, 26, &block, NULL) == MC_REGISTRY_OK &&
          block != NULL && block->kind == MC_BLOCK_BED);
    mc_client_tessellator *nw = NULL;
    mc_client_graphics_error graphics;
    CHECK(mc_client_nw_active_use(f->graphics,
                                  (mc_client_graphics_site){"native-test", "bed", "()V", 0}, &nw,
                                  &graphics) &&
          mc_client_nw_begin_quads(nw, &graphics));
    f->base.world->fields.f = 3;
    mc_registry_classes *registry = f->base.registry;
    unsigned int rows = 0;
    for (int32_t metadata = 0; metadata < 16; ++metadata)
        for (unsigned int override = 0; override < 2; ++override)
            for (unsigned int profile = 0; profile < 6; ++profile) {
                CHECK(bed_prepare(f, metadata, profile));
                mc_client_render_blocks fields;
                mc_client_render_blocks_construct_fields(&fields, cache, true);
                fields.f = profile == 2;
                fields.d = override ? 17 : -1;
                fields.e = true;
                fields.m = profile == 3;
                for (size_t i = 0; i < 12; ++i)
                    fields.P_to_aa[i] = (float)(i + 1) / 16.0f;
                int32_t saved_a[4];
                int32_t saved_top = 0;
                if (profile == 4) {
                    memcpy(saved_a, registry->bed_render_a, sizeof saved_a);
                    memset(registry->bed_render_a, 0, sizeof saved_a);
                }
                if (profile == 5) {
                    saved_top = registry->bed_render_c[metadata & 3][1];
                    registry->bed_render_c[metadata & 3][1] = 2;
                }
                mc_cv_context context = {.fields = &fields,
                                         .cache = cache,
                                         .registry = registry,
                                         .models = f->models,
                                         .graphics = f->graphics,
                                         .application = f->app};
                mc_client_nw_state before, after;
                CHECK(mc_client_nw_inspect(f->graphics, &before));
                bool rendered = false;
                const bool complete = mc_cv_render(&context, block, 8, 64, 8, &rendered);
                if (profile == 4)
                    memcpy(registry->bed_render_a, saved_a, sizeof saved_a);
                if (profile == 5)
                    registry->bed_render_c[metadata & 3][1] = saved_top;
                CHECK(complete && rendered && !context.failed && context.returned &&
                      context.depth == 0 && context.source_pc == 268 && !fields.e &&
                      fields.m == (profile == 3) && fields.d == (override ? 17 : -1) &&
                      mc_client_nw_inspect(f->graphics, &after));
                const int32_t vertices = (after.fields.o - before.fields.o) / 8;
                CHECK(vertices == (profile == 1 ? 12 : profile == 4 ? 36 : 30));
                printf("BED\t%" PRId32 "\t%d\t%u\ttrue\t%d\tfalse\t%s\t%d\t", metadata,
                       override ? 17 : -1, profile, fields.d, fields.m ? "true" : "false",
                       vertices);
                const double bounds[] = {block->fields.bs, block->fields.bt, block->fields.bu,
                                         block->fields.bv, block->fields.bw, block->fields.bx};
                for (size_t i = 0; i < 6; ++i)
                    printf("%s%016" PRIx64, i ? "," : "", bed_bits(bounds[i]));
                putchar('\t');
                for (int32_t i = before.fields.o; i < after.fields.o; ++i)
                    printf("%s%08" PRIx32, i == before.fields.o ? "" : ",",
                           (uint32_t)after.fields.g[i]);
                putchar('\n');
                ++rows;
            }
    CHECK(rows == 192 && bed_prepare(f, 0, 0));
    /* Captured locals retain the old genuine arrays across field replacement. */
    mc_registry_bed_render_array captured;
    CHECK(mc_registry_bed_render_array_get(registry, false, &captured, NULL) == MC_REGISTRY_OK);
    int32_t *saved_a = registry->bed_render_a;
    registry->bed_render_a = NULL;
    int32_t value = -1;
    const mc_registry_status captured_status =
        mc_registry_bed_render_array_at(&captured, 0, &value, NULL);
    registry->bed_render_a = saved_a;
    CHECK(captured_status == MC_REGISTRY_OK && value == 3);
    for (unsigned int control = 0; control < 4; ++control) {
        mc_client_render_blocks fields;
        mc_client_render_blocks_construct_fields(&fields, cache, true);
        fields.e = true;
        mc_cv_context context = {.fields = &fields,
                                 .cache = cache,
                                 .registry = registry,
                                 .models = f->models,
                                 .graphics = f->graphics};
        mc_client_nw_state before, after;
        CHECK(mc_client_nw_inspect(f->graphics, &before));
        int32_t **matrix = registry->bed_render_c;
        int32_t *row = matrix[0];
        if (control == 0)
            registry->bed_render_a = NULL;
        else if (control == 1)
            registry->bed_render_c = NULL;
        else
            matrix[0] = control == 2 ? NULL : (int32_t *)(uintptr_t)1;
        bool rendered = false;
        const bool complete = mc_cv_render(&context, block, 8, 64, 8, &rendered);
        registry->bed_render_a = saved_a;
        registry->bed_render_c = matrix;
        matrix[0] = row;
        const mc_client_render_global_status expected_status =
            control == 3 ? MC_CLIENT_RENDER_GLOBAL_UNIMPLEMENTED_DEPENDENCY
                         : MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE;
        const unsigned int expected_pc = control == 0 ? 673u : control == 1 ? 20u : 22u;
        const size_t expected_depth = control == 0 ? 2u : 4u;
        CHECK(!complete && !rendered && context.failed && !context.returned && fields.e &&
              context.failure.status == expected_status &&
              context.failure.native_policy == (control == 3) && context.source_pc == expected_pc &&
              context.depth == expected_depth && mc_client_nw_inspect(f->graphics, &after) &&
              after.fields.o == before.fields.o + (control == 0 ? 12 : 6) * 8);
    }
    CHECK(bed_tables_limit(f));
    CHECK(mc_client_chunk_cache_retire(&cache, &cache_error) == MC_CLIENT_CHUNK_CACHE_OK);
    return !ferror(stdout);
}

int main(int argc, char **argv) {
    if (argc != 3)
        return 2;
    picking_fixture *f = calloc(1, sizeof(*f));
    if (f == NULL || !picking_open(f, argv[1]) || !picking_case_open(f, argv[2]) ||
        !bed_controls(f) || !picking_case_close(f) || !picking_close(f))
        return 1;
    free(f);
    printf("Bed World: %u checks;192 original geometry rows and retained native failure controls\n",
           checks);
    return 0;
}
