/* Genuine FD/EW/FW paths. The included authored Region fixture supplies
 * production World/Registry/Entity/Math owners; its FD.n entry is unused. */
int mc_path_world_fixture_main(int argc, char **argv);
#define main mc_path_world_fixture_main
#include "test_world_random_tick.c"
#undef main
#include "../../../src/entity/mob/internal.h"
#include "../../../src/entity/mob/references_internal.h"
#include "../../../src/world/path/path_internal.h"
#include "../../../src/world/runtime/chunk_cache_internal.h"
#include "../../support/java_random.h"
#include "mc/world/runtime/path.h"

#undef CHECK
#define CHECK(c)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(c)) {                                                                                \
            fprintf(stderr, "Path check%u line%d: %s\n", checks, __LINE__, #c);                    \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

typedef struct path_profile {
    unsigned int id;
    char operation[32], scene[64];
    double source[3], target[3];
    uint32_t range, width, height;
} path_profile;

typedef struct path_operand {
    unsigned int id;
    char operation[24], owner[16], member[32], type[24], value[192];
} path_operand;

typedef struct path_fixture {
    random_fixture base;
    path_profile input;
    mc_entity *source, *target;
    mc_entity *source_argument, *target_argument;
    mc_entity_bounds *source_bounds, *target_bounds;
    mc_world_path_call *call;
    mc_client_chunk_cache *cache;
    mc_path_finder *finder;
    mc_path_budget budget;
    int32_t extent[3], step;
    int32_t point_inputs[2][3];
    bool points_stored, access_null;
    FILE *output;

    struct {
        const void *value;
        const char *class_name;
        size_t length;
    } extra[4];

    size_t extra_count;
    const void **extra_identity;
    void *holder_allocations[4];
    size_t holder_count;
    mc_path_point **component_points;
    size_t component_count;
    const void *result;
    const char *result_class;

    enum {
        PATH_VALUE_NULL,
        PATH_VALUE_REFERENCE,
        PATH_VALUE_INT,
        PATH_VALUE_FLOAT,
        PATH_VALUE_BOOL
    } result_kind;

    int32_t result_integer;
    float result_float;
    bool result_boolean;
    mc_path_error component_error;
    bool steps_open;
    size_t call_bindings, call_source_holds, call_target_holds;
    bool source_owned, target_owned, control_done;
    mc_path_query direct_query;
    bool has_direct_query;
} path_fixture;

static float word_float(uint32_t word) {
    float value;
    memcpy(&value, &word, sizeof(value));
    return value;
}

static bool replace_holder(path_fixture *f) {
    CHECK(f->holder_count < COUNT(f->holder_allocations));
    const void **holder = calloc(f->extra_count ? f->extra_count : 1, sizeof(*holder));
    CHECK(holder);
    for (size_t index = 0; index < f->extra_count; ++index)
        holder[index] = f->extra[index].value;
    f->holder_allocations[f->holder_count++] = holder;
    f->extra_identity = holder;
    return true;
}

static bool profile_table(const char *path, path_profile *profiles, size_t *count) {
    FILE *file = open_file(path, "rb");
    CHECK(file);
    char line[768];
    CHECK(fgets(line, sizeof(line), file));
    size_t used = 0;
    while (fgets(line, sizeof(line), file)) {
        CHECK(used < 256);
        path_profile *p = &profiles[used];
#ifdef _MSC_VER
        const int fields =
            sscanf_s(line, "%u %31s %63s %lf %lf %lf %lf %lf %lf %8x %8x %8x", &p->id, p->operation,
                     (unsigned int)sizeof(p->operation), p->scene, (unsigned int)sizeof(p->scene),
                     &p->source[0], &p->source[1], &p->source[2], &p->target[0], &p->target[1],
                     &p->target[2], &p->range, &p->width, &p->height);
#else
        const int fields =
            sscanf(line, "%u %31s %63s %lf %lf %lf %lf %lf %lf %8x %8x %8x", &p->id, p->operation,
                   p->scene, &p->source[0], &p->source[1], &p->source[2], &p->target[0],
                   &p->target[1], &p->target[2], &p->range, &p->width, &p->height);
#endif
        CHECK(fields == 12 && p->id == used);
        ++used;
    }
    CHECK(!ferror(file) && fclose(file) == 0 && used);
    *count = used;
    return true;
}

static bool operand_table(const char *path, path_operand *operands, size_t *count) {
    FILE *file = open_file(path, "rb");
    CHECK(file);
    char line[768];
    CHECK(fgets(line, sizeof(line), file));
    size_t used = 0;
    while (fgets(line, sizeof(line), file)) {
        CHECK(used < 512);
        path_operand *v = &operands[used];
#ifdef _MSC_VER
        const int fields =
            sscanf_s(line, "%u %23s %15s %31s %23s %191s", &v->id, v->operation,
                     (unsigned int)sizeof(v->operation), v->owner, (unsigned int)sizeof(v->owner),
                     v->member, (unsigned int)sizeof(v->member), v->type,
                     (unsigned int)sizeof(v->type), v->value, (unsigned int)sizeof(v->value));
#else
        const int fields = sscanf(line, "%u %23s %15s %31s %23s %191s", &v->id, v->operation,
                                  v->owner, v->member, v->type, v->value);
#endif
        CHECK(fields == 6);
        ++used;
    }
    CHECK(!ferror(file) && fclose(file) == 0);
    *count = used;
    return true;
}

static bool integers(const char *text, int32_t *values, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        char *end = NULL;
        const long long value = strtoll(text, &end, 10);
        CHECK(end != text && value >= INT32_MIN && value <= INT32_MAX &&
              (index + 1 == count ? !*end : *end == ','));
        values[index] = (int32_t)value;
        text = end + (index + 1 == count ? 0 : 1);
    }
    return true;
}

static bool terrain_cell(path_fixture *f, int32_t x, int32_t y, int32_t z, int32_t id,
                         int32_t metadata) {
    mc_chunk_state *chunk = NULL;
    CHECK(y >= 0 && y < 128 && id >= 0 && id < 256 && metadata >= 0 && metadata < 16 &&
          mc_chunk_cache_lookup(f->base.provider.cache, mc_java_i32_shr(x, 4),
                                mc_java_i32_shr(z, 4), &chunk) &&
          chunk && !chunk->blank && chunk->data);
    const size_t at =
        ((size_t)((uint32_t)x & 15u) << 11) | ((size_t)((uint32_t)z & 15u) << 7) | (size_t)y;
    chunk->data->blocks[at] = (uint8_t)id;
    const unsigned int shift = (unsigned int)(at & 1u) * 4u;
    const uint8_t mask = (uint8_t)(15u << shift);
    chunk->data->metadata[at >> 1] = (uint8_t)((chunk->data->metadata[at >> 1] & (uint8_t)~mask) |
                                               (uint8_t)((unsigned int)metadata << shift));
    const size_t column = ((size_t)((uint32_t)z & 15u) << 4) | ((uint32_t)x & 15u);
    if (id > 0 && chunk->data->heightmap[column] < y + 1)
        chunk->data->heightmap[column] = (uint8_t)(y + 1);
    return true;
}

static bool make_cow(path_fixture *f, const double *position, uint32_t width, uint32_t height,
                     mc_entity **out) {
    const mc_entity_class_literal *literal = NULL;
    mc_mob_error error;
    CHECK(mc_entity_class_literal_get(f->base.entities, MC_ENTITY_CLASS_KIND_COW, &literal) ==
              MC_ENTITY_OK &&
          mc_mob_allocate(f->base.entities, f->base.registry, literal, out, &error) == MC_MOB_OK &&
          mc_mob_construct(*out, f->base.world, &error) == MC_MOB_OK &&
          (*out)->phase == MC_ENTITY_READY && (*out)->mob && !(*out)->local_player);
    /* Declared SN.a(FF) size stores followed by genuine e(DDD) bounds update.
     * No constructor field snapshot is used to execute the caller. */
    (*out)->base.width = word_float(width);
    (*out)->base.height = word_float(height);
    (*out)->base.x = (*out)->base.aJ = (*out)->base.bl = position[0];
    (*out)->base.y = (*out)->base.aK = (*out)->base.bm = position[1] + (*out)->base.offset;
    (*out)->base.z = (*out)->base.aL = (*out)->base.bn = position[2];
    (*out)->base.yaw = 30.f;
    (*out)->base.aT = 0.f;
    CHECK(mc_entity_set_position(*out, position[0], position[1], position[2]) == MC_ENTITY_OK);
    bool inserted = false;
    mc_world_entity_error publication;
    CHECK(mc_world_runtime_insert_entity(f->base.world, *out, &inserted, &publication) ==
              MC_WORLD_STORAGE_OK &&
          inserted);
    return true;
}

