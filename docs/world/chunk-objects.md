# Generated Chunk ownership and finishing

`mc/world/chunk/object.h` exposes a concrete generated Chunk owned by the world
store. `mc/world/runtime/chunks.h` connects its constructor and full finishing
method to the actual GAME World. The caller-owned numerical interfaces remain
documented under [voxel data](chunk.md) and [normal finishing](finishing.md).

## Constructor and ownership

Construction adopts the supplied exclusive, malloc-compatible 32768-byte block
allocation without clearing or copying its bytes. Success clears the caller's
pointer and returns a borrowed store-owned Chunk. Failure preserves both the
block pointer and output. The same Chunk, state, data and block allocation can
then pass through generation, finishing, cache publication and saving.

The seventeen instance fields use the actual state and data cells. World d is
the supplied receiver; active, population, dirty, never-save, entity and last-save
fields share the cells consumed by existing lifecycle and storage operations.
Each of the three Nibble objects has a distinct stable header and packed span.
The height array, tile map, list array and eight entity lists also retain
their own identities. Inspection copies field values without replacing owners.
The slice lists now retain [actual typed Entity occurrences](entities.md),
including duplicates. Tile and activation support remains limited to known-empty
children; general Java collection methods are outside this owner.

Native storage combines the owner and embedded child backing in one allocation,
alongside the unchanged block allocation. The per-chunk store NBT allocation
limit bounds their combined size. Count admission includes existing reservations.
These checks precede the logical constructor stores and do not reproduce Java
object sizes or per-object allocation failures. Construction does not read a
resource, initialize Block, consume Random, sample a clock, finish, normalize or
publish the Chunk in a provider cache.

Generated final coordinates and data/block bindings must remain unchanged.
Ordinary flags and array contents remain mutable under exclusive access. Views
borrow the Chunk until checked retirement. Existing store leases and scans of
registered maps, insertion lists and fallback slots protect the same identity.

World free checks for generated and bound loaded owners with its exact d before any teardown.
World retirement may occur first and leaves that World allocated. Retire the
Chunks, then free the World, and keep the store live through those operations.
This uses the existing ownership hierarchy without a reverse persistence call
into simulation. Live Entity occurrences have a separate explicit terminal
disposal contract; ordinary retirement does not silently remove them.

## Full finishing method

The [paired source anchors](../reference/chunk.md#concrete-constructor-and-finalizer)
define constructor and finishing order. Each column starts its height search
at 127, testing physical Y126 through Y0. Actual Block active use captures q
before evaluating the current block byte; the later element read uses that
captured array. Initializer failures keep their complete registry cause. No
entry snapshot or all-entry opacity validation replaces these reached reads.

Each height write precedes the current World Dimension.e read. Direct sky starts
at 15/Y127 when that flag allows it. Signed q subtraction wraps as a Java int,
and only a positive result reaches the Nibble write, which masks its low four
bits. Y0 and other skipped values retain their preceding sky nibbles. Minimum
height publishes only after all 256 local columns complete.

The second pass queries four neighbors per column in west/east/north/south
order using wrapped coordinates and the real World height method. Different
heights reach the actual Sky handle and World queue submission. A normal return
marks dirty even when submission skips. Final dirty follows all 1024 neighbor
operations. A failure preserves preceding heights, sky, minimum, queue and dirty
effects. The error records the finalizer, column and interval call positions and
the complete nested registry or World-lighting cause.

The operation holds the World and leases this exact Chunk through all resource
callbacks and World calls. Native admission changes only the returned error.
Reached failures retain World diagnostics; successful q reads do not clear a
preceding World error. Finishing does not drain or propagate lighting.

## Persistence and verification boundary

The store distinguishes loaded records from generated owners with a closed
typed variant. Generated saves build the fresh eleven-field record directly
from current state and arrays, without a temporary Chunk or a replacement owner.
Actual child invariants are checked at the save boundary. Runtime saving keeps
the existing session gate and reached has-entities reset. Loaded records retain
their unknown NBT, opaque-data refusal, normalization and copy contracts.

All 38 selected cases pass on MSVC/GCC/Clang, including 33,458 new Chunk checks,
the existing World/registry and numerical replays, and four composition/storage
routes. Linux uses ASan/UBSan and leak detection. All 596 code/tool/fixture inputs
match before/after and retained host copies, with 35 products per compiler;
formatting passes 313 C/header files. Tests cover actual reservation-only quota
failure, queue failure before and after dirty, failed-Nether local prefixes,
retired-World guards, and live generated-to-loaded array round trips. Controlled
negative q elements and synthetic nonowning q-array guards are native controls.

The constructor and finisher have not received a new executed-original
comparison. Existing numerical reference
observations qualify their recorded contexts separately. Concrete GAME
generation and [known-empty activation](chunk-activation.md) now connect to
ordinary acquisition. Population, synchronous mutations, propagation and complete save
interoperability remain separate unfinished acceptance requirements.
