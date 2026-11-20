/* Included only by canonical store.c. Actual source transport admission shares
 * the existing Store/Chunk owner, reservations, stages, leases and import
 * journals. None of the legacy File/region transport guards is removed. */
#include "source_game_stage.h"

static mc_world_storage_status store_source_native_admit(const mc_world_store *store,
                                                         mc_world_storage_error *error) {
    if (store == NULL || store->source == NULL)
        return store_error(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "source native Store owner");
    const mc_world_store_source *s = store->source;
    if (s->state.active || s->classes->active)
        return store_error(error, MC_WORLD_STORAGE_BUSY, "source Store active native bookkeeping");
    if (s->state.phase != MC_WORLD_SOURCE_READY)
        return store_error(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                           "source Store native bookkeeping phase");
    return MC_WORLD_STORAGE_OK;
}

static mc_stored_chunk_status store_source_normalize(mc_world_store *store, store_chunk *entry,
                                                     mc_world_storage_error *error) {
    mc_world_store_source_classes *classes = store->source->classes;
    if (!classes->block_map_ready) {
        store_error(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                    "actual shared vi map not initialized");
        return MC_STORED_CHUNK_UNSUPPORTED_EDIT;
    }
    return mc_stored_chunk_normalize(entry->owner.stored, classes->block_map,
                                     sizeof(classes->block_map));
}

mc_world_storage_status mc_world_store_source_adopt_game(mc_world_store *store,
                                                         mc_world_runtime *world, int32_t dimension,
                                                         mc_nbt_document **document,
                                                         mc_world_store_game_stage **out,
                                                         mc_world_storage_error *error) {
    mc_world_storage_status status = store_source_native_admit(store, error);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    if (world == NULL || document == NULL || *document == NULL || out == NULL)
        return store_error(error, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                           "source GAME document adoption");
    mc_world_store_reservation *reservation = NULL;
    status = mc_world_store_reserve_chunk(store, dimension, &reservation, error);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    store_chunk *entry = reservation->entry;
    mc_stored_chunk_error detail;
    mc_stored_chunk_status decoded = mc_stored_chunk_adopt_game_document(
        document, &store->limits.nbt, &entry->owner.stored, &detail);
    if (decoded != MC_STORED_CHUNK_OK) {
        (void)mc_world_store_cancel_reservation(store, reservation, NULL);
        return store_chunk_error(error, decoded, &detail);
    }
    status = mc_world_chunk_storage_bind_loaded(world, store_chunk_state(entry),
                                                store->limits.nbt.max_allocation_bytes,
                                                &entry->loaded_binding, error);
    /* The document is now adopted; retain the exact partial candidate even if
     * later concrete-header allocation fails. No decode/reencode or retry. */
    mc_world_store_reservation **link = store_find_reservation(store, reservation);
    *link = reservation->next;
    --store->reservation_count;
    free(reservation);
    entry->game_stage = true;
    entry->stage.entry = entry;
    entry->source_header_failed = status != MC_WORLD_STORAGE_OK;
    entry->phase =
        status == MC_WORLD_STORAGE_OK ? MC_WORLD_STORE_GAME_PREPARED : MC_WORLD_STORE_GAME_FAILED;
    store_publish_chunk(store, entry, dimension);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    status = mc_world_store_lease_chunk(store, store_chunk_state(entry), error);
    if (status != MC_WORLD_STORAGE_OK) {
        entry->phase = MC_WORLD_STORE_GAME_FAILED;
        mc_world_chunk_source_disable(entry->loaded_binding);
        return status;
    }
    entry->coordinator_lease = true;
    *out = &entry->stage;
    return store_error(error, MC_WORLD_STORAGE_OK, NULL);
}

const uint8_t *mc_world_store_source_normalization_map(const mc_world_store *store) {
    return store != NULL && store->source != NULL && store->source->classes->block_map_ready
               ? store->source->classes->block_map
               : NULL;
}

bool mc_world_store_source_normalizer(mc_world_store *store, mc_world_source_normalizer *out) {
    if (store == NULL || store->source == NULL || out == NULL || store->source->state.active ||
        store->source->classes->active)
        return false;
    mc_world_store_source_classes *classes = store->source->classes;
    *out = (mc_world_source_normalizer){classes->block_map, &classes->block_map_ready,
                                        &classes->block_map_failed, &classes->block_map_failure,
                                        &classes->block_map_registry};
    return true;
}
