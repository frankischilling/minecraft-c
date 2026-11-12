static bool foliage_item_site(const mc_item_render_site *site, const char *owner,
                              const char *method, const char *descriptor, int32_t pc) {
    return site->class_name != NULL && strcmp(site->class_name, owner) == 0 &&
           site->method != NULL && strcmp(site->method, method) == 0 && site->descriptor != NULL &&
           strcmp(site->descriptor, descriptor) == 0 && site->pc == pc;
}

static bool foliage_item_icon(fixture *f, const mc_item *item, const mc_item *raw,
                              mc_item_stack *stack, const char *owner, int32_t metadata,
                              int32_t expected) {
    CHECK(mc_item_stack_source_damage_set(stack, metadata) == MC_ITEM_STACK_OK);
    mc_item_stack_fields before, after;
    CHECK(mc_item_stack_fields_get(stack, &before) == MC_ITEM_STACK_OK);
    mc_item_render_error error;
    int32_t icon = -777;
    CHECK(mc_item_render_icon(f->registry, item, metadata, &icon, &error) && icon == expected &&
          error.returned && error.status == MC_REGISTRY_OK && !error.native_policy &&
          !error.source_exception && error.throwable == MC_ITEM_RENDER_NO_THROWABLE &&
          foliage_item_site(&error.site, owner, "a", "(I)I", 8));
    icon = -777;
    CHECK(mc_item_render_icon(f->registry, raw, metadata, &icon, &error) && icon == expected &&
          error.returned && foliage_item_site(&error.site, owner, "a", "(I)I", 8));
    icon = -777;
    CHECK(mc_item_stack_render_icon(f->registry, stack, &icon, &error) && icon == expected &&
          error.returned && error.status == MC_REGISTRY_OK &&
          foliage_item_site(&error.site, "iz", "b", "()I", 8) && error.ancestor_count == 0 &&
          error.stack_identity == (uintptr_t)stack &&
          error.array_identity == (uintptr_t)f->registry->items.c &&
          error.item_identity == (uintptr_t)item && error.id_read && error.id == before.id &&
          error.damage_read && error.damage == metadata &&
          mc_item_stack_fields_get(stack, &after) == MC_ITEM_STACK_OK &&
          memcmp(&before, &after, sizeof(before)) == 0);
    return true;
}

