# Persistence reference evidence

Status: **DEVELOPMENT**. Static mapping and three original-only reference
packages are complete. Public C replays and native owner tests exercise their
declared boundaries; final exact-source acceptance and the complete owned
copied-world original/C roundtrip proof remain pending. None of the producer
hashes below establishes acceptance of the current working tree.

All observations use the client and server pinned in [binaries.md](binaries.md).
Obfuscated names are side-specific. Original bytecode, disassemblies, class
files and private process logs remain outside public source. The public packages
contain fingerprints and helper-generated persistence records, without original
class or method bytes. [region.md](region.md), [nbt.md](nbt.md) and
[compression.md](compression.md) retain the earlier reader/codec evidence.

## Static method mapping

The ignored `.local/persistence-foundation-research/` investigation checked fresh
`javap -c -p -s` output from both archives. Its paired checks preserve instruction
offsets, opcodes, branch targets and exception tables while explicitly rebinding
side-specific names and constant-pool indices. The global region, local region,
region-loader and static chunk-schema class pairs agree under those bindings;
13 selected metadata method pairs were checked separately. This is static
evidence, independently of the runtime packages below.

| Boundary | Client | Server | Instruction anchors |
| --- | --- | --- | --- |
| Global region open/cache | `rj.a(File,int,int):qj` | `kq.a(File,int,int):jx` | Signed shifts at 30/41 form the filename; lookup 63, `SoftReference.get` 80, live return 95; miss-triggered full clear 122. |
| Global stream access | `rj.c/d(File,int,int)` | `kq.c/d(File,int,int)` | Resolve region at 3; local `&31` masks 11/15; delegate at 16. `c` reads, `d` writes. |
| Local region commit | `qj.a(II[BI)V` | `jx.a(II[BI)V` | Required sectors 23..33, oversized return 43; reused-run location precedes payload, appended-run payload precedes location; timestamp follows successful payload. |
| Metadata load | `fm.c()Lei;` | `dq.c()Lct;` | Primary gzip read 29/info construction 45; backup read 83/construction 99; final null 109. |
| Metadata serialization | `ei.a(Lnu;Lnu;)V` | `ct.a(Liq;Liq;)V` | Private serializer samples a new clock at 63 and writes `LastPlayed` at 66. |
| McRegion list-save wrapper | `mx.a(Lei;Ljava/util/List;)V` | `ie.a(Lct;Ljava/util/List;)V` | Sets version 19132 at 4, delegates to the inherited list save at 10. The inherited one-argument handler save does not force that version. |
| Actual region load/write | `ld.a(Lfd;II)Llm;`, `ld.a(Lfd;Llm;)V` | `hb.a(Ldj;II)Lhi;`, `hb.a(Ldj;Lhi;)V` | Load checks `Level`/`Blocks`, decodes, then calls postload at 272. Write closes the region stream at 61, then adds consumed growth to `SizeOnDisk` at 89..94. |
| Static chunk schema | `to.a(Lfd;Lnu;)Llm;`, `to.a(Llm;Lfd;Lnu;)V` | `mg.a(Ldj;Liq;)Lhi;`, `mg.a(Lhi;Ldj;Liq;)V` | Decoder imports the five arrays at 31..99; writer uses live world time for `LastUpdate` at 28/31. |
| Postload normalization | `lm.i()V` -> `vi.a([B)V` | `hi.h()V` -> `nn.a([B)V` | Frozen 256-entry block-ID lookup, without rebuilding heights or lighting. |

The global open/stream rows use Java-style argument names for readability. Exact JVM
descriptors, modifiers, class fingerprints and selected method-code fingerprints
are retained by the project-owned helpers and validators. The `to/mg` instance
methods also implement a legacy per-chunk gzip-file route; the static schema
methods above must not be confused with that transport.

The global path is `region/r.<regionX>.<regionZ>.mcr` beneath the selected chunk
root, with arithmetic signed `regionX = x >> 5` and `regionZ = z >> 5`.
The runtime region package observes nine global filename/open requests, including
negative boundaries and signed extremes. It does **not** invoke global writes at
all nine coordinates. The local `&31` write masks are statically mapped; native
negative/extreme write tests are constructed C controls, not additional original
global-write observations.

## Three original-only packages

Every package retains both originals and an untouched same-call control per
side. The exporters revalidate retained process records, tools, class origins,
bindings, complete expected artifacts and before/after fingerprints before
deriving public replay inputs. Original-only producers execute no C probe and
carry no C-produced expected output.

