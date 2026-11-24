#include "nightmare_internal.h"

#include "../../block/registry/registry_internal.h"
#include "../../block/registry/runtime_internal.h"
#include "../../entity/construction_internal.h"
#include "../../entity/entity_internal.h"
#include "../../entity/mob/internal.h"
#include "../../entity/mob/spawn_failure_internal.h"
#include "../../entity/tick/tick_internal.h"
#include "../../item/render/internal.h"
#include "../path/finder_internal.h"
#include "block_queries_internal.h"
#include "construction_internal.h"
#include "entity_tick_math_internal.h"
#include "mc/core/java/java.h"
#include "mc/core/math/math.h"
#include "mc/entity/mob/construction.h"
#include "mc/entity/mob/spawning.h"
#include "mc/world/runtime/entities.h"
#include "mc/world/storage/source_chunk_loader.h"
#include "runtime_internal.h"
#include "spawn_postspawn_internal.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

struct mc_world_nightmare_call {
    mc_world_nightmare_state state;
    mc_world_nightmare_error error;
    mc_world_nightmare_cleanup cleanup;
    mc_world_nightmare_context context;
    mc_world_nightmare_limits limits;
    mc_world_nightmare_child current_child;
    unsigned int current_child_pc;
    mc_world_nightmare_class_array *array;
    mc_world_nightmare_class_error class_error;
    mc_path_budget budget;
    mc_path_finder *finder;
    mc_path_finder_state finder_state;
    mc_path_error path_error;
    mc_world_solid_support_detail support;
    mc_world_material_error material;
    mc_math_class_error math;
    mc_registry_error registry_error;
    mc_mob_error mob;
    mc_entity_position_rotation_error position;
    mc_mob_spawn_state eligibility;
    mc_mob_spawn_error eligibility_error;
    mc_world_source_chunk_error eligibility_source;
    mc_world_bed_spawn_error bed;
    mc_player_coordinates *exit, *candidate_exit;
    mc_world_entity_error publication;
    mc_world_spawn_postspawn_work postspawn;
    mc_player_sleep_call *wake;
    mc_player_sleep_state wake_state;
    mc_player_sleep_error wake_admission;
    mc_player_sleep_cleanup wake_cleanup;
    mc_entity_tick_error ambient;
    mc_registry_classes *registry;
    mc_math_classes *held_math;
    mc_world_runtime *worlds[3];
    size_t world_count;
    mc_world_access_failure receipts[16], pending_receipt;
    size_t receipt_count;
    bool classes_held, registry_held, failed, bed_owned, wake_owned;
    bool eligibility_source_copied, eligibility_capture_pending;
};

static mc_world_storage_status class_failed(mc_world_nightmare_namespace *n,
                                            mc_world_storage_status status, const char *operation) {
    n->phase = MC_WORLD_NIGHTMARE_CLASS_FAILED;
    n->failure.status = status;
    n->failure.operation = operation;
    n->failure.pc = n->pc;
    return status;
}

mc_world_storage_status mc_world_nightmare_classes_ensure(mc_world_runtime_classes *classes,
                                                          mc_world_nightmare_class_error *out) {
    mc_world_nightmare_class_error admission = {0};
    if (classes == NULL || classes->side != MC_WORLD_INFO_CLIENT) {
        admission.status = MC_WORLD_STORAGE_INVALID_ARGUMENT;
        admission.operation = "CQ defining client class namespace";
        if (out != NULL)
            *out = admission;
        return admission.status;
    }
    mc_world_nightmare_namespace *n = &classes->nightmare;
    if (n->phase == MC_WORLD_NIGHTMARE_CLASS_READY) {
        if (out != NULL)
            *out = admission;
        return MC_WORLD_STORAGE_OK;
    }
    if (n->phase == MC_WORLD_NIGHTMARE_CLASS_ENTERED) {
        admission.status = MC_WORLD_STORAGE_BUSY;
        admission.operation = "CQ native class initialization remains entered";
        admission.pc = n->pc;
        if (out != NULL)
            *out = admission;
        return admission.status;
    }
    if (n->phase == MC_WORLD_NIGHTMARE_CLASS_FAILED) {
        if (out != NULL)
            *out = n->failure;
        return n->failure.status;
    }
    n->phase = MC_WORLD_NIGHTMARE_CLASS_ENTERED;
    n->pc = 4;
    if (classes->spawning_chunks.order == NULL) {
        n->failure.set = mc_java_int_set_create(NULL, &classes->spawning_chunks.order);
        if (n->failure.set != MC_JAVA_INT_SET_OK) {
            (void)class_failed(n,
                               n->failure.set == MC_JAVA_INT_SET_ALLOCATION_FAILED
                                   ? MC_WORLD_STORAGE_ALLOCATION_FAILED
                                   : MC_WORLD_STORAGE_LIMIT,
                               "CQ actual HashSet constructor native storage");
            goto failed;
        }
    }
    n->pc = 7; /* The same actual b is used by the ordinary spawning body. */
    n->pc = 11;
    n->default_array = (mc_world_nightmare_class_array){3, n->defaults};
    n->candidate = &n->default_array;
    static const mc_entity_class_kind kinds[3] = {
        MC_ENTITY_CLASS_KIND_SPIDER, MC_ENTITY_CLASS_KIND_ZOMBIE, MC_ENTITY_CLASS_KIND_SKELETON};
    static const unsigned int pcs[3] = {16, 21, 26};
    for (size_t i = 0; i < 3; ++i) {
        n->pc = pcs[i];
        const mc_entity_class_literal *literal = NULL;
        n->failure.entity = (int)mc_entity_class_literal_get(classes->entities, kinds[i], &literal);
        if (n->failure.entity != MC_ENTITY_OK) {
            (void)class_failed(n,
                               n->failure.entity == MC_ENTITY_LIMIT
                                   ? MC_WORLD_STORAGE_LIMIT
                                   : MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                               "CQ actual canonical Mob Class literal");
            goto failed;
        }
        n->pc = pcs[i] + 2;
        n->defaults[i] = (mc_world_nightmare_class_value){.kind = MC_WORLD_NIGHTMARE_CLASS_ENTITY,
                                                          .value.entity = literal};
    }
    n->pc = 29;
    n->a = n->candidate;
    n->candidate = NULL;
    classes->spawning_initialized = true;
    n->pc = 32;
    n->phase = MC_WORLD_NIGHTMARE_CLASS_READY;
    n->failure = (mc_world_nightmare_class_error){0};
    if (out != NULL)
        *out = admission;
    return MC_WORLD_STORAGE_OK;
failed:
    if (out != NULL)
        *out = n->failure;
    return n->failure.status;
}

mc_world_nightmare_limits mc_world_nightmare_default_limits(void) {
    mc_world_path_limits path = mc_world_path_default_limits();
    /* A CQ invocation shares FW across up to20 attempts per captured Player.
     * These cumulative native ceilings cover the qualified two-Player scope;
     * byte/allocation budgets and explicit caller limits remain unchanged. */
    path.max_points = 131072u;
    path.max_expanded_points = 131072u;
    return (mc_world_nightmare_limits){1048576u, path, {MC_WORLD_BED_SPAWN_DEFAULT_LOOP_WORK}};
}

static mc_world_storage_status native_status(mc_entity_status status) {
    return status == MC_ENTITY_OK                  ? MC_WORLD_STORAGE_OK
           : status == MC_ENTITY_LIMIT             ? MC_WORLD_STORAGE_LIMIT
           : status == MC_ENTITY_ALLOCATION_FAILED ? MC_WORLD_STORAGE_ALLOCATION_FAILED
           : status == MC_ENTITY_BUSY              ? MC_WORLD_STORAGE_BUSY
                                                   : MC_WORLD_STORAGE_UNSUPPORTED_STATE;
}

