#include "construction_internal.h"
#include "runtime_internal.h"
#include "source_construction_internal.h"
#include "source_terminal_internal.h"

#include <stdlib.h>

static mc_world_storage_status result(mc_world_storage_error *e, mc_world_storage_status status,
                                      const char *operation) {
    if (e != NULL)
        *e = (mc_world_storage_error){status, operation, 0};
    return status;
}

static mc_world_storage_status source_result(mc_world_storage_error *e,
                                             const mc_world_source_error *source) {
    mc_world_storage_status status;
    switch (source->status) {
        case MC_WORLD_SOURCE_OK:
            status = MC_WORLD_STORAGE_OK;
            break;
        case MC_WORLD_SOURCE_ARGUMENT:
            status = MC_WORLD_STORAGE_INVALID_ARGUMENT;
            break;
        case MC_WORLD_SOURCE_BUSY:
            status = MC_WORLD_STORAGE_BUSY;
            break;
        case MC_WORLD_SOURCE_LIMIT:
            status = MC_WORLD_STORAGE_LIMIT;
            break;
        case MC_WORLD_SOURCE_ALLOCATION:
            status = MC_WORLD_STORAGE_ALLOCATION_FAILED;
            break;
        case MC_WORLD_SOURCE_FILE:
        case MC_WORLD_SOURCE_OUTPUT:
            status = MC_WORLD_STORAGE_IO_ERROR;
            break;
        case MC_WORLD_SOURCE_SOURCE_EXCEPTION:
            status = MC_WORLD_STORAGE_INVALID_DATA;
            break;
        default:
            status = MC_WORLD_STORAGE_UNSUPPORTED_STATE;
            break;
    }
    /* Compatibility summary only; the entire typed cause remains World-owned. */
    return result(e, status, source->operation);
}

