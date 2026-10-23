# Shared Random profile and platform clocks

The [public profile API](../../include/mc/core/java/random_profile.h) owns the
shared constructor state and lazy `Math.random` owner for one fresh application
and library-class lifetime. It belongs above worlds, registries and individual
Random owners. The native service and its focused tests are implemented;
World, population, registry and Entity callers share that live owner. Complete
client/server application startup remains unfinished.

## Ownership and operation order

`mc_java_random_profile_create` allocates the profile, initializes its real
platform clock and sets the captured initial uniquifier, `8682522807148012`.
Creation does not advance that value or allocate the Math owner. A new profile
represents fresh library state; it cannot attach to an already running JVM's
unknown constructor history.

`mc_java_random_construct_default` takes fresh caller-owned Random storage. It
loads the full 64-bit uniquifier, multiplies by `1181783497276652981` with Java
long wrapping and commits the new value through strong compare-and-exchange.
A failed comparison retries from a fresh load. After success, the call takes
one real clock sample, XORs the two full-width values and freshly seeds the
ordinary Random, including a positive-zero Gaussian cache and false flag.
Concurrent constructors may share the profile with disjoint outputs. The CAS
and clock sample are separate operations, so another constructor can commit
and sample between them.

The existing explicit `mc_java_random_seed` and `mc_java_random_reseed` calls
consume neither the uniquifier nor a clock sample. Fresh seed initializes every
semantic field. Reseed resets an initialized LCG and clears cache validity while
retaining the cached double's bits. Their seeded sequences and
[Gaussian contract](random-gaussian.md) remain separate from profile lifetime.

The first admitted `mc_java_math_random` caller elects one initializer before
allocating a separate retained Math Random. Other first callers wait and
recheck the phase. The initializer constructs through the same shared profile,
then publishes the owner with release semantics; readers acquire readiness
before using it.
Later calls continue that owner. Each double makes a 26-bit atomic LCG advance
followed by a separate 27-bit advance. Each step has its own CAS retry loop,
and another caller may advance the stream between those two steps. Ordinary
caller-owned Randoms remain distinct from this atomic Math owner.

## Failures, budgets and lifetime

Failed calls preserve caller outputs. A completed uniquifier advance or clock
sample remains consumed after a later failure. Math budget, allocation or
constructor failure latches native holder failure: the initializer receives
the first error, and later admitted calls receive `HOLDER_FAILED` without
retrying initialization. Argument or FP rejection before election leaves a
cold holder unchanged.

The allocation budget covers the actual native profile and optional Math owner
sizes. An unpublished Math allocation is freed on constructor failure. These
charges describe native storage, not the JVM's Random/AtomicLong allocation
graph or Java exception behavior. The initializer is the concrete ordinary,
callback-free holder path; this service does not implement a general Java
class-initialization engine.

The profile is opaque and noncopyable. It must outlive every operation and
dependent user. Allocation queries and destruction require quiescence, including
waiting callers; ordinary owners still need their existing exclusive-access
contract. Null free/query are harmless. Outputs must remain valid, aligned and
disjoint from profile storage and concurrently used owners or outputs.

The supported environment is Windows/Linux x86-64 with SSE2, binary64
evaluation, nearest-even rounding, masked FP exceptions and gradual underflow.
FTZ, DAZ, excess precision, reassociation and FMA contraction are excluded.
Production calls inspect controls without changing them. Asynchronous signals,
cancellation and arbitrary subclass or native reentry are outside this API.
CMake requires C17 thread support and enables MSVC's
`/experimental:c11atomics` for the implementation.

## Measured constructor inputs

The private observation harness can bind one immutable sequence to a fresh,
untouched profile. Each row supplies the full committed uniquifier and a signed
`nanoTime` return. Binding checks the entire multiplication progression before
publishing the borrowed sequence. It cannot bind after a constructor attempt,
after Math initialization, or for a second time. The ceiling is 65,536 rows;
a bound empty sequence stays distinct from ordinary real-clock operation.

A reached constructor still performs its actual uniquifier CAS first. It then
consumes the next declared sample at the normal clock-sampling boundary and
enters the same production XOR/constructor tail. It assigns neither a desired
Random state nor a supplied shared counter. Input exhaustion or an order
mismatch preserves the consumed CAS prefix and caller output, latches a native
harness failure and forbids silent resampling. A terminal completeness check
also rejects unused rows. Math uses the same sequence through its real lazy
owner construction.

The binding requires exclusive access and keeps its borrowed input alive until
profile retirement. It does not change the public concurrent real-clock path.
The focused input tests cover boundary values, fresh-owner admission, whole-tape
validation, exhaustion, unused rows, failure prefixes, output preservation and
Math allocation/holder behavior. Supplied inputs alone establish no observation
provenance, JVM clock equivalence or game parity; the separate original capture
must establish where measured rows came from.

## Concrete clock paths

The private [clock implementation](../../src/platform/time/nanotime.c) has two
platform paths. Windows initialization calls QueryPerformanceFrequency (QPF),
then retains an initial raw QueryPerformanceCounter (QPC) count. A later sample
queries QPC once, converts count and frequency separately to binary64, divides,
multiplies the quotient by one billion and
truncates only a finite result in `[-2^63,2^63)`. The initial count is retained
without being subtracted.

Linux initialization and sampling use `clock_gettime(CLOCK_MONOTONIC)`. Its
integer conversion requires `0 <= nsec < 1000000000`, a representable
`sec * 1000000000` intermediate and a representable final sum. The retained
frequency of one billion is a unit marker unused by this conversion. Neither
path falls back to wall time. Unsupported numeric edges return native `RANGE`;
they are not evidence of a corresponding Java clock exception or saturation.

