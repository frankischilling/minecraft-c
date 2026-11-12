#include "mc/client/controller/use.h"
#include "singleplayer_internal.h"

#include "block/registry/runtime_internal.h"
#include "client/application_state.h"
#include "item/render/internal.h"
#include "item/stack/source_internal.h"
#include "mc/item/use/use.h"
#include "mc/world/runtime/blocks.h"
#include "mc/world/runtime/placement.h"
#include "mc/world/runtime/world_access.h"
#include "mc/world/storage/source_chunk_loader.h"
#include "world/runtime/construction_internal.h"
#include "world/runtime/runtime_internal.h"

#include <stdlib.h>
#include <string.h>

struct mc_singleplayer_use_call {
    mc_singleplayer_use_state state;
    mc_singleplayer_use_error error, cleanup;
    mc_item_use_call *item;
    mc_item_use_state item_state;
    mc_item_use_error item_error, item_cleanup;
    mc_world_placement_error activation;
    mc_world_block_activation_context activation_context;
    mc_math_classes *math;
    mc_registry_classes *registry;
    mc_world_runtime_acquisition_error *acquisition;
    mc_world_source_chunk_error *source;
    mc_world_access_failure receipts[4];
    size_t receipt_count;
    bool world_held, player_held, stack_held;
};

static const char *descriptor(const mc_singleplayer_use_call *c) {
    return c->state.on_block ? "(Lgs;Lfd;Liz;IIII)Z" : "(Lgs;Lfd;Liz;)Z";
}

static bool fail(mc_singleplayer_use_call *c, mc_singleplayer_use_status status,
                 const char *operation, unsigned int pc, bool native) {
    c->error.status = status;
    c->error.operation = operation;
    c->error.site = (mc_player_site){"ob", "a", descriptor(c), pc};
    c->error.native_policy = native;
    c->state.source_pc = pc;
    return false;
}

static bool null_reference(mc_singleplayer_use_call *c, const char *operation, unsigned int pc) {
    c->error.source_exception = true;
    c->error.exception_class = "java/lang/NullPointerException";
    return fail(c, MC_SINGLEPLAYER_USE_SOURCE_NULL, operation, pc, false);
}

static bool world_hold(mc_singleplayer_use_call *c, unsigned int pc) {
    if (c->world_held || c->state.world == NULL)
        return true;
    if (!mc_world_runtime_binding_retain(c->state.world))
        return fail(c, MC_SINGLEPLAYER_USE_BUSY, "Use actual World occurrence", pc, true);
    c->world_held = true;
    return true;
}

static bool player_hold(mc_singleplayer_use_call *c, unsigned int pc) {
    if (c->player_held || c->state.player == NULL)
        return true;
    c->error.player = mc_entity_retain(c->state.player);
    if (c->error.player != MC_ENTITY_OK)
        return fail(c, MC_SINGLEPLAYER_USE_PLAYER, "Use actual Player occurrence", pc, true);
    c->player_held = true;
    return true;
}

static bool stack_hold(mc_singleplayer_use_call *c, unsigned int pc) {
    if (c->stack_held || c->state.stack == NULL)
        return true;
    c->error.stack = mc_item_stack_retain(c->state.stack);
    if (c->error.stack != MC_ITEM_STACK_OK)
        return fail(c, MC_SINGLEPLAYER_USE_STACK, "Use actual Stack occurrence", pc, true);
    c->stack_held = true;
    return true;
}

static bool registry_failure(mc_singleplayer_use_call *c, unsigned int pc) {
    mc_item_render_throwable throwable;
    mc_item_render_registry_classification(c->registry, &c->error.registry, &throwable,
                                           &c->error.exception_class);
    c->error.source_exception = throwable == MC_ITEM_RENDER_SOURCE_EXCEPTION;
    c->error.source_error = throwable == MC_ITEM_RENDER_SOURCE_ERROR;
    return fail(c, MC_SINGLEPLAYER_USE_REGISTRY, c->error.registry.message, pc,
                throwable == MC_ITEM_RENDER_NO_THROWABLE);
}

