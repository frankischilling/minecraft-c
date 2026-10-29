#ifndef MC_WORLD_STORAGE_JAVA_FILE_WRITER_H
#define MC_WORLD_STORAGE_JAVA_FILE_WRITER_H

#include "mc/core/java/writer.h"
#include "mc/world/storage/java_output.h"

typedef struct mc_java_typed_file_writer mc_java_typed_file_writer;

typedef enum mc_java_typed_file_writer_phase {
    MC_JAVA_TYPED_FILE_WRITER_RAW,
    MC_JAVA_TYPED_FILE_WRITER_CONSTRUCTING,
    MC_JAVA_TYPED_FILE_WRITER_READY,
    MC_JAVA_TYPED_FILE_WRITER_FAILED,
    MC_JAVA_TYPED_FILE_WRITER_RETIRING
} mc_java_typed_file_writer_phase;

typedef struct mc_java_typed_file_writer_options {
    size_t max_owner_bytes;
    mc_java_output_options file_output;
} mc_java_typed_file_writer_options;

typedef struct mc_java_typed_file_writer_error {
    mc_java_writer_status status;
    const char *operation;
    /* A propagated evidenced FOS exception, never a native policy/cleanup
     * refusal. PrintWriter-swallowed IO remains in output/writer state. */
    bool source_exception;
    mc_java_output_error output;
    mc_java_writer_error writer;
    mc_java_writer_retirement retirement;
} mc_java_typed_file_writer_error;

typedef struct mc_java_typed_file_writer_state {
    mc_java_typed_file_writer_phase phase;
    bool active, construct_entered, file_output_returned, writer_returned;
    bool output_state_present, writer_state_present;
    const mc_java_file_output *file_output;
    const mc_java_writer *writer;
    mc_java_file_output_state output_state;
    /* This scalar/static-text snapshot survives a consumed core Writer while
     * the wrapper retains an unconsumed FOS cleanup occurrence. */
    mc_java_writer_state writer_state;
    const mc_java_typed_file_writer_error *failure, *last_error, *cleanup;
    /* First complete FOS failure, including IO swallowed by PrintWriter. */
    const mc_java_output_error *first_output_failure;
} mc_java_typed_file_writer_state;

/* One exclusive concrete FileWriter(File)->PrintWriter(Writer) stack, sharing
 * the canonical counted FOS and unchanged core Writer/PrintWriter kernel.
 * Files/provider/native environment and supplied File/error views outlive all
 * operations and retained failures. No path extraction, reopen or parent mkdir.
 * Slots/errors/views must be disjoint; serial reentry is guarded, not general
 * thread safety. Inspect pointers borrow this owner until its next mutation or
 * successful retirement; const child identities are not callable child loans. */
mc_java_typed_file_writer_options mc_java_typed_file_writer_default_options(void);
mc_java_writer_status mc_java_typed_file_writer_allocate(mc_java_files *,
                                                         const mc_java_typed_file_writer_options *,
                                                         mc_java_typed_file_writer **,
                                                         mc_java_typed_file_writer_error *);

/* Cold allocation performs no File/charset/output work. Construct first runs
 * the SAME actual nonappend FOS(File), retaining failed candidates. Only after
 * its successful create/truncate does it read/resolve the Writer profile and
 * allocate the existing encoder in PRINT mode (autoFlush=false). A NULL profile
 * selects native DEFAULT charset/platform lines; explicit profiles require
 * PRINT mode. Nullable File reaches the actual FOS source exception first.
 * Failures are not resumable constructor attempts and remain inspectable.
 * Native allocation/charset failures do not invent Java OOM/Throwable parity. */
mc_java_writer_status mc_java_typed_file_writer_construct(mc_java_typed_file_writer *,
                                                          mc_java_file *,
                                                          const mc_java_writer_profile *,
                                                          mc_java_typed_file_writer_error *);
mc_java_writer_status mc_java_typed_file_writer_print(mc_java_typed_file_writer *,
                                                      const mc_java_properties_string_view *,
                                                      mc_java_typed_file_writer_error *);
mc_java_writer_status mc_java_typed_file_writer_println(mc_java_typed_file_writer *,
                                                        const mc_java_properties_string_view *,
                                                        mc_java_typed_file_writer_error *);
mc_java_writer_status mc_java_typed_file_writer_flush(mc_java_typed_file_writer *,
                                                      mc_java_typed_file_writer_error *);
mc_java_writer_status mc_java_typed_file_writer_source_close(mc_java_typed_file_writer *,
                                                             mc_java_typed_file_writer_error *);
bool mc_java_typed_file_writer_inspect(const mc_java_typed_file_writer *,
                                       mc_java_typed_file_writer_state *);

/* Native retirement never performs encoder flush, final replacement or Source
 * close. Its callback releases the canonical FOS occurrence natively. BUSY or
 * failed FOS cleanup keeps the wrapper and actual FOS for retry even if the core
 * Writer was consumed; this is observable in inspect. Only successful wrapper
 * consumption nulls the slot. Retrying does not close a reused OS descriptor.
 * NULL *slot succeeds. Already-closed successful owners remain inspectable
 * until this operation, allowing Settings to retain one bounded save attempt. */
mc_java_writer_status mc_java_typed_file_writer_retire(mc_java_typed_file_writer **,
                                                       mc_java_typed_file_writer_error *);

#endif
