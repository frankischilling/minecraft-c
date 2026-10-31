/* Included only in the canonical Display TU, after its real native kernels.
 * This owns the SDK fields and native children, not a second Display layout. */
#include "mc/core/java/java.h"

#include <SDL3/SDL_pixels.h>
#include <xmmintrin.h>

static bool stage_failure(mc_client_display_stage_error *e, mc_client_display_failure_kind kind,
                           mc_client_display_status status, const char *operation,
                           const char *method, const char *descriptor, int32_t pc) {
    e->kind = kind;
    e->operation = operation;
    e->site = (mc_client_display_site){"org/lwjgl/opengl/Display", method, descriptor, pc};
    if (e->display.status == MC_CLIENT_DISPLAY_OK) {
        e->display.status = status;
        e->display.operation = operation;
    }
    return false;
}

static bool stage_enter(mc_client_display *d, const char *method, const char *descriptor,
                         mc_client_display_stage_error *e) {
    if (d == NULL || !SDL_IsMainThread() || !d->sdk_profile)
        return stage_failure(e, MC_CLIENT_DISPLAY_NATIVE_POLICY, MC_CLIENT_DISPLAY_INVALID_ARGUMENT,
                             "staged Display real owner/main thread", method, descriptor, -1);
    if (d->stage_active || mc_client_display_sdk_external_busy(d))
        return stage_failure(e, MC_CLIENT_DISPLAY_NATIVE_POLICY, MC_CLIENT_DISPLAY_BUSY,
                             "staged Display operation active", method, descriptor, -1);
    if (d->stage_phase == MC_CLIENT_DISPLAY_FAILED || d->stage_phase == MC_CLIENT_DISPLAY_RETIRING)
        return stage_failure(e, MC_CLIENT_DISPLAY_NATIVE_POLICY, MC_CLIENT_DISPLAY_INVALID_ARGUMENT,
                             "staged Display live native phase", method, descriptor, -1);
    d->stage_active = true;
    return true;
}

static bool stage_leave(mc_client_display *d, bool ok, mc_client_display_stage_error *e,
                         mc_client_display_stage_error *out) {
    if (!ok) {
        d->stage_failure = *e;
        if (e->kind == MC_CLIENT_DISPLAY_NATIVE_ALLOCATION || e->kind == MC_CLIENT_DISPLAY_NATIVE_PLATFORM ||
            e->kind == MC_CLIENT_DISPLAY_NATIVE_POLICY)
            d->stage_phase = MC_CLIENT_DISPLAY_FAILED;
    }
    d->stage_active = false;
    if (out != NULL) *out = *e;
    return ok;
}

static bool literal_ascii(const char *ascii, mc_player_string **out, mc_client_display_stage_error *e,
                           const char *method, int32_t pc) {
    uint16_t units[32];
    const size_t length = strlen(ascii);
    if (length > sizeof(units) / sizeof(units[0]))
        return stage_failure(e, MC_CLIENT_DISPLAY_NATIVE_POLICY, MC_CLIENT_DISPLAY_LIMIT,
                             "Display native literal extent", method, "()V", pc);
    for (size_t i = 0; i < length; ++i) units[i] = (uint8_t)ascii[i];
    e->string = mc_player_string_create(units, length, out);
    if (e->string != MC_PLAYER_OK)
        return stage_failure(e, MC_CLIENT_DISPLAY_NATIVE_ALLOCATION, MC_CLIENT_DISPLAY_ALLOCATION_FAILED,
                             "Display actual immutable literal owner", method, "()V", pc);
    return true;
}

