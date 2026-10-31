/* Complete genuine App/Session/World/Player fixture; its main is not run. */
#define main mc_item_pickup_app_fixture_main
#include "../../client/statistics/test_player_dispatch.c"
#undef main
#include "../../../src/inventory/internal.h"
#include "../../../src/item/stack/source_internal.h"
#include "../../../src/world/runtime/runtime_internal.h"
#include "mc/entity/action/action.h"
#include "mc/entity/item/pickup.h"
#include "mc/entity/player/input.h"
#include "mc/item/registry/runtime.h"
#include "mc/world/runtime/world_access.h"
#include <inttypes.h>
#include <xmmintrin.h>

static bool reference_rows;
static mc_item_stack *allocation_source;
static bool allocation_called;

static mc_item_stack_status refuse_clone(mc_item_stack **out) {
    (void)out;
    allocation_called = true;
    return MC_ITEM_STACK_ALLOCATION_FAILED;
}

static mc_item_stack_status mutate_then_allocate(mc_item_stack **out) {
    allocation_called = true;
    mc_item_stack_status status = mc_item_stack_source_count_set(allocation_source, 13);
    if (status == MC_ITEM_STACK_OK)
        status = mc_item_stack_source_damage_set(allocation_source, 6);
    return status == MC_ITEM_STACK_OK ? mc_item_stack_allocate(out) : status;
}

static bool clone_controls(void) {
    for (int mode = 0; mode < 3; ++mode) {
        mc_item_stack *source = NULL, *guard = NULL;
        CHECK(mc_item_stack_allocate(&source) == MC_ITEM_STACK_OK &&
              mc_item_stack_allocate(&guard) == MC_ITEM_STACK_OK);
        if (mode == 1)
            CHECK(mc_item_stack_construct(source, 262, 2, 3) == MC_ITEM_STACK_OK);
        if (mode == 2)
            CHECK(mc_item_stack_construct_nbt(source, NULL, NULL) ==
                  MC_ITEM_STACK_INVALID_ARGUMENT);
        mc_item_stack_phase phase;
        CHECK(mc_item_stack_phase_get(source, &phase) == MC_ITEM_STACK_OK);
        CHECK(phase == (mode == 0   ? MC_ITEM_STACK_RAW
                        : mode == 1 ? MC_ITEM_STACK_READY
                                    : MC_ITEM_STACK_FAILED_CONSTRUCTION));
        mc_item_stack *copy = guard;
        mc_item_stack_source_site site = {0};
        allocation_called = false;
        CHECK(mc_item_stack_source_clone(source, refuse_clone, &copy, &site) ==
              MC_ITEM_STACK_ALLOCATION_FAILED);
        CHECK(allocation_called && copy == guard && site.selected && site.entered &&
              site.bytecode == 0);
        allocation_source = source;
        allocation_called = false;
        CHECK(mc_item_stack_source_clone(source, mutate_then_allocate, &copy, &site) ==
              MC_ITEM_STACK_OK);
        CHECK(allocation_called && copy != source && copy != guard && site.bytecode == 16);
        mc_item_stack_fields fields, original;
        mc_item_stack_phase after;
        CHECK(mc_item_stack_fields_get(copy, &fields) == MC_ITEM_STACK_OK && fields.count == 13 &&
              fields.damage == 6 && fields.animation == 0 && mc_item_stack_source(copy) == NULL);
        CHECK(mc_item_stack_fields_get(source, &original) == MC_ITEM_STACK_OK &&
              fields.id == original.id &&
              mc_item_stack_phase_get(source, &after) == MC_ITEM_STACK_OK && after == phase);
        mc_item_stack_release(copy);
        if (mode != 1) {
            copy = guard;
            CHECK(mc_item_stack_clone(source, &copy) == MC_ITEM_STACK_INVALID_PHASE &&
                  copy == guard);
        }
        mc_item_stack_release(guard);
        mc_item_stack_release(source);
    }
    allocation_source = NULL;
    return true;
}

