# Simplex and octave noise

This note records numerical observations from the supplied Beta 1.7.3 binaries.
The opt-in comparison invokes the actual original numerical classes and compares
exact C results and state. The [status note](../status.md) pins tested source and
environments. Full biome selection, terrain generation and chunks remain separate work.

## Reference identity

| Side | SHA-256 | Simplex | Octaves |
| --- | --- | --- | --- |
| Client | `af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c` | `cc` | `ug` |
| Server | `033a127e4a25a60b038f15369c89305a3d53752242a1cff11ae964954e79ba4d` | `bi` | `mq` |

Hashes and bytecode were checked on 2026-10-17 with JDK 17.0.17. Provenance
limits remain in the [binary audit](binaries.md). The complete simplex and octave
disassemblies agree after replacing corresponding obfuscated class names and
constant-pool reference indexes. Arithmetic, constants, branches and offsets
were retained. Offsets below are relative to the named method and were checked
on both sides.

## Seeded construction and dimensions

| Client / server methods | Offsets | Observed behavior |
| --- | --- | --- |
| `cc.<init>(Ljava/util/Random;)V` / `bi.<init>(Ljava/util/Random;)V` | 15/27/39; 21/33/45 | Draws three `nextDouble` values, multiplying each by 256.0 before storing fields `a`, `b`, `c`. |
| Same constructors | 51-68; 73-90; 92-135 | Initializes 256 permutation entries, swaps each with `nextInt(256-index)+index`, then mirrors the current entry into index+256. All 256 bounded draws occur, including bound 1. |
| `ug.<init>(Ljava/util/Random;I)V` / `mq.<init>(Ljava/util/Random;I)V` | 11; 19-41 | Constructs each simplex layer in ascending order using the same supplied Random. |
| `cc.a([DDDIIDDD)V` / `bi.a([DDDIIDDD)V` | 13-26; 38-52 | Uses only stored fields `a` and `b` as the two coordinate offsets. Field `c` is unused by this sampler. |

Bounded rejection can consume extra internal Random draws. Omitting the third
offset or the bound-1 call changes following generators. Sampling itself makes
no Random calls in these classes.

The public numerical sampler takes an output array, two origins, two integer
sizes, two coordinate scales and an amplitude. It is a 2D bulk method; these
classes expose no scalar or 3D sampler. The second coordinate is world Z in the
immediate consumers. The three-component gradient table contributes only its
first two components to this method.

## Constants, floor and cell arithmetic

The static initializers `cc.<clinit>()V` / `bi.<clinit>()V` calculate
`F = 0.5*(sqrt(3.0)-1.0)` at 230-242 and
`G = (3.0-sqrt(3.0))/6.0` at 245-259. They call `Math.sqrt` separately at
236 and 251. Reading the actual initialized fields on Temurin 17.0.17+10 gave
the same values on both sides:

| Field | Raw binary64 bits | Exact hexadecimal value |
| --- | --- | --- |
| `f` (F) | `3fd76cf5d0b09954` | `0x1.76cf5d0b09954p-2` |
| `g` (G) | `3fcb0cb174df99c8` | `0x1.b0cb174df99c8p-3` |

Private floor helper `cc.a(D)I` / `bi.a(D)I` compares with zero at 2/3,
casts to Java int at 7 or 12, and subtracts one at 14 on the nonpositive or
unordered branch. Both signed zeros map to -1; negative integers also take the
wrapping subtract-one branch; NaN maps to -1. Saturating casts also matter
outside int range. This differs from mathematical floor and from the improved
Perlin helper. The [Java numerical notes](java-semantics.md) describe the cast
and wrapping operations.

In `cc.a([DDDIIDDD)V` / `bi.a([DDDIIDDD)V`, cell indexes are added as a
Java int at 89, converted to double at 90, then multiplied by G at 94. The sum
wraps before conversion. Adding in a wider integer type or in double changes
large-coordinate results.

## Grid arithmetic and accumulation

These offsets refer to `cc.a([DDDIIDDD)V` / `bi.a([DDDIIDDD)V`:

| Offsets | Observed behavior |
| --- | --- |
| 6-10; 31-35; 487/493 | First dimension outermost, second innermost; flat order is `x*sizeZ+z`. |
| 13-26; 38-52 | Each coordinate is `(origin+index)*scale+storedOffset`, with addition before multiplication. |
| 54-125 | Skews with `(X+Z)*F`, applies the original floor helper to each coordinate plus skew, and subtracts cell origins unskewed by the wrapped cell sum. |
| 127-150 | Strict first-displacement greater-than second selects middle-corner offsets (1,0). Equality or unordered comparison selects (0,1). |
| 150-200 | Middle displacements subtract the selected 0/1 then add G. Last displacements subtract 1.0 then add `2.0*G`. |
| 202-286 | Masks cell indexes with 255, nests permutation lookups and selects gradients modulo 12 at 235/261/285. |
| 288-344; 346-402; 404-460 | Each attenuation is `(0.5-x*x)-z*z`. Negative attenuation contributes literal zero. Otherwise it is squared, then its square is multiplied by the dot product. |
| 470-486 | Accumulates `old + (70.0*((corner0+corner1)+corner2))*amplitude`, retaining both sum grouping and multiplication order. |

