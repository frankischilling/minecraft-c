/* Genuine CPU effects, authored PNG files, real selected Pack and GL context. */
#define MC_TEXTURE_EFFECT_CPU_MAIN mc_effect_upload_unused_cpu_main
#include "test_effects.c"
#undef MC_TEXTURE_EFFECT_CPU_MAIN
#include "../../../src/client/application_state.h"
#include "../../../src/client/display/display.c"
#include "../../../src/client/render_engine/chunk_statistics.h"
#include "../../../src/client/render_engine/effect_upload.h"
#include "../../../src/client/texture_packs/internal.h"
#include <SDL3/SDL_main.h>

typedef struct upload_call {
    int32_t level, x, y, width, height;
    GLuint texture;
    uint8_t pixels[1024];
} upload_call;

static upload_call calls[96];
static size_t call_count;
static mc_client_gl_texture_sub_image_fn actual_sub_image;
static mc_client_gl_get_integer_fn actual_get_integer;

static void GLAPIENTRY observe_upload(GLenum target, GLint level, GLint x, GLint y, GLsizei width,
                                      GLsizei height, GLenum format, GLenum type,
                                      const GLvoid *pixels) {
    actual_sub_image(target, level, x, y, width, height, format, type, pixels);
    if (call_count < sizeof(calls) / sizeof(*calls)) {
        upload_call *call = &calls[call_count++];
        *call = (upload_call){.level = level, .x = x, .y = y, .width = width, .height = height};
        GLint texture = 0;
        actual_get_integer(GL_TEXTURE_BINDING_2D, &texture);
        call->texture = (GLuint)texture;
        if (width >= 0 && height >= 0 && (size_t)width * (size_t)height * 4 <= sizeof(call->pixels))
            memcpy(call->pixels, pixels, (size_t)width * (size_t)height * 4);
    }
}

typedef void(GLAPIENTRY *read_texture_fn)(GLenum, GLint, GLenum, GLenum, GLvoid *);

