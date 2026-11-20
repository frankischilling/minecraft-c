#include "entity_index.h"

#include "mc/core/java/java.h"
#include "mc/entity/entity.h"

#include <float.h>
#include <limits.h>
#include <stdlib.h>

#define ENTITY_INDEX_INITIAL_CAPACITY UINT32_C(16)
#define ENTITY_INDEX_MAX_CAPACITY UINT32_C(0x40000000)

/* Server go/og, SHA256
 * 033a127e4a25a60b038f15369c89305a3d53752242a1cff11ae964954e79ba4d.
 * This typed owner implements the Entity-valued use by dp.G. */
struct mc_world_entity_index {
    mc_world_entity_index_node **a;
    int32_t b, c;
    float d;
    int32_t e;
    uint32_t capacity;
    size_t allocation_bytes;
    mc_world_entity_index_limits limits;
};

_Static_assert(FLT_RADIX == 2 && FLT_MANT_DIG == 24, "binary32 load factor required");

static mc_world_storage_status result(mc_world_entity_index_error *error,
                                      mc_world_storage_status status,
                                      mc_world_entity_index_method method, unsigned int bytecode,
                                      const char *operation, mc_entity_status entity_status) {
    if (error != NULL)
        *error = (mc_world_entity_index_error){.cause = {status, operation, 0},
                                               .entity_status = entity_status,
                                               .method = method,
                                               .bytecode = bytecode};
    return status;
}

static mc_world_storage_status completed(mc_world_entity_index_error *error) {
    return result(error, MC_WORLD_STORAGE_OK, MC_WORLD_ENTITY_INDEX_NONE, 0, NULL, MC_ENTITY_OK);
}

static mc_world_storage_status failed(mc_world_entity_index_error *error,
                                      mc_world_storage_status status,
                                      mc_world_entity_index_method method, unsigned int bytecode,
                                      const char *operation) {
    return result(error, status, method, bytecode, operation, MC_ENTITY_OK);
}

static uint32_t hash(int32_t key) {
    uint32_t bits = (uint32_t)key;
    bits ^= (bits >> 20u) ^ (bits >> 12u);     /* go.g: iushr4/8, xor9/10. */
    return bits ^ (bits >> 7u) ^ (bits >> 4u); /* iushr16/20, xor17/21. */
}

static size_t bucket(uint32_t bits, uint32_t capacity) {
    return (size_t)(bits & (capacity - UINT32_C(1))); /* go.a(II)I and4. */
}

static bool table_bytes(size_t capacity, size_t *out) {
    if (capacity > SIZE_MAX / sizeof(mc_world_entity_index_node *))
        return false;
    *out = capacity * sizeof(mc_world_entity_index_node *);
    return true;
}

static mc_world_entity_index_node *find(const mc_world_entity_index *index, int32_t key) {
    mc_world_entity_index_node *node = index->a[bucket(hash(key), index->capacity)];
    while (node != NULL && node->a != key)
        node = node->c;
    return node;
}

static mc_world_storage_status retain(mc_entity *value, mc_world_entity_index_error *error,
                                      mc_world_entity_index_method method, unsigned int bytecode) {
    if (value == NULL)
        return MC_WORLD_STORAGE_OK;
    const mc_entity_status status = mc_entity_retain(value);
    if (status == MC_ENTITY_OK)
        return MC_WORLD_STORAGE_OK;
    return result(error,
                  status == MC_ENTITY_LIMIT ? MC_WORLD_STORAGE_LIMIT
                                            : MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                  method, bytecode, "dedicated Entity index hold", status);
}

mc_world_entity_index_limits mc_world_entity_index_default_limits(void) {
    return (mc_world_entity_index_limits){65536u, 8u * 1024u * 1024u, 16u * 1024u * 1024u};
}

mc_world_storage_status mc_world_entity_index_create(const mc_world_entity_index_limits *limits,
                                                     mc_world_entity_index **out,
                                                     mc_world_entity_index_error *error) {
    const mc_world_entity_index_limits selected =
        limits == NULL ? mc_world_entity_index_default_limits() : *limits;
    if (out == NULL || selected.max_nodes > (size_t)INT32_MAX)
        return failed(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, MC_WORLD_ENTITY_INDEX_CONSTRUCTOR,
                      0, "dedicated Entity index constructor arguments");
    const size_t bytes = ENTITY_INDEX_INITIAL_CAPACITY * sizeof(mc_world_entity_index_node *);
    if (sizeof(mc_world_entity_index) + bytes > selected.max_retained_bytes ||
        sizeof(mc_world_entity_index) + bytes > selected.max_peak_bytes)
        return failed(error, MC_WORLD_STORAGE_LIMIT, MC_WORLD_ENTITY_INDEX_CONSTRUCTOR, 19,
                      "dedicated Entity index initial table budget");
    mc_world_entity_index *index = malloc(sizeof(*index));
    if (index == NULL)
        return failed(error, MC_WORLD_STORAGE_ALLOCATION_FAILED, MC_WORLD_ENTITY_INDEX_CONSTRUCTOR,
                      0, "dedicated Entity index owner allocation");
    *index = (mc_world_entity_index){.c = 12, .d = 0.75f, .limits = selected};
    index->a = calloc(ENTITY_INDEX_INITIAL_CAPACITY, sizeof(*index->a));
    if (index->a == NULL) {
        free(index);
        return failed(error, MC_WORLD_STORAGE_ALLOCATION_FAILED, MC_WORLD_ENTITY_INDEX_CONSTRUCTOR,
                      19, "dedicated Entity index initial table allocation");
    }
    index->capacity = ENTITY_INDEX_INITIAL_CAPACITY;
    index->allocation_bytes = sizeof(*index) + bytes;
    *out = index;
    return completed(error);
}

