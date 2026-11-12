#include "mc/client/settings/construction.h"

#include <stdio.h>
#include <string.h>

static unsigned int checks;
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition);                        \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

static bool same_float(float a, float b) {
    uint32_t x, y;
    memcpy(&x, &a, sizeof(x));
    memcpy(&y, &b, sizeof(y));
    return x == y;
}

static bool same_text(mc_game_settings_text a, mc_game_settings_text b) {
    return a.units == b.units && a.length == b.length && a.is_null == b.is_null;
}

/* Compare source values and reference identities, excluding only the explicit
 * native counter diagnostic. No structure-padding comparison or state writes. */
static bool same_state(const mc_game_settings_state *a, const mc_game_settings_state *b) {
    const mc_game_settings_values *x = &a->values, *y = &b->values;
    if (!same_float(x->music_volume, y->music_volume) ||
        !same_float(x->sound_volume, y->sound_volume) ||
        !same_float(x->mouse_sensitivity, y->mouse_sensitivity) ||
        x->invert_mouse != y->invert_mouse || x->render_distance != y->render_distance ||
        x->view_bobbing != y->view_bobbing || x->anaglyph != y->anaglyph ||
        x->advanced_opengl != y->advanced_opengl || x->performance_mode != y->performance_mode ||
        x->fancy_graphics != y->fancy_graphics || x->ambient_occlusion != y->ambient_occlusion ||
        !same_text(x->texture_pack, y->texture_pack) || x->difficulty != y->difficulty ||
        x->hide_gui != y->hide_gui || x->third_person_view != y->third_person_view ||
        x->show_debug_info != y->show_debug_info || !same_text(x->last_server, y->last_server) ||
        x->source_D != y->source_D || x->smooth_camera != y->smooth_camera ||
        x->debug_camera != y->debug_camera || !same_float(x->source_G, y->source_G) ||
        !same_float(x->source_H, y->source_H) || x->gui_scale != y->gui_scale ||
        a->bindings != b->bindings || a->binding_count != b->binding_count ||
        a->bindings_null != b->bindings_null || a->minecraft != b->minecraft ||
        a->options_path != b->options_path || a->file_identity != b->file_identity)
        return false;
    for (size_t i = 0; i < MC_GAME_SETTINGS_NAMED_BINDINGS; ++i)
        if (a->named[i] != b->named[i])
            return false;
    return true;
}

static bool defaults(void) {
    bool ok = false;
    mc_game_settings *settings = NULL, *first = NULL, *second = NULL, *empty = NULL;
    mc_game_settings_error error = {0};
    mc_game_settings_state before, after;
    CHECK(mc_game_settings_create_defaults(NULL, &settings, &error) == MC_GAME_SETTINGS_OK &&
          mc_game_settings_read_state(settings, &before) && before.native_borrows == 0);
    CHECK(mc_game_settings_borrow(settings, &error) == MC_GAME_SETTINGS_OK);
    first = settings;
    CHECK(mc_game_settings_borrow(settings, NULL) == MC_GAME_SETTINGS_OK);
    second = settings;
    CHECK(first == settings && second == settings &&
          mc_game_settings_read_state(settings, &after) && after.native_borrows == 2 &&
          same_state(&before, &after));
    mc_game_settings *same = settings;
    CHECK(mc_game_settings_retire(&settings, &error) == MC_GAME_SETTINGS_BUSY && settings == same &&
          error.status == MC_GAME_SETTINGS_BUSY && error.system_error == 0 &&
          mc_game_settings_read_state(settings, &after) && after.native_borrows == 2 &&
          same_state(&before, &after));
    CHECK(mc_game_settings_release_borrow(&first, &error) == MC_GAME_SETTINGS_OK && first == NULL &&
          mc_game_settings_read_state(settings, &after) && after.native_borrows == 1 &&
          mc_game_settings_retire(&settings, NULL) == MC_GAME_SETTINGS_BUSY && settings == same);
    CHECK(mc_game_settings_release_borrow(&second, NULL) == MC_GAME_SETTINGS_OK && second == NULL &&
          mc_game_settings_read_state(settings, &after) && after.native_borrows == 0 &&
          same_state(&before, &after)); /* Last release did not free this owner. */
    mc_game_settings *unborrowed = settings;
    CHECK(mc_game_settings_release_borrow(&unborrowed, &error) ==
              MC_GAME_SETTINGS_INVALID_ARGUMENT &&
          unborrowed == settings && error.status == MC_GAME_SETTINGS_INVALID_ARGUMENT &&
          mc_game_settings_borrow(NULL, &error) == MC_GAME_SETTINGS_INVALID_ARGUMENT &&
          mc_game_settings_release_borrow(NULL, &error) == MC_GAME_SETTINGS_INVALID_ARGUMENT &&
          mc_game_settings_release_borrow(&empty, &error) == MC_GAME_SETTINGS_OK && empty == NULL &&
          mc_game_settings_read_state(settings, &after) && after.native_borrows == 0 &&
          same_state(&before, &after));
    ok = true;
cleanup:
    if (mc_game_settings_release_borrow(&first, NULL) != MC_GAME_SETTINGS_OK ||
        mc_game_settings_release_borrow(&second, NULL) != MC_GAME_SETTINGS_OK ||
        mc_game_settings_retire(&settings, NULL) != MC_GAME_SETTINGS_OK)
        return false;
    return ok;
}

