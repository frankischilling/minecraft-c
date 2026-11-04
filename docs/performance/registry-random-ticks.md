# Registry reads during random ticks

The World random-tick scheduler uses private failure-only accessors for the
Registry's captured `Block.n` array. Successful reads leave the caller's diagnostic
untouched. The existing and new accessors share class active use, owner admission,
the captured array identity, signed bounds checks and complete failure records.
Each reached capture and read still occurs; the array is not cached across samples.

The later [static-name and cache validation measurements](world-queries.md#block-static-name-selection-and-cache-validation)
cover different operations and frozen builds. Their percentages cannot be
combined with the captured-array results here. The retained Linux phase
diagnosis and Source progress ownership change also keep separate scopes.

The [Windows receipt](../../tests/benchmarks/world_registry_tick_windows.json)
measures `mc_world_runtime_random_tick_begin` on genuine GAME Worlds with a Player,
Math and Registry owners, and 361 distinct loaded saved Chunks. Each completed call
performs all 28,880 random block samples. Air starts with an active ambient
cooldown; stone reaches ambient checks. Both timed workloads have zero enabled
Block ticks. Separate native controls exercise enabled behavior and failures.

The corrected MSVC Release comparison has a three-round initial cohort and a
separate six-round confirmation. Each variant/workload has 42 initial and 84
confirmation timings. Every fresh process retains two warmup rows and fourteen
timed rows; the variant that runs first alternates by round and workload.

| Clean cohort/workload | Baseline median ms | Candidate median ms | Baseline mean ms | Candidate mean ms | Baseline p95 ms | Candidate p95 ms |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Initial air | 4.25240 | 3.28305 | 4.28609 | 3.30944 | 4.8574 | 3.5714 |
| Initial stone | 4.06280 | 3.30385 | 4.09648 | 3.33017 | 4.4881 | 3.6932 |
| Confirmation air | 4.17300 | 3.29320 | 4.23263 | 3.34867 | 4.6669 | 3.9145 |
| Confirmation stone | 4.11840 | 3.13505 | 4.15872 | 3.22353 | 4.6363 | 3.8869 |

Confirmation median time falls by 21.08% for air and 23.88% for stone. The receipt
retains the less favorable stone maximum: candidate 5.0429 ms versus baseline
5.0152 ms. It also retains every process, sample, order stratum and outlier. These
are finite CPU measurements of the declared operation; they establish no scene
FPS, full `World.tick`, natural-terrain throughput or unrestricted-play result.
Their percentages cannot be added to the earlier raw-Chunk accessor results.

The timer brackets the actual random-tick begin call. Saved-Chunk loading, the
independent scalar oracle, inspection, outer call retirement, final byte/identity
checks and teardown are outside that interval. Warmup calls have their own raw
timings and are excluded from the reported distribution. Retirement timings are
retained separately. The observer does no per-Block internal timing.

Both executables use one byte-identical observer object and the same ordered
library tail containing 31 frozen archives. Both explicitly link the
corresponding Registry `runtime.c` and World `world_tick_random.c` objects before
that tail. Baseline objects come from C commit
`31c803e2fffe8ef1f7a5b4565d0558b3f1d0fff4`; the candidate replaces exactly those two
objects. The receipt binds their SHA256 identities, the observer, executables,
archives, compiler options, 182 captured project compilation inputs and 24
compiler/SDK headers. It describes the captured inputs, without asserting that
future repository edits have identical bytes or measured performance.

The first pair's compile commands accidentally included an extra Python
executable argument. MSVC emitted D9024 and D9027 and ignored that argument. Clean
compilation removed it and produced different object hashes, so those earlier
three- and six-round cohorts receive no final performance credit. Their complete
rows, summaries, controls and product bindings remain under `warned_preliminary`.
The new `clean_primary` pair supplies every result in the table. Across both
captures the receipt retains 72 processes, 1,152 raw rows including warmups, and
1,008 timed samples. Initial and confirmation cohorts stay separate throughout.

All eight captured control runs pass 15 profiles and 89,842 checks each. Their
complete native transcripts, actual observation rows and output hashes match.
Each run retains all 75 binary observations, including byte counts, plus hashes
for the native text, stdout, stderr and `actual.json`. These cover callback
arguments, weather writes, aliases, RNG/LCG state, complete failure prefixes and
captured-Chunk ownership. Before/after input inventories also match. Check counts
alone are not used as evidence of equivalence.

The controls prove random-call and captured-Chunk retirement with the null raw
array, null Random and enabled-null Block receiver still present. Whole-Store
admission refuses null Block bytes and preserves the graph; the final fixture
teardown restores its retained storage before disposal. Unrepaired whole-Store
terminal closure with null Block bytes remains outside this qualification. The
control receipts have `original_comparison: null`; this is a comparison against
the frozen C baseline and adds no new Java/original-game parity observation.

The public JSON projects absolute paths into repository identities and command
roles. Native text uses LF; captured stdout and `actual.json` use Windows CRLF.
The auditor reconstructs those text bindings, pairs the full control observations
and hashes, and invokes the existing `audit_cohort` scalar parser for every raw
timing row. It checks source Random/LCG progression, completed work, process order,
all statistics and order strata. Pinned canonical projection digests detect
coordinated edits to the published rows, summaries or capture bindings. Hashes
identify retained captures; the JSON does not reconstruct their private binaries.

Run the public receipt audit and corruption regressions without a native build:

```sh
python -B tests/tools/world/test_registry_tick_benchmark.py
python -B tests/tools/world/test_registry_tick_benchmark.py --receipt tests/benchmarks/world_registry_tick_windows.json
```

The optional current-source workload and its native controls can be built with:

```sh
cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DMC_BUILD_BENCHMARKS=ON
cmake --build build-release --target mc_benchmark_world_random_tick
python -B tests/tools/world/test_random_tick.py --executable BENCHMARK_EXE --work-dir NEW_CONTROL_DIRECTORY
```

For a new controlled pair, preserve the actual Ninja compiler/link commands and
freeze the baseline archives and observer once. Extract the two baseline runtime
objects from those archives. Compile only the candidate Registry and World
runtime objects with their respective actual flags and captured input closure;
the World target includes `-experimental:c11atomics`. Link each pair's two explicit
objects after the shared observer and before the identical complete library tail.
Preserve its order, duplicates, system libraries and link flags. Check the link
maps and all product hashes. Empty command fields must stay absent; do not parse
an empty field as a command line. Compiler-driver warnings require correction and
a fresh retained comparison. Separate full builds qualify as the same closure
only after their observer and archive identities have been checked.

Run semantic controls before timing, keep builds and other benchmarks idle, and
give every attempt a new artifact directory. Collect independent initial and
confirmation cohorts with the existing runner:

```sh
python -B tests/tools/world/benchmark_random_tick.py --executable baseline=BASELINE_EXE --executable candidate=CANDIDATE_EXE --mode compare --rounds 3 --artifacts NEW_INITIAL_DIRECTORY
python -B tests/tools/world/benchmark_random_tick.py --executable baseline=BASELINE_EXE --executable candidate=CANDIDATE_EXE --mode compare --rounds 6 --artifacts NEW_CONFIRMATION_DIRECTORY
```

The runner defaults to controls only unless `--mode compare` is supplied. Preserve
failed preparations and all native attempts. New executable or source projections
need their own control qualification; they do not replace the captured timings.
