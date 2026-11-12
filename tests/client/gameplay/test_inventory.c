/* Genuine saved Player, configured SDK key/mouse, UE/ID draws and source close. */
static int inventory_opened;
#define MC_WORLD_SELECTION_OPEN_INVENTORY_EXPECTED(initial) (1 + inventory_opened)
int mc_inventory_included_mining_main(int, char **);
#define MC_MINING_FIXTURE_MAIN mc_inventory_included_mining_main
#include "test_mining.c"
#undef MC_MINING_FIXTURE_MAIN
#include "client/gui/source_inventory_drawing.h"
#include "client/gui/source_inventory_screen_internal.h"
#include "client/render/living_draw_internal.h"
#include "client/world/entity_renderer/held_stack_parent_internal.h"
#include "entity/player/player_internal.h"
#include "inventory/internal.h"
#include "inventory/player/source_internal.h"

static bool inventory_helmet;
static mc_item_stack *inventory_observed_cursor;

static int inventory_frame(fixture *f) {
    CHECK(save_title_focus(f) == 0);
    if (frame(f) != 0) {
        const mc_client_inventory_draw_state *inventory = f->state.inventory;
        const mc_client_manager_draw_state *preview = inventory == NULL ? NULL : inventory->preview;
        const mc_client_living_draw_error *living =
            preview == NULL || preview->failure == NULL ? NULL : preview->failure->living;
        if (living != NULL)
            fprintf(stderr, "Inventory preview child %s.%s%s:%d: %s; nested %s.%s:%d\n",
                    living->site.class_name == NULL ? "native" : living->site.class_name,
                    living->site.method == NULL ? "none" : living->site.method,
                    living->site.descriptor == NULL ? "" : living->site.descriptor, living->site.pc,
                    living->operation,
                    living->child.class_name == NULL ? "native" : living->child.class_name,
                    living->child.method == NULL ? "none" : living->child.method, living->child.pc);
        return 1;
    }
    return 0;
}

static int inventory_key(fixture *f, SDL_Scancode code) {
    CHECK(gameplay_key(f, code, true) == 0 && gameplay_key(f, code, false) == 0 &&
          transfer(f) == 0);
    return 0;
}

static int inventory_field(fixture *f, int32_t index, int32_t id, int32_t count) {
    const mc_item_stack *stack = NULL;
    mc_item_stack_fields fields;
    CHECK(mc_inventory_player_source_get(f->app->h->local_player->fields.player.c, index, &stack,
                                         NULL) == MC_INVENTORY_OK);
    if (count == 0)
        CHECK(stack == NULL);
    else
        CHECK(stack != NULL && mc_item_stack_fields_get(stack, &fields) == MC_ITEM_STACK_OK &&
              fields.id == id && fields.count == count && fields.damage == 0);
    return 0;
}

static int inventory_receipt(fixture *f, mc_client_source_ue *screen, bool cursor_expected) {
    CHECK(f->app->r == &screen->container.base && !f->app->N && !atomic_load(&f->app->o) &&
          f->state.inventory != NULL && f->state.inventory->returned &&
          f->state.inventory->receiver == screen && f->state.inventory->background_returned &&
          f->state.inventory->preview_returned && f->state.inventory->pose_restored &&
          f->state.inventory->foreground_returned && f->state.inventory->mouse_stored &&
          f->state.inventory->slots_drawn == 45 &&
          f->state.inventory->cursor_drawn == cursor_expected &&
          screen->container.id.j == f->app->h->local_player->fields.player.d &&
          f->app->h->local_player->fields.player.e == screen->container.id.j);
    const mc_client_manager_draw_state *preview = f->state.inventory->preview;
    CHECK(preview != NULL && preview->returned && preview->living != NULL &&
          preview->living->primary_event_returned && preview->living->primary_render_returned &&
          preview->living->player.wrapper_returned && preview->living->player.layers_returned &&
          preview->living->player.name_returned &&
          preview->living->player.cleared_flag_stores == 6);
    if (inventory_helmet) {
        CHECK(f->state.hud != NULL && f->state.hud->returned && f->state.hud->pumpkin_entered &&
              f->state.hud->pumpkin_returned && f->state.hud->pumpkin_pc == 133 &&
              f->state.hud->pumpkin_vertices == 4 && f->state.hud->pumpkin_word_count == 48);
        const bool held = f->app->h->local_player->fields.player.c->main_slots[0] != NULL;
        CHECK(preview->held_call_count == (held ? 2u : 1u));
        for (size_t i = 0; i < preview->held_call_count; ++i)
            CHECK(preview->held_calls[i] != NULL && preview->held_calls[i]->state.source_returned &&
                  preview->held_calls[i]->state.block_inventory_returned);
        if (held)
            CHECK(preview->held_calls[0]->state.guard_discharged &&
                  preview->held_calls[0] != preview->held_calls[1]);
    }
    return 0;
}

