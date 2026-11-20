# Object identity and Tile sets

The accepted [native startup profile](../adr/0001-native-object-identity-profile.md)
uses an application-owned identity stream that outlives Worlds. The user approved
native startup rather than reproducing a selected JVM's complete startup history.
Lazy hashing, cached object identity and the selected Java collection behavior
remain required. Observed Thread words, object hashes and collection orders are
evidence outputs, never inputs to native gameplay.

## Identity state

The selected runtime is Windows x64 Temurin 17.0.17+10 HotSpot, using its default
`hashCode=5` algorithm. A first identity-hash request consumes one transformation
of the current logical Thread's four 32-bit xorshift words. The full new `W` word
is retained for the next transformation. The installed object hash is its low
31 bits, with zero replaced by `0xBAD`. A repeat request uses that object's cached
hash without consuming the requesting Thread, including a request from another
Thread. Null hashes to zero without a draw. Constructing an object does not itself
assign an identity hash.

[object_identity.c](../../src/core/java/object_identity.c) owns this transformation
and the native startup profile. Its startup sequence is separate from Java
`Random`, World randomness and `Math.random`. The application profile owns the
current main logical Thread above World and collection lifetimes; a World change
does not reset it. Fresh Tile and canonical Class owners have their own initially
unassigned cells. Hashes are neither serialized fields nor constructor inputs.
[identity.c](../../src/world/runtime/tile_entities/identity.c) checks actual held
owners before accessing those cells. This interface admits exclusive logical
Thread use; it does not model arbitrary concurrent OS threads, monitors or a race
to install an object header hash.

## Collection history

The private [Tile identity set](../../src/world/runtime/tile_entities/identity_set_internal.h)
accepts actual nullable Tile references and compares reference identity. Supported
Tile classes inherit `Object.hashCode` and `Object.equals` and are not Comparable.
Distinct references remain distinct when their hashes collide.

