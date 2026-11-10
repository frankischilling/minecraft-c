/* Reuse the actual SourceSaving constructors and checked terminal helpers.
 * The renamed program is never executed. Cold external graph ownership remains
 * with this fixture; these controls do not claim a complete Arrow composition. */
int bs_unused_query_main(int, char **);
#define MC_WORLD_COLLISION_QUERY_MAIN bs_unused_query_main
#include "test_collision_query_execution.c"
#undef MC_WORLD_COLLISION_QUERY_MAIN

#include "world/chunk/object_state_internal.h"
#include "world/runtime/blocks_execution_internal.h"

typedef enum bs_kind { BS_BLOCK, BS_METADATA, BS_SET } bs_kind;

typedef struct bs_fixture {
    cq_fixture cq;
    mc_world_block_execution frame, probe;
    mc_world_runtime_execution_result result, probe_result;
    mc_world_storage_error error, probe_error;
    mc_world_storage_status raw, probe_raw;
    mc_entity_tick_execution_world_token outer, descendant;
    size_t calls, probes, callbacks, root_loader;
    uint64_t reservation_generation;
    bool held, callback_valid, projected, leave_descendant;
} bs_fixture;

static const mc_entity_tick_execution_site bs_site = {"native.scalar.control", "read", "()V", 0};
static const mc_world_storage_error bs_error_marker = {MC_WORLD_STORAGE_IO_ERROR,
                                                       "untouched scalar caller error", 193};

static bool bs_error_same(mc_world_storage_error a, mc_world_storage_error b) {
    return a.status == b.status && a.operation == b.operation && a.system_error == b.system_error;
}

static bool bs_attempt_same(mc_entity_tick_execution_loader_attempt a,
                            mc_entity_tick_execution_loader_attempt b) {
    return cq_token_same(a.world, b.world) && a.slot == b.slot && a.generation == b.generation;
}

static bool bs_selection_same(mc_entity_tick_execution_loader_selection a,
                              mc_entity_tick_execution_loader_selection b) {
    return cq_token_same(a.world, b.world) && a.generation == b.generation && a.status == b.status;
}

static bool bs_records_same(const mc_world_runtime_execution_result *a,
                            const mc_world_runtime_execution_result *b) {
    if (a->selection_ready != b->selection_ready || a->root_pending != b->root_pending ||
        (a->selection_ready && !bs_selection_same(a->selection, b->selection)) ||
        (a->root_pending && !bs_attempt_same(a->root_attempt, b->root_attempt)))
        return false;
    for (size_t i = 0; i < MC_ENTITY_TICK_EXECUTION_LOADERS; ++i) {
        const mc_world_runtime_execution_pending *x = &a->pending[i], *y = &b->pending[i];
        if (x->used != y->used)
            return false;
        if (x->used &&
            (x->outcome_valid != y->outcome_valid || x->child_ready != y->child_ready ||
             x->child_pending != y->child_pending || !bs_attempt_same(x->attempt, y->attempt) ||
             x->actual_status != y->actual_status || x->direct_failure != y->direct_failure ||
             (x->child_ready && !bs_selection_same(x->child, y->child)) ||
             (x->child_pending && !bs_attempt_same(x->child_attempt, y->child_attempt))))
            return false;
    }
    for (size_t i = 0; i < MC_ENTITY_TICK_EXECUTION_WORLDS; ++i) {
        const mc_world_runtime_execution_close *x = &a->closes[i], *y = &b->closes[i];
        if (x->used != y->used)
            return false;
        if (x->used &&
            (x->child_failed != y->child_failed || x->kind != y->kind || x->phase != y->phase ||
             !cq_token_same(x->world, y->world) || x->source_status != y->source_status ||
             x->refused_status != y->refused_status || x->operation != y->operation ||
             x->site.class_name != y->site.class_name || x->site.method != y->site.method ||
             x->site.descriptor != y->site.descriptor || x->site.pc != y->site.pc))
            return false;
    }
    return true;
}

static bool bs_call_frame(bs_fixture *s, mc_world_block_execution *frame, bs_kind kind,
                          mc_world_runtime *world, int32_t x, int32_t y, int32_t z, int32_t value,
                          int32_t *out, bool *answer, mc_world_storage_error *error,
                          mc_world_storage_status *raw) {
    ++s->calls;
    if (kind == BS_BLOCK)
        return mc_world_runtime_block_get_in_execution(frame, world, x, y, z, out, error, raw);
    if (kind == BS_METADATA)
        return mc_world_runtime_metadata_get_in_execution(frame, world, x, y, z, out, error, raw);
    return mc_world_runtime_metadata_set_quiet_in_execution(frame, world, x, y, z, value, answer,
                                                            error, raw);
}