static bool prepare_path_fixture(path_fixture *f, const char *resources, const char *directory) {
    CHECK(setup(&f->base, resources, directory) && replace_holder(f));
    for (int32_t x = -2; x <= 2; ++x)
        for (int32_t z = -2; z <= 2; ++z) {
            mc_chunk_state *chunk = NULL;
            CHECK(load_chunk(&f->base, x, z, &chunk) &&
                  mc_chunk_cache_publish(f->base.provider.cache, x, z, chunk));
        }
    CHECK(f->base.owned_count == 25 && f->base.provider.cache->count == 25 &&
          make_cow(f, f->input.source, f->input.width, f->input.height, &f->source));
    f->source_owned = true;
    if (!strcmp(f->input.operation, "FD_entity"))
        CHECK(make_cow(f, f->input.target, UINT32_C(0x3f666666), UINT32_C(0x3fa66666), &f->target));
    f->target_owned = f->target != NULL;
    f->source_argument = f->source;
    f->target_argument = f->target;
    f->extent[0] = f->extent[2] = 1;
    f->extent[1] = 2;
    mc_java_random_reseed(f->source->random, 0);
    if (f->target)
        mc_java_random_reseed(f->target->random, 17);
    mc_java_random_reseed(f->base.world->fields.r, 89);
    CHECK(mc_entity_bounds_pool_reset(f->base.entities, NULL) == MC_ENTITY_OK &&
          mc_entity_vector_pool_reset(f->base.entities, NULL) == MC_ENTITY_OK);
    return true;
}

static bool apply_operands(path_fixture *f, const path_operand *operands, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        const path_operand *v = &operands[index];
        if (v->id != f->input.id)
            continue;
        int32_t values[8];
        if (!strcmp(v->operation, "cell")) {
            CHECK(!strcmp(v->owner, "lm") && !strcmp(v->member, "b/e") &&
                  integers(v->value, values, 5) &&
                  terrain_cell(f, values[0], values[1], values[2], values[3], values[4]));
        } else if (!strcmp(v->operation, "fill")) {
            CHECK(!strcmp(v->owner, "lm") && !strcmp(v->member, "b/e") &&
                  integers(v->value, values, 8) && values[0] <= values[3] &&
                  values[1] <= values[4] && values[2] <= values[5]);
            for (int32_t x = values[0]; x <= values[3]; ++x)
                for (int32_t y = values[1]; y <= values[4]; ++y)
                    for (int32_t z = values[2]; z <= values[5]; ++z)
                        CHECK(terrain_cell(f, x, y, z, values[6], values[7]));
        } else if (!strcmp(v->operation, "extent")) {
            CHECK(!strcmp(v->owner, "d") && integers(v->value, f->extent, 3));
        } else if (!strcmp(v->operation, "step")) {
            CHECK(integers(v->value, &f->step, 1));
        } else if (!strcmp(v->operation, "points")) {
            char pair[192];
            CHECK(strlen(v->value) < sizeof(pair));
            memcpy(pair, v->value, strlen(v->value) + 1);
            char *second = strchr(pair, ';');
            CHECK(second);
            *second++ = '\0';
            CHECK(integers(pair, f->point_inputs[0], 3) && integers(second, f->point_inputs[1], 3));
            f->points_stored = true;
        } else if (!strcmp(v->operation, "field") || !strcmp(v->operation, "target_field")) {
            mc_entity *actor = !strcmp(v->operation, "field") ? f->source : f->target;
            CHECK(!strcmp(v->owner, "sn") && !strcmp(v->member, "aW") && !strcmp(v->type, "R") &&
                  !strcmp(v->value, "NULL") && actor->bounds);
            if (actor == f->source)
                f->source_bounds = actor->bounds;
            else
                f->target_bounds = actor->bounds;
            actor->bounds = NULL;
        } else if (!strcmp(v->operation, "operand")) {
            CHECK(!strcmp(v->type, "R") && !strcmp(v->value, "NULL"));
            if (!strcmp(v->owner, "call") && !strcmp(v->member, "source"))
                f->source_argument = NULL;
            else if (!strcmp(v->owner, "call") && !strcmp(v->member, "target"))
                f->target_argument = NULL;
            else {
                CHECK(!strcmp(v->owner, "fw") && !strcmp(v->member, "access"));
                f->access_null = true;
            }
        } else if (!strcmp(v->operation, "blank_alias")) {
            CHECK(!strcmp(v->owner, "ok") && !strcmp(v->member, "b") &&
                  integers(v->value, values, 2) && f->base.world->fallback.blank &&
                  mc_chunk_cache_publish(f->base.provider.cache, values[0], values[1],
                                         &f->base.world->fallback));
        } else {
            CHECK(false);
            return false;
        }
    }
    return true;
}

static bool failed_path_inputs(const path_fixture *f) {
    return (!f->source_bounds || !f->source->bounds) && (!f->target_bounds || !f->target->bounds);
}

static bool retire_path_fixture(path_fixture *f) {
    if (f->control_done)
        return !f->base.world && !f->base.entities && !f->call && !f->source && !f->target;
    CHECK(failed_path_inputs(f));
    const bool borrowed_children = f->call != NULL;
    CHECK(mc_world_path_call_retire(&f->call) == MC_WORLD_PATH_OK && !f->call &&
          failed_path_inputs(f));
    if (borrowed_children) {
        CHECK(f->base.world->persistent_bindings == f->call_bindings &&
              f->source->holds == f->call_source_holds &&
              (!f->target || f->target->holds == f->call_target_holds));
        f->finder = NULL;
        f->cache = NULL;
    }
    if (f->finder) {
        mc_path_finder_destroy(f->finder);
        f->finder = NULL;
    }
    CHECK(mc_client_chunk_cache_retire(&f->cache, NULL) == MC_CLIENT_CHUNK_CACHE_OK &&
          failed_path_inputs(f));
    /* Native operation retirement proved the original failed prefix. Only
     * final fixture disposal restores its independently retained input fields. */
    if (f->source_bounds)
        f->source->bounds = f->source_bounds;
    if (f->target_bounds)
        f->target->bounds = f->target_bounds;
    if (f->source_owned)
        mc_entity_release(f->source);
    if (f->target_owned)
        mc_entity_release(f->target);
    f->source = f->target = NULL;
    for (size_t index = 0; index < f->holder_count; ++index)
        free(f->holder_allocations[index]);
    return teardown(&f->base);
}

typedef struct graph_object {
    const void *identity;
    const char *class_name;
    size_t length;
} graph_object;

typedef struct graph_writer {
    path_fixture *fixture;
    graph_object *objects;
    size_t count, capacity;
} graph_writer;

static bool graph_expands(const char *name) {
    return name[0] == '[' || !strcmp(name, "fw") || !strcmp(name, "lf") || !strcmp(name, "jx") ||
           !strcmp(name, "wm") || !strcmp(name, "d") || !strcmp(name, "dh") || !strcmp(name, "ew");
}

static void graph_ref(graph_writer *g, const void *identity, const char *class_name,
                      size_t length) {
    FILE *out = g->fixture->output;
    if (!identity) {
        fputs("null", out);
        return;
    }
    fprintf(out, "{\"class\":\"%s\",\"identity\":\"%016" PRIxPTR "\"}", class_name,
            (uintptr_t)identity);
    if (!graph_expands(class_name))
        return;
    for (size_t index = 0; index < g->count; ++index)
        if (g->objects[index].identity == identity) {
            if (strcmp(g->objects[index].class_name, class_name) ||
                g->objects[index].length != length)
                exit(3);
            return;
        }
    if (g->count == g->capacity) {
        const size_t capacity = g->capacity ? g->capacity * 2 : 64;
        graph_object *objects = realloc(g->objects, capacity * sizeof(*objects));
        if (!objects)
            exit(3);
        g->objects = objects;
        g->capacity = capacity;
    }
    g->objects[g->count++] = (graph_object){identity, class_name, length};
}

static void graph_point(graph_writer *g, const mc_path_point *node) {
    graph_ref(g, node, "d", 0);
}

static void graph_result(graph_writer *g) {
    path_fixture *f = g->fixture;
    switch (f->result_kind) {
        case PATH_VALUE_REFERENCE:
            graph_ref(g, f->result, f->result_class, 0);
            break;
        case PATH_VALUE_INT:
            fprintf(f->output, "%d", f->result_integer);
            break;
        case PATH_VALUE_FLOAT:
            fprintf(f->output, "{\"F\":\"%08" PRIx32 "\"}", float_bits(f->result_float));
            break;
        case PATH_VALUE_BOOL:
            fputs(f->result_boolean ? "true" : "false", f->output);
            break;
        default:
            fputs("null", f->output);
            break;
    }
}

