# ItemStack state

`mc_item` owns item fields and independent storage provenance through
[`mc_item_stack`](../../include/mc/item/stack/stack.h). Registry lookup, item use,
containers and world callbacks remain separate behavior.

An owner starts raw, then construction assigns ID, count and damage in order.
NBT construction reads Short `id`, Byte `Count` and Short `Damage`; missing fields
use zero. A wrong field type preserves assignments already completed. Failed
construction cannot be retried, while a failed read into an existing ready owner
keeps that owner ready. Counts and IDs receive no registry validation or clamp.

Native holds preserve the same identity across container aliases. Clone and
split create new owners with animation zero and no source provenance. Split
wraps the source count subtraction before allocating its result, so allocation
failure retains that count change.

Source preservation is an explicit extra operation. It copies an immutable NBT
compound into an independent owned tree, leaving the old source intact on
failure. A successful replacement can borrow the old source during copying.
Unknown fields follow the same item identity; clone and split do not inherit
them. Container fields such as `Slot` are supplied by the container.

The builder replaces known fields at their retained positions, appends missing
ones, and narrows ID/count/damage to Short/Byte/Short. It returns an independent
NBT document. Normal writer NaN and empty-list canonicalization applies. Its
allocation ceiling includes the live item/source graph and each simultaneous
scratch, encoded-buffer and codec allocation. It does not measure whole-world
memory or reproduce writes into an original mutable compound.

The separate [combat body](combat.md) now implements source durability on the
same live Stack. It performs independent Item lookups for damageability and
maximum damage, with the wrapped damage store between them. Breaking requires
damage strictly greater than the maximum. The broken statistic precedes the
count decrement, negative clamp and damage reset. A later Player Inventory
clear does not alter the retained Stack's count or add an Inventory dirty store.
The source's post-break Player hook is empty and produces no modern break effects.

`item.stack` exercises 93 native contracts, including failed reads, self-borrowed
source replacement, aliases, split wrapping, container overlays and document
independence. The GCC 13.3 and Clang 18.1 Linux runs pass with address, leak and
undefined-behavior checks enabled. Native Windows MSVC 19.51 passes the same
checks without sanitizers. Those state-record checks retain their original scope;
the newer held attack packet separately compares the reached combat mutations.
The frozen Windows Iron-Sword-break SDK scenario also exercises source clearing
and independently matches every actual save against all 40 live Inventory slots.
Broader gameplay/container integration remains unverified. See the
[source evidence and verification scope](../reference/state-records.md).
