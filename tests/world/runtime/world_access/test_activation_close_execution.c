/* Reuse the actual Registry resources/constructors and native teardown helpers.
 * The renamed program is never run; no legacy Store/region writer is called. */
int ac_unused_tile_main(int, char **);
#define main ac_unused_tile_main
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

typedef struct ac_fixture {
    source_save_fixture source;
    mc_entity_tick_execution execution;
    mc_world_runtime_acquisition_error acquired;
    mc_world_runtime_execution_result result;
    mc_world_access *receiver;
    mc_world_access_receipt *receipt;
    mc_world_access_failure_lease *node;
    mc_world_storage_error callback_cause;
    mc_ws_file_bytes inputs[3];
    const char *mode;
    mc_world_storage_status callback_status, free_during;
    size_t callbacks, dirty_callbacks, saved_bindings;
    bool source_throwable, fault, projected, removed, callback_valid, null_error;
} ac_fixture;

static const mc_world_runtime_execution_code ac_code = {{"lm", "e", "()V", 6},
                                                        {"lm", "e", "()V", 33}};

static void ac_marker(const char *event) {
    printf("ACTIVATION_CLOSE_CASE %s\n", event);
    fflush(stdout);
}

static void ac_restore(ac_fixture *s) {
    if (s->projected) {
        s->source.game.worlds[0]->persistent_bindings = s->saved_bindings;
        s->projected = false;
    }
}