static bool uploaded_pixels(mc_client_display *display, const upload_call *call, size_t axis) {
    mc_client_display_error e;
    NEED(mc_client_display_source_texture_bind(display, call->texture, &e));
    read_texture_fn read_texture = (read_texture_fn)SDL_GL_GetProcAddress("glGetTexImage");
    NEED(read_texture != NULL && axis <= 256);
    uint8_t *pixels = malloc(axis * axis * 4);
    NEED(pixels != NULL);
    read_texture(GL_TEXTURE_2D, call->level, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    NEED(display->gl.GetError() == GL_NO_ERROR);
    for (int32_t y = 0; y < call->height; ++y)
        NEED(memcmp(pixels + ((size_t)(call->y + y) * axis + (size_t)call->x) * 4,
                    call->pixels + (size_t)y * (size_t)call->width * 4,
                    (size_t)call->width * 4) == 0);
    free(pixels);
    return true;
}

static bool run_upload(fx_effects *f, const char *directory) {
    client_application *app = calloc(1, sizeof(*app));
    mc_client_effect_upload_error *e = calloc(1, sizeof(*e));
    mc_client_effect_registration_error *registration = calloc(1, sizeof(*registration));
    NEED(app != NULL && e != NULL && registration != NULL);
    NEED(mc_client_application_lifetime_register(app, NULL) == MC_CLIENT_APPLICATION_OWNER_OK);
    NEED(mc_game_settings_create_defaults(NULL, &app->settings, NULL) == MC_GAME_SETTINGS_OK);
    mc_client_texture_pack_list *packs = NULL;
    NEED(mc_client_texture_pack_list_allocate(mc_resource_files_source(f->files), NULL, &packs) ==
         MC_TEXTURE_PACK_OK);
    mc_texture_pack_error pack_error;
    const mc_texture_pack_status pack_status =
        mc_client_texture_pack_list_construct(packs, app, directory, &pack_error);
    if (pack_status != MC_TEXTURE_PACK_OK)
        fprintf(stderr, "actual Pack constructor: status%d %s pc%u ik%u\n", (int)pack_status,
                pack_error.operation == NULL ? "" : pack_error.operation, pack_error.source_pc,
                pack_error.ik_pc);
    NEED(pack_status == MC_TEXTURE_PACK_OK);
    mc_client_default_font *font = NULL;
    mc_client_render_engine_classes *classes = NULL;
    mc_client_render_engine *engine = NULL;
    mc_client_display *display = NULL;
    NEED(mc_client_render_engine_classes_allocate_context(NULL, NULL, NULL, &classes) ==
         MC_CLIENT_RENDER_ENGINE_OK);
    NEED(mc_client_default_font_create(32u * 1024u * 1024u, &font) == MC_CLIENT_FONT_OK);
    NEED(mc_client_render_engine_allocate(classes, font, NULL, &engine) ==
         MC_CLIENT_RENDER_ENGINE_OK);
    NEED(mc_client_render_engine_construct(engine, packs, app->settings, NULL) ==
         MC_CLIENT_RENDER_ENGINE_OK);
    mc_client_render_engine_phase phase;
    const mc_client_tessellator *D = NULL;
    int32_t updates = -1;
    mc_client_graphics_error graphics;
    const mc_client_graphics_site fps_read = {"net/minecraft/client/Minecraft", "run", "()V", 589};
    NEED(mc_client_chunk_statistics_inspect(classes, &phase, &D, &updates) &&
         phase == MC_CLIENT_RENDER_ENGINE_RAW && D == NULL);
    NEED(mc_client_chunk_statistics_get(classes, fps_read, &updates, &graphics) && updates == 0);
    NEED(mc_client_chunk_statistics_inspect(classes, &phase, &D, &updates) &&
         phase == MC_CLIENT_RENDER_ENGINE_READY && D == classes->nw.a && D != NULL);
    NEED(mc_client_chunk_statistics_increment(classes, fps_read, &graphics));
    NEED(mc_client_chunk_statistics_increment(classes, fps_read, &graphics));
    NEED(mc_client_chunk_statistics_get(classes, fps_read, &updates, &graphics) && updates == 2);
    classes->dk_b = INT32_MAX; /* Declared genuine source field overflow control. */
    NEED(mc_client_chunk_statistics_increment(classes, fps_read, &graphics));
    NEED(mc_client_chunk_statistics_get(classes, fps_read, &updates, &graphics) &&
         updates == INT32_MIN);
    NEED(mc_client_chunk_statistics_reset(classes, fps_read, &graphics));
    NEED(mc_client_chunk_statistics_get(classes, fps_read, &updates, &graphics) && updates == 0);
    NEED(mc_client_display_create("Actual dynamic texture uploads", 64, 64, true, &display,
                                  &e->display));
    actual_sub_image = display->gl.TexSubImage2D;
    actual_get_integer = display->gl.GetIntegerv;
    display->gl.TexSubImage2D = observe_upload;
    mc_client_texture_effect *portal = NULL, *flow = NULL;
    const mc_texture_effect_services service = services(f);
    NEED(mc_client_texture_effect_allocate(&service, MC_TEXTURE_EFFECT_PORTAL, NULL, &portal,
                                           &e->child) == MC_TEXTURE_EFFECT_OK);
    NEED(mc_client_texture_effect_construct(portal, NULL, 0, &e->child) == MC_TEXTURE_EFFECT_OK);
    NEED(mc_client_texture_effect_allocate(&service, MC_TEXTURE_EFFECT_FLOWING_WATER, NULL, &flow,
                                           &e->child) == MC_TEXTURE_EFFECT_OK);
    NEED(mc_client_texture_effect_construct(flow, NULL, 0, &e->child) == MC_TEXTURE_EFFECT_OK);
    NEED(mc_client_render_engine_register_effect(engine, portal, 0, registration));
    NEED(mc_client_render_engine_register_effect(engine, flow, 0, registration));
    NEED(mc_client_render_engine_register_effect(engine, portal, 0, registration));
    mc_client_argb_image *image = NULL;
    mc_client_texture separate = 0;
    NEED(mc_client_argb_image_create(16, 16, 4096, &image) == MC_CLIENT_RENDER_ENGINE_OK);
    NEED(mc_client_render_engine_register_image(engine, display, NULL, image, &separate, NULL) ==
         MC_CLIENT_RENDER_TEXTURE_OK);
    /* Declared source fields on the genuine existing allocation. */
    ((mc_texture_effect_owner *)portal)->fields.d = (int32_t)separate;
    mc_game_settings_state settings;
    NEED(mc_game_settings_read_state(app->settings, &settings));
    settings.values.anaglyph = true;
    NEED(mc_game_settings_set_values(app->settings, &settings.values, NULL) == MC_GAME_SETTINGS_OK);
    call_count = 0;
    NEED(mc_client_render_engine_update_effects(engine, display, e));
    NEED(e->source_returned && e->ji_pc == 789 && e->updated_occurrences == 3 &&
         e->atlas_uploads == 6 && e->separate_uploads == 2 && call_count == 8);
    mc_texture_effect_state state;
    NEED(mc_client_texture_effect_inspect(portal, &state) && state.counter == 4 && state.fields.c &&
         state.native_holds == 3);
    NEED(memcmp(calls[0].pixels, calls[5].pixels, 1024) != 0);
    NEED(calls[0].texture == calls[1].texture && calls[5].texture == calls[0].texture &&
         calls[6].texture == separate && calls[7].texture == separate &&
         memcmp(calls[6].pixels, state.fields.a, 1024) == 0 &&
         memcmp(calls[7].pixels, state.fields.a, 1024) == 0);
    NEED(calls[1].x == calls[2].x && calls[2].y == calls[1].y + 16 &&
         calls[3].x == calls[1].x + 16 && calls[3].y == calls[1].y && calls[4].x == calls[3].x &&
         calls[4].y == calls[2].y);
    NEED(uploaded_pixels(display, &calls[5], 256) && uploaded_pixels(display, &calls[7], 16));
    for (size_t n = 1; n <= 4; ++n)
        NEED(uploaded_pixels(display, &calls[n], 256));
    NEED(engine->fields.f->position == 0 && engine->fields.f->limit == 1024 &&
         memcmp(engine->fields.f->data, state.fields.a, 1024) == 0);

    /* A pending real driver error survives source bind and subimage calls. */
    display->gl.BindTexture(0, 0);
    NEED(mc_client_display_source_texture_bind(display, separate, &e->display));
    NEED(mc_client_display_source_texture_sub_image(display, 0, 0, 0, 16, 16, state.fields.a,
                                                    state.fields.a_length, &e->display));
    NEED(display->gl.GetError() == GL_INVALID_ENUM && display->gl.GetError() == GL_NO_ERROR);
    display->gl.PixelStorei(GL_UNPACK_ROW_LENGTH, 19);
    const size_t before = call_count;
    NEED(!mc_client_display_source_texture_sub_image(display, 0, 0, 0, 16, 16, state.fields.a,
                                                     state.fields.a_length, &e->display));
    NEED(e->display.status == MC_CLIENT_DISPLAY_UNSUPPORTED_GL && call_count == before);
    display->gl.PixelStorei(GL_UNPACK_ROW_LENGTH, 0);

    /* The List retains NULL and earlier uploads remain when its store fails. */
    NEED(!mc_client_render_engine_register_effect(engine, NULL, 0, registration) &&
         registration->appended);
    engine->fields.i = NULL;
    call_count = 0;
    NEED(!mc_client_render_engine_update_effects(engine, display, e) && e->ji_pc == 34 &&
         e->updated_occurrences == 0);
    engine->fields.i = app->settings;
    NEED(!mc_client_render_engine_update_effects(engine, display, e) && e->ji_pc == 37 &&
         e->occurrence == 3 && e->updated_occurrences == 3 && e->atlas_uploads == 6 &&
         e->separate_uploads == 0 && !e->second_pass && call_count == 6);
    NEED(engine->active_resource_calls == 0 && classes->active_resource_calls == 0 &&
         engine->holds == 1);
    NEED(mc_client_render_engine_retire_effects(engine, registration));
    NEED(!mc_client_render_engine_update_effects(engine, display, e) &&
         e->status == MC_CLIENT_EFFECT_UPLOAD_INVALID_PHASE);
    NEED(mc_client_texture_effect_release(&portal, &e->child) == MC_TEXTURE_EFFECT_OK);
    NEED(mc_client_texture_effect_release(&flow, &e->child) == MC_TEXTURE_EFFECT_OK);
    mc_client_argb_image_release(image);
    NEED(mc_client_render_engine_retire_lookup(engine, &e->lookup) == MC_CLIENT_LOOKUP_OK);
    NEED(mc_client_texture_lookup_free_metadata(engine));
    NEED(mc_client_render_engine_release(engine) == MC_CLIENT_RENDER_ENGINE_OK);
    NEED(mc_client_render_engine_retire_resources(classes, NULL) == MC_CLIENT_RENDER_TEXTURE_OK);
    NEED(mc_client_render_engine_classes_release(classes) == MC_CLIENT_RENDER_ENGINE_OK);
    NEED(mc_client_default_font_release(font) == MC_CLIENT_FONT_OK);
    NEED(mc_client_display_destroy(display, &e->display));
    NEED(mc_client_texture_pack_list_release(packs, NULL) == MC_TEXTURE_PACK_OK);
    NEED(mc_game_settings_retire(&app->settings, NULL) == MC_GAME_SETTINGS_OK);
    NEED(mc_client_application_lifetime_retire(app, NULL) == MC_CLIENT_APPLICATION_OWNER_OK);
    free(app);
    free(registration);
    free(e);
    return true;
}

int main(int argc, char **argv) {
    if (argc != 3)
        return 2;
    fx_effects f = {0};
    if (!prepare(&f, argv[1]))
        return 1;
    char path[4096];
    if (!fx_path(path, sizeof(path), argv[1], "pack.png") || !fx_png(path, 16, 16) ||
        !fx_path(path, sizeof(path), argv[1], "terrain.png") || !fx_png(path, 256, 256) ||
        !run_upload(&f, argv[2]))
        return 1;
    if (mc_registry_classes_free(f.registry, NULL) != MC_REGISTRY_OK ||
        mc_resource_files_free(f.files) != MC_REGISTRY_OK)
        return 1;
    mc_java_random_profile_free(f.profile);
    mc_materials_destroy(f.materials);
    free(f.trig);
    printf("Actual dynamic texture uploads: %u\n", checks);
    return 0;
}
