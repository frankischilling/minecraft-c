/* Reuse the actual Registry resources/constructors and native teardown helpers.
 * The renamed program is never run; no legacy Store/region writer is called. */
int ks_unused_tile_main(int, char **);
#define main ks_unused_tile_main
#include "../tile_entities/test_tile_entities.c"
#undef main
#undef CHECK

#include "mc/world/runtime/saving/saving.h"
#include "mc/world/runtime/source_construction.h"
#include "mc/world/storage/source_handler.h"
#include "mc/world/storage/source_region_output.h"
#include "platform/time/millis.h"
#include "world/runtime/source_terminal_internal.h"

#ifdef _MSC_VER
#include <share.h>
#endif

#define VERIFY(x)                                                                                  \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(x)) {                                                                                \
            fprintf(stderr, "source saving line%d check%u: %s\n", __LINE__, checks, #x);           \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

typedef struct source_save_fixture {
    fixture game;
    mc_java_files *files;
    mc_java_logging *logging;
    mc_java_file *directory;
    mc_world_store_source_classes *handlers;
    mc_world_source_regions *source_regions;
    mc_world_source_error source;
    mc_world_source_region_error region;
    mc_world_storage_error error;
    mc_world_runtime_lifecycle_error lifecycle;
    mc_world_game_save_error save;
    const char *root;
    int64_t first, last;
} source_save_fixture;

static mc_world_info_name *ss_name(const char *value) {
    const size_t n = strlen(value);
    uint16_t *units = malloc((n + 1) * sizeof(*units));
    if (units == NULL)
        return NULL;
    for (size_t i = 0; i < n; ++i)
        units[i] = (unsigned char)value[i];
    mc_world_info_name *out = NULL;
    const mc_world_info_status result =
        mc_world_info_name_copy((mc_nbt_string){units, n}, SIZE_MAX, &out);
    free(units);
    return result == MC_WORLD_INFO_OK ? out : NULL;
}

static int ss_setup(source_save_fixture *s, const char *root) {
    fixture *f = &s->game;
    s->root = f->root = root;
    VERIFY(mc_math_trig_init(&trig) && mc_materials_create(&f->materials));
    VERIFY(mc_java_random_profile_create(4u * 1024u * 1024u, &f->profile) ==
           MC_JAVA_RANDOM_PROFILE_OK);
    f->name = ss_name("SourceSave");
    VERIFY(f->name != NULL);
    resources *r = &f->sources[0];
    r->streams[0] = (stream){.bytes = language, .length = sizeof(language) - 1};
    r->streams[1] = r->streams[2] = (stream){.bytes = language};
    r->api = (mc_registry_resources){r, open_resource, close_resource, ready_resource};
    const mc_registry_options registry = {MC_REGISTRY_CLIENT, f->profile, f->materials, 0, &r->api};
    VERIFY(mc_registry_classes_allocate(&registry, &f->registry[0], NULL) == MC_REGISTRY_OK &&
           mc_world_runtime_classes_create(MC_WORLD_INFO_CLIENT, f->profile, &trig, &f->classes[0],
                                           NULL) == MC_WORLD_STORAGE_OK &&
           mc_world_runtime_classes_bind_registry(f->classes[0], f->registry[0], NULL) ==
               MC_REGISTRY_OK);
    VERIFY(mc_entity_classes_create(MC_ENTITY_CLIENT, f->profile, NULL, &f->entity_classes[0]) ==
               MC_ENTITY_OK &&
           mc_world_runtime_classes_bind_entities(f->classes[0], f->entity_classes[0]) ==
               MC_ENTITY_OK &&
           mc_registry_classes_initialize_blocks(f->registry[0], NULL) == MC_REGISTRY_OK &&
           r->opens == 3 && r->closes == 1 && !r->bad);
    const mc_block *chest = NULL;
    VERIFY(mc_registry_block_static_get(f->registry[0], "av", &chest, NULL) == MC_REGISTRY_OK &&
           chest != NULL);
    f->chest_id[0] = mc_block_fields_get(chest)->bn;
    VERIFY(mc_java_files_create(NULL, &s->files, &s->source.file) == MC_JAVA_FILE_OK &&
           mc_java_logging_create(NULL, &s->logging, &s->source.logging) == MC_JAVA_LOGGING_OK &&
           mc_world_source_regions_create(s->files, NULL, &s->source_regions, &s->region) ==
               MC_WORLD_SOURCE_OK &&
           mc_world_store_source_classes_create(s->files, s->logging, NULL, &s->handlers,
                                                &s->source) == MC_WORLD_SOURCE_OK &&
           mc_world_store_source_classes_bind_regions(s->handlers, s->source_regions, &s->source) ==
               MC_WORLD_SOURCE_OK);
    mc_world_info_name *path = ss_name(root);
    VERIFY(path != NULL &&
           mc_java_file_allocate(s->files, &s->directory, &s->source.file) == MC_JAVA_FILE_OK &&
           mc_java_file_construct_string(s->directory, path, &s->source.file) == MC_JAVA_FILE_OK);
    mc_world_info_name_release(path);
    return 0;
}

static int ss_open(source_save_fixture *s) {
    fixture *f = &s->game;
    mc_world_info_name *folder = ss_name("world");
    VERIFY(folder != NULL &&
           mc_world_store_source_allocate(s->handlers, &f->stores[0], &s->source) ==
               MC_WORLD_SOURCE_OK &&
           mc_world_store_source_construct(f->stores[0], s->directory, folder, false, &s->source) ==
               MC_WORLD_SOURCE_OK);
    mc_world_info_name_release(folder);
    VERIFY(mc_world_store_is_source(f->stores[0]) && mc_world_store_root(f->stores[0]) == NULL);
    VERIFY(create_world(f, 0));
    mc_world_runtime_source_construction_state constructor;
    VERIFY(mc_world_runtime_source_construction_inspect(f->worlds[0], &constructor) &&
           constructor.metadata_called && constructor.metadata_returned &&
           constructor.loader != NULL);
    return 0;
}

static int ss_stop(source_save_fixture *s) {
    fixture *f = &s->game;
    VERIFY(mc_world_runtime_retire_entity_containers(f->worlds[0], &s->error) ==
           MC_WORLD_STORAGE_OK);
    f->terminal[0] = true;
    VERIFY(retire_descriptors(f, 0));
    VERIFY(mc_world_runtime_retire_world(f->worlds[0], &s->error) == MC_WORLD_STORAGE_OK);
    VERIFY(mc_world_store_retire_chunk(f->stores[0], f->states[0], &s->error) ==
           MC_WORLD_STORAGE_OK);
    f->states[0] = NULL;
    f->chunks[0] = NULL;
    VERIFY(mc_world_runtime_free(f->worlds[0], &s->error) == MC_WORLD_STORAGE_OK);
    f->worlds[0] = NULL;
    mc_world_provider_release(f->dimensions[0]);
    f->dimensions[0] = NULL;
    VERIFY(mc_world_store_source_release(&f->stores[0], &s->source) == MC_WORLD_SOURCE_OK &&
           f->stores[0] == NULL);
    VERIFY(mc_world_source_regions_flush(s->source_regions, &s->region) == MC_WORLD_SOURCE_OK);
    f->bound[0] = f->terminal[0] = f->descriptors_retired[0] = false;
    return 0;
}

static int ss_finish(source_save_fixture *s) {
    fixture *f = &s->game;
    VERIFY(mc_world_runtime_classes_free(f->classes[0], &s->error) == MC_WORLD_STORAGE_OK &&
           mc_registry_classes_free(f->registry[0], NULL) == MC_REGISTRY_OK &&
           mc_entity_classes_free(f->entity_classes[0]) == MC_ENTITY_OK);
    VERIFY(mc_world_store_source_classes_retire(&s->handlers, &s->source) == MC_WORLD_SOURCE_OK &&
           mc_world_source_regions_retire(&s->source_regions, &s->region) == MC_WORLD_SOURCE_OK &&
           mc_java_logging_retire(&s->logging, &s->source.logging) == MC_JAVA_LOGGING_OK &&
           mc_java_file_release(&s->directory, &s->source.file) == MC_JAVA_FILE_OK &&
           mc_java_files_release(&s->files, &s->source.file) == MC_JAVA_FILE_OK);
    mc_world_info_name_release(f->name);
    mc_java_random_profile_free(f->profile);
    mc_materials_destroy(f->materials);
    return 0;
}

/* Native kernel component controls. External caller owns the real source
 * Store/loader/import code and complete graph throughout unjoined legacy arms.
 * No foreign Chunk.d, manual loader reservation/finish, modeled Source frame,
 * getter replay, cancellation or Source field restoration is performed. */
#include "world/runtime/runtime_execution_internal.h"
#include "world/runtime/world_access_internal.h"

typedef struct ks_fixture {
    source_save_fixture source;
    mc_entity_tick_execution execution;
    mc_world_runtime_acquisition_error acquired;
    mc_world_runtime_execution_result result;
    mc_world_access *receiver;
    mc_world_access_receipt *receipt;
    mc_world_access_failure_lease *node;
    mc_entity_tick_execution_world_token callback_world;
    mc_world_storage_error callback_cause;
    mc_ws_file_bytes inputs[2];
    const char *mode;
    mc_world_storage_status callback_status, free_during;
    size_t callbacks, dirty_callbacks, saved_bindings;
    bool source_throwable, fault, projected, removed, callback_valid;
} ks_fixture;

static const mc_world_runtime_execution_code ks_code = {{"lm", "e", "()V", 6},
                                                        {"lm", "e", "()V", 33}};

static void ks_marker(const char *event) {
    printf("KERNEL_OWNER_CASE %s\n", event);
    fflush(stdout);
}

static void ks_restore(ks_fixture *s) {
    if (s->projected) {
        s->source.game.worlds[0]->persistent_bindings = s->saved_bindings;
        s->projected = false;
    }
}

static int ks_files(ks_fixture *s, bool capture) {
    static const char *const leaves[] = {"world/level.dat", "world/region/r.0.0.mcr"};
    for (size_t i = 0; i < COUNT(leaves); ++i) {
        char *path = NULL;
        mc_ws_file_bytes current = {0};
        mc_world_storage_error read_error = {0};
        const bool read_ok =
            mc_ws_path_join(s->source.root, leaves[i], &path, &read_error) == MC_WORLD_STORAGE_OK &&
            mc_ws_read_file(path, 1024u * 1024u, &current, &read_error) == MC_WORLD_STORAGE_OK;
        free(path);
        if (capture && read_ok) {
            s->inputs[i] = current;
            current = (mc_ws_file_bytes){0};
        }
        const bool same = capture || (current.length == s->inputs[i].length &&
                                      current.data != NULL && s->inputs[i].data != NULL &&
                                      memcmp(current.data, s->inputs[i].data, current.length) == 0);
        if (!read_ok || !same)
            fprintf(stderr,
                    "kernel file readback leaf=%s capture=%d status=%u operation=%s error=%d "
                    "bytes=%zu expected=%zu same=%d\n",
                    leaves[i], capture, (unsigned int)read_error.status,
                    read_error.operation == NULL ? "NULL" : read_error.operation,
                    read_error.system_error, current.length, s->inputs[i].length, same);
        mc_ws_free_file_bytes(&current);
        VERIFY(read_ok && same);
    }
    return 0;
}

static mc_world_storage_status ks_dirty(void *context, mc_world_runtime *world, int32_t x,
                                        int32_t y, int32_t z, mc_tile_entity *tile,
                                        mc_world_storage_error *cause,
                                        mc_world_access_failure *failure) {
    ks_fixture *s = context;
    (void)cause;
    (void)failure;
    ++s->dirty_callbacks;
    s->callback_valid &= world == s->source.game.worlds[0] && tile != NULL && x == 8 && y == 64 &&
                         z == 8 && s->callbacks == 0;
    return MC_WORLD_STORAGE_OK; /* Declared authored WA input, not a Tile body. */
}

static mc_world_storage_status ks_added(void *context, mc_world_runtime *world, mc_entity *entity,
                                        mc_world_storage_error *cause,
                                        mc_world_access_failure *failure) {
    ks_fixture *s = context;
    fixture *f = &s->source.game;
    ++s->callbacks;
    mc_world_runtime *selected = NULL;
    s->callback_valid &=
        world == f->worlds[0] && s->callbacks == 1 && entity != NULL &&
        mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_ITEM) && world->fields.b->count == 1 &&
        world->fields.b->items[0] == entity && world->entity_membership.count == 1 &&
        world->entity_membership.entries[0] == entity && world->fields.c->count == 1 &&
        mc_live_tile_entities_lists_valid(world) &&
        mc_entity_tick_execution_current_world(&s->execution, &s->callback_world) &&
        s->callback_world.slot == 1 &&
        mc_entity_tick_execution_read_world(&s->execution, &s->callback_world, &selected) &&
        selected == world;
    s->node = mc_entity_tick_execution_world_lease(&s->execution);
    bool removed = false;
    mc_world_storage_status status =
        mc_world_runtime_remove_world_access(world, s->receiver, &removed, cause);
    s->removed = status == MC_WORLD_STORAGE_OK && removed;
    if (!s->removed)
        return status == MC_WORLD_STORAGE_OK ? MC_WORLD_STORAGE_UNSUPPORTED_STATE : status;
    s->free_during = mc_world_access_free(s->receiver, NULL);
    status = mc_world_access_receipt_create(s->receiver, NULL, &s->receipt, failure, cause);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    s->receipt->exception_class = s->source_throwable ? "java/lang/IllegalStateException" : NULL;
    *cause =
        (mc_world_storage_error){s->callback_status, "Kernel authored Entity callback failure", 71};
    s->callback_cause = *cause;
    if (s->fault) {
        s->saved_bindings = world->persistent_bindings;
        world->persistent_bindings = 0; /* Downward native fault only. */
        s->projected = true;
    }
    return s->callback_status;
}

