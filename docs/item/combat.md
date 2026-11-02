# Held Item damage and hit bodies

Player attacks now dispatch through the actual Item Class for all 38 runtime
Item kinds in the original client namespace. The original registry inventory
contains 202 nonnull IDs. The dispatch table resolves the inherited GM bodies,
DQ tool bodies, QD Sword bodies and SA Saddle hit body. It uses Class identity
and live fields, with no Item-ID substitute or added READY requirement.

## Attack and hit order

The first Inventory read supplies attack damage. GM returns 1. DQ reads its
stored tool damage; QD reads its distinct Sword damage field. For example,
the original Iron Sword returns 8 and Diamond Sword returns 10. These are
Beta 1.7.3 values. Player adds the downward-velocity bonus before calling target
hurt, then discards that Boolean result.

After hurt returns, Player reads its current Inventory and held Stack again.
That Stack can differ from the one used for damage. Its hit method performs
a separate active use and capture of the actual `gm.c` array, followed by a
fresh Stack ID read and signed array lookup. The earlier Item receiver is not
reused. GM's inherited hit returns false. DQ, QD and SA return true, so their
calls increment the used statistic even when hurt was rejected or the victim
died. Statistics capture their own actual array before the later Stack ID read.

## Durability and break

DQ requests 2 damage and QD requests 1 through the actual IZ durability body.
`IZ.e()` looks up whether the current Item has positive maximum damage. Only
then does the body add damage with Java integer wrapping and store it. It
reloads that damage and calls `IZ.j()` through a new Item-array capture and
Stack ID read. The two lookups remain independent.

A Stack breaks only when the stored damage is strictly greater than the newly
read maximum. Reaching the maximum exactly does not break it. For a Player
attacker, the broken statistic executes before count changes. The body then
reloads count, subtracts 1 with integer wrapping, stores it, reloads it for the
negative test, clamps a negative result to 0, and finally resets damage to 0.
Failures retain every completed store and statistic.

This source body has no modern break sound or break-particle call. The later
`IZ.a(GS)` body is a literal return. Player clears a nonpositive held Stack
through the real `GS.H` and `IX.a(index,NULL)` path. The Inventory setter receiver
and selected-index receiver are separate source reads; the setter preserves
main/armor array choice and source null/bounds failures. Clearing does not add
an Inventory dirty store or change the retained Stack's count.

## Saddle

SA uses the actual Pig Class test and saddle watcher. An unsaddled Pig becomes
saddled, then the same Stack count decreases by 1 without a clamp. A Saddle
whose count is 0 becomes -1; a count of -3 becomes -4 before Player clears the slot.
An already saddled Pig or a non-Pig target leaves count unchanged. All three
paths return true and increment Item-used. This attack-hit behavior does not
establish the separate right-button Entity interaction.

## Evidence and limits

The [held attack packet](../../tests/reference/entity/held_item_attack/README.md)
contains 422 exact selected rows over genuine original App, World, Chunk,
Player, target and Stack owners. It covers all 38 Classes, all 22 direct GM IDs,
20 tools/Swords, 320 durability cases, 120 breaks and 16 Saddle cases. The first
32 Dirt/Wool observations are preserved, and the older five unarmed rows remain
unchanged. Expanded nonfatal durability scenes explicitly prepare Cow health 20.

Three additional health-10 Cow cases preserve genuine Diamond Sword death:
ordinary damage 10, falling damage 11 and a simultaneous Sword break. Each
process's own Cow-private Random state independently determines two sound
draws, pitch, `nextInt(3)` rejection, Leather quantity and ordered publication.
The two original ordinary cases drop 2 and 0 Items. Their raw results remain
recorded; 40 fixed fields compare only after each process passes its own audit.
Constructor and shared Math entropy remain independent.

The Windows component selection passes eight tests in 17.75 seconds. Its 425
genuine native attacks pass 37,416 checks; the attack packet's ten corruption
tests and geometry packet's eight tests pass. The reached renderer bodies have
their [separate geometry scope](../client/first-person-hand.md#held-item-leaves-and-extrusion).
Direct Item combat passes 588 checks. The Player attack component passes 1,804,
including 1,056 checks in twelve new callback/order controls, with the original
five unarmed rows unchanged. These authored controls retain their own scope.
The [damaged-Item HUD bar](../client/gui-ingame.md#damaged-item-overlay) now has a
passing selected CPU comparison: nine profiles, 24 rectangle prefixes and 576
active XYZ/color words. The original completes 24 draws; native CPU prefixes
complete zero. Each owns and validates its actual inactive backing without a
fabricated clear. Eight overlay packet controls and the three-test selection
pass in 0.76 seconds.

The same frozen runtime passes the Windows SDK Dirt profile in 14.20 seconds
with 1,446 checks and the Iron Sword break profile in 16.74 seconds with 3,631
checks. The isolated unarmed profile passes in 13.22 seconds with 2,106 checks.
Each observes three hand frames before and after the actual click, twenty GY
particles, removal at death age 21, a real post-death autosave and cleanup.
Independent decoding matches all 40 live Inventory slots to the NBT at every
actual save entry and in the final disk image. These latest three runs publish
Wool without picking it up. A separate earlier 13.38-second Sword pass proves
actual Wool pickup and its slot-0 clone; that result is not combined with the
latest run's counts.

The frozen runtime also passes all 611 Windows public tests in 280.15 seconds
and all 606 sanitized Linux public tests in 145.58 seconds. Sixteen public
Inventory corruption tests run in CI. Earlier GUI-color, Dirt-hand,
maximum-durability and focus failures retain their historical scopes. The
[qualification record](../testing.md#held-item-attacks-and-direct-extrusion)
explains the corrected producer-specific Wool observer and its failed
all-World conservation assertion. Hosted checks apply to their exact tested
commit. These results establish neither complete combat, original pixels,
global focus stability nor FPS.
