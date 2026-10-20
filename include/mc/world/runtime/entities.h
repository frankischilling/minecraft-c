#ifndef MC_WORLD_RUNTIME_ENTITIES_H
#define MC_WORLD_RUNTIME_ENTITIES_H

#include "mc/entity/entity.h"
#include "mc/world/runtime/construction.h"

/* Bind the exact application Entity classes above all Worlds of this class.
 * Binding requires matching side/profile, no earlier World allocation, and no
 * existing Entity binding. It retains one native class hold without class
 * initialization, ID consumption or Random/Math activity. WorldClasses releases
 * that hold at destruction; the application profile still outlives both owners.
 * An absent binding stays an explicit dependency at reached Entity allocation. */
mc_entity_status mc_world_runtime_classes_bind_entities(mc_world_runtime_classes *classes,
                                                        mc_entity_classes *entities);

#endif