| Package | Frozen public evidence | Finite replay scope |
| --- | --- | --- |
| Level metadata | [level_fixtures.json](../../tests/reference/storage/level_fixtures.json) | Four contexts, each with 10 codec inputs and 16 handler cases. Pure codec replay has 80 calls: 40 decode, 32 fresh serializer and eight handler-saved-schema checks. |
| Region writing | [region_write_fixtures.json](../../tests/reference/storage/region_write_fixtures.json) | Four fresh/import-copy profiles, 34 writes and complete image/growth frames; 26 successful writes and eight unchanged-image oversized branches. The observed original runs retain 54 intermediate snapshots in total. |
| Chunk storage | [chunk_storage_fixtures.json](../../tests/reference/storage/chunk_storage_fixtures.json) | Four contexts, each with 28 cases and 88 raw NBT artifacts. The 268 C calls comprise 196 original-derived codec/normalization/serialization checks or declared departures, plus 72 separate opaque-retention/refusal policy checks. |

The current producer manifest identities are:

| Package | Ignored producer directory | Manifest SHA-256 |
| --- | --- | --- |
| Level | `.local/level-fixture-producer/capture-90d00c7a2af84972bd8e754e3d2904b9/` | `95f69061fb4985fcfc7de9e02240f9702b2c57438bbc541f51e194bba0067095` |
| Region writing | `.local/persistence-foundation-research/region-runtime/production/capture-07d5d22ac6db47cf961370cd7b8b590c/` | `b3159f86456007f866c5099541a2e7ef4b3828cf4d184fab802a1f4a60d9db06` |
| Chunk storage | `.local/chunk-storage-fixture-producer/capture-0484da7f40744da58f0766678a3beada/` | `d1d8ad9e9a1f802ddb89ee9f0ff5272bbe57d9ae8313d26cd54d80b86ddab8cc` |

All three record `tested_commit: null`. Level/chunk producers archive executed
helper/driver sources and before/after fingerprints for the wider source
closure; they do not archive every historical C source and CMake file. Those
fingerprints establish historical attribution, not independent reconstruction
of an exact Git checkpoint. Final acceptance must bind fresh captures to the
actual source objects and executable bytes. A caller-supplied commit label
alone does not supply that proof.

### Metadata and clocks

`LevelObservation.java` invokes genuine `ei/ct` codecs and `mx/ie` handlers.
Cases cover missing fields and `Data`, exact wrong types including
`Player.Dimension`, nonzero Byte booleans, complete signed scalar values,
UTF-16 NUL/unpaired/pair code units, retained versus empty-list Player handling,
primary/old fallback, ignored `level.dat_new`, two saves and session conflicts.

The constructor creates `data`, optionally creates `players`, and writes an
eight-byte big-endian session token for **both** boolean values. That boolean is
not a read-only mode. Session checking reads one long and allows trailing bytes;
it provides timestamp-token checking rather than OS exclusion. Missing, short,
conflicting and older tokens are observed separately.

A missing `Data` child produces defaults and prevents fallback. A throwing
primary read/type conversion tries `level.dat_old`. Original metadata output
uses a newly sampled `LastPlayed`, leaving the existing Info field stale; the
McRegion list-save route also changes version to 19132. The one-argument Info
serializer retains loaded Player, while the empty-list serializer drops it.

Controls compare complete typed metadata trees with compound order permitted to
differ. Each sampled output `LastPlayed` is independently checked against its
recorded clock interval before exclusion from cross-run equality. Complete raw
files, session bytes, inventories and process clocks remain available. These
are not byte-equal whole-snapshot controls, and the 80 pure codec replays do not
compare native handler I/O or locking to the original handler.

### Region allocation, growth and lifetime

`RegionWriteObservation.java` calls actual `rj/kq`, `qj/jx` and `hz/fh` methods
on fresh helper-owned files. Closing the deflater output stream is the first
commit; before close, the new slot remains absent and the file contains only
the zero headers. Direct commits provide bounded valid compressed lengths that
discriminate `(length+5)/4096+1`: 4090 occupies one sector, 4091 occupies two,
and 1,044,475 returns without changing either a new or existing slot.

The retained allocation sequence checks same-run rewrite, freed-run first fit,
whole-run append, unchanged suffix/dead bytes, headers, timestamps, consumed and
reset growth, close/reopen and complete decoded payloads. Each C replay receives
the actual compressed input and timestamp from the original write, so complete
image equality does not claim independent compressor byte identity or a shared
clock. The oversized original method returns void; the C record-too-large status
is an explicit diagnostic for its observed unchanged-image branch.

