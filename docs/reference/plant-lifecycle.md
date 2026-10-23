# Plant lifecycle evidence

The unchanged client archive is SHA256
`af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c`;
the dedicated-server archive is
`033a127e4a25a60b038f15369c89305a3d53752242a1cff11ae964954e79ba4d`.
Complete paired bytecode bodies establish the methods below. The existing
[predicate map](plant-predicates.md) covers placement and survival dependencies.

| Body | Client / server | Reached operations |
| --- | --- | --- |
| Flower neighbor | `wb.b` / `nw.b(World,IIII)V` | Empty base8, final survival helper17 |
| Final flower/reed survival | `wb.h`, `ri.h` / `nw.g`, `ko.g(World,III)V` | Virtual survival6, metadata23, concrete drop26, notifying air write35 |
| Cactus neighbor | `or.b` / `je.b(World,IIII)V` | Same survival/drop/removal sequence, directly in the override |
| Inherited flower tick | `wb.a` / `nw.a(World,III,Random)V` | Final helper6; inherited only by flower, tall grass and dead bush |
| Reed/cactus tick | `ri.a`, `or.a` / `ko.a`, `je.a` | Above air7, height scan24/bn28, metadata51, upper notifying write74 then metadata reset84; otherwise increment99 |
| Mushroom tick | `tl.a` / `md.a` | Supplied nextInt100@4; candidate draws14/26/33/44; air59/survival73; extra draws83/95; fresh air110/survival124; notifying write141 |
| Crop tick | `ni.a` / `ij.a` | Parent8, growth light18, metadata31, factor49, binary32 division60/f2i61, nextInt62, notifying metadata78 |
| Crop growth factor | `ni.i` / `ij.h(World,III)F` | Eight reads10..102 before comparisons; absolute farmland loops236/static246/field249, moisture266, off-center division293, addition300, conditional half333 |
| Sapling tick | `he.a` / `eu.a` | Remote flag1, parent16, growth light26, nextInt30@38, metadata49, stage72 or grow86 |
| Sapling growth | `he.c` / `eu.b` | Metadata5 masked3, quiet clear18, selected constructor35/53/65, oak selector74/big constructor84, place98, live own ID110, quiet restore115 on ordinary false |
| Farmland neighbor | `vl.b` / `no.b(World,IIII)V` | Empty base8, above material18/predicate25, dirt static36/field39, notifying write42 |
| Farmland tick | `vl.a` / `no.a` | Supplied nextInt5@3, water scan15, rain28, moisture set41 or read52/decrement71; crop83, dirt94/97, notifying write100 |
| Farmland step-on | `vl.b` / `no.b(World,III,Entity)V` | World Random1/nextInt4@5, dirt16/19, notifying write22; Entity unused |
| Growth light | `fd.n` / `dj.l(III)I` | Delegate5 to `a(IIIZ)I`; asymmetric border24, live sampling statics43..79, five calls92..140, Chunk acquire215, late World.f236, Chunk call239 |

World and Random descriptors use client `fd` and server `dj`. Method names in
the table belong to their listed declaring class, including inherited bodies.
Plant native errors retain the current/deepest concrete hook and a complete
predicate child. Sapling errors also retain a nonrecursive tree site alongside
the actual lower mutation; they do not reconstruct a full Java stack.

## Native contracts

`world.plant_lifecycle` constructs actual client/server GAME Worlds and loads
sixteen serialized input Chunks. It exercises all represented plant kinds,
supported and unsupported neighbors, real Item publication, remote drop guards,
cross-border and full-height column removal, ignored Random arguments, growth branches,
sapling tree selection/restoration and late native failures. Farmland moisture
and its separate step-on body use actual World services. The original fluid
fixture supplies ownership and transport, not expected plant behavior.

The new World-query cases exercise signed sky subtraction, all four live
sampling statics and all five unique maximum positions, short-circuit failures,
borders, Y clamps, blank results and released leases. Existing piston regression
coverage exposed a Windows stack overflow during error initialization. Calling
the existing movement initializer in place reduced that initializer's MSVC
Debug frame from 158,744 to 34,456 bytes; the same failing piston test passes.

A second regression removes two adjacent 127-block cactus/reed columns. It
reproduced stack exhaustion on the default Windows Debug stack because every
recursive removal retained large mutation errors. Independent checked heap
workspaces now retain those diagnostics until descendants return. Acquisition
errors leave the stack before concrete setters recurse, and Registry-only
callers use a small factory. MSVC unwind records report allocation reductions
from 67,944 to 200 bytes for the World setter, 45,304 to 112 for the plant setter
and 22,936 to 472 for one neighbor call. The full-height case and all 17,595
native plant checks pass without a changed stack size or recursion cap.
Diagnostic allocation failures retain completed drops and earlier mutations;
all acquired workspaces, World holds and Chunk leases are released.

## Original dependency experiments

[`PlantLifecycleObservation.java`](../../tests/reference/world/PlantLifecycleObservation.java)
executes unchanged original Block and selected tree bodies against prepared
World services. The 59 declared [requests](../../tests/reference/world/plants/requests.tsv)
include every represented plant kind, growth and rejection branches, supplied
and World Random streams, drops and farmland. It retains full sparse maps,
boundary traces, setter events and selected deterministic Item fields.

The [native probe](../../tests/world/runtime/plants/reference_probe.c) loads real
GAME Chunks for each request. The comparison projects all block/metadata cells
in X/Z0..31 and Y0..127, both Random continuations and ordered Stack/delay/position
fields. Original boundary traces remain separate because the two World service
implementations differ. Default-constructor entropy and Item Math.random fields
are excluded explicitly; no metadata bits, array order or Random draws are
normalized away.

The first comparison returned normally for all 59 inputs on both sides but
failed five successful-tree maps per side. Every mismatch was a leaf decay bit:
27 oak, 45 taiga, 26 birch, 262 big-oak and 26 variant-three leaves. The prepared
original write service omitted old-leaf removal callbacks. The service now
follows each original Chunk overload's distinct store, old/new callback and
metadata order, including its remote guards. Its region-presence service models
the four loaded native Chunks. Both unchanged archives then returned normally
for all 59 requests, with zero mismatching projected fields per side. The failed
capture remains retained separately.

The public client/server fixtures are projected from those original captures.
Their provenance records archive, observer, request, raw capture and projected
fixture digests. Public replay checks 59 cases and 12 semantic corruption guards
per side: before/after metadata, drops, all eight Random-continuation fields and
request order. Another 154 malformed-input guards per side check exact types,
keys, signed integer ranges and bit-string formats on both comparison inputs.
These cover the discovered boolean-for-integer equality gap in Python. Replay
requires neither Java nor the original archives.

Fresh comparisons match both sides on MSVC 19.51, GCC 13.3 and Clang 18.1.3 with
Java 17. Complete local suites pass 183/185/185 tests respectively, with no
failures, errors or skips; Linux uses ASan/UBSan and leak checks. Each suite
includes the optional original comparison as well as the public native tests.
All three hosts pass mirrors and lifecycle intervals, and all 549 C/header files
pass clang-format 18. These finite experiments do not verify original World/Chunk
construction, general notification, natural population, persistence or whole-game parity.
