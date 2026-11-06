/* Genuine Player, Inventory, Session, statistics writer and World constructors.
 * The imported fixture entry point is never executed. */
#define main mc_drop_unused_statistics_fixture_main
#include "../../client/statistics/test_player_dispatch.c"
#undef main

#include "core/java/random_profile_internal.h"
#include "inventory/internal.h"
#include "mc/core/math/trig_double.h"
#include "mc/entity/player/drop.h"
#include "mc/world/runtime/world_access.h"
#include "world/runtime/runtime_internal.h"
#include "world/storage/generated_internal.h"

#include <math.h>
#include <stdatomic.h>
#include <xmmintrin.h>

typedef struct drop_observer {
    mc_player_drop_call **call;
    mc_entity *item;
    size_t calls;
    bool fail, busy, published;
} drop_observer;

static mc_world_storage_status drop_added(void *context, mc_world_runtime *world, mc_entity *item,
                                          mc_world_storage_error *error) {
    drop_observer *o = context;
    ++o->calls;
    o->item = item;
    o->published = world->entity_membership.count != 0 &&
                   world->entity_membership.entries[world->entity_membership.count - 1] == item &&
                   item->item.c == 40;
    o->busy = mc_local_player_drop_retire(o->call, NULL) == MC_PLAYER_DROP_BUSY && *o->call != NULL;
    if (o->fail) {
        *error = (mc_world_storage_error){MC_WORLD_STORAGE_LIMIT, "authored drop added refusal", 0};
        return MC_WORLD_STORAGE_LIMIT;
    }
    return MC_WORLD_STORAGE_OK;
}

static bool drop_stack(mc_inventory_player *inventory, int32_t count, mc_item_stack **out) {
    CHECK(mc_item_stack_allocate(out) == MC_ITEM_STACK_OK &&
          mc_item_stack_construct(*out, 3, count, 7) == MC_ITEM_STACK_OK &&
          mc_inventory_player_set(inventory, 0, *out, NULL) == MC_INVENTORY_OK);
    return true;
}

static bool drop_retire(mc_player_drop_call **call) {
    CHECK(mc_local_player_drop_retire(call, NULL) == MC_PLAYER_DROP_OK && *call == NULL);
    return true;
}

static bool drop_statistic(fixture *f, int32_t expected) {
    mc_registry_statistic_view view;
    int32_t value = -1;
    CHECK(mc_registry_classes_inspect_statistics(f->app.registry, &view, NULL) == MC_REGISTRY_OK &&
          view.named[16] != NULL && mc_statistic_fields_get(view.named[16])->e == 2011 &&
          mc_client_stats_writer_value(f->app.stats_writer, view.named[16], &value, NULL) ==
              MC_CLIENT_STATS_OK &&
          value == expected);
    return true;
}

static bool drop_random_state(fixture *f, uint64_t shared_before) {
    mc_java_random shared = {.state = shared_before};
    for (size_t i = 0; i < 4; ++i)
        (void)mc_java_random_next_double(&shared);
    CHECK(atomic_load(&f->app.profile->math->state) == shared.state);
    return true;
}

static bool drop_velocity(const mc_player_drop_state *state, const mc_math_classes *math, float yaw,
                          float pitch, const float *random, bool scatter) {
    const mc_math_trig *table = mc_math_classes_table(math);
    CHECK(table != NULL);
    double expected[3];
    if (scatter) {
        /* GS.a(Stack,Z)63..111: binary32 radius/angle and products precede
         * widening. This arm neither uses the pose nor applies double jitter. */
        const float radius = random[0] * .5f;
        const float half_angle = random[1] * 3.1415927f;
        const float angle = half_angle * 2.0f;
        const float x = -mc_math_sin(table, angle) * radius;
        const float z = mc_math_cos(table, angle) * radius;
        expected[0] = (double)x;
        expected[1] = .20000000298023224;
        expected[2] = (double)z;
    } else {
        /* Evaluate the original bytecode expression stack independently of
         * the production SSE helpers. Each named float is a binary32 result;
         * the original f2d sites widen only the completed products/sums. */
        const float yaw_turn = yaw / 180.0f;
        const float pitch_turn = pitch / 180.0f;
        const float yaw_angle = yaw_turn * 3.1415927f;
        const float pitch_angle = pitch_turn * 3.1415927f;
        const float x_direction = -mc_math_sin(table, yaw_angle) * mc_math_cos(table, pitch_angle);
        const float z_direction = mc_math_cos(table, yaw_angle) * mc_math_cos(table, pitch_angle);
        const float x = x_direction * .3f;
        const float z = z_direction * .3f;
        const float y_direction = -mc_math_sin(table, pitch_angle) * .3f;
        const float y = y_direction + .1f;
        const float half_angle = random[0] * 3.1415927f;
        const float angle = half_angle * 2.0f;
        const float spread = .02f * random[1];
        const float difference = random[2] - random[3];
        const float y_jitter = difference * .1f;
        const double x_jitter = mc_math_native_double_cos((double)angle) * (double)spread;
        const double z_jitter = mc_math_native_double_sin((double)angle) * (double)spread;
        expected[0] = (double)x + x_jitter;
        expected[1] = (double)y + (double)y_jitter;
        expected[2] = (double)z + z_jitter;
    }
    const double observed[] = {state->velocity_x, state->velocity_y, state->velocity_z};
    const double item[] = {state->item->base.velocity_x, state->item->base.velocity_y,
                           state->item->base.velocity_z};
    CHECK(memcmp(observed, expected, sizeof expected) == 0 &&
          memcmp(item, expected, sizeof expected) == 0);
    return true;
}

