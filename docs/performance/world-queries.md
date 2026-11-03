# World query performance

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
