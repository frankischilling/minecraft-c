/* Authored finite YC controls with genuine FP/Registry/Tile/screen owners.
 * The small font is authored test input, not the extracted original charset.
 * CPU mode constructs no App/Display; optional --sdk mode uses real SDK input.
 * Null/short Sign arrays remain outside the native fixed-array representation. */
#include "client/application_state.h"
#include "client/display/staged.h"
#include "client/gui/display_screen.h"
#include "client/gui/source_screen_initialize.h"
#include "client/gui/source_screen_transition_receipts_internal.h"
#include "client/gui/source_sign_screen.h"
#include "mc/block/material/material.h"
#include "mc/block/registry/registry.h"
#include "mc/core/java/random_profile.h"
#include "mc/world/runtime/construction.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned int checks;
static mc_math_trig trig;
#define COUNT(a) (sizeof(a) / sizeof((a)[0]))
#define CHECK(c)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(c)) {                                                                                \
            fprintf(stderr, "YC check%u line%d: %s\n", checks, __LINE__, #c);                      \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

typedef struct fixture {
    mc_java_random_profile *profile;
    mc_materials *materials;
    mc_registry_classes *registry;
    mc_world_runtime_classes *worlds;
    mc_client_screen_classes *classes;
    mc_client_font_mapping *mapping;
    mc_player_string *mapping_empty;
    mc_tile_entity *tile;
    mc_tile_entity_name *empty;
    mc_client_source_yc *screen;
    const mc_player_string *constructor_title; /* Borrowed from the live screen namespace. */
    mc_registry_resources resources;
    size_t cursor, opens, closes;
    int32_t expected_edit_row;
    bool expected_read_flag, expected_invalid;
} fixture;

static const uint8_t authored_font[] = "# authored YC fixture\nA \xc3\xa9~\n";

static mc_java_properties_status read_font(void *context, uint8_t *out, size_t capacity,
                                           size_t *count) {
    fixture *f = context;
    size_t n = sizeof(authored_font) - 1 - f->cursor;
    if (n > capacity)
        n = capacity;
    memcpy(out, authored_font + f->cursor, n);
    f->cursor += n;
    *count = n;
    return MC_JAVA_PROPERTIES_OK;
}

static mc_registry_status open_font(void *context, const char *name,
                                    mc_java_properties_input *out) {
    fixture *f = context;
    if (strcmp(name, "/font.txt") != 0)
        return MC_REGISTRY_NULL_REFERENCE;
    ++f->opens;
    f->cursor = 0;
    *out = (mc_java_properties_input){f, read_font};
    return MC_REGISTRY_OK;
}

static mc_registry_status close_font(void *context, const mc_java_properties_input *input) {
    fixture *f = context;
    if (input->context != f)
        return MC_REGISTRY_INVALID_ARGUMENT;
    ++f->closes;
    return MC_REGISTRY_OK;
}

static bool font_ready(void *context, const mc_java_properties_input *input) {
    fixture *f = context;
    return input->context == f && f->cursor < sizeof(authored_font) - 1;
}

static int setup(fixture *f, size_t mapping_allocations) {
    f->resources = (mc_registry_resources){f, open_font, close_font, font_ready};
    CHECK(mc_java_random_profile_create(SIZE_MAX, &f->profile) == MC_JAVA_RANDOM_PROFILE_OK &&
          mc_materials_create(&f->materials));
    const mc_registry_options options = {.side = MC_REGISTRY_CLIENT,
                                         .random_profile = f->profile,
                                         .materials = f->materials,
                                         .resources = &f->resources};
    CHECK(mc_registry_classes_allocate(&options, &f->registry, NULL) == MC_REGISTRY_OK &&
          mc_world_runtime_classes_create(MC_WORLD_INFO_CLIENT, f->profile, &trig, &f->worlds,
                                          NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_classes_bind_registry(f->worlds, f->registry, NULL) == MC_REGISTRY_OK &&
          mc_client_screen_classes_create(f->profile, &f->resources, f->registry, &f->classes,
                                          NULL) == MC_CLIENT_SCREEN_OK &&
          mc_player_string_create(NULL, 0, &f->mapping_empty) == MC_PLAYER_OK &&
          mc_client_font_mapping_create(&f->resources, f->mapping_empty, SIZE_MAX,
                                        mapping_allocations,
                                        &f->mapping) == MC_CLIENT_FONT_MAPPING_OK);
    mc_client_sign_class_state yc;
    mc_client_font_mapping_state fp;
    CHECK(mc_client_screen_sign_class_inspect(f->classes, &yc, NULL) == MC_CLIENT_SCREEN_OK &&
          yc.phase == MC_CLIENT_SCREEN_CLASS_COLD && yc.accepted == NULL &&
          mc_client_font_mapping_inspect(f->mapping, &fp) && fp.phase == MC_REGISTRY_COLD &&
          fp.a == NULL && f->opens == 0 && f->closes == 0);
    return 0;
}

static int dispose_screen(fixture *f) {
    if (f->screen != NULL) {
        CHECK(mc_client_screen_sign_retire(&f->screen, NULL) == MC_CLIENT_SCREEN_OK &&
              f->screen == NULL);
    }
    if (f->tile != NULL) {
        mc_tile_entity_release(f->tile);
        f->tile = NULL;
    }
    return 0;
}

static int teardown(fixture *f) {
    CHECK(dispose_screen(f) == 0);
    mc_tile_entity_name_release(f->empty);
    f->empty = NULL;
    if (f->classes != NULL)
        CHECK(mc_client_screen_classes_release(&f->classes, NULL) == MC_CLIENT_SCREEN_OK &&
              f->classes == NULL);
    if (f->mapping != NULL) {
        CHECK(mc_client_font_mapping_release(f->mapping) == MC_CLIENT_FONT_MAPPING_OK);
        f->mapping = NULL;
    }
    mc_player_string_release(f->mapping_empty);
    f->mapping_empty = NULL;
    if (f->worlds != NULL) {
        CHECK(mc_world_runtime_classes_free(f->worlds, NULL) == MC_WORLD_STORAGE_OK);
        f->worlds = NULL;
    }
    if (f->registry != NULL) {
        CHECK(mc_registry_classes_free(f->registry, NULL) == MC_REGISTRY_OK);
        f->registry = NULL;
    }
    mc_materials_destroy(f->materials);
    mc_java_random_profile_free(f->profile);
    f->materials = NULL;
    f->profile = NULL;
    return 0;
}

static bool line_is(mc_tile_entity *tile, int32_t row, const uint16_t *units, size_t count) {
    mc_tile_entity_name *held = NULL;
    mc_tile_entity_name_view view;
    if (mc_tile_entity_sign_get_line(tile, row, &held) != MC_TILE_ENTITY_OK)
        return false;
    const bool ok = mc_tile_entity_name_get_view(held, &view) == MC_TILE_ENTITY_OK &&
                    !view.is_null && view.text.length == count &&
                    (count == 0 || memcmp(view.text.units, units, count * sizeof(*units)) == 0);
    mc_tile_entity_name_release(held);
    return ok;
}

static int put_line(fixture *f, int32_t row, const uint16_t *units, size_t count) {
    mc_tile_entity_name *name = NULL;
    CHECK(mc_tile_entity_name_create(units, count, &name) == MC_TILE_ENTITY_OK);
    const mc_tile_entity_status status = mc_tile_entity_sign_set_line(f->tile, row, name);
    mc_tile_entity_name_release(name);
    CHECK(status == MC_TILE_ENTITY_OK && line_is(f->tile, row, units, count));
    return 0;
}

static int unchanged_tile(fixture *f) {
    mc_tile_entity_snapshot tile;
    CHECK(mc_tile_entity_get_snapshot(f->tile, &tile) == MC_TILE_ENTITY_OK &&
          tile.kind == MC_TILE_ENTITY_SIGN && tile.phase == MC_TILE_ENTITY_READY &&
          tile.world == NULL && tile.x == 0 && tile.y == 0 && tile.z == 0 &&
          tile.invalid == f->expected_invalid && tile.sign_line_count == 4 &&
          tile.sign_edit_row == f->expected_edit_row &&
          tile.sign_read_flag == f->expected_read_flag);
    mc_client_sign_screen_state state;
    CHECK(mc_client_screen_sign_inspect(f->screen, &state, NULL) == MC_CLIENT_SCREEN_OK &&
          state.detail != NULL && !state.detail->dirty_returned && !state.detail->display_entered);
    return 0;
}

static int fresh_with_input(fixture *f, mc_client_sdk_input *input) {
    CHECK(dispose_screen(f) == 0 &&
          mc_tile_entity_allocate_in_classes(f->worlds, MC_TILE_ENTITY_SIGN, &f->tile) ==
              MC_TILE_ENTITY_OK &&
          mc_tile_entity_construct(f->tile) == MC_TILE_ENTITY_OK);
    f->expected_edit_row = -1;
    f->expected_read_flag = true;
    f->expected_invalid = false;
    mc_tile_entity_name *blank[4] = {0};
    for (int32_t row = 0; row < 4; ++row)
        CHECK(mc_tile_entity_sign_get_line(f->tile, row, &blank[row]) == MC_TILE_ENTITY_OK);
    CHECK(blank[0] != NULL && blank[0] == blank[1] && blank[0] == blank[2] && blank[0] == blank[3]);
    if (f->empty == NULL) {
        f->empty = blank[0];
        blank[0] = NULL;
    } else
        CHECK(blank[0] == f->empty);
    for (size_t i = 0; i < COUNT(blank); ++i)
        mc_tile_entity_name_release(blank[i]);
    const mc_client_sign_screen_dependencies dependencies = {
        .mapping = f->mapping, .input = input, .literal_classes = f->worlds};
    CHECK(mc_client_screen_sign_allocate(f->classes, &dependencies, NULL, &f->screen, NULL) ==
              MC_CLIENT_SCREEN_OK &&
          mc_client_screen_sign_construct(f->screen, f->tile, NULL) == MC_CLIENT_SCREEN_OK);
    mc_client_sign_screen_state state;
    mc_client_screen_state base;
    mc_player_string_view title;
    static const uint16_t caption[] = {'E', 'd', 'i', 't', ' ', 's', 'i', 'g', 'n',
                                       ' ', 'm', 'e', 's', 's', 'a', 'g', 'e', ':'};
    CHECK(mc_client_screen_sign_inspect(f->screen, &state, NULL) == MC_CLIENT_SCREEN_OK &&
          state.phase == MC_CLIENT_SCREEN_READY && state.tile == f->tile && state.tile_held &&
          state.literal_classes_held && state.input_held == (input != NULL) && state.tick == 0 &&
          state.row == 0 && !state.terminal_prepared && state.initialization_count == 0 &&
          mc_player_string_read(state.title, &title) && title.length == COUNT(caption) &&
          memcmp(title.units, caption, sizeof(caption)) == 0 &&
          mc_client_screen_inspect(mc_client_screen_sign_base(f->screen), &base, NULL) ==
              MC_CLIENT_SCREEN_OK &&
          base.da.b == NULL && base.da.g == NULL && base.da.h == NULL && base.da.c == 0 &&
          base.da.d == 0 && base.da.e != NULL && base.da.a == NULL);
    if (f->constructor_title == NULL)
        f->constructor_title = state.title;
    else
        CHECK(state.title == f->constructor_title);
    CHECK(unchanged_tile(f) == 0 && f->opens == 1 && f->closes == 1);
    return 0;
}

static int fresh(fixture *f) {
    return fresh_with_input(f, NULL);
}

static int key(fixture *f, uint16_t character, int32_t code) {
    mc_client_screen_error error;
    CHECK(mc_client_screen_sign_key(f->screen, character, code, &error) == MC_CLIENT_SCREEN_OK &&
          error.source_returned && error.site.pc == 174 &&
          strcmp(error.site.class_name, "yc") == 0 && strcmp(error.site.method, "a") == 0 &&
          strcmp(error.site.descriptor, "(CI)V") == 0 && unchanged_tile(f) == 0);
    return 0;
}

static int text_and_navigation(fixture *f) {
    CHECK(fresh(f) == 0);
    const uint16_t accepted[] = {'A', ' ', 0x00e9, '~'};
    for (size_t i = 0; i < COUNT(accepted); ++i)
        CHECK(key(f, accepted[i], 0) == 0);
    CHECK(line_is(f->tile, 0, accepted, COUNT(accepted)));
    mc_tile_entity_name *same = NULL, *again = NULL;
    CHECK(mc_tile_entity_sign_get_line(f->tile, 0, &same) == MC_TILE_ENTITY_OK);
    const uint16_t rejected[] = {0, 8, 9, 13, 0x00a7, 0x03a9, 0x4e2d, 0xd800, 0xdc00, 0xffff};
    for (size_t i = 0; i < COUNT(rejected); ++i)
        CHECK(key(f, rejected[i], 0) == 0);
    CHECK(mc_tile_entity_sign_get_line(f->tile, 0, &again) == MC_TILE_ENTITY_OK && again == same);
    mc_tile_entity_name_release(same);
    mc_tile_entity_name_release(again);
    const int32_t navigation[] = {200, 208, 28};
    for (size_t kind = 0; kind < COUNT(navigation); ++kind) {
        CHECK(fresh(f) == 0);
        mc_client_sign_screen_state initial, state;
        CHECK(mc_client_screen_sign_inspect(f->screen, &initial, NULL) == MC_CLIENT_SCREEN_OK);
        for (int32_t i = 0; i < 8; ++i) {
            const int32_t expected = navigation[kind] == 200 ? (3 * (i + 1)) % 4 : (i + 1) % 4;
            CHECK(key(f, 0, navigation[kind]) == 0 &&
                  mc_client_screen_sign_inspect(f->screen, &state, NULL) == MC_CLIENT_SCREEN_OK &&
                  state.row == expected &&
                  state.row_stores == initial.row_stores + (unsigned)i + 1 &&
                  state.line_stores == initial.line_stores);
        }
    }
    CHECK(fresh(f) == 0);
    const int32_t ordinary[] = {1, 156, 15, -1, INT32_MIN, INT32_MAX};
    for (size_t i = 0; i < COUNT(ordinary); ++i)
        CHECK(key(f, 'A', ordinary[i]) == 0);
    CHECK(line_is(f->tile, 0, (uint16_t[]){'A', 'A', 'A', 'A', 'A', 'A'}, 6));
    CHECK(fresh(f) == 0 && put_line(f, 0, (uint16_t[]){'A', '~', ' '}, 3) == 0 &&
          key(f, 'A', 14) == 0 && line_is(f->tile, 0, (uint16_t[]){'A', '~', 'A'}, 3));
    CHECK(key(f, 'A', 28) == 0 && line_is(f->tile, 1, (uint16_t[]){'A'}, 1) &&
          key(f, '~', 200) == 0 && line_is(f->tile, 0, (uint16_t[]){'A', '~', 'A', '~'}, 4));
    return 0;
}

static int utf16_and_identity(fixture *f) {
    uint16_t row[17];
    for (size_t i = 0; i < COUNT(row); ++i)
        row[i] = '~';
    CHECK(fresh(f) == 0 && put_line(f, 0, row, 14) == 0 && key(f, 'A', 0) == 0);
    row[14] = 'A';
    CHECK(line_is(f->tile, 0, row, 15) && key(f, ' ', 0) == 0 && line_is(f->tile, 0, row, 15) &&
          key(f, 'A', 14) == 0 && line_is(f->tile, 0, row, 15));
    row[14] = '~';
    CHECK(put_line(f, 0, row, 16) == 0 && key(f, 'A', 0) == 0 && line_is(f->tile, 0, row, 16) &&
          key(f, 'A', 14) == 0 && line_is(f->tile, 0, row, 15));
    CHECK(put_line(f, 0, row, 17) == 0 && key(f, 0, 14) == 0 && key(f, 0, 14) == 0 &&
          key(f, 'A', 0) == 0 && line_is(f->tile, 0, row, 15));
    CHECK(put_line(f, 0, (uint16_t[]){0xd83d, 0xde00}, 2) == 0 && key(f, 0, 14) == 0 &&
          line_is(f->tile, 0, (uint16_t[]){0xd83d}, 1) && key(f, 'A', 0) == 0 &&
          line_is(f->tile, 0, (uint16_t[]){0xd83d, 'A'}, 2));
    CHECK(put_line(f, 0, (uint16_t[]){0xdc00}, 1) == 0 && key(f, 0, 14) == 0 &&
          line_is(f->tile, 0, NULL, 0));

    /* Every constructor blank is replaced before the one-unit substring.
     * The result must come from the actual owner-class literal pool. */
    CHECK(fresh(f) == 0);
    mc_tile_entity_name *shared = NULL;
    CHECK(mc_tile_entity_name_create((uint16_t[]){'A'}, 1, &shared) == MC_TILE_ENTITY_OK);
    for (int32_t i = 0; i < 4; ++i)
        CHECK(mc_tile_entity_sign_set_line(f->tile, i, shared) == MC_TILE_ENTITY_OK);
    CHECK(key(f, 'A', 0) == 0);
    mc_tile_entity_name *changed = NULL, *untouched = NULL;
    CHECK(mc_tile_entity_sign_get_line(f->tile, 0, &changed) == MC_TILE_ENTITY_OK &&
          mc_tile_entity_sign_get_line(f->tile, 1, &untouched) == MC_TILE_ENTITY_OK &&
          changed != shared && untouched == shared);
    mc_tile_entity_name_release(changed);
    mc_tile_entity_name_release(untouched);
    CHECK(key(f, 0, 14) == 0 &&
          mc_tile_entity_sign_get_line(f->tile, 0, &changed) == MC_TILE_ENTITY_OK &&
          changed != shared && line_is(f->tile, 0, (uint16_t[]){'A'}, 1));
    mc_tile_entity_name_release(changed);
    CHECK(key(f, 0, 14) == 0 &&
          mc_tile_entity_sign_get_line(f->tile, 0, &changed) == MC_TILE_ENTITY_OK &&
          changed == f->empty);
    mc_tile_entity_name_release(changed);
    mc_tile_entity_name_release(shared);
    CHECK(key(f, 0, 14) == 0 &&
          mc_tile_entity_sign_get_line(f->tile, 0, &changed) == MC_TILE_ENTITY_OK &&
          changed == f->empty);
    mc_tile_entity_name_release(changed);
    return 0;
}

static int late_null_rows(fixture *f) {
    const struct {
        int32_t row, key;
        uint16_t character;
        unsigned int pc;
    } cases[] = {{0, 0, 'A', 135},
                 {0, 14, 0, 62},
                 {3, 200, 'A', 135},
                 {1, 208, 'A', 135},
                 {1, 28, 'A', 135}};

    for (size_t i = 0; i < COUNT(cases); ++i) {
        CHECK(fresh(f) == 0 &&
              mc_tile_entity_sign_set_line(f->tile, cases[i].row, NULL) == MC_TILE_ENTITY_OK);
        mc_client_screen_error error;
        CHECK(mc_client_screen_sign_key(f->screen, cases[i].character, cases[i].key, &error) ==
                  MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE &&
              !error.native_policy && error.source_exception && !error.source_error &&
              strcmp(error.exception_class, "java/lang/NullPointerException") == 0 &&
              strcmp(error.site.class_name, "yc") == 0 && error.site.pc == cases[i].pc);
        mc_client_sign_screen_state state;
        mc_tile_entity_name *null_row = f->empty;
        CHECK(mc_client_screen_sign_inspect(f->screen, &state, NULL) == MC_CLIENT_SCREEN_OK &&
              state.phase == MC_CLIENT_SCREEN_READY && state.row == cases[i].row &&
              state.tile == f->tile && state.tile_held &&
              mc_tile_entity_sign_get_line(f->tile, cases[i].row, &null_row) == MC_TILE_ENTITY_OK &&
              null_row == NULL && unchanged_tile(f) == 0);
        /* Rejected char must not dereference that same null row or poison it. */
        CHECK(key(f, 0x03a9, 0) == 0);
    }
    return 0;
}

static int semantic_flag_prestate(fixture *f) {
    CHECK(fresh(f) == 0);
    /* Actual semantic read reaches private c=false; no private field write.
     * Missing typed fields have their real empty/zero defaults. */
    const mc_nbt_tag empty = {.type = MC_NBT_COMPOUND};
    CHECK(mc_tile_entity_read_nbt(f->tile, &empty, NULL) == MC_TILE_ENTITY_OK &&
          mc_tile_entity_sign_set_edit_row(f->tile, INT32_MIN) == MC_TILE_ENTITY_OK &&
          mc_tile_entity_set_invalid(f->tile, true) == MC_TILE_ENTITY_OK);
    f->expected_read_flag = false;
    f->expected_edit_row = INT32_MIN;
    f->expected_invalid = true;
    CHECK(key(f, 'A', 28) == 0 && key(f, 0, 14) == 0 && key(f, 0x03a9, 200) == 0 &&
          unchanged_tile(f) == 0);
    mc_client_sign_screen_state before, after;
    mc_client_screen_error error;
    CHECK(mc_client_screen_sign_inspect(f->screen, &before, NULL) == MC_CLIENT_SCREEN_OK &&
          mc_client_screen_sign_tick(f->screen, &error) == MC_CLIENT_SCREEN_OK &&
          error.source_returned && error.site.pc == 10 &&
          mc_client_screen_sign_tick(f->screen, &error) == MC_CLIENT_SCREEN_OK &&
          mc_client_screen_sign_inspect(f->screen, &after, NULL) == MC_CLIENT_SCREEN_OK &&
          after.tick == before.tick + 2 && after.tick_stores == before.tick_stores + 2 &&
          after.row == before.row && unchanged_tile(f) == 0);
    return 0;
}

static int direct_button_guard(fixture *f) {
    CHECK(fresh(f) == 0);
    mc_client_sign_screen_state before, after;
    mc_client_screen_error error;
    mc_client_source_ke *button = NULL;
    CHECK(mc_client_screen_sign_inspect(f->screen, &before, NULL) == MC_CLIENT_SCREEN_OK &&
          mc_client_screen_button_allocate(f->classes, &button, &error) == MC_CLIENT_SCREEN_OK &&
          mc_client_screen_button_construct_default(
              button, 7, 10, 20,
              (mc_client_screen_text){MC_CLIENT_SCREEN_TEXT_PLAYER, before.title, NULL},
              &error) == MC_CLIENT_SCREEN_OK);
    mc_client_display_screen_call *first = NULL, *second = NULL;
    /* Two real nonzero-button invocations, separate caller output slots.
     * This guards the literal no-display branch, NOT successful multi-Done. */
    CHECK(mc_client_screen_sign_button(f->screen, button, &first, &error) == MC_CLIENT_SCREEN_OK &&
          error.source_returned && first == NULL &&
          mc_client_screen_sign_button(f->screen, button, &second, &error) == MC_CLIENT_SCREEN_OK &&
          error.source_returned && second == NULL &&
          mc_client_screen_sign_inspect(f->screen, &after, NULL) == MC_CLIENT_SCREEN_OK &&
          after.source_calls == before.source_calls + 2 && after.last_pc == 30 &&
          !after.detail->dirty_returned && !after.detail->display_entered &&
          unchanged_tile(f) == 0);
    CHECK(mc_client_screen_sign_button(f->screen, NULL, &first, &error) ==
              MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE &&
          !error.native_policy && error.source_exception && error.site.pc == 1 && first == NULL &&
          mc_client_screen_button_retire(&button, &error) == MC_CLIENT_SCREEN_OK && button == NULL);
    return 0;
}

static int retained_sign_receipt(fixture *f) {
    CHECK(fresh(f) == 0);
    /* Actual DA initialization enters YC.b and fails at its missing SDK code
     * dependency. This registered RAW App supplies the declared nullable q;
     * no App constructor, Display or successful repeat operation is credited. */
    client_application *app = calloc(1, sizeof(*app));
    CHECK(app != NULL &&
          mc_client_application_lifetime_register(app, NULL) == MC_CLIENT_APPLICATION_OWNER_OK);
    mc_client_screen_init_error initialization;
    CHECK(mc_client_screen_initialize(mc_client_screen_sign_base(f->screen), app, 320, 240,
                                      &initialization) == MC_CLIENT_SCREEN_INPUT_FAILED &&
          initialization.entry.pc == 45 && initialization.screen.native_policy &&
          initialization.screen.site.pc == 10 && initialization.screen.sign != NULL);
    mc_client_sign_screen_state state;
    CHECK(mc_client_screen_sign_inspect(f->screen, &state, NULL) == MC_CLIENT_SCREEN_OK &&
          initialization.screen.sign == state.detail);
    mc_client_screen_error retained = initialization.screen;
    retained.initialization = &initialization; /* The actual enclosing output value. */
    mc_screen_transition_receipts receipts = {0};
    /* A parent's diagnostic projection may expose only the same actual
     * initialization child, without a duplicate direct Sign view. */
    mc_client_screen_error nested = retained;
    nested.sign = NULL;
    mc_screen_transition_receipts nested_receipts = {0};
    CHECK(mc_screen_transition_sign_capture(&receipts, &retained, state.detail) &&
          retained.sign == &receipts.sign && retained.initialization == &receipts.initialization &&
          retained.initialization->screen.sign == &receipts.sign &&
          retained.sign->input.status == state.detail->input.status &&
          retained.sign->child.pc == state.detail->child.pc);
    CHECK(mc_screen_transition_sign_capture(&nested_receipts, &nested, state.detail) &&
          nested.sign == NULL && nested.initialization == &nested_receipts.initialization &&
          nested.initialization->screen.sign == &nested_receipts.sign);
    mc_client_screen_error cleanup;
    mc_client_source_da *external = mc_client_screen_sign_base(f->screen);
    CHECK(mc_client_screen_retain(external, &cleanup) == MC_CLIENT_SCREEN_OK &&
          mc_client_screen_sign_retire(&f->screen, &cleanup) == MC_CLIENT_SCREEN_BUSY &&
          f->screen != NULL);
    mc_screen_transition_initialization_after(&receipts, &initialization);
    mc_screen_transition_sign_after(&receipts, state.detail);
    mc_screen_transition_initialization_after(&nested_receipts, &initialization);
    mc_screen_transition_sign_after(&nested_receipts, state.detail);
    CHECK(retained.initialization->screen.sign == &receipts.sign && retained.site.pc == 10 &&
          retained.sign->input.status == state.detail->input.status &&
          mc_client_screen_release(&external, &cleanup) == MC_CLIENT_SCREEN_OK && external == NULL);
    CHECK(mc_client_screen_sign_terminal_prepare(f->screen, &cleanup) == MC_CLIENT_SCREEN_OK &&
          mc_client_screen_sign_inspect(f->screen, &state, NULL) == MC_CLIENT_SCREEN_OK);
    mc_screen_transition_sign_after(&receipts, state.detail);
    mc_screen_transition_sign_after(&nested_receipts, state.detail);
    CHECK(mc_client_screen_sign_retire(&f->screen, &cleanup) == MC_CLIENT_SCREEN_OK &&
          f->screen == NULL);
    mc_screen_transition_sign_after(&receipts, NULL);
    mc_screen_transition_initialization_after(&receipts, NULL);
    mc_screen_transition_sign_after(&nested_receipts, NULL);
    mc_screen_transition_initialization_after(&nested_receipts, NULL);
    memset(&initialization, 0, sizeof(initialization)); /* End that caller output's value scope. */
    CHECK(retained.status == MC_CLIENT_SCREEN_INPUT_FAILED && retained.site.pc == 10 &&
          retained.native_policy && retained.sign == &receipts.sign &&
          retained.initialization->entry.pc == 45 &&
          retained.initialization->screen.sign == &receipts.sign && nested.sign == NULL &&
          nested.initialization->entry.pc == 45 &&
          nested.initialization->screen.sign == &nested_receipts.sign &&
          nested.initialization->screen.sign->input.status == retained.sign->input.status &&
          retained.sign->input.status != MC_CLIENT_SDK_INPUT_OK && receipts.sign_source == NULL &&
          receipts.initialization_source == NULL &&
          retained.sign->dirty.world_access_failure.receiver == NULL &&
          retained.sign->dirty.world_access_failure.render_global == NULL &&
          retained.sign->dirty.world_access_failure.receipt == NULL &&
          retained.sign->dirty.publication.world_access_failure.receipt == NULL &&
          retained.sign->dirty.batch.publication.world_access_failure.receipt == NULL &&
          mc_client_application_lifetime_retire(app, NULL) == MC_CLIENT_APPLICATION_OWNER_OK);
    free(app);
    return 0;
}

static int retained_class_and_terminal(fixture *f) {
    CHECK(fresh(f) == 0);
    mc_client_sign_class_state yc;
    mc_client_font_mapping_state fp;
    CHECK(mc_client_screen_sign_class_inspect(f->classes, &yc, NULL) == MC_CLIENT_SCREEN_OK &&
          yc.phase == MC_CLIENT_SCREEN_CLASS_READY && yc.accepted != NULL &&
          yc.mapping_identity == f->mapping && yc.mapping_held &&
          mc_client_font_mapping_inspect(f->mapping, &fp) && fp.a == yc.accepted);
    CHECK(mc_client_font_mapping_release(f->mapping) == MC_CLIENT_FONT_MAPPING_OK);
    f->mapping = NULL; /* End caller's actual occurrence; YC owns the defining capability. */
    CHECK(fresh(f) == 0 && key(f, 0x00e9, 0) == 0 && f->opens == 1 && f->closes == 1);
    CHECK(mc_client_screen_sign_class_inspect(f->classes, &yc, NULL) == MC_CLIENT_SCREEN_OK &&
          yc.mapping_held && yc.mapping_identity != NULL &&
          mc_client_font_mapping_inspect(yc.mapping_identity, &fp) && fp.native_holds == 1 &&
          fp.a == yc.accepted);
    mc_client_screen_error error;
    const mc_player_string *captured = yc.accepted;
    CHECK(mc_client_screen_sign_class_terminal_prepare(f->classes, &error) == MC_CLIENT_SCREEN_OK &&
          mc_client_screen_sign_class_inspect(f->classes, &yc, NULL) == MC_CLIENT_SCREEN_OK &&
          yc.admission_ended && !yc.mapping_held && yc.mapping_identity == NULL &&
          yc.accepted == captured && put_line(f, 0, (uint16_t[]){'A'}, 1) == 0 &&
          key(f, 0, 14) == 0 && key(f, 0x00e9, 0) == 0 && f->opens == 1 && f->closes == 1);
    mc_client_source_yc *not_created = NULL;
    const mc_client_sign_screen_dependencies ended = {.literal_classes = f->worlds};
    CHECK(mc_client_screen_sign_allocate(f->classes, &ended, NULL, &not_created, &error) !=
              MC_CLIENT_SCREEN_OK &&
          not_created == NULL && error.native_policy &&
          mc_client_screen_sign_class_terminal_prepare(f->classes, &error) == MC_CLIENT_SCREEN_OK);
    mc_client_source_da *external = mc_client_screen_sign_base(f->screen);
    CHECK(mc_client_screen_retain(external, &error) == MC_CLIENT_SCREEN_OK &&
          mc_client_screen_sign_terminal_prepare(f->screen, &error) == MC_CLIENT_SCREEN_BUSY &&
          error.native_policy && !error.source_returned);
    mc_client_sign_screen_state state;
    CHECK(mc_client_screen_sign_inspect(f->screen, &state, NULL) == MC_CLIENT_SCREEN_OK &&
          !state.terminal_prepared && state.tile_held && state.tile == f->tile);
    mc_client_screen_classes *class_occurrence = f->classes;
    CHECK(mc_client_screen_classes_retain(class_occurrence, &error) == MC_CLIENT_SCREEN_OK &&
          mc_client_screen_classes_release(&class_occurrence, &error) == MC_CLIENT_SCREEN_OK &&
          class_occurrence == NULL &&
          mc_client_screen_release(&external, &error) == MC_CLIENT_SCREEN_OK && external == NULL);
    CHECK(mc_tile_entity_sign_set_line(f->tile, 0, NULL) == MC_TILE_ENTITY_OK &&
          mc_client_screen_sign_key(f->screen, 'A', 0, &error) ==
              MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE);
    const mc_client_screen_error source_failure = error;
    CHECK(mc_client_screen_sign_inspect(f->screen, &state, NULL) == MC_CLIENT_SCREEN_OK);
    const size_t calls = state.source_calls;
    CHECK(mc_client_screen_sign_terminal_prepare(f->screen, &error) == MC_CLIENT_SCREEN_OK &&
          mc_client_screen_sign_inspect(f->screen, &state, NULL) == MC_CLIENT_SCREEN_OK &&
          state.terminal_prepared && state.phase == MC_CLIENT_SCREEN_RETIRING && !state.tile_held &&
          !state.input_held && !state.literal_classes_held && state.tile == NULL &&
          state.source_calls == calls && state.failure->status == source_failure.status &&
          state.failure->site.pc == source_failure.site.pc && !state.failure->native_policy);
    CHECK(mc_client_screen_sign_retire(&f->screen, &error) == MC_CLIENT_SCREEN_OK &&
          f->screen == NULL);
    return 0;
}

static int constructor_prefix(fixture *f) {
    CHECK(fresh(f) == 0 && mc_client_screen_sign_retire(&f->screen, NULL) == MC_CLIENT_SCREEN_OK);
    const mc_client_screen_options quota = {1024u * 1024u, 0, 1024u * 1024u, 4096, 512};
    const mc_client_sign_screen_dependencies deps = {.mapping = f->mapping,
                                                     .literal_classes = f->worlds};
    mc_client_screen_error error;
    CHECK(mc_client_screen_sign_allocate(f->classes, &deps, &quota, &f->screen, &error) ==
              MC_CLIENT_SCREEN_OK &&
          mc_client_screen_sign_construct(f->screen, f->tile, &error) == MC_CLIENT_SCREEN_LIMIT &&
          error.native_policy && strcmp(error.site.class_name, "da") == 0 && error.site.pc == 5 &&
          strcmp(error.parent_site.class_name, "yc") == 0 && error.parent_site.pc == 1);
    mc_client_sign_screen_state state;
    CHECK(mc_client_screen_sign_inspect(f->screen, &state, NULL) == MC_CLIENT_SCREEN_OK &&
          state.phase == MC_CLIENT_SCREEN_FAILED && state.tile == NULL && !state.tile_held &&
          state.title == NULL && state.row == 0 && state.tick == 0);
    const mc_client_screen_error saved = *state.failure;
    CHECK(mc_client_screen_sign_construct(f->screen, f->tile, &error) ==
              MC_CLIENT_SCREEN_INVALID_PHASE &&
          mc_client_screen_sign_inspect(f->screen, &state, NULL) == MC_CLIENT_SCREEN_OK &&
          state.failure->status == saved.status && state.failure->site.pc == saved.site.pc &&
          f->opens == 1 && f->closes == 1);
    return 0;
}

static int failed_class(void) {
    fixture f = {0};
    CHECK(setup(&f, 0) == 0);
    mc_client_screen_error error;
    const mc_client_sign_screen_dependencies deps = {.mapping = f.mapping};
    CHECK(mc_client_screen_sign_allocate(f.classes, &deps, NULL, &f.screen, &error) !=
              MC_CLIENT_SCREEN_OK &&
          f.screen == NULL && error.native_policy);
    mc_client_sign_class_state first, next;
    CHECK(mc_client_screen_sign_class_inspect(f.classes, &first, NULL) == MC_CLIENT_SCREEN_OK &&
          first.phase == MC_CLIENT_SCREEN_CLASS_FAILED && first.accepted == NULL);
    const size_t opens = f.opens, closes = f.closes;
    const mc_client_screen_error saved = *first.failure;
    mc_client_font_mapping *replacement = NULL;
    CHECK(mc_client_font_mapping_create(&f.resources, f.mapping_empty, SIZE_MAX, SIZE_MAX,
                                        &replacement) == MC_CLIENT_FONT_MAPPING_OK &&
          mc_client_screen_sign_active_use(f.classes, replacement, &error) ==
              MC_CLIENT_SCREEN_CLASS_ERRONEOUS &&
          error.native_policy && f.opens == opens && f.closes == closes &&
          mc_client_screen_sign_class_inspect(f.classes, &next, NULL) == MC_CLIENT_SCREEN_OK &&
          next.phase == MC_CLIENT_SCREEN_CLASS_FAILED && next.failure->status == saved.status &&
          next.failure->site.pc == saved.site.pc && next.accepted == NULL);
    CHECK(mc_client_font_mapping_release(replacement) == MC_CLIENT_FONT_MAPPING_OK &&
          teardown(&f) == 0);
    return 0;
}

/* Optional actual SDK fixture. Root supplies only the authored two-icon tree
 * produced by tests/tools/client/test_display_sdk.py:authored_resources.
 * This performs no event polling, rendering, visible/focus or full App proof. */
static int sdk_lifecycle(const char *resources_path) {
    int result = 1;
    fixture f = {0};
    mc_resource_files *files = NULL;
    mc_client_sdk_icons *icons = NULL;
    mc_client_display *display = NULL;
    mc_client_display_mode *mode = NULL;
    mc_client_sdk_input *input = NULL;
    mc_client_display_screen_call *done_call = NULL;
    mc_client_source_da *raw_r_receiver = NULL;
    client_application *app = NULL;
    bool app_registered = false;
    mc_client_sdk_icons_error icons_error;
    mc_client_display_stage_error display_error;
    mc_client_screen_error error;
    mc_client_sdk_input_state input_state;
    mc_client_screen_init_error initialization;
#define SDK_CHECK(c)                                                                               \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(c)) {                                                                                \
            fprintf(stderr, "YC SDK check%u line%d: %s\n", checks, __LINE__, #c);                  \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)
    SDK_CHECK(setup(&f, SIZE_MAX) == 0 &&
              mc_resource_files_create(resources_path, &files) == MC_REGISTRY_OK &&
              mc_client_sdk_icons_allocate(mc_resource_files_source(files), &icons, &icons_error) ==
                  MC_CLIENT_SDK_ICONS_OK &&
              mc_client_sdk_icons_construct(icons, &icons_error) == MC_CLIENT_SDK_ICONS_OK &&
              mc_client_display_stage_allocate(&display, &display_error) &&
              mc_client_display_stage_bind_icons(display, icons, &display_error) &&
              mc_client_display_mode_allocate(&mode, &display_error) &&
              mc_client_display_mode_construct(mode, 160, 120, &display_error) &&
              mc_client_display_stage_set_mode(display, mode, &display_error) &&
              mc_client_display_create_staged(display, true, &display_error));
#ifdef _WIN32
    /* Place this same hidden SDK window on monitor2 before YC/Input work.
     * This native test placement does not alter a Source field or grab focus. */
    int display_count = 0;
    SDL_DisplayID *displays = SDL_GetDisplays(&display_count);
    SDK_CHECK(displays != NULL && display_count >= 2);
    const SDL_DisplayID selected_display = displays[1];
    SDL_free(displays);
    SDK_CHECK(SDL_SetWindowPosition(mc_client_display_window(display),
                                    (int)SDL_WINDOWPOS_CENTERED_DISPLAY(selected_display),
                                    (int)SDL_WINDOWPOS_CENTERED_DISPLAY(selected_display)) &&
              SDL_GetDisplayForWindow(mc_client_display_window(display)) == selected_display);
#endif
    input = mc_client_display_inputs(display);
    SDK_CHECK(input != NULL && mc_client_sdk_input_inspect(input, &input_state) &&
              input_state.phase == MC_CLIENT_SDK_INPUT_READY);
    SDK_CHECK(fresh_with_input(&f, input) == 0 &&
              mc_client_screen_sign_initialize(f.screen, &error) == MC_CLIENT_SCREEN_OK &&
              error.source_returned && error.site.pc == 51 &&
              mc_client_sdk_input_inspect(input, &input_state) && input_state.repeat_enabled);
    mc_client_screen_state base;
    mc_client_screen_list_state list;
    SDK_CHECK(mc_client_screen_inspect(mc_client_screen_sign_base(f.screen), &base, NULL) ==
                  MC_CLIENT_SCREEN_OK &&
              base.da.b == NULL && mc_client_screen_list_inspect(base.da.e, &list) &&
              list.size == 1 &&
              mc_client_screen_closed(mc_client_screen_sign_base(f.screen), &error) ==
                  MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE &&
              !error.native_policy && error.source_exception && error.site.pc == 8 &&
              strcmp(error.site.class_name, "yc") == 0 && strcmp(error.site.method, "h") == 0 &&
              mc_client_sdk_input_inspect(input, &input_state) && !input_state.repeat_enabled &&
              unchanged_tile(&f) == 0);

    SDK_CHECK(fresh_with_input(&f, input) == 0);
    /* Explicit raw public App defaults with a genuine registered native owner.
     * No App constructor, forged ready phase or private YC field is supplied. */
    app = calloc(1, sizeof(*app));
    SDK_CHECK(app != NULL &&
              mc_client_application_lifetime_register(app, NULL) == MC_CLIENT_APPLICATION_OWNER_OK);
    app_registered = true;
    SDK_CHECK(mc_client_screen_initialize(mc_client_screen_sign_base(f.screen), app, 320, 200,
                                          &initialization) == MC_CLIENT_SCREEN_OK &&
              mc_client_screen_inspect(mc_client_screen_sign_base(f.screen), &base, NULL) ==
                  MC_CLIENT_SCREEN_OK &&
              base.da.b == app && base.da.g == NULL && app->active_world == NULL &&
              mc_client_sdk_input_inspect(input, &input_state) && input_state.repeat_enabled &&
              mc_client_screen_closed(mc_client_screen_sign_base(f.screen), &error) ==
                  MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE &&
              !error.native_policy && error.source_exception && error.site.pc == 11 &&
              strcmp(error.site.class_name, "yc") == 0 && strcmp(error.site.method, "h") == 0 &&
              mc_client_sdk_input_inspect(input, &input_state) && !input_state.repeat_enabled);
    /* A declared public App.r prestate under the genuine App/YC lifetime.
     * This is not credited as Source publication/display completion. */
    SDK_CHECK(mc_client_screen_sign_initialize(f.screen, &error) == MC_CLIENT_SCREEN_OK &&
              mc_client_sdk_input_inspect(input, &input_state) && input_state.repeat_enabled &&
              mc_client_screen_inspect(mc_client_screen_sign_base(f.screen), &base, NULL) ==
                  MC_CLIENT_SCREEN_OK);
    const mc_client_source_ke *done = NULL;
    SDK_CHECK(mc_client_screen_list_button(base.da.e, 0, &done) && done != NULL);
    raw_r_receiver = mc_client_screen_sign_base(f.screen);
    app->r = raw_r_receiver;
    /* The list's returned const view names this real mutable KE owner. The
     * actual protected callback only reads it; its screen keeps it live. */
    SDK_CHECK(mc_client_screen_sign_button(f.screen, (mc_client_source_ke *)done, &done_call,
                                           &error) == MC_CLIENT_SCREEN_APP_FAILED &&
              !error.native_policy && error.source_exception && !error.source_error &&
              error.exception_class != NULL &&
              strcmp(error.exception_class, "java/lang/NullPointerException") == 0 &&
              error.site.class_name != NULL && strcmp(error.site.class_name, "yc") == 0 &&
              error.site.pc == 27 && error.parent_site.class_name != NULL &&
              strcmp(error.parent_site.class_name, "yc") == 0 && error.parent_site.method != NULL &&
              strcmp(error.parent_site.method, "h") == 0 && error.parent_site.pc == 11 &&
              done_call != NULL && !done_call->active && !done_call->r_stored &&
              done_call->old_screen == raw_r_receiver && done_call->site.class_name != NULL &&
              strcmp(done_call->site.class_name, "net/minecraft/client/Minecraft") == 0 &&
              done_call->site.pc == 22 && done_call->screen.source_exception &&
              !done_call->screen.native_policy && done_call->screen.site.pc == 11 &&
              mc_client_sdk_input_inspect(input, &input_state) && !input_state.repeat_enabled);
    const mc_client_screen_error closed = error;
    SDK_CHECK(mc_client_display_screen_retire(&done_call) == MC_CLIENT_DISPLAY_SCREEN_OK &&
              done_call == NULL && app->r == raw_r_receiver);
    app->r = NULL; /* End only this fixture-owned public view after its actual child. */
    raw_r_receiver = NULL;
    mc_client_display *held_display = display;
    SDK_CHECK(!mc_client_display_stage_retire(&display, &display_error) && display == held_display);
    mc_client_sign_screen_state state;
    SDK_CHECK(mc_client_screen_sign_terminal_prepare(f.screen, &error) == MC_CLIENT_SCREEN_OK &&
              mc_client_screen_sign_inspect(f.screen, &state, NULL) == MC_CLIENT_SCREEN_OK &&
              state.terminal_prepared && !state.input_held && !state.tile_held &&
              !state.literal_classes_held && state.failure->status == closed.status &&
              state.failure->site.pc == closed.site.pc && state.detail->dirty_returned &&
              state.detail->display_entered && !state.detail->display_returned &&
              mc_client_screen_sign_retire(&f.screen, &error) == MC_CLIENT_SCREEN_OK &&
              f.screen == NULL);
    result = 0;
cleanup:
    if (done_call != NULL &&
        mc_client_display_screen_retire(&done_call) != MC_CLIENT_DISPLAY_SCREEN_OK) {
        fprintf(stderr, "YC SDK actual display child remains retained\n");
        return 1; /* Preserve its App/old-YC borrows rather than free through them. */
    }
    if (raw_r_receiver != NULL && app != NULL && app->r == raw_r_receiver)
        app->r = NULL; /* Native ending of the declared raw fixture view only. */
    if (teardown(&f) != 0)
        result = 1;
    if (app_registered &&
        mc_client_application_lifetime_retire(app, NULL) != MC_CLIENT_APPLICATION_OWNER_OK) {
        fprintf(stderr, "YC SDK retained App failed to retire\n");
        result = 1;
    } else
        free(app);
    if (display != NULL && !mc_client_display_stage_retire(&display, &display_error)) {
        fprintf(stderr, "YC SDK Display failed terminal retirement\n");
        result = 1;
    }
    mc_client_display_mode_release(&mode);
    if (icons != NULL &&
        mc_client_sdk_icons_release(&icons, &icons_error) != MC_CLIENT_SDK_ICONS_OK)
        result = 1;
    if (files != NULL && mc_resource_files_free(files) != MC_REGISTRY_OK)
        result = 1;
#undef SDK_CHECK
    return result;
}

int main(int argc, char **argv) {
    if (argc != 1 && !(argc == 3 && strcmp(argv[1], "--sdk") == 0))
        return 2;
    fixture f = {0};
    if (setup(&f, SIZE_MAX) != 0)
        return 1;
    int result = text_and_navigation(&f);
    if (result == 0)
        result = utf16_and_identity(&f);
    if (result == 0)
        result = late_null_rows(&f);
    if (result == 0)
        result = semantic_flag_prestate(&f);
    if (result == 0)
        result = direct_button_guard(&f);
    if (result == 0)
        result = constructor_prefix(&f);
    if (result == 0)
        result = retained_sign_receipt(&f);
    if (result == 0)
        result = retained_class_and_terminal(&f);
    if (teardown(&f) != 0)
        result = 1;
    if (result == 0)
        result = failed_class();
    if (result == 0 && argc == 3)
        result = sdk_lifecycle(argv[2]);
    if (result == 0)
        printf("PASS: %u YC source-order/UTF16/owner checks\n", checks);
    return result;
}
