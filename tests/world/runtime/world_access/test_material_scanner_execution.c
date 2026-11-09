/* Reuse the genuine public collision/SourceSaving fixture; never run its main. */
int sc_unused_query_main(int, char **);
#define MC_WORLD_COLLISION_QUERY_MAIN sc_unused_query_main
#include "test_collision_query_execution.c"
#undef MC_WORLD_COLLISION_QUERY_MAIN

#include "mc/entity/arrow/construction.h"
#include "mc/entity/bounds/bounds.h"
#include "world/runtime/material_movement_execution_internal.h"

/* Only the closed scanner modes below execute. The byte-pinned preceding
 * fixture helpers and renamed old mains are reused, never run as programs. */
typedef struct scan_fixture {
    cq_fixture cq;
    mc_math_classes *math_owner;
    mc_math_execution math;
    mc_world_material_movement_error error, probe_error;
    mc_world_material_scan_query query, probe_query;
    mc_world_runtime_execution_result result, probe_result;
    mc_entity_bounds *boxes[6];
    mc_entity *actor;
    size_t calls, callbacks;
    bool held_callback, callback_valid, scratch_valid;
    unsigned int scratch_pc;
} scan_fixture;

static bool sc_call(scan_fixture *s, mc_world_material_scan_kind kind, const mc_entity_bounds *box,
                    mc_material *material, mc_entity *actor, bool *out) {
    ++s->calls;
    mc_world_material_movement_error_init(&s->error);
    return mc_world_runtime_material_scan_in_execution(
        &s->cq.execution, s->cq.source.game.classes[0], s->cq.source.game.worlds[0], cq_code, box,
        material, actor, kind, NULL, out, &s->error, &s->query, &s->result);
}

static int sc_push(scan_fixture *s, mc_entity_tick_execution_world_token *token) {
    VERIFY(mc_entity_tick_execution_push_world(&s->cq.execution, s->cq.source.game.worlds[0],
                                               cq_site, token) == MC_WORLD_STORAGE_OK);
    return 0;
}

static int sc_value(scan_fixture *s, mc_world_material_scan_kind kind, size_t box,
                    mc_material_id material_id, mc_entity *actor, bool expected) {
    fixture *f = &s->cq.source.game;
    mc_entity_tick_execution_world_token token;
    VERIFY(sc_push(s, &token) == 0);
    unsigned char random[sizeof(*f->worlds[0]->fields.r)];
    memcpy(random, f->worlds[0]->fields.r, sizeof(random));
    bool value = !expected;
    mc_material *material =
        kind == MC_WORLD_MATERIAL_SCAN_LIQUID ? NULL : mc_materials_get(f->materials, material_id);
    VERIFY(sc_call(s, kind, s->boxes[box], material, actor, &value) && value == expected &&
           s->error.status == MC_WORLD_STORAGE_OK && s->error.bytecode == 0 &&
           s->result.ownership_status == MC_WORLD_STORAGE_OK && !s->result.selection_ready &&
           !cq_pending(&s->result) && mg_closes_empty(&s->result) &&
           memcmp(random, f->worlds[0]->fields.r, sizeof(random)) == 0 &&
           mc_entity_tick_execution_pop_world(&s->cq.execution, &token, false) ==
               MC_WORLD_STORAGE_OK);
    return 0;
}

