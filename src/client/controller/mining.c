#include "mc/client/controller/mining.h"
#include "singleplayer_internal.h"

#include "../../block/registry/registry_internal.h"
#include "../../block/registry/runtime_internal.h"
#include "../../entity/player/player_internal.h"
#include "../../inventory/internal.h"
#include "../../inventory/player/source_internal.h"
#include "../../item/stack/source_internal.h"
#include "../../world/runtime/mutation/mutation_internal.h"
#include "../application_state.h"
#include "mc/block/registry/runtime.h"
#include "mc/block/registry/step_sound.h"
#include "mc/client/sound/sound_manager.h"
#include "mc/core/java/java.h"
#include "mc/entity/player/statistics.h"
#include "mc/item/mining/destroy.h"
#include "mc/statistics/registry/registry.h"
#include "mc/world/runtime/auxiliary.h"
#include "mc/world/runtime/blocks.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <xmmintrin.h>

struct mc_singleplayer_mining_call {
    mc_singleplayer_mining_state state;
    mc_singleplayer_mining_error error;
    mc_player_mining_error player;
    mc_inventory_current_error inventory;
    mc_world_auxiliary_error auxiliary;
    mc_world_mutation_error *mutation;
    mc_player_statistic_error statistic;
    mc_sound_error sound;
    mc_sound_manager_report sound_report;
    mc_step_sound_name sound_name;
    mc_item_stack *held_stack;
    mc_item_mining_destroy_error tool;
    mc_inventory_error slots;
};

static bool failed(mc_singleplayer_mining_call *call, mc_singleplayer_mining_status status,
                   const char *operation, const char *owner, const char *method, unsigned int pc,
                   bool native_policy) {
    call->state.status = status;
    call->state.source_pc = pc;
    call->error.status = status;
    call->error.operation = operation;
    call->error.site = (mc_player_site){owner, method, "(IIII)V", pc};
    if (strcmp(method, "b") == 0)
        call->error.site.descriptor = "(IIII)Z";
    if (strcmp(owner, "uu") == 0 && strcmp(method, "a") == 0)
        call->error.site.descriptor = "(Lfd;Lgs;IIII)V";
    call->error.native_policy = native_policy;
    return false;
}

static bool floating(mc_singleplayer_mining_call *call, const char *owner, const char *method,
                     unsigned int pc) {
    return (_mm_getcsr() & 0xffc0u) == 0x1f80u ||
           failed(call, MC_SINGLEPLAYER_MINING_FLOATING, "singleplayer mining floating environment",
                  owner, method, pc, true);
}

static float add(float a, float b) {
    return _mm_cvtss_f32(_mm_add_ss(_mm_set_ss(a), _mm_set_ss(b)));
}

static float divide(float a, float b) {
    return _mm_cvtss_f32(_mm_div_ss(_mm_set_ss(a), _mm_set_ss(b)));
}

static float integer_float(int32_t value) {
    return _mm_cvtss_f32(_mm_cvtsi32_ss(_mm_setzero_ps(), value));
}

static client_application *app(mc_singleplayer_mining_call *call, const char *owner,
                               const char *method, unsigned int pc) {
    client_application *value = call->state.controller->fields.a;
    if (value == NULL)
        (void)failed(call, MC_SINGLEPLAYER_MINING_SOURCE_NULL,
                     "actual mining null App field receiver", owner, method, pc, false);
    return value;
}

static mc_world_runtime *world(mc_singleplayer_mining_call *call, const char *owner,
                               const char *method, unsigned int field_pc, unsigned int call_pc) {
    client_application *value = app(call, owner, method, field_pc);
    if (value == NULL)
        return NULL;
    if (value->active_world == NULL)
        (void)failed(call, MC_SINGLEPLAYER_MINING_SOURCE_NULL,
                     "actual mining null World invocation", owner, method, call_pc, false);
    return value->active_world;
}

