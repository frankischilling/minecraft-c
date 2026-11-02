#include "../../block/registry/registry_internal.h"
#include "../stack/source_internal.h"
#include "internal.h"
#include "mc/block/registry/rendering.h"
#include "mc/block/registry/runtime.h"
#include <string.h>

static mc_item_render_site at(const char *c, const char *m, const char *d, int32_t pc) {
    return (mc_item_render_site){c, m, d, pc};
}

void mc_item_render_error_init(mc_item_render_error *e) {
    if (e == NULL)
        return;
    memset(e, 0, sizeof(*e));
    e->site.pc = e->child.pc = -1;
    for (size_t i = 0; i < 3; ++i)
        e->ancestors[i].pc = -1;
    e->registry.initializer_pc = e->registry.constructor_pc = e->registry.active_initializer_pc =
        -1;
}

static bool fail(mc_item_render_error *e, mc_registry_status status, const char *operation) {
    e->status = status;
    e->operation = operation;
    e->native_policy = e->throwable == MC_ITEM_RENDER_NO_THROWABLE;
    return false;
}

static bool source_null(mc_item_render_error *e, const char *operation) {
    e->throwable = MC_ITEM_RENDER_SOURCE_EXCEPTION;
    e->source_exception = true;
    e->exception_class = "java/lang/NullPointerException";
    return fail(e, MC_REGISTRY_NULL_REFERENCE, operation);
}

static const char *registry_source_exception(mc_registry_status status) {
    switch (status) {
        case MC_REGISTRY_NULL_REFERENCE:
            return "java/lang/NullPointerException";
        case MC_REGISTRY_INDEX_OUT_OF_RANGE:
            return "java/lang/ArrayIndexOutOfBoundsException";
        case MC_REGISTRY_ILLEGAL_ARGUMENT:
        case MC_REGISTRY_DUPLICATE_BLOCK:
        case MC_REGISTRY_DUPLICATE_STATISTIC:
            return "java/lang/IllegalArgumentException";
        default:
            return NULL;
    }
}

static bool same_registry_cause(const mc_registry_error *a, const mc_registry_error *b,
                                bool cached) {
    return (cached ? a->status == MC_REGISTRY_FAILED_INITIALIZATION : a->status == b->status) &&
           a->message == b->message && a->initializer_pc == b->initializer_pc &&
           a->block_id == b->block_id && a->constructor_class == b->constructor_class &&
           a->constructor_pc == b->constructor_pc && a->profile_status == b->profile_status &&
           a->active_initializer_class == b->active_initializer_class &&
           a->active_initializer_pc == b->active_initializer_pc &&
           a->item_argument == b->item_argument &&
           a->format_error.status == b->format_error.status &&
           a->format_error.offset == b->format_error.offset &&
           a->format_error.length == b->format_error.length &&
           a->format_error.value == b->format_error.value &&
           a->format_error.flags == b->format_error.flags &&
           a->format_error.conversion == b->format_error.conversion &&
           a->format_error.date_time == b->format_error.date_time;
}

/* JL active use is also reached by IZ's successful use wrapper. These are
 * actual independent initializer latches on the same Registry, including the
 * source translation/recipe dependencies they can reach. Inspect their exact
 * immutable causes; a native failed-bootstrap status alone proves no Error. */
static const mc_registry_error *other_initializer_cause(mc_registry_classes *classes,
                                                        const mc_registry_error *error, bool cached,
                                                        bool exception_only) {
    const mc_registry_class_state *states[] = {&classes->statistics_state,
                                               &classes->statistic_key_state,
                                               &classes->statistic_base_state,
                                               &classes->translation_state,
                                               &classes->translation_wrapper_state,
                                               &classes->achievements_state,
                                               &classes->crafting_state,
                                               &classes->smelting_state};
    for (size_t i = 0; i < sizeof states / sizeof *states; ++i)
        if (states[i]->phase == MC_REGISTRY_FAILED &&
            same_registry_cause(error, &states[i]->failure, cached) &&
            (!exception_only || registry_source_exception(states[i]->failure.status) != NULL))
            return &states[i]->failure;
    return NULL;
}

