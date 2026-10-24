# Client Timer

The Timer module implements client class `py`'s constructor and update arithmetic.
Its interface takes a wall-clock millisecond sample followed by a raw nanosecond
sample. The application supplies those samples in that order. Timer performs no
OS calls, SDL calls or World updates.

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

The arithmetic requires IEEE binary32/binary64, nearest-even rounding, gradual
underflow and disabled floating traps, without reassociation, excess precision
or fused operations. The owner does not alter floating controls. NaN payloads,
floating exception flags and identical OS clock sampling times are outside this
interface's contract.

The public `client.timer` test uses authored clock sequences and literal
source-derived outcomes. It covers fractions, stalls, backward clocks, signed
long wrapping, drift correction, the ten-tick cap, negative/nonfinite rates and
speeds, and output preservation. Its 96 checks pass on MSVC and sanitized
GCC/Clang. These are
native arithmetic tests; original Timer execution and full client timing remain
unqualified. See the [client startup scope](startup.md) for integration limits.
