/* Real Registry/NW owners with no Display: check the selected inventory
 * leaves and failure prefixes independently of graphical SDK scenarios. */
int mc_inventory_faces_fixture_main(void);
#define main mc_inventory_faces_fixture_main
#include "test_render_blocks_faces.c"
#undef main

#include "block/registry/registry_internal.h"
#include "client/render/item_overlay.h"
#include "item/render/internal.h"
#include "item/stack/source_internal.h"
#include "mc/item/registry/runtime.h"
#include <inttypes.h>

typedef struct durability_rectangle {
    int32_t x, y, width, height, color, before[48], words[48];
    mc_client_nw_fields fields;
} durability_rectangle;

static bool durability_vertices(durability_rectangle *receipt) {
    mc_client_render_engine_classes *graphics = NULL;
    mc_client_tessellator *nw = NULL;
    mc_client_graphics_error error;
    CHECK(mc_client_render_engine_classes_create(NULL, &graphics) == MC_CLIENT_RENDER_ENGINE_OK &&
          mc_client_nw_active_use(graphics,
                                  (mc_client_graphics_site){"native-test", "durability", "()V", 0},
                                  &nw, &error) &&
          mc_client_nw_fields_get(nw, &receipt->fields) && receipt->fields.g != NULL &&
          receipt->fields.B >= 48);
    memcpy(receipt->before, receipt->fields.g, sizeof(receipt->before));
    mc_client_item_overlay_rectangle_context prefix = {.nw = nw,
                                                       .x = receipt->x,
                                                       .y = receipt->y,
                                                       .width = receipt->width,
                                                       .height = receipt->height,
                                                       .color = receipt->color};
    CHECK(mc_client_item_overlay_rectangle_entered(&prefix) && prefix.source_pc == 67 &&
          prefix.vertices == 4 && !prefix.source_exception &&
          mc_client_nw_fields_get(nw, &receipt->fields) && receipt->fields.w &&
          receipt->fields.h == 6 && receipt->fields.p == 4 && receipt->fields.o == 48 &&
          receipt->fields.r == 7 && receipt->fields.l && !receipt->fields.m && !receipt->fields.n);
    memcpy(receipt->words, receipt->fields.g, sizeof(receipt->words));
    for (size_t i = 0; i < 48; ++i)
        if (i % 8 == 3 || i % 8 == 4 || i % 8 == 6 || i % 8 == 7)
            CHECK(receipt->words[i] == receipt->before[i]);
    /* A second actual begin must fail before disturbing the first batch. */
    prefix = (mc_client_item_overlay_rectangle_context){.nw = nw};
    CHECK(!mc_client_item_overlay_rectangle_entered(&prefix) && prefix.source_pc == 1 &&
          prefix.vertices == 0 && !prefix.source_exception &&
          prefix.error.status == MC_CLIENT_GRAPHICS_SOURCE_ILLEGAL_STATE &&
          !prefix.error.native_policy && mc_client_nw_fields_get(nw, &receipt->fields) &&
          receipt->fields.w && receipt->fields.h == 6 && receipt->fields.o == 48 &&
          receipt->fields.p == 4 &&
          memcmp(receipt->words, receipt->fields.g, sizeof(receipt->words)) == 0);
    /* Terminal native namespace retirement does not draw or close the source batch. */
    CHECK(mc_client_render_engine_classes_release(graphics) == MC_CLIENT_RENDER_ENGINE_OK);
    return true;
}

static void durability_words(const int32_t words[48]) {
    putchar('[');
    for (size_t i = 0; i < 48; ++i)
        printf("%s\"%08" PRIx32 "\"", i == 0 ? "" : ",", (uint32_t)words[i]);
    putchar(']');
}

