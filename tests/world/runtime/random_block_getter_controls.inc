/* Native caller contracts, separate from unchanged-original Block profiles.
 * Reuse the complete fieldwise acquisition/descendant comparator through a
 * passive diagnostic record; no EW owner or EW Source method is involved. */
#include "mc/world/runtime/chunk_cache.h"

#define ew_text_equal n_text_equal
#define ew_error_equal rb_getter_record_equal
#include "../../benchmarks/chunk_cache_error_equal.inc"
#undef ew_error_equal
#undef ew_text_equal

static bool rb_getter_same_cause(const mc_world_storage_error *a, const mc_world_storage_error *b) {
    return a->status == b->status && n_text_equal(a->operation, b->operation) &&
           a->system_error == b->system_error;
}

static bool rb_getter_projected(const mc_world_mutation_error *actual,
                                const mc_world_runtime_acquisition_error *reference) {
    bool ok = false;
    mc_client_chunk_cache_error *a = calloc(1, sizeof(*a));
    mc_client_chunk_cache_error *b = calloc(1, sizeof(*b));
    CHECK(a != NULL && b != NULL);
    a->acquisition = *reference;
    /* rb_acquisition does not project detail.cause: scalar and acquiring
     * failures instead retain the independently checked query/top-level cause. */
    a->acquisition.cause = (mc_world_storage_error){0};
    b->acquisition.status = actual->query.acquisition;
    b->acquisition.registry = actual->registry;
    b->acquisition.tile_entity = actual->tile_entity;
    b->acquisition.batch = actual->batch;
    b->acquisition.activation_bytecode = actual->activation_bytecode;
    b->acquisition.activation_world_bytecode = actual->activation_world_bytecode;
    b->acquisition.load_bytecode = actual->load_bytecode;
    b->acquisition.load_builder_bytecode = actual->load_builder_bytecode;
    b->acquisition.load_entity = actual->load_entity;
    b->acquisition.population = actual->query.population;
    b->acquisition.source_chunk = actual->query.source_chunk;
    CHECK(rb_getter_record_equal(a, b) &&
          n_same_error(&actual->query.registry, &reference->registry));
    /* All three declared failures precede population. The untouched outer
     * population record must still equal its full canonical initial record. */
    CHECK(!reference->population.reached);
    a->acquisition.population = (mc_world_population_context){0};
    b->acquisition.population = actual->population;
    CHECK(rb_getter_record_equal(a, b));
    ok = true;
cleanup:
    free(b);
    free(a);
    return ok;
}

static bool rb_getter_provider_delta(const mc_chunk_provider_cache_observation *before,
                                     const mc_chunk_provider_cache_observation *after,
                                     uint64_t gets) {
    /* This fixture's actual exported provider uses the generic GET path.
     * Presence checks have their own Source calls and do not perform GETs. */
    return !before->owned && !after->owned && before->map_count == after->map_count &&
           before->insertion_count == after->insertion_count &&
           before->index_capacity == after->index_capacity &&
           before->index_bytes == after->index_bytes &&
           before->mutation_depth == after->mutation_depth &&
           before->direct_bytes == after->direct_bytes && before->hits == after->hits &&
           before->misses == after->misses && after->generic_gets == before->generic_gets + gets &&
           before->raw_exports == after->raw_exports && before->rebuilds == after->rebuilds;
}

/* Each case obtains an independent public-query reference before the actual
 * single Leaves invocation. No failing child is replayed within that call.
 * All authored faults and cells are restored on every cleanup path. */
