# MobSpawner preview rendering

The special TileEntity path joins Chunk compilation, RenderGlobal traversal,
the TileEntity dispatcher and the existing Entity renderer. The first supported
preview is the default Pig in a loaded MobSpawner. Spawner simulation and its
spawning predicates are separate work.

## Publication and traversal

`dk.a()V` captures the old B identities before clearing that List. After block
geometry, the new Set subtracts the old Set, adds its remaining identities to
the shared F List at584, then removes old identities absent from the new B at608.
An unchanged recompile does not append another preview. Removal preserves
surviving F order and removes every matching row, including duplicates. A later
failure preserves an earlier successful publication.

The native ledger records the actual List, index and TileEntity, with a separate
owning hold for each published row. It supplies the proof for RenderGlobal's
otherwise untyped element. A changed List object, malformed extent or unmatched
row refuses before dereferencing an unproved TileEntity. Clearing the actual
shared List ends those occurrences; releasing a Chunk's B does not end an
independent F occurrence. Inactive ledger cells are reused on recompilation.

NULL in the old B List remains a valid Set member. It needs no Tile object
hold; every non-NULL row still requires its exact Tile ownership proof. Component
fixtures that allocate Lists directly record that native identity separately
from their unchanged Source fields and expected original rows.

TileEntities inherit Object identity equality and hashing in the original.
The native [identity profile and Set](../reference/object-identity.md) now retain
lazy hashes, bucket/tree history and both size-dependent removeAll branches.
Multiple new identities publish together in Set traversal order after a complete
toArray. Hashes use the approved native startup stream; concrete order can differ
from a particular JVM launch. Duplicate B entries still contribute to the
ArrayList size used by removeAll.

`n.a(BT,YN,F)V` reads the live size at534, captures the fresh LL singleton
at542, reads the actual row at551 and calls LL at560. Each successful retained
child retires before the next iteration. A failed child remains owned by the
same N call until checked cleanup. Copied scalar receipts survive cleanup;
borrowed child and diagnostic views do not.

## Dispatcher and cached preview

The ordinary LL entry evaluates TileEntity distance against4096 before querying
World brightness and setting color. It then reads the Tile's coordinates and
the dispatcher origins and enters the coordinates overload. That overload
looks up the actual runtime Class; a missing renderer returns without a draw.
The direct coordinates overload has no distance or brightness wrapper.

AG uses its own name-to-Entity cache. A miss invokes the
[named Entity factory](../testing/named-entity-factory.md) with NULL World and
stores the nullable result. A cached NULL remains a miss on the next draw.
A nonnull preview receives the Tile's actual World through the Entity World
setter, the interpolated Spawner rotation and the original transform/scale.
It then enters the existing TH coordinates overload. The cache owns this Entity;
it is never inserted into the World's Entity list. Cache destruction releases
its Entity and name occurrences, including their World bindings.

The entry points are in
[draw.h](../../include/mc/client/render/tile_entity_renderer/draw.h).
Finite storage limits are native admission policy. They do not replace Source
allocation, reflection, exception or graphics behavior.

## Validation and remaining work

[test_spawner_frame.py](../../tests/client/test_spawner_frame.py) authors one
saved Pig Spawner with a long positive delay and uses the ordinary selection
and World loop. Acceptance requires sixteen full World/HUD frames across
ordinary save and reselection, the real Pig model and coordinates TH child,
stable cached preview identity, rebinding to the new World and checked
child/window cleanup. Every sampled actual World Entity list must exclude the
preview. Selected Chunk NBT must retain the Spawner and have an empty ordinary
Entity list. Native Windows captures must place
both source windows on physical monitor2. The fixture records current
source/product/DLL and imported-asset continuity.

The final two-epoch Windows capture passes 1,236 checks. Its first draw creates
one Pig and compiles six model parts, 36 faces and 144 vertices. Fifteen later
draws reuse the cache and compiled parts, including all eight after reselection.
These are actual model-call and lazy-compilation counts, not an FPS measurement.
The [sanitized native record](../../tests/benchmarks/spawner_render_windows.json)
binds those finite observations to selected executed-input, image and private
receipt hashes. SDK stdout is newline-normalized text; the separate factory
runner retains raw bounded pipes. Neither record supplies original pixels.

The named-factory fixture passes 552 checks on Windows, GCC and Clang. Its direct original-backed
subset is three Pig constructor projections and the same receiver's NULL-to-World
transition; other checks cover native ownership, initialization and explicit
failure frontiers. The packet audit rejects 64 corruptions in ordinary and
optimized Python on Windows and WSL. These totals are not counts of original
comparisons. Earlier compile/test/transport failures remain in private evidence;
the qualifying records are separate fresh processes.

The complete MSVC Release build and all 1,345 configured Windows tests pass in
472.08 seconds. Every preceding name remains, with four additions, unique
Start/completion joins and no skipped or unrun tests. GCC 15.2 passes 29 selected
tests in 103.05 seconds. Clang 21.1.8 passes 20 selected tests in 53.86 seconds
plus the two corrected regressions in 1.60 seconds. Both Linux configurations
use Debug `-g1`, ASan, UBSan and explicit leak detection. The regressions preserve
all 42 original selector outcomes and 31 recenter rows.

The 31 GB WSL filesystem could not hold both complete sanitizer executable
trees. Those failed build attempts remain separate from the selected passes;
generated products were cleaned before rebuilding the affected targets. No
sanitizer or assertion was disabled. Validation text and current admission
fingerprints were updated after the completed checks; runtime and harness
bytes remain unchanged. Hosted results must name their exact commit separately.

The original named-factory observations have their own finite evidence packet.
They do not qualify composed AG graphics, original pixels or full-frame parity.
Sign and MovingPiston drawing, larger new-identity Sets, unsupported named
constructors, sustained play and FPS measurements remain open. The separate
[Spawner tick path](../world/spawner-tick.md) now joins the actual expired-delay
factory, predicate, publication and particle methods; its evidence does not
extend the original graphics claims above.