static bool durability_controls(mc_registry_classes *registry, mc_client_item_renderer *renderer) {
    static const struct {
        const char *profile;
        int32_t id, damage, maximum, width, intensity;
    } cases[] = {{"iron-pristine", 267, 0, 250, 0, 0},       {"iron-one", 267, 1, 250, 13, 254},
                 {"iron-half", 267, 125, 250, 7, 128},       {"iron-last", 267, 249, 250, 0, 1},
                 {"iron-maximum", 267, 250, 250, 0, 0},      {"iron-over", 267, 251, 250, 0, -1},
                 {"diamond-maximum", 276, 1561, 1561, 0, 0}, {"shovel-one", 256, 1, 250, 13, 254},
                 {"shovel-maximum", 256, 250, 250, 0, 0}};

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        mc_item_stack *stack = NULL;
        CHECK(mc_item_stack_allocate(&stack) == MC_ITEM_STACK_OK &&
              mc_item_stack_construct(stack, cases[i].id, 1, cases[i].damage) == MC_ITEM_STACK_OK);
        mc_client_item_overlay_context overlay = {
            .owner = renderer, .registry = registry, .stack = stack, .x = 7, .y = 11};
        const bool returned = mc_client_item_overlay_draw(&overlay);
        const bool damaged = cases[i].damage > 0;
        CHECK(overlay.count_read && !overlay.count_drawn && overlay.durability_returned &&
              overlay.maximum == cases[i].maximum && overlay.damage_read &&
              overlay.damage == cases[i].damage && overlay.damaged == damaged &&
              overlay.bar_rectangles == 0 && overlay.bar_vertices == 0 && !overlay.bar_returned);
        if (damaged) {
            CHECK(!returned && !overlay.returned && overlay.source_pc == 150 &&
                  overlay.native_policy && overlay.bar_samples == 2 &&
                  overlay.bar_width == cases[i].width &&
                  overlay.bar_intensity == cases[i].intensity);
            for (size_t sample = 0; sample < 2; ++sample)
                CHECK(overlay.bar_damage[sample] == cases[i].damage &&
                      overlay.bar_maximum[sample] == cases[i].maximum);
        } else
            CHECK(returned && overlay.returned && overlay.source_pc == 288 &&
                  overlay.bar_samples == 0);
        durability_rectangle rectangles[3] = {0};
        if (damaged) {
            int32_t color, shade;
            mc_client_item_overlay_bar_colors(overlay.bar_intensity, &color, &shade);
            rectangles[0] = (durability_rectangle){.x = 9, .y = 24, .width = 13, .height = 2};
            rectangles[1] =
                (durability_rectangle){.x = 9, .y = 24, .width = 12, .height = 1, .color = shade};
            rectangles[2] = (durability_rectangle){
                .x = 9, .y = 24, .width = overlay.bar_width, .height = 1, .color = color};
            for (size_t rectangle = 0; rectangle < 3; ++rectangle)
                CHECK(durability_vertices(&rectangles[rectangle]));
        }
        printf("ITEM_DURABILITY {\"scope\":\"entered-rectangle-prefix\",\"profile\":\"%s\","
               "\"id\":%d,\"damage\":%d,\"max\":%d,\"x\":7,\"y\":11,\"draws\":0,"
               "\"rectangle_prefixes\":%d,\"damageable\":true,\"damaged\":%s,"
               "\"samples\":[{\"damage\":%d,\"maximum\":%d}",
               cases[i].profile, cases[i].id, cases[i].damage, overlay.maximum, damaged ? 3 : 0,
               damaged ? "true" : "false", overlay.damage, overlay.maximum);
        for (size_t sample = 0; sample < overlay.bar_samples; ++sample)
            printf(",{\"damage\":%d,\"maximum\":%d}", overlay.bar_damage[sample],
                   overlay.bar_maximum[sample]);
        if (damaged)
            printf("],\"bar_width\":%d,\"bar_intensity\":%d,\"rectangles\":[", overlay.bar_width,
                   overlay.bar_intensity);
        else
            printf("],\"bar_width\":null,\"bar_intensity\":null,\"rectangles\":[");
        for (size_t rectangle = 0; rectangle < (damaged ? 3u : 0u); ++rectangle) {
            const durability_rectangle *r = &rectangles[rectangle];
            printf("%s{\"x\":%d,\"y\":%d,\"width\":%d,\"height\":%d,\"color\":%d,"
                   "\"h\":%d,\"p\":%d,\"o\":%d,\"r\":%d,\"l\":true,\"m\":false,\"n\":false,"
                   "\"before\":",
                   rectangle == 0 ? "" : ",", r->x, r->y, r->width, r->height, r->color,
                   r->fields.h, r->fields.p, r->fields.o, r->fields.r);
            durability_words(r->before);
            printf(",\"words\":");
            durability_words(r->words);
            putchar('}');
        }
        puts("]}");
        mc_item_stack_release(stack);
    }
    mc_client_item_overlay_rectangle_context null_nw = {0};
    CHECK(!mc_client_item_overlay_rectangle_entered(&null_nw) && null_nw.source_pc == 1 &&
          null_nw.vertices == 0 && null_nw.source_exception &&
          strcmp(null_nw.exception_class, "java/lang/NullPointerException") == 0);
    durability_rectangle wrapped = {
        .x = INT32_MAX, .y = INT32_MAX, .width = 1, .height = 2, .color = INT32_MIN};
    CHECK(durability_vertices(&wrapped) && wrapped.words[0] == 1325400064 &&
          wrapped.words[1] == 1325400064 && wrapped.words[8] == 1325400064 &&
          wrapped.words[9] == -822083584 && wrapped.words[16] == -822083584 &&
          wrapped.words[17] == -822083584);

    mc_item_stack *raw_stack = NULL;
    CHECK(mc_item_stack_allocate(&raw_stack) == MC_ITEM_STACK_OK && registry->items.c != NULL &&
          registry->items.c->length > 0);
    mc_item *saved = registry->items.c->elements[0];
    for (int kind = 0; kind < MC_ITEM_KIND_COUNT; ++kind) {
        mc_item *raw = NULL;
        /* The private allocator belongs to an admitted native initialization
         * flow, including the already initialized GM base Class. End that
         * preparation before entering the source-transparent overlay leaf. */
        ++registry->entered;
        const mc_registry_status allocated =
            mc_registry_item_allocate(registry, (mc_item_kind)kind, &raw);
        --registry->entered;
        CHECK(allocated == MC_REGISTRY_OK && raw != NULL && raw->phase == MC_ITEM_RAW);
        raw->fields.a = 23;
        raw->fields.bj = true;
        registry->items.c->elements[0] = raw;
        mc_client_item_overlay_context overlay = {
            .owner = renderer, .registry = registry, .stack = raw_stack};
        CHECK(mc_client_item_overlay_draw(&overlay) && overlay.returned && overlay.maximum == 23 &&
              overlay.damage_read && !overlay.damaged && raw->phase == MC_ITEM_RAW &&
              raw->fields.bj && raw->fields.a == 23);
        CHECK(mc_item_stack_source_damage_set(raw_stack, 1) == MC_ITEM_STACK_OK);
        for (int32_t limit = 0; limit >= -1; --limit) {
            raw->fields.a = limit;
            overlay = (mc_client_item_overlay_context){
                .owner = renderer, .registry = registry, .stack = raw_stack};
            CHECK(mc_client_item_overlay_draw(&overlay) && overlay.returned &&
                  overlay.maximum == limit && !overlay.damage_read && !overlay.damaged &&
                  !overlay.bar_samples && raw->phase == MC_ITEM_RAW);
        }
        CHECK(mc_item_stack_source_damage_set(raw_stack, 0) == MC_ITEM_STACK_OK);
    }
    mc_item *raw = registry->items.c->elements[0];
    raw->fields.a = 1;
    CHECK(mc_item_stack_source_damage_set(raw_stack, INT32_MAX) == MC_ITEM_STACK_OK);
    mc_client_item_overlay_context extreme = {
        .owner = renderer, .registry = registry, .stack = raw_stack};
    CHECK(!mc_client_item_overlay_draw(&extreme) && extreme.source_pc == 150 &&
          extreme.bar_samples == 2 && extreme.bar_width == -2147483622 &&
          extreme.bar_intensity == -2147483138);
    registry->items.c->elements[0] = NULL;
    extreme = (mc_client_item_overlay_context){
        .owner = renderer, .registry = registry, .stack = raw_stack};
    CHECK(!mc_client_item_overlay_draw(&extreme) && extreme.source_pc == 91 &&
          extreme.child.pc == 8 && strcmp(extreme.child.method, "e") == 0 &&
          extreme.source_exception && !extreme.native_policy && !extreme.damage_read &&
          strcmp(extreme.exception_class, "java/lang/NullPointerException") == 0);
    registry->items.c->elements[0] = saved;
    mc_item_stack_phase phase;
    CHECK(mc_item_stack_phase_get(raw_stack, &phase) == MC_ITEM_STACK_OK &&
          phase == MC_ITEM_STACK_RAW);
    mc_item_stack_release(raw_stack);
    return true;
}

static bool inherited_item_controls(mc_registry_classes *registry) {
    static const struct {
        mc_item_kind kind;
        const char *full_3d_owner;
        bool literal_full_3d, rotate;
    } cases[] = {
        {MC_ITEM_SHOVEL, "dq", true, false},  {MC_ITEM_PICKAXE, "dq", true, false},
        {MC_ITEM_AXE, "dq", true, false},     {MC_ITEM_SWORD, "qd", true, false},
        {MC_ITEM_HOE, "la", true, false},     {MC_ITEM_FISHING_ROD, "cs", true, true},
        {MC_ITEM_BOW, "gm", false, false},    {MC_ITEM_MAP, "gm", false, false},
        {MC_ITEM_SHEARS, "gm", false, false}, {MC_ITEM_FLINT_AND_STEEL, "gm", false, false}};

    static const int32_t damage[] = {INT32_MIN, 0, 15, INT32_MAX};
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        mc_item *raw = NULL;
        CHECK(mc_registry_item_allocate(registry, cases[i].kind, &raw) == MC_REGISTRY_OK &&
              raw != NULL && raw->phase == MC_ITEM_RAW);
        raw->fields.bh = 193 + (int32_t)i;
        raw->fields.bi = false;
        const mc_item before = *raw;
        const size_t bytes = registry->allocation_bytes;
        const mc_registry_class_state declaring = registry->item_states[cases[i].kind];
        for (size_t j = 0; j < sizeof(damage) / sizeof(damage[0]); ++j) {
            int32_t icon = -77, color = -77;
            bool flag = true;
            mc_item_render_error error;
            CHECK(mc_item_render_icon(registry, raw, damage[j], &icon, &error) && error.returned &&
                  error.site.pc == 4 && strcmp(error.site.class_name, "gm") == 0 &&
                  icon == before.fields.bh);
            CHECK(mc_item_render_color(registry, raw, damage[j], &color, &error) &&
                  error.returned && error.site.pc == 2 &&
                  strcmp(error.site.class_name, "gm") == 0 && color == 0xffffff);
            CHECK(mc_item_render_full_3d(registry, raw, &flag, &error) && error.returned &&
                  flag == cases[i].literal_full_3d &&
                  strcmp(error.site.class_name, cases[i].full_3d_owner) == 0 &&
                  error.site.pc == (cases[i].literal_full_3d ? 1 : 4));
            CHECK(mc_item_render_rotate(registry, raw, &flag, &error) && error.returned &&
                  flag == cases[i].rotate && error.site.pc == 1 &&
                  strcmp(error.site.class_name, cases[i].rotate ? "cs" : "gm") == 0);
        }
        CHECK(memcmp(&before, raw, sizeof(before)) == 0 && registry->allocation_bytes == bytes &&
              memcmp(&declaring, &registry->item_states[cases[i].kind], sizeof(declaring)) == 0);
        raw->fields.bi = true;
        raw->fields.bh = INT32_MIN;
        bool flag = false;
        int32_t icon = 77;
        mc_item_render_error error;
        CHECK(mc_item_render_full_3d(registry, raw, &flag, &error) && flag &&
              mc_item_render_icon(registry, raw, INT32_MAX, &icon, &error) && icon == INT32_MIN &&
              raw->phase == MC_ITEM_RAW);
        icon = 77;
        CHECK(!mc_item_render_icon(registry, NULL, 0, &icon, &error) && icon == 77 &&
              error.status == MC_REGISTRY_INVALID_ARGUMENT && error.site.pc == -1 &&
              !error.source_exception);
        CHECK(!mc_item_render_icon(registry, raw, 0, NULL, &error) &&
              error.status == MC_REGISTRY_INVALID_ARGUMENT && error.site.pc == -1);
        CHECK(!mc_item_render_icon(registry, raw, 0, &icon, NULL) && icon == 77);
        registry->entered = 1;
        const bool busy = !mc_item_render_icon(registry, raw, 0, &icon, &error);
        registry->entered = 0;
        CHECK(busy && icon == 77 && error.status == MC_REGISTRY_BUSY && error.site.pc == -1);
    }

    static const struct {
        mc_item_kind kind;
        const char *icon_owner;
    } overrides[] = {
        {MC_ITEM_DYE, "km"}, {MC_ITEM_SAPLING_BLOCK, "mr"}, {MC_ITEM_LEAVES_BLOCK, "uy"}};

    for (size_t i = 0; i < sizeof(overrides) / sizeof(overrides[0]); ++i) {
        mc_item *raw = NULL;
        CHECK(mc_registry_item_allocate(registry, overrides[i].kind, &raw) == MC_REGISTRY_OK &&
              raw != NULL && raw->phase == MC_ITEM_RAW);
        raw->fields.bh = 109;
        mc_item_render_error error;
        int32_t value = 77;
        CHECK(!mc_item_render_icon(registry, raw, 15, &value, &error) && value == 77 &&
              error.status == MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY && error.site.pc == 0 &&
              strcmp(error.site.class_name, overrides[i].icon_owner) == 0);
        if (overrides[i].kind == MC_ITEM_LEAVES_BLOCK)
            CHECK(!mc_item_render_color(registry, raw, 0, &value, &error) && value == 77 &&
                  error.status == MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY &&
                  strcmp(error.site.class_name, "uy") == 0);
        else
            CHECK(mc_item_render_color(registry, raw, 0, &value, &error) && value == 0xffffff &&
                  strcmp(error.site.class_name, "gm") == 0);
    }
    return true;
}

