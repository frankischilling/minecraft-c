#include "internal.h"
#include <stdint.h>
#include <stdlib.h>

/* Concrete identity-key native HashMap. Bucket order/hash are native transport,
 * not asserted to equal JDK identityHashCode/HashMap iteration. All keys are
 * the actual passive canonical Class cells. No source name/string hashing. */
static size_t bucket(const mc_entity_class_literal *key, size_t capacity) {
    uintptr_t value = (uintptr_t)key;
    value ^= value >> 7;
    value ^= value >> 17;
    return (size_t)value & (capacity - 1);
}

bool manager_map_put(mc_client_render_manager *m, const mc_entity_class_literal *key,
                     mc_client_managed_renderer *renderer, int32_t pc) {
    manager_map *map = m->map;
    mc_client_render_manager_classes *c = m->classes;
    if (map->capacity == 0 || map->size + 1 > map->capacity * 3 / 4) {
        const size_t capacity = map->capacity == 0 ? 16 : map->capacity * 2;
        manager_map_entry **buckets = manager_allocate(c, capacity * sizeof(*buckets), "th", pc);
        if (buckets == NULL)
            return false;
        for (size_t i = 0; i < map->capacity; ++i) {
            manager_map_entry *p = map->buckets[i];
            while (p != NULL) {
                manager_map_entry *next = p->next;
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
    }
    const size_t b = bucket(key, map->capacity);
    for (manager_map_entry *p = map->buckets[b]; p != NULL; p = p->next)
        if (p->key == key) {
            /* This constructor's 28 distinct keys never take replacement. */
            manager_fail(&c->failure, MC_CLIENT_MANAGER_INVALID_PHASE,
                         "TH repeated source Class key", NULL, NULL, NULL, -1, true);
            return false;
        }
    manager_map_entry *p = manager_allocate(c, sizeof(*p), "th", pc);
    if (p == NULL)
        return false;
    p->key = key;
    p->owner = renderer;
    p->value = renderer->bb != NULL ? (const void *)renderer->bb : (const void *)renderer;
    p->next = map->buckets[b];
    map->buckets[b] = p;
    ++map->size;
    ++m->put_count;
    m->last_put_pc = pc;
    m->last_constructor_pc = pc;
    return true;
}

mc_client_manager_status mc_client_render_manager_entry(mc_client_render_manager *m, size_t index,
                                                        const mc_entity_class_literal **key,
                                                        mc_client_managed_renderer **renderer,
                                                        mc_client_manager_error *error) {
    mc_client_manager_error e;
    manager_error_init(&e);
    if (m == NULL || key == NULL || renderer == NULL)
        manager_fail(&e, MC_CLIENT_MANAGER_INVALID_ARGUMENT, "manager actual Map entry output",
                     NULL, NULL, NULL, -1, true);
    else if (m->active || m->classes->active)
        manager_fail(&e, MC_CLIENT_MANAGER_BUSY, "manager Map active operation", NULL, NULL, NULL,
                     -1, true);
    else if (m->classes->phase == MC_CLIENT_MANAGER_RETIRING || m->map == NULL ||
             index >= m->map->size)
        manager_fail(&e, MC_CLIENT_MANAGER_INVALID_PHASE, "manager Map entry admission", NULL, NULL,
                     NULL, -1, true);
    else {
        size_t current = 0;
        for (size_t i = 0; i < m->map->capacity; ++i)
            for (manager_map_entry *p = m->map->buckets[i]; p != NULL; p = p->next)
                if (current++ == index) {
                    *key = p->key;
                    *renderer = p->owner;
                    goto done;
                }
    }
done:
    if (error != NULL)
        *error = e;
    return e.status;
}
