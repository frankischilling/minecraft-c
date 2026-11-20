# Dispensers

The client implements nine-slot Dispenser storage, its inventory and 45-slot
container, screen drawing and input, loaded-block activation, placement and
orientation. Finite CPU controls and Windows monitor-2 save/reload scenes cover
these paths. Neighbor callbacks, random slot selection, powered firing, removal drops and
dedicated-server behavior remain unfinished. Sustained play and FPS are not
qualified by these scenes.

## Storage and containers

The original client uses `AZ` for the TileEntity. Its inventory name and NBT ID
are both `Trap`; the screen displays `Dispenser`. Construction allocates nine
slots before constructing the actual Random. NBT reads replace the inventory
array, interpret each Slot Byte as unsigned, ignore out-of-range indices and
keep the last entry when indices repeat. Empty serialized Items lists use the
original list writer's Byte element type.

Assignment clamps a Stack count above 64, then dirties the actual TileEntity.
The nonempty split branches also dirty it. A failed World observer does not
undo the inventory changes already reached. Its real diagnostic producer
remains protected until checked native cleanup.

`SH` places nine ordinary Slots above the Player's 36 visible cells. It declares
the constructor and usability check, inherits `DW.a(int)`, and has no dedicated
Shift-transfer override. The earlier MSVC component comparison matches sixteen
declared cases against two fresh original VMs. Two original null-Player Shift
cases remain outside the native comparison. Successful outer Player Shift-clicks
and random slot selection remain unqualified.

The [supplied-input container reproducer](../../tests/reference/inventory/dispenser_container/README.md)
retains a separate finite original-component scope. It needs the caller's own
unchanged JAR and selected SDK/JDK inputs; its offline controls do not execute
the original program or native implementation.

## Placement and orientation

Client `XQ.c` performs the actual `RW` factory and TileEntity installation before
its private orientation body. The remote flag skips orientation after that
installation. Otherwise, all four north/south/west/east block reads finish
before the first opaque-array access. Each reached `UU.o` access is fresh and
the paired operand is short-circuited. The four conditions are independent:
an X-axis choice can overwrite an earlier Z-axis choice. Glass demonstrates
why Material solidity cannot replace the opaque table.

Placed-by reads the actual Living yaw, multiplies by four and divides by 360 as
separate binary32 operations, widens to double, adds 0.5 and enters the defining
Math helper. The resulting directions write metadata 2,5,3,4. Double yaw/90
changes boundary cases and is not an equivalent implementation. This override
has no remote guard. Null Living fails before Math or World; null World fails
at the selected metadata invocation after the yaw calculation.

Both orientation paths use the notifying metadata setter. Equal metadata still
notifies World observers. Retained native calls preserve a failed observer's
actual receipt or lease, the already changed metadata and the installed AZ.
Cleanup consumes the native owners without retrying the failed Source call.
Provider-null classification comes from the actual reached read, rather than
a later World-field lookup. Registry failures retain the classification from
their defining namespace, including distinct Source Exception and Error flags.

## Activation and screen

Client `XQ.a(World,III,Player)` reads the remote flag first. Its remote return
precedes TileEntity lookup and Player use. Otherwise it performs one ordinary
World getter, compares the returned object's canonical Class with the defining
namespace's AZ Class, then invokes the actual Player continuation. A nullable
TileEntity survives the checkcast; a null Player fails at the later invocation.
Distinct canonical Class identities produce the reached ClassCastException.

The client constructs `GQ` and its `SH`, then enters ordinary screen display.
GQ uses `/gui/trap.png`, the inherited container layout and the two original
foreground labels. World, Player and TileEntity are the actual supplied owners.

Borrowed diagnostic names carry their adapter's owner identifier through
inventory, container, Player and screen errors. Cleanup expires matching
borrowed views while preserving recorded Source flags and sites. Static
Registry exception-class literals survive adapter expiry. Native terminal
cleanup drains retained calls before retiring owning containers, including
when fresh Player state reaches another Screen's container.

## Validation scope

