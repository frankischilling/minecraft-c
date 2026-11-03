#include "client/render/blocks/inventory_color.h"

typedef struct inventory_resource_preparation {
    resources *input;
    mc_registry_classes *registry;
    mc_block *raw_base;
} inventory_resource_preparation;

static mc_registry_status inventory_resource_open(void *context, const char *name,
                                                  mc_java_properties_input *out) {
    inventory_resource_preparation *preparation = context;
    /* BASE's private allocator requires an admitted initialization flow. This
     * actual typed resource callback runs inside that real flow; no entered
     * flag, phase or constructed Block result is manufactured by the fixture. */
    if (preparation->raw_base == NULL) {
        mc_block *saved = preparation->registry->current_block;
        const mc_registry_status status = mc_registry_block_allocate(
            preparation->registry, MC_BLOCK_BASE, &preparation->raw_base);
        preparation->registry->current_block = saved;
        if (status != MC_REGISTRY_OK)
            return status;
    }
    return open_resource(preparation->input, name, out);
}

static mc_registry_status inventory_resource_close(void *context,
                                                   const mc_java_properties_input *input) {
    inventory_resource_preparation *preparation = context;
    return close_resource(preparation->input, input);
}

static bool inventory_resource_ready(void *context, const mc_java_properties_input *input) {
    inventory_resource_preparation *preparation = context;
    return ready_resource(preparation->input, input);
}

/* Declared inputs mirror the guarded original matrix, not expected textures. */
static const int32_t inventory_metadata[] = {0,  1,  2,   3,  4,  5,         6,        7,
                                             8,  9,  10,  11, 12, 13,        14,       15,
                                             -1, -2, -17, 16, 31, INT32_MIN, INT32_MAX};
static const int32_t inventory_late_metadata[] = {0, 3, INT32_MIN, INT32_MAX};

static void inventory_bounds_row(const mc_block_fields *fields) {
    printf("\t%016" PRIx64 "\t%016" PRIx64 "\t%016" PRIx64 "\t%016" PRIx64 "\t%016" PRIx64
           "\t%016" PRIx64,
           double_bits(fields->bs), double_bits(fields->bt), double_bits(fields->bu),
           double_bits(fields->bv), double_bits(fields->bw), double_bits(fields->bx));
}

static void inventory_restore_bounds(mc_block *block, const mc_block_fields *fields) {
    block->fields.bs = fields->bs;
    block->fields.bt = fields->bt;
    block->fields.bu = fields->bu;
    block->fields.bv = fields->bv;
    block->fields.bw = fields->bw;
    block->fields.bx = fields->bx;
}

static bool inventory_shape_unchanged_except_bounds(const mc_block *block, const mc_block *before) {
    mc_block expected = *before;
    inventory_restore_bounds(&expected, &block->fields);
    return memcmp(block, &expected, sizeof(expected)) == 0;
}

