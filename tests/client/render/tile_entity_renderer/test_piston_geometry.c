/* Direct HY observations over genuine constructors and saved GAME Worlds.
 * One App/Registry/LL/HY/JI/NW/Display namespace is shared across all rows.
 * No UK field repair, Entity publication, World-array edit or NW reset occurs.
 * The included startup main is never executed; only its service helpers run. */
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#endif
int mc_piston_geometry_startup_fixture_main(int argc, char **argv);
#define main mc_piston_geometry_startup_fixture_main
#include "../../startup/test_application_startup.c"
#undef main

#include "block/registry/registry_internal.h"
#include "client/display/fog_surface.h"
#include "client/display/source_surface.h"
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
#include "mc/world/runtime/tile_entities/piston.h"
#include "mc/world/storage/source_regions.h"
#include "world/chunk/object_state_internal.h"
#include "world/runtime/chunk_provider_internal.h"
#include "world/runtime/construction_internal.h"
#include "world/runtime/runtime_internal.h"
#include "world/storage/objects_internal.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <errno.h>
#include <inttypes.h>

typedef struct piston_geometry_input {
    unsigned int index;
    char folder[32], name[128];
    int32_t block, metadata, facing, ticks;
    uint32_t partial;
    bool extending, render_head, ao;
    int64_t seed;
} piston_geometry_input;

typedef struct piston_geometry_world {
    mc_world_store *store;
    mc_world_runtime *world;
    mc_chunk_state *chunks[4];
    mc_chunk_provider_fields provider;
    mc_tile_entity *tile;
    mc_entity *viewer;
} piston_geometry_world;

typedef struct piston_geometry_fixture {
    fixture runtime;
    client_application *app;
    mc_client_texture_pack_list *packs;
    mc_resource_files *textures;
    mc_client_default_font *font;
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
    mc_client_tile_renderer *hy;
    mc_client_tessellator *nw;
    mc_client_tile_renderer_draw_call *call;
    mc_client_tile_renderer_draw_error *draw_error;
    mc_client_texture_lookup_error *lookup_error;
    mc_live_tile_entity_error *tile_error;
    mc_piston_tick_error *tick_error;
    mc_world_source_error *source_error;
    mc_world_source_region_error *region_error;
    piston_geometry_world worlds[27];
    FILE *output;
    size_t rows;
} piston_geometry_fixture;

