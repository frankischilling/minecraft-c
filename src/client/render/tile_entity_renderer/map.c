#include "internal.h"

#include <stdint.h>
#include <stdlib.h>

/* Canonical pointer identity, not name/profile/kind equality. This native hash
 * and iteration order do not claim JDK identityHashCode/bucket equivalence. */
static size_t bucket(const mc_tile_entity_class_literal *key, size_t capacity) {
    uintptr_t value = (uintptr_t)key;
    value ^= value >> 7;
    value ^= value >> 17;
    return (size_t)value & (capacity - 1);
}

tile_renderer_map_node *tile_renderer_map_node_find(const tile_renderer_map *map,
                                                    const mc_tile_entity_class_literal *key) {
    if (map == NULL || map->capacity == 0)
        return NULL;
    for (tile_renderer_map_node *p = map->buckets[bucket(key, map->capacity)]; p != NULL;
         p = p->next)
        if (p->key == key)
            return p;
    return NULL;
}

bool tile_renderer_map_put(mc_client_tile_renderer_dispatcher *m,
                           const mc_tile_entity_class_literal *key, mc_client_tile_renderer *value,
                           int32_t pc, bool constructor, mc_client_tile_renderer_error *e) {
    mc_client_tile_renderer_classes *c = m->classes;
    tile_renderer_map *map = m->map;
    const char *method = constructor ? "<init>" : "a";
    const char *descriptor = constructor ? "()V" : "(Ljava/lang/Class;)Lje;";
    if (m->put_calls == SIZE_MAX) {
        tile_renderer_fail(e, MC_CLIENT_TILE_RENDERER_LIMIT, "LL native Map put counter bound",
                           "ll", method, descriptor, pc, true);
        return false;
    }
    tile_renderer_map_node *existing = tile_renderer_map_node_find(map, key);
    if (existing != NULL) {
        existing->value = value; /* Null-value replacement preserves this actual node. */
    } else {
        if (map->capacity == 0 || map->size >= map->threshold) {
            if (map->capacity > SIZE_MAX / 2) {
                tile_renderer_fail(e, MC_CLIENT_TILE_RENDERER_LIMIT, "LL native Map capacity bound",
                                   "ll", method, descriptor, pc, true);
                return false;
            }
            const size_t capacity = map->capacity == 0 ? 16 : map->capacity * 2;
            if (capacity > SIZE_MAX / sizeof(*map->buckets)) {
                tile_renderer_fail(e, MC_CLIENT_TILE_RENDERER_LIMIT,
                                   "LL native Map backing size bound", "ll", method, descriptor, pc,
                                   true);
                return false;
            }
            tile_renderer_map_node **buckets = tile_renderer_allocate(
                c, capacity * sizeof(*buckets), "ll", method, descriptor, pc, e);
            if (buckets == NULL)
                return false;
            for (size_t i = 0; i < map->capacity; ++i) {
                tile_renderer_map_node *p = map->buckets[i];
                while (p != NULL) {
                    tile_renderer_map_node *next = p->next;
                    const size_t b = bucket(p->key, capacity);
                    p->next = buckets[b];
                    buckets[b] = p;
                    p = next;
                }
            }
            c->bytes -= map->capacity * sizeof(*buckets);
            free(map->buckets);
            map->buckets = buckets;
            map->capacity = capacity;
            map->threshold = capacity - capacity / 4;
        }
        tile_renderer_map_node *p =
            tile_renderer_allocate(c, sizeof(*p), "ll", method, descriptor, pc, e);
        if (p == NULL)
            return false;
        const size_t b = bucket(key, map->capacity);
        p->key = key;
        p->value = value;
        p->next = map->buckets[b];
        map->buckets[b] = p;
        ++map->size;
        ++map->mod_count;
    }
    ++m->put_calls;
    m->last_put_pc = pc;
    if (constructor)
        m->last_constructor_pc = pc;
    return true;
}

void tile_renderer_map_retire(mc_client_tile_renderer_classes *c, tile_renderer_map **slot) {
    tile_renderer_map *map = *slot;
    if (map == NULL)
        return;
    for (size_t i = 0; i < map->capacity; ++i) {
        tile_renderer_map_node *p = map->buckets[i];
        while (p != NULL) {
            tile_renderer_map_node *next = p->next;
            c->bytes -= sizeof(*p);
            free(p);
            p = next;
        }
    }
    c->bytes -= map->capacity * sizeof(*map->buckets);
    free(map->buckets);
    c->bytes -= sizeof(*map);
    free(map);
    *slot = NULL;
}