static bool ks_pending(const mc_world_runtime_execution_result *result) {
    bool pending = result->root_pending;
    for (size_t i = 0; i < MC_ENTITY_TICK_EXECUTION_LOADERS; ++i)
        pending |= result->pending[i].used;
    return pending;
}

static int ks_remember(ks_fixture *s) {
    fixture *f = &s->source.game;
    VERIFY(f->providers[0].cache->count == 1 && f->providers[0].cache->insertion_count == 1);
    f->states[0] = f->providers[0].cache->insertions[0];
    f->chunks[0] = mc_world_store_chunk_object(f->stores[0], f->states[0]);
    mc_world_chunk_fields fields;
    VERIFY(f->states[0] != NULL && f->states[0]->active && f->chunks[0] != NULL &&
           mc_world_store_owns_chunk(f->stores[0], f->states[0]) &&
           mc_world_chunk_fields_get(f->chunks[0], &fields) == MC_WORLD_STORAGE_OK &&
           fields.d == f->worlds[0]);
    return 0;
}

static int ks_positive(ks_fixture *s) {
    fixture *f = &s->source.game;
    mc_world_runtime *world = f->worlds[0];
    mc_world_runtime_source_construction_state constructor;
    mc_world_source_chunk_loader_state loaded = {0};
    VERIFY(mc_world_runtime_source_construction_inspect(world, &constructor) &&
           constructor.loader != NULL);
    for (int pass = 0; pass < 2; ++pass) {
        ks_marker(pass == 0 ? "cold BEGIN" : "cache BEGIN");
        const size_t bindings = world->persistent_bindings;
        mc_entity_tick_execution_world_token outer;
        VERIFY(mc_entity_tick_execution_push_world(
                   &s->execution, world,
                   (mc_entity_tick_execution_site){"native.kernel.control", "get", "()V", pass},
                   &outer) == MC_WORLD_STORAGE_OK);
        mc_chunk_state *actual = NULL;
        const mc_chunk_acquire_status status =
            pass == 0 ? mc_world_runtime_get_in_execution(&s->execution, world, ks_code, 0, 0,
                                                          &actual, &s->acquired, &s->result)
                      : mc_world_runtime_get_failure_in_execution(&s->execution, world, ks_code, 0,
                                                                  0, &actual, NULL, &s->result);
        VERIFY(status == MC_CHUNK_ACQUIRE_OK && actual != NULL && actual->active &&
               s->result.ownership_status == MC_WORLD_STORAGE_OK && s->result.selection_ready &&
               !ks_pending(&s->result) && s->execution.loader_count == 0 &&
               s->execution.receipt_count == 0);
        if (pass == 0) {
            VERIFY(ks_remember(s) == 0 && actual == f->states[0] &&
                   mc_world_source_chunk_loader_inspect(constructor.loader, &loaded) &&
                   loaded.phase == MC_WORLD_SOURCE_READY && loaded.reader != NULL &&
                   loaded.document != NULL);
        } else {
            mc_world_source_chunk_loader_state after;
            VERIFY(actual == f->states[0] &&
                   mc_world_source_chunk_loader_inspect(constructor.loader, &after) &&
                   after.reader == loaded.reader && after.document == loaded.document &&
                   after.record == loaded.record && after.phase == loaded.phase &&
                   after.last_pc == loaded.last_pc);
        }
        mc_entity_tick_execution_loader_view view;
        VERIFY(mc_entity_tick_execution_loader_read(&s->execution, &s->result.selection, &view) &&
               view.status == status && view.failure == NULL &&
               f->providers[0].cache->insertion_count == 1 &&
               mc_entity_tick_execution_pop_world(&s->execution, &outer, false) ==
                   MC_WORLD_STORAGE_OK &&
               world->persistent_bindings == bindings && s->execution.world_count == 0 &&
               s->execution.lease_count == 0);
        s->result = (mc_world_runtime_execution_result){
            0}; /* Expire ordinary selection before owner retirement. */
        ks_marker(pass == 0 ? "cold END" : "cache END");
    }
    return 0;
}

