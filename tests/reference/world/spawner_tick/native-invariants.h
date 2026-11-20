/* Derived finite original invariants. No whole-field or captured-RNG native parity. */
#ifndef MC_ORIGINAL_SPAWNER_TICK_INVARIANTS_H
#define MC_ORIGINAL_SPAWNER_TICK_INVARIANTS_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct mc_spawner_tick_reference {
    uint32_t case_index;
    int32_t counter_delta;
    bool returned;
    const char *exception_class;
    uint64_t rotation_bits;
    uint32_t initial_particles;
} mc_spawner_tick_reference;

static const mc_spawner_tick_reference mc_spawner_tick_references[10] = {
    {0, 0, true, NULL, UINT64_C(0x4014000000000000), 2},
    {1, 1, false, "java.lang.ClassCastException", UINT64_C(0x4014000000000000), 2},
    {2, 1, true, NULL, UINT64_C(0x4014000000000000), 2},
    {3, 4, true, NULL, UINT64_C(0x4014000000000000), 2},
    {4, 4, true, NULL, UINT64_C(0x4014000000000000), 2},
    {5, 4, true, NULL, UINT64_C(0x4014000000000000), 2},
    {6, 4, true, NULL, UINT64_C(0x4014000000000000), 2},
    {7, 4, true, NULL, UINT64_C(0x4014000000000000), 2},
    {8, 4, true, NULL, UINT64_C(0x4014000000000000), 2},
    {9, 4, true, NULL, UINT64_C(0x4014000000000000), 2},
};

/* Each actually observed success group: smoke, flame, explode. */
static const uint32_t mc_spawner_tick_effect_counts[3] = {20, 20, 20};

/* Positive counts and final random delays differ between original VMs. */
#endif
