#include "recipes_internal.h"

#include "../../block/registry/registry_internal.h"
#include "../registry/stacks_internal.h"

#include <limits.h>

typedef struct smelting_entry {
    int32_t key;
    mc_item_stack *value;
    struct smelting_entry *next;
} smelting_entry;

struct mc_smelting_map {
    mc_java_int_set *keys;
    smelting_entry *entries;
};

static mc_registry_status own_error(mc_smelting_manager *manager, int32_t pc)
{
    mc_registry_error *error = &manager->classes->last_error;
    error->constructor_class = manager->classes->options.side == MC_REGISTRY_CLIENT ? "ey" : "de";
    error->constructor_pc = pc;
    return error->status;
}

static mc_registry_status fail(mc_smelting_manager *manager, int32_t pc, mc_registry_status status,
                               const char *message)
{
    (void)mc_registry_fail(manager->classes, status, message, NULL);
    return own_error(manager, pc);
}

static void *set_allocate(void *context, size_t bytes)
{
    return mc_registry_allocate(context, bytes);
}

static mc_registry_status set_error(mc_registry_classes *classes, mc_java_int_set_status status)
{
    if (status == MC_JAVA_INT_SET_ALLOCATION_FAILED &&
        (classes->last_error.status == MC_REGISTRY_LIMIT ||
         classes->last_error.status == MC_REGISTRY_ALLOCATION_FAILED))
        return classes->last_error.status;
    return mc_registry_fail(classes,
                            status == MC_JAVA_INT_SET_LIMIT ? MC_REGISTRY_LIMIT
                            : status == MC_JAVA_INT_SET_ALLOCATION_FAILED
                                ? MC_REGISTRY_ALLOCATION_FAILED
                                : MC_REGISTRY_INVALID_ARGUMENT,
                            "Smelting Integer-key traversal storage", NULL);
}

static smelting_entry *find(const struct mc_smelting_map *map, int32_t key)
{
    if (map != NULL)
        for (smelting_entry *entry = map->entries; entry != NULL; entry = entry->next)
            if (entry->key == key)
                return entry;
    return NULL;
}

mc_registry_status mc_registry_smelting_put(mc_smelting_manager *manager, int32_t input,
                                            mc_item_stack *output)
{
    if (manager == NULL || manager->classes == NULL)
        return MC_REGISTRY_INVALID_ARGUMENT;
    mc_registry_classes *classes = manager->classes;
    if (classes->entered == 0)
        return fail(manager, 0, MC_REGISTRY_INVALID_ARGUMENT,
                    "Smelting put requires admitted owner");
    struct mc_smelting_map *map = manager->b; /* getfield1 before box5 */
    if (map == NULL)
        return fail(manager, 9, MC_REGISTRY_NULL_REFERENCE, "Smelting Map.put null receiver");
    smelting_entry *entry = find(map, input);
    if (entry != NULL) {
        entry->value = output;
        return MC_REGISTRY_OK;
    }
    /* Extra native value associations preserve actual Stack identity. The
     * shared Integer-key kernel supplies HashMap bucket/tree history; values
     * do not participate in that history. Native allocations precede insertion
     * and do not claim Java boxing/node allocation or OOME timing. */
    entry = mc_registry_allocate(classes, sizeof(*entry));
    if (entry == NULL)
        return own_error(manager, 9);
    bool changed = false;
    mc_java_int_set_status status = mc_java_int_set_add(map->keys, input, &changed);
    if (status != MC_JAVA_INT_SET_OK) {
        (void)set_error(classes, status);
        return own_error(manager, 9);
    }
    if (!changed)
        return fail(manager, 9, MC_REGISTRY_INVALID_ARGUMENT,
                    "Smelting native key/value invariant");
    *entry = (smelting_entry){input, output, map->entries};
    map->entries = entry;
    return MC_REGISTRY_OK;
}