static int bs_reenter(bs_fixture *s, bs_kind kind) {
    /* All retained records are ordinary VALUE continuations, never owning nodes.
     * This snapshot adds no loader, World or receipt occurrence. */
    mc_world_runtime_execution_result *saved = malloc(sizeof(*saved));
    VERIFY(saved != NULL && s->frame.entered && !s->frame.completed);
    *saved = s->result;
    const size_t callbacks = s->callbacks;
    const mc_world_storage_error source_error = s->error;
    const mc_world_storage_status source_raw = s->raw;
    const mc_world_metadata_source_error source = s->frame.source;
    const mc_chunk_acquire_status get_status = s->frame.get_status;
    const bool get_entered = s->frame.get_entered, get_returned = s->frame.get_returned;
    const bool native_stopped = s->frame.native_stopped;
    int32_t out = 12345;
    bool answer = true;
    s->probe_error = bs_error_marker;
    s->probe_raw = MC_WORLD_STORAGE_LIMIT;
    const bool completed = bs_call_frame(s, &s->frame, kind, s->cq.source.game.worlds[0], 8, 64, 8,
                                         9, &out, &answer, &s->probe_error, &s->probe_raw);
    const bool same =
        !completed && out == 12345 && answer && bs_error_same(s->probe_error, bs_error_marker) &&
        s->probe_raw == MC_WORLD_STORAGE_LIMIT && bs_error_same(s->error, source_error) &&
        s->raw == source_raw && s->frame.entered && !s->frame.completed &&
        s->frame.get_entered == get_entered && s->frame.get_returned == get_returned &&
        s->frame.get_status == get_status && s->frame.native_stopped == native_stopped &&
        s->frame.source.source_exception == source.source_exception &&
        s->frame.source.exception_class == source.exception_class &&
        s->frame.source.world_pc == source.world_pc &&
        s->frame.source.chunk_pc == source.chunk_pc &&
        s->frame.source.nibble_pc == source.nibble_pc && s->callbacks == callbacks &&
        bs_records_same(&s->result, saved) &&
        (saved->ownership_status == MC_WORLD_STORAGE_OK
             ? s->result.ownership_status == MC_WORLD_STORAGE_INVALID_ARGUMENT
             : s->result.ownership_status == saved->ownership_status &&
                   cq_owner_error_same(s->result.ownership_error, saved->ownership_error));
    free(saved);
    VERIFY(same);
    return 0;
}

static void bs_prepare(bs_fixture *s) {
    mc_world_runtime_execution_result_init(&s->result);
    mc_world_block_execution_init(&s->frame, &s->cq.execution, cq_code, bs_site, &s->result);
    s->error = bs_error_marker;
    s->raw = MC_WORLD_STORAGE_LIMIT;
}

static int bs_begin(bs_fixture *s) {
    mc_entity_tick_execution_init(&s->cq.execution, NULL);
    VERIFY(mc_entity_tick_execution_push_world(&s->cq.execution, s->cq.source.game.worlds[0],
                                               bs_site, &s->outer) == MC_WORLD_STORAGE_OK);
    bs_prepare(s);
    return 0;
}

static int bs_end(bs_fixture *s, bool failed) {
    VERIFY(mc_entity_tick_execution_pop_world(&s->cq.execution, &s->outer, failed) ==
           MC_WORLD_STORAGE_OK);
    /* Expire every ordinary selection and borrowed diagnostic before its owner. */
    s->frame = (mc_world_block_execution){0};
    s->result = (mc_world_runtime_execution_result){0};
    VERIFY(mc_entity_tick_execution_retire(&s->cq.execution) == MC_WORLD_STORAGE_OK);
    return 0;
}

static int bs_value(bs_fixture *s, bs_kind kind, int32_t x, int32_t y, int32_t z, int32_t metadata,
                    int32_t expected, bool expected_bool, bool reached_get) {
    VERIFY(bs_begin(s) == 0);
    int32_t out = INT32_C(0x13579bdf);
    bool answer = !expected_bool;
    VERIFY(bs_call_frame(s, &s->frame, kind, s->cq.source.game.worlds[0], x, y, z, metadata, &out,
                         &answer, &s->error, &s->raw) &&
           (kind == BS_SET ? answer == expected_bool : out == expected) &&
           s->raw == MC_WORLD_STORAGE_OK && s->error.status == MC_WORLD_STORAGE_OK &&
           s->frame.entered && s->frame.completed && !s->frame.native_stopped &&
           s->frame.get_entered == reached_get && s->frame.get_returned == reached_get &&
           !s->frame.source.source_exception && s->result.ownership_status == MC_WORLD_STORAGE_OK &&
           !cq_pending(&s->result) && !s->result.selection_ready && bs_end(s, false) == 0);
    return 0;
}

