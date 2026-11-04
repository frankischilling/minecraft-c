# World query performance

## Block static-name selection and cache validation

The Block static loader and passive inspector share a bounded decoder for the
existing 96 names. The loader still performs class active use before selection;
the inspector does not initialize the class. Each successful call reads the
current nullable named cell. Argument admission, entered/failed owners, output
preservation and complete diagnostics retain their existing order. No Block
value or class state is cached.

The [Windows CPU receipt](../../tests/benchmarks/static_cpu_windows.json) records
four alternating rounds and eight fresh processes for the actual public getter
on a READY client Registry. Median cost falls from 168.526 to 28.962 ns/get,
82.814% lower using the unrounded observations. Every process passes 1,011
native controls. Separate persistent controls cover the complete name domain,
cold/entered/failed owners, live rebinding, NULL cells and unchanged failures.
These are finite native CPU results, with no new original-method or FPS claim.

Generic cache validation keeps the existing path through 512 entries. Larger
maps use allocation-free 512-entry hash batches and the existing 1,024-word
scratch size. Each batch checks its live Chunk values and duplicate keys, then
checks later keys against that batch. A 64-probe collision bound returns to
the complete pairwise validator. Insertion entries still receive their original
validation; no cache, Source state or persistent trust flag is changed.

The same receipt keeps all three candidate histories. The selected third
candidate has these changes in median QPC time for the declared validation
workload:

| Entries | Mixed keys | Keys in one hash bucket |
| --- | ---: | ---: |
| 513 | 96.625% lower | 7.552% higher |
| 1,024 | 97.323% lower | 3.343% higher |
| 4,096 | 93.703% lower | 2.592% higher |

Each process passes 780 semantic controls, including boundary and cross-batch
duplicates, live entry edits, malformed descriptors, NULL/data/Blank values and
insertion-list rules. The collision regressions and earlier rejected candidates
remain in the receipt. QPC elapsed time is primary; raw process CPU counters
are retained even when their coarse resolution produces zero. Captured timings
belong to their frozen builds. The public receipt keeps all 32 processes and
440 timed samples across the four distinct pairs. Its auditor and eight
corruption-test methods pass.

The published workloads have separate Windows semantic smoke checks: 1,032
Block controls after additional untimed fixture checks, and 780 cache controls.
They supply no comparison or timing credit. Public include/timer/formatting
projections do not rebind the historical binaries; the Block observer's new
POSIX host-clock path has no Linux timing qualification. The default
`world.cache_validation_controls` also passes all 780 checks on both Windows
and Linux without enabling optional benchmark builds.

