/* Native Source-stage contracts over the existing genuine GAME/Store fixture. */
#define MC_OWNED_CACHE_FIXTURE_MAIN mc_stage_original_owned_cache_program
#include "test_owned_cache.c"
#undef MC_OWNED_CACHE_FIXTURE_MAIN
#include "world/runtime/saving/progress_internal.h"

typedef enum sp_mode {
    SP_SUCCESS,
    SP_LEVEL_FATAL,
    SP_CHUNKS_FATAL,
    SP_NULL,
    SP_REENTRY,
    SP_EXPORT
} sp_mode;

typedef struct sp_state {
    source_save_fixture *source;
    sp_mode mode;
    mc_chunk_state *chunk;
    mc_chunk_cache *escaped;
    mc_world_save_stage stages[2];
    size_t calls, nested_calls, blocked_calls;
    size_t outer_active, outer_leases;
    mc_world_runtime_lifecycle_error nested;
    bool failed, metadata_returned;
} sp_state;

static bool sp_check(bool value, const char *expression, int line) {
    ++checks;
    if (!value)
        fprintf(stderr, "stage progress line%d check%u: %s\n", line, checks, expression);
    return value;
}

#define SP_CHECK(x) sp_check((x), #x, __LINE__)

static mc_chunk_callback_result sp_nested(void *owner, mc_world_save_stage stage) {
    sp_state *state = owner;
    mc_world_runtime *world = state->source->game.worlds[0];
    mc_chunk_provider_cache_observation observed;
    ++state->nested_calls;
    if (!SP_CHECK(stage == MC_WORLD_SAVE_LEVEL && state->nested_calls == 1 &&
                  world->active_calls == state->outer_active + 2 &&
                  world->local_leases == state->outer_leases &&
                  mc_chunk_provider_cache_observe(world->fields.v, &observed) && observed.owned &&
                  observed.mutation_depth == 2))
        state->failed = true;
    /* An authored native callback failure, before nested session/metadata. */
    return MC_CHUNK_CALLBACK_FATAL;
}

static mc_chunk_callback_result sp_blocked(void *owner, const mc_world_save_context *context,
                                           mc_world_save_stage stage) {
    sp_state *state = owner;
    (void)context;
    (void)stage;
    ++state->blocked_calls;
    return MC_CHUNK_CALLBACK_FATAL;
}

static mc_chunk_callback_result sp_progress(void *owner, mc_world_save_stage stage) {
    sp_state *state = owner;
    mc_world_runtime *world = state->source->game.worlds[0];
    mc_chunk_provider_cache_observation observed;
    if (!SP_CHECK(state->calls < COUNT(state->stages) &&
                  stage == (state->calls == 0 ? MC_WORLD_SAVE_LEVEL : MC_WORLD_SAVE_CHUNKS) &&
                  world->active_calls == state->outer_active + 1 &&
                  world->local_leases == state->outer_leases && world->lifecycle_error != NULL &&
                  world->acquisition_error == NULL &&
                  mc_chunk_provider_cache_observe(world->fields.v, &observed) && observed.owned &&
                  observed.mutation_depth == 1)) {
        state->failed = true;
        return MC_CHUNK_CALLBACK_FATAL;
    }
    state->stages[state->calls++] = stage;
    if (stage == MC_WORLD_SAVE_CHUNKS) {
        mc_world_source_output_state output;
        if (!SP_CHECK(
                mc_world_store_source_output_inspect(state->source->game.stores[0], &output) &&
                output.nbt_written && output.closed && output.source_returned)) {
            state->failed = true;
            return MC_CHUNK_CALLBACK_FATAL;
        }
        state->metadata_returned = true;
    }
    if (stage == MC_WORLD_SAVE_LEVEL && state->mode == SP_REENTRY) {
        mc_world_runtime_lifecycle_error *outer = world->lifecycle_error;
        mc_world_runtime_acquisition_error *outer_acquisition = world->acquisition_error;
        mc_chunk_state *got = NULL;
        const mc_chunk_lifecycle_status status =
            mc_world_runtime_save_stages_detailed(world, true, state, sp_nested, &state->nested);
        if (!SP_CHECK(
                status == MC_CHUNK_LIFECYCLE_CALLBACK_FAILED && state->nested.status == status &&
                state->nested.cause.status == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
                strcmp(state->nested.cause.operation, "runtime lifecycle result") == 0 &&
                state->nested.save.region_bytecode == 0 && world->lifecycle_error == outer &&
                world->acquisition_error == outer_acquisition &&
                world->active_calls == state->outer_active + 1 &&
                world->local_leases == state->outer_leases &&
                mc_world_runtime_get_detailed(world, 0, 0, &got, NULL) == MC_CHUNK_ACQUIRE_OK &&
                got == state->chunk && world->lifecycle_error == outer &&
                world->acquisition_error == outer_acquisition &&
                mc_chunk_provider_cache_observe(world->fields.v, &observed) && observed.owned &&
                observed.mutation_depth == 1)) {
            state->failed = true;
            return MC_CHUNK_CALLBACK_FATAL;
        }
    }
    if (stage == MC_WORLD_SAVE_LEVEL && state->mode == SP_EXPORT) {
        const mc_chunk_acquire_context *escaped = mc_world_runtime_acquisition(world);
        if (!SP_CHECK(escaped != NULL && escaped->cache != NULL))
            state->failed = true;
        else
            state->escaped = escaped->cache;
        return MC_CHUNK_CALLBACK_FATAL;
    }
    return (stage == MC_WORLD_SAVE_LEVEL && state->mode == SP_LEVEL_FATAL) ||
                   (stage == MC_WORLD_SAVE_CHUNKS && state->mode == SP_CHUNKS_FATAL)
               ? MC_CHUNK_CALLBACK_FATAL
               : MC_CHUNK_CALLBACK_OK;
}

