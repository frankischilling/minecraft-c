# GAME Chunk acquisition and activation

Ordinary GAME acquisition loads a region record or calls the
[concrete normal generator](provider-generation.md). Generation returns its
same final store-owned state. The existing coordinator then publishes that
identity in the cache map and insertion list, sets active, registers actual
Chunk children and evaluates population gates. COMPOSITION retains its
separate numerical/callback contract.

## Loaded ownership

A staged loaded record retains its existing stored state, data, arrays
and source NBT document. Before semantic imports, a new concrete header binds
World d, three distinct Nibble views, the tile Map, List array and eight
distinct empty Entity lists to that same storage. It does not copy or clear
payloads, finish the Chunk, change its coordinates or replace its state.
The Store owns and leases that same header through Entity then Tile imports.
Normalization follows both loops; ordinary loaded time stamping still belongs
to the acquisition coordinator.
Concrete loaded coordinates remain final after binding. Generic unbound
stored records retain their separate native editing contract.

The retained stored serializer continues to overlay current fields on source
NBT, preserving unknown root/Level children. It does not switch to the fresh
generated serializer because a concrete header exists. Actual nonempty
Chest/Spawner imports use the [live Tile owners](tiles.md). Nonempty Entity NBT
and unsupported registered Tile constructors remain explicit reached failures.
Failed stages retain their owned prefixes and disable source continuation.
The [fixed-shape decoder](storage.md) keeps its native transport contract.
Wrong-location loading stops after first imports at the unfinished relocation
diagnostic, before normalization or a second builder.

The header has a separate native allocation limit equal to the store NBT
allocation cap. This does not add its bytes to the decoder's existing budget
or reproduce Java decoder allocation and partial-failure timing. A failed
header allocation frees the unpublished loaded record and preserves output.
Retirement frees the borrowed header before its stored payload. World free
checks both generated and loaded bindings to exact d before teardown.

## Activation and detach

Tile registration uses the actual Chunk Map and World d. The World Collection
method first reads L, selecting actual G when true or c when false. Only that
selected destination receives actual retained Map values, preserving duplicates.
Entity registration uses the
actual current slice and World b, once per each of eight slices. Empty input
does not reach individual Entity hooks. The implementation checks reached
destination metadata even when the source is empty; it does not preflight
the alternative branch or future slices.

The coordinator sets active before registration. A later failure retains
cache publication, insertion history, active and earlier effects, while the
ordinary output remains unchanged. Chunk operations use their actual d
receiver rather than substituting the callback coordinator's World. Each
entered World invocation holds that receiver through its operation.

Detach first clears active. Tile Map traversal invalidates each actual value
without a World invocation or Map removal. Empty traversal reaches no Tile.
Each Entity slice
then appends to actual World D. The existing pending maintenance path performs
detach, real main/extra saving, pending removal and cache/list unpublication.
Main GAME saves use the actual wall clock and current Info, not a supplied
test timestamp. Exact store retirement remains a separate checked operation.

Collections here have real finite identities and native pointer/count/capacity
metadata. Tile c/G operations maintain their actual occurrence ledger and
modification count. Values-view heap identity, Java allocation/OOM timing,
nonempty Entity activation and complete ticking remain outside this boundary.

## Verification boundary

The [paired source anchors](../reference/chunk.md#activation-and-detach)
qualify the empty collection order. Native integration covers acquisition,
cache identity, persistence and checked ownership separately from the retained
original terrain comparisons. Final empty World lists alone do not demonstrate
the number of empty calls or branch selection. Defensive null/invalid receiver
guards without a supported constructor path remain source-reviewed.

The preceding known-empty checkpoint passed 51 selected cases on MSVC, GCC and Clang, including 544 native
integration checks. Linux runs use ASan, UBSan and leak detection. Tests cover
natural population-gate failure, exact loaded array and unknown-NBT retention,
live Info growth, cold loaded Registry state, opaque refusal, real GAME region
timestamps, cache limits and checked maintenance/retirement. Existing private
generation input/data comparisons and numerical fixtures pass separately.
These are selected local checks, not a fresh complete local suite or a new
executed-original activation observation.

Run `ctest --test-dir <build> -R '^world.provider_activation$' --output-on-failure`.

Current focused MSVC validation also covers real Tile imports, retained failed
Entity/Tile stages, wrong-location refusal after first imports, full untouched
block bytes and independent typed caller frames. The [Tile test](tiles.md)
adds source hooks, item drops and coordinated disposal. Original replay and
complete live Entity/Tile save interoperability remain unfinished. The separate
[GAME save contracts](saving.md) cover supported live Tile writes and constructed
native coordinator unload/reload on the same continuing World.

Feature population, full object lifecycle, complete synchronous block/lighting
closure, live-player metadata saving and playable startup remain
unfinished. A natural population gate fails explicitly; it is not disabled to
make acquisition appear complete.
