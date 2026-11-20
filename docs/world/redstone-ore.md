# Redstone Ore activation and decay

Ordinary client Redstone Ore executes its left-click, walking, right-click,
display and tick bodies. The shared activation and particle helpers are in
[step.c](../../src/world/runtime/step.c); the decay tick is in
[redstone_ore.c](../../src/world/runtime/mutation/redstone_ore/redstone_ore.c).
The [67-profile original packet](../../tests/reference/world/redstone_ore/README.md)
defines the observed component scope.

## Activation and caller order

Left click and walking invoke the same private activation body, then their
inherited empty base body. Right click invokes activation before the inherited
base method returns false. The direct Actor arguments are unused in these
methods and may be null.

Activation completes the particle helper first. It then captures the receiver's
own `bn`, loads live `Block.aO`, and compares their IDs. A match loads live
`Block.aP` and invokes the notifying World ID setter. The ordinary setter
Boolean is discarded. The comparison uses the method receiver, so a prepared
receiver and the cell's current Block can select different behavior. The
private lit flag does not control activation.

The real [OS mining caller](../../src/client/controller/mining.c) reaches the
left-click hook at bytecode 74 when its progress field is zero. After that hook,
its strength check indexes a fresh live `Block.m` at bytecode 87 using the cached
old ID. Ore activation may already have changed the cell. The packet checks
this initial mining continuation; it does not qualify general mining destruction.

The real [OB right-use caller](../../src/client/controller/use.c) receives Ore's
false activation result and continues into the held Item. The declared case
uses a genuine Cobblestone Item4: its generic BlockItem body places above the
Ore, emits the step sound, consumes the stack and updates the selected use
statistic. Callback failures retain the effects completed before the failure.

## Particles and decay

The private particle helper captures `World.r` once. It visits +Y, -Y, +Z, -Z,
+X and -X. Each face consumes three floats before its opacity query, with
integer-to-float conversion and float addition before widening the coordinates.
An exposed face moves the corresponding coordinate by 0.0625 outside the cell.
The source's final bounds test compares particle Y against zero rather than
the cell's Y. Both this quirk and the callback's exact Random prefix remain.
The display wrapper checks its private lit flag before invoking this helper.

The tick captures its own ID, loads live `Block.aP`, and compares IDs. A match
loads live `Block.aO` and invokes the notifying World ID setter, discarding its
Boolean. It never reads the supplied Random or `World.r`, and it does not test
the private lit flag. A null Random argument is therefore valid. A nonmatching
receiver can also return with a null World, after the live lit static read.

Activation does not enqueue a 30-tick deadline. Decay occurs when an existing
caller invokes the tick, including scheduled dispatch and Player-centered
random World work. The tick has no random deactivation test. The packet reaches
both callers through genuine `World.l` calls, including the complete 361-Chunk
random-work footprint.

## Failures and ownership

The order of nullable reads determines the failed prefix. Activation particles
can fail before either static is read. Activation's null `aO` and `aP` receivers
fail at bytecodes 16 and 30. Tick reads `aP` even for an inactive receiver; its
null receiver fails at 7. A matched tick then dereferences `aO` at 21 before
invoking World at 24. An inactive tick or an unlit display wrapper can return
with a null World.

[Step diagnostics](../../include/mc/world/runtime/step.h) store the actual
selected source failure beside separate wrapper, activation, particle and
opacity sites. The particle helper publishes that witness even when its
optional output is absent. Registry, query, presentation and setter children
retain their own typed diagnostics; a native failure status alone does not
establish a Java exception.

A null WorldAccess List fails at the actual `List.size` call at bytecode 9.
The value-only null-list witness survives particle and notifying-setter paths.
An active tick can commit its Chunk change before failing in a height-range
notification. A prepared height of 65 reaches the cell notification instead.
Neither case rolls back the completed writes. Null cell/range interface
elements have separate classification work remaining.

[Placement diagnostics](../../include/mc/world/runtime/placement.h) own the
shared Step child and retain its reached World, Registry and callback producers
until checked retirement. The OS and OB call owners keep that placement graph
alive. Direct walking and display diagnostics continue to borrow their producer
graphs. General mining destruction still has separate borrowed-error ownership
work.

## Evidence and remaining scope

Two fresh original VMs produce identical complete streams for all 67 profiles,
with 63 original class pins and 13 unchanged helper products. Windows and
sanitized Linux each match 19,187 selected scalars and pass 15 retained-receipt
controls. The native reader's ten test groups pass in normal Python and with `-O`. The
[testing record](../testing.md#redstone-ore-activation-and-decay) describes the
compared state and receipt checks.

The original App uses its genuine headless `kq` constructor under the existing
native-link guard. Native pairing explicitly prepares the App's method inputs;
it does not qualify App construction or GUI initialization. Dedicated-server
decay remains unchanged. Natural encounters, unrestricted mining and play,
original graphics/audio and FPS remain outside this packet. The complete
Windows build and 1,261 tests pass, alongside 15 focused Linux sanitizer tests
and strict GCC compilation of 12 changed units. Both native hosts match the
same selected original scalars and receipt controls. These checks preserve
the component scope rather than establishing unrestricted play.