static mc_item_stack *make_stack(int32_t id, int32_t count, int32_t damage) {
    mc_item_stack *s = NULL;
    if (mc_item_stack_allocate(&s) != MC_ITEM_STACK_OK)
        return NULL;
    if (mc_item_stack_construct(s, id, count, damage) != MC_ITEM_STACK_OK) {
        mc_item_stack_release(s);
        return NULL;
    }
    return s;
}

static bool clear_inventory(mc_inventory_player *inventory) {
    for (int32_t i = 0; i < 36; ++i)
        CHECK(mc_inventory_player_set(inventory, i, NULL, NULL) == MC_INVENTORY_OK);
    return true;
}

static bool put(mc_inventory_player *inventory, int32_t slot, int32_t id, int32_t count,
                int32_t damage) {
    mc_item_stack *s = make_stack(id, count, damage);
    CHECK(s != NULL);
    const mc_inventory_status status = mc_inventory_player_set(inventory, slot, s, NULL);
    mc_item_stack_release(s);
    CHECK(status == MC_INVENTORY_OK);
    return true;
}

static bool scene(mc_inventory_player *inventory, int n, mc_item_stack **out) {
    CHECK(clear_inventory(inventory));
    int32_t id = 262, count = 5, damage = 7;
    if (n == 1) {
        count = 10;
        damage = 9;
        CHECK(put(inventory, 0, 262, 60, 0));
    }
    if (n == 2 || n == 3 || n == 4 || n == 9) {
        for (int32_t i = 0; i < 36; ++i)
            CHECK(put(inventory, i, 262, i == 0 && n != 2 && n != 9 ? 63 : 64, 0));
        count = n == 3 ? 10 : n == 4 ? 1 : 5;
    }
    if (n == 5 || n == 6) {
        id = 35;
        count = 3;
        damage = n == 5 ? 2 : 1;
        CHECK(put(inventory, 0, 35, 2, 1));
    }
    if (n == 7 || n == 8 || n == 9 || n == 22) {
        id = 270;
        count = n == 22 ? 0 : 2;
        damage = n == 7 ? 0 : 1;
    }
    if (n == 10)
        count = 0;
    if (n == 11)
        count = -4;
    if (n == 12)
        count = INT32_MAX;
    if (n == 13)
        count = INT32_MIN;
    if (n == 14 || n == 15)
        count = 3;
    if (n == 16)
        CHECK(put(inventory, 0, 263, 1, 0));
    if (n == 17)
        id = -1;
    if (n == 18)
        id = 0;
    if (n == 19) {
        id = 35;
        count = 3;
        damage = -2;
        CHECK(put(inventory, 0, 35, 2, -2));
    }
    if (n == 20) {
        count = 70;
        CHECK(put(inventory, 0, 262, -2, 0));
    }
    if (n == 21)
        CHECK(put(inventory, 0, 262, INT32_MIN, 0));
    mc_item_stack *incoming = make_stack(id, count, damage);
    CHECK(incoming != NULL);
    if (n == 14 || n == 15) {
        CHECK(mc_inventory_player_set(inventory, 0, incoming, NULL) == MC_INVENTORY_OK);
        if (n == 15)
            CHECK(mc_inventory_player_set(inventory, 1, incoming, NULL) == MC_INVENTORY_OK);
    }
    *out = incoming;
    return true;
}

static bool row(mc_inventory_player *inventory, mc_item_stack *incoming, int n,
                const char *result) {
    if (!reference_rows)
        return true;
    mc_item_stack_fields f;
    CHECK(mc_item_stack_fields_get(incoming, &f) == MC_ITEM_STACK_OK);
    printf("INSERT %d %s %" PRId32 ":%" PRId32 ":%" PRId32 ":%" PRId32, n, result, f.id, f.count,
           f.damage, f.animation);
    mc_inventory_player_state state;
    CHECK(mc_inventory_player_read_state(inventory, &state));
    for (size_t i = 0; i < 36; ++i) {
        mc_item_stack *s = state.main_slots[i];
        if (s == NULL) {
            fputs(" -", stdout);
            continue;
        }
        CHECK(mc_item_stack_fields_get(s, &f) == MC_ITEM_STACK_OK);
        int alias = s == incoming ? -1 : (int)i;
        for (size_t j = 0; j < i && alias != -1; ++j)
            if (state.main_slots[j] == s) {
                alias = (int)j;
                break;
            }
        printf(" %d:%" PRId32 ":%" PRId32 ":%" PRId32 ":%" PRId32, alias, f.id, f.count, f.damage,
               f.animation);
    }
    printf(" %d\n", state.changed ? 1 : 0);
    return !ferror(stdout);
}

