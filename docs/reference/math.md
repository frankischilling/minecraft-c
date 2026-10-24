# Float trig table and double-floor helper

The Overworld cave pass depends on Minecraft's float sine/cosine table and its
double-to-int floor helper. Both supplied Beta 1.7.3 binaries produced the same
65,536 table entries and the same 366 sampled method results on native Temurin
17.0.17+10. The table comparison covers every entry; the calls cover 132 float
inputs for each lookup and 102 double inputs for floor. They do not exhaust
the input space of any method.

An isolated C strategy experiment reproduced the complete frozen table and
all 366 results with MSVC, GCC and Clang on the measured x86-64 hosts. It used
the captured original results. Fresh production comparisons now match the
actual C module and both originals on native Windows and WSL Linux. All 27
local CTests pass without skips on each of three toolchains. This note does
not establish cave, complete chunk or full-game parity. The
[status note](../status.md) records broader acceptance.

## Reference identity

| Side | JAR SHA-256 | Original math class | Class SHA-256 |
| --- | --- | --- | --- |
| Client | `af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c` | `in` | `8693f3179fb0ff1df382e1d3388d23ab688ad3e26415454655da88f10b054f63` |
| Server | `033a127e4a25a60b038f15369c89305a3d53752242a1cff11ae964954e79ba4d` | `fq` | `33b6eb4c19f19a336e3debc78efcd7f6b717081484b4648542c92625501c7188` |

The [binary audit](binaries.md) records provenance limits for these references.
The actual class-file instructions and constant-pool bytes are the primary
static evidence. Their relevant bodies match after corresponding owner names
and constant-pool references are mapped; offsets, opcodes, constants and branch
targets remain intact. Both class files have major version 49, minor 0.

The numerical oracle was the untouched class loaded from each pinned JAR.
[TrigMathObservation.java](../../tests/reference/math/TrigMathObservation.java)
is a project-owned reflective observer, not a recompilation of an original
implementation. Its frozen source SHA-256 is
`f5af8190d96b33c7dfa93acaf5a13a25adb5d54cd6cf12ae459f034c87be6f06`.

| Runtime input | Baseline identity / SHA-256 |
| --- | --- |
| Runtime | Temurin OpenJDK `17.0.17+10`, Windows x86-64 |
| `bin/java.exe` | `974fd93b1279f87db5c86e15469dc5f76e708c3a341847523170c532118c8841` |
| `bin/javac.exe` | `cb8056d3878c2bd252e0f6316e009cdf8a140ef7cb38878c62b8aa2c75fbd5f3` |
| `lib/modules` | `2d290b746c6497265cc2498d30e9695166a3fc282f231ebed467ce29c32946a8` |

This runtime is newer than the game. These results do not establish the table
contents of every historical or future JVM.

## Original methods and constants

All offsets below apply to both original owners and are relative to the named
method. The table is private static field `a:[F`.

| Binding | Descriptor | Bytecode evidence |
| --- | --- | --- |
| Sine, `a(float)` | `(F)F` | Float multiply at 4-6; Java `f2i` at 7; mask 65535 at 8-10; table load at 11; return at 12. |
| Cosine, `b(float)` | `(F)F` | Float multiply at 4-6; float phase addition at 7-9; `f2i` at 10; mask at 11-13; table load at 14; return at 15. |
| Double-floor, `b(double)` | `(D)I` | Java `d2i` at 1; compare against the converted int at 4-7; conditional wrapping decrement at 10-12; return at 17. |
| Table initializer | `()V` | Allocate 65,536 floats at 0-4; loop at 9-41; ordered double arithmetic at 19-32; actual `Math.sin(D)D` at 33; narrow to float at 36; store at 37; return at 44. |

The numeric constant-pool bytes are identical on the two sides:

| Role | Type | Raw bits | Value |
| --- | --- | --- | --- |
| Index mask | int | `0000ffff` | 65535 |
| Table length | int | `00010000` | 65536 |
| Lookup multiplier | float | `4622f983` | `0x1.45f306p+13f` |
| Cosine phase | float | `46800000` | 16384.0f |
| Initializer factor | double | `4000000000000000` | 2.0 |
| Initializer PI | double | `400921fb54442d18` | `0x1.921fb54442d18p+1` |
| Initializer divisor | double | `40f0000000000000` | 65536.0 |

