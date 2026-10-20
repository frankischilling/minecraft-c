#include "mc/world/runtime/population/dungeon.h"

#include "../construction_internal.h"
#include "../mutation/mutation_internal.h"
#include "../runtime_internal.h"
#include "mc/block/registry/runtime.h"
#include "mc/item/registry/runtime.h"

#include <stdint.h>

typedef struct dungeon_frame {
    mc_world_runtime *world;
    mc_java_random *random;
    bool entered;
    size_t stack_bytes, stack_limit;
    mc_item_stack *stack;
    mc_tile *tile;
    mc_tile_name *name;
    mc_population_dungeon_error error;
} dungeon_frame;

static mc_registry_error empty_registry(void)
{
    return (mc_registry_error){.initializer_pc = -1,
                               .block_id = -1,
                               .constructor_pc = -1,
                               .active_initializer_pc = -1,
                               .item_argument = -1};
}

static mc_population_dungeon_error empty_error(void)
{
    const mc_registry_error registry = empty_registry();
    return (mc_population_dungeon_error){.chest_group = SIZE_MAX,
                                         .chest_trial = SIZE_MAX,
                                         .fill_index = SIZE_MAX,
                                         .registry = registry,
                                         .material = {.registry = registry},
                                         .mutation = mc_world_mutation_empty_error(),
                                         .tile = mc_live_tiles_empty_error()};
}

void mc_population_dungeon_limits_default(mc_population_dungeon_limits *out)
{
    if (out != NULL) {
        const size_t bytes = mc_item_stack_object_bytes();
        out->max_stack_allocation_bytes = bytes > SIZE_MAX / 16u ? SIZE_MAX : bytes * 16u;
    }
}

static bool fail(dungeon_frame *frame, mc_world_storage_status status, const char *operation)
{
    frame->error.status = status;
    frame->error.operation = operation;
    frame->error.cause = (mc_world_storage_error){status, operation, 0};
    return false;
}

static bool storage_fail(dungeon_frame *frame, const mc_world_storage_error *cause)
{
    frame->error.status = cause->status;
    frame->error.operation = cause->operation;
    frame->error.cause = *cause;
    return false;
}

static bool registry_fail(dungeon_frame *frame, const mc_registry_error *cause,
                          const char *operation)
{
    frame->error.registry = *cause;
    frame->error.status = mc_world_runtime_registry_result(frame->world, cause->status, cause,
                                                           operation, &frame->error.cause);
    frame->error.operation = frame->error.cause.operation;
    return false;
}

static bool item_fail(dungeon_frame *frame, mc_item_stack_status status, const char *operation)
{
    frame->error.item.status = status;
    return fail(frame,
                status == MC_ITEM_STACK_ALLOCATION_FAILED ? MC_WORLD_STORAGE_ALLOCATION_FAILED
                : status == MC_ITEM_STACK_LIMIT           ? MC_WORLD_STORAGE_LIMIT
                                                          : MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                operation);
}

static bool tile_fail(dungeon_frame *frame, mc_tile_status status, const char *operation)
{
    if (frame->error.tile.status != MC_WORLD_STORAGE_OK)
        return storage_fail(frame, &frame->error.tile.cause);
    frame->error.tile.tile.status = status;
    const mc_world_storage_status mapped =
        status == MC_TILE_ALLOCATION_FAILED ? MC_WORLD_STORAGE_ALLOCATION_FAILED
        : status == MC_TILE_LIMIT           ? MC_WORLD_STORAGE_LIMIT
                                            : MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    frame->error.tile.status = mapped;
    frame->error.tile.operation = operation;
    frame->error.tile.cause = (mc_world_storage_error){mapped, operation, 0};
    return storage_fail(frame, &frame->error.tile.cause);
}

