# World query performance

The later [provider-scope result](provider-cache-scopes.md) measures resident
reads during callbacks with live positive hints and correct descriptor-owner
invalidation. Earlier linear-scope measurements below retain their historical
source/product scope.

## EW render getters

[Chunk rendering and EW diagnostics](chunk-rendering.md) records the measured
minimum-brightness and opaque-query hot paths, six shared failure-only getters,
336 retained CPU samples and exact native controls. Median minimum-brightness
cost is 38.332% lower and opaque cost is 64.516% lower in the fixed workload.
The separate six-process frame cohort fails foreground/World-progress admission;
it supplies no FPS improvement claim. Earlier receipts retain their captured scope.

## EW metadata diagnostics

Public EW metadata access keeps its complete diagnostic contract. A private
failure-only accessor shares the same Source read and admission, leaves caller
diagnostics untouched on success, and supplies the full canonical failure
record. Only CV's EW metadata branch changes; direct World access, Block reads,
outputs, active state and leases retain their behavior.

The [Windows CPU receipt](../../tests/benchmarks/chunk_cache_metadata_windows.json)
retains the existing 950 original-reference rows, 3,541 native checks, 26 CPU
control rows and all 56 samples from eight fresh processes. Median elapsed cost
per 65,536 actual reads falls from 9.06405 to 1.08805 ms, or 138.3064 to
16.6023 ns/read: an 87.995984% reduction using unrounded observations. The same
compiled caller uses a genuine nine-Chunk EW and checks values, errors, ownership,
Random and zero acquisitions outside timing. The 7,352-byte public record's
nominal initialization-plus-copy size is not measured DRAM traffic.

The separate [EW frame cohort](../../tests/benchmarks/chunk_cache_metadata_frames_windows.json)
is FAIL. All eight attempts remain; five pass individual admission and three
lose native foreground validity and end paused. Their World-time/random/display
counts are 0, 60 and 129 despite 195, 192 and 194 World coordinator returns.
The first paused baseline's 39.18 raw FPS accompanies zero World-time steps and
supplies no comparison credit. Source/product/asset guards pass; the foreground
cause remains unestablished. No aggregate, passing-subset FPS gain or retry is
claimed. This cohort stays separate from the earlier failed work-ledger cohort.

The [metadata benchmark guide](../../tests/benchmarks/chunk_cache_metadata.md)
records the public targets, driver and contract limits. Runtime bodies match
the measured candidate. Four current Windows Workbench SDK profiles separately
pass 7,446 checks in 29.83 CTest seconds, preserving 1,979 selected inputs and
413 assets. Every join takes one call with no BUSY return. The fixture follows
the documented bounded BUSY-only native completion protocol; its controlled
refusals, windows and stopped-disk assertions remain intact. The earlier
startup-resource BUSY failure stays retained, with its exact child unresolved;
a natural BUSY-to-OK transition is still unobserved.

