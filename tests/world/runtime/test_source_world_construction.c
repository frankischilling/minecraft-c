/* Canonical client LOAD_OR_CREATE with real source File/HC/fm/ld owners. */
#include "../../../src/world/runtime/block_queries_internal.h"
#include "../../../src/world/runtime/runtime_internal.h"
#include "../../../src/world/runtime/source_construction_internal.h"
#include "../../../src/world/runtime/source_terminal_internal.h"
#include "../../../src/world/runtime/tile_entities/source_detailed_internal.h"
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
    /* Independent genuine output owners. They never become dependencies or
     * replacements of the queried source Worlds. */
    mc_materials *sentinel_materials;
    mc_java_random_profile *sentinel_profile;
    mc_registry_classes *sentinel_registry;
    mc_world_runtime_classes *sentinel_classes;
    mc_tile_entity *sentinel_tile;
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

typedef enum source_query_control { QUERY_PLANT, QUERY_MATERIAL, QUERY_TILE } source_query_control;

typedef struct saved_query_failure {
    mc_world_runtime_acquisition_error acquisition;
    unsigned char acquisition_bytes[sizeof(mc_world_runtime_acquisition_error)];
    mc_world_source_chunk_error producer;
    mc_world_source_chunk_loader_state loader;
} saved_query_failure;

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

static bool same_cause(const mc_world_storage_error *a, const mc_world_storage_error *b) {
    return a->status == b->status && a->operation == b->operation &&
           a->system_error == b->system_error;
}

static bool empty_registry(const mc_registry_error *e) {
    return e->status == MC_REGISTRY_OK && e->message == NULL && e->initializer_pc == -1 &&
           e->block_id == -1 && e->constructor_class == NULL && e->constructor_pc == -1 &&
           e->profile_status == MC_JAVA_RANDOM_PROFILE_OK && e->active_initializer_class == NULL &&
           e->active_initializer_pc == -1 && e->item_argument == -1 &&
           e->format_error.status == MC_JAVA_FORMAT_OK && e->format_error.offset == 0 &&
           e->format_error.length == 0 && e->format_error.value == 0 &&
           e->format_error.flags == 0 && e->format_error.conversion == 0 &&
           e->format_error.date_time == 0;
}

/* These independent genuine owners are only nonnull failure-output sentinels.
 * The queried source class keeps its absent Registry binding, so neither getter
 * can accidentally reach Registry/Tile construction and still satisfy the test. */
