/* Genuine resource-backed Registry fixture; its existing entry is not run. */
#define main mc_db_texture_unused_registry_fixture_main
#include "../../../item/registry/test_runtime.c"
#undef main

#include "mc/block/registry/rendering.h"

#include <errno.h>
#include <inttypes.h>
#include <stdlib.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif
