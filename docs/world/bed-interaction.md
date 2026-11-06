# Bed neighbors and activation

The World implements the original Bed neighbor body and reached activation branches on
the actual client GAME World. [Item355 placement](../item/bed-use.md),
[rendering](../client/bed-world.md), [Player sleep rejection](../entity/player-sleep.md)
and [saved-Bed respawn](bed-spawn.md) retain separate scopes.

## Neighbor changes

`VE.b(World,III,neighborID)` reads the current metadata, direction and live
facing table. A head checks the foot at 48; an absent partner triggers the
notifying removal at 64 without a drop. A foot checks its head at 94 and
removes itself at 110 if absent. Only after that removal returns does it read
the current remote flag at 115 and, for a local World, enter the actual drop
body at 129. There is no early remote gate on orphan removal.

[mutation/bed.c](../../src/world/runtime/mutation/bed.c) reuses the existing
notifying mutation, drop, Random and ItemEntity publication bodies. It retains
their reached diagnostic causes. Healthy pairs remain unchanged. The source
does not add a support check to this neighbor method.

## Activation and Player message

`VE.a(World,III,Player)` returns immediately for a remote World. A foot follows
the current facing table to its head and returns if that head is absent. The
Overworld provider check precedes occupancy handling. An occupied head scans
the actual World Player List in order, performs the actual Player class cast,
calls each sleeping predicate and compares the sleeping coordinate fields.
A matching sleeper reaches the occupied message. A stale occupied flag is
cleared through the genuine notifying metadata setter before trying sleep.

The reached `GS.b` operates on the clicked Player's actual World, independently
of the Block World's argument. Its actual CW result is compared by identity
with the activation code namespace's cells. Daytime reaches the Registry
literal `tile.bed.noSleep` and the actual Player message invocation at 424.
[placement/bed.inc](../../src/world/runtime/placement/bed.inc) retains each
query, mutation, Player and reached class owner until checked retirement.

The enclosing client supplies the Player message child through the existing
activation context. Its [DC wrapper](../../src/client/entity/player_message.h)
captures that Player's App, that App's UQ and calls the real translated
[HUD insertion](../client/chat.md). It does not substitute the active App or
an English string. The gameplay owner retires this message child before the
controller Use call so borrowed causes remain live through a failed call.

The retained night route calls the complete [Player sleep body](../entity/player-sleep.md).
Its GS defining Registry comes explicitly from the activation context; the
client supplies its actual Registry. A returned CW.a reaches the same notifying
occupancy setter used by wake. The placement owner retains the sleep child
through checked cleanup, including failed queries, bounds and callbacks.
The older finite-prefix comparison still stops before119. The Nether arm
retains its reached notifying removals before the explosion256 frontier.

## Original comparison boundary

The [packet](../../tests/reference/world/bed_interaction/README.md) contains
85 original profiles: 64 neighbor calls, 16 healthy-pair daytime activations,
four absent-head activations and one remote/null-Player activation. Two fresh
VMs agree on finite observations. A third read-only JDI VM records 408 selected
calls and 20 live Code fingerprints. The packet preserves complete Block and
metadata plane hashes, actual drops/Random state, Player preservation, CW
identity and the reached message key.

The native World replay compares 2,569 scalar values and losslessly reconstructs
all 49,152 Block/metadata bytes per plane pair. Its authored message adapter
checks the actual target, key and ordering at the World boundary. Original
notice contents and prepared original App/Font observations remain retained
outside that comparison. This World adapter alone supplies no native
DC/App/UQ or drawing qualification; separate client controls cover that join.

```sh
ctest --test-dir build --output-on-failure -R '^(world[.]bed_interaction|tools[.]bed_interaction_packet(_optimized)?)$'
```

The separate [sleep/wake comparison](../entity/player-sleep.md#evidence-and-limits)
qualifies 76 prepared Player components and 9,784 scalar values on Windows and
sanitized Linux. Eight native World caller controls also pass, including
complete 361-Chunk/28,880-sample tails and retained failure/cleanup paths.
The eleven HUD timer inputs and four-direction Player Display calls have
separate native graphics scope. None extends the original 85-profile World
packet or qualifies loaded-night gameplay.

Default-hostile CQ33/FW7, Nether explosion256, original whole-frame pixels/audio,
Linux loaded Bed play and sustained FPS remain unfinished or unqualified.

The corrected native replay passes on Windows in 1.49 CTest seconds and on
sanitized Linux in 3.88 seconds, with 4,834 native checks. Its first failure
exposed an exact-kind check that rejected concrete DC objects at VE301; the
actual Class assignability check now accepts DC and preserves the true
non-Player ClassCastException control. The first loaded Windows opportunity
fails during loading while Remote Desktop holds focus, before any Bed action.
That retained failure supplies no loaded placement or message qualification.
[Testing](../testing.md#bed-placement-and-daytime-interaction) records both scopes.
