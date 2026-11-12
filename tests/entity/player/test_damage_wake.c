/* Actual Player damage over the existing genuine World/Bed/sleep fixture.
 * Imported test entry points are never executed. */
#define MC_SLEEP_WAKE_ENTRY mc_damage_wake_unused_sleep_main
#include "test_sleep_wake.c"
#undef MC_SLEEP_WAKE_ENTRY

#include "../../../src/entity/tick/tick_internal.h"
#include "entity/movement/damage_execution_internal.h"
#include "mc/entity/movement/effects.h"
#include "world/chunk/object_state_internal.h"
#include "world/runtime/mutation/drops/execution_internal.h"
#include "world/runtime/mutation/mutation_internal.h"
#include "world/runtime/world_access_internal.h"
#include "world/storage/objects_internal.h"

static bool dw_zero(const char *root, int32_t direction) {
    bool ok = false;
    sw_fixture s = {0};
    mc_entity_effect_error *error = calloc(1, sizeof(*error));
    char path[8192];
    const int32_t input[5] = {direction, 1, direction, 0, 0};
    CHECK(error != NULL &&
          snprintf(path, sizeof(path), "%s/direction-%" PRId32, root, direction) > 0 &&
          sw_prepare(&s, path, input));
    mc_entity *p = s.actor;
    mc_player_fields *fields = &p->local_player->fields.player;
    const mc_player_coordinates *saved = fields->private_b;
    const mc_player_coordinates *bed = fields->v;
    const size_t holds = p->holds;
    const uint64_t random = p->random->state;
    bool result = true;
    p->local_player->fields.living.av = 99;
    const mc_entity_status status =
        mc_entity_effect_damage_in_context(p, s.world->registry[0], NULL, 0, &result, error);
    if (status != MC_ENTITY_OK)
        fprintf(stderr, "damage wake status%d pc%u: %s\n", (int)status,
                (unsigned int)error->source_bytecode, error->operation);
    CHECK(status == MC_ENTITY_OK && !result && !fields->u && fields->private_a == 0 &&
          fields->private_b == saved && fields->v == bed &&
          p->local_player->fields.living.Y == 20 && p->local_player->fields.living.av == 0 &&
          p->base.width == .6f && p->base.height == 1.8f && p->base.offset == 1.62f &&
          p->holds == holds && p->random->state == random && !mc_entity_effect_has_pending(p) &&
          !s.world->worlds[0]->fields.J);
    uint8_t metadata = UINT8_MAX;
    CHECK(mc_chunk_nibbles_get(s.world->states[0]->data->metadata, MC_CHUNK_NIBBLE_BYTES, 8, 64, 8,
                               &metadata) &&
          metadata == (direction | 8));
    /* A second ordinary zero-damage call does not wake or move again. */
    const mc_entity_base_fields after = p->base;
    result = true;
    CHECK(mc_entity_effect_damage_in_context(p, NULL, NULL, 0, &result, NULL) == MC_ENTITY_OK &&
          !result && sw_base_same(&after, &p->base) && p->holds == holds);
    ok = true;
cleanup:
    if (s.actor != NULL && mc_entity_effect_has_pending(s.actor) &&
        mc_entity_effect_cleanup(s.actor, NULL) != MC_ENTITY_OK)
        ok = false;
    if (!sw_retire(&s))
        ok = false;
    free(error);
    return ok;
}

static bool dw_control(const char *root, int32_t mode) {
    bool ok = false;
    sw_fixture s = {0};
    mc_entity_effect_error *error = calloc(1, sizeof(*error));
    mc_entity_effect_error *other = calloc(1, sizeof(*other));
    mc_player_tick_error *parent = calloc(1, sizeof(*parent));
    mc_entity_tick_error *classification = calloc(1, sizeof(*classification));
    char path[8192];
    const int32_t input[5] = {mode, 1, 0, 0, 0};
    CHECK(error != NULL && other != NULL && parent != NULL && classification != NULL &&
          snprintf(path, sizeof(path), "%s/control-%" PRId32, root, mode) > 0 &&
          sw_prepare(&s, path, input));
    mc_entity *p = s.actor;
    mc_world_runtime *world = s.world->worlds[0];
    mc_player_fields *fields = &p->local_player->fields.player;
    const size_t holds = p->holds;
    const uint64_t random = p->random->state;
    const mc_player_coordinates *saved = fields->private_b;
    if (mode == 0)
        p->local_player->fields.living.Y = 0;
    if (mode == 1)
        world->fields.B = true;
    if (mode == 2)
        p->base.world = NULL;
    if (mode == 3 || mode == 7 || mode >= 12)
        p->bounds = NULL;
    if (mode == 4)
        CHECK(sw_cell(s.world->states[0]->data, 8, 64, 8, 0, 0));
    if (mode == 5)
        fields->v = NULL;
    mc_entity_effect_error *output = mode == 7 || mode == 8 ? NULL : error;
    bool answer = true;
    const mc_entity_status status =
        mode == 11 ? mc_entity_effect_fire_damage_in_context(p, s.world->registry[0], 0, error)
                   : mc_entity_effect_damage_in_context(p, mode == 6 ? NULL : s.world->registry[0],
                                                        NULL, mode == 10 ? 4 : 0,
                                                        mode == 9 ? NULL : &answer, output);
    if (mode == 0 || mode == 1) {
        CHECK(status == MC_ENTITY_OK && !answer && fields->u && fields->private_a == 37 &&
              p->holds == holds && !mc_entity_effect_has_pending(p));
    } else if (mode == 2 || mode == 9) {
        CHECK(status != MC_ENTITY_OK && answer && fields->u && fields->private_a == 37 &&
              !mc_entity_effect_has_pending(p) && p->holds == holds &&
              error->source_bytecode == (mode == 2 ? 25 : 0));
    } else if (mode == 3 || mode == 6 || mode == 7 || mode >= 12) {
        CHECK(status != MC_ENTITY_OK && answer && fields->u && fields->private_a == 37 &&
              mc_entity_effect_has_pending(p) && p->holds > holds);
        if (output != NULL) {
            CHECK(error->player_wake.pending && !error->player_wake.active &&
                  !error->player_wake.returned && error->player_wake.receiver == p &&
                  error->player_wake.error != NULL && error->source_bytecode == 35);
            const mc_entity_status original_status = error->status;
            const char *const original_operation = error->operation;
            const mc_player_sleep_error *const original_error = error->player_wake.error;
            const mc_player_site original_site = error->player_wake.site;
            const bool original_source_exception = error->player_wake.source_exception;
            if (mode >= 13) {
                /* Authored cleanup envelopes carry a genuinely failed effect.
                 * These select the actual three parent cleanup sinks; the
                 * separate client fixture enters its environment sink through
                 * the real World/Player/fire call chain. */
                mc_player_tick_error_init(parent);
                mc_entity_effect_error *slot;
                mc_player_tick_effect_child selected;
                if (mode == 13) {
                    parent->child = MC_PLAYER_TICK_CHILD_ENVIRONMENT;
                    slot = &parent->child_error.environment.effect;
                    selected = MC_PLAYER_TICK_EFFECT_ENVIRONMENT;
                } else {
                    parent->child = MC_PLAYER_TICK_CHILD_ACTION;
                    parent->child_error.action.child_kind = MC_PLAYER_ACTION_CHILD_TRAVEL;
                    slot = mode == 14 ? &parent->child_error.action.travel.effect
                                      : &parent->child_error.action.travel.movement.effect;
                    selected =
                        mode == 14 ? MC_PLAYER_TICK_EFFECT_TRAVEL : MC_PLAYER_TICK_EFFECT_MOVEMENT;
                }
                *slot = *error;
                const size_t pending_holds = p->holds;
                p->holds = SIZE_MAX;
                const bool refused = !mc_player_tick_error_retire(parent);
                p->holds = pending_holds;
                CHECK(refused && parent->cleanup.child == MC_PLAYER_TICK_CLEANUP_EFFECT &&
                      parent->cleanup.effect == selected &&
                      parent->cleanup.status == MC_ENTITY_LIMIT && slot->player_wake.pending &&
                      slot->player_wake.receiver == p && slot->status == original_status &&
                      mc_player_tick_error_retire(parent) && !slot->player_wake.pending &&
                      parent->cleanup.status == MC_ENTITY_OK && !mc_entity_effect_has_pending(p));
                *error = *slot;
            }
            if (mode < 13) {
                CHECK(mc_entity_effect_damage_in_context(p, NULL, NULL, 0, &answer, other) ==
                          MC_ENTITY_BUSY &&
                      other->player_wake.pending && other->player_wake.error == original_error);
                CHECK(mc_entity_effect_dead(p, other) == MC_ENTITY_BUSY &&
                      other->player_wake.pending);
                mc_entity_tick_error_init(classification);
                const mc_entity_status busy = mc_entity_effect_damage_in_context(
                    p, NULL, NULL, 0, &answer, &classification->effect);
                CHECK(busy == MC_ENTITY_BUSY &&
                      !mc_tick_effect(classification, busy, "authored pending damage admission",
                                      501) &&
                      classification->status == MC_ENTITY_BUSY && classification->native_policy &&
                      !classification->source_exception && classification->exception_class == NULL);
            }
            if (mode == 12) {
                /* Cleanup must preserve its child even if outer effect
                 * admission fails before that child can be inspected. */
                const size_t pending_holds = p->holds;
                p->holds = SIZE_MAX;
                const bool refused = mc_entity_effect_error_retire(error);
                p->holds = pending_holds;
                CHECK(!refused && error->player_wake.pending && error->player_wake.receiver == p &&
                      error->player_wake.error == original_error &&
                      error->player_wake.cleanup_status == MC_ENTITY_LIMIT);
                struct mc_player_nbt_coordinate *node = p->local_player->nbt_coordinates;
                while (node != NULL && &node->value != fields->v)
                    node = node->next;
                CHECK(node != NULL && node->pins >= 2);
                const size_t pins = node->pins;
                node->pins = 0;
                const bool child_refused = mc_entity_effect_error_retire(error);
                node->pins = pins;
                CHECK(!child_refused && error->player_wake.pending &&
                      error->player_wake.cleanup != NULL &&
                      error->player_wake.cleanup->child == MC_PLAYER_SLEEP_CLEANUP_COORDINATE &&
                      error->status == original_status && error->operation == original_operation &&
                      error->player_wake.error == original_error);
            }
            CHECK(mc_entity_effect_error_retire(error) && !error->player_wake.pending &&
                  error->player_wake.receiver == NULL && error->player_wake.error == NULL &&
                  error->player_wake.cleanup == NULL && error->status == original_status &&
                  error->operation == original_operation &&
                  error->player_wake.site.pc == original_site.pc &&
                  error->player_wake.site.method == original_site.method &&
                  error->player_wake.source_exception == original_source_exception &&
                  p->holds == holds && !mc_entity_effect_has_pending(p));
        } else {
            CHECK(mc_entity_effect_cleanup(p, NULL) == MC_ENTITY_OK && p->holds == holds &&
                  !mc_entity_effect_has_pending(p));
        }
        CHECK(fields->u && fields->private_a == 37);
    } else if (mode == 10) {
        /* Genuine wake completes before the null-App statistic failure. */
        CHECK(status != MC_ENTITY_OK && answer && !fields->u && fields->private_a == 0 &&
              fields->private_b == saved && !mc_entity_effect_has_pending(p) && p->holds == holds);
    } else {
        CHECK(status == MC_ENTITY_OK && !fields->u && fields->private_a == 0 &&
              fields->private_b == saved && !world->fields.J && !mc_entity_effect_has_pending(p) &&
              p->holds == holds);
        if (mode != 11)
            CHECK(!answer);
    }
    CHECK(p->random->state == random);
    ok = true;
cleanup:
    if (s.actor != NULL && mc_entity_effect_has_pending(s.actor) &&
        mc_entity_effect_cleanup(s.actor, NULL) != MC_ENTITY_OK)
        ok = false;
    if (!sw_retire(&s))
        ok = false;
    free(error);
    free(other);
    free(parent);
    free(classification);
    return ok;
}

