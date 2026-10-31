/* Actual original-equivalent prepared RAW App and genuine GAME/TH/Sheep owners.
 * No original observations are execution inputs. The included startup helpers
 * supply native allocation/services only; no App constructor/READY is forged. */
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#endif
int mc_animal_draw_startup_fixture_main(int argc, char **argv);
#define main mc_animal_draw_startup_fixture_main
#include "../startup/test_application_startup.c"
#undef main

#include "client/display/camera_surface.h"
#include "client/display/fog_surface.h"
#include "client/display/world_frame_surface.h"
#include "client/render/living_draw_internal.h"
#include "client/render/manager/draw_internal.h"
#include "client/render/manager/internal.h"
#include "client/render_engine/internal.h"
#include "client/render_engine/nw.h"
#include "client/render_engine/texture_lookup.h"
#include "client/texture_packs/internal.h"
#include "core/java/random_profile_internal.h"
#include "entity/mob/internal.h"
#include "mc/client/render/manager/draw.h"
#include "mc/entity/vector/vector.h"
#include "mc/world/runtime/environment.h"
#include "world/runtime/chunk_provider_internal.h"
#include "world/runtime/runtime_internal.h"
#include "world/storage/objects_internal.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <inttypes.h>
#include <math.h>

#define MOB_COUNT(a) (sizeof(a) / sizeof((a)[0]))

typedef struct mob_fixture {
    fixture runtime;
    client_application *app;
    mc_client_display *display;
    mc_client_default_font *font;
    mc_client_texture_pack_list *packs;
    mc_resource_files *textures;
    mc_math_classes *math;
    mc_client_render_manager_classes *managers;
    mc_client_render_manager *manager;
    mc_client_managed_renderer *renderer;
    mc_world_runtime_classes *world_classes;
    mc_world_regions *regions;
    mc_world_store *store;
    mc_world_info_name *name;
    mc_world_provider *dimension;
    mc_world_runtime *world;
    mc_chunk_provider_fields provider;
    mc_chunk_state *chunks[25];
    mc_entity *actor, *camera, *actors[4];
    const char *kind;
    mc_client_texture texture_names[7];
    mc_client_texture_lookup_error *lookup_error;
    mc_client_manager_draw_error *draw_error;
    mc_client_render_manager_draw_call *call;
    FILE *output;
} mob_fixture;