static bool registry_hold(mc_singleplayer_use_call *c, unsigned int pc) {
    if (c->registry != NULL)
        return true;
    /* Native association with the invoking class namespace, read only when
     * UU/IZ first reaches a static use. No OB source App field is invoked and
     * no replacement namespace is created for nullable arguments. */
    mc_registry_classes *registry = NULL;
    if (c->state.world != NULL && c->state.world->classes != NULL)
        registry = c->state.world->classes->registry;
    else if (c->state.controller->fields.a != NULL)
        registry = c->state.controller->fields.a->registry;
    if (registry == NULL)
        return fail(c, MC_SINGLEPLAYER_USE_DEPENDENCY, "Use actual invoking Registry namespace", pc,
                    true);
    if (mc_registry_classes_retain(registry, MC_REGISTRY_CLIENT, c->state.controller->profile,
                                   &c->error.registry) != MC_REGISTRY_OK)
        return fail(c, MC_SINGLEPLAYER_USE_REGISTRY, "Use actual Registry occurrence", pc, true);
    c->registry = registry;
    return true;
}

static bool receipt(mc_singleplayer_use_call *c, const mc_world_access_failure *v) {
    if (v->receipt == NULL)
        return true;
    for (size_t i = 0; i < c->receipt_count; ++i)
        if (c->receipts[i].receipt == v->receipt)
            return true;
    if (c->receipt_count == sizeof c->receipts / sizeof *c->receipts)
        return false;
    if (mc_world_access_failure_retain(v, &c->receipts[c->receipt_count]) != MC_WORLD_STORAGE_OK)
        return false;
    ++c->receipt_count;
    return true;
}

static bool query_capture(mc_singleplayer_use_call *c) {
    mc_world_runtime_acquisition_error *e = c->acquisition;
    if (e == NULL)
        return true;
    if (e->source_chunk != NULL && c->source == NULL) {
        c->source = malloc(sizeof(*c->source));
        if (c->source == NULL)
            return false;
        *c->source = *e->source_chunk;
        e->source_chunk = c->source;
    }
    return receipt(c, &e->batch.publication.world_access_failure) &&
           receipt(c, &e->tile_entity.world_access_failure) &&
           receipt(c, &e->tile_entity.publication.world_access_failure) &&
           receipt(c, &e->tile_entity.batch.publication.world_access_failure);
}

static void query_source_exception(mc_singleplayer_use_call *c) {
    const mc_world_runtime_acquisition_error *e = c->acquisition;
    const mc_world_access_failure *failures[] = {
        &e->batch.publication.world_access_failure, &e->tile_entity.world_access_failure,
        &e->tile_entity.publication.world_access_failure,
        &e->tile_entity.batch.publication.world_access_failure};
    for (size_t i = 0; i < sizeof failures / sizeof *failures; ++i) {
        const char *exception_class = NULL;
        if (mc_world_access_failure_source_exception(failures[i], &exception_class)) {
            c->error.source_exception = true;
            c->error.exception_class = exception_class;
            c->error.native_policy = false;
            return;
        }
    }
}

static void item_snapshot(mc_singleplayer_use_call *c) {
    if (c->item == NULL || !mc_item_use_inspect(c->item, &c->item_state))
        return;
    const mc_item_use_state *v = &c->item_state;
    c->state.item = v;
    c->state.item_returned = v->returned;
    c->state.result_stack = v->result_stack;
    if (c->state.on_block) {
        c->state.x = v->target_x;
        c->state.y = v->target_y;
        c->state.z = v->target_z;
        c->state.face = v->target_face;
        c->state.block_id = v->block_id;
        c->state.metadata = v->metadata;
        c->state.count_read = v->count_read;
        c->state.count_before = v->count_before;
        c->state.count_after = v->count_after;
    }
    c->state.placement_admitted = v->predicate_returned && v->can_place;
    c->state.setter_returned = v->setter_returned;
    c->state.placed = v->setter_returned && v->setter_value;
    c->state.sound_returned = v->sound_returned;
    c->state.count_stored = v->count_stored;
    c->state.statistic_returned = v->statistic_returned;
}