static void graph_fields(graph_writer *g, graph_object object) {
    FILE *out = g->fixture->output;
    const char *name = object.class_name;
    if (name[0] == '[') {
        fprintf(out, ",\"length\":%zu,\"values\":[", object.length);
        for (size_t index = 0; index < object.length; ++index) {
            if (index)
                fputc(',', out);
            if (!strcmp(name, "[Ljava.lang.Object;")) {
                const void *const *holder = object.identity;
                if (holder[index] != g->fixture->extra[index].value)
                    exit(3);
                graph_ref(g, g->fixture->extra[index].value, g->fixture->extra[index].class_name,
                          g->fixture->extra[index].length);
            } else if (!strcmp(name, "[[Llm;")) {
                const mc_client_chunk_cache *cache =
                    object.identity == g->fixture->cache->state.c ? g->fixture->cache : NULL;
                if (!cache)
                    exit(3);
                graph_ref(g, cache->state.c[index], "[Llm;", (size_t)cache->state.columns);
            } else if (!strcmp(name, "[Llm;")) {
                mc_chunk_state *const *array = object.identity;
                graph_ref(g, array[index], array[index] && array[index]->blank ? "li" : "lm", 0);
            } else {
                const void *const *array = object.identity;
                graph_ref(g, array[index], !strcmp(name, "[Lwm;") ? "wm" : "d", 0);
            }
        }
        fputc(']', out);
        return;
    }
    fputs(",\"fields\":{", out);
    if (!strcmp(name, "d")) {
        const mc_path_point *p = object.identity;
        fprintf(out,
                "\"d.a\":%d,\"d.b\":%d,\"d.c\":%d,\"d.d\":%d,\"d.e\":{\"F\":\"%08" PRIx32
                "\"},\"d.f\":{\"F\":\"%08" PRIx32 "\"},\"d.g\":{\"F\":\"%08" PRIx32 "\"},\"d.h\":",
                p->a, p->b, p->c, p->d, float_bits(p->e), float_bits(p->f), float_bits(p->g));
        graph_point(g, p->h);
        fprintf(out, ",\"d.i\":%s,\"d.j\":%d", p->i ? "true" : "false", p->j);
    } else if (!strcmp(name, "dh")) {
        const mc_path *p = object.identity;
        fprintf(out, "\"dh.a\":%d,\"dh.b\":", p->a);
        graph_ref(g, p->b, "[Ld;", p->array_length);
        fprintf(out, ",\"dh.c\":%d", p->c);
    } else if (!strcmp(name, "fw")) {
        const mc_path_finder *w = object.identity;
        fputs("\"fw.a\":", out);
        graph_ref(g, w->a, "ew", 0);
        fputs(",\"fw.b\":", out);
        graph_ref(g, w->b, "lf", 0);
        fputs(",\"fw.c\":", out);
        graph_ref(g, w->c, "jx", 0);
        fputs(",\"fw.d\":", out);
        graph_ref(g, w->d, "[Ld;", 32);
    } else if (!strcmp(name, "lf")) {
        const mc_path_heap *heap = object.identity;
        fputs("\"lf.a\":", out);
        graph_ref(g, heap->a, "[Ld;", heap->capacity);
        fprintf(out, ",\"lf.b\":%d", heap->b);
    } else if (!strcmp(name, "jx")) {
        const mc_path_map *map = object.identity;
        fputs("\"jx.a\":", out);
        graph_ref(g, map->a, "[Lwm;", map->capacity);
        fprintf(out, ",\"jx.b\":%d,\"jx.c\":%d,\"jx.d\":{\"F\":\"%08" PRIx32 "\"},\"jx.e\":%d",
                map->b, map->c, float_bits(map->d), map->e);
    } else if (!strcmp(name, "wm")) {
        const mc_path_map_entry *entry = object.identity;
        fprintf(out, "\"wm.a\":%d,\"wm.b\":", entry->a);
        graph_point(g, entry->b);
        fputs(",\"wm.c\":", out);
        graph_ref(g, entry->c, "wm", 0);
        fprintf(out, ",\"wm.d\":%d", entry->d);
    } else if (!strcmp(name, "ew")) {
        const mc_client_chunk_cache *cache = object.identity;
        fprintf(out, "\"ew.a\":%d,\"ew.b\":%d,\"ew.c\":", cache->state.a, cache->state.b);
        graph_ref(g, cache->state.c, "[[Llm;", (size_t)cache->state.rows);
        fputs(",\"ew.d\":", out);
        graph_ref(g, cache->state.d, "fd", 0);
    } else
        exit(3);
    fputc('}', out);
}

static void emit_graph(path_fixture *f) {
    graph_writer g = {.fixture = f};
    FILE *out = f->output;
    fputs("{\"roots\":{\"world\":", out);
    graph_ref(&g, f->base.world, "fd", 0);
    fputs(",\"provider_fallback\":", out);
    graph_ref(&g, &f->base.world->fallback, "li", 0);
    fputs(",\"actor\":", out);
    graph_ref(&g, f->source, "bx", 0);
    fputs(",\"target\":", out);
    graph_ref(&g, f->target, "bx", 0);
    fputs(",\"cache\":", out);
    graph_ref(&g, f->cache, "ew", 0);
    fputs(",\"finder\":", out);
    const bool component =
        strcmp(f->input.operation, "FW_coordinates") && strncmp(f->input.operation, "FW_", 3) &&
        strncmp(f->input.operation, "EW_", 3) && strncmp(f->input.operation, "FD_", 3);
    graph_ref(&g, component ? NULL : f->finder, "fw", 0);
    fputs(",\"result\":", out);
    graph_result(&g);
    fputs(",\"extra\":", out);
    graph_ref(&g, f->extra_identity, "[Ljava.lang.Object;", f->extra_count);
    fputs("},\"objects\":[", out);
    for (size_t index = 0; index < g.count; ++index) {
        const graph_object object = g.objects[index];
        fprintf(out, "%s{\"class\":\"%s\",\"identity\":\"%016" PRIxPTR "\"", index ? "," : "",
                object.class_name, (uintptr_t)object.identity);
        graph_fields(&g, object);
        fputc('}', out);
    }
    fputs("]}", out);
    free(g.objects);
}

static void bounds_json(FILE *out, const mc_entity_bounds *b) {
    if (!b) {
        fputs("null", out);
        return;
    }
    fprintf(out,
            "{\"eq.a\":{\"D\":\"%016" PRIx64 "\"},\"eq.b\":{\"D\":\"%016" PRIx64
            "\"},\"eq.c\":{\"D\":\"%016" PRIx64 "\"},\"eq.d\":{\"D\":\"%016" PRIx64
            "\"},\"eq.e\":{\"D\":\"%016" PRIx64 "\"},\"eq.f\":{\"D\":\"%016" PRIx64 "\"}}",
            double_bits(b->min_x), double_bits(b->min_y), double_bits(b->min_z),
            double_bits(b->max_x), double_bits(b->max_y), double_bits(b->max_z));
}

static void relevant_entity(FILE *out, const mc_entity *entity) {
    if (!entity) {
        fputs("null", out);
        return;
    }
    fprintf(out,
            "{\"sn.aM\":{\"D\":\"%016" PRIx64 "\"},\"sn.aN\":{\"D\":\"%016" PRIx64
            "\"},\"sn.aO\":{\"D\":\"%016" PRIx64 "\"},\"sn.bg\":{\"F\":\"%08" PRIx32
            "\"},\"sn.bh\":{\"F\":\"%08" PRIx32 "\"}",
            double_bits(entity->base.x), double_bits(entity->base.y), double_bits(entity->base.z),
            float_bits(entity->base.width), float_bits(entity->base.height));
    const void *references[] = {entity->base.world, entity->bounds, entity->random,
                                entity->watcher};
    const char *names[] = {"sn.aI", "sn.aW", "sn.bs", "sn.bD"};
    const char *classes[] = {"fd", "eq", "java.util.Random", "ud"};
    for (size_t index = 0; index < COUNT(references); ++index) {
        fprintf(out, ",\"%s\":", names[index]);
        if (references[index])
            fprintf(out, "{\"class\":\"%s\",\"identity\":\"%016" PRIxPTR "\"}", classes[index],
                    (uintptr_t)references[index]);
        else
            fputs("null", out);
    }
    fputc('}', out);
}

