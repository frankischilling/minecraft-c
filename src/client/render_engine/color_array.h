#ifndef MC_CLIENT_RENDER_ENGINE_COLOR_ARRAY_H
#define MC_CLIENT_RENDER_ENGINE_COLOR_ARRAY_H
#include "mc/client/render_engine/render_engine.h"
#include "source.h"

typedef struct mc_client_color_array mc_client_color_array;

typedef enum mc_client_color_array_phase {
    MC_CLIENT_COLOR_ARRAY_RAW,
    MC_CLIENT_COLOR_ARRAY_READY,
    MC_CLIENT_COLOR_ARRAY_FAILED
} mc_client_color_array_phase;

typedef enum mc_client_color_array_status {
    MC_CLIENT_COLOR_ARRAY_OK,
    MC_CLIENT_COLOR_ARRAY_INVALID_ARGUMENT,
    MC_CLIENT_COLOR_ARRAY_INVALID_PHASE,
    MC_CLIENT_COLOR_ARRAY_LIMIT,
    MC_CLIENT_COLOR_ARRAY_ALLOCATION_FAILED,
    MC_CLIENT_COLOR_ARRAY_BUSY,
    MC_CLIENT_COLOR_ARRAY_NULL_REFERENCE,
    MC_CLIENT_COLOR_ARRAY_NEGATIVE_SIZE,
    MC_CLIENT_COLOR_ARRAY_UNIMPLEMENTED,
    MC_CLIENT_COLOR_ARRAY_INDEX_OUT_OF_RANGE
} mc_client_color_array_status;

typedef struct mc_client_color_array_limits {
    size_t max_native_bytes, max_allocations;
} mc_client_color_array_limits;

typedef struct mc_client_color_array_error {
    mc_client_color_array_status status;
    const char *operation;
    mc_client_graphics_site site;
    bool native_policy;
    int32_t width, height, actual_length;
    size_t native_allocations, native_bytes;
} mc_client_color_array_error;

typedef struct mc_client_color_array_state {
    mc_client_color_array_phase phase;
    size_t native_holds, active_calls, native_allocations, native_bytes;
    size_t length;
    const uint32_t *bits; /* Java int payload bits, not an image-plane alias. */
    mc_client_color_array_error failure;
} mc_client_color_array_state;

mc_client_color_array_limits mc_client_color_array_default_limits(void);
/* Native candidate output starts NULL. Once owner allocation succeeds it is
 * retained on failure, separately from a completed source newarray. A READY
 * candidate is the actual distinct zero-initialized int[] identity. No class,
 * resource, FP, GL or clock use. All slots/errors/owners are live and disjoint. */
mc_client_color_array_status mc_client_color_array_create(int32_t length,
                                                          const mc_client_color_array_limits *,
                                                          mc_client_graphics_site actual_newarray,
                                                          mc_client_color_array **candidate,
                                                          mc_client_color_array_error *);
mc_client_color_array_status mc_client_color_array_retain(mc_client_color_array *);
/* Successful release consumes/nulls this occurrence even when others remain.
 * Only a final active occurrence is BUSY and preserved. Cleanup allocates none. */
mc_client_color_array_status mc_client_color_array_release(mc_client_color_array **,
                                                           mc_client_color_array_error *);
bool mc_client_color_array_can_release(const mc_client_color_array *);
/* Final parent preflight counts all its aliased occurrences together. A hold
 * outside that retiring set permits parent cleanup while the array survives. */
bool mc_client_color_array_can_release_occurrences(const mc_client_color_array *, size_t count);
bool mc_client_color_array_inspect(const mc_client_color_array *, mc_client_color_array_state *);
/* Narrow native data primitive for declared actual-array fixtures/consumers;
 * no game method, class initialization or source caller is credited. */
mc_client_color_array_status mc_client_color_array_write(mc_client_color_array *, size_t,
                                                         uint32_t bits);

/* Actual ji.b(BufferedImage)[I: getWidth1 -> getHeight6 -> Java imul12 ->
 * newarray13 -> getRGB26. Candidate is retained on an entered native failure.
 * Caller keeps the genuine canonical getRGB image live/exclusive. */
mc_client_color_array_status mc_client_color_array_from_image(mc_client_argb_image *,
                                                              const mc_client_color_array_limits *,
                                                              mc_client_color_array **candidate,
                                                              mc_client_color_array_error *);
/* Actual selected ji.a(BufferedImage,int[]) path: width1/height6/getRGB20,
 * same destination identity. NULL/short destination getRGB behavior is an
 * explicit frontier before that child executes; no partial writes invented.
 * This helper alone does not complete ji.b() refresh452 or climate sampling. */
mc_client_color_array_status mc_client_color_array_fill(mc_client_argb_image *,
                                                        mc_client_color_array *,
                                                        mc_client_color_array_error *);
#endif
