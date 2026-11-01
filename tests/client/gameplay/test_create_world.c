/* Fresh World creation through actual SDK records; no prepared save or terrain. */
int mc_create_world_included_mining_main(int, char **);
#define MC_MINING_FIXTURE_MAIN mc_create_world_included_mining_main
#include "test_mining.c"
#undef MC_MINING_FIXTURE_MAIN
#include "client/gui/source_create_world.h"
#include "client/gui/source_create_world_drawing.h"
#include "client/render/blocks/internal.h"

static const char fresh_name[] = " SDK Fresh:777 ";
static const char fresh_folder[] = "SDK Fresh_777";

typedef struct fresh_work {
    mining_work files;
    reload_epoch epochs[2];
    bool saving, sampled, selected;
    size_t samples, world_frames[2], menu_draws;
    int64_t save_first, save_last;
} fresh_work;

static fresh_work *fresh;

static bool fresh_text_is(mc_client_screen_text text, const char *ascii) {
    mc_player_string_view value;
    if (!mc_client_screen_text_read(text, &value) || value.length != strlen(ascii))
        return false;
    for (size_t i = 0; i < value.length; ++i)
        if (value.units[i] != (unsigned char)ascii[i])
            return false;
    return true;
}

static int fresh_repeat(fixture *f, bool expected) {
    mc_client_sdk_input_state input;
    CHECK(mc_client_sdk_input_inspect(mc_client_display_inputs(f->app->display), &input) &&
          input.repeat_enabled == expected);
    return 0;
}

static int fresh_initial_save(fixture *f, bool expected) {
    mc_client_screen_transition_state transitions;
    mc_client_world_load_state load;
    CHECK(mc_client_screen_transitions_inspect(f->app->screen_transitions, &transitions) &&
          transitions.world_load != NULL &&
          mc_client_world_load_inspect(transitions.world_load, &load) && load.source_returned &&
          load.initial_save_returned == expected);
    return 0;
}

static int fresh_frame(fixture *f) {
    CHECK(save_title_focus(f) == 0 && frame(f) == 0);
    return 0;
}

static int fresh_click(fixture *f, mc_client_source_da *screen, int32_t id) {
    const size_t before = f->state.mouse_events;
    CHECK(push_button(f, screen, id, true) == 0 && push_button(f, screen, id, false) == 0 &&
          transfer(f) == 0);
    const uint64_t deadline = SDL_GetTicks() + 60000;
    size_t frames = 0;
    do {
        CHECK(fresh_frame(f) == 0);
        ++frames;
    } while (f->state.mouse_events < before + 2 && frames < 128 && SDL_GetTicks() < deadline);
    CHECK(f->state.mouse_events >= before + 2);
    return 0;
}

static int fresh_key(fixture *f, SDL_Scancode key) {
    const size_t before = f->state.keyboard_events;
    CHECK(gameplay_key(f, key, true) == 0 && gameplay_key(f, key, false) == 0 && transfer(f) == 0);
    const uint64_t deadline = SDL_GetTicks() + 60000;
    size_t frames = 0;
    do {
        CHECK(fresh_frame(f) == 0);
        ++frames;
    } while (f->state.keyboard_events < before + 2 && frames < 128 && SDL_GetTicks() < deadline);
    CHECK(f->state.keyboard_events >= before + 2);
    return 0;
}

static int fresh_type(fixture *f, const char *text) {
    const size_t before = f->state.keyboard_events, length = strlen(text);
    CHECK(length > 0 && length <= 32);
    SDL_Event event = {0};
    event.type = SDL_EVENT_TEXT_INPUT;
    event.text.windowID = SDL_GetWindowID(mc_client_display_window(f->app->display));
    event.text.text = text;
    CHECK(SDL_PushEvent(&event) && transfer(f) == 0);
    const uint64_t deadline = SDL_GetTicks() + 60000;
    size_t frames = 0;
    do {
        CHECK(fresh_frame(f) == 0);
        ++frames;
    } while (f->state.keyboard_events < before + length && frames < 128 &&
             SDL_GetTicks() < deadline);
    CHECK(f->state.keyboard_events >= before + length);
    return 0;
}

static int fresh_field(mc_client_source_ro *field, const char *text, bool focused) {
    mc_client_text_field_state state;
    CHECK(mc_client_text_field_inspect(field, &state) && state.fields.a == focused &&
          state.fields.b && fresh_text_is(state.fields.h, text));
    return 0;
}

static int fresh_draw(fixture *f, fresh_work *work, const char *image) {
    CHECK(fresh_frame(f) == 0);
    const mc_client_create_world_draw_error *draw = f->state.create_world_draw;
    CHECK(draw != NULL && draw->source_returned && draw->labels_drawn == 5 &&
          draw->fields_drawn == 2 && draw->rectangles_drawn == 4 && draw->buttons_drawn == 2 &&
          mining_capture(f, &work->files, image) == 0);
    ++work->menu_draws;
    return 0;
}

