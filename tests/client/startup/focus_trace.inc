/* Opt-in native observation only. The configured SDL build uses its generated
 * corrected Windows TU, not the pristine cached WIN_UpdateFocus body. Neither
 * these records nor a passing run establish entered-code or causal proof. */
#ifdef _WIN32
#include <stdarg.h>

enum { FOCUS_TRACE_CAPACITY = 4096, FOCUS_TRACE_PATH = 32768 };

typedef struct focus_trace_record {
    uint64_t sequence;
    int64_t counter;
    const char *phase;
    uintptr_t hwnd, wparam, lparam, result, focus, active, foreground, keyboard, hook_wparam;
    uintptr_t frame, canvas, parent, root, sdl_parent, sdl_frame, sdl_canvas;
    uint64_t style, frame_flags, canvas_flags;
    DWORD thread, last_error;
    UINT message;
    unsigned int depth;
    int outcome;
    bool counter_valid, keyboard_valid, host_sample, completed, sdk_active_valid, sdk_active;
} focus_trace_record;

static struct {
    HANDLE file;
    HHOOK before, after;
    WCHAR path[FOCUS_TRACE_PATH];
    DWORD thread, before_error, after_error, unhook_before_error, unhook_after_error;
    LARGE_INTEGER frequency;
    uint64_t sequence, overwritten, candidate_overwritten, reentrant, unmatched, written;
    size_t count, next, candidate_count;
    unsigned int depth, maximum_depth;
    bool enabled, finished, recording, before_installed, after_installed;
    bool unhook_before, unhook_after, candidate, io_failed, frequency_valid;
    bool original_completed, exit_fallback, finish_ok, dispatch_primed;
    focus_trace_record ring[FOCUS_TRACE_CAPACITY], first[FOCUS_TRACE_CAPACITY], candidate_record;
} focus_trace;

static bool focus_trace_finish(bool completed);

/* Called only on explicit opt-in failure, never from a hook. Preserve the
 * caller's actual Win32 error while stderr's own implementation runs. */
static void focus_trace_diagnostic(const char *operation, DWORD error, uintptr_t pending) {
    const DWORD saved = GetLastError();
    fprintf(stderr, "[WORLD_FOCUS_TRACE] operation=%s error=%lu pending-handle=%" PRIxPTR "\n",
            operation, (unsigned long)error, pending);
    SetLastError(saved);
}

static bool focus_trace_close_file(void) {
    const bool closed = CloseHandle(focus_trace.file) != 0;
    if (closed)
        focus_trace.file = NULL;
    else {
        const DWORD error = GetLastError();
        focus_trace_diagnostic("CloseHandle", error, (uintptr_t)focus_trace.file);
    }
    return closed;
}

static bool focus_trace_message(UINT message) {
    return message == WM_SETFOCUS || message == WM_KILLFOCUS || message == WM_ACTIVATE ||
           message == WM_NCACTIVATE || message == WM_ENTERIDLE || message == WM_ACTIVATEAPP ||
           message == WM_MOUSEACTIVATE || message == WM_CHILDACTIVATE || message == WM_SHOWWINDOW ||
           message == WM_CREATE || message == WM_NCCREATE || message == WM_DESTROY ||
           message == WM_NCDESTROY;
}

/* No callback work allocates, formats, pumps, writes files or changes focus.
 * Its public SDL dispatch is primed outside hooks before either is installed;
 * the pinned REAL SDL_GetKeyboardFocus is the keyboard owner's pointer read.
 * Raw pointers are identities only; no message payload pointer is retained. */
