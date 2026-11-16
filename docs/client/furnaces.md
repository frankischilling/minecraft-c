# Furnaces

The client connects Furnace placement and activation to a real three-slot
TileEntity, its smelting loop, container, output Slot and screen. Input and fuel
use ordinary Slots. The output Slot rejects insertion. The screen reads the
same TileEntity that the World ticks and saves; it does not pause the World.

## Original behavior

`SK` stores remaining burn time, the last fuel duration and cook progress.
Positive burn time decrements before the World remote flag is read. A recipe
and usable fuel can start a burn; cooking advances only while burning and able
to smelt. At exactly 200, progress resets before the actual smelt call. Fuel
continues burning without input or with blocked output. Ignition, smelting and
burning-state transitions dirty the TileEntity; ordinary countdown does not.

The original fuel selector gives wood-material Blocks 300 ticks, sticks 100,
coal 1,600, lava buckets 20,000 and saplings 100. Lava consumption leaves no
bucket. Smelting uses the existing ten-recipe singleton and its actual result
Stacks. Empty output receives a clone. Existing output increments by one.
The original capacity expression and signed arithmetic remain intact.

Inventory assignment and splitting do not call dirty themselves. NBT writes
`BurnTime` and `CookTime` as Shorts and the three inventory entries with Byte
Slots. The fuel duration is recomputed from the current fuel after loading.
When that duration is zero, the client flame-scale getter stores 200 before
dividing. It is not a persisted counter.

`BM` has 39 Slots: input, fuel, output and the Player's 36 visible cells.
Shift transfers output to the Player in reverse order. Player main inventory
and hotbar transfer between each other; Shift does not route Player items into
input or fuel. `VQ` invokes the real Stack crafted callback, tests Iron and Fish
awards independently, then invokes the ordinary Slot changed body. The local
Player's existing achievement prerequisite checks still apply.

`OV` uses `/gui/furnace.png`, the inherited 176 by 166 container layout and
the original labels, flame and cook indicators. Input, close and eligibility
use the actual `ID`, `DW`, `GP` and Player continuations. No listener or packet
simulation drives its progress. The original client caller inventory finds
`BM.a()` through `DW.a(LW)` and its superclass call, with no direct Player tick
invocation of that method.

## Block changes and ownership

`TC.c` runs the inherited TileEntity factory and World installation before
its orientation helper. That helper queries all four neighbors before its
fresh, short-circuited opacity-array reads. Placed-by orientation reads Living
yaw before activating the defining Math class and writing metadata.

When burning changes, `TC.a` saves metadata and the actual old TileEntity,
sets its class-static suppression flag, replaces the Block, clears the flag,
restores metadata, validates the saved TileEntity and reinstalls that same
object. A failed replacement does not execute a later flag reset or repair.

The actual deferred World and Chunk publication paths remain involved. In the
recorded original lifecycle, each variant replacement leaves a valid, empty
temporary Furnace in the active TileEntity list. The saved Furnace remains
the mapped owner. Later World ticks keep those empty objects. Native shutdown
counts and retires their actual owning occurrences without changing source
lists to hide them.

Furnace screen and container fields retain their own occurrences of one typed
inventory view. That view holds the actual TileEntity. Terminal admission
counts that root alongside World lists and Chunk maps. Extra external holds
refuse collection; native retirement supplies no source close, inventory drop,
counter reset or World tick.

## Evidence and remaining work

The current retained-lookup checkpoint passes all 1,341 Windows tests in
618.75 seconds, plus 27 selected GCC and 28 selected Clang sanitizer tests.
The final monitor-2 Furnace cycle passes 9,113 native checks, Chest placement
passes 1,723 and Workbench passes 2,052. All 2,344 selected inputs and assets
remain unchanged during each process; source windows close at completion.
Four Furnace images were inspected. Independent readback finds Player Iron in
slot8 and the saved Furnace at Burn1396/Cook5. This save is distinct from the
earlier Burn1397/Cook4 save used by the original-client packet below.

The following component, lifecycle and earlier SDK results retain the preceding
Furnace checkpoint `9c2fb78e45465ac6d1eda5f122df994e8ee8ffec`.