static bool item_result(mc_singleplayer_use_call *c, mc_item_use_status status, unsigned int pc) {
    item_snapshot(c);
    if (status == MC_ITEM_USE_OK) {
        if (c->item != NULL && c->item_state.returned)
            return true;
        return fail(c, MC_SINGLEPLAYER_USE_DEPENDENCY, "Use actual IZ child did not return", pc,
                    true);
    }
    c->error.item = &c->item_error;
    c->error.child = (mc_player_site){c->item_error.site.class_name, c->item_error.site.method,
                                      c->item_error.site.descriptor, c->item_error.site.pc};
    c->error.source_exception = c->item_error.source_exception;
    c->error.source_error = c->item_error.source_error;
    c->error.exception_class = c->item_error.exception_class;
    return fail(c, MC_SINGLEPLAYER_USE_ITEM, c->item_error.operation, pc,
                c->item_error.native_policy);
}

static bool on_block(mc_singleplayer_use_call *c) {
    if (c->state.world == NULL)
        return null_reference(c, "OB use-on-block actual World query null receiver", 7);
    if (!world_hold(c, 7))
        return false;
    c->acquisition = calloc(1, sizeof(*c->acquisition));
    if (c->acquisition == NULL)
        return fail(c, MC_SINGLEPLAYER_USE_ALLOCATION, "Use reached World query diagnostic", 7,
                    true);
    c->error.acquisition = c->acquisition;
    const mc_world_storage_status status =
        mc_world_runtime_block_get_detailed(c->state.world, c->state.x, c->state.y, c->state.z,
                                            &c->state.clicked_id, &c->error.world, c->acquisition);
    if (status != MC_WORLD_STORAGE_OK)
        fail(c, MC_SINGLEPLAYER_USE_WORLD, c->error.world.operation, 7, true);
    if (!query_capture(c)) {
        if (c->error.status == MC_SINGLEPLAYER_USE_OK)
            fail(c, MC_SINGLEPLAYER_USE_LIMIT, "Use World query producer retention", 7, true);
        c->cleanup.status = MC_SINGLEPLAYER_USE_CLEANUP;
        c->cleanup.operation = "Use World query producer retention";
        c->cleanup.native_policy = true;
        return false;
    }
    if (status != MC_WORLD_STORAGE_OK) {
        query_source_exception(c);
        return false;
    }
    c->state.block_read = true;
    if (c->state.clicked_id > 0) {
        if (!registry_hold(c, 17))
            return false;
        mc_registry_block_m_view array;
        if (mc_registry_block_m_get(c->registry, &array, &c->error.registry) != MC_REGISTRY_OK)
            return registry_failure(c, 17);
        const mc_block *block;
        if (mc_registry_block_m_at(&array, c->state.clicked_id, &block, &c->error.registry) !=
            MC_REGISTRY_OK)
            return registry_failure(c, 22);
        if (block == NULL)
            return null_reference(c, "OB actual Block activation null receiver", 31);
        if (!player_hold(c, 31))
            return false;
        c->state.activation_entered = true;
        bool activated;
        c->error.activation = &c->activation;
        if (mc_world_block_activate_with_context(c->registry, block, c->state.world, c->state.x,
                                                 c->state.y, c->state.z, c->state.player,
                                                 &c->activation_context, &activated,
                                                 &c->activation) != MC_WORLD_STORAGE_OK) {
            c->error.child =
                (mc_player_site){c->activation.site.class_name, c->activation.site.method,
                                 c->activation.site.descriptor, c->activation.site.pc};
            c->error.source_exception = c->activation.source_exception;
            c->error.source_error = c->activation.source_error;
            c->error.exception_class = c->activation.exception_class;
            return fail(c, MC_SINGLEPLAYER_USE_WORLD, c->activation.operation, 31,
                        c->activation.native_policy);
        }
        c->error.activation = NULL;
        c->state.activation_returned = true;
        c->state.activated = activated;
        if (activated) {
            c->state.result = true;
            c->state.source_pc = 38;
            return true;
        }
    }
    if (c->state.stack == NULL) {
        c->state.result = false;
        c->state.source_pc = 44;
        return true;
    }
    if (!stack_hold(c, 56) || !player_hold(c, 56) || !registry_hold(c, 56))
        return false;
    c->state.item_entered = true;
    const mc_item_use_context item_context = {.math = c->math};
    if (!item_result(c,
                     mc_item_use_on_block_with_context_begin(
                         c->registry, c->state.stack, c->state.player, c->state.world, c->state.x,
                         c->state.y, c->state.z, c->state.face, &item_context, &c->item,
                         &c->item_error),
                     56))
        return false;
    c->state.result = c->item_state.result;
    c->state.source_pc = 59;
    return true;
}

