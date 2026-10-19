# Chunk save, unload and owner retirement

The lifecycle coordinator and native world runtime compose saving, detachment,
ordered pending unloads and explicit owner retirement. Their interfaces and
failure policies are described below. Source
`46546d8322de52beb6c3a51f30e827c0260d2b30` passes finite qualification at the
[lifecycle checkpoint](../reference/lifecycle.md#exact-source-lifecycle-checkpoint).
The accepted [persistence foundation](../reference/storage.md#exact-source-storage-checkpoint)
remains a separate historical checkpoint. The v7 and v8 matrices are excluded
from this checkpoint. Complete Windows v10, GCC v9 and Clang v9 suites each
passed all 92 tests without failures or skips. Independent audits, storage
rechecks and complete capture preservation also pass. Windows v9 is excluded
after an original setup skip.
The reference controller retains bounded JDI mirrors from
first serialization. Its identity regression and the profile-scoped helper
interval regression have separate Windows/Linux passes.

## Saving and unloading

`mc/world/cache/lifecycle.h` borrows the cache, ordered pending set, stable clock
and explicit callback owners. Normal eligibility uses dirty, never-save,
entity-save eligibility and signed `now >= wrap(last_save_time + 600)`. Forced
eligibility instead compares current and last-save times. Active/populated do
not gate saving; blank eligibility is false.

Saving traverses the live indexed insertion list. Forced extra-save precedes
eligibility unless `never_save` is set. A present loader stamps the current clock
before main save. A returning wrapper clears dirty and counts an eligible
attempt, including caught I/O or an absent loader. Incremental attempt 24 returns
false even at the last entry.
A completed forced traversal flushes once. Callbacks may append, remove, replace
or recursively process live objects; there is no shadow graph or rollback.

Maintenance uses a 100-iteration loop, selecting at most 100 pending keys.
It retains each selected key and mapped identity through active=false, tile
invalidation, eight deferred entity
slices, main save and extra save. It removes that key from pending and the
current map, then only the first matching retained identity from the list.
Replacement at the selected key is still removed. Duplicate/list-only or
other-key references survive. Unload does not test eligibility or clear dirty.
Missing selected map entries return a bounded state error.

`mc_chunk_lifecycle_detach` also exposes the direct chunk entry, including
unpublished identities. It retains one local lease, clears active before tile
invalidation and visits every entity slice. Blank chunks have a virtual no-op
body. It applies no saving gate and does not save or change cache membership.
Earlier effects survive callback failures; all successful local retains unwind.

Dedicated saving-off skips unload and loader maintenance. Provider maintenance
still runs and returns its own Boolean, independently of pending emptiness.
Direct cache saving remains distinct from that gate. World saving checks the
gate, reports level progress, checks session, saves metadata/auxiliary, reports
chunk progress and invokes the chunk pass while discarding its Boolean. The
client incremental entry exposes that Boolean and saves metadata only for pass 0.
These functions do not implement world ticking.

## Ownership and native composition

The store registers up to 256 borrowed cache descriptors and optional live
fallback slots. Retirement rescans their actual map/list/fallback identities,
checks active state and explicit leases, and refuses while any reference remains.
No coordinate, packed key or cached Boolean substitutes for identity. Invalid
registered metadata prevents retirement. Unregistering a cache never frees its
chunks. Retiring one safe owner recovers admission capacity without saving,
clearing shared regions, changing metadata or deleting session files.

Reservations preallocate a link and reserve one slot from the same owned-plus-
reserved admission bound. They do not reserve every payload/codec allocation.
Generated retention can fail after generation changed RNG, manager and lighting
state. Composition preserves those effects, cancels its reservation and frees
unpublished staging only. A successful copy becomes the stable published
identity. Loaded owners keep their source NBT and never use the generated-copy
route.

`mc/world/chunk/references.h` provides balanced local lease hooks for acquisition,
population, saving and unloading. External deferred-registration adapters need
their own leases until their references end. Callers must retire every other
borrowed view before releasing its owner. C cannot detect arbitrary raw pointers;
this is explicit ownership, not a Java garbage-collector model.

`mc/world/runtime/runtime.h` binds an existing store and empty caller-owned
cache/ordered pending descriptors. Its const live descriptor contains pointers
to stable time/spawn/force/save-off cells. In legacy composition, each metadata
save obtains a fresh store view, overlays live
time/spawn and preserves current SizeOnDisk and loaded last_played. Serialized
LastPlayed uses the supplied clock. Metadata saved before chunk growth
intentionally lags that operation's later live SizeOnDisk.
`mc_world_runtime_read_info` exposes the same fresh overlay for inspection. A
failed query preserves its output, and inspection preserves the last runtime
failure diagnostic.

An optional retained [WorldInfo](info.md) supplies the actual time/spawn cells
and every field of the runtime's metadata view. Callback changes need no
entry/exit copying; copied Infos keep independent scalars even with a shared
store. Runtime-owned flags remain separate. Typed chunk transport accounts
growth only to the calling Info. This mode can start without disk metadata;
it does not manufacture a file to initialize the World. Its actual World/list
metadata save operation remains explicitly unsupported. Standalone typed
no-list store serialization is a distinct route, not a fallback for that call.

Runtime entry functions block destruction across callbacks, including progress,
auxiliary and provider callbacks without a selected chunk. Current chunk leases
block retirement even if callbacks remove every map/list reference. Free refuses
nonempty cache/list/pending, an active call/lease or a persistent binding; it does not unload or free
store chunks.

The runtime can borrow a nullable application Random profile. Without one,
existing storage/numerical operations remain available and default Random
construction is rejected. With one, the bridge invokes the actual profile
constructor once under call protection. Its exact typed status survives
unrelated operations; success preserves prior storage diagnostics. A bound
profile must outlive the runtime and its users. Runtime creation performs no
late substitute for the original World constructor's Random/clock sequence.

Persistent binding retains/releases protect actual Entity/Tile world fields
independently of entered calls, chunk leases and collection membership.
[ItemEntity construction](../entity/construction.md) retains at its reached
world-field store; even later constructor failure keeps that binding until
the Entity's final release. Removing a collection entry alone releases no
world-field binding. Retain overflow and release underflow are reported without
wrapping the count. These lifetime operations invoke no callbacks.

The native loader activates only empty entity/tile runtime records. Explicit
registration/detachment callbacks are empty while every required coordinator
boundary still runs. Generic callbacks can represent separately observed external
feature effects; they do not implement those feature bodies. Runtime writes
reset entity-save eligibility only for known-empty records, after session check
and before serialization. Later failures can retain that reset. Generic opaque
transport preserves its existing policy. Native storage failures escape as
fatal outcomes with retained diagnostics; this safer policy differs from
swallowed original I/O failures.

## Test domains

Constructed tests cover flag/overflow eligibility, 23/24/25 cutoffs, live mutation,
recursive lease depths, retained-key removal, 100/101 unloads and dedicated gates.
Owned-file tests cover registry/reservation bounds, retirement/reload, session
conflict before q-reset, metadata accounting, generated-copy independence and
failed retention after generation. These are contracts, not original-game oracle
values. The actual-runtime [JDK corpus](../reference/java-semantics.md#integer-set-history)
is separate evidence. Tick-driven scheduling, general deferred cleanup,
entity/player bodies and running-game restart comparisons remain required.

The native protocol integration has 67,659 constructed checks on MSVC and
sanitized GCC/Clang working-tree builds. A separate development replay matches
133 genuine dedicated-constructor operations at 268 full API states, including
78 actual writes, unload and reload. It compares every supported array/header,
live metadata, ordered insertion/pending state and complete file inventory.
Untouched files remain byte-exact. Written records compare all typed NBT fields,
region headers/timestamps and trailing padding; compression bytes and compound
tag order have no semantic equality requirement. Each operation also preserves
native bytes outside its own measured write set.

The finite Boat/Sign public fixture adds 61 original callback pairs across
client, base-server and dedicated contexts. Replay compares 141 supported C
states and 690 complete normal arrays, with 39 response corruptions and seven
actual invalid-input controls. The fixture passes on all three working-tree
builds. Fresh original-only captures pass strict full-graph/control validation;
fresh base detach capture plus C comparison and native activation-refusal checks
also pass on Windows JDK17 and Linux JDK21. The refusal preserves all copied
file bytes through failed loading and EOF. Boat/Sign bodies remain external.

See [lifecycle reference evidence](../reference/lifecycle.md) for capture,
comparison, clocks and ownership limits. These development counts identify their
own finite runs. The [exact-source checkpoint](../reference/lifecycle.md#exact-source-lifecycle-checkpoint)
records the complete qualified matrix and audits; no working-tree pass
substitutes for source, executable and complete-result qualification.