static mc_world_storage_status path_status(mc_path_status status) {
    return status == MC_PATH_OK           ? MC_WORLD_STORAGE_OK
           : status == MC_PATH_LIMIT      ? MC_WORLD_STORAGE_LIMIT
           : status == MC_PATH_ALLOCATION ? MC_WORLD_STORAGE_ALLOCATION_FAILED
           : status == MC_PATH_BUSY       ? MC_WORLD_STORAGE_BUSY
                                          : MC_WORLD_STORAGE_UNSUPPORTED_STATE;
}

static mc_world_storage_status registry_status(mc_registry_status status) {
    return status == MC_REGISTRY_OK                  ? MC_WORLD_STORAGE_OK
           : status == MC_REGISTRY_LIMIT             ? MC_WORLD_STORAGE_LIMIT
           : status == MC_REGISTRY_ALLOCATION_FAILED ? MC_WORLD_STORAGE_ALLOCATION_FAILED
           : status == MC_REGISTRY_BUSY              ? MC_WORLD_STORAGE_BUSY
                                                     : MC_WORLD_STORAGE_UNSUPPORTED_STATE;
}

static void at(mc_world_nightmare_call *c, unsigned int pc, mc_world_nightmare_child child,
               unsigned int child_pc) {
    c->state.source_pc = pc;
    c->current_child = child;
    c->current_child_pc = child_pc;
}

static bool is_error(const char *name) {
    if (name == NULL)
        return false;
    const size_t length = strlen(name);
    return length >= 5 && strcmp(name + length - 5, "Error") == 0;
}

static bool fail(mc_world_nightmare_call *c, mc_world_storage_status status, const char *operation,
                 bool native, const char *exception) {
    if (!c->failed) {
        c->failed = true;
        c->error.status = status;
        c->error.operation = operation;
        c->error.native_policy = native;
        c->error.exception_class = exception;
        c->error.source_error = !native && is_error(exception);
        c->error.source_exception = !native && exception != NULL && !c->error.source_error;
        c->error.pc = c->state.source_pc;
        c->error.child = c->current_child;
        c->error.child_pc = c->current_child_pc;
        c->error.cause = (mc_world_storage_error){status, operation, 0};
    }
    return false;
}

static bool source_null(mc_world_nightmare_call *c, const char *operation) {
    return fail(c, MC_WORLD_STORAGE_UNSUPPORTED_STATE, operation, false,
                "java/lang/NullPointerException");
}

static bool work(mc_world_nightmare_call *c) {
    if (c->state.work == c->limits.max_work)
        return fail(c, MC_WORLD_STORAGE_LIMIT, "CQ pending loop work ceiling", true, NULL);
    ++c->state.work;
    return true;
}

static bool fp(mc_world_nightmare_call *c) {
    return (_mm_getcsr() & 0xffc0u) == 0x1f80u ||
           fail(c, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "CQ reached strict floating environment",
                true, NULL);
}

static bool capture(mc_world_nightmare_call *c, const mc_world_access_failure *value) {
    if (value->receipt == NULL)
        return true;
    for (size_t i = 0; i < c->receipt_count; ++i)
        if (c->receipts[i].receipt == value->receipt)
            return true;
    if (c->receipt_count == sizeof(c->receipts) / sizeof(*c->receipts)) {
        c->pending_receipt = *value;
        return false;
    }
    const mc_world_storage_status status =
        mc_world_access_failure_retain(value, &c->receipts[c->receipt_count]);
    if (status != MC_WORLD_STORAGE_OK) {
        c->pending_receipt = *value;
        return false;
    }
    ++c->receipt_count;
    return true;
}

static void capture_nearby(mc_world_nightmare_call *c, const mc_world_nearby_error *e) {
    if (!capture(c, &e->batch.publication.world_access_failure) ||
        !capture(c, &e->tile_entity.world_access_failure) ||
        !capture(c, &e->tile_entity.publication.world_access_failure))
        return;
    (void)capture(c, &e->tile_entity.batch.publication.world_access_failure);
}

static void registry_exception(mc_world_nightmare_call *c, const mc_registry_error *error) {
    if (error->status == MC_REGISTRY_OK)
        return;
    mc_item_render_throwable throwable;
    const char *exception = NULL;
    mc_item_render_registry_classification(c->registry, error, &throwable, &exception);
    if (throwable != MC_ITEM_RENDER_NO_THROWABLE) {
        c->error.native_policy = false;
        c->error.exception_class = exception;
        c->error.source_exception = throwable == MC_ITEM_RENDER_SOURCE_EXCEPTION;
        c->error.source_error = throwable == MC_ITEM_RENDER_SOURCE_ERROR;
    }
}

static bool capture_eligibility(mc_world_nightmare_call *c) {
    const mc_mob_spawn_failure_view view = mc_mob_spawn_error_project(&c->eligibility_error);
    const mc_world_source_chunk_error **source = view.query != NULL ? &view.query->source_chunk
                                                 : view.acquisition != NULL
                                                     ? &view.acquisition->source_chunk
                                                     : NULL;
    if (source != NULL && *source != NULL && !c->eligibility_source_copied) {
        /* This passive copy is part of the already admitted call owner. No
         * post-failure request allocation or second loader operation occurs. */
        c->eligibility_source = **source;
        c->eligibility_source_copied = true;
        *source = &c->eligibility_source;
    }
    const mc_world_entity_batch_error *batch = NULL;
    const mc_live_tile_entity_error *tile = NULL;
    if (view.nearby != NULL) {
        batch = &view.nearby->batch;
        tile = &view.nearby->tile_entity;
    } else if (view.acquisition != NULL) {
        batch = &view.acquisition->batch;
        tile = &view.acquisition->tile_entity;
    }
    c->eligibility_capture_pending =
        batch != NULL && (!capture(c, &batch->publication.world_access_failure) ||
                          !capture(c, &tile->world_access_failure) ||
                          !capture(c, &tile->publication.world_access_failure) ||
                          !capture(c, &tile->batch.publication.world_access_failure));
    return !c->eligibility_capture_pending;
}

static void eligibility_receipt_exception(mc_world_nightmare_call *c,
                                          const mc_world_access_failure *receipt,
                                          mc_world_storage_status status, const char *operation) {
    const char *exception = NULL;
    if (status == c->error.status && operation == c->error.operation &&
        mc_world_access_failure_source_exception(receipt, &exception)) {
        c->error.native_policy = false;
        c->error.exception_class = exception;
        c->error.source_error = is_error(exception);
        c->error.source_exception = !c->error.source_error;
    }
}