void mc_item_render_registry_classification(mc_registry_classes *classes,
                                            const mc_registry_error *error,
                                            mc_item_render_throwable *kind, const char **name) {
    *kind = MC_ITEM_RENDER_NO_THROWABLE;
    *name = NULL;
    const bool cached = error->status == MC_REGISTRY_FAILED_INITIALIZATION;
    const mc_registry_error *first = NULL;
    if (classes != NULL) {
        if (classes->blocks_phase == MC_REGISTRY_FAILED &&
            classes->block_states[MC_BLOCK_BASE].phase == MC_REGISTRY_FAILED &&
            same_registry_cause(&classes->block_failure,
                                &classes->block_states[MC_BLOCK_BASE].failure, false) &&
            same_registry_cause(error, &classes->block_failure, cached))
            first = &classes->block_failure;
        if (first == NULL) {
            for (size_t i = 0; i < (size_t)MC_BLOCK_KIND_COUNT; ++i)
                if (classes->block_states[i].phase == MC_REGISTRY_FAILED &&
                    same_registry_cause(error, &classes->block_states[i].failure, cached)) {
                    first = &classes->block_states[i].failure;
                    break;
                }
        }
        if (first == NULL) {
            for (size_t i = 0; i < (size_t)MC_ITEM_KIND_COUNT; ++i)
                if (classes->item_states[i].phase == MC_REGISTRY_FAILED &&
                    same_registry_cause(error, &classes->item_states[i].failure, cached)) {
                    first = &classes->item_states[i].failure;
                    break;
                }
        }
        if (first == NULL)
            first = other_initializer_cause(classes, error, cached, false);
    }
    if (first != NULL && registry_source_exception(first->status) != NULL) {
        *kind = MC_ITEM_RENDER_SOURCE_ERROR;
        *name = cached ? "java/lang/NoClassDefFoundError" : "java/lang/ExceptionInInitializerError";
    } else if (first != NULL && first->status == MC_REGISTRY_FAILED_INITIALIZATION &&
               classes != NULL) {
        /* A genuinely reached already erroneous dependency propagates its
         * Error unchanged. Match its actual immutable cause rather than
         * inferring a Throwable from FAILED_INITIALIZATION alone. */
        for (size_t i = 0; i < (size_t)MC_BLOCK_KIND_COUNT; ++i)
            if (classes->block_states[i].phase == MC_REGISTRY_FAILED &&
                same_registry_cause(first, &classes->block_states[i].failure, true) &&
                registry_source_exception(classes->block_states[i].failure.status) != NULL) {
                *kind = MC_ITEM_RENDER_SOURCE_ERROR;
                *name = "java/lang/NoClassDefFoundError";
                return;
            }
        for (size_t i = 0; i < (size_t)MC_ITEM_KIND_COUNT; ++i)
            if (classes->item_states[i].phase == MC_REGISTRY_FAILED &&
                same_registry_cause(first, &classes->item_states[i].failure, true) &&
                registry_source_exception(classes->item_states[i].failure.status) != NULL) {
                *kind = MC_ITEM_RENDER_SOURCE_ERROR;
                *name = "java/lang/NoClassDefFoundError";
                return;
            }
        if (other_initializer_cause(classes, first, true, true) != NULL) {
            *kind = MC_ITEM_RENDER_SOURCE_ERROR;
            *name = "java/lang/NoClassDefFoundError";
            return;
        }
    } else if (!cached && error->active_initializer_class == NULL) {
        *name = registry_source_exception(error->status);
        if (*name != NULL)
            *kind = MC_ITEM_RENDER_SOURCE_EXCEPTION;
    }
}

static bool registry_failed(mc_registry_classes *classes, mc_item_render_error *e) {
    mc_item_render_registry_classification(classes, &e->registry, &e->throwable,
                                           &e->exception_class);
    e->source_exception = e->throwable == MC_ITEM_RENDER_SOURCE_EXCEPTION;
    if (e->registry.active_initializer_class != NULL)
        e->child = at(e->registry.active_initializer_class, "<clinit>", "()V",
                      e->registry.active_initializer_pc);
    return fail(e, e->registry.status, e->registry.message);
}

/* Native storage provenance only; this does not supply a Source receiver or
 * initialize its class. Callers perform nullable invocation checks first. */
static bool admit(mc_registry_classes *classes, const mc_item *item, const void *out,
                  mc_item_render_error *e) {
    if (classes == NULL || item == NULL || out == NULL || item->classes != classes)
        return fail(e, MC_REGISTRY_INVALID_ARGUMENT, "Item render actual storage/output");
    if (classes->entered != 0)
        return fail(e, MC_REGISTRY_BUSY, "Item render actual Registry entered");
    return true;
}

typedef enum render_body {
    RENDER_NONE = 0,
    RENDER_GM,
    RENDER_DQ,
    RENDER_QD,
    RENDER_LA,
    RENDER_CS,
    RENDER_KM,
    RENDER_BI,
    RENDER_GO,
    RENDER_EN,
    RENDER_MR,
    RENDER_UY
} render_body;