static int fresh_open(fixture *f, mc_client_source_rq *parent, mc_client_source_fj **out) {
    CHECK(f->app->r == &parent->base && fresh_click(f, &parent->base, 3) == 0 &&
          f->app->r != NULL && f->app->r->kind == MC_CLIENT_SCREEN_CREATE_WORLD);
    mc_client_source_fj *screen = (mc_client_source_fj *)f->app->r;
    mc_client_create_world_state state;
    mc_client_screen_error error;
    CHECK(mc_client_screen_create_world_inspect(screen, &state, &error) == MC_CLIENT_SCREEN_OK &&
          state.fields.a == &parent->base && state.fields.i != NULL && state.fields.j != NULL &&
          !state.fields.m && state.initialization_count == 1 && fresh_repeat(f, true) == 0);
    mc_client_text_field_state name, seed;
    CHECK(mc_client_text_field_inspect(state.fields.i, &name) &&
          mc_client_text_field_inspect(state.fields.j, &seed) && name.fields.a && !seed.fields.a &&
          name.fields.i == 32 && fresh_field(state.fields.j, "", false) == 0);
    *out = screen;
    return 0;
}

static int fresh_wire(fresh_work *work, const mc_nbt_tag *root, const char *name) {
    mc_nbt_limits limits = mc_nbt_default_limits();
    limits.max_output_bytes = 1048576;
    mc_nbt_error error;
    size_t size = 0, written = 0;
    CHECK(mc_nbt_measure(root, &limits, &size, &error) == MC_NBT_OK && size <= 1048576);
    uint8_t *wire = malloc(size);
    char path[8192];
    CHECK(wire != NULL && mc_nbt_write(root, &limits, wire, size, &written, &error) == MC_NBT_OK &&
          written == size && mining_path(&work->files, name, path) == 0 &&
          SDL_SaveFile(path, wire, size));
    free(wire);
    return 0;
}

/* A value-only observation. Do not call the source WorldInfo writer here: it
 * samples LastPlayed and can rename a Player compound. This private snapshot
 * copies actual typed cells without changing any World, Info or clock owner. */
static int fresh_info(fresh_work *work, mc_world_runtime *world, const char *path) {
    mc_world_info_fields fields;
    CHECK(mc_world_info_fields_get(world->fields.x, &fields) == MC_WORLD_INFO_OK &&
          fields.name != NULL && fields.player == NULL && fields.value.seed == 777);
    const char *names[] = {"RandomSeed", "SpawnX",    "SpawnY",     "SpawnZ",         "Time",
                           "SizeOnDisk", "version",   "rainTime",   "thunderTime",    "raining",
                           "thundering", "LevelName", "LastPlayed", "PlayerDimension"};
    mc_nbt_tag tags[14] = {0}, root = {.type = MC_NBT_COMPOUND};
    const mc_nbt_tag *children[14];
    uint16_t keys[14][32] = {0};
    for (size_t i = 0; i < 14; ++i) {
        const size_t length = strlen(names[i]);
        CHECK(length <= 32);
        for (size_t j = 0; j < length; ++j)
            keys[i][j] = (unsigned char)names[i][j];
        tags[i].name = (mc_nbt_string){keys[i], length};
        tags[i].type = MC_NBT_INT;
        children[i] = &tags[i];
    }
    tags[0].type = tags[4].type = tags[5].type = tags[12].type = MC_NBT_LONG;
    tags[0].value.long_value = fields.value.seed;
    tags[1].value.int_value = fields.value.spawn_x;
    tags[2].value.int_value = fields.value.spawn_y;
    tags[3].value.int_value = fields.value.spawn_z;
    tags[4].value.long_value = fields.value.time;
    tags[5].value.long_value = fields.value.size_on_disk;
    tags[6].value.int_value = fields.value.version;
    tags[7].value.int_value = fields.value.rain_time;
    tags[8].value.int_value = fields.value.thunder_time;
    tags[9].type = tags[10].type = MC_NBT_BYTE;
    tags[9].value.byte_value = fields.value.raining ? 1 : 0;
    tags[10].value.byte_value = fields.value.thundering ? 1 : 0;
    tags[11].type = MC_NBT_STRING;
    tags[11].value.string = mc_world_info_name_text(fields.name);
    tags[12].value.long_value = fields.value.last_played;
    tags[13].value.int_value = fields.value.player_dimension;
    root.value.compound.items = children;
    root.value.compound.length = 14;
    return fresh_wire(work, &root, path);
}