Both first frozen full suites retain a sole failure from the older Workbench
packet's exact runtime binding. A fresh four-profile successor now supplies
current source/capture bindings and passes all strict audits; it does not
replace those full-suite results. The final Linux suite passes 674/674;
Windows remains FAIL at 684/685 at the Pig draw-prefix gate, where frustum
selection prevents the model callback. Its cause remains unresolved, and
neither that failure nor these CPU measurements establish an FPS result.
The [test record](../testing.md#static-registry-selection-cache-validation-and-save-progress)
keeps both full attempts, focused controls and measurement scopes separate.

Audit the retained receipt without building or launching native code:

```sh
python -B tests/tools/world/benchmark_static_cpu.py
```

With `MC_BUILD_BENCHMARKS=ON`, the public source targets are
`mc_benchmark_block_static_getter` and `mc_benchmark_cache_validation`.
The runner's explicit `--kind block` or `--kind cache`, `--baseline`,
`--candidate` and fresh `--artifacts` arguments collect a new pair. Preserve
actual build/input identities and pass controls before timing; a new build
does not inherit the retained measurements.

## Source saving and passive observations

The concrete GAME Source full-save caller now uses an internal owner/stage
callback. Its actual LoadingScreen callback never reads the mutable save
context. A synchronous stack bridge enters the unchanged full lifecycle, with
the same progress, session, metadata, SavedData and Chunk call order. NULL
still means no progress for that call. The public mutable-context API and
configured-options callback keep their permanent raw-export behavior. An actual
public borrow inside a stage callback also keeps the provider permanently
generic. See the [saving contract](../world/saving.md#source-progress-ownership).

Eligibility-only fixture observations use a scalar count from the retained
World's current provider. They no longer export an unused mutable descriptor.
This is limited to the selected fixtures, which do not author a provider
replacement. A captured descriptor and the current provider can differ after
rebinding; general equivalence for that case is not claimed. The scalar route
also validates cache metadata. Intentional mutable-alias, malformed-cache and
terminal identity controls retain their explicit raw access. No focus input,
recovery action, frame, clock, Source save or gameplay assertion is repaired.

## Retained Linux phase diagnosis

A private isolated-X11 Food capture measures a 36.165-second random-Chunk body.
Its inclusive acquisition interval is 34.936 seconds; 32 actual population
callbacks account for 33.716 seconds and 52 generation calls for 2.309 seconds.
These nested instrumented intervals overlap and cannot be added. All 52 actual
loads return MISS, taking 2.117 ms in total. A later 5.367-second lighting drain
visits 499 regions and 22,672 cells, with 187,841 generic GETs and four nibble
writes. These observations identify reached work, without assigning the whole
interval to cache validation or generation.

The first random body makes 1,235,464 GET attempts, all generic and none handled
by the owned index. The historical Food observer unconditionally called
`bind_fields` before first admission, permanently exporting that provider.
Pre-attempt snapshots show owned=false, raw_exports=1 and cache counts 310–362;
the whole capture's maximum is 382. The larger-than-512 validation change was
therefore not exercised by that stall. The Source progress escape above is a
separate production exposure, not the established export in this capture.

The failed Linux Food captures and their exact timing/input profiles remain
retained. Removing an observer export does not retrospectively turn them into
an unmodified baseline or qualify Linux Food interaction. No current scene-FPS,
full-frame parity or unrestricted-play improvement follows from these CPU
measurements or passive timings.

## Render ChunkCache reads

The section compiler and RenderBlocks use an internal failure-only Block
accessor. It performs the same admission and source read as the public
ChunkCache accessor, once. Success leaves its optional error output untouched;
a failure materializes the complete canonical diagnostic, including unreached
descendants. The public accessor retains its complete success record.

This avoids initializing and copying a 7,352-byte error record for each
successful scalar read. The [fixed-work EW comparison](../../tests/benchmarks/chunk_cache.md)
checks actual saved World/ChunkCache owners, arrays, read values, failures and
holds. Its separate 84-sample Windows confirmation reduces median time per
65,536 reads from 8.39885 to 0.74005 ms (91.19%). This is CPU API work, not an
FPS measurement. Original geometry replays and loaded scenes cover the callers.

## Random Chunk reads

The World scheduler's random-Chunk child uses an internal failure-only raw Block
accessor. It shares the public accessor's admission and checked read body, reloads
the current data and Block array on every read, and preserves captured normal and
Blank Chunk behavior. Complete diagnostics are constructed on failure. The public
accessor still supplies its complete 128-byte success record; the internal
caller's error alone stays untouched on success. Capture inspection fields,
absolute read counts, retained failures, World errors and ownership holds remain
unchanged.

The [random-Chunk CPU benchmark](../../tests/benchmarks/world_random_tick.md)
measures actual `mc_world_runtime_random_tick_begin` over 361 distinct saved
Chunks and all 28,880 samples per call. Loading, warmup, the independent scalar
oracle, inspection, outer retirement and final state checks stay outside timing.
The two homogeneous workloads use air with an active ambient cooldown and stone
with ambient checks reached. Enabled Block virtuals are exercised by separate
controls.

The initial Windows cohort has 42 samples per variant/workload; a separate
confirmation has 84. Both variants share one observer and 31 identical archives,
with two runtime objects overridden in the candidate.

| Cohort/workload | Baseline median ms | Candidate median ms | Baseline mean ms | Candidate mean ms | Baseline p95 ms | Candidate p95 ms |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Initial air | 5.43535 | 5.11545 | 5.79623 | 5.32856 | 7.4992 | 6.7411 |
| Initial stone | 4.99200 | 4.68580 | 5.06727 | 4.76815 | 5.6243 | 5.3099 |
| Confirmation air | 5.06880 | 4.31255 | 5.10261 | 4.40022 | 5.8014 | 4.8984 |
| Confirmation stone | 4.77915 | 4.32035 | 4.80397 | 4.37188 | 5.1728 | 4.9637 |

All 504 recorded timings, warmup rows, process order and outliers remain in the
[receipt](../../tests/benchmarks/world_random_tick_windows.json). Initial air
round 1 has a slightly worse candidate median. Confirmation stone retains a
higher candidate maximum, 5.6478 ms versus 5.5441 ms, and some individual process
p95s favor baseline. Confirmation mean and median are lower in every matched
round and both order strata; the percentage varies between cohorts.

Independent frozen-baseline controls pass 510 paired accessor checks, including
complete error/state records, success sentinels, error=NULL, busy/read-limit
refusal, source-array changes and unrepaired capture retirement. The promoted
public benchmark also passes all 15 existing genuine-World profiles against the
exact frozen C baseline: 89,842 checks per variant and all 76 complete output
hashes match each other and the captured controls. This final run qualifies the
public workload/control-entry projection with the current combined dependencies;
it supplies no replacement timing samples. Persistent `world.chunk_tick` and
`world.random_tick` tests pass after promotion.

These measurements cover the declared random-tick CPU work. They are separate
from the EW accessor comparison above; their percentages cannot be added or
converted into a scene-FPS or full World.tick result. Source-reference parity
remains scoped to the linked fixtures.

## World reads

Successful scalar World reads no longer construct the complete acquisition
diagnostic tree. A private failure-only getter performs the same single lookup
and preserves its output on success. Failure still supplies the complete
receipt. Public detailed getters retain their success-reset behavior.
Light propagation materializes its canonical success receipt once per operation,
preserving the distinct earlier unreached state.

Scalar queries and light propagation read the same four live provider bindings
directly. They avoid constructing the full provider view on every GET, while
retaining admission, lookup order, leases and failure diagnostics. An
[84-sample Windows CPU comparison](../../tests/benchmarks/light_provider_views_windows.json)
reduces median fixed-work Light-drain time from 3.1315 to 2.92625 ms (6.55%).
This measures the specified drain, not scene FPS.

The source-owned cache also handles synchronous nested reads during a mutation
scope. Its index can be stale there, so the getter scans the current bounded
map in ordinary lookup order. It preserves generic-call accounting and checks
the requested Chunk and context. A miss follows the original coordinator.
Raw descriptor export permanently disables this optimization. Caller-owned
caches retain full validation; the index rebuilds only after the outer scope ends.

Provider presence checks now also use the owned index while no mutation is in
progress. This operation compares packed keys only. A present key with a NULL
or malformed Chunk value still counts as present, and the actual `World.v`
receiver can be a different live provider. Presence adds no GET counters,
validation of pending state, loading, callbacks or owner holds. Mutation scopes,
raw exports, changed descriptors and caller-owned storage keep the existing live
linear membership check.

## Controlled Windows measurement

The fixed-work CPU benchmark uses authored saved Chunks, an actual constructed
source World, and unchanged settings and compiler options. It performs no World
ticks or rendering. Each candidate runs in three fresh processes in alternating
order, with 21 samples of 65,536 calls per operation. Timings include the scalar
query, result consumption and checksum calculation.

On Windows 10 x64, MSVC 19.51.36260 Release `/O2 /fp:strict`,
Intel Core i9-10900F (10 cores, 20 logical processors):

| Operation | Indexed, 1 Chunk before (ns/call) | Indexed after | Nested, 289 Chunks before | Nested after |
| --- | ---: | ---: | ---: | ---: |
| Block | 203.84 | 29.17 | 1549.16 | 81.55 |
| Metadata | 209.20 | 30.21 | 1529.32 | 82.41 |
| Saved skylight | 290.19 | 83.14 | 1660.35 | 196.47 |
| Nullable-error Chunk getter | 17.04 | 17.45 | 1298.63 | 70.75 |

These are medians for the declared CPU workloads. The baseline already includes
the earlier owned-hit diagnostic-frame split and ChunkCache constant baseline.
The after candidate adds failure-only scalar queries and nested live-map reads.
The direct indexed getter is an unchanged-work control. This comparison measures
neither gameplay FPS nor the time to generate, mesh or render a World.
The [recorded samples](../../tests/benchmarks/world_queries_windows.json) include
all three candidates. Their libraries share the final mining interfaces and
all other compiled members; only the five query objects differ.

Every timed sample checks its result checksum afterwards. The benchmark also
checks World/Chunk fields, target block/metadata/light/height arrays, Random
state, ownership holds, cache layout and exact lookup counters outside timing.
Saved inputs remain unchanged apart from the actual constructor's session lock.
Native callback regressions separately exercise publication, same-key replacement,
invalid requested data, reentry, raw export and the final outer-scope rebuild.

## Provider presence measurement

A separate comparison starts from commit `8a213bc6`. Both candidates use its
same compiled simulation and dependency closure, with only
`chunk_provider.c.obj` replaced. Their CPU observer object is identical. Three
fresh processes per candidate retain 21 samples per operation, each containing
65,536 calls. Presence hits traverse a fixed selection of loaded coordinates;
misses use coordinates absent from the authored save.

| Operation and loaded Chunks | Baseline (ns/call) | Indexed presence |
| --- | ---: | ---: |
| Presence hit, 1 | 9.30 | 10.56 |
| Presence hit, 289 | 77.74 | 15.03 |
| Presence miss, 289 | 138.83 | 16.80 |
| Saved skylight, 289 | 155.82 | 87.22 |
| Presence hit, 410 | 102.38 | 14.74 |
| Presence miss, 410 | 188.31 | 15.22 |
| Saved skylight, 410 | 208.90 | 90.77 |
| Presence hit, 512 | 124.02 | 15.16 |
| Presence miss, 512 | 234.26 | 15.90 |
| Saved skylight, 512 | 212.00 | 87.42 |
| Presence hit, mutation scope, 512 | 124.22 | 126.37 |
| Saved skylight, mutation scope, 512 | 324.51 | 324.65 |

The small single-Chunk guard cost and unchanged linear mutation path are retained
controls. The [record](../../tests/benchmarks/chunk_presence_windows.json)
contains all 1,260 CPU samples, code fingerprints and continuity receipts.
Semantic controls pass before and after the optimization, including actual
callback publication/replacement, packed-key aliases, invalid descriptors,
NULL values, foreign providers and permanent raw-export fallback.

The same record preserves a failed three-round native frame cohort. Two
intervals lose foreground focus and enter the source Pause screen; two intervals
also overlap edits to selected test files. Every attempt remains recorded,
including individually valid intervals. The cohort supplies no FPS improvement
claim. The new CPU comparison is separate from the earlier qualified frame pair
below.

## Reproduce

Build with tests and the optional benchmark enabled. Use Release builds with
identical compiler options for comparisons; do not run builds or other benchmarks
at the same time.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DMC_BUILD_BENCHMARKS=ON
cmake --build build --target mc_benchmark_world_queries
python -B tests/tools/world/benchmark_queries.py \
  --executable current=build/mc_benchmark_world_queries \
  --scope indexed --chunks 1 --rounds 3 --artifacts .local/query-benchmarks
python -B tests/tools/world/benchmark_queries.py \
  --executable current=build/mc_benchmark_world_queries \
  --scope mutation --chunks 289 --rounds 3 --artifacts .local/query-benchmarks
python -B tests/tools/world/benchmark_queries.py \
  --executable current=build/mc_benchmark_world_queries \
  --scope indexed --chunks 512 --rounds 3 --artifacts .local/query-benchmarks
```

On Windows, use the `.exe` target path. Repeat `--executable label=path` to
compare candidates. The runner retains raw samples, executable/source/input
hashes, checksums and per-operation distributions. It creates its own resources
and saves; Java, original archives, imported assets and a display are unnecessary.
The current observer includes presence hits and misses alongside its four
earlier operations, and accepts 1, 289, 410 or 512 loaded Chunks.
Timing thresholds are deliberately excluded from CTest.

## Matched Windows frame intervals

Both C client candidates also run the same authored seed-123 save, Player
position/Inventory, 854x480 window, Tiny distance, performance mode 1 and zero
swap interval. The camera starts at yaw 180 and pitch -30, looking away from
the Table. The original resource set and existing OpenAL null-device fixture
are common. Hardware is the i9 above with an NVIDIA RTX 3060, OpenGL 3.3 core,
driver 581.42. Each fresh process warms up for two seconds, then runs ten seconds
of ordinary source frames with no capture, disk checks or assertion callbacks
inside the timed loop.

One complete valid matched pair gives:

| Metric | Baseline | Current |
| --- | ---: | ---: |
| Frames / measured seconds | 54 / 10.052 | 200 / 10.027 |
| FPS | 5.37 | 19.95 |
| Mean host frame time | 186.14 ms | 50.13 ms |
| p95 host frame time | 625.66 ms | 109.69 ms |
| Maximum host frame time | 1064.96 ms | 301.48 ms |
| World-tick returns | 160 | 191 |

The [frame record](../../tests/benchmarks/client_frames_windows.json) retains
raw rows, validity, camera/settings and candidate fingerprints. A subsequent
three-round cohort fails because two baseline intervals lose foreground validity;
all six attempts remain recorded. It supplies no aggregate speedup claim.
A separate run through the public toolkit passes at 20.88 FPS. These finite
intervals retain the baseline qualifications above, natural construction entropy,
ongoing Chunk work and ordinary source timing. They do not establish fully warmed
performance, original-game pixels, Far distance or unrestricted play.

The Windows-only toolkit reuses an existing Release Ninja/MSVC SDK build that
contains `mc_test_workbench_open_shutdown`. Enable application constructor/startup
and display tests as described in [build setup](../building.md), and use an x64
Visual Studio developer shell. Supply locally imported resources:

```powershell
python -B scripts/benchmarks/client_frames.py --build build-windows --out .local/frame-builds --assets assets-local --artifacts .local/frame-runs --rounds 1
```

The builder compiles only the observer and freezes its actual configured support
objects/libraries into a separate output directory. Repeat `--executable label=path`
to compare already-built observers. All attempts remain in artifacts; a failed
focus, camera, source-screen, cache-ownership or cleanup check fails the cohort.

## Earlier frame observations

A fresh focused Tiny-distance Release scene before these changes returned
43 frames in 10.246 seconds (4.20 FPS), with mean host frame time 238.27 ms,
p95 445.01 ms and maximum 1783.57 ms. Its cache remained owned and unexported.
This exposes severe CPU-bound lag in that finite scene, not a general baseline
for arbitrary Worlds or Far distance.

A later candidate run entered Pause and Workbench screens during 88 of 131
measured frames. Another entered a Workbench during its final ten frames while
the same native process retained foreground focus. These rejected intervals
cannot establish gameplay FPS improvement or a common cause for those transitions.
Focused gameplay, default-Far settings, generation stalls, GPU time, extended
sessions and Linux frame times still require further qualification beyond the
matched finite intervals above.
