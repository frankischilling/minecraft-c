#include "mc/item/recipes/recipes.h"
#include "mc/item/registry/registry.h"
#include "mc/resources/files/files.h"
#include "mc/statistics/achievements/achievements.h"
#include "mc/statistics/registry/registry.h"

#include <inttypes.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

#define GRAPH_LIMIT 4096u

typedef struct cell {
    int32_t id;
    const mc_statistic *object;
} cell;

typedef struct icon_table {
    const mc_item_stack *objects[GRAPH_LIMIT];
    size_t count;
} icon_table;

static bool row(const char *format, ...) {
    va_list args;
    va_start(args, format);
    int result = vprintf(format, args);
    va_end(args);
    return result >= 0;
}

static int compare(const void *left, const void *right) {
    int32_t a = ((const cell *)left)->id, b = ((const cell *)right)->id;
    return a < b ? -1 : a != b;
}

static const cell *reference(const cell *cells, size_t count, const mc_statistic *object) {
    if (object == NULL)
        return NULL;
    for (size_t i = 0; i < count; ++i)
        if (cells[i].object == object)
            return &cells[i];
    return NULL;
}

static char formatter(const mc_statistic *object, const mc_registry_statistic_base_view *base) {
    const mc_statistic_fields *f = mc_statistic_fields_get(object);
    return f->a == base->i ? 'i' : f->a == base->j ? 'j' : f->a == base->k ? 'k' : 0;
}

static bool graph(mc_registry_classes *owner, const mc_registry_statistic_view *view,
                  const mc_registry_statistic_base_view *base, cell *cells) {
    if (view->map_size > GRAPH_LIMIT || view->named == NULL || base->i == NULL || base->j == NULL ||
        base->k == NULL || base->i == base->j || base->i == base->k || base->j == base->k)
        return false;
    for (size_t i = 0; i < view->map_size; ++i) {
        if (mc_registry_classes_statistic_entry(owner, i, &cells[i].id, &cells[i].object) !=
                MC_REGISTRY_OK ||
            cells[i].object == NULL)
            return false;
        const mc_statistic_fields *f = mc_statistic_fields_get(cells[i].object);
        const mc_statistic *found = NULL;
        mc_statistic_kind kind = mc_statistic_kind_get(cells[i].object);
        if (f == NULL || f->e != cells[i].id ||
            (kind != MC_STATISTIC_GENERAL && kind != MC_STATISTIC_ITEM &&
             kind != MC_STATISTIC_ACHIEVEMENT) ||
            formatter(cells[i].object, base) == 0 ||
            mc_registry_classes_find_statistic(owner, cells[i].id, &found) != MC_REGISTRY_OK ||
            found != cells[i].object)
            return false;
        if (kind == MC_STATISTIC_ITEM) {
            int32_t associated;
            if (mc_statistic_item_id(cells[i].object, &associated) != MC_REGISTRY_OK)
                return false;
        }
        for (size_t j = 0; j < i; ++j)
            if (cells[i].id == cells[j].id || cells[i].object == cells[j].object)
                return false;
    }
    qsort(cells, view->map_size, sizeof(*cells), compare);
    for (size_t i = 0; i < 23; ++i)
        if (reference(cells, view->map_size, view->named[i]) == NULL)
            return false;
    mc_statistic *const *arrays[] = {view->C, view->D, view->E, view->F};
    for (size_t a = 0; a < 4; ++a) {
        size_t length = a == 0 ? 256 : 32000;
        if (arrays[a] != NULL)
            for (size_t i = 0; i < length; ++i)
                if (arrays[a][i] != NULL && reference(cells, view->map_size, arrays[a][i]) == NULL)
                    return false;
    }
    const mc_statistic_list *lists[] = {view->b, view->c, view->d, view->e};
    for (size_t a = 0; a < 4; ++a) {
        size_t length = mc_statistic_list_size(lists[a]);
        if (length > GRAPH_LIMIT)
            return false;
        for (size_t i = 0; i < length; ++i) {
            const mc_statistic *object = NULL;
            if (mc_statistic_list_get(lists[a], i, &object) != MC_REGISTRY_OK ||
                (object != NULL && reference(cells, view->map_size, object) == NULL))
                return false;
        }
    }
    return true;
}