static int sc_match(scan_fixture *s) {
    fixture *f = &s->cq.source.game;
    VERIFY(f->providers[0].cache->count == 0 &&
           sc_value(s, MC_WORLD_MATERIAL_SCAN_MATCH, 0, MC_MATERIAL_e, NULL, true) == 0 &&
           cq_remember(&s->cq) == 0 && s->query.leaf == MC_WORLD_MATERIAL_SCAN_SCALAR_LEAF &&
           s->query.returned && s->query.status == MC_WORLD_STORAGE_OK);
    mc_world_runtime_source_construction_state construction;
    mc_world_source_chunk_loader_state before;
    VERIFY(mc_world_runtime_source_construction_inspect(f->worlds[0], &construction) &&
           mc_world_source_chunk_loader_inspect(construction.loader, &before) &&
           sc_value(s, MC_WORLD_MATERIAL_SCAN_MATCH, 0, MC_MATERIAL_e, NULL, true) == 0 &&
           sc_value(s, MC_WORLD_MATERIAL_SCAN_MATCH, 1, MC_MATERIAL_a, NULL, false) == 0 &&
           cq_loader_same(&s->cq, &before) && f->registry[0]->blocks.m[0] == NULL &&
           f->providers[0].cache->count == 1 && f->providers[0].cache->insertion_count == 1);
    return 0;
}

static int sc_liquid(scan_fixture *s) {
    fixture *f = &s->cq.source.game;
    mc_registry_classes *registry = f->registry[0];
    mc_block *failed = NULL;
    VERIFY(registry->entered == 0 && registry->blocks_phase == MC_REGISTRY_READY &&
           registry->block_states[MC_BLOCK_GLOWSTONE].phase == MC_REGISTRY_READY &&
           registry->blocks.m[200] == NULL &&
           mc_registry_block_allocate(registry, MC_BLOCK_GLOWSTONE, &failed) == MC_REGISTRY_OK);
    const mc_block_arguments arguments = {.id = 200, .texture = 105, .material = NULL};
    VERIFY(mc_registry_block_construct(registry, failed, &arguments) ==
               MC_REGISTRY_NULL_REFERENCE &&
           failed->phase == MC_BLOCK_CONSTRUCTION_FAILED && failed->constructor_pc == 148 &&
           failed->fields.bA == NULL && registry->blocks.m[200] == failed &&
           registry->entered == 0 && registry->blocks_phase == MC_REGISTRY_READY);
    VERIFY(sc_value(s, MC_WORLD_MATERIAL_SCAN_LIQUID, 2, MC_MATERIAL_a, NULL, true) == 0 &&
           cq_remember(&s->cq) == 0 &&
           sc_value(s, MC_WORLD_MATERIAL_SCAN_LIQUID, 0, MC_MATERIAL_a, NULL, false) == 0);
    mc_entity_tick_execution_world_token token;
    VERIFY(sc_push(s, &token) == 0);
    bool value = true;
    VERIFY(!sc_call(s, MC_WORLD_MATERIAL_SCAN_LIQUID, s->boxes[3], NULL, NULL, &value) && value &&
           s->error.bytecode == 151 && s->error.registry.status == MC_REGISTRY_NULL_REFERENCE &&
           s->result.ownership_status == MC_WORLD_STORAGE_OK && !cq_pending(&s->result) &&
           s->query.leaf == MC_WORLD_MATERIAL_SCAN_SCALAR_LEAF && s->query.returned &&
           s->query.status == MC_WORLD_STORAGE_OK &&
           f->states[0]->data->blocks[cell(8, 67, 8)] == 200 && failed->fields.bA == NULL &&
           mc_entity_tick_execution_pop_world(&s->cq.execution, &token, true) ==
               MC_WORLD_STORAGE_OK);
    return 0;
}