static int sp_export_contract(sp_state *state, const mc_chunk_provider_cache_observation *before) {
    mc_world_runtime *world = state->source->game.worlds[0];
    mc_chunk_cache *cache = state->escaped;
    mc_chunk_provider_cache_observation exported, after;
    VERIFY(cache != NULL && cache->count == 2 && oc_observe(world, &exported) == 0 &&
           !exported.owned && exported.index_capacity == 0 &&
           exported.raw_exports > before->raw_exports);
    const int32_t key = cache->entries[1].key;
    cache->entries[1].key = cache->entries[0].key;
    mc_chunk_state *got = state->chunk;
    mc_world_runtime_acquisition_error error;
    const mc_chunk_acquire_status invalid =
        mc_world_runtime_get_detailed(world, 0, 0, &got, &error);
    cache->entries[1].key = key;
    VERIFY(invalid == MC_CHUNK_ACQUIRE_INVALID_ARGUMENT && error.status == invalid &&
           got == state->chunk && mc_chunk_cache_validate(cache) &&
           mc_world_runtime_get_detailed(world, 0, 0, &got, NULL) == MC_CHUNK_ACQUIRE_OK &&
           got == state->chunk && oc_observe(world, &after) == 0 && !after.owned &&
           after.index_capacity == 0 && after.hits == exported.hits &&
           after.rebuilds == exported.rebuilds && after.generic_gets == exported.generic_gets + 2);
    return 0;
}

static int sp_case(source_save_fixture *source, sp_mode mode, const char *name) {
    mc_world_runtime *world = source->game.worlds[0];
    sp_state state = {.source = source, .mode = mode};
    mc_chunk_state *second = NULL;
    VERIFY(oc_get(source, 0, &state.chunk) == 0 && oc_get(source, 1, &second) == 0 &&
           second != state.chunk);
    /* Authored unit-test work: both real source Chunks must reach the save path. */
    state.chunk->dirty = second->dirty = true;
    state.outer_active = world->active_calls;
    state.outer_leases = world->local_leases;
    mc_world_runtime_lifecycle_error *previous = world->lifecycle_error;
    mc_world_runtime_acquisition_error *previous_acquisition = world->acquisition_error;
    const mc_world_save_context original = world->saving;
    mc_chunk_provider_cache_observation before, after;
    VERIFY(oc_observe(world, &before) == 0 && before.owned && before.raw_exports == 0 &&
           before.mutation_depth == 0 && ss_copy(source, "before", "level.dat", "level.dat") == 0 &&
           ss_copy(source, "before", "region/r.0.0.mcr", "region.mcr") == 0);
    if (mode == SP_NULL) {
        /* Native configured-context control only. Source fields are untouched;
         * per-call NULL must override this sentinel rather than fall back. */
        world->saving.progress = &state;
        world->saving.report_progress = sp_blocked;
    }
    const mc_chunk_lifecycle_status status = mc_world_runtime_save_stages_detailed(
        world, true, &state, mode == SP_NULL ? NULL : sp_progress, &source->lifecycle);
    const bool binding_preserved =
        world->saving.progress == (mode == SP_NULL ? (void *)&state : original.progress) &&
        world->saving.report_progress == (mode == SP_NULL ? sp_blocked : original.report_progress);
    world->saving = original;
    const bool expected_success = mode == SP_SUCCESS || mode == SP_NULL || mode == SP_REENTRY;
    VERIFY(!state.failed && binding_preserved &&
           status ==
               (expected_success ? MC_CHUNK_LIFECYCLE_OK : MC_CHUNK_LIFECYCLE_CALLBACK_FAILED) &&
           source->lifecycle.status == status && world->lifecycle_error == previous &&
           world->acquisition_error == previous_acquisition &&
           world->active_calls == state.outer_active && world->local_leases == state.outer_leases &&
           state.calls == (mode == SP_NULL                               ? 0u
                           : mode == SP_LEVEL_FATAL || mode == SP_EXPORT ? 1u
                                                                         : 2u) &&
           state.nested_calls == (mode == SP_REENTRY ? 1u : 0u) && state.blocked_calls == 0 &&
           state.metadata_returned ==
               (mode == SP_SUCCESS || mode == SP_REENTRY || mode == SP_CHUNKS_FATAL) &&
           oc_observe(world, &after) == 0 && after.mutation_depth == 0 &&
           after.map_count == before.map_count && after.insertion_count == before.insertion_count);
    if (expected_success) {
        VERIFY(source->lifecycle.cause.status == MC_WORLD_STORAGE_OK &&
               source->lifecycle.cause.operation == NULL &&
               source->lifecycle.cause.system_error == 0 &&
               source->lifecycle.save.region_bytecode == 0 &&
               source->lifecycle.save.entity_slice == SIZE_MAX &&
               source->lifecycle.save.entity_index == SIZE_MAX &&
               source->lifecycle.batch.index == SIZE_MAX && !state.chunk->dirty && !second->dirty);
    } else {
        VERIFY(source->lifecycle.cause.status == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
               strcmp(source->lifecycle.cause.operation, "runtime lifecycle result") == 0 &&
               source->lifecycle.cause.system_error == 0 &&
               source->lifecycle.save.region_bytecode == 0 &&
               source->lifecycle.save.metadata.status == MC_WORLD_STORAGE_OK &&
               state.chunk->dirty && second->dirty);
    }
    if (mode == SP_EXPORT)
        VERIFY(sp_export_contract(&state, &before) == 0);
    else
        VERIFY(after.owned && after.raw_exports == before.raw_exports);
    VERIFY(ss_copy(source, "after", "level.dat", "level.dat") == 0 &&
           ss_copy(source, "after", "region/r.0.0.mcr", "region.mcr") == 0);
    printf("STAGE_PROGRESS_CASE mode=%s calls=%zu nested=%zu blocked=%zu status=%d owned=%d "
           "raw_exports=%" PRIu64 " dirty=%d metadata=%d binding=1 balanced=1\n",
           name, state.calls, state.nested_calls, state.blocked_calls, (int)status, after.owned,
           after.raw_exports, state.chunk->dirty, state.metadata_returned);
    return 0;
}

