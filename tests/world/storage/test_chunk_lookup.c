/* Genuine source Handler/World helpers; the existing program is never run. */
#include "../runtime/saving/mob_nbt_source_support.inc"
#include "world/runtime/terminal_inventory_internal.h"
#include "world/storage/chunk_lookup_internal.h"
#include "world/storage/generated_internal.h"
#include <inttypes.h>

enum { SI_MAX = 512, SI_REUSE_ATTEMPTS = 32 };

typedef struct si_entry {
    mc_chunk_state *state;
    mc_world_chunk *object;
    bool generated;
} si_entry;

typedef struct si_work {
    source_save_fixture source;
    si_entry entries[SI_MAX];
    size_t count, owner_bound, cold_probes, pointer_reuses;
    bool linear, bulk;
    const char *mode;
    mc_stored_chunk *foreign;
} si_work;

static bool si_same(const mc_world_store_chunk_lookup_view *a,
                    const mc_world_store_chunk_lookup_view *b) {
    return a->indexed == b->indexed && a->disabled == b->disabled &&
           a->chunk_count == b->chunk_count && a->index_count == b->index_count &&
           a->capacity == b->capacity && a->index_bytes == b->index_bytes &&
           a->peak_bytes == b->peak_bytes && a->byte_limit == b->byte_limit &&
           a->base_owner_bytes == b->base_owner_bytes;
}

static int si_observe(si_work *w, mc_world_store_chunk_lookup_view *out) {
    VERIFY(
        mc_world_store_chunk_lookup_inspect(w->source.game.stores[0], out) &&
        out->chunk_count == w->count && out->base_owner_bytes <= w->owner_bound &&
        out->index_bytes <= out->peak_bytes && out->peak_bytes <= out->byte_limit &&
        out->byte_limit == (w->owner_bound - out->base_owner_bytes < 262144
                                ? w->owner_bound - out->base_owner_bytes
                                : 262144) &&
        (!out->indexed || (out->index_count == w->count && out->capacity >= 16 &&
                           out->index_count <= out->capacity / 2 && !out->disabled)) &&
        (out->indexed || (out->capacity == 0 && out->index_count == 0 && out->index_bytes == 0)));
    return 0;
}

static int si_budget(si_work *w) {
    source_save_fixture *s = &w->source;
    mc_world_store *probe = NULL;
    mc_world_store_chunk_lookup_view initial;
    VERIFY(mc_world_store_source_allocate(s->handlers, &probe, &s->source) == MC_WORLD_SOURCE_OK &&
           mc_world_store_chunk_lookup_inspect(probe, &initial) && !initial.indexed &&
           !initial.disabled && initial.chunk_count == 0 && initial.base_owner_bytes > 0 &&
           mc_world_store_source_release(&probe, &s->source) == MC_WORLD_SOURCE_OK &&
           probe == NULL);
    const mc_world_source_options defaults = mc_world_store_source_default_options();
    w->owner_bound = defaults.max_owner_bytes;
    if (w->linear) {
        VERIFY(mc_world_store_source_classes_retire(&s->handlers, &s->source) ==
               MC_WORLD_SOURCE_OK);
        mc_world_source_options options = defaults;
        options.max_owner_bytes = initial.base_owner_bytes;
        for (size_t attempt = 0; attempt < 257; ++attempt) {
            ++w->cold_probes;
            const mc_world_source_status result = mc_world_store_source_classes_create(
                s->files, s->logging, &options, &s->handlers, &s->source);
            if (result == MC_WORLD_SOURCE_OK)
                break;
            VERIFY(result == MC_WORLD_SOURCE_LIMIT && s->handlers == NULL &&
                   options.max_owner_bytes <= defaults.max_owner_bytes - 1024);
            options.max_owner_bytes += 1024;
        }
        VERIFY(s->handlers != NULL &&
               mc_world_store_source_classes_bind_regions(s->handlers, s->source_regions,
                                                          &s->source) == MC_WORLD_SOURCE_OK);
        w->owner_bound = options.max_owner_bytes;
    }
    return 0;
}

