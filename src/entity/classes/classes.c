#include "../bounds/bounds_internal.h"
#include "../minecart/classes_internal.h"
#include "../vector/vector_internal.h"
#include "classes_internal.h"
#include "memory_internal.h"

#include <stdlib.h>

/* Paired jc/ga Class literals followed by query-only sn/lq and gs/em, then
 * client-only dc. These
 * static bytes are diagnostics, not the namespace's String values or Class keys. */
static const char *const literal_names[MC_ENTITY_CLASS_KIND_COUNT][2] = {
    {"sl", "lp"}, {"by", "be"}, {"hl", "ez"}, {"qv", "kf"}, {"ls", "hl"}, {"gz", "ep"},
    {"gb", "ec"}, {"fr", "dv"}, {"cn", "bn"}, {"nt", "ip"}, {"uz", "ne"}, {"uw", "nb"},
    {"bp", "aw"}, {"ya", "pi"}, {"wh", "oc"}, {"dl", "ce"}, {"bx", "bd"}, {"ww", "or"},
    {"xt", "pf"}, {"gi", "eh"}, {"qw", "kg"}, {"ju", "gl"}, {"yl", "pr"}, {"fz", "eb"},
    {"sn", "lq"}, {"gs", "em"}, {"dc", NULL}, {"xw", NULL}, {"em", NULL}, {"vv", NULL},
    {"cf", NULL}, {"lx", NULL}, {"c", NULL}};

/* Nearest admitted superclass from the paired class declarations. Monster
 * gz/ep reaches Living ls/hl through ii/fp; animals through bg/ao -> ii/fp;
 * Squid xt/pf through ar/ae -> ii/fp; Ghast bp/aw through wq/ok. Player gs/em
 * directly extends ls/hl. PigZombie ya/pi extends Zombie uz/ne -> gz/ep.
 * Entity sn/lq has no parent in this admitted namespace. COUNT is a sentinel,
 * never a Class literal. No interface or dynamically loaded Class is admitted. */
