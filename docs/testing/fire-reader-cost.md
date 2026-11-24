# Fire reader cost on Windows

The synthetic Fire reader and corruption test CLI took a median 57.890 seconds
with the preserved reader and 8.507 seconds with the regex reader in normal
Python. Under `-O`, the medians were 58.204 and 8.828 seconds. These are elapsed
times for ten Python tests on one Windows host, with two observations per
implementation in each mode. They do not measure gameplay, FPS, native replay,
Linux, or whole CI suite time.

The [machine-readable record](../../tests/benchmarks/fire_reader_windows.json)
retains every full-precision duration, pair order, median, stream hash, receipt
hash, source hash, and selected input pin.

| Mode | Baseline median (s) | Regex median (s) | Median elapsed reduction |
|---|---:|---:|---:|
| Normal | 57.890 | 8.507 | 85.30% |
| `-O` | 58.204 | 8.828 | 84.83% |

The baseline was copied from `82a0a89df053c15e9c5c21f524a05dbe2a114e0f`.
Its SHA256 is `dc739c41699f759a1f325402372b616f4f2b7015f652db43433834f536fc0e88`;
the candidate reader is
`d2036fc3589d5b560dcf19fb2a97321e8bc7e8fd5b3c163d6e0e3e0e130dbff7`.
The source diff changes the `runs` helper, its compiled regex constant, and the
`re` import. The test bodies, generated fixtures, model, parser, validator, and
corruption cases stay unchanged. Fixtures remain fresh; the candidate adds no
cache.

The host recorded CPython 3.14.3 (MSC v.1944, 64-bit AMD64), Windows
10.0.19045, Intel64 Family 6 Model 165 Stepping 5, and 20 logical CPUs.
The runs used fresh sequential children in this order:

| Pair | Mode | First process (s) | Second process (s) |
|---|---|---|---|
| 1 | Normal | Baseline 57.918 | Candidate 8.582 |
| 2 | `-O` | Candidate 8.773 | Baseline 57.853 |
| 3 | Normal | Candidate 8.433 | Baseline 57.862 |
| 4 | `-O` | Baseline 58.555 | Candidate 8.883 |

The timer starts before loading the bounded capture helper and calling it. It
includes helper loading, process launch, the plain unittest CLI, pipe/file
capture, stream hashing, and receipt writing.
Reader and selected-input hashes were taken outside that span. The JSON also
keeps unittest's separately printed durations. There are only two observations
per implementation per mode; the record provides no confidence interval or
prediction for another host.

A read-only audit rechecked all eight raw stdout/stderr pairs, their receipts,
exit statuses, exact commands, and before/after guards. Each process completed
the same ten cases with exit 0. The thirteen selected nonreader files and the
selected reader source remained unchanged across each run. The recorded
environment fingerprint was identical for all eight children. Runtime metadata
was recorded, but Python executable byte continuity was not.

Four separate instrumented children, excluded from the timing table, matched
the complete ten-case inventory, all 143 subtests, all 71 canonical positive
synthetic transport hashes, and every comparison value across both readers and
both modes. Two vector-control children each passed 1,545 vector cases for
`bytes` and `bytearray`: 3,090 calls per implementation per mode, with unchanged
inputs and the same output digest. The existing 71-profile native cohort was
not changed by this reader edit; this cost experiment did not execute native
processes or provide a new native qualification.

An earlier instrumented baseline sample was rejected because its selected local
wrapper changed across the input guard. Its raw receipt and streams are retained
by hash in the record, and its elapsed time is excluded. The guard was not
relaxed. Preliminary runs, profiling, an exploratory cache variant, inventory
audits, and vector controls are also outside the paired timing statistics.

To repeat the plain workload, save the baseline revision's
`tests/reference/world/fire/test_native.py` as `BASELINE_READER.py` outside the
Fire package, and use the candidate at its repository path. Verify both source
hashes and the record's relative public input pins first. From the repository
root, prepend the Fire package to `PYTHONPATH` and run:

```text
python -B BASELINE_READER.py
python -B tests/reference/world/fire/test_native.py
python -B -O tests/reference/world/fire/test_native.py
python -B -O BASELINE_READER.py
python -B tests/reference/world/fire/test_native.py
python -B BASELINE_READER.py
python -B -O BASELINE_READER.py
python -B -O tests/reference/world/fire/test_native.py
```

These portable commands reproduce the process roles and pair order. They are
not the verbatim commands in the retained private receipts. For comparable
measurements, keep the inherited environment fixed with the Fire package
prepended to `PYTHONPATH`, run from the repository root, and use
`tests/reference/client/controller/capture.py` with a new output directory and
180-second timeout for each command. Place `time.perf_counter()` immediately
around `capture(...)` and retain its complete stdout, stderr, and receipt.
Check reader/selected-input hashes before and after, outside that timed span.
The record lists the twelve public input pins; its thirteenth guarded file is
an unpublished audit wrapper that was not executed by the timed plain CLI.
