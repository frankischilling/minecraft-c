/* Genuine GZ caller, Player/App/statistics and hurt producers. Source fields
 * below are declared component inputs; native hold faults are labeled apart. */
int mc_monster_hurt_unused_statistics_main(int, char **);
#define MC_PLAYER_STATISTICS_MAX_PROVIDER_CHUNKS 64
#define main mc_monster_hurt_unused_statistics_main
#include "../client/statistics/test_player_dispatch.c"
#undef main

#include "entity/mob/references_internal.h"
#include "entity/mob/tick/internal.h"
#include "entity/player/damage_wake_internal.h"
#include "mc/entity/mob/construction.h"
#include "mc/world/runtime/entities.h"
#include "mc/world/runtime/entity_tick.h"
#include "world/runtime/chunk_provider_internal.h"
#include "world/runtime/world_access_internal.h"
#include "world/storage/objects_internal.h"

typedef struct hurt_sound {
    mc_world_access *receiver;
    mc_world_access_receipt *receipt;
    size_t calls;
} hurt_sound;

static mc_world_storage_status hurt_native_sound(void *context, mc_world_runtime *world,
                                                 const mc_registry_string *name, double x, double y,
                                                 double z, float volume, float pitch,
                                                 mc_world_storage_error *error) {
    hurt_sound *s = context;
    (void)world;
    (void)name;
    (void)x;
    (void)y;
    (void)z;
    (void)volume;
    (void)pitch;
    ++s->calls;
    *error = (mc_world_storage_error){MC_WORLD_STORAGE_IO_ERROR,
                                      "Authored Monster target sound refusal", 61};
    return error->status;
}

static mc_world_storage_status hurt_source_sound(void *context, mc_world_runtime *world,
                                                 const mc_registry_string *name, double x, double y,
                                                 double z, float volume, float pitch,
                                                 mc_world_storage_error *error,
                                                 mc_world_access_failure *failure) {
    hurt_sound *s = context;
    const mc_world_storage_status status =
        hurt_native_sound(context, world, name, x, y, z, volume, pitch, error);
    const mc_world_storage_error first = *error;
    const mc_world_storage_status created =
        mc_world_access_receipt_create(s->receiver, NULL, &s->receipt, failure, error);
    if (created != MC_WORLD_STORAGE_OK)
        return created;
    s->receipt->exception_class = "java/lang/IllegalStateException";
    *error = first;
    return status;
}

static bool hurt_actor(fixture *f, mc_entity **out) {
    const mc_entity_class_literal *literal = NULL;
    mc_mob_error error;
    CHECK(mc_entity_class_literal_get(f->app.entities, MC_ENTITY_CLASS_KIND_ZOMBIE, &literal) ==
              MC_ENTITY_OK &&
          mc_mob_allocate(f->app.entities, f->app.registry, literal, out, &error) == MC_MOB_OK &&
          mc_mob_construct(*out, f->world, &error) == MC_MOB_OK);
    CHECK((*out)->mob->monster_damage == 5 && (*out)->mob->living.Y == 20);
    CHECK(mc_entity_set_position(*out, 9.5, 200.5, 8.5) == MC_ENTITY_OK &&
          mc_entity_set_position(f->player, 8.5, 200.5, 8.5) == MC_ENTITY_OK);
    (*out)->mob->living.ae = 0;
    f->world->fields.B = false;
    f->world->fields.q = 2;
    return true;
}

static bool hurt_caller(const mc_mob_tick_error *e, int32_t amount, bool returned) {
    const mc_mob_tick_attack_arguments *a = &e->state.attack;
    CHECK(a->stored && a->hurt_invoked && a->hurt_damage == amount && a->hurt_caller.pc == 61 &&
          strcmp(a->hurt_caller.class_name, "gz") == 0 && strcmp(a->hurt_caller.method, "a") == 0 &&
          strcmp(a->hurt_caller.descriptor, "(Lsn;F)V") == 0 && a->hurt_returned == returned &&
          a->returned == returned && e->state.returned == returned &&
          e->site.pc == (returned ? 65 : 61));
    if (returned)
        CHECK(a->caller_return_site.pc == 65 &&
              strcmp(a->caller_return_site.class_name, "gz") == 0 &&
              strcmp(a->caller_return_site.method, "a") == 0 &&
              strcmp(a->caller_return_site.descriptor, "(Lsn;F)V") == 0);
    return true;
}

