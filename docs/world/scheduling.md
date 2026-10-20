# Scheduled update state

The [entry API](../../include/mc/world/schedule/entry.h) owns one stable scheduled
record: full signed X/Y/Z/block values, signed due time and a signed sequence.
Construction allocates before consuming the shared sequence, advances that
sequence with Java long wrapping, then stores its old value in the record. The
GAME World class owner shares the lazily initialized sequence across its Worlds;
constructing another World does not reset it.

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

## World insertion

The [GAME scheduling entry](../../include/mc/world/runtime/scheduling.h) implements
client `fd.c(IIIII)V` and base-server `dj.c(IIIII)V`. Dedicated `dp` inherits this
method; its same-named shadow fields are not the base E/F owners. This mapping
does not qualify a dedicated-server runtime or its provider startup.

The operation constructs a fresh `qy`/`ki` record before testing current World.a
or the nonloading radius-eight presence box. Ordinary absence, duplicate keys and
immediate no-ops still consume the shared sequence. Signed coordinate arithmetic
wraps before the region test; it does not add a block-access border preflight.

With a false immediate flag, presence success permits insertion. Positive block
IDs first receive current Info time plus the signed delay with long wrapping.
Nonpositive IDs retain default due time zero without reading Info. F.contains,
F.add and E.add use the same record at separate source boundaries. An equal key
keeps the old owner/time/sequence even when the new delay differs. Ordinary false
from either add is discarded; failed E insertion retains successful F insertion.

With a true immediate flag, the operation instead acquires the actual current
block, compares its full ID, requires it positive, captures live Block.m and
invokes the selected tick with current World.r. It does not read delay, Info or
E/F on this branch. The fresh record remains held through synchronous calls.
Base/inherited empty tick bodies return normally, including with a null Random
that is never dereferenced. Reached concrete bodies without an implementation
fail explicitly at their actual entry. [Flowing and still bodies](fluids.md)
now execute through the same dispatch. This does not complete fluid/falling,
plant, fire, drop, entity or other synchronous gameplay descendants.

The presence body is shared with log/leaf removal. It visits chunk X then Z,
reloading current provider v for each nonacquiring contains call. Its early Y
rejection and empty/reversed ranges remain distinct from normal block bounds.
Neither insertion nor this helper drains queues or performs a later tick.

The native diagnostic exposes the separate scheduled-class initialization latch
and next sequence without active use. The World and local record remain held
through resource callbacks and nested calls. Failure reports separate World,
presence/provider, collection, Registry and concrete-tick frames and preserves
all reached effects. Native admission leaves earlier World diagnostics intact.

F/E retain their separate entry and allocation limits. Checked record allocation
also applies to discarded requests and locals held through reentry. The World
constructor's direct-byte counter is not a combined dynamic scheduler budget.
These policies do not reproduce Java allocation/exception timing or introduce a
global recursion/work limit. Explicit [native retirement](construction.md) keeps
E/F inspectable, including an F-only prefix, until final free releases only their
owned holds. It does not repair queues or execute pending game work.

World tick processing, queue persistence and complete original/native scheduling
comparison remain unfinished. The source bodies above establish ordering; they
are separate from the collection replay and finite native contracts below.

## Native validation

`world.scheduling` passes 424 checks on native Windows MSVC and Linux GCC/Clang,
with Linux address, leak and undefined-behavior sanitizers. All 60 selected cases
pass on each compiler, including the existing GAME construction, mutation,
Registry, collection and persistence contracts. Each host retains the same 624
selected inputs and its 51 actual products.

The new case uses seven genuinely constructed client Worlds, three class owners,
real Registry bindings and twelve loaded known-empty records. It checks lazy
shared sequence state, duplicates, full IDs/coordinates, signed time wrapping,
ordinary absence, reversed presence ranges, immediate empty ticks with null
Random, reached Registry/concrete-tick failures, separate F/E limits, F-only
failure effects and retained ownership through retirement. A temporary actual
RAW Info owner checks that scheduling reads the time field without a constructor
phase gate. Synthetic sequence wrap and explicit receiver-field controls remain
native experiments; they are not original-game observations.

Final maintenance saves all twelve records using the actual GAME clock, verifies
their region timestamps against the child execution interval and unloads the
actual owners. This covers the test's known-empty loaded route. It does not
qualify scheduled-state serialization or complete save interoperability.

Reproduce with `ctest --test-dir build -R "^world.scheduling$" --output-on-failure`.
The normal driver retains inputs, outputs and fresh owned save files. Runtime
cases in this new unit use client Worlds; paired source establishes the
base-server mapping, without a new native server scheduling qualification.

## Original observations

The project-owned [observer](../../tests/reference/world/SchedulingObservation.java)
uses the unchanged [mutation observer's](../../tests/reference/world/MutationObservation.java)
genuine World construction and context reader. Separate fresh client and base
server experiments each completed 18 direct scheduling calls with natural zero
exit. Each retained transcript contains 39 complete context objects, full E/F
iteration and record identities, actual outcomes and flag-restoration receipts.
All 18 framing controls reject per side. The initial static sequence remains
unobserved until a real scheduling call returns normally.

Both observed queues finish with eight records. Duplicate scheduling retains the
old record while consuming another sequence value. Wrapped presence ranges
admit full coordinate and block-ID keys; absent and immediate no-op calls still
consume sequence values. These are observations of the recorded calls, rather
than a universal scheduling result. The optional present-stone tick was skipped
on both sides because neither genuine setup supplied a suitable footprint.

No original class, queue, time or Random state is replaced. Explicit a/o/B inputs
are restored after recording the source effects. Constructor internals and
inspector effects outside the measured World state remain uninstrumented. These
captures have no native replay, helper-free control equivalence, dedicated route,
population, tick-draining or save-interoperability claim. See the
[reference evidence and reproduction](../reference/state-records.md).

## Collection validation

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