static bool observe_state(path_fixture *f, const char *phase) {
    mc_entity_class_snapshot classes;
    mc_entity_vector_snapshot vectors;
    CHECK(mc_entity_classes_get_snapshot(f->base.entities, &classes) == MC_ENTITY_OK &&
          mc_entity_vector_get_snapshot(f->base.entities, &vectors) == MC_ENTITY_OK);
    fprintf(f->output,
            "{\"event\":\"path_state\",\"case\":%u,\"phase\":\"%s\","
            "\"operation\":\"%s\",\"scene\":\"%s\",\"actor_fields\":",
            f->input.id, phase, f->input.operation, f->input.scene);
    relevant_entity(f->output, f->source);
    fputs(",\"actor_bounds\":", f->output);
    bounds_json(f->output, f->source->bounds);
    fputs(",\"target_fields\":", f->output);
    relevant_entity(f->output, f->target);
    fputs(",\"target_bounds\":", f->output);
    bounds_json(f->output, f->target ? f->target->bounds : NULL);
    fprintf(f->output,
            ",\"own_seed\":\"%012" PRIx64 "\",\"target_seed\":", f->source->random->state);
    if (f->target)
        fprintf(f->output, "\"%012" PRIx64 "\"", f->target->random->state);
    else
        fputs("\"-\"", f->output);
    fprintf(f->output,
            ",\"world_seed\":\"%012" PRIx64 "\",\"math_seed\":\"%012" PRIx64
            "\",\"box_cursor\":%d,\"vector_cursor\":%d,\"loaded_entities\":%zu,"
            "\"cache_size\":%zu,\"graph\":",
            f->base.world->fields.r->state, atomic_load(&f->base.profile->math->state),
            classes.bounds_pool_cursor, vectors.cursor, f->base.world->fields.b->count,
            f->base.provider.cache->count);
    emit_graph(f);
    fputs("}\n", f->output);
    CHECK(fflush(f->output) == 0);
    return true;
}

static bool step_graph(path_fixture *f, const char *name) {
    fprintf(f->output,
            "{\"event\":\"path_step\",\"case\":%u,\"step\":\"%s\",\"graph\":", f->input.id, name);
    emit_graph(f);
    fprintf(f->output, ",\"box_cursor\":%d,\"vector_cursor\":%d}\n",
            f->base.entities->bounds_cursor, f->base.entities->vector_cursor);
    CHECK(fflush(f->output) == 0);
    return true;
}

static bool entered_finder(path_fixture *f, bool cache) {
    if (cache)
        CHECK(mc_client_chunk_cache_create(f->base.world, -16, -16, -16, 32, 96, 32, &f->cache,
                                           NULL) == MC_CLIENT_CHUNK_CACHE_OK);
    f->budget = (mc_path_budget){.max_bytes = SIZE_MAX, .max_allocations = SIZE_MAX};
    mc_path_error error;
    CHECK(mc_path_finder_create(f->base.entities, f->base.registry, f->base.math,
                                f->access_null ? NULL : f->cache, NULL, &f->budget, &f->finder,
                                &error) == MC_PATH_OK &&
          f->finder);
    return true;
}

static bool new_points(path_fixture *f, size_t count) {
    CHECK(count <= INT32_MAX && f->finder);
    f->component_points = pf_alloc(f->finder, count * sizeof(*f->component_points), true);
    CHECK(f->component_points);
    f->component_count = count;
    return true;
}

static mc_path_point *new_point(path_fixture *f, int32_t x, int32_t y, int32_t z) {
    mc_path_point *node = pf_point_new(f->finder);
    if (node)
        pf_point_construct(f->finder, node, x, y, z);
    return node;
}

static void point_result(path_fixture *f, const mc_path_point *node) {
    f->result_kind = PATH_VALUE_REFERENCE;
    f->result = node;
    f->result_class = "d";
}

static void path_result(path_fixture *f, const mc_path *path) {
    f->result_kind = PATH_VALUE_REFERENCE;
    f->result = path;
    f->result_class = "dh";
}

