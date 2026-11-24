/* Actual canonical Mob/World constructors; imported main is never executed. */
int death_profiles_unused_wolf_main(int, char **);
#define MC_WOLF_ATTACK_BODY_MAIN_NAME death_profiles_unused_wolf_main
#include "../test_wolf_attack.c"
#undef MC_WOLF_ATTACK_BODY_MAIN_NAME
#undef main

#include "entity/mob/death_execution_internal.h"
#include "world/runtime/world_access_internal.h"
#include "world/storage/generated_internal.h"

typedef struct death_owned_fixture {
    random_fixture base;
    mc_entity *actor, *source, *items[4];
    mc_entity_effect_error *error, *reentry;
    mc_math_execution math;
    mc_entity_tick_execution execution;
    mc_entity_tick_execution_world_token outer;
    mc_world_runtime_execution_result result;
    mc_world_access *observer;
    mc_world_access_receipt *receipt;
    size_t added, refuse_at, saved_bindings;
    bool valid, native_stop, removed;
} death_owned_fixture;

#undef CHECK
#define CHECK(c)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(c)) {                                                                                \
            fprintf(stderr, "Owned death profile check%u line%d: %s\n", checks, __LINE__, #c);     \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

static const mc_world_runtime_execution_code death_code = {{"lm", "e", "()V", 6},
                                                           {"lm", "e", "()V", 33}};

static size_t death_members(const death_owned_fixture *f, const mc_entity *item) {
    const mc_world_chunk *chunk = mc_world_store_chunk_object(f->base.store, f->base.owned[0]);
    size_t count = 0;
    for (size_t i = 0; i < chunk->m->length; ++i)
        for (size_t j = 0; j < chunk->m->items[i]->count; ++j)
            count += chunk->m->items[i]->items[j] == item;
    return count;
}

static mc_world_storage_status death_added(void *context, mc_world_runtime *world, mc_entity *item,
                                           mc_world_storage_error *error,
                                           mc_world_access_failure *failure) {
    death_owned_fixture *f = context;
    mc_mob_death_state state;
    mc_mob_death_execution_view view;
    mc_entity_drop_state drop;
    mc_item_stack_fields stack;
    f->valid &=
        f->added < COUNT(f->items) && world == f->base.world &&
        mc_mob_death_inspect(f->actor, &state) && state.pending && state.active &&
        state.drops_attempted == f->added + 1 && state.drops_returned == f->added &&
        state.status_entered ==
            (state.kind == MC_ENTITY_CLASS_KIND_CREEPER && state.record_reached) &&
        mc_mob_death_execution_inspect(f->actor, &view) && view.has_child && !view.drop_returned &&
        !view.completed && mc_entity_drop_inspect(view.drop, &drop) && drop.active &&
        drop.item == item && drop.item_constructed && drop.pickup_delay_stored &&
        view.child.publication_world.open && view.child.constructor_world.consumed &&
        mc_item_stack_fields_get(item->stack, &stack) == MC_ITEM_STACK_OK && stack.count == 1 &&
        item->item.c == 10 && item->base.bF && world->fields.b->count == f->added + 1 &&
        world->fields.b->items[f->added] == item && death_members(f, item) == 1 &&
        mc_entity_effect_cleanup(f->actor, f->reentry) == MC_ENTITY_BUSY;
    mc_entity_effect_error_init(f->reentry); /* Callback-local borrowed view ends here. */
    if (f->added < COUNT(f->items)) {
        for (size_t i = 0; i < f->added; ++i)
            f->valid &= f->items[i] != item && f->items[i]->stack != item->stack;
        f->items[f->added++] = item;
    }
    if (f->added != f->refuse_at)
        return MC_WORLD_STORAGE_OK;
    if (f->native_stop) {
        f->saved_bindings = world->persistent_bindings;
        world->persistent_bindings = 0; /* Temporary downward native counter fault. */
        return MC_WORLD_STORAGE_OK;
    }
    bool removed = false;
    if (mc_world_runtime_remove_world_access(world, f->observer, &removed, error) !=
            MC_WORLD_STORAGE_OK ||
        !removed)
        return MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    f->removed = true;
    const mc_world_storage_status created =
        mc_world_access_receipt_create(f->observer, NULL, &f->receipt, failure, error);
    if (created != MC_WORLD_STORAGE_OK)
        return created;
    f->receipt->exception_class = "java/lang/IllegalStateException";
    *error = (mc_world_storage_error){MC_WORLD_STORAGE_IO_ERROR,
                                      "Owned death actual publication callback refusal", 61};
    return error->status;
}

static bool death_setup(death_owned_fixture *f, const char *resources, const char *directory,
                        mc_entity_class_kind kind, int64_t seed, bool remote, bool missing) {
    CHECK(setup(&f->base, resources, directory));
    uint8_t *blocks = calloc(MC_CHUNK_BLOCKS, 1);
    mc_world_chunk *object = NULL;
    CHECK(blocks != NULL &&
          mc_world_store_create_generated(f->base.store, f->base.world, 0, 0, 0, &blocks,
                                          MC_CHUNK_BLOCKS, &object, NULL) == MC_WORLD_STORAGE_OK &&
          blocks == NULL);
    mc_chunk_state *chunk = mc_world_chunk_state(object);
    CHECK(chunk != NULL && mc_chunk_cache_publish(f->base.provider.cache, 0, 0, chunk));
    f->base.owned[f->base.owned_count++] = chunk;
    CHECK(wolf_actor(&f->base, kind, &f->actor));
    if (kind == MC_ENTITY_CLASS_KIND_CREEPER)
        CHECK(wolf_actor(&f->base, MC_ENTITY_CLASS_KIND_SKELETON, &f->source));
    else if (kind == MC_ENTITY_CLASS_KIND_SKELETON)
        CHECK(wolf_actor(&f->base, MC_ENTITY_CLASS_KIND_COW, &f->source));
    if (kind == MC_ENTITY_CLASS_KIND_SLIME)
        CHECK(mc_mob_slime_set_size(f->actor, 1, NULL) == MC_MOB_OK);
    CHECK(mc_entity_set_position(f->actor, 8.5, 64.5, 8.5) == MC_ENTITY_OK);
    mc_java_random_reseed(f->actor->random, seed);
    f->base.world->fields.B = remote;
    f->error = calloc(1, sizeof(*f->error));
    f->reentry = calloc(1, sizeof(*f->reentry));
    CHECK(f->error != NULL && f->reentry != NULL);
    mc_entity_effect_error_init(f->error);
    mc_entity_effect_error_init(f->reentry);
    mc_math_execution_init(&f->math, missing ? NULL : f->base.math);
    mc_entity_tick_execution_init(&f->execution, &f->math);
    CHECK(mc_entity_tick_execution_push_world(
              &f->execution, f->base.world,
              (mc_entity_tick_execution_site){"native.death.profiles", "run", "()V", 0},
              &f->outer) == MC_WORLD_STORAGE_OK);
    const mc_world_access_source_callbacks callbacks = {.entity_added = death_added};
    CHECK(mc_world_access_source_create(&callbacks, f, &f->observer, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_add_world_access(f->base.world, f->observer, NULL) ==
              MC_WORLD_STORAGE_OK);
    f->valid = true;
    return true;
}

static bool death_finish(death_owned_fixture *f, bool completed) {
    mc_entity_effect_error_init(f->reentry);
    CHECK(mc_entity_effect_error_retire(f->error) && !mc_entity_effect_has_pending(f->actor));
    f->result = (mc_world_runtime_execution_result){0};
    mc_entity_effect_error_init(f->reentry); /* Expire its callback/duplicate-entry borrows. */
    CHECK(mc_entity_tick_execution_pop_world(&f->execution, &f->outer, !completed) ==
              MC_WORLD_STORAGE_OK &&
          mc_entity_tick_execution_retire(&f->execution) == MC_WORLD_STORAGE_OK &&
          mc_math_execution_retire(&f->math) == MC_MATH_CLASS_OK);
    if (f->receipt != NULL)
        CHECK(mc_world_access_receipt_retire(&f->receipt, NULL) == MC_WORLD_STORAGE_OK);
    if (!f->removed) {
        bool removed = false;
        CHECK(mc_world_runtime_remove_world_access(f->base.world, f->observer, &removed, NULL) ==
                  MC_WORLD_STORAGE_OK &&
              removed);
    }
    CHECK(mc_world_access_free(f->observer, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_retire_entity_containers(f->base.world, NULL) == MC_WORLD_STORAGE_OK &&
          mc_mob_retire(&f->actor, NULL) == MC_MOB_OK);
    if (f->source != NULL)
        CHECK(mc_mob_retire(&f->source, NULL) == MC_MOB_OK);
    CHECK(teardown(&f->base));
    free(f->error);
    free(f->reentry);
    memset(f, 0, sizeof(*f));
    return true;
}

static bool death_profiles(death_owned_fixture *f, const char *resources, const char *directory,
                           bool remote) {
    const mc_entity_class_kind kinds[] = {
        MC_ENTITY_CLASS_KIND_SHEEP,   MC_ENTITY_CLASS_KIND_PIG,      MC_ENTITY_CLASS_KIND_COW,
        MC_ENTITY_CLASS_KIND_CHICKEN, MC_ENTITY_CLASS_KIND_WOLF,     MC_ENTITY_CLASS_KIND_ZOMBIE,
        MC_ENTITY_CLASS_KIND_SPIDER,  MC_ENTITY_CLASS_KIND_SKELETON, MC_ENTITY_CLASS_KIND_CREEPER,
        MC_ENTITY_CLASS_KIND_SLIME,   MC_ENTITY_CLASS_KIND_SQUID,    MC_ENTITY_CLASS_KIND_GHAST};
    const int64_t seeds[] = {0, 2, 3};
    const size_t skeleton[] = {1, 1, 4};
    for (size_t k = 0; k < COUNT(kinds); ++k)
        for (size_t s = 0; s < (remote ? 1u : COUNT(seeds)); ++s) {
            CHECK(death_setup(f, resources, directory, kinds[k], seeds[s], remote,
                              remote && kinds[k] != MC_ENTITY_CLASS_KIND_CREEPER));
            const size_t holds = f->actor->holds;
            const uint64_t random = f->actor->random->state;
            mc_entity_status raw = MC_ENTITY_WORLD_FAILED;
            CHECK(mc_mob_death_entered_in_execution(&f->execution, f->base.classes, death_code,
                                                    f->actor, f->base.registry, f->source, &raw,
                                                    f->error, &f->result));
            const size_t quantity = remote ? (kinds[k] == MC_ENTITY_CLASS_KIND_CREEPER ? 1u : 0u)
                                    : kinds[k] == MC_ENTITY_CLASS_KIND_SHEEP    ? 1u
                                    : kinds[k] == MC_ENTITY_CLASS_KIND_WOLF     ? 0u
                                    : kinds[k] == MC_ENTITY_CLASS_KIND_SQUID    ? s + 1u
                                    : kinds[k] == MC_ENTITY_CLASS_KIND_SKELETON ? skeleton[s]
                                    : kinds[k] == MC_ENTITY_CLASS_KIND_CREEPER  ? s + 1u
                                                                                : s;
            CHECK(raw == MC_ENTITY_OK && f->error->death.returned && !f->error->death.pending &&
                  f->error->death.status_returned && f->error->death.drops_returned == quantity &&
                  f->error->death.drops_retired == quantity && f->added == quantity && f->valid &&
                  f->actor->holds == holds && f->base.world->fields.b->count == quantity);
            if (quantity != 0)
                CHECK(f->math.owned == f->base.math && !f->math.failed);
            else {
                CHECK(f->math.owned == NULL && !f->math.failed);
                if (remote || kinds[k] == MC_ENTITY_CLASS_KIND_WOLF)
                    CHECK(f->actor->random->state == random);
                else
                    CHECK(f->error->death.quantity_returned &&
                          f->actor->random->state == UINT64_C(0xbb20b4600a74));
            }
            if (kinds[k] == MC_ENTITY_CLASS_KIND_CREEPER)
                CHECK(f->error->death.record_reached);
            CHECK(death_finish(f, true));
        }
    return true;
}

static bool death_failure(death_owned_fixture *f, const char *resources, const char *directory,
                          size_t at, bool native_stop) {
    CHECK(death_setup(f, resources, directory,
                      native_stop ? MC_ENTITY_CLASS_KIND_SQUID : MC_ENTITY_CLASS_KIND_SKELETON, 3,
                      false, false));
    f->refuse_at = at;
    f->native_stop = native_stop;
    mc_entity_status raw = MC_ENTITY_WORLD_FAILED;
    const bool returned =
        mc_mob_death_entered_in_execution(&f->execution, f->base.classes, death_code, f->actor,
                                          f->base.registry, f->source, &raw, f->error, &f->result);
    bool refused_retire = false;
    if (native_stop)
        refused_retire = !mc_entity_effect_error_retire(f->error);
    if (f->saved_bindings != 0)
        f->base.world->persistent_bindings = f->saved_bindings;
    mc_mob_death_execution_view view;
    mc_entity_drop_state drop;
    CHECK(!returned && f->added == at && f->valid && f->error->death.pending &&
          !f->error->death.q_returned && !f->error->death.status_entered &&
          f->error->death.drops_attempted == at && f->error->death.drops_returned == at - 1 &&
          f->error->death.drops_retired == at - 1 &&
          mc_mob_death_execution_inspect(f->actor, &view) && view.has_child && !view.completed &&
          !view.drop_returned && mc_entity_drop_inspect(view.drop, &drop) &&
          drop.item == f->items[at - 1] && f->base.world->fields.b->count == at);
    if (native_stop)
        CHECK(refused_retire && raw == MC_ENTITY_OK && f->error->status == MC_ENTITY_OK &&
              view.drop_status == MC_ENTITY_OK && view.child.publication_world.open &&
              view.child.publication_world.close_attempted &&
              !view.child.publication_world.failed &&
              view.child.result->ownership_status == MC_WORLD_STORAGE_UNSUPPORTED_STATE);
    else
        CHECK(raw == MC_ENTITY_WORLD_FAILED && f->error->death.source_exception &&
              !strcmp(f->error->death.exception_class, "java/lang/IllegalStateException") &&
              f->receipt != NULL && f->receipt->holds == 1 &&
              view.child.result->ownership_status == MC_WORLD_STORAGE_OK);
    const mc_java_random random = *f->actor->random;
    const uint64_t shared = atomic_load(&f->base.profile->math->state);
    const uint64_t unique = atomic_load(&f->base.profile->uniquifier);
    const int32_t next_id = f->base.entities->next_id;
    view = (mc_mob_death_execution_view){0};
    drop = (mc_entity_drop_state){0};
    CHECK(mc_entity_effect_error_retire(f->error) && !f->error->death.pending &&
          !f->error->death.returned && !f->error->death.q_returned &&
          !f->error->death.status_entered && f->error->death.drops_returned == at - 1 &&
          mc_test_java_random_equal(f->actor->random, &random) &&
          atomic_load(&f->base.profile->math->state) == shared &&
          atomic_load(&f->base.profile->uniquifier) == unique &&
          f->base.entities->next_id == next_id && f->base.world->fields.b->count == at);
    for (size_t i = 0; i < at; ++i)
        CHECK(f->base.world->fields.b->items[i] == f->items[i] &&
              f->items[i]->phase == MC_ENTITY_READY && f->items[i]->item.c == 10 &&
              death_members(f, f->items[i]) == 1);
    return death_finish(f, false);
}

static bool death_admission(death_owned_fixture *f, const char *resources, const char *directory) {
    CHECK(death_setup(f, resources, directory, MC_ENTITY_CLASS_KIND_SQUID, 3, false, true));
    mc_entity_status raw = MC_ENTITY_LIMIT;
    const mc_java_random before = *f->actor->random;
    const size_t holds = f->actor->holds, bindings = f->base.world->persistent_bindings;
    CHECK(!mc_mob_death_entered_in_execution(NULL, f->base.classes, death_code, f->actor,
                                             f->base.registry, NULL, &raw, f->error, &f->result) &&
          raw == MC_ENTITY_LIMIT && f->error->status == MC_ENTITY_OK && !f->error->death.entered &&
          !mc_entity_effect_has_pending(f->actor) && !f->actor->mob->living.ah &&
          mc_test_java_random_equal(f->actor->random, &before) && f->actor->holds == holds &&
          f->base.world->persistent_bindings == bindings &&
          f->result.ownership_status == MC_WORLD_STORAGE_INVALID_ARGUMENT);
    CHECK(!mc_mob_death_entered_in_execution(&f->execution, f->base.classes, death_code, f->actor,
                                             f->base.registry, NULL, &raw, f->error, &f->result) &&
          raw == MC_ENTITY_OK);
    mc_mob_death_execution_view original, duplicate;
    CHECK(mc_mob_death_execution_inspect(f->actor, &original) && original.has_child);
    const mc_java_random random = *f->actor->random;
    const uint64_t shared = atomic_load(&f->base.profile->math->state);
    mc_world_runtime_execution_result other;
    CHECK(!mc_mob_death_entered_in_execution(&f->execution, f->base.classes, death_code, f->actor,
                                             f->base.registry, NULL, &raw, f->reentry, &other) &&
          raw == MC_ENTITY_BUSY && mc_mob_death_execution_inspect(f->actor, &duplicate) &&
          duplicate.drop == original.drop && duplicate.child.result == original.child.result &&
          duplicate.result == original.result && !duplicate.drop_returned &&
          mc_test_java_random_equal(f->actor->random, &random) &&
          atomic_load(&f->base.profile->math->state) == shared && f->added == 0 &&
          f->base.world->fields.b->count == 0);
    original = duplicate = (mc_mob_death_execution_view){0};
    other = (mc_world_runtime_execution_result){0};
    mc_entity_effect_error_init(f->reentry);
    return death_finish(f, false);
}

int main(int argc, char **argv) {
    if (argc != 4)
        return 2;
    death_owned_fixture *f = calloc(1, sizeof(*f));
    if (f == NULL)
        return 1;
    bool passed = false;
    if (strcmp(argv[1], "profiles") == 0)
        passed = death_profiles(f, argv[2], argv[3], false);
    else if (strcmp(argv[1], "remote") == 0)
        passed = death_profiles(f, argv[2], argv[3], true);
    else if (strcmp(argv[1], "callback-1") == 0)
        passed = death_failure(f, argv[2], argv[3], 1, false);
    else if (strcmp(argv[1], "callback-2") == 0)
        passed = death_failure(f, argv[2], argv[3], 2, false);
    else if (strcmp(argv[1], "callback-3") == 0)
        passed = death_failure(f, argv[2], argv[3], 3, false);
    else if (strcmp(argv[1], "callback-4") == 0)
        passed = death_failure(f, argv[2], argv[3], 4, false);
    else if (strcmp(argv[1], "native-stop") == 0)
        passed = death_failure(f, argv[2], argv[3], 2, true);
    else if (strcmp(argv[1], "admission") == 0)
        passed = death_admission(f, argv[2], argv[3]);
    free(f);
    if (passed)
        printf("PASS: %u execution-owned Mob death checks;mode=%s\n", checks, argv[1]);
    return passed ? 0 : 1;
}
