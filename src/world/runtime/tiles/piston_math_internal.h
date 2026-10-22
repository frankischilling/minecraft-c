#ifndef MC_WORLD_RUNTIME_TILES_PISTON_MATH_INTERNAL_H
#define MC_WORLD_RUNTIME_TILES_PISTON_MATH_INTERNAL_H

#include <stdbool.h>
#include <stdint.h>

/* These operations require the admitted x86-64 binary32/binary64 environment.
 * The interpolation NaN choices qualify this composed original body against
 * its captured interpreter, rather than defining a general Java float ABI.
 * Raw input/result representations include signed zeros and NaN payloads. */
bool mc_piston_math_admitted(void);
uint32_t mc_piston_interpolate_bits(uint32_t current_bits, uint32_t previous_bits,
                                    uint32_t factor_bits);
float mc_piston_float_subtract(float left, float right);
float mc_piston_float_multiply(float left, float right);
float mc_piston_float_add(float left, float right);
double mc_piston_coordinate_subtract(double coordinate, float offset);

#endif