static int inventory_wait(fixture *f, bool open, mc_client_source_ue **screen) {
    const uint64_t deadline = SDL_GetTicks() + 15000;
    while (SDL_GetTicks() < deadline) {
        CHECK(inventory_frame(f) == 0);
        if (open && f->app->r != NULL && f->app->r->kind == MC_CLIENT_SCREEN_INVENTORY) {
            *screen = (mc_client_source_ue *)f->app->r;
            CHECK(inventory_receipt(f, *screen, false) == 0);
            ++inventory_opened;
            return 0;
        }
        if (!open && f->app->r == NULL && f->app->N && !atomic_load(&f->app->o))
            return 0;
    }
    fprintf(stderr, "Inventory SDK open/close deadline\n");
    return 1;
}

static int inventory_click(fixture *f, mc_client_source_ue *o, int32_t slot, bool right,
                           bool carried) {
    mc_container_slot_state fields;
    CHECK(mc_container_slot_read(o->container.id.j, slot, &fields, NULL) == MC_INVENTORY_OK);
    const int32_t x =
        mc_java_i32_sub(o->container.base.fields.c, o->container.id.a) / 2 + fields.x + 8;
    const int32_t y =
        mc_java_i32_sub(o->container.base.fields.d, o->container.id.i) / 2 + fields.y + 8;
    SDL_Window *window = mc_client_display_window(f->app->display);
    int width, height;
    CHECK(SDL_GetWindowSize(window, &width, &height));
    const float native_x = (float)((int64_t)x * width / o->container.base.fields.c);
    const float native_y = (float)((int64_t)y * height / o->container.base.fields.d);
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
    const size_t calls = o->container.source_calls;
    const uint64_t deadline = SDL_GetTicks() + 15000;
    while (o->container.source_calls == calls && SDL_GetTicks() < deadline)
        CHECK(inventory_frame(f) == 0);
    CHECK(o->container.source_calls > calls && inventory_receipt(f, o, carried) == 0);
    if (!carried)
        CHECK(f->state.inventory->tooltip_drawn);
    return 0;
}

