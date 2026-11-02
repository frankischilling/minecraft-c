# Crafting Table and Workbench

The client now has a distinct Workbench screen (`OO`) over the actual 3x3
Container (`IT`). It shares the inherited `ID` drawing and event bodies with
Player Inventory. A finite Windows SDK scenario crafts a Table from four Logs,
places and opens it, makes a Wooden Pickaxe, closes with leftover ingredients,
and performs ordinary Save and Quit. Independent decoding checks the exact
saved Inventory, every live Item and the placed Table. This does not establish
unrestricted crafting or a fully playable client.

## Opening and screen behavior

Crafting Table activation reads the actual World remote flag before using the
Player. A remote World returns true immediately. The local branch invokes the
actual Player Workbench method. The client override captures its Minecraft
receiver, constructs `OO` from the current Inventory, World and coordinates,
then performs synchronous display replacement. The
[activation caller](../../src/client/controller/use.c) retains this child and
its failure prefix before considering held-Item fallback.

`OO` constructs its own `IT`; the Player's default `AA` remains intact.
Inherited `ID.b` publishes that exact `IT` as the current Container. This differs
from `UE`, whose initialization override only clears its button list.
Workbench does not pause World ticks. Its background uses
`/gui/crafting.png`, and its labels and all 46 Slots use the actual Font, Item
renderer, cursor and translation paths. It has no Player-preview override.

The [shared ID implementation](../../src/client/gui/source_inventory_screen.c)
keeps separate source close children. `OO.h` first invokes inherited `ID.h`,
then closes its own freshly captured Container. The keyboard route first
executes `GS.r`, restoring the default Container, before displaying no screen.
Consequently direct `OO.h` and keyboard close have different caller contexts.
They are not interchangeable tests.

## Container semantics and lifetime

The [Container kernel](../../src/inventory/container/interaction_click.c)
implements the original Workbench Shift ranges: result to slots 10 through 45
in reverse, main slots 10 through 36 to hotbar slots 37 through 45, hotbar to
main, and crafting inputs to the full main/hotbar range. Recipe-output pickup
uses the real statistic, ingredient and container-Item children.

Workbench close first runs the inherited cursor-drop body. It then reads the
captured World remote flag. The local branch reads and drops each of nine
crafting cells. Original `IT` has no explicit grid clear here. Repeated direct
closes can therefore publish the same retained Stack aliases again; the
implementation preserves that behavior.

Native construction has separate allocation and constructor entry so a failed
`IT` prefix stays owned by its `OO`. Constructor failure, source exception and
native cleanup remain distinct. Each actual Player receiving the `ID.b` store
has a retained native publication record. Retiring an owned Workbench Container
while one of those Players still publishes it returns `BUSY` before consuming
rights or changing admission. Joining terminal cleanup while Workbench remains
open is unfinished. Ordinary source close restores the default field naturally;
native cleanup does not repair it.

Successful native input retirement ends its borrowed Inventory and screen-init
diagnostic views. Scalar failure sites and Source exception types remain
available. A refused retirement keeps the still-live views. This prevents a
failed Workbench activation from exposing a consumed screen through the
menu loop's retained gameplay-input report.

## Original and native component evidence

The [Container packet](../../tests/reference/inventory/workbench_interaction/README.md)
retains 116 ordinary and 116 read-only debugger calls on unchanged original
classes, 50 actual Item constructor/drop audits, 1,538 selected ordered
callbacks, 77 live Code pins and the complete 46-Slot graph. Thirty corruption
controls pass. The native probe matches all 116 selected rows with independent
entropy and arithmetic audits. The prior 73 Player Inventory rows still pass.
These comparisons retain their declared method and caller scopes.

The [drawing packet](../../tests/reference/client/workbench_drawing/README.md)
observes two complete original Workbench draws per ordinary/debugger process.
It records 15 live Code pins, 132 signed Block scalar queries and 72 returned
Tessellator batches: 3,456 raw words, with 2,400 active and 1,056 independently
audited inactive words. Four selected gradient/tile batches contain 108 active
words. The existing shared native UB prefix matches that selected projection;
it completes zero draws and does not itself enter native `OO`. Forty-six
corruption controls pass. Full native UI frames require the separate SDK run.

The original Log metadata getter treats only face 0 or 1 as an end face.
Negative face indices use the side branch. The native getter now preserves
that distinction. Log and Crafting Table inventory color and shape use their
actual inherited `UU` bodies. Crafting Table's bottom texture still invokes
the current `UU.y` receiver, including its null failure, rather than substituting
a fixed texture number.

Sequential crafting also displays intermediate recipe outputs. Two adjacent
Planks produce a Wooden Pressure Plate before the full Table recipe appears.
Its inventory shape hook changes all six shared Block bounds to
`(0,.375,0,1,.625,1)`; it does not use the inherited no-op hook. The
[original scalar packet](../../tests/reference/block/pressure_plate_inventory/README.md)
observes both registered Pressure Plates in two fresh ordinary processes,
including constructor bounds and repeated hook calls. Twenty-three packet
tests pass. Native controls retain that mutation through a later graphics
failure and preserve unrelated fields.

