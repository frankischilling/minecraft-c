#include "../../block/registry/registry_internal.h"
#include "../../world/runtime/runtime_internal.h"
#include "../classes/memory_internal.h"
#include "../construction_internal.h"
#include "../living/construction_internal.h"
#include "../player/player_internal.h"
#include "../player/tick_math_internal.h"
#include "internal.h"
#include "mc/block/registry/runtime.h"
#include "mc/world/runtime/entities.h"
#include <string.h>

static mc_mob_site location(const char *cl, const char *method, const char *descriptor,
                            int32_t pc) {
    return (mc_mob_site){cl, method, descriptor, pc};
}

static const char *source_class(mc_entity_class_kind kind) {
    switch (kind) {
        case MC_ENTITY_CLASS_KIND_SPIDER:
            return "cn";
        case MC_ENTITY_CLASS_KIND_ZOMBIE:
            return "uz";
        case MC_ENTITY_CLASS_KIND_SKELETON:
            return "fr";
        case MC_ENTITY_CLASS_KIND_CREEPER:
            return "gb";
        case MC_ENTITY_CLASS_KIND_SLIME:
            return "uw";
        case MC_ENTITY_CLASS_KIND_PIG:
            return "wh";
        case MC_ENTITY_CLASS_KIND_SHEEP:
            return "dl";
        case MC_ENTITY_CLASS_KIND_COW:
            return "bx";
        case MC_ENTITY_CLASS_KIND_CHICKEN:
            return "ww";
        case MC_ENTITY_CLASS_KIND_WOLF:
            return "gi";
        case MC_ENTITY_CLASS_KIND_SQUID:
            return "xt";
        default:
            return NULL;
    }
}

bool mc_entity_is_mob_kind(mc_entity_class_kind kind) {
    return source_class(kind) != NULL;
}

bool mc_entity_has_mob(const mc_entity *entity) {
    return entity != NULL && entity->mob != NULL && entity->class_literal != NULL &&
           mc_entity_is_mob_kind(entity->class_literal->kind);
}

static mc_mob_status result(mc_mob_error *e, mc_mob_status status, const char *operation) {
    if (e != NULL) {
        memset(e, 0, sizeof(*e));
        e->status = status;
        e->operation = operation;
        e->native_policy = status != MC_MOB_OK;
        e->site.pc = e->child.pc = -1;
    }
    return status;
}

static mc_mob_status fail(mc_mob_error *e, mc_mob_status status, const char *operation,
                          const char *cl, const char *method, const char *descriptor, int32_t pc,
                          bool native) {
    e->status = status;
    e->operation = operation;
    e->site = location(cl, method, descriptor, pc);
    e->native_policy = native;
    return status;
}

static mc_mob_status entity_status(mc_entity_status status) {
    return status == MC_ENTITY_LIMIT               ? MC_MOB_LIMIT
           : status == MC_ENTITY_ALLOCATION_FAILED ? MC_MOB_ALLOCATION
           : status == MC_ENTITY_UNSUPPORTED_FP    ? MC_MOB_UNSUPPORTED_FP
                                                   : MC_MOB_ENTITY_FAILED;
}

static mc_mob_class_cause class_cause(const mc_mob_error *e) {
    return (mc_mob_class_cause){
        e->status,           e->operation,       e->site,   e->child,    e->native_policy,
        e->source_exception, e->exception_class, e->entity, e->registry, e->stack};
}

static bool initializer_wraps(const char *exception) {
    /* Only these concrete non-Error exceptions are produced here. Initializer
     * and linkage Errors propagate with their original class identity. */
    return exception != NULL &&
           (strcmp(exception, "java/lang/NullPointerException") == 0 ||
            strcmp(exception, "java/lang/ArrayIndexOutOfBoundsException") == 0 ||
            strcmp(exception, "java/lang/IllegalArgumentException") == 0);
}

static mc_mob_status class_latch(mc_mob_class_namespace *n, mc_mob_error *e) {
    n->skeleton_phase = MC_ENTITY_CLASS_FAILED;
    n->failure = *e;
    e->first_class_failure = class_cause(&n->failure);
    if (e->source_exception && initializer_wraps(e->exception_class))
        e->exception_class = "java/lang/ExceptionInInitializerError";
    return e->status;
}

void mc_mob_classes_dispose(mc_entity_classes *classes) {
    mc_mob_class_namespace *n = classes->mobs;
    if (n == NULL)
        return;
    mc_item_stack_release(n->skeleton_a);
    mc_item_stack_release(n->skeleton_candidate);
    classes->allocated_bytes -= n->skeleton_stack_bytes;
    mc_mob_sheep_colors *colors = n->sheep_a == NULL ? n->sheep_candidate : n->sheep_a;
    if (colors != NULL) {
        for (size_t i = 0; i < colors->length; ++i)
            mc_entity_class_memory_retire(classes, colors->rows[i], sizeof(*colors->rows[i]));
        mc_entity_class_memory_retire(classes, colors, sizeof(*colors));
    }
    mc_entity_class_memory_retire(classes, n->sheep_row_candidate, sizeof(*n->sheep_row_candidate));
    mc_entity_class_memory_retire(classes, n, sizeof(*n));
    classes->mobs = NULL;
    classes->mob_dispose = NULL;
}

