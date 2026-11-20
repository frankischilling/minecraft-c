/* Fatal remaining LivingEntity controls over genuine World and Item owners. */
int mc_remaining_death_wolf_fixture_main(int, char **);
#define MC_WOLF_ATTACK_BODY_MAIN_NAME mc_remaining_death_wolf_fixture_main
#include "../test_wolf_attack.c"
#undef MC_WOLF_ATTACK_BODY_MAIN_NAME
#undef main

#include "entity/mob/death_internal.h"
#include "item/registry/registry_internal.h"
#include "mc/item/stack/stack.h"
#include "mc/world/runtime/world_access.h"
#include "world/storage/generated_internal.h"

#undef CHECK
#define CHECK(c)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(c)) {                                                                                \
            fprintf(stderr, "Remaining death check%u line%d: %s\n", checks, __LINE__, #c);         \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

typedef struct remaining_fixture {
    random_fixture base;
    mc_entity *actor, *source, *items[3];
    mc_entity_effect_error *error, *reentry;
    mc_world_access *observer;
    const char *resources, *directory;
    int32_t item_id;
    size_t added, refuse_at, ink_slot;
    bool bad_callback, clear_random, null_ink;
    mc_java_random *saved_random;
    mc_item *saved_ink;
    mc_entity_watcher *saved_watcher;
    mc_entity_watch_map *saved_map;
    mc_entity_watch_record *changed_record;
    mc_entity_watch_payload_kind saved_payload;
} remaining_fixture;

static size_t members(remaining_fixture *f, const mc_entity *item) {
    const mc_world_chunk *chunk = mc_world_store_chunk_object(f->base.store, f->base.owned[0]);
    size_t count = 0;
    for (size_t i = 0; i < chunk->m->length; ++i)
        for (size_t j = 0; j < chunk->m->items[i]->count; ++j)
            if (chunk->m->items[i]->items[j] == item)
                ++count;
    return count;
}

static bool world_member(remaining_fixture *f, const mc_entity *item) {
    for (size_t i = 0; i < f->base.world->fields.b->count; ++i)
        if (f->base.world->fields.b->items[i] == item)
            return true;
    return false;
}

static mc_world_storage_status remaining_added(void *context, mc_world_runtime *world,
                                               mc_entity *item, mc_world_storage_error *error) {
    remaining_fixture *f = context;
    mc_item_stack_fields stack;
    mc_mob_death_state pending;
    if (f->added >= COUNT(f->items) || world != f->base.world ||
        !mc_mob_death_inspect(f->actor, &pending) || !pending.active || !pending.pending ||
        !f->actor->mob->living.ah || !mc_entity_is_kind(item, MC_ENTITY_CLASS_KIND_ITEM) ||
        mc_item_stack_fields_get(item->stack, &stack) != MC_ITEM_STACK_OK ||
        stack.id != f->item_id || stack.count != 1 || stack.damage != 0 || item->item.c != 10 ||
        !item->base.bF || !world_member(f, item) || members(f, item) != 1 ||
        mc_entity_effect_cleanup(f->actor, f->reentry) != MC_ENTITY_BUSY)
        f->bad_callback = true;
    if (f->added < COUNT(f->items)) {
        for (size_t i = 0; i < f->added; ++i)
            if (f->items[i] == item || f->items[i]->stack == item->stack)
                f->bad_callback = true;
        f->items[f->added++] = item;
    }
    if (f->clear_random && f->added == 1) {
        f->saved_random = f->actor->random;
        f->actor->random = NULL;
    }
    if (f->null_ink && f->added == 1) {
        f->saved_ink = f->base.registry->items.named[f->ink_slot];
        f->base.registry->items.named[f->ink_slot] = NULL;
    }
    *error = (mc_world_storage_error){
        .status = f->added == f->refuse_at ? MC_WORLD_STORAGE_IO_ERROR : MC_WORLD_STORAGE_OK,
        .operation =
            f->added == f->refuse_at ? "authored remaining death publication refusal" : NULL};
    return error->status;
}

static bool prepare(remaining_fixture *f, mc_entity_class_kind kind, int64_t seed, int32_t size,
                    bool remote) {
    CHECK(setup(&f->base, f->resources, f->directory));
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
    if (kind == MC_ENTITY_CLASS_KIND_SLIME)
        CHECK(mc_mob_slime_set_size(f->actor, size, NULL) == MC_MOB_OK);
    CHECK(mc_entity_set_position(f->actor, 8.5, 64.5, 8.5) == MC_ENTITY_OK);
    mc_java_random_reseed(f->actor->random, seed);
    f->base.world->fields.B = remote;
    f->item_id = kind == MC_ENTITY_CLASS_KIND_SLIME   ? 341
                 : kind == MC_ENTITY_CLASS_KIND_SQUID ? 351
                                                      : 289;
    f->added = f->refuse_at = 0;
    f->clear_random = f->null_ink = f->bad_callback = false;
    f->error = calloc(1, sizeof(*f->error));
    f->reentry = calloc(1, sizeof(*f->reentry));
    CHECK(f->error != NULL && f->reentry != NULL);
    const mc_world_access_callbacks callbacks = {.entity_added = remaining_added};
    CHECK(mc_world_access_create(&callbacks, f, &f->observer, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_add_world_access(f->base.world, f->observer, NULL) ==
              MC_WORLD_STORAGE_OK);
    for (size_t i = 0; i < 106; ++i)
        if (strcmp(mc_registry_item_static_name(i), "aU") == 0)
            f->ink_slot = i;
    return true;
}

static bool finish(remaining_fixture *f) {
    CHECK(mc_entity_effect_cleanup(f->actor, NULL) == MC_ENTITY_OK &&
          !mc_entity_effect_has_pending(f->actor));
    if (f->saved_random != NULL) {
        f->actor->random = f->saved_random;
        f->saved_random = NULL;
    }
    if (f->saved_ink != NULL) {
        f->base.registry->items.named[f->ink_slot] = f->saved_ink;
        f->saved_ink = NULL;
    }
    if (f->saved_watcher != NULL) {
        f->actor->watcher = f->saved_watcher;
        f->saved_watcher = NULL;
    }
    if (f->saved_map != NULL) {
        f->actor->watcher->map = f->saved_map;
        f->saved_map = NULL;
    }
    if (f->changed_record != NULL) {
        f->changed_record->payload_kind = f->saved_payload;
        f->changed_record = NULL;
    }
    bool removed = false;
    CHECK(mc_world_runtime_remove_world_access(f->base.world, f->observer, &removed, NULL) ==
              MC_WORLD_STORAGE_OK &&
          removed && mc_world_access_free(f->observer, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_retire_entity_containers(f->base.world, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_mob_retire(&f->actor, NULL) == MC_MOB_OK);
    if (f->source != NULL)
        CHECK(mc_mob_retire(&f->source, NULL) == MC_MOB_OK);
    CHECK(teardown(&f->base));
    memset(&f->base, 0, sizeof(f->base));
    free(f->error);
    free(f->reentry);
    return true;
}

static bool profiles(remaining_fixture *f) {
    const mc_entity_class_kind kinds[] = {MC_ENTITY_CLASS_KIND_SLIME, MC_ENTITY_CLASS_KIND_SQUID,
                                          MC_ENTITY_CLASS_KIND_GHAST};
    const int64_t seeds[] = {0, 2, 3};
    const uint64_t states[] = {UINT64_C(0xbb20b4600a74), UINT64_C(0xbb2c7239d74e),
                               UINT64_C(0xbb26934cf0e1)};
    for (size_t k = 0; k < COUNT(kinds); ++k)
        for (size_t s = 0; s < COUNT(seeds); ++s) {
            CHECK(prepare(f, kinds[k], seeds[s], 1, false));
            const size_t holds = f->actor->holds;
            CHECK(mc_mob_death_entered(f->actor, f->base.registry, NULL, f->error));
            const size_t quantity = s + (kinds[k] == MC_ENTITY_CLASS_KIND_SQUID ? 1u : 0u);
            CHECK(f->error->death.returned && f->error->death.q_returned &&
                  f->error->death.quantity_returned &&
                  f->error->death.quantity == (int32_t)quantity &&
                  f->error->death.drops_returned == quantity &&
                  f->error->death.drops_retired == quantity && !f->error->death.pending &&
                  f->actor->mob->living.ah && !f->actor->base.be && f->actor->holds == holds &&
                  f->actor->random->state == states[s] && f->added == quantity && !f->bad_callback);
            CHECK(finish(f));
        }
    const int32_t sizes[] = {0, 2, 4, -1, 257};
    for (size_t i = 0; i < COUNT(sizes); ++i) {
        CHECK(prepare(f, MC_ENTITY_CLASS_KIND_SLIME, 2, sizes[i], false));
        const uint64_t random = f->actor->random->state;
        const bool tiny = sizes[i] == 257; /* Actual Byte narrowing makes UW.v return1. */
        CHECK(mc_mob_death_entered(f->actor, tiny ? f->base.registry : NULL, NULL, f->error) &&
              f->error->death.returned && f->added == (tiny ? 1u : 0u) &&
              f->error->death.quantity_attempted == tiny &&
              f->actor->random->state == (tiny ? states[1] : random) && !f->bad_callback);
        CHECK(finish(f));
    }
    for (size_t k = 0; k < COUNT(kinds); ++k) {
        CHECK(prepare(f, kinds[k], 3, 1, true));
        f->saved_random = f->actor->random;
        f->actor->random = NULL;
        f->saved_watcher = f->actor->watcher;
        f->actor->watcher = NULL;
        CHECK(mc_mob_death_entered(f->actor, NULL, NULL, f->error) && f->error->death.returned &&
              !f->error->death.q_entered && f->added == 0);
        CHECK(finish(f));
    }
    for (size_t k = 0; k < COUNT(kinds); ++k) {
        CHECK(prepare(f, kinds[k], 0, 2, false) && wolf_actor(&f->base, kinds[k], &f->source));
        f->actor->mob->living.T = 7;
        const size_t source_holds = f->source->holds;
        const uint64_t source_random = f->source->random->state;
        const int32_t source_health = f->source->mob->living.Y;
        CHECK(mc_mob_death_entered(f->actor, f->base.registry, f->source, f->error) &&
              f->error->death.returned && f->error->death.score == 7 &&
              f->error->death.score_callback_entered && f->error->death.score_callback_returned &&
              f->error->death.kill_callback_entered && f->error->death.kill_callback_returned &&
              f->error->death.source_hold_acquired && f->error->death.source_hold_released &&
              f->error->death.source_identity == (uintptr_t)f->source &&
              f->source->holds == source_holds && f->source->random->state == source_random &&
              f->source->mob->living.Y == source_health && !f->bad_callback);
        CHECK(finish(f));
    }
    return true;
}

static bool failures(remaining_fixture *f) {
    for (size_t at = 1; at <= 3; ++at) {
        CHECK(prepare(f, MC_ENTITY_CLASS_KIND_SQUID, 3, 1, false));
        f->refuse_at = at;
        CHECK(!mc_mob_death_entered(f->actor, f->base.registry, NULL, f->error) && f->added == at &&
              !f->bad_callback && f->error->death.pending && !f->error->death.returned &&
              f->error->death.q_pc == 32);
        const uint64_t random = f->actor->random->state;
        CHECK(mc_entity_effect_cleanup(f->actor, NULL) == MC_ENTITY_OK && f->added == at &&
              f->base.world->fields.b->count == at && f->actor->random->state == random);
        CHECK(finish(f));
    }
    CHECK(prepare(f, MC_ENTITY_CLASS_KIND_SQUID, 3, 1, false));
    f->clear_random = true;
    CHECK(mc_mob_death_entered(f->actor, f->base.registry, NULL, f->error) && f->added == 3 &&
          f->actor->random == NULL && !f->bad_callback);
    CHECK(finish(f));
    CHECK(prepare(f, MC_ENTITY_CLASS_KIND_SQUID, 3, 1, false));
    f->null_ink = true;
    CHECK(!mc_mob_death_entered(f->actor, f->base.registry, NULL, f->error) && f->added == 1 &&
          f->error->death.source_exception && f->error->death.q_pc == 28 &&
          f->error->death.child.pc == 2 && !strcmp(f->error->death.child.class_name, "iz") &&
          f->error->death.stack_allocated && !f->error->death.stack_constructed &&
          !f->bad_callback);
    CHECK(finish(f));
    CHECK(prepare(f, MC_ENTITY_CLASS_KIND_SQUID, 3, 1, false));
    CHECK(!mc_mob_death_entered(f->actor, NULL, NULL, f->error) &&
          f->error->death.stack_allocated && !f->error->death.stack_constructed &&
          f->error->death.q_pc == 23 && f->error->death.native_policy &&
          !f->error->death.source_exception && f->added == 0);
    CHECK(finish(f));
    for (unsigned int mode = 0; mode < 4; ++mode) {
        CHECK(prepare(f, MC_ENTITY_CLASS_KIND_SLIME, 3, 1, false));
        if (mode == 0) {
            f->saved_watcher = f->actor->watcher;
            f->actor->watcher = NULL;
        } else if (mode == 1) {
            f->saved_map = f->actor->watcher->map;
            f->actor->watcher->map = NULL;
        } else {
            for (mc_entity_watch_node *n = f->actor->watcher->map->head; n != NULL; n = n->next)
                if (n->key == 16)
                    f->changed_record = n->record;
            CHECK(f->changed_record != NULL);
            f->saved_payload = f->changed_record->payload_kind;
            f->changed_record->payload_kind =
                mode == 2 ? MC_ENTITY_WATCH_PAYLOAD_INTEGER : MC_ENTITY_WATCH_PAYLOAD_NULL;
        }
        const uint64_t random = f->actor->random->state;
        CHECK(!mc_mob_death_entered(f->actor, NULL, NULL, f->error) &&
              f->error->death.source_exception &&
              f->error->death.watcher_pc == (mode == 0   ? 6
                                             : mode == 1 ? 8
                                             : mode == 2 ? 19
                                                         : 22) &&
              !f->error->death.quantity_attempted && f->actor->random->state == random &&
              f->added == 0 && f->actor->mob->living.ah);
        CHECK(finish(f));
    }
    return true;
}

int main(int argc, char **argv) {
    if (argc != 3)
        return 2;
    remaining_fixture *f = calloc(1, sizeof(*f));
    if (f == NULL)
        return 1;
    f->resources = argv[1];
    f->directory = argv[2];
    if (!profiles(f) || !failures(f))
        return 1;
    free(f);
    printf("PASS: %u remaining Living death checks\n", checks);
    return 0;
}
