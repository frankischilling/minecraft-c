# Full normal finishing observations

The finishing comparison observes complete original normal-chunk finishing,
including local arrays, world-height queries, ordered sky requests and dirty
state. Its C interfaces are described in [normal chunk finishing](../world/finishing.md).
Actual cache generation, loading, unloading, population and propagation remain
separate behavior.

## Original bindings

The supplied archives use these SHA-256 fingerprints:

| Side | SHA-256 |
| --- | --- |
| Client | `af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c` |
| Server | `033a127e4a25a60b038f15369c89305a3d53752242a1cff11ae964954e79ba4d` |

| Behavior | Client | Server | Bytecode offsets |
| --- | --- | --- | --- |
| Full normal finishing | `lm.c()V` | `hi.b()V` | Local rebuild 0–179; neighbor traversal 182–213; dirty write 218 |
| Four neighbor comparisons | `lm.c(II)V` | `hi.c(II)V` | West/east/north/south calls 37/48/59/70 |
| Compare world and local heights | `lm.f(III)V` | `hi.f(III)V` | World-height call 6; interval requests 31/62; dirty writes 36/67 |
| World height | `fd.d(II)I` | `dj.d(II)I` | Border return 25; absent return 40; unsigned column-height return 64 |
| Packed cache key | `yy.a(II)I` | `qa.a(II)I` | Sign and low-15-bit packing 0–36 |
| Cache presence | `ok.a(II)Z` | `jb.a(II)Z` | `Map.containsKey` 12 |
| Cache resolution | `ok.b(II)Llm;` | `jb.b(II)Lhi;` | Null lookup calls load/generate method 28 |
| Load/generate and publication | `ok.c(II)Llm;` | `jb.c(II)Lhi;` | Remove pending 14; generator 78; map publication 95; insertion-list append 107 |
| World light admission and queue | `fd.a(Leb;IIIIIIZ)V` | `dj.a(Lco;IIIIIIZ)V` | No-sky/depth/midpoint/blank guards 0–104; storage 105–325 |

Selected bodies match after explicit class and method correspondence. This
does not establish whole-class equivalence. Earlier [local-array evidence](chunk.md)
and [queue evidence](lighting.md) describe their individual stages.

Generation returns its finished chunk before cache publication at offset 95.
World queries during finishing therefore read the actual existing cache rather
than automatically reading the measured chunk. Packed-key aliases can resolve
objects with other coordinates. The original load/generate method also removes
pending keys, calls lifecycle/population methods, and can reenter generation.
The C cache implements only explicit post-generation map publication and ordered
list append, with valid nonnull entries.

## Genuine setup and fixed inputs

`HeightTraceObservation.java` reuses the genuine world construction and complete
snapshots in `ChunkObservation.java`. Classes load from the supplied archive
through an isolated `URLClassLoader` whose parent is the platform loader.
The observer verifies selected classes' loader and archive origins. It replaces
no original world, provider, cache or queue fields, and never drains lighting.

The corpus uses world seed 23. On the measured server setup, the original spawn
test accepts `(0,64,0)` immediately. The observer requires the genuine constructor
to retain one normal origin chunk, one matching insertion entry, an empty
pending set and an empty queue. The controller independently observes one
`dj.c()V` spawn entry and zero visits to its random-walk step at offset 24.
It does not set a Random seed or rewrite constructor state. An empty client
cache is populated by calling the original cache load/generate method at `(0,0)`.

This setup is a checked property of this seed and these references. Other seeds
can enter the server's unseeded spawn walk and yield different cache and queue
baselines. Such worlds must not be forced to match this corpus.

Six fixed cases use chunk coordinates `(0,0)`, `(1,0)`, `(-1,0)`, `(32768,0)`,
`(2147483647,-2147483648)` and `(2000000,0)`. They exercise ordinary presence,
missing neighbors, negative addressing, a packed-key alias, wrapped coordinates
and the asymmetric border. Two literal block-input patterns vary column
heights. Each case constructs one real normal chunk and invokes its full
finishing method twice. Between passes only the caller-owned block bytes change.
The measured chunk remains unpublished for both passes; already-published
identity visibility is covered by constructed C contracts and bytecode mapping,
not by this original observation corpus.

## Actual height traces and untouched controls

