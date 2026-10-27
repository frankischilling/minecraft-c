#ifndef MC_CLIENT_GUI_FONT_MAPPING_H
#define MC_CLIENT_GUI_FONT_MAPPING_H
#include "mc/core/java/reader.h"
#include "mc/entity/player/player.h"
#include "mc/resources/translation/translation.h"

typedef struct mc_client_font_mapping mc_client_font_mapping;

typedef enum mc_client_font_mapping_status {
    MC_CLIENT_FONT_MAPPING_OK,
    MC_CLIENT_FONT_MAPPING_INVALID_ARGUMENT,
    MC_CLIENT_FONT_MAPPING_LIMIT,
    MC_CLIENT_FONT_MAPPING_ALLOCATION_FAILED,
    MC_CLIENT_FONT_MAPPING_BUSY,
    MC_CLIENT_FONT_MAPPING_UNIMPLEMENTED_DEPENDENCY,
    MC_CLIENT_FONT_MAPPING_READER_FAILED,
    MC_CLIENT_FONT_MAPPING_PREVIOUS_CLASS_FAILURE
} mc_client_font_mapping_status;

typedef struct mc_client_font_mapping_error {
    mc_client_font_mapping_status status, cause;
    const char *operation, *method;
    int32_t pc;
    mc_registry_status resource, close;
    mc_java_reader_status reader;
    mc_java_properties_status stream;
    bool source_exception_caught, normal_close_reached;
} mc_client_font_mapping_error;

typedef struct mc_client_font_mapping_state {
    mc_registry_phase phase;
    const mc_player_string *a; /* actual static String */
    const uint16_t *b;         /* actual separately allocated char[15] */
    const mc_player_string *failed_local_prefix;
    size_t native_bytes, native_allocations, native_holds;
    mc_client_font_mapping_error failure;
} mc_client_font_mapping_state;

/* One actual fp class lifetime, shared by the sj owners on that class loader.
 * Resources and its stream contexts remain borrowed through every failed hold.
 * empty_literal is the same canonical native String used by mk's literal.
 * Creation performs no resource active use/read. max_bytes accounts this
 * class's native reader arena/arrays, separately from String owner storage;
 * max_allocations bounds those allocations. Resource callbacks must not mutate,
 * reenter or retire this owner or its admitted String/resources dependencies.
 * Text is bounded to65536 units
 * and encoded input to1MiB. These limits do not emulate Java OOME scheduling. */
mc_client_font_mapping_status mc_client_font_mapping_create(const mc_registry_resources *,
                                                            mc_player_string *empty_literal,
                                                            size_t max_bytes,
                                                            size_t max_allocations,
                                                            mc_client_font_mapping **out);
mc_client_font_mapping_status mc_client_font_mapping_retain(mc_client_font_mapping *);
mc_client_font_mapping_status mc_client_font_mapping_release(mc_client_font_mapping *);
bool mc_client_font_mapping_inspect(const mc_client_font_mapping *, mc_client_font_mapping_state *);

/* Invoke only at a reached fp getstatic (sj.measure35 or draw288). Null text,
 * empty text and skipped formatting need not reach it. IOException/null stream
 * returns the completed-line prefix as source fp.a() catches Exception78;
 * native malformed callbacks/limits/allocation failures remain explicit.
 * Read failure does not invent a finally-close. Normal EOF reaches close72.
 * Failure and partial local state are retained; no failed initializer retry.
 * Returned a is borrowed until final owner release; out unchanged on failure. */
mc_client_font_mapping_status mc_client_font_mapping_active_use(mc_client_font_mapping *,
                                                                const mc_player_string **out,
                                                                mc_client_font_mapping_error *);
#endif