static int ks_failure(ks_fixture *s) {
    fixture *f = &s->source.game;
    mc_world_runtime *world = f->worlds[0];
    ks_marker("failure BEGIN");
    s->callback_valid = true;
    const mc_world_access_source_callbacks callbacks = {.entity_added = ks_added,
                                                        .tile_entity_dirty = ks_dirty};
    VERIFY(mc_world_access_source_create(&callbacks, s, &s->receiver, NULL) ==
               MC_WORLD_STORAGE_OK &&
           mc_world_runtime_add_world_access(world, s->receiver, NULL) == MC_WORLD_STORAGE_OK);
    mc_entity_tick_execution_world_token outer;
    VERIFY(mc_entity_tick_execution_push_world(
               &s->execution, world,
               (mc_entity_tick_execution_site){"native.kernel.control", "get", "()V", 0},
               &outer) == MC_WORLD_STORAGE_OK);
    mc_chunk_state *actual = NULL;
    const mc_chunk_acquire_status status = mc_world_runtime_get_in_execution(
        &s->execution, world, ks_code, 0, 0, &actual, &s->acquired, &s->result);
    ks_restore(s); /* Restore the exact actual native counter BEFORE any assertion or unwind. */
    mc_world_runtime_source_construction_state construction;
    mc_world_source_chunk_loader_state loaded;
    VERIFY(mc_world_runtime_source_construction_inspect(world, &construction) &&
           mc_world_source_chunk_loader_inspect(construction.loader, &loaded) &&
           loaded.phase == MC_WORLD_SOURCE_READY && loaded.reader != NULL &&
           loaded.document != NULL);
    VERIFY(ks_remember(s) == 0 && status == MC_CHUNK_ACQUIRE_CALLBACK_FAILED && actual == NULL &&
           s->callbacks == 1 && s->callback_valid && s->removed &&
           s->free_during == MC_WORLD_STORAGE_BUSY && s->receipt != NULL &&
           s->receiver->active_holds == 1 && s->node == &s->execution.leases[1] &&
           s->acquired.status == status && s->acquired.activation_bytecode == 42 &&
           s->acquired.activation_world_bytecode == 34 && s->acquired.batch.index == 0 &&
           s->acquired.batch.publication.world_access_bytecode == 29 &&
           s->acquired.batch.publication.world_access_failure.receipt == s->receipt &&
           s->acquired.source_chunk == NULL &&
           s->acquired.cause.status == s->callback_cause.status &&
           s->acquired.cause.operation == s->callback_cause.operation &&
           s->acquired.cause.system_error == s->callback_cause.system_error);
    const mc_world_access_failure *view = NULL;
    const char *exception = NULL;
    VERIFY(mc_world_access_failure_lease_inspect(s->node, &view) && view->receiver == s->receiver &&
           view->receipt == s->receipt &&
           mc_world_access_failure_source_exception(view, &exception) == s->source_throwable);
    VERIFY(!s->source_throwable ||
           (exception != NULL && strcmp(exception, "java/lang/IllegalStateException") == 0));
    if (s->fault) {
        VERIFY(s->result.ownership_status == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
               s->result.ownership_error.site.pc == 33 &&
               s->result.ownership_error.operation != NULL &&
               strcmp(s->result.ownership_error.operation, "World Entity owned receipt capture") ==
                   0 &&
               s->execution.current_lease == 1 && s->execution.world_open &&
               s->execution.receipt_count == 0 && !s->execution.capture_pending &&
               s->execution.first.status == MC_WORLD_STORAGE_OK &&
               s->execution.capture.status == MC_WORLD_STORAGE_OK && ks_pending(&s->result) &&
               !s->result.selection_ready && s->result.root_pending &&
               s->result.root_attempt.slot < MC_ENTITY_TICK_EXECUTION_LOADERS &&
               s->result.pending[s->result.root_attempt.slot].used &&
               s->result.pending[s->result.root_attempt.slot].outcome_valid &&
               s->result.pending[s->result.root_attempt.slot].actual_status == status);
        const mc_entity_tick_execution_error first = s->result.ownership_error;
        VERIFY(mc_entity_tick_execution_pop_world(&s->execution, &s->callback_world, true) ==
                   MC_WORLD_STORAGE_OK &&
               s->execution.current_lease == outer.slot &&
               mc_world_runtime_execution_finish_pending(&s->execution, &s->result) ==
                   MC_WORLD_STORAGE_OK &&
               !ks_pending(&s->result) && s->result.selection_ready &&
               s->result.ownership_error.status == first.status &&
               s->result.ownership_error.operation == first.operation &&
               s->result.ownership_error.site.pc == first.site.pc);
    } else {
        VERIFY(s->result.ownership_status == MC_WORLD_STORAGE_OK && s->result.selection_ready &&
               !ks_pending(&s->result) && s->execution.current_lease == outer.slot &&
               s->execution.receipt_count == 1);
    }
    mc_entity_tick_execution_loader_view selected;
    VERIFY(mc_entity_tick_execution_loader_read(&s->execution, &s->result.selection, &selected) &&
           selected.status == status && selected.failure == NULL &&
           mc_entity_tick_execution_pop_world(&s->execution, &outer, true) == MC_WORLD_STORAGE_OK &&
           s->execution.current_lease == SIZE_MAX && s->execution.lease_count == 1 &&
           mc_world_access_failure_lease_inspect(s->node, &view) &&
           mc_world_access_receipt_retire(&s->receipt, NULL) == MC_WORLD_STORAGE_BUSY &&
           mc_world_access_free(s->receiver, NULL) == MC_WORLD_STORAGE_BUSY);
    const mc_world_storage_error source_cause = s->acquired.cause;
    s->acquired.batch.publication.world_access_failure = (mc_world_access_failure){0};
    s->acquired.source_chunk = NULL;
    s->result = (mc_world_runtime_execution_result){0};
    selected = (mc_entity_tick_execution_loader_view){0};
    view = NULL;
    VERIFY(mc_entity_tick_execution_retire(&s->execution) == MC_WORLD_STORAGE_OK &&
           mc_world_access_receipt_retire(&s->receipt, NULL) == MC_WORLD_STORAGE_OK &&
           mc_world_access_free(s->receiver, NULL) == MC_WORLD_STORAGE_OK && s->callbacks == 1 &&
           s->acquired.cause.status == source_cause.status &&
           s->acquired.cause.operation == source_cause.operation &&
           s->acquired.cause.system_error == source_cause.system_error);
    s->receiver = NULL;
    s->node = NULL;
    ks_marker("failure END");
    return 0;
}