static bool sdk_class_initialize(mc_client_display *d, mc_client_display_stage_error *e) {
    if (d->class_initialized) return true;
    if (sdk_namespace != NULL && sdk_namespace != d)
        return stage_failure(e, MC_CLIENT_DISPLAY_NATIVE_POLICY, MC_CLIENT_DISPLAY_BUSY,
                             "one actual source SDK namespace", "<clinit>", "()V", -1);
    if (d->icons == NULL)
        return stage_failure(e, MC_CLIENT_DISPLAY_NATIVE_POLICY, MC_CLIENT_DISPLAY_INVALID_ARGUMENT,
                             "missing imported SDK icon prerequisite", "<clinit>", "()V", -1);
    sdk_namespace = d;
    if (!literal_ascii("Game", &d->title, e, "<clinit>", 26)) return false;
    d->source_last_store = 28;
    if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
        sdl_failure(&e->display, "actual linked SDK SDL video bootstrap");
        return stage_failure(e, MC_CLIENT_DISPLAY_NATIVE_PLATFORM, e->display.status,
                             e->display.operation, "<clinit>", "()V", 41);
    }
    d->video = true;
    const char *driver = SDL_GetCurrentVideoDriver();
    if (driver == NULL || (strcmp(driver, "windows") != 0 && strcmp(driver, "x11") != 0))
        return stage_failure(e, MC_CLIENT_DISPLAY_NATIVE_POLICY, MC_CLIENT_DISPLAY_UNSUPPORTED_GL,
                             "actual SDK Windows/X11 platform implementation", "<clinit>", "()V", 44);
    mc_client_sdk_desktop_mode mode;
    if (mc_client_sdk_platform_initial_mode(&mode, &e->platform) != MC_CLIENT_SDK_PLATFORM_OK)
        return stage_failure(e, MC_CLIENT_DISPLAY_NATIVE_PLATFORM, MC_CLIENT_DISPLAY_SDL_ERROR,
                             "actual SDK desktop-mode discovery", "<clinit>", "()V", 53);
    if (!mc_client_display_mode_platform(mode.width, mode.height, mode.bits_per_pixel, mode.frequency,
                                         &d->initial_mode, e)) {
        e->child = e->site;
        e->site = (mc_client_display_site){"org/lwjgl/opengl/Display", "<clinit>", "()V", 53};
        return false;
    }
    d->source_last_store = 59;
    if (!mc_client_display_mode_retain(d->initial_mode))
        return stage_failure(e, MC_CLIENT_DISPLAY_NATIVE_POLICY, MC_CLIENT_DISPLAY_LIMIT,
                             "initial/current SAME Mode hold", "<clinit>", "()V", 62);
    d->current_mode = d->initial_mode;
    d->source_last_store = 62;
    if (mc_client_sdk_update_allocate(d, NULL, NULL, 0, 0, &d->updates, &e->update) != MC_CLIENT_SDK_UPDATE_OK)
        return stage_failure(e, MC_CLIENT_DISPLAY_NATIVE_ALLOCATION, MC_CLIENT_DISPLAY_ALLOCATION_FAILED,
                             "Display real update owner", "<clinit>", "()V", -1);
    d->class_initialized = true;
    return true;
}

/* Native window text bridge: no substitute/default text, embedded NUL truncation
 * or surrogate replacement is credited as source setTitle29. The source field
 * has already received the SAME String before this reached native admission. */
static bool native_title(mc_client_display *d, char **out, mc_client_display_stage_error *e,
                           const char *method, const char *descriptor, int32_t pc) {
    mc_player_string_view text;
    if (!mc_player_string_read(d->title, &text) || text.length > (SIZE_MAX - 1) / 3)
        return stage_failure(e, MC_CLIENT_DISPLAY_NATIVE_POLICY, MC_CLIENT_DISPLAY_LIMIT,
                             "actual Display title native view/extent", method, descriptor, pc);
    char *utf8 = malloc(text.length * 3 + 1);
    if (utf8 == NULL)
        return stage_failure(e, MC_CLIENT_DISPLAY_NATIVE_ALLOCATION, MC_CLIENT_DISPLAY_ALLOCATION_FAILED,
                             "actual Display title UTF8 bridge", method, descriptor, pc);
    size_t count = 0;
    for (size_t i = 0; i < text.length; ++i) {
        uint32_t scalar = text.units[i];
        if (scalar == 0 || (scalar >= 0xdc00u && scalar <= 0xdfffu)) goto unsupported;
        if (scalar >= 0xd800u && scalar <= 0xdbffu) {
            if (i + 1 == text.length || text.units[i + 1] < 0xdc00u || text.units[i + 1] > 0xdfffu)
                goto unsupported;
            scalar = 0x10000u + ((scalar - 0xd800u) << 10) + text.units[++i] - 0xdc00u;
        }
        if (scalar < 0x80u) utf8[count++] = (char)scalar;
        else if (scalar < 0x800u) {
            utf8[count++] = (char)(0xc0u | (scalar >> 6));
            utf8[count++] = (char)(0x80u | (scalar & 63u));
        } else if (scalar < 0x10000u) {
            utf8[count++] = (char)(0xe0u | (scalar >> 12));
            utf8[count++] = (char)(0x80u | ((scalar >> 6) & 63u));
            utf8[count++] = (char)(0x80u | (scalar & 63u));
        } else {
            utf8[count++] = (char)(0xf0u | (scalar >> 18));
            utf8[count++] = (char)(0x80u | ((scalar >> 12) & 63u));
            utf8[count++] = (char)(0x80u | ((scalar >> 6) & 63u));
            utf8[count++] = (char)(0x80u | (scalar & 63u));
        }
    }
    utf8[count] = '\0';
    *out = utf8;
    return true;
unsupported:
    free(utf8);
    return stage_failure(e, MC_CLIENT_DISPLAY_NATIVE_POLICY, MC_CLIENT_DISPLAY_INVALID_ARGUMENT,
                         "Display title NUL/unpaired-surrogate native profile", method, descriptor, pc);
}

