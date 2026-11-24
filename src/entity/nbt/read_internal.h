#ifndef MC_ENTITY_NBT_READ_INTERNAL_H
#define MC_ENTITY_NBT_READ_INTERNAL_H

#include "mc/entity/nbt/nbt.h"

/* Same source reader. A containing owner supplies actual external ownership
 * availability; its internal scratch alone cannot receive a failed GS93 child.
 * false stops only at reached93 after its real prefix; true requires out.
 * No source body, input snapshot, retry or World-derived Registry is added. */
mc_entity_nbt_status mc_entity_read_nbt_with_child_output(mc_entity *, mc_registry_classes *,
                                                          const mc_nbt_tag *, const mc_nbt_limits *,
                                                          bool can_transfer_children,
                                                          mc_entity_nbt_error *out);

#endif