static bool eligibility_failed(mc_world_nightmare_call *c, mc_world_storage_status status) {
    if (c->failed)
        return false;
    mc_mob_spawn_error *e = &c->eligibility_error;
    c->error.eligibility = e;
    (void)fail(c, status, e->operation, e->native_policy, e->exception_class);
    c->error.child_pc = e->site.pc < 0 ? 0 : (unsigned int)e->site.pc;
    const mc_mob_spawn_failure_view view = mc_mob_spawn_error_project(e);
    if (view.cause != NULL && view.cause->status == status && view.cause->operation == e->operation)
        c->error.cause = *view.cause;
    if (e->registry.status != MC_REGISTRY_OK && e->registry.message == e->operation &&
        registry_status(e->registry.status) == status)
        registry_exception(c, &e->registry);
    if (view.registry != NULL && registry_status(view.registry->status) == status)
        registry_exception(c, view.registry);
    (void)capture_eligibility(c);
    if (c->eligibility_source_copied && c->eligibility_source.source.status != MC_WORLD_SOURCE_OK &&
        c->eligibility_source.source.source_exception &&
        !c->eligibility_source.source.native_policy &&
        c->eligibility_source.source.exception_class != NULL) {
        c->error.native_policy = false;
        c->error.exception_class = c->eligibility_source.source.exception_class;
        c->error.source_error = is_error(c->error.exception_class);
        c->error.source_exception = !c->error.source_error;
    }
    const mc_world_entity_batch_error *batch = NULL;
    const mc_live_tile_entity_error *tile = NULL;
    if (view.nearby != NULL) {
        batch = &view.nearby->batch;
        tile = &view.nearby->tile_entity;
    } else if (view.acquisition != NULL) {
        batch = &view.acquisition->batch;
        tile = &view.acquisition->tile_entity;
    }
    if (batch != NULL) {
        eligibility_receipt_exception(c, &batch->publication.world_access_failure,
                                      batch->publication.status, batch->publication.operation);
        eligibility_receipt_exception(c, &tile->world_access_failure, tile->status,
                                      tile->operation);
        eligibility_receipt_exception(c, &tile->publication.world_access_failure,
                                      tile->publication.status, tile->publication.operation);
        eligibility_receipt_exception(c, &tile->batch.publication.world_access_failure,
                                      tile->batch.publication.status,
                                      tile->batch.publication.operation);
    }
    return false;
}

static bool floor_value(mc_world_nightmare_call *c, double value, unsigned int pc, int32_t *out) {
    at(c, pc, MC_WORLD_NIGHTMARE_MATH, 0);
    if (c->context.math == NULL)
        return fail(c, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "CQ defining Math namespace", true,
                    NULL);
    const mc_math_class_status status = mc_math_classes_ensure(c->context.math, &c->math);
    if (status != MC_MATH_CLASS_OK) {
        c->error.math = &c->math;
        return fail(c,
                    status == MC_MATH_CLASS_LIMIT               ? MC_WORLD_STORAGE_LIMIT
                    : status == MC_MATH_CLASS_ALLOCATION_FAILED ? MC_WORLD_STORAGE_ALLOCATION_FAILED
                    : status == MC_MATH_CLASS_BUSY              ? MC_WORLD_STORAGE_BUSY
                                                   : MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                    c->math.operation, true, NULL);
    }
    if (mc_math_classes_table(c->context.math) != c->context.code_classes->trig)
        return fail(c, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "CQ actual Math table binding", true,
                    NULL);
    if (!fp(c))
        return false;
    *out = mc_math_floor_double(value);
    return true;
}

static bool random_int(mc_world_nightmare_call *c, unsigned int pc, int32_t bound, int32_t *out) {
    at(c, pc == 200 ? 194 : pc - 5, MC_WORLD_NIGHTMARE_NONE, 0);
    if (c->state.world == NULL)
        return source_null(c, "CQ null World Random field receiver");
    mc_java_random *random = c->state.world->fields.r;
    at(c, pc, MC_WORLD_NIGHTMARE_NONE, 0);
    if (random == NULL)
        return source_null(c, "CQ null actual World Random receiver");
    if (!mc_java_random_next_bounded(random, bound, out))
        return fail(c, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "CQ nonpositive Random bound", false,
                    "java/lang/IllegalArgumentException");
    ++c->state.random_calls;
    return true;
}

static bool list_size(mc_world_nightmare_call *c, unsigned int pc, size_t *out) {
    at(c, pc, MC_WORLD_NIGHTMARE_LIST, 0);
    const mc_world_reference_list *list = c->state.players;
    if (list == NULL)
        return source_null(c, "CQ null captured Player List");
    mc_world_runtime *owner = c->context.list_owner;
    if (list->count == 0 && list->capacity == 0 && list->items == NULL &&
        (owner == NULL || list != owner->player_membership.list)) {
        *out = 0;
        return true;
    }
    if (owner == NULL) {
        return fail(c, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "CQ typed Player List owner", true,
                    NULL);
    }
    const mc_world_entity_list_ledger *ledger = &owner->player_membership;
    if (list != ledger->list || list->count != ledger->count ||
        list->capacity != ledger->capacity || list->count > list->capacity ||
        list->capacity > (size_t)INT32_MAX || ((list->capacity == 0) != (list->items == NULL)) ||
        ((ledger->capacity == 0) != (ledger->entries == NULL)))
        return fail(c, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "CQ captured Player List provenance",
                    true, NULL);
    *out = list->count;
    return true;
}

static bool next_player(mc_world_nightmare_call *c) {
    size_t count;
    if (!list_size(c, 31, &count))
        return false;
    if (c->state.players->mod_count != c->state.expected_mod_count)
        return fail(c, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "CQ Iterator.next changed List", false,
                    "java/util/ConcurrentModificationException");
    if (c->state.cursor >= count)
        return fail(c, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "CQ Iterator.next exhausted List", false,
                    "java/util/NoSuchElementException");
    mc_world_runtime *owner = c->context.list_owner;
    const size_t index = c->state.cursor;
    mc_entity *value = owner->player_membership.entries[index];
    if (c->state.players->items[index] != value)
        return fail(c, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "CQ Player occurrence provenance", true,
                    NULL);
    ++c->state.cursor; /* Iterator.next advances before the caller's cast. */
    at(c, 36, MC_WORLD_NIGHTMARE_LIST, 0);
    if (value != NULL) {
        const mc_entity_class_literal *player = NULL;
        bool matches = false;
        mc_entity_status status = mc_entity_class_literal_get(c->context.code_classes->entities,
                                                              MC_ENTITY_CLASS_KIND_PLAYER, &player);
        if (status == MC_ENTITY_OK)
            status =
                mc_entity_class_literal_is_assignable_from(player, value->class_literal, &matches);
        if (status != MC_ENTITY_OK)
            return fail(c, native_status(status), "CQ actual Player cast Class namespace", true,
                        NULL);
        if (!matches)
            return fail(c, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "CQ actual Player cast", false,
                        "java/lang/ClassCastException");
        if (!mc_entity_is_kind(value, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER))
            return fail(c, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "CQ canonical Player field body",
                        true, NULL);
        status = mc_entity_retain(value);
        if (status != MC_ENTITY_OK)
            return fail(c, native_status(status), "CQ captured Player native occurrence", true,
                        NULL);
    }
    at(c, 39, MC_WORLD_NIGHTMARE_NONE, 0);
    mc_entity_release(c->state.player);
    c->state.player = value;
    ++c->state.players_started;
    return true;
}

static bool solid(mc_world_nightmare_call *c, unsigned int pc, unsigned int helper_pc, int32_t x,
                  int32_t y, int32_t z, bool *out) {
    at(c, pc, MC_WORLD_NIGHTMARE_SUPPORT, helper_pc);
    if (c->state.world == NULL)
        return source_null(c, "CQ null World solid-support receiver");
    const mc_world_storage_status status =
        mc_world_solid_support_detailed_held(c->state.world, x, y, z, out, &c->support);
    if (status == MC_WORLD_STORAGE_OK)
        return true;
    c->error.support = &c->support;
    (void)fail(c, status, c->support.error.operation, true, NULL);
    c->error.cause = c->support.error.cause;
    registry_exception(c, &c->support.error.registry);
    capture_nearby(c, &c->support.access);
    const char *exception = NULL;
    if (mc_world_access_failure_source_exception(
            &c->support.access.batch.publication.world_access_failure, &exception)) {
        c->error.native_policy = false;
        c->error.source_exception = true;
        c->error.exception_class = exception;
    }
    return false;
}

