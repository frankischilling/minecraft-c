/* Opt-in Windows frame benchmark. Actual normal frames run outside fixture
 * observers; setup never exports the provider/cache fields. The runner authors
 * the saved camera pose before loading and keeps ordinary simulation active. */
int mc_perf_unused_auto_main(int, char **);
#define MC_WORKBENCH_AUTO_CLOSE_FIXTURE_MAIN mc_perf_unused_auto_main
#include "../client/gameplay/test_workbench_auto_close.c"
#undef MC_WORKBENCH_AUTO_CLOSE_FIXTURE_MAIN
#include "client/display/sdk_update.h"
#include <windows.h>

enum { PERF_ROWS = 200000, PERF_FPS_ROWS = 64, PERF_FPS_UNITS = 96 };

typedef struct perf_row {
    int64_t begin, end, run137, run203, run387;
    uint64_t frame, ticks, world_ticks, draws, flags;
    uintptr_t foreground;
    DWORD foreground_pid;
    int32_t timer_ticks, tick_returns, screen;
    unsigned int valid;
    bool returned, source_focus;
} perf_row;

typedef struct perf_fps {
    int64_t at;
    uint64_t frame;
    uint16_t units[PERF_FPS_UNITS];
    size_t length;
    bool truncated;
} perf_fps;

static uint64_t perf_ft(FILETIME x) {
    return (uint64_t)x.dwLowDateTime | ((uint64_t)x.dwHighDateTime << 32);
}

static bool perf_qpc(int64_t *out) {
    LARGE_INTEGER sample;
    if (!QueryPerformanceCounter(&sample))
        return false;
    *out = sample.QuadPart;
    return true;
}

/* Passive scalar reads only, outside warmup/timing. The same saved Rotation
 * is authored on disk for both candidates; no source pose is repaired here. */
static int perf_pose(fixture *f, const char *stage) {
    CHECK(f->app->h != NULL);
    const mc_entity_base_fields *p = &f->app->h->base;
    uint64_t x, y, z;
    uint32_t yaw, pitch;
    memcpy(&x, &p->x, sizeof(x));
    memcpy(&y, &p->y, sizeof(y));
    memcpy(&z, &p->z, sizeof(z));
    memcpy(&yaw, &p->yaw, sizeof(yaw));
    memcpy(&pitch, &p->aT, sizeof(pitch));
    printf("PERF_POSE stage=%s x=%016" PRIx64 " y=%016" PRIx64 " z=%016" PRIx64 " yaw=%08" PRIx32
           " pitch=%08" PRIx32 "\n",
           stage, x, y, z, yaw, pitch);
    return 0;
}

/* No raw Chunk/provider/cache binding occurs on this setup path. Existing
 * selected-world helpers are used only for real SDK selection/focus/input.
 * reload_capture_epoch is scalar/identity inspection, not a binding export. */
static int perf_cache_receipt(fixture *f, const char *stage) {
    const mc_world_runtime *world = f->app->active_world;
    if (world == NULL) {
        printf("PERF_CACHE stage=%s present=0\n", stage);
        return 0;
    }
    mc_chunk_provider_cache_observation c;
    CHECK(mc_chunk_provider_cache_observe(world->fields.v, &c));
    printf("PERF_CACHE stage=%s present=1 owned=%d map-count=%zu insertion-count=%zu "
           "index-capacity=%zu index-bytes=%zu mutation-depth=%zu direct-bytes=%zu "
           "hits=%" PRIu64 " misses=%" PRIu64 " generic-gets=%" PRIu64 " raw-exports=%" PRIu64
           " rebuilds=%" PRIu64 "\n",
           stage, (int)c.owned, c.map_count, c.insertion_count, c.index_capacity, c.index_bytes,
           c.mutation_depth, c.direct_bytes, c.hits, c.misses, c.generic_gets, c.raw_exports,
           c.rebuilds);
    CHECK(c.owned && c.raw_exports == 0 && c.mutation_depth == 0);
    return 0;
}

static int perf_resume(fixture *f) {
    mc_client_source_da *recovered[4] = {0};
    size_t focus_pauses = 0, eligible;
    int64_t world_time;
    /* NULL chooses the existing passive cache eligibility observer; it does
     * not export raw fields or create/re-enable an index. Genuine Pause ID4
     * and ungrabbed-world Mouse acquisition remain the source operations. */
    CHECK(save_title_resume(f, f->app->active_world, NULL, recovered, &focus_pauses, &eligible,
                            &world_time) == 0);
    return 0;
}

