#ifndef MC_BLOCK_REGISTRY_WORLD_COLOR_OPS_INTERNAL_H
#define MC_BLOCK_REGISTRY_WORLD_COLOR_OPS_INTERNAL_H

#include <stdbool.h>
#include <stdint.h>

/* The adapter owns its actual XP/namespace and diagnostics. Each channel call
 * resolves XP.a afresh after the preceding sampling callback returns. */
typedef struct mc_block_world_color_ops {
    bool (*metadata)(void *, int32_t, int32_t, int32_t, unsigned int, int32_t *);
    bool (*climate)(void *, int32_t, int32_t, unsigned int, unsigned int);
    bool (*channel)(void *, bool, unsigned int, unsigned int, unsigned int, double *);
    bool (*constant)(void *, bool, unsigned int, int32_t *);
    bool (*lookup)(void *, double, double, unsigned int, int32_t *);
    void (*pc)(void *, unsigned int);
} mc_block_world_color_ops;

#endif
