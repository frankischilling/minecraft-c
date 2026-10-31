# Player NBT reference observations

This package observes the unchanged client LocalPlayer's real Living, Player
and Inventory NBT methods, metadata save and Info.Player attachment. It uses
genuine `kq`, Session, World, Player, Inventory and Stack owners. Its authored
observers do not replace game classes, copy the NBT algorithms or synthesize an
application receiver. See the [runtime contract](../../../../docs/entity/player-nbt.md)
and [selected class/Code pins](source-pins.json).

## Evidence and limits

The normal original profile has 33 cases: two direct writes plus 31 reads,
with 11 complete reads and 20 interrupted reads. Native comparison checks 40
normal trees/1,302 tag nodes, field widths and raw values, failure prefixes,
state, Inventory identity decisions and Random continuation. Three LastPlayed
values are checked through their declared clock treatment. The persisted native
load uses the same input as the original direct round trip; it does not add an
original World-reopen observation.

The separate JDI observer stops the actual `gs.a(Lnu;)V` at wake invocation 93.
Its 37 compared records contain 33 tags and four state/Random/site/attachment
records. The original subsequently completes wake/read/clear/publication.
Native stops at the genuine wake dependency after earlier stores; that prefix
does not establish full-call wake parity.

The earlier isolated Windows Debug/default 1 MiB and sanitized Clang18.1.3/GCC13.3.0 products each pass 486 NBT,
110 coordinate, 9 passive diagnostic, 169 publication, 678 active-tick and 247
allocation checks, with the same 12 selected regressions. Both Linux profiles
also close 17 native target dependencies and 28 comparator controls normally and
under `-O`. These are selected component qualifications, not a whole frontend
or full gameplay run. The ordinary Windows stack checkpoints do not bound
arbitrary recursive callbacks.

Fresh original runs were qualified on Windows JDK17. Linux qualification uses
actual Linux-native executables against the unchanged retained Windows original
observations. It does not claim original execution on Linux. Full wake,
frontend startup/gameplay, server behavior, every concrete save descendant and
unobserved floating-remainder cases and Java mutable-tree/OOME/caught-file-failure
parity remain outside this package.

## Current combined qualification

The subsequent combined source passes all 1,072 local tests: 376 Windows and 348 per sanitized Linux compiler. Its current binaries pass 490 NBT checks. Four source-backed assertions and the strict comparator require Living getter failure locations 4/29/39/49; the earlier DeathTime/AttackTime diagnostics incorrectly named later stores 42/52. The comparator now passes 31 controls normally and under optimized Python. Fresh Windows originals and current Linux-native comparisons against the retained Windows observations retain the same 33-case/1,302-tag and separate wake93 scope.

## Capture a native run

After building the corresponding native executable, run the
[authored native driver](../../../tools/entity/test_player_nbt.py) from the
repository root. Paths below are explicit local inputs; keep outputs in an
ignored private directory.

```text
python -B tests/tools/entity/test_player_nbt.py --executable <mc_test_player_nbt> --kind nbt --source-root . --work-dir <native-work-dir>
python -B tests/tools/entity/test_player_nbt.py --executable <mc_test_player_nbt_coordinates> --kind coordinates --source-root . --work-dir <coordinate-work-dir>
```

The driver creates a unique child directory and authored resource files. A
passing report requires the actual child to exit 0, emit no stderr, complete both
bounded readers with empty reader_errors, be reaped and emit its positive final
marker. Executable/driver/capture-helper hashes must remain unchanged. The
report pins the physical streams and capture receipt. Raw stdout or a copied
completion line alone is insufficient.

## Capture the original on Windows

[`run_original.py`](run_original.py) requires the supplied client JAR,
matching JDK17 java/javac in the same bin directory, the four pinned SDK JARs
(`lwjgl-2.9.0.jar`, `lwjgl_util-2.9.0.jar`, `jinput-2.0.5.jar`,
`jutils-1.0.0.jar`) and the pinned `lwjgl64.dll`. It derives javac from the
explicit java path. Keep the default 6 GiB pre-phase free-space requirement.

```text
python -B tests/reference/entity/player_nbt/run_original.py --client-jar <b1.7.3.jar> --libraries <SDK-jar-directory> --natives <native-library-directory> --java <JDK17-bin/java.exe> --source-root . --work-dir <original-work-dir>
```

The runner checks the whole supplied JAR and complete selected class/Code
metadata, SDK/native hashes, matching runtime/compiler versions and unchanged
inputs. It compiles the two authored Java helpers and retains the exact
three-class helper inventory, hashes and Code metadata. The real constructor
and method profile runs with the supplied classes, including its actual native
CPU dependency; this is not full graphical startup. The normal process and JDI
parent/target streams are retained with bounded completion checks. Helper and
game-class origins remain bound to the selected inputs. Output consists of
private captures, worlds and receipts, never public supplied assets or captured
answers. A successful original-only report makes no native parity claim.

## Compare completed receipts

[`compare.py`](compare.py) consumes the native and original **report.json**
files, their retained products and the actual native executable. It requires a
fresh output path.

```text
python -B tests/reference/entity/player_nbt/compare.py --native-run <native-report.json> --executable <actual-native-executable> --original-run <original-report.json> --source-root . --output <new-comparison.json>
```

The configured native executable must be the actual command receiver and match
its before/after/current hashes. Native input paths, commands and ELF identity
remain direct. Every consumed stream/receipt must be present in the products
inventory; embedded and physical captures must agree. The original helpers
are checked against their exact before/after/on-disk class and Code inventories
using the trusted project metadata utility bound to the original input hashes.

For retained Windows original evidence read on Linux, add
`--relocations <original-input-map.json>`. That JSON object maps recorded
original input paths to existing current files with the exact old hashes. It
may contain only original input keys; it cannot rescue native inputs or rewrite
commands. Each raw original ORIGIN path/file URI must match the recorded pinned
Windows JAR location before a derived location-only view is passed to the
existing validator. Foreign hosts and nonempty URI queries/fragments are
rejected. Raw captures, game fields, exceptions, NBT and Random values remain
unchanged. Use `--source-root` for the trusted source containing the verifier
bound by that original receipt.

The independent parser/receipt controls require no JAR or game execution:

```text
python -B tests/reference/entity/player_nbt/test_compare.py
python -O -B tests/reference/entity/player_nbt/test_compare.py
```

They reject incomplete captures, missing stream/helper inventories, wrong
native executable identity, malformed case completion, invalid mappings and
foreign or decorated origins. A completed comparison reports the normal
33-case result and the distinct wake93 scope explicitly.
