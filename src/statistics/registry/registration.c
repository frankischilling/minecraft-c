#include "../../block/registry/registry_internal.h"
#include "../achievements/achievements_internal.h"
#include "registry_internal.h"

#include <limits.h>

typedef struct statistic_map_entry {
    int32_t id;
    mc_statistic *statistic;
    struct statistic_map_entry *next;
} statistic_map_entry;

struct mc_statistic_map {
    statistic_map_entry *first, *last;
    size_t size;
};

static mc_registry_status diagnostic(mc_registry_error *error, mc_registry_status status,
                                     const char *message) {
    if (error != NULL)
        *error = (mc_registry_error){.status = status,
                                     .message = message,
                                     .initializer_pc = -1,
                                     .block_id = -1,
                                     .constructor_pc = -1,
                                     .profile_status = MC_JAVA_RANDOM_PROFILE_OK,
                                     .active_initializer_pc = -1,
                                     .item_argument = -1};
    return status;
}

static void stage(mc_statistic *statistic, bool general, int32_t pc) {
    const bool client = statistic->classes->options.side == MC_REGISTRY_CLIENT;
    statistic->source_class = general ? (client ? "yd" : "pm") : (client ? "vr" : "ns");
    statistic->source_pc = pc;
}

static mc_registry_status own_error(mc_statistic *statistic) {
    mc_registry_error *error = &statistic->classes->last_error;
    error->constructor_class = statistic->source_class;
    error->constructor_pc = statistic->source_pc;
    return error->status;
}

static mc_registry_status fail(mc_statistic *statistic, mc_registry_status status,
                               const char *message) {
    (void)mc_registry_fail(statistic->classes, status, message, NULL);
    return own_error(statistic);
}

static statistic_map_entry *find_entry(const mc_statistic_map *map, int32_t id) {
    for (statistic_map_entry *entry = map->first; entry != NULL; entry = entry->next)
        if (entry->id == id)
            return entry;
    return NULL;
}

mc_registry_status mc_registry_statistic_collections(mc_registry_classes *classes) {
    if (classes == NULL)
        return MC_REGISTRY_INVALID_ARGUMENT;
    if (classes->entered == 0 || classes->statistics_state.phase != MC_REGISTRY_INITIALIZING)
        return mc_registry_fail(classes, MC_REGISTRY_INVALID_ARGUMENT,
                                "Statistics collections require their admitted initializer", NULL);
    mc_registry_initializer_stage(classes, 0);
    mc_statistic_map *map = mc_registry_allocate(classes, sizeof(*map));
    if (map == NULL)
        return classes->last_error.status;
    mc_registry_initializer_stage(classes, 4);
    *map = (mc_statistic_map){0};
    mc_registry_initializer_stage(classes, 7);
    classes->statistics.a = map;

    mc_registry_initializer_stage(classes, 10);
    mc_registry_list *list = mc_registry_allocate(classes, sizeof(*list));
    if (list == NULL)
        return classes->last_error.status;
    mc_registry_initializer_stage(classes, 14);
    *list = (mc_registry_list){0};
    mc_registry_initializer_stage(classes, 17);
    classes->statistics.b = list;

    mc_registry_initializer_stage(classes, 20);
    list = mc_registry_allocate(classes, sizeof(*list));
    if (list == NULL)
        return classes->last_error.status;
    mc_registry_initializer_stage(classes, 24);
    *list = (mc_registry_list){0};
    mc_registry_initializer_stage(classes, 27);
    classes->statistics.c = list;

    mc_registry_initializer_stage(classes, 30);
    list = mc_registry_allocate(classes, sizeof(*list));
    if (list == NULL)
        return classes->last_error.status;
    mc_registry_initializer_stage(classes, 34);
    *list = (mc_registry_list){0};
    mc_registry_initializer_stage(classes, 37);
    classes->statistics.d = list;

    mc_registry_initializer_stage(classes, 40);
    list = mc_registry_allocate(classes, sizeof(*list));
    if (list == NULL)
        return classes->last_error.status;
    mc_registry_initializer_stage(classes, 44);
    *list = (mc_registry_list){0};
    mc_registry_initializer_stage(classes, 47);
    classes->statistics.e = list;
    return MC_REGISTRY_OK;
}

