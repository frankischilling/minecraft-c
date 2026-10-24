# Improved Perlin and octave noise

This note records numerical bytecode observations from the supplied Beta 1.7.3
binaries. The opt-in comparison executes their actual noise classes and compares
exact numerical/state records with C. The [status note](../status.md) pins tested
source and environments. Noise results alone do not establish terrain or chunk parity.

## Reference identity

| Side | SHA-256 | Improved noise | Octaves |
| --- | --- | --- | --- |
| Client | `af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c` | `am` | `uf` |
| Server | `033a127e4a25a60b038f15369c89305a3d53752242a1cff11ae964954e79ba4d` | `ab` | `mp` |

Hashes were reread on 2026-10-17; provenance limits remain in the
[binary audit](binaries.md). Symbols are side-specific. The complete numerical
class disassemblies agree after replacing corresponding obfuscated class names
and constant-pool reference indexes. Arithmetic, constants, branches and offsets
were retained in that comparison. Every offset below was checked on both sides
and is relative to the named method, not a source line.

## Construction and scalar evaluation

| Client / server methods | Bytecode offsets | Observed behavior |
| --- | --- | --- |
| `am.<init>(Ljava/util/Random;)V` / `ab.<init>(Ljava/util/Random;)V` | 15/27/39; 21/33/45 | Takes three `nextDouble` draws for X/Y/Z offsets, each multiplied by 256.0. |
| Same constructors | 51-68; 73-90; 92-135 | Initializes 256 permutation values, then swaps with `nextInt(256-index)+index` and mirrors into a 512-slot table. All 256 bounded draws occur, including bound 1. |
| `uf.<init>(Ljava/util/Random;I)V` / `mp.<init>(Ljava/util/Random;I)V` | 11; 19-41 | Constructs improved instances in ascending octave order using the same supplied Random. |
| `am.a(DDD)D` / `ab.a(DDD)D` | 5/13/22; 27/32/37; 45-73; 76-122 | Adds offsets; casts coordinates to Java int, conditionally decrements, masks lattice indexes with 255, then computes fractions from the unmasked integers. |
| Same scalar methods | 124-206; 309-493 | Computes X/Y/Z fade factors, then interpolates corners along X, along Y, and finally along Z. |
| `am.b(DDD)D` / `ab.b(DDD)D` | 5-7 | Interpolation subtracts upper-minus-lower, multiplies by the factor, then adds lower. |
| `am.a(IDDD)D` / `ab.a(IDDD)D` | 1-85 | Gradient selection uses the low four hash bits, including special cases 12 and 14, with independently signed terms. |
| `am.a(DD)D` / `ab.a(DD)D` | 1-4 | The two arguments become X and Y; Z is literal 0.0. |

Bounded Random rejection can consume extra internal draws, so seed advancement
must follow calls rather than a fixed constructor step count. Omitting the final
bound-1 draw changes the next octave. Sampling itself makes no Random calls in
these classes.