int main(int argc, char **argv) {
    VERIFY(argc == 3);
    const char *mode = argv[2];
    /* Keep the actual old public mutable-context and callback-export controls. */
    if (strcmp(mode, "progress") == 0 || strcmp(mode, "callback") == 0 ||
        strcmp(mode, "callback-export") == 0)
        return mc_stage_original_owned_cache_program(argc, argv);
    const bool success = strcmp(mode, "stage-success") == 0;
    const bool level = strcmp(mode, "stage-level-fatal") == 0;
    const bool chunks = strcmp(mode, "stage-chunks-fatal") == 0;
    const bool null_callback = strcmp(mode, "stage-null") == 0;
    const bool reentry = strcmp(mode, "stage-reentry") == 0;
    const bool export_inside = strcmp(mode, "stage-export") == 0;
    VERIFY(success || level || chunks || null_callback || reentry || export_inside);
    source_save_fixture *source = calloc(1, sizeof(*source));
    VERIFY(source != NULL && ss_setup(source, argv[1]) == 0 && oc_open(source, SIZE_MAX) == 0);
    mc_world_runtime_lifecycle_error null_error;
    VERIFY(mc_world_runtime_save_stages_detailed(NULL, true, NULL, NULL, &null_error) ==
               MC_CHUNK_LIFECYCLE_INVALID_ARGUMENT &&
           null_error.status == MC_CHUNK_LIFECYCLE_INVALID_ARGUMENT &&
           null_error.cause.status == MC_WORLD_STORAGE_INVALID_ARGUMENT &&
           strcmp(null_error.cause.operation, "runtime lifecycle owner") == 0 &&
           mc_world_runtime_save_stages_detailed(NULL, true, NULL, NULL, NULL) ==
               MC_CHUNK_LIFECYCLE_INVALID_ARGUMENT);
    const sp_mode selected = success         ? SP_SUCCESS
                             : level         ? SP_LEVEL_FATAL
                             : chunks        ? SP_CHUNKS_FATAL
                             : null_callback ? SP_NULL
                             : reentry       ? SP_REENTRY
                                             : SP_EXPORT;
    VERIFY(sp_case(source, selected, mode) == 0 && oc_stop(source) == 0 && ss_finish(source) == 0);
    printf("STAGE_PROGRESS_FINISH mode=%s checks=%u cleanup=1 genuine-source-world=1\n", mode,
           checks);
    VERIFY(fflush(stdout) == 0);
    free(source);
    return 0;
}