static bool block_get(mc_singleplayer_mining_call *call, client_application *value, int32_t id,
                      const mc_block **out, const char *owner, const char *method,
                      unsigned int pc) {
    if (mc_registry_block_get(value->registry, id, out, &call->error.registry) == MC_REGISTRY_OK)
        return true;
    return failed(call, MC_SINGLEPLAYER_MINING_REGISTRY, call->error.registry.message, owner,
                  method, pc,
                  call->error.registry.status != MC_REGISTRY_NULL_REFERENCE &&
                      call->error.registry.status != MC_REGISTRY_INDEX_OUT_OF_RANGE);
}

static bool query(mc_singleplayer_mining_call *call, mc_world_runtime *target, bool metadata,
                  int32_t *out, const char *owner, const char *method, unsigned int pc) {
    if (target == NULL)
        return failed(call, MC_SINGLEPLAYER_MINING_SOURCE_NULL,
                      "actual mining null World invocation", owner, method, pc, false);
    const mc_world_storage_status status =
        metadata ? mc_world_runtime_metadata_get(target, call->state.x, call->state.y,
                                                 call->state.z, out, &call->error.world)
                 : mc_world_runtime_block_get(target, call->state.x, call->state.y, call->state.z,
                                              out, &call->error.world);
    return status == MC_WORLD_STORAGE_OK ||
           failed(call, MC_SINGLEPLAYER_MINING_WORLD, call->error.world.operation, owner, method,
                  pc, true);
}

static bool strength(mc_singleplayer_mining_call *call, const mc_block *block, const char *method,
                     unsigned int player_pc, unsigned int invoke_pc, float *out) {
    client_application *value = app(call, "os", method, player_pc);
    if (value == NULL)
        return false;
    call->error.player = &call->player;
    if (mc_block_player_strength(block, value->h, out, &call->player) == MC_ENTITY_OK)
        return true;
    return failed(call, MC_SINGLEPLAYER_MINING_PLAYER, call->player.operation, "os", method,
                  invoke_pc, call->player.native_policy);
}

static bool initial_hook(mc_singleplayer_mining_call *call, const mc_block *block) {
    for (size_t depth = 0; depth != 64; ++depth) {
        if (block == NULL)
            return failed(call, MC_SINGLEPLAYER_MINING_SOURCE_NULL,
                          "actual initial Block hook null receiver", "os", "a", 74, false);
        switch (block->kind) {
            case MC_BLOCK_STAIRS:
                block = block->subtype.ss.a;
                continue;
            case MC_BLOCK_TNT:
            case MC_BLOCK_REDSTONE_ORE:
            case MC_BLOCK_DOOR:
            case MC_BLOCK_BUTTON:
            case MC_BLOCK_TRAPDOOR:
            case MC_BLOCK_NOTE_BLOCK:
            case MC_BLOCK_CAKE:
            case MC_BLOCK_LEVER:
                return failed(call, MC_SINGLEPLAYER_MINING_DEPENDENCY,
                              "actual concrete initial Block mining hook", "os", "a", 74, true);
            default:
                return true; /* Actual inherited UU.b(World,III,Player): return0. */
        }
    }
    return failed(call, MC_SINGLEPLAYER_MINING_LIMIT, "initial Stairs hook native frame extent",
                  "os", "a", 74, true);
}

static bool removed_hook(mc_singleplayer_mining_call *call, const mc_block *block) {
    for (size_t depth = 0; depth != 64; ++depth) {
        if (block->kind == MC_BLOCK_STAIRS) {
            block = block->subtype.ss.a;
            if (block == NULL)
                return failed(call, MC_SINGLEPLAYER_MINING_SOURCE_NULL,
                              "actual Stairs removal hook null prototype", "ob", "b", 92, false);
        } else if (block->kind == MC_BLOCK_TNT)
            return failed(call, MC_SINGLEPLAYER_MINING_DEPENDENCY,
                          "actual TNT removal-by-Player hook", "ob", "b", 92, true);
        else
            return true; /* Actual inherited UU.c(World,IIII): return0. */
    }
    return failed(call, MC_SINGLEPLAYER_MINING_LIMIT, "removal Stairs hook native frame extent",
                  "ob", "b", 92, true);
}

