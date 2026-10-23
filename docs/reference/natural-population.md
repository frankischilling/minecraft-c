# Fresh World population comparison

The seed-23 comparison runs the production GAME World against the supplied
Beta 1.7.3 client and server archives. Both original runs construct genuine
persistent base Worlds. C constructs the corresponding client or base-server
load-or-create World with a fresh Store, the real normal Dimension and no saved
metadata or prepared Chunk payloads. This does not establish dedicated-server
startup.

The original constructor selects spawn `(0,64,0)` and retains origin Chunk
`(0,0)`. Both implementations then call the cache's `c` route for `(1,0)`,
`(0,1)`, `(1,1)` and a repeated `(1,1)` hit. The fourth distinct Chunk completes
the origin's population neighborhood. C computes terrain, features, mutation
and lighting through its installed services; it imports no observed feature
results.

## Compared state

The initial state and each of the four returns compare:

- Every byte of Blocks, Data, SkyLight, BlockLight and HeightMap for all retained
  normal Chunks, plus minimum height, coordinates, flags and last-save time.
- Actual Chunk identities in the cache map and insertion list, pending keys,
  every ordered light-region kind and bound, World time, spawn and force flags.
- Provider `q/d/e/f/g/h/r/s/t/w` and manager `a/b/c` arrays as complete raw
  binary64 values, including nullable fields and actual lengths. Provider `v`
  and manager `d` compare references to the thirteen actual Biome statics.
- The numerical arrays' cross-field aliases and replacements across all five
  boundaries. Native observer holds preserve actual old arrays during capture;
  these holds do not trigger copy-on-write or enter source methods.
- Provider and cave Random raw 48-bit state, Gaussian flag and raw cached bits,
  followed by eight `nextLong` results drawn from copies of each complete state.
- Light enqueue/drain depth, shared `Chunk.a`, the falling flag, and complete
  bootstrap console output with platform line endings normalized.

| Boundary | Normal Chunks | Ordered light regions |
| --- | ---: | ---: |
| Constructor returned | 1 | 0 |
| `c(1,0)` returned | 2 | 3 |
| `c(0,1)` returned | 3 | 9 |
| `c(1,1)` returned | 4 | 1,777 |
| Repeated hit returned | 4 | 1,777 |

Only origin becomes populated. The four minimum heights are 64, 66, 67 and 70.
Temperature `w` changes from null to a 256-element array. The repeated hit
preserves the complete compared state.

`World.r` starts from a clock-dependent default Random in each process. The
observer records its real state and copied continuation as a separate input;
it never replaces or reseeds that owner. The harness validates its complete
record and requires unchanged state throughout each run. It also checks that
the original Random and its seed backing retain their identities. Thus this
schedule does not require equal clock seeds across Java and C. This result does
not cover another schedule that consumes `World.r`.

## Original authority and observation

[`NaturalPopulationObservation.java`](../../tests/reference/world/population/NaturalPopulationObservation.java)
composes the unchanged original
[`CacheCallbacksObservation`](../../tests/reference/cache/CacheCallbacksObservation.java)
and [`ChunkObservation`](../../tests/reference/chunk/ChunkObservation.java)
helpers. It uses isolated archive loading, exact class/descriptor checks and
read-only reflection. Random continuation uses serialization copies and checks
that the actual seed and Gaussian fields stay unchanged. No original game field
is assigned. The fresh untraced World projection must also reproduce the
complete older [callback corpus](cache-callbacks.md), whose JDI observations
remain a separate coordinator test.

The [native probe](../../tests/world/runtime/population/natural_probe.c) uses
the real file resource adapter. The fresh comparison extracts and fingerprints
the three required resource files from each verified original archive. Original
resources, binaries and generated Worlds remain local.

Public `world.natural_population_replay` uses the same native probe with a
separate, project-authored minimal resource binding. It compares complete cache
state with the older original-derived fixture and validates numerical transport
and repeated-hit stability. It has no original numerical oracle. The fresh
comparison above retains the exact supplied-resource profile.

The internal Store iterator borrows actual owners in ownership order, including
unpublished GAME stages. It adds no lease or source eligibility. The probe
checks invalid-ordinal and argument failures preserve their output, then uses
the iterator for complete terminal cleanup after observation. Cleanup failure
fails the process.

## Reproduction and limits

Configure with `MC_ENABLE_POPULATION_REFERENCE_TESTS=ON`, both
`MC_REFERENCE_CLIENT_JAR` and `MC_REFERENCE_SERVER_JAR`, and explicit JDK17+
`Java_JAVA_EXECUTABLE` / `Java_JAVAC_EXECUTABLE` paths. Set
`MC_REFERENCE_TESTED_COMMIT` to the full clean source commit when qualifying a
committed checkout, then run:

```sh
cmake --build build
ctest --test-dir build --output-on-failure -R '^reference\.natural_population$'
```

Use `--config Debug` and `-C Debug` with a multi-configuration build. Each run
retains a fresh capture directory, complete original/native JSON, process
streams, tool/source fingerprints, extracted resources and a report. Verified
source exports carry a complete file manifest for Linux copies. The harness
rejects injected Java/compiler options and checks selected inputs again after
execution. Each child has a 120-second deadline and bounded console output;
each JSON observation has a 64 MiB limit. These are direct-child and observation
bounds, not operating-system filesystem or process-tree quotas.

This is one seed, one four-call schedule and one populated origin per side.
It does not compare every feature's internal call trace, immutable noise-layer
objects, Chunk backing-object aliases, cross-language Random-owner graphs,
clock/profile equality, arbitrary recursion or failure paths. Persistence,
unload/reload, other seeds and Biomes, extreme coordinates, nonempty gameplay
observers and multiplayer remain separate acceptance work. The original
[controller source map](normal-population.md) and
[native contract](../world/population/normal.md) describe the wider implementation.