static int si_open(si_work *w) {
    source_save_fixture *s = &w->source;
    fixture *f = &s->game;
    mc_world_info_name *folder = ss_name("world");
    VERIFY(folder != NULL &&
           mc_world_store_source_allocate(s->handlers, &f->stores[0], &s->source) ==
               MC_WORLD_SOURCE_OK &&
           mc_world_store_source_construct(f->stores[0], s->directory, folder, false, &s->source) ==
               MC_WORLD_SOURCE_OK);
    mc_world_info_name_release(folder);
    mc_world_runtime_construction_limits limits = mc_world_runtime_default_construction_limits();
    limits.max_provider_chunks = 4;
    limits.max_scheduled_entries = 0;
    VERIFY(mc_world_provider_create(0, NULL, &f->dimensions[0], NULL) == MC_WORLD_PROVIDER_OK &&
           mc_world_runtime_allocate_world(MC_WORLD_RUNTIME_CLIENT_LOAD_OR_CREATE, f->classes[0],
                                           &limits, &f->worlds[0],
                                           &s->error) == MC_WORLD_STORAGE_OK);
    const mc_world_runtime_world_arguments arguments = {
        .store = f->stores[0], .name = f->name, .seed = -99, .dimension = f->dimensions[0]};
    VERIFY(mc_world_runtime_construct_world(f->worlds[0], &arguments, &s->error) ==
               MC_WORLD_STORAGE_OK &&
           mc_chunk_provider_bind_fields(f->worlds[0]->fields.v, &f->providers[0], &s->error) ==
               MC_WORLD_STORAGE_OK);
    mc_world_runtime_source_construction_state source;
    mc_world_runtime_construction construction;
    VERIFY(mc_world_runtime_source_construction_inspect(f->worlds[0], &source) &&
           source.metadata_called && source.metadata_returned && source.loader != NULL &&
           mc_world_runtime_construction_get(f->worlds[0], &construction) == MC_WORLD_STORAGE_OK &&
           construction.phase == MC_WORLD_RUNTIME_READY && construction.cache_registered);
    mc_chunk_state *state = NULL;
    mc_world_runtime_acquisition_error detail;
    VERIFY(mc_world_runtime_get_detailed(f->worlds[0], 0, 0, &state, &detail) ==
               MC_CHUNK_ACQUIRE_OK &&
           state != NULL && state->active && state->x == 0 && state->z == 0 &&
           mc_world_store_game_stage_count(f->stores[0]) == 0 &&
           f->providers[0].cache->insertion_count == 1 &&
           f->providers[0].cache->insertions[0] == state);
    mc_world_chunk *object = mc_world_store_chunk_object(f->stores[0], state);
    VERIFY(object != NULL && mc_world_store_generated_chunk(f->stores[0], state) == NULL);
    w->entries[w->count++] = (si_entry){state, object, false};
    mc_world_source_state handler;
    mc_world_store_chunk_lookup_view view;
    VERIFY(mc_world_store_source_inspect(f->stores[0], &handler) && handler.allocations == 2 &&
           handler.phase == MC_WORLD_SOURCE_READY && !handler.active && si_observe(w, &view) == 0);
    printf("STORE_INDEX_OWNER mode=%s max-owner=%zu base=%zu source-e=%" PRId64
           " fixed-prefix-allocations=%zu cold-probes=%zu actual-world=1 loaded-stored=1\n",
           w->mode, w->owner_bound, view.base_owner_bytes, handler.source_e, handler.allocations,
           w->cold_probes);
    return 0;
}

