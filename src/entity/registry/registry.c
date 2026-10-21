#include "../classes/classes_internal.h"
#include "registry_internal.h"

#include <stdlib.h>

static const uint16_t name_units[24][12] = {{'A', 'r', 'r', 'o', 'w'},
                                            {'S', 'n', 'o', 'w', 'b', 'a', 'l', 'l'},
                                            {'I', 't', 'e', 'm'},
                                            {'P', 'a', 'i', 'n', 't', 'i', 'n', 'g'},
                                            {'M', 'o', 'b'},
                                            {'M', 'o', 'n', 's', 't', 'e', 'r'},
                                            {'C', 'r', 'e', 'e', 'p', 'e', 'r'},
                                            {'S', 'k', 'e', 'l', 'e', 't', 'o', 'n'},
                                            {'S', 'p', 'i', 'd', 'e', 'r'},
                                            {'G', 'i', 'a', 'n', 't'},
                                            {'Z', 'o', 'm', 'b', 'i', 'e'},
                                            {'S', 'l', 'i', 'm', 'e'},
                                            {'G', 'h', 'a', 's', 't'},
                                            {'P', 'i', 'g', 'Z', 'o', 'm', 'b', 'i', 'e'},
                                            {'P', 'i', 'g'},
                                            {'S', 'h', 'e', 'e', 'p'},
                                            {'C', 'o', 'w'},
                                            {'C', 'h', 'i', 'c', 'k', 'e', 'n'},
                                            {'S', 'q', 'u', 'i', 'd'},
                                            {'W', 'o', 'l', 'f'},
                                            {'P', 'r', 'i', 'm', 'e', 'd', 'T', 'n', 't'},
                                            {'F', 'a', 'l', 'l', 'i', 'n', 'g', 'S', 'a', 'n', 'd'},
                                            {'M', 'i', 'n', 'e', 'c', 'a', 'r', 't'},
                                            {'B', 'o', 'a', 't'}};