static bool mutation_frame(mc_singleplayer_mining_call *call, const char *owner, const char *method,
                           unsigned int pc) {
    if (call->mutation == NULL)
        call->mutation = malloc(sizeof(*call->mutation));
    if (call->mutation == NULL)
        return failed(call, MC_SINGLEPLAYER_MINING_ALLOCATION,
                      "mining reached mutation diagnostic allocation", owner, method, pc, true);
    mc_world_mutation_error_init(call->mutation);
    call->error.mutation = call->mutation;
    return true;
}

static bool base_destroy(mc_singleplayer_mining_call *call, bool *removed) {
    client_application *value = app(call, "ob", "b", 4);
    if (value == NULL)
        return false;
    mc_world_runtime *captured = value->active_world;
    mc_registry_block_m_view array;
    if (mc_registry_block_m_get(value->registry, &array, &call->error.registry) != MC_REGISTRY_OK)
        return failed(call, MC_SINGLEPLAYER_MINING_REGISTRY, call->error.registry.message, "ob",
                      "b", 9, true);
    int32_t id;
    if (!query(call, captured, false, &id, "ob", "b", 17))
        return false;
    const mc_block *block;
    if (mc_registry_block_m_at(&array, id, &block, &call->error.registry) != MC_REGISTRY_OK)
        return failed(call, MC_SINGLEPLAYER_MINING_REGISTRY, call->error.registry.message, "ob",
                      "b", 20, false);
    if (block == NULL)
        return failed(call, MC_SINGLEPLAYER_MINING_SOURCE_NULL,
                      "actual ob.b null Block.bn receiver", "ob", "b", 33, false);
    int32_t metadata;
    if (!query(call, captured, true, &metadata, "ob", "b", 41))
        return false;
    const int32_t packed = mc_java_i32_add(block->fields.bn, mc_java_i32_mul(metadata, 256));
    call->error.auxiliary = &call->auxiliary;
    if (mc_world_runtime_auxiliary_effect(captured, NULL, 2001, call->state.x, call->state.y,
                                          call->state.z, packed,
                                          &call->auxiliary) != MC_WORLD_STORAGE_OK)
        return failed(call, MC_SINGLEPLAYER_MINING_WORLD, call->auxiliary.operation, "ob", "b", 49,
                      call->auxiliary.native_policy);
    call->state.auxiliary_returned = true;
    if (!mc_world_auxiliary_error_retire(&call->auxiliary))
        return failed(call, MC_SINGLEPLAYER_MINING_BUSY, "mining auxiliary receipt retirement",
                      "ob", "b", 49, true);
    if (!query(call, captured, true, &metadata, "ob", "b", 57))
        return false;
    if (!mutation_frame(call, "ob", "b", 68))
        return false;
    if (mc_world_runtime_block_set(captured, call->state.x, call->state.y, call->state.z, 0,
                                   removed, call->mutation) != MC_WORLD_STORAGE_OK)
        return failed(call, MC_SINGLEPLAYER_MINING_WORLD, call->mutation->operation, "ob", "b", 68,
                      true);
    call->state.removal_returned = true;
    call->state.removed = *removed;
    if (*removed && !removed_hook(call, block))
        return false;
    return true;
}

