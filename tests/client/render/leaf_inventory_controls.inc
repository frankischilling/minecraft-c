#include "client/render/blocks/inventory_color.h"

static void leaf_inventory_context(fixture *f, mc_client_render_engine_classes *graphics) {
    memset(f->context, 0, sizeof(*f->context));
    mc_client_render_blocks_construct_fields(&f->fields, NULL, false);
    f->context->fields = &f->fields;
    f->context->registry = f->registry;
    f->context->models = f->models;
    f->context->graphics = graphics;
}

static bool leaf_inventory_controls(fixture *f) {
    static const int32_t leaf_colors[16] = {4764952, 6396257, 8431445, 6396257, 4764952, 6396257,
                                            8431445, 6396257, 4764952, 6396257, 8431445, 6396257,
                                            4764952, 6396257, 8431445, 6396257};
    static const int32_t signed_metadata[] = {INT32_MIN, -4, -3, -2, -1, 16, INT32_MAX};
    static const int32_t signed_colors[] = {4764952, 4764952, 6396257, 8431445,
                                            6396257, 4764952, 6396257};
    const mc_block *leaf_read;
    CHECK(mc_registry_block_get(f->registry, 18, &leaf_read, NULL) == MC_REGISTRY_OK &&
          leaf_read != NULL && leaf_read->kind == MC_BLOCK_LEAVES);
    mc_block *leaf = (mc_block *)(void *)leaf_read;
    const mc_block initial = *leaf;
    mc_client_colorizer_state state;
    CHECK(mc_client_colorizer_inspect(f->graphics, MC_CLIENT_COLORIZER_FOLIAGE, &state) &&
          state.phase == MC_CLIENT_COLORIZER_RAW && state.a == NULL);
    /* Registry-only color must retain its genuine missing namespace boundary. */
    int32_t color = -777;
    CHECK(mc_block_render_inventory_color(leaf, 0, &color) ==
              MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY &&
          color == -777);
    for (int32_t metadata = 0; metadata < 16; ++metadata) {
        leaf_inventory_context(f, f->graphics);
        CHECK(mc_cv_push(f->context, "a", "(Luu;IF)V"));
        color = -777;
        CHECK(mc_cv_leaf_inventory_color(f->context, metadata, &color) &&
              color == leaf_colors[metadata] && !f->context->failed && f->context->depth == 1 &&
              f->context->frames[0].pc == 14 &&
              f->context->source_pc == ((metadata & 1)   ? 10u
                                        : (metadata & 2) ? 21u
                                                         : 25u) &&
              memcmp(leaf, &initial, sizeof(initial)) == 0);
        CHECK(mc_client_colorizer_inspect(f->graphics, MC_CLIENT_COLORIZER_FOLIAGE, &state) &&
              state.phase == MC_CLIENT_COLORIZER_READY && state.a != NULL &&
              state.last_store_pc == 4);
        mc_client_color_array_state array;
        CHECK(mc_client_color_array_inspect(state.a, &array) && array.length == 65536 &&
              array.phase == MC_CLIENT_COLOR_ARRAY_READY);
        CHECK(mc_block_render_inventory_shape(leaf) == MC_REGISTRY_OK &&
              memcmp(leaf, &initial, sizeof(initial)) == 0);
        printf("LEAF_INVENTORY_COLOR %d %08" PRIx32 " %016" PRIx64 " %016" PRIx64 " %016" PRIx64
               " %016" PRIx64 " %016" PRIx64 " %016" PRIx64 "\n",
               metadata, (uint32_t)color, double_bits(leaf->fields.bs),
               double_bits(leaf->fields.bt), double_bits(leaf->fields.bu),
               double_bits(leaf->fields.bv), double_bits(leaf->fields.bw),
               double_bits(leaf->fields.bx));
        /* The complete production entry must consume that color before its
         * actual Display call74. This CPU fixture has no Display, so it cannot
         * claim returned GL faces or manufacture a successful draw. */
        leaf_inventory_context(f, f->graphics);
        CHECK(!mc_cv_inventory(f->context, leaf, metadata, .75f) && f->context->failed &&
              f->context->source_pc == 74 && f->context->depth == 1 &&
              strcmp(f->context->failure.source_class, "cv") == 0 &&
              f->context->inventory_faces == 0 && !f->nw->fields.w &&
              memcmp(leaf, &initial, sizeof(initial)) == 0);
    }
    /* JH's constant methods ignore the current int[] and floating controls.
     * A NULL actual array is not a substitute for a missing JH namespace. */
    mc_client_colorizer_error error;
    CHECK(mc_client_colorizer_set(f->graphics, MC_CLIENT_COLORIZER_FOLIAGE, NULL, NULL, &error) ==
          MC_CLIENT_COLORIZER_OK);
    for (size_t i = 0; i < sizeof(signed_metadata) / sizeof(*signed_metadata); ++i) {
        leaf_inventory_context(f, f->graphics);
        CHECK(mc_cv_push(f->context, "a", "(Luu;IF)V"));
        color = -777;
        const unsigned int saved = _mm_getcsr();
        _mm_setcsr(saved | 0x8000u);
        const bool returned = mc_cv_leaf_inventory_color(f->context, signed_metadata[i], &color);
        _mm_setcsr(saved);
        CHECK(returned && color == signed_colors[i] && !f->context->failed &&
              mc_client_colorizer_inspect(f->graphics, MC_CLIENT_COLORIZER_FOLIAGE, &state) &&
              state.phase == MC_CLIENT_COLORIZER_READY && state.a == NULL);
    }
    leaf_inventory_context(f, f->graphics);
    const unsigned int saved = _mm_getcsr();
    _mm_setcsr(saved | 0x8000u);
    const bool invalid_fp = mc_cv_inventory(f->context, leaf, 3, .75f);
    _mm_setcsr(saved);
    CHECK(!invalid_fp && f->context->failed && f->context->source_pc == 31 &&
          f->context->failure.status == MC_CLIENT_RENDER_GLOBAL_UNSUPPORTED_FP);

    /* Inherited UU.g is a no-op even for non-cube live bounds and RAW owners. */
    CHECK(mc_block_render_bounds(leaf, .125f, .25f, .375f, .75f, .875f, 1) == MC_REGISTRY_OK);
    const mc_block authored = *leaf;
    CHECK(mc_block_render_inventory_shape(leaf) == MC_REGISTRY_OK &&
          memcmp(leaf, &authored, sizeof(authored)) == 0);
    printf("LEAF_INVENTORY_SHAPE %016" PRIx64 " %016" PRIx64 " %016" PRIx64 " %016" PRIx64
           " %016" PRIx64 " %016" PRIx64 "\n",
           double_bits(leaf->fields.bs), double_bits(leaf->fields.bt), double_bits(leaf->fields.bu),
           double_bits(leaf->fields.bv), double_bits(leaf->fields.bw),
           double_bits(leaf->fields.bx));
    mc_block *raw = NULL;
    CHECK(mc_registry_block_allocate(f->registry, MC_BLOCK_LEAVES, &raw) == MC_REGISTRY_OK);
    const mc_block raw_before = *raw;
    CHECK(mc_block_render_inventory_shape(raw) == MC_REGISTRY_OK &&
          memcmp(raw, &raw_before, sizeof(raw_before)) == 0);
    /* Restore only authored bounds; never overwrite unrelated live owner state. */
    leaf->fields.bs = initial.fields.bs;
    leaf->fields.bt = initial.fields.bt;
    leaf->fields.bu = initial.fields.bu;
    leaf->fields.bv = initial.fields.bv;
    leaf->fields.bw = initial.fields.bw;
    leaf->fields.bx = initial.fields.bx;

    mc_client_render_engine_classes *cold = NULL;
    CHECK(mc_client_render_engine_classes_create(NULL, &cold) == MC_CLIENT_RENDER_ENGINE_OK);
    leaf_inventory_context(f, cold);
    f->fields.b = false;
    CHECK(!mc_cv_inventory(f->context, leaf, 2, .75f) && f->context->source_pc == 114 &&
          f->context->inventory_faces == 0 &&
          mc_client_colorizer_inspect(cold, MC_CLIENT_COLORIZER_FOLIAGE, &state) &&
          state.phase == MC_CLIENT_COLORIZER_RAW && state.a == NULL &&
          memcmp(leaf, &initial, sizeof(initial)) == 0);
    memset(f->context, 0, sizeof(*f->context));
    CHECK(mc_client_render_engine_classes_release(cold) == MC_CLIENT_RENDER_ENGINE_OK);

    mc_client_render_engine_classes *failed = NULL;
    CHECK(mc_client_render_engine_classes_create(NULL, &failed) == MC_CLIENT_RENDER_ENGINE_OK);
    mc_client_colorizer_options limits = mc_client_colorizer_default_options();
    limits.initial_array[MC_CLIENT_COLORIZER_FOLIAGE] = (mc_client_color_array_limits){0, 0};
    CHECK(mc_client_colorizer_initialize(failed, MC_CLIENT_COLORIZER_FOLIAGE, &limits, &error) !=
          MC_CLIENT_COLORIZER_OK);
    for (int32_t metadata = 0; metadata < 16; ++metadata) {
        leaf_inventory_context(f, failed);
        CHECK(mc_cv_push(f->context, "a", "(Luu;IF)V"));
        color = -777;
        CHECK(!mc_cv_leaf_inventory_color(f->context, metadata, &color) && color == -777 &&
              f->context->failed && f->context->depth == 2 &&
              f->context->source_pc == ((metadata & 1)   ? 7u
                                        : (metadata & 2) ? 18u
                                                         : 22u) &&
              strcmp(f->context->failure.source_class, "bk") == 0 &&
              strcmp(f->context->failure.source_descriptor, "(I)I") == 0 &&
              strcmp(f->context->failure.child.class_name, "jh") == 0 &&
              strcmp(f->context->failure.child.method, "<clinit>") == 0 &&
              f->context->failure.child.pc == 2 &&
              f->context->colorizer_error.status == MC_CLIENT_COLORIZER_CLASS_FAILED);
    }
    memset(f->context, 0, sizeof(*f->context));
    CHECK(mc_client_render_engine_classes_release(failed) == MC_CLIENT_RENDER_ENGINE_OK);
    return true;
}