#define RECORD(kind, id, index, length, client, server)                                            \
    { MC_ENTITY_CLASS_KIND_##kind, id, {name_units[index], length}, client, server }
static const mc_entity_registry_record records[] = {
    RECORD(ARROW, 10, 0, 5, 46, 46),         RECORD(SNOWBALL, 11, 1, 8, 55, 55),
    RECORD(ITEM, 1, 2, 4, 63, 63),           RECORD(PAINTING, 9, 3, 8, 72, 72),
    RECORD(MOB, 48, 4, 3, 81, 81),           RECORD(MONSTER, 49, 5, 7, 90, 90),
    RECORD(CREEPER, 50, 6, 7, 99, 99),       RECORD(SKELETON, 51, 7, 8, 108, 108),
    RECORD(SPIDER, 52, 8, 6, 117, 117),      RECORD(GIANT, 53, 9, 5, 127, 126),
    RECORD(ZOMBIE, 54, 10, 6, 136, 135),     RECORD(SLIME, 55, 11, 5, 145, 144),
    RECORD(GHAST, 56, 12, 5, 154, 153),      RECORD(PIG_ZOMBIE, 57, 13, 9, 163, 162),
    RECORD(PIG, 90, 14, 3, 172, 171),        RECORD(SHEEP, 91, 15, 5, 181, 180),
    RECORD(COW, 92, 16, 3, 190, 189),        RECORD(CHICKEN, 93, 17, 7, 199, 198),
    RECORD(SQUID, 94, 18, 5, 208, 207),      RECORD(WOLF, 95, 19, 4, 217, 216),
    RECORD(PRIMED_TNT, 20, 20, 9, 226, 225), RECORD(FALLING_SAND, 21, 21, 11, 235, 234),
    RECORD(MINECART, 40, 22, 8, 244, 243),   RECORD(BOAT, 41, 23, 4, 253, 252)};
#undef RECORD

static mc_entity_registry_status finish(mc_entity_registry_error frame,
                                        mc_entity_registry_error *out) {
    if (out != NULL)
        *out = frame;
    return frame.status;
}

static mc_entity_registry_status fail(mc_entity_registry_error *frame,
                                      mc_entity_registry_status status, const char *operation) {
    frame->status = frame->cause = status;
    frame->operation = operation;
    return status;
}

static void *request(mc_entity_classes *classes, size_t bytes, mc_entity_registry_error *frame) {
    if (classes->allocated_bytes > classes->limits.max_allocation_bytes ||
        bytes > classes->limits.max_allocation_bytes - classes->allocated_bytes) {
        fail(frame, MC_ENTITY_REGISTRY_LIMIT, "EntityList native map allocation budget");
        return NULL;
    }
    void *pointer = malloc(bytes);
    if (pointer == NULL) {
        fail(frame, MC_ENTITY_REGISTRY_ALLOCATION_FAILED, "EntityList native map allocation");
        return NULL;
    }
    classes->allocated_bytes += bytes;
    return pointer;
}

mc_entity_registry_status mc_entity_classes_initialize_registry(mc_entity_classes *classes,
                                                                mc_entity_registry_error *out) {
    mc_entity_registry_error frame = {0};
    if (classes == NULL)
        return finish((mc_entity_registry_error){.status = MC_ENTITY_REGISTRY_INVALID_ARGUMENT,
                                                 .cause = MC_ENTITY_REGISTRY_INVALID_ARGUMENT,
                                                 .operation = "EntityList class owner"},
                      out);
    if (classes->registry_phase == MC_ENTITY_CLASS_READY)
        return finish(frame, out);
    if (classes->registry_phase == MC_ENTITY_CLASS_FAILED) {
        frame = classes->registry_failure;
        frame.status = MC_ENTITY_REGISTRY_CLASS_FAILED;
        return finish(frame, out);
    }
    if (classes->registry_phase != MC_ENTITY_CLASS_COLD) {
        fail(&frame, MC_ENTITY_REGISTRY_BUSY, "EntityList class initialization reentry");
        return finish(frame, out);
    }
    classes->registry_phase = MC_ENTITY_CLASS_INITIALIZING;
    for (size_t i = 0; i < 4; ++i) {
        frame.initializer_bytecode = (unsigned int)i * 10u;
        mc_entity_registry_map *map = request(classes, sizeof(*map), &frame);
        if (map == NULL)
            goto failed;
        *map = (mc_entity_registry_map){0};
        classes->registry_maps[i] = map; /* a7, b17, c27, d37. */
    }
    for (size_t i = 0; i < sizeof(records) / sizeof(records[0]); ++i) {
        const mc_entity_registry_record *record = &records[i];
        frame.initializer_bytecode =
            classes->source == MC_ENTITY_CLIENT ? record->client_pc : record->server_pc;
        const mc_entity_class_literal *literal = NULL;
        frame.class_cause = mc_entity_class_literal_get(classes, record->kind, &literal);
        if (frame.class_cause != MC_ENTITY_OK) {
            fail(&frame, MC_ENTITY_REGISTRY_STATE_ERROR, "EntityList canonical Class literal");
            goto failed;
        }
        static const unsigned int puts[] = {5, 16, 30, 44};
        for (size_t map_index = 0; map_index < 4; ++map_index) {
            frame.registration_bytecode = puts[map_index];
            mc_entity_registry_node *node = request(classes, sizeof(*node), &frame);
            if (node == NULL)
                goto failed;
            *node = (mc_entity_registry_node){literal, record, NULL};
            mc_entity_registry_map *map = classes->registry_maps[map_index];
            if (map->tail == NULL)
                map->head = node;
            else
                map->tail->next = node;
            map->tail = node;
            ++map->count;
        }
    }
    classes->registry_phase = MC_ENTITY_CLASS_READY;
    return finish((mc_entity_registry_error){0}, out);
failed:
    classes->registry_failure = frame;
    classes->registry_phase = MC_ENTITY_CLASS_FAILED;
    return finish(frame, out);
}

mc_entity_registry_status mc_entity_registry_inspect(const mc_entity_classes *classes,
                                                     mc_entity_registry_view *out) {
    if (classes == NULL || out == NULL)
        return MC_ENTITY_REGISTRY_INVALID_ARGUMENT;
    mc_entity_registry_view view = {.phase = classes->registry_phase,
                                    .first_failure = classes->registry_failure};
    for (size_t i = 0; i < 4; ++i) {
        view.maps[i] = classes->registry_maps[i];
        view.counts[i] = classes->registry_maps[i] == NULL ? 0 : classes->registry_maps[i]->count;
    }
    *out = view;
    return MC_ENTITY_REGISTRY_OK;
}

static bool names_equal(mc_nbt_string left, mc_nbt_string right) {
    if (left.length != right.length || (left.length != 0 && left.units == NULL))
        return false;
    for (size_t i = 0; i < left.length; ++i)
        if (left.units[i] != right.units[i])
            return false;
    return true;
}

static mc_entity_registry_status lookup(mc_entity_classes *classes, mc_nbt_string name,
                                        const mc_entity_class_literal *type, int32_t id,
                                        size_t map_index, const mc_entity_registry_node **out,
                                        mc_entity_registry_error *error) {
    mc_entity_registry_status status = mc_entity_classes_initialize_registry(classes, error);
    if (status != MC_ENTITY_REGISTRY_OK)
        return status;
    for (const mc_entity_registry_node *node = classes->registry_maps[map_index]->head;
         node != NULL; node = node->next) {
        if ((map_index == 0 && names_equal(name, node->record->name)) ||
            (map_index == 1 && type == node->type) || (map_index == 2 && id == node->record->id)) {
            *out = node;
            return MC_ENTITY_REGISTRY_OK;
        }
    }
    *out = NULL;
    return MC_ENTITY_REGISTRY_OK;
}

mc_entity_registry_status mc_entity_registry_named_class(mc_entity_classes *classes,
                                                         mc_nbt_string name,
                                                         const mc_entity_class_literal **out,
                                                         mc_entity_registry_error *error) {
    if (out == NULL || (name.length != 0 && name.units == NULL))
        return finish((mc_entity_registry_error){.status = MC_ENTITY_REGISTRY_INVALID_ARGUMENT,
                                                 .cause = MC_ENTITY_REGISTRY_INVALID_ARGUMENT,
                                                 .operation = "EntityList name lookup arguments"},
                      error);
    const mc_entity_registry_node *node = NULL;
    mc_entity_registry_status status = lookup(classes, name, NULL, 0, 0, &node, error);
    if (status == MC_ENTITY_REGISTRY_OK)
        *out = node == NULL ? NULL : node->type;
    return status;
}

mc_entity_registry_status mc_entity_registry_class_name(mc_entity_classes *classes,
                                                        const mc_entity_class_literal *type,
                                                        mc_nbt_string *out,
                                                        mc_entity_registry_error *error) {
    if (out == NULL)
        return finish((mc_entity_registry_error){.status = MC_ENTITY_REGISTRY_INVALID_ARGUMENT,
                                                 .cause = MC_ENTITY_REGISTRY_INVALID_ARGUMENT,
                                                 .operation = "EntityList Class lookup output"},
                      error);
    const mc_entity_registry_node *node = NULL;
    mc_entity_registry_status status =
        lookup(classes, (mc_nbt_string){0}, type, 0, 1, &node, error);
    if (status == MC_ENTITY_REGISTRY_OK)
        *out = node == NULL ? (mc_nbt_string){0} : node->record->name;
    return status;
}

mc_entity_registry_status mc_entity_registry_numeric_class(mc_entity_classes *classes, int32_t id,
                                                           const mc_entity_class_literal **out,
                                                           mc_entity_registry_error *error) {
    if (out == NULL)
        return finish((mc_entity_registry_error){.status = MC_ENTITY_REGISTRY_INVALID_ARGUMENT,
                                                 .cause = MC_ENTITY_REGISTRY_INVALID_ARGUMENT,
                                                 .operation = "EntityList numeric lookup output"},
                      error);
    const mc_entity_registry_node *node = NULL;
    mc_entity_registry_status status =
        lookup(classes, (mc_nbt_string){0}, NULL, id, 2, &node, error);
    if (status == MC_ENTITY_REGISTRY_OK)
        *out = node == NULL ? NULL : node->type;
    return status;
}

void mc_entity_registry_dispose(mc_entity_classes *classes) {
    for (size_t i = 0; i < 4; ++i) {
        mc_entity_registry_map *map = classes->registry_maps[i];
        if (map == NULL)
            continue;
        mc_entity_registry_node *node = map->head;
        while (node != NULL) {
            mc_entity_registry_node *next = node->next;
            free(node);
            classes->allocated_bytes -= sizeof(*node);
            node = next;
        }
        free(map);
        classes->allocated_bytes -= sizeof(*map);
        classes->registry_maps[i] = NULL;
    }
}
