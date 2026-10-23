# SavedData ownership

`mc_world_saved_data` owns the ID counters, named cache and ordered save list
used by the actual World. It is separate from WorldInfo and chunk storage.
The ordinary load-or-create World constructor creates this owner
before loading WorldInfo; the direct-metadata and copy overloads have their own
order. The complete native World constructor remains unfinished.

The [interface](../../include/mc/world/saved_data/storage.h) constructs an owner,
loads and publishes named MapData values, saves dirty ordered occurrences,
allocates IDs, exposes passive inspection and retires its retained owners.
Construction copies the store's fixed `data/idcounts.dat` path and admitted
UTF-16 keys. No retained pointer borrows a metadata document or the supplying
store. Each owner has independent counters, even when several use one file.
The owner remains usable after that store closes. A NULL store selects the
original in-memory mode and performs no file operations.

## Construction and allocation

Construction reads one raw named Compound, without gzip or a `data`/`Data`
envelope. It keeps only Short children. Ordinary NBT duplicate-name replacement
happens before that filtering, so a later non-Short replacement removes an
earlier counter. Root names and trailing input bytes do not become counter
fields. Construction does not create a missing ID file or world metadata.

The first ID for a key is zero. Subsequent calls increment a signed Short with
Java narrowing: 32767 advances to -32768, and -1 advances to zero. Return values
are sign-extended integers. Keys compare exact UTF-16 units, including embedded
NUL and unpaired surrogates. A NULL key and a nonnull empty key stay distinct in
memory. Input spans are copied; later caller edits cannot rename a stored key.

The counter changes before persistence begins. Each successful write directly
overwrites raw `idcounts.dat`. There is no session check, compression, recovery
rotation or implicit metadata save on this route. A stale session token therefore
does not prevent ID allocation. Other store operations retain their own session
policy.

## Files, diagnostics and bounds

The opened writer truncates the actual file and emits the reached Compound,
Short type, modified-UTF name, payload and End bytes. An encoded name over
65535 bytes fails after the already emitted tag type. File truncation and earlier
bytes survive a later encoding, write or output-byte-limit failure. The native
writer always attempts checked flush/close and retains the first error.

Caught I/O or malformed-data/encoding failures preserve the original normal
counter return. `last_caught` retains their diagnostic instead of printing a
Java stack trace. A missing constructor input is an empty owner without a caught
error. Native allocation, capacity and codec limits return explicit failures;
`next_id` preserves its output parameter but retains any completed counter
change. Neither result implies rollback of an earlier file effect.

Defaults allow 4096 counters, 65536 retained UTF-16 units and 4 MiB of owner
allocations. The header defines accounting exclusions and separate input/NBT
budgets. Raw output applies byte, tag, element and depth bounds. Input uses the
existing bounded regular-file reader, which closes before parsing; the original
parses before its close. These checked native resource/lifetime policies do not
claim equivalent JVM allocation failures, streaming read errors or durability.

## Remaining collection and subtype work

Native emission currently follows insertion order. Actual Java HashMap ordering
remains unfinished, including which key's encoding failure is reached first.
NULL and empty keys would both serialize with empty tag names, making that
ordering affect the reloaded counter. A file-backed owner containing both now
returns `UNSUPPORTED_STATE` before opening, after its counter change. It preserves
the prior file and output parameter. In-memory coexistence remains supported.
This explicit gap must be implemented before complete equivalence is claimed.

Named SavedData uses the separate behavior below. The ID-counter file has no
named-value envelope, dirty flag or implicit named save.
[ID reference and verification](../reference/saved-data-ids.md)
separates constructed native contracts, actual original captures and the limited
memory replay.

## Named values and MapData

The manager retains actual nullable UTF-16 String keys, MapData values and
ordered occurrences. GAME managers bind the distinct client or server base
type. An opposite-base put is rejected before mutation; a miss constructs the
requested class before catching an opposite-base cast without caching it.
Cache lookup precedes the requested Class, so a hit can
return a value even when that argument is NULL. A miss in memory mode or for a
missing file returns NULL. A present file constructs the selected client/server
MapData before reading its gzip lowercase `data` Compound. A caught read or
subtype failure after construction still publishes the default or partially
read value. A second lookup returns that same cached owner.

Put removes an existing key and the first identity-equal ordered occurrence,
then inserts the new entry and appends its occurrence. Other aliases survive;
two keys may retain the same value in the ordered list. Neither put nor read
marks a value dirty. Save visits the actual list and clears the same value's
dirty flag after a normal or caught writer result. A later alias then observes
the cleared flag. Memory mode also clears dirty. An escaping native resource
failure preserves dirty and any completed mutation or file prefix.

Each value retains its immutable nullable name separately from its lookup key.
The value name selects `<name>.dat` in the copied handler data directory; a
NULL name selects `null.dat`. The World get, put and ID wrappers delegate through
the current actual `World.z`. Full World saves already visit that owner's list.
They do not replace it with a temporary manager.

The [MapData interface](../../include/mc/world/saved_data/value.h) owns its base
name and dirty cells, center coordinates, signed dimension and scale, update
counter, colors array and three distinct tracker collection descriptors. New
values have a zeroed 16,384-byte colors array and empty trackers. Read uses exact
numeric tag types and signed scale clamping. A standard 128-by-128 input retains
the same colors array at any length. Other dimensions publish a new zeroed
array before copying. The original impossible AND guards and linear row-spill
behavior remain; later type, source-load or destination-store failures preserve
earlier scalar and pixel changes.

Write emits seven fields: dimension, centers, raw scale, Short width and height of 128,
and the current colors backing. Its record retains that same array, so later
pixel edits remain visible even after the value is retired. NULL colors survives
record creation and fails when encoding is reached. Fresh output drops unknown
old root/data tags. Save directly overwrites gzip without metadata rotation,
session checks, clocks, Random calls or ID rewrites.

## Shared arrays and remaining qualification

[Shared byte-array handles](../../include/mc/storage/nbt/array.h) distinguish a
NULL array from an actual empty array. A decoded handle identifies one exact
reachable ByteArray tag and keeps its actual document and mutable payload alive.
No payload copy supplies MapData's standard-size read. Different empty tags
retain different owners even when both decoded payload pointers are NULL.
All access is exclusive; a caller must not run a const reader or codec while
editing the same bytes. Immutable provenance trees keep their separate contract.

Default manager limits add 4,096 named entries and ordered occurrences. Retained
String/value/array owners, records, decoded NBT and compression have separate
bounds; the header describes direct owner/staging accounting. Native failures
retain completed prefixes rather than rolling back Java-visible mutations.

Constructed tests cover ownership, aliases, arbitrary colors lengths, signed
scale, malformed types, resize failures and allocation bounds. The
[named-data comparison](../reference/population-named-data.md#committed-checkpoint)
records clean-source original/native and public-replay qualification on MSVC,
GCC and Clang, plus separate hosted public/JDK checks. Its finite profile
executes two successful full saves and a separate manager load after the
seed23 origin prefix while retaining the same World. Gameplay restart and
original qualification of nondefault maps or failure behavior remain separate
work.
Map item updates, player trackers, map packets and nonempty Player serialization
remain unfinished. Filename conversion, original stream/error byte prefixes,
printed caught diagnostics, Java allocation timing and unrestricted collection
ordering remain unqualified; these are not approved full-game exceptions.
