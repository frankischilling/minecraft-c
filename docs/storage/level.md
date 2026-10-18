# World metadata codec

[`level.h`](../../include/mc/storage/level/level.h) interprets the original
`level.dat` root's `Data` compound. It consumes an already parsed NBT tree and
builds an independent owned NBT document. Gzip, paths, fallback, session tokens
and file rotation belong to the world owner.

## Types and defaults

| Fields | Exact tag type | Missing value |
| --- | --- | --- |
| RandomSeed, Time, LastPlayed, SizeOnDisk | Long | 0 |
| SpawnX/Y/Z, version, rainTime, thunderTime | Int | 0 |
| raining, thundering | Byte | false |
| LevelName | String | empty UTF-16 string |
| Player | Compound | absent |
| Player.Dimension | Int | 0 |

Every nonzero Byte boolean is true. Present fields of another type fail;
numeric types are not coerced. A missing `Data` child succeeds with defaults.
This distinction matters to the original primary/backup loader: defaults do
not trigger fallback. Version and spawn values are not range-validated here.

`mc_level_decode` returns borrowed strings and Player data. Keep the source NBT
document alive and immutable. Names use explicit UTF-16 units, preserving NUL
and lone surrogates through the existing modified UTF-8 codec.

## Building and ownership

`mc_level_build` takes an info view, an optional source root and explicit
`saved_at_ms`. It writes that clock sample as LastPlayed without changing the
input's loaded LastPlayed field. Version is copied unchanged: the actual
McRegion world-save route separately chooses 19132. A null Player explicitly
omits the record at this codec seam.

With a source root, known fields replace their existing positions. Unknown
root/Data children and the root name remain. Player children remain opaque.
Without a source root, the result has a fresh unnamed root and only the known
schema. Normal NBT serialization still canonicalizes Java NaNs and empty-list
type bytes; preserving unsupported application data does not bypass the writer.

The original info serializer constructs fresh compounds and drops unknown
root/Data fields. Retaining them here is an intermediate preservation policy,
not evidence that C implements player serialization. The world owner further
refuses unsupported Player replacement or removal.

Success returns a document owned independently of every input. Release it with
`mc_nbt_document_free`. Failure leaves the result pointer unchanged. The
allocation limit includes this call's temporary overlay, serialized bytes,
writer scratch and resulting document, excluding preexisting borrowed inputs.
Other NBT limits apply to the whole result.

## Verification scope

`storage.level` covers exact types/defaults, signed endpoints, UTF-16 ownership,
clock/version separation, unknown-field retention, Player selection and bounded
failure preservation. `tools.level_reference` validates the lossless original
fixture, runs complete original-derived C codec comparisons and separate
preservation-policy checks. The live original comparison is `reference.level`
under `MC_ENABLE_STORAGE_REFERENCE_TESTS`.

Original handler observations additionally retain primary/backup precedence,
rotation, constructor flags and lock checks. Those observations alone do not
verify the native world owner. Source-bound acceptance remains in progress.