static mc_mob_class_namespace *class_namespace(mc_entity_classes *classes, mc_mob_error *e,
                                               const char *cl) {
    if (classes->mobs != NULL)
        return classes->mobs;
    mc_mob_class_namespace *n = mc_entity_class_memory_allocate(
        classes, sizeof(*n), &e->entity, "Mob native class initialization storage", 0);
    if (n == NULL) {
        fail(e, entity_status(e->entity.status), e->entity.operation, cl, "<clinit>", "()V", -1,
             true);
        return NULL;
    }
    *n = (mc_mob_class_namespace){0};
    classes->mobs = n;
    classes->mob_dispose = mc_mob_classes_dispose;
    return n;
}

static mc_mob_status sheep_use(mc_entity_classes *classes, mc_mob_error *e) {
    mc_mob_class_namespace *n = classes->mobs;
    if (n != NULL && n->sheep_phase == MC_ENTITY_CLASS_READY)
        return MC_MOB_OK;
    if (n != NULL && n->sheep_phase == MC_ENTITY_CLASS_FAILED) {
        e->first_class_failure = class_cause(&n->sheep_failure);
        e->source_exception = n->sheep_failure.source_exception;
        e->exception_class = e->source_exception ? "java/lang/NoClassDefFoundError" : NULL;
        return fail(e, MC_MOB_CLASS_ERRONEOUS, "previous Sheep class initialization failure", "dl",
                    "<clinit>", "()V", (int32_t)n->sheep_pc, n->sheep_failure.native_policy);
    }
    if (n != NULL && n->sheep_phase != MC_ENTITY_CLASS_COLD)
        return fail(e, MC_MOB_BUSY, "Sheep class initializer active", "dl", "<clinit>", "()V", -1,
                    true);
    n = class_namespace(classes, e, "dl");
    if (n == NULL)
        return e->status;
    n->sheep_phase = MC_ENTITY_CLASS_INITIALIZING;
    n->sheep_pc = 2;
    n->sheep_candidate = mc_entity_class_memory_allocate(
        classes, sizeof(*n->sheep_candidate), &e->entity, "Sheep actual float[16][] allocation", 2);
    if (n->sheep_candidate == NULL)
        goto failed;
    *n->sheep_candidate = (mc_mob_sheep_colors){.length = 16};
    /* Values are source constants; each row below is a distinct actual native
     * allocation. This table is neither the static field nor a ready cache. */
    static const float values[16][3] = {
        {1.0f, 1.0f, 1.0f}, {0.95f, 0.7f, 0.2f}, {0.9f, 0.5f, 0.85f}, {0.6f, 0.7f, 0.95f},
        {0.9f, 0.9f, 0.2f}, {0.5f, 0.8f, 0.1f},  {0.95f, 0.7f, 0.8f}, {0.3f, 0.3f, 0.3f},
        {0.6f, 0.6f, 0.6f}, {0.3f, 0.6f, 0.7f},  {0.7f, 0.4f, 0.9f},  {0.2f, 0.4f, 0.8f},
        {0.5f, 0.4f, 0.3f}, {0.4f, 0.5f, 0.2f},  {0.8f, 0.3f, 0.3f},  {0.1f, 0.1f, 0.1f}};
    static const uint16_t row_pcs[16] = {8,   26,  47,  68,  89,  110, 132, 154,
                                         176, 198, 220, 242, 264, 286, 308, 330};
    static const unsigned int publish_pcs[16] = {22,  43,  64,  85,  106, 127, 149, 171,
                                                 193, 215, 237, 259, 281, 303, 325, 347};
    n->sheep_array_bytes = sizeof(*n->sheep_candidate);
    for (size_t i = 0; i < 16; ++i) {
        n->sheep_pc = row_pcs[i];
        n->sheep_row_candidate =
            mc_entity_class_memory_allocate(classes, sizeof(*n->sheep_row_candidate), &e->entity,
                                            "Sheep actual float[3] allocation", row_pcs[i]);
        if (n->sheep_row_candidate == NULL)
            goto failed;
        n->sheep_array_bytes += sizeof(*n->sheep_row_candidate);
        *n->sheep_row_candidate = (mc_mob_sheep_color_row){.length = 3};
        for (size_t j = 0; j < 3; ++j)
            n->sheep_row_candidate->values[j] = values[i][j];
        n->sheep_candidate->rows[i] = n->sheep_row_candidate;
        n->sheep_row_candidate = NULL;
        ++n->sheep_rows_published;
        n->sheep_pc = publish_pcs[i];
    }
    n->sheep_a = n->sheep_candidate; /* DL.a publishes only at putstatic348. */
    n->sheep_candidate = NULL;
    n->sheep_pc = 351;
    n->sheep_phase = MC_ENTITY_CLASS_READY;
    return MC_MOB_OK;
failed:
    fail(e, entity_status(e->entity.status), e->entity.operation, "dl", "<clinit>", "()V",
         (int32_t)n->sheep_pc, true);
    n->sheep_phase = MC_ENTITY_CLASS_FAILED;
    n->sheep_failure = *e;
    e->first_class_failure = class_cause(&n->sheep_failure);
    return e->status;
}

