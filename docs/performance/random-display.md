# Random display sampling

The real `fd.q(III)V` sampler performs 1,000 samples per call. Each sample
draws six values from the World's current Random, then reads a Block. Positive
IDs select the current Block array and invoke the actual display virtual with
the separate local Random constructed at entry.

The sampler now uses the existing internal failure-only Block getter. That
getter shares the detailed getter's admission and read body. Success leaves
the acquisition diagnostic untouched; failure supplies the complete current
diagnostic, including failures before acquisition. The sampler exposes this
record only after a failed query. A successful sample neither reads it nor
publishes a diagnostic pointer. The public detailed getter keeps its contract.

This removes repeated initialization of an unused acquisition tree. It does
not cache Blocks, Chunks, arrays or Random values, reduce the sample count, or
change particle and sound dispatch.

## Detail storage on Block dispatch

MSVC Release cleared 30,728 bytes of diagnostics at every positive Block
dispatch. The sampler initializes the header, then clears the detail union
once when a child needs it or when an earlier failure needs to expose it.
The public Block call still initializes the complete record. Poisoned-buffer
checks cover success without a child and failures before and inside a child.

Six Windows Release pairs compared this change against eager initialization.
The eager adapter called `mc_world_runtime_block_random_display` at the
sampler's dispatch site. Both adapters used the current failure-only Block
getter and shared dispatcher.
All 114 control profiles passed per adapter, with matching equal-input
results and ordered events. Each row includes 126 measured calls per variant,
each with 1,000 samples and 6,000 World Random draws.

| Workload | Median before / after | p95 before / after |
| --- | ---: | ---: |
| Air | 0.1244 / 0.1203 ms | 0.1350 / 0.1271 ms |
| Stone | 0.6117 / 0.1806 ms | 0.6401 / 0.1980 ms |

## Fixed-work comparison

Configure with `MC_BUILD_BENCHMARKS=ON` and build
`mc_benchmark_random_display_detailed` and
`mc_benchmark_random_display_failure`. Both executables link the same compiled
observer object. The detailed adapter compiles the production sampler with
its three exported function names changed and the one getter call restored
to the preceding detailed getter. The candidate calls the ordinary runtime.
All remaining dependencies come from the same build.

Run the comparison with an artifact root; each invocation retains a fresh
subdirectory:

```sh
python -B tests/tools/world/benchmark_random_display.py \
  --executable detailed=build/mc_benchmark_random_display_detailed \
  --executable failure=build/mc_benchmark_random_display_failure \
  --artifacts .local/random-display-comparison
```

The runner first executes the existing 99 direct profiles and 15 sampler
profiles against each adapter. These controls cover actual callbacks,
late World/Random changes, null fields, failed queries and checked retirement.
Equal-input results, source positions and ordered events compare exactly.
Each sampler's local default entropy remains independent. Its initial/final
trajectory is checked within that call; neither those entropy words nor the
resulting native Redstone Torch particle arguments receive cross-process
equality credit. Complete raw outputs remain in each attempt.

The timed workloads use nine genuine saved Chunks filled with air or stone.
The fixture's explicit provider export selects the generic cache path.
Each process executes two warmups and 21 measured calls per workload; four
alternating rounds produce eight fresh processes and 336 measured intervals.
Every completed call executes all 1,000 samples and 6,000 World Random draws.
Stone also executes all 1,000 actual no-op Block display virtuals.

The timer brackets the actual retained sampler begin call. Construction,
loading, scalar replay, complete Chunk-plane hashes, state assertions, output
and outer retirement are outside that interval. Checks cover final sampled
coordinates, Random trajectories, World fields, Chunk state and all planes,
default-constructor progression, Math state, generic acquisition counts,
ownership and terminal cleanup. Input resources, saved region bytes, runtime
sources, harness files and executables are hashed before and after the run.
All warmups, process order, raw timings and outliers remain in the receipt.

## Windows results

