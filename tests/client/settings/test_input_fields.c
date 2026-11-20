#include "../../../src/inventory/internal.h"
#include "mc/client/settings/construction.h"
#include "source_input.h"

#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xmmintrin.h>

static unsigned int checks;
#define CHECK(expression)                                                                          \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(expression)) {                                                                       \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expression);                       \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

static float from_bits(uint32_t bits) {
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static uint32_t float_bits(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static bool inventory_cases(void) {
    bool okay = false;
    mc_inventory_player *inventory = NULL, *server = NULL;
    mc_inventory_selection_error error;
    CHECK(mc_inventory_player_create(MC_REGISTRY_CLIENT, NULL, NULL, &inventory, NULL) ==
          MC_INVENTORY_OK);
    static const int32_t selected[] = {-28, -9, -1, 0, 1, 7, 8, 9, 28};
    static const int32_t wheels[] = {INT32_MIN, -120, -1, 0, 1, 120, INT32_MAX};
    mc_item_stack **main_slots = inventory->main_slots, **armor_slots = inventory->armor_slots;
    inventory->changed = true;
    for (size_t s = 0; s < sizeof(selected) / sizeof(*selected); ++s) {
        for (size_t w = 0; w < sizeof(wheels) / sizeof(*wheels); ++w) {
            inventory->selected = selected[s];
            CHECK(mc_inventory_player_scroll(inventory, wheels[w], &error) == MC_INVENTORY_OK);
            CHECK(error.source_entered && error.mutation_reached && !error.native_policy &&
                  error.site.pc == 66 && inventory->selected >= 0 && inventory->selected < 9);
            CHECK(inventory->main_slots == main_slots && inventory->armor_slots == armor_slots &&
                  inventory->player == NULL && inventory->changed && !inventory->active &&
                  inventory->cursor == NULL);
            for (size_t slot = 0; slot < 36; ++slot)
                CHECK(inventory->main_slots[slot] == NULL);
            for (size_t slot = 0; slot < 4; ++slot)
                CHECK(inventory->armor_slots[slot] == NULL);
            printf("SCROLL %" PRId32 " %" PRId32 " %" PRId32 "\n", selected[s], wheels[w],
                   inventory->selected);
        }
    }
    for (int32_t slot = 0; slot < 9; ++slot) {
        CHECK(mc_inventory_player_select_hotbar(inventory, slot, &error) == MC_INVENTORY_OK);
        CHECK(inventory->selected == slot && error.mutation_reached && !error.source_entered &&
              !error.native_policy && inventory->changed);
    }
    CHECK(mc_inventory_player_select_hotbar(inventory, -1, &error) ==
          MC_INVENTORY_INDEX_OUT_OF_BOUNDS);
    CHECK(inventory->selected == 8 && error.native_policy && !error.mutation_reached);
    CHECK(mc_inventory_player_select_hotbar(inventory, 9, &error) ==
          MC_INVENTORY_INDEX_OUT_OF_BOUNDS);
    CHECK(inventory->selected == 8);
    inventory->active = true;
    const mc_inventory_status busy_scroll = mc_inventory_player_scroll(inventory, 1, &error);
    inventory->active = false;
    CHECK(busy_scroll == MC_INVENTORY_BUSY && inventory->selected == 8 && !error.source_entered);
    CHECK(mc_inventory_player_scroll(NULL, 1, &error) == MC_INVENTORY_INVALID_ARGUMENT);
    CHECK(error.native_policy && !error.source_entered);
    CHECK(mc_inventory_player_create(MC_REGISTRY_SERVER, NULL, NULL, &server, NULL) ==
          MC_INVENTORY_OK);
    CHECK(mc_inventory_player_scroll(server, 1, &error) == MC_INVENTORY_UNIMPLEMENTED_DEPENDENCY);
    CHECK(server->selected == 0 && !error.source_entered);
    CHECK(mc_inventory_player_select_hotbar(server, 2, &error) ==
          MC_INVENTORY_UNIMPLEMENTED_DEPENDENCY);
    CHECK(server->selected == 0);
    okay = true;
cleanup:
    if (mc_inventory_player_retire(&inventory, NULL) != MC_INVENTORY_OK ||
        mc_inventory_player_retire(&server, NULL) != MC_INVENTORY_OK)
        okay = false;
    return okay;
}

static bool settings_cases(void) {
    bool okay = false;
    const unsigned int saved_mxcsr = _mm_getcsr();
    mc_game_settings *settings = NULL, *raw = NULL;
    mc_game_settings_state before, state;
    mc_game_settings_error error;
    mc_java_files *files = NULL;
    mc_game_settings_classes *classes = NULL;
    mc_game_settings_source_error *source_error = calloc(1, sizeof(*source_error));
    CHECK(source_error != NULL);
    CHECK(mc_game_settings_create_defaults(NULL, &settings, &error) == MC_GAME_SETTINGS_OK);
    CHECK(mc_game_settings_read_state(settings, &before));
    typedef mc_game_settings_status (*toggle)(mc_game_settings *, mc_game_settings_error *);
    static const toggle toggles[] = {mc_game_settings_source_input_toggle_hide_gui,
                                     mc_game_settings_source_input_toggle_debug,
                                     mc_game_settings_source_input_toggle_third_person,
                                     mc_game_settings_source_input_toggle_smooth_camera};
    for (size_t n = 0; n < sizeof(toggles) / sizeof(*toggles); ++n) {
        CHECK(toggles[n](settings, &error) == MC_GAME_SETTINGS_OK);
        CHECK(mc_game_settings_read_state(settings, &state));
        CHECK(state.values.hide_gui == (n == 0) && state.values.show_debug_info == (n == 1) &&
              state.values.third_person_view == (n == 2) && state.values.smooth_camera == (n == 3));
        CHECK(state.values.music_volume == before.values.music_volume &&
              state.values.mouse_sensitivity == before.values.mouse_sensitivity &&
              state.values.source_G == before.values.source_G &&
              state.values.source_D == before.values.source_D &&
              state.values.texture_pack.units == before.values.texture_pack.units &&
              state.bindings == before.bindings && state.file_identity == before.file_identity);
        CHECK(toggles[n](settings, &error) == MC_GAME_SETTINGS_OK);
    }
    CHECK(mc_game_settings_source_input_toggle_debug(NULL, &error) ==
          MC_GAME_SETTINGS_INVALID_ARGUMENT);

    static const struct {
        uint32_t initial;
        int32_t wheel;
        uint32_t expected;
    } cases[] = {{0x00000000, 1, 0x3e800000},         {0x00000000, -1, 0xbe800000},
                 {0x80000000, 0, 0x80000000},         {0x3f800000, INT32_MAX, 0x3fa00000},
                 {0x3f800000, INT32_MIN, 0x3f400000}, {0x7f800000, 1, 0x7f800000},
                 {0xff800000, -1, 0xff800000},        {0x00000001, 1, 0x3e800000},
                 {0x7f7fffff, -120, 0x7f7fffff},      {0xbf800000, -1, 0xbfa00000}};

    _mm_setcsr(0x1f80u);
    for (size_t n = 0; n < sizeof(cases) / sizeof(*cases); ++n) {
        CHECK(mc_game_settings_read_state(settings, &state));
        state.values.source_D = true;
        state.values.source_G = from_bits(cases[n].initial);
        CHECK(mc_game_settings_set_values(settings, &state.values, &error) == MC_GAME_SETTINGS_OK);
        mc_game_settings_source_input_wheel_report report;
        CHECK(mc_game_settings_source_input_wheel(settings, cases[n].wheel, &report, &error) ==
              MC_GAME_SETTINGS_OK);
        CHECK(mc_game_settings_read_state(settings, &state));
        CHECK(float_bits(state.values.source_G) == cases[n].expected);
        CHECK(report.D_read == (cases[n].wheel != 0) && report.G_read == (cases[n].wheel != 0) &&
              report.G_stored == (cases[n].wheel != 0));
        if (cases[n].wheel != 0)
            CHECK(report.source_pc == 418 && float_bits(report.before_G) == cases[n].initial &&
                  float_bits(report.after_G) == cases[n].expected);
    }
    state.values.source_D = false;
    state.values.source_G =
        from_bits(0x7fa12345); /* Untouched even with unsupported FP controls. */
    CHECK(mc_game_settings_set_values(settings, &state.values, &error) == MC_GAME_SETTINGS_OK);
    _mm_setcsr(0x9fc0u); /* FTZ and DAZ: forbidden only if source arithmetic is reached. */
    mc_game_settings_source_input_wheel_report report;
    CHECK(mc_game_settings_source_input_wheel(settings, 1, &report, &error) == MC_GAME_SETTINGS_OK);
    CHECK(report.D_read && !report.D && !report.G_read && !report.G_stored);
    CHECK(mc_game_settings_read_state(settings, &state) &&
          float_bits(state.values.source_G) == 0x7fa12345);
    CHECK(mc_game_settings_source_input_wheel(settings, 0, &report, &error) == MC_GAME_SETTINGS_OK);
    CHECK(!report.D_read && !report.G_read && !report.G_stored);
    state.values.source_D = true;
    state.values.source_G = 1.0f;
    CHECK(mc_game_settings_set_values(settings, &state.values, &error) == MC_GAME_SETTINGS_OK);
    CHECK(mc_game_settings_source_input_wheel(settings, 1, &report, &error) ==
          MC_GAME_SETTINGS_STATE_ERROR);
    CHECK(report.D_read && report.D && report.G_read && !report.G_stored &&
          report.source_pc == 413);
    CHECK(mc_game_settings_read_state(settings, &state) && state.values.source_G == 1.0f);
    _mm_setcsr(saved_mxcsr);
    CHECK(mc_java_files_create(NULL, &files, &source_error->file) == MC_JAVA_FILE_OK);
    CHECK(mc_game_settings_classes_create(files, NULL, NULL, &classes, source_error) ==
          MC_GAME_SETTINGS_OK);
    CHECK(mc_game_settings_source_allocate(classes, NULL, &raw, source_error) ==
          MC_GAME_SETTINGS_OK);
    CHECK(mc_game_settings_source_input_toggle_debug(raw, &error) == MC_GAME_SETTINGS_STATE_ERROR);
    memset(&report, 0x5a, sizeof(report));
    const mc_game_settings_source_input_wheel_report sentinel = report;
    CHECK(mc_game_settings_source_input_wheel(raw, 1, &report, &error) ==
          MC_GAME_SETTINGS_STATE_ERROR);
    CHECK(memcmp(&report, &sentinel, sizeof(report)) == 0);
    okay = true;
cleanup:
    _mm_setcsr(saved_mxcsr);
    if (mc_game_settings_retire(&settings, NULL) != MC_GAME_SETTINGS_OK ||
        mc_game_settings_retire(&raw, NULL) != MC_GAME_SETTINGS_OK ||
        mc_game_settings_classes_release(&classes, source_error) != MC_GAME_SETTINGS_OK ||
        mc_java_files_release(&files, source_error == NULL ? NULL : &source_error->file) !=
            MC_JAVA_FILE_OK)
        okay = false;
    free(source_error);
    return okay;
}

int main(void) {
    if (!inventory_cases() || !settings_cases())
        return 1;
    printf("%u client input field checks passed\n", checks);
    return 0;
}
