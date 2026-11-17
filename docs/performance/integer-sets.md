# Integer-set kernel costs

The Integer and Tile identity sets share a private bucket/tree kernel. Root
lookup stays inline to avoid an extra out-of-line call. Allocation sizes and
collection history remain intact; the measured cost varies by workload.

[The public workload](../../tests/benchmarks/java/int_set.c) checks every API
operation and folds results and iteration order into checksums. It covers 4,096
dispersed keys, 4,096 colliding keys, and 16,384 keys reduced to eight survivors
followed by 256 order copies. Allocation, cleanup, checks and checksum work are
inside each `clock()` interval.

The [earlier record](../../tests/benchmarks/java/int_set_linux.json) retains every
process and timing row. Its final cohort compares the Integer implementation at
`1be733fe26744259f4a2cb9fd87a9141d3bb99e7`, an intermediate shared kernel and
the inline-root implementation. One warmup per variant precedes all six
permutations of variant order. Each process runs 128 iterations per case.
Workload results, operation counts and checksums agree across all variants.

The host is an Intel Core i9-10900F under WSL2, Linux 6.6.87.2 and GCC 15.2.0,
with affinity to logical CPU 0. All variants use C17, `-O2 -DNDEBUG`, strict
warnings and the same workload. GCC's clock measures process CPU time; values
below are medians of six measured processes per variant.

| Case | Pre-refactor CPU ms | Inline-root CPU ms | Change |
| --- | ---: | ---: | ---: |
| Dispersed operations | 87.474 | 92.041 | +5.22% |
| Collision trees | 203.270 | 199.987 | -1.62% |
| Sparse order copies | 748.576 | 554.443 | -25.93% |

That checkpoint retains a dispersed regression. The preceding candidate cohort
also remains in the record: +6.17%, +0.04% and -23.56% respectively. A separate
capture pinned a private header instead of the formatted compiler input; it is
excluded from final-source qualification, with all timings retained. No cohort
establishes gameplay FPS, frame-time improvement, Windows performance or another
machine's behavior.

## Cached traversal cursor

The current copy operation caches its valid size and table end, scans empty
buckets with a local cursor, and copies each complete next chain. It stops
after the known number of entries, avoiding trailing empty buckets. An empty
set reports count zero without forming a table pointer. Order, keys, node
layout, allocation, limits, mutation history and the separate `first` query
remain unchanged.

The [new record](../../tests/benchmarks/java/int_set_cursor_linux.json) contains
all smoke, warmup and measured processes from two cohorts: the rejected nested
loop and the selected cursor. Each uses the unchanged public workload, GCC
15.2.0, CPU 0, three smoke runs at eight iterations, three warmups at 128, and
all six variant permutations at 128. Actual emitted cursor instructions were
reviewed before timing; the measured ELF exactly matches that preflight product.

These are medians from the cursor cohort, in process CPU milliseconds:

| Case | Pre-refactor | Shared kernel at `26e634d3` | Cached cursor | Cursor/shared change |
| --- | ---: | ---: | ---: | ---: |
| Dispersed operations | 89.7540 | 93.2680 | 87.7215 | -5.95% |
| Collision trees | 202.0455 | 200.2595 | 200.9525 | +0.35% |
| Sparse order copies | 751.6210 | 543.1595 | 497.1525 | -8.47% |

The cursor is faster than the shared-kernel version in all six dispersed and
all six sparse pairs. Dispersed versus pre-refactor has a median reduction of
2.26%, with five of six pairs faster. Tree pairs cross zero, with three faster
and three slower. These fixed pairs are descriptive; they establish no
statistical noninferiority or universal performance preservation.

The earlier nested loop reduced dispersed time by 7.39% but increased sparse
time by 39.61%, with all six sparse pairs slower. It was rejected. Both cohorts'
complete raw timing rows and required sparse-gate results remain in the record.

Candidate-linked validation passes six selected tests on each of MSVC, GCC and
Clang: native contracts, allocator/failure histories, captured JDK order, World
order in normal/optimized Python, and pending-queue replay. Separate fresh JDK
observed/control traces match the fixed seven-case oracle on each host: 574
operations and 588 C responses. Linux runs enable ASan leak detection and UBSan.
A retained-empty-table regression checks output preservation, count zero and
null output with zero capacity. These controls do not qualify ordinary gameplay
FPS, Windows timing or other hardware.

The subsequent complete MSVC Release build and 1,380-test Windows sweep pass
with all preceding names retained and no skips. The four-worker sweep takes
571.01 seconds; all 6,602 public files and 689 native products stay unchanged
during it. This integration run is separate from the CPU timing cohort.

From the repository root, build and run the current workload:

```sh
gcc-15 -std=c17 -O2 -DNDEBUG -Wall -Wextra -Wpedantic -Werror -Iinclude \
  tests/benchmarks/java/int_set.c src/core/java/int_set.c \
  src/core/java/hash_table_internal.c -o int-set-benchmark
./int-set-benchmark 8
taskset -c 0 ./int-set-benchmark 128
```

For the baseline, extract `src/core/java/int_set.c` from the named commit into a
separate directory. Compile it with the same workload, flags and public header,
without the shared kernel unit. Retain every attempt, alternate variant order,
compare checksums before clocks, and record source/product identities and host
details. Check the native runtime's `clock()` semantics before comparing platforms.