static bool drop_controls(fixture *f) {
    mc_math_classes *math = NULL;
    CHECK(mc_math_classes_create(SIZE_MAX, &math, NULL) == MC_MATH_CLASS_OK);
    mc_player_drop_call *call = NULL;
    mc_player_drop_state state;
    mc_player_drop_error error;
    CHECK(mc_local_player_drop_selected_begin(NULL, NULL, NULL, &call, &error) ==
              MC_PLAYER_DROP_NULL_REFERENCE &&
          error.source_exception && !error.native_policy && drop_retire(&call));
    const uint64_t player_before = f->player->random->state;
    const uint64_t shared_before = atomic_load(&f->app.profile->math->state);
    CHECK(mc_local_player_drop_selected_begin(NULL, NULL, f->player, &call, &error) ==
              MC_PLAYER_DROP_OK &&
          mc_local_player_drop_inspect(call, &state) && state.returned &&
          state.inventory_split_returned && state.stack == NULL && !state.item_allocated &&
          !state.statistic_returned && state.random_draws == 0 && state.source_pc == 20 &&
          f->player->random->state == player_before &&
          atomic_load(&f->app.profile->math->state) == shared_before && drop_retire(&call));
    mc_math_class_state math_state;
    CHECK(mc_math_classes_state(math, &math_state, NULL) == MC_MATH_CLASS_OK &&
          math_state.phase == MC_MATH_CLASS_COLD && drop_statistic(f, 0));
    CHECK(mc_local_player_drop_stack_begin(NULL, NULL, f->player, NULL, true, &call, &error) ==
              MC_PLAYER_DROP_OK &&
          mc_local_player_drop_inspect(call, &state) && state.returned && state.source_pc == 4 &&
          !state.inventory_split_returned && drop_retire(&call));

    uint8_t *blocks = calloc(MC_CHUNK_BLOCKS, 1);
    mc_world_chunk *chunk = NULL;
    CHECK(blocks != NULL &&
          mc_world_store_create_generated(f->store, f->world, 0, 0, 0, &blocks, MC_CHUNK_BLOCKS,
                                          &chunk, NULL) == MC_WORLD_STORAGE_OK &&
          blocks == NULL);
    const mc_chunk_acquire_context *acquisition = mc_world_runtime_acquisition(f->world);
    mc_chunk_state *chunk_state = mc_world_chunk_state(chunk);
    CHECK(acquisition != NULL && chunk_state != NULL &&
          mc_chunk_cache_publish(acquisition->cache, 0, 0, chunk_state));
    CHECK(mc_entity_set_position(f->player, .5, 64.0, .5) == MC_ENTITY_OK);
    /* Declared source pose input before these real component calls. */
    f->player->base.yaw = 0.0f;
    f->player->base.aT = 0.0f;
    mc_inventory_player *inventory = f->player->local_player->fields.player.c;
    mc_world_access *access = NULL;
    drop_observer observer = {.call = &call};
    const mc_world_access_callbacks callbacks = {.entity_added = drop_added};
    CHECK(mc_world_access_create(&callbacks, &observer, &access, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_add_world_access(f->world, access, NULL) == MC_WORLD_STORAGE_OK);
    mc_item_stack *stack = NULL;
    int32_t statistics = 0;
    for (unsigned int mode = 0; mode < 4; ++mode) {
        CHECK(drop_stack(inventory, mode == 1 ? 2 : 1, &stack));
        mc_java_random oracle = *f->player->random;
        float random[4];
        for (size_t i = 0; i < 4; ++i)
            random[i] = mc_java_random_next_float(&oracle);
        const uint64_t shared = atomic_load(&f->app.profile->math->state);
        observer.calls = 0;
        observer.fail = mode == 2;
        client_application *app = f->player->local_player->fields.minecraft;
        if (mode == 3)
            f->player->local_player->fields.minecraft = NULL; /* Authored nullable source input. */
        const mc_player_drop_status status =
            mc_local_player_drop_selected_begin(f->app.registry, math, f->player, &call, &error);
        f->player->local_player->fields.minecraft = app;
        CHECK(status == (mode == 2   ? MC_PLAYER_DROP_WORLD
                         : mode == 3 ? MC_PLAYER_DROP_STATISTIC
                                     : MC_PLAYER_DROP_OK));
        CHECK(mc_local_player_drop_inspect(call, &state) && state.selected_action &&
              state.inventory_split_returned && state.item_allocated && state.item_constructed &&
              state.pickup_delay_stored && state.publication_entered &&
              state.publication_returned == (mode != 2) && state.inserted == (mode != 2) &&
              state.returned == (mode < 2) && state.statistic_returned == (mode < 2) &&
              state.random_draws == 4 && memcmp(state.draws, random, sizeof random) == 0 &&
              f->player->random->state == oracle.state && observer.calls == 1 &&
              observer.item == state.item && observer.published && observer.busy);
        CHECK(drop_random_state(f, shared));
        CHECK(state.item->item.c == 40 && state.item->stack == state.stack && state.x == .5 &&
              state.z == .5 && state.y == (64.0 - .30000001192092896) + (double).12f &&
              fabs(state.velocity_x) < .021 && fabs(state.velocity_y - (double).1f) < .101 &&
              state.velocity_z > .279 && state.velocity_z < .321);
        CHECK(drop_velocity(&state, math, 0.0f, 0.0f, random, false));
        mc_inventory_player_state slots;
        mc_item_stack_fields fields;
        CHECK(mc_inventory_player_read_state(inventory, &slots) &&
              mc_item_stack_fields_get(state.stack, &fields) == MC_ITEM_STACK_OK &&
              fields.id == 3 && fields.count == 1 && fields.damage == 7);
        if (mode == 1)
            CHECK(slots.main_slots[0] == stack && state.stack != stack &&
                  mc_item_stack_fields_get(stack, &fields) == MC_ITEM_STACK_OK &&
                  fields.count == 1);
        else
            CHECK(slots.main_slots[0] == NULL && state.stack == stack);
        if (mode < 2)
            ++statistics;
        if (mode == 2)
            CHECK(error.native_policy && !error.source_exception && error.site.pc == 315 &&
                  error.publication != NULL && error.publication->world_access_bytecode != 0);
        if (mode == 3)
            CHECK(error.source_exception && !error.native_policy && error.site.pc == 323 &&
                  strcmp(error.exception_class, "java/lang/NullPointerException") == 0);
        CHECK(drop_statistic(f, statistics) && drop_retire(&call));
        CHECK(mc_inventory_player_set(inventory, 0, NULL, NULL) == MC_INVENTORY_OK);
        mc_item_stack_release(stack);
        stack = NULL;
    }

    /* Declared component poses and Random seeds precede every real call.
     * Opposite signs, steep pitch and angles beyond one revolution expose
     * sign, degree conversion, lookup and premature-widening mistakes. */
    static const struct {
        float yaw, pitch;
        int64_t seed;
        bool scatter;
    } vectors[] = {{37.5f, 22.75f, 0, false},
                   {-123.25f, -46.5f, -1, false},
                   {-360.125f, 89.75f, INT64_C(123456789), false},
                   {721.75f, -89.5f, INT64_MIN, false},
                   {37.5f, 22.75f, 0, true},
                   {-123.25f, -46.5f, -1, true}};

    observer.fail = false;
    for (size_t i = 0; i < sizeof vectors / sizeof vectors[0]; ++i) {
        CHECK(drop_stack(inventory, 1, &stack));
        f->player->base.yaw = vectors[i].yaw;
        f->player->base.aT = vectors[i].pitch;
        mc_java_random_reseed(f->player->random, vectors[i].seed);
        mc_java_random oracle = *f->player->random;
        float random[4] = {0};
        const size_t draws = vectors[i].scatter ? 2u : 4u;
        for (size_t j = 0; j < draws; ++j)
            random[j] = mc_java_random_next_float(&oracle);
        const uint64_t shared = atomic_load(&f->app.profile->math->state);
        observer.calls = 0;
        CHECK(mc_local_player_drop_stack_begin(f->app.registry, math, f->player, stack,
                                               vectors[i].scatter, &call,
                                               &error) == MC_PLAYER_DROP_OK &&
              mc_local_player_drop_inspect(call, &state) && state.returned && state.inserted &&
              state.scatter == vectors[i].scatter && !state.inventory_split_returned &&
              state.random_draws == draws &&
              memcmp(state.draws, random, draws * sizeof random[0]) == 0 &&
              f->player->random->state == oracle.state && observer.calls == 1 &&
              observer.item == state.item && observer.published && observer.busy &&
              drop_random_state(f, shared) &&
              drop_velocity(&state, math, vectors[i].yaw, vectors[i].pitch, random,
                            vectors[i].scatter) &&
              drop_statistic(f, ++statistics) && drop_retire(&call));
        mc_inventory_player_state slots;
        mc_item_stack_fields fields;
        CHECK(mc_inventory_player_read_state(inventory, &slots) && slots.main_slots[0] == stack &&
              mc_item_stack_fields_get(stack, &fields) == MC_ITEM_STACK_OK && fields.count == 1 &&
              mc_inventory_player_set(inventory, 0, NULL, NULL) == MC_INVENTORY_OK);
        mc_item_stack_release(stack);
        stack = NULL;
    }
    /* Restore the declared zero pose for the subsequent failure-prefix cases. */
    f->player->base.yaw = 0.0f;
    f->player->base.aT = 0.0f;
    /* Scatter is the same real overload, with two private draws and no
     * directional double jitter. The selected Inventory is untouched. */
    CHECK(mc_item_stack_allocate(&stack) == MC_ITEM_STACK_OK &&
          mc_item_stack_construct(stack, 3, 0, 7) == MC_ITEM_STACK_OK);
    observer.fail = false;
    mc_java_random oracle = *f->player->random;
    float scatter_draws[2];
    for (size_t i = 0; i < 2; ++i)
        scatter_draws[i] = mc_java_random_next_float(&oracle);
    CHECK(mc_local_player_drop_stack_begin(f->app.registry, math, f->player, stack, true, &call,
                                           &error) == MC_PLAYER_DROP_OK &&
          mc_local_player_drop_inspect(call, &state) && state.returned && state.scatter &&
          !state.inventory_split_returned && state.stack == stack && state.random_draws == 2 &&
          memcmp(state.draws, scatter_draws, sizeof scatter_draws) == 0 &&
          f->player->random->state == oracle.state && state.velocity_y == .20000000298023224 &&
          state.item->stack == stack && state.item->item.c == 40 &&
          drop_velocity(&state, math, 0.0f, 0.0f, scatter_draws, true) &&
          drop_statistic(f, ++statistics) && drop_retire(&call));
    mc_item_stack_release(stack);
    stack = NULL;

    /* Actual absent-chunk publication returns false, yet GS still credits the
     * drop statistic and returns. No WorldAccess callback is reached. */
    CHECK(mc_entity_set_position(f->player, 32.5, 64.0, .5) == MC_ENTITY_OK &&
          drop_stack(inventory, 1, &stack));
    observer.calls = 0;
    CHECK(mc_local_player_drop_selected_begin(f->app.registry, math, f->player, &call, &error) ==
              MC_PLAYER_DROP_OK &&
          mc_local_player_drop_inspect(call, &state) && state.returned &&
          state.publication_returned && !state.inserted && state.statistic_returned &&
          observer.calls == 0 && drop_statistic(f, ++statistics) && drop_retire(&call));
    mc_item_stack_release(stack);
    stack = NULL;
    CHECK(mc_entity_set_position(f->player, .5, 64.0, .5) == MC_ENTITY_OK);

    /* These are declared native allocation budgets and nullable source fields
     * before each actual call. Restore them before inspecting the real failure;
     * no result, phase, random state, source count or callback is repaired. */
    for (unsigned int mode = 0; mode < 5; ++mode) {
        CHECK(drop_stack(inventory, 1, &stack));
        const mc_entity_limits limits = f->app.entities->limits;
        mc_world_runtime *world = f->player->base.world;
        mc_java_random *random = f->player->random;
        const unsigned int controls = _mm_getcsr();
        const uint64_t random_before = random->state;
        const uint64_t shared = atomic_load(&f->app.profile->math->state);
        observer.calls = 0;
        if (mode == 0)
            f->app.entities->limits.max_live_entities = f->app.entities->live_entities;
        if (mode == 1)
            f->app.entities->limits.max_allocation_bytes =
                f->app.entities->allocated_bytes + sizeof(mc_entity);
        if (mode == 2)
            _mm_setcsr(controls | 0x8040u);
        if (mode == 3)
            f->player->base.world = NULL;
        if (mode == 4)
            f->player->random = NULL;
        const mc_player_drop_status status =
            mc_local_player_drop_selected_begin(f->app.registry, math, f->player, &call, &error);
        f->app.entities->limits = limits;
        f->player->base.world = world;
        f->player->random = random;
        _mm_setcsr(controls);
        CHECK(status == (mode < 2    ? MC_PLAYER_DROP_ENTITY
                         : mode == 2 ? MC_PLAYER_DROP_FLOATING
                                     : MC_PLAYER_DROP_NULL_REFERENCE));
        CHECK(mc_local_player_drop_inspect(call, &state) && !state.returned &&
              state.inventory_split_returned && state.stack == stack &&
              !state.publication_returned && !state.statistic_returned && observer.calls == 0 &&
              state.item_allocated == (mode != 0) && state.item_constructed == (mode >= 3) &&
              !state.inserted && state.random_draws == (mode == 3 ? 4u : 0u));
        mc_inventory_player_state slots;
        CHECK(mc_inventory_player_read_state(inventory, &slots) && slots.main_slots[0] == NULL);
        if (mode < 3)
            CHECK(random->state == random_before &&
                  atomic_load(&f->app.profile->math->state) == shared && !error.source_exception &&
                  error.native_policy);
        if (mode == 1)
            CHECK(state.item->phase == MC_ENTITY_FAILED_CONSTRUCTION && error.site.pc == 36);
        if (mode == 2)
            CHECK(state.item->phase == MC_ENTITY_RAW && error.site.pc == 24);
        if (mode == 3)
            CHECK(state.publication_entered == false && error.source_exception &&
                  error.site.pc == 315 && error.child.pc == 5 && state.item->base.world == NULL);
        if (mode == 4)
            CHECK(error.source_exception && error.site.pc == 225 &&
                  state.item->base.velocity_z == (double).3f);
        CHECK(drop_statistic(f, statistics) && drop_retire(&call));
        mc_item_stack_release(stack);
        stack = NULL;
    }
    const int32_t selected = inventory->selected;
    inventory->selected = -1; /* Declared source index input to actual IX.a. */
    const mc_player_drop_status indexed =
        mc_local_player_drop_selected_begin(f->app.registry, math, f->player, &call, &error);
    inventory->selected = selected;
    CHECK(indexed == MC_PLAYER_DROP_INVENTORY && error.source_exception && !error.native_policy &&
          error.site.pc == 13 && strcmp(error.child.class_name, "ix") == 0 &&
          strcmp(error.exception_class, "java/lang/ArrayIndexOutOfBoundsException") == 0 &&
          mc_local_player_drop_inspect(call, &state) && !state.inventory_split_returned &&
          !state.item_allocated && drop_retire(&call));
    bool removed = false;
    CHECK(mc_world_runtime_remove_world_access(f->world, access, &removed, NULL) ==
              MC_WORLD_STORAGE_OK &&
          removed && mc_world_access_free(access, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_retire_entity_containers(f->world, NULL) == MC_WORLD_STORAGE_OK &&
          mc_chunk_cache_init(acquisition->cache, acquisition->cache->entries,
                              acquisition->cache->capacity, acquisition->cache->insertions,
                              acquisition->cache->insertion_capacity) &&
          mc_world_store_retire_chunk(f->store, chunk_state, NULL) == MC_WORLD_STORAGE_OK &&
          mc_math_classes_release(&math, NULL) == MC_MATH_CLASS_OK);
    return true;
}

int main(int argc, char **argv) {
    if (argc != 3 || CD(argv[2]) != 0 || !mc_math_trig_init(&trig))
        return 2;
    fixture *f = calloc(1, sizeof(*f));
    if (f == NULL || !setup(f, argv[1], "normal", true) || !drop_controls(f) || !teardown(f))
        return 1;
    free(f);
    printf("PASS: %zu Player drop source-order/ownership checks\n", checks);
    return 0;
}
