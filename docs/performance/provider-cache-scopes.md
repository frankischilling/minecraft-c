# Provider cache reads during callbacks

The source-owned provider keeps a map index for ordinary resident Chunk reads.
Chunk acquisition and lifecycle callbacks can publish, replace or remove map
entries before the outer operation returns. Those nested reads need the live
map, including its current Chunk identities.

During an owned mutation scope, the index now supplies positive hints only.
Each lookup probes at most sixteen existing table words. A hint is usable only
when its ordinal is within the current map and the live entry has the requested
packed key. The selected live Chunk must pass the same payload check as the
linear path. The hint stores an ordinal, never a retained Chunk pointer.

An empty slot, shifted ordinal, different key or exhausted probe proves nothing
about absence. The lookup then takes the existing live linear path. A valid
linear hit remembers its current ordinal in one home slot. This may evict
another hint; that key can still use the linear path. Collisions affect cost,
not membership or identity. Newly published entries can therefore become hints
without allocating or scanning for a free table slot.

This relies on the owned provider's existing writer contract: completed writes
preserve unique packed map keys, and no callback observes the middle of an
entry publication or compaction. A raw mutable export permanently destroys the
optional index. Caller-owned caches continue through the generic coordinator.

The outermost scope still fully validates the cache and rebuilds the canonical
index before it can prove absence. Acquisition still executes its coordinator,
pending edits, publication, activation and population. The optimization changes
only already-resident GET lookup inside an owned scope. It adds no allocation,
public layout field, Source callback or additional timing observer.

The existing scoped generic-get counter continues to count scoped dispatches,
including positive-hint hits. It no longer implies that every such dispatch
performed a linear scan. Ordinary depth-zero hit accounting is unchanged.

## Descriptor ownership after World.v changes

A GAME World's acquisition, lifecycle and saving descriptors retain their
constructed cache binding when a callback replaces its Source `World.v` field.
Scope entry now captures that descriptor's actual owner and closes the same
owner after callbacks. Reading `World.v` again at scope exit could leave the
original owner open or close an unrelated provider.

Public descriptor exports, successful raw Chunk exports and save progress
callbacks invalidate the actual escaping cache's owner. They also retain the
existing conservative invalidation of the current `World.v` receiver when it
differs. Returning from a callback cannot restore either index after export.

Owner discovery applies only to GAME construction and verifies the exact
provider kind, World and cache address. Composition accepts arbitrary callback
owners and never interprets them as concrete providers. This ownership rule
does not replace Source `World.v` dispatch, presence checks, generation or
population receivers.

## Measurement scope

The preceding [frame baseline](client-frame-baseline.md) remains a diagnostic
observation. Fixed-work resident lookup measurements and native frame intervals
answer different questions: the former fixes calls and coordinates; the latter
uses real deadlines and can execute different frame and simulation counts.
Neither establishes equality with the original game's performance.

## Fixed-work Windows result

One strict MSVC x64 Release workload object links against the actual frozen
baseline and candidate libraries. All thirty dependency roles are checked;
only `mc_simulation.lib` differs. The baseline is `ff81d0ea`. Existing public
layouts and counter meanings remain compatible with the shared object.

Each fresh process loads an authored source World with sparse resident Chunk
coordinates, qualifies every requested identity and packed-key alias, then
enters an explicit scope of depth one or two. Each timed sample performs
65,536 real `get_failure` calls, uniformly selecting across every resident key.
Timing includes coordinate and alias selection, status aggregation, and both
coordinate and pointer-identity checksum folds. QPC measures elapsed time;
these are neither API-only latency nor process CPU measurements.

The balanced order is baseline/candidate/candidate/baseline, followed by
candidate/baseline/baseline/candidate, separately for each scene. Forty-eight
fresh processes retain 336 samples. Each process contributes seven samples;
the table reports the median of four process medians per version. Twelve
earlier qualification processes are retained separately and excluded from
these medians.

| Resident Chunks | Scope depth | Baseline ns/call with loop | Candidate ns/call with loop |
| --- | ---: | ---: | ---: |
| 8 | 1 | 20.89 | 22.50 |
| 8 | 2 | 23.86 | 22.28 |
| 64 | 1 | 34.66 | 22.94 |
| 64 | 2 | 34.06 | 22.98 |
| 256 | 1 | 67.98 | 25.63 |
| 256 | 2 | 67.07 | 27.21 |

