The optional Light-drain benchmark measures one complete public
`mc_world_runtime_light_drain` call on a constructed Source World. It checks
work and owner continuity before accepting a timing sample. It has no FPS
threshold and does not open a display.

Build it with the same Release compiler/configuration for each variant:

```sh
cmake -S . -B build-release -DBUILD_TESTING=ON -DMC_BUILD_BENCHMARKS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-release --config Release --target mc_benchmark_light_drain
python -B tests/tools/world/benchmark_light_drain.py --executable current=PATH_TO_EXECUTABLE --rounds 3 --artifacts .local/light-drain-benchmark
```

Pass two uniquely named `--executable` arguments to compare variants. The runner
alternates process order between rounds and retains all seven samples per
process, stdout/stderr, authored input hashes, semantic receipts and summary
statistics. Use matching benchmark code in both variants; a baseline predating
the harness needs the harness and optional target registration added without
runtime changes. A shared compiled benchmark object can link both sets of
runtime libraries. Executable/object hashes identify the retained products;
they are not cross-host timing or reproducible-build requirements.

The runner authors nine real saved Chunks, loads them through the ordinary
Source World constructor/getter, then submits 500 distinct SKY regions with
coalescing disabled. Each region covers eight air cells with saved sky15 and
height0. One measured drain processes the original predecremented Source budget
of 499 regions: 3,992 cells and 37,924 successful GETs, leaving one queued region.
Submissions, hashing, assertions, output, final queue retirement and file work
are outside the interval. No timed cell changes its stored value. The measured
query path uses public operations and never calls the new private canonical
query helper directly.

Each sample checks ordered queue bounds, every loaded Chunk plane/state,
listener calls, Random/profile state, live holds, allocation accounting and
cache counters. Additional untimed controls use actual setter callbacks to
observe a completed value15 store before removing its nullable Nibble, and to
export the live provider and force subsequent generic reads. A later native
work-budget failure must retain a canonical successful query child. The runner
requires matching operation/failure/value receipts across all processes and
unchanged authored files before reporting PASS.

[The retained Windows record](light_drain_windows.json) contains the actual
42 raw samples for the promoted shared-core implementation over the frozen
`f44895cb7a0b3ee2ba4287a366d0e302843b7ca9` closure. Only its query/propagation
objects changed; other later gameplay changes are excluded. Original propagation
controls also passed 1,479 checks per variant, and the updated query controls
passed 6,301 checks. The separate discarded prototype is excluded from this
record. Read its limits before using the finite CPU result; a scene FPS effect
requires a fully qualified frame comparison.