static mc_mob_status skeleton_use(mc_entity_classes *classes, mc_registry_classes *registry,
                                  mc_mob_error *e) {
    mc_mob_class_namespace *n = classes->mobs;
    if (n != NULL && n->skeleton_phase == MC_ENTITY_CLASS_READY)
        return MC_MOB_OK;
    if (n != NULL && n->skeleton_phase == MC_ENTITY_CLASS_FAILED) {
        e->first_class_failure = class_cause(&n->failure);
        e->source_exception = n->failure.source_exception;
        e->exception_class = e->source_exception ? "java/lang/NoClassDefFoundError" : NULL;
        return fail(e, MC_MOB_CLASS_ERRONEOUS, "previous Skeleton class initialization failure",
                    "fr", "<clinit>", "()V", (int32_t)n->skeleton_pc, n->failure.native_policy);
    }
    if (n != NULL && n->skeleton_phase != MC_ENTITY_CLASS_COLD)
        return fail(e, MC_MOB_BUSY, "Skeleton class initializer active", "fr", "<clinit>", "()V",
                    -1, true);
    if (registry == NULL)
        return fail(e, MC_MOB_ARGUMENT, "Skeleton initializer requires its actual Item namespace",
                    "fr", "<clinit>", "()V", 4, true);
    if (mc_registry_classes_retain(registry, MC_REGISTRY_CLIENT, classes->profile, &e->registry) !=
        MC_REGISTRY_OK)
        return fail(e, MC_MOB_REGISTRY_FAILED, "Skeleton actual Item namespace occurrence", "fr",
                    "<clinit>", "()V", 4, true);
    /* This matched native validation pair cannot overlap an effectful call.
     * Registry remains borrowed from the caller through the real initializer. */
    (void)mc_registry_classes_release(registry, NULL);
    if (n == NULL) {
        n = class_namespace(classes, e, "fr");
        if (n == NULL)
            return e->status;
    }
    n->skeleton_phase = MC_ENTITY_CLASS_INITIALIZING;
    mc_mob_status status = MC_MOB_OK;
    const size_t bytes = mc_item_stack_object_bytes();
    if (classes->allocated_bytes > classes->limits.max_allocation_bytes ||
        bytes > classes->limits.max_allocation_bytes - classes->allocated_bytes) {
        fail(e, MC_MOB_LIMIT, "Skeleton actual Stack allocation budget", "fr", "<clinit>", "()V", 0,
             true);
        goto failed;
    }
    e->stack = mc_item_stack_allocate(&n->skeleton_candidate);
    if (e->stack != MC_ITEM_STACK_OK) {
        fail(e, e->stack == MC_ITEM_STACK_LIMIT ? MC_MOB_LIMIT : MC_MOB_ALLOCATION,
             "Skeleton actual RAW Stack allocation", "fr", "<clinit>", "()V", 0, true);
        goto failed;
    }
    classes->allocated_bytes += bytes;
    n->skeleton_stack_bytes = bytes;
    n->skeleton_pc = 0;
    const mc_item *bow = NULL;
    if (mc_registry_item_static_get(registry, "i", &bow, &e->registry) != MC_REGISTRY_OK) {
        n->skeleton_pc = 4;
        mc_registry_status original = e->registry.status;
        const bool previous = original == MC_REGISTRY_FAILED_INITIALIZATION;
        if (previous)
            original = registry->item_states[MC_ITEM_BASE].failure.status;
        const char *exception =
            original == MC_REGISTRY_NULL_REFERENCE ? "java/lang/NullPointerException"
            : original == MC_REGISTRY_INDEX_OUT_OF_RANGE
                ? "java/lang/ArrayIndexOutOfBoundsException"
            : original == MC_REGISTRY_ILLEGAL_ARGUMENT || original == MC_REGISTRY_DUPLICATE_BLOCK ||
                    original == MC_REGISTRY_DUPLICATE_STATISTIC
                ? "java/lang/IllegalArgumentException"
                : NULL;
        e->source_exception = exception != NULL;
        e->exception_class =
            e->source_exception && previous ? "java/lang/NoClassDefFoundError" : exception;
        if (e->registry.active_initializer_class != NULL)
            e->child = location(e->registry.active_initializer_class, "<clinit>", "()V",
                                e->registry.active_initializer_pc);
        fail(e, MC_MOB_REGISTRY_FAILED, "Skeleton actual GM.i active use", "fr", "<clinit>", "()V",
             4, !e->source_exception);
        goto failed;
    }
    n->skeleton_pc = 4;
    const mc_item_fields *fields = mc_item_fields_get(bow);
    if (fields == NULL) {
        e->source_exception = true;
        e->exception_class = "java/lang/NullPointerException";
        e->child = location("iz", "<init>", "(Lgm;I)V", 2);
        fail(e, MC_MOB_SOURCE_NULL, "actual Skeleton Stack Item.bf null receiver", "fr", "<clinit>",
             "()V", 8, false);
        n->skeleton_pc = 8;
        goto failed;
    }
    e->stack = mc_item_stack_construct(n->skeleton_candidate, fields->bf, 1, 0);
    if (e->stack != MC_ITEM_STACK_OK) {
        e->child = location("iz", "<init>", "(III)V", 0);
        fail(e, MC_MOB_STACK_FAILED, "actual Skeleton Bow Stack constructor", "fr", "<clinit>",
             "()V", 8, true);
        n->skeleton_pc = 8;
        goto failed;
    }
    n->skeleton_pc = 8;
    n->skeleton_a = n->skeleton_candidate; /* Actual FR.a11; native ownership transfers. */
    n->skeleton_candidate = NULL;
    n->skeleton_pc = 14;
    n->skeleton_phase = MC_ENTITY_CLASS_READY;
    goto done;
failed:
    status = class_latch(n, e);
done:
    return status;
}

