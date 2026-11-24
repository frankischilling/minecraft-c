/* Authored canonical RQ/SaveFormat controls, separate from gameplay and pixels. */
#include "client/application_state.h"
#include "client/gui/source_screen_initialize.h"
#include "client/gui/source_world_list.h"
#include "client/gui/source_world_selection_internal.h"
#include "mc/resources/files/files.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned int checks;
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(x)) {                                                                                \
            fprintf(stderr, "world selection check%u line%d: %s\n", checks, __LINE__, #x);         \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

static bool text_is(mc_client_screen_text text, const char *ascii) {
    mc_player_string_view view;
    if (!mc_client_screen_text_read(text, &view) || view.length != strlen(ascii))
        return false;
    for (size_t i = 0; i < view.length; ++i)
        if (view.units[i] != (uint8_t)ascii[i])
            return false;
    return true;
}

static bool name_is(mc_world_info_name *name, const char *ascii) {
    if (name == NULL)
        return false;
    mc_nbt_string view = mc_world_info_name_text(name);
    if (view.length != strlen(ascii))
        return false;
    for (size_t i = 0; i < view.length; ++i)
        if (view.units[i] != (uint8_t)ascii[i])
            return false;
    return true;
}

static int construction_prefixes(mc_client_screen_classes *classes, mc_client_source_da *parent) {
    mc_client_screen_error e = {0};
    mc_client_source_rq *rq = NULL;
    mc_client_screen_options budget = {1024u * 1024u, 0, 1024u * 1024u, 4096u, 512u};
    CHECK(mc_client_screen_world_selection_allocate(classes, &budget, &rq, &e) ==
          MC_CLIENT_SCREEN_OK);
    CHECK(mc_client_screen_world_selection_construct(rq, parent, &e) == MC_CLIENT_SCREEN_LIMIT &&
          strcmp(e.site.class_name, "da") == 0 && e.site.pc == 5 && e.parent_site.pc == 1);
    CHECK(rq->base.phase == MC_CLIENT_SCREEN_FAILED && rq->fields.j == NULL &&
          rq->fields.a == NULL && rq->store_count == 0 && rq->fields.m == 0);
    CHECK(mc_client_screen_world_selection_retire(&rq, &e) == MC_CLIENT_SCREEN_OK && rq == NULL);
    budget.maximum_allocations = 1;
    CHECK(mc_client_screen_world_selection_allocate(classes, &budget, &rq, &e) ==
          MC_CLIENT_SCREEN_OK);
    CHECK(mc_client_screen_world_selection_construct(rq, parent, &e) == MC_CLIENT_SCREEN_OK);
    CHECK(mc_client_screen_world_selection_initialize(rq, &e) == MC_CLIENT_SCREEN_LIMIT &&
          strcmp(e.site.class_name, "rq") == 0 && e.site.pc == 0 && e.native_policy);
    CHECK(rq->fields.j != NULL && rq->fields.a == parent && rq->fields.m == 0 &&
          rq->fields.n == NULL && rq->store_count == 4);
    CHECK(mc_client_screen_world_selection_retire(&rq, &e) == MC_CLIENT_SCREEN_OK && rq == NULL);
    return 0;
}

static int fixture(const char *root_path, bool populated) {
    mc_java_random_profile *profile = NULL;
    mc_materials *materials = NULL;
    mc_resource_files *resources = NULL;
    mc_registry_classes *registry = NULL;
    mc_client_screen_classes *classes = NULL;
    mc_client_source_da *parent = NULL;
    mc_client_source_rq *rq = NULL;
    mc_client_screen_error e = {0};
    mc_client_screen_init_error *init = calloc(1, sizeof(*init));
    client_application *app = calloc(1, sizeof(*app));
    CHECK(init != NULL && app != NULL);
    CHECK(mc_java_random_profile_create(SIZE_MAX, &profile) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_materials_create(&materials));
    CHECK(mc_resource_files_create(root_path, &resources) == MC_REGISTRY_OK);
    const mc_registry_options options = {.side = MC_REGISTRY_CLIENT,
                                         .random_profile = profile,
                                         .materials = materials,
                                         .resources = mc_resource_files_source(resources)};
    CHECK(mc_registry_classes_allocate(&options, &registry, NULL) == MC_REGISTRY_OK);
    CHECK(mc_client_screen_classes_create(profile, mc_resource_files_source(resources), registry,
                                          &classes, &e) == MC_CLIENT_SCREEN_OK);
    CHECK(mc_client_screen_allocate(classes, NULL, &parent, &e) == MC_CLIENT_SCREEN_OK);
    CHECK(mc_client_screen_construct(parent, &e) == MC_CLIENT_SCREEN_OK);
    CHECK(mc_client_screen_world_selection_allocate(classes, NULL, &rq, &e) == MC_CLIENT_SCREEN_OK);
    CHECK(rq->fields.m == 0 && rq->fields.j == NULL && rq->fields.a == NULL);
    CHECK(mc_client_screen_world_selection_construct(rq, parent, &e) == MC_CLIENT_SCREEN_OK &&
          e.source_returned && e.site.pc == 31);
    CHECK(rq->fields.a == parent && rq->fields.j != NULL && !rq->fields.l && rq->fields.m == 0 &&
          rq->fields.n == NULL && text_is(rq->fields.i, "Select world"));
    CHECK(rq->store_count == 4 && rq->stores[0].pc == 12 && rq->stores[1].pc == 18 &&
          rq->stores[2].pc == 23 && rq->stores[3].pc == 28);
    mc_client_screen_class_state class_state;
    CHECK(mc_client_screen_classes_inspect(classes, &class_state, &e) == MC_CLIENT_SCREEN_OK &&
          class_state.phase == MC_CLIENT_SCREEN_CLASS_COLD && class_state.a == NULL);
    CHECK(mc_client_screen_retire(&parent, &e) == MC_CLIENT_SCREEN_BUSY && parent == rq->fields.a);
    CHECK(construction_prefixes(classes, parent) == 0);
    CHECK(mc_client_screen_initialize(&rq->base, NULL, 427, 240, init) ==
              MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE &&
          init->screen.site.pc == 19 && rq->fields.m == 0);
    CHECK(mc_client_application_lifetime_register(app, NULL) == MC_CLIENT_APPLICATION_OWNER_OK);
    CHECK(mc_client_screen_initialize(&rq->base, app, 427, 240, init) ==
              MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE &&
          !init->screen.native_policy && init->screen.site.pc == 10 &&
          strcmp(init->screen.site.class_name, "rq") == 0);
    CHECK(text_is(rq->fields.i, "Choose save") && text_is(rq->fields.p, "Realm") &&
          text_is(rq->fields.q, "Convert") && rq->fields.n == NULL && rq->fields.m == 0 &&
          rq->fields.o == NULL);

    char saves_path[4096];
    int n = snprintf(saves_path, sizeof(saves_path), "%s/saves", root_path);
    CHECK(n > 0 && (size_t)n < sizeof(saves_path));
    uint16_t units[4096];
    const size_t path_length = strlen(saves_path);
    CHECK(path_length < sizeof(units) / sizeof(units[0]));
    for (size_t i = 0; i < path_length; ++i) {
        CHECK((uint8_t)saves_path[i] < 128u);
        units[i] = (uint8_t)saves_path[i];
    }
    mc_world_info_name *path = NULL;
    CHECK(mc_world_info_name_copy((mc_nbt_string){units, path_length}, SIZE_MAX, &path) ==
          MC_WORLD_INFO_OK);
    mc_java_files *files = NULL;
    mc_java_file *saves = NULL;
    mc_java_file_error fe;
    CHECK(mc_java_files_create(NULL, &files, &fe) == MC_JAVA_FILE_OK);
    CHECK(mc_java_file_allocate(files, &saves, &fe) == MC_JAVA_FILE_OK);
    CHECK(mc_java_file_construct_string(saves, path, &fe) == MC_JAVA_FILE_OK);
    mc_math_classes *math = NULL;
    mc_math_class_error me;
    CHECK(mc_math_classes_create(SIZE_MAX, &math, &me) == MC_MATH_CLASS_OK);
    mc_client_save_formats *formats = NULL;
    mc_client_save_format *format = NULL;
    mc_client_save_error se;
    CHECK(mc_client_save_formats_create(files, math, NULL, &formats, &se) == MC_CLIENT_SAVE_OK);
    CHECK(mc_client_save_format_allocate(formats, &format, &se) == MC_CLIENT_SAVE_OK);
    CHECK(mc_client_save_format_construct(format, saves, &se) == MC_CLIENT_SAVE_OK);
    app->aa = format; /* Actual canonical App.aa, with no alternate directory/list service. */
    CHECK(mc_client_screen_initialize(&rq->base, app, 427, 240, init) == MC_CLIENT_SCREEN_OK &&
          init->screen.source_returned);
    CHECK(rq->fields.m == -1 && rq->fields.n != NULL && rq->fields.o != NULL &&
          rq->last_initialization->list == rq->fields.n &&
          mc_client_save_list_size(rq->fields.n) == (populated ? 3u : 0u));
    CHECK(rq->fields.o->a == rq && rq->fields.o->fields.a == app && rq->fields.o->fields.b == 427 &&
          rq->fields.o->fields.c == 240 && rq->fields.o->fields.h == 32 &&
          rq->fields.o->fields.i == 176 && rq->fields.o->fields.j == 36 &&
          rq->fields.o->fields.k == -2.0f);
    mc_client_save_format_state fs;
    CHECK(mc_client_save_format_read_state(format, &fs, &se) == MC_CLIENT_SAVE_OK &&
          fs.source_a == saves && fs.pending_list == NULL);
    if (populated) {
        const char *const folders[] = {"alpha", "bravo", "older"};
        for (size_t i = 0; i < 3; ++i) {
            mc_client_save_selection_fields row;
            CHECK(mc_client_save_selection_read(mc_client_save_list_at(rq->fields.n, i), &row,
                                                &se) == MC_CLIENT_SAVE_OK &&
                  name_is(row.filename, folders[i]) && row.last_played == (i == 2 ? 1000 : 2000));
            CHECK(row.requires_conversion == (i == 0));
        }
    }
    mc_client_screen_list_state buttons;
    CHECK(mc_client_screen_list_inspect(rq->base.fields.e, &buttons) && buttons.size == 5);
    const int32_t ids[] = {1, 6, 2, 3, 0}, xs[] = {59, 59, 139, 217, 217},
                  ys[] = {188, 212, 212, 188, 212}, widths[] = {150, 70, 70, 150, 150};
    const char *const labels[] = {"Load", "Rename", "Delete", "New", "Back"};
    for (size_t i = 0; i < 5; ++i) {
        const mc_client_source_ke *button = NULL;
        mc_client_screen_ke_state state;
        CHECK(mc_client_screen_list_button(rq->base.fields.e, i, &button));
        CHECK(mc_client_screen_button_inspect((mc_client_source_ke *)button, &state, &e) ==
                  MC_CLIENT_SCREEN_OK &&
              state.fields.f == ids[i] && state.fields.a == widths[i] && state.fields.b == 20 &&
              state.fields.c == xs[i] && state.fields.d == ys[i] && state.fields.g == (i >= 3) &&
              state.fields.h && text_is(state.fields.e, labels[i]));
    }
    const size_t before_active = rq->initialization_count;
    mc_client_save_list *before_list = rq->fields.n;
    mc_client_source_world_list *before_scroll = rq->fields.o;
    ++rq->base.active_calls; /* Native active-occurrence admission control only. */
    const mc_client_screen_status rejected = mc_client_screen_world_selection_initialize(rq, &e);
    --rq->base.active_calls;
    CHECK(rejected == MC_CLIENT_SCREEN_BUSY && rq->initialization_count == before_active &&
          rq->fields.n == before_list && rq->fields.o == before_scroll && rq->fields.m == -1);
    const mc_client_save_list *old_list = rq->fields.n;
    const mc_client_source_world_list *old_scroll = rq->fields.o;
    const mc_client_screen_list *same_buttons = rq->base.fields.e;
    rq->fields.m = 2; /* Prepared selected-row state; reinitialization must reset after sort. */
    CHECK(mc_client_screen_initialize(&rq->base, app, 427, 240, init) == MC_CLIENT_SCREEN_OK);
    CHECK(rq->fields.m == -1 && rq->fields.n != old_list && rq->fields.o != old_scroll &&
          rq->base.fields.e == same_buttons &&
          mc_client_save_list_size(old_list) == (populated ? 3u : 0u));
    CHECK(mc_client_screen_list_inspect(same_buttons, &buttons) && buttons.size == 5);
    mc_client_world_selection_state inspected;
    CHECK(mc_client_screen_world_selection_inspect(rq, &inspected, &e) == MC_CLIENT_SCREEN_OK &&
          inspected.initialization_count == 3 && inspected.fields.a == parent);
    mc_client_source_da *external = &rq->base;
    CHECK(mc_client_screen_retain(external, &e) == MC_CLIENT_SCREEN_OK);
    CHECK(mc_client_screen_world_selection_retire(&rq, &e) == MC_CLIENT_SCREEN_BUSY && rq != NULL);
    CHECK(mc_client_screen_release(&external, &e) == MC_CLIENT_SCREEN_OK && external == NULL);
    CHECK(mc_client_application_lifetime_retire(app, NULL) == MC_CLIENT_APPLICATION_OWNER_BUSY);
    CHECK(mc_client_screen_world_selection_retire(&rq, &e) == MC_CLIENT_SCREEN_OK && rq == NULL);
    CHECK(mc_client_screen_retire(&parent, &e) == MC_CLIENT_SCREEN_OK && parent == NULL);
    CHECK(mc_client_application_lifetime_retire(app, NULL) == MC_CLIENT_APPLICATION_OWNER_OK);
    CHECK(mc_client_save_format_release(&format, &se) == MC_CLIENT_SAVE_OK);
    CHECK(mc_client_save_formats_release(&formats, &se) == MC_CLIENT_SAVE_OK);
    CHECK(mc_math_classes_release(&math, &me) == MC_MATH_CLASS_OK);
    CHECK(mc_java_file_release(&saves, &fe) == MC_JAVA_FILE_OK);
    CHECK(mc_java_files_release(&files, &fe) == MC_JAVA_FILE_OK);
    mc_world_info_name_release(path);
    CHECK(mc_client_screen_classes_release(&classes, &e) == MC_CLIENT_SCREEN_OK);
    CHECK(mc_registry_classes_free(registry, NULL) == MC_REGISTRY_OK);
    CHECK(mc_resource_files_free(resources) == MC_REGISTRY_OK);
    mc_materials_destroy(materials);
    mc_java_random_profile_free(profile);
    free(init);
    free(app);
    return 0;
}

int main(int argc, char **argv) {
    if (argc != 3 || (strcmp(argv[2], "empty") != 0 && strcmp(argv[2], "populated") != 0))
        return 2;
    if (fixture(argv[1], strcmp(argv[2], "populated") == 0) != 0)
        return 1;
    printf("Canonical world selection passed (%u checks)\n", checks);
    return 0;
}
