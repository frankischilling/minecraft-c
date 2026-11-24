/* Native ownership/Source-operation integration controls, NOT an observed Java
 * Block race or complete SL invocation. The actual captured Block36 operand and
 * actual stored cell61/62 are deliberately distinct inputs. Direct modes author
 * a private Arrow native frame at its helper-entry seam; Ray modes run the full
 * public owned fd.a(bt,bt,ZZ) body with independent defining graphs A and B. */
#include "entity/arrow/tick_internal.h"
#include "world/runtime/raytrace_execution_internal.h"
#include "world/runtime/tile_entities/tile_entities_internal.h"

typedef struct fol_arrow_source {
    mc_entity_snapshot entity;
    mc_arrow_fields arrow;
    mc_entity_bounds bounds;
    mc_entity_watcher_snapshot watcher;
    mc_java_random random;
} fol_arrow_source;

typedef struct fol_fixture {
    aol_fixture base;
    mc_arrow_tick_call *arrow_call;
    mc_world_raytrace_call *ray;
    mc_entity_vector *start, *end;
    mc_entity_tick_execution *execution;
    mc_world_runtime_execution_result *result;
    mc_entity_tick_execution_world_token root, lazy_world, external;
    mc_world_access *access;
    mc_world_access_receipt *receipt;
    mc_world_access_failure_lease *lease;
    mc_world_storage_error callback_cause;
    fol_arrow_source arrow_before;
    char *exception_name;
    mc_block *saved_m[2];
    bool saved_opacity, ray_mode, fail_mode, native_failure, callback_valid, removed;
    size_t callbacks, installations, bindings, code_borrows, arrow_holds;
    int32_t y, metadata;
} fol_fixture;

static bool fol_source_take(fol_fixture *s, fol_arrow_source *out) {
    mc_entity *arrow = s->base.arrow;
    if (mc_entity_get_snapshot(arrow, &out->entity) != MC_ENTITY_OK || arrow->bounds == NULL ||
        arrow->random == NULL ||
        mc_entity_watcher_get_snapshot(arrow->watcher, &out->watcher) != MC_ENTITY_OK ||
        out->watcher.size != 1)
        return false;
    out->arrow = arrow->arrow->fields;
    out->bounds = *arrow->bounds;
    out->random = *arrow->random;
    return true;
}

static bool fol_source_same(fol_fixture *s) {
    fol_arrow_source now;
    if (!fol_source_take(s, &now))
        return false;
    const mc_entity_snapshot *a = &s->arrow_before.entity, *b = &now.entity;
    /* All actual SN base fields, rather than the native Entity/Arrow headers:
     * holds, bookkeeping and the borrowed tick backlink are not Source fields. */
#define FOL_BASE(f)                                                                                \
    if (a->base.f != b->base.f)                                                                    \
    return false
    FOL_BASE(id);
    FOL_BASE(aF);
    FOL_BASE(aG);
    FOL_BASE(aH);
    FOL_BASE(world);
    FOL_BASE(aX);
    FOL_BASE(aY);
    FOL_BASE(aZ);
    FOL_BASE(ba);
    FOL_BASE(bb);
    FOL_BASE(bc);
    FOL_BASE(bd);
    FOL_BASE(be);
    FOL_BASE(private_b);
    FOL_BASE(bq);
    FOL_BASE(bt);
    FOL_BASE(bu);
    FOL_BASE(bv);
    FOL_BASE(bw);
    FOL_BASE(bx);
    FOL_BASE(by);
    FOL_BASE(bz);
    FOL_BASE(private_c);
    FOL_BASE(bC);
    FOL_BASE(bF);
    FOL_BASE(bG);
    FOL_BASE(bH);
    FOL_BASE(bI);
    FOL_BASE(bM);
    FOL_BASE(client_only.bA);
    FOL_BASE(client_only.bB);
    FOL_BASE(client_only.bJ);
    FOL_BASE(client_only.bK);
    FOL_BASE(client_only.bL);
#undef FOL_BASE
#define FOL_DOUBLE(f)                                                                              \
    if (!aol_double(a->base.f, b->base.f))                                                         \
    return false
    FOL_DOUBLE(aE);
    FOL_DOUBLE(aJ);
    FOL_DOUBLE(aK);
    FOL_DOUBLE(aL);
    FOL_DOUBLE(x);
    FOL_DOUBLE(y);
    FOL_DOUBLE(z);
    FOL_DOUBLE(velocity_x);
    FOL_DOUBLE(velocity_y);
    FOL_DOUBLE(velocity_z);
    FOL_DOUBLE(bl);
    FOL_DOUBLE(bm);
    FOL_DOUBLE(bn);
    FOL_DOUBLE(private_d);
    FOL_DOUBLE(private_e);
#undef FOL_DOUBLE
#define FOL_FLOAT(f)                                                                               \
    if (!aol_float(a->base.f, b->base.f))                                                          \
    return false
    FOL_FLOAT(yaw);
    FOL_FLOAT(aT);
    FOL_FLOAT(aU);
    FOL_FLOAT(aV);
    FOL_FLOAT(offset);
    FOL_FLOAT(width);
    FOL_FLOAT(height);
    FOL_FLOAT(bi);
    FOL_FLOAT(bj);
    FOL_FLOAT(bk);
    FOL_FLOAT(y_size);
    FOL_FLOAT(bp);
    FOL_FLOAT(br);
    FOL_FLOAT(bE);
#undef FOL_FLOAT
#define FOL_ARROW(f)                                                                               \
    if (s->arrow_before.arrow.f != now.arrow.f)                                                    \
    return false
    FOL_ARROW(a);
    FOL_ARROW(b);
    FOL_ARROW(c);
    FOL_ARROW(d);
    FOL_ARROW(e);
    FOL_ARROW(f);
    FOL_ARROW(g);
    FOL_ARROW(h);
    FOL_ARROW(i);
    FOL_ARROW(j);
    FOL_ARROW(k);
#undef FOL_ARROW
#define FOL_BOUNDS(f)                                                                              \
    if (!aol_double(s->arrow_before.bounds.f, now.bounds.f))                                       \
    return false
    FOL_BOUNDS(min_x);
    FOL_BOUNDS(min_y);
    FOL_BOUNDS(min_z);
    FOL_BOUNDS(max_x);
    FOL_BOUNDS(max_y);
    FOL_BOUNDS(max_z);
#undef FOL_BOUNDS
    const mc_entity_watcher_snapshot *wa = &s->arrow_before.watcher, *wb = &now.watcher;
    return a->source == b->source && a->class_literal == b->class_literal && a->phase == b->phase &&
           a->bounds == b->bounds && a->random == b->random && a->watcher == b->watcher &&
           a->stack == b->stack && wa->dirty == wb->dirty && wa->size == wb->size &&
           wa->record == wb->record && wa->index == wb->index && wa->type == wb->type &&
           wa->payload_kind == wb->payload_kind && wa->byte_value == wb->byte_value &&
           wa->record_dirty == wb->record_dirty && aol_random(&s->arrow_before.random, &now.random);
}