`HeightTraceController.java` launches the observation JVM with JDWP bound to
`127.0.0.1` on an ephemeral port, then attaches using JDI. It requires actual
method-return values and method-bytecode access. Original game classes are
neither translated nor redefined.

Helper begin/end calls bound each measured pass. A thread-filtered breakpoint
at the original world-height method's entry records arguments only when the
receiver, direct original `f(III)V` caller, measured chunk and thread match.
A temporary method-exit request pairs that exact call with its actual returned
integer. Entry/exit arguments and frame depth must agree. Every pass requires
1024 completed pairs in order and zero unmatched entries. The trace records
ordinal, query coordinates, local height and returned world height.

At each begin/end marker, `SUSPEND_ALL` permits a complete bounded inventory of
Java threads and their stacks. The measured non-daemon main thread must be
unique; other recorded threads must be daemon threads without frames from the
original archive's loader. The inventory allows at most 64 threads and 64
frames per thread. These four marker inventories are finite observations;
they do not establish continuous absence of concurrent activity between markers.

The same observation program also runs in a fresh JVM without JDWP. The driver
requires its **entire snapshot file** to be byte-equal to the traced snapshot
file for each case and side. This includes complete setup, before/after local
arrays, cache context, queue order and identities, and both passes. It neither
normalizes away genuine setup differences nor compares only array projections.

The driver independently checks trace order and each result against the recorded
cache, then compares the complete C projection: local data, dirty flag, shared
depth, every live queued kind/bound and all 1024 query records per pass. Cache
and nonqueue world projections must remain unchanged; the raw input edit must
preserve the complete world snapshot. The two original projections must also
agree directly for these checked matching contexts.

## Reproduction

Build `mc_chunk_finish_probe`, then run:

```sh
python scripts/reference/chunk/compare_finish.py --client b1.7.3.jar --server b1.7.3_server.jar --executable build/mc_chunk_finish_probe --work-dir .local/reference/chunk-finish --tested-commit FULL_COMMIT_ID
```

On Windows select the executable under the build's `Debug` directory.
`--java`, `--javac` and `--javap` can select one JDK 17+ installation with
`jdk.jdi`. Configure `MC_ENABLE_FINISH_REFERENCE_TESTS=ON`, the two
`MC_REFERENCE_*_JAR` paths and `MC_REFERENCE_TESTED_COMMIT` to run the equivalent
CTest `reference.chunk_finish`.

`--capture-only` records both originals, traces and untouched controls without
running C. Repeated `--case N` selects fixed indices 0 through 5 for diagnostics;
a partial selection does not satisfy the six-case corpus. A successful full
original-only capture can be exported without running either original again:

```sh
python scripts/reference/chunk/export_finish.py --manifest CAPTURE_DIRECTORY/manifest.json --output tests/world/chunk/finish_fixtures.json
```

The exporter requires all six cases, matching untouched controls and matching
original projections. It retains source-record and manifest hashes plus bounded
lossless compressed replay inputs and complete expected projections. Public
`tools.finish_interop` replays both sides' inputs through C, including all 24576
query records and 48 complete before/after data/queue stages.

The public fixture comes from the successful original-only capture
`.local/reference/finish-fixtures/capture-ca34ded514654e0eb6891161b3feaa50/manifest.json`,
whose SHA-256 is
`4ed39379f4b97d48c9cc0d8587c438dac8d282b21f9085008359e110e928a6bc`.
All six original pairs agree, all twelve untouched controls match their complete
traced snapshots, and all 48 marker inventories pass. Inputs and compiled helper
fingerprints remain unchanged. This capture records source bytes rather than a
commit label. The exported C fixture replay passes on native Windows; current
exact-source platform results are recorded separately in [status](../status.md).
An independent decoder checks every replay field against the original snapshot
and input files without using the exporter or projection helpers. It matches
all twelve binary inputs, 48 complete stages, 6484 queued-region records and
24576 queries, together with all 24 source-record fingerprints and twelve
untouched controls. The fixture SHA-256 is
`6d49c6e2c41cf6423ca082eaba02b26e023ffd8569e4ee9064077df63f779d3b`.

## Bounds, provenance and limits