static int sc_accelerate(scan_fixture *s) {
    fixture *f = &s->cq.source.game;
    VERIFY(f->providers[0].cache->count == 0 &&
           sc_value(s, MC_WORLD_MATERIAL_SCAN_ACCELERATE, 0, MC_MATERIAL_e, NULL, false) == 0 &&
           s->query.leaf == MC_WORLD_MATERIAL_SCAN_NO_LEAF && !s->query.returned &&
           f->providers[0].cache->count == 0 &&
           sc_value(s, MC_WORLD_MATERIAL_SCAN_MATCH, 0, MC_MATERIAL_e, NULL, true) == 0 &&
           cq_remember(&s->cq) == 0 &&
           sc_value(s, MC_WORLD_MATERIAL_SCAN_ACCELERATE, 0, MC_MATERIAL_e, NULL, true) == 0);
    VERIFY(mc_arrow_allocate(f->entity_classes[0], &s->actor, NULL) == MC_ENTITY_OK &&
           mc_arrow_construct_world(s->actor, f->worlds[0], NULL) == MC_ENTITY_OK &&
           s->actor->phase == MC_ENTITY_READY && s->actor->holds == 1);
    const mc_entity_base_fields before = s->actor->base;
    unsigned char random_before[sizeof(*s->actor->random)];
    memcpy(random_before, s->actor->random, sizeof(random_before));
    VERIFY(sc_value(s, MC_WORLD_MATERIAL_SCAN_ACCELERATE, 4, MC_MATERIAL_g, s->actor, true) == 0);
    const double dx = s->actor->base.velocity_x - before.velocity_x;
    const double dy = s->actor->base.velocity_y - before.velocity_y;
    const double dz = s->actor->base.velocity_z - before.velocity_z;
    /* Independently analytic finite gradient: equal positive X/Z, zero Y,
     * normalized magnitude0.014. This is not an original raw-bit oracle. */
    VERIFY(dx > 0.0098 && dx < 0.0100 && dx == dz && dy == 0.0 && dx * dx + dz * dz > 0.00019599 &&
           dx * dx + dz * dz < 0.00019601 && s->actor->base.x == before.x &&
           s->actor->base.y == before.y && s->actor->base.z == before.z && s->actor->holds == 1 &&
           memcmp(random_before, s->actor->random, sizeof(random_before)) == 0 &&
           s->query.returned && s->query.status == MC_WORLD_STORAGE_OK &&
           f->providers[0].cache->count == 1 && f->providers[0].cache->insertion_count == 1);
    mc_entity_release(s->actor);
    s->actor = NULL;
    return 0;
}

static void sc_native_probe(scan_fixture *s) {
    fixture *f = &s->cq.source.game;
    cq_scope before;
    mc_math_class_state math_before = {0}, math_after = {0};
    const bool snapped =
        cq_scope_take(&s->cq, &before) &&
        mc_math_classes_state(s->math_owner, &math_before, NULL) == MC_MATH_CLASS_OK;
    bool out = true;
    ++s->calls;
    const bool completed = mc_world_runtime_material_scan_in_execution(
        &s->cq.execution, f->classes[0], f->worlds[0], cq_code, NULL, NULL, NULL,
        MC_WORLD_MATERIAL_SCAN_ACCELERATE, NULL, &out, &s->probe_error, &s->probe_query,
        &s->probe_result);
    s->callback_valid &=
        snapped && !completed && out && s->probe_result.ownership_status == MC_WORLD_STORAGE_BUSY &&
        s->probe_error.bytecode == 0 && s->probe_query.leaf == MC_WORLD_MATERIAL_SCAN_NO_LEAF &&
        !s->probe_query.returned && cq_scope_same(&s->cq, &before) &&
        mc_math_classes_state(s->math_owner, &math_after, NULL) == MC_MATH_CLASS_OK &&
        math_before.phase == math_after.phase && math_before.holds == math_after.holds &&
        math_before.source_a == math_after.source_a;
}

