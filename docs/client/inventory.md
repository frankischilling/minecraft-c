# Player Inventory

The configured Inventory key now opens the actual `UE -> ID -> DA` screen in
the singleplayer client. E is the default binding. The screen borrows the
Player's existing default Container (`GS.d`) and its 45 Slots; controller calls
read the current active Container (`GS.e`) when reached. Opening Inventory
does not replace either Container. `ID.c()` returns false, so ordinary World
ticks continue while Inventory is open.

## Source behavior and ownership

[Screen construction and events](../../src/client/gui/source_inventory_screen.c)
use a distinct static `ID.l` Item renderer, the real open-Inventory achievement,
and UE's button-list clear override. E and Escape close through `DC.r`, source
display replacement and `ID.h`. A dead or removed Player reaches the same
source close route from the screen tick.

The [Container kernel](../../src/inventory/container/interaction.c) implements
normal and right-click pickup, placement, splitting, swapping and merging,
armor acceptance, Shift transfer, crafting-output pickup, outside drops and
cursor/crafting-grid close. The
[Player adapter](../../src/entity/player/container_interaction.c) forwards the
actual singleplayer controller calls, including real Player drop/publication
and statistic children. It preserves fresh field reads, Stack aliases, return
clones and partial mutations. Closing clears cursor/grid cells only after
their reached drop children return.

[Inventory drawing](../../src/client/gui/source_inventory_drawing.c) uses the
actual Slots, carried cursor, Item renderer, Font and translation calls.
[The Player preview](../../src/client/gui/source_inventory_drawing_preview.c)
enters the real render manager and Player renderer. UE stores the current mouse
coordinates only after ID drawing returns. Its pose restoration and GL suffix
are source success continuations; failure retains the reached prefix without
a finally-style rollback.

Each screen reuses one native interaction record after checked retirement of
the preceding returned child. A failed child remains retained for diagnosis
and cleanup. Terminal preparation stops screen admission and drains native
adapter/controller and constructor Player/Container holds before World
admission. It leaves source fields and copied failure sites intact and hides
borrowed identities whose native holds have ended.

## Bounded evidence

The [original Container packet](../../tests/reference/inventory/container_interaction/README.md)
retains 73 unchanged-original calls, ten Item drops, 753 selected ordered
callbacks and 59 live Code pins. Its separate Item-name inventory covers all
202 Item IDs, 38 runtime classes and six declaring bodies, with 404 scalar
cases. The packet has 24 corruption controls. Drop comparison independently
audits each process's actual entropy and arithmetic before comparing the
declared deterministic projection; it does not assert complete call-trace or
cross-backend trig-bit equality.

Six focused Windows CPU tests passed in 2.16 seconds. Item-name controls made
3,139 checks; the original-comparison probe made 22,017 checks, including
negative source prefixes. These finite controls cover more interaction
branches than the SDK acceptance below. Five Pumpkin HUD geometry profiles
check signed dimensions and all 48 packed words. Block getter controls cover
IDs 86/91, inherited behavior and overflow.

The final common Windows SDK profile passed in 16.42 seconds through
[test_inventory.c](../../tests/client/gameplay/test_inventory.c) and its
[independent save verifier](../../tests/tools/client/test_inventory_loop.py).
Actual SDK keys and clicks open Inventory, return at least three UI frames and
advance the real World time by at least three ticks. They move saved Dirt with
left/right pickup, placement, splitting and merging, split and merge Arrows,
and draw the Arrow tooltip. E closes to three returned World/HUD frames with
the real Dirt hand. A second E opening draws the Player preview holding Dirt;
Escape then drops the carried six-Dirt Stack through the actual World
publication path with that same Stack identity.

Ordinary Save and quit returns to the title screen. Independent NBT decoding
requires exactly Dirt count 1 in slot 0 and Arrow count 10 in slot 10, plus one
saved World Item containing the six dropped Dirt. The unrelated actual
`r.9.9.mcr` region bytes remain unchanged. Checked cleanup completes and both
native windows are absent.

The final Pumpkin-head Windows SDK profile passed in 13.24 seconds. The actual HUD
overlay helper returns at source PC 133 and retains all 48 packed words. The
3D Pumpkin helmet and held Dirt enter two distinct, stable Item-render children;
the first child's native guard ends before the second enters, and both source
calls return. Independent saved NBT also requires the Pumpkin in armor slot
103, with count 1 and damage 0.

The [original drawing packet](../../tests/reference/client/inventory_drawing/README.md)
records two complete UE draws per ordinary/read-only JDI process on a real
Display, fourteen primary Part returns, seven first-draw compilations and
fourteen live Code pins. Four selected completed Tessellator batches retain
192 words. Shared native UB kernels match all 108 active words and audit the
84 inactive words against each process's own preceding backing. Those native
CPU prefixes complete zero draws; the SDK profiles above separately qualify
returned native UI frames. Thirty-one corruption controls protect the packet.

## Remaining qualification

The full Windows MSVC Release build passes. Its initial public run passes
618 of 620 tests in 295.95 seconds, with all 4,175 source inputs, 507 products
and 823 assets unchanged. One old input test still expected E to stop at the
former Inventory frontier; it now exercises the remaining F11 fullscreen
frontier. The other failure records lost Display focus without a specific
foreground-owner receipt. Both isolated tests pass in 53.72 seconds. The
initial broad run remains a failed run; all 620 named public tests have passing
results across the broad run and the two-test follow-up.

The full Linux Clang 18 Debug ASan/UBSan build and all 615 public tests pass
in 204.09 seconds under Xvfb/software OpenGL, with leak detection and
halt-on-error. Later test and documentation changes are synchronized and
checked separately; runtime sources stay unchanged. Formatting covers 1,965
C/header/fragments, and current source bindings cover 10,141 entries in 4,174
files.
The results above do not qualify original pixels, whole-frame parity, FPS, general focus
stability, Linux SDK behavior or Inventory rendering for every Item. Shift,
crafting and armor have finite component evidence; the SDK profiles qualify
the specific UI, cursor-close and save sequences described above.
