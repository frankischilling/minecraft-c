# Shared collision bounds

[`mc/entity/bounds/bounds.h`](../../include/mc/entity/bounds/bounds.h) implements
the shared client `eq` / server `cz` pool and their complete movement geometry
helpers. The [source map](../reference/bounds.md) records the paired bodies and
the remaining vector, ray and text operations. This module does not execute
Entity movement or FallingSand physics.

## Identity and lifetime

The existing `mc_entity_classes` owner contains one bounds pool for its loaded
game-class identity. It spans Worlds and Entity subtypes. Bounds active use
initializes only that pool: it does not initialize Entity, consume an Entity ID,
create a Random or use a World. Entity construction continues to allocate its
own permanent box separately, with the same constructor effect and failure order.

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

The classes ledger charges each actual native wrapper, backing pointer capacity
and replacement allocation while the old backing array still exists. An append
failure retires only its unpublished new child, leaving earlier source effects
intact. Ordinary allocation failure does not poison the bounds class. A failure
during its actual initializer latches the existing FAILED phase.

Errors retain the operation and reached source PC, the pooled-factory frame under
a transform, and the actual initializer/direct-factory/append child when reached.
Outputs remain unchanged on failure. Caller-owned numerical values and Entity's
permanent box cannot be adopted by native pool retain/release.

Pool acquisition is constant time except backing growth. Public native retain
and release validate identity by scanning the owner's live pooled and detached
objects; a large batch of external releases can therefore have quadratic cost.
This lifetime policy and native allocation budget are not claims of identical
JDK garbage collection or allocation-failure behavior.

Ray/segment intersection, Vec3 and HitResult pools, point containment through a
real vector, client mean-edge length and Java text conversion remain unfinished.
World collision Lists and concrete Block shapes have their own
[collision contract](../world/collision/boxes.md). Current Item/Falling movement
and environment effects have a separate [movement contract](movement.md).
FallingSand landing/drop and full original-game replay remain unfinished.

## Verification

`entity.bounds` passes 4,601 checks on MSVC and sanitized GCC/Clang, including
identity/reuse/clear, actual growth-budget peaks, retained failure prefixes and
an exact eight-by-eight quiet/signaling NaN matrix on both class owners.
All 160/162/162 configured final-source cases pass without skips. Each exact
final probe also matches all 17,388 client and 16,146 server original raw rows;
the public reproduction command is in the [source map](../reference/bounds.md).
Independent audits check every case, retained stream and compiled-product join.
The complete selected native graph pins 749 inputs, 254 dependency files,
220 products and 40 captures per host. These checks qualify the specified
helpers and native ownership, not the unfinished game routes above.
