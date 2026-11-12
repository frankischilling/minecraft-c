/* Reuse the actual Registry resources/constructors and native teardown helpers.
 * The renamed program is never run; no legacy Store/region writer is called. */
int kl_unused_tile_main(int, char **);
#define main kl_unused_tile_main
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

/* Selected-arm comparison for the actually reached ld/as18 non-Compound-root
 * failure. Compare typed active source/input/storage fields, not padding or
 * unrelated inactive file/output/Registry diagnostics. */
static bool kl_site_same(const mc_world_source_site *a, const mc_world_source_site *b) {
    return a->class_name == b->class_name && a->method == b->method &&
           a->descriptor == b->descriptor && a->pc == b->pc;
}

static bool kl_input_same(const mc_nbt_stream_error *a, const mc_nbt_stream_error *b) {
    return a->parse.status == b->parse.status && a->parse.offset == b->parse.offset &&
           a->source_fault == b->source_fault && a->source_class == b->source_class &&
           a->source_method == b->source_method && a->source_pc == b->source_pc &&
           a->utf_error_byte == b->utf_error_byte && a->utf_prefix_units == b->utf_prefix_units &&
           a->utf_partial_character == b->utf_partial_character &&
           a->input.transport_status == b->input.transport_status &&
           a->input.system_error == b->input.system_error &&
           a->input.source_exception == b->input.source_exception &&
           a->input.operation == b->input.operation &&
           a->input.native_written_on_error == b->input.native_written_on_error &&
           strncmp(a->input.message, b->input.message, sizeof(a->input.message)) == 0;
}

static bool kl_error_same(const mc_world_source_chunk_error *a,
                          const mc_world_source_chunk_error *b) {
    if (a == NULL || b == NULL || a->source.frame_count != b->source.frame_count ||
        a->source.frame_count != 1 || a->source.frames[0].pc != 18 ||
        a->source.status != b->source.status || a->source.cause_status != b->source.cause_status ||
        a->source.operation != b->source.operation ||
        a->source.exception_class != b->source.exception_class ||
        a->source.native_policy != b->source.native_policy ||
        a->source.source_exception != b->source.source_exception ||
        a->source.caught_io != b->source.caught_io ||
        a->source.caught_exception != b->source.caught_exception ||
        a->source.diagnostic_frontier != b->source.diagnostic_frontier ||
        a->source.session_mismatch != b->source.session_mismatch ||
        !kl_site_same(&a->source.cause_site, &b->source.cause_site) ||
        !kl_site_same(&a->source.prior_site, &b->source.prior_site) ||
        !kl_site_same(&a->source.frames[0], &b->source.frames[0]) ||
        !kl_input_same(&a->source.input, &b->source.input) || !kl_input_same(&a->input, &b->input))
        return false;
    return a->storage.status == b->storage.status && a->storage.operation == b->storage.operation &&
           a->storage.system_error == b->storage.system_error &&
           a->compression.status == b->compression.status &&
           a->compression.operation == b->compression.operation &&
           a->compression.source_exception == b->compression.source_exception &&
           a->compression.source_class == b->compression.source_class &&
           a->compression.source_method == b->compression.source_method &&
           a->compression.source_pc == b->compression.source_pc;
}

/* Native finite component controls over the genuine SourceSavingWorld graph.
 * Its Source Store, loader/reader, import/Registry/Entity code and resources
 * stay alive until the neutral execution has checked-retired. The one actual
 * kernel get alone reserves and finishes its loader attempt. No manual loader
 * begin/finish, unrelated query, getter replay, callback or source-field repair.
 */
#include "world/runtime/runtime_execution_internal.h"

typedef struct kl_fixture {
    source_save_fixture source;
    mc_entity_tick_execution execution;
    mc_world_runtime_execution_result result;
    mc_entity_tick_execution_world_token world;
    mc_entity_tick_execution_loader_selection selected;
    mc_entity_tick_execution_loader_view view;
    mc_ws_file_bytes inputs[4];
    size_t bindings;
} kl_fixture;

static const mc_world_runtime_execution_code kl_code = {{"lm", "e", "()V", 6},
                                                        {"lm", "e", "()V", 33}};

static bool kl_world_same(const mc_entity_tick_execution_world_token *a,
                          const mc_entity_tick_execution_world_token *b) {
    return a->owner == b->owner && a->slot == b->slot && a->generation == b->generation;
}

