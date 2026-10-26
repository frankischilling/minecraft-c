#include "mc/entity/player/tick.h"
#include <stdlib.h>
typedef struct fixture fixture;
static bool allocation_cases(fixture *, client_application *, mc_entity *, mc_movement_input *);
#define MC_PLAYER_ACTIVE_TICK_MAIN mc_active_allocation_fixture_main
#define MC_PLAYER_ACTIVE_TICK_CASES allocation_cases
#include "test_tick.c"
#undef MC_PLAYER_ACTIVE_TICK_MAIN
#undef MC_PLAYER_ACTIVE_TICK_CASES

static bool fail_next;
static size_t attempts, allocations, retirements, outstanding, peak;

void *mc_player_tick_test_allocate(size_t bytes) {
    ++attempts;
    if (fail_next) {
        fail_next = false;
        return NULL;
    }
    void *memory = malloc(bytes);
    if (memory != NULL) {
        ++allocations;
        ++outstanding;
        if (outstanding > peak)
            peak = outstanding;
    }
    return memory;
}

void mc_player_tick_test_free(void *memory) {
    if (memory != NULL) {
        ++retirements;
        --outstanding;
    }
    free(memory);
}

#define MC_PLAYER_TICK_TEST_ALLOCATOR
#define run mc_active_allocation_run
#define rejected mc_active_allocation_rejected
#include "../../../src/entity/player/tick.c"
#undef rejected
#undef run

typedef struct reentry {
    mc_entity *player;
    client_application *app;
    const mc_player_action_context *context;
    mc_player_tick_error *outer;
    mc_player_tick_error inner;
    mc_entity_status status;
    bool called, isolated;
    size_t sounds, particles;
} reentry;

static const char sentinel[] = "outer-buffer-sentinel";

