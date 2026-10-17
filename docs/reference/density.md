# Normal-dimension density and base blocks

This note records bytecode evidence for Beta 1.7.3 density composition and the
following base-block interpolation. Genuine original setup and three fixed
numerical cases succeeded on both sides with Temurin 17.0.17+10. Initial C
comparisons match the full corpus; final exact-source acceptance is pending.
The [status note](../status.md) records
tested source and environments. Surface replacement, caves, population,
lighting and complete terrain remain outside this pass.

## Reference identity

| Side | SHA-256 | Generator | Improved octaves | Climate | Blocks |
| --- | --- | --- | --- | --- | --- |
| Client | `af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c` | `yf` | `uf` | `xv` | `uu` |
| Server | `033a127e4a25a60b038f15369c89305a3d53752242a1cff11ae964954e79ba4d` | `pk` | `mp` | `ph` | `na` |

Hashes and bytecode were rechecked on 2026-10-17 with JDK 17.0.17. The
[binary audit](binaries.md) records provenance limits. The complete constructor,
private density method and base interpolation method agree after mapping side
class names and constant-pool references and normalizing display padding.
Operations, constants, branches and offsets were retained. This is static
correspondence; numerical agreement requires separate observations.

| Operation | Client descriptor | Server descriptor |
| --- | --- | --- |
| Constructor | `yf.<init>(Lfd;J)V` | `pk.<init>(Ldj;J)V` |
| Density | `yf.a([DIIIIII)[D` | `pk.a([DIIIIII)[D` |
| Base interpolation | `yf.a(II[B[Lkd;[D)V` | `pk.a(II[B[Lgs;[D)V` |
| Full chunk consumer | `yf.b(II)Llm;` | `pk.b(II)Lhi;` |

Offsets below are relative to the named method and apply to both sides unless
separate methods are identified.

## Constructor order and Random continuation

The generator stores its world at 56, constructs `Random(seed)` at 65 and stores
it in `j` at 68. All eight octave objects share that Random and are constructed
in the following order:

| Field | Improved layers | Octave constructor / field store | Use |
| --- | ---: | --- | --- |
| `k` | 16 | 82 / 85 | Low density channel |
| `l` | 16 | 99 / 102 | High density channel |
| `m` | 8 | 116 / 119 | Mixing channel |
| `n` | 4 | 132 / 135 | Later surface work |
| `o` | 4 | 148 / 151 | Later surface work |
| `a` | 10 | 165 / 168 | Column scale |
| `b` | 16 | 182 / 185 | Column bias |
| `c` | 8 | 199 / 202 | Later consumer |

These are 82 improved layers. Each `am/ab.<init>(Ljava/util/Random;)V` makes
three `nextDouble()` calls at 15/27/39 and 256 bounded `nextInt()` calls in
92-135, including bound one. The resulting totals are 246 and 20,992 API calls.
Bounded rejection can consume extra internal Random steps. The
[noise note](noise.md) records layer construction and continuation semantics.
Omitting `n/o` changes the column channels; omitting `c` changes the following
Random state. Density and base interpolation themselves draw no random values.

The cave helper has its own Random: base constructors `fv.<init>()V` /
`dy.<init>()V` construct it at 15 and store it at 18. Whole-world constructors
also initialize time and unseeded bookkeeping state. Those values are separate
from the explicit seeded channel sequence above.

## Climate and five density channels

The normal base pass requests density at integer origins `chunkX*4`, 0,
`chunkZ*4`, dimensions XYZ=5x17x5 (call 44, store `q` at 47). The resulting
425 doubles have X/Z/Y flat order: `(x*5+z)*17+y`.

Before sampling noise, private density obtains the world's manager at 29/41
and loads processed temperature `a:[D` at 32 and rainfall `b:[D` at 44. The
world getters `fd.a()Lxv;` / `dj.a()Lph;` load provider `t` at 1 and its manager
`b` at 4. These arrays must describe the intended current 16x16 climate grid.
The [climate note](biome.md) records how those mutable arrays are produced.

The density method loads double 684.412 into two scale locals at 15-23 and
makes these five requests, in order:

| Call / scratch store | Octave field | Scratch, normal length | Scales |
| --- | --- | --- | --- |
| 74 / 77 | `a`, 10 layers | `g`, 25 | X/Z: 1.121, 1.121; final argument 0.5 |
| 105 / 108 | `b`, 16 layers | `h`, 25 | X/Z: 200, 200; final argument 0.5 |
| 151 / 154 | `m`, 8 layers | `d`, 425 | XYZ: 684.412/80, 684.412/160, 684.412/80 |
| 185 / 188 | `k`, 16 layers | `e`, 425 | XYZ: 684.412, 684.412, 684.412 |
| 219 / 222 | `l`, 16 layers | `f`, 425 | XYZ: 684.412, 684.412, 684.412 |

The short `uf/mp.a([DIIIIDDD)[D` overload forwards Y origin 10, Y size one
and Y scale one at 2-19; its final double is unused. Thus `g/h` use the
improved-noise height-one branch, not simplex amplitude rules. The other
requests use `a([DDDDIIIDDD)[D` and its general bulk branch, including cached
gradient operands. Independent scalar Perlin samples do not reproduce these
bulk paths. Each octave request starts factor one and halves it per layer,
using the factor as amplitude divisor; there is no final normalization.

The density loops traverse X, then Z, then Y (241/263/533). Climate stride is
integer `16/sizeX` at 231-236, used on both axes. Sampling
`index*stride+stride/2` at 248-280 gives positions 1,4,7,10,13 for the normal
five-point axes. Climate indexing is `sampleX*16+sampleZ` at 282-305.

The following names describe intermediates for this note. Keep each listed
arithmetic step and branch in order:

| Offsets in density | Operation |
| --- | --- |
| 282-335 | Load T/R; multiply R*T; subtract from one; square twice; subtract from one again to obtain climate multiplier Q. |
| 337-369 | Form A=`(g+256)/512`, multiply A by Q, clamp A above one. |
| 371-410 | Form B=`h/8000`; if negative, negate then multiply by 0.3; multiply by three, then subtract two. |
| 412-458 | If B<0: divide by two, clamp below -1, divide by 1.4, divide by two again, set A to zero. |
| 463-479 | Otherwise clamp B above one, then divide by eight. |
| 481-525 | Clamp A below zero, add 0.5; multiply B by sizeY then divide by 16; center=`sizeY/2+B*4` in double. |
| 543-571 | Vertical term=`((double(Y)-center)*12)/A`; multiply negative results by four. |
| 573-616 | Low=`e/512`, high=`f/512`, mix=`((d/10)+1)/2`. |
| 618-657 | Mix<0 selects low; mix>1 selects high; otherwise low+(high-low)*mix. Exact zero/one take the interpolation branch. |
| 659-664 | Subtract the vertical term. |
| 666-703 | Apply the top fade when Y>sizeY-4. |
| 710-714 | Store and advance the flat/Y counters. |