static bool harvest(mc_singleplayer_mining_call *call, const mc_block *block,
                    mc_world_runtime *target, mc_entity *player) {
    if (block == NULL)
        return failed(call, MC_SINGLEPLAYER_MINING_SOURCE_NULL,
                      "actual OS harvest null Block receiver", "os", "b", 158, false);
    switch (block->kind) {
        case MC_BLOCK_LEAVES:
        case MC_BLOCK_SNOW_LAYER:
        case MC_BLOCK_ICE:
            return failed(call, MC_SINGLEPLAYER_MINING_DEPENDENCY,
                          "actual concrete Block harvest override", "os", "b", 158, true);
        default:
            break;
    }
    /* Client VG (Log) only invokespecial UU.a at10, then returns13. It enters
     * this same body with the same Block receiver and all unchanged arguments.
     * FK (Glass) declares no harvest method and inherits UU directly. */
    if (mc_registry_classes_initialize_statistics(block->classes, &call->error.registry) !=
        MC_REGISTRY_OK)
        return failed(call, MC_SINGLEPLAYER_MINING_REGISTRY, call->error.registry.message, "uu",
                      "a", 1, true);
    mc_registry_statistic_view statistics;
    if (mc_registry_classes_inspect_statistics(block->classes, &statistics,
                                               &call->error.registry) != MC_REGISTRY_OK)
        return failed(call, MC_SINGLEPLAYER_MINING_REGISTRY, call->error.registry.message, "uu",
                      "a", 1, true);
    const int32_t id = block->fields.bn;
    if (statistics.C == NULL || id < 0 || id >= 256)
        return failed(call,
                      statistics.C == NULL ? MC_SINGLEPLAYER_MINING_SOURCE_NULL
                                           : MC_SINGLEPLAYER_MINING_REGISTRY,
                      "actual mined statistic array access", "uu", "a", 8, false);
    if (player == NULL)
        return failed(call, MC_SINGLEPLAYER_MINING_SOURCE_NULL,
                      "actual Block harvest null Player statistic receiver", "uu", "a", 10, false);
    call->error.statistic = &call->statistic;
    if (mc_local_player_add_statistic(player, statistics.C[id], 1, &call->statistic) !=
        MC_PLAYER_STATISTIC_OK)
        return failed(call, MC_SINGLEPLAYER_MINING_PLAYER, call->statistic.operation, "uu", "a", 10,
                      true);
    call->state.statistic_returned = true;
    if (!mutation_frame(call, "uu", "a", 22))
        return false;
    if (target == NULL)
        return failed(call, MC_SINGLEPLAYER_MINING_SOURCE_NULL,
                      "actual Block drop null World receiver", "uu", "a", 22, false);
    if (mc_world_runtime_call_enter(target, &call->error.world) != MC_WORLD_STORAGE_OK)
        return failed(call, MC_SINGLEPLAYER_MINING_WORLD, call->error.world.operation, "uu", "a",
                      22, true);
    const bool done = mc_world_mutation_drop(target, block, call->state.x, call->state.y,
                                             call->state.z, call->state.metadata, call->mutation);
    mc_world_runtime_call_leave(target);
    if (!done)
        return failed(call, MC_SINGLEPLAYER_MINING_WORLD, call->mutation->operation, "uu", "a", 22,
                      true);
    call->state.drop_returned = true;
    return true;
}

static bool clear_held(mc_singleplayer_mining_call *call, mc_entity *player) {
    if (player == NULL)
        return failed(call, MC_SINGLEPLAYER_MINING_SOURCE_NULL,
                      "actual os.b null fresh Player H receiver", "os", "b", 120, false);
    if (!mc_entity_is_kind(player, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER) ||
        player->local_player == NULL)
        return failed(call, MC_SINGLEPLAYER_MINING_PLAYER, "mining H canonical Player storage",
                      "os", "b", 120, true);
    /* GS.H captures its Inventory setter receiver before rereading its
     * current Inventory and selected field. Do not reuse earlier GS.G input. */
    mc_inventory_player *destination = player->local_player->fields.player.c;
    mc_inventory_player *current = player->local_player->fields.player.c;
    if (current == NULL)
        return failed(call, MC_SINGLEPLAYER_MINING_SOURCE_NULL,
                      "actual gs.H null Inventory selected-field receiver", "os", "b", 120, false);
    call->state.cleared_slot = current->selected;
    if (destination == NULL)
        return failed(call, MC_SINGLEPLAYER_MINING_SOURCE_NULL,
                      "actual gs.H null captured Inventory setter receiver", "os", "b", 120, false);
    call->error.slots = &call->slots;
    const mc_inventory_status status =
        mc_inventory_player_source_set(destination, call->state.cleared_slot, NULL, &call->slots);
    if (status != MC_INVENTORY_OK)
        return failed(call, MC_SINGLEPLAYER_MINING_PLAYER, call->slots.operation, "os", "b", 120,
                      call->slots.frame_count == 0 || (status != MC_INVENTORY_NULL_REFERENCE &&
                                                       status != MC_INVENTORY_INDEX_OUT_OF_BOUNDS));
    call->state.clear_returned = true;
    return true;
}