static bool kl_pending_empty(const mc_world_runtime_execution_result *result) {
    if (result->root_pending)
        return false;
    for (size_t i = 0; i < MC_ENTITY_TICK_EXECUTION_LOADERS; ++i)
        if (result->pending[i].used)
            return false;
    return true;
}

static void kl_marker(const char *value) {
    printf("KERNEL_LOADER_CASE %s\n", value);
    fflush(stdout);
}

static int kl_files(kl_fixture *s, bool capture) {
    static const char *const names[] = {"world/level.dat", "world/data/idcounts.dat",
                                        "world/region/r.0.0.mcr", "world/preserve.bin"};
    for (size_t i = 0; i < COUNT(names); ++i) {
        char *path = NULL;
        mc_ws_file_bytes current = {0};
        mc_world_storage_error error = {0};
        const bool read_ok =
            mc_ws_path_join(s->source.root, names[i], &path, &error) == MC_WORLD_STORAGE_OK &&
            mc_ws_read_file(path, 2u * 1024u * 1024u, &current, &error) == MC_WORLD_STORAGE_OK;
        free(path);
        bool same = true;
        if (read_ok && capture) {
            s->inputs[i] = current;
            current = (mc_ws_file_bytes){0};
        } else if (read_ok) {
            same = current.length == s->inputs[i].length &&
                   (current.length == 0 ||
                    (current.data != NULL && s->inputs[i].data != NULL &&
                     memcmp(current.data, s->inputs[i].data, current.length) == 0));
        }
        if (!read_ok || !same)
            fprintf(stderr,
                    "kernel loader input %s capture=%d status=%u operation=%s error=%d "
                    "bytes=%zu expected=%zu same=%d\n",
                    names[i], capture ? 1 : 0, (unsigned int)error.status,
                    error.operation == NULL ? "(null)" : error.operation, error.system_error,
                    current.length, s->inputs[i].length, same ? 1 : 0);
        mc_ws_free_file_bytes(&current);
        VERIFY(read_ok && same);
    }
    return 0;
}

