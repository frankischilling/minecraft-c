# Source ChunkRenderer compilation

The loaded authored saved-World frame runs the real dirty-chunk selector,
constructs its padded ChunkCache and compiles standard Block geometry through
the actual RenderBlocks and Tessellator. Empty and standard Block sections
complete. Current frame source calls real
[terrain submission](terrain-submission.md) at `px.a(FJ)V:486`; a normal N return
continues to the explicit U.b496 lighting-enable dependency. Submission is
implemented but native-unverified, with build/replay and hosted qualification
pending. Specialized Block shapes and TileEntity rendering remain unfinished;
this does not establish a playable frame. See the
[RenderBlocks contract](render-blocks.md) for geometry and finite comparisons.

## Selection and source effects

`n.a(Lls;Z)Z` reads the same dirty List and actual camera. The source's old
whole-queue sort branch is unreachable. The live branch selects nearby work,
replaces its original List slots with NULL before compilation, sorts the
temporary near List and compiles it in reverse. The two-cell far array's exact
decrements admit only one populated slot. It must not be replaced by a generic
nearest-two selection. Identity compaction and the original count comparison
can empty a duplicate-far queue while returning false.

Distance subtracts each integer section center from the camera's binary64
coordinate, narrows each difference separately and then performs binary32
squares and additions. MD compares visibility, distance and ordinal. Its
equal-ordinal result is -1, including equal identities. The native object sorter
follows the observed JDK17 TimSort calls, galloping, partial mutations and
comparison-contract failure. A contract failure remains a failed selector after
earlier NULL stores, without a fabricated compilation return.

The N caller clears DK.u only after that same compiler returns. Retained calls
own N and the exact camera. Completed child cache occurrences end before the
next compile; failed children retain their typed source, cache and cleanup
causes. Native cleanup does not compact a failed source queue or replay it.

## ChunkCache and compiler

The actual EW constructor stores its World in d and captures its X/Z origins, allocates the
rectangular Chunk array and acquires inclusive X-major/Z-minor coordinates.
Y endpoints do not change acquisition. Each cache cell retains the exact Store
Chunk identity. Block, metadata, TileEntity, light, brightness, material,
predicate and manager accessors use those captured identities, with each
method's own Y, bounds and NULL rules. They do not reacquire through World.
The TileEntity path invokes the canonical held-Chunk Map/factory method.
The native rectangular limit is 1,048,576 cells, separate from Java OOM timing.

A getter result becomes visible in the source cell and native occurrence ledger
only after the cache's independent Store lease succeeds. If that admission
fails, earlier published cells retain their leases and the failed cell remains
NULL. Failed-constructor accessors therefore cannot expose the unleased result.

Dirty DK compilation increments the shared counter, sets both pass flags,
clears the shared Chunk light marker, captures old TileEntity identities and
clears its actual B List. It then constructs EW and CV. Empty sections execute
the source TileEntity List operations and publish A/E; the N caller then clears u.
Nonair work begins C+pass, compiles Push/translation/scale/translation commands,
begins the same static NW and installs its actual translation. Standard
RenderBlocks calls append four vertices per face; NW performs its source quad
expansion. Successful passes draw, Pop, end the same list and reset translation
before A/E publication. A missing specialized geometry or TileEntity eligibility
child retains its reached prefix.

Compiled matrix commands execute on the canonical selected matrix and stacks.
Compilation copies operands without changing them. Failed native replay keeps
earlier effects. Aborting a failed pending definition ends its active native
binding but preserves its commands and failed-building record; it does not
invent `glEndList`. NW translation preserves raw double stores and the source
add-before-float-narrow vertex arithmetic.

## Sky changes and standard lighting

The concrete WorldAccess sky-change callback executes the whole `n.e()V`.
It rereads the live DK array and appends the same DK only when A is true and u
is false, then marks it dirty. Append precedes the dirty store. The general
range marker is not equivalent because it ignores A.

Standard lighting disable now includes U class active use: the shared Ge
namespace allocates the real 16-float buffer before `u.a()` disables lighting,
the two lights and ColorMaterial in order. Both Sky and the later terrain path
use this class lifetime and its retained first initialization failure.

## Evidence

The [cache fixture](../../tests/reference/client/chunk_cache/README.md) contains
950 unchanged-original rows. The [empty compiler fixture](../../tests/reference/client/chunk_compile/README.md)
compares a complete real DK/EW/CV call without GL. Native controls additionally
check real cache leases, failed-constructor access, nested source PCs, unchanged
outputs, nonair list completion, subsequent failed-family prefixes and checked
retirement. A one-stone section publishes 36 expanded vertices and the caller
clears u; rail and bed controls verify bounds change before their missing mesh
children. These prepared bodies have narrower scope than ordinary generated
sections and rendered terrain.

The [sky-change fixture](../../tests/reference/client/sky_changed/README.md)
contains eight original profiles. [Matrix and translation observations](../../tests/reference/client/compiled_matrices/README.md)
and the [lighting fixture](../../tests/reference/client/lighting/README.md)
separately qualify their declared class, method and
device scopes. The preceding standard Block checkpoint's captured back buffer
contains Sky without terrain or HUD. There is no new loaded-frame result for
the native-unverified submission increment. Full matched original frames remain
unqualified.

The [object-sort regression](../../tests/reference/client/render_global_update_sort/README.md)
checks 108 authored JDK17 cases, including every comparison and final or partial
array. It is separate from unchanged-game selector observations and does not
claim original MD execution for those authored comparators.

The [actual selector fixture](../../tests/reference/client/render_global_update/README.md)
compares 42 whole N-call profiles, including reverse compilation order, ordinary
false versus source failure, NULL camera short circuits, duplicate identities,
float narrowing and the genuine 85-element comparison-contract failure. Original
JDI receipts pin the unchanged loaded N/MD/DK/EW code and reached call PCs.
