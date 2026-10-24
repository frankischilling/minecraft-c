# Wire position Set

The [collection module](../../src/block/registry/wire_set.c) supplies the shared
wire Block's position Set. Its keys retain signed coordinates, full equality,
bucket traversal and independently held snapshots. The
[synchronous wire hooks](../world/wire.md) now have native implementation and
contracts. This collection result does not establish wire or redstone parity.

## Original evidence

The original client key is `wf`; the server key is `oa`. Both constructors
`(III)V` store their three immutable coordinate fields at PCs 6, 11 and 16.
Their `equals(Ljava/lang/Object;)Z` checks the actual class family and all three
coordinates. `hashCode()I` returns wrapping `x * 8976890 + y * 981131 + z` at
PC 20. Neither class implements Comparable.

| Input | SHA-256 |
| --- | --- |
| Client archive | `af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c` |
| Server archive | `033a127e4a25a60b038f15369c89305a3d53752242a1cff11ae964954e79ba4d` |
| Client `wf.class`, 500 bytes | `b6a5e47120868ba2d2b35e8ef8aca5f0637438cfdc1628a1fa5d633771f5c805` |
| Server `oa.class`, 500 bytes | `44135438a9b95a06829930668644cd9a1a63f9231efc9d37ccc7e80f2ebd3ede` |

The retained experiment uses Windows x64 Temurin 17.0.17+10 and its actual
HashSet, HashMap and ArrayList implementations. Class resources, selected JDK
images, tools and original archives were pinned before and after execution.
The [reflection probe](../../scripts/reference/block/wire_set/WireSetProbe.java)
constructs actual original keys in an isolated loader. It records complete
bucket/node graphs and shallow ArrayList storage without modifying the classes.

| Case, run separately on each side twice | Observed behavior |
| --- | --- |
| Lazy allocation, duplicates and snapshots | First add creates capacity 16 and threshold 12. Duplicate adds retain the original key and node. Clear retains the table; a snapshot keeps its old keys through refill. |
| Ordinary growth | Capacity becomes 16, 32, 64 and 128 at sizes 1, 13, 25 and 49. Resize preserves each ordinary split's relative order. |
| Equal full hashes | Distinct `(i,0,-i*8976890)` keys grow to capacity 32 at size 9 and 64 at size 10, then treeify at size 11. Trees retain full coordinate equality. |
| Tree split | Sixteen multiples of 64 share a bucket. Growth at size 49 splits it into two eight-node trees, preserving all 48 previously active node identities. |

Client and server equal-hash trees have different roots and traversal orders.
Their retained object identity hashes explain each observed tie decision;
coordinate hashes alone cannot reproduce that order. Some collision coordinates
exceed World bounds. These are collection calls, not demonstrated World geometry.

The observer holds objects strongly, reflects fields, fingerprints Class
resources and initializes the Map's cached keySet view through iteration. Its
own identity-hash reads occur only after the final encoded state. Those reads
can assign previously unused hashes. No uninstrumented equivalence or
garbage-collection lifetime claim follows.

## Native ownership and ordering

The actual Registry-created wire Block owns the Set and its power flag. Worlds
using that Registry share them. Set and Map construction remain two separate
permanent arena allocations. Reclaimable keys, nodes, tables and snapshots
share the Registry byte bound with permanent allocations and class-owned
ItemStacks. Current and peak accounting include overlapping replacements.

Every fresh add or remove constructs a key, including duplicate or absent
coordinates. Successful duplicate insertion preserves the existing member.
Snapshots hold the original keys in bucket traversal order. Copy and clear are
separate operations; nested snapshots survive removal, clear and refill.
Outstanding snapshots prevent Registry destruction before any owner retires.

Each reached native collection operation commits its complete logical transition
or preserves the Set. Allocation history, cached identity inputs and peak bytes
can advance on failure. Gameplay operations preserve Registry constructor
diagnostics. This bounded resource policy does not reproduce Java
OutOfMemoryError prefixes.

An optional input supplies identity hashes lazily, once per successfully
supplied key. Bind it before any key or table construction. Distinct full hashes
can form trees without this input. A required equal-hash tie with no input
returns `MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY` and preserves the logical Set.
Native object serials describe correspondence and never choose ordering.
Measured inputs establish conditional collection behavior; the default JVM
identity generator and its surrounding call history remain unimplemented.

## Installed identity-source research

