/* Genuine Registry/World fixture; its original entry point is not invoked. */
#define main mc_item_mining_unused_construction_main
#include "../../entity/player/test_construction.c"
#undef main

#include "../../../src/block/registry/registry_internal.h"
#include "../../../src/inventory/internal.h"
#include "../../../src/item/stack/source_internal.h"
#include "mc/inventory/mining.h"
#include "mc/item/mining/mining.h"

#include <inttypes.h>

static bool speed_text(mc_inventory_status status, float value, char out[16]) {
    if (status == MC_INVENTORY_NULL_REFERENCE)
        memcpy(out, "NULL", 5);
    else {
        CHECK(status == MC_INVENTORY_OK);
        uint32_t bits;
        memcpy(&bits, &value, sizeof(bits));
        CHECK(snprintf(out, 16, "%08" PRIx32, bits) == 8);
    }
    return true;
}

static bool harvest_text(mc_inventory_status status, bool value, char out[16]) {
    CHECK(status == MC_INVENTORY_OK || status == MC_INVENTORY_NULL_REFERENCE);
    memcpy(out,
           status == MC_INVENTORY_NULL_REFERENCE ? "NULL"
           : value                               ? "1"
                                                 : "0",
           status == MC_INVENTORY_NULL_REFERENCE ? 5u : 2u);
    return true;
}

static bool matrix(fixture *f) {
    const mc_registry_item_array *items = NULL;
    CHECK(mc_registry_item_c_get(f->registry, &items, NULL) == MC_REGISTRY_OK && items != NULL);
    mc_inventory_player *inventory = NULL;
    CHECK(mc_inventory_player_create(MC_REGISTRY_CLIENT, NULL, NULL, &inventory, NULL) ==
          MC_INVENTORY_OK);
    size_t item_count = 0, rows = 0;
    bool seen[MC_ITEM_KIND_COUNT] = {false};
    for (size_t id = 0; id < items->length; ++id) {
        const mc_item *item = items->elements[id];
        if (item == NULL)
            continue;
        ++item_count;
        seen[item->kind] = true;
        mc_item_stack *stack = NULL;
        CHECK(mc_item_stack_allocate(&stack) == MC_ITEM_STACK_OK &&
              mc_item_stack_construct(stack, (int32_t)id, -3, -7) == MC_ITEM_STACK_OK &&
              mc_inventory_player_set(inventory, 0, stack, NULL) == MC_INVENTORY_OK);
        for (int32_t block_id = -1; block_id < 256; ++block_id) {
            const mc_block *block = NULL;
            if (block_id >= 0) {
                CHECK(mc_registry_block_get(f->registry, block_id, &block, NULL) == MC_REGISTRY_OK);
                if (block == NULL)
                    continue;
            }
            char values[6][16];
            float speed = -777.0f;
            bool harvest = false;
            mc_inventory_status status =
                mc_item_mining_speed(f->registry, item, stack, block, &speed, NULL);
            CHECK(speed_text(status, speed, values[0]));
            status = mc_item_mining_harvest(f->registry, item, block, &harvest, NULL);
            CHECK(harvest_text(status, harvest, values[1]));
            status = mc_item_stack_mining_speed(f->registry, stack, block, &speed, NULL);
            CHECK(speed_text(status, speed, values[2]));
            status = mc_item_stack_mining_harvest(f->registry, stack, block, &harvest, NULL);
            CHECK(harvest_text(status, harvest, values[3]));
            status = mc_inventory_player_mining_speed(inventory, f->registry, block, &speed, NULL);
            CHECK(speed_text(status, speed, values[4]));
            status = mc_inventory_player_can_harvest(inventory, f->registry, block, &harvest, NULL);
            CHECK(harvest_text(status, harvest, values[5]));
            printf("MINING\t%zu\t%d\t%s\t%s\t%s\t%s\t%s\t%s\n", id, block_id, values[0], values[1],
                   values[2], values[3], values[4], values[5]);
            ++rows;
        }
        mc_item_stack_fields fields;
        CHECK(mc_item_stack_fields_get(stack, &fields) == MC_ITEM_STACK_OK &&
              fields.id == (int32_t)id && fields.count == -3 && fields.damage == -7 &&
              fields.animation == 0);
        mc_item_stack_release(stack);
    }
    for (size_t kind = 0; kind < MC_ITEM_KIND_COUNT; ++kind)
        CHECK(seen[kind]);
    CHECK(item_count == 202 && rows == 19594 &&
          mc_inventory_player_retire(&inventory, NULL) == MC_INVENTORY_OK);
    printf("TOTAL\t%zu\t%zu\n", item_count, rows);
    return true;
}

