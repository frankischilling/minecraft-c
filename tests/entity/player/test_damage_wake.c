/* Actual Player damage over the existing genuine World/Bed/sleep fixture.
 * Imported test entry points are never executed. */
#define MC_SLEEP_WAKE_ENTRY mc_damage_wake_unused_sleep_main
#include "test_sleep_wake.c"
#undef MC_SLEEP_WAKE_ENTRY

#include "../../../src/entity/tick/tick_internal.h"
#include "entity/movement/damage_execution_internal.h"
#include "mc/entity/movement/effects.h"
#include "world/chunk/object_state_internal.h"
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
} dw_recursive_mode;

typedef struct dw_recursive_cell_sample {
    int32_t x, y, z;
    uint8_t head_id, head_metadata, orphan_id, orphan_metadata;
    bool sleeping;
} dw_recursive_cell_sample;

typedef struct dw_recursive_observation {
    sw_fixture *fixture;
    dw_recursive_cell_sample samples[2];
    size_t cells;
    bool order_valid, external_open;
    dw_recursive_mode mode;
    mc_entity_tick_execution *execution;
    mc_entity_tick_execution_world_token external;
    mc_world_access *listener;
    mc_world_access_receipt *receipt;
    mc_world_chunk *chunk;
    mc_world_runtime *original_chunk_world;
} dw_recursive_observation;

static mc_world_storage_status dw_recursive_cell(void *context, mc_world_runtime *world, int32_t x,
                                                 int32_t y, int32_t z,
                                                 mc_world_storage_error *error,
                                                 mc_world_access_failure *failure) {
    dw_recursive_observation *o = context;
    const sw_fixture *s = o->fixture;
    const mc_chunk_data *data = s->world->states[0]->data;
    const mc_player_fields *player = &s->actor->local_player->fields.player;
    const bool head = o->cells == 0;
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
                      y == 64 && z == 8 && orphan == (head ? 26 : 0) && read && head_id == 26 &&
                      metadata == 8 && orphan_metadata == (head ? 8 : 0) && player->u &&
                      player->private_a == 37 && world->fields.J;
    ++o->cells;
    *error = (mc_world_storage_error){0};
    if (head) {
        if (o->mode == DW_RECURSIVE_NULL_CHUNK_WORLD)
            o->chunk->d = NULL;
        return MC_WORLD_STORAGE_OK;
    }
    if (o->mode == DW_RECURSIVE_LEGACY_NULL || o->mode == DW_RECURSIVE_OWNED_NULL)
        s->actor->base.world = NULL;
    if (o->mode == DW_RECURSIVE_OBSERVER_ERROR) {
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
    mc_math_execution math;
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
    CHECK(sw_cell(data, 7, 64, 8, 26, 8) && data->blocks[boxes_index(7, 64, 7)] == 0);
    if (mode == DW_RECURSIVE_NULL_CHUNK_WORLD) {
        observation.chunk = mc_world_store_chunk_object(world->options.store, f->states[0]);
        CHECK(observation.chunk != NULL && observation.chunk->d == world);
        observation.original_chunk_world = observation.chunk->d;
    }
    const mc_world_access_source_callbacks callbacks = {.cell = dw_recursive_cell};
    CHECK(mc_world_access_source_create(&callbacks, &observation, &listener, NULL) ==
          MC_WORLD_STORAGE_OK);
    observation.listener = listener;
    CHECK(mc_world_access_add_receipted(world, listener, &occurrence, NULL) == MC_WORLD_STORAGE_OK);
    registered = true;
    const size_t holds = p->holds, entities = world->fields.b->count;
    const uint64_t random = p->random->state;
    p->local_player->fields.living.av = 99;
    mc_entity_status raw = MC_ENTITY_BUSY;
    bool accepted = true, continued = false;
    mc_entity_effect_error_init(error);
    if (legacy) {
        raw = mc_entity_effect_damage_in_context(p, f->registry[0], NULL, 0, &accepted, error);
    } else {
        mc_math_execution_init(&math, NULL);
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
    }
    if (legacy && s.actor != NULL && mc_entity_effect_has_pending(s.actor) &&
        mc_entity_effect_cleanup(s.actor, NULL) != MC_ENTITY_OK)
        return false;
    if (contexts_ready && (mc_entity_tick_execution_retire(execution) != MC_WORLD_STORAGE_OK ||
                           mc_math_execution_retire(&math) != MC_MATH_CLASS_OK))
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

int main(int argc, char **argv) {
    int result = 1;
    char root[8192];
    if (argc != 3 || !decode_path(argv[2], root, sizeof(root)) || !mc_math_trig_init(&trig))
        return 2;
    const char *const names[] = {"owned-wake-recursion", "owned-wake-null-exit",
                                 "owned-wake-observer-error", "owned-wake-scope-drift",
                                 "owned-wake-null-chunk-world"};
    const dw_recursive_mode modes[] = {DW_RECURSIVE_READY, DW_RECURSIVE_OWNED_NULL,
                                       DW_RECURSIVE_OBSERVER_ERROR, DW_RECURSIVE_SCOPE_DRIFT,
                                       DW_RECURSIVE_NULL_CHUNK_WORLD};
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