static uint32_t pg_fbits(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static uint64_t pg_dbits(double value) {
    uint64_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static void pg_string(FILE *out, const char *value) {
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

static mc_world_info_name *pg_text(const char *ascii) {
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

static bool pg_integer(const char *text, int64_t low, int64_t high, int64_t *out) {
    char *end = NULL;
    errno = 0;
    const long long value = strtoll(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || value < low || value > high)
        return false;
    *out = (int64_t)value;
    return true;
}

static bool pg_boolean(const char *text, bool *out) {
    if (strcmp(text, "true") == 0)
        *out = true;
    else if (strcmp(text, "false") == 0)
        *out = false;
    else
        return false;
    return true;
}

static int pg_input(char *line, unsigned int expected, piston_geometry_input *out) {
    char *cells[12], *cursor = line;
    for (size_t i = 0; i < 12; ++i) {
        cells[i] = cursor;
        char *next = strchr(cursor, '\t');
        CHECK((i == 11) == (next == NULL));
        if (next != NULL) {
            *next = '\0';
            cursor = next + 1;
        }
    }
    int64_t value;
    CHECK(pg_integer(cells[0], 0, 26, &value) && value == expected);
    out->index = (unsigned int)value;
    char folder[32];
    CHECK(snprintf(folder, sizeof(folder), "case%02u", out->index) > 0 &&
          strcmp(cells[1], folder) == 0);
    memcpy(out->folder, folder, strlen(folder) + 1);
    CHECK(pg_integer(cells[2], 0, 255, &value));
    out->block = (int32_t)value;
    CHECK(pg_integer(cells[3], 0, 15, &value));
    out->metadata = (int32_t)value;
    CHECK(pg_integer(cells[4], 0, 5, &value));
    out->facing = (int32_t)value;
    CHECK(pg_integer(cells[5], 0, 2, &value));
    out->ticks = (int32_t)value;
    CHECK(strlen(cells[6]) == 8);
    uint32_t bits = 0;
    for (size_t i = 0; i < 8; ++i) {
        const unsigned char c = (unsigned char)cells[6][i];
        CHECK((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'));
        bits = (bits << 4u) | (uint32_t)(c <= '9' ? c - '0' : c - 'a' + 10);
    }
    out->partial = bits;
    CHECK(pg_boolean(cells[7], &out->extending) && pg_boolean(cells[8], &out->render_head) &&
          pg_boolean(cells[9], &out->ao) && strlen(cells[10]) < sizeof(out->name) &&
          pg_integer(cells[11], INT64_MIN, INT64_MAX, &out->seed));
    memcpy(out->name, cells[10], strlen(cells[10]) + 1);
    return 0;
}

static void pg_bounds(FILE *out, const mc_block *block) {
    if (block == NULL) {
        fputs("null", out);
        return;
    }
    const double bounds[] = {block->fields.bs, block->fields.bt, block->fields.bu,
                             block->fields.bv, block->fields.bw, block->fields.bx};
    fputc('[', out);
    for (size_t i = 0; i < 6; ++i)
        fprintf(out, "%s\"%016" PRIx64 "\"", i == 0 ? "" : ",", pg_dbits(bounds[i]));
    fputc(']', out);
}

static void pg_cv(FILE *out, const mc_client_render_blocks *fields) {
    fprintf(out,
            "{\"d\":%" PRId32 ",\"e\":%s,\"f\":%s,\"b\":%s,\"g\":%" PRId32 ",\"h\":%" PRId32
            ",\"i\":%" PRId32 ",\"j\":%" PRId32 ",\"k\":%" PRId32 ",\"l\":%" PRId32
            ",\"m\":%s,\"O\":%" PRId32,
            fields->d, fields->e ? "true" : "false", fields->f ? "true" : "false",
            fields->b ? "true" : "false", fields->g, fields->h, fields->i, fields->j, fields->k,
            fields->l, fields->m ? "true" : "false", fields->O);
    for (size_t i = 0; i < 27; ++i)
        fprintf(out, ",\"%c\":\"%08" PRIx32 "\"", (int)(i < 13 ? 'n' + i : 'A' + i - 13),
                pg_fbits(fields->n_to_N[i]));
    for (size_t i = 0; i < 12; ++i) {
        char name[3] = {'a', 'a', '\0'};
        if (i < 11) {
            name[0] = (char)('P' + i);
            name[1] = '\0';
        }
        fprintf(out, ",\"%s\":\"%08" PRIx32 "\",\"a%c\":%s", name, pg_fbits(fields->P_to_aa[i]),
                (int)('b' + i), fields->ab_to_am[i] ? "true" : "false");
    }
    fputc('}', out);
}

static void pg_nw(FILE *out, const mc_client_nw_fields *f) {
    fprintf(out,
            "{\"h\":%" PRId32 ",\"i\":\"%016" PRIx64 "\",\"j\":\"%016" PRIx64 "\",\"k\":%" PRId32
            ",\"l\":%s,\"m\":%s,\"n\":%s,\"o\":%" PRId32 ",\"p\":%" PRId32
            ",\"q\":%s,\"r\":%" PRId32 ",\"s\":\"%016" PRIx64 "\",\"t\":\"%016" PRIx64
            "\",\"u\":\"%016" PRIx64 "\",\"v\":%" PRId32 ",\"w\":%s,\"x\":%s,\"z\":%" PRId32
            ",\"A\":%" PRId32 ",\"B\":%" PRId32 "}",
            f->h, pg_dbits(f->i), pg_dbits(f->j), f->k, f->l ? "true" : "false",
            f->m ? "true" : "false", f->n ? "true" : "false", f->o, f->p, f->q ? "true" : "false",
            f->r, pg_dbits(f->s), pg_dbits(f->t), pg_dbits(f->u), f->v, f->w ? "true" : "false",
            f->x ? "true" : "false", f->z, f->A, f->B);
}

static int pg_gl(piston_geometry_fixture *f) {
    mc_client_source_draw_state draw = {0};
    mc_client_source_fog_state fog = {0};
    const bool known = mc_client_display_source_draw_inspect(f->display, &draw) &&
                       mc_client_display_source_fog_inspect(f->display, &fog);
    CHECK(mc_client_display_make_current(f->display, NULL));
    GLint texture = 0, source = 0, destination = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture);
    glGetIntegerv(GL_BLEND_SRC, &source);
    glGetIntegerv(GL_BLEND_DST, &destination);
    const GLboolean blend = glIsEnabled(GL_BLEND), cull = glIsEnabled(GL_CULL_FACE);
    uint32_t error = UINT32_MAX;
    CHECK(mc_client_display_source_get_error(f->display, &error, NULL));
    fprintf(f->output,
            "{\"source_known\":%s,\"blend\":%s,\"cull\":%s,\"blend_src\":%d,"
            "\"blend_dst\":%d,\"bound_texture\":%d,\"error\":%" PRIu32,
            known ? "true" : "false", blend ? "true" : "false", cull ? "true" : "false", source,
            destination, texture, error);
    if (known)
        fprintf(f->output,
                ",\"lighting\":%s,\"light0\":%s,\"light1\":%s,\"color_material\":%s,"
                "\"shade\":%d",
                draw.lighting_enabled ? "true" : "false", fog.lights[0] ? "true" : "false",
                fog.lights[1] ? "true" : "false", fog.color_material ? "true" : "false",
                draw.shade_flat ? GL_FLAT : GL_SMOOTH);
    fputc('}', f->output);
    CHECK(error == 0);
    return 0;
}

static void pg_tile_list(FILE *out, const mc_world_reference_list *list,
                         const mc_tile_entity *selected) {
    fputc('[', out);
    for (size_t i = 0; i < list->count; ++i)
        fprintf(out, "%s{\"class\":\"uk\",\"selected\":%s}", i == 0 ? "" : ",",
                list->items[i] == selected ? "true" : "false");
    fputc(']', out);
}

static int pg_snapshot(piston_geometry_fixture *f, piston_geometry_world *w,
                       const piston_geometry_input *input) {
    mc_tile_entity_snapshot tile;
    mc_world_info_fields info;
    mc_client_nw_state nw;
    mc_client_tile_renderer_object_state renderer;
    const mc_block *carried = NULL, *head = NULL, *moving = NULL;
    int32_t id = -1, metadata = -1;
    CHECK(
        mc_tile_entity_get_snapshot(w->tile, &tile) == MC_TILE_ENTITY_OK &&
        mc_world_info_fields_get(w->world->fields.x, &info) == MC_WORLD_INFO_OK &&
        mc_client_nw_inspect(f->runtime.graphics, &nw) &&
        mc_client_tile_renderer_object_inspect(f->hy, &renderer) &&
        renderer.piston_blocks != NULL &&
        mc_registry_block_get(f->runtime.services.registry, input->block, &carried, NULL) ==
            MC_REGISTRY_OK &&
        mc_registry_block_get(f->runtime.services.registry, 34, &head, NULL) == MC_REGISTRY_OK &&
        mc_registry_block_get(f->runtime.services.registry, 36, &moving, NULL) == MC_REGISTRY_OK &&
        head != NULL && moving != NULL &&
        mc_world_runtime_block_get(w->world, 8, 64, 8, &id, NULL) == MC_WORLD_STORAGE_OK &&
        mc_world_runtime_metadata_get(w->world, 8, 64, 8, &metadata, NULL) == MC_WORLD_STORAGE_OK &&
        tile.kind == MC_TILE_ENTITY_PISTON && w->world->fields.c->count == 1 &&
        w->world->fields.c->items[0] == w->tile && w->world->fields.G->count == 0 &&
        w->world->fields.b->count == 0 && w->world->fields.d->count == 0);
    FILE *out = f->output;
    fprintf(
        out,
        "{\"tile\":{\"class\":\"uk\",\"world_same\":%s,\"xyz\":[%" PRId32 ",%" PRId32 ",%" PRId32
        "],\"invalid\":%s,\"block\":%" PRId32 ",\"metadata\":%" PRId32 ",\"facing\":%" PRId32
        ",\"extending\":%s,\"render_head\":%s,\"current_bits\":\"%08" PRIx32
        "\",\"previous_bits\":\"%08" PRIx32 "\"},\"world_time\":%" PRId64
        ",\"world_iterating\":%s,\"world_entities\":%zu,\"world_players\":%zu,"
        "\"active_tiles\":",
        tile.world == w->world ? "true" : "false", tile.x, tile.y, tile.z,
        tile.invalid ? "true" : "false", tile.piston_block_id, tile.piston_block_data,
        tile.piston_facing, tile.piston_extending ? "true" : "false",
        tile.piston_render_head ? "true" : "false", tile.piston_progress_bits,
        tile.piston_previous_progress_bits, info.value.time, w->world->fields.L ? "true" : "false",
        w->world->fields.b->count, w->world->fields.d->count);
    pg_tile_list(out, w->world->fields.c, w->tile);
    fputs(",\"pending_tiles\":", out);
    pg_tile_list(out, w->world->fields.G, w->tile);
    mc_world_chunk *chunk = mc_world_store_chunk_object(w->store, w->chunks[0]);
    CHECK(chunk != NULL && chunk->l != NULL);
    fputs(",\"mapped_tiles\":[", out);
    for (size_t i = 0; i < chunk->l->count; ++i) {
        mc_live_tile_entity_map_entry entry;
        CHECK(mc_live_tile_entities_map_entry_at(chunk, i, &entry) == MC_WORLD_STORAGE_OK &&
              entry.tile_entity == w->tile);
        fprintf(out, "%s{\"class\":\"uk\",\"selected\":%s}", i == 0 ? "" : ",",
                entry.tile_entity == w->tile ? "true" : "false");
    }
    fprintf(out,
            "],\"block\":%" PRId32 ",\"world_metadata\":%" PRId32
            ",\"cv_class\":\"cv\",\"cv_world_same\":%s,\"cv\":",
            id, metadata, renderer.piston_blocks->c == w->world ? "true" : "false");
    pg_cv(out, renderer.piston_blocks);
    fputs(",\"carried_bounds\":", out);
    pg_bounds(out, carried);
    fputs(",\"head_bounds\":", out);
    pg_bounds(out, head);
    fputs(",\"moving_bounds\":", out);
    pg_bounds(out, moving);
    fprintf(out, ",\"head_texture\":%" PRId32 ",\"nw\":", head->subtype.h.a);
    pg_nw(out, &nw.fields);
    fputs(",\"gl\":", out);
    CHECK(pg_gl(f) == 0);
    fputc('}', out);
    return 0;
}

static int pg_world_open(piston_geometry_fixture *f, const piston_geometry_input *input) {
    piston_geometry_world *w = &f->worlds[input->index];
    mc_world_info_name *folder = pg_text(input->folder), *name = pg_text(input->name);
    CHECK(folder != NULL && name != NULL &&
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
    if (status != MC_WORLD_STORAGE_OK)
        fprintf(stderr, "Piston World case%u status%d operation=%s\n", input->index, status,
                w->world->last_error.operation == NULL ? "none" : w->world->last_error.operation);
    CHECK(status == MC_WORLD_STORAGE_OK &&
          mc_chunk_provider_bind_fields(w->world->fields.v, &w->provider, NULL) ==
              MC_WORLD_STORAGE_OK);
    mc_world_runtime_source_construction_state source;
    CHECK(mc_world_runtime_source_construction_inspect(w->world, &source) &&
          source.store == w->store && source.metadata_returned && !source.loaded_info_was_null);
    for (int32_t x = 0; x < 2; ++x)
        for (int32_t z = 0; z < 2; ++z) {
            mc_chunk_state **chunk = &w->chunks[(size_t)(x * 2 + z)];
            CHECK(mc_world_runtime_get(w->world, x, z, chunk) == MC_CHUNK_ACQUIRE_OK &&
                  *chunk != NULL && !(*chunk)->blank && (*chunk)->active &&
                  (*chunk)->terrain_populated && mc_world_store_chunk_object(w->store, *chunk));
        }
    int32_t id = -1, metadata = -1;
    mc_world_chunk *chunk = mc_world_store_chunk_object(w->store, w->chunks[0]);
    CHECK(chunk != NULL && chunk->l != NULL && chunk->l->count == 0 &&
          mc_world_runtime_block_get(w->world, 8, 64, 8, &id, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_metadata_get(w->world, 8, 64, 8, &metadata, NULL) ==
              MC_WORLD_STORAGE_OK &&
          id == 36 && metadata == input->metadata &&
          mc_tile_entity_allocate(w->world, MC_TILE_ENTITY_PISTON, &w->tile) == MC_TILE_ENTITY_OK &&
          mc_tile_entity_piston_construct(w->tile, input->block, input->metadata, input->facing,
                                          input->extending, input->render_head,
                                          NULL) == MC_TILE_ENTITY_OK &&
          mc_live_tile_entities_set(w->world, 8, 64, 8, w->tile, f->tile_error) ==
              MC_WORLD_STORAGE_OK);
    mc_tile_entity *published = NULL;
    CHECK(mc_live_tile_entities_get(w->world, 8, 64, 8, &published, f->tile_error) ==
              MC_WORLD_STORAGE_OK &&
          published == w->tile && chunk->l->count == 1);
    mc_tile_entity_release(published);
    for (int32_t tick = 0; tick < input->ticks; ++tick)
        CHECK(mc_tile_entity_piston_tick(w->tile, NULL, f->tick_error) == MC_WORLD_STORAGE_OK);
    const mc_entity_class_literal *pig = NULL;
    mc_mob_error error;
    CHECK(mc_entity_class_literal_get(f->runtime.services.entities, MC_ENTITY_CLASS_KIND_PIG,
                                      &pig) == MC_ENTITY_OK &&
          mc_mob_allocate(f->runtime.services.entities, f->runtime.services.registry, pig,
                          &w->viewer, &error) == MC_MOB_OK &&
          mc_mob_construct(w->viewer, w->world, &error) == MC_MOB_OK);
    float partial;
    memcpy(&partial, &input->partial, sizeof(partial));
    CHECK(mc_client_tile_renderer_bind_frame(f->manager, w->world, f->engine, NULL, w->viewer,
                                             partial, NULL) == MC_CLIENT_TILE_RENDERER_OK);
    mc_game_settings_state settings;
    CHECK(mc_game_settings_read_state(f->app->settings, &settings));
    settings.values.ambient_occlusion = input->ao;
    CHECK(mc_game_settings_set_values(f->app->settings, &settings.values, NULL) ==
              MC_GAME_SETTINGS_OK &&
          w->world->fields.b->count == 0 && w->world->fields.d->count == 0);
    return 0;
}

static int pg_draw(piston_geometry_fixture *f, const piston_geometry_input *input) {
    CHECK(pg_world_open(f, input) == 0);
    piston_geometry_world *w = &f->worlds[input->index];
    fprintf(f->output,
            "%s{\"index\":%u,\"profile\":{\"folder\":\"%s\",\"block\":%" PRId32
            ",\"metadata\":%" PRId32 ",\"facing\":%" PRId32 ",\"ticks\":%" PRId32
            ",\"partial_bits\":\"%08" PRIx32 "\",\"extending\":%s,\"render_head\":%s,"
            "\"ambient_occlusion\":%s,\"seed\":%" PRId64 ",\"name\":",
            f->rows == 0 ? "" : ",\n", input->index, input->folder, input->block, input->metadata,
            input->facing, input->ticks, input->partial, input->extending ? "true" : "false",
            input->render_head ? "true" : "false", input->ao ? "true" : "false", input->seed);
    pg_string(f->output, input->name);
    fputs("},\"before\":", f->output);
    CHECK(pg_snapshot(f, w, input) == 0);
    float partial;
    memcpy(&partial, &input->partial, sizeof(partial));
    const mc_client_manager_draw_services services = {f->math, f->runtime.graphics, f->display,
                                                      f->runtime.services.application_classes};
    const mc_client_tile_renderer_status status = mc_client_tile_renderer_coordinates_draw_begin(
        f->manager, w->tile, .375, .625, -.125, partial, &services, NULL, &f->call, f->draw_error);
    mc_client_tile_renderer_draw_state state;
    CHECK(f->call != NULL && mc_client_tile_renderer_draw_inspect(f->call, &state));
    fprintf(f->output, ",\"returned\":%s,\"failure\":", state.returned ? "true" : "false");
    if (status == MC_CLIENT_TILE_RENDERER_OK)
        fputs("null", f->output);
    else {
        fprintf(f->output, "{\"status\":%d,\"class\":", status);
        pg_string(f->output, f->draw_error->site.class_name);
        fprintf(f->output, ",\"pc\":%" PRId32 ",\"operation\":", f->draw_error->site.pc);
        pg_string(f->output, f->draw_error->operation);
        fputc('}', f->output);
        fprintf(stderr, "Piston draw case%u status%d site=%s:%d operation=%s\n", input->index,
                status, f->draw_error->site.class_name, f->draw_error->site.pc,
                f->draw_error->operation);
    }
    mc_client_nw_state nw;
    CHECK(mc_client_nw_inspect(f->runtime.graphics, &nw) && nw.fields.g != NULL &&
          state.piston_vertices <= (size_t)nw.fields.B / 8);
    fprintf(f->output, ",\"vertices\":%zu,\"word_count\":%zu,\"words\":[", state.piston_vertices,
            state.piston_vertices * 8);
    for (size_t i = 0; i < state.piston_vertices * 8; ++i)
        fprintf(f->output, "%s\"%08" PRIx32 "\"", i == 0 ? "" : ",", (uint32_t)nw.fields.g[i]);
    fprintf(f->output,
            "],\"progress_bits\":\"%08" PRIx32 "\",\"translation_bits\":[\"%016" PRIx64
            "\",\"%016" PRIx64 "\",\"%016" PRIx64
            "\"],\"skipped\":%s,\"tessellator_begun\":%s,\"tessellator_drawn\":%s,\"after\":",
            pg_fbits(state.piston_progress), pg_dbits(state.piston_translation[0]),
            pg_dbits(state.piston_translation[1]), pg_dbits(state.piston_translation[2]),
            state.piston_skipped ? "true" : "false", state.tessellator_begun ? "true" : "false",
            state.tessellator_drawn ? "true" : "false");
    CHECK(pg_snapshot(f, w, input) == 0);
    static const uint16_t terrain[] = {'/', 't', 'e', 'r', 'r', 'a', 'i', 'n', '.', 'p', 'n', 'g'};
    bool found = false;
    mc_client_texture texture = 0;
    CHECK(mc_client_render_engine_cached_texture(
        f->engine, (mc_texture_pack_text){terrain, sizeof(terrain) / sizeof(*terrain), false},
        &found, &texture));
    GLint width = 0, height = 0;
    if (found) {
        CHECK(mc_client_display_make_current(f->display, NULL));
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &width);
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &height);
    }
    fprintf(f->output, ",\"texture\":{\"found\":%s,\"id\":%" PRIu32 ",\"width\":%d,\"height\":%d}",
            found ? "true" : "false", texture, width, height);
    fputc('}', f->output);
    CHECK(fflush(f->output) == 0 &&
          mc_client_tile_renderer_draw_retire(&f->call, f->draw_error) ==
              MC_CLIENT_TILE_RENDERER_OK &&
          f->call == NULL);
    CHECK(status == MC_CLIENT_TILE_RENDERER_OK && state.returned && state.renderer_returned &&
          state.renderer_class != NULL && strcmp(state.renderer_class, "hy") == 0 &&
          (state.piston_skipped || state.tessellator_drawn));
    ++f->rows;
    return 0;
}

/* Separate authored native fault after the unchanged27 original-comparison
 * rows. The real U class is already READY. Its actual Source buffer field is
 * assigned NULL once and stays NULL through every cleanup and final close. */
static int pg_lighting_null_buffer(piston_geometry_fixture *f) {
    piston_geometry_world *w = &f->worlds[0];
    CHECK(f->rows == 27 && f->call == NULL &&
          mc_client_tile_renderer_bind_frame(f->manager, w->world, f->engine, NULL, w->viewer, 0.0f,
                                             NULL) == MC_CLIENT_TILE_RENDERER_OK);
    mc_client_lighting_state initial;
    mc_entity_vector_snapshot vectors_before;
    CHECK(mc_client_lighting_inspect(f->runtime.graphics, &initial) &&
          initial.phase == MC_CLIENT_RENDER_ENGINE_READY && initial.a != NULL &&
          mc_entity_vector_get_snapshot(f->runtime.services.entities, &vectors_before) ==
              MC_ENTITY_OK);
    const size_t world_bindings = w->world->persistent_bindings;
    f->runtime.graphics->u_a = NULL; /* Declared native Source-field input; never restored. */
    const mc_client_manager_draw_services services = {f->math, f->runtime.graphics, f->display,
                                                      f->runtime.services.application_classes};
    CHECK(mc_client_tile_renderer_coordinates_draw_begin(
              f->manager, w->tile, .375, .625, -.125, 0.0f, &services, NULL, &f->call,
              f->draw_error) == MC_CLIENT_TILE_RENDERER_LIGHTING_FAILED &&
          f->call != NULL);
    mc_client_tile_renderer_draw_state state;
    CHECK(mc_client_tile_renderer_draw_inspect(f->call, &state) && !state.returned &&
          !state.renderer_returned && state.tessellator_begun && state.tessellator_drawn &&
          state.piston_head_returned && !state.piston_base_returned &&
          state.piston_vertices == 60 && state.failure != NULL && state.source_pc == 360);
    const mc_client_tile_renderer_draw_error source = *state.failure;
    CHECK(source.status == MC_CLIENT_TILE_RENDERER_LIGHTING_FAILED && !source.native_policy &&
          source.source_exception && source.exception_class != NULL &&
          strcmp(source.exception_class, "java/lang/NullPointerException") == 0 &&
          strcmp(source.site.class_name, "hy") == 0 && source.site.pc == 360 &&
          strcmp(source.child.class_name, "u") == 0 && strcmp(source.child.method, "b") == 0 &&
          source.child.pc == 74 && strcmp(source.helper.class_name, "u") == 0 &&
          source.helper.pc == 3 && source.lighting != NULL);
    const mc_client_lighting_error lighting = *source.lighting;
    CHECK(lighting.status == MC_CLIENT_LIGHTING_GRAPHICS_FAILED && !lighting.native_policy &&
          !lighting.source_returned && lighting.site.pc == 74 && lighting.child.pc == 3 &&
          strcmp(lighting.child.descriptor, "(FFFF)Ljava/nio/FloatBuffer;") == 0 &&
          lighting.helper.pc == 10 &&
          strcmp(lighting.helper.descriptor, "(DDDD)Ljava/nio/FloatBuffer;") == 0 &&
          lighting.graphics.status == MC_CLIENT_GRAPHICS_NULL_REFERENCE &&
          !lighting.graphics.native_policy && lighting.completed_display_calls == 5 &&
          lighting.completed_buffer_fills == 0 && lighting.pooled_vectors == 2);
    mc_client_nw_state nw;
    mc_entity_vector_snapshot vectors_prefix;
    mc_tile_entity_snapshot tile_prefix;
    mc_client_source_draw_state draw_prefix = {0}, draw_retired = {0};
    mc_client_source_fog_state fog_prefix = {0}, fog_retired = {0};
    CHECK(mc_client_nw_inspect(f->runtime.graphics, &nw) && nw.fields.g != NULL && !nw.fields.w &&
          nw.fields.h == 0 && nw.fields.o == 0 && nw.fields.p == 0 && nw.fields.s == 0 &&
          nw.fields.t == 0 && nw.fields.u == 0 &&
          mc_entity_vector_get_snapshot(f->runtime.services.entities, &vectors_prefix) ==
              MC_ENTITY_OK &&
          vectors_prefix.cursor == vectors_before.cursor + 2 &&
          mc_tile_entity_get_snapshot(w->tile, &tile_prefix) == MC_TILE_ENTITY_OK &&
          f->runtime.graphics->u_a == NULL && f->hy->piston_blocks != NULL &&
          mc_client_display_source_draw_inspect(f->display, &draw_prefix) &&
          mc_client_display_source_fog_inspect(f->display, &fog_prefix) &&
          draw_prefix.lighting_enabled && fog_prefix.lights[0] && fog_prefix.lights[1] &&
          fog_prefix.color_material);
    const mc_client_render_blocks cv_prefix = *f->hy->piston_blocks;
    const int32_t block_ids[4] = {29, 33, 34, 36};
    const mc_block *blocks[4] = {NULL};
    mc_block_fields bounds_prefix[4];
    for (size_t i = 0; i < 4; ++i) {
        CHECK(mc_registry_block_get(f->runtime.services.registry, block_ids[i], &blocks[i], NULL) ==
                  MC_REGISTRY_OK &&
              blocks[i] != NULL);
        bounds_prefix[i] = blocks[i]->fields;
    }
    const size_t word_bytes = state.piston_vertices * 8 * sizeof(*nw.fields.g);
    int32_t *words = malloc(word_bytes);
    CHECK(words != NULL);
    memcpy(words, nw.fields.g, word_bytes);
    CHECK(mc_client_tile_renderer_draw_retire(&f->call, f->draw_error) ==
              MC_CLIENT_TILE_RENDERER_OK &&
          f->call == NULL &&
          mc_client_tile_renderer_draw_retire(&f->call, f->draw_error) ==
              MC_CLIENT_TILE_RENDERER_OK &&
          w->world->persistent_bindings == world_bindings && f->runtime.graphics->u_a == NULL &&
          memcmp(&cv_prefix, f->hy->piston_blocks, sizeof(cv_prefix)) == 0);
    mc_client_nw_state nw_retired;
    mc_entity_vector_snapshot vectors_retired;
    mc_tile_entity_snapshot tile_retired;
    CHECK(mc_client_nw_inspect(f->runtime.graphics, &nw_retired) &&
          memcmp(&nw.fields, &nw_retired.fields, sizeof(nw.fields)) == 0 &&
          memcmp(words, nw_retired.fields.g, word_bytes) == 0 &&
          mc_entity_vector_get_snapshot(f->runtime.services.entities, &vectors_retired) ==
              MC_ENTITY_OK &&
          vectors_retired.cursor == vectors_prefix.cursor &&
          mc_tile_entity_get_snapshot(w->tile, &tile_retired) == MC_TILE_ENTITY_OK &&
          tile_retired.kind == tile_prefix.kind && tile_retired.phase == tile_prefix.phase &&
          tile_retired.world == tile_prefix.world && tile_retired.x == tile_prefix.x &&
          tile_retired.y == tile_prefix.y && tile_retired.z == tile_prefix.z &&
          tile_retired.invalid == tile_prefix.invalid &&
          tile_retired.piston_block_id == tile_prefix.piston_block_id &&
          tile_retired.piston_block_data == tile_prefix.piston_block_data &&
          tile_retired.piston_facing == tile_prefix.piston_facing &&
          tile_retired.piston_extending == tile_prefix.piston_extending &&
          tile_retired.piston_render_head == tile_prefix.piston_render_head &&
          tile_retired.piston_progress_bits == tile_prefix.piston_progress_bits &&
          tile_retired.piston_previous_progress_bits == tile_prefix.piston_previous_progress_bits &&
          mc_client_display_source_draw_inspect(f->display, &draw_retired) &&
          mc_client_display_source_fog_inspect(f->display, &fog_retired) &&
          memcmp(&draw_prefix, &draw_retired, sizeof(draw_prefix)) == 0 &&
          memcmp(&fog_prefix, &fog_retired, sizeof(fog_prefix)) == 0);
    for (size_t i = 0; i < 4; ++i)
        CHECK(memcmp(&bounds_prefix[i], &blocks[i]->fields, sizeof(bounds_prefix[i])) == 0);
    free(words);
    fprintf(f->output, "\n],\"native_controls\":{\"lighting_null_buffer\":{"
                       "\"scope\":\"authored-native-fault\",\"source_returned\":false,"
                       "\"hy_pc\":360,\"u_pc\":74,\"float_clear_pc\":3,"
                       "\"exception_class\":\"java/lang/NullPointerException\","
                       "\"tessellator_drawn\":true,\"vertices\":60,\"completed_display_calls\":5,"
                       "\"pooled_vectors\":2,\"cleanup_source_unchanged\":true,\"retired\":true,"
                       "\"retire_idempotent\":true,\"u_buffer_remains_null\":true}}");
    return 0;
}

static int pg_open(piston_geometry_fixture *f, const char *resources, const char *assets,
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
          mc_client_texture_pack_list_allocate(mc_resource_files_source(f->textures), NULL,
                                               &f->packs) == MC_TEXTURE_PACK_OK &&
          mc_client_texture_pack_list_construct(f->packs, f->app, world_root, NULL) ==
              MC_TEXTURE_PACK_OK &&
          mc_client_default_font_create(32u * 1024u * 1024u, &f->font) == MC_CLIENT_FONT_OK &&
          mc_client_render_engine_allocate(f->runtime.graphics, f->font, NULL, &f->engine) ==
              MC_CLIENT_RENDER_ENGINE_OK &&
          mc_client_render_engine_construct(f->engine, f->packs, f->app->settings, NULL) ==
              MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(mc_client_display_create("Direct Piston renderer comparison", 256, 256, true, &f->display,
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
    f->tick_error = calloc(1, sizeof(*f->tick_error));
    CHECK(f->source_error && f->region_error && f->draw_error && f->lookup_error && f->tile_error &&
          f->tick_error &&
          mc_java_files_create(NULL, &f->files, &f->source_error->file) == MC_JAVA_FILE_OK &&
          mc_java_logging_create(NULL, &f->logging, &f->source_error->logging) ==
              MC_JAVA_LOGGING_OK &&
          mc_world_source_regions_create(f->files, NULL, &f->regions, f->region_error) ==
              MC_WORLD_SOURCE_OK &&
          mc_world_store_source_classes_create(f->files, f->logging, NULL, &f->handlers,
                                               f->source_error) == MC_WORLD_SOURCE_OK &&
          mc_world_store_source_classes_bind_regions(f->handlers, f->regions, f->source_error) ==
              MC_WORLD_SOURCE_OK);
    mc_world_info_name *root = pg_text(world_root);
    CHECK(root != NULL &&
          mc_java_file_allocate(f->files, &f->root, &f->source_error->file) == MC_JAVA_FILE_OK &&
          mc_java_file_construct_string(f->root, root, &f->source_error->file) == MC_JAVA_FILE_OK);
    mc_world_info_name_release(root);
    CHECK(mc_client_tile_renderer_classes_bind(f->runtime.services.render_classes, f->world_classes,
                                               f->runtime.services.profile, NULL, &f->renderers,
                                               NULL) == MC_CLIENT_TILE_RENDERER_OK &&
          mc_client_tile_renderer_static_get(f->renderers, &f->manager, NULL) ==
              MC_CLIENT_TILE_RENDERER_OK);
    const mc_tile_entity_class_literal *piston = NULL;
    CHECK(mc_tile_entity_class_literal_get(f->renderers->tile_classes,
                                           MC_TILE_ENTITY_CLASS_KIND_PISTON,
                                           &piston) == MC_TILE_ENTITY_OK &&
          mc_client_tile_renderer_lookup_class(f->manager, piston, &f->hy, NULL) ==
              MC_CLIENT_TILE_RENDERER_OK &&
          f->hy != NULL);
    mc_client_graphics_error error;
    CHECK(mc_client_nw_active_use(
        f->runtime.graphics, (mc_client_graphics_site){"PistonGeometryFixture", "state", "()V", -1},
        &f->nw, &error));
    const mc_client_display_info *device = mc_client_display_info_get(f->display);
    CHECK(device != NULL);
    fputs("{\"schema\":\"NATIVE_PISTON_DIRECT_GEOMETRY_V1\",\"device\":{\"vendor\":", f->output);
    pg_string(f->output, device->vendor);
    fputs(",\"renderer\":", f->output);
    pg_string(f->output, device->renderer);
    fputs(",\"version\":", f->output);
    pg_string(f->output, device->version);
    fprintf(f->output, ",\"hidden\":true,\"monitor\":2,\"display_id\":%" PRIu32 "},\"rows\":[\n",
            selected);
    return 0;
}

static int pg_close(piston_geometry_fixture *f) {
    CHECK(f->call == NULL && f->rows == 27 &&
          mc_client_tile_renderer_classes_retire(&f->renderers, NULL) ==
              MC_CLIENT_TILE_RENDERER_OK);
    for (size_t i = 0; i < f->rows; ++i) {
        piston_geometry_world *w = &f->worlds[i];
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
          mc_resource_files_free(f->textures) == MC_REGISTRY_OK &&
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
    CHECK(fixture_close(&f->runtime) == 0);
    free(f->draw_error);
    free(f->lookup_error);
    free(f->tile_error);
    free(f->tick_error);
    free(f->source_error);
    free(f->region_error);
    return 0;
}

int main(int argc, char **argv) {
    CHECK(argc == 6);
    SDL_SetMainReady();
    piston_geometry_fixture *f = calloc(1, sizeof(*f));
    CHECK(f != NULL);
    f->output = fopen(argv[5], "wb");
    FILE *profiles = fopen(argv[4], "rb");
    CHECK(f->output != NULL && profiles != NULL && pg_open(f, argv[1], argv[2], argv[3]) == 0);
    char line[1024];
    unsigned int index = 0;
    while (fgets(line, sizeof(line), profiles) != NULL) {
        const size_t length = strlen(line);
        CHECK(length != 0 && line[length - 1] == '\n');
        line[length - 1] = '\0';
        if (length > 1 && line[length - 2] == '\r')
            line[length - 2] = '\0';
        piston_geometry_input input = {0};
        CHECK(pg_input(line, index, &input) == 0 && pg_draw(f, &input) == 0);
        ++index;
    }
    CHECK(index == 27 && !ferror(profiles) && fclose(profiles) == 0 &&
          pg_lighting_null_buffer(f) == 0 && pg_close(f) == 0);
    fprintf(f->output, ",\"cleanup\":true,\"rows_complete\":%u}\n", index);
    CHECK(!ferror(f->output) && fclose(f->output) == 0);
    free(f);
    printf("PASS: 27 Source Piston direct HY geometry rows; checks=%u; cleanup=1\n", checks);
    return 0;
}