static bool destroy_held(mc_singleplayer_mining_call *call) {
    client_application *value = app(call, "os", "b", 87);
    if (value == NULL)
        return false;
    call->error.tool = &call->tool;
    if (mc_item_stack_mining_destroy(value->registry, call->held_stack, call->state.block_id,
                                     call->state.x, call->state.y, call->state.z, value->h,
                                     &call->tool) != MC_ENTITY_OK)
        return failed(call, MC_SINGLEPLAYER_MINING_PLAYER, call->tool.operation, "os", "b", 90,
                      call->tool.native_policy);
    call->state.tool_returned = true;
    call->state.item_used_returned = call->tool.used_statistic_returned;
    if (mc_item_stack_source_count_get(call->held_stack, &call->state.count_after) !=
        MC_ITEM_STACK_OK)
        return failed(call, MC_SINGLEPLAYER_MINING_PLAYER, "mining late captured Stack count read",
                      "os", "b", 95, true);
    /* OS uses ifne98, unlike the Player attack's count<=0 exhaustion test. */
    if (call->state.count_after != 0)
        return true;
    call->state.exhausted_stack = true;
    value = app(call, "os", "b", 107);
    if (value == NULL)
        return false;
    (void)value->h; /* IZ.a(GS)110 is the literal return and ignores this argument. */
    call->state.destroy_callback_returned = true;
    value = app(call, "os", "b", 117);
    return value != NULL && clear_held(call, value->h);
}

static bool destroy(mc_singleplayer_mining_call *call) {
    mc_world_runtime *target = world(call, "os", "b", 4, 10);
    if (target == NULL || !query(call, target, false, &call->state.block_id, "os", "b", 10))
        return false;
    target = world(call, "os", "b", 19, 25);
    if (target == NULL || !query(call, target, true, &call->state.metadata, "os", "b", 25))
        return false;
    bool removed;
    if (!base_destroy(call, &removed))
        return false;
    client_application *value = app(call, "os", "b", 45);
    if (value == NULL)
        return false;
    mc_entity *player = value->h;
    if (player == NULL)
        return failed(call, MC_SINGLEPLAYER_MINING_SOURCE_NULL,
                      "actual os.b null Player current-Stack receiver", "os", "b", 48, false);
    mc_local_player_snapshot snapshot;
    if (mc_local_player_get_snapshot(player, &snapshot) != MC_ENTITY_OK)
        return failed(call, MC_SINGLEPLAYER_MINING_PLAYER, "mining current-Stack canonical Player",
                      "os", "b", 48, true);
    const mc_item_stack *stack;
    call->error.inventory = &call->inventory;
    if (mc_inventory_player_current(snapshot.player.c, &stack, &call->inventory) != MC_INVENTORY_OK)
        return failed(call, MC_SINGLEPLAYER_MINING_PLAYER, call->inventory.operation, "os", "b", 48,
                      call->inventory.native_policy);
    if (stack != NULL) {
        if (mc_item_stack_retain((mc_item_stack *)stack) != MC_ITEM_STACK_OK)
            return failed(call, MC_SINGLEPLAYER_MINING_LIMIT, "mining captured Stack occurrence",
                          "os", "b", 48, true);
        call->held_stack = (mc_item_stack *)stack;
        call->state.held_stack_identity = (uintptr_t)stack;
    }
    value = app(call, "os", "b", 57);
    if (value == NULL)
        return false;
    const mc_block *block;
    if (!block_get(call, value, call->state.block_id, &block, "os", "b", 65))
        return false;
    player = value->h;
    call->error.player = &call->player;
    if (player == NULL)
        return failed(call, MC_SINGLEPLAYER_MINING_SOURCE_NULL,
                      "actual os.b null Player harvest receiver", "os", "b", 66, false);
    if (mc_local_player_can_harvest(player, block, &call->state.harvest_allowed, &call->player) !=
        MC_ENTITY_OK)
        return failed(call, MC_SINGLEPLAYER_MINING_PLAYER, call->player.operation, "os", "b", 66,
                      call->player.native_policy);
    if (call->held_stack != NULL && !destroy_held(call))
        return false;
    if (removed && call->state.harvest_allowed) {
        value = app(call, "os", "b", 143);
        if (value == NULL || !block_get(call, value, call->state.block_id, &block, "os", "b", 138))
            return false;
        target = value->active_world;
        value = app(call, "os", "b", 150);
        if (value == NULL || !harvest(call, block, target, value->h))
            return false;
    }
    call->state.removed = removed;
    return true;
}

