/* Real prepared App/World/DN and genuine PB/XW/SN owners. Constructors use
 * their actual independent default entropy; observations never feed execution. */
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#endif
int mc_item_particle_startup_fixture_main(int argc, char **argv);
#define main mc_item_particle_startup_fixture_main
#include "../client/startup/test_application_startup.c"
#undef main
#include "block/registry/registry_internal.h"
#include "block/registry/runtime_internal.h"
#include "client/display/camera_surface.h"
#include "client/display/fog_surface.h"
#include "client/display/world_frame_surface.h"
#include "client/particles/effect_renderer_internal.h"
#include "client/particles/render_internal.h"
#include "client/render_engine/internal.h"
#include "client/render_engine/nw.h"
#include "client/render_engine/texture_lookup.h"
#include "client/texture_packs/internal.h"
#include "client/world/render_global_access.h"
#include "client/world/render_global_internal.h"
#include "core/java/random_profile_internal.h"
#include "entity/mob/internal.h"
#include "entity/particles/particle_internal.h"
#include "mc/client/particles/render.h"
#include "mc/entity/mob/tick.h"
#include "mc/item/registry/runtime.h"
#include "mc/world/runtime/presentation.h"
#include "world/runtime/chunk_provider_internal.h"
#include "world/runtime/runtime_internal.h"
#include "world/runtime/terminal_inventory_internal.h"
#include "world/storage/objects_internal.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <inttypes.h>
#include <math.h>
#define MOB_COUNT(a) (sizeof(a) / sizeof((a)[0]))

typedef struct ip_fixture {
    fixture runtime;
    client_application *app;
    mc_client_display *display;
    mc_client_default_font *font;
    mc_client_texture_pack_list *packs;
    mc_resource_files *textures;
    mc_math_classes *math;
    mc_world_runtime_classes *world_classes;
    mc_world_regions *regions;
    mc_world_store *store;
    mc_world_info_name *name;
    mc_world_provider *dimension;
    mc_world_runtime *world;
    mc_chunk_provider_fields provider;
    mc_chunk_state *chunks[25];
    mc_entity *camera, *particle, *slime;
    /* Actual caller occurrences matching the authored helper's `all` List. */
    mc_entity *contexts[32];
    size_t context_count;
    const mc_item *item;
    const mc_block *snow;
    mc_client_effect_renderer *effects;
    mc_client_effect_draw_call *call;
    mc_client_render_global *renderer;
    mc_client_cx_classes *cx_classes;
    mc_render_global_access *access;
    mc_client_render_global_error *global_error;
    mc_world_mutation_error *presentation_error;
    mc_mob_tick_error *mob_error;
    mc_particle_error *particle_error;
    mc_item_particle_tick_error *tick_error;
    mc_effect_renderer_error *effects_error;
    mc_effect_draw_error *draw_error;
    mc_client_texture_lookup_error *lookup_error;
    FILE *output;
} ip_fixture;

