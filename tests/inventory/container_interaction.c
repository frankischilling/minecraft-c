#include "mc/inventory/container_interaction.h"
#include "mc/resources/files/files.h"
#include "mc/statistics/registry/registry.h"

#include "../../src/block/registry/registry_internal.h"
#include "../../src/inventory/container/interaction_internal.h"
#include "../../src/inventory/internal.h"
#include "../../src/inventory/player/source_internal.h"
#include "../../src/item/stack/source_internal.h"

#include <stdio.h>
#include <stdlib.h>
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
    mc_inventory_player *inventory;
    mc_container *container;
    mc_container_interaction_call *call;
    mc_container_interaction_host host;
    mc_item_stack *a, *b, *returned;
    unsigned inventory_calls, cursor_calls, drops, crafted;
    int32_t crafted_count, drop_count;
    bool fail_cursor, fail_drop, fail_crafted, observed_cursor, observed_ingredient;
    bool null_result_on_cursor, null_grid_on_cursor, null_grid_on_drop, null_grid_on_crafted;
    bool null_result_on_crafted;
    mc_item_stack **saved_result, **saved_grid;
} fixture;

static mc_item_stack *stack(int32_t id, int32_t count, int32_t damage) {
    mc_item_stack *s = NULL;
    if (mc_item_stack_allocate(&s) != MC_ITEM_STACK_OK)
        return NULL;
    if (mc_item_stack_construct(s, id, count, damage) != MC_ITEM_STACK_OK) {
        mc_item_stack_release(s);
        return NULL;
    }
    return s;
}

static bool fields(const mc_item_stack *s, int32_t id, int32_t count) {
    mc_item_stack_fields f;
    return s != NULL && mc_item_stack_fields_get(s, &f) == MC_ITEM_STACK_OK && f.id == id &&
           f.count == count;
}

static mc_inventory_status injected(mc_container_interaction_error *e, const char *operation) {
    e->status = MC_INVENTORY_UNIMPLEMENTED_DEPENDENCY;
    e->native_policy = true;
    e->operation = operation;
    return e->status;
}

static mc_inventory_status inventory(void *context, mc_entity *player, mc_inventory_player **out,
                                     mc_container_interaction_error *e) {
    fixture *f = context;
    (void)player;
    (void)e;
    ++f->inventory_calls;
    *out = f->inventory;
    return MC_INVENTORY_OK;
}

static mc_inventory_status cursor_changed(void *context, mc_entity *player, mc_item_stack *s,
                                          mc_container_interaction_error *e) {
    fixture *f = context;
    (void)player;
    ++f->cursor_calls;
    f->observed_cursor = f->inventory->cursor == s;
    if (f->null_result_on_cursor) {
        f->saved_result = f->container->result->slots;
        f->container->result->slots = NULL;
    }
    if (f->null_grid_on_cursor) {
        f->saved_grid = f->container->crafting->slots;
        f->container->crafting->slots = NULL;
    }
    return f->fail_cursor ? injected(e, "authored cursor child failure") : MC_INVENTORY_OK;
}

static mc_inventory_status drop(void *context, mc_entity *player, mc_item_stack *s,
                                mc_container_interaction_error *e) {
    fixture *f = context;
    (void)player;
    ++f->drops;
    f->observed_cursor = f->inventory->cursor != NULL;
    if (s != NULL)
        (void)mc_item_stack_source_count_get(s, &f->drop_count);
    if (f->null_grid_on_drop) {
        f->saved_grid = f->container->crafting->slots;
        f->container->crafting->slots = NULL;
    }
    return f->fail_drop ? injected(e, "authored drop child failed after observation")
                        : MC_INVENTORY_OK;
}

static mc_inventory_status crafted(void *context, mc_entity *player, mc_item_stack *s,
                                   mc_container_interaction_error *e) {
    fixture *f = context;
    (void)player;
    ++f->crafted;
    if (s != NULL)
        (void)mc_item_stack_source_count_get(s, &f->crafted_count);
    f->observed_ingredient =
        f->container->crafting->slots != NULL && fields(f->container->crafting->slots[0], 17, 2);
    if (f->null_grid_on_crafted) {
        f->saved_grid = f->container->crafting->slots;
        f->container->crafting->slots = NULL;
    }
    if (f->null_result_on_crafted) {
        f->saved_result = f->container->result->slots;
        f->container->result->slots = NULL;
    }
    return f->fail_crafted ? injected(e, "authored crafting statistic child failure")
                           : MC_INVENTORY_OK;
}

