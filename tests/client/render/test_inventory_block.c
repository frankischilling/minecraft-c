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
    } overrides[] = {{MC_ITEM_DYE, "km"},
                     {MC_ITEM_LOG_BLOCK, "go"},
                     {MC_ITEM_SLAB_BLOCK, "en"},
                     {MC_ITEM_SAPLING_BLOCK, "mr"},
                     {MC_ITEM_LEAVES_BLOCK, "uy"}};

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
          mc_block_render_inventory_shape(grass) == MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY);
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
    CHECK(pumpkin_getter_controls(&f));
    const mc_block *dirt = NULL;
    CHECK(mc_registry_block_get(f.registry, 3, &dirt, NULL) == MC_REGISTRY_OK && dirt != NULL);
    int32_t color = 0, icon = -1;
    CHECK(mc_block_render_inventory_color(dirt, 7, &color) == MC_REGISTRY_OK && color == 0xffffff);
    CHECK(mc_block_render_bounds((mc_block *)(void *)dirt, .125f, .25f, .375f, .75f, .875f, 1) ==
          MC_REGISTRY_OK);
    const mc_block_fields before = *mc_block_fields_get(dirt);
    CHECK(mc_block_render_inventory_shape(dirt) == MC_REGISTRY_OK &&
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
              color == 0xffffff && mc_block_render_inventory_shape(wool) == MC_REGISTRY_OK &&
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
    CHECK(inherited_item_controls(f.registry));
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
