#ifndef MC_WORLD_RUNTIME_POPULATION_DUNGEON_H
#define MC_WORLD_RUNTIME_POPULATION_DUNGEON_H

#include "mc/core/java/java.h"
#include "mc/world/runtime/blocks.h"
#include "mc/world/runtime/mutations.h"
#include "mc/world/runtime/tiles/tiles.h"

/* A separate native per-call request budget for fresh loot Stack objects.
 * Count every successful Stack allocation, including later released/replaced
 * objects. Check before each source NEW, before its argument evaluation.
 * NULL limits admits all sixteen possible source allocations. Explicit zero
 * still admits validation rejection and nonallocating NULL-loot branches.
 * Existing World/Chunk/Tile/Stack/name owners, Registry initialization, nested
 * hooks, allocator overhead and the C call stack are outside this budget.
 * It is not a combined retained-World bound or Java heap/OOME equivalence. */
typedef struct mc_population_dungeon_limits {
    size_t max_stack_allocation_bytes;
} mc_population_dungeon_limits;

void mc_population_dungeon_limits_default(mc_population_dungeon_limits *out);

typedef struct mc_population_dungeon_error {
    mc_world_storage_status status;
    const char *operation;
    const char *feature_class, *feature_method;
    unsigned int bytecode, loot_bytecode, mob_bytecode, air_bytecode;
    unsigned int allocation_bytecode, stack_constructor_bytecode;
    const char *stack_constructor_class, *stack_constructor_descriptor;
    size_t chest_group, chest_trial, fill_index; /* SIZE_MAX outside those loops. */
    mc_world_storage_error cause;
    mc_registry_error registry;
    mc_world_material_error material;
    mc_world_mutation_error mutation;
    mc_live_tile_error tile;
    mc_item_stack_error item;
} mc_population_dungeon_error;

/* Complete stateless er/da placement and its two helpers. Their constructor
 * only invokes the empty pg/jm base; inherited scale(DDD)V is return0.
 * No native feature allocation, retained palette or replacement Random exists.
 *
 * Use actual exclusive GAME World/Registry/Chunk/Random dependencies, held
 * across acquisition and synchronous reentry. RAW/constructing/failed World
 * prefixes are admitted; retirement and COMPOSITION are refused. Keep all
 * argument/result/error/limit storage disjoint from owners and each other.
 * A NULL Random fails only at its first reached invocation. No earlier class
 * initialization, coordinate clamp, quiet-write substitution or rollback occurs.
 * Java int arithmetic and absolute inclusive loop increments wrap, including
 * extreme inputs whose original loops can skip or fail to terminate.
 *
 * Validation reads every cell in X/Y/Z order, counts two-high perimeter
 * openings, and accepts one through five. Construction uses X/descending Y/Z,
 * live Material.a, late Block fields, and NOTIFYING writes. Setter false is
 * ignored. Two groups each try three Chest sites; accepted sites call loot
 * eight times, retain the same nullable Chest, and replace real random slots.
 * Final Spawner selection resolves the real shared literal before its setter.
 * All reached hooks/observers/lighting/Tile/dirty effects remain real; missing
 * concrete descendants fail explicitly and remain unfinished game behavior.
 *
 * Native failure preserves ordinary outputs and all completed source effects;
 * only private temporary holds are released. Success/ordinary rejection clears
 * this attempt's error. This implementation is not by itself original natural
 * population or arbitrary-World synchronous-closure qualification. */
mc_world_storage_status mc_population_dungeon_place(mc_world_runtime *world, mc_java_random *random,
                                                    const mc_population_dungeon_limits *limits,
                                                    int32_t x, int32_t y, int32_t z, bool *out,
                                                    mc_population_dungeon_error *error);

/* Direct calls to the same real private-helper implementations used above,
 * for feature composition and explicitly labeled helper observations. Loot
 * publishes one independent nullable Stack hold with no provenance. Mob
 * publishes a held shared Registry-backed Tile name, not copied equal text.
 * Release with mc_item_stack_release / mc_tile_name_release respectively.
 * They do not place blocks, query Chest slots, spawn mobs or drain ticks. */
mc_world_storage_status mc_population_dungeon_loot(mc_world_runtime *world, mc_java_random *random,
                                                   const mc_population_dungeon_limits *limits,
                                                   mc_item_stack **out,
                                                   mc_population_dungeon_error *error);
mc_world_storage_status mc_population_dungeon_mob(mc_world_runtime *world, mc_java_random *random,
                                                  mc_tile_name **out,
                                                  mc_population_dungeon_error *error);

#endif
