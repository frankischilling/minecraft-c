# Player drop reference identities

`source-pins.json` records authored class and method identities from the supplied
Beta 1.7.3 client archive. It pins the complete method inventories and selected
Code identities for Minecraft, LocalPlayer, Player, Inventory, ItemStack, Item,
World and the Math helper. It resolves the actual inherited LocalPlayer drop
methods and their selected invocation references. It contains no original Code
bytes, classes, assets or disassembly.

```text
python tests/reference/entity/player_drop/verify_source.py --jar b1.7.3.jar
```

The verifier checks the exact archive hash, inventories, inheritance, Code
hashes and invocation references without executing game classes. A missing
archive returns `UNAVAILABLE` with exit77; invalid or changed inputs return
`FAIL` with exit1. Static matching is distinct from an executed original drop.

Public native controls use genuine Player, Inventory, Item, statistics and World
owners. They check one-Item splitting, count1 removal, empty slots, directional
and scatter throws, exact native velocity words across declared angle/seed
inputs, Random progress, publication callbacks, failed prefixes and active-call
retirement refusal. Native double trigonometry retains its platform/JDK limit.

The separate SDK fixture continues mining and pickup with actual Q input,
empty-hand Q, Item/HUD/hand frames and another save/reselection. Its independent
NBT decoder compares loaded Items with the selected live paused-entry state.
Unloaded Items must preserve their exact Chunk and typed pre-save body. Eight
public decoder controls reject loss, duplication, relocation and changed float
bits. See the [runtime contract](../../../../docs/entity/player-drop.md).