static bool material(mc_world_nightmare_call *c, int32_t x, int32_t y, int32_t z,
                     mc_material **out) {
    at(c, 246, MC_WORLD_NIGHTMARE_MATERIAL, 72);
    const mc_world_storage_status status =
        mc_world_runtime_material_get_detailed(c->state.world, x, y, z, out, &c->material);
    if (status == MC_WORLD_STORAGE_OK)
        return true;
    c->error.material = &c->material;
    (void)fail(c, status, c->material.operation, true, NULL);
    c->error.cause = c->material.cause;
    registry_exception(c, &c->material.registry);
    return false;
}

static bool land_position(mc_world_nightmare_call *c, bool *out) {
    *out = false;
    bool value;
    if (!solid(c, 246, 50, c->state.x, mc_java_i32_sub(c->state.y, 1), c->state.z, &value))
        return false;
    if (!value)
        return true;
    if (!solid(c, 246, 61, c->state.x, c->state.y, c->state.z, &value))
        return false;
    if (value)
        return true;
    mc_material *selected = NULL;
    if (!material(c, c->state.x, c->state.y, c->state.z, &selected))
        return false;
    at(c, 246, MC_WORLD_NIGHTMARE_MATERIAL, 75);
    if (selected == NULL)
        return source_null(c, "CQ land spawn null Material receiver");
    if (mc_material_d(selected))
        return true;
    if (!solid(c, 246, 88, c->state.x, mc_java_i32_add(c->state.y, 1), c->state.z, &value))
        return false;
    *out = !value;
    return true;
}

static bool pick_coordinate(mc_world_nightmare_call *c, unsigned int axis) {
    static const unsigned int field_pc[3] = {80, 140, 110};
    static const unsigned int floor_pc[3] = {83, 143, 113};
    static const unsigned int first_pc[3] = {92, 152, 122};
    static const unsigned int second_pc[3] = {102, 162, 132};
    at(c, field_pc[axis], MC_WORLD_NIGHTMARE_NONE, 0);
    mc_entity *player = c->state.player;
    if (player == NULL)
        return source_null(c, "CQ null Player coordinate receiver");
    const double value = axis == 0 ? player->base.x : axis == 1 ? player->base.y : player->base.z;
    int32_t center, first, second;
    if (!floor_value(c, value, floor_pc[axis], &center) ||
        !random_int(c, first_pc[axis], axis == 1 ? 16 : 32, &first) ||
        !random_int(c, second_pc[axis], axis == 1 ? 16 : 32, &second))
        return false;
    const int32_t coordinate = mc_java_i32_sub(mc_java_i32_add(center, first), second);
    if (axis == 0)
        c->state.x = coordinate;
    else if (axis == 1)
        c->state.initial_y = coordinate;
    else
        c->state.z = coordinate;
    return true;
}

static bool caught_reflection(mc_world_nightmare_call *c, const char *name, const char *cause) {
    at(c, 362, MC_WORLD_NIGHTMARE_REFLECTION_PRINT, 0);
    c->error.reflection_caught = true;
    c->error.print_stack_trace_pending = true;
    c->error.reflection_exception = name;
    c->error.reflection_cause = cause;
    return fail(c, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                "CQ actual caught Exception.printStackTrace dependency", false, name);
}

static bool mob_failure(mc_world_nightmare_call *c, bool constructor_body) {
    c->error.mob = &c->mob;
    if (c->mob.source_exception && !c->mob.native_policy &&
        (constructor_body || !is_error(c->mob.exception_class)))
        return caught_reflection(c,
                                 constructor_body ? "java/lang/reflect/InvocationTargetException"
                                                  : c->mob.exception_class,
                                 constructor_body ? c->mob.exception_class : NULL);
    const mc_world_storage_status status =
        c->mob.status == MC_MOB_LIMIT        ? MC_WORLD_STORAGE_LIMIT
        : c->mob.status == MC_MOB_ALLOCATION ? MC_WORLD_STORAGE_ALLOCATION_FAILED
        : c->mob.status == MC_MOB_BUSY       ? MC_WORLD_STORAGE_BUSY
                                             : MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    return fail(c, status, c->mob.operation, c->mob.native_policy, c->mob.exception_class);
}

static bool construct_mob(mc_world_nightmare_call *c) {
    at(c, 326, MC_WORLD_NIGHTMARE_MOB, 0);
    if (c->state.class_index < 0 || (size_t)c->state.class_index >= c->array->length)
        return caught_reflection(c, "java/lang/ArrayIndexOutOfBoundsException", NULL);
    if (c->array->items == NULL)
        return fail(c, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "CQ captured Class-array backing", true,
                    NULL);
    const mc_world_nightmare_class_value selected_value =
        c->array->items[(size_t)c->state.class_index];
    c->state.selected_class =
        selected_value.kind == MC_WORLD_NIGHTMARE_CLASS_ENTITY ? selected_value.value.entity : NULL;
    c->state.selected_world_class =
        selected_value.kind == MC_WORLD_NIGHTMARE_CLASS_WORLD ? selected_value.value.world : NULL;
    at(c, 336, MC_WORLD_NIGHTMARE_MOB, 0);
    if (selected_value.kind == MC_WORLD_NIGHTMARE_CLASS_WORLD) {
        if (selected_value.value.world != c->context.code_classes)
            return fail(c, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                        "CQ reflected actual World Class namespace", true, NULL);
        return caught_reflection(c, "java/lang/NoSuchMethodException", NULL);
    }
    if (selected_value.kind != MC_WORLD_NIGHTMARE_CLASS_NULL &&
        selected_value.kind != MC_WORLD_NIGHTMARE_CLASS_ENTITY)
        return fail(c, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "CQ actual typed Class cell", true,
                    NULL);
    if (c->state.selected_class == NULL)
        return caught_reflection(c, "java/lang/NullPointerException", NULL);
    mc_entity_class_literal_view selected;
    const mc_entity_status inspected =
        mc_entity_class_literal_inspect(c->state.selected_class, &selected);
    if (inspected != MC_ENTITY_OK)
        return fail(c, native_status(inspected), "CQ actual reflected Class identity", true, NULL);
    switch (selected.kind) {
        case MC_ENTITY_CLASS_KIND_SPIDER:
        case MC_ENTITY_CLASS_KIND_ZOMBIE:
        case MC_ENTITY_CLASS_KIND_SKELETON:
        case MC_ENTITY_CLASS_KIND_CREEPER:
        case MC_ENTITY_CLASS_KIND_SLIME:
        case MC_ENTITY_CLASS_KIND_PIG:
        case MC_ENTITY_CLASS_KIND_SHEEP:
        case MC_ENTITY_CLASS_KIND_COW:
        case MC_ENTITY_CLASS_KIND_CHICKEN:
        case MC_ENTITY_CLASS_KIND_WOLF:
        case MC_ENTITY_CLASS_KIND_SQUID:
            break;
        default:
            /* No different native constructor is substituted for a reflected
             * public(World) signature absent from the implemented namespace. */
            return fail(c, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                        "CQ reflected public World constructor concrete dependency", true, NULL);
    }
    at(c, 347, MC_WORLD_NIGHTMARE_MOB, 0);
    if (mc_mob_allocate(c->context.code_classes->entities, c->registry, c->state.selected_class,
                        &c->state.candidate, &c->mob) != MC_MOB_OK)
        return mob_failure(c, false);
    if (mc_mob_construct(c->state.candidate, c->state.world, &c->mob) != MC_MOB_OK)
        return mob_failure(c, true);
    at(c, 353, MC_WORLD_NIGHTMARE_NONE, 0);
    mc_entity_release(c->state.mob);
    c->state.mob = c->state.candidate;
    c->state.candidate = NULL;
    ++c->state.constructors;
    return true;
}

