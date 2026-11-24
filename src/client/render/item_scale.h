#ifndef MC_CLIENT_RENDER_ITEM_SCALE_H
#define MC_CLIENT_RENDER_ITEM_SCALE_H

#include "living_draw_internal.h"

/* Entered BB.a(Lhl;DDDFF)V188..229. The caller already evaluated the actual
 * Block.m[Stack.c] receiver for UU.d()Z at187 and owns the captured Stack and
 * Registry namespace across this prefix. Append to its live draw receipt;
 * do not initialize it here. No Display, transform, allocation or source
 * mutation runs. Failure preserves scale and all earlier parent effects. */
bool mc_client_item_block_scale(mc_registry_classes *, mc_item_stack *, const mc_block *,
                                float *scale, mc_client_living_draw_error *);

#endif