static bool log_item_getter_controls(fixture *f) {
    static const int32_t metadata[] = {INT32_MIN, -17, -1, 0, 1, 2, 3, 15, 16, INT32_MAX};
    static const int32_t wool_textures[16] = {64,  210, 194, 178, 162, 146, 130, 114,
                                              225, 209, 193, 177, 161, 145, 129, 113};
    const mc_registry_item_array *items = NULL;
    const mc_item *item = NULL;
    CHECK(mc_registry_item_c_get(f->registry, &items, NULL) == MC_REGISTRY_OK &&
          mc_registry_item_c_at(f->registry, items, 17, &item, NULL) == MC_REGISTRY_OK &&
          item != NULL && item->kind == MC_ITEM_LOG_BLOCK);
    mc_item *raw = NULL;
    CHECK(mc_registry_item_allocate(f->registry, MC_ITEM_LOG_BLOCK, &raw) == MC_REGISTRY_OK &&
          raw != NULL && raw->phase == MC_ITEM_RAW);
    raw->fields.bh = -909;
    raw->ck_a = INT32_MIN;
    const mc_item before = *raw;
    mc_block *actual_k = f->registry->blocks.named[16];
    CHECK(actual_k != NULL && actual_k->kind == MC_BLOCK_LOG &&
          strcmp(mc_registry_block_static_name(16), "K") == 0);
    const size_t bytes = f->registry->allocation_bytes;
    mc_item_render_error error;
    for (size_t i = 0; i < sizeof(metadata) / sizeof(metadata[0]); ++i) {
        const int32_t expected = metadata[i] == 1 ? 116 : metadata[i] == 2 ? 117 : 20;
        int32_t icon = -777;
        CHECK(mc_item_render_icon(f->registry, item, metadata[i], &icon, &error) &&
              icon == expected && error.returned && error.site.pc == 8 &&
              strcmp(error.site.class_name, "go") == 0);
        CHECK(mc_item_render_icon(f->registry, raw, metadata[i], &icon, &error) &&
              icon == expected && error.returned && error.site.pc == 8 && !error.source_exception &&
              error.throwable == MC_ITEM_RENDER_NO_THROWABLE &&
              memcmp(raw, &before, sizeof(before)) == 0 && f->registry->allocation_bytes == bytes);
    }
    /* K is a fresh source static capture, independent of the m[17] cell. */
    mc_block *actual_m17 = f->registry->blocks.m[17];
    f->registry->blocks.m[17] = NULL;
    int32_t icon = -777;
    const bool named_only = mc_item_render_icon(f->registry, item, 1, &icon, &error);
    f->registry->blocks.m[17] = actual_m17;
    CHECK(named_only && icon == 116);
    f->registry->blocks.named[16] = NULL;
    icon = -777;
    const bool null_k = mc_item_render_icon(f->registry, item, INT32_MIN, &icon, &error);
    f->registry->blocks.named[16] = actual_k;
    CHECK(!null_k && icon == -777 && !error.returned && error.site.pc == 5 &&
          strcmp(error.site.class_name, "go") == 0 && error.status == MC_REGISTRY_NULL_REFERENCE &&
          error.source_exception && !error.native_policy &&
          error.throwable == MC_ITEM_RENDER_SOURCE_EXCEPTION &&
          strcmp(error.exception_class, "java/lang/NullPointerException") == 0);
    const mc_block *wool = NULL, *dirt = NULL, *bed = NULL;
    CHECK(mc_registry_block_get(f->registry, 35, &wool, NULL) == MC_REGISTRY_OK && wool != NULL &&
          mc_registry_block_get(f->registry, 3, &dirt, NULL) == MC_REGISTRY_OK && dirt != NULL &&
          mc_registry_block_get(f->registry, 26, &bed, NULL) == MC_REGISTRY_OK && bed != NULL);
    f->registry->blocks.named[16] = (mc_block *)(void *)wool;
    for (int32_t m = 0; m < 16; ++m) {
        /* Unlike BI.a, GO passes metadata through without EE.d inversion. */
        icon = -777;
        const bool returned = mc_item_render_icon(f->registry, item, m, &icon, &error);
        CHECK(returned && icon == wool_textures[m] && error.site.pc == 8);
    }
    CHECK(mc_item_render_icon(f->registry, item, 16, &icon, &error) && icon == 226);
    f->registry->blocks.named[16] = (mc_block *)(void *)dirt;
    CHECK(mc_item_render_icon(f->registry, item, INT32_MAX, &icon, &error) &&
          icon == dirt->fields.bm && error.site.pc == 8);
    f->registry->blocks.named[16] = (mc_block *)(void *)bed;
    icon = -777;
    const bool frontier = mc_item_render_icon(f->registry, item, 0, &icon, &error);
    f->registry->blocks.named[16] = actual_k;
    CHECK(!frontier && icon == -777 && !error.returned && error.site.pc == 5 &&
          strcmp(error.site.class_name, "go") == 0 &&
          error.status == MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY && error.native_policy &&
          error.throwable == MC_ITEM_RENDER_NO_THROWABLE && error.exception_class == NULL);
    CHECK(mc_item_render_icon(f->registry, item, 2, &icon, &error) && icon == 117 &&
          error.site.pc == 8 && f->registry->blocks.named[16] == actual_k &&
          memcmp(raw, &before, sizeof(before)) == 0);
    return true;
}