The top fade converts `Y-(sizeY-4)` to float at 682, divides by float three at
685, then promotes to double at 686. For normal Y14/15/16 the factors are
promoted binary32 thirds. The final expression is density*(1-F)+(-10)*F in
that order (689-703). Double thirds or a reassociated expression change the
operation graph. Java 17 specifies floating formats and evaluation rules in
[JLS 15.4 and 15.7](https://docs.oracle.com/javase/specs/jls/se17/html/jls-15.html#jls-15.4).

## Base interpolation, layout and IDs

Base interpolation visits coarse X/Z cells 0..3 and coarse Y cells 0..15
(53-77). It loads four lower-Y corners at 109/136/163/190 and computes their
next-plane differences times 0.125 at 193-323. Each cell has eight sub-Y
steps. At each sub-Y step, opposite-X differences times 0.25 define two X-edge
increments (335-366); at each of four sub-X steps, a Z difference times 0.25
defines the four sub-Z values (413-438).

Execution order is coarseX/coarseZ/coarseY/subY/subX/subZ. Values advance by
repeated additions: Z at 552-557, X edges at 565-577, four Y corners at 585-611.
A closed trilinear formula can round differently.

Block dimensions XYZ are 16x128x16. The 32,768-byte flat layout is
`(x*16+z)*128+y`, equivalently `(x<<11)|(z<<7)|y`; Y is fastest, as in density.
The initial index is formed at 377-406 and advances by 128 along Z at 545-550.
Temperature is read at `x*16+z` at 441-462. The biome-array argument is unused
by this method.

| Offsets in base interpolation | Ordered decision |
| --- | --- |
| 464-465 | Start with air, literal ID zero. |
| 467-518 | Below Y64 use stationary water, except Y63 with temperature strictly below 0.5 uses ice. |
| 523-536 | Density strictly greater than zero overrides air/fluid with stone. Exact zero retains the preceding choice. |
| 543-544 | Convert the ID to byte and store. |

Both block registries establish the following IDs. Their base constructor
`uu.<init>(ILln;)V` / `na.<init>(ILhj;)V` stores its supplied ID in `bn:I` at 102.

| Field / block | ID | Registry `<clinit>()V`: ID operand / constructor / field store |
| --- | ---: | --- |
| `u`, stone | 1 | 196 / 198 / 222 |
| `C`, stationary water | 9 | 467 / 472 / 495 |
| `aU`, ice | 79 | 2784 / 2788 / 2811 |

The base pass reads ice at 504/507, water at 515/518 and stone at 530/533.
Bedrock, biome top/filler blocks and caves are applied by later methods.

## Consumer order, reuse and bounds

Full chunk methods `yf.b(II)Llm;` / `pk.b(II)Lhi;` reseed generator `j` at 17
from wrapping long products of chunk coordinates by 341873128712 and
132897987541. Existing octave objects keep their constructor state. The methods
allocate blocks at 22, construct a chunk at 36, request the 16x16 biome grid at
65, read current manager temperature `a` at 78, then call base interpolation at
93, surface replacement at 104, caves at 119 and a chunk update at 124.

Climate origins use wrapping Java int `chunk*16` (56/60); density origins use
separate wrapping int `chunk*4` (32/37 in the base method). Deriving one from
the other after overflow changes the domain. The existing
[Java numerical notes](java-semantics.md) also apply to large noise coordinates.

Private density allocates only a null output (8-14), overwrites its requested
prefix and retains an oversized suffix. In contrast, every octave bulk call
clears the whole supplied scratch array at 18-36 before accumulating samples.
Base interpolation overwrites all 32,768 block bytes, including air, and
retains any longer suffix. Neither method grows a short nonnull output.

The normal valid precondition is a current processed 16x16 climate grid,
425 density slots, 25-slot `g/h`, 425-slot `d/e/f`, and 32,768 block slots.
The private general-shape signature supplies no safe arbitrary-grid contract:
its climate stride and capacities assume its consumer. Bounded C interfaces
can validate the fixed normal shape and all capacities before writes. Original
short-array or incompatible-shape faults need not be executed to define that
contract. An intervening manager call can change the climate input state.

The [C interface](../../include/mc/world/terrain/terrain.h) implements this fixed
shape with caller-owned state and borrowed buffers. `mc_terrain_normal_init`
constructs all channels and retains Random continuation; density, base and
combined generation have separate entry points. All validate null pointers,
minimum lengths and double-span byte-size overflow before mutation. Combined
generation validates blocks before sampling changes workspace or density.
Initialized state and disjoint mutable storage are caller preconditions;
pointer overlap is not checked. Read-only inputs may overlap each other.
Sampling calls allocate nothing and leave constructor state unchanged. Finite exactness
assumes binary32/binary64 round-to-nearest without contraction; exact NaN
payload identity is not promised.

## Original setup and observation limits

Untouched density calls through its actual world to obtain climate arrays.
The earlier climate-only seeded fixture therefore does not establish density
setup. An implementation can accept explicit climate arrays; the original
observation still needs a legitimate world/provider/manager context.

The client has an original no-op handler `wy implements wt`: its metadata,
chunk-loader and file getters return null at 0-1; save/lock methods return at
zero. The public real-world constructor `fd.<init>(Lwt;Ljava/lang/String;Lxa;J)V`
accepts this handler, a normal `rh` provider and seed. It constructs world
metadata at 275, map storage at 292, attaches its provider at 300, constructs
its chunk provider at 305, updates daylight at 312 and rain flags at 316, then
returns at 319 without spawn search. Original `mm.<init>(Lnb;JI)V` uses this
same handler/constructor at 1-16; its later network dependency is unnecessary
for direct `fd` construction.

Client map storage `hc.b()V` asks for `idcounts` at 23 and skips file reads
when the handler returns null at 30. `fd.b()Lcl;` accepts the null loader,
obtains a generator from the provider at 24 and constructs `ok` at 27.
Provider `xa.a(Lfd;)V` builds climate at 6; `xa.b()Lcl;` constructs
`yf(world,world.s())` at 15. These are static dependencies for a genuine
constructor route; successful execution and its resource closure must be
recorded separately.

Normal provider constructors `rh.<init>()V` / `kp.<init>()V` only call their
superclass at 1 and return at 4. Provider initialization `xa.e()V` / `os.c()V`
fills sixteen brightness floats at 5-44 without calls. World static initializers
`fd/dj.<clinit>()V` only set static `A` to zero at 1. Chunk-provider constructors
`ok/jb` construct blank `li/hg` chunks at 49, then store the supplied loader at
62 and generator at 67; they do not invoke the loader in those constructors.

The server's original `dq implements om` uses writable storage. Its constructor
creates world/data directories at 28/70 and invokes the session-lock writer at
87; `dq.f()V` opens the output at 23 and writes at 35. Genuine fresh setup can
use it with normal provider `kp` and
`dj.<init>(Lom;Ljava/lang/String;JLos;)V`. This constructor reads metadata at
281, creates fresh metadata at 373 when absent, attaches the provider at 398,
constructs its chunk provider at 403 and searches for spawn at 415 for a new
world. Provider `os.a(Ldj;)V` builds climate at 6; `os.b()Lbl;` constructs
`pk(world,world.l())` at 15. Spawn search `dj.c()V` calls provider `os.a(II)Z`
at 18, which calls the world's block query `dj.a(II)I` at 6. This route can
perform chunk generation beyond the numerical observation pass.

The world's `dj.a(III)I` obtains a chunk at 48; `dj.c(II)Lhi;` dispatches to
its provider at 6. On a missing chunk, `jb.b(II)Lhi;` calls `c(II)Lhi;` at 28,
whose generator fallback is at 78. Thus spawn work can reseed the provider's
generator and fill its climate/scratch arrays before world construction
returns. Constructor-state observations should use a fresh actual `yf/pk`
constructed with the legitimate world and explicit seed, then regenerate the
actual manager's 16x16 grid before each measured density/base pass.

Both genuine routes succeeded for seed zero and chunk (0,0) on native
Temurin 17.0.17+10. Client construction used `wy/rh/fd`; the inventoried fresh
root contained no child files. Server construction used `dq/kp/dj`, including
the original spawn search; it created directories and one eight-byte
`session.lock` beneath its fresh root. No save method was explicitly called.
The processes completed in approximately 0.43 and 0.61 seconds respectively.
These observations establish the exercised constructors, not complete game
or server startup, general spawn bounds or save compatibility.

After setup, the observation constructed fresh actual `yf/pk` instances and
read all 82 layers: 246 offset values and 41,984 permutation entries. Four
following `nextLong()` values were obtained from an in-memory serialized clone
of the original Random. Each numerical pass was preceded by the actual
manager's 16x16 biome-grid method. The observation captured three climate
arrays, five scratch arrays, 425 density values, all 32,768 base bytes and the
base method's 425-value `q` array. Valid oversized buffers showed three retained
density/block suffix elements and three cleared elements per scratch suffix.
The direct density prefix matched `q`, and Random continuation was unchanged
after the numerical calls. No replacement world, altered class or recompiled
original implementation was used.

The two 103-record outputs were byte-for-byte equal, each 271,170 bytes with
SHA-256 `7d41c79303f869eefa5141c69ed51bcd5b6a2cc3a7e696db71fdfc70a57c0ea4`.
Local setup manifests pin the supplied JARs, runtime/compiler/module files,
observation source/class and captures; their hashes were independently checked.
Initialization diagnostics were captured separately from numerical records.
Earlier catalogue initialization reaches statistics, recipes, achievements and
bundled translation resources, as recorded in the
[climate setup evidence](biome.md#setup-and-remaining-scope). This first capture
does not establish all transitive initialization behavior or broader density
coverage. The full original corpus and initial C comparisons are described below.

The paired original observations also include a cold sea-surface case and a
warm water case. Their complete records matched between sides before fixed C
expectations were extracted:

| Seed / chunk | Air | Stone | Water | Ice |
| --- | ---: | ---: | ---: | ---: |
| 0 / (0,0) | 10,349 | 22,419 | 0 | 0 |
| 0 / (-1099,279) | 16,288 | 16,362 | 17 | 101 |
| 1 / (0,0) | 16,384 | 14,088 | 2,296 | 0 |

Each case has 32,768 base bytes; the cold case's ice is at Y63 with processed
temperature below 0.5. All three density prefixes match their base method's
`q` values. Public fixed vectors preserve each case's 512 climate input bits,
425 density bits, 1,325 scratch bits and every base byte through run-length
encoding. A seed-zero constructor fixture preserves all 246 offset bits, the
first eight permutation entries of each of 82 layers and four continuation
longs. These initializers and decoded bytes were checked directly against both
original captures. Constructed threshold and repeated-addition controls are
labelled separately from the original observations.

## Numerical protocol and original corpus

`tests/reference/density/DensityProbe.java` invokes the actual supplied classes
in an isolated class loader. Each `init` uses a new owned root and genuine
world/provider construction, then creates a fresh numerical generator. It
checks the stored world seed and fresh scratch nullness. Initialization output
is redirected to diagnostics; method failures remain failures. Missing pinned
binaries or unavailable original setup return 77; malformed requests and
numerical execution/assertion failures return 1.

| Request | Original observation |
| --- | --- |
| `init seed` | Genuine setup and fresh generator; reset observation buffers. |
| `state` | All 82 layers' offsets and full 512-entry permutation arrays. |
| `buffers d e f g h density blocks` | Allocate valid bounded spans with deterministic nonzero prefills. |
| `density x z` | Refresh actual 16x16 climate, then invoke private normal density. |
| `base x z` | Invoke original base for the matching preceding density and current climate. |
| `generate x z` | Refresh climate, obtain density, then invoke original base. |

Each snapshot records seven nullable array descriptors, complete lengths and
raw double bits or block bytes; it also records processed climate, auxiliary
climate samples, biome identity letters and four copied-Random continuation
longs. The seven-bit retention mask follows `d/e/f/g/h/density/blocks` order.
Fixture operations `init/buffers` use zero retention masks. Climate records
contain copied 256-value prefixes; init's null climate means unobserved, not a
claim that server spawn left its manager arrays null. C terrain methods accept
independent explicit climate arrays and do not regenerate the caller's inputs.

On every original base/generate request, the helper checks all 425 recomputed
`q` bits against the preceding density prefix and checks each complete scratch
array against its preceding values. Thus observing the original base method
does not silently substitute an unrelated caller field. The helper consumes
no actual generator Random values when reporting continuation.

The original-only corpus completed 150 snapshots per side on native Temurin
17.0.17+10. All records were byte-for-byte equal: 20,483,526 bytes per side,
SHA-256 `8a3e1f1ca00c1990020178c18911e66a17bb4119b9d1623bdc297f6562d210ab`.
There were nine genuine setups per side, including eight distinct seeds and
a fresh repeat of seed zero. The inventoried client roots contained no files;
server roots contained nine eight-byte session locks plus directories. No save
method was explicitly invoked. Setup and numerical execution together took
approximately 1.22 seconds client and 1.83 seconds server in that run.

Requests cover seeds 0,1,-1, signed-long extremes, 17,-17 and 123456789;
positive/negative neighbors, moved and repeated chunks, the fixed cold
location, separate wrapping origin products and Java-int coordinate extremes.
These last coordinates exercise numerical methods, not full-world block
reachability. Buffer sequences include minimum, distinct oversized and bounded
maximum capacities with retained output and cleared scratch suffixes.

Each side passed 82 original `q`/full-scratch recomputation checks. Independent
capture validation also passed 49 fixture equivalences and 346 preserved-field
conditions, including standalone/composed results, wrapped origins, constructor
state, Random continuation and untouched inputs/outputs. The saved manifest
pins both JARs, JDK runtime/compiler/modules, exact helper source/class,
requests, fixture source and captures; all eleven artifact hashes were checked.
These are original-only results. Final C/client, C/server and cross-platform
comparisons require their own executable/source manifests and results.

Initial C probes built with native MSVC and WSL GCC/Clang each matched all 150
captured snapshots as parsed JSON, including constructor state, climate, span
contents and retention masks. The initial executable/output hashes and captures
were independently checked. These executables were built from uncommitted
source; this establishes an initial comparison baseline. Final acceptance
requires the finished comparison helper and exact source attribution for each
platform run.

Reproduce the static observations without starting a world:

```powershell
Get-FileHash -Algorithm SHA256 b1.7.3.jar,b1.7.3_server.jar
javap -classpath b1.7.3.jar -c -s -p yf uf am xv uu fd xa rh wy wt hc ok mm
javap -classpath b1.7.3_server.jar -c -s -p pk mp ab ph na dj os kp om dq es jb
```