mc_mob_status mc_mob_allocate(mc_entity_classes *classes, mc_registry_classes *registry,
                              const mc_entity_class_literal *literal, mc_entity **out,
                              mc_mob_error *out_error) {
    mc_mob_error local;
    mc_mob_error *e = out_error == NULL ? &local : out_error;
    result(e, MC_MOB_OK, NULL);
    mc_entity_class_literal_view view;
    if (classes == NULL || out == NULL || literal == NULL ||
        mc_entity_class_literal_inspect(literal, &view) != MC_ENTITY_OK || view.owner != classes ||
        view.source != MC_ENTITY_CLIENT || !mc_entity_is_mob_kind(view.kind))
        return result(e, MC_MOB_ARGUMENT, "Mob actual canonical client Class/output");
    e->entity.status = mc_entity_classes_retain(classes, MC_ENTITY_CLIENT, classes->profile);
    if (e->entity.status != MC_ENTITY_OK)
        return fail(e, entity_status(e->entity.status), "Mob Entity namespace occurrence",
                    source_class(view.kind), "<init>", "(Lfd;)V", -1, true);
    mc_mob_status status = MC_MOB_OK;
    if (mc_entity_base_class_initialize(classes, &e->entity) != MC_ENTITY_OK) {
        status = fail(e, entity_status(e->entity.status), e->entity.operation, "sn", "<clinit>",
                      "()V", 1, true);
        goto done;
    }
    if (view.kind == MC_ENTITY_CLASS_KIND_SKELETON) {
        status = skeleton_use(classes, registry, e);
        if (status != MC_MOB_OK)
            goto done;
    }
    if (view.kind == MC_ENTITY_CLASS_KIND_SHEEP) {
        status = sheep_use(classes, e);
        if (status != MC_MOB_OK)
            goto done;
    }
    if (mc_entity_allocate_canonical(classes, view.kind, "concrete Mob allocation", out,
                                     &e->entity) != MC_ENTITY_OK)
        status = fail(e, entity_status(e->entity.status), e->entity.operation,
                      source_class(view.kind), "<init>", "(Lfd;)V", -1, true);
done:
    (void)mc_entity_classes_release(classes);
    return status;
}

static mc_entity_watch_record *watch_record(const mc_entity *entity, int32_t index) {
    if (entity->watcher == NULL || entity->watcher->map == NULL)
        return NULL;
    mc_entity_watch_node *node = entity->watcher->map->head;
    for (size_t i = 0; node != NULL && i < entity->watcher->map->size; ++i, node = node->next)
        if (node->key == index)
            return node->record;
    return NULL;
}

mc_entity_status mc_mob_watcher_initialize(mc_entity *entity, mc_entity_error *error) {
    mc_mob_entity_state *s = entity->mob;
    const mc_entity_class_kind kind = entity->class_literal->kind;
    const char *cl = source_class(kind);
    const bool byte_hook = kind == MC_ENTITY_CLASS_KIND_CREEPER ||
                           kind == MC_ENTITY_CLASS_KIND_SLIME || kind == MC_ENTITY_CLASS_KIND_PIG ||
                           kind == MC_ENTITY_CLASS_KIND_SHEEP || kind == MC_ENTITY_CLASS_KIND_WOLF;
    s->watcher_hook_pc =
        kind == MC_ENTITY_CLASS_KIND_CREEPER || kind == MC_ENTITY_CLASS_KIND_SLIME ||
                kind == MC_ENTITY_CLASS_KIND_SHEEP || kind == MC_ENTITY_CLASS_KIND_WOLF
            ? 1u
            : 0u;
    if (!byte_hook)
        return MC_ENTITY_OK; /* Actual inherited LS.b() is a literal return. */
    const int8_t value = kind == MC_ENTITY_CLASS_KIND_CREEPER ? -1
                         : kind == MC_ENTITY_CLASS_KIND_SLIME ? 1
                                                              : 0;
    mc_entity_status status = mc_entity_construct_watch_byte(entity, 16, value, error);
    unsigned int caller = kind == MC_ENTITY_CLASS_KIND_CREEPER || kind == MC_ENTITY_CLASS_KIND_WOLF
                              ? 14u
                          : kind == MC_ENTITY_CLASS_KIND_PIG ? 10u
                                                             : 18u;
    if (status == MC_ENTITY_OK) {
        s->watcher_hook_pc = caller;
        if (kind == MC_ENTITY_CLASS_KIND_CREEPER) {
            status = mc_entity_construct_watch_byte(entity, 17, 0, error);
            caller = 27;
        } else if (kind == MC_ENTITY_CLASS_KIND_WOLF) {
            status = mc_entity_construct_watch_string(entity, 17, mc_player_empty_literal(), error);
            caller = 25;
            if (status == MC_ENTITY_OK) {
                s->watcher_hook_pc = caller;
                /* SN calls this hook before the LS constructor stores Y=10. */
                status = mc_entity_construct_watch_integer(entity, 18, s->living.Y, error);
                caller = 45;
            }
        }
    }
    if (status != MC_ENTITY_OK) {
        s->failure.entity = *error;
        s->failure.child = location("ud", "a", "(ILjava/lang/Object;)V", error->bytecode);
        fail(&s->failure, entity_status(status), error->operation, cl, "b", "()V", (int32_t)caller,
             true);
    } else {
        s->watcher_hook_pc = kind == MC_ENTITY_CLASS_KIND_CREEPER ? 30u
                             : kind == MC_ENTITY_CLASS_KIND_PIG   ? 13u
                             : kind == MC_ENTITY_CLASS_KIND_WOLF  ? 48u
                                                                  : 21u;
    }
    return status;
}

