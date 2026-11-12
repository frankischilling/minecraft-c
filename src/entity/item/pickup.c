#include "mc/entity/item/pickup.h"
#include "../../block/registry/registry_internal.h"
#include "../../block/registry/runtime_internal.h"
#include "../../item/stack/source_internal.h"
#include "../../world/runtime/runtime_internal.h"
#include "../entity_internal.h"
#include "../player/player_internal.h"
#include "../player/statistics_status_internal.h"
#include "mc/block/registry/runtime.h"
#include "mc/item/registry/runtime.h"
#include <float.h>
#include <string.h>
#if !defined(__SSE2__) && !defined(_M_X64)
#error "Item pickup requires x86-64/SSE2"
#endif
#if defined(__FAST_MATH__) || (defined(__FINITE_MATH_ONLY__) && __FINITE_MATH_ONLY__ > 0) ||       \
    (defined(_M_FP_FAST) && _M_FP_FAST)
#error "Item pickup requires strict floating transformations"
#endif
#include <emmintrin.h>
_Static_assert(FLT_RADIX == 2 && FLT_MANT_DIG == 24 && FLT_EVAL_METHOD == 0,
               "Item pickup requires binary32 evaluation");

void mc_item_pickup_error_init(mc_item_pickup_error *e) {
    if (e == NULL)
        return;
    memset(e, 0, sizeof(*e));
    mc_inventory_insert_error_init(&e->insertion);
    mc_world_movement_error_init(&e->sound);
    e->registry = e->insertion.registry;
    e->statistic.writer.index = e->statistic.writer.text_offset = SIZE_MAX;
}

static void at(mc_item_pickup_error *e, unsigned int pc) {
    e->source_class = "hl";
    e->source_method = "b(Lgs;)V";
    e->bytecode = e->pickup_bytecode = pc;
}

static bool fail(mc_item_pickup_error *e, mc_entity_status status, const char *op) {
    e->status = status;
    e->operation = op;
    return false;
}

static bool registry(mc_item_pickup_error *e, mc_registry_status status) {
    return status == MC_REGISTRY_OK ||
           fail(e,
                status == MC_REGISTRY_LIMIT               ? MC_ENTITY_LIMIT
                : status == MC_REGISTRY_ALLOCATION_FAILED ? MC_ENTITY_ALLOCATION_FAILED
                : status == MC_REGISTRY_BUSY              ? MC_ENTITY_BUSY
                                                          : MC_ENTITY_STATE_ERROR,
                "Item pickup actual code Registry operation");
}

static bool code(mc_entity *item, mc_registry_classes *classes, mc_item_pickup_error *e) {
    return (classes != NULL && classes->entities == item->classes) ||
           fail(e, MC_ENTITY_STATE_ERROR, "Item pickup actual code Registry namespace");
}

static bool fields(mc_entity *item, mc_item_stack_fields *out, mc_item_pickup_error *e) {
    if (item->stack == NULL)
        return fail(e, MC_ENTITY_STATE_ERROR, "Item pickup null current Stack receiver");
    e->stack = mc_item_stack_fields_get(item->stack, out);
    return e->stack == MC_ITEM_STACK_OK ||
           fail(e, MC_ENTITY_STATE_ERROR, "Item pickup actual Stack field");
}

static bool award(mc_entity *player, mc_registry_classes *classes, size_t index,
                  mc_item_pickup_error *e) {
    if (!registry(e, mc_registry_classes_initialize_achievements(classes, &e->registry)))
        return false;
    const mc_statistic *achievement = classes->achievements.named[index];
    at(e, index == 1 ? 60u : 83u);
    const mc_player_statistic_status status =
        mc_local_player_add_statistic(player, achievement, 1, &e->statistic);
    e->statistic_bytecode = e->statistic.site.pc;
    return status == MC_PLAYER_STATISTIC_OK ||
           fail(e, mc_player_statistic_entity_status(&e->statistic),
                "Item pickup actual Player achievement descendant");
}