## Native checks

The following focused targets pass on MSVC 19.51 in strict Debug and GCC 13.3 /
Clang 18.1.3 with ASan, UBSan and leak checks. Windows runs use no sanitizers.

| Target | Result | Scope |
| --- | --- | --- |
| `platform.nanotime` | 172 Windows / 173 Linux checks | Exact scalar identities and integer bounds, live OS brackets, output preservation, immutable clock storage and controlled unsupported MXCSR states |
| `platform.nanotime_conversion` | 17,769 checks over 4,866 captured vectors on each compiler | Exact status/value comparison with actual Java scalar results and unchanged failure outputs |
| `core.java_random_profile` | 191 checks on each compiler | Native ownership, budgets, failure latch, constructor commits, cold contention, atomic Math continuation and FP rejection |
| `core.java_random_constructor_observation` | 18 checks on each compiler | Recorded constructor inputs through the production tail and plain Random continuations |

All 80 public CTest cases pass on each compiler, including these four targets.
This checkpoint does not include opt-in original-game comparisons. The recorded
constructor replay does not sample a clock, advance the shared uniquifier or
reproduce the production atomic Math holder.

## Java scalar conversion capture

The [scalar helper](../../tests/reference/java/NanotimeConversionObservation.java)
evaluates the documented Java 17 count/frequency operator formula and the
explicit native range policy. Its timespec cases call `Math.multiplyExact`
before `Math.addExact`. It samples no clock. The captured corpus has 2,608
counter and 2,258 timespec vectors, including signed64 and 2^53 boundaries,
positive frequencies and invalid/range inputs.

Three requested Windows Temurin 17.0.17 modes (`-Xint`, tier 1 and default)
produce byte-equal complete output. The
[public fixture](../../tests/reference/java/nanotime_conversion_fixtures.h) and
[provenance](../../tests/reference/java/nanotime_conversion_provenance.json)
retain those results and input/tool/native hashes. Mode flags alone do not
prove method compilation or intrinsic selection. This comparison qualifies the
scalar formula and declared C range policy; it does not prove the installed
native `System.nanoTime` implementation's QPC conversion.

## Instrumented constructor observation

The [public JDI trace](../../tests/reference/java/random_constructor_observation.tsv)
and [provenance](../../tests/reference/java/random_constructor_provenance.json)
record a Windows JDK 17 `-Xint` execution. The holder is absent among 409 loaded
types at VM start. The observer retains ten object mirrors from their first
recorded identities through completion.

The trace delivers 221 JDI events. It records three default constructors,
three successful uniquifier returns, three full native `nanoTime` returns and
their three seeded-constructor delegates. An explicit-seed owner and its later
reseed leave the shared uniquifier unchanged in this scenario. One Math owner
survives three calls, including a separate default construction between its
second and third call. Five nextDouble returns, including two inside the
explicit owner's Gaussian miss, account for ten next(bits) returns.

Compiler, target and observer exit naturally with code 0, are reaped and have
empty stderr. Recorded source, tool, native-library and source-archive pins
remain unchanged. Debugger pauses alter clock samples. The trace observes full
`nanoTime` return values rather than underlying QPC count/frequency pairs; it
does not close the installed-native build/source bridge, observe concurrent
Math calls or execute original game classes.

| Public capture artifact | SHA-256 |
| --- | --- |
| Scalar stdout, recorded in provenance | `33bdead02a3e18570707bb189cbdf75e4f0cc3ea36cd3beb6bb7c9ba531f6f4b` |
| Scalar provenance | `3646aec5a5d7ff5244e48f9f78eb3f5b945bf6c0fb4a728a3ed38da30bf08bfe` |
| Formatted scalar fixture | `f8b3c8d59d7b6816f7593bf3b257a5b605bbbc930bec543a6569ebec0ebf2b87` |
| Constructor JDI trace | `7c0034058912efc2791edfef70ebdcbf878b1a584457edf456dc716ee7f6cf88` |
| Constructor provenance | `93146a3ecd01f8e1a86d9bbac89582a20e0e0a1ac364d200e36fded486e1acd9` |

## Reproduction

Both recorders require the retained Windows Temurin 17.0.17 distribution and
check its tool, module, native-library, release and source-archive hashes.
The constructor recorder also needs `jdk.jdi` and the pinned debugger transport.
Replace the placeholders below. Each output directory must be new and its
parent must already exist.

```powershell
$jdk = "<retained-windows-jdk>"
python scripts/reference/java/capture_nanotime_conversion.py --java "$jdk/bin/java.exe" --javac "$jdk/bin/javac.exe" --output-dir ".local/nanotime-scalar-<new-id>"
python scripts/reference/java/capture_random_constructor.py --java "$jdk/bin/java.exe" --javac "$jdk/bin/javac.exe" --output-dir ".local/random-constructor-<new-id>"
```

Inspect each complete `capture.json`, unchanged input hashes and actual child
exit/reaping records before accepting its output. The scalar recorder writes a
generated header in its new directory; it does not replace public fixtures.
The constructor recorder retains the observer trace there. A fresh constructor
capture will have different clock samples and a different trace hash.

The native tests use the committed scalar fixture without Java or game files:

```sh
ctest --test-dir <build> -C Debug -R '^(platform[.]nanotime|platform[.]nanotime_conversion|core[.]java_random_profile|core[.]java_random_constructor_observation)$' --output-on-failure
```

Whole population, world/registry/entity ownership, startup history and application
wiring remain unfinished. Private callback drafts do not supply that production
integration. These tests and captures establish no original-game, Java 6,
arbitrary-FP, graphical, multiplayer or performance result.