static const uint16_t spider_units[] = {'/', 'm', 'o', 'b', '/', 's', 'p', 'i',
                                        'd', 'e', 'r', '.', 'p', 'n', 'g'};
static const uint16_t zombie_units[] = {'/', 'm', 'o', 'b', '/', 'z', 'o', 'm',
                                        'b', 'i', 'e', '.', 'p', 'n', 'g'};
static const uint16_t skeleton_units[] = {'/', 'm', 'o', 'b', '/', 's', 'k', 'e', 'l',
                                          'e', 't', 'o', 'n', '.', 'p', 'n', 'g'};
static const uint16_t creeper_units[] = {'/', 'm', 'o', 'b', '/', 'c', 'r', 'e',
                                         'e', 'p', 'e', 'r', '.', 'p', 'n', 'g'};
static const uint16_t slime_units[] = {'/', 'm', 'o', 'b', '/', 's', 'l',
                                       'i', 'm', 'e', '.', 'p', 'n', 'g'};
static const uint16_t pig_units[] = {'/', 'm', 'o', 'b', '/', 'p', 'i', 'g', '.', 'p', 'n', 'g'};
static const uint16_t sheep_units[] = {'/', 'm', 'o', 'b', '/', 's', 'h',
                                       'e', 'e', 'p', '.', 'p', 'n', 'g'};
static const uint16_t cow_units[] = {'/', 'm', 'o', 'b', '/', 'c', 'o', 'w', '.', 'p', 'n', 'g'};
static const uint16_t chicken_units[] = {'/', 'm', 'o', 'b', '/', 'c', 'h', 'i',
                                         'c', 'k', 'e', 'n', '.', 'p', 'n', 'g'};
static const uint16_t wolf_units[] = {'/', 'm', 'o', 'b', '/', 'w', 'o',
                                      'l', 'f', '.', 'p', 'n', 'g'};
static const uint16_t squid_units[] = {'/', 'm', 'o', 'b', '/', 's', 'q',
                                       'u', 'i', 'd', '.', 'p', 'n', 'g'};