static bool foliage_item_icon_controls(fixture *f) {
    static const int32_t metadata[] = {INT32_MIN, -17, -4, -3, -2, -1, 0,        1,
                                       2,         3,   4,  5,  15, 16, INT32_MAX};

    static const struct {
        int32_t metadata, base, leaf;
    } late[] = {{0, -123, -123},
                {1, INT32_MAX, -2147483569},
                {2, INT32_MIN, INT32_MIN},
                {3, INT32_MAX, INT32_MAX},
                {5, INT32_MAX - 79, INT32_MIN},
                {-3, INT32_MIN, -2147483568},
                {-1, INT32_MAX, INT32_MAX},
                {INT32_MIN, INT32_MIN, INT32_MIN},
                {INT32_MAX, INT32_MIN, INT32_MIN}};

    const mc_registry_item_array *items;
    CHECK(mc_registry_item_c_get(f->registry, &items, NULL) == MC_REGISTRY_OK);
    for (unsigned int leaf = 0; leaf < 2; ++leaf) {
        const int32_t id = leaf ? 18 : 6;
        const size_t named = leaf ? 17u : 5u;
        const char *owner = leaf ? "uy" : "mr";
        const mc_item_kind kind = leaf ? MC_ITEM_LEAVES_BLOCK : MC_ITEM_SAPLING_BLOCK;
        const mc_item *item;
        CHECK(mc_registry_item_c_at(f->registry, items, id, &item, NULL) == MC_REGISTRY_OK &&
              item != NULL && item->kind == kind &&
              strcmp(mc_registry_block_static_name(named), leaf ? "L" : "z") == 0);
        mc_item *raw = NULL;
        mc_item_stack *stack = NULL;
        CHECK(mc_registry_item_allocate(f->registry, kind, &raw) == MC_REGISTRY_OK && raw != NULL &&
              raw->phase == MC_ITEM_RAW && mc_item_stack_allocate(&stack) == MC_ITEM_STACK_OK &&
              mc_item_stack_construct(stack, id, 2, 0) == MC_ITEM_STACK_OK);
        raw->fields.bh = INT32_MIN;
        raw->ck_a = INT32_MAX;
        const mc_item raw_before = *raw, item_before = *item;
        mc_block *block = f->registry->blocks.named[named];
        CHECK(block != NULL && block->kind == (leaf ? MC_BLOCK_LEAVES : MC_BLOCK_SAPLING));
        const mc_block block_before = *block;
        const size_t bytes = f->registry->allocation_bytes;
        for (size_t i = 0; i < sizeof(metadata) / sizeof(metadata[0]); ++i) {
            const int32_t variant = metadata[i] & 3;
            const int32_t expected =
                leaf ? (variant == 1 ? block_before.fields.bm + 80 : block_before.fields.bm)
                : variant == 1 ? 63
                : variant == 2 ? 79
                               : block_before.fields.bm;
            CHECK(foliage_item_icon(f, item, raw, stack, owner, metadata[i], expected));
        }
        for (size_t i = 0; i < sizeof(late) / sizeof(late[0]); ++i) {
            block->fields.bm = late[i].base;
            const int32_t variant = late[i].metadata & 3;
            const int32_t expected = leaf           ? late[i].leaf
                                     : variant == 1 ? 63
                                     : variant == 2 ? 79
                                                    : late[i].base;
            const bool returned =
                foliage_item_icon(f, item, raw, stack, owner, late[i].metadata, expected);
            block->fields.bm = block_before.fields.bm;
            CHECK(returned);
        }
        /* MR/UY capture named z/L, independently of the ordinary m[id] cell. */
        mc_block *array_block = f->registry->blocks.m[id];
        f->registry->blocks.m[id] = NULL;
        const bool named_only =
            foliage_item_icon(f, item, raw, stack, owner, 0, block_before.fields.bm);
        f->registry->blocks.m[id] = array_block;
        CHECK(named_only);
        const mc_block *dirt, *log;
        CHECK(mc_registry_block_get(f->registry, 3, &dirt, NULL) == MC_REGISTRY_OK &&
              dirt != NULL &&
              mc_registry_block_get(f->registry, 17, &log, NULL) == MC_REGISTRY_OK && log != NULL);
        f->registry->blocks.m[id] = (mc_block *)(void *)dirt;
        const bool unrelated_array =
            foliage_item_icon(f, item, raw, stack, owner, 0, block_before.fields.bm);
        f->registry->blocks.m[id] = array_block;
        CHECK(unrelated_array);
        f->registry->blocks.named[named] = (mc_block *)(void *)dirt;
        const bool rebound = foliage_item_icon(f, item, raw, stack, owner, 1, dirt->fields.bm);
        f->registry->blocks.named[named] = block;
        CHECK(rebound && foliage_item_icon(f, item, raw, stack, owner, 0, block_before.fields.bm));
        /* Log distinguishes the literal side0 from its metadata-sensitive sides. */
        f->registry->blocks.named[named] = (mc_block *)(void *)log;
        const bool side_zero = foliage_item_icon(f, item, raw, stack, owner, 1, 21);
        f->registry->blocks.named[named] = block;
        CHECK(side_zero);
        mc_item_render_error error;
        int32_t icon = -777;
        f->registry->blocks.named[named] = NULL;
        const bool null_direct = mc_item_render_icon(f->registry, item, -1, &icon, &error);
        f->registry->blocks.named[named] = block;
        CHECK(!null_direct && icon == -777 && !error.returned &&
              error.status == MC_REGISTRY_NULL_REFERENCE && !error.native_policy &&
              error.source_exception && error.throwable == MC_ITEM_RENDER_SOURCE_EXCEPTION &&
              strcmp(error.exception_class, "java/lang/NullPointerException") == 0 &&
              foliage_item_site(&error.site, owner, "a", "(I)I", 5));
        CHECK(mc_item_stack_source_damage_set(stack, -1) == MC_ITEM_STACK_OK);
        f->registry->blocks.named[named] = NULL;
        const bool null_wrapped = mc_item_stack_render_icon(f->registry, stack, &icon, &error);
        f->registry->blocks.named[named] = block;
        CHECK(!null_wrapped && icon == -777 && !error.returned &&
              error.status == MC_REGISTRY_NULL_REFERENCE && !error.native_policy &&
              error.source_exception && error.throwable == MC_ITEM_RENDER_SOURCE_EXCEPTION &&
              strcmp(error.exception_class, "java/lang/NullPointerException") == 0 &&
              foliage_item_site(&error.site, owner, "a", "(I)I", 5) &&
              error.stack_identity == (uintptr_t)stack && error.item_identity == (uintptr_t)item &&
              error.id_read && error.id == id && error.damage_read && error.damage == -1 &&
              error.ancestor_count == 2 &&
              foliage_item_site(&error.ancestors[0], "iz", "b", "()I", 5) &&
              foliage_item_site(&error.ancestors[1], "gm", "b", "(Liz;)I", 5));
        mc_item *saved_item = f->registry->items.c->elements[id];
        f->registry->items.c->elements[id] = NULL;
        const bool null_item = mc_item_stack_render_icon(f->registry, stack, &icon, &error);
        f->registry->items.c->elements[id] = saved_item;
        CHECK(!null_item && icon == -777 && error.status == MC_REGISTRY_NULL_REFERENCE &&
              error.id_read && !error.damage_read && error.item_identity == 0 &&
              error.source_exception && !error.native_policy &&
              foliage_item_site(&error.site, "iz", "b", "()I", 5));
        CHECK(memcmp(raw, &raw_before, sizeof(raw_before)) == 0 &&
              memcmp(item, &item_before, sizeof(item_before)) == 0 &&
              memcmp(block, &block_before, sizeof(block_before)) == 0 &&
              f->registry->allocation_bytes == bytes);
        mc_item_stack_release(stack);
    }
    return true;
}

