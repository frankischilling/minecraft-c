#include "mc/core/java/integer_parse.h"

#include <inttypes.h>
#include <stdio.h>

/* Finite public-interface projection. Each one-unit input is enumerated here;
 * no expected digit classification/value or reference runtime is an input. */
int main(void) {
    if (printf("native-integer-decimal-unit-v1\n") < 0)
        return 1;
    for (uint32_t unit = 0; unit < UINT32_C(65536); ++unit) {
        const uint16_t input = (uint16_t)unit;
        const mc_java_integer_parse_text text = {&input, 1};
        int32_t value = 193;
        const mc_java_integer_parse_status status = mc_java_integer_parse(&text, &value);
        if (status == MC_JAVA_INTEGER_PARSE_OK) {
            if (printf("%" PRIu32 "\tvalue\t%" PRId32 "\n", unit, value) < 0)
                return 1;
        } else if (status == MC_JAVA_INTEGER_PARSE_NUMBER_FORMAT && value == 193) {
            if (printf("%" PRIu32 "\tNumberFormatException\n", unit) < 0)
                return 1;
        } else {
            fprintf(stderr, "unexpected parse status/output for unit %" PRIu32 "\n", unit);
            return 1;
        }
    }
    return printf("end\t65536\n") < 0 || fflush(stdout) != 0 || ferror(stdout) ? 1 : 0;
}
