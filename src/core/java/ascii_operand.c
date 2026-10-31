#include "mc/core/java/ascii_operand.h"
#include <string.h>

bool mc_java_ascii_operand_equal_ignore_case(const uint16_t *units, size_t length,
                                             const char *ascii) {
    if (ascii == NULL || (length != 0 && units == NULL) || strlen(ascii) != length)
        return false;
    for (size_t i = 0; i < length; ++i) {
        uint16_t a = units[i];
        unsigned char b = (unsigned char)ascii[i];
        if (b > 127u)
            return false;
        if (b >= 'A' && b <= 'Z')
            b = (unsigned char)(b + ('a' - 'A'));
        if (a >= 'A' && a <= 'Z')
            a = (uint16_t)(a + ('a' - 'A'));
        else if (a == UINT16_C(0x0130) || a == UINT16_C(0x0131))
            a = 'i';
        else if (a == UINT16_C(0x017f))
            a = 's';
        else if (a == UINT16_C(0x212a))
            a = 'k';
        if (a != b)
            return false;
    }
    return true;
}