mc_registry_status mc_registry_statistic_list_add(mc_registry_classes *classes,
                                                  mc_registry_list *list, mc_statistic *statistic) {
    if (classes == NULL)
        return MC_REGISTRY_INVALID_ARGUMENT;
    if (classes->entered == 0)
        return mc_registry_fail(classes, MC_REGISTRY_INVALID_ARGUMENT,
                                "Statistic list mutation requires admitted initialization", NULL);
    if (list == NULL)
        return mc_registry_fail(classes, MC_REGISTRY_NULL_REFERENCE,
                                "Statistic List.add null receiver", NULL);
    if (list->count >= (size_t)INT32_MAX)
        return mc_registry_fail(classes, MC_REGISTRY_LIMIT, "Statistic native list count bound",
                                NULL);
    if (list->count == list->capacity) {
        size_t capacity = list->capacity == 0 ? 10u : list->capacity;
        if (list->capacity != 0) {
            size_t growth = capacity / 2u;
            if (growth == 0)
                growth = 1;
            if (growth > (size_t)INT32_MAX - capacity)
                capacity = (size_t)INT32_MAX;
            else
                capacity += growth;
        }
        if (capacity > SIZE_MAX / sizeof(void *))
            return mc_registry_fail(classes, MC_REGISTRY_LIMIT,
                                    "Statistic native list allocation bound", NULL);
        void **elements = mc_registry_allocate(classes, capacity * sizeof(*elements));
        if (elements == NULL)
            return classes->last_error.status;
        for (size_t i = 0; i < list->count; ++i)
            elements[i] = list->elements[i];
        /* The common arena retains the previous native backing allocation. */
        list->elements = elements;
        list->capacity = capacity;
    }
    list->elements[list->count] = statistic;
    ++list->count;
    return MC_REGISTRY_OK;
}

static mc_registry_status map_put(mc_statistic *statistic, mc_statistic_map *map, int32_t id) {
    if (map == NULL)
        return fail(statistic, MC_REGISTRY_NULL_REFERENCE, "Statistic Map.put110 null receiver");
    statistic_map_entry *entry = find_entry(map, id);
    if (entry != NULL) {
        entry->statistic = statistic;
        return MC_REGISTRY_OK;
    }
    if (map->size >= (size_t)INT32_MAX)
        return fail(statistic, MC_REGISTRY_LIMIT, "Statistic native map count bound");
    entry = mc_registry_allocate(statistic->classes, sizeof(*entry));
    if (entry == NULL)
        return own_error(statistic);
    *entry = (statistic_map_entry){.id = id, .statistic = statistic, .next = NULL};
    if (map->last == NULL)
        map->first = entry;
    else
        map->last->next = entry;
    map->last = entry;
    ++map->size;
    return MC_REGISTRY_OK;
}

static size_t decimal_units(int32_t value, uint16_t units[11]) {
    uint16_t reversed[10];
    size_t digits = 0, count = 0;
    uint32_t magnitude;
    if (value < 0) {
        units[count++] = '-';
        magnitude = (uint32_t)(-(value + 1)) + 1u;
    } else {
        magnitude = (uint32_t)value;
    }
    do {
        reversed[digits++] = (uint16_t)('0' + magnitude % 10u);
        magnitude /= 10u;
    } while (magnitude != 0);
    while (digits != 0)
        units[count++] = reversed[--digits];
    return count;
}

