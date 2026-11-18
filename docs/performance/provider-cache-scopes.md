# Provider cache reads during callbacks

The source-owned provider keeps a map index for ordinary resident Chunk reads.
Chunk acquisition and lifecycle callbacks can publish, replace or remove map
entries before the outer operation returns. Those nested reads need the live
map, including its current Chunk identities.

During an owned mutation scope, the index now supplies positive hints only.
Each lookup probes at most sixteen existing table words. A hint is usable only
when its ordinal is within the current map and the live entry has the requested
packed key. The selected live Chunk must pass the same payload check as the
linear path. The hint stores an ordinal, never a retained Chunk pointer.

An empty slot, shifted ordinal, different key or exhausted probe proves nothing
about absence. The lookup then takes the existing live linear path. A valid
linear hit remembers its current ordinal in one home slot. This may evict
another hint; that key can still use the linear path. Collisions affect cost,
not membership or identity. Newly published entries can therefore become hints
without allocating or scanning for a free table slot.

This relies on the owned provider's existing writer contract: completed writes
preserve unique packed map keys, and no callback observes the middle of an
entry publication or compaction. A raw mutable export permanently destroys the
optional index. Caller-owned caches continue through the generic coordinator.

The outermost scope still fully validates the cache and rebuilds the canonical
index before it can prove absence. Acquisition still executes its coordinator,
pending edits, publication, activation and population. The optimization changes
only already-resident GET lookup inside an owned scope. It adds no allocation,
public layout field, Source callback or additional timing observer.

The existing scoped generic-get counter continues to count scoped dispatches,
including positive-hint hits. It no longer implies that every such dispatch
performed a linear scan. Ordinary depth-zero hit accounting is unchanged.

## Descriptor ownership after World.v changes

A GAME World's acquisition, lifecycle and saving descriptors retain their
constructed cache binding when a callback replaces its Source `World.v` field.
Scope entry now captures that descriptor's actual owner and closes the same
owner after callbacks. Reading `World.v` again at scope exit could leave the
original owner open or close an unrelated provider.

Public descriptor exports, successful raw Chunk exports and save progress
callbacks invalidate the actual escaping cache's owner. They also retain the
existing conservative invalidation of the current `World.v` receiver when it
differs. Returning from a callback cannot restore either index after export.

Owner discovery applies only to GAME construction and verifies the exact
provider kind, World and cache address. Composition accepts arbitrary callback
owners and never interprets them as concrete providers. This ownership rule
does not replace Source `World.v` dispatch, presence checks, generation or
population receivers.

## Measurement scope

The preceding [frame baseline](client-frame-baseline.md) remains a diagnostic
observation. Fixed-work resident lookup measurements and native frame intervals
answer different questions: the former fixes calls and coordinates; the latter
uses real deadlines and can execute different frame and simulation counts.
Neither establishes equality with the original game's performance.

## Fixed-work Windows result

One strict MSVC x64 Release workload object links against the actual frozen
baseline and candidate libraries. All thirty dependency roles are checked;
only `mc_simulation.lib` differs. The baseline is `ff81d0ea`. Existing public
layouts and counter meanings remain compatible with the shared object.

Each fresh process loads an authored source World with sparse resident Chunk
coordinates, qualifies every requested identity and packed-key alias, then
enters an explicit scope of depth one or two. Each timed sample performs
65,536 real `get_failure` calls, uniformly selecting across every resident key.
Timing includes coordinate and alias selection, status aggregation, and both
coordinate and pointer-identity checksum folds. QPC measures elapsed time;
these are neither API-only latency nor process CPU measurements.

The balanced order is baseline/candidate/candidate/baseline, followed by
candidate/baseline/baseline/candidate, separately for each scene. Forty-eight
fresh processes retain 336 samples. Each process contributes seven samples;
the table reports the median of four process medians per version. Twelve
earlier qualification processes are retained separately and excluded from
these medians.

| Resident Chunks | Scope depth | Baseline ns/call with loop | Candidate ns/call with loop |
| --- | ---: | ---: | ---: |
| 8 | 1 | 20.89 | 22.50 |
| 8 | 2 | 23.86 | 22.28 |
| 64 | 1 | 34.66 | 22.94 |
| 64 | 2 | 34.06 | 22.98 |
| 256 | 1 | 67.98 | 25.63 |
| 256 | 2 | 67.07 | 27.21 |

Larger resident maps show lower elapsed time. The eight-Chunk depth-one case
costs about 1.61 ns more per call; the optimization does not improve every
scene. This cohort does not establish statistical significance.