static int inventory_run(fixture *f, mining_work *work) {
    CHECK(mc_client_menu_loop_create(f->bootstrap, &f->loop) &&
          mc_client_menu_loop_inspect(f->loop, &f->state) &&
          repeat_statistics_controls(f, true) == 0 && save_title_focus(f) == 0 &&
          ticks(f, 2) == 0 && mining_choose(f, work, NULL, &work->epochs[0]) == 0 &&
          mining_ready(f, work, false) == 0);
    mc_entity *player = f->app->h;
    mc_container *container = player->local_player->fields.player.d;
    CHECK(inventory_field(f, 0, 0, 0) == 0 && inventory_field(f, 9, 3, 7) == 0 &&
          inventory_field(f, 10, 262, 10) == 0);
    mc_world_info_fields before, after;
    CHECK(mc_world_info_fields_get(f->app->active_world->fields.x, &before) == MC_WORLD_INFO_OK);
    mc_client_source_ue *first = NULL;
    CHECK(inventory_key(f, SDL_SCANCODE_E) == 0 && inventory_wait(f, true, &first) == 0);
    const uint64_t ticks_before = f->state.ticks;
    const uint64_t deadline = SDL_GetTicks() + 10000;
    size_t draws = 0;
    while ((f->state.ticks < ticks_before + 3 || draws < 3) && SDL_GetTicks() < deadline) {
        CHECK(inventory_frame(f) == 0 && inventory_receipt(f, first, false) == 0);
        ++draws;
    }
    CHECK(f->state.ticks >= ticks_before + 3 && draws >= 3 &&
          mc_world_info_fields_get(f->app->active_world->fields.x, &after) == MC_WORLD_INFO_OK &&
          mc_java_i64_sub(after.value.time, before.value.time) >= 3 && player == f->app->h &&
          container == first->container.id.j);
    const uint64_t inventory_world_ticks =
        (uint64_t)mc_java_i64_sub(after.value.time, before.value.time);
    CHECK(mining_capture(f, work, "inventory-open.png") == 0);
    CHECK(inventory_click(f, first, 9, false, true) == 0 && inventory_field(f, 9, 0, 0) == 0 &&
          inventory_click(f, first, 36, true, true) == 0 && inventory_field(f, 0, 3, 1) == 0 &&
          inventory_click(f, first, 37, false, false) == 0 && inventory_field(f, 1, 3, 6) == 0 &&
          inventory_click(f, first, 10, true, true) == 0 && inventory_field(f, 10, 262, 5) == 0 &&
          inventory_click(f, first, 10, false, false) == 0 && inventory_field(f, 10, 262, 10) == 0);
    CHECK(mining_capture(f, work, "inventory-moved.png") == 0 &&
          inventory_key(f, SDL_SCANCODE_E) == 0 && inventory_wait(f, false, NULL) == 0 &&
          player == f->app->h && container == player->local_player->fields.player.d &&
          container == player->local_player->fields.player.e);
    for (size_t i = 0; i < 3; ++i) {
        CHECK(inventory_frame(f) == 0 && f->state.world_frame != NULL &&
              f->state.world_frame->returned && f->state.hud != NULL && f->state.hud->returned &&
              f->state.world_frame->hand != NULL && f->state.world_frame->hand->returned &&
              f->state.world_frame->hand->held_item != NULL &&
              f->state.world_frame->hand->held_item->returned &&
              f->state.world_frame->hand->held_item->block_item &&
              f->state.world_frame->hand->held_item->block_inventory_returned &&
              inventory_field(f, 0, 3, 1) == 0);
    }
    CHECK(mining_capture(f, work, "inventory-held-dirt.png") == 0);
    mc_client_source_ue *second = NULL;
    CHECK(inventory_key(f, SDL_SCANCODE_E) == 0 && inventory_wait(f, true, &second) == 0 &&
          second != first && second->container.id.j == container &&
          inventory_click(f, second, 37, false, true) == 0 && inventory_field(f, 1, 0, 0) == 0);
    const mc_item_stack *carried = NULL;
    CHECK(mc_inventory_player_source_cursor_get(player->local_player->fields.player.c, &carried,
                                                NULL) == MC_INVENTORY_OK &&
          carried != NULL && mc_item_stack_retain((mc_item_stack *)carried) == MC_ITEM_STACK_OK);
    inventory_observed_cursor = (mc_item_stack *)carried;
    CHECK(inventory_key(f, SDL_SCANCODE_ESCAPE) == 0 && inventory_wait(f, false, NULL) == 0);
    size_t dropped = 0;
    const mc_world_reference_list *entities = f->app->active_world->fields.b;
    for (size_t i = 0; i < entities->count; ++i) {
        mc_entity *entity = entities->items[i];
        if (entity != NULL && mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_ITEM) &&
            entity->stack == carried && !entity->base.be) {
            mc_item_stack_fields fields;
            CHECK(mc_item_stack_fields_get(carried, &fields) == MC_ITEM_STACK_OK &&
                  fields.id == 3 && fields.count == 6 && fields.damage == 0);
            ++dropped;
        }
    }
    mc_item_stack_release(inventory_observed_cursor);
    inventory_observed_cursor = NULL;
    CHECK(dropped == 1 && inventory_field(f, 0, 3, 1) == 0 && inventory_field(f, 1, 0, 0) == 0 &&
          inventory_field(f, 10, 262, 10) == 0 &&
          mining_semantic(player, work, "inventory-player.nbt") == 0);
    char path[8192];
    CHECK(mining_path(work, "inventory-save-title.png", path) == 0 &&
          save_title_run(f, NULL, path, false, 1, 0) == 0 &&
          reload_snapshot("inventory-saved") == 0);
    printf("INVENTORY_SDK draws=%zu world-ticks=%" PRIu64
           " same-player=1 same-container=1 slots=45 left-right=1 tooltip=1 held-dirt=1 "
           "cursor-close-drop=1 dirt-count=6 ordinary-save=1 helmet=%d\n",
           draws, inventory_world_ticks, (int)inventory_helmet);
    return 0;
}

#ifndef MC_INVENTORY_FIXTURE_MAIN
#define MC_INVENTORY_FIXTURE_MAIN main
#endif
int MC_INVENTORY_FIXTURE_MAIN(int argc, char **argv) {
    CHECK(argc == 3 || (argc == 4 && strcmp(argv[3], "--helmet") == 0));
    inventory_helmet = argc == 4;
    fixture *f = calloc(1, sizeof(*f));
    mining_work *work = calloc(1, sizeof(*work));
    CHECK(f != NULL && work != NULL);
    mining_fixture = f;
    work->directory = argv[2];
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
            result = inventory_run(f, work);
        }
    }
    mc_item_stack_release(inventory_observed_cursor);
    inventory_observed_cursor = NULL;
    if (f->loop != NULL && !mc_client_menu_loop_terminal_retire(&f->loop)) {
        if (mc_client_menu_loop_inspect(f->loop, &f->state))
            fprintf(
                stderr, "Inventory cleanup refused: %s; world=%s bootstrap=%d\n",
                f->state.retirement_operation == NULL ? "unknown" : f->state.retirement_operation,
                f->state.retirement_world == NULL || f->state.retirement_world->operation == NULL
                    ? "none"
                    : f->state.retirement_world->operation,
                (int)f->state.retirement_bootstrap);
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
        printf("INVENTORY_FINISH cleanup=1 windows-absent=1 checks=%u\n", checks);
    free(work);
    free(f);
    return result;
}
