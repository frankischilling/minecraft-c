# World query performance

Successful scalar World reads no longer construct the complete acquisition
diagnostic tree. A private failure-only getter performs the same single lookup
and preserves its output on success. Failure still supplies the complete
receipt. Public detailed getters retain their success-reset behavior.
Light propagation materializes its canonical success receipt once per operation,
preserving the distinct earlier unreached state.

The source-owned cache also handles synchronous nested reads during a mutation
scope. Its index can be stale there, so the getter scans the current bounded
map in ordinary lookup order. It preserves generic-call accounting and checks
the requested Chunk and context. A miss follows the original coordinator.
Raw descriptor export permanently disables this optimization. Caller-owned
caches retain full validation; the index rebuilds only after the outer scope ends.

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
```

On Windows, use the `.exe` target path. Repeat `--executable label=path` to
compare candidates. The runner retains raw samples, executable/source/input
hashes, checksums and per-operation distributions. It creates its own resources
and saves; Java, original archives, imported assets and a display are unnecessary.
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
