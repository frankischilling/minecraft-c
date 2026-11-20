# Player ally-alert retirement

The real Player ally-alert helper obtains a fresh Class List of Wolves. Each
List occurrence owns its own native hold, including repeated appearances of
one Wolf. The entered helper also owns its Player and nullable target. Checked
retirement must prove all those holds before releasing anything.

The previous validator counted every occurrence again for every List entry.
A successful check of 1,024 entries therefore performed 1,049,600 checked List
gets. This work belongs to native cleanup; it is separate from the original
query, watcher reads and target assignments.

The current validator keeps that allocation-free scan through eight entries.
Larger Lists count pointer identities in an optional native table, then check
each distinct Entity's total required holds, including Player/target aliases.
Every actual List occurrence still passes its existing checked getter. The
source List, its order and its owning ledger remain untouched. No Entity
`equals` or source hash method runs. Releases begin only after all ownership,
bounds and receipt checks succeed.

Optional scratch is capped at 1 MiB, independently of the World's source
budgets. Allocation or extent refusal uses the former scan. It introduces no
new cleanup refusal, source work, changed budget or replay. Pointer-table
probing has its usual collision-dependent cost; the finite measurements below
cover genuine allocator-produced identities through 1,024 entries.

## Fixed work

Configure `MC_BUILD_BENCHMARKS=ON` and build these three targets:

- `mc_benchmark_player_ally_retire_quadratic`
- `mc_benchmark_player_ally_retire_candidate`
- `mc_benchmark_player_ally_retire_fallback`

They use one identical compiled observer object and the same ordered core
archive graph. The quadratic adapter supplies the exact former validator.
The candidate uses production validation. The fallback adapter compiles that
same production leaf with its optional `calloc` replaced only in the test
translation unit by an observed failing allocator. Ordinary runtime binaries
have no test switch. Watcher representation, constructors and all other linked
runtime work are identical between the three products.

Each process constructs ten genuine World/App/statistics contexts. One authored
saved air Chunk per context loads through the actual store and normal Chunk
loader. Actual canonical Wolf constructors and World insertions produce either
1, 8, 64, 256 or 1,024 distinct Wolves, or that many source occurrences of the
same Wolf. Each actual ally-alert invocation queries its own leased Class List.
The first Wolf is also the held target, exercising its extra owning occurrence.
The untamed Wolves complete all reached watcher checks without alert stores.

Only checked retirement is timed. Constructors, insertion, query/body, selected
World/Entity/Random hashes, exact hold/lease/ID checks and terminal World/Chunk cleanup
remain outside each interval. Each workload also proves that insufficient hold
metadata refuses without consuming the call or changing its completed source
state; restoring only that native metadata permits cleanup without replay.
Default constructor entropy remains independent between processes.

Four rotated rounds run twelve serial processes. Two warmups and 21 measured
calls per workload give 2,520 measured retirements, 240 warmups and 120 explicit
hold-refusal/retry controls. Each process completes 103,920 native checks. The
forced-fallback product verifies every actual refused scratch request; its
control attempts and successful retries follow the genuine retirement path.

```sh
python -B tests/tools/entity/benchmark_player_ally_retire.py \
  --executable quadratic=build/mc_benchmark_player_ally_retire_quadratic \
  --executable candidate=build/mc_benchmark_player_ally_retire_candidate \
  --executable fallback=build/mc_benchmark_player_ally_retire_fallback \
  --artifacts .local/ally-retire-comparison
```

Each invocation retains a fresh directory. The runner records streamed bounded
stdout/stderr, timeout/reap evidence, every raw clock interval, exact compilation
and link bindings, and source/product/resource/saved-Chunk hashes. Partial,
misordered or changed work cannot qualify. Six portable corruption tests run
under ordinary and optimized Python.

## Final-core Windows result

The [later complete receipt](../../tests/benchmarks/player_ally_retire_windows_current.json)
binds `ally-retire-yev7nltq`, after the compact watcher Byte diagnostic change.
All 1,902 guards remain unchanged through another 12 serial processes and
2,520 measured retirements. Its three adapters again share one compiled
observer and the identical ordered core archive graph. The earlier receipt
below remains intact; differences between the two cohorts are not attributed
to the unrelated overlay change.

| Occurrences | Distinct: former | Distinct: pointer count | Repeated: former | Repeated: pointer count |
| ---: | ---: | ---: | ---: | ---: |
| 1 | 0.20 | 0.20 | 0.30 | 0.20 |
| 8 | 0.60 | 0.60 | 0.60 | 0.60 |
| 64 | 19.40 | 1.20 | 19.50 | 1.00 |
| 256 | 286.55 | 3.60 | 296.90 | 2.60 |
| 1,024 | 4,559.80 | 16.20 | 4,723.45 | 9.30 |

Values are median microseconds over 84 completed calls per cell. At 1,024
entries, these are 99.64% and 99.80% reductions within this cohort. Forced
allocation failure retains medians of 4,733.45 and 4,772.95 microseconds and
passes the same cleanup controls. Through eight entries the same allocation-free
algorithm remains; tiny timings are quantized, with no percentage gain assigned.

## Earlier frozen Windows result

The [complete scalar receipt](../../tests/benchmarks/player_ally_retire_windows.json)
binds the actual `ally-retire-3yrp7j9x` cohort, Windows Release MSVC 19.51,
strict floating transformations and one Intel Core i9-10900F host. All 1,901
guarded inputs remained unchanged. Thirty-one ordered linked libraries and
the shared observer are identical across adapters. Each cell below is the
median of 84 completed retirement intervals, in microseconds.

| Occurrences | Distinct: former | Distinct: current | Repeated: former | Repeated: current |
| ---: | ---: | ---: | ---: | ---: |
| 1 | 0.20 | 0.20 | 0.20 | 0.20 |
| 8 | 0.60 | 0.60 | 0.60 | 0.60 |
| 64 | 19.90 | 1.40 | 19.90 | 1.00 |
| 256 | 293.10 | 4.45 | 304.40 | 2.60 |
| 1,024 | 4,695.90 | 21.05 | 4,868.65 | 9.40 |

At 1,024 occurrences, these medians decrease by 99.55% for distinct Wolves and
99.81% for repeats. The forced allocation failure retains the former cost:
4,918.50 and 4,863.40 microseconds respectively. Tiny-case medians remain
unchanged at this clock's 0.1-microsecond resolution.

Initial exploratory compile and loaded-cache teardown failures remain retained
locally. Their partial measurements receive no comparison credit. The accepted
cohort rebuilds both validators against the same completed watcher migration;
it does not attribute a change of core representation to this optimization.

This qualifies finite native cleanup cost and ownership controls. It supplies
no FPS, ordinary Wolf population frequency, loaded gameplay or original
runtime comparison. Later source/build changes do not inherit these timings.
