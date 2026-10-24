# WorldAccess callbacks

The GAME World runtime dispatches six of the ten original WorldAccess interface
slots through opaque native WorldAccess receivers. The public
[`runtime/world_access.h`](../../include/mc/world/runtime/world_access.h) interface copies a
typed callback table and borrows its context. Cell and range dispatch live in
[`mutation/notifications.c`](../../src/world/runtime/mutation/notifications.c), sound and
particle dispatch in
[`presentation.c`](../../src/world/runtime/presentation.c), Entity publication in
[`entity_publication.c`](../../src/world/runtime/entity_publication.c), Entity
sound in [`movement.c`](../../src/world/runtime/movement.c), and TileEntity dirty in
[`tile_entities.c`](../../src/world/runtime/tile_entities/tile_entities.c).

The ABI and loop order below come from the verified original client `fd`/`pm`
and server `dj`/`jp` class bytes. The supplied archive hashes are recorded in
[reference binaries](../reference/binaries.md). Native controls exercise these
dispatchers with constructed callback receivers. Original nonempty WorldAccess
traces remain pending. Concrete renderer, audio and tracker receivers remain
unimplemented.

## Implemented interface slots

Each interface method returns void. Native callbacks return a storage status to
transport a reached failure and its cause through C callers.

| Payload / native slot | Client interface member | Server interface member |
| --- | --- | --- |
| Cell / `cell` | `pm.a(III)V` | `jp.a(III)V` |
| Range / `range` | `pm.b(IIIIII)V` | `jp.a(IIIIII)V` |
| Sound / `sound` | `pm.a(Ljava/lang/String;DDDFF)V` | `jp.a(Ljava/lang/String;DDDFF)V` |
| Particle / `particle` | `pm.a(Ljava/lang/String;DDDDDD)V` | `jp.a(Ljava/lang/String;DDDDDD)V` |
| Entity added / `entity_added` | `pm.a(Lsn;)V` | `jp.a(Llq;)V` |
| TileEntity dirty / `tile_entity_dirty` | `pm.a(IIILow;)V` | `jp.a(IIILjh;)V` |

The four remaining slots are Entity removed (`pm.b(Lsn;)V` /
`jp.b(Llq;)V`), global refresh (`pm.e()V` / `jp.a()V`), record playback
(`a(Ljava/lang/String;III)V` on both), and player/event dispatch
(`pm.a(Lgs;IIIII)V` / `jp.a(Lem;IIIII)V`). They have no native callback slots
in this increment. The refresh call occurs conditionally inside the larger
World tick after a changed skylight value is stored; it is not an unconditional
standalone refresh operation.

## Source loops and payload order

PCs are offsets within each listed World method. The paired bodies use the same
loop PCs. Each size and get call has its own preceding load of current `u`.

| World method: client / server | Descriptor: client / server when different | size | get | cast | invoke | increment |
| --- | --- | --- | --- | --- | --- | --- |
| Cell `fd.j` / `dj.h` | `(III)V` | 9 | 23 | 28 | 34 | 39 |
| Full range `fd.b` / `dj.b` | `(IIIIII)V` | 9 | 23 | 28 | 40 | 45 |
| Positional sound `fd.a` / `dj.a` | `(DDDLjava/lang/String;FF)V` | 9 | 23 | 28 | 41 | 46 |
| Particle `fd.a` / `dj.a` | `(Ljava/lang/String;DDDDDD)V` | 9 | 23 | 28 | 43 | 48 |
| Entity added `fd.c` / `dj.c` | `(Lsn;)V` / `(Llq;)V` | 7 | 20 | 25 | 29 | 34 |
| Entity sound `fd.a` / `dj.a` | `(Lsn;Ljava/lang/String;FF)V` / `(Llq;Ljava/lang/String;FF)V` | 9 | 23 | 28 | 53 | 58 |
| TileEntity dirty `fd.b` / `dj.b` | `(IIILow;)V` / `(IIILjh;)V` | 28 | 42 | 47 | 55 | 60 |

