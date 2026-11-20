#ifndef MC_CLIENT_DISPLAY_SDK_CURSOR_H
#define MC_CLIENT_DISPLAY_SDK_CURSOR_H

#include "../render_engine/ge.h"
#include "sdk_input.h"

typedef struct mc_client_sdk_cursor mc_client_sdk_cursor;

/* Calls use the SDL main thread and live exclusive owners. Output/error/slot
 * storage is disjoint from owners and borrowed inspection views. Native
 * callbacks cannot reenter or retire the active cursor/input/Display. */

typedef enum mc_client_sdk_cursor_status {
    MC_CLIENT_SDK_CURSOR_OK,
    MC_CLIENT_SDK_CURSOR_INVALID_ARGUMENT,
    MC_CLIENT_SDK_CURSOR_INVALID_PHASE,
    MC_CLIENT_SDK_CURSOR_BUSY,
    MC_CLIENT_SDK_CURSOR_LIMIT,
    MC_CLIENT_SDK_CURSOR_ALLOCATION_FAILED,
    MC_CLIENT_SDK_CURSOR_DISPLAY_FAILED,
    MC_CLIENT_SDK_CURSOR_PLATFORM_FAILED,
    MC_CLIENT_SDK_CURSOR_BUFFER_FAILED,
    MC_CLIENT_SDK_CURSOR_CLOCK_FAILED,
    MC_CLIENT_SDK_CURSOR_SOURCE_EXCEPTION,
    MC_CLIENT_SDK_CURSOR_UNSUPPORTED_PROFILE
} mc_client_sdk_cursor_status;

typedef enum mc_client_sdk_cursor_phase {
    MC_CLIENT_SDK_CURSOR_RAW,
    MC_CLIENT_SDK_CURSOR_CONSTRUCTING,
    MC_CLIENT_SDK_CURSOR_READY,
    MC_CLIENT_SDK_CURSOR_FAILED,
    MC_CLIENT_SDK_CURSOR_RETIRING
} mc_client_sdk_cursor_phase;

typedef struct mc_client_sdk_cursor_limits {
    size_t max_bytes, max_allocations;
} mc_client_sdk_cursor_limits;

typedef struct mc_client_sdk_cursor_error {
    mc_client_sdk_cursor_status status;
    const char *operation, *exception_class;
    mc_client_graphics_site site, caller;
    bool native_policy, native_created, source_cursors_stored;
    mc_client_graphics_error buffer;
    mc_client_display_error display;
    mc_client_sdk_input_error input;
    unsigned long platform_error;
    int platform_clock_status;
    char detail[256];
} mc_client_sdk_cursor_error;

typedef struct mc_client_sdk_cursor_state {
    mc_client_sdk_cursor_phase phase;
    size_t holds, native_bytes, native_allocations;
    bool active, source_cursors_stored, source_destroyed, native_created, native_retired;
    int32_t source_index;
    int32_t last_store_pc, platform, native_hotspot_x, native_hotspot_y;
    unsigned best_width, best_height;
    bool one_bit_supported;
    int64_t element_delay, element_timeout;
    size_t flipped_position, flipped_limit;
    const void *native_handle;
    const mc_client_sdk_cursor_error *failure;
} mc_client_sdk_cursor_state;

mc_client_sdk_cursor_limits mc_client_sdk_cursor_default_limits(void);
/* Native RAW allocation retains a real canonical Display context lease. The
 * caller supplies its live, current SDK Display on the SDL main thread. No
 * cursor is created, selected or source-created flag stored here. Temporary
 * native provider/SDL overhead and the existing context lease have their own
 * bounds; max_bytes/max_allocations cover this owner's C storage. */
mc_client_sdk_cursor_status mc_client_sdk_cursor_allocate(mc_client_display *,
                                                          const mc_client_sdk_cursor_limits *,
                                                          mc_client_sdk_cursor **,
                                                          mc_client_sdk_cursor_error *);
/* Exact reached vy profile: Cursor(32,32,16,16,1,images,delays), with genuine
 * direct native-order views supplied by ge. Each has the required remaining
 * extent and stays live/exclusive through this call only. Implements real
 * flip/Windows one-bit conversion/native allocation and platform element
 * timing. Other frames/platforms/animated source methods remain unsupported.
 * No cursor selection or Mouse.currentCursor source store is performed. */
mc_client_sdk_cursor_status mc_client_sdk_cursor_construct32(mc_client_sdk_cursor *,
                                                             mc_client_sdk_input *,
                                                             mc_client_direct_int_buffer *images,
                                                             mc_client_direct_int_buffer *delays,
                                                             mc_client_sdk_cursor_error *);
mc_client_sdk_cursor_status mc_client_sdk_cursor_retain(mc_client_sdk_cursor *,
                                                        mc_client_sdk_cursor_error *);
/* Native retirement is not source Cursor.destroy(). Success consumes/nulls
 * this occurrence. BUSY/failure preserves it. A foreign-installed current
 * cursor must be deselected before final retirement. No source destroyed flag
 * or setNativeCursor call is invented. The Display stays held until cleanup
 * completes, including retry after earlier destructive native effects. */
mc_client_sdk_cursor_status mc_client_sdk_cursor_release(mc_client_sdk_cursor **,
                                                         mc_client_sdk_cursor_error *);
bool mc_client_sdk_cursor_inspect(const mc_client_sdk_cursor *, mc_client_sdk_cursor_state *);

#endif
