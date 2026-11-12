#ifndef MC_CORE_MATH_TRIG_DOUBLE_H
#define MC_CORE_MATH_TRIG_DOUBLE_H
/* Concrete native binary64 Math adapter. Strict FP build/environment required.
 * Uses the platform double libm, never the game float lookup table. This is a
 * real effect; exact original-JDK bit parity remains a future observation gate. */
double mc_math_native_double_sin(double);
double mc_math_native_double_cos(double);
#endif
