#include "mc/world/storage/status.h"

const char *mc_world_storage_status_string(mc_world_storage_status status)
{
    switch (status) {
        case MC_WORLD_STORAGE_OK:
            return "ok";
        case MC_WORLD_STORAGE_INVALID_ARGUMENT:
            return "invalid argument";
        case MC_WORLD_STORAGE_MISSING:
            return "missing storage record";
        case MC_WORLD_STORAGE_IO_ERROR:
            return "filesystem operation failed";
        case MC_WORLD_STORAGE_INVALID_DATA:
            return "invalid stored data";
        case MC_WORLD_STORAGE_UNSUPPORTED_STATE:
            return "unsupported state change";
        case MC_WORLD_STORAGE_SESSION_MISMATCH:
            return "session token mismatch";
        case MC_WORLD_STORAGE_LIMIT:
            return "resource limit";
        case MC_WORLD_STORAGE_ALLOCATION_FAILED:
            return "allocation failed";
        case MC_WORLD_STORAGE_BUSY:
            return "chunk still referenced";
    }
    return "unknown world storage status";
}
