# Random constructor recording and replay

The private profile interface in
[src/core/java/random_profile_internal.h](../../src/core/java/random_profile_internal.h)
can record actual default-constructor inputs or replay one complete declared
sequence. Ordinary calls still commit the shared full-width uniquifier, take
one native clock sample and run the same constructor tail. The
[profile and clock contract](../reference/random-profile.md) describes that path.

Bind once to a fresh untouched profile, before any default constructor or Math
initialization. Recording and replay are mutually exclusive. The caller owns
the typed record array or immutable input sequence and keeps it aligned, live
and disjoint from the profile, Random outputs and other users until profile
retirement. These arrays do not count toward the profile's allocation budget.
That budget covers the actual profile and optional retained Math owner sizes.

A successful recorded row contains the actual committed U and signed nanoTime
return, constructor kind, numeric destination identities and sample/tail return
flags. A failed sample has no valid nanoTime. Ordinary construction and the
first Math-owner construction use the same recorder; later ready-Math draws
add no row. Numeric identities can expire and grant no pointer, caller-role,
thread or object-generation rights.

Binding, row inspection, completion and destruction require external lifecycle
quiescence. Join every actual borrower and wait for dependent owners and Math
callers to drain. An inactive recorder flag alone does not establish this.
The observer gate is nonblocking: an overlapping call records CONCURRENT and
continues its real constructor without a row. Full capacity records OVERFLOW.
The first observer error stays latched while later flags accumulate. Neither
error changes the constructor's own status or output, so a successful Source
call does not prove a complete observation. Zero capacity is a valid recorder
that overflows on the first committed constructor.

Replay consumes supplied U/nanoTime pairs at the same boundary after the real
U commit. It checks the whole declared progression before binding, then requires
exact consumption at completion. Unused input latches EXTRA_INPUTS and returns
RANGE; exhaustion and order failures also remain latched. These native
validation failures are expected controls, not original clock exceptions.
Unused recording capacity is valid. A failed history has no unbind, retry,
resampling, truncation or padding repair.

Bootstrap's private
[configuration and terminal receipt](../../src/client/runtime/bootstrap_random_internal.h)
use that same entered startup body. Config is borrowed through open; the
selected array and terminal output stay live through the retained bootstrap's
final checked close. Failed binding retains a partial bootstrap for ordinary
cleanup. The terminal receipt is written at actual profile retirement after
all dependent owners and workers drain. It reports binding, view and completion
results separately from Source/open/close results and is not rewritten after
that profile is consumed. Successful close alone does not qualify replay.

`core.java_random_profile_inputs` preserves the earlier literal constructor
vectors and adds actual record-to-whole-replay, freshness, capacity, budget and
FP controls. `core.java_random_record_concurrency_source_valid` runs fixed real
thread cohorts and checks returned statuses, full U progression, retained rows
and first-cause behavior. Its exit0 certifies Source/invariant validity. Genuine
overlap coverage requires both `ample_concurrent_qualified` and
`overflow_first_cause_qualified` to be1. A passing CTest with UNQUALIFIED output
supplies no such coverage. The harness does not retry for a different scheduling
outcome. The optional `client.bootstrap_random_admission`
uses real no-window invalid-config and oversized-binding paths when the actual
client runtime target exists; it needs no imported assets or physical monitor.

The forwarding checks use the existing production tail and verify operand
routing and consistency. Separate retained literal vectors check arithmetic.
Supplied or captured constructor inputs establish no original JVM clock,
whole-frame or caller-role equivalence. Evolved Random/Math/work state remains
an output, and the change makes no FPS improvement claim. Positive GUI startup
and full-frame scenarios require separate manual qualification.

The [combined validation](../testing.md#constructor-recording-and-owned-material-lookup)
passes all 820 configured Windows tests and 25 selected Linux Clang sanitizer
tests. Both real thread cohorts observe overlap on each host. Windows also
executes the genuine no-window bootstrap admission path; that runtime target is
absent from the local Linux graph. These results retain the finite scopes above.
