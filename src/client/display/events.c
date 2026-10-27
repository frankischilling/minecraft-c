#include "events.h"

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_video.h>
#include <stdlib.h>
#include <string.h>

typedef struct event_memory {
    struct event_memory *next;
    size_t bytes;
    unsigned char data[];
} event_memory;

typedef struct event_owner event_owner;

struct mc_client_event_record {
    event_owner *owner;
    mc_client_event_port *ticket_port;
    struct mc_client_event_record *next;
    event_memory *memory;
    SDL_Event event;
    SDL_WindowID window;
    int width, height;
    bool window_extent;
    size_t bytes;
    bool ticket;
};

struct mc_client_event_port {
    event_owner *owner;
    struct mc_client_event_port *next;
    SDL_Window *window;
    SDL_WindowID id;
    int width, height;
    bool dirty, resized, close_requested;
    size_t tickets;
};

struct event_owner {
    mc_client_event_port *ports;
    mc_client_event_record *head, *tail;
    size_t port_count, records, bytes, tickets, max_bytes;
    uint64_t generation;
    bool active;
};

/* SDL's event queue is itself process-wide. This one native owner is confined
 * to the real SDL main thread; it is never shared mutable worker scratch. */
static event_owner *shared;

static SDL_WindowID event_window(const SDL_Event *v) {
    /* Route by the record's value, even after its actual window has died.
     * SDL_GetWindowFromEvent would turn a stale window ID into a global event. */
    const uint32_t type = v->type;
    if (type >= SDL_EVENT_WINDOW_FIRST && type <= SDL_EVENT_WINDOW_LAST)
        return v->window.windowID;
    if (type == SDL_EVENT_KEY_DOWN || type == SDL_EVENT_KEY_UP)
        return v->key.windowID;
    if (type == SDL_EVENT_TEXT_EDITING)
        return v->edit.windowID;
    if (type == SDL_EVENT_TEXT_INPUT)
        return v->text.windowID;
    if (type == SDL_EVENT_TEXT_EDITING_CANDIDATES)
        return v->edit_candidates.windowID;
    if (type == SDL_EVENT_MOUSE_MOTION)
        return v->motion.windowID;
    if (type == SDL_EVENT_MOUSE_BUTTON_DOWN || type == SDL_EVENT_MOUSE_BUTTON_UP)
        return v->button.windowID;
    if (type == SDL_EVENT_MOUSE_WHEEL)
        return v->wheel.windowID;
    if (type >= SDL_EVENT_FINGER_DOWN && type <= SDL_EVENT_FINGER_CANCELED)
        return v->tfinger.windowID;
    if (type >= SDL_EVENT_PINCH_BEGIN && type <= SDL_EVENT_PINCH_END)
        return v->pinch.windowID;
    if (type >= SDL_EVENT_DROP_FILE && type <= SDL_EVENT_DROP_POSITION)
        return v->drop.windowID;
    if (type == SDL_EVENT_PEN_PROXIMITY_IN || type == SDL_EVENT_PEN_PROXIMITY_OUT)
        return v->pproximity.windowID;
    if (type == SDL_EVENT_PEN_DOWN || type == SDL_EVENT_PEN_UP)
        return v->ptouch.windowID;
    if (type == SDL_EVENT_PEN_BUTTON_DOWN || type == SDL_EVENT_PEN_BUTTON_UP)
        return v->pbutton.windowID;
    if (type == SDL_EVENT_PEN_MOTION)
        return v->pmotion.windowID;
    if (type == SDL_EVENT_PEN_AXIS)
        return v->paxis.windowID;
    if (type >= SDL_EVENT_RENDER_TARGETS_RESET && type <= SDL_EVENT_RENDER_DEVICE_LOST)
        return v->render.windowID;
    if (type >= SDL_EVENT_USER && type < SDL_EVENT_LAST)
        return v->user.windowID;
    return 0;
}

static mc_client_events_status fail(event_owner *o, mc_client_events_error *e,
                                    mc_client_events_status status, const char *operation,
                                    const char *detail) {
    if (e != NULL) {
        e->status = status;
        e->operation = operation;
        e->records_retained = o == NULL ? 0 : o->records;
        e->bytes_retained = o == NULL ? 0 : o->bytes;
        size_t n = 0;
        if (detail != NULL)
            while (n + 1 < sizeof(e->detail) && detail[n] != '\0') {
                e->detail[n] = detail[n];
                ++n;
            }
        e->detail[n] = '\0';
    }
    return status;
}

