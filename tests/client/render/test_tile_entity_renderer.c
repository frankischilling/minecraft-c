#include "client/render/tile_entity_renderer/internal.h"
#include "core/java/object_identity_profile_internal.h"
#include "mc/block/material/material.h"
#include "mc/core/math/math.h"
#include "mc/world/runtime/construction.h"
#include "mc/world/runtime/entities.h"
#include "world/runtime/tile_entities/identity_internal.h"

#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Authored CPU observation of the actual LL owner and its canonical graph.
 * Reference modes take expected state/geometry from the public original fixture.
 * Native identity controls use explicitly authored cells, never JVM captures. */
static unsigned int checks, rows;
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "TileEntityRenderer:%d check%u: %s\n", __LINE__, checks, #condition);  \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

typedef struct fixture {
    mc_java_random_profile *profile;
    mc_materials *materials;
    mc_registry_classes *registry;
    mc_entity_classes *entities;
    mc_world_runtime_classes *worlds;
    mc_client_render_classes *models;
    mc_client_tile_renderer_classes *classes;
    mc_client_tile_renderer_dispatcher *manager;
    const mc_tile_entity_class_literal *literals[9];
    mc_client_tile_renderer_error error;
} fixture;

typedef struct identities {
    const void *values[128];
    size_t count;
} identities;

static const mc_tile_entity_class_kind literal_kinds[9] = {
    MC_TILE_ENTITY_CLASS_KIND_FURNACE,    MC_TILE_ENTITY_CLASS_KIND_CHEST,
    MC_TILE_ENTITY_CLASS_KIND_JUKEBOX,    MC_TILE_ENTITY_CLASS_KIND_DISPENSER,
    MC_TILE_ENTITY_CLASS_KIND_SIGN,       MC_TILE_ENTITY_CLASS_KIND_SPAWNER,
    MC_TILE_ENTITY_CLASS_KIND_NOTE_BLOCK, MC_TILE_ENTITY_CLASS_KIND_PISTON,
    MC_TILE_ENTITY_CLASS_KIND_TILE_ENTITY};

static void row(const char *format, ...) {
    ++rows;
    va_list args;
    va_start(args, format);
    (void)vprintf(format, args);
    va_end(args);
    (void)putchar('\n');
}