static bool component_run(path_fixture *f, bool *returned) {
    const char *operation = f->input.operation;
    const bool cache = !strncmp(operation, "FW_", 3) || !strncmp(operation, "EW_", 3);
    CHECK(entered_finder(f, cache));
    mc_path_finder *w = f->finder;
    const int32_t x = (int32_t)f->input.target[0], y = (int32_t)f->input.target[1],
                  z = (int32_t)f->input.target[2];
    *returned = true;
    if (!strcmp(operation, "FW_coordinates")) {
        *returned = mc_path_finder_to_coordinates(w, f->source_argument, x, y, z,
                                                  word_float(f->input.range)) == MC_PATH_OK;
        if (*returned)
            path_result(f, w->state.result);
    } else if (!strcmp(operation, "EW_block") || !strcmp(operation, "EW_metadata")) {
        int32_t value = INT32_MIN;
        mc_client_chunk_cache_error error;
        const mc_client_chunk_cache_status status =
            !strcmp(operation, "EW_block")
                ? mc_client_chunk_cache_block(f->cache, x, y, z, &value, &error)
                : mc_client_chunk_cache_metadata(f->cache, x, y, z, &value, &error);
        f->has_direct_query = true;
        f->direct_query = (mc_path_query){
            .metadata = !strcmp(operation, "EW_metadata"),
            .completed = status == MC_CLIENT_CHUNK_CACHE_OK,
            .x = x,
            .y = y,
            .z = z,
            .value = value,
            .status = status,
            .site = {"ew", !strcmp(operation, "EW_metadata") ? "e" : "a", "(III)I", 0}};
        *returned = status == MC_CLIENT_CHUNK_CACHE_OK;
        if (*returned) {
            f->result_kind = PATH_VALUE_INT;
            f->result_integer = value;
        } else {
            const char *exception = error.status == MC_CLIENT_CHUNK_CACHE_SOURCE_INDEX
                                        ? "java/lang/ArrayIndexOutOfBoundsException"
                                    : error.status == MC_CLIENT_CHUNK_CACHE_SOURCE_NULL
                                        ? "java/lang/NullPointerException"
                                    : error.status == MC_CLIENT_CHUNK_CACHE_SOURCE_NEGATIVE_ARRAY
                                        ? "java/lang/NegativeArraySizeException"
                                        : NULL;
            f->component_error.status = exception ? MC_PATH_SOURCE_EXCEPTION : MC_PATH_CHILD_FAILED;
            f->component_error.source_exception = exception != NULL;
            f->component_error.native_policy = exception == NULL;
            f->component_error.exception_class = exception;
            f->component_error.site =
                (mc_path_site){"ew", error.method, error.descriptor, (int32_t)error.bytecode};
        }
    } else if (!strcmp(operation, "FW_clearance") || !strcmp(operation, "FW_neighbor")) {
        mc_path_point *extent = new_point(f, f->extent[0], f->extent[1], f->extent[2]);
        CHECK(extent);
        f->extra[0].value = extent;
        f->extra[0].class_name = "d";
        f->extra_count = 1;
        CHECK(replace_holder(f));
        if (!strcmp(operation, "FW_clearance")) {
            f->result_kind = PATH_VALUE_INT;
            *returned = pf_clearance(w, x, y, z, extent, &f->result_integer);
        } else {
            mc_path_point *node = NULL;
            *returned = pf_candidate(w, x, y, z, extent, f->step, &node);
            if (*returned)
                point_result(f, node);
        }
    } else if (!strcmp(operation, "FW_intern") || !strcmp(operation, "D_distance") ||
               !strcmp(operation, "D_equals")) {
        CHECK(f->points_stored && new_points(f, 2));
        for (size_t i = 0; i < 2; ++i) {
            const int32_t *p = f->point_inputs[i];
            f->component_points[i] = !strcmp(operation, "FW_intern")
                                         ? pf_intern(w, p[0], p[1], p[2])
                                         : new_point(f, p[0], p[1], p[2]);
            CHECK(f->component_points[i]);
            f->extra[i].value = f->component_points[i];
            f->extra[i].class_name = "d";
        }
        f->extra_count = 2;
        CHECK(replace_holder(f));
        if (!strcmp(operation, "D_distance")) {
            f->result_kind = PATH_VALUE_FLOAT;
            *returned =
                pf_distance(w, f->component_points[0], f->component_points[1], &f->result_float);
        } else {
            f->result_kind = PATH_VALUE_BOOL;
            f->result_boolean =
                !strcmp(operation, "D_equals")
                    ? pf_point_equals(f->component_points[0], f->component_points[1])
                    : f->component_points[0] == f->component_points[1];
        }
    } else if (!strcmp(operation, "LF_sequence") || !strcmp(operation, "JX_sequence")) {
        const bool heap = !strcmp(operation, "LF_sequence");
        const size_t count = !heap                                        ? 13u
                             : !strcmp(f->input.scene, "heap_growth")     ? 1025u
                             : !strcmp(f->input.scene, "duplicate_point") ? 1u
                                                                          : 4u;
        CHECK(new_points(f, count));
        f->extra[0].value = heap ? (const void *)w->b : (const void *)w->c;
        f->extra[0].class_name = heap ? "lf" : "jx";
        f->extra[1].value = f->component_points;
        f->extra[1].class_name = "[Ld;";
        f->extra[1].length = count;
        f->extra_count = 2;
        CHECK(replace_holder(f));
        CHECK(step_graph(f, "constructed"));
        for (size_t i = 0; i < count; ++i) {
            mc_path_point *node = f->component_points[i] = new_point(f, (int32_t)i, 64, 0);
            CHECK(node);
            if (heap) {
                node->g = count == 1025 ? (float)(i % 17u) : 1.f;
                CHECK(pf_heap_add(w, node));
            } else
                CHECK(pf_map_put(w, node->j, node));
        }
        CHECK(step_graph(f, "inserted"));
        if (!strcmp(f->input.scene, "duplicate_point"))
            *returned = pf_heap_add(w, f->component_points[0]);
        else if (!strcmp(f->input.scene, "heap_ties")) {
            fprintf(f->output,
                    "{\"event\":\"path_step\",\"case\":%u,\"step\":\"popped\",\"values\":[",
                    f->input.id);
            graph_writer g = {.fixture = f};
            size_t popped = 0;
            while (w->b->b != 0) {
                mc_path_point *node = NULL;
                CHECK(pf_heap_pop(w, &node) && node);
                const size_t tie_order[] = {0, 3, 2, 1};
                CHECK(popped < COUNT(tie_order) && node == f->component_points[tie_order[popped]]);
                if (popped++)
                    fputc(',', f->output);
                graph_point(&g, node);
            }
            fputs("]}\n", f->output);
            free(g.objects);
        } else if (!strcmp(f->input.scene, "map_clear"))
            CHECK(pf_map_clear(w) && step_graph(f, "cleared"));
    } else if (!strcmp(operation, "DH_sequence")) {
        if (!strcmp(f->input.scene, "null_array")) {
            *returned = pf_path_construct(w, NULL, 0);
            CHECK(!*returned && w->state.pending_path && !w->state.pending_path->b &&
                  w->state.pending_path->constructor_pc == 6);
        } else {
            const size_t count = !strcmp(f->input.scene, "empty_last") ? 0 : 2;
            CHECK(new_points(f, count));
            if (count) {
                f->component_points[0] = new_point(f, 1, 2, 3);
                f->component_points[1] = new_point(f, 4, 5, 6);
                CHECK(f->component_points[0] && f->component_points[1]);
            }
            CHECK(pf_path_construct(w, f->component_points, count));
            mc_path *path = w->state.result;
            f->extra[0].value = f->component_points;
            f->extra[0].class_name = "[Ld;";
            f->extra[0].length = count;
            f->extra[1].value = f->extra[2].value = path;
            f->extra[1].class_name = f->extra[2].class_name = "dh";
            f->extra_count = 3;
            CHECK(replace_holder(f));
            if (!strcmp(f->input.scene, "empty_last")) {
                const mc_path_point *last = NULL;
                CHECK(mc_path_last(path, &last, &f->component_error) == MC_PATH_OK && !last);
                point_result(f, last);
            } else if (!strcmp(f->input.scene, "null_entity") ||
                       !strcmp(f->input.scene, "bad_index_null_entity")) {
                if (!strcmp(f->input.scene, "bad_index_null_entity"))
                    CHECK(mc_path_advance(path, NULL) == MC_PATH_OK &&
                          mc_path_advance(path, NULL) == MC_PATH_OK);
                mc_entity_vector *vector = NULL;
                *returned =
                    mc_path_waypoint(path, NULL, &vector, &f->component_error) == MC_PATH_OK;
                CHECK(!*returned && !vector);
            } else {
                path_result(f, path);
                CHECK(step_graph(f, "initial"));
                for (size_t i = 0; i < 2; ++i) {
                    mc_entity_vector *vector = NULL;
                    CHECK(mc_path_waypoint(path, f->source, &vector, &f->component_error) ==
                              MC_PATH_OK &&
                          vector);
                    CHECK(double_bits(vector->x) == double_bits(i ? 4.5 : 1.5) &&
                          double_bits(vector->y) == double_bits(i ? 5. : 2.) &&
                          double_bits(vector->z) == double_bits(i ? 6.5 : 3.5));
                    fprintf(f->output,
                            "{\"event\":\"path_step\",\"case\":%u,\"step\":\"waypoint%zu\","
                            "\"value\":{\"bt.a\":{\"D\":\"%016" PRIx64
                            "\"},\"bt.b\":{\"D\":\"%016" PRIx64 "\"},\"bt.c\":{\"D\":\"%016" PRIx64
                            "\"}}}\n",
                            f->input.id, i, double_bits(vector->x), double_bits(vector->y),
                            double_bits(vector->z));
                    CHECK(mc_entity_vector_release(f->base.entities, vector) == MC_ENTITY_OK &&
                          mc_path_advance(path, NULL) == MC_PATH_OK &&
                          step_graph(f, i ? "advanced1" : "advanced0"));
                }
                bool finished = false;
                CHECK(mc_path_finished(path, &finished, &f->component_error) == MC_PATH_OK &&
                      finished);
                fprintf(
                    f->output,
                    "{\"event\":\"path_step\",\"case\":%u,\"step\":\"finished\",\"value\":true}\n",
                    f->input.id);
                CHECK(mc_path_advance(path, NULL) == MC_PATH_OK && step_graph(f, "past_end"));
            }
        }
    } else {
        CHECK(false);
        return false;
    }
    if (!*returned && f->component_error.status == MC_PATH_OK)
        f->component_error = w->error.path;
    if (!*returned)
        f->result_kind = PATH_VALUE_NULL;
    return true;
}

static void site_json(FILE *out, const mc_path_site *site) {
    fprintf(out, "{\"class\":\"%s\",\"method\":\"%s\",\"descriptor\":\"%s\",\"pc\":%d}",
            site->class_name ? site->class_name : "-", site->method ? site->method : "-",
            site->descriptor ? site->descriptor : "-", site->pc);
}

static bool finite_oracles(path_fixture *f) {
    const unsigned int id = f->input.id;
    if (id >= 25 && id <= 34) {
        const int32_t classification[] = {0, 1, 0, 1, -1, -2, 1, 0, 1, 1};
        CHECK(f->result_kind == PATH_VALUE_INT && f->result_integer == classification[id - 25]);
    }
    if (id >= 35 && id <= 43) {
        const int32_t height[] = {65, 63, 62, 61, -1, 1, 0, 64, -1};
        const mc_path_point *node = f->result;
        CHECK(f->result_kind == PATH_VALUE_REFERENCE &&
              (height[id - 35] < 0
                   ? node == NULL
                   : node && node->a == 9 && node->b == height[id - 35] && node->c == 8));
    }
    if (id == 44)
        CHECK(f->result_kind == PATH_VALUE_INT && f->result_integer == 0);
    if (id >= 46 && id <= 49)
        CHECK(f->result_kind == PATH_VALUE_BOOL && f->result_boolean &&
              f->component_points[0] == f->component_points[1]);
    if (id == 50)
        CHECK(f->result_kind == PATH_VALUE_FLOAT &&
              float_bits(f->result_float) == UINT32_C(0x3fddb3d7));
    if (id == 62)
        CHECK(f->result_kind == PATH_VALUE_BOOL && !f->result_boolean &&
              f->component_points[0] != f->component_points[1] &&
              f->component_points[0]->j == f->component_points[1]->j);
    if (id == 51) {
        CHECK(f->finder->b->b == 0 && f->finder->b->capacity == 1024);
        for (size_t index = 0; index < 4; ++index)
            CHECK(f->component_points[index]->d == -1 && !f->finder->b->a[index]);
    }
    if (id == 52)
        CHECK(f->finder->b->b == 1025 && f->finder->b->capacity == 2048);
    if (id == 53 || id == 54)
        CHECK(f->finder->c->capacity == 32 && f->finder->c->c == 24 &&
              f->finder->c->b == (id == 53 ? 13 : 0) && f->finder->c->e == (id == 53 ? 13 : 14));
    if (id == 55)
        CHECK(f->finder->state.result->c == 3 &&
              f->finder->state.result->b == f->component_points &&
              f->extra[1].value == f->extra[2].value);
    if (id == 61)
        CHECK(!f->result && f->finder->state.result->a == 0 &&
              f->finder->state.result->array_length == 0);
    if ((id >= 5 && id <= 11) || id == 18 || id == 19 || id == 22)
        CHECK(f->result_kind == PATH_VALUE_REFERENCE && !f->result);
    if (id == 12 || id == 17 || id == 20 || id == 21 || id == 23 || id == 24) {
        const mc_path *path = f->result;
        CHECK(f->result_kind == PATH_VALUE_REFERENCE && path &&
              path->a == (id == 20 || id == 21 ? 1 : 5) && path->c == 0);
    }
    return true;
}

