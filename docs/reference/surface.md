# Normal-dimension surface replacement

This note records Beta 1.7.3's normal surface pass after
[density and base interpolation](density.md). Both supplied binaries produced
the same 290 numerical snapshots, including seven independently captured
fixed cases. Exact-source local comparisons match C, client and server with
native MSVC and WSL GCC/Clang. All 24 local CTests passed without skips on each
toolchain. The [status note](../status.md) records broader acceptance and CI.
Caves, population, lighting, chunk finishing and save compatibility remain
separate work.

## Reference identity

| Side | SHA-256 | Generator | Improved octaves | Biomes / blocks |
| --- | --- | --- | --- | --- |
| Client | `af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c` | `yf` | `uf` | `kd` / `uu` |
| Server | `033a127e4a25a60b038f15369c89305a3d53752242a1cff11ae964954e79ba4d` | `pk` | `mp` | `gs` / `na` |

The supplied class files, inspected with `javap 17.0.17` on 2026-10-17, are the
primary evidence. The [binary audit](binaries.md) records their provenance
limits. The paired surface methods have the same operations, constants,
branches and offsets after corresponding class names and constant-pool
references are mapped. Numerical observations provide a separate check.

| Operation | Client descriptor | Server descriptor |
| --- | --- | --- |
| Surface | `yf.a(II[B[Lkd;)V` | `pk.a(II[B[Lgs;)V` |
| Preceding base | `yf.a(II[B[Lkd;[D)V` | `pk.a(II[B[Lgs;[D)V` |
| Full chunk entry | `yf.b(II)Llm;` | `pk.b(II)Lhi;` |
| Bulk improved octaves | `uf.a([DDDDIIIDDD)[D` | `mp.a([DDDDIIIDDD)[D` |

Offsets below apply to both sides and are relative to the named method. The
surface method ends at offset 604.

## Entry order and Random context

