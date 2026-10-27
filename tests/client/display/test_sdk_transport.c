#include "events.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stdio.h>
#include <string.h>

/* Test-only actual pinned SDL3.4.18 internal producer. This exact declaration
 * is bound to src/events/SDL_events_c.h; there is no authored implementation.
 * It gives the real SDL temporary-memory list/GET/next-pump free path, rather
 * than pretending an application-owned pointer was SDL-owned. */
extern void *SDL_AllocateTemporaryMemory(size_t size);

static unsigned checks;
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition);                        \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

static bool lifetime(void) {
    bool ok = false, video = false;
    SDL_Window *a = NULL, *b = NULL;
    mc_client_event_port *pa = NULL, *pb = NULL;
    mc_client_event_record *first = NULL, *second = NULL, *key = NULL;
    mc_client_events_error error;
    mc_client_events_state state;
    CHECK(SDL_InitSubSystem(SDL_INIT_VIDEO));
    video = true;
    a = SDL_CreateWindow("owned event A", 160, 120, SDL_WINDOW_HIDDEN);
    b = SDL_CreateWindow("owned event B", 160, 120, SDL_WINDOW_HIDDEN);
    CHECK(a != NULL && b != NULL);
    CHECK(mc_client_events_attach(a, 0, &pa, &error) == MC_CLIENT_EVENTS_OK);
    CHECK(mc_client_events_attach(b, 0, &pb, &error) == MC_CLIENT_EVENTS_OK);
    char *one = SDL_AllocateTemporaryMemory(6), *two = SDL_AllocateTemporaryMemory(7);
    CHECK(one != NULL && two != NULL);
    memcpy(one, "first", 6);
    memcpy(two, "second", 7);
    SDL_Event event = {0};
    event.type = SDL_EVENT_TEXT_INPUT;
    event.text.windowID = SDL_GetWindowID(a);
    event.text.text = one;
    CHECK(SDL_PushEvent(&event));
    event.text.text = two;
    CHECK(SDL_PushEvent(&event));
    event = (SDL_Event){0};
    event.type = SDL_EVENT_KEY_DOWN;
    event.key.windowID = SDL_GetWindowID(b);
    event.key.scancode = SDL_SCANCODE_A;
    CHECK(SDL_PushEvent(&event));
    CHECK(mc_client_events_retain_pending(pa, &error) == MC_CLIENT_EVENTS_OK &&
          !error.pump_completed);
    CHECK(mc_client_events_take(pa, SDL_EVENT_TEXT_INPUT, SDL_EVENT_TEXT_INPUT, false, &first,
                                &error) == MC_CLIENT_EVENTS_OK &&
          first != NULL);
    CHECK(strcmp(mc_client_event_read(first)->text.text, "first") == 0);
    CHECK(mc_client_events_take(pa, SDL_EVENT_KEY_DOWN, SDL_EVENT_KEY_UP, false, &key, &error) ==
              MC_CLIENT_EVENTS_OK &&
          key == NULL);
    CHECK(mc_client_events_take(pb, SDL_EVENT_KEY_DOWN, SDL_EVENT_KEY_UP, false, &key, &error) ==
              MC_CLIENT_EVENTS_OK &&
          key != NULL);
    CHECK(mc_client_event_read(key)->key.scancode == SDL_SCANCODE_A);
    mc_client_event_release(&key);
    mc_client_event_port *same = pa;
    CHECK(mc_client_events_retire(&pa, &error) == MC_CLIENT_EVENTS_BUSY && pa == same);
    CHECK(mc_client_events_retire(&pb, &error) == MC_CLIENT_EVENTS_OK && pb == NULL);
    CHECK(mc_client_events_inspect(pa, &state) && state.ports == 1 && state.port_tickets == 1);
    CHECK(mc_client_events_pump(pa, &error) == MC_CLIENT_EVENTS_OK && error.pump_completed);
    /* The real SDL-owned source memory was transferred to TLS on GET and
     * freed by this subsequent pump. These bytes belong to our live ticket. */
    CHECK(strcmp(mc_client_event_read(first)->text.text, "first") == 0);
    CHECK(mc_client_events_take(pa, SDL_EVENT_TEXT_INPUT, SDL_EVENT_TEXT_INPUT, false, &second,
                                &error) == MC_CLIENT_EVENTS_OK &&
          second != NULL);
    CHECK(strcmp(mc_client_event_read(second)->text.text, "second") == 0);
    ok = true;
cleanup:
    mc_client_event_release(&key);
    mc_client_event_release(&second);
    mc_client_event_release(&first);
    if (pb != NULL && mc_client_events_retire(&pb, &error) != MC_CLIENT_EVENTS_OK)
        ok = false;
    if (pa != NULL && mc_client_events_retire(&pa, &error) != MC_CLIENT_EVENTS_OK)
        ok = false;
    if (b != NULL)
        SDL_DestroyWindow(b);
    if (a != NULL)
        SDL_DestroyWindow(a);
    if (video)
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
    return ok;
}

static bool pending_failure(bool opaque) {
    bool ok = false, video = false;
    SDL_Window *window = NULL;
    mc_client_event_port *port = NULL;
    mc_client_events_error error;
    CHECK(SDL_InitSubSystem(SDL_INIT_VIDEO));
    video = true;
    window = SDL_CreateWindow("owned pending head", 160, 120, SDL_WINDOW_HIDDEN);
    CHECK(window != NULL);
    CHECK(mc_client_events_attach(window, 0, &port, &error) == MC_CLIENT_EVENTS_OK);
    SDL_Event event = {0};
    if (opaque) {
        const Uint32 type = SDL_RegisterEvents(1);
        CHECK(type != 0);
        event.type = type;
        event.user.windowID = SDL_GetWindowID(window);
        event.user.data1 = &event;
    } else {
        const size_t bytes = 9u * 1024u * 1024u;
        char *text = SDL_AllocateTemporaryMemory(bytes);
        CHECK(text != NULL);
        memset(text, 'x', bytes - 1);
        text[bytes - 1] = '\0';
        event.type = SDL_EVENT_TEXT_INPUT;
        event.text.windowID = SDL_GetWindowID(window);
        event.text.text = text;
    }
    CHECK(SDL_PushEvent(&event));
    const mc_client_events_status expected =
        opaque ? MC_CLIENT_EVENTS_UNSUPPORTED_PAYLOAD : MC_CLIENT_EVENTS_LIMIT;
    CHECK(mc_client_events_retain_pending(port, &error) == expected);
    CHECK(error.pending_type == event.type && SDL_HasEvent(event.type));
    CHECK(mc_client_events_retain_pending(port, NULL) == expected && SDL_HasEvent(event.type));
    ok = true;
cleanup:
    if (port != NULL && mc_client_events_retire(&port, &error) != MC_CLIENT_EVENTS_OK)
        ok = false;
    if (window != NULL)
        SDL_DestroyWindow(window);
    if (video)
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
    return ok;
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    if (!lifetime() || !pending_failure(false) || !pending_failure(true))
        return 1;
    printf("SDK_TRANSPORT_OK|checks=%u\n", checks);
    return 0;
}
