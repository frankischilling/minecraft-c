#ifndef MC_CLIENT_RENDER_BLOCKS_INVENTORY_COLOR_H
#define MC_CLIENT_RENDER_BLOCKS_INVENTORY_COLOR_H

#include <stdbool.h>
#include <stdint.h>

struct mc_cv_context;

/* Exact BK.b(I)I reached by CV.a(Block,metadata,brightness): independent of
 * Block fields, World climate and the nullable JH array. The actual graphics
 * namespace performs JH active use even for its constant-return methods.
 * Caller owns the live CV/graphics context and its enclosing source frame. */
bool mc_cv_leaf_inventory_color(struct mc_cv_context *, int32_t metadata, int32_t *out);

#endif
