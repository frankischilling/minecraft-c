/* Authored UTF16/resources and genuine DA, Registry, Font and FP owners.
 * No display, imported assets, original classes or fabricated ready Font. */
#include "block/registry/runtime_internal.h"
#include "client/gui/bitmap_font_internal.h"
#include "client/gui/source_text_field_internal.h"
#include "client/render_engine/nw.h"
#include "mc/block/material/material.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

static unsigned int checks;
#define COUNT(a) (sizeof(a) / sizeof((a)[0]))
#define CHECK(c)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(c)) {                                                                                \
            fprintf(stderr, "RO check%u line%d: %s\n", checks, __LINE__, #c);                      \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

typedef struct fixture {
    const uint8_t *bytes;
    size_t length, cursor, opens, closes;
    mc_client_source_ro *field;
    mc_client_source_da *parent;
    mc_player_string *paste;
    size_t tabs, clips;
    bool callback_bad, busy, clipboard_fail;
} fixture;

static mc_java_properties_status read_bytes(void *context, uint8_t *bytes, size_t capacity,
                                            size_t *out) {
    fixture *f = context;
    size_t count = f->length - f->cursor;
    if (count > capacity)
        count = capacity;
    memcpy(bytes, f->bytes + f->cursor, count);
    f->cursor += count;
    *out = count;
    return MC_JAVA_PROPERTIES_OK;
}

static mc_registry_status opened(void *context, const char *name, mc_java_properties_input *out) {
    fixture *f = context;
    if (strcmp(name, "/font.txt") != 0)
        return MC_REGISTRY_NULL_REFERENCE;
    ++f->opens;
    f->cursor = 0;
    *out = (mc_java_properties_input){f, read_bytes};
    return MC_REGISTRY_OK;
}

static mc_registry_status closed(void *context, const mc_java_properties_input *stream) {
    fixture *f = context;
    if (stream->context != f)
        return MC_REGISTRY_INVALID_ARGUMENT;
    ++f->closes;
    return MC_REGISTRY_OK;
}

static bool ready(void *context, const mc_java_properties_input *stream) {
    fixture *f = context;
    return stream->context == f && f->cursor < f->length;
}

static bool tab(void *context, mc_client_source_da *parent, mc_client_screen_error *error) {
    fixture *f = context;
    ++f->tabs;
    f->callback_bad |= parent != f->parent || !f->field->active;
    mc_client_source_ro *slot = f->field;
    mc_client_screen_error refused;
    f->busy = !mc_screen_text_field_release_locked(&slot, &refused) && slot == f->field &&
              refused.status == MC_CLIENT_SCREEN_BUSY;
    return mc_screen_text_field_focus_locked(f->field, false, error);
}

static bool clipboard(void *context, mc_player_string **out, mc_client_screen_error *error) {
    fixture *f = context;
    ++f->clips;
    if (*out != NULL || !f->field->active)
        f->callback_bad = true;
    if (f->clipboard_fail) {
        mc_screen_fail(error, MC_CLIENT_SCREEN_LIMIT, "authored clipboard native refusal", "da",
                       "d", "()Ljava/lang/String;", 0);
        return false;
    }
    if (f->paste != NULL && mc_player_string_retain(f->paste) != MC_PLAYER_OK)
        return false;
    *out = f->paste;
    return true;
}

static bool text_is(mc_client_source_ro *o, const uint16_t *units, size_t length) {
    mc_player_string_view value;
    return mc_client_screen_text_read(o->fields.h, &value) && value.length == length &&
           (length == 0 || memcmp(value.units, units, length * sizeof(*units)) == 0);
}

static int set(mc_client_source_ro *o, const uint16_t *units, size_t length) {
    mc_player_string *value = NULL;
    CHECK(mc_player_string_create(units, length, &value) == MC_PLAYER_OK &&
          mc_screen_text_field_set_text_locked(
              o, (mc_client_screen_text){MC_CLIENT_SCREEN_TEXT_PLAYER, value, NULL}, NULL));
    mc_player_string_release(value);
    CHECK(text_is(o, units, length));
    return 0;
}

int main(void) {
    static const uint8_t whitelist[] = "#authored comment\nABCD\t\xc3\xa9\n";
    fixture f = {.bytes = whitelist, .length = sizeof(whitelist) - 1};
    const mc_registry_resources resources = {&f, opened, closed, ready};
    mc_java_random_profile *profile = NULL;
    mc_materials *materials = NULL;
    mc_registry_classes *registry = NULL;
    mc_client_screen_classes *screens = NULL;
    mc_client_source_da *parent = NULL;
    mc_client_render_engine_classes *graphics = NULL;
    mc_client_loading_classes *gui = NULL;
    mc_client_bitmap_font *font = NULL;
    mc_client_screen_error error;
    CHECK(mc_java_random_profile_create(SIZE_MAX, &profile) == MC_JAVA_RANDOM_PROFILE_OK &&
          mc_materials_create(&materials));
    const mc_registry_options options = {MC_REGISTRY_CLIENT, profile, materials, 0, &resources};
    CHECK(mc_registry_classes_allocate(&options, &registry, NULL) == MC_REGISTRY_OK &&
          mc_client_screen_classes_create(profile, &resources, registry, &screens, NULL) ==
              MC_CLIENT_SCREEN_OK &&
          mc_client_screen_allocate(screens, NULL, &parent, NULL) == MC_CLIENT_SCREEN_OK &&
          mc_client_screen_construct(parent, NULL) == MC_CLIENT_SCREEN_OK);
    CHECK(mc_client_render_engine_classes_allocate_context(NULL, NULL, NULL, &graphics) ==
              MC_CLIENT_RENDER_ENGINE_OK &&
          mc_client_loading_classes_create(graphics, &gui) == MC_CLIENT_LOADING_OK &&
          mc_client_loading_classes_bind_resources(gui, &resources, NULL) == MC_CLIENT_LOADING_OK &&
          mc_client_bitmap_font_allocate(gui, NULL, &font) == MC_CLIENT_BITMAP_FONT_OK);
    const mc_registry_string *initial = NULL;
    CHECK(mc_registry_runtime_literal_ascii(registry, "AB", &initial, NULL) == MC_REGISTRY_OK &&
          mc_screen_lock(screens, &error));
    f.parent = parent;
    f.field = mc_screen_text_field_allocate_locked(parent, &error);
    CHECK(f.field != NULL &&
          mc_screen_text_field_construct_locked(
              f.field, parent, font, 10, 20, 200, 20,
              (mc_client_screen_text){MC_CLIENT_SCREEN_TEXT_REGISTRY, NULL, initial}, &error));
    mc_client_source_ro *o = f.field;
    mc_client_text_field_state state;
    CHECK(mc_client_text_field_inspect(o, &state) && state.phase == MC_CLIENT_SCREEN_READY &&
          state.fields.c == font && state.fields.l == parent &&
          state.fields.h.registry == initial &&
          state.fields.h.kind == MC_CLIENT_SCREEN_TEXT_REGISTRY && state.fields.d == 10 &&
          state.fields.e == 20 && state.fields.f == 200 && state.fields.g == 20 &&
          state.fields.i == 0 && state.fields.j == 0 && !state.fields.a && state.fields.b &&
          state.fields.k == 0 && state.site.pc == 53 && f.opens == 0);
    const mc_client_text_field_hooks hooks = {gui->mapping, &f, tab, clipboard};
    CHECK(mc_screen_text_field_key_locked(o, 'A', 0, NULL, &error) && o->site.pc == 14 &&
          o->fields.h.registry == initial && f.opens == 0);
    CHECK(mc_screen_text_field_focus_locked(o, true, &error) &&
          mc_screen_text_field_set_maximum_locked(o, 2, &error) &&
          mc_screen_text_field_key_locked(o, 'C', 0, &hooks, &error) &&
          o->fields.h.registry == initial && f.opens == 1 && f.closes == 1);
    CHECK(mc_screen_text_field_set_maximum_locked(o, 0, &error) &&
          mc_screen_text_field_key_locked(o, 'C', 0, &hooks, &error) &&
          o->fields.h.kind == MC_CLIENT_SCREEN_TEXT_PLAYER &&
          text_is(o, (const uint16_t[]){'A', 'B', 'C'}, 3));
    CHECK(mc_screen_text_field_key_locked(o, '?', 0, &hooks, &error) &&
          text_is(o, (const uint16_t[]){'A', 'B', 'C'}, 3) &&
          mc_screen_text_field_key_locked(o, 0x00e9, 0, &hooks, &error) &&
          text_is(o, (const uint16_t[]){'A', 'B', 'C', 0x00e9}, 4) &&
          mc_screen_text_field_key_locked(o, 0, 14, &hooks, &error) &&
          mc_screen_text_field_key_locked(o, 'D', 14, &hooks, &error) &&
          text_is(o, (const uint16_t[]){'A', 'B', 'D'}, 3));
    CHECK(mc_screen_text_field_set_maximum_locked(o, -1, &error) &&
          mc_screen_text_field_key_locked(o, 'A', 0, &hooks, &error) &&
          text_is(o, (const uint16_t[]){'A', 'B', 'D'}, 3));
    CHECK(mc_screen_text_field_set_maximum_locked(o, 0, &error) &&
          mc_screen_text_field_key_locked(o, 9, 0, &hooks, &error) && f.tabs == 1 && f.busy &&
          !f.callback_bad && !o->fields.a && text_is(o, (const uint16_t[]){'A', 'B', 'D', 9}, 4) &&
          o->failure.status == MC_CLIENT_SCREEN_OK);
    CHECK(mc_screen_text_field_focus_locked(o, true, &error));
    o->fields.j = INT32_MAX; /* Declared wrapping cursor input. */
    CHECK(mc_screen_text_field_tick_locked(o, &error) && o->fields.j == INT32_MIN);
    CHECK(mc_screen_text_field_focus_locked(o, true, &error) && o->fields.j == INT32_MIN &&
          mc_screen_text_field_focus_locked(o, false, &error) && o->fields.j == INT32_MIN &&
          mc_screen_text_field_focus_locked(o, true, &error) && o->fields.j == 0);
    CHECK(mc_screen_text_field_mouse_locked(o, 10, 20, 99, &error) && o->fields.a &&
          mc_screen_text_field_mouse_locked(o, 210, 20, 0, &error) && !o->fields.a &&
          mc_screen_text_field_mouse_locked(o, 10, 40, -1, &error) && !o->fields.a);
    o->fields.a = true;
    o->fields.b = false; /* Disabled mouse calls still execute a(false). */
    CHECK(mc_screen_text_field_key_locked(o, 'A', 0, NULL, &error) && o->site.pc == 14 &&
          mc_screen_text_field_mouse_locked(o, 10, 20, 0, &error) && !o->fields.a);
    o->fields.b = true;
    o->fields.d = INT32_MAX - 2;
    o->fields.f = 5; /* Java iadd wraps the right bound; do not widen geometry. */
    CHECK(mc_screen_text_field_mouse_locked(o, INT32_MAX - 1, 20, 0, &error) && !o->fields.a);
    o->fields.d = 10;
    o->fields.f = 200;
    CHECK(mc_screen_text_field_focus_locked(o, true, &error) && set(o, (uint16_t[]){'A'}, 1) == 0 &&
          mc_screen_text_field_set_maximum_locked(o, 2, &error));
    const uint16_t raw_paste[] = {'?', ':', 0, 0xd83d, 0xde00};
    CHECK(mc_player_string_create(raw_paste, COUNT(raw_paste), &f.paste) == MC_PLAYER_OK &&
          mc_screen_text_field_key_locked(o, 22, 0, &hooks, &error) && f.clips == 1 &&
          text_is(o, (uint16_t[]){'A', '?', ':', 0, 0xd83d, 0xde00}, 6));
    mc_player_string_release(f.paste);
    f.paste = NULL;
    CHECK(mc_screen_text_field_key_locked(o, 0, 14, &hooks, &error) &&
          text_is(o, (uint16_t[]){'A', '?', ':', 0, 0xd83d}, 5));
    uint16_t long_text[40], clipboard_text[40];
    for (size_t i = 0; i < COUNT(long_text); ++i) {
        long_text[i] = 'A';
        clipboard_text[i] = '?';
    }
    CHECK(set(o, NULL, 0) == 0 &&
          mc_player_string_create(clipboard_text, COUNT(clipboard_text), &f.paste) ==
              MC_PLAYER_OK &&
          mc_screen_text_field_key_locked(o, 22, 0, &hooks, &error) &&
          text_is(o, clipboard_text, 32));
    CHECK(set(o, long_text, COUNT(long_text)) == 0);
    const mc_player_string *same = o->fields.h.player;
    CHECK(mc_screen_text_field_key_locked(o, 22, 0, &hooks, &error) && o->fields.h.player == same &&
          text_is(o, long_text, COUNT(long_text)));
    f.clipboard_fail = true;
    CHECK(!mc_screen_text_field_key_locked(o, 22, 14, &hooks, &error) &&
          error.status == MC_CLIENT_SCREEN_LIMIT && strcmp(error.site.class_name, "da") == 0 &&
          error.parent_site.pc == 34 && o->fields.h.player == same);
    f.clipboard_fail = false;
    mc_player_string_release(f.paste);
    f.paste = NULL;
    CHECK(set(o, (uint16_t[]){'A'}, 1) == 0);
    mc_client_text_field_hooks missing_mapping = hooks;
    missing_mapping.mapping = NULL;
    CHECK(!mc_screen_text_field_key_locked(o, 0, 14, &missing_mapping, &error) &&
          error.site.pc == 145 && error.native_policy && text_is(o, NULL, 0) &&
          o->fields.h.player == mc_player_empty_literal());
    CHECK(mc_screen_text_field_set_text_locked(o, (mc_client_screen_text){0}, &error) &&
          mc_screen_text_field_key_locked(o, '?', 0, &hooks, &error) &&
          !mc_screen_text_field_key_locked(o, 'A', 0, &hooks, &error) &&
          error.status == MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE && error.site.pc == 159 &&
          !mc_screen_text_field_key_locked(o, 0, 14, &hooks, &error) && error.site.pc == 118 &&
          !mc_screen_text_field_key_locked(o, 22, 0, &hooks, &error) && error.site.pc == 51);
    o->fields.l = NULL; /* Declared nullable parent; failure precedes FP use. */
    CHECK(!mc_screen_text_field_key_locked(o, 9, 0, &hooks, &error) && error.site.pc == 25 &&
          error.status == MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE && f.tabs == 1);
    o->fields.l = parent;
    CHECK(!f.callback_bad && f.opens == 1 && f.closes == 1);
    CHECK(mc_screen_text_field_release_locked(&f.field, &error) && f.field == NULL &&
          !mc_client_text_field_inspect(o, &state));
    CHECK(mtx_unlock(&screens->mutex) == thrd_success &&
          mc_client_screen_retire(&parent, &error) == MC_CLIENT_SCREEN_OK && parent == NULL &&
          mc_client_screen_classes_release(&screens, &error) == MC_CLIENT_SCREEN_OK);
    CHECK(mc_client_bitmap_font_retire(&font, NULL) == MC_CLIENT_BITMAP_FONT_OK && font == NULL &&
          mc_client_loading_classes_release(gui) == MC_CLIENT_LOADING_OK &&
          mc_client_render_engine_classes_release(graphics) == MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(mc_registry_classes_free(registry, NULL) == MC_REGISTRY_OK);
    mc_materials_destroy(materials);
    mc_java_random_profile_free(profile);
    printf("PASS: %u RO text-field source-order/ownership checks\n", checks);
    return 0;
}
