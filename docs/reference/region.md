# Original region evidence

The verified client class `qj` and server class `jx` implement the original
region record storage. Their [JAR fingerprints](binaries.md) remain pinned.
The byte-span reader and CLI covered by the original observations below are
read-only. Filename parsing, global chunk coordinates, cache lifetime and world
schemas remain outside that evidence. Original classes accept a `File`; their
local indexing alone does not establish those caller behaviors. The later
[client source transport](#current-client-source-transport) has separate
implementation limits and authored qualification.

The recorded investigation used 36 synthetic fixture definitions and 49 queries
per side/runtime on Temurin 17 and Oracle 21, producing 196 query records. Each
query used a fresh generated copy. No user world or original region writer was
used. Raw source, binaries, copies and complete local runtime reports stay
outside public source. This historical investigation does not count as a fresh
live original-region comparison for the current C revision.

## Valid format anchors

Method-relative offsets below were checked in both supplied binaries with:

```text
javap -classpath b1.7.3.jar -c -s -p qj
javap -classpath b1.7.3_server.jar -c -s -p jx
```

| Interface | Evidence on both sides |
| --- | --- |
| Constructor `(Ljava/io/File;)V` | Allocates two 1024-int arrays; location table read at 297..393 and timestamps at 396..425. Locations are at `4*i`, timestamps at `4096+4*i`. |
| Private `d(II)Z` | Local coordinate bounds 0..17 require both coordinates in 0..31. |
| Private `e(II)I` | Local index `x+32*z` at 4..10. |
| Read `a(II)Ljava/io/DataInputStream;` | Bounds check at 3, missing location check at 28..33, packed start/count at 34..45, seek at 74..85, signed length at 92, selector at 152. |
| Compression selection | Selector 1 constructs gzip at 185/198; selector 2 constructs the default Java inflater stream at 237/250. |
| Timestamp storage | Raw big-endian int bits are retained. The inspected writer derives seconds from current time and narrows to int at 423..430. No timestamp writes were invoked by the observations. |

Recorded valid local mappings include `(0,0)`, `(31,0)` and `(0,1)`, plus raw
timestamp patterns `80000000`, `ffffffff` and `12345678`. Embedded Level
coordinates were returned independently of the queried local slot. The region
reader does not establish a chunk/world schema from those values.

The original constructor opens writable storage and may repair a file. The C
inspector reads only and never calls that constructor. It applies bounded
validation to inconsistent extents, overlap, record lengths and selectors;
those diagnostics are separate from valid-format comparisons. Whole-container
checksums follow the [compression module's policy](compression.md).

## Live original comparison

`scripts/reference/region/compare_region.py` runs the production C probe and
untouched client `qj` and server `jx` readers on the complete deterministic
36-case, 49-query corpus. The corpus includes local coordinate and raw timestamp
mapping, missing records, overlapping extents, signed/high-bit offsets,
empty/out-of-range counts, declared length boundaries, compression selectors,
mismatched containers, an unread corrupt gzip trailer, and short/unaligned files.
Gzip fixtures use an explicit fixed header, CRC and size trailer rather than
the host-dependent header of `gzip.compress`.

The C probe reads 36 pristine inputs and must leave all their bytes unchanged.
Each original query receives its own fresh file copy, because even constructing
the original reader can change short or unaligned files. Copies are never
shared between queries, sides or controls. Each side repeats the complete
observation on a separate fresh set of copies; the entire observed/control
JSON snapshot files must match byte for byte. No personal world, original
writer or world/chunk loader is involved.

`RegionObservation.java` uses an isolated URL loader with the platform loader
as parent. It verifies the supplied JAR hash and exact class origins. Bindings
include the public region constructor, synchronized `a(int,int)` stream reader,
`c(int,int)` presence method, `b()` close method, and exact private file,
stream, location/timestamp, free-sector and growth fields. Original utilities
`as`/`ag` parse a `DataInput` into compounds `nu`/`iq` and serialize that root
through `DataOutput`. Root parsing and complete stream draining use separate
actual region-read calls. The helper changes no original field.

Every record retains the request index/coordinates, complete before/after file
fingerprints, constructor outcome, all 1024 location and timestamp values,
free-sector bits, growth counter, presence, canonical root, complete drained
bytes and close outcome. Expected original exceptions retain their class names.
The driver validates complete typed records, ordered coverage, every copy hash
and all metadata against the actual files. Selected class URLs must identify
the exact supplied archive; archive byte counts and fingerprints must agree.

All 49 original records compare between client and server. C is checked against
both originals for eight compatible queries. The other 41 queries are retained
as explicit diagnostic departures: immutable C inspection rejects malformed
framing, extents, overlap, coordinates, selectors or invalid whole containers
where original construction or prefix reads can behave differently. The driver
independently checks those C diagnostics and unattempted-stage outputs. These
counts do not claim C parity with every malformed original outcome. Valid
payload, decompressed and serialized bytes stay exact; only compound child
ordering may differ. Compressed and raw consumption are checked separately.

The helper preflights all requests before invoking an original constructor.
It requires absolute canonical paths under an owned nonsymlink copies root,
distinct per-query files smaller than 64 KiB, at most 256 requests, 8192-byte
lines and a 512 KiB request file. Output is a new file outside the mutable
copies root. Decompressed streams and canonical roots have 1 MiB bounds;
complete JSON and captured streams have 32 MiB bounds. Java children use
512 MiB heaps; driver child processes have 90-second deadlines. These are process/input/output
bounds, not operating-system process-tree or filesystem quotas.

The manifest records the actual executing driver, loaded helper modules,
fixture definitions, project C/header sources, C probe, Python executable,
supplied archives and JDK tools/runtime files. Inputs and compiled helper
classes must retain matching before/after hashes. Raw process logs, complete
snapshots, all copy inventories and comparison classifications remain in fresh
ignored capture directories. A caller's commit label supplements those hashes;
it does not prove source correspondence by itself.

Use JDK17+ containing `java`, `javac` and `javap` from the same installation:

```sh
python scripts/reference/region/compare_region.py --client <client.jar> --server <server.jar> --executable <mc_region_probe> --work-dir .local/region-live --tested-commit <40-hex-commit>
```

For CTest enable `MC_ENABLE_REGION_REFERENCE_TESTS=ON`, set
`MC_REFERENCE_CLIENT_JAR`, `MC_REFERENCE_SERVER_JAR` and
`MC_REFERENCE_TESTED_COMMIT`, then build and run:

```sh
cmake --build build --target mc_region_probe
ctest --test-dir build -R '^tools\.region_(live|probe)$' --output-on-failure
ctest --test-dir build -R '^reference\.region$' --output-on-failure
```

Add `-C Debug` for a multi-configuration build. `tools.region_live` runs 17
Java-free protocol, geometry, omission, provenance, preservation and failure
classification guards. `tools.region_probe` sends a real UTF-8 stdin path
containing Unicode directory/file names to the C probe and checks three ordered
full JSON records, both complete headers, decoded roots and unchanged bytes.
That regression passes on MSVC and WSL GCC/Clang. Windows probe paths are
decoded as UTF-8 before opening through the wide-character file API.

Missing/unsupported environment prerequisites produce exit77/unavailable
before measurement. A wrong archive, malformed record, binding error, child
failure or timeout fails with exit1. Child exit77 during compilation or measured
execution is a failure. Final input/class preservation errors also force failure;
they cannot become an unavailable result. CTest marks unavailable reference
runs skipped, never successful comparisons.

## Verified source checkpoint

Commit `092301ae8282d50a734df9018f791925aa9be99b` passed the live
`reference.region` comparison on native MSVC and WSL GCC/Clang, without skips.
The archived test times are 10.82, 88.13 and 88.00 seconds respectively. Each
capture retains all 36 fixture definitions and 49 ordered queries, both
originals, two complete byte-equal controls, eight compatible queries and
41 diagnostic departures. All 36 pristine C inputs remain unchanged.

The exact local manifests are:

| Build | Manifest | SHA-256 |
| --- | --- | --- |
| MSVC | `build-generation-windows/reference-region/capture-afbcf131bea940babb42f5fce3a1ac92/manifest.json` | `9b6c9697aaaee869c588a267085cf347eb4c56e08623519add7a4e8a293c5e31` |
| GCC | `build-generation-gcc/reference-region/capture-112a50f9774d49bfa06fbbdf5e317119/manifest.json` | `c03b0bbaef4373b9e117722bc9ab54f07f7be0c1a80225847bc60df491db1b18` |
| Clang | `build-generation-clang/reference-region/capture-cbfff5b5a80242868a885281516fd27d/manifest.json` | `935bb9009bc22a051783d352ebf03df3dbef93ada37f0b4ab529175a1bce7fd2` |

An independent standard-library audit, `.local/region-exact-audit.py`, imports
no production validator and starts no JVM or C probe. Its report
`.local/region-exact-audit.json` has SHA-256
`3f706a1fefd55325ed789ae2d83a248ca4833e1c702301e926ff0538bce4ca89`.
For each build it verifies 52 project source fingerprints against immutable
Git objects at the checkpoint, all 62 attributed inputs and seven compiled
classes before/after, complete copy inventories, all 196 original records and
client/server equality. It reconstructs all fixture bytes independently under
the corresponding Windows/Linux Python runtime, retaining valid differences
between zlib encoders. It also checks 180,224 original and 44,032 C
location/timestamp pairs against file bytes, complete original controls and
the C diagnostic classifications. The three region test logs were archived
before later test runs. Subsequent CMake development is recorded separately
from the immutable checkpoint inputs.

[Hosted validation at the same commit](https://github.com/frankischilling/minecraft-c/actions/runs/39082649705)
passed 42 tests on each of Windows, GCC and Clang. Its formatting job checked
87 C source/header files. Those hosted checks and the local live-original
comparisons provide separate evidence; hosted runners do not receive the
supplied original archives.

This completes the finite read-only region checkpoint on the development
branch. It does not establish main-branch integration. Region filename/global
coordinate mapping, world schemas, save writes, allocation/reuse and restart
compatibility remain outside this module's evidence. Cache acquisition has its
own [contract and evidence](../world/cache.md).

## Recorded-observation check

`scripts/reference/region/compare_capture.py` starts only the project's C probe.
It reads a local historical observation manifest and the corresponding generated
files, checks recorded original fingerprints and each before/after file hash,
then selects complete aligned, nonoverlapping files with supported, valid
containers. It does not run Java, an original reader or the capture producer.

The current local capture yields 32 selected observations across four recorded
side/runtime groups. Another 164 observations fall outside this profile. Metadata,
declared payload spans, decompressed bytes and root serialization are compared.
Sixteen serialized roots differ only in compound child order; the same bounded
byte-slice comparison used by raw NBT sorts only compound child records. All
other bytes remain exact. Actual C consumption is reported independently of
historical stream read-ahead.

```text
python scripts/reference/region/compare_capture.py --executable <mc_region_probe> --capture <recorded-manifest.json> --work-dir .local/region-capture --tested-commit <40-hex-commit>
```

For CTest set `MC_ENABLE_REGION_CAPTURE_TESTS=ON`,
`MC_REGION_CAPTURE_MANIFEST=<absolute recorded manifest>` and
`MC_REFERENCE_TESTED_COMMIT=<source commit>`. The optional test is named
`reference.region_capture` and labeled `captured`. Missing captures or probes
return 77/unavailable. Data/execution mismatches return 1/fail. Output directories
containing inputs/source are refused; report paths cannot target fixture files.
A fresh running/failure/unavailable result replaces an earlier report.

The result records C/source/capture/input hashes, selected counts, byte records
and explicit mode `recorded_observations`. Its live original-region check stays
`pending` because this command does not execute an original reader. The live
pipeline above records fresh original execution separately. Historical replay
and live observations both retain the read-only scope; neither establishes full
region/save parity.

## Current client source transport

The client `mx`/`ld` route now uses
[`mc_world_source_chunk_loader`](../../include/mc/world/storage/source_chunk_loader.h)
and a shared
[`mc_world_source_regions`](../../include/mc/world/storage/source_regions.h)
RegionFileCache owner for `rj`. This route uses the handler's same Store and
File namespace.
Ordinary `ld.a` holds the actual world File; the Nether factory constructs the
actual `DIM-1` child before storing it. Construction holds the File without
opening a region. [SaveHandler ownership](../world/save-handler.md) describes
the enclosing World and loader lifetimes.

At the reached `rj.c` read, the source cache constructs the region directory
File and `r.<x >> 5>.<z >> 5>.mcr` File through the existing UTF-16 File owners.
Equal File keys reuse the same `qj` owner before `exists` or `mkdirs` is called.
Different Worlds and dimensions share the bound namespace. At 256 entries, a
new key reaches the source flush before its `qj` constructor. An explicit
flush closes the retained `qj` descriptors before clearing the map. This
implements the shared `rj.a()` body; SaveFormat's `hi.c()` caller remains an
explicit dependency. The legacy
`mc_world_regions` cache still owns whole-file images under its separate UTF-8
root-path API.

Each source `qj` retains its File,
[`mc_java_random_access`](../../include/mc/world/storage/java_random_access.h),
location and timestamp arrays, free-sector state, growth counter and last
modification value. Reads seek and consume bytes from that live descriptor;
they do not read from a retained whole-file image. Cached header entries remain
cached while a later body read can observe an external in-place change. File
keys and descriptors keep their own native occurrences until flush or final
retirement.

This constructor is writable. It opens `RandomAccessFile(File,"rw")`, writes
the source 8192-byte header when the initial length is below 4096, and retains
the original padding loop at the current cursor. It does not silently replace
that loop with an alignment repair. Signed location math, source sector/length
checks and selector decisions remain in their reached order. Missing or
source-rejected records return null. The compressed byte array starts zeroed,
and `qj` makes one bulk descriptor read whose returned byte count is ignored,
as in the source. Native bounds, allocation failures and unsupported providers
remain failures rather than null records.

Selector 1 uses GZIPInputStream over that byte array; selector 2 uses the default
wrapped InflaterInputStream. The incremental NBT parser reads the actual
compressed stream. `ld` neither closes that stream nor drains it after parsing.
The decoder adopts the completed document into the existing GAME payload,
then the canonical import code and final `lm.i` run. Native retirement releases
the stream and document occurrences without inventing a source close or
reading an unrequested trailer. The corrupt-trailer control parses a complete
NBT record successfully, then detects its CRC error only on a later explicit
read.

The Windows provider uses the selected JDK 17 read/write sharing flags without
delete sharing. Renaming an open region is therefore refused on Windows.
Unix permits the rename and keeps the opened file attached to the descriptor.
POSIX opens use `O_RDWR | O_CREAT | O_CLOEXEC`; the source declares the required
POSIX.1-2008 feature profile before its headers. Both providers use the same
File native-path conversion as the other source File operations.

### Native policies and unfinished paths

| Policy | Current bound or behavior |
| --- | --- |
| Source cache retention | Up to 256 strongly retained `qj` owners until capacity flush, explicit flush or native retirement. Java SoftReference collection timing is not modeled. |
| Region extent | Default 256 MiB, with separate signed-int/sector-list admission after the reached constructor effects. |
| Compressed byte array | Default 64 MiB native bound; the original sector-count and declared-length checks also apply. |
| Generated filename String | Default 8 MiB allocation budget in `max_path_bytes`; the File namespace applies its own path and String limits. |
| Incremental compression | Default 1 MiB stream-owner and 128 MiB inflater-allocation budgets, 64 MiB compressed-input and output bounds, and 1024 gzip members. |
| NBT | Default 64 MiB input/output and 128 MiB allocation bounds, 1,000,000 tags/elements, and depth 64. No whole-container validation is forced after a completed source parse. |
| File keys | Exact UTF-16 comparison on Unix. The Windows source cache admits ASCII case-insensitive File keys; non-ASCII WinNTFileSystem case mapping is an explicit dependency. |
| HashMap profile | Ordinary JDK 8+ bucket behavior is admitted. A required tree bin reports its unfinished SDK dependency. |

These budgets are separate native allocation and intake policies, not a shared
Java heap bound. The source decoder also retains its fixed-array shape policy.
Exact allocation-failure timing for the debug StringBuilder calls is outside
this qualification. Failed native and source-call prefixes retain their Files,
descriptors, input owners and typed causes for checked cleanup.

The `qj` read catch can return null for an IOException reached inside that
method, including GZIP construction. Decompression or NBT failures reached
later through `ld` retain a fatal loader cause; they cannot trigger replacement
generation. Constructor and flush IOException diagnostics stop at their
unfinished `printStackTrace` calls. Missing chunk data and relocation retain
their source printing dependencies. Source saving checks the session and then
stops at bytecode 16 of `ld.a(fd,lm)`, before completing `rj.d` output/deflater
ownership.
Source save allocation/reuse, restart compatibility and failed-I/O durability
remain unqualified.

### Focused native results

On 2026-10-29, the Windows MSVC Debug storage-focused run passed five tests
with no skips: the source handler constructor, source World constructor,
ordinary and close-fault HC controls, and `storage.source_regions`. The source
World retained read-cause test also passed its later Windows rerun.

GCC 13.3 and Clang 18.1.3 each built and ran those five tests plus the ordinary
and close-fault source metadata/session tests under Linux WSL. All 14 test
executions passed, with no skips, using Debug C17, strict warnings as errors,
ASan, UBSan and leak detection. Their offline builds used the retained pinned
zlib source. The initial GCC test-only shadow warning was corrected without
changing assertions before the successful run.

`storage.source_regions` performs 75 authored checks over real files. The
control reads a changed compressed body through the warm cache's retained
header offset, then verifies that flush exposes the changed header. It also
checks gzip/zlib parsing, the unread corrupt trailer, the platform's open-file
rename result, fresh header creation and counted retirement. The source World
fixture exercises eight constructor cases and 278 checks, including READY
existing Worlds and a retained malformed-region parse failure. These are
authored native results for the new client transport. They do not extend the
earlier live-original inspector comparison or qualify complete terrain
preparation, Display, source save/reload or gameplay.
