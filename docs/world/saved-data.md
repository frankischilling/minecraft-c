# Saved-data ID ownership

`mc_world_saved_data` owns the ID counters loaded during original World startup.
It is separate from WorldInfo, chunk storage and the future named SavedData
subtypes. The ordinary load-or-create World constructor creates this owner
before loading WorldInfo; the direct-metadata and copy overloads have their own
order. The complete native World constructor remains unfinished.

The [interface](../../include/mc/world/saved_data/storage.h) constructs an owner,
allocates the next ID, exposes read-only counter inspection and retires it.
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

Named SavedData uses different behavior: typed construction, a shared cache/list,
dirty flags and gzip files with lowercase `data`. Those operations and the
concrete mutable MapData owner remain unfinished. They are not implemented by
the ID-counter interface. [Reference and verification](../reference/saved-data-ids.md)
separates constructed native contracts, actual original captures and the limited
memory replay.
