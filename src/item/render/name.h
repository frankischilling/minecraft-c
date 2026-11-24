#ifndef MC_ITEM_RENDER_NAME_H
#define MC_ITEM_RENDER_NAME_H

#include "internal.h"
#include "mc/resources/translation/translation.h"

/* Four reusable temporary String identities cover one raw-name/key/outer/trim
 * chain. Reset invalidates every temporary result and is called only after the
 * tooltip draw returns. Destroy rejects live results; no per-frame Registry
 * arena growth or source field repair is involved. Initialize from all-zero. */
typedef struct mc_item_name_workspace {
    void *storage;
    size_t limit, bytes, used;
} mc_item_name_workspace;

bool mc_item_name_workspace_init(mc_item_name_workspace *, size_t max_bytes);
bool mc_item_name_workspace_reset(mc_item_name_workspace *);
bool mc_item_name_workspace_destroy(mc_item_name_workspace *);

/* Entered client bodies on live, exclusively held storage. GM/CK return the
 * actual nullable field identity; BI/EN/KM allocate distinct result Strings;
 * RX returns the two actual owner-local literals. No constructor/READY gate. */
bool mc_item_render_name(mc_registry_classes *, const mc_item *, mc_item_stack *,
                         mc_item_name_workspace *, const mc_registry_string **,
                         mc_item_render_error *);
bool mc_item_stack_render_name(mc_registry_classes *, mc_item_stack *, mc_item_name_workspace *,
                               const mc_registry_string **, mc_item_render_error *);
/* ID captures NH.a at362 before IZ.l at370. Keep that receiver separate from
 * the subsequent NH.b373 call. This initializes NH, never the DO wrapper. */
bool mc_item_name_translation_get(mc_registry_classes *, const mc_translation **,
                                  mc_item_render_error *);
/* Actual NH.b appends .name, uses an empty missing-property fallback, then
 * ID's outer StringBuilder/trim382. Translated values borrow NH; final temporary Strings borrow the
 * workspace. */
bool mc_item_name_tooltip_translate(mc_registry_classes *, const mc_translation *,
                                    const mc_registry_string *, mc_item_name_workspace *,
                                    const mc_registry_string **, mc_item_render_error *);
#endif
