/* Genuine prepared RAW App and GAME/TH/Wolf/Zombie/Skeleton owners.
 * No original observations are execution inputs. The included startup helpers
 * supply native allocation/services only; no App constructor/READY is forged. */
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#endif
int mc_remaining_draw_startup_fixture_main(int argc, char **argv);
#define main mc_remaining_draw_startup_fixture_main
#include "../startup/test_application_startup.c"
#undef main

#include "client/display/camera_surface.h"
#include "client/display/fog_surface.h"
#include "client/display/world_frame_surface.h"
#include "client/render/living_draw_internal.h"
#include "client/render/manager/draw_internal.h"
#include "client/render/manager/internal.h"
#include "client/render_engine/color_array.h"
#include "client/render_engine/internal.h"
#include "client/render_engine/nw.h"
#include "client/render_engine/texture_lookup.h"
#include "client/texture_packs/internal.h"
#include "client/world/entity_renderer/held_stack_guard_internal.h"
#include "client/world/entity_renderer/held_stack_parent_internal.h"
#include "client/world/entity_renderer/internal.h"
#include "core/java/random_profile_internal.h"
#include "entity/mob/internal.h"
#include "item/registry/registry_internal.h"
#include "mc/client/render/manager/draw.h"
#include "mc/client/world/held_item_draw.h"
#include "mc/client/world/held_item_tick.h"
#include "mc/entity/vector/vector.h"
#include "mc/item/registry/runtime.h"
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
    client_application *app, *alternate_app;
    mc_client_texture_pack_list *alternate_packs;
    mc_client_display *display;
    mc_client_default_font *font;
    mc_client_texture_pack_list *packs;
    mc_resource_files *textures;
    mc_math_classes *math;
    mc_client_render_manager_classes *managers;
    mc_client_manager_consumer manager_consumer;
    mc_client_manager_error *manager_error;
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
    mc_client_entity_renderer_error *hand_error;
    mc_client_held_item *hand, *hand_extra;
    mc_client_held_item *initial_hand, *alternate_hand,
        *alternate_candidate;   /* hand borrowed; extra is an actual occurrence. */
    mc_item_stack *class_stack; /* borrowed from genuine Entity namespace FR.a. */
    const mc_item *bow_item;    /* actual canonical GM.i; never allocated by fixture. */
    mc_client_texture_lookup_error *lookup_error;
    mc_client_manager_draw_error *draw_error;
    mc_client_render_manager_draw_call *call;
    FILE *output;
    size_t observed_lists, observed_bow_batches;
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
    MOB_PS_ARRAY,
    MOB_VERTEX,
    MOB_QUAD,
    MOB_VECTOR,
    MOB_RA,
    MOB_IY,
    MOB_CV,
    MOB_STACK,
    MOB_ITEM
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
    mob_field_ref(out, &first, "own_model", renderer->own_model,
                  renderer->own_model ? renderer->own_model->source_class : "ko");
    mob_field_ref(out, &first, "actor", f->actor, f->kind);
    mob_field_ref(out, &first, "camera", f->camera, "bx");
    mob_field_ref(out, &first, "world", f->world, "fd");
    mob_field_ref(out, &first, "engine", f->app->render_engine, "ji");
    mob_field_ref(out, &first, "settings", f->app->settings, "kv");
    mob_field_ref(out, &first, "hand", mc_client_held_item_identity(f->hand), "ra");
    mob_field_ref(out, &first, "hand_app", f->hand->source->a, "kq");
    mob_field_ref(out, &first, "hand_engine", f->hand->source->a->render_engine, "ji");
    mob_field_ref(out, &first, "class_bow_stack", f->class_stack, "iz");
    mob_field_ref(out, &first, "class_bow_item", f->bow_item, "qz");
    mob_field_ref(out, &first, "initial_hand", mc_client_held_item_identity(f->initial_hand), "ra");
    mob_field_ref(out, &first, "alternate_hand", mc_client_held_item_identity(f->alternate_hand),
                  "ra");
    mob_field_ref(out, &first, "alternate_app", f->alternate_app, "kq");
    mob_field_ref(out, &first, "alternate_engine", f->alternate_app->render_engine, "ji");
    fputs("},\"objects\":[", out);
    mob_queue(graph, manager, "th", MOB_TH, 0);
    mob_queue(graph, renderer, renderer->source_class, MOB_XY, 0);
    mob_queue(graph, renderer->primary, renderer->primary->source_class, MOB_MODEL, 0);
    mob_queue(graph, renderer->secondary,
              renderer->secondary ? renderer->secondary->source_class : "ko", MOB_MODEL, 0);
    mob_queue(graph, renderer->own_model,
              renderer->own_model ? renderer->own_model->source_class : "ko", MOB_MODEL, 0);
    mob_queue(graph, mc_client_held_item_identity(f->hand), "ra", MOB_RA, 0);
    mob_queue(graph, mc_client_held_item_identity(f->initial_hand), "ra", MOB_RA, 0);
    mob_queue(graph, mc_client_held_item_identity(f->alternate_hand), "ra", MOB_RA, 0);
    mob_queue(graph, f->class_stack, "iz", MOB_STACK, 0);
    mob_queue(graph, f->bow_item, "qz", MOB_ITEM, 0);
    for (size_t index = 0; index < graph->count; ++index) {
        const mob_node node = graph->nodes[index];
        fprintf(out, "%s{\"class\":\"%s\",\"identity\":\"%016" PRIxPTR "\"", index ? "," : "",
                node.class_name, (uintptr_t)node.pointer);
        if (node.kind == MOB_IB_ARRAY || node.kind == MOB_TZ_ARRAY || node.kind == MOB_PS_ARRAY) {
            fprintf(out, ",\"length\":%zu,\"values\":[", node.length);
            for (size_t i = 0; i < node.length; ++i) {
                const void *value =
                    node.kind == MOB_IB_ARRAY
                        ? (const void *)((mc_client_model_vertex *const *)node.pointer)[i]
                    : node.kind == MOB_TZ_ARRAY
                        ? (const void *)((mc_client_model_quad *const *)node.pointer)[i]
                        : (const void *)((const manager_part_array *)node.pointer)->elements[i];
                const char *kind = node.kind == MOB_IB_ARRAY   ? "ib"
                                   : node.kind == MOB_TZ_ARRAY ? "tz"
                                                               : "ps";
                if (i)
                    fputc(',', out);
                mob_ref(out, value, kind);
                mob_queue(graph, value, kind,
                          node.kind == MOB_IB_ARRAY   ? MOB_VERTEX
                          : node.kind == MOB_TZ_ARRAY ? MOB_QUAD
                                                      : MOB_PART,
                          0);
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
                if (renderer->own_model)
                    mob_field_ref(out, &first, "v.a", renderer->own_model,
                                  renderer->own_model->source_class);
                mob_queue(graph, renderer->bw_model, "fh", MOB_FH, 0);
                mob_queue(graph, renderer->bw_blocks, "cv", MOB_CV, 0);
                break;
            case MOB_MODEL: {
                const manager_model *model = node.pointer;
                if (strcmp(model->source_class, "o") != 0) {
                    CHECK(strcmp(model->source_class, "ej") == 0 ||
                          strcmp(model->source_class, "lc") == 0);
                    mob_biped_fields(out, &first, graph, &model->fields, false);
                    break;
                }
                const mc_client_model_part *parts[] = {
                    model->fields.a, model->fields.b, model->fields.c, model->fields.d,
                    model->fields.e, model->fields.f, model->fields.g, model->fields.h,
                    model->fields.i, model->extra_j,  model->extra_k};
                for (size_t i = 0; i < 11; ++i) {
                    char name[12];
                    (void)snprintf(name, sizeof(name), "o.%c", (int)('a' + i));
                    mob_field_ref(out, &first, name, parts[i], "ps");
                    mob_queue(graph, parts[i], "ps", MOB_PART, 0);
                }
                mob_field_float(out, &first, "ko.m", model->fields.m);
                mob_field_bool(out, &first, "ko.n", model->fields.n);
                break;
            }
            case MOB_FH:
                mob_biped_fields(out, &first, graph, node.pointer, false);
                break;
            case MOB_RA: {
                const mc_client_source_ra *ra = node.pointer;
                mob_field_ref(out, &first, "ra.a", ra->a, "kq");
                mob_field_ref(out, &first, "ra.b", ra->b, "iz");
                mob_field_float(out, &first, "ra.c", ra->c);
                mob_field_float(out, &first, "ra.d", ra->d);
                mob_field_ref(out, &first, "ra.e", ra->e, "cv");
                mob_field_ref(out, &first, "ra.f", ra->f, "iy");
                mob_field_int(out, &first, "ra.g", ra->g);
                mob_queue(graph, ra->e, "cv", MOB_CV, 0);
                mob_queue(graph, ra->f, "iy", MOB_IY, 0);
                mob_queue(graph, ra->b, "iz", MOB_STACK, 0);
                break;
            }
            case MOB_IY: {
                const mc_client_source_iy *iy = node.pointer;
                mob_field_ref(out, &first, "iy.a", iy->a, "[I");
                mob_field_int(out, &first, "iy.b", iy->b);
                mob_field_ref(out, &first, "iy.c", iy->c, "kv");
                mob_field_ref(out, &first, "iy.d", iy->d, "sj");
                break;
            }
            case MOB_STACK: {
                mc_item_stack_fields stack;
                CHECK(mc_item_stack_fields_get(node.pointer, &stack) == MC_ITEM_STACK_OK);
                mob_field_int(out, &first, "iz.a", stack.count);
                mob_field_int(out, &first, "iz.b", stack.animation);
                mob_field_int(out, &first, "iz.c", stack.id);
                mob_field_int(out, &first, "iz.d", stack.damage);
                break;
            }
            case MOB_ITEM: {
                const mc_item_fields *item = mc_item_fields_get(node.pointer);
                CHECK(item);
                mob_field_int(out, &first, "gm.bf", item->bf);
                mob_field_int(out, &first, "gm.bg", item->bg);
                mob_field_int(out, &first, "gm.a", item->a);
                mob_field_int(out, &first, "gm.bh", item->bh);
                mob_field_bool(out, &first, "gm.bi", item->bi);
                mob_field_bool(out, &first, "gm.bj", item->bj);
                mob_field_ref(out, &first, "gm.bk", item->bk, "gm");
                if (!first)
                    fputc(',', out);
                first = false;
                fputs("\"gm.bl\":", out);
                mc_registry_string_view text;
                CHECK(mc_registry_string_read(item->bl, &text));
                fputc('"', out);
                for (size_t i = 0; i < text.length; ++i)
                    fprintf(out, "\\u%04x", text.units[i]);
                fputc('"', out);
                break;
            }
            case MOB_CV: {
                const mc_client_render_blocks *cv = node.pointer;
                mob_field_ref(out, &first, "cv.c", cv->c, "xp");
                mob_field_int(out, &first, "cv.d", cv->d);
                mob_field_bool(out, &first, "cv.e", cv->e);
                mob_field_bool(out, &first, "cv.f", cv->f);
                mob_field_bool(out, &first, "cv.b", cv->b);
                const int32_t ints[] = {cv->g, cv->h, cv->i, cv->j, cv->k, cv->l};
                for (size_t i = 0; i < 6; ++i) {
                    char name[12];
                    (void)snprintf(name, sizeof(name), "cv.%c", (int)('g' + i));
                    mob_field_int(out, &first, name, ints[i]);
                }
                mob_field_bool(out, &first, "cv.m", cv->m);
                for (size_t i = 0; i < 27; ++i) {
                    char name[12];
                    (void)snprintf(name, sizeof(name), "cv.%c",
                                   (int)(i < 13 ? 'n' + i : 'A' + i - 13));
                    mob_field_float(out, &first, name, cv->n_to_N[i]);
                }
                mob_field_int(out, &first, "cv.O", cv->O);
                for (size_t i = 0; i < 12; ++i) {
                    char name[12];
                    if (i < 11)
                        (void)snprintf(name, sizeof(name), "cv.%c", (int)('P' + i));
                    else
                        (void)snprintf(name, sizeof(name), "cv.aa");
                    mob_field_float(out, &first, name, cv->P_to_aa[i]);
                    (void)snprintf(name, sizeof(name), "cv.a%c", (int)('b' + i));
                    mob_field_bool(out, &first, name, cv->ab_to_am[i]);
                }
                break;
            }
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
    CHECK(graph->count > 0 && graph->count < MOB_COUNT(graph->nodes));
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
    static const unsigned int selected[] = {0, 0, 1, 1, 2, 2, 3, 0, 0, 0, 0, 2, 2, 0};
    static const char *const kinds[] = {"gi", "uz", "fr", "fr"};
    CHECK(index < MOB_COUNT(selected));
    f->actor = f->actors[selected[index]];
    f->kind = kinds[selected[index]];
    f->renderer = NULL;
    for (size_t i = 0; i < f->manager->map->capacity; ++i)
        for (manager_map_entry *e = f->manager->map->buckets[i]; e; e = e->next)
            if (e->key == f->actor->class_literal)
                f->renderer = e->owner;
    CHECK(f->renderer && f->renderer->primary &&
          strcmp(f->renderer->source_class, strcmp(f->kind, "gi") == 0 ? "we" : "v") == 0);
    CHECK(strcmp(f->renderer->primary->source_class, strcmp(f->kind, "gi") == 0   ? "o"
                                                     : strcmp(f->kind, "uz") == 0 ? "ej"
                                                                                  : "lc") == 0);
    return 0;
}

static int mob_prepare(mob_fixture *f) {
    const mc_entity_class_kind kinds[] = {MC_ENTITY_CLASS_KIND_WOLF, MC_ENTITY_CLASS_KIND_ZOMBIE,
                                          MC_ENTITY_CLASS_KIND_SKELETON,
                                          MC_ENTITY_CLASS_KIND_SKELETON};
    for (size_t i = 0; i < MOB_COUNT(kinds); ++i)
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
    CHECK(mob_pose(f->camera, 8.5, 65, 12.5, 180, 15) == 0);
    bool inserted = false;
    CHECK(mc_world_runtime_insert_entity(f->world, f->camera, &inserted, NULL) ==
              MC_WORLD_STORAGE_OK &&
          inserted);
    CHECK(mc_client_render_manager_classes_bind(f->runtime.services.render_classes,
                                                f->runtime.services.registry,
                                                f->runtime.services.profile, NULL, &f->managers,
                                                f->manager_error) == MC_CLIENT_MANAGER_OK);
    /* Real native-only admission association, before actual TH active use.
     * No direct consumers write or Source field/constructor-phase repair. */
    CHECK(mc_client_render_manager_consumer_attach(f->managers, &f->manager_consumer,
                                                   f->manager_error) == MC_CLIENT_MANAGER_OK &&
          f->manager_consumer.attached && f->manager_consumer.classes == f->managers);
    CHECK(mc_client_render_manager_static_get(f->managers, &f->manager, f->manager_error) ==
          MC_CLIENT_MANAGER_OK);
    /* Same genuine TH.a/newRA/constructor/publication fragment as declared Source setup.
     * No App READY/constructor completion, no RA.b injection, no synthetic Bow. */
    CHECK(f->manager->fields.f == NULL && f->app->constructor_phase == MC_CLIENT_APP_RAW);
    const mc_client_entity_renderer_services hand_services = {
        f->runtime.services.render_classes, f->runtime.graphics, f->runtime.services.profile,
        f->display};
    mc_client_held_item *candidate = NULL;
    CHECK(mc_client_held_item_allocate(&hand_services, NULL, &candidate, f->hand_error) ==
          MC_CLIENT_ENTITY_RENDERER_OK);
    CHECK(mc_client_held_item_construct(candidate, f->app, f->hand_error) ==
          MC_CLIENT_ENTITY_RENDERER_OK);
    const mc_client_source_ra *identity = mc_client_held_item_identity(candidate);
    const mc_client_manager_status publication = mc_client_render_manager_publish_hand(
        f->managers, f->manager, &candidate, f->manager_error);
    if (publication != MC_CLIENT_MANAGER_OK) {
        const mc_client_manager_error *e = f->manager_error;
        fprintf(stderr,
                "Remaining hand publication status%d source=%s.%s:%d th=%d renderer=%d frame=%d "
                "native=%d operation=%s hand_status=%d\n",
                (int)publication, e->source_class ? e->source_class : "?",
                e->source_method ? e->source_method : "?", e->pc, e->th_pc, e->renderer_pc,
                e->frame_pc, (int)e->native_policy, e->operation ? e->operation : "?",
                (int)e->hand.status);
    }
    CHECK(publication == MC_CLIENT_MANAGER_OK && candidate == NULL);
    f->hand = f->manager->hand; /* borrowed, publication owns the actual final occurrence */
    f->initial_hand = f->hand;
    mc_client_held_item_state hand;
    CHECK(f->hand && mc_client_held_item_inspect(f->hand, &hand) && hand.pending_ra == identity &&
          hand.ra_app == f->app && hand.ra_b == NULL && hand.ra_c == 0 && hand.ra_d == 0 &&
          hand.ra_g == -1 && hand.ra_last_store_pc == 60 && hand.map_zero_stores == 16384 &&
          hand.map_font == NULL && hand.map_engine == f->app->render_engine && hand.map_lease);
    mc_client_argb_image_state image;
    mc_client_color_array_state array;
    mc_client_texture_source_state texture;
    CHECK(mc_client_argb_image_inspect(hand.map_image, &image) && image.constructed &&
          image.width == 128 && image.height == 128 && image.pixel_count == 16384 &&
          mc_client_color_array_inspect(hand.map_array, &array) && array.length == 16384);
    for (size_t pixel = 0; pixel < 16384; ++pixel) {
        CHECK(image.pixels[pixel] == 0 && array.bits[pixel] == 0);
    }
    CHECK(
        mc_client_display_texture_state_inspect(f->display, (uint32_t)hand.map_texture, &texture) &&
        texture.known && texture.image_uploaded);
    typedef void(APIENTRY * level_fn)(GLenum, GLint, GLenum, GLint *);
    typedef void(APIENTRY * integer_fn)(GLenum, GLint *);
    level_fn level = (level_fn)SDL_GL_GetProcAddress("glGetTexLevelParameteriv");
    integer_fn integer = (integer_fn)SDL_GL_GetProcAddress("glGetIntegerv");
    GLint width = 0, height = 0, bound = 0;
    CHECK(level && integer);
    integer(GL_TEXTURE_BINDING_2D, &bound);
    level(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &width);
    level(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &height);
    CHECK(bound == hand.map_texture && width == 128 && height == 128);
    CHECK(f->runtime.services.entities->mobs != NULL);
    f->class_stack = f->runtime.services.entities->mobs->skeleton_a;
    mc_item_stack_fields fields;
    CHECK(f->class_stack && mc_item_stack_fields_get(f->class_stack, &fields) == MC_ITEM_STACK_OK &&
          fields.id == 261 && fields.count == 1 && fields.animation == 0 && fields.damage == 0);
    CHECK(mc_registry_item_static_get(f->runtime.services.registry, "i", &f->bow_item, NULL) ==
              MC_REGISTRY_OK &&
          f->bow_item && f->bow_item->kind == MC_ITEM_BOW &&
          mc_item_fields_get(f->bow_item)->bf == fields.id);
    fprintf(f->output,
            "{\"event\":\"hand_setup\",\"ordinal\":0,\"genuine_publication\":true,\"RAW_app\":true,"
            "\"IY_width\":128,\"IY_height\":128,\"IY_texture\":%d,\"class_stack\":",
            hand.map_texture);
    mob_ref(f->output, f->class_stack, "iz");
    fputs(",\"hand\":", f->output);
    mob_ref(f->output, identity, "ra");
    fputs("}\n", f->output);
    CHECK(mc_client_render_manager_bind_frame(f->manager, f->math, f->world, f->app->render_engine,
                                              NULL, f->camera, f->app->settings, .375f,
                                              NULL) == MC_CLIENT_MANAGER_OK);
    CHECK(mc_client_render_manager_origin_store(f->managers, 0, 8.5, NULL) ==
              MC_CLIENT_MANAGER_OK &&
          mc_client_render_manager_origin_store(f->managers, 1, 64, NULL) == MC_CLIENT_MANAGER_OK &&
          mc_client_render_manager_origin_store(f->managers, 2, 8.5, NULL) == MC_CLIENT_MANAGER_OK);
    CHECK(mob_select(f, 0) == 0 && f->manager->fields.f == mc_client_held_item_identity(f->hand) &&
          f->manager->fields.p == NULL);
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
    /* Source helper leaves D NULL; IK is independently owned and passed to JI. */
    CHECK(mc_client_default_font_create(32u * 1024u * 1024u, &f->font) == MC_CLIENT_FONT_OK &&
          mc_client_render_engine_allocate(f->runtime.graphics, f->font, NULL,
                                           &f->app->render_engine) == MC_CLIENT_RENDER_ENGINE_OK &&
          mc_client_render_engine_construct(f->app->render_engine, f->packs, f->app->settings,
                                            NULL) == MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(
        mc_client_display_create("Genuine remaining Mob draw", 256, 256, true, &f->display, NULL));
    f->app->display = f->display;
    f->lookup_error = calloc(1, sizeof(*f->lookup_error));
    f->draw_error = calloc(1, sizeof(*f->draw_error));
    f->hand_error = calloc(1, sizeof(*f->hand_error));
    f->manager_error = calloc(1, sizeof(*f->manager_error));
    CHECK(f->lookup_error && f->draw_error && f->hand_error && f->manager_error &&
          mob_world_open(f, directory) == 0);
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
        "/mob/wolf.png",     "/mob/wolf_tame.png", "/mob/wolf_angry.png",    "/mob/zombie.png",
        "/mob/skeleton.png", "/gui/items.png",     "%clamp%/misc/shadow.png"};
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
              state.mag_filter == GL_NEAREST && state.wrap_s == (i == 6 ? GL_CLAMP : GL_REPEAT) &&
              state.wrap_t == state.wrap_s && engine->fields.b->count == before + 1 &&
              !engine->fields.j && !engine->fields.k);
        fprintf(f->output,
                "{\"event\":\"texture\",\"index\":%u,\"key\":\"%s\",\"name\":%u,\"cache_before\":%"
                "zu,\"cache_after\":%zu,\"wrap_s\":%d,\"wrap_t\":%d,\"min\":%d,\"mag\":%d,"
                "\"transfer_hex\":\"",
                i, keys[i], f->texture_names[i], before, engine->fields.b->count, state.wrap_s,
                state.wrap_t, state.min_filter, state.mag_filter);
        CHECK(engine->fields.f != NULL && engine->fields.f->limit == (i == 6   ? 65536u
                                                                      : i == 5 ? 262144u
                                                                               : 8192u));
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

/* Caller-authored v2 setup. No constructor completion/static publication for
 * the second RAW App, no default entropy rebinding, and no App.i/D store. */
static int remaining_alternate_prepare(mob_fixture *f, const char *directory) {
    CHECK(mc_client_application_constructor_allocate(&f->runtime.services, NULL, &f->alternate_app,
                                                     f->runtime.error) == MC_CLIENT_APP_OK);
    client_application *a = f->alternate_app;
    CHECK(a && a->constructor_phase == MC_CLIENT_APP_RAW && a->source_namespace_borrowed &&
          atomic_load(&a->source_static_holds) == 0 &&
          f->runtime.services.application_classes->a == f->app);
    CHECK(mc_game_settings_create_defaults(NULL, &a->settings, NULL) == MC_GAME_SETTINGS_OK);
    a->active_world = f->world;
    a->display = f->display;
    char path[4096];
    CHECK(snprintf(path, sizeof(path), "%s/alternate-hand", directory) > 0);
    CHECK(mc_client_texture_pack_list_allocate(mc_resource_files_source(f->textures), NULL,
                                               &f->alternate_packs) == MC_TEXTURE_PACK_OK &&
          mc_client_texture_pack_list_construct(f->alternate_packs, a, path, NULL) ==
              MC_TEXTURE_PACK_OK);
    CHECK(mc_client_render_engine_allocate(f->runtime.graphics, f->font, NULL, &a->render_engine) ==
              MC_CLIENT_RENDER_ENGINE_OK &&
          mc_client_render_engine_construct(a->render_engine, f->alternate_packs, a->settings,
                                            NULL) == MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(a->D == NULL && a->font_renderer == NULL && a->render_engine != f->app->render_engine &&
          a->render_engine->fields.b->count == 0);
    const mc_client_entity_renderer_services services = {f->runtime.services.render_classes,
                                                         f->runtime.graphics,
                                                         f->runtime.services.profile, f->display};
    CHECK(mc_client_held_item_allocate(&services, NULL, &f->alternate_candidate, f->hand_error) ==
              MC_CLIENT_ENTITY_RENDERER_OK &&
          mc_client_held_item_construct(f->alternate_candidate, a, f->hand_error) ==
              MC_CLIENT_ENTITY_RENDERER_OK);
    f->alternate_hand = f->alternate_candidate;
    mc_client_held_item_state s;
    CHECK(mc_client_held_item_inspect(f->alternate_hand, &s) && s.ra_app == a && s.ra_b == NULL &&
          s.ra_c == 0 && s.ra_d == 0 && s.ra_g == -1 && s.map_engine == a->render_engine &&
          s.map_font == NULL && s.map_zero_stores == 16384 && s.map_lease);
    mc_client_argb_image_state image;
    mc_client_color_array_state array;
    mc_client_texture_source_state texture;
    CHECK(mc_client_argb_image_inspect(s.map_image, &image) && image.constructed &&
          image.width == 128 && image.height == 128 && image.pixel_count == 16384 &&
          mc_client_color_array_inspect(s.map_array, &array) && array.length == 16384);
    for (size_t i = 0; i < 16384; ++i)
        CHECK(image.pixels[i] == 0 && array.bits[i] == 0);
    CHECK(mc_client_display_texture_state_inspect(f->display, (uint32_t)s.map_texture, &texture) &&
          texture.known && texture.image_uploaded);
    fprintf(f->output,
            "{\"event\":\"hand_setup\",\"ordinal\":1,\"genuine_publication\":false,\"RAW_app\":"
            "true,\"IY_width\":128,\"IY_height\":128,\"IY_texture\":%d,\"class_stack\":",
            s.map_texture);
    mob_ref(f->output, f->class_stack, "iz");
    fputs(",\"hand\":", f->output);
    mob_ref(f->output, mc_client_held_item_identity(f->alternate_hand), "ra");
    fputs(",\"application\":", f->output);
    mob_ref(f->output, a, "kq");
    fputs(",\"engine\":", f->output);
    mob_ref(f->output, a->render_engine, "ji");
    fputs("}\n", f->output);
    return 0;
}

static mc_entity_watch_record *remaining_watch(mc_entity *actor, int32_t index) {
    if (!actor || !actor->watcher || !actor->watcher->map)
        return NULL;
    for (mc_entity_watch_node *n = actor->watcher->map->head; n; n = n->next)
        if (n->key == index)
            return n->record;
    return NULL;
}

/* Actual typed mutable records are prepared as caller operands. These stores
 * are not evidence that the GI/UD setter methods were entered natively. */
static int remaining_inputs(mob_fixture *f, unsigned int index) {
    if ((index >= 7 && index <= 10) || index == 13) {
        CHECK(strcmp(f->kind, "gi") == 0);
        mc_mob_wolf_fields *w = &f->actor->mob->wolf;
        mc_entity_watch_record *flags = remaining_watch(f->actor, 16),
                               *health = remaining_watch(f->actor, 18);
        CHECK(flags && flags->type == 0 && health && health->type == 2);
        const int8_t old_flags = flags->byte_value;
        const int32_t old_health = health->integer_value;
        flags->byte_value = (int8_t)((uint8_t)flags->byte_value & UINT8_C(248));
        w->b = w->c = w->h = w->i = 0;
        w->f = false;
        health->integer_value = 0;
        if (index == 7)
            flags->byte_value = (int8_t)((uint8_t)flags->byte_value | UINT8_C(1));
        if (index == 8)
            flags->byte_value = (int8_t)((uint8_t)flags->byte_value | UINT8_C(2));
        if (index == 9) {
            flags->byte_value = (int8_t)((uint8_t)flags->byte_value | UINT8_C(4));
            health->integer_value = 12;
        }
        if (index == 10) {
            w->b = .3f;
            w->c = -.2f;
            w->h = .6f;
            w->i = .1f;
            w->f = true;
        }
        if (old_flags != flags->byte_value) {
            flags->dirty = true;
            f->actor->watcher->dirty = true;
        }
        if (old_health != health->integer_value) {
            health->dirty = true;
            f->actor->watcher->dirty = true;
        }
    }
    if (index == 11 || index == 12) {
        const mc_registry_item_array *items = NULL;
        CHECK(mc_registry_item_c_get(f->runtime.services.registry, &items, NULL) ==
                  MC_REGISTRY_OK &&
              items && items->length > 261 && items->elements[261] == f->bow_item);
        items->elements[261]->fields.bi = index == 11;
    }
    if (index == 12) {
        CHECK(f->alternate_app->render_engine->fields.b->count == 0 &&
              f->alternate_candidate == f->alternate_hand && f->manager->hand == f->initial_hand &&
              mc_client_render_manager_publish_hand(f->managers, f->manager,
                                                    &f->alternate_candidate,
                                                    f->manager_error) == MC_CLIENT_MANAGER_OK &&
              f->alternate_candidate == NULL);
        f->hand = f->manager->hand;
        CHECK(f->hand == f->alternate_hand && f->manager->displaced_count == 1);
    }
    if (index == 13) {
        mc_client_held_item *p = f->initial_hand;
        CHECK(mc_client_held_item_retain(p, f->hand_error) == MC_CLIENT_ENTITY_RENDERER_OK &&
              mc_client_render_manager_publish_hand(f->managers, f->manager, &p,
                                                    f->manager_error) == MC_CLIENT_MANAGER_OK &&
              p == NULL);
        f->hand = f->manager->hand;
        CHECK(f->hand == f->initial_hand && f->manager->displaced_count == 2 &&
              f->hand->holds == 2);
    }
    if (index >= 7) {
        fprintf(f->output,
                "{\"event\":\"caller_inputs\",\"case\":%u,\"scope\":\"declared_mutable_operands_"
                "not_draw_getter_receipts\",\"actor\":",
                index);
        mob_ref(f->output, f->actor, f->kind);
        fputs(",\"hand\":", f->output);
        mob_ref(f->output, mc_client_held_item_identity(f->hand), "ra");
        fprintf(f->output, ",\"item_full3d\":%s",
                mc_item_fields_get(f->bow_item)->bi ? "true" : "false");
        if (strcmp(f->kind, "gi") == 0) {
            const mc_mob_wolf_fields *w = &f->actor->mob->wolf;
            mc_entity_watch_record *flags = remaining_watch(f->actor, 16),
                                   *health = remaining_watch(f->actor, 18);
            CHECK(flags && health);
            fprintf(f->output, ",\"watcher16\":%d,\"watcher18\":%d,\"wet\":%s,\"wet_fields\":[",
                    (int)flags->byte_value, health->integer_value, w->f ? "true" : "false");
            const float values[] = {w->b, w->c, w->h, w->i};
            for (size_t i = 0; i < 4; ++i) {
                if (i)
                    fputc(',', f->output);
                mob_float(f->output, values[i]);
            }
            fputc(']', f->output);
        }
        fprintf(f->output, ",\"displaced_count_native\":%zu}\n", f->manager->displaced_count);
    }
    CHECK(f->manager->fields.f == mc_client_held_item_identity(f->hand) && f->app->D == NULL &&
          f->app->font_renderer == NULL && f->alternate_app->D == NULL &&
          f->alternate_app->font_renderer == NULL &&
          f->runtime.services.application_classes->a == f->app);
    return 0;
}

static int remaining_hooks(mob_fixture *f, unsigned int index,
                           const mc_client_living_draw_state *s) {
    const mc_client_living_draw_held_state *h = &s->held;
    CHECK(s->rotation_returned && s->prescale_returned && s->color_returned);
    CHECK(h->own_model_identity == 0 || h->own_model_identity == (uintptr_t)f->renderer->own_model);
    fprintf(f->output, "{\"event\":\"hooks\",\"case\":%u,\"age\":", index);
    mob_float(f->output, s->age);
    fprintf(
        f->output,
        ",\"rotation_returned\":true,\"prescale_returned\":true,\"color_returned\":true,\"color\":%"
        "d,\"held_entered\":%s,\"stack_returned\":%s,\"attachment_returned\":%s,\"full3d_"
        "attempted\":%s,\"full3d_returned\":%s,\"full3d\":%s,\"held_returned\":%s,\"stack\":",
        s->color, h->entered ? "true" : "false", h->stack_returned ? "true" : "false",
        h->attachment_returned ? "true" : "false", h->full3d_attempted ? "true" : "false",
        h->full3d_returned ? "true" : "false", h->full3d ? "true" : "false",
        h->returned ? "true" : "false");
    mob_ref(f->output, (const void *)h->stack_identity, "iz");
    fputs(",\"own_model\":", f->output);
    mob_ref(f->output, (const void *)h->own_model_identity,
            f->renderer->own_model ? f->renderer->own_model->source_class : "fh");
    fputs(",\"arm\":", f->output);
    mob_ref(f->output, (const void *)h->arm_identity, "ps");
    fputs(",\"item\":", f->output);
    mob_ref(f->output, (const void *)h->item_identity, "qz");
    fputs(",\"hand\":", f->output);
    mob_ref(f->output, (const void *)h->hand_identity, "ra");
    fputs("}\n", f->output);
    return 0;
}

static int remaining_alternate_texture(mob_fixture *f, unsigned int index) {
    mc_client_render_engine *engine = f->alternate_app->render_engine;
    const mc_held_stack_parent_error *e = f->call->held_workspace;
    uint16_t units[128];
    const mc_texture_pack_text key = mob_text("/gui/items.png", units);
    bool found = false;
    mc_client_texture name = 0;
    CHECK(index == 12 && e && e->state.engine == engine &&
          mc_client_render_engine_cached_texture(engine, key, &found, &name) && found &&
          name == e->state.texture && engine->fields.b->count == 1 && engine->fields.f &&
          engine->fields.f->limit == 262144);
    mc_client_texture_source_state t;
    CHECK(mc_client_display_texture_state_inspect(f->display, name, &t) && t.known &&
          t.image_uploaded && t.min_filter == GL_NEAREST && t.mag_filter == GL_NEAREST &&
          t.wrap_s == GL_REPEAT && t.wrap_t == GL_REPEAT);
    fprintf(f->output,
            "{\"event\":\"alternate_texture\",\"case\":%u,\"scope\":\"native_transfer_bytes_Source_"
            "upload_parameters_only\",\"engine\":",
            index);
    mob_ref(f->output, engine, "ji");
    fprintf(f->output,
            ",\"name\":%u,\"cache_after\":%zu,\"wrap_s\":%d,\"wrap_t\":%d,\"min\":%d,\"mag\":%d,"
            "\"transfer_hex\":\"",
            name, engine->fields.b->count, t.wrap_s, t.wrap_t, t.min_filter, t.mag_filter);
    for (size_t i = 0; i < engine->fields.f->limit; ++i)
        fprintf(f->output, "%02x", engine->fields.f->data[i]);
    fputs("\"}\n", f->output);
    return 0;
}

typedef GLboolean(APIENTRY *mob_enabled_fn)(GLenum);
typedef void(APIENTRY *mob_boolean_fn)(GLenum, GLboolean *);
typedef void(APIENTRY *mob_integer_fn)(GLenum, GLint *);
typedef void(APIENTRY *mob_read_fn)(GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void *);
typedef void(APIENTRY *mob_finish_fn)(void);

static int mob_emit_gl(mob_fixture *f) {
    mc_client_display *display = f->display;
    float color[4] = {0}, projection[16], modelview[16], normal[3] = {0};
    mc_client_source_draw_state draw;
    mc_client_source_fog_state fog;
    CHECK(mc_client_display_source_draw_inspect(display, &draw) &&
          mc_client_display_source_fog_inspect(display, &fog));
    if (draw.color_known) {
        CHECK(
            mc_client_display_source_current_attribute(display, GL_CURRENT_COLOR, color, 4, NULL));
    }
    CHECK(mc_client_display_camera_matrices_inspect(display, projection, modelview));
    if (fog.normal_known) {
        CHECK(mc_client_display_source_current_attribute(display, GL_CURRENT_NORMAL, normal, 3,
                                                         NULL));
    }
    int32_t mode;
    size_t depth, projection_depth;
    CHECK(mc_client_display_camera_stack_inspect(display, &mode, &depth, &projection_depth));

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
    fprintf(out, "{\"color_known\":%s,\"normal_known\":%s,\"color\":",
            draw.color_known ? "true" : "false", fog.normal_known ? "true" : "false");
    if (draw.color_known) {
        fputc('[', out);
        for (size_t i = 0; i < 4; ++i)
            fprintf(out, "%s\"%08" PRIx32 "\"", i ? "," : "", mob_fbits(color[i]));
        fputc(']', out);
    } else
        fputs("null", out);
    fputs(",\"normal\":", out);
    if (fog.normal_known) {
        fputc('[', out);
        for (size_t i = 0; i < 3; ++i)
            fprintf(out, "%s\"%08" PRIx32 "\"", i ? "," : "", mob_fbits(normal[i]));
        fputc(']', out);
    } else
        fputs("null", out);
    fputs(",\"modelview\":[", out);
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

static size_t mob_parts(const manager_model *model, const mc_client_managed_renderer *renderer,
                        const mc_client_model_part *parts[11]) {
    if (model == NULL)
        return 0;
    if (strcmp(model->source_class, "wn") == 0) {
        size_t capacity = 0;
        if (!model->array_b ||
            !manager_part_array_native_extent(renderer, model->array_b, &capacity) ||
            capacity != 8 || model->array_b->length != 8)
            abort();
        parts[0] = model->fields.a;
        for (size_t i = 0; i < capacity; ++i)
            parts[i + 1] = model->array_b->elements[i];
        return capacity + 1;
    }
    const mc_client_model_part *all[] = {model->fields.a, model->fields.b, model->fields.c,
                                         model->fields.d, model->fields.e, model->fields.f,
                                         model->fields.g, model->fields.h, model->fields.i,
                                         model->extra_j,  model->extra_k};
    const size_t width = strcmp(model->source_class, "o") == 0 ? 11u : 9u;
    size_t count = 0;
    for (size_t i = 0; i < width; ++i)
        if (all[i])
            parts[count++] = all[i];
    return count;
}

static int mob_emit_state(mob_fixture *f, unsigned int index, const char *phase) {
    FILE *out = f->output;
    fprintf(out, "{\"event\":\"state\",\"case\":%u,\"phase\":\"%s\",\"graph\":", index, phase);
    CHECK(mob_emit_graph(f) == 0);
    fputs(",\"parts\":[", out);
    const manager_model *models[] = {f->renderer->primary, f->renderer->secondary,
                                     f->renderer->own_model};
    size_t ordinal = 0;
    for (size_t model = 0; model < MOB_COUNT(models); ++model) {
        if (!models[model] || (model > 0 && models[model] == models[0]) ||
            (model > 1 && models[model] == models[1]))
            continue;
        const mc_client_model_part *parts[11];
        const size_t count = mob_parts(models[model], f->renderer, parts);
        for (size_t i = 0; i < count; ++i) {
            const mc_client_model_part *part = parts[i];
            mc_client_source_list_state state = {0};
            const bool valid = part->o > 0 &&
                               mc_client_display_list_inspect(f->display, part->o, &state) &&
                               state.defined;
            fprintf(out, "%s{\"model\":", ordinal++ ? "," : "");
            mob_ref(out, models[model], models[model]->source_class);
            fputs(",\"part\":", out);
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

static int mob_frame(mob_fixture *f, unsigned int index) {
    (void)index;
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
    const manager_model *models[] = {f->renderer->primary, f->renderer->secondary,
                                     f->renderer->own_model};
    for (size_t model = 0; model < MOB_COUNT(models); ++model) {
        if (!models[model] || (model > 0 && models[model] == models[0]) ||
            (model > 1 && models[model] == models[1]))
            continue;
        const mc_client_model_part *parts[11];
        const size_t count = mob_parts(models[model], f->renderer, parts);
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
            ++f->observed_lists;
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

/* PRIVATE candidate. Counters/arrays below observe the real retained call. */
static void remaining_nw_fields(FILE *out, const mc_client_nw_fields *n) {
    bool first = true;
    fputc('{', out);
    mob_field_int(out, &first, "nw.h", n->h);
    mob_field_double(out, &first, "nw.i", n->i);
    mob_field_double(out, &first, "nw.j", n->j);
    mob_field_int(out, &first, "nw.k", n->k);
    mob_field_bool(out, &first, "nw.l", n->l);
    mob_field_bool(out, &first, "nw.m", n->m);
    mob_field_bool(out, &first, "nw.n", n->n);
    mob_field_int(out, &first, "nw.o", n->o);
    mob_field_int(out, &first, "nw.p", n->p);
    mob_field_bool(out, &first, "nw.q", n->q);
    mob_field_int(out, &first, "nw.r", n->r);
    mob_field_double(out, &first, "nw.s", n->s);
    mob_field_double(out, &first, "nw.t", n->t);
    mob_field_double(out, &first, "nw.u", n->u);
    mob_field_int(out, &first, "nw.v", n->v);
    mob_field_bool(out, &first, "nw.w", n->w);
    mob_field_bool(out, &first, "nw.x", n->x);
    mob_field_int(out, &first, "nw.z", n->z);
    mob_field_int(out, &first, "nw.A", n->A);
    mob_field_int(out, &first, "nw.B", n->B);
    fputc('}', out);
}

static int remaining_model_work(mob_fixture *f, unsigned int index,
                                const mc_client_model_runtime_state *w) {
    const bool wolf = strcmp(f->kind, "gi") == 0;
    CHECK(w && w->model == f->renderer->primary && w->returned && w->pose_returned &&
          w->part_calls == (wolf ? 11u : 7u) && w->part_returns == w->part_calls);
    CHECK(w->part_a_calls == (wolf ? 6u : 7u) && w->part_a_returns == w->part_a_calls &&
          w->part_b_calls == (wolf ? 5u : 0u) && w->part_b_returns == w->part_b_calls);
    if (wolf)
        CHECK(w->pose_stores == 9 && w->math_calls == 0);
    else
        CHECK(w->fh_pose_returned && w->ej_pose_returned && w->fh_render_returned &&
              w->math_calls == 23 && w->cosine_calls == 10 && w->sine_calls == 12 &&
              w->square_root_calls == 1 && w->absolute_calls == 0);
    fprintf(f->output,
            "{\"event\":\"model_work\",\"case\":%u,\"phase\":\"primary\",\"model\":", index);
    mob_ref(f->output, w->model, w->runtime_class);
    fprintf(f->output,
            ",\"pose_stores\":%u,\"math_calls\":%u,\"cosine_calls\":%u,\"sine_calls\":%u,\"sqrt_"
            "calls\":%u,\"part_a_calls\":%u,\"part_b_calls\":%u,\"part_c_calls\":%u,\"fh_pose_"
            "returned\":%s,\"ej_pose_returned\":%s,\"fh_render_returned\":%s,\"pose_site\":",
            w->pose_stores, w->math_calls, w->cosine_calls, w->sine_calls, w->square_root_calls,
            w->part_a_calls, w->part_b_calls, w->part_c_calls,
            w->fh_pose_returned ? "true" : "false", w->ej_pose_returned ? "true" : "false",
            w->fh_render_returned ? "true" : "false");
    mob_site(f->output, w->pose_site);
    fputs(",\"render_site\":", f->output);
    mob_site(f->output, w->render_site);
    fputs(",\"arguments\":[", f->output);
    CHECK(w->argument_count == 6);
    for (size_t i = 0; i < 6; ++i) {
        if (i)
            fputc(',', f->output);
        mob_float(f->output, w->arguments[i]);
    }
    fputs("],\"fh_pose_site\":", f->output);
    if (w->fh_pose_returned)
        mob_site(f->output, w->fh_pose_site);
    else
        fputs("null", f->output);
    fputs(",\"ej_pose_site\":", f->output);
    if (w->ej_pose_returned)
        mob_site(f->output, w->ej_pose_site);
    else
        fputs("null", f->output);
    fputs(",\"fh_render_site\":", f->output);
    if (w->fh_render_returned)
        mob_site(f->output, w->fh_render_site);
    else
        fputs("null", f->output);
    fputs("}\n", f->output);
    return 0;
}

static int remaining_event_work(mob_fixture *f, unsigned int index,
                                const mc_client_model_runtime_state *w) {
    const bool wolf = strcmp(f->kind, "gi") == 0;
    CHECK(w && w->returned && w->event_returned && w->model == f->renderer->primary &&
          w->actor_identity == (uintptr_t)f->actor && w->argument_count == 3 &&
          w->math_calls == (wolf ? (index == 7   ? 9u
                                    : index == 8 ? 12u
                                                 : 13u)
                                 : 0u) &&
          w->cosine_calls == (wolf ? (index == 7   ? 1u
                                      : index == 8 ? 4u
                                                   : 5u)
                                   : 0u) &&
          w->sine_calls == (wolf ? 8u : 0u) && w->square_root_calls == 0 && w->part_calls == 0);
    fprintf(f->output, "{\"event\":\"event_work\",\"case\":%u,\"model\":", index);
    mob_ref(f->output, w->model, w->runtime_class);
    fputs(",\"actor\":", f->output);
    mob_ref(f->output, (const void *)w->actor_identity, f->kind);
    fputs(",\"arguments\":[", f->output);
    for (size_t i = 0; i < 3; ++i) {
        if (i)
            fputc(',', f->output);
        mob_float(f->output, w->arguments[i]);
    }
    fprintf(f->output,
            "],\"math_calls\":%u,\"cosine_calls\":%u,\"sine_calls\":%u,\"pose_stores\":%u,\"pivot_"
            "calls\":%u,\"pivot_stores\":%u,\"entity_leaf_calls\":%u,\"event_site\":",
            w->math_calls, w->cosine_calls, w->sine_calls, w->pose_stores, w->pivot_calls,
            w->pivot_stores, w->entity_leaf_calls);
    mob_site(f->output, w->event_site);
    fputs("}\n", f->output);
    return 0;
}

static int remaining_bow(mob_fixture *f, unsigned int index) {
    const mc_held_stack_parent_error *e = f->call->held_workspace;
    CHECK(e && e->status == MC_HELD_STACK_PARENT_OK && e->state.source_returned &&
          e->state.hand == f->hand && e->state.ra == mc_client_held_item_identity(f->hand) &&
          e->state.stack == f->class_stack && e->state.actor == f->actor &&
          e->state.application == f->hand->source->a &&
          e->state.engine == f->hand->source->a->render_engine && e->state.icon_returned &&
          e->state.geometry_returned);
    const mc_held_bow_state *s = &e->geometry.state;
    CHECK(s->fragment_completed && s->rescale_disabled && s->pop_returned &&
          s->begin_returns == 6 && s->normal_returns == 6 && s->draw_returns == 6 &&
          s->vertex_returns == 264);
    fprintf(f->output, "{\"event\":\"held\",\"case\":%u,\"hand\":", index);
    mob_ref(f->output, e->state.ra, "ra");
    fputs(",\"stack\":", f->output);
    mob_ref(f->output, e->state.stack, "iz");
    fputs(",\"application\":", f->output);
    mob_ref(f->output, e->state.application, "kq");
    fputs(",\"engine\":", f->output);
    mob_ref(f->output, e->state.engine, "ji");
    fputs(",\"key\":", f->output);
    mob_ref(f->output, e->state.key, "java.lang.String");
    mc_player_string_view key;
    CHECK(e->state.key && mc_player_string_read(e->state.key, &key));
    fputs(",\"key_text\":\"", f->output);
    for (size_t i = 0; i < key.length; ++i)
        fprintf(f->output, "\\u%04x", key.units[i]);
    fprintf(f->output,
            "\",\"icon\":%d,\"texture\":%u,\"source_returned\":true,\"geometry_returned\":true}\n",
            e->state.icon, e->state.texture);
    static const int32_t pcs[] = {335, 423, 554, 687, 824, 959};
    for (size_t i = 0; i < 6; ++i) {
        const mc_held_bow_batch_receipt *b = &e->geometry.batches[i];
        CHECK(b->attempted && b->returned && b->captured && b->word_count == (i < 2 ? 48u : 768u) &&
              b->before.o == (int32_t)b->word_count && b->before.w && !b->after.w);
        fprintf(f->output,
                "{\"event\":\"bow_batch\",\"case\":%u,\"ordinal\":%zu,\"caller_pc\":%d,\"scope\":"
                "\"captured_raw_NW_g\",\"submission_bytes_paired\":false,\"before\":",
                index, i, pcs[i]);
        remaining_nw_fields(f->output, &b->before);
        fputs(",\"after_native_only\":", f->output);
        remaining_nw_fields(f->output, &b->after);
        fputs(",\"words\":[", f->output);
        mob_words(f->output, (const uint8_t *)b->words, b->word_count);
        fputs("]}\n", f->output);
        ++f->observed_bow_batches;
    }
    return 0;
}

typedef struct remaining_prefix {
    mc_client_source_ra ra;
    mc_client_source_iy iy;
    mc_client_render_blocks cv;
    manager_model model;
    mc_client_model_part parts[11];
    size_t part_count;
    mc_client_nw_fields nw;
    int32_t *words;
    size_t word_count;
    float projection[16], modelview[16];
    mc_client_source_draw_state draw;
    mc_client_source_fog_state fog;
    int32_t vector_cursor;
    uint64_t own, camera, world, shared;
    mc_item_stack_fields stack;
} remaining_prefix;

static int remaining_prefix_get(mob_fixture *f, remaining_prefix *p) {
    memset(p, 0, sizeof(*p));
    p->ra = *f->hand->source;
    p->iy = *p->ra.f;
    p->cv = *p->ra.e;
    p->model = *f->renderer->primary;
    const mc_client_model_part *parts[11];
    p->part_count = mob_parts(f->renderer->primary, f->renderer, parts);
    CHECK(p->part_count <= 11);
    for (size_t i = 0; i < p->part_count; ++i)
        p->parts[i] = *parts[i];
    CHECK(mc_client_nw_fields_get(f->runtime.graphics->nw.a, &p->nw) && p->nw.B > 0 &&
          (size_t)p->nw.B <= 2u * 1024u * 1024u);
    p->word_count = (size_t)p->nw.B;
    p->words = malloc(p->word_count * sizeof(*p->words));
    CHECK(p->words);
    memcpy(p->words, p->nw.g, p->word_count * sizeof(*p->words));
    CHECK(mc_client_display_camera_matrices_inspect(f->display, p->projection, p->modelview) &&
          mc_client_display_source_draw_inspect(f->display, &p->draw) &&
          mc_client_display_source_fog_inspect(f->display, &p->fog));
    p->vector_cursor = f->runtime.services.entities->vector_cursor;
    p->own = f->actor->random->state;
    p->camera = f->camera->random->state;
    p->world = f->world->fields.r->state;
    p->shared = f->runtime.services.profile->math->state;
    CHECK(mc_item_stack_fields_get(f->class_stack, &p->stack) == MC_ITEM_STACK_OK);
    return 0;
}

static int remaining_prefix_equal(mob_fixture *f, const remaining_prefix *p) {
    remaining_prefix *q = calloc(1, sizeof(*q));
    CHECK(q && remaining_prefix_get(f, q) == 0);
    CHECK(memcmp(&p->ra, &q->ra, sizeof(p->ra)) == 0 &&
          memcmp(&p->iy, &q->iy, sizeof(p->iy)) == 0 &&
          memcmp(&p->cv, &q->cv, sizeof(p->cv)) == 0 &&
          memcmp(&p->model, &q->model, sizeof(p->model)) == 0 && p->part_count == q->part_count &&
          memcmp(p->parts, q->parts, p->part_count * sizeof(*p->parts)) == 0);
    /* Compare observed named fields, not unspecified structure padding. */
    CHECK(p->nw.h == q->nw.h && p->nw.o == q->nw.o && p->nw.p == q->nw.p && p->nw.w == q->nw.w &&
          p->nw.g == q->nw.g && p->nw.e == q->nw.e && p->word_count == q->word_count &&
          memcmp(p->words, q->words, p->word_count * sizeof(*p->words)) == 0);
    CHECK(memcmp(p->projection, q->projection, sizeof(p->projection)) == 0 &&
          memcmp(p->modelview, q->modelview, sizeof(p->modelview)) == 0 &&
          p->draw.color_known == q->draw.color_known &&
          p->fog.normal_known == q->fog.normal_known &&
          (!p->draw.color_known ||
           memcmp(p->draw.color, q->draw.color, sizeof(p->draw.color)) == 0) &&
          (!p->fog.normal_known ||
           memcmp(p->fog.normal, q->fog.normal, sizeof(p->fog.normal)) == 0) &&
          p->draw.texture_enabled == q->draw.texture_enabled &&
          p->draw.alpha_test == q->draw.alpha_test &&
          p->fog.normalize_enabled == q->fog.normalize_enabled &&
          p->fog.rescale_normal_enabled == q->fog.rescale_normal_enabled);
    CHECK(p->vector_cursor == q->vector_cursor && p->own == q->own && p->camera == q->camera &&
          p->world == q->world && p->shared == q->shared &&
          memcmp(&p->stack, &q->stack, sizeof(p->stack)) == 0);
    free(q->words);
    free(q);
    return 0;
}

static int remaining_guard_controls(mob_fixture *f, unsigned int index,
                                    remaining_prefix **prefix_out) {
    mc_client_render_manager_draw_call *call = f->call;
    mc_held_stack_parent_error *workspace = call->held_workspace;
    const void *token = call->held_guard_token;
    CHECK(workspace && token == workspace && call->held_guard == f->hand && f->hand->active &&
          f->hand->held_stack_parent == call && f->hand->held_stack_token == token &&
          f->hand_extra == f->hand);
    remaining_prefix *before = calloc(1, sizeof(*before));
    CHECK(before && remaining_prefix_get(f, before) == 0);
    const size_t holds = f->hand->holds;
    mc_client_held_item_draw_call *first_person = NULL;
    mc_client_held_item_draw_error *draw_error = calloc(1, sizeof(*draw_error));
    mc_client_held_item_tick_error *tick_error = calloc(1, sizeof(*tick_error));
    mc_client_entity_renderer_error *error = calloc(1, sizeof(*error));
    CHECK(draw_error && tick_error && error);
    CHECK(mc_client_held_item_draw_begin(f->hand, f->math, .375f, false, &first_person,
                                         draw_error) == MC_CLIENT_HELD_ITEM_DRAW_BUSY &&
          first_person == NULL && draw_error->native_policy);
    mc_client_held_item_tick_call *tick = NULL;
    CHECK(mc_client_held_item_tick_begin(f->hand, &tick, tick_error) ==
              MC_CLIENT_HELD_ITEM_TICK_BUSY &&
          tick == NULL && tick_error->native_policy);
    CHECK(mc_client_held_item_retain(f->hand, error) == MC_CLIENT_ENTITY_RENDERER_BUSY &&
          error->native_policy);
    CHECK(mc_client_held_item_release(&f->hand_extra, error) == MC_CLIENT_ENTITY_RENDERER_BUSY &&
          f->hand_extra == f->hand && error->native_policy);
    const int wrong_token = 0;
    CHECK(entity_renderer_held_stack_guard_discharge(f->hand, call, &wrong_token, error) ==
              MC_CLIENT_ENTITY_RENDERER_INVALID_ARGUMENT &&
          error->native_policy);
    CHECK(!mc_client_draw_held_guard_discharge(&call->scope, f->hand, token, error) &&
          error->status == MC_CLIENT_ENTITY_RENDERER_BUSY && error->native_policy);
    CHECK(f->call == call && call->held_workspace == workspace && call->held_guard_token == token &&
          call->held_guard == f->hand && f->hand->held_stack_parent == call &&
          f->hand->held_stack_token == token && f->hand->holds == holds &&
          remaining_prefix_equal(f, before) == 0);
    free(error);
    free(tick_error);
    free(draw_error);
    *prefix_out = before;
    fprintf(f->output,
            "{\"event\":\"control\",\"name\":\"held_guard\",\"case\":%u,\"first_person_busy\":true,"
            "\"held_tick_busy\":true,\"retain_busy\":true,\"release_busy\":true,\"wrong_token_"
            "refused\":true,\"nonretiring_refused\":true,\"prefix_preserved\":true}\n",
            index);
    return 0;
}

static int mob_profile(mob_fixture *f, unsigned int index, const char *directory) {
    CHECK(remaining_inputs(f, index) == 0 && mob_frame(f, index) == 0 &&
          mob_pixels(f, directory, index, true) == 0 && mob_emit_state(f, index, "before") == 0);
    const uint64_t own = f->actor->random->state, camera = f->camera->random->state,
                   world = f->world->fields.r->state,
                   shared = f->runtime.services.profile->math->state;
    const int32_t pool = f->runtime.services.entities->vector_cursor;
    const size_t borrows = atomic_load(&f->app->render_borrows),
                 alternate_borrows = atomic_load(&f->alternate_app->render_borrows);
    const bool skeleton = strcmp(f->kind, "fr") == 0;
    if (skeleton) {
        CHECK(f->hand_extra == NULL &&
              mc_client_held_item_retain(f->hand, NULL) == MC_CLIENT_ENTITY_RENDERER_OK);
        f->hand_extra = f->hand;
    }
    const mc_client_manager_draw_services services = {f->math, f->runtime.graphics, f->display,
                                                      f->runtime.services.application_classes};
    const float partial = (index == 1 || index == 3 || index == 5) ? 1.f : .375f;
    const mc_client_manager_status status = mc_client_render_manager_draw_begin(
        f->manager, f->actor, partial, &services, NULL, &f->call, f->draw_error);
    if (status != MC_CLIENT_MANAGER_OK)
        fprintf(stderr, "Remaining draw case%u status%d %s.%s:%d %s\n", index, (int)status,
                f->draw_error->site.class_name ? f->draw_error->site.class_name : "?",
                f->draw_error->site.method ? f->draw_error->site.method : "?",
                f->draw_error->site.pc, f->draw_error->operation ? f->draw_error->operation : "?");
    CHECK(status == MC_CLIENT_MANAGER_OK && f->call);
    mc_client_manager_draw_state state;
    CHECK(mc_client_render_manager_draw_inspect(f->call, &state) && state.returned &&
          state.actor == f->actor && state.renderer == f->renderer && state.primary_returned &&
          state.post_draw_returned && state.living && state.living->returned && state.post_draw &&
          state.post_draw->shadow_returned && state.post_draw->fire_checked &&
          !state.post_draw->burning);
    CHECK(state.living->color_returned && state.living->color == 0 &&
          state.living->primary_event_returned && state.living->primary_render_returned);
    for (size_t i = 0; i < 4; ++i)
        CHECK(state.living->pass_attempted[i] && !state.living->pass_result[i]);
    CHECK(remaining_hooks(f, index, state.living) == 0 &&
          remaining_event_work(f, index, &state.living->primary_event) == 0 &&
          remaining_model_work(f, index, &state.living->primary_render) == 0);
    if (skeleton)
        CHECK(remaining_bow(f, index) == 0);
    else
        CHECK(f->call->held_workspace == NULL);
    if (index == 12)
        CHECK(remaining_alternate_texture(f, index) == 0);
    CHECK(f->actor->random->state == own && f->camera->random->state == camera &&
          f->world->fields.r->state == world && f->runtime.services.profile->math->state == shared);
    static const int32_t growth[] = {264, 0, 168, 0, 168, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    CHECK(f->runtime.services.entities->vector_cursor == pool + growth[index]);
    CHECK(mob_emit_state(f, index, "after") == 0 && mob_pixels(f, directory, index, false) == 0);
    mc_client_nw_state nw;
    CHECK(mc_client_nw_inspect(f->runtime.graphics, &nw) && nw.fields.e &&
          nw.fields.e->position == 192 && !nw.fields.w);
    fprintf(f->output,
            "{\"event\":\"draw\",\"index\":%u,\"outcome\":1,\"nw_position\":%zu,\"nw_words\":[",
            index, nw.fields.e->position);
    mob_words(f->output, (const uint8_t *)nw.fields.g, nw.fields.e->position);
    uint32_t gl_error = UINT32_MAX;
    CHECK(mc_client_display_source_get_error(f->display, &gl_error, NULL) && gl_error == 0);
    fprintf(f->output, "],\"gl_error\":%u}\n", gl_error);
    if (index == 0 || index == 2 || index == 4) {
        CHECK(mob_lists(f) == 0);
    }
    CHECK(mob_queries(f, index) == 0 && mob_texture_calls(f, index, &state) == 0);
    remaining_prefix *prefix = NULL;
    if (skeleton)
        CHECK(remaining_guard_controls(f, index, &prefix) == 0);
    CHECK(mc_client_application_constructor_retire(&f->app, f->runtime.error) ==
              MC_CLIENT_APP_BUSY &&
          f->app->constructor_phase == MC_CLIENT_APP_RAW &&
          f->runtime.services.application_classes->a == f->app);
    if (index == 12)
        CHECK(mc_client_application_constructor_retire(&f->alternate_app, f->runtime.error) ==
                  MC_CLIENT_APP_BUSY &&
              f->alternate_app->constructor_phase == MC_CLIENT_APP_RAW);
    CHECK(mc_client_render_manager_draw_retire(&f->call, f->draw_error) == MC_CLIENT_MANAGER_OK &&
          f->call == NULL && atomic_load(&f->app->render_borrows) == borrows &&
          atomic_load(&f->alternate_app->render_borrows) == alternate_borrows);
    if (skeleton) {
        CHECK(!f->hand->active && f->hand->held_stack_parent == NULL &&
              f->hand->held_stack_token == NULL && remaining_prefix_equal(f, prefix) == 0);
        free(prefix->words);
        free(prefix);
        mc_client_held_item_state after;
        CHECK(mc_client_held_item_inspect(f->hand, &after) &&
              after.ra_phase == MC_CLIENT_ENTITY_RENDERER_READY &&
              mc_client_held_item_retain(f->hand, NULL) == MC_CLIENT_ENTITY_RENDERER_OK);
        mc_client_held_item *extra = f->hand;
        CHECK(mc_client_held_item_release(&extra, NULL) == MC_CLIENT_ENTITY_RENDERER_OK &&
              extra == NULL &&
              mc_client_held_item_release(&f->hand_extra, NULL) == MC_CLIENT_ENTITY_RENDERER_OK &&
              f->hand_extra == NULL);
        fprintf(f->output,
                "{\"event\":\"control\",\"name\":\"held_retirement\",\"case\":%u,\"guard_"
                "discharged\":true,\"RA_usable\":true,\"prefix_preserved\":true}\n",
                index);
    }
    fprintf(f->output,
            "{\"event\":\"control\",\"name\":\"retained_graph\",\"case\":%u,\"app_retire_busy\":"
            "true,\"render_borrows_restored\":true}\n",
            index);
    return 0;
}

static int mob_close(mob_fixture *f) {
    CHECK(f->call == NULL && f->hand_extra == NULL);
    CHECK(mc_client_render_manager_consumer_detach(&f->manager_consumer, f->manager_error) ==
              MC_CLIENT_MANAGER_OK &&
          !f->manager_consumer.attached && f->manager_consumer.classes == NULL);
    CHECK(mc_client_render_manager_retire_hands(f->managers, NULL) == MC_CLIENT_MANAGER_OK);
    CHECK(f->alternate_candidate == NULL);
    f->hand = f->initial_hand = f->alternate_hand =
        NULL; /* borrowed occurrences ended; no Source TH.f reset */
    f->alternate_app->active_world = NULL;
    CHECK(mc_client_render_engine_retire_lookup(f->alternate_app->render_engine, f->lookup_error) ==
              MC_CLIENT_LOOKUP_OK &&
          mc_client_texture_lookup_free_metadata(f->alternate_app->render_engine) &&
          mc_client_render_engine_release(f->alternate_app->render_engine) ==
              MC_CLIENT_RENDER_ENGINE_OK);
    f->alternate_app->render_engine = NULL;
    CHECK(mc_client_texture_pack_list_release(f->alternate_packs, NULL) == MC_TEXTURE_PACK_OK &&
          f->alternate_app->D == NULL && f->alternate_app->font_renderer == NULL &&
          mc_game_settings_retire(&f->alternate_app->settings, NULL) == MC_GAME_SETTINGS_OK);
    f->alternate_app->display = NULL;
    CHECK(mc_client_application_constructor_retire(&f->alternate_app, f->runtime.error) ==
              MC_CLIENT_APP_OK &&
          f->alternate_app == NULL);
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
    CHECK(f->app->D == NULL);
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
    free(f->manager_error);
    free(f->hand_error);
    free(f->draw_error);
    free(f->lookup_error);
    CHECK(fixture_close(&f->runtime) == 0);
    return 0;
}

int main(int argc, char **argv) {
    CHECK(argc == 5);
    SDL_SetMainReady();
    mob_fixture *f = calloc(1, sizeof(*f));
    CHECK(f);
    char path[4096];
    CHECK(snprintf(path, sizeof(path), "%s/native.ndjson", argv[4]) > 0);
    f->output = fopen(path, "wb");
    CHECK(f->output && mob_open(f, argv[1], argv[2], argv[3]) == 0);
    fprintf(f->output,
            "{\"event\":\"context\",\"scope\":\"PRIVATE_V2_FOURTEEN_CANDIDATE\",\"RAW_app\":true,"
            "\"namespace_attached\":true,\"canonical_nonnull\":true,\"static_holds\":%zu,\"default_"
            "profile_rebound\":false,\"displaced_LC_constructor_graph_paired\":false,\"live_GL_"
            "order_paired\":false}\n",
            atomic_load(&f->app->source_static_holds));
    const mc_client_display_info *device = mc_client_display_info_get(f->display);
    CHECK(device);
    fputs("{\"event\":\"device\",\"vendor\":", f->output);
    mob_string(f->output, device->vendor);
    fputs(",\"renderer\":", f->output);
    mob_string(f->output, device->renderer);
    fputs(",\"version\":", f->output);
    mob_string(f->output, device->version);
    fputs("}\n", f->output);
    CHECK(mob_textures(f) == 0 && mob_prepare(f) == 0 &&
          remaining_alternate_prepare(f, argv[4]) == 0);
    mc_client_tessellator *tessellator = NULL;
    mc_client_graphics_error *error = calloc(1, sizeof(*error));
    const mc_client_graphics_site site = {"RemainingDrawFixture", "prepare", "()V", -1};
    CHECK(error && mc_client_nw_active_use(f->runtime.graphics, site, &tessellator, error));
    free(error);
    mc_client_nw_state nw;
    CHECK(mc_client_nw_inspect(f->runtime.graphics, &nw) && nw.fields.g && nw.fields.e &&
          nw.fields.B > 0);
    memset((void *)nw.fields.g, 0, (size_t)nw.fields.B * sizeof(*nw.fields.g));
    ((mc_client_direct_int_buffer *)(void *)nw.fields.e)->position = 0;
    CHECK(mc_entity_vector_pool_reset(f->runtime.services.entities, NULL) == MC_ENTITY_OK);
    for (unsigned int i = 0; i < 14; ++i)
        CHECK(mob_select(f, i) == 0 && mob_profile(f, i, argv[4]) == 0);
    CHECK(f->observed_lists == 25 && f->observed_bow_batches == 30);
    fprintf(f->output,
            "{\"event\":\"footer\",\"cases\":14,\"model_lists\":%zu,\"Bow_backing_batches\":%zu,"
            "\"scope\":\"private_v2_fourteen\"}\n",
            f->observed_lists, f->observed_bow_batches);
    CHECK(!ferror(f->output) && fclose(f->output) == 0 && mob_close(f) == 0);
    free(f);
    printf("Remaining draw private: %u checks;14 genuine profiles\n", checks);
    return 0;
}
