# Chest access

The client connects an existing Chest's right-click body to its real inventory,
container, screen, transfers, close and saved-World reselection. Single Chests
expose 27 cells; double Chests expose an ordered 54-cell inventory. The Player's
36 visible cells follow those cells in the container.

Left and right clicks use the inherited container input path. Shift transfers
from Chest to Player search the destination in reverse order; Player-to-Chest
transfers search forward. E and Escape follow the inherited source close path.
The screen does not pause the World. Player eligibility checks the actual
World/TileEntity identity before distance, with the original eight-block limit.

## Original bodies and native joins

Chest activation follows `e.a(FD,III,GS)`: read and cast the clicked TileEntity,
check the top and neighboring tops, then repeat the neighbor ID reads before
constructing each reached `OG`. West and north neighbors precede the accumulated
inventory; east and south follow it. The remote flag is read after those
operations. An obstructed Chest consumes activation without opening a screen.
Native retirement does not retry any query or complete a failed constructor.

`DC.a(LW)` constructs `HP`, whose superclass receives the actual new `CD`.
`HP` reads inventory size again after that superclass call; it does not reuse
the container's row count. Its background uses the two original
`/gui/container.png` rectangles, with separate Chest and Player inventory names.
It inherits `ID` input and close behavior and adds no Player preview.

The [component reference](../../tests/reference/inventory/chest_container/README.md)
records 111 calls against unchanged original `CD`, `OG`, `HP`, Slot and Stack
bodies in two fresh VM modes. Controlled `LW` receivers expose call order,
nullable arguments, repeated size reads, aliases and failed prefixes. They are
not concrete Chest/Player/World or original drawing observations. Native backend
comparisons consume nine original slot layouts and seventeen Shift profiles.

Chunk compilation now calls the existing TileEntity renderer dispatcher at its
actual static read and invocation. Chest's absent special renderer is determined
by the real Class lookup and its null cache entries. The checked block texture
path executes the original single/double adjacency queries and opaque-array
branches, retaining the first failed query and its source frame. It does not
continue through a callback error to obtain a replacement texture.

## Ownership and shutdown

A Chest inventory view holds its actual TileEntity. An `OG` holds each supplied
child independently; `CD` and `HP` hold their own constructor occurrences.
Finishing activation releases its local rights while the displayed screen keeps
the same inventory alive. Source close ends the Player's active-container alias;
native retirement never supplies that close or repairs the alias.

Shutdown with a screen still open uses the selected terminal root. The Player
sidecar adopts the actual container only after local ownership admission.
World collection then counts each container root, each distinct `OG`'s two child
rights, and each distinct Chest view's one TileEntity right. Shared edges remain
distinct occurrences. Extra external inventory holds refuse collection until
released; a foreign-World TileEntity cannot be collected as a local Chest.

Terminal sealing blocks new source calls and owning holds. Completed diagnostic
receipts can drain after their enclosing input/draw/GUI borrows have ended,
before renderer detachment. Existing rights can still release. These steps do
not close an inventory, drop a cursor, save a World or change source Stack fields.

## Verification

Portable controls cover World activation, inventory/container operations,
screen construction, checked texture queries, Player eligibility and terminal
ownership. The original packet and Python observation controls are independent
of the imported assets. Native scalar texture projections compare two adapters;
they do not establish a new original JVM or pixel comparison.

The visible Windows scenarios use authored saved Worlds and the actual
Frame/Canvas on physical monitor 2. Their runner checks native receipts,
independent Player/Chest NBT, complete region allocations, fresh PNGs, window
cleanup and input continuity. The dense scenario fills all 90 displayed cells
and exercises cursor and tooltip drawing in their respective source branches.
The shutdown scenario supplies an authored SDL close request to the live Frame
and compares
source aliases, Stack identities/fields, time, ticks and disk snapshots across
native preparation.

The final Windows MSVC Release cohort passes all five profiles: single (1,135
native checks), double (1,061), dense (981), blocked (814) and shutdown (1,142).
Each process checks its Frame and Canvas on physical monitor 2 and finishes with
both windows absent. Twelve selected Windows CTests pass, including the eleven
original-packet controls and fifteen GUI-observer controls. Five core inventory,
activation and terminal ownership tests also pass on GCC Debug with ASan, UBSan
and leak detection. The texture control passes 3,034 checks, including Java
integer overflow and callback-mutated field reads; its two native adapters do
not constitute an original-program comparison.

A genuine HP foreground call with its Font field NULL reproduces missing Java
exception flags in the shared drawing helper. The control fails before the fix
and passes afterward, preserving `hp.k()V:19` and `NullPointerException`.
Inventory, Workbench and Chest CPU screens and the Chunk compiler also pass.
Separate Inventory and Workbench SDK/save regressions pass with monitor-2 bounds
selected, including cursor-close drops and the Workbench's inherited/direct
close sequence.

```powershell
cmake --build BUILD --config Release --target mc_test_chest_loop
python -B tests/tools/client/test_chest_loop.py --executable BUILD/Release/mc_test_chest_loop.exe --assets ASSETS --artifacts .local/chest-captures --profile single
```

Other profiles are `double`, `dense`, `blocked` and `shutdown`. Imported assets
must include the SDK icons. The runner refuses to launch without a physical
second monitor. Run separate invocations serially. To register these local
CTest cases, enable `MC_ENABLE_CLIENT_CHEST_TESTS`,
`MC_ENABLE_CLIENT_DISPLAY_TESTS` and provide
`MC_APPLICATION_STARTUP_IMPORTED_RESOURCES`. OpenAL uses its null driver in
these controls, so they do not qualify audible output. The CTest cases share
the desktop focus resource lock.

Chest placement and its neighbor-placement rules, unrestricted generated Worlds,
original-created save round trips, Linux visible input, sustained play, complete
original pixels and FPS remain separate requirements. The later special-renderer
TileEntity list/HashSet continuation also remains unfinished; passing Chest's
no-special-renderer branch does not complete that route.