static int fresh_before(fixture *f) {
    fresh->sampled = false;
    if (!fresh->saving || fresh->selected || f->app->r == NULL ||
        f->app->r->kind != MC_CLIENT_SCREEN_PAUSE || f->app->N || !atomic_load(&f->app->o))
        return 0;
    CHECK(f->app->active_world == fresh->epochs[0].world && f->app->h == fresh->epochs[0].player &&
          mining_semantic(f->app->h, &fresh->files, "player-entry.nbt") == 0 &&
          fresh_info(fresh, f->app->active_world, "info-entry.nbt") == 0);
    fresh->sampled = true;
    ++fresh->samples;
    return 0;
}

static void fresh_after(fixture *f, bool returned) {
    if (fresh->saving && fresh->sampled && returned && f->app->active_world == NULL &&
        f->app->h == NULL && f->app->r != NULL && f->app->r->kind == MC_CLIENT_SCREEN_MAIN_MENU)
        fresh->selected = true;
}

static int fresh_ready(fixture *f, fresh_work *work, size_t epoch) {
    CHECK(epoch < 2);
    const uint64_t deadline = SDL_GetTicks() + 300000;
    mc_client_source_da *recovered[4] = {0};
    size_t focus_pauses = 0, eligible = 0;
    int64_t time = 0;
    for (size_t i = 0; i < 512 && SDL_GetTicks() < deadline; ++i) {
        CHECK(fresh_frame(f) == 0 && f->app->active_world != NULL && f->app->h != NULL);
        if (f->app->r != NULL) {
            CHECK(save_title_resume(f, f->app->active_world, NULL, recovered, &focus_pauses,
                                    &eligible, &time) == 0);
            continue;
        }
        const mc_client_world_frame_state *world = f->state.world_frame;
        CHECK(f->app->N && !atomic_load(&f->app->o) && world != NULL && world->returned &&
              world->source_pc == 1013 && world->world == f->app->active_world &&
              world->terrain_returned && world->updates_returned && f->state.hud != NULL &&
              f->state.hud->returned && f->state.hud->source_pc == 2101 &&
              f->state.render_returned && f->state.clocks.frame_returned &&
              f->state.clocks.timer_ticks >= 0 && f->state.clocks.timer_ticks <= 10 &&
              f->state.clocks.tick_returns == f->state.clocks.timer_ticks);
        ++work->world_frames[epoch];
        if (work->world_frames[epoch] >= 3) {
            CHECK(mining_capture(f, &work->files, epoch == 0 ? "created.png" : "reloaded.png") ==
                  0);
            return 0;
        }
    }
    CHECK(work->world_frames[epoch] >= 3);
    return 0;
}

typedef struct fresh_copy {
    const char *destination;
    unsigned int depth;
    size_t *files, *bytes;
} fresh_copy;

static bool fresh_copy_tree(const char *, const char *, unsigned int, size_t *, size_t *);

static SDL_EnumerationResult SDLCALL fresh_copy_entry(void *context, const char *directory,
                                                      const char *name) {
    fresh_copy *walk = context;
    char source[8192], target[8192];
    if (strchr(name, '/') != NULL || strchr(name, '\\') != NULL)
        return SDL_ENUM_FAILURE;
    const int a = snprintf(source, sizeof(source), "%s%s", directory, name);
    const int b = snprintf(target, sizeof(target), "%s/%s", walk->destination, name);
    SDL_PathInfo info;
    if (a <= 0 || (size_t)a >= sizeof(source) || b <= 0 || (size_t)b >= sizeof(target) ||
        !SDL_GetPathInfo(source, &info))
        return SDL_ENUM_FAILURE;
    if (info.type == SDL_PATHTYPE_DIRECTORY)
        return walk->depth < 8 &&
                       fresh_copy_tree(source, target, walk->depth + 1, walk->files, walk->bytes)
                   ? SDL_ENUM_CONTINUE
                   : SDL_ENUM_FAILURE;
    if (info.type != SDL_PATHTYPE_FILE || info.size > 32u * 1048576u || *walk->files >= 4096 ||
        info.size > 256u * 1048576u - *walk->bytes || !SDL_CopyFile(source, target))
        return SDL_ENUM_FAILURE;
    ++*walk->files;
    *walk->bytes += (size_t)info.size;
    return SDL_ENUM_CONTINUE;
}

static bool fresh_copy_tree(const char *source, const char *target, unsigned int depth,
                            size_t *files, size_t *bytes) {
    fresh_copy walk = {target, depth, files, bytes};
    return SDL_CreateDirectory(target) && SDL_EnumerateDirectory(source, fresh_copy_entry, &walk);
}

