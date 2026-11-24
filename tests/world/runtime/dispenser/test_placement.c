/* Authored native XQ controls over real GAME/Store/Chunk/Tile owners. Loaded
 * cell prestates are declared before calls. The imported fixture main is never
 * run; no Source callee, Entity phase or returned result is replaced. */
int mc_dispenser_placement_unused_boxes_main(int, char **);
#define MC_COLLISION_BOXES_MAIN mc_dispenser_placement_unused_boxes_main
#include "../collision/test_boxes.c"
#undef MC_COLLISION_BOXES_MAIN

#include "../../../../src/core/math/execution_internal.h"
#include "../../../../src/world/runtime/blocks_execution_internal.h"
#include "../../../../src/world/runtime/mutation/dispenser/dispenser_internal.h"
#include "../../../../src/world/runtime/mutation/mutation_internal.h"
#include "../../../../src/world/runtime/tile_entities/tile_entities_internal.h"
#include "../../../../src/world/runtime/world_access_internal.h"
#include "mc/block/material/material.h"
#include "mc/world/runtime/tile_entities/dispenser.h"

typedef struct placement_observer {
    fixture *game;
    int32_t x, y, z, metadata;
    size_t calls;
    bool valid;
} placement_observer;

static bool placement_metadata(fixture *f, int32_t x, int32_t y, int32_t z, int32_t expected) {
    uint8_t actual = 255;
    return mc_chunk_nibbles_get(f->states[0]->data->metadata, MC_CHUNK_NIBBLE_BYTES, x, y, z,
                                &actual) &&
           actual == expected;
}

static mc_world_storage_status placement_cell(void *context, mc_world_runtime *world, int32_t x,
                                              int32_t y, int32_t z, mc_world_storage_error *cause) {
    placement_observer *o = context;
    ++o->calls;
    o->valid = o->valid && world == o->game->worlds[0] && x == o->x && y == o->y && z == o->z &&
               placement_metadata(o->game, x, y, z, o->metadata);
    if (!o->valid) {
        *cause = (mc_world_storage_error){MC_WORLD_STORAGE_INVALID_DATA,
                                          "XQ actual metadata notification prefix", 0};
        return cause->status;
    }
    return MC_WORLD_STORAGE_OK;
}

static bool placement_added(fixture *f, int32_t x, int32_t y, int32_t z,
                            mc_world_mutation_error *error) {
    if (mc_world_runtime_call_enter(f->worlds[0], NULL) != MC_WORLD_STORAGE_OK)
        return false;
    mc_world_mutation_error_init(error);
    const bool called =
        mc_world_mutation_block_added(f->worlds[0], f->registry[0]->blocks.m[23], x, y, z, error);
    mc_world_runtime_call_leave(f->worlds[0]);
    return called;
}

static bool placement_tile(fixture *f, int32_t x, int32_t y, int32_t z) {
    bool ok = false;
    mc_tile_entity *tile = NULL;
    mc_tile_entity_snapshot snapshot;
    CHECK(mc_live_tile_entities_get(f->worlds[0], x, y, z, &tile, NULL) == MC_WORLD_STORAGE_OK &&
          tile != NULL && mc_tile_entity_get_snapshot(tile, &snapshot) == MC_TILE_ENTITY_OK &&
          snapshot.kind == MC_TILE_ENTITY_DISPENSER && snapshot.phase == MC_TILE_ENTITY_READY &&
          snapshot.world == f->worlds[0] && snapshot.x == x && snapshot.y == y && snapshot.z == z &&
          !snapshot.invalid && snapshot.dispenser_capacity == 9 &&
          snapshot.dispenser_random_constructed);
    for (int32_t i = 0; i < 9; ++i) {
        mc_item_stack *stack = NULL;
        const mc_tile_entity_status status = mc_tile_entity_dispenser_get(tile, i, &stack);
        const bool empty = status == MC_TILE_ENTITY_OK && stack == NULL;
        mc_item_stack_release(stack);
        CHECK(empty);
    }
    ok = true;
cleanup:
    mc_tile_entity_release(tile);
    return ok;
}