typedef struct render_methods {
    render_body icon, color, full_3d, rotate;
} render_methods;

/* Actual supplied-client getMethod declarations for every represented Item
 * class. Constructor GM.bi is independent of the DQ/QD/LA/CS literal bodies.
 * Unimplemented metadata/color overrides remain their own source frontiers;
 * an additional kind has no inherited body until independently verified. */
static const render_methods methods[MC_ITEM_KIND_COUNT] = {
    [MC_ITEM_BASE] = {RENDER_GM, RENDER_GM, RENDER_GM, RENDER_GM},
    [MC_ITEM_SHOVEL] = {RENDER_GM, RENDER_GM, RENDER_DQ, RENDER_GM},
    [MC_ITEM_PICKAXE] = {RENDER_GM, RENDER_GM, RENDER_DQ, RENDER_GM},
    [MC_ITEM_AXE] = {RENDER_GM, RENDER_GM, RENDER_DQ, RENDER_GM},
    [MC_ITEM_FLINT_AND_STEEL] = {RENDER_GM, RENDER_GM, RENDER_GM, RENDER_GM},
    [MC_ITEM_FOOD] = {RENDER_GM, RENDER_GM, RENDER_GM, RENDER_GM},
    [MC_ITEM_BOW] = {RENDER_GM, RENDER_GM, RENDER_GM, RENDER_GM},
    [MC_ITEM_COAL] = {RENDER_GM, RENDER_GM, RENDER_GM, RENDER_GM},
    [MC_ITEM_SWORD] = {RENDER_GM, RENDER_GM, RENDER_QD, RENDER_GM},
    [MC_ITEM_HOE] = {RENDER_GM, RENDER_GM, RENDER_LA, RENDER_GM},
    [MC_ITEM_SEEDS] = {RENDER_GM, RENDER_GM, RENDER_GM, RENDER_GM},
    [MC_ITEM_ARMOR] = {RENDER_GM, RENDER_GM, RENDER_GM, RENDER_GM},
    [MC_ITEM_PAINTING] = {RENDER_GM, RENDER_GM, RENDER_GM, RENDER_GM},
    [MC_ITEM_SIGN] = {RENDER_GM, RENDER_GM, RENDER_GM, RENDER_GM},
    [MC_ITEM_DOOR] = {RENDER_GM, RENDER_GM, RENDER_GM, RENDER_GM},
    [MC_ITEM_BUCKET] = {RENDER_GM, RENDER_GM, RENDER_GM, RENDER_GM},
    [MC_ITEM_MINECART] = {RENDER_GM, RENDER_GM, RENDER_GM, RENDER_GM},
    [MC_ITEM_SADDLE] = {RENDER_GM, RENDER_GM, RENDER_GM, RENDER_GM},
    [MC_ITEM_REDSTONE] = {RENDER_GM, RENDER_GM, RENDER_GM, RENDER_GM},
    [MC_ITEM_SNOWBALL] = {RENDER_GM, RENDER_GM, RENDER_GM, RENDER_GM},
    [MC_ITEM_BOAT] = {RENDER_GM, RENDER_GM, RENDER_GM, RENDER_GM},
    [MC_ITEM_BLOCK_PLACER] = {RENDER_GM, RENDER_GM, RENDER_GM, RENDER_GM},
    [MC_ITEM_EGG] = {RENDER_GM, RENDER_GM, RENDER_GM, RENDER_GM},
    [MC_ITEM_FISHING_ROD] = {RENDER_GM, RENDER_GM, RENDER_CS, RENDER_CS},
    [MC_ITEM_DYE] = {RENDER_KM, RENDER_GM, RENDER_GM, RENDER_GM},
    [MC_ITEM_BED] = {RENDER_GM, RENDER_GM, RENDER_GM, RENDER_GM},
    [MC_ITEM_SHEARS] = {RENDER_GM, RENDER_GM, RENDER_GM, RENDER_GM},
    [MC_ITEM_RECORD] = {RENDER_GM, RENDER_GM, RENDER_GM, RENDER_GM},
    [MC_ITEM_SOUP] = {RENDER_GM, RENDER_GM, RENDER_GM, RENDER_GM},
    [MC_ITEM_COOKIE] = {RENDER_GM, RENDER_GM, RENDER_GM, RENDER_GM},
    [MC_ITEM_MAP] = {RENDER_GM, RENDER_GM, RENDER_GM, RENDER_GM},
    [MC_ITEM_BLOCK] = {RENDER_GM, RENDER_GM, RENDER_GM, RENDER_GM},
    [MC_ITEM_WOOL_BLOCK] = {RENDER_BI, RENDER_GM, RENDER_GM, RENDER_GM},
    [MC_ITEM_LOG_BLOCK] = {RENDER_GO, RENDER_GM, RENDER_GM, RENDER_GM},
    [MC_ITEM_SLAB_BLOCK] = {RENDER_EN, RENDER_GM, RENDER_GM, RENDER_GM},
    [MC_ITEM_SAPLING_BLOCK] = {RENDER_MR, RENDER_GM, RENDER_GM, RENDER_GM},
    [MC_ITEM_LEAVES_BLOCK] = {RENDER_UY, RENDER_UY, RENDER_GM, RENDER_GM},
    [MC_ITEM_PISTON_BLOCK] = {RENDER_GM, RENDER_GM, RENDER_GM, RENDER_GM}};

