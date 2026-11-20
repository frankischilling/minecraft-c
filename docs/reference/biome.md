# Climate fields and biome classification

This note records bytecode evidence and the isolated numerical comparison for
the Beta 1.7.3 climate managers and biome classifier. Its explicit-seed fixture
separates climate grids and classification from original world startup.
The [shared Biome bootstrap evidence](biome-registry.md) records actual
class-owned objects, lists and table aliases, plus the source order of reference
sampling.
Complete World, terrain, population and rendering acceptance remains separate.

## Reference identity

| Side | SHA-256 | Manager | Classifier | Simplex octaves |
| --- | --- | --- | --- | --- |
| Client | `af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c` | `xv` | `kd` | `ug` |
| Server | `033a127e4a25a60b038f15369c89305a3d53752242a1cff11ae964954e79ba4d` | `ph` | `gs` | `mq` |

Hashes and bytecode were checked on 2026-10-17 with JDK 17.0.17. The
[binary audit](binaries.md) records provenance limits. Shared manager methods
agree after mapping corresponding class names, seed getters and constant-pool
indexes; client-only `xv.b(II)D` is excluded. The classifier table builder and
both numerical selectors also agree after normalizing side symbols, reference
indexes and display padding. Constants, arithmetic, branches and offsets were
retained. Every offset below is relative to its named method on both sides,
except where a client-only method is identified.

## Channels and climate arithmetic

Constructors `xv.<init>(Lfd;)V` / `ph.<init>(Ldj;)V` create three independent
Random instances from wrapping signed-long world-seed products. Fields `e`, `f`,
`g` use multipliers 9871, 39811, 543321 and octave counts 4, 4, 2. Seed getters
are called separately at 14/41/68, multiplication occurs at 20/47/74, and octave
constructors at 25/52/79. The manager retains no world field. Noise-backed seed getters
`fd.s()J` / `dj.l()J` read world-info field `x` at 1 and call `ei.b()J` /
`ct.b()J` at 4; those methods return their stored `a:J` at 1/4.

| Channel | Each coordinate scale, exact loaded double | Frequency multiplier |
| --- | --- | --- |
| `e` | `0.02500000037252903` (promoted `0.025f`) | `0.25` |
| `f` | `0.05000000074505806` (promoted `0.05f`) | `0.3333333333333333` |
| `g` | `0.25` | `0.5882352941176471` |

Calls use the short octave overload, including its default amplitude-divisor
multiplier 0.5 and initial scale division by 1.5. The [simplex note](simplex.md)
records construction, coordinate layout and octave arithmetic.

In biome-grid methods `xv.a([Lkd;IIII)[Lkd;` /
`ph.a([Lgs;IIII)[Lgs;`, arguments are output, block X/Z origins and width/height.
Channel calls occur in `e`, `f`, `g` order at
49/81/113. Let E/F/G be those raw samples. The following binary64 operation
grouping is retained:

| Operation | Offsets |
| --- | --- |
| `U = (G*1.1)+0.5` | 142-157 |
| `wT=0.01`, `vT=1.0-wT` | 159-168 |
| `T0 = (((E*0.15)+0.7)*vT)+(U*wT)` | 170-194 |
| `wR=0.002`, `vR=1.0-wR` | 196-205 |
| `R = (((F*0.15)+0.5)*vR)+(U*wR)` | 207-231 |
| `T = 1.0-((1.0-T0)*(1.0-T0))` | 233-244 |
| Clamp T below 0, R below 0, T above 1, R above 1 | 246-284 |
| Store T in manager `a`, R in `b`; classify T/R | 286-317; classifier call 314 |

Manager `c` retains raw G. Rainfall has no corresponding complement/square
transform. The temperature expression performs both `1.0-T0` subtractions;
algebraic expansion or reassociation can change rounding. Clamp comparisons
leave unordered values unchanged. The [Java numerical notes](java-semantics.md)
describe the binary32/binary64 and wrapping rules used by this work.

Temperature-only methods `xv.a([DIIII)[D` / `ph.a([DIIII)[D` call `e` at 44
and `g` at 74, then perform the same temperature blend at 103-165 and clamp
below/above at 167-185. They write the supplied or newly allocated output at 192
and assign raw G to manager `c` at 77. With disjoint output, manager `a`, `b`
and `d` are unchanged. Both grid paths traverse X outermost, Z innermost, with
flat index `x*height+z`.

The concrete [Dimension temperature implementation](../world/temperature.md)
preserves these separate publication points. Output allocation19 is local until
the method returns209; raw temperature44 precedes auxiliary74 and `c`77. A later
auxiliary allocation failure therefore leaves earlier raw writes on a reused
input. The Overworld population caller stores the returned array in provider `w`
only at2684 in the [installed controller](../world/population/overworld.md).
Whole-controller reentry and natural population qualification remain unfinished.

The complete fixed overrides `hd.a([DIIII)[D` / `et.a([DIIII)[D` allocate output19
only for null/short input, read actual fixed `f`30, call prefix `Arrays.fill`33
and return37. They preserve a larger output suffix and do not access normal
noise or manager `a/b/c/d`. These bodies use the same descriptor on both sides.
Native allocation limits are separate controls, not original OOME observations.