static uint32_t fb(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static uint64_t db(double value) {
    uint64_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static bool add_identity(identities *set, const void *value) {
    if (value == NULL || set->count == sizeof(set->values) / sizeof(set->values[0]))
        return false;
    for (size_t i = 0; i < set->count; ++i)
        if (set->values[i] == value)
            return false;
    set->values[set->count++] = value;
    return true;
}

/* These spellings map concrete canonical source storage types. They are not
 * expected field values, class-name-based replacement objects or a Map query. */
static const char *map_type(const tile_renderer_map *map) {
    return map == NULL ? "null" : "java.util.HashMap";
}

static const char *spawner_map_type(const tile_renderer_spawner_map *map) {
    return map == NULL ? "null" : "java.util.HashMap";
}

static const char *sign_type(const mc_client_sign_model *model) {
    return model == NULL ? "null" : "rf";
}

static const char *part_type(const mc_client_model_part *part) {
    return part == NULL ? "null" : "ps";
}

static bool renderer_name(mc_client_tile_renderer *renderer, const char **name) {
    if (renderer == NULL) {
        *name = "null";
        return true;
    }
    mc_client_tile_renderer_object_state state;
    CHECK(mc_client_tile_renderer_object_inspect(renderer, &state));
    CHECK(state.source_class != NULL && state.identity == renderer &&
          state.phase == MC_CLIENT_MODEL_READY);
    *name = state.source_class;
    return true;
}

static bool passive_classes(fixture *f) {
    mc_tile_entity_piston_class_snapshot state;
    CHECK(mc_tile_entity_piston_class_get_snapshot(f->worlds, &state) == MC_TILE_ENTITY_OK);
    CHECK(state.base_phase == MC_TILE_ENTITY_CLASS_COLD &&
          state.piston_phase == MC_TILE_ENTITY_CLASS_COLD && state.base_name_count == 0 &&
          state.base_class_count == 0 && state.list_identity == NULL && state.list_count == 0 &&
          state.live_tile_entities == 0);
    return true;
}

static bool fixture_open(fixture *f) {
    static mc_math_trig trig;
    CHECK(mc_java_random_profile_create(SIZE_MAX, &f->profile) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_materials_create(&f->materials));
    CHECK(mc_entity_classes_create(MC_ENTITY_CLIENT, f->profile, NULL, &f->entities) ==
          MC_ENTITY_OK);
    const mc_registry_options registry = {
        .side = MC_REGISTRY_CLIENT, .random_profile = f->profile, .materials = f->materials};
    /* The optional resource source is absent, never a fake successful loader.
     * This cold registry is genuinely allocated/bound; no initializer is run. */
    CHECK(mc_registry_classes_allocate(&registry, &f->registry, NULL) == MC_REGISTRY_OK);
    CHECK(mc_math_trig_init(&trig));
    CHECK(mc_world_runtime_classes_create(MC_WORLD_INFO_CLIENT, f->profile, &trig, &f->worlds,
                                          NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_classes_bind_registry(f->worlds, f->registry, NULL) == MC_REGISTRY_OK);
    CHECK(mc_world_runtime_classes_bind_entities(f->worlds, f->entities) == MC_ENTITY_OK);
    CHECK(mc_client_render_classes_create(f->entities, f->profile, &f->models) ==
          MC_CLIENT_MODEL_OK);
    CHECK(mc_client_tile_renderer_classes_bind(f->models, f->worlds, f->profile, NULL, &f->classes,
                                               &f->error) == MC_CLIENT_TILE_RENDERER_OK);
    mc_client_tile_renderer_state cold;
    CHECK(mc_client_tile_renderer_inspect(f->classes, &cold));
    CHECK(cold.phase == MC_CLIENT_TILE_RENDERER_COLD && cold.a == NULL && cold.candidate == NULL &&
          cold.entries == 0 && cold.fields.m == NULL);
    for (size_t i = 0; i < 9; ++i)
        CHECK(mc_tile_entity_class_literal_get(cold.tile_classes, literal_kinds[i],
                                               &f->literals[i]) == MC_TILE_ENTITY_OK);
    CHECK(passive_classes(f));
    return true;
}

static bool literal_rows(fixture *f) {
    for (size_t i = 0; i < 9; ++i) {
        mc_tile_entity_class_literal_view view, parent_view;
        CHECK(mc_tile_entity_class_literal_inspect(f->literals[i], &view) == MC_TILE_ENTITY_OK);
        CHECK(view.world_classes == f->worlds && view.side == MC_WORLD_INFO_CLIENT);
        const mc_tile_entity_class_literal *parent = NULL;
        const mc_tile_entity_status status =
            mc_tile_entity_class_literal_superclass(f->literals[i], &parent);
        const char *parent_name;
        if (view.kind == MC_TILE_ENTITY_CLASS_KIND_TILE_ENTITY) {
            /* The canonical root's declared Java superclass is Object; that
             * Class is explicitly outside this native TileEntity namespace.
             * INVALID_KIND preserves the output and creates no fake cell. */
            CHECK(status == MC_TILE_ENTITY_INVALID_KIND && parent == NULL);
            parent_name = view.superclass_name;
        } else {
            CHECK(status == MC_TILE_ENTITY_OK && parent != NULL);
            CHECK(mc_tile_entity_class_literal_inspect(parent, &parent_view) == MC_TILE_ENTITY_OK);
            parent_name = parent_view.client_name;
            CHECK(strcmp(parent_name, view.superclass_name) == 0);
        }
        row("TILE_LITERAL %s %s %d", view.client_name, parent_name, parent == f->literals[8]);
    }
    return true;
}

static bool map_rows(fixture *f, const char *stage) {
    mc_client_tile_renderer_state state;
    mc_client_tile_renderer_map_entry entries[9], root, null_key;
    const char *names[9];
    CHECK(mc_client_tile_renderer_inspect(f->classes, &state) && state.entries <= 9);
    CHECK(state.phase == MC_CLIENT_TILE_RENDERER_READY && state.a == f->manager);
    CHECK(mc_client_tile_renderer_map_find(f->manager, f->literals[8], &root, &f->error) ==
          MC_CLIENT_TILE_RENDERER_OK);
    CHECK(mc_client_tile_renderer_map_find(f->manager, NULL, &null_key, &f->error) ==
          MC_CLIENT_TILE_RENDERER_OK);
    for (size_t i = 0; i < state.entries; ++i) {
        mc_tile_entity_class_literal_view view;
        CHECK(mc_client_tile_renderer_entry(f->manager, i, &entries[i], &f->error) ==
              MC_CLIENT_TILE_RENDERER_OK);
        CHECK(entries[i].present && entries[i].identity != NULL);
        CHECK(mc_tile_entity_class_literal_inspect(entries[i].key, &view) == MC_TILE_ENTITY_OK);
        names[i] = view.client_name;
        if (entries[i].value != NULL) {
            mc_client_tile_renderer_object_state object;
            CHECK(mc_client_tile_renderer_object_inspect(entries[i].value, &object));
            CHECK(object.manager == f->manager);
        }
    }
    /* Only logical entries are sorted, matching the authored observer. The
     * admitted native Class hashes do not reproduce a JVM startup history. */
    for (size_t i = 1; i < state.entries; ++i)
        for (size_t j = i; j > 0 && strcmp(names[j - 1], names[j]) > 0; --j) {
            const char *name = names[j - 1];
            names[j - 1] = names[j];
            names[j] = name;
            const mc_client_tile_renderer_map_entry entry = entries[j - 1];
            entries[j - 1] = entries[j];
            entries[j] = entry;
        }
    row("TILE_MAP %s %zu %zu %zu %zu %d %d", stage, state.entries, state.capacity, state.threshold,
        state.null_values, root.present, null_key.present);
    for (size_t i = 0; i < state.entries; ++i) {
        const char *name;
        CHECK(renderer_name(entries[i].value, &name));
        row("TILE_ENTRY %s %s %s", stage, names[i], name);
    }
    return true;
}

static bool constructor_rows(fixture *f) {
    mc_client_tile_renderer_state state;
    mc_client_tile_renderer_object_state objects[3];
    identities renderers = {0};
    size_t backpointers = 0;
    const size_t keys[3] = {4, 5, 7};
    CHECK(mc_client_tile_renderer_inspect(f->classes, &state));
    CHECK(state.phase == MC_CLIENT_TILE_RENDERER_READY && state.fields.m == f->manager->map &&
          state.a == f->manager && state.candidate == f->manager);
    for (size_t i = 0; i < 3; ++i) {
        mc_client_tile_renderer_map_entry entry;
        mc_tile_entity_class_literal_view key;
        CHECK(mc_client_tile_renderer_map_find(f->manager, f->literals[keys[i]], &entry,
                                               &f->error) == MC_CLIENT_TILE_RENDERER_OK);
        CHECK(entry.present && entry.value != NULL && add_identity(&renderers, entry.value));
        CHECK(mc_client_tile_renderer_object_inspect(entry.value, &objects[i]));
        CHECK(mc_tile_entity_class_literal_inspect(entry.key, &key) == MC_TILE_ENTITY_OK);
        if (objects[i].manager == f->manager)
            ++backpointers;
        row("TILE_RENDERER %s %s %d", key.client_name, objects[i].source_class,
            objects[i].manager == f->manager);
    }
    CHECK(objects[1].spawner_map != NULL && objects[1].spawner_map != state.fields.m);
    row("TILE_CTOR %s %zu %d %zu %zu %s %zu %d", map_type(f->manager->map), state.entries,
        state.a == f->manager, renderers.count, backpointers,
        spawner_map_type(objects[1].spawner_map), objects[1].spawner_entries,
        objects[2].piston_blocks == NULL);
    row("TILE_DEFAULTS %d %d %d %d %08" PRIx32 " %08" PRIx32 " %016" PRIx64 " %016" PRIx64
        " %016" PRIx64 " %016" PRIx64 " %016" PRIx64 " %016" PRIx64,
        state.fields.n == NULL, state.fields.e == NULL, state.fields.f == NULL,
        state.fields.g == NULL, fb(state.fields.h), fb(state.fields.i), db(state.fields.j),
        db(state.fields.k), db(state.fields.l), db(state.b), db(state.c), db(state.d));
    return true;
}

static bool sign_geometry(fixture *f) {
    mc_client_tile_renderer_map_entry entry;
    mc_client_tile_renderer_object_state state;
    CHECK(mc_client_tile_renderer_map_find(f->manager, f->literals[4], &entry, &f->error) ==
          MC_CLIENT_TILE_RENDERER_OK);
    CHECK(mc_client_tile_renderer_object_inspect(entry.value, &state));
    CHECK(state.sign_model != NULL && state.sign_model == state.sign_candidate &&
          state.sign_phase == MC_CLIENT_MODEL_READY);
    const mc_client_sign_model *model = state.sign_model;
    const mc_client_model_part *parts[2] = {model->a, model->b};
    CHECK(parts[0] != NULL && parts[1] != NULL && parts[0] != parts[1]);
    row("TILE_SIGN %s %s %s %d", sign_type(model), part_type(parts[0]), part_type(parts[1]),
        parts[0] != parts[1]);
    identities graph = {0}, vertices = {0}, positions = {0};
    size_t original_count = 0, quad_count = 0, corner_count = 0;
    CHECK(add_identity(&graph, model));
    for (size_t p = 0; p < 2; ++p) {
        const mc_client_model_part *part = parts[p];
        CHECK(add_identity(&graph, part) && add_identity(&graph, part->j) &&
              add_identity(&graph, part->k));
        row("TILE_PART %zu %d %d %d %d %d %d %d %08" PRIx32 " %08" PRIx32 " %08" PRIx32
            " %08" PRIx32 " %08" PRIx32 " %08" PRIx32,
            p, part->l, part->m, part->g, part->h, part->i, part->n, part->o, fb(part->a),
            fb(part->b), fb(part->c), fb(part->d), fb(part->e), fb(part->f));
        for (size_t v = 0; v < 8; ++v) {
            const mc_client_model_vertex *vertex = part->j[v];
            CHECK(vertex != NULL && vertex->a != NULL && add_identity(&vertices, vertex) &&
                  add_identity(&positions, vertex->a) && add_identity(&graph, vertex) &&
                  add_identity(&graph, vertex->a));
            row("TILE_VERTEX %zu %zu %016" PRIx64 " %016" PRIx64 " %016" PRIx64 " %08" PRIx32
                " %08" PRIx32,
                p, v, db(vertex->a->x), db(vertex->a->y), db(vertex->a->z), fb(vertex->b),
                fb(vertex->c));
            ++original_count;
        }
        for (size_t q = 0; q < 6; ++q) {
            const mc_client_model_quad *quad = part->k[q];
            CHECK(quad != NULL && quad->a != NULL && quad->b == 4 && add_identity(&graph, quad) &&
                  add_identity(&graph, quad->a));
            ++rows;
            printf("TILE_QUAD %zu %zu %d %d", p, q, quad->b, quad->c);
            for (size_t corner = 0; corner < (size_t)quad->b; ++corner) {
                const mc_client_model_vertex *vertex = quad->a[corner];
                int index = -1;
                bool copy = true;
                CHECK(vertex != NULL && vertex->a != NULL);
                for (size_t v = 0; v < 8; ++v) {
                    if (vertex->a == part->j[v]->a)
                        index = (int)v;
                    if (vertex == part->j[v])
                        copy = false;
                }
                CHECK(index >= 0 && add_identity(&vertices, vertex) &&
                      add_identity(&graph, vertex));
                printf(" %d %d %08" PRIx32 " %08" PRIx32, index, copy, fb(vertex->b),
                       fb(vertex->c));
                ++corner_count;
            }
            putchar('\n');
            ++quad_count;
        }
    }
    row("TILE_GRAPH %zu %zu %zu %zu %zu %zu %zu", sizeof(parts) / sizeof(parts[0]), original_count,
        quad_count, corner_count, vertices.count, positions.count, graph.count);
    return true;
}

static bool lookup_rows(fixture *f, bool reverse) {
    for (size_t step = 0; step < 9; ++step) {
        const mc_tile_entity_class_literal *key = f->literals[reverse ? 8 - step : step];
        mc_client_tile_renderer_state before, after, repeated;
        mc_client_tile_renderer *first = NULL, *second = NULL;
        mc_client_tile_renderer_map_entry node, second_node;
        mc_tile_entity_class_literal_view view;
        const char *first_name, *second_name;
        CHECK(mc_client_tile_renderer_inspect(f->classes, &before));
        CHECK(mc_client_tile_renderer_lookup_class(f->manager, key, &first, &f->error) ==
              MC_CLIENT_TILE_RENDERER_OK);
        CHECK(mc_client_tile_renderer_inspect(f->classes, &after));
        CHECK(mc_client_tile_renderer_map_find(f->manager, key, &node, &f->error) ==
              MC_CLIENT_TILE_RENDERER_OK);
        CHECK(mc_client_tile_renderer_lookup_class(f->manager, key, &second, &f->error) ==
              MC_CLIENT_TILE_RENDERER_OK);
        CHECK(mc_client_tile_renderer_map_find(f->manager, key, &second_node, &f->error) ==
              MC_CLIENT_TILE_RENDERER_OK);
        CHECK(mc_client_tile_renderer_inspect(f->classes, &repeated));
        CHECK(mc_tile_entity_class_literal_inspect(key, &view) == MC_TILE_ENTITY_OK);
        CHECK(renderer_name(first, &first_name) && renderer_name(second, &second_name));
        row("TILE_LOOKUP %zu %s %s %zu %zu %s %zu %d %d %d", step, view.client_name, first_name,
            before.entries, after.entries, second_name, repeated.entries, first == second,
            second_node.present, node.identity == second_node.identity);
        char stage[24];
        (void)snprintf(stage, sizeof(stage), "lookup%02zu", step);
        CHECK(map_rows(f, stage));
    }
    return true;
}

static bool null_rows(fixture *f) {
    mc_client_tile_renderer_state before, after;
    mc_client_tile_renderer *renderer = NULL;
    bool has_renderer = true;
    CHECK(mc_client_tile_renderer_inspect(f->classes, &before));
    CHECK(mc_client_tile_renderer_lookup_tile(f->manager, NULL, &renderer, &f->error) ==
          MC_CLIENT_TILE_RENDERER_OK);
    CHECK(mc_client_tile_renderer_has_renderer(f->manager, NULL, &has_renderer, &f->error) ==
          MC_CLIENT_TILE_RENDERER_OK);
    CHECK(mc_client_tile_renderer_inspect(f->classes, &after));
    const char *name;
    CHECK(renderer_name(renderer, &name));
    row("TILE_NULL_TILE %s %d %zu %zu", name, has_renderer, before.entries, after.entries);
    CHECK(mc_client_tile_renderer_lookup_class(f->manager, NULL, &renderer, &f->error) ==
          MC_CLIENT_TILE_RENDERER_NULL_REFERENCE);
    CHECK(!f->error.native_policy && f->error.pc == 26 && f->error.ll_pc == 26 &&
          strcmp(f->error.source_class, "ll") == 0 && strcmp(f->error.source_method, "a") == 0 &&
          strcmp(f->error.source_descriptor, "(Ljava/lang/Class;)Lje;") == 0);
    /* NULL_REFERENCE maps to Java's source NPE category. pc26 is the actual
     * native source site, never an invented Java stack line number. */
    printf("TILE_NATIVE_ERROR %d NULL_REFERENCE %d %s %s %s %d %d\n", (int)f->error.status,
           f->error.native_policy, f->error.source_class, f->error.source_method,
           f->error.source_descriptor, f->error.pc, f->error.ll_pc);
    const char *source = f->error.source_class, *method = f->error.source_method;
    const int32_t pc = f->error.pc;
    CHECK(mc_client_tile_renderer_inspect(f->classes, &after));
    mc_client_tile_renderer_map_entry null_key;
    CHECK(mc_client_tile_renderer_map_find(f->manager, NULL, &null_key, &f->error) ==
          MC_CLIENT_TILE_RENDERER_OK);
    row("TILE_NULL_CLASS java.lang.NullPointerException %s %s pc%d %zu %zu %d", source, method, pc,
        before.entries, after.entries, null_key.present);
    CHECK(map_rows(f, "after_nulls"));
    return true;
}

static bool fixture_close(fixture *f) {
    CHECK(passive_classes(f));
    CHECK(mc_client_tile_renderer_classes_retire(&f->classes, &f->error) ==
              MC_CLIENT_TILE_RENDERER_OK &&
          f->classes == NULL);
    mc_entity_vector_snapshot vectors;
    CHECK(mc_entity_vector_get_snapshot(f->entities, &vectors) == MC_ENTITY_OK &&
          vectors.holds == 0);
    CHECK(mc_client_render_classes_free(f->models) == MC_CLIENT_MODEL_OK);
    CHECK(mc_world_runtime_classes_free(f->worlds, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_registry_classes_free(f->registry, NULL) == MC_REGISTRY_OK);
    CHECK(mc_entity_classes_free(f->entities) == MC_ENTITY_OK);
    mc_materials_destroy(f->materials);
    mc_java_random_profile_free(f->profile);
    return true;
}

static bool same_identity_thread(mc_java_identity_thread a, mc_java_identity_thread b) {
    return a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w;
}

static bool fresh_class_cells(fixture *f, mc_java_identity_cell **cells) {
    for (size_t i = 0; i < 9; ++i)
        CHECK(mc_tile_entity_class_literal_identity_cell(f->literals[i], &cells[i]) ==
                  MC_TILE_ENTITY_OK &&
              cells[i]->hash == 0);
    return true;
}

static bool class_chain(fixture *f, size_t bucket, const size_t *indices, size_t count) {
    CHECK(bucket < f->manager->map->capacity);
    tile_renderer_map_node *node = f->manager->map->buckets[bucket];
    for (size_t i = 0; i < count; ++i) {
        CHECK(node != NULL && node->key == f->literals[indices[i]]);
        node = node->next;
    }
    CHECK(node == NULL);
    return true;
}

static bool lazy_class_identity(void) {
    fixture f = {0};
    mc_java_identity_cell *cells[9];
    mc_java_identity_cell expected_cells[9] = {{0}};
    CHECK(fixture_open(&f) && fresh_class_cells(&f, cells));
    mc_java_identity_thread *thread = mc_java_random_profile_main_identity(f.profile);
    CHECK(thread != NULL);
    mc_java_identity_thread expected = *thread;
    const size_t constructor_keys[] = {4, 5, 7};
    for (size_t i = 0; i < 3; ++i)
        (void)mc_java_object_identity_hash(&expected, &expected_cells[constructor_keys[i]]);
    CHECK(mc_client_tile_renderer_static_get(f.classes, &f.manager, &f.error) ==
          MC_CLIENT_TILE_RENDERER_OK);
    CHECK(same_identity_thread(*thread, expected));
    CHECK(f.manager->map->size == 3 && f.manager->map->capacity == 16 &&
          f.manager->map->threshold == 12 && f.manager->map->mod_count == 3);
    for (size_t i = 0; i < 9; ++i)
        CHECK(cells[i]->hash == expected_cells[i].hash);
    /* Passive Map inspection of an unhashed Class neither caches it nor draws. */
    mc_client_tile_renderer_map_entry missing, entry;
    CHECK(mc_client_tile_renderer_map_find(f.manager, f.literals[0], &missing, &f.error) ==
              MC_CLIENT_TILE_RENDERER_OK &&
          !missing.present);
    for (size_t i = 0; i < 3; ++i) {
        CHECK(mc_client_tile_renderer_entry(f.manager, i, &entry, &f.error) ==
              MC_CLIENT_TILE_RENDERER_OK);
        const tile_renderer_map_node *node = entry.identity;
        mc_java_identity_cell *cell = NULL;
        CHECK(mc_tile_entity_class_literal_identity_cell(entry.key, &cell) == MC_TILE_ENTITY_OK &&
              node->hash == (cell->hash ^ (cell->hash >> 16u)));
    }
    CHECK(cells[0]->hash == 0 && cells[8]->hash == 0 && same_identity_thread(*thread, expected));
    /* get(Furnace) hashes that key, then the recursive get(base) hashes base.
     * The final cache put reuses Furnace's cell; base is not inserted. */
    (void)mc_java_object_identity_hash(&expected, &expected_cells[0]);
    (void)mc_java_object_identity_hash(&expected, &expected_cells[8]);
    mc_client_tile_renderer *renderer = NULL;
    CHECK(mc_client_tile_renderer_lookup_class(f.manager, f.literals[0], &renderer, &f.error) ==
              MC_CLIENT_TILE_RENDERER_OK &&
          renderer == NULL && same_identity_thread(*thread, expected));
    for (size_t i = 0; i < 9; ++i)
        CHECK(cells[i]->hash == expected_cells[i].hash);
    CHECK(mc_client_tile_renderer_map_find(f.manager, f.literals[0], &entry, &f.error) ==
              MC_CLIENT_TILE_RENDERER_OK &&
          entry.present && entry.value == NULL && f.manager->map->size == 4 &&
          f.manager->map->mod_count == 4);
    const void *cached_node = entry.identity;
    const size_t puts = f.manager->put_calls;
    CHECK(mc_client_tile_renderer_lookup_class(f.manager, f.literals[0], &renderer, &f.error) ==
              MC_CLIENT_TILE_RENDERER_OK &&
          renderer == NULL && f.manager->put_calls == puts + 1);
    CHECK(mc_client_tile_renderer_map_find(f.manager, f.literals[0], &entry, &f.error) ==
              MC_CLIENT_TILE_RENDERER_OK &&
          entry.identity == cached_node && f.manager->map->size == 4 &&
          f.manager->map->mod_count == 4);
    CHECK(mc_client_tile_renderer_lookup_class(f.manager, f.literals[4], &renderer, &f.error) ==
              MC_CLIENT_TILE_RENDERER_OK &&
          renderer != NULL && f.manager->put_calls == puts + 1);
    CHECK(mc_client_tile_renderer_map_find(f.manager, f.literals[8], &entry, &f.error) ==
              MC_CLIENT_TILE_RENDERER_OK &&
          !entry.present);
    CHECK(mc_client_tile_renderer_lookup_class(f.manager, f.literals[8], &renderer, &f.error) ==
              MC_CLIENT_TILE_RENDERER_OK &&
          renderer == NULL && f.manager->put_calls == puts + 1);
    CHECK(mc_client_tile_renderer_lookup_class(f.manager, NULL, &renderer, &f.error) ==
              MC_CLIENT_TILE_RENDERER_NULL_REFERENCE &&
          f.error.pc == 26 && !f.error.native_policy && same_identity_thread(*thread, expected));
    for (size_t i = 0; i < 9; ++i)
        CHECK(cells[i]->hash == expected_cells[i].hash);
    CHECK(fixture_close(&f));
    return true;
}

static bool colliding_class_identity(void) {
    fixture f = {0};
    mc_java_identity_cell *cells[9];
    CHECK(fixture_open(&f) && fresh_class_cells(&f, cells));
    mc_java_identity_thread *thread = mc_java_random_profile_main_identity(f.profile);
    CHECK(thread != NULL);
    const mc_java_identity_thread saved = *thread;
    /* Authored cached native cells, installed before their first Map use.
     * Raw hashes below 2^16 equal their spread. All share bucket3 at16 and
     * alternate bit 0x10 in insertion order, so resize must stably split them. */
    const size_t insertion[] = {4, 5, 7, 0, 1, 2, 3, 6, 8};
    for (size_t i = 0; i < 9; ++i)
        cells[insertion[i]]->hash = (uint32_t)(i + 2u) * 16u + 3u;
    CHECK(mc_client_tile_renderer_static_get(f.classes, &f.manager, &f.error) ==
              MC_CLIENT_TILE_RENDERER_OK &&
          same_identity_thread(*thread, saved));
    CHECK(class_chain(&f, 3, insertion, 3));
    for (size_t i = 3; i < 8; ++i) {
        mc_client_tile_renderer *renderer = NULL;
        CHECK(mc_client_tile_renderer_lookup_class(f.manager, f.literals[insertion[i]], &renderer,
                                                   &f.error) == MC_CLIENT_TILE_RENDERER_OK &&
              renderer == NULL && f.manager->map->size == i + 1u &&
              f.manager->map->capacity == 16 && f.manager->map->threshold == 12 &&
              f.manager->map->mod_count == (uint32_t)(i + 1u));
        CHECK(class_chain(&f, 3, insertion, i + 1u));
    }
    tile_renderer_map_node *nodes[9] = {0};
    for (size_t i = 0; i < 8; ++i) {
        nodes[insertion[i]] = tile_renderer_map_node_find(f.manager->map, f.literals[insertion[i]]);
        CHECK(nodes[insertion[i]] != NULL &&
              nodes[insertion[i]]->hash == cells[insertion[i]]->hash);
    }
    /* Source LL never inserts its base key: its eight reachable concrete keys
     * stop before collision growth. This ninth canonical key is an explicit
     * private Map-child control for the finite backend's full admitted domain,
     * not an assertion about an ordinary LL caller or a tree-bin execution. */
    tile_renderer_error_init(&f.error);
    f.manager->active = f.classes->active = true;
    const bool put = tile_renderer_map_put(f.manager, f.literals[8], NULL, 39, false, &f.error);
    f.manager->active = f.classes->active = false;
    CHECK(put && f.manager->map->size == 9 && f.manager->map->capacity == 32 &&
          f.manager->map->threshold == 24 && f.manager->map->mod_count == 9 &&
          same_identity_thread(*thread, saved));
    nodes[8] = tile_renderer_map_node_find(f.manager->map, f.literals[8]);
    CHECK(nodes[8] != NULL);
    const size_t low[] = {4, 7, 1, 3, 8}, high[] = {5, 0, 2, 6};
    CHECK(class_chain(&f, 3, low, 5) && class_chain(&f, 19, high, 4));
    for (size_t i = 0; i < 32; ++i)
        if (i != 3 && i != 19)
            CHECK(f.manager->map->buckets[i] == NULL);
    const size_t traversal[] = {4, 7, 1, 3, 8, 5, 0, 2, 6};
    for (size_t i = 0; i < 9; ++i) {
        mc_client_tile_renderer_map_entry entry;
        CHECK(mc_client_tile_renderer_entry(f.manager, i, &entry, &f.error) ==
                  MC_CLIENT_TILE_RENDERER_OK &&
              entry.key == f.literals[traversal[i]] && entry.identity == nodes[traversal[i]]);
    }
    const size_t puts = f.manager->put_calls;
    for (size_t i = 0; i < 9; ++i) {
        mc_client_tile_renderer *renderer = NULL;
        mc_client_tile_renderer_map_entry entry;
        CHECK(mc_client_tile_renderer_lookup_class(f.manager, f.literals[i], &renderer, &f.error) ==
              MC_CLIENT_TILE_RENDERER_OK);
        CHECK(mc_client_tile_renderer_map_find(f.manager, f.literals[i], &entry, &f.error) ==
                  MC_CLIENT_TILE_RENDERER_OK &&
              entry.present && entry.identity == nodes[i] && entry.value == renderer &&
              entry.value == nodes[i]->value);
    }
    /* Five cached-null concrete lookups still recurse and replace their value;
     * positive hits and the base return do not put. No key draws again. */
    CHECK(f.manager->put_calls == puts + 5u && f.manager->map->size == 9 &&
          f.manager->map->mod_count == 9 && same_identity_thread(*thread, saved));
    CHECK(class_chain(&f, 3, low, 5) && class_chain(&f, 19, high, 4));
    for (size_t i = 0; i < 9; ++i)
        CHECK(cells[insertion[i]]->hash == (uint32_t)(i + 2u) * 16u + 3u);
    CHECK(fixture_close(&f));
    return true;
}

static bool identity_controls(void) {
    CHECK(lazy_class_identity() && colliding_class_identity());
    printf("Native LL Class identity: %u checks passed; cleanup=1\n", checks);
    return true;
}

static bool observe(const char *profile) {
    fixture f = {0};
    CHECK(fixture_open(&f));
    row("TILE_CASE %s", profile);
    CHECK(literal_rows(&f));
    CHECK(mc_client_tile_renderer_static_get(f.classes, &f.manager, &f.error) ==
          MC_CLIENT_TILE_RENDERER_OK);
    CHECK(constructor_rows(&f) && map_rows(&f, "initial"));
    if (strcmp(profile, "constructor") == 0)
        CHECK(sign_geometry(&f));
    else if (strcmp(profile, "forward") == 0 || strcmp(profile, "reverse") == 0)
        CHECK(lookup_rows(&f, strcmp(profile, "reverse") == 0));
    else
        CHECK(null_rows(&f));
    CHECK(fixture_close(&f));
    CHECK(ferror(stdout) == 0);
    printf("Source TileEntityRenderer: %s; rows=%u; checks=%u; cleanup=1\n", profile, rows, checks);
    return true;
}

int main(int argc, char **argv) {
    if (argc == 2 && strcmp(argv[1], "identity") == 0)
        return identity_controls() ? 0 : 1;
    if (argc != 2 || (strcmp(argv[1], "constructor") != 0 && strcmp(argv[1], "forward") != 0 &&
                      strcmp(argv[1], "reverse") != 0 && strcmp(argv[1], "nulls") != 0)) {
        fprintf(stderr, "TileEntityRenderer requires constructor|forward|reverse|nulls|identity\n");
        return 1;
    }
    return observe(argv[1]) ? 0 : 1;
}
