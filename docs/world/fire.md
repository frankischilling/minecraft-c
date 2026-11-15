# Fire source bodies

Ordinary client Fire placement, addition, neighbor and tick calls now execute
their concrete Beta 1.7.3 bodies in
[fire.c](../../src/world/runtime/mutation/fire/fire.c). Reached Portal frame
creation, Stairs delegation and TNT priming also execute. The
[71-profile original packet](../../tests/reference/world/fire/README.md)
defines the verified component scope; unrestricted play remains unfinished.

## Fire order and state

Fire reads the actual Registry-owned encouragement and flammability arrays.
Their references are captured before the corresponding World block getter,
then indexed at the reached array operation. Null-array and provider failures
therefore retain their original precedence. Placement and Fire survival use
encouragement; [Lava ignition](fluids.md) uses Material flammability. A Crafting
Table can consequently trigger a Lava-created Fire that removes itself during
its addition body.

Addition first checks the block below against the live Obsidian static. A
match enters the actual Portal creation body. An ordinary false result
continues to Fire support and flammable-neighbor checks, then removal or a
40-tick scheduling request. Neighbor updates perform their own survival and
removal sequence. Unsupported Fire removal during a tick deliberately falls
through into later tick work.

The tick caches its old metadata before any age update. Below age 15 it draws
`nextInt(3)/2` and stores the incremented metadata; the later probabilities
still use the cached old age. Scheduling precedes the remaining survival,
burn and spread work. Netherrack, rain, support and flammable-neighbor tests
keep their source short circuits.

The six burn attempts run in +X, -X, -Y, +Y, -Z, +Z order, with Random bounds
300, 300, 250, 250, 300, 300. They draw even for a zero flammability entry.
Spread traverses X, Z, then Y; ordinary in-range coordinates produce 53
candidate cells. It keeps Java integer arithmetic and the original rain
queries, including the west test that uses the original Fire's Z coordinate.
Rain executes the actual World sky, height and Biome predicates. Prepared
Plains, Desert and Tundra cases exercise their different precipitation results.

## Reached Portal, Stairs and TNT calls

[Portal creation and neighbor updates](../../src/world/runtime/mutation/portal.c)
validate the real frame in either orientation. Successful creation stores
`World.o=true`, performs six actual Portal writes, then stores false after
normal completion. A callback failure preserves committed cells and leaves the
flag true. It does not restore an earlier flag value. Neighbor checks use the
actual Portal cells and frame; a failed check removes the reached cell through
the notifying World setter. Frame creation alone does not qualify Portal travel.

[Stairs lifecycle and tick wrappers](../../src/world/runtime/mutation/stairs/stairs.c)
delegate through the actual captured wrapped Block. Addition first invokes the
inherited empty neighbor body, then the wrapped addition. Removal and tick
invoke their corresponding wrapped methods. Stairs' inherited neighbor method
does not read the wrapped field. Null wrapped receivers and failed children
retain the reached wrapper and child diagnostics.

Burning TNT captures the old cell's TNT comparison before replacing or removing
that cell, then reloads the live TNT receiver for activation. The flag-one
[activation body](../../src/world/runtime/mutation/tnt/activation.c) observes
the remote-World guard, constructs a genuine
[PrimedTNT Entity](../../include/mc/entity/primed_tnt/construction.h), publishes
it, resolves `random.fuse`, and invokes the Entity sound overload. The ordinary
publication Boolean is discarded; the local Entity remains held through sound
even when publication returns false. Completed World/Chunk publication holds
survive a later callback failure.

The constructor calls the actual base Entity body and its default Random
construction. It sets size 0.98, offset height/2, position, one shared Math draw,
the original float direction arithmetic and IN sine/cosine velocities, fuse 80,
and previous position. The world-only constructor changes size without
rebuilding the base bounds. Native constructor controls cover that distinction,
Math-holder failure, missing trig data after earlier effects, canonical subtype
identity and lifetime. The legacy full constructor borrows the actual existing
IN table; it does not implement cold IN class initialization.

## Collection and caller evidence

The scheduled-membership owner `World.F` now appends linked-bin insertions and
preserves low/high relative order during growth. Its real traversal matches the
[JDK membership observations](../../tests/reference/java/scheduled_membership_fixtures.json).
The comparison uses the actual owner state, without sorting snapshot output.
Collision-tree and identity-dependent membership ordering remain unqualified.
The separate ordered queue retains its own tree history and record holds.

`World.P` uses the existing packed-Integer Set implementation. The
[independent World Set observation](../../tests/reference/java/world_set_order_fixtures.json)
checks the actual 361-Chunk input footprint and its insertion, growth, removal
and traversal history. That observation confirms the existing runtime order;
it introduces no change to the Integer Set implementation or its older goldens.

The Fire packet includes the real `World.l` scheduled-drain caller and one
Player-centered random-work call. The latter visits all 361 cached Chunks,
performs 28,880 samples and reaches exactly one Fire tick. Before/after states
retain complete Chunk planes, Random, light regions, weather, class counters,
scheduled queues and callback effects. Three World-tick interruption cases also
check retained callback receipt identity and checked retirement. Direct
ordinary diagnostics continue to borrow their live producer graph.

These results cover ordinary client calls. Canonical owned Fire/Portal/Stairs
mutation paths and server handlers remain separate work. PrimedTNT fuse ticks,
explosions, subtype NBT, rendering and collision virtuals, Flint and Steel use,
natural encounters, original graphics/audio and measured FPS remain unqualified.
Source class counters are explicit prepared inputs; unused default Entity
Random entropy and native Java stack objects are unpaired. The
[test record](../testing.md#fire-source-bodies) keeps validation results separate
from these behavior and ownership contracts.
