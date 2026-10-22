# Piston source map

The [verified original binaries](binaries.md) remain the behavioral authority.
The [native module](../world/tiles/piston.md) preserves the reached class, Tile,
Block and bounds identities. Mappings supply naming aliases only.

For navigation, [jonkadelic's mappings](https://github.com/jonkadelic/b1.7.3-official-mappings)
name client `uk` as `PistonTileEntity` and `uk.l()V` as `finalTick`.
[BetterThanUpdates' Yarn Beta mappings](https://github.com/BetterThanUpdates/Mappings)
name intermediary `class_283` as `PistonBlockEntity`. Those intermediary names
need their own obfuscated-name join; they do not replace the separate client
and server descriptors below. Neither name establishes behavior or coverage.

## Paired bodies

| Behavior | Client | Server | Reached order |
| --- | --- | --- | --- |
| Piston Tile | `uk extends ow` | `mu extends jh` | Separate class List `m`; fields ID `a`, metadata `b`, facing `c`, extending `i`, render flag `j`, current `k`, previous `l` |
| Default constructor | `uk()` | `mu()` | Empty base constructor1; return4 |
| Full constructor | `uk(IIIZZ)` | `mu(IIIZZ)` | Base1; ID6, metadata12, facing16, extending22, render flag28; return31 |
| Class initializer | `uk.<clinit>` | `mu.<clinit>` | ArrayList new0, constructor4, publication7, return10 |
| Interpolation | `uk.a(F)F` | `mu.a(F)F` | Only factor greater than1 clamps; previous9, current13, previous17; fsub20, fmul22, fadd23 |
| NBT read | `uk.a(nu)` | `mu.a(iq)` | Base2; Int ID9/store12, data19/store22, facing29/store32; Float progress40/stores44,47; Boolean54/store57 |
| NBT write | `uk.b(nu)` | `mu.b(iq)` | Base2; ID12, data22, facing32; previous progress42; extending52 |
| Explicit finalization | `uk.l()V` | `mu.k()V` | `fcmpg` previous5; current/previous stores13/16; World removal35; own invalidation39; World ID58; live moving Block61/64; notifying setter94; discard97 |
| Tile tick | `uk.n_()V` | `mu.g_()V` | Current to previous5; completion push21, removal40, invalidation44, ID63, live moving Block66/69, setter99; otherwise fadd111/store112, cap126, extending push153 |
| Private Entity push | `uk.a(FF)V` | `mu.a(FF)V` | Progress subtraction9/16; moving Block18, shifted box46, nearby60, isEmpty67, addAll80, iterator89, hasNext98, next108, actual movement156, normal clear165 |
| Moving removed hook | `ut.b(fd,III)` | `mz.b(dj,III)` | Actual Tile lookup5; Piston finalizer28; inherited container removal40 |
| Moving neighbor hook | `ut.b(fd,IIII)` | `mz.b(dj,IIII)` | World B1; true guard4; actual acquiring Tile getter12; return18 |
| Container removal fallback | `rw.b(fd,III)` | `lb.b(dj,III)` | Empty Block parent body6, then actual World Tile removal14 |
| Moving collision getter | `ut.e(fd,III)` | `mz.e(dj,III)` | Typed lookup6; interpolation21, extending28, optional fsub37, ID48, facing55, shifted helper58 |
| Moving state shape | `ut.a(xp,III)` | `mz.a(pb,III)` | Typed lookup6; Block array16, ID21/index24; null/same-object return38; carrier state shape46; progress52, extending59, optional fsub68, facing73; singleton stores96/117/138/159/180/201 |
| Typed Tile lookup | `ut.c(xp,III)` | `mz.b(pb,III)` | Actual BlockAccess Tile getter5, null/instanceof19, cast27 |
| Shifted geometry | `ut.a(fd,IIIIFI)` | `mz.a(dj,IIIIFI)` | ID0/self return; capture Block array16, index21, actual carrier getter27; null returns38; six coordinate stores |
| Direction class | `wj` | `od` | Four actual six-int arrays, independently initialized from Block/Tile classes |

Base Tile registers Furnace, Chest, RecordPlayer, Trap, Sign, MobSpawner, Music
and Piston in that order. Its Piston Class literal does not run Piston's own
initializer. Base read assigns coordinates in order. Base write resolves the
actual runtime Class's registered name before writing `id` and coordinates.
Factory construction is inside its Exception catch; concrete read46 is outside.

The client also declares render flag and offset methods with no matching server
methods. Native field inspection does not imply a server render method exists.
Server `mu.k()V` is finalization, distinct from client `uk.k()Z` flag inspection.

## Direction and in-place geometry

| Direction field | Values | Allocation / publication |
| --- | --- | --- |
| `a`, opposite | `1,0,3,2,5,4` | 2 / 28 |
| `b`, X | `0,0,0,0,-1,1` | 33 / 59 |
| `c`, Y | `-1,1,0,0,0,0` | 64 / 90 |
| `d`, Z | `0,0,-1,1,0,0` | 95 / 121 |

The initializer returns124. Earlier arrays remain published on a later native
allocation failure. The existing bed Block's four-by-two neighbor array belongs
to a different original class.

| Coordinate | Direction read | Index | Float multiply | Double subtract | Store |
| --- | ---: | ---: | ---: | ---: | ---: |
| minX | 45 | 50 | 54 | 56 | 57 |
| maxX | 66 | 71 | 75 | 77 | 78 |
| minY | 87 | 92 | 96 | 98 | 99 |
| maxY | 108 | 113 | 117 | 119 | 120 |
| minZ | 129 | 134 | 138 | 140 | 141 |
| maxZ | 150 | 155 | 159 | 161 | 162 |

Each current coordinate is read before its Direction getstatic. Conversion to
binary32 precedes multiplication; widening precedes subtraction. The helper
does not allocate a coordinate copy or invoke a generic add-based translation.
The separate MovingPiston state-shape body updates the Block singleton fields
in the order documented below. Tile tick and its private Entity push are
implemented with supported actual movement dispatches. Explicit finalization and
both MovingPiston removal and neighbor hooks also have constructed native
coverage in fresh local MSVC and sanitized GCC/Clang contracts. The neighbor
hook's true B guard skips acquisition; its false branch retains the real getter's effects,
including lazy invalid-Tile Map cleanup, before discarding the returned hold.
Earlier finalization finite state projections and source-backed completion/failure-prefix
joins match on all three compilers. Full native errors remain distinct from
Java Throwables. The retained interpolation and geometry observations below
keep their separate helper scopes.

## Singleton state shape

The native virtual dispatch implements all 16 paired `a(BlockAccess,III)`
declarations in the registered Block hierarchy, including the base empty body.
Client `xp` and server `pb` remain separate source interfaces. The current native
entries use an actual World as that BlockAccess receiver. The instruction PCs
below match on both sides; constant-pool entries and method names remain
side-specific.

| Client / server class | Reached state-shape body |
| --- | --- |
| `uu` / `na` | Empty return0 inherited by classes without an override |
| `ak` / `z` | Neighbor ID reads7/26 with short-circuit evaluation; setters69/106 |
| `bv` / `bc` | Metadata5; pressed/unpressed setters46/68 |
| `h` / `g` | Metadata5; masked facing14; setters64/78/92/106/120/134; invalid facing preserves fields |
| `jq` / `gi` | Metadata5; extended predicate14, masked facing22; directional setters72/86/100/114/128/142 or full shape155; invalid extended facing preserves fields |
| `jr` / `gj` | Metadata5; integer height arithmetic15..20, float division24, setter35 |
| `le` / `hc` | Metadata7; orientation helper12; shape helper15 (`le.d(I)V` / `hc.c(I)V`) with setters10/24/41/58/73 |
| `oi` / `iz` | Metadata5; facing and pressed depth select setters83/115/145/177; other facings preserve fields |
| `oq` / `jd` | Metadata6; shape helper11 (`oq.d(I)V` / `jd.c(I)V`) with setters10/35/52/71/88 |
| `pc` / `jj` | Metadata5; slope/flat setters32/46; `ph` / `jn` inherits this body |
| `ss` / `lu` | Full-shape setter7 |
| `uj` / `mt` | Standing flag1 returns7; otherwise metadata13, initial setter45 and directional setters68/89/112/133 |
| `um` / `mw` | Metadata5; integer width arithmetic16..21, float division25, setter48 |
| `ut` / `mz` | Held typed Tile, captured carrier, virtual state shape and six singleton stores as below |
| `ve` / `nj` | Private helper1 (`ve.r()V` / `nj.o()V`), shape setter8 |
| `xr` / `pd` | Metadata5; masked facing selects setters45/79/111/145 or default179 |

MovingPiston retains the actual typed lookup result through the complete call.
It captures the Block array before reading the Tile's carried ID and checking
the signed index. A null carrier or the same Block object returns at38. The
guard compares object identity; equality of numerical IDs does not establish
that identity, and a nonnull entry at ID zero still receives its virtual call.

The captured carrier's state-shape call completes at46 before progress52,
extending59 and facing73 are read from the held Tile. Factor positive zero still
executes the full interpolation arithmetic. Extending applies binary32
`1 - progress` at68. Each later carrier field read precedes its independent
Direction active use and signed index check.

| Singleton field | Carrier field read | Direction read | Index | Float multiply | Double subtract | Store |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| `bs`, minX | 81 | 84 | 89 | 93 | 95 | 96 |
| `bt`, minY | 102 | 105 | 110 | 114 | 116 | 117 |
| `bu`, minZ | 123 | 126 | 131 | 135 | 137 | 138 |
| `bv`, maxX | 144 | 147 | 152 | 156 | 158 | 159 |
| `bw`, maxY | 165 | 168 | 173 | 177 | 179 | 180 |
| `bx`, maxZ | 186 | 189 | 194 | 198 | 200 | 201 |

Direction components convert to binary32 before multiplication. The product
widens before binary64 subtraction from the current carrier field. Completed
carrier updates, Direction publications, singleton stores and World acquisition
effects survive later failures. This body neither obtains nor resets pooled
bounds; the collision helper's minX/maxX/minY/maxY/minZ/maxZ order remains separate.

The expanded `world.piston_geometry` contract includes state-shape cases for
carrier dispatch, object identity, progress order, partial stores, raw NaN
payloads and native holds. It passes 386 checks per compiler within the complete
public suites described in [testing](../testing.md). No retained original runtime
comparison of the state-shape body is established.
The earlier interpolation, collision geometry and finalization observations
retain their existing scopes.

## Original observations

### Active tick input family

A new original client/server family admits all 114 ordered caller inputs per
side, with 114 before and 114 after snapshots. Its recipe keeps a 34-case cold
prefix, an explicit warm boundary and five genuine fresh seed23 Worlds. Each
World receives 567 actual five-int quiet-air setters. Actors, duplicate
publication, parked owners, reused Tiles and shared class-List history remain
part of the same ordered recipe.

Both sides finish observation normally, retaining 24 actual action throws each.
Setup, snapshot, observer and close failures are zero. Independent offline
qualification checks full lossless stdout/stderr, source/tool/JAR and compiled
helper seals, command environments/working directories, actual recorded closure
controls, all input/action records and the admitted passive graphs. No subject
is restarted by qualification.

This qualifies the original observations. A separate MSVC native execution
retains both complete lossless streams, but its observer protocol is invalid:
production direct-handle messages split two observer records per side, and
34,978 records per side contain duplicate top-level keys. The raw streams and
failed protocol audit remain intact. No parser repair qualifies the whole stream.

A diagnostic restricted to intact records matches all 114 caller inputs and
normal-return versus failure classifications per side. All 456 selected raw
current/previous-progress comparisons per side match the qualified originals.
It does not compare Java exception classes with native status codes, all error
sites, complete graphs or constructor history. A reviewed record-buffer framing
fix remains unexecuted. Complete native tick protocol qualification and broader
comparison remain pending. Constructor Random/Math entropy and clocks are
natural inputs; seed23 does not fix all constructor or generation history.
Snapshots are non-atomic, and unadmitted class statics remain outside the graph.
Reference-field/array-component assignability and complete terminal inline
payloads rely on the pinned helper and genuine JVM rather than an independent
whole-heap validator. Native historical backing lifetimes and Java wrapper
identities require their own declared comparison limits.

### Active finalization family

A separately qualified original family retains 57 ordered finalization/removal
inputs per side, including reused owners and declared before/after projections.
Fresh native observations run those same inputs once per side/compiler on MSVC
and sanitized GCC/Clang. All six processes finish naturally with complete streams
and cleanup. Independent native qualification checks 4,809 before/after file-pin
joins, including the recorded working-source capsule, tools, products, graphs
and full streams.

Finite comparison matches all 342 rows and 684 snapshots, including 2,100
complete cached-array images and 1,050 full changed-offset/value delta lists.
Each side/compiler agrees on 42 normal void completions, three true Boolean
results and source-backed prefixes for nine actual null-World failures, two
full signed-ID -1 failures and one null-Block255 failure. The three prior callback
completion gaps and invalid Chest Map discrepancy are absent on all six runs.
Full actual native errors and Java Throwable records are retained separately;
these source-backed joins do not equate every descendant failure representation.

The joined projection covers source-mapped owners, Tile/Chunk/World fields,
ordered membership and light records, full cached arrays and action-preserved
Random/scalar state. It does not qualify whole-graph parity or constructor/
generation history. The observed Chest slots are empty; this family supplies no
arbitrary nonempty inventory claim. The [full projection limits and artifact pins](../wiki/Storage-and-reference.md#piston-finalization-and-neighbor-observations)
retain Registry, registration-map, Store/JDK, generation, entropy and missing
identity domains explicitly.

### Earlier interpolation and geometry families

The retained float family executes 19,140 records per side: 25 constructor,
18,292 interpolation, 676 writer and 147 reader records. Both payload streams
are identical. Immediate raw rereads preserve every supplied current, previous
and factor bit pattern. Eight wrong-type reads per side retain complete actual
ClassCastException traces. The composed interpolation results uniquely match
the tested left/right/right NaN-selection model among eight candidates. Writer
uses previous progress; semantic tags retain payloads and actual Java data-stream
encoding canonicalizes NaNs.

The direct geometry family executes 11,026 records per side after real registry
bootstrap, including all registered carriers, exceptional binary32 inputs,
wrapping coordinates, genuine null shapes and live pool aliases. Both payload
streams are identical. Its 10,870 positive results uniquely match the tested
left-NaN double subtraction and negative invalid-result model. Only the complete
composed bodies are observed; no universal scalar instruction contract is claimed.

Native replay selects 11,001 geometry records: 10,986 successful payloads and
15 reached body-failure prefixes. Three reflection receiver-admission failures
and 22 real null-World dependency failures are explicitly excluded with their
actual traces. Selected original getters leave World unused; native GAME Worlds
supply the actual pool owner. This comparison does not establish original
nonnull-World metadata or Tile traversal.

All original children exit naturally. The direct geometry capture's outer
producer retains a failed final JAR-pin bookkeeping attempt after both subjects
completed. A separate qualification verifies those after pins, complete streams,
commands, inputs and exceptions. The failed receipt and producer remain unchanged;
the subjects were not restarted. Replay verifies that evidence and reports the
limitation explicitly.

Both native replay tools pass for client and server on MSVC and sanitized
GCC/Clang: 18,292 exact interpolation records and the selected 11,001 geometry
records per side/compiler. This does not enlarge their stated helper scope.

## Retained-capture replay

Build targets `mc_piston_float_probe` and `mc_piston_geometry_probe`. Each tool
requires its separately retained original capture family and a fresh output
directory. These captures, original JARs and raw research stay outside version
control. The tools verify fixed capture identities and reject missing or
changed evidence; they do not launch the original game again.

```text
python tests/tools/world/tiles/compare_piston_interpolation.py --native <probe> --original <float-family> --output <new-output>
python tests/tools/world/collision/compare_piston_geometry.py --repo . --probe <probe> --original <geometry-family>/execution-a --output <new-output>
```

Reports retain exact inputs, raw streams, process completion, cleanup signatures
and explicit exclusions. Compiled source/build qualification remains separate
from the comparison tools' selected source and executable hashes.
