#ifndef MC_TEST_LOOKUP_OBSERVATION_H
#define MC_TEST_LOOKUP_OBSERVATION_H
#include "application_state.h"
#include "client/render_engine/color_lookup.h"
#include "client/render_engine/colorizer.h"
#include "client/render_engine/internal.h"
#include "client/render_engine/textures.h"
#include "client/startup/application_construction.h"
#include "client/texture_packs/internal.h"
#include "mc/resources/files/files.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Authored source-only observation, using the real full constructor/services.
 * No preconstructed child result, clock/Random fixture or App layout copy. */
typedef struct observation {
    mc_client_application_constructor_services services;
    mc_materials *materials;
    mc_resource_files *files;
    mc_client_render_engine_classes *classes;
    mc_client_default_font *font;
    client_application *app;
    mc_client_texture_pack_list *packs;
    mc_client_render_engine *engine;
    mc_client_display *display;
    mc_client_application_construction_error *app_error;
    mc_client_texture_lookup_error *error;
    mc_client_color_array *observed[128];
    size_t observed_count;
    bool failed, event_started;
    const char *pack_class;
} observation;

#define OBS_NEED(x)                                                                                \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "lookup observation failure line%d: %s\n", __LINE__, #x);              \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

static inline void obs_string(const char *value) {
    if (value == NULL) {
        fputs("null", stdout);
        return;
    }
    putchar('"');
    for (const unsigned char *p = (const unsigned char *)value; *p != 0; ++p) {
        if (*p == '"' || *p == '\\') {
            putchar('\\');
            putchar(*p);
        } else if (*p < 32 || *p > 126)
            printf("\\u%04x", (unsigned)*p);
        else
            putchar(*p);
    }
    putchar('"');
}

static inline void obs_hex(const uint8_t *bytes, size_t count) {
    putchar('"');
    for (size_t i = 0; i < count; ++i)
        printf("%02x", (unsigned)bytes[i]);
    putchar('"');
}

static inline mc_texture_pack_text obs_key(const char *value, uint16_t units[128]) {
    if (value == NULL)
        return (mc_texture_pack_text){NULL, 0, true};
    size_t count = strlen(value);
    if (count > 128)
        return (mc_texture_pack_text){NULL, 0, true};
    for (size_t i = 0; i < count; ++i)
        units[i] = (unsigned char)value[i];
    return (mc_texture_pack_text){units, count, false};
}

static inline void obs_utf16(const uint16_t *units, size_t count, bool is_null) {
    if (is_null) {
        fputs("null", stdout);
        return;
    }
    putchar('"');
    for (size_t i = 0; i < count; ++i)
        printf("%04x", (unsigned)units[i]);
    putchar('"');
}

static inline size_t obs_identity(observation *o, mc_client_color_array *array) {
    if (array == NULL)
        return 0;
    for (size_t i = 0; i < o->observed_count; ++i)
        if (o->observed[i] == array)
            return i + 1;
    if (o->observed_count == 128 ||
        mc_client_color_array_retain(array) != MC_CLIENT_COLOR_ARRAY_OK) {
        o->failed = true;
        return 0;
    }
    o->observed[o->observed_count++] = array;
    return o->observed_count;
}

static inline void obs_array(observation *o, mc_client_color_array *array) {
    if (array == NULL) {
        fputs("null", stdout);
        return;
    }
    mc_client_color_array_state state;
    if (!mc_client_color_array_inspect(array, &state) ||
        state.phase != MC_CLIENT_COLOR_ARRAY_READY) {
        o->failed = true;
        fputs("null", stdout);
        return;
    }
    printf("{\"id\":%zu,\"length\":%zu,\"hex\":\"", obs_identity(o, array), state.length);
    for (size_t i = 0; i < state.length; ++i)
        printf("%08" PRIx32, state.bits[i]);
    fputs("\"}", stdout);
}

static inline void obs_site(mc_client_graphics_site site) {
    fputs("{\"class\":", stdout);
    obs_string(site.class_name);
    fputs(",\"method\":", stdout);
    obs_string(site.method);
    fputs(",\"descriptor\":", stdout);
    obs_string(site.descriptor);
    printf(",\"pc\":%" PRId32 "}", site.pc);
}

