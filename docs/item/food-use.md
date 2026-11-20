# Food and soup use

Registered Food Items now execute their Beta 1.7.3 in-air bodies through the
ordinary ItemStack and single-player controller paths. The nine Items are
Apple, Mushroom Stew, Bread, Raw Porkchop, Cooked Porkchop, Golden Apple,
Raw Fish, Cooked Fish and Cookie. This is the original immediate-use behavior.

## Source behavior

Food reads, Java-wrap subtracts and stores Stack count before invoking Player
healing. There is no positive-count, full-health, remote-World or World-presence
gate. A NULL Player therefore fails after count changes. Neither this body nor
its in-air Stack wrapper increments the used-Item statistic or emits a sound.

Healing returns immediately when health is nonpositive. Otherwise it Java-wrap
adds the Item's healing amount, clamps values above 20, then stores hurt
resistance as the signed healing-period field divided by two. Overflow to a
negative health value survives the upper clamp. Ordinary Food and Cookie return
the same Stack, including count zero or negative.

Mushroom Stew first completes that same Food body. It then allocates a distinct
Stack before reading the live Bowl Item, invokes the Item-based Stack
constructor and returns a Bowl with count 1, damage 0 and animation 0. The
original Stew count still decreases. Even a larger declared Stew count is
replaced by one Bowl in the controller's selected slot.

The controller compares returned identity before testing a same-result count.
Changed identity or count replaces the selected slot; an exactly zero returned
count clears it. A negative count remains selected. Failures preserve the
completed count/healing prefix and stop before later slot replacement.

## Ownership

The Item-use call retains its input Stack and lazily holds the Player at the
reached healing invocation. Same-Stack results have a separate returned
occurrence. Soup's newly allocated Stack remains call-owned, including a RAW
constructor-failure prefix. Its borrowed result can be retained independently;
checked call retirement releases only the call's occurrences.

The internal Living healing kernel changes the real represented fields without
adding constructor, World, statistic or timing work. Native owner refusals stay
separate from original NULL-reference exceptions. Native RAW-owner, retain-limit
and nullable-Bowl controls do not establish original reachability of those
constructed states.

## Evidence and limits

The [original packet](../../tests/reference/item/food_use/README.md) records
four fresh original VMs: two repeat fourteen ItemStack calls and two repeat
fourteen controller calls. Windows MSVC and sanitized Linux native comparisons match
all 28 selected rows, including six NULL-Player failure prefixes, signed count
and health overflow, same/distinct result identity, selected-slot clearing or
replacement, returned Stack fields and actual zero used-statistic deltas.
Thirteen comparator controls reject changed observations. The native fixture
uses genuine App, statistics writer, World, Player, Inventory and Stack owners.

These finite method comparisons are separate from loaded SDK interaction,
save/reload, original pixels/audio, unrestricted survival play and frame rate.

## Saved Windows interaction

The optional `client.food_loop` scenario passes 1,806 checks in 8.46 seconds.
Its authored Player starts at health 5 with two Apples and three Mushroom Stews.
One ordinary right click leaves one Apple and health 9. The real hotbar-2 key
and one Soup click produce health 19 and a distinct Bowl with count 1. The
displaced Soup's count is 2; its temporary observation hold ends before saving.
Each click has one air-use, count store and equip reset, with no used statistic,
placement, sound or selected-slot clear. Actual HUD and held-item draws return.

Escape and Pause's Save and Quit button leave through the ordinary source path.
One passive owned NBT sample and independent Region/Player decoding agree on
health 19, slot 0 Apple/count 1 and slot 1 Bowl/count 1, both damage 0. Reselection
constructs distinct World/Player/input owners and preserves those values. The
selected slot resets to zero because this save schema does not store it.
All 1,976 selected inputs/products and 413 imported assets stay unchanged;
cleanup leaves no windows. The returned Bowl screenshot was inspected, without
an original-pixel comparison.

The initial fixture incorrectly compared a completed hand receipt's borrowed
Stack pointer after child retirement. A diagnostic replay proves every other
draw/HUD predicate true, that pointer NULL and the live renderer's Stack equal
to the expected Apple. The corrected observer requires retired pointers NULL
and reads identity through that same canonical live renderer, retaining every
scalar draw/HUD check. It changes no game behavior, input, camera or clocks.
Both failed captures and refused idle guards remain retained. The passing
guard and cohort monitor record no last-input timestamp changes; this does not
identify an input actor or establish unrestricted focus stability.

A private Linux Xvfb/Openbox probe reaches the saved World, health 5, inventory,
held rendering and HUD under Clang ASan/UBSan. It stops before the first Apple
input at the unchanged short-frame/focus admission. Its isolated mount namespace
preserves the real account directory, runtime, products and assets. Linux Food
interaction and save/reload remain unqualified pending diagnosis of that gate.

Further private diagnostics observe a real source Pause even after native and
SDL Canvas focus return. One explicitly declared Back to Game button action
restores the same World/Player and held-item readiness without changing health,
position, rotation or Inventory. Before Apple input, Pause is published again:
ticks advance from 24 to 34, but the last-tick age is 5,059 ms, the screen is
Pause and mouse capture is inactive. No Food input occurs. These retained
failures identify a focus/pause/coordinator boundary; they do not establish its
cause or justify changing the original admission, clocks or input behavior.
