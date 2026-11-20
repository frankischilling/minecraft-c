#include "mc/entity/living/sleeping.h"

#include "../entity_internal.h"

#include <string.h>

static mc_entity_sleeping_status result(mc_entity_sleeping_error *error,
                                        mc_entity_sleeping_status status) {
    if (error != NULL)
        error->status = status;
    return status;
}

mc_entity_sleeping_status mc_entity_living_is_sleeping(mc_entity *living, bool *out,
                                                       mc_entity_sleeping_error *error) {
    if (error != NULL)
        memset(error, 0, sizeof(*error));
    if (out == NULL)
        return result(error, MC_ENTITY_SLEEPING_INVALID_ARGUMENT);
    if (living == NULL)
        return result(error, MC_ENTITY_SLEEPING_SOURCE_NULL);
    mc_entity_class_literal_view view;
    const mc_entity_status status = mc_entity_class_literal_inspect(living->class_literal, &view);
    if (error != NULL)
        error->entity = status;
    if (status != MC_ENTITY_OK)
        return result(error, MC_ENTITY_SLEEPING_ENTITY_FAILED);
    if (view.kind == MC_ENTITY_CLASS_KIND_LOCAL_PLAYER) {
        bool sleeping;
        if (mc_player_is_sleeping(living, &sleeping, error == NULL ? NULL : &error->player) !=
            MC_PLAYER_OK)
            return result(error, MC_ENTITY_SLEEPING_PLAYER_FAILED);
        *out = sleeping;
        return result(error, MC_ENTITY_SLEEPING_OK);
    }
    /* Untouched archive: LS.N()Z Code2=42ca1f85243342bd41983ab86f2235408
     * fa46ded7466d4525196f66b9dbb5f3e returns false. GS.N Code5 is the sole
     * override among every original Living subclass; native DC inherits it.
     * Query-only GS and future overrides are not fabricated as LS bodies. */
    switch (view.kind) {
        case MC_ENTITY_CLASS_KIND_LIVING_ENTITY:
        case MC_ENTITY_CLASS_KIND_MONSTER:
        case MC_ENTITY_CLASS_KIND_CREEPER:
        case MC_ENTITY_CLASS_KIND_SKELETON:
        case MC_ENTITY_CLASS_KIND_SPIDER:
        case MC_ENTITY_CLASS_KIND_GIANT:
        case MC_ENTITY_CLASS_KIND_ZOMBIE:
        case MC_ENTITY_CLASS_KIND_SLIME:
        case MC_ENTITY_CLASS_KIND_GHAST:
        case MC_ENTITY_CLASS_KIND_PIG_ZOMBIE:
        case MC_ENTITY_CLASS_KIND_PIG:
        case MC_ENTITY_CLASS_KIND_SHEEP:
        case MC_ENTITY_CLASS_KIND_COW:
        case MC_ENTITY_CLASS_KIND_CHICKEN:
        case MC_ENTITY_CLASS_KIND_SQUID:
        case MC_ENTITY_CLASS_KIND_WOLF:
            *out = false;
            return result(error, MC_ENTITY_SLEEPING_OK);
        default:
            return result(error, MC_ENTITY_SLEEPING_UNIMPLEMENTED_DEPENDENCY);
    }
}