Fade retains the grouping `(t*t)*t` times `(t*(t*6.0-15.0)+10.0)`. Reassociation,
fused operations and a different interpolation expression need independent
evidence before substitution. Java 17 specifies binary64 double expressions and
evaluation rules in [JLS 15.4 and 15.7](https://docs.oracle.com/javase/specs/jls/se17/html/jls-15.html#jls-15.4).
Pin the comparison runtime and use strict C floating-point options; this does
not claim validation against an original-era JVM.

The cast/decrement sequence behaves as floor only within Java int range. Large
coordinates require Java saturation and wrapping decrement, as covered by the
[numeric compatibility helpers](java-semantics.md). Replacing that sequence
with an unrestricted C floor-to-integer cast changes boundary behavior.

## Bulk and octave behavior

The improved bulk method is `am.a([DDDDIIIDDDD)V` on the client and
`ab.a([DDDDIIIDDDD)V` on the server. Arguments are output array, X/Y/Z origins,
X/Y/Z sizes, X/Y/Z scales, and an amplitude divisor.

| Offsets, both sides | Observed behavior |
| --- | --- |
| 0-3; 27-31 | A dedicated branch handles Y size 1 and computes the reciprocal divisor. |
| 43-56; 129-143; 269-352 | That branch uses X/Z coordinates and ignores Y origin, Y scale and stored Y offset. It evaluates a Y=0 lattice/fraction slice, including specialized gradient `a(IDD)D` at 269. |
| 360-371 | Adds the scaled result to the existing array element. |
| 430-607 | General bulk traverses X outermost, Z next, Y innermost. |
| 610-685; 687-701 | Computes current Y fraction/fade, but refreshes cached corner terms only for the first Y sample or a changed masked Y lattice index. |
| 798-976; 981-1031 | The cache holds four X-interpolated gradient values containing the earlier Y fraction; current Y/Z fades combine them before accumulation. |

The cache therefore retains more than permutation hashes. Replacing general
bulk with independent scalar calls is not equivalent when samples share a Y
cell. Concrete original numerical observations in the public regression test
check this consequence of the retained gradient operands and branch. The special
Y-size-1 branch also differs from the scalar two-argument plane.

On client `uf` and server `mp`, scalar octave method `a(DD)D` starts factor 1,
samples coordinates multiplied by that factor, divides the sample by it, then
halves the factor (offsets 27-50). Frequency decreases and amplitude increases;
there is no final normalization.

Bulk octave method `a([DDDDIIIDDD)[D` clears the entire supplied array at 18-36,
then passes scaled coordinate steps and the factor as divisor at 62-90, halving
it at 93-99. Direct improved bulk instead accumulates onto existing values.
The shorter overload `a([DIIIIDDD)[D` forwards X/Z origins and sizes with Y
origin 10.0, Y size 1 and Y scale 1.0 at 2-19. Its final double parameter is
unused. A generic Perlin/fBm wrapper does not establish any of these behaviors.

## Consumers and remaining scope

Client generators `yf`, `qn`, `jd` correspond to server `pk`, `ka`, `gb` for
normal, Nether and Sky providers. Their constructor descriptors are `(Lfd;J)V`
and `(Ldj;J)V`. Overworld/Sky construct octaves with counts
16,16,8,4,4,10,16,8 at 82/99/116/132/148/165/182/199. Nether constructs
16,16,8,4,4,10,16 at 70/87/104/120/136/153/170.

Overworld-generator surface methods `yf.a(II[B[Lkd;)V` and
`pk.a(II[B[Lgs;)V` call bulk at 39/77/126, including both Y-size-1 and general
shapes. Density methods `a([DIIIIII)[D` call the short octave overload at 74/105
and bulk at 151/185/219. These callsites establish actual consumers, not complete
generator equivalence. Sky reachability remains a separate question.

Reproduce these observations without executing game classes:

```powershell
javap -classpath b1.7.3.jar -c -s -p am uf yf qn jd
javap -classpath b1.7.3_server.jar -c -s -p ab mp pk ka gb
```

The live corpus has 783 requests per original side: 80 state, 105 scalar-3D,
168 scalar-octave, 61 additive grids and 369 octave grids. It compares 140218
returned double bit patterns, 399872 permutation entries and 3132 following
Random longs per side, with separate client/C, server/C and client/server results.
Cases cover seeded construction, shared-generator order, zero layers, both bulk
paths, in-cell Y reuse, accumulation versus clearing, negative/large coordinates,
adjacent cell boundaries and caller-buffer suffixes. Fourteen original-only
observations independently supply the public fixed vectors; their constants were
checked against both originals. No C-derived output is its own expected value.

Enable `MC_ENABLE_NOISE_REFERENCE_TESTS` with both supplied JAR paths and
`MC_REFERENCE_TESTED_COMMIT`, as described in [testing](../testing.md). The helper
records executable/tool/source/JAR hashes and exact request/output artifacts.
It replaces reports at running/unavailable/final states, uses fresh compiled
classes, and rejects output paths overlapping inputs. Missing or mismatched
prerequisites return77; execution or comparison failures return1. Public tests
verify prerequisite reporting and input preservation without running originals.

Separate simplex noise (`cc`/`bi`), biome selection, density combination,
surface replacement, caves, features/population, lighting, chunks and full
world-generation ordering remain outside this numerical increment.