bool mc_client_display_stage_allocate(mc_client_display **out, mc_client_display_stage_error *error) {
    mc_client_display_stage_error e = {0};
    bool ok = false;
    if (out == NULL || !SDL_IsMainThread())
        stage_failure(&e, MC_CLIENT_DISPLAY_NATIVE_POLICY, MC_CLIENT_DISPLAY_INVALID_ARGUMENT,
                       "staged Display RAW output/main thread", NULL, NULL, -1);
    else {
        mc_client_display *d = calloc(1, sizeof(*d));
        if (d == NULL)
            stage_failure(&e, MC_CLIENT_DISPLAY_NATIVE_ALLOCATION, MC_CLIENT_DISPLAY_ALLOCATION_FAILED,
                           "staged Display RAW allocation", NULL, NULL, -1);
        else {
            d->sdk_profile = true;
            d->source_last_store = -1;
            *out = d;
            ok = true;
        }
    }
    if (error != NULL) *error = e;
    return ok;
}

bool mc_client_display_stage_bind_icons(mc_client_display *d, mc_client_sdk_icons *icons,
                                       mc_client_display_stage_error *out) {
    mc_client_display_stage_error e = {0};
    if (!stage_enter(d, NULL, NULL, &e)) {
        if (out != NULL) *out = e;
        return false;
    }
    bool ok = false;
    if (d->class_initialized || d->stage_phase != MC_CLIENT_DISPLAY_RAW || d == sdk_namespace)
        stage_failure(&e, MC_CLIENT_DISPLAY_NATIVE_POLICY, MC_CLIENT_DISPLAY_INVALID_ARGUMENT,
                       "bind imported icons before first SDK active use", NULL, NULL, -1);
    else if (mc_client_sdk_icons_retain(icons, &e.icons) != MC_CLIENT_SDK_ICONS_OK)
        stage_failure(&e, MC_CLIENT_DISPLAY_NATIVE_POLICY, MC_CLIENT_DISPLAY_INVALID_ARGUMENT,
                       "bind actual READY imported icon occurrence", NULL, NULL, -1);
    else {
        /* READY pixels have no pending stream. Their final release cannot
         * invoke a provider; this is a normal counted owner replacement. */
        mc_client_sdk_icons_error cleanup = {0};
        (void)mc_client_sdk_icons_release(&d->icons, &cleanup);
        d->icons = icons;
        ok = true;
    }
    return stage_leave(d, ok, &e, out);
}