typedef enum dw_recursive_mode {
    DW_RECURSIVE_READY,
    DW_RECURSIVE_LEGACY_NULL,
    DW_RECURSIVE_OWNED_NULL,
    DW_RECURSIVE_OBSERVER_ERROR,
    DW_RECURSIVE_SCOPE_DRIFT,
    DW_RECURSIVE_NULL_CHUNK_WORLD,
    DW_RECURSIVE_FOOT,
    DW_RECURSIVE_FOOT_SOURCE,
    DW_RECURSIVE_FOOT_SCOPE,
    DW_RECURSIVE_FOOT_MATH_MISSING,
    DW_RECURSIVE_LADDER,
    DW_RECURSIVE_LADDER_SOURCE,
    DW_RECURSIVE_LADDER_SCOPE,
    DW_RECURSIVE_LADDER_MATH_MISSING,
    DW_RECURSIVE_LADDER_REMOVAL_SOURCE,
    DW_RECURSIVE_LADDER_SUPPORTED,
    DW_RECURSIVE_TORCH,
    DW_RECURSIVE_TORCH_SOURCE,
    DW_RECURSIVE_TORCH_SCOPE,
    DW_RECURSIVE_TORCH_MATH_MISSING,
    DW_RECURSIVE_TORCH_REMOVAL_SOURCE,
    DW_RECURSIVE_TORCH_SUPPORTED,
    DW_RECURSIVE_TORCH_EARLY,
    DW_RECURSIVE_TORCH_EARLY_SOURCE,
    DW_RECURSIVE_TORCH_EARLY_REMOVAL_SOURCE,
    DW_RECURSIVE_FLOWER,
    DW_RECURSIVE_FLOWER_SOURCE,
    DW_RECURSIVE_FLOWER_SCOPE,
    DW_RECURSIVE_FLOWER_MATH_MISSING,
    DW_RECURSIVE_FLOWER_REMOVAL_SOURCE,
    DW_RECURSIVE_FLOWER_SUPPORTED,
    DW_RECURSIVE_FLOWER_SKY_SUPPORTED,
    DW_RECURSIVE_FLOWER_DARK,
    DW_RECURSIVE_SAPLING,
    DW_RECURSIVE_SAPLING_SOURCE,
    DW_RECURSIVE_SAPLING_SCOPE,
    DW_RECURSIVE_SAPLING_MATH_MISSING,
    DW_RECURSIVE_SAPLING_REMOVAL_SOURCE,
    DW_RECURSIVE_SAPLING_SUPPORTED,
    DW_RECURSIVE_TALL_GRASS,
    DW_RECURSIVE_TALL_GRASS_SOURCE,
    DW_RECURSIVE_TALL_GRASS_SCOPE,
    DW_RECURSIVE_TALL_GRASS_MATH_MISSING,
    DW_RECURSIVE_TALL_GRASS_REMOVAL_SOURCE,
    DW_RECURSIVE_TALL_GRASS_SUPPORTED,
    DW_RECURSIVE_TALL_GRASS_SEED1234,
    DW_RECURSIVE_TALL_GRASS_NO_ITEM,
    DW_RECURSIVE_TALL_GRASS_NO_ITEM_MATH_MISSING,
} dw_recursive_mode;

static bool dw_foot(dw_recursive_mode mode) {
    return mode == DW_RECURSIVE_FOOT || mode == DW_RECURSIVE_FOOT_SOURCE ||
           mode == DW_RECURSIVE_FOOT_SCOPE || mode == DW_RECURSIVE_FOOT_MATH_MISSING;
}

static bool dw_ladder(dw_recursive_mode mode) {
    return mode == DW_RECURSIVE_LADDER || mode == DW_RECURSIVE_LADDER_SOURCE ||
           mode == DW_RECURSIVE_LADDER_SCOPE || mode == DW_RECURSIVE_LADDER_MATH_MISSING ||
           mode == DW_RECURSIVE_LADDER_REMOVAL_SOURCE || mode == DW_RECURSIVE_LADDER_SUPPORTED;
}

static bool dw_torch(dw_recursive_mode mode) {
    return mode == DW_RECURSIVE_TORCH || mode == DW_RECURSIVE_TORCH_SOURCE ||
           mode == DW_RECURSIVE_TORCH_SCOPE || mode == DW_RECURSIVE_TORCH_MATH_MISSING ||
           mode == DW_RECURSIVE_TORCH_REMOVAL_SOURCE || mode == DW_RECURSIVE_TORCH_SUPPORTED ||
           mode == DW_RECURSIVE_TORCH_EARLY || mode == DW_RECURSIVE_TORCH_EARLY_SOURCE ||
           mode == DW_RECURSIVE_TORCH_EARLY_REMOVAL_SOURCE;
}

static bool dw_torch_early(dw_recursive_mode mode) {
    return mode == DW_RECURSIVE_TORCH_EARLY || mode == DW_RECURSIVE_TORCH_EARLY_SOURCE ||
           mode == DW_RECURSIVE_TORCH_EARLY_REMOVAL_SOURCE;
}

static bool dw_flower(dw_recursive_mode mode) {
    return mode >= DW_RECURSIVE_FLOWER && mode <= DW_RECURSIVE_FLOWER_DARK;
}

static bool dw_flower_supported(dw_recursive_mode mode) {
    return mode == DW_RECURSIVE_FLOWER_SUPPORTED || mode == DW_RECURSIVE_FLOWER_SKY_SUPPORTED;
}

static bool dw_sapling(dw_recursive_mode mode) {
    return mode >= DW_RECURSIVE_SAPLING && mode <= DW_RECURSIVE_SAPLING_SUPPORTED;
}

static bool dw_tall_grass(dw_recursive_mode mode) {
    return mode >= DW_RECURSIVE_TALL_GRASS && mode <= DW_RECURSIVE_TALL_GRASS_NO_ITEM_MATH_MISSING;
}

static bool dw_family(dw_recursive_mode mode) {
    return dw_sapling(mode) || dw_tall_grass(mode);
}

static bool dw_family_supported(dw_recursive_mode mode) {
    return mode == DW_RECURSIVE_SAPLING_SUPPORTED || mode == DW_RECURSIVE_TALL_GRASS_SUPPORTED;
}

static bool dw_family_no_item(dw_recursive_mode mode) {
    return mode == DW_RECURSIVE_TALL_GRASS_NO_ITEM ||
           mode == DW_RECURSIVE_TALL_GRASS_NO_ITEM_MATH_MISSING;
}

static bool dw_family_math_missing(dw_recursive_mode mode) {
    return mode == DW_RECURSIVE_SAPLING_MATH_MISSING ||
           mode == DW_RECURSIVE_TALL_GRASS_MATH_MISSING ||
           mode == DW_RECURSIVE_TALL_GRASS_NO_ITEM_MATH_MISSING;
}

static bool dw_family_normal(dw_recursive_mode mode) {
    return mode == DW_RECURSIVE_SAPLING || mode == DW_RECURSIVE_TALL_GRASS ||
           mode == DW_RECURSIVE_TALL_GRASS_SEED1234 || dw_family_no_item(mode);
}

static int64_t dw_family_seed(dw_recursive_mode mode) {
    return dw_family_no_item(mode)                    ? INT64_C(0x7fffffff)
           : mode == DW_RECURSIVE_TALL_GRASS_SEED1234 ? INT64_C(0x1234)
                                                      : INT64_C(1);
}

/* Independent unsigned Java LCG model. It predicts only declared Random
 * consumption, never World getter answers or the source predicate result. */
static uint32_t dw_family_bits(uint64_t *state, unsigned int bits) {
    *state = (*state * UINT64_C(0x5deece66d) + UINT64_C(11)) & UINT64_C(0xffffffffffff);
    return (uint32_t)(*state >> (48u - bits));
}

