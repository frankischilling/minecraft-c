/* Genuine screen/World owners with prepared direct oz.b fields. */
#include "client/application_state.h"
#include "client/gui/source_pause_screen_internal.h"
#include "client/gui/source_screen_initialize.h"
#include "mc/core/math/classes.h"
#include "mc/resources/files/files.h"
#include "world/runtime/runtime_internal.h"

#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned int checks;
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(x)) {                                                                                \
            fprintf(stderr, "Pause screen check%u line%d: %s\n", checks, __LINE__, #x);            \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

static int rows(mc_client_screen_classes *classes, client_application *app,
                mc_world_runtime *world) {
    const int32_t sizes[][2] = {{427, 240}, {320, 200}, {-1, -1}, {INT32_MIN, INT32_MAX}};
    for (unsigned int mode = 0; mode < 3; ++mode) {
        for (size_t n = 0; n < sizeof(sizes) / sizeof(sizes[0]); ++n) {
            mc_client_source_oz *pause = NULL;
            mc_client_screen_error e;
            CHECK(mc_client_screen_pause_allocate(classes, NULL, &pause, &e) ==
                  MC_CLIENT_SCREEN_OK);
            CHECK(mc_client_screen_pause_construct(pause, &e) == MC_CLIENT_SCREEN_OK &&
                  e.source_returned && pause->save_pass == 0 && pause->ticks == 0 &&
                  pause->base.store_count == 6);
            pause->base.fields.b = mode == 0 ? NULL : app;
            pause->base.fields.c = sizes[n][0];
            pause->base.fields.d = sizes[n][1];
            app->active_world = mode == 2 ? world : NULL;
            pause->save_pass = 123;
            pause->ticks = 17 + (int32_t)n;
            const mc_client_screen_status status = mc_client_screen_pause_initialize(pause, &e);
            CHECK(status ==
                  (mode == 0 ? MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE : MC_CLIENT_SCREEN_OK));
            size_t count;
            CHECK(mc_client_screen_list_size(pause->base.fields.e, &count) &&
                  count == (mode == 0 ? 1u : 5u));
            CHECK(pause->save_pass == 0 && pause->ticks == 17 + (int32_t)n);
            printf("PAUSE %u %zu %d %d %d %zu\n", mode, n, pause->save_pass, pause->ticks,
                   mode != 0, count);
            for (size_t index = 0; index < count; ++index) {
                const mc_client_source_ke *button = NULL;
                CHECK(mc_client_screen_list_button(pause->base.fields.e, index, &button) &&
                      button != NULL && button->phase == MC_CLIENT_SCREEN_READY);
                const mc_client_screen_ke_fields *f = &button->fields;
                mc_player_string_view text;
                CHECK(mc_client_screen_text_read(f->e, &text));
                printf("PAUSEBUTTON %u %zu %zu %d %d %d %d %d %d %d ", mode, n, index, f->f, f->c,
                       f->d, f->a, f->b, f->g, f->h);
                for (size_t unit = 0; unit < text.length; ++unit)
                    printf("%04x", (unsigned int)text.units[unit]);
                putchar('\n');
            }
            CHECK(mc_client_screen_pause_tick(pause, &e) == MC_CLIENT_SCREEN_OK &&
                  e.source_returned && e.site.pc == 14 && pause->ticks == 18 + (int32_t)n &&
                  pause->save_pass == 0);
            printf("PAUSETICK %u %zu %d %d\n", mode, n, pause->save_pass, pause->ticks);
            mc_client_screen_list_state list;
            CHECK(mc_client_screen_list_inspect(pause->base.fields.e, &list) &&
                  list.mod_count == (mode == 0 ? 2 : 6));
            CHECK(mc_client_screen_pause_retire(&pause, &e) == MC_CLIENT_SCREEN_OK &&
                  pause == NULL);
        }
    }
    app->active_world = NULL;
    return 0;
}

static int controls(mc_client_screen_classes *classes, client_application *app) {
    mc_client_source_oz *pause = NULL;
    mc_client_screen_error e;
    mc_client_screen_options quota = {1024u * 1024u, 0, 1024u * 1024u, 4096, 512};
    CHECK(mc_client_screen_pause_allocate(classes, &quota, &pause, &e) == MC_CLIENT_SCREEN_OK);
    CHECK(mc_client_screen_pause_tick(pause, &e) == MC_CLIENT_SCREEN_BUSY && e.native_policy &&
          !e.source_returned && pause->ticks == 0 && pause->save_pass == 0);
    CHECK(mc_client_screen_pause_construct(pause, &e) == MC_CLIENT_SCREEN_LIMIT && e.site.pc == 5 &&
          e.parent_site.pc == 1 && pause->base.phase == MC_CLIENT_SCREEN_FAILED &&
          pause->base.fields.e == NULL);
    CHECK(mc_client_screen_pause_retire(&pause, &e) == MC_CLIENT_SCREEN_OK);
    CHECK(mc_client_screen_pause_allocate(classes, NULL, &pause, &e) == MC_CLIENT_SCREEN_OK &&
          mc_client_screen_pause_construct(pause, &e) == MC_CLIENT_SCREEN_OK);
    mc_client_screen_init_error *init = calloc(1, sizeof(*init));
    CHECK(init != NULL);
    CHECK(mc_client_screen_initialize(&pause->base, app, 427, 240, init) == MC_CLIENT_SCREEN_OK);
    mc_client_screen_list_state list;
    CHECK(mc_client_screen_list_inspect(pause->base.fields.e, &list) && list.mod_count == 7 &&
          list.size == 5 && pause->initialization_count == 1);
    const mc_client_source_ke *old = NULL;
    CHECK(mc_client_screen_list_button(pause->base.fields.e, 0, &old));
    mc_client_screen_ke_state old_state;
    CHECK(mc_client_screen_button_inspect((mc_client_source_ke *)old, &old_state, &e) ==
              MC_CLIENT_SCREEN_OK &&
          old_state.holds == 2);
    pause->ticks = INT32_MAX;
    pause->base.active_calls = 1;
    CHECK(mc_client_screen_pause_tick(pause, &e) == MC_CLIENT_SCREEN_BUSY && !e.source_returned &&
          pause->ticks == INT32_MAX);
    pause->base.active_calls = 0;
    CHECK(mc_client_screen_pause_tick(pause, &e) == MC_CLIENT_SCREEN_OK && e.source_returned &&
          e.site.pc == 14 && pause->ticks == INT32_MIN);
    pause->save_pass = 87;
    CHECK(mc_client_screen_initialize(&pause->base, app, 320, 200, init) == MC_CLIENT_SCREEN_OK &&
          pause->save_pass == 0 && pause->ticks == INT32_MIN && pause->initialization_count == 2);
    CHECK(mc_client_screen_list_inspect(pause->base.fields.e, &list) && list.mod_count == 14 &&
          list.size == 5);
    CHECK(mc_client_screen_button_inspect((mc_client_source_ke *)old, &old_state, &e) ==
              MC_CLIENT_SCREEN_OK &&
          old_state.holds == 1 && old_state.fields.c == 113);
    mc_client_source_da *hold = &pause->base;
    CHECK(mc_client_screen_retain(hold, &e) == MC_CLIENT_SCREEN_OK);
    CHECK(mc_client_screen_pause_retire(&pause, &e) == MC_CLIENT_SCREEN_BUSY && pause != NULL);
    CHECK(mc_client_screen_release(&hold, &e) == MC_CLIENT_SCREEN_OK && hold == NULL);
    CHECK(mc_client_screen_pause_retire(&pause, &e) == MC_CLIENT_SCREEN_OK && pause == NULL);
    free(init);
    return 0;
}

int main(int argc, char **argv) {
    if (argc != 2)
        return 2;
    mc_java_random_profile *profile = NULL;
    mc_materials *materials = NULL;
    mc_resource_files *resources = NULL;
    mc_registry_classes *registry = NULL;
    mc_client_screen_classes *classes = NULL;
    mc_math_classes *math = NULL;
    mc_world_runtime_classes *world_classes = NULL;
    mc_world_runtime *world = NULL;
    client_application *app = calloc(1, sizeof(*app));
    CHECK(app != NULL);
    CHECK(mc_java_random_profile_create(SIZE_MAX, &profile) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_materials_create(&materials));
    CHECK(mc_resource_files_create(argv[1], &resources) == MC_REGISTRY_OK);
    const mc_registry_options options = {.side = MC_REGISTRY_CLIENT,
                                         .random_profile = profile,
                                         .materials = materials,
                                         .resources = mc_resource_files_source(resources)};
    CHECK(mc_registry_classes_allocate(&options, &registry, NULL) == MC_REGISTRY_OK);
    CHECK(mc_client_screen_classes_create(profile, mc_resource_files_source(resources), registry,
                                          &classes, NULL) == MC_CLIENT_SCREEN_OK);
    CHECK(mc_math_classes_create(SIZE_MAX, &math, NULL) == MC_MATH_CLASS_OK &&
          mc_math_classes_ensure(math, NULL) == MC_MATH_CLASS_OK);
    CHECK(mc_world_runtime_classes_create(MC_WORLD_INFO_CLIENT, profile,
                                          mc_math_classes_table(math), &world_classes,
                                          NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_allocate_world(MC_WORLD_RUNTIME_CLIENT_LOAD_OR_CREATE, world_classes,
                                          NULL, &world, NULL) == MC_WORLD_STORAGE_OK);
    world->fields.B = true; /* Prepared actual fd field, no constructor equivalence. */
    CHECK(mc_client_application_lifetime_register(app, NULL) == MC_CLIENT_APPLICATION_OWNER_OK);
    CHECK(rows(classes, app, world) == 0 && controls(classes, app) == 0);
    mc_client_screen_class_state state;
    CHECK(mc_client_screen_classes_inspect(classes, &state, NULL) == MC_CLIENT_SCREEN_OK &&
          state.phase == MC_CLIENT_SCREEN_CLASS_COLD && state.a == NULL);
    CHECK(mc_client_screen_classes_release(&classes, NULL) == MC_CLIENT_SCREEN_OK);
    CHECK(mc_client_application_lifetime_retire(app, NULL) == MC_CLIENT_APPLICATION_OWNER_OK);
    CHECK(mc_world_runtime_free(world, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_classes_free(world_classes, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_math_classes_release(&math, NULL) == MC_MATH_CLASS_OK);
    CHECK(mc_registry_classes_free(registry, NULL) == MC_REGISTRY_OK);
    CHECK(mc_resource_files_free(resources) == MC_REGISTRY_OK);
    mc_materials_destroy(materials);
    mc_java_random_profile_free(profile);
    free(app);
    printf("%u pause screen checks passed\n", checks);
    return 0;
}
