# World collision boxes

The [collision interface](../../../include/mc/world/runtime/collision/boxes.h)
collects actual Block collision contributions into the World's mutable `K`
List, then performs the original nearby Entity scan. The [paired source
map](../../reference/collision-boxes.md) records the reached methods and order.
Entity movement and FallingSand physics remain unfinished.

## Collection and identity

A query clears current `K` before reading its bounds. It captures the six
integer limits in source order, visits X outside Z outside Y, and starts Y
at the Java-wrapped lower floor minus one. The upper bounds are exclusive.
Each X/Z column first checks provider presence at Y=64 without loading it.
For each reached Y, the query captures the actual Block array before acquiring
the block ID, indexes that captured array, then invokes a nonnull Block's
actual contribution with current `K`.

The Block contribution captures its returned pooled box, tests the supplied
query against that same box, and appends it to the passed List. A genuine
null shape skips both query and destination reads. There is no generic
state-shape update before this virtual getter. Block singleton bounds remain
live mutable fields shared by Worlds using the same registry.

Stairs and piston heads contribute their two ordered parts independently.
They reset singleton bounds to the full unit box only on normal completion.
A later factory or append failure preserves earlier contributions and the
last reached singleton shape. Piston bases force full bounds before their
base contribution. Farmland's collision box is full height despite its lower
constructor shape. Metadata, float intermediates, wrapped integer additions
and strict face comparisons follow the respective concrete getter.

After Block collection, the query acquires a real pooled expansion by 0.25
and invokes [nearby collection](entities.md). It keeps that returned mutable
List identity. Every index performs a candidate get/virtual call, followed by
a separate get and moving-Entity pair call. The currently constructible Item
and FallingSand kinds inherit the two genuine null base virtuals. Boat and
Minecart construction and their positive permanent-box paths remain unfinished;
an unknown concrete descendant fails explicitly.

## Native ownership

The constructor-created `K` descriptor is captured independently of current
source `K`. Every occurrence has a separate typed owner record: either one
pooled-box hold or one hold on the Entity owning a permanent box. Coordinates
are never copied into a replacement result box. Duplicate references retain
separate holds. Ordinary clear releases occurrences while preserving descriptor
and backing capacity. Growth charges overlapping old and new source/typed
arrays, and a failed acquisition of the new occurrence discards only
unpublished replacement storage.

Returned views hold the World and actual descriptor. They preserve lifetime,
not contents: a later query can clear/refill the same List while earlier views
remain held. Explicit pool reset can overwrite the same held pooled boxes.
Client pool clear detaches held objects until their last occurrence is
released. A borrowed box from `get` must stop being used before its occurrence
or owner can disappear. Do not copy a held view; release it exactly once.

Native retirement validates all typed containers before releasing any of
them. Its deeper collision audit checks actual pooled provenance and duplicate
hold multiplicity. Ordinary clear uses linear structural admission instead;
pooled hold lookup/release still scans the pool's live objects. Retirement and
free remain busy while any returned view is held. Final free uses the captured
constructor descriptor and never adopts or frees an unknown replacement List.

## Remaining dependencies

Moving-piston lookup runs the real Tile getter. A completed absence or real
chest/spawner fails the original piston-type test and yields a genuine null
shape. Its block-added override is the original immediate return, allowing
lazy lookup to reread an empty map. Positive piston Tile construction,
interpolation, Direction arrays and stored-Block geometry are unfinished.
Missing positive geometry is an explicit reached dependency failure.

General provider replacement, complete original World traversal replay,
application pool reset timing, other Entity descendants and movement remain
unfinished. Native constructed contracts and original Block getter projections
have distinct scopes; neither proves complete gameplay parity.
