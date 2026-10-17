# Approved intentional differences

| Decision | Approval | Scope |
| --- | --- | --- |
| Local usernames; omit Minecraft/Microsoft account authentication | Owner's development prompt | Does not exclude multiplayer handshakes, joining, skins, or any other gameplay behavior. Servers requiring authenticated accounts are outside supported identity operation. |

There are no other approved gameplay exceptions. The MIT license choice covers
new project work and does not grant rights to distribute original resources.

Malformed input uses bounded memory and defined C behavior. Necessary
departures from original malformed-input behavior are recorded separately
with the parser evidence. Valid gameplay behavior remains in scope.
Legacy online-resource behavior, including unavailable skin
services, remains unresolved and needs an owner decision before being declared
complete.

## Bounded malformed-input handling

The raw NBT parser rejects negative list lengths rather than reproducing the
original reader's empty-loop result. It enforces explicit byte, allocation,
tag, element and nesting limits; caller-constructed heterogeneous lists are
rejected rather than written as malformed streams. These are parser safety
and API constraints, not approved changes to normal gameplay. See
[the codec's bounds](storage/nbt.md#bounds-and-failure-behavior) and
[original observations](reference/nbt.md).

Memory gzip/zlib decoding validates complete member checksums/trailers and
enforces input, output, allocation and member budgets. The original gzip NBT
helper can return a root before reading a missing or corrupt trailer; the C
module rejects those containers. Recognized later gzip members must also be
valid even when an earlier member contains a complete root. Reserved gzip header
flags are rejected where the selected Java readers accept them. These are
malformed-container departures, with [specific original observations](reference/compression.md),
not a change to valid gameplay or a save-compatibility claim.

The noise interface rejects dimensions/products above `INT32_MAX` and insufficient
caller spans before writing. It uses defined Java casts/wrapping instead of C
signed overflow or out-of-range casts. These are bounded C call contracts, not
changes to valid terrain-generation scenarios; exact nonfinite NaN payloads are
not promised. See [the noise interface](world/noise.md).

The [simplex interface](world/simplex.md) applies the same checked-span policy
without reproducing Java array failures. Valid numerical calls retain the
original floor/cell-wrap behavior and output accumulation/clearing. Nonfinite
NaN payload identity is outside the exact comparison contract.

The [climate/biome interface](world/biome.md) uses bounded transactional buffer
growth and explicit ownership. Classifier inputs must be finite and within
[0,1], temperature grids may be rectangular, and biome grids are square. The
original rectangular biome width/width call pattern remains documented evidence,
not a supported comparison domain or an approved gameplay exception.
