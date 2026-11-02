#include "../internal.h"
#include "terminal_internal.h"

#include <string.h>

/* At most 46 Slot owners and thirteen other allocations per Container, plus
 * IX's three allocations. No allocation, source call or owner mutation is
 * permitted while constructing this proof. Stack occurrences may alias. */
typedef struct terminal_proof {
    const void *allocations[128];
    size_t sizes[128], count;
    mc_inventory_error error;
} terminal_proof;

static bool refuse(terminal_proof *p, mc_inventory_status status, const char *operation) {
    p->error = (mc_inventory_error){.status = status, .operation = operation};
    return false;
}

static bool overlaps(const void *a, size_t an, const void *b, size_t bn) {
    const uintptr_t ap = (uintptr_t)a, bp = (uintptr_t)b;
    return ap <= bp ? bp - ap < an : ap - bp < bn;
}

static bool budget_shape(const inv_budget *b) {
    return b->limit >= 256u && b->limit <= 1073741824u && b->max_slots != 0 &&
           b->max_slots <= 1048576u && b->used != 0 && b->used <= b->limit;
}

static bool allocation(terminal_proof *p, const void *value, size_t size, size_t *total) {
    if (value == NULL || p->count == sizeof p->allocations / sizeof *p->allocations)
        return refuse(p, MC_INVENTORY_INVALID_ARGUMENT, "Inventory terminal native storage");
    for (size_t i = 0; i < p->count; ++i)
        if (overlaps(value, size, p->allocations[i], p->sizes[i]))
            return refuse(p, MC_INVENTORY_INVALID_ARGUMENT,
                          "Inventory terminal aliased owned storage");
    size_t bytes;
    if (!inv_allocation_bytes(value, size, &bytes) || bytes > SIZE_MAX - *total)
        return refuse(p, MC_INVENTORY_INVALID_ARGUMENT,
                      "Inventory terminal native allocation extent");
    p->allocations[p->count] = value;
    p->sizes[p->count++] = size;
    *total += bytes;
    return true;
}

static bool list(terminal_proof *p, const inv_list *l, const inv_budget *b, size_t *total) {
    if (!allocation(p, l, sizeof(*l), total))
        return false;
    if (l->size > l->capacity || l->capacity > b->max_slots ||
        l->capacity > SIZE_MAX / sizeof(*l->values) || (l->capacity == 0) != (l->values == NULL))
        return refuse(p, MC_INVENTORY_INVALID_ARGUMENT, "Inventory terminal List extent");
    return l->values == NULL || allocation(p, l->values, l->capacity * sizeof(*l->values), total);
}

static bool slot_shape(const mc_container *c, const inv_slot *slot, size_t index) {
    const bool aa = c->kind == MC_CONTAINER_PLAYER;
    const int32_t width = aa ? 2 : 3;
    const size_t craft = (size_t)(width * width), main_start = aa ? 9u : 10u;
    mc_container_slot_state s = {.kind = MC_SLOT_NORMAL,
                                 .inventory_kind = MC_SLOT_INVENTORY_PLAYER,
                                 .inventory = c->inventory,
                                 .container_index = (int32_t)index,
                                 .armor_type = -1};
    if (index == 0) {
        s.kind = MC_SLOT_CRAFTING;
        s.inventory_kind = MC_SLOT_INVENTORY_RESULT;
        s.inventory = c->result;
        s.player = c->inventory->player;
        s.crafting_input = c->crafting;
        s.x = aa ? 144 : 124;
        s.y = aa ? 36 : 35;
    } else if (index <= craft) {
        const int32_t cell = (int32_t)index - 1;
        s.inventory_kind = MC_SLOT_INVENTORY_CRAFTING;
        s.inventory = c->crafting;
        s.inventory_index = cell;
        s.x = (aa ? 88 : 30) + (cell % width) * 18;
        s.y = (aa ? 26 : 17) + (cell / width) * 18;
    } else if (aa && index < main_start) {
        const int32_t armor = (int32_t)index - 5;
        s.kind = MC_SLOT_ARMOR;
        s.armor_type = armor;
        s.armor_container = c;
        s.inventory_index = 39 - armor;
        s.x = 8;
        s.y = 8 + armor * 18;
    } else if (index < main_start + 27u) {
        const int32_t cell = (int32_t)(index - main_start);
        s.inventory_index = 9 + cell;
        s.x = 8 + (cell % 9) * 18;
        s.y = 84 + (cell / 9) * 18;
    } else {
        const int32_t cell = (int32_t)(index - main_start - 27u);
        s.inventory_index = cell;
        s.x = 8 + cell * 18;
        s.y = 142;
    }
    const mc_container_slot_state *v = &slot->state;
    return slot->owner == c && v->kind == s.kind && v->inventory_kind == s.inventory_kind &&
           v->inventory == s.inventory && v->player == s.player &&
           v->crafting_input == s.crafting_input && v->armor_container == s.armor_container &&
           v->container_index == s.container_index && v->inventory_index == s.inventory_index &&
           v->x == s.x && v->y == s.y && v->armor_type == s.armor_type;
}

