# Nearby Entity collection

The [nearby interface](../../../include/mc/world/runtime/collision/entities.h)
implements the complete base World nearby query and the normal/Blank Chunk
gather bodies. The [paired source map](../../reference/nearby-entities.md)
records their order. This is the Entity collection used by later collision
work; it does not perform Block shape collection or move an Entity.

World owns one actual constructor-created `R` List. A query clears current
`R` before reading the query bounds, calculates its four chunk limits once,
then visits X outside Z in inclusive order. Each coordinate first uses the
provider's nonloading presence method. A present coordinate still runs the
real acquiring getter, with its loading, activation and population effects.
Only after that getter returns does the query load current `R` for the Chunk
call. It returns current `R` again after the scan.

Normal gather reads current query Y coordinates to select slices. It clamps
only a negative lower slice and an upper slice past the array length. Each
slice captures its List once, then repeatedly loads that same List's size and
indexed candidate. It skips only the excluded reference, reads the candidate's
permanent bounds, and appends each strict intersection to its passed destination.
Duplicate references remain separate occurrences in their original order.
No dead, player, construction-World, ID, coordinate or membership filter is added.

Blank dispatch uses the actual acquired state's Blank identity before resolving
a normal header. Its original override returns immediately, including with null
query or destination arguments. It neither clears nor inspects the destination.
A normal state lacking a genuine store-owned header is an explicit dependency
failure, rather than a successful empty result.

Acquisition uses the installed concrete coordinator. A different live provider
can supply its actual presence result, but a reached getter whose native binding
does not match stops explicitly. It never acquires through the old provider on
behalf of that replacement. General provider replacement remains unfinished.

## Ownership and mutable views

Each appended occurrence retains its actual Entity independently of Chunk and
World membership. Two occurrences of the same pointer own two holds. The native
typed ledger stays separate from the source `void *` List and captures the
constructor descriptor independently of the replaceable source field.

A successful query returns a held List view. It preserves descriptor and World
lifetime, not contents: another query can clear and rewrite the same List while
an earlier view remains held. `count` and `get` read its current contents. A
borrowed Entity returned by `get` must be retained before any operation that
could clear its last occurrence. Release every view exactly once; retirement
and free return BUSY while a view is held.

Ordinary clear releases each occurrence, sets size to zero, and retains List
identity and backing capacity. Append grows distinct source-pointer and typed
hold spans together. Native accounting charges old and new capacities during
replacement. A late allocation or hold failure preserves earlier appends.
Unknown replacement List descriptors are not adopted as owning storage.

Terminal native container retirement validates the complete typed ownership
graph before releasing any occurrence. It then retires nearby results alongside
World and Chunk Entity/TileEntity containers. It does not execute gameplay clear,
removal, dirty stores, membership changes, callbacks or saves. Final World free
uses the captured original descriptor even if source `R` was replaced.

## Arithmetic and errors

Each range evaluates `(coordinate - 2.0) / 16.0` or
`(coordinate + 2.0) / 16.0` before the actual Java floor helper. NaN becomes
integer zero. Negative overflow and negative infinity can become `INT_MAX`
because the helper's subtract-one wraps. Inclusive World loop increments also
wrap as Java int; no silent work cap or successful endpoint shortcut is added.
Normal Chunk slice bounds still limit its reached scan to actual slices.

Intersection uses the original rejection comparisons. Touching faces reject;
unordered NaN comparisons can admit an occurrence. Query coordinates are not
normalized, copied into a stable six-coordinate snapshot, or written back.
Caller keeps the query's actual storage owner live independently of the result
List, including when ordinary clear releases an Entity's last result hold.

Diagnostics retain the reached World and Chunk PCs, selected slice/index, native
Entity hold cause, and the exact acquiring attempt's loading, activation,
population, Registry and TileEntity descendants. Failure preserves the caller's result
and all completed source effects. Native budgets and ownership admission do not
claim identical Java ArrayList allocation-failure or garbage-collection timing.

Block collision shapes and World `K`, Entity movement/environment effects,
FallingSand tick/landing/drop, and native application bounds-reset routes remain
unfinished. Nearby collection alone does not qualify those gameplay routes.

## Verification

`world.nearby_entities` passes 606 checks on each of MSVC, GCC and Clang.
All 161/163/163 configured cases pass without skips, including numerical Java
checks. Linux runs use ASan, UBSan and leak detection. The native fixture uses
eight GAME Worlds and fourteen retained loaded Chunk records; all twelve region
files retain their prepared slot/timestamp inventory. That inventory check is
not a comparison of every saved payload byte or a gameplay persistence replay.

Fresh original/native gather comparisons pass 353 exact successful rows and
three precisely checked nullable-failure prefixes per side and compiler. Eight
generic malformed Java collection cases are retained outside native replay.
The [source map](../../reference/nearby-entities.md) records reproduction and
the different original/native research setup. Original ChunkProvider traversal,
genuine descendant reentry replacing R during acquisition, and whole-game
movement/collision remain unqualified.