static mc_registry_status duplicate_message(mc_statistic *statistic) {
    mc_registry_classes *classes = statistic->classes;
    const mc_registry_string *prefix = NULL, *between = NULL, *suffix = NULL;
    stage(statistic, false, 29);
    mc_registry_status status =
        mc_registry_literal_ascii(classes, "Duplicate stat id: \"", &prefix);
    if (status != MC_REGISTRY_OK)
        return own_error(statistic);
    stage(statistic, false, 34);
    const mc_statistic_map *map = classes->statistics.a;
    stage(statistic, false, 44);
    if (map == NULL)
        return fail(statistic, MC_REGISTRY_NULL_REFERENCE,
                    "Statistic duplicate Map.get44 null receiver");
    const statistic_map_entry *entry = find_entry(map, statistic->fields.e);
    stage(statistic, false, 52);
    if (entry == NULL || entry->statistic == NULL)
        return fail(statistic, MC_REGISTRY_NULL_REFERENCE,
                    "Statistic duplicate getfield52 null owner");
    const mc_registry_string *existing = entry->statistic->fields.f;
    stage(statistic, false, 58);
    status = mc_registry_literal_ascii(classes, "\" and \"", &between);
    if (status != MC_REGISTRY_OK)
        return own_error(statistic);
    stage(statistic, false, 64);
    const mc_registry_string *current = statistic->fields.f;
    stage(statistic, false, 70);
    status = mc_registry_literal_ascii(classes, "\" at id ", &suffix);
    if (status != MC_REGISTRY_OK)
        return own_error(statistic);
    stage(statistic, false, 76);
    uint16_t id_units[11];
    const size_t id_length = decimal_units(statistic->fields.e, id_units);
    static const uint16_t null_units[4] = {'n', 'u', 'l', 'l'};
    const uint16_t *parts[6] = {prefix->units,  existing == NULL ? null_units : existing->units,
                                between->units, current == NULL ? null_units : current->units,
                                suffix->units,  id_units};
    const size_t lengths[6] = {prefix->length,  existing == NULL ? 4u : existing->length,
                               between->length, current == NULL ? 4u : current->length,
                               suffix->length,  id_length};
    stage(statistic, false, 82);
    size_t length = 0;
    for (size_t i = 0; i < 6u; ++i) {
        if (lengths[i] > (size_t)INT32_MAX - length)
            return fail(statistic, MC_REGISTRY_LIMIT,
                        "Statistic duplicate message native length bound");
        if (lengths[i] != 0 && parts[i] == NULL)
            return fail(statistic, MC_REGISTRY_INVALID_ARGUMENT,
                        "Statistic duplicate message invalid String storage");
        length += lengths[i];
    }
    if (length > SIZE_MAX / sizeof(uint16_t))
        return fail(statistic, MC_REGISTRY_LIMIT,
                    "Statistic duplicate message native allocation bound");
    mc_registry_string *message = mc_registry_allocate(classes, sizeof(*message));
    if (message == NULL)
        return own_error(statistic);
    uint16_t *units = mc_registry_allocate(classes, length * sizeof(*units));
    if (units == NULL)
        return own_error(statistic);
    size_t offset = 0;
    for (size_t i = 0; i < 6u; ++i)
        for (size_t j = 0; j < lengths[i]; ++j)
            units[offset++] = parts[i][j];
    *message = (mc_registry_string){.units = units, .length = length};
    classes->statistics.duplicate_message = message;
    stage(statistic, false, 88);
    return fail(statistic, MC_REGISTRY_DUPLICATE_STATISTIC, "Duplicate statistic ID");
}

mc_registry_status mc_registry_statistic_register_base(mc_statistic *statistic) {
    if (statistic == NULL || statistic->classes == NULL)
        return MC_REGISTRY_INVALID_ARGUMENT;
    mc_registry_classes *classes = statistic->classes;
    if (classes->entered == 0)
        return mc_registry_fail(classes, MC_REGISTRY_INVALID_ARGUMENT,
                                "Statistic registration requires admitted initialization", NULL);
    stage(statistic, false, 0);
    mc_registry_status status = mc_registry_statistics_active_use(classes);
    if (status != MC_REGISTRY_OK)
        return status;
    const mc_statistic_map *map = classes->statistics.a;
    stage(statistic, false, 10);
    if (map == NULL)
        return fail(statistic, MC_REGISTRY_NULL_REFERENCE,
                    "Statistic Map.containsKey10 null receiver");
    if (find_entry(map, statistic->fields.e) != NULL)
        return duplicate_message(statistic);
    stage(statistic, false, 89);
    mc_registry_list *list = classes->statistics.b;
    stage(statistic, false, 93);
    status = mc_registry_statistic_list_add(classes, list, statistic);
    if (status != MC_REGISTRY_OK)
        return own_error(statistic);
    stage(statistic, false, 99);
    mc_statistic_map *destination = classes->statistics.a;
    stage(statistic, false, 110);
    status = map_put(statistic, destination, statistic->fields.e);
    if (status != MC_REGISTRY_OK)
        return status;
    stage(statistic, false, 121);
    const mc_registry_string *key = NULL;
    status = mc_registry_statistic_key(classes, statistic->fields.e, &key);
    if (status != MC_REGISTRY_OK)
        return status; /* Keep the actual nested b initializer's first cause. */
    stage(statistic, false, 124);
    statistic->fields.h = key;
    stage(statistic, false, 128);
    return MC_REGISTRY_OK;
}

