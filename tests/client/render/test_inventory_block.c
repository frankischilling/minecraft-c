/* Real Registry/NW owners with no Display: check the selected inventory
 * leaves and failure prefixes independently of graphical SDK scenarios. */
int mc_inventory_faces_fixture_main(void);
#define main mc_inventory_faces_fixture_main
#include "test_render_blocks_faces.c"
#undef main

#include "client/render/item_overlay.h"
#include "item/render/internal.h"
#include "mc/item/registry/runtime.h"

static bool inventory_controls(void) {
    fixture f = {0};
    CHECK(mc_materials_create(&f.materials));
    CHECK(mc_java_random_profile_create(SIZE_MAX, &f.profile) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_entity_classes_create(MC_ENTITY_CLIENT, f.profile, NULL, &f.entities) == MC_ENTITY_OK);
    static const uint8_t language[] = "stat.mineBlock=Mine %1$s\nstat.useItem=Use %1$s\n"
                                      "stat.breakItem=Break %1$s\nstat.craftItem=Craft %1$s\n";
    resources r = {.streams = {{language, sizeof language - 1, 0, false}, {0}, {0}}};
    const mc_registry_resources api = {&r, open_resource, close_resource, ready_resource};
    const mc_registry_options options = {MC_REGISTRY_CLIENT, f.profile, f.materials, 0, &api};
    CHECK(mc_registry_classes_allocate(&options, &f.registry, NULL) == MC_REGISTRY_OK);
    CHECK(mc_registry_classes_bind_entities(f.registry, f.entities, NULL) == MC_REGISTRY_OK);
    CHECK(mc_registry_classes_initialize_blocks(f.registry, NULL) == MC_REGISTRY_OK);
    const mc_block *dirt = NULL;
    CHECK(mc_registry_block_get(f.registry, 3, &dirt, NULL) == MC_REGISTRY_OK && dirt != NULL);
    int32_t color = 0, icon = -1;
    CHECK(mc_block_render_inventory_color(dirt, 7, &color) == MC_REGISTRY_OK && color == 0xffffff);
    CHECK(mc_block_render_bounds((mc_block *)(void *)dirt, .125f, .25f, .375f, .75f, .875f, 1) ==
          MC_REGISTRY_OK);
    const mc_block_fields before = *mc_block_fields_get(dirt);
    CHECK(mc_block_render_inventory_shape(dirt) == MC_REGISTRY_OK &&
          memcmp(&before, mc_block_fields_get(dirt), sizeof before) == 0);
    const mc_item *item;
    const mc_registry_item_array *items;
    CHECK(mc_registry_item_c_get(f.registry, &items, NULL) == MC_REGISTRY_OK &&
          mc_registry_item_c_at(f.registry, items, 3, &item, NULL) == MC_REGISTRY_OK &&
          item != NULL);
    mc_item_render_error item_error;
    CHECK(mc_item_render_icon(f.registry, item, 7, &icon, &item_error) && item_error.returned &&
          icon == 2);
    bool flag = true;
    CHECK(mc_item_render_full_3d(f.registry, item, &flag, &item_error) && !flag);
    flag = true;
    CHECK(mc_item_render_rotate(f.registry, item, &flag, &item_error) && !flag);
    CHECK(mc_item_render_color(f.registry, item, 7, &color, &item_error) && color == 0xffffff);
    CHECK(mc_client_render_classes_create(f.entities, f.profile, &f.models) == MC_CLIENT_MODEL_OK);
    mc_client_model_error model_error;
    CHECK(mc_client_render_blocks_active_use(f.models, &model_error));
    mc_client_item_renderer *renderer = NULL;
    CHECK(mc_client_item_renderer_allocate(f.models, 0, &renderer) == MC_CLIENT_MODEL_OK &&
          mc_client_item_renderer_construct(renderer, &model_error) == MC_CLIENT_MODEL_OK);
    mc_client_item_overlay_context overlay = {.owner = renderer};
    CHECK(mc_client_item_overlay_draw(&overlay) && overlay.returned && overlay.null_stack &&
          overlay.source_pc == 4 && !overlay.count_read && !overlay.durability_returned);
    mc_item_stack *stack = NULL;
    CHECK(mc_item_stack_allocate(&stack) == MC_ITEM_STACK_OK &&
          mc_item_stack_construct(stack, 3, 1, 0) == MC_ITEM_STACK_OK);
    overlay =
        (mc_client_item_overlay_context){.owner = renderer, .registry = f.registry, .stack = stack};
    CHECK(mc_client_item_overlay_draw(&overlay) && overlay.returned && !overlay.null_stack &&
          overlay.source_pc == 288 && overlay.count_read && overlay.count == 1 &&
          !overlay.count_drawn && overlay.durability_returned && overlay.maximum == 0 &&
          !overlay.damage_read && !overlay.damaged);
    mc_item_stack_release(stack);
    CHECK(mc_client_item_renderer_free(&renderer) == MC_CLIENT_MODEL_OK && renderer == NULL);
    CHECK(mc_client_render_engine_classes_create(NULL, &f.graphics) == MC_CLIENT_RENDER_ENGINE_OK);
    mc_client_graphics_error graphics;
    CHECK(mc_client_nw_active_use(f.graphics,
                                  (mc_client_graphics_site){"native-test", "inventory", "()V", 0},
                                  &f.nw, &graphics));
    for (int32_t type = -2; type < 20; ++type)
        CHECK(mc_cv_inventory_type_3d(type) ==
              (type == 0 || type == 10 || type == 11 || type == 13 || type == 16));
    f.context = calloc(1, sizeof(*f.context));
    CHECK(f.context != NULL);
    for (unsigned int which = 0; which < 4; ++which) {
        mc_client_render_blocks_construct_fields(&f.fields, NULL, false);
        f.fields.b = (which & 1u) == 0;
        *f.context = (mc_cv_context){.fields = &f.fields,
                                     .registry = f.registry,
                                     .models = f.models,
                                     .graphics = f.graphics};
        const bool null_block = which < 2;
        CHECK(!mc_cv_inventory(f.context, null_block ? NULL : dirt, 7, .75f));
        const unsigned int expected =
            null_block ? (f.fields.b ? 14u : 78u) : (f.fields.b ? 74u : 114u);
        CHECK(f.context->failed && !f.context->returned && f.context->depth == 1 &&
              f.context->source_pc == expected && f.context->inventory_faces == 0 &&
              !f.nw->fields.w && f.nw->fields.h == 0 &&
              strcmp(f.context->failure.source_descriptor, "(Luu;IF)V") == 0 &&
              memcmp(&before, mc_block_fields_get(dirt), sizeof before) == 0);
    }
    CHECK(mc_client_render_engine_classes_release(f.graphics) == MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(mc_client_render_classes_free(f.models) == MC_CLIENT_MODEL_OK);
    CHECK(mc_registry_classes_free(f.registry, NULL) == MC_REGISTRY_OK);
    CHECK(mc_entity_classes_free(f.entities) == MC_ENTITY_OK);
    mc_java_random_profile_free(f.profile);
    mc_materials_destroy(f.materials);
    free(f.context);
    CHECK(!r.invalid && r.opens == 3 && r.closes == 1);
    return true;
}

int main(void) {
    if (!inventory_controls())
        return 1;
    printf("Inventory Block controls: %u checks; real Dirt/BlockItem/NW failure prefixes\n",
           checks);
    return 0;
}
