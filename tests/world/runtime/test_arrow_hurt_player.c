/* Real Player/App/Inventory/StatsWriter owners; imported main never runs. */
#define MC_PLAYER_STATISTICS_MAX_PROVIDER_CHUNKS 32
int arrow_player_unused_drop_main(int, char **);
#define MC_PLAYER_DROP_EXECUTION_MAIN arrow_player_unused_drop_main
#include "../../entity/player/test_drop_execution.c"
#undef MC_PLAYER_DROP_EXECUTION_MAIN

#include "entity/arrow/tick_internal.h"
#include "entity/living/fields_internal.h"
#include "entity/player/death_execution_internal.h"
#include "entity/player/sleep_execution_internal.h"
#include "inventory/player/source_internal.h"
#include "mc/entity/player/sleep.h"
#include "mc/world/chunk/nibbles.h"
#include "mc/world/runtime/placement.h"
#include "mc/world/runtime/players.h"
#include "world/runtime/mutation/drops/execution_internal.h"

typedef struct ap_fixture {
    pd_fixture player;
    mc_entity *arrow, *item;
    mc_arrow_tick_call *call;
    mc_world_access *observer;
    mc_world_access_receipt *receipt;
    mc_item_stack *stack;
    const char *mode;
    uint64_t occurrence;
    size_t sounds, impacts, additions, cells;
    bool valid, removed, wake_before_sound;
} ap_fixture;

static bool ap_is(const ap_fixture *s, const char *mode) {
    return strcmp(s->mode, mode) == 0;
}

static bool ap_ladder(const ap_fixture *s) {
    return ap_is(s, "wake-bed-ladder") || ap_is(s, "wake-bed-ladder-source") ||
           ap_is(s, "wake-bed-ladder-removal-source");
}

static bool ap_bed(const ap_fixture *s) {
    return ap_is(s, "wake-bed-foot") || ap_is(s, "wake-bed-foot-source") || ap_ladder(s);
}

static double ap_arrow_x(const ap_fixture *s) {
    return ap_ladder(s) ? 9.4 : ap_bed(s) ? 7.6 : 8.5;
}

static double ap_arrow_velocity(const ap_fixture *s) {
    return ap_ladder(s) ? -(double).6f : (double).6f;
}

static size_t ap_index(int32_t x, int32_t y, int32_t z) {
    return (size_t)x * 2048u + (size_t)z * 128u + (size_t)y;
}

static bool ap_cell(mc_chunk_data *data, int32_t x, int32_t y, int32_t z, uint8_t id,
                    uint8_t metadata) {
    data->blocks[ap_index(x, y, z)] = id;
    return mc_chunk_nibbles_set(data->metadata, MC_CHUNK_NIBBLE_BYTES, x, y, z, metadata);
}

/* Prepared normal Chunks, followed by the real sleep entry. The Arrow ray
 * later crosses the actual sleeping bounds above the Bed collision box. */
static bool ap_enter_bed(ap_fixture *s) {
    fixture *f = &s->player.game;
    const mc_chunk_acquire_context *a = mc_world_runtime_acquisition(f->world);
    for (int32_t x = -2; x <= 1; ++x)
        for (int32_t z = -2; z <= 1; ++z) {
            if (x == 0 && z == 0)
                continue;
            uint8_t *blocks = calloc(MC_CHUNK_BLOCKS, 1);
            mc_world_chunk *chunk = NULL;
            CHECK(blocks != NULL &&
                  mc_world_store_create_generated(f->store, f->world, 0, x, z, &blocks,
                                                  MC_CHUNK_BLOCKS, &chunk,
                                                  NULL) == MC_WORLD_STORAGE_OK &&
                  blocks == NULL &&
                  mc_chunk_cache_publish(a->cache, x, z, mc_world_chunk_state(chunk)));
        }
    mc_chunk_data *data = mc_world_chunk_state(s->player.chunk)->data;
    memset(data->heightmap, 64, MC_CHUNK_HEIGHTMAP_BYTES);
    for (int32_t x = 0; x < 16; ++x)
        for (int32_t z = 0; z < 16; ++z) {
            CHECK(ap_cell(data, x, 63, z, 2, 0));
            for (int32_t y = 64; y < 128; ++y)
                CHECK(mc_chunk_nibbles_set(data->skylight, MC_CHUNK_NIBBLE_BYTES, x, y, z, 15));
        }
    CHECK(ap_cell(data, 8, 64, 8, 26, 8) && ap_cell(data, 8, 64, 7, 26, 0) &&
          mc_entity_set_position(f->player, 8.5, 64.0 + (double)1.62f, 8.5) == MC_ENTITY_OK);
    f->world->fields.f = 5;
    const mc_player_sleep_context context = {.code_registry = f->app.registry};
    mc_player_sleep_call *call = NULL;
    mc_player_sleep_error error = {0};
    mc_player_sleep_state state = {0};
    const mc_player_sleep_status status =
        mc_player_try_sleep_begin(f->player, 8, 64, 8, &context, &call, &error);
    if (status != MC_PLAYER_SLEEP_OK)
        fprintf(stderr, "Arrow Bed preparation:%d pc%u %s\n", (int)status, error.site.pc,
                error.operation == NULL ? "none" : error.operation);
    CHECK(status == MC_PLAYER_SLEEP_OK && mc_player_sleep_inspect(call, &state) && state.returned &&
          f->player->local_player->fields.player.u && f->world->fields.J &&
          f->player->base.width == .2f && f->player->base.height == .2f &&
          mc_player_sleep_retire(&call, NULL) == MC_PLAYER_SLEEP_OK &&
          ap_cell(data, 8, 64, 8, 26, 12) &&
          ap_cell(data, 7, 64, 8, ap_ladder(s) ? 65 : 26, ap_ladder(s) ? 2 : 0));
    f->player->local_player->fields.player.private_a = 37;
    return true;
}