static bool pose(mc_world_nightmare_call *c, unsigned int pc, double x, double y, double z,
                 float yaw) {
    at(c, pc, MC_WORLD_NIGHTMARE_POSITION, 0);
    const mc_entity_status status =
        mc_entity_source_position_rotation(c->state.mob, x, y, z, yaw, 0.0f, &c->position);
    if (status == MC_ENTITY_OK)
        return true;
    c->error.position = &c->position;
    return fail(c, native_status(status), c->position.operation, c->position.native_policy,
                c->position.source_null ? "java/lang/NullPointerException" : NULL);
}

static bool initial_pose(mc_world_nightmare_call *c, float x, float y, float z) {
    at(c, 379, MC_WORLD_NIGHTMARE_NONE, 0);
    if (c->state.world == NULL)
        return source_null(c, "CQ initial pose null World Random field receiver");
    mc_java_random *random = c->state.world->fields.r;
    at(c, 382, MC_WORLD_NIGHTMARE_NONE, 0);
    if (random == NULL)
        return source_null(c, "CQ initial pose null Random receiver");
    const float draw = mc_java_random_next_float(random);
    ++c->state.random_calls;
    at(c, 387, MC_WORLD_NIGHTMARE_NONE, 0);
    if (!fp(c))
        return false;
    const float yaw = mc_tick_fmul(draw, 360.0f);
    return pose(c, 389, wt_widen(x), wt_widen(y), wt_widen(z), yaw);
}

static bool eligible(mc_world_nightmare_call *c, bool *out) {
    at(c, 394, MC_WORLD_NIGHTMARE_ELIGIBILITY, 0);
    c->state.eligibility = &c->eligibility;
    const mc_world_storage_status status =
        mc_mob_spawn_eligible(c->state.mob, c->context.math, c->context.code_classes, out,
                              &c->eligibility, &c->eligibility_error);
    if (status != MC_WORLD_STORAGE_OK) {
        return eligibility_failed(c, status);
    }
    ++c->state.predicates;
    return true;
}

static bool path_to_player(mc_world_nightmare_call *c, bool *out) {
    *out = false;
    at(c, 410, MC_WORLD_NIGHTMARE_FINDER, 0);
    const mc_path_status status =
        mc_path_finder_to_entity(c->finder, c->state.mob, c->state.player, 32.0f);
    if (!mc_path_finder_inspect(c->finder, &c->finder_state))
        return fail(c, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "CQ actual FW state view", true, NULL);
    if (status != MC_PATH_OK) {
        c->error.finder = c->finder_state.error;
        const mc_path_error *error = &c->finder_state.error->path;
        return fail(c, path_status(status), error->operation, error->native_policy,
                    error->exception_class);
    }
    if (!c->finder_state.returned)
        return fail(c, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "CQ actual FW returned result", true,
                    NULL);
    ++c->state.paths;
    mc_path *value = c->finder_state.result;
    if (value != NULL && mc_path_retain(value, &c->path_error) != MC_PATH_OK) {
        c->error.path = &c->path_error;
        return fail(c, path_status(c->path_error.status), c->path_error.operation,
                    c->path_error.native_policy, c->path_error.exception_class);
    }
    at(c, 413, MC_WORLD_NIGHTMARE_NONE, 0);
    mc_path_release(c->state.path);
    c->state.path = value;
    if (value == NULL)
        return true;
    at(c, 422, MC_WORLD_NIGHTMARE_PATH, 0);
    mc_path_state state;
    if (!mc_path_inspect(value, &state))
        return fail(c, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "CQ actual DH length view", true, NULL);
    if (state.a <= 1)
        return true;
    at(c, 431, MC_WORLD_NIGHTMARE_PATH, 0);
    const mc_path_point *last = NULL;
    if (mc_path_last(value, &last, &c->path_error) != MC_PATH_OK) {
        c->error.path = &c->path_error;
        return fail(c, path_status(c->path_error.status), c->path_error.operation,
                    c->path_error.native_policy, c->path_error.exception_class);
    }
    c->state.last_point = last;
    mc_path_point_state point;
    if (last == NULL) {
        at(c, 438, MC_WORLD_NIGHTMARE_PATH, 0);
        return source_null(c, "CQ last PathPoint null receiver");
    }
    if (!mc_path_point_inspect(value, last, &point))
        return fail(c, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "CQ retained actual PathPoint", true,
                    NULL);
    static const unsigned int pcs[3] = {447, 470, 493};
    const int32_t coordinates[3] = {point.a, point.c, point.b};
    for (size_t axis = 0; axis < 3; ++axis) {
        at(c, pcs[axis], MC_WORLD_NIGHTMARE_NONE, 0);
        if (c->state.player == NULL)
            return source_null(c, "CQ path acceptance null Player coordinate");
        if (!fp(c))
            return false;
        const double target = axis == 0   ? c->state.player->base.x
                              : axis == 1 ? c->state.player->base.z
                                          : c->state.player->base.y;
        const double difference = wt_sub((double)coordinates[axis], target);
        uint64_t bits;
        memcpy(&bits, &difference, sizeof(bits));
        bits &= UINT64_C(0x7fffffffffffffff);
        double absolute;
        memcpy(&absolute, &bits, sizeof(absolute));
        if (!(absolute < 1.5)) /* Source dcmpg rejects NaN and equality. */
            return true;
    }
    *out = true;
    return true;
}

static bool bed_exit(mc_world_nightmare_call *c) {
    int32_t coordinates[3];
    static const unsigned int field_pc[3] = {505, 513, 521};
    static const unsigned int floor_pc[3] = {508, 516, 524};
    for (size_t axis = 0; axis < 3; ++axis) {
        at(c, field_pc[axis], MC_WORLD_NIGHTMARE_NONE, 0);
        if (c->state.player == NULL)
            return source_null(c, "CQ Bed exit null Player coordinate");
        const double value = axis == 0   ? c->state.player->base.x
                             : axis == 1 ? c->state.player->base.y
                                         : c->state.player->base.z;
        if (!floor_value(c, value, floor_pc[axis], &coordinates[axis]))
            return false;
    }
    at(c, 528, MC_WORLD_NIGHTMARE_BED, 0);
    c->bed_owned = true;
    const mc_world_storage_status status =
        mc_world_bed_spawn_exit(c->state.world, c->registry, coordinates[0], coordinates[1],
                                coordinates[2], 1, &c->limits.bed, &c->candidate_exit, &c->bed);
    if (status != MC_WORLD_STORAGE_OK) {
        c->error.bed = &c->bed;
        (void)fail(c, status, c->bed.operation, c->bed.native_policy, c->bed.exception_class);
        c->error.cause = c->bed.cause;
        return false;
    }
    if (!mc_world_bed_spawn_error_retire(&c->bed)) {
        c->error.bed = &c->bed;
        return fail(c, MC_WORLD_STORAGE_BUSY, "CQ completed Bed child retirement", true, NULL);
    }
    c->bed_owned = false;
    at(c, 531, MC_WORLD_NIGHTMARE_NONE, 0);
    mc_world_bed_spawn_position_free(c->exit);
    c->exit = c->candidate_exit;
    c->candidate_exit = NULL;
    c->state.exit = c->exit;
    if (c->exit == NULL) {
        at(c, 538, MC_WORLD_NIGHTMARE_NONE, 0);
        c->candidate_exit = malloc(sizeof(*c->candidate_exit));
        if (c->candidate_exit == NULL)
            return fail(c, MC_WORLD_STORAGE_ALLOCATION_FAILED, "CQ actual fallback br storage",
                        true, NULL);
        at(c, 550, MC_WORLD_NIGHTMARE_NONE, 0);
        *c->candidate_exit =
            (mc_player_coordinates){c->state.x, mc_java_i32_add(c->state.y, 1), c->state.z};
        c->exit = c->candidate_exit;
        c->candidate_exit = NULL;
        c->state.exit = c->exit;
    }
    return true;
}

