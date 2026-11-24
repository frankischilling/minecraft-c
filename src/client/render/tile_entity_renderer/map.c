#include "internal.h"

#include "../../../core/java/object_identity_profile_internal.h"
#include "../../../world/runtime/tile_entities/identity_internal.h"

#include <stdint.h>
#include <stdlib.h>

static bool same_class_namespace(mc_client_tile_renderer_dispatcher *,
                                 const mc_tile_entity_class_literal *,
                                 mc_client_tile_renderer_error *);

tile_renderer_map_node *tile_renderer_map_node_find(const tile_renderer_map *map,
                                                    const mc_tile_entity_class_literal *key) {
    if (map == NULL || map->capacity == 0)
        return NULL;
    /* Passive diagnostic lookup must not acquire an Object identity hash. */
    for (size_t i = 0; i < map->capacity; ++i)
        for (tile_renderer_map_node *p = map->buckets[i]; p != NULL; p = p->next)
            if (p->key == key)
                return p;
    return NULL;
}

static bool class_hash(mc_client_tile_renderer_dispatcher *m,
                       const mc_tile_entity_class_literal *key, uint32_t *out, const char *method,
                       const char *descriptor, int32_t pc, mc_client_tile_renderer_error *e) {
    if (key == NULL) {
        *out = 0; /* HashMap.hash(null) does not invoke Object.hashCode. */
        return true;
    }
    uint32_t hash = 0;
    e->tile_entity = mc_tile_entity_class_literal_identity_hash(
        key, mc_java_random_profile_main_identity(m->classes->profile), &hash);
    if (e->tile_entity != MC_TILE_ENTITY_OK) {
        tile_renderer_fail(e, MC_CLIENT_TILE_RENDERER_TILE_ENTITY_FAILED,
                           "LL actual Class identity hash", "ll", method, descriptor, pc, true);
        return false;
    }
    *out = hash ^ (hash >> 16u);
    return true;
}

static tile_renderer_map_node *find_hashed(const tile_renderer_map *map,
                                           const mc_tile_entity_class_literal *key, uint32_t hash) {
    for (tile_renderer_map_node *p = map->buckets[hash & (map->capacity - 1)]; p != NULL;
         p = p->next)
        if (p->hash == hash && p->key == key)
            return p;
    return NULL;
}

/* Only nine canonical same-namespace Class keys can enter this no-remove Map;
 * Source LL never puts NULL. Default capacity16/threshold12 cannot resize by
 * size here. Nine colliding insertions can ask treeifyBin to grow16->32, but
 * cannot reach a tree. This is not a general arbitrary-key HashMap backend. */
static bool resize(mc_client_tile_renderer_dispatcher *m, const char *method,
                   const char *descriptor, int32_t pc, mc_client_tile_renderer_error *e) {
    tile_renderer_map *map = m->map;
    if (map->capacity != 0 && map->capacity != 16) {
        tile_renderer_fail(e, MC_CLIENT_TILE_RENDERER_INVALID_PHASE,
                           "LL finite canonical Class Map resize history", "ll", method, descriptor,
                           pc, true);
        return false;
    }
    const size_t old_capacity = map->capacity;
    const size_t capacity = old_capacity == 0 ? 16 : old_capacity * 2;
    /* HashMap.resize stores its threshold before allocating the new array;
     * a native allocation refusal retains that completed scalar effect. */
    map->threshold = capacity - capacity / 4;
    tile_renderer_map_node **buckets = tile_renderer_allocate(
        m->classes, capacity * sizeof(*buckets), "ll", method, descriptor, pc, e);
    if (buckets == NULL)
        return false;
    for (size_t i = 0; i < old_capacity; ++i) {
        tile_renderer_map_node *lo = NULL, *lo_tail = NULL, *hi = NULL, *hi_tail = NULL;
        for (tile_renderer_map_node *p = map->buckets[i]; p != NULL;) {
            tile_renderer_map_node *next = p->next;
            if ((p->hash & old_capacity) == 0) {
                if (lo_tail == NULL)
                    lo = p;
                else
                    lo_tail->next = p;
                lo_tail = p;
            } else {
                if (hi_tail == NULL)
                    hi = p;
                else
                    hi_tail->next = p;
                hi_tail = p;
            }
            p = next;
        }
        if (lo_tail != NULL)
            lo_tail->next = NULL;
        if (hi_tail != NULL)
            hi_tail->next = NULL;
        buckets[i] = lo;
        buckets[i + old_capacity] = hi;
    }
    m->classes->bytes -= old_capacity * sizeof(*buckets);
    free(map->buckets);
    map->buckets = buckets;
    map->capacity = capacity;
    return true;
}