static int bs_values(bs_fixture *s) {
    fixture *f = &s->cq.source.game;
    static const int32_t outside[][3] = {{-32000001, 64, 0}, {32000000, 64, 0}, {0, 64, -32000001},
                                         {0, 64, 32000001},  {0, -1, 0},        {0, 128, 0}};
    for (size_t i = 0; i < COUNT(outside); ++i)
        for (bs_kind kind = BS_BLOCK; kind <= BS_SET; kind = (bs_kind)(kind + 1))
            VERIFY(bs_value(s, kind, outside[i][0], outside[i][1], outside[i][2], 7, 0, false,
                            false) == 0 &&
                   f->providers[0].cache->count == 0);
    VERIFY(bs_value(s, BS_BLOCK, 8, 64, 8, 0, 1, false, true) == 0 && cq_remember(&s->cq) == 0 &&
           bs_value(s, BS_METADATA, 8, 64, 8, 0, 5, false, true) == 0 &&
           bs_value(s, BS_BLOCK, 8, 65, 8, 0, 3, false, true) == 0 &&
           bs_value(s, BS_METADATA, 8, 65, 8, 0, 10, false, true) == 0 &&
           bs_value(s, BS_BLOCK, 8, 66, 8, 0, 0, false, true) == 0 &&
           bs_value(s, BS_METADATA, 8, 66, 8, 0, 0, false, true) == 0);
    mc_world_runtime_source_construction_state construction;
    mc_world_source_chunk_loader_state loaded;
    VERIFY(mc_world_runtime_source_construction_inspect(f->worlds[0], &construction) &&
           mc_world_source_chunk_loader_inspect(construction.loader, &loaded));
    const size_t index = cell(8, 64, 8) >> 1;
    f->states[0]->dirty = false;
    VERIFY(bs_value(s, BS_SET, 8, 64, 8, 0x25, 0, true, true) == 0 && f->states[0]->dirty &&
           f->chunks[0]->e->a[index] == 0xA5);
    f->states[0]->dirty = false;
    VERIFY(bs_value(s, BS_SET, 8, 65, 8, -2, 0, true, true) == 0 && f->states[0]->dirty &&
           f->chunks[0]->e->a[index] == 0xE5 &&
           bs_value(s, BS_METADATA, 8, 65, 8, 0, 14, false, true) == 0 &&
           cq_loader_same(&s->cq, &loaded) && f->providers[0].cache->count == 1 &&
           f->providers[0].cache->insertion_count == 1);
    return 0;
}

static int bs_blank(bs_fixture *s) {
    fixture *f = &s->cq.source.game;
    mc_chunk_cache *cache = f->providers[0].cache;
    mc_chunk_state *blank = f->providers[0].fallback;
    VERIFY(blank == &f->worlds[0]->fallback && blank->blank && cache->count == 0);
    /* Native cache inputs use the actual provider-owned Blank identity. No
     * loader, generation or Source publication behavior is claimed here. */
    static const int32_t boundary[][3] = {
        {-32000000, 0, 0}, {31999999, 127, 0}, {0, 0, -32000000}, {0, 127, 32000000}};
    const bool dirty = blank->dirty;
    uint8_t *const blocks = blank->data->blocks;
    const uint8_t first = blocks[0];
    for (size_t i = 0; i < COUNT(boundary); ++i) {
        VERIFY(mc_chunk_cache_publish(cache, mc_java_i32_shr(boundary[i][0], 4),
                                      mc_java_i32_shr(boundary[i][2], 4), blank));
        for (bs_kind kind = BS_BLOCK; kind <= BS_SET; kind = (bs_kind)(kind + 1))
            VERIFY(bs_value(s, kind, boundary[i][0], boundary[i][1], boundary[i][2], 9, 0, true,
                            true) == 0 &&
                   blank->dirty == dirty && blank->data->blocks == blocks && blocks[0] == first);
        VERIFY(mc_chunk_cache_init(cache, cache->entries, cache->capacity, cache->insertions,
                                   cache->insertion_capacity));
    }
    return 0;
}

static int bs_metadata_fault(bs_fixture *s, bs_kind kind, unsigned int variant, int32_t y) {
    fixture *f = &s->cq.source.game;
    mc_world_chunk *chunk = f->chunks[0];
    mc_world_chunk_nibble *const original = chunk->e;
    uint8_t backing = 0xA5;
    mc_world_chunk_nibble input = {.a = variant == 1 ? NULL : &backing, .length = 1};
    chunk->e = variant == 0 ? NULL : &input;
    f->states[0]->dirty = false;
    VERIFY(bs_begin(s) == 0);
    int32_t out = 12345;
    bool answer = false;
    const bool completed = bs_call_frame(s, &s->frame, kind, f->worlds[0], 8, y, 8, 7, &out,
                                         &answer, &s->error, &s->raw);
    const bool dirty = f->states[0]->dirty;
    /* Return the fixture's authored field only for checked terminal teardown;
     * the failed Source body is never resumed after this restoration. */
    chunk->e = original;
    const unsigned int nibble_pc = variant == 0     ? 0u
                                   : kind == BS_SET ? (y == 64 ? 42u : 70u)
                                                    : (y == 64 ? 36u : 47u);
    VERIFY(!completed && out == 12345 && !answer && s->raw == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
           s->frame.entered && s->frame.get_entered && s->frame.get_returned &&
           s->frame.get_status == MC_CHUNK_ACQUIRE_OK && !s->frame.completed &&
           !s->frame.native_stopped && s->frame.source.source_exception &&
           strcmp(s->frame.source.exception_class, variant == 2
                                                       ? "java/lang/ArrayIndexOutOfBoundsException"
                                                       : "java/lang/NullPointerException") == 0 &&
           s->frame.source.world_pc == (kind == BS_SET ? 70u : 68u) &&
           s->frame.source.chunk_pc == (kind == BS_SET ? 14u : 7u) &&
           s->frame.source.nibble_pc == nibble_pc && dirty == (kind == BS_SET) && backing == 0xA5 &&
           s->result.ownership_status == MC_WORLD_STORAGE_OK && !s->result.selection_ready &&
           !cq_pending(&s->result) && bs_reenter(s, kind) == 0 && bs_end(s, true) == 0);
    return 0;
}

