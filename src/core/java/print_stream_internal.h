#ifndef MC_CORE_JAVA_PRINT_STREAM_INTERNAL_H
#define MC_CORE_JAVA_PRINT_STREAM_INTERNAL_H

#include "../../platform/text/stdout.h"
#include "mc/core/java/print_stream.h"

#include <threads.h>

typedef mc_java_writer_status (*mc_ps_native_write)(void *, const mc_text_stdout *, const uint8_t *,
                                                    size_t, size_t *, bool *,
                                                    mc_java_writer_error *);

struct mc_java_print_stream {
    mtx_t monitor;
    size_t holds;
    mc_java_print_phase phase;
    bool active;
    thrd_t source_thread;
    bool auto_flush, trouble, closing, out_present, interrupted_thread_pending;
    bool interruption_in_write;
    bool nested_failure;
    mc_java_print_site nested_site;
    mc_text_stdout native;
    mc_java_print_profile profile;
    uint8_t bytes[128];
    size_t byte_count;
    uint16_t *text;
    size_t text_count;
    mc_java_source_encoder *encoder;
    bool physical_known;
    size_t physical, byte_returns, println_returns;
    mc_java_writer_error first_io;
    mc_java_print_error failure;
    /* Private native fault-control seam, not an alternative source encoder or
     * public callback contract. Production always uses the real byte kernel. */
    mc_ps_native_write native_write;
    void *native_context;
};

struct mc_java_system_output {
    mtx_t monitor;
    size_t holds;
    mc_java_print_phase phase;
    bool active;
    mc_java_print_stream *out, *pending, *pending_old;
    mc_java_print_error failure;
};

mc_java_print_status mc_ps_fail(mc_java_print_error *, mc_java_print_status, const char *);
mc_java_print_status mc_ps_native_profile(mc_java_print_profile *, mc_java_print_error *);
mc_java_writer_status mc_ps_native_stdout_write(void *, const mc_text_stdout *, const uint8_t *,
                                                size_t, size_t *, bool *, mc_java_writer_error *);

#endif