typedef struct active_observation {
    mc_game_settings *settings, *borrow;
    size_t calls;
    bool checked;
} active_observation;

static mc_game_settings_status refuse_active(void *context,
                                             const mc_game_settings_diagnostic *event,
                                             mc_game_settings_error *error) {
    active_observation *o = context;
    ++o->calls;
    mc_game_settings_state before, after;
    mc_game_settings *retirement_slot = o->settings;
    o->checked = event->kind == MC_GAME_SETTINGS_SAVE_FAILED &&
                 event->exception == MC_GAME_SETTINGS_SOURCE_NULL_FILE &&
                 mc_game_settings_read_state(o->settings, &before) && before.native_borrows == 1 &&
                 mc_game_settings_borrow(o->settings, NULL) == MC_GAME_SETTINGS_BUSY &&
                 mc_game_settings_release_borrow(&o->borrow, NULL) == MC_GAME_SETTINGS_BUSY &&
                 o->borrow == o->settings &&
                 mc_game_settings_retire(&retirement_slot, NULL) == MC_GAME_SETTINGS_BUSY &&
                 retirement_slot == o->settings &&
                 mc_game_settings_read_state(o->settings, &after) && after.native_borrows == 1 &&
                 same_state(&before, &after);
    /* A real production diagnostic observer deliberately REFUSES delivery.
     * No successful callback stands in for a missing game/IO operation. */
    if (error != NULL)
        *error = (mc_game_settings_error){MC_GAME_SETTINGS_DIAGNOSTIC_FAILED, 0,
                                          "authored active borrow diagnostic refusal"};
    return MC_GAME_SETTINGS_DIAGNOSTIC_FAILED;
}

static bool active_gate(void) {
    bool ok = false;
    active_observation observation = {0};
    mc_game_settings_error error = {0};
    CHECK(mc_game_settings_create_defaults(NULL, &observation.settings, &error) ==
          MC_GAME_SETTINGS_OK);
    CHECK(mc_game_settings_borrow(observation.settings, &error) == MC_GAME_SETTINGS_OK);
    observation.borrow = observation.settings;
    const mc_game_settings_diagnostics sink = {&observation, refuse_active};
    mc_game_settings_report report = {0};
    CHECK(mc_game_settings_save(observation.settings, &sink, &report, &error) ==
              MC_GAME_SETTINGS_DIAGNOSTIC_FAILED &&
          error.status == MC_GAME_SETTINGS_DIAGNOSTIC_FAILED && observation.calls == 1 &&
          observation.checked && report.outer_exception_caught &&
          report.outer_exception == MC_GAME_SETTINGS_SOURCE_NULL_FILE);
    CHECK(mc_game_settings_release_borrow(&observation.borrow, &error) == MC_GAME_SETTINGS_OK &&
          observation.borrow == NULL);
    mc_game_settings_state after;
    CHECK(mc_game_settings_read_state(observation.settings, &after) && after.native_borrows == 0);
    ok = true;
cleanup:
    if (mc_game_settings_release_borrow(&observation.borrow, NULL) != MC_GAME_SETTINGS_OK ||
        mc_game_settings_retire(&observation.settings, NULL) != MC_GAME_SETTINGS_OK)
        return false;
    return ok;
}

static bool same_source(const mc_game_settings_source_state *a,
                        const mc_game_settings_source_state *b) {
    return a->phase == b->phase && a->N == b->N && a->pending_file == b->pending_file &&
           a->pending_stream == b->pending_stream && a->classes == b->classes &&
           a->failure == b->failure && a->retirement == b->retirement &&
           a->load_report == b->load_report && a->source_stores == b->source_stores &&
           a->stores == b->stores && a->last_store_pc == b->last_store_pc &&
           a->last_load_pc == b->last_load_pc && a->last_save_pc == b->last_save_pc &&
           a->save_writer == b->save_writer && a->active == b->active &&
           a->reader_present == b->reader_present;
}