static mc_inventory_player *inventory(mc_singleplayer_use_call *c, unsigned int player_pc,
                                      unsigned int inventory_pc) {
    if (c->state.player == NULL) {
        null_reference(c, "OB use actual Player Inventory field receiver", player_pc);
        return NULL;
    }
    mc_local_player_snapshot snapshot;
    c->error.player = mc_local_player_get_snapshot(c->state.player, &snapshot);
    if (c->error.player != MC_ENTITY_OK) {
        fail(c, MC_SINGLEPLAYER_USE_PLAYER, "Use actual concrete Player fields", player_pc, true);
        return NULL;
    }
    if (snapshot.player.c == NULL)
        null_reference(c, "OB use actual Inventory field receiver", inventory_pc);
    return snapshot.player.c;
}

static bool slot_store(mc_singleplayer_use_call *c, mc_item_stack *value, bool clear) {
    const unsigned int first = clear ? 61u : 36u, second = clear ? 68u : 43u;
    const unsigned int store = clear ? 75u : 51u;
    mc_inventory_player *array_owner = inventory(c, first, first + 3u);
    if (array_owner == NULL)
        return false;
    mc_inventory_player_state array;
    if (!mc_inventory_player_read_state(array_owner, &array))
        return fail(c, MC_SINGLEPLAYER_USE_INVENTORY, "Use actual captured main-array view", store,
                    true);
    mc_inventory_player *selected_owner = inventory(c, second, second + 3u);
    if (selected_owner == NULL)
        return false;
    mc_inventory_player_state selected;
    if (!mc_inventory_player_read_state(selected_owner, &selected))
        return fail(c, MC_SINGLEPLAYER_USE_INVENTORY, "Use actual current selected-index view",
                    store, true);
    c->state.selected = selected.selected;
    if (array.main_slots == NULL)
        return null_reference(c, "OB use actual main-array store null receiver", store);
    if (selected.selected < 0 || selected.selected >= 36) {
        c->error.source_exception = true;
        c->error.exception_class = "java/lang/ArrayIndexOutOfBoundsException";
        return fail(c, MC_SINGLEPLAYER_USE_SOURCE_INDEX, "OB use actual main-array store index",
                    store, false);
    }
    if (mc_inventory_player_set(array_owner, selected.selected, value, &c->error.inventory) !=
        MC_INVENTORY_OK)
        return fail(c, MC_SINGLEPLAYER_USE_INVENTORY,
                    "Use actual selected-array occurrence replacement", store, true);
    if (clear)
        c->state.zero_slot_cleared = true;
    else
        c->state.selected_slot_stored = true;
    return true;
}

static bool in_air(mc_singleplayer_use_call *c) {
    if (c->state.stack == NULL)
        return null_reference(c, "OB use-in-air actual original Stack count receiver", 1);
    if (!stack_hold(c, 1))
        return false;
    c->error.stack = mc_item_stack_source_count_get(c->state.stack, &c->state.count_before);
    if (c->error.stack != MC_ITEM_STACK_OK)
        return fail(c, MC_SINGLEPLAYER_USE_STACK, "Use actual original Stack count", 1, true);
    c->state.count_read = true;
    if (!world_hold(c, 9) || !player_hold(c, 9) || !registry_hold(c, 9))
        return false;
    c->state.item_entered = true;
    if (!item_result(c,
                     mc_item_use_in_air_begin(c->registry, c->state.stack, c->state.world,
                                              c->state.player, &c->item, &c->item_error),
                     9))
        return false;
    mc_item_stack *result = c->item_state.result_stack;
    c->state.same_stack = result == c->state.stack;
    if (c->state.same_stack) {
        if (result == NULL) {
            c->state.source_pc = 79;
            return true;
        }
        c->error.stack = mc_item_stack_source_count_get(result, &c->state.count_after);
        if (c->error.stack != MC_ITEM_STACK_OK)
            return fail(c, MC_SINGLEPLAYER_USE_STACK, "Use current returned Stack count", 27, true);
        c->state.count_compared = true;
        if (c->state.count_after == c->state.count_before) {
            c->state.source_pc = 79;
            return true;
        }
    }
    if (!slot_store(c, result, false))
        return false;
    if (result == NULL)
        return null_reference(c, "OB use actual returned Stack count receiver after store", 54);
    c->error.stack = mc_item_stack_source_count_get(result, &c->state.count_after);
    if (c->error.stack != MC_ITEM_STACK_OK)
        return fail(c, MC_SINGLEPLAYER_USE_STACK, "Use returned Stack zero-count check", 54, true);
    if (c->state.count_after == 0 && !slot_store(c, NULL, true))
        return false;
    c->state.result = true;
    c->state.source_pc = 77;
    return true;
}

