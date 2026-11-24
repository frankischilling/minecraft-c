#ifndef MC_STORAGE_REGION_INTERNAL_H
#define MC_STORAGE_REGION_INTERNAL_H

#include <stddef.h>
#include <stdint.h>

/* Each input index names a packed location. Both index arrays have room for
 * count entries. The sort uses them as workspace and leaves the result in sorted. */
void mc_region_sort_ranges(const uint32_t *locations, uint16_t *indices, uint16_t *sorted,
                           size_t count);

#endif