static bool dw_family_random(dw_recursive_mode mode, uint64_t before, uint64_t *after,
                             unsigned int *draws) {
    *after = before;
    *draws = 0;
    if (dw_family_supported(mode))
        return false;
    (void)dw_family_bits(after, 24); /* Inherited chance float, even at chance1. */
    ++*draws;
    bool item = true;
    if (dw_tall_grass(mode)) {
        const uint32_t bits = dw_family_bits(after, 31);
        ++*draws;
        item = ((UINT64_C(8) * bits) >> 31u) == 0;
    }
    if (item)
        for (size_t i = 0; i < 3; ++i) {
            (void)dw_family_bits(after, 24);
            ++*draws;
        }
    return item;
}

static bool dw_attached_neighbor(dw_recursive_mode mode) {
    return dw_ladder(mode) || dw_torch(mode) || dw_flower(mode) || dw_family(mode);
}

static uint8_t dw_neighbor_id(dw_recursive_mode mode) {
    return (uint8_t)(dw_sapling(mode)      ? 6
                     : dw_tall_grass(mode) ? 31
                     : dw_flower(mode)     ? 37
                     : dw_torch(mode)      ? 50
                                           : 65);
}

static uint8_t dw_neighbor_metadata(dw_recursive_mode mode) {
    return (uint8_t)(dw_sapling(mode)      ? 2
                     : dw_tall_grass(mode) ? 1
                     : dw_flower(mode)     ? 0
                     : dw_torch(mode)      ? 1
                                           : 2);
}

static int32_t dw_neighbor_item_id(dw_recursive_mode mode) {
    return dw_tall_grass(mode) ? 295 : (int32_t)dw_neighbor_id(mode);
}

static int32_t dw_neighbor_item_damage(dw_recursive_mode mode) {
    return dw_sapling(mode) ? 2 : 0;
}

typedef struct dw_recursive_cell_sample {
    int32_t x, y, z;
    uint8_t head_id, head_metadata, orphan_id, orphan_metadata;
    bool sleeping;
} dw_recursive_cell_sample;

typedef struct dw_recursive_observation {
    sw_fixture *fixture;
    dw_recursive_cell_sample samples[2];
    dw_recursive_cell_sample publication_sample;
    size_t cells_at_addition;
    size_t cells;
    size_t ranges;
    size_t additions;
    bool publication_valid;
    mc_entity *published_item; /* Borrowed from the actual published World occurrence. */
    bool order_valid, external_open;
    dw_recursive_mode mode;
    mc_entity_tick_execution *execution;
    mc_entity_tick_execution_world_token external;
    mc_world_access *listener;
    mc_world_access_receipt *receipt;
    mc_world_chunk *chunk;
    mc_world_runtime *original_chunk_world;
} dw_recursive_observation;

static mc_world_storage_status dw_recursive_added(void *context, mc_world_runtime *world,
                                                  mc_entity *item, mc_world_storage_error *error,
                                                  mc_world_access_failure *failure) {
    (void)failure;
    dw_recursive_observation *o = context;
    o->published_item = item;
    const mc_chunk_data *data = o->fixture->world->states[0]->data;
    uint8_t head_metadata = UINT8_MAX, neighbor_metadata = UINT8_MAX;
    const bool cells_read =
        mc_chunk_nibbles_get(data->metadata, MC_CHUNK_NIBBLE_BYTES, 8, 64, 8, &head_metadata) &&
        mc_chunk_nibbles_get(data->metadata, MC_CHUNK_NIBBLE_BYTES, 7, 64, 8, &neighbor_metadata);
    o->publication_sample =
        (dw_recursive_cell_sample){7,
                                   64,
                                   8,
                                   data->blocks[boxes_index(8, 64, 8)],
                                   head_metadata,
                                   data->blocks[boxes_index(7, 64, 8)],
                                   neighbor_metadata,
                                   o->fixture->actor->local_player->fields.player.u};
    o->cells_at_addition = o->cells;
    mc_item_stack_fields stack = {0};
    mc_entity_tick_execution_world_token token;
    o->publication_valid =
        ++o->additions == 1 && (dw_foot(o->mode) || dw_attached_neighbor(o->mode)) &&
        o->cells == (dw_attached_neighbor(o->mode) ? 1u : 2u) && o->order_valid && cells_read &&
        o->publication_sample.head_id == 26 && head_metadata == 8 &&
        o->publication_sample.orphan_id ==
            (dw_attached_neighbor(o->mode) ? dw_neighbor_id(o->mode) : 0) &&
        neighbor_metadata == (dw_attached_neighbor(o->mode) ? dw_neighbor_metadata(o->mode) : 0) &&
        world == o->fixture->world->worlds[0] && o->fixture->actor->local_player->fields.player.u &&
        world->fields.J && item != NULL && mc_entity_is_kind(item, MC_ENTITY_CLASS_KIND_ITEM) &&
        item->base.bF && item->item.c == 10 &&
        mc_item_stack_fields_get(item->stack, &stack) == MC_ITEM_STACK_OK &&
        !dw_family_no_item(o->mode) &&
        stack.id == (dw_attached_neighbor(o->mode) ? dw_neighbor_item_id(o->mode) : 355) &&
        stack.count == 1 && stack.damage == dw_neighbor_item_damage(o->mode) &&
        item->base.x >= 7.15 && item->base.x < 7.85 && item->base.y >= 64.15 &&
        item->base.y < 64.85 && item->base.z >= 8.15 && item->base.z < 8.85 &&
        world->fields.b->items[world->fields.b->count - 1] == item &&
        mc_entity_tick_execution_current_world(o->execution, &token) &&
        o->execution->leases[token.slot].phase == MC_WORLD_ACCESS_FAILURE_LEASE_ARMED;
    *error = (mc_world_storage_error){0};
    if (o->publication_valid &&
        (o->mode == DW_RECURSIVE_FOOT_SOURCE || o->mode == DW_RECURSIVE_LADDER_SOURCE ||
         o->mode == DW_RECURSIVE_TORCH_SOURCE || o->mode == DW_RECURSIVE_TORCH_EARLY_SOURCE ||
         o->mode == DW_RECURSIVE_FLOWER_SOURCE || o->mode == DW_RECURSIVE_SAPLING_SOURCE ||
         o->mode == DW_RECURSIVE_TALL_GRASS_SOURCE)) {
        const mc_world_storage_status status =
            mc_world_access_receipt_create(o->listener, NULL, &o->receipt, failure, error);
        if (status != MC_WORLD_STORAGE_OK)
            return status;
        o->receipt->exception_class = "java/lang/IllegalStateException";
        *error = (mc_world_storage_error){MC_WORLD_STORAGE_IO_ERROR, "Bed Item observer", 57};
        return error->status;
    }
    if (o->publication_valid &&
        (o->mode == DW_RECURSIVE_FOOT_SCOPE || o->mode == DW_RECURSIVE_LADDER_SCOPE ||
         o->mode == DW_RECURSIVE_TORCH_SCOPE || o->mode == DW_RECURSIVE_FLOWER_SCOPE ||
         o->mode == DW_RECURSIVE_SAPLING_SCOPE || o->mode == DW_RECURSIVE_TALL_GRASS_SCOPE)) {
        const mc_world_storage_status status = mc_entity_tick_execution_push_world(
            o->execution, world, (mc_entity_tick_execution_site){"test", "added", "()V", 0},
            &o->external);
        o->external_open = status == MC_WORLD_STORAGE_OK;
        return status;
    }
    return o->publication_valid ? MC_WORLD_STORAGE_OK : MC_WORLD_STORAGE_UNSUPPORTED_STATE;
}

