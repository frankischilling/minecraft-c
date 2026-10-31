# Shared collision bounds

[`mc/entity/bounds/bounds.h`](../../include/mc/entity/bounds/bounds.h) implements
the client `eq` / server `cz` standalone factory, shared pool and complete
movement geometry helpers. The [source map](../reference/bounds.md) records the
paired bodies and the remaining vector, ray and text operations. This module
does not execute Entity movement or FallingSand physics.

## Identity and lifetime

The `mc_entity_classes` owner contains one bounds class and pool for its loaded
game-class identity. It spans Worlds and Entity subtypes. Bounds active use
initializes only the bounds class, publishing its empty pool List and zero cursor.
It does not initialize Entity, consume an Entity ID, create a Random or use a
World. Entity construction allocates its own permanent box separately, with the
same constructor effect and failure order.

`mc_entity_bounds_create` returns a fresh standalone box with one native hold.
It stores the supplied min X/Y/Z and max X/Y/Z coordinates in that order, without
arithmetic, endpoint sorting or finite-value checks. Signed zeros and NaN bits
remain unchanged. After any required class initialization, the factory neither
appends to the pool nor advances its cursor. Pool reset, reuse and client clear
leave the standalone box's identity and coordinates unchanged.

Retain or release this box through its original classes owner. Its last native
release frees it; classes destruction returns BUSY while any bounds hold remains.
Keep the classes owner live and access it exclusively. Output and error storage
must be disjoint from each other and from owner storage.

A pooled factory call appends one zero box only when the cursor is at least
the current List length. It then increments the Java int cursor before getting
the indexed box and setting its six coordinates. A failed get retains that
increment and any completed append. It does not fill a cursor gap or repair a
failed source prefix.

Every successful pooled get or transform returns one native hold. Release that
hold through the same classes owner. Reset sets only the source cursor to zero;
it keeps boxes, capacity and coordinates. Subsequent gets reuse those same
identities and overwrite coordinates even if an earlier caller still holds
the box. A native hold protects storage, not a snapshot of its values.

The client-only clear drops the pool's references, keeps backing capacity and
then resets the cursor. Held detached boxes remain live until their last native
release. A server clear request is an invalid native operation because `cz` has
no such method. Classes destruction returns BUSY while any bounds hold remains.
No helper resets or clears the pool implicitly.

The original client resets bounds once per outer frame before its queued ticks
and rendering. The dedicated server resets once per complete server tick before
its Worlds. Client clear belongs to its error recovery method. Those application
routes are source-mapped; the native game loop does not yet execute them.

## Numerical bodies

The module implements directional sweep, symmetric expansion, shifted copy,
symmetric contraction, pooled copy, X/Y/Z offset clamps, strict box intersection,
same-object translation and same-object coordinate assignment. Translation returns
its original receiver. The native assignment convenience returns the target after
executing the original void copy body.

Sweep extends each minimum only for a negative delta and each maximum only for
a positive delta. Either signed zero and NaN leave that axis unchanged. Expand
subtracts deltas from minima and adds them to maxima; contract does the reverse.
Contract may invert a box. No method sorts endpoints, adds an epsilon or requires
finite values.

Pooled transformations compute all six coordinates before entering the factory.
Reset can therefore make a transform overwrite its own source slot without
corrupting the remaining input coordinates. Same-object translation and assignment
perform their six stores in min X/Y/Z, max X/Y/Z order.

Clamps return the supplied delta when either other axis is separated or merely
touching. Otherwise they reduce a motion component only when its directional gap
is smaller than the requested move. Already-overlapping boxes are not separated
by a correction. The original comparisons reject `max <= min` or `min >= max`;
rewriting them as positive overlap conjunctions changes NaN behavior. Intersection
likewise rejects touching faces but can accept NaN coordinates through unordered
comparisons. Signed zero is retained by plain assignment and by untouched sweep
axes. Arithmetic uses the repository's strict binary64 build contract.

For an addition with two NaN operands, the captured x86-64 interpreter selects
the quieted right operand, retaining its sign and payload. Strict C compiler
flags alone did not preserve that choice: the first comparison found payload
differences on all three compilers. A local compatibility operation handles
that case explicitly. Ordinary arithmetic, subtraction, assigned bits and
unordered branch behavior keep their separate source rules. Host floating
exception flags are outside this numerical interface.

## Native limits and errors

The classes ledger charges each actual standalone or pooled wrapper, the pool
descriptor, backing pointer capacity and replacement allocation while the old
backing array still exists. An append failure retires only its unpublished new
child, leaving earlier source effects intact. A failure during the bounds class
initializer latches its FAILED phase.

A standalone factory allocation or native return-hold failure preserves completed
class initialization without poisoning the class. A failed return hold retires
only the unpublished new wrapper. Factory failures leave the output pointer
unchanged. Their errors retain the operation and reached source PC, including
the actual initializer or allocation child when reached. The return-hold limit
is a native ownership failure reported at return17, not a Java exception.

Pooled operations also preserve completed append, cursor and coordinate effects
on failure. Transform errors retain the reached pooled-factory frame and its
actual initializer/direct-factory/append child. Retain/release accept only live
standalone, pooled or detached boxes from the same owner; they reject foreign
boxes, caller-owned values and Entity's permanent box.

Pool acquisition is constant time except backing growth. Public native retain
and release validate identity by scanning the owner's live standalone, pooled
and detached objects; a large batch of external releases can have quadratic cost.
This lifetime policy and native allocation budget are not claims of identical
JDK garbage collection or allocation-failure behavior.

Ray/segment intersection, Vec3 and HitResult pools, point containment through a
real vector, client mean-edge length and Java text conversion remain unfinished.
World collision Lists and concrete Block shapes have their own
[collision contract](../world/collision/boxes.md). Current Item/Falling movement
and environment effects have a separate [movement contract](movement.md).
FallingSand landing/drop and full original-game replay remain unfinished.

## Verification

Focused MSVC and sanitized GCC/Clang `entity.bounds` runs pass 4,751 native checks.
Standalone factory coverage includes both class owners, raw coordinate stores, fresh
identity, pool independence, native ownership and retained failure prefixes.
This focused result does not establish a new full-suite pass or an original-game
comparison of the standalone factory.

The historical bounds baseline passed 4,601 checks on MSVC and sanitized
GCC/Clang, including identity/reuse/clear, actual growth-budget peaks, retained
failure prefixes and an exact eight-by-eight quiet/signaling NaN matrix on both
class owners. Its 160/162/162 configured cases passed without skips. The probes
from that baseline matched all 17,388 client and 16,146 server original raw rows
in the retained helper corpus. The public reproduction command remains in the
[source map](../reference/bounds.md).

Independent audits of that historical baseline checked every case, retained
stream and compiled-product join. Its selected native graph pinned 749 inputs,
254 dependency files, 220 products and 40 captures per host. Those results
qualify that helper corpus and its native ownership behavior; they do not extend
to the standalone factory addition or the unfinished game routes above.