static mc_world_storage_status sc_dirty(void *context, mc_world_runtime *world, int32_t x,
                                        int32_t y, int32_t z, mc_tile_entity *tile,
                                        mc_world_storage_error *cause,
                                        mc_world_access_failure *failure) {
    scan_fixture *s = context;
    ++s->callbacks;
    s->cq.node = mc_entity_tick_execution_world_lease(&s->cq.execution);
    s->callback_valid &= world == s->cq.source.game.worlds[0] && x == 8 && y == 64 && z == 8 &&
                         tile == NULL && s->cq.node != NULL &&
                         s->cq.node->phase == MC_WORLD_ACCESS_FAILURE_LEASE_ARMED;
    if (!s->held_callback) {
        sc_native_probe(s);
        mc_entity_tick_execution_world_token nested;
        const mc_world_storage_status pushed =
            mc_entity_tick_execution_push_world(&s->cq.execution, world, cq_site, &nested);
        s->callback_valid &= pushed == MC_WORLD_STORAGE_OK;
        if (pushed != MC_WORLD_STORAGE_OK)
            return pushed;
        bool value = true;
        const bool completed =
            sc_call(s, MC_WORLD_MATERIAL_SCAN_MATCH, s->boxes[5], NULL, NULL, &value);
        s->callback_valid &= completed && !value && s->error.cell_work == 0 &&
                             s->query.leaf == MC_WORLD_MATERIAL_SCAN_NO_LEAF &&
                             s->result.ownership_status == MC_WORLD_STORAGE_OK;
        const mc_world_storage_status popped =
            mc_entity_tick_execution_pop_world(&s->cq.execution, &nested, !completed);
        s->callback_valid &= popped == MC_WORLD_STORAGE_OK;
        return popped;
    }
    bool removed = false;
    mc_world_storage_status status =
        mc_world_runtime_remove_world_access(world, s->cq.receiver, &removed, cause);
    s->cq.removed = status == MC_WORLD_STORAGE_OK && removed;
    if (!s->cq.removed)
        return status == MC_WORLD_STORAGE_OK ? MC_WORLD_STORAGE_UNSUPPORTED_STATE : status;
    status = mc_world_access_receipt_create(s->cq.receiver, NULL, &s->cq.receipt, failure, cause);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    s->cq.receipt->exception_class = "java/lang/IllegalStateException";
    *cause =
        (mc_world_storage_error){MC_WORLD_STORAGE_IO_ERROR, "Scanner real callback failure", 73};
    return cause->status;
}

static int sc_nodes(scan_fixture *s, bool held) {
    fixture *f = &s->cq.source.game;
    s->held_callback = held;
    s->callback_valid = true;
    const mc_world_access_source_callbacks callbacks = {.tile_entity_dirty = sc_dirty};
    VERIFY(mc_world_access_source_create(&callbacks, s, &s->cq.receiver, NULL) ==
               MC_WORLD_STORAGE_OK &&
           mc_world_runtime_add_world_access(f->worlds[0], s->cq.receiver, NULL) ==
               MC_WORLD_STORAGE_OK);
    mc_entity_tick_execution_world_token outer;
    VERIFY(sc_push(s, &outer) == 0);
    const mc_world_storage_status status = mc_world_runtime_tile_entity_dirty_in_execution(
        &s->cq.execution, f->worlds[0], cq_code, 8, 64, 8, NULL, &s->cq.dirty, &s->cq.result);
    if (held) {
        VERIFY(status == MC_WORLD_STORAGE_IO_ERROR && s->cq.removed && s->cq.receipt != NULL);
        const mc_world_access_failure *view = NULL;
        VERIFY(mc_world_access_failure_lease_inspect(s->cq.node, &view) &&
               view->receipt == s->cq.receipt && s->cq.receiver->active_holds == 1);
        sc_native_probe(s);
        VERIFY(s->callback_valid &&
               mc_world_access_receipt_retire(&s->cq.receipt, NULL) == MC_WORLD_STORAGE_BUSY &&
               mc_entity_tick_execution_pop_world(&s->cq.execution, &outer, true) ==
                   MC_WORLD_STORAGE_OK);
        s->cq.dirty.world_access_failure = (mc_world_access_failure){0};
        s->cq.result = (mc_world_runtime_execution_result){0};
        view = NULL;
        VERIFY(mc_entity_tick_execution_retire(&s->cq.execution) == MC_WORLD_STORAGE_OK &&
               mc_world_access_receipt_retire(&s->cq.receipt, NULL) == MC_WORLD_STORAGE_OK);
    } else {
        VERIFY(status == MC_WORLD_STORAGE_OK && s->callback_valid &&
               mc_entity_tick_execution_pop_world(&s->cq.execution, &outer, false) ==
                   MC_WORLD_STORAGE_OK);
        bool removed = false;
        VERIFY(mc_world_runtime_remove_world_access(f->worlds[0], s->cq.receiver, &removed, NULL) ==
                   MC_WORLD_STORAGE_OK &&
               removed);
    }
    VERIFY(mc_world_access_free(s->cq.receiver, NULL) == MC_WORLD_STORAGE_OK);
    s->cq.receiver = NULL;
    s->cq.node = NULL;
    return 0;
}