static int perf_choose(fixture *f) {
    CHECK(perf_cache_receipt(f, "before-selection") == 0);
    mc_client_source_rq *rq = NULL;
    CHECK(f->app->r != NULL && f->app->r->kind == MC_CLIENT_SCREEN_MAIN_MENU &&
          open_selection(f, f->app->r, &rq) == 0 && release_mouse(f, rq) == 0);
    int32_t index = -1;
    for (size_t i = 0; i < mc_client_save_list_size(rq->fields.n); ++i)
        if (filename_is(mc_client_save_list_at(rq->fields.n, i), "alpha")) {
            CHECK(index == -1);
            index = (int32_t)i;
        }
    CHECK(index >= 0);
    const mc_client_world_list_fields *list = &rq->fields.o->fields;
    const int32_t y = list->h + 4 - mc_java_float_to_i32(list->m) + index * 36 + 8;
    CHECK(push_mouse(f, &rq->base, rq->base.fields.c / 2, y, true) == 0 && transfer(f) == 0);
    const uint64_t selection_deadline = SDL_GetTicks() + 5000;
    while (rq->fields.m != index && SDL_GetTicks() < selection_deadline)
        CHECK(frame(f) == 0);
    CHECK(rq->fields.m == index && release_mouse(f, rq) == 0 &&
          push_button(f, &rq->base, 1, true) == 0 && push_button(f, &rq->base, 1, false) == 0 &&
          transfer(f) == 0);
    /* The mining observer requires held Dirt. This scene starts with an
     * authored Pickaxe and uses the ordinary source frame/focus path instead. */
    const uint64_t deadline = SDL_GetTicks() + 180000;
    for (size_t frames = 0; frames < 180 && SDL_GetTicks() < deadline; ++frames) {
        CHECK(save_title_focus(f) == 0 && frame(f) == 0);
        if (f->app->active_world == NULL)
            continue;
        CHECK(perf_cache_receipt(f, "first-loaded") == 0);
        if (f->app->r != NULL || !f->app->N)
            CHECK(perf_resume(f) == 0);
        reload_epoch observed;
        CHECK(reload_capture_epoch(f, &observed) == 0);
        return 0;
    }
    return 1;
}

static int perf_world_ready(fixture *f) {
    const uint64_t deadline = SDL_GetTicks() + 15000;
    while (SDL_GetTicks() < deadline) {
        CHECK(inventory_frame(f) == 0);
        if (f->app->r != NULL || !f->app->N) {
            CHECK(perf_resume(f) == 0);
            continue;
        }
        const mc_client_render_global_entities_state *entities =
            f->state.world_frame == NULL ? NULL : f->state.world_frame->entities;
        if (entities == NULL || entities->delayed || !entities->th_returned)
            continue;
        mc_client_manager_state manager;
        CHECK(f->app->render_classes != NULL &&
              mc_client_render_manager_inspect(f->app->render_classes->th, &manager));
        const bool ready =
            manager.source_views_available && manager.phase == MC_CLIENT_MANAGER_READY &&
            manager.a != NULL && manager.a == manager.candidate && manager.last_frame_pc >= 0 &&
            manager.fields.e == f->app->render_engine && manager.fields.g == f->app->active_world &&
            manager.fields.h == f->app->i && manager.fields.k == f->app->settings &&
            manager.fields.p == f->app->font_renderer && f->app->r == NULL;
        if (!ready)
            fprintf(stderr,
                    "Automatic-close warm owner views%d phase%d engine%d world%d camera%d "
                    "settings%d font%d screen%d\n",
                    (int)manager.source_views_available, (int)manager.phase,
                    manager.fields.e == f->app->render_engine,
                    manager.fields.g == f->app->active_world, manager.fields.h == f->app->i,
                    manager.fields.k == f->app->settings, manager.fields.p == f->app->font_renderer,
                    f->app->r == NULL ? -1 : (int)f->app->r->kind);
        CHECK(ready);
        puts(
            "WORKBENCH_WORLD_READY source-N-TH-frame=1 engine-camera-settings=1 no-field-repair=1");
        return 0;
    }
    return 1;
}