static bool container(terminal_proof *p, mc_container *c, mc_inventory_player *ix,
                      mc_registry_classes *registry, mc_world_runtime *world, bool aa,
                      bool sealed) {
    size_t total = 0;
    if (!allocation(p, c, sizeof(*c), &total))
        return false;
    if (c->active || c->constructing || c->interaction_active || c->interaction_holds != 0)
        return refuse(p, MC_INVENTORY_BUSY, "Inventory terminal entered Container or child hold");
    if (c->terminal_sealed != sealed)
        return refuse(p, MC_INVENTORY_BUSY, "Inventory terminal Container seal state");
    const size_t count = aa ? 45u : 46u;
    if (!budget_shape(&c->budget) || c->side != MC_REGISTRY_CLIENT ||
        c->kind != (aa ? MC_CONTAINER_PLAYER : MC_CONTAINER_WORKBENCH) || c->classes != registry ||
        c->inventory != ix || !c->inventory_bound || !c->constructor_entered || !c->constructed ||
        c->constructor_failed || c->constructor_pc != (aa ? 290 : 266) || c->local != aa ||
        c->world != (aa ? NULL : world))
        return refuse(p, MC_INVENTORY_INVALID_ARGUMENT, "Inventory terminal Container identity");
    mc_inventory_crafting *g = c->crafting;
    mc_inventory_result *r = c->result;
    if (!allocation(p, g, sizeof(*g), &total) || !allocation(p, r, sizeof(*r), &total))
        return false;
    if (g->active)
        return refuse(p, MC_INVENTORY_BUSY, "Inventory terminal entered Crafting grid");
    if (g->terminal_sealed != sealed || r->terminal_sealed != sealed)
        return refuse(p, MC_INVENTORY_BUSY, "Inventory terminal child seal state");
    const size_t cells = aa ? 4u : 9u;
    if (g->side != MC_REGISTRY_CLIENT || r->side != MC_REGISTRY_CLIENT || g->handler != c ||
        g->budget != &c->budget || r->budget != &c->budget || !g->container_owned ||
        !r->container_owned || g->width != (aa ? 2 : 3) || g->size != cells ||
        g->local_budget.used != 0 || g->local_budget.limit != 0 || g->local_budget.max_slots != 0 ||
        r->local_budget.used != 0 || r->local_budget.limit != 0 || r->local_budget.max_slots != 0)
        return refuse(p, MC_INVENTORY_INVALID_ARGUMENT, "Inventory terminal owned grid/result");
    if (!allocation(p, g->slots, cells * sizeof(*g->slots), &total) ||
        !allocation(p, r->slots, sizeof(*r->slots), &total) ||
        !list(p, c->slots, &c->budget, &total) || !list(p, c->snapshots, &c->budget, &total) ||
        !list(p, c->listeners, &c->budget, &total) ||
        !allocation(p, c->blocked_players, sizeof(*c->blocked_players), &total))
        return false;
    const inv_set *set = c->blocked_players;
    if (set->size > set->capacity || set->capacity > c->budget.max_slots ||
        set->capacity > SIZE_MAX / sizeof(*set->members) ||
        (set->capacity == 0) != (set->members == NULL) || c->slots->size != count ||
        c->snapshots->size != count)
        return refuse(p, MC_INVENTORY_INVALID_ARGUMENT, "Inventory terminal Container lists");
    if (set->members != NULL &&
        !allocation(p, set->members, set->capacity * sizeof(*set->members), &total))
        return false;
    const inv_slot *nodes[46];
    bool visited[46] = {0};
    for (size_t i = 0; i < count; ++i) {
        nodes[i] = c->slots->values[i];
        if (!allocation(p, nodes[i], sizeof(*nodes[i]), &total))
            return false;
        if (!slot_shape(c, nodes[i], i))
            return refuse(p, MC_INVENTORY_INVALID_ARGUMENT, "Inventory terminal Slot source graph");
    }
    const inv_slot *node = c->allocated_slots;
    for (size_t n = 0; n < count; ++n) {
        size_t i = 0;
        while (i < count && nodes[i] != node)
            ++i;
        if (i == count || visited[i])
            return refuse(p, MC_INVENTORY_INVALID_ARGUMENT, "Inventory terminal Slot owner chain");
        visited[i] = true;
        node = node->allocated_next;
    }
    if (node != NULL || total != c->budget.used)
        return refuse(p, MC_INVENTORY_INVALID_ARGUMENT,
                      "Inventory terminal Container allocation balance");
    return true;
}

