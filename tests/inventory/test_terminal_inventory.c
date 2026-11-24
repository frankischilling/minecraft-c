#include "../../src/inventory/internal.h"
#include "../../src/inventory/player/source_internal.h"
#include "../../src/inventory/player/terminal_internal.h"
#include "mc/inventory/container_interaction.h"
#include "mc/inventory/mining.h"
#include "mc/inventory/player_current.h"
#include "mc/inventory/runtime/insertion.h"
#include "mc/inventory/runtime/nbt.h"
#include "mc/inventory/runtime/tick.h"
#include "mc/resources/files/files.h"
#include "mc/statistics/registry/registry.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

static unsigned checks;
#define CHECK(value)                                                                               \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(value)) {                                                                            \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #value);                            \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

typedef struct fixture {
    mc_inventory_player *ix;
    mc_container *aa, *it;
    mc_registry_classes *classes;
    mc_entity *player;
    mc_world_runtime *world;
} fixture;

typedef struct graph_copy {
    mc_inventory_player ix;
    mc_container aa, it;
    mc_inventory_crafting ag, ig;
    mc_inventory_result ar, ir;
    mc_container_slot_state as[45], is[46];
    mc_item_stack *main[36], *armor[4], *agv[4], *igv[9], *arv, *irv;
} graph_copy;

static graph_copy copy(const fixture *f) {
    graph_copy c = {.ix = *f->ix,
                    .aa = *f->aa,
                    .it = *f->it,
                    .ag = *f->aa->crafting,
                    .ig = *f->it->crafting,
                    .ar = *f->aa->result,
                    .ir = *f->it->result};
    memcpy(c.main, f->ix->main_slots, sizeof c.main);
    memcpy(c.armor, f->ix->armor_slots, sizeof c.armor);
    memcpy(c.agv, f->aa->crafting->slots, sizeof c.agv);
    memcpy(c.igv, f->it->crafting->slots, sizeof c.igv);
    c.arv = f->aa->result->slots[0];
    c.irv = f->it->result->slots[0];
    for (size_t i = 0; i < 45; ++i)
        c.as[i] = ((inv_slot *)f->aa->slots->values[i])->state;
    for (size_t i = 0; i < 46; ++i)
        c.is[i] = ((inv_slot *)f->it->slots->values[i])->state;
    return c;
}

static bool unchanged(const fixture *f, const graph_copy *a) {
    const graph_copy b = copy(f);
    return memcmp(&a->ix, &b.ix, sizeof b.ix) == 0 && memcmp(&a->aa, &b.aa, sizeof b.aa) == 0 &&
           memcmp(&a->it, &b.it, sizeof b.it) == 0 && memcmp(&a->ag, &b.ag, sizeof b.ag) == 0 &&
           memcmp(&a->ig, &b.ig, sizeof b.ig) == 0 && memcmp(&a->ar, &b.ar, sizeof b.ar) == 0 &&
           memcmp(&a->ir, &b.ir, sizeof b.ir) == 0 && memcmp(a->main, b.main, sizeof b.main) == 0 &&
           memcmp(a->armor, b.armor, sizeof b.armor) == 0 &&
           memcmp(a->agv, b.agv, sizeof b.agv) == 0 && memcmp(a->igv, b.igv, sizeof b.igv) == 0 &&
           a->arv == b.arv && a->irv == b.irv && memcmp(a->as, b.as, sizeof b.as) == 0 &&
           memcmp(a->is, b.is, sizeof b.is) == 0;
}

static bool refuse(const fixture *f, mc_container *aa, mc_container *it,
                   mc_inventory_status wanted) {
    const graph_copy before = copy(f);
    mc_inventory_player_terminal out = {0};
    mc_inventory_error e;
    const mc_inventory_status status = mc_inventory_player_terminal_prepare(
        f->ix, f->player, f->classes, f->world, aa, it, &out, &e);
    return status == wanted && e.status == status && e.frame_count == 0 && !e.mutation_reached &&
           !e.notification_reached && out.inventory == NULL && out.default_container == NULL &&
           out.active_container == NULL && out.player == NULL && !out.sealed &&
           unchanged(f, &before);
}

