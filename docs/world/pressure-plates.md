# Pressure-plate contact and release

The [pressure update](../../src/world/runtime/mutation/pressure_plate.c) and
[lifecycle hooks](../../src/world/runtime/mutation/pressure_lifecycle.c) implement
the paired pressure-plate bodies on the actual GAME World. The
[source map](../reference/pressure-plates.md) records both original owners and
their distinct contact, tick, private update, neighbor and removal frames.

## Query and transitions

Contact first reads the remote flag. A remote World returns before metadata or
the unused Entity argument. A local contact returns when metadata is exactly
one; otherwise the private helper rereads metadata and queries the World.
Passing an Entity to contact does not publish it. The wooden category uses the
real untyped nearby query with a null exclusion, preserving actual Chunk slice
membership, duplicate occurrences and the shared mutable result List.

Each reached category comparison reloads the receiver field and initializes
the actual category class at its own static read. The comparisons are separate,
so aliased category fields can reach multiple queries. Living and Player use
the real [Class-filtered query](class-entities.md) with fresh result Lists. Each
target literal resolves from the Block's defining classes before the bounds
factory. A later query replaces the selected List only after it returns; the
previous result remains held during that query. An unmatched category
leaves the result List null and fails at its actual size call.

Bounds use the Block's defining Entity classes and the real shared bounds pool.
The X/Z minimum adds `0.125f` after conversion to float. The maximum wraps the
Java integer increment, converts to float and subtracts `0.125f`. These float
results widen to double; Y uses double and a height of `0.25`. Neither a double
shortcut nor an implicit pool reset gives the same behavior. Native holds retain
the actual pooled box and query List through their reached use.

A rising transition writes notifying metadata one, notifies the current cell
and the cell below, marks the cell's observer range and requests positional
`random.click` at volume `0.3f`, pitch `0.6f`. A falling transition writes zero
and follows the same order with pitch `0.5f`. A nonempty query then schedules
the live receiver ID with its virtual delay of twenty ticks. Failures retain
every earlier metadata, pool, query, notification and scheduler effect.

## Lifecycle and scheduling

Addition is the original empty body. A neighbor call tests solid support below;
unsupported plates read metadata, execute the inherited drop path and use the
notifying air setter. Removal reads current metadata and notifies both cells
when it is positive, then invokes the empty inherited removal body. The below
notification can synchronously call the plate's own support hook.

The scheduled-tick body ignores its Random argument. It skips a remote World
or zero metadata and otherwise runs the same private helper. Repeated contact
while metadata is one does not release a plate; this tick body does. The real
World scheduler retains its existing presence, immediate/deferred, record,
membership and ordered-insertion behavior. Immediate reentry has no added
gameplay recursion cap. Queue draining and a running game loop remain separate.

Diagnostics retain separate outer contact/tick and private-helper PCs, actual
bounds and nearby children, and full mutation/scheduling causes. The finite
current/deepest hook projection does not represent an arbitrary source stack.
Native owner, allocation and hold limits remain distinct from Java exceptions.

## Native verification

The `world.pressure_plate` case passes 561 checks per host on MSVC and sanitized
GCC/Clang. It constructs both Registry sides, eight real GAME Worlds and four
serialized loaded chunks per World. Actual Item/Falling publication and duplicate
occurrences drive activation and release. It also checks empty membership,
remote/off/on guards, source float/int wrapping, category aliases, distinct
metadata/query errors, actual Living/Player filtering, aliased result replacement,
literal-resolution failures before the pool, support-loss removal and real
scheduler limits that retain metadata or F membership after a later failure.
Native views, records, Worlds,
Chunks and Entity holds retire through their real owner APIs.

All 171 MSVC and 173 cases per GCC/Clang public suite pass without skips; the
movement regression passes 560 checks per host. The retained capture includes
inputs, complete streams and all thirty-two fixture region slots with their
explicit timestamps. Those regions are fixture inputs, not saved final pressure
state or original observations. See [testing](../testing.md) for the full
configuration and source-copy scope.

Detector-rail helpers, other Entity constructors, nonempty concrete
observer/sound dispatch, complete redstone propagation, tick draining and
original-runtime pressure-plate equality remain unfinished.