For each index, the initializer evaluates double `i * PI`, then multiplies by
2, then divides by 65536. It calls the runtime's `Math.sin` on that double and
narrows the result to float. The original initializer does not call `sinf` or
`StrictMath`. The observations compare the final binary32 table entries; they
do not compare every intermediate binary64 sine result.

Sine multiplies its float argument by the float multiplier, converts with
Java's float-to-int rules, masks the result to 16 bits and loads the table.
Cosine adds the float phase **before conversion**. Converting the sine index
first and then adding 16384 changes that computation at boundaries. The
[Java primitive note](java-semantics.md) describes the narrowing and wrapping
operations used by the C implementation.

Double-floor converts to int, compares the original double with that int
converted back to double, and subtracts one with Java int wrapping when the
original is smaller. Its behavior at narrowing limits differs from an
unrestricted mathematical floor. It is also distinct from the
[simplex floor convention](simplex.md).

## Complete table and sampled calls

The two originals produced the same 262,144-byte sequence when all observed
float bits were serialized in index order as four big-endian bytes. Its
SHA-256 is
`87a67a39fff59636acb04edcda4ef622831037291f11fb61586c57596771dac2`.
The observer checked the table's array identity and complete raw-bit hash
again after the calls. Both were unchanged.

| Table index | Observed binary32 bits |
| ---: | --- |
| 0 | `00000000` |
| 1 | `38c90fdb` |
| 16384 | `3f800000` |
| 32768 | `250d3132` |
| 49152 | `bf800000` |
| 65535 | `b8c90fdb` |

Index 32768 is approximately 1.2246468525851679e-16, a positive residual from
the original initializer. Replacing it with zero or imposing unchecked
quarter-wave symmetry would change the observed table.

The deterministic [fixture generator](../../scripts/reference/math/fixtures.py)
preserves the baseline's 366 requests and their coverage reasons. Every
expected result comes from actual method invocation. Input generation chooses
boundary values; it does not calculate numerical expectations.

| Request group | Coverage |
| --- | --- |
| 132 distinct float patterns, each passed to sine and cosine | Both signed zeros; signed minimum/maximum subnormals and minimum normals; signed 0.5 and 1; float half PI, PI and twice PI; maximum finite values; infinities; five NaN encodings; adjacent floats near scaled indexes and f2i saturation. |
| 102 distinct double patterns passed to floor | Both signed zeros; signed minimum/maximum subnormals and minimum normals; maximum finite values; infinities; five NaN encodings; exact negative integers and adjacent doubles; int narrowing boundaries; signed 2^53. |

The scaled-index targets are -65537, -65536, -65535, -32768, -16384, -2, -1,
0, 1, 2, 16383, 16384, 32767, 32768, 65535, 65536 and 65537. The corpus takes
the float nearest each target divided by the exact multiplier and raw-bit
neighbors at offsets -2 through +2 where representable. It also takes five
neighbors near the signed int conversion limits divided by the multiplier
and near signed 1e10 and 1e30.

Floor boundary targets are -2147483649, -2147483648, -2147483647, -1024, -2,
-1, -0.5, 0.5, 1, 2, 1024, 2147483646, 2147483647, 2147483648, 2147483649
and signed 2^53, each with raw-bit neighbors at offsets -2 through +2.

### Nonfinite values and NaN payloads

The float NaN request encodings are `7fc00000`, `ffc00000`, `7fc00001`,
`7fa00001` and `ff800001`. The double encodings are `7ff8000000000000`,
`fff8000000000000`, `7ff8000000000001`, `7ff0000000000001` and
`fff0000000000001`. Each group includes three quiet encodings and two
signaling encodings.

The observer retains the requested raw strings and constructs Java values
with `Float.intBitsToFloat` or `Double.longBitsToDouble` before reflective
invocation. It does not canonicalize those strings. Copying or passing a
signaling NaN can quiet it in the JVM or processor, so these observations do
not establish signaling status inside the original callee. All five requested
NaN encodings give the same conversion-dependent result on both sides.

