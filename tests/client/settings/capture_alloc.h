#ifndef MC_TEST_SETTINGS_CAPTURE_ALLOC_H
#define MC_TEST_SETTINGS_CAPTURE_ALLOC_H

/* Include the real CRT declarations BEFORE the narrowly forced test macro,
 * avoiding a renamed/imported malloc declaration on MSVC. Only the separately
 * declared direct production Info object receives this forced header/define. */
#include <stdlib.h>

void *mc_test_settings_capture_malloc(size_t bytes);
void mc_test_settings_capture_fail_next(void);

#ifdef MC_SETTINGS_CAPTURE_INFO_CONTROL
#define malloc mc_test_settings_capture_malloc
#endif

#endif
