/* Real saved inputs -> UE recipes -> placed/opened OO -> IT crafting/close. */
#include <stdbool.h>
static int wb_statistics_controls(const void *, bool);
#define MC_WORLD_SELECTION_STATISTICS_CONTROLS(f, initial) wb_statistics_controls((f), (initial))
int mc_workbench_included_inventory_main(int, char **);
#define MC_INVENTORY_FIXTURE_MAIN mc_workbench_included_inventory_main
#include "test_inventory.c"
#undef MC_INVENTORY_FIXTURE_MAIN
#include "client/gui/source_workbench_drawing.h"
#include "client/gui/source_workbench_screen_internal.h"
#include "client/render/model_graph_internal.h"

typedef struct workbench_work {
    mining_work mining;
    bool shift_down, saving, save_selected;
    size_t save_samples, ui_frames;
    uint64_t ui_ticks;
} workbench_work;

static workbench_work *workbench;

static int wb_stack(const mc_item_stack *stack, int32_t id, int32_t count) {
    if (count == 0)
        CHECK(stack == NULL);
    else {
        mc_item_stack_fields fields;
        CHECK(stack != NULL && mc_item_stack_fields_get(stack, &fields) == MC_ITEM_STACK_OK &&
              fields.id == id && fields.count == count && fields.damage == 0);
    }
    return 0;
}

static int wb_cursor(fixture *f, int32_t id, int32_t count) {
    const mc_item_stack *stack = NULL;
    CHECK(mc_inventory_player_source_cursor_get(f->app->h->local_player->fields.player.c, &stack,
                                                NULL) == MC_INVENTORY_OK &&
          wb_stack(stack, id, count) == 0);
    return 0;
}

static int wb_slot(mc_client_source_id *o, int32_t index, int32_t id, int32_t count) {
    mc_inventory_slot *slot = NULL;
    const mc_item_stack *stack = NULL;
    CHECK(mc_container_source_slot(o->id.j, index, &slot, NULL) == MC_INVENTORY_OK &&
          mc_inventory_slot_source_get(slot, &stack, NULL) == MC_INVENTORY_OK &&
          wb_stack(stack, id, count) == 0);
    return 0;
}

static int wb_ui(fixture *f, mc_client_source_id *o, int32_t cursor_id, int32_t cursor_count) {
    const bool table = o->base.kind == MC_CLIENT_SCREEN_WORKBENCH;
    const mc_client_inventory_draw_state *draw = f->state.inventory;
    mc_client_sdk_input_state input;
    CHECK(f->app->r == &o->base && !f->app->N && !atomic_load(&f->app->o) && draw != NULL &&
          draw->returned && draw->id_returned && draw->screen_receiver == &o->base &&
          draw->workbench == table && draw->background_returned && draw->foreground_returned &&
          draw->slots_drawn == (table ? 46u : 45u) && draw->cursor_drawn == (cursor_count != 0) &&
          f->app->h->local_player->fields.player.e == o->id.j &&
          mc_client_sdk_input_inspect(mc_client_display_inputs(f->app->display), &input) &&
          !input.grabbed && wb_cursor(f, cursor_id, cursor_count) == 0);
    if (table)
        CHECK(draw->receiver == NULL && !draw->preview_returned && !draw->pose_restored &&
              !draw->mouse_stored && draw->preview == NULL && draw->source_pc == 490 &&
              o->id.j != f->app->h->local_player->fields.player.d);
    else
        CHECK(draw->receiver != NULL && &draw->receiver->container == o && draw->preview_returned &&
              draw->pose_restored && draw->mouse_stored &&
              o->id.j == f->app->h->local_player->fields.player.d);
    return 0;
}

static int wb_wait(fixture *f, mc_client_screen_kind kind, mc_client_source_id **out) {
    const uint64_t deadline = SDL_GetTicks() + 15000;
    do {
        CHECK(inventory_frame(f) == 0);
        if (f->app->r != NULL && f->app->r->kind == kind) {
            if (kind == MC_CLIENT_SCREEN_INVENTORY) {
                *out = &((mc_client_source_ue *)f->app->r)->container;
                ++inventory_opened;
            } else {
                CHECK(kind == MC_CLIENT_SCREEN_WORKBENCH);
                *out = &((mc_client_source_oo *)f->app->r)->container;
            }
            CHECK(wb_ui(f, *out, 0, 0) == 0);
            return 0;
        }
    } while (SDL_GetTicks() < deadline);
    fprintf(stderr, "Workbench requested screen deadline kind%d\n", (int)kind);
    return 1;
}

static int wb_closed(fixture *f) {
    const uint64_t deadline = SDL_GetTicks() + 15000;
    do {
        CHECK(inventory_frame(f) == 0);
        if (f->app->r == NULL && f->app->N && !atomic_load(&f->app->o))
            return 0;
    } while (SDL_GetTicks() < deadline);
    return 1;
}

