# Reference-reader performance

## Locale selection for native replays

The held-item attack and plant lifecycle drivers run their Linux native
children with `LC_ALL=C`. Both fixtures use authored English templates.
The statistics registry tests separately exercise `C.UTF-8` and fallback
when a locale category is unavailable.

On WSL2 with Clang ASan/UBSan, one instrumented held-item run spent 39.66
of 42.83 seconds in the setup interval that constructs the water texture
and triggers cold registry initialization. A second comparison used the
same uninstrumented executables and fixtures under both locale settings.

| Replay | C.UTF-8 wall time | C wall time |
| --- | ---: | ---: |
| Held item attack | 43.839 s | 9.092 s |
| Plant lifecycle | 29.784 s | 12.135 s |

Both settings produced the same 422 held-item rows, passed the three
independent Cow entropy audits, and wrote identical plant native packets
for both sides. The timings cover the complete replay commands on WSL2.

## Plant lifecycle replay

The corruption controls copy only the containers along each changed path.
The cell validator checks the same five exact integer types, bounds and
ordering without building a generator or slicing each cell.

Four alternating WSL2 pairs ran both sides' controls over the same captured
native packets. Median Python CPU time for the guards fell from 5.990 to
1.933 seconds. All 332 ordered comparison outcomes and 152 additional cell
validation cases matched. The complete replay still compares 59 cases per
side and runs 12 corruption and 154 malformed-input guards per side. The
timing covers the Python guards with the native probe already captured.

## Redstone Ore fixture construction

The `ff81d0ea` GCC CI run passed 1,341 of 1,343 tests. Both Redstone Ore native
reader controls reached their 180-second limit while running alongside the
normal and optimized Fire controls. Windows, Clang and formatting passed.

The Ore controls repeatedly built the same synthetic native transport before
passing each corruption to the real parser and validator. The large random
World-tick profile alone contains 361 complete Chunks. A diagnostic cProfile
run completed all ten original test methods: repeated plane run encoding
accounted for 250.247 of 261.441 profiled seconds. Profiling adds overhead;
these figures are not a plain before/after speed comparison.

The test helper now derives each complete profile fixture once. Every check
receives a fresh deep copy. The after, retired and cleanup states also receive
independent copies of the same prepared after-state encoding. Public reader,
model, comparison assertions, profile and corruption loops, and watchdogs are
unchanged.

All 67 old/new profile transports match byte for byte. Separate controls verify
phase-copy and pristine-fixture isolation. The complete ten-method candidate
suite passes in fresh normal and optimized Python processes, taking 10.879 and
11.267 seconds on Windows. This establishes the retained test result and lower
duplicate construction work; it does not establish a whole-CI speed ratio.

## Bed reference-reader CPU comparison

The public World [Bed packet controls](../../tests/reference/world/bed_interaction/test_packet.py)
spent most of their time revalidating the same original packet while testing
changed native payloads. The source validator checks all 85 original profiles,
plane fingerprints, 408 selected calls and 20 live Code records. Repeating
that work for each of 1,110 native-payload mutations added no new source input.

The public `native_rows` entry still validates every supplied original packet.
It then delegates to the same parser body. Only the internal mutation loop
uses that parser over its already validated original. A final deep comparison
requires the whole original to remain unchanged. All 1,167 corruption controls
remain: 1,110 native payloads, 23 original-packet mutations, 24 provenance
mutations, five JSON cases and five sparse-plane domain/order cases. Original
mutations still execute the complete source validator. No parser condition,
comparison field, assertion or watchdog is removed.

### Fixed work and measurement

Both cohorts ran on the same Intel Core i9-10900F, ten cores and twenty logical
processors. Windows 10 build 19045 used CPython 3.14.3; Ubuntu under WSL used
CPython 3.12.3. Each host ran three serial baseline/applied pairs in fresh
normal Python processes, six processes per host. Windows ran baseline first;
WSL reversed that order. The two hosts ran serially because they share the
physical CPU. Other builds and native tests were terminal during measurement.

Each process used the same immutable public packet and complete 85-row native
sample. It completed all 1,167 rejection controls and the 2,569-value projection.
Baseline/applied stdout was byte-identical within each host; all processes
exited zero with empty stderr. Before/after checks preserved every packet,
helper, reader snapshot and sample fingerprint. The original packet and its
historical capture provenance did not change.

Wall time spans the complete fresh process. CPU time includes its complete
user and kernel time: Windows `GetProcessTimes` after child completion, or
serial `RUSAGE_CHILDREN` differences after WSL child completion. These are
separate measurements; WSL wall time includes substantial additional waiting.

