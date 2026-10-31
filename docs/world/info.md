# WorldInfo ownership

`mc_world_info` implements the original client `ei` / server `ct` metadata
record. Its [interface](../../include/mc/world/info/info.h) and implementation
live under `world/info`; the library depends on storage and the private platform
clock. It initializes no World, provider, registry, display or Java runtime.
The native runtime can retain this owner and borrow its actual live cells.
World constructor ordering remains separate work.

Each WorldInfo has thirteen scalar cells and two nullable references: name and
Player compound. A copy has independent scalar cells and retains the same two
references. Sharing a save handler does not make two Worlds share one live
WorldInfo. The store's current `mc_level_info` remains its separate, expiring
persistence view.

```mermaid
flowchart LR
    A["WorldInfo A: own scalars"] --> N["shared immutable name"]
    B["WorldInfo B: own scalars"] --> N
    A --> P["shared Player: mutable root name"]
    B --> P
```

## Construction and references

Allocate a stable RAW owner, then enter one fresh, NBT or copy constructor.
Fresh construction stores only seed and the exact supplied nullable name.
NBT construction consumes the inner Data compound, writes fields in original
order, publishes Player, then reads its Dimension into a separate cached cell.
A wrong Dimension therefore leaves Player assigned in the failed owner. Copy
construction copies the cached dimension without reading Player again.
Reached failures latch FAILED_CONSTRUCTION and retain completed stores; they
cannot be retried. A failed owner must not become an enclosing World's metadata.

Names preserve UTF-16 code units and identity. NULL differs from a nonnull empty
name, and equal text does not merge identities. Name replacement retains the
new identity before releasing the old. Client Player replacement accepts NULL
and never refreshes the cached dimension; the corresponding server operation
returns UNSUPPORTED_OPERATION because that method is absent there.

Native ownership conversions are separate from those constructors:

- `name_copy` creates a distinct immutable name identity.
- `player_copy` retains an independent immutable child tree and a stable mutable
  root/name overlay. Its base-tag name getter returns the shared empty literal
  for a NULL raw name; an explicitly supplied empty name keeps its identity.
- `data_copy` retains the original inner Data tree, including unknown values,
  types, order and raw floating bits. Its selected name/Player handles can be
  used by multiple constructors. Typed Data reference setters assign existing
  handles, preserving aliases across fresh and NBT construction. Player
  assignment renames that same compound to the shared Player key literal.

Immutable source provenance remains a historical view when the selected live
references change. It is not the live Player root after a rename. Copies retain
the provenance owner as an additional native lifetime operation; the original
copy constructor does not deep-copy or preserve a Data tree.

All owners use explicit native holds and exclusive access. Independent holds
keep old references alive across replacement and Info retirement. Ownership
copies check their heap budgets, including tree, wrappers and name storage;
validation scratch and retained storage occupy separate phases. Hold overflow,
allocation and admission failures are explicit native policies. No atomic
synchronization, Java GC or allocation-failure equivalence is claimed.

## No-list serialization

`record_create` executes the no-list serializer and returns a stable inner Data
record. It allocates the destination, captures the current Player reference,
then writes seed, spawn, Time and SizeOnDisk. One real wall-clock sample supplies
output LastPlayed. It then writes name, version and weather, and finally renames
and inserts that same Player root. The record holds the actual name and
Player identities after its Info retires; `record_name` exposes the identity
supplying the LevelName span.

Serialization does not change live LastPlayed, cached dimension or Info.player.
It does not inspect Player.Dimension, force version 19132, preserve unknown Data
children or create an outer root/Data envelope. A nonnull empty name succeeds.
A NULL name fails at LevelName after the clock point and before Player rename.
The native output pointer remains unchanged on failure. Later codec failure
cannot undo an already completed shared Player rename.

The borrowed record can be passed to the generic NBT codec while its references
remain live and exclusive. Normal writing has the codec's documented NaN/list
canonicalization. Insertion order represents the source put sequence; it does
not reproduce Java HashMap byte traversal order.

## Runtime and storage binding

`live_get` exposes a const descriptor of pointers to this owner's actual time
and spawn cells, only in READY. Setters and direct cell writes immediately affect
the same values. Copies have different cell addresses. Keep an Info hold and
exclusive access throughout use; the descriptor acquires no hold itself.

An optional `runtime_options.world_info` retains that exact owner. Acquisition
and lifecycle contexts point directly at its cells across callbacks, including
failing callbacks. Full metadata reads use all its fields and cached dimension.
Runtime flags have separate runtime-owned cells. `runtime_live` now returns a
const pointer descriptor: write `*live->time` or `*live->saving_disabled`.
Info survives runtime retirement under an independent hold, without retaining
the World in return. The legacy NULL binding keeps separate private scalar
backing and its earlier store-view overlay behavior.

Typed runtime chunk transport attributes successful signed Java-int region
growth only to the calling Info's SizeOnDisk. It needs no disk metadata record
and does not change a shared store's legacy size counter. Creating a runtime
does not save metadata or run the original World constructor.

`store_save_world_info` checks this native store's session token, invokes the
actual no-list serializer, packages its record, and shares gzip/checked rotation
with the legacy writer. That session admission is extra native policy: the
original one-argument handler does not itself check it. The typed route keeps
the Info's version and accepts opaque Player children without rereading
Dimension. Its replacement persistence view never replaces the live Info.
Unknown retained root/Data fields survive as an additional native preservation
policy. The legacy opaque-Player pointer guard remains intact.

The actual World caller passes live players to a different serializer. An
Info-bound runtime explicitly refuses that metadata operation until those
descendants exist; it does not substitute no-list saving. Mutable Player
children, player simulation and original World construction also remain open.
[Construction evidence](../reference/world-info.md) and
[binding contracts](../reference/world-info-runtime.md) distinguish these scopes.