static bool slab_item_getter_controls(fixture *f) {
    static const int32_t metadata[] = {INT32_MIN, -17, -1, 0, 1, 2, 3, 15, 16, INT32_MAX};
    const mc_registry_item_array *items = NULL;
    const mc_item *item = NULL;
    CHECK(mc_registry_item_c_get(f->registry, &items, NULL) == MC_REGISTRY_OK &&
          mc_registry_item_c_at(f->registry, items, 44, &item, NULL) == MC_REGISTRY_OK &&
          item != NULL && item->kind == MC_ITEM_SLAB_BLOCK);
    mc_item *raw = NULL;
    CHECK(mc_registry_item_allocate(f->registry, MC_ITEM_SLAB_BLOCK, &raw) == MC_REGISTRY_OK &&
          raw != NULL && raw->phase == MC_ITEM_RAW);
    raw->fields.bh = -909;
    raw->ck_a = INT32_MIN;
    const mc_item before = *raw;
    mc_block *actual_al = f->registry->blocks.named[43];
    CHECK(actual_al != NULL && actual_al->kind == MC_BLOCK_SLAB && actual_al->fields.bn == 44 &&
          strcmp(mc_registry_block_static_name(43), "al") == 0);
    const mc_block actual_before = *actual_al;
    const mc_registry_class_state declaring = f->registry->block_states[MC_BLOCK_SLAB];
    const size_t bytes = f->registry->allocation_bytes;
    mc_item_render_error error;
    for (size_t i = 0; i < sizeof(metadata) / sizeof(metadata[0]); ++i) {
        const int32_t expected = metadata[i] == 0   ? 5
                                 : metadata[i] == 1 ? 192
                                 : metadata[i] == 2 ? 4
                                 : metadata[i] == 3 ? 16
                                                    : 6;
        int32_t icon = -777;
        CHECK(mc_item_render_icon(f->registry, item, metadata[i], &icon, &error) &&
              icon == expected && error.returned && error.site.pc == 8 &&
              strcmp(error.site.class_name, "en") == 0);
        CHECK(mc_item_render_icon(f->registry, raw, metadata[i], &icon, &error) &&
              icon == expected && error.returned && error.site.pc == 8 && !error.source_exception &&
              error.throwable == MC_ITEM_RENDER_NO_THROWABLE &&
              memcmp(raw, &before, sizeof(before)) == 0 &&
              memcmp(actual_al, &actual_before, sizeof(actual_before)) == 0 &&
              memcmp(&declaring, &f->registry->block_states[MC_BLOCK_SLAB], sizeof(declaring)) ==
                  0 &&
              f->registry->allocation_bytes == bytes);
    }
    /* EN loads the named singleton, independently of the ordinary m[44] cell. */
    mc_block *actual_m44 = f->registry->blocks.m[44];
    f->registry->blocks.m[44] = NULL;
    int32_t icon = -777;
    const bool named_only = mc_item_render_icon(f->registry, item, 1, &icon, &error);
    f->registry->blocks.m[44] = actual_m44;
    CHECK(named_only && icon == 192);
    f->registry->blocks.named[43] = NULL;
    icon = -777;
    const bool null_al = mc_item_render_icon(f->registry, item, INT32_MIN, &icon, &error);
    f->registry->blocks.named[43] = actual_al;
    CHECK(!null_al && icon == -777 && !error.returned && error.site.pc == 5 &&
          strcmp(error.site.class_name, "en") == 0 && error.status == MC_REGISTRY_NULL_REFERENCE &&
          error.source_exception && !error.native_policy &&
          error.throwable == MC_ITEM_RENDER_SOURCE_EXCEPTION &&
          strcmp(error.exception_class, "java/lang/NullPointerException") == 0);
    const mc_block *wool = NULL, *dirt = NULL, *bed = NULL;
    CHECK(mc_registry_block_get(f->registry, 35, &wool, NULL) == MC_REGISTRY_OK && wool != NULL &&
          mc_registry_block_get(f->registry, 3, &dirt, NULL) == MC_REGISTRY_OK && dirt != NULL &&
          mc_registry_block_get(f->registry, 26, &bed, NULL) == MC_REGISTRY_OK && bed != NULL);
    f->registry->blocks.named[43] = (mc_block *)(void *)wool;
    CHECK(mc_item_render_icon(f->registry, item, 2, &icon, &error) && icon == 194 &&
          mc_item_render_icon(f->registry, item, 16, &icon, &error) && icon == 226);
    /* A fresh different receiver must use its live signed field, not the Slab
     * Item id, its ck.a field, the original singleton or a cached texture. */
    const int32_t original_bm = dirt->fields.bm;
    f->registry->blocks.named[43] = (mc_block *)(void *)dirt;
    static const int32_t textures[] = {INT32_MIN, -1, INT32_MAX};
    for (size_t i = 0; i < sizeof(textures) / sizeof(textures[0]); ++i) {
        ((mc_block *)(void *)dirt)->fields.bm = textures[i];
        CHECK(mc_item_render_icon(f->registry, raw, metadata[i], &icon, &error) &&
              icon == textures[i] && error.returned && error.site.pc == 8);
    }
    ((mc_block *)(void *)dirt)->fields.bm = original_bm;
    f->registry->blocks.named[43] = (mc_block *)(void *)bed;
    icon = -777;
    const bool frontier = mc_item_render_icon(f->registry, item, 0, &icon, &error);
    f->registry->blocks.named[43] = actual_al;
    CHECK(!frontier && icon == -777 && !error.returned && error.site.pc == 5 &&
          strcmp(error.site.class_name, "en") == 0 &&
          error.status == MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY && error.native_policy &&
          error.throwable == MC_ITEM_RENDER_NO_THROWABLE && error.exception_class == NULL);
    CHECK(mc_item_render_icon(f->registry, item, 3, &icon, &error) && icon == 16 &&
          error.site.pc == 8 && f->registry->blocks.named[43] == actual_al &&
          memcmp(raw, &before, sizeof(before)) == 0);
    return true;
}

static mc_registry_status log_missing_stream(void *context, const char *name,
                                             mc_java_properties_input *out) {
    (void)name;
    (void)out;
    ++*(size_t *)context;
    return MC_REGISTRY_NULL_REFERENCE; /* Actual absent Class resource stream. */
}

static bool log_item_initializer_controls(fixture *f) {
    for (unsigned int mode = 0; mode < 2; ++mode) {
        size_t opens = 0;
        const mc_registry_resources registry_resources = {.context = &opens,
                                                          .open = log_missing_stream};
        const mc_registry_options options = {MC_REGISTRY_CLIENT, f->profile, f->materials, 0,
                                             mode == 0 ? &registry_resources : NULL};
        mc_registry_classes *registry = NULL;
        mc_registry_error first;
        CHECK(mc_registry_classes_allocate(&options, &registry, NULL) == MC_REGISTRY_OK &&
              mc_registry_classes_bind_entities(registry, f->entities, NULL) == MC_REGISTRY_OK &&
              mc_registry_classes_initialize_items(registry, &first) ==
                  (mode == 0 ? MC_REGISTRY_NULL_REFERENCE : MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY));
        /* Starting with GM lets its tool Block lookup initialize UU while GM
         * is already entered. UU's recursive GM use then returns normally,
         * so it publishes GO before the real statistics/Translation failure.
         * Starting with UU instead fails inside GM before publishing GO.
         * Inspection does not re-enter either erroneous declaring class. */
        CHECK(registry->blocks_phase == MC_REGISTRY_FAILED && registry->items.c != NULL &&
              registry->items.c->elements != NULL && registry->items.c->length > 17);
        const mc_item *item = registry->items.c->elements[17];
        CHECK(item != NULL && item->kind == MC_ITEM_LOG_BLOCK &&
              item->phase == MC_ITEM_CONSTRUCTED && opens == (mode == 0 ? 1u : 0u));
        for (unsigned int attempt = 0; attempt < 2; ++attempt) {
            mc_item_render_error error;
            int32_t icon = -777;
            CHECK(!mc_item_render_icon(registry, item, 1, &icon, &error) && icon == -777 &&
                  !error.returned && error.site.pc == 0 &&
                  strcmp(error.site.class_name, "go") == 0 &&
                  error.status == MC_REGISTRY_FAILED_INITIALIZATION && !error.source_exception &&
                  error.native_policy == (mode != 0) &&
                  error.throwable ==
                      (mode == 0 ? MC_ITEM_RENDER_SOURCE_ERROR : MC_ITEM_RENDER_NO_THROWABLE) &&
                  opens == (mode == 0 ? 1u : 0u) && first.message == error.registry.message &&
                  first.active_initializer_pc == error.registry.active_initializer_pc);
            if (mode == 0)
                CHECK(error.exception_class != NULL &&
                      strcmp(error.exception_class, "java/lang/NoClassDefFoundError") == 0);
            else
                CHECK(error.exception_class == NULL);
        }
        CHECK(mc_registry_classes_free(registry, NULL) == MC_REGISTRY_OK);
    }
    return true;
}

