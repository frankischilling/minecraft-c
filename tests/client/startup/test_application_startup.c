#include "client/gui/loading_internal.h"
#include "client/startup/application_construction_internal.h"
#include "client/startup/application_directory_classes.h"
#include "client/startup/application_owner_internal.h"
#include "client/startup/application_startup.h"
#include "client/startup/application_startup_input.h"
#include "client/startup/application_startup_internal.h"
#include "client/startup/application_startup_statistics.h"
#include "mc/block/registry/runtime.h"
#include "mc/resources/files/files.h"
#ifdef MC_STARTUP_CONTROLLER_BOUNDARY
#include "controller_boundary_observer.h"
#endif
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_opengl.h>
#include <SDL3/SDL_video.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* All Apps come from the actual
 * whole constructor. No fabricated READY App, successful child or provider. */
static unsigned checks;
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(x)) {                                                                                \
            fprintf(stderr, "startup check%u line%d: %s\n", checks, __LINE__, #x);                 \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

typedef struct fixture {
    mc_client_application_constructor_services services;
    mc_materials *materials;
    mc_resource_files *files;
    mc_client_render_engine_classes *graphics;
    mc_client_application_construction_error *error;
} fixture;

static int fixture_open(fixture *f, const char *resources) {
    memset(f, 0, sizeof(*f));
    f->error = calloc(1, sizeof(*f->error));
    CHECK(f->error != NULL);
    CHECK(mc_java_random_profile_create(SIZE_MAX, &f->services.profile) ==
          MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_materials_create(&f->materials));
    CHECK(mc_resource_files_create(resources, &f->files) == MC_REGISTRY_OK);
    const mc_registry_options opts = {.side = MC_REGISTRY_CLIENT,
                                      .random_profile = f->services.profile,
                                      .materials = f->materials,
                                      .resources = mc_resource_files_source(f->files)};
    CHECK(mc_registry_classes_allocate(&opts, &f->services.registry, NULL) == MC_REGISTRY_OK);
    CHECK(mc_entity_classes_create(MC_ENTITY_CLIENT, f->services.profile, NULL,
                                   &f->services.entities) == MC_ENTITY_OK);
    CHECK(mc_client_render_classes_create(f->services.entities, f->services.profile,
                                          &f->services.render_classes) == MC_CLIENT_MODEL_OK);
    CHECK(mc_client_render_engine_classes_create(NULL, &f->graphics) == MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(mc_client_loading_classes_create(f->graphics, &f->services.gui_classes) ==
          MC_CLIENT_LOADING_OK);
    CHECK(mc_client_application_classes_create(NULL, &f->services.application_classes, NULL) ==
          MC_CLIENT_APPLICATION_OWNER_OK);
    CHECK(mc_sound_backend_create(f->services.profile, NULL, &f->services.sound_backend, NULL) ==
          MC_SOUND_BACKEND_OK);
    mc_sound_manager_backend backend;
    CHECK(mc_sound_backend_manager_interface(f->services.sound_backend, &backend));
    CHECK(mc_sound_manager_shared_create(&backend, &f->services.sound_shared, NULL) == MC_SOUND_OK);
    return 0;
}

static int fixture_close(fixture *f) {
    CHECK(mc_client_application_classes_release(&f->services.application_classes, NULL) ==
          MC_CLIENT_APPLICATION_OWNER_OK);
    CHECK(mc_client_loading_classes_release(f->services.gui_classes) == MC_CLIENT_LOADING_OK);
    CHECK(mc_client_render_engine_classes_release(f->graphics) == MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(mc_sound_manager_shared_retire(&f->services.sound_shared, NULL) == MC_SOUND_OK);
    CHECK(mc_sound_backend_retire(&f->services.sound_backend, NULL) == MC_SOUND_BACKEND_OK);
    CHECK(mc_client_render_classes_free(f->services.render_classes) == MC_CLIENT_MODEL_OK);
    CHECK(mc_entity_classes_free(f->services.entities) == MC_ENTITY_OK);
    if (f->services.registry != NULL)
        CHECK(mc_registry_classes_free(f->services.registry, NULL) == MC_REGISTRY_OK);
    CHECK(mc_resource_files_free(f->files) == MC_REGISTRY_OK);
    mc_materials_destroy(f->materials);
    mc_java_random_profile_free(f->services.profile);
    free(f->error);
    return 0;
}

typedef struct startup_fixture {
    mc_client_startup_services services;
    mc_client_startup_error *error;
    mc_resource_files *imported_files;
    mc_resource_files *texture_files; /* Actual jar-child class-resource provider. */
} startup_fixture;

static int startup_services_options(startup_fixture *f,
                                    const mc_game_settings_class_options *settings_options) {
    memset(f, 0, sizeof(*f));
    f->error = calloc(1, sizeof(*f->error));
    CHECK(f->error != NULL);
    CHECK(mc_java_files_create(NULL, &f->services.files, &f->error->file) == MC_JAVA_FILE_OK);
    CHECK(mc_client_save_formats_create(f->services.files, NULL, NULL, &f->services.save_formats,
                                        &f->error->save) == MC_CLIENT_SAVE_OK);
    CHECK(mc_game_settings_classes_create(f->services.files, NULL, settings_options,
                                          &f->services.settings_classes,
                                          &f->error->settings) == MC_GAME_SETTINGS_OK);
    CHECK(mc_client_directory_inputs_create(NULL, 4u * 1048576u, &f->services.directory_inputs,
                                            &f->error->directory) == MC_CLIENT_GAME_DIRECTORY_OK);
    return 0;
}

static int startup_services(startup_fixture *f) {
    return startup_services_options(f, NULL);
}

static int startup_icons(startup_fixture *f, const char *imported_resources) {
    CHECK(mc_resource_files_create(imported_resources, &f->imported_files) == MC_REGISTRY_OK);
    CHECK(mc_client_sdk_icons_allocate(mc_resource_files_source(f->imported_files),
                                       &f->services.icons,
                                       &f->error->icons) == MC_CLIENT_SDK_ICONS_OK);
    CHECK(mc_client_sdk_icons_construct(f->services.icons, &f->error->icons) ==
          MC_CLIENT_SDK_ICONS_OK);
    const size_t length = strlen(imported_resources);
    CHECK(length <= SIZE_MAX - 5);
    char *jar_root = malloc(length + 5);
    CHECK(jar_root != NULL);
    memcpy(jar_root, imported_resources, length);
    memcpy(jar_root + length, "/jar", 5);
    const mc_registry_status class_source = mc_resource_files_create(jar_root, &f->texture_files);
    free(jar_root);
    CHECK(class_source == MC_REGISTRY_OK);
    f->services.texture_resources = mc_resource_files_source(f->texture_files);
    return 0;
}

static int startup_services_close(startup_fixture *f, fixture *runtime) {
    CHECK(mc_client_sdk_icons_release(&f->services.icons, &f->error->icons) ==
          MC_CLIENT_SDK_ICONS_OK);
    CHECK(mc_game_settings_classes_release(&f->services.settings_classes, &f->error->settings) ==
          MC_GAME_SETTINGS_OK);
    CHECK(mc_client_directory_inputs_release(&f->services.directory_inputs, &f->error->directory) ==
          MC_CLIENT_GAME_DIRECTORY_OK);
    CHECK(mc_client_save_formats_release(&f->services.save_formats, &f->error->save) ==
          MC_CLIENT_SAVE_OK);
    CHECK(mc_java_files_release(&f->services.files, &f->error->file) == MC_JAVA_FILE_OK);
    /* GUI/ge class owners borrow the class provider through final retirement.
     * End the actual runtime owners before consuming either provider. */
    CHECK(fixture_close(runtime) == 0);
    if (f->texture_files != NULL) {
        CHECK(mc_resource_files_free(f->texture_files) == MC_REGISTRY_OK);
        f->texture_files = NULL;
    }
    if (f->imported_files != NULL) {
        CHECK(mc_resource_files_free(f->imported_files) == MC_REGISTRY_OK);
        f->imported_files = NULL;
    }
    free(f->error);
    return 0;
}

static int actual_app(fixture *f, bool fullscreen, client_application **out) {
    CHECK(mc_client_application_constructor_allocate(&f->services, NULL, out, f->error) ==
          MC_CLIENT_APP_OK);
    const mc_client_application_constructor_arguments arguments = {NULL, NULL, NULL,
                                                                   64,   64,   fullscreen};
    CHECK(mc_client_application_constructor_construct(*out, &arguments, f->error) ==
          MC_CLIENT_APP_OK);
    CHECK((*out)->constructor_phase == MC_CLIENT_APP_READY);
    return 0;
}

static int entity_renderer_cpu(const char *resources) {
    fixture f;
    CHECK(fixture_open(&f, resources) == 0);
    client_application *app = NULL;
    CHECK(actual_app(&f, false, &app) == 0);
    CHECK(app->font_renderer == NULL && app->settings == NULL && app->render_engine == NULL &&
          app->t == NULL);
    mc_client_entity_renderer_error *error = calloc(1, sizeof(*error));
    CHECK(error != NULL);
    const mc_client_entity_renderer_services services = {f.services.render_classes, f.graphics,
                                                         f.services.profile, NULL};
    CHECK(mc_client_entity_renderer_class_active_use(f.services.render_classes, error) ==
          MC_CLIENT_ENTITY_RENDERER_OK);
    for (unsigned case_id = 0; case_id < 4; ++case_id) {
        mc_client_entity_renderer_options options = mc_client_entity_renderer_default_options();
        if (case_id == 1)
            options.model_bytes = 0;
        if (case_id == 2)
            options.map_array.max_allocations = 0;
        if (case_id == 3)
            options.image_bytes = 0;
        mc_client_source_px *owner = NULL;
        CHECK(mc_client_entity_renderer_allocate(&services, &options, &owner, error) ==
              MC_CLIENT_ENTITY_RENDERER_OK);
        CHECK(mc_client_entity_renderer_construct(owner, app, error) !=
              MC_CLIENT_ENTITY_RENDERER_OK);
        mc_client_entity_renderer_state state;
        CHECK(mc_client_entity_renderer_inspect(owner, &state));
        CHECK(state.fields.j == app && state.fields.c == NULL && state.last_store_pc == 201 &&
              state.pending_ra != NULL && state.fields.J != NULL && state.fields.f != NULL &&
              app->t == NULL);
        if (case_id == 1) {
            CHECK(error->status == MC_CLIENT_ENTITY_RENDERER_MODEL_FAILED && error->ra_pc == 20 &&
                  state.ra_last_store_pc == 16 && state.ra_blocks == NULL &&
                  state.pending_iy == NULL);
        } else {
            CHECK(state.ra_last_store_pc == 37 && state.ra_app == app && state.ra_blocks != NULL &&
                  state.pending_iy != NULL && state.map_font == NULL &&
                  state.map_settings == NULL && state.map_engine == NULL && !state.map_lease &&
                  state.map_zero_stores == 0);
            if (case_id == 2) {
                CHECK(error->status == MC_CLIENT_ENTITY_RENDERER_ARRAY_FAILED &&
                      error->iy_pc == 8 && state.iy_last_store_pc == -1 &&
                      state.map_array == NULL && state.map_image == NULL);
            } else if (case_id == 3) {
                CHECK(error->status == MC_CLIENT_ENTITY_RENDERER_IMAGE_FAILED &&
                      error->iy_pc == 36 && state.iy_last_store_pc == 20 &&
                      state.map_array != NULL && state.map_image == NULL);
            } else {
                CHECK(error->status == MC_CLIENT_ENTITY_RENDERER_NULL_REFERENCE &&
                      error->iy_pc == 39 && state.iy_last_store_pc == 20 &&
                      state.map_array != NULL && state.map_image != NULL);
                mc_client_argb_image_state image;
                mc_client_color_array_state array;
                CHECK(mc_client_argb_image_inspect(state.map_image, &image));
                CHECK(mc_client_color_array_inspect(state.map_array, &array));
                CHECK(image.width == 128 && image.height == 128 && image.type == 2 &&
                      image.pixel_count == 16384 && array.length == 16384 &&
                      array.bits != image.pixels);
            }
        }
        CHECK(mc_client_entity_renderer_retire(&owner, error) == MC_CLIENT_ENTITY_RENDERER_OK &&
              owner == NULL);
    }
    free(error);
    CHECK(SDL_WasInit(0) == 0);
    CHECK(mc_client_application_constructor_retire(&app, f.error) == MC_CLIENT_APP_BUSY);
    CHECK(mc_client_application_classes_release(&f.services.application_classes, NULL) ==
          MC_CLIENT_APPLICATION_OWNER_OK);
    CHECK(mc_client_application_constructor_retire(&app, f.error) == MC_CLIENT_APP_OK);
    CHECK(fixture_close(&f) == 0);
    return 0;
}

static int cpu_source_prefix(const char *resources) {
    fixture f;
    startup_fixture startup;
    CHECK(fixture_open(&f, resources) == 0);
    CHECK(startup_services(&startup) == 0);
    client_application *app = NULL;
    mc_client_application_startup *attempt = NULL;
    CHECK(actual_app(&f, true, &app) == 0);
    CHECK(mc_client_application_source_stop(app, startup.error) == MC_CLIENT_STARTUP_OK);
    CHECK(!atomic_load(&app->J));
    CHECK(mc_client_application_startup_allocate(app, &startup.services, NULL, &attempt,
                                                 startup.error) == MC_CLIENT_STARTUP_OK);
    CHECK(mc_client_application_run_prefix(attempt, startup.error) ==
          MC_CLIENT_STARTUP_UNIMPLEMENTED);
    CHECK(startup.error->child.pc == 62);
    CHECK(strcmp(startup.error->child.method, "setFullscreen") == 0);
    CHECK(atomic_load(&app->J) && !app->running && !atomic_load(&app->o));
    CHECK(app->display == NULL && app->game_directory == NULL && app->aa == NULL);
    CHECK(app->Q && app->d == 64 && app->e == 64);
    mc_client_startup_state state;
    CHECK(mc_client_application_startup_inspect(attempt, &state));
    CHECK(state.run_J_store_completed && state.mode == NULL && state.display == NULL &&
          !state.Z_stored && !state.aa_stored);
    CHECK(mc_client_application_constructor_retire(&app, f.error) == MC_CLIENT_APP_BUSY);
    CHECK(app != NULL && app->constructor_phase == MC_CLIENT_APP_READY);
    CHECK(mc_client_application_source_stop(app, startup.error) == MC_CLIENT_STARTUP_OK);
    CHECK(!atomic_load(&app->J));
    CHECK(mc_client_application_startup_retire(&attempt, startup.error) == MC_CLIENT_STARTUP_OK);
    CHECK(attempt == NULL && app->startup == NULL && !atomic_load(&app->J));
    CHECK(mc_client_application_constructor_retire(&app, f.error) == MC_CLIENT_APP_BUSY);
    CHECK(app != NULL && app->constructor_phase == MC_CLIENT_APP_RETIRING && !atomic_load(&app->J));
    CHECK(mc_client_application_classes_release(&f.services.application_classes, NULL) ==
          MC_CLIENT_APPLICATION_OWNER_OK);
    CHECK(mc_client_application_constructor_retire(&app, f.error) == MC_CLIENT_APP_OK);
    CHECK(startup_services_close(&startup, &f) == 0);
    return 0;
}

static int real_window_prefix(const char *resources, const char *imported_resources,
                              const char *kind) {
    fixture f;
    startup_fixture startup;
    CHECK(fixture_open(&f, resources) == 0);
    CHECK(startup_services(&startup) == 0);
    client_application *apps[2] = {NULL, NULL};
    CHECK(startup_icons(&startup, imported_resources) == 0);
    const mc_java_file *canonical = NULL;
    const mc_player_string *canonical_title = NULL;
    const mc_player_string *canonical_font = NULL;
    const bool engine_limit = strcmp(kind, "window-render-engine-limit") == 0;
    const bool font_limit = strcmp(kind, "window-render-font-limit") == 0;
    const bool bitmap_limit = strcmp(kind, "window-bitmap-font-limit") == 0;
    const bool shared_font = strcmp(kind, "window-font-shared-owner") == 0;
    const bool color_limit = strcmp(kind, "window-color-lookup-limit") == 0;
    const bool colorizer_limit = strcmp(kind, "window-colorizer-limit") == 0;
    const bool map_array_limit = strcmp(kind, "window-entity-map-array-limit") == 0;
    const bool map_image_limit = strcmp(kind, "window-entity-map-image-limit") == 0;
    mc_client_color_array *independent_color = NULL;
    size_t independent_length = 0;
    mc_client_color_lookup_error *retained_lookup = NULL;
    mc_client_colorizer_error *retained_setter = NULL;
    /* Successful sj uses one actual runtime/Display. Its ge/nw namespace ends
     * terminally, so do not reset/replace it to pretend a second runtime works.
     * Earlier CPU-only failures may still exercise two actual App occurrences. */
    const size_t app_count = engine_limit || font_limit || bitmap_limit ? 2u : 1u;
    for (size_t index = 0; index < app_count; ++index) {
        CHECK(actual_app(&f, false, &apps[index]) == 0);
        client_application *app = apps[index];
        mc_client_application_startup *attempt = NULL;
        mc_client_startup_options options = mc_client_application_startup_default_options();
        options.hidden = true;
        if (engine_limit)
            options.render_engine.max_constructor_allocations = 0;
        if (font_limit)
            options.maximum_default_font_bytes = 0;
        if (bitmap_limit)
            options.bitmap_font.max_constructor_allocations = 0;
        if (color_limit)
            options.color_lookup.arrays.max_allocations = 0;
        if (colorizer_limit)
            options.colorizers.initial_array[MC_CLIENT_COLORIZER_WATER].max_allocations = 0;
        if (map_array_limit)
            options.entity_renderer.map_array.max_allocations = 0;
        if (map_image_limit)
            options.entity_renderer.image_bytes = 0;
        CHECK(mc_client_application_startup_allocate(app, &startup.services, &options, &attempt,
                                                     startup.error) == MC_CLIENT_STARTUP_OK);
        const mc_client_startup_status result =
            mc_client_application_run_prefix(attempt, startup.error);
        printf("actual startup window %zu status=%d caller=%u child=%u operation=%s\n", index,
               (int)result, startup.error->caller.pc, startup.error->child.pc,
               startup.error->operation == NULL ? "" : startup.error->operation);
        /* Real discovery/SDL/IO refusals fail this explicit platform profile;
         * they are never treated as skipped or expected parity. This source
         * plan requires review of its actual native home/APPDATA target before
         * execution: source b legitimately creates .minecraft when missing. */
        if (engine_limit || font_limit) {
            CHECK(result == MC_CLIENT_STARTUP_RENDER_ENGINE_FAILED &&
                  startup.error->caller.pc == 235);
            CHECK(startup.error->engine.status == (engine_limit
                                                       ? MC_CLIENT_RENDER_ENGINE_LIMIT
                                                       : MC_CLIENT_RENDER_ENGINE_FONT_FAILED));
            CHECK(startup.error->child.pc == (engine_limit ? 5u : 150u));
            mc_client_startup_state failed;
            mc_client_render_engine_state engine;
            CHECK(mc_client_application_startup_inspect(attempt, &failed));
            CHECK(failed.ji_active_use_completed && failed.D_stored && failed.z_stored &&
                  !failed.p_stored && failed.last_startup_store == 219 &&
                  failed.engine_candidate != NULL);
            CHECK(app->render_engine == NULL && app->D == failed.pack_candidate &&
                  app->settings == failed.settings_candidate);
            CHECK(mc_client_render_engine_inspect(failed.engine_candidate, &engine));
            CHECK(engine.phase == MC_CLIENT_RENDER_ENGINE_FAILED &&
                  engine.last_store_pc == (engine_limit ? 0u : 109u));
            mc_client_render_engine_class_state graphics;
            CHECK(mc_client_render_engine_classes_inspect(failed.engine_classes, &graphics));
            CHECK(graphics.ji_ready && !graphics.ji_a && graphics.native_resource_count == 0 &&
                  !graphics.resource_retirement_started);
            if (engine_limit) {
                CHECK(engine.fields.b == NULL && graphics.ge_phase == MC_CLIENT_RENDER_ENGINE_RAW);
                mc_client_font_snapshot font;
                CHECK(mc_client_default_font_inspect(failed.default_font, &font) && !font.ready &&
                      !font.failed && font.font_bytes == 0);
            } else {
                CHECK(engine.fields.l == app->D && engine.fields.i == app->settings &&
                      engine.graphics_created && !engine.graphics_disposed);
                CHECK(startup.error->engine.font.status == MC_CLIENT_FONT_LIMIT);
                mc_client_argb_image_state image;
                CHECK(mc_client_argb_image_inspect(engine.fields.m, &image) &&
                      image.native_holds == 2);
                for (size_t pixel = 0; pixel < image.pixel_count; ++pixel)
                    CHECK(image.pixels[pixel] == UINT32_C(0xffffffff));
            }
            const mc_client_render_engine_error retained = startup.error->engine;
            mc_client_render_engine *external_engine = attempt->engine_candidate;
            CHECK(mc_client_render_engine_retain(external_engine) == MC_CLIENT_RENDER_ENGINE_OK);
            CHECK(mc_client_application_startup_retire(&attempt, startup.error) ==
                  MC_CLIENT_STARTUP_BUSY);
            CHECK(attempt != NULL && attempt->engine_candidate == external_engine &&
                  app->D == failed.pack_candidate && app->settings == failed.settings_candidate &&
                  app->display == failed.display);
            CHECK(mc_client_render_engine_release(external_engine) == MC_CLIENT_RENDER_ENGINE_OK);
            CHECK(mc_client_application_startup_retire(&attempt, startup.error) ==
                      MC_CLIENT_STARTUP_OK &&
                  attempt == NULL);
            CHECK(app->render_engine == NULL && app->D == NULL && app->settings == NULL &&
                  app->startup == NULL && atomic_load(&app->J));
            CHECK(retained.operation != NULL && retained.ji_pc == (engine_limit ? 5u : 150u));
            if (font_limit)
                CHECK(retained.font.operation != NULL &&
                      retained.font.status == MC_CLIENT_FONT_LIMIT);
            continue;
        }
        if (bitmap_limit) {
            CHECK(result == MC_CLIENT_STARTUP_BITMAP_FONT_FAILED &&
                  startup.error->caller.pc == 256);
            CHECK(startup.error->bitmap_font.status == MC_CLIENT_BITMAP_FONT_LIMIT &&
                  startup.error->bitmap_font.native_policy && startup.error->child.pc == 8);
            mc_client_startup_state failed;
            mc_client_bitmap_font_state font;
            CHECK(mc_client_application_startup_inspect(attempt, &failed));
            CHECK(failed.p_stored && !failed.q_stored && failed.last_startup_store == 238 &&
                  app->render_engine == failed.engine_candidate && app->font_renderer == NULL &&
                  app->font_renderer_retained == failed.font_candidate &&
                  !app->font_renderer_constructing);
            CHECK(mc_client_bitmap_font_inspect(app->font_renderer_retained, &font));
            CHECK(font.phase == MC_CLIENT_BITMAP_FONT_FAILED && font.fields.b == NULL &&
                  font.fields.d == NULL && font.last_store_pc == -1 && !font.has_native_generation);
            CHECK(!mc_client_render_engine_resources_pending(f.graphics));
            CHECK(failed.font_classes == mc_client_loading_screen_classes(app->loading_screen) &&
                  failed.font_resource != NULL);
            if (index == 0)
                canonical_font = failed.font_resource;
            else
                CHECK(canonical_font == failed.font_resource);
            mc_client_bitmap_font *external = app->font_renderer_retained;
            CHECK(mc_client_bitmap_font_retain(external) == MC_CLIENT_BITMAP_FONT_OK);
            CHECK(mc_client_application_startup_retire(&attempt, startup.error) ==
                  MC_CLIENT_STARTUP_BUSY);
            CHECK(attempt != NULL && app->font_renderer_retained == external &&
                  app->render_engine == failed.engine_candidate &&
                  app->D == failed.pack_candidate && app->settings == failed.settings_candidate &&
                  app->display == failed.display);
            CHECK(mc_client_bitmap_font_release(&external, &startup.error->bitmap_font) ==
                      MC_CLIENT_BITMAP_FONT_OK &&
                  external == NULL);
            CHECK(mc_client_application_startup_retire(&attempt, startup.error) ==
                  MC_CLIENT_STARTUP_OK);
            CHECK(attempt == NULL && app->startup == NULL && app->font_renderer_retained == NULL &&
                  app->font_renderer == NULL && app->render_engine == NULL &&
                  app->display == NULL && atomic_load(&app->J));
            continue;
        }
        if (color_limit || colorizer_limit) {
            CHECK(result == MC_CLIENT_STARTUP_COLORS_FAILED &&
                  startup.error->caller.pc == (color_limit ? 268u : 271u));
            mc_client_startup_state failed;
            CHECK(mc_client_application_startup_inspect(attempt, &failed));
            CHECK(failed.last_startup_store == 259 && failed.q_stored && failed.p_stored &&
                  failed.colors_entered && !failed.color_metadata_retired &&
                  failed.color_lookups_completed == (color_limit ? 0u : 1u) &&
                  failed.color_setters_completed == 0 &&
                  failed.color_lookup_source_returned == !color_limit);
            CHECK(app->font_renderer != NULL && app->font_renderer_retained == app->font_renderer &&
                  app->render_engine == failed.engine_candidate && app->display == failed.display &&
                  app->t == NULL);
            CHECK(failed.color_literals[0] != NULL && failed.color_literals[1] == NULL &&
                  failed.color_literals[2] == NULL);
            mc_client_colorizer_state water, grass, foliage;
            CHECK(mc_client_colorizer_inspect(f.graphics, MC_CLIENT_COLORIZER_WATER, &water));
            CHECK(mc_client_colorizer_inspect(f.graphics, MC_CLIENT_COLORIZER_GRASS, &grass));
            CHECK(mc_client_colorizer_inspect(f.graphics, MC_CLIENT_COLORIZER_FOLIAGE, &foliage));
            CHECK(grass.phase == MC_CLIENT_COLORIZER_RAW &&
                  foliage.phase == MC_CLIENT_COLORIZER_RAW);
            mc_player_string_view name;
            CHECK(mc_player_string_read(failed.color_literals[0], &name));
            mc_client_color_array *cached = NULL;
            bool found = false;
            CHECK(mc_client_render_engine_cached_colors(
                app->render_engine, (mc_texture_pack_text){name.units, name.length, false}, &found,
                &cached));
            mc_client_texture_lookup_state pending;
            CHECK(mc_client_render_engine_lookup_inspect(app->render_engine, &pending));
            if (color_limit) {
                CHECK(startup.error->colors.status == MC_CLIENT_LOOKUP_LIMIT &&
                      startup.error->colors.native_policy &&
                      startup.error->colors.array.status == MC_CLIENT_COLOR_ARRAY_LIMIT &&
                      startup.error->colors.array.site.pc == 13 && !found &&
                      failed.color_argument == NULL);
                CHECK(pending.pending && pending.color_lookup && !pending.source_returned &&
                      water.phase == MC_CLIENT_COLORIZER_RAW && water.a == NULL);
            } else {
                CHECK(startup.error->colorizer.status == MC_CLIENT_COLORIZER_ARRAY_FAILED &&
                      startup.error->colorizer.native_policy &&
                      startup.error->colorizer.site.pc == 2);
                CHECK(found && cached == failed.color_argument &&
                      cached == failed.color_results[0] &&
                      water.phase == MC_CLIENT_COLORIZER_FAILED && water.a == NULL &&
                      !pending.pending);
                independent_color = cached;
                CHECK(mc_client_color_array_retain(independent_color) == MC_CLIENT_COLOR_ARRAY_OK);
                mc_client_color_array_state array;
                CHECK(mc_client_color_array_inspect(independent_color, &array));
                independent_length = array.length;
            }
            retained_lookup = calloc(1, sizeof(*retained_lookup));
            retained_setter = calloc(1, sizeof(*retained_setter));
            CHECK(retained_lookup != NULL && retained_setter != NULL);
            *retained_lookup = startup.error->colors;
            *retained_setter = startup.error->colorizer;
            mc_client_bitmap_font *external_font = app->font_renderer;
            CHECK(mc_client_bitmap_font_retain(external_font) == MC_CLIENT_BITMAP_FONT_OK);
            CHECK(mc_client_application_startup_retire(&attempt, startup.error) ==
                  MC_CLIENT_STARTUP_BUSY);
            CHECK(attempt != NULL && !attempt->color_metadata_retired &&
                  app->font_renderer == external_font &&
                  attempt->color_argument == failed.color_argument &&
                  app->render_engine == failed.engine_candidate);
            CHECK(mc_client_bitmap_font_release(&external_font, &startup.error->bitmap_font) ==
                  MC_CLIENT_BITMAP_FONT_OK);
            CHECK(mc_client_application_startup_retire(&attempt, startup.error) ==
                  MC_CLIENT_STARTUP_BUSY);
            CHECK(attempt != NULL && attempt->color_metadata_retired &&
                  attempt->color_argument == NULL && app->font_renderer == NULL &&
                  app->render_engine == failed.engine_candidate &&
                  app->D == failed.pack_candidate && app->settings == failed.settings_candidate &&
                  app->display == failed.display);
            CHECK(mc_client_render_engine_lookup_inspect(app->render_engine, &pending) &&
                  !pending.pending && pending.failure == NULL && pending.cleanup == NULL);
            mc_client_render_texture_error *terminal = calloc(1, sizeof(*terminal));
            CHECK(terminal != NULL);
            CHECK(mc_client_render_engine_retire_resources(f.graphics, terminal) ==
                  MC_CLIENT_RENDER_TEXTURE_OK);
            free(terminal);
            /* Genuine native cleanup diagnostic after Engine/lookup consumption:
             * the same GL_INVALID_ENUM mechanism used by the retained Settings
             * lifetime control below, with no fake child or close callback. */
            typedef void(APIENTRY * enable_fn)(GLenum);
            const SDL_FunctionPointer symbol = SDL_GL_GetProcAddress("glEnable");
            enable_fn enable = NULL;
            _Static_assert(sizeof(enable) == sizeof(symbol), "actual SDL GL bridge");
            memcpy(&enable, &symbol, sizeof(enable));
            CHECK(enable != NULL);
            enable((GLenum)UINT32_MAX);
            CHECK(mc_client_application_startup_retire(&attempt, startup.error) ==
                  MC_CLIENT_STARTUP_DISPLAY_FAILED);
            CHECK(attempt != NULL && attempt->engine_candidate == NULL &&
                  app->render_engine == NULL && app->display == NULL &&
                  attempt->failure.colors.status == retained_lookup->status &&
                  attempt->failure.colorizer.status == retained_setter->status);
            for (size_t at = 0; at < MC_CLIENT_COLORIZER_COUNT; ++at)
                CHECK(attempt->color_results[at] == NULL);
            if (retained_lookup->operation != NULL)
                CHECK(strcmp(attempt->failure.colors.operation, retained_lookup->operation) == 0);
            if (retained_setter->operation != NULL)
                CHECK(strcmp(attempt->failure.colorizer.operation, retained_setter->operation) ==
                      0);
            CHECK(mc_client_application_startup_retire(&attempt, startup.error) ==
                      MC_CLIENT_STARTUP_OK &&
                  attempt == NULL);
            continue;
        }
        if (map_array_limit || map_image_limit) {
            CHECK(result == MC_CLIENT_STARTUP_ENTITY_RENDERER_FAILED &&
                  startup.error->caller.pc == 304);
            mc_client_startup_state failed;
            mc_client_entity_renderer_state px;
            CHECK(mc_client_application_startup_inspect(attempt, &failed));
            CHECK(!failed.t_stored && app->t == NULL && failed.last_startup_store == 259 &&
                  failed.entity_renderer_candidate != NULL && failed.q_stored && failed.p_stored);
            CHECK(mc_client_entity_renderer_inspect(failed.entity_renderer_candidate, &px));
            CHECK(px.phase == MC_CLIENT_ENTITY_RENDERER_FAILED && px.last_store_pc == 201 &&
                  px.fields.c == NULL && px.pending_ra != NULL && px.ra_last_store_pc == 37 &&
                  px.ra_app == app && px.ra_blocks != NULL && px.pending_iy != NULL);
            CHECK(px.iy_last_store_pc == (map_array_limit ? -1 : 20) &&
                  px.failure->site.pc == (map_array_limit ? 8 : 36) &&
                  strcmp(px.failure->site.class_name, "iy") == 0 && !px.map_lease &&
                  px.map_image == NULL && px.map_zero_stores == 0);
            CHECK((px.map_array == NULL) == map_array_limit);
            CHECK(mc_client_application_constructor_retire(&apps[index], f.error) ==
                  MC_CLIENT_APP_BUSY);
            CHECK(mc_client_application_startup_retire(&attempt, startup.error) ==
                  MC_CLIENT_STARTUP_BUSY);
            CHECK(attempt != NULL && attempt->entity_renderer_candidate == NULL &&
                  app->font_renderer == NULL && app->render_engine != NULL && app->display != NULL);
            mc_client_render_texture_error *terminal = calloc(1, sizeof(*terminal));
            CHECK(terminal != NULL);
            CHECK(mc_client_render_engine_retire_resources(f.graphics, terminal) ==
                  MC_CLIENT_RENDER_TEXTURE_OK);
            free(terminal);
            CHECK(mc_client_application_startup_retire(&attempt, startup.error) ==
                      MC_CLIENT_STARTUP_OK &&
                  attempt == NULL && app->startup == NULL && app->display == NULL);
            continue;
        }
        CHECK(result == MC_CLIENT_STARTUP_STATISTICS_FAILED && startup.error->caller.pc == 337 &&
              startup.error->child.pc == 48 && strcmp(startup.error->child.class_name, "cj") == 0 &&
              strcmp(startup.error->child.method, "<init>") == 0 &&
              strcmp(startup.error->child.descriptor, "(Lgr;Lxi;Ljava/io/File;)V") == 0);
        const mc_client_startup_stats_call *stats_call = attempt->statistics;
        CHECK(app->session == NULL && stats_call != NULL && stats_call->entered &&
              !stats_call->active && !stats_call->I_stored && !stats_call->callback_installed &&
              !stats_call->source_calls_completed && app->stats_writer == NULL &&
              app->stats_writer_retained == stats_call->candidate &&
              stats_call->candidate != NULL && stats_call->published == NULL &&
              stats_call->failure.writer.exception == MC_CLIENT_STATS_NULL_REFERENCE &&
              stats_call->failure.source_writer.base.bytecode == 48 && attempt->logo == NULL &&
              attempt->input == NULL && app->C == NULL);
        mc_client_stats_state stats_state;
        mc_client_stats_file_state stats_files;
        CHECK(mc_client_stats_writer_read_state(app->stats_writer_retained, &stats_state) &&
              stats_state.phase == MC_CLIENT_STATS_FAILED_CONSTRUCTION &&
              stats_state.workers_started == 0 && stats_state.workers_joined == 0);
        CHECK(mc_client_stats_writer_read_file_state(app->stats_writer_retained, &stats_files) &&
              stats_files.parent == app->game_directory && stats_files.pending_file != NULL &&
              stats_files.pending_builder && stats_files.children[0] == NULL);
        CHECK(app->startup->settings_error_line == NULL &&
              startup.error->settings.line.units == NULL);
        mc_client_startup_state state;
        CHECK(mc_client_application_startup_inspect(attempt, &state));
        CHECK(state.display_created && state.Z_stored && state.aa_stored &&
              state.last_startup_store == 307 && state.z_stored && state.D_stored &&
              state.p_stored && state.q_stored && app->startup == attempt);
        CHECK(app->display == state.display && app->game_directory != NULL && app->aa != NULL);
        CHECK(app->diagnostic_game_directory == NULL && app->settings == state.settings_candidate &&
              app->settings != NULL && app->D == state.pack_candidate && app->D != NULL);
        mc_game_settings_source_state actual_settings;
        mc_game_settings_state values;
        CHECK(mc_game_settings_source_inspect(app->settings, &actual_settings));
        CHECK(actual_settings.phase == MC_GAME_SETTINGS_SOURCE_READY && actual_settings.N != NULL &&
              actual_settings.source_stores == 36 && actual_settings.last_store_pc == 371);
        CHECK(mc_game_settings_read_state(app->settings, &values));
        CHECK(values.minecraft == app && values.file_identity == actual_settings.N &&
              values.options_path == NULL && actual_settings.N != app->game_directory);
        CHECK(atomic_load(&app->J) && !app->running && !atomic_load(&app->o));
        CHECK(app->render_engine == state.engine_candidate && state.engine_candidate != NULL &&
              state.engine_classes == mc_client_loading_screen_graphics(app->loading_screen) &&
              state.ji_active_use_completed);
        mc_client_render_engine_state engine;
        CHECK(mc_client_render_engine_inspect(app->render_engine, &engine));
        CHECK(engine.phase == MC_CLIENT_RENDER_ENGINE_READY && engine.last_store_pc == 109 &&
              engine.fields.l == app->D && engine.fields.i == app->settings &&
              engine.graphics_disposed);
        CHECK(mc_client_render_engine_class_identity(app->render_engine) == state.engine_classes);
        mc_client_render_engine_class_state graphics;
        CHECK(mc_client_render_engine_classes_inspect(state.engine_classes, &graphics) &&
              graphics.ji_ready && graphics.ge_phase == MC_CLIENT_RENDER_ENGINE_READY &&
              graphics.native_resource_count != 0 && !graphics.resource_retirement_started);
        CHECK(mc_client_render_engine_resources_pending(f.graphics));
        CHECK(app->font_renderer != NULL && app->font_renderer == app->font_renderer_retained &&
              app->font_renderer == state.font_candidate && !app->font_renderer_constructing);
        CHECK(state.font_classes == mc_client_loading_screen_classes(app->loading_screen) &&
              state.font_classes == f.services.gui_classes && state.font_resource != NULL);
        CHECK(state.colors_entered && !state.color_metadata_retired &&
              state.color_argument == NULL && state.color_lookups_completed == 3 &&
              state.color_setters_completed == 3 && state.color_lookup_source_returned &&
              state.last_color_lookup_pc == 292 && state.last_color_setter_pc == 295 &&
              state.t_stored && app->t == state.entity_renderer_candidate && app->t != NULL);
        static const mc_client_colorizer_kind kinds[] = {
            MC_CLIENT_COLORIZER_WATER, MC_CLIENT_COLORIZER_GRASS, MC_CLIENT_COLORIZER_FOLIAGE};
        for (size_t at = 0; at < MC_CLIENT_COLORIZER_COUNT; ++at) {
            CHECK(state.color_literals[at] != NULL &&
                  state.color_literals[at] == app->source_namespace->startup_color_literals[at]);
            mc_player_string_view name;
            CHECK(mc_player_string_read(state.color_literals[at], &name));
            bool found = false;
            mc_client_color_array *cached = NULL;
            CHECK(mc_client_render_engine_cached_colors(
                      app->render_engine, (mc_texture_pack_text){name.units, name.length, false},
                      &found, &cached) &&
                  found);
            mc_client_colorizer_state colorizer;
            CHECK(mc_client_colorizer_inspect(f.graphics, kinds[at], &colorizer));
            CHECK(colorizer.phase == MC_CLIENT_COLORIZER_READY && colorizer.last_store_pc == 1 &&
                  colorizer.a == cached && cached == state.color_results[at]);
            mc_client_color_array_state array;
            CHECK(mc_client_color_array_inspect(cached, &array) &&
                  array.phase == MC_CLIENT_COLOR_ARRAY_READY);
            CHECK(array.length != 0); /* No invented65536 requirement for real fallback4096. */
            if (at == 0) {
                independent_color = cached;
                independent_length = array.length;
                CHECK(mc_client_color_array_retain(independent_color) == MC_CLIENT_COLOR_ARRAY_OK);
            }
            for (size_t other = 0; other < at; ++other)
                CHECK(state.color_results[other] != cached);
        }
        mc_client_entity_renderer_state px;
        CHECK(mc_client_entity_renderer_inspect(app->t, &px));
        CHECK(px.phase == MC_CLIENT_ENTITY_RENDERER_READY && px.last_store_pc == 213 &&
              px.ra_phase == MC_CLIENT_ENTITY_RENDERER_READY && px.ra_last_store_pc == 60 &&
              px.iy_phase == MC_CLIENT_ENTITY_RENDERER_READY && px.iy_last_store_pc == 42 &&
              px.fields.j == app && px.ra_app == app && px.fields.c == px.pending_ra &&
              px.ra_blocks != NULL && px.fields.f != NULL && px.fields.J != NULL &&
              px.map_font == app->font_renderer && px.map_settings == app->settings &&
              px.map_engine == app->render_engine && px.map_lease && px.map_zero_stores == 16384);
        CHECK(state.manager_consumer_attached && state.manager_f_stored &&
              state.last_manager_pc == 321 && state.manager_classes != NULL &&
              state.manager_capture != NULL && state.manager_hand_candidate == NULL);
        mc_client_render_manager_classes *global_manager = attempt->manager_classes;
        mc_client_manager_state manager;
        CHECK(mc_client_render_manager_inspect(global_manager, &manager));
        CHECK(manager.a == state.manager_capture && manager.entries == 28 &&
              manager.completed_bw_stores == 28 && manager.hand_occurrence != NULL &&
              manager.fields.f != NULL && manager.fields.f != px.fields.c);
        mc_client_held_item_state global_hand;
        CHECK(mc_client_held_item_inspect(manager.hand_occurrence, &global_hand));
        CHECK(global_hand.pending_ra == manager.fields.f && global_hand.ra_app == app &&
              global_hand.map_font == app->font_renderer &&
              global_hand.map_engine == app->render_engine &&
              global_hand.map_settings == app->settings &&
              global_hand.pending_ra != px.pending_ra && global_hand.map_image != px.map_image &&
              global_hand.map_array != px.map_array && global_hand.ra_blocks != px.ra_blocks &&
              global_hand.map_lease && global_hand.map_zero_stores == 16384 &&
              global_hand.ra_last_store_pc == 60);
        CHECK(mc_client_render_manager_classes_retire(&global_manager, &startup.error->manager) ==
              MC_CLIENT_MANAGER_BUSY);
        /* Real completed RA followed by a null captured receiver: failure is
         * late putfield321, no earlier dummy child or fake successful source. */
        const mc_client_entity_renderer_services hand_services = {
            f.services.render_classes, f.graphics, f.services.profile, app->display};
        mc_client_held_item *late_null = NULL;
        CHECK(mc_client_held_item_allocate(&hand_services, NULL, &late_null,
                                           &startup.error->entity_renderer) ==
              MC_CLIENT_ENTITY_RENDERER_OK);
        CHECK(mc_client_held_item_construct(late_null, app, &startup.error->entity_renderer) ==
              MC_CLIENT_ENTITY_RENDERER_OK);
        const mc_client_source_ra *late_identity = mc_client_held_item_identity(late_null);
        CHECK(mc_client_render_manager_publish_hand(global_manager, NULL, &late_null,
                                                    &startup.error->manager) ==
                  MC_CLIENT_MANAGER_NULL_REFERENCE &&
              startup.error->manager.pc == 321 && late_null != NULL &&
              mc_client_held_item_identity(late_null) == late_identity);
        CHECK(mc_client_held_item_retire(&late_null, &startup.error->entity_renderer) ==
                  MC_CLIENT_ENTITY_RENDERER_OK &&
              late_null == NULL);
        /* Another genuine RA over this real App, using the SAME body, replaces
         * global F and retains the old identity for native terminal cleanup.
         * This arranged direct-call control is not a second whole startup. */
        mc_client_held_item *replacement = NULL;
        CHECK(mc_client_held_item_allocate(&hand_services, NULL, &replacement,
                                           &startup.error->entity_renderer) ==
              MC_CLIENT_ENTITY_RENDERER_OK);
        CHECK(mc_client_held_item_construct(replacement, app, &startup.error->entity_renderer) ==
              MC_CLIENT_ENTITY_RENDERER_OK);
        const mc_client_source_ra *replacement_identity = mc_client_held_item_identity(replacement);
        CHECK(mc_client_render_manager_publish_hand(global_manager, attempt->manager_capture,
                                                    &replacement, &startup.error->manager) ==
                  MC_CLIENT_MANAGER_OK &&
              replacement == NULL);
        CHECK(mc_client_render_manager_inspect(global_manager, &manager) &&
              manager.fields.f == replacement_identity && manager.displaced_hands == 1);
        mc_client_held_item *same = manager.hand_occurrence;
        CHECK(mc_client_held_item_retain(same, &startup.error->entity_renderer) ==
              MC_CLIENT_ENTITY_RENDERER_OK);
        CHECK(mc_client_render_manager_publish_hand(global_manager, attempt->manager_capture, &same,
                                                    &startup.error->manager) ==
                  MC_CLIENT_MANAGER_OK &&
              same == NULL);
        CHECK(mc_client_render_manager_inspect(global_manager, &manager) &&
              manager.fields.f == replacement_identity && manager.displaced_hands == 1);
        CHECK(mc_client_held_item_inspect(manager.hand_occurrence, &global_hand) &&
              global_hand.holds == 1);
        mc_client_held_item *external_global = manager.hand_occurrence;
        CHECK(mc_client_held_item_retain(external_global, &startup.error->entity_renderer) ==
              MC_CLIENT_ENTITY_RENDERER_OK);
        mc_client_color_array_state map_array;
        mc_client_argb_image_state map_image;
        CHECK(mc_client_color_array_inspect(px.map_array, &map_array));
        CHECK(mc_client_argb_image_inspect(px.map_image, &map_image));
        CHECK(map_array.length == 16384 && map_image.width == 128 && map_image.height == 128 &&
              map_image.type == 2 && map_image.pixel_count == 16384 &&
              map_array.bits != map_image.pixels);
        for (size_t at = 0; at < 16384; ++at)
            CHECK(map_array.bits[at] == 0 && map_image.pixels[at] == 0);
        mc_client_texture map_id;
        memcpy(&map_id, &px.map_texture, sizeof(map_id));
        mc_client_argb_image *map_registered = NULL;
        CHECK(
            mc_client_render_engine_registered_image(app->render_engine, map_id, &map_registered) &&
            map_registered == px.map_image);
        GLint bound = 0, width = 0, height = 0;
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &bound);
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &width);
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &height);
        mc_client_texture current_map_id;
        memcpy(&current_map_id, &global_hand.map_texture, sizeof(current_map_id));
        CHECK((uint32_t)bound == current_map_id && width == 128 && height == 128 &&
              glGetError() == GL_NO_ERROR);
        uint8_t *uploaded = malloc(16384u * 4u);
        CHECK(uploaded != NULL);
        memset(uploaded, 0xa5, 16384u * 4u);
        glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, uploaded);
        CHECK(glGetError() == GL_NO_ERROR);
        for (size_t at = 0; at < 16384u * 4u; ++at)
            CHECK(uploaded[at] == 0);
        free(uploaded);
        mc_client_source_px *external_px = app->t;
        CHECK(mc_client_entity_renderer_retain(external_px, &startup.error->entity_renderer) ==
              MC_CLIENT_ENTITY_RENDERER_OK);
        CHECK(mc_client_application_startup_retire(&attempt, startup.error) ==
              MC_CLIENT_STARTUP_BUSY);
        CHECK(app->t == external_px && app->font_renderer != NULL &&
              app->display == state.display && attempt->entity_renderer_candidate == external_px);
        CHECK(attempt->statistics == stats_call && stats_call->retired &&
              stats_call->candidate == NULL && stats_call->published == NULL &&
              app->stats_writer == NULL && app->stats_writer_retained == NULL &&
              app->statistics_retired && stats_call->failure.source_writer.base.bytecode == 48 &&
              strcmp(stats_call->failure.source_writer.base.class_name, "cj") == 0);
        CHECK(mc_client_application_constructor_retire(&apps[index], f.error) ==
              MC_CLIENT_APP_BUSY);
        CHECK(mc_client_entity_renderer_release(&external_px, &startup.error->entity_renderer) ==
                  MC_CLIENT_ENTITY_RENDERER_OK &&
              external_px == NULL);
        CHECK(!attempt->manager_consumer.attached && attempt->manager_classes == NULL &&
              attempt->manager_capture == NULL);
        CHECK(mc_client_render_manager_retire_hands(global_manager, &startup.error->manager) ==
              MC_CLIENT_MANAGER_BUSY);
        CHECK(mc_client_render_manager_inspect(global_manager, &manager) &&
              manager.phase == MC_CLIENT_MANAGER_RETIRING && !manager.source_views_available);
        mc_client_render_manager *rejected = NULL;
        CHECK(mc_client_render_manager_static_get(global_manager, &rejected,
                                                  &startup.error->manager) ==
                  MC_CLIENT_MANAGER_INVALID_PHASE &&
              rejected == NULL);
        CHECK(mc_client_held_item_identity(external_global) == replacement_identity);
        CHECK(app->font_renderer != NULL && app->render_engine != NULL &&
              app->display == state.display);
        CHECK(mc_client_held_item_release(&external_global, &startup.error->entity_renderer) ==
                  MC_CLIENT_ENTITY_RENDERER_OK &&
              external_global == NULL);
        /* Actual fixture/runtime authority ends global RA ownership with the
         * real Display/Settings/App still alive, then ends this TH namespace.
         * This is never called by the per-App retirement implementation. */
        CHECK(mc_client_render_manager_retire_hands(global_manager, &startup.error->manager) ==
              MC_CLIENT_MANAGER_OK);
        CHECK(mc_client_render_manager_classes_retire(&global_manager, &startup.error->manager) ==
                  MC_CLIENT_MANAGER_OK &&
              global_manager == NULL);
        mc_client_bitmap_font_state font;
        CHECK(mc_client_bitmap_font_inspect(app->font_renderer, &font));
        CHECK(font.phase == MC_CLIENT_BITMAP_FONT_READY && font.widths_completed == 256 &&
              font.glyphs_defined == 256 && font.palettes_defined == 32 &&
              font.last_store_pc == 255 && font.has_native_generation);
        mc_client_argb_image *registered = NULL;
        mc_client_texture texture;
        memcpy(&texture, &font.fields.a, sizeof(texture));
        CHECK(mc_client_render_engine_registered_image(app->render_engine, texture, &registered) &&
              registered == font.retained_image && registered != NULL &&
              registered != px.map_image);
        mc_client_font_mapping_state mapping;
        CHECK(mc_client_font_mapping_inspect(f.services.gui_classes->mapping, &mapping) &&
              mapping.phase == MC_REGISTRY_COLD && mapping.a == NULL && mapping.b == NULL);
        mc_client_bitmap_font *external_font = app->font_renderer;
        CHECK(mc_client_bitmap_font_retain(external_font) == MC_CLIENT_BITMAP_FONT_OK);
        CHECK(mc_client_application_startup_retire(&attempt, startup.error) ==
              MC_CLIENT_STARTUP_BUSY);
        CHECK(attempt != NULL && app->t == NULL && !attempt->t_stored &&
              attempt->entity_renderer_candidate == NULL && app->font_renderer == external_font &&
              app->font_renderer_retained == external_font &&
              app->render_engine == state.engine_candidate && app->D == state.pack_candidate &&
              app->settings == state.settings_candidate && app->display == state.display);
        CHECK(attempt->statistics == stats_call && stats_call->retired &&
              stats_call->candidate == NULL && stats_call->published == NULL &&
              stats_call->failure.source_writer.base.bytecode == 48 &&
              stats_call->failure.source_writer.base.operation != NULL &&
              strcmp(stats_call->failure.source_writer.base.class_name, "cj") == 0);
        CHECK(mc_client_bitmap_font_release(&external_font, &startup.error->bitmap_font) ==
                  MC_CLIENT_BITMAP_FONT_OK &&
              external_font == NULL);
        mc_client_bitmap_font *shared = NULL;
        if (shared_font) {
            /* A genuine second sj on the SAME GUI/Engine/Display, not a proxy,
             * lease flag, replacement namespace or post-hoc App.q store. */
            CHECK(mc_client_bitmap_font_allocate(f.services.gui_classes, NULL, &shared) ==
                  MC_CLIENT_BITMAP_FONT_OK);
            CHECK(mc_client_bitmap_font_construct(
                      shared, app->settings, attempt->font_resource, app->render_engine,
                      app->display, &startup.error->bitmap_font) == MC_CLIENT_BITMAP_FONT_OK);
        }
        CHECK(mc_client_application_startup_retire(&attempt, startup.error) ==
              MC_CLIENT_STARTUP_BUSY);
        CHECK(attempt != NULL && app->font_renderer == NULL &&
              app->font_renderer_retained == NULL && attempt->font_candidate == NULL &&
              !attempt->q_stored && attempt->color_metadata_retired &&
              attempt->color_argument == NULL && app->render_engine == state.engine_candidate &&
              app->D == state.pack_candidate && app->settings == state.settings_candidate &&
              app->display == state.display);
        CHECK(mc_client_render_engine_classes_inspect(f.graphics, &graphics) &&
              !graphics.resource_retirement_started &&
              mc_client_render_engine_resources_pending(f.graphics));
        mc_client_render_texture_error *terminal = calloc(1, sizeof(*terminal));
        CHECK(terminal != NULL);
        if (shared_font) {
            CHECK(mc_client_render_engine_retire_resources(f.graphics, terminal) ==
                  MC_CLIENT_RENDER_TEXTURE_BUSY);
            CHECK(mc_client_render_engine_classes_inspect(f.graphics, &graphics) &&
                  !graphics.resource_retirement_started);
            CHECK(mc_client_bitmap_font_retire(&shared, &startup.error->bitmap_font) ==
                      MC_CLIENT_BITMAP_FONT_OK &&
                  shared == NULL);
        }
        /* The actual fixture/runtime namespace owner, not startup/App, invokes
         * the canonical terminal operation while its SAME Display is alive. */
        CHECK(app->display == state.display);
        CHECK(mc_client_render_engine_retire_resources(f.graphics, terminal) ==
              MC_CLIENT_RENDER_TEXTURE_OK);
        CHECK(!mc_client_render_engine_resources_pending(f.graphics));
        CHECK(mc_client_render_engine_classes_inspect(f.graphics, &graphics) &&
              graphics.resource_retirement_started && graphics.native_resource_count == 0);
        free(terminal);
        mc_client_render_engine *external_engine = app->render_engine;
        CHECK(mc_client_render_engine_retain(external_engine) == MC_CLIENT_RENDER_ENGINE_OK);
        CHECK(mc_client_application_startup_retire(&attempt, startup.error) ==
              MC_CLIENT_STARTUP_BUSY);
        CHECK(app->render_engine == external_engine && app->D == state.pack_candidate &&
              app->settings == state.settings_candidate && app->display == state.display &&
              attempt != NULL);
        CHECK(mc_client_render_engine_release(external_engine) == MC_CLIENT_RENDER_ENGINE_OK);

        CHECK(mc_client_texture_pack_list_retain(app->D) == MC_TEXTURE_PACK_OK);
        CHECK(mc_client_application_startup_retire(&attempt, startup.error) ==
              MC_CLIENT_STARTUP_BUSY);
        CHECK(attempt != NULL && app->settings == state.settings_candidate &&
              app->D == state.pack_candidate);
        CHECK(app->render_engine == NULL && attempt->engine_candidate == NULL &&
              attempt->default_font == NULL);
        CHECK(mc_client_texture_pack_list_release(app->D, NULL) == MC_TEXTURE_PACK_OK);
        mc_client_texture_pack *borrowed = NULL;
        CHECK(mc_client_texture_pack_selected(app->D, &borrowed) && borrowed != NULL);
        CHECK(mc_client_texture_pack_retain(borrowed) == MC_TEXTURE_PACK_OK);
        CHECK(mc_client_application_startup_retire(&attempt, startup.error) ==
              MC_CLIENT_STARTUP_BUSY);
        CHECK(app->settings == state.settings_candidate && app->D == state.pack_candidate &&
              app->startup == attempt);
        CHECK(mc_client_texture_pack_release(borrowed, NULL) == MC_TEXTURE_PACK_OK);

        mc_client_application_class_state classes;
        CHECK(mc_client_application_classes_inspect(f.services.application_classes, &classes));
        CHECK(classes.af == app->game_directory && classes.a == app);
        const mc_nbt_string path = mc_world_info_name_text(mc_java_file_path(app->game_directory));
        printf("actual startup directory UTF16[%zu]", path.length);
        for (size_t unit = 0; unit < path.length; ++unit)
            printf(" %04x", (unsigned int)path.units[unit]);
        putchar('\n');
        mc_client_save_format_state saves;
        CHECK(mc_client_save_format_read_state(app->aa, &saves, &startup.error->save) ==
              MC_CLIENT_SAVE_OK);
        CHECK(saves.phase == MC_CLIENT_SAVE_READY && saves.last_store_pc == 18);
        CHECK(saves.source_a == state.saves_candidate && saves.source_a != app->game_directory);
        mc_client_display_stage_state display;
        CHECK(mc_client_display_stage_inspect(app->display, &display));
        CHECK(display.phase == MC_CLIENT_DISPLAY_READY && display.title != NULL);
        mc_client_display_mode_state mode;
        CHECK(mc_client_display_mode_inspect(state.mode, &mode));
        CHECK(mode.width == 64 && mode.height == 64 && mode.bits_per_pixel == 0 &&
              mode.frequency == 0 && !mode.fullscreen_capable);
        mc_client_directory_dispatch_state dispatch;
        CHECK(mc_client_application_directory_dispatch_inspect(f.services.application_classes,
                                                               &dispatch));
        CHECK(dispatch.pd_phase == MC_CLIENT_APPLICATION_CLASS_READY &&
              dispatch.kn_phase == MC_CLIENT_APPLICATION_CLASS_READY &&
              dispatch.enum_allocations == 5 && dispatch.values_allocations == 1 &&
              dispatch.clone_allocations == 1 && dispatch.map_allocations == 1);
        for (size_t item = 0; item < 5; ++item) {
            CHECK(dispatch.pd[item] != NULL && dispatch.names[item] != NULL &&
                  dispatch.values_entries[item] == dispatch.pd[item]);
            CHECK(dispatch.switch_map[item] == (item < 4 ? (int32_t)item + 1 : 0));
            for (size_t other = item + 1; other < 5; ++other)
                CHECK(dispatch.pd[item] != dispatch.pd[other]);
        }
        if (index == 0) {
            canonical = app->game_directory;
            canonical_title = display.title;
        } else
            CHECK(canonical == app->game_directory && canonical_title == display.title);
        CHECK(mc_client_application_constructor_retire(&app, f.error) == MC_CLIENT_APP_BUSY);
        CHECK(app == apps[index] && app->constructor_phase == MC_CLIENT_APP_READY);
        CHECK(mc_client_application_startup_retire(&attempt, startup.error) ==
              MC_CLIENT_STARTUP_OK);
        CHECK(app->display == NULL && app->game_directory == NULL && app->aa == NULL &&
              app->startup == NULL && app->settings == NULL && app->render_engine == NULL &&
              app->D == NULL && atomic_load(&app->J));
    }
    for (size_t index = 0; index + 1u < app_count; ++index)
        CHECK(mc_client_application_constructor_retire(&apps[index], f.error) == MC_CLIENT_APP_OK);
    const size_t current = app_count - 1u;
    CHECK(mc_client_application_constructor_retire(&apps[current], f.error) == MC_CLIENT_APP_BUSY);
    CHECK(atomic_load(&apps[current]->J)); /* Native cleanup did not invent source f. */
    CHECK(mc_client_application_classes_release(&f.services.application_classes, NULL) ==
          MC_CLIENT_APPLICATION_OWNER_OK);
    CHECK(mc_client_application_constructor_retire(&apps[current], f.error) == MC_CLIENT_APP_OK);
    CHECK(startup_services_close(&startup, &f) == 0);
    if (independent_color != NULL) {
        mc_client_color_array_state array;
        CHECK(mc_client_color_array_inspect(independent_color, &array) &&
              array.phase == MC_CLIENT_COLOR_ARRAY_READY && array.length == independent_length);
        CHECK(mc_client_color_array_release(&independent_color, NULL) == MC_CLIENT_COLOR_ARRAY_OK &&
              independent_color == NULL);
    }
    /* These copied diagnostic pointers were traced to static source text,
     * not Engine/Pack/array-owned views; exercise their lifetime after actual
     * parent/namespace/resource retirement in these native controls. */
    if (retained_lookup != NULL && retained_lookup->operation != NULL)
        CHECK(strlen(retained_lookup->operation) != 0);
    if (retained_setter != NULL && retained_setter->operation != NULL)
        CHECK(strlen(retained_setter->operation) != 0);
    free(retained_lookup);
    free(retained_setter);
    return 0;
}