static bool publish(mc_world_nightmare_call *c) {
    at(c, 591, MC_WORLD_NIGHTMARE_PUBLICATION, 0);
    bool ignored;
    const mc_world_storage_status status =
        mc_world_runtime_insert_entity(c->state.world, c->state.mob, &ignored, &c->publication);
    if (status == MC_WORLD_STORAGE_OK) {
        ++c->state.publications; /* Counts returned calls, including false. */
        return true;
    }
    c->error.publication = &c->publication;
    (void)fail(c, status, c->publication.operation, true, NULL);
    c->error.cause = c->publication.cause;
    const char *exception = NULL;
    if (mc_world_access_failure_source_exception(&c->publication.world_access_failure,
                                                 &exception)) {
        c->error.native_policy = false;
        c->error.source_exception = true;
        c->error.exception_class = exception;
    }
    (void)capture(c, &c->publication.world_access_failure);
    return false;
}

static bool retire_postspawn(mc_world_nightmare_call *c) {
    if (!c->postspawn.entered)
        return true;
    mc_world_storage_error cleanup;
    const mc_world_storage_status status = mc_world_spawn_postspawn_retire(&c->postspawn, &cleanup);
    if (status == MC_WORLD_STORAGE_OK) {
        c->error.postspawn = NULL;
        return true;
    }
    c->error.postspawn = &c->postspawn;
    return fail(c, status, cleanup.operation, true, NULL);
}

static bool postspawn(mc_world_nightmare_call *c, float x, float y, float z) {
    at(c, 622, MC_WORLD_NIGHTMARE_POSTSPAWN, 0);
    if (!retire_postspawn(c))
        return false;
    mc_world_storage_error admission;
    const mc_world_storage_status status = mc_world_spawn_postspawn_begin(
        c->state.world, c->registry, c->state.mob, x, y, z, &c->postspawn, &admission);
    c->state.random_calls += c->postspawn.random_calls;
    c->state.publications += c->postspawn.publications;
    if (status != MC_WORLD_STORAGE_OK) {
        c->error.postspawn = &c->postspawn;
        return fail(c, status, c->postspawn.entered ? c->postspawn.operation : admission.operation,
                    c->postspawn.entered ? c->postspawn.native_policy : true,
                    c->postspawn.entered ? c->postspawn.exception_class : NULL);
    }
    if (!c->postspawn.returned)
        return fail(c, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "CQ postspawn actual return", true,
                    NULL);
    ++c->state.postspawn_calls;
    return true;
}

static bool wake_player(mc_world_nightmare_call *c) {
    at(c, 630, MC_WORLD_NIGHTMARE_WAKE, 0);
    const mc_player_sleep_context context = {.code_registry = c->registry};
    c->wake_owned = true;
    const mc_player_sleep_status status = mc_player_wake_begin(
        c->state.player, true, false, false, &context, &c->wake, &c->wake_admission);
    if (c->wake != NULL && !mc_player_sleep_inspect(c->wake, &c->wake_state))
        return fail(c, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "CQ retained wake view", true, NULL);
    if (status != MC_PLAYER_SLEEP_OK) {
        const mc_player_sleep_error *error =
            c->wake == NULL ? &c->wake_admission : c->wake_state.failure;
        c->error.wake = error;
        return fail(c,
                    error->world.status != MC_WORLD_STORAGE_OK ? error->world.status
                    : status == MC_PLAYER_SLEEP_LIMIT          ? MC_WORLD_STORAGE_LIMIT
                    : status == MC_PLAYER_SLEEP_ALLOCATION_FAILED
                        ? MC_WORLD_STORAGE_ALLOCATION_FAILED
                    : status == MC_PLAYER_SLEEP_BUSY ? MC_WORLD_STORAGE_BUSY
                                                     : MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                    error->operation, error->native_policy, error->exception_class);
    }
    if (c->wake == NULL || !c->wake_state.returned)
        return fail(c, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "CQ wake actual returned child", true,
                    NULL);
    ++c->state.wakes;
    if (mc_player_sleep_retire(&c->wake, &c->wake_cleanup) != MC_PLAYER_SLEEP_OK) {
        c->error.wake_cleanup = &c->wake_cleanup;
        return fail(c, MC_WORLD_STORAGE_BUSY, c->wake_cleanup.operation, true, NULL);
    }
    c->wake_owned = false;
    c->error.wake = NULL;
    c->error.wake_cleanup = NULL;
    return true;
}

static bool ambient(mc_world_nightmare_call *c) {
    at(c, 635, MC_WORLD_NIGHTMARE_AMBIENT, 0);
    mc_entity_tick_error_init(&c->ambient);
    if (mc_tick_ambient_entered(c->state.mob, c->registry, &c->ambient)) {
        ++c->state.ambient_calls;
        return true;
    }
    c->error.ambient = &c->ambient;
    (void)fail(c, native_status(c->ambient.status), c->ambient.operation, c->ambient.native_policy,
               c->ambient.exception_class);
    (void)capture(c, &c->ambient.world.world_access_failure);
    (void)capture(c, &c->ambient.world.presentation.world_access_failure);
    return false;
}

static bool attempt(mc_world_nightmare_call *c) {
    if (!retire_postspawn(c))
        return false;
    ++c->state.attempts_started;
    /* Source order is X, Z, Y; every Random receiver is freshly loaded. */
    if (!pick_coordinate(c, 0) || !pick_coordinate(c, 2) || !pick_coordinate(c, 1))
        return false;
    if (c->state.initial_y < 1)
        c->state.initial_y = 1;
    else if (c->state.initial_y > 128)
        c->state.initial_y = 128;
    if (c->array->length > (size_t)INT32_MAX)
        return fail(c, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "CQ captured array signed length", true,
                    NULL);
    if (!random_int(c, 200, (int32_t)c->array->length, &c->state.class_index))
        return false;
    c->state.y = c->state.initial_y;
    while (c->state.y > 2) {
        bool value;
        if (!work(c) ||
            !solid(c, 224, 0, c->state.x, mc_java_i32_sub(c->state.y, 1), c->state.z, &value))
            return false;
        if (value)
            break;
        c->state.y = mc_java_i32_sub(c->state.y, 1);
    }
    const int32_t maximum_y = mc_java_i32_add(c->state.initial_y, 16);
    for (;;) {
        bool permitted;
        if (!work(c) || !land_position(c, &permitted))
            return false;
        if (permitted || c->state.y >= maximum_y || c->state.y >= 128)
            break;
        c->state.y = mc_java_i32_add(c->state.y, 1);
    }
    if (c->state.y >= maximum_y || c->state.y >= 128) {
        c->state.y = c->state.initial_y;
        return true;
    }
    at(c, 306, MC_WORLD_NIGHTMARE_NONE, 0);
    if (!fp(c))
        return false;
    const float x = wt_fadd(wt_i2f(c->state.x), .5f), y = wt_i2f(c->state.y);
    const float z = wt_fadd(wt_i2f(c->state.z), .5f);
    if (!construct_mob(c) || !initial_pose(c, x, y, z))
        return false;
    bool allowed;
    if (!eligible(c, &allowed))
        return false;
    if (!allowed)
        return true;
    if (!path_to_player(c, &allowed))
        return false;
    if (!allowed)
        return true;
    if (!bed_exit(c))
        return false;
    at(c, 565, MC_WORLD_NIGHTMARE_NONE, 0);
    if (!fp(c))
        return false;
    const float exit_x = wt_fadd(wt_i2f(c->exit->x), .5f);
    const double exit_y = (double)c->exit->y;
    const float exit_z = wt_fadd(wt_i2f(c->exit->z), .5f);
    if (!pose(c, 585, wt_widen(exit_x), exit_y, wt_widen(exit_z), 0.0f) || !publish(c))
        return false;
    /* These are separate late br reads in the original postspawn operands. */
    const float post_x = wt_fadd(wt_i2f(c->exit->x), .5f), post_y = wt_i2f(c->exit->y);
    const float post_z = wt_fadd(wt_i2f(c->exit->z), .5f);
    if (!postspawn(c, post_x, post_y, post_z) || !wake_player(c) || !ambient(c))
        return false;
    at(c, 638, MC_WORLD_NIGHTMARE_NONE, 0);
    c->state.result = true;
    at(c, 640, MC_WORLD_NIGHTMARE_NONE, 0);
    c->state.player_succeeded = true;
    return true;
}