static void focus_trace_append(focus_trace_record record) {
    if (focus_trace.recording) {
        ++focus_trace.reentrant;
        return;
    }
    focus_trace.recording = true;
    record.sequence = ++focus_trace.sequence;
    LARGE_INTEGER counter;
    record.counter_valid = QueryPerformanceCounter(&counter) != 0;
    record.counter = record.counter_valid ? counter.QuadPart : 0;
    record.thread = GetCurrentThreadId();
    record.depth = focus_trace.depth;
    record.focus = (uintptr_t)GetFocus();
    record.active = (uintptr_t)GetActiveWindow();
    record.foreground = (uintptr_t)GetForegroundWindow();
    record.keyboard_valid = focus_trace.dispatch_primed;
    if (record.keyboard_valid)
        record.keyboard = (uintptr_t)SDL_GetKeyboardFocus();
    focus_trace.ring[focus_trace.next] = record;
    focus_trace.next = (focus_trace.next + 1u) % FOCUS_TRACE_CAPACITY;
    if (focus_trace.count < FOCUS_TRACE_CAPACITY)
        ++focus_trace.count;
    else
        ++focus_trace.overwritten;
    /* A completed fixture step is a candidate, not an assertion that an
     * intermediate native/SDL difference is a persistent failure or its cause. */
    if (!focus_trace.candidate && record.completed && record.host_sample && record.canvas != 0 &&
        record.frame != 0 && record.focus == record.canvas && record.foreground == record.frame &&
        record.root == record.frame && record.keyboard_valid && record.keyboard == 0) {
        focus_trace.candidate = true;
        focus_trace.candidate_record = record;
        focus_trace.candidate_count = focus_trace.count;
        focus_trace.candidate_overwritten = focus_trace.overwritten;
        const size_t first =
            (focus_trace.next + FOCUS_TRACE_CAPACITY - focus_trace.count) % FOCUS_TRACE_CAPACITY;
        for (size_t i = 0; i < focus_trace.count; ++i)
            focus_trace.first[i] = focus_trace.ring[(first + i) % FOCUS_TRACE_CAPACITY];
    }
    focus_trace.recording = false;
}

static LRESULT CALLBACK focus_trace_before(int code, WPARAM hook_wparam, LPARAM data) {
    const DWORD saved = GetLastError();
    if (code >= 0) {
        ++focus_trace.depth;
        if (focus_trace.depth > focus_trace.maximum_depth)
            focus_trace.maximum_depth = focus_trace.depth;
        const CWPSTRUCT *message = (const CWPSTRUCT *)data;
        if (focus_trace_message(message->message)) {
            focus_trace_record record = {.phase = "wndproc-before",
                                         .hwnd = (uintptr_t)message->hwnd,
                                         .wparam = (uintptr_t)message->wParam,
                                         .lparam = (uintptr_t)message->lParam,
                                         .message = message->message,
                                         .hook_wparam = (uintptr_t)hook_wparam,
                                         .last_error = saved};
            focus_trace_append(record);
        }
    }
    SetLastError(saved); /* Observer work must not reach the next hook as errno. */
    return CallNextHookEx(NULL, code, hook_wparam, data);
}

static LRESULT CALLBACK focus_trace_after(int code, WPARAM hook_wparam, LPARAM data) {
    const DWORD saved = GetLastError();
    if (code >= 0) {
        const CWPRETSTRUCT *message = (const CWPRETSTRUCT *)data;
        if (focus_trace_message(message->message)) {
            focus_trace_record record = {.phase = "wndproc-after",
                                         .hwnd = (uintptr_t)message->hwnd,
                                         .wparam = (uintptr_t)message->wParam,
                                         .lparam = (uintptr_t)message->lParam,
                                         .result = (uintptr_t)message->lResult,
                                         .message = message->message,
                                         .hook_wparam = (uintptr_t)hook_wparam,
                                         .last_error = saved};
            focus_trace_append(record);
        }
        if (focus_trace.depth != 0)
            --focus_trace.depth;
        else
            ++focus_trace.unmatched;
    }
    SetLastError(saved);
    /* No observer work after chaining: preserve its real result and LastError. */
    return CallNextHookEx(NULL, code, hook_wparam, data);
}