static int fresh_save(fixture *f, fresh_work *work) {
    CHECK(f->app->r == NULL && fresh_key(f, SDL_SCANCODE_ESCAPE) == 0 && f->app->r != NULL &&
          f->app->r->kind == MC_CLIENT_SCREEN_PAUSE);
    const uint64_t deadline = SDL_GetTicks() + 60000;
    size_t frames = 0;
    while ((f->app->N || !atomic_load(&f->app->o)) && frames < 128 && SDL_GetTicks() < deadline) {
        CHECK(fresh_frame(f) == 0);
        ++frames;
    }
    CHECK(!f->app->N && atomic_load(&f->app->o));
    mc_client_source_da *pause = f->app->r;
    work->saving = true;
    CHECK(mc_platform_current_time_millis(&work->save_first) == MC_PLATFORM_MILLIS_OK &&
          fresh_click(f, pause, 1) == 0 &&
          mc_platform_current_time_millis(&work->save_last) == MC_PLATFORM_MILLIS_OK);
    work->saving = false;
    CHECK(work->selected && work->samples > 0 && f->app->active_world == NULL &&
          f->app->h == NULL && f->app->i == NULL && f->app->r != NULL &&
          f->app->r->kind == MC_CLIENT_SCREEN_MAIN_MENU);
    const char *saves = SDL_GetEnvironmentVariable(SDL_GetEnvironment(), "MC_FRESH_WORLD_SAVES");
    char target[8192];
    CHECK(saves != NULL && mining_path(&work->files, "snapshots/saved", target) == 0);
    size_t files = 0, bytes = 0;
    CHECK(fresh_copy_tree(saves, target, 0, &files, &bytes) && files > 0);
    printf("CREATE_WORLD_SAVE samples=%zu paused=1 owned-player=1 observed-info=1 "
           "selected-before-leave-frame=1 first=%" PRId64 " last=%" PRId64
           " files=%zu bytes=%zu ordinary-title=1\n",
           work->samples, work->save_first, work->save_last, files, bytes);
    CHECK(mining_capture(f, &work->files, "saved-title.png") == 0);
    return 0;
}

static int fresh_reselect(fixture *f, fresh_work *work) {
    mc_client_source_rq *rq = NULL;
    CHECK(open_selection(f, f->app->r, &rq) == 0 && release_mouse(f, rq) == 0 &&
          mc_client_save_list_size(rq->fields.n) == 1 &&
          filename_is(mc_client_save_list_at(rq->fields.n, 0), fresh_folder));
    const mc_client_world_list_fields *list = &rq->fields.o->fields;
    const int32_t y = list->h + 4 - mc_java_float_to_i32(list->m) + 8;
    CHECK(push_mouse(f, &rq->base, rq->base.fields.c / 2, y, true) == 0 && transfer(f) == 0);
    const uint64_t deadline = SDL_GetTicks() + 60000;
    size_t frames = 0;
    do {
        CHECK(fresh_frame(f) == 0);
        ++frames;
    } while (rq->fields.m != 0 && frames < 128 && SDL_GetTicks() < deadline);
    CHECK(rq->fields.m == 0 && release_mouse(f, rq) == 0 && fresh_click(f, &rq->base, 1) == 0 &&
          fresh_ready(f, work, 1) == 0 && reload_capture_epoch(f, &work->epochs[1]) == 0 &&
          fresh_initial_save(f, false) == 0 && work->epochs[1].world != work->epochs[0].world &&
          work->epochs[1].player != work->epochs[0].player &&
          work->epochs[1].handler != work->epochs[0].handler &&
          work->epochs[1].controller != work->epochs[0].controller &&
          work->epochs[1].call != work->epochs[0].call &&
          mining_semantic(f->app->h, &work->files, "player-reloaded.nbt") == 0 &&
          fresh_info(work, f->app->active_world, "info-reloaded.nbt") == 0);
    printf("CREATE_WORLD_RELOAD epochs=2 frames=%zu distinct-owners=1 ordinary-selection=1 "
           "initial-save=0\n",
           work->world_frames[1]);
    return 0;
}

