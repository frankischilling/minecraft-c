#ifndef MC_WORLD_STORAGE_COMPRESSED_OUTPUT_INTERNAL_H
#define MC_WORLD_STORAGE_COMPRESSED_OUTPUT_INTERNAL_H
#include "mc/world/storage/compressed_output.h"

/* Passive native cleanup admission, including incomplete construction and
 * consumed-close retry prefixes. It neither finishes nor reads borrowed NBT. */
mc_java_output_status mc_compressed_output_terminal_admit(const mc_compressed_output *,
                                                          const mc_java_file_output *, bool *);
#endif