static int ac_files(ac_fixture *s, bool capture) {
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

static mc_world_storage_status ac_dirty(void *context, mc_world_runtime *world, int32_t x,
                                        int32_t y, int32_t z, mc_tile_entity *tile,
                                        mc_world_storage_error *cause,
                                        mc_world_access_failure *failure) {
    ac_fixture *s = context;
    (void)cause;
    (void)failure;
    ++s->dirty_callbacks;
    s->callback_valid &= world == s->source.game.worlds[0] && tile != NULL && x == 8 && y == 64 &&
                         z == 8 && s->callbacks == 0;
    return MC_WORLD_STORAGE_OK; /* Declared authored WA input, not a Tile body. */
}

static mc_world_storage_status ac_added(void *context, mc_world_runtime *world, mc_entity *entity,
                                        mc_world_storage_error *cause,
                                        mc_world_access_failure *failure) {
    ac_fixture *s = context;
    fixture *f = &s->source.game;
    ++s->callbacks;
    s->callback_valid &=
        world == f->worlds[0] && s->callbacks == 1 && entity != NULL &&
        mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_ITEM) && world->fields.b->count == 1 &&
        world->fields.b->items[0] == entity && world->entity_membership.count == 1 &&
        world->entity_membership.entries[0] == entity && world->fields.c->count == 1 &&
        mc_live_tile_entities_lists_valid(world) && s->execution.world_open &&
        s->execution.world == world && s->execution.world_held;
    mc_world_access_failure_lease *invoked = mc_entity_tick_execution_world_lease(&s->execution);
    s->callback_valid &= invoked != NULL && invoked->phase == MC_WORLD_ACCESS_FAILURE_LEASE_ARMED;
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

static bool ac_pending(const mc_world_runtime_execution_result *result) {
    bool pending = result->root_pending;
    for (size_t i = 0; i < MC_ENTITY_TICK_EXECUTION_LOADERS; ++i)
        pending |= result->pending[i].used;
    return pending;
}

static int ac_remember(ac_fixture *s) {
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

/* Returned result is the ONLY no-GET descendant close authority. */
static bool ac_same_world(mc_entity_tick_execution_world_token a,
                          mc_entity_tick_execution_world_token b) {
    return a.owner == b.owner && a.slot == b.slot && a.generation == b.generation;
}

static bool ac_same_site(mc_entity_tick_execution_site a, mc_entity_tick_execution_site b) {
    return a.class_name == b.class_name && a.method == b.method && a.descriptor == b.descriptor &&
           a.pc == b.pc;
}

static bool ac_same_error(mc_entity_tick_execution_error a, mc_entity_tick_execution_error b) {
    return a.status == b.status && a.operation == b.operation && ac_same_site(a.site, b.site);
}

static bool ac_same_close(mc_world_runtime_execution_close a, mc_world_runtime_execution_close b) {
    return a.used == b.used && a.child_failed == b.child_failed && a.kind == b.kind &&
           a.phase == b.phase && ac_same_world(a.world, b.world) &&
           a.source_status == b.source_status && a.refused_status == b.refused_status &&
           a.operation == b.operation && ac_same_site(a.site, b.site);
}

static bool ac_receipt_held(ac_fixture *s) {
    const mc_world_access_failure *view = NULL;
    return mc_world_access_failure_lease_inspect(s->node, &view) && view->receiver == s->receiver &&
           view->receipt == s->receipt && s->receiver->active_holds == 1 &&
           s->receiver->memberships == 0 && s->receipt->holds == 1 &&
           s->execution.receipt_count == 1;
}

/* Passive source/output witnesses, not a guessed producer or returned frame. */
typedef struct ac_prefix {
    const void *entities, *tiles, *membership, *item, *chest;
    size_t entity_count, tile_count, membership_count, cache_count, insertions, pending;
    size_t callbacks, dirty_callbacks;
    mc_world_source_chunk_loader_state loader;
} ac_prefix;

static bool ac_prefix_take(ac_fixture *s, ac_prefix *out) {
    fixture *f = &s->source.game;
    mc_world_runtime *world = f->worlds[0];
    mc_world_runtime_source_construction_state construction;
    if (!mc_world_runtime_source_construction_inspect(world, &construction) ||
        !mc_world_source_chunk_loader_inspect(construction.loader, &out->loader) ||
        world->fields.b->count != 1 || world->fields.c->count != 1 ||
        world->entity_membership.count != 1)
        return false;
    out->entities = world->fields.b;
    out->tiles = world->fields.c;
    out->membership = world->entity_membership.entries;
    out->item = world->fields.b->items[0];
    out->chest = world->fields.c->items[0];
    out->entity_count = world->fields.b->count;
    out->tile_count = world->fields.c->count;
    out->membership_count = world->entity_membership.count;
    out->cache_count = f->providers[0].cache->count;
    out->insertions = f->providers[0].cache->insertion_count;
    out->pending = f->providers[0].pending->count;
    out->callbacks = s->callbacks;
    out->dirty_callbacks = s->dirty_callbacks;
    return true;
}

static bool ac_prefix_same(ac_fixture *s, const ac_prefix *before) {
    ac_prefix now = {0};
    return ac_prefix_take(s, &now) && now.entities == before->entities &&
           now.tiles == before->tiles && now.membership == before->membership &&
           now.item == before->item && now.chest == before->chest &&
           now.entity_count == before->entity_count && now.tile_count == before->tile_count &&
           now.membership_count == before->membership_count &&
           now.cache_count == before->cache_count && now.insertions == before->insertions &&
           now.pending == before->pending && now.callbacks == before->callbacks &&
           now.dirty_callbacks == before->dirty_callbacks &&
           now.loader.phase == before->loader.phase && now.loader.reader == before->loader.reader &&
           now.loader.document == before->loader.document &&
           now.loader.record == before->loader.record &&
           now.loader.last_pc == before->loader.last_pc;
}

static int ac_failure(ac_fixture *s) {
    fixture *f = &s->source.game;
    mc_world_runtime *world = f->worlds[0];
    s->callback_valid = true;
    const mc_world_access_source_callbacks callbacks = {.entity_added = ac_added,
                                                        .tile_entity_dirty = ac_dirty};
    VERIFY(mc_world_access_source_create(&callbacks, s, &s->receiver, NULL) ==
               MC_WORLD_STORAGE_OK &&
           mc_world_runtime_add_world_access(world, s->receiver, NULL) == MC_WORLD_STORAGE_OK);
    mc_entity_tick_execution_world_token outer;
    VERIFY(mc_entity_tick_execution_push_world(
               &s->execution, world,
               (mc_entity_tick_execution_site){"native.activation.close", "get", "()V", 0},
               &outer) == MC_WORLD_STORAGE_OK);
    mc_chunk_state *actual = NULL;
    ac_marker("get entered");
    const mc_chunk_acquire_status status =
        mc_world_runtime_get_in_execution(&s->execution, world, ac_code, 0, 0, &actual,
                                          s->null_error ? NULL : &s->acquired, &s->result);
    ac_restore(s); /* Restore saved native scalar IMMEDIATELY, before all assertions/inspection. */
    ac_marker("get returned");
    VERIFY(status == MC_CHUNK_ACQUIRE_CALLBACK_FAILED && actual == NULL && ac_remember(s) == 0 &&
           s->callbacks == 1 && s->callback_valid && s->removed &&
           s->free_during == MC_WORLD_STORAGE_BUSY && s->receipt != NULL &&
           s->receiver->active_holds == 1 && s->receipt->holds == 1 && world->active_calls == 0 &&
           world->local_leases == 0 &&
           s->result.ownership_status == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
           s->result.ownership_error.operation != NULL &&
           strcmp(s->result.ownership_error.operation, "World Entity owned receipt capture") == 0 &&
           s->result.ownership_error.site.pc == 33 && s->result.root_pending &&
           !s->result.selection_ready && ac_pending(&s->result) &&
           s->execution.receipt_count == 0 && !s->execution.capture_pending);
    if (!s->null_error)
        VERIFY(s->acquired.status == status &&
               s->acquired.cause.status == s->callback_cause.status &&
               s->acquired.cause.operation == s->callback_cause.operation &&
               s->acquired.cause.system_error == s->callback_cause.system_error &&
               s->acquired.activation_bytecode == 42 &&
               s->acquired.activation_world_bytecode == 34 && s->acquired.batch.index == 0 &&
               s->acquired.batch.publication.world_access_bytecode == 29 &&
               s->acquired.batch.publication.world_access_failure.receipt == s->receipt &&
               s->acquired.source_chunk == NULL);
    size_t index = SIZE_MAX, count = 0;
    for (size_t i = 0; i < MC_ENTITY_TICK_EXECUTION_WORLDS; ++i)
        if (s->result.closes[i].used) {
            index = i;
            ++count;
        }
    VERIFY(count == 1 && index < MC_ENTITY_TICK_EXECUTION_WORLDS && index != outer.slot);
    mc_world_runtime_execution_close saved = s->result.closes[index];
    mc_world_runtime *selected_world = NULL;
    VERIFY(saved.world.slot == index && saved.world.owner == &s->execution &&
           saved.world.generation != 0 &&
           saved.kind == MC_WORLD_RUNTIME_EXECUTION_CLOSE_CHUNK_ENTITY &&
           saved.phase == MC_WORLD_RUNTIME_EXECUTION_CLOSE_LOCALS_DRAINED && saved.child_failed &&
           saved.source_status == MC_WORLD_STORAGE_IO_ERROR &&
           saved.refused_status == MC_WORLD_STORAGE_UNSUPPORTED_STATE && saved.operation != NULL &&
           strcmp(saved.operation, "Chunk Entity activation owned scope cleanup") == 0 &&
           saved.site.pc == 33 && saved.site.class_name != NULL &&
           strcmp(saved.site.class_name, "lm") == 0 &&
           mc_entity_tick_execution_read_world(&s->execution, &saved.world, &selected_world) &&
           selected_world == world && s->execution.current_lease == index &&
           s->execution.world_open &&
           s->execution.world_slots[index].phase == MC_ENTITY_TICK_EXECUTION_WORLD_OPEN &&
           !s->execution.world_slots[index].loader_open &&
           s->result.root_attempt.slot < MC_ENTITY_TICK_EXECUTION_LOADERS &&
           ac_same_world(s->result.root_attempt.world, outer));
    const size_t pending_index = s->result.root_attempt.slot;
    const mc_world_runtime_execution_pending returned = s->result.pending[pending_index];
    VERIFY(returned.used && returned.outcome_valid && returned.actual_status == status &&
           ac_same_world(returned.attempt.world, outer) && returned.direct_failure == NULL &&
           !returned.child_ready && !returned.child_pending && !s->result.closes[outer.slot].used);
    s->node = &s->execution.leases[index]; /* ONLY the actual returned capsule chooses this node. */
    const mc_world_access_failure *view = NULL;
    const char *exception = NULL;
    VERIFY(mc_world_access_failure_lease_inspect(s->node, &view) && view->receiver == s->receiver &&
           view->receipt == s->receipt &&
           mc_world_access_failure_source_exception(view, &exception) && exception != NULL &&
           strcmp(exception, "java/lang/IllegalStateException") == 0 &&
           s->node->callback_status == MC_WORLD_STORAGE_IO_ERROR &&
           s->node->callback_error.operation == s->callback_cause.operation);
    const mc_entity_tick_execution_error first_native = s->result.ownership_error;
    ac_prefix source = {0};
    VERIFY(ac_prefix_take(s, &source) && source.loader.phase == MC_WORLD_SOURCE_READY &&
           source.loader.reader != NULL && source.loader.document != NULL);
    ac_marker("capsule selected");
    /* Exact existing HELD right supplies this view; no new owning occurrence. */
    VERIFY(mc_entity_tick_execution_capture_receipt(&s->execution, view) == MC_WORLD_STORAGE_OK &&
           s->execution.receipt_count == 1 && s->receipt->holds == 1 &&
           ac_prefix_same(s, &source) && ac_receipt_held(s) &&
           ac_same_error(s->result.ownership_error, first_native));
    ac_marker("receipt selected");
    mc_world_runtime_execution_close bad = saved;
    VERIFY(bad.world.generation < UINT64_MAX);
    ++bad.world.generation; /* Native token-integrity input, not an expired Source-scope claim. */
    const size_t bindings = world->persistent_bindings;
    VERIFY(mc_world_runtime_execution_retry_close(&s->execution, &s->result, &bad) ==
               MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
           ac_same_close(s->result.closes[index], saved) && s->execution.current_lease == index &&
           world->persistent_bindings == bindings && s->execution.world_open &&
           ac_receipt_held(s) && ac_prefix_same(s, &source) &&
           ac_same_error(s->result.ownership_error, first_native));
    VERIFY(mc_world_runtime_execution_finish_pending(&s->execution, &s->result) ==
               MC_WORLD_STORAGE_BUSY &&
           ac_same_close(s->result.closes[index], saved) && s->result.root_pending &&
           s->result.pending[pending_index].used && s->execution.current_lease == index &&
           world->persistent_bindings == bindings && ac_prefix_same(s, &source) &&
           ac_receipt_held(s) && ac_same_error(s->result.ownership_error, first_native));
    ac_marker("token rejected");
    /* Exact slot alias and equal copied VALUE in independent actual runs. */
    VERIFY(mc_world_runtime_execution_retry_close(
               &s->execution, &s->result, s->null_error ? &saved : &s->result.closes[index]) ==
               MC_WORLD_STORAGE_OK &&
           !s->result.closes[index].used && s->execution.current_lease == outer.slot &&
           s->execution.world_slots[index].phase == MC_ENTITY_TICK_EXECUTION_WORLD_RETAINED &&
           s->result.root_pending && s->result.pending[pending_index].used &&
           world->persistent_bindings == bindings && ac_prefix_same(s, &source) &&
           ac_receipt_held(s) && ac_same_error(s->result.ownership_error, first_native));
    ac_marker("close retried");
    VERIFY(mc_world_runtime_execution_retry_close(&s->execution, &s->result, &saved) ==
               MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
           !s->result.closes[index].used && s->execution.current_lease == outer.slot &&
           s->result.root_pending && world->persistent_bindings == bindings && ac_receipt_held(s) &&
           ac_prefix_same(s, &source) && ac_same_error(s->result.ownership_error, first_native));
    ac_marker("capsule consumed");
    VERIFY(mc_world_runtime_execution_finish_pending(&s->execution, &s->result) ==
               MC_WORLD_STORAGE_OK &&
           !s->result.root_pending && !ac_pending(&s->result) && s->result.selection_ready &&
           s->result.selection.status == status &&
           ac_same_world(s->result.selection.world, outer) && ac_prefix_same(s, &source) &&
           ac_same_error(s->result.ownership_error, first_native));
    mc_entity_tick_execution_loader_view no_loader;
    VERIFY(mc_entity_tick_execution_loader_read(&s->execution, &s->result.selection, &no_loader) &&
           no_loader.status == status && no_loader.failure == NULL &&
           mc_entity_tick_execution_pop_world(&s->execution, &outer, true) == MC_WORLD_STORAGE_OK &&
           !s->execution.world_open && s->execution.current_lease == SIZE_MAX &&
           s->execution.world_count == 2 && s->execution.lease_count == 1 &&
           mc_world_access_receipt_retire(&s->receipt, NULL) == MC_WORLD_STORAGE_BUSY &&
           mc_world_access_free(s->receiver, NULL) == MC_WORLD_STORAGE_BUSY && actual == NULL &&
           ac_prefix_same(s, &source));
    ac_marker("get finished");
    if (!s->null_error)
        VERIFY(s->acquired.cause.status == s->callback_cause.status &&
               s->acquired.cause.operation == s->callback_cause.operation &&
               s->acquired.cause.system_error == s->callback_cause.system_error &&
               s->acquired.activation_bytecode == 42 &&
               s->acquired.activation_world_bytecode == 34);
    s->acquired.batch.publication.world_access_failure = (mc_world_access_failure){0};
    s->acquired.source_chunk = NULL;
    no_loader = (mc_entity_tick_execution_loader_view){0};
    saved = bad = (mc_world_runtime_execution_close){0};
    view = NULL;
    selected_world = NULL;
    s->result = (mc_world_runtime_execution_result){0};
    VERIFY(mc_entity_tick_execution_retire(&s->execution) == MC_WORLD_STORAGE_OK &&
           s->execution.world_count == 0 && s->execution.lease_count == 0 &&
           s->execution.receipt_count == 0 && world->persistent_bindings + 2 == s->saved_bindings &&
           mc_world_access_receipt_retire(&s->receipt, NULL) == MC_WORLD_STORAGE_OK &&
           mc_world_access_free(s->receiver, NULL) == MC_WORLD_STORAGE_OK &&
           ac_prefix_same(s, &source));
    s->receiver = NULL;
    s->node = NULL;
    ac_marker("owner retired");
    return 0;
}

int main(int argc, char **argv) {
    VERIFY(argc == 3 && (strcmp(argv[1], "error") == 0 || strcmp(argv[1], "null-error") == 0));
    printf("ACTIVATION_CLOSE_BEGIN schema=1 mode=%s\n", argv[1]);
    fflush(stdout);
    ac_fixture *s = calloc(1, sizeof(*s));
    VERIFY(s != NULL);
    s->mode = argv[1];
    s->null_error = strcmp(s->mode, "null-error") == 0;
    s->fault = s->source_throwable = true;
    s->callback_status = MC_WORLD_STORAGE_IO_ERROR;
    VERIFY(ss_setup(&s->source, argv[2]) == 0 && ac_files(s, true) == 0 &&
           ss_open(&s->source) == 0 && s->source.game.providers[0].cache->count == 0);
    mc_entity_tick_execution_init(&s->execution, NULL);
    VERIFY(ac_failure(s) == 0 &&
           mc_entity_tick_execution_retire(&s->execution) == MC_WORLD_STORAGE_OK);
    VERIFY(ss_stop(&s->source) == 0 && ac_files(s, false) == 0 && ss_finish(&s->source) == 0);
    for (size_t i = 0; i < COUNT(s->inputs); ++i)
        mc_ws_free_file_bytes(&s->inputs[i]);
    free(s);
    printf(
        "ACTIVATION_CLOSE_OPEN Tile-refusal/consumed-nonOK/direct-close-wrong-current/full-SL\n");
    printf("PASS: %u actual activation close-result checks;mode=%s\n", checks, argv[1]);
    return 0;
}
