#ifndef MC_WORLD_STORAGE_STATUS_H
#define MC_WORLD_STORAGE_STATUS_H

#include <stdint.h>

typedef enum mc_world_storage_status {
    MC_WORLD_STORAGE_OK = 0,
    MC_WORLD_STORAGE_INVALID_ARGUMENT,
    MC_WORLD_STORAGE_MISSING,
    MC_WORLD_STORAGE_IO_ERROR,
    MC_WORLD_STORAGE_INVALID_DATA,
    MC_WORLD_STORAGE_UNSUPPORTED_STATE,
    MC_WORLD_STORAGE_SESSION_MISMATCH,
    MC_WORLD_STORAGE_LIMIT,
    MC_WORLD_STORAGE_ALLOCATION_FAILED,
    MC_WORLD_STORAGE_BUSY
} mc_world_storage_status;

typedef struct mc_world_storage_error {
    mc_world_storage_status status;
    const char *operation; /* Static diagnostic, no borrowed path or transient string. */
    uint32_t system_error; /* errno on POSIX, a documented native code on Windows. */
} mc_world_storage_error;

const char *mc_world_storage_status_string(mc_world_storage_status status);

#endif
