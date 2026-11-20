#ifndef MC_CORE_TEXT_MUTF8_INPUT_H
#define MC_CORE_TEXT_MUTF8_INPUT_H
#include "mc/core/text/mutf8.h"
#include <stdbool.h>

typedef struct mc_mutf8_prefix {
    size_t written, input_offset, error_byte;
    bool partial_character;
} mc_mutf8_prefix;

/* Private DataInput scratch operation over the SAME read_unit kernel. Unlike
 * the unchanged public failure-atomic decoder, this writes the valid char
 * prefix and reports exact failure progress. This is required for the actual
 * DataInputStream chararr state after malformed readUTF. No prevalidation pass.
 * error_byte/partial follow DataInputStream's two-/three-byte diagnostics. */
mc_mutf8_status mc_mutf8_decode_prefix(const uint8_t *input, size_t length, uint16_t *output,
                                       size_t capacity, mc_mutf8_prefix *progress);
#endif
