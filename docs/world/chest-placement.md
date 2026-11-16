# Chest placement

Chest placement follows the Beta 1.7.3 `E.a(FD,III)Z` override and its private
`E.h(FD,III)Z` helper. The public override counts west, east, north and south
Chest neighbors before testing whether the count exceeds one. Each comparison
reads the receiver's current `bn` after the World getter returns.

If the count permits placement, the override calls `h` independently for each
neighbor in the same order. Each helper freshly queries its center; a Chest
center then checks its four neighbors until another Chest is found. These
queries cannot reuse the first pass's IDs. Coordinates use Java integer wrap.
The predicate does not add top obstruction, facing, Player or remote checks.

The direct override does not call the base Block placement body. An occupied
target can therefore pass a direct call; the enclosing World check separately
handles its collision and occupancy rules. A legal adjacent Chest can form a
double Chest. Extending an existing double or bridging two Chests is rejected.

## Original observations

The [reference packet](../../tests/reference/world/chest_placement/README.md)
records 55 public calls and nine separate private-helper calls on genuine saved
Worlds and four original Chunks. Two ordinary and two read-only JDI fresh VMs
agree on the logical outcomes. The two debugger journals agree on 482 actual
World getter entries and returns across all 64 cases, with three live method
Code hashes matching the original archive.

Native replay compares every public outcome and its actual query sequence:
coordinates, returned ID, E method and caller PC, helper-parent PC, query count
and helper-entry count. The nine standalone private calls remain separate
original evidence. A NULL World throws before entering the getter; it has zero
entered getter records.

These observations cover the predicate bodies. They do not observe an original
Player placing a Chest, original Item consumption, screen output or a complete
original save round trip. Authored array inputs, selected unchanged fields and
queues, independent constructor entropy and the ordinary session-lock write
remain explicit in the packet.

## Item use and ownership

Ordinary `CK` ItemBlock use supplies the actual held Item and target to the
World predicate. Successful placement uses the notifying Block/metadata setter,
then the actual placed-face and placed-by hooks, sound and late Stack decrement.
Chest inherits the literal base placed-face/by returns. Its inherited `RW`
addition creates a real `JS` and publishes it through the ordinary World
List/Chunk Map path. No later Tile lookup supplies a missing creation.

The separate owned mutation route retains that same constructor/set child,
defining Registry and factory namespace, actual evaluated World and canonical
execution result. It preserves the List-before-provider-before-install prefix.
Native cleanup does not repeat the constructor or complete a failed source
setter. Other unconnected TileEntity factories and removal routes retain their
explicit limits.

A nullable hook operand is admitted only with the exact current NULL native
World slot. `RW.c` still evaluates its factory at PC 15 before the null World
invocation at PC 18. Constructor failure takes priority. Separate controls cover
that direct prefix and a real resource callback during cold `Block.m` active use
at Chunk PC 279, which clears the leased Chunk's World before its fresh read at
286. The written Block/metadata and reached constructor effects survive the
later source exception. World-method admission is not relaxed by this hook rule.

The common placement diagnostic still has eight query slots. Only an entered
Chest predicate allocates its bounded child, which owns up to 24 distinct query
records. The World wrapper's initial target query stays in the common frame.
Every child retains its query arguments, return and source site; copied loader
causes and callback producer rights survive until checked retirement. On x86-64
GCC 15.2, the common error grows from 896 to 904 bytes. This is a layout result,
with no frame-time or FPS claim.

## Verification scopes

The native predicate replay and ordinary Item-use controls pass on Windows MSVC
Release and GCC Debug with ASan, UBSan and leak detection. Clang Debug sanitizer
replay also matches every public outcome and query sequence. Constructed controls
cover the distinct outer World check, legal orientations, rejected third/bridge
placements, preserved failed-query prefixes, unchanged predicate inputs and
native cleanup. Owned mutation controls separately cover real JS installation,
provider/deferred-list failures and refusal to retire an active callback.

The first authored Windows SDK placement scenario passes 1,464 checks on
physical monitor 2. It starts with held Chests and Diamonds and no Chest Blocks
or TileEntities. Actual input places a single Chest, moves five Diamonds into it,
strafes to place a legal second Chest, and rejects a third without consumption.
It opens the double Chest, saves through the ordinary menu, selects the saved
World again and independently decodes two Chests, the payload and the remaining
held Chest. Frame/Canvas witnesses, six fresh images and terminal cleanup are
checked. This initial capture applies to its recorded inputs; later verification
is recorded separately. The later frozen-source cohort passes the placement
profile with 1,846 checks and all five preceding Chest profiles. Its strengthened
reader requires the six ordered taps, three strafe groups and complete source
save/reload receipts, tied to actual entry/snapshot inventories. The 24 reader
controls pass normally and optimized on Windows and WSL. Separate Inventory and
Workbench SDK/save regressions also pass on the same native build.

The [Chest access record](../client/chests.md) covers the existing single,
double, dense, blocked and open-screen shutdown scenarios. Unrestricted Worlds,
all neighbor mutations, original-created save interchange, Linux visible input,
matched original pixels/audio and sustained play/FPS remain unqualified.

## Rendering fixture correction

Clang's preceding hosted Chunk compiler test found an invalid Boolean read in
face rendering. Its stack-allocated CV fixture omitted Java NEW defaults;
`mc_client_render_blocks_construct_fields` correctly performs only the explicit
constructor stores. Production CV owners already use zero-initialized storage.
The fixture now initializes all three fresh field records before invoking that
helper. An explicit compile/link comparison with Clang stack-pattern
initialization fails before this correction and passes afterward; normal Clang
sanitizer execution also passes. No production constructor reset or sanitizer
relaxation was added.

Pure diagnostic receipt copying/expiry now belongs to the CPU SourceScreens
module. Inventory, Workbench and Chest constructor checks can build without
startup/audio. Actual event controls remain enabled when their module exists
and are reported unavailable when it is excluded. The no-audio configuration
does not qualify event dispatch or audible output.