static bool begin(dungeon_frame *frame, mc_world_runtime *world, mc_java_random *random,
                  const mc_population_dungeon_limits *limits, const void *out, const char *method)
{
    mc_population_dungeon_limits selected;
    mc_population_dungeon_limits_default(&selected);
    if (limits != NULL)
        selected = *limits;
    *frame = (dungeon_frame){.world = world,
                             .random = random,
                             .stack_limit = selected.max_stack_allocation_bytes,
                             .error = empty_error()};
    if (world == NULL || out == NULL)
        return fail(frame, MC_WORLD_STORAGE_INVALID_ARGUMENT, "Dungeon native arguments");
    if (world->construction.mode != MC_WORLD_RUNTIME_GAME ||
        world->construction.phase == MC_WORLD_RUNTIME_RETIRED)
        return fail(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Dungeon World phase");
    if (mc_world_runtime_call_enter(world, &frame->error.cause) != MC_WORLD_STORAGE_OK)
        return storage_fail(frame, &frame->error.cause);
    frame->entered = true;
    frame->error.feature_class = world->classes->side == MC_WORLD_INFO_CLIENT ? "er" : "da";
    frame->error.feature_method = method;
    return true;
}

static mc_world_storage_status finish(dungeon_frame *frame, bool completed,
                                      mc_population_dungeon_error *error)
{
    mc_item_stack_release(frame->stack);
    mc_tile_release(frame->tile);
    mc_tile_name_release(frame->name);
    if (completed)
        frame->error = empty_error();
    else if (frame->entered)
        frame->world->last_error = frame->error.cause;
    if (frame->entered)
        mc_world_runtime_call_leave(frame->world);
    if (error != NULL)
        *error = frame->error;
    return frame->error.status;
}

static void stage(dungeon_frame *frame, unsigned int pc)
{
    frame->error.bytecode = pc;
    frame->error.air_bytecode = 0;
}

static bool bounded(dungeon_frame *frame, int32_t bound, int32_t *out)
{
    if (frame->random == NULL)
        return fail(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Dungeon Random receiver is null");
    if (!mc_java_random_next_bounded(frame->random, bound, out))
        return fail(frame, MC_WORLD_STORAGE_INVALID_ARGUMENT, "Dungeon Random bound");
    return true;
}

static bool draw(dungeon_frame *frame, unsigned int pc, int32_t bound, int32_t *out)
{
    stage(frame, pc);
    return bounded(frame, bound, out);
}

static bool air(dungeon_frame *frame, unsigned int pc, int32_t x, int32_t y, int32_t z, bool *out)
{
    stage(frame, pc);
    frame->error.air_bytecode = 4;
    int32_t id;
    mc_world_storage_error cause = {0};
    if (mc_world_runtime_block_get(frame->world, x, y, z, &id, &cause) != MC_WORLD_STORAGE_OK)
        return storage_fail(frame, &cause);
    *out = id == 0;
    return true;
}

static bool material_get(dungeon_frame *frame, unsigned int pc, int32_t x, int32_t y, int32_t z,
                         mc_material **out)
{
    stage(frame, pc);
    if (mc_world_runtime_material_get_detailed(frame->world, x, y, z, out,
                                               &frame->error.material) != MC_WORLD_STORAGE_OK)
        return storage_fail(frame, &frame->error.material.cause);
    return true;
}

static bool solid(dungeon_frame *frame, unsigned int pc, mc_material *material, bool *out)
{
    stage(frame, pc);
    if (material == NULL)
        return fail(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                    "Dungeon Material.a receiver is null");
    *out = mc_material_a(material);
    return true;
}

static bool solid_at(dungeon_frame *frame, unsigned int query_pc, unsigned int call_pc, int32_t x,
                     int32_t y, int32_t z, bool *out)
{
    mc_material *material;
    return material_get(frame, query_pc, x, y, z, &material) &&
           solid(frame, call_pc, material, out);
}

static bool static_id(dungeon_frame *frame, const char *name, unsigned int load_pc,
                      unsigned int field_pc, int32_t *out)
{
    stage(frame, load_pc);
    const mc_block *block = NULL;
    mc_registry_error cause = empty_registry();
    if (mc_registry_block_static_get(mc_world_runtime_registry(frame->world), name, &block,
                                     &cause) != MC_REGISTRY_OK)
        return registry_fail(frame, &cause, "Dungeon Block static active use");
    stage(frame, field_pc);
    if (block == NULL)
        return fail(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Dungeon Block.bn receiver is null");
    *out = mc_block_fields_get(block)->bn;
    return true;
}

static bool write_block(dungeon_frame *frame, unsigned int pc, int32_t x, int32_t y, int32_t z,
                        int32_t id)
{
    stage(frame, pc);
    bool ignored;
    if (mc_world_runtime_block_set(frame->world, x, y, z, id, &ignored, &frame->error.mutation) !=
        MC_WORLD_STORAGE_OK)
        return storage_fail(frame, &frame->error.mutation.cause);
    return true;
}

static bool validate(dungeon_frame *frame, int32_t x, int32_t y, int32_t z, int32_t rx, int32_t rz,
                     bool *out)
{
    const int32_t x0 = mc_java_i32_sub(mc_java_i32_sub(x, rx), 1);
    const int32_t x1 = mc_java_i32_add(mc_java_i32_add(x, rx), 1);
    const int32_t y0 = mc_java_i32_sub(y, 1), y1 = mc_java_i32_add(y, 4);
    const int32_t z0 = mc_java_i32_sub(mc_java_i32_sub(z, rz), 1);
    const int32_t z1 = mc_java_i32_add(mc_java_i32_add(z, rz), 1);
    int32_t openings = 0;
    for (int32_t xx = x0; xx <= x1; xx = mc_java_i32_add(xx, 1)) {
        for (int32_t yy = y0; yy <= y1; yy = mc_java_i32_add(yy, 1)) {
            for (int32_t zz = z0; zz <= z1; zz = mc_java_i32_add(zz, 1)) {
                mc_material *material;
                bool present;
                if (!material_get(frame, 89, xx, yy, zz, &material))
                    return false;
                if (yy == y0) {
                    if (!solid(frame, 105, material, &present))
                        return false;
                    if (!present) {
                        *out = false;
                        return true;
                    }
                }
                if (yy == y1) {
                    if (!solid(frame, 127, material, &present))
                        return false;
                    if (!present) {
                        *out = false;
                        return true;
                    }
                }
                if ((xx == x0 || xx == x1 || zz == z0 || zz == z1) && yy == y) {
                    bool first, second;
                    if (!air(frame, 195, xx, yy, zz, &first))
                        return false;
                    if (first) {
                        if (!air(frame, 210, xx, mc_java_i32_add(yy, 1), zz, &second))
                            return false;
                        if (second)
                            openings = mc_java_i32_add(openings, 1);
                    }
                }
            }
        }
    }
    *out = openings >= 1 && openings <= 5;
    return true;
}

static bool construct(dungeon_frame *frame, int32_t x, int32_t y, int32_t z, int32_t rx, int32_t rz)
{
    const int32_t x0 = mc_java_i32_sub(mc_java_i32_sub(x, rx), 1);
    const int32_t x1 = mc_java_i32_add(mc_java_i32_add(x, rx), 1);
    const int32_t y0 = mc_java_i32_sub(y, 1), y1 = mc_java_i32_add(y, 4);
    const int32_t z0 = mc_java_i32_sub(mc_java_i32_sub(z, rz), 1);
    const int32_t z1 = mc_java_i32_add(mc_java_i32_add(z, rz), 1);
    for (int32_t xx = x0; xx <= x1; xx = mc_java_i32_add(xx, 1)) {
        for (int32_t yy = mc_java_i32_add(y, 3); yy >= y0; yy = mc_java_i32_sub(yy, 1)) {
            for (int32_t zz = z0; zz <= z1; zz = mc_java_i32_add(zz, 1)) {
                if (xx == x0 || yy == y0 || zz == z0 || xx == x1 || yy == y1 || zz == z1) {
                    bool present;
                    if (yy >= 0) {
                        if (!solid_at(frame, 388, 391, xx, mc_java_i32_sub(yy, 1), zz, &present))
                            return false;
                        if (!present) {
                            if (!write_block(frame, 405, xx, yy, zz, 0))
                                return false;
                            continue;
                        }
                    }
                    if (!solid_at(frame, 419, 422, xx, yy, zz, &present))
                        return false;
                    if (!present)
                        continue;
                    int32_t id;
                    if (yy == y0) {
                        int32_t choice;
                        if (!draw(frame, 439, 4, &choice))
                            return false;
                        if (choice != 0) {
                            if (!static_id(frame, "ap", 452, 455, &id) ||
                                !write_block(frame, 458, xx, yy, zz, id))
                                return false;
                            continue;
                        }
                    }
                    if (!static_id(frame, "x", 472, 475, &id) ||
                        !write_block(frame, 478, xx, yy, zz, id))
                        return false;
                } else if (!write_block(frame, 493, xx, yy, zz, 0)) {
                    return false;
                }
            }
        }
    }
    return true;
}

static bool loot_draw(dungeon_frame *frame, unsigned int pc, int32_t bound, int32_t *out)
{
    frame->error.loot_bytecode = pc;
    return bounded(frame, bound, out);
}

static bool allocate_stack(dungeon_frame *frame, unsigned int pc)
{
    frame->error.loot_bytecode = pc;
    frame->error.allocation_bytecode = pc;
    const size_t bytes = mc_item_stack_object_bytes();
    if (frame->stack_bytes > frame->stack_limit || bytes > frame->stack_limit - frame->stack_bytes)
        return item_fail(frame, MC_ITEM_STACK_LIMIT, "Dungeon loot Stack request bound");
    const mc_item_stack_status status = mc_item_stack_allocate(&frame->stack);
    if (status != MC_ITEM_STACK_OK)
        return item_fail(frame, status, "Dungeon loot Stack allocation");
    frame->stack_bytes += bytes;
    return true;
}

static bool item_static(dungeon_frame *frame, const char *name, unsigned int pc,
                        const mc_item **out)
{
    frame->error.loot_bytecode = pc;
    mc_registry_error cause = empty_registry();
    if (mc_registry_item_static_get(mc_world_runtime_registry(frame->world), name, out, &cause) !=
        MC_REGISTRY_OK)
        return registry_fail(frame, &cause, "Dungeon Item static active use");
    return true;
}

static bool construct_stack(dungeon_frame *frame, unsigned int pc, const mc_item *item,
                            int32_t count, int32_t damage, unsigned int arguments)
{
    frame->error.loot_bytecode = pc;
    const bool client = frame->world->classes->side == MC_WORLD_INFO_CLIENT;
    const char *descriptor = arguments == 3   ? (client ? "(Lgm;II)V" : "(Lej;II)V")
                             : arguments == 2 ? (client ? "(Lgm;I)V" : "(Lej;I)V")
                                              : (client ? "(Lgm;)V" : "(Lej;)V");
    if (item == NULL) {
        frame->error.stack_constructor_class =
            frame->world->classes->side == MC_WORLD_INFO_CLIENT ? "iz" : "fy";
        frame->error.stack_constructor_descriptor = descriptor;
        frame->error.stack_constructor_bytecode = 2;
        return item_fail(frame, MC_ITEM_STACK_INVALID_ARGUMENT,
                         "Dungeon Stack Item.bf receiver is null");
    }
    const int32_t id = mc_item_fields_get(item)->bf;
    const mc_item_stack_status status = mc_item_stack_construct(frame->stack, id, count, damage);
    if (status != MC_ITEM_STACK_OK) {
        frame->error.stack_constructor_class =
            frame->world->classes->side == MC_WORLD_INFO_CLIENT ? "iz" : "fy";
        frame->error.stack_constructor_descriptor = descriptor;
        frame->error.stack_constructor_bytecode = 7;
        return item_fail(frame, status, "Dungeon Stack numeric constructor");
    }
    return true;
}

static bool loot(dungeon_frame *frame)
{
    int32_t selector;
    if (!loot_draw(frame, 3, 11, &selector))
        return false;
    const char *name;
    unsigned int new_pc, static_pc, count_pc = 0, invoke_pc;
    int32_t count = 1, damage = 0;
    switch (selector) {
        case 0:
            name = "ay";
            new_pc = 11;
            static_pc = 15;
            invoke_pc = 18;
            break;
        case 1:
            name = "m";
            new_pc = 27;
            static_pc = 31;
            count_pc = 36;
            invoke_pc = 41;
            break;
        case 2:
            name = "S";
            new_pc = 50;
            static_pc = 54;
            invoke_pc = 57;
            break;
        case 3:
            name = "R";
            new_pc = 66;
            static_pc = 70;
            count_pc = 75;
            invoke_pc = 80;
            break;
        case 4:
            name = "K";
            new_pc = 89;
            static_pc = 93;
            count_pc = 98;
            invoke_pc = 103;
            break;
        case 5:
            name = "I";
            new_pc = 112;
            static_pc = 116;
            count_pc = 121;
            invoke_pc = 126;
            break;
        case 6:
            name = "au";
            new_pc = 136;
            static_pc = 140;
            invoke_pc = 143;
            break;
        case 7: {
            int32_t gate;
            if (!loot_draw(frame, 156, 100, &gate))
                return false;
            if (gate != 0)
                return true;
            name = "ar";
            new_pc = 162;
            static_pc = 166;
            invoke_pc = 169;
            break;
        }
        case 8: {
            int32_t gate;
            if (!loot_draw(frame, 181, 2, &gate))
                return false;
            if (gate != 0)
                return true;
            name = "aA";
            new_pc = 187;
            static_pc = 191;
            count_pc = 196;
            invoke_pc = 201;
            break;
        }
        case 9: {
            int32_t gate;
            if (!loot_draw(frame, 214, 10, &gate))
                return false;
            if (gate != 0)
                return true;
            if (!allocate_stack(frame, 220))
                return false;
            const mc_registry_item_array *captured = NULL;
            mc_registry_error cause = empty_registry();
            frame->error.loot_bytecode = 224;
            if (mc_registry_item_c_get(mc_world_runtime_registry(frame->world), &captured,
                                       &cause) != MC_REGISTRY_OK)
                return registry_fail(frame, &cause, "Dungeon Item.c active use");
            const mc_item *base = NULL;
            if (!item_static(frame, "bd", 227, &base))
                return false;
            frame->error.loot_bytecode = 230;
            if (base == NULL)
                return fail(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                            "Dungeon record Item.bf receiver is null");
            const int32_t id = mc_item_fields_get(base)->bf;
            int32_t offset;
            if (!loot_draw(frame, 235, 2, &offset))
                return false;
            const mc_item *record = NULL;
            frame->error.loot_bytecode = 239;
            if (mc_registry_item_c_at(mc_world_runtime_registry(frame->world), captured,
                                      mc_java_i32_add(id, offset), &record,
                                      &cause) != MC_REGISTRY_OK)
                return registry_fail(frame, &cause, "Dungeon captured Item.c element");
            return construct_stack(frame, 240, record, 1, 0, 1);
        }
        case 10:
            name = "aU";
            new_pc = 250;
            static_pc = 254;
            invoke_pc = 259;
            damage = 3;
            break;
        default:
            return true;
    }
    if (!allocate_stack(frame, new_pc))
        return false;
    const mc_item *item = NULL;
    if (!item_static(frame, name, static_pc, &item))
        return false;
    if (count_pc != 0) {
        if (!loot_draw(frame, count_pc, 4, &count))
            return false;
        count = mc_java_i32_add(count, 1);
    }
    return construct_stack(frame, invoke_pc, item, count, damage,
                           selector == 10  ? 3u
                           : count_pc != 0 ? 2u
                                           : 1u);
}

static void loot_completed(dungeon_frame *frame)
{
    frame->error.loot_bytecode = 0;
    frame->error.allocation_bytecode = 0;
    frame->error.stack_constructor_bytecode = 0;
    frame->error.stack_constructor_class = NULL;
    frame->error.stack_constructor_descriptor = NULL;
}

static bool tile_get(dungeon_frame *frame, unsigned int pc, unsigned int cast_pc, mc_tile_kind kind,
                     int32_t x, int32_t y, int32_t z)
{
    stage(frame, pc);
    if (mc_live_tiles_get(frame->world, x, y, z, &frame->tile, &frame->error.tile) !=
        MC_WORLD_STORAGE_OK)
        return storage_fail(frame, &frame->error.tile.cause);
    stage(frame, cast_pc);
    if (frame->tile != NULL) {
        mc_tile_snapshot snapshot;
        const mc_tile_status status = mc_tile_get_snapshot(frame->tile, &snapshot);
        if (status != MC_TILE_OK)
            return tile_fail(frame, status, "Dungeon Tile cast snapshot");
        if (snapshot.kind != kind)
            return tile_fail(frame, MC_TILE_INVALID_KIND, "Dungeon concrete Tile cast");
    }
    return true;
}

static bool fill_chest(dungeon_frame *frame)
{
    for (size_t fill = 0; fill < 8u; ++fill) {
        frame->error.fill_index = fill;
        stage(frame, 728);
        if (!loot(frame))
            return false;
        loot_completed(frame);
        if (frame->stack == NULL)
            continue;
        stage(frame, 743);
        int32_t size;
        mc_tile_status status = mc_tile_chest_size(frame->tile, &size);
        if (status != MC_TILE_OK)
            return tile_fail(frame, status, "Dungeon Chest size receiver");
        int32_t slot;
        if (!draw(frame, 746, size, &slot))
            return false;
        stage(frame, 751);
        status = mc_tile_chest_set_detailed(frame->tile, slot, frame->stack, &frame->error.tile);
        if (status != MC_TILE_OK)
            return tile_fail(frame, status, "Dungeon Chest slot assignment");
        mc_item_stack_release(frame->stack);
        frame->stack = NULL;
    }
    frame->error.fill_index = SIZE_MAX;
    return true;
}

static bool chests(dungeon_frame *frame, int32_t x, int32_t y, int32_t z, int32_t rx, int32_t rz)
{
    static const unsigned int queries[4] = {600, 621, 642, 663};
    static const unsigned int predicates[4] = {603, 624, 645, 666};
    for (size_t group = 0; group < 2u; ++group) {
        frame->error.chest_group = group;
        for (size_t trial = 0; trial < 3u; ++trial) {
            frame->error.chest_trial = trial;
            int32_t xx, zz;
            if (!draw(frame, 541, mc_java_i32_add(mc_java_i32_mul(rx, 2), 1), &xx))
                return false;
            xx = mc_java_i32_sub(mc_java_i32_add(x, xx), rx);
            if (!draw(frame, 563, mc_java_i32_add(mc_java_i32_mul(rz, 2), 1), &zz))
                return false;
            zz = mc_java_i32_sub(mc_java_i32_add(z, zz), rz);
            bool is_air;
            if (!air(frame, 579, xx, y, zz, &is_air))
                return false;
            if (!is_air)
                continue;
            int32_t count = 0;
            for (size_t direction = 0; direction < 4u; ++direction) {
                const int32_t nx = direction == 0   ? mc_java_i32_sub(xx, 1)
                                   : direction == 1 ? mc_java_i32_add(xx, 1)
                                                    : xx;
                const int32_t nz = direction == 2   ? mc_java_i32_sub(zz, 1)
                                   : direction == 3 ? mc_java_i32_add(zz, 1)
                                                    : zz;
                bool present;
                if (!solid_at(frame, queries[direction], predicates[direction], nx, y, nz,
                              &present))
                    return false;
                if (present)
                    count = mc_java_i32_add(count, 1);
            }
            if (count != 1)
                continue;
            int32_t id;
            if (!static_id(frame, "av", 691, 694, &id) || !write_block(frame, 697, xx, y, zz, id) ||
                !tile_get(frame, 708, 711, MC_TILE_CHEST, xx, y, zz) || !fill_chest(frame))
                return false;
            mc_tile_release(frame->tile);
            frame->tile = NULL;
            break;
        }
    }
    frame->error.chest_group = frame->error.chest_trial = SIZE_MAX;
    return true;
}

static bool mob(dungeon_frame *frame)
{
    frame->error.mob_bytecode = 2;
    int32_t choice;
    if (!bounded(frame, 4, &choice))
        return false;
    const char *name;
    switch (choice) {
        case 0:
            frame->error.mob_bytecode = 10;
            name = "Skeleton";
            break;
        case 1:
            frame->error.mob_bytecode = 18;
            name = "Zombie";
            break;
        case 2:
            frame->error.mob_bytecode = 26;
            name = "Zombie";
            break;
        case 3:
            frame->error.mob_bytecode = 34;
            name = "Spider";
            break;
        default:
            frame->error.mob_bytecode = 37;
            name = "";
            break;
    }
    if (mc_live_tiles_literal(frame->world, name, &frame->name, &frame->error.tile) !=
        MC_WORLD_STORAGE_OK)
        return storage_fail(frame, &frame->error.tile.cause);
    return true;
}

mc_world_storage_status mc_population_dungeon_place(mc_world_runtime *world, mc_java_random *random,
                                                    const mc_population_dungeon_limits *limits,
                                                    int32_t x, int32_t y, int32_t z, bool *out,
                                                    mc_population_dungeon_error *error)
{
    dungeon_frame frame;
    if (!begin(&frame, world, random, limits, out, "a(World,Random,III)Z"))
        return finish(&frame, false, error);
    int32_t rx, rz;
    if (!draw(&frame, 5, 2, &rx) || !draw(&frame, 14, 2, &rz))
        return finish(&frame, false, error);
    rx = mc_java_i32_add(rx, 2);
    rz = mc_java_i32_add(rz, 2);
    bool valid;
    if (!validate(&frame, x, y, z, rx, rz, &valid))
        return finish(&frame, false, error);
    if (!valid) {
        *out = false;
        return finish(&frame, true, error);
    }
    if (!construct(&frame, x, y, z, rx, rz) || !chests(&frame, x, y, z, rx, rz))
        return finish(&frame, false, error);
    int32_t id;
    if (!static_id(&frame, "at", 781, 784, &id) || !write_block(&frame, 787, x, y, z, id) ||
        !tile_get(&frame, 797, 800, MC_TILE_SPAWNER, x, y, z))
        return finish(&frame, false, error);
    stage(&frame, 809);
    if (!mob(&frame))
        return finish(&frame, false, error);
    frame.error.mob_bytecode = 0;
    stage(&frame, 812);
    const mc_tile_status status = mc_tile_spawner_set_name(frame.tile, frame.name);
    if (status != MC_TILE_OK) {
        (void)tile_fail(&frame, status, "Dungeon Spawner name receiver");
        return finish(&frame, false, error);
    }
    *out = true;
    return finish(&frame, true, error);
}

mc_world_storage_status mc_population_dungeon_loot(mc_world_runtime *world, mc_java_random *random,
                                                   const mc_population_dungeon_limits *limits,
                                                   mc_item_stack **out,
                                                   mc_population_dungeon_error *error)
{
    dungeon_frame frame;
    if (!begin(&frame, world, random, limits, out, "a(Random)ItemStack") || !loot(&frame))
        return finish(&frame, false, error);
    *out = frame.stack;
    frame.stack = NULL;
    return finish(&frame, true, error);
}

mc_world_storage_status mc_population_dungeon_mob(mc_world_runtime *world, mc_java_random *random,
                                                  mc_tile_name **out,
                                                  mc_population_dungeon_error *error)
{
    dungeon_frame frame;
    if (!begin(&frame, world, random, NULL, out, "b(Random)String") || !mob(&frame))
        return finish(&frame, false, error);
    *out = frame.name;
    frame.name = NULL;
    return finish(&frame, true, error);
}