static bool emit(const char *side, const char *entry, const cell *cells,
                 const mc_registry_statistic_view *view,
                 const mc_registry_statistic_base_view *base) {
    const char *boundary = strcmp(entry, "item") == 0       ? "item-icon-null2"
                           : strcmp(entry, "crafting") == 0 ? "crafting-null24"
                           : strcmp(entry, "smelting") == 0 ? "smelting-null75"
                                                            : "complete";
    if (!row("STAT_BOOTSTRAP_V1\t%s\t%s\t%s\n", side, entry, boundary) ||
        !row("STATE\t%u\t%u\nMAP\t%zu\n", view->G ? 1u : 0u, view->H ? 1u : 0u, view->map_size))
        return false;
    for (size_t i = 0; i < view->map_size; ++i) {
        const mc_statistic *object = cells[i].object;
        const mc_statistic_fields *f = mc_statistic_fields_get(object);
        bool item = mc_statistic_kind_get(object) == MC_STATISTIC_ITEM;
        const char *kind = item ? "item"
                           : mc_statistic_kind_get(object) == MC_STATISTIC_ACHIEVEMENT
                               ? "achievement"
                               : "general";
        if (!row("MAPSTAT\t%" PRId32 "\t%s\t%u\t%c\t", cells[i].id, kind, f->g ? 1u : 0u,
                 formatter(object, base)))
            return false;
        int32_t associated;
        if (item) {
            if (mc_statistic_item_id(object, &associated) != MC_REGISTRY_OK ||
                !row("%" PRId32 "\n", associated))
                return false;
        } else if (!row("NULL\n"))
            return false;
    }
    for (size_t i = 0; i < 23; ++i)
        if (!row("NAMED\t%zu\t%" PRId32 "\n", i,
                 reference(cells, view->map_size, view->named[i])->id))
            return false;
    mc_statistic *const *arrays[] = {view->C, view->D, view->E, view->F};
    for (size_t a = 0; a < 4; ++a) {
        size_t count = 0, length = a == 0 ? 256 : 32000;
        char label = (char)('C' + a);
        if (arrays[a] == NULL) {
            if (!row("ARRAY\t%c\tNULL\n", label))
                return false;
        } else {
            if (!row("ARRAY\t%c\t%zu\n", label, length))
                return false;
            for (size_t i = 0; i < length; ++i)
                if (arrays[a][i] != NULL) {
                    if (!row("SLOT\t%c\t%zu\t%" PRId32 "\n", label, i,
                             reference(cells, view->map_size, arrays[a][i])->id))
                        return false;
                    ++count;
                }
        }
        if (!row("END_ARRAY\t%c\t%zu\n", label, count))
            return false;
    }
    const mc_statistic_list *lists[] = {view->b, view->c, view->d, view->e};
    for (size_t a = 0; a < 4; ++a) {
        size_t length = mc_statistic_list_size(lists[a]);
        char label = (char)('b' + a);
        if (!row("LIST\t%c\t%zu\n", label, length))
            return false;
        for (size_t i = 0; i < length; ++i) {
            const mc_statistic *object = NULL;
            if (mc_statistic_list_get(lists[a], i, &object) != MC_REGISTRY_OK ||
                !row("ELEMENT\t%c\t%zu\t", label, i))
                return false;
            if (object == NULL
                    ? !row("NULL\n")
                    : !row("%" PRId32 "\n", reference(cells, view->map_size, object)->id))
                return false;
        }
    }
    return true;
}

static size_t icon_reference(const icon_table *icons, const mc_item_stack *stack) {
    for (size_t i = 0; i < icons->count; ++i)
        if (icons->objects[i] == stack)
            return i + 1;
    return 0;
}

static bool achievement_graph(const cell *cells, const mc_registry_statistic_view *stats,
                              const mc_registry_achievement_view *view, icon_table *icons) {
    if (view->named == NULL || mc_statistic_list_size(view->e) > GRAPH_LIMIT ||
        (view->e != NULL && (view->e == stats->b || view->e == stats->c || view->e == stats->d ||
                             view->e == stats->e)))
        return false;
    for (size_t i = 0; i < stats->map_size; ++i) {
        const mc_statistic *object = cells[i].object;
        if (mc_statistic_kind_get(object) != MC_STATISTIC_ACHIEVEMENT)
            continue;
        const mc_achievement_fields *fields = mc_statistic_achievement_fields_get(object);
        if (fields == NULL || fields->l == NULL || fields->client_callback != NULL ||
            (fields->c != NULL && (reference(cells, stats->map_size, fields->c) == NULL ||
                                   mc_statistic_kind_get(fields->c) != MC_STATISTIC_ACHIEVEMENT)))
            return false;
        if (fields->d != NULL && icon_reference(icons, fields->d) == 0) {
            if (icons->count == GRAPH_LIMIT)
                return false;
            mc_item_stack_fields values;
            mc_item_stack_phase phase;
            if (mc_item_stack_fields_get(fields->d, &values) != MC_ITEM_STACK_OK ||
                mc_item_stack_phase_get(fields->d, &phase) != MC_ITEM_STACK_OK ||
                phase != MC_ITEM_STACK_READY)
                return false;
            icons->objects[icons->count++] = fields->d;
        }
    }
    for (size_t i = 0; i < 16; ++i)
        if (view->named[i] != NULL &&
            (reference(cells, stats->map_size, view->named[i]) == NULL ||
             mc_statistic_kind_get(view->named[i]) != MC_STATISTIC_ACHIEVEMENT))
            return false;
    for (size_t i = 0; i < mc_statistic_list_size(view->e); ++i) {
        const mc_statistic *object = NULL;
        if (mc_statistic_list_get(view->e, i, &object) != MC_REGISTRY_OK ||
            (object != NULL && (reference(cells, stats->map_size, object) == NULL ||
                                mc_statistic_kind_get(object) != MC_STATISTIC_ACHIEVEMENT)))
            return false;
    }
    return true;
}