bool mc_client_display_stage_set_mode(mc_client_display *d, mc_client_display_mode *mode,
                                     mc_client_display_stage_error *out) {
    mc_client_display_stage_error e = {0};
    if (!stage_enter(d, "setDisplayMode", "(Lorg/lwjgl/opengl/DisplayMode;)V", &e)) {
        if (out != NULL) *out = e;
        return false;
    }
    bool ok = false;
    if (mode != NULL && !mode->constructed)
        stage_failure(&e, MC_CLIENT_DISPLAY_NATIVE_POLICY, MC_CLIENT_DISPLAY_INVALID_ARGUMENT,
                       "actual constructed Mode caller domain", "setDisplayMode", "(Lorg/lwjgl/opengl/DisplayMode;)V", -1);
    else if (!sdk_class_initialize(d, &e)) {}
    else if (mode == NULL)
        stage_failure(&e, MC_CLIENT_DISPLAY_SOURCE_NULL_POINTER, MC_CLIENT_DISPLAY_INVALID_ARGUMENT,
                       "actual null DisplayMode", "setDisplayMode", "(Lorg/lwjgl/opengl/DisplayMode;)V", 10);
    else if (!mc_client_display_mode_retain(mode))
        stage_failure(&e, MC_CLIENT_DISPLAY_NATIVE_POLICY, MC_CLIENT_DISPLAY_LIMIT,
                       "current SAME Mode hold", "setDisplayMode", "(Lorg/lwjgl/opengl/DisplayMode;)V", 25);
    else {
        mc_client_display_mode_release(&d->current_mode);
        d->current_mode = mode;
        d->source_last_store = 25;
        e.current_mode_stored = true;
        if (d->window_created)
            stage_failure(&e, MC_CLIENT_DISPLAY_NATIVE_POLICY, MC_CLIENT_DISPLAY_UNSUPPORTED_GL,
                           "actual reached created-window destroy/reconfiguration body", "setDisplayMode",
                           "(Lorg/lwjgl/opengl/DisplayMode;)V", 34);
        else ok = true;
    }
    return stage_leave(d, ok, &e, out);
}

bool mc_client_display_stage_set_title(mc_client_display *d, mc_player_string *title,
                                      mc_client_display_stage_error *out) {
    mc_client_display_stage_error e = {0};
    if (!stage_enter(d, "setTitle", "(Ljava/lang/String;)V", &e)) {
        if (out != NULL) *out = e;
        return false;
    }
    bool ok = false;
    if (!sdk_class_initialize(d, &e)) goto done;
    if (title == NULL) {
        if (d->empty_title == NULL && !literal_ascii("", &d->empty_title, &e, "setTitle", 10)) goto done;
        title = d->empty_title;
    }
    e.string = mc_player_string_retain(title);
    if (e.string != MC_PLAYER_OK) {
        stage_failure(&e, MC_CLIENT_DISPLAY_NATIVE_POLICY, MC_CLIENT_DISPLAY_LIMIT,
                       "Display SAME title occurrence", "setTitle", "(Ljava/lang/String;)V", 14);
        goto done;
    }
    mc_player_string_release(d->title);
    d->title = title;
    d->source_last_store = 14;
    e.title_stored = true;
    if (d->window_created) {
        char *utf8 = NULL;
        if (!native_title(d, &utf8, &e, "setTitle", "(Ljava/lang/String;)V", 29)) goto done;
        ok = SDL_SetWindowTitle(d->window, utf8);
        free(utf8);
        if (!ok) {
            sdl_failure(&e.display, "actual live native window title");
            stage_failure(&e, MC_CLIENT_DISPLAY_NATIVE_PLATFORM, e.display.status,
                           e.display.operation, "setTitle", "(Ljava/lang/String;)V", 29);
        }
    } else ok = true;
done:
    return stage_leave(d, ok, &e, out);
}

