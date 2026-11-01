/* The Store list owns every node and defines all ordinals. This optional,
 * private index owns only integer state identities and borrowed node pointers.
 * No caller-owned Chunk payload, cache descriptor or coordinate is indexed. */
#include <stdint.h>

#define STORE_CHUNK_LOOKUP_MIN_COUNT 8u
#define STORE_CHUNK_LOOKUP_PEAK_LIMIT (256u * 1024u)

typedef struct store_chunk_lookup_slot {
    uintptr_t state_key;
    store_chunk *entry;
} store_chunk_lookup_slot;

typedef struct store_chunk_lookup {
    size_t capacity, count;
    store_chunk_lookup_slot slots[];
} store_chunk_lookup;

static size_t store_chunk_lookup_base_bytes(const mc_world_store *store) {
    return sizeof(*store) + (store->source == NULL ? 0 : sizeof(*store->source));
}

static size_t store_chunk_lookup_limit(const mc_world_store *store) {
    if (store->source == NULL)
        return STORE_CHUNK_LOOKUP_PEAK_LIMIT;
    const size_t base = store_chunk_lookup_base_bytes(store);
    const mc_world_store_source_classes *classes = store->source->classes;
    if (classes == NULL || classes->options.max_owner_bytes < base)
        return 0;
    const size_t available = classes->options.max_owner_bytes - base;
    return available < STORE_CHUNK_LOOKUP_PEAK_LIMIT ? available : STORE_CHUNK_LOOKUP_PEAK_LIMIT;
}

static size_t store_chunk_lookup_bytes(const store_chunk_lookup *index) {
    return index == NULL ? 0 : sizeof(*index) + index->capacity * sizeof(*index->slots);
}

static size_t store_chunk_lookup_hash(uintptr_t key, size_t mask) {
    /* uintptr_t preserves object identity. No ordering or dereference of the
     * caller's state is needed, including for a foreign live identity. */
    uint64_t bits = (uint64_t)key;
    bits ^= bits >> 33u;
    bits *= UINT64_C(0xff51afd7ed558ccd);
    bits ^= bits >> 33u;
    bits *= UINT64_C(0xc4ceb9fe1a85ec53);
    bits ^= bits >> 33u;
    return (size_t)bits & mask;
}

static void store_chunk_lookup_clear(mc_world_store *store) {
    free(store->chunk_lookup);
    store->chunk_lookup = NULL;
}

static void store_chunk_lookup_disable(mc_world_store *store) {
    store_chunk_lookup_clear(store);
    store->chunk_lookup_disabled = true;
}

static bool store_chunk_lookup_insert(store_chunk_lookup *index, uintptr_t key,
                                      store_chunk *entry) {
    size_t slot = store_chunk_lookup_hash(key, index->capacity - 1u);
    for (size_t i = 0; i < index->capacity; ++i) {
        if (index->slots[slot].entry == NULL) {
            index->slots[slot] = (store_chunk_lookup_slot){key, entry};
            ++index->count;
            return true;
        }
        if (index->slots[slot].state_key == key)
            return false; /* A private invariant failure falls back to the list. */
        slot = (slot + 1u) & (index->capacity - 1u);
    }
    return false;
}

static store_chunk *store_chunk_lookup_find(const store_chunk_lookup *index, uintptr_t key) {
    size_t slot = store_chunk_lookup_hash(key, index->capacity - 1u);
    for (size_t i = 0; i < index->capacity; ++i) {
        const store_chunk_lookup_slot *value = &index->slots[slot];
        if (value->entry == NULL)
            return NULL;
        if (value->state_key == key)
            return value->entry;
        slot = (slot + 1u) & (index->capacity - 1u);
    }
    return NULL;
}