static int fresh_run(fixture *f, fresh_work *work, bool menu_only) {
    mc_game_settings_state parsed;
    mc_game_settings_source_state options;
    CHECK(mc_game_settings_read_state(f->app->settings, &parsed) &&
          parsed.values.render_distance == 3 &&
          mc_game_settings_source_inspect(f->app->settings, &options) &&
          options.phase == MC_GAME_SETTINGS_SOURCE_READY && !options.active && options.N != NULL &&
          options.load_report != NULL && options.load_report->source_opened &&
          options.load_report->source_close_reached && !options.load_report->file_absent &&
          !options.load_report->outer_exception_caught && options.load_report->bad_options == 0 &&
          options.load_report->lines_read == 1 && options.load_report->bytes_read == 15);
    puts("CREATE_WORLD_OPTIONS view-distance=3 lines=1 bytes=15 source-loaded=1");
    CHECK(mc_client_menu_loop_create(f->bootstrap, &f->loop) &&
          mc_client_menu_loop_inspect(f->loop, &f->state) && save_title_focus(f) == 0 &&
          ticks(f, 2) == 0 && f->app->r != NULL && f->app->r->kind == MC_CLIENT_SCREEN_MAIN_MENU &&
          fresh_repeat(f, false) == 0);
    mc_client_source_da *title = f->app->r;
    mc_client_source_rq *rq = NULL;
    mc_client_source_fj *first = NULL, *screen = NULL;
    CHECK(open_selection(f, title, &rq) == 0 && release_mouse(f, rq) == 0 &&
          mc_client_save_list_size(rq->fields.n) == 0 && fresh_open(f, rq, &first) == 0 &&
          fresh_draw(f, work, "create-initial.png") == 0);
    mc_client_create_world_state state;
    mc_client_screen_error error;
    CHECK(mc_client_screen_create_world_inspect(first, &state, &error) == MC_CLIENT_SCREEN_OK &&
          fresh_key(f, SDL_SCANCODE_ESCAPE) == 0 &&
          f->app->r == mc_client_screen_create_world_base(first) && fresh_repeat(f, true) == 0 &&
          fresh_click(f, mc_client_screen_create_world_base(first), 1) == 0 &&
          f->app->r == &rq->base && fresh_repeat(f, false) == 0 &&
          fresh_open(f, rq, &screen) == 0 && screen != first &&
          fresh_draw(f, work, "create-reopened.png") == 0 &&
          mc_client_screen_create_world_inspect(screen, &state, &error) == MC_CLIENT_SCREEN_OK);
    mc_client_text_field_state name;
    mc_player_string_view initial;
    CHECK(mc_client_text_field_inspect(state.fields.i, &name) &&
          mc_client_screen_text_read(name.fields.h, &initial) && initial.length <= 32);
    const size_t erase = initial.length;
    for (size_t i = 0; i < erase; ++i)
        CHECK(fresh_key(f, SDL_SCANCODE_BACKSPACE) == 0);
    mc_client_source_ke *create = button(mc_client_screen_create_world_base(screen), 0);
    CHECK(fresh_field(state.fields.i, "", true) == 0 && create != NULL && !create->fields.g &&
          fresh_type(f, fresh_name) == 0 && fresh_field(state.fields.i, fresh_name, true) == 0 &&
          fresh_key(f, SDL_SCANCODE_TAB) == 0 &&
          fresh_field(state.fields.i, fresh_name, false) == 0 &&
          fresh_field(state.fields.j, "", true) == 0 && fresh_type(f, "777") == 0 &&
          fresh_field(state.fields.j, "777", true) == 0 &&
          mc_client_screen_create_world_inspect(screen, &state, &error) == MC_CLIENT_SCREEN_OK &&
          fresh_text_is(state.fields.l, fresh_folder) &&
          fresh_draw(f, work, "create-configured.png") == 0);
    printf("CREATE_WORLD_MENU draws=%zu labels=5 fields=2 rectangles=4 buttons=2 "
           "escape-noop=1 cancel-exact-parent=1 repeat-closed=1 reopened=1 "
           "typed-name=1 sanitized-folder=1 tab-sdk=1 typed-seed=777\n",
           work->menu_draws);
    CHECK(fflush(stdout) == 0);
    if (menu_only) {
        CHECK(fresh_click(f, mc_client_screen_create_world_base(screen), 1) == 0 &&
              f->app->r == &rq->base && fresh_repeat(f, false) == 0 &&
              f->app->active_world == NULL);
        return 0;
    }
    CHECK(fresh_key(f, SDL_SCANCODE_RETURN) == 0 && fresh_repeat(f, false) == 0 &&
          mc_client_screen_create_world_inspect(screen, &state, &error) == MC_CLIENT_SCREEN_OK &&
          state.fields.m && state.random_constructed && state.random_drawn && state.seed_parsed &&
          !state.seed_hashed && state.selected_seed == 777 && fresh_ready(f, work, 0) == 0 &&
          reload_capture_epoch(f, &work->epochs[0]) == 0 && fresh_initial_save(f, true) == 0 &&
          fresh_info(work, f->app->active_world, "info-created.nbt") == 0);
    printf("CREATE_WORLD_CREATED seed=777 enter-sdk=1 normal-loader=1 frames=%zu "
           "world-returned=1 hud-returned=1 initial-save=1\n",
           work->world_frames[0]);
    CHECK(fflush(stdout) == 0 && fresh_save(f, work) == 0 && fresh_reselect(f, work) == 0);
    return 0;
}