static int si_add_generated(si_work *w, int32_t x, int32_t z) {
    fixture *f = &w->source.game;
    uint8_t *blocks = calloc(MC_CHUNK_BLOCKS, 1);
    mc_world_chunk *object = NULL;
    VERIFY(blocks != NULL && w->count < SI_MAX);
    uint8_t *const original = blocks;
    const mc_world_storage_status result = mc_world_store_create_generated(
        f->stores[0], f->worlds[0], 0, x, z, &blocks, MC_CHUNK_BLOCKS, &object, &w->source.error);
    if (result != MC_WORLD_STORAGE_OK)
        free(blocks);
    VERIFY(result == MC_WORLD_STORAGE_OK && blocks == NULL && object != NULL &&
           object->data->blocks == original && object->d == f->worlds[0]);
    mc_chunk_state *state = mc_world_chunk_state(object);
    VERIFY(state != NULL && state->x == x && state->z == z && !state->active &&
           mc_world_store_generated_chunk(f->stores[0], state) == object &&
           mc_world_store_chunk_object(f->stores[0], state) == object);
    w->entries[w->count++] = (si_entry){state, object, true};
    return 0;
}

static int si_add_stored(si_work *w, const mc_chunk_state *input) {
    fixture *f = &w->source.game;
    mc_chunk_state *state = NULL;
    VERIFY(w->count < SI_MAX &&
           mc_world_store_retain_chunk(f->stores[0], 0, input, &state, &w->source.error) ==
               MC_WORLD_STORAGE_OK &&
           state != NULL && state != input && state->data != input->data &&
           state->data->blocks != input->data->blocks && state->x == input->x &&
           state->z == input->z && mc_world_store_chunk_object(f->stores[0], state) == NULL &&
           mc_world_store_generated_chunk(f->stores[0], state) == NULL);
    w->entries[w->count++] = (si_entry){state, NULL, false};
    return 0;
}

typedef struct si_visit {
    const si_work *work;
    size_t ordinal, visits;
    bool bad;
} si_visit;

static mc_world_storage_status si_visit_chunk(void *owner, const mc_world_chunk *chunk,
                                              mc_world_storage_error *error) {
    si_visit *v = owner;
    while (v->ordinal < v->work->count && v->work->entries[v->ordinal].object == NULL)
        ++v->ordinal;
    if (v->ordinal == v->work->count || v->work->entries[v->ordinal].object != chunk) {
        v->bad = true;
        if (error != NULL)
            *error = (mc_world_storage_error){MC_WORLD_STORAGE_INVALID_ARGUMENT,
                                              "fixture canonical visitor order", 0};
        return MC_WORLD_STORAGE_INVALID_ARGUMENT;
    }
    ++v->ordinal;
    ++v->visits;
    return MC_WORLD_STORAGE_OK;
}

static int si_queries(si_work *w) {
    mc_world_store *store = w->source.game.stores[0];
    mc_world_store_chunk_lookup_view before, after;
    VERIFY(si_observe(w, &before) == 0 && w->foreign != NULL);
    const mc_chunk_state *foreign = mc_stored_chunk_state(w->foreign);
    VERIFY(!mc_world_store_owns_chunk(store, foreign) &&
           mc_world_store_chunk_object(store, foreign) == NULL &&
           mc_world_store_generated_chunk(store, foreign) == NULL);
    size_t objects = 0;
    for (size_t round = 0; round < 3; ++round) {
        for (size_t i = 0; i < w->count; ++i) {
            const size_t ordinal = round == 1 ? w->count - 1 - i : i;
            si_entry *entry = &w->entries[ordinal];
            mc_chunk_state *actual = NULL;
            VERIFY(mc_world_store_chunk_at(store, ordinal, &actual, &w->source.error) ==
                       MC_WORLD_STORAGE_OK &&
                   actual == entry->state && mc_world_store_owns_chunk(store, actual) &&
                   mc_world_store_chunk_object(store, actual) == entry->object &&
                   mc_world_store_generated_chunk(store, actual) ==
                       (entry->generated ? entry->object : NULL));
            if (round == 0 && entry->object != NULL)
                ++objects;
        }
    }
    mc_chunk_state *sentinel = w->entries[0].state;
    VERIFY(mc_world_store_chunk_at(store, w->count, &sentinel, &w->source.error) ==
               MC_WORLD_STORAGE_INVALID_ARGUMENT &&
           sentinel == w->entries[0].state &&
           mc_world_store_lease_chunk(store, foreign, &w->source.error) ==
               MC_WORLD_STORAGE_INVALID_ARGUMENT &&
           mc_world_store_release_chunk(store, foreign, &w->source.error) ==
               MC_WORLD_STORAGE_INVALID_ARGUMENT);
    si_visit visits = {.work = w};
    VERIFY(mc_world_store_terminal_visit_chunks(store, w->source.game.worlds[0], si_visit_chunk,
                                                &visits, &w->source.error) == MC_WORLD_STORAGE_OK &&
           !visits.bad && visits.visits == objects && si_observe(w, &after) == 0 &&
           si_same(&before, &after));
    return 0;
}