static bool create(fixture *f, mc_registry_classes *r) {
    *f = (fixture){0};
    f->host = (mc_container_interaction_host){f, inventory, cursor_changed, drop, crafted};
    return mc_inventory_player_create(MC_REGISTRY_CLIENT, NULL, NULL, &f->inventory, NULL) ==
               MC_INVENTORY_OK &&
           mc_container_player_create(MC_REGISTRY_CLIENT, r, f->inventory, true, NULL,
                                      &f->container, NULL) == MC_INVENTORY_OK;
}

static bool destroy(fixture *f) {
    bool ok = mc_container_interaction_retire(&f->call, NULL) == MC_INVENTORY_OK;
    if (!ok)
        return false;
    /* Only after checked call retirement, restore authored source links for
     * fixture destruction. None is repaired while a source invocation runs. */
    if (f->saved_result != NULL)
        f->container->result->slots = f->saved_result;
    if (f->saved_grid != NULL)
        f->container->crafting->slots = f->saved_grid;
    f->saved_result = f->saved_grid = NULL;
    mc_item_stack_release(f->a);
    mc_item_stack_release(f->b);
    mc_item_stack_release(f->returned);
    f->a = f->b = f->returned = NULL;
    ok = mc_container_retire(&f->container, NULL) == MC_INVENTORY_OK && ok;
    return mc_inventory_player_retire(&f->inventory, NULL) == MC_INVENTORY_OK && ok;
}

static mc_inventory_status click(fixture *f, int32_t slot, int32_t button, bool shift,
                                 mc_container_interaction_error *e) {
    return mc_container_click_begin(f->container, NULL, slot, button, shift, &f->host, &f->call, e);
}