static int bs_metadata(bs_fixture *s) {
    fixture *f = &s->cq.source.game;
    VERIFY(bs_value(s, BS_METADATA, 8, 64, 8, 0, 5, false, true) == 0 && cq_remember(&s->cq) == 0);
    mc_world_chunk *chunk = f->chunks[0];
    mc_world_chunk_nibble *const original = chunk->e;
    uint8_t *backing = calloc(MC_CHUNK_NIBBLE_BYTES, 1);
    VERIFY(backing != NULL);
    const size_t index = cell(8, 64, 8) >> 1;
    backing[index] = 0xC3;
    mc_world_chunk_nibble replacement = {backing, MC_CHUNK_NIBBLE_BYTES};
    chunk->e = &replacement;
    VERIFY(bs_value(s, BS_METADATA, 8, 64, 8, 0, 3, false, true) == 0 &&
           bs_value(s, BS_METADATA, 8, 65, 8, 0, 12, false, true) == 0 &&
           bs_value(s, BS_SET, 8, 64, 8, 0x25, 0, true, true) == 0 && backing[index] == 0xC5 &&
           bs_value(s, BS_SET, 8, 65, 8, -2, 0, true, true) == 0 && backing[index] == 0xE5 &&
           original->a[index] == 0xA5);
    chunk->e = original;
    free(backing);
    for (unsigned int variant = 0; variant < 3; ++variant)
        for (int32_t y = 64; y <= 65; ++y)
            for (bs_kind kind = BS_METADATA; kind <= BS_SET; kind = (bs_kind)(kind + 1))
                VERIFY(bs_metadata_fault(s, kind, variant, y) == 0);
    return 0;
}

static bool bs_refused(bs_fixture *s, mc_world_block_execution *frame, bs_kind kind,
                       mc_world_storage_error *error, mc_world_storage_status *raw) {
    int32_t out = 12345;
    bool answer = true;
    *error = bs_error_marker;
    *raw = MC_WORLD_STORAGE_LIMIT;
    const bool completed = bs_call_frame(s, frame, kind, s->cq.source.game.worlds[0], -32000001, -1,
                                         32000001, 7, &out, &answer, error, raw);
    return !completed && out == 12345 && answer && bs_error_same(*error, bs_error_marker) &&
           *raw == MC_WORLD_STORAGE_LIMIT && !frame->entered && !frame->get_entered &&
           !frame->get_returned && !frame->completed;
}

static void bs_probe_node(bs_fixture *s) {
    cq_scope before = {0};
    const bool snapped = cq_scope_take(&s->cq, &before);
    for (bs_kind kind = BS_BLOCK; kind <= BS_SET; kind = (bs_kind)(kind + 1)) {
        mc_world_runtime_execution_result_init(&s->probe_result);
        mc_world_block_execution_init(&s->probe, &s->cq.execution, cq_code, bs_site,
                                      &s->probe_result);
        ++s->probes;
        s->callback_valid &= bs_refused(s, &s->probe, kind, &s->probe_error, &s->probe_raw) &&
                             s->probe_result.ownership_status == MC_WORLD_STORAGE_BUSY &&
                             !s->probe_result.selection_ready && !cq_pending(&s->probe_result) &&
                             snapped && cq_scope_same(&s->cq, &before);
    }
}

static mc_world_storage_status bs_dirty(void *context, mc_world_runtime *world, int32_t x,
                                        int32_t y, int32_t z, mc_tile_entity *tile,
                                        mc_world_storage_error *cause,
                                        mc_world_access_failure *failure) {
    bs_fixture *s = context;
    ++s->callbacks;
    s->cq.node = mc_entity_tick_execution_world_lease(&s->cq.execution);
    s->callback_valid &= world == s->cq.source.game.worlds[0] && x == 8 && y == 64 && z == 8 &&
                         tile == NULL && s->cq.node != NULL &&
                         s->cq.node->phase == MC_WORLD_ACCESS_FAILURE_LEASE_ARMED;
    bs_probe_node(s);
    if (!s->held)
        return MC_WORLD_STORAGE_OK;
    bool removed = false;
    mc_world_storage_status status =
        mc_world_runtime_remove_world_access(world, s->cq.receiver, &removed, cause);
    if (status != MC_WORLD_STORAGE_OK || !removed)
        return status == MC_WORLD_STORAGE_OK ? MC_WORLD_STORAGE_UNSUPPORTED_STATE : status;
    s->cq.removed = true;
    status = mc_world_access_receipt_create(s->cq.receiver, NULL, &s->cq.receipt, failure, cause);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    s->cq.receipt->exception_class = "java/lang/IllegalStateException";
    *cause =
        (mc_world_storage_error){MC_WORLD_STORAGE_IO_ERROR, "Scalar real callback failure", 77};
    return cause->status;
}