bool mc_client_display_create_staged(mc_client_display *d, bool hidden,
                                     mc_client_display_stage_error *out) {
    mc_client_display_stage_error e = {0};
    if (!stage_enter(d, "create", "()V", &e)) {
        if (out != NULL) *out = e;
        return false;
    }
    bool ok = false;
    if (!sdk_class_initialize(d, &e)) goto done;
    if (d->window_created) {
        stage_failure(&e, MC_CLIENT_DISPLAY_SOURCE_ILLEGAL_STATE, MC_CLIENT_DISPLAY_INVALID_ARGUMENT,
                       "actual second source Display.create", "create",
                       "(Lorg/lwjgl/opengl/PixelFormat;Lorg/lwjgl/opengl/Drawable;Lorg/lwjgl/opengl/ContextAttribs;)V", 12);
        goto done;
    }
    d->stage_phase = MC_CLIENT_DISPLAY_CREATING;
    const int32_t width = d->current_mode->width, height = d->current_mode->height;
    size_t requested;
    if (width <= 0 || height <= 0 || width > DISPLAY_WINDOW_AXIS || height > DISPLAY_WINDOW_AXIS ||
        !product((size_t)width, (size_t)height, &requested) || requested > DISPLAY_WINDOW_PIXELS) {
        stage_failure(&e, MC_CLIENT_DISPLAY_NATIVE_POLICY, MC_CLIENT_DISPLAY_LIMIT,
                       "reached native window extent policy", "createWindow", "()V", 71);
        goto done;
    }
    char *title = NULL;
    if (!native_title(d, &title, &e, "createWindow", "()V", 71)) goto done;
    const bool window = native_create_window(d, title, width, height, hidden, &e.display);
    free(title);
    if (!window) {
        stage_failure(&e, MC_CLIENT_DISPLAY_NATIVE_PLATFORM, e.display.status,
                       e.display.operation, "createWindow", "()V", 71);
        goto done;
    }
    d->window_created = true;
    d->source_last_store = 77;
    e.window_created = true;
    /* The actual createWindow stores source width86/height95 from the SAME
     * pending Mode. App.d/e and high-DPI pixel dimensions are untouched. */
    d->source_last_store = 86;
    d->source_last_store = 95;
    mc_client_sdk_update_create_dimensions(d->updates, width, height);
    /* Native creation already uses the actual same retained title. Source
     * createWindow invokes setTitle101 again; perform its real live setter. */
    title = NULL;
    if (!native_title(d, &title, &e, "createWindow", "()V", 101)) goto done;
    const bool titled = SDL_SetWindowTitle(d->window, title);
    free(title);
    if (!titled) {
        sdl_failure(&e.display, "actual createWindow title setter");
        stage_failure(&e, MC_CLIENT_DISPLAY_NATIVE_PLATFORM, e.display.status,
                       e.display.operation, "createWindow", "()V", 101);
        goto done;
    }
    if (mc_client_sdk_input_allocate(d, d->events, &d->inputs, &e.input) != MC_CLIENT_SDK_INPUT_OK ||
        mc_client_sdk_input_construct_default(d->inputs, &e.input) != MC_CLIENT_SDK_INPUT_OK) {
        e.has_input = true;
        stage_failure(&e, MC_CLIENT_DISPLAY_NATIVE_PLATFORM, MC_CLIENT_DISPLAY_SDL_ERROR,
                       "actual createWindow input construction", "createWindow", "()V", 104);
        goto done;
    }
    if (mc_client_sdk_update_attach(d->updates, d->inputs, d->events, &e.update) != MC_CLIENT_SDK_UPDATE_OK) {
        stage_failure(&e, MC_CLIENT_DISPLAY_NATIVE_POLICY, MC_CLIENT_DISPLAY_INVALID_ARGUMENT,
                       "actual once-only update/input attachment", "createWindow", "()V", 104);
        goto done;
    }
    if (mc_client_sdk_window_icons_allocate(d->window, d->icons, &d->window_icons, &e.icons) != MC_CLIENT_SDK_ICONS_OK ||
        mc_client_sdk_window_icons_install(d->window_icons, &e.icons) != MC_CLIENT_SDK_ICONS_OK) {
        stage_failure(&e, MC_CLIENT_DISPLAY_NATIVE_PLATFORM, MC_CLIENT_DISPLAY_SDL_ERROR,
                       "actual imported default native window icons", "createWindow", "()V", 139);
        goto done;
    }
    if (!native_create_context(d, &e.display)) {
        stage_failure(&e, MC_CLIENT_DISPLAY_NATIVE_PLATFORM, e.display.status,
                       e.display.operation, "create",
                       "(Lorg/lwjgl/opengl/PixelFormat;Lorg/lwjgl/opengl/Drawable;Lorg/lwjgl/opengl/ContextAttribs;)V", 102);
        goto done;
    }
    d->stage_phase = MC_CLIENT_DISPLAY_READY;
    ok = true;
done:
    return stage_leave(d, ok, &e, out);
}