static int query_sentinels_create(fixture *f) {
    CHECK(mc_materials_create(&f->sentinel_materials));
    CHECK(mc_java_random_profile_create(SIZE_MAX, &f->sentinel_profile) ==
          MC_JAVA_RANDOM_PROFILE_OK);
    const mc_registry_options options = {.side = MC_REGISTRY_CLIENT,
                                         .random_profile = f->sentinel_profile,
                                         .materials = f->sentinel_materials};
    CHECK(mc_registry_classes_allocate(&options, &f->sentinel_registry, NULL) == MC_REGISTRY_OK);
    CHECK(mc_world_runtime_classes_create(MC_WORLD_INFO_CLIENT, f->sentinel_profile, &trig,
                                          &f->sentinel_classes, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_classes_bind_registry(f->sentinel_classes, f->sentinel_registry, NULL) ==
          MC_REGISTRY_OK);
    CHECK(mc_tile_entity_allocate_in_classes(f->sentinel_classes, MC_TILE_ENTITY_CHEST,
                                             &f->sentinel_tile) == MC_TILE_ENTITY_OK);
    return 0;
}

/* One actual getter, first on a fresh World's READY loader and empty cache.
 * The fixture's real compressed non-Compound NBT causes ld/as18; there is no
 * replacement loader, fabricated child or second query for comparison. */
static int source_detailed_failure(fixture *f, mc_world_runtime *world,
                                   const mc_world_source_chunk_loader *loader,
                                   source_query_control selected, saved_query_failure **out) {
    CHECK(selected == QUERY_MATERIAL || selected == QUERY_TILE);
    saved_query_failure *saved = calloc(1, sizeof(*saved));
    CHECK(saved != NULL);
    mc_world_runtime_acquisition_error *q = &saved->acquisition;
    mc_live_tile_entities_error_init(&q->tile_entity);
    q->registry = q->tile_entity.registry;
    mc_world_entity_load_error_init(&q->load_entity);
    mc_world_entity_batch_error_init(&q->batch);
    mc_world_source_chunk_loader_state before;
    CHECK(mc_world_source_chunk_loader_inspect(loader, &before) &&
          before.phase == MC_WORLD_SOURCE_READY && !before.active && !before.retiring &&
          before.holds == 1 && before.reader == NULL && before.document == NULL &&
          before.failure != NULL && before.failure->storage.status == MC_WORLD_STORAGE_OK &&
          world->active_calls == 0 && world->local_leases == 0 &&
          world->acquisition.cache->count == 0 && world->acquisition.pending->count == 0 &&
          world->acquisition_error == NULL && world->lifecycle_error == NULL &&
          mc_world_runtime_registry(world) == NULL);
    bool provider_null = true;
    if (selected == QUERY_MATERIAL) {
        mc_material *const sentinel = mc_materials_get(f->sentinel_materials, MC_MATERIAL_b);
        mc_material *material = sentinel;
        mc_world_material_error error;
        CHECK(sentinel != NULL && mc_world_runtime_material_get_source_detailed(
                                      world, 0, 64, 0, &material, &error, q, &provider_null) ==
                                      MC_WORLD_STORAGE_INVALID_DATA);
        CHECK(material == sentinel && error.status == MC_WORLD_STORAGE_INVALID_DATA &&
              error.bytecode == 4 && error.operation == q->cause.operation &&
              same_cause(&error.cause, &q->cause) && empty_registry(&error.registry));
    } else {
        mc_tile_entity *tile = f->sentinel_tile;
        mc_live_tile_entity_error *error = malloc(sizeof(*error));
        CHECK(error != NULL && tile != NULL);
        CHECK(mc_live_tile_entities_get_source_detailed(world, 0, 64, 0, &tile, error, q,
                                                        &provider_null) ==
              MC_WORLD_STORAGE_INVALID_DATA);
        CHECK(tile == f->sentinel_tile && error->status == MC_WORLD_STORAGE_INVALID_DATA &&
              error->bytecode == 7 && error->provider_bytecode == 0 && error->load_bytecode == 6 &&
              error->load_builder_bytecode == 0 && error->operation == q->cause.operation &&
              same_cause(&error->cause, &q->cause) && empty_registry(&error->registry));
        free(error);
    }
    const mc_world_source_chunk_error *child = mc_world_source_chunk_loader_last_error(loader);
    CHECK(!provider_null && q->status == MC_CHUNK_ACQUIRE_CALLBACK_FAILED &&
          q->cause.status == MC_WORLD_STORAGE_INVALID_DATA && q->source_chunk == child &&
          child != NULL && same_cause(&q->cause, &child->storage) &&
          child->source.frame_count == 1 && child->source.frames[0].pc == 18 &&
          child->source.source_exception && !child->source.native_policy &&
          child->input.source_fault != MC_NBT_SOURCE_NONE);
    CHECK(empty_registry(&q->registry) && q->tile_entity.status == MC_WORLD_STORAGE_OK &&
          empty_registry(&q->tile_entity.registry) && q->batch.index == SIZE_MAX &&
          q->batch.publication.status == MC_WORLD_STORAGE_OK && q->activation_bytecode == 0 &&
          q->activation_world_bytecode == 0 && q->load_bytecode == 6 &&
          q->load_builder_bytecode == 0 && q->load_entity.index == SIZE_MAX &&
          q->load_entity.native.status == MC_WORLD_STORAGE_OK && !q->population.reached &&
          world->active_calls == 0 && world->local_leases == 0 &&
          world->acquisition.cache->count == 0 && world->acquisition.pending->count == 0 &&
          world->acquisition_error == NULL && world->lifecycle_error == NULL);
    CHECK(mc_world_source_chunk_loader_inspect(loader, &saved->loader) &&
          saved->loader.phase == MC_WORLD_SOURCE_FAILED && !saved->loader.active &&
          saved->loader.holds == before.holds && saved->loader.last_pc == 18 &&
          saved->loader.reader != NULL && saved->loader.document == NULL &&
          saved->loader.record != NULL && saved->loader.failure == child);
    /* Byte snapshots of the complete ACTUAL records, not expected synthetic
     * diagnostics. Later owner-lifetime checks cannot overlook an overwritten
     * nested codec/Source field or caller acquisition member. */
    memcpy(saved->acquisition_bytes, q, sizeof(*q));
    memcpy(&saved->producer, child, sizeof(saved->producer));
    *out = saved;
    return 0;
}

static int source_detailed_retained(const saved_query_failure *saved,
                                    const mc_world_source_chunk_loader *loader, size_t holds) {
    mc_world_source_chunk_loader_state state;
    CHECK(mc_world_source_chunk_loader_inspect(loader, &state) && state.holds == holds &&
          state.failure == saved->loader.failure && state.reader == saved->loader.reader &&
          state.record == saved->loader.record && state.document == saved->loader.document &&
          saved->acquisition.source_chunk == state.failure &&
          mc_world_source_chunk_loader_last_error(loader) == state.failure);
    CHECK(memcmp(saved->acquisition_bytes, &saved->acquisition, sizeof(saved->acquisition)) == 0 &&
          memcmp(&saved->producer, state.failure, sizeof(saved->producer)) == 0);
    return 0;
}

static int test_case(fixture *f, const char *folder, scenario selected_case,
                     source_query_control selected_query) {
    saved_query_failure *saved_query = NULL;
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
    if (selected_case == PRIMARY && selected_query == QUERY_PLANT) {
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
    } else if (selected_case == PRIMARY) {
        CHECK(source_detailed_failure(f, world, source.loader, selected_query, &saved_query) == 0);
    }
    mc_world_source_chunk_loader *external = NULL;
    if (selected_case == PRIMARY) {
        external = (mc_world_source_chunk_loader *)source.loader;
        CHECK(mc_world_source_chunk_loader_retain(external, &f->source) == MC_WORLD_SOURCE_OK);
        if (saved_query != NULL)
            CHECK(source_detailed_retained(saved_query, external, 2) == 0);
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
        if (saved_query != NULL)
            CHECK(world->active_calls == 0 &&
                  source_detailed_retained(saved_query, external, 2) == 0);
        CHECK(mc_world_source_chunk_loader_release(&external, &f->source) == MC_WORLD_SOURCE_OK &&
              external == NULL);
        if (saved_query != NULL)
            CHECK(source_detailed_retained(saved_query, source.loader, 1) == 0);
    }
    CHECK(mc_world_runtime_free(world, &f->error) == MC_WORLD_STORAGE_OK);
    world = NULL;
    /* The loader's final release ends the typed descendant borrow. Do not
     * dereference its saved pointer after the actual World free. */
    free(saved_query);
    mc_world_source_state handler;
    CHECK(mc_world_store_source_inspect(store, &handler) && handler.holds == 1);
    CHECK(mc_world_store_source_release(&store, &f->source) == MC_WORLD_SOURCE_OK && store == NULL);
    printf("SOURCE_WORLD case=%s ready=%d failed_call=%u cleanup=1\n", folder,
           state.phase == MC_WORLD_RUNTIME_READY, state.failed_call_pc);
    if (selected_query != QUERY_PLANT)
        printf("SOURCE_QUERY kind=%s actual_saved_failure=1 retained_loader=1 cleanup=1\n",
               selected_query == QUERY_MATERIAL ? "material" : "tile");
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
        CHECK(test_case(f, folders[i], (scenario)i, QUERY_PLANT) == 0);
    CHECK(query_sentinels_create(f) == 0);
    CHECK(test_case(f, "primary", PRIMARY, QUERY_MATERIAL) == 0);
    CHECK(test_case(f, "primary", PRIMARY, QUERY_TILE) == 0);
    mc_tile_entity_snapshot sentinel;
    CHECK(mc_tile_entity_get_snapshot(f->sentinel_tile, &sentinel) == MC_TILE_ENTITY_OK &&
          sentinel.kind == MC_TILE_ENTITY_CHEST && sentinel.phase == MC_TILE_ENTITY_RAW &&
          sentinel.world == NULL && sentinel.chest_capacity == 0);
    mc_tile_entity_release(f->sentinel_tile);
    CHECK(mc_world_runtime_classes_free(f->sentinel_classes, &f->error) == MC_WORLD_STORAGE_OK);
    CHECK(mc_registry_classes_free(f->sentinel_registry, NULL) == MC_REGISTRY_OK);
    mc_java_random_profile_free(f->sentinel_profile);
    mc_materials_destroy(f->sentinel_materials);
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
