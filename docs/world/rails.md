# Rail topology, lifecycle and detection

Rail hooks run through the actual [World mutations](mutations.md),
[contact interface](../../include/mc/world/runtime/contact.h) and concrete
tick dispatch. The [source map](../reference/rails.md) identifies all thirteen
connection-helper methods, ten shapes, inherited lifecycle, detector and
powered-search bodies. Source coverage is complete for those bodies; native
contract results are recorded below. Executed-original rail equality remains
unverified.

## Real topology and synchronous writes

The [connection helper](../../src/world/runtime/mutation/rails/connections.c)
owns a private List of coordinate occurrences. Construction reads the actual
World ID and metadata before capturing defining-Registry Block.m and selecting
the rail flag. The retained outer rail reference is nullable and unused by
these reads. The defining code owner remains independent of the supplied
World's Registry.

Resolving a neighbor tries its same height, then above, then below, constructing
a fresh helper at the first rail found. Membership compares X/Z only. Pruning
removes nonreciprocal links or replaces a coordinate with the resolved height.
Admission accepts an existing link before rejecting a new link at exactly
size2; it adds no general height rejection. Physical degree separately counts
the four horizontal directions through three-level presence.

Reshape follows the source's ordered straight, curve and slope decisions.
Powered and unpowered ambiguous junctions have different priorities, with
later eligible assignments winning. Flagged rails skip curves and preserve
the bit8 value read at their later metadata access. The rebuilt List survives
an equal nonforced result. Forced reshape still notifies, and reconnect always
notifies even on equality. These calls use the real notifying metadata setter;
observer and neighbor callbacks run synchronously before the helper continues.

The [added hook](../../src/world/runtime/mutation/rails/lifecycle.c) checks
the remote flag, builds a helper, asks World power and forces reshape. Neighbor
first checks metadata and support, including the applicable slope support.
Support loss calls the real inherited drop before notifying air. Supported
rails compare their saved receiver ID with the late live powered-rail singleton
ID. Powered rails ask current/above power and both directed searches in order.
Ordinary rails react to a positive source ID only after the selected source's
capability call and their own flag check; degree3 triggers a fresh shape pass.
Live receiver IDs are reread before later notifications.

The [powered search](../../src/world/runtime/mutation/rails/power_search.c)
follows actual straight/sloped coordinates and bit8 metadata. Starting at0,
the source depth>=8 branch returns false before a World getter. A verifier
reads the supplied World's ID before the defining owner's live U singleton,
then metadata, axis compatibility and actual power. Failed queries preserve
ordinary output and earlier effects. No constant-ID predicate, cached topology,
quiet setter or extra traversal cap replaces this flow.

## Detector queries and release

Detector contact ignores its Entity argument. A remote World returns; a local
contact with bit8 already set returns after its metadata read. Tick ignores
its Random argument and uses the opposite bit guard. Both pass their captured
metadata into the [private update](../../src/world/runtime/mutation/rails/detector.c).

Update resolves the actual Minecart Class from the Block's defining Entity
classes before floating arithmetic or bounds allocation. It obtains a real
pooled box using Java float conversions and wrapped upper coordinates, then
calls the [Class-filtered query](class-entities.md). The result is a fresh List
over actual Chunk membership, independent of World.R. Rejected genuine
Item/Falling occurrences provide supported negative queries; an empty result
comes from that traversal rather than a supplied empty-list substitute.

The captured result and saved bit8 choose the transition, even if synchronous
metadata callbacks change the live cell. Rising writes saved metadata OR8;
falling writes saved metadata AND7. Current and below notifications follow,
then a single-cell observer range. A nonempty captured List schedules the
late live receiver ID with delay20 through the existing real scheduler,
including an already-on update. Immediate/deferred scheduling and partial
membership/ordered insertion retain their existing behavior. Neither contact
nor this helper publishes the Entity passed to contact.