static int kl_actual_get(kl_fixture *s, bool null_error) {
    fixture *f = &s->source.game;
    mc_world_runtime *world = f->worlds[0];
    mc_world_runtime_source_construction_state construction;
    mc_world_source_chunk_loader_state loader = {0};
    mc_world_runtime_construction ready;
    VERIFY(mc_world_runtime_construction_get(world, &ready) == MC_WORLD_STORAGE_OK &&
           ready.mode == MC_WORLD_RUNTIME_GAME && ready.phase == MC_WORLD_RUNTIME_READY &&
           ready.entry == MC_WORLD_RUNTIME_CLIENT_LOAD_OR_CREATE &&
           mc_world_runtime_source_construction_inspect(world, &construction) &&
           construction.store == f->stores[0] && construction.store_held &&
           construction.saved_data_published && construction.loader != NULL &&
           mc_world_source_chunk_loader_inspect(construction.loader, &loader) &&
           loader.phase == MC_WORLD_SOURCE_READY && loader.last_pc == 6 && loader.reader == NULL &&
           loader.record == NULL && loader.document == NULL && !loader.active &&
           loader.holds == 1 && loader.store == f->stores[0] && f->providers[0].cache->count == 0 &&
           f->providers[0].cache->insertion_count == 0);
    mc_world_runtime_acquisition_error *caller = null_error ? NULL : calloc(1, sizeof(*caller));
    VERIFY(null_error || caller != NULL);
    s->bindings = world->persistent_bindings;
    mc_entity_tick_execution_init(&s->execution, NULL);
    VERIFY(mc_entity_tick_execution_push_world(
               &s->execution, world,
               (mc_entity_tick_execution_site){"native.kernel.loader", "get", "()V", 0},
               &s->world) == MC_WORLD_STORAGE_OK &&
           world->persistent_bindings == s->bindings + 1u);
    mc_chunk_state sentinel = {0};
    mc_chunk_state *out = &sentinel;
    kl_marker("entered");
    /* Exactly ONE reached GET. No test-authored loader reservation or finish. */
    const mc_chunk_acquire_status actual = mc_world_runtime_get_in_execution(
        &s->execution, world, kl_code, 0, 0, &out, caller, &s->result);
    VERIFY(actual == MC_CHUNK_ACQUIRE_CALLBACK_FAILED && out == &sentinel &&
           s->result.ownership_status == MC_WORLD_STORAGE_OK &&
           s->result.ownership_error.status == MC_WORLD_STORAGE_OK && s->result.selection_ready &&
           kl_pending_empty(&s->result) && s->result.selection.status == actual &&
           kl_world_same(&s->result.selection.world, &s->world) &&
           mc_entity_tick_execution_loader_read(&s->execution, &s->result.selection, &s->view) &&
           s->view.status == actual && s->view.failure != NULL);
    s->selected = s->result.selection;
    /* The result token selected the diagnostic BEFORE producer inspection.
     * This passive inspect validates its actual provenance, never selects an
     * owner from ambient last_error or supplies an uninvoked outcome. */
    VERIFY(
        mc_world_source_chunk_loader_inspect(construction.loader, &loader) &&
        loader.phase == MC_WORLD_SOURCE_FAILED && loader.last_pc == 18 && loader.reader != NULL &&
        loader.record != NULL && !loader.active && loader.failure != NULL &&
        s->view.failure != loader.failure && s->view.failure == &s->execution.loaders[0] &&
        kl_error_same(s->view.failure, loader.failure) &&
        s->view.failure->source.status == MC_WORLD_SOURCE_SOURCE_EXCEPTION &&
        s->view.failure->source.source_exception && !s->view.failure->source.native_policy &&
        s->view.failure->source.frame_count == 1 &&
        s->view.failure->source.frames[0].class_name != NULL &&
        s->view.failure->source.frames[0].method != NULL &&
        s->view.failure->source.frames[0].descriptor != NULL &&
        strcmp(s->view.failure->source.frames[0].class_name, "ld") == 0 &&
        strcmp(s->view.failure->source.frames[0].method, "a") == 0 &&
        strcmp(s->view.failure->source.frames[0].descriptor, "(Lfd;II)Llm;") == 0 &&
        s->view.failure->source.frames[0].pc == 18 &&
        s->view.failure->input.source_fault == MC_NBT_SOURCE_ROOT &&
        s->view.failure->storage.status == MC_WORLD_STORAGE_INVALID_DATA &&
        s->execution.loader_count == 1 && s->execution.world_count == 1 &&
        s->execution.lease_count == 1 && s->execution.receipt_count == 0 &&
        s->execution.phase == MC_ENTITY_TICK_EXECUTION_FAILED &&
        s->execution.first.status == MC_WORLD_STORAGE_OK &&
        s->execution.capture.status == MC_WORLD_STORAGE_OK &&
        s->execution.admission.status == MC_WORLD_STORAGE_OK &&
        s->execution.cleanup.status == MC_WORLD_STORAGE_OK && !s->execution.capture_pending &&
        !s->execution.world_slots[s->world.slot].loader_open && f->providers[0].cache->count == 0 &&
        f->providers[0].cache->insertion_count == 0 && f->providers[0].pending->count == 0);
    if (!null_error)
        VERIFY(caller->status == actual && caller->cause.status == MC_WORLD_STORAGE_INVALID_DATA &&
               caller->cause.status == loader.failure->storage.status &&
               caller->cause.operation == loader.failure->storage.operation &&
               caller->cause.system_error == loader.failure->storage.system_error &&
               caller->load_bytecode == 6 && caller->load_builder_bytecode == 0 &&
               caller->source_chunk == loader.failure &&
               kl_error_same(caller->source_chunk, s->view.failure));
    kl_marker("selected");
    /* This is caller diagnostic storage, never a source owner or field. */
    free(caller);
    caller = NULL;
    s->view = (mc_entity_tick_execution_loader_view){0};
    VERIFY(caller == NULL &&
           mc_entity_tick_execution_loader_read(&s->execution, &s->selected, &s->view) &&
           s->view.failure == &s->execution.loaders[0] &&
           kl_error_same(s->view.failure, loader.failure));
    kl_marker("caller-cleared");
    mc_entity_tick_execution_loader_selection stale = s->selected;
    ++stale.generation;
    const mc_entity_tick_execution_loader_view untouched = {MC_CHUNK_ACQUIRE_EMPTY, NULL};
    mc_entity_tick_execution_loader_view probe = untouched;
    VERIFY(!mc_entity_tick_execution_loader_read(&s->execution, &stale, &probe) &&
           probe.status == untouched.status && probe.failure == untouched.failure);
    stale = s->selected;
    ++stale.world.generation;
    VERIFY(!mc_entity_tick_execution_loader_read(&s->execution, &stale, &probe) &&
           probe.status == untouched.status && probe.failure == untouched.failure);
    VERIFY(mc_entity_tick_execution_pop_world(&s->execution, &s->world, true) ==
               MC_WORLD_STORAGE_OK &&
           !s->execution.world_open && s->execution.world_count == 1 &&
           s->execution.loader_count == 1 && s->execution.lease_count == 0 &&
           world->persistent_bindings == s->bindings + 1u &&
           mc_entity_tick_execution_loader_read(&s->execution, &s->selected, &s->view) &&
           s->view.failure == &s->execution.loaders[0] &&
           kl_error_same(s->view.failure, loader.failure));
    mc_world_source_chunk_loader_state retained = {0};
    VERIFY(mc_world_runtime_free(world, &s->source.error) == MC_WORLD_STORAGE_BUSY &&
           mc_world_source_chunk_loader_inspect(construction.loader, &retained) &&
           retained.failure == loader.failure && retained.reader == loader.reader &&
           retained.phase == MC_WORLD_SOURCE_FAILED && retained.last_pc == 18 &&
           mc_entity_tick_execution_loader_read(&s->execution, &s->selected, &s->view) &&
           kl_error_same(s->view.failure, retained.failure));
    kl_marker("popped");
    s->view = (mc_entity_tick_execution_loader_view){0};
    loader = retained = (mc_world_source_chunk_loader_state){0};
    construction = (mc_world_runtime_source_construction_state){0};
    VERIFY(mc_entity_tick_execution_retire(&s->execution) == MC_WORLD_STORAGE_OK &&
           s->execution.phase == MC_ENTITY_TICK_EXECUTION_RETIRED &&
           s->execution.loader_count == 0 && s->execution.world_count == 0 &&
           world->persistent_bindings == s->bindings);
    probe = untouched;
    VERIFY(!mc_entity_tick_execution_loader_read(&s->execution, &s->selected, &probe) &&
           probe.status == untouched.status && probe.failure == untouched.failure);
    kl_marker("retired");
    return 0;
}