/* Prepared actual-owner retirement control, NOT MC.a execution evidence. The
 * genuine App constructor and genuine standalone kv(App,realFile) run; no z,
 * af, Z or aa source store is arranged and no READY descendant is fabricated.
 * The record owns actual failed Settings + actual staged Display for cleanup. */
static uint64_t diagnostic_hash(mc_game_settings_text text) {
    uint64_t value = UINT64_C(1469598103934665603);
    for (size_t index = 0; index < text.length; ++index) {
        value ^= text.units[index];
        value *= UINT64_C(1099511628211);
    }
    return value;
}

static int prepared_line_lifetime(const char *resources, const char *imported,
                                  const char *fixture_directory, bool output_limit) {
    fixture f;
    startup_fixture startup;
    CHECK(fixture_open(&f, resources) == 0);
    mc_game_settings_class_options caps = mc_game_settings_class_default_options();
    if (output_limit)
        caps.maximum_string_bytes = 128; /* Class keys fit; a long bad-line output does not. */
    CHECK(startup_services_options(&startup, &caps) == 0);
    CHECK(startup_icons(&startup, imported) == 0);
    client_application *app = NULL;
    CHECK(actual_app(&f, false, &app) == 0);
    mc_client_application_startup *record = NULL;
    CHECK(mc_client_application_startup_allocate(app, &startup.services, NULL, &record,
                                                 startup.error) == MC_CLIENT_STARTUP_OK);
    const size_t length = strlen(fixture_directory);
    CHECK(length <= MC_GAME_SETTINGS_MAX_PATH_BYTES && length < SIZE_MAX / sizeof(uint16_t));
    uint16_t *units = calloc(length == 0 ? 1 : length, sizeof(*units));
    CHECK(units != NULL);
    for (size_t index = 0; index < length; ++index) {
        CHECK((unsigned char)fixture_directory[index] < 128);
        units[index] = (unsigned char)fixture_directory[index];
    }
    mc_world_info_name *name = NULL;
    CHECK(mc_world_info_name_copy((mc_nbt_string){units, length}, SIZE_MAX, &name) ==
          MC_WORLD_INFO_OK);
    free(units);
    mc_java_file *parent = NULL;
    CHECK(mc_java_file_allocate(startup.services.files, &parent, &startup.error->file) ==
          MC_JAVA_FILE_OK);
    CHECK(mc_java_file_construct_string(parent, name, &startup.error->file) == MC_JAVA_FILE_OK);
    mc_world_info_name_release(name);
    CHECK(mc_game_settings_source_allocate(startup.services.settings_classes, NULL,
                                           &record->settings_candidate,
                                           &record->failure.settings) == MC_GAME_SETTINGS_OK);
    CHECK(mc_game_settings_source_construct(record->settings_candidate, app, parent, NULL,
                                            &record->failure.settings) == MC_GAME_SETTINGS_LIMIT);
    CHECK(record->failure.settings.line.units != NULL && record->failure.settings.line.length != 0);
    CHECK(record->failure.settings.source_exception == MC_GAME_SETTINGS_SOURCE_NUMBER_FORMAT);
    CHECK(!record->z_stored && app->settings == NULL && app->game_directory == NULL &&
          app->aa == NULL);
    record->failure.status = MC_CLIENT_STARTUP_SETTINGS_FAILED;
    record->failure.operation = "prepared actual kv native failure";
#ifdef MC_SETTINGS_CAPTURE_ALLOCATION_CONTROL
    /* A future separately declared direct Info object uses the SAME name-copy
     * body with only its native allocator failing once. No child result mock. */
    extern void mc_test_settings_capture_fail_next(void);
    mc_test_settings_capture_fail_next();
    CHECK(application_startup_retain_settings_error(record, startup.error) ==
          MC_CLIENT_STARTUP_STRING_FAILED);
    CHECK(record->settings_error_line == NULL && record->settings_candidate != NULL &&
          record->failure.settings.native.status == MC_GAME_SETTINGS_LIMIT);
#endif
    CHECK(application_startup_retain_settings_error(record, startup.error) == MC_CLIENT_STARTUP_OK);
    CHECK(record->settings_error_line != NULL);
    const mc_game_settings_source_error copied = record->failure.settings;
    const uint64_t before = diagnostic_hash(copied.line);
    CHECK(mc_client_display_mode_allocate(&record->mode, &startup.error->display));
    CHECK(mc_client_display_mode_construct(record->mode, 64, 64, &startup.error->display));
    CHECK(mc_client_display_stage_allocate(&record->display, &startup.error->display));
    CHECK(mc_client_display_stage_bind_icons(record->display, startup.services.icons,
                                             &startup.error->display));
    CHECK(mc_client_display_stage_set_mode(record->display, record->mode, &startup.error->display));
    static const uint16_t title_units[] = {'L', 'i', 'f', 'e', 't', 'i', 'm', 'e'};
    mc_player_string *title = NULL;
    CHECK(mc_player_string_create(title_units, sizeof(title_units) / sizeof(title_units[0]),
                                  &title) == MC_PLAYER_OK);
    CHECK(mc_client_display_stage_set_title(record->display, title, &startup.error->display));
    mc_player_string_release(title);
    CHECK(mc_client_display_create_staged(record->display, true, &startup.error->display));
    app->display = record->display; /* Native resource view only, not a source field. */
    record->display_created = true;
    typedef void(APIENTRY * enable_fn)(GLenum);
    const SDL_FunctionPointer symbol = SDL_GL_GetProcAddress("glEnable");
    enable_fn enable = NULL;
    _Static_assert(sizeof(enable) == sizeof(symbol), "actual SDL GL function pointer bridge");
    memcpy(&enable, &symbol, sizeof(enable));
    CHECK(enable != NULL);
    enable((GLenum)UINT32_MAX); /* Real pending GL_INVALID_ENUM; no fake cleanup callback. */
    CHECK(mc_client_application_startup_retire(&record, startup.error) ==
          MC_CLIENT_STARTUP_DISPLAY_FAILED);
    CHECK(record != NULL && record->settings_candidate == NULL && record->display == NULL &&
          app->display == NULL && app->startup == record && record->settings_error_line != NULL);
    CHECK(record->failure.settings.line.units == copied.line.units &&
          diagnostic_hash(record->failure.settings.line) == before &&
          diagnostic_hash(copied.line) == before);
    CHECK(mc_client_application_startup_retire(&record, startup.error) == MC_CLIENT_STARTUP_OK);
    CHECK(record == NULL && app->startup == NULL && atomic_load(&app->J));
    CHECK(mc_java_file_release(&parent, &startup.error->file) == MC_JAVA_FILE_OK);
    CHECK(mc_client_application_constructor_retire(&app, f.error) == MC_CLIENT_APP_BUSY);
    CHECK(mc_client_application_classes_release(&f.services.application_classes, NULL) ==
          MC_CLIENT_APPLICATION_OWNER_OK);
    CHECK(mc_client_application_constructor_retire(&app, f.error) == MC_CLIENT_APP_OK);
    CHECK(startup_services_close(&startup, &f) == 0);
    return 0;
}