static bool insertions(fixture *f) {
    mc_inventory_player *inventory = f->player->local_player->fields.player.c;
    const size_t holds = f->app.registry->runtime_borrows;
    for (int n = 0; n < 23; ++n) {
        mc_item_stack *incoming = NULL;
        CHECK(scene(inventory, n, &incoming));
        bool added = true;
        mc_inventory_insert_error e;
        const mc_entity_status status =
            mc_inventory_player_insert(inventory, incoming, f->app.registry, NULL, &added, &e);
        const char *label;
        if (n == 17 || n == 18) {
            CHECK(status == MC_ENTITY_STATE_ERROR && added);
            CHECK(e.add_bytecode == 1 && e.stack_method != NULL &&
                  strcmp(e.stack_method, "e()Z") == 0);
            CHECK(e.stack_bytecode == (n == 17 ? 7u : 8u));
            label = n == 17 ? "INDEX" : "NULL";
        } else {
            CHECK(status == MC_ENTITY_OK);
            label = added ? "TRUE" : "FALSE";
        }
        CHECK(row(inventory, incoming, n, label));
        if (n == 3) {
            mc_item_stack_fields source, target;
            CHECK(!added && mc_item_stack_fields_get(incoming, &source) == MC_ITEM_STACK_OK &&
                  source.count == 9);
            CHECK(mc_item_stack_fields_get(inventory->main_slots[0], &target) == MC_ITEM_STACK_OK &&
                  target.count == 64);
        }
        CHECK(!inventory->active && f->app.registry->runtime_borrows == holds);
        mc_item_stack_release(incoming);
    }
    CHECK(clear_inventory(inventory));
    mc_item_stack *incoming = make_stack(262, 2, 0);
    CHECK(incoming != NULL);
    const mc_inventory_insert_limits none = {0};
    mc_inventory_insert_error e;
    bool added = true;
    CHECK(mc_inventory_player_insert(inventory, incoming, f->app.registry, &none, &added, &e) ==
          MC_ENTITY_LIMIT);
    CHECK(added && e.add_bytecode == 8 && e.loop_work == 0 && !inventory->active);
    mc_item_stack_fields fields;
    CHECK(mc_item_stack_fields_get(incoming, &fields) == MC_ITEM_STACK_OK && fields.count == 2);
    CHECK(inventory->main_slots[0] == NULL);
    mc_item_stack_release(incoming);
    return true;
}

typedef struct sound_observation {
    size_t calls;
    mc_entity *item;
    mc_inventory_player *inventory;
    int32_t count_at_sound, first_slot_count;
    bool dead_at_sound;
    float volume, pitch;
} sound_observation;

static mc_world_storage_status sound_sink(void *context, mc_world_runtime *world,
                                          const mc_registry_string *name, double x, double y,
                                          double z, float volume, float pitch,
                                          mc_world_storage_error *error) {
    (void)world;
    (void)name;
    (void)x;
    (void)y;
    (void)z;
    (void)error;
    sound_observation *s = context;
    mc_item_stack_fields fields;
    ++s->calls;
    s->volume = volume;
    s->pitch = pitch;
    s->dead_at_sound = s->item->base.be;
    if (mc_item_stack_fields_get(s->item->stack, &fields) != MC_ITEM_STACK_OK)
        return MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    s->count_at_sound = fields.count;
    if (mc_item_stack_fields_get(s->inventory->main_slots[0], &fields) != MC_ITEM_STACK_OK)
        return MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    s->first_slot_count = fields.count;
    return MC_WORLD_STORAGE_OK; /* Actual supported observation transport slot. */
}