static int bs_nodes(bs_fixture *s, bool held) {
    fixture *f = &s->cq.source.game;
    s->held = held;
    s->callback_valid = true;
    const mc_world_access_source_callbacks callbacks = {.tile_entity_dirty = bs_dirty};
    VERIFY(mc_world_access_source_create(&callbacks, s, &s->cq.receiver, NULL) ==
               MC_WORLD_STORAGE_OK &&
           mc_world_runtime_add_world_access(f->worlds[0], s->cq.receiver, NULL) ==
               MC_WORLD_STORAGE_OK &&
           bs_begin(s) == 0);
    const mc_world_storage_status status = mc_world_runtime_tile_entity_dirty_in_execution(
        &s->cq.execution, f->worlds[0], cq_code, 8, 64, 8, NULL, &s->cq.dirty, &s->cq.result);
    if (held) {
        const mc_world_access_failure *view = NULL;
        VERIFY(status == MC_WORLD_STORAGE_IO_ERROR && s->cq.removed &&
               mc_world_access_failure_lease_inspect(s->cq.node, &view) &&
               view->receipt == s->cq.receipt && s->cq.receiver->active_holds == 1);
        bs_probe_node(s);
        VERIFY(mc_world_access_receipt_retire(&s->cq.receipt, NULL) == MC_WORLD_STORAGE_BUSY);
        s->cq.dirty.world_access_failure = (mc_world_access_failure){0};
        s->cq.result = s->probe_result = (mc_world_runtime_execution_result){0};
        view = NULL;
        VERIFY(bs_end(s, true) == 0 &&
               mc_world_access_receipt_retire(&s->cq.receipt, NULL) == MC_WORLD_STORAGE_OK);
    } else {
        bool removed = false;
        VERIFY(status == MC_WORLD_STORAGE_OK &&
               mc_world_runtime_remove_world_access(f->worlds[0], s->cq.receiver, &removed, NULL) ==
                   MC_WORLD_STORAGE_OK &&
               removed && bs_end(s, false) == 0);
    }
    VERIFY(s->callback_valid && mc_world_access_free(s->cq.receiver, NULL) == MC_WORLD_STORAGE_OK);
    s->cq.receiver = NULL;
    s->cq.node = NULL;
    return 0;
}

static int bs_admission(bs_fixture *s) {
    fixture *f = &s->cq.source.game;
    for (unsigned int mode = 0; mode < 3; ++mode) {
        mc_entity_tick_execution_init(&s->cq.execution, NULL);
        mc_world_runtime *foreign = NULL;
        mc_entity_tick_execution_world_token token = {0};
        if (mode == 2)
            VERIFY(mc_world_runtime_allocate_world(MC_WORLD_RUNTIME_CLIENT_LOAD_OR_CREATE,
                                                   f->classes[0], NULL, &foreign,
                                                   NULL) == MC_WORLD_STORAGE_OK &&
                   mc_entity_tick_execution_push_world(&s->cq.execution, foreign, bs_site,
                                                       &token) == MC_WORLD_STORAGE_OK);
        for (bs_kind kind = BS_BLOCK; kind <= BS_SET; kind = (bs_kind)(kind + 1)) {
            bs_prepare(s);
            if (mode == 0)
                s->frame.execution = NULL;
            VERIFY(bs_refused(s, &s->frame, kind, &s->error, &s->raw) &&
                   s->result.ownership_status != MC_WORLD_STORAGE_OK &&
                   !s->result.selection_ready && !cq_pending(&s->result) &&
                   f->providers[0].cache->count == 0);
        }
        if (mode == 2)
            VERIFY(mc_entity_tick_execution_pop_world(&s->cq.execution, &token, false) ==
                       MC_WORLD_STORAGE_OK &&
                   mc_world_runtime_free(foreign, NULL) == MC_WORLD_STORAGE_OK);
        VERIFY(mc_entity_tick_execution_retire(&s->cq.execution) == MC_WORLD_STORAGE_OK);
    }
    VERIFY(bs_value(s, BS_BLOCK, 8, 64, 8, 0, 1, false, true) == 0 && cq_remember(&s->cq) == 0 &&
           bs_nodes(s, false) == 0 && bs_nodes(s, true) == 0 && s->callbacks == 2 &&
           s->probes == 9);
    return 0;
}

