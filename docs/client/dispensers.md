# Dispensers

The client has a nine-slot Dispenser TileEntity, its inventory adapter and
45-slot container, a typed screen and drawing path, and loaded-block activation.
Storage and activation have finite native CPU coverage. An authored Windows
monitor-2 cycle opens the actual screen, transfers items, closes it, saves and
reselects the World, then opens the saved Dispenser again. Placement, orientation,
random slot selection, powered firing, removal drops and dedicated-server
behavior remain unfinished.

## Storage and containers

The original client uses `AZ` for the TileEntity. Its inventory name and NBT ID
are both `Trap`; the screen's displayed title is `Dispenser`. Construction
allocates nine slots before constructing the actual Random. NBT reads replace
the inventory array, interpret each Slot Byte as unsigned, ignore out-of-range
indices and keep the last entry when indices repeat. Empty serialized Items
lists use the shared original list writer's Byte element type.

Assignment clamps a Stack count above 64 and then dirties the actual TileEntity.
The nonempty split branches also dirty it. These calls retain a failed World
observer's real diagnostic producer. A failed dirty continuation does not undo
the already reached inventory changes.

`SH` places nine ordinary Slots above the Player's 36 visible inventory cells.
Its declared methods are the constructor and usability check. It inherits the
ordinary `DW.a(int)` getter; it has no dedicated Shift-transfer override. The
MSVC component comparison matches sixteen declared cases against two fresh
original VMs, covering slot layout, aliases, inventory calls, selected failures
and the inherited getter. Two original null-Player Shift cases remain outside
the native comparison. Successful outer Player Shift-clicks and random slot
selection remain unqualified.

## Activation and screen

Client `XQ.a(World,III,Player)` first reads the World's remote flag. Its remote
return precedes the TileEntity lookup and Player use. Otherwise it performs one
ordinary World getter, checks the returned object's canonical Class against the
defining namespace's `AZ` Class, and invokes the actual Player continuation.
The nullable TileEntity operand survives the checkcast. A null Player fails at
the later invocation. Distinct canonical Class identities produce the reached
ClassCastException, including separate namespaces with the same Registry.

The client continuation constructs `GQ` and its `SH`, then enters the ordinary
screen display path. `GQ` uses `/gui/trap.png`, the inherited container layout
and the two original foreground labels. Drawing, input and native cleanup share
the existing container-screen owners. The World, Player and TileEntity remain
the actual objects supplied to this call.

Native diagnostic names borrowed from Dispenser failure producers carry an
owner identifier through inventory, container, Player and screen errors. Checked
cleanup expires matching borrowed views while preserving the recorded source
flags and sites. Fresh Player state can select another Screen's container, so
native terminal cleanup first drains all retained calls before retiring owning
containers. Expiry visits every owned Screen and diagnostic envelope by the
ended adapter identifier. Both Screen creation orders have real SDK regressions.

## Validation scope

Five selected CPU tests pass on Windows MSVC Release. Four core tests also pass
on GCC and Clang with ASan, UBSan and leak detection: storage, container,
loaded-World activation and failed dirty-producer ownership. Windows adds a real
Player-child failure projected through the production container-screen helper.
That helper test does not enter SDK mouse handling or render a frame.

The activation control includes a regression that fails before the canonical
Class comparison fix and passes afterward. The storage control separately
exercises the fresh semantic writer used by ordinary Chunk saves: nonempty Items
lists contain Compounds, while empty semantic lists remain END until the wire
writer emits BYTE. The regression fails with the former nonempty END list and
passes after the fix. Authored loaded cells precede activation calls; these
controls do not invoke or qualify placement and orientation.

The monitor-2 cycle passes 1,081 checks over 53 frames, including four returned
Dispenser GUI frames. It moves five diamonds from Dispenser slot 0 through the
cursor into Player slot 9 and leaves three gold ingots in Dispenser slot 8. Both
the saved snapshot and the later World files contain those typed inventories,
block 23 and the supplied metadata 3. Reload creates distinct World and Player
owners. Four native images are retained; the title and reloaded screen were
inspected. Selected source inputs and all 413 supplied asset files remain
unchanged, the native process returns naturally and both SDK windows close.

Two fresh hidden monitor-2 SDK processes pass 50 and 44 cross-owner checks. The
old entered Screen uses fresh Player state to reach another actual container.
The regressions expose an expired borrowed exception name and a retained-call
dependency before their respective fixes; both creation orders then complete
checked native cleanup. These are component entries, separate from the ordinary
current-screen input cycle above.

The complete MSVC Release build and a fresh full Windows sweep pass all 1,394
configured tests in 516.09 seconds. All 1,387 preceding names remain, with
exactly seven additions and no failed, skipped or unrun tests. Source inputs
and all 703 native products remain unchanged through that run. Final result
annotations, input pins and the optional cross-owner test timeout are checked
separately; runtime and native test code retain the swept bytes.

An earlier complete run remains failed at 1,305 passes and 89 source-binding
preflight failures. All 89 names later execute and pass within an audited
115-test selection. Those results remain separate from the fresh complete run.

Run the selected CPU tests from a configured build with:

```text
ctest --test-dir build -C Release --output-on-failure -R "^(world[.]dispenser_(activation|storage|dirty)|inventory[.]dispenser_container|client[.]dispenser_dirty)$"
```

The supplied-asset SDK tests are optional and require physical monitor 2.
`MC_ENABLE_CLIENT_DISPENSER_GAMEPLAY_TESTS` and
`MC_ENABLE_CLIENT_DISPENSER_CROSS_OWNER_TESTS` register them when
`MC_APPLICATION_STARTUP_IMPORTED_RESOURCES` supplies the imported asset root.
The ordinary gameplay registration also requires `MC_ENABLE_CLIENT_DISPLAY_TESTS`.
Their authored SDK input exercises transport; it does not establish physical
input timing, original pixels, sustained play, FPS or complete Dispenser parity.
