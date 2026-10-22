# Entity constructor evidence

The native [Entity contract](../entity/construction.md) implements the paired
base and ItemEntity constructors, owned children and native failure prefixes.
World publication, Tile drops and [Item NBT](../entity/nbt.md) have separate
contracts. [Ordinary Item persistence](entity-game-storage.md) now has its own
finite disk/source checks. Ticking and other concrete Entity descendants remain unfinished.
The evidence below separates native ownership tests, actual original snapshots
and recorded-input scalar formula replay.

The [Minecart source map](minecart-construction.md) records its separate
initializer, constructors and publication path, with passing native contracts
on MSVC, GCC and Clang. Original Minecart runtime observations remain
unverified; the Item results below retain their recorded inputs and source scope.

## Original methods and installed support

The pinned client/server archives are those in the [binary audit](binaries.md).
Complete local disassemblies supply both base declarations/constructors,
ItemEntity overloads, AABB construction/position/size, watcher registration and
static initialization, record construction and ID equality/hash. The complete
[field mapping](../entity/construction.md#field-mapping) accounts for 66 client
base fields, 61 server base fields and six ItemEntity fields, including shadowed
names and the five absent server fields.

The installed Temurin 17 source and compiled support checks cover the reached
ordinary Random/Math, fresh collections, boxed Byte0/Integer0..6 and constructor
ancestors. Byte0 is a shared library cache value, not a new per-entity box.
Class-key hashing reaches native Object identity hashing. Native VM identity,
CDS restoration, arbitrary loader/bootstrap history and JVM allocation failure
are not implemented or qualified by this constructor-only typed watcher API.
The compiled support capture binds 15 installed classes; it is not evidence
that every method or native body in that dump was reviewed.

## Actual original observations

On 2026-10-19, the original-only
[`EntityConstructionObservation.java`](../../tests/reference/entity/EntityConstructionObservation.java)
ran once for each side on pinned Windows Temurin 17.0.17.10, using `-Xint`,
normal verification and only the required `java.lang`/`java.util` private opens.
An isolated platform-parent loader invoked the actual original constructors
with a null World. Numeric stacks used their actual original constructors.

| Case | Overload | XYZ | Supplied stack ID/count/damage |
| --- | --- | --- | --- |
| 0, first | World-only | No XYZ arguments | Null |
| 1 | Full | 1.25, 64, -2.5 | 1 / 7 / 3 |
| 2 | Full | -0, -0, +0 | Null |
| 3 | Full | 33554432.125, -17.75, -33554432.125 | -1 / -7 / 32768 |
| 4 | Full | +2^53, -2^53, -2^53 | Null |

Each side returned exactly 455 ASCII/LF rows: all instance fields with explicit
server absences, six bounds doubles, owned/reference identities, watcher and
class state, private Random/cache and shared Math poststate. Checks cover
distinct owned children, stable shared owners, supplied-stack identity, the
five-ID continuation and typed Byte0/dirty flags. The world-only case directly
retains base-size bounds after changing its dimensions and offset.

Only after each constructor returned did the helper read Math's existing owner.
It inferred one or four nextDouble inputs by reversing the 48-bit LCG from its
actual poststate. Forward transitions, later inter-case continuation and raw
phase/yaw/motion outputs must agree before output publication. These inputs are
explicitly labeled `INFERRED_DRAW`: they are not a method-exit trace, clock or
uniquifier observation, or proof of the first cold prestate. Separate original
sides have different real entropy and are not required to produce equal bytes.

The reflective helper constructs supplied stacks before invoking the Entity
constructor. It does not observe the chest caller's raw-new/argument-allocation
order or its failure prefixes. Null World observations do not exercise World
construction, insertion, callbacks, game bootstrap or concurrency.

The [capturer](../../scripts/reference/entity/capture_entity.py) reused the
pinned bounded process helper. Compilation and both original children exited
naturally with code 0, were reaped and had empty stderr. All twelve measured
inputs and the compiled observer remained byte/hash-identical. Raw originals,
class files, process commands and receipts remain ignored local evidence.
Public [provenance](../../tests/reference/entity/item_formula_provenance.json)
contains content hashes and counts, without private paths or process IDs.

| Retained artifact | Bytes | SHA-256 |
| --- | ---: | --- |
| Original observer source | 19358 | `90efbd2e831a3b508933adfd59e7038b55761e3e52822784e753b42af418dc27` |
| Compiled observer | 17919 | `c421bd6a7679a663cfc15e701977cd0feb33209bbdc16055ecd426e164cd060f` |
| Original client output | 14088 | `22ceb7377da197318f40196d18bd25a9df28e98039712ad19cdbb1f73b5d21c1` |
| Original server output | 13983 | `55aacf94987431ef3e6c96043a7091e3c2ee3738371daa17fae1bff1b3b75f9d` |
| Capture receipt | 7232 | `221117ab19cf470858f9d50043dba6a419f4dd44022570201b7893859de3dae1` |
| Scalar fixture | 1457 | `3e08eca01a4cf327a58c68ab3e4742652f86debd10847f8f80ae643b941110d9` |

Receipt and fixture byte counts must be checked against their actual files when
binding a later source checkpoint. Source hashes are not themselves a test run.

To capture again, use the exact JDK required by the capturer and a fresh ignored
output directory:

```powershell
python scripts/reference/entity/capture_entity.py --java "$env:JAVA_HOME/bin/java.exe" --javac "$env:JAVA_HOME/bin/javac.exe" --client-jar b1.7.3.jar --server-jar b1.7.3_server.jar --output-dir .local/entity-new-capture
```

## Native verification domains

`entity.construction` passes 1,276 native checks per compiler. It covers both
sources and overloads, complete literal/default fields, RAW/READY/failed holds,
same children, shared class state, stack aliasing, ID wrap/equality, position
rounding and fifteen native allocation boundaries per source. Budget failures
retain their real field/ID/class prefixes; failed classes do not retry. An actual
unsupported-FP call reaches the private Random boundary after ID/bounds without
advancing the uniquifier. These are native contracts, not JVM OOM equivalence.

Quiescent tests observe the real profile without injecting clock/seed values.
Cold construction consumes the private and lazy Math uniquifier transitions;
later construction consumes one. Ready-holder cases check exactly two/eight
LCG steps from a genuine before-state. The first cold Math draw count cannot be
independently proved from its final state alone; that limitation remains explicit.

`world.runtime_profile-and-entity-bindings` passes 104 constructed integration
checks per compiler using actual stores, runtimes and profiles. It covers absent
profiles, retained typed diagnostics, RAW profile mismatch, real default
construction and READY/failed Entity bindings that keep the runtime BUSY until
final release. A real limited profile fails at the first Math allocation after
world publication. The existing runtime/lifecycle cases pass separately.

The [scalar fixture](../../tests/reference/entity/item_formula_observation.tsv)
exports 34 inferred inputs and 50 actual original field outputs across ten
side/cases. `entity.item_formula_observation` feeds those inputs only to the
actual private production formula functions used by the constructor. It checks
phase/yaw float narrowing, X/Z double-to-float-to-double motion and Y literals.
It does not inject entropy into a constructor or compare a native private seed
with an unrelated original process's seed. Three CLI refusal controls remain
separate from original observations. This replay does not establish whole-field
or whole-Entity parity.

MSVC uses native Windows strict Debug without sanitizers. GCC and Clang use
Ubuntu WSL Debug with ASan/UBSan and leak checks. The current full-suite result
and exact source binding are recorded in [testing](../testing.md) and
[status](../status.md); previous 80-case results are historical checkpoints.