All sixty processes pass exact output, Chunk identity, complete plane and
state checks. Outside-clock checks preserve World and Random state, cache and
pending storage, diagnostic sentinels, ownership holds, scope depth and checked
cleanup. The authored save inputs remain unchanged apart from the real session
lock. Source, object, executable and dependency guards remain unchanged during
each cohort.

The [finite record](../../tests/benchmarks/provider_cache_scope_windows.json)
contains every balanced sample and source/product fingerprints. At the
`986ca4f2` cache checkpoint, clarifying the measured source's initial comment
left every subsequent byte unchanged; the record preserves both hashes and
that identity proof. The later [quiet-setter harness](quiet-setter-acquisition.md)
adds a default entry binding to reuse the same provider fixture. That later
source change does not alter the earlier measurement bindings. The host is an Intel
Core i9-10900F running native Windows, using MSVC 14.51.36231 with `/O2 /W4 /WX`
and strict floating-point settings.

These scenes measure warmed, already-resident reads. They perform no map churn,
load, generation, actual callback body, rebuild or rendering inside the clock.
Separate regression controls cover those map and callback transitions. This
measurement establishes no gameplay FPS or original-game performance gain.

## Four native frame intervals

A separate baseline/candidate/candidate/baseline cohort uses one existing
compiled frame observer, 92 actual baseline dependency roles and thirty
controlled candidate roles. Sixty-two common roles stay identical, and only
the simulation library differs among the controlled roles. No archive member
is substituted. The observer adds passive cache snapshots after each existing
host-end QPC read. Its between-frame overhead remains part of the deadlines;
no estimated overhead is subtracted.

Every interval uses the same authored Tiny sky view at 854×480, OpenGL 3.3,
swap interval zero and performance mode one. The GPU is an NVIDIA GeForce
RTX 3060 with driver 581.42. Startup receives physical DISPLAY2's work bounds
before window creation. Both real frame and canvas windows pass the monitor
geometry assertion before warmup. All four intervals pass foreground, complete
pose, simulation-work, source/product and checked-cleanup guards. No interval
is retried.

| Order | Version | Frames | World ticks | FPS | Host p95 ms | Maximum host ms |
| ---: | --- | ---: | ---: | ---: | ---: | ---: |
| 1 | Baseline | 365 | 190 | 36.46 | 47.64 | 243.10 |
| 2 | Candidate | 380 | 192 | 37.95 | 51.94 | 169.41 |
| 3 | Candidate | 381 | 192 | 38.05 | 45.05 | 171.64 |
| 4 | Baseline | 373 | 194 | 37.24 | 56.40 | 242.44 |

The two-interval FPS medians are 36.85 and 38.00, a 3.12% difference. Median
maximum host-frame time changes from 242.77 to 170.52 ms. Both candidate FPS
observations exceed both baseline observations, but p95 does not improve in
every interval. Two observations per version establish no statistical
significance or unrestricted smooth-play result.

These deadline-driven intervals perform different work. Baseline miss counts
are 21 and 22; candidate counts are 29 and 21. Scoped GET dispatch totals range
from 1.179 to 1.559 million. The [finite frame record](../../tests/benchmarks/provider_cache_scope_frames_windows.json)
preserves every interval's work ledger, clocks, monitor receipts, cache totals,
graphics context and product fingerprints. The comparison supports a native
performance opportunity; it does not isolate the hint's contribution from
changes in work or execution scheduling.

All 28 frames exceeding 100 ms still coincide with indexed misses. The probe
does not separate disk loading, generation, population, activation, lighting or
validation time. Residual acquisition stalls and the steady rendering prefix
remain separate performance targets. This is no pixel, audio, GPU-duration or
original-game FPS comparison.

## Native ownership and mutation regressions

The source-owned cache driver covers sixteen modes. New controls exercise a
nullable Source provider, a second live provider, progress-callback rebinding,
nested live-map changes and a nonselected invalid payload left by a real
entity callback. The map controls retain packed aliases, replacement identity,
append, ordinal shifts, equal-count reinsertion, failed-publication byte
atomicity, nested depth and final canonical rebuild. Corruption remains live
through the outer validation failure, committed publication and diagnostic
checks; restoring it afterwards does not replay callbacks or recreate an index.

One compiled final test object links against both authentic old and candidate
libraries. All thirteen retained positive modes produce byte-identical old/new
stdout. The three old owner modes reach their expected semantic guards and
fail; all sixteen candidate modes pass. The old failures are ordinary checked
assertions, with complete capture and no timeout or crash. A separate earlier
owner-null proof also retains the unchanged 183-check positive control and
34-check candidate result. This is native ownership evidence, not a fresh
original-game comparison of these replaceable-field graphs.

