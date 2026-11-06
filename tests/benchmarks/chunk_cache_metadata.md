# EW metadata CPU comparison

The public `mc_client_chunk_cache_metadata` accessor keeps its complete success
and failure diagnostic record. The internal failure-only accessor executes the
same admission and Source `ew.e(III)I` read, leaves the caller's error bytes
untouched on success, and materializes the complete canonical record on failure.
Only the EW branch of CV metadata access uses that internal contract. Direct
World access and the existing Block helper retain their contracts.

The shared body preserves Y checks, signed array indices, blank-Chunk behavior,
the actual `object->e` nibble read, output preservation, active-state transitions
and leases. Metadata does not read `object->data`. Missing native occurrence or
nonblank object storage remains outside the existing native-owner contract.

## Retained Windows qualification

The [CPU receipt](chunk_cache_metadata_windows.json) retains the existing 950
unchanged original-reference rows and 3,541 native checks. Its 26 CPU control
rows cover admission, busy/retiring owners, Y and array bounds, nibble/backing
failures, blank/data independence, odd/even nibbles and failed construction.
These new native profiles do not extend the original-reference scope.

Four alternating rounds retain eight fresh processes and all 56 samples,
28 per accessor. Both variants use one compiled workload and identical function
pointer dispatch over a genuine nine-Chunk leased EW. Each sample performs
65,536 actual reads across 1,024 varied coordinate points. Authored metadata,
controls, assertions, hashes and cleanup stay outside timing. Values, full
failure records, untouched success sentinels, World/owner/lease state, Random
and zero provider acquisitions are checked separately.

| Accessor | Median ms / 65,536 calls | Median ns / call |
| --- | ---: | ---: |
| Public complete record | 9.06405 | 138.3064 |
| Internal failure-only record | 1.08805 | 16.6023 |

The median reduction is 87.995984%, calculated from the unrounded observations.
This is a finite getter CPU result. The captured diagnostic record is 7,352
bytes; initializing and copying it gives a nominal 14,704 bytes per public call.
That record-size calculation is not measured DRAM traffic. It does not identify
the executed store count, provider cost, GPU time or scene FPS.

The receipt pins the historical producer independently of later public tooling.
The promoted runtime bodies match the measured candidate. The retained MSVC
samples remain the timing evidence; fresh controls below add no timed samples.

## Current public validation

Windows passes 684/685 public CTests in 247.42 seconds (255.9969 seconds outer).
Its sole failure is `client.world_selection_animals`: the Pig profile records
Source focus 0, paused 1 and display-active 0 at 77 frames/35 ticks, with
`msrdc.exe` / `TscShellContainerClass` recorded as foreground. This is a focus
refusal, distinct from the historical frustum-culling failure. Three passive
input-timestamp changes do not establish a human or cause. There is no current
full Windows pass.

Linux Clang 18 Debug passes 674/674 public CTests in 147.72 seconds
(268.4867 seconds outer), using ASan/UBSan, leak detection, halt-on-error and
UBSan stack traces with Xvfb/software GL. Each cohort preserves 4,767 source
inputs, 638 products and 413 assets, with no drift or observer errors.

Both metadata CPU executables freshly pass 6,342 control checks each on each
host, with zero timing samples. `client.chunk_cache` and `client.fluid` pass
on both hosts, as do the frame-observation (43 methods), static-CPU (18) and
native-packet (71) audits. These controls do not extend the 87.995984% timing
result to Linux, frame rate or original-game parity. Earlier full-suite results
and failures remain separate historical evidence.

## Current Workbench shutdown qualification

Four current Windows SDK profiles pass 7,446 checks in 29.83 CTest seconds:

| Profile | Native checks | CTest seconds |
| --- | ---: | ---: |
| Clean | 1,679 | 7.57 |
| Container | 1,677 | 7.31 |
| Player | 2,064 | 7.33 |
| World | 2,026 | 7.59 |

All 1,979 selected inputs (1,450 runtime files, 527 harness files and two
binaries) and 413 assets remain unchanged. The pre-launch guard records zero
last-input timestamp changes; the passive monitor records one. These observations
do not identify a human or establish an input-free desktop.

The fixture now follows the documented bootstrap completion protocol: retry
only BUSY, retain the same nonnull owner, and use the normal client's
35,000-attempt bound and 1 ms delay. Controlled menu/ownership refusals remain
one-shot; window-absence and stopped-disk assertions remain intact. The helper
supplies no Source frame, input, clock, getter or World progress.

The earlier one-call failure reached startup resource cleanup and returned
BUSY; its exact child remains undistinguished. All four current join receipts
record one call and zero BUSY returns. Three separate clean regressions pass
1,677, 1,797 and 1,619 checks, also with one call and zero BUSY returns.
A natural BUSY-to-OK transition remains unobserved. These native qualification
times are not performance measurements and do not qualify full public suites.

## Reproduce

Configure matching Release binaries with optional benchmarks enabled:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DMC_BUILD_BENCHMARKS=ON
cmake --build build --target mc_benchmark_chunk_cache_metadata_public mc_benchmark_chunk_cache_metadata_failure
python -B tests/tools/world/benchmark_chunk_cache_metadata.py --executable public=build/mc_benchmark_chunk_cache_metadata_public --executable failure=build/mc_benchmark_chunk_cache_metadata_failure --mode controls --rounds 1 --artifacts .local/metadata-cpu
python -B tests/tools/world/benchmark_chunk_cache_metadata.py --executable public=build/mc_benchmark_chunk_cache_metadata_public --executable failure=build/mc_benchmark_chunk_cache_metadata_failure --mode measure --rounds 4 --artifacts .local/metadata-cpu
```

Append `.exe` on Windows. The two executables share one workload OBJECT target,
the ordinary Source-World dependencies and strict compiler settings. Only these
CPU targets omit the unused Shell32 delay-load option; client defaults remain.
The driver reuses the existing authored save and independent NBT decoder,
retains every attempt and sample, and checks source/executable/input continuity.
It needs no display, imported assets, original archive or JVM.

## Separate failed frame cohort

The [EW frame receipt](chunk_cache_metadata_frames_windows.json) retains all
eight attempts from four alternating rounds. Five pass individual admission;
three lose native foreground validity and end paused. Those failures retain
392, 253 and 117 focus-invalid frames, with World-time steps of 0, 60 and 129
against 195, 192 and 194 enclosing World coordinator returns. Random-Chunk and
random-display calls follow the smaller simulation counts.

The first paused baseline reports 39.18 raw FPS with zero World-time steps.
It therefore receives no simulation-backed comparison credit. Source, product
and asset guards pass, but the whole cohort remains FAIL. The foreground-loss
cause is not established. No aggregate or passing-subset FPS gain is claimed,
and no retry is performed.

The earlier [eight-profile work-ledger cohort](client_frame_work_qualification_windows.json)
remains a separate failed cohort with seven individually admitted intervals.
Neither cohort changes the fixed-work CPU result or establishes original-game
timing, pixels or unrestricted-play parity.

Replay both retained receipts without a native process or assets:

```sh
python -B tests/tools/benchmarks/test_chunk_cache_metadata.py
python -B tests/tools/benchmarks/test_chunk_cache_metadata_frames.py
```