| Input / method | Observed result |
| --- | --- |
| Either signed zero, sine | Float `00000000` |
| Either signed zero, cosine | Float `3f800000` |
| Positive infinity, sine or cosine | Float `b8c90fdb` |
| Negative infinity or any requested NaN, sine or cosine | Float `00000000` |
| Either signed zero or any requested NaN, floor | Int 0, bits `00000000` |
| Either infinity, floor | INT_MAX, bits `7fffffff` |
| Exact INT_MIN, floor input `c1e0000000000000` | INT_MIN, bits `80000000` |
| Adjacent smaller double, floor input `c1e0000000000001` | INT_MAX, bits `7fffffff` |

The last result follows from saturated d2i conversion to INT_MIN followed by
the conditional wrapping decrement. Nonfinite arguments are valid inputs to
these pure methods. The lookup's mask keeps its table index in bounds.

## C strategy and acceptance boundary

The accepted [C interface](../../include/mc/core/math/math.h) uses a caller-owned
`mc_math_trig` containing 65,536 floats, or 256 KiB on the measured targets.
Initialization is exclusive; subsequent lookup calls read the initialized
table through a const pointer. `mc_math_trig_init` performs no allocation and
returns false for a null table. `mc_math_sin` and `mc_math_cos` require an
initialized nonnull table. `mc_math_floor_double` has no table dependency.

The selected initializer strategy preserves the ordered double operations,
calls host double `sin`, then explicitly narrows to float. The strategy
experiment matched every final float table entry and every sampled call with
native MSVC and GCC/Clang builds, including their sanitizer runs. These are
comparisons against the frozen native Temurin 17 baseline. The subsequent
production acceptance below uses the [math module](../../src/core/math/math.c)
and fresh original executions under each recorded runtime.

This backend is scoped to the measured x86-64 implementations. A future
library, architecture or compiler change must pass the complete table check;
these observations do not establish universal agreement between C `sin` and
Java `Math.sin`. Lookup arithmetic also requires binary32 multiply/add and
the exact Java conversion/mask sequence. Fast math, reassociation and FMA
contraction are excluded from the accepted build contract.

### Exact-source local acceptance

At source `d6665e7f3e1b57c16e9e12c14983abd2ba227441`, fresh comparisons passed
all 65,536 table entries and all 366 calls independently for C/client,
C/server and client/server on these measured x86-64 environments:

| Environment / C math backend | C compiler | Java runtime/compiler | C sanitizers |
| --- | --- | --- | --- |
| Native Windows 10 / MSVC runtime | MSVC 19.51.36260.0 | Temurin 17.0.17+10 | Off |
| WSL Linux / glibc 2.39 | GCC 13.3.0 | OpenJDK 21.0.12.1 | Address / undefined behavior |
| WSL Linux / glibc 2.39 | Clang 18.1.3 | OpenJDK 21.0.12.1 | Address / undefined behavior |

Both original runtimes and all three C builds produce table SHA-256
`87a67a39fff59636acb04edcda4ef622831037291f11fb61586c57596771dac2`.
Each participant records 132 sine, 132 cosine and 102 floor calls. Every
ordered numerical record agrees across all nine participants, including all
589,824 table values and 3,294 sampled calls across the three runs.

Each manifest reports pass with an empty error list. All 13 source hashes,
13 artifact hashes and 20 before/after input hashes per run were independently
checked, including actual probes, both JARs, runtime/compiler executables,
runtime modules/release, requests and fresh compiled Java classes. The class
hash is unchanged across each run; different Java compilers can produce
different class bytes from the same pinned observer source. Original table
before/after hashes match every observed entry; all three C probes report
`TABLE_UNCHANGED=1`.

Each toolchain's complete 27-test log has 27 passes and no skips. The public
[unit](../../tests/core/math/test_math.c) reports 903 checks, including all
observed float/floor cases, 37 selected table entries, independent table
objects, reinitialization and read preservation. Its complete-table FNV1a
fingerprint is `74c4d1bb1bf96b54`; this supplements live entry-by-entry
validation. All ten [public helper guards](../../tests/tools/math/test_reference.py),
including the fixture with a directory alias, also pass on each toolchain.
Public tests and prerequisite guards use no original JARs or JDK. These are
local acceptance results; CI and broader
integration results are recorded separately in the [status note](../status.md).