static bool pickup_case(fixture *f, int n) {
    mc_inventory_player *inventory = f->player->local_player->fields.player.c;
    CHECK(clear_inventory(inventory));
    const mc_item *special_item = NULL;
    CHECK(mc_registry_item_static_get(f->app.registry, "aD", &special_item, NULL) ==
          MC_REGISTRY_OK);
    const int32_t id = n == 4 ? 17 : n == 5 ? mc_item_fields_get(special_item)->bf : 262;
    mc_item_stack *stack = make_stack(id, 5, 0);
    CHECK(stack != NULL);
    mc_entity *item = NULL;
    CHECK(mc_item_entity_allocate(f->app.entities, &item, NULL) == MC_ENTITY_OK);
    CHECK(mc_item_entity_construct(item, f->world, 8, 200, 8, stack, NULL) == MC_ENTITY_OK);
    CHECK(mc_entity_item_pickup_delay_set(item, n == 1 ? 3 : 0) == MC_ENTITY_OK);
    item->base.be = false;
    mc_java_random_reseed(item->random, 38);
    const uint64_t before = item->random->state;
    f->world->fields.B = n == 0;
    if (n == 2 || n == 3) {
        for (int32_t i = 0; i < 36; ++i)
            CHECK(put(inventory, i, 262, i == 0 && n == 3 ? 63 : 64, 0));
    }
    client_application *app = f->player->local_player->fields.minecraft;
    f->player->local_player->fields.minecraft = NULL;
    mc_item_pickup_error *e = malloc(sizeof(*e));
    CHECK(e != NULL);
    const mc_entity_status status =
        mc_item_entity_pickup(item, f->player, f->app.registry, NULL, e);
    f->player->local_player->fields.minecraft = app;
    f->world->fields.B = false;
    const bool completed = n <= 3;
    CHECK((status == MC_ENTITY_OK) == completed && !item->base.be);
    if (!completed) {
        if (n == 4 || n == 5)
            CHECK(e->statistic.status == MC_PLAYER_STATISTIC_NULL_REFERENCE);
        else
            CHECK(e->pickup_bytecode == 123 && e->notification_bytecode == 4 &&
                  item->random->state != before);
    }
    mc_item_stack_fields fields;
    CHECK(mc_item_stack_fields_get(stack, &fields) == MC_ITEM_STACK_OK);
    CHECK(fields.count == (n == 0 || n == 1 || n == 2 ? 5 : n == 3 ? 4 : 0));
    if (n <= 5)
        CHECK(item->random->state == before);
    if (reference_rows) {
        printf("PICKUP %d %d %" PRId32 " %d %012" PRIx64 "\n", n, completed ? 1 : 0, fields.count,
               item->base.be ? 1 : 0, item->random->state);
        CHECK(row(inventory, stack, 100 + n, completed ? "DONE" : "NULL"));
    }
    free(e);
    mc_entity_release(item);
    mc_item_stack_release(stack);
    return true;
}

static bool actual_parent_chain(mc_entity *player, const mc_statistic *statistic, size_t depth) {
    CHECK(depth < 16);
    const mc_achievement_fields *fields = mc_statistic_achievement_fields_get(statistic);
    CHECK(fields != NULL);
    if (fields->c != NULL)
        CHECK(actual_parent_chain(player, fields->c, depth + 1));
    CHECK(mc_local_player_add_statistic(player, statistic, 1, NULL) == MC_PLAYER_STATISTIC_OK);
    return true;
}