Nine global open requests establish filenames and retained hits. A hit at 256
entries retains the cache; the next miss closes all 256 held handles and leaves
one cache entry. The helper strongly retains every returned region to make this
finite observation deterministic. It does not establish natural SoftReference
collection, LRU behavior or a general bound of 256 live original handles.

Observed/control comparisons retain exact non-snapshot call sequences, arguments,
identities and results; complete final state and every final file byte must
agree except explicitly written timestamp slots. Each excluded timestamp is
independently bound to its write interval. Raw images are retained unchanged.
This is not JDI evidence of every nested invocation.

### Chunk schema and actual loaders

`ChunkStorageObservation.java` uses genuine persistent client `hi -> mx -> fd`
and base-server `ie -> dj` construction in complete owned copies of an original
saved baseline. Those server observations describe the base-world API, not
dedicated-server startup. They invoke static `to/mg` schemas and actual `ld/hb`
region loaders/writers without assigning original game fields or replacing
providers. A genuine world-time API sets 1234 without ticking the world.

Records retain all five arrays, coordinates, minimum, population and transient
flags, `q`, `r`, frozen normalization map, entity/tile owners, full surrounding
cache/queue state, raw input/output NBT, inventories and bounded thread evidence.
Observation/control JSON and every NBT artifact are byte-equal within each side;
paired-side comparisons retain complete supported projections and typed trees,
allowing compound order only. Loaded minimum and transient fields remain zero;
`LastUpdate`, including a wrong-typed value, does not restore `r`. The direct
loader does not perform the cache coordinator's later live-time stamp.

Wrong-location input keeps the coordinates stored in `Level`: the original
loader's apparent relocation writes root-level keys and decodes unchanged Level
again. Writing that result addresses its stored coordinates. Missing or short
arrays are retained by the original decoder without unsafe voxel getter calls;
the actual loader returns null for missing `Level` or `Blocks`. Wrong exact tag
types remain recorded exceptions.

The corpus includes unknown entity/tile IDs and a genuine factory-loaded and
serialized Boat and Sign. Original unknown factories discard unsupported
objects; entity loading can still set `q`, and serialization recomputes it.
The original fresh region writer drops unknown root/Level tags. These finite
observations do not implement or validate every entity/tile subclass.

## Native ownership and deliberate departures

[writer.h](../../include/mc/storage/region/writer.h) owns a bounded mutable region
image. Defaults are 64 MiB image and 128 MiB peak allocations, with absolute image
limit `0x7ffff000`; opaque compressed records are capped at 1,044,474 bytes.
Import validates allocation-safe extents, alignment, header size and overlap,
while preserving malformed payload/selectors for the unchanged read-only
diagnostics. Failed in-memory writes preserve bytes, allocation state, growth
and borrowed spans. Original malformed-file repairs and partial I/O failure
behavior are deliberately excluded.

[regions.h](../../include/mc/world/storage/regions.h) supplies bounded UTF-8 native
paths, signed global addressing, cached images and checked sibling-file
replacement. Its default retained cache budget is 128 MiB. Windows paths use
strict UTF-8 to wide APIs. C keeps deterministic strong image ownership until
clear/eviction; this differs from original SoftReferences. Reads may create
missing region headers. Existing malformed allocation tables are not repaired
or overwritten during a read. There is no fsync, crash transaction, path-alias
resolution, concurrent-owner exclusion or filesystem quota guarantee.

[chunk.h](../../include/mc/world/storage/chunk.h) narrows imports to the normal
fixed lengths: Blocks 32768, three nibble arrays 16384 each and HeightMap 256.
It preserves unknown root/Level fields and nonempty entity/tile lists opaquely,
without factories or attachment. Opaque records permit only unchanged
coordinates, arrays and population plus explicit LastUpdate; unsafe edits are
refused. Normalization that changes blocks can therefore prevent a later opaque
rewrite. This preservation policy is stronger than the original model route and
is tested separately from original serializer equivalence.

[store.h](../../include/mc/world/storage/store.h) borrows a shared region owner,
copies its supplied frozen block map and retains stable chunk identities, bounded
by 512 by default. It supports roots for dimensions 0 and -1, owns metadata
strings/trees and checks session prefixes. Resource failures are fatal rather
than selecting older metadata or generating replacements. Checked metadata
rotation completes `level.dat_new`, preflights current/old regular files, then
stops on the first failed delete/move and retains recovery files. Unknown
metadata is preserved. Save inputs must refer to the currently owned opaque
Player; replacement, introduction or dropping is refused. Successful saves
rebind strings and Player to the new owned document; borrowed views expire on
successful load/save or free.

