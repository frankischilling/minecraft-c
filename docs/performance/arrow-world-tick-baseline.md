# Arrow World tick baseline

The optional [benchmark module](../../cmake/arrow_world_tick_baseline.cmake), enabled
by `MC_BUILD_BENCHMARKS` and `BUILD_TESTING`, builds
`mc_benchmark_arrow_world_tick_baseline` from one workload OBJECT and the real
simulation/resource/persistence/Math libraries. Its
[C workload](../../tests/benchmarks/arrow_world_tick_baseline.c) accepts:

```text
mc_benchmark_arrow_world_tick_baseline CELL FRESH_ROOT_UTF8_HEX FRESH_JSONL_PATH
```

`CELL` is `empty`, `arrow1` or `arrow8`. Supply a fresh empty input directory,
encoded as UTF-8 hexadecimal, and an unused JSONL path. Each process constructs
one genuine World with 30 stored air Chunks at x=-2..2, z=-2..3. The same rectangle
covers the actual inclusive 32-block presence queries for all eight planned lanes.
Real Arrow constructors, heading and World insertion use x=8.5, y=66,
z=0.5+4*i, speed 0.6 and spread 0. Constructors retain their real Random draws.
Loading, presence checks, preallocation, both observations, reporting and checked
retirement occur outside the clock pair. The interval encloses eight actual
`mc_world_runtime_tick_entities` (`FD.g`) calls and their loop/status bookkeeping.

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
These are single cold qualification intervals. A cohort with four processes,
two warmups and 21 measured samples per cell has not run. No allocation-cost
attribution, bottleneck or FPS result follows from these values. The historical
140,088-byte execution-frame `sizeof` is a storage fact, not a measured cost.

The first strict build stopped at C4133 because the test observer supplied raw
provider fields to `mc_chunk_provider_cache_observe`. The corrected observation
passes the World's actual native provider, `w->fields.v`; it adds no cast or
Source-state repair. The original failed build is retained separately. Private
build evidence binds the actual workload object, ordered libraries, link map
and import closure. It also records `vcruntime140.dll` and
`vcruntime140_threads.dll` from their installed MSVC 14.51.36231 compiler-directory
owners and the exact bytes staged into `build-windows` for these runs.