static bool room(const event_owner *o, size_t bytes) {
    return bytes <= o->max_bytes && o->bytes <= o->max_bytes - bytes;
}

static void *memory(mc_client_event_record *r, size_t bytes, mc_client_events_error *e) {
    event_owner *o = r->owner;
    if (bytes > SIZE_MAX - sizeof(event_memory) || !room(o, sizeof(event_memory) + bytes)) {
        fail(o, e, MC_CLIENT_EVENTS_LIMIT, "retain SDL payload", "native byte budget");
        return NULL;
    }
    event_memory *m = malloc(sizeof(*m) + bytes);
    if (m == NULL) {
        fail(o, e, MC_CLIENT_EVENTS_ALLOCATION_FAILED, "retain SDL payload", "native allocation");
        return NULL;
    }
    m->next = r->memory;
    m->bytes = sizeof(*m) + bytes;
    r->memory = m;
    r->bytes += m->bytes;
    o->bytes += m->bytes;
    return m->data;
}

static bool string(mc_client_event_record *r, const char *in, const char **out,
                   mc_client_events_error *e) {
    if (in == NULL) {
        *out = NULL;
        return true;
    }
    size_t n = 0;
    const size_t available = r->owner->max_bytes - r->owner->bytes;
    while (n < available && in[n] != '\0')
        ++n;
    if (n == available) {
        fail(r->owner, e, MC_CLIENT_EVENTS_LIMIT, "retain SDL string", "native string byte budget");
        return false;
    }
    char *copy = memory(r, n + 1, e);
    if (copy == NULL)
        return false;
    memcpy(copy, in, n + 1);
    *out = copy;
    return true;
}

static bool strings(mc_client_event_record *r, const char *const *in, int32_t count,
                    const char ***out, mc_client_events_error *e) {
    if (count < 0 || (count != 0 && in == NULL) || (size_t)count > SIZE_MAX / sizeof(char *)) {
        fail(r->owner, e, MC_CLIENT_EVENTS_UNSUPPORTED_PAYLOAD, "retain SDL string array",
             "invalid native extent");
        return false;
    }
    if (count == 0) {
        *out = NULL;
        return true;
    }
    const char **copy = memory(r, (size_t)count * sizeof(*copy), e);
    if (copy == NULL)
        return false;
    for (int32_t i = 0; i < count; ++i)
        if (!string(r, in[i], &copy[i], e))
            return false;
    *out = copy;
    return true;
}

