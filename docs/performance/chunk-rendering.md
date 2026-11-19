# Chunk rendering and EW diagnostics

## Tessellator colors and terrain selection

Block-face color writes can use the RenderGlobal scene's existing Tessellator
class hold. They still check the active receiver and floating-point mode and
perform the same color conversions and writes. The per-call hold remains the
fallback when the scene cannot supply the ownership.

Four alternating Windows Release pairs used the same native section compiler
workload and authored input. Each sample compiled 25 sections. Vertex bytes,
draw counts, and World bindings matched.

| Section and lighting | Median before | Median after |
| --- | ---: | ---: |
| Filled, ambient occlusion | 5.265 ms | 5.152 ms |
| Surface, ambient occlusion | 0.746 ms | 0.720 ms |
| Filled, flat | 2.217 ms | 2.225 ms |
| Surface, flat | 0.300 ms | 0.290 ms |

Terrain selection now appends directly to the admitted list while its backing
has room. List growth keeps its checked allocation path. Batching still
reloads the live list after any draw that can call back.

Eight alternating Windows Release CPU pairs ran the actual selection prefix
for both passes on 17×17×8 and 33×33×8 renderer grids. Time per visited
section fell by 12.7–18.6% across the four median comparisons, with all 32
paired cases faster. Selection order, counters, failure receipts, list state,
and floating-point state matched. In six pairs per grid with full graphics
submission, both framebuffer hashes and work counts matched, and median frame
time changed by less than 1%.

Four alternating pairs measured both changes together during ten seconds of
Far-distance movement after a two-second warmup. All eight runs used the same
authored 854×480 scene, returned 200 World ticks, walked 42.91 blocks, and
saved the same Player position and World time through Save and Quit.

| Pair | FPS before / after | Frame p99 before / after |
| --- | ---: | ---: |
| 1 | 84.29 / 71.51 | 17.80 / 16.68 ms |
| 2 | 72.15 / 72.56 | 16.54 / 16.48 ms |
| 3 | 71.17 / 72.80 | 17.38 / 16.75 ms |
| 4 | 72.56 / 73.99 | 16.24 / 16.48 ms |

The moving results varied. Median FPS was nearly level, and frame p99 fell
in three pairs. The first attempts hit the observer's disk snapshot cap on
both builds. The rows above use the same larger snapshot cap on both builds
and all passed the save check. Seven focused rendering and ownership tests
passed on Windows Release and on Linux Clang with ASan and UBSan.

## Tessellator lookup during section compilation

The warmed Tessellator lookup clears the caller's graphics error record in
place. MSVC previously cleared a 1,272-byte stack temporary and copied it into
the caller before checking the class phase. The lookup still reads the live
singleton and writes the call site, including on the ready path. A regression
control poisons the error record and checks the returned owner, caller and
cleared diagnostics.

Four alternating Windows MSVC Release pairs ran 750,000 ready lookups per
sample. Both executables used the same workload object and alternated two
caller sites. Median time fell from 29.64 to 12.05 ns per call.

A second Windows workload compiled two authored sections through the native
compiler. Four alternating pairs per lighting mode used 30 compilations per
sample. The table gives the median of the per-process medians after two warmup
samples. All runs matched emitted vertex bytes, draw counts, input data and
World binding counts.

| Section | Lighting | Before | After |
| --- | --- | ---: | ---: |
| 16³ filled volume | Flat | 2.277 ms | 2.147 ms |
| 16×1×16 surface | Flat | 0.311 ms | 0.293 ms |
| 16³ filled volume | Ambient occlusion | 5.138 ms | 5.078 ms |
| 16×1×16 surface | Ambient occlusion | 0.739 ms | 0.731 ms |

Flat compile time fell in every pair. Ambient occlusion varied by pair. A
separate trial also inlined entry to the block-render diagnostic frames. It
added no measurable gain in these workloads and was dropped. The final code
passed all 12 focused tests on Windows MSVC Release and on Linux Clang Debug
with ASan and UBSan. The tests cover Chunk compilation, ambient occlusion,
flat lighting, face geometry, inventory geometry and graphics ownership.

Two alternating native Far pairs at 854×480 measured ten seconds of forward
input after a two-second warmup. Both builds shared the observer and all
libraries except RenderEngine. All four runs matched the authored save and
textures, returned 200 World ticks, moved 42.91 blocks and produced the same
saved Player position and World time through ordinary Save and Quit.

| Pair | FPS before / after | Frame p95 before / after | Frame p99 before / after |
| --- | ---: | ---: | ---: |
| 1 | 71.32 / 72.36 | 15.71 / 15.54 ms | 16.70 / 16.54 ms |
| 2 | 71.59 / 71.40 | 15.71 / 16.08 ms | 16.83 / 17.80 ms |

The frame results vary by pair. The fixed-work improvement belongs to the
lookup and flat section compile measurements.

## Render-distance rebuilds

The authored Windows rebuild case reached its 8 MiB arena limit on the
sixth Tiny/Far change. Retiring displaced renderer grids lets all 12 changes
complete. The arena charge peaked at 4,207,616 bytes with an older grid held
by the selection list and fell to 4,146,304 bytes on the next rebuild after
the list cleared. The failure case checks retry with the actual bounds owner
restored. Run the case
with `ctest --test-dir build-windows -R '^client\.render_global_world_rebuild_budget$'`
on a Windows client configured for startup and display tests.

The first cleanup implementation grew each Chunk renderer from 208 to 224
bytes. Three alternating Far rounds measured 72.91 FPS before, 67.62 FPS
with that layout, and 65.44 FPS when the same build also reused registry
diagnostics during face lighting. All nine runs qualified and completed
200 World ticks. The final implementation restores the 208-byte layout. The
face-lighting trial was dropped.

Four alternating pairs against `22837ae9` measured 72.21 FPS before and 72.23
FPS after, with two pairs faster and two slower. Median run p95 frame time
was 15.32 ms before and 15.26 ms after. All eight intervals completed 200
World ticks in the stationary authored Far scene at 854×480 after a two-second
warmup, on an i9-10900F with an RTX 3060. The client's final counters still
reported 214–233 Chunk updates per second across the eight runs. The rebuild
test checks arena use during distance changes.

## Direct Block reads during section compilation

In one sampled Windows Far run, `block_value` was the most frequently
observed project function, with 221 of 3,418 render-thread samples. The
samples record wall-clock positions and include pause overhead. Frame
comparisons below used uninstrumented builds.

The cached Block read now distinguishes an array read from a default air
result. The public getter still reports the reached success site. The
failure-only getter can return without writing that site. Nested failures
still include the Block read before the enclosing getter, and the row scan
retains its last successful value if a later read fails.