static inline void obs_error(const mc_client_texture_lookup_error *e) {
    printf("{\"status\":%d,\"site\":", (int)e->status);
    obs_site(e->site);
    fputs(",\"dependency\":", stdout);
    obs_site(e->dependency);
    fputs(",\"operation\":", stdout);
    obs_string(e->operation);
    printf(",\"native_policy\":%s,\"caught_source_io\":%s,\"frames\":[",
           e->native_policy ? "true" : "false", e->caught_source_io ? "true" : "false");
    for (size_t i = 0; i < e->frame_count; ++i) {
        if (i != 0)
            putchar(',');
        obs_site(e->frames[i]);
    }
    printf("],\"pack_status\":%d,\"cleanup_pack_status\":%d,\"array_status\":%d,\"array_site\":",
           (int)e->pack.status, (int)e->cleanup_pack.status, (int)e->array.status);
    obs_site(e->array.site);
    printf(",\"texture_status\":%d,\"texture_pc\":%u,\"buffer_status\":%d,\"owner_status\":%d}",
           (int)e->texture.status, e->texture.source_pc, (int)e->buffer.status, (int)e->owner);
}

static inline void obs_state(observation *o) {
    const mc_client_render_engine_fields *f = &o->engine->fields;
    fputs("{\"b\":[", stdout);
    for (size_t i = 0; i < f->b->count; ++i) {
        const mc_client_render_string_id_entry *entry = &f->b->string_ids[i];
        if (i != 0)
            putchar(',');
        fputs("{\"key\":", stdout);
        obs_utf16(entry->units, entry->length, entry->is_null);
        printf(",\"id\":%" PRIu32 "}", entry->id);
    }
    fputs("],\"c\":[", stdout);
    for (size_t i = 0; i < f->c->count; ++i) {
        const mc_client_render_color_array_entry *entry = &f->c->color_arrays[i];
        if (i != 0)
            putchar(',');
        fputs("{\"key\":", stdout);
        obs_utf16(entry->units, entry->length, entry->is_null);
        printf(",\"id\":%zu}", obs_identity(o, entry->array));
    }
    printf("],\"d_count\":%zu,\"j\":%s,\"k\":%s,\"ledger\":[", f->d->count, f->j ? "true" : "false",
           f->k ? "true" : "false");
    for (size_t i = 0; i < o->classes->b->count; ++i) {
        int32_t id;
        memcpy(&id, (const uint8_t *)(const void *)o->classes->b->entries + i * sizeof(id),
               sizeof(id));
        if (i != 0)
            putchar(',');
        printf("%" PRId32, id);
    }
    int32_t first;
    memcpy(&first, f->e->bytes->data, sizeof(first));
    printf("],\"e\":[%zu,%zu,%zu],\"e0\":%" PRId32
           ",\"f\":[%zu,%zu,%zu],\"l_null\":%s,\"selected_null\":%s}",
           f->e->position, f->e->limit, f->e->capacity, first, f->f->position, f->f->limit,
           f->f->capacity, f->l == NULL ? "true" : "false",
           o->packs->selected == NULL ? "true" : "false");
}

static inline void obs_event(observation *o, const char *label) {
    if (o->event_started)
        putchar(',');
    o->event_started = true;
    fputs("{\"label\":", stdout);
    obs_string(label);
}

static inline void obs_begin(observation *o, const char *mode, const char *profile,
                             const char *device) {
    fputs("LOOKUP_RECORD {\"schema\":1,\"runtime\":\"native\",\"mode\":", stdout);
    obs_string(mode);
    fputs(",\"profile\":", stdout);
    obs_string(profile);
    printf(",\"setup\":{\"actual_app\":true,\"app_z_alias\":true,\"pack\":\"%s\",\"engine_i_null\":"
           "%s},\"device\":",
           o->pack_class, o->engine->fields.i == NULL ? "true" : "false");
    fputs(device == NULL ? "null" : device, stdout);
    fputs(",\"events\":[", stdout);
}

