#include "internal.h"

#ifdef _MSC_VER
static __declspec(thread) stats_source_error_scope *current_scope;
#else
static _Thread_local stats_source_error_scope *current_scope;
#endif

void stats_source_scope_open(stats_source_error_scope *scope, mc_client_stats_error *base) {
    *scope = (stats_source_error_scope){.previous = current_scope, .base = base};
    current_scope = scope;
}

void stats_source_scope_close(stats_source_error_scope *scope) {
    /* Every call is paired in its owning native stack frame. There is no
     * source callback or retained pointer that can outlive/reorder the scope. */
    current_scope = scope->previous;
}

static stats_source_error_scope *find_scope(const mc_client_stats_error *base) {
    for (stats_source_error_scope *scope = current_scope; scope != NULL; scope = scope->previous)
        if (scope->base == base)
            return scope;
    return NULL;
}

mc_client_stats_file_error stats_source_snapshot(const mc_client_stats_error *base) {
    const stats_source_error_scope *scope = find_scope(base);
    mc_client_stats_file_error result =
        scope == NULL ? (mc_client_stats_file_error){0} : scope->value;
    result.base = *base;
    return result;
}

void stats_source_child(mc_client_stats_error *base, mc_java_file_error child) {
    stats_source_error_scope *scope = find_scope(base);
    if (scope != NULL)
        scope->value.file = child;
}

void stats_source_frame(mc_client_stats_error *base, const char *cl, const char *method,
                        const char *descriptor, unsigned int pc) {
    stats_source_error_scope *scope = find_scope(base);
    if (scope != NULL && base->status != MC_CLIENT_STATS_OK && scope->value.caller_count < 4)
        scope->value.callers[scope->value.caller_count++] =
            (mc_client_stats_site){cl, method, descriptor, pc};
}

void stats_source_cause_set(mc_client_stats_error *base, const mc_client_stats_error *cause) {
    stats_source_error_scope *scope = find_scope(base);
    if (scope != NULL)
        scope->value.caught = (mc_client_stats_source_cause){
            cause->status, cause->exception,  cause->operation, cause->class_name,
            cause->method, cause->descriptor, cause->bytecode,  cause->system_error,
            cause->reader, scope->value.file};
}

void stats_source_publish_failure(mc_client_stats_writer *writer,
                                  const mc_client_stats_error *base) {
    stats_source *source = writer->memory->source;
    if (source != NULL && source->failure.base.status == MC_CLIENT_STATS_OK)
        source->failure = stats_source_snapshot(base);
}