#define LITERAL(name)                                                                              \
    { SIZE_MAX, sizeof(name##_units) / sizeof(*name##_units), name##_units }
static const mc_player_string spider_texture = LITERAL(spider);
static const mc_player_string zombie_texture = LITERAL(zombie);
static const mc_player_string skeleton_texture = LITERAL(skeleton);
static const mc_player_string creeper_texture = LITERAL(creeper);
static const mc_player_string slime_texture = LITERAL(slime);
static const mc_player_string pig_texture = LITERAL(pig);
static const mc_player_string sheep_texture = LITERAL(sheep);
static const mc_player_string cow_texture = LITERAL(cow);
static const mc_player_string chicken_texture = LITERAL(chicken);
static const mc_player_string wolf_texture = LITERAL(wolf);
static const mc_player_string squid_texture = LITERAL(squid);
#undef LITERAL

static bool fp(mc_mob_error *e, const char *cl, const char *method, const char *descriptor,
               unsigned int pc) {
    if ((_mm_getcsr() & 0xffc0u) == 0x1f80u)
        return true;
    fail(e, MC_MOB_UNSUPPORTED_FP, "Mob source strict floating controls", cl, method, descriptor,
         (int32_t)pc, true);
    return false;
}

static bool slime_size(mc_entity *entity, int32_t size, mc_mob_error *e) {
    mc_mob_entity_state *s = entity->mob;
    mc_entity_watch_record *record = watch_record(entity, 16);
    if (record == NULL || record->type != 0) {
        e->child = location("ud", "b", "(ILjava/lang/Object;)V", record == NULL ? 19 : 22);
        fail(e, MC_MOB_ENTITY_FAILED, "actual Slime watcher Byte16 setter", "uw", "e", "(I)V", 15,
             true);
        return false;
    }
    const int8_t value = mc_java_i8_narrow(size);
    if (record->byte_value != value) {
        record->byte_value = value;
        record->dirty = true;
        entity->watcher->dirty = true;
    }
    if (!fp(e, "uw", "e", "(I)V", 23)) {
        return false;
    }
    entity->base.width = at_fmul(0.6f, (float)size);
    entity->base.height = at_fmul(0.6f, (float)size);
    s->living.Y = mc_java_i32_mul(size, size);
    mc_entity_source_position(entity, entity->base.x, entity->base.y, entity->base.z);
    return true;
}

mc_mob_status mc_mob_slime_set_size(mc_entity *entity, int32_t size, mc_mob_error *out) {
    mc_mob_error local;
    result(&local, MC_MOB_OK, NULL);
    if (!mc_entity_has_mob(entity) || !mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_SLIME))
        result(&local, MC_MOB_ARGUMENT, "Slime actual concrete size receiver");
    else if (entity->phase != MC_ENTITY_READY)
        result(&local, entity->phase == MC_ENTITY_CONSTRUCTING ? MC_MOB_BUSY : MC_MOB_PHASE,
               "Slime constructed size receiver");
    else
        (void)slime_size(entity, size, &local);
    if (out != NULL)
        *out = local;
    return local.status;
}

mc_mob_status mc_mob_sheep_set_color(mc_entity *entity, int32_t color, mc_mob_error *out) {
    mc_mob_error local;
    result(&local, MC_MOB_OK, NULL);
    if (!mc_entity_has_mob(entity) || !mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_SHEEP)) {
        result(&local, MC_MOB_ARGUMENT, "Sheep actual concrete color receiver");
    } else if (entity->phase != MC_ENTITY_READY) {
        result(&local, entity->phase == MC_ENTITY_CONSTRUCTING ? MC_MOB_BUSY : MC_MOB_PHASE,
               "Sheep constructed color receiver");
    } else {
        mc_entity_watch_record *record = watch_record(entity, 16);
        if (record == NULL || record->type != 0) {
            const int32_t child = entity->watcher == NULL || entity->watcher->map == NULL ? 8
                                  : record == NULL                                        ? 16
                                                                                          : 19;
            local.child = location("ud", "a", "(I)B", child);
            fail(&local, MC_MOB_ENTITY_FAILED, "actual Sheep watcher Byte16 getter", "dl", "e_",
                 "(I)V", 6, true);
        } else {
            const int32_t masked = ((int32_t)record->byte_value & 240) | (color & 15);
            const int8_t value = mc_java_i8_narrow(masked);
            if (record->byte_value != value) {
                record->byte_value = value;
                record->dirty = true;
                entity->watcher->dirty = true;
            }
        }
    }
    if (out != NULL)
        *out = local;
    return local.status;
}