static bool hurt_value(fixture *f, int32_t *out) {
    mc_registry_statistic_view stats;
    CHECK(mc_registry_classes_inspect_statistics(f->app.registry, &stats, NULL) == MC_REGISTRY_OK &&
          mc_client_stats_writer_value(f->app.stats_writer, stats.named[18], out, NULL) ==
              MC_CLIENT_STATS_OK);
    return true;
}

static bool hurt_one(const char *resources, unsigned int mode) {
    fixture *f = calloc(1, sizeof(*f));
    mc_mob_tick_error *e = calloc(1, sizeof(*e));
    mc_entity_effect_error *cleanup = calloc(1, sizeof(*cleanup));
    mc_entity *actor = NULL;
    mc_math_classes *math = NULL;
    char directory[64];
    CHECK(f != NULL && e != NULL && cleanup != NULL &&
          snprintf(directory, sizeof(directory), "monster-hurt-%u", mode) > 0 &&
          setup(f, resources, directory, true) && hurt_actor(f, &actor) &&
          mc_math_classes_create(SIZE_MAX, &math, NULL) == MC_MATH_CLASS_OK &&
          mc_math_classes_ensure(math, NULL) == MC_MATH_CLASS_OK);
    const size_t holds = f->player->holds;
    const uint64_t actor_seed = actor->random->state;
    const int32_t amount = mode == 1 ? 7 : 5;
    if (mode == 1)
        actor->mob->monster_damage = amount; /* Actual mutable GZ.c operand. */
    if (mode == 2)
        f->player->local_player->fields.living.Y = 0;
    if (mode == 3) {
        f->player->base.by = 20;
        f->player->local_player->fields.living.au = 5;
    }
    hurt_sound sound = {0};
    if (mode == 4 || mode == 5) {
        if (mode == 4) {
            const mc_world_access_callbacks callbacks = {.sound = hurt_native_sound};
            CHECK(mc_world_access_create(&callbacks, &sound, &sound.receiver, NULL) ==
                  MC_WORLD_STORAGE_OK);
        } else {
            const mc_world_access_source_callbacks callbacks = {.sound = hurt_source_sound};
            CHECK(mc_world_access_source_create(&callbacks, &sound, &sound.receiver, NULL) ==
                  MC_WORLD_STORAGE_OK);
        }
        CHECK(mc_world_runtime_add_world_access(f->world, sound.receiver, NULL) ==
              MC_WORLD_STORAGE_OK);
    }
    mc_world_reference_list *players = f->world->fields.d;
    if (mode >= 6) {
        if (mode <= 7) {
            f->player->local_player->fields.player.u = true;
            f->world->fields.d = NULL; /* Reached real wake World.y List receiver. */
        } else
            f->player->holds = SIZE_MAX; /* Native runner admission only. */
    }
    const mc_entity_status status = mt_monster_component(actor, f->app.registry, math, NULL,
                                                         f->player, 1.f, mode == 7 ? NULL : e);
    if (mode == 8)
        f->player->holds = holds;
    CHECK(actor->mob->living.ae == (mode == 8 ? 0 : 20) && actor->random->state == actor_seed &&
          !actor->mob->tick_active && actor->holds == 1);
    if (mode <= 3) {
        const bool result = mode <= 1;
        CHECK(status == MC_ENTITY_OK && hurt_caller(e, amount, true) &&
              e->state.attack.hurt_result == result && !e->state.attack.hurt_prior_pending &&
              !mc_entity_effect_has_pending(f->player) && f->player->holds == holds);
        CHECK(e->state.attack.hurt_health_stored == result &&
              f->player->local_player->fields.living.Y == (mode == 2 ? 0
                                                           : result  ? 20 - amount
                                                                     : 20));
        if (result)
            CHECK(e->state.attack.hurt_health_before == 20 &&
                  e->state.attack.hurt_health_after == 20 - amount);
        int32_t value = -1;
        CHECK(hurt_value(f, &value) && value == (mode == 2 ? 0 : amount) &&
              e->state.attack.hurt_statistic_returned == (mode != 2) &&
              e->state.attack.hurt_statistic_amount == (mode == 2 ? 0 : amount) &&
              mc_mob_tick_error_retire(e));
    } else if (mode <= 5) {
        CHECK(status != MC_ENTITY_OK && hurt_caller(e, amount, false) &&
              !e->state.attack.hurt_result && !e->state.attack.hurt_prior_pending &&
              e->state.attack.hurt_health_stored && e->state.attack.hurt_health_before == 20 &&
              e->state.attack.hurt_health_after == 15 && e->state.attack.hurt_statistic_returned &&
              e->state.attack.hurt_statistic_amount == 5 &&
              f->player->local_player->fields.living.Y == 15 && sound.calls == 1 &&
              e->native_policy == (mode == 4) && e->source_exception == (mode == 5));
        if (mode == 5)
            CHECK(strcmp(e->exception_class, "java/lang/IllegalStateException") == 0 &&
                  e->detail.effect.world.cause.system_error == 61);
        CHECK(mc_mob_tick_error_retire(e) && !mc_entity_effect_has_pending(f->player) &&
              f->player->holds == holds && sound.calls == 1);
    } else if (mode <= 7) {
        CHECK(status != MC_ENTITY_OK && mc_entity_effect_has_pending(f->player) &&
              f->player->local_player->fields.living.Y == 20 && f->player->holds > holds);
        struct mc_player_damage_wake_work *pending = f->player->local_player->pending_damage_wake;
        mc_entity_player_wake_state original;
        CHECK(pending != NULL && mc_player_damage_wake_inspect(f->player, &original) &&
              original.source_exception && !original.native_policy && !original.returned);
        if (mode == 6) {
            CHECK(hurt_caller(e, amount, false) && !e->state.attack.hurt_prior_pending &&
                  e->source_exception && !e->native_policy && e->detail.effect.player_wake.pending);
            const mc_entity_status first = e->status;
            const char *operation = e->operation;
            const size_t held = f->player->holds;
            f->player->holds = SIZE_MAX; /* Native cleanup admission fault. */
            const bool retired = mc_mob_tick_error_retire(e);
            f->player->holds = held;
            CHECK(!retired && e->status == first && e->operation == operation &&
                  e->source_exception && e->detail.effect.player_wake.pending &&
                  e->detail.effect.player_wake.cleanup_status == MC_ENTITY_LIMIT &&
                  f->player->local_player->pending_damage_wake == pending);
            CHECK(mc_mob_tick_error_retire(e) && e->status == first && e->operation == operation &&
                  e->source_exception && !e->detail.effect.player_wake.pending);
        } else {
            actor->mob->living.ae = 0; /* New declared source attack attempt. */
            CHECK(mt_monster_component(actor, f->app.registry, math, NULL, f->player, 1.f, e) ==
                      MC_ENTITY_BUSY &&
                  hurt_caller(e, amount, false) && e->state.attack.hurt_prior_pending &&
                  e->native_policy && !e->source_exception && e->exception_class == NULL &&
                  actor->mob->living.ae == 20 && e->detail.effect.player_wake.pending &&
                  e->detail.effect.player_wake.error == original.error);
            CHECK(mc_mob_tick_error_retire(e) && !e->detail.effect.player_wake.pending &&
                  f->player->local_player->pending_damage_wake == pending &&
                  mc_player_damage_wake_inspect(f->player, &original) && original.source_exception);
            CHECK(mc_entity_effect_cleanup(f->player, cleanup) == MC_ENTITY_OK);
        }
        CHECK(!mc_entity_effect_has_pending(f->player) && f->player->holds == holds &&
              f->world->fields.d == NULL && f->player->local_player->fields.living.Y == 20);
        /* Source cleanup already completed with NULL still installed. Restore
         * only this fixture's source List input for final graph disposal. */
        f->world->fields.d = players;
    } else {
        CHECK(status == MC_ENTITY_LIMIT && e->native_policy && !e->source_exception &&
              !e->state.returned && !e->state.attack.hurt_invoked &&
              !e->state.attack.hurt_returned && !mc_entity_effect_has_pending(f->player) &&
              f->player->local_player->fields.living.Y == 20 && mc_mob_tick_error_retire(e));
    }
    if (sound.receiver != NULL) {
        CHECK(mc_world_access_receipt_retire(&sound.receipt, NULL) == MC_WORLD_STORAGE_OK);
        bool removed = false;
        CHECK(mc_world_runtime_remove_world_access(f->world, sound.receiver, &removed, NULL) ==
                  MC_WORLD_STORAGE_OK &&
              removed && mc_world_access_free(sound.receiver, NULL) == MC_WORLD_STORAGE_OK);
    }
    CHECK(mc_entity_effect_cleanup(f->player, NULL) == MC_ENTITY_OK &&
          mc_mob_retire(&actor, NULL) == MC_MOB_OK &&
          mc_math_classes_release(&math, NULL) == MC_MATH_CLASS_OK && teardown(f));
    free(cleanup);
    free(e);
    free(f);
    return true;
}

