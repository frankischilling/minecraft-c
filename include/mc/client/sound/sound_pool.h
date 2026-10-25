#ifndef MC_CLIENT_SOUND_SOUND_POOL_H
#define MC_CLIENT_SOUND_SOUND_POOL_H

#include "mc/client/sound/sound.h"

#define MC_SOUND_POOL_MAX_TEXT_UNITS 65536u
#define MC_SOUND_POOL_MAX_PATH_BYTES 65536u
#define MC_SOUND_POOL_MAX_ENTRIES 8192u
#define MC_SOUND_POOL_MAX_GROUPS 4096u
#define MC_SOUND_POOL_MAX_ALLOCATION_BYTES (64u * 1024u * 1024u)

typedef struct mc_sound_pool mc_sound_pool;
typedef struct mc_sound_pool_registration mc_sound_pool_registration;
typedef struct mc_sound_pool_entry mc_sound_pool_entry;

typedef struct mc_sound_pool_report {
    int32_t source_pc;
    mc_sound_source_exception exception;
    bool group_created, entry_created, group_appended, full_list_appended, count_incremented;
} mc_sound_pool_report;

typedef struct mc_sound_pool_state {
    bool strip_digits, registration_active;
    int32_t count;
    size_t group_count, full_entry_count, owned_entry_count;
    size_t allocation_bytes, allocation_limit;
    /* Native storage value copy for observation, not the Java Random identity.
     * Changing this copy cannot seed, advance or otherwise mutate the pool. */
    mc_java_random random;
} mc_sound_pool_state;

typedef struct mc_sound_pool_group_view {
    mc_sound_text key;
    size_t entry_count;
} mc_sound_pool_group_view;

typedef struct mc_sound_pool_entry_view {
    mc_sound_text logical_name;
    mc_sound_file_identity file;
} mc_sound_pool_entry_view;

/* Create one real ordinary default Random through the shared live profile.
 * The profile must outlive construction, and should be the application's
 * shared profile rather than a reset per pool. No explicit seed is accepted.
 * max_allocation_bytes covers the owner, allocation headers, temporary owned
 * registration and live replacement arrays, up to MAX_ALLOCATION_BYTES.
 * Calls require exclusive access. Returned owners/entries remain unchanged on
 * failure; report and error outputs instead describe the reached prefix.
 * All supplied storage must be live, readable/writable as declared and
 * disjoint from owners and output storage modified by the call. */
mc_sound_status mc_sound_pool_create(mc_java_random_profile *profile, size_t max_allocation_bytes,
                                     mc_sound_pool **out, mc_sound_error *error);
/* NULL is harmless. A live registration makes retirement BUSY. */
mc_sound_status mc_sound_pool_free(mc_sound_pool *pool, mc_sound_error *error);
mc_sound_status mc_sound_pool_set_strip_digits(mc_sound_pool *pool, bool strip_digits,
                                               mc_sound_error *error);

/* First stage of hr.a(String,File): retain the original logical name, take the
 * prefix before its first dot, optionally remove selected-JDK17 BMP decimal
 * units, replace slash with dot, and insert any new empty group (source PC86).
 * Null, missing-dot and stripped-empty names report reached source failures.
 * Nonnull empty names fail the substring at PC10; '.ogg' with stripping off
 * instead creates the nonnull empty group. No normalization of UTF-16 occurs.
 * is_null requires {NULL,0}; nonnull empty text uses is_null=false.
 * A successful token holds the pool exclusive until commit or retirement.
 * Native failure may retain an inserted group; report records reached effects.
 * The token is native ownership, not a fabricated original File/URL object. */
mc_sound_status mc_sound_pool_begin(mc_sound_pool *pool, mc_sound_text logical_name,
                                    mc_sound_pool_registration **out, mc_sound_pool_report *report,
                                    mc_sound_error *error);

/* Resolve the real File/URI/URL between begin and commit. This call copies an
 * already resolved identity; it never claims to implement File.toURI/toURL.
 * A NULL identity represents source null File at PC98, after group insertion.
 * Nonnull malformed/native over-budget descriptors are native failures.
 * Group append (124), full-list append (136) and wrapped count store (149)
 * retain their reached prefixes on failure. A failed commit is terminal for
 * its token, which must be retired; no retry/rollback is hidden here.
 * Success retires the token, sets *registration=NULL and returns a stable
 * borrowed entry. Failure preserves *out and the live registration pointer. */
mc_sound_status mc_sound_pool_commit(mc_sound_pool_registration **registration,
                                     const mc_sound_file_identity *identity,
                                     const mc_sound_pool_entry **out, mc_sound_pool_report *report,
                                     mc_sound_error *error);
/* Release a token, including a failed one; never remove a reached group or
 * entry/list prefix. NULL is harmless. Do not retain a token after pool free. */
void mc_sound_pool_registration_free(mc_sound_pool_registration *registration);

/* Convenience route through the concrete native File/URI resolver. It begins
 * registration before resolving, commits only an actual resolved identity,
 * and always retires its token. NULL path reaches source null File at PC98. */
mc_sound_status mc_sound_pool_add_file(mc_sound_pool *pool, mc_sound_text logical_name,
                                       const char *native_utf8_path,
                                       const mc_sound_pool_entry **out,
                                       mc_sound_pool_report *report, mc_sound_error *error);

/* Lookup takes the actual already normalized key, not a new filename. Null
 * keys are legal HashMap misses. Missing keys and an empty full list return
 * OK with *out=NULL and no draw. Every present group uses real nextInt(size),
 * including size1; an empty retained group reports source bound failure at
 * PC31 without drawing. Selection outputs change only on successful return. */
mc_sound_status mc_sound_pool_select(mc_sound_pool *pool, mc_sound_text key,
                                     const mc_sound_pool_entry **out, mc_sound_pool_report *report,
                                     mc_sound_error *error);
mc_sound_status mc_sound_pool_select_any(mc_sound_pool *pool, const mc_sound_pool_entry **out,
                                         mc_sound_pool_report *report, mc_sound_error *error);

/* Quiescent read-only observations. Group indices follow native insertion
 * order for inspection; this does not claim Java HashMap iteration order.
 * Entry views and identities remain borrowed until pool retirement. The
 * original and resolved strings are owned copies, including NUL/surrogates in
 * counted text. Native UTF-8 filenames must be nonempty, valid UTF-8 C strings.
 * Indices out of range return native INVALID_ARGUMENT without changing out. */
mc_sound_status mc_sound_pool_read_state(const mc_sound_pool *pool, mc_sound_pool_state *out,
                                         mc_sound_error *error);
mc_sound_status mc_sound_pool_read_group(const mc_sound_pool *pool, size_t index,
                                         mc_sound_pool_group_view *out, mc_sound_error *error);
mc_sound_status mc_sound_pool_group_entry(const mc_sound_pool *pool, size_t group, size_t index,
                                          const mc_sound_pool_entry **out, mc_sound_error *error);
mc_sound_status mc_sound_pool_entry_at(const mc_sound_pool *pool, size_t index,
                                       const mc_sound_pool_entry **out, mc_sound_error *error);
mc_sound_status mc_sound_pool_entry_read(const mc_sound_pool_entry *entry,
                                         mc_sound_pool_entry_view *out, mc_sound_error *error);

#endif