The complete Windows MSVC Release build and 1,268-test suite pass, including
every cache mode and both normal/optimized reader controls. Twelve selected
Linux tests also pass under ASan, UBSan and leak detection. Strict GCC13
compilation covers the two changed runtime units, native fixture and benchmark.
The [test record](../testing.md#provider-cache-scopes-and-owner-rebinding)
preserves their exact source and result scope.

## Chunk presence during population

The Windows DISPLAY2 trace at `c4153f33` found random ticks calling into
neighboring Chunk generation and population. In one 230 ms frame, two
population calls took 201 ms in total and generation took 7 ms. The sampled
Chunk itself was already resident. Two finer captures recorded 62 population
calls. Median ore placement times were 20.9 and 21.6 ms per call in the two
processes. Spring work usually took about 1 ms, with a maximum of 96 ms in one
capture. The four longest spring spans in each process contained no nested
generation or population calls. These timings attribute the stalls in this
fixed scene.

Population mutations check Chunk presence while publishing and updating chunks.
For an owned cache with more than eight entries, these checks can now accept
the same bounded ordinal hint as a scoped GET. The current map key must match
before the hint proves presence. An absent or stale hint falls back to the
live scan, as do smaller scoped maps. Presence reads key membership and leaves
the table, payloads and World state alone. The `scope-presence` control covers
packed aliases, a null value behind a present key, compaction, an appended
key and an absent key at two levels of nesting.

A private MSVC Release comparison linked one workload object and a shared
set of dependency archives. The baseline provider came from `c4153f33`. The
candidate replaced just that object. Four alternating pairs per cache size
and depth produced 48 fresh processes. Each produced seven samples of 65,536
checks for resident keys and seven for a 75% hit mix. Each process checked
every answer, packed aliases, diagnostics, cache counters, saved fixture files
and Chunk planes. The four eight-entry medians shifted by −0.5% to +2.3%.
Times below are per sample.

| Resident entries | Scope depth | Resident keys, base → candidate | 75% hit mix, base → candidate |
| --- | --- | ---: | ---: |
| 64 | 1 | 1.639 → 0.718 ms | 1.985 → 1.270 ms |
| 64 | 2 | 1.645 → 0.715 ms | 1.984 → 1.269 ms |
| 256 | 1 | 4.799 → 0.838 ms | 5.824 → 2.884 ms |
| 256 | 2 | 4.730 → 0.861 ms | 5.674 → 3.004 ms |

Six uninstrumented `DISPLAY2` runs passed the fixed-camera, foreground and
World-work checks. The two products shared the observer and dependencies,
which included the local Sign screen edit. The timed scene stayed in gameplay.
Median FPS was 34.39 before and 35.76 after. Median run p95 fell from 72.12
to 61.23 ms, while median run p99 rose from 129.92 to 139.41 ms. One of three
pairs lost FPS and had a higher p95. An earlier instrumented comparison with
the initial candidate lost foreground and could not qualify. The frame
measurements cover this scene and leave the spring spikes open.

Ten focused tests passed on each host, including the 17 owned-cache profiles,
natural population, lighting and random ticks. Linux ran with Clang, ASan,
UBSan and leak detection.

### Ore placement and lighting diagnostics

Two more DISPLAY2 traces timed samples from over 915,000 ore candidate reads
and 107,000 quiet setter calls. The setters took most of the sampled time.
Direct lighting submissions and column updates were both substantial parts of
the setter. Each changed ore cell can query four neighboring heights and submit
multiple lighting regions.

Ordinary height and light submission now request acquisition diagnostics only
when the underlying Chunk lookup fails. Ore and clay use the same quiet ID
setter body with a failure-only diagnostic. The ore and clay feature frame
starts with a canonical diagnostic, and a failed write immediately passes it
back. The public setters continue to return canonical success diagnostics.

A private Windows MSVC Release comparison timed the ore phase in four Chunk
population calls per process. It reused the native ring fixture and seeded the
live World Random after construction so whole output files could be compared.
The fixture exports the provider and exercises the generic cache path. Each
side ran four fresh processes per variant with balanced order. All 32 processes
produced byte-identical native observations within their side, including Chunk
planes and Random continuations. Times are median sums of the four ore phases.

| Runtime changes | Client | Server regression control |
| --- | ---: | ---: |
| Baseline | 112.51 ms | 108.93 ms |
| Lighting acquisition | 96.55 ms | 96.64 ms |
| Quiet setter | 98.79 ms | 96.06 ms |
| Both | 86.53 ms | 91.46 ms |

All six uninstrumented DISPLAY2 runs qualified. The two products again shared
the local Sign screen edit in the common libraries. In the fixed Tiny scene,
median FPS rose from 37.87 to 38.89. Median run p95 fell from 52.72 to 49.98 ms,
and median run p99 fell from 108.68 to 91.84 ms. All three pairs had a lower
p95. One pair lost FPS, one had a higher p99, and the longest candidate frame
still took 161 ms.

The full Windows build and 18 focused CTests passed. A targeted Linux build
passed 22 checks with Clang, ASan and UBSan after the full sanitizer build ran
out of disk space. Both selections cover the shared cache fixture's save-stage
and projection consumers, the light queue, failed mutations and population.

#### Mutation diagnostic initialization

Two later DISPLAY2 traces timed 7,267 of 88,716 quiet setter calls during ore
placement. Initialization accounted for about 29–30% of estimated setter time,
compared with 3–4% for allocation. The replacements still performed light
submissions and neighboring height queries. An underground opaque replacement
normally skips rebuilding its own height map.

The mutation initializer now prepares one canonical diagnostic using the
existing child initializers, then copies it into each caller's workspace.
Concurrent callers get independent records with all nested sentinels. The
setter still allocates its workspace at the same point and performs the same
ordered Chunk calls.

Repeating the seeded four-Chunk workload in Windows MSVC Release took a
median of 126.33 → 103.41 ms on the client and 128.57 → 101.66 ms on the
server regression control. Eight fresh process pairs per side ran in
alternating order. The 32 processes produced byte-identical native reports
within each side, including reaching the same four population calls and
retaining the same exported Chunk data and Random continuations.

At 854×480 on Tiny, three complete DISPLAY2 pairs had median p99 frame time
of 163.73 → 114.85 ms and median FPS of 24.36 → 24.19. The fourth baseline
stopped on an active Entity tick during the interval, so its frame indices
failed validation and the pair was excluded. The two builds shared libraries
containing the local Sign screen edit. The timed scene stayed in gameplay.
All six runs in a separate repeat qualified. Median p99 was 162.54 → 137.28 ms,
and median FPS was 20.37 → 24.15. One repeat pair had a higher p99. Both groups
had a lower median p95, from 92.10 to 83.61 ms and from 112.91 to 86.53 ms,
respectively.

#### Light queue scans during ore placement

Two DISPLAY2 traces estimated 20–21 ms in light queue scans and 12–13 ms
in allocation and append during ore placement. The full submissions took
77–80 ms across each ten-second interval. The estimates come from 9,983
sampled setter calls across 122,143 replacements.

The coalescing scan reuses the validated list while it examines up to five
records in newest-first order. The scan cannot call back into the World.
An accepted expansion updates the chosen region in place and ends the scan.
The merge test computes volume with unsigned 32-bit arithmetic and applies
the signed Java comparison to the wrapped result. Boundary tests cover a
wrapped negative volume increase, wrapping at `INT32_MAX`, and failures
from a null list, invalid list metadata, or a null queued region.

In the seeded four-Chunk Windows MSVC workload, each side ran eight fresh
processes per variant in balanced order. All 64 native reports matched
byte-for-byte within each side. Median client ore placement took 87.41 ms
before, 85.11 ms with the list change, 86.49 ms with the arithmetic change,
and 84.47 ms with both. The median server regression control was 88.05 ms
before and 80.49 ms with both. Five of eight pairs were faster on each side.
A separate fixed sequence of 12 million bounds checks per process took a
median of 248.67 → 104.63 ms across eight pairs and returned the same merge
count and checked fields. A differential check matched every result and all
six output bounds across one million inputs and 125,000 same-pointer calls.

All six native DISPLAY2 runs qualified at 854×480 on Tiny. Median FPS was
38.78 → 40.46, median p95 frame time was 49.07 → 50.16 ms, and median p99
was 75.00 → 74.56 ms. A separate six-run repeat measured 31.00 → 31.66 FPS,
69.52 → 61.82 ms p95, and 103.96 → 85.86 ms p99. The longest candidate
frames took 113 ms and 144 ms, respectively. Both builds shared the local
Sign screen edit and stayed in gameplay during timing.

#### Height and light diagnostics in Chunk mutations

The ordinary Chunk mutation callers now request diagnostics only when a
neighboring height read or a light submission fails. Both calls still check
presence and then use the current provider for the separate Chunk lookup.
The lookup writes its population detail directly to the diagnostic on
failure. Public and retained execution calls still return their full
diagnostics, including the site when the native continuation stops.

Two fixed Windows MSVC workloads used three million calls per process and
eight alternating process pairs. Median time was 805.25 → 726.72 ms for
height reads over the fixture's present, blank, absent and aliased Chunks
and border coordinates. The light submission workload repeatedly merged
into the same region and took 972.85 → 947.99 ms. All processes returned the
same checked outcomes within each workload.

In the four-Chunk population workload, median client ore time was
72.19 ms before, 66.32 ms with the height change, 72.50 ms with the
submission change, and 69.27 ms with both. The server regression control
went from 66.02 to 67.67 ms with both. All 64 processes produced identical
native reports within each side. The combined workload showed a small
client gain and a small server regression in this sample.

All six DISPLAY2 runs in the initial comparison qualified at 854×480 on
Tiny. Median FPS was 30.19 → 36.23, median p95 was 67.21 → 51.36 ms, and
median p99 was 98.12 → 88.55 ms. One pair lost FPS and had a higher p99, and
the longest candidate frame took 141 ms. Net resident Chunk growth ranged
from 15 to 29 across runs. Both builds shared the local Sign screen edit
and stayed in gameplay during timing.

All six runs in the separate repeat also qualified. Median FPS was
41.21 → 43.43 and median p95 was 50.38 → 43.06 ms. Median p99 went from
63.88 to 64.47 ms. Two of three pairs had a higher p99, and the longest
candidate frame took 102 ms.

### Tree placement diagnostics

Tree placement uses quiet Block setters that write the caller's diagnostic
only on failure. Each setter still reserves its own workspace before it
mutates the Chunk. Completed writes retain their existing effects if a
later read fails. The small-tree fixture verifies the soil write and the
clean nested mutation record after a later Registry failure. A separate
test checks changed and unchanged metadata writes with a poisoned
diagnostic.

MSVC Release reduced the stack reservation in the tree's setter wrapper
from 25,024 to 64 bytes and removed the stack probe. In eight alternating
fixed-work pairs, all 146 placement receipts matched across the 16
processes. The receipt includes the Block and light planes, the ordered
light queue and failure sites. Existing fixture checks cover Random,
ownership and save bytes. Timings bracket the actual tree placement calls.

| Fixed workload | Before (ms) | After (ms) |
| --- | ---: | ---: |
| Client, 20 small trees | 11.94 | 10.73 |
| Client, one large oak | 1.77 | 1.53 |
| Server regression control, 20 small trees | 11.90 | 10.76 |
| Server regression control, one large oak | 1.84 | 1.54 |

Four instrumented DISPLAY2 runs had 11 tree groups with matching position,
species, successful result and operation counts. All 11 finished faster.
The tree with 1,159 writes fell from 6.02 to 5.84 ms at the median. The
tree with 1,072 writes fell from 5.18 to 4.01 ms. Both groups went from
one returned diagnostic per write to zero.

All 12 uninstrumented DISPLAY2 runs qualified at 854×480 on Tiny. They
shared the local Sign screen edit. Median FPS fell in both groups. The
first group's p99 was slightly lower, and the repeat's p99 was higher.
World and render work varied across processes.

| Scene group | Median FPS before / after | Median p95 frame time (ms) | Median p99 frame time (ms) |
| --- | ---: | ---: | ---: |
| First | 53.06 / 52.15 | 30.03 / 28.62 | 55.72 / 55.37 |
| Repeat | 54.61 / 52.24 | 28.28 / 27.83 | 50.65 / 53.34 |

### Heightmap reads during Block updates

Incremental height updates read the live Chunk fields they need and
capture the heightmap again after the range callback. The minimum-height
scan uses the array validated by the preceding store. It keeps the source
X/Z traversal and the initial minimum of 127. The retained execution
test clears the heightmap from a successful range callback. The setter
reports the missing array at the height store, keeps the earlier Block
and metadata writes, and releases both holds.

The same 146 fixed-work receipts matched across 16 Windows processes.
All eight pairs were faster for both the small-tree workload and the
large oak on each side.

| Fixed workload | Before (ms) | After (ms) |
| --- | ---: | ---: |
| Client, 20 small trees | 10.80 | 7.89 |
| Client, one large oak | 1.54 | 1.17 |
| Server regression control, 20 small trees | 10.85 | 7.86 |
| Server regression control, one large oak | 1.56 | 1.20 |

Four instrumented DISPLAY2 runs found 14 count-matched successful tree
groups and all 14 were faster. For the largest tree, with 1,159 writes,
the placement median fell from 6.33 to 3.46 ms. Full Chunk field reads
fell from 163,355 to 17,467. Sampled height time fell from 4.40 to 1.28 ms.
These are instrumented observations and the phase times overlap.

The following groups each contained six uninstrumented DISPLAY2
runs at 854×480 on Tiny. Both builds shared the local Sign screen edit.
Whole-scene medians varied as World and render work changed across runs.

| Scene group | Median FPS before / after | Median p95 frame time (ms) | Median p99 frame time (ms) |
| --- | ---: | ---: | ---: |
| Heightmap only | 54.27 / 53.14 | 27.50 / 28.22 | 46.08 / 49.30 |
| Both tree changes | 53.21 / 50.40 | 27.66 / 33.37 | 55.52 / 54.18 |

### Live field reads in Chunk setters

Block and metadata setters read the Chunk fields they use at each step.
The World receiver is read again after Registry access and before each
light submission. A retained execution check clears the metadata backing
from a successful height observer. The second metadata write reports
the missing backing after the earlier Block, metadata, height and
lighting work. Both native holds close on this path.

All 146 fixed-work receipts matched in each of the eight Windows
process pairs. The client median for 20 small trees moved from 7.79 to
7.83 ms, and the large oak moved from 1.156 to 1.154 ms.

Eight of nine count-matched trees in four instrumented DISPLAY2 runs
were faster. The largest tree took 3.66 to 3.96 ms in its single
matched pair, with 17,467 to 7,477 copies of the full Chunk view.
The next largest matched placement, with 1,011 writes, fell from
3.15 to 3.00 ms at the median.

### Spring fluid diagnostics

Two DISPLAY2 traces separated spring placement from the immediate flowing
Block tick. One tick took 129 ms and made 37,441 fluid material queries. Sampled
allocation and initialization of their diagnostic workspaces accounted for an
estimated 85 ms. The initial placement took 0.028 ms in that trace.

Ordinary fluid material reads now initialize the full diagnostic only on
failure. The workspace allocation still precedes the lookup. Failures retain
the reached site, cause and canonical fields, and successful queries still
update the World's registry diagnostics. Native client and server controls
cover a missing provider, missing Registry binding, null Block array and null
selected Block.

A fixed MSVC Release workload placed an accepted spring into an open shelf
with a lower basin. Each process ran the same authored layout on both sides
with fresh loaded Chunks and seeded supplied Random. Eight alternating pairs
compared the baseline and candidate with only the fluid object replaced.
Median client time fell from 296.95 to 140.15 ms, and server regression time
fell from 292.55 to 139.04 ms. Every process observed 115 water cells, 225
scheduling requests, one deferred entry in each scheduler collection, and
199 ordered lighting regions. The Chunk planes, fluid scratch state, queued
light bounds and checked Random state matched. The full native fluid controls
returned identical output with 636 checks per executable.

Four instrumented DISPLAY2 processes also observed natural springs. All
13 matched springs with at least 500 material queries took less time in
the candidate. Matches use coordinates, the fluid ID and the query counts.
The spring that made 37,441 immediate-tick queries took 119.86 ms before
and 36.19 ms after. That compares the reached fluid tick. Each process can
still do different generation and rendering work across the full interval.

All eight uninstrumented DISPLAY2 runs in the first cohort qualified. Median
FPS went from 41.17 to 39.38, and median run p99 rose from 77.59 to 96.62 ms.
The candidate's resident map grew by a median of 25 Chunks during measurement,
compared with 21.5 for the baseline. In a separate six-run repeat, median FPS
went from 29.27 to 28.46, and p99 fell from 154.05 to 139.78 ms.
Both frame products shared the local Sign screen edit in their common archives.

Nineteen focused tests passed on Windows and on Linux with Clang, ASan, UBSan
and leak detection. They include the fluid fixture's other consumers and the
population, scheduling, lighting and random-tick paths.

A separate resident diagnostic used 441 authored flat Chunks around the saved
camera. The live map stayed at 361 Chunks, with zero generation, population or
mesh compilations during the capture. The one qualified DISPLAY2 run averaged
295 FPS at 854×480, with a 9.43 ms p95. Opaque submission took 0.33 ms at the
median. This scene gives a steady-state control for future captures of terrain
and mesh work.

#### Registry lookups in spring route searches

Two sampled runs of the fixed accepted spring counted 824,345 Block static
lookups and 84,032 material reads on the client. Samples put the static
lookups at about 30 ms per run. In a separate measurement
immediately after the spring, one native lighting drain took 45–48 ms and
returned with 393 queued regions. The spring placement measurements stop
before the drain.

Fluid predicates read the live named cells and Block array in the original
order, and Registry lookup failures still populate their diagnostic. The
internal success paths leave the caller's record alone. An idle, ready
Registry can capture the live Block array without repeating initialization
admission. Ordinary fluid writes also skip initializing their outer
diagnostic before calling a setter that supplies its own full record.

The fixed Windows MSVC spring took a median of 159.32 → 136.95 ms on the
client and 150.51 → 142.04 ms on the server regression control. Seven of
eight client pairs and six of eight server pairs were faster. The 16 runs
matched the recorded Chunk planes, fluid scratch, lighting order and Random
state, and the ordinary fixture returned the same 637 checks on both builds.
This fixture exports its provider and exercises the generic lookup path.

Four separate instrumented DISPLAY2 runs qualified. Six of seven natural
springs matched by coordinates, fluid ID and at least 500 material queries
took less time in the candidate. The matched tick with 11,085 queries fell
from 16.97 to 11.72 ms.

All six uninstrumented DISPLAY2 runs qualified at 854×480 on Tiny. Median
FPS fell from 31.95 to 29.78, and median p99 rose from 85.64 to 104.76 ms.
Net resident Chunk growth ranged from 19 to 33. The two builds shared the
local Sign screen edit in their common libraries and stayed in gameplay.
All six runs in the separate repeat qualified. Median FPS rose from 33.45
to 35.48, and median p99 fell from 88.34 to 85.79 ms. The longest candidate
frames took 151 ms in the first group and 119 ms in the repeat.

#### Live Block fields in spring flow searches

The flow search reads the five named Block fields directly when the Registry
is ready and idle. It checks readiness after the World Block ID read, which
can call back. It still compares one live field at a time and stops at the
first match. Other Registry states use the regular active-use path. Native
client and server tests exercise all five null fields and replacements that
match before a later null field.

The fixed MSVC Release workload used the same accepted spring in all runs.
Each group measured eight alternating pairs with matching Chunk data, fluid
scratch, lighting order and Random state.

| Fixed spring | Median before / after (ms) | Candidate faster |
| --- | ---: | ---: |
| Client, exported provider | 59.56 / 47.68 | 8 of 8 |
| Client, source-owned provider | 56.73 / 45.98 | 5 of 8 |
| Client, source-owned provider, pinned to one logical CPU | 55.17 / 45.16 | 7 of 8 |
| Server regression, exported provider | 59.55 / 46.43 | 8 of 8 |

All source-owned runs kept the cache unexported during measurement and
observed 296,020 owned cache hits in the checked interval. The recorded
result had 115 water cells, one deferred entry in each scheduler collection
and 199 ordered light regions.

Six sampled DISPLAY2 runs yielded nine springs with matching coordinates,
fluid ID and per-operation counts. All nine immediate ticks were faster at
their matching group medians. The largest match made 29,274 obstacle checks.
Its median immediate tick fell from 10.86 to 8.80 ms, with sampled time in
the obstacle checks falling from 4.04 to 2.68 ms. The samples add overhead.

All 12 uninstrumented DISPLAY2 runs qualified at Tiny distance and 854×480.
Both builds shared the local Sign screen edit. Four of six pairs had higher
FPS, and three of six had lower p99. The longest candidate frame took 107.35 ms.

| Scene group | Median FPS before / after | Median p95 frame time (ms) | Median p99 frame time (ms) |
| --- | ---: | ---: | ---: |
| First | 51.76 / 52.78 | 35.16 / 31.88 | 56.89 / 67.36 |
| Repeat | 38.40 / 42.64 | 51.91 / 43.61 | 89.30 / 70.96 |

#### Material diagnostics during fluid ticks

Ordinary fluid Material reads keep the caller's diagnostic untouched while
the World query and Registry lookup run. A failed lookup fills the same
canonical diagnostic in the caller's storage after the lookup returns.
Previously each read reserved and freed a separate heap diagnostic. Removing
that reservation also removes its native allocation-failure point. Retained
execution continues to use its owned diagnostics. Native client and server
controls verify canonical reset of poisoned diagnostics and the later
failure site when a successful lookup reads a null Material field.

Two instrumented DISPLAY2 captures separated the remaining Material costs.
In the largest timed spring, 13,045 Material reads took an estimated 9.57 ms
of a 15.98 ms immediate fluid tick. Samples attributed 4.05 ms to diagnostic
reservation, 1.12 ms to World Block reads, 0.62 ms to Registry selection
and 3.30 ms to successful publication and release. The samples add overhead.

Each fixed MSVC Release workload used eight alternating pairs.

| Accepted spring | Median before / after (ms) |
| --- | ---: |
| Client, source-owned cache, pinned to one logical CPU | 90.19 / 55.41 |
| Client, exported cache | 89.27 / 61.94 |
| Server regression, exported cache | 89.69 / 64.42 |

All pairs were faster. The source-owned fixture retained 296,020 cache hits,
115 water cells, one deferred entry in each scheduler collection and 199
ordered light regions. All recorded Chunk planes, fluid scratch and Random
state matched.

Six instrumented DISPLAY2 runs yielded 14 springs matched by position,
fluid ID and operation counts, apart from the diagnostic allocation count.
All 14 immediate fluid ticks took less time in the candidate. The largest
match repeated 13,045 Material reads. Its median immediate tick fell from
16.50 to 10.74 ms, and sampled Material time fell from 7.52 to 1.94 ms.

All 12 uninstrumented DISPLAY2 runs qualified at Tiny distance and 854×480.
Both builds shared the local Sign screen edit. All six pairs had lower
candidate FPS, and five had higher p99. Whole-scene timing differed from
the fixed and matched spring results. The longest candidate frame was
230.70 ms.

| Scene group | Median FPS before / after | Median p95 frame time (ms) | Median p99 frame time (ms) |
| --- | ---: | ---: | ---: |
| First | 24.00 / 23.55 | 88.58 / 92.42 | 132.66 / 149.70 |
| Repeat | 36.37 / 35.69 | 59.30 / 55.86 | 82.86 / 87.63 |

### Generic resident Chunk lookups

After a provider exports its live cache, ordinary GETs fully validate the
cache and pending list on every call. A completed lookup can now return
before setting up the acquisition frame used by callbacks. A miss that needs
loading enters the existing coordinator after the same validation. Owned
index scopes, rebound providers and retained execution keep their existing
dispatch. Public success diagnostics and failure-only outputs keep their
respective contracts.

A private variant of the resident workload exported the provider once before
timing. Seven samples of 65,536 calls in each fresh process checked exact
Chunk identities, packed aliases, diagnostics, Random state, cache and pending
contents, and ownership. Four alternating process pairs ran at each size.
Entries below are median process medians on Windows MSVC Release.

| Resident Chunks | Before | After |
| --- | ---: | ---: |
| 8 | 29.42 ms | 5.06 ms |
| 64 | 45.97 ms | 24.80 ms |
| 256 | 139.13 ms | 106.42 ms |

The fixed spring also uses an exported provider. In eight alternating pairs,
median client time fell from 226.85 to 102.30 ms. The server regression control
fell from 206.18 to 101.53 ms. The 16 runs matched the recorded Chunk planes,
Random state, fluid scratch, schedule counters and ordered light regions.

In a separate set of eight alternating pairs, the first lighting drain after
that spring fell from 87.91 to 37.33 ms at the median. All runs matched both
recorded light planes and the order and bounds of the 393 remaining regions.

All six runs in the frozen-input DISPLAY2 repeat qualified at 854×480 on Tiny.
Median FPS moved from 20.56 to 22.86 and median p99 moved from 153.51 to
138.45 ms. Two pairs had lower FPS and higher p99. Both builds shared the
local Sign screen edit and stayed in gameplay. Two separate diagnostic runs
kept the owned index and counted zero calls to the new shortcut during
measurement, so the scene provides no FPS attribution for the shortcut.
A source-binding update during the initial attempt invalidated its last run.

## Reproduce one resident scene

Configure with `-DMC_BUILD_BENCHMARKS=ON` and build the
`mc_benchmark_provider_cache_scope` target. The public runner creates fresh,
asset-independent authored World files and retains raw output plus a checked
report:

```sh
python -B scripts/benchmarks/provider_cache_scope.py \
  --executable build/mc_benchmark_provider_cache_scope \
  --artifacts .local/provider-cache-scope --chunks 256 --depth 2
```

Use the corresponding `.exe` path on Windows. Resident counts are 8, 64 or
256; depth is one or two. A single invocation observes one executable and
cannot establish a revision speedup. The normal and optimized Python reader
controls reject malformed completion, semantic receipts and modified authored
files, including the full 256-Chunk region layout.