static bool scalar_payload(uint32_t type) {
    switch (type) {
        case SDL_EVENT_QUIT:
        case SDL_EVENT_TERMINATING:
        case SDL_EVENT_LOW_MEMORY:
        case SDL_EVENT_WILL_ENTER_BACKGROUND:
        case SDL_EVENT_DID_ENTER_BACKGROUND:
        case SDL_EVENT_WILL_ENTER_FOREGROUND:
        case SDL_EVENT_DID_ENTER_FOREGROUND:
        case SDL_EVENT_LOCALE_CHANGED:
        case SDL_EVENT_SYSTEM_THEME_CHANGED:
        case SDL_EVENT_DISPLAY_ORIENTATION:
        case SDL_EVENT_DISPLAY_ADDED:
        case SDL_EVENT_DISPLAY_REMOVED:
        case SDL_EVENT_DISPLAY_MOVED:
        case SDL_EVENT_DISPLAY_DESKTOP_MODE_CHANGED:
        case SDL_EVENT_DISPLAY_CURRENT_MODE_CHANGED:
        case SDL_EVENT_DISPLAY_CONTENT_SCALE_CHANGED:
        case SDL_EVENT_DISPLAY_USABLE_BOUNDS_CHANGED:
        case SDL_EVENT_WINDOW_SHOWN:
        case SDL_EVENT_WINDOW_HIDDEN:
        case SDL_EVENT_WINDOW_EXPOSED:
        case SDL_EVENT_WINDOW_MOVED:
        case SDL_EVENT_WINDOW_RESIZED:
        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
        case SDL_EVENT_WINDOW_METAL_VIEW_RESIZED:
        case SDL_EVENT_WINDOW_MINIMIZED:
        case SDL_EVENT_WINDOW_MAXIMIZED:
        case SDL_EVENT_WINDOW_RESTORED:
        case SDL_EVENT_WINDOW_MOUSE_ENTER:
        case SDL_EVENT_WINDOW_MOUSE_LEAVE:
        case SDL_EVENT_WINDOW_FOCUS_GAINED:
        case SDL_EVENT_WINDOW_FOCUS_LOST:
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
        case SDL_EVENT_WINDOW_HIT_TEST:
        case SDL_EVENT_WINDOW_ICCPROF_CHANGED:
        case SDL_EVENT_WINDOW_DISPLAY_CHANGED:
        case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
        case SDL_EVENT_WINDOW_SAFE_AREA_CHANGED:
        case SDL_EVENT_WINDOW_OCCLUDED:
        case SDL_EVENT_WINDOW_ENTER_FULLSCREEN:
        case SDL_EVENT_WINDOW_LEAVE_FULLSCREEN:
        case SDL_EVENT_WINDOW_DESTROYED:
        case SDL_EVENT_WINDOW_HDR_STATE_CHANGED:
        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP:
        case SDL_EVENT_KEYMAP_CHANGED:
        case SDL_EVENT_KEYBOARD_ADDED:
        case SDL_EVENT_KEYBOARD_REMOVED:
        case SDL_EVENT_SCREEN_KEYBOARD_SHOWN:
        case SDL_EVENT_SCREEN_KEYBOARD_HIDDEN:
        case SDL_EVENT_MOUSE_MOTION:
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP:
        case SDL_EVENT_MOUSE_WHEEL:
        case SDL_EVENT_MOUSE_ADDED:
        case SDL_EVENT_MOUSE_REMOVED:
        case SDL_EVENT_JOYSTICK_AXIS_MOTION:
        case SDL_EVENT_JOYSTICK_BALL_MOTION:
        case SDL_EVENT_JOYSTICK_HAT_MOTION:
        case SDL_EVENT_JOYSTICK_BUTTON_DOWN:
        case SDL_EVENT_JOYSTICK_BUTTON_UP:
        case SDL_EVENT_JOYSTICK_ADDED:
        case SDL_EVENT_JOYSTICK_REMOVED:
        case SDL_EVENT_JOYSTICK_BATTERY_UPDATED:
        case SDL_EVENT_JOYSTICK_UPDATE_COMPLETE:
        case SDL_EVENT_GAMEPAD_AXIS_MOTION:
        case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        case SDL_EVENT_GAMEPAD_BUTTON_UP:
        case SDL_EVENT_GAMEPAD_ADDED:
        case SDL_EVENT_GAMEPAD_REMOVED:
        case SDL_EVENT_GAMEPAD_REMAPPED:
        case SDL_EVENT_GAMEPAD_TOUCHPAD_DOWN:
        case SDL_EVENT_GAMEPAD_TOUCHPAD_MOTION:
        case SDL_EVENT_GAMEPAD_TOUCHPAD_UP:
        case SDL_EVENT_GAMEPAD_SENSOR_UPDATE:
        case SDL_EVENT_GAMEPAD_UPDATE_COMPLETE:
        case SDL_EVENT_GAMEPAD_STEAM_HANDLE_UPDATED:
        case SDL_EVENT_FINGER_DOWN:
        case SDL_EVENT_FINGER_UP:
        case SDL_EVENT_FINGER_MOTION:
        case SDL_EVENT_FINGER_CANCELED:
        case SDL_EVENT_PINCH_BEGIN:
        case SDL_EVENT_PINCH_UPDATE:
        case SDL_EVENT_PINCH_END:
        case SDL_EVENT_AUDIO_DEVICE_ADDED:
        case SDL_EVENT_AUDIO_DEVICE_REMOVED:
        case SDL_EVENT_AUDIO_DEVICE_FORMAT_CHANGED:
        case SDL_EVENT_SENSOR_UPDATE:
        case SDL_EVENT_PEN_PROXIMITY_IN:
        case SDL_EVENT_PEN_PROXIMITY_OUT:
        case SDL_EVENT_PEN_DOWN:
        case SDL_EVENT_PEN_UP:
        case SDL_EVENT_PEN_BUTTON_DOWN:
        case SDL_EVENT_PEN_BUTTON_UP:
        case SDL_EVENT_PEN_MOTION:
        case SDL_EVENT_PEN_AXIS:
        case SDL_EVENT_CAMERA_DEVICE_ADDED:
        case SDL_EVENT_CAMERA_DEVICE_REMOVED:
        case SDL_EVENT_CAMERA_DEVICE_APPROVED:
        case SDL_EVENT_CAMERA_DEVICE_DENIED:
        case SDL_EVENT_RENDER_TARGETS_RESET:
        case SDL_EVENT_RENDER_DEVICE_RESET:
        case SDL_EVENT_RENDER_DEVICE_LOST:
        case SDL_EVENT_POLL_SENTINEL:
            return true;
        default:
            return false;
    }
}