The Log Item icon reads the current `UU.K`, then calls that actual Block's
metadata texture getter with side 2 and the unchanged signed operand.
Substituting a texture constant would lose both live receiver behavior and
the original null/class-initialization failures.

Three adjacent Planks produce a Slab while the Pickaxe recipe is being filled.
Its Item icon likewise reads the actual current `UU.al` and delegates the
unchanged metadata to that Block with side 2. The Slab inherits white inventory
color and the no-op shape hook, preserving its live bounds. The SDK also
reached [Creeper environmental damage](../entity/creeper-damage.md) during
nonpausing crafting. Its finite null-attacker path now uses the real inherited
Monster body; fatal death/drop and nonnull-attacker paths remain unfinished.

Closing the screen publishes seven Planks and two Sticks. The dropped Sticks
use the real Item billboard renderer, including the texture/icon/color reads,
captured Tessellator, per-copy Random offsets and actual draw continuation.
This is separate from the held Pickaxe's extruded geometry.

The [dropped-Item packet](../../tests/reference/client/item_billboard/README.md)
completes four real original parent calls and ten quads per ordinary/debugger
process. It retains constructor entropy, Source Random reseeding and copy
offsets, bob, actual GL calls, 25 live Code pins and 109 unchanged inputs.
Ten native entered prefixes match all 340 selected active words; each process
checks its own 140 inactive words. Another 160 original unwritten tail words
remain independently audited. The native CPU prefixes complete zero draws;
their comparison does not claim the full parent or pixels. Forty corruption
controls pass. The complete native SDK qualifies its own graphical caller
separately.

## Native crafting and save qualification

The [curated native receipt](../../tests/reference/client/workbench_drawing/native-validation.json)
records a 22.96-second Windows MSVC Release run with 1,853 checks. Real E,
left/right/Shift clicks and world targeting craft 16 Planks, one Table, four
Sticks and one Pickaxe. The actual statistic and achievement-notice paths
execute. Workbench draws three observed UI frames while World time advances
seven ticks; main-to-hotbar and hotbar-to-main Shift transfers both return.

Keyboard close first restores the default Container. The inherited close and
direct Workbench close publish the same captured cursor and grid Stacks into
the World, while the source grid alias remains retained. Three subsequent
frames draw the actual equipped Pickaxe with 66 faces and 264 vertices.
The completed hand receipt's Stack borrow has already expired before overlays;
the test checks its scalar return and the live held-renderer owner's Stack
identity instead of requiring a stale pointer.

Independent NBT decoding matches all 40 Inventory cells and every Item's raw
source fields at the selected paused save entry. The ordinary save contains
403 Chunks and three Items, including unrelated natural drops. Recipe material
totals remain exactly seven Planks, two Sticks and one Pickaxe across the
Inventory and World pickup boundary. The Table occupies the picked adjacent
cell, its original Dirt target remains, and an unrelated region keeps its
exact bytes. Seven native screenshots and checked window cleanup complete.
Source inputs and imported assets remain unchanged throughout the run.

## Qualification limits

The first native SDK attempt opens Inventory before any RenderManager frame
has returned. The actual delayed Entity pass leaves its Engine null, and the
Player preview reaches the retained `BW` null-Engine exception. Native execution
then stops at the unimplemented caught `Exception.printStackTrace` child.
This failure remains recorded. Unchanged-original cold calls print the same
first null-Engine exception, then escape from the Player-name distance suffix
because the camera is also null. They do not return or restore the temporary
preview pose. The [cold/warm packet](../../tests/reference/client/player_preview_caught/README.md)
retains the same caught Throwable, its real print return, exact Source/JDK
line metadata and the distinct uncaught suffix exception. Thirty-six
corruption controls pass. Fresh warm calls after the genuine RenderManager frame return.
The successful-crafting SDK scenario therefore requires that actual N-to-TH
frame and checks its current Engine, camera, Settings, Font and World before
opening Inventory. No runtime field repair or synthetic cache binding is used.
Cold-prefix printing and continuation remain separate unfinished fidelity work.

The bounded native crafting, placement, close/drop and exact-save scenario
passes. Original pixels, unrestricted container behavior, Linux SDK gameplay,
global focus stability, FPS and the live-open terminal join remain unfinished
or unqualified. Eligibility close after distance or Table removal also needs
its separate composed caller qualification. The broader client remains pre-playable.

Enable `MC_ENABLE_CLIENT_WORKBENCH_TESTS` with the existing imported-resource
and display options, then run:

```powershell
ctest --test-dir build-windows -R '^client\.workbench_loop$' --output-on-failure --no-tests=error
```

Public component and packet tests require no original assets or JVM.