static mc_tile_entity *fol_map(fol_fixture *s, int32_t x, int32_t y, int32_t z) {
    const mc_world_chunk *chunk = s->base.cq.source.game.chunks[0];
    for (size_t i = 0; i < chunk->l->count; ++i) {
        mc_live_tile_entity_map_entry entry;
        if (mc_live_tile_entities_map_entry_at(chunk, i, &entry) != MC_WORLD_STORAGE_OK)
            return NULL;
        if (entry.x == x && entry.y == y && entry.z == z)
            return entry.tile_entity;
    }
    return NULL;
}

static bool fol_metadata(fol_fixture *s, int32_t y, uint8_t *out) {
    const mc_world_chunk *chunk = s->base.cq.source.game.chunks[0];
    return mc_chunk_nibbles_get(chunk->e->a, chunk->e->length, 8, y, 8, out);
}

static void fol_cell(fol_fixture *s, int32_t x, int32_t y, int32_t z, uint8_t id) {
    /* Explicit fixture input in the real already-bound Chunk's actual array.
     * No source setter/getter/hook is replaced, nor is any Tile Map authored. */
    s->base.cq.source.game.chunks[0]
        ->data->blocks[((size_t)x << 11u) | ((size_t)z << 7u) | (size_t)y] = id;
}

static uint64_t fol_gets(fol_fixture *s) {
    mc_chunk_provider_cache_observation view;
    if (!mc_chunk_provider_cache_observe(s->base.cq.source.game.worlds[0]->fields.v, &view))
        return UINT64_MAX;
    return view.hits + view.generic_gets;
}