static int sc_admission(scan_fixture *s) {
    for (unsigned int probe = 0; probe < 2; ++probe) {
        mc_math_execution absent;
        mc_math_execution_init(&absent, NULL);
        mc_entity_tick_execution_init(&s->cq.execution, &absent);
        mc_entity_tick_execution_world_token outer;
        VERIFY(sc_push(s, &outer) == 0);
        bool out = true;
        VERIFY(!sc_call(s, MC_WORLD_MATERIAL_SCAN_MATCH, probe == 0 ? NULL : s->boxes[0], NULL,
                        NULL, &out) &&
               out && s->result.ownership_status == MC_WORLD_STORAGE_OK &&
               s->query.leaf == MC_WORLD_MATERIAL_SCAN_NO_LEAF && !s->query.returned &&
               s->error.bytecode == (probe == 0 ? 1u : 4u) && absent.owned == NULL &&
               absent.failed == (probe != 0) &&
               mc_entity_tick_execution_pop_world(&s->cq.execution, &outer, true) ==
                   MC_WORLD_STORAGE_OK &&
               mc_entity_tick_execution_retire(&s->cq.execution) == MC_WORLD_STORAGE_OK &&
               mc_math_execution_retire(&absent) == MC_MATH_CLASS_OK);
    }
    mc_entity_tick_execution_init(&s->cq.execution, &s->math);
    VERIFY(sc_value(s, MC_WORLD_MATERIAL_SCAN_MATCH, 0, MC_MATERIAL_e, NULL, true) == 0 &&
           cq_remember(&s->cq) == 0 && sc_nodes(s, false) == 0 && sc_nodes(s, true) == 0 &&
           s->callbacks == 2);
    return 0;
}

static mc_world_storage_status sc_added(void *context, mc_world_runtime *world, mc_entity *entity,
                                        mc_world_storage_error *cause,
                                        mc_world_access_failure *failure) {
    (void)cause;
    (void)failure;
    scan_fixture *s = context;
    ++s->callbacks;
    s->scratch_pc = s->error.bytecode;
    mc_world_access_failure_lease *node = mc_entity_tick_execution_world_lease(&s->cq.execution);
    s->scratch_valid &= world == s->cq.source.game.worlds[0] && entity != NULL &&
                        mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_ITEM) && s->callbacks == 1 &&
                        s->scratch_pc == 135 && node != NULL &&
                        node->phase == MC_WORLD_ACCESS_FAILURE_LEASE_ARMED &&
                        s->query.leaf == MC_WORLD_MATERIAL_SCAN_SCALAR_LEAF && !s->query.returned;
    /* Passive observation only. On packet1 red, normal Source execution and
     * checked graph teardown still finish before main asserts scratch_valid. */
    return MC_WORLD_STORAGE_OK;
}

