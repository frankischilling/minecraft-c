# Player damage and armor

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

## Verification scope

The [original observations](../../tests/reference/entity/player_hurt/README.md)
cover prepared Player inputs, consecutive calls on the same Player, armor
carry and break boundaries, early returns and selected failure prefixes. They
record complete selected Player fields, Inventory/Stack state, statistics,
Random streams, World fields, sounds and identity relations. The original
objects and native fixture have their documented construction scopes.

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

## Remaining work

Fatal damage reaches the Player death invocation and reports the unfinished
dependency after the reached sound. Player death, inventory scattering,
death-screen and respawn composition are not complete. Sleeping wake, Arrow
attacker resolution and selected Living ally-alert paths remain unfinished.
This component does not establish full combat,
single-player playability, audio output equality or a frame-rate improvement.