static bool foliage_item_default_color(fixture *f, const mc_item *item,
                                       mc_client_render_engine_classes *graphics) {
    const mc_item_render_color_context contexts[] = {
        {NULL, mc_client_colorizer_read}, {graphics, NULL}, {graphics, mc_client_colorizer_read}};
    mc_client_colorizer_state before, after;
    mc_client_render_engine_class_state held, released;
    CHECK(mc_client_colorizer_inspect(graphics, MC_CLIENT_COLORIZER_FOLIAGE, &before) &&
          mc_client_render_engine_classes_inspect(graphics, &held));
    for (size_t i = 0; i <= sizeof(contexts) / sizeof(contexts[0]); ++i) {
        int32_t color = -777;
        mc_item_render_error error;
        const mc_item_render_color_context *context =
            i < sizeof(contexts) / sizeof(contexts[0]) ? &contexts[i] : NULL;
        CHECK(mc_item_render_color_with_context(f->registry, item, INT32_MIN, context, &color,
                                                &error) &&
              color == 0xffffff && error.returned && error.status == MC_REGISTRY_OK &&
              !error.native_policy && !error.source_exception &&
              error.throwable == MC_ITEM_RENDER_NO_THROWABLE && !error.colorizer_entered &&
              !error.colorizer_returned && foliage_item_site(&error.site, "gm", "f", "(I)I", 2));
    }
    CHECK(mc_client_colorizer_inspect(graphics, MC_CLIENT_COLORIZER_FOLIAGE, &after) &&
          after.phase == before.phase && after.a == before.a &&
          after.failed_candidate == before.failed_candidate &&
          after.last_store_pc == before.last_store_pc &&
          after.failure.status == before.failure.status &&
          after.failure.operation == before.failure.operation &&
          mc_client_render_engine_classes_inspect(graphics, &released) &&
          released.native_holds == held.native_holds && graphics->active_source_calls == 0 &&
          graphics->active_resource_calls == 0);
    return true;
}