static int sc_scratch(scan_fixture *s) {
    fixture *f = &s->cq.source.game;
    s->scratch_valid = true;
    const mc_world_access_source_callbacks callbacks = {.entity_added = sc_added};
    VERIFY(mc_world_access_source_create(&callbacks, s, &s->cq.receiver, NULL) ==
               MC_WORLD_STORAGE_OK &&
           mc_world_runtime_add_world_access(f->worlds[0], s->cq.receiver, NULL) ==
               MC_WORLD_STORAGE_OK &&
           sc_value(s, MC_WORLD_MATERIAL_SCAN_LIQUID, 0, MC_MATERIAL_a, NULL, false) == 0 &&
           cq_remember(&s->cq) == 0 && s->callbacks == 1 && f->worlds[0]->fields.b->count == 1 &&
           f->worlds[0]->fields.c->count == 1);
    bool removed = false;
    VERIFY(mc_world_runtime_remove_world_access(f->worlds[0], s->cq.receiver, &removed, NULL) ==
               MC_WORLD_STORAGE_OK &&
           removed && mc_world_access_free(s->cq.receiver, NULL) == MC_WORLD_STORAGE_OK);
    s->cq.receiver = NULL;
    return 0;
}

static int sc_loader(scan_fixture *s) {
    fixture *f = &s->cq.source.game;
    mc_entity_tick_execution_world_token outer;
    VERIFY(sc_push(s, &outer) == 0);
    bool out = true;
    VERIFY(
        !sc_call(s, MC_WORLD_MATERIAL_SCAN_MATCH, s->boxes[0],
                 mc_materials_get(f->materials, MC_MATERIAL_e), NULL, &out) &&
        out && s->query.leaf == MC_WORLD_MATERIAL_SCAN_SCALAR_LEAF && s->query.returned &&
        s->query.status == MC_WORLD_STORAGE_INVALID_DATA && s->error.bytecode == 101 &&
        s->result.ownership_status == MC_WORLD_STORAGE_OK && s->result.selection_ready &&
        !cq_pending(&s->result) && s->result.selection.status == MC_CHUNK_ACQUIRE_CALLBACK_FAILED &&
        mc_entity_tick_execution_loader_read(&s->cq.execution, &s->result.selection, &s->cq.view) &&
        s->cq.view.failure != NULL &&
        s->error.block.nearby.query.source_chunk == s->cq.view.failure);
    mc_world_runtime_source_construction_state construction;
    mc_world_source_chunk_loader_state producer;
    VERIFY(
        mc_world_runtime_source_construction_inspect(f->worlds[0], &construction) &&
        mc_world_source_chunk_loader_inspect(construction.loader, &producer) &&
        producer.failure != NULL && producer.phase == MC_WORLD_SOURCE_FAILED &&
        producer.last_pc == 18 && s->cq.view.failure != producer.failure &&
        kl_error_same(s->cq.view.failure, producer.failure) &&
        mc_entity_tick_execution_pop_world(&s->cq.execution, &outer, true) == MC_WORLD_STORAGE_OK &&
        mc_world_runtime_free(f->worlds[0], NULL) == MC_WORLD_STORAGE_BUSY &&
        mc_entity_tick_execution_loader_read(&s->cq.execution, &s->result.selection, &s->cq.view) &&
        kl_error_same(s->cq.view.failure, producer.failure));
    s->error.block.nearby.query.source_chunk = NULL;
    s->cq.view = (mc_entity_tick_execution_loader_view){0};
    s->result = (mc_world_runtime_execution_result){0};
    return 0;
}

static bool sc_mode(const char *mode) {
    return strcmp(mode, "match") == 0 || strcmp(mode, "liquid") == 0 ||
           strcmp(mode, "accelerate") == 0 || strcmp(mode, "admission") == 0 ||
           strcmp(mode, "scratch") == 0 || strcmp(mode, "loader-error") == 0;
}