static const mc_entity_class_kind literal_parents[MC_ENTITY_CLASS_KIND_COUNT] = {
    [MC_ENTITY_CLASS_KIND_ARROW] = MC_ENTITY_CLASS_KIND_ENTITY,
    [MC_ENTITY_CLASS_KIND_SNOWBALL] = MC_ENTITY_CLASS_KIND_ENTITY,
    [MC_ENTITY_CLASS_KIND_ITEM] = MC_ENTITY_CLASS_KIND_ENTITY,
    [MC_ENTITY_CLASS_KIND_PAINTING] = MC_ENTITY_CLASS_KIND_ENTITY,
    [MC_ENTITY_CLASS_KIND_LIVING_ENTITY] = MC_ENTITY_CLASS_KIND_ENTITY,
    [MC_ENTITY_CLASS_KIND_MONSTER] = MC_ENTITY_CLASS_KIND_LIVING_ENTITY,
    [MC_ENTITY_CLASS_KIND_CREEPER] = MC_ENTITY_CLASS_KIND_MONSTER,
    [MC_ENTITY_CLASS_KIND_SKELETON] = MC_ENTITY_CLASS_KIND_MONSTER,
    [MC_ENTITY_CLASS_KIND_SPIDER] = MC_ENTITY_CLASS_KIND_MONSTER,
    [MC_ENTITY_CLASS_KIND_GIANT] = MC_ENTITY_CLASS_KIND_MONSTER,
    [MC_ENTITY_CLASS_KIND_ZOMBIE] = MC_ENTITY_CLASS_KIND_MONSTER,
    [MC_ENTITY_CLASS_KIND_SLIME] = MC_ENTITY_CLASS_KIND_LIVING_ENTITY,
    [MC_ENTITY_CLASS_KIND_GHAST] = MC_ENTITY_CLASS_KIND_LIVING_ENTITY,
    [MC_ENTITY_CLASS_KIND_PIG_ZOMBIE] = MC_ENTITY_CLASS_KIND_ZOMBIE,
    [MC_ENTITY_CLASS_KIND_PIG] = MC_ENTITY_CLASS_KIND_LIVING_ENTITY,
    [MC_ENTITY_CLASS_KIND_SHEEP] = MC_ENTITY_CLASS_KIND_LIVING_ENTITY,
    [MC_ENTITY_CLASS_KIND_COW] = MC_ENTITY_CLASS_KIND_LIVING_ENTITY,
    [MC_ENTITY_CLASS_KIND_CHICKEN] = MC_ENTITY_CLASS_KIND_LIVING_ENTITY,
    [MC_ENTITY_CLASS_KIND_SQUID] = MC_ENTITY_CLASS_KIND_LIVING_ENTITY,
    [MC_ENTITY_CLASS_KIND_WOLF] = MC_ENTITY_CLASS_KIND_LIVING_ENTITY,
    [MC_ENTITY_CLASS_KIND_PRIMED_TNT] = MC_ENTITY_CLASS_KIND_ENTITY,
    [MC_ENTITY_CLASS_KIND_FALLING_SAND] = MC_ENTITY_CLASS_KIND_ENTITY,
    [MC_ENTITY_CLASS_KIND_MINECART] = MC_ENTITY_CLASS_KIND_ENTITY,
    [MC_ENTITY_CLASS_KIND_BOAT] = MC_ENTITY_CLASS_KIND_ENTITY,
    [MC_ENTITY_CLASS_KIND_ENTITY] = MC_ENTITY_CLASS_KIND_COUNT,
    [MC_ENTITY_CLASS_KIND_PLAYER] = MC_ENTITY_CLASS_KIND_LIVING_ENTITY,
    [MC_ENTITY_CLASS_KIND_LOCAL_PLAYER] = MC_ENTITY_CLASS_KIND_PLAYER,
    [MC_ENTITY_CLASS_KIND_PARTICLE] = MC_ENTITY_CLASS_KIND_ENTITY,
    [MC_ENTITY_CLASS_KIND_PICKUP_PARTICLE] = MC_ENTITY_CLASS_KIND_PARTICLE,
    [MC_ENTITY_CLASS_KIND_VV] = MC_ENTITY_CLASS_KIND_ENTITY,
    [MC_ENTITY_CLASS_KIND_CF] = MC_ENTITY_CLASS_KIND_ENTITY,
    [MC_ENTITY_CLASS_KIND_LX] = MC_ENTITY_CLASS_KIND_ENTITY,
    [MC_ENTITY_CLASS_KIND_C] = MC_ENTITY_CLASS_KIND_ENTITY};

mc_entity_status mc_entity_base_class_initialize(mc_entity_classes *classes,
                                                 mc_entity_error *error) {
    if (classes->entity_phase == MC_ENTITY_CLASS_READY)
        return MC_ENTITY_OK;
    if (classes->entity_phase == MC_ENTITY_CLASS_FAILED)
        return mc_entity_error_set(error, MC_ENTITY_CLASS_FAILED_STATUS,
                                   "entity class initialization", 0);
    if (classes->entity_phase != MC_ENTITY_CLASS_COLD)
        return mc_entity_error_set(error, MC_ENTITY_STATE_ERROR, "entity class initialization", 0);
    /* sn/lq.<clinit>: the sole effect is the shared plain int counter store. */
    classes->entity_phase = MC_ENTITY_CLASS_INITIALIZING;
    classes->next_id = 0;
    classes->entity_phase = MC_ENTITY_CLASS_READY;
    return MC_ENTITY_OK;
}

