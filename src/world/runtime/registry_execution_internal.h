#ifndef MC_WORLD_RUNTIME_REGISTRY_EXECUTION_INTERNAL_H
#define MC_WORLD_RUNTIME_REGISTRY_EXECUTION_INTERNAL_H

#include "collision/blocks_execution_internal.h"
#include "mc/world/runtime/blocks.h"

/* ONE actual World material body, shared with unchanged legacy detailed/plain
 * entries. This private bool is CONTINUATION success, not a Source status. It
 * is true only after actual material publication with both channels successful;
 * a successful nullable material field can publish NULL. False preserves *out.
 *
 * The caller already owns the exact evaluated nonNULL World scope BEFORE later
 * Source arguments. Invocation-NULL Source classification belongs at the actual
 * caller, not this native interface. No scope begin/pop or code inference occurs.
 * code_classes is the actual defining World/Registry/Entity namespace borrow,
 * kept live/exclusive independently of mutable Entity.world and Chunk.d. The
 * currently joined domain requires runtime->classes == code_classes; this is
 * native qualification, never a Source class/READY/active-use test. code is the
 * actual unchanged defining Chunk caller bundle, not derived from a receiver.
 *
 * Required error/query_error/query_status/owned are caller-stable, fresh,
 * disjoint enclosing-frame storage; query_error is already initialized scratch
 * with any intended outer source PCs. All owners, outputs and native arguments
 * are disjoint. owned is initialized here (when nonNULL), including on native
 * interface refusal. Other outputs/scratch are untouched on preflight refusal.
 * query_status is assigned ONLY to the actual one lower collision entry return;
 * do not inspect it as a getter outcome if this native preflight refused.
 *
 * Inspect owned->ownership_status FIRST. NonOK stops at material PC4 before
 * Source-error projection, Registry/material work or out publication. Preserve
 * query_status, the complete typed query_error and owned verbatim, including
 * failed/pending selections, native-first error and close capsules. The material
 * error keeps its reached query4 prefix; no owner status becomes a Source cause,
 * and a native-only stop never clears the lower failed prefix. The lower frame
 * remains primary even when actual Source failure and native refusal coincide.
 * Ownership OK does NOT by itself classify a lower failure as a Java exception:
 * Kernel2/native preflight and insufficiently typed arms still need the parent's
 * explicit current Source classifier; unresolved arms remain UNRESOLVED.
 *
 * Only a completed successful lower read reaches air Material14 or Block.m18,
 * captured array23 and selected Block.bA24. No alternate getter, last-error
 * mining, desired value, fallback, Source retry or native-only Source resume is
 * supplied. Retain all actual result VALUEs and typed diagnostics through genuine
 * checked pending/child-close unwind, then clear expired views before retirement.
 *
 * Caller also keeps the real provider/Store/import/resource graph jointly live
 * under the published scalar child contract. This entry does not adopt arbitrary
 * foreign Chunk.d/Store graphs or qualify unrelated code namespaces. Unjoined
 * contextful import/loading, generation/population and foreign graph lifetimes
 * remain OPEN and require their real external ownership proof, never a guessed
 * loader namespace. Same-code pointer qualification alone is not that proof.
 * Base scanner/parent frame/classifier/cleanup and full SL remain separate OPEN
 * joins. No retained owner or Source field is added by this descriptor.
 */
bool mc_world_runtime_material_get_in_execution(mc_entity_tick_execution *,
                                                mc_world_runtime_classes *, mc_world_runtime *,
                                                mc_world_runtime_execution_code, int32_t, int32_t,
                                                int32_t, mc_material **, mc_world_material_error *,
                                                mc_world_collision_error *,
                                                mc_world_storage_status *query_status,
                                                mc_world_runtime_execution_result *);

#endif
