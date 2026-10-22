# Minecart inventory, NBT and storage

The [Minecart module](minecart.md) supplies seven Player-independent inventory
operations for canonical client `yl` and server `pr` instances. The
[public API](../../include/mc/entity/minecart/inventory.h) accepts every live
construction phase and any signed Type. It adds no World, READY, Registry or
Type1 requirement. Callers keep owners exclusive and output/error storage
disjoint from owners and borrowed inputs.

## Slots and ownership

| Operation | Contract |
| --- | --- |
| `mc_minecart_inventory_size` | Report 27. |
| `mc_minecart_inventory_name` | Return the immutable UTF16 name `Minecart`. |
| `mc_minecart_inventory_limit` | Report 64. |
| `mc_minecart_inventory_changed` | Return without a dirty flag, callback or World effect. |
| `mc_minecart_inventory_get` | Return NULL or one retained hold on the loaded Stack. |
| `mc_minecart_inventory_split` | Return NULL, transfer a whole occurrence, or construct a distinct partial result. |
| `mc_minecart_inventory_set` | Store the supplied identity, then clamp its count only when greater than 64. |

Reported size 27 is independent of the actual `h` array. Construction allocates
36 entries; a Type1 NBT read replaces them with 27. Indexed operations use the
actual array bounds, so slots 27 through 35 are accessible before replacement.
A RAW cart can have NULL `h`: constant/no-op methods still work, while a reached
array access reports the null reference. Negative indexes fail at that access.

Every installed occurrence owns a Stack hold, including duplicate identities.
`get` adds a caller hold; whole removal transfers the existing occurrence hold.
The caller releases each returned nonnull Stack. `set` acquires its occurrence
hold before publishing the identity and releasing the old hold, including
self-assignment. Its later clamp mutates that same Stack, so aliases observe it.
A native retain failure leaves the slot and count unchanged.

Split accepts zero and negative amounts and signed counts. An empty slot returns
NULL. When count is no greater than amount, it clears the slot and returns the
same Stack. Otherwise the descendant stores the Java-wrapped decrement at 7,
allocates at 10, then reads the original ID at 15 and damage at 20 before
construction at 23.
A native allocation failure retains the decrement and the occupied slot, with
output unchanged. This is an explicit native failure contract, not JVM OOME
evidence. Private Stack kernels accept RAW, READY and failed owners; existing
public Stack READY wrappers retain their own admission. The
[implementation](../../src/entity/minecart/inventory.c) and
[diagnostics](../../include/mc/entity/minecart/inventory_errors.h) keep native
hold failures separate from entered source methods. Success clears diagnostics.

## Semantic NBT

[Entity NBT](../../include/mc/entity/nbt/nbt.h) runs the existing base read before
the [Minecart hook](../../src/entity/minecart/nbt.c). Type is read and stored
first. Type2 then reads PushX, PushZ and Short Fuel in order. Types other than
1 or 2 leave the existing inventory, push and fuel fields untouched. Missing scalars supply
zero; a wrong type fails only when its getter is reached. Earlier base and own
stores survive a later failure.

Type1 captures Items before allocating and publishing a fresh 27 array. A wrong
Items tag fails before replacement; a missing list still produces the empty new
array. Each record reads Slot as Byte, defaults a missing Slot to 0 and masks it
with 255. Out-of-range slots skip Stack construction. Valid slots capture the
target array before allocation and construction, then install the new Stack.
Later duplicates replace earlier entries. A malformed later record retains the
new array and completed stores. Detached old arrays and their occurrence holds
stay live until every borrowed input read finishes, on both outcomes.

Writing emits the seven base fields and Type. Type1 adds Items by traversing
all actual 27 or 36 slots; Type2 adds PushX, PushZ and narrowed Short Fuel. Each
nonnull record contains narrowed Byte Slot, Short id, Byte Count and Short
Damage from current Stack fields. Other Types add only Type. The independent
result preserves raw floating representations; normal codec output applies its
own canonicalization. Its budget includes the retained own-field tree while
copying the final base-plus-own tree.

