#include "mc/world/runtime/render_entities.h"

#include "chunk_provider_internal.h"
#include "construction_internal.h"
#include "mc/core/java/java.h"
#include "runtime_internal.h"

#include <limits.h>

static mc_world_storage_status result(mc_world_storage_error *error, mc_world_storage_status status,
                                      const char *operation) {
    if (error != NULL)
        *error = (mc_world_storage_error){status, operation, 0};
    return status;
}

static mc_world_storage_status admit(const mc_world_runtime *world, mc_world_storage_error *error) {
    if (world == NULL)
        return result(error, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                      "render List actual World receiver");
    if (world->classes == NULL || world->classes->side != MC_WORLD_INFO_CLIENT ||
        world->construction.mode != MC_WORLD_RUNTIME_GAME ||
        world->construction.phase == MC_WORLD_RUNTIME_RETIRED || world->entity_containers_retired)
        return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                      "render List live CLIENT GAME World");
    return MC_WORLD_STORAGE_OK;
}

mc_world_storage_status mc_world_runtime_render_entities_list(mc_world_runtime *world,
                                                              const mc_world_reference_list **out,
                                                              mc_world_storage_error *error) {
    if (out == NULL)
        return result(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "fd.o actual List output");
    const mc_world_storage_status status = admit(world, error);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    *out = world->fields.b; /* Actual fd.o getfield1; no List dereference. */
    return result(error, MC_WORLD_STORAGE_OK, NULL);
}

mc_world_storage_status mc_world_runtime_render_weather_list(mc_world_runtime *world,
                                                             const mc_world_reference_list **out,
                                                             mc_world_storage_error *error) {
    if (out == NULL)
        return result(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "World.e actual List output");
    const mc_world_storage_status status = admit(world, error);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    *out = world->fields.e;
    return result(error, MC_WORLD_STORAGE_OK, NULL);
}

static const mc_world_entity_list_ledger *ledger(const mc_world_runtime *world,
                                                 const mc_world_reference_list *list) {
    if (list == NULL)
        return NULL;
    const mc_world_entity_list_ledger *all[] = {&world->entity_membership,
                                                &world->weather_membership};
    for (size_t i = 0; i < sizeof(all) / sizeof(all[0]); ++i) {
        const mc_world_entity_list_ledger *owned = all[i];
        if (list != owned->list)
            continue;
        if (list->count > list->capacity || list->capacity > (size_t)INT32_MAX ||
            list->capacity > SIZE_MAX / (sizeof(*list->items) + sizeof(*owned->entries)) ||
            owned->count != list->count || owned->capacity != list->capacity ||
            ((list->capacity == 0) != (list->items == NULL)) ||
            ((owned->capacity == 0) != (owned->entries == NULL)) ||
            (list->items != NULL && (void *)list->items == (void *)owned->entries) ||
            list->capacity * (sizeof(*list->items) + sizeof(*owned->entries)) >
                world->construction.direct_allocation_bytes)
            return NULL;
        return owned;
    }
    return NULL;
}

mc_world_storage_status
mc_world_runtime_render_entity_list_count(mc_world_runtime *world,
                                          const mc_world_reference_list *list, int32_t *out,
                                          mc_world_storage_error *error) {
    if (out == NULL)
        return result(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "render List.size output");
    const mc_world_storage_status status = admit(world, error);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    if (ledger(world, list) == NULL)
        return result(error,
                      list == NULL ? MC_WORLD_STORAGE_INVALID_ARGUMENT
                                   : MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                      "render List.size actual typed owning identity");
    *out = (int32_t)list->count;
    return result(error, MC_WORLD_STORAGE_OK, NULL);
}

mc_world_storage_status mc_world_runtime_render_entity_list_get(mc_world_runtime *world,
                                                                const mc_world_reference_list *list,
                                                                int32_t index, mc_entity **out,
                                                                mc_world_storage_error *error) {
    if (out == NULL)
        return result(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "render List.get output");
    const mc_world_storage_status status = admit(world, error);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    const mc_world_entity_list_ledger *owned = ledger(world, list);
    if (owned == NULL)
        return result(error,
                      list == NULL ? MC_WORLD_STORAGE_INVALID_ARGUMENT
                                   : MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                      "render List.get actual typed owning identity");
    if (index < 0 || (size_t)index >= list->count)
        return result(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "render List.get source index");
    mc_entity *entity = owned->entries[(size_t)index];
    if (list->items[(size_t)index] != entity)
        return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                      "render List.get typed occurrence");
    if (entity != NULL) {
        mc_entity_snapshot snapshot;
        mc_entity_class_literal_view type;
        if (mc_entity_get_snapshot(entity, &snapshot) != MC_ENTITY_OK ||
            mc_entity_class_literal_inspect(snapshot.class_literal, &type) != MC_ENTITY_OK ||
            type.owner != world->classes->entities || type.source != MC_ENTITY_CLIENT)
            return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                          "render List.get canonical Entity class cast");
    }
    *out = entity;
    return result(error, MC_WORLD_STORAGE_OK, NULL);
}

mc_world_storage_status mc_world_runtime_render_entities_lease(mc_world_runtime *world,
                                                               mc_world_storage_error *error) {
    const mc_world_storage_status status = admit(world, error);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    if (world->local_leases == SIZE_MAX)
        return result(error, MC_WORLD_STORAGE_LIMIT, "render List native lease overflow");
    ++world->local_leases;
    return result(error, MC_WORLD_STORAGE_OK, NULL);
}

mc_world_storage_status
mc_world_runtime_render_entities_release_lease(mc_world_runtime *world,
                                               mc_world_storage_error *error) {
    if (world == NULL)
        return result(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "render List native lease World");
    if (world->local_leases == 0)
        return result(error, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                      "render List unmatched native lease");
    --world->local_leases;
    return result(error, MC_WORLD_STORAGE_OK, NULL);
}

mc_world_storage_status mc_world_runtime_render_block_present(mc_world_runtime *world, int32_t x,
                                                              int32_t y, int32_t z, bool *out,
                                                              mc_world_storage_error *error) {
    if (out == NULL)
        return result(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "fd.i actual presence output");
    const mc_world_storage_status status = admit(world, error);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    if (y < 0 || y >= 128) {
        *out = false; /* Actual ordinary return12, before the provider receiver. */
        return result(error, MC_WORLD_STORAGE_OK, NULL);
    }
    return mc_world_runtime_chunk_provider_contains(world, mc_java_i32_shr(x, 4),
                                                    mc_java_i32_shr(z, 4), out, error);
}