static int si_busy(si_work *w) {
    fixture *f = &w->source.game;
    mc_world_store_chunk_lookup_view before, after;
    mc_world_source_state owner_before, owner_after;
    const size_t world_bytes = sizeof(*f->worlds[0]);
    const size_t error_bytes = sizeof(*owner_before.failure);
    unsigned char *snapshot = malloc(world_bytes + 2 * error_bytes);
    si_entry *entry = &w->entries[1];
    VERIFY(snapshot != NULL && entry->generated && si_observe(w, &before) == 0 &&
           mc_world_store_source_inspect(f->stores[0], &owner_before) &&
           owner_before.failure != NULL && owner_before.cleanup != NULL);
    memcpy(snapshot, f->worlds[0], world_bytes);
    memcpy(snapshot + world_bytes, owner_before.failure, error_bytes);
    memcpy(snapshot + world_bytes + error_bytes, owner_before.cleanup, error_bytes);
    VERIFY(mc_world_store_lease_chunk(f->stores[0], entry->state, &w->source.error) ==
           MC_WORLD_STORAGE_OK);
    si_visit visits = {.work = w};
    VERIFY(mc_world_store_retire_chunk(f->stores[0], entry->state, &w->source.error) ==
               MC_WORLD_STORAGE_BUSY &&
           mc_world_runtime_retire_entity_containers(f->worlds[0], &w->source.error) ==
               MC_WORLD_STORAGE_BUSY &&
           mc_world_store_terminal_visit_chunks(f->stores[0], f->worlds[0], si_visit_chunk, &visits,
                                                &w->source.error) == MC_WORLD_STORAGE_BUSY &&
           visits.visits == 0 &&
           mc_world_store_terminal_validate(f->stores[0], f->worlds[0], f->providers[0].cache,
                                            &f->worlds[0]->acquisition.fallback,
                                            &w->source.error) == MC_WORLD_STORAGE_BUSY &&
           si_observe(w, &after) == 0 && si_same(&before, &after) &&
           mc_world_store_source_inspect(f->stores[0], &owner_after) &&
           owner_after.phase == owner_before.phase && owner_after.holds == owner_before.holds &&
           owner_after.allocations == owner_before.allocations &&
           owner_after.source_e == owner_before.source_e &&
           owner_after.source_b == owner_before.source_b &&
           owner_after.source_c == owner_before.source_c &&
           owner_after.source_d == owner_before.source_d &&
           owner_after.pending_fos == owner_before.pending_fos &&
           owner_after.pending_dos == owner_before.pending_dos &&
           owner_after.last_store_pc == owner_before.last_store_pc &&
           memcmp(snapshot, f->worlds[0], world_bytes) == 0 &&
           memcmp(snapshot + world_bytes, owner_after.failure, error_bytes) == 0 &&
           memcmp(snapshot + world_bytes + error_bytes, owner_after.cleanup, error_bytes) == 0 &&
           !mc_world_chunk_entity_containers_retired(entry->object) &&
           mc_world_store_release_chunk(f->stores[0], entry->state, &w->source.error) ==
               MC_WORLD_STORAGE_OK &&
           mc_world_store_release_chunk(f->stores[0], entry->state, &w->source.error) ==
               MC_WORLD_STORAGE_INVALID_ARGUMENT);
    free(snapshot);
    VERIFY(si_queries(w) == 0);
    return 0;
}