static int wb_shift(fixture *f, bool down) {
    CHECK(gameplay_key(f, SDL_SCANCODE_LSHIFT, down) == 0 && transfer(f) == 0);
    workbench->shift_down = down;
    const uint64_t deadline = SDL_GetTicks() + 15000;
    bool held = !down;
    mc_client_sdk_input_error error;
    do {
        CHECK(inventory_frame(f) == 0 &&
              mc_client_sdk_keyboard_key_down(mc_client_display_inputs(f->app->display), 42, &held,
                                              &error) == MC_CLIENT_SDK_INPUT_OK);
    } while (held != down && SDL_GetTicks() < deadline);
    CHECK(held == down);
    return 0;
}

static int wb_click(fixture *f, mc_client_source_id *o, int32_t index, bool right, bool shift,
                    int32_t cursor_id, int32_t cursor_count) {
    if (shift)
        CHECK(wb_shift(f, true) == 0);
    mc_container_slot_state slot;
    CHECK(mc_container_slot_read(o->id.j, index, &slot, NULL) == MC_INVENTORY_OK);
    const int32_t x = mc_java_i32_sub(o->base.fields.c, o->id.a) / 2 + slot.x + 8;
    const int32_t y = mc_java_i32_sub(o->base.fields.d, o->id.i) / 2 + slot.y + 8;
    SDL_Window *window = mc_client_display_window(f->app->display);
    int width, height;
    CHECK(SDL_GetWindowSize(window, &width, &height));
    const float native_x = (float)((int64_t)x * width / o->base.fields.c);
    const float native_y = (float)((int64_t)y * height / o->base.fields.d);
    SDL_WarpMouseInWindow(window, native_x, native_y);
    SDL_Event motion = {0};
    motion.type = SDL_EVENT_MOUSE_MOTION;
    motion.motion.windowID = SDL_GetWindowID(window);
    motion.motion.x = native_x;
    motion.motion.y = native_y;
    CHECK(SDL_PushEvent(&motion));
    for (unsigned int i = 0; i < 2; ++i) {
        SDL_Event event = {0};
        event.type = i == 0 ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
        event.button.windowID = SDL_GetWindowID(window);
        event.button.button = right ? SDL_BUTTON_RIGHT : SDL_BUTTON_LEFT;
        event.button.down = i == 0;
        event.button.x = native_x;
        event.button.y = native_y;
        CHECK(SDL_PushEvent(&event));
    }
    CHECK(transfer(f) == 0);
    const size_t before = o->source_calls;
    const uint64_t deadline = SDL_GetTicks() + 15000;
    while (o->source_calls == before && SDL_GetTicks() < deadline)
        CHECK(inventory_frame(f) == 0);
    mc_player_container_state call;
    CHECK(o->source_calls == before + 1 && o->calls != NULL &&
          mc_player_container_inspect(o->calls->call, &call) && call.returned &&
          call.interaction != NULL && call.interaction->returned &&
          call.interaction->slot == index && call.interaction->button == (right ? 1 : 0) &&
          call.interaction->shift == shift && wb_ui(f, o, cursor_id, cursor_count) == 0);
    if (shift) {
        bool held = false;
        mc_client_sdk_input_error error;
        CHECK(mc_client_sdk_keyboard_key_down(mc_client_display_inputs(f->app->display), 42, &held,
                                              &error) == MC_CLIENT_SDK_INPUT_OK &&
              held && wb_shift(f, false) == 0);
    }
    return 0;
}

static int wb_stat(const fixture *f, int32_t id, int32_t expected, bool used) {
    mc_registry_statistic_view statistics;
    int32_t value;
    CHECK(mc_registry_classes_inspect_statistics(f->app->registry, &statistics, NULL) ==
          MC_REGISTRY_OK);
    mc_statistic *const *array = used ? statistics.E : statistics.D;
    CHECK(array != NULL && array[id] != NULL &&
          mc_client_stats_writer_value(f->app->stats_writer, array[id], &value, NULL) ==
              MC_CLIENT_STATS_OK &&
          value == expected);
    return 0;
}

static int wb_achievement(const fixture *f, size_t index, int32_t expected) {
    mc_registry_achievement_view achievements;
    int32_t value;
    CHECK(mc_registry_classes_inspect_achievements(f->app->registry, &achievements, NULL) ==
              MC_REGISTRY_OK &&
          achievements.named != NULL &&
          mc_client_stats_writer_value(f->app->stats_writer, achievements.named[index], &value,
                                       NULL) == MC_CLIENT_STATS_OK &&
          value == expected);
    return 0;
}

