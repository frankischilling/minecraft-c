# Client Timer

The Timer module implements client class `py`'s construction and update arithmetic.
The existing public interface accepts a wall-clock millisecond sample followed
by a raw nanosecond sample. A separate private constructor obtains those clocks
from the native platform in source order. Neither path calls SDL or updates a
World.

Construction retains the tick rate and initial synchronization samples, sets
speed to one and leaves the previous high-resolution seconds at zero. Each
update converts nanoseconds to milliseconds with signed truncation toward zero.
Long additions and differences use Java wrapping arithmetic. A wall delta above
1,000 milliseconds or below zero resets the previous high-resolution seconds
without resetting the accumulated synchronization state.

Accumulated wall time above 1,000 milliseconds updates the drift adjustment using
the source's widened binary32 `0.2f` coefficient. The elapsed seconds product is
clamped to zero through one. Tick rate and speed retain their supplied binary32
values. After accumulating ticks, the module subtracts the uncapped Java
float-to-int result from the float remainder, then caps the published positive
tick count at ten. Negative and nonfinite inputs retain their source arithmetic;
the module does not replace them with defaults.

`mc_timer_create` owns one allocation, retired by `mc_timer_free`. Updates
allocate nothing. `mc_timer_view_get` returns the tick count, partial ticks,
tick rate and speed without advancing the owner. Paused render-partial
preservation belongs to the caller, as in the original client loop.
The actual render-partial field `c` has storage distinct from accumulator `e`.
Both start at zero; the update copies `e` to `c` at its final source store.

The private ordered constructor first allocates a zeroed candidate, then writes
speed, remainder, adjustment and rate. It samples wall time at source25 and
stores it at28 before reaching native nanotime initialization/sample at32.
The native floating-point prerequisite occurs there. A failed wall call never
reads the later clock, and a nanotime failure retains the completed wall store.
Failed candidates remain inspectable, cannot retry construction and reject
ordinary updates. Retirement allocates nothing and consumes the unique owner.
The supplied-clock wrapper retains its existing signature and ownership rules.

The arithmetic requires IEEE binary32/binary64, nearest-even rounding, gradual
underflow and disabled floating traps, without reassociation, excess precision
or fused operations. The owner does not alter floating controls. NaN payloads,
floating exception flags and identical OS clock sampling times are outside this
interface's contract.

The public `client.timer` test uses authored clock sequences and literal
source-derived outcomes. It covers fractions, stalls, backward clocks, signed
long wrapping, drift correction, the ten-tick cap, negative/nonfinite rates and
speeds, and output preservation. Its 96 checks pass on MSVC and sanitized
GCC/Clang. The separate ordered-constructor qualification passes 1,267 authored
checks on each compiler, alongside the existing Timer, clock and conversion
controls. Fresh original constructors cover eight rates and all ten fields on
Windows and Linux. Clock fields use their actual host observation windows;
the reflected nanosecond quotient does not reveal the exact raw sample.

The combined 393/385/385 source also passes these constructor and existing
arithmetic contracts. The original observations retain their selected CPU
qualification scope. Application publication, pause handling and full client
timing remain unfinished. See
the [client startup scope](startup.md) for integration limits.