static void fresh_render_diagnostic(const char *layer, const mc_client_render_global_error *error) {
    if (error == NULL)
        return;
    fprintf(stderr,
            "[fresh-render-error] layer=%s status=%d source=%s.%s%s:%u enclosing=%u "
            "operation=%s native=%d returned=%d\n",
            layer, (int)error->status, creature_diagnostic_text(error->source_class),
            creature_diagnostic_text(error->source_method),
            creature_diagnostic_text(error->source_descriptor), error->source_pc,
            error->enclosing_pc, creature_diagnostic_text(error->operation),
            (int)error->native_policy, (int)error->source_returned);
    fprintf(stderr,
            "[fresh-render-sites] layer=%s ancestor=%s.%s%s:%" PRId32 " parent=%s.%s%s:%" PRId32
            " child=%s.%s%s:%" PRId32 "\n",
            layer, creature_diagnostic_text(error->ancestor.class_name),
            creature_diagnostic_text(error->ancestor.method),
            creature_diagnostic_text(error->ancestor.descriptor), error->ancestor.pc,
            creature_diagnostic_text(error->parent.class_name),
            creature_diagnostic_text(error->parent.method),
            creature_diagnostic_text(error->parent.descriptor), error->parent.pc,
            creature_diagnostic_text(error->child.class_name),
            creature_diagnostic_text(error->child.method),
            creature_diagnostic_text(error->child.descriptor), error->child.pc);
    fprintf(stderr,
            "[fresh-render-children] layer=%s graphics-status=%d graphics-operation=%s "
            "graphics-site=%s.%s%s:%" PRId32 " world-status=%d world-operation=%s "
            "registry-status=%d registry-message=%s\n",
            layer, (int)error->graphics.status, creature_diagnostic_text(error->graphics.operation),
            creature_diagnostic_text(error->graphics.site.class_name),
            creature_diagnostic_text(error->graphics.site.method),
            creature_diagnostic_text(error->graphics.site.descriptor), error->graphics.site.pc,
            (int)error->world.status, creature_diagnostic_text(error->world.operation),
            (int)error->registry.status, creature_diagnostic_text(error->registry.message));
}

static void fresh_cache_diagnostic(const char *layer, const mc_client_chunk_cache_error *error) {
    if (error == NULL)
        return;
    fprintf(stderr,
            "[fresh-cache-error] layer=%s status=%d source=ew.%s%s:%u child-pc=%u "
            "operation=%s storage-status=%d storage-operation=%s acquisition-status=%d\n",
            layer, (int)error->status, creature_diagnostic_text(error->method),
            creature_diagnostic_text(error->descriptor), error->bytecode, error->child_bytecode,
            creature_diagnostic_text(error->operation), (int)error->storage.status,
            creature_diagnostic_text(error->storage.operation), (int)error->acquisition.status);
    for (size_t i = 0;
         i < error->nested_count && i < sizeof(error->nested) / sizeof(error->nested[0]); ++i) {
        const mc_client_chunk_cache_frame *frame = &error->nested[i];
        fprintf(stderr, "[fresh-cache-frame] layer=%s index=%zu source=ew.%s%s:%u\n", layer, i,
                creature_diagnostic_text(frame->method),
                creature_diagnostic_text(frame->descriptor), frame->bytecode);
    }
}

static void fresh_compilation_diagnostics(const mc_client_world_frame_state *world) {
    if (world == NULL)
        return;
    fprintf(stderr,
            "[fresh-world-frame] pc=%u returned=%d update-calls=%zu updates-returned=%d "
            "terrain-returned=%d\n",
            world->source_pc, (int)world->returned, world->update_calls,
            (int)world->updates_returned, (int)world->terrain_returned);
    if (world->failure != NULL) {
        const mc_client_world_frame_error *error = world->failure;
        fprintf(stderr,
                "[fresh-world-frame-error] status=%d source=%s.%s%s:%" PRId32
                " child=%s.%s%s:%" PRId32 " operation=%s native=%d\n",
                (int)error->status, creature_diagnostic_text(error->site.class_name),
                creature_diagnostic_text(error->site.method),
                creature_diagnostic_text(error->site.descriptor), error->site.pc,
                creature_diagnostic_text(error->child.class_name),
                creature_diagnostic_text(error->child.method),
                creature_diagnostic_text(error->child.descriptor), error->child.pc,
                creature_diagnostic_text(error->operation), (int)error->native_policy);
        fresh_render_diagnostic("world-frame.update", error->update);
    }
    const mc_client_render_global_update_state *update = world->update;
    if (update == NULL)
        return;
    fprintf(stderr,
            "[fresh-update] pc=%u returned=%d index=%" PRId32 " initial=%" PRId32 " near=%" PRId32
            " far=%" PRId32 " compile-attempts=%zu completed=%zu remaining=%zu\n",
            update->source_pc, (int)update->returned, update->index, update->initial_count,
            update->near_count, update->far_count, update->compile_attempts,
            update->completed_compiles, update->remaining_count);
    fresh_render_diagnostic("update.failure", update->failure);
    fresh_render_diagnostic("update.cleanup", update->cleanup);
    const mc_client_chunk_compile_state *compile = update->compilation;
    if (compile == NULL)
        return;
    fprintf(
        stderr,
        "[fresh-compilation] pc=%u last-store=%u returned=%d active=%d block-id=%" PRId32
        " x=%" PRId32 " y=%" PRId32 " z=%" PRId32 " pass=%" PRId32
        " list-open=%d matrix-pushed=%d tessellator-started=%d later-pass=%d pass-rendered=%d\n",
        compile->source_pc, compile->last_store_pc, (int)compile->returned, (int)compile->active,
        compile->block_id, compile->x, compile->y, compile->z, compile->pass,
        (int)compile->list_open, (int)compile->matrix_pushed, (int)compile->tessellator_started,
        (int)compile->needs_later_pass, (int)compile->pass_rendered);
    fresh_render_diagnostic("compilation.failure", compile->failure);
    fresh_cache_diagnostic("compilation.cache-failure", compile->cache_failure);
    fresh_cache_diagnostic("compilation.cache-cleanup", compile->cache_cleanup);
    const mc_cv_context *cv = compile->render_blocks;
    if (cv == NULL)
        return;
    fprintf(stderr,
            "[fresh-render-blocks] pc=%u last-store=%u depth=%zu failed=%d returned=%d "
            "world-child=%d world-child-failed=%d\n",
            cv->source_pc, cv->last_store_pc, cv->depth, (int)cv->failed, (int)cv->returned,
            (int)cv->world_error.kind, (int)cv->world_error.failed);
    fresh_render_diagnostic("render-blocks.failure", &cv->failure);
    if (cv->cache_error.status != MC_CLIENT_CHUNK_CACHE_OK)
        fresh_cache_diagnostic("render-blocks.cache", &cv->cache_error);
    for (size_t i = 0; i < cv->depth && i < sizeof(cv->frames) / sizeof(cv->frames[0]); ++i) {
        const mc_cv_frame *frame = &cv->frames[i];
        fprintf(stderr, "[fresh-render-blocks-frame] index=%zu source=%s.%s%s:%u\n", i,
                creature_diagnostic_text(frame->class_name),
                creature_diagnostic_text(frame->method),
                creature_diagnostic_text(frame->descriptor), frame->pc);
    }
}

