/* Reuse the actual Registry resources/constructors and native teardown helpers.
 * The renamed program is never run; no legacy Store/region writer is called. */
int tl_unused_tile_main(int, char **);
#define main tl_unused_tile_main
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

/* Native component inputs over the real SourceSaving constructor graph.
 * The caller keeps actual Registry/classes/Store/import/loader code live through
 * all diagnostics and cleanup. No imported main, legacy save, fake current node,
 * Source pointer repair, or manual loader reserve/finish is executed. */
#include "world/runtime/tile_entities/execution_internal.h"
#include "world/runtime/tile_entities/tile_entities_internal.h"
#include "world/runtime/world_access_internal.h"

typedef struct tl_fixture {
    source_save_fixture source;
    mc_entity_tick_execution execution;
    mc_world_runtime_execution_result result, probe_result;
    mc_live_tile_entity_error error, probe_error;
    mc_tile_entity *tile;
    mc_world_access *receiver;
    mc_world_access_receipt *receipt;
    mc_world_access_failure_lease *node;
    mc_world_storage_error callback_cause;
    mc_world_source_chunk_loader_state loaded;
    mc_ws_file_bytes inputs[3];
    const char *mode;
    size_t callbacks, probes;
    mc_world_storage_status free_during, nested_status, nested_pop;
    bool callback_valid, probe_valid, removed;
} tl_fixture;

