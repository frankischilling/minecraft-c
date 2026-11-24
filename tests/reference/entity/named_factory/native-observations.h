/* Original finite Pig fields only. No native parity, entropy or whole-frame claim. */
#ifndef MC_NAMED_FACTORY_ORIGINAL_OBSERVATIONS_H
#define MC_NAMED_FACTORY_ORIGINAL_OBSERVATIONS_H
#include <stdbool.h>
#include <stdint.h>

typedef struct mc_named_factory_pig_reference {
    uint32_t case_index;
    int32_t expected_id;
    bool world_nonnull;
    uint32_t width_bits, height_bits;
    int32_t health;
    int8_t watcher0, watcher16;
    const char *texture_ascii;
} mc_named_factory_pig_reference;

static const mc_named_factory_pig_reference mc_named_factory_pig_references[3] = {
    {5, 0, false, UINT32_C(0x3f666666), UINT32_C(0x3f666666), 10, 0, 0, "/mob/pig.png"},
    {6, 1, false, UINT32_C(0x3f666666), UINT32_C(0x3f666666), 10, 0, 0, "/mob/pig.png"},
    {7, 2, true, UINT32_C(0x3f666666), UINT32_C(0x3f666666), 10, 0, 0, "/mob/pig.png"},
};

/* Same original E0 receiver: NULL World before, actual FD World after. */
typedef struct mc_named_factory_world_setter_reference {
    int32_t expected_id;
    bool world_before_nonnull, world_after_nonnull;
} mc_named_factory_world_setter_reference;

static const mc_named_factory_world_setter_reference mc_named_factory_world_setter_reference_value =
    {0, false, true};

/* Original cases4/8 return NULL after caught InstantiationException and real
 * printStackTrace. A native print52 frontier is not replay of those outcomes. */
#endif