static bool same_class(const mc_game_settings_class_state *a,
                       const mc_game_settings_class_state *b) {
    return a->phase == b->phase && a->holds == b->holds && a->array_bytes == b->array_bytes &&
           a->arrays == b->arrays && a->J == b->J && a->K == b->K && a->L == b->L && a->M == b->M &&
           a->options_literal == b->options_literal && a->pending_array == b->pending_array &&
           a->pending_length == b->pending_length && a->stdout_captured == b->stdout_captured &&
           a->stdout_borrowed == b->stdout_borrowed && a->last_store_pc == b->last_store_pc &&
           a->failure == b->failure && a->retirement == b->retirement;
}

static bool source_owner(bool failed_constructor) {
    bool ok = false;
    mc_java_files *files = NULL;
    mc_game_settings_classes *classes = NULL;
    mc_game_settings *settings = NULL, *borrow = NULL;
    mc_game_settings_source_error error = {0};
    mc_game_settings_error native = {0};
    mc_java_file_options options = mc_java_file_default_options();
    if (failed_constructor)
        options.max_path_units = 10; /* Real bound: options.txt needs11 units. */
    CHECK(mc_java_files_create(&options, &files, &error.file) == MC_JAVA_FILE_OK &&
          mc_game_settings_classes_create(files, NULL, NULL, &classes, &error) ==
              MC_GAME_SETTINGS_OK &&
          mc_game_settings_source_allocate(classes, NULL, &settings, &error) ==
              MC_GAME_SETTINGS_OK);
    if (failed_constructor)
        CHECK(mc_game_settings_source_construct(settings, NULL, NULL, NULL, &error) ==
                  MC_GAME_SETTINGS_FILE_ERROR &&
              error.site.pc == 368 && error.file.status == MC_JAVA_FILE_LIMIT);
    mc_game_settings_source_state before, after;
    mc_game_settings_state values_before, values_after;
    mc_game_settings_class_state class_before, class_after;
    CHECK(mc_game_settings_source_inspect(settings, &before) &&
          before.phase ==
              (failed_constructor ? MC_GAME_SETTINGS_SOURCE_FAILED : MC_GAME_SETTINGS_SOURCE_RAW) &&
          !before.active && before.native_borrows == 0 &&
          mc_game_settings_read_state(settings, &values_before) &&
          values_before.native_borrows == 0 &&
          mc_game_settings_classes_inspect(classes, &class_before));
    const mc_game_settings_source_error failure = *before.failure;
    const mc_game_settings_source_error retirement = *before.retirement;
    CHECK(mc_game_settings_borrow(settings, &native) == MC_GAME_SETTINGS_OK);
    borrow = settings;
    mc_game_settings *same = settings;
    CHECK(mc_game_settings_retire(&settings, &native) == MC_GAME_SETTINGS_BUSY &&
          settings == same && mc_game_settings_source_inspect(settings, &after) &&
          after.native_borrows == 1 && same_source(&before, &after) &&
          after.failure->native.status == failure.native.status &&
          after.failure->site.pc == failure.site.pc &&
          after.failure->file.status == failure.file.status &&
          after.retirement->native.status == retirement.native.status &&
          after.retirement->site.pc == retirement.site.pc &&
          mc_game_settings_read_state(settings, &values_after) &&
          values_after.native_borrows == 1 && same_state(&values_before, &values_after) &&
          mc_game_settings_classes_inspect(classes, &class_after) &&
          same_class(&class_before, &class_after));
    CHECK(mc_game_settings_release_borrow(&borrow, &native) == MC_GAME_SETTINGS_OK &&
          borrow == NULL && mc_game_settings_source_inspect(settings, &after) &&
          after.native_borrows == 0 && same_source(&before, &after));
    CHECK(mc_game_settings_retire(&settings, &native) == MC_GAME_SETTINGS_OK && settings == NULL &&
          mc_game_settings_classes_inspect(classes, &class_after) &&
          class_after.holds + 1 == class_before.holds);
    ok = true;
cleanup:
    if (mc_game_settings_release_borrow(&borrow, NULL) != MC_GAME_SETTINGS_OK ||
        mc_game_settings_retire(&settings, NULL) != MC_GAME_SETTINGS_OK)
        return false;
    if (mc_game_settings_classes_release(&classes, &error) != MC_GAME_SETTINGS_OK ||
        mc_java_files_release(&files, &error.file) != MC_JAVA_FILE_OK)
        return false;
    return ok;
}

int main(void) {
    if (!defaults() || !active_gate() || !source_owner(false) || !source_owner(true))
        return 1;
    if (printf("PASS: %u Settings instance borrow checks\n", checks) < 0 || ferror(stdout))
        return 1;
    return 0;
}
