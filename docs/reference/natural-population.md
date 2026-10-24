# Fresh World population comparison

The seed-23 comparison runs the production GAME World against the supplied
Beta 1.7.3 client and server archives. Both original runs construct genuine
persistent base Worlds. C constructs the corresponding client or base-server
load-or-create World with a fresh Store, the real normal Dimension and no saved
metadata or prepared Chunk payloads. This does not establish dedicated-server
startup.

The original constructor selects spawn `(0,64,0)` and retains origin Chunk
`(0,0)`. The `origin` and `ring` profiles then use the actual cache's `c` route.
C computes terrain, features, mutation and lighting through its installed
services; it imports no observed feature results.

The separate [save/readback profile](population-persistence.md) extends the
unchanged origin prefix with actual populated-Chunk persistence. It keeps the
same World alive for two cache saves and four direct region loads.

## Acquisition profiles

The default `origin` profile calls `(1,0)`, `(0,1)`, `(1,1)` and `(1,1)`.
The fourth distinct Chunk completes the origin's population neighborhood. The
last call returns the cached identity without changing the compared state.

The `ring` profile keeps those four calls and continues with `(-1,0)`,
`(-1,1)`, `(0,-1)`, `(1,-1)`, `(-1,-1)` and `(-1,-1)`. It retains the nine normal
Chunks in the square `[-1,1]` on both axes. Four become populated: `(0,0)`,
`(-1,0)`, `(0,-1)` and `(-1,-1)`. The constructor state and ten returns provide
eleven observation boundaries per side.

| Boundary | Normal Chunks | Newly populated | Ordered light regions |
| --- | ---: | --- | ---: |
| Constructor returned | 1 | None | 0 |
| `c(1,0)` returned | 2 | None | 3 |
| `c(0,1)` returned | 3 | None | 9 |
| `c(1,1)` returned | 4 | `(0,0)` | 1,777 |
| Repeated `c(1,1)` returned | 4 | None | 1,777 |
| `c(-1,0)` returned | 5 | None | 1,778 |
| `c(-1,1)` returned | 6 | `(-1,0)` | 2,950 |
| `c(0,-1)` returned | 7 | None | 2,952 |
| `c(1,-1)` returned | 8 | `(0,-1)` | 5,824 |
| `c(-1,-1)` returned | 9 | `(-1,-1)` | 6,968 |
| Repeated `c(-1,-1)` returned | 9 | None | 6,968 |

The first five rows also describe the complete `origin` profile. Its four
normal minimum heights are 64, 66, 67 and 70. Provider temperature `w` changes
from null to a 256-element array at the first population. The ring reuses that
same array through the later three populations; each boundary compares its
current contents as well as its identity. Both repeated hits preserve the
complete compared state.

Negative cache keys follow the original `yy/qa.a(II)I` packing, with independent
coordinate sign bits and fifteen-bit halves. For example, `(-1,0)` maps to
`-65536`, `(0,-1)` to `65535`, and `(-1,-1)` to `-1`. The validator checks each
retained map key against its actual Chunk coordinates and requires the requested
key to publish the returned identity. It assigns comparison identities in
chronological insertion order and checks owner and state continuity between
calls.

## Compared state

The fresh comparison checks the initial state and every return in either
profile:

- Every byte of Blocks, Data, SkyLight, BlockLight and HeightMap for all retained
  normal Chunks, plus minimum height, coordinates, flags and last-save time.
- Actual Chunk identities in the cache map and insertion list, pending keys,
  every ordered light-region kind and bound, World time, spawn and force flags.
- Provider `q/d/e/f/g/h/r/s/t/w` and manager `a/b/c` arrays as complete raw
  binary64 values, including nullable fields and actual lengths. Provider `v`
  and manager `d` compare references to the thirteen actual Biome statics.
- The numerical arrays' cross-field aliases and replacements across all five
  origin or eleven ring boundaries. Native observer holds preserve actual old
  arrays during capture; these holds do not trigger copy-on-write or enter
  source methods. Comparison identities describe aliases within a run; they do
  not compare VM addresses or raw observer IDs between runs.
- Provider and cave Random raw 48-bit state, Gaussian flag and raw cached bits,
  followed by eight `nextLong` results drawn from copies of each complete state.
- Light enqueue/drain depth, shared `Chunk.a`, the falling flag, and complete
  bootstrap console output with platform line endings normalized.

`World.r` starts from a clock-dependent default Random in each process. The
observer records its real state and copied continuation as a separate input;
it never replaces or reseeds that owner. The harness validates its complete
record and requires unchanged state throughout each run. It also checks that
the original Random and its seed backing retain their identities. Neither
profile requires equal clock seeds across Java and C. This result does not
cover a schedule that consumes `World.r`.

## Original authority and observation

[`NaturalPopulationObservation.java`](../../tests/reference/world/population/NaturalPopulationObservation.java)
composes the unchanged original
[`CacheCallbacksObservation`](../../tests/reference/cache/CacheCallbacksObservation.java)
and [`ChunkObservation`](../../tests/reference/chunk/ChunkObservation.java)
helpers. It uses isolated archive loading, exact class/descriptor checks and
read-only reflection. Random continuation uses serialization copies and checks
that the actual seed and Gaussian fields stay unchanged. No original game field
is assigned. Both profiles must also reproduce the complete older four-call
[callback corpus](cache-callbacks.md), whose JDI observations remain a separate
coordinator test.

The [native probe](../../tests/world/runtime/population/natural_probe.c) uses
the real file resource adapter. The fresh comparison extracts and fingerprints
the three required resource files from each verified original archive. Original
resources, binaries and generated Worlds remain local.