The pooled box and result List remain held through the reached work and are
released on every return. There is no implicit pool reset. The
[Minecart constructors](../entity/minecart.md) now supply real positioned
instances for normal publication and positive Class queries. The separate
[Minecart validation](../reference/minecart-construction.md#native-validation)
passes construction, publication and detector contracts on all three native
hosts. These finite cases cover positive detection and scheduling; gameplay
rail physics remains unfinished.

## Ownership, limits and diagnostics

Callers keep the actual Block, defining Registry and reached GAME World live
and exclusive. Internal held bodies use their caller's World entry; public
contact owns its nonnull World entry. Reached public query, setter and scheduler
calls retain their own normal entry bookkeeping. Unused nullable arguments remain unused.
For example, a valid search receiver with null World and depth>=8 returns
ordinary false before reading World; a direct verifier reaches its getter.
Native admission and an entered source PC0 remain distinct.

Helpers, List storage and coordinate objects use the World's direct native
allocation budget, including overlapping old/new backing during growth.
Temporary helpers retire before their World/Registry lifetime ends. These
native limits and storage choices do not reproduce Java allocation-failure or
garbage-collection timing. Failed construction disposes its private owners;
failure on a surviving helper retains its earlier private mutations. Actual
World bytes, metadata, acquisitions, class initialization, entropy, pool cursor,
notifications, lighting records and scheduler prefixes are never rolled back.

The public [rail sites](../../include/mc/world/runtime/rails/errors.h) accompany
the complete [mutation error](../../include/mc/world/runtime/mutations.h):

| Channel | Current sites | Deepest site |
| --- | --- | --- |
| Detector | Contact, tick, update | Detector cause |
| Lifecycle | Added, shape, neighbor | Lifecycle cause |
| Connections | Helper method | Connection cause |
| Predicate | Presence or ID predicate | Predicate cause |
| Accessor | Rail flag accessor | Accessor cause |
| Powered search | Search, verifier | Search cause |

These seventeen slots use explicit selection bits independent of `entered`,
class names and PC values. Selected-but-unentered invocations and entered PC0
are representable. A selected lower deepest cause wins; otherwise the caller
explicitly selects a lower current or outer fallback. Restoring named outer
currents preserves complete lower power, support, reader, Registry, mutation
and scheduling children. Generic hook fields stay separate. The flat record
describes current/deepest contexts, not an arbitrary call stack or work limit.
Fresh success errors do not erase older World diagnostic history.

## Verification scope

The earlier rail checkpoint passed 814 checks across 144 literal cases on each
host, covering graph/search/lifecycle contracts with literal Chunk layouts,
ordinary notification flow, namespace/order failures, source depth guards
and native allocation cleanup. The separate pure transport fixture passes
1,444 checks across 94 constructed error cases, including all sites, PC0,
unentered selections, aliases and rejected plans.

All 175 configured MSVC tests and 177 tests on each sanitized GCC/Clang host
ran and passed without failures, errors or skips. Linux uses ASan, UBSan and
leak checks; controller mirrors and lifecycle intervals also pass. Snow,
movement and power regressions pass 392, 564 and 7,593 checks per host.
The [source-map record](../reference/rails.md#primary-evidence-and-validation-boundary)
binds the 977-file tested source, held public baseline and closed receipts.
[Testing](../testing.md) describes the strict-profile and source evidence.
These results remain finite native contracts over constructed inputs.

The later [Minecart validation](../reference/minecart-construction.md#native-validation)
passes 196 detector checks across 24 literal cases on each host within full
177/179/179 MSVC/GCC/Clang D suites, with zero failures, errors or skips. It
covers real published membership, positive contact, duplicate scheduling,
deactivation and retained failure prefixes. Gameplay rail physics, ordinary
positive rail-drop publication, scheduler draining, rendering and
original-runtime rail equality remain unverified. The native runtime and
public contract tests do not require a JVM or original game archives.