static bool slab_item_initializer_controls(fixture *f) {
    for (unsigned int mode = 0; mode < 2; ++mode) {
        size_t opens = 0;
        const mc_registry_resources registry_resources = {.context = &opens,
                                                          .open = log_missing_stream};
        const mc_registry_options options = {MC_REGISTRY_CLIENT, f->profile, f->materials, 0,
                                             mode == 0 ? &registry_resources : NULL};
        mc_registry_classes *registry = NULL;
        mc_registry_error first;
        CHECK(mc_registry_classes_allocate(&options, &registry, NULL) == MC_REGISTRY_OK &&
              mc_registry_classes_bind_entities(registry, f->entities, NULL) == MC_REGISTRY_OK &&
              mc_registry_classes_initialize_items(registry, &first) ==
                  (mode == 0 ? MC_REGISTRY_NULL_REFERENCE : MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY));
        /* This real recursive initialization publishes EN before the later
         * statistics/Translation failure. Its getstatic must now observe UU's
         * cached failure, without re-entering the failed dependency. */
        CHECK(registry->blocks_phase == MC_REGISTRY_FAILED && registry->items.c != NULL &&
              registry->items.c->elements != NULL && registry->items.c->length > 44);
        const mc_item *item = registry->items.c->elements[44];
        CHECK(item != NULL && item->kind == MC_ITEM_SLAB_BLOCK &&
              item->phase == MC_ITEM_CONSTRUCTED);
        const mc_item before = *item;
        for (unsigned int attempt = 0; attempt < 2; ++attempt) {
            mc_item_render_error error;
            int32_t icon = -777;
            CHECK(!mc_item_render_icon(registry, item, 2, &icon, &error) && icon == -777 &&
                  !error.returned && error.site.pc == 0 &&
                  strcmp(error.site.class_name, "en") == 0 &&
                  error.status == MC_REGISTRY_FAILED_INITIALIZATION && !error.source_exception &&
                  error.native_policy == (mode != 0) &&
                  error.throwable ==
                      (mode == 0 ? MC_ITEM_RENDER_SOURCE_ERROR : MC_ITEM_RENDER_NO_THROWABLE) &&
                  opens == (mode == 0 ? 1u : 0u) && registry->blocks_phase == MC_REGISTRY_FAILED &&
                  memcmp(item, &before, sizeof(before)) == 0 &&
                  first.message == error.registry.message &&
                  first.active_initializer_class == error.registry.active_initializer_class &&
                  first.active_initializer_pc == error.registry.active_initializer_pc);
            if (mode == 0)
                CHECK(error.exception_class != NULL &&
                      strcmp(error.exception_class, "java/lang/NoClassDefFoundError") == 0);
            else
                CHECK(error.exception_class == NULL);
        }
        CHECK(mc_registry_classes_free(registry, NULL) == MC_REGISTRY_OK);
    }
    return true;
}

static bool pumpkin_getter_controls(fixture *f) {
    static const int32_t metadata[] = {INT32_MIN, -1, 0, 1, 2, 3, 4, 15, 16, INT32_MAX};
    /* The original FC metadata orientation table, without metadata masking. */
    static const int32_t front_side[] = {-9, -9, 3, 4, 2, 5, -9, -9, -9, -9};
    static const int32_t sides[] = {INT32_MIN, -1, 0, 1, 2, 3, 4, 5, 6, INT32_MAX};

    static const struct {
        int32_t base, side, unlit_front, lit_front;
    } wrapped[] = {{INT32_MAX, -2147483633, -2147483632, -2147483631},
                   {INT32_MIN, -2147483632, -2147483631, -2147483630},
                   {-17, -1, 0, 1},
                   {0, 16, 17, 18},
                   {102, 118, 119, 120}};

    for (int32_t lit = 0; lit < 2; ++lit) {
        const mc_block *read = NULL;
        CHECK(mc_registry_block_get(f->registry, lit == 0 ? 86 : 91, &read, NULL) ==
                  MC_REGISTRY_OK &&
              read != NULL && read->kind == MC_BLOCK_PUMPKIN &&
              strcmp(read->constructor_class, "fc") == 0 && read->fields.bm == 102 &&
              read->subtype.fc.a == (lit != 0));
        mc_block *pumpkin = (mc_block *)(void *)read;
        CHECK(mc_block_render_bounds(pumpkin, .125f, .25f, .375f, .75f, .875f, 1) ==
              MC_REGISTRY_OK);
        const mc_block_fields before = pumpkin->fields;
        for (size_t m = 0; m < sizeof(metadata) / sizeof(metadata[0]); ++m) {
            int32_t color = -777;
            CHECK(mc_block_render_inventory_color(pumpkin, metadata[m], &color) == MC_REGISTRY_OK &&
                  color == 0xffffff && mc_block_render_inventory_shape(pumpkin) == MC_REGISTRY_OK &&
                  memcmp(&before, &pumpkin->fields, sizeof(before)) == 0);
            for (size_t s = 0; s < sizeof(sides) / sizeof(sides[0]); ++s) {
                int32_t texture = -777;
                const int32_t expected = sides[s] == 0 || sides[s] == 1 ? 102
                                         : sides[s] == front_side[m]    ? 119 + lit
                                                                        : 118;
                CHECK(mc_block_render_texture_metadata(pumpkin, sides[s], metadata[m], &texture) ==
                          MC_REGISTRY_OK &&
                      texture == expected);
                /* FC.a(I) deliberately ignores its lit flag and metadata. */
                const int32_t side_only = sides[s] == 0 || sides[s] == 1 ? 102
                                          : sides[s] == 3                ? 119
                                                                         : 118;
                CHECK(mc_registry_block_texture_side(f->registry, pumpkin, sides[s], &texture) ==
                          MC_REGISTRY_OK &&
                      texture == side_only);
            }
        }
        for (size_t n = 0; n < sizeof(wrapped) / sizeof(wrapped[0]); ++n) {
            /* Declared live source field inputs, prepared between calls. */
            pumpkin->fields.bm = wrapped[n].base;
            int32_t texture = -777;
            CHECK(mc_block_render_texture_metadata(pumpkin, 0, 0, &texture) == MC_REGISTRY_OK &&
                  texture == wrapped[n].base);
            CHECK(mc_block_render_texture_metadata(pumpkin, -1, 0, &texture) == MC_REGISTRY_OK &&
                  texture == wrapped[n].side);
            CHECK(mc_block_render_texture_metadata(pumpkin, 3, 0, &texture) == MC_REGISTRY_OK &&
                  texture == (lit == 0 ? wrapped[n].unlit_front : wrapped[n].lit_front));
            CHECK(mc_registry_block_texture_side(f->registry, pumpkin, 3, &texture) ==
                      MC_REGISTRY_OK &&
                  texture == wrapped[n].unlit_front);
        }
        pumpkin->fields.bm = before.bm;
        CHECK(memcmp(&before, &pumpkin->fields, sizeof(before)) == 0 &&
              pumpkin->subtype.fc.a == (lit != 0));
    }
    const mc_block *grass = NULL;
    int32_t unchanged = -777;
    CHECK(mc_registry_block_get(f->registry, 2, &grass, NULL) == MC_REGISTRY_OK && grass != NULL &&
          mc_block_render_inventory_color(grass, 0, &unchanged) ==
              MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY &&
          unchanged == -777 &&
          mc_block_render_inventory_shape((mc_block *)(void *)grass) ==
              MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY);
    return true;
}

static bool pressure_bounds(const mc_block *block, const uint64_t expected[6]) {
    const double values[] = {block->fields.bs, block->fields.bt, block->fields.bu,
                             block->fields.bv, block->fields.bw, block->fields.bx};
    for (size_t i = 0; i < 6; ++i)
        CHECK(double_bits(values[i]) == expected[i]);
    return true;
}

