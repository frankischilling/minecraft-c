/* Genuine source World/Store ownership; no provider or validity flag is forged. */
#include "mc/world/runtime/world_access.h"
#include "mob_nbt_source_support.inc"
#include "world/runtime/acquisition_internal.h"
#include <inttypes.h>

static const int32_t oc_x[] = {0, 2, 4, 7};
static const int32_t oc_z[] = {0, 2, -3, 7};

static int oc_observe(mc_world_runtime *world, mc_chunk_provider_cache_observation *out) {
    VERIFY(world != NULL && mc_chunk_provider_cache_observe(world->fields.v, out));
    return 0;
}

/* Keep the existing source-save Registry/File/Handler setup, while constructing
 * this World without the old fixture helper's deliberate raw cache export. */
static int oc_open(source_save_fixture *s, size_t budget) {
    fixture *f = &s->game;
    mc_world_info_name *folder = ss_name("world");
    VERIFY(folder != NULL &&
           mc_world_store_source_allocate(s->handlers, &f->stores[0], &s->source) ==
               MC_WORLD_SOURCE_OK &&
           mc_world_store_source_construct(f->stores[0], s->directory, folder, false, &s->source) ==
               MC_WORLD_SOURCE_OK);
    mc_world_info_name_release(folder);
    mc_world_runtime_construction_limits limits = mc_world_runtime_default_construction_limits();
    limits.max_provider_chunks = 8;
    limits.max_scheduled_entries = 0;
    if (budget != SIZE_MAX)
        limits.max_provider_allocation_bytes = budget;
    VERIFY(mc_world_provider_create(0, NULL, &f->dimensions[0], NULL) == MC_WORLD_PROVIDER_OK &&
           mc_world_runtime_allocate_world(MC_WORLD_RUNTIME_CLIENT_LOAD_OR_CREATE, f->classes[0],
                                           &limits, &f->worlds[0],
                                           &s->error) == MC_WORLD_STORAGE_OK);
    const mc_world_runtime_world_arguments arguments = {
        .store = f->stores[0], .name = f->name, .seed = -99, .dimension = f->dimensions[0]};
    VERIFY(mc_world_runtime_construct_world(f->worlds[0], &arguments, &s->error) ==
           MC_WORLD_STORAGE_OK);
    mc_world_runtime_construction state;
    mc_world_runtime_source_construction_state source;
    VERIFY(mc_world_runtime_construction_get(f->worlds[0], &state) == MC_WORLD_STORAGE_OK &&
           state.phase == MC_WORLD_RUNTIME_READY && state.contexts_bound &&
           state.cache_registered &&
           mc_world_runtime_source_construction_inspect(f->worlds[0], &source) &&
           source.metadata_called && source.metadata_returned && source.loader != NULL);
    mc_chunk_provider_cache_observation observed;
    VERIFY(oc_observe(f->worlds[0], &observed) == 0 && observed.map_count == 0 &&
           observed.insertion_count == 0 && observed.mutation_depth == 0 &&
           observed.raw_exports == 0);
    return 0;
}

static int oc_get(source_save_fixture *s, size_t coordinate, mc_chunk_state **out) {
    mc_world_runtime_acquisition_error error;
    VERIFY(coordinate < 4 &&
           mc_world_runtime_get_detailed(s->game.worlds[0], oc_x[coordinate], oc_z[coordinate], out,
                                         &error) == MC_CHUNK_ACQUIRE_OK &&
           error.status == MC_CHUNK_ACQUIRE_OK && *out != NULL && (*out)->active &&
           mc_world_store_owns_chunk(s->game.stores[0], *out));
    return 0;
}