static bool emit_reference(const cell *cells, size_t count, const mc_statistic *object) {
    return object == NULL ? row("NULL") : row("%" PRId32, reference(cells, count, object)->id);
}

static bool emit_achievements(const cell *cells, const mc_registry_statistic_view *stats,
                              const mc_registry_achievement_view *view, const icon_table *icons) {
    if (!row("ACHIEVEMENTS\t%" PRId32 "\t%" PRId32 "\t%" PRId32 "\t%" PRId32 "\t", view->a, view->b,
             view->c, view->d) ||
        !(view->e == NULL ? row("NULL\n") : row("%zu\n", mc_statistic_list_size(view->e))))
        return false;
    for (size_t i = 0; i < 16; ++i)
        if (!row("ACH_NAMED\t%zu\t", i) ||
            !emit_reference(cells, stats->map_size, view->named[i]) || !row("\n"))
            return false;
    for (size_t i = 0; i < stats->map_size; ++i) {
        const mc_achievement_fields *fields = mc_statistic_achievement_fields_get(cells[i].object);
        if (fields == NULL)
            continue;
        if (!row("ACH\t%" PRId32 "\t", cells[i].id) ||
            !emit_reference(cells, stats->map_size, fields->c) ||
            !row("\t%u\t", fields->special ? 1u : 0u) ||
            !(fields->d == NULL ? row("NULL\n") : row("I:%zu\n", icon_reference(icons, fields->d))))
            return false;
    }
    for (size_t i = 0; i < icons->count; ++i) {
        mc_item_stack_fields fields;
        if (mc_item_stack_fields_get(icons->objects[i], &fields) != MC_ITEM_STACK_OK ||
            !row("ICON\t%zu\t%" PRId32 "\t%" PRId32 "\t%" PRId32 "\t%" PRId32 "\n", i + 1,
                 fields.count, fields.animation, fields.id, fields.damage))
            return false;
    }
    for (size_t i = 0; i < mc_statistic_list_size(view->e); ++i) {
        const mc_statistic *object = NULL;
        if (mc_statistic_list_get(view->e, i, &object) != MC_REGISTRY_OK ||
            !row("ACH_LIST\t%zu\t", i) || !emit_reference(cells, stats->map_size, object) ||
            !row("\n"))
            return false;
    }
    return row("END\n");
}

static char *root_argument(int argc, char **argv) {
    if (argc == 4) {
        size_t length = strlen(argv[3]);
        if (length == 0 || length > 32768)
            return NULL;
        char *copy = malloc(length + 1);
        if (copy != NULL)
            memcpy(copy, argv[3], length + 1);
        return copy;
    }
    if (argc != 5 || strcmp(argv[3], "--root-hex") != 0)
        return NULL;
    size_t length = strlen(argv[4]);
    if (length == 0 || length > 65536 || length % 2 != 0)
        return NULL;
    char *root = malloc(length / 2 + 1);
    if (root == NULL)
        return NULL;
    for (size_t i = 0; i < length / 2; ++i) {
        unsigned int value = 0;
        for (size_t j = 0; j < 2; ++j) {
            unsigned char c = (unsigned char)argv[4][2 * i + j];
            if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) {
                free(root);
                return NULL;
            }
            value = value * 16u + (c <= '9' ? (unsigned int)c - (unsigned int)'0'
                                            : (unsigned int)c - (unsigned int)'a' + 10u);
        }
        if (value == 0) {
            free(root);
            return NULL;
        }
        root[i] = (char)(unsigned char)value;
    }
    root[length / 2] = 0;
    return root;
}

