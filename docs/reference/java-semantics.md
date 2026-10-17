# Java numeric and random primitives

This is the first increment for [issue #2](https://github.com/frankischilling/minecraft-c/issues/2).
`include/mc/core/java/java.h` and `src/core/java/java.c` implement defined C17 operations needed
when translating Java bytecode. The tests establish the listed primitive results;
they do not establish Minecraft simulation, terrain, or random-consumption order.

## Evidence and provenance

The original client calls the Java standard library directly. These observations
come from `javap -classpath b1.7.3.jar -c -s -p jd`, not recompiled decompiler output:

| Side | JAR SHA-256 | Class | Method descriptor | Bytecode offsets | Observation |
| --- | --- | --- | --- | --- | --- |
| Client | `af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c` | `jd` | `<init>(Lfd;J)V` | 60, 64, 65 | Allocates `java/util/Random`, loads the supplied long, and invokes `Random.<init>(J)V`. |
| Client | Same hash | `jd` | `a(II[B[Lkd;[D)V` | 186 | Invokes `Random.nextDouble()D`. |

The local disassembly is `.local/reference/client-worldgen-random.javap`.
These callsites show why the library sequence matters. They do not prove that
the C implementation calls it at the correct times. No original server callsite
or game-state differential is covered by this increment.

The controlled numeric oracle is the actual installed Temurin runtime,
OpenJDK `17.0.17+10`, Windows x86-64. `JavaSemanticsProbe` executes Java operators
and the runtime's `java.util.Random`; it does not implement another RNG or run
recompiled game classes. The installed runtime is newer than the game, so this
is explicitly a Java primitive comparison, not an original-era JVM validation.

| Runtime input | SHA-256 |
| --- | --- |
| Temurin `bin/java.exe` | `974fd93b1279f87db5c86e15469dc5f76e708c3a341847523170c532118c8841` |
| Temurin `lib/modules` | `2d290b746c6497265cc2498d30e9695166a3fc282f231ebed467ce29c32946a8` |

The algorithm is also specified by the primary Java documentation:
[Random](https://docs.oracle.com/javase/8/docs/api/java/util/Random.html),
[integer division and remainder](https://docs.oracle.com/javase/specs/jls/se7/html/jls-15.html#jls-15.17.2),
[shifts](https://docs.oracle.com/javase/specs/jls/se7/html/jls-15.html#jls-15.19),
and [narrowing conversions](https://docs.oracle.com/javase/specs/jls/se7/html/jls-5.html#jls-5.1.3).
The Java API specifies the algorithms used by ordinary `Random`; subclasses can
override them. Game subclasses and their behavior still require separate audit.

The installed runtime's `javap -c -s -p java.util.Random` gives these anchors:

| Method descriptor | Bytecode offsets | Observed operations |
| --- | --- | --- |
| `initialScramble(J)J` | 1 through 8 | Seed XOR multiplier, then 48-bit mask. |
| `next(I)I` | 13 through 24; 38 through 45 | Multiply by 25214903917, add 11, mask to 48 bits, then unsigned shift. |
| `nextInt(I)I` | 0 through 13; 25 through 69 | Rejects a nonpositive bound before advancing; power-of-two scaling or signed-overflow rejection. |
| `nextLong()J` | 3 through 17 | Two signed int results, each sign-extended; first shifted by 32, second added. |
| `nextFloat()F` | 1 through 9 | Takes 24 bits and divides by 2^24. |
| `nextDouble()D` | 1 through 22 | Takes 26 and 27 bits and scales their sum by 2^-53. |

In the compiled probe, `integers()V` offsets 74, 84, and 94 execute `iadd`,
`isub`, and `imul`; offsets 117 and 131 execute and catch zero-divisor `idiv`
and `irem`, and offsets 187 and 197 execute nonzero division/remainder.
Regenerate offsets with `javap` when changing the probe; they describe this
source revision and javac 17, rather than a stable public interface.

## C behavior and API

The integer helpers preserve Java's 32/64-bit wrapping, masked shift counts,
arithmetic right shift, logical right shift, and low-bit narrowing to byte,
short, char, and int. C arithmetic uses unsigned values internally. Conversion
back to signed values stays inside the destination range.

Division truncates toward zero; remainder keeps the dividend's sign.
`MIN_VALUE / -1` returns `MIN_VALUE`, and its remainder is zero. A zero divisor
returns `false` and preserves the output, allowing the caller to implement the
reference exception's effect. Null outputs also return `false`.

Float/double conversion to int/long truncates, maps NaN to zero, and saturates
at the integer bounds. Bounds are checked before the C cast. The supported
baseline requires binary32 float and binary64 double and excludes fast-math.
This API does not yet cover arbitrary Java floating expression evaluation,
transcendentals, text encodings, collections, or exception propagation.

The random helpers implement explicit seeding, unbounded/bounded int, long,
float, and double. Invalid bounds return `false` without consuming state.
Each instance requires one owner or external synchronization. Gaussian draws,
byte arrays, boolean draws, default seeding, subclass overrides, and serialized
Java object state are outside this increment.

The bounded-int rejection must use the sign of the wrapping Java expression.
The long result must add the signed second half; concatenating two unsigned
halves changes outputs whenever that half is negative. For example, seed zero's
first long is `-4962768465676381896`. Its first draw with bound 1073741825 is
`516548029`, after rejection of earlier candidates. Both are observed runtime
values and fixed public regression vectors.

## Verification

Public test `mc_test_java` runs without Java or game files. It asserts 146
fixed edge and oracle checks, including negative/extreme seeds, overflow,
negative/out-of-width shifts, division exceptions, saturation boundaries,
bounded-draw rejection, and the signed halves of `nextLong`.

The optional differential runs independently generated C/Java records and
compares their full text. Floating results use exact bit patterns. It covers
11-by-11 integer edge combinations at each width, 17 shift distances, every
byte/short/char result over inputs -65536 through 65536, cast boundaries, and
64 draws per method for seven seeds and seven bounds. Mixed draws check
consumption through rejection and methods that take two internal draws.

```powershell
python scripts/reference/java/compare_java.py --executable build/mc_test_java.exe `
  --java 'C:/Program Files/Eclipse Adoptium/jdk-17.0.17.10-hotspot/bin/java.exe' `
  --javac 'C:/Program Files/Eclipse Adoptium/jdk-17.0.17.10-hotspot/bin/javac.exe' `
  --work-dir build/reference-java
```

The script also accepts tools from PATH/JAVA_HOME. Missing runtime, compiler,
or C executable returns exit 77 and `UNAVAILABLE`; it never reports parity
success for missing inputs. Execution failure or any difference returns 1.

Observed on 2026-10-17 with standalone MinGW GCC C17 and strict diagnostics:
146 public checks passed, and all 135768 differential records matched.
The normalized vector SHA-256 was
`6e267821b006c8219a806e69ac82f8d46bd208c24ebdfcaf02af89d8d6139c82`.
The exact integrated commit and native platform/CI results belong in
`docs/status.md`; a vector digest alone does not identify a tested commit.
