# Owned World acquisition and TileEntity calls

The owned acquisition entries share the existing World get/acquire body, cache
and activation coordinator. Their caller holds the evaluated World before later
source arguments and supplies the defining Chunk code sites independently of
nullable or mutable Chunk.d. Ordinary queries retain their existing legacy
entry. No Entity field or public error layout grows.

## One actual query and its owners

The kernel reserves one loader attempt before the actual query and records its
returned status and direct source frame before restoring the scoped acquisition
frame. It completes that exact reservation without replaying a getter or
searching ambient last-error storage. A fixed two-slot VALUE ledger preserves
actual pending attempts if structural cleanup refuses. Only the caller that
restores the matching current World can finish its returned attempt. That native
continuation performs no source work or implicit scope pop.

Native ownership status remains separate from the first source failure. Entity
activation/publication and TileEntity callbacks record actual capture, admission
and pop refusals at their operation, before parent completion can report another
error. The native result preserves that first ownership error while the existing
public error preserves the source cause. NULL diagnostic output does not discard
the required ownership result.

The implementation is in the shared [kernel](../../src/world/runtime/runtime.c),
[result bookkeeping](../../src/world/runtime/runtime_execution.c) and
[private interface](../../src/world/runtime/runtime_execution_internal.h).

## TileEntity source order

Owned TileEntity activation captures Chunk.d before Map.values. Inherited
dirtying preserves the initial Tile.world guard and reads the fresh invocation
receiver before coordinates. World dirtying retains its presence/acquisition,
Chunk mutation and live observer order. A failing callback transfers the actual
local WorldAccess receiver's existing right before temporary TileEntity and
remaining receiver releases. A same-node HELD or ARMED lookup refuses through
checked lease admission before live_enter or source provider fields. Genuine
callback nesting first enters a fresh child scope.

Owned lookup shares the real cold query and supported lazy Block/factory chain.
The caller explicitly borrows the defining Registry and factory classes; reached
static fields and constructors use those owners at their original positions.
Lazy Chunk.d is captured before coordinate arithmetic and Block invocation,
including a constructor before a nullable World.set invocation. No source pointer
is rebound or repaired. These joins are in the shared
[activation](../../src/world/runtime/activation.c) and
[TileEntity](../../src/world/runtime/tile_entities/tile_entities.c) bodies.

## Executed controls

The [native fixture](../../tests/world/runtime/world_access/test_kernel_execution.c)
passes 193 checks per host in four fresh processes: 48 empty cold/cache checks,
48 source callback checks, 48 native callback checks and 49 first-owner checks.
Its genuine SourceSaving inputs contain either an empty Chunk or actual Item
and Chest NBT. Real constructors, import, Tile Map registration and Entity
activation reach the registered callback. The callback removes itself, attempts
checked producer retirement and returns its real protected receipt. The first
owner case restores its temporary native binding-count fault immediately after
the one query returns, then checks the actual capture operation/site and finishes
the returned pending attempt through exact native unwind. It never repeats source
work or repairs a source field.

The empty mode performs one actual cold get and a same-key cache hit, including
the failure-only API with NULL diagnostic output. The source loader's reader,
document, record and PC remain unchanged across the cache hit. This fixture's
existing provider-field inspection exports the raw cache, so it does not qualify
the optional hash-index fast path or measure query speed.

The [bounded runner](../../tests/tools/world/test_kernel_execution.py) requires
the [explicit source bundle](../../tests/tools/world/kernel_execution_bundle.json),
fresh controlled ASCII roots, complete stdout/stderr/capture JSON and unchanged
source/product/helper/six-table guards. It independently hashes all three authored
files before and after every process; only the genuine eight-byte session.lock
addition is allowed. Deleted, renamed or empty replacement input roots fail the
terminal guard. Native byte readback runs after existing checked World/Store
teardown closes the cached region handle and before releasing input snapshots.
Production file-sharing behavior is unchanged.

Normal and optimized Python each reject 168 corrupt protocols, 80 altered or
missing retained payload sets, seven literal-input changes, four changed bundles
and three real whole-directory loss/replacement cases. Initial MSVC inspector
initialization and Windows live-handle readback failures remain retained. Strict
warnings stay enabled; the final fixture passes on both hosts.

The six focused tests pass Windows in 37.62 seconds and Linux in 14.71 seconds.
Frozen broader regressions pass Windows 94/94 in 146.51 seconds and Linux 92/92
in 94.01 seconds, with unique Passed names and no skipped or unrun tests. Linux
uses Clang Debug ASan/UBSan with leak detection and halt-on-error. Its minimal
client graph omits the two CPU Arrow drawing cases; they receive no Linux credit.
Both builds, exact clang-format 18.1.3 over 2,281 files and thirteen actual GCC
syntax recipes pass. These timings are test durations, not game FPS measurements.

Run the configured build's six focused tests with:

```text
ctest --test-dir <build> -C Release --output-on-failure -R "^(world\.kernel_execution_(empty|source|native|first-owner)|tools\.world_kernel_execution_(normal|optimized))$"
```

For Linux Debug, omit the Windows configuration argument.

## Remaining composition

The finite kernel fixture enters Tile Map registration, not the separate cold
owned Tile lookup/lazy construction chain or a malformed source load through the
new kernel entry. Those routes need their own runtime controls. Loading/import,
generation/population and captured-provider descendants retain an explicit
external caller graph lifetime contract. General caller unwind propagation,
foreign A-query/Chunk.d B joint Store teardown, complete Arrow flight, pickup and
World-tick dispatch remain unfinished. No new original JVM, visible game,
whole-frame/audiovisual equality or sustained FPS result accompanies these checks.