static bool start(mc_singleplayer_mining_call *call) {
    mc_world_runtime *target = world(call, "os", "a", 4, 19);
    if (target == NULL)
        return false;
    client_application *value = app(call, "os", "a", 11);
    if (value == NULL)
        return false;
    bool extinguished;
    call->error.auxiliary = &call->auxiliary;
    if (mc_world_runtime_extinguish_fire(target, value->h, call->state.x, call->state.y,
                                         call->state.z, call->state.face, &extinguished,
                                         &call->auxiliary) != MC_WORLD_STORAGE_OK)
        return failed(call, MC_SINGLEPLAYER_MINING_WORLD, call->auxiliary.operation, "os", "a", 19,
                      call->auxiliary.native_policy);
    if (!mc_world_auxiliary_error_retire(&call->auxiliary))
        return failed(call, MC_SINGLEPLAYER_MINING_BUSY, "mining extinguish diagnostic retirement",
                      "os", "a", 19, true);
    target = world(call, "os", "a", 26, 32);
    int32_t id;
    if (target == NULL || !query(call, target, false, &id, "os", "a", 32))
        return false;
    call->state.block_id = id;
    mc_client_singleplayer_controller *c = call->state.controller;
    if (id > 0) {
        if (!floating(call, "os", "a", 47))
            return false;
        if (c->fields.f == 0.0f) {
            value = app(call, "os", "a", 61);
            const mc_block *block;
            if (value == NULL || !block_get(call, value, id, &block, "os", "a", 56))
                return false;
            if (!initial_hook(call, block))
                return false;
        }
        value = app(call, "os", "a", 92);
        const mc_block *block;
        if (value == NULL || !block_get(call, value, id, &block, "os", "a", 87) ||
            !strength(call, block, "a", 92, 95, &call->state.strength))
            return false;
        if (!floating(call, "os", "a", 99))
            return false;
        /* fcmpl99/iflt100 takes the return arm when strength is NaN. */
        if (call->state.strength >= 1.0f)
            return destroy(call);
    }
    return true;
}