static void fresh_diagnostics(fixture *f) {
    if (f->loop != NULL)
        (void)mc_client_menu_loop_inspect(f->loop, &f->state);
    fprintf(stderr,
            "[fresh-world] %s.%s:%" PRId64 " operation=%s frames=%" PRIu64 " ticks=%" PRIu64
            " failed=%d\n",
            creature_diagnostic_text(f->state.source_class),
            creature_diagnostic_text(f->state.source_method), f->state.source_pc,
            creature_diagnostic_text(f->state.operation), f->state.frames, f->state.ticks,
            (int)f->state.failed);
    if (f->app == NULL)
        return;
    if (f->state.world_tick != NULL &&
        f->state.world_tick->entities.child == MC_WORLD_ENTITY_TICK_CHILD_FALLING_SAND) {
        const mc_falling_entity_tick_error *falling =
            &f->state.world_tick->entities.child_error.falling;
        fprintf(stderr,
                "[fresh-falling] status=%d pc=%u child=%d operation=%s cleanup=%d:%s "
                "drop=%s:%u\n",
                (int)falling->status, falling->bytecode, (int)falling->child,
                creature_diagnostic_text(falling->operation), (int)falling->cleanup_status,
                creature_diagnostic_text(falling->cleanup_operation),
                creature_diagnostic_text(falling->drop_method), falling->drop_bytecode);
        if (falling->movement != NULL)
            fprintf(stderr, "[fresh-falling-move] status=%d pc=%u operation=%s\n",
                    (int)falling->movement->status, falling->movement->bytecode,
                    creature_diagnostic_text(falling->movement->operation));
        if (falling->mutation != NULL)
            fprintf(stderr, "[fresh-falling-mutation] status=%d operation=%s hook=%s.%s:%u\n",
                    (int)falling->mutation->status,
                    creature_diagnostic_text(falling->mutation->operation),
                    creature_diagnostic_text(falling->mutation->hook_class),
                    creature_diagnostic_text(falling->mutation->hook_method),
                    falling->mutation->hook_bytecode);
        if (falling->placement != NULL)
            fprintf(stderr, "[fresh-falling-placement] status=%d operation=%s source=%s.%s:%u\n",
                    (int)falling->placement->status,
                    creature_diagnostic_text(falling->placement->operation),
                    creature_diagnostic_text(falling->placement->site.class_name),
                    creature_diagnostic_text(falling->placement->site.method),
                    falling->placement->site.pc);
    }
    fresh_compilation_diagnostics(f->state.world_frame);
    if (f->state.create_world_draw != NULL) {
        const mc_client_create_world_draw_error *draw = f->state.create_world_draw;
        fprintf(stderr,
                "[fresh-world-draw] status=%d %s.%s:%u operation=%s "
                "labels=%zu fields=%zu rectangles=%zu buttons=%zu\n",
                (int)draw->screen.status, creature_diagnostic_text(draw->screen.site.class_name),
                creature_diagnostic_text(draw->screen.site.method), draw->screen.site.pc,
                creature_diagnostic_text(draw->screen.operation), draw->labels_drawn,
                draw->fields_drawn, draw->rectangles_drawn, draw->buttons_drawn);
    }
    mc_client_screen_transition_state transitions;
    if (mc_client_screen_transitions_inspect(f->app->screen_transitions, &transitions)) {
        if (transitions.failure != NULL)
            fprintf(stderr, "[fresh-world-screen] status=%d %s.%s:%u operation=%s\n",
                    (int)transitions.failure->status,
                    creature_diagnostic_text(transitions.failure->site.class_name),
                    creature_diagnostic_text(transitions.failure->site.method),
                    transitions.failure->site.pc,
                    creature_diagnostic_text(transitions.failure->operation));
        mc_client_world_load_state load;
        if (transitions.world_load != NULL &&
            mc_client_world_load_inspect(transitions.world_load, &load) && load.failure != NULL) {
            const mc_client_world_load_error *error = load.failure;
            fprintf(stderr,
                    "[fresh-world-load] status=%d %s.%s:%d child=%s.%s:%d "
                    "operation=%s terrain=%zu returned=%d\n",
                    (int)error->status, creature_diagnostic_text(error->site.class_name),
                    creature_diagnostic_text(error->site.method), error->site.pc,
                    creature_diagnostic_text(error->child.class_name),
                    creature_diagnostic_text(error->child.method), error->child.pc,
                    creature_diagnostic_text(error->operation), load.terrain_samples,
                    (int)load.source_returned);
            if (error->source_store != NULL)
                fprintf(stderr, "[fresh-world-store] status=%d operation=%s cause=%s.%s:%d\n",
                        (int)error->source_store->status,
                        creature_diagnostic_text(error->source_store->operation),
                        creature_diagnostic_text(error->source_store->cause_site.class_name),
                        creature_diagnostic_text(error->source_store->cause_site.method),
                        error->source_store->cause_site.pc);
            if (error->world != NULL)
                fprintf(stderr, "[fresh-world-storage] status=%d operation=%s system=%" PRIu32 "\n",
                        (int)error->world->status,
                        creature_diagnostic_text(error->world->operation),
                        error->world->system_error);
            if (error->terrain != NULL) {
                const mc_client_world_prepare_state *prepare = error->terrain;
                fprintf(stderr,
                        "[fresh-world-prepare] status=%d pc=%u operation=%s "
                        "samples=%zu blocks=%zu lights=%zu\n",
                        (int)prepare->status, prepare->source_pc,
                        creature_diagnostic_text(prepare->operation), prepare->samples_started,
                        prepare->block_samples, prepare->light_calls);
                if (prepare->acquisition != NULL) {
                    const mc_world_runtime_acquisition_error *acquire = prepare->acquisition;
                    fprintf(stderr,
                            "[fresh-world-acquisition] status=%d cause=%s "
                            "population=%d current=%s.%s:%u origin=%s.%s:%u\n",
                            (int)acquire->status,
                            creature_diagnostic_text(acquire->cause.operation),
                            (int)acquire->population.reached,
                            creature_diagnostic_text(acquire->population.current.provider_class),
                            creature_diagnostic_text(acquire->population.current.method),
                            acquire->population.current.bytecode,
                            creature_diagnostic_text(acquire->population.origin.provider_class),
                            creature_diagnostic_text(acquire->population.origin.method),
                            acquire->population.origin.bytecode);
                }
            }
        }
    }
    mining_mob_diagnostics(f);
    (void)fflush(stderr);
}

