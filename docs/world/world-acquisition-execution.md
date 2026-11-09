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

## Direct loader errors and cold Tile lookup

Two [loader controls](../../tests/world/runtime/world_access/test_kernel_loader_execution.c)
perform one actual owned get each over a complete authored MCR with non-Compound
NBT. The ordinary error-output case passes 49 checks; the NULL-output case passes
48. Each selects the kernel's actual protected loader copy before inspecting the
known producer. The full caller frame and direct projection match the returned
failure. Discarding the caller diagnostic, rejecting stale generation tokens,
popping the failed scope and attempting BUSY World retirement preserve the inline
copy until checked execution retirement. There is no manually reserved attempt,
extra getter or outcome supplied after failure. No Chunk was published, so cleanup
uses the actual empty descriptor/World path and normal source-region flush.

Five [Tile lookup controls](../../tests/world/runtime/world_access/test_tile_lookup_execution.c)
pass 327 checks: 62 for cold imported-Map lookup, 97 for real lazy construction,
and 56 each for ARMED refusal, HELD refusal and a fresh nested alias occurrence.
The lazy input authors one Chest Block byte before bootstrap while leaving its
TileEntities list empty. The actual supported factory, constructor, World.set
and Map reread produce the returned Chest. Existing-Map cache calls with NULL
unused defining borrows do not invoke a factory or preheat another namespace.

The source's [Chest capacity distinction](tile-entities.md#objects-and-shared-classes)
is preserved: lazy construction has 36 physical slots; NBT import replaces that
array with 27. The first fixture incorrectly required 36 in both cases and failed
the common imported prefix on both hosts. Original client/server Code and the
existing native bodies confirm the distinction. The corrected fixture keeps
every other Map/list/World/coordinate/ownership check, including at least three real
holds. Those initial failures provide no callback-admission execution credit.

Actual World-dirty callbacks exercise the current ARMED and HELD nodes. The HELD
case genuinely removes its receiver and returns a managed receipt; producer and
receipt retirement remain BUSY until the protected right releases. Lookup refuses
at the real early admission operation before source prefix work. The nested case
pushes a distinct alias B occurrence beneath ARMED A, performs its real cached
lookup, consumes the returned hold and closes B without disturbing A. It does not
pretend to test foreign World/Store ownership.

The [common runner](../../tests/tools/world/test_world_query_controls.py) uses an
[explicit source bundle](../../tests/tools/world/world_query_controls_bundle.json)
and [deferred input recipes](../../tests/tools/world/world_query_inputs.py).
All seven processes have fresh controlled ASCII roots and retained exact stdout,
stderr and typed capture JSON. Full source/product/helper/six-table guards and
all authored files remain unchanged; only the genuine eight-byte session.lock
addition is allowed. Whole-root loss still fails the terminal check. Normal and
optimized Python each reject 350 corrupt transports, 140 retained payload
changes, seven literal changes, four changed bundles and three real directory
loss/replacements, while preserving the first failure.

These nine CTests pass Windows in 60.17 seconds and sanitized Linux in 25.84
seconds, with 424 native checks per host. Broader frozen regressions pass Windows
103/103 in 206.96 seconds and Linux 101/101 in 127.93 seconds, without skipped or
unrun tests. Both builds, exact formatting of 2,283 files and GCC syntax for both
new C units pass; the sole changed lookup fixture passes its recipe again.
At that query-control checkpoint, production runtime bodies remained byte-identical
to the earlier component.

```text
ctest --test-dir <build> -C Release --output-on-failure -R "^(world\.(kernel_loader_execution_(error|null-error)|tile_lookup_execution_(map|lazy|armed|held|nested))|tools\.world_query_controls_(normal|optimized))$"
```

## Collision ID and metadata reads

The [owned collision reader](collision/block-values.md) now shares its existing
complete scalar body with the held entry. It carries the actual selected loader
outcome and pending native continuations before diagnostic projection, preserves
the source cause separately from native ownership errors, and stops before
scalar payload work when native completion refuses. The caller still owns its
exact evaluated World and defining code/resources.

Seven new native modes pass 375 checks per host over real cold/cache reads,
outside-domain guards, one-query malformed loading and genuine ARMED/HELD
admission. With the earlier loader/Tile controls, the 16-test selection passes 799
native checks per host. Disjoint broader runs preserve all preceding coverage:
110 unique Windows and 108 sanitized Linux tests pass, without skips or unrun
names. Both builds, formatting of 2,285 files and two actual GCC recipes pass.
Parent Arrow result/classifier/checked-unwind composition, structural native
completion refusal and whole gameplay remain open; this is no frame/FPS result.

## Callee close continuations

The [activation close result](activation-close.md) now carries an exact
callee-created scope token and drained phase when its genuine checked close
leaves that scope open. A native caller can retry that one published close,
restore its actual parent and finish a matching returned get; no source body
is repeated and no ambient current scope is guessed. Forwarding preserves both
pending-attempt and close ledgers before a local child frame disappears.

Two genuine one-get Entity-B cases pass 114 native checks per host with normal
and NULL acquisition diagnostics. They select B only from the actual returned
record, preserve the first source/native failures and verify capture, close,
parent completion and owner/producer teardown. All common modes pass 913 native
checks per host; disjoint complete selections pass 112 unique Windows and 110
sanitized Linux tests. Tile refusal, consumed-nonOK and arbitrary unrecorded
callee graphs receive no runtime qualification from these cases. The Arrow
parent's final result/classifier/unwind integration remains open.

## Remaining composition

The finite query controls cover direct malformed loading, cold owned Tile
lookup/lazy construction and owned collision ID/metadata through their actual
entered bodies. Loading/import, generation/population and captured-provider
descendants still retain an explicit
external caller graph lifetime contract. General caller unwind propagation,
foreign A-query/Chunk.d B joint Store teardown, complete Arrow flight, pickup and
World-tick dispatch remain unfinished. No new original JVM, visible game,
whole-frame/audiovisual equality or sustained FPS result accompanies these checks.