static bool step_sound(mc_singleplayer_mining_call *call, const mc_block *block) {
    client_application *value = app(call, "os", "c", 113);
    if (value == NULL)
        return false;
    mc_sound_manager *manager = value->B;
    if (mc_registry_step_sound_name(block->classes, block->fields.by, false, &call->sound_name,
                                    &call->error.registry) != MC_REGISTRY_OK)
        return failed(call, MC_SINGLEPLAYER_MINING_REGISTRY, call->error.registry.message, "os",
                      "c", 121, true);
    if (!floating(call, "os", "c", 125))
        return false;
    const float x = add(integer_float(call->state.x), 0.5f);
    const float y = add(integer_float(call->state.y), 0.5f);
    const float z = add(integer_float(call->state.z), 0.5f);
    float volume, pitch;
    if (mc_registry_step_sound_volume(block->fields.by, &volume, &call->error.registry) !=
        MC_REGISTRY_OK)
        return failed(call, MC_SINGLEPLAYER_MINING_REGISTRY, call->error.registry.message, "os",
                      "c", 144, true);
    volume = divide(add(volume, 1.0f), 8.0f);
    if (mc_registry_step_sound_pitch(block->fields.by, &pitch, &call->error.registry) !=
        MC_REGISTRY_OK)
        return failed(call, MC_SINGLEPLAYER_MINING_REGISTRY, call->error.registry.message, "os",
                      "c", 157, true);
    pitch = _mm_cvtss_f32(_mm_mul_ss(_mm_set_ss(pitch), _mm_set_ss(0.5f)));
    if (manager == NULL)
        return failed(call, MC_SINGLEPLAYER_MINING_SOURCE_NULL,
                      "actual mining null SoundManager invocation", "os", "c", 163, false);
    mc_registry_string_view name;
    if (!mc_registry_string_read(call->sound_name.string, &name))
        return failed(call, MC_SINGLEPLAYER_MINING_REGISTRY,
                      "mining reached step sound String representation", "os", "c", 163, true);
    call->error.sound = &call->sound;
    call->error.sound_report = &call->sound_report;
    if (mc_sound_manager_effect(manager, (mc_sound_text){name.units, name.length, false}, x, y, z,
                                volume, pitch, &call->sound_report, &call->sound) != MC_SOUND_OK)
        return failed(call, MC_SINGLEPLAYER_MINING_SOUND, call->sound.operation, "os", "c", 163,
                      true);
    call->state.sound_returned = true;
    if (mc_registry_step_sound_name_release(&call->sound_name, &call->error.registry) !=
        MC_REGISTRY_OK)
        return failed(call, MC_SINGLEPLAYER_MINING_REGISTRY,
                      "mining returned step sound String retirement", "os", "c", 163, true);
    return true;
}

static bool progress(mc_singleplayer_mining_call *call) {
    mc_client_singleplayer_controller *c = call->state.controller;
    if (c->fields.i > 0) {
        c->fields.i = mc_java_i32_sub(c->fields.i, 1);
        call->state.source_pc = 17;
        return true;
    }
    if (call->state.x != c->fields.c || call->state.y != c->fields.d ||
        call->state.z != c->fields.e) {
        c->fields.f = 0.0f;
        c->fields.g = 0.0f;
        c->fields.h = 0.0f;
        c->fields.c = call->state.x;
        c->fields.d = call->state.y;
        c->fields.e = call->state.z;
        call->state.changed_coordinate = true;
        call->state.source_pc = 248;
        return true;
    }
    mc_world_runtime *target = world(call, "os", "c", 46, 52);
    int32_t id;
    if (target == NULL || !query(call, target, false, &id, "os", "c", 52))
        return false;
    call->state.block_id = id;
    if (id == 0) {
        call->state.source_pc = 62;
        return true;
    }
    client_application *value = app(call, "os", "c", 82);
    const mc_block *block;
    if (value == NULL || !block_get(call, value, id, &block, "os", "c", 68) ||
        !strength(call, block, "c", 82, 85, &call->state.strength) ||
        !floating(call, "os", "c", 88))
        return false;
    c->fields.f = add(c->fields.f, call->state.strength);
    call->state.progress_stored = true;
    if (!floating(call, "os", "c", 98))
        return false;
    if (fmodf(c->fields.h, 4.0f) == 0.0f && block != NULL && !step_sound(call, block))
        return false;
    if (!floating(call, "os", "c", 172))
        return false;
    c->fields.h = add(c->fields.h, 1.0f);
    /* fcmpl181/iflt182 skips destruction for either sign of NaN. */
    if (c->fields.f >= 1.0f) {
        if (!destroy(call))
            return false;
        c->fields.f = 0.0f;
        c->fields.g = 0.0f;
        c->fields.h = 0.0f;
        c->fields.i = 5;
        call->state.cooldown_stored = true;
    }
    call->state.source_pc = 248;
    return true;
}

static mc_singleplayer_mining_status admission_result(mc_singleplayer_mining_error *out,
                                                      mc_singleplayer_mining_status status,
                                                      const char *operation) {
    if (out != NULL)
        *out = (mc_singleplayer_mining_error){
            .status = status, .operation = operation, .native_policy = true};
    return status;
}

