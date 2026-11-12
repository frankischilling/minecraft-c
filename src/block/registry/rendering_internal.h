#ifndef MC_BLOCK_REGISTRY_RENDERING_INTERNAL_H
#define MC_BLOCK_REGISTRY_RENDERING_INTERNAL_H

#include "../../item/render/internal.h"

/* Same virtual metadata operation as the public wrapper, with the existing
 * typed Source-site/Registry failure record. No allocation, owner adoption or
 * additional Source gate occurs. DB side1 performs real Block active use at5
 * before capturing live aw; a successful NULL capture fails only at invoke10.
 * Other sides preserve DB invokespecial17 and UU's virtual-side call2.
 * The genuine aw target selects SM.a(II)I5, a direct bm1 read/return4;
 * it never uses the superficially equal virtual side-texture shortcut.
 *
 * Caller keeps the actual Block/class/dependency owners live and exclusive.
 * out and optional error storage are disjoint from each other and all owner
 * storage. out is unchanged on failure; error is initialized on entry. The
 * finite ancestor slots are diagnostics only, never a recursive work bound.
 * They are reached native operation sites, not an observed Java child trace. */
mc_registry_status mc_block_render_texture_metadata_detail(const mc_block *, int32_t side,
                                                           int32_t metadata, int32_t *,
                                                           mc_item_render_error *);

#endif