bool mc_client_display_stage_inspect(const mc_client_display *d, mc_client_display_stage_state *out) {
    if (d == NULL || out == NULL || !d->sdk_profile) return false;
    *out = (mc_client_display_stage_state){d->stage_phase, d->initial_mode, d->current_mode, d->title,
                                          d->stage_active, d->class_initialized, d->window_created,
                                          d->source_last_store, &d->stage_failure};
    return true;
}

bool mc_client_display_sdk_created(const mc_client_display *d) {
    return d != NULL && d->sdk_profile && d->window_created;
}

bool mc_client_display_sdk_live(const mc_client_display *d) {
    return d != NULL && d->sdk_profile && d->class_initialized &&
           d->stage_phase != MC_CLIENT_DISPLAY_FAILED && d->stage_phase != MC_CLIENT_DISPLAY_RETIRING;
}

bool mc_client_display_sdk_operation_busy(const mc_client_display *d) {
    if (d == NULL) return false;
    mc_client_sdk_update_state update;
    mc_client_sdk_input_state input;
    return d->stage_active || d->native_retiring ||
           (mc_client_sdk_input_inspect(d->inputs, &input) && input.active) ||
           (mc_client_sdk_update_inspect(d->updates, &update) && update.active);
}

bool mc_client_display_sdk_external_busy(const mc_client_display *d) {
    if (d == NULL) return false;
    if (mc_client_display_sdk_operation_busy(d) || d->source.active_calls != 0 ||
        d->texture_resource_calls != 0) return true;
    for (const mc_client_display_texture_lease *lease = d->source.texture_leases;
         lease != NULL; lease = lease->next)
        if (lease->active) return true;
    return false;
}

bool mc_client_display_sdk_resized(mc_client_display *d, bool *out, mc_client_display_error *e) {
    int width, height;
    if (d == NULL || out == NULL || !SDL_GetWindowSize(d->window, &width, &height)) return sdl_failure(e, "actual native resize state");
    const bool queued = mc_client_events_take_resized(d->events);
    *out = queued || width != d->native_width || height != d->native_height;
    d->native_width = width;
    d->native_height = height;
    return success(e);
}

bool mc_client_display_sdk_extent(mc_client_display *d, bool width, int32_t *out, mc_client_display_error *e) {
    int w, h;
    if (d == NULL || out == NULL || !SDL_GetWindowSize(d->window, &w, &h)) return sdl_failure(e, "actual late native window extent");
    *out = width ? w : h;
    return success(e);
}

bool mc_client_display_sdk_source_extent(const mc_client_display *d, bool width, int32_t *out) {
    mc_client_sdk_update_state source;
    if (d == NULL || out == NULL || !d->sdk_profile ||
        !mc_client_sdk_update_inspect(d->updates, &source)) return false;
    *out = width ? source.width : source.height;
    return true;
}

bool mc_client_display_sdk_input_candidate(const mc_client_display *d, mc_client_sdk_input **out) {
    return d != NULL && d->sdk_profile && d->stage_active &&
           d->stage_phase == MC_CLIENT_DISPLAY_CREATING && d->window_created &&
           d->inputs == NULL && out == &d->inputs;
}

bool mc_client_display_sdk_update_candidate(const mc_client_display *d, mc_client_sdk_update **out) {
    return d != NULL && d->sdk_profile && d->stage_active && sdk_namespace == d &&
           !d->class_initialized && d->updates == NULL && out == &d->updates;
}

mc_client_event_port *mc_client_display_event_port(mc_client_display *d) { return d == NULL ? NULL : d->events; }
mc_client_sdk_input *mc_client_display_inputs(mc_client_display *d) { return d == NULL ? NULL : d->inputs; }
mc_client_sdk_update *mc_client_display_update_owner(mc_client_display *d) { return d == NULL ? NULL : d->updates; }