static mc_world_storage_status ap_bed_cell(void *context, mc_world_runtime *world, int32_t x,
                                           int32_t y, int32_t z, mc_world_storage_error *cause,
                                           mc_world_access_failure *failure) {
    (void)failure;
    ap_fixture *s = context;
    fixture *f = &s->player.game;
    mc_chunk_data *data = mc_world_chunk_state(s->player.chunk)->data;
    uint8_t metadata = UINT8_MAX;
    s->valid &=
        ap_bed(s) && world == f->world && s->cells < 2 && x == (s->cells == 0 ? 8 : 7) && y == 64 &&
        z == 8 && f->player->local_player->fields.player.u &&
        f->player->local_player->fields.player.private_a == 37 &&
        f->player->local_player->fields.living.Y == 20 &&
        mc_chunk_nibbles_get(data->metadata, MC_CHUNK_NIBBLE_BYTES, 8, 64, 8, &metadata) &&
        metadata == 8 &&
        data->blocks[ap_index(7, 64, 8)] == (s->cells == 0 ? (ap_ladder(s) ? 65 : 26) : 0) &&
        s->additions == (ap_ladder(s) && s->cells == 1 ? 1u : 0u);
    ++s->cells;
    *cause = (mc_world_storage_error){0};
    if (s->valid && s->cells == 2 && ap_is(s, "wake-bed-ladder-removal-source")) {
        const mc_world_storage_status status =
            mc_world_access_receipt_create(s->observer, NULL, &s->receipt, failure, cause);
        if (status != MC_WORLD_STORAGE_OK)
            return status;
        s->receipt->exception_class = "java/lang/IllegalStateException";
        *cause =
            (mc_world_storage_error){MC_WORLD_STORAGE_IO_ERROR, "Arrow Ladder cell observer", 37};
        return cause->status;
    }
    return s->valid ? MC_WORLD_STORAGE_OK : MC_WORLD_STORAGE_UNSUPPORTED_STATE;
}

static bool ap_name(const mc_registry_string *name, const char *expected) {
    mc_registry_string_view text = {0};
    if (!mc_registry_string_read(name, &text) || text.length != strlen(expected))
        return false;
    for (size_t i = 0; i < text.length; ++i)
        if (text.units[i] != (uint16_t)expected[i])
            return false;
    return true;
}

