/* Genuine World/Registry/Entity helpers; the included tick main is not run. */
#define main mc_entity_drop_world_fixture_main
int mc_entity_drop_world_fixture_main(int, char **);
#include "../../world/runtime/test_world_random_tick.c"
#undef main

#include "../../../src/entity/drop/internal.h"
#include "../../../src/world/storage/generated_internal.h"
#include "../../support/java_random.h"
#include "mc/entity/drop.h"
#include "mc/world/runtime/entities.h"
#include "mc/world/runtime/world_access.h"

#include <xmmintrin.h>

#undef CHECK
#define CHECK(c)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(c)) {                                                                                \
            fprintf(stderr, "Entity drop check%u line%d: %s\n", checks, __LINE__, #c);             \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

typedef struct entity_drop_observer {
    mc_entity_drop_call **call;
    mc_entity *item;
    size_t calls;
    bool fail, bad, active_busy;
    int32_t id, count, metadata;
    mc_item_stack *stack;
} entity_drop_observer;

static mc_world_storage_status entity_drop_added(void *context, mc_world_runtime *world,
                                                 mc_entity *item, mc_world_storage_error *error) {
    entity_drop_observer *observer = context;
    ++observer->calls;
    observer->item = item;
    mc_entity_drop_state state;
    mc_entity_drop_error cleanup;
    mc_item_stack_fields fields;
    observer->active_busy = mc_entity_drop_retire(observer->call, &cleanup) == MC_ENTITY_BUSY &&
                            *observer->call != NULL && cleanup.status == MC_ENTITY_BUSY;
    if (!observer->active_busy || !mc_entity_drop_inspect(*observer->call, &state) ||
        !state.active || state.item != item || !state.item_constructed ||
        !state.pickup_delay_stored || !state.publication_entered || state.publication_returned ||
        state.constructor_world != world || state.publication_world != world ||
        item->phase != MC_ENTITY_READY || item->base.world != world || item->item.c != 10 ||
        world->entity_membership.count == 0 ||
        world->entity_membership.entries[world->entity_membership.count - 1] != item ||
        mc_item_stack_fields_get(item->stack, &fields) != MC_ITEM_STACK_OK ||
        fields.id != observer->id || fields.count != observer->count ||
        fields.damage != observer->metadata ||
        (observer->stack != NULL && item->stack != observer->stack))
        observer->bad = true;
    if (observer->fail) {
        *error = (mc_world_storage_error){MC_WORLD_STORAGE_IO_ERROR,
                                          "authored Entity drop publication refusal", 29};
        return MC_WORLD_STORAGE_IO_ERROR;
    }
    return MC_WORLD_STORAGE_OK;
}

static bool entity_drop_retired(mc_entity_drop_call **call) {
    CHECK(mc_entity_drop_retire(call, NULL) == MC_ENTITY_OK && *call == NULL);
    return true;
}

static bool entity_drop_success(random_fixture *f, mc_entity *actor) {
    mc_entity_drop_call *call = NULL;
    mc_entity_drop_state state;
    mc_entity_drop_error error;
    mc_world_access *access = NULL;
    entity_drop_observer observer = {.call = &call, .id = 35, .count = 1, .metadata = 0};
    const mc_world_access_callbacks callbacks = {.entity_added = entity_drop_added};
    CHECK(mc_world_access_create(&callbacks, &observer, &access, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_add_world_access(f->world, access, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_entity_set_position(actor, 8.5, 64.5, 8.5) == MC_ENTITY_OK);
    const size_t actor_holds = actor->holds, bindings = f->world->persistent_bindings;
    const size_t registry_holds = f->registry->runtime_borrows;
    const mc_java_random actor_random = *actor->random, world_random = *f->world->fields.r;
    mc_java_random shared = {.state = atomic_load(&f->profile->math->state)};
    double draws[4];
    for (size_t i = 0; i < COUNT(draws); ++i)
        draws[i] = mc_java_random_next_double(&shared);
    const uint64_t unique = atomic_load(&f->profile->uniquifier);
    CHECK(mc_entity_drop_numeric_begin(f->registry, actor, 35, 1, .25f, &call, &error) ==
              MC_ENTITY_OK &&
          call != NULL && mc_entity_drop_inspect(call, &state) && state.returned && !state.active &&
          state.numeric && state.stack_allocated && state.stack_constructed &&
          state.item_allocated && state.item_constructed && state.pickup_delay_stored &&
          state.publication_returned && state.inserted && state.numeric_pc == 15 &&
          state.stack_pc == 44 && state.x == 8.5 && state.y == 64.75 && state.z == 8.5 &&
          state.constructor_world == f->world && state.publication_world == f->world &&
          observer.calls == 1 && observer.active_busy && !observer.bad &&
          state.item == observer.item && state.item->base.y == 64.75 &&
          state.item->stack == state.stack && state.item->item.c == 10 &&
          state.item->base.width == .25f && state.item->base.height == .25f &&
          state.item->base.offset == .125f && actor->holds == actor_holds + 1 &&
          f->registry->runtime_borrows == registry_holds + 1 &&
          f->world->persistent_bindings == bindings + 3 &&
          mc_test_java_random_equal(actor->random, &actor_random) &&
          mc_test_java_random_equal(f->world->fields.r, &world_random) &&
          atomic_load(&f->profile->math->state) == shared.state &&
          atomic_load(&f->profile->uniquifier) == unique * UINT64_C(1181783497276652981));
    /* The four shared draws belong solely to HL16/65/77/101. Independent
     * primitive arithmetic verifies the unmodified constructor's velocity. */
    volatile double phase = draws[0] * 3.141592653589793, doubled = phase * 2;
    volatile float expected_phase = (float)doubled, expected_yaw = (float)(draws[1] * 360.0);
    volatile double scaled_x = draws[2] * .20000000298023224;
    volatile double centered_x = scaled_x - .10000000149011612;
    volatile float expected_x = (float)centered_x;
    volatile double scaled_z = draws[3] * .20000000298023224;
    volatile double centered_z = scaled_z - .10000000149011612;
    volatile float expected_z = (float)centered_z;
    CHECK(float_bits(state.item->item.d) == float_bits(expected_phase) &&
          float_bits(state.item->base.yaw) == float_bits(expected_yaw) &&
          double_bits(state.item->base.velocity_x) == double_bits((double)expected_x) &&
          double_bits(state.item->base.velocity_y) == double_bits(.20000000298023224) &&
          double_bits(state.item->base.velocity_z) == double_bits((double)expected_z));
    mc_entity *published = state.item;
    CHECK(mc_entity_retain(published) == MC_ENTITY_OK && entity_drop_retired(&call) &&
          published->phase == MC_ENTITY_READY && published->item.c == 10 &&
          published->base.world == f->world && actor->holds == actor_holds &&
          f->registry->runtime_borrows == registry_holds &&
          f->world->persistent_bindings == bindings + 1);
    mc_entity_release(published);
    observer.fail = true;
    observer.calls = 0;
    const size_t members = f->world->entity_membership.count;
    CHECK(mc_entity_drop_numeric_begin(f->registry, actor, 35, 1, 0, &call, &error) ==
              MC_ENTITY_WORLD_FAILED &&
          call != NULL && mc_entity_drop_inspect(call, &state) && !state.returned &&
          state.item_constructed && state.pickup_delay_stored && state.publication_entered &&
          !state.publication_returned && error.site.pc == 39 && error.publication != NULL &&
          error.publication->status == MC_WORLD_STORAGE_IO_ERROR &&
          error.publication->cause.system_error == 29 && error.native_policy &&
          !error.source_exception && f->world->entity_membership.count == members + 1 &&
          observer.calls == 1 && observer.active_busy && !observer.bad);
    published = state.item;
    CHECK(mc_entity_retain(published) == MC_ENTITY_OK && entity_drop_retired(&call) &&
          published->phase == MC_ENTITY_READY && published->item.c == 10 &&
          f->world->entity_membership.entries[members] == published);
    mc_entity_release(published);
    bool removed = false;
    CHECK(mc_world_runtime_remove_world_access(f->world, access, &removed, NULL) ==
              MC_WORLD_STORAGE_OK &&
          removed && mc_world_access_free(access, NULL) == MC_WORLD_STORAGE_OK);
    return true;
}

static bool entity_drop_nullable_and_false(random_fixture *f, mc_entity *actor) {
    mc_entity_drop_call *call = NULL;
    mc_entity_drop_error error;
    mc_entity_drop_state state;
    const size_t members = f->world->entity_membership.count;
    CHECK(mc_entity_set_position(actor, 10000, -0.0, 10000) == MC_ENTITY_OK &&
          mc_entity_drop_stack_begin(f->registry, actor, NULL, 0, &call, &error) == MC_ENTITY_OK &&
          mc_entity_drop_inspect(call, &state) && state.returned && state.item_constructed &&
          state.stack == NULL && state.item->stack == NULL && state.publication_returned &&
          !state.inserted && state.stack_pc == 44 && double_bits(state.y) == 0 &&
          double_bits(state.item->base.y) == 0 && f->world->entity_membership.count == members &&
          entity_drop_retired(&call));
    mc_item_stack *stack = NULL;
    CHECK(mc_item_stack_allocate(&stack) == MC_ITEM_STACK_OK);
    CHECK(mc_entity_drop_stack_begin(f->registry, actor, stack, .5f, &call, &error) ==
              MC_ENTITY_OK &&
          mc_entity_drop_inspect(call, &state) && state.returned && state.stack == stack &&
          state.item->stack == stack && state.y == .5 && !state.inserted);
    mc_item_stack_phase phase;
    CHECK(mc_item_stack_phase_get(stack, &phase) == MC_ITEM_STACK_OK &&
          phase == MC_ITEM_STACK_RAW && entity_drop_retired(&call));
    CHECK(mc_item_stack_construct(stack, 35, 1, 12) == MC_ITEM_STACK_OK);
    actor->base.world =
        NULL; /* Declared nullable receiver input; retained original owner stays live. */
    const mc_entity_status missing =
        mc_entity_drop_stack_begin(f->registry, actor, stack, 0, &call, &error);
    CHECK(missing == MC_ENTITY_STATE_ERROR && mc_entity_drop_inspect(call, &state) &&
          state.item_constructed && state.pickup_delay_stored && !state.publication_entered &&
          state.constructor_world == NULL && state.publication_world == NULL &&
          error.source_exception && !error.native_policy && error.site.pc == 39 &&
          !strcmp(error.exception_class, "java/lang/NullPointerException") &&
          entity_drop_retired(&call) && actor->base.world == NULL);
    actor->base.world = f->world; /* Fixture's owning field restored only after retirement. */
    mc_item_stack_fields fields;
    CHECK(mc_item_stack_fields_get(stack, &fields) == MC_ITEM_STACK_OK && fields.id == 35 &&
          fields.count == 1 && fields.damage == 12);
    mc_item_stack_release(stack);
    CHECK(mc_item_stack_allocate(&stack) == MC_ITEM_STACK_OK &&
          mc_item_stack_construct_nbt(stack, NULL, NULL) == MC_ITEM_STACK_INVALID_ARGUMENT &&
          mc_item_stack_phase_get(stack, &phase) == MC_ITEM_STACK_OK &&
          phase == MC_ITEM_STACK_FAILED_CONSTRUCTION);
    CHECK(mc_entity_drop_stack_begin(f->registry, actor, stack, 0, &call, &error) == MC_ENTITY_OK &&
          mc_entity_drop_inspect(call, &state) && state.returned && state.item_constructed &&
          state.item->stack == stack && state.stack == stack && !state.inserted &&
          mc_item_stack_phase_get(stack, &phase) == MC_ITEM_STACK_OK &&
          phase == MC_ITEM_STACK_FAILED_CONSTRUCTION && entity_drop_retired(&call));
    mc_item_stack_release(stack);
    return true;
}

static bool entity_drop_failures(random_fixture *f, mc_entity *actor) {
    mc_entity_drop_call *call = NULL;
    mc_entity_drop_error error;
    mc_entity_drop_state state;
    CHECK(mc_entity_drop_numeric_begin(f->registry, actor, 35, 1, 0, NULL, &error) ==
              MC_ENTITY_INVALID_ARGUMENT &&
          mc_entity_drop_numeric_begin(NULL, NULL, 35, 1, 0, &call, &error) ==
              MC_ENTITY_STATE_ERROR &&
          call != NULL && error.source_exception && error.site.pc == 0 &&
          mc_entity_drop_inspect(call, &state) && state.stack == NULL && state.item == NULL &&
          entity_drop_retired(&call));
    CHECK(mc_entity_set_position(actor, 8.5, 64.5, 8.5) == MC_ENTITY_OK);
    const size_t allocation_limit = f->entities->limits.max_allocation_bytes;
    f->entities->limits.max_allocation_bytes = f->entities->allocated_bytes;
    const mc_entity_status allocation =
        mc_entity_drop_numeric_begin(f->registry, actor, 35, 1, 0, &call, &error);
    CHECK(allocation == MC_ENTITY_LIMIT && mc_entity_drop_inspect(call, &state) &&
          state.stack_allocated && state.stack_constructed && state.item == NULL &&
          !state.item_allocated && state.numeric_pc == 12 && state.stack_pc == 0 &&
          entity_drop_retired(&call));
    f->entities->limits.max_allocation_bytes = allocation_limit;
    const unsigned int csr = _mm_getcsr();
    _mm_setcsr(csr | 0x40u);
    const mc_entity_status floating =
        mc_entity_drop_numeric_begin(f->registry, actor, 35, 1, 0, &call, &error);
    _mm_setcsr(csr);
    CHECK(floating == MC_ENTITY_UNSUPPORTED_FP && mc_entity_drop_inspect(call, &state) &&
          state.item_allocated && !state.item_constructed && state.item->phase == MC_ENTITY_RAW &&
          state.constructor_world == f->world && state.stack_pc == 17 && state.numeric_pc == 12 &&
          error.native_policy && !error.source_exception && entity_drop_retired(&call));
    const int32_t next_id = f->entities->next_id;
    f->entities->limits.max_allocation_bytes = f->entities->allocated_bytes + sizeof(mc_entity);
    const mc_entity_status construction =
        mc_entity_drop_numeric_begin(f->registry, actor, 35, 1, 0, &call, &error);
    CHECK(construction == MC_ENTITY_LIMIT && mc_entity_drop_inspect(call, &state) &&
          state.item_allocated && !state.item_constructed &&
          state.item->phase == MC_ENTITY_FAILED_CONSTRUCTION && state.item->base_bytecode == 24 &&
          state.stack_constructed && state.item->stack == NULL && state.stack_pc == 24 &&
          state.item->base.id == next_id && f->entities->next_id == mc_java_i32_add(next_id, 1) &&
          error.entity.status == MC_ENTITY_LIMIT && error.entity.operation != NULL &&
          !strcmp(error.entity.operation, "bounds factory"));
    mc_entity_drop_call *failed = call;
    mc_entity *failed_item = state.item;
    mc_item_stack *failed_stack = state.stack;
    const size_t actor_holds = actor->holds, bindings = f->world->persistent_bindings;
    const size_t registry_holds = f->registry->runtime_borrows;
    ++f->registry->entered;
    mc_entity_drop_error cleanup;
    const mc_entity_status busy = mc_entity_drop_retire(&call, &cleanup);
    --f->registry->entered;
    CHECK(busy == MC_ENTITY_BUSY && cleanup.status == MC_ENTITY_BUSY && call == failed &&
          mc_entity_drop_inspect(call, &state) && state.item == failed_item &&
          state.stack == failed_stack && state.item->phase == MC_ENTITY_FAILED_CONSTRUCTION &&
          state.error->status == MC_ENTITY_LIMIT && state.error->site.pc == 24 &&
          state.error->entity.status == MC_ENTITY_LIMIT &&
          !strcmp(state.error->entity.operation, "bounds factory") && actor->holds == actor_holds &&
          f->world->persistent_bindings == bindings &&
          f->registry->runtime_borrows == registry_holds && entity_drop_retired(&call));
    f->entities->limits.max_allocation_bytes = allocation_limit;
    CHECK(mc_entity_drop_numeric_begin(f->registry, actor, 35, 1, 0, &call, NULL) == MC_ENTITY_OK &&
          call != NULL);
    mc_entity_drop_call *same = call;
    CHECK(mc_entity_drop_numeric_begin(f->registry, actor, 35, 1, 0, &call, &error) ==
              MC_ENTITY_INVALID_ARGUMENT &&
          call == same && entity_drop_retired(&call));
    return true;
}

static bool entity_drop_chunk(random_fixture *f) {
    uint8_t *blocks = calloc(MC_CHUNK_BLOCKS, 1);
    mc_world_chunk *object = NULL;
    CHECK(blocks != NULL &&
          mc_world_store_create_generated(f->store, f->world, 0, 0, 0, &blocks, MC_CHUNK_BLOCKS,
                                          &object, NULL) == MC_WORLD_STORAGE_OK &&
          blocks == NULL);
    mc_chunk_state *chunk = mc_world_chunk_state(object);
    CHECK(chunk != NULL && mc_chunk_cache_publish(f->provider.cache, 0, 0, chunk));
    f->owned[f->owned_count++] = chunk;
    return true;
}

int main(int argc, char **argv) {
    if (argc != 3)
        return 2;
    random_fixture *f = calloc(1, sizeof(*f));
    mc_entity *actor = NULL;
    bool passed =
        f != NULL && setup(f, argv[1], argv[2]) && entity_drop_chunk(f) &&
        mc_falling_entity_allocate(f->entities, &actor, NULL) == MC_ENTITY_OK &&
        mc_falling_entity_construct(actor, f->world, 8.5, 64.5, 8.5, 12, NULL) == MC_ENTITY_OK;
    double warmup;
    if (passed)
        passed = mc_java_math_random(f->profile, &warmup) == MC_JAVA_RANDOM_PROFILE_OK &&
                 entity_drop_success(f, actor) && entity_drop_nullable_and_false(f, actor) &&
                 entity_drop_failures(f, actor);
    mc_entity_release(actor);
    if (passed)
        passed = teardown(f);
    free(f);
    if (passed)
        printf("PASS: %u Entity drop source-order/ownership checks\n", checks);
    return passed ? 0 : 1;
}