typedef struct smelting_step {
    int32_t key_pc, key_id_pc;
    bool block_key;
    const char *key;
    int32_t allocation_pc, output_pc;
    bool block_output;
    const char *output;
    int32_t constructor_pc, registration_pc, count, damage;
} smelting_step;

/* Actual manager instructions, using current object fields rather than an
 * initialized key/output-ID table. Key evaluation precedes output allocation. */
static const smelting_step steps[] = {
    {16, 19, true, "I", 22, 26, false, "m", 29, 32, 1, 0},
    {36, 39, true, "H", 42, 46, false, "n", 49, 52, 1, 0},
    {56, 59, true, "ax", 62, 66, false, "l", 69, 72, 1, 0},
    {76, 79, true, "F", 82, 86, true, "N", 89, 92, 1, 0},
    {96, 99, false, "ao", 102, 106, false, "ap", 109, 112, 1, 0},
    {116, 119, false, "aS", 122, 126, false, "aT", 129, 132, 1, 0},
    {136, 139, true, "x", 142, 146, true, "u", 149, 152, 1, 0},
    {156, 159, false, "aG", 162, 166, false, "aF", 169, 172, 1, 0},
    {176, 179, true, "aW", 182, 186, false, "aU", 191, 194, 1, 2},
    {198, 201, true, "K", 204, 208, false, "k", 213, 216, 1, 1}};

static mc_registry_status initialize_step(mc_smelting_manager *manager, const smelting_step *step)
{
    mc_registry_classes *classes = manager->classes;
    manager->constructor_pc = step->key_pc;
    mc_block *block = NULL;
    mc_item *item = NULL;
    mc_registry_status status = step->block_key
                                    ? mc_registry_block_static_load(classes, step->key, &block)
                                    : mc_registry_item_static_load(classes, step->key, &item);
    if (status != MC_REGISTRY_OK)
        return status;
    manager->constructor_pc = step->key_id_pc;
    if ((step->block_key && block == NULL) || (!step->block_key && item == NULL))
        return fail(manager, step->key_id_pc, MC_REGISTRY_NULL_REFERENCE,
                    "Smelting key ID read on null");
    const int32_t key = step->block_key ? block->fields.bn : item->fields.bf;
    manager->constructor_pc = step->allocation_pc;
    mc_item_stack *output = NULL;
    status = mc_registry_stack_allocate(classes, &output);
    if (status != MC_REGISTRY_OK)
        return own_error(manager, step->allocation_pc);
    manager->constructor_pc = step->output_pc;
    block = NULL;
    item = NULL;
    status = step->block_output ? mc_registry_block_static_load(classes, step->output, &block)
                                : mc_registry_item_static_load(classes, step->output, &item);
    if (status != MC_REGISTRY_OK)
        return status;
    manager->constructor_pc = step->constructor_pc;
    status = step->block_output
                 ? mc_registry_stack_construct_block_values(classes, output, block, step->count,
                                                            step->damage)
                 : mc_registry_stack_construct_item_values(classes, output, item, step->count,
                                                           step->damage);
    if (status != MC_REGISTRY_OK)
        return status;
    manager->constructor_pc = step->registration_pc;
    return mc_registry_smelting_put(manager, key, output);
}