static mc_world_storage_status ap_sound(void *context, mc_world_runtime *world,
                                        const mc_registry_string *name, double x, double y,
                                        double z, float volume, float pitch,
                                        mc_world_storage_error *cause,
                                        mc_world_access_failure *failure) {
    (void)failure;
    ap_fixture *s = context;
    fixture *f = &s->player.game;
    const bool impact = ap_name(name, "random.drr");
    const mc_entity *actor = impact ? s->arrow : f->player;
    mc_arrow_tick_call *c = s->arrow->arrow->tick_call;
    const bool inputs = world == f->world && c != NULL && c->state.active && !s->arrow->base.be &&
                        x == actor->base.x && y == actor->base.y - (double)actor->base.offset &&
                        z == actor->base.z && pitch > 0.f && volume > 0.f && c->hurt_source == NULL;
    s->valid &= inputs;
    if (impact) {
        ++s->impacts;
        s->valid &= volume == 1.f && c->hurt == NULL && c->hurt_bytes == 0 &&
                    c->state.hurt_returned && c->state.hurt_continued && c->state.hurt_result;
    } else {
        s->valid &= c->hurt != NULL && !c->hurt->returned && c->hurt_target == f->player;
        if ((ap_is(s, "wake") || ap_bed(s)) && c->hurt != NULL) {
            const mc_entity_player_wake_state *receipt = &c->hurt->error.player_wake;
            s->wake_before_sound =
                receipt->entered && receipt->returned && receipt->status == MC_ENTITY_OK &&
                receipt->site.pc == 234 && !receipt->active && !receipt->pending &&
                receipt->receiver == NULL && receipt->error == NULL && receipt->cleanup == NULL &&
                f->player->local_player->pending_damage_wake == NULL &&
                !f->player->local_player->fields.player.u &&
                f->player->local_player->fields.player.private_a == 0 &&
                f->player->base.width == .6f && f->player->base.height == 1.8f &&
                f->player->base.offset == 1.62f && !world->fields.J;
        }
    }
    ++s->sounds;
    *cause = (mc_world_storage_error){0};
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status ap_added(void *context, mc_world_runtime *world, mc_entity *item,
                                        mc_world_storage_error *cause,
                                        mc_world_access_failure *failure) {
    ap_fixture *s = context;
    fixture *f = &s->player.game;
    mc_arrow_tick_call *c = s->arrow->arrow->tick_call;
    mc_player_death_execution_view death = {0};
    ++s->additions;
    s->item = item;
    const bool death_view = mc_player_death_execution_inspect(f->player, &death);
    mc_entity_tick_execution_world_token current = {0};
    const bool current_world =
        c != NULL && mc_entity_tick_execution_current_world(&c->execution, &current);
    if (ap_bed(s)) {
        mc_item_stack_fields stack = {0};
        const mc_chunk_data *data = mc_world_chunk_state(s->player.chunk)->data;
        uint8_t metadata = UINT8_MAX;
        s->valid &=
            world == f->world && s->additions == 1 && s->cells == (ap_ladder(s) ? 1u : 2u) &&
            item != NULL && item->base.bF && item->item.c == 10 &&
            mc_item_stack_fields_get(item->stack, &stack) == MC_ITEM_STACK_OK &&
            stack.id == (ap_ladder(s) ? 65 : 355) && stack.count == 1 && stack.damage == 0 &&
            data->blocks[ap_index(7, 64, 8)] == (ap_ladder(s) ? 65 : 0) &&
            mc_chunk_nibbles_get(data->metadata, MC_CHUNK_NIBBLE_BYTES, 7, 64, 8, &metadata) &&
            metadata == (ap_ladder(s) ? 2 : 0) && f->player->local_player->fields.player.u &&
            f->player->local_player->fields.player.private_a == 37 &&
            f->player->local_player->fields.living.Y == 20 && c != NULL && c->hurt != NULL &&
            !c->hurt->returned && current_world &&
            c->execution.leases[current.slot].phase == MC_WORLD_ACCESS_FAILURE_LEASE_ARMED;
        *cause = (mc_world_storage_error){0};
        if (s->valid && (ap_is(s, "wake-bed-foot-source") || ap_is(s, "wake-bed-ladder-source"))) {
            const mc_world_storage_status status =
                mc_world_access_receipt_create(s->observer, NULL, &s->receipt, failure, cause);
            if (status != MC_WORLD_STORAGE_OK)
                return status;
            s->receipt->exception_class = "java/lang/IllegalStateException";
            *cause =
                (mc_world_storage_error){MC_WORLD_STORAGE_IO_ERROR, "Arrow Bed Item observer", 57};
            return cause->status;
        }
        return s->valid ? MC_WORLD_STORAGE_OK : MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    }
    s->valid &=
        world == f->world && item != NULL && item->stack == s->stack && item->base.bF &&
        c != NULL && c->hurt != NULL && !c->hurt->returned && death_view && death.has_child &&
        death.execution == &c->execution && death.child.result != &c->query_result &&
        death.child.result != &c->hurt->execution.owned && death.child.publication != NULL &&
        death.child.publication->entered && death.child.publication->body_entered &&
        !death.child.publication->observer_returned &&
        death.child.publication->execution == &c->execution &&
        death.child.publication->owned == death.child.result &&
        death.child.constructor_world.consumed && death.child.publication_world.open &&
        current_world && current.owner == death.child.publication_world.world.owner &&
        current.slot == death.child.publication_world.world.slot &&
        current.generation == death.child.publication_world.world.generation &&
        c->execution.leases[current.slot].phase == MC_WORLD_ACCESS_FAILURE_LEASE_ARMED &&
        f->player->local_player->fields.player.c->main_slots[0] == s->stack;
    if (ap_is(s, "fatal-source")) {
        bool removed = false;
        mc_world_storage_status status =
            mc_world_runtime_remove_world_access(world, s->observer, &removed, cause);
        if (status != MC_WORLD_STORAGE_OK || !removed)
            return status == MC_WORLD_STORAGE_OK ? MC_WORLD_STORAGE_UNSUPPORTED_STATE : status;
        s->removed = true;
        status = mc_world_access_receipt_create(s->observer, NULL, &s->receipt, failure, cause);
        if (status != MC_WORLD_STORAGE_OK)
            return status;
        s->receipt->exception_class = "java/lang/IllegalStateException";
        *cause = (mc_world_storage_error){MC_WORLD_STORAGE_IO_ERROR,
                                          "Arrow actual Player fatal Item Source failure", 57};
        return cause->status;
    }
    *cause = (mc_world_storage_error){0};
    return MC_WORLD_STORAGE_OK;
}

static bool ap_construct(ap_fixture *s) {
    fixture *f = &s->player.game;
    const bool null_list = ap_is(s, "wake-source-null-list");
    const bool wakes = ap_is(s, "wake") || null_list;
    const bool fatal = ap_is(s, "fatal") || ap_is(s, "fatal-source");
    bool inserted = false;
    mc_arrow_error error = {0};
    CHECK(pd_chunk(&s->player) &&
          mc_math_classes_create(SIZE_MAX, &s->player.math_owner, NULL) == MC_MATH_CLASS_OK &&
          mc_math_classes_ensure(s->player.math_owner, NULL) == MC_MATH_CLASS_OK &&
          mc_entity_set_position(f->player, 9.6, 65.75, 8.5) == MC_ENTITY_OK &&
          mc_world_runtime_insert_entity(f->world, f->player, &inserted, NULL) ==
              MC_WORLD_STORAGE_OK &&
          inserted);
    mc_living_entity_fields *living = mc_entity_living_fields(f->player);
    living->Y = fatal ? 4 : 20;
    living->E = 20;
    living->au = f->player->base.by = 0;
    living->av = 71;
    f->player->local_player->fields.player.u = wakes;
    f->world->fields.B = false;
    f->world->fields.q = 2;
    if (wakes) {
        CHECK(f->player->local_player->fields.player.v == NULL);
        f->player->local_player->fields.player.private_a = 37;
        CHECK(mc_world_runtime_update_player_sleeping(f->world, NULL) == MC_WORLD_STORAGE_OK &&
              f->world->fields.J);
    }
    if (fatal)
        CHECK(pd_stack(3, 2, 7, &s->stack) &&
              mc_inventory_player_source_set(f->player->local_player->fields.player.c, 0, s->stack,
                                             NULL) == MC_INVENTORY_OK);
    if (ap_bed(s))
        CHECK(ap_enter_bed(s));
    CHECK(mc_arrow_allocate(f->app.entities, &s->arrow, NULL) == MC_ENTITY_OK &&
          mc_arrow_construct_position(s->arrow, f->world, ap_arrow_x(s), ap_bed(s) ? 64.85 : 66,
                                      ap_bed(s) ? f->player->base.z : 8.5,
                                      &error) == MC_ENTITY_OK &&
          mc_arrow_heading(s->arrow, s->player.math_owner, ap_ladder(s) ? -1 : 1, 0, 0, .6f, 0,
                           &error) == MC_ENTITY_OK);
    inserted = false;
    CHECK(mc_world_runtime_insert_entity(f->world, s->arrow, &inserted, NULL) ==
              MC_WORLD_STORAGE_OK &&
          inserted);
    const mc_world_access_source_callbacks callbacks = {
        .sound = ap_sound, .cell = ap_bed_cell, .entity_added = ap_added};
    CHECK(mc_world_access_source_create(&callbacks, s, &s->observer, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_access_add_receipted(f->world, s->observer, &s->occurrence, NULL) ==
              MC_WORLD_STORAGE_OK);
    if (null_list) {
        CHECK(f->world->fields.d != NULL &&
              f->world->fields.d == f->world->player_membership.list &&
              f->world->player_membership.count == 1 &&
              f->world->player_membership.entries[0] == f->player);
        /* FD.y first dereferences this nullable Source operand. The actual
         * Player occurrence remains owned by the independent World ledger. */
        f->world->fields.d = NULL;
    }
    return true;
}

static bool ap_unwind_to(mc_arrow_tick_call *call, mc_arrow_tick_native_unwind_phase phase,
                         bool expected_quiet, mc_arrow_tick_state *state) {
    bool quiet = !expected_quiet;
    return mc_arrow_tick_native_unwind_step(call, &quiet) == MC_ARROW_TICK_OK &&
           quiet == expected_quiet && mc_arrow_tick_inspect(call, state) &&
           state->query_unwind == phase;
}

static bool ap_wake_failure(ap_fixture *s, mc_arrow_tick_state *state) {
    fixture *f = &s->player.game;
    mc_arrow_tick_call *call = s->call;
    mc_arrow_tick_hurt_work *hurt = call->hurt;
    mc_local_player_state *player = f->player->local_player;
    const mc_entity_player_wake_state *wake = &hurt->error.player_wake;
    mc_entity_damage_execution_view damage = {0};
    mc_entity_tick_execution_world_token token = {0};
    CHECK(state->hurt_cleanup_acquired && !state->direct_world_open &&
          state->failure->classification == MC_ARROW_TICK_FAILURE_SOURCE_EXCEPTION &&
          state->failure->source_exception && !state->failure->native_policy &&
          state->failure->exception_class != NULL &&
          strcmp(state->failure->exception_class, "java/lang/NullPointerException") == 0 &&
          state->failure->child_site.class_name != NULL &&
          strcmp(state->failure->child_site.class_name, "fd") == 0 &&
          state->failure->child_site.method != NULL &&
          strcmp(state->failure->child_site.method, "y") == 0 &&
          state->failure->child_site.pc == 5);
    CHECK(wake->entered && !wake->active && wake->pending && !wake->returned &&
          wake->receiver == f->player && wake->site.pc == 201 && wake->child.pc == 5 &&
          wake->error != NULL && wake->error->status == MC_PLAYER_SLEEP_SOURCE_NULL &&
          wake->error->world_sleep != NULL && wake->error->world_sleep->source_null &&
          wake->error->world_sleep->bytecode == 5 && !hurt->error.living_damage.entered &&
          mc_entity_damage_execution_inspect(&hurt->execution, &damage) &&
          damage.child == MC_ENTITY_DAMAGE_CHILD_WAKE && damage.execution == &call->execution &&
          damage.acquired_work != NULL && damage.acquired_work == player->pending_damage_wake &&
          damage.wake != NULL && damage.result == mc_player_wake_execution_result(damage.wake) &&
          damage.result != &hurt->execution.owned && damage.result != &call->query_result &&
          mc_entity_tick_execution_current_world(&call->execution, &token) &&
          token.owner == &call->execution &&
          mc_player_wake_execution_owns_world(damage.wake, &token));
    const mc_entity_tick_execution_world_slot *slot = &call->execution.world_slots[token.slot];
    CHECK(slot->phase == MC_ENTITY_TICK_EXECUTION_WORLD_OPEN && slot->world == f->world &&
          slot->site.pc == 198 && slot->generation == token.generation &&
          !player->fields.player.u && player->fields.player.private_a == 37 && f->world->fields.J &&
          f->world->fields.d == NULL && player->fields.living.Y == 20 &&
          player->fields.living.av == 0 && s->sounds == 0 && s->additions == 0 &&
          pd_statistic(f, 18, 0));

    const mc_java_random player_random = *f->player->random, arrow_random = *s->arrow->random;
    CHECK(ap_unwind_to(call, MC_ARROW_TICK_UNWIND_DIAGNOSTIC, false, state) &&
          state->failure->exception_class == call->exception_storage &&
          state->failure->effect == &hurt->error &&
          slot->phase == MC_ENTITY_TICK_EXECUTION_WORLD_OPEN);
    CHECK(ap_unwind_to(call, MC_ARROW_TICK_UNWIND_CAPTURE, false, state) &&
          call->captured_worlds[token.slot].owner == token.owner &&
          call->captured_worlds[token.slot].slot == token.slot &&
          call->captured_worlds[token.slot].generation == token.generation &&
          slot->phase == MC_ENTITY_TICK_EXECUTION_WORLD_OPEN && wake->pending);
    CHECK(ap_unwind_to(call, MC_ARROW_TICK_UNWIND_HURT_CLOSE, false, state) &&
          !call->execution.world_open && slot->phase == MC_ENTITY_TICK_EXECUTION_WORLD_RETAINED &&
          slot->generation == token.generation && slot->world == f->world && wake->pending &&
          player->pending_damage_wake == damage.acquired_work);
    CHECK(ap_unwind_to(call, MC_ARROW_TICK_UNWIND_HURT_CLEANUP, false, state) &&
          state->failure->effect == NULL && !state->hurt_cleanup_acquired &&
          hurt->execution.cleaned && player->pending_damage_wake == NULL && !wake->pending &&
          wake->receiver == NULL && wake->error == NULL && wake->cleanup == NULL &&
          !mc_entity_effect_has_pending(f->player));
    CHECK(ap_unwind_to(call, MC_ARROW_TICK_UNWIND_QUIESCENT, true, state) &&
          state->source_pc == 741 && state->failure->child_site.pc == 5 &&
          strcmp(state->failure->exception_class, "java/lang/NullPointerException") == 0 &&
          slot->phase == MC_ENTITY_TICK_EXECUTION_WORLD_RETAINED &&
          slot->generation == token.generation && !player->fields.player.u &&
          player->fields.player.private_a == 37 && player->fields.living.Y == 20 &&
          f->world->fields.d == NULL && f->world->fields.J &&
          f->world->player_membership.count == 1 &&
          f->world->player_membership.entries[0] == f->player &&
          f->player->random->state == player_random.state &&
          s->arrow->random->state == arrow_random.state && s->arrow->base.x == 8.5 &&
          !s->arrow->base.be && s->sounds == 0 && s->impacts == 0 && s->additions == 0 &&
          pd_statistic(f, 18, 0));
    return true;
}

static bool ap_bed_failure(ap_fixture *s, mc_arrow_tick_state *state) {
    fixture *f = &s->player.game;
    mc_arrow_tick_call *call = s->call;
    mc_arrow_tick_hurt_work *hurt = call->hurt;
    mc_entity_damage_execution_view damage = {0};
    const bool removal = ap_is(s, "wake-bed-ladder-removal-source");
    const size_t cells = ap_ladder(s) && !removal ? 1u : 2u;
    CHECK(state->hurt_cleanup_acquired &&
          state->failure->classification == MC_ARROW_TICK_FAILURE_SOURCE_EXCEPTION &&
          strcmp(state->failure->exception_class, "java/lang/IllegalStateException") == 0 &&
          strcmp(state->failure->child_site.class_name, "fd") == 0 &&
          strcmp(state->failure->child_site.method, removal ? "j" : "c") == 0 &&
          state->failure->child_site.pc == (removal ? 34 : 29) &&
          mc_entity_damage_execution_inspect(&hurt->execution, &damage) && damage.wake != NULL &&
          damage.execution == &call->execution && damage.result != &call->query_result &&
          damage.result != &hurt->execution.owned);
    const mc_world_mutation_drop_execution *drop =
        mc_player_wake_execution_failed_drop(damage.wake);
    if (removal) {
        const mc_world_placement_error *occupancy =
            hurt->error.player_wake.error == NULL ? NULL : hurt->error.player_wake.error->occupancy;
        CHECK(drop == NULL && occupancy != NULL && occupancy->mutation != NULL &&
              occupancy->mutation->hook_class != NULL &&
              strcmp(occupancy->mutation->hook_class, "dp") == 0 &&
              occupancy->mutation->hook_bytecode == 123);
    } else {
        CHECK(drop != NULL && drop->entity == s->item && !drop->active &&
              drop->publication_returned && drop->insertion.observer_returned &&
              drop->publication_scope.consumed && drop->constructor_scope.consumed &&
              drop->publication.cause.system_error == 57);
    }
    CHECK(!hurt->error.living_damage.entered && f->player->local_player->fields.player.u &&
          f->player->local_player->fields.player.private_a == 37 && f->world->fields.J &&
          f->player->local_player->fields.living.Y == 20 && s->cells == cells &&
          s->additions == 1 && s->sounds == 0 && s->impacts == 0 && s->item->base.bF &&
          s->item->item.c == 10 && pd_statistic(f, 18, 0));
    const mc_java_random player_random = *f->player->random, arrow_random = *s->arrow->random,
                         world_random = *f->world->fields.r;
    const mc_arrow_tick_native_unwind_phase phases[] = {
        MC_ARROW_TICK_UNWIND_DIAGNOSTIC, MC_ARROW_TICK_UNWIND_CAPTURE,
        MC_ARROW_TICK_UNWIND_HURT_CLOSE, MC_ARROW_TICK_UNWIND_HURT_CLEANUP,
        MC_ARROW_TICK_UNWIND_QUIESCENT};
    for (size_t i = 0; i < sizeof(phases) / sizeof(phases[0]); ++i) {
        CHECK(ap_unwind_to(call, phases[i], i == 4, state) && s->cells == cells &&
              s->additions == 1 && s->sounds == 0 && s->impacts == 0 && s->item->base.bF &&
              f->player->local_player->fields.player.u &&
              f->player->local_player->fields.player.private_a == 37 && f->world->fields.J &&
              f->player->local_player->fields.living.Y == 20 &&
              f->player->random->state == player_random.state &&
              s->arrow->random->state == arrow_random.state &&
              f->world->fields.r->state == world_random.state);
    }
    CHECK(state->failure->effect == NULL && !state->hurt_cleanup_acquired &&
          hurt->execution.cleaned && f->player->local_player->pending_damage_wake == NULL &&
          !mc_entity_effect_has_pending(f->player) && state->source_pc == 741 &&
          state->failure->child_site.pc == (removal ? 34 : 29) &&
          strcmp(state->failure->exception_class, "java/lang/IllegalStateException") == 0 &&
          pd_statistic(f, 18, 0));
    return true;
}

static bool ap_run(ap_fixture *s) {
    fixture *f = &s->player.game;
    mc_arrow_tick_error admission = {0};
    const mc_arrow_tick_status status = mc_arrow_tick_begin(
        s->arrow, f->app.world_classes, s->player.math_owner, NULL, &s->call, &admission);
    mc_arrow_tick_state state = {0};
    CHECK(s->call != NULL && admission.status == MC_ARROW_TICK_OK &&
          mc_arrow_tick_inspect(s->call, &state) && state.hurt_invoked && state.hurt_returned &&
          !state.hurt_prior_pending && s->valid);
    mc_living_entity_fields *living = mc_entity_living_fields(f->player);
    if (ap_is(s, "live") || ap_is(s, "fatal") || ap_is(s, "wake") || ap_is(s, "wake-bed-foot") ||
        ap_is(s, "wake-bed-ladder")) {
        const bool fatal = ap_is(s, "fatal");
        CHECK(status == MC_ARROW_TICK_OK && state.source_returned && state.hurt_continued &&
              state.hurt_result && state.hurt_bytes == 0 && s->call->hurt == NULL &&
              state.failure->status == MC_ARROW_TICK_OK && state.failure->effect == NULL &&
              living->Y == (fatal ? 0 : 16) && living->av == 0 && s->sounds == 2 &&
              s->impacts == 1 && s->arrow->base.be && s->arrow->base.bt == 1 &&
              s->arrow->arrow->fields.k == 1 &&
              s->arrow->base.x == ap_arrow_x(s) + ap_arrow_velocity(s) &&
              s->arrow->base.velocity_x == ap_arrow_velocity(s) * (double).99f &&
              s->arrow->base.velocity_y == -(double).03f &&
              !mc_entity_effect_has_pending(f->player) && pd_statistic(f, 18, 4));
        if (fatal)
            CHECK(s->additions == 1 && s->item != NULL && s->item->stack == s->stack &&
                  s->item->base.bF && s->item->item.c == 40 &&
                  f->player->local_player->fields.player.c->main_slots[0] == NULL &&
                  pd_statistic(f, 16, 1) && pd_statistic(f, 19, 1));
        else if (ap_bed(s))
            CHECK(s->additions == 1 && s->cells == 2 && s->item != NULL && s->item->base.bF &&
                  s->item->item.c == 10 && pd_statistic(f, 16, 0) && pd_statistic(f, 19, 0));
        else
            CHECK(s->additions == 0 && pd_statistic(f, 16, 0) && pd_statistic(f, 19, 0));
        if (ap_is(s, "wake") || ap_bed(s))
            CHECK(s->wake_before_sound && !f->player->local_player->fields.player.u &&
                  f->player->local_player->fields.player.private_a == 0 && !f->world->fields.J);
    } else {
        CHECK(status != MC_ARROW_TICK_OK && !state.source_returned && state.source_pc == 741 &&
              !state.hurt_continued && !state.hurt_result && !s->arrow->base.be &&
              s->arrow->base.x == ap_arrow_x(s) &&
              state.hurt_bytes == sizeof(mc_arrow_tick_hurt_work) &&
              state.failure->effect == &s->call->hurt->error && s->impacts == 0);
        if (ap_is(s, "wake-source-null-list")) {
            CHECK(ap_wake_failure(s, &state));
        } else if (ap_bed(s)) {
            CHECK(ap_bed_failure(s, &state));
        } else {
            mc_entity_damage_execution_view view = {0};
            mc_player_drop_state drop = {0};
            CHECK(living->Y == 0 && s->sounds == 1 && s->additions == 1 && s->item != NULL &&
                  state.hurt_cleanup_acquired &&
                  state.failure->classification == MC_ARROW_TICK_FAILURE_SOURCE_EXCEPTION &&
                  strcmp(state.failure->exception_class, "java/lang/IllegalStateException") == 0 &&
                  mc_entity_damage_execution_inspect(&s->call->hurt->execution, &view) &&
                  view.player_drop != NULL && view.result != &s->call->query_result &&
                  view.result != &s->call->hurt->execution.owned &&
                  mc_local_player_drop_inspect(view.player_drop, &drop) && drop.item == s->item &&
                  !drop.returned && !drop.publication_returned &&
                  f->player->local_player->fields.player.c->main_slots[0] == s->stack &&
                  pd_statistic(f, 18, 4) && pd_statistic(f, 16, 0) && pd_statistic(f, 19, 0) &&
                  mc_world_access_receipt_retire(&s->receipt, NULL) == MC_WORLD_STORAGE_BUSY);
            const mc_java_random player_random = *f->player->random,
                                 arrow_random = *s->arrow->random;
            const size_t sounds = s->sounds, additions = s->additions;
            bool quiet = false;
            size_t steps = 0;
            while (!quiet && steps++ < 48)
                CHECK(mc_arrow_tick_native_unwind_step(s->call, &quiet) == MC_ARROW_TICK_OK);
            CHECK(quiet && steps < 48 && mc_arrow_tick_inspect(s->call, &state) &&
                  state.failure->effect == NULL &&
                  state.hurt_bytes == sizeof(mc_arrow_tick_hurt_work) &&
                  s->call->hurt->execution.cleaned && !mc_entity_effect_has_pending(f->player) &&
                  s->sounds == sounds && s->additions == additions &&
                  f->player->random->state == player_random.state &&
                  s->arrow->random->state == arrow_random.state && s->arrow->base.x == 8.5 &&
                  !s->arrow->base.be &&
                  (s->stack == NULL ||
                   (s->item->stack == s->stack && s->item->base.bF &&
                    f->player->local_player->fields.player.c->main_slots[0] == s->stack)));
        }
    }
    CHECK(mc_arrow_tick_retire(&s->call) == MC_ARROW_TICK_OK && s->call == NULL &&
          s->arrow->arrow->tick_call == NULL);
    if (s->receipt != NULL)
        CHECK(mc_world_access_receipt_retire(&s->receipt, NULL) == MC_WORLD_STORAGE_OK);
    return true;
}

int main(int argc, char **argv) {
    if (argc != 4 ||
        (strcmp(argv[1], "live") != 0 && strcmp(argv[1], "fatal") != 0 &&
         strcmp(argv[1], "fatal-source") != 0 && strcmp(argv[1], "wake") != 0 &&
         strcmp(argv[1], "wake-bed-foot") != 0 && strcmp(argv[1], "wake-bed-foot-source") != 0 &&
         strcmp(argv[1], "wake-bed-ladder") != 0 &&
         strcmp(argv[1], "wake-bed-ladder-source") != 0 &&
         strcmp(argv[1], "wake-bed-ladder-removal-source") != 0 &&
         strcmp(argv[1], "wake-source-null-list") != 0) ||
        CD(argv[3]) != 0 || !mc_math_trig_init(&trig))
        return 2;
    ap_fixture *s = calloc(1, sizeof(*s));
    if (s == NULL)
        return 1;
    s->mode = argv[1];
    s->valid = true;
    printf("ARROW_HURT_PLAYER_BEGIN mode=%s\n", s->mode);
    fflush(stdout);
    const bool passed =
        setup(&s->player.game, argv[2], "normal", true) && ap_construct(s) && ap_run(s);
    if (s->call != NULL && mc_arrow_tick_retire(&s->call) != MC_ARROW_TICK_OK)
        return 1;
    if (s->receipt != NULL &&
        mc_world_access_receipt_retire(&s->receipt, NULL) != MC_WORLD_STORAGE_OK)
        return 1;
    if (s->observer != NULL && ((!s->removed && mc_world_access_detach_occurrence(
                                                    s->player.game.world, s->observer,
                                                    s->occurrence, NULL) != MC_WORLD_STORAGE_OK) ||
                                mc_world_access_free(s->observer, NULL) != MC_WORLD_STORAGE_OK))
        return 1;
    mc_entity_release(s->arrow);
    mc_item_stack_release(s->stack);
    if (mc_math_classes_release(&s->player.math_owner, NULL) != MC_MATH_CLASS_OK ||
        !pd_chunks_retire(&s->player) || !teardown(&s->player.game))
        return 1;
    free(s);
    if (!passed)
        return 1;
    printf("PASS: %zu actual Arrow Player hurt checks;mode=%s\n", checks, argv[1]);
    return 0;
}