static bool same_class_namespace(mc_client_tile_renderer_dispatcher *m,
                                 const mc_tile_entity_class_literal *key,
                                 mc_client_tile_renderer_error *e) {
    if (key == NULL)
        return true;
    mc_tile_entity_class_literal_view view;
    e->tile_entity = mc_tile_entity_class_literal_inspect(key, &view);
    if (e->tile_entity != MC_TILE_ENTITY_OK || view.owner != m->classes->tile_classes) {
        tile_renderer_fail(e, MC_CLIENT_TILE_RENDERER_INVALID_ARGUMENT,
                           "LL actual Class from same TileEntity namespace", NULL, NULL, NULL, -1,
                           true);
        return false;
    }
    return true;
}

static bool source_admit(mc_client_tile_renderer_dispatcher *m, mc_client_tile_renderer_error *e) {
    if (!tile_renderer_passive_admit(m, e))
        return false;
    if (m->classes->phase != MC_CLIENT_TILE_RENDERER_READY || m->classes->a != m ||
        m->map == NULL) {
        tile_renderer_fail(e, MC_CLIENT_TILE_RENDERER_INVALID_PHASE,
                           "LL source receiver requires completed class initialization", NULL, NULL,
                           NULL, -1, true);
        return false;
    }
    return true;
}

static bool lookup_class(mc_client_tile_renderer_dispatcher *m,
                         const mc_tile_entity_class_literal *key, mc_client_tile_renderer **out,
                         mc_client_tile_renderer_error *e) {
    tile_renderer_map_node *node = tile_renderer_map_node_find(m->map, key); /* Map.get5. */
    mc_client_tile_renderer *value = node == NULL ? NULL : node->value;
    if (value == NULL && key != m->classes->base) { /* Actual Class comparison21. */
        if (key == NULL) {
            tile_renderer_fail(e, MC_CLIENT_TILE_RENDERER_NULL_REFERENCE,
                               "actual null Class.getSuperclass receiver", "ll", "a",
                               "(Ljava/lang/Class;)Lje;", 26, false);
            return false;
        }
        const mc_tile_entity_class_literal *parent = NULL;
        e->tile_entity = mc_tile_entity_class_literal_superclass(key, &parent);
        if (e->tile_entity != MC_TILE_ENTITY_OK) {
            tile_renderer_fail(e, MC_CLIENT_TILE_RENDERER_TILE_ENTITY_FAILED,
                               "LL actual Class.getSuperclass child", "ll", "a",
                               "(Ljava/lang/Class;)Lje;", 26, true);
            return false;
        }
        if (!lookup_class(m, parent, &value, e)) /* Actual recursive call29. */
            return false;
        if (!tile_renderer_map_put(m, key, value, 39, false, e))
            return false;
    }
    *out = value;
    return true;
}

static bool lookup_tile(mc_client_tile_renderer_dispatcher *m, const mc_tile_entity *tile,
                        mc_client_tile_renderer **out, mc_client_tile_renderer_error *e) {
    if (tile == NULL) {
        *out = NULL; /* LL.b(OW) returns5 before Object.getClass8. */
        return true;
    }
    const mc_tile_entity_class_literal *key = NULL;
    e->tile_entity = mc_tile_entity_get_class(tile, &key);
    if (e->tile_entity != MC_TILE_ENTITY_OK) {
        tile_renderer_fail(e, MC_CLIENT_TILE_RENDERER_TILE_ENTITY_FAILED,
                           "LL actual TileEntity Object.getClass child", "ll", "b", "(Low;)Lje;", 8,
                           true);
        return false;
    }
    if (!same_class_namespace(m, key, e))
        return false;
    return lookup_class(m, key, out, e); /* Actual b(OW) recursive child11. */
}

mc_client_tile_renderer_status mc_client_tile_renderer_lookup_class(
    mc_client_tile_renderer_dispatcher *m, const mc_tile_entity_class_literal *key,
    mc_client_tile_renderer **out, mc_client_tile_renderer_error *error) {
    mc_client_tile_renderer_error e;
    tile_renderer_error_init(&e);
    if (out == NULL)
        tile_renderer_fail(&e, MC_CLIENT_TILE_RENDERER_INVALID_ARGUMENT, "LL Class lookup output",
                           NULL, NULL, NULL, -1, true);
    else if (source_admit(m, &e) && same_class_namespace(m, key, &e)) {
        mc_client_tile_renderer *value = NULL;
        m->active = m->classes->active = true;
        const bool ok = lookup_class(m, key, &value, &e);
        m->active = m->classes->active = false;
        if (ok)
            *out = value;
    }
    if (error != NULL)
        *error = e;
    return e.status;
}