static mc_world_storage_status resize(mc_world_entity_index *index,
                                      mc_world_entity_index_error *error) {
    if (index->capacity == ENTITY_INDEX_MAX_CAPACITY) {
        index->c = INT32_MAX; /* go.h: store17 before return20. */
        return MC_WORLD_STORAGE_OK;
    }
    const uint32_t capacity = index->capacity * UINT32_C(2);
    size_t bytes;
    if (!table_bytes(capacity, &bytes))
        return failed(error, MC_WORLD_STORAGE_LIMIT, MC_WORLD_ENTITY_INDEX_RESIZE, 22,
                      "dedicated Entity index replacement table size");
    const size_t old_bytes = (size_t)index->capacity * sizeof(*index->a);
    const size_t without_old = index->allocation_bytes - old_bytes;
    if (without_old > index->limits.max_retained_bytes ||
        bytes > index->limits.max_retained_bytes - without_old ||
        index->allocation_bytes > index->limits.max_peak_bytes ||
        bytes > index->limits.max_peak_bytes - index->allocation_bytes)
        return failed(error, MC_WORLD_STORAGE_LIMIT, MC_WORLD_ENTITY_INDEX_RESIZE, 22,
                      "dedicated Entity index replacement table budget");
    mc_world_entity_index_node **table = calloc(capacity, sizeof(*table));
    if (table == NULL)
        return failed(error, MC_WORLD_STORAGE_ALLOCATION_FAILED, MC_WORLD_ENTITY_INDEX_RESIZE, 22,
                      "dedicated Entity index replacement table allocation");
    /* go.a([Log;)V clears each old bucket33, then reverses by new-head stores
     * 58/66. Nothing fallible follows allocation; retain exact node identity. */
    for (uint32_t slot = 0; slot < index->capacity; ++slot) {
        mc_world_entity_index_node *node = index->a[slot];
        index->a[slot] = NULL;
        while (node != NULL) {
            mc_world_entity_index_node *next = node->c;
            const size_t target = bucket((uint32_t)node->d, capacity);
            node->c = table[target];
            table[target] = node;
            node = next;
        }
    }
    free(index->a);
    index->a = table; /* go.h store36, followed by float multiply46/store48. */
    index->capacity = capacity;
    index->allocation_bytes = without_old + bytes;
    index->c = (int32_t)((float)capacity * index->d);
    return MC_WORLD_STORAGE_OK;
}

mc_world_storage_status mc_world_entity_index_put(mc_world_entity_index *index, int32_t key,
                                                  mc_entity *value,
                                                  mc_world_entity_index_error *error) {
    if (index == NULL)
        return failed(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, MC_WORLD_ENTITY_INDEX_PUT, 0,
                      "dedicated Entity index put receiver");
    const uint32_t bits = hash(key);
    const size_t slot = bucket(bits, index->capacity);
    for (mc_world_entity_index_node *node = index->a[slot]; node != NULL; node = node->c) {
        if (node->a == key && node->b != value) {
            const mc_world_storage_status status =
                retain(value, error, MC_WORLD_ENTITY_INDEX_PUT, 42);
            if (status != MC_WORLD_STORAGE_OK)
                return status;
            mc_entity *old = node->b;
            node->b = value; /* No return after source putfield42. */
            mc_entity_release(old);
        }
    }
    index->e = mc_java_i32_add(index->e, 1); /* go.a: store62 before helper71. */
    if ((size_t)index->b >= index->limits.max_nodes ||
        index->allocation_bytes > index->limits.max_retained_bytes ||
        sizeof(mc_world_entity_index_node) >
            index->limits.max_retained_bytes - index->allocation_bytes ||
        index->allocation_bytes > index->limits.max_peak_bytes ||
        sizeof(mc_world_entity_index_node) > index->limits.max_peak_bytes - index->allocation_bytes)
        return failed(error, MC_WORLD_STORAGE_LIMIT, MC_WORLD_ENTITY_INDEX_NODE, 15,
                      "dedicated Entity index node budget");
    mc_world_entity_index_node *node = malloc(sizeof(*node));
    if (node == NULL)
        return failed(error, MC_WORLD_STORAGE_ALLOCATION_FAILED, MC_WORLD_ENTITY_INDEX_NODE, 15,
                      "dedicated Entity index node allocation");
    const mc_world_storage_status status = retain(value, error, MC_WORLD_ENTITY_INDEX_NODE, 24);
    if (status != MC_WORLD_STORAGE_OK) {
        free(node);
        return status;
    }
    *node =
        (mc_world_entity_index_node){key, value, index->a[slot], mc_java_i32_narrow((int64_t)bits)};
    index->a[slot] = node; /* helper aastore27 precedes count store36. */
    index->allocation_bytes += sizeof(*node);
    const int32_t before = index->b++;
    if (before >= index->c) {
        const mc_world_storage_status grown = resize(index, error);
        if (grown != MC_WORLD_STORAGE_OK)
            return grown;
    }
    return completed(error);
}