mc_singleplayer_mining_status
mc_client_singleplayer_mining_begin(mc_client_singleplayer_controller *c,
                                    mc_singleplayer_mining_action action, int32_t x, int32_t y,
                                    int32_t z, int32_t face, mc_singleplayer_mining_call **out,
                                    mc_singleplayer_mining_error *error) {
    if (c == NULL || out == NULL || *out != NULL ||
        (unsigned int)action > MC_SINGLEPLAYER_MINING_DESTROY ||
        c->fields.phase != MC_SINGLEPLAYER_CONTROLLER_READY)
        return admission_result(error, MC_SINGLEPLAYER_MINING_ARGUMENT,
                                "mining canonical READY controller/output/action");
    if (c->fields.entered)
        return admission_result(error, MC_SINGLEPLAYER_MINING_BUSY, "mining entered controller");
    mc_singleplayer_mining_call *call = calloc(1, sizeof(*call));
    if (call == NULL)
        return admission_result(error, MC_SINGLEPLAYER_MINING_ALLOCATION,
                                "mining retained call allocation");
    if (mc_client_singleplayer_controller_retain(c) != MC_SINGLEPLAYER_CONTROLLER_OK) {
        free(call);
        return admission_result(error, MC_SINGLEPLAYER_MINING_LIMIT,
                                "mining controller occurrence limit");
    }
    call->state = (mc_singleplayer_mining_state){.controller = c,
                                                 .action = action,
                                                 .x = x,
                                                 .y = y,
                                                 .z = z,
                                                 .face = face,
                                                 .active = true,
                                                 .error = &call->error};
    call->error.registry = mc_world_mutation_empty_registry();
    *out = call;
    c->fields.entered = true;
    const bool done = action == MC_SINGLEPLAYER_MINING_START      ? start(call)
                      : action == MC_SINGLEPLAYER_MINING_PROGRESS ? progress(call)
                                                                  : destroy(call);
    c->fields.entered = false;
    call->state.active = false;
    if (done) {
        call->state.returned = true;
        if (action != MC_SINGLEPLAYER_MINING_PROGRESS)
            call->state.source_pc = action == MC_SINGLEPLAYER_MINING_START ? 113u : 163u;
    }
    if (error != NULL)
        *error = call->error;
    return call->state.status;
}

bool mc_client_singleplayer_mining_inspect(const mc_singleplayer_mining_call *call,
                                           mc_singleplayer_mining_state *out) {
    if (call == NULL || out == NULL)
        return false;
    *out = call->state;
    return true;
}

mc_singleplayer_mining_status
mc_client_singleplayer_mining_retire(mc_singleplayer_mining_call **slot,
                                     mc_singleplayer_mining_error *error) {
    if (slot == NULL)
        return admission_result(error, MC_SINGLEPLAYER_MINING_ARGUMENT, "mining retirement slot");
    mc_singleplayer_mining_call *call = *slot;
    if (call == NULL)
        return admission_result(error, MC_SINGLEPLAYER_MINING_OK, NULL);
    if (call->state.active || call->state.controller->fields.entered)
        return admission_result(error, MC_SINGLEPLAYER_MINING_BUSY, "mining retirement entered");
    if (!mc_world_auxiliary_error_retire(&call->auxiliary))
        return admission_result(error, MC_SINGLEPLAYER_MINING_BUSY,
                                "mining auxiliary diagnostic retirement");
    if (call->sound_name.classes != NULL &&
        mc_registry_step_sound_name_release(&call->sound_name, &call->error.registry) !=
            MC_REGISTRY_OK)
        return admission_result(error, MC_SINGLEPLAYER_MINING_BUSY,
                                "mining step sound diagnostic occurrence retirement");
    if (mc_client_singleplayer_controller_release(call->state.controller) !=
        MC_SINGLEPLAYER_CONTROLLER_OK)
        return admission_result(error, MC_SINGLEPLAYER_MINING_BUSY,
                                "mining controller occurrence retirement");
    mc_item_stack_release(call->held_stack);
    free(call->mutation);
    free(call);
    *slot = NULL;
    return admission_result(error, MC_SINGLEPLAYER_MINING_OK, NULL);
}