/* These are ordinary stored Chunk inputs, not constructed runtime stand-ins.
 * The actual region writer, Source loader, activation, publication and World
 * update remain in the measured native path. No NPC/Player is saved here. */
static bool hurt_world_chunks(fixture *f) {
    static const char *const names[] = {
        "xPos",       "zPos",      "LastUpdate",       "Blocks",   "Data",        "SkyLight",
        "BlockLight", "HeightMap", "TerrainPopulated", "Entities", "TileEntities"};
    static const mc_nbt_type types[] = {MC_NBT_INT,        MC_NBT_INT,        MC_NBT_LONG,
                                        MC_NBT_BYTE_ARRAY, MC_NBT_BYTE_ARRAY, MC_NBT_BYTE_ARRAY,
                                        MC_NBT_BYTE_ARRAY, MC_NBT_BYTE_ARRAY, MC_NBT_BYTE,
                                        MC_NBT_LIST,       MC_NBT_LIST};
    const size_t lengths[] = {MC_CHUNK_BLOCKS, MC_CHUNK_NIBBLE_BYTES, MC_CHUNK_NIBBLE_BYTES,
                              MC_CHUNK_NIBBLE_BYTES, MC_CHUNK_HEIGHTMAP_BYTES};
    uint8_t *arrays =
        calloc(1, MC_CHUNK_BLOCKS + 3u * MC_CHUNK_NIBBLE_BYTES + MC_CHUNK_HEIGHTMAP_BYTES);
    CHECK(arrays != NULL);
    mc_nbt_tag root = {.type = MC_NBT_COMPOUND}, level = {.type = MC_NBT_COMPOUND};
    mc_nbt_tag fields[11] = {0};
    const mc_nbt_tag *root_fields[] = {&level}, *level_fields[11];
    uint16_t encoded_names[12][20] = {0};
    const char *level_name = "Level";
    for (size_t i = 0; i < strlen(level_name); ++i)
        encoded_names[11][i] = (uint16_t)(unsigned char)level_name[i];
    level.name = (mc_nbt_string){encoded_names[11], strlen(level_name)};
    root.value.compound.items = root_fields;
    root.value.compound.length = 1;
    level.value.compound.items = level_fields;
    level.value.compound.length = 11;
    for (size_t i = 0; i < 11; ++i) {
        fields[i].type = types[i];
        for (size_t j = 0; j < strlen(names[i]); ++j)
            encoded_names[i][j] = (uint16_t)(unsigned char)names[i][j];
        fields[i].name = (mc_nbt_string){encoded_names[i], strlen(names[i])};
        level_fields[i] = &fields[i];
    }
    size_t offset = 0;
    for (size_t i = 0; i < 5; ++i) {
        fields[i + 3u].value.byte_array.data = arrays + offset;
        fields[i + 3u].value.byte_array.length = lengths[i];
        offset += lengths[i];
    }
    for (size_t i = 0; i < 256; ++i)
        arrays[i * 128u + 63u] = 1; /* Declared stone plane at y63. */
    memset(arrays + offset - MC_CHUNK_HEIGHTMAP_BYTES, 64, MC_CHUNK_HEIGHTMAP_BYTES);
    fields[8].value.byte_value = 1;
    fields[9].value.list.element_type = MC_NBT_COMPOUND;
    fields[10].value.list.element_type = MC_NBT_COMPOUND;
    size_t length = 0;
    CHECK(mc_nbt_measure(&root, NULL, &length, NULL) == MC_NBT_OK && length != 0);
    uint8_t *wire = malloc(length);
    CHECK(wire != NULL);
    for (int32_t x = -2; x <= 2; ++x)
        for (int32_t z = -2; z <= 2; ++z) {
            fields[0].value.int_value = x;
            fields[1].value.int_value = z;
            size_t written = 0;
            uint32_t growth = 0;
            CHECK(mc_nbt_write(&root, NULL, wire, length, &written, NULL) == MC_NBT_OK &&
                  written == length &&
                  mc_world_regions_write(f->regions, mc_world_store_root(f->store), x, z, wire,
                                         length, 73, &growth, NULL) == MC_WORLD_STORAGE_OK);
        }
    free(wire);
    free(arrays);
    for (int32_t x = -2; x <= 2; ++x)
        for (int32_t z = -2; z <= 2; ++z) {
            mc_chunk_state *chunk = NULL;
            CHECK(mc_world_runtime_get(f->world, x, z, &chunk) == MC_CHUNK_ACQUIRE_OK &&
                  chunk != NULL && !chunk->blank && chunk->active);
        }
    return true;
}