static int oc_stop(source_save_fixture *s) {
    fixture *f = &s->game;
    mc_chunk_provider_fields raw = {0};
    VERIFY(mc_chunk_provider_bind_fields(f->worlds[0]->fields.v, &raw, &s->error) ==
               MC_WORLD_STORAGE_OK &&
           mc_world_runtime_retire_entity_containers(f->worlds[0], &s->error) ==
               MC_WORLD_STORAGE_OK &&
           mc_chunk_pending_clear(raw.pending) == MC_CHUNK_ACQUIRE_OK &&
           mc_chunk_cache_init(raw.cache, raw.cache->entries, raw.cache->capacity,
                               raw.cache->insertions, raw.cache->insertion_capacity) &&
           mc_world_runtime_retire_world(f->worlds[0], &s->error) == MC_WORLD_STORAGE_OK);
    while (mc_world_store_chunk_count(f->stores[0]) != 0) {
        const size_t before = mc_world_store_chunk_count(f->stores[0]);
        mc_chunk_state *state = NULL;
        VERIFY(mc_world_store_chunk_at(f->stores[0], 0, &state, &s->error) == MC_WORLD_STORAGE_OK &&
               state != NULL &&
               mc_world_store_retire_chunk(f->stores[0], state, &s->error) == MC_WORLD_STORAGE_OK &&
               mc_world_store_chunk_count(f->stores[0]) == before - 1);
    }
    VERIFY(mc_world_runtime_free(f->worlds[0], &s->error) == MC_WORLD_STORAGE_OK);
    f->worlds[0] = NULL;
    mc_world_provider_release(f->dimensions[0]);
    f->dimensions[0] = NULL;
    VERIFY(mc_world_store_source_release(&f->stores[0], &s->source) == MC_WORLD_SOURCE_OK &&
           f->stores[0] == NULL &&
           mc_world_source_regions_flush(s->source_regions, &s->region) == MC_WORLD_SOURCE_OK);
    return 0;
}

static int oc_owned(source_save_fixture *s) {
    mc_world_runtime *world = s->game.worlds[0];
    mc_chunk_provider_cache_observation initial, loaded, hit, after;
    mc_chunk_state *states[4] = {0}, *got = NULL;
    VERIFY(oc_observe(world, &initial) == 0 && initial.owned && initial.index_capacity >= 16 &&
           initial.index_bytes > 0 && initial.rebuilds == 1 && oc_get(s, 0, &states[0]) == 0 &&
           oc_observe(world, &loaded) == 0 && loaded.owned && loaded.map_count == 1 &&
           loaded.insertion_count == 1 && loaded.generic_gets > initial.generic_gets &&
           loaded.misses > initial.misses && loaded.rebuilds > initial.rebuilds &&
           loaded.mutation_depth == 0 && oc_get(s, 0, &got) == 0 && got == states[0] &&
           oc_observe(world, &hit) == 0 && hit.hits == loaded.hits + 1 &&
           hit.generic_gets == loaded.generic_gets && hit.rebuilds == loaded.rebuilds);
    mc_world_runtime_acquisition_error error;
    VERIFY(mc_chunk_cache_key(0, 0) == mc_chunk_cache_key(32768, 0) &&
           mc_world_runtime_get_detailed(world, 32768, 0, &got, &error) == MC_CHUNK_ACQUIRE_OK &&
           got == states[0] && oc_get(s, 1, &states[1]) == 0 && oc_get(s, 2, &states[2]) == 0 &&
           states[0] != states[1] && states[1] != states[2] && states[0] != states[2]);
    VERIFY(oc_observe(world, &after) == 0 && after.owned && after.map_count == 3 &&
           after.insertion_count == 3 && after.hits >= hit.hits + 1 && after.mutation_depth == 0);
    mc_chunk_provider_fields raw = {0};
    VERIFY(mc_chunk_provider_bind_fields(world->fields.v, &raw, &s->error) == MC_WORLD_STORAGE_OK &&
           mc_chunk_cache_validate(raw.cache) && oc_observe(world, &after) == 0 && !after.owned &&
           raw.direct_allocation_bytes == after.direct_bytes);
    for (size_t i = 0; i < 3; ++i) {
        VERIFY(raw.cache->entries[i].key == mc_chunk_cache_key(oc_x[i], oc_z[i]) &&
               raw.cache->entries[i].chunk == states[i] && raw.cache->insertions[i] == states[i] &&
               mc_chunk_acquire_get(mc_world_runtime_acquisition(world), oc_x[i], oc_z[i], &got) ==
                   MC_CHUNK_ACQUIRE_OK &&
               got == states[i]);
    }
    VERIFY(mc_chunk_cache_publish(raw.cache, 32768, 0, states[1]) && raw.cache->count == 3 &&
           raw.cache->insertion_count == 4 && raw.cache->insertions[3] == states[1] &&
           oc_get(s, 0, &got) == 0 && got == states[1] &&
           mc_chunk_cache_publish(raw.cache, 0, 0, states[0]) && raw.cache->insertion_count == 5 &&
           raw.cache->insertions[4] == states[0] && oc_get(s, 0, &got) == 0 && got == states[0] &&
           oc_get(s, 3, &states[3]) == 0 && oc_observe(world, &after) == 0 && !after.owned &&
           after.index_capacity == 0 && after.index_bytes == 0 && after.map_count == 4 &&
           after.insertion_count == 6);
    puts("OWNED_CACHE_CASE mode=owned hit=1 miss=1 packed-alias=1 exact-identities=1 "
         "insertion-order=1 generic-equal=1 raw-publish-visible=1");
    return 0;
}

