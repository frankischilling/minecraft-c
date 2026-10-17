#ifndef MC_CORE_MATH_MATH_H
#define MC_CORE_MATH_MATH_H

#include <stdbool.h>
#include <stdint.h>

#define MC_MATH_TRIG_VALUES 65536u

/* Caller-owned initialized lookup table. No internal pointers or allocation.
 * Treat values as read-only after successful initialization; a const table
 * can be shared by readers. Initialization/reinitialization requires exclusive
 * access, with no reader observing a partially filled table. */
typedef struct mc_math_trig {
    float values[MC_MATH_TRIG_VALUES];
} mc_math_trig;

/* Overwrite the complete table using the original ordered double-angle/sin
 * calculation followed by binary32 narrowing. Accepts arbitrary prior object
 * contents; null returns false. No other checked failure or allocation.
 * Exact table bits are validated on the measured x86-64 MSVC/GCC/Clang math
 * backends with binary32/binary64 round-to-nearest and strict floating rules.
 * Retains the original half-turn residual; values are not normalized. */
bool mc_math_trig_init(mc_math_trig *table);

/* Minecraft lookup semantics rather than general mathematical sin/cos.
 * Requires a nonnull, initialized, unmodified table. Float multiply, and for
 * cosine float phase addition, precede Java f2i conversion and index masking.
 * Every float angle is accepted, including nonfinite inputs and signed zero.
 * Calls only read table values; no implicit initialization or shared globals. */
float mc_math_sin(const mc_math_trig *table, float angle);
float mc_math_cos(const mc_math_trig *table, float angle);

/* Original double cast/compare helper, independent of a trig table. Truncate
 * with Java d2i; if value is below that integer converted back to double,
 * decrement with Java int wrapping. Nonfinite inputs have defined Java
 * narrowing behavior. This differs from unrestricted mathematical floor. */
int32_t mc_math_floor_double(double value);

#endif