int main(int argc, char **argv) {
    VERIFY(argc == 3 && (strcmp(argv[1], "empty") == 0 || strcmp(argv[1], "source") == 0 ||
                         strcmp(argv[1], "native") == 0 || strcmp(argv[1], "first-owner") == 0));
    printf("KERNEL_OWNER_BEGIN schema=1 mode=%s\n", argv[1]);
    fflush(stdout);
    ks_fixture *s = calloc(1, sizeof(*s));
    VERIFY(s != NULL);
    s->mode = argv[1];
    s->fault = strcmp(s->mode, "first-owner") == 0;
    s->source_throwable = strcmp(s->mode, "native") != 0;
    s->callback_status = s->source_throwable ? MC_WORLD_STORAGE_IO_ERROR : MC_WORLD_STORAGE_LIMIT;
    VERIFY(ss_setup(&s->source, argv[2]) == 0 && ks_files(s, true) == 0 &&
           ss_open(&s->source) == 0);
    VERIFY(s->source.game.providers[0].cache->count == 0 &&
           s->source.game.providers[0].cache->insertion_count == 0);
    mc_entity_tick_execution_init(&s->execution, NULL);
    VERIFY((strcmp(s->mode, "empty") == 0 ? ks_positive(s) : ks_failure(s)) == 0);
    VERIFY(mc_entity_tick_execution_retire(&s->execution) == MC_WORLD_STORAGE_OK);
    /* Normal source teardown closes its cached read/write region handle before
     * the independent snapshot reader opens that file on Windows. */
    VERIFY(ss_stop(&s->source) == 0 && ks_files(s, false) == 0 && ss_finish(&s->source) == 0);
    for (size_t i = 0; i < COUNT(s->inputs); ++i)
        mc_ws_free_file_bytes(&s->inputs[i]);
    free(s);
    printf("PASS: %u actual kernel execution checks;mode=%s\n", checks, argv[1]);
    return 0;
}