The complete generator constructor builds 82 improved-noise layers using
retained `Random j`. Surface channels `n/o` each have four layers, with
constructor calls/stores at 132/135 and 148/151. They precede the later
column-scale, column-bias and final channels. Surface uses these existing
layers without changing them. The [noise note](noise.md) and
[constructor sequence](density.md#constructor-order-and-random-continuation)
record their seeded draw order.

The full chunk entry has this order:

| Offsets in `b(II)` | Operation |
| --- | --- |
| 4-17 | Promote chunk X/Z separately to long, multiply by `341873128712L` / `132897987541L`, add, then call `j.setSeed` at 17. |
| 22, 36 | Allocate 32,768 block bytes, then construct the original chunk. |
| 65-81 | Refresh the actual 16x16 biome grid at wrapped int `chunk*16` origins and obtain processed temperature. |
| 93, 104 | Invoke base interpolation, then surface on the same block and biome arrays. |
| 119, 124 | Invoke the cave helper, then chunk finishing (`lm.c()` / `hi.b()`). |

The seed arithmetic wraps as Java long before Random's state masking. It has
no world-seed XOR; world seed already selected the persistent noise channels.
The chunk byte-array constructor, climate grid and base pass do not consume
generator `j`. Thus a prepared normal-prefix surface observation starts from
the chunk-reseeded state. A direct call on a fresh numerical generator starts
from the post-constructor state instead. Both contexts are captured separately.

## Scratch requests and layout

Surface makes all three octave requests before its first Random draw. Its
scale constant is 0.03125 at 4-7. Each chunk origin below is multiplied by 16
as a Java int before conversion to double.

| Call / store | Channel / field | Origins XYZ | Shape XYZ | Scales XYZ |
| --- | --- | --- | --- | --- |
| 39 / 42 | `n` / `r` (sand) | `(chunkX*16, chunkZ*16, 0)` | `(16,16,1)` | `(0.03125,0.03125,1)` |
| 77 / 80 | `n` / `s` (gravel) | `(chunkX*16,109.0134,chunkZ*16)` | `(16,1,16)` | `(0.03125,1,0.03125)` |
| 126 / 129 | `o` / `t` (thickness) | `(chunkX*16,chunkZ*16,0)` | `(16,16,1)` | `(0.0625,0.0625,0.0625)` |

Sand and thickness use the improved-noise general bulk branch. Gravel uses
its distinct height-one branch, which ignores origin Y, scale Y and layer
offset Y; the loaded 109.0134 therefore does not affect this request. Scalar
sampling or a generic two-dimensional substitute would change the contract.

The constructor supplies 256 positive-zero doubles in each of `r/s/t`.
The octave wrapper allocates a null array, or clears the full length of an
existing array before filling the requested prefix. Valid oversized arrays
retain identity and length, and their suffixes become positive zero. The
original wrapper does not grow a short nonnull array. The C interface checks
capacities before mutation rather than exercising that invalid original call.

Traversal is local Z outer, X inner (132-149), then descending Y from 127
through zero (294-300, 586-589). Biomes and fields use `X*16+Z` (152-177);
blocks use `(X*16+Z)*128+Y` (303-318). This is the existing base layout, but
the Z-first traversal determines which column receives each Random draw.

## Draws and stone replacement

For each column, these calculations occur in order:

| Offsets | Calculation |
| --- | --- |
| 177-200 | Add `nextDouble()*0.2` to sand field `r`; test strictly greater than zero. |
| 214-239 | Add the next `nextDouble()*0.2` to gravel field `s`; test strictly greater than 3. |
| 253-275 | Divide `t` by 3, add 3, add the next `nextDouble()*0.25`, then narrow with Java `d2i`. |
| 277-292 | Set remaining thickness to -1; load mutable top/filler locals from biome byte fields `p/q`. |

The three `nextDouble()` calls are at 182/219/266, giving 768 calls per pass.
At every Y, `nextInt(5)` at 328 occurs before reading the block at 352.
If `Y <= draw`, bedrock is written at 345 and the next Y begins. All 32,768
positions consume this bounded draw, including high air and non-stone cells.
Y zero always becomes bedrock; Y1-4 vary. API call totals do not fix the number
of internal Random steps because bounded rejection can consume extra steps.

Outside the bedrock branch, air resets only remaining thickness to -1
(355-363). Nonzero non-stone bytes retain their values without resetting the
counter (366-374). At stone with remaining==-1, the method does the following:

| Offsets | Behavior |
| --- | --- |
| 383-400 | Thickness<=0 selects air top and stone filler. |
| 403-433 | Otherwise, Y60 through Y65 inclusive reloads the biome's top/filler. |
| 435-455 | In that sea band, gravel selects air top and gravel filler. |
| 457-483 | Sand then selects sand top and filler, overriding gravel when both flags hold. |
| 485-504 | If Y<64 and selected top is air, replace top with stationary water. |
| 506-533 | Set remaining=thickness; write top at Y>=63, otherwise filler. This first placement does not decrement remaining. |

At later stone with remaining>0, the method decrements once and writes filler
(537-550). When that decrement reaches zero and filler is sand, it calls
`nextInt(4)` at 572 and changes filler to sandstone at 584. A zero result still
consumes the draw and changes the local filler. Remaining<=0 leaves later
stone unchanged, except that remaining==-1 re-enters first-stone handling.

Air does not restore top/filler. A later stone segment outside Y60-65 can
therefore retain an earlier local choice, including sandstone filler.
Bedrock and other non-stone blocks also leave these locals unchanged.
These details affect both block output and Random continuation.

## Block and material identities

The registries `uu/na` pass each ID into the constructor, which stores it in
base field `bn`. The relevant static initialization operands/stores are:

| Field | ID / role | ID operand / static store offsets |
| --- | --- | --- |
| `u` | 1 stone | 196 / 222 |
| `v` | 2 grass | 229 / 252 |
| `w` | 3 dirt | 259 / 280 |
| `A` | 7 bedrock | 393 / 425 |
| `C` | 9 stationary water | 467 / 495 |
| `F` | 12 sand | 583 / 606 |
| `G` | 13 gravel | 613 / 636 |
| `R` | 24 sandstone | 989 / 1010 |

Air is literal zero. Surface reads registry fields for bedrock at 338, stone
at 368/391, gravel at 448, sand at 462/476/558, water at 497 and sandstone at
577, then reads each object's `bn` value. Ice and other non-stone base bytes
are retained outside the bedrock decision.

Both biome constructors store grass/dirt defaults in `p/q` at 12/23. Catalogue
setup `kd/gs.a()V` replaces Desert `h` with sand/sand at 65/68 and Ice Desert
`j` at 85/88. The other initialized subclasses have no additional `p/q`
writes. Actual initialization confirmed all 13 identities: 2/3 except h/j,
which use 12/12. These are public mutable byte fields.

The library accepts 256 explicit top/filler byte-pattern pairs in column
order. The reference C probe's identity-to-default adapter uses the verified
fresh catalogue values only; Java reads actual selected objects' current
`p/q` and asserts the defaults. The comparison does not cover independently
mutated global biome materials. Separate constructed C tests exercise
explicit nondefault byte patterns.

## C storage contract

[surface.h](../../include/mc/world/terrain/surface.h) exposes the chunk-seed
adapter, separate noise/apply operations and the composed surface operation.
Calls allocate nothing. Noise sampling reads unmodified constructor state,
clears all three complete scratch spans and preserves descriptors. Apply
reads prepared fields and material pairs, advances a separate initialized
Random without reseeding, selectively writes the first 32,768 block bytes and
preserves the suffix. Copy `random_after_init` for direct constructor-context
calls; the embedded constructor continuation remains read-only.

Null, short and overflowing byte-span arguments fail before mutation; the
composed operation completes preflight before clearing fields. Mutable spans
and Random must be disjoint from each other, descriptors, constructor state
and read-only inputs. Those are caller storage preconditions. Apply's
read-only fields may alias each other. Original comparisons use finite
generated fields; arbitrary C input controls are labelled separately.

## Genuine setup and seven fixed observations

Original observations use actual client `wy/rh/fd` or server `dq/kp/dj` world
and normal-provider constructors, then construct a fresh actual `yf/pk` after
world setup. Server spawn can transitively invoke full chunk generation,
caves and finishing on its provider generator. The measured fresh generator
is separate: the later direct pass invokes climate, base and surface only.
The [density setup evidence](density.md#original-setup-and-observation-limits) describes the
route. These observations establish the specified numerical methods, not
complete world startup or chunk parity.

Seven paired fixed contexts were captured before the C comparisons:

| Seed / chunk | Surface Random context |
| --- | --- |
| 0 / (0,0) | Post-constructor and chunk-reseeded, separately |
| 1 / (0,0) | Post-constructor and chunk-reseeded, separately |
| 0 / (-1099,279) | Post-constructor and chunk-reseeded, separately |
| 0 / (-8,1) | Chunk-reseeded |

Every paired record matches full base/surface bytes, all 82 layers before and
after, actual biome/materials/climate, density `q`, complete `r/s/t`, retained
identities and copied-Random continuations. The first three locations also
match the prior independent density captures. The seventh case contains 306
gravel and 197 sandstone bytes; 102 actual field columns satisfy r>0 and s>3,
guaranteeing both perturbed flags without identifying which branch wrote
each cell. A bounded valid client search found that case at candidate 145;
fresh full captures then matched on both sides.

[Public fixed vectors](../../tests/world/terrain/surface_fixtures.h) preserve
all seven complete base/surface buffers through run-length encoding, all
5,439 scratch double bits, 1,792 material pairs and 56 before/after continuation
longs. Each block buffer has 32,768 prefix bytes plus three unchanged suffix
bytes, giving 458,794 decoded bytes across the fourteen buffers. Initializers
were parsed directly against both captures. Original source/class/tool/JAR,
capture and owned-file hashes remain in ignored local manifests. Separate
scope annotations preserve the original hashes while qualifying server setup's
transitive generation.

The [unit tests](../../tests/world/terrain/test_surface.c) separately label
constructed controls for air/non-stone retention, every-Y bedrock draws,
sea/thickness boundaries, sand/gravel precedence, sandstone draws of zero and
one, persistent filler across air gaps, arbitrary byte patterns, suffixes and
failed-C-call immutability. Their fixed Random schedules came from
`java.util.Random` alone, and six chunk-seed boundaries from integer arithmetic
modulo 2^64. They are not additional supplied-game observations.

## Numerical protocol and corpus

[SurfaceProbe.java](../../tests/reference/surface/SurfaceProbe.java) invokes
untouched supplied classes in an isolated class loader. It verifies actual
registry/material bindings, genuine setup, stored world seed and fresh
generator scratch state. Random observations serialize to memory and draw
from a copy, preserving the sampled generator's live state.

| Request | Meaning |
| --- | --- |
| `init seed` | New genuine setup and fresh measured generator; reset observation state. |
| `state` | Observe all 82 layers and both constructor/live Random continuations. |
| `buffers sand gravel thickness blocks` | Install valid bounded, deterministically prefilled spans; invalidate matching base/fields. |
| `base x z` | Refresh actual 16x16 climate/materials, then original base and `q425`; preserve Random and surface fields. |
| `seed x z` | Explicitly reseed live Random with the wrapped normal chunk formula. |
| `surface x z` | Original surface / composed C operation on matching base/materials. |
| `apply x z` | Original recomposed surface / standalone C apply using matching retained fields. |

There is no separate original apply method. An apply request requires a prior
matching surface and no intervening base/buffer replacement. Java recomputes
the fields and proves every bit equals their retained values before comparing
with C's read-only apply. Explicit reseeding and state observation preserve
that condition.

Snapshots contain full spans, raw binary64 bits, material bytes, climate/biome
identities, `q425`, last explicit chunk-seed bits and four continuation longs
for both live Random and the stored constructor context. The four-bit
retention mask follows sand/gravel/thickness/blocks. Init/buffers use zero
masks. Null climate/density/materials mean unobserved, including after server
spawn; they do not describe the provider generator's state.

The original-only corpus completed 290 snapshots per side on native Temurin
17.0.17+10: 34,345,620 bytes each, SHA-256
`4d3fcd34c94391b3a44b2dd3abfa954e9f22b522f6daac060dce4735dfc18947`.
Its 10 genuine setups per side include eight distinct seeds. Client roots
contained no files; server roots contained ten eight-byte session locks plus
directories. The measured pass explicitly invokes no save method.

Requests include signed-long seed extremes, neighboring/moved/cold/gravel
chunks, Java-int coordinate extremes, separately wrapped origins, repeated
mutated passes, reset/reseed equivalence, retained apply and minimum/oversized
buffers. Extreme coordinates exercise numerical methods, not full-world
reachability. Each side completed 105 surface-state assertions and 20 apply
field assertions; all seven prior fixed cases also match. Independent checks
recounted 50 equivalences, 1,861 preserved-field conditions, 114 preserved live
Random states and 105 advances.

| Recorded data per participant, including repeated observations | Count |
| --- | ---: |
| Binary64 values | 554,948 |
| Permutation entries | 1,511,424 |
| Live / constructor continuation longs | 1,160 each |
| Block bytes | 8,914,866 |
| Biome identities | 67,072 |
| Material bytes | 134,144 |
| Explicit chunk-seed observations | 225 |

All source/class/tool/JAR/request/fixture/output hashes and actual owned-world
inventories were independently checked. This original-only baseline preceded
the fresh original executions and separately built C probes below.

## Local C acceptance and reproduction

At source `f7ac19ebed8a14c499bd233c2dcf3fb0f2502360`, the finished helper passed
all 290 records independently for C/client, C/server and client/server on:

| Environment | C compiler | Java runtime/compiler | C sanitizers |
| --- | --- | --- | --- |
| Native Windows 10 | MSVC 19.51.36260.0 | Temurin 17.0.17+10 | Off |
| WSL Linux | GCC 13.3.0 | OpenJDK 21.0.12.1 | Address / undefined behavior |
| WSL Linux | Clang 18.1.3 | OpenJDK 21.0.12.1 | Address / undefined behavior |

Each surface manifest reports pass with no errors. Its 17 source hashes,
13 artifact hashes and 24 preserved-input hashes were independently checked,
including actual executable, compiled Java class, both JARs and JDK
runtime/compiler/modules. Every complete parsed snapshot agrees across all
nine participants; cardinalities match the table above.

All C/client/server controls agree, including 50 equivalences, 1,861 preserved
fields, 114 unchanged live-Random states, 105 advances, 255 cleared scratch
suffix checks, 174 retained block suffix checks and 20 apply-field checks.
Each original side also completes 105 surface-state assertions and 20
recomputed-field assertions. Each run's actual inventory matches its report:
ten client seed directories with no files, and ten eight-byte server session
locks plus directories. Server directory counts vary with genuine setup;
these records do not establish full-world equality.

Each toolchain's complete 24-test log has 24 passes and no skips, including all
seven optional original comparisons. The surface unit reports 1,114,102
checks and the six public surface helper guards pass on each. These are local
exact-source results; CI status is recorded separately.

Enable `MC_ENABLE_SURFACE_REFERENCE_TESTS` with both pinned JAR paths,
JDK 11+ runtime/compiler paths and `MC_REFERENCE_TESTED_COMMIT` identifying
the source used to build `mc_surface_probe`. Then run:

```powershell
ctest --test-dir build -R '^reference.surface$' --output-on-failure
```

Add `-C Debug` for a multi-configuration Windows build. The standalone
[helper](../../scripts/reference/surface/compare_surface.py) also accepts:

```powershell
python scripts/reference/surface/compare_surface.py `
  --executable build/Debug/mc_surface_probe.exe `
  --client b1.7.3.jar --server b1.7.3_server.jar `
  --java java --javac javac --work-dir .local/reference/surface
```

Supply `--tested-commit` with the full 40-hex source identity after rebuilding
that source. It is recorded as caller-supplied; the helper also hashes actual
sources and the selected executable. Each run freshly compiles Java and
compares every JSON field independently for C/client, C/server and
client/server, without numeric normalization.

The helper replaces the report with running, unavailable, fail or pass state
and a fresh run identity. It preserves requests/stdout/stderr, source/tool/
executable/class hashes, input-preservation hashes, per-side comparisons and
controls, and bounded file inventories. New unique original roots remain under
the ignored setup directory after success or failure. A completed mismatch
remains a failure if a later setup is unavailable. Missing prerequisites or
unavailable setup return 77; numerical, protocol, assertion, timeout outside
setup and comparison errors remain failures. Children have 60-second limits;
retained output is limited to 64 MiB per stream, with 512 MiB JVM heaps.

The six [public helper guards](../../tests/tools/surface/test_reference.py)
use generated placeholders/process results to check prerequisite reports,
input-preserving output handling and mismatch precedence. They run no original
classes, JVM or C probe.

Reproduce the static evidence without starting a world:

```powershell
Get-FileHash -Algorithm SHA256 b1.7.3.jar,b1.7.3_server.jar
javap -classpath b1.7.3.jar -c -s -p yf uf am kd uu lm
javap -classpath b1.7.3_server.jar -c -s -p pk mp ab gs na hi
```