static int bs_loader(bs_fixture *s) {
    fixture *f = &s->cq.source.game;
    VERIFY(bs_begin(s) == 0);
    int32_t out = 12345;
    bool answer = false;
    VERIFY(!bs_call_frame(s, &s->frame, BS_BLOCK, f->worlds[0], 8, 64, 8, 0, &out, &answer,
                          &s->error, &s->raw) &&
           out == 12345 && s->frame.entered && s->frame.get_entered && s->frame.get_returned &&
           s->frame.get_status == MC_CHUNK_ACQUIRE_CALLBACK_FAILED && !s->frame.completed &&
           !s->frame.native_stopped && s->raw == MC_WORLD_STORAGE_INVALID_DATA &&
           !s->frame.source.source_exception && s->result.ownership_status == MC_WORLD_STORAGE_OK &&
           s->result.selection_ready && !cq_pending(&s->result) && bs_reenter(s, BS_BLOCK) == 0);
    mc_world_runtime_source_construction_state construction;
    mc_world_source_chunk_loader_state producer;
    mc_entity_tick_execution_loader_view view;
    VERIFY(mc_entity_tick_execution_loader_read(&s->cq.execution, &s->result.selection, &view) &&
           view.failure != NULL &&
           mc_world_runtime_source_construction_inspect(f->worlds[0], &construction) &&
           mc_world_source_chunk_loader_inspect(construction.loader, &producer) &&
           producer.failure != NULL && view.failure != producer.failure &&
           kl_error_same(view.failure, producer.failure) && producer.last_pc == 18 &&
           s->frame.acquisition.source_chunk == view.failure &&
           mc_entity_tick_execution_pop_world(&s->cq.execution, &s->outer, true) ==
               MC_WORLD_STORAGE_OK &&
           mc_world_runtime_free(f->worlds[0], NULL) == MC_WORLD_STORAGE_BUSY &&
           mc_entity_tick_execution_loader_read(&s->cq.execution, &s->result.selection, &view) &&
           kl_error_same(view.failure, producer.failure));
    s->frame = (mc_world_block_execution){0};
    s->result = (mc_world_runtime_execution_result){0};
    view = (mc_entity_tick_execution_loader_view){0};
    VERIFY(mc_entity_tick_execution_retire(&s->cq.execution) == MC_WORLD_STORAGE_OK);
    return 0;
}

static mc_world_storage_status bs_added(void *context, mc_world_runtime *world, mc_entity *entity,
                                        mc_world_storage_error *cause,
                                        mc_world_access_failure *failure) {
    (void)cause;
    (void)failure;
    bs_fixture *s = context;
    ++s->callbacks;
    mc_entity_tick_execution *execution = &s->cq.execution;
    mc_entity_tick_execution_world_slot *outer = &execution->world_slots[s->outer.slot];
    s->callback_valid &= world == s->cq.source.game.worlds[0] && entity != NULL &&
                         mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_ITEM) &&
                         s->callbacks == 1 && s->frame.entered && s->frame.get_entered &&
                         !s->frame.get_returned && outer->loader_open &&
                         outer->loader_slot < MC_ENTITY_TICK_EXECUTION_LOADERS;
    if (!s->callback_valid)
        return MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    if (s->leave_descendant)
        return mc_entity_tick_execution_push_world(execution, world, bs_site, &s->descendant);
    s->root_loader = outer->loader_slot;
    mc_entity_tick_execution_loader_slot *loader = &execution->loader_slots[s->root_loader];
    s->reservation_generation = outer->selection_generation;
    s->callback_valid &= loader->phase == MC_ENTITY_TICK_EXECUTION_LOADER_RESERVED &&
                         loader->generation == s->reservation_generation &&
                         s->reservation_generation < UINT64_MAX;
    if (!s->callback_valid)
        return MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    /* Native integrity fault, not a Source mutation: keep the real ledger
     * internally consistent while making ONLY the captured outer attempt stale.
     * No loader receipt/selection/reservation is invented or manually begun. */
    ++outer->selection_generation;
    ++loader->generation;
    s->projected = true;
    return MC_WORLD_STORAGE_OK;
}