static mc_world_storage_status fol_cell_failure(void *context, mc_world_runtime *world, int32_t x,
                                                int32_t y, int32_t z, mc_world_storage_error *cause,
                                                mc_world_access_failure *failure) {
    fol_fixture *s = context;
    fixture *f = &s->base.cq.source.game;
    ++s->callbacks;
    uint8_t metadata = 0;
    mc_tile_entity *tile = fol_map(s, 8, s->y, 8);
    s->lease = mc_entity_tick_execution_world_lease(s->execution);
    s->callback_valid =
        s->callbacks == 1 && world == f->worlds[0] && x == 8 && y == s->y && z == 8 &&
        tile != NULL && tile->kind == MC_TILE_ENTITY_FURNACE && tile->world == world &&
        tile->x == 8 && tile->y == y && tile->z == 8 && tile->holds == 2 &&
        fol_metadata(s, y, &metadata) && metadata == s->metadata && s->lease != NULL &&
        s->lease->phase == MC_WORLD_ACCESS_FAILURE_LEASE_ARMED && s->access->active_holds == 1 &&
        mc_entity_tick_execution_current_world(s->execution, &s->lazy_world);
    bool removed = false;
    mc_world_storage_status status =
        mc_world_runtime_remove_world_access(world, s->access, &removed, cause);
    s->removed = status == MC_WORLD_STORAGE_OK && removed;
    if (!s->removed)
        return status == MC_WORLD_STORAGE_OK ? MC_WORLD_STORAGE_UNSUPPORTED_STATE : status;
    status = mc_world_access_receipt_create(s->access, NULL, &s->receipt, failure, cause);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    if (!s->native_failure) {
        static const char name[] = "java/lang/IllegalStateException";
        s->exception_name = malloc(sizeof(name));
        if (s->exception_name == NULL)
            return MC_WORLD_STORAGE_ALLOCATION_FAILED;
        memcpy(s->exception_name, name, sizeof(name));
        s->receipt->exception_class = s->exception_name;
    }
    /* A declared native structural fault, after the actual metadata stores and
     * protected callback entry. Only the fixture may later remove this token. */
    status = mc_entity_tick_execution_push_world(
        s->execution, world,
        (mc_entity_tick_execution_site){"native.furnace.control", "nested", "()V", 0},
        &s->external);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    *cause = (mc_world_storage_error){MC_WORLD_STORAGE_IO_ERROR,
                                      "Native Furnace metadata observer failure", 77};
    s->callback_cause = *cause;
    return cause->status;
}

static int fol_second_graph(fol_fixture *s) {
    fixture *f = &s->base.cq.source.game;
    resources *r = &f->sources[1];
    r->streams[0] = (stream){.bytes = language, .length = sizeof(language) - 1};
    r->streams[1] = r->streams[2] = (stream){.bytes = language};
    r->api = (mc_registry_resources){r, open_resource, close_resource, ready_resource};
    const mc_registry_options options = {MC_REGISTRY_CLIENT, f->profile, f->materials, 0, &r->api};
    VERIFY(mc_registry_classes_allocate(&options, &f->registry[1], NULL) == MC_REGISTRY_OK &&
           mc_world_runtime_classes_create(MC_WORLD_INFO_CLIENT, f->profile, &trig, &f->classes[1],
                                           NULL) == MC_WORLD_STORAGE_OK &&
           mc_world_runtime_classes_bind_registry(f->classes[1], f->registry[1], NULL) ==
               MC_REGISTRY_OK &&
           mc_world_runtime_classes_bind_entities(f->classes[1], f->entity_classes[0]) ==
               MC_ENTITY_OK &&
           mc_registry_classes_initialize_blocks(f->registry[1], NULL) == MC_REGISTRY_OK &&
           r->opens == 3 && r->closes == 1 && !r->bad);
    VERIFY(f->registry[1] != f->registry[0] && f->classes[1] != f->classes[0] &&
           f->registry[1]->blocks.m[36]->kind == MC_BLOCK_MOVING_PISTON &&
           f->registry[0]->blocks.m[61]->kind == MC_BLOCK_FURNACE && f->registry[0]->blocks.o[1] &&
           f->registry[1]->blocks.o[1]);
    /* A's source Ray array selects its genuine captured Block36. B remains the
     * independently supplied actual Tile/factory/TC static namespace. Neither
     * graph is inferred from World/Chunk.d; this is NOT a JVM classloader claim. */
    for (size_t i = 0; i < 2; ++i) {
        s->saved_m[i] = f->registry[1]->blocks.m[61 + i];
        f->registry[1]->blocks.m[61 + i] = f->registry[1]->blocks.m[36];
    }
    s->saved_opacity = f->registry[1]->blocks.o[1];
    f->registry[1]->blocks.o[1] = false;
    return 0;
}