static uint32_t mob_fbits(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static uint64_t mob_dbits(double value) {
    uint64_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static void mob_ref(FILE *out, const void *pointer, const char *class_name) {
    if (pointer == NULL)
        fputs("null", out);
    else
        fprintf(out, "{\"class\":\"%s\",\"identity\":\"%016" PRIxPTR "\"}", class_name,
                (uintptr_t)pointer);
}

static void mob_string(FILE *out, const char *value) {
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

static void mob_float(FILE *out, float value) {
    fprintf(out, "{\"F\":\"%08" PRIx32 "\"}", mob_fbits(value));
}

static void mob_double(FILE *out, double value) {
    fprintf(out, "{\"D\":\"%016" PRIx64 "\"}", mob_dbits(value));
}

static void mob_field_ref(FILE *out, bool *first, const char *name, const void *pointer,
                          const char *class_name) {
    fprintf(out, "%s\"%s\":", *first ? "" : ",", name);
    *first = false;
    mob_ref(out, pointer, class_name);
}

static void mob_field_float(FILE *out, bool *first, const char *name, float value) {
    fprintf(out, "%s\"%s\":", *first ? "" : ",", name);
    *first = false;
    mob_float(out, value);
}

static void mob_field_double(FILE *out, bool *first, const char *name, double value) {
    fprintf(out, "%s\"%s\":", *first ? "" : ",", name);
    *first = false;
    mob_double(out, value);
}

static void mob_field_int(FILE *out, bool *first, const char *name, int32_t value) {
    fprintf(out, "%s\"%s\":%" PRId32, *first ? "" : ",", name, value);
    *first = false;
}

static void mob_field_bool(FILE *out, bool *first, const char *name, bool value) {
    fprintf(out, "%s\"%s\":%s", *first ? "" : ",", name, value ? "true" : "false");
    *first = false;
}

typedef enum mob_node_kind {
    MOB_TH,
    MOB_XY,
    MOB_MODEL,
    MOB_FH,
    MOB_PART,
    MOB_IB_ARRAY,
    MOB_TZ_ARRAY,
    MOB_VERTEX,
    MOB_QUAD,
    MOB_VECTOR
} mob_node_kind;

typedef struct mob_node {
    const void *pointer;
    const char *class_name;
    mob_node_kind kind;
    size_t length;
} mob_node;

typedef struct mob_graph {
    mob_node nodes[4096];
    size_t count;
} mob_graph;

static void mob_queue(mob_graph *graph, const void *pointer, const char *class_name,
                      mob_node_kind kind, size_t length) {
    if (pointer == NULL)
        return;
    for (size_t i = 0; i < graph->count; ++i)
        if (graph->nodes[i].pointer == pointer) {
            if (graph->nodes[i].kind != kind || graph->nodes[i].length != length ||
                strcmp(graph->nodes[i].class_name, class_name) != 0)
                abort();
            return;
        }
    if (graph->count == MOB_COUNT(graph->nodes))
        abort();
    graph->nodes[graph->count++] = (mob_node){pointer, class_name, kind, length};
}

static void mob_biped_fields(FILE *out, bool *first, mob_graph *graph,
                             const mc_client_biped_model *model, bool quadruped) {
    const mc_client_model_part *parts[] = {model->a, model->b, model->c, model->d, model->e,
                                           model->f, model->g, model->h, model->i};
    for (size_t i = quadruped ? 3u : 0u; i < MOB_COUNT(parts); ++i) {
        char name[16];
        (void)snprintf(name, sizeof(name), "%s.%c", quadruped ? "nn" : "fh", (int)('a' + i));
        mob_field_ref(out, first, name, parts[i], "ps");
        mob_queue(graph, parts[i], "ps", MOB_PART, 0);
    }
    if (!quadruped) {
        mob_field_bool(out, first, "fh.j", model->j);
        mob_field_bool(out, first, "fh.k", model->k);
        mob_field_bool(out, first, "fh.l", model->l);
    }
    mob_field_float(out, first, "ko.m", model->m);
    mob_field_bool(out, first, "ko.n", model->n);
}

static void mob_part_fields(FILE *out, const mc_client_model_part *part, mob_graph *graph) {
    bool first = true;
    const float values[] = {part->a, part->b, part->c, part->d, part->e, part->f};
    for (size_t i = 0; i < MOB_COUNT(values); ++i) {
        char name[8];
        (void)snprintf(name, sizeof(name), "ps.%c", (int)('a' + i));
        mob_field_float(out, &first, name, values[i]);
    }
    mob_field_bool(out, &first, "ps.g", part->g);
    mob_field_bool(out, &first, "ps.h", part->h);
    mob_field_bool(out, &first, "ps.i", part->i);
    mob_field_ref(out, &first, "ps.j", part->j, "[Lib;");
    mob_field_ref(out, &first, "ps.k", part->k, "[Ltz;");
    mob_field_int(out, &first, "ps.l", part->l);
    mob_field_int(out, &first, "ps.m", part->m);
    mob_field_bool(out, &first, "ps.n", part->n);
    mob_field_int(out, &first, "ps.o", part->o);
    if (graph != NULL) {
        mob_queue(graph, part->j, "[Lib;", MOB_IB_ARRAY, 8);
        mob_queue(graph, part->k, "[Ltz;", MOB_TZ_ARRAY, 6);
    }
}

static int mob_emit_graph(mob_fixture *f) {
    FILE *out = f->output;
    mc_client_render_manager *manager = f->manager;
    mc_client_managed_renderer *renderer = f->renderer;
    mob_graph *graph = calloc(1, sizeof(*graph));
    CHECK(graph != NULL);
    fputs("{\"roots\":{", out);
    bool first = true;
    mob_field_ref(out, &first, "manager", manager, "th");
    mob_field_ref(out, &first, "renderer", renderer, renderer->source_class);
    mob_field_ref(out, &first, "primary", renderer->primary, renderer->primary->source_class);
    mob_field_ref(out, &first, "fur", renderer->secondary,
                  renderer->secondary ? renderer->secondary->source_class : "ko");
    mob_field_ref(out, &first, "actor", f->actor, f->kind);
    mob_field_ref(out, &first, "camera", f->camera, "bx");
    mob_field_ref(out, &first, "world", f->world, "fd");
    mob_field_ref(out, &first, "engine", f->app->render_engine, "ji");
    mob_field_ref(out, &first, "settings", f->app->settings, "kv");
    fputs("},\"objects\":[", out);
    mob_queue(graph, manager, "th", MOB_TH, 0);
    mob_queue(graph, renderer, renderer->source_class, MOB_XY, 0);
    mob_queue(graph, renderer->primary, renderer->primary->source_class, MOB_MODEL, 0);
    mob_queue(graph, renderer->secondary,
              renderer->secondary ? renderer->secondary->source_class : "ko", MOB_MODEL, 0);
    for (size_t index = 0; index < graph->count; ++index) {
        const mob_node node = graph->nodes[index];
        fprintf(out, "%s{\"class\":\"%s\",\"identity\":\"%016" PRIxPTR "\"", index ? "," : "",
                node.class_name, (uintptr_t)node.pointer);
        if (node.kind == MOB_IB_ARRAY || node.kind == MOB_TZ_ARRAY) {
            fprintf(out, ",\"length\":%zu,\"values\":[", node.length);
            for (size_t i = 0; i < node.length; ++i) {
                const void *value =
                    node.kind == MOB_IB_ARRAY
                        ? (const void *)((mc_client_model_vertex *const *)node.pointer)[i]
                        : (const void *)((mc_client_model_quad *const *)node.pointer)[i];
                if (i)
                    fputc(',', out);
                mob_ref(out, value, node.kind == MOB_IB_ARRAY ? "ib" : "tz");
                mob_queue(graph, value, node.kind == MOB_IB_ARRAY ? "ib" : "tz",
                          node.kind == MOB_IB_ARRAY ? MOB_VERTEX : MOB_QUAD, 0);
            }
            fputs("]}", out);
            continue;
        }
        fputs(",\"fields\":{", out);
        first = true;
        switch (node.kind) {
            case MOB_TH:
                mob_field_ref(out, &first, "th.e", manager->fields.e, "ji");
                mob_field_ref(out, &first, "th.f", manager->fields.f, "ra");
                mob_field_ref(out, &first, "th.g", manager->fields.g, "fd");
                mob_field_ref(out, &first, "th.h", manager->fields.h, "bx");
                mob_field_float(out, &first, "th.i", manager->fields.i);
                mob_field_float(out, &first, "th.j", manager->fields.j);
                mob_field_ref(out, &first, "th.k", manager->fields.k, "kv");
                mob_field_double(out, &first, "th.l", manager->fields.l);
                mob_field_double(out, &first, "th.m", manager->fields.m);
                mob_field_double(out, &first, "th.n", manager->fields.n);
                mob_field_ref(out, &first, "th.o", manager->fields.o, "java.util.HashMap");
                mob_field_ref(out, &first, "th.p", manager->fields.p, "sj");
                break;
            case MOB_XY:
                mob_field_ref(out, &first, "bw.a", renderer->bw_model, "fh");
                mob_field_ref(out, &first, "bw.b", renderer->manager, "th");
                mob_field_float(out, &first, "bw.c", renderer->shadow_size);
                mob_field_float(out, &first, "bw.d", renderer->shadow_opacity);
                mob_field_ref(out, &first, "bw.e", renderer->bw_blocks, "cv");
                mob_field_ref(out, &first, "gv.e", renderer->primary,
                              renderer->primary->source_class);
                mob_field_ref(out, &first, "gv.f", renderer->secondary,
                              renderer->secondary ? renderer->secondary->source_class : "ko");
                mob_queue(graph, renderer->bw_model, "fh", MOB_FH, 0);
                break;
            case MOB_MODEL: {
                const manager_model *model = node.pointer;
                if (strcmp(model->source_class, "td") == 0) {
                    const mc_client_model_part *parts[] = {
                        model->fields.a, model->fields.b, model->fields.c, model->fields.d,
                        model->fields.e, model->fields.f, model->fields.g, model->fields.h};
                    for (size_t i = 0; i < 8; ++i) {
                        char name[8];
                        (void)snprintf(name, sizeof(name), "td.%c", (int)('a' + i));
                        mob_field_ref(out, &first, name, parts[i], "ps");
                        mob_queue(graph, parts[i], "ps", MOB_PART, 0);
                    }
                    mob_field_float(out, &first, "ko.m", model->fields.m);
                    mob_field_bool(out, &first, "ko.n", model->fields.n);
                } else {
                    mob_biped_fields(out, &first, graph, &model->fields, true);
                    if (strcmp(model->source_class, "hh") == 0) {
                        const mc_client_model_part *parts[] = {model->fields.a, model->fields.b,
                                                               model->fields.c};
                        for (size_t i = 0; i < 3; ++i) {
                            char name[8];
                            (void)snprintf(name, sizeof(name), "hh.%c", (int)('a' + i));
                            mob_field_ref(out, &first, name, parts[i], "ps");
                            mob_queue(graph, parts[i], "ps", MOB_PART, 0);
                        }
                    }
                }
                break;
            }
            case MOB_FH:
                mob_biped_fields(out, &first, graph, node.pointer, false);
                break;
            case MOB_PART:
                mob_part_fields(out, node.pointer, graph);
                break;
            case MOB_VERTEX: {
                const mc_client_model_vertex *v = node.pointer;
                mob_field_ref(out, &first, "ib.a", v->a, "bt");
                mob_field_float(out, &first, "ib.b", v->b);
                mob_field_float(out, &first, "ib.c", v->c);
                mob_queue(graph, v->a, "bt", MOB_VECTOR, 0);
                break;
            }
            case MOB_QUAD: {
                const mc_client_model_quad *q = node.pointer;
                mob_field_ref(out, &first, "tz.a", q->a, "[Lib;");
                mob_field_int(out, &first, "tz.b", q->b);
                mob_field_bool(out, &first, "tz.c", q->c);
                mob_queue(graph, q->a, "[Lib;", MOB_IB_ARRAY, 4);
                break;
            }
            case MOB_VECTOR: {
                const mc_entity_vector *v = node.pointer;
                mob_field_double(out, &first, "bt.a", v->x);
                mob_field_double(out, &first, "bt.b", v->y);
                mob_field_double(out, &first, "bt.c", v->z);
                break;
            }
            default:
                abort();
        }
        fputs("}}", out);
    }
    fputs("]}", out);
    CHECK(graph->count == (strcmp(f->kind, "bx") == 0 ? 994u
                           : strcmp(f->kind, "ww") == 0
                               ? 939u
                               : 1160u)); /* Independent actual fixed constructor topology. */
    free(graph);
    return 0;
}

static int mob_world_open(mob_fixture *f, const char *directory) {
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

static int mob_construct(mob_fixture *f, mc_entity_class_kind kind, mc_entity **out) {
    const mc_entity_class_literal *literal = NULL;
    CHECK(mc_entity_class_literal_get(f->runtime.services.entities, kind, &literal) ==
          MC_ENTITY_OK);
    mc_mob_error error;
    CHECK(mc_mob_allocate(f->runtime.services.entities, f->runtime.services.registry, literal, out,
                          &error) == MC_MOB_OK);
    CHECK(mc_mob_construct(*out, f->world, &error) == MC_MOB_OK);
    return 0;
}

static int mob_pose(mc_entity *entity, double x, double y, double z, float yaw, float pitch) {
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

static int mob_select(mob_fixture *f, unsigned int index) {
    static const unsigned int selected[] = {0, 1, 1, 2, 2, 3, 3, 0};
    static const char *const kinds[] = {"dl", "wh", "bx", "ww"};
    CHECK(index < 8);
    f->actor = f->actors[selected[index]];
    f->kind = kinds[selected[index]];
    f->renderer = NULL;
    for (size_t i = 0; i < f->manager->map->capacity; ++i)
        for (manager_map_entry *e = f->manager->map->buckets[i]; e; e = e->next)
            if (e->key == f->actor->class_literal)
                f->renderer = e->owner;
    CHECK(f->renderer != NULL && f->renderer->primary != NULL);
    return 0;
}

static int mob_prepare(mob_fixture *f) {
    const mc_entity_class_kind kinds[] = {MC_ENTITY_CLASS_KIND_SHEEP, MC_ENTITY_CLASS_KIND_PIG,
                                          MC_ENTITY_CLASS_KIND_COW, MC_ENTITY_CLASS_KIND_CHICKEN};
    for (size_t i = 0; i < 4; ++i)
        CHECK(mob_construct(f, kinds[i], &f->actors[i]) == 0);
    CHECK(mob_construct(f, MC_ENTITY_CLASS_KIND_COW, &f->camera) == 0);
    for (size_t i = 0; i < 4; ++i) {
        mc_entity *actor = f->actors[i];
        CHECK(mob_pose(actor, 8.5, 64, 8.5, 30, 15) == 0);
        actor->base.bl = actor->base.bn = 8.25;
        actor->base.bm = 64;
        actor->base.aU = 10;
        actor->base.aV = -5;
        actor->base.bt = 17;
        mc_living_entity_fields *l = &actor->mob->living;
        l->I = 10;
        l->H = 20;
        l->ak = .2f;
        l->al = .4f;
        l->am = 3;
        l->W = .1f;
        l->X = .4f;
        bool inserted = false;
        CHECK(mc_world_runtime_insert_entity(f->world, actor, &inserted, NULL) ==
                  MC_WORLD_STORAGE_OK &&
              inserted);
    }
    f->actors[3]->mob->chicken.g = .25f;
    f->actors[3]->mob->chicken.b = .75f;
    f->actors[3]->mob->chicken.f = .5f;
    f->actors[3]->mob->chicken.c = 1.25f;
    CHECK(mob_pose(f->camera, 8.5, 65, 12.5, 180, 15) == 0);
    bool inserted = false;
    CHECK(mc_world_runtime_insert_entity(f->world, f->camera, &inserted, NULL) ==
              MC_WORLD_STORAGE_OK &&
          inserted);
    CHECK(mc_client_render_manager_classes_bind(
              f->runtime.services.render_classes, f->runtime.services.registry,
              f->runtime.services.profile, NULL, &f->managers, NULL) == MC_CLIENT_MANAGER_OK &&
          mc_client_render_manager_static_get(f->managers, &f->manager, NULL) ==
              MC_CLIENT_MANAGER_OK);
    CHECK(mc_client_render_manager_bind_frame(f->manager, f->math, f->world, f->app->render_engine,
                                              NULL, f->camera, f->app->settings, .375f,
                                              NULL) == MC_CLIENT_MANAGER_OK);
    CHECK(mc_client_render_manager_origin_store(f->managers, 0, 8.5, NULL) ==
              MC_CLIENT_MANAGER_OK &&
          mc_client_render_manager_origin_store(f->managers, 1, 64, NULL) == MC_CLIENT_MANAGER_OK &&
          mc_client_render_manager_origin_store(f->managers, 2, 8.5, NULL) == MC_CLIENT_MANAGER_OK);
    CHECK(mob_select(f, 0) == 0 && f->manager->fields.f == NULL && f->manager->fields.p == NULL);
    return 0;
}

static int mob_open(mob_fixture *f, const char *resources, const char *textures,
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
    CHECK(mc_client_display_create("Genuine animal draw", 256, 256, true, &f->display, NULL));
    f->app->display = f->display;
    f->lookup_error = calloc(1, sizeof(*f->lookup_error));
    f->draw_error = calloc(1, sizeof(*f->draw_error));
    CHECK(f->lookup_error && f->draw_error && mob_world_open(f, directory) == 0);
    return 0;
}

static mc_texture_pack_text mob_text(const char *text, uint16_t units[128]) {
    const size_t length = strlen(text);
    if (length > 128)
        abort();
    for (size_t i = 0; i < length; ++i)
        units[i] = (uint16_t)(unsigned char)text[i];
    return (mc_texture_pack_text){units, length, false};
}

static int mob_textures(mob_fixture *f) {
    static const char *const keys[] = {
        "/mob/sheep.png",  "/mob/sheep_fur.png", "%clamp%/misc/shadow.png", "/mob/pig.png",
        "/mob/saddle.png", "/mob/cow.png",       "/mob/chicken.png"};
    mc_client_render_engine *engine = f->app->render_engine;
    CHECK(engine->fields.b->count == 0);
    for (unsigned int i = 0; i < MOB_COUNT(keys); ++i) {
        uint16_t units[128];
        const mc_texture_pack_text text = mob_text(keys[i], units);
        const size_t before = engine->fields.b->count;
        bool found = true;
        mc_client_texture cached = 0;
        CHECK(mc_client_render_engine_cached_texture(engine, text, &found, &cached) && !found);
        CHECK(mc_client_render_engine_texture(engine, f->display, NULL, text, &f->texture_names[i],
                                              f->lookup_error) == MC_CLIENT_LOOKUP_OK);
        mc_client_texture_lookup_state lookup;
        /* Successful attempts are retired, not retained as a last-result view.
         * Observe the real Map/ID/transfer/display effects instead. */
        CHECK(mc_client_render_engine_lookup_inspect(engine, &lookup) && !lookup.pending &&
              mc_client_render_engine_cached_texture(engine, text, &found, &cached) && found &&
              cached == f->texture_names[i]);
        mc_client_texture_source_state state;
        CHECK(mc_client_display_texture_state_inspect(f->display, f->texture_names[i], &state) &&
              state.known && state.image_uploaded && state.min_filter == GL_NEAREST &&
              state.mag_filter == GL_NEAREST && state.wrap_s == (i == 2 ? GL_CLAMP : GL_REPEAT) &&
              state.wrap_t == state.wrap_s && engine->fields.b->count == before + 1 &&
              !engine->fields.j && !engine->fields.k);
        fprintf(f->output,
                "{\"event\":\"texture\",\"index\":%u,\"key\":\"%s\",\"name\":%u,\"cache_before\":%"
                "zu,\"cache_after\":%zu,\"wrap_s\":%d,\"wrap_t\":%d,\"min\":%d,\"mag\":%d,"
                "\"transfer_hex\":\"",
                i, keys[i], f->texture_names[i], before, engine->fields.b->count, state.wrap_s,
                state.wrap_t, state.min_filter, state.mag_filter);
        CHECK(engine->fields.f != NULL && engine->fields.f->limit == (i == 2 ? 65536u : 8192u));
        for (size_t byte = 0; byte < engine->fields.f->limit; ++byte)
            fprintf(f->output, "%02x", engine->fields.f->data[byte]);
        const size_t generated = f->runtime.graphics->b->count;
        CHECK(mc_client_render_engine_texture(engine, f->display, NULL, text, &cached,
                                              f->lookup_error) == MC_CLIENT_LOOKUP_OK &&
              cached == f->texture_names[i]);
        CHECK(mc_client_render_engine_lookup_inspect(engine, &lookup) && !lookup.pending &&
              engine->fields.b->count == before + 1 && f->runtime.graphics->b->count == generated);
        fprintf(f->output, "\",\"cached_name\":%u}\n", cached);
    }
    return 0;
}

typedef GLboolean(APIENTRY *mob_enabled_fn)(GLenum);
typedef void(APIENTRY *mob_boolean_fn)(GLenum, GLboolean *);
typedef void(APIENTRY *mob_integer_fn)(GLenum, GLint *);
typedef void(APIENTRY *mob_read_fn)(GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void *);
typedef void(APIENTRY *mob_finish_fn)(void);

static int mob_emit_gl(mob_fixture *f) {
    mc_client_display *display = f->display;
    float color[4], projection[16], modelview[16], normal[3];
    CHECK(mc_client_display_source_current_attribute(display, GL_CURRENT_COLOR, color, 4, NULL));
    CHECK(mc_client_display_camera_matrices_inspect(display, projection, modelview));
    CHECK(mc_client_display_source_current_attribute(display, GL_CURRENT_NORMAL, normal, 3, NULL));
    int32_t mode;
    size_t depth, projection_depth;
    CHECK(mc_client_display_camera_stack_inspect(display, &mode, &depth, &projection_depth));
    mc_client_source_draw_state draw;
    mc_client_source_fog_state fog;
    CHECK(mc_client_display_source_draw_inspect(display, &draw) &&
          mc_client_display_source_fog_inspect(display, &fog));
    mob_enabled_fn enabled = (mob_enabled_fn)SDL_GL_GetProcAddress("glIsEnabled");
    mob_boolean_fn boolean = (mob_boolean_fn)SDL_GL_GetProcAddress("glGetBooleanv");
    mob_integer_fn integer = (mob_integer_fn)SDL_GL_GetProcAddress("glGetIntegerv");
    CHECK(enabled && boolean && integer);
    GLboolean mask = GL_FALSE;
    GLint function = 0, source = 0, destination = 0, texture = 0;
    boolean(GL_DEPTH_WRITEMASK, &mask);
    integer(GL_DEPTH_FUNC, &function);
    integer(GL_BLEND_SRC_RGB, &source);
    integer(GL_BLEND_DST_RGB, &destination);
    integer(GL_TEXTURE_BINDING_2D, &texture);
    FILE *out = f->output;
    fputs("{\"color\":[", out);
    for (size_t i = 0; i < 4; ++i)
        fprintf(out, "%s\"%08" PRIx32 "\"", i ? "," : "", mob_fbits(color[i]));
    fputs("],\"normal\":[", out);
    for (size_t i = 0; i < 3; ++i)
        fprintf(out, "%s\"%08" PRIx32 "\"", i ? "," : "", mob_fbits(normal[i]));
    fputs("],\"modelview\":[", out);
    for (size_t i = 0; i < 16; ++i)
        fprintf(out, "%s\"%08" PRIx32 "\"", i ? "," : "", mob_fbits(modelview[i]));
    fputs("],\"projection\":[", out);
    for (size_t i = 0; i < 16; ++i)
        fprintf(out, "%s\"%08" PRIx32 "\"", i ? "," : "", mob_fbits(projection[i]));
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

static size_t mob_parts(const manager_model *model, const mc_client_model_part *parts[9]) {
    if (model == NULL)
        return 0;
    const mc_client_biped_model *m = &model->fields;
    if (strcmp(model->source_class, "td") == 0) {
        const mc_client_model_part *p[] = {m->a, m->g, m->h, m->b, m->c, m->d, m->e, m->f};
        memcpy(parts, p, sizeof(p));
        return 8;
    }
    const mc_client_model_part *p[] = {m->d, m->e, m->f, m->g, m->h, m->i, m->b, m->c, m->a};
    const size_t count = strcmp(model->source_class, "hh") == 0 ? 9u : 6u;
    memcpy(parts, p, count * sizeof(*parts));
    return count;
}

static int mob_emit_state(mob_fixture *f, unsigned int index, const char *phase) {
    FILE *out = f->output;
    fprintf(out, "{\"event\":\"state\",\"case\":%u,\"phase\":\"%s\",\"graph\":", index, phase);
    CHECK(mob_emit_graph(f) == 0);
    fputs(",\"parts\":[", out);
    const manager_model *models[] = {f->renderer->primary, f->renderer->secondary};
    size_t ordinal = 0;
    for (size_t model = 0; model < 2; ++model) {
        const mc_client_model_part *parts[9];
        const size_t count = mob_parts(models[model], parts);
        for (size_t i = 0; i < count; ++i) {
            const mc_client_model_part *part = parts[i];
            mc_client_source_list_state state = {0};
            const bool valid = part->o > 0 &&
                               mc_client_display_list_inspect(f->display, part->o, &state) &&
                               state.defined;
            fprintf(out, "%s{\"part\":", ordinal++ ? "," : "");
            mob_ref(out, part, "ps");
            fputs(",\"fields\":{", out);
            mob_part_fields(out, part, NULL);
            fprintf(out, "},\"is_list\":%s}", valid ? "true" : "false");
        }
    }
    const mc_entity_classes *classes = f->runtime.services.entities;
    const mc_entity_vector_pool *pool = classes->vector_pool;
    CHECK(pool != NULL);
    fprintf(out, "],\"pool\":%d,\"pool_storage\":{\"storage\":", classes->vector_cursor);
    mob_ref(out, pool, "java.util.ArrayList");
    fprintf(out, ",\"length\":%zu,\"vectors\":[", pool->length);
    for (size_t i = 0; i < pool->length; ++i) {
        const mc_entity_vector *v = pool->elements[i];
        fprintf(out, "%s{\"actual\":", i ? "," : "");
        mob_ref(out, v, "bt");
        fputs(",\"fields\":{", out);
        bool first = true;
        mob_field_double(out, &first, "bt.a", v->x);
        mob_field_double(out, &first, "bt.b", v->y);
        mob_field_double(out, &first, "bt.c", v->z);
        fputs("}}", out);
    }
    fputs("]},\"gl\":", out);
    CHECK(mob_emit_gl(f) == 0);
    const mc_java_random_profile *profile = f->runtime.services.profile;
    fprintf(out,
            ",\"entropy\":{\"actor\":\"%012" PRIx64 "\",\"camera\":\"%012" PRIx64
            "\",\"world\":\"%012" PRIx64 "\",\"math\":\"%012" PRIx64
            "\",\"uniquifier\":{\"J\":\"%016" PRIx64 "\"}}}\n",
            f->actor->random->state, f->camera->random->state, f->world->fields.r->state,
            profile->math->state, atomic_load_explicit(&profile->uniquifier, memory_order_relaxed));
    return 0;
}

static int mob_frame(mob_fixture *f) {
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
    /* Authored observer background, outside all observed source calls/NW.
     * The same plane is specified by the original helper's immediate quad. */
    const float points[6][3] = {{-3, 0, -3}, {-3, 0, 3}, {3, 0, 3},
                                {-3, 0, -3}, {3, 0, 3},  {3, 0, -3}};
    uint32_t words[6][8] = {{0}};
    for (size_t vertex = 0; vertex < 6; ++vertex)
        for (size_t axis = 0; axis < 3; ++axis)
            words[vertex][axis] = mob_fbits(points[vertex][axis]);
    mc_client_display_context_lease *lease = NULL;
    CHECK(mc_client_display_context_acquire(d, &lease, NULL));
    for (unsigned int array = 0; array < 4; ++array)
        CHECK(mc_client_display_source_array(d, (mc_client_source_array)array, array == 0, NULL));
    CHECK(mc_client_display_source_pointer(lease, MC_CLIENT_SOURCE_VERTEX_ARRAY, words, 0, NULL));
    CHECK(mc_client_display_source_batch(lease, words, (const uint8_t *)words, sizeof(words), 6,
                                         GL_TRIANGLES, NULL));
    CHECK(mc_client_display_source_array(d, MC_CLIENT_SOURCE_VERTEX_ARRAY, false, NULL) &&
          mc_client_display_context_release(&lease, NULL));
    CHECK(mc_client_display_source_texture_enable(d, true, NULL) &&
          mc_client_display_source_alpha_test(d, true, NULL) &&
          mc_client_display_source_alpha_function(d, GL_GREATER, .1f, NULL) &&
          mc_client_display_source_rescale_normal(d, false, NULL) &&
          mc_client_display_source_normalize(d, false, NULL) &&
          mc_client_display_source_color(d, .125f, .25f, .5f, .75f, NULL));
    return 0;
}

static int mob_pixels(mob_fixture *f, const char *directory, unsigned int index, bool baseline) {
    CHECK(f->display != NULL);
    char path[4096];
    CHECK(snprintf(path, sizeof(path), "%s/%s%u.rgba", directory, baseline ? "baseline" : "draw",
                   index) > 0);
    mob_read_fn read = (mob_read_fn)SDL_GL_GetProcAddress("glReadPixels");
    mob_finish_fn finish = (mob_finish_fn)SDL_GL_GetProcAddress("glFinish");
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

static void mob_words(FILE *out, const uint8_t *bytes, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        uint32_t word;
        memcpy(&word, bytes + i * sizeof(word), sizeof(word));
        fprintf(out, "%s\"%08" PRIx32 "\"", i ? "," : "", word);
    }
}

static int mob_lists(mob_fixture *f) {
    const manager_model *models[] = {f->renderer->primary, f->renderer->secondary};
    for (size_t model = 0; model < 2; ++model) {
        const mc_client_model_part *parts[9];
        const size_t count = mob_parts(models[model], parts);
        for (size_t part = 0; part < count; ++part) {
            if (!parts[part]->n)
                continue;
            mc_client_source_list_state state;
            CHECK(mc_client_display_list_inspect(f->display, parts[part]->o, &state) &&
                  state.defined && !state.building && state.draws == 6 && state.vertices == 36);
            fprintf(f->output, "{\"event\":\"list\",\"part\":");
            mob_ref(f->output, parts[part], "ps");
            fprintf(f->output, ",\"list\":%d,\"commands\":%zu,\"batches\":[", parts[part]->o,
                    state.commands);
            for (size_t face = 0; face < 6; ++face) {
                mc_client_source_batch_view batch;
                CHECK(mc_client_display_list_batch(f->display, parts[part]->o, face, &batch) &&
                      batch.vertices == 6 && batch.byte_count == 48 * sizeof(uint32_t));
                fprintf(f->output, "%s{\"vertices\":%d,\"mode\":%d,\"arrays\":%u,\"words\":[",
                        face ? "," : "", batch.vertices, batch.mode, batch.arrays);
                mob_words(f->output, batch.bytes, batch.byte_count / sizeof(uint32_t));
                fputs("]}", f->output);
            }
            fputs("]}\n", f->output);
        }
    }
    return 0;
}

static void mob_site(FILE *out, mc_client_graphics_site site) {
    fprintf(out, "{\"class\":\"%s\",\"method\":\"%s\",\"descriptor\":\"%s\",\"pc\":%d}",
            site.class_name, site.method, site.descriptor, site.pc);
}

static int mob_texture_calls(mob_fixture *f, unsigned int index,
                             const mc_client_manager_draw_state *state) {
    const mc_client_living_draw_state *states[] = {state->living, state->post_draw};
    fprintf(f->output, "{\"event\":\"texture_calls\",\"case\":%u,\"calls\":[", index);
    size_t ordinal = 0;
    for (size_t phase = 0; phase < 2; ++phase) {
        CHECK(states[phase] != NULL &&
              states[phase]->texture_call_count <= MC_CLIENT_LIVING_DRAW_TEXTURE_CALLS);
        for (size_t i = 0; i < states[phase]->texture_call_count; ++i) {
            const mc_client_living_draw_texture_call *call = &states[phase]->texture_calls[i];
            CHECK(call->engine_identity == (uintptr_t)f->app->render_engine &&
                  call->key_identity != 0 && call->attempted && call->lookup_entered &&
                  call->lookup_returned && call->bind_attempted && call->bind_returned &&
                  call->returned && !call->failed && !call->native_policy &&
                  !call->source_exception && call->exception_class == NULL && !call->key_null &&
                  call->key_copied && call->key_length == call->key_stored &&
                  call->key_stored <= 64);
            fprintf(f->output, "%s{\"phase\":\"%s\",\"engine\":", ordinal++ ? "," : "",
                    phase ? "post" : "living");
            mob_ref(f->output, (const void *)call->engine_identity, "ji");
            fputs(",\"key\":", f->output);
            mob_ref(f->output, (const void *)call->key_identity, "java.lang.String");
            fputs(",\"units\":[", f->output);
            for (size_t unit = 0; unit < call->key_stored; ++unit)
                fprintf(f->output, "%s%u", unit ? "," : "", (unsigned int)call->key_units[unit]);
            fputs("],\"caller\":", f->output);
            mob_site(f->output, call->caller);
            fputs(",\"lookup_site\":", f->output);
            mob_site(f->output, call->lookup_site);
            fputs(",\"bind_site\":", f->output);
            mob_site(f->output, call->bind_site);
            fprintf(f->output, ",\"texture_id\":%d,\"returned\":true}", call->texture_id);
        }
    }
    fputs("]}\n", f->output);
    return 0;
}

static int mob_queries(mob_fixture *f, unsigned int index) {
    mc_registry_block_view blocks;
    CHECK(mc_registry_classes_inspect_blocks(f->runtime.services.registry, &blocks, NULL) ==
          MC_REGISTRY_OK);
    const size_t count = mc_client_render_manager_draw_query_count(f->call);
    fprintf(f->output, "{\"event\":\"queries\",\"case\":%u,\"queries\":[", index);
    for (size_t i = 0; i < count; ++i) {
        mc_client_manager_draw_query query;
        CHECK(mc_client_render_manager_draw_query_at(f->call, i, &query) && query.nargs <= 6 &&
              query.invoked && query.returned && !query.failed && !query.native_policy &&
              !query.source_exception && query.exception_class == NULL);
        fprintf(f->output, "%s{\"receiver\":\"%016" PRIxPTR "\",\"receiver_role\":\"", i ? "," : "",
                query.receiver);
        if (query.receiver == (uintptr_t)f->world)
            fputs("world", f->output);
        else {
            size_t block = 0;
            while (block < 256u && (uintptr_t)blocks.m[block] != query.receiver)
                ++block;
            CHECK(block < 256u && blocks.m[block] != NULL);
            fprintf(f->output, "block:%zu", block);
        }
        fputs("\",\"method\":", f->output);
        mob_site(f->output, query.method);
        fputs(",\"caller\":", f->output);
        mob_site(f->output, query.caller);
        fputs(",\"arguments\":[", f->output);
        for (size_t argument = 0; argument < query.nargs; ++argument)
            fprintf(f->output, "%s%d", argument ? "," : "", query.args[argument]);
        fputs("],\"value\":", f->output);
        switch (query.kind) {
            case MC_CLIENT_MANAGER_DRAW_QUERY_INTEGER:
                fprintf(f->output, "%d", query.value.integer);
                break;
            case MC_CLIENT_MANAGER_DRAW_QUERY_BOOLEAN:
                fputs(query.value.boolean ? "true" : "false", f->output);
                break;
            case MC_CLIENT_MANAGER_DRAW_QUERY_FLOAT_BITS:
                fprintf(f->output, "{\"F\":\"%08" PRIx32 "\"}", query.value.float_bits);
                break;
        }
        fputs(",\"invoked\":true,\"returned\":true,\"failed\":false,\"native_policy\":false,"
              "\"source_exception\":false,\"exception_class\":null}",
              f->output);
    }
    fputs("]}\n", f->output);
    return 0;
}

static int mob_model_work(mob_fixture *f, unsigned int index, const char *phase,
                          const mc_client_model_runtime_state *work) {
    CHECK(work != NULL && work->model != NULL && work->runtime_class != NULL && work->returned &&
          work->pose_returned);
    const bool cow = strcmp(work->runtime_class, "hh") == 0,
               chicken = strcmp(work->runtime_class, "td") == 0;
    const unsigned int stores = cow || chicken ? 11u : 7u, math = chicken ? 2u : 4u,
                       parts = cow       ? 9u
                               : chicken ? 8u
                                         : 6u;
    CHECK(work->pose_stores == stores && work->math_calls == math && work->part_calls == parts &&
          work->part_returns == parts);
    if (cow)
        CHECK(work->nn_pose_returned && work->nn_render_returned);
    fprintf(f->output, "{\"event\":\"model_work\",\"case\":%u,\"phase\":\"%s\",\"model\":", index,
            phase);
    mob_ref(f->output, work->model, work->runtime_class);
    fprintf(f->output,
            ",\"pose_stores\":%u,\"math_calls\":%u,\"part_calls\":%u,\"part_returns\":%u,\"nn_pose_"
            "returned\":%s,\"nn_render_returned\":%s,\"pose_site\":",
            work->pose_stores, work->math_calls, work->part_calls, work->part_returns,
            work->nn_pose_returned ? "true" : "false", work->nn_render_returned ? "true" : "false");
    mob_site(f->output, work->pose_site);
    fputs(",\"render_site\":", f->output);
    mob_site(f->output, work->render_site);
    fputs("}\n", f->output);
    return 0;
}

static int mob_profile(mob_fixture *f, unsigned int index, const char *directory) {
    CHECK(mob_frame(f) == 0 && mob_pixels(f, directory, index, true) == 0 &&
          mob_emit_state(f, index, "before") == 0);
    const uint64_t own = f->actor->random->state, camera = f->camera->random->state,
                   world = f->world->fields.r->state,
                   shared = f->runtime.services.profile->math->state;
    const int32_t pool = f->runtime.services.entities->vector_cursor;
    const size_t app_borrows = atomic_load(&f->app->render_borrows);
    const mc_client_manager_draw_services services = {f->math, f->runtime.graphics, f->display,
                                                      f->runtime.services.application_classes};
    const float partial = index == 6 ? .625f : .375f;
    const mc_client_manager_status status = mc_client_render_manager_draw_begin(
        f->manager, f->actor, partial, &services, NULL, &f->call, f->draw_error);
    if (status != MC_CLIENT_MANAGER_OK)
        fprintf(stderr, "Mob draw failed case%u status%d at%s.%s:%d op=%s\n", index, (int)status,
                f->draw_error->site.class_name ? f->draw_error->site.class_name : "?",
                f->draw_error->site.method ? f->draw_error->site.method : "?",
                f->draw_error->site.pc, f->draw_error->operation ? f->draw_error->operation : "?");
    CHECK(status == MC_CLIENT_MANAGER_OK && f->call != NULL);
    mc_client_manager_draw_state state;
    CHECK(mc_client_render_manager_draw_inspect(f->call, &state) && state.returned &&
          state.renderer == f->renderer && state.actor == f->actor);
    CHECK(!state.post_only && state.primary_returned && state.post_draw_returned && state.living &&
          state.living->returned && state.post_draw && state.post_draw->shadow_settings &&
          state.post_draw->shadow_returned &&
          state.post_draw->shadow_quads == (index == 5 || index == 6 ? 1u : 9u) &&
          state.post_draw->shadow_vertices == (index == 5 || index == 6 ? 4u : 36u) &&
          state.post_draw->fire_checked && !state.post_draw->burning);
    CHECK(state.living->texture_call_count == (index == 1 || index == 2   ? 4u
                                               : index == 0 || index == 7 ? 1u
                                                                          : 0u));
    for (size_t pass = 0; pass < 4; ++pass)
        CHECK(state.living->pass_attempted[pass] &&
              state.living->pass_result[pass] == ((index == 0 || index == 7) && pass == 0));
    CHECK(state.living->primary_render.part_calls == (index == 3 || index == 4   ? 9u
                                                      : index == 5 || index == 6 ? 8u
                                                                                 : 6u) &&
          state.living->primary_render.part_returns == state.living->primary_render.part_calls);
    CHECK(state.living->flap_arguments_read == (index == 5 || index == 6) &&
          state.living->flap_math_returned == (index == 5 || index == 6));
    if (index == 3 || index == 4)
        CHECK(state.living->primary_render.nn_pose_returned &&
              state.living->primary_render.nn_render_returned);
    fprintf(f->output,
            "{\"event\":\"animation\",\"case\":%u,\"age\":{\"F\":\"%08" PRIx32
            "\"},\"flap_arguments_read\":%s,\"flap_math_returned\":%s,\"flap_first\":{\"F\":\"%"
            "08" PRIx32 "\"},\"flap_second\":{\"F\":\"%08" PRIx32 "\"}}\n",
            index, mob_fbits(state.living->age),
            state.living->flap_arguments_read ? "true" : "false",
            state.living->flap_math_returned ? "true" : "false",
            mob_fbits(state.living->flap_first), mob_fbits(state.living->flap_second));
    CHECK(mob_model_work(f, index, "primary", &state.living->primary_render) == 0);
    if (index == 0 || index == 7)
        CHECK(mob_model_work(f, index, "pass0", &state.living->pass_render[0]) == 0);
    CHECK(f->actor->random->state == own && f->camera->random->state == camera &&
          f->world->fields.r->state == world && f->runtime.services.profile->math->state == shared);
    static const int32_t growth[] = {288, 144, 0, 216, 0, 192, 0, 0};
    CHECK(f->runtime.services.entities->vector_cursor == pool + growth[index]);
    CHECK(mob_emit_state(f, index, "after") == 0 && mob_pixels(f, directory, index, false) == 0);
    mc_client_nw_state nw;
    CHECK(mc_client_nw_inspect(f->runtime.graphics, &nw) && nw.fields.e &&
          nw.fields.e->position == (index == 5 || index == 6 ? 48u : 432u) && nw.fields.g &&
          !nw.fields.w);
    fprintf(f->output,
            "{\"event\":\"draw\",\"index\":%u,\"outcome\":1,\"nw_position\":%zu,\"nw_words\":[",
            index, nw.fields.e->position);
    mob_words(f->output, (const uint8_t *)nw.fields.g, nw.fields.e->position);
    uint32_t gl_error = UINT32_MAX;
    CHECK(mc_client_display_source_get_error(f->display, &gl_error, NULL) && gl_error == 0);
    fprintf(f->output, "],\"gl_error\":%u}\n", gl_error);
    if (index == 0 || index == 1 || index == 3 || index == 5)
        CHECK(mob_lists(f) == 0);
    CHECK(mob_queries(f, index) == 0);
    CHECK(mob_texture_calls(f, index, &state) == 0);
    /* Real retained ownership, never manually forged active flags. */
    mc_client_render_manager_draw_call *nested = NULL;
    mc_client_manager_draw_error *nested_error = calloc(1, sizeof(*nested_error));
    mc_client_manager_error *owner_error = calloc(1, sizeof(*owner_error));
    CHECK(nested_error && owner_error);
    CHECK(mc_client_render_manager_draw_begin(f->manager, f->actor, .375f, &services, NULL, &nested,
                                              nested_error) == MC_CLIENT_MANAGER_BUSY &&
          nested == NULL);
    mc_client_render_manager_classes *same = f->managers;
    CHECK(mc_client_render_manager_classes_retire(&same, owner_error) == MC_CLIENT_MANAGER_BUSY &&
          same == f->managers);
    if (index < 8) {
        CHECK(atomic_load(&f->app->render_borrows) > app_borrows);
        CHECK(mc_client_application_constructor_retire(&f->app, f->runtime.error) ==
                  MC_CLIENT_APP_BUSY &&
              f->app->constructor_phase == MC_CLIENT_APP_RAW &&
              f->runtime.services.application_classes->a == f->app);
    }
    free(owner_error);
    free(nested_error);
    CHECK(mc_client_render_manager_draw_retire(&f->call, f->draw_error) == MC_CLIENT_MANAGER_OK &&
          f->call == NULL);
    CHECK(atomic_load(&f->app->render_borrows) == app_borrows);
    fprintf(f->output,
            "{\"event\":\"control\",\"name\":\"retained_graph\",\"case\":%u,\"nested_busy\":true,"
            "\"class_retire_busy\":true,\"app_retire_busy\":%s,\"render_borrows_restored\":true}\n",
            index, index < 8 ? "true" : "false");
    return 0;
}

static int mob_controls(mob_fixture *f) {
    const mc_client_manager_draw_services services = {f->math, f->runtime.graphics, f->display,
                                                      f->runtime.services.application_classes};
    mc_client_manager_draw_limits limits = mc_client_render_manager_draw_default_limits();
    limits.max_bytes = 0;
    CHECK(mc_client_render_manager_draw_begin(f->manager, f->actor, .375f, &services, &limits,
                                              &f->call, f->draw_error) == MC_CLIENT_MANAGER_LIMIT &&
          f->call == NULL && f->draw_error->native_policy && !f->draw_error->source_exception);
    fputs("{\"event\":\"control\",\"name\":\"allocation_zero\",\"admission_only\":true}\n",
          f->output);
    CHECK(mc_client_render_manager_draw_begin(f->manager, NULL, .375f, &services, NULL, &f->call,
                                              f->draw_error) != MC_CLIENT_MANAGER_OK &&
          f->call != NULL && f->draw_error->source_exception && !f->draw_error->native_policy &&
          f->draw_error->exception_class != NULL);
    fprintf(f->output,
            "{\"event\":\"control\",\"name\":\"null_actor\",\"exception\":\"%s\",\"pc\":%d}\n",
            f->draw_error->exception_class, f->draw_error->site.pc);
    CHECK(mc_client_render_manager_draw_retire(&f->call, f->draw_error) == MC_CLIENT_MANAGER_OK &&
          f->call == NULL);
    mc_entity_classes *foreign_classes = NULL;
    mc_entity *foreign = NULL;
    const mc_entity_class_literal *literal = NULL;
    mc_mob_error *mob_error = calloc(1, sizeof(*mob_error));
    CHECK(mob_error != NULL &&
          mc_entity_classes_create(MC_ENTITY_CLIENT, f->runtime.services.profile, NULL,
                                   &foreign_classes) == MC_ENTITY_OK &&
          mc_entity_class_literal_get(foreign_classes, MC_ENTITY_CLASS_KIND_COW, &literal) ==
              MC_ENTITY_OK &&
          mc_mob_allocate(foreign_classes, NULL, literal, &foreign, mob_error) == MC_MOB_OK &&
          mc_mob_construct(foreign, NULL, mob_error) == MC_MOB_OK);
    CHECK(mc_client_render_manager_draw_begin(f->manager, foreign, .375f, &services, NULL, &f->call,
                                              f->draw_error) != MC_CLIENT_MANAGER_OK &&
          f->call != NULL && f->draw_error->native_policy && !f->draw_error->source_exception);
    CHECK(mc_client_render_manager_draw_retire(&f->call, f->draw_error) == MC_CLIENT_MANAGER_OK &&
          mc_mob_retire(&foreign, mob_error) == MC_MOB_OK &&
          mc_entity_classes_free(foreign_classes) == MC_ENTITY_OK);
    free(mob_error);
    fputs("{\"event\":\"control\",\"name\":\"foreign_actor\",\"native_admission\":true}\n",
          f->output);
    /* Last drawing control: native shadow ceiling after the real primary/fur.
     * Its unfinished NW/depth/blend prefix survives diagnosis and retirement. */
    limits = mc_client_render_manager_draw_default_limits();
    limits.max_shadow_quads = 0;
    CHECK(mob_frame(f) == 0);
    CHECK(mc_client_render_manager_draw_begin(f->manager, f->actor, .375f, &services, &limits,
                                              &f->call, f->draw_error) == MC_CLIENT_MANAGER_LIMIT &&
          f->call != NULL && f->draw_error->native_policy && !f->draw_error->source_exception);
    CHECK(f->draw_error->living != NULL &&
          f->draw_error->living->status == MC_CLIENT_LIVING_DRAW_LIMIT &&
          f->draw_error->living->native_policy && !f->draw_error->living->source_exception &&
          strcmp(f->draw_error->operation, "Shadow reached native quad ceiling") == 0 &&
          strcmp(f->draw_error->living->operation, f->draw_error->operation) == 0 &&
          strcmp(f->draw_error->site.class_name, "bw") == 0 &&
          strcmp(f->draw_error->site.method, "a") == 0 &&
          strcmp(f->draw_error->site.descriptor, "(Luu;DDDIIIFFDDD)V") == 0 &&
          f->draw_error->site.pc == 241);
    mc_client_manager_draw_state state;
    CHECK(mc_client_render_manager_draw_inspect(f->call, &state) && !state.returned &&
          state.primary_returned && state.post_draw && state.post_draw->shadow_started &&
          !state.post_draw->shadow_returned && state.post_draw->shadow_quads == 0 &&
          state.post_draw->shadow_vertices == 0);
    mc_client_nw_state before, after;
    CHECK(mc_client_nw_inspect(f->runtime.graphics, &before) && before.fields.w);
    CHECK(mc_client_render_manager_draw_retire(&f->call, f->draw_error) == MC_CLIENT_MANAGER_OK &&
          mc_client_nw_inspect(f->runtime.graphics, &after) && before.fields.w == after.fields.w &&
          before.fields.h == after.fields.h && before.fields.o == after.fields.o);
    fputs("{\"event\":\"control\",\"name\":\"shadow_limit\",\"primary_returned\":true,\"native_"
          "policy\":true,\"unfinished_NW_preserved\":true,\"manager_status\":\"LIMIT\",\"living_"
          "status\":\"LIMIT\",\"quads\":0,\"vertices\":0,\"site\":{\"class\":\"bw\",\"method\":"
          "\"a\",\"descriptor\":\"(Luu;DDDIIIFFDDD)V\",\"pc\":241},\"operation\":\"Shadow reached "
          "native quad ceiling\"}\n",
          f->output);
    return 0;
}

static int mob_close(mob_fixture *f) {
    CHECK(f->call == NULL);
    CHECK(mc_client_render_manager_classes_retire(&f->managers, NULL) == MC_CLIENT_MANAGER_OK);
    f->app->active_world = NULL;
    for (size_t i = 0; i < 4; ++i) {
        mc_entity_release(f->actors[i]);
        f->actors[i] = NULL;
    }
    mc_entity_release(f->camera);
    f->actor = f->camera = NULL;
    CHECK(mc_world_runtime_retire_entity_containers(f->world, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_chunk_pending_clear(f->provider.pending) == MC_CHUNK_ACQUIRE_OK &&
          mc_chunk_cache_init(f->provider.cache, f->provider.cache->entries,
                              f->provider.cache->capacity, f->provider.cache->insertions,
                              f->provider.cache->insertion_capacity));
    mc_world_provider_unbind(f->dimension);
    CHECK(mc_world_runtime_retire_world(f->world, NULL) == MC_WORLD_STORAGE_OK);
    for (size_t i = 0; i < 25; ++i)
        CHECK(mc_world_store_retire_chunk(f->store, f->chunks[i], NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_free(f->world, NULL) == MC_WORLD_STORAGE_OK);
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
    CHECK(mc_game_settings_retire(&f->app->settings, NULL) == MC_GAME_SETTINGS_OK);
    CHECK(mc_resource_files_free(f->textures) == MC_REGISTRY_OK);
    CHECK(
        mc_client_application_constructor_retire(&f->app, f->runtime.error) == MC_CLIENT_APP_BUSY &&
        f->app->constructor_phase == MC_CLIENT_APP_RETIRING && !f->app->source_namespace_borrowed);
    CHECK(mc_client_application_classes_release(&f->runtime.services.application_classes, NULL) ==
          MC_CLIENT_APPLICATION_OWNER_OK);
    CHECK(mc_client_application_constructor_retire(&f->app, f->runtime.error) == MC_CLIENT_APP_OK &&
          f->app == NULL);
    /* World's genuine Registry->Entity-namespace binding outlives World.
     * Retire that owner before the shared startup helper frees EntityClasses. */
    CHECK(mc_registry_classes_free(f->runtime.services.registry, NULL) == MC_REGISTRY_OK);
    f->runtime.services.registry = NULL;
    free(f->draw_error);
    free(f->lookup_error);
    CHECK(fixture_close(&f->runtime) == 0);
    return 0;
}

typedef struct animal_component_observation {
    float projection[16], modelview[16];
    int32_t matrix_mode, vector_cursor;
    size_t modelview_depth, projection_depth, app_borrows;
    bool nw_open;
    int32_t nw_vertices, nw_raw_index, nw_added_vertices;
    uint64_t actor_random, camera_random, world_random, shared_random;
    const manager_model *models[2];
    mc_client_biped_model model_fields[2];
    const mc_client_model_part *parts[18];
    mc_client_model_part part_fields[18];
    size_t model_count, part_count;
} animal_component_observation;

static int animal_component_observe(mob_fixture *f, animal_component_observation *out) {
    CHECK(mc_client_display_camera_matrices_inspect(f->display, out->projection, out->modelview));
    CHECK(mc_client_display_camera_stack_inspect(f->display, &out->matrix_mode,
                                                 &out->modelview_depth, &out->projection_depth));
    mc_client_nw_state nw;
    CHECK(mc_client_nw_inspect(f->runtime.graphics, &nw));
    out->nw_open = nw.fields.w;
    out->nw_vertices = nw.fields.h;
    out->nw_raw_index = nw.fields.o;
    out->nw_added_vertices = nw.fields.p;
    out->vector_cursor = f->runtime.services.entities->vector_cursor;
    out->app_borrows = atomic_load(&f->app->render_borrows);
    out->actor_random = f->actor->random->state;
    out->camera_random = f->camera->random->state;
    out->world_random = f->world->fields.r->state;
    out->shared_random = f->runtime.services.profile->math->state;
    out->model_count = out->part_count = 0;
    const manager_model *models[] = {f->renderer->primary, f->renderer->secondary};
    for (size_t i = 0; i < 2; ++i) {
        if (models[i] == NULL)
            continue;
        out->models[out->model_count] = models[i];
        memcpy(&out->model_fields[out->model_count], &models[i]->fields, sizeof(models[i]->fields));
        ++out->model_count;
        const mc_client_model_part *parts[9];
        const size_t count = mob_parts(models[i], parts);
        for (size_t p = 0; p < count; ++p) {
            if (parts[p] == NULL)
                continue;
            CHECK(out->part_count < MOB_COUNT(out->parts));
            out->parts[out->part_count] = parts[p];
            memcpy(&out->part_fields[out->part_count], parts[p], sizeof(*parts[p]));
            ++out->part_count;
        }
    }
    return 0;
}

static int animal_component_preserved(mob_fixture *f, const animal_component_observation *before) {
    animal_component_observation after;
    CHECK(animal_component_observe(f, &after) == 0);
    CHECK(memcmp(before->projection, after.projection, sizeof(after.projection)) == 0 &&
          memcmp(before->modelview, after.modelview, sizeof(after.modelview)) == 0 &&
          before->matrix_mode == after.matrix_mode &&
          before->modelview_depth == after.modelview_depth &&
          before->projection_depth == after.projection_depth && before->nw_open == after.nw_open &&
          before->nw_vertices == after.nw_vertices && before->nw_raw_index == after.nw_raw_index &&
          before->nw_added_vertices == after.nw_added_vertices &&
          before->vector_cursor == after.vector_cursor &&
          before->actor_random == after.actor_random &&
          before->camera_random == after.camera_random &&
          before->world_random == after.world_random &&
          before->shared_random == after.shared_random);
    CHECK(before->model_count == after.model_count && before->part_count == after.part_count);
    for (size_t i = 0; i < before->model_count; ++i)
        CHECK(before->models[i] == after.models[i] &&
              memcmp(&before->model_fields[i], &after.model_fields[i],
                     sizeof(before->model_fields[i])) == 0);
    for (size_t i = 0; i < before->part_count; ++i)
        CHECK(before->parts[i] == after.parts[i] &&
              memcmp(&before->part_fields[i], &after.part_fields[i],
                     sizeof(before->part_fields[i])) == 0);
    return 0;
}

static int animal_component_rng_preserved(mob_fixture *f,
                                          const animal_component_observation *before) {
    CHECK(before->actor_random == f->actor->random->state &&
          before->camera_random == f->camera->random->state &&
          before->world_random == f->world->fields.r->state &&
          before->shared_random == f->runtime.services.profile->math->state);
    return 0;
}

/* The actual allocator creates/publishes the native retained call. This does
 * not fabricate a scope or pretend TH's interpolation/brightness was entered.
 * The existing entered body receives actual separately held actor/renderer.
 */
static int animal_component_scope(mob_fixture *f, mc_entity *actor,
                                  mc_client_managed_renderer *renderer, mc_math_classes *math) {
    CHECK(f->call == NULL);
    const mc_client_manager_draw_services services = {math, f->runtime.graphics, f->display,
                                                      f->runtime.services.application_classes};
    CHECK(manager_draw_allocate(f->manager, &services, NULL, &f->call, f->draw_error) ==
              MC_CLIENT_MANAGER_OK &&
          f->call != NULL && f->manager->draw_call == f->call && f->call->scope.owner == f->call);
    CHECK(manager_draw_capture_actor(f->call, actor, &f->call->owner_work) &&
          manager_draw_capture_renderer(f->call, renderer, &f->call->owner_work) &&
          f->call->actor_hold == actor && f->call->renderer_hold == renderer);
    return 0;
}

int mc_private_animal_component_ko(mob_fixture *f) {
    static const unsigned int cases[] = {1, 3, 5};
    static const char *const classes[] = {"eh", "hh", "td"};
    CHECK(f->call == NULL);
    mc_client_model_runtime_error *error = calloc(1, sizeof(*error));
    CHECK(error != NULL);
    for (size_t i = 0; i < MOB_COUNT(cases); ++i) {
        CHECK(mob_select(f, cases[i]) == 0);
        mc_client_render_manager *manager = f->manager;
        mc_client_managed_renderer *renderer = f->renderer;
        CHECK(mc_client_render_manager_retain(manager, NULL) == MC_CLIENT_MANAGER_OK &&
              mc_client_managed_renderer_retain(renderer, NULL) == MC_CLIENT_MANAGER_OK);
        manager_model *model = renderer->primary;
        uint8_t before[sizeof(*model)];
        memcpy(before, model, sizeof(before));
        const mc_client_model_part *parts[9] = {0};
        uint8_t part_bytes[9][sizeof(mc_client_model_part)];
        const size_t part_count = mob_parts(model, parts);
        CHECK(part_count > 0 && part_count <= MOB_COUNT(parts));
        for (size_t part = 0; part < part_count; ++part) {
            CHECK(parts[part] != NULL);
            memcpy(part_bytes[part], parts[part], sizeof(part_bytes[part]));
        }
        animal_component_observation observation;
        CHECK(animal_component_observe(f, &observation) == 0);
        CHECK(mc_client_model_event_entered(
            renderer, model, NULL, .25f, -.5f, .375f, NULL,
            (mc_client_graphics_site){"AnimalComponentFixture", "event", "(Lls;FFF)V", -1}, error));
        CHECK(error->status == MC_CLIENT_MODEL_RUNTIME_OK && error->state.returned &&
              error->state.event_returned && !error->state.pose_returned &&
              error->state.model == model && strcmp(error->state.runtime_class, classes[i]) == 0 &&
              strcmp(error->site.class_name, "ko") == 0 && strcmp(error->site.method, "a") == 0 &&
              strcmp(error->site.descriptor, "(Lls;FFF)V") == 0 && error->site.pc == 0 &&
              error->state.actor_identity == 0 && error->state.argument_count == 3 &&
              error->state.math_calls == 0 && error->state.pose_stores == 0 &&
              error->state.part_calls == 0 && error->state.part_returns == 0 &&
              error->state.compiled_parts == 0 && error->state.faces == 0 &&
              error->state.vertices == 0 && error->ancestor_count == 0 &&
              error->selected_child == MC_CLIENT_MODEL_RUNTIME_CHILD_NONE &&
              memcmp(before, model, sizeof(before)) == 0);
        for (size_t part = 0; part < part_count; ++part)
            CHECK(memcmp(part_bytes[part], parts[part], sizeof(part_bytes[part])) == 0);
        CHECK(animal_component_preserved(f, &observation) == 0 &&
              atomic_load(&f->app->render_borrows) == observation.app_borrows);
        CHECK(mc_client_managed_renderer_release(&renderer, NULL) == MC_CLIENT_MANAGER_OK &&
              renderer == NULL &&
              mc_client_render_manager_release(&manager, NULL) == MC_CLIENT_MANAGER_OK &&
              manager == NULL);
        fprintf(f->output,
                "{\"event\":\"control\",\"name\":\"component_ko_empty\",\"model\":\"%s\","
                "\"actor_null\":true,\"services_null\":true,\"source_pc\":0,"
                "\"returned\":true,\"model_and_parts_unchanged\":true,"
                "\"no_math_parts_gl_pool_rng\":true}\n",
                classes[i]);
    }
    free(error);
    return 0;
}

int mc_private_animal_component_hh_null(mob_fixture *f) {
    CHECK(mob_select(f, 3) == 0 && f->call == NULL && mob_frame(f) == 0);
    manager_model *model = f->renderer->primary;
    mc_client_model_part *removed_horn = model->fields.b;
    CHECK(removed_horn != NULL && !removed_horn->n && removed_horn->o == 0 &&
          model->fields.d != NULL && model->fields.e != NULL);
    animal_component_observation entered;
    CHECK(animal_component_observe(f, &entered) == 0);
    model->fields.b = NULL; /* Declared source input, retained through final teardown. */
    const mc_client_manager_draw_services services = {f->math, f->runtime.graphics, f->display,
                                                      f->runtime.services.application_classes};
    CHECK(mc_client_render_manager_draw_begin(f->manager, f->actor, .375f, &services, NULL,
                                              &f->call, f->draw_error) ==
              MC_CLIENT_MANAGER_UNIMPLEMENTED_DEPENDENCY &&
          f->call != NULL && f->draw_error->native_policy && !f->draw_error->source_exception &&
          f->draw_error->living != NULL);
    const mc_client_living_draw_error *living = f->draw_error->living;
    CHECK(living->caught.active && living->state.primary_event_returned &&
          !living->state.primary_render_returned && !living->state.returned &&
          living->caught.selected_child == MC_CLIENT_LIVING_DRAW_CHILD_MODEL &&
          living->caught.exception_class != NULL &&
          strcmp(living->caught.exception_class, "java/lang/NullPointerException") == 0 &&
          strcmp(living->site.class_name, "gv") == 0 && living->site.pc == 747 &&
          strcmp(living->child.class_name, "java/lang/Exception") == 0 &&
          strcmp(living->child.method, "printStackTrace") == 0);
    const mc_client_model_runtime_error *error = &living->caught.detail.model;
    CHECK(error->status == MC_CLIENT_MODEL_RUNTIME_SOURCE_EXCEPTION && error->source_exception &&
          !error->native_policy && error->exception_class != NULL &&
          strcmp(error->exception_class, "java/lang/NullPointerException") == 0 &&
          strcmp(error->caller.class_name, "gv") == 0 && error->caller.pc == 297 &&
          strcmp(error->site.class_name, "hh") == 0 && strcmp(error->site.method, "b") == 0 &&
          error->site.pc == 24 && error->state.nn_pose_returned &&
          error->state.nn_pose_site.pc == 115 && !error->state.nn_render_returned &&
          error->state.nn_render_site.pc == 10 && !error->state.pose_returned &&
          !error->state.returned && error->state.pose_stores == 7 && error->state.math_calls == 4 &&
          error->state.part_calls == 0 && error->state.part_returns == 0 &&
          error->state.compiled_parts == 0 && error->state.faces == 0 &&
          error->state.vertices == 0 && !error->part.pending_list && error->ancestor_count == 2 &&
          strcmp(error->ancestors[0].class_name, "hh") == 0 &&
          strcmp(error->ancestors[0].method, "a") == 0 && error->ancestors[0].pc == 10 &&
          strcmp(error->ancestors[1].class_name, "nn") == 0 &&
          strcmp(error->ancestors[1].method, "a") == 0 && error->ancestors[1].pc == 10 &&
          error->parent.pc == 10 && strcmp(error->parent.class_name, "nn") == 0);
    CHECK(mob_fbits(error->state.arguments[3]) == mob_fbits(3.75f) &&
          mob_fbits(error->state.arguments[4]) == mob_fbits(2.5f) &&
          mob_fbits(model->fields.d->d) == mob_fbits(2.5f / 57.295776f) &&
          mob_fbits(model->fields.d->e) == mob_fbits(3.75f / 57.295776f) &&
          mob_fbits(model->fields.e->d) == UINT32_C(0x3fc90fdb));
    animal_component_observation prefix;
    CHECK(model->fields.b == NULL && !removed_horn->n && removed_horn->o == 0 &&
          animal_component_observe(f, &prefix) == 0 &&
          prefix.vector_cursor == entered.vector_cursor &&
          animal_component_rng_preserved(f, &entered) == 0);
    CHECK(mc_client_render_manager_draw_retire(&f->call, f->draw_error) == MC_CLIENT_MANAGER_OK &&
          f->call == NULL && model->fields.b == NULL && !removed_horn->n && removed_horn->o == 0 &&
          animal_component_preserved(f, &prefix) == 0);
    /* Deliberately no model->fields.b restoration and no matrix/list repair. */
    fputs("{\"event\":\"control\",\"name\":\"component_hh_null_horn\","
          "\"source_site\":\"hh.b(FFFFFF)V:24\",\"nn_pose_return\":115,"
          "\"selected_pose_returned\":false,\"nn_render_returned\":false,"
          "\"selected_render_returned\":false,\"stores\":7,\"math_calls\":4,"
          "\"part_calls\":0,\"ancestors\":[\"hh.a:10\",\"nn.a:10\"],"
          "\"caught_source_exception\":\"java/lang/NullPointerException\","
          "\"native_frontier\":\"gv.a:747\",\"null_survives_retirement\":true,"
          "\"prefix_survives_retirement\":true}\n",
          f->output);
    return 0;
}

int mc_private_animal_component_wrong_bridge(mob_fixture *f, unsigned int selected) {
    CHECK((selected == 3 || selected == 5) && mob_select(f, selected) == 0 && f->call == NULL &&
          mob_frame(f) == 0);
    mc_client_managed_renderer *renderer = f->renderer;
    mc_entity *wrong = f->actors[0]; /* Genuine canonical Sheep, not a foreign fake. */
    CHECK(wrong != NULL && mc_entity_is_kind(wrong, MC_ENTITY_CLASS_KIND_SHEEP));
    const uint64_t wrong_random = wrong->random->state;
    animal_component_observation before;
    CHECK(animal_component_observe(f, &before) == 0 &&
          animal_component_scope(f, wrong, renderer, f->math) == 0);
    CHECK(!mc_client_living_draw_entered(
        &f->call->scope, renderer, wrong, 0, 0, 0, 0, .375f,
        (mc_client_graphics_site){"AnimalComponentFixture", "living", "(Lsn;DDDFF)V", -1},
        &f->call->living));
    const mc_client_living_draw_error *error = &f->call->living;
    CHECK(error->status == MC_CLIENT_LIVING_DRAW_SOURCE_EXCEPTION && error->source_exception &&
          !error->native_policy && error->throwable == MC_CLIENT_LIVING_DRAW_EXCEPTION &&
          error->exception_class != NULL &&
          strcmp(error->exception_class, "java/lang/ClassCastException") == 0 &&
          strcmp(error->site.class_name, selected == 3 ? "va" : "ip") == 0 &&
          strcmp(error->site.method, "a") == 0 &&
          strcmp(error->site.descriptor, "(Lsn;DDDFF)V") == 0 && error->site.pc == 2 &&
          !error->caught.active && !error->state.returned && !error->state.primary_event_returned &&
          !error->state.primary_render_returned && error->state.texture_call_count == 0 &&
          !error->state.matrix_popped && f->call->math_hold == NULL && f->call->owner_count == 0);
    CHECK(animal_component_preserved(f, &before) == 0 && wrong->random->state == wrong_random);
    CHECK(mc_client_render_manager_draw_retire(&f->call, f->draw_error) == MC_CLIENT_MANAGER_OK &&
          f->call == NULL && animal_component_preserved(f, &before) == 0);
    fprintf(f->output,
            "{\"event\":\"control\",\"name\":\"component_wrong_bridge\","
            "\"renderer\":\"%s\",\"actual_actor\":\"dl\",\"source_pc\":2,"
            "\"exception\":\"java/lang/ClassCastException\",\"before_push\":true,"
            "\"actual_retained_scope\":true,\"checked_retirement\":true}\n",
            selected == 3 ? "va" : "ip");
    return 0;
}

int mc_private_animal_component_chicken_math(mob_fixture *f) {
    CHECK(mob_select(f, 5) == 0 && f->call == NULL && mob_frame(f) == 0);
    CHECK(mob_fbits(f->actor->mob->chicken.g) == mob_fbits(.25f) &&
          mob_fbits(f->actor->mob->chicken.b) == mob_fbits(.75f) &&
          mob_fbits(f->actor->mob->chicken.f) == mob_fbits(.5f) &&
          mob_fbits(f->actor->mob->chicken.c) == mob_fbits(1.25f));
    mc_math_classes *limited = NULL;
    mc_math_class_state before, failed, after_retirement, original;
    CHECK(mc_math_classes_create(4096, &limited, NULL) == MC_MATH_CLASS_OK &&
          mc_math_classes_state(limited, &before, NULL) == MC_MATH_CLASS_OK &&
          before.phase == MC_MATH_CLASS_COLD && before.holds == 1 && before.source_a == NULL &&
          limited != f->math &&
          mc_math_classes_state(f->math, &original, NULL) == MC_MATH_CLASS_OK &&
          original.phase == MC_MATH_CLASS_READY);
    const mc_math_trig *world_table = mc_math_classes_table(f->math);
    animal_component_observation entered;
    CHECK(animal_component_observe(f, &entered) == 0);
    CHECK(animal_component_scope(f, f->actor, f->renderer, limited) == 0);
    CHECK(!mc_client_living_draw_entered(
        &f->call->scope, f->renderer, f->actor, 0, 0, 0, 0, .375f,
        (mc_client_graphics_site){"AnimalComponentFixture", "living", "(Lsn;DDDFF)V", -1},
        &f->call->living));
    const mc_client_living_draw_error *error = &f->call->living;
    CHECK(error->status == MC_CLIENT_LIVING_DRAW_LIMIT && error->native_policy &&
          !error->source_exception && error->throwable == MC_CLIENT_LIVING_DRAW_NO_THROWABLE &&
          !error->caught.active && strcmp(error->site.class_name, "ip") == 0 &&
          strcmp(error->site.method, "a") == 0 && strcmp(error->site.descriptor, "(Lww;F)F") == 0 &&
          error->site.pc == 36 && strcmp(error->parent.class_name, "gv") == 0 &&
          error->parent.pc == 147 && error->selected_child == MC_CLIENT_LIVING_DRAW_CHILD_MATH &&
          strcmp(error->child.class_name, "in") == 0 &&
          strcmp(error->child.method, "<clinit>") == 0 && error->child.pc == 2 &&
          error->detail.math.status == MC_MATH_CLASS_LIMIT && error->detail.math.source_pc == 2 &&
          error->state.flap_arguments_read && !error->state.flap_math_returned &&
          mob_fbits(error->state.flap_first) == UINT32_C(0x3ee00000) &&
          mob_fbits(error->state.flap_second) == UINT32_C(0x3f480000) &&
          !error->state.primary_event_returned && !error->state.primary_render_returned &&
          error->state.texture_call_count == 0 && f->call->math_hold == limited &&
          mc_math_classes_state(limited, &failed, NULL) == MC_MATH_CLASS_OK &&
          failed.phase == MC_MATH_CLASS_FAILED && failed.holds == 2 && failed.source_a == NULL);
    animal_component_observation prefix;
    CHECK(animal_component_observe(f, &prefix) == 0 &&
          animal_component_rng_preserved(f, &entered) == 0);
    CHECK(mc_client_render_manager_draw_retire(&f->call, f->draw_error) == MC_CLIENT_MANAGER_OK &&
          f->call == NULL && animal_component_preserved(f, &prefix) == 0 &&
          mc_math_classes_state(limited, &after_retirement, NULL) == MC_MATH_CLASS_OK &&
          after_retirement.phase == MC_MATH_CLASS_FAILED && after_retirement.holds == 1 &&
          after_retirement.source_a == failed.source_a &&
          mc_math_classes_table(f->math) == world_table);
    mc_math_class_state original_after;
    CHECK(mc_math_classes_state(f->math, &original_after, NULL) == MC_MATH_CLASS_OK &&
          original_after.phase == original.phase && original_after.holds == original.holds &&
          original_after.source_a == original.source_a &&
          mc_math_classes_release(&limited, NULL) == MC_MATH_CLASS_OK && limited == NULL);
    fputs("{\"event\":\"control\",\"name\":\"component_chicken_math_limit\","
          "\"declared_native_service_bytes\":4096,\"capture_succeeded\":true,"
          "\"source_site\":\"ip.a(Lww;F)F:36\",\"child_site\":\"in.<clinit>:2\","
          "\"flap_first\":\"3ee00000\",\"flap_second\":\"3f480000\","
          "\"native_policy\":true,\"source_exception\":false,"
          "\"failed_phase_survives_retirement\":true,\"world_math_unchanged\":true,"
          "\"prefix_survives_retirement\":true}\n",
          f->output);
    return 0;
}

int main(int argc, char **argv) {
    CHECK(argc == 5 || argc == 6);
    SDL_SetMainReady();
    mob_fixture *f = calloc(1, sizeof(*f));
    CHECK(f != NULL);
    char path[4096];
    CHECK(snprintf(path, sizeof(path), "%s/native.ndjson", argv[4]) > 0);
    f->output = fopen(path, "wb");
    CHECK(f->output != NULL && mob_open(f, argv[1], argv[2], argv[3]) == 0);
    fprintf(f->output,
            "{\"event\":\"context\",\"RAW_app\":true,\"namespace_attached\":true,\"canonical_"
            "nonnull\":true,\"static_holds\":%zu,\"default_profile_rebound\":false}\n",
            atomic_load(&f->app->source_static_holds));
    const mc_client_display_info *device = mc_client_display_info_get(f->display);
    CHECK(device != NULL);
    fputs("{\"event\":\"device\",\"vendor\":", f->output);
    mob_string(f->output, device->vendor);
    fputs(",\"renderer\":", f->output);
    mob_string(f->output, device->renderer);
    fputs(",\"version\":", f->output);
    mob_string(f->output, device->version);
    fprintf(f->output, ",\"major\":%d,\"minor\":%d,\"native_gl_profile\":%d}\n", device->gl_major,
            device->gl_minor, device->profile);
    CHECK(mob_textures(f) == 0 && mob_prepare(f) == 0);
    mc_client_tessellator *tessellator = NULL;
    const mc_client_graphics_site site = {"AnimalDrawFixture", "prepare", "()V", -1};
    mc_client_graphics_error *graphics_error = calloc(1, sizeof(*graphics_error));
    CHECK(graphics_error != NULL &&
          mc_client_nw_active_use(f->runtime.graphics, site, &tessellator, graphics_error));
    free(graphics_error);
    mc_client_nw_state nw;
    CHECK(mc_client_nw_inspect(f->runtime.graphics, &nw) && nw.fields.g && nw.fields.e &&
          nw.fields.B > 0);
    memset((void *)nw.fields.g, 0, (size_t)nw.fields.B * sizeof(*nw.fields.g));
    ((mc_client_direct_int_buffer *)(void *)nw.fields.e)->position = 0;
    CHECK(mc_entity_vector_pool_reset(f->runtime.services.entities, NULL) == MC_ENTITY_OK);
    if (argc == 6) {
        CHECK(mob_frame(f) == 0);
        if (strcmp(argv[5], "component-ko") == 0)
            CHECK(mc_private_animal_component_ko(f) == 0);
        else if (strcmp(argv[5], "component-hh-null") == 0)
            CHECK(mc_private_animal_component_hh_null(f) == 0);
        else if (strcmp(argv[5], "component-va-cast") == 0)
            CHECK(mc_private_animal_component_wrong_bridge(f, 3) == 0);
        else if (strcmp(argv[5], "component-ip-cast") == 0)
            CHECK(mc_private_animal_component_wrong_bridge(f, 5) == 0);
        else if (strcmp(argv[5], "component-ip-math") == 0)
            CHECK(mc_private_animal_component_chicken_math(f) == 0);
        else
            CHECK(false);
    } else {
        for (unsigned int index = 0; index < 8; ++index) {
            CHECK(mob_select(f, index) == 0 && mob_profile(f, index, argv[4]) == 0);
        }
        CHECK(mob_controls(f) == 0);
    }
    CHECK(!ferror(f->output) && fclose(f->output) == 0);
    CHECK(mob_close(f) == 0);
    free(f);
    printf("Animal draw: %u checks; genuine RAW App/World/Animals/TH\n", checks);
    return 0;
}