mc_entity_status mc_entity_classes_create(mc_entity_source source, mc_java_random_profile *profile,
                                          const mc_entity_limits *limits, mc_entity_classes **out) {
    if ((source != MC_ENTITY_CLIENT && source != MC_ENTITY_SERVER) || profile == NULL ||
        out == NULL)
        return MC_ENTITY_INVALID_ARGUMENT;
    const mc_entity_limits selected =
        limits == NULL ? (mc_entity_limits){MC_ENTITY_DEFAULT_ALLOCATION_BYTES, SIZE_MAX} : *limits;
    if (selected.max_allocation_bytes < sizeof(mc_entity_classes))
        return MC_ENTITY_LIMIT;
    mc_entity_classes *classes = malloc(sizeof(*classes));
    if (classes == NULL)
        return MC_ENTITY_ALLOCATION_FAILED;
    *classes = (mc_entity_classes){.source = source,
                                   .profile = profile,
                                   .limits = selected,
                                   .allocated_bytes = sizeof(*classes)};
    for (size_t index = 0; index < (size_t)MC_ENTITY_CLASS_KIND_COUNT; ++index)
        classes->literals[index] =
            (mc_entity_class_literal){.owner = classes,
                                      .source = source,
                                      .kind = (mc_entity_class_kind)index,
                                      .client_name = literal_names[index][0],
                                      .server_name = literal_names[index][1]};
    *out = classes;
    return MC_ENTITY_OK;
}

mc_entity_status mc_entity_classes_get_snapshot(const mc_entity_classes *classes,
                                                mc_entity_class_snapshot *out) {
    if (classes == NULL || out == NULL)
        return MC_ENTITY_INVALID_ARGUMENT;
    *out = (mc_entity_class_snapshot){.source = classes->source,
                                      .entity_class = classes->entity_phase,
                                      .bounds_class = classes->bounds_phase,
                                      .watcher_class = classes->watcher_phase,
                                      .next_id = classes->next_id,
                                      .bounds_pool_cursor = classes->bounds_cursor,
                                      .bounds_pool_identity = classes->bounds_pool,
                                      .watcher_types_identity = classes->types,
                                      .watcher_type_count =
                                          classes->types == NULL ? 0 : classes->types->size,
                                      .live_entities = classes->live_entities,
                                      .allocation_bytes = classes->allocated_bytes};
    return MC_ENTITY_OK;
}

mc_entity_status mc_entity_classes_retain(mc_entity_classes *classes, mc_entity_source source,
                                          mc_java_random_profile *profile) {
    if (classes == NULL || classes->source != source || classes->profile != profile)
        return MC_ENTITY_INVALID_ARGUMENT;
    if (classes->borrowers == SIZE_MAX)
        return MC_ENTITY_LIMIT;
    ++classes->borrowers;
    return MC_ENTITY_OK;
}

mc_entity_status mc_entity_classes_runtime_release_admit(const mc_entity_classes *classes) {
    if (classes == NULL)
        return MC_ENTITY_INVALID_ARGUMENT;
    if (classes->borrowers == 0)
        return MC_ENTITY_STATE_ERROR;
    return MC_ENTITY_OK;
}

mc_entity_status mc_entity_classes_release(mc_entity_classes *classes) {
    const mc_entity_status status = mc_entity_classes_runtime_release_admit(classes);
    if (status != MC_ENTITY_OK)
        return status;
    --classes->borrowers;
    return status;
}

static void class_retire(mc_entity_classes *classes, void *storage, size_t bytes) {
    if (storage != NULL) {
        free(storage);
        classes->allocated_bytes -= bytes;
    }
}

mc_entity_status mc_entity_classes_free(mc_entity_classes *classes) {
    if (classes == NULL)
        return MC_ENTITY_OK;
    if (classes->live_entities != 0 || classes->borrowers != 0 || classes->bounds_holds != 0 ||
        classes->vector_holds != 0)
        return MC_ENTITY_BUSY;
    mc_minecart_class_dispose(classes);
    mc_entity_registry_dispose(classes);
    if (classes->types != NULL) {
        mc_entity_type_entry *entry = classes->types->head;
        while (entry != NULL) {
            mc_entity_type_entry *next = entry->next;
            class_retire(classes, entry, sizeof(*entry));
            entry = next;
        }
        class_retire(classes, classes->types, sizeof(*classes->types));
    }
    mc_entity_bounds_dispose(classes);
    mc_entity_vector_dispose(classes);
    free(classes);
    return MC_ENTITY_OK;
}

