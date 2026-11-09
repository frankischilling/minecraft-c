/* Reuse the actual Registry resources/constructors and native teardown helpers.
 * The renamed program is never run; no legacy Store/region writer is called. */
int cq_unused_tile_main(int, char **);
#define main cq_unused_tile_main
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

/* Genuine SourceSaving graph; all defining code/Registry/Store/import/loader
 * owners remain externally live and exclusive through cleanup. Native component
 * inputs only: no modeled child return, Source pointer fault or manual attempt. */
#include "world/runtime/collision/blocks_execution_internal.h"
#include "world/runtime/tile_entities/execution_internal.h"
#include "world/runtime/world_access_internal.h"

typedef struct cq_fixture {
    source_save_fixture source;
    mc_entity_tick_execution execution;
    mc_world_runtime_execution_result result, probe_result;
    mc_world_collision_error collision, probe;
    mc_live_tile_entity_error dirty;
    mc_entity_tick_execution_loader_selection selected;
    mc_entity_tick_execution_loader_view view;
    mc_world_access *receiver;
    mc_world_access_receipt *receipt;
    mc_world_access_failure_lease *node;
    mc_world_storage_error callback_cause;
    mc_world_source_chunk_loader_state loaded;
    mc_ws_file_bytes inputs[4];
    const char *mode;
    size_t callbacks, probes;
    mc_world_storage_status free_during;
    bool callback_valid, probe_valid, removed;
} cq_fixture;

/* Actual fixed client code provenance from the native defining caller's held
 * namespaces, not nullable World/Chunk.d or a producer's ambient error. */
static const mc_world_runtime_execution_code cq_code = {{"lm", "e", "()V", 6},
                                                        {"lm", "e", "()V", 33}};
static const mc_entity_tick_execution_site cq_site = {"native.collision.control", "read", "()V", 0};

static void cq_marker(const char *event) {
    printf("COLLISION_QUERY_CASE %s\n", event);
    fflush(stdout);
}

static bool cq_pending(const mc_world_runtime_execution_result *result) {
    bool pending = result->root_pending;
    for (size_t i = 0; i < MC_ENTITY_TICK_EXECUTION_LOADERS; ++i)
        pending |= result->pending[i].used;
    return pending;
}

static mc_world_storage_status cq_get(cq_fixture *s, int32_t x, int32_t y, int32_t z, bool metadata,
                                      int32_t *out, mc_world_collision_error *error,
                                      mc_world_runtime_execution_result *result) {
    return mc_world_collision_block_value_in_execution(
        &s->execution, s->source.game.worlds[0], cq_code, x, y, z, metadata, out, error, result);
}