static void pressure_expected_shape(mc_block *expected) {
    expected->fields.bs = 0.0;
    expected->fields.bt = .375;
    expected->fields.bu = 0.0;
    expected->fields.bv = 1.0;
    expected->fields.bw = .625;
    expected->fields.bx = 1.0;
}

static bool pressure_plate_getter_controls(fixture *f) {
    static const uint64_t constructor_bounds[] = {
        UINT64_C(0x3fb0000000000000), 0,
        UINT64_C(0x3fb0000000000000), UINT64_C(0x3fee000000000000),
        UINT64_C(0x3fa0000000000000), UINT64_C(0x3fee000000000000)};
    static const uint64_t inventory_bounds[] = {0,
                                                UINT64_C(0x3fd8000000000000),
                                                0,
                                                UINT64_C(0x3ff0000000000000),
                                                UINT64_C(0x3fe4000000000000),
                                                UINT64_C(0x3ff0000000000000)};
    static const int32_t metadata[] = {INT32_MIN, -1, 0, 1, 2, INT32_MAX};
    static const int32_t sides[] = {INT32_MIN, -1, 0, 1, 2, 3, 4, 5, 6, INT32_MAX};
    static const int32_t textures[] = {INT32_MIN, -17, 0, 4, INT32_MAX};
    static const int32_t ids[] = {70, 72};
    for (size_t i = 0; i < sizeof(ids) / sizeof(ids[0]); ++i) {
        const mc_block *read = NULL;
        CHECK(mc_registry_block_get(f->registry, ids[i], &read, NULL) == MC_REGISTRY_OK &&
              read != NULL && read->kind == MC_BLOCK_PRESSURE_PLATE &&
              read->phase == MC_BLOCK_CONSTRUCTED && strcmp(read->constructor_class, "bv") == 0 &&
              read->subtype.bv.a == (i == 0 ? f->registry->pressure_b : f->registry->pressure_a) &&
              read->fields.bm == (i == 0 ? 1 : 4) && pressure_bounds(read, constructor_bounds));
        mc_block *plate = (mc_block *)(void *)read;
        const mc_block original = *plate;
        mc_block_render_description description;
        CHECK(mc_block_render_describe(plate, &description) == MC_REGISTRY_OK &&
              description.type == 0);
        for (size_t m = 0; m < sizeof(metadata) / sizeof(metadata[0]); ++m) {
            int32_t color = -777;
            CHECK(mc_block_render_inventory_color(plate, metadata[m], &color) == MC_REGISTRY_OK &&
                  color == 0xffffff);
            for (size_t s = 0; s < sizeof(sides) / sizeof(sides[0]); ++s) {
                int32_t texture = -777;
                CHECK(mc_block_render_texture_metadata(plate, sides[s], metadata[m], &texture) ==
                          MC_REGISTRY_OK &&
                      texture == original.fields.bm &&
                      mc_registry_block_texture_side(f->registry, plate, sides[s], &texture) ==
                          MC_REGISTRY_OK &&
                      texture == original.fields.bm &&
                      memcmp(plate, &original, sizeof(original)) == 0);
            }
        }
        mc_block expected = original;
        pressure_expected_shape(&expected);
        CHECK(mc_block_render_inventory_shape(plate) == MC_REGISTRY_OK &&
              pressure_bounds(plate, inventory_bounds) &&
              memcmp(plate, &expected, sizeof(expected)) == 0 &&
              mc_block_render_inventory_shape(plate) == MC_REGISTRY_OK &&
              memcmp(plate, &expected, sizeof(expected)) == 0);
        for (size_t t = 0; t < sizeof(textures) / sizeof(textures[0]); ++t) {
            /* Independently authored fields before each source invocation;
             * BV.g must overwrite every bound and preserve the live texture. */
            CHECK(mc_block_render_bounds(plate, -.125f, .25f, .375f, .75f, 1.25f, 1.5f) ==
                  MC_REGISTRY_OK);
            plate->fields.bm = textures[t];
            expected = *plate;
            pressure_expected_shape(&expected);
            CHECK(mc_block_render_inventory_shape(plate) == MC_REGISTRY_OK &&
                  pressure_bounds(plate, inventory_bounds) &&
                  memcmp(plate, &expected, sizeof(expected)) == 0);
            for (size_t m = 0; m < sizeof(metadata) / sizeof(metadata[0]); ++m) {
                int32_t color = -777, texture = -777;
                CHECK(mc_block_render_inventory_color(plate, metadata[m], &color) ==
                          MC_REGISTRY_OK &&
                      color == 0xffffff &&
                      mc_block_render_texture_metadata(plate, INT32_MIN, metadata[m], &texture) ==
                          MC_REGISTRY_OK &&
                      texture == textures[t] &&
                      mc_block_render_texture_metadata(plate, INT32_MAX, metadata[m], &texture) ==
                          MC_REGISTRY_OK &&
                      texture == textures[t] && memcmp(plate, &expected, sizeof(expected)) == 0);
            }
        }
        /* Teardown of the independent input preparation, after all returns. */
        plate->fields = original.fields;
        CHECK(memcmp(plate, &original, sizeof(original)) == 0);
    }
    mc_block *raw = NULL;
    CHECK(mc_registry_block_allocate(f->registry, MC_BLOCK_PRESSURE_PLATE, &raw) ==
              MC_REGISTRY_OK &&
          raw != NULL && raw->phase == MC_BLOCK_RAW);
    raw->fields.bm = INT32_MIN;
    raw->fields.br = true;
    mc_block expected = *raw;
    pressure_expected_shape(&expected);
    const size_t bytes = f->registry->allocation_bytes;
    const mc_registry_class_state declaring = f->registry->block_states[MC_BLOCK_PRESSURE_PLATE];
    CHECK(mc_block_render_inventory_shape(raw) == MC_REGISTRY_OK &&
          pressure_bounds(raw, inventory_bounds) && memcmp(raw, &expected, sizeof(expected)) == 0 &&
          raw->phase == MC_BLOCK_RAW && bytes == f->registry->allocation_bytes &&
          memcmp(&declaring, &f->registry->block_states[MC_BLOCK_PRESSURE_PLATE],
                 sizeof(declaring)) == 0);
    CHECK(mc_block_render_inventory_shape(NULL) == MC_REGISTRY_INVALID_ARGUMENT);
    return true;
}

static bool slab_block_getter_controls(fixture *f) {
    static const int32_t ids[] = {43, 44};
    static const int32_t metadata[] = {INT32_MIN, -1, 0, 1, 2, 3, 4, INT32_MAX};
    for (size_t i = 0; i < sizeof(ids) / sizeof(ids[0]); ++i) {
        const mc_block *read = NULL;
        CHECK(mc_registry_block_get(f->registry, ids[i], &read, NULL) == MC_REGISTRY_OK &&
              read != NULL && read->kind == MC_BLOCK_SLAB && read->subtype.ys.b == (i == 0) &&
              read->fields.bs == 0 && read->fields.bt == 0 && read->fields.bu == 0 &&
              read->fields.bv == 1 && read->fields.bw == (i == 0 ? 1.0 : .5) &&
              read->fields.bx == 1);
        mc_block *block = (mc_block *)(void *)read;
        const mc_block original = *block;
        for (unsigned int prepared = 0; prepared < 2; ++prepared) {
            if (prepared != 0)
                CHECK(mc_block_render_bounds(block, -.125f, .25f, .375f, .75f, 1.25f, 1.5f) ==
                      MC_REGISTRY_OK);
            const mc_block before = *block;
            for (size_t m = 0; m < sizeof(metadata) / sizeof(metadata[0]); ++m) {
                int32_t color = -777;
                CHECK(
                    mc_block_render_inventory_color(block, metadata[m], &color) == MC_REGISTRY_OK &&
                    color == 0xffffff && mc_block_render_inventory_shape(block) == MC_REGISTRY_OK &&
                    memcmp(block, &before, sizeof(before)) == 0);
            }
        }
        /* Independent native input preparation ends after all source returns. */
        block->fields = original.fields;
        CHECK(memcmp(block, &original, sizeof(original)) == 0);
    }
    mc_block *raw = NULL;
    CHECK(mc_registry_block_allocate(f->registry, MC_BLOCK_SLAB, &raw) == MC_REGISTRY_OK &&
          raw != NULL && raw->phase == MC_BLOCK_RAW &&
          mc_block_render_bounds(raw, -.25f, .125f, .5f, .625f, .875f, 1.25f) == MC_REGISTRY_OK);
    raw->fields.bm = INT32_MIN;
    raw->subtype.ys.b = true;
    const mc_block before = *raw;
    const size_t bytes = f->registry->allocation_bytes;
    const mc_registry_class_state declaring = f->registry->block_states[MC_BLOCK_SLAB];
    int32_t color = -777;
    CHECK(mc_block_render_inventory_color(raw, INT32_MIN, &color) == MC_REGISTRY_OK &&
          color == 0xffffff && mc_block_render_inventory_shape(raw) == MC_REGISTRY_OK &&
          raw->phase == MC_BLOCK_RAW && memcmp(raw, &before, sizeof(before)) == 0 &&
          f->registry->allocation_bytes == bytes &&
          memcmp(&declaring, &f->registry->block_states[MC_BLOCK_SLAB], sizeof(declaring)) == 0);
    return true;
}

