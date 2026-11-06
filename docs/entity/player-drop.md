# Player item drops

The gameplay keyboard loop connects the configured drop key to the original
Player action. Q removes one Item from the selected Inventory slot, constructs
a genuine dropped Item, applies the original throw velocity and pickup delay,
publishes it to the World, and increments `stat.drop`. An empty selected slot
returns without constructing an Item or incrementing that statistic.

## Original order

The authority is the verified original client archive, SHA256
`af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c`.
The [static identity packet](../../tests/reference/entity/player_drop/README.md)
is separate from native execution and original runtime comparisons.

`Minecraft.k:812` calls `dc.D()`, inherited from `gs`. `GS.D` captures the
Inventory receiver, reads the current selected index, splits one Item at13,
then calls `GS.a(Stack,false)` at17. The keyboard event continues after that
method returns.

The Stack overload returns at4 when the Stack is NULL. Otherwise it allocates
the Item before reading the World and position arguments. The constructor
receives `(x, (y - 0.30000001192092896) + (double)0.12f, z)` and the actual
split Stack. The original LocalPlayer inherits the literal eye-height body
used by this call.

The returned Item receives pickup delay40. Directional throws preserve the
original float operations and game sine/cosine table, followed by four Player
Random float draws and the double sine/cosine jitter. The scatter overload
uses two Player Random float draws and its separate velocity body. Item
construction retains its own existing Random consumption.

`GS.a(Item)` invokes the real World insertion. An ordinary false insertion
return still reaches the drop statistic; a failed insertion retains its
completed mutation prefix and stops before that statistic. No sound, swing,
equip reset or immediate World tick is added to the action.

## Ownership and verification

Each call retains the actual Player, split or provided Stack, reached World
occurrences, Item and failed publication receipt until checked retirement.
Inventory removal, Random draws and publication effects survive a later
failure. Cleanup neither rolls them back nor replays the action.

The authored component uses genuine Player, Inventory, statistics, Item,
Chunk and World owners. It covers empty slots, count1 removal, count2 splitting,
directional and scatter throws, Random progress, real publication callbacks,
statistic failure, and attempted retirement from an active callback.
Additional seeded directional/scatter cases compare the exact native velocity
words at positive, negative, steep and multiple-turn angles. They verify
Player and shared Random progress independently.

The Windows SDK fixture extends the ordinary mining/pickup sequence with a Q
drop, an empty-hand Q, returned Item/HUD/hand frames, and another normal
save/title/reselection. Independent decoding compares Player and every saved
Item with semantic snapshots from the actual selected paused entry frame.
Loaded Items are compared with the selected live entry. Unloaded Items must
preserve their exact Chunk and complete typed body from that frame's disk
snapshot. Eight public decoder controls reject lost, duplicated, moved or
changed Items, including one-bit motion changes.
The fixture supplies no runtime pose, Item, Random, Timer or source-return repair.

Native double trigonometry uses the existing platform math adapter. Original
JDK bit equality for this jitter, matched original pixels/audio, default-Far
performance and extended play remain unqualified. The client remains
pre-playable.

The fresh Windows MSVC Release SDK scenario passes in 23.28 seconds with 3,526
native checks, four World loads and three save/title transitions. It verifies
one nonempty Q drop, one empty-hand Q, four empty HUD/hand frames, three real
Item draw frames, saved/reloaded Dirt and checked window/owner cleanup.
Every saved Item matches the relevant live or pre-save disk state. Screenshots
show the dropped Dirt, empty hotbar and empty hand. Fifteen focused Windows
suites and fourteen Linux Clang ASan/UBSan suites pass; the drop component
completes 251 checks per host. Earlier Inventory-key, focus and natural-jockey
failures retain their own scope; this finite pass does not establish extended
play or an original executed-drop comparison.