static int fol_arrow_frame(fol_fixture *s, bool box) {
    fixture *f = &s->base.cq.source.game;
    mc_arrow_tick_call *c = calloc(1, sizeof(*c));
    VERIFY(c != NULL && s->base.arrow->arrow->tick_call == NULL &&
           mc_entity_retain(s->base.arrow) == MC_ENTITY_OK);
    /* Explicit native private-frame input, matching mc_arrow_tick_begin up to
     * its Source body call. No preceding invocation/failure is reset or resumed. */
    c->limits = mc_arrow_tick_default_limits();
    c->error.site.pc = c->error.child_site.pc = -1;
    c->cleanup = c->error;
    c->state.receiver = s->base.arrow;
    c->state.phase = MC_ARROW_TICK_ACTIVE;
    c->state.active = true;
    c->state.failure = &c->error;
    c->state.cleanup = &c->cleanup;
    c->receiver_held = true;
    c->code_classes = f->classes[0];
    ++c->code_classes->native_borrows;
    c->code_held = true;
    c->registry = f->registry[0];
    c->query_code = cq_code;
    mc_world_runtime_execution_result_init(&c->query_result);
    mc_math_execution_init(&c->math, s->base.math_owner);
    mc_entity_tick_execution_init(&c->execution, &c->math);
    s->base.arrow->arrow->tick_call = c;
    s->arrow_call = c;
    s->execution = &c->execution;
    s->result = &c->query_result;
    VERIFY(c->state.receiver->phase == MC_ENTITY_READY &&
           c->state.receiver->holds == s->arrow_holds + 1 &&
           c->limits.max_parent_bytes >= sizeof(*c) && c->execution.identity == &c->execution &&
           c->math.owned == NULL && c->query_result.ownership_status == MC_WORLD_STORAGE_OK &&
           c->code_classes->native_borrows == s->code_borrows + 1 &&
           mc_arrow_tick_world_begin(c, f->worlds[0], box ? 155u : 131u));
    s->root = c->direct_world;
    return 0;
}

