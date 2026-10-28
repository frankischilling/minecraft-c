/* Exact primitive TU once, with real stdio forwarding in test_java_output.c.
 * The final link map must exclude the archive byte_output.o in this target. */
#include <stdio.h>
size_t mc_test_output_write(const void *, size_t, size_t, FILE *);
int mc_test_output_ferror(FILE *);
int mc_test_output_close(FILE *);
#define fwrite mc_test_output_write
#define ferror mc_test_output_ferror
#define fclose mc_test_output_close
#include "../../../src/platform/files/byte_output.c"
#undef fwrite
#undef ferror
#undef fclose