static bool placement_combinations(fixture *f) {
    bool ok = false, registered = false;
    mc_world_runtime *world = f->worlds[0];
    mc_world_mutation_error *error = malloc(sizeof(*error));
    mc_world_access *access = NULL;
    placement_observer observer = {.game = f, .valid = true};
    const mc_world_access_callbacks callbacks = {.cell = placement_cell};
    /* Bits are north/south/west/east. These independent expected results
     * distinguish the X-over-Z assignments from an else-if implementation. */
    static const int32_t expected[16] = {3, 3, 2, 3, 5, 5, 5, 5, 4, 4, 4, 4, 3, 3, 2, 3};
    const mc_java_random block_random = *f->registry[0]->blocks.m[23]->subtype.xq.a;
    mc_java_random_constructor_records_view before, after;
    CHECK(error != NULL && f->registry[0]->blocks.o[1] && !f->registry[0]->blocks.o[20] &&
          f->registry[0]->blocks.t[23] && mc_material_a(f->registry[0]->blocks.m[20]->fields.bA));
    /* All loaded cells are authored before the first actual XQ addition. */
    boxes_air(f, 0);
    for (unsigned int mask = 0; mask < 16; ++mask) {
        const int32_t x = 2 + (int32_t)(mask & 3u) * 4;
        const int32_t z = 2 + (int32_t)(mask >> 2u) * 4;
        CHECK(boxes_cell(f, 0, x, 64, z, 23, 0) &&
              boxes_cell(f, 0, x, 64, z - 1, (mask & 1u) != 0 ? 1 : 0, 0) &&
              boxes_cell(f, 0, x, 64, z + 1, (mask & 2u) != 0 ? 1 : 0, 0) &&
              boxes_cell(f, 0, x - 1, 64, z, (mask & 4u) != 0 ? 1 : 0, 0) &&
              boxes_cell(f, 0, x + 1, 64, z, (mask & 8u) != 0 ? 1 : 0, 0));
    }
    /* Glass's solid Material must not substitute for the actual UU.o entry.
     * A south Stone alone selected2 above; south Glass must leave default3. */
    CHECK(boxes_cell(f, 0, 8, 66, 8, 23, 0) && boxes_cell(f, 0, 8, 66, 9, 20, 0) &&
          boxes_cell(f, 0, 8, 68, 8, 23, 7) &&
          mc_world_access_create(&callbacks, &observer, &access, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_add_world_access(world, access, NULL) == MC_WORLD_STORAGE_OK);
    registered = true;
    for (unsigned int mask = 0; mask < 16; ++mask) {
        observer.x = 2 + (int32_t)(mask & 3u) * 4;
        observer.y = 64;
        observer.z = 2 + (int32_t)(mask >> 2u) * 4;
        observer.metadata = expected[mask];
        const size_t active = world->fields.c->count;
        CHECK(mc_java_random_profile_constructor_records_get(f->profile, &before) ==
              MC_JAVA_RANDOM_PROFILE_OK);
        CHECK(placement_added(f, observer.x, 64, observer.z, error) &&
              error->status == MC_WORLD_STORAGE_OK && error->hook_class == NULL && observer.valid &&
              observer.calls == (size_t)mask + 1u &&
              placement_metadata(f, observer.x, 64, observer.z, expected[mask]) &&
              world->fields.c->count == active + 1u &&
              placement_tile(f, observer.x, 64, observer.z));
        CHECK(mc_java_random_profile_constructor_records_get(f->profile, &after) ==
                  MC_JAVA_RANDOM_PROFILE_OK &&
              after.recorded == before.recorded + 1u && after.failures == 0 &&
              memcmp(&block_random, f->registry[0]->blocks.m[23]->subtype.xq.a,
                     sizeof(block_random)) == 0);
    }
    observer.x = observer.z = 8;
    observer.y = 66;
    observer.metadata = 3;
    CHECK(placement_added(f, 8, 66, 8, error) && observer.valid && observer.calls == 17 &&
          placement_metadata(f, 8, 66, 8, 3) && placement_tile(f, 8, 66, 8));

    const size_t active = world->fields.c->count;
    /* Public World.B is an explicit entry prestate. RW must still install AZ;
     * only the private orientation returns7 without reading/writing neighbors. */
    world->fields.B = true;
    const bool remote_added = placement_added(f, 8, 68, 8, error);
    world->fields.B = false;
    CHECK(remote_added && error->status == MC_WORLD_STORAGE_OK && observer.calls == 17 &&
          placement_metadata(f, 8, 68, 8, 7) && world->fields.c->count == active + 1u &&
          placement_tile(f, 8, 68, 8));
    ok = true;
cleanup:
    world->fields.B = false;
    if (registered) {
        bool removed = false;
        if (mc_world_runtime_remove_world_access(world, access, &removed, NULL) !=
                MC_WORLD_STORAGE_OK ||
            !removed)
            ok = false;
    }
    if (mc_world_access_free(access, NULL) != MC_WORLD_STORAGE_OK)
        ok = false;
    free(error);
    return ok;
}

static bool placement_query_controls(fixture *f) {
    bool ok = false;
    mc_world_runtime *world = f->worlds[0];
    mc_world_mutation_error *error = malloc(sizeof(*error));
    mc_chunk_provider *const provider = world->fields.v;
    CHECK(error != NULL);
    mc_world_mutation_error_init(error);
    CHECK(!mc_world_mutation_dispenser_orient(NULL, f->registry[0]->blocks.m[23], 8, 70, 8, error,
                                              NULL) &&
          error->dispenser_source_null && error->hook_bytecode == 1 &&
          strcmp(error->hook_class, "xq") == 0 && error->cause_hook_class == NULL);
    CHECK(mc_world_runtime_call_enter(world, NULL) == MC_WORLD_STORAGE_OK);
    mc_world_mutation_error_init(error);
    /* This private-body control supplies an actual nullable public provider
     * prestate. The first acquiring FD.a15 fails; no later h read/write runs. */
    world->fields.v = NULL;
    const bool queried = mc_world_mutation_dispenser_orient(world, f->registry[0]->blocks.m[23], 8,
                                                            70, 8, error, NULL);
    world->fields.v = provider; /* Native fixture view ends; never a Source retry. */
    mc_world_runtime_call_leave(world);
    CHECK(!queried && error->status == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          strcmp(error->hook_class, "xq") == 0 && error->hook_bytecode == 15 &&
          strcmp(error->cause_hook_class, "xq") == 0 && error->cause_hook_bytecode == 15 &&
          error->query.source_null && error->query.bytecode == 48 &&
          error->query.provider_bytecode == 6 && !error->dispenser_source_null &&
          placement_metadata(f, 8, 70, 8, 0));
    /* Both absolute coordinate extremes exercise Java wrapped neighbor math.
     * The genuine World bounds return air; FD.d's bounded setter returns with
     * no acquisition, callback or in-range cell mutation. */
    const size_t active = world->fields.c->count;
    for (size_t i = 0; i < 2; ++i) {
        const int32_t coordinate = i == 0 ? INT32_MIN : INT32_MAX;
        CHECK(mc_world_runtime_call_enter(world, NULL) == MC_WORLD_STORAGE_OK);
        mc_world_mutation_error_init(error);
        const bool completed = mc_world_mutation_dispenser_orient(
            world, f->registry[0]->blocks.m[23], coordinate, 64, coordinate, error, NULL);
        mc_world_runtime_call_leave(world);
        CHECK(completed && error->status == MC_WORLD_STORAGE_OK && error->hook_class == NULL &&
              world->fields.c->count == active && placement_metadata(f, 8, 70, 8, 0));
    }
    ok = true;
cleanup:
    world->fields.v = provider;
    free(error);
    return ok;
}

typedef struct placement_execution {
    fixture *game;
    mc_math_classes *math_owner;
    mc_math_execution math;
    mc_entity_tick_execution execution;
    mc_entity_tick_execution_world_token root, external;
    mc_world_runtime_execution_result result;
    mc_world_mutation_execution_call *call;
    mc_world_mutation_error error;
    mc_world_access *access;
    mc_world_access_receipt *receipt;
    char *exception_name;
    size_t callbacks;
    bool valid, root_open, external_open, registered;
} placement_execution;

static const mc_entity_tick_execution_site placement_site = {"native.dispenser.control", "add",
                                                             "()V", 0};

static const mc_world_runtime_execution_code placement_code = {{"lm", "e", "()V", 6},
                                                               {"lm", "e", "()V", 33}};

static bool placement_execution_begin(placement_execution *s, mc_world_runtime *world) {
    mc_math_execution_init(&s->math, s->math_owner);
    mc_entity_tick_execution_init(&s->execution, &s->math);
    mc_world_runtime_execution_result_init(&s->result);
    mc_world_mutation_error_init(&s->error);
    s->root_open = mc_entity_tick_execution_push_world(&s->execution, world, placement_site,
                                                       &s->root) == MC_WORLD_STORAGE_OK;
    return s->root_open;
}

static bool placement_execution_end(placement_execution *s) {
    if (mc_world_mutation_execution_native_close(s->call) != MC_WORLD_STORAGE_OK ||
        mc_world_mutation_execution_retire(&s->call) != MC_WORLD_STORAGE_OK)
        return false;
    if (s->root_open) {
        if (mc_entity_tick_execution_pop_world(&s->execution, &s->root, true) !=
            MC_WORLD_STORAGE_OK)
            return false;
        s->root_open = false;
    }
    return mc_entity_tick_execution_retire(&s->execution) == MC_WORLD_STORAGE_OK &&
           mc_math_execution_retire(&s->math) == MC_MATH_CLASS_OK;
}

static bool placement_result_quiet(const mc_world_runtime_execution_result *result) {
    if (result->ownership_status != MC_WORLD_STORAGE_OK || result->selection_ready ||
        result->root_pending)
        return false;
    for (size_t i = 0; i < MC_ENTITY_TICK_EXECUTION_LOADERS; ++i)
        if (result->pending[i].used)
            return false;
    for (size_t i = 0; i < MC_ENTITY_TICK_EXECUTION_WORLDS; ++i)
        if (result->closes[i].used)
            return false;
    return true;
}

static bool placement_provider_witness(fixture *f) {
    bool ok = false;
    placement_execution *s = calloc(1, sizeof(*s));
    mc_world_block_execution *scalar = calloc(1, sizeof(*scalar));
    mc_world_runtime *world = f->worlds[0];
    mc_chunk_provider *const provider = world->fields.v;
    const bool deferred = world->fields.L;
    CHECK(s != NULL && scalar != NULL &&
          mc_math_classes_create(SIZE_MAX, &s->math_owner, NULL) == MC_MATH_CLASS_OK &&
          boxes_cell(f, 0, 8, 72, 8, 23, 0));
    s->game = f;
    mc_math_execution_init(&s->math, s->math_owner);
    mc_entity_tick_execution_init(&s->execution, &s->math);
    mc_world_block_execution_init(scalar, &s->execution, placement_code, placement_site,
                                  &s->result);
    mc_world_storage_error cause = {MC_WORLD_STORAGE_IO_ERROR, "native untouched sentinel", 91};
    mc_world_storage_status raw = MC_WORLD_STORAGE_LIMIT;
    bool provider_null = true;
    int32_t value = 29;
    world->fields.v = NULL;
    /* No evaluated World scope: native admission precedes the actual source
     * receiver read, even though the public v prestate is NULL. */
    CHECK(!mc_world_runtime_block_get_source_in_execution(scalar, world, 8, 72, 8, &value, &cause,
                                                          &raw, &provider_null) &&
          !provider_null && !scalar->entered && !scalar->get_entered && !scalar->get_returned &&
          value == 29 && raw == MC_WORLD_STORAGE_LIMIT &&
          cause.status == MC_WORLD_STORAGE_IO_ERROR && cause.system_error == 91 &&
          s->result.ownership_status != MC_WORLD_STORAGE_OK && placement_execution_end(s));

    CHECK(placement_execution_begin(s, world));
    mc_world_block_execution_init(scalar, &s->execution, placement_code, placement_site,
                                  &s->result);
    provider_null = true;
    /* The real out-of-domain Source return does not read v. */
    CHECK(mc_world_runtime_block_get_source_in_execution(scalar, world, INT32_MAX, 72, INT32_MAX,
                                                         &value, &cause, &raw, &provider_null) &&
          !provider_null && scalar->entered && scalar->completed && !scalar->get_entered &&
          value == 0 && raw == MC_WORLD_STORAGE_OK);
    mc_world_block_execution_init(scalar, &s->execution, placement_code, placement_site,
                                  &s->result);
    value = 29;
    provider_null = false;
    mc_chunk_provider_cache_observation before, after;
    CHECK(mc_chunk_provider_cache_observe(provider, &before));
    CHECK(!mc_world_runtime_block_get_source_in_execution(scalar, world, 8, 72, 8, &value, &cause,
                                                          &raw, &provider_null) &&
          provider_null && scalar->entered && !scalar->completed && !scalar->get_entered &&
          !scalar->get_returned && !scalar->native_stopped && value == 29 &&
          raw == MC_WORLD_STORAGE_UNSUPPORTED_STATE && placement_result_quiet(&s->result) &&
          mc_chunk_provider_cache_observe(provider, &after) && before.hits == after.hits &&
          before.generic_gets == after.generic_gets && placement_execution_end(s));

    const size_t active = world->fields.c->count, queued = world->fields.G->count;
    const size_t mapped = f->chunks[0]->l->count;
    mc_java_random_constructor_records_view constructors_before, constructors_after;
    CHECK(mc_java_random_profile_constructor_records_get(f->profile, &constructors_before) ==
          MC_JAVA_RANDOM_PROFILE_OK);
    /* These are public Source entry fields on the actual constructed World.
     * FD.a's L branch appends the real new AZ to G without acquiring a Chunk,
     * so RW returns before XQ.h reaches the nullable provider at its first read. */
    world->fields.L = true;
    CHECK(placement_execution_begin(s, world));
    mc_world_mutation_execution_context context = {.execution = &s->execution,
                                                   .code_classes = f->classes[0],
                                                   .code = placement_code,
                                                   .result = &s->result,
                                                   .world = s->root};
    CHECK(!mc_world_mutation_execution_added(&context, world, f->registry[0]->blocks.m[23], 8, 72,
                                             8, &s->call, &s->error, placement_site) &&
          s->call != NULL && s->error.status == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          strcmp(s->error.hook_class, "xq") == 0 && s->error.hook_bytecode == 15 &&
          strcmp(s->error.cause_hook_class, "xq") == 0 && s->error.cause_hook_bytecode == 15 &&
          s->error.query.source_null && s->error.query.bytecode == 48 &&
          s->error.query.provider_bytecode == 6 && s->error.provider_bytecode == 6 &&
          !s->error.dispenser_source_null && world->fields.c->count == active &&
          world->fields.G->count == queued + 1u && f->chunks[0]->l->count == mapped &&
          placement_metadata(f, 8, 72, 8, 0));
    mc_tile_entity *tile = world->fields.G->items[queued];
    mc_tile_entity_snapshot snapshot;
    mc_entity_tick_execution_site source_null = {NULL, NULL, NULL, -1};
    mc_world_storage_status child = MC_WORLD_STORAGE_LIMIT;
    CHECK(mc_tile_entity_get_snapshot(tile, &snapshot) == MC_TILE_ENTITY_OK &&
          snapshot.kind == MC_TILE_ENTITY_DISPENSER && snapshot.phase == MC_TILE_ENTITY_READY &&
          snapshot.world == NULL && snapshot.x == 8 && snapshot.y == 72 && snapshot.z == 8 &&
          snapshot.dispenser_random_constructed &&
          mc_world_mutation_execution_failed_tile_added(s->call, &child) == NULL &&
          mc_world_mutation_execution_source_null(s->call, &source_null) &&
          strcmp(source_null.class_name, "fd") == 0 && strcmp(source_null.method, "c") == 0 &&
          strcmp(source_null.descriptor, "(II)Llm;") == 0 && source_null.pc == 6 &&
          placement_result_quiet(&s->result) &&
          mc_java_random_profile_constructor_records_get(f->profile, &constructors_after) ==
              MC_JAVA_RANDOM_PROFILE_OK &&
          constructors_after.recorded == constructors_before.recorded + 1u &&
          world->fields.v == NULL && world->fields.L);
    CHECK(placement_execution_end(s) && world->fields.G->count == queued + 1u &&
          f->chunks[0]->l->count == mapped && world->fields.v == NULL && world->fields.L);

    /* End the authored public field view only after the failed body and its
     * checked owner are consumed. No getter/Source continuation is retried. */
    world->fields.v = provider;
    world->fields.L = deferred;
    CHECK(placement_execution_begin(s, NULL));
    context.world = s->root;
    CHECK(!mc_world_mutation_execution_added(&context, NULL, f->registry[0]->blocks.m[23], 8, 72, 8,
                                             &s->call, &s->error, placement_site) &&
          s->call != NULL && s->error.hook_bytecode == 6 && !s->error.query.source_null &&
          !s->error.dispenser_source_null &&
          mc_world_mutation_execution_source_null(s->call, &source_null) &&
          strcmp(source_null.class_name, "rw") == 0 && strcmp(source_null.method, "c") == 0 &&
          source_null.pc == 18 && world->fields.G->count == queued + 1u &&
          placement_execution_end(s));
    ok = true;
cleanup:
    if (s != NULL) {
        if (s->execution.identity != NULL && !placement_execution_end(s))
            return false;
        if (mc_math_classes_release(&s->math_owner, NULL) != MC_MATH_CLASS_OK)
            return false;
        free(s);
    }
    world->fields.v = provider;
    world->fields.L = deferred;
    free(scalar);
    return ok;
}

static mc_world_storage_status placement_source_failure(void *context, mc_world_runtime *world,
                                                        int32_t x, int32_t y, int32_t z,
                                                        mc_world_storage_error *cause,
                                                        mc_world_access_failure *failure) {
    placement_execution *s = context;
    ++s->callbacks;
    s->valid = s->callbacks == 1 && world == s->game->worlds[0] && x == 8 && y == 70 && z == 8 &&
               placement_metadata(s->game, 8, 70, 8, 3);
    bool removed = false;
    const mc_world_storage_status status =
        mc_world_runtime_remove_world_access(world, s->access, &removed, cause);
    s->registered = !(status == MC_WORLD_STORAGE_OK && removed);
    s->valid = s->valid && !s->registered &&
               mc_world_access_free(s->access, NULL) == MC_WORLD_STORAGE_BUSY;
    if (!s->valid)
        return MC_WORLD_STORAGE_INVALID_DATA;
    const mc_world_storage_status made =
        mc_world_access_receipt_create(s->access, NULL, &s->receipt, failure, cause);
    if (made != MC_WORLD_STORAGE_OK)
        return made;
    s->receipt->exception_class = s->exception_name; /* Actual producer-owned text. */
    *cause = (mc_world_storage_error){MC_WORLD_STORAGE_IO_ERROR,
                                      "XQ authored self-removing metadata producer", 73};
    return cause->status;
}

static bool placement_owned_failure(fixture *f) {
    bool ok = false;
    placement_execution *s = calloc(1, sizeof(*s));
    CHECK(s != NULL);
    s->game = f;
    const char name[] = "test/DispenserPlacementFailure";
    s->exception_name = malloc(sizeof(name));
    CHECK(s->exception_name != NULL &&
          mc_math_classes_create(SIZE_MAX, &s->math_owner, NULL) == MC_MATH_CLASS_OK);
    memcpy(s->exception_name, name, sizeof(name));
    mc_math_execution_init(&s->math, s->math_owner);
    mc_entity_tick_execution_init(&s->execution, &s->math);
    mc_world_runtime_execution_result_init(&s->result);
    mc_world_mutation_error_init(&s->error);
    CHECK(boxes_cell(f, 0, 8, 70, 8, 23, 0));
    const size_t active = f->worlds[0]->fields.c->count;
    const mc_world_access_source_callbacks callbacks = {.cell = placement_source_failure};
    CHECK(mc_world_access_source_create(&callbacks, s, &s->access, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_add_world_access(f->worlds[0], s->access, NULL) == MC_WORLD_STORAGE_OK);
    s->registered = true;
    CHECK(mc_entity_tick_execution_push_world(&s->execution, f->worlds[0], placement_site,
                                              &s->root) == MC_WORLD_STORAGE_OK);
    s->root_open = true;
    const mc_world_mutation_execution_context context = {
        .execution = &s->execution,
        .code_classes = f->classes[0],
        .code = {{"lm", "e", "()V", 6}, {"lm", "e", "()V", 33}},
        .result = &s->result,
        .world = s->root};
    CHECK(!mc_world_mutation_execution_added(&context, f->worlds[0], f->registry[0]->blocks.m[23],
                                             8, 70, 8, &s->call, &s->error, placement_site) &&
          s->call != NULL && s->error.status == MC_WORLD_STORAGE_IO_ERROR && s->valid &&
          s->callbacks == 1 && !s->registered && s->error.cause.system_error == 73 &&
          strcmp(s->error.hook_class, "xq") == 0 && s->error.hook_bytecode == 15 &&
          strcmp(s->error.cause_hook_class, "xq") == 0 && s->error.cause_hook_bytecode == 150 &&
          placement_metadata(f, 8, 70, 8, 3) && f->worlds[0]->fields.c->count == active + 1u &&
          placement_tile(f, 8, 70, 8));
    const mc_world_notification_execution *observer =
        mc_world_mutation_execution_failure_observer(s->call);
    if (observer == NULL || !observer->entered || observer->completed ||
        observer->callbacks_returned != 1 || s->receipt == NULL || s->receipt->holds != 1 ||
        s->error.world_access_failure.receipt != s->receipt)
        fprintf(stderr,
                "Dispenser retained failure observer=%d entered=%d completed=%d callbacks=%zu "
                "receipt=%d holds=%zu matched=%d\n",
                observer != NULL, observer != NULL && observer->entered,
                observer != NULL && observer->completed,
                observer == NULL ? 0 : observer->callbacks_returned, s->receipt != NULL,
                s->receipt == NULL ? 0 : s->receipt->holds,
                s->error.world_access_failure.receipt == s->receipt);
    /* The owned execution moves the real callback occurrence into a lease;
     * it does not manufacture an additional receipt hold. */
    CHECK(observer != NULL && observer->entered && !observer->completed &&
          observer->callbacks_returned == 1 && s->receipt != NULL && s->receipt->holds == 1 &&
          s->receipt->leases != NULL && s->access->active_holds == 1 &&
          s->access->receipt_holds == 1 && s->access->memberships == 0 &&
          s->error.world_access_failure.receipt == s->receipt &&
          mc_world_access_receipt_retire(&s->receipt, NULL) == MC_WORLD_STORAGE_BUSY &&
          mc_world_access_free(s->access, NULL) == MC_WORLD_STORAGE_BUSY);
    const char *actual_class = NULL;
    CHECK(mc_world_access_failure_source_exception(&s->error.world_access_failure, &actual_class) &&
          actual_class == s->exception_name);
    /* A failed callback cannot be bypassed by opening another World scope.
     * Execution retirement refuses the still-open actual root. */
    CHECK(mc_entity_tick_execution_push_world(&s->execution, f->worlds[0], placement_site,
                                              &s->external) == MC_WORLD_STORAGE_BUSY &&
          s->external.owner == NULL);
    CHECK(mc_entity_tick_execution_retire(&s->execution) == MC_WORLD_STORAGE_BUSY &&
          s->error.status == MC_WORLD_STORAGE_IO_ERROR && s->error.cause.system_error == 73 &&
          placement_metadata(f, 8, 70, 8, 3) && s->callbacks == 1 && s->receipt->holds == 1 &&
          s->receipt->leases != NULL && s->access->active_holds == 1);
    CHECK(mc_world_mutation_execution_native_close(s->call) == MC_WORLD_STORAGE_OK &&
          mc_world_mutation_execution_retire(&s->call) == MC_WORLD_STORAGE_OK && s->call == NULL);
    CHECK(mc_entity_tick_execution_pop_world(&s->execution, &s->root, true) == MC_WORLD_STORAGE_OK);
    s->root_open = false;
    CHECK(mc_entity_tick_execution_retire(&s->execution) == MC_WORLD_STORAGE_OK &&
          mc_math_execution_retire(&s->math) == MC_MATH_CLASS_OK && s->receipt->holds == 1 &&
          s->receipt->leases == NULL && s->access->active_holds == 0 && s->callbacks == 1 &&
          placement_metadata(f, 8, 70, 8, 3));
    ok = true;
cleanup:
    if (s != NULL) {
        if (s->external_open && mc_entity_tick_execution_pop_world(&s->execution, &s->external,
                                                                   false) != MC_WORLD_STORAGE_OK)
            return false;
        if (mc_world_mutation_execution_native_close(s->call) != MC_WORLD_STORAGE_OK ||
            mc_world_mutation_execution_retire(&s->call) != MC_WORLD_STORAGE_OK)
            return false;
        if (s->root_open && mc_entity_tick_execution_pop_world(&s->execution, &s->root, true) !=
                                MC_WORLD_STORAGE_OK)
            return false;
        if (s->execution.identity != NULL &&
            (mc_entity_tick_execution_retire(&s->execution) != MC_WORLD_STORAGE_OK ||
             mc_math_execution_retire(&s->math) != MC_MATH_CLASS_OK))
            return false;
        if (s->registered) {
            bool removed = false;
            if (mc_world_runtime_remove_world_access(f->worlds[0], s->access, &removed, NULL) !=
                    MC_WORLD_STORAGE_OK ||
                !removed)
                return false;
        }
        /* The producer's own receipt kept this borrow live after execution
         * cleanup. End that diagnostic view before consuming its final owner. */
        s->error.world_access_failure = (mc_world_access_failure){0};
        if (mc_world_access_receipt_retire(&s->receipt, NULL) != MC_WORLD_STORAGE_OK ||
            mc_world_access_free(s->access, NULL) != MC_WORLD_STORAGE_OK ||
            mc_math_classes_release(&s->math_owner, NULL) != MC_MATH_CLASS_OK)
            return false;
        free(s->exception_name);
        free(s);
    }
    return ok;
}

int main(int argc, char **argv) {
    int result = 1;
    fixture *f = NULL;
    mc_java_random_constructor_record *records = NULL;
    char root[8192];
    const uint16_t title[] = {'X', 'Q', ' ', 'p', 'l', 'a', 'c', 'e'};
    if (argc != 3 || strcmp(argv[1], "world-dispenser-placement") != 0 ||
        !decode_path(argv[2], root, sizeof(root)) || !mc_math_trig_init(&trig))
        return 1;
    f = calloc(1, sizeof(*f));
    records = calloc(256, sizeof(*records));
    CHECK(f != NULL && records != NULL &&
          mc_world_regions_create(NULL, &f->regions, NULL) == MC_WORLD_STORAGE_OK &&
          mc_materials_create(&f->materials) &&
          mc_java_random_profile_create(8u * 1024u * 1024u, &f->profile) ==
              MC_JAVA_RANDOM_PROFILE_OK &&
          mc_java_random_profile_bind_constructor_records(f->profile, records, 256) ==
              MC_JAVA_RANDOM_PROFILE_OK &&
          mc_world_info_name_copy((mc_nbt_string){title, COUNT(title)}, SIZE_MAX, &f->name) ==
              MC_WORLD_INFO_OK);
    resources *r = &f->sources[0];
    r->streams[0] = (stream){.bytes = language, .length = sizeof(language) - 1u};
    r->streams[1] = r->streams[2] = (stream){.bytes = language};
    r->api = (mc_registry_resources){r, open_resource, close_resource, ready_resource};
    const mc_registry_options options = {MC_REGISTRY_CLIENT, f->profile, f->materials, 0, &r->api};
    CHECK(mc_registry_classes_allocate(&options, &f->registry[0], NULL) == MC_REGISTRY_OK &&
          mc_world_runtime_classes_create(MC_WORLD_INFO_CLIENT, f->profile, &trig, &f->classes[0],
                                          NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_classes_bind_registry(f->classes[0], f->registry[0], NULL) ==
              MC_REGISTRY_OK &&
          mc_entity_classes_create(MC_ENTITY_CLIENT, f->profile, NULL, &f->entity_classes[0]) ==
              MC_ENTITY_OK &&
          mc_world_runtime_classes_bind_entities(f->classes[0], f->entity_classes[0]) ==
              MC_ENTITY_OK &&
          mc_registry_classes_initialize_blocks(f->registry[0], NULL) == MC_REGISTRY_OK &&
          boxes_open_store(f, 0, root) && boxes_create_world(f, 0) && placement_combinations(f) &&
          placement_query_controls(f) && placement_owned_failure(f) &&
          placement_provider_witness(f));
    result = 0;
cleanup:
    if (!cleanup_fixture(f)) {
        fputs("Dispenser placement cleanup refused; surviving owners retained\n", stderr);
        return 1;
    }
    free(records);
    if (result == 0)
        printf("Dispenser placement CPU controls passed: %u\n", checks);
    return result;
}