The [component packet](../../tests/reference/inventory/furnace/README.md)
contains 207 unchanged-client observations in two ordinary and two passive
debugger VMs. The debugger journals agree on 2,754 entered calls and 74 live
Code pins. Native core and container controls compare their stated finite
projections; null-array/native-admission and exception-class coverage remain
explicitly separate.

The [World lifecycle packet](../../tests/reference/world/furnace_lifecycle/README.md)
records seven steps, 196 entry/exit phases and 23 live Code pins. Original
phase records are source evidence, not an implicit claim that native execution
replays every phase. Native World controls separately exercise real smelting,
same-Tile replacement, deferred publication and a failed observer continuation.

Nine selected tests pass on Windows MSVC Release and on Clang Debug with ASan,
UBSan and leak detection. Eight tests pass on the GCC sanitizer build, which
excludes client screens. The full Windows Release build and all 1,335 tests
pass in 531.50 seconds. A separate audit matches every configured name and ID,
complete unique ordinals, no skips or unrun cases and all 1,326 preceding tests.
Core replay compares 173 component profiles and
reports its 34 other profiles separately. Container replay covers all 27 `BM`
profiles; the real Player output fixture passes 1,374 checks. Native lifecycle
replay matches all seven original before/after projections, including identity
and valid temporary TileEntities, then completes checked native cleanup.

Twenty-two selected shared Clang regressions pass across a batch and a frozen
retry: Chest/Workbench/Inventory, terminal ownership, World ticking, Chunk
compilation and owned Tile lookup. The first batch passed 21/22; one native
lookup completed but its continuity guard rejected provenance metadata changed
during execution. Its retained failure is separate from the frozen retry, which
passes in 145.57 seconds without a code change.

The preceding Windows monitor-2 SDK cycle passes 9,242 native checks. It places the held
Furnace, manually inserts two ore and two coal, takes the first Iron, saves and
reselects the World during the same burn, then takes the second Iron. The
independent saved-state reader finds one Iron in Player slot8 and the remaining
ore/fuel in the actual Furnace, with BurnTime1397 and CookTime4. Those values
belong to this process; subsequent runs need not have identical wall-clock
save timing. Four captured images were inspected. Both source windows fit
monitor 2 before and after reload and are absent at completion. All 2,343
selected input fingerprints and the imported assets remain unchanged. An earlier
cycle passes 10,075 checks at its own recorded input checkpoint.

All six Chest profiles also pass on monitor 2 with these runtime changes,
including placement and open-screen shutdown. Inventory and Workbench save
regressions pass 808 and 2,114 checks. Their selected inputs and assets remain
unchanged during each process and their source windows close successfully.

The authored held Furnace has no crafting achievement history. Its actual
`buildFurnace` prerequisite is absent, so the original `DC` condition skips
the Iron award. This scene checks that presence gate and zero values on the
same real writer; the separate Player controls qualify positive Iron/Fish
awards with their declared prerequisites. No live statistic is granted to make
the scene pass.

Open-screen shutdown uses an authored SDL close request to the live Frame.
Native preparation preserves source aliases, counters, Stack fields, World
time/ticks and every retained save byte. The runner checks ordered input/cook,
save-entry/save/reload receipts against the retained files. Its 30 independent
corruption controls cover those joins, NBT and region bounds, PNG integrity,
native host continuity and input enumeration. These are native controls,
without an original pixel or audible-output comparison.

The [retained lazy lookup](../world/furnace-lazy-lookup.md) now joins the whole
client Furnace addition through actual factory, installation and orientation.
Arrow and Ray preserve failed child scopes and close them through checked
native-only unwind. Four declared native profiles qualify both shape paths,
source-field preservation and Source-exception/native-failure separation.

A separate [original save readback packet](../../tests/reference/world/furnace_save_readback/README.md)
loads copies of the earlier 9,242-check scene's actual saved files in two fresh
original client VMs. Both recover Block62/metadata2, ore15x1, coal263x1,
Burn1397/Cook4, recomputed duration1600 and serialized Player Iron in slot8.
The saved SK exists before lazy lookup and retains the same identity. This is
one C-to-original load/read boundary, with no original tick/save, concrete
Player construction or bidirectional roundtrip qualification.
Server ticking, multiplayer container synchronization, unrestricted save round
trips, sustained play, original pixels, audible output and FPS remain separate
qualification work.