static mc_singleplayer_use_status admission(mc_singleplayer_use_error *out,
                                            mc_singleplayer_use_status status,
                                            const char *operation) {
    if (out != NULL)
        *out = (mc_singleplayer_use_error){.status = status,
                                           .operation = operation,
                                           .native_policy = status != MC_SINGLEPLAYER_USE_OK};
    return status;
}

static mc_singleplayer_use_status
begin(mc_client_singleplayer_controller *controller, mc_entity *player, mc_world_runtime *world,
      mc_item_stack *stack, bool block, int32_t x, int32_t y, int32_t z, int32_t face,
      const mc_singleplayer_use_context *context, mc_singleplayer_use_call **out,
      mc_singleplayer_use_error *error) {
    if (controller == NULL || out == NULL || *out != NULL ||
        controller->fields.phase != MC_SINGLEPLAYER_CONTROLLER_READY)
        return admission(error, MC_SINGLEPLAYER_USE_ARGUMENT,
                         "Use canonical READY controller/output");
    if (controller->fields.entered)
        return admission(error, MC_SINGLEPLAYER_USE_BUSY, "Use entered controller");
    mc_singleplayer_use_call *c = calloc(1, sizeof(*c));
    if (c == NULL)
        return admission(error, MC_SINGLEPLAYER_USE_ALLOCATION,
                         "Use retained controller-call allocation");
    if (mc_client_singleplayer_controller_retain(controller) != MC_SINGLEPLAYER_CONTROLLER_OK) {
        free(c);
        return admission(error, MC_SINGLEPLAYER_USE_LIMIT, "Use controller occurrence limit");
    }
    c->state = (mc_singleplayer_use_state){.controller = controller,
                                           .player = player,
                                           .world = world,
                                           .stack = stack,
                                           .on_block = block,
                                           .clicked_x = x,
                                           .clicked_y = y,
                                           .clicked_z = z,
                                           .x = x,
                                           .y = y,
                                           .z = z,
                                           .face = face,
                                           .active = true,
                                           .error = &c->error,
                                           .cleanup = &c->cleanup};
    if (context != NULL) {
        if (context->activation != NULL)
            c->activation_context = *context->activation;
        c->math = context->math;
    }
    *out = c;
    controller->fields.entered = true;
    c->state.returned = block ? on_block(c) : in_air(c);
    controller->fields.entered = false;
    c->state.active = false;
    if (error != NULL)
        *error = c->error;
    return c->error.status;
}

mc_singleplayer_use_status mc_client_singleplayer_use_on_block_begin(
    mc_client_singleplayer_controller *c, mc_entity *p, mc_world_runtime *w, mc_item_stack *s,
    int32_t x, int32_t y, int32_t z, int32_t face, mc_singleplayer_use_call **out,
    mc_singleplayer_use_error *e) {
    return begin(c, p, w, s, true, x, y, z, face, NULL, out, e);
}

mc_singleplayer_use_status mc_client_singleplayer_use_on_block_with_context_begin(
    mc_client_singleplayer_controller *c, mc_entity *p, mc_world_runtime *w, mc_item_stack *s,
    int32_t x, int32_t y, int32_t z, int32_t face, const mc_world_block_activation_context *context,
    mc_singleplayer_use_call **out, mc_singleplayer_use_error *e) {
    const mc_singleplayer_use_context sources = {.activation = context};
    return begin(c, p, w, s, true, x, y, z, face, &sources, out, e);
}