mc_client_tile_renderer_status
mc_client_tile_renderer_lookup_tile(mc_client_tile_renderer_dispatcher *m,
                                    const mc_tile_entity *tile, mc_client_tile_renderer **out,
                                    mc_client_tile_renderer_error *error) {
    mc_client_tile_renderer_error e;
    tile_renderer_error_init(&e);
    if (out == NULL)
        tile_renderer_fail(&e, MC_CLIENT_TILE_RENDERER_INVALID_ARGUMENT,
                           "LL TileEntity lookup output", NULL, NULL, NULL, -1, true);
    else if (source_admit(m, &e)) {
        mc_client_tile_renderer *value = NULL;
        m->active = m->classes->active = true;
        const bool ok = lookup_tile(m, tile, &value, &e);
        m->active = m->classes->active = false;
        if (ok)
            *out = value;
    }
    if (error != NULL)
        *error = e;
    return e.status;
}

mc_client_tile_renderer_status
mc_client_tile_renderer_has_renderer(mc_client_tile_renderer_dispatcher *m,
                                     const mc_tile_entity *tile, bool *out,
                                     mc_client_tile_renderer_error *error) {
    mc_client_tile_renderer_error e;
    tile_renderer_error_init(&e);
    if (out == NULL)
        tile_renderer_fail(&e, MC_CLIENT_TILE_RENDERER_INVALID_ARGUMENT, "LL eligibility output",
                           NULL, NULL, NULL, -1, true);
    else if (source_admit(m, &e)) {
        mc_client_tile_renderer *value = NULL;
        m->active = m->classes->active = true;
        const bool ok = lookup_tile(m, tile, &value, &e); /* LL.a(OW) calls b2. */
        m->active = m->classes->active = false;
        if (ok)
            *out = value != NULL;
    }
    if (error != NULL)
        *error = e;
    return e.status;
}

static mc_client_tile_renderer_map_entry entry_view(const tile_renderer_map_node *node) {
    return node == NULL ? (mc_client_tile_renderer_map_entry){0}
                        : (mc_client_tile_renderer_map_entry){true, node, node->key, node->value};
}

mc_client_tile_renderer_status mc_client_tile_renderer_entry(mc_client_tile_renderer_dispatcher *m,
                                                             size_t index,
                                                             mc_client_tile_renderer_map_entry *out,
                                                             mc_client_tile_renderer_error *error) {
    mc_client_tile_renderer_error e;
    tile_renderer_error_init(&e);
    if (out == NULL)
        tile_renderer_fail(&e, MC_CLIENT_TILE_RENDERER_INVALID_ARGUMENT, "LL Map entry output",
                           NULL, NULL, NULL, -1, true);
    else if (tile_renderer_passive_admit(m, &e)) {
        if (m->map == NULL || index >= m->map->size)
            tile_renderer_fail(&e, MC_CLIENT_TILE_RENDERER_INVALID_ARGUMENT, "LL Map entry index",
                               NULL, NULL, NULL, -1, true);
        else {
            size_t current = 0;
            for (size_t i = 0; i < m->map->capacity; ++i)
                for (tile_renderer_map_node *p = m->map->buckets[i]; p != NULL; p = p->next)
                    if (current++ == index) {
                        *out = entry_view(p);
                        goto done;
                    }
        }
    }
done:
    if (error != NULL)
        *error = e;
    return e.status;
}

mc_client_tile_renderer_status mc_client_tile_renderer_map_find(
    mc_client_tile_renderer_dispatcher *m, const mc_tile_entity_class_literal *key,
    mc_client_tile_renderer_map_entry *out, mc_client_tile_renderer_error *error) {
    mc_client_tile_renderer_error e;
    tile_renderer_error_init(&e);
    if (out == NULL)
        tile_renderer_fail(&e, MC_CLIENT_TILE_RENDERER_INVALID_ARGUMENT, "LL Map find output", NULL,
                           NULL, NULL, -1, true);
    else if (tile_renderer_passive_admit(m, &e) && same_class_namespace(m, key, &e)) {
        if (m->map == NULL)
            tile_renderer_fail(&e, MC_CLIENT_TILE_RENDERER_INVALID_PHASE,
                               "LL Map has not been published", NULL, NULL, NULL, -1, true);
        else
            *out = entry_view(tile_renderer_map_node_find(m->map, key));
    }
    if (error != NULL)
        *error = e;
    return e.status;
}
