/* Authored source-callee controls; no whole startup/menu draw/original claim. */
#include "client/application_access.h"
#include "client/application_state.h"
#include "client/gui/loading_screen.h"
#include "client/gui/source_screen_initialize_internal.h"
#include "mc/resources/files/files.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif
static unsigned int checks;
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(x)) {                                                                                \
            fprintf(stderr, "screen init check%u line%d: %s\n", checks, __LINE__, #x);             \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

static int cpu_prefixes(void) {
    mc_java_random_profile *profile = NULL;
    mc_client_screen_classes *classes = NULL;
    mc_client_source_da *screen = NULL;
    mc_client_screen_error e = {0};
    mc_client_screen_init_error *error = calloc(1, sizeof(*error));
    client_application *app = calloc(1, sizeof(*app));
    CHECK(error != NULL && app != NULL);
    CHECK(mc_java_random_profile_create(SIZE_MAX, &profile) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_client_screen_classes_create(profile, NULL, NULL, &classes, &e) ==
          MC_CLIENT_SCREEN_OK);
    CHECK(mc_client_screen_allocate(classes, NULL, &screen, &e) == MC_CLIENT_SCREEN_OK);
    CHECK(mc_client_screen_construct(screen, &e) == MC_CLIENT_SCREEN_OK);
    CHECK(mc_client_screen_initialize(screen, NULL, 17, 19, error) ==
              MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE &&
          !error->screen.native_policy && error->screen.site.pc == 19 &&
          !error->screen.source_returned);
    mc_client_screen_state state;
    mc_client_screen_init_state first;
    CHECK(mc_client_screen_inspect(screen, &state, &e) == MC_CLIENT_SCREEN_OK &&
          state.da.h != NULL && state.da.b == NULL && state.da.g == NULL && state.da.c == 0 &&
          state.da.d == 0);
    CHECK(mc_client_screen_initialization_inspect(screen, 0, &first) &&
          first.phase == MC_CLIENT_SCREEN_INIT_FAILED && first.app_held && !first.app_counted &&
          first.held_particles == state.da.h && first.store_count == 2 && first.stores[0].pc == 9 &&
          first.stores[1].pc == 14);
    mc_client_screen_du_state du;
    CHECK(mc_client_screen_particles_inspect(first.held_particles, &du, &e) ==
              MC_CLIENT_SCREEN_OK &&
          du.phase == MC_CLIENT_SCREEN_READY && du.b == NULL && du.last_pc == 20);
    CHECK(mc_client_application_lifetime_register(app, NULL) == MC_CLIENT_APPLICATION_OWNER_OK);
    CHECK(mc_client_screen_initialize(screen, app, INT32_MIN, INT32_MAX, error) ==
              MC_CLIENT_SCREEN_OK &&
          error->screen.source_returned);
    mc_client_application_volatile_state app_state;
    CHECK(mc_client_application_volatile_inspect(app, &app_state) &&
          app_state.native_screen_borrows == 2 && app_state.native_worker_holds == 0 &&
          app_state.native_effect_borrows == 0 && app_state.native_resource_thread_borrows == 0);
    CHECK(mc_client_screen_inspect(screen, &state, &e) == MC_CLIENT_SCREEN_OK &&
          state.da.b == app && state.da.c == INT32_MIN && state.da.d == INT32_MAX &&
          state.da.g == NULL);
    mc_client_screen_list_state list;
    CHECK(mc_client_screen_list_inspect(state.da.e, &list) && list.size == 0 &&
          list.mod_count == 1);
    CHECK(mc_client_screen_initialize(screen, app, 0, 0, error) == MC_CLIENT_SCREEN_OK);
    CHECK(mc_client_screen_list_inspect(state.da.e, &list) && list.size == 0 &&
          list.mod_count == 2); /* empty clear still changes modCount */
    CHECK(mc_client_application_lifetime_retire(app, NULL) == MC_CLIENT_APPLICATION_OWNER_BUSY);
    mc_client_source_da *external = screen;
    CHECK(mc_client_screen_retain(external, &e) == MC_CLIENT_SCREEN_OK);
    CHECK(mc_client_screen_retire(&screen, &e) == MC_CLIENT_SCREEN_BUSY && screen == external);
    CHECK(mc_client_screen_release(&external, &e) == MC_CLIENT_SCREEN_OK && external == NULL);
    /* Native bookkeeping fault only: the real release API must reject underflow
     * without consuming its occurrence. Restore exact owned count for retry. */
    const size_t owned_count = atomic_load_explicit(&app->screen_borrows, memory_order_seq_cst);
    atomic_store_explicit(&app->screen_borrows, 0, memory_order_seq_cst);
    const mc_client_screen_status rejected_release = mc_client_screen_retire(&screen, &e);
    atomic_store_explicit(&app->screen_borrows, owned_count, memory_order_seq_cst);
    CHECK(rejected_release == MC_CLIENT_SCREEN_APP_FAILED && screen != NULL &&
          e.initialization != NULL);
    CHECK(mc_client_screen_initialization_inspect(screen, 1, &first) && first.da_pc == 48 &&
          first.failure->screen.source_returned && first.app_held && first.app_counted);
    CHECK(mc_client_screen_retire(&screen, &e) == MC_CLIENT_SCREEN_OK && screen == NULL);
    CHECK(mc_client_application_volatile_inspect(app, &app_state) &&
          app_state.native_screen_borrows == 0);
    CHECK(mc_client_application_lifetime_retire(app, NULL) == MC_CLIENT_APPLICATION_OWNER_OK);
    CHECK(mc_client_screen_classes_release(&classes, &e) == MC_CLIENT_SCREEN_OK);
    mc_java_random_profile_free(profile);
    free(app);
    free(error);
    return 0;
}

static int directory(const char *path) {
#ifdef _WIN32
    const int made = _mkdir(path);
#else
    const int made = mkdir(path, 0700);
#endif
    CHECK(made == 0 || errno == EEXIST);
    return 0;
}

static int write_input(const char *root, const char *name, const char *text) {
    char path[4096];
    const int n = snprintf(path, sizeof(path), "%s/%s", root, name);
    CHECK(n > 0 && (size_t)n < sizeof(path));
    FILE *f = NULL;
#ifdef _WIN32
    CHECK(fopen_s(&f, path, "wb") == 0 && f != NULL);
#else
    f = fopen(path, "wb");
    CHECK(f != NULL);
#endif
    const size_t size = strlen(text), written = fwrite(text, 1, size, f);
    const int closed = fclose(f);
    CHECK(written == size && closed == 0);
    return 0;
}

typedef struct forwarding {
    const mc_registry_resources *real;
    mc_registry_resources source;
    client_application *mutate;
    mc_client_source_da **slot;
    bool saw_busy;
    unsigned int lang_opens;
} forwarding;

static mc_registry_status forward_open(void *context, const char *name,
                                       mc_java_properties_input *out) {
    forwarding *f = context;
    const mc_registry_status status = f->real->open(f->real->context, name, out);
    if (strcmp(name, "/lang/en_US.lang") == 0) {
        ++f->lang_opens;
        if (f->mutate != NULL) {
            f->mutate->n = false;
            f->mutate->font_renderer = NULL;
        } /* explicit live-operand mutation witness */
        if (f->slot != NULL) {
            mc_client_source_da *old = *f->slot;
            mc_client_screen_error e = {0};
            f->saw_busy =
                mc_client_screen_retire(f->slot, &e) == MC_CLIENT_SCREEN_BUSY && *f->slot == old;
        }
    }
    return status;
}

static mc_registry_status forward_close(void *context, const mc_java_properties_input *in) {
    forwarding *f = context;
    return f->real->close(f->real->context, in);
}

static bool forward_ready(void *context, const mc_java_properties_input *in) {
    forwarding *f = context;
    return f->real->ready != NULL && f->real->ready(f->real->context, in);
}

static int resource_initialization(const char *root) {
    char path[4096];
    int n = snprintf(path, sizeof(path), "%s/title", root);
    CHECK(n > 0 && (size_t)n < sizeof(path) && directory(path) == 0);
    n = snprintf(path, sizeof(path), "%s/lang", root);
    CHECK(n > 0 && (size_t)n < sizeof(path) && directory(path) == 0);
    CHECK(write_input(root, "title/splashes.txt", "authored splash\n") == 0);
    CHECK(write_input(root, "lang/en_US.lang", "menu.singleplayer=Solo\nmenu.mods=Textures\n") ==
          0);
    CHECK(write_input(root, "lang/stats_US.lang", "# authored empty statistics labels\n") == 0);
    mc_java_random_profile *profile = NULL;
    mc_materials *materials = NULL;
    mc_resource_files *files = NULL;
    mc_registry_classes *registry = NULL;
    mc_client_screen_classes *classes = NULL;
    mc_client_source_fu *menu = NULL, *other = NULL;
    mc_client_render_engine_classes *render = NULL;
    mc_client_loading_classes *gui = NULL;
    mc_client_bitmap_font *font_one = NULL, *font_two = NULL;
    client_application *app_one = calloc(1, sizeof(*app_one)),
                       *app_two = calloc(1, sizeof(*app_two));
    mc_client_screen_init_error *error = calloc(1, sizeof(*error));
    mc_client_screen_error e = {0};
    CHECK(app_one != NULL && app_two != NULL && error != NULL);
    CHECK(mc_java_random_profile_create(SIZE_MAX, &profile) == MC_JAVA_RANDOM_PROFILE_OK &&
          mc_materials_create(&materials));
    CHECK(mc_resource_files_create(root, &files) == MC_REGISTRY_OK);
    forwarding f = {0};
    f.real = mc_resource_files_source(files);
    f.source = (mc_registry_resources){&f, forward_open, forward_close, forward_ready};
    const mc_registry_options options = {.side = MC_REGISTRY_CLIENT,
                                         .random_profile = profile,
                                         .materials = materials,
                                         .resources = &f.source};
    CHECK(mc_registry_classes_allocate(&options, &registry, NULL) == MC_REGISTRY_OK);
    CHECK(mc_client_screen_classes_create(profile, &f.source, registry, &classes, &e) ==
          MC_CLIENT_SCREEN_OK);
    CHECK(mc_client_render_engine_classes_create(NULL, &render) == MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(mc_client_loading_classes_create(render, &gui) == MC_CLIENT_LOADING_OK);
    CHECK(mc_client_bitmap_font_allocate(gui, NULL, &font_one) == MC_CLIENT_BITMAP_FONT_OK &&
          mc_client_bitmap_font_allocate(gui, NULL, &font_two) ==
              MC_CLIENT_BITMAP_FONT_OK); /* actual RAW owners, no ready/draw claim */
    CHECK(mc_client_application_lifetime_register(app_one, NULL) ==
              MC_CLIENT_APPLICATION_OWNER_OK &&
          mc_client_application_lifetime_register(app_two, NULL) == MC_CLIENT_APPLICATION_OWNER_OK);
    app_one->font_renderer = font_one;
    app_one->n = true;
    app_two->font_renderer = font_two;
    app_two->n = true;
    CHECK(mc_client_screen_menu_allocate(classes, NULL, &menu, &e) == MC_CLIENT_SCREEN_OK &&
          mc_client_screen_menu_construct(menu, &e) == MC_CLIENT_SCREEN_OK);
    mc_client_source_da *screen = mc_client_screen_menu_base(menu);
    f.mutate = app_one;
    f.slot = &screen;
    CHECK(mc_client_screen_initialize(screen, app_one, 320, 240, error) == MC_CLIENT_SCREEN_OK &&
          error->screen.source_returned);
    CHECK(f.lang_opens == 1 && f.saw_busy && app_one->font_renderer == NULL && !app_one->n);
    f.mutate = NULL;
    f.slot = NULL;
    mc_client_screen_state state;
    mc_client_screen_init_state first, second;
    mc_client_screen_list_state list;
    CHECK(mc_client_screen_inspect(screen, &state, &e) == MC_CLIENT_SCREEN_OK &&
          state.da.g == font_one && state.da.b == app_one);
    CHECK(mc_client_screen_initialization_inspect(screen, 0, &first) &&
          first.phase == MC_CLIENT_SCREEN_INIT_RETURNED && first.da_pc == 48 &&
          first.fu_pc == 422 && first.button_count == 5 && !first.applet_branch && first.app_held &&
          first.app_counted);
    CHECK(mc_client_screen_list_inspect(state.da.e, &list) && list.size == 5 &&
          list.mod_count == 6);
    const mc_client_source_ke *button = NULL;
    mc_client_screen_ke_state button_state;
    CHECK(mc_client_screen_list_button(state.da.e, 0, &button) &&
          mc_client_screen_button_inspect((mc_client_source_ke *)button, &button_state, &e) ==
              MC_CLIENT_SCREEN_OK &&
          button_state.fields.f == 1 && button_state.fields.c == 60 &&
          button_state.fields.d == 108 && button_state.fields.a == 200 &&
          button_state.fields.e.kind == MC_CLIENT_SCREEN_TEXT_REGISTRY);
    mc_player_string_view label;
    CHECK(mc_client_screen_text_read(button_state.fields.e, &label) && label.length == 4 &&
          label.units[0] == 'S');
    CHECK(mc_client_screen_list_button(state.da.e, 1, &button) &&
          mc_client_screen_button_inspect((mc_client_source_ke *)button, &button_state, &e) ==
              MC_CLIENT_SCREEN_OK &&
          button_state.fields.f == 2 && !button_state.fields.g && state.fu.l == button);
    const mc_player_string *fallback = button_state.fields.e.player;
    CHECK(button_state.fields.e.kind == MC_CLIENT_SCREEN_TEXT_PLAYER && fallback != NULL);
    mc_client_source_du *old_du = first.held_particles;
    CHECK(mc_client_screen_particles_retain(old_du, &e) == MC_CLIENT_SCREEN_OK);
    const mc_client_screen_list *same_list = state.da.e;
    CHECK(mc_client_screen_initialize(screen, app_two, 640, 480, error) == MC_CLIENT_SCREEN_OK);
    CHECK(mc_client_screen_inspect(screen, &state, &e) == MC_CLIENT_SCREEN_OK &&
          state.da.e == same_list && state.da.g == font_two && state.da.b == app_two &&
          state.da.h != old_du);
    CHECK(mc_client_screen_initialization_inspect(screen, 1, &second) && second.applet_branch &&
          second.button_count == 4 && second.held_font == font_two &&
          second.held_particles != old_du && second.replaced_l == first.buttons[1]);
    CHECK(mc_client_screen_list_inspect(same_list, &list) && list.size == 4 &&
          list.mod_count == 11 && same_list->entries[4] == NULL);
    CHECK(mc_client_screen_list_button(same_list, 1, &button) &&
          mc_client_screen_button_inspect((mc_client_source_ke *)button, &button_state, &e) ==
              MC_CLIENT_SCREEN_OK &&
          button_state.fields.e.player == fallback);
    CHECK(mc_client_screen_menu_initialize(menu, error) ==
          MC_CLIENT_SCREEN_OK); /* actual b adds without da.clear */
    CHECK(mc_client_screen_list_inspect(same_list, &list) && list.size == 8 &&
          list.mod_count == 15);
    CHECK(mc_client_screen_menu_allocate(classes, NULL, &other, &e) == MC_CLIENT_SCREEN_OK &&
          mc_client_screen_menu_construct(other, &e) == MC_CLIENT_SCREEN_OK);
    CHECK(mc_client_screen_menu_initialize(other, error) ==
              MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE &&
          !error->screen.native_policy && error->screen.site.pc == 268 &&
          !error->screen.source_returned);
    CHECK(mc_client_screen_inspect(mc_client_screen_menu_base(other), &state, &e) ==
              MC_CLIENT_SCREEN_OK &&
          mc_client_screen_list_inspect(state.da.e, &list) && list.size == 3 &&
          list.mod_count == 3 && state.fu.l != NULL);
    CHECK(mc_client_screen_menu_retire(&other, &e) == MC_CLIENT_SCREEN_OK);
    mc_client_application_volatile_state a, b;
    CHECK(mc_client_application_volatile_inspect(app_one, &a) &&
          mc_client_application_volatile_inspect(app_two, &b) && a.native_screen_borrows == 2 &&
          b.native_screen_borrows == 2);
    CHECK(app_one->r == NULL && app_two->r == NULL &&
          mc_client_application_lifetime_retire(app_one, NULL) == MC_CLIENT_APPLICATION_OWNER_BUSY);
    CHECK(mc_client_screen_menu_retire(&menu, &e) == MC_CLIENT_SCREEN_OK && menu == NULL);
    CHECK(mc_client_application_volatile_inspect(app_one, &a) &&
          mc_client_application_volatile_inspect(app_two, &b) && a.native_screen_borrows == 1 &&
          b.native_screen_borrows == 0);
    CHECK(mc_client_application_lifetime_retire(app_one, NULL) == MC_CLIENT_APPLICATION_OWNER_BUSY);
    CHECK(mc_client_screen_particles_retire(&old_du, &e) == MC_CLIENT_SCREEN_OK && old_du == NULL);
    CHECK(mc_client_application_lifetime_retire(app_one, NULL) == MC_CLIENT_APPLICATION_OWNER_OK &&
          mc_client_application_lifetime_retire(app_two, NULL) == MC_CLIENT_APPLICATION_OWNER_OK);
    CHECK(mc_client_screen_classes_release(&classes, &e) == MC_CLIENT_SCREEN_OK);
    CHECK(mc_client_bitmap_font_release(&font_one, NULL) == MC_CLIENT_BITMAP_FONT_OK &&
          mc_client_bitmap_font_release(&font_two, NULL) == MC_CLIENT_BITMAP_FONT_OK);
    CHECK(mc_client_loading_classes_release(gui) == MC_CLIENT_LOADING_OK &&
          mc_client_render_engine_classes_release(render) == MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(mc_registry_classes_free(registry, NULL) == MC_REGISTRY_OK &&
          mc_resource_files_free(files) == MC_REGISTRY_OK);
    mc_materials_destroy(materials);
    mc_java_random_profile_free(profile);
    free(app_one);
    free(app_two);
    free(error);
    return 0;
}

int main(int argc, char **argv) {
    if (argc == 1) {
        if (cpu_prefixes() != 0)
            return 1;
    } else if (argc == 3 && strcmp(argv[1], "resource") == 0) {
        if (resource_initialization(argv[2]) != 0)
            return 1;
    } else
        return 2;
    printf("Source screen initialization passed (%u checks)\n", checks);
    return 0;
}
