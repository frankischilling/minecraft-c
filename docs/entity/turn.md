# Entity turning

`mc_entity_turn` implements the original client base Entity `sn.d(FF)V`
operation on an existing, held, READY native Entity. The declaration is in
[turn.h](../../include/mc/entity/movement/turn.h). It changes current and
previous yaw/pitch on the same owner; it does not construct or substitute a
Player, move the Entity, or supply mouse/camera integration.

## Original operation

The source saves old pitch and yaw, then executes these stores in order:

| Store | Original field | Native field | Bytecode PC |
| --- | --- | --- | ---: |
| Scaled yaw addition | `aS` | `base.yaw` | 25 |
| Scaled pitch subtraction | `aT` | `base.aT` | 42 |
| Optional lower pitch clamp | `aT` | `base.aT` | 58 |
| Optional upper pitch clamp | `aT` | `base.aT` | 74 |
| Previous pitch adjustment | `aV` | `base.aV` | 89 |
| Previous yaw adjustment | `aU` | `base.aU` | 105 |

Current angles and deltas widen separately to double. Each delta multiplies
by double `0.15` (bits `3fc3333333333333`), then yaw adds and pitch subtracts
before each result narrows to float. Pitch comparisons clamp below -90 or
above 90. Previous angles add the actual float current-minus-old difference,
with a separate float rounding at subtraction and addition. Yaw has no wrap
or clamp, and previous angles are not replaced by the old current angles.

For example, a zero-angle Entity turned by `(3,3)` reaches yaw bits
`3ee66666` and pitch bits `bee66666`. Multiplication by float `.15f` would
produce a different yaw bit pattern. Starting at pitch 89 with pitch delta -10
clamps to 90 and adjusts previous pitch by 1, rather than by the unclamped 1.5.

## Floating behavior and ownership

The operation admits signed zero, subnormals, finite extremes, infinities and
NaNs. SSE2 widening, double arithmetic, narrowing and float arithmetic retain
the captured Java evaluation, including overflow to infinity without an
out-of-range C cast. NaN pitch comparisons skip both clamps. Infinite delta
can produce an infinite yaw while pitch clamps to the appropriate endpoint;
infinity-minus-infinity in a previous-angle adjustment produces NaN.

The existing strict x86-64/SSE2 environment is required: nearest-even,
gradual underflow, masked traps, no FTZ/DAZ, reassociation, excess intermediate
precision or fused operations. The native MXCSR check rejects unsupported
controls before any field store. Arithmetic status flags may change; they
are not part of the preserved Entity state.

Native null/phase errors also precede mutation. READY admission follows the
existing direct Entity mutation contract; it is not an original Java phase
check. The caller keeps the real Entity/class owner live and exclusive.
There is no allocation, hold change, class initialization, bounds mutation,
World read, watcher change, Random draw, tick or callback. Constructor progress
diagnostics remain unchanged.

## Verification

[Native tests](../../tests/entity/movement/test_turn.c) pass 137 checks per
compiler on MSVC 19.51, GCC 13.3 and Clang 18.1.3. Linux uses ASan, UBSan and leak
checks. Tests construct actual Item/Falling owners through the production
allocation and null-World constructors. Only their four original public angle
fields receive explicit observation inputs. No READY phase, class identity,
ID, hold, child owner or constructor entropy is fabricated.

The [unchanged-original comparison](../../tests/reference/entity/turn/README.md)
matches all 3,330 records and 16,650 projected fields per compiler across 3,280
cases. It includes both actual Item/Falling receivers, a special-value grid,
authored binary32 bit patterns and successive turns on the same owner.
Comparison retains raw float bits, including the observed NaN bits; no NaN
normalization or numeric tolerance is used. Each observer separately checks
unrelated source fields, bounds identity/values and private Random state.

Original JAR/class/method pins are in
[provenance.json](../../tests/reference/entity/turn/provenance.json), and the
finite native/compiler/source scope is in
[validation.json](../../tests/reference/entity/turn/validation.json). The new
turn code and observers were initially compiled in isolated projects against
retained, hashed native construction libraries. The integrated MSVC and
sanitized Clang/GCC products also pass the native and unchanged-original
comparisons through the root build graph.

These measurements cover the client inherited body. Dedicated-server turning
correspondence, actual Player construction, settings/sensitivity/invert/mouse
capture, frame scheduling and camera composition remain separate work.