static const render_methods *item_methods(const mc_item *item) {
    return (unsigned int)item->kind < (unsigned int)MC_ITEM_KIND_COUNT ? &methods[item->kind]
                                                                       : NULL;
}

static const char *body_class(render_body body) {
    static const char *const names[] = {NULL, "gm", "dq", "qd", "la", "cs",
                                        "km", "bi", "go", "en", "mr", "uy"};
    return names[body];
}

static bool icon_entered(mc_registry_classes *classes, const mc_item *item, int32_t damage,
                         int32_t *out, mc_item_render_error *e) {
    if (!admit(classes, item, out, e))
        return false;
    const render_methods *resolved = item_methods(item);
    if (resolved == NULL || resolved->icon == RENDER_NONE)
        return fail(e, MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY,
                    "Item render unresolved icon declaration");
    if (resolved->icon == RENDER_BI) {
        e->site = at("bi", "a", "(I)I", 0);
        const mc_block *wool = NULL;
        if (mc_registry_block_static_get(classes, "ac", &wool, &e->registry) != MC_REGISTRY_OK)
            return registry_failed(classes, e);
        e->site.pc = 5;
        const mc_registry_status initialized =
            mc_registry_block_class_initialize(classes, MC_BLOCK_WOOL);
        if (initialized != MC_REGISTRY_OK) {
            e->registry = classes->last_error;
            return registry_failed(classes, e);
        }
        /* BI uses EE.d, the inverse dye-order mapping, before the virtual
         * Block texture call. Preserve the captured actual UU.ac receiver. */
        const int32_t metadata = (~damage) & 15;
        e->site.pc = 8;
        if (wool == NULL)
            return source_null(e, "Wool Item icon null captured Block receiver");
        const mc_registry_status status = mc_block_render_texture_metadata(wool, 2, metadata, out);
        if (status != MC_REGISTRY_OK)
            return fail(e, status, "Wool Item icon actual Block texture override");
        e->site.pc = 11;
        return true;
    }
    if (resolved->icon != RENDER_GM) {
        e->site = at(body_class(resolved->icon), "a", "(I)I", 0);
        return fail(e, MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY,
                    "Item render actual icon override boundary");
    }
    e->site = at("gm", "a", "(I)I", 1);
    *out = item->fields.bh;
    e->site.pc = 4;
    return true;
}

bool mc_item_render_icon(mc_registry_classes *classes, const mc_item *item, int32_t damage,
                         int32_t *out, mc_item_render_error *e) {
    if (e == NULL)
        return false;
    mc_item_render_error_init(e);
    e->damage = damage;
    e->item_identity = (uintptr_t)item;
    if (!icon_entered(classes, item, damage, out, e))
        return false;
    e->returned = true;
    return true;
}

bool mc_item_render_full_3d(mc_registry_classes *classes, const mc_item *item, bool *out,
                            mc_item_render_error *e) {
    if (e == NULL)
        return false;
    mc_item_render_error_init(e);
    e->item_identity = (uintptr_t)item;
    if (!admit(classes, item, out, e))
        return false;
    const render_methods *resolved = item_methods(item);
    if (resolved == NULL || resolved->full_3d == RENDER_NONE)
        return fail(e, MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY,
                    "Item render unresolved full3D declaration");
    if (resolved->full_3d == RENDER_GM) {
        e->site = at("gm", "b", "()Z", 1);
        *out = item->fields.bi;
        e->site.pc = 4;
    } else if (resolved->full_3d == RENDER_DQ || resolved->full_3d == RENDER_QD ||
               resolved->full_3d == RENDER_LA || resolved->full_3d == RENDER_CS) {
        e->site = at(body_class(resolved->full_3d), "b", "()Z", 0);
        *out = true; /* Actual DQ/QD/LA/CS literal1, independent of GM.bi. */
        e->site.pc = 1;
    } else
        return fail(e, MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY,
                    "Item render actual full3D override boundary");
    e->returned = true;
    return true;
}