static int si_reservation_fallback(si_work *w) {
    fixture *f = &w->source.game;
    mc_world_store_chunk_lookup_view before, after;
    mc_world_store_reservation *reservation = NULL;
    VERIFY(si_observe(w, &before) == 0 && mc_world_store_reservation_count(f->stores[0]) == 0 &&
           mc_world_store_reserve_chunk(f->stores[0], 0, &reservation, &w->source.error) ==
               MC_WORLD_STORAGE_OK &&
           reservation != NULL && mc_world_store_reservation_count(f->stores[0]) == 1 &&
           si_observe(w, &after) == 0 && si_same(&before, &after) &&
           mc_world_store_terminal_validate(f->stores[0], f->worlds[0], f->providers[0].cache,
                                            &f->worlds[0]->acquisition.fallback,
                                            &w->source.error) == MC_WORLD_STORAGE_BUSY &&
           mc_world_store_cancel_reservation(f->stores[0], reservation, &w->source.error) ==
               MC_WORLD_STORAGE_OK &&
           mc_world_store_reservation_count(f->stores[0]) == 0 && si_observe(w, &after) == 0 &&
           si_same(&before, &after));
    mc_chunk_cache_entry entries[2];
    mc_chunk_state *insertions[2];
    mc_chunk_cache cache;
    mc_chunk_state *fallback = w->entries[1].state;
    VERIFY(mc_chunk_cache_init(&cache, entries, COUNT(entries), insertions, COUNT(insertions)) &&
           mc_world_store_register_cache(f->stores[0], &cache, &fallback, &w->source.error) ==
               MC_WORLD_STORAGE_OK &&
           mc_world_store_retire_chunk(f->stores[0], fallback, &w->source.error) ==
               MC_WORLD_STORAGE_BUSY &&
           mc_world_store_terminal_validate(f->stores[0], f->worlds[0], f->providers[0].cache,
                                            &f->worlds[0]->acquisition.fallback,
                                            &w->source.error) == MC_WORLD_STORAGE_BUSY &&
           mc_world_store_unregister_cache(f->stores[0], &cache, &w->source.error) ==
               MC_WORLD_STORAGE_OK &&
           si_observe(w, &after) == 0 && si_same(&before, &after));
    return 0;
}

static int si_remove(si_work *w, size_t ordinal) {
    VERIFY(ordinal > 0 && ordinal < w->count &&
           mc_world_store_retire_chunk(w->source.game.stores[0], w->entries[ordinal].state,
                                       &w->source.error) == MC_WORLD_STORAGE_OK);
    memmove(w->entries + ordinal, w->entries + ordinal + 1,
            (w->count - ordinal - 1) * sizeof(w->entries[0]));
    --w->count;
    w->entries[w->count] = (si_entry){0};
    return 0;
}

static int si_cycle(si_work *w) {
    VERIFY(si_remove(w, 3) == 0 && si_queries(w) == 0 && si_remove(w, w->count - 1) == 0 &&
           si_queries(w) == 0);
    /* Real allocator behavior only. Integer identities are saved before free;
     * no stale pointer is dereferenced, passed to an API, or forced to recur. */
    for (size_t i = 0; i < SI_REUSE_ATTEMPTS; ++i) {
        VERIFY(si_add_stored(w, w->entries[1].state) == 0);
        const uintptr_t previous = (uintptr_t)w->entries[w->count - 1].state;
        VERIFY(si_remove(w, w->count - 1) == 0 && si_add_stored(w, w->entries[1].state) == 0);
        w->pointer_reuses += previous == (uintptr_t)w->entries[w->count - 1].state;
        VERIFY(si_queries(w) == 0 && si_remove(w, w->count - 1) == 0);
    }
    return 0;
}