Larger resident maps show lower elapsed time. The eight-Chunk depth-one case
costs about 1.61 ns more per call; the optimization does not improve every
scene. This cohort does not establish statistical significance.

All sixty processes pass exact output, Chunk identity, complete plane and
state checks. Outside-clock checks preserve World and Random state, cache and
pending storage, diagnostic sentinels, ownership holds, scope depth and checked
cleanup. The authored save inputs remain unchanged apart from the real session
lock. Source, object, executable and dependency guards remain unchanged during
each cohort.

The [finite record](../../tests/benchmarks/provider_cache_scope_windows.json)
contains every balanced sample and source/product fingerprints. At the
`986ca4f2` cache checkpoint, clarifying the measured source's initial comment
left every subsequent byte unchanged; the record preserves both hashes and
that identity proof. The later [quiet-setter harness](quiet-setter-acquisition.md)
adds a default entry binding to reuse the same provider fixture. That later
source change does not alter the earlier measurement bindings. The host is an Intel
Core i9-10900F running native Windows, using MSVC 14.51.36231 with `/O2 /W4 /WX`
and strict floating-point settings.

These scenes measure warmed, already-resident reads. They perform no map churn,
load, generation, actual callback body, rebuild or rendering inside the clock.
Separate regression controls cover those map and callback transitions. This
measurement establishes no gameplay FPS or original-game performance gain.

## Four native frame intervals

A separate baseline/candidate/candidate/baseline cohort uses one existing
compiled frame observer, 92 actual baseline dependency roles and thirty
controlled candidate roles. Sixty-two common roles stay identical, and only
the simulation library differs among the controlled roles. No archive member
is substituted. The observer adds passive cache snapshots after each existing
host-end QPC read. Its between-frame overhead remains part of the deadlines;
no estimated overhead is subtracted.

Every interval uses the same authored Tiny sky view at 854×480, OpenGL 3.3,
swap interval zero and performance mode one. The GPU is an NVIDIA GeForce
RTX 3060 with driver 581.42. Startup receives physical DISPLAY2's work bounds
before window creation. Both real frame and canvas windows pass the monitor
geometry assertion before warmup. All four intervals pass foreground, complete
pose, simulation-work, source/product and checked-cleanup guards. No interval
is retried.

| Order | Version | Frames | World ticks | FPS | Host p95 ms | Maximum host ms |
| ---: | --- | ---: | ---: | ---: | ---: | ---: |
| 1 | Baseline | 365 | 190 | 36.46 | 47.64 | 243.10 |
| 2 | Candidate | 380 | 192 | 37.95 | 51.94 | 169.41 |
| 3 | Candidate | 381 | 192 | 38.05 | 45.05 | 171.64 |
| 4 | Baseline | 373 | 194 | 37.24 | 56.40 | 242.44 |

The two-interval FPS medians are 36.85 and 38.00, a 3.12% difference. Median
maximum host-frame time changes from 242.77 to 170.52 ms. Both candidate FPS
observations exceed both baseline observations, but p95 does not improve in
every interval. Two observations per version establish no statistical
significance or unrestricted smooth-play result.

These deadline-driven intervals perform different work. Baseline miss counts
are 21 and 22; candidate counts are 29 and 21. Scoped GET dispatch totals range
from 1.179 to 1.559 million. The [finite frame record](../../tests/benchmarks/provider_cache_scope_frames_windows.json)
preserves every interval's work ledger, clocks, monitor receipts, cache totals,
graphics context and product fingerprints. The comparison supports a native
performance opportunity; it does not isolate the hint's contribution from
changes in work or execution scheduling.

All 28 frames exceeding 100 ms still coincide with indexed misses. The probe
does not separate disk loading, generation, population, activation, lighting or
validation time. Residual acquisition stalls and the steady rendering prefix
remain separate performance targets. This is no pixel, audio, GPU-duration or
original-game FPS comparison.

## Native ownership and mutation regressions

The source-owned cache driver covers sixteen modes. New controls exercise a
nullable Source provider, a second live provider, progress-callback rebinding,
nested live-map changes and a nonselected invalid payload left by a real
entity callback. The map controls retain packed aliases, replacement identity,
append, ordinal shifts, equal-count reinsertion, failed-publication byte
atomicity, nested depth and final canonical rebuild. Corruption remains live
through the outer validation failure, committed publication and diagnostic
checks; restoring it afterwards does not replay callbacks or recreate an index.