## Observation and comparison protocol

The Java observer is compiled freshly for each comparison. It loads only the
known math class from one pinned JAR using an isolated class loader, reads the
actual private table, and invokes only `a(F)F`, `b(F)F` and `b(D)I`. Its static
initializer performs the original table calculation. No original world,
registry, generator or cave class is requested by the observer.

Input TSV records are `sin` or `cos` followed by an eight-digit lowercase
float bit string, or `floor` followed by a sixteen-digit lowercase double
bit string. The Java helper takes `side`, `pinnedJar` and `requestsPath` as
arguments. The [C probe](../../tests/core/math/math_probe.c) reads the same
requests from stdin.

Both output streams identify field `a:[F`, length 65536, and the three method
descriptors. They then emit all 65,536 ordered `TABLE` records and one `CALL`
record for each request. Calls retain the operation, descriptor, original
input bits and raw eight-digit result; floor also includes its signed decimal
integer. Float results are observed with `Float.floatToRawIntBits`.

The original observer emits table hashes before and after the calls and
asserts table identity/content preservation. The C probe compares an exact
byte copy after its calls and emits `TABLE_UNCHANGED`. The
[comparison helper](../../scripts/reference/math/compare_math.py) validates each
side's headers, counts and preservation controls, independently hashes the
parsed complete table, and removes only those explicit transport-control
records before comparison. Table entries, descriptors, request order and
numeric bits are never normalized. C/client, C/server and client/server are
compared independently.

The baseline's project-owned observer source, class, raw requests, raw
outputs, fixtures, original class/JAR identities and runtime/modules hashes
are retained with its record. JAR hashes and the table's identity/content had
explicit before/after checks. Other baseline source/tool/request hashes are
post-capture attribution, not evidence that those files were checked before
execution. The production helper records its own exact tested source/tool/
runtime/probe inputs before execution, checks them afterward, checks the
compiled observer before/after, and rechecks all published artifacts.

Baseline child processes were limited to 15 seconds and 128 MiB heap.
Requests were bounded to 768 lines, 32 characters per line and 32,768 file
bytes. The actual batch used 366 lines. The driver checked stdout/stderr file
sizes against 4 MiB after each child completed; that was a post-child size
check, not a hard disk-write cap. The selected initializer itself has a fixed
65,536-iteration loop.

The production helper compiles fresh classes and writes fresh running,
unavailable, fail or pass reports with a new run identity. Reports contain
pinned JAR identities, source/class/tool/runtime/probe/request/output hashes
and input-preservation checks. Missing prerequisites return unavailable 77;
numerical execution, schema, timeout, comparison or preservation errors return
failure 1. A completed-side mismatch remains a failure if a later side becomes
unavailable. Each child has a 15-second timeout, with 128 MiB JVM heaps;
retained stdout/stderr is limited to 4 MiB per stream after checking full
stream sizes. Temporary disk writes while the child runs have no hard cap.
The 768-request, 32-character and 32,768-byte limits also apply to production
requests. The
[testing guide](../testing.md) owns invocation and acceptance details.

This evidence supports the observed table, lookup and floor behavior only.
The Overworld cave pass still needs its own Random-order, geometry, block and
state comparisons after the [base](density.md) and [surface](surface.md)
stages. Full chunk finishing, lighting, population and other dimensions remain
outside this prerequisite.

For a fresh comparison, set `MC_ENABLE_MATH_REFERENCE_TESTS=ON`, both supplied
JAR paths and `MC_REFERENCE_TESTED_COMMIT` to the full built source identity,
then run `ctest --test-dir build -R '^reference.math$' --output-on-failure`.
Add `-C Debug` for a multi-configuration Windows build. The standalone helper
accepts the same explicit executable/JAR/JDK/work-directory/source settings as
the other original comparisons. Reports remain in ignored local directories.

Static evidence can be reproduced without loading an original game class:

```powershell
Get-FileHash -Algorithm SHA256 b1.7.3.jar,b1.7.3_server.jar
javap -classpath b1.7.3.jar -c -s -p in
javap -classpath b1.7.3_server.jar -c -s -p fq
```