## Reused arrays and adapters

The fields are reusable mutable arrays, with no coordinate keys or cache-hit
path in these methods. Each call recomputes samples. The octave wrapper clears
the entire reused double array before filling its prefix, so unused double
suffixes become zero. The direct biome method replaces its requested output
prefix and retains an oversized biome array's suffix. It does not assign field
`d`; convenience methods `xv.a(IIII)[Lkd;` / `ph.a(IIII)[Lgs;` pass `d` at 3,
call the direct method at 11 and store its result in `d` at 14.

Point methods `xv.a(II)Lkd;` / `ph.a(II)Lgs;` request a 1x1 convenience grid
at 5 and return element zero at 9. Chunk adapters `xv.a(Lyy;)Lkd;` /
`ph.a(Lqa;)Lgs;` shift each coordinate left by four at 6/12, then call the
point method at 13. These are Java int shifts with wrapping, not unbounded
coordinate multiplication.

The biome method allocates its output from width*height at 0-22, but passes
width twice to each noise call at operand loads 36/38, 68/70 and 100/102.
Temperature-only passes both requested dimensions independently. The current
biome scope uses square grids; rectangular biome behavior remains static
evidence. Original array failures are outside numerical comparison scope.

Client-only `xv.b(II)D` samples `e` at 1x1 with the same promoted 0.025f scales
but frequency multiplier 0.5, calling at 24 and storing raw output in manager
`a` at 27. It returns `a[0]` at 35/36 without the climate blend or clamp. Client
`fd.a(Lsn;F)Lbt;` calls it at 68, converts to float at 71, obtains a point biome
at 82 and calls `kd.a(F)I` at 87 for sky color. Server `ph` has no corresponding
raw-temperature method; server `gs` has no corresponding color method.

## Float table and identities

Static initializers `kd.<clinit>()V` / `gs.<clinit>()V` construct these objects
before allocating the 4096-slot table at 311-317 and invoking its builder at 320.
The methods return object identity. These classes define no numeric biome-ID
registry; any C integer encoding must be distinguished from the original fields.

| Field / original name | Initializer store offset | Observed table entries, each side |
| --- | --- | --- |
| `a` / Rainforest | 22 | 13 |
| `b` / Swampland | 47 | 138 |
| `c` / Seasonal Forest | 67 | 57 |
| `d` / Forest | 92 | 839 |
| `e` / Savanna | 112 | 518 |
| `f` / Shrubland | 132 | 397 |
| `g` / Taiga | 160 | 473 |
| `h` / Desert | 183 | 54 |
| `i` / Plains | 203 | 32 |
| `j` / Ice Desert | 234 | 0 |
| `k` / Tundra | 262 | 1575 |
| `l` / Hell | 285 | 0 |
| `m` / Sky | 308 | 0 |

Builders `kd.a()V` / `gs.a()V` iterate i/j from 0 through 63, storing at
`i+j*64` at 19-24. They divide each index by `63.0f` using binary32 operations
at 26-34 and call the float classifier at 35. They also set Desert/Ice Desert
top/filler block bytes at 51-88, so explicitly rebuilding the table has other
state effects. Actual initialized tables matched on both sides. Encoding each
entry as its one-letter field label in flat array order, with no newline, gives
SHA-256 `5fa3a4d73e11ea4f2fb703e7703df5cb13f3ef64a7780746f60da74d21ac5e51`.
All 13 catalogue objects were distinct; Ice Desert, Hell and Sky are constructed
but absent from the climate table and decision-tree returns.

Float classifiers `kd.a(FF)Lkd;` / `gs.a(FF)Lgs;` first compute Q=R*T in
binary32 at 2/3, then apply these ordered branches with float constants:

| Condition after earlier branches fail | Identity | Offsets |
| --- | --- | --- |
| T<0.1f | Tundra | 4-14 |
| Q<0.2f, T<0.5f | Tundra | 15-32 |
| Q<0.2f, T<0.95f | Savanna | 33-43 |
| Q<0.2f | Desert | 44-47 |
| Q>0.5f, T<0.7f | Swampland | 48-65 |
| T<0.5f | Taiga | 66-76 |
| T<0.97f, Q<0.35f | Shrubland | 77-94 |
| T<0.97f | Forest | 95-98 |
| Q<0.45f | Plains | 99-109 |
| Q<0.9f | Seasonal Forest | 110-120 |
| Otherwise | Rainforest | 121-124 |

Double selectors `kd.a(DD)Lkd;` / `gs.a(DD)Lgs;` multiply T/R separately by
63.0, cast to Java int at 5/13 and return table entry `i+j*64` at 25-28. Thus
manager classification uses double blending, quantization, a float-built table
and a float decision tree. Applying the tree directly to unquantized doubles
changes results. Finite inputs in [0,1] address valid table cells; the original
selector has no explicit clamp or bounds check.

## Setup and remaining scope