static bool notification(mc_entity *player, mc_entity *picked, mc_item_pickup_error *e) {
    const mc_entity_status status =
        mc_local_player_pickup_notification(player, picked, e->captured_count, &e->notification);
    if (status == MC_ENTITY_OK)
        return true;
    e->source_class = "dc";
    e->source_method = "b(Lsn;I)V";
    e->bytecode = e->notification_bytecode = e->notification.notification_pc;
    return fail(e, status, "Item pickup actual LocalPlayer notification descendant");
}

static bool body(mc_entity *item, mc_entity *player, mc_registry_classes *classes,
                 const mc_inventory_insert_limits *limits, bool *player_held,
                 mc_item_pickup_error *e) {
    at(e, 4);
    if (item->base.world == NULL)
        return fail(e, MC_ENTITY_STATE_ERROR, "Item pickup null World B receiver");
    if (item->base.world->fields.B)
        return true;
    at(e, 15);
    mc_item_stack_fields f;
    if (!fields(item, &f, e))
        return false;
    e->captured_count = f.count;
    if (item->item.c != 0)
        return true;
    at(e, 27);
    if (player == NULL)
        return fail(e, MC_ENTITY_STATE_ERROR, "Item pickup null Player Inventory receiver");
    if (player->classes != item->classes || player->phase != MC_ENTITY_READY ||
        !mc_entity_is_kind(player, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER) ||
        player->local_player == NULL)
        return fail(e, MC_ENTITY_STATE_ERROR, "Item pickup actual canonical Player owner");
    const mc_entity_status held = mc_entity_retain(player);
    if (held != MC_ENTITY_OK)
        return fail(e, held, "Item pickup local Player hold");
    *player_held = true;
    mc_inventory_player *inventory = player->local_player->fields.player.c;
    mc_item_stack *incoming = item->stack; /* source argument a31 before invoke34 */
    at(e, 34);
    if (inventory == NULL)
        return fail(e, MC_ENTITY_STATE_ERROR, "Item pickup null Inventory insertion receiver");
    const mc_entity_status inserted = mc_inventory_player_insert(
        inventory, incoming, classes, limits, &e->inserted, &e->insertion);
    if (inserted != MC_ENTITY_OK)
        return fail(e, inserted, "Item pickup actual Inventory insertion descendant");
    e->insertion_returned = true;
    if (!e->inserted)
        return true;
    at(e, 44);
    if (!fields(item, &f, e))
        return false;
    const int32_t first_id = f.id;
    at(e, 47);
    if (!code(item, classes, e))
        return false;
    const mc_block *wood;
    if (!registry(e, mc_registry_block_static_get(classes, "K", &wood, &e->registry)))
        return false;
    at(e, 50);
    const mc_block_fields *wood_fields = mc_block_fields_get(wood);
    if (wood_fields == NULL)
        return fail(e, MC_ENTITY_STATE_ERROR, "Item pickup null Block K bn receiver");
    if (first_id == wood_fields->bn) {
        at(e, 57);
        if (!award(player, classes, 1, e))
            return false;
    }
    at(e, 67);
    if (!fields(item, &f, e))
        return false;
    const int32_t second_id = f.id;
    at(e, 70);
    const mc_item *special_item;
    if (!registry(e, mc_registry_item_static_get(classes, "aD", &special_item, &e->registry)))
        return false;
    at(e, 73);
    const mc_item_fields *special_fields = mc_item_fields_get(special_item);
    if (special_fields == NULL)
        return fail(e, MC_ENTITY_STATE_ERROR, "Item pickup null Item aD bf receiver");
    if (second_id == special_fields->bf) {
        at(e, 80);
        if (!award(player, classes, 14, e))
            return false;
    }
    mc_world_runtime *world = item->base.world; /* actual receiver87 before ldc91 */
    at(e, 91);
    const mc_registry_string *name;
    if (!registry(e, mc_registry_runtime_literal_ascii(classes, "random.pop", &name, &e->registry)))
        return false;
    at(e, 99);
    if (item->random == NULL)
        return fail(e, MC_ENTITY_STATE_ERROR, "Item pickup null first Entity Random receiver");
    const float first = mc_java_random_next_float(item->random);
    at(e, 106);
    if (item->random == NULL)
        return fail(e, MC_ENTITY_STATE_ERROR, "Item pickup null second Entity Random receiver");
    const float second = mc_java_random_next_float(item->random);
    at(e, 109);
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u)
        return fail(e, MC_ENTITY_UNSUPPORTED_FP, "Item pickup pitch FP admission");
    const __m128 difference = _mm_sub_ss(_mm_set_ss(first), _mm_set_ss(second));
    const __m128 scaled = _mm_mul_ss(difference, _mm_set_ss(.7f));
    const __m128 shifted = _mm_add_ss(scaled, _mm_set_ss(1.0f));
    const float pitch = _mm_cvtss_f32(_mm_mul_ss(shifted, _mm_set_ss(2.0f)));
    at(e, 117);
    const mc_world_storage_status sound =
        mc_world_runtime_entity_sound(world, item, name, .2f, pitch, &e->sound);
    if (sound != MC_WORLD_STORAGE_OK)
        return fail(e,
                    sound == MC_WORLD_STORAGE_LIMIT               ? MC_ENTITY_LIMIT
                    : sound == MC_WORLD_STORAGE_ALLOCATION_FAILED ? MC_ENTITY_ALLOCATION_FAILED
                    : sound == MC_WORLD_STORAGE_BUSY              ? MC_ENTITY_BUSY
                                                                  : MC_ENTITY_WORLD_FAILED,
                    "Item pickup actual World entity sound descendant");
    at(e, 123);
    if (!notification(player, item, e))
        return false;
    /* The current Stack is reloaded only after the real notification returns. */
    at(e, 130);
    if (!fields(item, &f, e))
        return false;
    if (f.count <= 0)
        item->base.be = true; /* Real inherited sn.K literal store2. */
    return true;
}