static int fol_failure(fol_fixture *s, const mc_world_collision_error *collision) {
    fixture *f = &s->base.cq.source.game;
    const mc_world_mutation_error *child = collision->lazy_addition;
    VERIFY(s->callbacks == 1 && s->callback_valid && s->removed && child != NULL &&
           child->status == MC_WORLD_STORAGE_IO_ERROR &&
           child->cause.status == s->callback_cause.status &&
           child->cause.operation == s->callback_cause.operation &&
           child->cause.system_error == 77 && child->hook_class != NULL &&
           strcmp(child->hook_class, "tc") == 0 && child->hook_bytecode == 15 &&
           child->cause_hook_bytecode == 150 &&
           s->result->ownership_status == MC_WORLD_STORAGE_BUSY && !s->result->selection_ready &&
           !s->result->root_pending);
    VERIFY(s->receipt != NULL && child->world_access_failure.receiver == s->access &&
           child->world_access_failure.receipt == s->receipt &&
           collision->nearby.tile_entity.world_access_failure.receipt == s->receipt &&
           s->access->active_holds == 1 &&
           mc_world_access_receipt_retire(&s->receipt, NULL) == MC_WORLD_STORAGE_BUSY &&
           mc_world_access_free(s->access, NULL) == MC_WORLD_STORAGE_BUSY);
    if (s->native_failure)
        VERIFY(s->exception_name == NULL && s->receipt->exception_class == NULL);
    else
        VERIFY(s->exception_name != NULL && s->receipt->exception_class == s->exception_name);
    const mc_world_mutation_error saved = *child;
    const uint64_t gets = fol_gets(s);
    const size_t bindings = f->worlds[0]->persistent_bindings;
    const size_t tiles = f->chunks[0]->l->count;
    mc_entity_tick_execution_world_token current;
    VERIFY(mc_entity_tick_execution_current_world(s->execution, &current) &&
           cq_token_same(current, s->external) && fol_source_same(s));
    bool quiescent = true;
    if (s->ray_mode) {
        mc_world_raytrace_state state;
        VERIFY(mc_world_raytrace_inspect(s->ray, &state) && state.failure != NULL &&
               state.failure->source_exception == !s->native_failure &&
               state.failure->native_policy == s->native_failure);
        if (s->native_failure)
            VERIFY(state.failure->exception_class == NULL);
        else
            VERIFY(state.failure->status == MC_WORLD_RAYTRACE_SOURCE_EXCEPTION &&
                   state.failure->exception_class == s->exception_name &&
                   strcmp(state.failure->exception_class, "java/lang/IllegalStateException") == 0);
        VERIFY(mc_world_raytrace_retire(&s->ray) == MC_WORLD_RAYTRACE_CLEANUP_FAILED &&
               s->ray != NULL &&
               mc_world_raytrace_native_unwind_step(s->ray, &quiescent) == MC_WORLD_STORAGE_BUSY &&
               quiescent);
    } else {
        mc_arrow_tick_call *c = s->arrow_call;
        VERIFY(
            c->lazy_tile_entity != NULL &&
            mc_live_tile_entity_query_execution_owns_world(c->lazy_tile_entity, &s->lazy_world) &&
            !mc_live_tile_entity_query_execution_owns_world(c->lazy_tile_entity, &s->external) &&
            mc_live_tile_entity_query_execution_failure(c->lazy_tile_entity) == child &&
            c->error.block == collision && c->error.query == s->result);
        const mc_arrow_tick_error first = c->error;
        VERIFY(first.classification == (s->native_failure
                                            ? MC_ARROW_TICK_FAILURE_NATIVE
                                            : MC_ARROW_TICK_FAILURE_SOURCE_EXCEPTION) &&
               first.source_exception == !s->native_failure && !first.source_error &&
               first.native_policy == s->native_failure && c->exception_storage == NULL);
        if (s->native_failure)
            VERIFY(first.exception_class == NULL);
        else
            VERIFY(first.exception_class == s->exception_name &&
                   strcmp(first.exception_class, "java/lang/IllegalStateException") == 0);
        VERIFY(mc_live_tile_entity_query_execution_native_close(c->lazy_tile_entity) ==
                   MC_WORLD_STORAGE_BUSY &&
               mc_live_tile_entity_query_execution_retire(&c->lazy_tile_entity) ==
                   MC_WORLD_STORAGE_BUSY &&
               mc_arrow_tick_retire(&s->arrow_call) != MC_ARROW_TICK_OK && s->arrow_call == c &&
               c->error.status == first.status && c->error.operation == first.operation &&
               c->error.site.pc == first.site.pc && c->error.block == collision &&
               c->work.block.lazy_addition == child &&
               mc_arrow_tick_native_unwind_step(c, &quiescent) != MC_ARROW_TICK_OK);
        VERIFY(c->error.classification == first.classification &&
               c->error.source_exception == first.source_exception &&
               c->error.native_policy == first.native_policy);
        if (s->native_failure)
            VERIFY(c->error.exception_class == NULL && c->exception_storage == NULL &&
                   c->exception_storage_bytes == 0);
        else
            VERIFY(c->exception_storage != NULL &&
                   c->error.exception_class == c->exception_storage &&
                   c->error.exception_class != s->exception_name &&
                   c->exception_storage_bytes == strlen(s->exception_name) + 1 &&
                   strcmp(c->error.exception_class, s->exception_name) == 0);
    }
    VERIFY(mc_entity_tick_execution_current_world(s->execution, &current) &&
           cq_token_same(current, s->external) && fol_gets(s) == gets &&
           f->worlds[0]->persistent_bindings == bindings && f->chunks[0]->l->count == tiles &&
           collision->lazy_addition == child && child->status == saved.status &&
           child->hook_bytecode == saved.hook_bytecode &&
           child->cause_hook_bytecode == saved.cause_hook_bytecode &&
           child->world_access_failure.receipt == saved.world_access_failure.receipt &&
           fol_source_same(s));
    /* Correct only the fixture's unrelated native token. The Source error,
     * installed Tile, canonical first refusal and retained child are untouched. */
    VERIFY(mc_entity_tick_execution_pop_world(s->execution, &s->external, false) ==
               MC_WORLD_STORAGE_OK &&
           mc_entity_tick_execution_current_world(s->execution, &current) &&
           cq_token_same(current, s->lazy_world));
    quiescent = false;
    for (size_t i = 0; i < 40 && !quiescent; ++i) {
        if (s->ray_mode)
            VERIFY(mc_world_raytrace_native_unwind_step(s->ray, &quiescent) == MC_WORLD_STORAGE_OK);
        else
            VERIFY(mc_arrow_tick_native_unwind_step(s->arrow_call, &quiescent) == MC_ARROW_TICK_OK);
    }
    VERIFY(
        quiescent && fol_gets(s) == gets && s->callbacks == 1 && f->chunks[0]->l->count == tiles &&
        s->result->ownership_status == MC_WORLD_STORAGE_BUSY && collision->lazy_addition == NULL &&
        collision->nearby.tile_entity.cause.status == MC_WORLD_STORAGE_IO_ERROR &&
        collision->nearby.tile_entity.cause.operation == s->callback_cause.operation &&
        fol_source_same(s));
    /* The retained mutation owner is gone; the first parent diagnostic and its
     * owning text/producer occurrence remain valid until checked parent retire. */
    if (s->ray_mode) {
        mc_world_raytrace_state state;
        VERIFY(mc_world_raytrace_inspect(s->ray, &state) && state.failure != NULL &&
               state.failure->source_exception == !s->native_failure &&
               state.failure->native_policy == s->native_failure &&
               state.failure->exception_class == s->exception_name);
        if (!s->native_failure)
            VERIFY(strcmp(state.failure->exception_class, "java/lang/IllegalStateException") == 0);
    } else {
        mc_arrow_tick_call *c = s->arrow_call;
        VERIFY(c->lazy_tile_entity == NULL && c->error.source_exception == !s->native_failure &&
               c->error.native_policy == s->native_failure &&
               c->error.classification == (s->native_failure
                                               ? MC_ARROW_TICK_FAILURE_NATIVE
                                               : MC_ARROW_TICK_FAILURE_SOURCE_EXCEPTION));
        if (!s->native_failure)
            VERIFY(c->error.exception_class == c->exception_storage &&
                   c->error.exception_class != s->exception_name &&
                   strcmp(c->error.exception_class, s->exception_name) == 0 &&
                   c->exception_storage_bytes == strlen(s->exception_name) + 1);
    }
    return 0;
}

