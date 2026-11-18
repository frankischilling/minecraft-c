# Chunk rendering and EW diagnostics

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
control varied by 11–14%. The cause of the slower material results remains
undetermined.

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