bool tile_renderer_map_put(mc_client_tile_renderer_dispatcher *m,
                           const mc_tile_entity_class_literal *key, mc_client_tile_renderer *value,
                           int32_t pc, bool constructor, mc_client_tile_renderer_error *e) {
    tile_renderer_map *map = m->map;
    const char *method = constructor ? "<init>" : "a";
    const char *descriptor = constructor ? "()V" : "(Ljava/lang/Class;)Lje;";
    if (key == NULL || !same_class_namespace(m, key, e)) {
        if (key == NULL)
            tile_renderer_fail(e, MC_CLIENT_TILE_RENDERER_INVALID_ARGUMENT,
                               "LL never puts a null Class key", "ll", method, descriptor, pc,
                               true);
        return false;
    }
    uint32_t hash = 0;
    if (!class_hash(m, key, &hash, method, descriptor, pc, e))
        return false;
    if (m->put_calls == SIZE_MAX) {
        tile_renderer_fail(e, MC_CLIENT_TILE_RENDERER_LIMIT, "LL native Map put counter bound",
                           "ll", method, descriptor, pc, true);
        return false;
    }
    if (map->capacity == 0 && !resize(m, method, descriptor, pc, e))
        return false;
    tile_renderer_map_node *existing = find_hashed(map, key, hash);
    if (existing != NULL) {
        existing->value = value; /* Null-value replacement preserves this actual node. */
    } else {
        if (map->size >= (size_t)MC_TILE_ENTITY_CLASS_KIND_COUNT) {
            tile_renderer_fail(e, MC_CLIENT_TILE_RENDERER_INVALID_PHASE,
                               "LL finite canonical Class Map key bound", "ll", method, descriptor,
                               pc, true);
            return false;
        }
        tile_renderer_map_node *p =
            tile_renderer_allocate(m->classes, sizeof(*p), "ll", method, descriptor, pc, e);
        if (p == NULL)
            return false;
        const size_t b = hash & (map->capacity - 1);
        p->key = key;
        p->value = value;
        p->hash = hash;
        if (map->buckets[b] == NULL)
            map->buckets[b] = p;
        else {
            size_t bin_count = 0;
            tile_renderer_map_node *tail = map->buckets[b];
            while (tail->next != NULL) {
                tail = tail->next;
                ++bin_count;
            }
            tail->next = p;
            /* Actual putVal links the new node before treeifyBin/resize and
             * increments modCount/size only after that child returns. */
            if (bin_count >= 7 && !resize(m, method, descriptor, pc, e))
                return false;
        }
        ++map->mod_count;
        if (++map->size > map->threshold && !resize(m, method, descriptor, pc, e))
            return false;
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
    tile_renderer_map_node *node = NULL;
    /* HashMap.getNode skips hash(key) entirely while its table is absent. */
    if (m->map->capacity != 0) {
        uint32_t hash = 0;
        if (!class_hash(m, key, &hash, "a", "(Ljava/lang/Class;)Lje;", 5, e))
            return false;
        node = find_hashed(m->map, key, hash); /* Map.get5. */
    }
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

bool tile_renderer_lookup_tile_entered(mc_client_tile_renderer_dispatcher *m,
                                       const mc_tile_entity *tile, mc_client_tile_renderer **out,
                                       mc_client_tile_renderer_error *e) {
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
        const bool ok = tile_renderer_lookup_tile_entered(m, tile, &value, &e);
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
        const bool ok =
            tile_renderer_lookup_tile_entered(m, tile, &value, &e); /* LL.a(OW) calls b2. */
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