Protected constructors `xv.<init>()V` / `ph.<init>()V` only call Object at 1
and return at 4, leaving the arrays and channels null. The isolated fixture
invoked those constructors and assigned actual `ug` / `mq` instances to `e/f/g`,
using separate Java Random instances, the verified seed products and counts.
For a fixed stored world seed, this produces the channel state prescribed by
the world-taking constructor: that constructor only assigns those three fields,
and each normal getter returns the stored seed. This is a static construction
argument, not an executed comparison of world constructors or provider startup.

The original-only observation captured the actual catalogue/table, nine float
and ten double selector cases, and 20 shared manager snapshots per side, plus
the client raw-temperature observation. It exercised
valid 2x3 temperature grids, 2x2/4x4 biome grids and 4x4-to-point-to-4x4 reuse
under seeds 0, -1, signed-long minimum and maximum. Shared results matched
exactly between sides. The helper constructed no world/storage instance; C was
not invoked. These observations establish the exercised fixture and cases.

Classifier initialization has broader dependencies. Biome constructors read
block catalogues `uu` / `na` at 5/16; those initializers access item registries
`gm` / `ej` at 3398 and call statistics `jl.b()V` / `gg.b()V` at 3674.
Statistics initialization calls achievements `ep.a()V` / `cy.a()V` at 624,
and its `d()V` can reach recipe singletons. Achievement initializers `ep` / `cy`
print at 451; recipe constructors `hk.<init>()V` / `ey.<init>()V` print at
3234. Translation constructors `nh.<init>()V` / `ii.<init>()V` call
`Class.getResourceAsStream` at 23/37 for `/lang/en_US.lang` and
`/lang/stats_US.lang`; both sides' `b.<init>()V` call it at 27 for
`/achievement/map.txt`. All three resources are present in both pinned archives.
Observed initialization banners were 16 achievements and 151 recipes on each
side, captured separately from numerical records. This static trace and isolated
run do not establish every transitive initializer or world startup.

Overworld ChunkProvider methods `yf.b(II)Llm;` / `pk.b(II)Lhi;` request a square 16x16
biome grid at 65, then read manager temperature `a` at 78 before block and
surface calls at 93/104. Population methods `yf.a(Lcl;II)V` /
`pk.a(Lbl;II)V` request a point biome at chunk origin+(16,16), call 33, and
later a 16x16 temperature grid at origin+(8,8), call 2681. These anchors establish
consumers and call order. Density combination, top/filler application, snow,
spawn lists, feature generators, constant-dimension managers and world behavior
remain outside climate/classifier comparison.

Reproduce the bytecode observations without starting a game or world:

```powershell
Get-FileHash -Algorithm SHA256 b1.7.3.jar,b1.7.3_server.jar
javap -classpath b1.7.3.jar -c -s -p xv kd ug xa yf fd ei
javap -classpath b1.7.3_server.jar -c -s -p ph gs mq os pk dj ct
javap -classpath b1.7.3.jar -c -s -p uu gm jl ep hk nh b
javap -classpath b1.7.3_server.jar -c -s -p na ej gg cy ey ii b
```

## C comparison and reproduction

Enable `MC_ENABLE_BIOME_REFERENCE_TESTS` with both pinned local JAR paths,
JDK11+ runtime/compiler paths and `MC_REFERENCE_TESTED_COMMIT` identifying the
source used to build `mc_biome_probe`. Then run
`ctest --test-dir build -R '^reference.biome$' --output-on-failure` (add
`-C Debug` for a multi-configuration Windows build). The standalone helper is
`scripts/reference/biome/compare_biome.py`; its executable/JAR/JDK/work-directory
and tested-commit arguments follow the other original comparison families.

The corpus contains 2879 common snapshots and 32 separate client-only snapshots.
It compares C/client, C/server, client/server and client-only/C separately.
Every record includes all six array descriptors, lengths, exact binary64 bits,
identity letters and retained/returned/pairwise-alias masks. The table record
contains all 4096 actual initialized entries, and the catalogue record contains
all thirteen original names. Direct float and double lookup are kept distinct.
Fixture-only `init`/`buffers` commands have a fixed zero retention mask; method
calls record actual identities. Allocation address values are never compared.

Controls independently check repeated calls, float-versus-table distinctions,
unchanged fields, temperature-path agreement and point/cache agreement on each
side. Valid requests cover seed-product wrapping, signed coordinate boundaries,
squares 1/2/4/16, temperature rectangles, fresh/retained/oversize arrays,
large-small-large sequences and allocated-empty arrays. Client-only raw samples
and preserved fields have a separate phase without a server counterpart.

Reports record exact source, tools, runtime, executable, inputs and artifact
hashes. Every invocation publishes fresh running/unavailable/fail/pass status;
classes compile in a fresh directory, and output paths cannot contain or
overwrite inputs. Missing prerequisites or isolated original setup return77;
method execution failures and comparison differences return1. Initialization
diagnostics are captured separately from numerical output. Public guard tests
use generated placeholders without executing original classes.

The [status note](../status.md) records exact tested source and platform results.
These comparisons establish the specified numerical fixture and state
sequences, not world startup, complete terrain or all biome gameplay behavior.
