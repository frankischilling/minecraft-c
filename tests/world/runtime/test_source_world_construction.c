/* Canonical client LOAD_OR_CREATE with real source File/HC/fm/ld owners. */
#include "../../../src/world/runtime/runtime_internal.h"
#include "../../../src/world/runtime/source_construction_internal.h"
#include "../../../src/world/runtime/source_terminal_internal.h"
#include "mc/world/dimension/world_provider.h"
#include "mc/world/runtime/queries.h"
#include "mc/world/runtime/source_construction.h"
#include "mc/world/storage/source_regions.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;
static mc_math_trig trig;
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(x)) {                                                                                \
            fprintf(stderr, "source World check%u line%d: %s\n", checks, __LINE__, #x);            \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

typedef struct fixture {
    mc_java_files *files;
    mc_java_logging *logging;
    mc_java_file *root;
    mc_world_store_source_classes *handlers;
    mc_world_source_regions *regions;
    mc_java_random_profile *profile;
    mc_world_runtime_classes *classes;
    mc_world_info_name *display;
    mc_world_source_error source;
    mc_world_source_region_error region;
    mc_world_storage_error error;
} fixture;

typedef enum scenario {
    PRIMARY,
    OLD_ONLY,
    BAD_PRIMARY,
    BAD_OLD,
    BAD_IDS,
    LIMITED_IDS,
    NETHER,
    MISSING
} scenario;

static mc_world_info_name *text(const char *ascii) {
    const size_t n = strlen(ascii);
    uint16_t *units = malloc((n + 1u) * sizeof(*units));
    if (units == NULL)
        return NULL;
    for (size_t i = 0; i < n; ++i) {
        if ((uint8_t)ascii[i] >= 128u) {
            free(units);
            return NULL;
        }
        units[i] = (uint8_t)ascii[i];
    }
    mc_world_info_name *out = NULL;
    const mc_world_info_status status =
        mc_world_info_name_copy((mc_nbt_string){units, n}, SIZE_MAX, &out);
    free(units);
    return status == MC_WORLD_INFO_OK ? out : NULL;
}

typedef struct terminal_snapshot {
    unsigned char world[sizeof(mc_world_runtime)];
    unsigned char sidecar[sizeof(mc_world_runtime_source_construction)];
    bool has_sidecar, has_hc, has_loader;
    mc_world_source_state handler;
    mc_world_source_class_state classes;
    mc_world_source_regions_state regions;
    mc_world_saved_data_source_state hc;
    mc_world_source_chunk_loader_state loader;
    mc_world_source_error handler_failure, handler_cleanup, class_failure, class_cleanup;
    mc_world_source_region_error region_failure, region_cleanup;
    mc_world_saved_data_source_error hc_failure, hc_cleanup;
    mc_world_source_chunk_error loader_failure, loader_cleanup;
} terminal_snapshot;

static int terminal_capture(const fixture *f, const mc_world_runtime *world,
                            const mc_world_store *store, terminal_snapshot *out) {
    memcpy(out->world, world, sizeof(out->world));
    CHECK(mc_world_store_source_inspect(store, &out->handler));
    CHECK(mc_world_store_source_classes_inspect(f->handlers, &out->classes));
    CHECK(mc_world_source_regions_inspect(f->regions, &out->regions));
    memcpy(&out->handler_failure, out->handler.failure, sizeof(out->handler_failure));
    memcpy(&out->handler_cleanup, out->handler.cleanup, sizeof(out->handler_cleanup));
    memcpy(&out->class_failure, out->classes.failure, sizeof(out->class_failure));
    memcpy(&out->class_cleanup, out->classes.cleanup, sizeof(out->class_cleanup));
    memcpy(&out->region_failure, out->regions.failure, sizeof(out->region_failure));
    memcpy(&out->region_cleanup, out->regions.cleanup, sizeof(out->region_cleanup));
    const mc_world_runtime_source_construction *source = world->source_construction;
    if (source != NULL) {
        out->has_sidecar = true;
        memcpy(out->sidecar, source, sizeof(out->sidecar));
        const mc_world_saved_data *hc =
            source->saved_data_published ? world->fields.z : source->pending_saved_data;
        if (hc != NULL) {
            out->has_hc = true;
            CHECK(mc_world_saved_data_source_inspect(hc, &out->hc));
            memcpy(&out->hc_failure, out->hc.failure, sizeof(out->hc_failure));
            memcpy(&out->hc_cleanup, out->hc.cleanup, sizeof(out->hc_cleanup));
        }
        if (source->loader != NULL) {
            out->has_loader = true;
            CHECK(mc_world_source_chunk_loader_inspect(source->loader, &out->loader));
            memcpy(&out->loader_failure, out->loader.failure, sizeof(out->loader_failure));
            memcpy(&out->loader_cleanup, out->loader.cleanup, sizeof(out->loader_cleanup));
        }
    }
    return 0;
}

static int terminal_attempt(const fixture *f, mc_world_runtime *world,
                            mc_world_store *const *caller_store_slot,
                            mc_world_storage_status world_status,
                            mc_world_storage_status store_status) {
    /* Large retained causes and raw canonical-owner bytes belong on the heap.
     * None of these snapshots is passed as a replacement source owner. */
    terminal_snapshot *before = calloc(1, sizeof(*before)), *after = calloc(1, sizeof(*after));
    CHECK(before != NULL && after != NULL);
    mc_world_store *store = *caller_store_slot;
    CHECK(terminal_capture(f, world, store, before) == 0);
    mc_world_terminal_source_view view;
    memset(&view, 0xa5, sizeof(view));
    unsigned char sentinel[sizeof(view)];
    memcpy(sentinel, &view, sizeof(sentinel));
    mc_world_storage_error error = {0};
    CHECK(mc_world_runtime_terminal_source_inspect(world, &view, &error) == world_status &&
          error.status == world_status);
    if (world_status == MC_WORLD_STORAGE_OK) {
        const mc_world_runtime_source_construction *source = world->source_construction;
        CHECK(view.store_slots[0] == &source->store && view.loader_slot == &source->loader &&
              *view.loader_slot == source->loader && error.operation == NULL);
        mc_world_store *const *hc_store = NULL;
        mc_world_terminal_loader_view loader;
        CHECK(mc_world_saved_data_terminal_store_slot(world->fields.z, &hc_store, NULL) ==
                  MC_WORLD_STORAGE_OK &&
              mc_world_source_chunk_loader_terminal_inspect(source->loader, &loader, NULL) ==
                  MC_WORLD_STORAGE_OK &&
              view.store_slots[1] == hc_store && view.store_slots[2] == loader.store_slot &&
              loader.holds == 1 && *loader.regions_slot == f->regions);
        mc_world_terminal_store_anchor anchors[4] = {{caller_store_slot}};
        for (size_t i = 0; i < 3; ++i) {
            CHECK(view.store_slots[i] != NULL && *view.store_slots[i] == store &&
                  view.store_slots[i] != caller_store_slot);
            for (size_t j = 0; j < i; ++j)
                CHECK(view.store_slots[i] != view.store_slots[j]);
            anchors[i + 1].slot = view.store_slots[i];
        }
        CHECK(mc_world_store_terminal_source_admit(store, anchors, 4, &error) == store_status &&
              error.status == store_status);
        if (store_status == MC_WORLD_STORAGE_OK)
            CHECK(before->handler.holds == sizeof(anchors) / sizeof(anchors[0]) &&
                  error.operation == NULL);
        else
            CHECK(store_status == MC_WORLD_STORAGE_BUSY &&
                  before->handler.holds == sizeof(anchors) / sizeof(anchors[0]) + 1 &&
                  error.operation != NULL &&
                  strcmp(error.operation, "Store terminal source external Handler occurrence") ==
                      0);
        view = (mc_world_terminal_source_view){0};
        CHECK(mc_world_runtime_terminal_source_inspect(world, &view, NULL) == MC_WORLD_STORAGE_OK &&
              view.store_slots[0] == &source->store && view.store_slots[1] == hc_store &&
              view.store_slots[2] == loader.store_slot && view.loader_slot == &source->loader);
    } else {
        CHECK(memcmp(sentinel, &view, sizeof(view)) == 0);
        CHECK(mc_world_runtime_terminal_source_inspect(world, &view, NULL) == world_status &&
              memcmp(sentinel, &view, sizeof(view)) == 0);
    }
    CHECK(*caller_store_slot == store && terminal_capture(f, world, store, after) == 0);
    CHECK(memcmp(before->world, after->world, sizeof(before->world)) == 0 &&
          before->has_sidecar == after->has_sidecar && before->has_hc == after->has_hc &&
          before->has_loader == after->has_loader);
    if (before->has_sidecar)
        CHECK(memcmp(before->sidecar, after->sidecar, sizeof(before->sidecar)) == 0);
    /* These three getters copy their actual stored state, not reconstructed
     * aggregate views. Diagnostic objects are copied separately below. */
    CHECK(memcmp(&before->handler, &after->handler, sizeof(before->handler)) == 0);
    if (before->has_hc)
        CHECK(memcmp(&before->hc, &after->hc, sizeof(before->hc)) == 0);
    if (before->has_loader)
        CHECK(memcmp(&before->loader, &after->loader, sizeof(before->loader)) == 0);
    const mc_world_source_class_state *a = &before->classes, *b = &after->classes;
    CHECK(a->phase == b->phase && a->holds == b->holds && a->handlers == b->handlers &&
          a->literal_allocations == b->literal_allocations && a->active == b->active &&
          a->last_store_pc == b->last_store_pc && a->files == b->files &&
          a->logging == b->logging && a->source_a == b->source_a &&
          a->pending_logger == b->pending_logger && a->minecraft == b->minecraft &&
          a->players == b->players && a->data == b->data && a->session_lock == b->session_lock &&
          a->failure == b->failure && a->cleanup == b->cleanup);
    const mc_world_source_regions_state *ra = &before->regions, *rb = &after->regions;
    CHECK(ra->holds == rb->holds && ra->entries == rb->entries && ra->active == rb->active &&
          ra->activated == rb->activated && ra->retiring == rb->retiring &&
          ra->files == rb->files && ra->pending_directory == rb->pending_directory &&
          ra->pending_file == rb->pending_file &&
          ra->pending_random_access == rb->pending_random_access && ra->failure == rb->failure &&
          ra->cleanup == rb->cleanup);
    CHECK(
        memcmp(&before->handler_failure, &after->handler_failure,
               sizeof(before->handler_failure)) == 0 &&
        memcmp(&before->handler_cleanup, &after->handler_cleanup,
               sizeof(before->handler_cleanup)) == 0 &&
        memcmp(&before->class_failure, &after->class_failure, sizeof(before->class_failure)) == 0 &&
        memcmp(&before->class_cleanup, &after->class_cleanup, sizeof(before->class_cleanup)) == 0 &&
        memcmp(&before->region_failure, &after->region_failure, sizeof(before->region_failure)) ==
            0 &&
        memcmp(&before->region_cleanup, &after->region_cleanup, sizeof(before->region_cleanup)) ==
            0 &&
        memcmp(&before->hc_failure, &after->hc_failure, sizeof(before->hc_failure)) == 0 &&
        memcmp(&before->hc_cleanup, &after->hc_cleanup, sizeof(before->hc_cleanup)) == 0 &&
        memcmp(&before->loader_failure, &after->loader_failure, sizeof(before->loader_failure)) ==
            0 &&
        memcmp(&before->loader_cleanup, &after->loader_cleanup, sizeof(before->loader_cleanup)) ==
            0);
    free(before);
    free(after);
    return 0;
}

static int terminal_controls(fixture *f, mc_world_runtime *world, mc_world_store **store_slot) {
    CHECK(terminal_attempt(f, world, store_slot, MC_WORLD_STORAGE_OK, MC_WORLD_STORAGE_OK) == 0);
    mc_world_store *extra_store = *store_slot;
    CHECK(mc_world_store_source_retain(extra_store, &f->source) == MC_WORLD_SOURCE_OK);
    CHECK(terminal_attempt(f, world, store_slot, MC_WORLD_STORAGE_OK, MC_WORLD_STORAGE_BUSY) == 0);
    CHECK(mc_world_store_source_release(&extra_store, &f->source) == MC_WORLD_SOURCE_OK &&
          extra_store == NULL);
    CHECK(terminal_attempt(f, world, store_slot, MC_WORLD_STORAGE_OK, MC_WORLD_STORAGE_OK) == 0);
    mc_world_source_chunk_loader *extra_loader = world->source_construction->loader;
    CHECK(mc_world_source_chunk_loader_retain(extra_loader, &f->source) == MC_WORLD_SOURCE_OK);
    mc_world_source_state handler;
    mc_world_source_chunk_loader_state loader;
    CHECK(mc_world_store_source_inspect(*store_slot, &handler) && handler.holds == 4 &&
          mc_world_source_chunk_loader_inspect(extra_loader, &loader) && loader.holds == 2);
    CHECK(terminal_attempt(f, world, store_slot, MC_WORLD_STORAGE_BUSY, MC_WORLD_STORAGE_OK) == 0);
    CHECK(mc_world_source_chunk_loader_release(&extra_loader, &f->source) == MC_WORLD_SOURCE_OK &&
          extra_loader == NULL);
    CHECK(terminal_attempt(f, world, store_slot, MC_WORLD_STORAGE_OK, MC_WORLD_STORAGE_OK) == 0);
    return 0;
}

static int test_case(fixture *f, const char *folder, scenario selected_case) {
    mc_world_store *store = NULL;
    mc_world_runtime *world = NULL;
    mc_world_info_name *name = text(folder);
    CHECK(name != NULL);
    CHECK(mc_world_store_source_allocate(f->handlers, &store, &f->source) == MC_WORLD_SOURCE_OK);
    CHECK(mc_world_store_source_construct(store, f->root, name, false, &f->source) ==
          MC_WORLD_SOURCE_OK);
    mc_world_info_name_release(name);
    CHECK(mc_world_store_is_source(store) && mc_world_store_root(store) == NULL);
    mc_world_runtime_construction_limits limits = mc_world_runtime_default_construction_limits();
    limits.max_provider_chunks = 4;
    if (selected_case == LIMITED_IDS)
        limits.saved_data.max_ids = 0;
    CHECK(mc_world_runtime_allocate_world(MC_WORLD_RUNTIME_CLIENT_LOAD_OR_CREATE, f->classes,
                                          &limits, &world, &f->error) == MC_WORLD_STORAGE_OK);
    mc_world_runtime_source_construction_state source;
    CHECK(!mc_world_runtime_source_construction_inspect(world, &source));
    CHECK(terminal_attempt(f, world, &store, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                           MC_WORLD_STORAGE_OK) == 0);
    const mc_world_runtime_world_arguments args = {.store = store, .name = f->display, .seed = 777};
    mc_world_source_regions_state region_before, region_after;
    CHECK(mc_world_source_regions_inspect(f->regions, &region_before));
    const mc_world_storage_status status =
        mc_world_runtime_construct_world(world, &args, &f->error);
    mc_world_runtime_fields fields;
    mc_world_runtime_construction state;
    CHECK(mc_world_runtime_fields_get(world, &fields) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_construction_get(world, &state) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_source_construction_inspect(world, &source));
    CHECK(source.store == store && source.store_held && fields.w == store);
    CHECK(mc_world_runtime_classes_free(f->classes, &f->error) == MC_WORLD_STORAGE_BUSY);
    const bool ids_failure = selected_case == BAD_IDS || selected_case == LIMITED_IDS;
    const bool metadata_failure = selected_case == BAD_PRIMARY || selected_case == BAD_OLD;
    if (ids_failure) {
        CHECK(status == (selected_case == LIMITED_IDS ? MC_WORLD_STORAGE_LIMIT
                                                      : MC_WORLD_STORAGE_UNSUPPORTED_STATE));
        CHECK(state.phase == MC_WORLD_RUNTIME_FAILED_CONSTRUCTION && state.failed_call_pc == 273 &&
              state.last_store_pc == 264 && fields.z == NULL && fields.x == NULL && !fields.s);
        CHECK(source.pending_saved_data != NULL && !source.saved_data_published &&
              !source.metadata_called && source.loader == NULL);
        mc_world_saved_data_source_state hc;
        CHECK(mc_world_saved_data_source_inspect(source.pending_saved_data, &hc) &&
              hc.phase == MC_WORLD_SAVED_DATA_SOURCE_FAILED && hc.source_a == store &&
              hc.a_stored && !hc.constructor_returned);
        if (selected_case == BAD_IDS) {
            CHECK(source.saved_data_failure->site.pc == 152 &&
                  source.saved_data_failure->diagnostic_frontier &&
                  source.saved_data_failure->caught_exception && !hc.close_returned);
        } else {
            CHECK(source.saved_data_failure->status == MC_WORLD_STORAGE_LIMIT &&
                  source.saved_data_failure->native_policy && hc.close_returned);
        }
    } else {
        CHECK(fields.z != NULL && source.saved_data_published && source.pending_saved_data == NULL);
        mc_world_saved_data_source_state hc;
        CHECK(mc_world_saved_data_source_inspect(fields.z, &hc) &&
              hc.phase == MC_WORLD_SAVED_DATA_SOURCE_READY && hc.source_a == store &&
              hc.constructor_returned && hc.load_returned && hc.b_stored && hc.c_stored &&
              hc.d_stored && hc.source_b != hc.source_c && hc.source_c != hc.source_d &&
              hc.source_b != hc.source_d);
        CHECK(source.metadata_called &&
              mc_world_runtime_last_source_store_error(world) == source.metadata);
        if (metadata_failure) {
            CHECK(status == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
                  state.phase == MC_WORLD_RUNTIME_FAILED_CONSTRUCTION &&
                  state.failed_call_pc == 281 && state.last_store_pc == 276);
            CHECK(fields.x == NULL && !fields.s && fields.t == NULL && source.loader == NULL &&
                  !source.metadata_returned && source.metadata->diagnostic_frontier &&
                  source.metadata->caught_exception && !source.metadata->native_policy);
            CHECK(source.metadata->frame_count == 1 &&
                  source.metadata->frames[0].pc == (selected_case == BAD_OLD ? 105 : 51));
            mc_world_source_input_state input;
            CHECK(mc_world_store_source_input_inspect(store, &input) && !input.parsed &&
                  input.closed && input.close_pc == 29 && !input.output_transferred &&
                  !input.source_returned && input.old_file == (selected_case == BAD_OLD));
            CHECK(source.metadata->input.source_fault != MC_NBT_SOURCE_NONE &&
                  source.metadata->input_close.source_fault == MC_NBT_SOURCE_NONE);
        } else {
            CHECK(source.metadata_returned && fields.x != NULL);
            mc_world_info_fields info;
            CHECK(mc_world_info_fields_get(fields.x, &info) == MC_WORLD_INFO_OK &&
                  info.name == f->display); /* Exact UTF16 String identity at source390/376. */
            if (selected_case == MISSING) {
                CHECK(source.loaded_info_was_null && fields.s && fields.y &&
                      info.value.seed == 777);
                CHECK(status != MC_WORLD_STORAGE_OK &&
                      state.phase == MC_WORLD_RUNTIME_FAILED_CONSTRUCTION &&
                      state.failed_call_pc == 415 && fields.v != NULL);
            } else {
                CHECK(!source.loaded_info_was_null && !fields.s && !fields.y &&
                      info.value.seed == 23);
                CHECK(info.value.version == (selected_case == OLD_ONLY ? 17 : 19132));
                if (selected_case == NETHER) {
                    mc_world_provider_state dimension;
                    CHECK(status != MC_WORLD_STORAGE_OK &&
                          state.phase == MC_WORLD_RUNTIME_FAILED_CONSTRUCTION && fields.t != NULL &&
                          mc_world_provider_state_get(fields.t, &dimension) ==
                              MC_WORLD_PROVIDER_OK &&
                          dimension.kind == MC_WORLD_PROVIDER_NETHER && fields.v == NULL);
                } else {
                    CHECK(status == MC_WORLD_STORAGE_OK && state.phase == MC_WORLD_RUNTIME_READY &&
                          state.failed_call_pc == 0 && state.last_store_pc == 406 &&
                          state.contexts_bound && state.cache_registered && fields.v != NULL);
                    CHECK(mc_world_source_regions_inspect(f->regions, &region_after) &&
                          region_after.entries == region_before.entries &&
                          region_after.activated == region_before.activated);
                    CHECK(source.loader != NULL);
                    mc_world_source_chunk_loader_state ld;
                    mc_world_source_state handler;
                    CHECK(mc_world_source_chunk_loader_inspect(source.loader, &ld) &&
                          mc_world_store_source_inspect(store, &handler) &&
                          ld.phase == MC_WORLD_SOURCE_READY && ld.store == store &&
                          ld.source_a == handler.source_b && ld.regions == f->regions &&
                          handler.holds == 4);
                    CHECK(mc_world_saved_data_count(fields.z) ==
                          (selected_case == PRIMARY ? 2u : 0u));
                    if (selected_case == PRIMARY) {
                        mc_world_saved_data_view data;
                        CHECK(mc_world_saved_data_inspect(fields.z, &data) == MC_WORLD_STORAGE_OK &&
                              data.id_count == 2 && data.named_count == 0 &&
                              data.ordered_count == 0);
                        const mc_world_saved_id *a = mc_world_saved_data_id_at(fields.z, 0),
                                                *b = mc_world_saved_data_id_at(fields.z, 1);
                        CHECK(a != NULL && b != NULL &&
                              ((a->value == 7 && b->value == -4) ||
                               (a->value == -4 && b->value == 7)));
                    }
                    mc_world_source_input_state input;
                    CHECK(mc_world_store_source_input_inspect(store, &input) && input.parsed &&
                          input.closed && input.output_transferred && input.source_returned &&
                          input.info == NULL && input.old_file == (selected_case == OLD_ONLY));
                    mc_world_info *sentinel = fields.x;
                    CHECK(mc_world_store_load_world_info(store, MC_WORLD_INFO_CLIENT, &sentinel,
                                                         &f->error) ==
                              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
                          sentinel == fields.x);
                    CHECK(mc_world_store_root(store) == NULL); /* Legacy transport guard stays. */
                }
            }
        }
    }
    /* One constructor invocation is terminal, including failed read prefixes.
     * Re-entry cannot replace the retained child, resample constructor clocks
     * or retry a source metadata/HC call to recover from its failure. */
    const mc_world_runtime_source_construction_state retained = source;
    mc_world_source_state before_retry, after_retry;
    CHECK(mc_world_store_source_inspect(store, &before_retry));
    CHECK(mc_world_runtime_construct_world(world, &args, &f->error) ==
          MC_WORLD_STORAGE_UNSUPPORTED_STATE);
    mc_world_runtime_construction unchanged;
    CHECK(mc_world_runtime_construction_get(world, &unchanged) == MC_WORLD_STORAGE_OK &&
          unchanged.phase == state.phase && unchanged.failed_call_pc == state.failed_call_pc &&
          unchanged.last_store_pc == state.last_store_pc);
    CHECK(mc_world_runtime_source_construction_inspect(world, &source) &&
          source.pending_saved_data == retained.pending_saved_data &&
          source.loader == retained.loader && source.metadata_called == retained.metadata_called &&
          source.metadata_returned == retained.metadata_returned);
    CHECK(mc_world_store_source_inspect(store, &after_retry) &&
          after_retry.holds == before_retry.holds && after_retry.source_e == before_retry.source_e);
    if (state.phase == MC_WORLD_RUNTIME_READY)
        CHECK(terminal_controls(f, world, &store) == 0);
    else
        CHECK(terminal_attempt(f, world, &store, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                               MC_WORLD_STORAGE_OK) == 0);
    if (selected_case == PRIMARY) {
        /* The copied region has a real zlib record with a non-Compound root.
         * Query acquisition must keep the exact retained ld/as failure, with
         * no second disk read or reconstruction from the smaller status. */
        mc_world_query_error query;
        int32_t value = -731;
        CHECK(mc_world_runtime_plant_light(world, 0, 64, 0, &value, &query) ==
              MC_WORLD_STORAGE_INVALID_DATA);
        const mc_world_source_chunk_error *const child =
            mc_world_source_chunk_loader_last_error(source.loader);
        CHECK(value == -731 && query.status == MC_WORLD_STORAGE_INVALID_DATA &&
              query.bytecode == 23 && query.provider_bytecode == 6 &&
              query.acquisition != MC_CHUNK_ACQUIRE_OK && query.source_chunk == child &&
              child != NULL && child->source.frame_count == 1 && child->source.frames[0].pc == 18 &&
              child->source.source_exception && child->input.source_fault != MC_NBT_SOURCE_NONE);
        mc_world_source_chunk_loader_state failed_loader;
        CHECK(mc_world_source_chunk_loader_inspect(source.loader, &failed_loader) &&
              failed_loader.phase == MC_WORLD_SOURCE_FAILED && failed_loader.last_pc == 18 &&
              failed_loader.reader != NULL && failed_loader.failure == child);
        /* A later actual no-acquisition result clears this caller frame only;
         * it cannot consume or overwrite the retained failed loader prefix. */
        CHECK(mc_world_runtime_saved_light(world, NULL, 32, 64, 32, &value, &query) ==
                  MC_WORLD_STORAGE_OK &&
              value == 0 && query.source_chunk == NULL &&
              mc_world_source_chunk_loader_last_error(source.loader) == child &&
              child->source.frames[0].pc == 18);
    }
    mc_world_source_chunk_loader *external = NULL;
    if (selected_case == PRIMARY) {
        external = (mc_world_source_chunk_loader *)source.loader;
        CHECK(mc_world_source_chunk_loader_retain(external, &f->source) == MC_WORLD_SOURCE_OK);
    }
    CHECK(mc_world_runtime_retire_world(world, &f->error) == MC_WORLD_STORAGE_OK);
    if (external != NULL) {
        CHECK(mc_world_runtime_free(world, &f->error) == MC_WORLD_STORAGE_BUSY);
        CHECK(mc_world_runtime_fields_get(world, &fields) == MC_WORLD_STORAGE_OK &&
              fields.z == NULL && fields.w == store && fields.x != NULL && fields.v != NULL);
        CHECK(mc_world_runtime_source_construction_inspect(world, &source) && source.retiring &&
              !source.saved_data_published && source.pending_saved_data == NULL &&
              source.loader == external && source.store == store && source.store_held);
        mc_world_info_fields info;
        CHECK(mc_world_info_fields_get(fields.x, &info) == MC_WORLD_INFO_OK &&
              info.name == f->display);
        CHECK(mc_world_source_chunk_loader_release(&external, &f->source) == MC_WORLD_SOURCE_OK &&
              external == NULL);
    }
    CHECK(mc_world_runtime_free(world, &f->error) == MC_WORLD_STORAGE_OK);
    world = NULL;
    mc_world_source_state handler;
    CHECK(mc_world_store_source_inspect(store, &handler) && handler.holds == 1);
    CHECK(mc_world_store_source_release(&store, &f->source) == MC_WORLD_SOURCE_OK && store == NULL);
    printf("SOURCE_WORLD case=%s ready=%d failed_call=%u cleanup=1\n", folder,
           state.phase == MC_WORLD_RUNTIME_READY, state.failed_call_pc);
    return 0;
}

int main(int argc, char **argv) {
    CHECK(argc == 2 && mc_math_trig_init(&trig));
    fixture *f = calloc(1, sizeof(*f));
    CHECK(f != NULL);
    CHECK(mc_java_files_create(NULL, &f->files, &f->source.file) == MC_JAVA_FILE_OK);
    CHECK(mc_java_logging_create(NULL, &f->logging, &f->source.logging) == MC_JAVA_LOGGING_OK);
    CHECK(mc_world_source_regions_create(f->files, NULL, &f->regions, &f->region) ==
          MC_WORLD_SOURCE_OK);
    CHECK(mc_world_store_source_classes_create(f->files, f->logging, NULL, &f->handlers,
                                               &f->source) == MC_WORLD_SOURCE_OK);
    CHECK(mc_world_store_source_classes_bind_regions(f->handlers, f->regions, &f->source) ==
          MC_WORLD_SOURCE_OK);
    mc_world_info_name *path = text(argv[1]);
    CHECK(path != NULL);
    CHECK(mc_java_file_allocate(f->files, &f->root, &f->source.file) == MC_JAVA_FILE_OK);
    CHECK(mc_java_file_construct_string(f->root, path, &f->source.file) == MC_JAVA_FILE_OK);
    mc_world_info_name_release(path);
    const uint16_t display[] = {'D', 0xe9, 'm', 'o', 0xd800};
    CHECK(mc_world_info_name_copy((mc_nbt_string){display, 5}, SIZE_MAX, &f->display) ==
          MC_WORLD_INFO_OK);
    CHECK(mc_java_random_profile_create(SIZE_MAX, &f->profile) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_world_runtime_classes_create(MC_WORLD_INFO_CLIENT, f->profile, &trig, &f->classes,
                                          &f->error) == MC_WORLD_STORAGE_OK);
    const char *const folders[] = {"primary", "old-only",    "bad-primary", "bad-old",
                                   "bad-ids", "limited-ids", "nether",      "missing"};
    for (size_t i = 0; i < sizeof(folders) / sizeof(folders[0]); ++i)
        CHECK(test_case(f, folders[i], (scenario)i) == 0);
    CHECK(mc_world_runtime_classes_free(f->classes, &f->error) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_store_source_classes_retire(&f->handlers, &f->source) == MC_WORLD_SOURCE_OK);
    CHECK(mc_world_source_regions_retire(&f->regions, &f->region) == MC_WORLD_SOURCE_OK);
    CHECK(mc_java_logging_retire(&f->logging, &f->source.logging) == MC_JAVA_LOGGING_OK);
    CHECK(mc_java_file_release(&f->root, &f->source.file) == MC_JAVA_FILE_OK);
    CHECK(mc_java_files_release(&f->files, &f->source.file) == MC_JAVA_FILE_OK);
    mc_world_info_name_release(f->display);
    mc_java_random_profile_free(f->profile);
    free(f);
    printf("Source World constructor passed (%u checks)\n", checks);
    return 0;
}