static bool controls(mc_registry_classes *r) {
    bool ok = false;
    fixture f = {0};
    mc_container_interaction_error e;
    mc_container_interaction_state state;
    CHECK(create(&f, r));
    inv_list *list = f.container->slots;
    f.container->slots = NULL;
    CHECK(click(&f, -1, 2, false, &e) == MC_INVENTORY_OK && f.inventory_calls == 0);
    f.container->slots = list;
    CHECK(mc_container_interaction_inspect(f.call, &state) && state.returned && !state.active);
    CHECK(mc_container_retire(&f.container, NULL) == MC_INVENTORY_BUSY);
    CHECK(mc_container_interaction_take_result(f.call, &f.returned) == MC_INVENTORY_OK &&
          f.returned == NULL);
    CHECK(destroy(&f));

    CHECK(create(&f, r));
    CHECK(click(&f, -1, 0, false, &e) == MC_INVENTORY_INDEX_OUT_OF_BOUNDS && e.source_exception &&
          strcmp(e.exception_class, "java/lang/IndexOutOfBoundsException") == 0 &&
          e.site.pc == 193 && !f.inventory->changed);
    CHECK(destroy(&f));

    CHECK(create(&f, r));
    CHECK(click(&f, 9, 0, false, NULL) == MC_INVENTORY_OK && f.inventory->changed);
    CHECK(mc_container_interaction_take_result(f.call, &f.returned) == MC_INVENTORY_OK &&
          f.returned == NULL);
    CHECK(destroy(&f));

    CHECK(create(&f, r));
    f.a = stack(3, 5, 0);
    CHECK(f.a != NULL && mc_inventory_player_set(f.inventory, 9, f.a, NULL) == MC_INVENTORY_OK);
    CHECK(click(&f, 9, 1, false, &e) == MC_INVENTORY_OK && fields(f.a, 3, 2) &&
          fields(f.inventory->cursor, 3, 3) && f.inventory->cursor != f.a && f.observed_cursor);
    CHECK(mc_container_interaction_take_result(f.call, &f.returned) == MC_INVENTORY_OK &&
          fields(f.returned, 3, 5) && f.returned != f.a && f.returned != f.inventory->cursor);
    CHECK(destroy(&f));

    CHECK(create(&f, r));
    f.a = stack(35, 5, 7);
    CHECK(f.a != NULL && mc_inventory_player_source_cursor_store(
                             f.inventory, f.a, &(mc_entity *){0}, NULL) == MC_INVENTORY_OK);
    CHECK(click(&f, 9, 1, false, &e) == MC_INVENTORY_OK && fields(f.a, 35, 4) &&
          fields(f.inventory->main_slots[9], 35, 1) && f.inventory->main_slots[9] != f.a);
    CHECK(destroy(&f));

    CHECK(create(&f, r));
    f.a = stack(3, 65, 0);
    f.b = stack(3, 1, 0);
    CHECK(f.a != NULL && f.b != NULL &&
          mc_inventory_player_set(f.inventory, 9, f.a, NULL) == MC_INVENTORY_OK &&
          mc_inventory_player_source_cursor_store(f.inventory, f.b, &(mc_entity *){0}, NULL) ==
              MC_INVENTORY_OK);
    CHECK(click(&f, 9, 0, false, &e) == MC_INVENTORY_OK && fields(f.a, 3, 64) && fields(f.b, 3, 2));
    CHECK(destroy(&f));

    CHECK(create(&f, r));
    f.a = stack(3, 5, 0);
    CHECK(f.a != NULL && mc_inventory_player_set(f.inventory, 9, f.a, NULL) == MC_INVENTORY_OK &&
          mc_inventory_player_source_cursor_store(f.inventory, f.a, &(mc_entity *){0}, NULL) ==
              MC_INVENTORY_OK);
    CHECK(click(&f, 9, 0, false, &e) == MC_INVENTORY_OK && fields(f.a, 3, 5) &&
          f.inventory->cursor == NULL);
    CHECK(destroy(&f));

    const int32_t armor_ids[] = {298, 3, 86};
    for (size_t i = 0; i < 3; ++i) {
        CHECK(create(&f, r));
        f.a = stack(armor_ids[i], 4, 0);
        CHECK(f.a != NULL && mc_inventory_player_source_cursor_store(
                                 f.inventory, f.a, &(mc_entity *){0}, NULL) == MC_INVENTORY_OK);
        CHECK(click(&f, 5, 0, false, &e) == MC_INVENTORY_OK);
        CHECK(i == 1 ? f.inventory->armor_slots[3] == NULL && fields(f.a, 3, 4)
                     : fields(f.inventory->armor_slots[3], armor_ids[i], 1) &&
                           fields(f.a, armor_ids[i], 3));
        CHECK(destroy(&f));
    }

    CHECK(create(&f, r));
    f.a = stack(3, 3, 0);
    CHECK(f.a != NULL && mc_inventory_player_set(f.inventory, 9, f.a, NULL) == MC_INVENTORY_OK);
    CHECK(click(&f, 9, 0, true, &e) == MC_INVENTORY_OK && f.inventory->main_slots[9] == NULL &&
          fields(f.inventory->main_slots[0], 3, 3) && fields(f.a, 3, 0));
    CHECK(mc_container_interaction_take_result(f.call, &f.returned) == MC_INVENTORY_OK &&
          fields(f.returned, 3, 3) && f.returned != f.a);
    CHECK(destroy(&f));

    for (unsigned mode = 0; mode < 4; ++mode) {
        CHECK(create(&f, r));
        f.a = stack(17, 2, 0);
        CHECK(f.a != NULL &&
              mc_inventory_crafting_set(f.container->crafting, 0, f.a, NULL) == MC_INVENTORY_OK &&
              fields(f.container->result->slots[0], 5, 4));
        if (mode == 1) {
            f.b = stack(5, 4, 0);
            CHECK(f.b != NULL && mc_inventory_player_source_cursor_store(
                                     f.inventory, f.b, &(mc_entity *){0}, NULL) == MC_INVENTORY_OK);
        }
        f.fail_crafted = mode == 3;
        const mc_inventory_status status = click(&f, 0, 0, mode == 2, &e);
        CHECK(status == (mode == 3 ? MC_INVENTORY_UNIMPLEMENTED_DEPENDENCY : MC_INVENTORY_OK) &&
              f.crafted == 1 && f.observed_ingredient);
        CHECK(f.crafted_count == (mode == 1 ? 8 : mode == 2 ? 0 : 4));
        CHECK(fields(f.a, 17, mode == 3 ? 2 : 1) && !f.inventory->changed == (mode != 2));
        CHECK(mode == 2 ? fields(f.inventory->main_slots[8], 5, 4)
                        : fields(f.inventory->cursor, 5, mode == 1 ? 8 : 4));
        CHECK(destroy(&f));
    }

    CHECK(create(&f, r));
    f.a = stack(3, 5, 0);
    f.fail_cursor = true;
    CHECK(f.a != NULL && mc_inventory_player_set(f.inventory, 9, f.a, NULL) == MC_INVENTORY_OK);
    CHECK(click(&f, 9, 0, false, &e) == MC_INVENTORY_UNIMPLEMENTED_DEPENDENCY &&
          f.inventory->main_slots[9] == NULL && f.inventory->cursor == f.a && f.observed_cursor &&
          e.site.pc == 361 && f.inventory->changed);
    CHECK(destroy(&f));

    CHECK(create(&f, r));
    f.a = stack(3, 5, 0);
    f.fail_drop = true;
    CHECK(f.a != NULL && mc_inventory_player_source_cursor_store(
                             f.inventory, f.a, &(mc_entity *){0}, NULL) == MC_INVENTORY_OK);
    CHECK(click(&f, -999, 1, false, &e) == MC_INVENTORY_UNIMPLEMENTED_DEPENDENCY && f.drops == 1 &&
          f.drop_count == 1 && f.observed_cursor && fields(f.a, 3, 4) &&
          f.inventory->cursor == f.a && e.site.pc == 77);
    CHECK(destroy(&f));

    CHECK(create(&f, r));
    f.a = stack(3, 5, 0);
    f.b = stack(17, 2, 0);
    CHECK(f.a != NULL && f.b != NULL &&
          mc_inventory_player_source_cursor_store(f.inventory, f.a, &(mc_entity *){0}, NULL) ==
              MC_INVENTORY_OK &&
          mc_inventory_crafting_set(f.container->crafting, 0, f.b, NULL) == MC_INVENTORY_OK);
    CHECK(mc_container_close_begin(f.container, NULL, &f.host, &f.call, &e) == MC_INVENTORY_OK &&
          f.drops == 2 && f.inventory->cursor == NULL && f.container->crafting->slots[0] == NULL &&
          f.container->result->slots[0] == NULL && fields(f.a, 3, 5) && fields(f.b, 17, 2));
    CHECK(mc_container_interaction_inspect(f.call, &state) && state.cursor_cleared &&
          state.crafting_cells_cleared == 1);
    CHECK(destroy(&f));

    CHECK(create(&f, r));
    f.a = stack(17, 2, 0);
    f.fail_drop = true;
    CHECK(f.a != NULL &&
          mc_inventory_crafting_set(f.container->crafting, 0, f.a, NULL) == MC_INVENTORY_OK);
    CHECK(mc_container_close_begin(f.container, NULL, &f.host, &f.call, &e) ==
              MC_INVENTORY_UNIMPLEMENTED_DEPENDENCY &&
          f.container->crafting->slots[0] == f.a && fields(f.a, 17, 2) && e.site.pc == 27 &&
          strcmp(e.site.class_name, "aa") == 0);
    CHECK(destroy(&f));
    for (unsigned kind = 0; kind < 2; ++kind) {
        CHECK(create(&f, r));
        f.a = stack(kind == 0 ? 5 : 17, 0, 0);
        CHECK(f.a != NULL);
        if (kind == 0) {
            CHECK(mc_inventory_result_set(f.container->result, 0, f.a, NULL) == MC_INVENTORY_OK);
            f.null_result_on_cursor = true;
        } else {
            CHECK(mc_inventory_crafting_set(f.container->crafting, 0, f.a, NULL) ==
                  MC_INVENTORY_OK);
            f.null_grid_on_cursor = true;
        }
        CHECK(click(&f, kind == 0 ? 0 : 1, 0, false, &e) == MC_INVENTORY_NULL_REFERENCE &&
              e.source_exception && e.site.pc == 375 && e.dependency.pc == 6 &&
              strcmp(e.dependency.class_name, kind == 0 ? "wl" : "mq") == 0 &&
              f.inventory->cursor == f.a && f.crafted == 0);
        CHECK(kind == 0 ? f.saved_result[0] == NULL : f.saved_grid[0] == NULL);
        CHECK(destroy(&f));
    }

    CHECK(create(&f, r));
    f.a = stack(17, 2, 0);
    f.null_grid_on_crafted = true;
    CHECK(f.a != NULL &&
          mc_inventory_crafting_set(f.container->crafting, 0, f.a, NULL) == MC_INVENTORY_OK);
    CHECK(click(&f, 0, 0, false, &e) == MC_INVENTORY_NULL_REFERENCE && e.source_exception &&
          e.child.pc == 235 && strcmp(e.child.class_name, "yv") == 0 && e.dependency.pc == 4 &&
          strcmp(e.dependency.class_name, "mq") == 0 && f.crafted == 1 && f.crafted_count == 4 &&
          f.observed_ingredient && fields(f.a, 17, 2) && fields(f.inventory->cursor, 5, 4));
    CHECK(destroy(&f));

    CHECK(create(&f, r));
    f.a = stack(17, 2, 0);
    f.null_grid_on_drop = true;
    CHECK(f.a != NULL &&
          mc_inventory_crafting_set(f.container->crafting, 0, f.a, NULL) == MC_INVENTORY_OK);
    CHECK(mc_container_close_begin(f.container, NULL, &f.host, &f.call, &e) ==
              MC_INVENTORY_NULL_REFERENCE &&
          e.source_exception && e.site.pc == 36 && e.dependency.pc == 6 && f.drops == 1 &&
          f.saved_grid[0] == f.a && fields(f.a, 17, 2));
    CHECK(destroy(&f));

    CHECK(create(&f, r));
    f.a = stack(17, 2, 0);
    f.null_result_on_crafted = true;
    CHECK(f.a != NULL &&
          mc_inventory_crafting_set(f.container->crafting, 0, f.a, NULL) == MC_INVENTORY_OK);
    CHECK(click(&f, 0, 0, false, &e) == MC_INVENTORY_NULL_REFERENCE && e.source_exception &&
          strcmp(e.child.class_name, "yv") == 0 && e.child.pc == 264 && f.crafted == 1 &&
          f.crafted_count == 4 && fields(f.a, 17, 1) && fields(f.inventory->cursor, 5, 4) &&
          e.inventory.mutation_reached && e.inventory.notification_reached &&
          e.inventory.frame_count == 3 && e.inventory.frames[0].pc == 81 &&
          strcmp(e.inventory.frames[0].class_name, "mq") == 0 && e.inventory.frames[1].pc == 15 &&
          strcmp(e.inventory.frames[2].class_name, "wl") == 0 && e.inventory.frames[2].pc == 6);
    CHECK(destroy(&f));

    CHECK(create(&f, r));
    f.saved_grid = f.container->crafting->slots;
    f.container->crafting->slots = NULL;
    CHECK(mc_container_close_begin(f.container, NULL, &f.host, &f.call, &e) ==
              MC_INVENTORY_NULL_REFERENCE &&
          e.source_exception && e.site.pc == 17 && e.dependency.pc == 4 && f.drops == 0);
    CHECK(destroy(&f));

    CHECK(create(&f, r));
    f.a = stack(3, 1, 0);
    CHECK(f.a != NULL);
    mc_registry_item_array *items = r->items.c;
    mc_item *dirt = items->elements[3];
    items->elements[3] = NULL;
    int32_t maximum = -123;
    mc_container_interaction_error_init(&e);
    const bool null_element = inv_interaction_max(&e, r, f.a, &maximum);
    items->elements[3] = dirt;
    CHECK(!null_element && e.source_exception && maximum == -123 &&
          strcmp(e.child.class_name, "iz") == 0 && strcmp(e.child.method, "c") == 0 &&
          e.child.pc == 4);
    r->items.c = NULL;
    mc_container_interaction_error_init(&e);
    const bool null_array = inv_interaction_max(&e, r, f.a, &maximum);
    r->items.c = items;
    CHECK(!null_array && e.source_exception && maximum == -123 && e.child.pc == 1 &&
          strcmp(e.child.method, "c") == 0 && e.dependency.pc == 7 &&
          strcmp(e.dependency.method, "a") == 0);
    mc_container_interaction_error_init(&e);
    CHECK(inv_interaction_max(&e, r, f.a, &maximum) && maximum == 64);
    CHECK(destroy(&f));
    ok = true;
cleanup:
    (void)destroy(&f);
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
        !controls(classes))
        goto cleanup;
    printf("Container interaction kernel controls passed: %u\n", checks);
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