static bool finder_observations(path_fixture *f) {
    mc_path_finder_state state = {0};
    if (f->finder)
        CHECK(mc_path_finder_inspect(f->finder, &state) && state.query_count == state.cell_queries);
    const mc_path_query *queries = f->has_direct_query ? &f->direct_query : state.queries;
    const size_t query_count = f->has_direct_query ? 1u : state.query_count;
    fprintf(f->output, "{\"event\":\"path_queries\",\"case\":%u,\"queries\":[", f->input.id);
    for (size_t index = 0; index < query_count; ++index) {
        const mc_path_query *query = &queries[index];
        fprintf(
            f->output,
            "%s{\"metadata\":%s,\"x\":%d,\"y\":%d,\"z\":%d,\"completed\":%s,\"value\":%d,\"site\":",
            index ? "," : "", query->metadata ? "true" : "false", query->x, query->y, query->z,
            query->completed ? "true" : "false", query->value);
        site_json(f->output, &query->site);
        fputc('}', f->output);
    }
    fprintf(f->output,
            "],\"points\":%zu,\"expanded_points\":%zu,\"predecessor_steps\":%zu,\"work\":%zu,"
            "\"extent\":",
            state.points, state.expanded_points, state.predecessor_steps, state.work);
    if (state.extent) {
        mc_path_point_state extent;
        CHECK(mc_path_finder_point_inspect(f->finder, state.extent, &extent) &&
              extent.constructed && extent.constructor_pc == 36 && !extent.h);
        fprintf(f->output, "{\"class\":\"d\",\"identity\":\"%016" PRIxPTR "\"",
                (uintptr_t)state.extent);
        /* This separate actual-node observation is not a fabricated FW field. */
        graph_writer g = {.fixture = f};
        graph_fields(&g, (graph_object){state.extent, "d", 0});
        fputc('}', f->output);
        free(g.objects);
    } else
        fputs("null", f->output);
    fputs(",\"pending_path\":", f->output);
    if (state.pending_path) {
        mc_path_state pending;
        CHECK(mc_path_inspect(state.pending_path, &pending) && !pending.constructed &&
              pending.constructor_pc == 6 && pending.a == 0 && pending.c == 0 && !pending.b);
        fprintf(f->output, "{\"class\":\"dh\",\"identity\":\"%016" PRIxPTR "\"",
                (uintptr_t)state.pending_path);
        graph_writer g = {.fixture = f};
        graph_fields(&g, (graph_object){state.pending_path, "dh", 0});
        fputc('}', f->output);
        free(g.objects);
    } else
        fputs("null", f->output);
    fputs("}\n", f->output);
    if (!f->finder)
        return true;
    if (f->finder->b) {
        CHECK(f->finder->b->b >= 0 && (size_t)f->finder->b->b <= f->finder->b->capacity);
        for (int32_t index = 0; index < f->finder->b->b; ++index)
            CHECK(f->finder->b->a[index] && f->finder->b->a[index]->d == index);
    }
    if (f->finder->c) {
        size_t entries = 0;
        for (size_t bucket = 0; bucket < f->finder->c->capacity; ++bucket)
            for (const mc_path_map_entry *entry = f->finder->c->a[bucket]; entry;
                 entry = entry->c) {
                CHECK(entry->b && entry->a == entry->b->j && ++entries <= (size_t)INT32_MAX);
            }
        CHECK(entries == (size_t)f->finder->c->b);
    }
    if (f->result_kind == PATH_VALUE_REFERENCE && f->result && !strcmp(f->result_class, "dh")) {
        const mc_path *path = f->result;
        mc_path_state path_state;
        CHECK(mc_path_inspect(path, &path_state) && path_state.constructed &&
              path_state.constructor_pc == 12 && path_state.a >= 0 &&
              (size_t)path_state.a == path_state.array_length &&
              path_state.classes == f->base.entities);
        for (size_t index = 0; index < path_state.array_length; ++index) {
            mc_path_point_state node;
            CHECK(mc_path_point_inspect(path, path_state.b[index], &node) && node.constructed);
            if (!strcmp(f->input.operation, "FD_coordinates") ||
                !strcmp(f->input.operation, "FD_entity") ||
                !strcmp(f->input.operation, "FW_coordinates"))
                CHECK(node.h == (index ? path_state.b[index - 1] : NULL));
        }
        if (f->input.id <= 4) {
            /* Finite authored terrain follows the pinned source radius and
             * +Z/-X/+X/-Z search order, including closest partial return. */
            const size_t lengths[] = {1, 5, 9, 11, 4};
            CHECK(path_state.array_length == lengths[f->input.id] && path_state.c == 0);
        }
    }
    return true;
}

static bool run_path(path_fixture *f) {
    const mc_entity source = *f->source;
    const mc_mob_entity_state source_mob = *f->source->mob;
    const mc_entity target = f->target ? *f->target : (mc_entity){0};
    const mc_mob_entity_state target_mob = f->target ? *f->target->mob : (mc_mob_entity_state){0};
    const mc_java_random own = *f->source->random, world = *f->base.world->fields.r;
    const mc_java_random other = f->target ? *f->target->random : (mc_java_random){0};
    const uint64_t shared_u = atomic_load(&f->base.profile->uniquifier);
    const uint64_t shared_math = atomic_load(&f->base.profile->math->state);
    const size_t source_holds = f->source->holds, target_holds = f->target ? f->target->holds : 0;
    f->call_bindings = f->base.world->persistent_bindings;
    f->call_source_holds = source_holds;
    f->call_target_holds = target_holds;
    CHECK(observe_state(f, "before"));
    bool returned = false;
    mc_world_path_status fd_status = MC_WORLD_PATH_OK;
    const mc_world_path_error *fd_error = NULL;
    if (!strncmp(f->input.operation, "FD_", 3)) {
        if (!strcmp(f->input.operation, "FD_entity"))
            fd_status = mc_world_runtime_path_to_entity_begin(
                f->base.world, f->source_argument, f->target_argument, word_float(f->input.range),
                f->base.math, NULL, &f->call);
        else {
            CHECK(f->input.target[0] >= INT32_MIN && f->input.target[0] <= INT32_MAX &&
                  f->input.target[1] >= INT32_MIN && f->input.target[1] <= INT32_MAX &&
                  f->input.target[2] >= INT32_MIN && f->input.target[2] <= INT32_MAX);
            fd_status = mc_world_runtime_path_to_coordinates_begin(
                f->base.world, f->source_argument, (int32_t)f->input.target[0],
                (int32_t)f->input.target[1], (int32_t)f->input.target[2],
                word_float(f->input.range), f->base.math, NULL, &f->call);
        }
        mc_world_path_call_state state;
        CHECK(f->call && mc_world_path_call_inspect(f->call, &state) && !state.active &&
              !state.retiring && state.world == f->base.world);
        returned = state.source_returned;
        CHECK((fd_status == MC_WORLD_PATH_OK) == returned && state.failure);
        fd_error = state.failure;
        f->cache = state.cache;
        f->finder = (mc_path_finder *)state.finder;
        if (returned)
            path_result(f, state.result);
        CHECK(f->source->holds == source_holds + (f->source_argument ? 1u : 0u));
        if (f->target)
            CHECK(f->target->holds == target_holds + (f->target_argument ? 1u : 0u));
        /* Completed requests retain storage through independent FD/EW native
         * occurrences. retire_world also admits a separate collection/phase
         * contract, so it is not a passive lifetime probe here. */
        CHECK(f->base.world->persistent_bindings == f->call_bindings + 1u + (f->cache ? 1u : 0u));
    } else
        CHECK(component_run(f, &returned));
    CHECK(finite_oracles(f) && observe_state(f, "after") && finder_observations(f) &&
          failed_path_inputs(f));
    /* The complete actual source Entity storage is unchanged. Native request
     * occurrences are independently accounted, and constructor errors survive. */
    CHECK(!memcmp(&source.base, &f->source->base, sizeof(source.base)) &&
          !memcmp(&source_mob, f->source->mob, sizeof(source_mob)) &&
          (!f->target || (!memcmp(&target.base, &f->target->base, sizeof(target.base)) &&
                          !memcmp(&target_mob, f->target->mob, sizeof(target_mob)))) &&
          mc_test_java_random_equal(&own, f->source->random) &&
          mc_test_java_random_equal(&world, f->base.world->fields.r) &&
          (!f->target || mc_test_java_random_equal(&other, f->target->random)) &&
          atomic_load(&f->base.profile->uniquifier) == shared_u &&
          atomic_load(&f->base.profile->math->state) == shared_math);
    const mc_path_error *component = &f->component_error;
    fprintf(f->output,
            "{\"event\":\"path_result\",\"case\":%u,\"outcome\":%d,\"source_returned\":%s,"
            "\"status\":%u,\"source_exception\":%s,\"native_policy\":%s,\"exception_class\":\"%s\","
            "\"site\":",
            f->input.id, returned ? 1 : -1, returned ? "true" : "false",
            fd_error ? (unsigned int)fd_status : (unsigned int)component->status,
            (fd_error ? fd_error->source_exception : component->source_exception) ? "true"
                                                                                  : "false",
            (fd_error ? fd_error->native_policy : component->native_policy) ? "true" : "false",
            fd_error ? (fd_error->exception_class ? fd_error->exception_class : "-")
                     : (component->exception_class ? component->exception_class : "-"));
    site_json(f->output, fd_error ? &fd_error->site : &component->site);
    fputs(",\"child\":", f->output);
    site_json(f->output, fd_error ? &fd_error->child : &component->child_site);
    fputs("}\n", f->output);
    CHECK(fflush(f->output) == 0);
    return true;
}