typedef struct oc_progress {
    mc_chunk_cache *cache;
    size_t calls;
    mc_world_save_stage stage;
} oc_progress;

static mc_chunk_callback_result
oc_capture_progress(void *owner, const mc_world_save_context *context, mc_world_save_stage stage) {
    oc_progress *capture = owner;
    ++capture->calls;
    capture->stage = stage;
    capture->cache = context->chunks->cache;
    return MC_CHUNK_CALLBACK_FATAL; /* Before session/metadata/auxiliary/Chunk writes. */
}

static int oc_export(source_save_fixture *s, const char *mode) {
    mc_world_runtime *world = s->game.worlds[0];
    mc_chunk_state *states[2] = {0}, *got = NULL;
    mc_chunk_provider_cache_observation before, exported, after;
    VERIFY(oc_get(s, 0, &states[0]) == 0 && oc_get(s, 1, &states[1]) == 0 &&
           oc_observe(world, &before) == 0 && before.owned);
    mc_chunk_cache *escaped = NULL;
    mc_chunk_provider_fields raw = {0};
    if (strcmp(mode, "provider") == 0) {
        VERIFY(mc_chunk_provider_bind_fields(world->fields.v, &raw, &s->error) ==
               MC_WORLD_STORAGE_OK);
        escaped = raw.cache;
    } else if (strcmp(mode, "acquisition") == 0)
        escaped = mc_world_runtime_acquisition(world)->cache;
    else if (strcmp(mode, "lifecycle") == 0)
        escaped = mc_world_runtime_lifecycle(world)->cache;
    else if (strcmp(mode, "saving") == 0)
        escaped = mc_world_runtime_saving(world)->chunks->cache;
    else if (strcmp(mode, "get") == 0 || strcmp(mode, "acquire") == 0) {
        VERIFY((strcmp(mode, "get") == 0
                    ? mc_world_runtime_get(world, 0, 0, &got)
                    : mc_world_runtime_acquire(world, 0, 0, &got)) == MC_CHUNK_ACQUIRE_OK &&
               got == states[0]);
    } else if (strcmp(mode, "progress") == 0) {
        oc_progress progress = {0};
        VERIFY(mc_world_runtime_save_with_progress_detailed(world, false, &progress,
                                                            oc_capture_progress, &s->lifecycle) ==
                   MC_CHUNK_LIFECYCLE_CALLBACK_FAILED &&
               progress.calls == 1 && progress.stage == MC_WORLD_SAVE_LEVEL &&
               progress.cache != NULL);
        escaped = progress.cache;
    } else
        VERIFY(false);
    VERIFY(oc_observe(world, &exported) == 0 && !exported.owned && exported.index_capacity == 0 &&
           exported.index_bytes == 0 && exported.raw_exports > before.raw_exports);
    if (strcmp(mode, "provider") == 0)
        VERIFY(raw.direct_allocation_bytes == exported.direct_bytes);
    if (escaped == NULL) {
        VERIFY(mc_chunk_provider_bind_fields(world->fields.v, &raw, &s->error) ==
               MC_WORLD_STORAGE_OK);
        escaped = raw.cache;
    }
    VERIFY(escaped->count == 2 && escaped->insertion_count == 2 &&
           mc_chunk_cache_publish(escaped, 32768, 0, states[1]) && escaped->insertion_count == 3 &&
           escaped->insertions[2] == states[1] && oc_get(s, 0, &got) == 0 && got == states[1]);
    /* The exported descriptor is genuinely caller-mutable. Verify the original
     * defensive generic rejection and restore this one authored edit at once. */
    const int32_t key = escaped->entries[1].key;
    escaped->entries[1].key = escaped->entries[0].key;
    got = states[0];
    mc_world_runtime_acquisition_error detail;
    const mc_chunk_acquire_status invalid =
        mc_world_runtime_get_detailed(world, 0, 0, &got, &detail);
    escaped->entries[1].key = key;
    VERIFY(invalid == MC_CHUNK_ACQUIRE_INVALID_ARGUMENT && got == states[0] &&
           detail.status == invalid && mc_chunk_cache_validate(escaped) &&
           mc_chunk_cache_publish(escaped, 0, 0, states[0]) && oc_get(s, 0, &got) == 0 &&
           got == states[0] && oc_observe(world, &after) == 0 && !after.owned &&
           after.index_capacity == 0 && after.hits == exported.hits &&
           after.rebuilds == exported.rebuilds && after.generic_gets >= exported.generic_gets + 3);
    printf("OWNED_CACHE_CASE mode=%s irreversible-export=1 retained-alias=1 mutation-visible=1 "
           "invalid-generic=1 output-preserved=1 no-rebuild=1\n",
           mode);
    return 0;
}

