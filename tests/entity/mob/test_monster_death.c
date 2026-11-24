/* Fatal Monster controls on actual World, Registry and published Item owners. */
int mc_monster_death_wolf_fixture_main(int, char **);
#define MC_WOLF_ATTACK_BODY_MAIN_NAME mc_monster_death_wolf_fixture_main
#include "../test_wolf_attack.c"
#undef MC_WOLF_ATTACK_BODY_MAIN_NAME
#undef main

#include "entity/mob/death_internal.h"
#include "mc/item/stack/stack.h"
#include "world/storage/generated_internal.h"

#undef CHECK
#define CHECK(c)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(c)) {                                                                                \
            fprintf(stderr, "Monster death check%u line%d: %s\n", checks, __LINE__, #c);           \
            if (f->error != NULL)                                                                  \
                fprintf(stderr, "operation=%s site=%s.%s:%d child=%s.%s:%d\n",                     \
                        f->error->operation == NULL ? "none" : f->error->operation,                \
                        f->error->death.site.class_name, f->error->death.site.method,              \
                        f->error->death.site.pc, f->error->death.child.class_name,                 \
                        f->error->death.child.method, f->error->death.child.pc);                   \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

typedef struct monster_death_fixture {
    random_fixture base;
    mc_entity *actor, *source;
    mc_world_access *observer;
    mc_entity_effect_error *error, *reentry;
    mc_entity *items[4];
    size_t added, refuse_at, sounds;
    const char *sound;
    bool bad_callback, null_world, null_random;
    mc_java_random *saved_random;
    const char *resources, *directory;
} monster_death_fixture;

static size_t chunk_members(monster_death_fixture *f, const mc_entity *item) {
    const mc_world_chunk *chunk = mc_world_store_chunk_object(f->base.store, f->base.owned[0]);
    size_t count = 0;
    for (size_t i = 0; i < chunk->m->length; ++i)
        for (size_t j = 0; j < chunk->m->items[i]->count; ++j)
            if (chunk->m->items[i]->items[j] == item)
                ++count;
    return count;
}

static bool world_member(monster_death_fixture *f, const mc_entity *item) {
    for (size_t i = 0; i < f->base.world->fields.b->count; ++i)
        if (f->base.world->fields.b->items[i] == item)
            return true;
    return false;
}

static mc_world_storage_status added(void *context, mc_world_runtime *world, mc_entity *item,
                                     mc_world_storage_error *error) {
    monster_death_fixture *f = context;
    mc_mob_death_state pending;
    mc_item_stack_fields fields;
    if (world != f->base.world || f->added >= COUNT(f->items) ||
        !mc_mob_death_inspect(f->actor, &pending) || !pending.active || !pending.pending ||
        !f->actor->mob->living.ah || !world_member(f, item) || chunk_members(f, item) != 1 ||
        mc_item_stack_fields_get(item->stack, &fields) != MC_ITEM_STACK_OK || fields.count != 1 ||
        fields.damage != 0 || item->item.c != 10 || item->base.be || !item->base.bF ||
        mc_entity_effect_cleanup(f->actor, f->reentry) != MC_ENTITY_BUSY)
        f->bad_callback = true;
    if (f->added < COUNT(f->items))
        f->items[f->added++] = item;
    if (f->null_world)
        f->actor->base.world = NULL;
    if (f->null_random && f->saved_random == NULL) {
        f->saved_random = f->actor->random;
        f->actor->random = NULL;
    }
    *error = (mc_world_storage_error){
        .status = f->added == f->refuse_at ? MC_WORLD_STORAGE_IO_ERROR : MC_WORLD_STORAGE_OK,
        .operation = f->added == f->refuse_at ? "authored Monster publication refusal" : NULL};
    return error->status;
}

static mc_world_storage_status monster_death_sound(void *context, mc_world_runtime *world,
                                                   const mc_registry_string *name, double x,
                                                   double y, double z, float volume, float pitch,
                                                   mc_world_storage_error *error) {
    monster_death_fixture *f = context;
    mc_registry_string_view text = {0};
    if (world != f->base.world || !mc_registry_string_read(name, &text) ||
        text.length != strlen(f->sound) || x != f->actor->base.x ||
        y != f->actor->base.y - (double)f->actor->base.offset || z != f->actor->base.z ||
        volume != 1.f || !(pitch >= .8f && pitch <= 1.2f) || f->added != 0 ||
        f->actor->mob->living.ah)
        f->bad_callback = true;
    if (!f->bad_callback)
        for (size_t i = 0; i < text.length; ++i)
            if (text.units[i] != (uint16_t)f->sound[i])
                f->bad_callback = true;
    ++f->sounds;
    *error = (mc_world_storage_error){0};
    return MC_WORLD_STORAGE_OK;
}

static bool prepare(monster_death_fixture *f, const char *resources, const char *directory) {
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
    const mc_world_access_callbacks callbacks = {.sound = monster_death_sound,
                                                 .entity_added = added};
    CHECK(mc_world_access_create(&callbacks, f, &f->observer, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_add_world_access(f->base.world, f->observer, NULL) ==
              MC_WORLD_STORAGE_OK);
    f->error = calloc(1, sizeof(*f->error));
    f->reentry = calloc(1, sizeof(*f->reentry));
    CHECK(f->error != NULL && f->reentry != NULL);
    return true;
}

static bool begin(monster_death_fixture *f, mc_entity_class_kind kind, mc_entity_class_kind source,
                  int64_t seed, bool remote) {
    CHECK(prepare(f, f->resources, f->directory));
    CHECK(wolf_actor(&f->base, kind, &f->actor));
    if (source != MC_ENTITY_CLASS_KIND_ENTITY)
        CHECK(wolf_actor(&f->base, source, &f->source));
    CHECK(mc_entity_set_position(f->actor, 8.5, 64.5, 8.5) == MC_ENTITY_OK);
    if (f->source != NULL)
        CHECK(mc_entity_set_position(f->source, 11.5, 64.5, 12.5) == MC_ENTITY_OK);
    mc_java_random_reseed(f->actor->random, seed);
    f->actor->mob->living.Y = 4;
    f->base.world->fields.B = remote;
    f->added = f->sounds = f->refuse_at = 0;
    f->bad_callback = f->null_world = f->null_random = false;
    f->sound = kind == MC_ENTITY_CLASS_KIND_ZOMBIE     ? "mob.zombiedeath"
               : kind == MC_ENTITY_CLASS_KIND_SPIDER   ? "mob.spiderdeath"
               : kind == MC_ENTITY_CLASS_KIND_SKELETON ? "mob.skeletonhurt"
                                                       : "mob.creeperdeath";
    return true;
}

static bool finish(monster_death_fixture *f) {
    /* Source cleanup consumes the failed call before fixture operands are restored. */
    CHECK(mc_entity_effect_cleanup(f->actor, NULL) == MC_ENTITY_OK &&
          !mc_entity_effect_has_pending(f->actor));
    if (f->saved_random != NULL) {
        f->actor->random = f->saved_random;
        f->saved_random = NULL;
    }
    f->actor->base.world = f->base.world;
    bool removed = false;
    CHECK(mc_world_runtime_remove_world_access(f->base.world, f->observer, &removed, NULL) ==
              MC_WORLD_STORAGE_OK &&
          removed && mc_world_access_free(f->observer, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_retire_entity_containers(f->base.world, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_mob_retire(&f->actor, NULL) == MC_MOB_OK);
    if (f->source != NULL)
        CHECK(mc_mob_retire(&f->source, NULL) == MC_MOB_OK);
    CHECK(teardown(&f->base));
    memset(&f->base, 0, sizeof(f->base));
    free(f->error);
    free(f->reentry);
    return true;
}

static bool emit(monster_death_fixture *f, const char *kind, const char *mode, const char *source,
                 int64_t seed, bool remote, bool result) {
    printf("MONSTER_DEATH {\"profile\":\"%s-%s-%s-%s-%" PRId64
           "\",\"kind\":\"%s\",\"mode\":\"%s\",\"source\":\"%s\",\"remote\":%s,"
           "\"seed\":%" PRId64 ",\"result\":%s,\"dying\":%s,\"dead\":%s,\"health\":%" PRId32
           ",\"random\":\"%016" PRIx64 "\",\"items\":[",
           kind, mode, source, remote ? "remote" : "local", seed, kind, mode, source,
           remote ? "true" : "false", seed, result ? "true" : "null",
           f->actor->mob->living.ah ? "true" : "false", f->actor->base.be ? "true" : "false",
           f->actor->mob->living.Y, f->actor->random->state);
    for (size_t i = 0; i < f->added; ++i) {
        mc_item_stack_fields fields;
        CHECK(mc_item_stack_fields_get(f->items[i]->stack, &fields) == MC_ITEM_STACK_OK);
        printf("%s[%" PRId32 ",%" PRId32 ",%" PRId32 ",%" PRId32 ",%s,%zu]", i ? "," : "",
               fields.id, fields.count, fields.damage, f->items[i]->item.c,
               world_member(f, f->items[i]) ? "true" : "false", chunk_members(f, f->items[i]));
    }
    printf("],\"sounds\":[%s%s%s],\"player\":null}\n", f->sounds ? "\"" : "",
           f->sounds ? f->sound : "", f->sounds ? "\"" : "");
    CHECK(fflush(stdout) == 0);
    return true;
}

static bool profiles(monster_death_fixture *f) {
    const mc_entity_class_kind kinds[] = {MC_ENTITY_CLASS_KIND_ZOMBIE, MC_ENTITY_CLASS_KIND_SPIDER,
                                          MC_ENTITY_CLASS_KIND_SKELETON,
                                          MC_ENTITY_CLASS_KIND_CREEPER};
    const char *names[] = {"uz", "cn", "fr", "gb"};
    const int64_t seeds[] = {0, 2, 3};
    for (size_t k = 0; k < COUNT(kinds); ++k) {
        for (size_t s = 0; s < COUNT(seeds); ++s) {
            CHECK(begin(f, kinds[k], MC_ENTITY_CLASS_KIND_ENTITY, seeds[s], false));
            const size_t holds = f->actor->holds;
            CHECK(mc_mob_death_entered(f->actor, f->base.registry, NULL, f->error));
            CHECK(f->error->death.returned && f->actor->mob->living.ah && !f->bad_callback &&
                  f->actor->holds == holds && !f->error->death.pending &&
                  f->error->death.drops_returned == f->added &&
                  f->error->death.drops_retired == f->added);
            CHECK(emit(f, names[k], "direct", "null", seeds[s], false, false) && finish(f));
        }
    }
    for (size_t k = 0; k < COUNT(kinds); ++k) {
        CHECK(begin(f, kinds[k], MC_ENTITY_CLASS_KIND_COW, 38, false));
        f->actor->mob->living.Y = 4;
        bool result = false;
        CHECK(mc_entity_effect_damage_with_math(f->actor, f->base.registry, f->base.math, f->source,
                                                4, &result, f->error) == MC_ENTITY_OK &&
              result && f->actor->mob->living.Y == 0 && f->actor->mob->living.ah &&
              f->sounds == 1 && !f->bad_callback && f->error->death.returned &&
              f->actor->mob->creature_target == f->source);
        CHECK(emit(f, names[k], "fatal", "cow", 38, false, result) && finish(f));
    }
    for (unsigned int remote = 0; remote < 2; ++remote)
        for (size_t s = 0; s < 2; ++s) {
            CHECK(begin(f, MC_ENTITY_CLASS_KIND_CREEPER, MC_ENTITY_CLASS_KIND_SKELETON, seeds[s],
                        remote != 0));
            CHECK(mc_mob_death_entered(f->actor, f->base.registry, f->source, f->error) &&
                  f->error->death.record_reached && f->error->death.second_quantity_returned &&
                  f->added >= 1 && !f->bad_callback);
            CHECK(emit(f, "gb", "direct", "skeleton", seeds[s], remote != 0, false) && finish(f));
        }
    return true;
}

static bool failures(monster_death_fixture *f) {
    /* Seed3 reaches two Arrows, then two Bones; each failed publication remains
     * visible and cleanup may neither replay the call nor remove earlier Items. */
    for (size_t at = 1; at <= 4; ++at) {
        CHECK(begin(f, MC_ENTITY_CLASS_KIND_SKELETON, MC_ENTITY_CLASS_KIND_COW, 3, false));
        f->refuse_at = at;
        CHECK(!mc_mob_death_entered(f->actor, f->base.registry, f->source, f->error) &&
              f->added == at && !f->bad_callback && !f->error->death.returned &&
              f->error->death.pending && f->actor->mob->living.ah);
        const uint64_t random = f->actor->random->state;
        CHECK(mc_entity_effect_cleanup(f->actor, NULL) == MC_ENTITY_OK &&
              f->actor->random->state == random && f->base.world->fields.b->count == at &&
              f->added == at);
        CHECK(finish(f));
    }
    CHECK(begin(f, MC_ENTITY_CLASS_KIND_SKELETON, MC_ENTITY_CLASS_KIND_ENTITY, 3, false));
    f->null_world = true;
    CHECK(!mc_mob_death_entered(f->actor, f->base.registry, NULL, f->error) && f->added == 1 &&
          f->error->death.source_exception && !f->error->death.returned && !f->bad_callback);
    CHECK(finish(f));
    CHECK(begin(f, MC_ENTITY_CLASS_KIND_SKELETON, MC_ENTITY_CLASS_KIND_ENTITY, 3, false));
    f->null_random = true;
    CHECK(!mc_mob_death_entered(f->actor, f->base.registry, NULL, f->error) && f->added == 2 &&
          f->error->death.q_pc == 39 && f->error->death.quantity == 2 &&
          f->error->death.second_quantity_attempted && !f->error->death.second_quantity_returned &&
          f->error->death.source_exception && !f->bad_callback);
    CHECK(finish(f));
    CHECK(begin(f, MC_ENTITY_CLASS_KIND_SKELETON, MC_ENTITY_CLASS_KIND_ENTITY, 0, false));
    CHECK(!mc_mob_death_entered(f->actor, NULL, NULL, f->error) && f->added == 0 &&
          f->error->death.quantity_returned && f->error->death.quantity == 0 &&
          f->error->death.second_quantity_returned && f->error->death.second_quantity == 1 &&
          f->error->death.q_pc == 51 && f->error->death.native_policy &&
          !f->error->death.source_exception);
    CHECK(finish(f));
    CHECK(begin(f, MC_ENTITY_CLASS_KIND_CREEPER, MC_ENTITY_CLASS_KIND_SKELETON, 2, false));
    f->null_random = true;
    CHECK(!mc_mob_death_entered(f->actor, f->base.registry, f->source, f->error) &&
          f->error->death.record_reached && f->error->death.creeper_pc == 24 &&
          f->error->death.source_exception && !f->error->death.returned && !f->bad_callback);
    CHECK(finish(f));
    return true;
}

int main(int argc, char **argv) {
    if (argc != 3)
        return 2;
    monster_death_fixture *f = calloc(1, sizeof(*f));
    if (f == NULL)
        return 1;
    f->resources = argv[1];
    f->directory = argv[2];
    if (!profiles(f) || !failures(f))
        return 1;
    free(f);
    printf("PASS: %u Monster death source-order/ownership checks\n", checks);
    return 0;
}
