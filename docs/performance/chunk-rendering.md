# Chunk rendering and EW diagnostics

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