static int fol_call(fol_fixture *s, int32_t y, bool box, bool install) {
    fixture *f = &s->base.cq.source.game;
    s->y = y;
    VERIFY(fol_source_take(s, &s->arrow_before));
    const size_t bindings = f->worlds[0]->persistent_bindings;
    const uint64_t before_gets = fol_gets(s);
    const size_t maps = f->chunks[0]->l->count, members = f->worlds[0]->fields.c->count;
    mc_tile_entity_piston_class_snapshot before, after;
    VERIFY(mc_tile_entity_piston_class_get_snapshot(f->classes[0], &before) == MC_TILE_ENTITY_OK);
    bool completed;
    mc_world_raytrace_state state = {0};
    const mc_world_collision_error *collision = NULL;
    if (s->ray_mode) {
        s->execution = &s->base.cq.execution;
        s->result = &s->base.cq.result;
        mc_world_runtime_execution_result_init(s->result);
        VERIFY(mc_entity_tick_execution_push_world(s->execution, f->worlds[0], cq_site, &s->root) ==
                   MC_WORLD_STORAGE_OK &&
               mc_entity_vector_create(f->entity_classes[0], 8.25, (double)y + .25, 8.25, &s->start,
                                       NULL) == MC_ENTITY_OK &&
               mc_entity_vector_create(f->entity_classes[0], 8.75, (double)y + .75, 8.75, &s->end,
                                       NULL) == MC_ENTITY_OK);
        const mc_world_collision_block_execution_code code = {
            cq_code, f->registry[0], f->classes[0], {"lm", "d", "(III)Low;", 67}};
        mc_world_raytrace_status status = MC_WORLD_RAYTRACE_OK;
        completed = mc_world_raytrace_flags_begin_in_execution(
            s->execution, f->classes[1], f->worlds[0], code, s->start, s->end, false, box, &s->ray,
            &status, s->result);
        VERIFY(s->ray != NULL && mc_world_raytrace_inspect(s->ray, &state) && !state.active &&
               state.cells_started == 1 && state.cells_read == 1 && state.x == 8 && state.y == y &&
               state.z == 8 && state.movement_box_calls == (box ? 1u : 0u) &&
               state.block_ray_calls == (box ? 0u : 1u));
        if (s->fail_mode) {
            VERIFY(!completed && !state.source_returned && state.failure != NULL &&
                   state.failure->collision != NULL);
            collision = state.failure->collision;
        } else
            VERIFY(completed && status == MC_WORLD_RAYTRACE_OK && state.source_returned &&
                   state.phase == MC_WORLD_RAYTRACE_RETURNED && state.failure == NULL &&
                   state.result == NULL && state.source_pc == 284);
    } else {
        VERIFY(fol_arrow_frame(s, box) == 0);
        const mc_block *block = f->registry[0]->blocks.m[36];
        completed = mc_arrow_tick_block_shape(s->arrow_call, f->worlds[0], block, 8, y, 8, box,
                                              box ? 170u : 146u);
        s->arrow_call->state.active = false; /* Native helper fixture boundary, not SL return. */
        s->arrow_call->state.phase = completed ? MC_ARROW_TICK_RETURNED : MC_ARROW_TICK_FAILED;
        collision = &s->arrow_call->work.block;
        if (s->fail_mode)
            VERIFY(!completed && s->arrow_call->failed && s->arrow_call->lazy_tile_entity != NULL);
        else
            VERIFY(completed && !s->arrow_call->failed && s->arrow_call->lazy_tile_entity == NULL &&
                   s->arrow_call->block_box == NULL && !s->arrow_call->direct_world_open &&
                   aol_quiet(s->result));
    }
    VERIFY(fol_source_same(s));
    mc_tile_entity *tile = fol_map(s, 8, y, 8);
    uint8_t metadata = 0;
    VERIFY(tile != NULL && tile->kind == MC_TILE_ENTITY_FURNACE &&
           tile->phase == MC_TILE_ENTITY_READY && tile->world == f->worlds[0] && tile->holds == 2 &&
           tile->classes == f->classes[0]->tile_entities && tile->x == 8 && tile->y == y &&
           tile->z == 8 && tile->state.furnace.capacity == 3 &&
           tile->state.furnace.burn_remaining == 0 && tile->state.furnace.fuel_duration == 0 &&
           tile->state.furnace.cook_progress == 0 && !tile->invalid &&
           f->chunks[0]->l->count == maps + (install ? 1u : 0u) &&
           f->worlds[0]->fields.c->count == members + (install ? 1u : 0u) &&
           mc_tile_entity_piston_class_get_snapshot(f->classes[0], &after) == MC_TILE_ENTITY_OK &&
           after.live_tile_entities == before.live_tile_entities + (install ? 1u : 0u) &&
           fol_metadata(s, y, &metadata) && metadata == s->metadata);
    if (install)
        ++s->installations;
    /* Actual native provider entry counts, not a claimed JVM/JDI trace. Eight
     * gets reach failing metadata notification; successful orientation also
     * completes its six Source neighbor notifications. Ray adds ID+metadata. */
    const uint64_t expected = (install ? (s->fail_mode ? 8u : 14u) : 1u) + (s->ray_mode ? 2u : 0u);
    if (fol_gets(s) - before_gets != expected)
        fprintf(stderr, "lazy Furnace actual provider gets=%llu expected=%llu\n",
                (unsigned long long)(fol_gets(s) - before_gets), (unsigned long long)expected);
    VERIFY(fol_gets(s) - before_gets == expected);
    if (s->fail_mode)
        VERIFY(fol_failure(s, collision) == 0);
    if (s->ray_mode) {
        VERIFY(mc_world_raytrace_retire(&s->ray) == MC_WORLD_RAYTRACE_OK && s->ray == NULL &&
               mc_entity_vector_release(f->entity_classes[0], s->start) == MC_ENTITY_OK &&
               mc_entity_vector_release(f->entity_classes[0], s->end) == MC_ENTITY_OK &&
               mc_entity_tick_execution_pop_world(s->execution, &s->root, s->fail_mode) ==
                   MC_WORLD_STORAGE_OK);
        s->start = s->end = NULL;
        if (s->fail_mode)
            VERIFY(mc_entity_tick_execution_retire(s->execution) == MC_WORLD_STORAGE_OK);
    } else {
        VERIFY(mc_arrow_tick_retire(&s->arrow_call) == MC_ARROW_TICK_OK && s->arrow_call == NULL &&
               s->base.arrow->arrow->tick_call == NULL && s->base.arrow->holds == s->arrow_holds &&
               f->classes[0]->native_borrows == s->code_borrows);
    }
    s->execution = NULL;
    s->result = NULL;
    VERIFY(f->worlds[0]->persistent_bindings == bindings + (install ? 1u : 0u) &&
           f->worlds[0]->active_calls == 0 && fol_map(s, 8, y, 8) == tile && tile->holds == 2 &&
           fol_source_same(s));
    if (s->fail_mode) {
        VERIFY(mc_world_access_receipt_retire(&s->receipt, NULL) == MC_WORLD_STORAGE_OK &&
               mc_world_access_free(s->access, NULL) == MC_WORLD_STORAGE_OK);
        s->access = NULL;
        free(s->exception_name);
        s->exception_name = NULL;
    }
    return 0;
}