static int perf_run(fixture *f) {
    CHECK(mc_client_menu_loop_create(f->bootstrap, &f->loop) &&
          mc_client_menu_loop_inspect(f->loop, &f->state) &&
          repeat_statistics_controls(f, true) == 0 && save_title_focus(f) == 0 &&
          ticks(f, 2) == 0 && perf_choose(f) == 0 && perf_world_ready(f) == 0);
    CHECK(perf_cache_receipt(f, "setup-returned") == 0);
    mc_game_settings_state settings;
    CHECK(mc_game_settings_read_state(f->app->settings, &settings));
    const mc_client_display_info *gpu = mc_client_display_info_get(f->app->display);
    CHECK(gpu != NULL);
    printf("PERF_GPU_VENDOR %s\nPERF_GPU_RENDERER %s\nPERF_GPU_VERSION %s\n", gpu->vendor,
           gpu->renderer, gpu->version);
    printf("PERF_CONTEXT major=%d minor=%d profile=%d swap-zero=%d width=%d height=%d "
           "view-distance=%d performance-mode=%d\n",
           gpu->gl_major, gpu->gl_minor, gpu->profile, (int)gpu->swap_interval_zero, f->app->d,
           f->app->e, settings.values.render_distance, settings.values.performance_mode);
    CHECK(settings.values.render_distance == 3 && f->app->r == NULL && f->app->N);
    CHECK(perf_pose(f, "pre-warmup") == 0);
    perf_row *rows = calloc(PERF_ROWS, sizeof(*rows));
    perf_fps *fps = calloc(PERF_FPS_ROWS, sizeof(*fps));
    LARGE_INTEGER frequency;
    CHECK(rows != NULL && fps != NULL && QueryPerformanceFrequency(&frequency) &&
          frequency.QuadPart > 0);
    /* Fault in observation storage before warmup, not on sampled frames. */
    for (size_t i = 0; i < PERF_ROWS * sizeof(*rows); i += 4096)
        ((volatile unsigned char *)rows)[i] = 0;
    const int64_t hz = frequency.QuadPart;
    SDL_Window *window = mc_client_display_window(f->app->display);
    size_t count = 0, fps_count = 0;
    uint64_t warm_frames = 0;
    bool healthy = true, capacity = false;
    int64_t start, end;
    CHECK(perf_qpc(&start));
    end = start;
    /* Natural warmup, no Source timing input, observer, screenshot or repair. */
    while (end - start < 2 * hz) {
        if (!mc_client_menu_loop_frame(f->loop)) {
            healthy = false;
            break;
        }
        ++warm_frames;
        CHECK(perf_qpc(&end));
    }
    CHECK(healthy && mc_client_menu_loop_inspect(f->loop, &f->state));
    CHECK(perf_cache_receipt(f, "warmup-returned") == 0);
    CHECK(perf_pose(f, "timed-entry") == 0);
    const uint64_t ticks_before = f->state.world_tick_returns;
    FILETIME pc0, pe0, pk0, pu0, pc1, pe1, pk1, pu1;
    CHECK(GetProcessTimes(GetCurrentProcess(), &pc0, &pe0, &pk0, &pu0));
    CHECK(perf_qpc(&start));
    end = start;
    uintptr_t last_fps = 0;
    /* TIMED SECTION: QPC, actual normal frame, shallow scalar observations.
     * No assertions, prints, captures, filesystem checks or fixture callbacks.
     * Inherent Source diagnostics/autosaves/sound/pacing remain ordinary work. */
    while (end - start < 10 * hz && count < PERF_ROWS) {
        perf_row *r = &rows[count];
        if (!perf_qpc(&r->begin)) {
            healthy = false;
            break;
        }
        r->returned = mc_client_menu_loop_frame(f->loop);
        if (!perf_qpc(&r->end)) {
            healthy = false;
            break;
        }
        end = r->end;
        mc_client_menu_loop_state state;
        if (!mc_client_menu_loop_inspect(f->loop, &state)) {
            healthy = false;
            break;
        }
        r->frame = state.frames;
        r->ticks = state.ticks;
        r->world_ticks = state.world_tick_returns;
        r->draws = state.world_draws;
        r->run137 = state.clocks.run_137;
        r->run203 = state.clocks.run_203;
        r->run387 = state.clocks.run_387;
        r->timer_ticks = state.clocks.timer_ticks;
        r->tick_returns = state.clocks.tick_returns;
        r->valid = (state.clocks.run_137_read ? 1u : 0u) | (state.clocks.run_203_read ? 2u : 0u) |
                   (state.clocks.run_387_read ? 4u : 0u);
        r->flags = (uint64_t)SDL_GetWindowFlags(window);
        r->foreground = (uintptr_t)GetForegroundWindow();
        (void)GetWindowThreadProcessId((HWND)r->foreground, &r->foreground_pid);
        r->source_focus = f->app->N;
        r->screen = f->app->r == NULL ? -1 : (int32_t)f->app->r->kind;
        const uintptr_t current_fps = (uintptr_t)f->app->K;
        if (current_fps != last_fps && fps_count < PERF_FPS_ROWS) {
            mc_player_string_view text;
            if (mc_player_string_read(f->app->K, &text)) {
                perf_fps *v = &fps[fps_count++];
                v->at = r->end;
                v->frame = r->frame;
                v->truncated = text.length > PERF_FPS_UNITS;
                v->length = text.length < PERF_FPS_UNITS ? text.length : PERF_FPS_UNITS;
                memcpy(v->units, text.units, v->length * sizeof(*v->units));
            }
            last_fps = current_fps;
        }
        ++count;
        if (!r->returned || state.failed || !state.running) {
            healthy = false;
            break;
        }
    }
    /* END TIMED SECTION. Final inspection/disk/provenance work is external. */
    capacity = count == PERF_ROWS && end - start < 10 * hz;
    CHECK(GetProcessTimes(GetCurrentProcess(), &pc1, &pe1, &pk1, &pu1));
    CHECK(perf_cache_receipt(f, "timed-returned") == 0);
    CHECK(perf_pose(f, "timed-returned") == 0);
    CHECK(mc_client_menu_loop_inspect(f->loop, &f->state));
    size_t focus_invalid = 0;
    for (size_t i = 0; i < count; ++i)
        if (!(rows[i].flags & SDL_WINDOW_INPUT_FOCUS) ||
            (rows[i].flags & (SDL_WINDOW_MINIMIZED | SDL_WINDOW_HIDDEN)) || !rows[i].source_focus ||
            rows[i].screen != -1)
            ++focus_invalid;
    printf("PERF_RESULT qpc-hz=%" PRId64 " start=%" PRId64 " end=%" PRId64 " warmup-frames=%" PRIu64
           " frames=%zu world-ticks=%" PRIu64
           " healthy=%d capacity=%d focus-invalid=%zu process-kernel-100ns=%" PRIu64
           " process-user-100ns=%" PRIu64 "\n",
           hz, start, end, warm_frames, count, f->state.world_tick_returns - ticks_before,
           (int)healthy, (int)capacity, focus_invalid, perf_ft(pk1) - perf_ft(pk0),
           perf_ft(pu1) - perf_ft(pu0));
    if (!healthy)
        fprintf(stderr, "PERF_FRONTIER %s.%s:%" PRId64 " %s\n",
                f->state.source_class == NULL ? "native" : f->state.source_class,
                f->state.source_method == NULL ? "frame" : f->state.source_method,
                f->state.source_pc, f->state.operation == NULL ? "unknown" : f->state.operation);
    for (size_t i = 0; i < count; ++i) {
        const perf_row *r = &rows[i];
        if (i == 0 || r->foreground != rows[i - 1].foreground)
            printf("PERF_FOREGROUND frame=%" PRIu64 " hwnd=%" PRIuPTR " pid=%lu own-pid=%lu\n",
                   r->frame, r->foreground, (unsigned long)r->foreground_pid,
                   (unsigned long)GetCurrentProcessId());
        printf("PERF_ROW %" PRIu64 ",%" PRId64 ",%" PRId64 ",%" PRId64 ",%" PRId64 ",%" PRId64
               ",%d,%d,%u,%" PRIu64 ",%d,%d,%d,%" PRIu64 ",%" PRIu64 ",%" PRIu64 "\n",
               r->frame, r->begin, r->end, r->run137, r->run203, r->run387, r->timer_ticks,
               r->tick_returns, r->valid, r->flags, (int)r->source_focus, r->screen,
               (int)r->returned, r->ticks, r->world_ticks, r->draws);
    }
    for (size_t i = 0; i < fps_count; ++i) {
        printf("PERF_SOURCE_FPS at=%" PRId64 " frame=%" PRIu64 " truncated=%d words=", fps[i].at,
               fps[i].frame, (int)fps[i].truncated);
        for (size_t j = 0; j < fps[i].length; ++j)
            printf("%04x", (unsigned)fps[i].units[j]);
        putchar('\n');
    }
    free(fps);
    free(rows);
    fflush(stdout);
    return healthy && !capacity && focus_invalid == 0 && count > 0 ? 0 : 1;
}