static bool exercise(mc_registry_classes *classes) {
    bool ok = false;
    /* Identity-only tokens are never dereferenced by Inventory. Genuine READY
     * Player/World/Registry ownership admission is tested by the Player owner
     * fixture; this test isolates the local native graph and Source values. */
    unsigned player_token = 0, world_token = 0;
    fixture f = {.classes = classes,
                 .player = (mc_entity *)&player_token,
                 .world = (mc_world_runtime *)&world_token};
    mc_inventory_player_terminal terminal = {0}, second = {0};
    mc_item_stack *raw = NULL, *value = NULL;
    mc_container *extra = NULL, *fresh = NULL;
    mc_inventory_crafting *extra_grid = NULL;
    mc_container_interaction_call *call = NULL;
    mc_inventory_error e;
    CHECK(mc_inventory_player_create(MC_REGISTRY_CLIENT, f.player, NULL, &f.ix, &e) ==
          MC_INVENTORY_OK);
    CHECK(mc_container_player_create_default(MC_REGISTRY_CLIENT, classes, f.ix, NULL, &f.aa, &e) ==
          MC_INVENTORY_OK);
    CHECK(mc_container_workbench_create(MC_REGISTRY_CLIENT, classes, f.ix, f.world, -8, 64, 9, NULL,
                                        &f.it, &e) == MC_INVENTORY_OK);
    CHECK(mc_container_workbench_allocate(MC_REGISTRY_CLIENT, classes, NULL, &fresh, &e) ==
          MC_INVENTORY_OK);
    CHECK(refuse(&f, f.it, f.aa, MC_INVENTORY_INVALID_ARGUMENT));
    CHECK(refuse(&f, f.aa, f.aa, MC_INVENTORY_INVALID_ARGUMENT));
    CHECK(refuse(&f, f.aa, fresh, MC_INVENTORY_INVALID_ARGUMENT));
    CHECK(mc_container_workbench_create(MC_REGISTRY_CLIENT, classes, f.ix, f.world, 0, 0, 0, NULL,
                                        &extra, &e) == MC_INVENTORY_OK);
    CHECK(refuse(&f, f.aa, f.it, MC_INVENTORY_BUSY));
    CHECK(mc_container_retire(&extra, &e) == MC_INVENTORY_OK);
    CHECK(mc_container_retain(f.it, &e) == MC_INVENTORY_OK);
    CHECK(refuse(&f, f.aa, f.it, MC_INVENTORY_BUSY));
    CHECK(mc_container_release(f.it, &e) == MC_INVENTORY_OK);
    f.aa->active = true;
    CHECK(refuse(&f, f.aa, f.it, MC_INVENTORY_BUSY));
    f.aa->active = false;
    f.it->interaction_active = true;
    CHECK(refuse(&f, f.aa, f.it, MC_INVENTORY_BUSY));
    f.it->interaction_active = false;
    f.ix->active = true;
    CHECK(refuse(&f, f.aa, f.it, MC_INVENTORY_BUSY));
    f.ix->active = false;
    f.it->crafting->active = true;
    CHECK(refuse(&f, f.aa, f.it, MC_INVENTORY_BUSY));
    f.it->crafting->active = false;
    f.it->result->terminal_sealed = true;
    CHECK(refuse(&f, f.aa, f.it, MC_INVENTORY_BUSY));
    f.it->result->terminal_sealed = false;
    f.it->constructor_failed = true;
    CHECK(refuse(&f, f.aa, f.it, MC_INVENTORY_INVALID_ARGUMENT));
    f.it->constructor_failed = false;
    f.it->constructor_pc = 263;
    CHECK(refuse(&f, f.aa, f.it, MC_INVENTORY_INVALID_ARGUMENT));
    f.it->constructor_pc = 266;
    f.it->world = NULL;
    CHECK(refuse(&f, f.aa, f.it, MC_INVENTORY_INVALID_ARGUMENT));
    f.it->world = f.world;
    f.ix->containers = 1;
    CHECK(refuse(&f, f.aa, f.it, MC_INVENTORY_INVALID_ARGUMENT));
    f.ix->containers = 2;
    ++f.it->budget.used;
    CHECK(refuse(&f, f.aa, f.it, MC_INVENTORY_INVALID_ARGUMENT));
    --f.it->budget.used;
    f.it->slots->capacity = SIZE_MAX;
    CHECK(refuse(&f, f.aa, f.it, MC_INVENTORY_INVALID_ARGUMENT));
    f.it->slots->capacity = f.it->snapshots->capacity;
    f.it->crafting->handler = f.aa;
    CHECK(refuse(&f, f.aa, f.it, MC_INVENTORY_INVALID_ARGUMENT));
    f.it->crafting->handler = f.it;
    inv_slot *last = f.it->slots->values[45];
    last->state.inventory_index = 9;
    CHECK(refuse(&f, f.aa, f.it, MC_INVENTORY_INVALID_ARGUMENT));
    last->state.inventory_index = 8;
    inv_slot *head = f.it->allocated_slots;
    f.it->allocated_slots = head->allocated_next;
    CHECK(refuse(&f, f.aa, f.it, MC_INVENTORY_INVALID_ARGUMENT));
    f.it->allocated_slots = head;
    void *old = f.it->slots->values[45];
    f.it->slots->values[45] = f.it->slots->values[44];
    CHECK(refuse(&f, f.aa, f.it, MC_INVENTORY_INVALID_ARGUMENT));
    f.it->slots->values[45] = old;
    void **buffer = f.it->snapshots->values;
    f.it->snapshots->values = f.it->slots->values;
    CHECK(refuse(&f, f.aa, f.it, MC_INVENTORY_INVALID_ARGUMENT));
    f.it->snapshots->values = buffer;
    CHECK(mc_item_stack_allocate(&raw) == MC_ITEM_STACK_OK);
    CHECK(mc_inventory_player_set(f.ix, 0, raw, &e) == MC_INVENTORY_OK);
    CHECK(mc_inventory_player_set(f.ix, 39, raw, &e) == MC_INVENTORY_OK);
    mc_entity *notified = NULL;
    CHECK(mc_inventory_player_source_cursor_store(f.ix, raw, &notified, &e) == MC_INVENTORY_OK &&
          notified == f.player);
    /* RAW/negative IDs/counts are not an admission policy of native disposal. */
    CHECK(mc_item_stack_construct(raw, INT32_MIN, -7, INT32_MAX) == MC_ITEM_STACK_OK);
    CHECK(mc_inventory_crafting_set(f.aa->crafting, 0, raw, &e) == MC_INVENTORY_OK);
    CHECK(mc_inventory_crafting_set(f.it->crafting, 0, raw, &e) == MC_INVENTORY_OK);
    CHECK(mc_inventory_crafting_set(f.it->crafting, 8, raw, &e) == MC_INVENTORY_OK);
    CHECK(mc_inventory_result_set(f.aa->result, 0, raw, &e) == MC_INVENTORY_OK);
    CHECK(mc_inventory_result_set(f.it->result, 0, raw, &e) == MC_INVENTORY_OK);
    CHECK(inv_hold(raw, &e) == MC_INVENTORY_OK);
    f.it->snapshots->values[45] = raw;
    f.ix->selected = INT32_MIN;
    f.it->window_id = -17;
    f.it->transaction = INT16_MIN;
    /* Exhaust the local admission budgets: handoff must need no new byte. */
    f.ix->local_budget.limit = f.ix->local_budget.used;
    f.aa->budget.limit = f.aa->budget.used;
    f.it->budget.limit = f.it->budget.used;
    const graph_copy before = copy(&f);
    CHECK(mc_inventory_player_terminal_prepare(f.ix, f.player, classes, f.world, f.aa, f.it,
                                               &terminal, &e) == MC_INVENTORY_OK);
    CHECK(terminal.inventory == f.ix && terminal.default_container == f.aa &&
          terminal.active_container == f.it && terminal.player == f.player && terminal.sealed &&
          e.frame_count == 0);
    graph_copy sealed = before;
    sealed.ix.terminal_sealed = sealed.aa.terminal_sealed = sealed.it.terminal_sealed = true;
    sealed.ag.terminal_sealed = sealed.ig.terminal_sealed = true;
    sealed.ar.terminal_sealed = sealed.ir.terminal_sealed = true;
    CHECK(unchanged(&f, &sealed));
    CHECK(mc_inventory_player_terminal_admit(&terminal, classes, f.world, &e) == MC_INVENTORY_OK);
    CHECK(mc_inventory_player_terminal_admit(&terminal, classes, f.world, &e) == MC_INVENTORY_OK);
    CHECK(mc_inventory_player_terminal_admit(&terminal, classes, NULL, &e) ==
              MC_INVENTORY_INVALID_ARGUMENT &&
          e.frame_count == 0);
    CHECK(mc_inventory_player_terminal_prepare(f.ix, f.player, classes, f.world, f.aa, f.it,
                                               &second, &e) == MC_INVENTORY_BUSY &&
          !second.sealed && second.inventory == NULL && e.frame_count == 0);
    const mc_item_stack *borrow = NULL;
    CHECK(mc_inventory_player_borrow(f.ix, 0, &borrow, &e) == MC_INVENTORY_OK && borrow == raw);
    CHECK(mc_inventory_crafting_borrow_slot(f.it->crafting, 8, &borrow, &e) == MC_INVENTORY_OK &&
          borrow == raw);
    CHECK(mc_inventory_result_borrow(f.it->result, 0, &borrow, &e) == MC_INVENTORY_OK &&
          borrow == raw);
    CHECK(mc_inventory_player_source_cursor_get(f.ix, &borrow, &e) == MC_INVENTORY_OK &&
          borrow == raw);
    CHECK(mc_inventory_player_get(f.ix, 0, &value, &e) == MC_INVENTORY_BUSY && value == NULL &&
          e.frame_count == 0);
    CHECK(mc_inventory_player_set(f.ix, 0, NULL, &e) == MC_INVENTORY_BUSY && e.frame_count == 0);
    CHECK(mc_inventory_player_source_set(f.ix, -1, NULL, &e) == MC_INVENTORY_BUSY &&
          e.frame_count == 0);
    CHECK(mc_inventory_player_split(f.ix, 0, 1, &value, &e) == MC_INVENTORY_BUSY && value == NULL &&
          e.frame_count == 0);
    CHECK(mc_inventory_player_changed(f.ix, &e) == MC_INVENTORY_BUSY && e.frame_count == 0);
    CHECK(mc_inventory_player_source_cursor_store(f.ix, NULL, &notified, &e) == MC_INVENTORY_BUSY &&
          e.frame_count == 0 && notified == f.player);
    CHECK(mc_inventory_crafting_get(f.it->crafting, 0, &value, &e) == MC_INVENTORY_BUSY &&
          e.frame_count == 0);
    CHECK(mc_inventory_crafting_get_xy(f.it->crafting, 0, 0, &value, &e) == MC_INVENTORY_BUSY &&
          e.frame_count == 0);
    CHECK(mc_inventory_crafting_set(f.it->crafting, 0, NULL, &e) == MC_INVENTORY_BUSY &&
          e.frame_count == 0);
    CHECK(mc_inventory_crafting_split(f.it->crafting, 0, 1, &value, &e) == MC_INVENTORY_BUSY &&
          e.frame_count == 0);
    CHECK(mc_inventory_crafting_changed(f.it->crafting, &e) == MC_INVENTORY_BUSY &&
          e.frame_count == 0);
    CHECK(mc_inventory_result_get(f.it->result, 0, &value, &e) == MC_INVENTORY_BUSY &&
          e.frame_count == 0);
    CHECK(mc_inventory_result_set(f.it->result, 0, NULL, &e) == MC_INVENTORY_BUSY &&
          e.frame_count == 0);
    CHECK(mc_inventory_result_split(f.it->result, 0, 1, &value, &e) == MC_INVENTORY_BUSY &&
          e.frame_count == 0);
    CHECK(mc_inventory_result_changed(f.it->result, &e) == MC_INVENTORY_BUSY && e.frame_count == 0);
    CHECK(mc_container_retain(f.it, &e) == MC_INVENTORY_BUSY && e.frame_count == 0);
    CHECK(mc_container_inventory_changed(f.it, &e) == MC_INVENTORY_BUSY && e.frame_count == 0);
    CHECK(mc_container_workbench_create(MC_REGISTRY_CLIENT, classes, f.ix, f.world, 0, 0, 0, NULL,
                                        &extra, &e) == MC_INVENTORY_BUSY &&
          extra == NULL && e.frame_count == 0);
    CHECK(mc_container_player_create_default(MC_REGISTRY_CLIENT, classes, f.ix, NULL, &extra, &e) ==
              MC_INVENTORY_BUSY &&
          extra == NULL && e.frame_count == 0);
    CHECK(mc_container_workbench_construct(fresh, f.ix, f.world, 0, 0, 0, &e) ==
              MC_INVENTORY_BUSY &&
          !fresh->constructor_entered && e.frame_count == 0);
    CHECK(mc_inventory_crafting_create(MC_REGISTRY_CLIENT, f.it, 1, 1, NULL, &extra_grid, &e) ==
              MC_INVENTORY_BUSY &&
          extra_grid == NULL && e.frame_count == 0);
    mc_container_interaction_error ie;
    CHECK(mc_container_click_begin(f.it, f.player, 0, 0, false, NULL, &call, &ie) ==
              MC_INVENTORY_BUSY &&
          call == NULL && ie.site.class_name == NULL && ie.site.pc == -1);
    CHECK(mc_container_close_begin(f.it, f.player, NULL, &call, &ie) == MC_INVENTORY_BUSY &&
          call == NULL && ie.site.class_name == NULL);
    mc_inventory_slot *slot = NULL;
    CHECK(mc_container_source_slot(f.it, 0, &slot, &ie) == MC_INVENTORY_OK && slot != NULL);
    CHECK(mc_inventory_slot_source_get(slot, &borrow, &ie) == MC_INVENTORY_BUSY &&
          ie.site.class_name == NULL && ie.site.pc == -1);
    bool flag = false;
    int32_t icon = 0;
    CHECK(mc_inventory_slot_source_has(slot, &flag, &ie) == MC_INVENTORY_BUSY &&
          ie.site.class_name == NULL);
    CHECK(mc_inventory_slot_source_background(slot, &icon, &ie) == MC_INVENTORY_BUSY &&
          ie.site.class_name == NULL);
    mc_inventory_selection_error se;
    CHECK(mc_inventory_player_scroll(f.ix, 1, &se) == MC_INVENTORY_BUSY && !se.source_entered &&
          se.site.class_name == NULL);
    CHECK(mc_inventory_player_select_hotbar(f.ix, 0, &se) == MC_INVENTORY_BUSY &&
          !se.mutation_reached);
    mc_inventory_current_error ce;
    CHECK(mc_inventory_player_current(f.ix, &borrow, &ce) == MC_INVENTORY_BUSY &&
          !ce.source_entered && ce.site.class_name == NULL);
    float speed = 0;
    CHECK(mc_inventory_player_mining_speed(f.ix, NULL, NULL, &speed, &e) == MC_INVENTORY_BUSY &&
          e.frame_count == 0);
    CHECK(mc_inventory_player_can_harvest(f.ix, NULL, NULL, &flag, &e) == MC_INVENTORY_BUSY &&
          e.frame_count == 0);
    mc_inventory_insert_error insert;
    CHECK(mc_inventory_player_insert(f.ix, raw, classes, NULL, &flag, &insert) == MC_ENTITY_BUSY &&
          insert.bytecode == 0 && insert.inventory.frame_count == 0);
    mc_inventory_nbt_error ne;
    mc_nbt_tree *nbt = NULL;
    CHECK(mc_inventory_player_read_nbt(f.ix, classes, NULL, NULL, &ne) == MC_INVENTORY_NBT_BUSY);
    CHECK(mc_inventory_player_build_nbt(f.ix, NULL, &nbt, &ne) == MC_INVENTORY_NBT_BUSY &&
          nbt == NULL);
    mc_inventory_tick_error te;
    CHECK(mc_inventory_player_tick(f.ix, classes, NULL, &te) == MC_INVENTORY_TICK_BUSY &&
          te.completed_slots == 0);
    mc_inventory_player *ix_owner = f.ix;
    mc_container *aa_owner = f.aa, *it_owner = f.it;
    mc_inventory_crafting *grid_owner = f.it->crafting;
    mc_inventory_result *result_owner = f.it->result;
    CHECK(mc_inventory_player_retire(&ix_owner, &e) == MC_INVENTORY_BUSY && ix_owner == f.ix &&
          e.frame_count == 0);
    CHECK(mc_container_retire(&aa_owner, &e) == MC_INVENTORY_BUSY && aa_owner == f.aa &&
          e.frame_count == 0);
    CHECK(mc_container_retire(&it_owner, &e) == MC_INVENTORY_BUSY && it_owner == f.it &&
          e.frame_count == 0);
    CHECK(mc_inventory_crafting_retire(&grid_owner, &e) == MC_INVENTORY_BUSY &&
          grid_owner == f.it->crafting && e.frame_count == 0);
    CHECK(mc_inventory_result_retire(&result_owner, &e) == MC_INVENTORY_BUSY &&
          result_owner == f.it->result && e.frame_count == 0);
    CHECK(unchanged(&f, &sealed));
    CHECK(mc_inventory_player_terminal_admit(&terminal, classes, f.world, &e) == MC_INVENTORY_OK);
    mc_inventory_player_terminal_dispose(&terminal);
    f.ix = NULL;
    f.aa = f.it = NULL;
    CHECK(terminal.inventory == NULL && terminal.default_container == NULL &&
          terminal.active_container == NULL && terminal.player == NULL && !terminal.sealed);
    mc_item_stack_fields fields;
    CHECK(mc_item_stack_fields_get(raw, &fields) == MC_ITEM_STACK_OK && fields.id == INT32_MIN &&
          fields.count == -7 && fields.damage == INT32_MAX);
    mc_inventory_player_terminal_dispose(&terminal);
    ok = true;
cleanup:
    mc_container_interaction_retire(&call, NULL);
    mc_inventory_crafting_retire(&extra_grid, NULL);
    mc_container_retire(&fresh, NULL);
    mc_container_retire(&extra, NULL);
    if (terminal.sealed) {
        mc_inventory_player_terminal_dispose(&terminal);
        f.ix = NULL;
        f.aa = f.it = NULL;
    }
    mc_container_retire(&f.it, NULL);
    mc_container_retire(&f.aa, NULL);
    mc_inventory_player_retire(&f.ix, NULL);
    mc_item_stack_release(raw);
    mc_item_stack_release(value);
    return ok;
}

