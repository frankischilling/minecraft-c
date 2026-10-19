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

An ordered schedule, world admission, immediate updates, tick draining and
persistence are not implemented by these APIs. The original ordered tree's
history remains observable after time changes; a newly sorted priority queue
would not preserve that behavior.

`world.scheduled_membership` exercises 329 native contracts for collisions,
identity, wrapping, separate holds, growth limits and unchanged failure outputs.
The GCC 13.3 Linux run passes with address, leak and undefined-behavior checks
enabled. Native Windows MSVC 19.51 passes the same checks without sanitizers.
Clang and original/world scheduler comparisons remain unverified. See the
[source evidence](../reference/state-records.md).