static inline bool obs_open(observation *o, const char *resources, const char *directory,
                            bool color) {
    memset(o, 0, sizeof(*o));
    o->app_error = calloc(1, sizeof(*o->app_error));
    o->error = calloc(1, sizeof(*o->error));
    OBS_NEED(o->app_error != NULL && o->error != NULL);
    OBS_NEED(mc_java_random_profile_create(SIZE_MAX, &o->services.profile) ==
             MC_JAVA_RANDOM_PROFILE_OK);
    OBS_NEED(mc_materials_create(&o->materials));
    OBS_NEED(mc_resource_files_create(resources, &o->files) == MC_REGISTRY_OK);
    const mc_registry_options registry = {.side = MC_REGISTRY_CLIENT,
                                          .random_profile = o->services.profile,
                                          .materials = o->materials,
                                          .resources = mc_resource_files_source(o->files)};
    OBS_NEED(mc_registry_classes_allocate(&registry, &o->services.registry, NULL) ==
             MC_REGISTRY_OK);
    OBS_NEED(mc_entity_classes_create(MC_ENTITY_CLIENT, o->services.profile, NULL,
                                      &o->services.entities) == MC_ENTITY_OK);
    OBS_NEED(mc_client_render_classes_create(o->services.entities, o->services.profile,
                                             &o->services.render_classes) == MC_CLIENT_MODEL_OK);
    OBS_NEED(mc_client_render_engine_classes_create(NULL, &o->classes) ==
             MC_CLIENT_RENDER_ENGINE_OK);
    OBS_NEED(mc_client_loading_classes_create(o->classes, &o->services.gui_classes) ==
             MC_CLIENT_LOADING_OK);
    OBS_NEED(mc_client_application_classes_create(NULL, &o->services.application_classes, NULL) ==
             MC_CLIENT_APPLICATION_OWNER_OK);
    OBS_NEED(mc_sound_backend_create(o->services.profile, NULL, &o->services.sound_backend, NULL) ==
             MC_SOUND_BACKEND_OK);
    mc_sound_manager_backend backend;
    OBS_NEED(mc_sound_backend_manager_interface(o->services.sound_backend, &backend));
    OBS_NEED(mc_sound_manager_shared_create(&backend, &o->services.sound_shared, NULL) ==
             MC_SOUND_OK);
    OBS_NEED(mc_client_application_constructor_allocate(&o->services, NULL, &o->app,
                                                        o->app_error) == MC_CLIENT_APP_OK);
    const mc_client_application_constructor_arguments arguments = {.width = 854, .height = 480};
    OBS_NEED(mc_client_application_constructor_construct(o->app, &arguments, o->app_error) ==
             MC_CLIENT_APP_OK);
    OBS_NEED(mc_game_settings_create_file(o->app, directory, NULL, NULL, &o->app->settings, NULL,
                                          NULL) == MC_GAME_SETTINGS_OK);
    OBS_NEED(mc_client_texture_pack_list_allocate(mc_resource_files_source(o->files), NULL,
                                                  &o->packs) == MC_TEXTURE_PACK_OK);
    OBS_NEED(mc_client_texture_pack_list_construct(o->packs, o->app, directory, NULL) ==
             MC_TEXTURE_PACK_OK);
    OBS_NEED(o->packs->selected != NULL);
    o->pack_class = o->packs->selected->kind == MC_TEXTURE_PACK_ZIP ? "vp" : "pa";
    OBS_NEED(mc_client_default_font_create(32u * 1024u * 1024u, &o->font) == MC_CLIENT_FONT_OK);
    OBS_NEED(mc_client_render_engine_allocate(o->classes, o->font, NULL, &o->engine) ==
             MC_CLIENT_RENDER_ENGINE_OK);
    OBS_NEED(mc_client_render_engine_construct(o->engine, o->packs, color ? NULL : o->app->settings,
                                               NULL) == MC_CLIENT_RENDER_ENGINE_OK);
    return true;
}