Compilation copies the three helper sources and uses Java 17 target classes,
disabled annotation processing, and empty class/source/module search paths.
Java environment injection is rejected. The tracing path has been exercised
with native Windows JDK 17 and WSL JDK 21; this runtime coverage does not by
itself establish full-corpus or current-source acceptance. Exact completed
platform results belong in [status](../status.md).

Each debuggee has a 512 MiB heap and a 90-second controller deadline. The driver
bounds the controller process to 120 seconds, compilation and C replay to 30
seconds, and each output stream and complete record to 32 MiB. Ordinary world
snapshots allow at most 512 map/list/pending entries and 32768 queued regions;
the C replay also reserves space for 1024 possible additional regions.
World inventories are observed during execution and recorded afterward. These
limits are not an operating-system process-tree or filesystem quota.

Manifests retain original JARs, actual sources and helper modules, executable,
Python and JDK fingerprints, copied/compiled helpers, commands, records and
world inventories. Inputs and compiled classes are checked again after the run.
The supplied source ID is a label; retained byte fingerprints identify the
actual tested inputs. Missing prerequisites return 77. Original execution,
control, trace or completed comparison failures fail the run.
Genuine original setup unavailability propagates only before every measurement
marker and with no snapshot. The controller retains raw logs and a separate
`.unavailable.json` report without creating a successful trace. Synthetic checks
verify that an exit77 after a begin marker, an exit77 with a snapshot, and a
dependency error during measurement all fail instead of becoming unavailable.

Constructed `world.chunk_finish` tests cover ownership and resource contracts,
packed-key replacement, signed borders, blank/missing objects, admission and
depth arithmetic, atomic preflight, dirty effects, trace order and identity
visibility. They complement actual original observations and do not independently
prove original parity. Neither the fixed corpus nor its public fixtures
establish cache lifecycle, arbitrary provider behavior, propagation, complete
Overworld generation or playable client/server parity.

## Exact-source results

Source `a18615f0d9d90758f0ea95b0218cec97733d2adf` passes all 37 configured
CTests on native MSVC19.51 and WSL GCC13.3/Clang18.1, without skips. Both Linux
builds enable ASan/UBSan. The suite contains 32 public tests, the numerical Java
oracle and four actual-original comparisons: finishing, queue, chunk and nibble.
Original finishing uses Temurin17 on Windows and OpenJDK21 on WSL.

Every build matches all six cases separately between C/client, C/server and
client/server. Each compares 24576 query records and 48 complete data/queue
stages. All twelve untouched controls and 48 marker inventories pass per build.
Each manifest has no errors and retains identical before/after fingerprints for
54 inputs and six helper classes. An independent audit matches all 44 recorded
project source files byte-for-byte to that commit.

Reports remain beneath each build's `reference-chunk-finish/` directory:

| Build | Capture identifier | Manifest SHA-256 |
| --- | --- | --- |
| Windows/MSVC | `a60059037dcc430e919c6d1ac60962b0` | `e316ad859cecb8e85bd428bc1573f56a46c2ca031f97ccae09508158ed8e7b42` |
| WSL/GCC | `b1e77a256714431fa167f4a9ad74fc28` | `4bb63d2c38176c8a4202be1d7cc32c055dfc691a87f639dee649536c4e38a7c5` |
| WSL/Clang | `4f7a4f7d9621417a844a1d88ef9a1a4b` | `b082aee9a9fe386b4d705941add7709f9836d8a9a20d0f8cf9cd3f5411beecf1` |

Native Windows and Linux GCC/Clang hosted checks pass all 33 public/numerical
tests and formatting for 75 C/header files on that source in
[run39075690802](https://github.com/frankischilling/minecraft-c/actions/runs/39075690802).
These results establish the declared fixed scenarios and ownership contracts.

The runtime is integrated at `fa17e3d8885632ca0baecf71bdf9551e47be68eb`.
Final branch source `7c82b4a861e742e717b6ea8fd9646c5611211dba` and the resulting
main both pass all 33 hosted public/numerical tests per compiler and formatting
for 75 C/header files. The integration tree equals the tested branch tree; its
runtime/reference sources remain identical to the locally tested source. See
[branch checks](https://github.com/frankischilling/minecraft-c/actions/runs/39076093215)
and [main checks](https://github.com/frankischilling/minecraft-c/actions/runs/39076208781).