mc_singleplayer_use_status mc_client_singleplayer_use_on_block_with_sources_begin(
    mc_client_singleplayer_controller *c, mc_entity *p, mc_world_runtime *w, mc_item_stack *s,
    int32_t x, int32_t y, int32_t z, int32_t face, const mc_singleplayer_use_context *context,
    mc_singleplayer_use_call **out, mc_singleplayer_use_error *e) {
    return begin(c, p, w, s, true, x, y, z, face, context, out, e);
}

mc_singleplayer_use_status mc_client_singleplayer_use_in_air_begin(
    mc_client_singleplayer_controller *c, mc_entity *p, mc_world_runtime *w, mc_item_stack *s,
    mc_singleplayer_use_call **out, mc_singleplayer_use_error *e) {
    return begin(c, p, w, s, false, 0, 0, 0, 0, NULL, out, e);
}

bool mc_client_singleplayer_use_inspect(const mc_singleplayer_use_call *c,
                                        mc_singleplayer_use_state *out) {
    if (c == NULL || out == NULL)
        return false;
    *out = c->state;
    return true;
}

static mc_singleplayer_use_status cleanup_failure(mc_singleplayer_use_call *c,
                                                  mc_singleplayer_use_error *out,
                                                  const char *operation) {
    c->cleanup.status = MC_SINGLEPLAYER_USE_CLEANUP;
    c->cleanup.operation = operation;
    c->cleanup.native_policy = true;
    if (out != NULL)
        *out = c->cleanup;
    return MC_SINGLEPLAYER_USE_CLEANUP;
}

mc_singleplayer_use_status mc_client_singleplayer_use_retire(mc_singleplayer_use_call **slot,
                                                             mc_singleplayer_use_error *error) {
    if (slot == NULL)
        return admission(error, MC_SINGLEPLAYER_USE_ARGUMENT, "Use retirement slot");
    mc_singleplayer_use_call *c = *slot;
    if (c == NULL)
        return admission(error, MC_SINGLEPLAYER_USE_OK, NULL);
    if (c->state.active || c->state.controller->fields.entered)
        return admission(error, MC_SINGLEPLAYER_USE_BUSY, "Use entered retirement");
    if (!query_capture(c))
        return cleanup_failure(c, error, "Use query producer capture retirement");
    if (c->item != NULL) {
        if (mc_item_use_retire(&c->item, &c->item_cleanup) != MC_ITEM_USE_OK) {
            c->cleanup.item = &c->item_cleanup;
            return cleanup_failure(c, error, "Use Item child checked retirement");
        }
        c->error.item = c->cleanup.item = NULL;
        c->state.item = NULL;
        c->state.result_stack = NULL;
    }
    if (!mc_world_placement_error_retire(&c->activation)) {
        c->cleanup.activation = &c->activation;
        return cleanup_failure(c, error, "Use activation checked retirement");
    }
    c->error.activation = c->cleanup.activation = NULL;
    if (c->registry != NULL) {
        if (mc_registry_classes_release(c->registry, &c->cleanup.registry) != MC_REGISTRY_OK)
            return cleanup_failure(c, error, "Use Registry occurrence retirement");
        c->registry = NULL;
    }
    for (size_t i = 0; i < c->receipt_count; ++i)
        mc_world_access_failure_release(&c->receipts[i]);
    c->receipt_count = 0;
    if (c->world_held) {
        mc_world_runtime_binding_release(c->state.world);
        c->world_held = false;
    }
    if (c->player_held) {
        mc_entity_release(c->state.player);
        c->player_held = false;
    }
    if (c->stack_held) {
        mc_item_stack_release(c->state.stack);
        c->stack_held = false;
    }
    if (mc_client_singleplayer_controller_release(c->state.controller) !=
        MC_SINGLEPLAYER_CONTROLLER_OK)
        return cleanup_failure(c, error, "Use controller occurrence retirement");
    free(c->source);
    free(c->acquisition);
    free(c);
    *slot = NULL;
    return admission(error, MC_SINGLEPLAYER_USE_OK, NULL);
}