The set preserves lazy table allocation, `h ^ (h >>> 16)` spreading, duplicate
handling, bucket chains, stable resize splits, tree bins and removal history.
Tree ordering uses signed spread hashes, then the actual Source class names and
identity-hash tie rule where reached. Iteration and `toArray` follow ascending
buckets and each bucket's `next` chain, including tree bins; they do not sort
objects or walk trees in key order. The shared
[hash-table kernel](../../src/core/java/hash_table_internal.c) also serves the
existing [Integer set](java-semantics.md#integer-set-history).

`removeAll` uses the selected JDK's strict size comparison. If the receiver is
larger, it traverses the argument and performs ordinary removals. Otherwise,
including equal sizes, it traverses the receiver and uses iterator removal with
`movable=false` and the stored hash. ArrayList arguments retain duplicate entries
in their size and use linear identity comparisons. An unallocated set's `contains`
skips hashing; an allocated empty set still hashes. `remove` reaches hashing even
when the table is absent.

Each nonnull node owns one Tile hold. A complete `toArray` snapshot owns independent
holds and prevents Set retirement until released. Completed hash draws and earlier
bulk mutations survive a later failure. Native entry, storage and allocation
budgets have typed diagnostics; their failures are not claims about Java OOM
timing. Checked retirement releases native ownership without replaying Source
operations, clearing cached hashes or resetting Thread state.
Default Set limits are 65,536 entries, 8 MiB retained requested storage and
16 MiB temporary reservation, including attached snapshots. These are explicit
native bounds. The supported canonical Tile Class maps acquire the same lazy
Class cells before their reached hash operations; other Class-key maps retain
their separate implementation and verification scopes.

## DK compile joins

The original archive SHA-256 is
`af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c`.
The verified 627-byte `dk.a()V` Code SHA-256 is
`35602b4098cc8c800885e0460a88c514e8f579351b524a667befb39b3e037d10`.
The literal invoke PCs below map to
[source_chunk_compile.c](../../src/client/world/source_chunk_compile.c), which
keeps its Set children and borrowed failure details until checked retirement.
`B` is the chunk's Tile ArrayList; `F` is the shared render Tile ArrayList.

| Source PC | Operation and traversal | Native join |
| --- | --- | --- |
| 106 | Copy the old `B` encounter sequence into a fresh Set | `old_set` / `add_b` |
| 562 | Copy the newly collected `B` sequence into a fresh Set | `new_set` / `add_b` |
| 572 | Remove old Set members from the new Set | `mc_tile_identity_set_remove_all_set` |
| 584 | Append the complete new Set `toArray` sequence to fresh `F` | `publish_new`; snapshot before List publication |
| 596 | Remove fresh `B` members from the old Set | `mc_tile_identity_set_remove_all_span` |
| 608 | Remove old Set members from fresh `F`, preserving survivor order | `remove_old`; Set `contains` and ArrayList batch-removal prefix |

The old copy precedes `B.clear` at PC116. The real render scan appends eligible
Tiles to `B` at PC411. Fresh field reads and the collection encounter order remain
part of these joins; pointer sorting or insertion-order substitutes would change
which objects consume hashes and which order reaches `F`.

## Evidence and limits

Private qualification uses the selected runtime in fresh interpreter and default
VMs. Ordinary roles omit JDWP; debugger roles add it explicitly. These are bounded
cohorts with different observation scopes, not interchangeable repetitions.

| Cohort | Runs | Qualified observation |
| --- | --- | --- |
| Standalone identity control v3 | 4: ordinary/JDWP in both modes | Fresh and cached hashes; raw Thread/canary brackets around static-field, mirror-ID and primitive-array reads |
| Standalone event control | 4: ordinary/JDWP in both modes | Two boundary cycles; each debugger role has 2 stops and 4 reads covering event-thread suspension, frames, arguments, proved live raw slots and collection fields |
| Ordinary original DK | 2, one per mode | Genuine World/DK construction, two moving Piston Tiles and two direct compile calls |
| Original DK with JDI | 2, one per mode | The same Source operations with 12 literal pre/post PCs per compile and complete collection/header observations |

Each DK-JDI mode has 2 compile calls, 24 stops, 48 complete reads and 53 audited
passive brackets. The independent audit checked 928 bucket rows and 76 node rows
per mode, with no tree rows. It checked actual Tile headers against node spread
hashes, Set subtraction, List append/survivor order and cached reuse on the second
compile. Dead local slots were omitted; later Set reads used mirrors captured
while their slots were live. Read-only native snapshots, zero-GC checks, complete
process streams, normal exits and unchanged guarded inputs bound these results.

Separate JDK-only Integer controls observed 523 operations across seven scenarios
in each of four genuine replicates, two per VM mode. They retain full bucket,
tree, link and iterator state. GCC and Clang probes matched all 523 structural
rows; node correspondence uses the unique authored Integer key. A selective
mutant forcing iterator-removal `movable` guards true was rejected at an
iterator-removal structural check. These controls exercise the shared kernel's
tree/removal behavior beyond the two-Tile DK cases.

Native controls in [test_object_identity.c](../../tests/core/java/test_object_identity.c),
[test_identity_set.c](../../tests/world/runtime/tile_entities/test_identity_set.c)
and [test_chunk_tile_eligibility.c](../../tests/client/world/test_chunk_tile_eligibility.c) cover
authored identity state, collisions, ownership, failure prefixes and the compile
join. Authored native hashes are test inputs, not imported original observations. The native
identity leaf passes 71 checks, Tile Set controls pass 1,132 checks in both
client/server namespaces, and DK continuation controls pass 289 checks. The
earlier selected Windows, GCC and Clang groups pass 11, 9 and 10 tests respectively.
Final affected selections pass six Windows, nine GCC and twelve Clang tests;
both Linux groups enable address, undefined-behavior and leak checks. The new
`client.tile_entity_renderer_identity` control passes 299 checks for actual
lazy Class consumers, cached reads, collision chains and stable resize splits.
Its ninth-key backend control is separate from ordinary LL's eight concrete
keys and does not claim an original caller or tree-bin execution.

The final monitor-2 pushed-Stone scene passes 964 checks through two moving
Tiles, drawing, settled extension/retraction and ordinary save/reselection.
Furnace and dense-Chest regressions pass 9,540 and 1,016 checks. All three
processes preserve their selected inputs/assets and close their windows.
The [Integer workload record](../performance/integer-sets.md) reports mixed
costs, including an unresolved dispersed-operation regression.

The evidence does not establish observer-off equivalence, whole-VM/bootstrap
history, actual-DK collision/tree behavior, arbitrary Collection or concurrent
mutation behavior, complete game parity or an FPS result. Direct DK calls do not
qualify an original application-frame caller. No captured seed, hash or traversal
order supplies the native startup stream.