static mc_world_storage_status dw_recursive_range(void *context, mc_world_runtime *world,
                                                  int32_t min_x, int32_t min_y, int32_t min_z,
                                                  int32_t max_x, int32_t max_y, int32_t max_z,
                                                  mc_world_storage_error *error,
                                                  mc_world_access_failure *failure) {
    (void)failure;
    dw_recursive_observation *o = context;
    const mc_chunk_data *data = o->fixture->world->states[0]->data;
    const mc_player_fields *p = &o->fixture->actor->local_player->fields.player;
    const bool valid =
        o->mode == DW_RECURSIVE_FLOWER_DARK && ++o->ranges == 1 &&
        world == o->fixture->world->worlds[0] && min_x == 7 && max_x == 7 && min_z == 8 &&
        max_z == 8 && min_y == 64 && max_y == 65 && o->cells == 1 && o->additions == 1 &&
        o->publication_valid && data->blocks[boxes_index(7, 64, 8)] == 0 &&
        data->heightmap[(8u << 4u) | 7u] == 65 && p->u && p->private_a == 37 && world->fields.J;
    if (!valid) {
        fprintf(stderr, "Flower range mismatch: (%d,%d,%d)-(%d,%d,%d) count=%zu\n", min_x, min_y,
                min_z, max_x, max_y, max_z, o->ranges);
        *error = (mc_world_storage_error){MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                          "Flower exact height-change observer", 0};
        return error->status;
    }
    puts("FLOWER_WAKE_RANGE min=7,64,8 max=7,65,8 prior-cells=1 Item=1 height-before-store=65");
    *error = (mc_world_storage_error){0};
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status dw_recursive_cell(void *context, mc_world_runtime *world, int32_t x,
                                                 int32_t y, int32_t z,
                                                 mc_world_storage_error *error,
                                                 mc_world_access_failure *failure) {
    dw_recursive_observation *o = context;
    const sw_fixture *s = o->fixture;
    const mc_chunk_data *data = s->world->states[0]->data;
    const mc_player_fields *player = &s->actor->local_player->fields.player;
    const bool head = o->cells == 0;
    const bool attached = dw_attached_neighbor(o->mode);
    const uint8_t orphan = data->blocks[boxes_index(7, 64, 8)];
    const uint8_t head_id = data->blocks[boxes_index(8, 64, 8)];
    uint8_t metadata = UINT8_MAX, orphan_metadata = UINT8_MAX;
    const bool read =
        mc_chunk_nibbles_get(data->metadata, MC_CHUNK_NIBBLE_BYTES, 8, 64, 8, &metadata) &&
        mc_chunk_nibbles_get(data->metadata, MC_CHUNK_NIBBLE_BYTES, 7, 64, 8, &orphan_metadata);
    if (o->cells < COUNT(o->samples))
        o->samples[o->cells] = (dw_recursive_cell_sample){
            x, y, z, head_id, metadata, orphan, orphan_metadata, player->u};
    o->order_valid &= o->cells < 2 && world == s->world->worlds[0] && x == (head ? 8 : 7) &&
                      y == 64 && z == 8 &&
                      orphan == (head ? (attached ? dw_neighbor_id(o->mode) : 26) : 0) && read &&
                      head_id == 26 && metadata == 8 &&
                      orphan_metadata == (head ? (attached           ? dw_neighbor_metadata(o->mode)
                                                  : dw_foot(o->mode) ? 0
                                                                     : 8)
                                               : 0) &&
                      player->u && player->private_a == 37 && world->fields.J &&
                      (!attached || o->additions == (head || dw_family_no_item(o->mode) ? 0u : 1u));
    ++o->cells;
    *error = (mc_world_storage_error){0};
    if (head) {
        if (o->mode == DW_RECURSIVE_NULL_CHUNK_WORLD)
            o->chunk->d = NULL;
        return MC_WORLD_STORAGE_OK;
    }
    if (o->mode == DW_RECURSIVE_LEGACY_NULL || o->mode == DW_RECURSIVE_OWNED_NULL)
        s->actor->base.world = NULL;
    if (o->mode == DW_RECURSIVE_OBSERVER_ERROR || o->mode == DW_RECURSIVE_LADDER_REMOVAL_SOURCE ||
        o->mode == DW_RECURSIVE_TORCH_REMOVAL_SOURCE ||
        o->mode == DW_RECURSIVE_TORCH_EARLY_REMOVAL_SOURCE ||
        o->mode == DW_RECURSIVE_FLOWER_REMOVAL_SOURCE ||
        o->mode == DW_RECURSIVE_SAPLING_REMOVAL_SOURCE ||
        o->mode == DW_RECURSIVE_TALL_GRASS_REMOVAL_SOURCE) {
        const mc_world_storage_status status =
            mc_world_access_receipt_create(o->listener, NULL, &o->receipt, failure, error);
        if (status != MC_WORLD_STORAGE_OK)
            return status;
        o->receipt->exception_class = "java/lang/IllegalStateException";
        *error = (mc_world_storage_error){MC_WORLD_STORAGE_IO_ERROR, "Orphan cell observer", 37};
        return error->status;
    }
    if (o->mode == DW_RECURSIVE_SCOPE_DRIFT) {
        const mc_world_storage_status status = mc_entity_tick_execution_push_world(
            o->execution, world, (mc_entity_tick_execution_site){"test", "cell", "()V", 0},
            &o->external);
        o->external_open = status == MC_WORLD_STORAGE_OK;
        return status;
    }
    return MC_WORLD_STORAGE_OK;
}

static bool dw_recursive(const char *root, dw_recursive_mode mode) {
    const bool legacy = mode == DW_RECURSIVE_LEGACY_NULL;
    const bool null_exit = legacy || mode == DW_RECURSIVE_OWNED_NULL;
    bool ok = false, contexts_ready = false, damage_entered = false, registered = false;
    sw_fixture s = {0};
    mc_world_access *listener = NULL;
    uint64_t occurrence = 0;
    mc_entity_effect_error *error = calloc(1, sizeof(*error));
    mc_entity_damage_execution *damage = calloc(1, sizeof(*damage));
    mc_entity_tick_execution *execution = calloc(1, sizeof(*execution));
    mc_math_execution math = {0};
    mc_math_classes *math_owner = NULL;
    dw_recursive_observation observation = {
        .fixture = &s, .order_valid = true, .mode = mode, .execution = execution};
    const int32_t input[5] = {0, 1, 0, 0, 0};
    CHECK(error != NULL && damage != NULL && execution != NULL && sw_prepare(&s, root, input));
    fixture *f = s.world;
    mc_world_runtime *world = f->worlds[0];
    mc_entity *p = s.actor;
    mc_player_fields *fields = &p->local_player->fields.player;
    mc_chunk_data *data = f->states[0]->data;
    /* Direction zero points along +Z. The sleeping Player's Bed has a foot
     * at (8,64,7). The other head's own foot cell (7,64,7) remains air. */
    const bool foot = dw_foot(mode);
    const bool attached = dw_attached_neighbor(mode);
    CHECK(sw_cell(data, 7, 64, 8, attached ? dw_neighbor_id(mode) : 26,
                  attached ? dw_neighbor_metadata(mode)
                  : foot   ? 0
                           : 8) &&
          data->blocks[boxes_index(7, 64, foot || attached ? 9 : 7)] == 0);
    if (mode == DW_RECURSIVE_LADDER_SUPPORTED)
        CHECK(sw_cell(data, 7, 64, 9, 1, 0));
    if (dw_torch(mode)) {
        CHECK(sw_cell(data, 7, 63, 8, dw_torch_early(mode) ? 0 : 1, 0) &&
              data->blocks[boxes_index(6, 64, 8)] == 0);
        if (mode == DW_RECURSIVE_TORCH_SUPPORTED)
            CHECK(sw_cell(data, 6, 64, 8, 1, 0));
    }
    if (dw_flower(mode)) {
        CHECK(sw_cell(data, 7, 63, 8,
                      dw_flower_supported(mode) || mode == DW_RECURSIVE_FLOWER_DARK ? 2 : 0, 0));
        memset(data->skylight,
               mode == DW_RECURSIVE_FLOWER_SKY_SUPPORTED || mode == DW_RECURSIVE_FLOWER_DARK ? 0
                                                                                             : 255,
               sizeof(data->skylight));
        memset(data->block_light, 0, sizeof(data->block_light));
        memset(data->heightmap, mode == DW_RECURSIVE_FLOWER_DARK ? 65 : 64,
               sizeof(data->heightmap));
        world->fields.f = 5;
        f->classes[0]->chunk_a = false;
    }
    if (dw_family(mode)) {
        CHECK(sw_cell(data, 7, 63, 8, dw_family_supported(mode) ? 2 : 0, 0));
        memset(data->skylight, 255, sizeof(data->skylight));
        memset(data->block_light, 0, sizeof(data->block_light));
        memset(data->heightmap, 64, sizeof(data->heightmap));
        world->fields.f = 5;
        f->classes[0]->chunk_a = false;
        mc_java_random_reseed(world->fields.r, dw_family_seed(mode));
    }
    if (mode == DW_RECURSIVE_NULL_CHUNK_WORLD) {
        observation.chunk = mc_world_store_chunk_object(world->options.store, f->states[0]);
        CHECK(observation.chunk != NULL && observation.chunk->d == world);
        observation.original_chunk_world = observation.chunk->d;
    }
    const mc_world_access_source_callbacks callbacks = {
        .cell = dw_recursive_cell, .range = dw_recursive_range, .entity_added = dw_recursive_added};
    CHECK(mc_world_access_source_create(&callbacks, &observation, &listener, NULL) ==
          MC_WORLD_STORAGE_OK);
    observation.listener = listener;
    CHECK(mc_world_access_add_receipted(world, listener, &occurrence, NULL) == MC_WORLD_STORAGE_OK);
    registered = true;
    const size_t holds = p->holds, entities = world->fields.b->count;
    const uint64_t random = p->random->state;
    const uint64_t world_random = world->fields.r->state;
    mc_java_random drop_random = *world->fields.r;
    unsigned int family_draws = 0;
    if (dw_family(mode)) {
        const bool modeled_item =
            dw_family_random(mode, world_random, &drop_random.state, &family_draws);
        CHECK(modeled_item == (!dw_family_supported(mode) && !dw_family_no_item(mode)));
    } else {
        for (size_t i = 0; i < 4; ++i)
            (void)mc_java_random_next_float(&drop_random);
    }
    p->local_player->fields.living.av = 99;
    mc_entity_status raw = MC_ENTITY_BUSY;
    bool accepted = true, continued = false;
    mc_entity_effect_error_init(error);
    if (legacy) {
        raw = mc_entity_effect_damage_in_context(p, f->registry[0], NULL, 0, &accepted, error);
    } else {
        if ((foot || attached) && mode != DW_RECURSIVE_FOOT_MATH_MISSING &&
            mode != DW_RECURSIVE_LADDER_MATH_MISSING && mode != DW_RECURSIVE_TORCH_MATH_MISSING &&
            mode != DW_RECURSIVE_FLOWER_MATH_MISSING && !dw_family_math_missing(mode))
            CHECK(mc_math_classes_create(SIZE_MAX, &math_owner, NULL) == MC_MATH_CLASS_OK);
        mc_math_execution_init(&math, math_owner);
        mc_entity_tick_execution_init(execution, &math);
        contexts_ready = true;
        const mc_world_runtime_execution_code code = {{"lm", "e", "()V", 6},
                                                      {"lm", "e", "()V", 33}};
        mc_entity_damage_execution_init(damage, execution, f->classes[0], code, f->registry[0]);
        continued =
            mc_entity_effect_damage_in_execution(damage, p, NULL, 0, &accepted, error, &raw);
        damage_entered = true;
    }
    uint8_t head_metadata = UINT8_MAX, orphan_metadata = UINT8_MAX;
    CHECK(mc_chunk_nibbles_get(data->metadata, MC_CHUNK_NIBBLE_BYTES, 8, 64, 8, &head_metadata) &&
          mc_chunk_nibbles_get(data->metadata, MC_CHUNK_NIBBLE_BYTES, 7, 64, 8, &orphan_metadata));
    const uint8_t orphan = data->blocks[boxes_index(7, 64, 8)];
    if (dw_family(mode))
        CHECK(error->player_wake.entered && error->player_bytecode == 35);
    if (!legacy) {
        mc_entity_damage_failure failure;
        const char *exception =
            !continued && mc_entity_damage_execution_source_failure(damage, &failure) &&
                    failure.source_proven && failure.exception_class != NULL
                ? failure.exception_class
                : "NONE";
        printf("OWNED_WAKE_RECURSION {\"continued\":%s,\"raw\":%d,\"pc\":%u,"
               "\"wake_entered\":%s,\"pending\":%s,\"result\":%s,\"exception\":\"%s\","
               "\"after\":{\"sleeping\":%s,\"timer\":%d,\"health\":%d,\"idle\":%d,"
               "\"head\":[%u,%u],\"orphan\":[%u,%u],\"world_sleeping\":%s,\"world_same\":%s,"
               "\"position\":[\"%016" PRIx64 "\",\"%016" PRIx64 "\",\"%016" PRIx64 "\"],"
               "\"dimensions\":[\"%08" PRIx32 "\",\"%08" PRIx32 "\",\"%08" PRIx32 "\"]},"
               "\"callbacks\":[",
               sw_bool(continued), (int)raw, (unsigned int)error->player_bytecode,
               sw_bool(error->player_wake.entered), sw_bool(mc_entity_effect_has_pending(p)),
               continued ? sw_bool(accepted) : "null", exception, sw_bool(fields->u),
               fields->private_a, p->local_player->fields.living.Y,
               p->local_player->fields.living.av, (unsigned int)data->blocks[boxes_index(8, 64, 8)],
               (unsigned int)head_metadata, (unsigned int)orphan, (unsigned int)orphan_metadata,
               sw_bool(world->fields.J), sw_bool(p->base.world == world),
               movement_double_bits(p->base.x), movement_double_bits(p->base.y),
               movement_double_bits(p->base.z), movement_float_bits(p->base.width),
               movement_float_bits(p->base.height), movement_float_bits(p->base.offset));
        for (size_t i = 0; i < observation.cells && i < COUNT(observation.samples); ++i) {
            const dw_recursive_cell_sample *cell = &observation.samples[i];
            printf("%s{\"xyz\":[%d,%d,%d],\"head\":[%u,%u],\"orphan\":[%u,%u],"
                   "\"sleeping\":%s}",
                   i == 0 ? "" : ",", cell->x, cell->y, cell->z, (unsigned int)cell->head_id,
                   (unsigned int)cell->head_metadata, (unsigned int)cell->orphan_id,
                   (unsigned int)cell->orphan_metadata, sw_bool(cell->sleeping));
        }
        puts("]}");
    }
    if ((foot || attached) && observation.published_item != NULL) {
        const mc_entity *item = observation.published_item;
        mc_item_stack_fields stack = {0};
        CHECK(mc_item_stack_fields_get(item->stack, &stack) == MC_ITEM_STACK_OK);
        printf("%s {\"world_seed\":\"%016" PRIx64 "\","
               "\"player_seed\":\"%016" PRIx64 "\",\"item\":{"
               "\"position\":[\"%016" PRIx64 "\",\"%016" PRIx64 "\",\"%016" PRIx64 "\"],"
               "\"delay\":%d,\"stack\":[%d,%d,%d],\"inserted\":%s,\"world_same\":%s}}\n",
               dw_family(mode)   ? "PLANT_FAMILY_WAKE_DROP"
               : dw_flower(mode) ? "FLOWER_WAKE_DROP"
               : dw_torch(mode)  ? "TORCH_WAKE_DROP"
               : attached        ? "LADDER_WAKE_DROP"
                                 : "FOOT_WAKE_DROP",
               world->fields.r->state, p->random->state, movement_double_bits(item->base.x),
               movement_double_bits(item->base.y), movement_double_bits(item->base.z), item->item.c,
               stack.id, stack.count, stack.damage, sw_bool(item->base.bF),
               sw_bool(item->base.world == world));
        if (attached) {
            const dw_recursive_cell_sample *sample = &observation.publication_sample;
            printf("%s {\"head\":[%u,%u],\"%s\":[%u,%u],"
                   "\"sleeping\":%s,\"prior_cells\":%zu}\n",
                   dw_family(mode)   ? "PLANT_FAMILY_WAKE_PUBLICATION"
                   : dw_flower(mode) ? "FLOWER_WAKE_PUBLICATION"
                   : dw_torch(mode)  ? "TORCH_WAKE_PUBLICATION"
                                     : "LADDER_WAKE_PUBLICATION",
                   (unsigned int)sample->head_id, (unsigned int)sample->head_metadata,
                   dw_family(mode)   ? "plant"
                   : dw_flower(mode) ? "flower"
                   : dw_torch(mode)  ? "torch"
                                     : "ladder",
                   (unsigned int)sample->orphan_id, (unsigned int)sample->orphan_metadata,
                   sw_bool(sample->sleeping), observation.cells_at_addition);
        }
    }
    if (dw_torch(mode)) {
        uint8_t support_metadata = UINT8_MAX, floor_metadata = UINT8_MAX;
        CHECK(
            mc_chunk_nibbles_get(data->metadata, MC_CHUNK_NIBBLE_BYTES, 6, 64, 8,
                                 &support_metadata) &&
            mc_chunk_nibbles_get(data->metadata, MC_CHUNK_NIBBLE_BYTES, 7, 63, 8, &floor_metadata));
        printf("TORCH_WAKE_SUPPORT {\"torch_support\":[%u,%u],\"torch_floor\":[%u,%u]}\n",
               (unsigned int)data->blocks[boxes_index(6, 64, 8)], (unsigned int)support_metadata,
               (unsigned int)data->blocks[boxes_index(7, 63, 8)], (unsigned int)floor_metadata);
    }
    if (dw_flower(mode)) {
        uint8_t floor_metadata = UINT8_MAX, sky = UINT8_MAX, block = UINT8_MAX;
        CHECK(mc_chunk_nibbles_get(data->metadata, MC_CHUNK_NIBBLE_BYTES, 7, 63, 8,
                                   &floor_metadata) &&
              mc_chunk_nibbles_get(data->skylight, MC_CHUNK_NIBBLE_BYTES, 7, 64, 8, &sky) &&
              mc_chunk_nibbles_get(data->block_light, MC_CHUNK_NIBBLE_BYTES, 7, 64, 8, &block));
        printf("FLOWER_WAKE_ENV {\"flower_floor\":[%u,%u],\"light\":[%u,%u,%u],"
               "\"sky_subtraction\":%d,\"chunk_light_seen\":%s,"
               "\"world_seed\":\"%016" PRIx64 "\",\"player_seed\":\"%016" PRIx64 "\"}\n",
               (unsigned int)data->blocks[boxes_index(7, 63, 8)], (unsigned int)floor_metadata,
               (unsigned int)sky, (unsigned int)block,
               (unsigned int)data->heightmap[(8u << 4u) | 7u], world->fields.f,
               sw_bool(f->classes[0]->chunk_a), world->fields.r->state, p->random->state);
        CHECK(f->classes[0]->chunk_a ==
              (mode != DW_RECURSIVE_FLOWER_SKY_SUPPORTED && mode != DW_RECURSIVE_FLOWER_DARK));
    }
    if (dw_family(mode)) {
        uint8_t floor_metadata = UINT8_MAX, sky = UINT8_MAX, block = UINT8_MAX;
        CHECK(mc_chunk_nibbles_get(data->metadata, MC_CHUNK_NIBBLE_BYTES, 7, 63, 8,
                                   &floor_metadata) &&
              mc_chunk_nibbles_get(data->skylight, MC_CHUNK_NIBBLE_BYTES, 7, 64, 8, &sky) &&
              mc_chunk_nibbles_get(data->block_light, MC_CHUNK_NIBBLE_BYTES, 7, 64, 8, &block));
        printf("PLANT_FAMILY_WAKE_ENV {\"family\":\"%s\",\"plant_floor\":[%u,%u],"
               "\"light\":[%u,%u,%u],\"sky_subtraction\":%d,\"chunk_light_seen\":%s,"
               "\"world_seed\":\"%016" PRIx64 "\",\"player_seed\":\"%016" PRIx64 "\","
               "\"seed_input\":%" PRId64 ",\"items\":%zu,\"math_owned\":%s,\"model_draws\":%u}\n",
               dw_sapling(mode) ? "sapling" : "tall-grass",
               (unsigned int)data->blocks[boxes_index(7, 63, 8)], (unsigned int)floor_metadata,
               (unsigned int)sky, (unsigned int)block,
               (unsigned int)data->heightmap[(8u << 4u) | 7u], world->fields.f,
               sw_bool(f->classes[0]->chunk_a), world->fields.r->state, p->random->state,
               dw_family_seed(mode), observation.additions, sw_bool(math_owner != NULL),
               family_draws);
        CHECK(sky == 15 && block == 0 && data->heightmap[(8u << 4u) | 7u] == 64 &&
              world->fields.f == 5 && f->classes[0]->chunk_a &&
              world->fields.r->state == drop_random.state && p->random->state == random);
    }
    CHECK(observation.ranges == (mode == DW_RECURSIVE_FLOWER_DARK ? 1u : 0u));
    if (null_exit) {
        const mc_entity_player_wake_state *wake = &error->player_wake;
        if (wake->child.pc != 4)
            fprintf(stderr, "NULL_EXIT observed=%s:%u child=%s:%u callback_count=%zu\n",
                    wake->site.class_name == NULL ? "none" : wake->site.class_name, wake->site.pc,
                    wake->child.class_name == NULL ? "none" : wake->child.class_name,
                    wake->child.pc, observation.cells);
        CHECK(raw != MC_ENTITY_OK && accepted && wake->pending && wake->source_exception &&
              !wake->native_policy && wake->site.pc == 103 && wake->child.pc == 4 &&
              wake->child.class_name != NULL && strcmp(wake->child.class_name, "ve") == 0 &&
              wake->exception_class != NULL &&
              strcmp(wake->exception_class, "java/lang/NullPointerException") == 0 && fields->u &&
              fields->private_a == 37 && world->fields.J && p->base.world == NULL &&
              p->local_player->fields.living.Y == 20 && p->local_player->fields.living.av == 0 &&
              head_metadata == 8 && orphan == 0 && orphan_metadata == 0 &&
              observation.order_valid && observation.cells == 2);
        if (!legacy) {
            mc_entity_damage_failure failure;
            CHECK(!continued && mc_entity_damage_execution_source_failure(damage, &failure) &&
                  failure.source_proven &&
                  failure.kind == MC_ENTITY_DAMAGE_FAILURE_SOURCE_EXCEPTION &&
                  failure.site.pc == 4 && strcmp(failure.site.class_name, "ve") == 0);
        }
        ok = true;
        goto cleanup;
    }
    if (mode == DW_RECURSIVE_NULL_CHUNK_WORLD) {
        mc_entity_damage_failure failure;
        CHECK(!continued && raw != MC_ENTITY_OK && accepted && error->player_wake.pending &&
              error->player_wake.site.pc == 80 && fields->u && fields->private_a == 37 &&
              world->fields.J && head_metadata == 8 && orphan == 0 && orphan_metadata == 0 &&
              observation.order_valid && observation.cells == 1 && observation.chunk->d == NULL &&
              mc_entity_damage_execution_source_failure(damage, &failure) &&
              failure.source_proven && failure.kind == MC_ENTITY_DAMAGE_FAILURE_SOURCE_EXCEPTION &&
              failure.site.pc == 194 && strcmp(failure.site.class_name, "lm") == 0 &&
              strcmp(failure.exception_class, "java/lang/NullPointerException") == 0);
        ok = true;
        goto cleanup;
    }
    if (mode == DW_RECURSIVE_LADDER_SUPPORTED || mode == DW_RECURSIVE_TORCH_SUPPORTED ||
        dw_flower_supported(mode) || dw_family_supported(mode)) {
        CHECK(continued && raw == MC_ENTITY_OK && !accepted && !fields->u &&
              fields->private_a == 0 && !world->fields.J && head_metadata == 8 &&
              orphan == dw_neighbor_id(mode) && orphan_metadata == dw_neighbor_metadata(mode) &&
              observation.cells == 1 && observation.additions == 0 && observation.order_valid &&
              p->random->state == random && world->fields.r->state == world_random &&
              world->fields.b->count == entities && !mc_entity_effect_has_pending(p));
        ok = true;
        goto cleanup;
    }
    if (mode == DW_RECURSIVE_LADDER_REMOVAL_SOURCE || mode == DW_RECURSIVE_TORCH_REMOVAL_SOURCE ||
        mode == DW_RECURSIVE_TORCH_EARLY_REMOVAL_SOURCE ||
        mode == DW_RECURSIVE_FLOWER_REMOVAL_SOURCE || mode == DW_RECURSIVE_SAPLING_REMOVAL_SOURCE ||
        mode == DW_RECURSIVE_TALL_GRASS_REMOVAL_SOURCE) {
        mc_entity_damage_failure failure;
        const mc_world_placement_error *occupancy =
            error->player_wake.error == NULL ? NULL : error->player_wake.error->occupancy;
        const mc_world_mutation_error *mutation = occupancy == NULL ? NULL : occupancy->mutation;
        CHECK(!continued && raw != MC_ENTITY_OK && accepted && fields->u &&
              fields->private_a == 37 && world->fields.J && head_metadata == 8 && orphan == 0 &&
              orphan_metadata == 0 && observation.cells == 2 && observation.additions == 1 &&
              observation.order_valid && observation.publication_valid &&
              mc_entity_damage_execution_source_failure(damage, &failure) &&
              failure.source_proven && failure.site.pc == 34 &&
              strcmp(failure.exception_class, "java/lang/IllegalStateException") == 0 &&
              mutation != NULL && mutation->hook_class != NULL &&
              strcmp(mutation->hook_class, dw_flower(mode) || dw_family(mode) ? "wb"
                                           : dw_torch(mode)                   ? "vm"
                                                                              : "dp") == 0 &&
              mutation->hook_bytecode == (dw_flower(mode) || dw_family(mode) ? 17u
                                          : dw_torch_early(mode)             ? 6u
                                          : dw_torch(mode)                   ? 164u
                                                                             : 123u) &&
              (!dw_torch_early(mode) || mutation->torch_survive_bytecode == 35));
        ok = true;
        goto cleanup;
    }
    if (foot || attached) {
        if (!dw_family_normal(mode) && mode != DW_RECURSIVE_FOOT && mode != DW_RECURSIVE_LADDER &&
            mode != DW_RECURSIVE_TORCH && mode != DW_RECURSIVE_TORCH_EARLY &&
            mode != DW_RECURSIVE_FLOWER && mode != DW_RECURSIVE_FLOWER_DARK) {
            mc_entity_damage_execution_view view = {0};
            CHECK(!continued && accepted && fields->u && fields->private_a == 37 &&
                  world->fields.J && head_metadata == 8 &&
                  orphan == (attached ? dw_neighbor_id(mode) : 0) &&
                  orphan_metadata == (attached ? dw_neighbor_metadata(mode) : 0) &&
                  observation.order_valid && observation.cells == (attached ? 1u : 2u) &&
                  world->fields.r->state == drop_random.state && p->random->state == random &&
                  mc_entity_damage_execution_inspect(damage, &view) && view.wake != NULL);
            const mc_world_mutation_drop_execution *drop =
                mc_player_wake_execution_failed_drop(view.wake);
            CHECK(drop != NULL && !drop->active && drop->entity != NULL &&
                  drop->constructor_scope.consumed && drop->publication_returned);
            if (mode == DW_RECURSIVE_FOOT_MATH_MISSING ||
                mode == DW_RECURSIVE_LADDER_MATH_MISSING ||
                mode == DW_RECURSIVE_TORCH_MATH_MISSING ||
                mode == DW_RECURSIVE_FLOWER_MATH_MISSING || dw_family_math_missing(mode)) {
                CHECK(raw == MC_ENTITY_OK && math.failed && damage->math_failed &&
                      view.result->math_failed && drop->insertion.entered &&
                      !drop->insertion.chunk_returned && observation.additions == 0 &&
                      world->fields.b->count == entities);
            } else {
                CHECK(observation.additions == 1 && observation.publication_valid &&
                      world->fields.b->count == entities + 1);
                if (mode == DW_RECURSIVE_FOOT_SOURCE || mode == DW_RECURSIVE_LADDER_SOURCE ||
                    mode == DW_RECURSIVE_TORCH_SOURCE || mode == DW_RECURSIVE_TORCH_EARLY_SOURCE ||
                    mode == DW_RECURSIVE_FLOWER_SOURCE || mode == DW_RECURSIVE_SAPLING_SOURCE ||
                    mode == DW_RECURSIVE_TALL_GRASS_SOURCE) {
                    mc_entity_damage_failure failure;
                    CHECK(raw != MC_ENTITY_OK && drop->publication_scope.consumed &&
                          drop->insertion.observer_returned &&
                          mc_entity_damage_execution_source_failure(damage, &failure) &&
                          failure.source_proven && failure.site.pc == 29 &&
                          failure.kind == MC_ENTITY_DAMAGE_FAILURE_SOURCE_EXCEPTION &&
                          strcmp(failure.exception_class, "java/lang/IllegalStateException") == 0);
                } else {
                    CHECK(raw == MC_ENTITY_OK && observation.external_open &&
                          view.result->ownership_status == MC_WORLD_STORAGE_BUSY &&
                          !mc_player_wake_execution_owns_world(view.wake, &observation.external) &&
                          mc_entity_damage_execution_cleanup(damage, error) == MC_ENTITY_BUSY &&
                          observation.additions == 1);
                }
            }
            ok = true;
            goto cleanup;
        }
        if (!continued)
            fprintf(stderr,
                    "Block drop stopped: raw=%d callbacks=%zu ownership=%d operation=%s child=%s "
                    "math=%d\n",
                    (int)raw, observation.cells, (int)damage->owned.ownership_status,
                    damage->owned.ownership_error.operation == NULL
                        ? "none"
                        : damage->owned.ownership_error.operation,
                    error->operation == NULL ? "none" : error->operation, (int)math.failed);
        CHECK(continued && raw == MC_ENTITY_OK && !accepted && !fields->u &&
              fields->private_a == 0 && !world->fields.J && head_metadata == 8 && orphan == 0 &&
              orphan_metadata == 0 && observation.order_valid && observation.cells == 2 &&
              world->fields.b->count == entities + (dw_family_no_item(mode) ? 0u : 1u) &&
              !mc_entity_effect_has_pending(p) &&
              observation.additions == (dw_family_no_item(mode) ? 0u : 1u) &&
              observation.publication_valid == !dw_family_no_item(mode) &&
              p->random->state == random && world->fields.r->state == drop_random.state);
        if (dw_family_no_item(mode))
            CHECK(observation.published_item == NULL && observation.receipt == NULL &&
                  !observation.external_open && !math.failed && !damage->math_failed &&
                  (mode != DW_RECURSIVE_TALL_GRASS_NO_ITEM_MATH_MISSING || math_owner == NULL));
        ok = true;
        goto cleanup;
    }
    if (mode != DW_RECURSIVE_READY) {
        CHECK(!continued && accepted && error->player_wake.pending &&
              error->player_wake.site.pc == 80 && fields->u && fields->private_a == 37 &&
              world->fields.J && head_metadata == 8 && orphan == 0 && orphan_metadata == 0 &&
              observation.order_valid && observation.cells == 2 &&
              p->local_player->fields.living.Y == 20 && p->local_player->fields.living.av == 0 &&
              p->random->state == random && world->fields.b->count == entities);
        mc_entity_damage_execution_view view;
        CHECK(mc_entity_damage_execution_inspect(damage, &view) && view.wake != NULL);
        if (mode == DW_RECURSIVE_OBSERVER_ERROR) {
            mc_entity_damage_failure failure;
            CHECK(raw != MC_ENTITY_OK && error->player_wake.error != NULL &&
                  error->player_wake.error->world.system_error == 37 &&
                  mc_entity_damage_execution_source_failure(damage, &failure) &&
                  failure.source_proven &&
                  failure.kind == MC_ENTITY_DAMAGE_FAILURE_SOURCE_EXCEPTION &&
                  failure.site.pc == 34 &&
                  strcmp(failure.exception_class, "java/lang/IllegalStateException") == 0 &&
                  observation.receipt != NULL &&
                  mc_world_access_receipt_retire(&observation.receipt, NULL) ==
                      MC_WORLD_STORAGE_BUSY);
        } else {
            mc_entity_tick_execution_world_token current;
            CHECK(raw == MC_ENTITY_OK && view.result->ownership_status == MC_WORLD_STORAGE_BUSY &&
                  observation.external_open &&
                  mc_entity_tick_execution_current_world(execution, &current) &&
                  current.slot == observation.external.slot &&
                  current.generation == observation.external.generation &&
                  !mc_player_wake_execution_owns_world(view.wake, &current) &&
                  mc_entity_damage_execution_cleanup(damage, error) == MC_ENTITY_BUSY &&
                  error->player_wake.pending && observation.cells == 2);
        }
        ok = true;
        goto cleanup;
    }
    CHECK(continued && raw == MC_ENTITY_OK && !accepted);
    CHECK(!fields->u && fields->private_a == 0 && !world->fields.J && fields->v == s.before_v &&
          fields->private_b == s.before_q && p->local_player->fields.living.Y == 20 &&
          p->local_player->fields.living.av == 0 && p->base.width == .6f &&
          p->base.height == 1.8f && p->base.offset == 1.62f && p->base.x == 7.5 &&
          p->base.y == 65.720001220703125 && p->base.z == 7.5 && p->holds == holds &&
          p->random->state == random && world->fields.b->count == entities &&
          !mc_entity_effect_has_pending(p) && head_metadata == 8 && orphan == 0 &&
          orphan_metadata == 0 && data->blocks[boxes_index(8, 64, 8)] == 26 &&
          data->blocks[boxes_index(8, 64, 7)] == 26 && observation.order_valid &&
          observation.cells == 2);
    ok = true;
cleanup:
    if (observation.external_open &&
        mc_entity_tick_execution_pop_world(execution, &observation.external, false) !=
            MC_WORLD_STORAGE_OK)
        return false;
    if (damage_entered) {
        const uint64_t family_world_random = s.world->worlds[0]->fields.r->state;
        const uint64_t family_player_random = s.actor->random->state;
        const size_t family_entities = s.world->worlds[0]->fields.b->count;
        const size_t family_cells = observation.cells, family_additions = observation.additions;
        const uint8_t family_plant = s.world->states[0]->data->blocks[boxes_index(7, 64, 8)];
        const bool family_sleeping = s.actor->local_player->fields.player.u;
        const int32_t family_timer = s.actor->local_player->fields.player.private_a;
        const size_t receipt_holds = observation.receipt == NULL ? 0 : observation.receipt->holds;
        /* The canonical execution already holds this producer. Native cleanup
         * must finish without adding a legacy diagnostic occurrence. */
        if (observation.receipt != NULL)
            observation.receipt->holds = SIZE_MAX;
        const mc_entity_status status = mc_entity_damage_execution_cleanup(damage, error);
        const bool receipt_unchanged =
            observation.receipt == NULL || observation.receipt->holds == SIZE_MAX;
        if (observation.receipt != NULL)
            observation.receipt->holds = receipt_holds;
        if (status != MC_ENTITY_OK) {
            fprintf(stderr, "Owned wake damage cleanup refused: %d\n", (int)status);
            if (mc_entity_damage_execution_cleanup(damage, error) != MC_ENTITY_OK)
                return false;
            ok = false;
        }
        if (!receipt_unchanged || !damage->cleaned || mc_entity_effect_has_pending(s.actor))
            return false;
        if (dw_family(mode) &&
            (s.world->worlds[0]->fields.r->state != family_world_random ||
             s.actor->random->state != family_player_random ||
             s.world->worlds[0]->fields.b->count != family_entities ||
             observation.cells != family_cells || observation.additions != family_additions ||
             s.world->states[0]->data->blocks[boxes_index(7, 64, 8)] != family_plant ||
             s.actor->local_player->fields.player.u != family_sleeping ||
             s.actor->local_player->fields.player.private_a != family_timer))
            return false;
    }
    if (legacy && s.actor != NULL && mc_entity_effect_has_pending(s.actor) &&
        mc_entity_effect_cleanup(s.actor, NULL) != MC_ENTITY_OK)
        return false;
    if (contexts_ready && (mc_entity_tick_execution_retire(execution) != MC_WORLD_STORAGE_OK ||
                           mc_math_execution_retire(&math) != MC_MATH_CLASS_OK))
        return false;
    if (mc_math_classes_release(&math_owner, NULL) != MC_MATH_CLASS_OK)
        return false;
    if (observation.original_chunk_world != NULL)
        observation.chunk->d = observation.original_chunk_world;
    if (observation.receipt != NULL &&
        mc_world_access_receipt_retire(&observation.receipt, NULL) != MC_WORLD_STORAGE_OK)
        return false;
    if (registered && mc_world_access_detach_occurrence(s.world->worlds[0], listener, occurrence,
                                                        NULL) != MC_WORLD_STORAGE_OK)
        return false;
    if (listener != NULL && mc_world_access_free(listener, NULL) != MC_WORLD_STORAGE_OK)
        return false;
    if (!sw_retire(&s))
        return false;
    free(execution);
    free(damage);
    free(error);
    if (damage_entered)
        puts("OWNED_WAKE_RECURSION_CLEANUP complete=1");
    return ok;
}

typedef struct dw_drop_sample {
    uint64_t position[3];
    int32_t id, count, damage, delay;
} dw_drop_sample;

typedef struct dw_drop_observation {
    dw_drop_sample items[8];
    size_t count;
    bool valid;
} dw_drop_observation;

static mc_world_storage_status dw_drop_added(void *context, mc_world_runtime *world,
                                             mc_entity *item, mc_world_storage_error *error,
                                             mc_world_access_failure *failure) {
    (void)failure;
    dw_drop_observation *o = context;
    mc_item_stack_fields stack = {0};
    o->valid &= o->count < COUNT(o->items) && item != NULL && item->base.world == world &&
                item->base.bF && item->item.c == 10 &&
                mc_item_stack_fields_get(item->stack, &stack) == MC_ITEM_STACK_OK;
    if (o->valid)
        o->items[o->count++] = (dw_drop_sample){{movement_double_bits(item->base.x),
                                                 movement_double_bits(item->base.y),
                                                 movement_double_bits(item->base.z)},
                                                stack.id,
                                                stack.count,
                                                stack.damage,
                                                item->item.c};
    *error = (mc_world_storage_error){0};
    return o->valid ? MC_WORLD_STORAGE_OK : MC_WORLD_STORAGE_UNSUPPORTED_STATE;
}

/* This compares two real entries to the same Block bodies. It checks owned
 * frame reuse across multiple Items and the crop-specific constructor order;
 * it is a native ownership control, not an independent original comparison. */
static bool dw_block_drops(const char *root) {
    bool ok = false;
    sw_fixture s = {0};
    const int32_t input[5] = {0, 0, 0, 0, 0};
    mc_math_classes *math_owner = NULL;
    mc_math_execution math = {0};
    mc_entity_tick_execution *execution = calloc(1, sizeof(*execution));
    mc_world_runtime_execution_result *result = calloc(1, sizeof(*result));
    mc_world_mutation_error *error = calloc(1, sizeof(*error));
    mc_world_mutation_drop_execution *drop = NULL;
    mc_world_access *listener = NULL;
    uint64_t occurrence = 0;
    bool scope_open = false, registered = false, math_ready = false;
    mc_entity_tick_execution_world_token outer = {0};
    dw_drop_observation observation = {.valid = true};
    CHECK(execution != NULL && result != NULL && error != NULL && sw_prepare(&s, root, input) &&
          mc_math_classes_create(SIZE_MAX, &math_owner, NULL) == MC_MATH_CLASS_OK);
    mc_world_runtime *world = s.world->worlds[0];
    const mc_world_access_source_callbacks callbacks = {.entity_added = dw_drop_added};
    CHECK(mc_world_access_source_create(&callbacks, &observation, &listener, NULL) ==
              MC_WORLD_STORAGE_OK &&
          mc_world_access_add_receipted(world, listener, &occurrence, NULL) == MC_WORLD_STORAGE_OK);
    registered = true;
    mc_math_execution_init(&math, math_owner);
    math_ready = true;
    mc_entity_tick_execution_init(execution, &math);
    CHECK(mc_entity_tick_execution_push_world(
              execution, world, (mc_entity_tick_execution_site){"test", "drop", "()V", 0},
              &outer) == MC_WORLD_STORAGE_OK);
    scope_open = true;
    const mc_world_runtime_execution_code code = {{"lm", "e", "()V", 6}, {"lm", "e", "()V", 33}};
    mc_registry_block_view blocks;
    CHECK(mc_registry_classes_inspect_blocks(s.world->registry[0], &blocks, NULL) ==
          MC_REGISTRY_OK);
    const int32_t ids[] = {26, 43, 82, 59};
    const int32_t metadata[] = {0, 2, 0, 7};
    for (size_t profile = 0; profile < COUNT(ids); ++profile) {
        dw_drop_observation legacy = {0};
        uint64_t seed = 0;
        for (size_t owned = 0; owned < 2; ++owned) {
            observation = (dw_drop_observation){.valid = true};
            mc_java_random_reseed(world->fields.r, 2468);
            *error = (mc_world_mutation_error){0};
            if (owned == 0) {
                CHECK(mc_world_mutation_drop(world, blocks.m[ids[profile]], 7, 64, 8,
                                             metadata[profile], error));
                legacy = observation;
                seed = world->fields.r->state;
            } else {
                mc_world_runtime_execution_result_init(result);
                const mc_world_mutation_execution_context context = {execution, s.world->classes[0],
                                                                     code, result, outer};
                mc_world_storage_status raw = MC_WORLD_STORAGE_BUSY;
                CHECK(mc_world_mutation_drop_in_execution(&context, world, blocks.m[ids[profile]],
                                                          7, 64, 8, metadata[profile], &drop, error,
                                                          &raw) &&
                      raw == MC_WORLD_STORAGE_OK && drop != NULL && drop->completed &&
                      !drop->active && drop->entity == NULL && drop->stack == NULL &&
                      drop->constructor_scope.consumed && drop->publication_scope.consumed &&
                      mc_world_mutation_drop_result_quiet(result) && observation.valid &&
                      observation.count == legacy.count && world->fields.r->state == seed);
                for (size_t i = 0; i < observation.count; ++i) {
                    const dw_drop_sample *a = &observation.items[i], *b = &legacy.items[i];
                    CHECK(memcmp(a->position, b->position, sizeof(a->position)) == 0 &&
                          a->id == b->id && a->count == b->count && a->damage == b->damage &&
                          a->delay == b->delay);
                }
                CHECK((ids[profile] != 59 || observation.count > 1) &&
                      observation.count == (ids[profile] == 43   ? 2u
                                            : ids[profile] == 82 ? 4u
                                            : ids[profile] == 26 ? 1u
                                                                 : legacy.count) &&
                      observation.count != 0 &&
                      mc_world_mutation_drop_retire(&drop) == MC_WORLD_STORAGE_OK && drop == NULL);
            }
        }
    }
    ok = true;
cleanup:
    if (drop != NULL && mc_world_mutation_drop_retire(&drop) != MC_WORLD_STORAGE_OK)
        return false;
    if (scope_open &&
        (mc_entity_tick_execution_pop_world(execution, &outer, false) != MC_WORLD_STORAGE_OK ||
         mc_entity_tick_execution_retire(execution) != MC_WORLD_STORAGE_OK))
        return false;
    if (math_ready && mc_math_execution_retire(&math) != MC_MATH_CLASS_OK)
        return false;
    if (math_owner != NULL) {
        mc_math_class_state state = {0};
        /* Release the execution's selected occurrence before the fixture's
         * final one. Releasing only the latter hides a leaked table on hosts
         * without leak detection. */
        if (mc_math_classes_state(math_owner, &state, NULL) != MC_MATH_CLASS_OK ||
            state.holds != 1 || mc_math_classes_release(&math_owner, NULL) != MC_MATH_CLASS_OK ||
            math_owner != NULL)
            return false;
    }
    if (registered && mc_world_access_detach_occurrence(s.world->worlds[0], listener, occurrence,
                                                        NULL) != MC_WORLD_STORAGE_OK)
        return false;
    if (listener != NULL && mc_world_access_free(listener, NULL) != MC_WORLD_STORAGE_OK)
        return false;
    if (s.world != NULL && !sw_retire(&s))
        return false;
    free(execution);
    free(result);
    free(error);
    return ok;
}

int main(int argc, char **argv) {
    int result = 1;
    char root[8192];
    if (argc != 3 || !decode_path(argv[2], root, sizeof(root)) || !mc_math_trig_init(&trig))
        return 2;
    if (strcmp(argv[1], "owned-block-drops") == 0) {
        if (!dw_block_drops(root))
            return 1;
        return printf("PASS: %u owned Block drop checks\n", checks) < 0;
    }
    const char *const names[] = {"owned-wake-recursion",
                                 "owned-wake-null-exit",
                                 "owned-wake-observer-error",
                                 "owned-wake-scope-drift",
                                 "owned-wake-null-chunk-world",
                                 "owned-wake-foot",
                                 "owned-wake-foot-source",
                                 "owned-wake-foot-scope",
                                 "owned-wake-foot-math-missing",
                                 "owned-wake-ladder",
                                 "owned-wake-ladder-source",
                                 "owned-wake-ladder-scope",
                                 "owned-wake-ladder-math-missing",
                                 "owned-wake-ladder-removal-source",
                                 "owned-wake-ladder-supported",
                                 "owned-wake-torch",
                                 "owned-wake-torch-source",
                                 "owned-wake-torch-scope",
                                 "owned-wake-torch-math-missing",
                                 "owned-wake-torch-removal-source",
                                 "owned-wake-torch-supported",
                                 "owned-wake-torch-early",
                                 "owned-wake-torch-early-source",
                                 "owned-wake-torch-early-removal-source",
                                 "owned-wake-flower",
                                 "owned-wake-flower-source",
                                 "owned-wake-flower-scope",
                                 "owned-wake-flower-math-missing",
                                 "owned-wake-flower-removal-source",
                                 "owned-wake-flower-supported",
                                 "owned-wake-flower-sky-supported",
                                 "owned-wake-flower-dark",
                                 "owned-wake-sapling",
                                 "owned-wake-sapling-source",
                                 "owned-wake-sapling-scope",
                                 "owned-wake-sapling-math-missing",
                                 "owned-wake-sapling-removal-source",
                                 "owned-wake-sapling-supported",
                                 "owned-wake-tall-grass",
                                 "owned-wake-tall-grass-source",
                                 "owned-wake-tall-grass-scope",
                                 "owned-wake-tall-grass-math-missing",
                                 "owned-wake-tall-grass-removal-source",
                                 "owned-wake-tall-grass-supported",
                                 "owned-wake-tall-grass-seed1234",
                                 "owned-wake-tall-grass-no-item",
                                 "owned-wake-tall-grass-no-item-math-missing"};
    const dw_recursive_mode modes[] = {DW_RECURSIVE_READY,
                                       DW_RECURSIVE_OWNED_NULL,
                                       DW_RECURSIVE_OBSERVER_ERROR,
                                       DW_RECURSIVE_SCOPE_DRIFT,
                                       DW_RECURSIVE_NULL_CHUNK_WORLD,
                                       DW_RECURSIVE_FOOT,
                                       DW_RECURSIVE_FOOT_SOURCE,
                                       DW_RECURSIVE_FOOT_SCOPE,
                                       DW_RECURSIVE_FOOT_MATH_MISSING,
                                       DW_RECURSIVE_LADDER,
                                       DW_RECURSIVE_LADDER_SOURCE,
                                       DW_RECURSIVE_LADDER_SCOPE,
                                       DW_RECURSIVE_LADDER_MATH_MISSING,
                                       DW_RECURSIVE_LADDER_REMOVAL_SOURCE,
                                       DW_RECURSIVE_LADDER_SUPPORTED,
                                       DW_RECURSIVE_TORCH,
                                       DW_RECURSIVE_TORCH_SOURCE,
                                       DW_RECURSIVE_TORCH_SCOPE,
                                       DW_RECURSIVE_TORCH_MATH_MISSING,
                                       DW_RECURSIVE_TORCH_REMOVAL_SOURCE,
                                       DW_RECURSIVE_TORCH_SUPPORTED,
                                       DW_RECURSIVE_TORCH_EARLY,
                                       DW_RECURSIVE_TORCH_EARLY_SOURCE,
                                       DW_RECURSIVE_TORCH_EARLY_REMOVAL_SOURCE,
                                       DW_RECURSIVE_FLOWER,
                                       DW_RECURSIVE_FLOWER_SOURCE,
                                       DW_RECURSIVE_FLOWER_SCOPE,
                                       DW_RECURSIVE_FLOWER_MATH_MISSING,
                                       DW_RECURSIVE_FLOWER_REMOVAL_SOURCE,
                                       DW_RECURSIVE_FLOWER_SUPPORTED,
                                       DW_RECURSIVE_FLOWER_SKY_SUPPORTED,
                                       DW_RECURSIVE_FLOWER_DARK,
                                       DW_RECURSIVE_SAPLING,
                                       DW_RECURSIVE_SAPLING_SOURCE,
                                       DW_RECURSIVE_SAPLING_SCOPE,
                                       DW_RECURSIVE_SAPLING_MATH_MISSING,
                                       DW_RECURSIVE_SAPLING_REMOVAL_SOURCE,
                                       DW_RECURSIVE_SAPLING_SUPPORTED,
                                       DW_RECURSIVE_TALL_GRASS,
                                       DW_RECURSIVE_TALL_GRASS_SOURCE,
                                       DW_RECURSIVE_TALL_GRASS_SCOPE,
                                       DW_RECURSIVE_TALL_GRASS_MATH_MISSING,
                                       DW_RECURSIVE_TALL_GRASS_REMOVAL_SOURCE,
                                       DW_RECURSIVE_TALL_GRASS_SUPPORTED,
                                       DW_RECURSIVE_TALL_GRASS_SEED1234,
                                       DW_RECURSIVE_TALL_GRASS_NO_ITEM,
                                       DW_RECURSIVE_TALL_GRASS_NO_ITEM_MATH_MISSING};
    _Static_assert(COUNT(names) == COUNT(modes), "Wake mode table sizes must agree");
    for (size_t i = 0; i < COUNT(names); ++i)
        if (strcmp(argv[1], names[i]) == 0) {
            if (!dw_recursive(root, modes[i]))
                return 1;
            return printf("PASS: %u owned Player wake recursion checks\n", checks) < 0;
        }
    if (strcmp(argv[1], "damage-wake") != 0)
        return 2;
    for (int32_t direction = 0; direction < 4; ++direction)
        CHECK(dw_zero(root, direction));
    for (int32_t mode = 0; mode <= 15; ++mode)
        CHECK(dw_control(root, mode));
    CHECK(dw_recursive(root, DW_RECURSIVE_LEGACY_NULL));
    result = 0;
cleanup:
    if (result == 0 && printf("PASS: %u Player damage wake checks\n", checks) < 0)
        return 1;
    return result;
}
