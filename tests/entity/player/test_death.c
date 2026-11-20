/* Genuine Player/App/World/Inventory owners; the imported observation main is not run. */
#define main mc_death_unused_statistics_fixture_main
#include "../../client/statistics/test_player_dispatch.c"
#undef main

#include "entity/player/container_interaction.h"
#include "inventory/internal.h"
#include "inventory/player/source_internal.h"
#include "mc/entity/movement/effects.h"
#include "mc/entity/movement/movement.h"
#include "mc/entity/player/drop.h"
#include "mc/world/runtime/world_access.h"
#include "world/runtime/runtime_internal.h"
#include "world/storage/generated_internal.h"
#include "world/storage/objects_internal.h"

typedef struct death_observer {
    fixture *owner;
    size_t calls, refuse_at, sounds;
    bool valid, reentry_busy;
    int32_t ids[8], counts[8], damage[8];
    double x[8], y[8], z[8];
    mc_entity_effect_error *reentry;
} death_observer;

static mc_world_storage_status cactus_sound(void *context, mc_world_runtime *world,
                                            const mc_registry_string *name, double x, double y,
                                            double z, float volume, float pitch,
                                            mc_world_storage_error *error) {
    death_observer *o = context;
    (void)x;
    (void)y;
    (void)z;
    (void)error;
    static const char expected[] = "random.hurt";
    o->valid = o->valid && world == o->owner->world && name != NULL && name->length == 11 &&
               volume == 1.0f && pitch > 0;
    if (name != NULL && name->length == 11)
        for (size_t i = 0; i < 11; ++i)
            o->valid = o->valid && name->units[i] == (uint16_t)expected[i];
    ++o->sounds;
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status death_added(void *context, mc_world_runtime *world, mc_entity *item,
                                           mc_world_storage_error *error) {
    death_observer *o = context;
    const size_t index = o->calls++;
    mc_item_stack_fields fields = {0};
    mc_player_death_state pending;
    o->valid = o->valid && index < 8 && world == o->owner->world && item->item.c == 40 &&
               mc_item_stack_fields_get(item->stack, &fields) == MC_ITEM_STACK_OK &&
               mc_player_death_inspect(o->owner->player, &pending) && pending.active &&
               pending.pending;
    if (index < 8 && o->valid) {
        o->ids[index] = fields.id;
        o->counts[index] = fields.count;
        o->damage[index] = fields.damage;
        o->x[index] = item->base.velocity_x;
        o->y[index] = item->base.velocity_y;
        o->z[index] = item->base.velocity_z;
    }
    o->reentry_busy = mc_entity_effect_cleanup(o->owner->player, o->reentry) == MC_ENTITY_BUSY &&
                      o->reentry->player_death.pending;
    if (o->refuse_at != 0 && o->calls == o->refuse_at) {
        *error = (mc_world_storage_error){MC_WORLD_STORAGE_IO_ERROR,
                                          "Authored Player death publication refusal", 0};
        return error->status;
    }
    return MC_WORLD_STORAGE_OK;
}

static bool stat(fixture *f, size_t index, int32_t expected) {
    mc_registry_statistic_view view;
    int32_t actual = -1;
    CHECK(mc_registry_classes_inspect_statistics(f->app.registry, &view, NULL) == MC_REGISTRY_OK &&
          mc_client_stats_writer_value(f->app.stats_writer, view.named[index], &actual, NULL) ==
              MC_CLIENT_STATS_OK &&
          actual == expected);
    return true;
}

static bool stack_at(fixture *f, int32_t slot, mc_item_stack *stack) {
    CHECK(mc_inventory_player_source_set(f->player->local_player->fields.player.c, slot, stack,
                                         NULL) == MC_INVENTORY_OK);
    return true;
}

static bool make_stack(int32_t id, int32_t count, int32_t damage, mc_item_stack **out) {
    CHECK(mc_item_stack_allocate(out) == MC_ITEM_STACK_OK &&
          mc_item_stack_construct(*out, id, count, damage) == MC_ITEM_STACK_OK);
    return true;
}

static bool chunk_at(fixture *f) {
    uint8_t *blocks = calloc(MC_CHUNK_BLOCKS, 1);
    mc_world_chunk *chunk = NULL;
    CHECK(blocks != NULL &&
          mc_world_store_create_generated(f->store, f->world, 0, 0, 0, &blocks, MC_CHUNK_BLOCKS,
                                          &chunk, NULL) == MC_WORLD_STORAGE_OK &&
          blocks == NULL);
    const mc_chunk_acquire_context *acquisition = mc_world_runtime_acquisition(f->world);
    CHECK(acquisition != NULL &&
          mc_chunk_cache_publish(acquisition->cache, 0, 0, mc_world_chunk_state(chunk)));
    CHECK(mc_entity_set_position(f->player, .5, 64, .5) == MC_ENTITY_OK);
    return true;
}

static bool retire_chunks(fixture *f) {
    CHECK(!mc_entity_effect_has_pending(f->player));
    const mc_chunk_acquire_context *a = mc_world_runtime_acquisition(f->world);
    CHECK(mc_world_runtime_retire_entity_containers(f->world, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(a != NULL && mc_chunk_pending_clear(a->pending) == MC_CHUNK_ACQUIRE_OK &&
          mc_chunk_cache_init(a->cache, a->cache->entries, a->cache->capacity, a->cache->insertions,
                              a->cache->insertion_capacity));
    while (mc_world_store_chunk_count(f->store) != 0) {
        mc_chunk_state *chunk = NULL;
        CHECK(mc_world_store_chunk_at(f->store, 0, &chunk, NULL) == MC_WORLD_STORAGE_OK &&
              mc_world_store_retire_chunk(f->store, chunk, NULL) == MC_WORLD_STORAGE_OK);
    }
    return true;
}

static bool empty_and_callbacks(fixture *f) {
    mc_entity *p = f->player;
    mc_entity_effect_error *e = malloc(sizeof(*e));
    CHECK(e != NULL);
    const size_t holds = p->holds;
    const uint64_t random = p->random->state;
    CHECK(mc_entity_effect_death_with_math(p, f->app.registry, NULL, NULL, e) == MC_ENTITY_OK);
    CHECK(e->player_death.returned && e->player_death.living_returned &&
          e->player_death.inventory_returned && !e->player_death.pending &&
          e->player_death.site.pc == 156 && p->local_player->fields.living.ah &&
          p->base.width == .2f && p->base.height == .2f && p->base.offset == .1f &&
          p->base.velocity_y == .10000000149011612 && p->base.velocity_x == 0 &&
          p->base.velocity_z == 0 && p->random->state == random && p->holds == holds &&
          stat(f, 19, 1) && stat(f, 16, 0));
    CHECK(mc_entity_effect_dead_with_context(p, NULL, NULL, e) == MC_ENTITY_OK &&
          e->player_death.closing && e->player_death.returned && e->player_death.close_calls == 2 &&
          e->player_death.closes_returned == 2 && p->base.be && p->holds == holds);
    free(e);
    return true;
}

static bool scatter(fixture *f, bool fail_second, bool notch) {
    CHECK(chunk_at(f));
    mc_entity *p = f->player;
    mc_inventory_player *inventory = p->local_player->fields.player.c;
    mc_item_stack *stack = NULL;
    CHECK(make_stack(3, 2, 7, &stack) && stack_at(f, 0, stack) && stack_at(f, 35, stack) &&
          stack_at(f, 39, stack));
    if (notch) {
        static const uint16_t units[] = {'N', 'o', 't', 'c', 'h'};
        mc_player_string *name = NULL;
        CHECK(mc_player_string_create(units, 5, &name) == MC_PLAYER_OK);
        mc_player_string_release((mc_player_string *)p->local_player->fields.player.l);
        p->local_player->fields.player.l = name;
    }
    mc_math_classes *math = NULL;
    mc_entity_effect_error *e = malloc(sizeof(*e)), *reentry = malloc(sizeof(*reentry));
    CHECK(e != NULL && reentry != NULL &&
          mc_math_classes_create(SIZE_MAX, &math, NULL) == MC_MATH_CLASS_OK);
    death_observer observer = {
        .owner = f, .valid = true, .refuse_at = fail_second ? 2 : 0, .reentry = reentry};
    mc_world_access *access = NULL;
    const mc_world_access_callbacks callbacks = {.entity_added = death_added};
    CHECK(mc_world_access_create(&callbacks, &observer, &access, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_add_world_access(f->world, access, NULL) == MC_WORLD_STORAGE_OK);
    mc_java_random oracle = *p->random;
    const size_t holds = p->holds;
    const mc_entity_status result =
        mc_entity_effect_death_with_math(p, f->app.registry, math, NULL, e);
    CHECK(result == (fail_second ? MC_ENTITY_WORLD_FAILED : MC_ENTITY_OK));
    CHECK(observer.valid && observer.reentry_busy &&
          observer.calls == (fail_second ? 2u
                             : notch     ? 4u
                                         : 3u));
    const mc_math_trig *table = mc_math_classes_table(math);
    CHECK(table != NULL);
    for (size_t i = 0; i < observer.calls; ++i) {
        const float radius = mc_java_random_next_float(&oracle) * .5f;
        const float angle = (mc_java_random_next_float(&oracle) * 3.1415927f) * 2.0f;
        CHECK(observer.x[i] == (double)(-mc_math_sin(table, angle) * radius) &&
              observer.z[i] == (double)(mc_math_cos(table, angle) * radius) &&
              observer.y[i] == .20000000298023224);
        CHECK(observer.ids[i] == (notch && i == 0 ? 260 : 3) &&
              observer.counts[i] == (notch && i == 0 ? 1 : 2) &&
              observer.damage[i] == (notch && i == 0 ? 0 : 7));
    }
    CHECK(p->random->state == oracle.state && !inventory->changed);
    if (fail_second) {
        CHECK(e->player_death.pending && !e->player_death.returned &&
              e->player_death.drops_returned == 1 && e->player_death.slots_cleared == 1 &&
              inventory->main_slots[0] == NULL && inventory->main_slots[35] == stack &&
              inventory->armor_slots[3] == stack && e->player_death.drop_error != NULL &&
              !inventory->active && stat(f, 16, 1) && stat(f, 19, 0));
        bool ignored = true;
        CHECK(mc_entity_effect_damage_with_math(p, f->app.registry, math, NULL, 1, &ignored,
                                                reentry) == MC_ENTITY_BUSY &&
              ignored);
        CHECK(mc_entity_effect_error_retire(e) && !mc_entity_effect_has_pending(p) &&
              inventory->main_slots[35] == stack && inventory->armor_slots[3] == stack &&
              p->holds == holds);
    } else {
        CHECK(e->player_death.returned && !e->player_death.pending &&
              e->player_death.slots_cleared == 3 && inventory->main_slots[0] == NULL &&
              inventory->main_slots[35] == NULL && inventory->armor_slots[3] == NULL &&
              p->holds == holds && stat(f, 16, notch ? 4 : 3) && stat(f, 19, 1));
    }
    bool removed = false;
    CHECK(mc_world_runtime_remove_world_access(f->world, access, &removed, NULL) ==
              MC_WORLD_STORAGE_OK &&
          removed && mc_world_access_free(access, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_math_classes_release(&math, NULL) == MC_MATH_CLASS_OK);
    mc_item_stack_release(stack);
    free(e);
    free(reentry);
    return retire_chunks(f);
}

static bool source_and_nulls(fixture *f) {
    mc_entity *p = f->player;
    mc_math_classes *math = NULL;
    mc_entity_effect_error *e = malloc(sizeof(*e));
    CHECK(e != NULL && mc_math_classes_create(SIZE_MAX, &math, NULL) == MC_MATH_CLASS_OK);
    p->local_player->fields.living.T = 7;
    p->local_player->fields.living.ac = 90;
    p->base.yaw = 30;
    const size_t holds = p->holds;
    CHECK(mc_entity_effect_death_with_math(p, f->app.registry, math, p, e) == MC_ENTITY_OK);
    const mc_math_trig *table = mc_math_classes_table(math);
    const float angle = ((90.0f + 30.0f) * 3.1415927f) / 180.0f;
    CHECK(table != NULL && e->player_death.score_returned && e->player_death.kill_returned &&
          p->local_player->fields.player.g == 7 && stat(f, 21, 1) && stat(f, 19, 1) &&
          p->base.velocity_x == (double)(-mc_math_cos(table, angle) * .1f) &&
          p->base.velocity_z == (double)(-mc_math_sin(table, angle) * .1f) && p->holds == holds);
    CHECK(mc_math_classes_release(&math, NULL) == MC_MATH_CLASS_OK);
    free(e);
    return true;
}

static bool retained_null(fixture *f) {
    mc_entity *p = f->player;
    mc_inventory_player *inventory = p->local_player->fields.player.c;
    mc_item_stack **array = inventory->main_slots;
    const size_t holds = p->holds;
    inventory->main_slots = NULL;
    /* NULL diagnostic cannot discard the pending producer or its self hold. */
    CHECK(mc_entity_effect_death_with_math(p, f->app.registry, NULL, NULL, NULL) ==
          MC_ENTITY_STATE_ERROR);
    inventory->main_slots = array;
    mc_player_death_state state;
    CHECK(mc_player_death_inspect(p, &state) && state.pending && state.source_exception &&
          !state.native_policy && state.inventory_pc == 7 && state.site.pc == 68 &&
          p->local_player->fields.living.ah && p->base.width == .2f && p->base.offset == 1.62f &&
          p->base.velocity_y == .10000000149011612 && p->holds == holds + 1);
    CHECK(mc_entity_effect_cleanup(p, NULL) == MC_ENTITY_OK && !mc_entity_effect_has_pending(p) &&
          p->holds == holds);
    return true;
}

static bool close_cursor(fixture *f, bool refuse) {
    CHECK(chunk_at(f));
    mc_inventory_player *inventory = f->player->local_player->fields.player.c;
    mc_item_stack *cursor = NULL;
    CHECK(make_stack(3, 2, 7, &cursor) && inventory->cursor == NULL);
    inventory->cursor = cursor; /* This owning Source field receives the allocation hold. */
    mc_math_classes *math = NULL;
    mc_entity_effect_error *e = malloc(sizeof(*e)), *reentry = malloc(sizeof(*reentry));
    CHECK(e != NULL && reentry != NULL &&
          mc_math_classes_create(SIZE_MAX, &math, NULL) == MC_MATH_CLASS_OK);
    death_observer observer = {
        .owner = f, .valid = true, .refuse_at = refuse ? 1 : 0, .reentry = reentry};
    const mc_world_access_callbacks callbacks = {.entity_added = death_added};
    mc_world_access *access = NULL;
    CHECK(mc_world_access_create(&callbacks, &observer, &access, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_add_world_access(f->world, access, NULL) == MC_WORLD_STORAGE_OK);
    const mc_entity_status status =
        mc_entity_effect_dead_with_context(f->player, f->app.registry, math, e);
    CHECK(status == (refuse ? MC_ENTITY_STATE_ERROR : MC_ENTITY_OK) && observer.valid &&
          observer.reentry_busy && observer.calls == 1 && f->player->base.be &&
          e->player_death.closing && e->player_death.close_calls == (refuse ? 1u : 2u) &&
          e->player_death.closes_returned == (refuse ? 0u : 2u) &&
          inventory->cursor == (refuse ? cursor : NULL) && stat(f, 16, refuse ? 0 : 1));
    if (refuse)
        CHECK(e->player_death.pending && e->player_death.close_error != NULL &&
              e->player_death.close_error->drop.status == MC_PLAYER_DROP_WORLD &&
              e->player_death.close_error->drop.publication != NULL &&
              mc_entity_effect_error_retire(e) && inventory->cursor == cursor &&
              !mc_entity_effect_has_pending(f->player));
    bool removed = false;
    CHECK(mc_world_runtime_remove_world_access(f->world, access, &removed, NULL) ==
              MC_WORLD_STORAGE_OK &&
          removed && mc_world_access_free(access, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_math_classes_release(&math, NULL) == MC_MATH_CLASS_OK);
    free(e);
    free(reentry);
    return retire_chunks(f);
}

static bool cactus_movement(fixture *f) {
    CHECK(chunk_at(f));
    mc_entity *p = f->player;
    mc_chunk_state *chunk = NULL;
    CHECK(mc_world_store_chunk_at(f->store, 0, &chunk, NULL) == MC_WORLD_STORAGE_OK &&
          chunk != NULL && chunk->data != NULL);
    /* Authored contact operand, reached through real zero-displacement
     * movement/collision/contact dispatch rather than a replacement callback. */
    chunk->data->blocks[63] = 81; /* Local x0/z0/y63. */
    p->local_player->fields.living.Y = 2;
    mc_item_stack *stack = NULL;
    CHECK(make_stack(3, 2, 7, &stack) && stack_at(f, 0, stack));
    mc_math_classes *math = NULL;
    mc_entity_move_error *move = malloc(sizeof(*move));
    mc_entity_effect_error *reentry = malloc(sizeof(*reentry));
    CHECK(move != NULL && reentry != NULL &&
          mc_math_classes_create(SIZE_MAX, &math, NULL) == MC_MATH_CLASS_OK);
    death_observer observer = {.owner = f, .valid = true, .reentry = reentry};
    const mc_world_access_callbacks callbacks = {.entity_added = death_added,
                                                 .sound = cactus_sound};
    mc_world_access *access = NULL;
    CHECK(mc_world_access_create(&callbacks, &observer, &access, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_add_world_access(f->world, access, NULL) == MC_WORLD_STORAGE_OK);
    const size_t holds = p->holds;
    CHECK(mc_entity_move_with_math(p, f->app.registry, math, 0, 0, 0, NULL, move) == MC_ENTITY_OK);
    mc_math_class_state before;
    CHECK(mc_math_classes_state(math, &before, NULL) == MC_MATH_CLASS_OK &&
          before.phase == MC_MATH_CLASS_COLD && p->local_player->fields.living.Y == 1 &&
          observer.sounds == 1 && observer.calls == 0 && observer.valid && stat(f, 18, 1));
    CHECK(mc_entity_move_with_math(p, f->app.registry, math, 0, 0, 0, NULL, move) == MC_ENTITY_OK);
    CHECK(p->local_player->fields.living.Y == 1 && observer.sounds == 1 && observer.calls == 0 &&
          observer.valid && stat(f, 18, 2));
    /* Separate authored immunity-boundary operand. No World/Player tick or
     * elapsed-time equivalence is inferred from these three method calls. */
    p->base.by = 0;
    CHECK(mc_entity_move_with_math(p, f->app.registry, math, 0, 0, 0, NULL, move) == MC_ENTITY_OK);
    mc_math_class_state after;
    CHECK(mc_math_classes_state(math, &after, NULL) == MC_MATH_CLASS_OK &&
          after.phase == MC_MATH_CLASS_READY && p->local_player->fields.living.Y == 0 &&
          p->local_player->fields.living.ah && p->base.offset == .1f && p->holds == holds &&
          observer.valid && observer.reentry_busy && observer.sounds == 2 && observer.calls == 1 &&
          observer.ids[0] == 3 && observer.counts[0] == 2 && observer.damage[0] == 7 &&
          p->local_player->fields.player.c->main_slots[0] == NULL &&
          !mc_entity_effect_has_pending(p) && stat(f, 18, 3) && stat(f, 16, 1) && stat(f, 19, 1));
    bool removed = false;
    CHECK(mc_world_runtime_remove_world_access(f->world, access, &removed, NULL) ==
              MC_WORLD_STORAGE_OK &&
          removed && mc_world_access_free(access, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_math_classes_release(&math, NULL) == MC_MATH_CLASS_OK);
    mc_item_stack_release(stack);
    free(move);
    free(reentry);
    return retire_chunks(f);
}

int main(int argc, char **argv) {
    if (argc != 3 || CD(argv[2]) != 0 || !mc_math_trig_init(&trig))
        return 2;
    fixture *f = calloc(1, sizeof(*f));
    if (f == NULL || !setup(f, argv[1], "empty", true) || !empty_and_callbacks(f) || !teardown(f) ||
        !setup(f, argv[1], "scatter", true) || !scatter(f, false, false) || !teardown(f) ||
        !setup(f, argv[1], "failed-scatter", true) || !scatter(f, true, false) || !teardown(f) ||
        !setup(f, argv[1], "notch", true) || !scatter(f, false, true) || !teardown(f) ||
        !setup(f, argv[1], "source", true) || !source_and_nulls(f) || !teardown(f) ||
        !setup(f, argv[1], "null-array", true) || !retained_null(f) || !teardown(f) ||
        !setup(f, argv[1], "cursor-close", true) || !close_cursor(f, false) || !teardown(f) ||
        !setup(f, argv[1], "failed-close", true) || !close_cursor(f, true) || !teardown(f) ||
        !setup(f, argv[1], "cactus", true) || !cactus_movement(f) || !teardown(f))
        return 1;
    free(f);
    printf("Player death/scatter: %zu checks\n", checks);
    return 0;
}