static bool graph(terminal_proof *p, mc_inventory_player *ix, mc_entity *player,
                  mc_registry_classes *registry, mc_world_runtime *world, mc_container *aa,
                  mc_container *it, bool sealed) {
    if (ix == NULL || player == NULL || registry == NULL || world == NULL || aa == NULL ||
        it == NULL || aa == it)
        return refuse(p, MC_INVENTORY_INVALID_ARGUMENT, "Inventory terminal owner arguments");
    size_t total = 0;
    if (!allocation(p, ix, sizeof(*ix), &total))
        return false;
    if (ix->active)
        return refuse(p, MC_INVENTORY_BUSY, "Inventory terminal entered IX");
    if (ix->terminal_sealed != sealed)
        return refuse(p, MC_INVENTORY_BUSY, "Inventory terminal IX seal state");
    if (ix->containers > 2)
        return refuse(p, MC_INVENTORY_BUSY, "Inventory terminal IX retained by another Container");
    if (ix->side != MC_REGISTRY_CLIENT || ix->player != player || ix->containers != 2 ||
        !budget_shape(&ix->local_budget) || ix->local_budget.max_slots < 36u)
        return refuse(p, MC_INVENTORY_INVALID_ARGUMENT, "Inventory terminal IX owner graph");
    if (!allocation(p, ix->main_slots, 36u * sizeof(*ix->main_slots), &total) ||
        !allocation(p, ix->armor_slots, 4u * sizeof(*ix->armor_slots), &total))
        return false;
    if (total != ix->local_budget.used)
        return refuse(p, MC_INVENTORY_INVALID_ARGUMENT, "Inventory terminal IX allocation balance");
    return container(p, aa, ix, registry, world, true, sealed) &&
           container(p, it, ix, registry, world, false, sealed);
}

static bool empty(const mc_inventory_player_terminal *r) {
    return r->inventory == NULL && r->default_container == NULL && r->active_container == NULL &&
           r->player == NULL && !r->sealed;
}

mc_inventory_status mc_inventory_player_terminal_prepare(mc_inventory_player *ix, mc_entity *player,
                                                         mc_registry_classes *registry,
                                                         mc_world_runtime *world, mc_container *aa,
                                                         mc_container *it,
                                                         mc_inventory_player_terminal *out,
                                                         mc_inventory_error *error) {
    terminal_proof p = {0};
    if (out == NULL || !empty(out))
        (void)refuse(&p, MC_INVENTORY_INVALID_ARGUMENT, "Inventory terminal fresh owner output");
    else if (graph(&p, ix, player, registry, world, aa, it, false)) {
        for (size_t i = 0; i < p.count; ++i)
            if (overlaps(out, sizeof(*out), p.allocations[i], p.sizes[i])) {
                (void)refuse(&p, MC_INVENTORY_INVALID_ARGUMENT,
                             "Inventory terminal output aliases owned storage");
                break;
            }
        if (p.error.status == MC_INVENTORY_OK) {
            ix->terminal_sealed = true;
            aa->terminal_sealed = it->terminal_sealed = true;
            aa->crafting->terminal_sealed = it->crafting->terminal_sealed = true;
            aa->result->terminal_sealed = it->result->terminal_sealed = true;
            *out = (mc_inventory_player_terminal){ix, aa, it, player, true};
        }
    }
    return inv_finish(error, &p.error);
}

mc_inventory_status mc_inventory_player_terminal_admit(const mc_inventory_player_terminal *r,
                                                       mc_registry_classes *registry,
                                                       mc_world_runtime *world,
                                                       mc_inventory_error *error) {
    terminal_proof p = {0};
    if (r == NULL || !r->sealed)
        (void)refuse(&p, MC_INVENTORY_INVALID_ARGUMENT, "Inventory terminal sealed owner record");
    else
        (void)graph(&p, r->inventory, r->player, registry, world, r->default_container,
                    r->active_container, true);
    return inv_finish(error, &p.error);
}

void mc_inventory_player_terminal_dispose(mc_inventory_player_terminal *r) {
    if (r == NULL || empty(r))
        return;
    inv_container_free(r->active_container);
    inv_container_free(r->default_container);
    inv_player_free(r->inventory);
    memset(r, 0, sizeof(*r));
}
