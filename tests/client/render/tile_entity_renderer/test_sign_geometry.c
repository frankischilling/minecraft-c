/* Direct actual PO/RF/SJ observations. Saved inputs only, plus declared public edit-row stores.
 * No HY fixture is executed or changed. Terminal null-Font GL prefix is never repaired. */
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#endif
int mc_sign_geometry_startup_fixture_main(int argc, char **argv);
#define main mc_sign_geometry_startup_fixture_main
#include "../../startup/test_application_startup.c"
#undef main

#include "block/registry/registry_internal.h"
#include "client/display/camera_surface.h"
#include "client/display/fog_surface.h"
#include "client/display/source_surface.h"
#include "client/gui/bitmap_font.h"
#include "client/render/tile_entity_renderer/internal.h"
#include "client/render_engine/internal.h"
#include "client/render_engine/lighting.h"
#include "client/render_engine/nw.h"
#include "client/render_engine/texture_lookup.h"
#include "mc/client/render/tile_entity_renderer/draw.h"
#include "mc/core/math/classes.h"
#include "mc/entity/mob/construction.h"
#include "mc/world/runtime/blocks.h"
#include "mc/world/runtime/source_construction.h"
#include "mc/world/storage/source_regions.h"
#include "world/chunk/object_state_internal.h"
#include "world/runtime/chunk_provider_internal.h"
#include "world/runtime/construction_internal.h"
#include "world/runtime/runtime_internal.h"
#include "world/runtime/tile_entities/tile_entities_internal.h"
#include "world/storage/objects_internal.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <errno.h>
#include <inttypes.h>

typedef struct sign_geometry_input {
    unsigned int index;
    char folder[32], name[128], kind[128], lines[4][61];
    int32_t block, metadata, edit;
    uint32_t partial;
    bool null_font;
    int64_t seed;
} sign_geometry_input;

typedef struct sign_geometry_world {
    mc_world_store *store;
    mc_world_runtime *world;
    mc_chunk_state *chunks[4];
    mc_chunk_provider_fields provider;
    mc_tile_entity *tile;
    mc_entity *viewer;
    mc_tile_entity_name **lines;
} sign_geometry_world;

typedef struct sign_geometry_fixture {
    fixture runtime;
    client_application *app;
    mc_client_texture_pack_list *packs;
    mc_resource_files *textures;
    mc_client_default_font *font;
    mc_client_bitmap_font *bitmap;
    mc_client_render_engine *engine;
    mc_client_display *display;
    mc_math_classes *math;
    mc_world_runtime_classes *world_classes;
    mc_java_files *files;
    mc_java_logging *logging;
    mc_java_file *root;
    mc_world_source_regions *regions;
    mc_world_store_source_classes *handlers;
    mc_client_tile_renderer_classes *renderers;
    mc_client_tile_renderer_dispatcher *manager;
    mc_client_tile_renderer *po;
    mc_client_sign_model *model;
    mc_client_model_part *board, *pole;
    mc_client_tessellator *nw;
    mc_client_tile_renderer_draw_call *call;
    mc_client_tile_renderer_draw_error *draw_error;
    mc_client_texture_lookup_error *lookup_error;
    mc_live_tile_entity_error *tile_error;
    mc_world_source_error *source_error;
    mc_world_source_region_error *region_error;
    sign_geometry_world worlds[36];
    FILE *output;
    size_t rows;
} sign_geometry_fixture;