Metadata saves use explicit clock/version 19132 while retaining the requested
loaded `last_played` in the live view. Chunk transport leaves dirty/time/other
runtime flags unchanged and updates live `SizeOnDisk` with Java wrapping only
after successful region writing. The acquisition adapter alone allows missing
records to fall through; malformed/resource failures are fatal, and nonempty
opaque entities/tiles are rejected before retention/activation. Generic storage
loads still support preservation. Retire external cache/list/view references
before free. Free never saves, deletes session.lock or clears a shared cache.

The public native owner checks use fresh temporary roots and recorded inputs.
They cover 52 paired original-derived metadata inputs, 56 paired chunk imports,
both dimensions and signed extremes, stable ownership, real loaded acquisition
with empty registration callbacks, checked failures and transport byte limits.
These C-created and C-reloaded files are not yet the complete copied-world
original-to-C-to-original and C-to-original-to-C acceptance proof. Player/entity
simulation, scheduled-data interpretation, unload/save scheduling and terrain-feature bodies
remain outside these storage boundaries.

## Reproduction

Run from the repository root. Live observations require the pinned archives and
a complete JDK17+; `java`, `javac` and, where used, `javap` must come from the same
installation. Java is reference tooling, not a native executable dependency.
The level/chunk drivers accept `--java`, `--javac`, `--javap`; region-write
accepts `--java`, `--javac`. Injected Java option/classpath environments are
rejected. Work roots must be owned, isolated paths; never supply a personal world.

```sh
python scripts/reference/storage/compare_level.py --client <client.jar> --server <server.jar> --executable <mc_level_probe> --work-dir .local/reference-level --tested-commit <40-hex-commit>
python scripts/reference/storage/compare_region_write.py --client <client.jar> --server <server.jar> --executable <mc_region_writer_probe> --work-dir .local/reference-region-write --tested-commit <40-hex-commit>
python scripts/reference/storage/compare_chunk_storage.py --client <client.jar> --server <server.jar> --executable <mc_stored_chunk_probe> --work-dir .local/reference-chunk-storage --tested-commit <40-hex-commit>
```

For original-only producers, omit `--executable` and add `--capture-only` to
level/chunk, or `--mode original-only` to region-write. Each run creates a fresh
capture directory. Export only a successful original-only manifest:

```sh
python scripts/reference/storage/export_level.py --manifest <level-capture/manifest.json> --output .local/level-fixtures.json
python scripts/reference/storage/export_region_write.py --manifest <region-capture/manifest.json> --output .local/region-write-fixtures.json
python scripts/reference/storage/export_chunk_storage.py --manifest <chunk-capture/manifest.json> --output .local/chunk-storage-fixtures.json
```

Public replay needs Python and built C probes, without a JDK or supplied archives:

```sh
python tests/tools/storage/test_level.py --executable <mc_level_probe>
python tests/tools/storage/test_region_write.py --executable <mc_region_writer_probe>
python tests/tools/storage/test_chunk_storage.py --executable <mc_stored_chunk_probe>
python tests/tools/storage/test_regions.py --executable <mc_world_regions_probe>
python tests/tools/storage/test_store.py --executable <mc_world_store_probe>
```

CTest registers these public tool checks plus the C units `storage.level`,
`storage.region_writer` and `world.stored_chunk`. To add the three live original
tests, configure `MC_ENABLE_STORAGE_REFERENCE_TESTS=ON`, set
`MC_REFERENCE_CLIENT_JAR`, `MC_REFERENCE_SERVER_JAR` and
`MC_REFERENCE_TESTED_COMMIT`, then build the probes and run:

```sh
cmake --build build --target mc_level_probe mc_region_writer_probe mc_stored_chunk_probe mc_world_regions_probe mc_world_store_probe
ctest --test-dir build -R '^reference\.(level|region_write|chunk_storage)$' --output-on-failure
```

Use `-C Debug` for a multi-configuration build. Original JVM children have
90-second deadlines; level uses a 128 MiB heap, region/chunk use 512 MiB.
Captured streams and owned inventories are bounded and monitored, without an
OS process-tree/filesystem quota claim. Missing environment prerequisites can
return 77 before measurement. Wrong archives/bindings, measured child failures
including exit77, timeouts, malformed records or failed preservation checks
return failure; unavailable runs are skipped, never counted as comparisons.