static int kl_stop(source_save_fixture *s) {
    fixture *f = &s->game;
    VERIFY(f->states[0] == NULL && f->chunks[0] == NULL && f->providers[0].cache->count == 0 &&
           f->providers[0].cache->insertion_count == 0);
    VERIFY(mc_world_runtime_retire_entity_containers(f->worlds[0], &s->error) ==
           MC_WORLD_STORAGE_OK);
    f->terminal[0] = true;
    VERIFY(retire_descriptors(f, 0));
    VERIFY(mc_world_runtime_retire_world(f->worlds[0], &s->error) == MC_WORLD_STORAGE_OK &&
           mc_world_runtime_free(f->worlds[0], &s->error) == MC_WORLD_STORAGE_OK);
    f->worlds[0] = NULL;
    mc_world_provider_release(f->dimensions[0]);
    f->dimensions[0] = NULL;
    VERIFY(mc_world_store_source_release(&f->stores[0], &s->source) == MC_WORLD_SOURCE_OK &&
           f->stores[0] == NULL &&
           mc_world_source_regions_flush(s->source_regions, &s->region) == MC_WORLD_SOURCE_OK);
    f->bound[0] = f->terminal[0] = f->descriptors_retired[0] = false;
    return 0;
}

int main(int argc, char **argv) {
    VERIFY(argc == 3 && (strcmp(argv[1], "error") == 0 || strcmp(argv[1], "null-error") == 0));
    printf("KERNEL_LOADER_BEGIN %s\n", argv[1]);
    fflush(stdout);
    kl_fixture *s = calloc(1, sizeof(*s));
    VERIFY(s != NULL && ss_setup(&s->source, argv[2]) == 0 && kl_files(s, true) == 0 &&
           ss_open(&s->source) == 0);
    VERIFY(kl_actual_get(s, strcmp(argv[1], "null-error") == 0) == 0);
    /* Existing normal teardown flushes retained RW region handles BEFORE the
     * independent Windows byte-preservation reader opens the same input. */
    VERIFY(kl_stop(&s->source) == 0 && kl_files(s, false) == 0 && ss_finish(&s->source) == 0);
    for (size_t i = 0; i < COUNT(s->inputs); ++i)
        mc_ws_free_file_bytes(&s->inputs[i]);
    free(s);
    printf("PASS: %u actual kernel direct-loader checks;mode=%s\n", checks, argv[1]);
    return 0;
}