Dot helper `cc.a([IDD)D` / `bi.a([IDD)D` multiplies the first gradient
coefficient by the first displacement at 5, the second by the second at 11,
then adds at 12. Zero coefficients are still multiplied. The ordered projected
gradient rows are (1,1), (-1,1), (1,-1), (-1,-1), (1,0), (-1,0), (1,0),
(-1,0), (0,1), (0,-1), (0,1), (0,-1), built by initializer offset 227.
Duplicate rows affect the modulo-12 distribution.

Direct sampling adds to the existing array prefix and retains any unused suffix.
Each point evaluates all three corners; there is no improved-Perlin height-one path
or cached Y fraction. Java 17 defines binary64 expressions and evaluation order
in [JLS 15.4 and 15.7](https://docs.oracle.com/javase/specs/jls/se17/html/jls-15.html#jls-15.4).
Exact comparisons require the pinned runtime and strict C floating-point options;
reassociation or fused operations can change results. Original-era JVM behavior
and NaN payload identity are not established by this evidence.

## Octave scaling and reused arrays

| Client `ug` / server `mq` methods | Offsets | Observed behavior |
| --- | --- | --- |
| `a([DDDIIDDD)[D` | 15/18 | Forwards the caller's final frequency multiplier and supplies 0.5 as the amplitude-divisor multiplier to the extended overload. |
| `a([DDDIIDDDD)[D` | 0-14 | Divides each initial coordinate scale by 1.5. |
| Same extended method | 16-38; 41-59 | Allocates when output is null or too short; otherwise clears the entire supplied array, including its unused suffix. |
| Same extended method | 62-66; 95-111 | Starts frequency and amplitude divisor at 1. Each layer receives scales `(initialScale/1.5)*frequency` and amplitude `0.55/divisor`. |
| Same extended method | 114-119; 121-126 | Updates frequency with the first caller multiplier, then the divisor with the final caller multiplier. |

The default divisor multiplier 0.5 gives amplitudes 0.55, 1.1, 2.2, and so on.
Frequency and amplitude progression are independent. There is no final
normalization, and the final parameter of the extended overload is used. A
zero-layer wrapper still allocates or clears. Direct additive sampling and
octave fill therefore have different buffer semantics.

The C interface uses caller-owned state and buffers. It bounds dimensions,
products and capacities before writes, rather than reproducing Java allocation
or array failures. These checks do not establish equivalence for malformed
original calls.

## Live comparisons

The corpus contains 812 requests per side: one constants record, 144 state
records, 117 additive grids, 374 explicit-factor fills and 176 calls through the
actual short overload. It compares 204923 returned double bit patterns, 348160
permutation entries and 3248 subsequent Random longs. C/client, C/server and
client/server comparisons are separate. Initialized constants match exactly.

Each implementation also passes 177 equivalence controls, three distinct-factor
controls, 667 suffix checks, 104 zero-layer checks and a constants-without-draws
control. These distinguish default from explicit 0.5, independent octave factors,
mixed Perlin/simplex construction, accumulation and full-span clearing.
Fourteen targeted original-only observations supply all public fixed vectors;
every value was traced to both agreeing originals, independently of C output.

Enable `MC_ENABLE_SIMPLEX_REFERENCE_TESTS` with both supplied JARs and an exact
`MC_REFERENCE_TESTED_COMMIT`, as described in [testing](../testing.md). The helper
records source/executable/tool/JAR hashes and request/result artifacts, uses
fresh compiled classes, and atomically replaces current reports. Missing or
mismatched prerequisites return77; execution/comparison errors return1. Public
generated tests cover unavailable reports and input-preserving output paths.

## Consumers and remaining scope

Client manager `xv.<init>(Lfd;)V` and server `ph.<init>(Ldj;)V` create three
independent Random instances from wrapping world-seed products. Fields `e`, `f`,
`g` use multipliers 9871, 39811, 543321 and octave counts 4, 4, 2, respectively;
wrapper constructors occur at 25/52/79. The biome-array methods
`xv.a([Lkd;IIII)[Lkd;` / `ph.a([Lgs;IIII)[Lgs;` call these wrappers at
49/81/113. Each call passes the first requested size twice, at operand loads
36/38, 68/70 and 100/102, despite allocating the result from both requested
sizes. This consumer quirk does not restrict the sampler's independent sizes.

Climate blending, clamps, biome classification, nonsquare consumer behavior,
provider reachability and complete biome results need their own evidence and
tests. Improved Perlin is documented [separately](noise.md). Neither noise
family alone establishes density combination, surfaces, caves, population,
lighting, chunk output or full world-generation ordering.

Reproduce the bytecode observations without starting the game:

```powershell
Get-FileHash -Algorithm SHA256 b1.7.3.jar, b1.7.3_server.jar
javap -classpath b1.7.3.jar -c -s -p cc ug xv
javap -classpath b1.7.3_server.jar -c -s -p bi mq ph
```
