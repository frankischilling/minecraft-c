# Native comparison boundaries

The packet contains original observations. Its capture and processing receipts
describe that source-only checkpoint. Later native execution has its separate
scope and evidence in [Skeleton firing](../../../../docs/entity/skeleton-firing.md).

Row 16 prepares an actual null WorldAccess occurrence before the recording
observer. The current C registration code accepts null entries and preserves
their occurrence identities. Its selector returns that null entry in order.
The Entity-coordinate sound body still reads the actual Skeleton x/y/offset/z
and computes y-offset before reaching its WorldAccess invocation at FD53.

At the initial export review, the final C dispatcher returned `MC_WORLD_STORAGE_UNSUPPORTED_STATE`
for a null sound receiver and leaves `mc_world_access_failure` empty.
At that revision, `mc_world_access_failure_source_exception` required a valid
invocation receipt and could not classify the empty result as the original
NullPointerException.
The occurrence and reached prefix were representable, while the complete
Throwable outcome needed implementation. The later actual sound dispatcher
records a value-only null-receiver failure, without inventing a callback owner
or receipt. Profile 16 now matches the original null-reference outcome through
the real FR124 sound call on Windows and sanitized Linux. The original review
JSON and processing receipt remain unchanged as historical evidence.

The reviewed paths are `src/world/runtime/world_access.c` registration,
selection and sound dispatch; `src/world/runtime/movement.c` Entity-coordinate
sound; and `include/mc/world/runtime/world_access_error.h` invocation receipts.
Their exact reviewed byte identities are recorded in the packet's processing
audit. Later implementation changes need a new review before this boundary is
changed.

Row 21 changes the Skeleton's World to B from a real sound callback. The Arrow
keeps constructor World A, while the later source publication uses B. A later
static review corrected the initial boundary's location: the native publication
API explicitly admits an Entity with a different construction World. The
single-World terminal graph collector then rejects an Entity whose current
World lies outside that root. Publication refusal must not be inferred from
this cleanup constraint.

The initial review JSON and its processing receipt retain that earlier
unqualified admission assumption as history. The follow-up review checked
`include/mc/world/runtime/entities.h` and
`src/world/runtime/terminal_inventory.c`. A native fixture needs a legitimate
cross-World retirement join before executing this profile; rebinding the
Arrow's source World for cleanup would alter the observed graph. The current
comparison therefore records profile 21 as explicitly unexecuted, with no
native prefix or outcome credit. Its complete original result remains intact
in all four source runs. Native qualification is tracked separately in
[Skeleton firing](../../../../docs/entity/skeleton-firing.md).

Rows 15/17 throw authored Java exceptions from actual WorldAccess callbacks.
Their genuine FR prefixes and membership changes are useful source evidence.
The helper Throwable class, message, stack and helper Code remain source-only
details. An ordinary native callback refusal must keep its own classification;
it does not become that Java exception.

Ordinary VMs cannot observe unpublished FR locals in rows 12 through 16 and 22. The
additional local-Arrow domain uses only two passive VM witnesses. No ordinary
raw row is filled with debugger or expected state. Source journal events are
source observations; a future native runner need not fabricate an event stream.