The Windows MSVC Release fixed-work comparison ran six alternating process
pairs, each with seven samples of 65,536 reads over nine leased Chunks.
Median time fell from 9.46 to 7.48 ns per read. Every pair passed exact Block
value, ownership, World state and untouched success-error checks. A separate
six-pair getter workload measured minimum brightness at 53.60 and 51.75 ns
before and after. Material, opacity and normal cube also took less time at
the median. Brightness increased from 52.81 to 53.03 ns.

The frame baseline was `911d56be`. The stationary authored Far scene ran at
854×480 on an i9-10900F with an RTX 3060. All 14 matched ten-second runs
completed 200 World ticks. Section compilation continued throughout the
timing window after a two-second warmup, and FPS rose in three of four pairs.
After a 25-second warmup, the final counters showed zero Chunk updates in all
six runs.

| Far timing window | Median FPS before / after | Median p95 before / after | Median p99 before / after |
| --- | ---: | ---: | ---: |
| Two-second warmup | 72.83 / 73.43 | 15.13 / 15.20 ms | 16.03 / 16.30 ms |
| 25-second warmup | 431.67 / 431.35 | 3.39 / 3.28 ms | 6.30 / 6.46 ms |

The first trial also forced inlining of the shared Block read. It lowered
direct lookup time from 9.28 to 5.91 ns and raised median FPS from 72.26 to
73.39 during section builds. After a 25-second warmup its median FPS fell
from 437.75 to 426.92, and minimum brightness rose from 53.33 to 55.25 ns
in the fixed-work control. The final version lets the compiler decide where
to inline the shared read.

A separate vertex trial paired the x and y floating-point operations. It
failed the authored test for a vertex with distinct NaN payloads. Finite
vertex output matched across all six CPU pairs. The trial was dropped.

## Block-read diagnostic setup

The shared Chunk byte reader now returns the reached read status. The
failure-only caller builds its diagnostic frame after a failed read. Public
calls still report either the array read or the default air result on success.
Nested getters keep the reached failure site, and failed row scans keep the
last successful byte. A separate cursor stops at the failed position. Render
cache callers check success before entering the code that captures failure
details.

The [fixed-work Block benchmark](../../tests/benchmarks/chunk_cache.md)
read 65,536 positions per sample. Both cohorts matched returned bytes,
public diagnostics, success sentinels, state and ownership checks. The
MSVC Release failure-only getter reserves 48 bytes of stack space in its
prologue, down from 264, and the success path skips its diagnostic stores.

| Windows cohort | Median before / after | p95 before / after |
| --- | ---: | ---: |
| Six pairs, 42 samples per build | 7.53 / 6.16 ns | 0.6019 / 0.7634 ms per sample |
| Ten pairs, 70 samples per build | 7.33 / 6.02 ns | 0.5803 / 0.4870 ms per sample |

Six additional pairs exercised the typed render getters with matching result
hashes, light planes, ownership and World state. Median brightness moved from
51.09 to 50.68 ns, minimum brightness from 51.25 to 51.51 ns, material from
16.87 to 16.43 ns, opacity from 22.23 to 21.42 ns, normal cube from 25.14 to
24.07 ns, and Chunk manager from 7.66 to 7.71 ns.

Four early Far pairs compared these changes against `8abc9bf7` after a
two-second warmup. The stationary authored scene ran at 854×480 on an
i9-10900F with an RTX 3060. All eight ten-second runs completed 200 World
ticks and continued compiling sections. Median FPS rose from 73.92 to 75.99
(+2.80%) and every pair gained FPS. Median run p95 fell from 14.95 to 14.64 ms.
Median p99 fell from 15.72 to 15.60 ms, with three pairs improving.

Three pairs after a 25-second warmup completed 200 World ticks per run and
reported zero Chunk updates. Median FPS fell from 458.43 to 457.07, and
median p95 rose from 2.98 to 3.06 ms. Median p99 fell from 6.14 to 6.02 ms.

## Tessellator vertices under a scene hold

Cloud emission uses the class hold already owned by RenderGlobal for UV
vertices in the canonical buffer. The Tessellator checks its published
receiver, reentry and retirement before writing. It takes the
usual temporary class hold if a vertex could flush the batch, or if the
scene or buffer checks fail. Both routes use the same vertex writer. On MSVC,
the shared admission checks and UV stores are inlined in the hot entry points.

Six alternating Windows MSVC Release pairs ran the same authored CPU
fixture. Median cost per input vertex fell from 53.58 to 40.09 ns for
triangles and from 54.76 to 41.55 ns for quads with duplicated vertices.
Each process used nine timed samples per mode. The pairs matched the full
vertex allocation, buffer positions, diagnostics and x86 floating-point state.
Six separate public-entry pairs measured 53.49 and 53.08 ns for triangles
and 54.46 and 53.77 ns for quads.

The paired CPU regression covers 22 cases, including signed cursor wrap,
partial writes, busy and retired receivers, and the automatic draw cutoff.
The Display test also reaches that cutoff through a real GL draw. Its callback
sees the temporary class hold and gets BUSY on a nested vertex call.
Eight focused tests passed on Windows MSVC Release, including 72 authored
cloud states. Seven passed on WSL Clang Debug with ASan, UBSan and llvmpipe.

The frame observer compared the final route with `b321b9e0` in a stationary,
authored Far scene at 854×480 on an i9-10900F with an RTX 3060. All 14
qualifying intervals advanced 200 World ticks. The early runs kept compiling
sections and the settled runs reported zero Chunk updates.

| Warmup and workload | Median FPS before / after | Median p95 before / after (ms) | Median p99 before / after (ms) |
| --- | ---: | ---: | ---: |
| 2 seconds, early, four pairs | 76.00 / 76.10 | 14.54 / 14.50 | 15.62 / 15.24 |
| 25 seconds, settled, three pairs | 448.73 / 450.86 | 3.01 / 2.96 | 6.13 / 6.21 |

An earlier trial also changed UV submission during Chunk compilation. In
three matched early rounds, that trial measured 76.05 median FPS against
78.25 for the baseline, so the scene route is limited to cloud vertices.
One settled baseline interval in the intermediate cloud trial lost
foreground focus. Its receipt was kept and the cohort did not qualify.

## Tessellator error initialization

Two settled Far traces at `3339393c` recorded 6,205 render-thread positions
in frames without a World tick. Scene-held UV submission accounted for 544
positions and its vertex writer for 249. Sampling adds overhead. The timing
comparisons use separate uninstrumented executables.

Tessellator admission clears the caller's graphics error in place. MSVC
previously zeroed a 1,272-byte temporary and copied it to the caller. The
scene vertex entry's stack reservation fell from 1,408 to 128 bytes.
The receiver checks, vertex writer and automatic flush path use the
same error fields and control flow.