int main(int argc, char **argv) {
    int result = 1;
    mc_resource_files *files = NULL;
    mc_materials *materials = NULL;
    mc_java_random_profile *profile = NULL;
    mc_registry_classes *owner = NULL;
    mc_registry_error error = {0};
    char *root = NULL;
    cell *cells = NULL;
    icon_table *icons = NULL;
#ifdef _WIN32
    if (_setmode(_fileno(stdout), _O_BINARY) < 0 || _setmode(_fileno(stderr), _O_BINARY) < 0)
        goto cleanup;
#endif
#ifdef SIGPIPE
    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
        goto cleanup;
#endif
    if ((argc != 4 && argc != 5) ||
        (strcmp(argv[1], "client") != 0 && strcmp(argv[1], "server") != 0) ||
        (strcmp(argv[2], "stats") != 0 && strcmp(argv[2], "block") != 0 &&
         strcmp(argv[2], "item") != 0 && strcmp(argv[2], "crafting") != 0 &&
         strcmp(argv[2], "smelting") != 0))
        goto cleanup;
    root = root_argument(argc, argv);
    if (root == NULL || mc_resource_files_create(root, &files) != MC_REGISTRY_OK ||
        !mc_materials_create(&materials) ||
        mc_java_random_profile_create(SIZE_MAX, &profile) != MC_JAVA_RANDOM_PROFILE_OK)
        goto cleanup;
    const mc_registry_options options = {strcmp(argv[1], "client") == 0 ? MC_REGISTRY_CLIENT
                                                                        : MC_REGISTRY_SERVER,
                                         profile, materials, 0, mc_resource_files_source(files)};
    if (mc_registry_classes_allocate(&options, &owner, &error) != MC_REGISTRY_OK)
        goto cleanup;
    mc_registry_status status =
        strcmp(argv[2], "stats") == 0   ? mc_registry_classes_initialize_statistics(owner, &error)
        : strcmp(argv[2], "block") == 0 ? mc_registry_classes_initialize_blocks(owner, &error)
        : strcmp(argv[2], "item") == 0  ? mc_registry_classes_initialize_items(owner, &error)
        : strcmp(argv[2], "crafting") == 0 ? mc_registry_classes_initialize_crafting(owner, &error)
                                           : mc_registry_classes_initialize_smelting(owner, &error);
    mc_registry_statistic_view view;
    mc_registry_statistic_base_view base;
    mc_registry_achievement_view achievements;
    bool item = strcmp(argv[2], "item") == 0;
    bool crafting = strcmp(argv[2], "crafting") == 0;
    bool smelting = strcmp(argv[2], "smelting") == 0;
    if (mc_registry_classes_inspect_statistics(owner, &view, NULL) != MC_REGISTRY_OK ||
        mc_registry_classes_inspect_statistic_base(owner, &base, NULL) != MC_REGISTRY_OK ||
        mc_registry_classes_inspect_achievements(owner, &achievements, NULL) != MC_REGISTRY_OK)
        goto cleanup;
    if (item) {
        if (status != MC_REGISTRY_NULL_REFERENCE || view.phase != MC_REGISTRY_FAILED ||
            view.initializer_pc != 624 || view.C == NULL || view.D != NULL || view.E != NULL ||
            view.F != NULL || view.G || view.H || error.constructor_pc != 2 ||
            error.constructor_class == NULL ||
            strcmp(error.constructor_class, options.side == MC_REGISTRY_CLIENT ? "iz" : "fy") != 0)
            goto cleanup;
    } else {
        if (view.phase != MC_REGISTRY_READY || view.initializer_pc != 635 || view.C == NULL ||
            view.E == NULL || view.F == NULL || achievements.phase != MC_REGISTRY_READY ||
            achievements.initializer_pc != 454)
            goto cleanup;
        if (crafting || smelting) {
            if (status != MC_REGISTRY_NULL_REFERENCE || view.D != NULL ||
                error.constructor_pc != (crafting ? 24 : 75) || error.constructor_class == NULL ||
                strcmp(error.constructor_class, options.side == MC_REGISTRY_CLIENT ? "jl" : "gg") !=
                    0)
                goto cleanup;
        } else if (status != MC_REGISTRY_OK || view.D == NULL)
            goto cleanup;
    }
    cells = calloc(GRAPH_LIMIT, sizeof(*cells));
    icons = calloc(1, sizeof(*icons));
    if (cells == NULL || icons == NULL || !graph(owner, &view, &base, cells) ||
        !achievement_graph(cells, &view, &achievements, icons) ||
        !emit(argv[1], argv[2], cells, &view, &base) ||
        !emit_achievements(cells, &view, &achievements, icons) || fflush(stdout) != 0 ||
        ferror(stdout))
        goto cleanup;
    result = 0;
cleanup:
    if (result != 0)
        fprintf(stderr, "statistic array probe failed: status=%d constructor=%s pc=%d\n",
                (int)error.status, error.constructor_class == NULL ? "-" : error.constructor_class,
                (int)error.constructor_pc);
    free(cells);
    free(icons);
    free(root);
    if (mc_registry_classes_free(owner, NULL) != MC_REGISTRY_OK)
        return 1; /* Do not retire dependencies still borrowed by a live owner. */
    if (files != NULL && mc_resource_files_free(files) != MC_REGISTRY_OK)
        result = 1;
    mc_materials_destroy(materials);
    mc_java_random_profile_free(profile);
    return result;
}