| Host | Pair | Version | Wall seconds | CPU seconds |
| --- | --- | --- | ---: | ---: |
| Windows | 0 | Baseline | 41.890857 | 41.843750 |
| Windows | 0 | Applied | 13.903660 | 13.828125 |
| Windows | 1 | Baseline | 42.040039 | 41.906250 |
| Windows | 1 | Applied | 13.888430 | 13.796875 |
| Windows | 2 | Baseline | 41.867438 | 41.765625 |
| Windows | 2 | Applied | 13.924269 | 13.875000 |
| WSL | 0 | Applied | 21.589597 | 14.877467 |
| WSL | 0 | Baseline | 58.144066 | 42.223755 |
| WSL | 1 | Applied | 21.545398 | 14.849493 |
| WSL | 1 | Baseline | 57.104686 | 42.503310 |
| WSL | 2 | Applied | 20.973624 | 14.850092 |
| WSL | 2 | Baseline | 57.370220 | 42.220716 |

Windows median CPU falls from 41.843750 to 13.828125 seconds, **66.95295% lower**.
Its baseline range is 41.765625–41.906250; applied is 13.796875–13.875000.
Median wall time changes from 41.890857 to 13.903660 seconds.

WSL median CPU falls from 42.223755 to 14.850092 seconds, **64.83001% lower**.
Its baseline range is 42.220716–42.503310; applied is 14.849493–14.877467.
Median wall time changes from 57.370220 to 21.545398 seconds. Wall ranges are
57.104686–58.144066 baseline and 20.973624–21.589597 applied. Windows wall
ranges are 41.867438–42.040039 and 13.888430–13.924269 respectively.

### Source identity and limits

The comparison used authored snapshots of the two reader files. The applied
public bytes equal the measured applied snapshots. AST checks preserve the
public validation and parser bodies; reversing the small extraction and
test-only changes restores the exact baseline bytes.

| File | Baseline SHA256 | Applied SHA256 |
| --- | --- | --- |
| `run_native.py` | `462d508d9a37ca5090847ada40abe0f7311ea6bfce43118858e99c729d149050` | `bac70c2b42619463642d792c7e156625349023fa6d59b36ccfb3c3b0bf4a3717` |
| `test_packet.py` | `4092d6ded025e04470829afdf3aa144e6dd7d8353df15ca7b6ca9471cc65856c` | `46612d3664f5ee0a6d021197b60ea7de3c70f1266f5a427d45510f8745593138` |

The current controls run without a JAR, JVM or original assets:

```sh
python -B tests/reference/world/bed_interaction/test_packet.py
python -B -O tests/reference/world/bed_interaction/test_packet.py
```

Raw process records and full precision timing receipts remain private. The
figures above are selected numeric observations. This cohort measures normal
Python reader-control CPU and wall time. It establishes no optimized-Python
speedup, native execution speedup, whole-CI percentage or gameplay FPS gain.

## Player sleep/wake comparison diagnostics

The [Player sleep/wake reader](../../tests/tools/entity/test_player_sleep_wake.py)
built failure messages during every successful comparison. Its corruption
controls repeat long prefixes of the 76-row result. Four explicit failure
branches now construct those messages only when a comparison fails. Every
recursive visit, equality operator, strict type check and scalar counter stays
in place.

The measurement used three fresh, serial Linux processes before and three
after for each mode: normal Python and `-O`, twelve processes altogether.
Each process ran the canonical `--controls` command, admitted the complete
original packet and passed all 4,556 rejection controls under the same
90-second limit. Wall time spans the complete child process; CPU time is its
user plus kernel time from serial `RUSAGE_CHILDREN` differences. Input hashing
outside the child is excluded.

| Mode | Median wall before | Median wall after | Median CPU before | Median CPU after |
| --- | ---: | ---: | ---: | ---: |
| Normal | 18.092993 s | 10.860059 s | 17.522261 s | 10.266599 s |
| `-O` | 18.545970 s | 11.898417 s | 17.909595 s | 11.303462 s |

All 4,556 ordered rejection IDs, exception classes and message bytes match
exactly: 148 parser cases and 4,408 changed-copy comparisons. Sixteen focused
checks also preserve type, key, length and scalar failures, including bool/int,
nested values, `None` and Unicode. The complete 76-row comparison still visits
9,784 scalars; the native replay retains those same required comparisons and
its existing fixtures. Packet, helper and configuration hashes stayed fixed.
Source and data assertions, the 180-second native subprocess limit and the
240-second native CTest limit are unchanged. Windows normal and `-O` acceptance
also passed all 4,556 controls; no Windows before/after speed claim is made.