static int bs_completion(bs_fixture *s, bs_kind kind) {
    fixture *f = &s->cq.source.game;
    s->callback_valid = true;
    const mc_world_access_source_callbacks callbacks = {.entity_added = bs_added};
    VERIFY(mc_world_access_source_create(&callbacks, s, &s->cq.receiver, NULL) ==
               MC_WORLD_STORAGE_OK &&
           mc_world_runtime_add_world_access(f->worlds[0], s->cq.receiver, NULL) ==
               MC_WORLD_STORAGE_OK &&
           bs_begin(s) == 0);
    int32_t out = 12345;
    bool answer = false;
    const bool completed = bs_call_frame(s, &s->frame, kind, f->worlds[0], 8, 64, 8, 7, &out,
                                         &answer, &s->error, &s->raw);
    /* Restore only the two documented native projection scalars, before any
     * checked observation/drain. This does not restore or resume Source work. */
    if (s->projected) {
        s->cq.execution.world_slots[s->outer.slot].selection_generation = s->reservation_generation;
        s->cq.execution.loader_slots[s->root_loader].generation = s->reservation_generation;
        s->projected = false;
    }
    VERIFY(cq_remember(&s->cq) == 0);
    const size_t index = cell(8, 64, 8) >> 1;
    const bool dirty = f->states[0]->dirty;
    VERIFY(!completed && s->callback_valid && s->callbacks == 1 && out == 12345 && !answer &&
           s->raw == MC_WORLD_STORAGE_OK && s->frame.entered && s->frame.get_entered &&
           s->frame.get_returned && s->frame.get_status == MC_CHUNK_ACQUIRE_OK &&
           s->frame.native_stopped && !s->frame.completed && !s->frame.source.source_exception &&
           s->result.ownership_status == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
           s->result.root_pending && !s->result.selection_ready && cq_pending(&s->result) &&
           s->result.pending[s->result.root_attempt.slot].actual_status == MC_CHUNK_ACQUIRE_OK &&
           s->result.root_attempt.world.owner == s->outer.owner &&
           s->result.root_attempt.world.slot == s->outer.slot &&
           s->result.root_attempt.world.generation == s->outer.generation &&
           s->result.root_attempt.slot == s->root_loader &&
           s->result.root_attempt.generation == s->reservation_generation &&
           s->cq.execution.world_slots[s->outer.slot].selection_generation ==
               s->reservation_generation &&
           s->cq.execution.loader_slots[s->root_loader].generation == s->reservation_generation &&
           bs_error_same(s->error, bs_error_marker) && f->chunks[0]->e->a[index] == 0 && !dirty &&
           bs_reenter(s, kind) == 0);
    const mc_entity_tick_execution_error first = s->result.ownership_error;
    VERIFY(mc_world_runtime_execution_finish_pending(&s->cq.execution, &s->result) ==
               MC_WORLD_STORAGE_OK &&
           !cq_pending(&s->result) && s->result.selection_ready && out == 12345 && !answer &&
           !s->frame.completed && s->frame.native_stopped && s->raw == MC_WORLD_STORAGE_OK &&
           f->chunks[0]->e->a[index] == 0 && f->states[0]->dirty == dirty && s->callbacks == 1 &&
           s->result.ownership_error.status == first.status &&
           s->result.ownership_error.operation == first.operation);
    bool removed = false;
    VERIFY(mc_world_runtime_remove_world_access(f->worlds[0], s->cq.receiver, &removed, NULL) ==
               MC_WORLD_STORAGE_OK &&
           removed && bs_end(s, true) == 0 &&
           mc_world_access_free(s->cq.receiver, NULL) == MC_WORLD_STORAGE_OK);
    s->cq.receiver = NULL;
    return 0;
}

static int bs_open_descendant(bs_fixture *s) {
    fixture *f = &s->cq.source.game;
    s->callback_valid = s->leave_descendant = true;
    const mc_world_access_source_callbacks callbacks = {.entity_added = bs_added};
    VERIFY(mc_world_access_source_create(&callbacks, s, &s->cq.receiver, NULL) ==
               MC_WORLD_STORAGE_OK &&
           mc_world_runtime_add_world_access(f->worlds[0], s->cq.receiver, NULL) ==
               MC_WORLD_STORAGE_OK &&
           bs_begin(s) == 0);
    int32_t out = 12345;
    bool answer = false;
    VERIFY(!bs_call_frame(s, &s->frame, BS_SET, f->worlds[0], 8, 64, 8, 7, &out, &answer, &s->error,
                          &s->raw) &&
           !answer && s->callback_valid && s->callbacks == 1 &&
           s->frame.get_status == MC_CHUNK_ACQUIRE_CALLBACK_FAILED && s->frame.get_returned &&
           s->frame.native_stopped && !s->frame.completed && s->raw == MC_WORLD_STORAGE_BUSY &&
           s->result.root_pending && !s->result.selection_ready && cq_remember(&s->cq) == 0);
    size_t close_index = SIZE_MAX, close_count = 0;
    for (size_t i = 0; i < MC_ENTITY_TICK_EXECUTION_WORLDS; ++i)
        if (s->result.closes[i].used) {
            close_index = i;
            ++close_count;
        }
    VERIFY(close_count == 1 && close_index < MC_ENTITY_TICK_EXECUTION_WORLDS);
    const mc_world_runtime_execution_close close = s->result.closes[close_index];
    VERIFY(close.kind == MC_WORLD_RUNTIME_EXECUTION_CLOSE_CHUNK_ENTITY &&
           close.phase == MC_WORLD_RUNTIME_EXECUTION_CLOSE_LOCALS_DRAINED &&
           close.source_status == MC_WORLD_STORAGE_OK && !close.child_failed &&
           close.refused_status == MC_WORLD_STORAGE_BUSY &&
           s->cq.execution.current_lease == s->descendant.slot &&
           s->cq.execution.world_slots[s->descendant.slot].parent == close.world.slot &&
           f->chunks[0]->e->a[cell(8, 64, 8) >> 1] == 0 && !f->states[0]->dirty &&
           bs_reenter(s, BS_SET) == 0 &&
           mc_world_runtime_execution_finish_pending(&s->cq.execution, &s->result) ==
               MC_WORLD_STORAGE_BUSY &&
           s->result.root_pending && !answer);
    VERIFY(mc_entity_tick_execution_pop_world(&s->cq.execution, &s->descendant, false) ==
               MC_WORLD_STORAGE_OK &&
           mc_world_runtime_execution_retry_close(&s->cq.execution, &s->result, &close) ==
               MC_WORLD_STORAGE_OK &&
           !s->result.closes[close_index].used &&
           mc_world_runtime_execution_finish_pending(&s->cq.execution, &s->result) ==
               MC_WORLD_STORAGE_OK &&
           !cq_pending(&s->result) && s->result.selection_ready && !answer && !s->frame.completed &&
           s->frame.native_stopped && s->callbacks == 1 &&
           f->chunks[0]->e->a[cell(8, 64, 8) >> 1] == 0 && !f->states[0]->dirty);
    bool removed = false;
    VERIFY(mc_world_runtime_remove_world_access(f->worlds[0], s->cq.receiver, &removed, NULL) ==
               MC_WORLD_STORAGE_OK &&
           removed && bs_end(s, true) == 0 &&
           mc_world_access_free(s->cq.receiver, NULL) == MC_WORLD_STORAGE_OK);
    s->cq.receiver = NULL;
    return 0;
}