static bool inventory_family_controls(fixture *f, mc_block *prepared_base) {
    mc_client_render_engine_classes *matrix_graphics = NULL;
    mc_cv_context *matrix_context = calloc(1, sizeof(*matrix_context));
    CHECK(matrix_context != NULL && mc_client_render_engine_classes_create(
                                        NULL, &matrix_graphics) == MC_CLIENT_RENDER_ENGINE_OK);
    size_t block_count = 0, cube_count = 0;
    for (int32_t id = 1; id < 256; ++id) {
        const mc_block *read;
        CHECK(mc_registry_block_get(f->registry, id, &read, NULL) == MC_REGISTRY_OK);
        if (read == NULL)
            continue;
        ++block_count;
        mc_block *block = (mc_block *)(void *)read;
        const mc_block initial = *block;
        mc_block_render_description description;
        CHECK(mc_block_render_describe(block, &description) == MC_REGISTRY_OK);
        printf("BLOCK\t%d\t%d\n", id, description.type);
        for (size_t m = 0; m < sizeof(inventory_metadata) / sizeof(*inventory_metadata); ++m) {
            int32_t color = INT32_C(0x12345678);
            const mc_registry_status status =
                mc_block_render_inventory_color(block, inventory_metadata[m], &color);
            if (block->kind == MC_BLOCK_LEAVES) {
                CHECK(status == MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY &&
                      color == INT32_C(0x12345678));
                memset(matrix_context, 0, sizeof(*matrix_context));
                matrix_context->graphics = matrix_graphics;
                matrix_context->registry = f->registry;
                CHECK(mc_cv_push(matrix_context, "a", "(Luu;IF)V") &&
                      mc_cv_leaf_inventory_color(matrix_context, inventory_metadata[m], &color));
            } else
                CHECK(status == MC_REGISTRY_OK);
            CHECK(memcmp(block, &initial, sizeof(initial)) == 0);
            printf("COLOR\t%d\t%d\t%08" PRIx32 "\n", id, inventory_metadata[m], (uint32_t)color);
        }
        for (unsigned int scene = 0; scene < 2; ++scene) {
            inventory_restore_bounds(block, &initial.fields);
            if (scene != 0)
                CHECK(mc_block_render_bounds(block, .125f, .25f, .375f, .75f, .875f, 1) ==
                      MC_REGISTRY_OK);
            const mc_block shape_input = *block;
            CHECK(mc_block_render_inventory_shape(block) == MC_REGISTRY_OK &&
                  inventory_shape_unchanged_except_bounds(block, &shape_input));
            printf("SHAPE\t%d\t%u", id, scene);
            inventory_bounds_row(&shape_input.fields);
            inventory_bounds_row(&block->fields);
            putchar('\n');
        }
        inventory_restore_bounds(block, &initial.fields);
        if (description.type == 0) {
            ++cube_count;
            for (size_t m = 0; m < sizeof(inventory_metadata) / sizeof(*inventory_metadata); ++m)
                for (int32_t side = 0; side < 6; ++side) {
                    int32_t texture;
                    CHECK(mc_block_render_texture_metadata(block, side, inventory_metadata[m],
                                                           &texture) == MC_REGISTRY_OK);
                    printf("TEXTURE\t%d\t%d\t%d\t%08" PRIx32 "\n", id, inventory_metadata[m], side,
                           (uint32_t)texture);
                }
            const int32_t late_fields[] = {-3, 197};
            for (size_t field = 0; field < sizeof(late_fields) / sizeof(*late_fields); ++field) {
                block->fields.bm = late_fields[field];
                for (size_t m = 0;
                     m < sizeof(inventory_late_metadata) / sizeof(*inventory_late_metadata); ++m)
                    for (int32_t side = 0; side < 6; ++side) {
                        int32_t texture;
                        CHECK(mc_block_render_texture_metadata(block, side,
                                                               inventory_late_metadata[m],
                                                               &texture) == MC_REGISTRY_OK);
                        printf("TEXTURE_LATE\t%d\t%d\t%d\t%d\t%08" PRIx32 "\n", id,
                               late_fields[field], inventory_late_metadata[m], side,
                               (uint32_t)texture);
                    }
            }
            block->fields.bm = initial.fields.bm;
        }
        CHECK(memcmp(block, &initial, sizeof(initial)) == 0);
    }
    CHECK(block_count == 96 && cube_count == 57);
    memset(matrix_context, 0, sizeof(*matrix_context));
    CHECK(mc_client_render_engine_classes_release(matrix_graphics) == MC_CLIENT_RENDER_ENGINE_OK);
    free(matrix_context);

    /* Native admission and source-pure RAW paths are separate from the normal
     * original matrix; no successful texture or original exception is faked. */
    mc_block *refused_base = NULL;
    CHECK(f->registry->entered == 0 &&
          mc_registry_block_allocate(f->registry, MC_BLOCK_BASE, &refused_base) ==
              MC_REGISTRY_INVALID_ARGUMENT &&
          refused_base == NULL &&
          strcmp(f->registry->last_error.message,
                 "Block active use requires an admitted initialization flow") == 0);
    for (unsigned int kind = 0; kind < MC_BLOCK_KIND_COUNT; ++kind) {
        mc_block *raw = kind == MC_BLOCK_BASE ? prepared_base : NULL;
        const mc_registry_status allocation =
            kind == MC_BLOCK_BASE
                ? MC_REGISTRY_OK
                : mc_registry_block_allocate(f->registry, (mc_block_kind)kind, &raw);
        if (allocation != MC_REGISTRY_OK || raw == NULL || raw->phase != MC_BLOCK_RAW)
            fprintf(stderr,
                    "inventory RAW allocation kind=%u status=%d phase=%d blocks=%d "
                    "class=%d pc=%d operation=%s\n",
                    kind, (int)allocation, raw == NULL ? -1 : (int)raw->phase,
                    (int)f->registry->blocks_phase, (int)f->registry->block_states[kind].phase,
                    f->registry->block_states[kind].pc,
                    f->registry->last_error.message == NULL ? "none"
                                                            : f->registry->last_error.message);
        CHECK(allocation == MC_REGISTRY_OK && raw != NULL && raw->phase == MC_BLOCK_RAW);
        const mc_block raw_before = *raw;
        int32_t color = -777;
        const mc_registry_status color_status =
            mc_block_render_inventory_color(raw, INT32_MIN, &color);
        CHECK(color_status == (kind == MC_BLOCK_LEAVES ? MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY
                                                       : MC_REGISTRY_OK) &&
              color == (kind == MC_BLOCK_LEAVES ? -777 : 0xffffff) &&
              memcmp(raw, &raw_before, sizeof(raw_before)) == 0 &&
              mc_block_render_inventory_shape(raw) == MC_REGISTRY_OK &&
              inventory_shape_unchanged_except_bounds(raw, &raw_before));
        const mc_block_kind saved_kind = raw->kind;
        raw->kind = MC_BLOCK_KIND_COUNT;
        const mc_block invalid = *raw;
        color = -777;
        CHECK(mc_block_render_inventory_color(raw, 0, &color) == MC_REGISTRY_INVALID_ARGUMENT &&
              color == -777 &&
              mc_block_render_inventory_shape(raw) == MC_REGISTRY_INVALID_ARGUMENT &&
              memcmp(raw, &invalid, sizeof(invalid)) == 0);
        raw->kind = saved_kind;
    }
    int32_t unchanged = -777;
    CHECK(mc_block_render_inventory_color(NULL, 0, &unchanged) == MC_REGISTRY_INVALID_ARGUMENT &&
          unchanged == -777 &&
          mc_block_render_inventory_shape(NULL) == MC_REGISTRY_INVALID_ARGUMENT);
    return true;
}