static int wb_notice(const fixture *f, size_t index) {
    mc_registry_achievement_view achievements;
    const mc_client_achievement_notice_fields *notice =
        mc_client_achievement_notice_fields_get(f->app->achievement_notice);
    CHECK(mc_registry_classes_inspect_achievements(f->app->registry, &achievements, NULL) ==
              MC_REGISTRY_OK &&
          achievements.named != NULL && notice != NULL && notice->a == f->app &&
          notice->f == achievements.named[index] && !notice->i && notice->d != NULL &&
          notice->e == mc_statistic_fields_get(achievements.named[index])->f);
    return 0;
}

static int wb_statistics_controls(const void *context, bool initial) {
    const fixture *f = context;
    CHECK(wb_achievement(f, 0, 1 + inventory_opened) == 0 && wb_achievement(f, 1, 1) == 0);
    if (initial) {
        mc_client_stats_state state;
        const mc_client_achievement_notice_fields *notice =
            mc_client_achievement_notice_fields_get(f->app->achievement_notice);
        CHECK(mc_client_stats_writer_read_state(f->app->stats_writer, &state) &&
              state.phase == MC_CLIENT_STATS_READY && state.total_count == 2 &&
              state.unsent_count == 2 && state.dirty && notice != NULL && notice->a == f->app &&
              notice->f == NULL && notice->g == 0 && !notice->i && notice->d == NULL &&
              notice->e == NULL && wb_achievement(f, 2, 0) == 0 && wb_achievement(f, 3, 0) == 0);
    } else
        CHECK(wb_achievement(f, 2, 1) == 0 && wb_achievement(f, 3, 1) == 0 && wb_notice(f, 3) == 0);
    return 0;
}

static int wb_craft_ue(fixture *f, mc_client_source_id *o) {
    CHECK(inventory_field(f, 9, 17, 4) == 0 && wb_achievement(f, 1, 1) == 0 &&
          wb_achievement(f, 2, 0) == 0 && wb_achievement(f, 3, 0) == 0 &&
          wb_click(f, o, 9, false, false, 17, 4) == 0 &&
          wb_click(f, o, 1, false, false, 0, 0) == 0 && wb_slot(o, 1, 17, 4) == 0);
    for (int32_t i = 1; i <= 4; ++i) {
        CHECK(wb_slot(o, 0, 5, 4) == 0 && wb_click(f, o, 0, false, false, 5, 4) == 0 &&
              wb_click(f, o, 10, false, false, 0, 0) == 0 &&
              inventory_field(f, 10, 5, i * 4) == 0 && wb_slot(o, 1, i == 4 ? 0 : 17, 4 - i) == 0);
    }
    CHECK(wb_stat(f, 5, 16, false) == 0 && wb_click(f, o, 10, false, false, 5, 16) == 0);
    for (int32_t i = 1; i <= 4; ++i)
        CHECK(wb_click(f, o, i, true, false, 5, 16 - i) == 0 && wb_slot(o, i, 5, 1) == 0);
    CHECK(wb_click(f, o, 10, false, false, 0, 0) == 0 && wb_slot(o, 0, 58, 1) == 0 &&
          wb_click(f, o, 0, false, false, 58, 1) == 0 &&
          wb_click(f, o, 37, false, false, 0, 0) == 0 && inventory_field(f, 1, 58, 1) == 0 &&
          wb_stat(f, 58, 1, false) == 0 && wb_achievement(f, 2, 1) == 0 && wb_notice(f, 2) == 0 &&
          wb_click(f, o, 10, false, false, 5, 12) == 0 &&
          wb_click(f, o, 1, true, false, 5, 11) == 0 &&
          wb_click(f, o, 3, true, false, 5, 10) == 0 &&
          wb_click(f, o, 10, false, false, 0, 0) == 0 && wb_slot(o, 0, 280, 4) == 0 &&
          wb_click(f, o, 0, false, false, 280, 4) == 0 &&
          wb_click(f, o, 11, false, false, 0, 0) == 0 && inventory_field(f, 10, 5, 10) == 0 &&
          inventory_field(f, 11, 280, 4) == 0 && wb_stat(f, 280, 4, false) == 0);
    for (int32_t i = 0; i <= 4; ++i)
        CHECK(wb_slot(o, i, 0, 0) == 0);
    CHECK(mining_capture(f, &workbench->mining, "workbench-crafted-2x2.png") == 0);
    puts("WORKBENCH_UE logs=4 planks-crafted=16 table-crafted=1 sticks-crafted=4 ingredients=1 "
         "notices=1");
    return 0;
}