mc_entity_status mc_item_entity_pickup(mc_entity *item, mc_entity *player,
                                       mc_registry_classes *classes,
                                       const mc_inventory_insert_limits *limits,
                                       mc_item_pickup_error *out) {
    mc_item_pickup_error frame;
    mc_item_pickup_error_init(&frame);
    bool held = false, player_held = false, registry_held = false;
    if (item == NULL)
        fail(&frame, MC_ENTITY_INVALID_ARGUMENT, "Item pickup receiver");
    else if (item->phase != MC_ENTITY_READY)
        fail(&frame, MC_ENTITY_INVALID_PHASE, "Item pickup READY native owner");
    else if (!mc_entity_is_kind(item, MC_ENTITY_CLASS_KIND_ITEM) ||
             item->classes->source != MC_ENTITY_CLIENT)
        fail(&frame, MC_ENTITY_STATE_ERROR, "Item pickup actual client Item kind");
    else if (classes != NULL && classes->entities != item->classes)
        fail(&frame, MC_ENTITY_STATE_ERROR, "Item pickup native code namespace admission");
    else {
        const mc_entity_status status = mc_entity_retain(item);
        if (status != MC_ENTITY_OK)
            fail(&frame, status, "Item pickup lifetime hold");
        else {
            held = true;
            if (classes != NULL)
                registry_held = registry(
                    &frame, mc_registry_classes_retain(classes, MC_REGISTRY_CLIENT,
                                                       item->classes->profile, &frame.registry));
            if (frame.status == MC_ENTITY_OK)
                (void)body(item, player, classes, limits, &player_held, &frame);
        }
    }
    if (registry_held && !mc_registry_runtime_borrow_release(classes))
        fail(&frame, MC_ENTITY_STATE_ERROR, "Item pickup Registry borrow retirement");
    if (player_held)
        mc_entity_release(player);
    if (held)
        mc_entity_release(item);
    if (frame.status == MC_ENTITY_OK)
        mc_item_pickup_error_init(&frame);
    if (out != NULL)
        *out = frame;
    return frame.status;
}