static bool achievement_pickups(fixture *f) {
    mc_registry_achievement_view view;
    CHECK(mc_registry_classes_inspect_achievements(f->app.registry, &view, NULL) == MC_REGISTRY_OK);
    const mc_item *special = NULL;
    CHECK(mc_registry_item_static_get(f->app.registry, "aD", &special, NULL) == MC_REGISTRY_OK);
    const size_t indices[] = {1, 14};
    const int32_t ids[] = {17, mc_item_fields_get(special)->bf};
    for (size_t n = 0; n < 2; ++n) {
        const mc_statistic *achievement = view.named[indices[n]];
        const mc_achievement_fields *fields = mc_statistic_achievement_fields_get(achievement);
        CHECK(fields != NULL && fields->c != NULL && actual_parent_chain(f->player, fields->c, 0));
        int32_t before, after;
        CHECK(mc_client_stats_writer_value(f->app.stats_writer, achievement, &before, NULL) ==
              MC_CLIENT_STATS_OK);
        mc_inventory_player *inventory = f->player->local_player->fields.player.c;
        CHECK(clear_inventory(inventory));
        mc_item_stack *stack = make_stack(ids[n], 2, 0);
        mc_entity *item = NULL;
        CHECK(stack != NULL &&
              mc_item_entity_allocate(f->app.entities, &item, NULL) == MC_ENTITY_OK &&
              mc_item_entity_construct(item, f->world, 8, 200, 8, stack, NULL) == MC_ENTITY_OK &&
              mc_entity_item_pickup_delay_set(item, 0) == MC_ENTITY_OK);
        mc_item_pickup_error *error = malloc(sizeof(*error));
        CHECK(error != NULL);
        CHECK(mc_item_entity_pickup(item, f->player, f->app.registry, NULL, error) ==
              MC_ENTITY_STATE_ERROR);
        CHECK(error->insertion_returned && error->inserted && error->pickup_bytecode == 123 &&
              error->notification_bytecode == 25 &&
              error->statistic.status == MC_PLAYER_STATISTIC_OK && !item->base.be);
        CHECK(mc_client_stats_writer_value(f->app.stats_writer, achievement, &after, NULL) ==
                  MC_CLIENT_STATS_OK &&
              after == mc_java_i32_add(before, 1));
        CHECK(mc_client_achievement_notice_fields_get(f->app.achievement_notice)->f == achievement);
        free(error);
        mc_entity_release(item);
        mc_item_stack_release(stack);
    }
    return true;
}

static bool nullable_prefixes(fixture *f) {
    mc_entity *item = NULL;
    CHECK(mc_item_entity_allocate(f->app.entities, &item, NULL) == MC_ENTITY_OK &&
          mc_item_entity_construct_world(item, f->world, NULL) == MC_ENTITY_OK);
    f->world->fields.B = true;
    CHECK(mc_item_entity_pickup(item, NULL, NULL, NULL, NULL) == MC_ENTITY_OK);
    f->world->fields.B = false;
    mc_item_pickup_error *error = malloc(sizeof(*error));
    CHECK(error != NULL);
    CHECK(mc_item_entity_pickup(item, NULL, NULL, NULL, error) == MC_ENTITY_STATE_ERROR &&
          error->pickup_bytecode == 15);
    mc_entity_release(item);
    item = NULL;
    CHECK(mc_item_entity_allocate(f->app.entities, &item, NULL) == MC_ENTITY_OK &&
          mc_item_entity_construct_world(item, NULL, NULL) == MC_ENTITY_OK);
    CHECK(mc_item_entity_pickup(item, NULL, NULL, NULL, error) == MC_ENTITY_STATE_ERROR &&
          error->pickup_bytecode == 4);
    mc_entity_release(item);
    item = NULL;
    mc_item_stack *stack = make_stack(262, 1, 0);
    CHECK(stack != NULL && mc_item_entity_allocate(f->app.entities, &item, NULL) == MC_ENTITY_OK &&
          mc_item_entity_construct(item, f->world, 8, 200, 8, stack, NULL) == MC_ENTITY_OK &&
          mc_entity_item_pickup_delay_set(item, 3) == MC_ENTITY_OK);
    CHECK(mc_item_entity_pickup(item, NULL, NULL, NULL, error) == MC_ENTITY_OK);
    mc_entity_release(item);
    mc_item_stack_release(stack);
    free(error);
    return true;
}

