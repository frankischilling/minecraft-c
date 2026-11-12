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
#include "mc/item/registry/registry.h"
#include "mc/statistics/registry/registry.h"
#include "mc/world/runtime/auxiliary.h"
#include "mc/world/runtime/blocks.h"
#include "mc/world/runtime/placement.h"

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
    mc_world_placement_error activation;
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
    if ((strcmp(owner, "uu") == 0 || strcmp(owner, "bk") == 0 || strcmp(owner, "jr") == 0 ||
         strcmp(owner, "nk") == 0) &&
        strcmp(method, "a") == 0)
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

static bool initial_hook(mc_singleplayer_mining_call *call, const mc_block *block,
                         mc_world_runtime *target, mc_entity *player) {
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
                return failed(call, MC_SINGLEPLAYER_MINING_DEPENDENCY,
                              "actual concrete initial Block mining hook", "os", "a", 74, true);
            case MC_BLOCK_LEVER: {
                call->error.activation = &call->activation;
                call->error.initial_hook = (mc_player_site){"xr", "b", "(Lfd;IIILgs;)V", 8};
                bool ignored;
                if (mc_world_block_activate(block->classes, block, target, call->state.x,
                                            call->state.y, call->state.z, player, &ignored,
                                            &call->activation) != MC_WORLD_STORAGE_OK)
                    return failed(call, MC_SINGLEPLAYER_MINING_WORLD, call->activation.operation,
                                  "os", "a", 74, call->activation.native_policy);
                return true; /* XR.b discards the activation result, then returns12. */
            }
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

static bool harvest_statistic(mc_singleplayer_mining_call *call, const mc_block *block,
                              mc_entity *player, const char *owner, unsigned int static_pc,
                              unsigned int index_pc, unsigned int invoke_pc) {
    if (mc_registry_classes_initialize_statistics(block->classes, &call->error.registry) !=
        MC_REGISTRY_OK)
        return failed(call, MC_SINGLEPLAYER_MINING_REGISTRY, call->error.registry.message, owner,
                      "a", static_pc, true);
    mc_registry_statistic_view statistics;
    if (mc_registry_classes_inspect_statistics(block->classes, &statistics,
                                               &call->error.registry) != MC_REGISTRY_OK)
        return failed(call, MC_SINGLEPLAYER_MINING_REGISTRY, call->error.registry.message, owner,
                      "a", static_pc, true);
    const int32_t id = block->fields.bn;
    if (statistics.C == NULL || id < 0 || id >= 256)
        return failed(call,
                      statistics.C == NULL ? MC_SINGLEPLAYER_MINING_SOURCE_NULL
                                           : MC_SINGLEPLAYER_MINING_REGISTRY,
                      "actual mined statistic array access", owner, "a", index_pc, false);
    if (player == NULL)
        return failed(call, MC_SINGLEPLAYER_MINING_SOURCE_NULL,
                      "actual Block harvest null Player statistic receiver", owner, "a", invoke_pc,
                      false);
    call->error.statistic = &call->statistic;
    if (mc_local_player_add_statistic(player, statistics.C[id], 1, &call->statistic) !=
        MC_PLAYER_STATISTIC_OK)
        return failed(call, MC_SINGLEPLAYER_MINING_PLAYER, call->statistic.operation, owner, "a",
                      invoke_pc, true);
    call->state.statistic_returned = true;
    return true;
}

static bool harvest_base(mc_singleplayer_mining_call *call, const mc_block *block,
                         mc_world_runtime *target, mc_entity *player) {
    if (!harvest_statistic(call, block, player, "uu", 1, 8, 10))
        return false;
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

static bool harvest_current(mc_singleplayer_mining_call *call, mc_entity *player, unsigned int pc,
                            const mc_item_stack **out) {
    if (player == NULL)
        return failed(call, MC_SINGLEPLAYER_MINING_SOURCE_NULL,
                      "actual Leaves harvest null Player current-Stack receiver", "bk", "a", pc,
                      false);
    if (!mc_entity_is_kind(player, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER) ||
        player->local_player == NULL)
        return failed(call, MC_SINGLEPLAYER_MINING_PLAYER,
                      "Leaves harvest canonical Player storage", "bk", "a", pc, true);
    mc_inventory_player *inventory = player->local_player->fields.player.c;
    if (inventory == NULL)
        return failed(call, MC_SINGLEPLAYER_MINING_SOURCE_NULL,
                      "actual gs.G null Inventory receiver", "bk", "a", pc, false);
    call->error.inventory = &call->inventory;
    if (mc_inventory_player_current(inventory, out, &call->inventory) != MC_INVENTORY_OK)
        return failed(call, MC_SINGLEPLAYER_MINING_PLAYER, call->inventory.operation, "bk", "a", pc,
                      call->inventory.native_policy);
    return true;
}

static bool harvest_leaves(mc_singleplayer_mining_call *call, const mc_block *block,
                           mc_world_runtime *target, mc_entity *player) {
    if (target == NULL)
        return failed(call, MC_SINGLEPLAYER_MINING_SOURCE_NULL,
                      "actual Leaves harvest null World remote-field receiver", "bk", "a", 1,
                      false);
    if (target->fields.B)
        return harvest_base(call, block, target, player);
    const mc_item_stack *current;
    if (!harvest_current(call, player, 8, &current))
        return false;
    if (current == NULL)
        return harvest_base(call, block, target, player);
    /* The second actual GS.G follows the NULL test. In particular, neither
     * read borrows the captured Stack already damaged/cleared by OS. */
    if (!harvest_current(call, player, 15, &current))
        return false;
    if (current == NULL)
        return failed(call, MC_SINGLEPLAYER_MINING_SOURCE_NULL,
                      "actual Leaves harvest null second Stack ID receiver", "bk", "a", 18, false);
    int32_t id;
    if (mc_item_stack_source_id_get(current, &id) != MC_ITEM_STACK_OK)
        return failed(call, MC_SINGLEPLAYER_MINING_PLAYER, "Leaves harvest live Stack ID", "bk",
                      "a", 18, true);
    if (mc_registry_classes_initialize_items(block->classes, &call->error.registry) !=
        MC_REGISTRY_OK)
        return failed(call, MC_SINGLEPLAYER_MINING_REGISTRY, call->error.registry.message, "bk",
                      "a", 21, true);
    const mc_item *shears;
    if (mc_registry_classes_inspect_item_static(block->classes, "bc", &shears,
                                                &call->error.registry) != MC_REGISTRY_OK)
        return failed(call, MC_SINGLEPLAYER_MINING_REGISTRY, call->error.registry.message, "bk",
                      "a", 21, true);
    if (shears == NULL)
        return failed(call, MC_SINGLEPLAYER_MINING_SOURCE_NULL,
                      "actual Leaves harvest null static Shears ID receiver", "bk", "a", 24, false);
    if (id != mc_item_fields_get(shears)->bf)
        return harvest_base(call, block, target, player);
    if (!harvest_statistic(call, block, player, "bk", 31, 38, 40))
        return false;
    mc_item_stack *stack = NULL;
    if (mc_item_stack_allocate(&stack) != MC_ITEM_STACK_OK)
        return failed(call, MC_SINGLEPLAYER_MINING_ALLOCATION, "Leaves harvest Stack allocation",
                      "bk", "a", 50, true);
    const mc_block *leaves;
    if (mc_registry_block_static_get(block->classes, "L", &leaves, &call->error.registry) !=
        MC_REGISTRY_OK) {
        mc_item_stack_release(stack);
        return failed(call, MC_SINGLEPLAYER_MINING_REGISTRY, call->error.registry.message, "bk",
                      "a", 54, true);
    }
    if (leaves == NULL) {
        mc_item_stack_release(stack);
        return failed(call, MC_SINGLEPLAYER_MINING_SOURCE_NULL,
                      "actual Leaves harvest null static Leaves ID receiver", "bk", "a", 57, false);
    }
    const int32_t leaf_id = leaves->fields.bn;
    if (mc_item_stack_construct(stack, leaf_id, 1, call->state.metadata & 3) != MC_ITEM_STACK_OK) {
        mc_item_stack_release(stack);
        return failed(call, MC_SINGLEPLAYER_MINING_PLAYER,
                      "Leaves harvest numeric Stack constructor", "bk", "a", 65, true);
    }
    bool done = mutation_frame(call, "bk", "a", 68);
    if (done && mc_world_runtime_call_enter(target, &call->error.world) != MC_WORLD_STORAGE_OK)
        done = failed(call, MC_SINGLEPLAYER_MINING_WORLD, call->error.world.operation, "bk", "a",
                      68, true);
    else if (done) {
        done = mc_world_mutation_drop_stack(target, block, call->state.x, call->state.y,
                                            call->state.z, stack, call->mutation);
        mc_world_runtime_call_leave(target);
        if (!done)
            (void)failed(call, MC_SINGLEPLAYER_MINING_WORLD, call->mutation->operation, "bk", "a",
                         68, true);
    }
    mc_item_stack_release(stack);
    if (done)
        call->state.drop_returned = true;
    return done;
}

static bool harvest_set(mc_singleplayer_mining_call *call, mc_world_runtime *target, int32_t id,
                        const char *owner, unsigned int pc) {
    if (target == NULL)
        return failed(call, MC_SINGLEPLAYER_MINING_SOURCE_NULL,
                      "actual harvest null World notifying-setter receiver", owner, "a", pc, false);
    if (!mutation_frame(call, owner, "a", pc))
        return false;
    bool ignored;
    if (mc_world_runtime_block_set(target, call->state.x, call->state.y, call->state.z, id,
                                   &ignored, call->mutation) != MC_WORLD_STORAGE_OK)
        return failed(call, MC_SINGLEPLAYER_MINING_WORLD, call->mutation->operation, owner, "a", pc,
                      true);
    return true;
}

static bool harvest_snow(mc_singleplayer_mining_call *call, const mc_block *block,
                         mc_world_runtime *target, mc_entity *player) {
    if (!mutation_frame(call, "jr", "a", 0))
        return false;
    if (target != NULL &&
        mc_world_runtime_call_enter(target, &call->error.world) != MC_WORLD_STORAGE_OK)
        return failed(call, MC_SINGLEPLAYER_MINING_WORLD, call->error.world.operation, "jr", "a", 0,
                      true);
    const bool done = mc_world_mutation_snow_harvest_drop(
        target, block, call->state.x, call->state.y, call->state.z, call->mutation);
    if (target != NULL)
        mc_world_runtime_call_leave(target);
    if (!done)
        return failed(call, MC_SINGLEPLAYER_MINING_WORLD, call->mutation->operation, "jr", "a",
                      call->mutation->drop_bytecode, true);
    call->state.drop_returned = true;
    return harvest_set(call, target, 0, "jr", 140) &&
           harvest_statistic(call, block, player, "jr", 145, 152, 154);
}

static bool harvest(mc_singleplayer_mining_call *call, const mc_block *block,
                    mc_world_runtime *target, mc_entity *player) {
    if (block == NULL)
        return failed(call, MC_SINGLEPLAYER_MINING_SOURCE_NULL,
                      "actual OS harvest null Block receiver", "os", "b", 158, false);
    if (block->kind == MC_BLOCK_LEAVES)
        return harvest_leaves(call, block, target, player);
    if (block->kind == MC_BLOCK_SNOW_LAYER)
        return harvest_snow(call, block, target, player);
    /* VG's wrapper, FK and both NR/JP superclass resolutions enter UU with
     * the same receiver. Ice continues only after that body returns. */
    if (!harvest_base(call, block, target, player))
        return false;
    if (block->kind != MC_BLOCK_ICE)
        return true;
    mc_material *material;
    if (mc_world_runtime_material_get(target, call->state.x, mc_java_i32_sub(call->state.y, 1),
                                      call->state.z, &material,
                                      &call->error.world) != MC_WORLD_STORAGE_OK)
        return failed(call, MC_SINGLEPLAYER_MINING_WORLD, call->error.world.operation, "nk", "a",
                      21, true);
    if (material == NULL)
        return failed(call, MC_SINGLEPLAYER_MINING_SOURCE_NULL,
                      "actual Ice harvest null below Material receiver", "nk", "a", 28, false);
    if (!mc_material_c(material) && !mc_material_d(material))
        return true;
    const mc_block *water;
    if (mc_registry_block_static_get(block->classes, "B", &water, &call->error.registry) !=
        MC_REGISTRY_OK)
        return failed(call, MC_SINGLEPLAYER_MINING_REGISTRY, call->error.registry.message, "nk",
                      "a", 48, true);
    if (water == NULL)
        return failed(call, MC_SINGLEPLAYER_MINING_SOURCE_NULL,
                      "actual Ice harvest null flowing Water ID receiver", "nk", "a", 51, false);
    return harvest_set(call, target, water->fields.bn, "nk", 54);
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
            target = value->active_world;
            value = app(call, "os", "a", 71);
            if (value == NULL || !initial_hook(call, block, target, value->h))
                return false;
            call->state.initial_hook_returned = true;
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
    if (!mc_world_placement_error_retire(&call->activation))
        return admission_result(error, MC_SINGLEPLAYER_MINING_BUSY,
                                "mining initial Block activation diagnostic retirement");
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
