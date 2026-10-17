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