static int cq_files(cq_fixture *s, bool capture) {
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

static int cq_remember(cq_fixture *s) {
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

static bool cq_token_same(mc_entity_tick_execution_world_token a,
                          mc_entity_tick_execution_world_token b) {
    return a.owner == b.owner && a.slot == b.slot && a.generation == b.generation;
}

static bool cq_owner_error_same(mc_entity_tick_execution_error a,
                                mc_entity_tick_execution_error b) {
    return a.status == b.status && a.operation == b.operation &&
           a.site.class_name == b.site.class_name && a.site.method == b.site.method &&
           a.site.descriptor == b.site.descriptor && a.site.pc == b.site.pc;
}

typedef struct cq_scope {
    mc_entity_tick_execution_world_token current;
    mc_entity_tick_execution_phase phase;
    mc_entity_tick_execution_error first, capture, cleanup;
    mc_world_access_failure_lease *node;
    mc_world_access_failure_lease_phase node_phase;
    mc_world_access *node_receiver;
    mc_world_access_receipt *node_receipt;
    size_t bindings, active_calls, world_count, lease_count, receipt_count, loader_count;
    size_t cache_count, insertion_count, receiver_holds;
    bool open, held, child_failed, capture_pending;
    mc_world_source_chunk_loader_state loader;
} cq_scope;

static bool cq_scope_take(cq_fixture *s, cq_scope *out) {
    fixture *f = &s->source.game;
    mc_world_runtime *world = f->worlds[0];
    mc_world_runtime_source_construction_state construction;
    if (!mc_entity_tick_execution_current_world(&s->execution, &out->current) ||
        !mc_world_runtime_source_construction_inspect(world, &construction) ||
        !mc_world_source_chunk_loader_inspect(construction.loader, &out->loader))
        return false;
    out->phase = s->execution.phase;
    out->first = s->execution.first;
    out->capture = s->execution.capture;
    out->cleanup = s->execution.cleanup;
    out->node = mc_entity_tick_execution_world_lease(&s->execution);
    if (out->node == NULL)
        return false;
    out->node_phase = out->node->phase;
    out->node_receiver = out->node->receiver;
    out->node_receipt = out->node->receipt;
    out->bindings = world->persistent_bindings;
    out->active_calls = world->active_calls;
    out->world_count = s->execution.world_count;
    out->lease_count = s->execution.lease_count;
    out->receipt_count = s->execution.receipt_count;
    out->loader_count = s->execution.loader_count;
    out->cache_count = f->providers[0].cache->count;
    out->insertion_count = f->providers[0].cache->insertion_count;
    out->receiver_holds = s->receiver->active_holds;
    out->open = s->execution.world_open;
    out->held = s->execution.world_held;
    out->child_failed = s->execution.child_failed;
    out->capture_pending = s->execution.capture_pending;
    return true;
}

static bool cq_scope_same(cq_fixture *s, const cq_scope *before) {
    cq_scope after = {0};
    return cq_scope_take(s, &after) && cq_token_same(after.current, before->current) &&
           after.phase == before->phase && cq_owner_error_same(after.first, before->first) &&
           cq_owner_error_same(after.capture, before->capture) &&
           cq_owner_error_same(after.cleanup, before->cleanup) && after.node == before->node &&
           after.node_phase == before->node_phase && after.node_receiver == before->node_receiver &&
           after.node_receipt == before->node_receipt && after.bindings == before->bindings &&
           after.active_calls == before->active_calls && after.world_count == before->world_count &&
           after.lease_count == before->lease_count &&
           after.receipt_count == before->receipt_count &&
           after.loader_count == before->loader_count && after.cache_count == before->cache_count &&
           after.insertion_count == before->insertion_count &&
           after.receiver_holds == before->receiver_holds && after.open == before->open &&
           after.held == before->held && after.child_failed == before->child_failed &&
           after.capture_pending == before->capture_pending &&
           after.loader.phase == before->loader.phase &&
           after.loader.reader == before->loader.reader &&
           after.loader.document == before->loader.document &&
           after.loader.record == before->loader.record &&
           after.loader.last_pc == before->loader.last_pc;
}

static bool kl_world_same(const mc_entity_tick_execution_world_token *a,
                          const mc_entity_tick_execution_world_token *b) {
    return a->owner == b->owner && a->slot == b->slot && a->generation == b->generation;
}

static int cq_empty_stop(source_save_fixture *s) {
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

static bool cq_loader_same(cq_fixture *s, const mc_world_source_chunk_loader_state *before) {
    mc_world_runtime_source_construction_state construction;
    mc_world_source_chunk_loader_state after;
    return mc_world_runtime_source_construction_inspect(s->source.game.worlds[0], &construction) &&
           mc_world_source_chunk_loader_inspect(construction.loader, &after) &&
           after.phase == before->phase && after.reader == before->reader &&
           after.document == before->document && after.record == before->record &&
           after.last_pc == before->last_pc;
}

static int cq_positive(cq_fixture *s, size_t pass, bool metadata_first) {
    static const struct {
        bool metadata;
        int32_t y, expected;
    } id_first[] = {{false, 64, 1}, {true, 64, 5}, {false, 65, 3}, {true, 65, 10}};

    static const size_t metadata_order[] = {3, 0, 1, 2};
    const size_t selected = metadata_first ? metadata_order[pass] : pass;
    fixture *f = &s->source.game;
    mc_world_runtime *world = f->worlds[0];
    mc_entity_tick_execution_world_token outer;
    const size_t calls = world->active_calls, bindings = world->persistent_bindings;
    VERIFY(mc_entity_tick_execution_push_world(&s->execution, world, cq_site, &outer) ==
           MC_WORLD_STORAGE_OK);
    int32_t actual = INT32_C(0x13579bdf);
    mc_world_collision_error_init(&s->collision);
    const mc_world_storage_status status =
        cq_get(s, 8, id_first[selected].y, 8, id_first[selected].metadata, &actual,
               (pass & 1u) ? NULL : &s->collision, &s->result);
    VERIFY(status == MC_WORLD_STORAGE_OK && actual == id_first[selected].expected &&
           s->result.ownership_status == MC_WORLD_STORAGE_OK && !s->result.selection_ready &&
           !cq_pending(&s->result) && s->execution.loader_count == 0 &&
           s->execution.receipt_count == 0 && world->active_calls == calls);
    if (pass == 0) {
        VERIFY(cq_remember(s) == 0 && world->fields.b->count == 0 && world->fields.c->count == 0);
        mc_world_runtime_source_construction_state construction;
        VERIFY(mc_world_runtime_source_construction_inspect(world, &construction) &&
               mc_world_source_chunk_loader_inspect(construction.loader, &s->loaded) &&
               s->loaded.phase == MC_WORLD_SOURCE_READY && s->loaded.reader != NULL &&
               s->loaded.document != NULL);
    } else
        VERIFY(world->persistent_bindings == bindings + 1 && cq_loader_same(s, &s->loaded));
    const size_t before_pop = world->persistent_bindings;
    VERIFY(mc_entity_tick_execution_pop_world(&s->execution, &outer, false) ==
               MC_WORLD_STORAGE_OK &&
           world->persistent_bindings + 1 == before_pop && s->execution.world_count == 0 &&
           s->execution.lease_count == 0 && f->providers[0].cache->count == 1 &&
           f->providers[0].cache->insertion_count == 1);
    s->result = (mc_world_runtime_execution_result){0};
    return 0;
}

static int cq_domain(cq_fixture *s) {
    static const struct {
        int32_t x, y, z;
        bool metadata;
    } cases[] = {{-32000001, 64, 8, false}, {32000000, 64, 8, true}, {8, 64, -32000001, false},
                 {8, 64, 32000001, true},   {8, -1, 8, false},       {8, 128, 8, true}};

    fixture *f = &s->source.game;
    mc_world_runtime *world = f->worlds[0];
    mc_world_runtime_source_construction_state construction;
    VERIFY(mc_world_runtime_source_construction_inspect(world, &construction) &&
           mc_world_source_chunk_loader_inspect(construction.loader, &s->loaded) &&
           s->loaded.last_pc == 6 && s->loaded.reader == NULL && s->loaded.document == NULL);
    for (size_t i = 0; i < COUNT(cases); ++i) {
        const size_t bindings = world->persistent_bindings, calls = world->active_calls;
        mc_entity_tick_execution_world_token outer;
        VERIFY(mc_entity_tick_execution_push_world(&s->execution, world, cq_site, &outer) ==
               MC_WORLD_STORAGE_OK);
        int32_t actual = INT32_C(0x13579bdf);
        mc_world_collision_error_init(&s->collision);
        VERIFY(cq_get(s, cases[i].x, cases[i].y, cases[i].z, cases[i].metadata, &actual,
                      (i & 1u) ? NULL : &s->collision, &s->result) == MC_WORLD_STORAGE_OK &&
               actual == 0 && s->result.ownership_status == MC_WORLD_STORAGE_OK &&
               !s->result.selection_ready && !cq_pending(&s->result) &&
               s->execution.loader_count == 0 && f->providers[0].cache->count == 0 &&
               f->providers[0].cache->insertion_count == 0 && cq_loader_same(s, &s->loaded) &&
               world->active_calls == calls &&
               mc_entity_tick_execution_pop_world(&s->execution, &outer, false) ==
                   MC_WORLD_STORAGE_OK &&
               world->persistent_bindings == bindings && s->execution.world_count == 0 &&
               s->execution.lease_count == 0);
        s->result = (mc_world_runtime_execution_result){0};
    }
    return 0;
}

static int cq_loader_failure(cq_fixture *s, bool null_error) {
    fixture *f = &s->source.game;
    mc_world_runtime *world = f->worlds[0];
    mc_world_runtime_source_construction_state construction;
    VERIFY(mc_world_runtime_source_construction_inspect(world, &construction) &&
           construction.loader != NULL && f->providers[0].cache->count == 0);
    const size_t bindings = world->persistent_bindings, calls = world->active_calls;
    mc_entity_tick_execution_world_token outer;
    VERIFY(mc_entity_tick_execution_push_world(&s->execution, world, cq_site, &outer) ==
           MC_WORLD_STORAGE_OK);
    mc_world_collision_error *caller = null_error ? NULL : calloc(1, sizeof(*caller));
    VERIFY(null_error || caller != NULL);
    mc_world_collision_error_init(caller);
    int32_t actual = INT32_C(0x13579bdf);
    cq_marker("loader entered");
    /* Exactly one actual scalar query; its reached kernel get reserves/finishes. */
    const mc_world_storage_status status = cq_get(s, 8, 64, 8, false, &actual, caller, &s->result);
    VERIFY(status == MC_WORLD_STORAGE_INVALID_DATA && actual == INT32_C(0x13579bdf) &&
           s->result.ownership_status == MC_WORLD_STORAGE_OK && s->result.selection_ready &&
           !cq_pending(&s->result) &&
           s->result.selection.status == MC_CHUNK_ACQUIRE_CALLBACK_FAILED &&
           kl_world_same(&s->result.selection.world, &outer) &&
           mc_entity_tick_execution_loader_read(&s->execution, &s->result.selection, &s->view) &&
           s->view.failure != NULL && s->view.status == MC_CHUNK_ACQUIRE_CALLBACK_FAILED &&
           world->active_calls == calls);
    s->selected = s->result.selection;
    /* Selection is fixed BEFORE passive producer provenance validation. */
    mc_world_source_chunk_loader_state producer;
    VERIFY(mc_world_source_chunk_loader_inspect(construction.loader, &producer) &&
           producer.phase == MC_WORLD_SOURCE_FAILED && producer.last_pc == 18 &&
           producer.reader != NULL && producer.record != NULL && producer.failure != NULL &&
           s->view.failure == &s->execution.loaders[0] && s->view.failure != producer.failure &&
           kl_error_same(s->view.failure, producer.failure) &&
           s->view.failure->source.status == MC_WORLD_SOURCE_SOURCE_EXCEPTION &&
           s->view.failure->source.source_exception && !s->view.failure->source.native_policy &&
           s->view.failure->input.source_fault == MC_NBT_SOURCE_ROOT &&
           s->view.failure->source.frame_count == 1 && s->view.failure->source.frames[0].pc == 18 &&
           s->view.failure->source.frames[0].class_name != NULL &&
           strcmp(s->view.failure->source.frames[0].class_name, "ld") == 0 &&
           s->view.failure->storage.status == status && s->execution.loader_count == 1 &&
           s->execution.phase == MC_ENTITY_TICK_EXECUTION_FAILED &&
           f->providers[0].cache->count == 0 && f->providers[0].cache->insertion_count == 0);
    if (!null_error)
        VERIFY(caller->nearby.query.status == status &&
               caller->nearby.query.acquisition == MC_CHUNK_ACQUIRE_CALLBACK_FAILED &&
               caller->nearby.query.bytecode == 48 && caller->nearby.query.provider_bytecode == 6 &&
               caller->nearby.load_bytecode == 6 &&
               caller->nearby.query.source_chunk == s->view.failure &&
               caller->nearby.query.cause.operation == producer.failure->storage.operation &&
               caller->nearby.query.cause.status == producer.failure->storage.status &&
               caller->nearby.query.cause.system_error == producer.failure->storage.system_error);
    cq_marker("loader selected");
    free(caller); /* Actual caller diagnostics only, no Source owner or field. */
    caller = NULL;
    s->view = (mc_entity_tick_execution_loader_view){0};
    VERIFY(caller == NULL &&
           mc_entity_tick_execution_loader_read(&s->execution, &s->selected, &s->view) &&
           kl_error_same(s->view.failure, producer.failure) &&
           mc_entity_tick_execution_pop_world(&s->execution, &outer, true) == MC_WORLD_STORAGE_OK &&
           world->persistent_bindings == bindings + 1 &&
           mc_world_runtime_free(world, &s->source.error) == MC_WORLD_STORAGE_BUSY &&
           mc_entity_tick_execution_loader_read(&s->execution, &s->selected, &s->view) &&
           kl_error_same(s->view.failure, producer.failure));
    cq_marker("loader retained");
    s->view = (mc_entity_tick_execution_loader_view){0};
    s->result = (mc_world_runtime_execution_result){0};
    producer = (mc_world_source_chunk_loader_state){0};
    construction = (mc_world_runtime_source_construction_state){0};
    VERIFY(mc_entity_tick_execution_retire(&s->execution) == MC_WORLD_STORAGE_OK &&
           world->persistent_bindings == bindings && s->execution.loader_count == 0 &&
           s->execution.world_count == 0);
    cq_marker("loader retired");
    return 0;
}

static void cq_probe_node(cq_fixture *s, bool metadata, bool public_error) {
    cq_scope before = {0};
    const bool snapped = cq_scope_take(s, &before);
    int32_t actual = INT32_C(0x13579bdf);
    mc_world_collision_error_init(&s->probe);
    const mc_world_storage_status status =
        cq_get(s, 8, 64, 8, metadata, &actual, public_error ? &s->probe : NULL, &s->probe_result);
    ++s->probes;
    s->probe_valid &=
        snapped && status == MC_WORLD_STORAGE_BUSY && actual == INT32_C(0x13579bdf) &&
        s->probe_result.ownership_status == status &&
        s->probe_result.ownership_error.operation != NULL &&
        strcmp(s->probe_result.ownership_error.operation,
               "Collision block value evaluated World ownership") == 0 &&
        s->probe_result.ownership_error.site.class_name == cq_site.class_name &&
        s->probe_result.ownership_error.site.method == cq_site.method &&
        s->probe_result.ownership_error.site.descriptor == cq_site.descriptor &&
        s->probe_result.ownership_error.site.pc == cq_site.pc && !s->probe_result.selection_ready &&
        !cq_pending(&s->probe_result) &&
        (!public_error ||
         (s->probe.nearby.query.status == status && s->probe.nearby.query.bytecode == 0 &&
          s->probe.nearby.query.provider_bytecode == 0 && s->probe.nearby.query.operation != NULL &&
          strcmp(s->probe.nearby.query.operation,
                 "Collision block value evaluated World ownership") == 0)) &&
        cq_scope_same(s, &before);
}

static mc_world_storage_status cq_dirty_callback(void *context, mc_world_runtime *world, int32_t x,
                                                 int32_t y, int32_t z, mc_tile_entity *tile,
                                                 mc_world_storage_error *cause,
                                                 mc_world_access_failure *failure) {
    cq_fixture *s = context;
    ++s->callbacks;
    s->node = mc_entity_tick_execution_world_lease(&s->execution);
    s->callback_valid &= world == s->source.game.worlds[0] && x == 8 && y == 64 && z == 8 &&
                         tile == NULL && s->callbacks == 1 && s->node != NULL &&
                         s->node->phase == MC_WORLD_ACCESS_FAILURE_LEASE_ARMED &&
                         s->receiver->active_holds == 1;
    if (strcmp(s->mode, "armed") == 0) {
        cq_probe_node(s, false, true);
        cq_probe_node(s, true, false);
        return MC_WORLD_STORAGE_OK; /* Declared authored input after native-only probes. */
    }
    bool removed = false;
    const mc_world_storage_status removal =
        mc_world_runtime_remove_world_access(world, s->receiver, &removed, cause);
    s->removed = removal == MC_WORLD_STORAGE_OK && removed;
    if (!s->removed)
        return removal == MC_WORLD_STORAGE_OK ? MC_WORLD_STORAGE_UNSUPPORTED_STATE : removal;
    s->free_during = mc_world_access_free(s->receiver, NULL);
    const mc_world_storage_status created =
        mc_world_access_receipt_create(s->receiver, NULL, &s->receipt, failure, cause);
    if (created != MC_WORLD_STORAGE_OK)
        return created;
    s->receipt->exception_class = "java/lang/IllegalStateException";
    *cause = (mc_world_storage_error){MC_WORLD_STORAGE_IO_ERROR,
                                      "Collision query control authored callback failure", 77};
    s->callback_cause = *cause;
    return cause->status;
}

static int cq_callback_case(cq_fixture *s) {
    mc_world_runtime *world = s->source.game.worlds[0];
    s->callback_valid = s->probe_valid = true;
    const mc_world_access_source_callbacks callbacks = {.tile_entity_dirty = cq_dirty_callback};
    VERIFY(mc_world_access_source_create(&callbacks, s, &s->receiver, NULL) ==
               MC_WORLD_STORAGE_OK &&
           mc_world_runtime_add_world_access(world, s->receiver, NULL) == MC_WORLD_STORAGE_OK);
    mc_entity_tick_execution_world_token outer;
    VERIFY(mc_entity_tick_execution_push_world(&s->execution, world, cq_site, &outer) ==
           MC_WORLD_STORAGE_OK);
    const mc_world_storage_status status = mc_world_runtime_tile_entity_dirty_in_execution(
        &s->execution, world, cq_code, 8, 64, 8, NULL, &s->dirty, &s->result);
    VERIFY(s->callbacks == 1 && s->callback_valid);
    if (strcmp(s->mode, "held") == 0) {
        VERIFY(status == MC_WORLD_STORAGE_IO_ERROR && s->removed && s->receipt != NULL &&
               s->free_during == MC_WORLD_STORAGE_BUSY && s->receiver->active_holds == 1 &&
               s->dirty.world_access_failure.receipt == s->receipt &&
               s->dirty.cause.status == s->callback_cause.status &&
               s->dirty.cause.operation == s->callback_cause.operation);
        const mc_world_access_failure *view = NULL;
        VERIFY(mc_world_access_failure_lease_inspect(s->node, &view) &&
               view->receiver == s->receiver && view->receipt == s->receipt);
        cq_probe_node(s, false, true);
        cq_probe_node(s, true, false);
        VERIFY(s->probes == 2 && s->probe_valid &&
               mc_entity_tick_execution_pop_world(&s->execution, &outer, true) ==
                   MC_WORLD_STORAGE_OK &&
               mc_world_access_receipt_retire(&s->receipt, NULL) == MC_WORLD_STORAGE_BUSY &&
               mc_world_access_free(s->receiver, NULL) == MC_WORLD_STORAGE_BUSY);
        s->dirty.world_access_failure = (mc_world_access_failure){0};
        s->probe.nearby.query.source_chunk = NULL;
        s->result = s->probe_result = (mc_world_runtime_execution_result){0};
        view = NULL;
        VERIFY(mc_entity_tick_execution_retire(&s->execution) == MC_WORLD_STORAGE_OK &&
               mc_world_access_receipt_retire(&s->receipt, NULL) == MC_WORLD_STORAGE_OK &&
               mc_world_access_free(s->receiver, NULL) == MC_WORLD_STORAGE_OK &&
               s->dirty.cause.status == s->callback_cause.status &&
               s->dirty.cause.operation == s->callback_cause.operation);
    } else {
        VERIFY(status == MC_WORLD_STORAGE_OK && s->probes == 2 && s->probe_valid &&
               s->execution.receipt_count == 0 && s->receiver->active_holds == 0);
        bool removed = false;
        VERIFY(mc_world_runtime_remove_world_access(world, s->receiver, &removed, NULL) ==
                   MC_WORLD_STORAGE_OK &&
               removed &&
               mc_entity_tick_execution_pop_world(&s->execution, &outer, false) ==
                   MC_WORLD_STORAGE_OK);
        s->result = s->probe_result = (mc_world_runtime_execution_result){0};
        VERIFY(mc_world_access_free(s->receiver, NULL) == MC_WORLD_STORAGE_OK);
    }
    s->receiver = NULL;
    s->node = NULL;
    return 0;
}

int main(int argc, char **argv) {
    VERIFY(argc == 3 &&
           (strcmp(argv[1], "id-first") == 0 || strcmp(argv[1], "metadata-first") == 0 ||
            strcmp(argv[1], "domain") == 0 || strcmp(argv[1], "loader-error") == 0 ||
            strcmp(argv[1], "loader-null-error") == 0 || strcmp(argv[1], "armed") == 0 ||
            strcmp(argv[1], "held") == 0));
    printf("COLLISION_QUERY_BEGIN schema=1 mode=%s\n", argv[1]);
    fflush(stdout);
    cq_fixture *s = calloc(1, sizeof(*s));
    VERIFY(s != NULL);
    s->mode = argv[1];
    VERIFY(ss_setup(&s->source, argv[2]) == 0 && cq_files(s, true) == 0 &&
           ss_open(&s->source) == 0);
    mc_entity_tick_execution_init(&s->execution, NULL);
    if (strcmp(s->mode, "domain") == 0) {
        cq_marker("domain BEGIN");
        VERIFY(cq_domain(s) == 0);
        cq_marker("domain END");
    } else if (strncmp(s->mode, "loader-", 7) == 0)
        VERIFY(cq_loader_failure(s, strcmp(s->mode, "loader-null-error") == 0) == 0);
    else {
        cq_marker("cold BEGIN");
        VERIFY(cq_positive(s, 0, strcmp(s->mode, "metadata-first") == 0) == 0);
        cq_marker("cold END");
        if (strcmp(s->mode, "armed") == 0 || strcmp(s->mode, "held") == 0) {
            cq_marker("callback BEGIN");
            VERIFY(cq_callback_case(s) == 0);
            cq_marker("callback END");
        } else {
            cq_marker("cache BEGIN");
            for (size_t pass = 1; pass < 4; ++pass)
                VERIFY(cq_positive(s, pass, strcmp(s->mode, "metadata-first") == 0) == 0);
            cq_marker("cache END");
        }
    }
    VERIFY(mc_entity_tick_execution_retire(&s->execution) == MC_WORLD_STORAGE_OK);
    VERIFY((s->source.game.states[0] == NULL ? cq_empty_stop(&s->source) : ss_stop(&s->source)) ==
               0 &&
           cq_files(s, false) == 0 && ss_finish(&s->source) == 0);
    for (size_t i = 0; i < COUNT(s->inputs); ++i)
        mc_ws_free_file_bytes(&s->inputs[i]);
    free(s);
    printf("COLLISION_QUERY_OPEN Blank/foreign-Store/structural-unwind/full-SL\n");
    printf("PASS: %u actual owned collision scalar checks;mode=%s\n", checks, argv[1]);
    return 0;
}
