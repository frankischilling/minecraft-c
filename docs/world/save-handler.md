# Source SaveHandler

The source SaveHandler uses the same opaque WorldStore identity as the storage
module. Its native class scope retains the shared File namespace and Logger.
The constructor follows the original order: initialize the Logger, allocate the
Store, sample the session token, construct the world/players/data Files, create
the reached directories, and write the eight-byte big-endian `session.lock`.
SaveFormat returns that same completed Store. Failed candidates retain their
actual Files, streams, directory effects and typed causes for cleanup.

Metadata reads use the real FileInputStream, GZIP, DataInput/NBT and WorldInfo
owners. The method checks `level.dat`, then `level.dat_old` when the primary is
absent. If both are absent it returns NULL. A caught read/schema exception stops
at its actual, unfinished `printStackTrace` call; it does not silently continue
to the fallback. Successful reads transfer the actual WorldInfo occurrence.
Retained document/Data/Info aliases remain valid until their own holds end.

Session checking reads exactly eight bytes through the existing DataInput
scratch/readFully implementation, compares the live Store token, closes the
stream and returns on a match. Short reads retain their actual intake and
finally-close effects. A mismatch or caught I/O error reaches the original
exception allocation, which remains an explicit dependency. Native cleanup
never deletes the lock, invents a source exception, or repeats a consumed close.

## Source chunk loader

`mc_world_store_source_loader` implements `mx.a(xa)` and returns a retained
[`mc_world_source_chunk_loader`](../../include/mc/world/storage/source_chunk_loader.h).
For an ordinary dimension, its `ld.a` field holds the handler's exact world
File. A Nether provider reaches a new `File(world, "DIM-1")` and `mkdirs`
before the loader stores that File. The ignored source `mkdirs` boolean stays
ignored. `ld(File)` performs no region open or chunk read. The current Nether
chunk-provider dependency occurs later than this completed loader construction.

The World keeps this loader in its existing source-construction owner and
retains a separate occurrence of the same Store. The loader holds its own
Store, File and shared region-namespace occurrences. A failed factory can
return a native candidate containing the reached File prefix; the World records
that candidate for checked retirement. No second Store or legacy root path is
created. [SavedData](saved-data.md) uses the same handler through its separate
File factory and lifetime.

On a read, `ld` calls the shared source region namespace with its stored File
and the requested chunk coordinates. A source null record becomes a loader
miss. A returned record enters the existing incremental DataInput/NBT parser.
The completed document moves directly into the canonical GAME stored-chunk
decoder and Store stage. The existing Entity and TileEntity import bodies then
run over that same document and Chunk. `lm.i`/`vi` normalization follows both
imports and uses the shared map initialized from the bound Registry. There is
no serialization round trip or replacement chunk owner.

The loader keeps a counted document occurrence through its retained call
prefix, independently of the Store's adopted occurrence. Its full typed error
records region, compression, NBT, Registry and Store causes. Enclosing callers
borrow that error while the loader remains live. Native cleanup can consume
earlier children before a later cleanup failure; remaining slots and the
original failure stay available for inspection and retry. Strict loader
retirement rejects extra loader holds before consuming its children.

## Region descriptors and incremental input

The handler classes cold-bind one
[`mc_world_source_regions`](../../include/mc/world/storage/source_regions.h)
namespace using the same File namespace. Client Worlds and dimensions share
that `rj` cache. `mc_world_source_regions_flush` runs its shared `rj.a()`
close/clear body; the SaveFormat `hi.c()` caller is still an explicit dependency.
Its File keys select
`region/r.<x >> 5>.<z >> 5>.mcr`; `qj` receives local coordinates masked with 31.
Cached `qj` owners retain real read/write RandomAccessFile descriptors and
their header tables. The legacy `mc_world_regions` whole-file cache remains a
separate transport with its existing contract.

Selector 1 constructs GZIP input over the compressed byte array returned by
`qj`; selector 2 constructs the default wrapped inflater. Both feed the same
incremental parser. `ld` does not close its DataInputStream or request an EOF
read after parsing. Native retirement therefore neither credits a source close
nor drains the stream or forces an unread gzip trailer check. A later explicit
read can still detect a corrupt trailer. This preserves the distinction between
a constructor IOException caught inside `qj` and a later parse/read failure
that must stop loading rather than generate a replacement chunk.

The selected Windows JDK 17 RandomAccessFile provider permits read/write sharing
and denies delete sharing. The native Windows handle uses those same flags,
so a rename while the descriptor is open fails. Unix permits renaming the open
file; the cached descriptor continues to read that file until close. POSIX
opens use `O_CLOEXEC`, declared through the source's POSIX.1-2008 feature profile.
The [region reference](../reference/region.md#current-client-source-transport)
describes the cache, limits and read behavior in more detail.

## Limits and remaining source calls

Source Stores keep the legacy whole-file transport guards. Native chunk
reservations, cache registration, leases, generated owners, GAME stages and
checked retirement use the canonical Store algorithms through explicit source
admission. The default Store bound remains 512 retained or reserved chunks.
Fixed-array shape checks and the existing NBT/header allocation budgets remain
native policies; they do not reproduce every malformed Java array outcome or
Java heap-allocation failure.

Missing `Level` or `Blocks` data reaches an unfinished source `println` call;
coordinate mismatch reaches the relocation diagnostic. These return typed
failures rather than successful misses. The source chunk-save entry performs its real
session check, then stops at bytecode 16 of `ld.a(fd,lm)`, where `rj.d` and its
deflater/output ownership are still required. Region allocation/reuse for saving, the remaining
handler writes and the reached exception/printing methods remain explicit
dependencies. Cache flush and native retirement do not establish source saving,
reload or crash durability.

## Focused qualification

The 2026-10-29 Windows MSVC Debug run passed five tests with no skips:
`storage.source_handler_constructor`, `world.source_construction`,
`world.saved_data_source_ordinary`, `world.saved_data_source_close` and
`storage.source_regions`. A later Windows `world.source_construction` rerun
passed the added retained read-cause checks. These use `/W4 /WX /fp:strict`.

Linux WSL runtime builds with GCC 13.3 and Clang 18.1.3 each passed those five
tests plus `storage.source_handler_reads_ordinary` and
`storage.source_handler_reads_close`: 14 test executions, no failures or skips.
Both used Debug C17, strict warnings as errors, `-fno-fast-math`,
`-ffp-contract=off`, ASan, UBSan and leak detection. Builds used the retained
pinned zlib source with downloads disabled. The GCC test build first exposed a
shadowed typedef name in the World fixture; renaming its parameter preserved
every assertion before the successful rerun.

The World test covers eight constructor scenarios and 278 checks, including
READY existing Worlds, old metadata, retained HC/metadata failures, a malformed
region read, File/String identity and checked cleanup. Region controls exercise
both compression selectors, a corrupt unread gzip trailer, real descriptor
reads after an in-place payload change, platform-specific rename behavior,
flush and new-header creation. These authored runs do not execute an original
game or qualify complete terrain preparation, Display, source save/reload or
gameplay.

Earlier constructor and metadata/session cohorts passed twenty selected tests
per compiler and a 27-case scope, respectively, on Windows, Clang and GCC.
Their ordinary and close-fault read controls pass 257 and 283 checks. The earlier
corrected Windows fixture failure and affected rerun remain part of that record. Legacy storage and
live-original compatibility results also retain their separate scopes.