mc_registry_status mc_registry_smelting_construct(mc_smelting_manager *manager)
{
    if (manager == NULL || manager->classes == NULL)
        return MC_REGISTRY_INVALID_ARGUMENT;
    mc_registry_classes *classes = manager->classes;
    if (classes->entered == 0)
        return fail(manager, 0, MC_REGISTRY_INVALID_ARGUMENT,
                    "Smelting constructor requires admitted owner");
    manager->constructor_pc = 1;
    manager->constructor_pc = 5;
    manager->constructor_pc = 9;
    struct mc_smelting_map *map = mc_registry_allocate(classes, sizeof(*map));
    if (map == NULL)
        return own_error(manager, 9);
    *map = (struct mc_smelting_map){0};
    const mc_java_int_set_allocator allocator = {classes, set_allocate, NULL};
    mc_java_int_set_status set_status =
        mc_java_int_set_create_with_allocator(NULL, &allocator, &map->keys);
    if (set_status != MC_JAVA_INT_SET_OK) {
        (void)set_error(classes, set_status);
        return own_error(manager, 9);
    }
    manager->constructor_pc = 12;
    manager->b = map;
    for (size_t i = 0; i < sizeof(steps) / sizeof(*steps); ++i) {
        const mc_registry_status status = initialize_step(manager, &steps[i]);
        if (status != MC_REGISTRY_OK)
            return status;
    }
    manager->constructor_pc = 219;
    return MC_REGISTRY_OK;
}

size_t mc_smelting_manager_size(const mc_smelting_manager *manager)
{
    return manager == NULL || manager->b == NULL ? 0 : mc_java_int_set_size(manager->b->keys);
}

mc_registry_status mc_smelting_manager_find(const mc_smelting_manager *manager, int32_t input,
                                            const mc_item_stack **out)
{
    if (manager == NULL || out == NULL)
        return MC_REGISTRY_INVALID_ARGUMENT;
    if (manager->b == NULL)
        return MC_REGISTRY_NULL_REFERENCE;
    smelting_entry *entry = find(manager->b, input);
    *out = entry == NULL ? NULL : entry->value;
    return MC_REGISTRY_OK;
}

mc_registry_status mc_registry_smelting_keys(mc_registry_classes *classes,
                                             const mc_smelting_manager *manager, int32_t **out,
                                             size_t *count)
{
    if (classes == NULL || manager == NULL || out == NULL || count == NULL)
        return MC_REGISTRY_INVALID_ARGUMENT;
    if (classes->entered == 0 || manager->classes != classes)
        return mc_registry_fail(classes, MC_REGISTRY_INVALID_ARGUMENT,
                                "Smelting order requires admitted matching class owner", NULL);
    if (manager->b == NULL)
        return mc_registry_fail(classes, MC_REGISTRY_NULL_REFERENCE,
                                "Smelting Map.values null receiver", NULL);
    const size_t size = mc_java_int_set_size(manager->b->keys);
    if (size > (size_t)INT32_MAX || size > SIZE_MAX / sizeof(int32_t))
        return mc_registry_fail(classes, MC_REGISTRY_LIMIT, "Smelting native traversal bound",
                                NULL);
    int32_t *keys = NULL;
    if (size != 0) {
        keys = mc_registry_allocate(classes, size * sizeof(*keys));
        if (keys == NULL)
            return classes->last_error.status;
    }
    size_t copied = 0;
    mc_java_int_set_status status =
        mc_java_int_set_copy_order(manager->b->keys, keys, size, &copied);
    if (status != MC_JAVA_INT_SET_OK)
        return set_error(classes, status);
    if (copied != size)
        return mc_registry_fail(classes, MC_REGISTRY_INVALID_ARGUMENT,
                                "Smelting native traversal size invariant", NULL);
    *out = keys;
    *count = size;
    return MC_REGISTRY_OK;
}

mc_registry_status mc_smelting_manager_keys(const mc_smelting_manager *manager, int32_t *output,
                                            size_t capacity, size_t *count)
{
    if (manager == NULL || count == NULL || (output == NULL && capacity != 0))
        return MC_REGISTRY_INVALID_ARGUMENT;
    if (manager->b == NULL)
        return MC_REGISTRY_NULL_REFERENCE;
    const mc_java_int_set_status status =
        mc_java_int_set_copy_order(manager->b->keys, output, capacity, count);
    return status == MC_JAVA_INT_SET_OK      ? MC_REGISTRY_OK
           : status == MC_JAVA_INT_SET_LIMIT ? MC_REGISTRY_LIMIT
                                             : MC_REGISTRY_INVALID_ARGUMENT;
}
