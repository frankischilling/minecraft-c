# Material identities and receiver methods

`mc_block` implements the original 28 material objects, 14 palette objects and
16-slot palette table. The [public interface](../../include/mc/block/material/material.h)
uses case-sensitive original field selectors, `a..z,A,B` for materials and
`b..o` for palettes. A material selector is not a block ID. Separate objects stay
distinct even when their method results or colors agree.

One `mc_materials` owner represents one shared material-class lifetime. All
worlds in that execution must borrow that same owner. Material, palette and table
addresses remain stable until destruction. There is no reset or owning-copy
operation. The application must retire every borrower before destroying the
owner; this module does not implement application startup or enforce a global
singleton. Access and destruction require external serialization.

Construction uses one fixed native allocation and publishes only a complete
owner. Null output or allocation failure returns false without writing the
output. This native boundary does not reproduce each Java allocation,
allocation exceptions or failed partial class initialization. These original
constructor bodies call no game callback, Random, world or block registry.

## Primary behavior

The inputs are the [fingerprinted originals](binaries.md). The following are
complete paired classes, not inferred modern material categories:

| Client / server | Constructor and receiver behavior |
| --- | --- |
| `ln / hj` | Base constructor stores `G=true` at bytecode 6 and final palette `C` at 11; other fields start zero. Methods `a,b,c` return true and `d` false. |
| `lu / ho` | Constructor calls inherited `f` at 6; methods `a,b,c` return false. |
| `sg / lk` | Constructor calls `f` at 6 and `k` at 11; `a,c` return false, inherited `b` remains true, `d` returns true. |
| `aq / ac`, `ou / jg` | Call the base constructor; `a,b,c` return false and inherited `d` remains false. |
| `dx / cm` | Palette constructor stores index `q` at 6, RGB `p` at 11 and the same object into table `a[index]` at 19. |

Base `e,g,i,j` read independent fields `D,E,G,H`. Public `f` sets only `E=true`
and returns the same object. Every alias sees that mutation. Method `h` returns
false when `F` is set; otherwise it dispatches `c`. It is distinct from `b`,
material identity and block/light opacity tables. The initializer's fluent
methods set `D`, `F`, `G` and `H` in original order; the receiver retains an
explicit subtype independently of those flags.

`ln/hj.<clinit>` constructs all 28 materials in `a..z,A,B` order, with constructor
calls at bytecodes 7 through 445. `dx/cm.<clinit>` allocates 16 slots and
constructs 14 palettes in `b..o` order. Slots 14 and 15 remain null. Palette `e`
and `h` both have RGB 10987431, with distinct identities and indices 3 and 6.
No later writes to the original public palette table are implemented here.

## Verification and reproduction

`block.material` checks all 28 successful initial result vectors, all palette
values, distinct/stable identities, the complete table and shared-alias `f`
mutation. It also checks isolation of two independent native fixture owners,
idempotence and safe supported null operations. It passes 28,623 checks on
native Windows MSVC and Linux GCC/Clang. The Linux runs use ASan/UBSan with leak
checks; Windows uses strict Debug without sanitizers. Allocator failure is not
injected.

The original client and server each produced the same 116 rows in one fresh
Windows JDK17 `-Xint` run. All three native probes match those bytes exactly,
with no value or newline normalization. The
[captured fixture](../../tests/reference/material/material_observation.tsv) is
1,911 bytes, SHA-256
`900bd25cc4e6f7857cd4faabda18d7b3baa075ae8672f5985152a45161ab831c`.
Its [sanitized provenance](../../tests/reference/material/material_provenance.json)
records unchanged input pins, the compiled helper and all three child receipts:
compiler plus two JVMs, each reaped with exit 0 and empty stderr. Private
commands, paths and process IDs stay outside the published metadata.

`block.material` and `block.material_observation` pass together on each compiler.
These are two new selected cases; the earlier complete 80-case suite is a
separate checkpoint. Run the material cases after a normal build:

```text
ctest --test-dir <build> --no-tests=error -V -R "^block[.]material(_observation)?$"
```

The [original observation helper](../../tests/reference/material/MaterialObservation.java)
loads only the material/palette classes from each pinned JAR in an isolated
class loader. It observes all palette values/table identities and each
material's initial and post-`f` receiver results. Reflection invokes the original
virtual methods; no recompiled game source or native executable produces
expectations. Each run uses a fresh JVM, so its mutation is confined to that run.

The [capture script](../../scripts/reference/material/capture_material.py) requires
the recorded Windows JDK17 binary profile and the two original JAR hashes. It
owns the compiler and two `-Xint` JVM children, checks complete 116-row literal
LF output, requires exact client/server equality and records input hashes before
and after, child status and stream hashes in a new private output directory:

```text
python scripts/reference/material/capture_material.py --java <jdk>/bin/java.exe --javac <jdk>/bin/javac.exe --client-jar <client.jar> --server-jar <server.jar> --output-dir <new-private-directory>
```

The raw JARs and private command/path receipts are not distributed. This
receiver-only observation does not run a game, create a world or qualify a full
block registry. The registry, original property arrays, block callbacks,
application bootstrap, terrain population and world integration remain open.