static bool rb_getter_caller_case(fixture *f, const mc_block *leaves, unsigned int mode) {
    bool ok = false, authored = false;
    mc_world_runtime *w = f->worlds[0];
    mc_chunk_state *chunk = f->chunks[0][0][0];
    mc_chunk_provider *provider = w->fields.v;
    mc_chunk_data *data = chunk->data;
    const size_t capacity = data == NULL ? 0 : data->blocks_capacity;
    const size_t position = cell(8, 48, 8);
    uint8_t old_block = 0, old_metadata = 0;
    const bool remote = w->fields.B;
    mc_world_mutation_error *error = malloc(sizeof(*error));
    mc_world_runtime_acquisition_error *reference = malloc(sizeof(*reference));
    mc_chunk_data *data_before = malloc(sizeof(*data_before));
    uint8_t *blocks_before = malloc(MC_CHUNK_BLOCKS);
    int32_t *scratch_before = malloc(32768u * sizeof(*scratch_before));
    CHECK(mode < 3 && chunk != NULL && data != NULL && data->blocks != NULL && provider != NULL &&
          leaves->subtype.bk.a != NULL && error != NULL && reference != NULL &&
          data_before != NULL && blocks_before != NULL && scratch_before != NULL &&
          w->fields.r != NULL && w->fields.C != NULL &&
          mc_chunk_nibbles_get(data->metadata, MC_CHUNK_NIBBLE_BYTES, 8, 48, 8, &old_metadata));
    old_block = data->blocks[position];
    authored = true;
    data->blocks[position] = 18;
    CHECK(mc_chunk_nibbles_set(data->metadata, MC_CHUNK_NIBBLE_BYTES, 8, 48, 8, 8));
    w->fields.B = false;
    if (mode == 0)
        data->blocks_capacity = 0; /* Metadata succeeds; first Block payload read fails. */
    else if (mode == 1)
        w->fields.v = NULL; /* Metadata fails before reaching its provider GET. */
    else
        chunk->data = NULL; /* Real cached normal-Chunk acquisition rejection. */

    mc_chunk_provider_cache_observation before_reference, after_reference, after_caller;
    CHECK(mc_chunk_provider_cache_observe(provider, &before_reference) && !before_reference.owned);
    const size_t active = w->active_calls, leases = w->local_leases;
    const size_t bindings = w->persistent_bindings;
    const size_t registry_bytes = f->registry[0]->allocation_bytes;
    const size_t registry_managed = f->registry[0]->managed_current_bytes;
    const size_t registry_peak = f->registry[0]->total_peak_bytes;
    const unsigned int registry_entered = f->registry[0]->entered;
    const size_t registry_borrows = f->registry[0]->runtime_borrows;
    const uint64_t unique = atomic_load_explicit(&f->profile->uniquifier, memory_order_relaxed);
    const size_t profile_bytes = f->profile->allocated_bytes;
    const size_t dimension_holds = mc_world_provider_hold_count(f->dimensions[0]);
    const mc_java_random world_random = *w->fields.r, math_random = math_snapshot(f);
    mc_java_random parameter;
    mc_java_random_seed(&parameter, 17);
    const mc_java_random supplied = parameter;
    int32_t *const scratch = leaves->subtype.bk.a;
    memcpy(scratch_before, scratch, 32768u * sizeof(*scratch_before));
    memcpy(data_before, data, sizeof(*data_before));
    memcpy(blocks_before, data->blocks, MC_CHUNK_BLOCKS);
    mc_chunk_state state_before;
    memcpy(&state_before, chunk, sizeof(state_before));
    const size_t entities = w->fields.b->count;
    const size_t light_records = w->fields.C->count;
    const size_t store_chunks = mc_world_store_chunk_count(f->stores[0]);
    int32_t unchanged = 777;
    mc_world_storage_error cause = {0};
    memset(reference, 0xa5, sizeof(*reference));
    const mc_world_storage_status status =
        mode == 0
            ? mc_world_runtime_block_get_detailed(w, 4, 44, 4, &unchanged, &cause, reference)
            : mc_world_runtime_metadata_get_detailed(w, 8, 48, 8, &unchanged, &cause, reference);
    CHECK(status == MC_WORLD_STORAGE_UNSUPPORTED_STATE && unchanged == 777 &&
          cause.status == status &&
          reference->status ==
              (mode == 2 ? MC_CHUNK_ACQUIRE_INVALID_ARGUMENT : MC_CHUNK_ACQUIRE_OK) &&
          reference->source_chunk == NULL &&
          mc_chunk_provider_cache_observe(provider, &after_reference) &&
          rb_getter_provider_delta(&before_reference, &after_reference, mode == 1 ? 0u : 1u));

    const bool returned = random_block_call(w, leaves, 8, 48, 8, &parameter, error);
    CHECK(!returned && error->status == status && n_text_equal(error->operation, cause.operation) &&
          rb_getter_same_cause(&error->cause, &cause) &&
          rb_getter_same_cause(&error->query.cause, &cause) &&
          rb_getter_same_cause(mc_world_runtime_last_error(w), &cause) &&
          error->query.status == status && n_text_equal(error->query.operation, cause.operation) &&
          error->query.bytecode == 0 && error->query.provider_bytecode == 0 &&
          error->query.chunk_bytecode == 0 && strcmp(error->hook_class, "bk") == 0 &&
          strcmp(error->hook_method, "a(Lfd;IIILjava/util/Random;)V") == 0 &&
          error->hook_bytecode == (mode == 0 ? 156u : 13u) && error->cause_hook_class == NULL &&
          error->cause_hook_method == NULL && error->cause_hook_bytecode == 0 &&
          error->presence_bytecode == 0 && rb_getter_projected(error, reference));
    CHECK(mc_chunk_provider_cache_observe(provider, &after_caller) &&
          rb_getter_provider_delta(&after_reference, &after_caller,
                                   mode == 0   ? 2u
                                   : mode == 1 ? 0u
                                               : 1u) &&
          w->active_calls == active && w->local_leases == leases &&
          w->persistent_bindings == bindings &&
          mc_world_provider_hold_count(f->dimensions[0]) == dimension_holds &&
          f->registry[0]->allocation_bytes == registry_bytes &&
          f->registry[0]->managed_current_bytes == registry_managed &&
          f->registry[0]->total_peak_bytes == registry_peak &&
          f->registry[0]->entered == registry_entered &&
          f->registry[0]->runtime_borrows == registry_borrows &&
          f->profile->allocated_bytes == profile_bytes &&
          atomic_load_explicit(&f->profile->uniquifier, memory_order_relaxed) == unique &&
          mc_test_java_random_equal(w->fields.r, &world_random) &&
          mc_test_java_random_equal(&parameter, &supplied) && math_is(f, &math_random) &&
          leaves->subtype.bk.a == scratch &&
          memcmp(scratch, scratch_before, 32768u * sizeof(*scratch)) == 0 &&
          memcmp(data, data_before, sizeof(*data)) == 0 &&
          memcmp(data->blocks, blocks_before, MC_CHUNK_BLOCKS) == 0 &&
          memcmp(chunk, &state_before, sizeof(*chunk)) == 0 && w->fields.b->count == entities &&
          w->fields.C->count == light_records &&
          mc_world_store_chunk_count(f->stores[0]) == store_chunks);
    uint8_t metadata = 0;
    CHECK(data->blocks[position] == 18 &&
          mc_chunk_nibbles_get(data->metadata, MC_CHUNK_NIBBLE_BYTES, 8, 48, 8, &metadata) &&
          metadata == 8);
    ok = true;
cleanup:
    if (authored) {
        w->fields.v = provider;
        w->fields.B = remote;
        chunk->data = data;
        data->blocks_capacity = capacity;
        data->blocks[position] = old_block;
        if (!mc_chunk_nibbles_set(data->metadata, MC_CHUNK_NIBBLE_BYTES, 8, 48, 8, old_metadata))
            ok = false;
    }
    free(scratch_before);
    free(blocks_before);
    free(data_before);
    free(reference);
    free(error);
    return ok;
}

static bool random_block_getter_contract(fixture *f, const mc_block *leaves) {
    bool ok = false;
    CHECK(rb_getter_caller_case(f, leaves, 0) && rb_getter_caller_case(f, leaves, 1) &&
          rb_getter_caller_case(f, leaves, 2));
    puts("BLOCK_RANDOM getters metadata-then-block-failure=1 provider-null=1 cached-data-null=1 "
         "complete-projection=1 exact-gets=1 native-only=1");
    ok = true;
cleanup:
    return ok;
}
