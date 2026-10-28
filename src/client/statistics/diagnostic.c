#include "internal.h"
#include <inttypes.h>
#include <stdio.h>

mc_client_stats_status stats_emit(mc_client_stats_writer *writer,
                                  const mc_client_stats_options *options,
                                  const mc_client_stats_diagnostic *event,
                                  mc_client_stats_report *report, mc_client_stats_error *error) {
    if (writer != NULL && writer->memory->source != NULL)
        return stats_source_diagnostic(writer, event, report, error);
    if (report != NULL) {
        ++report->diagnostics;
        if (event->kind == MC_CLIENT_STATS_UNKNOWN_STATISTIC)
            ++report->unknown_ids;
        if (event->kind == MC_CLIENT_STATS_CHECKSUM_MISMATCH)
            report->checksum_mismatch = true;
        if (event->kind == MC_CLIENT_STATS_CAUGHT_EXCEPTION)
            report->caught = event->cause;
    }
    if (writer != NULL) {
        mtx_lock(&writer->mutex);
        ++writer->diagnostic_depth;
        mtx_unlock(&writer->mutex);
    }
    mc_client_stats_status status = MC_CLIENT_STATS_OK;
    if (options->diagnostics.emit != NULL)
        status = options->diagnostics.emit(options->diagnostics.context, event);
    else {
        int result = 0;
        FILE *stream = stdout;
        switch (event->kind) {
            case MC_CLIENT_STATS_RELOCATING:
                result = fprintf(stdout, "Relocating %s\n", event->path_utf8);
                break;
            case MC_CLIENT_STATS_UNKNOWN_STATISTIC:
                result = fprintf(stdout, "%" PRId32 " is not a valid stat\n", event->statistic_id);
                break;
            case MC_CLIENT_STATS_CHECKSUM_MISMATCH:
                result = fputs("CHECKSUM MISMATCH\n", stdout);
                break;
            case MC_CLIENT_STATS_CAUGHT_EXCEPTION:
                stream = stderr;
                result = fprintf(
                    stderr, "[Client stats] caught source exception %d at %s.%s:%u: %s\n",
                    (int)event->cause.exception,
                    event->cause.class_name == NULL ? "native" : event->cause.class_name,
                    event->cause.method == NULL ? "operation" : event->cause.method,
                    event->cause.bytecode,
                    event->cause.operation == NULL ? "source failure" : event->cause.operation);
                break;
        }
        if (result < 0 || fflush(stream) != 0)
            status = MC_CLIENT_STATS_DIAGNOSTIC_FAILED;
    }
    if (writer != NULL) {
        mtx_lock(&writer->mutex);
        --writer->diagnostic_depth;
        mtx_unlock(&writer->mutex);
    }
    if (status != MC_CLIENT_STATS_OK)
        return stats_fail(error, MC_CLIENT_STATS_DIAGNOSTIC_FAILED,
                          "Client stats diagnostic transport", NULL, NULL, NULL, 0);
    return status;
}
