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
| Moving removed hook | `ut.b(fd,III)` | `mz.b(dj,III)` | Actual Tile lookup5; Piston finalizer28; inherited container removal40 |
| Container removal fallback | `rw.b(fd,III)` | `lb.b(dj,III)` | Empty Block parent body6, then actual World Tile removal14 |
| Moving collision getter | `ut.e(fd,III)` | `mz.e(dj,III)` | Typed lookup6; interpolation21, extending28, optional fsub37, ID48, facing55, shifted helper58 |
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
The separate MovingPiston state-shape update is a different body and remains
unfinished, along with tick and Entity pushing. Explicit finalization is now
implemented but has no fresh native execution or active original comparison.

## Original observations

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