static bool body(mc_world_nightmare_call *c) {
    at(c, 0, MC_WORLD_NIGHTMARE_CLASSES, 0);
    const mc_world_storage_status initialized =
        mc_world_nightmare_classes_ensure(c->context.code_classes, &c->class_error);
    if (initialized != MC_WORLD_STORAGE_OK) {
        c->error.classes = &c->class_error;
        return fail(c, initialized, c->class_error.operation, true, NULL);
    }
    at(c, 7, MC_WORLD_NIGHTMARE_FINDER, 0);
    const mc_path_finder_limits limits = {
        c->limits.path.max_points, c->limits.path.max_expanded_points,
        c->limits.path.max_cell_queries, c->limits.path.max_predecessor_steps,
        c->limits.path.max_work};
    const mc_path_status created = mc_path_finder_create_for_world(
        c->context.code_classes->entities, c->registry, c->context.math, c->state.world, &limits,
        &c->budget, &c->finder, &c->path_error);
    c->state.finder = c->finder;
    if (created != MC_PATH_OK) {
        if (c->finder != NULL && mc_path_finder_inspect(c->finder, &c->finder_state))
            c->error.finder = c->finder_state.error;
        c->error.path = &c->path_error;
        return fail(c, path_status(created), c->path_error.operation, c->path_error.native_policy,
                    c->path_error.exception_class);
    }
    size_t count;
    if (!list_size(c, 12, &count))
        return false;
    c->state.expected_mod_count = c->state.players->mod_count;
    for (;;) {
        if (!list_size(c, 21, &count))
            return false;
        if (c->state.cursor == count)
            break;
        if (!work(c) || !next_player(c))
            return false;
        at(c, 41, MC_WORLD_NIGHTMARE_NONE, 0);
        c->array = c->context.code_classes->nightmare.a;
        c->state.class_array_identity = (uintptr_t)c->array;
        c->state.class_array_length = c->array == NULL ? 0 : c->array->length;
        if (c->array == NULL || c->array->length == 0)
            continue;
        c->state.player_succeeded = false;
        for (c->state.attempt = 0; c->state.attempt < 20 && !c->state.player_succeeded;
             c->state.attempt = mc_java_i32_add(c->state.attempt, 1)) {
            at(c, 78, MC_WORLD_NIGHTMARE_NONE, 0);
            if (!work(c) || !attempt(c))
                return false;
            at(c, 643, MC_WORLD_NIGHTMARE_NONE, 0);
            ++c->state.attempts_returned;
        }
    }
    at(c, 653, MC_WORLD_NIGHTMARE_NONE, 0);
    c->state.returned = true;
    return true;
}

static mc_world_storage_status admission(mc_world_nightmare_error *out,
                                         mc_world_storage_status status, const char *operation) {
    if (out != NULL)
        *out = (mc_world_nightmare_error){.status = status,
                                          .operation = operation,
                                          .native_policy = status != MC_WORLD_STORAGE_OK,
                                          .cause = {status, operation, 0}};
    return status;
}

static bool hold_world(mc_world_nightmare_call *c, mc_world_runtime *world) {
    if (world == NULL)
        return true;
    if (world->construction.mode != MC_WORLD_RUNTIME_GAME ||
        world->construction.phase == MC_WORLD_RUNTIME_RETIRED || world->classes == NULL ||
        world->classes->side != MC_WORLD_INFO_CLIENT)
        return fail(c, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "CQ actual client GAME World owner",
                    true, NULL);
    if (c->world_count == sizeof(c->worlds) / sizeof(*c->worlds))
        return fail(c, MC_WORLD_STORAGE_LIMIT, "CQ retained World occurrence ceiling", true, NULL);
    if (!mc_world_runtime_binding_retain(world))
        return fail(c, world->last_error.status, world->last_error.operation, true, NULL);
    c->worlds[c->world_count++] = world;
    return true;
}

mc_world_storage_status mc_world_nightmare_begin(mc_world_runtime *world,
                                                 mc_world_reference_list *players,
                                                 const mc_world_nightmare_context *context,
                                                 const mc_world_nightmare_limits *limits,
                                                 mc_world_nightmare_call **out,
                                                 mc_world_nightmare_error *error) {
    if (out == NULL || *out != NULL || context == NULL || context->code_classes == NULL ||
        context->code_classes->side != MC_WORLD_INFO_CLIENT)
        return admission(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "CQ retained call arguments");
    const mc_world_nightmare_limits selected =
        limits == NULL ? mc_world_nightmare_default_limits() : *limits;
    if (selected.path.max_native_bytes < sizeof(mc_world_nightmare_call) ||
        selected.path.max_allocations == 0)
        return admission(error, MC_WORLD_STORAGE_LIMIT, "CQ retained working owner ceiling");
    if (context->code_classes->native_borrows == SIZE_MAX)
        return admission(error, MC_WORLD_STORAGE_LIMIT, "CQ defining class occurrence ceiling");
    mc_world_nightmare_call *c = calloc(1, sizeof(*c));
    if (c == NULL)
        return admission(error, MC_WORLD_STORAGE_ALLOCATION_FAILED, "CQ retained call allocation");
    c->context = *context;
    c->limits = selected;
    c->budget = (mc_path_budget){selected.path.max_native_bytes, selected.path.max_allocations,
                                 sizeof(*c), sizeof(*c), 1};
    c->state.world = world;
    c->state.players = players;
    c->state.failure = &c->error;
    c->state.cleanup = &c->cleanup;
    c->state.active = true;
    c->classes_held = true;
    ++context->code_classes->native_borrows;
    c->registry = context->code_classes->registry;
    mc_world_bed_spawn_error_init(&c->bed);
    *out = c;
    (void)admission(error, MC_WORLD_STORAGE_OK, NULL);
    bool entered = false;
    if (!hold_world(c, world) || !hold_world(c, context->list_owner))
        goto done;
    if (c->registry != NULL) {
        const mc_registry_status held = mc_registry_classes_retain(
            c->registry, MC_REGISTRY_CLIENT, context->code_classes->profile, &c->registry_error);
        if (held != MC_REGISTRY_OK) {
            c->error.registry = &c->registry_error;
            (void)fail(c, registry_status(held), "CQ defining Registry native occurrence", true,
                       NULL);
            goto done;
        }
        c->registry_held = true;
    }
    if (context->math != NULL) {
        const mc_math_class_status held = mc_math_classes_retain(context->math, &c->math);
        if (held != MC_MATH_CLASS_OK) {
            c->error.math = &c->math;
            (void)fail(c,
                       held == MC_MATH_CLASS_LIMIT ? MC_WORLD_STORAGE_LIMIT
                                                   : MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                       "CQ defining Math native occurrence", true, NULL);
            goto done;
        }
        c->held_math = context->math;
    }
    if (world != NULL) {
        mc_world_storage_error cause;
        const mc_world_storage_status status = mc_world_runtime_call_enter(world, &cause);
        if (status != MC_WORLD_STORAGE_OK) {
            (void)fail(c, status, cause.operation, true, NULL);
            goto done;
        }
        entered = true;
    }
    (void)body(c);
done:
    if (entered)
        mc_world_runtime_call_leave(world);
    c->state.active = false;
    if (world != NULL)
        world->last_error = c->error.cause;
    return c->error.status;
}