typedef struct oc_callback_state {
    source_save_fixture *source;
    mc_world_access *access;
    mc_chunk_state *outer, *nested;
    size_t calls;
    bool export_inside, failed;
} oc_callback_state;

static bool oc_callback_check(bool value, const char *expression, int line) {
    ++checks;
    if (!value)
        fprintf(stderr, "owned cache callback line%d check%u: %s\n", line, checks, expression);
    return value;
}

#define OC_CALLBACK(x) oc_callback_check((x), #x, __LINE__)

static mc_world_storage_status oc_added(void *owner, mc_world_runtime *world, mc_entity *entity,
                                        mc_world_storage_error *error) {
    oc_callback_state *state = owner;
    mc_chunk_provider_cache_observation before = {0}, after = {0};
    mc_world_runtime_acquisition_error detail;
    mc_world_storage_error blocked;
    ++state->calls;
    bool ok = OC_CALLBACK(
        state->calls == 1 && entity != NULL && world == state->source->game.worlds[0] &&
        mc_chunk_provider_cache_observe(world->fields.v, &before) && before.owned &&
        before.mutation_depth > 0 &&
        mc_world_runtime_free(world, &blocked) == MC_WORLD_STORAGE_BUSY &&
        mc_world_access_free(state->access, &blocked) == MC_WORLD_STORAGE_BUSY &&
        mc_world_runtime_get_detailed(world, 0, 0, &state->outer, &detail) == MC_CHUNK_ACQUIRE_OK &&
        state->outer != NULL);
    if (ok && state->export_inside) {
        const mc_chunk_acquire_context *escaped = mc_world_runtime_acquisition(world);
        ok = OC_CALLBACK(escaped != NULL && escaped->cache != NULL);
    }
    if (ok)
        ok = OC_CALLBACK(
            mc_world_runtime_get_detailed(world, 2, 2, &state->nested, &detail) ==
                MC_CHUNK_ACQUIRE_OK &&
            state->nested != NULL && state->nested != state->outer &&
            mc_chunk_provider_cache_observe(world->fields.v, &after) && after.hits == before.hits &&
            after.generic_gets >= before.generic_gets + 2 && after.rebuilds == before.rebuilds &&
            (state->export_inside ? !after.owned && after.index_capacity == 0
                                  : after.owned && after.mutation_depth == before.mutation_depth));
    if (!ok) {
        state->failed = true;
        *error = (mc_world_storage_error){MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                          "owned cache real entity callback control", 0};
        return error->status;
    }
    *error = (mc_world_storage_error){0};
    return MC_WORLD_STORAGE_OK;
}