mc_registry_status mc_registry_statistic_register(mc_statistic *statistic) {
    if (statistic != NULL && statistic->kind == MC_STATISTIC_ACHIEVEMENT)
        return mc_registry_achievement_register(statistic);
    mc_registry_status status = mc_registry_statistic_register_base(statistic);
    if (status != MC_REGISTRY_OK)
        return status;
    mc_registry_classes *classes = statistic->classes;
    if (statistic->kind == MC_STATISTIC_GENERAL) {
        stage(statistic, true, 5);
        mc_registry_list *list = classes->statistics.c;
        stage(statistic, true, 9);
        status = mc_registry_statistic_list_add(classes, list, statistic);
        if (status != MC_REGISTRY_OK)
            return own_error(statistic);
        stage(statistic, true, 16);
    }
    return MC_REGISTRY_OK;
}

mc_registry_status mc_registry_classes_inspect_statistics(const mc_registry_classes *classes,
                                                          mc_registry_statistic_view *out,
                                                          mc_registry_error *error) {
    if (classes == NULL || out == NULL)
        return diagnostic(error, MC_REGISTRY_INVALID_ARGUMENT, "Statistics view arguments");
    const mc_registry_statistic_storage *storage = &classes->statistics;
    *out = (mc_registry_statistic_view){.phase = classes->statistics_state.phase,
                                        .initializer_pc = classes->statistics_state.pc,
                                        .map_size = storage->a == NULL ? 0 : storage->a->size,
                                        .b = storage->b,
                                        .c = storage->c,
                                        .d = storage->d,
                                        .e = storage->e,
                                        .named = storage->named,
                                        .C = storage->C,
                                        .D = storage->D,
                                        .E = storage->E,
                                        .F = storage->F,
                                        .G = storage->G,
                                        .H = storage->H,
                                        .duplicate_message = storage->duplicate_message};
    return diagnostic(error, MC_REGISTRY_OK, NULL);
}

mc_registry_status mc_registry_classes_find_statistic(const mc_registry_classes *classes,
                                                      int32_t id, const mc_statistic **out) {
    if (classes == NULL || out == NULL)
        return MC_REGISTRY_INVALID_ARGUMENT;
    const statistic_map_entry *entry =
        classes->statistics.a == NULL ? NULL : find_entry(classes->statistics.a, id);
    *out = entry == NULL ? NULL : entry->statistic;
    return MC_REGISTRY_OK;
}

mc_registry_status mc_registry_classes_statistic_entry(const mc_registry_classes *classes,
                                                       size_t index, int32_t *id,
                                                       const mc_statistic **out) {
    if (classes == NULL || id == NULL || out == NULL || (const void *)id == (const void *)out)
        return MC_REGISTRY_INVALID_ARGUMENT;
    const mc_statistic_map *map = classes->statistics.a;
    if (map == NULL || index >= map->size)
        return MC_REGISTRY_INDEX_OUT_OF_RANGE;
    const statistic_map_entry *entry = map->first;
    for (size_t i = 0; i < index; ++i)
        entry = entry->next;
    *id = entry->id;
    *out = entry->statistic;
    return MC_REGISTRY_OK;
}

size_t mc_statistic_list_size(const mc_statistic_list *list) {
    return list == NULL ? 0 : list->count;
}

mc_registry_status mc_statistic_list_get(const mc_statistic_list *list, size_t index,
                                         const mc_statistic **out) {
    if (list == NULL || out == NULL)
        return MC_REGISTRY_INVALID_ARGUMENT;
    if (index >= list->count)
        return MC_REGISTRY_INDEX_OUT_OF_RANGE;
    *out = list->elements[index];
    return MC_REGISTRY_OK;
}
