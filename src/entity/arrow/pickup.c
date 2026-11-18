#include "mc/entity/item/pickup.h"
#include "../../block/registry/registry_internal.h"
#include "../../block/registry/runtime_internal.h"
#include "../../world/runtime/runtime_internal.h"
#include "../player/player_internal.h"
#include "internal.h"
#include "mc/block/registry/runtime.h"
#include "mc/item/registry/runtime.h"
#include <emmintrin.h>

#if defined(__FAST_MATH__) || (defined(__FINITE_MATH_ONLY__) && __FINITE_MATH_ONLY__ > 0) ||       \
    (defined(_M_FP_FAST) && _M_FP_FAST)
#error "Arrow pickup requires strict floating transformations"
#endif

static void at(mc_item_pickup_error *error, unsigned int pc) {
    error->bytecode = error->pickup_bytecode = pc;
}

static mc_entity_status fail(mc_item_pickup_error *error, mc_entity_status status,
                             const char *operation) {
    error->status = status;
    error->operation = operation;
    return status;
}

static mc_entity_status from_registry(mc_registry_status status) {
    switch (status) {
        case MC_REGISTRY_LIMIT:
            return MC_ENTITY_LIMIT;
        case MC_REGISTRY_ALLOCATION_FAILED:
            return MC_ENTITY_ALLOCATION_FAILED;
        case MC_REGISTRY_BUSY:
            return MC_ENTITY_BUSY;
        default:
            return MC_ENTITY_STATE_ERROR;
    }
}

static mc_entity_status feedback(mc_entity *arrow, mc_entity *player, mc_registry_classes *classes,
                                 mc_item_pickup_error *error) {
    mc_world_runtime *const world = arrow->base.world;
    const mc_registry_string *name;
    at(error, 58);
    const mc_registry_status literal =
        mc_registry_runtime_literal_ascii(classes, "random.pop", &name, &error->registry);
    if (literal != MC_REGISTRY_OK)
        return fail(error, from_registry(literal), "Arrow pickup sound name");
    at(error, 66);
    if (arrow->random == NULL)
        return fail(error, MC_ENTITY_STATE_ERROR, "Arrow pickup first Random receiver");
    const float first = mc_java_random_next_float(arrow->random);
    at(error, 73);
    const float second = mc_java_random_next_float(arrow->random);
    at(error, 76);
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u)
        return fail(error, MC_ENTITY_UNSUPPORTED_FP, "Arrow pickup pitch FP admission");
    const __m128 difference = _mm_sub_ss(_mm_set_ss(first), _mm_set_ss(second));
    const __m128 scaled = _mm_mul_ss(difference, _mm_set_ss(.7f));
    const __m128 shifted = _mm_add_ss(scaled, _mm_set_ss(1.0f));
    const float pitch = _mm_cvtss_f32(_mm_mul_ss(shifted, _mm_set_ss(2.0f)));
    at(error, 84);
    const mc_world_storage_status played =
        mc_world_runtime_entity_sound(world, arrow, name, .2f, pitch, &error->sound);
    if (played != MC_WORLD_STORAGE_OK) {
        const mc_entity_status status = played == MC_WORLD_STORAGE_LIMIT ? MC_ENTITY_LIMIT
                                        : played == MC_WORLD_STORAGE_ALLOCATION_FAILED
                                            ? MC_ENTITY_ALLOCATION_FAILED
                                        : played == MC_WORLD_STORAGE_BUSY ? MC_ENTITY_BUSY
                                                                          : MC_ENTITY_WORLD_FAILED;
        return fail(error, status, "Arrow pickup World sound");
    }
    at(error, 90);
    const mc_entity_status notified =
        mc_local_player_pickup_notification(player, arrow, 1, &error->notification);
    if (notified != MC_ENTITY_OK) {
        error->source_class = "dc";
        error->source_method = "b(Lsn;I)V";
        error->bytecode = error->notification_bytecode = error->notification.notification_pc;
        return fail(error, notified, "Arrow pickup LocalPlayer notification");
    }
    at(error, 94);
    arrow->base.be = true;
    return MC_ENTITY_OK;
}