static int perf_quit(fixture *f, uint32_t frame_id) {
    /* Actual Frame.windowClosing -> Source J=false, not an Escape/save/pose.
     * Also reached after a failed warmup/measurement, without a repair. */
    SDL_Event event = {0};
    event.type = SDL_EVENT_WINDOW_CLOSE_REQUESTED;
    event.window.windowID = frame_id;
    CHECK(SDL_PushEvent(&event));
    mc_client_sdk_update_error update;
    for (unsigned int i = 0; i < 100 && atomic_load(&f->app->J); ++i) {
        CHECK(
            mc_client_sdk_display_process_messages(mc_client_display_update_owner(f->app->display),
                                                   &update) == MC_CLIENT_SDK_UPDATE_OK);
        SDL_Delay(2);
    }
    CHECK(!atomic_load(&f->app->J));
    CHECK(mc_client_menu_loop_inspect(f->loop, &f->state));
    if (!f->state.failed)
        CHECK(mc_client_menu_loop_frame(f->loop));
    return 0;
}

int main(int argc, char **argv) {
    CHECK(argc == 3);
    fixture *f = calloc(1, sizeof(*f));
    workbench = calloc(1, sizeof(*workbench));
    automatic = calloc(1, sizeof(*automatic));
    CHECK(f != NULL && workbench != NULL && automatic != NULL);
    automatic->mode = "performance";
    mining_fixture = f;
    workbench->mining.directory = argv[2];
    const mc_client_application_options options = {.assets = argv[1],
                                                   .username = "Selection control",
                                                   .width = 854,
                                                   .height = 480,
                                                   .hidden = false};
    CHECK(mc_client_bootstrap_open_standalone(&options, &f->bootstrap) == MC_CLIENT_BOOTSTRAP_OK);
    f->app = mc_client_bootstrap_app(f->bootstrap);
    mc_client_frame_state host;
    CHECK(f->app != NULL &&
          mc_client_source_frame_inspect(mc_client_bootstrap_frame(f->bootstrap), &host) &&
          host.frame_window != NULL && host.canvas_window != NULL);
    const uint32_t frame_id = SDL_GetWindowID(host.frame_window);
    const uint32_t canvas_id = SDL_GetWindowID(host.canvas_window);
    int result = perf_run(f);
    if (perf_quit(f, frame_id) != 0)
        result = 1;
    mining_extra_before_frame = NULL;
    mining_extra_after_frame = NULL;
    if (!mc_client_menu_loop_terminal_retire(&f->loop))
        result = 1;
    mc_client_bootstrap_status closed = MC_CLIENT_BOOTSTRAP_BUSY;
    const uint64_t deadline = SDL_GetTicks() + 35000;
    do {
        closed = mc_client_bootstrap_close(&f->bootstrap);
        if (closed == MC_CLIENT_BOOTSTRAP_BUSY)
            SDL_Delay(1);
    } while (closed == MC_CLIENT_BOOTSTRAP_BUSY && SDL_GetTicks() < deadline);
    const bool cleaned = closed == MC_CLIENT_BOOTSTRAP_OK && f->bootstrap == NULL &&
                         f->loop == NULL && SDL_GetWindowFromID(frame_id) == NULL &&
                         SDL_GetWindowFromID(canvas_id) == NULL;
    printf("PERF_FINISH cleanup=%d\n", (int)cleaned);
    if (cleaned) {
        free(automatic);
        free(workbench);
        free(f);
    }
    return cleaned ? result : 1;
}
