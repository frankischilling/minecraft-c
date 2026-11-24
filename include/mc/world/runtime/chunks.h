#ifndef MC_WORLD_RUNTIME_CHUNKS_H
#define MC_WORLD_RUNTIME_CHUNKS_H

#include "mc/world/chunk/object.h"
#include "mc/world/runtime/lighting.h"

typedef struct mc_world_chunk_error {
    mc_world_storage_status status;
    const char *operation;
    /* Local full-finalizer PC, then c(II)'s f call and f(III)'s reached call.
     * Zero means native admission/unreached frame. Registry/light retain the
     * complete nested source cause; these are not Java Throwable identities. */
    unsigned int bytecode, column_bytecode, interval_bytecode;
    mc_world_storage_error cause;
    mc_registry_error registry;
    mc_world_light_error light;
} mc_world_chunk_error;

/* The reached (World,byte[],II) Chunk constructor, with final native store
 * ownership established BEFORE sampling/finishing. Require a live GAME World
 * other than RETIRED, actual handler w and a current Dimension for native
 * catalog selection0/-1. This is the provider constructor seam, not arbitrary
 * NULL-World/reflection/subclass construction. No Registry/class use, Random,
 * clock, generation, normalization, activation or cache publication occurs.
 *
 * Caller supplies one exclusive malloc-compatible array of EXACTLY32768 bytes.
 * Preserve its bytes. Success transfers that SAME allocation (*blocks=NULL),
 * returning a borrowed store-owned object. No late retain-copy is performed.
 * Failure keeps blocks/out unchanged. Native count/link/per-chunk budget
 * admission is0; it does not claim Java per-object allocation failure timing.
 * Store's per-chunk NBT max_allocation_bytes bounds combined backing+blocks;
 * it is separate from World/provider direct budgets and later NBT build peaks.
 * Hold the actual World across construction/linkage; generated owners keep
 * World free BUSY until checked store retirement. Retired World may remain
 * inspectable. Keep store live until chunks and all borrowing Worlds retire. */
mc_world_storage_status mc_world_runtime_chunk_construct(mc_world_runtime *world, int32_t x,
                                                         int32_t z, uint8_t **blocks, size_t length,
                                                         mc_world_chunk **out,
                                                         mc_world_chunk_error *error);

/* Actual full lm.c()/hi.b() finalizer on the SAME constructed/store-owned
 * Chunk. Hold World and lease Chunk through the full call. q active capture
 * precedes block bytes; later iaload uses that captured array. Per-column
 * height writes precede live Dimension.e; signed raw q subtraction wraps as
 * Java, with nibble masking only at the reached write. Minimum publishes179
 * after all columns. Then1024 ordered current-World height/submit operations
 * precede final dirty218; each successful interval sets dirty at36/67 even
 * if submit skipped. Preserve all reached partial fields/queue/registry effects
 * and the complete first cause on failure. No rollback, all-q preflight,
 * fixed no-sky snapshot, numerical finisher or propagation/drain is used.
 * Repeated finishing retains skipped skylight. Native admission changes only
 * error; reached failure retains World diagnostics. Success leaves previous
 * diagnostics except actual reached World/getter effects. Ordinary result is
 * status only. Views/results/errors/arguments/dependencies remain disjoint and
 * exclusively accessed. GAME generation remains unavailable until its other
 * actual provider/density/surface/cave joins are implemented. */
mc_world_storage_status mc_world_runtime_chunk_finish(mc_world_chunk *chunk,
                                                      mc_world_chunk_error *error);

#endif