Nine selected Windows CPU tests cover storage, containers, activation, dirty
failures and placement. The four new controls pass 320 orientation checks,
178 placed-by checks,81 Registry namespace checks and 77 Inventory/Container
projection checks. They include all 16 opaque masks, glass,44 raw yaw inputs,
remote and equal-metadata behavior, genuine initialization failures and checked
producer cleanup. Eight core tests also pass on GCC and Clang with ASan, UBSan
and leak detection. ABI and offline controller checks retain separate scopes.

The new monitor-2 placement scene passes 1,870 checks over 52 frames, including
three returned GUI frames. One SDK tap uses an authored count-one BlockItem 23
against Stone, places a Dispenser in adjacent air facing north (metadata 2), and
consumes that Stack. Both the initial and reloaded screens contain nine empty
AZ cells and 45 total Slots. Reload creates distinct World and Player owners.
The saved snapshot and later files preserve all 441 declared Chunks' complete
Blocks/Data planes, changing only the target block and facing nibble. The
fixture contains the actual radius-nine tick area and a neighbor margin before
startup; it does not repair generated terrain after loading.

Three fresh images retain the placed block, title and reloaded block. The
process returns naturally and both windows close. All 413 supplied assets and
6,269 selected Source/tool/product inputs remain unchanged. The sound receipt
establishes a returning World/Sound continuation, not heard audio. This finite
scene covers yaw zero; the CPU controls supply the other declared yaw cases.

A fresh loaded-inventory regression passes 1,059 checks over 52 frames, including
four returned GUI frames. It moves five diamonds from AZ slot 0 through the
cursor into Player slot 9, leaves three gold ingots in AZ slot 8, saves and
reopens those typed inventories with supplied metadata 3. That scene qualifies
the selected inventories and cells, rather than the complete surrounding terrain.
Two fresh hidden monitor-2 processes also pass 50 and 44 cross-owner checks for
both Screen creation orders. They are component entries, separate from ordinary
current-screen input and rendered-frame qualification.

The complete MSVC Release build and full Windows sweep pass all 1,404 tests in
514.46 seconds. All 1,394 preceding names remain, with ten additions and no
failed, skipped or unrun tests. All 6,690 project files and 708 native products
remain unchanged during the run. Final result annotations and their current
input pins are checked separately; native code and products retain the swept bytes.

Two fresh original JVMs execute the direct XQ addition and placed-by bodies
over 62 genuine saved Worlds each. All 124 observed rows agree exactly between
interpreter and default modes. The independent physical audit verifies 122 live
AZ objects, 118 persisted AZ records and 378 untouched Chunk records, with complete
Blocks/Data, writer, metadata rotation and absent-data-file checks. This is a
finite original component result. It supplies no paired native cohort, outer CK,
callback-failure, rendering or whole-class parity claim.

The earlier loaded-activation checkpoint passed the complete Windows suite of
1,394 tests, with 1,387 preceding names and seven additions. That historical
sweep and its source-binding failure prefixes retain their original scope.
Current placement validation is recorded separately from that checkpoint.

Run the selected CPU tests from a configured build with:

```text
ctest --test-dir build -C Release --output-on-failure -R "^(world[.]dispenser_(activation|storage|dirty|orientation|placed_by|registry_failure)|inventory[.]dispenser_(container|registry_projection)|client[.]dispenser_dirty)$"
```

Supplied-asset SDK tests require physical monitor 2. The options
`MC_ENABLE_CLIENT_DISPENSER_PLACEMENT_TESTS`,
`MC_ENABLE_CLIENT_DISPENSER_GAMEPLAY_TESTS` and
`MC_ENABLE_CLIENT_DISPENSER_CROSS_OWNER_TESTS` register their respective scenes
when `MC_APPLICATION_STARTUP_IMPORTED_RESOURCES` supplies the asset root. The
ordinary placement and loaded-gameplay registrations also require
`MC_ENABLE_CLIENT_DISPLAY_TESTS`. Authored SDK input does not establish physical
input timing, ordinary item acquisition, original pixels or full Dispenser parity.