static bool body(mc_entity *entity, mc_world_runtime *world) {
    mc_mob_entity_state *s = entity->mob;
    mc_mob_error *e = &s->failure;
    const mc_entity_class_kind kind = entity->class_literal->kind;
    if (mc_entity_construct_base(entity, world, &e->entity) != MC_ENTITY_OK) {
        if (e->status == MC_MOB_OK) {
            e->child = location("sn", "<init>", "(Lfd;)V", e->entity.bytecode);
            fail(e, entity_status(e->entity.status), e->entity.operation, "ls", "<init>", "(Lfd;)V",
                 2, true);
        }
        return false;
    }
    if (mc_entity_construct_living(entity, &s->living, &s->living_pc, &e->entity) != MC_ENTITY_OK) {
        fail(e,
             e->entity.profile == MC_JAVA_RANDOM_PROFILE_UNSUPPORTED_FP
                 ? MC_MOB_UNSUPPORTED_FP
                 : entity_status(e->entity.status),
             e->entity.operation, "ls", "<init>", "(Lfd;)V", e->entity.bytecode, true);
        return false;
    }
    if (kind != MC_ENTITY_CLASS_KIND_SLIME) {
        s->creature_attacking = false;
        s->creature_pc = 10; /* II.e7 / return10. */
        if (kind == MC_ENTITY_CLASS_KIND_SPIDER || kind == MC_ENTITY_CLASS_KIND_ZOMBIE ||
            kind == MC_ENTITY_CLASS_KIND_SKELETON || kind == MC_ENTITY_CLASS_KIND_CREEPER) {
            s->monster_damage = 2;
            s->monster_pc = 7;
            s->living.Y = 20;
            s->monster_pc = 16; /* GZ.Y13 / return16. */
        } else if (kind == MC_ENTITY_CLASS_KIND_SQUID) {
            s->water_creature_pc = 5; /* AR invokes II2, then return5. */
        } else {
            s->animal_pc = 5; /* BG invokes II2, then return5. */
        }
    }
    switch (kind) {
        case MC_ENTITY_CLASS_KIND_SPIDER:
            s->living.O = &spider_texture;
            s->constructor_pc = 8;
            entity->base.width = 1.4f;
            entity->base.height = 0.9f;
            s->constructor_pc = 16;
            s->living.aB = 0.8f;
            s->constructor_pc = 25;
            return true;
        case MC_ENTITY_CLASS_KIND_ZOMBIE:
            s->living.O = &zombie_texture;
            s->constructor_pc = 8;
            s->living.aB = 0.5f;
            s->constructor_pc = 14;
            s->monster_damage = 5;
            s->constructor_pc = 22;
            return true;
        case MC_ENTITY_CLASS_KIND_SKELETON:
            s->living.O = &skeleton_texture;
            s->constructor_pc = 11;
            return true;
        case MC_ENTITY_CLASS_KIND_CREEPER:
            s->living.O = &creeper_texture;
            s->constructor_pc = 11;
            return true;
        case MC_ENTITY_CLASS_KIND_SLIME: {
            s->slime_jump_delay = 0;
            s->constructor_pc = 7;
            s->living.O = &slime_texture;
            s->constructor_pc = 13;
            int32_t size_draw, delay_draw;
            if (!mc_java_random_next_bounded(entity->random, 3, &size_draw)) {
                fail(e, MC_MOB_ENTITY_FAILED, "actual Slime own Random size draw", "uw", "<init>",
                     "(Lfd;)V", 22, true);
                return false;
            }
            const int32_t size = mc_java_i32_shl(1, size_draw);
            entity->base.offset = 0.0f;
            s->constructor_pc = 29;
            if (!mc_java_random_next_bounded(entity->random, 20, &delay_draw)) {
                fail(e, MC_MOB_ENTITY_FAILED, "actual Slime own Random jump draw", "uw", "<init>",
                     "(Lfd;)V", 39, true);
                return false;
            }
            s->slime_jump_delay = mc_java_i32_add(delay_draw, 10);
            s->constructor_pc = 45;
            if (!slime_size(entity, size, e)) {
                if (e->status == MC_MOB_UNSUPPORTED_FP) {
                    e->child = e->site;
                    e->site = location("uw", "<init>", "(Lfd;)V", 50);
                }
                return false;
            }
            s->constructor_pc = 53;
            return true;
        }
        case MC_ENTITY_CLASS_KIND_PIG:
            s->living.O = &pig_texture;
            s->constructor_pc = 8;
            entity->base.width = 0.9f;
            entity->base.height = 0.9f;
            s->constructor_pc = 19;
            return true;
        case MC_ENTITY_CLASS_KIND_SHEEP:
            s->living.O = &sheep_texture;
            s->constructor_pc = 8;
            entity->base.width = 0.9f;
            entity->base.height = 1.3f;
            s->constructor_pc = 19;
            return true;
        case MC_ENTITY_CLASS_KIND_COW:
            s->living.O = &cow_texture;
            s->constructor_pc = 8;
            entity->base.width = 0.9f;
            entity->base.height = 1.3f;
            s->constructor_pc = 19;
            return true;
        case MC_ENTITY_CLASS_KIND_CHICKEN: {
            s->chicken.a = false;
            s->constructor_pc = 7;
            s->chicken.b = 0.0f;
            s->constructor_pc = 12;
            s->chicken.c = 0.0f;
            s->constructor_pc = 17;
            s->chicken.h = 1.0f;
            s->constructor_pc = 22;
            s->living.O = &chicken_texture;
            s->constructor_pc = 28;
            entity->base.width = 0.3f;
            entity->base.height = 0.4f;
            s->constructor_pc = 36;
            s->living.Y = 4;
            s->constructor_pc = 41;
            int32_t draw;
            if (!mc_java_random_next_bounded(entity->random, 6000, &draw)) {
                fail(e, MC_MOB_ENTITY_FAILED, "actual Chicken own Random egg draw", "ww", "<init>",
                     "(Lfd;)V", 52, true);
                return false;
            }
            s->chicken.i = mc_java_i32_add(draw, 6000);
            s->constructor_pc = 62;
            return true;
        }
        case MC_ENTITY_CLASS_KIND_WOLF:
            s->wolf.a = false;
            s->constructor_pc = 7;
            s->living.O = &wolf_texture;
            s->constructor_pc = 13;
            entity->base.width = 0.8f;
            entity->base.height = 0.8f;
            s->constructor_pc = 21;
            s->living.aB = 1.1f;
            s->constructor_pc = 27;
            s->living.Y = 8;
            s->constructor_pc = 36;
            return true;
        case MC_ENTITY_CLASS_KIND_SQUID: {
            s->squid = (mc_mob_squid_fields){0}; /* XT's fourteen zero stores7..72. */
            s->constructor_pc = 72;
            s->living.O = &squid_texture;
            s->constructor_pc = 78;
            entity->base.width = 0.95f;
            entity->base.height = 0.95f;
            s->constructor_pc = 86;
            if (!fp(e, "xt", "<init>", "(Lfd;)V", 95))
                return false;
            const float draw = mc_java_random_next_float(entity->random);
            s->squid.l = at_fmul(at_fdiv(1.0f, at_fadd(draw, 1.0f)), 0.2f);
            s->constructor_pc = 107;
            return true;
        }
        default:
            return false;
    }
}