static int si_partial(si_work *w) {
    fixture *f = &w->source.game;
    mc_chunk_state *out = w->entries[0].state;
    mc_world_runtime_acquisition_error detail;
    const size_t before_stages = mc_world_store_game_stage_count(f->stores[0]);
    VERIFY(before_stages == 0 &&
           mc_world_runtime_get_detailed(f->worlds[0], 1, 0, &out, &detail) ==
               MC_CHUNK_ACQUIRE_CALLBACK_FAILED &&
           out == w->entries[0].state && detail.load_bytecode == 158 &&
           detail.cause.status == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
           mc_world_store_game_stage_count(f->stores[0]) == 1 &&
           !mc_chunk_cache_contains(f->providers[0].cache, 1, 0));
    mc_world_store_game_stage *stage = NULL;
    mc_world_store_game_stage_view view;
    VERIFY(mc_world_store_game_stage_at(f->stores[0], 0, &stage, &view, &w->source.error) ==
               MC_WORLD_STORAGE_OK &&
           stage != NULL && view.chunk != NULL && view.state != NULL &&
           view.phase == MC_WORLD_STORE_GAME_FAILED && view.state->x == 2 && view.state->z == 0 &&
           !view.state->active && !mc_world_store_chunk_source_enabled(f->stores[0], view.state));
    w->entries[w->count++] = (si_entry){view.state, view.chunk, false};
    VERIFY(si_queries(w) == 0 &&
           mc_world_store_lease_chunk(f->stores[0], view.state, &w->source.error) ==
               MC_WORLD_STORAGE_OK &&
           mc_world_store_release_chunk(f->stores[0], view.state, &w->source.error) ==
               MC_WORLD_STORAGE_OK);
    return 0;
}

static int si_stop(si_work *w) {
    source_save_fixture *s = &w->source;
    fixture *f = &s->game;
    mc_stored_chunk_free(w->foreign);
    w->foreign = NULL;
    VERIFY(mc_world_runtime_retire_entity_containers(f->worlds[0], &s->error) ==
               MC_WORLD_STORAGE_OK &&
           mc_chunk_pending_clear(f->providers[0].pending) == MC_CHUNK_ACQUIRE_OK &&
           mc_chunk_cache_init(f->providers[0].cache, f->providers[0].cache->entries,
                               f->providers[0].cache->capacity, f->providers[0].cache->insertions,
                               f->providers[0].cache->insertion_capacity) &&
           mc_world_runtime_retire_world(f->worlds[0], &s->error) == MC_WORLD_STORAGE_OK);
    mc_world_store_chunk_lookup_view before;
    VERIFY(si_observe(w, &before) == 0);
    size_t ordinal = 0;
    while (ordinal < w->count) {
        if (w->bulk && w->entries[ordinal].object == NULL) {
            ++ordinal;
            continue;
        }
        mc_chunk_state *first = NULL;
        VERIFY(mc_world_store_chunk_at(f->stores[0], ordinal, &first, &s->error) ==
                   MC_WORLD_STORAGE_OK &&
               first == w->entries[ordinal].state &&
               mc_world_store_retire_chunk(f->stores[0], first, &s->error) == MC_WORLD_STORAGE_OK);
        --w->count;
        memmove(w->entries + ordinal, w->entries + ordinal + 1,
                (w->count - ordinal) * sizeof(w->entries[0]));
    }
    mc_world_store_chunk_lookup_view empty;
    VERIFY(si_observe(w, &empty) == 0 && empty.disabled == w->linear &&
           empty.peak_bytes == before.peak_bytes &&
           (w->bulk ? w->count >= 8 && empty.indexed && empty.index_count == w->count
                    : w->count == 0 && !empty.indexed && empty.index_bytes == 0 &&
                          empty.index_count == 0) &&
           mc_world_runtime_free(f->worlds[0], &s->error) == MC_WORLD_STORAGE_OK);
    f->worlds[0] = NULL;
    mc_world_provider_release(f->dimensions[0]);
    f->dimensions[0] = NULL;
    VERIFY(mc_world_store_source_release(&f->stores[0], &s->source) == MC_WORLD_SOURCE_OK &&
           f->stores[0] == NULL &&
           mc_world_source_regions_flush(s->source_regions, &s->region) == MC_WORLD_SOURCE_OK);
    printf("STORE_INDEX_RETIRE mode=%s explicit-empty=%d bulk-retained=%zu bulk-indexed=%d"
           " historical-peak=%zu source-final-released=1\n",
           w->mode, !w->bulk, w->count, empty.indexed, empty.peak_bytes);
    w->count = 0;
    return 0;
}