static void store_chunk_lookup_publish(mc_world_store *store, store_chunk *entry) {
    if (store->chunk_lookup_disabled)
        return;
    store_chunk_lookup *old = store->chunk_lookup;
    if (old == NULL && store->chunk_count < STORE_CHUNK_LOOKUP_MIN_COUNT)
        return;
    mc_chunk_state *state = store_chunk_state(entry);
    if (state == NULL) {
        store_chunk_lookup_disable(store);
        return;
    }
    if (old != NULL && store->chunk_count <= old->capacity / 2u) {
        if (old->count != store->chunk_count - 1u ||
            !store_chunk_lookup_insert(old, (uintptr_t)state, entry))
            store_chunk_lookup_disable(store);
        return;
    }
    size_t capacity = 16;
    while (capacity / 2u < store->chunk_count) {
        if (capacity > SIZE_MAX / 2u) {
            store_chunk_lookup_disable(store);
            return;
        }
        capacity *= 2u;
    }
    if (capacity > (SIZE_MAX - sizeof(store_chunk_lookup)) / sizeof(store_chunk_lookup_slot)) {
        store_chunk_lookup_disable(store);
        return;
    }
    const size_t bytes = sizeof(store_chunk_lookup) + capacity * sizeof(store_chunk_lookup_slot);
    const size_t old_bytes = store_chunk_lookup_bytes(old);
    const size_t limit = store_chunk_lookup_limit(store);
    if (old_bytes > limit || bytes > limit - old_bytes) {
        store_chunk_lookup_disable(store);
        return;
    }
    store_chunk_lookup *next = calloc(1, bytes);
    if (next == NULL) {
        store_chunk_lookup_disable(store);
        return;
    }
    const size_t peak = old_bytes + bytes;
    if (peak > store->chunk_lookup_peak_bytes)
        store->chunk_lookup_peak_bytes = peak;
    next->capacity = capacity;
    for (store_chunk *node = store->chunks; node != NULL; node = node->next) {
        mc_chunk_state *identity = store_chunk_state(node);
        if (identity == NULL || !store_chunk_lookup_insert(next, (uintptr_t)identity, node)) {
            free(next);
            store_chunk_lookup_disable(store);
            return;
        }
    }
    if (next->count != store->chunk_count) {
        free(next);
        store_chunk_lookup_disable(store);
        return;
    }
    free(old);
    store->chunk_lookup = next;
}

static void store_chunk_lookup_erase(mc_world_store *store, uintptr_t key) {
    store_chunk_lookup *index = store->chunk_lookup;
    if (index == NULL)
        return;
    const size_t mask = index->capacity - 1u;
    size_t slot = store_chunk_lookup_hash(key, mask);
    size_t i = 0;
    while (i < index->capacity && index->slots[slot].entry != NULL &&
           index->slots[slot].state_key != key) {
        slot = (slot + 1u) & mask;
        ++i;
    }
    if (i == index->capacity || index->slots[slot].entry == NULL) {
        store_chunk_lookup_disable(store);
        return;
    }
    index->slots[slot] = (store_chunk_lookup_slot){0};
    --index->count;
    /* Reinsert only the following probe cluster. There is always an empty
     * slot because publication maintains at most one-half occupancy. Neither
     * allocation nor a Chunk getter/destructor/callback runs during erase. */
    slot = (slot + 1u) & mask;
    size_t moved = 0;
    while (moved < index->capacity && index->slots[slot].entry != NULL) {
        const store_chunk_lookup_slot saved = index->slots[slot];
        index->slots[slot] = (store_chunk_lookup_slot){0};
        --index->count;
        if (!store_chunk_lookup_insert(index, saved.state_key, saved.entry)) {
            store_chunk_lookup_disable(store);
            return;
        }
        slot = (slot + 1u) & mask;
        ++moved;
    }
    if (moved == index->capacity) {
        store_chunk_lookup_disable(store);
        return;
    }
    if (index->count == 0)
        store_chunk_lookup_clear(store);
}

bool mc_world_store_chunk_lookup_inspect(const mc_world_store *store,
                                         mc_world_store_chunk_lookup_view *out) {
    if (store == NULL || out == NULL)
        return false;
    const store_chunk_lookup *index = store->chunk_lookup;
    *out = (mc_world_store_chunk_lookup_view){.indexed = index != NULL,
                                              .disabled = store->chunk_lookup_disabled,
                                              .chunk_count = store->chunk_count,
                                              .index_count = index == NULL ? 0 : index->count,
                                              .capacity = index == NULL ? 0 : index->capacity,
                                              .index_bytes = store_chunk_lookup_bytes(index),
                                              .peak_bytes = store->chunk_lookup_peak_bytes,
                                              .byte_limit = store_chunk_lookup_limit(store),
                                              .base_owner_bytes =
                                                  store_chunk_lookup_base_bytes(store)};
    return true;
}