static bool wb_hit(fixture *f, int32_t id, int32_t z) {
    mc_entity_hit_result_view hit;
    mc_entity_hit_kind_class_view kinds;
    int32_t block;
    return f->app->y != NULL && mc_entity_hit_result_inspect(f->app->y, &hit) == MC_ENTITY_HIT_OK &&
           mc_entity_hit_kind_class_inspect(f->app->entities, &kinds) == MC_ENTITY_HIT_OK &&
           hit.fields.a == kinds.a && hit.fields.b == 0 && hit.fields.c == 64 &&
           hit.fields.d == z && hit.fields.e == 2 &&
           mc_world_runtime_block_get(f->app->active_world, 0, 64, z, &block, NULL) ==
               MC_WORLD_STORAGE_OK &&
           block == id;
}

static int wb_world_right(fixture *f, bool opening) {
    const uint64_t clicks = f->state.gameplay_right_clicks;
    const uint64_t placed = f->state.gameplay_placed_blocks;
    const uint64_t deadline = SDL_GetTicks() + 60000;
    for (size_t attempt = 0; attempt < 12 && SDL_GetTicks() < deadline; ++attempt) {
        CHECK(wb_hit(f, opening ? 58 : 3, opening ? 1 : 2));
        for (unsigned int i = 0; i < 2; ++i) {
            SDL_Event event = {0};
            event.type = i == 0 ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
            event.button.windowID = SDL_GetWindowID(mc_client_display_window(f->app->display));
            event.button.button = SDL_BUTTON_RIGHT;
            event.button.down = i == 0;
            CHECK(SDL_PushEvent(&event));
        }
        CHECK(transfer(f) == 0);
        const size_t events = f->state.mouse_events;
        for (size_t frame_index = 0; frame_index < 64 && SDL_GetTicks() < deadline; ++frame_index) {
            CHECK(inventory_frame(f) == 0);
            if (opening && f->app->r != NULL && f->app->r->kind == MC_CLIENT_SCREEN_WORKBENCH) {
                CHECK(f->state.gameplay_placed_blocks == placed);
                return 0;
            }
            if (!opening && f->state.gameplay_placed_blocks == placed + 1) {
                CHECK(f->state.gameplay_right_clicks > clicks && inventory_field(f, 1, 0, 0) == 0 &&
                      wb_stat(f, 58, 1, true) == 0);
                return 0;
            }
            if (f->state.mouse_events > events)
                break;
        }
        CHECK(f->state.gameplay_placed_blocks == placed);
    }
    fprintf(stderr, "Workbench actual world right-click deadline open%d\n", (int)opening);
    return 1;
}

static int wb_craft_oo(fixture *f, mc_client_source_id *o) {
    mc_world_info_fields before, after;
    CHECK(mc_world_info_fields_get(f->app->active_world->fields.x, &before) == MC_WORLD_INFO_OK);
    for (size_t i = 0; i < 3; ++i) {
        CHECK(inventory_frame(f) == 0 && wb_ui(f, o, 0, 0) == 0);
        ++workbench->ui_frames;
    }
    const uint64_t deadline = SDL_GetTicks() + 10000;
    do {
        CHECK(mc_world_info_fields_get(f->app->active_world->fields.x, &after) == MC_WORLD_INFO_OK);
        if (mc_java_i64_sub(after.value.time, before.value.time) >= 3)
            break;
        CHECK(inventory_frame(f) == 0 && wb_ui(f, o, 0, 0) == 0);
        ++workbench->ui_frames;
    } while (SDL_GetTicks() < deadline);
    CHECK(mc_java_i64_sub(after.value.time, before.value.time) >= 3);
    workbench->ui_ticks = (uint64_t)mc_java_i64_sub(after.value.time, before.value.time);
    CHECK(mining_capture(f, &workbench->mining, "workbench-open-3x3.png") == 0 &&
          wb_click(f, o, 11, false, true, 0, 0) == 0 && inventory_field(f, 0, 5, 10) == 0 &&
          inventory_field(f, 10, 0, 0) == 0 && wb_click(f, o, 37, false, true, 0, 0) == 0 &&
          inventory_field(f, 9, 5, 10) == 0 && inventory_field(f, 0, 0, 0) == 0 &&
          wb_click(f, o, 10, false, false, 5, 10) == 0);
    for (int32_t i = 1; i <= 3; ++i)
        CHECK(wb_click(f, o, i, true, false, 5, 10 - i) == 0 && wb_slot(o, i, 5, 1) == 0);
    CHECK(wb_click(f, o, 10, false, false, 0, 0) == 0 &&
          wb_click(f, o, 12, false, false, 280, 4) == 0 &&
          wb_click(f, o, 5, true, false, 280, 3) == 0 &&
          wb_click(f, o, 8, true, false, 280, 2) == 0 &&
          wb_click(f, o, 12, false, false, 0, 0) == 0 && wb_slot(o, 0, 270, 1) == 0 &&
          mining_capture(f, &workbench->mining, "workbench-pickaxe-recipe.png") == 0 &&
          wb_click(f, o, 0, false, false, 270, 1) == 0 &&
          wb_click(f, o, 37, false, false, 0, 0) == 0 && inventory_field(f, 0, 270, 1) == 0 &&
          wb_stat(f, 270, 1, false) == 0 && wb_achievement(f, 3, 1) == 0 && wb_notice(f, 3) == 0);
    for (int32_t i = 0; i <= 9; ++i)
        CHECK(wb_slot(o, i, 0, 0) == 0);
    CHECK(inventory_field(f, 9, 5, 7) == 0 && inventory_field(f, 11, 280, 2) == 0 &&
          wb_click(f, o, 12, false, false, 280, 2) == 0 &&
          wb_click(f, o, 1, false, false, 0, 0) == 0 && wb_slot(o, 1, 280, 2) == 0 &&
          wb_click(f, o, 10, false, false, 5, 7) == 0 &&
          mining_capture(f, &workbench->mining, "workbench-cursor-grid-before-close.png") == 0);
    puts("WORKBENCH_OO slots=46 shift-main-hotbar=1 shift-hotbar-main=1 pickaxe-crafted=1 "
         "ingredients=1 notices=1");
    return 0;
}