static bool foliage_item_color_controls(fixture *f) {
    static const struct {
        int32_t metadata, color, invoke, returned;
    } cases[] = {{0, 4764952, 22, 25},  {1, 6396257, 7, 10},          {2, 8431445, 18, 21},
                 {3, 6396257, 7, 10},   {4, 4764952, 22, 25},         {5, 6396257, 7, 10},
                 {6, 8431445, 18, 21},  {7, 6396257, 7, 10},          {8, 4764952, 22, 25},
                 {9, 6396257, 7, 10},   {10, 8431445, 18, 21},        {11, 6396257, 7, 10},
                 {12, 4764952, 22, 25}, {13, 6396257, 7, 10},         {14, 8431445, 18, 21},
                 {15, 6396257, 7, 10},  {INT32_MIN, 4764952, 22, 25}, {-4, 4764952, 22, 25},
                 {-3, 6396257, 7, 10},  {-2, 8431445, 18, 21},        {-1, 6396257, 7, 10},
                 {16, 4764952, 22, 25}, {INT32_MAX, 6396257, 7, 10}};

    const mc_registry_item_array *items;
    const mc_item *leaf, *sapling;
    mc_item *raw = NULL;
    CHECK(mc_registry_item_c_get(f->registry, &items, NULL) == MC_REGISTRY_OK &&
          mc_registry_item_c_at(f->registry, items, 18, &leaf, NULL) == MC_REGISTRY_OK &&
          leaf != NULL && leaf->kind == MC_ITEM_LEAVES_BLOCK &&
          mc_registry_item_c_at(f->registry, items, 6, &sapling, NULL) == MC_REGISTRY_OK &&
          sapling != NULL && sapling->kind == MC_ITEM_SAPLING_BLOCK &&
          mc_registry_item_allocate(f->registry, MC_ITEM_LEAVES_BLOCK, &raw) == MC_REGISTRY_OK &&
          raw != NULL && raw->phase == MC_ITEM_RAW);
    raw->fields.bh = INT32_MIN;
    raw->ck_a = INT32_MAX;
    const mc_item raw_before = *raw, leaf_before = *leaf;
    const size_t bytes = f->registry->allocation_bytes;
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        int32_t color = -777;
        mc_item_render_error error;
        CHECK(!mc_item_render_color(f->registry, leaf, cases[i].metadata, &color, &error) &&
              color == -777 && !error.returned &&
              error.status == MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY && error.native_policy &&
              !error.source_exception && error.throwable == MC_ITEM_RENDER_NO_THROWABLE &&
              error.exception_class == NULL && !error.colorizer_entered &&
              !error.colorizer_returned &&
              foliage_item_site(&error.site, "uy", "f", "(I)I", cases[i].invoke));
        mc_client_render_engine_classes *graphics = NULL;
        CHECK(mc_client_render_engine_classes_create(NULL, &graphics) ==
              MC_CLIENT_RENDER_ENGINE_OK);
        mc_client_colorizer_state state;
        mc_client_render_engine_class_state held, released;
        CHECK(mc_client_colorizer_inspect(graphics, MC_CLIENT_COLORIZER_FOLIAGE, &state) &&
              state.phase == MC_CLIENT_COLORIZER_RAW && state.a == NULL &&
              foliage_item_default_color(f, sapling, graphics) &&
              mc_client_render_engine_classes_inspect(graphics, &held));
        const mc_item_render_color_context missing[] = {{NULL, mc_client_colorizer_read},
                                                        {graphics, NULL}};
        for (size_t j = 0; j < sizeof(missing) / sizeof(missing[0]); ++j) {
            CHECK(!mc_item_render_color_with_context(f->registry, leaf, cases[i].metadata,
                                                     &missing[j], &color, &error) &&
                  color == -777 && !error.returned &&
                  error.status == MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY && error.native_policy &&
                  !error.source_exception && error.throwable == MC_ITEM_RENDER_NO_THROWABLE &&
                  !error.colorizer_entered && !error.colorizer_returned &&
                  foliage_item_site(&error.site, "uy", "f", "(I)I", cases[i].invoke) &&
                  mc_client_colorizer_inspect(graphics, MC_CLIENT_COLORIZER_FOLIAGE, &state) &&
                  state.phase == MC_CLIENT_COLORIZER_RAW && state.a == NULL);
        }
        const mc_item_render_color_context context = {graphics, mc_client_colorizer_read};
        /* UY.f owns its JH dispatch; neither ordinary nor named Blocks are operands. */
        mc_block *named = f->registry->blocks.named[17];
        mc_block *array = f->registry->blocks.m[18];
        f->registry->blocks.named[17] = NULL;
        f->registry->blocks.m[18] = NULL;
        const bool returned = mc_item_render_color_with_context(
            f->registry, leaf, cases[i].metadata, &context, &color, &error);
        f->registry->blocks.named[17] = named;
        f->registry->blocks.m[18] = array;
        CHECK(returned && color == cases[i].color && error.returned &&
              error.status == MC_REGISTRY_OK && !error.native_policy && !error.source_exception &&
              error.throwable == MC_ITEM_RENDER_NO_THROWABLE && error.exception_class == NULL &&
              error.colorizer_entered && error.colorizer_returned &&
              error.colorizer.status == MC_CLIENT_COLORIZER_OK &&
              foliage_item_site(&error.site, "uy", "f", "(I)I", cases[i].returned) &&
              foliage_item_site(&error.child, "jh",
                                cases[i].invoke == 7    ? "a"
                                : cases[i].invoke == 18 ? "b"
                                                        : "c",
                                "()I", 2) &&
              mc_client_colorizer_inspect(graphics, MC_CLIENT_COLORIZER_FOLIAGE, &state) &&
              state.phase == MC_CLIENT_COLORIZER_READY && state.a != NULL &&
              state.failed_candidate == NULL && state.last_store_pc == 4);
        mc_client_color_array_state array_state;
        CHECK(mc_client_color_array_inspect(state.a, &array_state) &&
              array_state.phase == MC_CLIENT_COLOR_ARRAY_READY && array_state.length == 65536 &&
              array_state.native_holds == 1 && array_state.active_calls == 0 &&
              mc_client_colorizer_inspect(graphics, MC_CLIENT_COLORIZER_GRASS, &state) &&
              state.phase == MC_CLIENT_COLORIZER_RAW && state.a == NULL &&
              mc_client_colorizer_inspect(graphics, MC_CLIENT_COLORIZER_WATER, &state) &&
              state.phase == MC_CLIENT_COLORIZER_RAW && state.a == NULL);
        mc_client_colorizer_error setter;
        CHECK(mc_client_colorizer_set(graphics, MC_CLIENT_COLORIZER_FOLIAGE, NULL, NULL, &setter) ==
              MC_CLIENT_COLORIZER_OK);
        /* Constant JH bodies ignore both the nullable array and FP controls. */
        color = -777;
        const unsigned int fp = _mm_getcsr();
        _mm_setcsr(fp | 0x8000u);
        const bool raw_returned = mc_item_render_color_with_context(
            f->registry, raw, cases[i].metadata, &context, &color, &error);
        _mm_setcsr(fp);
        CHECK(raw_returned && color == cases[i].color && error.returned &&
              error.colorizer_entered && error.colorizer_returned &&
              foliage_item_site(&error.site, "uy", "f", "(I)I", cases[i].returned) &&
              mc_client_colorizer_inspect(graphics, MC_CLIENT_COLORIZER_FOLIAGE, &state) &&
              state.phase == MC_CLIENT_COLORIZER_READY && state.a == NULL &&
              mc_client_render_engine_classes_inspect(graphics, &released) &&
              released.native_holds == held.native_holds && graphics->active_source_calls == 0 &&
              graphics->active_resource_calls == 0 &&
              mc_client_render_engine_classes_release(graphics) == MC_CLIENT_RENDER_ENGINE_OK);
    }
    CHECK(memcmp(raw, &raw_before, sizeof(raw_before)) == 0 &&
          memcmp(leaf, &leaf_before, sizeof(leaf_before)) == 0 &&
          f->registry->allocation_bytes == bytes);
    return true;
}

