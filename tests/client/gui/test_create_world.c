/* Authored FJ/RO components on genuine owners. These do not invoke or credit
 * World loading, graphical initialization, an original process or pixels. */
#include "client/application_state.h"
#include "client/gui/source_create_world_internal.h"
#include "core/java/random_profile_internal.h"
#include "mc/resources/files/files.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned int checks;
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(x)) {                                                                                \
            fprintf(stderr, "Create World check%u line%d: %s\n", checks, __LINE__, #x);            \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

static bool text_is(mc_client_screen_text value, const char *expected) {
    mc_player_string_view view;
    if (!mc_client_screen_text_read(value, &view) || view.length != strlen(expected))
        return false;
    for (size_t i = 0; i < view.length; ++i)
        if (view.units[i] != (uint8_t)expected[i])
            return false;
    return true;
}

static bool set_units(mc_client_source_ro *field, const uint16_t *units, size_t length,
                      mc_client_screen_error *error) {
    mc_player_string *value = NULL;
    if (mc_player_string_create(units, length, &value) != MC_PLAYER_OK)
        return false;
    const bool result = mc_screen_text_field_set_text_locked(
        field, (mc_client_screen_text){MC_CLIENT_SCREEN_TEXT_PLAYER, value, NULL}, error);
    mc_player_string_release(value);
    return result;
}

static bool set_ascii(mc_client_source_ro *field, const char *text, mc_client_screen_error *error) {
    uint16_t units[128];
    const size_t length = strlen(text);
    if (length > sizeof units / sizeof units[0])
        return false;
    for (size_t i = 0; i < length; ++i)
        units[i] = (uint8_t)text[i];
    return set_units(field, units, length, error);
}

static int model(mc_client_source_fj *o) {
    mc_client_screen_error e = {0};
    /* These genuinely constructed children and source fields are declared
     * component inputs, not manufactured successful FJ.b initialization. */
    mc_create_world_initialization *r = mc_screen_allocate(&o->base.memory, sizeof(*r));
    CHECK(r != NULL);
    o->initializations = o->last_initialization = r;
    r->name = mc_screen_text_field_allocate_locked(&o->base, &e);
    r->seed = mc_screen_text_field_allocate_locked(&o->base, &e);
    mc_player_string *empty = NULL;
    CHECK(mc_player_string_create(NULL, 0, &empty) == MC_PLAYER_OK);
    const mc_client_screen_text literal = {MC_CLIENT_SCREEN_TEXT_PLAYER, empty, NULL};
    CHECK(r->name != NULL && r->seed != NULL &&
          mc_screen_text_field_construct_locked(r->name, &o->base, NULL, 113, 60, 200, 20, literal,
                                                &e) &&
          mc_screen_text_field_construct_locked(r->seed, &o->base, NULL, 113, 116, 200, 20, literal,
                                                &e));
    o->fields.i = r->name;
    o->fields.j = r->seed;
    o->fields.i->fields.a = true;
    CHECK(mc_screen_text_field_set_maximum_locked(o->fields.i, 32, &e));
    mc_client_screen_list *list = (mc_client_screen_list *)o->base.fields.e;
    CHECK(list != NULL && list->size == 0);
    list->entries = mc_screen_allocate(&o->base.memory, 2 * sizeof(*list->entries));
    CHECK(list->entries != NULL);
    list->capacity = 2;
    for (int32_t i = 0; i < 2; ++i) {
        r->buttons[i] = mc_screen_button_allocate_locked(o->base.classes, &e);
        CHECK(r->buttons[i] != NULL &&
              mc_screen_button_construct_locked(r->buttons[i], i, 113, 168 + 24 * i, 200, 20,
                                                literal, &e) &&
              mc_screen_button_hold_locked(r->buttons[i], &e));
        list->entries[list->size++] = r->buttons[i];
        ++r->button_count;
    }
    mc_player_string_release(empty);
    return 0;
}

static int naming(mc_client_source_fj *o) {
    mc_client_screen_error e = {0};
    CHECK(set_ascii(o->fields.i, "unchanged", &e));
    const mc_player_string *original = o->fields.i->fields.h.player;
    CHECK(mc_screen_create_world_name_locked(o, &e) && e.source_returned &&
          o->fields.l.kind == MC_CLIENT_SCREEN_TEXT_PLAYER && o->fields.l.player == original &&
          o->metadata_queries == 1 && text_is(o->fields.l, "unchanged"));
    CHECK(set_ascii(o->fields.i, "  raw name  ", &e) && mc_screen_create_world_name_locked(o, &e) &&
          text_is(o->fields.l, "raw name") && text_is(o->fields.i->fields.h, "  raw name  "));
    static const uint16_t prohibited[] = {'/', '\n', '\r', '\t', 0,   '\f', '`', '?',
                                          '*', '\\', '<',  '>',  '|', '"',  ':'};
    for (size_t i = 0; i < sizeof prohibited / sizeof prohibited[0]; ++i) {
        const uint16_t value[] = {'L', prohibited[i], 'R'};
        CHECK(set_units(o->fields.i, value, 3, &e) && mc_screen_create_world_name_locked(o, &e) &&
              text_is(o->fields.l, "L_R"));
    }
    const size_t queries = o->metadata_queries;
    CHECK(set_ascii(o->fields.i, " \t\r", &e) && mc_screen_create_world_name_locked(o, &e) &&
          text_is(o->fields.l, "World--") && o->metadata_queries == queries + 3);
    CHECK(set_ascii(o->fields.i, "empty_directory", &e) &&
          mc_screen_create_world_name_locked(o, &e) && text_is(o->fields.l, "empty_directory"));
    client_application *app = o->base.fields.b;
    o->base.fields.b = NULL; /* Authored nullable source receiver before the call. */
    CHECK(set_ascii(o->fields.i, "  prefix/  ", &e));
    const bool completed = mc_screen_create_world_name_locked(o, &e);
    o->base.fields.b = app;
    CHECK(!completed && e.status == MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE && !e.native_policy &&
          e.site.pc == 75 && text_is(o->fields.l, "prefix_") &&
          text_is(o->fields.i->fields.h, "  prefix/  "));
    return 0;
}

static int keys(mc_client_source_fj *o) {
    mc_client_screen_error e = {0};
    const mc_client_text_field_hooks hooks = {.mapping = o->mapping};
    CHECK(set_ascii(o->fields.i, "x", &e));
    const int32_t name_tick = o->fields.i->fields.j, seed_tick = o->fields.j->fields.j;
    CHECK(mc_screen_create_world_tick_locked(o, &e) && o->fields.i->fields.j == name_tick + 1 &&
          o->fields.j->fields.j == seed_tick + 1);
    CHECK(mc_screen_create_world_key_locked(o, 9, 15, &hooks, NULL, NULL, &e) &&
          !o->fields.i->fields.a && o->fields.j->fields.a && o->fields.j->fields.j == 0 &&
          text_is(o->fields.i->fields.h, "x"));
    CHECK(mc_screen_create_world_key_locked(o, 9, 15, &hooks, NULL, NULL, &e) &&
          o->fields.i->fields.a && !o->fields.j->fields.a && o->fields.i->fields.j == 0);
    CHECK(mc_screen_create_world_key_locked(o, 0, 14, &hooks, NULL, NULL, &e) &&
          text_is(o->fields.i->fields.h, "") && !o->last_initialization->buttons[0]->fields.g &&
          text_is(o->fields.l, "World--"));
    CHECK(mc_screen_create_world_key_locked(o, ' ', 57, &hooks, NULL, NULL, &e) &&
          text_is(o->fields.i->fields.h, " ") && o->last_initialization->buttons[0]->fields.g &&
          text_is(o->fields.l, "World--"));
    CHECK(mc_screen_create_world_mouse_after_base_locked(o, 114, 117, 1, &e) &&
          !o->fields.i->fields.a && o->fields.j->fields.a);
    CHECK(mc_screen_create_world_mouse_after_base_locked(o, -10, -10, 0, &e) &&
          !o->fields.i->fields.a && !o->fields.j->fields.a);
    CHECK(mc_screen_text_field_focus_locked(o->fields.i, true, &e));
    const size_t calls = o->naming_calls;
    CHECK(!mc_screen_create_world_key_locked(o, 13, 28, &hooks, NULL, NULL, &e) &&
          e.status == MC_CLIENT_SCREEN_UNIMPLEMENTED_DEPENDENCY && e.site.pc == 51 &&
          o->naming_calls == calls); /* No successful action/load is fabricated. */
    return 0;
}

static int seeds(mc_client_source_fj *o, const mc_java_random_constructor_input *inputs,
                 size_t input_count) {
    static const struct {
        const char *text;
        int64_t result;
        unsigned int mode; /* 0 default, 1 parsed nonzero, 2 hash. */
    } cases[] = {{"", 0, 0},
                 {"0", 0, 0},
                 {"+000", 0, 0},
                 {"-0", 0, 0},
                 {"17", 17, 1},
                 {"-9223372036854775808", INT64_MIN, 1},
                 {"9223372036854775807", INT64_MAX, 1},
                 {"9223372036854775808", INT64_C(-1773151197), 2},
                 {"-9223372036854775809", INT64_C(1304595159), 2},
                 {" seed ", INT64_C(1025446831), 2},
                 {"Aa", 2112, 2}};

    mc_client_screen_error e = {0};
    const bool guard = o->fields.m;
    const size_t count = sizeof cases / sizeof cases[0];
    CHECK(input_count == count + 3);
    for (size_t i = 0; i < count + 2; ++i) {
        if (i < count)
            CHECK(set_ascii(o->fields.j, cases[i].text, &e));
        else if (i == count) {
            static const uint16_t arabic[] = {0x661, 0x662};
            CHECK(set_units(o->fields.j, arabic, 2, &e));
        } else {
            static const uint16_t surrogate_pair[] = {0xd83d, 0xde00};
            CHECK(set_units(o->fields.j, surrogate_pair, 2, &e));
        }
        mc_java_random oracle;
        mc_java_random_finish_default(inputs[i].committed_uniquifier, inputs[i].nanotime, &oracle);
        const int64_t random = mc_java_random_next_i64(&oracle);
        int64_t chosen = INT64_C(0x1122334455667788);
        CHECK(mc_screen_lock(o->base.classes, &e));
        const bool selected = mc_screen_create_world_seed_locked(o, &chosen, &e);
        CHECK(mtx_unlock(&o->base.classes->mutex) == thrd_success && selected);
        const int64_t expected = i == count           ? 12
                                 : i == count + 1     ? 1772899
                                 : cases[i].mode == 0 ? random
                                                      : cases[i].result;
        mc_client_create_world_state state;
        CHECK(mc_client_screen_create_world_inspect(o, &state, &e) == MC_CLIENT_SCREEN_OK &&
              state.random_constructed && state.random_drawn && state.default_seed == random &&
              state.selected_seed == expected && chosen == expected && o->fields.m == guard &&
              state.seed_hashed == (i == count + 1 || (i < count && cases[i].mode == 2)) &&
              state.seed_parsed == (i == count || (i > 0 && i < count && cases[i].mode != 2)));
    }
    CHECK(mc_screen_lock(o->base.classes, &e));
    mc_client_source_ro *field = o->fields.j;
    o->fields.j = NULL; /* Source null reached only AFTER actual Random nextLong. */
    int64_t chosen = 123;
    const bool success = mc_screen_create_world_seed_locked(o, &chosen, &e);
    o->fields.j = field;
    CHECK(mtx_unlock(&o->base.classes->mutex) == thrd_success);
    CHECK(!success && e.status == MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE && e.site.pc == 73 &&
          chosen == 123 && o->fields.m == guard);
    CHECK(mc_java_random_profile_constructor_inputs_complete(o->base.classes->profile) ==
          MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_screen_lock(o->base.classes, &e));
    const bool exhausted = mc_screen_create_world_seed_locked(o, &chosen, &e);
    CHECK(mtx_unlock(&o->base.classes->mutex) == thrd_success && !exhausted &&
          e.status == MC_CLIENT_SCREEN_RANDOM_FAILED && e.site.pc == 62 && chosen == 123);
    return 0;
}

int main(int argc, char **argv) {
    if (argc != 2)
        return 2;
    mc_java_random_profile *profile = NULL;
    mc_resource_files *resources = NULL;
    mc_client_screen_classes *classes = NULL;
    mc_client_source_da *parent = NULL;
    mc_client_source_fj *o = NULL;
    mc_client_screen_error e = {0};
    CHECK(mc_java_random_profile_create(SIZE_MAX, &profile) == MC_JAVA_RANDOM_PROFILE_OK);
    mc_java_random_constructor_input tape[14];
    uint64_t uniquifier = UINT64_C(8682522807148012);
    for (size_t i = 0; i < sizeof tape / sizeof tape[0]; ++i) {
        uniquifier *= UINT64_C(1181783497276652981);
        tape[i] = (mc_java_random_constructor_input){uniquifier, -(int64_t)(101 + i)};
    }
    CHECK(mc_java_random_profile_bind_constructor_inputs(profile, tape, 14) ==
          MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_resource_files_create(argv[1], &resources) == MC_REGISTRY_OK);
    CHECK(mc_client_screen_classes_create(profile, mc_resource_files_source(resources), NULL,
                                          &classes, &e) == MC_CLIENT_SCREEN_OK &&
          mc_client_screen_allocate(classes, NULL, &parent, &e) == MC_CLIENT_SCREEN_OK &&
          mc_client_screen_construct(parent, &e) == MC_CLIENT_SCREEN_OK &&
          mc_client_screen_create_world_allocate(classes, NULL, &o, &e) == MC_CLIENT_SCREEN_OK &&
          mc_client_screen_create_world_construct(o, parent, &e) == MC_CLIENT_SCREEN_OK);
    CHECK(o->fields.a == parent && o->fields.i == NULL && o->fields.j == NULL && !o->fields.m &&
          o->fields.l.kind == MC_CLIENT_SCREEN_TEXT_NULL && o->last_pc == 9);
    CHECK(mc_client_screen_retire(&parent, &e) == MC_CLIENT_SCREEN_BUSY && parent == o->fields.a);
    CHECK(mc_client_screen_create_world_initialize(o, &e) ==
              MC_CLIENT_SCREEN_UNIMPLEMENTED_DEPENDENCY &&
          e.site.pc == 0);
    /* Retire that actual failed initialization receipt before installing the
     * independently declared component graph. No successful b return is claimed. */
    CHECK(mc_client_screen_create_world_retire(&o, &e) == MC_CLIENT_SCREEN_OK && o == NULL &&
          mc_client_screen_create_world_allocate(classes, NULL, &o, &e) == MC_CLIENT_SCREEN_OK &&
          mc_client_screen_create_world_construct(o, parent, &e) == MC_CLIENT_SCREEN_OK);
    client_application *app = calloc(1, sizeof(*app));
    CHECK(app != NULL);
    o->base.fields.b = app; /* Declared genuine App input for the name component. */
    o->base.fields.c = 427;
    o->base.fields.d = 240;
    mc_player_string *empty = NULL;
    CHECK(mc_player_string_create(NULL, 0, &empty) == MC_PLAYER_OK);
    mc_client_font_mapping *mapping = NULL;
    CHECK(mc_client_font_mapping_create(mc_resource_files_source(resources), empty, SIZE_MAX,
                                        SIZE_MAX, &mapping) == MC_CLIENT_FONT_MAPPING_OK);
    o->mapping = mapping; /* Actual FP namespace capability, no static state assignment. */
    uint16_t path_units[4096];
    char path[4096];
    const int length = snprintf(path, sizeof path, "%s/saves", argv[1]);
    CHECK(length > 0 && (size_t)length < sizeof path && (size_t)length < 4096);
    for (int i = 0; i < length; ++i) {
        CHECK((uint8_t)path[i] < 128);
        path_units[i] = (uint8_t)path[i];
    }
    mc_world_info_name *root = NULL;
    mc_java_files *files = NULL;
    mc_java_file *saves = NULL;
    mc_math_classes *math = NULL;
    mc_client_save_formats *formats = NULL;
    mc_client_save_format *format = NULL;
    mc_client_save_error se;
    CHECK(mc_world_info_name_copy((mc_nbt_string){path_units, (size_t)length}, SIZE_MAX, &root) ==
              MC_WORLD_INFO_OK &&
          mc_java_files_create(NULL, &files, NULL) == MC_JAVA_FILE_OK &&
          mc_java_file_allocate(files, &saves, NULL) == MC_JAVA_FILE_OK &&
          mc_java_file_construct_string(saves, root, NULL) == MC_JAVA_FILE_OK &&
          mc_math_classes_create(SIZE_MAX, &math, NULL) == MC_MATH_CLASS_OK &&
          mc_client_save_formats_create(files, math, NULL, &formats, &se) == MC_CLIENT_SAVE_OK &&
          mc_client_save_format_allocate(formats, &format, &se) == MC_CLIENT_SAVE_OK &&
          mc_client_save_format_construct(format, saves, &se) == MC_CLIENT_SAVE_OK);
    app->aa = format;
    CHECK(mc_screen_lock(classes, &e));
    CHECK(model(o) == 0 && naming(o) == 0 && keys(o) == 0);
    CHECK(mtx_unlock(&classes->mutex) == thrd_success && seeds(o, tape, 14) == 0);
    CHECK(mc_client_screen_create_world_retire(&o, &e) == MC_CLIENT_SCREEN_OK && o == NULL &&
          mc_client_screen_retire(&parent, &e) == MC_CLIENT_SCREEN_OK && parent == NULL);
    CHECK(mc_client_save_format_release(&format, &se) == MC_CLIENT_SAVE_OK &&
          mc_client_save_formats_release(&formats, &se) == MC_CLIENT_SAVE_OK &&
          mc_math_classes_release(&math, NULL) == MC_MATH_CLASS_OK &&
          mc_java_file_release(&saves, NULL) == MC_JAVA_FILE_OK &&
          mc_java_files_release(&files, NULL) == MC_JAVA_FILE_OK);
    mc_world_info_name_release(root);
    CHECK(mc_client_font_mapping_release(mapping) == MC_CLIENT_FONT_MAPPING_OK);
    mc_player_string_release(empty);
    CHECK(mc_client_screen_classes_release(&classes, &e) == MC_CLIENT_SCREEN_OK);
    CHECK(mc_resource_files_free(resources) == MC_REGISTRY_OK);
    mc_java_random_profile_free(profile);
    free(app);
    printf("PASS: %u Create World source-name/seed/model checks\n", checks);
    return 0;
}