static uint32_t ip_fbits(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static uint64_t ip_dbits(double value) {
    uint64_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static const char *ip_label(const char *value) {
    return value ? value : "?";
}

static int ip_keep(ip_fixture *f, mc_entity *particle) {
    CHECK(f->context_count < MOB_COUNT(f->contexts));
    for (size_t i = 0; i < f->context_count; ++i)
        CHECK(f->contexts[i] != particle);
    CHECK(mc_entity_retain(particle) == MC_ENTITY_OK);
    f->contexts[f->context_count++] = particle;
    return 0;
}

static void ip_end_contexts(ip_fixture *f) {
    while (f->context_count != 0) {
        mc_entity **slot = &f->contexts[--f->context_count];
        mc_entity_release(*slot);
        *slot = NULL;
    }
}

static void ip_ref(FILE *out, const void *pointer, const char *class_name) {
    if (pointer == NULL)
        fputs("null", out);
    else
        fprintf(out, "{\"class\":\"%s\",\"identity\":\"%016" PRIxPTR "\"}", class_name,
                (uintptr_t)pointer);
}

static void ip_string(FILE *out, const char *value) {
    fputc('"', out);
    for (const unsigned char *p = (const unsigned char *)value; *p; ++p) {
        if (*p == '"' || *p == '\\')
            fputc('\\', out);
        if (*p < 32)
            fprintf(out, "\\u%04x", (unsigned int)*p);
        else
            fputc(*p, out);
    }
    fputc('"', out);
}

static void ip_float(FILE *out, float value) {
    fprintf(out, "{\"F\":\"%08" PRIx32 "\"}", ip_fbits(value));
}

static void ip_double(FILE *out, double value) {
    fprintf(out, "{\"D\":\"%016" PRIx64 "\"}", ip_dbits(value));
}

static void ip_field_ref(FILE *out, bool *first, const char *name, const void *pointer,
                         const char *class_name) {
    fprintf(out, "%s\"%s\":", *first ? "" : ",", name);
    *first = false;
    ip_ref(out, pointer, class_name);
}

static void ip_field_float(FILE *out, bool *first, const char *name, float value) {
    fprintf(out, "%s\"%s\":", *first ? "" : ",", name);
    *first = false;
    ip_float(out, value);
}

static void ip_field_double(FILE *out, bool *first, const char *name, double value) {
    fprintf(out, "%s\"%s\":", *first ? "" : ",", name);
    *first = false;
    ip_double(out, value);
}

static void ip_field_int(FILE *out, bool *first, const char *name, int32_t value) {
    fprintf(out, "%s\"%s\":%" PRId32, *first ? "" : ",", name, value);
    *first = false;
}

static void ip_field_bool(FILE *out, bool *first, const char *name, bool value) {
    fprintf(out, "%s\"%s\":%s", *first ? "" : ",", name, value ? "true" : "false");
    *first = false;
}

static int ip_world_open(ip_fixture *f, const char *directory) {
    struct fixture *runtime = &f->runtime;
    CHECK(mc_math_classes_create(SIZE_MAX, &f->math, NULL) == MC_MATH_CLASS_OK &&
          mc_math_classes_ensure(f->math, NULL) == MC_MATH_CLASS_OK);
    CHECK(mc_world_runtime_classes_create(MC_WORLD_INFO_CLIENT, runtime->services.profile,
                                          mc_math_classes_table(f->math), &f->world_classes,
                                          NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_classes_bind_registry(f->world_classes, runtime->services.registry,
                                                 NULL) == MC_REGISTRY_OK &&
          mc_world_runtime_classes_bind_entities(f->world_classes, runtime->services.entities) ==
              MC_ENTITY_OK);
    CHECK(mc_world_regions_create(NULL, &f->regions, NULL) == MC_WORLD_STORAGE_OK);
    uint8_t map[256];
    for (size_t i = 0; i < MOB_COUNT(map); ++i)
        map[i] = (uint8_t)i;
    const mc_world_store_options options = {.session_token = 2947,
                                            .block_map = map,
                                            .block_map_length = MOB_COUNT(map),
                                            .limits = mc_world_store_default_limits()};
    CHECK(mc_world_store_open(directory, f->regions, &options, &f->store, NULL) ==
          MC_WORLD_STORAGE_OK);
    const uint16_t title[] = {'M', 'o', 'b', 'D', 'r', 'a', 'w'};
    CHECK(mc_world_info_name_copy((mc_nbt_string){title, MOB_COUNT(title)}, SIZE_MAX, &f->name) ==
          MC_WORLD_INFO_OK);
    CHECK(mc_world_provider_create(0, NULL, &f->dimension, NULL) == MC_WORLD_PROVIDER_OK);
    mc_world_runtime_construction_limits limits = mc_world_runtime_default_construction_limits();
    limits.max_provider_chunks = 25;
    CHECK(mc_world_runtime_allocate_world(MC_WORLD_RUNTIME_CLIENT_DIRECT_METADATA, f->world_classes,
                                          &limits, &f->world, NULL) == MC_WORLD_STORAGE_OK);
    const mc_world_runtime_world_arguments arguments = {
        .store = f->store, .name = f->name, .seed = 23, .dimension = f->dimension};
    CHECK(mc_world_runtime_construct_world(f->world, &arguments, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_chunk_provider_bind_fields(f->world->fields.v, &f->provider, NULL) ==
          MC_WORLD_STORAGE_OK);
    size_t index = 0;
    for (int32_t x = -2; x <= 2; ++x)
        for (int32_t z = -2; z <= 2; ++z) {
            mc_chunk_state *chunk = NULL;
            CHECK(mc_world_store_load_game_chunk(f->store, f->world, 0, x, z, &chunk, NULL) ==
                      MC_WORLD_STORAGE_OK &&
                  chunk != NULL && !chunk->blank && chunk->terrain_populated);
            f->chunks[index++] = chunk;
            memset(chunk->data->skylight, 255, sizeof(chunk->data->skylight));
            CHECK(mc_chunk_cache_publish(f->provider.cache, x, z, chunk));
        }
    CHECK(index == 25 && f->provider.cache->count == 25);
    CHECK(mc_world_info_time_set(f->world->fields.x, 6000) == MC_WORLD_INFO_OK &&
          mc_world_runtime_initialize_environment(f->world, NULL) == MC_WORLD_STORAGE_OK);
    f->app->active_world = f->world;
    return 0;
}

static int ip_construct(ip_fixture *f, mc_entity_class_kind kind, mc_entity **out) {
    const mc_entity_class_literal *literal = NULL;
    CHECK(mc_entity_class_literal_get(f->runtime.services.entities, kind, &literal) ==
          MC_ENTITY_OK);
    mc_mob_error error;
    CHECK(mc_mob_allocate(f->runtime.services.entities, f->runtime.services.registry, literal, out,
                          &error) == MC_MOB_OK);
    CHECK(mc_mob_construct(*out, f->world, &error) == MC_MOB_OK);
    return 0;
}

static int ip_pose(mc_entity *entity, double x, double y, double z, float yaw, float pitch) {
    /* Declared SN.c-derived operands, then actual SN.e recomputes the real AABB.
     * SN.c leaves previous angles aU/aV untouched. This fixture does not claim
     * an additional native SN.c body qualification. */
    entity->base.aJ = entity->base.bl = x;
    entity->base.aK = entity->base.bm = y;
    entity->base.aL = entity->base.bn = z;
    entity->base.yaw = yaw;
    entity->base.aT = pitch;
    CHECK(mc_entity_set_position(entity, x, y, z) == MC_ENTITY_OK);
    return 0;
}

static int ip_open(ip_fixture *f, const char *resources, const char *textures,
                   const char *directory) {
    CHECK(fixture_open(&f->runtime, resources) == 0);
    CHECK(mc_client_application_constructor_allocate(&f->runtime.services, NULL, &f->app,
                                                     f->runtime.error) == MC_CLIENT_APP_OK &&
          f->app->constructor_phase == MC_CLIENT_APP_RAW && f->app->source_namespace_borrowed);
    /* Matching the original reflective caller input, NOT constructor a264. */
    CHECK(application_static_hold(f->app, NULL) == MC_CLIENT_APPLICATION_OWNER_OK);
    f->runtime.services.application_classes->a = f->app;
    CHECK(atomic_load(&f->app->source_static_holds) == 1 &&
          f->app->constructor_phase == MC_CLIENT_APP_RAW);
    CHECK(mc_registry_classes_initialize_statistics(f->runtime.services.registry, NULL) ==
          MC_REGISTRY_OK);
    CHECK(mc_game_settings_create_defaults(NULL, &f->app->settings, NULL) == MC_GAME_SETTINGS_OK);
    mc_game_settings_state settings;
    CHECK(mc_game_settings_read_state(f->app->settings, &settings) &&
          settings.values.fancy_graphics && !settings.values.show_debug_info);
    CHECK(mc_resource_files_create(textures, &f->textures) == MC_REGISTRY_OK);
    CHECK(mc_client_texture_pack_list_allocate(mc_resource_files_source(f->textures), NULL,
                                               &f->packs) == MC_TEXTURE_PACK_OK &&
          mc_client_texture_pack_list_construct(f->packs, f->app, directory, NULL) ==
              MC_TEXTURE_PACK_OK);
    f->app->D = f->packs;
    CHECK(mc_client_default_font_create(32u * 1024u * 1024u, &f->font) == MC_CLIENT_FONT_OK &&
          mc_client_render_engine_allocate(f->runtime.graphics, f->font, NULL,
                                           &f->app->render_engine) == MC_CLIENT_RENDER_ENGINE_OK &&
          mc_client_render_engine_construct(f->app->render_engine, f->packs, f->app->settings,
                                            NULL) == MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(mc_client_display_create("Genuine item particle", 256, 256, true, &f->display, NULL));
    f->app->display = f->display;
    f->lookup_error = calloc(1, sizeof(*f->lookup_error));
    f->draw_error = calloc(1, sizeof(*f->draw_error));
    CHECK(f->lookup_error && f->draw_error && ip_world_open(f, directory) == 0);
    return 0;
}

static mc_texture_pack_text ip_text(const char *text, uint16_t units[128]) {
    const size_t length = strlen(text);
    if (length > 128)
        abort();
    for (size_t i = 0; i < length; ++i)
        units[i] = (uint16_t)(unsigned char)text[i];
    return (mc_texture_pack_text){units, length, false};
}

typedef GLboolean(APIENTRY *ip_enabled_fn)(GLenum);
typedef void(APIENTRY *ip_boolean_fn)(GLenum, GLboolean *);
typedef void(APIENTRY *ip_integer_fn)(GLenum, GLint *);
typedef void(APIENTRY *ip_read_fn)(GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void *);
typedef void(APIENTRY *ip_finish_fn)(void);

static int ip_emit_gl(ip_fixture *f) {
    mc_client_display *display = f->display;
    float color[4], projection[16], modelview[16], normal[3];
    mc_client_source_fog_state fog;
    CHECK(mc_client_display_source_fog_inspect(display, &fog));
    const bool color_known = fog.current_color_known;
    /* A color-array draw leaves the legacy current color unspecified. The core
     * context cannot query GL_CURRENT_COLOR; retain unknown rather than invent
     * a value or substitute a different physical attribute. PB/DN never read it. */
    if (color_known)
        CHECK(
            mc_client_display_source_current_attribute(display, GL_CURRENT_COLOR, color, 4, NULL));
    CHECK(mc_client_display_camera_matrices_inspect(display, projection, modelview));
    CHECK(mc_client_display_source_current_attribute(display, GL_CURRENT_NORMAL, normal, 3, NULL));
    int32_t mode;
    size_t depth, projection_depth;
    CHECK(mc_client_display_camera_stack_inspect(display, &mode, &depth, &projection_depth));
    mc_client_source_draw_state draw;
    CHECK(mc_client_display_source_draw_inspect(display, &draw) &&
          mc_client_display_source_fog_inspect(display, &fog));
    CHECK(fog.current_color_known == color_known);
    ip_enabled_fn enabled = (ip_enabled_fn)SDL_GL_GetProcAddress("glIsEnabled");
    ip_boolean_fn boolean = (ip_boolean_fn)SDL_GL_GetProcAddress("glGetBooleanv");
    ip_integer_fn integer = (ip_integer_fn)SDL_GL_GetProcAddress("glGetIntegerv");
    CHECK(enabled && boolean && integer);
    GLboolean mask = GL_FALSE;
    GLint function = 0, source = 0, destination = 0, texture = 0;
    boolean(GL_DEPTH_WRITEMASK, &mask);
    integer(GL_DEPTH_FUNC, &function);
    integer(GL_BLEND_SRC_RGB, &source);
    integer(GL_BLEND_DST_RGB, &destination);
    integer(GL_TEXTURE_BINDING_2D, &texture);
    FILE *out = f->output;
    fprintf(out, "{\"color_known\":%s,\"color\":", color_known ? "true" : "false");
    if (color_known) {
        fputc('[', out);
        for (size_t i = 0; i < 4; ++i)
            fprintf(out, "%s\"%08" PRIx32 "\"", i ? "," : "", ip_fbits(color[i]));
        fputc(']', out);
    } else
        fputs("null", out);
    fputs(",\"normal\":[", out);
    for (size_t i = 0; i < 3; ++i)
        fprintf(out, "%s\"%08" PRIx32 "\"", i ? "," : "", ip_fbits(normal[i]));
    fputs("],\"modelview\":[", out);
    for (size_t i = 0; i < 16; ++i)
        fprintf(out, "%s\"%08" PRIx32 "\"", i ? "," : "", ip_fbits(modelview[i]));
    fputs("],\"projection\":[", out);
    for (size_t i = 0; i < 16; ++i)
        fprintf(out, "%s\"%08" PRIx32 "\"", i ? "," : "", ip_fbits(projection[i]));
    fprintf(out,
            "],\"mode\":%d,\"modelview_depth\":%zu,\"projection_depth\":%zu,\"depth_mask\":%s,"
            "\"depth_function\":%d,\"blend_src\":%d,\"blend_dst\":%d,\"texture\":%d,\"caps\":[",
            mode, depth, projection_depth, mask ? "true" : "false", function, source, destination,
            texture);
    const int32_t caps[] = {GL_CULL_FACE, GL_BLEND,     GL_ALPHA_TEST, GL_DEPTH_TEST, GL_LIGHTING,
                            32826,        GL_NORMALIZE, GL_TEXTURE_2D, GL_FOG};
    const bool values[] = {
        enabled(GL_CULL_FACE) != 0,  enabled(GL_BLEND) != 0, draw.alpha_test,
        enabled(GL_DEPTH_TEST) != 0, fog.lighting_enabled,   fog.rescale_normal_enabled,
        fog.normalize_enabled,       draw.texture_enabled,   fog.enabled};
    for (size_t i = 0; i < MOB_COUNT(caps); ++i)
        fprintf(out, "%s{\"cap\":%d,\"enabled\":%s}", i ? "," : "", caps[i],
                values[i] ? "true" : "false");
    fputs("]}", out);
    return 0;
}

static int ip_frame(ip_fixture *f) {
    mc_client_display *d = f->display;
    CHECK(mc_client_display_source_viewport(d, 256, 256, NULL));
    CHECK(mc_client_display_source_matrix_mode(d, GL_PROJECTION, NULL) &&
          mc_client_display_source_load_identity(d, NULL) &&
          mc_client_display_source_ortho(d, -2, 2, -1.5, 2.5, .1, 50, NULL));
    CHECK(mc_client_display_source_matrix_mode(d, GL_MODELVIEW, NULL) &&
          mc_client_display_source_load_identity(d, NULL) &&
          mc_client_display_source_translate(d, 0, -.25f, -6, NULL) &&
          mc_client_display_source_rotate(d, 18, 1, 0, 0, NULL) &&
          mc_client_display_source_rotate(d, -25, 0, 1, 0, NULL));
    CHECK(mc_client_display_source_clear_color(d, .1f, .15f, .2f, 1, NULL) &&
          mc_client_display_source_clear(d, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT, NULL) &&
          mc_client_display_source_depth_test(d, true, NULL) &&
          mc_client_display_source_depth_function(d, GL_LEQUAL, NULL) &&
          mc_client_display_source_depth_mask(d, true, NULL));
    CHECK(mc_client_display_source_lighting_enable(d, false, NULL) &&
          mc_client_display_source_fog_enable(d, false, NULL) &&
          mc_client_display_source_blend_enable(d, false, NULL) &&
          mc_client_display_source_cull_test(d, false, NULL) &&
          mc_client_display_source_texture_enable(d, false, NULL) &&
          mc_client_display_source_color(d, .7f, .7f, .7f, 1, NULL));
    CHECK(mc_client_display_source_texture_enable(d, true, NULL) &&
          mc_client_display_source_alpha_test(d, true, NULL) &&
          mc_client_display_source_alpha_function(d, GL_GREATER, .1f, NULL) &&
          mc_client_display_source_rescale_normal(d, false, NULL) &&
          mc_client_display_source_normalize(d, false, NULL) &&
          mc_client_display_source_color(d, .125f, .25f, .5f, .75f, NULL) &&
          mc_client_display_source_normal(d, .125f, .25f, .5f, NULL));
    return 0;
}

static int ip_pixels(ip_fixture *f, const char *directory, unsigned int index, bool baseline) {
    CHECK(f->display != NULL);
    char path[4096];
    CHECK(snprintf(path, sizeof(path), "%s/%s%u.rgba", directory, baseline ? "baseline" : "draw",
                   index) > 0);
    ip_read_fn read = (ip_read_fn)SDL_GL_GetProcAddress("glReadPixels");
    ip_finish_fn finish = (ip_finish_fn)SDL_GL_GetProcAddress("glFinish");
    CHECK(read && finish);
    uint8_t *pixels = malloc(256u * 256u * 4u);
    CHECK(pixels != NULL);
    finish();
    read(0, 0, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    FILE *file = fopen(path, "wb");
    CHECK(file && fwrite(pixels, 1, 256u * 256u * 4u, file) == 256u * 256u * 4u &&
          fclose(file) == 0);
    free(pixels);
    return 0;
}

static void ip_words(FILE *out, const uint8_t *bytes, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        uint32_t word;
        memcpy(&word, bytes + i * sizeof(word), sizeof(word));
        fprintf(out, "%s\"%08" PRIx32 "\"", i ? "," : "", word);
    }
}

static void ip_field_string(FILE *out, bool *first, const char *name,
                            const mc_player_string *text) {
    fprintf(out, "%s\"%s\":", *first ? "" : ",", name);
    *first = false;
    if (text == NULL) {
        fputs("null", out);
        return;
    }
    mc_player_string_view view;
    if (!mc_player_string_read(text, &view))
        abort();
    fputs("{\"actual\":", out);
    ip_ref(out, text, "java.lang.String");
    fputs(",\"utf16\":[", out);
    for (size_t i = 0; i < view.length; ++i)
        fprintf(out, "%s%u", i ? "," : "", (unsigned int)view.units[i]);
    fputs("]}", out);
}

static uint64_t ip_math_state(const ip_fixture *f) {
    if (f->runtime.services.profile->math == NULL)
        abort();
    return atomic_load_explicit(&f->runtime.services.profile->math->state, memory_order_relaxed);
}

static int ip_globals(ip_fixture *f) {
    FILE *out = f->output;
    mc_particle_origin origin;
    mc_entity_class_snapshot entities;
    mc_entity_vector_snapshot vectors;
    CHECK(mc_entity_particle_origin_read(f->runtime.services.entities, &origin, NULL) ==
              MC_ENTITY_OK &&
          mc_entity_classes_get_snapshot(f->runtime.services.entities, &entities) == MC_ENTITY_OK &&
          mc_entity_vector_get_snapshot(f->runtime.services.entities, &vectors) == MC_ENTITY_OK);
    fputs("{\"world\":", out);
    ip_ref(out, f->world, "fd");
    fputs(",\"camera\":", out);
    ip_ref(out, f->camera, "bx");
    fputs(",\"renderer\":", out);
    ip_ref(out, f->renderer, "n");
    fputs(",\"renderer_world\":", out);
    ip_ref(out, f->renderer->fields.k, "fd");
    fputs(",\"app_dn\":", out);
    ip_ref(out, f->app->effect_renderer, "dn");
    fputs(",\"origins\":[", out);
    ip_double(out, origin.l);
    fputc(',', out);
    ip_double(out, origin.m);
    fputc(',', out);
    ip_double(out, origin.n);
    fprintf(out,
            "],\"box_cursor\":%" PRId32 ",\"vector_cursor\":%" PRId32
            ",\"world_access_count\":%zu,\"effects\":{\"actual\":",
            entities.bounds_pool_cursor, vectors.cursor, f->world->world_accesses.count);
    ip_ref(out, f->effects, "dn");
    fputs(",\"world\":", out);
    ip_ref(out, f->effects->world, "fd");
    fputs(",\"engine\":", out);
    ip_ref(out, f->effects->render_engine, "ji");
    fputs(",\"random\":", out);
    ip_ref(out, f->effects->random, "java.util.Random");
    fprintf(out,
            ",\"random_seed\":\"%012" PRIx64 "\",\"array_identity\":\"%016" PRIxPTR
            "\",\"lists\":[",
            f->effects->random->state, (uintptr_t)f->effects->layers);
    for (size_t i = 0; i < 4; ++i) {
        if (i)
            fputc(',', out);
        const mc_effect_particle_list *list = f->effects->layers[i];
        fputs("{\"actual\":", out);
        ip_ref(out, list, "java.util.ArrayList");
        fprintf(out, ",\"size\":%zu,\"values\":[", list->size);
        for (size_t j = 0; j < list->size; ++j) {
            if (j)
                fputc(',', out);
            ip_ref(out, list->elements[j], "pb");
        }
        fputs("]}", out);
    }
    fputs("]},\"nw\":{\"actual\":", out);
    mc_client_nw_state nw;
    CHECK(mc_client_nw_inspect(f->runtime.graphics, &nw) && nw.a && nw.fields.e && nw.fields.g);
    ip_ref(out, nw.a, "nw");
    fputs(",\"fields\":{", out);
    bool first = true;
#define IP_NW_INT(member) ip_field_int(out, &first, "nw." #member, nw.fields.member)
#define IP_NW_BOOL(member) ip_field_bool(out, &first, "nw." #member, nw.fields.member)
#define IP_NW_DOUBLE(member) ip_field_double(out, &first, "nw." #member, nw.fields.member)
    IP_NW_INT(h);
    IP_NW_DOUBLE(i);
    IP_NW_DOUBLE(j);
    IP_NW_INT(k);
    IP_NW_BOOL(l);
    IP_NW_BOOL(m);
    IP_NW_BOOL(n);
    IP_NW_INT(o);
    IP_NW_INT(p);
    IP_NW_BOOL(q);
    IP_NW_INT(r);
    IP_NW_DOUBLE(s);
    IP_NW_DOUBLE(t);
    IP_NW_DOUBLE(u);
    IP_NW_INT(v);
    IP_NW_BOOL(w);
    IP_NW_BOOL(x);
    IP_NW_INT(z);
    IP_NW_INT(A);
    IP_NW_INT(B);
#undef IP_NW_INT
#undef IP_NW_BOOL
#undef IP_NW_DOUBLE
    /* Raw native pointer identities expose aliases, without claiming a JDK
     * Direct*Buffer implementation class or its private layout. */
    const void *refs[] = {nw.fields.d, nw.fields.e, nw.fields.f, nw.fields.g, nw.fields.y};
    fputs("},\"reference_identities\":[", out);
    for (size_t i = 0; i < MOB_COUNT(refs); ++i) {
        if (i)
            fputc(',', out);
        if (refs[i])
            fprintf(out, "\"%016" PRIxPTR "\"", (uintptr_t)refs[i]);
        else
            fputs("null", out);
    }
    const size_t selected =
        nw.fields.e->position > (size_t)nw.fields.o ? nw.fields.e->position : (size_t)nw.fields.o;
    CHECK(selected < 2048);
    fprintf(out, "],\"position\":%zu,\"selected_words\":[", nw.fields.e->position);
    ip_words(out, (const uint8_t *)nw.fields.g, selected);
    fputs("],\"backing_prefix\":[", out);
    ip_words(out, (const uint8_t *)nw.fields.g, 1024);
    fputs("]}}", out);
    return 0;
}

static int ip_state(ip_fixture *f, unsigned int index, const char *phase, mc_entity *particle) {
    FILE *out = f->output;
    mc_particle_snapshot state;
    CHECK(mc_particle_read(particle, &state));
    fprintf(out, "{\"event\":\"state\",\"case\":%u,\"phase\":", index);
    ip_string(out, phase);
    fputs(",\"actual\":", out);
    ip_ref(out, particle, "pb");
    fputs(",\"fields\":{", out);
    bool first = true;
    ip_field_int(out, &first, "sn.aD", particle->base.id);
    ip_field_double(out, &first, "sn.aE", particle->base.aE);
    ip_field_bool(out, &first, "sn.aF", particle->base.aF);
    ip_field_ref(out, &first, "sn.aG", particle->base.aG, "sn");
    ip_field_ref(out, &first, "sn.aH", particle->base.aH, "sn");
    ip_field_ref(out, &first, "sn.aI", particle->base.world, "fd");
    ip_field_double(out, &first, "sn.aJ", particle->base.aJ);
    ip_field_double(out, &first, "sn.aK", particle->base.aK);
    ip_field_double(out, &first, "sn.aL", particle->base.aL);
    ip_field_double(out, &first, "sn.aM", particle->base.x);
    ip_field_double(out, &first, "sn.aN", particle->base.y);
    ip_field_double(out, &first, "sn.aO", particle->base.z);
    ip_field_double(out, &first, "sn.aP", particle->base.velocity_x);
    ip_field_double(out, &first, "sn.aQ", particle->base.velocity_y);
    ip_field_double(out, &first, "sn.aR", particle->base.velocity_z);
    ip_field_float(out, &first, "sn.aS", particle->base.yaw);
    ip_field_float(out, &first, "sn.aT", particle->base.aT);
    ip_field_float(out, &first, "sn.aU", particle->base.aU);
    ip_field_float(out, &first, "sn.aV", particle->base.aV);
    ip_field_ref(out, &first, "sn.aW", particle->bounds, "eq");
    ip_field_bool(out, &first, "sn.aX", particle->base.aX);
    ip_field_bool(out, &first, "sn.aY", particle->base.aY);
    ip_field_bool(out, &first, "sn.aZ", particle->base.aZ);
    ip_field_bool(out, &first, "sn.ba", particle->base.ba);
    ip_field_bool(out, &first, "sn.bb", particle->base.bb);
    ip_field_bool(out, &first, "sn.bc", particle->base.bc);
    ip_field_bool(out, &first, "sn.bd", particle->base.bd);
    ip_field_bool(out, &first, "sn.be", particle->base.be);
    ip_field_float(out, &first, "sn.bf", particle->base.offset);
    ip_field_float(out, &first, "sn.bg", particle->base.width);
    ip_field_float(out, &first, "sn.bh", particle->base.height);
    ip_field_float(out, &first, "sn.bi", particle->base.bi);
    ip_field_float(out, &first, "sn.bj", particle->base.bj);
    ip_field_float(out, &first, "sn.bk", particle->base.bk);
    ip_field_int(out, &first, "sn.b", particle->base.private_b);
    ip_field_double(out, &first, "sn.bl", particle->base.bl);
    ip_field_double(out, &first, "sn.bm", particle->base.bm);
    ip_field_double(out, &first, "sn.bn", particle->base.bn);
    ip_field_float(out, &first, "sn.bo", particle->base.y_size);
    ip_field_float(out, &first, "sn.bp", particle->base.bp);
    ip_field_bool(out, &first, "sn.bq", particle->base.bq);
    ip_field_float(out, &first, "sn.br", particle->base.br);
    ip_field_ref(out, &first, "sn.bs", particle->random, "java.util.Random");
    ip_field_int(out, &first, "sn.bt", particle->base.bt);
    ip_field_int(out, &first, "sn.bu", particle->base.bu);
    ip_field_int(out, &first, "sn.bv", particle->base.bv);
    ip_field_int(out, &first, "sn.bw", particle->base.bw);
    ip_field_bool(out, &first, "sn.bx", particle->base.bx);
    ip_field_int(out, &first, "sn.by", particle->base.by);
    ip_field_int(out, &first, "sn.bz", particle->base.bz);
    ip_field_bool(out, &first, "sn.c", particle->base.private_c);
    ip_field_string(out, &first, "sn.bA", (const mc_player_string *)particle->base.client_only.bA);
    ip_field_string(out, &first, "sn.bB", particle->base.client_only.bB);
    ip_field_bool(out, &first, "sn.bC", particle->base.bC);
    ip_field_ref(out, &first, "sn.bD", particle->watcher, "ud");
    ip_field_float(out, &first, "sn.bE", particle->base.bE);
    ip_field_double(out, &first, "sn.d", particle->base.private_d);
    ip_field_double(out, &first, "sn.e", particle->base.private_e);
    ip_field_bool(out, &first, "sn.bF", particle->base.bF);
    ip_field_int(out, &first, "sn.bG", particle->base.bG);
    ip_field_int(out, &first, "sn.bH", particle->base.bH);
    ip_field_int(out, &first, "sn.bI", particle->base.bI);
    ip_field_int(out, &first, "sn.bJ", particle->base.client_only.bJ);
    ip_field_int(out, &first, "sn.bK", particle->base.client_only.bK);
    ip_field_int(out, &first, "sn.bL", particle->base.client_only.bL);
    ip_field_bool(out, &first, "sn.bM", particle->base.bM);
    ip_field_int(out, &first, "xw.b", state.particle.b);
    ip_field_float(out, &first, "xw.c", state.particle.c);
    ip_field_float(out, &first, "xw.d", state.particle.d);
    ip_field_int(out, &first, "xw.e", state.particle.e);
    ip_field_int(out, &first, "xw.f", state.particle.f);
    ip_field_float(out, &first, "xw.g", state.particle.g);
    ip_field_float(out, &first, "xw.h", state.particle.h);
    ip_field_float(out, &first, "xw.i", state.particle.i);
    ip_field_float(out, &first, "xw.j", state.particle.j);
    ip_field_float(out, &first, "xw.k", state.particle.k);
    fputs("},\"bounds\":{", out);
    first = true;
    ip_field_double(out, &first, "eq.a", particle->bounds->min_x);
    ip_field_double(out, &first, "eq.b", particle->bounds->min_y);
    ip_field_double(out, &first, "eq.c", particle->bounds->min_z);
    ip_field_double(out, &first, "eq.d", particle->bounds->max_x);
    ip_field_double(out, &first, "eq.e", particle->bounds->max_y);
    ip_field_double(out, &first, "eq.f", particle->bounds->max_z);
    mc_entity_watcher_snapshot watcher;
    CHECK(mc_entity_watcher_get_snapshot(particle->watcher, &watcher) == MC_ENTITY_OK);
    fprintf(out,
            "},\"watcher\":{\"size\":%zu,\"index\":%d,\"byte\":%d,\"dirty\":%s,\"record_dirty\":%s}"
            ",\"streams\":{\"own\":\"%012" PRIx64 "\",\"world\":\"%012" PRIx64
            "\",\"math\":\"%012" PRIx64 "\",\"uniquifier\":\"%016" PRIx64
            "\"},\"native\":{\"phase\":%d,\"ancestor_pc\":%u,\"item_pc\":%u,\"tick_pc\":%u,\"tick_"
            "active\":%s},\"watcher_type\":%d,\"watcher_record\":",
            watcher.size, watcher.index, (int)watcher.byte_value, watcher.dirty ? "true" : "false",
            watcher.record_dirty ? "true" : "false", particle->random->state,
            f->world->fields.r->state, ip_math_state(f),
            atomic_load(&f->runtime.services.profile->uniquifier), (int)state.phase,
            state.particle_pc, state.item_pc, state.tick_pc, state.tick_active ? "true" : "false",
            watcher.type);
    ip_ref(out, watcher.record, "ma");
    fputs(",\"globals\":", out);
    CHECK(ip_globals(f) == 0);
    fputs("}\n", out);
    return 0;
}

static int ip_constructor_receipt(ip_fixture *f, unsigned int index, mc_entity *particle,
                                  uint64_t before, uint64_t world_seed, uint64_t u) {
    const mc_particle_constructor_random *receipt = &f->particle_error->constructor_random;
    CHECK(receipt->math_count == 5 && receipt->own_count == 4 && receipt->own_initial_stored &&
          receipt->own_identity == (uintptr_t)particle->random);
    mc_java_random math = {0};
    math.state = before;
    for (size_t i = 0; i < 5; ++i)
        CHECK(ip_dbits(mc_java_random_next_double(&math)) == ip_dbits(receipt->math_values[i]));
    CHECK(math.state == ip_math_state(f) && world_seed == f->world->fields.r->state &&
          atomic_load(&f->runtime.services.profile->uniquifier) ==
              u * UINT64_C(1181783497276652981));
    mc_java_random own = receipt->own_initial;
    for (size_t i = 0; i < 4; ++i)
        CHECK(ip_fbits(mc_java_random_next_float(&own)) == ip_fbits(receipt->own_values[i]));
    CHECK(own.state == particle->random->state);
    fprintf(f->output,
            "{\"event\":\"constructor_random\",\"case\":%u,\"math_before\":\"%012" PRIx64
            "\",\"math_after\":\"%012" PRIx64 "\",\"own_initial\":\"%012" PRIx64
            "\",\"own_after\":\"%012" PRIx64 "\",\"math_values\":[",
            index, before, math.state, receipt->own_initial.state, own.state);
    for (size_t i = 0; i < 5; ++i) {
        if (i)
            fputc(',', f->output);
        ip_double(f->output, receipt->math_values[i]);
    }
    fputs("],\"own_values\":[", f->output);
    for (size_t i = 0; i < 4; ++i) {
        if (i)
            fputc(',', f->output);
        ip_float(f->output, receipt->own_values[i]);
    }
    fputs("],\"default_entropy_rebound\":false}\n", f->output);
    return 0;
}

static int ip_new(ip_fixture *f, unsigned int index, mc_world_runtime *world, mc_entity **out) {
    const uint64_t before = ip_math_state(f), world_seed = f->world->fields.r->state,
                   u = atomic_load(&f->runtime.services.profile->uniquifier);
    CHECK(mc_item_particle_allocate(f->runtime.services.entities, out, NULL) == MC_ENTITY_OK);
    CHECK(mc_item_particle_construct(*out, world, f->runtime.services.registry, f->math, 8.5, 65.5,
                                     8.5, f->item, f->particle_error) == MC_ENTITY_OK);
    CHECK(ip_constructor_receipt(f, index, *out, before, world_seed, u) == 0);
    mc_particle_snapshot state;
    int32_t layer = -1, icon = -1;
    CHECK(mc_particle_read(*out, &state) && state.phase == MC_ENTITY_READY &&
          state.particle_pc == 306 && state.item_pc == 58 &&
          mc_item_particle_layer(*out, &layer) == MC_ENTITY_OK && layer == 2);
    CHECK(mc_registry_item_base_icon(f->runtime.services.registry, f->item, 0, &icon, NULL) ==
              MC_REGISTRY_OK &&
          state.particle.b == icon &&
          ip_fbits(state.particle.h) == ip_fbits(mc_block_fields_get(f->snow)->bz));
    CHECK(ip_state(f, index, "constructor", *out) == 0);
    CHECK(ip_keep(f, *out) == 0);
    return 0;
}

static int ip_prepare(ip_fixture *f, mc_entity *p, const char *scenario) {
    CHECK(p->classes == f->runtime.services.entities);
    CHECK(mc_entity_set_position(p, 8.5, 66, 8.5) == MC_ENTITY_OK);
    p->base.aJ = 8.25;
    p->base.aK = 65.75;
    p->base.aL = 8.75;
    p->base.velocity_x = .125;
    p->base.velocity_y = .25;
    p->base.velocity_z = -.0625;
    p->base.aX = false;
    p->base.be = false;
    mc_particle_fields *v = &p->particle->fields;
    v->b = 30;
    v->c = .25f;
    v->d = 1.5f;
    v->e = 0;
    v->f = 20;
    v->g = .75f;
    v->h = .5f;
    v->i = .25f;
    v->j = .5f;
    v->k = .75f;
    if (strcmp(scenario, "ground") == 0) {
        CHECK(mc_entity_set_position(p, 8.5, 64.25, 8.5) == MC_ENTITY_OK);
        p->base.aK = 64;
        p->base.velocity_y = -.5;
    }
    if (strcmp(scenario, "expiry_air") == 0)
        v->e = v->f = 4;
    if (strcmp(scenario, "render") == 0)
        CHECK(mc_entity_set_position(p, 8.5, 65.5, 8.5) == MC_ENTITY_OK);
    return 0;
}

static int ip_nw_prepare(ip_fixture *f) {
    mc_client_tessellator *tessellator = NULL;
    mc_client_graphics_error *error = calloc(1, sizeof(*error));
    CHECK(error && mc_client_nw_active_use(
                       f->runtime.graphics,
                       (mc_client_graphics_site){"ItemParticleFixture", "prepare", "()V", -1},
                       &tessellator, error));
    mc_client_nw_state nw;
    CHECK(mc_client_nw_inspect(f->runtime.graphics, &nw) && nw.fields.g && nw.fields.e &&
          nw.fields.B >= 1024);
    /* The genuine empty batch precedes caller backing preparation ONCE. The
     * later actual DN begin resets its normal-array flag; PB sets no normal. */
    CHECK(mc_client_nw_begin_quads(tessellator, error) &&
          mc_client_nw_normal(tessellator, .25f, -.5f, .75f, error) &&
          mc_client_nw_draw(tessellator, f->display, error));
    memset((void *)nw.fields.g, 0, (size_t)nw.fields.B * sizeof(*nw.fields.g));
    for (size_t i = 0; i < 128; ++i) {
        ((uint32_t *)(void *)nw.fields.g)[i * 8 + 6] = UINT32_C(0x10203000) + (uint32_t)i;
        ((uint32_t *)(void *)nw.fields.g)[i * 8 + 7] = UINT32_C(0x20304000) + (uint32_t)i;
    }
    ((mc_client_direct_int_buffer *)(void *)nw.fields.e)->position = 0;
    free(error);
    return 0;
}

static int ip_n_open(ip_fixture *f) {
    f->global_error = calloc(1, sizeof(*f->global_error));
    f->presentation_error = calloc(1, sizeof(*f->presentation_error));
    f->mob_error = calloc(1, sizeof(*f->mob_error));
    CHECK(f->global_error && f->presentation_error && f->mob_error);
    /* The bootstrap's actual borrowed native service association; its owner
     * remains live through N callbacks and checked renderer retirement. */
    f->app->source_math_classes = f->math;
    CHECK(mc_client_cx_classes_create(&f->cx_classes, NULL) == MC_CLIENT_CX_OK &&
          mc_client_cx_allocate(f->cx_classes, &f->app->S, NULL) == MC_CLIENT_CX_OK &&
          mc_client_cx_construct(f->app->S, NULL) == MC_CLIENT_CX_OK);
    CHECK(mc_client_render_global_allocate(f->runtime.graphics, NULL, &f->renderer,
                                           f->global_error) == MC_CLIENT_RENDER_GLOBAL_OK &&
          mc_client_render_global_construct(f->renderer, f->app, f->app->render_engine, f->display,
                                            f->global_error) == MC_CLIENT_RENDER_GLOBAL_OK);
    f->app->g = f->renderer;
    CHECK(f->renderer->phase == MC_CLIENT_RENDER_GLOBAL_READY &&
          f->renderer->world_occurrence == NULL && f->renderer->fields.k == NULL);
    /* Original-equivalent prepared Source field. This does NOT execute N.a(fd)
     * or install an owning native World slot. The fixture owns this live World. */
    f->renderer->fields.k = f->world;
    CHECK(mc_render_global_access_create(f->renderer, &f->access, f->global_error) ==
              MC_CLIENT_RENDER_GLOBAL_OK &&
          mc_render_global_access_source_add(f->access, f->world, f->global_error) ==
              MC_CLIENT_RENDER_GLOBAL_OK);
    mc_render_global_access_state state;
    CHECK(mc_render_global_access_inspect(f->access, &state) && state.registrations == 1 &&
          state.active_callbacks == 0 && state.retained_failures == 0 &&
          f->world->world_accesses.count == 1 && f->renderer->world_occurrence == NULL);
    return 0;
}

static int ip_emit(ip_fixture *f, unsigned int index, bool null_dn) {
    const mc_registry_string *name = NULL;
    CHECK(mc_registry_runtime_literal_ascii(f->runtime.services.registry, "slime", &name, NULL) ==
          MC_REGISTRY_OK);
    const uint64_t math = ip_math_state(f), world = f->world->fields.r->state,
                   u = atomic_load(&f->runtime.services.profile->uniquifier);
    const size_t count = f->effects->layers[2]->size;
    if (null_dn) {
        CHECK(ip_state(f, index, "before", f->particle) == 0);
        f->app->effect_renderer = NULL; /* Declared next caller operand. */
    } else {
        fprintf(f->output,
                "{\"event\":\"context\",\"case\":%u,\"phase\":\"before\",\"globals\":", index);
        CHECK(ip_globals(f) == 0);
        fputs("}\n", f->output);
    }
    CHECK(mc_world_runtime_particle(f->world, name, 8.5, 65.5, 8.5, .75, -.5, .25,
                                    f->presentation_error) == MC_WORLD_STORAGE_OK);
    mc_render_global_access_state state;
    CHECK(mc_render_global_access_inspect(f->access, &state) && state.active_callbacks == 0 &&
          state.retained_failures == 0 && f->renderer->fields.k == f->world &&
          f->renderer->world_occurrence == NULL && f->world->fields.r->state == world);
    if (null_dn) {
        CHECK(f->app->effect_renderer == NULL && f->effects->layers[2]->size == count &&
              ip_math_state(f) == math &&
              atomic_load(&f->runtime.services.profile->uniquifier) == u);
        CHECK(ip_state(f, index, "after", f->particle) == 0);
    } else {
        mc_java_random replay = {0};
        replay.state = math;
        for (size_t i = 0; i < 5; ++i)
            (void)mc_java_random_next_double(&replay);
        CHECK(f->effects->layers[2]->size == count + 1 && ip_math_state(f) == replay.state &&
              atomic_load(&f->runtime.services.profile->uniquifier) ==
                  u * UINT64_C(1181783497276652981));
        mc_entity *particle = mc_client_effect_renderer_particle(f->effects, 2, count);
        mc_particle_snapshot snapshot;
        CHECK(particle && mc_entity_is_kind(particle, MC_ENTITY_CLASS_KIND_ITEM_PARTICLE) &&
              mc_particle_read(particle, &snapshot) && snapshot.item_pc == 58 &&
              snapshot.phase == MC_ENTITY_READY && particle->base.world == f->world);
        CHECK(ip_state(f, index, "after", particle) == 0 && ip_keep(f, particle) == 0);
    }
    fprintf(f->output, "{\"event\":\"N_emit\",\"case\":%u,\"dn_null\":%s,\"N\":", index,
            null_dn ? "true" : "false");
    ip_ref(f->output, f->renderer, "n");
    fputs(",\"N_world\":", f->output);
    ip_ref(f->output, f->renderer->fields.k, "fd");
    fputs(",\"DN\":", f->output);
    ip_ref(f->output, f->app->effect_renderer, "dn");
    fprintf(f->output, ",\"before_size\":%zu,\"after_size\":%zu,\"registrations\":%zu}\n", count,
            f->effects->layers[2]->size, state.registrations);
    /* End the next explicit fixture input only after observation of the guard.
     * No failed prefix is repaired: both calls returned normally. */
    if (null_dn)
        f->app->effect_renderer = f->effects;
    return 0;
}

static int ip_tick(ip_fixture *f, unsigned int index, const char *scenario, bool through_dn) {
    mc_entity *p = NULL;
    CHECK(ip_new(f, 97 + index, f->world, &p) == 0 && ip_prepare(f, p, scenario) == 0);
    if (through_dn)
        CHECK(mc_client_effect_renderer_change_world(f->effects, f->world, f->effects_error) ==
                  MC_EFFECT_RENDERER_OK &&
              mc_client_effect_renderer_add(f->effects, p, f->effects_error) ==
                  MC_EFFECT_RENDERER_OK);
    CHECK(ip_state(f, index, "before", p) == 0);
    const uint64_t own = p->random->state, math = ip_math_state(f),
                   world = f->world->fields.r->state;
    const double y = p->base.y;
    if (through_dn)
        CHECK(mc_client_effect_renderer_tick_in_context(f->effects, f->runtime.services.registry,
                                                        NULL, f->effects_error) ==
                  MC_EFFECT_RENDERER_OK &&
              f->effects->layers[2]->size == 0);
    else
        CHECK(mc_item_particle_tick(p, f->runtime.services.registry, NULL, f->tick_error) ==
              MC_ENTITY_OK);
    mc_particle_snapshot state;
    CHECK(mc_particle_read(p, &state) && state.tick_pc == 147 && !state.tick_active &&
          p->random->state == own && ip_math_state(f) == math &&
          f->world->fields.r->state == world && p->base.y != y);
    CHECK(p->base.aX == (strcmp(scenario, "ground") == 0) &&
          p->base.be == (strcmp(scenario, "expiry_air") == 0) &&
          state.particle.e == (strcmp(scenario, "expiry_air") == 0 ? 5 : 1));
    CHECK(ip_state(f, index, "after", p) == 0);
    mc_entity_release(p);
    return 0;
}

static int ip_texture(ip_fixture *f) {
    uint16_t units[128];
    const mc_texture_pack_text key = ip_text("/gui/items.png", units);
    mc_client_texture name = 0, cached = 0;
    CHECK(mc_client_render_engine_texture(f->app->render_engine, f->display, NULL, key, &name,
                                          f->lookup_error) == MC_CLIENT_LOOKUP_OK &&
          mc_client_render_engine_texture(f->app->render_engine, f->display, NULL, key, &cached,
                                          f->lookup_error) == MC_CLIENT_LOOKUP_OK &&
          name == cached);
    fprintf(f->output, "{\"event\":\"texture\",\"name\":%u,\"cached_name\":%u,\"transfer_hex\":\"",
            name, cached);
    const mc_client_direct_byte_buffer *bytes = f->app->render_engine->fields.f;
    CHECK(bytes && bytes->limit == 262144);
    for (size_t i = 0; i < bytes->limit; ++i)
        fprintf(f->output, "%02x", bytes->data[i]);
    fputs("\"}\n", f->output);
    return 0;
}

static int ip_draw(ip_fixture *f, unsigned int index, float partial, const char *directory) {
    CHECK(ip_frame(f) == 0 && ip_pixels(f, directory, index, true) == 0 &&
          ip_state(f, index, "before", f->particle) == 0);
    const uint64_t own = f->particle->random->state, math = ip_math_state(f),
                   world = f->world->fields.r->state;
    const size_t count = f->effects->layers[2]->size;
    uint64_t seeds[8];
    CHECK(count == 1 || count == 8);
    for (size_t i = 0; i < count; ++i)
        seeds[i] = f->effects->layers[2]->elements[i]->random->state;
    uint16_t units[128];
    const mc_texture_pack_text key = ip_text("/gui/items.png", units);
    bool cache_before = false, cache_after = false;
    mc_client_texture texture = 0;
    CHECK(mc_client_render_engine_cached_texture(f->app->render_engine, key, &cache_before,
                                                 &texture) &&
          cache_before == (index != 7));
    const mc_effect_draw_services services = {
        .graphics = f->runtime.graphics, .math = f->math, .display = f->display};
    CHECK(mc_client_effect_renderer_sprites_begin(f->effects, f->camera, partial, &services,
                                                  &f->call, f->draw_error) == MC_EFFECT_DRAW_OK &&
          f->call != NULL);
    CHECK(mc_client_render_engine_cached_texture(f->app->render_engine, key, &cache_after,
                                                 &texture) &&
          cache_after);
    mc_effect_draw_state state;
    CHECK(mc_client_effect_renderer_draw_inspect(f->call, &state) && state.returned &&
          state.owner == f->effects && state.camera == f->camera && state.item_particle_returned &&
          state.item_vertices_returned == 4 && state.visited == count && state.particle_pc == 361);
    CHECK(f->particle->random->state == own && ip_math_state(f) == math &&
          f->world->fields.r->state == world);
    for (size_t i = 0; i < count; ++i)
        CHECK(f->effects->layers[2]->elements[i]->random->state == seeds[i]);
    mc_client_nw_state nw;
    CHECK(mc_client_nw_inspect(f->runtime.graphics, &nw) && nw.fields.e && nw.fields.g &&
          nw.fields.e->position == 48 * count && !nw.fields.w);
    fprintf(f->output,
            "{\"event\":\"draw\",\"case\":%u,\"cache_before\":%s,\"cache_after\":%s,"
            "\"position\":%zu,\"words\":[",
            index, cache_before ? "true" : "false", cache_after ? "true" : "false",
            nw.fields.e->position);
    ip_words(f->output, (const uint8_t *)nw.fields.g, 48 * count);
    fputs("],\"backing_prefix\":[", f->output);
    ip_words(f->output, (const uint8_t *)nw.fields.g, 1024);
    fputs("],\"DN\":", f->output);
    ip_ref(f->output, f->effects, "dn");
    fputs(",\"engine\":", f->output);
    ip_ref(f->output, f->effects->render_engine, "ji");
    fputs(",\"gl\":", f->output);
    CHECK(ip_emit_gl(f) == 0);
    fputs("}\n", f->output);
    CHECK(mc_client_effect_renderer_add(f->effects, f->particle, f->effects_error) ==
          MC_EFFECT_RENDERER_BUSY);
    CHECK(mc_client_effect_renderer_draw_retire(&f->call, f->draw_error) == MC_EFFECT_DRAW_OK &&
          f->call == NULL && ip_state(f, index, "after", f->particle) == 0 &&
          ip_pixels(f, directory, index, false) == 0);
    return 0;
}

static int ip_landing(ip_fixture *f, const char *directory) {
    CHECK(mc_client_effect_renderer_change_world(f->effects, f->world, f->effects_error) ==
              MC_EFFECT_RENDERER_OK &&
          f->effects->layers[2]->size == 0);
    CHECK(ip_construct(f, MC_ENTITY_CLASS_KIND_SLIME, &f->slime) == 0);
    mc_mob_error error;
    CHECK(mc_mob_slime_set_size(f->slime, 1, &error) == MC_MOB_OK &&
          ip_pose(f->slime, 8.5, 64.05, 8.5, 30, 0) == 0);
    mc_entity *slime = f->slime;
    slime->base.aX = false;
    slime->base.velocity_x = slime->base.velocity_z = 0;
    slime->base.velocity_y = -.25;
    slime->mob->slime_jump_delay = 15;
    slime->mob->living.F = .25f;
    slime->mob->living.G = .015625f;
    slime->mob->living.aj = .5f;
    bool published = false;
    CHECK(mc_world_runtime_insert_entity(f->world, slime, &published, NULL) ==
              MC_WORLD_STORAGE_OK &&
          published);
    const mc_mob_tick_limits limits = {
        .max_work = MC_MOB_TICK_DEFAULT_WORK,
        .environment = {.max_particle_work = MC_ENTITY_TICK_DEFAULT_PARTICLE_WORK,
                        .material = {.max_cell_work =
                                         MC_WORLD_MATERIAL_MOVEMENT_DEFAULT_CELL_WORK}},
        .action = {.max_loop_work = MC_PLAYER_ACTION_DEFAULT_LOOP_WORK},
        .movement = {.max_loop_work = MC_ENTITY_MOVE_DEFAULT_LOOP_WORK},
        .path = mc_world_path_default_limits()};
    const uint64_t math_before = ip_math_state(f);
    const mc_entity_status result =
        mc_mob_tick(slime, f->runtime.services.registry, f->math, &limits, f->mob_error);
    if (result != MC_ENTITY_OK)
        fprintf(stderr,
                "Slime landing status%d %s.%s%s:%" PRId32 " child %s.%s%s:%" PRId32
                " native%d source%d operation=%s\n",
                (int)result, ip_label(f->mob_error->site.class_name),
                ip_label(f->mob_error->site.method), ip_label(f->mob_error->site.descriptor),
                f->mob_error->site.pc, ip_label(f->mob_error->child_site.class_name),
                ip_label(f->mob_error->child_site.method),
                ip_label(f->mob_error->child_site.descriptor), f->mob_error->child_site.pc,
                f->mob_error->native_policy, f->mob_error->source_exception,
                ip_label(f->mob_error->operation));
    CHECK(result == MC_ENTITY_OK && slime->base.aX && f->effects->layers[2]->size == 8);
    mc_java_random replay = {0};
    replay.state = math_before;
    for (size_t i = 0; i < 8 * 5; ++i)
        (void)mc_java_random_next_double(&replay);
    CHECK(ip_math_state(f) == replay.state);
    fprintf(f->output, "{\"event\":\"landing\",\"case\":10,\"slime\":");
    ip_ref(f->output, slime, "uw");
    fprintf(f->output, ",\"grounded\":%s,\"layer2_size\":%zu,\"particles\":[",
            slime->base.aX ? "true" : "false", f->effects->layers[2]->size);
    for (size_t i = 0; i < 8; ++i) {
        if (i)
            fputc(',', f->output);
        mc_entity *particle = f->effects->layers[2]->elements[i];
        CHECK(mc_entity_is_kind(particle, MC_ENTITY_CLASS_KIND_ITEM_PARTICLE) &&
              particle->base.world == f->renderer->fields.k);
        ip_ref(f->output, particle, "pb");
        for (size_t j = 0; j < i; ++j)
            CHECK(particle != f->effects->layers[2]->elements[j]);
    }
    fputs("]}\n", f->output);
    mc_entity_release(f->particle);
    f->particle = NULL;
    uint64_t seeds[8];
    for (size_t i = 0; i < 8; ++i) {
        mc_entity *particle = f->effects->layers[2]->elements[i];
        char phase[32];
        CHECK(snprintf(phase, sizeof(phase), "emitted_%zu", i) > 0 &&
              ip_state(f, 10, phase, particle) == 0 && ip_keep(f, particle) == 0 &&
              ip_prepare(f, particle, "air") == 0 &&
              mc_entity_set_position(particle, 8.25 + (double)i * .125, 66, 8.5) == MC_ENTITY_OK);
        CHECK(snprintf(phase, sizeof(phase), "layer_before_%zu", i) > 0 &&
              ip_state(f, 11, phase, particle) == 0);
        seeds[i] = particle->random->state;
    }
    CHECK(mc_client_effect_renderer_tick_in_context(f->effects, f->runtime.services.registry, NULL,
                                                    f->effects_error) == MC_EFFECT_RENDERER_OK &&
          f->effects->layers[2]->size == 8);
    for (size_t i = 0; i < 8; ++i) {
        mc_entity *particle = f->effects->layers[2]->elements[i];
        mc_particle_snapshot state;
        char phase[32];
        CHECK(mc_particle_read(particle, &state) && state.tick_pc == 147 && state.particle.e == 1 &&
              !particle->base.be && particle->random->state == seeds[i] &&
              snprintf(phase, sizeof(phase), "layer_after_%zu", i) > 0 &&
              ip_state(f, 11, phase, particle) == 0);
    }
    f->particle = f->effects->layers[2]->elements[7];
    CHECK(mc_entity_retain(f->particle) == MC_ENTITY_OK && ip_draw(f, 12, .375f, directory) == 0);
    return 0;
}

static int ip_source_failure(ip_fixture *f, const char *mode) {
    unsigned int index = 0, pc = 0;
    const char *source_class = NULL, *method = NULL;
    CHECK(ip_frame(f) == 0);
    if (strcmp(mode, "null_item") == 0 || strcmp(mode, "null_snow") == 0) {
        const bool null_item = strcmp(mode, "null_item") == 0;
        index = null_item ? 13u : 14u;
        pc = null_item ? 17u : 42u;
        source_class = "pb";
        method = "<init>";
        size_t snow_slot = SIZE_MAX;
        if (!null_item) {
            for (size_t i = 0; i < MOB_COUNT(f->runtime.services.registry->blocks.named); ++i)
                if (strcmp(mc_registry_block_static_name(i), "aV") == 0)
                    snow_slot = i;
            CHECK(snow_slot != SIZE_MAX &&
                  f->runtime.services.registry->blocks.named[snow_slot] == f->snow);
            /* Declared actual Source static-reference input. Its separately
             * allocated arena object stays owned; this field stays NULL even
             * through terminal cleanup. No failed-prefix repair is needed. */
            f->runtime.services.registry->blocks.named[snow_slot] = NULL;
        }
        const uint64_t math = ip_math_state(f), world = f->world->fields.r->state,
                       u = atomic_load(&f->runtime.services.profile->uniquifier);
        CHECK(mc_item_particle_allocate(f->runtime.services.entities, &f->particle, NULL) ==
              MC_ENTITY_OK);
        CHECK(mc_item_particle_construct(f->particle, f->world, f->runtime.services.registry,
                                         f->math, 8.5, 65.5, 8.5, null_item ? NULL : f->item,
                                         f->particle_error) == MC_ENTITY_STATE_ERROR);
        CHECK(f->particle_error->source_exception && !f->particle_error->native_policy &&
              f->particle_error->throwable == MC_PARTICLE_SOURCE_EXCEPTION &&
              strcmp(f->particle_error->exception_class, "java/lang/NullPointerException") == 0 &&
              strcmp(f->particle_error->source_class, "pb") == 0 &&
              strcmp(f->particle_error->source_method, "<init>") == 0 &&
              f->particle_error->source_pc == pc && f->particle_error->particle_pc == 306 &&
              f->particle->phase == MC_ENTITY_FAILED_CONSTRUCTION);
        CHECK(ip_constructor_receipt(f, index, f->particle, math, world, u) == 0 &&
              ip_state(f, index, "exception_receiver", f->particle) == 0);
        const mc_particle_fields fields = f->particle->particle->fields;
        const mc_entity_base_fields base = f->particle->base;
        const mc_entity_bounds box = *f->particle->bounds;
        const uint64_t own_after = f->particle->random->state, math_after = ip_math_state(f);
        const mc_entity_status retried = mc_item_particle_construct(
            f->particle, f->world, f->runtime.services.registry, f->math, 8.5, 65.5, 8.5,
            null_item ? NULL : f->item, f->particle_error);
        CHECK(retried == MC_ENTITY_INVALID_PHASE &&
              memcmp(&fields, &f->particle->particle->fields, sizeof(fields)) == 0 &&
              memcmp(&base, &f->particle->base, sizeof(base)) == 0 &&
              memcmp(&box, f->particle->bounds, sizeof(box)) == 0 &&
              f->particle->random->state == own_after && ip_math_state(f) == math_after &&
              f->particle->phase == MC_ENTITY_FAILED_CONSTRUCTION);
        CHECK(mc_entity_retain(f->particle) == MC_ENTITY_OK);
        mc_entity_release(f->particle); /* End exactly the additional native occurrence. */
        CHECK(memcmp(&fields, &f->particle->particle->fields, sizeof(fields)) == 0 &&
              memcmp(&base, &f->particle->base, sizeof(base)) == 0 &&
              memcmp(&box, f->particle->bounds, sizeof(box)) == 0 &&
              (null_item || f->runtime.services.registry->blocks.named[snow_slot] == NULL));
    } else {
        const bool draw = strcmp(mode, "null_world_draw") == 0;
        CHECK(draw || strcmp(mode, "null_world_tick") == 0);
        index = draw ? 16u : 15u;
        pc = draw ? 118u : 394u;
        source_class = "sn";
        method = draw ? "a" : "b";
        CHECK(ip_new(f, draw ? 106u : 105u, NULL, &f->particle) == 0 &&
              ip_prepare(f, f->particle, draw ? "render" : "air") == 0);
        if (draw)
            CHECK(mc_client_effect_renderer_add(f->effects, f->particle, f->effects_error) ==
                  MC_EFFECT_RENDERER_OK);
        CHECK(ip_state(f, index, "before", f->particle) == 0);
        const uint64_t own = f->particle->random->state, math = ip_math_state(f),
                       world = f->world->fields.r->state;
        if (!draw) {
            CHECK(mc_item_particle_tick(f->particle, f->runtime.services.registry, NULL,
                                        f->tick_error) != MC_ENTITY_OK);
            CHECK(f->tick_error->particle.source_exception &&
                  !f->tick_error->particle.native_policy &&
                  f->tick_error->particle.throwable == MC_PARTICLE_SOURCE_EXCEPTION &&
                  strcmp(f->tick_error->particle.exception_class,
                         "java/lang/NullPointerException") == 0 &&
                  f->tick_error->particle.tick_pc == 77 && f->tick_error->movement.source_null &&
                  strcmp(f->tick_error->particle.source_class, "xw") == 0 &&
                  strcmp(f->tick_error->particle.source_method, "w_") == 0 &&
                  f->tick_error->movement.source_null_pc == pc);
        } else {
            const mc_effect_draw_services services = {
                .graphics = f->runtime.graphics, .math = f->math, .display = f->display};
            CHECK(mc_client_effect_renderer_sprites_begin(f->effects, f->camera, .375f, &services,
                                                          &f->call, f->draw_error) ==
                      MC_EFFECT_DRAW_CHILD_FAILED &&
                  f->call != NULL);
            mc_effect_draw_state state;
            CHECK(mc_client_effect_renderer_draw_inspect(f->call, &state) && !state.returned &&
                  state.current == f->particle && state.particle_pc == 139 &&
                  state.item_vertices_returned == 0 && !state.color_set &&
                  f->draw_error->source_exception && !f->draw_error->native_policy &&
                  f->draw_error->entity_render != NULL &&
                  strcmp(f->draw_error->entity_render->site.class_name, "sn") == 0 &&
                  strcmp(f->draw_error->entity_render->site.method, "a") == 0 &&
                  f->draw_error->entity_render->site.pc == (int32_t)pc &&
                  strcmp(f->draw_error->exception_class, "java/lang/NullPointerException") == 0);
            mc_client_nw_state nw;
            CHECK(mc_client_nw_inspect(f->runtime.graphics, &nw) && nw.fields.w &&
                  nw.fields.e->position == 0);
            const mc_particle_fields fields = f->particle->particle->fields;
            const mc_entity_base_fields base = f->particle->base;
            CHECK(mc_client_effect_renderer_free(&f->effects) == MC_EFFECT_RENDERER_BUSY &&
                  mc_client_effect_renderer_add(f->effects, f->particle, f->effects_error) ==
                      MC_EFFECT_RENDERER_BUSY &&
                  ip_state(f, index, "exception_receiver", f->particle) == 0);
            CHECK(mc_client_effect_renderer_draw_retire(&f->call, f->draw_error) ==
                      MC_EFFECT_DRAW_OK &&
                  f->call == NULL && f->particle->base.world == NULL &&
                  memcmp(&fields, &f->particle->particle->fields, sizeof(fields)) == 0 &&
                  memcmp(&base, &f->particle->base, sizeof(base)) == 0 &&
                  mc_client_nw_inspect(f->runtime.graphics, &nw) && nw.fields.w &&
                  nw.fields.e->position == 0);
        }
        CHECK(f->particle->base.world == NULL && f->particle->random->state == own &&
              ip_math_state(f) == math && f->world->fields.r->state == world &&
              ip_state(f, index, "after", f->particle) == 0);
    }
    fprintf(f->output,
            "{\"event\":\"failure\",\"case\":%u,\"source_class\":\"%s\","
            "\"method\":\"%s\",\"pc\":%u,\"exception_class\":"
            "\"java/lang/NullPointerException\",\"source_exception\":true,"
            "\"native_policy\":false,\"prefix_preserved_through_cleanup\":true}\n",
            index, source_class, method, pc);
    return 0;
}

static int ip_pending_control(ip_fixture *f) {
    const mc_registry_string *name = NULL;
    mc_math_classes *limited = NULL;
    CHECK(mc_registry_runtime_literal_ascii(f->runtime.services.registry, "slime", &name, NULL) ==
              MC_REGISTRY_OK &&
          mc_math_classes_create(4096, &limited, NULL) == MC_MATH_CLASS_OK);
    mc_math_class_state ms;
    CHECK(mc_math_classes_state(limited, &ms, NULL) == MC_MATH_CLASS_OK &&
          ms.phase == MC_MATH_CLASS_COLD && ms.holds == 1 && ms.source_a == NULL);
    const uint64_t before = ip_math_state(f), world = f->world->fields.r->state;
    const size_t size = f->effects->layers[2]->size, dn_holds = f->effects->holds;
    f->app->source_math_classes = limited; /* Declared native service input. */
    CHECK(mc_world_runtime_particle(f->world, name, 8.5, 65.5, 8.5, 0, 0, 0,
                                    f->presentation_error) == MC_WORLD_STORAGE_LIMIT);
    mc_world_access_failure capture = {0};
    const mc_world_access_failure *original = &f->presentation_error->world_access_failure;
    mc_render_global_callback_state state;
    CHECK(mc_render_global_access_callback_inspect(f->access, original, &state) &&
          state.failure->status == MC_CLIENT_RENDER_GLOBAL_LIMIT &&
          state.failure->source_pc == 702 && state.failure->native_policy &&
          state.particle_receiver == f->effects && state.particle_world == f->world &&
          state.particle_math == limited && state.particle_candidate &&
          state.particle_candidate->phase == MC_ENTITY_FAILED_CONSTRUCTION && state.particle &&
          state.particle->native_policy && !state.particle->source_exception &&
          state.particle->exception_class == NULL &&
          state.particle->throwable == MC_PARTICLE_NO_THROWABLE &&
          strcmp(state.particle->source_class, "xw") == 0 && state.particle->source_pc == 164 &&
          state.particle->math.status == MC_MATH_CLASS_LIMIT &&
          state.particle->math.source_pc == 2 &&
          state.particle->constructor_random.math_count == 5 &&
          state.particle->constructor_random.own_count == 0);
    CHECK(mc_math_classes_state(limited, &ms, NULL) == MC_MATH_CLASS_OK &&
          ms.phase == MC_MATH_CLASS_FAILED && ms.holds == 2 && ms.source_a == NULL);
    mc_java_random replay = {0};
    replay.state = before;
    for (size_t i = 0; i < 5; ++i)
        CHECK(ip_dbits(mc_java_random_next_double(&replay)) ==
              ip_dbits(state.particle->constructor_random.math_values[i]));
    CHECK(ip_math_state(f) == replay.state && f->world->fields.r->state == world &&
          f->effects->layers[2]->size == size && f->effects->holds == dn_holds + 1 &&
          atomic_load(&f->app->render_borrows) == 1);
    mc_entity *candidate = (mc_entity *)(void *)state.particle_candidate;
    const mc_entity_base_fields base = candidate->base;
    const mc_particle_fields fields = candidate->particle->fields;
    const uint64_t seed = candidate->random->state;
    const mc_particle_error first = *state.particle;
    CHECK(mc_entity_retain(candidate) == MC_ENTITY_OK &&
          mc_world_access_failure_retain(original, &capture) == MC_WORLD_STORAGE_OK);
    CHECK(mc_client_application_constructor_retire(&f->app, f->runtime.error) ==
              MC_CLIENT_APP_BUSY &&
          strcmp(f->runtime.error->operation, "actual retained render still borrows App") == 0 &&
          f->app->constructor_phase == MC_CLIENT_APP_RAW && f->app->source_namespace_borrowed &&
          f->app->effect_renderer == f->effects && f->app->g == f->renderer &&
          f->app->source_math_classes == limited);
    CHECK(mc_client_effect_renderer_free(&f->effects) == MC_EFFECT_RENDERER_BUSY &&
          mc_render_global_access_retire(&f->access, f->global_error) ==
              MC_CLIENT_RENDER_GLOBAL_BUSY &&
          strcmp(f->global_error->operation, "RenderGlobal captured callback failure") == 0);
    /* A real caller occurrence release is nonfinal (2 -> 1), not BUSY. The
     * retained failed invocation must independently keep this exact owner. */
    CHECK(mc_math_classes_release(&limited, NULL) == MC_MATH_CLASS_OK && limited == NULL &&
          mc_math_classes_state(state.particle_math, &ms, NULL) == MC_MATH_CLASS_OK &&
          ms.holds == 1 && mc_render_global_access_callback_inspect(f->access, &capture, &state) &&
          memcmp(&first, state.particle, sizeof(first)) == 0);
    mc_world_access_failure_release(&capture);
    CHECK(
        mc_render_global_access_retire(&f->access, f->global_error) == MC_CLIENT_RENDER_GLOBAL_OK &&
        f->access == NULL && f->world->world_accesses.count == 0 &&
        atomic_load(&f->app->render_borrows) == 0 && f->effects->holds == dn_holds &&
        f->effects->layers[2]->size == size && candidate->phase == MC_ENTITY_FAILED_CONSTRUCTION &&
        candidate->random->state == seed && memcmp(&base, &candidate->base, sizeof(base)) == 0 &&
        memcmp(&fields, &candidate->particle->fields, sizeof(fields)) == 0 &&
        ip_math_state(f) == replay.state);
    /* Change the borrowed native fixture association only after real failed
     * owner retirement; no Source field or failed constructor is repaired. */
    f->app->source_math_classes = f->math;
    mc_entity_release(candidate);
    fputs("{\"event\":\"control\",\"name\":\"pending_N_App_DN_World_Math\",\"native_policy\":true,"
          "\"source_exception\":false,\"N_pc\":702,\"XW_pc\":164,\"IN_pc\":2,\"shared_draws\":5,"
          "\"private_draws\":0,\"WA_retire_refused\":true,\"App_retire_refused\":true,\"native_"
          "prefix_preserved\":true}\n",
          f->output);
    return 0;
}

static int ip_terminal_control(ip_fixture *f) {
    CHECK(f->access == NULL && f->world->world_accesses.count == 0 && f->context_count == 16);
    /* Selected Source profiles ended. End actual authored caller occurrences,
     * preserving every DN occurrence; no slot below is a guessed exemption. */
    ip_end_contexts(f);
    mc_entity_release(f->particle);
    f->particle = NULL;
    mc_entity *p = f->effects->layers[2]->elements[7];
    const size_t holds = p->holds;
    CHECK(mc_client_effect_renderer_add(f->effects, p, f->effects_error) == MC_EFFECT_RENDERER_OK &&
          f->effects->layers[2]->size == 9 && f->effects->layers[2]->elements[8] == p &&
          p->holds == holds + 1);
    mc_effect_renderer_terminal_view view;
    CHECK(mc_client_effect_renderer_terminal_inspect(f->effects, &view, f->effects_error) ==
              MC_EFFECT_RENDERER_OK &&
          view.world_slot == &f->effects->world && view.particle_occurrences == 9 &&
          view.holds == 1);
    size_t distinct = 0;
    for (size_t i = 0; i < 9; ++i) {
        bool seen = false;
        for (size_t j = 0; j < i; ++j)
            seen = seen || view.lists[2]->elements[j] == view.lists[2]->elements[i];
        if (!seen)
            ++distinct;
    }
    CHECK(distinct == 8);
    mc_world_terminal_entity_anchor entities[11] = {{&f->camera}, {&f->slime}};
    for (size_t i = 0; i < 9; ++i)
        entities[i + 2].slot = &view.lists[2]->elements[i];
    const mc_world_terminal_world_anchor worlds[] = {{view.world_slot}};
    const mc_world_terminal_anchors anchors = {entities, MOB_COUNT(entities), worlds,
                                               MOB_COUNT(worlds)};
    const size_t bindings = f->world->persistent_bindings;
    const mc_entity_base_fields base = p->base;
    const mc_particle_fields fields = p->particle->fields;
    const uint64_t seed = p->random->state;
    mc_world_terminal_inventory inventory = {0};
    mc_world_storage_error error = {0};
    CHECK(f->dimension == f->world->fields.t && mc_world_provider_hold_count(f->dimension) == 2);
    const mc_world_storage_status provider_status =
        mc_world_runtime_terminal_admit_owned(f->world, &anchors, NULL, &inventory, &error);
    if (provider_status != MC_WORLD_STORAGE_BUSY || error.operation == NULL ||
        strcmp(error.operation, "World terminal external WorldProvider hold") != 0)
        fprintf(stderr,
                "PB terminal Provider preflight status%d operation=%s holds%zu bindings%zu\n",
                (int)provider_status, ip_label(error.operation),
                mc_world_provider_hold_count(f->world->fields.t), bindings);
    CHECK(provider_status == MC_WORLD_STORAGE_BUSY &&
          strcmp(error.operation, "World terminal external WorldProvider hold") == 0);
    /* End only our actual create() occurrence. World.t owns the remaining
     * one, keeping the same bound Provider alive through World retirement. */
    mc_world_provider_release(f->dimension);
    f->dimension = NULL;
    CHECK(mc_world_provider_hold_count(f->world->fields.t) == 1 &&
          f->world->persistent_bindings == bindings);
    CHECK(mc_entity_retain(p) == MC_ENTITY_OK);
    const mc_world_storage_status external_status =
        mc_world_runtime_terminal_admit_owned(f->world, &anchors, NULL, &inventory, &error);
    if (external_status != MC_WORLD_STORAGE_BUSY || error.operation == NULL ||
        strcmp(error.operation, "World terminal external Entity or TileEntity hold") != 0)
        fprintf(stderr, "PB terminal Entity preflight status%d operation=%s holds%zu bindings%zu\n",
                (int)external_status, ip_label(error.operation), p->holds, bindings);
    CHECK(external_status == MC_WORLD_STORAGE_BUSY &&
          strcmp(error.operation, "World terminal external Entity or TileEntity hold") == 0 &&
          f->effects->layers[2]->size == 9 && f->world->persistent_bindings == bindings &&
          memcmp(&base, &p->base, sizeof(base)) == 0 &&
          memcmp(&fields, &p->particle->fields, sizeof(fields)) == 0 && p->random->state == seed);
    mc_entity_release(p); /* Only the extra unanchored external occurrence. */
    CHECK(mc_world_runtime_terminal_admit_owned(f->world, &anchors, NULL, &inventory, &error) ==
              MC_WORLD_STORAGE_OK &&
          inventory.entities == 10 && inventory.expected_world_bindings == 12 &&
          bindings == inventory.expected_world_bindings && f->effects->layers[2]->size == 9 &&
          memcmp(&base, &p->base, sizeof(base)) == 0 &&
          memcmp(&fields, &p->particle->fields, sizeof(fields)) == 0);
    fprintf(
        f->output,
        "{\"event\":\"control\",\"name\":\"nonempty_DN_terminal\",\"PB_occurrences\":9,\"distinct_"
        "PB\":8,\"total_entities\":%zu,\"world_bindings\":%zu,\"external_refused\":true}\n",
        inventory.entities, inventory.expected_world_bindings);
    return 0;
}

static int ip_close(ip_fixture *f) {
    CHECK(f->call == NULL);
    if (f->access != NULL)
        CHECK(mc_render_global_access_source_remove(f->access, f->world, f->global_error) ==
                  MC_CLIENT_RENDER_GLOBAL_OK &&
              mc_render_global_access_retire(&f->access, f->global_error) ==
                  MC_CLIENT_RENDER_GLOBAL_OK &&
              f->access == NULL);
    CHECK(mc_client_render_global_retire(&f->renderer, f->global_error) ==
              MC_CLIENT_RENDER_GLOBAL_OK &&
          f->renderer == NULL);
    f->app->g = NULL;
    f->app->source_math_classes = NULL;
    CHECK(mc_client_cx_release(&f->app->S, NULL) == MC_CLIENT_CX_OK &&
          mc_client_cx_classes_release(&f->cx_classes, NULL) == MC_CLIENT_CX_OK);
    ip_end_contexts(f);
    CHECK(mc_client_effect_renderer_free(&f->effects) == MC_EFFECT_RENDERER_OK);
    f->app->effect_renderer = NULL;
    mc_entity_release(f->particle);
    f->particle = NULL;
    f->app->i = NULL;
    mc_entity_release(f->camera);
    f->camera = NULL;
    mc_entity_release(f->slime);
    f->slime = NULL;
    f->app->active_world = NULL;
    CHECK(mc_world_runtime_retire_entity_containers(f->world, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_chunk_pending_clear(f->provider.pending) == MC_CHUNK_ACQUIRE_OK &&
          mc_chunk_cache_init(f->provider.cache, f->provider.cache->entries,
                              f->provider.cache->capacity, f->provider.cache->insertions,
                              f->provider.cache->insertion_capacity));
    mc_world_provider_unbind(f->world->fields.t);
    CHECK(mc_world_runtime_retire_world(f->world, NULL) == MC_WORLD_STORAGE_OK);
    for (size_t i = 0; i < 25; ++i)
        CHECK(mc_world_store_retire_chunk(f->store, f->chunks[i], NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_free(f->world, NULL) == MC_WORLD_STORAGE_OK);
    if (f->dimension != NULL)
        mc_world_provider_release(f->dimension);
    CHECK(mc_world_store_free_checked(&f->store, NULL) == MC_WORLD_STORAGE_OK);
    mc_world_regions_free(f->regions);
    mc_world_info_name_release(f->name);
    CHECK(mc_world_runtime_classes_free(f->world_classes, NULL) == MC_WORLD_STORAGE_OK &&
          mc_math_classes_release(&f->math, NULL) == MC_MATH_CLASS_OK);
    CHECK(mc_client_render_engine_retire_lookup(f->app->render_engine, f->lookup_error) ==
              MC_CLIENT_LOOKUP_OK &&
          mc_client_texture_lookup_free_metadata(f->app->render_engine) &&
          mc_client_render_engine_release(f->app->render_engine) == MC_CLIENT_RENDER_ENGINE_OK);
    f->app->render_engine = NULL;
    CHECK(mc_client_render_engine_retire_resources(f->runtime.graphics, NULL) ==
              MC_CLIENT_RENDER_TEXTURE_OK &&
          mc_client_default_font_release(f->font) == MC_CLIENT_FONT_OK &&
          mc_client_display_destroy(f->display, NULL));
    f->app->display = NULL;
    CHECK(mc_client_texture_pack_list_release(f->packs, NULL) == MC_TEXTURE_PACK_OK);
    f->app->D = NULL;
    CHECK(mc_game_settings_retire(&f->app->settings, NULL) == MC_GAME_SETTINGS_OK &&
          mc_resource_files_free(f->textures) == MC_REGISTRY_OK);
    CHECK(
        mc_client_application_constructor_retire(&f->app, f->runtime.error) == MC_CLIENT_APP_BUSY &&
        f->app->constructor_phase == MC_CLIENT_APP_RETIRING && !f->app->source_namespace_borrowed);
    CHECK(mc_client_application_classes_release(&f->runtime.services.application_classes, NULL) ==
              MC_CLIENT_APPLICATION_OWNER_OK &&
          mc_client_application_constructor_retire(&f->app, f->runtime.error) == MC_CLIENT_APP_OK &&
          f->app == NULL);
    CHECK(mc_registry_classes_free(f->runtime.services.registry, NULL) == MC_REGISTRY_OK);
    f->runtime.services.registry = NULL;
    free(f->tick_error);
    free(f->particle_error);
    free(f->effects_error);
    free(f->draw_error);
    free(f->lookup_error);
    free(f->global_error);
    free(f->presentation_error);
    free(f->mob_error);
    CHECK(fixture_close(&f->runtime) == 0);
    return 0;
}

#ifndef MC_ITEM_PARTICLE_MAIN
#define MC_ITEM_PARTICLE_MAIN main
#endif
int MC_ITEM_PARTICLE_MAIN(int argc, char **argv) {
    CHECK(argc == 5 || argc == 6);
    const char *mode = argc == 6 ? argv[5] : "main";
    SDL_SetMainReady();
    ip_fixture *f = calloc(1, sizeof(*f));
    CHECK(f != NULL);
    char path[4096];
    CHECK(snprintf(path, sizeof(path), "%s/native.ndjson", argv[4]) > 0);
    f->output = fopen(path, "wb");
    CHECK(f->output && ip_open(f, argv[1], argv[2], argv[3]) == 0);
    f->particle_error = calloc(1, sizeof(*f->particle_error));
    f->tick_error = calloc(1, sizeof(*f->tick_error));
    f->effects_error = calloc(1, sizeof(*f->effects_error));
    CHECK(f->particle_error && f->tick_error && f->effects_error);
    CHECK(ip_construct(f, MC_ENTITY_CLASS_KIND_COW, &f->camera) == 0 &&
          ip_pose(f->camera, 8.5, 65, 12.5, 30, 15) == 0);
    f->camera->base.bl = 8.25;
    f->camera->base.bm = 64.75;
    f->camera->base.bn = 12.25;
    f->app->i = f->camera;
    bool published = false;
    CHECK(mc_world_runtime_insert_entity(f->world, f->camera, &published, NULL) ==
              MC_WORLD_STORAGE_OK &&
          published);
    CHECK(mc_registry_item_static_get(f->runtime.services.registry, "aK", &f->item, NULL) ==
              MC_REGISTRY_OK &&
          f->item &&
          mc_registry_block_static_get(f->runtime.services.registry, "aV", &f->snow, NULL) ==
              MC_REGISTRY_OK &&
          f->snow);
    CHECK(mc_client_effect_renderer_allocate(f->runtime.services.entities, NULL, &f->effects,
                                             f->effects_error) == MC_EFFECT_RENDERER_OK &&
          mc_client_effect_renderer_construct(f->effects, f->world, f->app->render_engine,
                                              f->effects_error) == MC_EFFECT_RENDERER_OK);
    f->app->effect_renderer = f->effects;
    CHECK(ip_n_open(f) == 0 && ip_nw_prepare(f) == 0 &&
          mc_entity_bounds_pool_reset(f->runtime.services.entities, NULL) == MC_ENTITY_OK &&
          mc_entity_vector_pool_reset(f->runtime.services.entities, NULL) == MC_ENTITY_OK);
    if (strcmp(mode, "main") != 0) {
        CHECK(ip_source_failure(f, mode) == 0);
        CHECK(!ferror(f->output) && fclose(f->output) == 0 && ip_close(f) == 0);
        free(f);
        printf("Item particle: %u checks; genuine PB/XW/World/DN\n", checks);
        return 0;
    }
    CHECK(ip_emit(f, 0, false) == 0);
    CHECK(ip_new(f, 1, f->world, &f->particle) == 0);
    mc_entity_release(f->particle);
    f->particle = NULL;
    CHECK(ip_new(f, 2, NULL, &f->particle) == 0);
    mc_entity_release(f->particle);
    f->particle = NULL;
    CHECK(ip_tick(f, 3, "air", false) == 0 && ip_tick(f, 4, "ground", false) == 0 &&
          ip_tick(f, 5, "expiry_air", false) == 0 && ip_tick(f, 6, "expiry_air", true) == 0);
    CHECK(ip_new(f, 104, f->world, &f->particle) == 0 &&
          ip_prepare(f, f->particle, "render") == 0 &&
          mc_client_effect_renderer_change_world(f->effects, f->world, f->effects_error) ==
              MC_EFFECT_RENDERER_OK &&
          mc_client_effect_renderer_add(f->effects, f->particle, f->effects_error) ==
              MC_EFFECT_RENDERER_OK);
    CHECK(ip_draw(f, 7, .375f, argv[4]) == 0 && ip_texture(f) == 0 &&
          ip_draw(f, 8, 1, argv[4]) == 0 && ip_emit(f, 9, true) == 0 &&
          ip_landing(f, argv[4]) == 0);
    CHECK(ip_pending_control(f) == 0 && ip_terminal_control(f) == 0);
    CHECK(!ferror(f->output) && fclose(f->output) == 0 && ip_close(f) == 0);
    free(f);
    printf("Item particle: %u checks; genuine PB/XW/World/DN\n", checks);
    return 0;
}