static int fol_main(int argc, char **argv) {
    VERIFY(argc == 3 &&
           (strcmp(argv[1], "direct-success") == 0 || strcmp(argv[1], "direct-failure") == 0 ||
            strcmp(argv[1], "ray-success") == 0 || strcmp(argv[1], "ray-failure") == 0));
    printf("FURNACE_OWNED_LAZY_BEGIN schema=1 mode=%s\n", argv[1]);
    fflush(stdout);
    fol_fixture *s = calloc(1, sizeof(*s));
    VERIFY(s != NULL);
    s->ray_mode = strncmp(argv[1], "ray-", 4) == 0;
    s->fail_mode = strstr(argv[1], "failure") != NULL;
    s->base.cq.mode = argv[1];
    VERIFY(ss_setup(&s->base.cq.source, argv[2]) == 0 && cq_files(&s->base.cq, true) == 0 &&
           ss_open(&s->base.cq.source) == 0 &&
           mc_math_classes_create(SIZE_MAX, &s->base.math_owner, NULL) == MC_MATH_CLASS_OK);
    mc_math_execution_init(&s->base.math, s->base.math_owner);
    mc_entity_tick_execution_init(&s->base.cq.execution, &s->base.math);
    VERIFY(aol_prime(&s->base) == 0 && aol_arrow(&s->base, 8.5, 66.5, 8.5) == 0);
    fixture *f = &s->base.cq.source.game;
    const mc_java_random random = *f->worlds[0]->fields.r;
    s->arrow_holds = s->base.arrow->holds;
    s->code_borrows = f->classes[0]->native_borrows;
    s->bindings = f->worlds[0]->persistent_bindings;
    mc_tile_entity *piston = fol_map(s, 6, 66, 6);
    VERIFY(piston != NULL && piston->kind == MC_TILE_ENTITY_PISTON && f->chunks[0]->l->count == 1 &&
           aol_shape(&s->base, f->worlds[0], 36, 6, 66, 6, false) == 0);
    const mc_tile_entity piston_before = *piston;
    if (s->ray_mode)
        VERIFY(fol_second_graph(s) == 0);
    printf("FURNACE_OWNED_LAZY_CASE %s BEGIN\n", argv[1]);
    fflush(stdout);
    static const int32_t dx[] = {0, 0, -1, 1, -1};
    static const int32_t dz[] = {-1, 1, 0, 0, 0};
    static const int32_t facing[] = {3, 2, 5, 4, 3};
    const size_t cases = s->fail_mode ? 4 : 2 * COUNT(facing);
    for (size_t i = 0; i < cases; ++i) {
        if (s->fail_mode) {
            /* Separate real invocation after preceding checked retirement:
             * identical IO status without a producer Throwable is an explicit
             * native callback failure, never guessed exception or success. */
            s->native_failure = i % 2 == 1;
            s->callbacks = 0;
            s->removed = s->callback_valid = false;
            const mc_world_access_source_callbacks callbacks = {.cell = fol_cell_failure};
            VERIFY(mc_world_access_source_create(&callbacks, s, &s->access, NULL) ==
                       MC_WORLD_STORAGE_OK &&
                   mc_world_runtime_add_world_access(f->worlds[0], s->access, NULL) ==
                       MC_WORLD_STORAGE_OK);
            if (i != 0 && s->ray_mode)
                mc_entity_tick_execution_init(&s->base.cq.execution, &s->base.math);
        }
        const size_t orientation = i % COUNT(facing);
        const bool box = s->fail_mode ? i >= 2 : i >= COUNT(facing);
        const int32_t y = 72 + (int32_t)i * 2;
        s->metadata = facing[orientation];
        fol_cell(s, 8, y, 8, i % 2 == 0 ? 61 : 62);
        fol_cell(s, 8 + dx[orientation], y, 8 + dz[orientation], 1);
        if (orientation == 4)
            fol_cell(s, 9, y, 8, 1); /* Both opaque west/east retain default3. */
        VERIFY(fol_map(s, 8, y, 8) == NULL && fol_call(s, y, box, true) == 0);
        if (!s->fail_mode && (i == 0 || i == COUNT(facing))) {
            mc_tile_entity *same = fol_map(s, 8, y, 8);
            VERIFY(fol_call(s, y, !box, false) == 0 && fol_map(s, 8, y, 8) == same);
        }
    }
    VERIFY(fol_map(s, 6, 66, 6) == piston && piston->holds == piston_before.holds &&
           piston->world == piston_before.world && piston->x == piston_before.x &&
           piston->state.piston.block_id == piston_before.state.piston.block_id &&
           piston->state.piston.facing == piston_before.state.piston.facing &&
           piston->state.piston.progress_bits == piston_before.state.piston.progress_bits &&
           f->chunks[0]->l->count == 1 + s->installations &&
           f->worlds[0]->persistent_bindings == s->bindings + s->installations &&
           aol_random(f->worlds[0]->fields.r, &random) &&
           mc_entity_tick_execution_retire(&s->base.cq.execution) == MC_WORLD_STORAGE_OK &&
           mc_math_execution_retire(&s->base.math) == MC_MATH_CLASS_OK &&
           mc_math_classes_release(&s->base.math_owner, NULL) == MC_MATH_CLASS_OK);
    mc_entity_release(s->base.arrow);
    s->base.arrow = NULL;
    /* Source array inputs stay as reached throughout every checked cleanup. */
    if (s->ray_mode) {
        for (size_t i = 0; i < 2; ++i)
            f->registry[1]->blocks.m[61 + i] = s->saved_m[i];
        f->registry[1]->blocks.o[1] = s->saved_opacity;
        VERIFY(mc_world_runtime_classes_free(f->classes[1], NULL) == MC_WORLD_STORAGE_OK &&
               mc_registry_classes_free(f->registry[1], NULL) == MC_REGISTRY_OK);
    }
    printf("FURNACE_OWNED_LAZY_CASE %s END\n", argv[1]);
    fflush(stdout);
    VERIFY(ss_stop(&s->base.cq.source) == 0 && cq_files(&s->base.cq, false) == 0 &&
           ss_finish(&s->base.cq.source) == 0);
    for (size_t i = 0; i < COUNT(s->base.cq.inputs); ++i)
        mc_ws_free_file_bytes(&s->base.cq.inputs[i]);
    free(s);
    puts("FURNACE_OWNED_LAZY_OPEN original-race/full-Arrow/full-SL");
    printf("PASS: %u actual owned lazy Furnace checks;mode=%s\n", checks, argv[1]);
    return 0;
}
