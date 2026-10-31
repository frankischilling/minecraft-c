#ifndef MC_ITEM_RENDER_INTERNAL_H
#define MC_ITEM_RENDER_INTERNAL_H
#include "mc/item/registry/runtime.h"
#include "mc/item/stack/stack.h"

typedef struct mc_item_render_site {
    const char *class_name, *method, *descriptor;
    int32_t pc;
} mc_item_render_site;

typedef enum mc_item_render_throwable {
    MC_ITEM_RENDER_NO_THROWABLE,
    MC_ITEM_RENDER_SOURCE_EXCEPTION,
    MC_ITEM_RENDER_SOURCE_ERROR
} mc_item_render_throwable;

typedef struct mc_item_render_error {
    mc_registry_status status;
    const char *operation, *exception_class;
    bool native_policy, source_exception;
    mc_item_render_throwable throwable;
    mc_item_render_site site, child, ancestors[3];
    size_t ancestor_count;
    mc_registry_error registry;
    mc_item_stack_status stack;
    /* Passive actual operands, not an Item/Stack/array replacement. */
    uintptr_t stack_identity, array_identity, item_identity;
    int32_t id, damage;
    bool id_read, damage_read, returned;
} mc_item_render_error;

void mc_item_render_error_init(mc_item_render_error *);
/* Passive immutable declaring-class proof; no Source active use. */
void mc_item_render_registry_classification(mc_registry_classes *, const mc_registry_error *,
                                            mc_item_render_throwable *, const char **);
/* Synchronous entered kernels on independently held, live/exclusive genuine
 * storage. Nullable invocation checks belong to the Source caller. There is
 * no constructor/READY admission, class initialization or new owner hold in
 * the two pure Item bodies. Closed dispatch proves QZ inherits GM. */
bool mc_item_render_full_3d(mc_registry_classes *, const mc_item *, bool *, mc_item_render_error *);
bool mc_item_render_icon(mc_registry_classes *, const mc_item *, int32_t damage, int32_t *,
                         mc_item_render_error *);
/* Actual IZ.b -> IZ.a -> GM.b(IZ) -> IZ.i -> selected GM.a(int).
 * c_get precedes each current id read; c_at uses that captured array. */
bool mc_item_stack_render_icon(mc_registry_classes *, mc_item_stack *, int32_t *,
                               mc_item_render_error *);
#endif
