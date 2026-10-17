# Float lookup and double floor

`mc_core` provides the three numerical helpers used by normal cave geometry in
`src/core/math/math.c`, with the public interface in
`include/mc/core/math/math.h`. The [reference notes](../reference/math.md)
identify the original methods and exact observations.

`mc_math_trig` contains 65536 floats, occupying 256 KiB on the supported x86-64
targets. The caller supplies its storage in static memory, a suitably sized
allocation or another owned object. `mc_math_trig_init` overwrites every entry
and allocates nothing. A null pointer returns false; all valid objects finish
initialization. Arbitrary prior object contents are accepted.

Initialization requires exclusive access. Afterward, share the table through
const pointers and keep its values unchanged. There is no shared global state,
implicit initialization, allocation budget or destruction operation. Initialize
once for consumers that share the same table; reinitialization also requires
exclusive access.

The initializer preserves the original ordered double-angle expression,
evaluates double `sin` and narrows each result to binary32. Every resulting
table bit must agree with the original observations, including the nonzero
half-turn residue. The selected system math backends have been measured on
MSVC/GCC/Clang x86-64. A new backend needs the complete table comparison before
receiving the same compatibility claim. The CMake target links the system math
library on Unix. Runtime use requires neither Java nor original files.

`mc_math_sin` and `mc_math_cos` require a nonnull initialized, unmodified table.
They perform the original float multiplication, Java float-to-int conversion
and 16-bit index mask. Cosine adds 16384 in float before conversion. Moving that
addition after the cast can change the index. All float inputs are accepted,
including signed zero, infinities and NaNs; these are table lookups rather than
general mathematical sine/cosine functions. Both calls only read the table.

`mc_math_floor_double` is independent of a table. It applies Java double-to-int
conversion, compares the input against the integer promoted to double, then
decrements with Java int wrapping when smaller. Exact negative integers remain
unchanged. Inputs below INT_MIN, including negative infinity, can produce
INT_MAX after the wrapping decrement; NaN produces zero. This is distinct from
the [simplex floor convention](../world/simplex.md) and unrestricted mathematical
floor.

The supported numerical environment uses binary32/binary64 evaluation,
round-to-nearest and strict floating-point rules with contraction disabled.
The implementation checks expression evaluation width at compile time. Tests
preserve raw input encodings and compare returned bit patterns. Passing these
helpers establishes a cave prerequisite; normal cave scheduling and geometry
remain documented separately in the [cave runtime](../world/caves.md).
