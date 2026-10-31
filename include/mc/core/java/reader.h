#ifndef MC_CORE_JAVA_READER_H
#define MC_CORE_JAVA_READER_H

#include "mc/core/java/properties.h"

typedef struct mc_java_reader mc_java_reader;

typedef enum mc_java_reader_status {
    MC_JAVA_READER_OK = 0,
    MC_JAVA_READER_INVALID_ARGUMENT,
    MC_JAVA_READER_ALLOCATION_FAILED,
    MC_JAVA_READER_LIMIT,
    MC_JAVA_READER_IO_ERROR,
    MC_JAVA_READER_UNSUPPORTED_CHARSET,
    MC_JAVA_READER_UNIMPLEMENTED_CLOSE
} mc_java_reader_status;

typedef enum mc_java_reader_charset {
    MC_JAVA_READER_DEFAULT,
    MC_JAVA_READER_UTF8,
    MC_JAVA_READER_WINDOWS1252,
    MC_JAVA_READER_ISO8859_1,
    MC_JAVA_READER_US_ASCII
} mc_java_reader_charset;

typedef struct mc_java_reader_input {
    mc_java_properties_input bytes;
    /* InputStreamReader uses available() only to avoid a blocking additional
     * read. NULL has InputStream's ordinary default of no advertised bytes.
     * Any ready callback uses bytes.context; exceptions correspond to false. */
    bool (*ready)(void *context);
    mc_java_reader_status (*close)(void *context);
} mc_java_reader_input;

typedef struct mc_java_reader_line {
    const uint16_t *units;
    size_t length;
} mc_java_reader_line;

/* Native default charset discovery does not alter process locale. A selected
 * supported charset is returned only on success. Other native locales remain
 * explicit unsupported cases until their actual decoders are implemented. */
mc_java_reader_status mc_java_reader_default_charset(mc_java_reader_charset *out);

/* The reached BufferedReader(InputStreamReader(InputStream,charset)) readLine
 * path. Bytes use the Properties input contract; decoding uses replacement
 * for malformed input, as the default Java InputStreamReader does. Storage
 * belongs to the retained caller arena, including incomplete prefixes. The
 * source's canonical empty literal has static immutable storage. Native
 * allocation failures do not emulate Java OOM.
 * Creation performs no byte reads and does not close input on failure. */
mc_java_reader_status mc_java_reader_create(const mc_java_properties_allocator *allocator,
                                            const mc_java_reader_input *input,
                                            mc_java_reader_charset charset, mc_java_reader **out);

/* CR, LF and CRLF terminate lines; no terminator is retained. EOF succeeds
 * with NULL; an empty line has a nonnull line owner. Earlier line
 * views stay live through further reads, failure and close. Native failures
 * leave out unchanged. Use requires exclusive access to owner/input/arena. */
mc_java_reader_status mc_java_reader_read_line(mc_java_reader *reader,
                                               const mc_java_reader_line **out);

/* Close the underlying reader once. Repeated close succeeds. Read after
 * close reports IO_ERROR. A missing close operation is explicitly unfinished;
 * it leaves the owner open and cannot become a successful no-op. Arena
 * storage remains caller-owned. */
mc_java_reader_status mc_java_reader_close(mc_java_reader *reader);

#endif