Current public suites finish at 684/685 Windows CTests in 247.42 seconds and
674/674 Linux Clang 18 Debug ASan/UBSan CTests in 147.72 seconds under
Xvfb/software GL. Windows fails only the Pig focus check in
`client.world_selection_animals`, with Source focus 0/paused 1/display-active 0;
this is separate from the historical frustum failure and establishes no cause.
Each cohort preserves 4,767 source inputs, 638 products and 413 assets without
drift or observer errors. Both public metadata CPU executables pass 6,342 fresh
control checks each on both hosts with no timing samples. ChunkCache/Fluid and
the 43 frame, 18 static-CPU and 71 native-packet audit methods also pass on both.
The [guide](../../tests/benchmarks/chunk_cache_metadata.md#current-public-validation)
records the full validation scope. There is no full Windows pass or new FPS,
Linux timing or original-reference claim.

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

### Active Chunk indexing and registry reads

Random ticks build a 2 KiB ordinal index while gathering active Chunks. It
avoids scanning the coordinate ledger for each insert and traversal step.
The Set still determines traversal order, and the scheduler still checks the
iterator version before reading the ledger. The lookup returns to the linear
scan when the probe limit is reached or the index would exceed half capacity.
Registry reads from the same loop skip the initialization call when the Block
registry is already ready and idle.

Eight alternating pairs per workload on an i9-10900F with MSVC Release
measured 112 samples per build and workload. Each timed tick visited 361
Chunks and completed all 28,880 samples. Before and after passed the same
15 World controls and matched all 75 binary observations and the full native
transcript. A one-probe build exercised the fallback and matched the controls.
Four focused Windows tests and six Linux Clang tests passed. The Linux run
enabled ASan, UBSan and leak detection.

| Workload | Median before / after (ms) | p95 before / after (ms) |
| --- | ---: | ---: |
| Air, ambient cooldown active | 3.545 / 3.415 | 4.173 / 3.745 |
| Stone, ambient checks active | 3.581 / 3.404 | 4.498 / 4.008 |

Phase instrumentation measured gather at 58.15 µs before and 39.85 µs after
indexing. The coordinate lookup total fell from 40.65 to 10.00 µs. This probe
adds timers around each lookup. The full-tick table above uses ordinary builds.
The index-only cohort had mixed medians, and the same-executable control varied
by 2–4%, so the aggregate timing applies to the combined change.

Four matched Windows pairs in the fixed-input Tiny scene passed on `DISPLAY2`.
Median FPS rose from 42.98 to 44.06, and median run p95 frame time rose from
40.06 to 41.80 ms. Two of four pairs had higher FPS. A repeat cohort kept all
eight camera poses fixed. Two baseline runs lost foreground and paused
simulation, which invalidated that comparison. The frame results are mixed.

### Raw Block reads

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

### Random tick coordinates and packed Chunk keys

Each random sample advances the live World coordinate generator using 32-bit
wraparound and reads the coordinate bits directly. The scheduler checks the
four live provider bindings before GET. World Block and metadata reads use
floor division to find a Chunk at negative coordinates. The packed cache key
keeps the source sign bits and coordinate masks. Ore and clay Block reads
build their Registry diagnostic on failure.

Eight alternating MSVC Release pairs per workload measured 112 ticks for each
build. Each tick visited 361 Chunks and sampled 80 positions per Chunk. The
workloads used air with ambient cooldown active and stone with ambient checks
active. They ran no enabled Block updates. The 15 separate native random-tick
controls matched exactly, including cases with enabled Block updates.

| Fixed workload | Median before / after (ms) | p95 before / after (ms) |
| --- | ---: | ---: |
| Air | 5.235 / 4.782 | 6.763 / 5.890 |
| Stone | 4.861 / 4.418 | 6.961 / 5.822 |

The 289-Chunk mutation-scope query runner matched the state and checksums.
Across eight pairs pinned to one logical CPU, median time per Block read fell
from 47.68 to 44.59 ns and owned GET fell from 36.22 to 33.45 ns. The metadata
median rose from 61.26 to 92.61 ns in the first cohort and fell from 61.74 to
54.58 ns in the pinned repeat. Metadata p95 rose from 125.97 to 130.26 ns and
from 108.11 to 123.84 ns. The fixed seed-23 legacy population fixture matched the
recorded state across eight pairs. Four ore placements took a median of
57.45 ms before and 57.27 ms after on the client.

Separate instrumented baseline runs on `DISPLAY2` found a 121.12 ms random tick
with 98.49 ms inside two nested population calls. An instrumented Block dispatch
run identified Grass as the caller in a 102.63 ms update, with 94.58 ms in two
nested population calls and 61.41 ms in one of their springs. These timings
are inclusive. The change keeps the Grass update and population synchronous.

Two instrumented candidate runs captured 41 accepted springs. The longest
immediate fluid tick took 50.80 ms. Sampled timings put 37,441 material reads
at 19.23 ms, 378,360 named Block reads at 14.00 ms, and 76,229 Block ID reads
at 10.39 ms. The samples add overhead, and nested timings overlap.

All six pairs qualified across two uninstrumented `DISPLAY2` groups at Tiny
distance and 854×480. The camera and input matched. Both builds included the
same local Sign screen edit.

| Scene group | Median FPS before / after | Median p95 frame ms | Median p99 frame ms |
| --- | ---: | ---: | ---: |
| First | 30.17 / 24.79 | 69.05 / 83.33 | 113.41 / 149.46 |
| Repeat | 26.33 / 34.01 | 79.95 / 67.76 | 115.80 / 92.71 |

Four of six pairs had higher FPS and three had lower p99. Median results
moved in opposite directions across groups. The longest candidate frame took
187.71 ms, so long frame stalls remain.

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

### Saved light and Store leases

World queries and light propagation resolve the concrete Chunk and acquire its
Store lease with one identity lookup. The Store still distinguishes missing
objects, source admission failures and lease limits. Saved-light presence and
GET still run separately, using the same converted Chunk coordinates. Light
reads use the checked local cell index for both packed light planes.

Two instrumented DISPLAY2 captures measured median drains of 13.71 and
10.78 ms. The longest drains took 74.03 and 76.95 ms, inside 116.35 and
136.23 ms frames. Those drains made 214,409 and 234,712 saved-light calls.
Sampling one call in 32 put their inclusive cost at 56.86 and 61.27 ms.
The samples include instrumentation overhead.

In eight alternating Windows MSVC Release rounds with three builds, the
fixed drain median was 9.30 ms at baseline, 8.99 ms with the fused Store
lookup, and 8.28 ms with both changes. The baseline and combined build p95
were 12.61 and 13.20 ms. All 168 samples matched the recorded World state,
queue, ownership, Random, failure diagnostics and 37,924 owned GETs per
sample. The benchmark processes 499 queued regions and 3,992 cells using
nine authored Chunks.

Median saved-light query time fell from 269.55 to 243.63 ns with one indexed
Chunk and from 286.35 to 253.19 ns with 289. Each comparison ran six fresh
process pairs at 65,536 calls per sample. All operations matched their
checksums and state checks. Block, metadata, GET and presence medians varied
in both directions.

Four further instrumented DISPLAY2 runs qualified. Across drains with more
than 10,000 saved-light reads, median estimated cost per read moved from
273 and 276 ns to 241 and 236 ns in the two pairs. Three drain workloads
matched all nine recorded call counts. Their drain times moved from 25.47
to 26.14 ms, 37.03 to 30.52 ms, and 28.20 to 25.91 ms.

All six uninstrumented DISPLAY2 runs qualified with the same local Sign
screen edit in both builds. In the authored 854×480 Tiny scene, median FPS
fell from 19.86 to 19.02 and median p99 rose from 145.01 to 163.98 ms. Two
pairs moved in that direction and one moved in the other direction.

### Scalar Block acquisition diagnostics

Ordinary Block and metadata reads ask the Chunk coordinator for the storage
cause on failure. Detailed callers still receive the full acquisition record,
and the coordinator keeps its full scoped frame when it enters callbacks. A
retained admission failure constructs its full record in a separate function.

MSVC Release reduced the stack reservation in the acquisition dispatch from
5,184 to 256 bytes and in the ordinary scalar wrapper from 4,984 to 88 bytes.
The generated code for both functions dropped its stack probe. The native
owned and exported cache tests check that a hit leaves the caller's cause
alone and that invalid output and hold limit failures match the detailed GET.

Pinned Windows Release comparisons used 289 resident Chunks and eight
alternating pairs per scope. Each build supplied 56 samples of 65,536 calls
per operation. Recorded checksums and World state matched.

| Scope and query | Median before / after (ns) | p95 before / after (ns) |
| --- | ---: | ---: |
| Indexed Block read | 49.01 / 39.56 | 87.28 / 65.52 |
| Indexed metadata read | 56.41 / 48.06 | 95.20 / 90.65 |
| Indexed owned GET | 35.05 / 27.34 | 63.54 / 50.54 |
| Mutation-scope Block read | 50.79 / 39.18 | 95.88 / 70.54 |
| Mutation-scope metadata read | 90.19 / 50.00 | 132.07 / 97.67 |
| Mutation-scope owned GET | 38.41 / 29.72 | 71.01 / 56.56 |

The fixed accepted spring with the source-owned cache retained its state in
two additional eight-pair groups. Median time moved from 61.84 to 59.53 ms
and from 64.01 to 52.44 ms, with four and six faster candidate pairs.

All 12 uninstrumented DISPLAY2 runs qualified at Tiny distance and 854×480.
Both builds shared the local Sign screen edit. Five of six pairs had higher
FPS and four had lower p99. The longest candidate frame took 107.77 ms.

| Scene group | Median FPS before / after | Median p95 frame time (ms) | Median p99 frame time (ms) |
| --- | ---: | ---: | ---: |
| First | 40.19 / 47.40 | 48.55 / 40.37 | 77.49 / 67.16 |
| Repeat | 42.11 / 44.82 | 48.60 / 40.42 | 71.59 / 74.09 |

### Acquisition diagnostics in saved-light queries

World queries keep only the acquisition fields that can reach their error
record. The coordinator still constructs the full diagnostic when callbacks
run. A refused GET supplies the canonical unreached fields. Retained queries
still resolve their selected loader before copying the reported fields.

In MSVC Release, the stack reservation for the query acquisition wrapper
dropped from 5,112 to 968 bytes and lost its stack probe. Native owned and
exported cache tests confirm that a hit leaves the caller's failure record
untouched and that invalid output returns the same unreached diagnostic.

Two sampled DISPLAY2 runs after the Material change showed that saved-light
reads were the largest cost in the slowest drains. The 62.75 ms drain made
225,646 saved-light reads, with an estimated 48.39 ms in those calls.
The 42.16 ms drain made 206,743 reads, with an estimated 33.90 ms in them.
Sampling adds overhead and nested durations overlap.

In the first pinned fixed-work group, the 499-region drain fell from 4.50 to
3.97 ms at the median and from 7.98 to 7.26 ms at p95. Eight alternating
pairs supplied 56 samples per build. All workloads and failure receipts
matched, including the saved-light failure after seven earlier reads. In
the repeat, median time was 3.25 to 3.27 ms and p95 was 3.78 to 6.36 ms.

The 289-Chunk scalar query comparisons used eight pairs per scope. Median
saved-light time fell from 147.84 to 144.63 ns with an idle index and from
148.37 to 133.28 ns during a mutation scope. Its p95 rose from 227.54 to
230.45 ns and from 228.39 to 233.06 ns, respectively. In the repeat, median
saved-light time fell from 98.31 to 93.13 ns with an idle index and from
99.80 to 91.40 ns in mutation scope. Repeat p95 values moved from 180.79 to
201.51 ns and from 164.53 to 145.08 ns. Indexed owned GET medians moved from
29.53 to 37.08 ns in the first group and from 23.99 to 24.39 ns in the repeat.

Four further instrumented DISPLAY2 runs matched two drains by all nine
operation counts. The larger match made 86,476 saved-light reads and
took 21.61 ms before and 18.74 ms after. The smaller match made 14,215
saved-light reads and took 2.48 ms before and 2.66 ms after.

All 18 uninstrumented DISPLAY2 runs qualified at Tiny distance and 854×480.
Both builds in each group shared the local Sign screen edit. The isolated
comparison used the Material change in both builds. The combined groups
used the baseline before either change. The combined result varied across
groups, and the slowest combined candidate frame took 138.50 ms.

| Scene group | Median FPS before / after | Median p95 frame time (ms) | Median p99 frame time (ms) |
| --- | ---: | ---: | ---: |
| Saved-light only | 32.53 / 30.55 | 64.73 / 71.63 | 104.87 / 110.62 |
| Combined | 37.88 / 31.59 | 49.97 / 63.21 | 90.76 / 99.54 |
| Combined repeat | 43.36 / 45.22 | 41.11 / 42.92 | 70.69 / 67.25 |

### Live opacity and emission reads

Lighting propagation and Chunk setter opacity checks use an immediate
array element read when the Registry has finished initialization, is idle
and the live index is valid. The same capture and element operations
handle null arrays, invalid indices and active initialization. Lighting
reads the live array after the World Block lookup and repeats the emission
read when reached during a point recheck.

The native propagation test submits a one-cell Block-light region with
each array set to null in turn. It checks that the drain stops at the
reached element read, reports the null reference and consumes the failed
region. The fixed 499-region Windows drain matched all state and failure
receipts in eight alternating pairs, with 56 samples per build. Each
sample visited 3,992 cells. In the isolated comparison, median time fell
from 2.57 to 2.46 ms and p95 rose from 2.64 to 2.78 ms.

In the preceding DISPLAY2 phase traces, the slowest drains took 45.34
and 43.13 ms. Sampled opacity reads accounted for approximately 2.22
and 2.00 ms. Saved-light reads were still the largest measured cost in
both drains.

All 12 uninstrumented DISPLAY2 runs qualified at 854×480 on Tiny.
Both builds shared the same local Sign screen edit. Median FPS rose
in both groups and median p99 frame time rose as well. World work
varied across runs.

| Scene group | Median FPS before / after | Median p95 frame time (ms) | Median p99 frame time (ms) |
| --- | ---: | ---: | ---: |
| First | 41.69 / 43.86 | 45.35 / 45.09 | 67.99 / 70.52 |
| Repeat | 45.46 / 46.91 | 39.59 / 35.67 | 63.23 / 64.79 |

### Saved-light reads after acquisition

The saved-light query checks the Store owner and lease limit, then reads
the Chunk during the callback-free part of the call. It rounds in-range
coordinates down to their Chunk and reads the ready light statics directly
from the current class owner. A cold owner still initializes both statics
before the first identity comparison.

Each Windows Release group alternated eight baseline and candidate pairs.
The drain fixture used nine authored Chunks and consumed 499 queued regions.
All acquisition counts, light planes, queue contents and failure receipts
matched, including the callback that removed the reached Nibble backing.

| Workload | Median before / after | p95 before / after |
| --- | ---: | ---: |
| 3,992-cell drain | 2.54 / 2.23 ms | 2.90 / 2.47 ms |
| 3,992-cell repeat | 2.52 / 2.22 ms | 2.71 / 2.37 ms |
| 31,936-cell drain | 18.36 / 16.38 ms | 22.18 / 17.66 ms |
| Saved-light read, 289 indexed Chunks | 74.44 / 63.84 ns | 76.43 / 67.91 ns |
| Saved-light read, 289 Chunks in a mutation scope | 75.09 / 64.80 ns | 81.68 / 76.70 ns |

The longest sample in the larger drain went from 23.43 to 24.54 ms.
These timings cover the drain and scalar query calls on authored inputs.

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

### Work-ledger admission and failed cohort

The [eight-profile receipt](../../tests/benchmarks/client_frame_work_qualification_windows.json)
retains four alternating rounds in the natural seed-123 Tiny sky-view scene.
Seven intervals pass individual admission; round 1 current fails focus and
native-foreground checks and ends paused. Its 191 enclosing World returns
include 189 steps of live World time and 189 calls each to random-Chunk
and random-display work. The full cohort remains FAIL, with every attempt and
the source/product/asset guards retained. It supplies no aggregate FPS gain or
passing-subset comparison.

Admission requires at least ten seconds, complete focused ordinary frames,
finite identical endpoint x/y/z/yaw/pitch, the same starting save/assets and
complete matching graphics identity. Boundary counters must agree with the
raw frames and prove unpaused World-time and random/display advancement.
Individual zero-tick frames stay in the interval; natural work totals need not
match between processes. Endpoint pose equality does not prove every
intermediate position, and recorded graphics identity is not all GL state.
The [observer contract](../../tests/benchmarks/client_frames.md#eight-profile-work-ledger-cohort)
contains every raw FPS observation and the pure receipt audit command.

### Legacy frame-only captures

The following captures retain their earlier frame, focus and camera checks.
They lack the independent work checkpoints required by current admission and
are not combined with the eight-profile cohort.

Both C client candidates also run the same authored seed-123 save, Player
position/Inventory, 854x480 window, Tiny distance, performance mode 1 and zero
swap interval. The camera starts at yaw 180 and pitch -30, looking away from
the Table. The original resource set and existing OpenAL null-device fixture
are common. Hardware is the i9 above with an NVIDIA RTX 3060, OpenGL 3.3 core,
driver 581.42. Each fresh process warms up for two seconds, then runs ten seconds
of ordinary source frames with no capture, disk checks or assertion callbacks
inside the timed loop.

The earlier frame-only protocol accepted this pair:

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
A separate legacy toolkit smoke records 20.88 FPS. These finite
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