static bool clone_payload(mc_client_event_record *r, mc_client_events_error *e) {
    SDL_Event *v = &r->event;
    switch (v->type) {
        case SDL_EVENT_TEXT_EDITING:
            return string(r, v->edit.text, &v->edit.text, e);
        case SDL_EVENT_TEXT_INPUT:
            return string(r, v->text.text, &v->text.text, e);
        case SDL_EVENT_TEXT_EDITING_CANDIDATES: {
            const char **copy = NULL;
            if (!strings(r, v->edit_candidates.candidates, v->edit_candidates.num_candidates, &copy,
                         e))
                return false;
            v->edit_candidates.candidates = copy;
            return true;
        }
        case SDL_EVENT_DROP_BEGIN:
        case SDL_EVENT_DROP_FILE:
        case SDL_EVENT_DROP_TEXT:
        case SDL_EVENT_DROP_COMPLETE:
        case SDL_EVENT_DROP_POSITION:
            return string(r, v->drop.source, &v->drop.source, e) &&
                   string(r, v->drop.data, &v->drop.data, e);
        case SDL_EVENT_CLIPBOARD_UPDATE: {
            const char **copy = NULL;
            if (!strings(r, v->clipboard.mime_types, v->clipboard.num_mime_types, &copy, e))
                return false;
            v->clipboard.mime_types = copy;
            return true;
        }
        default:
            /* Admit only audited scalar SDL3.4.18 records or an explicitly
             * pointer-free user record. Reserved/private/future types do not
             * acquire an inferred lifetime merely by fitting SDL_Event. */
            if (scalar_payload(v->type) || (v->type >= SDL_EVENT_USER && v->type < SDL_EVENT_LAST &&
                                            v->user.data1 == NULL && v->user.data2 == NULL))
                return true;
            fail(r->owner, e, MC_CLIENT_EVENTS_UNSUPPORTED_PAYLOAD, "retain opaque SDL event",
                 "no declared payload ownership");
            return false;
    }
}

static void record_free(mc_client_event_record *r) {
    while (r->memory != NULL) {
        event_memory *m = r->memory;
        r->memory = m->next;
        free(m);
    }
    r->owner->bytes -= r->bytes;
    free(r);
}