int main(int argc, char **argv) {
    CHECK(argc == 3 || (argc == 4 && strcmp(argv[3], "--menu-only") == 0));
    const bool menu_only = argc == 4;
    fixture *f = calloc(1, sizeof(*f));
    fresh_work *work = calloc(1, sizeof(*work));
    CHECK(f != NULL && work != NULL);
    fresh = work;
    mining_fixture = f;
    work->files.directory = argv[2];
    mining_extra_before_frame = fresh_before;
    mining_extra_after_frame = fresh_after;
    const mc_client_application_options options = {.assets = argv[1],
                                                   .username = "Fresh world control",
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
            result = fresh_run(f, work, menu_only);
        }
    }
    if (result != 0)
        fresh_diagnostics(f);
    mining_extra_before_frame = NULL;
    mining_extra_after_frame = NULL;
    if (f->loop != NULL && !mc_client_menu_loop_terminal_retire(&f->loop))
        result = 1;
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
    printf("CREATE_WORLD_FINISH mode=%s cleanup=%d windows-absent=%d result=%d checks=%u\n",
           menu_only ? "menu-only" : "full", (int)(closed == MC_CLIENT_BOOTSTRAP_OK),
           (int)(frame_id != 0 && canvas_id != 0 && SDL_GetWindowFromID(frame_id) == NULL &&
                 SDL_GetWindowFromID(canvas_id) == NULL),
           result, checks);
    free(work);
    free(f);
    return result;
}