static bool hurt_world_observation(const mc_world_mob_hurt_observation *value, mc_entity *actor,
                                   mc_entity *target, bool returned) {
    const mc_world_mob_hurt_receipt *hit = &value->last;
    CHECK(value->reached_calls == 1 && value->returned_calls == (returned ? 1u : 0u) &&
          value->true_returns == (returned ? 1u : 0u) && !value->saturated && hit->present &&
          hit->returned == returned && hit->result == returned &&
          hit->parent_returned == returned && !hit->prior_pending &&
          hit->actor_identity == (uintptr_t)actor && hit->target_identity == (uintptr_t)target &&
          hit->actor_kind == MC_ENTITY_CLASS_KIND_ZOMBIE && hit->damage == 5 &&
          hit->caller_class != NULL && strcmp(hit->caller_class, "gz") == 0 &&
          strcmp(hit->caller_method, "a") == 0 && strcmp(hit->caller_descriptor, "(Lsn;F)V") == 0 &&
          hit->caller_pc == 61);
    if (returned)
        CHECK(hit->return_pc == 65 && hit->health_stored && hit->health_before == 20 &&
              hit->health_after == 15 && hit->statistic_returned && hit->statistic_amount == 5);
    else
        CHECK(!hit->health_stored && !hit->statistic_returned);
    return true;
}