mc_world_storage_status mc_world_entity_index_get(const mc_world_entity_index *index, int32_t key,
                                                  bool *present, mc_entity **out,
                                                  mc_world_entity_index_error *error) {
    if (index == NULL || present == NULL || out == NULL)
        return failed(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, MC_WORLD_ENTITY_INDEX_GET, 0,
                      "dedicated Entity index get arguments");
    const mc_world_entity_index_node *node = find(index, key);
    *present = node != NULL;
    *out = node == NULL ? NULL : node->b;
    return completed(error);
}

mc_world_storage_status mc_world_entity_index_contains(const mc_world_entity_index *index,
                                                       int32_t key, bool *out,
                                                       mc_world_entity_index_error *error) {
    if (index == NULL || out == NULL)
        return failed(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, MC_WORLD_ENTITY_INDEX_CONTAINS, 0,
                      "dedicated Entity index contains arguments");
    *out = find(index, key) != NULL;
    return completed(error);
}

mc_world_storage_status mc_world_entity_index_remove(mc_world_entity_index *index, int32_t key,
                                                     bool *present, mc_entity **out,
                                                     mc_world_entity_index_error *error) {
    if (index == NULL || present == NULL)
        return failed(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, MC_WORLD_ENTITY_INDEX_REMOVE, 0,
                      "dedicated Entity index remove arguments");
    mc_world_entity_index_node **link = &index->a[bucket(hash(key), index->capacity)];
    while (*link != NULL && (*link)->a != key)
        link = &(*link)->c;
    mc_world_entity_index_node *node = *link;
    *present = node != NULL;
    if (node == NULL) {
        if (out != NULL)
            *out = NULL;
        return completed(error);
    }
    index->e = mc_java_i32_add(index->e, 1); /* go.e store55, count65, unlink82/90. */
    --index->b;
    *link = node->c;
    if (out != NULL)
        *out = node->b;
    else
        mc_entity_release(node->b);
    free(node);
    index->allocation_bytes -= sizeof(*node);
    return completed(error);
}

static void release_nodes(mc_world_entity_index *index) {
    for (uint32_t slot = 0; slot < index->capacity; ++slot) {
        mc_world_entity_index_node *node = index->a[slot];
        index->a[slot] = NULL;
        while (node != NULL) {
            mc_world_entity_index_node *next = node->c;
            mc_entity_release(node->b);
            free(node);
            index->allocation_bytes -= sizeof(*node);
            node = next;
        }
    }
    index->b = 0;
}

mc_world_storage_status mc_world_entity_index_clear(mc_world_entity_index *index,
                                                    mc_world_entity_index_error *error) {
    if (index == NULL)
        return failed(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, MC_WORLD_ENTITY_INDEX_CLEAR, 0,
                      "dedicated Entity index clear receiver");
    index->e = mc_java_i32_add(index->e, 1); /* go.a() store7, even with b==0. */
    release_nodes(index);
    return completed(error);
}

mc_world_storage_status mc_world_entity_index_inspect(const mc_world_entity_index *index,
                                                      mc_world_entity_index_view *out,
                                                      mc_world_entity_index_error *error) {
    if (index == NULL || out == NULL)
        return failed(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, MC_WORLD_ENTITY_INDEX_INSPECT, 0,
                      "dedicated Entity index inspect arguments");
    *out =
        (mc_world_entity_index_view){index,    index->a, index->capacity, index->b,
                                     index->c, index->d, index->e,        index->allocation_bytes};
    return completed(error);
}

void mc_world_entity_index_free(mc_world_entity_index *index) {
    if (index == NULL)
        return;
    release_nodes(index);
    free(index->a);
    free(index);
}
