# Integer-set kernel costs

The Integer and Tile identity sets share a private bucket/tree kernel. Root
lookup stays inline to avoid an extra out-of-line call. Allocation sizes and
collection history remain intact; the measured cost varies by workload.

[The public workload](../../tests/benchmarks/java/int_set.c) checks every API
operation and folds results and iteration order into checksums. It covers 4,096
dispersed keys, 4,096 colliding keys, and 16,384 keys reduced to eight survivors
followed by 256 order copies. Allocation, cleanup, checks and checksum work are
inside each `clock()` interval.

The [record](../../tests/benchmarks/java/int_set_linux.json) retains every process
and timing row. The final cohort compares the Integer implementation at
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

The dispersed regression remains unresolved. The preceding candidate cohort
also remains in the record: +6.17%, +0.04% and -23.56% respectively. A separate
capture pinned a private header instead of the formatted compiler input; it is
excluded from final-source qualification, with all timings retained. No cohort
establishes gameplay FPS, frame-time improvement, Windows performance or another
machine's behavior.

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