### Public replay

Both public replays use the same native probe with a separate, project-authored
minimal resource binding. They require neither a JAR nor a Java process and
start fresh native GAME Worlds without imported Chunk or feature results.

`world.natural_population_replay` keeps the four-call origin scope. It compares
complete cache state with the older original-derived fixture and validates
native numerical transport and repeated-hit stability. It has no original
numerical oracle.

`world.natural_population_ring_replay` compares the complete declared cache
and numerical projection with the public
[ring fixture](../../tests/reference/world/population/ring_fixtures.json).
That fixture retains complete original observations for both sides, compressed
with byte counts and SHA-256 checks. Expected cache projections derive solely
from those original records. Loading rederives the projections, checks the
three Java observer source fingerprints and admits the exact ring schedule.
The replay also compares the original provider/manager arrays, array aliases,
provider/cave Random records and shared state at all eleven boundaries. Each
run's independent `World.r` must remain unchanged.

Only callback class-origin archive URLs are replaced with logical
`supplied:client` or `supplied:server` labels in the public observations. Capture
attribution records the Java profile, seed and observer source bytes/SHA-256;
dirty source cannot claim an exact tested commit. The fixture does not import
native results into its expected values or claim that its minimal resource
binding equals the original resources.

The internal Store iterator borrows actual owners in ownership order, including
unpublished GAME stages. It adds no lease or source eligibility. The probe
checks invalid-ordinal and argument failures preserve their output, then uses
the iterator for complete terminal cleanup after observation. Cleanup failure
fails the process.

## Reproduction and limits

### Committed checkpoint

The later [save/readback checkpoint](population-persistence.md#committed-checkpoint)
at `eee7fb0` includes all three population profiles and all public replays in
577 clean-source local test executions. The origin/ring checkpoint below retains
its original source and capture scope.

At code commit `4414dfd`, complete clean-source local suites pass 188 MSVC
19.51.36260.0, 190 GCC 13.3.0 and 190 Clang 18.1.3 tests: 568 executions, with no
failures, errors or skips. Linux enables ASan, UBSan and leak checks. Each suite
includes both fresh population profiles on both original sides, both public
native replays and the prepared plant comparisons. Windows uses JDK 17.0.17;
Linux uses JDK 17.0.20. All 550 C/header files pass clang-format 18, and all three
hosts pass controller mirrors and 556 positive/2,011 negative lifecycle-interval
controls.

The independent retained audit checks all 1,041 source-export files against
committed Git blobs, allowing checkout line-ending conversion, and verifies the
exact exported bytes in both source locations. It rehashes every declared
comparison input, retained file and process stream in all twelve fresh/public
population captures. It also checks every JUnit case, selected compiler binary
and build configuration. For each compiler, side and profile, the minimal-resource
native run matches the exact-resource native run's entire retained projection
after excluding each run's separately seeded, unchanged `World.r`: twelve
resource-variant pairings. This establishes resource-variant insensitivity for
these finite schedules, not general Registry or resource equality.

The [hosted code run](https://github.com/frankischilling/minecraft-c/actions/runs/39935917481)
passes 185 MSVC 19.44.35229.0, 187 GCC 13.3.0 and 187 Clang 18.1.3 tests, 559 total,
with formatting and all mirror/interval checks. All four actual checkout trees
match the committed code tree. Hosted tests include both public replays; the
nine optional fresh population/plant comparisons retain their local
archive-dependent scope.

### Commands

Configure with `MC_ENABLE_POPULATION_REFERENCE_TESTS=ON`, both
`MC_REFERENCE_CLIENT_JAR` and `MC_REFERENCE_SERVER_JAR`, and explicit JDK17+
`Java_JAVA_EXECUTABLE` / `Java_JAVAC_EXECUTABLE` paths. Set
`MC_REFERENCE_TESTED_COMMIT` to the full clean source commit when qualifying a
committed checkout, then run both fresh profiles:

```sh
cmake --build build
ctest --test-dir build --output-on-failure -R '^reference\.natural_population(_ring)?$'
```

The public replays use the normal test configuration:

```sh
ctest --test-dir build --output-on-failure -R '^world\.natural_population(_ring)?_replay$'
```

Use `--config Debug` and `-C Debug` with a multi-configuration build. The fresh
comparison driver accepts `--profile origin|ring`, defaulting to `origin`;
CTest selects the requested profile explicitly. Each fresh run retains complete
original/native JSON, process streams, tool/source fingerprints, extracted
resources and a report. Public runs retain native JSON and their selected
inputs. Verified source exports carry a complete file manifest for Linux copies.

The harness rejects injected Java/compiler options and checks selected inputs
again after execution. Each child has a 120-second deadline and bounded console
output; each JSON observation has a 64 MiB limit. The validators bound the
retained Chunk inventory to 32 and ordered light regions to 32,768. The public
ring package has a 16 MiB compressed-file limit. These are direct-child and
observation bounds, not operating-system filesystem or process-tree quotas.

This comparison covers one seed and two fixed schedules, with at most four
populated Chunks per side. It does not compare every feature's internal call
trace, immutable noise-layer objects, Chunk backing-object aliases,
cross-language Random-owner graphs, clock/profile equality, arbitrary recursion
or failure paths. Full persistence, unload/reload, other seeds and Biomes, extreme
coordinates, nonempty Entity/TileEntity collections or WorldAccess receivers, and
multiplayer remain separate acceptance work. The original
[controller source map](overworld-population.md) and
[native contract](../world/population/overworld.md) describe the wider implementation.
