# Live Chest and Spawner owners

[`runtime/tiles/tiles.h`](../../include/mc/world/runtime/tiles/tiles.h) exposes
the concrete Tile, shared class/literal owners, Chest inventory and Spawner
fields. World lookup, installation, removal and block hooks use actual retained
Chunks, Registry objects and World lists. This adds native gameplay bodies;
complete original/native Tile equivalence and populated save/reload remain open.

## Objects and shared classes

The application World-class owner holds one lazy Tile-class namespace shared
by its Worlds. Code literals retain the Registry's actual string identity.
The base class registers the original eight type names; reached constructors
currently implement Chest and Spawner. Other registered constructors fail
explicitly when reached. Class initialization preserves its completed prefix
and first failure rather than replacing it with a final static lookup table.

A newly constructed Chest has 36 physical slots and reports logical size 27.
Its NBT read creates the source's 27-slot inventory. Slot operations preserve
shared Stack identity, replacement order, count clamping and the later dirty
call. A failed dirty call can therefore leave the installed Stack and clamped
count visible. Spawner construction and reads retain its actual name and delay;
player/mob lookup and complete ticking are unfinished.

NBT import owns independent Tile and Stack source trees. Building overlays live
typed fields while preserving supported unknown fields. It uses existing Stack
read/write owners and native allocation limits. This preservation policy does
not establish original HashMap wire order or mutable-compound failure timing.

## World, Chunk and collection order

Lookup acquires through the actual current provider, including its nullable
receiver read on a cached hit. A missing Map value reaches captured Block.p,
the live Block.m element, the actual Tile-block cast and concrete factory in
source order. Existing Map values retain their own identity and mutable fields.
Installation and removal preserve earlier World-list, coordinate, Map and
invalid-state effects when a later operation fails. Chest removal constructs
and publishes real ItemEntities through the shared Entity owners.

Each Chunk Map entry and each World c/G occurrence owns a separate Tile hold.
Duplicate values are retained and released per occurrence. Map storage includes
its actual nodes, bucket table and import journal in the common Chunk direct
allocation ledger. The embedded owner remains inspectable and releasable when
source l is replaced. Source Map operations still require their current
supported descriptor; ownership cleanup does not repair that projection.

Activation sets active before registering actual Map values into c or G,
selected by the live L flag, then proceeds to the Entity slices. Tile detach
invalidates each actual value without dereferencing a World receiver or clearing
the Map. The explicit deferred-G phase preserves duplicate occurrences. These
bodies do not supply the missing complete World tick or nonempty Entity
activation protocol.

Detailed diagnostics carry the whole Tile/Registry/Stack/NBT/Entity/publication
cause with separate caller, provider, load and activation positions. Each
acquisition attempt owns a fresh synchronous diagnostic frame. A nested attempt
restores its predecessor; returned errors are not reconstructed from ambient
last-error state or diagnostic strings.

## Loading and native retirement

The GAME loader stages the same decoded state, arrays, document and concrete
header as an owned, leased Store entry. It traverses Entities before TileEntities
and normalizes blocks only after both loops complete. A nonempty Entity list
currently stops at its missing NBT factory before q is set. Supported Tile
records use the real factory, local installation and retained provenance.
Coordinate mismatch retains first-import effects and stops at the unfinished
relocation diagnostic before normalization. See [storage ownership](storage.md).

Failed stages remain counted and inspectable. Their direct source operations
are disabled, while passive owner validation and native teardown remain usable.
An empty Map can still own a table or import journal; count zero does not make
it eligible for the old empty serializer. Outer live Tile saving and nonempty
Entity serialization remain unsupported.

`mc_world_runtime_retire_entity_containers` now admits the complete World b/c/G
ownership and every bound Store Chunk, including failed stages, before releasing
any occurrence. It then terminates source use and releases actual Entity/Tile
holds without saving, clearing source flags or rebinding objects. External
aliases keep their real World binding. Class destruction also admits Registry
and Entity releases before retiring Tile literals; a genuine Registry BUSY
result leaves the entire class owner intact.

## Validation boundary

All 143 configured public cases pass on native Windows MSVC and all 145 on each
sanitized Ubuntu GCC/Clang build, with zero skips and local JDK comparisons.
The dedicated Tile scenario passes 468 checks on each host. Each run retains
672 unchanged selected source inputs, 254 external zlib pins, 161 products and
31 capture directories. These are native integration contracts, not original
Tile replay or complete populated persistence qualification.

`world.tiles` constructs real owners on both sides, exercises Chest/Spawner NBT,
Stack aliases, duplicate Map/List ownership, block hooks, actual item drops,
failed dirty effects and joint retirement. Real resource callbacks exercise
atomic class retirement during entered initialization. A copied native
normalization-map input removes the stored Tile block IDs after import, making
the required import-before-normalization order observable. Four complete seed
region baselines remain byte-equal. `--mode core` selects the smaller constructor
and collection subset for diagnosis.

`world.provider_activation` separately checks retained failed Entity/Tile loads,
complete pre-normalization block bytes, wrong-location refusal, unchanged
outputs, typed caller frames and real lease refusals. These native contracts
and source review do not supply original replay, a general Java collection/heap
profile, full resource-graph qualification or complete population/persistence.
