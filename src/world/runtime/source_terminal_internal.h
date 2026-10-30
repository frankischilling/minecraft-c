#ifndef MC_WORLD_RUNTIME_SOURCE_TERMINAL_INTERNAL_H
#define MC_WORLD_RUNTIME_SOURCE_TERMINAL_INTERNAL_H

#include "mc/world/runtime/construction.h"
#include "mc/world/storage/source_chunk_loader.h"

/* Passive canonical source-owner inspection. All slot pointers address actual
 * owning storage and expire on mutation/retirement. No source operation, IO,
 * retain/release or owner diagnostic mutation occurs. Caller outputs/errors
 * are disjoint; out is required, error optional, and failure preserves out. */
typedef struct mc_world_terminal_loader_view {
    mc_world_store *const *store_slot;
    mc_world_source_regions *const *regions_slot;
    size_t holds;
} mc_world_terminal_loader_view;

mc_world_storage_status
mc_world_source_chunk_loader_terminal_inspect(const mc_world_source_chunk_loader *,
                                              mc_world_terminal_loader_view *,
                                              mc_world_storage_error *);
mc_world_storage_status mc_world_saved_data_terminal_store_slot(const mc_world_saved_data *,
                                                                mc_world_store *const **out,
                                                                mc_world_storage_error *);

typedef struct mc_world_terminal_source_view {
    mc_world_store *const *store_slots[3]; /* World sidecar, actual HC, actual ld. */
    mc_world_source_chunk_loader *const *loader_slot;
} mc_world_terminal_source_view;

/* Intact READY client source graph only. Requires its one actual loader owner
 * and completed source HC, with no pending/retiring sidecar or input work. */
mc_world_storage_status mc_world_runtime_terminal_source_inspect(const mc_world_runtime *,
                                                                 mc_world_terminal_source_view *,
                                                                 mc_world_storage_error *);

typedef struct mc_world_terminal_store_anchor {
    mc_world_store *const *slot;
} mc_world_terminal_store_anchor;

/* Trusted owner modules supply actual, distinct OWNED slots for this SAME
 * Handler. Count slots by identity, not copied views or guessed allowances.
 * First scope is one intact selected Handler; shared service closure remains
 * separate. No cleanup, allocation or cached permission. */
mc_world_storage_status mc_world_store_terminal_source_admit(const mc_world_store *,
                                                             const mc_world_terminal_store_anchor *,
                                                             size_t count,
                                                             mc_world_storage_error *);

#endif
