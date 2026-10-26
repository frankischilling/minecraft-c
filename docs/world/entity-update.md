# World Entity updates

`mc_world_runtime_tick_entities` implements the client `fd.g()V` Entity and
TileEntity phases on a genuine CLIENT GAME World. Weather entries run first, followed
by loaded-list `removeAll(D)`, two live deferred-removal loops, the loaded Entity
loop, the captured active TileEntity iterator and the existing deferred-G body.
These are Entity updates. World time, block and weather simulation are separate.

`mc_world_runtime_tick_entity` enters the actual `fd.f(Entity)` wrapper.
`mc_world_runtime_update_entity` exposes `fd.a(Entity,boolean)`. True requires the
actual radius-32 region to be present before backups or ticks; false bypasses
that guard and tick but still performs bookkeeping. The real canonical Player
and Item bodies run through their production APIs. The outer World is separate
from the Entity's nullable construction World.

The source index loops reread their current List at each size/get/remove call.
Removal uses source Entity ID equality in the same class namespace, releases
the first matched actual occurrence, and does not deduplicate IDs. The selected
local stays held across callbacks even if the current index later selects a
different occurrence. WorldAccess removal observes the selected local. Typed
NULL weather entries append normally and fail only when their tick is invoked.

The active TileEntity iterator captures its List identity. `hasNext` reads its
current size; `next` and `remove` check the captured modification count. Chest
inherits the bytecode-proved empty base tick. Piston uses the existing concrete
tick. Spawner executes its real nearest-Player predicate, three Random floats,
smoke/flame calls, float-division rotation and delay logic. A dead Player remains
eligible for nearest selection; radius boundaries are strict and ties retain
the first candidate. The named mob factory at `cy.n_` PC223 remains a reached
dependency. No missing mob is replaced by a successful NULL result.

Source failures keep their prefix. In particular, a failed active TileEntity
iteration leaves `L=true`; only normal active completion stores false and enters
G. Nonfinite Entity repair restores the live backup fields in source order,
without repairing bounds or velocity. NaN skips the riding normalization/clamp
branches and Spawner rotation subtraction. Positive/negative infinities that
cannot progress through a source loop can reach the native work ceiling.

Item riding uses the actual inherited `sn.s_` body and proven pose dispatch for
Item, FallingSand, Minecart and canonical LocalPlayer owners. Raw mount/passenger
links still have no general association owner. Callers declare independently
held actors in a stable `relationship_actors` array through the whole call and
reentry. The receiver and retained ancestors also prove link lifetime. Unknown
nonnull links fail before dereference. Passenger continuation uses a tail loop
with separately retained occurrences, including duplicates and cycles, held
until reverse-order retirement. It does not own both directions of a cycle.

Other World source owners fail native admission before source effects; this is
not a Java exception. Actual SERVER-owner tests verify unchanged Entity/World
state and earlier diagnostics.

The context borrows the existing Registry/trig/Player policies. Empty real Lists
do not initialize these classes or require an irrelevant context. Each attempt
owns fixed private heap diagnostics and publishes the caller's error only at
return. Large failed children use a tagged union. Loop, passenger-call and
peak temporary-allocation limits stop at pending work and retain all earlier
effects; they never turn truncation into success. No implicit bounds/vector
pool reset occurs.

FallingSand and Minecart active ticks, non-Item riding wrappers/statistics,
other concrete active Entity descendants and the dedicated server override
remain explicit dependencies. The existing wrong-location Chunk diagnostic is
also a reached dependency for extreme packed-key aliases. Ordinary finite
client paths are qualified; no server gameplay, entire World loop, frontend
integration or playable client is claimed here.

The authored native test exercises real World/Store/Registry/Chunk owners,
25-chunk presence, 32 complete World-to-Player updates with real App/Input/stats,
Item physics, mutable removals, equal-ID owners, captured iterators, Piston,
Spawner, deep/cyclic actors and native bounds. The unchanged-original CPU harness
compares 54 ordered records: 37 states (35 complete and two explicit exception
prefixes) and 17 removal/particle events. Seven riding states include actual
FallingSand/Minecart mounts and NaN; seven TileEntity states include no-nearby,
remote, delay reset, enabled NaN and null-World order. Native policy frontiers
are reported separately from complete original executions.