static bool hurt_world(const char *resources, bool fail_wake) {
    fixture *f = calloc(1, sizeof(*f));
    mc_world_entity_tick_error *e = calloc(1, sizeof(*e));
    mc_entity *actor = NULL;
    mc_math_classes *math = NULL;
    mc_chunk_provider_fields provider = {0};
    CHECK(
        f != NULL && e != NULL &&
        setup(f, resources, fail_wake ? "monster-world-failure" : "monster-world-success", true) &&
        hurt_world_chunks(f) && hurt_actor(f, &actor) &&
        mc_math_classes_create(SIZE_MAX, &math, NULL) == MC_MATH_CLASS_OK &&
        mc_math_classes_ensure(math, NULL) == MC_MATH_CLASS_OK &&
        mc_chunk_provider_bind_fields(f->world->fields.v, &provider, NULL) == MC_WORLD_STORAGE_OK);
    /* Actual saved terrain and canonical owners, with component-selected pose,
     * nighttime and an owning target field. No enclosing behavior is skipped. */
    CHECK(mc_entity_set_position(actor, 9.5, 64, 8.5) == MC_ENTITY_OK &&
          mc_entity_set_position(f->player, 8.5, 64 + (double)1.62f, 8.5) == MC_ENTITY_OK &&
          mc_world_info_time_set(f->world->fields.x, 13000) == MC_WORLD_INFO_OK &&
          mc_mob_creature_target_store(actor, f->player, NULL) == MC_ENTITY_OK);
    f->world->fields.f = 4; /* Declared current night skylight subtraction. */
    bool inserted = false;
    CHECK(mc_world_runtime_insert_entity(f->world, f->player, &inserted, NULL) ==
              MC_WORLD_STORAGE_OK &&
          inserted &&
          mc_world_runtime_insert_entity(f->world, actor, &inserted, NULL) == MC_WORLD_STORAGE_OK &&
          inserted && actor->base.bF);
    mc_world_reference_list *players = f->world->fields.d;
    const size_t player_holds = f->player->holds;
    if (fail_wake) {
        f->player->local_player->fields.player.u = true;
        f->world->fields.d = NULL; /* Genuine wake fd.y List invocation fault. */
    }
    mc_world_entity_tick_context context = {0};
    context.player.travel.code_registry = f->app.registry;
    context.player.travel.trig = mc_math_classes_table(math);
    context.player.travel.math = math;
    context.math = math;
    const mc_world_storage_status status =
        mc_world_runtime_tick_entity(f->world, actor, &context, NULL, e);
    CHECK(hurt_world_observation(&e->mob_hurt, actor, f->player, !fail_wake));
    int32_t value = -1;
    CHECK(hurt_value(f, &value) && value == (fail_wake ? 0 : 5));
    if (!fail_wake) {
        /* Success has really reset the top diagnostic; only the value-only
         * receipt survives, independently of its former Mob/Effect union. */
        CHECK(status == MC_WORLD_STORAGE_OK && e->status == MC_WORLD_STORAGE_OK &&
              e->child == MC_WORLD_ENTITY_TICK_CHILD_NONE && e->operation == NULL &&
              e->source_class == NULL && e->bytecode == 0 &&
              f->player->local_player->fields.living.Y == 15 &&
              !mc_entity_effect_has_pending(f->player) && f->player->holds >= player_holds);
    } else {
        CHECK(status != MC_WORLD_STORAGE_OK && e->child == MC_WORLD_ENTITY_TICK_CHILD_MOB &&
              e->bytecode == 121 && e->child_error.mob.child == MC_MOB_TICK_CHILD_EFFECT &&
              hurt_caller(&e->child_error.mob, 5, false) && e->child_error.mob.source_exception &&
              !e->child_error.mob.native_policy &&
              e->child_error.mob.detail.effect.player_wake.pending &&
              f->player->local_player->fields.living.Y == 20 &&
              mc_entity_effect_has_pending(f->player) && f->player->holds > player_holds);
        const mc_world_storage_status first = e->status;
        const char *operation = e->operation;
        const mc_world_mob_hurt_observation observed = e->mob_hurt;
        struct mc_player_damage_wake_work *pending = f->player->local_player->pending_damage_wake;
        const size_t held = f->player->holds;
        f->player->holds = SIZE_MAX; /* Native cleanup admission control only. */
        const bool retired = mc_world_entity_tick_error_retire(e);
        f->player->holds = held;
        CHECK(!retired && e->status == first && e->operation == operation && e->bytecode == 121 &&
              e->child == MC_WORLD_ENTITY_TICK_CHILD_MOB && e->child_error.mob.source_exception &&
              e->child_error.mob.detail.effect.player_wake.cleanup_status == MC_ENTITY_LIMIT &&
              f->player->local_player->pending_damage_wake == pending &&
              e->mob_hurt.last.caller_class == observed.last.caller_class &&
              e->mob_hurt.last.caller_method == observed.last.caller_method &&
              e->mob_hurt.last.caller_descriptor == observed.last.caller_descriptor &&
              hurt_world_observation(&e->mob_hurt, actor, f->player, false));
        CHECK(mc_world_entity_tick_error_retire(e) && e->child == MC_WORLD_ENTITY_TICK_CHILD_NONE &&
              !mc_entity_effect_has_pending(f->player) && held >= 2 &&
              f->player->holds == held - 2 && f->world->fields.d == NULL &&
              f->player->local_player->fields.living.Y == 20);
        /* No source replay/repair was needed. Restore only for graph disposal. */
        f->world->fields.d = players;
    }
    CHECK(mc_world_entity_tick_error_retire(e));
    mc_chunk_state *retiring[64];
    const size_t count = provider.cache->insertion_count;
    CHECK(count <= sizeof(retiring) / sizeof(*retiring));
    if (count != 0)
        memcpy(retiring, provider.cache->insertions, count * sizeof(*retiring));
    /* The actual Chunk/World roots now protect the Zombie. End this caller's
     * allocation occurrence before asking the terminal transaction to consume
     * its target/path graph; an external Mob owner correctly prevents that
     * commit. Do not repair or clear any source reference field here. */
    CHECK(actor->holds > 1 && actor->mob->creature_target_hold == f->player);
    mc_entity_release(actor);
    actor = NULL;
    mc_world_storage_error terminal = {0};
    const mc_world_storage_status containers =
        mc_world_runtime_retire_entity_containers(f->world, &terminal);
    if (containers != MC_WORLD_STORAGE_OK)
        fprintf(stderr, "Monster World entity containers: status=%d operation=%s\n",
                (int)containers, terminal.operation == NULL ? "none" : terminal.operation);
    CHECK(containers == MC_WORLD_STORAGE_OK);
    CHECK(mc_chunk_pending_clear(provider.pending) == MC_CHUNK_ACQUIRE_OK);
    CHECK(mc_chunk_cache_init(provider.cache, provider.cache->entries, provider.cache->capacity,
                              provider.cache->insertions, provider.cache->insertion_capacity));
    const mc_world_storage_status world_retired =
        mc_world_runtime_retire_world(f->world, &terminal);
    if (world_retired != MC_WORLD_STORAGE_OK)
        fprintf(stderr, "Monster World retirement: status=%d operation=%s\n", (int)world_retired,
                terminal.operation == NULL ? "none" : terminal.operation);
    CHECK(world_retired == MC_WORLD_STORAGE_OK);
    for (size_t i = 0; i < count; ++i)
        CHECK(mc_world_store_retire_chunk(f->store, retiring[i], NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_math_classes_release(&math, NULL) == MC_MATH_CLASS_OK && teardown(f));
    free(e);
    free(f);
    return true;
}

int main(int argc, char **argv) {
    if (argc != 3 || CD(argv[2]) != 0 || !mc_math_trig_init(&trig))
        return 2;
    for (unsigned int mode = 0; mode < 9; ++mode)
        if (!hurt_one(argv[1], mode))
            return 1;
    if (!hurt_world(argv[1], false) || !hurt_world(argv[1], true))
        return 1;
    printf("Monster hurt: %zu checks;9 caller/producer and2 actual World controls\n", checks);
    return 0;
}