static bool connected(fixture *f) {
    for (int n = 0; n < 7; ++n)
        CHECK(pickup_case(f, n));
    mc_inventory_player *inventory = f->player->local_player->fields.player.c;
    CHECK(clear_inventory(inventory));
    mc_item_stack *stack = make_stack(262, 5, 0);
    CHECK(stack != NULL);
    mc_entity *item = NULL;
    CHECK(mc_item_entity_allocate(f->app.entities, &item, NULL) == MC_ENTITY_OK &&
          mc_item_entity_construct(item, f->world, 8, 200, 8, stack, NULL) == MC_ENTITY_OK &&
          mc_entity_item_pickup_delay_set(item, 0) == MC_ENTITY_OK);
    sound_observation observed = {.item = item, .inventory = inventory};
    mc_world_access *access = NULL;
    const mc_world_access_callbacks callbacks = {.sound = sound_sink};
    CHECK(mc_world_access_create(&callbacks, &observed, &access, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_add_world_access(f->world, access, NULL) == MC_WORLD_STORAGE_OK);
    mc_item_pickup_error *e = malloc(sizeof(*e));
    CHECK(e != NULL);
    mc_java_random expected = *item->random;
    const float first = mc_java_random_next_float(&expected),
                second = mc_java_random_next_float(&expected);
    volatile float difference = first - second, scaled = difference * .7f;
    volatile float shifted = scaled + 1.0f, expected_pitch = shifted * 2.0f;
    const int32_t next_id_before = f->app.entities->next_id;
    const size_t lives_before = f->app.entities->live_entities;
    CHECK(mc_item_entity_pickup(item, f->player, f->app.registry, NULL, e) ==
          MC_ENTITY_STATE_ERROR);
    CHECK(observed.calls == 1 && observed.count_at_sound == 0 && observed.first_slot_count == 5 &&
          !observed.dead_at_sound && observed.volume == .2f && observed.pitch == expected_pitch &&
          item->random->state == expected.state && !item->base.be);
    CHECK(e->pickup_bytecode == 123 && e->notification_bytecode == 25 &&
          e->notification.notification_pc == 25 &&
          e->notification.particle.status == MC_ENTITY_OK &&
          strstr(e->notification.operation, "null captured j") != NULL &&
          f->app.entities->next_id == mc_java_i32_add(next_id_before, 1) &&
          f->app.entities->live_entities == lives_before);
    CHECK(clear_inventory(inventory));
    CHECK(mc_item_stack_source_count_set(stack, 5) == MC_ITEM_STACK_OK);
    expected = *item->random;
    (void)mc_java_random_next_float(&expected);
    (void)mc_java_random_next_float(&expected);
    const unsigned int fp = _mm_getcsr();
    _mm_setcsr((fp & ~0x6000u) | 0x2000u);
    const mc_entity_status unsupported =
        mc_item_entity_pickup(item, f->player, f->app.registry, NULL, e);
    _mm_setcsr(fp);
    CHECK(unsupported == MC_ENTITY_UNSUPPORTED_FP && e->pickup_bytecode == 109 &&
          observed.calls == 1 && item->random->state == expected.state && !item->base.be);
    mc_item_stack_fields remainder;
    CHECK(mc_item_stack_fields_get(stack, &remainder) == MC_ITEM_STACK_OK && remainder.count == 0);
    bool removed = false;
    CHECK(mc_world_runtime_remove_world_access(f->world, access, &removed, NULL) ==
              MC_WORLD_STORAGE_OK &&
          removed);
    CHECK(mc_world_access_free(access, NULL) == MC_WORLD_STORAGE_OK);
    free(e);
    mc_entity_release(item);
    mc_item_stack_release(stack);
    return true;
}

int main(int argc, char **argv) {
    if (argc != 4 || CD(argv[2]) != 0 || !mc_math_trig_init(&trig))
        return 2;
    reference_rows = strcmp(argv[3], "reference") == 0;
    fixture *f = calloc(1, sizeof(*f));
    if (f == NULL || !setup(f, argv[1], "pickup", true) || !clone_controls() || !insertions(f) ||
        !connected(f) || !achievement_pickups(f) || !nullable_prefixes(f) || !teardown(f))
        return 1;
    free(f);
    printf("Item pickup: %zu checks\n", checks);
    return ferror(stdout) ? 1 : 0;
}