static mc_entity_status collect(mc_entity *arrow, mc_entity *player, mc_registry_classes *classes,
                                mc_item_stack *stack, mc_item_pickup_error *error) {
    mc_inventory_player *const inventory = player->local_player->fields.player.c;
    const mc_item *item;
    at(error, 40);
    const mc_registry_status registry =
        mc_registry_item_static_get(classes, "j", &item, &error->registry);
    if (registry != MC_REGISTRY_OK)
        return fail(error, from_registry(registry), "Arrow pickup live Item static");
    at(error, 44);
    const mc_item_fields *item_fields = mc_item_fields_get(item);
    if (item_fields == NULL)
        return fail(error, MC_ENTITY_STATE_ERROR, "Arrow pickup ItemStack Item receiver");
    error->stack = mc_item_stack_construct(stack, item_fields->bf, 1, 0);
    if (error->stack != MC_ITEM_STACK_OK)
        return fail(error, MC_ENTITY_ITEM_FAILED, "Arrow pickup ItemStack constructor");
    at(error, 47);
    if (inventory == NULL)
        return fail(error, MC_ENTITY_STATE_ERROR, "Arrow pickup Inventory receiver");
    error->captured_count = 1;
    const mc_entity_status inserted = mc_inventory_player_insert(
        inventory, stack, classes, NULL, &error->inserted, &error->insertion);
    if (inserted != MC_ENTITY_OK)
        return fail(error, inserted, "Arrow pickup Inventory insertion");
    error->insertion_returned = true;
    return error->inserted ? feedback(arrow, player, classes, error) : MC_ENTITY_OK;
}

mc_entity_status mc_arrow_contact_pickup(mc_entity *arrow, mc_entity *player,
                                         mc_registry_classes *classes,
                                         mc_item_pickup_error *error) {
    error->source_class = "sl";
    error->source_method = "b(Lgs;)V";
    if (!mc_entity_has_arrow(arrow) || arrow->phase != MC_ENTITY_READY || arrow->holds == 0)
        return fail(error, MC_ENTITY_INVALID_PHASE, "Arrow pickup held READY receiver");
    at(error, 4);
    if (arrow->base.world == NULL)
        return fail(error, MC_ENTITY_STATE_ERROR, "Arrow pickup null World receiver");
    const mc_arrow_fields *const fields = &arrow->arrow->fields;
    if (arrow->base.world->fields.B || !fields->i || !fields->a || fields->b > 0)
        return MC_ENTITY_OK;
    at(error, 33);
    if (player == NULL || player->classes != arrow->classes || player->phase != MC_ENTITY_READY ||
        player->holds == 0 || !mc_entity_is_kind(player, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER) ||
        player->local_player == NULL)
        return fail(error, MC_ENTITY_STATE_ERROR, "Arrow pickup held LocalPlayer receiver");
    at(error, 36);
    mc_item_stack *stack = NULL;
    error->stack = mc_item_stack_allocate(&stack);
    if (error->stack != MC_ITEM_STACK_OK)
        return fail(error, MC_ENTITY_ALLOCATION_FAILED, "Arrow pickup ItemStack allocation");
    at(error, 40);
    mc_entity_status status;
    if (classes == NULL || classes->entities != arrow->classes) {
        status = fail(error, MC_ENTITY_STATE_ERROR, "Arrow pickup Registry namespace");
    } else {
        const mc_registry_status retained = mc_registry_classes_retain(
            classes, MC_REGISTRY_CLIENT, arrow->classes->profile, &error->registry);
        if (retained != MC_REGISTRY_OK)
            status = fail(error, from_registry(retained), "Arrow pickup Registry hold");
        else {
            status = collect(arrow, player, classes, stack, error);
            if (!mc_registry_runtime_borrow_release(classes) && status == MC_ENTITY_OK)
                status = fail(error, MC_ENTITY_STATE_ERROR, "Arrow pickup Registry retirement");
        }
    }
    mc_item_stack_release(stack);
    return status;
}