The measured driver changed from SHA256
`ad9cf6fd65ce973763bd70a4a3c0e99744d54803271cdc5caf8257fb91855cd2` to
`b1bdcf5ba9dc4714f00818a969dc31f8f83b89d15da12408e63f8ea6920f1ce0`.
Only `compare()` changed. A separate bounded profile identified that function
as the main cost; its instrumented run is excluded from these medians. Raw
process records and full precision receipts remain private. These observations
measure Python corruption-control processes on one host. They establish no
native execution speedup, gameplay FPS gain or whole-CI percentage improvement.

## Player tick transport-control setup

The [Player tick controls](../../tests/tools/entity/test_player_sleep_tick.py)
copy only the changed row and state, and reuse serialized text for untouched
rows. Every control still feeds its complete transport to the actual parser,
including every reached prefix and plane digest. Independent normal and
optimized Python proofs match all 5,847 events: all 2,559 complete parser
payload hashes, the ordered 2,859 rejection IDs/classes/messages, and the
projection/comparison calls. The original and authored fixtures remain unchanged;
the parser and every function outside `controls()` have identical ASTs.

Three fresh serial baseline/applied pairs per mode ran on Windows 10 Home
10.0.19045, Python 3.14.3 (`MSC v.1944`, x86-64), and the Intel Core i9-10900F
with ten cores and twenty logical processors. Pair order alternated. Builds
and other heavy runtime work were terminal. All twelve children completed
the full controls within the unchanged 90-second bound with empty stderr,
complete pipe drains and unchanged producer, packet and Python binary hashes.

| Mode | Median CPU before | Median CPU after | Reduction | Median elapsed before | Median elapsed after |
| --- | ---: | ---: | ---: | ---: | ---: |
| Normal | 31.343750 s | 23.234375 s | 25.87% | 31.345033 s | 23.229065 s |
| `-O` | 31.406250 s | 23.078125 s | 26.52% | 31.414566 s | 23.076007 s |

The child measures `time.process_time()` and `time.perf_counter()` around
module import, fresh original-packet admission and the complete controls.
These intervals exclude process startup and before/after hash guards.
Profiling and instrumented proof runs are excluded. The measured driver
changes from SHA256 `333312ebe6de045d9f555c1dbad781d4ff889098453fc2b1bf95a0f7ab218251`
to `7677557f711787fbbade553807e80adfa240a211808b3db2cbf87c83701437d7`.
This measures the existing test setup on one host. It supplies no native
runtime, new NBT comparison, hosted-job, whole-suite or gameplay FPS speedup.

## Region writer fixture validation

The [region writer checker](../../scripts/reference/storage/compare_region_write.py)
validates each header vector without per-entry calls to the generic integer
checker. It compares scalar JSON list entries inline and checks bytes outside
each permitted write span in slices.
The byte audit covers the same shared extent, including padding outside a
write. Numeric types and signed 32-bit bounds still participate in validation.

Three alternating pairs of fresh processes ran the same 35 tests on an
Intel Core i9-10900F with Windows 10 and CPython 3.14.3. The baseline is
`2124f34e`. Times are elapsed seconds for the full Python process.

| Pair order | Baseline | Candidate |
| --- | ---: | ---: |
| Baseline first | 28.928 | 22.804 |
| Candidate first | 28.825 | 22.368 |
| Baseline first | 28.925 | 23.608 |

Median elapsed time fell from 28.925 to 22.804 seconds, a 21.2% reduction for
this test command. The expanded suite also checks that nested numeric type
aliases are rejected and a write cannot change padding in an unrelated sector.

### Header decoding and retained owners

The checker validates each 1,024-word header with an exact type check and
signed range extrema. It decodes signed words directly from the file when
comparing them to recorded owner state. Once every owner field has passed
type validation, the audit uses dictionary equality for the retained state.
Standalone audits still compare JSON types recursively.

Four alternating pairs of fresh CPython 3.14.3 processes ran the same 39
guards on Windows 10 with an Intel Core i9-10900F. The baseline was
`3e6dfb07`. All guards passed in both versions, including the added checks
for malformed header words and a numeric alias in a retained owner.

| Pair order | Baseline (s) | Candidate (s) |
| --- | ---: | ---: |
| Baseline first | 23.351 | 19.345 |
| Candidate first | 23.931 | 19.475 |
| Baseline first | 23.559 | 19.921 |
| Candidate first | 23.844 | 19.859 |

Median elapsed time fell from 23.702 to 19.667 seconds, a 17.0% reduction
for the Python guard suite. A separate authored decoder control matched
all 2,048 header words in signed and unsigned mode and rejected
out-of-bounds and misaligned image lengths.