#include "application_startup_statistics_controls.inc"

int main(int argc, char **argv) {
    CHECK(argc >= 3);
    if (strcmp(argv[2], "entity-renderer-cpu") == 0)
        CHECK(entity_renderer_cpu(argv[1]) == 0);
    else if (strcmp(argv[2], "cpu") == 0)
        CHECK(cpu_source_prefix(argv[1]) == 0);
    else if (strcmp(argv[2], "lifetime-output-limit") == 0 ||
             strcmp(argv[2], "lifetime-read-limit") == 0) {
        CHECK(argc == 5);
        CHECK(prepared_line_lifetime(argv[1], argv[3], argv[4],
                                     strcmp(argv[2], "lifetime-output-limit") == 0) == 0);
    } else if (strcmp(argv[2], "statistics-admission") == 0) {
        CHECK(argc == 3);
        CHECK(statistics_admission_profiles(argv[1]) == 0);
    } else if (strcmp(argv[2], "statistics-null-z") == 0) {
        CHECK(argc == 3);
        CHECK(statistics_null_z(argv[1]) == 0);
    } else if (strcmp(argv[2], "window-session") == 0 ||
               strcmp(argv[2], "window-session-gl-error") == 0 ||
               strcmp(argv[2], "window-statistics-foreign-ready") == 0) {
        CHECK(argc == 4);
        CHECK(statistics_session_window(argv[1], argv[3],
                                        strcmp(argv[2], "window-statistics-foreign-ready") == 0,
                                        strcmp(argv[2], "window-session-gl-error") == 0) == 0);
    } else {
        CHECK(strcmp(argv[2], "window-default-directory") == 0 ||
              strcmp(argv[2], "window-render-engine-limit") == 0 ||
              strcmp(argv[2], "window-render-font-limit") == 0 ||
              strcmp(argv[2], "window-bitmap-font-limit") == 0 ||
              strcmp(argv[2], "window-font-shared-owner") == 0 ||
              strcmp(argv[2], "window-color-lookup-limit") == 0 ||
              strcmp(argv[2], "window-colorizer-limit") == 0 ||
              strcmp(argv[2], "window-entity-map-array-limit") == 0 ||
              strcmp(argv[2], "window-entity-map-image-limit") == 0);
        CHECK(argc == 4);
        CHECK(real_window_prefix(argv[1], argv[3], argv[2]) == 0);
    }
    printf("Actual source startup authored contracts: %u checks\n", checks);
    return 0;
}