bool mc_item_render_color(mc_registry_classes *classes, const mc_item *item, int32_t damage,
                          int32_t *out, mc_item_render_error *e) {
    if (e == NULL)
        return false;
    mc_item_render_error_init(e);
    e->damage = damage;
    e->item_identity = (uintptr_t)item;
    if (!admit(classes, item, out, e))
        return false;
    const render_methods *resolved = item_methods(item);
    if (resolved == NULL || resolved->color == RENDER_NONE)
        return fail(e, MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY,
                    "Item render unresolved color declaration");
    e->site = at(body_class(resolved->color), "f", "(I)I", 0);
    if (resolved->color != RENDER_GM)
        return fail(e, MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY, "Item actual metadata color override");
    *out = 16777215;
    e->site.pc = 2;
    e->returned = true;
    return true;
}

bool mc_item_render_rotate(mc_registry_classes *classes, const mc_item *item, bool *out,
                           mc_item_render_error *e) {
    if (e == NULL)
        return false;
    mc_item_render_error_init(e);
    e->item_identity = (uintptr_t)item;
    if (!admit(classes, item, out, e))
        return false;
    const render_methods *resolved = item_methods(item);
    if (resolved == NULL || resolved->rotate == RENDER_NONE)
        return fail(e, MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY,
                    "Item render unresolved rotating declaration");
    e->site = at(body_class(resolved->rotate), "c", "()Z", 0);
    if (resolved->rotate != RENDER_GM && resolved->rotate != RENDER_CS)
        return fail(e, MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY,
                    "Item render actual rotating override boundary");
    *out = resolved->rotate == RENDER_CS; /* CS literal1; inherited GM literal0. */
    e->site.pc = 1;
    e->returned = true;
    return true;
}

bool mc_item_stack_render_icon(mc_registry_classes *classes, mc_item_stack *stack, int32_t *out,
                               mc_item_render_error *e) {
    if (e == NULL)
        return false;
    mc_item_render_error_init(e);
    e->stack_identity = (uintptr_t)stack;
    if (stack == NULL || out == NULL)
        return fail(e, MC_REGISTRY_INVALID_ARGUMENT, "Item render entered IZ storage/output");
    e->ancestors[0] = at("iz", "b", "()I", 1);
    e->ancestor_count = 1;
    e->site = at("iz", "a", "()Lgm;", 0);
    const mc_registry_item_array *array;
    if (mc_registry_item_c_get(classes, &array, &e->registry) != MC_REGISTRY_OK)
        return registry_failed(classes, e);
    e->array_identity = (uintptr_t)array;
    e->site.pc = 4;
    e->stack = mc_item_stack_source_id_get(stack, &e->id);
    if (e->stack != MC_ITEM_STACK_OK)
        return fail(e, MC_REGISTRY_INVALID_ARGUMENT, "Item render actual IZ id field");
    e->id_read = true;
    e->site.pc = 7;
    const mc_item *item;
    if (mc_registry_item_c_at(classes, array, e->id, &item, &e->registry) != MC_REGISTRY_OK)
        return registry_failed(classes, e);
    e->item_identity = (uintptr_t)item;
    e->site = at("iz", "b", "()I", 5);
    e->ancestor_count = 0;
    if (item == NULL)
        return source_null(e, "Item render null captured GM invoke receiver");
    e->ancestors[0] = e->site;
    e->ancestor_count = 1;
    e->site = at("gm", "b", "(Liz;)I", 2);
    e->stack = mc_item_stack_source_damage_get(stack, &e->damage); /* IZ.i field1/return4. */
    if (e->stack != MC_ITEM_STACK_OK)
        return fail(e, MC_REGISTRY_INVALID_ARGUMENT, "Item render actual IZ damage field");
    e->damage_read = true;
    e->site.pc = 5;
    e->ancestors[1] = e->site;
    e->ancestor_count = 2;
    if (!icon_entered(classes, item, e->damage, out, e))
        return false;
    e->ancestor_count = 0;
    e->site = at("iz", "b", "()I", 8);
    e->returned = true;
    return true;
}