mc_mob_status mc_mob_construct(mc_entity *entity, mc_world_runtime *world, mc_mob_error *out) {
    mc_mob_error local;
    result(&local, MC_MOB_OK, NULL);
    if (!mc_entity_has_mob(entity)) {
        result(&local, MC_MOB_ARGUMENT, "actual concrete Mob constructor receiver");
    } else if (entity->phase != MC_ENTITY_RAW) {
        result(&local, entity->phase == MC_ENTITY_CONSTRUCTING ? MC_MOB_BUSY : MC_MOB_PHASE,
               "Mob one-shot RAW constructor admission");
    } else if (world != NULL && mc_world_runtime_entities(world) != entity->classes) {
        result(&local, MC_MOB_ARGUMENT, "Mob exact World Entity namespace");
    } else {
        const mc_entity_class_kind kind = entity->class_literal->kind;
        const mc_entity_status status = mc_entity_constructor_begin(
            entity, kind, world, "Mob constructor", "Mob exact World profile", &local.entity);
        if (status != MC_ENTITY_OK) {
            fail(&local, entity_status(status), local.entity.operation, source_class(kind),
                 "<init>", "(Lfd;)V", -1, true);
        } else {
            result(&entity->mob->failure, MC_MOB_OK, NULL);
            const bool returned = body(entity, world);
            entity->phase = returned ? MC_ENTITY_READY : MC_ENTITY_FAILED_CONSTRUCTION;
            local = entity->mob->failure;
        }
    }
    if (out != NULL)
        *out = local;
    return local.status;
}

bool mc_mob_inspect(const mc_entity *entity, mc_mob_state *out) {
    if (!mc_entity_has_mob(entity) || out == NULL)
        return false;
    const mc_mob_entity_state *s = entity->mob;
    *out = (mc_mob_state){.kind = entity->class_literal->kind,
                          .phase = entity->phase,
                          .living = s->living,
                          .creature_path = s->creature_path,
                          .creature_target = s->creature_target,
                          .creature_attacking = s->creature_attacking,
                          .monster_damage = s->monster_damage,
                          .creeper_a = s->creeper_a,
                          .creeper_b = s->creeper_b,
                          .slime_jump_delay = s->slime_jump_delay,
                          .slime_a = s->slime_a,
                          .slime_b = s->slime_b,
                          .chicken = s->chicken,
                          .wolf = s->wolf,
                          .squid = s->squid,
                          .living_pc = s->living_pc,
                          .creature_pc = s->creature_pc,
                          .monster_pc = s->monster_pc,
                          .constructor_pc = s->constructor_pc,
                          .watcher_hook_pc = s->watcher_hook_pc,
                          .animal_pc = s->animal_pc,
                          .water_creature_pc = s->water_creature_pc,
                          .failure = &s->failure};
    const mc_entity_watch_record *a = watch_record(entity, 16), *b = watch_record(entity, 17),
                                 *c = watch_record(entity, 18);
    out->watcher_16_present = a != NULL;
    out->watcher_17_present = b != NULL;
    if (a != NULL) {
        out->watcher_16_type = a->type;
        out->watcher_16 = a->byte_value;
        out->watcher_16_dirty = a->dirty;
    }
    if (b != NULL) {
        out->watcher_17_type = b->type;
        out->watcher_17 = b->byte_value;
        out->watcher_17_string = b->string_value;
        out->watcher_17_dirty = b->dirty;
    }
    out->watcher_18_present = c != NULL;
    if (c != NULL) {
        out->watcher_18_type = c->type;
        out->watcher_18_integer = c->integer_value;
        out->watcher_18_dirty = c->dirty;
    }
    out->watcher_dirty = entity->watcher != NULL && entity->watcher->dirty;
    return true;
}

bool mc_mob_classes_inspect(const mc_entity_classes *classes, mc_mob_class_state *out) {
    if (classes == NULL || out == NULL)
        return false;
    *out = (mc_mob_class_state){0};
    const mc_mob_class_namespace *n = classes->mobs;
    if (n != NULL) {
        out->skeleton_phase = n->skeleton_phase;
        out->skeleton_pc = n->skeleton_pc;
        out->skeleton_a = n->skeleton_a;
        out->skeleton_candidate = n->skeleton_candidate;
        out->allocation_bytes = sizeof(*n) + n->skeleton_stack_bytes + n->sheep_array_bytes;
        out->skeleton_stack_bytes = n->skeleton_stack_bytes;
        out->failure = &n->failure;
        out->sheep_phase = n->sheep_phase;
        out->sheep_pc = n->sheep_pc;
        out->sheep_a = n->sheep_a;
        out->sheep_candidate = n->sheep_candidate;
        out->sheep_row_candidate = n->sheep_row_candidate;
        out->sheep_rows_published = n->sheep_rows_published;
        out->sheep_array_bytes = n->sheep_array_bytes;
        out->sheep_failure = &n->sheep_failure;
    }
    return true;
}

mc_mob_status mc_mob_retire(mc_entity **slot, mc_mob_error *error) {
    if (slot == NULL)
        return result(error, MC_MOB_ARGUMENT, "Mob terminal occurrence slot");
    if (*slot == NULL)
        return result(error, MC_MOB_OK, NULL);
    mc_entity *entity = *slot;
    if (!mc_entity_has_mob(entity))
        return result(error, MC_MOB_ARGUMENT, "Mob terminal concrete occurrence");
    if (entity->phase == MC_ENTITY_CONSTRUCTING || entity->holds != 1 || entity->mob->tick_active ||
        entity->mob->pending_ray != NULL || entity->mob->pending_path != NULL)
        return result(error, MC_MOB_BUSY, "Mob active/external terminal occurrence");
    mc_entity_release(entity);
    *slot = NULL;
    return result(error, MC_MOB_OK, NULL);
}
