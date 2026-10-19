# Scheduled update state

The [entry API](../../include/mc/world/schedule/entry.h) owns one stable scheduled
record: full signed X/Y/Z/block values, signed due time and a signed sequence.
Construction allocates before consuming the shared sequence, advances that
sequence with Java long wrapping, then stores its old value in the record. The
eventual class profile must share that sequence across worlds.

Equality uses all four key integers. The original wrapped hash is
`((((x * 128) * 1024) + z * 128) + y) * 256 + block`; collisions remain distinct
keys. Comparison uses signed due time and then signed sequence, without
subtraction or an extra coordinate tie breaker.

The [membership API](../../include/mc/world/schedule/membership.h) retains the
same record owners. Duplicate insertion preserves the existing owner and returns
false, including at its configured entry limit. An equal distinct query can
remove an entry; removal releases the stored membership hold. Other holds remain
valid. Checked growth stages new storage and the candidate hold before publishing
changes, and its allocation ceiling covers overlapping old/new bucket arrays.

Uniform time shifting changes the actual retained records with wrapped long
addition. It neither rebuilds ordering nor changes membership. The audited
membership operations expose no traversal order or callbacks.

The [ordered owner](../../include/mc/world/schedule/ordered.h) preserves the
captured library profile's red-black tree history. Insertion and removal search
from the historical root using current time/sequence fields. Insertion keeps the
stored identity on a comparison-zero hit; a path miss can insert another node holding the
same record. Each node owns a separate hold. Structural-first lookup follows
left links and returns an independent hold without comparing or removing.

After wrapped time changes, removal can miss even the record returned by first.
The tree remains in that state. Two-child deletion transfers the successor's
existing hold and retires the matched hold without allocating or retaining.
Empty first has a distinct exceptional status. Explicit native node/byte limits
preserve outputs and collection state on failed admission; they do not model
every original VM allocation failure.

World admission, immediate updates, tick draining, callbacks and persistence
remain outside these collection APIs.

`world.scheduled_membership` exercises 329 native contracts for collisions,
identity, wrapping, separate holds, growth limits and unchanged failure outputs.
`world.scheduled_ordered` adds 940 native checks for identity, limits, successor
holds, deletion histories and mutable-key search paths. Both suites pass on
Linux GCC 13.3 and Clang 18.1 with address, leak and undefined-behavior checks,
and native Windows MSVC 19.51 without sanitizers.

The ordered replay matches 13,584 captured JDK public-API steps on those three
compilers. Its project-owned Java records compare the source-proven time/sequence
pair; it uses no original game classes. This qualifies the finite collection
profile, while original/world scheduler comparisons remain pending. See the
[source evidence and fixture provenance](../reference/state-records.md).