The [retained Windows cohort](../../tests/benchmarks/random_display_windows.json)
passes all eight processes and 336 measured calls. Windows 10 x64 uses the
Intel Core i9-10900F, MSVC toolset 14.51.36231, C17 Release `/O2 /fp:strict` and the
same shared observer object and 31 ordered dependency archives. All 1,563
guarded runtime, harness and product inputs remain unchanged during the
cohort. Each process passes 276,606 native checks. Both separate full-profile
controls pass 72,124 checks over 114 profiles, with identical results for
the fields having equal inputs.

| Workload | Detailed median ms | Failure-only median ms | Median reduction | Detailed p95 ms | Failure-only p95 ms |
| --- | ---: | ---: | ---: | ---: | ---: |
| Air | 0.52895 | 0.31500 | 40.4481% | 0.8544 | 0.3612 |
| Stone | 1.08585 | 0.83810 | 22.8162% | 1.1872 | 0.9269 |

Each row includes all 84 samples per variant. Candidate medians are lower
in all four matched rounds for both workloads, including both execution-order
strata. Air maximum falls from 0.8611 to 0.5403 ms; stone maximum falls from
1.2813 to 1.1594 ms. These observations belong to the captured executable,
object, archive and source hashes. Later builds do not inherit their timings.
The current artifact-root wrapper adds a fresh subdirectory per invocation;
the frozen cohort used its explicitly fresh root directly. The current build
admission also accepts Ninja's read-only compilation database when CMake has not
exported a file, retaining the actual graph, rules, four commands and products.
Linux Clang Debug with ASan/UBSan passes the native controls and both portable
modes in a three-case follow-up; this supplies no Linux timing result. These
wrapper changes supply no replacement timing samples or change to the measured C body.

These are fixed-work CPU comparisons. They establish no whole-frame, GPU,
gameplay-FPS or unrestricted-play result, and add no unchanged-original
observations. Use `--mode controls --rounds 1` for the native controls without
collecting timings.

## Random draws and Block selection

The sampler uses the shared bounded Random implementation with the bound 16
visible to the compiler. It records the reached operation at each coordinate
draw and reloads the current World Random receiver after callbacks. Child
diagnostics start empty after the local Random constructor and are populated
when a sample reports a failure.

For positive Block IDs, the sampler reads the live Registry array directly
when the Registry is ready and idle. The existing two operations handle cold
and refused reads and report their reached failure sites.

Eight alternating Windows Release pairs used one benchmark observer and a
common library tail. Each pair replaced only the sampler object. All
114 native control profiles matched the equal-input results and ordered
events. The 168 timed samples per build and workload each ran 1,000 samples
and 6,000 World Random draws. The trajectories matched.

| Workload | Before median | After median | Before p95 | After p95 |
| --- | ---: | ---: | ---: | ---: |
| Air | 0.1217 ms | 0.0960 ms | 0.1300 ms | 0.1012 ms |
| Stone | 0.1755 ms | 0.1172 ms | 0.1868 ms | 0.1301 ms |

All eight paired medians improved for each workload. The sampler callback
control also checks that the active error reports the current source
position and has empty child fields.

Four alternating native Far pairs at 854×480 used the same observer,
authored assets, save and fixed camera. After a 30-second warmup, each
ten-second interval matched 200 World ticks, 200,000 display samples
and 1.2 million World Random draws. Every observed FPS string reported
zero Chunk updates.

| Median of per-run results | Before | After |
| --- | ---: | ---: |
| FPS | 526.88 | 527.04 |
| Frame interval p99 | 5.650 ms | 5.741 ms |
| Host frame p99 on frames that ran a tick | 8.136 ms | 7.894 ms |
| Longest frame interval | 8.584 ms | 8.455 ms |

FPS rose in three pairs. Overall frame p99 rose in three, while host
frame p99 on tick-bearing frames fell in all four. The longest observed
interval across all runs was 11.47 ms on the candidate.

One instrumented pair timed the sampler body inside the ordinary client
with the same fixture and warmup. Each run captured all 200 calls. Median
sampler time fell from 0.1178 to 0.0734 ms, and p95 fell from 0.1260 to
0.0770 ms. These spans include the timers and start after call allocation
and World admission.