int main(int argc, char **argv) {
    int result = 1;
    mc_resource_files *files = NULL;
    mc_materials *materials = NULL;
    mc_java_random_profile *profile = NULL;
    mc_registry_classes *classes = NULL;
    if (argc != 2 || mc_resource_files_create(argv[1], &files) != MC_REGISTRY_OK ||
        !mc_materials_create(&materials) ||
        mc_java_random_profile_create(SIZE_MAX, &profile) != MC_JAVA_RANDOM_PROFILE_OK)
        goto cleanup;
    const mc_registry_options options = {MC_REGISTRY_CLIENT, profile, materials, 0,
                                         mc_resource_files_source(files)};
    if (mc_registry_classes_allocate(&options, &classes, NULL) != MC_REGISTRY_OK ||
        mc_registry_classes_initialize_statistics(classes, NULL) != MC_REGISTRY_OK ||
        !exercise(classes))
        goto cleanup;
    printf("Inventory terminal checks passed: %u\n", checks);
    result = 0;
cleanup:
    if (mc_registry_classes_free(classes, NULL) != MC_REGISTRY_OK)
        result = 1;
    if (files != NULL && mc_resource_files_free(files) != MC_REGISTRY_OK)
        result = 1;
    mc_materials_destroy(materials);
    mc_java_random_profile_free(profile);
    return result;
}