static int wb_close(fixture *f, mc_client_source_id *o) {
    const mc_item_stack *cursor = NULL, *grid = NULL;
    mc_container_state container;
    CHECK(mc_inventory_player_source_cursor_get(f->app->h->local_player->fields.player.c, &cursor,
                                                NULL) == MC_INVENTORY_OK &&
          mc_container_read_state(o->id.j, &container) &&
          mc_inventory_crafting_borrow_slot(container.crafting, 0, &grid, NULL) ==
              MC_INVENTORY_OK &&
          wb_stack(cursor, 5, 7) == 0 && wb_stack(grid, 280, 2) == 0 &&
          inventory_key(f, SDL_SCANCODE_ESCAPE) == 0 && wb_closed(f) == 0 &&
          f->app->h->local_player->fields.player.e == f->app->h->local_player->fields.player.d &&
          o->inherited_closed_returned && o->direct_closed_returned && o->closed_parent != NULL &&
          o->closed_direct != NULL);
    mc_player_container_state parent, direct;
    mc_player_drop_state cursor_drop, grid_drop;
    CHECK(mc_player_container_inspect(o->closed_parent->call, &parent) && parent.returned &&
          parent.container == f->app->h->local_player->fields.player.d && parent.drop_calls == 1 &&
          mc_player_container_inspect(o->closed_direct->call, &direct) && direct.returned &&
          direct.direct_target && direct.container == o->id.j && direct.drop_calls == 1 &&
          mc_player_container_drop_inspect(o->closed_parent->call, 0, &cursor_drop) &&
          mc_player_container_drop_inspect(o->closed_direct->call, 0, &grid_drop));
    const mc_player_drop_state *drops[] = {&cursor_drop, &grid_drop};
    const mc_item_stack *stacks[] = {cursor, grid};
    for (size_t i = 0; i < 2; ++i) {
        CHECK(drops[i]->returned && drops[i]->publication_returned && drops[i]->inserted &&
              drops[i]->statistic_returned && drops[i]->stack == stacks[i] &&
              drops[i]->item != NULL && drops[i]->item->stack == stacks[i] &&
              drops[i]->item->base.world == f->app->active_world);
        const mc_world_reference_list *live = f->app->active_world->fields.b;
        size_t occurrences = 0;
        CHECK(live != NULL && live->count <= live->capacity);
        for (size_t j = 0; j < live->count; ++j)
            if (live->items[j] == drops[i]->item)
                ++occurrences;
        CHECK(occurrences == 1);
    }
    CHECK(wb_cursor(f, 0, 0) == 0 && wb_slot(o, 1, 280, 2) == 0);
    puts("WORKBENCH_CLOSE parent-default=1 direct-it=1 cursor-planks=7 grid-sticks=2 same-stacks=1 "
         "publications=2 grid-retained=1 default-restored=1");
    return 0;
}

