# Live GAME Chunk saving

[`saving/saving.h`](../../include/mc/world/runtime/saving/saving.h) writes one
actual committed Chunk from its owning GAME World. It uses the same Store
entry, header, arrays and source document; no replacement Chunk or opaque list
stands in for live state. Nonempty live
Entity serialization remains an explicit unfinished boundary.

The runtime save and maintenance callbacks use this path. Legacy composition
and passive opaque builders keep their separate behavior. Whole World metadata,
named saved data and complete population/persistence remain unfinished.

## Source order and effects

The paired Region save invokes the World session method before calling the
shared Chunk serializer. That serializer invokes the session method again,
reads the current World time and Chunk fields, then resets q at105. It walks
the actual Entity slice array and lists before reaching Tile Map values224.
The native coordinator performs both session checks, reads the reached nibble
descriptors and follows that q/Entity/Tile order.

Empty live Entity slices produce an explicit empty Entities list. The first
nonempty occurrence sets q=true at167 before stopping at its missing virtual
serializer183. q is never inferred from a retained NBT list or used to prove
that the live slices are empty. A failed NULL Entity insertion can leave q=true
with empty slices; a subsequent save genuinely resets it.

Null e/f/g descriptors fail before q changes. Null source m or l fails after
the reset at its reached receiver. Null byte-array projections remain available
to the later native tree/codec validation instead of being replaced with the
canonical owned arrays. Nonnull spans must remain readable for their fixed
native extents throughout the exclusive call.

The Tile builder serializes actual Map occurrences and physical Chest slots,
including shared values and slot35 in a freshly constructed 36-slot Chest.
Loaded Chests retain their source 27-slot read behavior. Spawner name precedes
Delay; a null name fails at its concrete save12. Earlier q changes survive a
later Tile, budget, codec or transport error. See [live Tile owners](tiles.md).

Successful transport publishes actual region growth and adds its Java signed
int value to the selected World Info's SizeOnDisk with wrapped long addition.
It does not update another World's Info or substitute convenience Store metadata
for a GAME binding. Lifecycle still owns last-save-time and dirty transitions.
No failed save restores those earlier effects or reports successful growth.

## Ownership, composition and diagnostics

One embedded Store save scope owns one real exact-identity Chunk lease. Nested
begin on that entry is BUSY. Scope cleanup releases only that lease and performs
no source validation, Map clearing, source disabling or terminal retirement.
Failed imports and terminally retired containers remain ineligible for saving.
An inactive completed Chunk can still retire after an ordinary unload while
its World continues.

The lower layer composes explicit live Entities and TileEntities over the
same stored baseline, preserving supported unknown root/Level children. It
never calls simulation code. The actual Tile builder preserves the documented
Tile/Stack provenance and consumed-record merge; an owned count-zero Map must
still use that builder. Deleted recognized records cannot be resurrected by
copying the old list. Generic opaque editing restrictions remain intact.

Each detailed lifecycle call installs a fresh stack diagnostic frame. Nested
acquisition and lifecycle calls suspend and restore their predecessor's frame.
A save error carries the complete Tile/Stack/NBT child, separate Region/shared
serializer positions, and complete lower composition/encoding diagnostics.
Native owner/buffer limits do not invent a codec failure. Success clears the
whole returned result; no error is reconstructed from ambient last_error.

## Limits and validation

The Tile builder accounts for its live Chunk/Tile/Stack graph, planning scratch
and independent Tile tree. The Store codec scope then charges that retained
tree, the independent complete output tree and overlapping pointer/codec/raw
buffers. Retained baseline documents, World/class owners and region/compression
owners keep their separately documented budgets. This is not a process-wide
or whole-graph heap guarantee.

The independent result and buffered checked replacement do not reproduce the
original mutable compound allocation failures, early stream/file setup or
caught-Exception/printStackTrace behavior. The runtime retains its existing real
wall-clock sampling position; the direct API accepts an explicit transport
timestamp. These limits are distinct from source field/List order and successful
supported-domain values. Original/native whole-save replay, arbitrary array
lengths, other Tile descendants, nonempty Entity serialization and complete
populated save/unload/reload qualification remain open.

`world.saving` passes 1,003 constructed native checks per host within all 144
MSVC and 146 GCC/Clang configured cases, with zero skips and local JDK comparisons.
Linux uses ASan/UBSan and leak checks. Each host retains 678 unchanged selected
inputs, 254 external zlib pins, 163 products and 32 captures. Clang-format18
checks 388 C files/headers. Four real client/base-server Worlds cover fresh 36-slot and imported
27-slot Chests, aliases, Stack narrowing, Spawner fields, complete saved arrays,
unknown root/Level/Tile/Stack fields, session failure, nullable source receivers,
late composition depth failure, error freshness and count-zero Map/journal saves.
An independent bounded Python decoder checks all four final region records.

Both installed GAME constructors currently bind BASE acquisition policy. The
same-World unload/reload checks therefore use explicit dedicated-policy native
coordinator descriptors with the actual World callbacks, collections and
references; they do not mutate the World's policy or establish a natural
dedicated-World startup route. They perform real save, detach, unpublication,
inactive Store retirement and reload without terminalizing the continuing World.
The save fixture uses identity block normalization. The separate Tile import
fixture's deliberate Chest/Spawner-to-air map remains an import-order control.

The default NBT depth limit is 64. A retained 61-node unknown child fits Tile
import and its independent Tile result, then fails when nested under the outer
Level. This exercises a genuine input-based composition error after q resets;
it is not an allocator failure injection or original Java OOM comparison.

Run `ctest --test-dir <build> -R '^world.saving$' --output-on-failure`.