mc_client_events_status mc_client_events_attach(SDL_Window *window, size_t max_bytes,
                                                mc_client_event_port **out,
                                                mc_client_events_error *e) {
    if (e != NULL)
        *e = (mc_client_events_error){0};
    if (!SDL_IsMainThread() || window == NULL || out == NULL)
        return fail(shared, e, MC_CLIENT_EVENTS_INVALID_ARGUMENT, "attach SDL event port",
                    "live window/output/main thread required");
    if (max_bytes == 0)
        max_bytes = 8u * 1024u * 1024u;
    if (shared != NULL && (shared->active || shared->max_bytes != max_bytes))
        return fail(shared, e, MC_CLIENT_EVENTS_BUSY, "attach SDL event port",
                    "active owner or different fixed native byte policy");
    const SDL_WindowID id = SDL_GetWindowID(window);
    int width = 0, height = 0;
    if (id == 0 || !SDL_GetWindowSize(window, &width, &height))
        return fail(shared, e, MC_CLIENT_EVENTS_SDL_FAILED, "attach SDL event port",
                    SDL_GetError());
    if (shared != NULL)
        for (mc_client_event_port *v = shared->ports; v != NULL; v = v->next)
            if (v->id == id)
                return fail(shared, e, MC_CLIENT_EVENTS_INVALID_ARGUMENT, "attach SDL event port",
                            "window already attached");
    event_owner *o = shared;
    const bool fresh = o == NULL;
    if (fresh) {
        if (max_bytes < sizeof(event_owner) + sizeof(mc_client_event_port))
            return fail(NULL, e, MC_CLIENT_EVENTS_LIMIT, "attach SDL event owner", "native budget");
        o = calloc(1, sizeof(*o));
        if (o == NULL)
            return fail(NULL, e, MC_CLIENT_EVENTS_ALLOCATION_FAILED, "attach SDL event owner",
                        "native allocation");
        o->bytes = sizeof(*o);
        o->max_bytes = max_bytes;
    }
    if (!room(o, sizeof(mc_client_event_port))) {
        const mc_client_events_status status =
            fail(o, e, MC_CLIENT_EVENTS_LIMIT, "attach SDL event port", "native budget");
        if (fresh)
            free(o);
        return status;
    }
    mc_client_event_port *p = calloc(1, sizeof(*p));
    if (p == NULL) {
        const mc_client_events_status status = fail(o, e, MC_CLIENT_EVENTS_ALLOCATION_FAILED,
                                                    "attach SDL event port", "native allocation");
        if (fresh)
            free(o);
        return status;
    }
    p->window = window;
    p->id = id;
    p->width = width;
    p->height = height;
    p->owner = o;
    p->next = o->ports;
    o->ports = p;
    ++o->port_count;
    o->bytes += sizeof(*p);
    if (fresh)
        shared = o;
    *out = p;
    return MC_CLIENT_EVENTS_OK;
}