mc_world_storage_status mc_world_runtime_source_bind_store(mc_world_runtime *world,
                                                           mc_world_store *store,
                                                           mc_world_storage_error *e) {
    if (!mc_world_store_is_source(store))
        return MC_WORLD_STORAGE_OK;
    if (world->classes->side != MC_WORLD_INFO_CLIENT)
        return result(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                      "source SaveHandler client World domain");
    if (world->source_construction != NULL)
        return result(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World source Store already bound");
    const size_t bytes = sizeof(mc_world_runtime_source_construction);
    const size_t used = world->construction.direct_allocation_bytes;
    const size_t maximum = world->construction_limits.max_direct_allocation_bytes;
    if (used > maximum || bytes > maximum - used)
        return result(e, MC_WORLD_STORAGE_LIMIT, "World source construction retained owner bound");
    mc_world_runtime_source_construction *source = calloc(1, bytes);
    if (source == NULL)
        return result(e, MC_WORLD_STORAGE_ALLOCATION_FAILED,
                      "World source construction retained owner");
    world->source_construction = source;
    world->construction.direct_allocation_bytes += bytes;
    source->retention.status = mc_world_store_source_retain(store, &source->retention);
    if (source->retention.status != MC_WORLD_SOURCE_OK)
        return source_result(e, &source->retention);
    source->store = store;
    return MC_WORLD_STORAGE_OK;
}

mc_world_storage_status mc_world_runtime_source_saved_data(mc_world_runtime *world,
                                                           mc_world_storage_error *e) {
    mc_world_runtime_source_construction *source = world->source_construction;
    if (source == NULL || source->store == NULL || source->retiring ||
        source->pending_saved_data != NULL || source->saved_data_published)
        return result(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World source HC ownership state");
    mc_world_storage_status status = mc_world_saved_data_source_allocate(
        world->classes->side, &world->construction_limits.saved_data, &source->pending_saved_data,
        &source->saved_data_failure);
    if (status == MC_WORLD_STORAGE_OK)
        status = mc_world_saved_data_source_construct(source->pending_saved_data, world->fields.w,
                                                      &source->saved_data_failure);
    if (status != MC_WORLD_STORAGE_OK)
        return result(e, status, "World actual source SavedData constructor");
    world->fields.z = source->pending_saved_data;
    source->pending_saved_data = NULL;
    source->saved_data_published = true;
    return MC_WORLD_STORAGE_OK;
}

mc_world_storage_status mc_world_runtime_source_read_info(mc_world_runtime *world,
                                                          mc_world_info **out,
                                                          mc_world_storage_error *e) {
    mc_world_runtime_source_construction *source = world->source_construction;
    if (source == NULL || source->store == NULL || source->retiring || source->metadata_called)
        return result(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World source metadata call state");
    source->metadata_called = true;
    source->metadata.status =
        mc_world_store_source_read_info(world->fields.w, out, &source->metadata);
    if (source->metadata.status != MC_WORLD_SOURCE_OK)
        return source_result(e, &source->metadata);
    source->metadata_returned = true;
    source->loaded_info_was_null = *out == NULL;
    return MC_WORLD_STORAGE_OK;
}

mc_world_storage_status mc_world_runtime_source_create_loader(mc_world_runtime *world,
                                                              mc_world_storage_error *e) {
    mc_world_runtime_source_construction *source = world->source_construction;
    if (source == NULL || source->store == NULL || source->retiring || source->loader != NULL)
        return result(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World source loader call state");
    source->loader_failure.status = mc_world_store_source_loader(
        world->fields.w, world->fields.t, &source->loader, &source->loader_failure);
    return source_result(e, &source->loader_failure);
}

mc_world_source_chunk_loader *mc_world_runtime_source_loader(mc_world_runtime *world) {
    return world == NULL || world->source_construction == NULL ? NULL
                                                               : world->source_construction->loader;
}

mc_world_storage_status mc_world_runtime_source_prepare_free(mc_world_runtime *world,
                                                             mc_world_storage_error *e) {
    mc_world_runtime_source_construction *source = world->source_construction;
    if (source == NULL)
        return MC_WORLD_STORAGE_OK;
    source->retiring = true;
    mc_world_storage_status status =
        mc_world_saved_data_retire(&source->pending_saved_data, &source->saved_data_cleanup);
    if (status != MC_WORLD_STORAGE_OK)
        return result(e, status, "World pending source HC native retirement");
    if (source->saved_data_published) {
        status = mc_world_saved_data_retire(&world->fields.z, &source->saved_data_cleanup);
        if (status != MC_WORLD_STORAGE_OK)
            return result(e, status, "World published source HC native retirement");
        source->saved_data_published = false;
    }
    source->loader_cleanup.status =
        mc_world_source_chunk_loader_retire(&source->loader, &source->loader_cleanup);
    if (source->loader_cleanup.status != MC_WORLD_SOURCE_OK)
        return source_result(e, &source->loader_cleanup);
    source->store_cleanup.status =
        mc_world_store_source_release(&source->store, &source->store_cleanup);
    if (source->store_cleanup.status != MC_WORLD_SOURCE_OK)
        return source_result(e, &source->store_cleanup);
    /* All remaining generic destruction is infallible and has no Store/loader
     * operation. End these consumed native views, without crediting a source
     * field store, IO close/finally, unload or session-lock deletion. */
    world->fields.w = NULL;
    world->options.store = NULL;
    world->loader.store = NULL;
    return MC_WORLD_STORAGE_OK;
}

void mc_world_runtime_source_free(mc_world_runtime *world) {
    free(world->source_construction);
    world->source_construction = NULL;
}

mc_world_storage_status mc_world_runtime_terminal_source_inspect(const mc_world_runtime *world,
                                                                 mc_world_terminal_source_view *out,
                                                                 mc_world_storage_error *error) {
    if (world == NULL || out == NULL)
        return result(error, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                      "World terminal source inspection arguments");
    if (world->active_calls != 0 || world->local_leases != 0)
        return result(error, MC_WORLD_STORAGE_BUSY, "World terminal source call or lease active");
    const mc_world_runtime_source_construction *source = world->source_construction;
    if (world->construction.mode != MC_WORLD_RUNTIME_GAME ||
        world->construction.phase != MC_WORLD_RUNTIME_READY ||
        world->construction.entry != MC_WORLD_RUNTIME_CLIENT_LOAD_OR_CREATE ||
        world->classes == NULL || world->classes->side != MC_WORLD_INFO_CLIENT ||
        world->classes->profile == NULL ||
        world->options.random_profile != world->classes->profile ||
        !world->construction.contexts_bound || !world->construction.cache_registered ||
        world->construction.failed_call_pc != 0 || source == NULL || source->retiring ||
        source->store == NULL || source->pending_saved_data != NULL ||
        !source->saved_data_published || !source->metadata_called || !source->metadata_returned ||
        source->retention.status != MC_WORLD_SOURCE_OK ||
        source->metadata.status != MC_WORLD_SOURCE_OK ||
        source->saved_data_failure.status != MC_WORLD_STORAGE_OK ||
        source->loader_failure.status != MC_WORLD_SOURCE_OK || source->loader == NULL ||
        world->fields.z == NULL || world->fields.x == NULL || world->fields.t == NULL ||
        world->fields.v == NULL || world->fields.s != source->loaded_info_was_null ||
        world->fields.w != source->store || world->options.store != source->store ||
        world->loader.store != source->store || !mc_world_store_is_source(source->store))
        return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                      "World terminal intact selected source construction");

    mc_world_source_state store;
    if (!mc_world_store_source_inspect(source->store, &store))
        return result(error, MC_WORLD_STORAGE_BUSY, "World terminal source Store active");
    if (store.phase != MC_WORLD_SOURCE_READY || store.holds == 0 || store.classes == NULL ||
        !store.mx_returned || !store.fm_returned)
        return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                      "World terminal source Store construction provenance");
    mc_world_source_class_state classes;
    if (!mc_world_store_source_classes_inspect(store.classes, &classes))
        return result(error, MC_WORLD_STORAGE_BUSY, "World terminal source Store classes active");
    if (classes.phase != MC_WORLD_SOURCE_READY || classes.holds == 0 || classes.handlers == 0 ||
        classes.files == NULL || classes.logging == NULL ||
        mc_world_store_source_files(source->store) != classes.files)
        return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                      "World terminal source Store class provenance");

    mc_world_store *const *hc_store;
    mc_world_storage_status status =
        mc_world_saved_data_terminal_store_slot(world->fields.z, &hc_store, error);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    mc_world_terminal_loader_view loader;
    status = mc_world_source_chunk_loader_terminal_inspect(source->loader, &loader, error);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    if (loader.holds == 0 || hc_store == NULL || loader.store_slot == NULL ||
        loader.regions_slot == NULL || *hc_store != source->store ||
        *loader.store_slot != source->store)
        return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                      "World terminal actual source owning Store slots");
    if (loader.holds != 1)
        return result(error, MC_WORLD_STORAGE_BUSY, "World terminal external loader occurrence");
    mc_world_source_regions *regions = mc_world_store_source_regions(source->store);
    if (regions == NULL || *loader.regions_slot != regions)
        return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                      "World terminal actual source shared regions identity");
    mc_world_source_regions_state region_state;
    if (!mc_world_source_regions_inspect(regions, &region_state) || region_state.retiring)
        return result(error, MC_WORLD_STORAGE_BUSY, "World terminal source shared regions active");
    if (region_state.holds == 0 || region_state.files != classes.files)
        return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                      "World terminal source shared File namespace provenance");

    *out = (mc_world_terminal_source_view){
        .store_slots = {&source->store, hc_store, loader.store_slot},
        .loader_slot = &source->loader};
    return result(error, MC_WORLD_STORAGE_OK, NULL);
}