static inline bool obs_close(observation *o) {
    if (o->engine != NULL) {
        OBS_NEED(mc_client_render_engine_retire_lookup(o->engine, o->error) == MC_CLIENT_LOOKUP_OK);
        OBS_NEED(mc_client_texture_lookup_free_metadata(o->engine));
        OBS_NEED(mc_client_render_engine_release(o->engine) == MC_CLIENT_RENDER_ENGINE_OK);
        o->engine = NULL;
    }
    if (o->classes != NULL)
        OBS_NEED(mc_client_render_engine_retire_resources(o->classes, NULL) ==
                 MC_CLIENT_RENDER_TEXTURE_OK);
    if (o->display != NULL) {
        OBS_NEED(mc_client_display_destroy(o->display, NULL));
        o->display = NULL;
    }
    if (o->packs != NULL) {
        OBS_NEED(mc_client_texture_pack_list_release(o->packs, NULL) == MC_TEXTURE_PACK_OK);
        o->packs = NULL;
    }
    if (o->app != NULL && o->app->settings != NULL)
        OBS_NEED(mc_game_settings_retire(&o->app->settings, NULL) == MC_GAME_SETTINGS_OK);
    if (o->app != NULL) {
        mc_client_application_construction_status status =
            mc_client_application_constructor_retire(&o->app, o->app_error);
        if (status == MC_CLIENT_APP_BUSY) {
            OBS_NEED(mc_client_application_classes_release(&o->services.application_classes,
                                                           NULL) == MC_CLIENT_APPLICATION_OWNER_OK);
            status = mc_client_application_constructor_retire(&o->app, o->app_error);
        }
        OBS_NEED(status == MC_CLIENT_APP_OK);
    }
    if (o->services.application_classes != NULL)
        OBS_NEED(mc_client_application_classes_release(&o->services.application_classes, NULL) ==
                 MC_CLIENT_APPLICATION_OWNER_OK);
    if (o->services.gui_classes != NULL)
        OBS_NEED(mc_client_loading_classes_release(o->services.gui_classes) ==
                 MC_CLIENT_LOADING_OK);
    if (o->classes != NULL)
        OBS_NEED(mc_client_render_engine_classes_release(o->classes) == MC_CLIENT_RENDER_ENGINE_OK);
    if (o->font != NULL)
        OBS_NEED(mc_client_default_font_release(o->font) == MC_CLIENT_FONT_OK);
    for (size_t i = 0; i < o->observed_count; ++i)
        OBS_NEED(mc_client_color_array_release(&o->observed[i], NULL) == MC_CLIENT_COLOR_ARRAY_OK);
    if (o->services.sound_shared != NULL)
        OBS_NEED(mc_sound_manager_shared_retire(&o->services.sound_shared, NULL) == MC_SOUND_OK);
    if (o->services.sound_backend != NULL)
        OBS_NEED(mc_sound_backend_retire(&o->services.sound_backend, NULL) == MC_SOUND_BACKEND_OK);
    if (o->services.render_classes != NULL)
        OBS_NEED(mc_client_render_classes_free(o->services.render_classes) == MC_CLIENT_MODEL_OK);
    if (o->services.entities != NULL)
        OBS_NEED(mc_entity_classes_free(o->services.entities) == MC_ENTITY_OK);
    if (o->services.registry != NULL)
        OBS_NEED(mc_registry_classes_free(o->services.registry, NULL) == MC_REGISTRY_OK);
    if (o->files != NULL)
        OBS_NEED(mc_resource_files_free(o->files) == MC_REGISTRY_OK);
    if (o->materials != NULL)
        mc_materials_destroy(o->materials);
    if (o->services.profile != NULL)
        mc_java_random_profile_free(o->services.profile);
    free(o->error);
    free(o->app_error);
    return !o->failed;
}

static inline bool obs_color(observation *o, const char *label, const char *name) {
    uint16_t units[128];
    mc_client_color_array *array = NULL;
    mc_client_texture_lookup_status status =
        mc_client_render_engine_colors(o->engine, NULL, obs_key(name, units), &array, o->error);
    obs_event(o, label);
    fputs(",\"outcome\":", stdout);
    obs_string(status == MC_CLIENT_LOOKUP_OK               ? "return"
               : status == MC_CLIENT_LOOKUP_NULL_REFERENCE ? "throw"
                                                           : "frontier");
    fputs(",\"exception\":", stdout);
    obs_string(status == MC_CLIENT_LOOKUP_NULL_REFERENCE ? "java.lang.NullPointerException" : NULL);
    fputs(",\"array\":", stdout);
    obs_array(o, array);
    fputs(",\"state\":", stdout);
    obs_state(o);
    fputs(",\"native\":", stdout);
    obs_error(o->error);
    putchar('}');
    if (array != NULL)
        OBS_NEED(mc_client_color_array_release(&array, NULL) == MC_CLIENT_COLOR_ARRAY_OK);
    OBS_NEED(mc_client_render_engine_retire_lookup(o->engine, o->error) == MC_CLIENT_LOOKUP_OK);
    return true;
}
#endif