static mc_client_events_status transfer_pending(event_owner *o, mc_client_events_error *e) {
    const int pending = SDL_PeepEvents(NULL, 0, SDL_PEEKEVENT, SDL_EVENT_FIRST, SDL_EVENT_LAST);
    mc_client_events_status status = MC_CLIENT_EVENTS_OK;
    if (pending < 0)
        status =
            fail(o, e, MC_CLIENT_EVENTS_SDL_FAILED, "inspect pumped SDL queue", SDL_GetError());
    for (int i = 0; i < pending && status == MC_CLIENT_EVENTS_OK; ++i) {
        SDL_Event peek;
        const int count = SDL_PeepEvents(&peek, 1, SDL_PEEKEVENT, SDL_EVENT_FIRST, SDL_EVENT_LAST);
        if (count != 1) {
            status = fail(o, e, MC_CLIENT_EVENTS_SDL_FAILED, "peek retained SDL head",
                          count < 0 ? SDL_GetError() : "exclusive queue head changed");
            break;
        }
        if (e != NULL)
            e->pending_type = peek.type;
        if (!room(o, sizeof(mc_client_event_record))) {
            status =
                fail(o, e, MC_CLIENT_EVENTS_LIMIT, "retain SDL event head", "native byte budget");
            break;
        }
        mc_client_event_record *r = calloc(1, sizeof(*r));
        if (r == NULL) {
            status = fail(o, e, MC_CLIENT_EVENTS_ALLOCATION_FAILED, "retain SDL event head",
                          "native allocation");
            break;
        }
        r->owner = o;
        r->event = peek;
        r->bytes = sizeof(*r);
        o->bytes += r->bytes;
        r->window = event_window(&peek);
        if (!clone_payload(r, e)) {
            status = e == NULL ? MC_CLIENT_EVENTS_UNSUPPORTED_PAYLOAD : e->status;
            record_free(r);
            break;
        }
        SDL_Event removed;
        if (SDL_PeepEvents(&removed, 1, SDL_GETEVENT, SDL_EVENT_FIRST, SDL_EVENT_LAST) != 1) {
            status = fail(o, e, MC_CLIENT_EVENTS_SDL_FAILED, "transfer retained SDL head",
                          SDL_GetError());
            record_free(r);
            break;
        }
        /* The sole consumer contract prevents a different head between peeks.
         * Native producers only append; no callback runs between these calls. */
        if (o->tail == NULL)
            o->head = r;
        else
            o->tail->next = r;
        o->tail = r;
        ++o->records;
        for (mc_client_event_port *v = o->ports; v != NULL; v = v->next)
            if (peek.type == SDL_EVENT_QUIT)
                v->close_requested = true;
            else if (v->id == r->window) {
                if (peek.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
                    v->close_requested = true;
                if (peek.type == SDL_EVENT_WINDOW_EXPOSED)
                    v->dirty = true;
                if (peek.type == SDL_EVENT_WINDOW_RESIZED) {
                    v->width = peek.window.data1;
                    v->height = peek.window.data2;
                    v->resized = true;
                }
                r->width = v->width;
                r->height = v->height;
                r->window_extent = true;
            }
        if (e != NULL) {
            ++e->records_transferred;
            e->pending_type = 0;
        }
    }
    return status;
}

static mc_client_events_status retain(mc_client_event_port *p, bool pump,
                                      mc_client_events_error *out) {
    mc_client_events_error local = {0};
    mc_client_events_error *e = out == NULL ? &local : out;
    *e = (mc_client_events_error){0};
    if (p == NULL || !SDL_IsMainThread())
        return fail(NULL, e, MC_CLIENT_EVENTS_INVALID_ARGUMENT, "retain SDL event owner",
                    "native admission");
    event_owner *o = p->owner;
    if (o->active)
        return fail(o, e, MC_CLIENT_EVENTS_BUSY, "retain SDL event owner",
                    "native operation active");
    if (pump && o->generation == UINT64_MAX)
        return fail(o, e, MC_CLIENT_EVENTS_LIMIT, "pump SDL event owner",
                    "native generation limit");
    o->active = true;
    if (pump) {
        SDL_PumpEvents();
        ++o->generation;
        e->pump_completed = true;
    }
    const mc_client_events_status status = transfer_pending(o, e);
    o->active = false;
    return status;
}

mc_client_events_status mc_client_events_pump(mc_client_event_port *p, mc_client_events_error *e) {
    return retain(p, true, e);
}

mc_client_events_status mc_client_events_retain_pending(mc_client_event_port *p,
                                                        mc_client_events_error *e) {
    return retain(p, false, e);
}

mc_client_events_status mc_client_events_take(mc_client_event_port *p, uint32_t first,
                                              uint32_t last, bool all_windows,
                                              mc_client_event_record **out,
                                              mc_client_events_error *e) {
    if (e != NULL)
        *e = (mc_client_events_error){0};
    if (p == NULL || out == NULL || first > last || !SDL_IsMainThread())
        return fail(NULL, e, MC_CLIENT_EVENTS_INVALID_ARGUMENT, "take retained SDL event",
                    "native admission");
    event_owner *o = p->owner;
    if (o->active || (all_windows && o->port_count != 1))
        return fail(o, e, MC_CLIENT_EVENTS_BUSY, "take retained SDL event",
                    "active or foreign port");
    mc_client_event_record **slot = &o->head;
    mc_client_event_record *previous = NULL;
    while (*slot != NULL) {
        mc_client_event_record *r = *slot;
        if (r->event.type >= first && r->event.type <= last &&
            (all_windows || r->window == 0 || r->window == p->id)) {
            *slot = r->next;
            if (o->tail == r)
                o->tail = previous;
            r->next = NULL;
            r->ticket = true;
            r->ticket_port = p;
            --o->records;
            ++o->tickets;
            ++p->tickets;
            *out = r;
            return MC_CLIENT_EVENTS_OK;
        }
        previous = r;
        slot = &r->next;
    }
    *out = NULL;
    return MC_CLIENT_EVENTS_OK;
}

const SDL_Event *mc_client_event_read(const mc_client_event_record *r) {
    return r == NULL ? NULL : &r->event;
}

bool mc_client_event_window_extent(const mc_client_event_record *r, int *width, int *height) {
    if (r == NULL || width == NULL || height == NULL || !r->window_extent)
        return false;
    *width = r->width;
    *height = r->height;
    return true;
}

void mc_client_event_release(mc_client_event_record **slot) {
    if (slot != NULL && *slot != NULL) {
        mc_client_event_record *r = *slot;
        if (r->ticket) {
            --r->owner->tickets;
            --r->ticket_port->tickets;
            record_free(r);
            *slot = NULL;
        }
    }
}

bool mc_client_events_inspect(const mc_client_event_port *p, mc_client_events_state *out) {
    if (p == NULL || out == NULL)
        return false;
    *out = (mc_client_events_state){.ports = p->owner->port_count,
                                    .records = p->owner->records,
                                    .bytes = p->owner->bytes,
                                    .tickets = p->owner->tickets,
                                    .pump_generation = p->owner->generation,
                                    .active = p->owner->active,
                                    .dirty = p->dirty,
                                    .resized = p->resized,
                                    .width = p->width,
                                    .height = p->height,
                                    .port_tickets = p->tickets};
    return true;
}

bool mc_client_events_take_dirty(mc_client_event_port *p) {
    const bool value = p != NULL && p->dirty;
    if (p != NULL)
        p->dirty = false;
    return value;
}

bool mc_client_events_take_resized(mc_client_event_port *p) {
    const bool value = p != NULL && p->resized;
    if (p != NULL)
        p->resized = false;
    return value;
}

bool mc_client_events_take_close(mc_client_event_port *p) {
    const bool value = p != NULL && p->close_requested;
    if (p != NULL)
        p->close_requested = false;
    return value;
}

mc_client_events_status mc_client_events_detach(mc_client_event_port **slot,
                                                mc_client_events_error *e) {
    if (e != NULL)
        *e = (mc_client_events_error){0};
    if (slot == NULL || !SDL_IsMainThread())
        return fail(shared, e, MC_CLIENT_EVENTS_INVALID_ARGUMENT, "detach SDL event port",
                    "native admission");
    mc_client_event_port *p = *slot;
    if (p == NULL)
        return MC_CLIENT_EVENTS_OK;
    event_owner *o = p->owner;
    if (o->active || p->tickets != 0)
        return fail(o, e, MC_CLIENT_EVENTS_BUSY, "detach SDL event port", "active owned events");
    for (mc_client_event_record *r = o->head; r != NULL; r = r->next)
        if (o->port_count == 1 || r->window == 0 || r->window == p->id)
            return fail(o, e, MC_CLIENT_EVENTS_BUSY, "detach SDL event port",
                        "queued owned events");
    mc_client_event_port **link = &o->ports;
    while (*link != p)
        link = &(*link)->next;
    *link = p->next;
    --o->port_count;
    o->bytes -= sizeof(*p);
    free(p);
    *slot = NULL;
    if (o->port_count == 0) {
        free(o);
        shared = NULL;
    }
    return MC_CLIENT_EVENTS_OK;
}

mc_client_events_status mc_client_events_retire(mc_client_event_port **slot,
                                                mc_client_events_error *out) {
    mc_client_events_error local = {0};
    mc_client_events_error *e = out == NULL ? &local : out;
    *e = (mc_client_events_error){0};
    if (slot == NULL || !SDL_IsMainThread())
        return fail(shared, e, MC_CLIENT_EVENTS_INVALID_ARGUMENT, "retire SDL event port",
                    "native admission");
    mc_client_event_port *p = *slot;
    if (p == NULL)
        return MC_CLIENT_EVENTS_OK;
    event_owner *o = p->owner;
    if (o->active || p->tickets != 0)
        return fail(o, e, MC_CLIENT_EVENTS_BUSY, "retire SDL event port",
                    "active owned occurrences");
    /* This terminal operation owns only already-transferred occurrences.
     * Pending SDL records remain SDL-owned, including unknown payloads. The
     * canonical caller next destroys this window; a later real pump keeps any
     * stale record's actual window ID instead of rerouting it to another port.
     * No unrelated window/global SDL head is removed merely for cleanup. */
    mc_client_event_record **link = &o->head;
    mc_client_event_record *previous = NULL;
    while (*link != NULL) {
        mc_client_event_record *r = *link;
        if (o->port_count == 1 || r->window == p->id) {
            *link = r->next;
            if (o->tail == r)
                o->tail = previous;
            --o->records;
            ++e->records_retired;
            record_free(r);
        } else {
            previous = r;
            link = &r->next;
        }
    }
    /* Global records stay with the other live ports. detach is intentionally
     * stricter, so terminal unlink is explicit rather than disguised discard. */
    mc_client_event_port **port = &o->ports;
    while (*port != p)
        port = &(*port)->next;
    *port = p->next;
    --o->port_count;
    o->bytes -= sizeof(*p);
    free(p);
    *slot = NULL;
    if (o->port_count == 0) {
        free(o);
        shared = NULL;
    }
    return MC_CLIENT_EVENTS_OK;
}