static bool crafting_getter_controls(fixture *f) {
    static const int32_t ids[] = {5, 17, 58};
    static const int32_t metadata[] = {INT32_MIN, -1, 0, 1, 2, 3, INT32_MAX};
    static const int32_t sides[] = {INT32_MIN, -1, 0, 1, 2, 3, 4, 5, 6, INT32_MAX};
    for (size_t i = 0; i < sizeof(ids) / sizeof(ids[0]); ++i) {
        const mc_block *block = NULL;
        CHECK(mc_registry_block_get(f->registry, ids[i], &block, NULL) == MC_REGISTRY_OK &&
              block != NULL);
        const mc_block_fields before = block->fields;
        for (size_t m = 0; m < sizeof(metadata) / sizeof(metadata[0]); ++m) {
            int32_t color = -777;
            CHECK(mc_block_render_inventory_color(block, metadata[m], &color) == MC_REGISTRY_OK &&
                  color == 0xffffff &&
                  mc_block_render_inventory_shape((mc_block *)(void *)block) == MC_REGISTRY_OK &&
                  memcmp(&before, &block->fields, sizeof(before)) == 0);
            for (size_t s = 0; s < sizeof(sides) / sizeof(sides[0]); ++s) {
                int32_t texture = -777;
                const int32_t expected = ids[i] == 5    ? 4
                                         : ids[i] == 17 ? (sides[s] == 0 || sides[s] == 1 ? 21
                                                           : metadata[m] == 1             ? 116
                                                           : metadata[m] == 2             ? 117
                                                                                          : 20)
                                                        : (sides[s] == 0                    ? 4
                                                           : sides[s] == 1                  ? 43
                                                           : sides[s] == 2 || sides[s] == 4 ? 60
                                                                                            : 59);
                CHECK(mc_block_render_texture_metadata(block, sides[s], metadata[m], &texture) ==
                          MC_REGISTRY_OK &&
                      texture == expected);
            }
        }
    }
    const mc_block *table = NULL;
    CHECK(mc_registry_block_get(f->registry, 58, &table, NULL) == MC_REGISTRY_OK);
    mc_block *planks = f->registry->blocks.named[4];
    /* Declared singleton input: FI's bottom reaches current UU.y. Restore
     * only after the call, without repairing an in-flight source receiver. */
    f->registry->blocks.named[4] = NULL;
    int32_t unchanged = -777;
    const mc_registry_status status = mc_block_render_texture_metadata(table, 0, 0, &unchanged);
    f->registry->blocks.named[4] = planks;
    CHECK(status == MC_REGISTRY_NULL_REFERENCE && unchanged == -777);
    return true;
}