Six alternating Windows MSVC Release pairs called the same scene vertex
entry in both builds. Median time fell from 40.88 to 22.94 ns per input
triangle vertex and from 41.58 to 24.05 ns per input quad vertex. The full
vertex allocations, buffer positions, diagnostics and MXCSR matched.

Three uninstrumented early Far pairs compared the change and
[native replay fix](../client/renderer-frames.md#native-replay-after-context-reentry)
with `3339393c`. The stationary authored scene ran at 854×480 on an
i9-10900F with an RTX 3060. All six intervals advanced 200 World ticks
and kept compiling sections after the two-second warmup. Median FPS
rose from 74.26 to 75.81. Overall p99 fell from 16.23 to 15.90 ms, while
p99 for frames containing a World tick rose from 16.35 to 16.62 ms.

Four pairs after a 25-second warmup also ran 200 World ticks per
interval and reported zero Chunk updates. Median FPS rose from 435.15
to 480.93. Median p95 fell from 3.96 to 3.37 ms and p99 from 6.85 to
6.72 ms. For frames without a World tick, median p99 went from 3.60
to 3.30 ms.

A separate packed matrix-translation trial matched 1,087,776 finite
cases. It selected a different NaN payload when two distinct payloads
reached the same addition, so the trial was rejected.

## Cached lighting and Tessellator counters

Two Windows Far traces after a two-second warmup recorded 7,005 render-thread
positions while sections compiled. Block reads accounted for 460 named samples,
cached lighting for 392, and the ambient occlusion body for 228. Of the 406
samples in the slowest 5% of frames, cached lighting accounted for 25 and Block
reads for 21. Sampling pauses the render thread briefly. The frame comparisons
use uninstrumented builds.

Cached lighting branches on Block registry readiness before entering the slot
loop. The ready path reads live slots in order and stops at the first match or
null slot. If the registry needs active use, each reached lookup uses the
captured name and keeps its original failure location. The [fixed-work getter
benchmark](../../tests/tools/world/benchmark_chunk_cache_render.py) ran six
alternating pairs with 65,536 queries per sample. Result hashes, light planes,
ownership and World state matched. A trial that split recursive sampling into
a separate routine took longer on both brightness workloads and was dropped.

| Getter | Median before / after (ns) |
| --- | ---: |
| Brightness | 53.49 / 49.91 |
| Minimum brightness | 51.95 / 49.84 |
| Material | 16.46 / 16.76 |
| Opacity | 21.21 / 21.90 |
| Normal cube | 24.19 / 25.05 |
| Chunk manager | 7.63 / 7.51 |

The renderer inlines its store bookkeeping with the same recorded program
counters. The Tessellator advances its hot counters with fixed-width unsigned
arithmetic and copies the resulting bits back to each signed field. Authored
tests cover wraparound in ordinary submission and while duplicating a quad.
Six alternating CPU pairs matched every stored vertex word, counter and
MXCSR result across 4,096 inputs per batch in each mode. Median submission time
fell from 53.34 to 52.93 ns in mode 4 and from 54.97 to 53.90 ns in quad mode.

Four early Far pairs compared these changes against `3b4c095e` in the
stationary authored 854×480 scene on an i9-10900F with an RTX 3060. All
eight ten-second intervals completed 200 World ticks and continued compiling
sections. Median FPS rose from 74.51 to 75.83 (+1.77%). Median run p95 fell
from 14.77 to 14.51 ms and p99 fell from 15.69 to 15.27 ms. FPS rose and p99
fell in every pair.

Three pairs after a 25-second warmup completed the same World tick count and
reported zero Chunk updates. Median FPS moved from 457.66 to 455.67 and
p99 from 5.94 to 5.96 ms. Median p95 fell from 2.97 to 2.96 ms. One pair
gained FPS, and one pair had lower p99.

## Sorted terrain array admission

The first terrain selection loop validates the sorted array span once.
Each entry still checks its renderer owner, null receiver and live pass
array. Malformed spans use the per-entry checks, preserving the counters
and selected-list prefix before failure. The later batching loop keeps
its live reads because appending a full batch can reach a GL callback.

Six alternating Windows Release pairs replayed two authored terrain
passes on each grid. Every pair matched the framebuffer, pixel coverage,
section order and work counts across 160 timed frames per process.

| Grid | Median before / after (ms) |
| --- | ---: |
| 17 × 17 × 8 sections | 0.16570 / 0.16273 |
| 33 × 33 × 8 sections | 0.24635 / 0.23250 |

A CPU-only fixture ran 64 selection sweeps per sample with a preallocated
selected list. Six alternating pairs matched section order, counters,
diagnostics, retained storage and floating-point controls. Median time
per section fell from 12.21 to 11.20 ns in pass 0 and from 11.27 to 10.21 ns
in pass 1 on the 17-section grid. On the 33-section grid it fell from
12.42 to 11.30 ns in pass 0 and from 11.51 to 10.30 ns in pass 1.

The CPU controls cover null and oversized arrays, signed and incomplete
ranges, empty and reversed spans, invalid passes and a later List
allocation refusal. They verify the reached diagnostic and retained
counter, cursor and selected-list state.

All four focused terrain-submission and entity pickup tests passed on
Windows MSVC Release and Linux Clang Debug with ASan and UBSan. Windows
compared all 35 terrain replay rows. Mesa compared 30 rows and reported
five uncredited rows for its unsupported occlusion profile.

## ChunkCache construction and retirement diagnostics

The first World getter establishes the canonical acquisition diagnostic.
Later successful getters preserve that record, and a failure captures its
full child diagnostic. Construction initializes the retained record in
place. Retirement keeps the Store error locally and fills the retained
cleanup record when a lease release fails. The retry resumes at the first
lease that still needs releasing.

Six rounds covered all six orders of the three Windows Release builds.
Each process started from the first loaded Far World with 289 resident
Chunks and the same 4,096-entry ceiling. Each measured sample created and
retired 2,560 3 × 3 ChunkCaches and made exactly 23,040 resident GETs.
Map and Chunk data hashes matched, and the fixture checked the selected
cells, source positions, diagnostics and leases.

| Build | Median construction and retirement (µs) |
| --- | ---: |
| Baseline | 3.477 |
| Reused acquisition diagnostic | 1.972 |
| Direct construction and cleanup records added | 1.707 |

## Scene-held terrain vertices

Terrain vertex submission uses RenderGlobal's retained scene hold when
the canonical Tessellator buffer has room for the write. Unsafe buffer
states and automatic drawing use the ordinary call hold.

Six alternating Windows Release pairs each measured 65,536 synthetic
terrain vertices per sample through the actual terrain vertex entry.
The full raw and upload buffer hashes, source positions, diagnostics,
and ownership receipts matched. Median time fell from 40.04 to 26.84 ns
per vertex. The device fixture also exercises the real automatic draw
callback and verifies rejection of reentry.

## Native Far frame checks

Six alternating ten-second Far runs per build used the same authored
inputs and starting position. Each pair matched the ending position,
200 World ticks and scheduler work counts. FPS changed only slightly
across this scene. Five of the six pairs had higher p99 frame time.

| Median across runs | Baseline | Candidate |
| --- | ---: | ---: |
| FPS | 72.41 | 72.52 |
| p95 frame time (ms) | 15.48 | 15.56 |
| p99 frame time (ms) | 16.69 | 16.89 |

One 75-second pair with builds idle matched 1,500 World ticks and the
ending Player position. Baseline and candidate FPS were 201.02 and
198.79, with p99 frame times of 17.86 and 17.84 ms. The provider held
1,213 and 1,214 Chunks at the end of the two intervals.

The extended authored route on the candidate completed 75 seconds and
1,500 World ticks. Save and Quit wrote World time 1,544 and Player Z
−324.15. One reload lost window focus and paused. A focused retry
selected another copy of the same save, ran 200 World ticks and saved
at World time 1,789 and Player Z −367.32. Save readback used an
independent NBT decoder.

Eight focused tests passed on Windows Release and Linux Clang with ASan
and UBSan. Both passed the automatic-draw reentry, acquisition,
ChunkCache, retained cleanup and Chunk compilation controls. Windows
compared all 35 terrain submission rows. Mesa compared 30 and reported
five uncredited for its unsupported occlusion profile.

## ChunkCache lookup and lease trial

The trial combined the canonical Store object lookup and lease acquisition
for each normal ChunkCache cell. Both MSVC Release observers used the
same 1,024-entry budget and began at the first loaded Far World with 289
resident Chunks. Map and Chunk data hashes matched across six alternating
pairs. Each process verified selected identities and leases. Each measured
sample created and retired 2,560 actual 3 × 3 ChunkCaches.

Median time per construction and retirement was 3.480 µs with the existing
path and 3.504 µs with the combined call. The combined call was faster in
three pairs and slower in three. The trial was dropped. The measurement
isolates CPU construction and retirement from the frame loop.

## Settled Far culling and terrain selection

Two sampled Windows Far runs after a 25-second warmup recorded 6,045
render-thread positions inside frames with no World tick and 907 inside
frames with a tick. Terrain selection accounted for 293 named samples in
the first group. The culling loop and its array lookup accounted for 324,
and Tessellator admission accounted for 371. The raw Block reader accounted
for 136 samples in frames with a tick. These are host-clock samples with
observer overhead. The frame comparisons use uninstrumented executables.

Culling now validates the array extent once per exclusive walk and uses
the same checked section for emptiness, throttling and the bounds test.
The classification counters use unsigned addition for their signed wrap.
The controls cover the null array, oversized extent, empty array, foreign
section, failed bounds test and counter wrap.

The fixed-work Windows Release comparison ran six alternating pairs per
case. The culling workload visited 5,408 mixed sections per call and matched
visibility bits, tested counts and throttle phase through signed wrap.
The terrain workloads matched framebuffer hashes, pixel coverage, section
order, dirty polls and work counts. Each workload was faster in all six
pairs.

| Workload | Median before / after |
| --- | ---: |
| Culling, 5,408 sections | 0.10236 / 0.07854 ms |
| Two terrain passes, 17×17×8 grid | 0.16628 / 0.16410 ms |
| Two terrain passes, 33×33×8 grid | 0.24995 / 0.24710 ms |

Four matched Far pairs compared these changes and the [random-display
optimization](random-display.md#detail-storage-on-block-dispatch) against
`d08ef54c`. The authored view was stationary at 854×480 on an i9-10900F with
an RTX 3060. All eight ten-second intervals completed 200 World ticks and
reported zero Chunk updates after the 25-second warmup. Median FPS rose
from 424.65 to 439.47. Median run p95 fell from 3.43 to 3.18 ms and p99 fell
from 6.35 to 6.11 ms. FPS rose and p99 fell in every pair.

Three additional pairs used a two-second warmup and continued compiling
sections throughout the timing window. Each run completed 200 World ticks.
Median FPS rose from 74.11 to 74.55, p95 fell from 15.05 to 14.87 ms, and p99
fell from 16.00 to 15.73 ms. Two pairs were faster.

## Lighting admission for repeated normals

A lit batch retains two successful normal proofs and replaces the older entry
on a miss. A hit requires all three input floats to match bit for bit. The
modelview and normal matrix, scale and flags are fixed for the batch. Every
vertex still checks its eye transform and any position-dependent light,
material or specular bounds.

The Windows Release fixture ran six alternating process pairs with 4,096
vertices per batch. Twelve successful mode and input combinations matched their
admission results, success sentinels and MXCSR flags. Five edge cases also
matched, including a nonfinite eye position after repeated normals and
overflow in eye and normal transforms.

| Normals per batch | Median range across four lighting modes |
| --- | ---: |
| One repeated normal | 38–48% less CPU time per vertex |
| Two alternating normals | 35–46% less CPU time per vertex |
| All unique normals | 3–4% more CPU time per vertex |

Four matched Far pairs compared lighting admission and [fixed-width Chunk byte
indices](world-queries.md#fixed-width-chunk-byte-indices) against `561571f5`.
The authored scene was stationary at 854×480 on an i9-10900F with an RTX 3060.
After the 25-second warmup, FPS rose in every pair and p99 fell in every pair.
All eight ten-second intervals completed 200 World ticks and reported zero
Chunk updates. The median of the run FPS rose from 431.56 to 450.15 (+4.31%).
Median run p95 fell from 3.46 to 3.10 ms and p99 fell from 6.20 to 6.03 ms.

Three two-second-warmup pairs kept compiling sections through all six intervals.
Each interval completed 200 World ticks. Median FPS fell from 74.65 to 74.30
and median run p99 rose from 15.49 to 15.97 ms. One pair improved on both
measures. Median run p95 rose from 14.76 to 14.86 ms.

### Normal proof lookup

A normal proof hit also covers the input's finiteness. The position check
runs first, and a normal miss checks all three components before arithmetic.
In six alternating Windows Release pairs against `7e5c1ffe`, all 12 mode and
input medians fell by 0.13–1.98%. Individual pairs varied. All 17 admission,
diagnostic and MXCSR controls matched.

Three settled Far pairs ran after a 25-second warmup. All six ten-second
intervals completed 200 World ticks with zero Chunk updates. Median FPS rose
from 445.65 to 448.39 (+0.61%). Median p95 rose from 3.14 to 3.19 ms, while
p99 fell from 6.15 to 6.07 ms. Two pairs gained FPS and all three had lower p99.

Two pairs after a two-second warmup continued compiling sections and completed
200 World ticks per run. The FPS gains varied from 0.51 to 7.43, with a median
of 77.43 FPS before and 81.40 after. Median p99 fell from 15.29 to 15.13 ms.

## Sky-change dirty marking

The sky-change callback now checks the Chunk renderer's fixed owner on
each reached array read. It still appends each clean, sky-lit renderer to
the dirty list before setting its dirty flag. The foreign-owner test
keeps the two earlier appends and flags, then retains the original
failure across a later successful call.

The fixed MSVC Release workload invokes the actual WorldAccess callback
on a live World. One quarter of the renderers are eligible for marking.
All eight alternating pinned pairs preserved the complete dirty-list
order and flags. The 5,408-renderer median fell from 198.54 to 0.028 ms.
The 200-renderer median fell from 0.0347 to 0.00157 ms.

Two ordinary Far traces had no sky change. A separate Far dusk scene
started at World time 12,420 and reached the same sky-change callback
at tick 12,541 in all four instrumented DISPLAY2 runs. Callback times
were 122.54 and 131.66 ms before, and 0.095 and 0.133 ms after.
The frames containing that callback fell from 186.13 to 61.62 ms and
from 202.20 to 59.43 ms. Entity work varied across processes.

Six uninstrumented DISPLAY2 runs of the same dusk scene at 854×480
went from 27.88 to 31.72 median FPS and from 80.61 to 64.34 ms median
p99. Two pairs improved on both measures. The longest frame was lower
in all three pairs. These intervals completed 190 to 201 World ticks
and both builds shared the local Sign edits.

## Chunk renderer ownership and Block lookup

Culling and dirty-list reads use the Chunk renderer's fixed native owner.
Each array or list read still checks the reached element. The dirty-list
distance comparator reads the three live camera coordinates and keeps its
original comparison order. Two controls place a live renderer from another
owner in the actual array or list and check the mutation prefix before the
same reached failure.

The MSVC Release culling control allocates actual renderers and marks both
passes empty. Each call still visits every renderer and advances the culling
counter. All eight alternating pinned pairs were faster for both grid sizes.

| Empty renderers per call | Median before | Median after |
| ---: | ---: | ---: |
| 200 | 0.0219 ms | 0.00229 ms |
| 5,408 | 114.15 ms | 0.0285 ms |

Compilation reads the TileEntity flag array directly when the Block Registry
is ready. It reads the live Block array after any TileEntity work. Other
Registry states and array failures use the existing lookup path. Four
instrumented Tiny runs matched 97 compile groups by position, Block work per
pass and TileEntity calls. Seventy-seven groups were faster. Their median
moved from 6.71 to 6.60 ms.

The combined change qualified all six uninstrumented Far runs at 854×480 on
DISPLAY2. Median FPS rose from 1.36 to 29.47. Median frame time fell from
739.20 to 30.90 ms. Median run p99 fell from 786.11 to 71.78 ms. The baseline
had 14 or 15 frames per interval, so its p99 is also its maximum. Baseline
intervals completed 140–150 World ticks, and candidate intervals completed
200. All three pairs used the same authored pose, assets, render distance
and local Sign edits. The game scene includes culling, compilation, drawing
and simulation.

Six uninstrumented Tiny runs used the same resolution and pose. Median FPS
rose from 52.81 to 54.86, while median p95 moved from 28.13 to 28.27 ms and
p99 from 47.90 to 48.95 ms. Two pairs had higher FPS and lower p99 occurred
in two pairs. The World completed 200 or 201 ticks per run.

## Working diagnostics in Chunk-cache getters

Chunk-cache getters keep the reached call frames and Registry diagnostic
on the stack. They build the full error record when a public caller
requests it or when a failure-only getter fails. This keeps each reached
receiver and child diagnostic, including a TileEntity lookup's result.
The constructor and retirement still hold their full diagnostics.

The Windows MSVC Release assembly for the brightness body reserves 528
bytes of stack, down from 8,112 bytes, and omits the stack probe. The
public error record still occupies 7,888 bytes on this build.

The fixed-work comparison ran eight alternating process pairs pinned to
logical CPU 4. Each process measured seven samples of 65,536 calls per
getter over the same nine leased Chunks. All eight pairs had lower
medians for all six getters. Outputs, reported errors, light side
effects, World and Random state, and ownership matched.

| Query | Before (ns) | After (ns) |
| --- | ---: | ---: |
| Brightness | 66.35 | 61.93 |
| Minimum brightness | 67.31 | 62.13 |
| Material | 46.21 | 40.37 |
| Opaque | 33.69 | 27.13 |
| Normal cube | 37.23 | 31.06 |
| Biome Manager | 14.39 | 7.86 |

Four instrumented DISPLAY2 runs found three Chunk compile groups with
matching counts for all seven recorded operations. All three were
faster. The largest group used 30,720 minimum-brightness and 24,576
opaque queries per compile. It fell from 7.59 to 7.17 ms at the median
across 781 baseline and 884 candidate compiles. The other two groups
fell from 7.63 to 7.22 ms and 7.19 to 6.78 ms. Instrumentation adds
overhead to both builds.

Each group below used six uninstrumented runs on DISPLAY2 at 854×480
on Tiny. Both builds shared the local Sign screen edit. Whole-scene
medians also reflect changes in World and render work between runs.

| Scene group | Median FPS before / after | Median p95 frame time (ms) | Median p99 frame time (ms) |
| --- | ---: | ---: | ---: |
| Chunk-cache getters | 54.02 / 53.90 | 28.83 / 27.23 | 50.95 / 51.10 |
| Getters and [Chunk setters](provider-cache-scopes.md#live-field-reads-in-chunk-setters) | 54.46 / 52.52 | 29.77 / 30.20 | 54.26 / 50.61 |

## Registry reads in the Block renderer

When the Registry is ready and idle, the Block renderer checks the same
allocation totals and records the canonical success diagnostic before
reading live Block statics. It avoids copying the full Registry view.
Other Registry states and invalid allocation totals use the existing
initializer and inspection. Ambient occlusion captures its array before
the neighbor Block lookup. Brightness keeps the null-array and index
checks at their original sites.

The fixed Windows MSVC Release run used each actual renderer call across a
prepared 3×3×3 neighborhood. Eight alternating process pairs each measured
seven samples of 131,072 calls per operation after warm-up. All eight pairs
had lower medians for both operations. Checksums, ownership and diagnostics
matched, along with all 48 original AO field and vertex rows in every process.

| Query | Before (ns) | After (ns) |
| --- | ---: | ---: |
| Ambient occlusion flag | 45.54 | 27.43 |
| Block brightness | 92.71 | 74.30 |

Four instrumented DISPLAY2 runs found three groups with equal counts for
all nine recorded Chunk compile operations. All improved. The largest
group had 79,872 Registry reads and 49,152 AO reads per compile. Its
median fell from 10.25 to 8.79 ms across 431 baseline and 563 candidate
observations. The other groups fell from 10.70 to 8.74 ms and from 9.60
to 8.33 ms. The instrumented runs include sampling overhead.

The 12 uninstrumented runs used 854×480 on Tiny and the same local Sign
screen edit in both builds. The first group had higher FPS and lower p99
frame time, and the repeat moved in the opposite direction. The amounts
of World and render work varied across these runs.

| Scene group | Median FPS before / after | Median p95 frame time (ms) | Median p99 frame time (ms) |
| --- | ---: | ---: | ---: |
| First | 48.98 / 53.03 | 33.04 / 33.56 | 64.77 / 61.47 |
| Repeat | 52.98 / 52.26 | 29.70 / 34.92 | 53.24 / 61.93 |

## Live Block fields during Chunk compilation

Brightness reads the four live named Block slots in order when the Registry
is ready and idle. Each read still stops at the first matching Block or
reports the reached null field. Cold and entered registries keep their
ordinary active-use checks. Opacity and normal-cube reads keep the captured
Block array and write Registry diagnostics on failure. The Chunk traversal
keeps its Y, Z, X order and reuses the bounded loop increments.

Two instrumented DISPLAY2 captures spent 6.00 and 6.42 ms per frame on average
in Chunk compilation. One frame spent 69.21 ms in compilation. Brightness
made 6.61 million and 8.59 million named Block reads across the measured
intervals. These native clock observations include instrumentation overhead.

The Windows MSVC Release comparison ran eight alternating process pairs.
Each sample made 65,536 calls over the same nine leased Chunks, with 56 samples
per build and operation. All processes passed 4,238,612 checks and matched
outputs, light side effects, ownership, Random, and failure diagnostics.

| Query | Before (ns) | After (ns) |
| --- | ---: | ---: |
| Brightness | 287.66 | 182.64 |
| Minimum brightness | 287.65 | 183.30 |
| Opaque | 116.47 | 82.76 |
| Normal cube | 120.06 | 85.32 |

These four medians fell in all eight pairs. Material moved from 99.91 to
100.94 ns, and the Biome Manager control moved from 38.93 to 38.16 ns.

Four further instrumented DISPLAY2 runs produced three Chunk compile workloads
with matching counts for all seven recorded operations. All three took less
time. The largest group, with 30,720 minimum-brightness reads and 24,576
opacity reads per compile, moved from 24.01 to 19.03 ms at the median across
83 baseline and 95 candidate observations.

All 12 uninstrumented DISPLAY2 runs qualified at 854×480 on Tiny. The two
cohorts moved from 18.83 to 21.84 FPS and from 19.72 to 20.26 FPS at the
median. Median p99 fell from 204.95 to 166.35 ms and from 229.47 to 179.21 ms.
One pair had a higher p99, and the longest candidate frame took 328 ms.
Both builds shared the local Sign screen edit and stayed in gameplay.

## Packed light and registry lookups

Chunk-cache reads use local block indices after checking the height. A light
read computes that index once for both packed planes, with separate bounds
checks and the SkyLight side effect between reads. Chunk coordinates round down
at negative positions. Named-block reads use the live registry slot directly
when the registry is ready and idle. Other states and unknown names go through
the existing initialization and error path.

Two Windows MSVC Release comparisons on an Intel Core i9-10900F measured the
same failure-only accessor on each build. Each comparison ran six alternating
rounds with 42 samples per build and operation. Each sample made 65,536 queries.
Every process passed 4,238,612 checks, including output, light side effects,
ownership and save integrity. The second comparison pinned the benchmark
process tree to one logical processor at Above Normal priority.

| Query | Default median before / after (ns) | Pinned median before / after (ns) |
| --- | ---: | ---: |
| Brightness | 357.8 / 205.8 | 360.7 / 157.7 |
| Minimum brightness | 357.7 / 204.1 | 333.7 / 196.9 |
| Material | 75.8 / 91.1 | 65.7 / 76.9 |
| Opaque | 112.1 / 93.8 | 79.8 / 93.0 |
| Normal cube | 99.4 / 103.9 | 110.4 / 108.0 |
| Biome manager | 29.7 / 33.7 | 28.0 / 31.0 |

Both brightness medians fell in every round. Material medians rose in both
comparisons, opaque and normal-cube results changed direction, and the manager
control varied by 11–14%.

A later pinned control using the same executable on both sides measured
45.52 and 45.53 ns per Material read. A fresh six-round comparison of the
earlier and current lookup code measured 57.65 and 46.78 ns per Material
read. Five of six paired round medians favored the current code. The direction
reversal across cohorts leaves the earlier Material slowdown unconfirmed.

All six native frame runs returned with active simulation, valid foreground
and clean shutdown. Four runs drifted from the authored camera pose, which
invalidated the FPS and frame-time comparison. A separate Windows
save/reselection scenario passed 1,775 checks across 49 frames and verified
that both game windows stayed within `DISPLAY2` at opening and after reload.
The focused Windows selection passed 11 tests. The Linux Clang selection passed
eight with ASan, UBSan and leak detection.

Run the fixed-work comparison with the failure-only target from each build.

```sh
python -B tests/tools/world/benchmark_chunk_cache_render.py \
  --executable before=BASELINE/mc_benchmark_chunk_cache_render_failure.exe \
  --executable after=CANDIDATE/mc_benchmark_chunk_cache_render_failure.exe \
  --rounds 6 --artifacts .local/render-revision-comparison
```

## Failure-only getter diagnostics

Six internal EW render getters leave caller diagnostics untouched on success.
Public brightness, minimum brightness, material, opaque, normal-cube and biome
manager getters retain complete success and failure records. Both contracts
execute the same Source bodies; direct World access retains its existing path.

Only the scalar/frame prefix and Registry record are needed during these calls.
Internal calls initialize those fields, then materialize the full canonical
output after failure, without repeating any child. Nested frames, reached
Registry records, output preservation and native admission remain intact. Plain
brightness still captures the Dimension table before light querying; minimum
brightness still queries light first. Dimension holds and the actual `lm.a`
positive-SkyLight effect retain their order.

## Measured target

Private host-timing probes use the ordinary authored Windows Tiny scene, two
seconds of natural warmup and ten seconds of Source frames. A coarse probe
passes all 304 frames: cell traversal and CV rendering occupy 10.178 of
10.205 ms/frame spent compiling Chunks. Cache creation takes 0.011 ms, explicit
NW drawing 0.004 ms and list completion 0.001 ms.

A finer probe passes all 295 frames and records 585,756 actual CV calls. Inside
those calls, minimum brightness runs 4,281,612 times and occupies 4.725 ms/frame;
opaque checks run 3,514,536 times and occupy 2.460 ms/frame. Existing failure-only
Block reads run 7,029,072 times and occupy 0.626 ms/frame. EW observations are
gated to the actual DK-to-CV call.

These inclusive host spans come from instrumented programs. Clocks, wrappers,
counters and prefaulted observation storage affect deadline work. No overhead
is subtracted; these are not FPS comparisons or GPU timers. Explicit NW counts
exclude automatic flushes. Rejected header admission and overly strict
post-link audits remain separate retained attempts.

## Fixed-work CPU comparison

The [Windows receipt](../../tests/benchmarks/chunk_cache_render_windows.json)
retains eight fresh processes in public/internal, internal/public,
public/internal, internal/public order. Each process runs seven samples for
each of six getters: 336 samples total, with 65,536 calls across 1,024 points
per sample. Both executables use one compiled workload object and typed
function-pointer dispatch over a genuine nine-Chunk leased EW.

| Getter | Public median ns/call | Internal median ns/call | Reduction |
| --- | ---: | ---: | ---: |
| Brightness | 334.171 | 204.162 | 38.905% |
| Minimum brightness | 333.337 | 205.561 | 38.332% |
| Material | 169.915 | 51.178 | 69.880% |
| Opaque | 172.246 | 61.120 | 64.516% |
| Normal cube | 167.396 | 59.422 | 64.502% |
| Biome manager | 118.960 | 14.219 | 88.047% |

Reductions use unrounded observations; all samples remain included. Each process
passes 4,238,612 native checks. Authoring, assertions, normalization, hashes,
snapshots, fault controls and cleanup are outside timing. Float bits, canonical
Material identities, manager identity and complete failure records are checked.
Public success records and untouched internal sentinels are checked separately.
World/Chunk owners, planes, leases, Dimension/manager state, provider counters
and Random remain guarded. The real brightness flag effect is observed before
the next explicit sample precondition.

The complete record is 7,352 bytes; its scalar/frame prefix is 200 bytes and
Registry record is 96 bytes. These are layout sizes, not measured memory traffic.

## Native frame comparison

The separate uninstrumented six-process comparison is **FAIL**. It preserves
the scene, settings, two-second warmup and ten-second measurement:

| Round | Build | Admission | Frames | Focus-invalid frames | World returns | World-time steps | Ends paused |
| --- | --- | --- | ---: | ---: | ---: | ---: | ---: |
| 0 | Before | FAIL | 302 | 185 | 196 | 91 | 1 |
| 0 | After | PASS | 372 | 0 | 192 | 192 | 0 |
| 1 | After | PASS | 399 | 0 | 192 | 192 | 0 |
| 1 | Before | FAIL | 295 | 81 | 192 | 148 | 1 |
| 2 | Before | PASS | 331 | 0 | 193 | 193 | 0 |
| 2 | After | PASS | 397 | 0 | 192 | 192 | 0 |

Both failed baseline intervals lose native foreground validity, end paused and
advance fewer World-time steps than coordinator returns. Products, selected
inputs and assets remain unchanged. No failed attempt is replaced or discarded;
there is no aggregate, passing-subset speedup or FPS improvement claim. The
earlier three-process baseline also remains failed. Foreground cause and general
native focus reliability remain unqualified.

## Correctness and reproduction

The existing 950 original-reference EW rows remain unchanged. Current native
replay passes 8,470 checks, including six-getter controls for exact float bits,
negative zero and NaN payloads, nullable identities, native refusals, nested
neighbor failures, Registry state, malformed backing and operand order. A
deliberate test-call alias to public success diagnostics preserves all 950 rows
but fails the internal-success sentinel. Its first build was rejected before
linking because external headers were not predeclared; a new attempt pins those
headers before compilation. Both attempts remain retained.

Windows passes 22 focused tests in 39.00 CTest seconds with 5,058 guarded inputs
unchanged. Linux Clang Debug passes 20 in 105.50 seconds with ASan/UBSan, leak
detection, halt-on-error, Xvfb/software GL and 4,001 unchanged inputs. Selections
cover actual Chunk compilation, flat/AO faces, Fluid, Lever scene geometry,
Redstone paths and benchmark controls. Windows also exercises the saved Lever
input loop. These finite checks do not extend original whole-frame pixels,
unrestricted gameplay, Linux loaded play or FPS.

Configure optional Release benchmarks, then run the complete pair. On Windows,
use a matching MSVC developer shell:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DMC_BUILD_BENCHMARKS=ON
cmake --build build --target mc_benchmark_chunk_cache_render_public mc_benchmark_chunk_cache_render_failure
python -B tests/tools/world/benchmark_chunk_cache_render.py --executable public=build/mc_benchmark_chunk_cache_render_public.exe --executable failure=build/mc_benchmark_chunk_cache_render_failure.exe --mode controls --rounds 1 --artifacts .local/render-controls
python -B tests/tools/world/benchmark_chunk_cache_render.py --executable public=build/mc_benchmark_chunk_cache_render_public.exe --executable failure=build/mc_benchmark_chunk_cache_render_failure.exe --mode measure --rounds 4 --artifacts .local/render-cpu
```

On Linux, omit `.exe`; the retained Linux run supplies sanitizer checks, not
timing samples. The driver retains every attempt and rejects the complete pair
if any process, work identity, guard or cleanup fails. The tools test audits
malformed receipts; the optional native benchmark-controls test runs both
adapters without timing samples. Execution hashes establish continuity; the
receipt records separate shared-object and library build bindings.

## Wrapped Block render coordinates

Flat lighting and ambient occlusion compute the center and its two wrapped
neighbors on each axis once per Block. Side, opacity and brightness operations
still reach their live receivers in source order. Chunk compilation also
reuses the Registry success record from pass zero for ready `uu.m` reads.
The full lookup still supplies diagnostics on its failure path.

Two profiled pairs on DISPLAY2 used actual forward input in the saved Far
scene at 854×480. Across all four processes, 1,026 meshes matched on renderer
position, pass cell and Block ID sequence, render and query counts, and held
Chunk snapshots. They included 2,281,472 ambient-occlusion selections per
process. Full mesh spans totaled 4,161–4,163 ms before and 3,942–3,982 ms
after, a median reduction of 4.8%. The slowest three-mesh burst in each process
compiled the same three fully occupied sections. Median compile time for those bursts
fell from 22.99 to 21.56 ms. These are inclusive CPU spans from an instrumented
build. The timed scalar samples within them overlap.

Four separate uninstrumented pairs used the same moving route. Each cohort
matched input, player, actor, random and scheduler state over 215 World ticks.
The dirty queue fell further in all four candidate runs. Its median net
decrease over ten seconds went from 1,031 to 1,151 entries. The queue count
includes renderer invalidation and completion through the original update
schedule.

| Uninstrumented workload | Before FPS | After FPS | Before p99 (ms) | After p99 (ms) |
| --- | ---: | ---: | ---: | ---: |
| Moving, four pairs | 67.12 | 65.98 | 21.64 | 22.64 |
| Heavy cave, one pair | 54.73 | 55.38 | 31.51 | 30.35 |
| Lighter cave, one pair | 63.91 | 62.85 | 24.19 | 26.57 |

The qualifying cave pairs each matched actor, random and scheduler state over
200 World ticks. An earlier lighter candidate interval lost foreground focus
and stayed paused. Its measurements are excluded from the table and the
failed interval remains in the local receipts.

Seventeen focused tests passed on each host, using Windows MSVC Release and
Linux Clang Debug with ASan and UBSan. Checks include signed Java wraparound
at both endpoints, 48 ambient-occlusion and 25 flat-lighting field and vertex
rows, 1,680 face rows, 950 ChunkCache rows, and 18,938 Chunk compilation
assertions, including its retained failure paths.

## Ambient-occlusion setting reads

The Block renderer's `Minecraft.v()` query reads the live published App and
the live ambient-occlusion bit. Both reads of `Minecraft.a` still acquire the
namespace mutex separately. Active use, temporary ownership and failure
diagnostics follow the original call order. The query copies the App pointer
and Settings bit directly, avoiding the full inspection snapshots.

Four alternating Windows MSVC Release pairs ran the same query fixture, with
seven timed samples of 250,000 queries per process. Median time fell from
159.87 to 137.50 ns per query, a 14.0% reduction. Each timed call returned the
expected bit and completed at source PC 23. All seven original fixture rows
matched, including a replaced App, a null Settings receiver and a cold namespace.

Two profiled moving pairs put the same three compiled units in the same link
order for both builds. Across all four processes, 1,076 meshes matched on
position, Block sequence, operation counts and held Chunk snapshots. They
included 2,383,872 ambient-occlusion queries per process. Median total mesh
time fell from 4,122.81 to 4,049.76 ms, a 1.8% reduction. An earlier comparison
against the retained baseline binary showed a 2.8% reduction across 1,067
matching meshes. In the same-link comparison, the median longest three-mesh
compile span was 21.01 ms before and 21.41 ms after.

The Windows game windows ran at 854×480 on DISPLAY2 with Far render distance.
Each moving cohort matched input, player, actor, random and scheduler state
over 215 World ticks. The two fixed-camera scenes matched actor, random and
scheduler state over 199–200 shared ticks per scene. Uninstrumented frame
results varied by pair and workload.

| Uninstrumented workload | Before FPS | After FPS | Before p99 (ms) | After p99 (ms) |
| --- | ---: | ---: | ---: | ---: |
| Moving, same link order, two pairs | 66.49 | 65.57 | 21.21 | 21.39 |
| Moving, retained baseline, two pairs | 66.39 | 66.54 | 23.05 | 22.70 |
| Heavy cave, two pairs | 55.47 | 54.74 | 30.73 | 30.41 |
| Lighter cave, two pairs | 64.53 | 63.84 | 22.51 | 23.65 |

A separate trial reused the Chunk address from the first Block read during
brightness lookup. Both variants increased fixed-work brightness and minimum
brightness time by about 2.5–3.4%. The production change uses only the faster
ambient-occlusion query. New ChunkCache controls verify distinct packed light
values at four adjacent Chunk corners and confirm that positive skylight sets
the Chunk flag before a reached block-light failure.

All 23 focused tests passed on Windows MSVC Release and Linux Clang Debug
with ASan and UBSan. The native ChunkCache replay passed 9,234 checks,
including the 950 original rows. Settings, owner, ambient-occlusion, flat
lighting, face and Chunk compile checks passed in the same selection.

## Registry reads and wrapped render neighbors

With a held ChunkCache, the first ambient-occlusion brightness sample and
first neighbor flag establish the Registry success record for their sequences.
Later initial brightness samples and neighbor flags reuse that record on the
ready path. They still reach the class admission check, read the live `uu.s`
or `uu.r` array and issue the original access call. Cold and refused admission
use the regular initializer and diagnostic path.

Portal bounds and side checks and the Grass upper-neighbor texture lookup
use signed Java wraparound for adjacent coordinates. Tests force both signed
endpoints and verify the coordinates and order of every reached callback,
including short-circuit cases. The ambient-occlusion tests replace the live
flag array between reads and exercise later null receivers, null brightness
arrays and Registry allocation overflow.

The Windows Release builds used the same link order. In two profiled moving
pairs on DISPLAY2, 1,095 meshes matched on position, Block sequence, operation
counts and held Chunk snapshots. Each process made 2,424,832 matching
ambient-occlusion selections. Median total CPU compile time fell from
4,114.52 to 4,047.40 ms, a 1.6% reduction. Median compile time for the longest
three-mesh burst fell from 22.03 to 20.85 ms. The separately sampled flag
span fell from 1,232 to 1,101 ms across matched meshes. Sampled spans overlap
other timers. Profiled moving p99 changed from 22.37 to 23.93 ms.

All native windows ran at 854×480 with Far render distance. The uninstrumented
moving pairs matched input and simulation over 214 World ticks. Both cave
pairs matched actor, random and scheduler state over 200 ticks each.

| Uninstrumented workload | Before FPS | After FPS | Before p99 (ms) | After p99 (ms) |
| --- | ---: | ---: | ---: | ---: |
| Moving, two pairs | 65.68 | 65.34 | 21.81 | 20.59 |
| Heavy cave, one pair | 54.18 | 55.45 | 31.18 | 29.77 |
| Lighter cave, one pair | 63.89 | 62.38 | 23.29 | 23.56 |

A separate trial deferred Tessellator cleanup diagnostic initialization.
Its isolated cloud pass took longer in both profiled pairs, with median time
moving from 2.26 to 2.32 ms. The trial was dropped.

All 22 focused tests passed on Windows MSVC Release and Linux Clang Debug
with ASan and UBSan. They include 843 CPU mesher checks, 48 original
ambient-occlusion rows, 1,680 original face rows, 950 original ChunkCache
rows, 18,938 Chunk compilation checks and graphics ownership controls.
