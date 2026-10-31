#ifndef MC_CORE_JAVA_PROPERTIES_H
#define MC_CORE_JAVA_PROPERTIES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct mc_java_properties mc_java_properties;
typedef struct mc_java_properties_string mc_java_properties_string;

typedef enum mc_java_properties_status {
    MC_JAVA_PROPERTIES_OK = 0,
    MC_JAVA_PROPERTIES_INVALID_ARGUMENT,
    MC_JAVA_PROPERTIES_ALLOCATION_FAILED,
    MC_JAVA_PROPERTIES_LIMIT,
    MC_JAVA_PROPERTIES_IO_ERROR,
    MC_JAVA_PROPERTIES_MALFORMED_UNICODE,
    MC_JAVA_PROPERTIES_INDEX_OUT_OF_RANGE
} mc_java_properties_status;

/* Allocations belong to the caller's arena, which must outlive the complete
 * Properties owner and all views. allocate returns distinct, suitably aligned
 * storage of at least size bytes, or NULL. It need not clear the storage.
 * The module never releases or rolls back individual allocations; this also
 * preserves Strings returned before later overwrites. Allocation failure is
 * native resource behavior, not Java OutOfMemoryError scheduling. */
typedef struct mc_java_properties_allocator {
    void *context;
    void *(*allocate)(void *context, size_t size);
} mc_java_properties_allocator;

/* A real byte stream used by load(InputStream), not a decoded text callback.
 * read returns OK with 0..capacity bytes (zero means EOF), or IO_ERROR with no
 * bytes. The destination and count must not alias owner storage. The load
 * implementation requests at most 8192 bytes and never closes this stream.
 * The caller owns its lifetime and any partially consumed source on failure. */
typedef struct mc_java_properties_input {
    void *context;
    mc_java_properties_status (*read)(void *context, uint8_t *destination, size_t capacity,
                                      size_t *count);
} mc_java_properties_input;

typedef struct mc_java_properties_string_view {
    const uint16_t *units;
    size_t length;
} mc_java_properties_string_view;

/* String-only Properties with null defaults: the reached load(InputStream)
 * and getProperty contract. ISO-8859-1 bytes, continuations and escapes follow
 * the recorded JDK profile. Earlier entries survive malformed Unicode, I/O
 * or native allocation failure. Repeated load changes the same live owner.
 * No general Hashtable, Reader/XML/store, inherited defaults, concurrency or
 * arbitrary Object-value contract is supplied by this module. */
mc_java_properties_status mc_java_properties_create(const mc_java_properties_allocator *allocator,
                                                    mc_java_properties **out);
mc_java_properties_status mc_java_properties_load(mc_java_properties *properties,
                                                  const mc_java_properties_input *input);

/* Lookup uses exact UTF-16 String equality, including lone surrogates and NUL.
 * A missing key succeeds with NULL. NULL units with zero length is an empty
 * key, not a Java null reference. On native failure, outputs stay unchanged.
 * Saved String references preserve identity/content through later loads. */
mc_java_properties_status mc_java_properties_get(const mc_java_properties *properties,
                                                 const uint16_t *key, size_t length,
                                                 const mc_java_properties_string **out);
bool mc_java_properties_string_read(const mc_java_properties_string *string,
                                    mc_java_properties_string_view *out);
size_t mc_java_properties_size(const mc_java_properties *properties);

/* Diagnostic enumeration uses this native owner's first-insertion order.
 * It does not claim JDK map iteration order. Replacing a value preserves the
 * first key owner and its position; equal-content value Strings stay distinct.
 * key and value output slots must be disjoint from each other and owner
 * storage; identical slots are rejected without changing either output.
 * All calls/view use require exclusive access and live allocator storage. */
mc_java_properties_status mc_java_properties_entry(const mc_java_properties *properties,
                                                   size_t index,
                                                   const mc_java_properties_string **key,
                                                   const mc_java_properties_string **value);

#endif
