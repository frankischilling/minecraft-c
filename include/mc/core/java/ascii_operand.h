#ifndef MC_CORE_JAVA_ASCII_OPERAND_H
#define MC_CORE_JAVA_ASCII_OPERAND_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
/* Java String.equalsIgnoreCase against a nonnull ASCII operand. UTF16 units
 * remain counted; this is not locale-sensitive full String uppercasing.
 * Native caller guarantees actual readable lengths/storage and ASCII operand.
 * The selected JDK17 ASCII-equivalent non-ASCII units are I-dot/I-dotless,
 * long-s and Kelvin-sign. Supplementary pairs cannot equal an ASCII unit. */
bool mc_java_ascii_operand_equal_ignore_case(const uint16_t *, size_t, const char *);
#endif
