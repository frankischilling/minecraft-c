#include "test_refresh_entity.h"
#include "client/application_state.h"
#include "mc/world/runtime/client_tick_state.h"
#include "world/runtime/entity_tick_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int mc_test_world_refresh_entity(client_application *app) {
    int result = 1;
    mc_world_refresh_entity_call *call = NULL;
    mc_world_refresh_entity_state state;
    mc_world_refresh_entity_error error;
    mc_entity *candidate = NULL;
    wt_scope *scope = NULL;
    bool candidate_appended = false;
    mc_world_runtime *world = app->active_world;
    mc_entity_snapshot before, after;
    const size_t count = world->entity_membership.count;
    const size_t player_count = world->player_membership.count;
    const size_t leases = world->local_leases;
    const size_t world_holds = world->persistent_bindings;
    const size_t entity_holds = app->h->holds;
    if (mc_entity_get_snapshot(app->h, &before) != MC_ENTITY_OK ||
        mc_world_runtime_refresh_entity_begin(world, app->h, &call, &error) !=
            MC_WORLD_STORAGE_OK ||
        !mc_world_runtime_refresh_entity_inspect(call, &state) || !state.returned ||
        state.source_pc != 102 || state.chunk_calls != 25 || state.chunk_returns != 25 ||
        !state.contains || state.appended || world->entity_membership.count != count ||
        world->player_membership.count != player_count || world->local_leases != leases ||
        mc_entity_get_snapshot(app->h, &after) != MC_ENTITY_OK ||
        memcmp(&before.base, &after.base, sizeof(before.base)) != 0)
        goto cleanup;
    for (size_t i = 0; i < 25; ++i)
        if (state.chunks[i].x != state.center_x - 2 + (int32_t)(i / 5) ||
            state.chunks[i].z != state.center_z - 2 + (int32_t)(i % 5))
            goto cleanup;
    if (mc_world_runtime_refresh_entity_retire(&call, NULL) != MC_WORLD_STORAGE_OK ||
        world->persistent_bindings != world_holds || app->h->holds != entity_holds)
        goto cleanup;
    if (mc_falling_entity_allocate(app->entities, &candidate, NULL) != MC_ENTITY_OK ||
        mc_falling_entity_construct_world(candidate, world, NULL) != MC_ENTITY_OK)
        goto cleanup;
    scope = calloc(1, sizeof(*scope));
    if (scope == NULL)
        goto cleanup;
    scope->world = world;
    mc_world_entity_tick_error_init(&scope->error);
    const int32_t real_id = candidate->base.id;
    candidate->base.id = app->h->base.id; /* Authored equals-by-ID control, restored below. */
    const mc_world_storage_status equal_status =
        mc_world_runtime_refresh_entity_begin(world, candidate, &call, &error);
    candidate->base.id = real_id;
    if (equal_status != MC_WORLD_STORAGE_OK ||
        !mc_world_runtime_refresh_entity_inspect(call, &state) || !state.contains ||
        state.appended || world->entity_membership.count != count ||
        mc_world_runtime_refresh_entity_retire(&call, NULL) != MC_WORLD_STORAGE_OK)
        goto cleanup;
    if (mc_entity_get_snapshot(candidate, &before) != MC_ENTITY_OK)
        goto cleanup;
    const mc_world_storage_status append_status =
        mc_world_runtime_refresh_entity_begin(world, candidate, &call, &error);
    candidate_appended = world->entity_membership.count == count + 1 &&
                         world->entity_membership.entries[count] == candidate;
    if (append_status != MC_WORLD_STORAGE_OK ||
        !mc_world_runtime_refresh_entity_inspect(call, &state) || state.contains ||
        !state.appended || !candidate_appended || world->player_membership.count != player_count ||
        world->local_leases != leases ||
        mc_entity_get_snapshot(candidate, &after) != MC_ENTITY_OK ||
        memcmp(&before.base, &after.base, sizeof(before.base)) != 0 ||
        mc_world_runtime_refresh_entity_retire(&call, NULL) != MC_WORLD_STORAGE_OK)
        goto cleanup;
    if (!wt_list_remove(scope, world->fields.b, (int32_t)count, 0))
        goto cleanup;
    candidate_appended = false;
    if (world->entity_membership.count != count || candidate->holds != 1)
        goto cleanup;
    result = 0;
    puts("WORLD_REFRESH returned=102 chunks=25 order=x-z contains=1 equals-id=1 "
         "append=1 no-chunk-membership=1 cleanup=1");
cleanup:
    if (mc_world_runtime_refresh_entity_retire(&call, NULL) != MC_WORLD_STORAGE_OK)
        result = 1;
    if (candidate_appended &&
        (scope == NULL || !wt_list_remove(scope, world->fields.b, (int32_t)count, 0)))
        result = 1;
    free(scope);
    mc_entity_release(candidate);
    if (result != 0)
        fputs("Loaded World refresh Entity check failed\n", stderr);
    return result;
}
