#include "entity_tick_internal.h"
#include <string.h>

static mc_world_entity_list_ledger *owner(wt_scope *s, mc_world_reference_list *list,
                                          unsigned int pc) {
    mc_world_entity_list_ledger *all[] = {
        &s->world->entity_membership, &s->world->entity_deferral, &s->world->player_membership,
        &s->world->weather_membership, &s->world->nearby_entities};
    if (list != NULL)
        for (size_t i = 0; i < sizeof(all) / sizeof(all[0]); ++i) {
            mc_world_entity_list_ledger *ledger = all[i];
            if (list != ledger->list)
                continue;
            if (list->count > list->capacity || list->capacity > (size_t)INT32_MAX ||
                list->capacity > SIZE_MAX / (sizeof(*list->items) + sizeof(*ledger->entries)) ||
                ledger->count != list->count || ledger->capacity != list->capacity ||
                ((list->capacity == 0) != (list->items == NULL)) ||
                ((ledger->capacity == 0) != (ledger->entries == NULL)) ||
                (list->items != NULL && (void *)list->items == (void *)ledger->entries))
                break;
            return ledger;
        }
    s->error.bytecode = pc;
    wt_fail(s, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World reached Entity List owner");
    return NULL;
}

bool wt_list_size(wt_scope *s, mc_world_reference_list *list, unsigned int pc, size_t *out) {
    if (owner(s, list, pc) == NULL)
        return false;
    *out = list->count;
    return true;
}

static bool row(wt_scope *s, mc_world_entity_list_ledger *ledger, size_t index, unsigned int pc,
                mc_entity **out) {
    if (index >= ledger->count || ledger->list->items[index] != ledger->entries[index]) {
        s->error.bytecode = pc;
        return wt_fail(s, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                       "World reached Entity occurrence provenance");
    }
    *out = ledger->entries[index];
    return true;
}

bool wt_list_get(wt_scope *s, mc_world_reference_list *list, int32_t index, unsigned int get_pc,
                 unsigned int cast_pc, mc_entity **out) {
    mc_world_entity_list_ledger *ledger = owner(s, list, get_pc);
    if (ledger == NULL)
        return false;
    if (index < 0 || (size_t)index >= list->count) {
        s->error.bytecode = get_pc;
        return wt_fail(s, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World Entity List index");
    }
    mc_entity *entity;
    if (!row(s, ledger, (size_t)index, cast_pc, &entity))
        return false;
    if (entity != NULL &&
        (s->world->classes == NULL || entity->classes != s->world->classes->entities)) {
        s->error.bytecode = cast_pc;
        return wt_fail(s, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                       "World Entity List source class cast");
    }
    if (!wt_hold(s, entity, cast_pc))
        return false;
    *out = entity;
    return true;
}

bool wt_list_remove(wt_scope *s, mc_world_reference_list *list, int32_t index, unsigned int pc) {
    mc_world_entity_list_ledger *ledger = owner(s, list, pc);
    if (ledger == NULL)
        return false;
    if (index < 0 || (size_t)index >= list->count) {
        s->error.bytecode = pc;
        return wt_fail(s, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World remove current List index");
    }
    mc_entity *victim;
    if (!row(s, ledger, (size_t)index, pc, &victim))
        return false;
    const size_t i = (size_t)index;
    memmove(list->items + i, list->items + i + 1, (list->count - i - 1) * sizeof(*list->items));
    memmove(ledger->entries + i, ledger->entries + i + 1,
            (ledger->count - i - 1) * sizeof(*ledger->entries));
    --list->count;
    --ledger->count;
    list->items[list->count] = NULL;
    ledger->entries[ledger->count] = NULL;
    mc_entity_release(victim);
    return true;
}

bool wt_list_clear(wt_scope *s, mc_world_reference_list *list, unsigned int pc) {
    mc_world_entity_list_ledger *ledger = owner(s, list, pc);
    if (ledger == NULL)
        return false;
    /* No source callback can run during release, but prove every reached
     * occurrence before clearing this actual owning collection. */
    for (size_t i = 0; i < list->count; ++i) {
        mc_entity *p;
        if (!row(s, ledger, i, pc, &p))
            return false;
    }
    const size_t count = list->count;
    list->count = ledger->count = 0;
    for (size_t i = 0; i < count; ++i) {
        mc_entity *p = ledger->entries[i];
        list->items[i] = NULL;
        ledger->entries[i] = NULL;
        mc_entity_release(p);
    }
    return true;
}

static bool equal(const mc_entity *a, const mc_entity *b) {
    return a == NULL ? b == NULL
                     : b != NULL && a->classes == b->classes && a->base.id == b->base.id;
}

bool wt_list_remove_all(wt_scope *s, mc_world_reference_list *list,
                        mc_world_reference_list *collection, unsigned int pc) {
    mc_world_entity_list_ledger *target = owner(s, list, pc);
    if (target == NULL)
        return false;
    if (collection == NULL) {
        s->error.bytecode = pc;
        return wt_fail(s, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World removeAll null Collection");
    }
    const size_t count = list->count;
    if (count == 0)
        return true; /* Objects.requireNonNull(c) still preceded this. */
    mc_world_entity_list_ledger *source = owner(s, collection, pc);
    if (source == NULL)
        return false;
    mc_entity **victims = wt_allocate(s, count * sizeof(*victims), pc);
    if (victims == NULL)
        return false;
    size_t read = 0, write = 0, removed = 0;
    for (; read < count && s->error.status == MC_WORLD_STORAGE_OK; ++read) {
        mc_entity *candidate;
        if (!row(s, target, read, pc, &candidate))
            break;
        bool contains = false;
        /* Actual ArrayList.contains invokes the key's equals on each value.
         * Neither IDs nor duplicate occurrences are pre-deduplicated. */
        for (size_t j = 0; j < collection->count; ++j) {
            s->error.comparison_index = j;
            if (!wt_work(s, pc))
                break;
            mc_entity *other;
            if (!row(s, source, j, pc, &other))
                break;
            if (equal(candidate, other)) {
                contains = true;
                break;
            }
        }
        if (s->error.status != MC_WORLD_STORAGE_OK)
            break;
        if (contains)
            victims[removed++] = candidate;
        else {
            list->items[write] = candidate;
            target->entries[write] = candidate;
            ++write;
        }
    }
    /* JDK ArrayList.batchRemove's exception/finally prefix retains the
     * unprocessed tail and commits the removals already selected. */
    if (read < count) {
        memmove(list->items + write, list->items + read, (count - read) * sizeof(*list->items));
        memmove(target->entries + write, target->entries + read,
                (count - read) * sizeof(*target->entries));
        write += count - read;
    }
    list->count = target->count = write;
    for (size_t i = write; i < count; ++i) {
        list->items[i] = NULL;
        target->entries[i] = NULL;
    }
    for (size_t i = 0; i < removed; ++i)
        mc_entity_release(victims[i]);
    wt_free(s, victims, count * sizeof(*victims));
    return s->error.status == MC_WORLD_STORAGE_OK;
}