bool mc_world_nightmare_inspect(const mc_world_nightmare_call *c, mc_world_nightmare_state *out) {
    if (c == NULL || out == NULL)
        return false;
    *out = c->state;
    out->native_bytes = c->budget.bytes;
    out->native_peak_bytes = c->budget.peak_bytes;
    out->native_allocations = c->budget.allocations;
    return true;
}

static mc_world_storage_status cleanup_failed(mc_world_nightmare_call *c,
                                              mc_world_nightmare_child child,
                                              mc_world_storage_status status, const char *operation,
                                              mc_world_nightmare_cleanup *out) {
    c->cleanup.status = status;
    c->cleanup.operation = operation;
    c->cleanup.child = child;
    if (out != NULL)
        *out = c->cleanup;
    return status;
}

static bool entity_release_admit(mc_entity *entity) {
    return entity == NULL ||
           (entity->holds != 0 &&
            (!mc_entity_has_mob(entity) ||
             (!entity->mob->tick_active && entity->mob->pending_ray == NULL &&
              entity->mob->pending_path == NULL && entity->mob->pending_death == NULL &&
              entity->mob->pending_wolf_damage == NULL &&
              entity->mob->pending_slime_removal == NULL)));
}

mc_world_storage_status mc_world_nightmare_retire(mc_world_nightmare_call **slot,
                                                  mc_world_nightmare_cleanup *out) {
    const mc_world_nightmare_cleanup empty = {0};
    if (slot == NULL) {
        if (out != NULL)
            *out = (mc_world_nightmare_cleanup){.status = MC_WORLD_STORAGE_INVALID_ARGUMENT,
                                                .operation = "CQ retirement slot"};
        return MC_WORLD_STORAGE_INVALID_ARGUMENT;
    }
    mc_world_nightmare_call *c = *slot;
    if (c == NULL) {
        if (out != NULL)
            *out = empty;
        return MC_WORLD_STORAGE_OK;
    }
    c->cleanup = empty;
    if (c->state.active)
        return cleanup_failed(c, MC_WORLD_NIGHTMARE_NONE, MC_WORLD_STORAGE_BUSY,
                              "CQ call remains active", out);
    if (c->eligibility_capture_pending && !capture_eligibility(c))
        return cleanup_failed(c, MC_WORLD_NIGHTMARE_ELIGIBILITY, MC_WORLD_STORAGE_LIMIT,
                              "CQ eligibility WorldAccess capture remains unavailable", out);
    if (c->pending_receipt.receipt != NULL) {
        const mc_world_access_failure pending = c->pending_receipt;
        c->pending_receipt = (mc_world_access_failure){0};
        if (!capture(c, &pending))
            return cleanup_failed(c, MC_WORLD_NIGHTMARE_PUBLICATION, MC_WORLD_STORAGE_LIMIT,
                                  "CQ reached WorldAccess capture remains unavailable", out);
    }
    if (c->wake_owned) {
        const mc_player_sleep_status status = mc_player_sleep_retire(&c->wake, &c->cleanup.wake);
        if (status != MC_PLAYER_SLEEP_OK)
            return cleanup_failed(c, MC_WORLD_NIGHTMARE_WAKE, MC_WORLD_STORAGE_BUSY,
                                  c->cleanup.wake.operation, out);
        c->wake_owned = false;
        c->error.wake = NULL;
        c->error.wake_cleanup = NULL;
    }
    if (c->bed_owned) {
        if (!mc_world_bed_spawn_error_retire(&c->bed)) {
            c->cleanup.bed = &c->bed;
            return cleanup_failed(c, MC_WORLD_NIGHTMARE_BED, MC_WORLD_STORAGE_BUSY,
                                  "CQ Bed child checked retirement", out);
        }
        c->bed_owned = false;
        c->error.bed = NULL;
    }
    if (c->postspawn.entered) {
        const mc_world_storage_status status =
            mc_world_spawn_postspawn_retire(&c->postspawn, &c->cleanup.world);
        if (status != MC_WORLD_STORAGE_OK) {
            c->cleanup.postspawn = &c->postspawn;
            return cleanup_failed(c, MC_WORLD_NIGHTMARE_POSTSPAWN, status,
                                  c->cleanup.world.operation, out);
        }
        c->error.postspawn = NULL;
    }
    if (c->finder != NULL) {
        const mc_path_status status = mc_path_finder_retire(&c->finder, &c->cleanup.finder);
        if (status != MC_PATH_OK)
            return cleanup_failed(c, MC_WORLD_NIGHTMARE_FINDER, path_status(status),
                                  c->cleanup.finder.operation, out);
        c->state.finder = NULL;
        c->state.last_point = NULL;
        c->error.finder = NULL;
    }
    if (!entity_release_admit(c->state.player) || !entity_release_admit(c->state.mob) ||
        !entity_release_admit(c->state.candidate))
        return cleanup_failed(c, MC_WORLD_NIGHTMARE_MOB, MC_WORLD_STORAGE_BUSY,
                              "CQ owned Entity child remains active", out);
    for (size_t i = 0; i < c->world_count; ++i) {
        size_t count = 0;
        for (size_t j = 0; j < c->world_count; ++j)
            if (c->worlds[i] == c->worlds[j])
                ++count;
        if (c->worlds[i]->persistent_bindings < count)
            return cleanup_failed(c, MC_WORLD_NIGHTMARE_NONE, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                  "CQ retained World occurrence metadata", out);
    }
    if (c->classes_held && c->context.code_classes->native_borrows == 0)
        return cleanup_failed(c, MC_WORLD_NIGHTMARE_CLASSES, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                              "CQ defining class occurrence metadata", out);
    if (c->registry_held) {
        const mc_registry_status status =
            mc_registry_classes_release(c->registry, &c->cleanup.registry);
        if (status != MC_REGISTRY_OK)
            return cleanup_failed(c, MC_WORLD_NIGHTMARE_CLASSES, registry_status(status),
                                  c->cleanup.registry.message, out);
        c->registry_held = false;
        c->error.registry = NULL;
    }
    if (c->held_math != NULL) {
        const mc_math_class_status status =
            mc_math_classes_release(&c->held_math, &c->cleanup.math);
        if (status != MC_MATH_CLASS_OK)
            return cleanup_failed(c, MC_WORLD_NIGHTMARE_MATH,
                                  status == MC_MATH_CLASS_BUSY ? MC_WORLD_STORAGE_BUSY
                                                               : MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                  c->cleanup.math.operation, out);
        c->error.math = NULL;
    }
    mc_path_release(c->state.path);
    mc_entity_release(c->state.candidate);
    mc_entity_release(c->state.mob);
    mc_entity_release(c->state.player);
    mc_world_bed_spawn_position_free(c->candidate_exit);
    mc_world_bed_spawn_position_free(c->exit);
    for (size_t i = 0; i < c->receipt_count; ++i)
        mc_world_access_failure_release(&c->receipts[i]);
    while (c->world_count != 0)
        mc_world_runtime_binding_release(c->worlds[--c->world_count]);
    if (c->classes_held)
        --c->context.code_classes->native_borrows;
    free(c);
    *slot = NULL;
    if (out != NULL)
        *out = empty;
    return MC_WORLD_STORAGE_OK;
}