static uint64_t wb_double_bits(double value) {
    uint64_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static uint32_t wb_float_bits(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static int wb_raw_save(fixture *f, FILE *raw) {
    mc_inventory_player_state inventory;
    CHECK(mc_inventory_player_read_state(f->app->h->local_player->fields.player.c, &inventory) &&
          inventory.main_slots != NULL && inventory.armor_slots != NULL &&
          fprintf(raw, "{\"inventory\":[") >= 0);
    size_t count = 0;
    for (size_t i = 0; i < 40; ++i) {
        const mc_item_stack *stack =
            i < 36 ? inventory.main_slots[i] : inventory.armor_slots[i - 36];
        if (stack == NULL)
            continue;
        mc_item_stack_fields fields;
        CHECK(mc_item_stack_fields_get(stack, &fields) == MC_ITEM_STACK_OK &&
              fprintf(raw, "%s[%zu,%" PRId32 ",%" PRId32 ",%" PRId32 "]", count == 0 ? "" : ",",
                      i < 36 ? i : i + 64, fields.id, fields.count, fields.damage) >= 0);
        ++count;
    }
    CHECK(fprintf(raw, "],\"items\":[") >= 0);
    const mc_world_reference_list *list = f->app->active_world->fields.b;
    CHECK(list != NULL && list->count <= list->capacity && list->count <= 4096);
    count = 0;
    for (size_t i = 0; i < list->count; ++i) {
        mc_entity *e = list->items[i];
        if (e == NULL || !mc_entity_is_kind(e, MC_ENTITY_CLASS_KIND_ITEM) || e->base.be)
            continue;
        mc_item_stack_fields fields;
        CHECK(count < 128 && mc_item_stack_fields_get(e->stack, &fields) == MC_ITEM_STACK_OK &&
              fprintf(raw,
                      "%s{\"stack\":[%" PRId32 ",%" PRId32 ",%" PRId32 "],"
                      "\"pos\":[\"%016" PRIx64 "\",\"%016" PRIx64 "\",\"%016" PRIx64 "\"],"
                      "\"ysize\":\"%08" PRIx32 "\",\"motion\":[\"%016" PRIx64 "\",\"%016" PRIx64
                      "\",\"%016" PRIx64 "\"],"
                      "\"rotation\":[\"%08" PRIx32 "\",\"%08" PRIx32 "\"],\"fall\":\"%08" PRIx32
                      "\","
                      "\"fire\":%" PRId32 ",\"air\":%" PRId32 ",\"ground\":%s,\"health\":%" PRId32
                      ",\"age\":%" PRId32 "}",
                      count == 0 ? "" : ",", fields.id, fields.count, fields.damage,
                      wb_double_bits(e->base.x), wb_double_bits(e->base.y),
                      wb_double_bits(e->base.z), wb_float_bits(e->base.y_size),
                      wb_double_bits(e->base.velocity_x), wb_double_bits(e->base.velocity_y),
                      wb_double_bits(e->base.velocity_z), wb_float_bits(e->base.yaw),
                      wb_float_bits(e->base.aT), wb_float_bits(e->base.bk), e->base.bv, e->base.bz,
                      e->base.aX ? "true" : "false", e->item.f, e->item.b) >= 0);
        char name[64];
        const int n = snprintf(name, sizeof(name), "workbench-items-%04zu.nbt", count);
        CHECK(n > 0 && (size_t)n < sizeof(name) &&
              mining_semantic(e, &workbench->mining, name) == 0);
        ++count;
    }
    CHECK(fprintf(raw, "]}\n") >= 0);
    char path[8192], text[32];
    const int n = snprintf(text, sizeof(text), "%zu\n", count);
    CHECK(n > 0 && (size_t)n < sizeof(text) &&
          mining_path(&workbench->mining, "workbench-items.count", path) == 0 &&
          SDL_SaveFile(path, text, (size_t)n));
    return 0;
}

static int wb_save_before(fixture *f) {
    if (!workbench->saving || workbench->save_selected || f->app->active_world == NULL ||
        f->app->r == NULL || f->app->r->kind != MC_CLIENT_SCREEN_PAUSE || f->app->N ||
        !atomic_load(&f->app->o))
        return 0;
    char path[8192];
    CHECK(mining_semantic(f->app->h, &workbench->mining, "workbench-player-entry.nbt") == 0 &&
          mining_path(&workbench->mining, "workbench-raw-entry.json", path) == 0);
    FILE *raw = fopen(path, "wb");
    CHECK(raw != NULL);
    const int result = wb_raw_save(f, raw);
    const int closed = fclose(raw);
    CHECK(result == 0 && closed == 0);
    ++workbench->save_samples;
    return 0;
}

static void wb_save_after(fixture *f, bool returned) {
    if (workbench->saving && returned && f->app->active_world == NULL && f->app->h == NULL &&
        f->app->r != NULL && f->app->r->kind == MC_CLIENT_SCREEN_MAIN_MENU)
        workbench->save_selected = true;
}

static int wb_world_ready(fixture *f) {
    /* Original N delays two Entity passes before TH receives Engine/camera/
     * Settings. A cold UE preview can throw in both its caught texture child
     * and its uncaught name suffix. This successful-crafting scenario starts
     * after that genuine N->TH frame; no cache/Source field is populated here. */
    const uint64_t deadline = SDL_GetTicks() + 15000;
    while (SDL_GetTicks() < deadline) {
        CHECK(inventory_frame(f) == 0);
        const mc_client_render_global_entities_state *entities =
            f->state.world_frame == NULL ? NULL : f->state.world_frame->entities;
        if (entities == NULL || entities->delayed || !entities->th_returned)
            continue;
        mc_client_manager_state manager;
        /* The returned N receipt keeps scalar progress; its borrowed th view
         * ends at checked N retirement. Inspect the still-live class owner. */
        CHECK(f->app->render_classes != NULL &&
              mc_client_render_manager_inspect(f->app->render_classes->th, &manager) &&
              manager.source_views_available && manager.phase == MC_CLIENT_MANAGER_READY &&
              manager.a != NULL && manager.a == manager.candidate && manager.last_frame_pc >= 0 &&
              manager.fields.e == f->app->render_engine &&
              manager.fields.g == f->app->active_world && manager.fields.h == f->app->i &&
              manager.fields.k == f->app->settings && manager.fields.p == f->app->font_renderer &&
              f->app->r == NULL);
        puts(
            "WORKBENCH_WORLD_READY source-N-TH-frame=1 engine-camera-settings=1 no-field-repair=1");
        return 0;
    }
    fprintf(stderr, "Workbench source N->TH frame deadline\n");
    return 1;
}

static int wb_run(fixture *f) {
    mining_work *m = &workbench->mining;
    CHECK(mc_client_menu_loop_create(f->bootstrap, &f->loop) &&
          mc_client_menu_loop_inspect(f->loop, &f->state) &&
          repeat_statistics_controls(f, true) == 0 && save_title_focus(f) == 0 &&
          ticks(f, 2) == 0 && mining_choose(f, m, NULL, &m->epochs[0]) == 0 &&
          wb_world_ready(f) == 0);
    mc_entity *player = f->app->h;
    mc_container *aa = player->local_player->fields.player.d;
    mc_client_source_id *ue = NULL;
    CHECK(inventory_key(f, SDL_SCANCODE_E) == 0 &&
          wb_wait(f, MC_CLIENT_SCREEN_INVENTORY, &ue) == 0 && wb_craft_ue(f, ue) == 0 &&
          inventory_key(f, SDL_SCANCODE_E) == 0 && wb_closed(f) == 0 &&
          inventory_key(f, SDL_SCANCODE_2) == 0);
    const uint64_t deadline = SDL_GetTicks() + 15000;
    do {
        CHECK(inventory_frame(f) == 0);
        if (player->local_player->fields.player.c->selected == 1 && wb_hit(f, 3, 2))
            break;
    } while (SDL_GetTicks() < deadline);
    CHECK(player == f->app->h && player->local_player->fields.player.c->selected == 1 &&
          inventory_field(f, 1, 58, 1) == 0 && wb_world_right(f, false) == 0 &&
          mining_capture(f, m, "workbench-placed.png") == 0);
    const uint64_t aim_deadline = SDL_GetTicks() + 15000;
    while (!wb_hit(f, 58, 1) && SDL_GetTicks() < aim_deadline)
        CHECK(inventory_frame(f) == 0);
    mc_client_source_id *oo = NULL;
    CHECK(wb_hit(f, 58, 1) && wb_world_right(f, true) == 0 &&
          wb_wait(f, MC_CLIENT_SCREEN_WORKBENCH, &oo) == 0 && oo != ue && player == f->app->h &&
          player->local_player->fields.player.d == aa && oo->id.j != aa &&
          wb_craft_oo(f, oo) == 0 && wb_close(f, oo) == 0 && inventory_key(f, SDL_SCANCODE_1) == 0);
    size_t held_frames = 0;
    const uint64_t held_deadline = SDL_GetTicks() + 15000;
    while (held_frames < 3 && SDL_GetTicks() < held_deadline) {
        CHECK(inventory_frame(f) == 0);
        const mc_client_world_frame_state *world = f->state.world_frame;
        const mc_client_first_person_hand_state *hand = world == NULL ? NULL : world->hand;
        const mc_item_stack *selected = NULL;
        mc_client_entity_renderer_state renderer;
        CHECK(mc_inventory_player_current(player->local_player->fields.player.c, &selected, NULL) ==
              MC_INVENTORY_OK);
        if (hand != NULL && hand->returned && hand->held_returned && hand->overlays_returned &&
            hand->held_item != NULL && hand->held_item->returned &&
            hand->held_item->equipped >= .99f && selected != NULL &&
            mc_client_entity_renderer_inspect(world->owner, &renderer) &&
            renderer.ra_b == selected) {
            mc_item_stack_fields fields;
            CHECK(mc_item_stack_fields_get(selected, &fields) == MC_ITEM_STACK_OK);
            if (fields.id == 270) {
                /* Overlays have retired the completed draw's borrowed Stack.
                 * Its scalar receipt remains; identity comes from the live RA.b. */
                CHECK(world->owner == f->app->t && hand->owner == world->owner &&
                      renderer.ra_occurrence != NULL && renderer.pending_ra == renderer.fields.c &&
                      renderer.ra_app == f->app && hand->held_item->stack == NULL &&
                      hand->held_item->stack_captured && hand->held_item->source_pc == 1377 &&
                      fields.count == 1 && fields.damage == 0 && hand->held_item->flat_item &&
                      hand->held_item->item_color_returned &&
                      hand->held_item->item_rotate_returned &&
                      hand->held_item->item_icon_returned &&
                      hand->held_item->item_geometry_returned && hand->held_item->faces == 66 &&
                      hand->held_item->vertices == 264 && f->state.hud != NULL &&
                      f->state.hud->returned);
                ++held_frames;
            }
        }
    }
    CHECK(held_frames == 3 && mining_capture(f, m, "workbench-held-pickaxe.png") == 0 &&
          wb_stat(f, 5, 16, false) == 0 && wb_stat(f, 58, 1, false) == 0 &&
          wb_stat(f, 280, 4, false) == 0 && wb_stat(f, 270, 1, false) == 0 &&
          wb_stat(f, 58, 1, true) == 0 && wb_achievement(f, 2, 1) == 0 &&
          wb_achievement(f, 3, 1) == 0);
    workbench->saving = true;
    mining_extra_before_frame = wb_save_before;
    mining_extra_after_frame = wb_save_after;
    char path[8192];
    CHECK(mining_path(m, "workbench-save-title.png", path) == 0 &&
          save_title_run(f, NULL, path, false, 1, 0) == 0 && workbench->save_selected &&
          workbench->save_samples > 0 && reload_snapshot("workbench-saved") == 0);
    printf("WORKBENCH_SDK ui-frames=%zu world-ticks=%" PRIu64
           " held-pickaxe-frames=%zu save-samples=%zu real-place=1 real-activate=1 "
           "craft2x2=1 craft3x3=1 shift=1 cursor-grid-close=1 ordinary-save=1\n",
           workbench->ui_frames, workbench->ui_ticks, held_frames, workbench->save_samples);
    return 0;
}

int main(int argc, char **argv) {
    CHECK(argc == 3);
    fixture *f = calloc(1, sizeof(*f));
    workbench = calloc(1, sizeof(*workbench));
    if (f == NULL || workbench == NULL) {
        free(workbench);
        free(f);
        return 1;
    }
    mining_fixture = f;
    workbench->mining.directory = argv[2];
    const mc_client_application_options options = {.assets = argv[1],
                                                   .username = "Selection control",
                                                   .width = 854,
                                                   .height = 480,
                                                   .hidden = false};
    int result = 1;
    uint32_t frame_id = 0, canvas_id = 0;
    if (mc_client_bootstrap_open_standalone(&options, &f->bootstrap) == MC_CLIENT_BOOTSTRAP_OK) {
        f->app = mc_client_bootstrap_app(f->bootstrap);
        mc_client_frame_state host;
        if (f->app != NULL &&
            mc_client_source_frame_inspect(mc_client_bootstrap_frame(f->bootstrap), &host) &&
            host.frame_window != NULL && host.canvas_window != NULL) {
            frame_id = SDL_GetWindowID(host.frame_window);
            canvas_id = SDL_GetWindowID(host.canvas_window);
            result = wb_run(f);
        }
    }
    mining_extra_before_frame = NULL;
    mining_extra_after_frame = NULL;
    if (f->app != NULL && workbench->shift_down) {
        (void)gameplay_key(f, SDL_SCANCODE_LSHIFT, false);
        (void)transfer(f);
    }
    if (f->loop != NULL && !mc_client_menu_loop_terminal_retire(&f->loop)) {
        if (mc_client_menu_loop_inspect(f->loop, &f->state))
            fprintf(stderr, "Workbench cleanup refused: %s\n",
                    f->state.retirement_operation == NULL ? "unknown"
                                                          : f->state.retirement_operation);
        result = 1;
    }
    mc_client_bootstrap_status closed = MC_CLIENT_BOOTSTRAP_BUSY;
    const uint64_t deadline = SDL_GetTicks() + 35000;
    do {
        closed = mc_client_bootstrap_close(&f->bootstrap);
        if (closed == MC_CLIENT_BOOTSTRAP_BUSY)
            SDL_Delay(1);
    } while (closed == MC_CLIENT_BOOTSTRAP_BUSY && SDL_GetTicks() < deadline);
    if (closed != MC_CLIENT_BOOTSTRAP_OK || f->bootstrap != NULL || f->loop != NULL ||
        frame_id == 0 || canvas_id == 0 || SDL_GetWindowFromID(frame_id) != NULL ||
        SDL_GetWindowFromID(canvas_id) != NULL)
        result = 1;
    if (result == 0)
        printf("WORKBENCH_FINISH cleanup=1 windows-absent=1 checks=%u\n", checks);
    free(workbench);
    free(f);
    return result;
}
