# Hoe and Seeds use

Registered Hoes and Seeds execute their original client on-block bodies through
the actual ItemStack wrapper. Hoe IDs 290 through 294 use `la`; Seeds 295 uses
`rk`. The loaded client can till Dirt, plant Seeds, save, and reload the result
in the bounded scenario below. Dye's distinct `km` bone meal override has its
separate [Block-use and loaded-Crop qualification](dye-use.md).

## Body and wrapper order

The authority is the unchanged Beta 1.7.3 client archive, SHA256
`af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c`.
The [executed original packet](../../tests/reference/item/farming_use/README.md)
keeps direct Item calls separate from `iz.a(Player,World,IIII)Z` calls.

Hoe reads both the target Block and the Block above it, even for face 0. Its
predicate is `(face != 0 && above == Air && target == live Grass ID) || target ==
live Dirt ID`. Dirt therefore bypasses the face and above gates. The body
captures the actual Farmland object, computes each sound coordinate as
`(double)((float)coordinate + 0.5f)`, and emits its step sound. Volume is
`(stepVolume + 1.0f) / 2.0f`; pitch is `stepPitch * 0.8f`, with the original
binary32 operation order.

Only after sound returns does Hoe read the live remote flag. Remote success
returns without changing the Block or Stack. Local execution reloads the
captured Farmland's ID, calls the notifying World setter at LA140, discards its
ordinary Boolean result, and calls actual Stack durability with amount 1 at
LA147. A normal false setter still reaches durability and a true Item result.
A failed setter or callback preserves its completed prefix and stops there.

Seeds rejects every non-UP face before dereferencing World, Stack or Player in
the Item body. UP reads the target and live Farmland ID, then calls the actual
above-air predicate, whose child performs one ordinary acquiring Block read.
On success it reads the receiver's private planting ID, calls the notifying
setter at RK59, ignores the Boolean result, and reloads/subtracts/stores Stack
count at RK65/69/70. There is no positive-count gate, remote gate, sound or
durability call. Signed subtraction wraps and does not clamp. Planting at Y127
or with planting ID 0 can therefore consume Seeds after a genuine false setter.

The enclosing IZ wrapper resolves the actual current Item and invokes its
on-block body at 14. Only a true result reaches `jl.E[stack.id]` and the Player
used-statistic call at 34; the array capture precedes the late Stack-ID read.
A direct Item call has no such used statistic. A NULL Player can succeed in a
direct body yet fail in the wrapper after Block, count or durability effects.

