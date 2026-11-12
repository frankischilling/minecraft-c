/* Genuine Mob/World/Registry constructors and checked native teardown. */
int death_owned_unused_wolf_main(int, char **);
#define MC_WOLF_ATTACK_BODY_MAIN_NAME death_owned_unused_wolf_main
#include "../test_wolf_attack.c"
#undef MC_WOLF_ATTACK_BODY_MAIN_NAME
#undef main

#include "entity/mob/death_execution_internal.h"
#include "entity/mob/death_internal.h"
#include "entity/tick/execution_internal.h"
#include "world/storage/generated_internal.h"

#undef CHECK
#define CHECK(c)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(c)) {                                                                                \
            fprintf(stderr, "Owned Mob death check%u line%d: %s\n", checks, __LINE__, #c);         \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

static bool death_owned_control(const char *resources, const char *directory) {
    random_fixture *f = calloc(1, sizeof(*f));
    mc_entity_effect_error *error = calloc(1, sizeof(*error));
    mc_entity *actor = NULL;
    CHECK(f != NULL && error != NULL && setup(f, resources, directory));
    uint8_t *blocks = calloc(MC_CHUNK_BLOCKS, 1);
    mc_world_chunk *object = NULL;
    CHECK(blocks != NULL &&
          mc_world_store_create_generated(f->store, f->world, 0, 0, 0, &blocks, MC_CHUNK_BLOCKS,
                                          &object, NULL) == MC_WORLD_STORAGE_OK &&
          blocks == NULL);
    mc_chunk_state *chunk = mc_world_chunk_state(object);
    CHECK(chunk != NULL && mc_chunk_cache_publish(f->provider.cache, 0, 0, chunk));
    f->owned[f->owned_count++] = chunk;
    CHECK(wolf_actor(f, MC_ENTITY_CLASS_KIND_SQUID, &actor) &&
          mc_entity_set_position(actor, 8.5, 64.5, 8.5) == MC_ENTITY_OK);
    mc_java_random_reseed(actor->random, 3);
    mc_math_execution math;
    mc_entity_tick_execution execution;
    mc_entity_tick_execution_world_token outer;
    mc_math_execution_init(&math, NULL);
    mc_entity_tick_execution_init(&execution, &math);
    CHECK(mc_entity_tick_execution_push_world(
              &execution, f->world,
              (mc_entity_tick_execution_site){"native.death.control", "run", "()V", 0},
              &outer) == MC_WORLD_STORAGE_OK);
    /* Fatal Squid q must create its first actual Stack/Item, then stop at
     * missing defining IN in FD.b8, retaining that same child before index1. */
    const mc_world_runtime_execution_code code = {{"lm", "e", "()V", 6}, {"lm", "e", "()V", 33}};
    mc_world_runtime_execution_result owned;
    mc_entity_status raw = MC_ENTITY_WORLD_FAILED;
    const bool returned = mc_mob_death_entered_in_execution(&execution, f->classes, code, actor,
                                                            f->registry, NULL, &raw, error, &owned);
    mc_mob_death_state pending;
    const bool passed =
        !returned && raw == MC_ENTITY_OK && error->status == MC_ENTITY_OK &&
        mc_mob_death_inspect(actor, &pending) && pending.quantity == 3 &&
        pending.quantity_returned && pending.drops_attempted == 1 && pending.drops_returned == 0 &&
        pending.drops_retired == 0 && pending.index == 0 && pending.q_pc == 32 &&
        !pending.q_returned && !pending.status_entered && pending.pending && math.failed &&
        math.first.caller.pc == 8 && actor->random->state == UINT64_C(0xbb26934cf0e1) &&
        f->world->fields.b->count == 0;
    if (!passed)
        fputs("Fatal Mob death must stop at its first owned drop without advancing q\n", stderr);
    mc_mob_death_execution_view death;
    mc_entity_drop_state drop;
    mc_item_stack_fields stack;
    CHECK(mc_mob_death_execution_inspect(actor, &death) && death.has_child &&
          !death.drop_returned && !death.completed && death.drop_status == MC_ENTITY_OK &&
          death.child.result != death.result && death.result != &owned &&
          mc_entity_drop_inspect(death.drop, &drop) && drop.item_constructed &&
          drop.pickup_delay_stored && drop.item->phase == MC_ENTITY_READY &&
          drop.item->item.c == 10 &&
          mc_item_stack_fields_get(drop.stack, &stack) == MC_ITEM_STACK_OK && stack.id == 351 &&
          stack.count == 1 && stack.damage == 0);
    const mc_java_random before_cleanup = *actor->random;
    const uint64_t shared = atomic_load(&f->profile->math->state);
    death = (mc_mob_death_execution_view){0};
    drop = (mc_entity_drop_state){0};
    owned = (mc_world_runtime_execution_result){0};
    CHECK(mc_entity_effect_cleanup(actor, NULL) == MC_ENTITY_OK &&
          mc_test_java_random_equal(actor->random, &before_cleanup) &&
          atomic_load(&f->profile->math->state) == shared && f->world->fields.b->count == 0 &&
          mc_entity_tick_execution_pop_world(&execution, &outer, true) == MC_WORLD_STORAGE_OK &&
          mc_entity_tick_execution_retire(&execution) == MC_WORLD_STORAGE_OK &&
          mc_math_execution_retire(&math) == MC_MATH_CLASS_OK &&
          mc_world_runtime_retire_entity_containers(f->world, NULL) == MC_WORLD_STORAGE_OK &&
          mc_mob_retire(&actor, NULL) == MC_MOB_OK && teardown(f));
    free(error);
    free(f);
    CHECK(passed);
    return true;
}

int main(int argc, char **argv) {
    if (argc != 4 || strcmp(argv[1], "math-missing") != 0)
        return 2;
    if (!death_owned_control(argv[2], argv[3]))
        return 1;
    printf("PASS: %u execution-owned Mob death checks;mode=%s\n", checks, argv[1]);
    return 0;
}