static bool controls(fixture *f) {
    const mc_registry_item_array *items = NULL;
    const mc_block *stone = NULL, *obsidian = NULL, *dirt = NULL, *cobweb = NULL;
    CHECK(mc_registry_item_c_get(f->registry, &items, NULL) == MC_REGISTRY_OK &&
          mc_registry_block_get(f->registry, 1, &stone, NULL) == MC_REGISTRY_OK &&
          mc_registry_block_get(f->registry, 3, &dirt, NULL) == MC_REGISTRY_OK &&
          mc_registry_block_get(f->registry, 49, &obsidian, NULL) == MC_REGISTRY_OK &&
          mc_registry_block_get(f->registry, 30, &cobweb, NULL) == MC_REGISTRY_OK);
    mc_item *pickaxe = items->elements[270], *sword = items->elements[268];
    CHECK(pickaxe != NULL && sword != NULL && pickaxe->kind == MC_ITEM_PICKAXE &&
          sword->kind == MC_ITEM_SWORD);
    float speed = -777.0f;
    bool harvest = true;
    mc_inventory_error e;
    const mc_item_constructor_phase phase = pickaxe->phase;
    pickaxe->phase = MC_ITEM_RAW;
    CHECK(mc_item_mining_speed(f->registry, pickaxe, NULL, stone, &speed, &e) == MC_INVENTORY_OK &&
          speed == 2.0f && pickaxe->phase == MC_ITEM_RAW);
    pickaxe->phase = phase;
    mc_block copy = *obsidian;
    CHECK(mc_item_mining_harvest(f->registry, pickaxe, obsidian, &harvest, &e) == MC_INVENTORY_OK &&
          !harvest &&
          mc_item_mining_harvest(f->registry, pickaxe, &copy, &harvest, &e) == MC_INVENTORY_OK &&
          harvest); /* AU compares singleton identity, then Material identity. */
    copy = *dirt;
    copy.fields.bn = cobweb->fields.bn;
    CHECK(mc_item_mining_speed(f->registry, sword, NULL, &copy, &speed, &e) == MC_INVENTORY_OK &&
          speed == 15.0f &&
          mc_item_mining_harvest(f->registry, sword, &copy, &harvest, &e) == MC_INVENTORY_OK &&
          harvest); /* QD compares Block IDs, not receiver identity. */
    mc_block *saved = pickaxe->dq.bk->elements[0];
    pickaxe->dq.bk->elements[0] = NULL;
    CHECK(mc_item_mining_speed(f->registry, pickaxe, NULL, NULL, &speed, &e) == MC_INVENTORY_OK &&
          speed == pickaxe->dq.bl);
    pickaxe->dq.bk->elements[0] = saved;
    const mc_item_tool_material *material = pickaxe->dq.a;
    pickaxe->dq.a = NULL;
    CHECK(mc_item_mining_harvest(f->registry, pickaxe, obsidian, &harvest, &e) ==
              MC_INVENTORY_NULL_REFERENCE &&
          e.frame_count == 1 && e.frames[0].pc == 11 &&
          mc_item_mining_harvest(f->registry, pickaxe, stone, &harvest, &e) == MC_INVENTORY_OK &&
          harvest); /* Generic rock arm never calls ToolMaterial.d. */
    pickaxe->dq.a = material;
    mc_inventory_player *inventory = NULL;
    mc_item_stack *stack = NULL;
    CHECK(mc_inventory_player_create(MC_REGISTRY_CLIENT, NULL, NULL, &inventory, NULL) ==
              MC_INVENTORY_OK &&
          mc_item_stack_allocate(&stack) == MC_ITEM_STACK_OK &&
          mc_item_stack_construct(stack, 270, 0, 2000) == MC_ITEM_STACK_OK &&
          mc_inventory_player_set(inventory, 36, stack, NULL) == MC_INVENTORY_OK);
    inventory->selected = 36;
    CHECK(mc_inventory_player_can_harvest(inventory, f->registry, stone, &harvest, &e) ==
              MC_INVENTORY_OK &&
          harvest &&
          mc_inventory_player_mining_speed(inventory, f->registry, stone, &speed, &e) ==
              MC_INVENTORY_INDEX_OUT_OF_BOUNDS &&
          e.frame_count == 1 && e.frames[0].pc == 10);
    inventory->selected = 0;
    CHECK(mc_inventory_player_set(inventory, 0, stack, NULL) == MC_INVENTORY_OK);
    mc_item_stack_release(stack);
    stack = NULL;
    CHECK(mc_item_stack_allocate(&stack) == MC_ITEM_STACK_OK &&
          mc_item_stack_construct(stack, -1, 1, 0) == MC_ITEM_STACK_OK &&
          mc_inventory_player_set(inventory, 0, stack, NULL) == MC_INVENTORY_OK);
    mc_item_stack_release(stack);
    CHECK(mc_inventory_player_mining_speed(inventory, f->registry, stone, &speed, &e) ==
              MC_INVENTORY_INDEX_OUT_OF_BOUNDS &&
          e.frame_count == 3 && e.frames[0].pc == 25 && e.frames[1].pc == 1 && e.frames[2].pc == 7);
    CHECK(mc_inventory_player_can_harvest(inventory, f->registry, stone, &harvest, &e) ==
              MC_INVENTORY_INDEX_OUT_OF_BOUNDS &&
          e.frame_count == 2 && e.frames[0].pc == 27 && e.frames[1].pc == 7);
    inventory->selected = INT32_MIN;
    CHECK(mc_inventory_player_can_harvest(inventory, NULL, dirt, &harvest, &e) == MC_INVENTORY_OK &&
          harvest); /* Material.i short-circuits Registry and array/index reads. */
    inventory->selected = 0;
    CHECK(mc_inventory_player_retire(&inventory, NULL) == MC_INVENTORY_OK);
    return true;
}

int main(int argc, char **argv) {
    if (argc != 3)
        return 2;
    mc_math_trig_init(&trig);
    fixture f = {0};
    if (!setup(&f, argv[1], argv[2]) || !matrix(&f) || !controls(&f) || !teardown(&f))
        return 1;
    printf("PASS: %u Item mining checks\n", checks);
    return 0;
}