int main(int argc, char **argv) {
    VERIFY(argc == 3 && (strcmp(argv[2], "indexed") == 0 || strcmp(argv[2], "linear") == 0 ||
                         strcmp(argv[2], "indexed-bulk") == 0));
    si_work *w = calloc(1, sizeof(*w));
    VERIFY(w != NULL);
    w->linear = strcmp(argv[2], "linear") == 0;
    w->bulk = strcmp(argv[2], "indexed-bulk") == 0;
    w->mode = argv[2];
    VERIFY(ss_setup(&w->source, argv[1]) == 0 && si_budget(w) == 0 && si_open(w) == 0 &&
           si_add_generated(w, 23, -7) == 0 && si_add_stored(w, w->entries[1].state) == 0 &&
           mc_stored_chunk_create(w->entries[1].state, NULL, &w->foreign, NULL) ==
               MC_STORED_CHUNK_OK &&
           si_reservation_fallback(w) == 0);
    mc_world_store_chunk_lookup_view view;
    do {
        VERIFY(si_observe(w, &view) == 0);
        if ((w->linear && view.disabled) || (!w->linear && w->count >= 20))
            break;
        VERIFY(w->count < SI_MAX - 2 && si_add_generated(w, (int32_t)w->count + 100, -7) == 0 &&
               si_add_stored(w, w->entries[w->count - 1].state) == 0);
    } while (true);
    VERIFY(si_observe(w, &view) == 0 &&
           (w->linear ? view.disabled && !view.indexed : view.indexed && !view.disabled) &&
           si_queries(w) == 0 && si_reservation_fallback(w) == 0 && si_busy(w) == 0 &&
           si_cycle(w) == 0 && si_partial(w) == 0 && si_observe(w, &view) == 0 &&
           (w->linear ? view.disabled && !view.indexed : view.indexed && !view.disabled));
    mc_world_source_state source;
    VERIFY(mc_world_store_source_inspect(w->source.game.stores[0], &source) &&
           source.allocations == 2);
    printf(
        "STORE_INDEX_CASE mode=%s retained=%zu indexed=%d disabled=%d capacity=%zu live-bytes=%zu"
        " peak-bytes=%zu optional-limit=%zu base-owner=%zu max-owner=%zu current-remaining=%zu"
        " fixed-prefix-allocations=%zu pointer-reuses=%zu reuse-attempts=%u loaded-stored=1 "
        "generated=1"
        " copied-stored=1 coordinate-alias=1 canonical-visits=1 passive-queries=1 busy-unchanged=1"
        " removal-order=1 failed-attached=1 no-clock-or-source-fields=1\n",
        argv[2], w->count, view.indexed, view.disabled, view.capacity, view.index_bytes,
        view.peak_bytes, view.byte_limit, view.base_owner_bytes, w->owner_bound,
        w->owner_bound - view.base_owner_bytes - view.index_bytes, source.allocations,
        w->pointer_reuses, (unsigned)SI_REUSE_ATTEMPTS);
    VERIFY(si_stop(w) == 0 && ss_finish(&w->source) == 0);
    printf("STORE_INDEX_FINISH mode=%s cleanup=1 source-files-closed=1 checks=%u\n", argv[2],
           checks);
    free(w);
    return 0;
}
