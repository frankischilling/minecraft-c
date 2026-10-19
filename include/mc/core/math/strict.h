#ifndef MC_CORE_MATH_STRICT_H
#define MC_CORE_MATH_STRICT_H

/* Strict binary64 math for the captured x86-64/SSE2 profile.
 *
 * Both inputs must be positive, finite IEEE 754 binary64 values, including
 * positive subnormals. Callers must provide round-to-nearest, ties-to-even,
 * with gradual underflow: no FTZ/DAZ or excess intermediate precision. Build
 * these functions with strict floating rules, without reassociation or FMA
 * contraction. The functions do not change the caller's FP control settings.
 *
 * Outside that input/environment domain no result or floating-exception
 * behavior is promised. No general NaN payload, errno or fenv-flag parity is
 * claimed. Exact native/original-profile qualification remains separate from
 * the source implementation. No allocation or caller-owned state is required.
 */

/* Netlib fdlibm 5.3 IEEE log evaluation and coefficient bits. The separately
 * licensed source and adaptation record are identified in licenses/fdlibm.txt. */
double mc_math_strict_log(double value);

/* One SSE2 binary64 square root under the required caller FP environment. */
double mc_math_strict_sqrt(double value);

#endif
