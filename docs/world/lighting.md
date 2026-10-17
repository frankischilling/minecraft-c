# Light-region queue

`mc/world/lighting/queue.h` provides caller-owned light-region bounds and ordered
queue storage. It belongs to `mc_world` and uses the shared Java arithmetic
helpers. It initializes no display, audio, files, networking or Java runtime.

The world must first admit a request through its provider, recursion and cached
midpoint checks. This module then performs the original storage operation:
inspect the newest five entries, count both light kinds within that window,
merge the first same-kind match in place, or append. Disabling coalescing always
appends. Successful merges retain the entry's original position.

## Bounds and arithmetic

Region bounds retain six signed 32-bit coordinates. A contained request succeeds
without mutation. A nearby request can expand the stored bounds only when its
wrapped volume increase is at most two. The original volume multiplies
`max-min` extents without an inclusive `+1`. Endpoint padding, extents, products
and the volume difference use defined Java integer wrapping.

The module preserves reversed bounds, zero-width regions and integer-edge
behavior. It does not normalize coordinates or clamp Y. These observations
describe the original helper, rather than a general geometry convention.

## Storage and failure

Call `mc_light_queue_init` with an array of `mc_light_region`. The queue borrows
that array; the caller owns its lifetime. Entries `[0,count)` are live, oldest
first. The queue and storage must remain disjoint, with exclusive mutation.
An empty `(NULL,0)` allocation is valid. Initialization preserves storage bytes.
An observed baseline can be restored explicitly through the documented fields.

A caller-selected capacity can hold at most one million entries. A full smaller
buffer still permits a merge; an append returns `MC_LIGHT_QUEUE_FULL` without
mutation. The caller must provide more storage or report the failed request.
It must never silently discard work. The module allocates no memory.

At exactly one million entries, another unmerged request returns
`MC_LIGHT_QUEUE_CLEARED` and clears the live count, matching the original abort
policy. The caller must report `More than 1000000 updates, aborting lighting updates`.
Unused storage remains outside the live queue contract. There is no transient
million-and-first slot because the original immediately clears that entry.

Null inputs, invalid queue metadata and invalid request kinds fail without
mutation. Live entry kinds and storage validity are caller invariants. A request
may alias a live entry; enqueue copies its value before changing storage.

World admission, static recursion state, height queries, chunk insertion,
finishing dirty effects, queue draining and light propagation remain separate
work. See the [original evidence](../reference/lighting.md) and
[parity matrix](../parity.md).