bool mc_client_display_stage_retire(mc_client_display **slot, mc_client_display_stage_error *out) {
    mc_client_display_stage_error e = {0};
    if (slot == NULL || !SDL_IsMainThread()) {
        stage_failure(&e, MC_CLIENT_DISPLAY_NATIVE_POLICY, MC_CLIENT_DISPLAY_INVALID_ARGUMENT,
                       "staged Display retire slot/main thread", NULL, NULL, -1);
        if (out != NULL) *out = e;
        return false;
    }
    mc_client_display *d = *slot;
    if (d == NULL) { if (out != NULL) *out = e; return true; }
    mc_client_sdk_input_state input;
    const bool input_live = mc_client_sdk_input_inspect(d->inputs, &input);
    mc_client_events_state events;
    const bool port_live = mc_client_events_inspect(d->events, &events);
    const size_t own_ticket = input_live && input.pending_native_event != NULL ? 1u : 0u;
    mc_client_sdk_update_state update;
    const bool update_active = mc_client_sdk_update_inspect(d->updates, &update) && update.active;
    if (!d->sdk_profile || d->stage_active || update_active || (input_live && input.active) ||
        d->source.context_holds != 0 || d->source.range_count != 0 || d->source.texture_lease_count != 0 ||
        d->source.active_calls != 0 || d->texture_resource_count != 0 ||
        (port_live && (events.active || events.port_tickets > own_ticket))) {
        stage_failure(&e, MC_CLIENT_DISPLAY_NATIVE_POLICY, MC_CLIENT_DISPLAY_BUSY,
                       "staged Display has active/held native descendants", NULL, NULL, -1);
        if (out != NULL) *out = e;
        return false;
    }
    d->stage_phase = MC_CLIENT_DISPLAY_RETIRING;
    /* Child retirement has its own active guard. Do not hold stage_active
     * while asking the input child to retire; RETIRING blocks all new source
     * entries and context acquisition throughout external cleanup callbacks. */
    if (mc_client_sdk_update_retire(&d->updates, &e.update) != MC_CLIENT_SDK_UPDATE_OK ||
        mc_client_sdk_input_retire(&d->inputs, &e.input) != MC_CLIENT_SDK_INPUT_OK) {
        e.has_input = e.input.status != MC_CLIENT_SDK_INPUT_OK;
        stage_failure(&e, MC_CLIENT_DISPLAY_NATIVE_PLATFORM, MC_CLIENT_DISPLAY_SDL_ERROR,
                       "staged Display device/update retirement", NULL, NULL, -1);
        goto retained;
    }
    d->stage_active = true;
    if (mc_client_sdk_window_icons_retire(&d->window_icons, &e.icons) != MC_CLIENT_SDK_ICONS_OK ||
        mc_client_sdk_icons_release(&d->icons, &e.icons) != MC_CLIENT_SDK_ICONS_OK) {
        stage_failure(&e, MC_CLIENT_DISPLAY_NATIVE_PLATFORM, MC_CLIENT_DISPLAY_SDL_ERROR,
                       "staged Display imported/native icon retirement", NULL, NULL, -1);
        goto retained;
    }
    mc_client_display_mode_release(&d->current_mode);
    mc_client_display_mode_release(&d->initial_mode);
    mc_player_string_release(d->title); d->title = NULL;
    mc_player_string_release(d->empty_title); d->empty_title = NULL;
    const bool owns_namespace = sdk_namespace == d;
    /* Diagnostic destruction retains its old consuming contract. Staged
     * destruction preserves a failed actual GL context/window, with the full
     * first and context-destroy causes, and never repeats uncertain deletions. */
    bool consumed = false;
    const bool ok = destroy_native(d, true, &consumed, &e.cleanup_display, &e.display);
    if (!consumed) {
        e.native_owner_retained = true;
        stage_failure(&e, e.display.status == MC_CLIENT_DISPLAY_BUSY ? MC_CLIENT_DISPLAY_NATIVE_POLICY
                                                                    : MC_CLIENT_DISPLAY_NATIVE_PLATFORM,
                       e.display.status, "staged Display native owner retained", NULL, NULL, -1);
        goto retained;
    }
    if (owns_namespace) sdk_namespace = NULL;
    *slot = NULL;
    if (!ok) stage_failure(&e, MC_CLIENT_DISPLAY_NATIVE_PLATFORM, e.display.status,
                           e.display.operation, NULL, NULL, -1);
    if (out != NULL) *out = e;
    return ok;
retained:
    d->stage_active = false;
    d->stage_failure = e;
    if (out != NULL) *out = e;
    return false;
}