static uint32_t sg_fbits(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static uint64_t sg_dbits(double value) {
    uint64_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static void sg_string(FILE *out, const char *value) {
    if (value == NULL) {
        fputs("null", out);
        return;
    }
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

static mc_world_info_name *sg_text(const char *ascii) {
    const size_t length = strlen(ascii);
    uint16_t *units = malloc((length + 1) * sizeof(*units));
    if (units == NULL)
        return NULL;
    for (size_t i = 0; i < length; ++i) {
        if ((unsigned char)ascii[i] >= 128u) {
            free(units);
            return NULL;
        }
        units[i] = (unsigned char)ascii[i];
    }
    mc_world_info_name *name = NULL;
    const mc_world_info_status status =
        mc_world_info_name_copy((mc_nbt_string){units, length}, SIZE_MAX, &name);
    free(units);
    return status == MC_WORLD_INFO_OK ? name : NULL;
}

static bool sg_integer(const char *text, int64_t low, int64_t high, int64_t *out) {
    char *end = NULL;
    errno = 0;
    const long long value = strtoll(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || value < low || value > high)
        return false;
    *out = (int64_t)value;
    return true;
}

static bool sg_boolean(const char *text, bool *out) {
    if (strcmp(text, "true") == 0)
        *out = true;
    else if (strcmp(text, "false") == 0)
        *out = false;
    else
        return false;
    return true;
}

static void sg_nw(FILE *out, const mc_client_nw_fields *f) {
    fprintf(out,
            "{\"h\":%" PRId32 ",\"i\":\"%016" PRIx64 "\",\"j\":\"%016" PRIx64 "\",\"k\":%" PRId32
            ",\"l\":%s,\"m\":%s,\"n\":%s,\"o\":%" PRId32 ",\"p\":%" PRId32
            ",\"q\":%s,\"r\":%" PRId32 ",\"s\":\"%016" PRIx64 "\",\"t\":\"%016" PRIx64
            "\",\"u\":\"%016" PRIx64 "\",\"v\":%" PRId32 ",\"w\":%s,\"x\":%s,\"z\":%" PRId32
            ",\"A\":%" PRId32 ",\"B\":%" PRId32 "}",
            f->h, sg_dbits(f->i), sg_dbits(f->j), f->k, f->l ? "true" : "false",
            f->m ? "true" : "false", f->n ? "true" : "false", f->o, f->p, f->q ? "true" : "false",
            f->r, sg_dbits(f->s), sg_dbits(f->t), sg_dbits(f->u), f->v, f->w ? "true" : "false",
            f->x ? "true" : "false", f->z, f->A, f->B);
}

static void sg_part(FILE *out, const mc_client_model_part *p) {
    fprintf(out,
            "{\"a\":\"%08" PRIx32 "\",\"b\":\"%08" PRIx32 "\",\"c\":\"%08" PRIx32
            "\",\"d\":\"%08" PRIx32 "\",\"e\":\"%08" PRIx32 "\",\"f\":\"%08" PRIx32
            "\",\"g\":%s,\"h\":%s,\"i\":%s,\"l\":%" PRId32 ",\"m\":%" PRId32
            ",\"n\":%s,\"o\":%" PRId32 "}",
            sg_fbits(p->a), sg_fbits(p->b), sg_fbits(p->c), sg_fbits(p->d), sg_fbits(p->e),
            sg_fbits(p->f), p->g ? "true" : "false", p->h ? "true" : "false",
            p->i ? "true" : "false", p->l, p->m, p->n ? "true" : "false", p->o);
}

static int sg_font(sign_geometry_fixture *f, mc_client_bitmap_font *font) {
    if (font == NULL) {
        fputs("null", f->output);
        return 0;
    }
    mc_client_bitmap_font_state state;
    mc_client_direct_buffer_state buffer;
    CHECK(mc_client_bitmap_font_inspect(font, &state) &&
          state.phase == MC_CLIENT_BITMAP_FONT_READY && state.fields.b != NULL &&
          mc_client_direct_int_buffer_inspect(state.fields.d, &buffer) &&
          buffer.position <= buffer.limit && buffer.limit <= buffer.capacity &&
          buffer.capacity == 1024 && buffer.storage != NULL && buffer.storage_bytes >= 4096 &&
          buffer.direct && buffer.native_order);
    fprintf(f->output,
            "{\"class\":\"sj\",\"texture\":%" PRId32 ",\"list_base\":%" PRId32 ",\"widths\":[",
            state.fields.a, state.fields.c);
    for (size_t i = 0; i < 256; ++i)
        fprintf(f->output, "%s%" PRId32, i ? "," : "", state.fields.b[i]);
    const uint16_t endian = 1;
    fprintf(
        f->output,
        "],\"buffer\":{\"class\":\"native.direct-int-buffer\",\"direct\":true,\"read_only\":false,"
        "\"position\":%zu,\"limit\":%zu,\"capacity\":%zu,\"order\":\"%s\",\"active_words\":[",
        buffer.position, buffer.limit, buffer.capacity,
        *(const uint8_t *)&endian ? "LITTLE_ENDIAN" : "BIG_ENDIAN");
    const int32_t *words = buffer.storage;
    for (size_t i = buffer.position; i < buffer.limit; ++i)
        fprintf(f->output, "%s\"%08" PRIx32 "\"", i == buffer.position ? "" : ",",
                (uint32_t)words[i]);
    fputs("]}}", f->output);
    return 0;
}

/* Source legacy matrices/attributes belong to the real Display facade, not
 * GL3.3 core's driver state. This passive snapshot neither initializes that
 * facade nor creates compatibility defaults. Unrepresented legacy cells are
 * explicit observation frontiers, rather than zero-filled query results. */
static void sg_vector(FILE *out, const float *values, size_t length) {
    fputc('[', out);
    for (size_t i = 0; i < length; ++i)
        fprintf(out, "%s\"%08" PRIx32 "\"", i ? "," : "", sg_fbits(values[i]));
    fputc(']', out);
}

static int sg_gl(sign_geometry_fixture *f) {
    CHECK(mc_client_display_make_current(f->display, NULL));
    mc_client_source_draw_state draw = {0};
    mc_client_source_fog_state fog = {0};
    float projection[16], modelview[16];
    int32_t mode;
    size_t modelview_depth, projection_depth;
    const bool draw_ready = mc_client_display_source_draw_inspect(f->display, &draw);
    const bool fog_ready = mc_client_display_source_fog_inspect(f->display, &fog);
    const bool matrices_ready =
        mc_client_display_camera_matrices_inspect(f->display, projection, modelview);
    const bool stack_ready = mc_client_display_camera_stack_inspect(
        f->display, &mode, &modelview_depth, &projection_depth);
    CHECK(draw_ready == fog_ready && draw_ready == matrices_ready && draw_ready == stack_ready);
    FILE *out = f->output;
    fputs("{\"representation\":\"core-driver-and-source-facade-v2\",\"unrepresented\":[\"texture_"
          "matrix\",\"texture_depth\",\"list_index\",\"list_mode\"],\"core\":{",
          out);
    /* All queried enums are retained in the actual GL3.3 core context. The RGB
     * blend factors are the corresponding Source glBlendFunc query operands. */
    {
        GLint value = 0;
        glGetIntegerv(GL_DEPTH_FUNC, &value);
        fprintf(out, "\"depth_func\":%d", value);
    }
    {
        GLint value = 0;
        glGetIntegerv(0x80C9, &value);
        fprintf(out, ",\"blend_src\":%d", value);
    }
    {
        GLint value = 0;
        glGetIntegerv(0x80C8, &value);
        fprintf(out, ",\"blend_dst\":%d", value);
    }
    {
        GLint value = 0;
        glGetIntegerv(GL_CULL_FACE_MODE, &value);
        fprintf(out, ",\"cull_mode\":%d", value);
    }
    {
        GLint value = 0;
        glGetIntegerv(GL_FRONT_FACE, &value);
        fprintf(out, ",\"front_face\":%d", value);
    }
    {
        GLint value = 0;
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &value);
        fprintf(out, ",\"bound_texture\":%d", value);
    }
    fprintf(out, ",\"depth_test\":%s", glIsEnabled(GL_DEPTH_TEST) ? "true" : "false");
    fprintf(out, ",\"blend\":%s", glIsEnabled(GL_BLEND) ? "true" : "false");
    fprintf(out, ",\"cull\":%s", glIsEnabled(GL_CULL_FACE) ? "true" : "false");
    GLboolean depth = GL_FALSE;
    glGetBooleanv(GL_DEPTH_WRITEMASK, &depth);
    fprintf(out, ",\"depth_write\":%s", depth ? "true" : "false");
    const GLenum error = glGetError();
    fprintf(out, ",\"error\":%u},\"source\":{\"initialized\":%s", (unsigned int)error,
            draw_ready ? "true" : "false");
    if (draw_ready) {
        fprintf(out,
                ",\"matrix_mode\":%" PRId32 ",\"modelview_depth\":%zu,\"projection_depth\":%zu,"
                "\"color_known\":%s,\"normal_known\":%s",
                mode, modelview_depth, projection_depth, draw.color_known ? "true" : "false",
                fog.normal_known ? "true" : "false");
        fprintf(out,
                ",\"lighting\":%s,\"light0\":%s,\"light1\":%s,\"color_material\":%s,\"shade\":%d,"
                "\"normalize\":%s,\"rescale_normal\":%s,\"texture2d\":%s",
                draw.lighting_enabled ? "true" : "false", fog.lights[0] ? "true" : "false",
                fog.lights[1] ? "true" : "false", fog.color_material ? "true" : "false",
                draw.shade_flat ? GL_FLAT : GL_SMOOTH, fog.normalize_enabled ? "true" : "false",
                fog.rescale_normal_enabled ? "true" : "false",
                draw.texture_enabled ? "true" : "false");
        fputs(",\"modelview\":", out);
        sg_vector(out, modelview, 16);
        fputs(",\"projection\":", out);
        sg_vector(out, projection, 16);
        fputs(",\"current_color\":", out);
        sg_vector(out, draw.color, 4);
        fputs(",\"current_normal\":", out);
        sg_vector(out, fog.normal, 3);
    }
    fputs("}}", out);
    CHECK(error == GL_NO_ERROR);
    return 0;
}

static void sg_tile_list(FILE *out, const mc_world_reference_list *list,
                         const mc_tile_entity *tile) {
    fputc('[', out);
    for (size_t i = 0; i < list->count; ++i)
        fprintf(out, "%s{\"class\":\"yk\",\"selected\":%s}", i ? "," : "",
                list->items[i] == tile ? "true" : "false");
    fputc(']', out);
}

static int sg_snapshot(sign_geometry_fixture *f, sign_geometry_world *w,
                       const sign_geometry_input *input) {
    mc_tile_entity_snapshot tile;
    mc_world_info_fields info;
    mc_client_nw_state nw;
    int32_t id = -1, meta = -1;
    CHECK(mc_tile_entity_get_snapshot(w->tile, &tile) == MC_TILE_ENTITY_OK &&
          tile.kind == MC_TILE_ENTITY_SIGN &&
          mc_world_info_fields_get(w->world->fields.x, &info) == MC_WORLD_INFO_OK &&
          mc_world_runtime_block_get(w->world, 8, 64, 8, &id, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_metadata_get(w->world, 8, 64, 8, &meta, NULL) == MC_WORLD_STORAGE_OK &&
          mc_client_nw_inspect(f->runtime.graphics, &nw) && nw.fields.o >= 0 &&
          nw.fields.o <= 4096 && nw.fields.g != NULL);
    FILE *out = f->output;
    fprintf(out,
            "{\"tile\":{\"class\":\"yk\",\"world_same\":%s,\"xyz\":[%" PRId32 ",%" PRId32
            ",%" PRId32 "],\"ow\":{\"e\":%" PRId32 ",\"f\":%" PRId32 ",\"g\":%" PRId32
            ",\"h\":%s},\"yk\":{\"b\":%" PRId32 ",\"c\":%s},\"text_array_same\":%s,\"lines\":[",
            tile.world == w->world ? "true" : "false", tile.x, tile.y, tile.z, tile.x, tile.y,
            tile.z, tile.invalid ? "true" : "false", tile.sign_edit_row,
            tile.sign_read_flag ? "true" : "false",
            w->tile->state.sign.lines == w->lines ? "true" : "false");
    for (int32_t i = 0; i < 4; ++i) {
        mc_tile_entity_name *line = NULL;
        mc_tile_entity_name_view v;
        CHECK(mc_tile_entity_sign_get_line(w->tile, i, &line) == MC_TILE_ENTITY_OK &&
              mc_tile_entity_name_get_view(line, &v) == MC_TILE_ENTITY_OK);
        if (i)
            fputc(',', out);
        if (v.is_null)
            fputs("null", out);
        else {
            fprintf(out, "{\"length\":%zu,\"units\":[", v.text.length);
            for (size_t j = 0; j < v.text.length; ++j)
                fprintf(out, "%s\"%04x\"", j ? "," : "", (unsigned int)v.text.units[j]);
            fputs("]}", out);
        }
        mc_tile_entity_name_release(line);
    }
    fprintf(out,
            "]},\"world\":{\"time\":%" PRId64
            ",\"iterating\":%s,\"entities\":%zu,\"players\":%zu,\"block\":%" PRId32
            ",\"metadata\":%" PRId32 ",\"active_tiles\":",
            info.value.time, w->world->fields.L ? "true" : "false", w->world->fields.b->count,
            w->world->fields.d->count, id, meta);
    sg_tile_list(out, w->world->fields.c, w->tile);
    fputs(",\"pending_tiles\":", out);
    sg_tile_list(out, w->world->fields.G, w->tile);
    mc_world_chunk *chunk = mc_world_store_chunk_object(w->store, w->chunks[0]);
    CHECK(chunk != NULL && chunk->l != NULL);
    fputs(",\"mapped_tiles\":[", out);
    for (size_t i = 0; i < chunk->l->count; ++i) {
        mc_live_tile_entity_map_entry e;
        CHECK(mc_live_tile_entities_map_entry_at(chunk, i, &e) == MC_WORLD_STORAGE_OK);
        fprintf(out, "%s{\"class\":\"yk\",\"selected\":%s}", i ? "," : "",
                e.tile_entity == w->tile ? "true" : "false");
    }
    fprintf(out, "]},\"model\":{\"same_model\":%s,\"same_board\":%s,\"same_pole\":%s,\"board\":",
            f->po->sign_model == f->model ? "true" : "false",
            f->model->a == f->board ? "true" : "false", f->model->b == f->pole ? "true" : "false");
    sg_part(out, f->board);
    fputs(",\"pole\":", out);
    sg_part(out, f->pole);
    const mc_client_tile_renderer_fields *m = &f->manager->fields;
    fprintf(out,
            "},\"manager\":{\"world_same\":%s,\"engine_same\":%s,\"font_same\":%s,\"primitives\":{"
            "\"h\":\"%08" PRIx32 "\",\"i\":\"%08" PRIx32 "\",\"j\":\"%016" PRIx64
            "\",\"k\":\"%016" PRIx64 "\",\"l\":\"%016" PRIx64 "\"}},\"font\":",
            m->f == w->world ? "true" : "false", m->e == f->engine ? "true" : "false",
            m->n == (input->null_font ? NULL : f->bitmap) ? "true" : "false", sg_fbits(m->h),
            sg_fbits(m->i), sg_dbits(m->j), sg_dbits(m->k), sg_dbits(m->l));
    CHECK(sg_font(f, m->n) == 0);
    static const uint16_t key[] = {'/', 'i', 't', 'e', 'm', '/', 's',
                                   'i', 'g', 'n', '.', 'p', 'n', 'g'};
    bool found = false;
    mc_client_texture texture = 0;
    size_t entries = SIZE_MAX;
    CHECK(mc_client_render_engine_cached_texture(
              f->engine, (mc_texture_pack_text){key, sizeof(key) / sizeof(*key), false}, &found,
              &texture) &&
          mc_client_render_map_count(f->engine->fields.b, &entries) &&
          entries == (found ? 1u : 0u));
    fputs(",\"textures\":{", out);
    if (found)
        fprintf(out, "\"/item/sign.png\":%" PRIu32, texture);
    fputs("},\"nw\":", out);
    sg_nw(out, &nw.fields);
    fputs(",\"nw_active_words\":[", out);
    for (int32_t i = 0; i < nw.fields.o; ++i)
        fprintf(out, "%s\"%08" PRIx32 "\"", i ? "," : "", (uint32_t)nw.fields.g[i]);
    fputs("],\"gl\":", out);
    CHECK(sg_gl(f) == 0);
    fputc('}', out);
    return 0;
}

static int sg_input(char *line, unsigned int expected, sign_geometry_input *out) {
    char *cells[14], *cursor = line;
    for (size_t i = 0; i < 14; ++i) {
        cells[i] = cursor;
        char *next = strchr(cursor, '\t');
        CHECK((i == 13) == (next == NULL));
        if (next) {
            *next = '\0';
            cursor = next + 1;
        }
    }
    int64_t value;
    CHECK(sg_integer(cells[0], 0, 35, &value) && value == expected);
    out->index = (unsigned int)value;
    char folder[32];
    CHECK(snprintf(folder, sizeof(folder), "case%02u", out->index) > 0 &&
          strcmp(folder, cells[1]) == 0);
    memcpy(out->folder, folder, strlen(folder) + 1);
    CHECK(sg_integer(cells[2], 63, 68, &value) && (value == 63 || value == 68));
    out->block = (int32_t)value;
    CHECK(sg_integer(cells[3], 0, 15, &value));
    out->metadata = (int32_t)value;
    CHECK(sg_integer(cells[4], -1, 4, &value));
    out->edit = (int32_t)value;
    CHECK(sg_boolean(cells[5], &out->null_font) && (!out->null_font || out->index == 35) &&
          strlen(cells[6]) == 8);
    uint32_t bits = 0;
    for (size_t i = 0; i < 8; ++i) {
        const unsigned char c = (unsigned char)cells[6][i];
        CHECK((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'));
        bits = (bits << 4u) | (uint32_t)(c <= '9' ? c - '0' : c - 'a' + 10);
    }
    out->partial = bits;
    CHECK(strlen(cells[7]) < sizeof(out->name) &&
          sg_integer(cells[8], INT64_MIN, INT64_MAX, &out->seed) &&
          strlen(cells[9]) < sizeof(out->kind));
    memcpy(out->name, cells[7], strlen(cells[7]) + 1);
    memcpy(out->kind, cells[9], strlen(cells[9]) + 1);
    for (size_t row = 0; row < 4; ++row) {
        size_t n = strlen(cells[10 + row]);
        CHECK(n <= 60 && n % 4 == 0);
        for (size_t i = 0; i < n; ++i)
            CHECK((cells[10 + row][i] >= '0' && cells[10 + row][i] <= '9') ||
                  (cells[10 + row][i] >= 'a' && cells[10 + row][i] <= 'f'));
        memcpy(out->lines[row], cells[10 + row], n + 1);
    }
    return 0;
}

static int sg_world_open(sign_geometry_fixture *f, const sign_geometry_input *input) {
    sign_geometry_world *w = &f->worlds[input->index];
    mc_world_info_name *folder = sg_text(input->folder), *name = sg_text(input->name);
    CHECK(folder && name &&
          mc_world_store_source_allocate(f->handlers, &w->store, f->source_error) ==
              MC_WORLD_SOURCE_OK &&
          mc_world_store_source_construct(w->store, f->root, folder, false, f->source_error) ==
              MC_WORLD_SOURCE_OK);
    mc_world_info_name_release(folder);
    mc_world_runtime_construction_limits limits = mc_world_runtime_default_construction_limits();
    limits.max_provider_chunks = 4;
    CHECK(mc_world_runtime_allocate_world(MC_WORLD_RUNTIME_CLIENT_LOAD_OR_CREATE, f->world_classes,
                                          &limits, &w->world, NULL) == MC_WORLD_STORAGE_OK);
    const mc_world_runtime_world_arguments args = {
        .store = w->store, .name = name, .seed = input->seed};
    const mc_world_storage_status status = mc_world_runtime_construct_world(w->world, &args, NULL);
    mc_world_info_name_release(name);
    CHECK(status == MC_WORLD_STORAGE_OK &&
          mc_chunk_provider_bind_fields(w->world->fields.v, &w->provider, NULL) ==
              MC_WORLD_STORAGE_OK);
    mc_world_runtime_source_construction_state source;
    CHECK(mc_world_runtime_source_construction_inspect(w->world, &source) &&
          source.store == w->store && source.metadata_returned && !source.loaded_info_was_null);
    for (int32_t x = 0; x < 2; ++x)
        for (int32_t z = 0; z < 2; ++z) {
            mc_chunk_state **chunk = &w->chunks[(size_t)(x * 2 + z)];
            CHECK(mc_world_runtime_get(w->world, x, z, chunk) == MC_CHUNK_ACQUIRE_OK && *chunk &&
                  !(*chunk)->blank && (*chunk)->active && (*chunk)->terrain_populated);
        }
    int32_t id = -1, meta = -1;
    CHECK(mc_world_runtime_block_get(w->world, 8, 64, 8, &id, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_metadata_get(w->world, 8, 64, 8, &meta, NULL) == MC_WORLD_STORAGE_OK &&
          id == input->block && meta == input->metadata &&
          mc_live_tile_entities_get(w->world, 8, 64, 8, &w->tile, f->tile_error) ==
              MC_WORLD_STORAGE_OK &&
          w->tile);
    mc_tile_entity_snapshot tile;
    CHECK(mc_tile_entity_get_snapshot(w->tile, &tile) == MC_TILE_ENTITY_OK &&
          tile.kind == MC_TILE_ENTITY_SIGN && tile.world == w->world && tile.sign_line_count == 4 &&
          tile.sign_edit_row == -1 && !tile.sign_read_flag && w->world->fields.c->count == 1 &&
          w->world->fields.c->items[0] == w->tile && w->world->fields.G->count == 0);
    w->lines = w->tile->state.sign.lines;
    for (int32_t row = 0; row < 4; ++row) {
        mc_tile_entity_name *line = NULL;
        mc_tile_entity_name_view v;
        CHECK(mc_tile_entity_sign_get_line(w->tile, row, &line) == MC_TILE_ENTITY_OK &&
              mc_tile_entity_name_get_view(line, &v) == MC_TILE_ENTITY_OK && !v.is_null);
        char units[61] = {0};
        CHECK(v.text.length <= 15);
        for (size_t i = 0; i < v.text.length; ++i)
            CHECK(snprintf(units + i * 4, 5, "%04x", (unsigned int)v.text.units[i]) == 4);
        const bool equal = strcmp(units, input->lines[row]) == 0;
        mc_tile_entity_name_release(line);
        CHECK(equal);
    }
    /* This mirrors only the original declared public editor-row prestate.
     * Text, private read flag, model, World lists and native cells are untouched. */
    if (input->edit != -1)
        CHECK(mc_tile_entity_sign_set_edit_row(w->tile, input->edit) == MC_TILE_ENTITY_OK);
    const mc_entity_class_literal *pig = NULL;
    mc_mob_error error;
    CHECK(mc_entity_class_literal_get(f->runtime.services.entities, MC_ENTITY_CLASS_KIND_PIG,
                                      &pig) == MC_ENTITY_OK &&
          mc_mob_allocate(f->runtime.services.entities, f->runtime.services.registry, pig,
                          &w->viewer, &error) == MC_MOB_OK &&
          mc_mob_construct(w->viewer, w->world, &error) == MC_MOB_OK);
    float partial;
    memcpy(&partial, &input->partial, sizeof(partial));
    CHECK(mc_client_tile_renderer_bind_frame(f->manager, w->world, f->engine,
                                             input->null_font ? NULL : f->bitmap, w->viewer,
                                             partial, NULL) == MC_CLIENT_TILE_RENDERER_OK &&
          w->world->fields.b->count == 0 && w->world->fields.d->count == 0);
    return 0;
}

static void sg_row_receipts(FILE *out, const mc_client_tile_renderer_draw_state *s) {
    fputc('[', out);
    for (size_t i = 0; i < 4; ++i) {
        const mc_client_tile_renderer_sign_row_receipt *r = &s->sign_rows[i];
        fprintf(
            out,
            "%s{\"captured\":%s,\"null\":%s,\"selected\":%s,\"units\":%zu,\"digest\":\"%016" PRIx64
            "\",\"measure_attempted\":%s,\"measure_returned\":%s,\"draw_attempted\":%s,\"draw_"
            "returned\":%s,\"width\":%" PRId32 ",\"x\":%" PRId32 ",\"y\":%" PRId32 "}",
            i ? "," : "", r->captured ? "true" : "false", r->is_null ? "true" : "false",
            r->selected ? "true" : "false", r->text_units, r->text_digest,
            r->measure_attempted ? "true" : "false", r->measure_returned ? "true" : "false",
            r->draw_attempted ? "true" : "false", r->draw_returned ? "true" : "false", r->width,
            r->x, r->y);
    }
    fputc(']', out);
}

static int sg_draw(sign_geometry_fixture *f, const sign_geometry_input *input) {
    CHECK(sg_world_open(f, input) == 0);
    sign_geometry_world *w = &f->worlds[input->index];
    fprintf(f->output, "%s{\"index\":%u,\"before\":", f->rows ? ",\n" : "", input->index);
    CHECK(sg_snapshot(f, w, input) == 0);
    float partial;
    memcpy(&partial, &input->partial, sizeof(partial));
    const mc_client_manager_draw_services services = {f->math, f->runtime.graphics, f->display,
                                                      f->runtime.services.application_classes};
    const mc_client_tile_renderer_status status = mc_client_tile_renderer_coordinates_draw_begin(
        f->manager, w->tile, .375, .625, -.125, partial, &services, NULL, &f->call, f->draw_error);
    mc_client_tile_renderer_draw_state state;
    CHECK(f->call && mc_client_tile_renderer_draw_inspect(f->call, &state));
    fprintf(f->output, ",\"returned\":%s,\"failure\":", state.returned ? "true" : "false");
    if (status == MC_CLIENT_TILE_RENDERER_OK)
        fputs("null", f->output);
    else {
        fprintf(f->output, "{\"status\":%d,\"class\":", status);
        sg_string(f->output, f->draw_error->exception_class);
        fputs(",\"source_class\":", f->output);
        sg_string(f->output, f->draw_error->site.class_name);
        fprintf(f->output, ",\"pc\":%" PRId32 ",\"native_policy\":%s,\"source_exception\":%s}",
                f->draw_error->site.pc, f->draw_error->native_policy ? "true" : "false",
                f->draw_error->source_exception ? "true" : "false");
    }
    fprintf(f->output,
            ",\"receipt\":{\"source_pc\":%u,\"model_parts\":%u,\"compiled_parts\":%u,\"faces\":%u,"
            "\"vertices\":%u,\"rows_measured\":%zu,\"rows_drawn\":%zu,\"text_units\":%zu,\"text_"
            "digest\":\"%016" PRIx64 "\",\"angle\":\"%08" PRIx32 "\",\"text_scale\":\"%08" PRIx32
            "\",\"model_returned\":%s,\"model_popped\":%s,\"font_returned\":%s,\"depth_disabled\":%"
            "s,\"depth_restored\":%s,\"color_restored\":%s,\"matrix_popped\":%s,\"rows\":",
            state.source_pc, state.model_parts, state.model_compiled_parts, state.model_faces,
            state.model_vertices, state.sign_rows_measured, state.sign_rows_drawn,
            state.sign_text_units, state.sign_text_digest, sg_fbits(state.sign_angle),
            sg_fbits(state.sign_text_scale), state.sign_model_returned ? "true" : "false",
            state.sign_model_popped ? "true" : "false", state.sign_font_returned ? "true" : "false",
            state.sign_depth_disabled ? "true" : "false",
            state.sign_depth_restored ? "true" : "false",
            state.sign_color_restored ? "true" : "false", state.matrix_popped ? "true" : "false");
    sg_row_receipts(f->output, &state);
    fputs("},\"after\":", f->output);
    CHECK(sg_snapshot(f, w, input) == 0);
    fputs(",\"constructed_font_after\":", f->output);
    CHECK(sg_font(f, f->bitmap) == 0 && fflush(f->output) == 0);
    if (input->null_font)
        CHECK(status == MC_CLIENT_TILE_RENDERER_NULL_REFERENCE && !state.returned &&
              state.failure != NULL && state.sign_depth_disabled && !state.sign_depth_restored &&
              !state.matrix_popped && state.sign_rows_measured == 0 && state.sign_rows_drawn == 0);
    else
        CHECK(status == MC_CLIENT_TILE_RENDERER_OK && state.returned && state.renderer_returned &&
              state.sign_rows_drawn == 4);
    CHECK(mc_client_tile_renderer_draw_retire(&f->call, f->draw_error) ==
              MC_CLIENT_TILE_RENDERER_OK &&
          f->call == NULL &&
          mc_client_tile_renderer_draw_retire(&f->call, f->draw_error) ==
              MC_CLIENT_TILE_RENDERER_OK);
    fputs(",\"retired\":true,\"retire_idempotent\":true,\"after_retire_gl\":", f->output);
    CHECK(sg_gl(f) == 0);
    fputc('}', f->output);
    ++f->rows;
    return 0;
}

static int sg_open(sign_geometry_fixture *f, const char *resources, const char *assets,
                   const char *world_root) {
    CHECK(fixture_open(&f->runtime, resources) == 0 &&
          mc_client_application_constructor_allocate(&f->runtime.services, NULL, &f->app,
                                                     f->runtime.error) == MC_CLIENT_APP_OK);
    const mc_client_application_constructor_arguments app_arguments = {NULL, NULL, NULL,
                                                                       854,  480,  false};
    CHECK(mc_client_application_constructor_construct(f->app, &app_arguments, f->runtime.error) ==
              MC_CLIENT_APP_OK &&
          f->app->constructor_phase == MC_CLIENT_APP_READY);
    CHECK(mc_registry_classes_initialize_statistics(f->runtime.services.registry, NULL) ==
              MC_REGISTRY_OK &&
          mc_game_settings_create_defaults(NULL, &f->app->settings, NULL) == MC_GAME_SETTINGS_OK &&
          mc_resource_files_create(assets, &f->textures) == MC_REGISTRY_OK &&
          mc_client_loading_classes_bind_resources(f->runtime.services.gui_classes,
                                                   mc_resource_files_source(f->textures),
                                                   NULL) == MC_CLIENT_LOADING_OK &&
          mc_client_texture_pack_list_allocate(mc_resource_files_source(f->textures), NULL,
                                               &f->packs) == MC_TEXTURE_PACK_OK &&
          mc_client_texture_pack_list_construct(f->packs, f->app, world_root, NULL) ==
              MC_TEXTURE_PACK_OK &&
          mc_client_default_font_create(32u * 1024u * 1024u, &f->font) == MC_CLIENT_FONT_OK &&
          mc_client_render_engine_allocate(f->runtime.graphics, f->font, NULL, &f->engine) ==
              MC_CLIENT_RENDER_ENGINE_OK &&
          mc_client_render_engine_construct(f->engine, f->packs, f->app->settings, NULL) ==
              MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(mc_client_display_create("Direct Sign renderer observation", 256, 256, true, &f->display,
                                   NULL));
    /* The native comparison window stays hidden and is explicitly placed on
     * monitor2. This is separate from the ordinary gameplay SDK window. */
    int display_count = 0;
    SDL_DisplayID *displays = SDL_GetDisplays(&display_count);
    CHECK(displays != NULL && display_count >= 2);
    const SDL_DisplayID selected = displays[1];
    SDL_free(displays);
    CHECK(SDL_SetWindowPosition(mc_client_display_window(f->display),
                                (int)SDL_WINDOWPOS_CENTERED_DISPLAY(selected),
                                (int)SDL_WINDOWPOS_CENTERED_DISPLAY(selected)));
    CHECK(mc_math_classes_create(SIZE_MAX, &f->math, NULL) == MC_MATH_CLASS_OK &&
          mc_math_classes_ensure(f->math, NULL) == MC_MATH_CLASS_OK &&
          mc_world_runtime_classes_create(MC_WORLD_INFO_CLIENT, f->runtime.services.profile,
                                          mc_math_classes_table(f->math), &f->world_classes,
                                          NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_classes_bind_registry(f->world_classes, f->runtime.services.registry,
                                                 NULL) == MC_REGISTRY_OK &&
          mc_world_runtime_classes_bind_entities(f->world_classes, f->runtime.services.entities) ==
              MC_ENTITY_OK);
    f->source_error = calloc(1, sizeof(*f->source_error));
    f->region_error = calloc(1, sizeof(*f->region_error));
    f->draw_error = calloc(1, sizeof(*f->draw_error));
    f->lookup_error = calloc(1, sizeof(*f->lookup_error));
    f->tile_error = calloc(1, sizeof(*f->tile_error));
    CHECK(f->source_error && f->region_error && f->draw_error && f->lookup_error && f->tile_error &&
          mc_java_files_create(NULL, &f->files, &f->source_error->file) == MC_JAVA_FILE_OK &&
          mc_java_logging_create(NULL, &f->logging, &f->source_error->logging) ==
              MC_JAVA_LOGGING_OK &&
          mc_world_source_regions_create(f->files, NULL, &f->regions, f->region_error) ==
              MC_WORLD_SOURCE_OK &&
          mc_world_store_source_classes_create(f->files, f->logging, NULL, &f->handlers,
                                               f->source_error) == MC_WORLD_SOURCE_OK &&
          mc_world_store_source_classes_bind_regions(f->handlers, f->regions, f->source_error) ==
              MC_WORLD_SOURCE_OK);
    mc_world_info_name *root = sg_text(world_root);
    CHECK(root != NULL &&
          mc_java_file_allocate(f->files, &f->root, &f->source_error->file) == MC_JAVA_FILE_OK &&
          mc_java_file_construct_string(f->root, root, &f->source_error->file) == MC_JAVA_FILE_OK);
    mc_world_info_name_release(root);
    CHECK(mc_client_tile_renderer_classes_bind(f->runtime.services.render_classes, f->world_classes,
                                               f->runtime.services.profile, NULL, &f->renderers,
                                               NULL) == MC_CLIENT_TILE_RENDERER_OK &&
          mc_client_tile_renderer_static_get(f->renderers, &f->manager, NULL) ==
              MC_CLIENT_TILE_RENDERER_OK);
    const mc_tile_entity_class_literal *sign = NULL;
    CHECK(mc_tile_entity_class_literal_get(f->renderers->tile_classes,
                                           MC_TILE_ENTITY_CLASS_KIND_SIGN,
                                           &sign) == MC_TILE_ENTITY_OK &&
          mc_client_tile_renderer_lookup_class(f->manager, sign, &f->po, NULL) ==
              MC_CLIENT_TILE_RENDERER_OK &&
          f->po != NULL);
    f->model = f->po->sign_model;
    CHECK(f->model != NULL && f->model->a != NULL && f->model->b != NULL);
    f->board = f->model->a;
    f->pole = f->model->b;
    const mc_client_display_info *device = mc_client_display_info_get(f->display);
    CHECK(device != NULL);
    fputs("{\"schema\":\"NATIVE_SIGN_DIRECT_DRAW_V1\",\"device\":{\"vendor\":", f->output);
    sg_string(f->output, device->vendor);
    fputs(",\"renderer\":", f->output);
    sg_string(f->output, device->renderer);
    fputs(",\"version\":", f->output);
    sg_string(f->output, device->version);
    fprintf(f->output,
            ",\"hidden\":true,\"monitor\":2,\"display_id\":%" PRIu32
            "},\"font_constructed\":{\"before_gl\":",
            selected);
    CHECK(sg_gl(f) == 0);
    static const uint16_t resource[] = {'/', 'f', 'o', 'n', 't', '/', 'd', 'e', 'f',
                                        'a', 'u', 'l', 't', '.', 'p', 'n', 'g'};
    mc_player_string *name = NULL;
    mc_client_bitmap_font_error *font_error = calloc(1, sizeof(*font_error));
    CHECK(font_error != NULL &&
          mc_player_string_create(resource, sizeof(resource) / sizeof(*resource), &name) ==
              MC_PLAYER_OK &&
          mc_client_bitmap_font_allocate(f->runtime.services.gui_classes, NULL, &f->bitmap) ==
              MC_CLIENT_BITMAP_FONT_OK &&
          mc_client_bitmap_font_construct(f->bitmap, f->app->settings, name, f->engine, f->display,
                                          font_error) == MC_CLIENT_BITMAP_FONT_OK);
    mc_player_string_release(name);
    free(font_error);
    fputs(",\"after_gl\":", f->output);
    CHECK(sg_gl(f) == 0);
    fputs(",\"font\":", f->output);
    CHECK(sg_font(f, f->bitmap) == 0);
    fputs(",\"model\":{\"board\":", f->output);
    sg_part(f->output, f->board);
    fputs(",\"pole\":", f->output);
    sg_part(f->output, f->pole);
    fputs("}},\"rows\":[\n", f->output);
    mc_client_graphics_error error;
    CHECK(mc_client_nw_active_use(
        f->runtime.graphics, (mc_client_graphics_site){"SignGeometryFixture", "state", "()V", -1},
        &f->nw, &error));
    return 0;
}

static int sg_close(sign_geometry_fixture *f) {
    CHECK(f->call == NULL && f->rows == 36 &&
          mc_client_tile_renderer_classes_retire(&f->renderers, NULL) ==
              MC_CLIENT_TILE_RENDERER_OK);
    for (size_t i = 0; i < f->rows; ++i) {
        sign_geometry_world *w = &f->worlds[i];
        mc_entity_release(w->viewer);
        w->viewer = NULL;
        mc_tile_entity_release(w->tile);
        w->tile = NULL;
        CHECK(mc_world_runtime_retire_entity_containers(w->world, NULL) == MC_WORLD_STORAGE_OK &&
              mc_chunk_pending_clear(w->provider.pending) == MC_CHUNK_ACQUIRE_OK &&
              mc_chunk_cache_init(w->provider.cache, w->provider.cache->entries,
                                  w->provider.cache->capacity, w->provider.cache->insertions,
                                  w->provider.cache->insertion_capacity) &&
              mc_world_runtime_retire_world(w->world, NULL) == MC_WORLD_STORAGE_OK);
        for (size_t chunk = 0; chunk < 4; ++chunk)
            CHECK(mc_world_store_retire_chunk(w->store, w->chunks[chunk], NULL) ==
                  MC_WORLD_STORAGE_OK);
        CHECK(mc_world_runtime_free(w->world, NULL) == MC_WORLD_STORAGE_OK &&
              mc_world_store_source_release(&w->store, f->source_error) == MC_WORLD_SOURCE_OK);
        w->world = NULL;
    }
    CHECK(mc_client_bitmap_font_release(&f->bitmap, NULL) == MC_CLIENT_BITMAP_FONT_OK);
    CHECK(
        mc_world_runtime_classes_free(f->world_classes, NULL) == MC_WORLD_STORAGE_OK &&
        mc_world_store_source_classes_retire(&f->handlers, f->source_error) == MC_WORLD_SOURCE_OK &&
        mc_world_source_regions_retire(&f->regions, f->region_error) == MC_WORLD_SOURCE_OK &&
        mc_java_logging_retire(&f->logging, &f->source_error->logging) == MC_JAVA_LOGGING_OK &&
        mc_java_file_release(&f->root, &f->source_error->file) == MC_JAVA_FILE_OK &&
        mc_java_files_release(&f->files, &f->source_error->file) == MC_JAVA_FILE_OK &&
        mc_math_classes_release(&f->math, NULL) == MC_MATH_CLASS_OK &&
        mc_client_render_engine_retire_lookup(f->engine, f->lookup_error) == MC_CLIENT_LOOKUP_OK &&
        mc_client_texture_lookup_free_metadata(f->engine) &&
        mc_client_render_engine_release(f->engine) == MC_CLIENT_RENDER_ENGINE_OK &&
        mc_client_render_engine_retire_resources(f->runtime.graphics, NULL) ==
            MC_CLIENT_RENDER_TEXTURE_OK &&
        mc_client_default_font_release(f->font) == MC_CLIENT_FONT_OK &&
        mc_client_display_destroy(f->display, NULL) &&
        mc_client_texture_pack_list_release(f->packs, NULL) == MC_TEXTURE_PACK_OK);
    f->display = NULL;
    CHECK(mc_game_settings_retire(&f->app->settings, NULL) == MC_GAME_SETTINGS_OK &&
          mc_client_application_constructor_retire(&f->app, f->runtime.error) ==
              MC_CLIENT_APP_BUSY &&
          mc_client_application_classes_release(&f->runtime.services.application_classes, NULL) ==
              MC_CLIENT_APPLICATION_OWNER_OK &&
          mc_client_application_constructor_retire(&f->app, f->runtime.error) == MC_CLIENT_APP_OK &&
          f->app == NULL);
    /* The genuine World Registry->Entity association must end before the
     * startup fixture releases that Entity namespace. */
    CHECK(mc_registry_classes_free(f->runtime.services.registry, NULL) == MC_REGISTRY_OK);
    f->runtime.services.registry = NULL;
    CHECK(fixture_close(&f->runtime) == 0 && mc_resource_files_free(f->textures) == MC_REGISTRY_OK);
    free(f->draw_error);
    free(f->lookup_error);
    free(f->tile_error);
    free(f->source_error);
    free(f->region_error);
    return 0;
}

int main(int argc, char **argv) {
    CHECK(argc == 6);
    SDL_SetMainReady();
    sign_geometry_fixture *f = calloc(1, sizeof(*f));
    CHECK(f);
    f->output = fopen(argv[5], "wb");
    FILE *profiles = fopen(argv[4], "rb");
    CHECK(f->output && profiles && sg_open(f, argv[1], argv[2], argv[3]) == 0);
    char line[2048];
    unsigned int index = 0;
    while (fgets(line, sizeof(line), profiles)) {
        size_t n = strlen(line);
        CHECK(n && line[n - 1] == '\n');
        line[n - 1] = '\0';
        if (n > 1 && line[n - 2] == '\r')
            line[n - 2] = '\0';
        sign_geometry_input input = {0};
        CHECK(sg_input(line, index, &input) == 0 && sg_draw(f, &input) == 0);
        ++index;
    }
    CHECK(index == 36 && !ferror(profiles) && fclose(profiles) == 0 && sg_close(f) == 0);
    fprintf(f->output, "],\"cleanup\":true,\"rows_complete\":%u}\n", index);
    CHECK(!ferror(f->output) && fclose(f->output) == 0);
    free(f);
    printf("PASS: 36 Source Sign direct PO observation rows; checks=%u; cleanup=1\n", checks);
    return 0;
}