static void focus_trace_mark(const char *phase, const mc_client_frame_state *host, int outcome,
                             bool completed, const bool *sdk_active) {
    if (!focus_trace.enabled || focus_trace.finished)
        return;
    const DWORD saved = GetLastError();
    focus_trace_record record = {
        .phase = phase, .outcome = outcome, .completed = completed, .last_error = saved};
    if (sdk_active != NULL) {
        record.sdk_active_valid = true;
        record.sdk_active = *sdk_active;
    }
    if (host != NULL) {
        record.host_sample = true;
        record.sdl_frame = (uintptr_t)host->frame_window;
        record.sdl_canvas = (uintptr_t)host->canvas_window;
        record.frame = (uintptr_t)SDL_GetPointerProperty(
            SDL_GetWindowProperties(host->frame_window), SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
        record.canvas = (uintptr_t)SDL_GetPointerProperty(
            SDL_GetWindowProperties(host->canvas_window), SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
        record.parent = (uintptr_t)GetParent((HWND)record.canvas);
        record.root = (uintptr_t)GetAncestor((HWND)record.canvas, GA_ROOT);
        record.style = (uint64_t)(uintptr_t)GetWindowLongPtrW((HWND)record.canvas, GWL_STYLE);
        record.sdl_parent = (uintptr_t)SDL_GetWindowParent(host->canvas_window);
        record.frame_flags = SDL_GetWindowFlags(host->frame_window);
        record.canvas_flags = SDL_GetWindowFlags(host->canvas_window);
    }
    focus_trace_append(record);
    SetLastError(saved);
}

static bool focus_trace_line(const char *format, ...) {
    if (focus_trace.io_failed || focus_trace.file == NULL)
        return false;
    char line[2048];
    va_list args;
    va_start(args, format);
    const int length = vsnprintf(line, sizeof(line), format, args);
    va_end(args);
    if (length < 0 || (size_t)length >= sizeof(line) ||
        focus_trace.written + (uint64_t)length > UINT64_C(16) * 1048576u) {
        const DWORD error = GetLastError();
        focus_trace.io_failed = true;
        focus_trace_diagnostic("trace-format-or-byte-bound", error, (uintptr_t)focus_trace.file);
        return false;
    }
    DWORD written = 0;
    const bool wrote = WriteFile(focus_trace.file, line, (DWORD)length, &written, NULL) != 0;
    const DWORD error = GetLastError();
    if (!wrote || written != (DWORD)length) {
        focus_trace.io_failed = true;
        focus_trace_diagnostic(wrote ? "WriteFile-short" : "WriteFile", error,
                               (uintptr_t)focus_trace.file);
        return false;
    }
    focus_trace.written += written;
    return true;
}

static void focus_trace_emit(const char *section, const focus_trace_record *r) {
    (void)focus_trace_line(
        "{\"type\":\"sample\",\"section\":\"%s\",\"seq\":%" PRIu64 ",\"qpc\":%" PRId64
        ",\"qpc_valid\":%d,\"phase\":\"%s\",\"thread\":%lu,"
        "\"depth\":%u,\"message\":%u,\"hook_wparam\":\"%" PRIxPTR "\",\"last_error\":%lu,"
        "\"hwnd\":\"%" PRIxPTR "\",\"wparam\":\"%" PRIxPTR "\",\"lparam\":\"%" PRIxPTR
        "\",\"result\":\"%" PRIxPTR "\",\"focus\":\"%" PRIxPTR "\",\"active\":\"%" PRIxPTR
        "\",\"foreground\":\"%" PRIxPTR "\",\"keyboard\":\"%" PRIxPTR "\",\"frame\":\"%" PRIxPTR
        "\",\"canvas\":\"%" PRIxPTR "\",\"parent\":\"%" PRIxPTR "\",\"root\":\"%" PRIxPTR
        "\",\"sdl_parent\":\"%" PRIxPTR "\",\"sdl_frame\":\"%" PRIxPTR
        "\",\"sdl_canvas\":\"%" PRIxPTR "\",\"style\":\"%" PRIx64 "\",\"frame_flags\":\"%" PRIx64
        "\",\"canvas_flags\":\"%" PRIx64
        "\",\"keyboard_valid\":%d,\"host_sample\":%d,\"completed\":%d,\"outcome\":%d,"
        "\"sdk_active_valid\":%d,\"sdk_active\":%d}\n",
        section, r->sequence, r->counter, r->counter_valid, r->phase, (unsigned long)r->thread,
        r->depth, r->message, r->hook_wparam, (unsigned long)r->last_error, r->hwnd, r->wparam,
        r->lparam, r->result, r->focus, r->active, r->foreground, r->keyboard, r->frame, r->canvas,
        r->parent, r->root, r->sdl_parent, r->sdl_frame, r->sdl_canvas, r->style, r->frame_flags,
        r->canvas_flags, r->keyboard_valid, r->host_sample, r->completed, r->outcome,
        r->sdk_active_valid, r->sdk_active);
}

static bool focus_trace_finish(bool completed) {
    if (!focus_trace.enabled)
        return true;
    if (focus_trace.finished)
        return focus_trace.finish_ok;
    const DWORD saved = GetLastError();
    focus_trace_mark("observer-stop", NULL, completed ? 1 : 0, false, NULL);
    focus_trace.original_completed = completed;
    if (focus_trace.after != NULL) {
        focus_trace.unhook_after = UnhookWindowsHookEx(focus_trace.after) != 0;
        focus_trace.unhook_after_error = focus_trace.unhook_after ? 0 : GetLastError();
        if (focus_trace.unhook_after)
            focus_trace.after = NULL;
        else
            focus_trace_diagnostic("UnhookWindowsHookEx-after", focus_trace.unhook_after_error,
                                   (uintptr_t)focus_trace.after);
    }
    if (focus_trace.before != NULL) {
        focus_trace.unhook_before = UnhookWindowsHookEx(focus_trace.before) != 0;
        focus_trace.unhook_before_error = focus_trace.unhook_before ? 0 : GetLastError();
        if (focus_trace.unhook_before)
            focus_trace.before = NULL;
        else
            focus_trace_diagnostic("UnhookWindowsHookEx-before", focus_trace.unhook_before_error,
                                   (uintptr_t)focus_trace.before);
    }
    focus_trace.finished = true;
    (void)focus_trace_line(
        "{\"type\":\"begin\",\"schema\":1,\"thread\":%lu,\"capacity\":%u,"
        "\"frequency\":%" PRId64 ",\"frequency_valid\":%d,\"before_installed\":%d,"
        "\"after_installed\":%d,\"dispatch_primed\":%d,\"before_error\":%lu,\"after_error\":%lu,"
        "\"scope\":\"test-only current-thread Windows observer; generated SDL source selection; "
        "not entered-code or causal proof\"}\n",
        (unsigned long)focus_trace.thread, (unsigned int)FOCUS_TRACE_CAPACITY,
        focus_trace.frequency.QuadPart, focus_trace.frequency_valid, focus_trace.before_installed,
        focus_trace.after_installed, focus_trace.dispatch_primed,
        (unsigned long)focus_trace.before_error, (unsigned long)focus_trace.after_error);
    if (focus_trace.candidate) {
        focus_trace_emit("first-candidate", &focus_trace.candidate_record);
        for (size_t i = 0; i < focus_trace.candidate_count; ++i)
            focus_trace_emit("candidate-history", &focus_trace.first[i]);
    }
    const size_t first =
        (focus_trace.next + FOCUS_TRACE_CAPACITY - focus_trace.count) % FOCUS_TRACE_CAPACITY;
    for (size_t i = 0; i < focus_trace.count; ++i)
        focus_trace_emit("final-history", &focus_trace.ring[(first + i) % FOCUS_TRACE_CAPACITY]);
    (void)focus_trace_line(
        "{\"type\":\"end\",\"records\":%" PRIu64 ",\"retained\":%zu,\"overwritten\":%" PRIu64
        ",\"reentrant_dropped\":%" PRIu64 ",\"unmatched_post\":%" PRIu64
        ",\"depth\":%u,\"maximum_depth\":%u,\"candidate\":%d,\"candidate_records\":%zu,"
        "\"candidate_overwritten\":%" PRIu64 ","
        "\"original_completed\":%d,\"exit_fallback\":%d,\"unhook_before\":%d,"
        "\"unhook_after\":%d,\"unhook_before_error\":%lu,\"unhook_after_error\":%lu,"
        "\"io_failed\":%d}\n",
        focus_trace.sequence, focus_trace.count, focus_trace.overwritten, focus_trace.reentrant,
        focus_trace.unmatched, focus_trace.depth, focus_trace.maximum_depth, focus_trace.candidate,
        focus_trace.candidate_count, focus_trace.candidate_overwritten, completed,
        focus_trace.exit_fallback, focus_trace.unhook_before, focus_trace.unhook_after,
        (unsigned long)focus_trace.unhook_before_error,
        (unsigned long)focus_trace.unhook_after_error, focus_trace.io_failed);
    /* The footer precedes close: only a complete native exit0 also proves
     * successful file close. On failure retain the exact pending HANDLE and
     * its diagnostic until process exit; do not blindly retry an invalid one. */
    const bool closed = focus_trace_close_file();
    const bool ok =
        !focus_trace.io_failed && closed && focus_trace.before == NULL && focus_trace.after == NULL;
    focus_trace.finish_ok = ok;
    SetLastError(saved);
    return ok;
}

static void focus_trace_at_exit(void) {
    focus_trace.exit_fallback = true;
    (void)focus_trace_finish(false);
}

static bool focus_trace_environment_absent(const WCHAR *name, const char *operation) {
    const DWORD saved = GetLastError();
    SetLastError(ERROR_SUCCESS);
    const DWORD length = GetEnvironmentVariableW(name, NULL, 0);
    const DWORD error = GetLastError();
    const bool absent = length == 0 && error == ERROR_ENVVAR_NOT_FOUND;
    if (!absent)
        focus_trace_diagnostic(operation, error, 0);
    SetLastError(saved);
    return absent;
}

static bool focus_trace_start(void) {
    const DWORD saved = GetLastError();
    SetLastError(ERROR_SUCCESS);
    const DWORD length = GetEnvironmentVariableW(L"MC_WORLD_SELECTION_FOCUS_TRACE",
                                                 focus_trace.path, FOCUS_TRACE_PATH);
    const DWORD environment_error = GetLastError();
    if (length == 0) {
        SetLastError(saved);
        return true;
    }
    if (length >= FOCUS_TRACE_PATH) {
        focus_trace_diagnostic("trace-path-bound", environment_error, 0);
        SetLastError(saved);
        return false;
    }
    focus_trace.file = CreateFileW(focus_trace.path, GENERIC_WRITE, 0, NULL, CREATE_NEW,
                                   FILE_ATTRIBUTE_NORMAL, NULL);
    if (focus_trace.file == INVALID_HANDLE_VALUE) {
        const DWORD error = GetLastError();
        focus_trace.file = NULL;
        focus_trace_diagnostic("CreateFileW-CREATE_NEW", error, 0);
        SetLastError(saved);
        return false;
    }
    const DWORD file_type = GetFileType(focus_trace.file);
    const DWORD type_error = GetLastError();
    if (file_type != FILE_TYPE_DISK) {
        focus_trace_diagnostic("trace-file-not-disk", type_error, (uintptr_t)focus_trace.file);
        (void)focus_trace_close_file();
        SetLastError(saved);
        return false;
    }
    focus_trace.enabled = true;
    focus_trace.thread = GetCurrentThreadId();
    focus_trace.frequency_valid = QueryPerformanceFrequency(&focus_trace.frequency) != 0;
    if (!focus_trace.frequency_valid) {
        const DWORD error = GetLastError();
        focus_trace_diagnostic("QueryPerformanceFrequency", error, (uintptr_t)focus_trace.file);
        (void)focus_trace_finish(false);
        SetLastError(saved);
        return false;
    }
    if (atexit(focus_trace_at_exit) != 0) {
        const DWORD error = GetLastError();
        focus_trace_diagnostic("atexit-registration", error, (uintptr_t)focus_trace.file);
        (void)focus_trace_finish(false);
        SetLastError(saved);
        return false;
    }
    if (!focus_trace_environment_absent(L"SDL3_DYNAMIC_API", "SDL3_DYNAMIC_API-not-absent") ||
        !focus_trace_environment_absent(L"SDL_DYNAPI_LOG_CALLS",
                                        "SDL_DYNAPI_LOG_CALLS-not-absent")) {
        (void)focus_trace_finish(false);
        SetLastError(saved);
        return false;
    }
    /* Opt-in observation primes the public dynapi trampoline here, outside
     * both hooks. The default path reaches none of this SDL initialization.
     * No focus setter, event pump or Source/game-state operation is added. */
    const DWORD prime_error = GetLastError();
    (void)SDL_GetKeyboardFocus();
    focus_trace.dispatch_primed = true;
    SetLastError(prime_error);
    focus_trace.before =
        SetWindowsHookExW(WH_CALLWNDPROC, focus_trace_before, NULL, focus_trace.thread);
    focus_trace.before_installed = focus_trace.before != NULL;
    focus_trace.before_error = focus_trace.before_installed ? 0 : GetLastError();
    if (!focus_trace.before_installed)
        focus_trace_diagnostic("SetWindowsHookExW-before", focus_trace.before_error, 0);
    if (focus_trace.before_installed) {
        focus_trace.after =
            SetWindowsHookExW(WH_CALLWNDPROCRET, focus_trace_after, NULL, focus_trace.thread);
        focus_trace.after_installed = focus_trace.after != NULL;
        focus_trace.after_error = focus_trace.after_installed ? 0 : GetLastError();
        if (!focus_trace.after_installed)
            focus_trace_diagnostic("SetWindowsHookExW-after", focus_trace.after_error, 0);
    }
    const bool ok = focus_trace.before_installed && focus_trace.after_installed;
    if (ok)
        focus_trace_mark("observer-start", NULL, 1, false, NULL);
    else
        (void)focus_trace_finish(false);
    SetLastError(saved);
    return ok;
}
#endif