static bool foliage_item_color_cause(const mc_client_colorizer_error *actual,
                                     const mc_client_colorizer_error *first) {
    return actual->native_policy == first->native_policy && actual->owner == first->owner &&
           actual->site.class_name == first->site.class_name &&
           actual->site.method == first->site.method &&
           actual->site.descriptor == first->site.descriptor && actual->site.pc == first->site.pc &&
           actual->array.status == first->array.status &&
           actual->array.operation == first->array.operation &&
           actual->array.site.class_name == first->array.site.class_name &&
           actual->array.site.method == first->array.site.method &&
           actual->array.site.descriptor == first->array.site.descriptor &&
           actual->array.site.pc == first->array.site.pc &&
           actual->array.native_policy == first->array.native_policy &&
           actual->array.width == first->array.width &&
           actual->array.height == first->array.height &&
           actual->array.actual_length == first->array.actual_length &&
           actual->array.native_allocations == first->array.native_allocations &&
           actual->array.native_bytes == first->array.native_bytes;
}

static bool foliage_item_color_failure_controls(fixture *f) {
    const mc_registry_item_array *items;
    const mc_item *leaf, *sapling;
    CHECK(mc_registry_item_c_get(f->registry, &items, NULL) == MC_REGISTRY_OK &&
          mc_registry_item_c_at(f->registry, items, 18, &leaf, NULL) == MC_REGISTRY_OK &&
          leaf != NULL &&
          mc_registry_item_c_at(f->registry, items, 6, &sapling, NULL) == MC_REGISTRY_OK &&
          sapling != NULL);
    static const int32_t metadata[] = {0, 1, 2};
    static const int32_t invoke[] = {22, 7, 18};
    for (unsigned int partial = 0; partial < 2; ++partial)
        for (size_t i = 0; i < sizeof(metadata) / sizeof(metadata[0]); ++i) {
            mc_client_render_engine_classes *graphics = NULL;
            CHECK(mc_client_render_engine_classes_create(NULL, &graphics) ==
                  MC_CLIENT_RENDER_ENGINE_OK);
            mc_client_colorizer_options limits = mc_client_colorizer_default_options();
            limits.initial_array[MC_CLIENT_COLORIZER_FOLIAGE] =
                (mc_client_color_array_limits){partial ? SIZE_MAX : 0, partial ? 1u : 0u};
            mc_client_colorizer_error preparation;
            /* A genuine independent IA active use fixes namespace allocation policy
             * without entering JH. The first JH failure belongs to UY below. */
            CHECK(mc_client_colorizer_initialize(graphics, MC_CLIENT_COLORIZER_GRASS, &limits,
                                                 &preparation) == MC_CLIENT_COLORIZER_OK);
            mc_client_colorizer_state before, first, after;
            mc_client_render_engine_class_state held, released;
            CHECK(mc_client_colorizer_inspect(graphics, MC_CLIENT_COLORIZER_FOLIAGE, &before) &&
                  before.phase == MC_CLIENT_COLORIZER_RAW && before.a == NULL &&
                  before.failed_candidate == NULL && before.last_store_pc == 0 &&
                  foliage_item_default_color(f, sapling, graphics) &&
                  mc_client_render_engine_classes_inspect(graphics, &held));
            const mc_item_render_color_context context = {graphics, mc_client_colorizer_read};
            int32_t color = -777;
            mc_item_render_error error;
            CHECK(!mc_item_render_color_with_context(f->registry, leaf, metadata[i], &context,
                                                     &color, &error) &&
                  color == -777 && !error.returned && error.status == MC_REGISTRY_LIMIT &&
                  error.native_policy && !error.source_exception &&
                  error.throwable == MC_ITEM_RENDER_NO_THROWABLE && error.exception_class == NULL &&
                  error.colorizer_entered && !error.colorizer_returned &&
                  error.colorizer.status == MC_CLIENT_COLORIZER_ARRAY_FAILED &&
                  error.colorizer.native_policy &&
                  strcmp(error.colorizer.operation, "actual colorizer zero65536 array") == 0 &&
                  error.colorizer.array.status == MC_CLIENT_COLOR_ARRAY_LIMIT &&
                  error.colorizer.array.native_policy &&
                  strcmp(error.colorizer.array.operation,
                         partial ? "array native plane allocation bound"
                                 : "array native object/plane bound") == 0 &&
                  error.colorizer.array.native_allocations == (partial ? 1u : 0u) &&
                  foliage_item_site(&error.site, "uy", "f", "(I)I", invoke[i]) &&
                  foliage_item_site(&error.child, "jh", "<clinit>", "()V", 2) &&
                  mc_client_colorizer_inspect(graphics, MC_CLIENT_COLORIZER_FOLIAGE, &first) &&
                  first.phase == MC_CLIENT_COLORIZER_FAILED && first.a == NULL &&
                  (first.failed_candidate != NULL) == (partial != 0) && first.last_store_pc == 0 &&
                  first.failure.status == MC_CLIENT_COLORIZER_ARRAY_FAILED &&
                  error.colorizer.operation == first.failure.operation &&
                  foliage_item_color_cause(&error.colorizer, &first.failure));
            if (partial) {
                mc_client_color_array_state candidate;
                CHECK(mc_client_color_array_inspect(first.failed_candidate, &candidate) &&
                      candidate.phase == MC_CLIENT_COLOR_ARRAY_FAILED &&
                      candidate.length == 65536 && candidate.native_holds == 1 &&
                      candidate.active_calls == 0 && candidate.native_allocations == 1 &&
                      candidate.bits == NULL &&
                      candidate.native_bytes == error.colorizer.array.native_bytes &&
                      candidate.failure.status == MC_CLIENT_COLOR_ARRAY_LIMIT);
            } else
                CHECK(error.colorizer.array.native_bytes == 0);
            for (unsigned int attempt = 0; attempt < 2; ++attempt) {
                CHECK(!mc_item_render_color_with_context(f->registry, leaf, metadata[i], &context,
                                                         &color, &error) &&
                      color == -777 && !error.returned &&
                      error.status == MC_REGISTRY_FAILED_INITIALIZATION && error.native_policy &&
                      !error.source_exception && error.throwable == MC_ITEM_RENDER_NO_THROWABLE &&
                      error.exception_class == NULL && error.colorizer_entered &&
                      !error.colorizer_returned &&
                      error.colorizer.status == MC_CLIENT_COLORIZER_CLASS_FAILED &&
                      strcmp(error.colorizer.operation,
                             "previous actual colorizer class initialization failed") == 0 &&
                      foliage_item_site(&error.site, "uy", "f", "(I)I", invoke[i]) &&
                      foliage_item_site(&error.child, "jh", "<clinit>", "()V", 2) &&
                      foliage_item_color_cause(&error.colorizer, &first.failure) &&
                      mc_client_colorizer_inspect(graphics, MC_CLIENT_COLORIZER_FOLIAGE, &after) &&
                      after.phase == first.phase && after.a == first.a &&
                      after.failed_candidate == first.failed_candidate &&
                      after.last_store_pc == first.last_store_pc &&
                      after.failure.status == first.failure.status &&
                      after.failure.operation == first.failure.operation &&
                      foliage_item_color_cause(&after.failure, &first.failure));
            }
            CHECK(foliage_item_default_color(f, sapling, graphics) &&
                  mc_client_render_engine_classes_inspect(graphics, &released) &&
                  released.native_holds == held.native_holds &&
                  graphics->active_source_calls == 0 && graphics->active_resource_calls == 0 &&
                  mc_client_render_engine_classes_release(graphics) == MC_CLIENT_RENDER_ENGINE_OK);
        }
    return true;
}