static bool inventory_controls(void) {
    fixture f = {0};
    CHECK(mc_materials_create(&f.materials));
    CHECK(mc_java_random_profile_create(SIZE_MAX, &f.profile) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_entity_classes_create(MC_ENTITY_CLIENT, f.profile, NULL, &f.entities) == MC_ENTITY_OK);
    static const uint8_t language[] = "stat.mineBlock=Mine %1$s\nstat.useItem=Use %1$s\n"
                                      "stat.breakItem=Break %1$s\nstat.craftItem=Craft %1$s\n";
    resources r = {.streams = {{language, sizeof language - 1, 0, false}, {0}, {0}}};
    const mc_registry_resources api = {&r, open_resource, close_resource, ready_resource};
    const mc_registry_options options = {MC_REGISTRY_CLIENT, f.profile, f.materials, 0, &api};
    CHECK(mc_registry_classes_allocate(&options, &f.registry, NULL) == MC_REGISTRY_OK);
    CHECK(mc_registry_classes_bind_entities(f.registry, f.entities, NULL) == MC_REGISTRY_OK);
    CHECK(mc_registry_classes_initialize_blocks(f.registry, NULL) == MC_REGISTRY_OK);
    CHECK(pumpkin_getter_controls(&f) && pressure_plate_getter_controls(&f) &&
          slab_block_getter_controls(&f) && crafting_getter_controls(&f));
    const mc_block *dirt = NULL;
    CHECK(mc_registry_block_get(f.registry, 3, &dirt, NULL) == MC_REGISTRY_OK && dirt != NULL);
    int32_t color = 0, icon = -1;
    CHECK(mc_block_render_inventory_color(dirt, 7, &color) == MC_REGISTRY_OK && color == 0xffffff);
    CHECK(mc_block_render_bounds((mc_block *)(void *)dirt, .125f, .25f, .375f, .75f, .875f, 1) ==
          MC_REGISTRY_OK);
    const mc_block_fields before = *mc_block_fields_get(dirt);
    CHECK(mc_block_render_inventory_shape((mc_block *)(void *)dirt) == MC_REGISTRY_OK &&
          memcmp(&before, mc_block_fields_get(dirt), sizeof before) == 0);
    const mc_block *wool = NULL;
    CHECK(mc_registry_block_get(f.registry, 35, &wool, NULL) == MC_REGISTRY_OK && wool != NULL &&
          mc_block_render_bounds((mc_block *)(void *)wool, .125f, .25f, .375f, .75f, .875f, 1) ==
              MC_REGISTRY_OK);
    const mc_block_fields wool_before = *mc_block_fields_get(wool);
    static const int32_t wool_textures[16] = {64,  210, 194, 178, 162, 146, 130, 114,
                                              225, 209, 193, 177, 161, 145, 129, 113};
    for (int32_t metadata = 0; metadata < 16; ++metadata) {
        CHECK(mc_block_render_inventory_color(wool, metadata, &color) == MC_REGISTRY_OK &&
              color == 0xffffff &&
              mc_block_render_inventory_shape((mc_block *)(void *)wool) == MC_REGISTRY_OK &&
              memcmp(&wool_before, mc_block_fields_get(wool), sizeof wool_before) == 0);
        for (int32_t side = 0; side < 6; ++side)
            CHECK(mc_block_render_texture_metadata(wool, side, metadata, &icon) == MC_REGISTRY_OK &&
                  icon == wool_textures[metadata]);
    }
    CHECK(mc_block_render_texture_metadata(wool, 0, 16, &icon) == MC_REGISTRY_OK && icon == 226 &&
          mc_block_render_inventory_color(wool, -1, &color) == MC_REGISTRY_OK && color == 0xffffff);
    const mc_item *item;
    const mc_registry_item_array *items;
    CHECK(mc_registry_item_c_get(f.registry, &items, NULL) == MC_REGISTRY_OK &&
          mc_registry_item_c_at(f.registry, items, 3, &item, NULL) == MC_REGISTRY_OK &&
          item != NULL);
    CHECK(inherited_item_controls(f.registry) && log_item_getter_controls(&f) &&
          slab_item_getter_controls(&f) && log_item_initializer_controls(&f) &&
          slab_item_initializer_controls(&f));
    mc_item_render_error item_error;
    CHECK(mc_item_render_icon(f.registry, item, 7, &icon, &item_error) && item_error.returned &&
          icon == 2);
    bool flag = true;
    CHECK(mc_item_render_full_3d(f.registry, item, &flag, &item_error) && !flag);
    flag = true;
    CHECK(mc_item_render_rotate(f.registry, item, &flag, &item_error) && !flag);
    CHECK(mc_item_render_color(f.registry, item, 7, &color, &item_error) && color == 0xffffff);
    const mc_item *wool_item = NULL;
    CHECK(mc_registry_item_c_at(f.registry, items, 35, &wool_item, NULL) == MC_REGISTRY_OK &&
          wool_item != NULL);
    static const int32_t wool_item_icons[16] = {113, 129, 145, 161, 177, 193, 209, 225,
                                                114, 130, 146, 162, 178, 194, 210, 64};
    int32_t actual_wool_icons[16];
    for (int32_t metadata = 0; metadata < 16; ++metadata) {
        CHECK(mc_item_render_icon(f.registry, wool_item, metadata, &icon, &item_error) &&
              item_error.returned && icon == wool_item_icons[metadata] &&
              mc_item_render_color(f.registry, wool_item, metadata, &color, &item_error) &&
              color == 0xffffff &&
              mc_item_render_full_3d(f.registry, wool_item, &flag, &item_error) && !flag &&
              mc_item_render_rotate(f.registry, wool_item, &flag, &item_error) && !flag);
        actual_wool_icons[metadata] = icon;
    }
    printf("WOOL_ITEM_ICONS ");
    for (size_t i = 0; i < 16; ++i)
        printf("%d%c", actual_wool_icons[i], i == 15 ? '\n' : ',');
    CHECK(mc_item_render_icon(f.registry, wool_item, -1, &icon, &item_error) && icon == 64 &&
          mc_item_render_icon(f.registry, wool_item, 16, &icon, &item_error) && icon == 113);
    mc_block *actual_ac = f.registry->blocks.named[34];
    f.registry->blocks.named[34] = NULL;
    icon = 87;
    CHECK(!mc_item_render_icon(f.registry, wool_item, 0, &icon, &item_error) && icon == 87 &&
          item_error.site.pc == 8 && strcmp(item_error.site.class_name, "bi") == 0 &&
          item_error.source_exception &&
          strcmp(item_error.exception_class, "java/lang/NullPointerException") == 0);
    f.registry->blocks.named[34] = (mc_block *)(void *)dirt;
    CHECK(mc_item_render_icon(f.registry, wool_item, 0, &icon, &item_error) && icon == 2);
    f.registry->blocks.named[34] = actual_ac;
    CHECK(mc_client_render_classes_create(f.entities, f.profile, &f.models) == MC_CLIENT_MODEL_OK);
    mc_client_model_error model_error;
    CHECK(mc_client_render_blocks_active_use(f.models, &model_error));
    mc_client_item_renderer *renderer = NULL;
    CHECK(mc_client_item_renderer_allocate(f.models, 0, &renderer) == MC_CLIENT_MODEL_OK &&
          mc_client_item_renderer_construct(renderer, &model_error) == MC_CLIENT_MODEL_OK);
    mc_client_item_overlay_context overlay = {.owner = renderer};
    CHECK(mc_client_item_overlay_draw(&overlay) && overlay.returned && overlay.null_stack &&
          overlay.source_pc == 4 && !overlay.count_read && !overlay.durability_returned);
    mc_item_stack *stack = NULL;
    CHECK(mc_item_stack_allocate(&stack) == MC_ITEM_STACK_OK &&
          mc_item_stack_construct(stack, 3, 1, 0) == MC_ITEM_STACK_OK);
    overlay =
        (mc_client_item_overlay_context){.owner = renderer, .registry = f.registry, .stack = stack};
    CHECK(mc_client_item_overlay_draw(&overlay) && overlay.returned && !overlay.null_stack &&
          overlay.source_pc == 288 && overlay.count_read && overlay.count == 1 &&
          !overlay.count_drawn && overlay.durability_returned && overlay.maximum == 0 &&
          !overlay.damage_read && !overlay.damaged);
    mc_item_stack_release(stack);
    CHECK(durability_controls(f.registry, renderer));
    CHECK(mc_client_item_renderer_free(&renderer) == MC_CLIENT_MODEL_OK && renderer == NULL);
    CHECK(mc_client_render_engine_classes_create(NULL, &f.graphics) == MC_CLIENT_RENDER_ENGINE_OK);
    mc_client_graphics_error graphics;
    CHECK(mc_client_nw_active_use(f.graphics,
                                  (mc_client_graphics_site){"native-test", "inventory", "()V", 0},
                                  &f.nw, &graphics));
    for (int32_t type = -2; type < 20; ++type)
        CHECK(mc_cv_inventory_type_3d(type) ==
              (type == 0 || type == 10 || type == 11 || type == 13 || type == 16));
    f.context = calloc(1, sizeof(*f.context));
    CHECK(f.context != NULL);
    for (unsigned int which = 0; which < 4; ++which) {
        mc_client_render_blocks_construct_fields(&f.fields, NULL, false);
        f.fields.b = (which & 1u) == 0;
        *f.context = (mc_cv_context){.fields = &f.fields,
                                     .registry = f.registry,
                                     .models = f.models,
                                     .graphics = f.graphics};
        const bool null_block = which < 2;
        CHECK(!mc_cv_inventory(f.context, null_block ? NULL : dirt, 7, .75f));
        const unsigned int expected =
            null_block ? (f.fields.b ? 14u : 78u) : (f.fields.b ? 74u : 114u);
        CHECK(f.context->failed && !f.context->returned && f.context->depth == 1 &&
              f.context->source_pc == expected && f.context->inventory_faces == 0 &&
              !f.nw->fields.w && f.nw->fields.h == 0 &&
              strcmp(f.context->failure.source_descriptor, "(Luu;IF)V") == 0 &&
              memcmp(&before, mc_block_fields_get(dirt), sizeof before) == 0);
    }
    const mc_block *plate_read = NULL;
    CHECK(mc_registry_block_get(f.registry, 72, &plate_read, NULL) == MC_REGISTRY_OK &&
          plate_read != NULL && plate_read->fields.bt == 0.0 && plate_read->fields.bw == .03125);
    mc_block plate_expected = *plate_read;
    pressure_expected_shape(&plate_expected);
    mc_client_render_blocks_construct_fields(&f.fields, NULL, false);
    f.fields.b = false; /* Actual CV field suppresses only the earlier GL color call. */
    *f.context = (mc_cv_context){
        .fields = &f.fields, .registry = f.registry, .models = f.models, .graphics = f.graphics};
    CHECK(!mc_cv_inventory(f.context, plate_read, INT32_MIN, .75f) && f.context->failed &&
          !f.context->returned && f.context->source_pc == 114 && f.context->inventory_faces == 0 &&
          !f.nw->fields.w && f.nw->fields.h == 0 &&
          memcmp(plate_read, &plate_expected, sizeof(plate_expected)) == 0);
    CHECK(mc_client_render_engine_classes_release(f.graphics) == MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(mc_client_render_classes_free(f.models) == MC_CLIENT_MODEL_OK);
    CHECK(mc_registry_classes_free(f.registry, NULL) == MC_REGISTRY_OK);
    CHECK(mc_entity_classes_free(f.entities) == MC_ENTITY_OK);
    mc_java_random_profile_free(f.profile);
    mc_materials_destroy(f.materials);
    free(f.context);
    CHECK(!r.invalid && r.opens == 3 && r.closes == 1);
    return true;
}

int main(void) {
    if (!inventory_controls())
        return 1;
    printf("Inventory Block controls: %u checks; real Dirt/BlockItem/NW failure prefixes\n",
           checks);
    return 0;
}