static bool limit_control(path_fixture *f, size_t control) {
    mc_world_path_limits limits = mc_world_path_default_limits();
    const char *names[] = {"cache_cells", "bytes",        "allocations",  "points",
                           "expanded",    "cell_queries", "predecessors", "work"};
    CHECK(control < COUNT(names));
    switch (control) {
        case 0:
            limits.max_cache_cells = 0;
            break;
        case 1:
            limits.max_native_bytes = 0;
            break;
        case 2:
            limits.max_allocations = 0;
            break;
        case 3:
            limits.max_points = 0;
            break;
        case 4:
            limits.max_expanded_points = 0;
            break;
        case 5:
            limits.max_cell_queries = 0;
            break;
        case 6:
            limits.max_predecessor_steps = 0;
            break;
        case 7:
            limits.max_work = 0;
            break;
        default:
            return false;
    }
    const mc_entity_base_fields fields = f->source->base;
    const mc_mob_entity_state mob = *f->source->mob;
    const mc_java_random own = *f->source->random, world = *f->base.world->fields.r;
    const size_t holds = f->source->holds, bindings = f->base.world->persistent_bindings;
    const size_t borrows = f->base.registry->runtime_borrows;
    const uint64_t shared_u = atomic_load(&f->base.profile->uniquifier);
    const uint64_t shared_math = atomic_load(&f->base.profile->math->state);
    mc_entity_class_snapshot before, after;
    mc_math_class_state math_before, math_after;
    CHECK(mc_entity_classes_get_snapshot(f->base.entities, &before) == MC_ENTITY_OK &&
          mc_math_classes_state(f->base.math, &math_before, NULL) == MC_MATH_CLASS_OK);
    const mc_world_path_status status = mc_world_runtime_path_to_coordinates_begin(
        f->base.world, f->source, 12, 64, 8, 16.f, f->base.math, &limits, &f->call);
    CHECK(status == MC_WORLD_PATH_LIMIT);
    fprintf(f->output,
            "{\"event\":\"path_control\",\"case\":%u,\"scene\":\"limit_%s\","
            "\"call_published\":%s,\"site\":",
            f->input.id, names[control], f->call ? "true" : "false");
    if (f->call) {
        mc_world_path_call_state state;
        CHECK(mc_world_path_call_inspect(f->call, &state) && !state.active &&
              !state.source_returned && state.failure &&
              state.failure->status == MC_WORLD_PATH_LIMIT && state.failure->native_policy &&
              !state.failure->source_exception && f->source->holds == holds + 1);
        site_json(f->output, &state.failure->site);
    } else {
        CHECK(control == 1 || control == 2);
        fputs("null", f->output);
    }
    CHECK(!memcmp(&fields, &f->source->base, sizeof(fields)) &&
          !memcmp(&mob, f->source->mob, sizeof(mob)) &&
          mc_test_java_random_equal(&own, f->source->random) &&
          mc_test_java_random_equal(&world, f->base.world->fields.r));
    CHECK(mc_world_path_call_retire(&f->call) == MC_WORLD_PATH_OK && !f->call &&
          f->source->holds == holds && f->base.world->persistent_bindings == bindings &&
          f->base.world->active_calls == 0 && f->base.world->local_leases == 0 &&
          f->base.registry->runtime_borrows == borrows &&
          mc_entity_classes_get_snapshot(f->base.entities, &after) == MC_ENTITY_OK &&
          after.live_path_arenas == before.live_path_arenas &&
          after.allocation_bytes == before.allocation_bytes &&
          mc_math_classes_state(f->base.math, &math_after, NULL) == MC_MATH_CLASS_OK &&
          math_after.holds == math_before.holds &&
          atomic_load(&f->base.profile->uniquifier) == shared_u &&
          atomic_load(&f->base.profile->math->state) == shared_math &&
          !memcmp(&fields, &f->source->base, sizeof(fields)) &&
          !memcmp(&mob, f->source->mob, sizeof(mob)));
    fputs(",\"source_unchanged\":true,\"retired_without_replay\":true}\n", f->output);
    return true;
}

static bool returned_path(path_fixture *f, mc_path **out) {
    CHECK(!f->call && !*out);
    CHECK(mc_world_runtime_path_to_coordinates_begin(f->base.world, f->source, 12, 64, 8, 16.f,
                                                     f->base.math, NULL,
                                                     &f->call) == MC_WORLD_PATH_OK);
    mc_world_path_call_state state;
    CHECK(mc_world_path_call_inspect(f->call, &state) && state.source_returned && state.result &&
          mc_path_retain(state.result, NULL) == MC_PATH_OK);
    *out = state.result;
    CHECK(mc_world_path_call_retire(&f->call) == MC_WORLD_PATH_OK && !f->call &&
          f->base.world->local_leases == 0 && f->base.world->active_calls == 0);
    mc_path_state path;
    CHECK(mc_path_inspect(*out, &path) && path.holds == 1 && path.arena_holds == 1 && path.a == 5);
    return true;
}

