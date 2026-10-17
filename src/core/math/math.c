#include "mc/core/math/math.h"

#include "mc/core/java/java.h"

#include <float.h>
#include <math.h>
#include <stddef.h>

_Static_assert(FLT_EVAL_METHOD == 0, "binary32/binary64 expression evaluation required");

bool mc_math_trig_init(mc_math_trig *table)
{
    if (table == NULL) {
        return false;
    }
    for (uint32_t i = 0; i < MC_MATH_TRIG_VALUES; ++i) {
        double angle = (double)i * 0x1.921fb54442d18p+1;
        angle = angle * 2.0;
        angle = angle / 65536.0;
        table->values[i] = (float)sin(angle);
    }
    return true;
}

float mc_math_sin(const mc_math_trig *table, float angle)
{
    float scaled = angle * 0x1.45f306p+13f;
    int32_t index = mc_java_float_to_i32(scaled);
    return table->values[(uint32_t)index & UINT32_C(65535)];
}

float mc_math_cos(const mc_math_trig *table, float angle)
{
    float scaled = angle * 0x1.45f306p+13f;
    scaled = scaled + 16384.0f;
    int32_t index = mc_java_float_to_i32(scaled);
    return table->values[(uint32_t)index & UINT32_C(65535)];
}

int32_t mc_math_floor_double(double value)
{
    int32_t integer = mc_java_double_to_i32(value);
    return value < (double)integer ? mc_java_i32_sub(integer, 1) : integer;
}