int main(int argc, char **argv) {
    VERIFY(argc == 3 && sc_mode(argv[1]));
    printf("MATERIAL_SCAN_BEGIN schema=1 mode=%s\n", argv[1]);
    fflush(stdout);
    scan_fixture *s = calloc(1, sizeof(*s));
    VERIFY(s != NULL);
    s->cq.mode = argv[1];
    VERIFY(ss_setup(&s->cq.source, argv[2]) == 0 && cq_files(&s->cq, true) == 0 &&
           ss_open(&s->cq.source) == 0 &&
           mc_math_classes_create(SIZE_MAX, &s->math_owner, NULL) == MC_MATH_CLASS_OK);
    mc_math_execution_init(&s->math, s->math_owner);
    mc_entity_tick_execution_init(&s->cq.execution, &s->math);
    static const double coordinates[6][6] = {{8, 64, 8, 8, 64, 8},     {8, 68, 8, 8, 68, 8},
                                             {8, 66, 8, 8, 66, 8},     {8, 67, 8, 8, 67, 8},
                                             {10, 70, 10, 10, 70, 10}, {8, 64, 8, 6, 62, 6}};
    for (size_t i = 0; i < COUNT(s->boxes); ++i)
        VERIFY(mc_entity_bounds_create(s->cq.source.game.entity_classes[0], coordinates[i][0],
                                       coordinates[i][1], coordinates[i][2], coordinates[i][3],
                                       coordinates[i][4], coordinates[i][5], &s->boxes[i],
                                       NULL) == MC_ENTITY_OK);
    printf("MATERIAL_SCAN_CASE %s BEGIN\n", argv[1]);
    fflush(stdout);
    const int status = strcmp(argv[1], "match") == 0        ? sc_match(s)
                       : strcmp(argv[1], "liquid") == 0     ? sc_liquid(s)
                       : strcmp(argv[1], "accelerate") == 0 ? sc_accelerate(s)
                       : strcmp(argv[1], "admission") == 0  ? sc_admission(s)
                       : strcmp(argv[1], "scratch") == 0    ? sc_scratch(s)
                                                            : sc_loader(s);
    VERIFY(status == 0);
    const size_t expected_calls = strcmp(argv[1], "match") == 0        ? 3u
                                  : strcmp(argv[1], "liquid") == 0     ? 3u
                                  : strcmp(argv[1], "accelerate") == 0 ? 4u
                                  : strcmp(argv[1], "admission") == 0  ? 6u
                                                                       : 1u;
    VERIFY(s->calls == expected_calls);
    const bool scratch_ok = strcmp(argv[1], "scratch") != 0 || s->scratch_valid;
    const unsigned int observed_pc = s->scratch_pc;
    const size_t calls = s->calls;
    mc_world_material_movement_error_init(&s->error);
    mc_world_material_movement_error_init(&s->probe_error);
    s->result = s->probe_result = (mc_world_runtime_execution_result){0};
    VERIFY(mc_entity_tick_execution_retire(&s->cq.execution) == MC_WORLD_STORAGE_OK &&
           mc_math_execution_retire(&s->math) == MC_MATH_CLASS_OK &&
           mc_math_classes_release(&s->math_owner, NULL) == MC_MATH_CLASS_OK);
    for (size_t i = 0; i < COUNT(s->boxes); ++i)
        VERIFY(mc_entity_bounds_release(s->cq.source.game.entity_classes[0], s->boxes[i]) ==
               MC_ENTITY_OK);
    printf("MATERIAL_SCAN_CASE %s END calls=%zu\n", argv[1], calls);
    fflush(stdout);
    VERIFY((s->cq.source.game.states[0] == NULL ? cq_empty_stop(&s->cq.source)
                                                : ss_stop(&s->cq.source)) == 0 &&
           cq_files(&s->cq, false) == 0 && ss_finish(&s->cq.source) == 0);
    for (size_t i = 0; i < COUNT(s->cq.inputs); ++i)
        mc_ws_free_file_bytes(&s->cq.inputs[i]);
    free(s);
    if (!scratch_ok) {
        fprintf(stderr,
                "Caller-stable scanner scratch: expected135 observed%u; checked cleanup complete\n",
                observed_pc);
        return 1;
    }
    printf("MATERIAL_SCAN_OPEN GET-OK-native-completion/foreign-graph/Base/full-SL\n");
    printf("PASS: %u actual owned scanner checks;mode=%s\n", checks, argv[1]);
    return 0;
}