mc_entity_status mc_entity_class_literal_get(const mc_entity_classes *classes,
                                             mc_entity_class_kind kind,
                                             const mc_entity_class_literal **out) {
    if (classes == NULL || out == NULL ||
        (unsigned int)kind >= (unsigned int)MC_ENTITY_CLASS_KIND_COUNT ||
        ((kind == MC_ENTITY_CLASS_KIND_LOCAL_PLAYER || kind == MC_ENTITY_CLASS_KIND_PARTICLE ||
          kind == MC_ENTITY_CLASS_KIND_PICKUP_PARTICLE || kind >= MC_ENTITY_CLASS_KIND_VV) &&
         classes->source != MC_ENTITY_CLIENT))
        return MC_ENTITY_INVALID_ARGUMENT;
    *out = &classes->literals[(size_t)kind];
    return MC_ENTITY_OK;
}

mc_entity_status mc_entity_class_literal_inspect(const mc_entity_class_literal *literal,
                                                 mc_entity_class_literal_view *out) {
    if (literal == NULL || out == NULL)
        return MC_ENTITY_INVALID_ARGUMENT;
    *out = (mc_entity_class_literal_view){.owner = literal->owner,
                                          .source = literal->source,
                                          .kind = literal->kind,
                                          .client_name = literal->client_name,
                                          .server_name = literal->server_name};
    return MC_ENTITY_OK;
}

static bool literal_is_canonical(const mc_entity_class_literal *literal) {
    return literal != NULL && literal->owner != NULL &&
           (literal->source == MC_ENTITY_CLIENT || literal->source == MC_ENTITY_SERVER) &&
           literal->source == literal->owner->source &&
           ((literal->kind != MC_ENTITY_CLASS_KIND_LOCAL_PLAYER &&
             literal->kind != MC_ENTITY_CLASS_KIND_PARTICLE &&
             literal->kind != MC_ENTITY_CLASS_KIND_PICKUP_PARTICLE &&
             literal->kind < MC_ENTITY_CLASS_KIND_VV) ||
            literal->source == MC_ENTITY_CLIENT) &&
           (unsigned int)literal->kind < (unsigned int)MC_ENTITY_CLASS_KIND_COUNT &&
           literal == &literal->owner->literals[(size_t)literal->kind];
}

mc_entity_status
mc_entity_class_literal_is_assignable_from(const mc_entity_class_literal *target,
                                           const mc_entity_class_literal *candidate, bool *out) {
    if (out == NULL || !literal_is_canonical(target) || !literal_is_canonical(candidate))
        return MC_ENTITY_INVALID_ARGUMENT;
    if (target->owner != candidate->owner) {
        *out = false;
        return MC_ENTITY_OK;
    }
    const mc_entity_class_literal *current = candidate;
    for (size_t depth = 0; depth < (size_t)MC_ENTITY_CLASS_KIND_COUNT; ++depth) {
        if (!literal_is_canonical(current))
            return MC_ENTITY_INVALID_ARGUMENT;
        if (target == current) {
            *out = true;
            return MC_ENTITY_OK;
        }
        const mc_entity_class_kind parent = literal_parents[(size_t)current->kind];
        if (parent == MC_ENTITY_CLASS_KIND_COUNT) {
            *out = false;
            return MC_ENTITY_OK;
        }
        current = &candidate->owner->literals[(size_t)parent];
    }
    return MC_ENTITY_INVALID_ARGUMENT;
}
