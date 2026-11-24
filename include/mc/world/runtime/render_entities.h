#ifndef MC_WORLD_RUNTIME_RENDER_ENTITIES_H
#define MC_WORLD_RUNTIME_RENDER_ENTITIES_H

#include "mc/entity/entity.h"
#include "mc/world/runtime/construction.h"

/* Actual fd.o() List return and a fresh read of the actual fd.e field.
 * These return the same nullable source identity; no size, row traversal,
 * Entity retain, class initialization or source callback occurs. Unknown
 * replacement identities can be returned, but their subsequent count/get is
 * an explicit unsupported native provenance. No World diagnostic is changed.
 * All arguments/output/error must be live, exclusive and disjoint. Output is
 * changed only on success, error is optional. GAME client RAW/failed prefixes
 * are readable; retired Worlds/containers and COMPOSITION are unsupported. */
mc_world_storage_status mc_world_runtime_render_entities_list(mc_world_runtime *,
                                                              const mc_world_reference_list **out,
                                                              mc_world_storage_error *);
mc_world_storage_status mc_world_runtime_render_weather_list(mc_world_runtime *,
                                                             const mc_world_reference_list **out,
                                                             mc_world_storage_error *);

/* Live reads of a captured constructor-owned b/e List, not a contents snapshot.
 * The old captured b stays readable if current source b is replaced. Typed
 * ledger/backing/extent and reached-row provenance are checked before a void*
 * row is interpreted. A genuine null row returns OK and NULL, so its caller
 * reaches the later source null receiver. NULL List/index failures preserve
 * out; the enclosing source caller supplies its own precise exception site.
 * Keep the same World, containers and Entity classes alive/exclusive; retain a
 * returned nonnull Entity before a source child that can remove its occurrence. */
mc_world_storage_status mc_world_runtime_render_entity_list_count(mc_world_runtime *,
                                                                  const mc_world_reference_list *,
                                                                  int32_t *out,
                                                                  mc_world_storage_error *);
mc_world_storage_status mc_world_runtime_render_entity_list_get(mc_world_runtime *,
                                                                const mc_world_reference_list *,
                                                                int32_t index, mc_entity **out,
                                                                mc_world_storage_error *);

/* One explicit native local lease keeps container retirement/free BUSY across
 * retained render calls and their callbacks. It neither freezes source Lists
 * nor retains their Entities individually. Each successful lease must have
 * exactly one matching release; do not copy its owning call/flag. Overflow or
 * unmatched release fails unchanged. Release performs no source cleanup and
 * changes no owner diagnostics. It must precede terminal World admission. */
mc_world_storage_status mc_world_runtime_render_entities_lease(mc_world_runtime *,
                                                               mc_world_storage_error *);
mc_world_storage_status mc_world_runtime_render_entities_release_lease(mc_world_runtime *,
                                                                       mc_world_storage_error *);

/* Exact fd.i(III)Z: out-of-height y returns false before reading a provider;
 * otherwise invoke actual current-provider presence at (x>>4,z>>4). No border
 * gate, Chunk load/acquisition or source callback occurs. Caller holds the
 * same live/exclusive GAME World across this body. Output/owner diagnostics
 * remain unchanged on refusal; optional error records only this call. */
mc_world_storage_status mc_world_runtime_render_block_present(mc_world_runtime *, int32_t x,
                                                              int32_t y, int32_t z, bool *out,
                                                              mc_world_storage_error *);

#endif
