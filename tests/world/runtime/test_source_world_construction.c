/* Canonical client LOAD_OR_CREATE with real source File/HC/fm/ld owners. */
#include "../../../src/world/runtime/runtime_internal.h"
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
                          handler.holds >= 4);
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