The loop starts at index zero. A callback runs synchronously before the index
increments. The next condition and get reread the current list, so an append
can be visited and self-removal can skip the entry shifted into the old index.
Nested dispatch runs immediately with its own index. The selected receiver
stays held through argument evaluation and callback return even if registration
changes. Dispatch neither snapshots membership nor scans future entries first.

Cell and full-range calls forward their arguments without clipping or sorting.
The separate column wrapper orders its two Y values before calling full range.
Repeated endpoints in the current redstone, plate, fluid and detector callers still
select the six-coordinate World method and its invoke PC 40. The separate
original `fd.k` / `dj.i(III)V` body invokes the range interface at PC 37; equal
coordinates alone do not select that body.

Sound and particle pass the same nullable String identity and supplied numerical
values. The Entity sound overload selects a WorldAccess receiver first, then reads live
X, Y, float offset and Z for each iteration. It widens offset at PC 44 and
subtracts at 45 before invoke 53. An earlier callback can change the coordinates
seen by the next one. An empty list returns before Entity reads or arithmetic.
Entity-added callbacks receive the same nullable Entity after the complete
Chunk/World publication prefix and before dedicated Entity-index insertion.
TileEntity dirty tests presence at PC 4 and conditionally marks the Chunk dirty at 16;
both presence outcomes reach the WorldAccess receiver loop with the same nullable TileEntity.

A NULL current list fails at its reached size call. A foreign nonnull raw entry
fails at its reached cast before payload evaluation.
A NULL entry passes the cast and fails at invocation after payload evaluation.
A typed receiver with a missing callback also fails only at invocation. Thus
NULL Entity sound can fail at field PC 33 before a missing sound slot at 53.
Callbacks stop the loop on failure without an increment, retry or rollback.
Earlier callback and store effects remain visible. Diagnostics preserve the
reached source positions and returned status, operation and native system error.

## Ownership and admission

Registration appends every duplicate or NULL occurrence to the constructor's
actual `u` descriptor, captured at store PC 192. Each nonnull occurrence owns a
separate membership hold. The table copy does not take ownership of context or
callback code; callers keep both live until `mc_world_access_free` succeeds.
Free stays busy while any World membership or active dispatch holds the receiver.

The original registration bodies `fd.a(Lpm;)V` / `dj.a(Ljp;)V` call List.add at
PC 5. Only the client has `fd.b(Lpm;)V`, calling List.remove at PC 5. Native
removal deletes the first identical handle or NULL occurrence. The original
List controls object equality; opaque native handles use identity.
Server removal remains explicitly unavailable. Registration and removal require
the current descriptor to be the captured owner. GAME raw, constructing and
failed prefixes are admitted when that descriptor exists; retired and
composition Worlds reject these operations.

List growth checks the full replacement-array peak against the World allocation
budget while old arrays remain live. A failed allocation preserves membership.
This native budget does not reproduce JDK allocation or out-of-memory timing.
Callers provide exclusive access, including when Worlds share a WorldAccess
receiver, and keep callback arguments live through synchronous reentry.

Terminal disposal validates the captured descriptor and typed ledger before
releasing anything. Current `u` may be that descriptor, NULL, or a shaped empty
borrowed list. A foreign nonempty list rejects retirement before commit. Empty
foreign lists are never adopted, cleared or freed. Retirement releases captured
membership holds without callbacks and leaves final descriptor destruction to
World free; it does not perform source WorldAccess receiver removal or game unload.

## Validation boundary

[`world.world_access`](../../tests/world/runtime/world_access/test_world_access.c) uses
genuine constructed GAME Worlds and typed native callback contexts. Controls
cover forwarded values and identities, live append/removal/replacement,
duplicates, nullable receivers, missing slots, Entity coordinate changes,
returned failure causes and allocation limits. Cross-World callbacks also enter
the real redstone notification paths from pressure-plate inputs while their Worlds share the
defining Registry. These controls exercise native dispatch and ownership;
they do not supply original nonempty callback traces or concrete presentation
and tracking implementations. Existing original redstone captures retain their
historical runtime and artifact pins. Their finite comparisons do not qualify
this later WorldAccess receiver increment, default identity, constructor clocks, drops or
full World equivalence.
