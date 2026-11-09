# Arrow World tick baseline

The optional [benchmark module](../../cmake/arrow_world_tick_baseline.cmake), enabled
by `MC_BUILD_BENCHMARKS` and `BUILD_TESTING`, builds
`mc_benchmark_arrow_world_tick_baseline` from one workload OBJECT and the real
simulation/resource/persistence/Math libraries. Its
[C workload](../../tests/benchmarks/arrow_world_tick_baseline.c) accepts:

```text
mc_benchmark_arrow_world_tick_baseline CELL FRESH_ROOT_UTF8_HEX FRESH_JSONL_PATH
mc_benchmark_arrow_world_tick_baseline CELL FRESH_PARENT_UTF8_HEX FRESH_JSONL_PATH --cohort
```

`CELL` is `empty`, `arrow1` or `arrow8`. Supply a fresh empty input directory,
encoded as UTF-8 hexadecimal, and an unused JSONL path. Each interval constructs
one genuine World with 30 stored air Chunks at x=-2..2, z=-2..3. The same rectangle
covers the actual inclusive 32-block presence queries for all eight planned lanes.
Real Arrow constructors, heading and World insertion use x=8.5, y=66,
z=0.5+4*i, speed 0.6 and spread 0. Constructors retain their real Random draws.
Loading, presence checks, preallocation, both observations, reporting and checked
retirement occur outside the clock pair. The interval encloses eight actual
`mc_world_runtime_tick_entities` (`FD.g`) calls and their loop/status bookkeeping.

## Repeated fresh intervals

The original CLI runs one interval. With final `--cohort`, the decoded root is a
parent containing 23 independently fresh empty directories, `interval-00` through
`interval-22`, prepared by the caller before launch. The output path must not
exist. One process runs one cell: indices 0..1 are warmups and 2..22 are measured
intervals. Each interval creates and retires its own actual World/classes/Math
and all 30 Chunks; Source state and Random are not reset to reuse a fixture.
All warmup and measured records remain retained. The external cohort starts four
independent processes per cell, twelve serial processes across the three cells.

The schema2 process envelope labels each full schema1 group with its index,
`warmup` or `measure`, and the ordinal within that phase. Complete observations,
constructor Random, actual World/error returns and checked cleanup remain in
each group. Its check count is a per-interval delta; the process footer retains
the cumulative total. Borrowed native context/Chunk pointers are detached only
after full checked cleanup succeeds. A failed setup, call, validation, write,
flush or retirement stops before another fixture; completed records are flushed
before the next interval. Context addresses and raw bytes remain process-local.

Before the first fixture, the process records 64 consecutive actual clock pairs
with raw integer start, end, frequency and return status. These separate clock
observations are outside every World interval and are never subtracted from
its values. The measured unit remains the entire eight `FD.g` calls plus their
loop/status bookkeeping. It is not one Arrow dispatch or one allocation.

The selected repeated-run profile is Windows x64 on an Intel Core i9-10900F,
using strict MSVC Release. This is a local CPU workload, not cross-platform
performance or an App/frame/FPS result. Allocator observation, cost attribution,
a bottleneck diagnosis and an optimization result remain separate work.

All twelve fresh processes exited zero with complete, reaped captures, empty
stderr and checked cleanup. Each passed the exact full-record reader and
unchanged source/product guards before and after execution. The independent
retained-data audit replays all 5,760 JSONL rows: 276 intervals (24 warmups and
252 measured), 2,208 OK World returns, 437,184 checks, 828 constructor records
with 2,484 actual Gaussian calls, 552 snapshots and 16,560 Chunk records.
No cohort attempt failed or was retried. The earlier qualification failures
remain separate historical evidence.

The table shows each process's median of 21 measured intervals, in milliseconds
for the entire eight-call interval. Each process is an independent unit;
its 21 samples are repeated measurements within that process.

| Cell | Process 0 | Process 1 | Process 2 | Process 3 |
| --- | ---: | ---: | ---: | ---: |
| empty | 0.4001 | 0.3640 | 0.3616 | 0.3614 |
| arrow1 | 1.6043 | 1.3777 | 1.4271 | 1.3903 |
| arrow8 | 8.1914 | 8.2700 | 8.0175 | 8.2398 |

Across all measured intervals, observed ranges were 0.2889–0.5652 ms (`empty`),
1.2838–2.0654 ms (`arrow1`) and 7.5517–11.9482 ms (`arrow8`). QPC frequency was
10,000,000 ticks/second throughout. All 768 separate clock-pair deltas were zero
or one QPC tick; their medians were zero at this clock resolution. Raw clock
operands, every warmup and every measured observation remain retained. No clock
pair or empty-cell value was subtracted from the measured intervals.

The host ran Windows 10 Home build 19045, using MSVC 14.51.36231 x64 Release,
C17, `/O2`, `/W4`, `/WX` and `/fp:strict`, on the i9-10900F (10 cores, 20 logical
processors). Power/thermal conditions and scheduler affinity were not controlled.
The qualification source seal is SHA-256
`2d56bae51d2a4aec009cd042070bf5163d77adf64f96c48277ce9ea1068bebeb`.
The actual workload object, ordered production archives, link map and static
import closure were checked; this is not a loaded-module census. Eighteen
affected Windows regressions also pass in 26.42 seconds, without skips.

This establishes a repeated baseline for the specified workload and machine.
It supplies no allocator event count, allocation-cost attribution, optimized
comparison, application frame rate or Linux benchmark qualification.

## Initial cold qualification (historical)

The first Windows x64 strict MSVC Release qualification completed one fresh
process and one interval for each cell. All three returned eight OK World
statuses, passed the exact JSONL reader, completed ordinary checked cleanup and
exited zero with complete, reaped captures and empty stderr. The JSONL retains
both complete schema observations: represented poses and Arrow fields, all 30
Chunk records, constructor/heading Random records, World/profile Random and
Gaussian caches, ownership ledgers, provider-cache observation and raw execution
context bytes. `context_raw` is process-local evidence; addresses and padding are
not a portable cross-process identity. The derived dispatch-path proofs are
0, 8 and 64 for the complete validated cells; these are state/path checks, not
observed allocator counts.

The retained QPC deltas for the entire eight-call intervals were 4,062 ticks
(`empty`), 17,870 (`arrow1`) and 87,165 (`arrow8`), at 10,000,000 ticks/second.
These were single cold qualification intervals. At that point, the cohort with
four processes per cell, two warmups and 21 measured intervals per process had
not run. No allocation-cost attribution, bottleneck or FPS result follows from
these values. The historical 140,088-byte execution-frame `sizeof` is a storage
fact, not a measured cost.

The first strict build stopped at C4133 because the test observer supplied raw
provider fields to `mc_chunk_provider_cache_observe`. The corrected observation
passes the World's actual native provider, `w->fields.v`; it adds no cast or
Source-state repair. The original failed build is retained separately. Private
build evidence binds the actual workload object, ordered libraries, link map
and import closure. It also records `vcruntime140.dll` and
`vcruntime140_threads.dll` from their installed MSVC 14.51.36231 compiler-directory
owners and the exact bytes staged into `build-windows` for these runs.