static mc_world_storage_status reenter_sound(void *data, mc_world_runtime *world,
                                             const mc_registry_string *name, double x, double y,
                                             double z, float volume, float pitch,
                                             mc_world_storage_error *error) {
    (void)world;
    (void)name;
    (void)x;
    (void)y;
    (void)z;
    (void)volume;
    (void)pitch;
    (void)error;
    reentry *r = data;
    ++r->sounds;
    if (!r->called) {
        r->called = true;
        r->isolated = r->outer->status == MC_ENTITY_BUSY && r->outer->operation == sentinel &&
                      outstanding == 1;
        r->player->local_player->fields.minecraft = NULL;
        r->status = mc_local_player_action_tick(r->player, r->context, NULL, &r->inner);
        r->player->local_player->fields.minecraft = r->app;
        r->isolated = r->isolated && r->outer->status == MC_ENTITY_BUSY &&
                      r->outer->operation == sentinel && outstanding == 1;
    }
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status reenter_particle(void *data, mc_world_runtime *world,
                                                const mc_registry_string *name, double x, double y,
                                                double z, double vx, double vy, double vz,
                                                mc_world_storage_error *error) {
    (void)world;
    (void)name;
    (void)x;
    (void)y;
    (void)z;
    (void)vx;
    (void)vy;
    (void)vz;
    (void)error;
    ++((reentry *)data)->particles;
    return MC_WORLD_STORAGE_OK;
}

static bool allocation_cases(fixture *f, client_application *app, mc_entity *p,
                             mc_movement_input *input) {
    bool ok = false;
    mc_player_tick_error error;
    reentry *r = NULL;
    mc_world_access *access = NULL;
    bool registered = false;
    (void)&tick_cases; /* Retain the reused authored fixture without executing its other suite. */
    const mc_player_action_context context = {
        .travel = {.code_registry = app->registry, .trig = &trig}};
    CHECK(reset_tick(f, p, input));
    const size_t holds = p->holds, borrows = app->registry->runtime_borrows;
    const int32_t before = p->base.bt;
    fail_next = true;
    CHECK(mc_player_tick(p, &context, NULL, &error) == MC_ENTITY_ALLOCATION_FAILED &&
          error.player_bytecode == 0 && error.local_bytecode == 0 &&
          error.child == MC_PLAYER_TICK_CHILD_NONE && p->base.bt == before && p->holds == holds &&
          app->registry->runtime_borrows == borrows && outstanding == 0);
    fail_next = true;
    CHECK(mc_player_tick(p, &context, NULL, NULL) == MC_ENTITY_ALLOCATION_FAILED &&
          p->base.bt == before && outstanding == 0);
    const size_t previous_attempts = attempts;
    CHECK(mc_player_tick(NULL, &context, NULL, &error) == MC_ENTITY_INVALID_ARGUMENT &&
          attempts == previous_attempts);
    CHECK(mc_player_tick(p, &context, NULL, &error) == MC_ENTITY_OK &&
          error.status == MC_ENTITY_OK && error.operation == NULL &&
          p->base.bt == mc_java_i32_add(before, 1) && outstanding == 0 &&
          allocations == retirements);
    mc_client_stats_writer *writer = app->stats_writer;
    app->stats_writer = NULL;
    const mc_entity_status failed = mc_player_tick(p, &context, NULL, &error);
    app->stats_writer = writer;
    CHECK(failed == MC_ENTITY_STATE_ERROR && error.local_bytecode == 10 && outstanding == 0 &&
          allocations == retirements && p->holds == holds);
    for (size_t i = 0; i < 64; ++i) {
        CHECK(mc_entity_bounds_pool_reset(p->classes, NULL) == MC_ENTITY_OK &&
              mc_entity_vector_pool_reset(p->classes, NULL) == MC_ENTITY_OK);
        CHECK(mc_player_tick(p, &context, NULL, &error) == MC_ENTITY_OK && outstanding == 0 &&
              allocations == retirements && p->holds == holds);
    }
    CHECK(attempts == allocations + 2 && allocations == retirements &&
          app->registry->runtime_borrows == borrows);
    /* The portal branch must expose its actual missing screen owner after
     * the tutorial, rather than silently running the ordinary action body. */
    CHECK(reset_tick(f, p, input));
    p->local_player->fields.player.A = true;
    p->local_player->fields.player.B = .375f;
    CHECK(mc_local_player_action_tick(p, &context, NULL, &error) == MC_ENTITY_STATE_ERROR &&
          error.local_bytecode == 70 && p->local_player->fields.player.C == .375f &&
          p->local_player->fields.player.A && outstanding == 0);
    /* A finite work policy fails before the next source loop iteration.
     * Large finite float angles can remain unchanged after adding 360. */
    CHECK(reset_tick(f, p, input));
    p->base.yaw = 1.0e30f;
    mc_player_tick_limits limited = {
        .action = {.max_loop_work = MC_PLAYER_ACTION_DEFAULT_LOOP_WORK},
        .environment = {
            .max_particle_work = MC_ENTITY_TICK_DEFAULT_PARTICLE_WORK,
            .material = {.max_cell_work = MC_WORLD_MATERIAL_MOVEMENT_DEFAULT_CELL_WORK}}};
    CHECK(mc_living_tick_body(p, &context, &limited, &error) == MC_ENTITY_LIMIT &&
          error.animation_work == 0 && error.living_bytecode == 242 && outstanding == 0);
    CHECK(reset_tick(f, p, input));
    for (int x = 7; x <= 10; ++x)
        for (int z = 7; z <= 10; ++z)
            for (int y = 7; y <= 10; ++y)
                f->states[0]->data->blocks[boxes_index(x, y, z)] = 8;
    p->base.private_c = false;
    p->base.velocity_x = .1;
    p->base.velocity_y = -.2;
    p->base.velocity_z = .15;
    r = calloc(1, sizeof(*r));
    CHECK(r != NULL);
    r->player = p;
    r->app = app;
    r->context = &context;
    r->outer = &error;
    const mc_world_access_callbacks callbacks = {.sound = reenter_sound,
                                                 .particle = reenter_particle};
    CHECK(mc_world_access_create(&callbacks, r, &access, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_add_world_access(f->worlds[0], access, NULL) == MC_WORLD_STORAGE_OK);
    registered = true;
    error.status = MC_ENTITY_BUSY;
    error.operation = sentinel;
    const mc_entity_status reentered = mc_player_tick(p, &context, NULL, &error);
    bool removed;
    CHECK(mc_world_runtime_remove_world_access(f->worlds[0], access, &removed, NULL) ==
              MC_WORLD_STORAGE_OK &&
          removed);
    registered = false;
    CHECK(reentered == MC_ENTITY_OK && r->called && r->isolated &&
          r->status == MC_ENTITY_STATE_ERROR && r->inner.local_bytecode == 4 && r->sounds > 0 &&
          r->particles > 0 && peak == 2 && outstanding == 0 && allocations == retirements &&
          p->holds == holds);
    ok = true;
cleanup:
    if (registered) {
        bool cleanup_removed;
        (void)mc_world_runtime_remove_world_access(f->worlds[0], access, &cleanup_removed, NULL);
    }
    if (access != NULL && mc_world_access_free(access, NULL) != MC_WORLD_STORAGE_OK)
        ok = false;
    free(r);
    return ok;
}

int main(int argc, char **argv) {
    return mc_active_allocation_fixture_main(argc, argv);
}