Static research binds part of that dependency to the selected Windows x64
Temurin `17.0.17+10` installation. Its `jvm.dll` has SHA-256
`5539382b75556a9873fafa2d8b7d0b36b169ac436a3608467da4f74c2600cbce`.
The official [debug archive](https://github.com/adoptium/temurin17-binaries/releases/tag/jdk-17.0.17%2B10)
matches its published SHA-256
`5b28781b15da6c5821bb36ac998452f1617c56408dc5198c404b8f8566a4bf4b`.
Its selected `jvm.pdb` matches the installed DLL's CodeView GUID
`4122B868-D49F-1967-841D-94A6BA98A7A6`, age 1. Matching symbols bind the inspected
`JVM_IHashCode` call path through `ObjectSynchronizer::FastHashCode` to
`get_next_hash`. This was file inspection; no JVM was launched for this research.

The installed generator corroborates the
[selected source](https://github.com/adoptium/jdk17u/blob/3ae1373f8abb8add664985ab772915e681f56ea0/src/hotspot/share/runtime/synchronizer.cpp#L851-L927):
selector 5 uses four thread-local 32-bit words, unsigned xor/shift arithmetic,
a 31-bit result mask and `0xBAD` for zero. The installed thread constructor
corroborates X from `os::random()`, Y=842502087, Z=`0x8767` and W=273326509.
File data contains selector 5 and shared RNG seed 1234567. These initial file
values do not establish any application's effective flag or live state.

This narrows the remaining implementation work without supplying a default
identity source. Thread construction, shared RNG consumers, prior identity
requests, cached object hashes and candidate installation can affect later tie
decisions. Intrinsic/interpreter entry paths and the complete relevant startup
and concurrency history still need qualification. The current no-drop wire
line has no required equal-full-hash identity ties. Its passing comparisons do
not exercise this dependency. Captured per-key hashes or RNG state would remain
conditional replay inputs.

## Development validation

The dirty development source based on `6ddfa8e` passes 199 Windows MSVC CTests.
GCC and Clang each pass 196 of 201 CTests, with the same five Python harnesses
timing out against mounted Windows sources. All five pass separately from a
byte-identical native Linux source copy, under their original deadlines and
assertions, using the unchanged built probes. Each supplemental run executes
189 Python checks. Linux native tests use ASan, UBSan and leak checks. The
original failed full-suite reports remain failed; these supplements do not
constitute a fresh passing full suite or clean-commit qualification.

`block.registry_managed` checks joint allocation admission, released-budget
reuse, surviving payloads and actual ItemStack allocation. The
`block.wire_position_set` contract checks duplicate identity, nested snapshots,
reclamation, resource failures and the identity-input boundary on both sides.

Separate local comparisons pass on MSVC, GCC and Clang. Each compiler matches
sixteen original streams: 708 states, 17,964 node observations, 5,224 snapshot
keys and 64 required identity inputs. Comparisons preserve replacement/reuse,
tree links and colors, bucket/table history and shallow aliases. An additional
file-only Windows audit accepts all sixteen baselines and rejects 296 corrupted
controls. This remains finite conditional collection evidence, with no actual
wire/World callback, autonomous identity-source or failure/OOME qualification.

## Committed collection checkpoint

At `1f462fa`, [hosted verification](https://github.com/frankischilling/minecraft-c/actions/runs/40020805241)
passes all four jobs and 607 public/JDK CTests: 201 MSVC, 203 GCC and 203 Clang.
All 576 C/header files pass clang-format 18. The actual checkout is merge
`146c455741c9850ff540fe92120a9d0f92ee6f8a`, with the same complete tree as that
code checkpoint: `68b0a3e06dfe573fee4c6c3503251799799e2228`. Every registered test
has an actual Passed result; no failed, skipped or unrun CTest is recorded.
Controller mirrors and lifecycle interval controls also pass. Hosted checks
use no supplied Minecraft archives. This checkpoint predates wire lifecycle
integration; its native contracts have the separate scope linked above.

## Reproduction

Public native contracts require no Java or original assets:

```sh
ctest --test-dir build --output-on-failure -R '^block\.(registry_managed|wire_position_set)$'
```

The original probe accepts `ORIGINAL_JAR client|server CASE`, where CASE is
`lazy_duplicate_snapshot`, `equal_hash_tree`, `resize` or `tree_split`. Compile
both authored helpers into a fresh ignored directory using the selected JDK:

```sh
javac -d .local/wire-classes scripts/reference/block/wire_set/ClassResourcePins.java scripts/reference/block/wire_set/WireSetProbe.java
java -Xmx256m --add-opens java.base/java.util=ALL-UNNAMED -cp .local/wire-classes WireSetProbe b1.7.3.jar client tree_split
```

Capture complete stdout/stderr, tool/archive hashes and process results for
each independent invocation. Direct probe output alone does not repeat the
retained capture audit or qualify an original/native comparison. The standalone
native `mc_wire_set_probe --input PLAN` accepts constructor operations and
conditional identity inputs, never an expected collection graph. Automated
capture-to-plan and complete comparison drivers remain local research tools.