One compiled final test object links against both authentic old and candidate
libraries. All thirteen retained positive modes produce byte-identical old/new
stdout. The three old owner modes reach their expected semantic guards and
fail; all sixteen candidate modes pass. The old failures are ordinary checked
assertions, with complete capture and no timeout or crash. A separate earlier
owner-null proof also retains the unchanged 183-check positive control and
34-check candidate result. This is native ownership evidence, not a fresh
original-game comparison of these replaceable-field graphs.

The complete Windows MSVC Release build and 1,268-test suite pass, including
every cache mode and both normal/optimized reader controls. Twelve selected
Linux tests also pass under ASan, UBSan and leak detection. Strict GCC13
compilation covers the two changed runtime units, native fixture and benchmark.
The [test record](../testing.md#provider-cache-scopes-and-owner-rebinding)
preserves their exact source and result scope.

## Chunk presence during population

The Windows DISPLAY2 trace at `c4153f33` found random ticks calling into
neighboring Chunk generation and population. In one 230 ms frame, two
population calls took 201 ms in total and generation took 7 ms. The sampled
Chunk itself was already resident. Two finer captures recorded 62 population
calls. Median ore placement times were 20.9 and 21.6 ms per call in the two
processes. Spring work usually took about 1 ms, with a maximum of 96 ms in one
capture. These nested timings attribute the frame stalls to reached work in the
fixed scene.

Population mutations check Chunk presence while publishing and updating chunks.
For an owned cache with more than eight entries, these checks can now accept
the same bounded ordinal hint as a scoped GET. The current map key must match
before the hint proves presence. An absent or stale hint falls back to the
live scan, as do smaller scoped maps. Presence reads key membership and leaves
the table, payloads and World state alone. The `scope-presence` control covers
packed aliases, a null value behind a present key, compaction, an appended
key and an absent key at two levels of nesting.

A private MSVC Release comparison linked one workload object and a shared
set of dependency archives. The baseline provider came from `c4153f33`. The
candidate replaced just that object. Four alternating pairs per cache size
and depth produced 48 fresh processes. Each produced seven samples of 65,536
checks for resident keys and seven for a 75% hit mix. Each process checked
every answer, packed aliases, diagnostics, cache counters, saved fixture files
and Chunk planes. The four eight-entry medians shifted by −0.5% to +2.3%.
Times below are per sample.

| Resident entries | Scope depth | Resident keys, base → candidate | 75% hit mix, base → candidate |
| --- | --- | ---: | ---: |
| 64 | 1 | 1.639 → 0.718 ms | 1.985 → 1.270 ms |
| 64 | 2 | 1.645 → 0.715 ms | 1.984 → 1.269 ms |
| 256 | 1 | 4.799 → 0.838 ms | 5.824 → 2.884 ms |
| 256 | 2 | 4.730 → 0.861 ms | 5.674 → 3.004 ms |

Six uninstrumented `DISPLAY2` runs passed the fixed-camera, foreground and
World-work checks. The two products shared the observer and dependencies,
which included the local Sign screen edit. The timed scene stayed in gameplay.
Median FPS was 34.39 before and 35.76 after. Median run p95 fell from 72.12
to 61.23 ms, while median run p99 rose from 129.92 to 139.41 ms. One of three
pairs lost FPS and had a higher p95. An earlier instrumented comparison with
the initial candidate lost foreground and could not qualify. The frame
measurements cover this scene and leave the spring spikes open.

Ten focused tests passed on each host, including the 17 owned-cache profiles,
natural population, lighting and random ticks. Linux ran with Clang, ASan,
UBSan and leak detection.

## Reproduce one resident scene

Configure with `-DMC_BUILD_BENCHMARKS=ON` and build the
`mc_benchmark_provider_cache_scope` target. The public runner creates fresh,
asset-independent authored World files and retains raw output plus a checked
report:

```sh
python -B scripts/benchmarks/provider_cache_scope.py \
  --executable build/mc_benchmark_provider_cache_scope \
  --artifacts .local/provider-cache-scope --chunks 256 --depth 2
```

Use the corresponding `.exe` path on Windows. Resident counts are 8, 64 or
256; depth is one or two. A single invocation observes one executable and
cannot establish a revision speedup. The normal and optimized Python reader
controls reject malformed completion, semantic receipts and modified authored
files, including the full 256-Chunk region layout.