static int tl_files(tl_fixture *s, bool capture) {
    static const char *const leaves[] = {"world/level.dat", "world/region/r.0.0.mcr",
                                         "world/preserve.bin"};
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

static int tl_remember(tl_fixture *s) {
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

/* Explicit actual client defining-code inputs, never inferred from Chunk.d.
 * ss_setup owns these client namespaces; these VALUE labels cause no ensure. */
static const mc_world_runtime_execution_code tl_code = {{"lm", "e", "()V", 6},
                                                        {"lm", "e", "()V", 33}};
static const mc_entity_tick_execution_site tl_lazy_site = {"lm", "d", "(III)Low;", 67};
static const mc_entity_tick_execution_site tl_entry_site = {"native.tile.control", "lookup", "()V",
                                                            0};

static void tl_marker(const char *event) {
    printf("TILE_LOOKUP_CASE %s\n", event);
    fflush(stdout);
}

static bool tl_pending(const mc_world_runtime_execution_result *result) {
    bool pending = result->root_pending;
    for (size_t i = 0; i < MC_ENTITY_TICK_EXECUTION_LOADERS; ++i)
        pending |= result->pending[i].used;
    return pending;
}

static bool tl_token_same(mc_entity_tick_execution_world_token a,
                          mc_entity_tick_execution_world_token b) {
    return a.owner == b.owner && a.slot == b.slot && a.generation == b.generation;
}

static bool tl_owner_error_same(mc_entity_tick_execution_error a,
                                mc_entity_tick_execution_error b) {
    return a.status == b.status && a.operation == b.operation &&
           a.site.class_name == b.site.class_name && a.site.method == b.site.method &&
           a.site.descriptor == b.site.descriptor && a.site.pc == b.site.pc;
}

static mc_world_storage_status tl_get(tl_fixture *s, mc_tile_entity **out,
                                      mc_live_tile_entity_error *error,
                                      mc_world_runtime_execution_result *result, bool defining) {
    fixture *f = &s->source.game;
    return mc_live_tile_entities_get_in_execution(
        &s->execution, f->worlds[0], tl_code, defining ? f->registry[0] : NULL,
        defining ? f->classes[0] : NULL, tl_lazy_site, 8, 64, 8, out, error, result);
}

typedef struct tl_scope {
    mc_entity_tick_execution_world_token current;
    mc_entity_tick_execution_phase phase;
    mc_entity_tick_execution_error first, capture, cleanup;
    mc_world_access_failure_lease *node;
    mc_world_access_failure_lease_phase node_phase;
    mc_world_access *node_receiver;
    mc_world_access_receipt *node_receipt;
    size_t bindings, active_calls, world_count, lease_count, receipt_count, loader_count;
    size_t cache_count, insertion_count, tile_holds, receiver_holds;
    bool open, held, child_failed, capture_pending;
    mc_world_source_chunk_loader_state loader;
} tl_scope;

static bool tl_scope_take(tl_fixture *s, tl_scope *out) {
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
    out->tile_holds = s->tile->holds;
    out->receiver_holds = s->receiver->active_holds;
    out->open = s->execution.world_open;
    out->held = s->execution.world_held;
    out->child_failed = s->execution.child_failed;
    out->capture_pending = s->execution.capture_pending;
    return true;
}

static bool tl_scope_same(tl_fixture *s, const tl_scope *before) {
    tl_scope after = {0};
    return tl_scope_take(s, &after) && tl_token_same(after.current, before->current) &&
           after.phase == before->phase && tl_owner_error_same(after.first, before->first) &&
           tl_owner_error_same(after.capture, before->capture) &&
           tl_owner_error_same(after.cleanup, before->cleanup) && after.node == before->node &&
           after.node_phase == before->node_phase && after.node_receiver == before->node_receiver &&
           after.node_receipt == before->node_receipt && after.bindings == before->bindings &&
           after.active_calls == before->active_calls && after.world_count == before->world_count &&
           after.lease_count == before->lease_count &&
           after.receipt_count == before->receipt_count &&
           after.loader_count == before->loader_count && after.cache_count == before->cache_count &&
           after.insertion_count == before->insertion_count &&
           after.tile_holds == before->tile_holds &&
           after.receiver_holds == before->receiver_holds && after.open == before->open &&
           after.held == before->held && after.child_failed == before->child_failed &&
           after.capture_pending == before->capture_pending &&
           after.loader.phase == before->loader.phase &&
           after.loader.reader == before->loader.reader &&
           after.loader.document == before->loader.document &&
           after.loader.record == before->loader.record &&
           after.loader.last_pc == before->loader.last_pc;
}

/* Called only under a genuine same-node ARMED callback or transferred HELD
 * lease. Admission/error observations are values; no owning node is copied. */
static void tl_probe_same_node(tl_fixture *s, bool public_error) {
    tl_scope before = {0};
    const bool snapped = tl_scope_take(s, &before);
    mc_tile_entity *actual = s->tile; /* Genuine retained owner, not an invented pointer. */
    const mc_world_storage_status status =
        tl_get(s, &actual, public_error ? &s->probe_error : NULL, &s->probe_result, true);
    const bool output_same = actual == s->tile;
    /* An unexpected successful result still owns its documented ordinary hold.
     * Consume that actual occurrence before checking the expected refusal. */
    if (status == MC_WORLD_STORAGE_OK)
        mc_tile_entity_release(actual);
    ++s->probes;
    s->probe_valid &=
        snapped && status == MC_WORLD_STORAGE_BUSY && output_same &&
        s->probe_result.ownership_status == status &&
        s->probe_result.ownership_error.status == status &&
        s->probe_result.ownership_error.operation != NULL &&
        strcmp(s->probe_result.ownership_error.operation,
               "TileEntity lookup evaluated World ownership") == 0 &&
        s->probe_result.ownership_error.site.class_name == tl_lazy_site.class_name &&
        s->probe_result.ownership_error.site.method == tl_lazy_site.method &&
        s->probe_result.ownership_error.site.descriptor == tl_lazy_site.descriptor &&
        s->probe_result.ownership_error.site.pc == tl_lazy_site.pc &&
        !s->probe_result.selection_ready && !tl_pending(&s->probe_result) &&
        (!public_error ||
         (s->probe_error.status == status && s->probe_error.bytecode == 0 &&
          s->probe_error.provider_bytecode == 0 && s->probe_error.operation != NULL &&
          strcmp(s->probe_error.operation, "TileEntity lookup evaluated World ownership") == 0)) &&
        tl_scope_same(s, &before);
}

static mc_world_storage_status tl_dirty_callback(void *context, mc_world_runtime *world, int32_t x,
                                                 int32_t y, int32_t z, mc_tile_entity *tile,
                                                 mc_world_storage_error *cause,
                                                 mc_world_access_failure *failure) {
    tl_fixture *s = context;
    ++s->callbacks;
    s->node = mc_entity_tick_execution_world_lease(&s->execution);
    s->callback_valid &= s->callbacks == 1 && world == s->source.game.worlds[0] && x == 8 &&
                         y == 64 && z == 8 && tile == s->tile && s->node != NULL &&
                         s->node->phase == MC_WORLD_ACCESS_FAILURE_LEASE_ARMED &&
                         s->receiver->active_holds == 1;
    if (strcmp(s->mode, "armed") == 0) {
        tl_probe_same_node(s, true);
        tl_probe_same_node(s, false);
        return MC_WORLD_STORAGE_OK; /* Declared callback returns after native probes. */
    }
    if (strcmp(s->mode, "nested") == 0) {
        tl_scope before = {0};
        const bool snapped = tl_scope_take(s, &before);
        if (!snapped) {
            *cause = (mc_world_storage_error){MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                              "Tile control actual nested scope inspection", 0};
            return cause->status;
        }
        mc_entity_tick_execution_world_token nested;
        const mc_world_storage_status pushed = mc_entity_tick_execution_push_world(
            &s->execution, world,
            (mc_entity_tick_execution_site){"native.tile.control", "nested", "()V", 0}, &nested);
        s->nested_status = pushed;
        if (pushed != MC_WORLD_STORAGE_OK) {
            *cause = (mc_world_storage_error){pushed, "Tile control nested admission", 0};
            return pushed;
        }
        mc_world_access_failure_lease *nested_node =
            mc_entity_tick_execution_world_lease(&s->execution);
        s->callback_valid &= nested_node != NULL && nested_node != s->node &&
                             nested_node->phase == MC_WORLD_ACCESS_FAILURE_LEASE_IDLE &&
                             nested.slot != before.current.slot &&
                             world->persistent_bindings == before.bindings + 1;
        mc_tile_entity *actual = NULL;
        s->nested_status = tl_get(s, &actual, &s->probe_error, &s->probe_result, true);
        s->callback_valid &= s->nested_status == MC_WORLD_STORAGE_OK && actual == s->tile &&
                             s->probe_result.ownership_status == MC_WORLD_STORAGE_OK &&
                             !s->probe_result.selection_ready && !tl_pending(&s->probe_result);
        if (s->nested_status == MC_WORLD_STORAGE_OK)
            mc_tile_entity_release(actual);
        s->nested_pop = mc_entity_tick_execution_pop_world(&s->execution, &nested,
                                                           s->nested_status != MC_WORLD_STORAGE_OK);
        s->callback_valid &=
            s->nested_pop == MC_WORLD_STORAGE_OK && snapped && tl_scope_same(s, &before);
        if (s->nested_status != MC_WORLD_STORAGE_OK) {
            *cause = s->probe_error.cause;
            *failure = s->probe_error.world_access_failure;
            return s->nested_status; /* Preserve actual unexpected child failure, not success. */
        }
        if (s->nested_pop != MC_WORLD_STORAGE_OK) {
            *cause = (mc_world_storage_error){s->nested_pop, "Tile control nested cleanup", 0};
            return s->nested_pop;
        }
        return MC_WORLD_STORAGE_OK;
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
                                      "Tile control authored callback failure", 77};
    s->callback_cause = *cause;
    return cause->status;
}

static int tl_positive(tl_fixture *s, int pass, const char *label) {
    fixture *f = &s->source.game;
    mc_world_runtime *world = f->worlds[0];
    tl_marker(label);
    const size_t before_bindings = world->persistent_bindings;
    mc_entity_tick_execution_world_token outer;
    VERIFY(mc_entity_tick_execution_push_world(&s->execution, world, tl_entry_site, &outer) ==
           MC_WORLD_STORAGE_OK);
    mc_tile_entity *actual = NULL;
    const mc_world_storage_status status =
        tl_get(s, &actual, pass == 1 ? NULL : &s->error, &s->result, pass != 2);
    VERIFY(status == MC_WORLD_STORAGE_OK && actual != NULL &&
           s->result.ownership_status == MC_WORLD_STORAGE_OK && !s->result.selection_ready &&
           !tl_pending(&s->result) && s->execution.loader_count == 0 &&
           s->execution.receipt_count == 0 && s->execution.current_lease == outer.slot);
    if (pass == 0) {
        s->tile = actual; /* Keep this genuine returned occurrence until before terminal cleanup. */
        VERIFY(tl_remember(s) == 0 && f->chest_id[0] == 54);
        mc_world_runtime_source_construction_state construction;
        VERIFY(mc_world_runtime_source_construction_inspect(world, &construction) &&
               construction.loader != NULL &&
               mc_world_source_chunk_loader_inspect(construction.loader, &s->loaded) &&
               s->loaded.phase == MC_WORLD_SOURCE_READY && s->loaded.reader != NULL &&
               s->loaded.document != NULL);
        mc_world_chunk_fields fields;
        size_t map_count = SIZE_MAX;
        mc_tile_entity_snapshot tile;
        VERIFY(mc_world_chunk_fields_get(f->chunks[0], &fields) == MC_WORLD_STORAGE_OK &&
               mc_world_chunk_tile_entity_count(fields.l, &map_count) == MC_WORLD_STORAGE_OK &&
               map_count == 1 && mc_live_tile_entities_map_valid(f->chunks[0]) &&
               mc_live_tile_entities_lists_valid(world) && world->fields.c->count == 1 &&
               mc_tile_entity_get_snapshot(actual, &tile) == MC_TILE_ENTITY_OK &&
               tile.kind == MC_TILE_ENTITY_CHEST && tile.phase == MC_TILE_ENTITY_READY &&
               tile.world == world && tile.x == 8 && tile.y == 64 && tile.z == 8 && !tile.invalid &&
               actual->holds >= 3);
        /* Construction keeps 36 physical slots; the source NBT read replaces
         * that array using the logical inventory size, 27. */
        if (strcmp(s->mode, "lazy") == 0) {
            VERIFY(actual->source == NULL && tile.chest_capacity == 36 &&
                   world->fields.b->count == 0);
            for (size_t i = 0; i < actual->state.chest.capacity; ++i)
                VERIFY(actual->state.chest.slots[i] == NULL);
        } else {
            VERIFY(actual->source != NULL && tile.chest_capacity == 27 &&
                   world->fields.b->count == 1 &&
                   mc_entity_is_kind(world->fields.b->items[0], MC_ENTITY_CLASS_KIND_ITEM));
            mc_item_stack_fields stack;
            VERIFY(actual->state.chest.slots[0] != NULL &&
                   mc_item_stack_fields_get(actual->state.chest.slots[0], &stack) ==
                       MC_ITEM_STACK_OK &&
                   stack.id == 262 && stack.count == 7 && stack.damage == 3);
        }
    } else {
        VERIFY(actual == s->tile && world->persistent_bindings == before_bindings + 1);
        mc_world_runtime_source_construction_state construction;
        mc_world_source_chunk_loader_state after;
        VERIFY(mc_world_runtime_source_construction_inspect(world, &construction) &&
               mc_world_source_chunk_loader_inspect(construction.loader, &after) &&
               after.phase == s->loaded.phase && after.reader == s->loaded.reader &&
               after.document == s->loaded.document && after.record == s->loaded.record &&
               after.last_pc == s->loaded.last_pc);
        const size_t actual_holds = actual->holds;
        mc_tile_entity_release(actual);
        VERIFY(s->tile->holds + 1 == actual_holds);
    }
    const size_t before_pop = world->persistent_bindings;
    VERIFY(mc_entity_tick_execution_pop_world(&s->execution, &outer, false) ==
               MC_WORLD_STORAGE_OK &&
           world->persistent_bindings + 1 == before_pop && s->execution.world_count == 0 &&
           s->execution.lease_count == 0 && f->providers[0].cache->count == 1 &&
           f->providers[0].cache->insertion_count == 1);
    s->result = (mc_world_runtime_execution_result){0};
    return 0;
}

static int tl_callback_case(tl_fixture *s) {
    fixture *f = &s->source.game;
    mc_world_runtime *world = f->worlds[0];
    s->callback_valid = s->probe_valid = true;
    const mc_world_access_source_callbacks callbacks = {.tile_entity_dirty = tl_dirty_callback};
    VERIFY(mc_world_access_source_create(&callbacks, s, &s->receiver, NULL) ==
               MC_WORLD_STORAGE_OK &&
           mc_world_runtime_add_world_access(world, s->receiver, NULL) == MC_WORLD_STORAGE_OK);
    mc_entity_tick_execution_world_token outer;
    VERIFY(mc_entity_tick_execution_push_world(&s->execution, world, tl_entry_site, &outer) ==
           MC_WORLD_STORAGE_OK);
    tl_marker("callback BEGIN");
    const mc_world_storage_status status = mc_world_runtime_tile_entity_dirty_in_execution(
        &s->execution, world, tl_code, 8, 64, 8, s->tile, &s->error, &s->result);
    VERIFY(s->callbacks == 1 && s->callback_valid);
    if (strcmp(s->mode, "held") == 0) {
        VERIFY(status == MC_WORLD_STORAGE_IO_ERROR && s->receipt != NULL && s->removed &&
               s->free_during == MC_WORLD_STORAGE_BUSY && s->receiver->memberships == 0 &&
               s->receiver->active_holds == 1 && s->node == &s->execution.leases[outer.slot] &&
               s->error.world_access_bytecode == 55 &&
               s->error.world_access_failure.receipt == s->receipt &&
               s->error.cause.status == s->callback_cause.status &&
               s->error.cause.operation == s->callback_cause.operation &&
               s->error.cause.system_error == s->callback_cause.system_error &&
               s->result.ownership_status == MC_WORLD_STORAGE_OK && !s->result.selection_ready &&
               !tl_pending(&s->result));
        const mc_world_access_failure *view = NULL;
        const char *exception = NULL;
        VERIFY(mc_world_access_failure_lease_inspect(s->node, &view) &&
               view->receiver == s->receiver && view->receipt == s->receipt &&
               mc_world_access_failure_source_exception(view, &exception) && exception != NULL &&
               strcmp(exception, "java/lang/IllegalStateException") == 0);
        tl_probe_same_node(s, true);
        tl_probe_same_node(s, false);
        VERIFY(s->probes == 2 && s->probe_valid &&
               s->error.cause.operation == s->callback_cause.operation &&
               mc_entity_tick_execution_pop_world(&s->execution, &outer, true) ==
                   MC_WORLD_STORAGE_OK &&
               mc_world_access_receipt_retire(&s->receipt, NULL) == MC_WORLD_STORAGE_BUSY &&
               mc_world_access_free(s->receiver, NULL) == MC_WORLD_STORAGE_BUSY);
        s->error.world_access_failure = (mc_world_access_failure){0};
        s->probe_error.world_access_failure = (mc_world_access_failure){0};
        s->result = s->probe_result = (mc_world_runtime_execution_result){0};
        view = NULL; /* All typed views expire before checked lease retirement. */
        VERIFY(mc_entity_tick_execution_retire(&s->execution) == MC_WORLD_STORAGE_OK &&
               mc_world_access_receipt_retire(&s->receipt, NULL) == MC_WORLD_STORAGE_OK &&
               mc_world_access_free(s->receiver, NULL) == MC_WORLD_STORAGE_OK &&
               s->error.cause.status == s->callback_cause.status &&
               s->error.cause.operation == s->callback_cause.operation &&
               s->error.cause.system_error == s->callback_cause.system_error);
    } else {
        VERIFY(status == MC_WORLD_STORAGE_OK && s->result.ownership_status == MC_WORLD_STORAGE_OK &&
               !s->result.selection_ready && !tl_pending(&s->result) &&
               s->execution.receipt_count == 0 && s->receiver->active_holds == 0 &&
               s->receipt == NULL);
        if (strcmp(s->mode, "armed") == 0)
            VERIFY(s->probes == 2 && s->probe_valid);
        else
            VERIFY(s->nested_status == MC_WORLD_STORAGE_OK && s->nested_pop == MC_WORLD_STORAGE_OK);
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
    tl_marker("callback END");
    return 0;
}

int main(int argc, char **argv) {
    VERIFY(argc == 3 && (strcmp(argv[1], "map") == 0 || strcmp(argv[1], "lazy") == 0 ||
                         strcmp(argv[1], "armed") == 0 || strcmp(argv[1], "held") == 0 ||
                         strcmp(argv[1], "nested") == 0));
    printf("TILE_LOOKUP_BEGIN schema=1 mode=%s\n", argv[1]);
    fflush(stdout);
    tl_fixture *s = calloc(1, sizeof(*s));
    VERIFY(s != NULL);
    s->mode = argv[1];
    VERIFY(ss_setup(&s->source, argv[2]) == 0 && tl_files(s, true) == 0 &&
           ss_open(&s->source) == 0 && s->source.game.providers[0].cache->count == 0 &&
           s->source.game.providers[0].cache->insertion_count == 0);
    mc_entity_tick_execution_init(&s->execution, NULL);
    VERIFY(tl_positive(s, 0, "cold BEGIN") == 0);
    tl_marker("cold END");
    if (strcmp(s->mode, "map") == 0 || strcmp(s->mode, "lazy") == 0) {
        VERIFY(tl_positive(s, 1, "cache BEGIN") == 0);
        tl_marker("cache END");
        VERIFY(tl_positive(s, 2, "unused-defining BEGIN") == 0);
        tl_marker("unused-defining END");
    } else
        VERIFY(tl_callback_case(s) == 0);
    VERIFY(mc_entity_tick_execution_retire(&s->execution) == MC_WORLD_STORAGE_OK);
    mc_tile_entity_release(s->tile);
    s->tile = NULL;
    /* Real stop closes cached region handles BEFORE independent disk readback. */
    VERIFY(ss_stop(&s->source) == 0 && tl_files(s, false) == 0 && ss_finish(&s->source) == 0);
    for (size_t i = 0; i < COUNT(s->inputs); ++i)
        mc_ws_free_file_bytes(&s->inputs[i]);
    free(s);
    printf("TILE_LOOKUP_OPEN foreign-Store/null-Chunk-world/caller-unwind/full-SL\n");
    printf("PASS: %u actual Tile lookup checks;mode=%s\n", checks, argv[1]);
    return 0;
}