The Boolean save resolves the actual name before the dead decision. Normal
false changes only `serialized`; failure preserves both outputs. The
[factory](../../include/mc/entity/nbt/factory.h) uses the real World-only
constructor, then reads NBT outside its constructor reflection failure boundary.
It does not publish the Entity into World or Chunk containers.

## Native preservation

[Storage preservation](../../include/mc/entity/nbt/storage.h) requires an actual
successful semantic read of the same complete input Compound. It does not run
that read for the caller. Inventory edits may intervene. This separate native
operation copies the raw root into an immutable generation.
For Type1, the last accepted input record for each slot 0 through 26 supplies
metadata to that slot's current nonnull occurrence. Receiving Stacks must be
READY; byte inspection and semantic storage writing accept all live Stack phases.
Preparation commits root and occurrence attachments together, leaving old
attachments intact on failure. This does not roll back an earlier semantic read.

Occurrence metadata belongs to the array slot, even when slots share a Stack.
Set, self-set, clearing and whole removal invalidate that occurrence; partial
split retains its metadata and gives the new Stack none. Preserving a Type
other than 1 replaces the root while retaining older inventory metadata. Reference-counted
generations let root and inventory retain different source trees. Detached
arrays protect their older generation through borrowed reads.

[Storage save](../../src/entity/minecart/storage.c) emits only current generated
records. It preserves unknown winner children while replacing Slot/id/Count/
Damage. It never replays overwritten or skipped archive records and never uses
`Stack.source` as fallback. Type is replaced; inactive Items/PushX/PushZ/Fuel are
removed. Other unknown root fields survive, including the distinct UTF16 name
Items followed by NUL. Preserved order and unknown fields are native policy,
not original mutable-Compound identity or byte-order equality.

The native budget charges direct Entity/array bytes, each distinct Stack and its
own source once, sidecar storage, and each distinct generation header/tree once.
Optional provenance allocations stay outside the constructor Class ledger.
Save also charges overlapping semantic intermediate/result trees, pointer
scratch and final validation/copy allocations. Borrowed caller trees, allocator
overhead and bounded C stack are excluded. This is one Entity graph, not a
whole-World heap bound. Pure byte inspection selects a residual budget before
the semantic Boolean call. Inspection errors wait until a true result needs
assembly; normal false needs no overlay even when the live graph exceeds the
ceiling. Semantic name, dead and failure behavior takes precedence.

## Source and validation boundary

The paired reference witness map `source-witnesses.json`, SHA256
`b15c42da5e2d41d2f3229cb71370051055239cc881e379693c7b4e1e2a0b1be2`,
indexes 57 complete method pairs. Under the
[retained archive identities](../reference/minecart-construction.md#primary-methods),
client inventory bodies occupy physical LF 288998-289100 and server 141902-142004;
own read is 288579-288655/141489-141565, and write 288503-288578/141413-141488.
These are bytecode source witnesses, not executed inventory comparisons.

The [inventory](../../tests/entity/minecart/test_inventory.c),
[NBT](../../tests/entity/minecart/test_nbt.c) and
[storage](../../tests/entity/minecart/test_storage.c) fixtures cover genuine
owners, phases, aliases, partial failures, measured allocation boundaries and
GAME save/reload paths. Local B validation passed all 544 CTest executions
across three hosts:
180 MSVC, 182 GCC and 182 Clang, with zero failures, errors or skips. Each host
passed inventory 598/40, NBT 558/40 and storage 335/14 (checks/source cases),
plus 314 independent disk checks. The [native validation record](../reference/minecart-inventory-nbt.md#native-validation)
binds those results to the local source. A separate one-test GCC probe covers
the CMake D storage interpreter selection; it is not a full D or hosted run.

Player usability, interactions, Minecart ticks, movement, collision and physics
remain outside this contract. No original inventory/NBT/storage comparison or
whole-game equality is claimed.