bool mc_world_runtime_source_construction_inspect(const mc_world_runtime *world,
                                                  mc_world_runtime_source_construction_state *out) {
    if (world == NULL || world->source_construction == NULL || out == NULL)
        return false;
    const mc_world_runtime_source_construction *source = world->source_construction;
    *out = (mc_world_runtime_source_construction_state){
        .store = source->store,
        .pending_saved_data = source->pending_saved_data,
        .loader = source->loader,
        .store_held = source->store != NULL,
        .saved_data_published = source->saved_data_published,
        .metadata_called = source->metadata_called,
        .metadata_returned = source->metadata_returned,
        .loaded_info_was_null = source->loaded_info_was_null,
        .retiring = source->retiring,
        .retention = &source->retention,
        .metadata = &source->metadata,
        .loader_failure = &source->loader_failure,
        .store_cleanup = &source->store_cleanup,
        .loader_cleanup = &source->loader_cleanup,
        .saved_data_failure = &source->saved_data_failure,
        .saved_data_cleanup = &source->saved_data_cleanup};
    return true;
}

const mc_world_source_error *
mc_world_runtime_last_source_store_error(const mc_world_runtime *world) {
    if (world == NULL || world->source_construction == NULL)
        return NULL;
    const mc_world_runtime_source_construction *source = world->source_construction;
    return source->metadata_called ? &source->metadata : &source->retention;
}
