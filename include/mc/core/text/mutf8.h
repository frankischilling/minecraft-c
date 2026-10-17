#ifndef MC_MUTF8_H
#define MC_MUTF8_H

#include <stddef.h>
#include <stdint.h>

typedef enum mc_mutf8_status {
    MC_MUTF8_OK,
    MC_MUTF8_INVALID_ARGUMENT,
    MC_MUTF8_INVALID_ENCODING,
    MC_MUTF8_BUFFER_TOO_SMALL,
    MC_MUTF8_OVERFLOW
} mc_mutf8_status;

/* Java modified UTF-8 payloads, without the data-stream length prefix.
 * Counts are explicit; UTF-16 units include embedded NUL and lone surrogates.
 * No allocation, terminator or 65535-byte limit is implicit.
 * Decode accepts the byte forms accepted by DataInputStream.readUTF, including
 * raw NUL and two-/three-byte non-shortest encodings. Encoding is canonical.
 *
 * Input may be NULL only when its count is zero. Output may be NULL only when
 * the result is empty. Count output pointers are required. Input, output and
 * count output storage must not overlap. Capacity counts elements, not bytes.
 * Failure leaves all outputs, including buffers and count outputs, unchanged.
 */
mc_mutf8_status mc_mutf8_decoded_length(const uint8_t *input, size_t length, size_t *units);
mc_mutf8_status mc_mutf8_decode(const uint8_t *input, size_t length, uint16_t *output,
                                size_t capacity, size_t *written);
mc_mutf8_status mc_mutf8_encoded_length(const uint16_t *input, size_t units, size_t *bytes);
mc_mutf8_status mc_mutf8_encode(const uint16_t *input, size_t units, uint8_t *output,
                                size_t capacity, size_t *written);

#endif