static bool bs_mode(const char *mode) {
    return strcmp(mode, "values") == 0 || strcmp(mode, "metadata") == 0 ||
           strcmp(mode, "blank") == 0 || strcmp(mode, "admission") == 0 ||
           strcmp(mode, "loader-error") == 0 || strcmp(mode, "completion-block") == 0 ||
           strcmp(mode, "completion-metadata") == 0 || strcmp(mode, "completion-set") == 0 ||
           strcmp(mode, "open-descendant") == 0;
}

#ifndef MC_WORLD_BLOCK_SCALAR_MAIN
#define MC_WORLD_BLOCK_SCALAR_MAIN main
#endif
int MC_WORLD_BLOCK_SCALAR_MAIN(int argc, char **argv) {
    VERIFY(argc == 3 && bs_mode(argv[1]));
    printf("BLOCK_SCALAR_BEGIN schema=1 mode=%s\n", argv[1]);
    fflush(stdout);
    bs_fixture *s = calloc(1, sizeof(*s));
    VERIFY(s != NULL);
    s->cq.mode = argv[1];
    VERIFY(ss_setup(&s->cq.source, argv[2]) == 0 && cq_files(&s->cq, true) == 0 &&
           ss_open(&s->cq.source) == 0);
    printf("BLOCK_SCALAR_CASE %s BEGIN\n", argv[1]);
    fflush(stdout);
    const int status =
        strcmp(argv[1], "values") == 0         ? bs_values(s)
        : strcmp(argv[1], "metadata") == 0     ? bs_metadata(s)
        : strcmp(argv[1], "blank") == 0        ? bs_blank(s)
        : strcmp(argv[1], "admission") == 0    ? bs_admission(s)
        : strcmp(argv[1], "loader-error") == 0 ? bs_loader(s)
        : strcmp(argv[1], "open-descendant") == 0
            ? bs_open_descendant(s)
            : bs_completion(s, strcmp(argv[1], "completion-block") == 0      ? BS_BLOCK
                               : strcmp(argv[1], "completion-metadata") == 0 ? BS_METADATA
                                                                             : BS_SET);
    VERIFY(status == 0);
    printf("BLOCK_SCALAR_CASE %s END calls=%zu callbacks=%zu probes=%zu\n", argv[1], s->calls,
           s->callbacks, s->probes);
    fflush(stdout);
    s->frame = s->probe = (mc_world_block_execution){0};
    s->result = s->probe_result = (mc_world_runtime_execution_result){0};
    VERIFY((s->cq.source.game.states[0] == NULL ? cq_empty_stop(&s->cq.source)
                                                : ss_stop(&s->cq.source)) == 0 &&
           cq_files(&s->cq, false) == 0 && ss_finish(&s->cq.source) == 0);
    for (size_t i = 0; i < COUNT(s->cq.inputs); ++i)
        mc_ws_free_file_bytes(&s->cq.inputs[i]);
    free(s);
    puts("BLOCK_SCALAR_OPEN external-cold-graph/full-Arrow");
    printf("PASS: %u actual owned block scalar checks;mode=%s\n", checks, argv[1]);
    return 0;
}
