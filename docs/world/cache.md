# Synchronous chunk acquisition

`mc/world/cache/acquire.h` composes the existing packed-key cache with explicit
loader, generator, activation and population callbacks. The coordinator owns no
storage or game objects. Callers supply stable cache entries, an insertion list,
a unique pending-key set, a fallback identity and live world values. Callback
owners are borrowed opaque pointers. There is no file I/O or hidden allocator.

The ordinary entry `mc_chunk_acquire` removes the requested pending key before
lookup, including on a hit. On a miss it tries the loader, then the optional
provider or retained fallback. A loaded chunk receives the current world time
after the loader returns. Generated and fallback identities keep their time.
Successful generation must supply a finished, nonnull, stable chunk.

Publication replaces or inserts the original requested packed key, appends that
same returned identity, then activates it. A normal activation sets `active`
before tile registration and all eight ordered entity-slice registrations.
Blank activation is a virtual no-op. The coordinator then checks the four live
population anchors in order: current, west, north, northwest. Wrapped coordinate
arithmetic, short-circuit order and repeated presence checks follow the original.
Earlier callbacks may change predicates evaluated later in the same call.

The population helper sets `terrain_populated` before invoking the provider.
A successful provider callback then marks the retained normal target dirty.
Without a provider, only the population flag changes. Blank dirty marking does
nothing. A failing callback retains the flag and all completed visible effects.
The helper passes this same coordinator as the population provider argument.

## Fast entry and dedicated policy

`mc_chunk_acquire_get` returns a cached hit without removing pending state. A
base-policy miss always acquires. A dedicated-policy miss acquires only when the
live world force flag or provider force flag is set. Otherwise it returns the
exact fallback without publication, activation or pending removal.

`mc_chunk_acquire_mark_drop` implements the dedicated pending producer. It uses
Java-wrapped `chunk*16+8-spawn` on each axis and adds the packed key when either
distance lies outside inclusive `[-128,128]`. It does not require a mapped chunk.
The pending set supplies membership and stable array removal; it does not supply
Java HashSet iteration order or an unload schedule.

## Ownership, recursion and failures

Keep the descriptor and callback bindings unchanged through a call and any
recursive calls. Callbacks may change the live borrowed world/cache/pending
state, but must return valid descriptors. Storage, live identities, data and
fallback must remain disjoint and alive. Replaced identities remain retained in
the insertion list. General storage disjointness is a caller precondition.

There is no in-flight placeholder or post-callback lookup substitution. If a
nested acquisition inserts the same packed key, the outer call still publishes
its retained result and appends it after the nested result. Both objects remain
alive. This contract does not assert that injected recursion occurs in gameplay.

Invalid arguments and a known preflight capacity failure preserve all state.
After callbacks, capacity is checked again. `PARTIAL_LIMIT` reports completed
callback effects and pending removal; there is no rollback. Callback failures
and invalid downstream state can also leave published or partially activated
objects. The output pointer is assigned only on success. Callers must handle
these statuses rather than treating partial state as a completed acquisition.

Present-null imported map values fail entry validation with `INVALID_ARGUMENT`.
A successful loader or generator returning null produces `STATE_ERROR` before
publication. The original can publish a null generator result before throwing;
that failure domain is outside the nonnull cache contract. Caught loader errors
continue to generation, with diagnostics owned by the loader. Fatal loader
failures stop.

`world.chunk_acquire` exercises 838 constructed checks on MSVC, GCC and Clang,
including callback order, loaded-time sampling, live population gates, blank
methods, aliases, wrapping, recursion and failure preservation. Independent
bytecode review matches all 231 acquisition instructions across client `ok`,
base server `jb` and dedicated `he`. Public contract checks and genuine original
observations remain separate evidence. Production comparisons of this combined
coordinator and retained normal generation now have a three-context production
oracle and lossless public replay. Their [reference evidence](../reference/cache.md)
separates genuine observations from injected callback tests. At source
`9a50ac67997082c89dbc43cf4274fba1427c1565`, all 52 selected local tests pass on
MSVC/GCC/Clang without skips, with Linux sanitizers, independent exact-source
audit and hosted checks. A separate [callback boundary comparison](../reference/cache-callbacks.md)
adds successful original loads, nonzero time stamping and naturally triggered
population. It replays complete external callback effects while C computes
coordinator transitions. Its final exact-source platform acceptance is pending.

This boundary does not decode disk chunks, populate terrain features, save or
unload objects, drain light queues or reproduce Java collection iteration.
Read-only [region inspection](../storage/region.md), unpublished
[normal generation](generation.md) and [full finishing](finishing.md) retain
their own contracts and evidence.
