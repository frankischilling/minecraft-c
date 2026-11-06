The optional random-Chunk benchmark calls the actual client World scheduler's
random-tick child with a real GAME World, Player, registry and 361 distinct saved
Chunks. Every completed call performs all 80 samples per Chunk. It uses authored
air and stone worlds, no display, original assets or JVM.

```sh
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DMC_BUILD_BENCHMARKS=ON
cmake --build build-release --target mc_benchmark_world_random_tick
python -B tests/tools/world/test_random_tick.py --executable BENCHMARK_EXE --work-dir .local/random-tick-controls
python -B tests/tools/world/benchmark_random_tick.py --receipt tests/benchmarks/world_random_tick_windows.json
```

For a new comparison, supply two executables built with identical workload
source, flags and dependency closure. The runner defaults to controls only:

```sh
python -B tests/tools/world/benchmark_random_tick.py --executable baseline=BASELINE_EXE --executable candidate=CANDIDATE_EXE --mode controls --artifacts NEW_DIRECTORY
python -B tests/tools/world/benchmark_random_tick.py --executable baseline=BASELINE_EXE --executable candidate=CANDIDATE_EXE --mode compare --rounds 6 --artifacts ANOTHER_NEW_DIRECTORY
```

The runner authors saved Chunks and executes all 15 existing World profiles before
measurement. Those profiles check normal/Blank aliases, source RNG and order,
actual ambient sound, native sound failure, reentrant candidate mutation,
snow/ice, Lightning, null raw arrays, null Block receivers and null Random.
The existing native/original reference work is documented in the
[random-tick fixture](../reference/world/random_tick/README.md).

Each comparison process loads all 361 Chunks before two warmups and 14 recorded
calls. World.g and World.r continue naturally between calls. A separate scalar
oracle checks every reached draw, LCG update, cooldown and work counter. The
timer brackets only `mc_world_runtime_random_tick_begin`; setup, saved-Chunk
loading, warmup, oracle, inspection, outer retirement, final byte/identity checks
and teardown remain separate. Air begins with an active ambient cooldown; stone
reaches ambient tests. Neither timed workload enters enabled Block virtuals.
Those effects remain covered by the separate controls.

The [Windows receipt](world_random_tick_windows.json) retains an initial
three-round cohort and a separate six-round confirmation. All 36 fresh processes,
576 raw rows including warmups, 504 recorded timings, process order and outliers
remain present. The two measured variants share one observer object and 31
byte-identical archives; the candidate adds exactly two runtime object overrides.
The runtime change preserves the public raw accessor's complete 128-byte success
error and moves complete diagnostic materialization outside the internal
successful raw-read path. Both paths share the same admission and current-array
read body. Capture inspect fields, read counts, failure records and World errors
remain intact. The private caller's error alone stays untouched on success.

| Cohort/workload | Baseline median ms | Candidate median ms | Baseline mean ms | Candidate mean ms | Baseline p95 ms | Candidate p95 ms |
|---|---:|---:|---:|---:|---:|---:|
| Initial air |5.43535|5.11545|5.79623|5.32856|7.4992|6.7411|
| Initial stone |4.99200|4.68580|5.06727|4.76815|5.6243|5.3099|
| Confirmation air |5.06880|4.31255|5.10261|4.40022|5.8014|4.8984|
| Confirmation stone |4.77915|4.32035|4.80397|4.37188|5.1728|4.9637|

Confirmation has 84 samples per variant/workload. Candidate mean and median are
lower in every confirmation round and both process-order strata. Initial air
round 1 has a slightly worse candidate median; confirmation stone has a higher
candidate maximum, 5.6478 ms versus 5.5441 ms. Individual process p95s also sometimes
favor baseline. These retained differences and the changed percentage between
cohorts prevent a universal speedup claim.

The receipt includes all 44 canonical paired-control rows, comparing an
independently linked frozen C baseline against the candidate, plus both complete
World transcripts and hashes for all 76 output files. The 510 private accessor
checks cover complete named error fields, success sentinels, error=NULL,
normal/RawBlank, actual holds, busy/read-limit refusal, NULL data/arrays, short and
rebound arrays, signed packed-index edges and retirement without repairing the
failed array. Absolute read counts are +1 after admission, including read failures,
and +0 after pre-admission refusal. The persistent `world.chunk_tick` tests use
opaque captures and actual owners to exercise the corresponding public/internal
success, failure, current-array, counter and cleanup contract. Both
`world.chunk_tick` and `world.random_tick` pass after promotion.

The promoted public benchmark subsequently passes controls only against the
exact frozen C baseline, with 15 profiles and 89,842 checks per variant. Every
complete native transcript and all 76 output hashes match each other and the
captured controls. The current executable includes the combined production
dependencies. A retained private argument bridge only selects the older baseline
executable's control entry; its executable and workload stay unchanged. The
fixture tool validates its temporary binary streams before removing them on
success and retains their hashes and byte counts. An outer audit initially
expected those files to remain; its failure is preserved and corrected read-only
from the existing receipts, with no native retry. This run qualifies the public
workload/control-entry projection and supplies no replacement timing samples.

Executable/object/source/archive/assembly hashes bind the captured products to
the frozen `55088bf8` C baseline and private candidate. The production capture layout
stays in its C file and the internal header exposes only a prototype. Header
placement, corrected optional-error wording, formatting, added persistent tests
and portable public clock/control entry are separately recorded as untimed
publication changes. The published harness has not supplied replacement timing
samples; the receipt keeps the original measured executable identities. Frozen
baseline here means C, not a new Java observation. The first preparation's strict
map rejection is retained; no native sample was discarded or retried.

The receipt auditor independently recomputes source Random/LCG/work state from
every raw row and recomputes statistics/order strata. Its regressions reject
skipped samples, altered RNG, dropped processes and changed callback/error
records. These finite Windows CPU results do not establish full World.tick,
natural terrain throughput, scene FPS, GPU performance or unrestricted play.