static int oc_callback(source_save_fixture *s, bool export_inside) {
    mc_world_runtime *world = s->game.worlds[0];
    oc_callback_state state = {.source = s, .export_inside = export_inside};
    const mc_world_access_callbacks callbacks = {.entity_added = oc_added};
    VERIFY(mc_world_access_create(&callbacks, &state, &state.access, &s->error) ==
               MC_WORLD_STORAGE_OK &&
           mc_world_runtime_add_world_access(world, state.access, &s->error) ==
               MC_WORLD_STORAGE_OK);
    mc_chunk_provider_cache_observation before, after, hit;
    mc_chunk_state *got = NULL;
    VERIFY(oc_observe(world, &before) == 0 && before.owned && oc_get(s, 0, &got) == 0 &&
           !state.failed && state.calls == 1 && got == state.outer &&
           oc_observe(world, &after) == 0 && after.map_count == 2 && after.insertion_count == 2 &&
           after.mutation_depth == 0 &&
           (export_inside
                ? !after.owned && after.index_capacity == 0 && after.rebuilds == before.rebuilds
                : after.owned && after.rebuilds == before.rebuilds + 1));
    VERIFY(
        oc_get(s, 1, &got) == 0 && got == state.nested && oc_observe(world, &hit) == 0 &&
        (export_inside
             ? !hit.owned && hit.hits == after.hits && hit.generic_gets == after.generic_gets + 1
             : hit.owned && hit.hits == after.hits + 1 && hit.generic_gets == after.generic_gets));
    bool removed = false;
    VERIFY(mc_world_runtime_remove_world_access(world, state.access, &removed, &s->error) ==
               MC_WORLD_STORAGE_OK &&
           removed && mc_world_access_free(state.access, &s->error) == MC_WORLD_STORAGE_OK);
    printf("OWNED_CACHE_CASE mode=%s real-entity-callback=1 nested-generic=1 outer-rebuild=%d "
           "callback-export-permanent=%d busy-lifetime=1\n",
           export_inside ? "callback-export" : "callback", !export_inside, export_inside);
    return 0;
}

static int oc_budget(source_save_fixture *s) {
    mc_chunk_provider_cache_observation initial, absent, after;
    VERIFY(oc_observe(s->game.worlds[0], &initial) == 0 && initial.owned &&
           initial.index_bytes > 0 && initial.direct_bytes > initial.index_bytes);
    const size_t base = initial.direct_bytes - initial.index_bytes;
    VERIFY(oc_stop(s) == 0 && oc_open(s, base) == 0 &&
           oc_observe(s->game.worlds[0], &absent) == 0 && !absent.owned &&
           absent.index_capacity == 0 && absent.index_bytes == 0 && absent.direct_bytes == base &&
           absent.raw_exports == 0);
    mc_chunk_state *first = NULL, *again = NULL;
    VERIFY(oc_get(s, 0, &first) == 0 && oc_get(s, 0, &again) == 0 && again == first &&
           oc_observe(s->game.worlds[0], &after) == 0 && !after.owned && after.hits == 0 &&
           after.generic_gets >= absent.generic_gets + 2 && after.direct_bytes == base);
    printf("OWNED_CACHE_CASE mode=budget dynamic-base=%zu optional-bytes=%zu exact-base-success=1 "
           "generic-fallback=1 exact-identity=1\n",
           base, initial.index_bytes);
    return 0;
}

int main(int argc, char **argv) {
    VERIFY(argc == 3);
    const char *mode = argv[2];
    const bool owned = strcmp(mode, "owned") == 0, budget = strcmp(mode, "budget") == 0;
    const bool callback = strcmp(mode, "callback") == 0,
               callback_export = strcmp(mode, "callback-export") == 0;
    const bool escape = strcmp(mode, "provider") == 0 || strcmp(mode, "acquisition") == 0 ||
                        strcmp(mode, "lifecycle") == 0 || strcmp(mode, "saving") == 0 ||
                        strcmp(mode, "get") == 0 || strcmp(mode, "acquire") == 0 ||
                        strcmp(mode, "progress") == 0;
    VERIFY(owned || budget || callback || callback_export || escape);
    source_save_fixture *source = calloc(1, sizeof(*source));
    VERIFY(source != NULL && ss_setup(source, argv[1]) == 0 && oc_open(source, SIZE_MAX) == 0);
    const int result = owned                         ? oc_owned(source)
                       : budget                      ? oc_budget(source)
                       : callback || callback_export ? oc_callback(source, callback_export)
                                                     : oc_export(source, mode);
    VERIFY(result == 0 && oc_stop(source) == 0 && ss_finish(source) == 0);
    printf("OWNED_CACHE_FINISH mode=%s checks=%u cleanup=1 genuine-source-world=1\n", mode, checks);
    VERIFY(fflush(stdout) == 0);
    free(source);
    return 0;
}
