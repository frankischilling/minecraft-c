#ifndef MC_WORLD_RUNTIME_MAINTENANCE_H
#define MC_WORLD_RUNTIME_MAINTENANCE_H

#include "mc/world/runtime/runtime.h"
#include "mc/world/runtime/saving/errors.h"

/* Actual client fd.p()V for a normal GAME Overworld/cache owner. Each pass
 * executes the concrete cache maintenance body, then repeats only if its
 * underlying generator returned true. The represented yf.a()Z returns literal
 * false, so this executes ONE real cache pass even with no pending unloads;
 * it does not mean "unload every pending Chunk". At most100 pending entries
 * are detached/saved/removed by that pass, in the actual ordered-HashSet order.
 * The actual region loader maintenance and generator calls still occur when
 * pending is empty. Their captured empty/false bodies are source-proved.
 *
 * Hold the same actual World through all callbacks and passes. Complete nested
 * detachment/Entity/TileEntity/save causes come from that attempt; failure
 * preserves every reached field/file/collection prefix. Existing checked
 * native storage failures remain fatal instead of emulating swallowed Java
 * IOExceptions. Nonempty unsupported serializer bodies remain dependencies.
 * Success clears error. No UI, Player/controller setup, simulation ticks,
 * lighting drain, implicit clear, save-all or replacement provider is added.
 * Composition, dedicated and unqualified server Worlds fail explicitly.
 * Constructor-bound provider/cache/pending/Store/generator bindings stay
 * unchanged; arbitrary replacement/null-provider states are unqualified.
 * Arguments/output and borrowed views require exclusive disjoint lifetimes. */
mc_chunk_lifecycle_status
mc_world_runtime_finish_maintenance(mc_world_runtime *world,
                                    mc_world_runtime_lifecycle_error *error);

#endif
