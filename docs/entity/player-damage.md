# Player damage, armor and death

The client LocalPlayer now enters the inherited LivingEntity hurt body after
the Player damage statistic. A completed nonfatal hit applies armor, changes
health, sets hurt timers and the attacked flag, consumes the reached random
streams, and dispatches `random.hurt` through the actual WorldAccess listeners.
Repeated weaker hits return false; a stronger hit during hurt resistance
applies only the difference. The Player statistic still precedes that decision.

The runtime shares the LivingEntity body with the existing Mob damage paths.
Its health child dispatches to Player armor for LocalPlayer and to the inherited
health subtraction for the supported Mobs. This preserves the original virtual
call at both the fresh-hit and stronger-hit sites.

Fatal LocalPlayer damage now continues through the genuine Player death body.
The inherited LivingEntity death flag, Player state changes, Inventory drops
and death statistic execute before the hurt call returns. The
[death-screen and respawn path](../client/death-respawn.md) has its own component
and loaded-client verification below; it does not establish unrestricted combat.

## Armor calculation and wear

[`armor.c`](../../src/inventory/runtime/armor.c) implements client Inventory
`ix.f()I` and `ix.e(I)V`. The HUD uses the same armor-value body as damage.
Each selected armor piece reads its actual Item maximum through `iz.j()` and
`gm.f()I`, its current Stack damage, and its `wa.bl` defense. The calculation
weights defense by the total remaining durability. Java integer wrapping,
division and remainder are preserved; unusual signed fields are not clamped.

Player `gs.b(I)V` calculates `amount * (25 - armor) + bO`, wears armor using
the original amount, then stores the remainder modulo 25 and subtracts the
quotient from health. Inventory wear invokes the existing Stack durability
body, including the broken-Item statistic. A zero count runs the original
empty hook and clears that armor slot. Inventory wear does not set the
Inventory changed flag.

Each armor slot owns its Stack occurrence. Aliased slots read the same live
Stack in order, including stores made through an earlier slot. Clearing one
slot releases only that occurrence. A later failure retains earlier durability,
statistic and slot stores; it does not publish a completed Player health result.
The native Inventory active guard covers the entire value or wear call.

## Player death and Inventory drops

[`death.c`](../../src/entity/player/death.c) implements client `gs.b(sn)V`.
It invokes inherited LivingEntity death first, then changes size to `0.2f` by
`0.2f`, reapplies the current position and stores upward motion. The original
`Notch` username branch scatters an Apple before the Inventory. Player's
inherited `ls.j()` returns zero, so it does not inherit an animal's ordinary
death Item selection.

Inventory `ix.g()` visits the main slots before armor slots. Each nonnull slot
invokes the existing Player scatter producer and clears only after that child
returns. Aliased slots remain separate ordered occurrences. A failure preserves
earlier drops and cleared slots, plus the actual pending child and current slot;
native cleanup neither rolls the action back nor repeats it. Attacker-dependent
motion or the null-attacker zeroing branch, final offset and death statistic
follow in source order. The reached Player removal body also performs its two
actual Container closes, including aliases, through checked close ownership.

## Verification scope

The [original observations](../../tests/reference/entity/player_hurt/README.md)
cover prepared Player inputs, consecutive calls on the same Player, armor
carry and break boundaries, early returns and selected failure prefixes. They
record complete selected Player fields, Inventory/Stack state, statistics,
Random streams, World fields, sounds and identity relations. The original
objects and native fixture have their documented construction scopes.

The unchanged 28-call packet now compares 21,247 scalar values and passes 529
native ownership/state assertions on Windows and Linux Clang ASan/UBSan. Its
fatal case retains the sound checkpoint and completed death separately. That
case has an empty Inventory, a non-Notch username and no attacker. Normal and
optimized Python readers each reject 64 authored corruptions.

Separate [Player death controls](../../tests/entity/player/test_death.c) pass
663 checks on each native host. Inventory scattering, the Notch Apple, nonnull
attackers and Container closes have authored native coverage; they do not
inherit the original runtime qualification of the single fatal profile.

Cactus contact now carries the invoking Registry and Math through actual
movement dispatch. Three authored zero-displacement movement calls exercise
nonfatal hurt, repeated-hit immunity and fatal scattering against a real Chunk
cell. The immunity boundary is an explicit fixture input. Math stays cold until
the reached scatter needs it. This is native component coverage, not a loaded
hazard encounter or an original cactus runtime comparison.

Separate [native armor controls](../../tests/inventory/player/test_armor.c)
exercise a four-piece weighted value, a Stack shared by two armor slots and a
main slot, a later invalid slot after an earlier break, null and active owners,
and signed durability wrap. The
[statistic callsite test](../../tests/client/statistics/test_player_callsites.c)
also invokes two consecutive ordinary Entity burning-timer bodies. The first
damages the Player and emits sound; the second advances the timer without
another hit. Those authored timer inputs do not establish a full fire-contact
or loaded-World gameplay session.

## Fall landing sound

The inherited client `ls.b(F)V` now continues after its damage call, including
when hurt resistance returns false. It reloads the current World and position,
floors `x`, `y - 0.20000000298023224 - offset` and `z`, then reads the landing
Block. A positive Block ID reaches the actual `Block.m` element and StepSound.
The sound call uses `StepSound.d()`, volume times `0.5f` and pitch times `0.75f`.
Its freshly allocated name remains live through every WorldAccess recipient.

The unchanged client bytecode establishes the continuation from the damage
invocation at 22 through the Block query at 61, StepSound calls at 86/91/99,
World sound at 105 and return at 108. Authored callsite controls exercise a
hurt callback that moves the Player, repeated rejected damage followed by
landing sound, air, callback refusal and a null World after completed hurt.
The fall accumulator clears only after the full callback returns successfully.
These controls do not compare original landing runtime output or loaded falls.

At source checkpoint `359e663a`, the four new armor/hurt cases pass in all three
hosted native jobs. The actual-App callsite test passes 297 checks on Windows
and sanitized Linux. The [full test record](../testing.md#player-damage-armor-and-landing)
separates those results from incomplete local suites and unresolved GUI failures.

## Remaining work

The [sleeping wake](player-damage-wake.md) and
[Living ally-alert body](player-ally-alert.md) have separate finite source,
ownership and comparison scopes. Arrow attacker resolution remains unfinished.
Original runtime comparisons for broader fatal inputs and
scattered Inventories remain separate work. The supported loaded Overworld
respawn route has its [declared scope](../client/death-respawn.md), including
saved-Bed validation, exact placement and missing-Bed notification. Prepared
[Bed use and sleep/wake scenes](../client/bed-sleep.md) retain their own scope.
Unrestricted sleep gameplay, Nether transition and centered
ChunkProvider branches remain unavailable.
These results do not establish full combat, unrestricted survival, audio output
equality or a frame-rate improvement.