static bool close_world_keep_entity_namespace(path_fixture *f) {
    random_fixture *b = &f->base;
    CHECK(!f->source && !f->target && !f->call && !f->finder && !f->cache);
    CHECK(mc_world_runtime_retire_entity_containers(b->world, NULL) == MC_WORLD_STORAGE_OK &&
          mc_chunk_pending_clear(b->provider.pending) == MC_CHUNK_ACQUIRE_OK);
    mc_chunk_cache *cache = b->provider.cache;
    CHECK(mc_chunk_cache_init(cache, cache->entries, cache->capacity, cache->insertions,
                              cache->insertion_capacity));
    if (b->dimension)
        mc_world_provider_unbind(b->dimension);
    CHECK(mc_world_runtime_retire_world(b->world, NULL) == MC_WORLD_STORAGE_OK);
    for (size_t index = 0; index < b->owned_count; ++index)
        CHECK(mc_world_store_retire_chunk(b->store, b->owned[index], NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_free(b->world, NULL) == MC_WORLD_STORAGE_OK);
    b->world = NULL;
    CHECK(mc_player_session_classes_free(&b->sessions, NULL) == MC_PLAYER_OK);
    if (b->dimension)
        mc_world_provider_release(b->dimension);
    b->dimension = NULL;
    CHECK(mc_world_store_free_checked(&b->store, NULL) == MC_WORLD_STORAGE_OK);
    mc_world_regions_free(b->regions);
    b->regions = NULL;
    mc_world_info_name_release(b->name);
    b->name = NULL;
    CHECK(mc_world_runtime_classes_free(b->classes, NULL) == MC_WORLD_STORAGE_OK);
    b->classes = NULL;
    CHECK(mc_math_classes_release(&b->math, NULL) == MC_MATH_CLASS_OK &&
          mc_registry_classes_free(b->registry, NULL) == MC_REGISTRY_OK);
    b->registry = NULL;
    return true;
}

static bool path_ownership_control(path_fixture *f) {
    CHECK(f->target && f->source_owned && f->target_owned);
    /* End only the fixture's separate provider caller occurrence. World.t and
     * provider.a remain the genuine coupled pair used by strict admission. */
    CHECK(f->base.dimension == f->base.world->fields.t &&
          mc_world_provider_hold_count(f->base.dimension) == 2);
    mc_world_provider_release(f->base.dimension);
    f->base.dimension = NULL;
    mc_path *path = NULL;
    CHECK(returned_path(f, &path));
    mc_path_state state;
    CHECK(mc_path_inspect(path, &state) &&
          mc_mob_path_store(f->source, path, NULL) == MC_ENTITY_OK &&
          mc_mob_path_store(f->target, path, NULL) == MC_ENTITY_OK &&
          mc_path_inspect(path, &state) && state.holds == 3);
    CHECK(mc_mob_path_store(f->source, path, NULL) == MC_ENTITY_OK &&
          mc_mob_path_store(f->target, path, NULL) == MC_ENTITY_OK &&
          mc_path_inspect(path, &state) && state.holds == 3 &&
          mc_path_advance(path, NULL) == MC_PATH_OK);
    mc_path *source = NULL, *target = NULL;
    CHECK(mc_mob_path_get(f->source, &source, NULL) == MC_ENTITY_OK &&
          mc_mob_path_get(f->target, &target, NULL) == MC_ENTITY_OK && source == target &&
          source == path && mc_path_inspect(path, &state) && state.c == 1 &&
          mc_mob_path_store(f->source, NULL, NULL) == MC_ENTITY_OK &&
          mc_path_inspect(path, &state) && state.holds == 2 &&
          f->target->mob->creature_path == path &&
          mc_mob_path_store(f->source, path, NULL) == MC_ENTITY_OK);
    mc_path *other = NULL;
    CHECK(returned_path(f, &other));
    const size_t proper_holds = other->holds;
    other->holds = SIZE_MAX; /* Native occurrence-ceiling fixture input. */
    const mc_mob_entity_state preserved_mob = *f->source->mob;
    mc_path_error error;
    CHECK(mc_mob_path_store(f->source, other, &error) == MC_ENTITY_LIMIT &&
          error.status == MC_PATH_LIMIT && error.native_policy && !error.source_exception &&
          error.site.class_name && !strcmp(error.site.class_name, "ii") && error.site.method &&
          !strcmp(error.site.method, "a") && error.site.descriptor &&
          !strcmp(error.site.descriptor, "(Ldh;)V") && error.site.pc == 2 &&
          !memcmp(&preserved_mob, f->source->mob, sizeof(preserved_mob)) &&
          f->source->mob->creature_path == path && f->source->mob->creature_path_hold == path &&
          other->holds == SIZE_MAX && path->holds == 3 && path->c == 1);
    /* Restore only the caller's deliberately changed native ownership count
     * after proving the rejected store left both source and held cells intact. */
    other->holds = proper_holds;
    mc_path_release(other);
    mc_entity_release(f->source);
    mc_entity_release(f->target);
    f->source_owned = f->target_owned = false;
    mc_world_terminal_inventory inventory;
    mc_world_storage_error terminal;
    CHECK(mc_world_runtime_terminal_admit(f->base.world, NULL, &inventory, &terminal) ==
              MC_WORLD_STORAGE_BUSY &&
          terminal.operation && !strcmp(terminal.operation, "World terminal external Path hold") &&
          path->c == 1 && path->holds == 3);
    mc_path_release(path); /* Both actual Mob fields still own the same DH. */
    CHECK(mc_world_runtime_terminal_admit(f->base.world, NULL, &inventory, NULL) ==
              MC_WORLD_STORAGE_OK &&
          inventory.entities == 2 && inventory.paths == 1 && inventory.path_occurrences == 2 &&
          inventory.path_arenas == 1 && path->holds == 2 && path->c == 1);
    CHECK(mc_entity_retain(f->source) == MC_ENTITY_OK &&
          mc_entity_retain(f->target) == MC_ENTITY_OK);
    f->source_owned = f->target_owned = true;
    CHECK(mc_world_runtime_retire_entity_containers(f->base.world, NULL) == MC_WORLD_STORAGE_OK &&
          f->source->mob->creature_path == path && f->target->mob->creature_path == path &&
          path->holds == 2 && path->c == 1);
    CHECK(mc_path_retain(path, NULL) == MC_PATH_OK);
    mc_entity_release(f->source);
    mc_entity_release(f->target);
    f->source_owned = f->target_owned = false;
    f->source = f->target = NULL;
    CHECK(mc_path_inspect(path, &state) && state.holds == 1 && state.arena_holds == 1 &&
          state.c == 1 && close_world_keep_entity_namespace(f) && mc_path_inspect(path, &state) &&
          state.c == 1);
    mc_entity_class_snapshot before, after;
    CHECK(mc_entity_classes_get_snapshot(f->base.entities, &before) == MC_ENTITY_OK &&
          before.live_entities == 0 && before.live_path_arenas == 1 &&
          mc_entity_classes_free(f->base.entities) == MC_ENTITY_BUSY);
    const size_t bytes = state.arena_bytes;
    mc_path_release(path);
    CHECK(mc_entity_classes_get_snapshot(f->base.entities, &after) == MC_ENTITY_OK &&
          after.live_path_arenas == 0 && before.allocation_bytes >= bytes &&
          after.allocation_bytes == before.allocation_bytes - bytes &&
          mc_entity_classes_free(f->base.entities) == MC_ENTITY_OK);
    f->base.entities = NULL;
    mc_java_random_profile_free(f->base.profile);
    f->base.profile = NULL;
    mc_materials_destroy(f->base.materials);
    f->base.materials = NULL;
    CHECK(mc_resource_files_free(f->base.files) == MC_REGISTRY_OK);
    f->base.files = NULL;
    for (size_t index = 0; index < f->holder_count; ++index)
        free(f->holder_allocations[index]);
    f->holder_count = 0;
    f->control_done = true;
    fprintf(f->output,
            "{\"event\":\"path_control\",\"case\":%u,\"scene\":\"shared_path_lifetime\","
            "\"shared_cursor\":1,\"strict_paths\":1,\"strict_occurrences\":2,"
            "\"strict_arenas\":1,\"external_refusal\":true,\"failed_store_preserved\":true,"
            "\"broad_retirement_preserved\":true,\"outlived_World\":true,\"class_BUSY_until_"
            "release\":true}\n",
            f->input.id);
    return true;
}

int main(int argc, char **argv) {
    if (argc != 6)
        return 2;
    path_profile profiles[256] = {0};
    path_operand operands[512] = {0};
    size_t count = 0, operand_count = 0;
    if (!profile_table(argv[3], profiles, &count) ||
        !operand_table(argv[4], operands, &operand_count))
        return 2;
    FILE *output = open_file(argv[5], "wb");
    if (!output)
        return 2;
    for (size_t index = 0; index < count + 9; ++index) {
        path_fixture *f = calloc(1, sizeof(*f));
        if (!f)
            return 2;
        const bool native = index >= count;
        f->input = native ? (path_profile){.id = (unsigned int)index,
                                           .operation = "FD_coordinates",
                                           .scene = "native_limit",
                                           .source = {8.5, 64, 8.5},
                                           .target = {12, 64, 8},
                                           .range = UINT32_C(0x41800000),
                                           .width = UINT32_C(0x3f666666),
                                           .height = UINT32_C(0x3fa66666)}
                          : profiles[index];
        if (native && index - count == 8) {
            memcpy(f->input.operation, "FD_entity", sizeof("FD_entity"));
            f->input.target[0] = 12.5;
            f->input.target[2] = 8.5;
        }
        f->output = output;
        char directory[8192];
        const int length = snprintf(directory, sizeof(directory), "%s/case%zu", argv[2], index);
        const bool passed = length > 0 && (size_t)length < sizeof(directory) &&
                            prepare_path_fixture(f, argv[1], directory) &&
                            apply_operands(f, operands, operand_count) &&
                            (native ? (index - count == 8 ? path_ownership_control(f)
                                                          : limit_control(f, index - count))
                                    : run_path(f));
        if (!retire_path_fixture(f)) {
            fputs("Path cleanup refused; actual live dependencies retained\n", stderr);
            return 1;
        }
        free(f);
        if (!passed)
            return 1;
    }
    if (fclose(output))
        return 1;
    printf("World paths: %u checks;%zu profiles\n", checks, count);
    return 0;
}
