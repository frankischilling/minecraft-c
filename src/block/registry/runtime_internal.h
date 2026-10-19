#ifndef MC_BLOCK_REGISTRY_RUNTIME_INTERNAL_H
#define MC_BLOCK_REGISTRY_RUNTIME_INTERNAL_H

#include "mc/block/registry/registry.h"

/* Drop one independently owned native borrow, including inside an admitted
 * synchronous registry flow. No owner destruction/class use/callback occurs;
 * entered still prevents class destruction. Caller must own that exact live
 * borrow. Unlike ordinary public release, this internal retirement operation
 * does not reject entered, so an unrelated manager can retire safely during a
 * resource callback. False means a violated native ownership precondition. */
bool mc_registry_runtime_borrow_release(mc_registry_classes *classes);

#endif
