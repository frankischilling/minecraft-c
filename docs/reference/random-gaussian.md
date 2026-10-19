# Seeded Random Gaussian state and strict math

The [Random interface](../../include/mc/core/java/java.h) now retains a Gaussian
cache on each owner. `mc_java_random_seed` fully initializes fresh native storage:
the scrambled 48-bit state, cached positive zero and a false validity flag.
`mc_java_random_reseed` implements `Random.setSeed` on an initialized owner: it
resets the LCG and clears validity while preserving the cached double's bits.
Neither operation obtains constructor entropy.

`mc_java_random_next_gaussian` implements ordinary Random's polar algorithm.
A cache hit clears validity and returns the stored value without advancing the
LCG. A miss draws two doubles in order, maps them to [-1, 1), and repeats while
their squared sum is at least one or exactly zero. It computes the multiplier
with strict log and square root, stores the second result, marks it valid, then
returns the first result. There is no rejection cap. Other primitive draws leave
the Gaussian cache intact. Complete initialized value copies retain both cache
fields; live shared owners require external synchronization.

## Math backend

The [strict math interface](../../include/mc/core/math/strict.h) admits positive,
finite binary64 inputs, including subnormals. Log uses a C17 adaptation of the
independently acquired [Netlib fdlibm 5.3 source](https://netlib.org/fdlibm/e_log.c).
The [separate notice](../../licenses/fdlibm.txt) preserves Sun's permission and
records the source hash and adaptations. The port uses `memcpy` word extraction,
defined integer conversions and exact hexadecimal coefficients, preserving
the source's branch conditions and floating expression association.

Square root uses one SSE2 binary64 square-root instruction on x86-64. The
required environment is round-to-nearest, ties-to-even, gradual underflow,
masked floating traps, binary64 expression evaluation, no FTZ/DAZ, no
reassociation and no FMA contraction. The functions do not alter FP controls.
The observation probe checks SSE2 control bits before executing requests.
These contracts do not establish general NaN-payload, errno or floating-status
equivalence. Double sine, cosine and power need separate qualification.

## Captured JDK evidence

The project-owned [Java observer](../../tests/reference/java/RandomGaussianObservation.java)
executes actual ordinary `java.util.Random` and `StrictMath` methods. Read-only
reflection observes the real 48-bit state, cached double bits and validity flag
for two independent explicit-seed owners after every command. No original game
class or native implementation supplies expected values.

The frozen profile is native Windows x86-64 Temurin 17.0.17+10. Capture compiled
the observer once and ran six separate JVMs: twice with `-Xint`, twice with
`-XX:TieredStopAtLevel=1`, and twice with default VM options. All six complete
raw output streams agree. These requested modes do not prove that a particular
method was compiled or that an intrinsic was selected.

| Captured input | SHA-256 |
| --- | --- |
| Observer source | `ad55c6bf3e6d60c0b729f1f3b1147a938e6a4b18817696f0a0651be8d374c58b` |
| JDK modules | `2d290b746c6497265cc2498d30e9695166a3fc282f231ebed467ce29c32946a8` |
| VM library | `5539382b75556a9873fafa2d8b7d0b36b169ac436a3608467da4f74c2600cbce` |
| Native Java library | `18b105254c16e1d83614ff4e32cd62ce402fbe6339bdd1b24ccc7d999fed7f5f` |
| Canonical command/result trace | `19459419db728d6505bcb7350936c0cc872f7ec81a66a0c73bb900ce9228e961` |
| Canonical public provenance | `f570b7f7c775a6e7b830267f3caaa5ce7656777f539ac39c67a55a5317b8a158` |

The [immutable fixture](../../tests/reference/java/random_gaussian_fixtures.json)
contains 14,013 operations. Its corpus includes source-selected log branch
boundaries, positive subnormals, exponent/mantissa neighbors, maximum finite
inputs, 261 explicit-seed histories and 4,096 mixed two-owner operations.
Mixed operations include cache hits, interleaved primitive draws, replacement,
same/different-seed resets and invalid bounds. All outputs and both complete
owner snapshots are compared after every command, including retained cache
bits while validity is false. This is finite coverage, not exhaustive math
input or seed enumeration.

Each JDK observation emits 1,285,210 literal-LF ASCII bytes with SHA-256
`4e2df45a844c64d231cb1e6b201ac650ef651a9ef3f0ed79c6f36c9bc4ae54c4`.
The eight immediate children, including version and compilation, exited zero,
were reaped and had empty stderr. All seven tool/runtime/helper inputs were
fingerprinted before and after capture and stayed unchanged.

## Native replay and remaining scope

The [controller](../../scripts/reference/java/compare_gaussian.py) validates the
fixed trace, helper and provenance hashes before native replay. GCC 13.3 and
Clang 18.1.3 Debug builds with ASan/UBSan and native MSVC 19.51 Debug without
sanitizers match all 14,013 operations. CMake registers the replay as
`core.java_gaussian_replay`; it needs Python but no installed JDK or game files.
Line-ending transport is the only normalization before complete event-string
comparison.

The focused native unit has 85 checks for captured Gaussian/mixed histories,
stale-cache reseeding, signed-zero cache hits, complete owner copies and eight
strict-math boundaries. The 157-case native protocol test covers numeric/ID
bounds, malformed math encodings, line endings, control bytes, exact failure
prefixes and fresh replacement. Both pass on all three compilers. The 75-test
public suite also passes on each compiler, including the new unit and replay;
the protocol test was checked separately and then registered with CTest.
These public runs do not execute original game classes or repeat the private
original-comparison matrices.

The controller uses regular files for child input and output, a 60-second
immediate-child deadline and polled output thresholds. Temporary disk writes
can exceed a threshold between polls; the final reads are bounded. This does
not claim an arbitrary descendant-process deadline or a hard disk-write cap.

Existing terrain tests compare initialized Random semantic fields instead of
struct padding. Rejected-call byte canaries still compare raw storage, including
deliberately poisoned prior cave output. The staged normal generator copies its
complete initialized Random before reseeding; the cave projection keeps its
fresh output-initialization contract. Legacy observations that expose only LCG
continuations retain that scope.

Default constructors, shared constructor uniquifier, platform clocks,
`Math.random`'s lazy owner, subclass overrides, serialized Java objects and
original game callback/consumption order remain separate work. This newer JDK
comparison does not establish original-era JVM or whole-game equivalence.