Hoe reuses the existing [durability body](combat.md#durability-and-break):
separate Item lookups, Java-wrapped damage, strict `damage > maximum`, optional
Player broken statistic before count changes, negative-count clamp and damage
reset. It adds no break sound or particle callback.

## Retained calls

Each Item-use call keeps its exact Registry and Stack, reached World, separate
query diagnostics, callback producers and mutation/sound children until checked
retirement. The two farming queries have distinct retained allocations; the
second query does not overwrite an earlier borrowed diagnostic.

At LA147 a nullable lazy hold retains a nonnull Player before the durability
child. The later IZ34 statistic reuses that same occurrence. NULL and non-Player
durability arguments keep their original legality, with no added constructor,
phase or `instanceof` admission gate. Failed broken statistics retain the Player
and completed Stack damage. Retirement releases exactly one held occurrence.

The borrowed `mc_item_use_error.durability` points to the complete typed
`mc_item_combat_error`, including nested statistic diagnostics and completed
stores. It remains valid until successful call retirement. Source exceptions,
native refusals and cleanup failures stay distinct; retirement does not roll
back or replay gameplay work.

A separate real Screen/container terminal handoff checks a sealed Player whose
retain operation refuses with BUSY. The Item-use classifier preserves that
native status after the completed Hoe sound/Farmland prefix, without entering
durability or changing holds. The initial LIMIT misclassification remains a
retained failing control; checked cleanup completes before its assertion.

## Component and original comparison

`client.placement_item` passes 519 authored checks, including 32 farming calls.
The controls cover live singleton and captured-ID reads, remote changes during
sound, ignored false setters, signed counts/damage, callback failures, NULL
operands, retained Player counts and busy retirement. Some authored success
controls deliberately project selected statistics to NULL; those projections
are not used by the original comparison bridge.

The original packet records 118 calls: 61 direct Item bodies and 57 separately
prepared IZ wrappers. Two ordinary VMs and one read-only JDI VM returned
identical complete output. Its 4,246 journal events and 21 reached Code hashes
retain the executed source order. Four direct NULL-Stack calls remain
original-only; all 61 direct rows are outside the native bridge's selection.

`item.farming_reference` uses genuine App, statistics writer, Session,
LocalPlayer, Registry and four canonical Chunk owners. It compares all 57
wrapper effect rows: signed Stack fields, target/above cells, remote state,
used/broken statistic deltas, exact sound bits, ordered notifications and every
changed byte across Blocks, metadata, Sky light, Block light and height arrays.
It does not substitute NULL used/broken-statistic entries.

All selected fields, including outcome and exception class, match for 54 rows.
Three declared Java observer failures retain exact common effect prefixes but
pair original `IllegalStateException` with a separately asserted native status
refusal. Exception-class equivalence is unqualified for those three. Nine
negative comparator controls reject changed arrays, notifications, sound bits,
Stack/statistic state and invented source exceptions. The bridge passes on
Windows MSVC in 0.32 seconds and Linux Clang ASan/UBSan in 0.48 seconds. Each run
retains its actual return code and before/after executable, profile, controller,
packet and resource hashes; intended game/statistics writes remain allowed.

Failures remain recorded. The first native bridge build rejected `strtok` under
MSVC `/WX`; a bounded portable cursor parser replaced it. The first executed
comparison then rejected maximum damage 128 versus original 59: the fixture had
read Item icon field `bh` instead of maximum field `a`. Correcting that bridge
mapping left the already-correct runtime durability body unchanged. Earlier
original capture attempts failed on Java final-field assignment/restoration;
the [packet records those attempts and its removed unexecuted rows](../../tests/reference/item/farming_use/README.md#capture-and-retained-failures).
Neither partial capture supplied the successful reference observations.

## Loaded till, plant and save cycle

The optional Windows `client.farming_loop` scenario passes in 6.84 seconds with
2,343 checks. It uses an authored saved scene and actual SDK input with 1,984
selected inputs/products and 413 imported assets unchanged. One fresh right
click tills Dirt; the actual hotbar 2 key selects Seeds; one further right
click plants. Each action increments its real used statistic once. The CK
placement predicate counters remain unchanged, proving these calls use the
farming bodies rather than BlockItem placement admission.

After the native BUSY classification fix, the rebuilt scenario separately
passes 2,640 checks in 9.76 seconds with the same exact save/readback. The
initial 6.84-second receipt remains separate evidence.

The returned state has Farmland 60, Crop 59 at stage 0, Wooden Hoe 290 count 1
with damage 1, and Seeds 295 count 1 with damage 0. Two target DK display-list
replacements complete. The 48 stage 0 position/UV vertices match the executed
original Crop geometry, while 48 color values match the source-lighting check.
The screenshot was viewed, without an original-pixel comparison.

Actual Escape enters Pause and button 1 performs save/title leave. One passive
pre-frame owned NBT sample supplies the expected state; an independent region
and NBT decoder reads Farmland 60/metadata 0, Crop 59/metadata 0 and exactly two
Inventory slots: slot 0 Hoe 290/count 1/damage 1 and slot 1 Seeds 295/count
1/damage 0. Reselection constructs distinct World and Player owners with those
saved cells and Stacks. Selected hotbar resets to 0 because this source save
format does not save the selected slot. Cleanup completes with no remaining
windows. Later paused-entry moisture/growth is observed rather than frozen or
repaired.

Enable this local scenario with `MC_ENABLE_CLIENT_FARMING_TESTS=ON`, display
tests and imported resources. The component, original comparison and loaded
scenario have separate scopes. They do not qualify unrestricted farming,
harvesting, original pixels, audio playback, server behavior or frame rate.
