/* Actual-App prepared-state contracts, authored and not executed here.
 * This does not construct Minecraft or mirror native running/captured into J.
 * Registration establishes default atomics; explicit fixture stores represent
 * source J states reached by the future constructor/run call composition. */
#include "client/application_state.h"
#include "client/gui/loading_screen.h"
#include "client/render_engine/nw.h"
#include "client/startup/application_owner.h"

#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks, cases;
#define CHECK(value)                                                                               \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(value)) {                                                                            \
            fprintf(stderr, "loading source J check %u failed at line %d\n", checks, __LINE__);    \
            result = 1;                                                                            \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

int main(void) {
    int result = 0;
    mc_client_render_engine_classes *graphics = NULL;
    mc_client_loading_classes *classes = NULL;
    mc_client_loading_screen *screen = NULL, *raw_screen = NULL, *null_screen = NULL;
    mc_player_string *text = NULL;
    client_application *app = calloc(1, sizeof(*app));
    client_application *raw = calloc(1, sizeof(*raw));
    mc_client_loading_error *error = calloc(1, sizeof(*error));
    mc_client_application_owner_error owner_error;
    mc_client_loading_state before, after;
    bool registered = false, retired = false;
    const uint16_t title[] = {'r', 'e', 'a', 'l', '-', 'J'};
    CHECK(app != NULL && raw != NULL && error != NULL);
    CHECK(mc_client_render_engine_classes_allocate_context(NULL, NULL, NULL, &graphics) ==
          MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(mc_client_loading_classes_create(graphics, &classes) == MC_CLIENT_LOADING_OK);
    CHECK(mc_player_string_create(title, sizeof(title) / sizeof(title[0]), &text) == MC_PLAYER_OK);

    app->running = true;
    app->captured = true;
    CHECK(mc_client_application_lifetime_register(app, &owner_error) ==
          MC_CLIENT_APPLICATION_OWNER_OK);
    registered = true;
    CHECK(!atomic_load_explicit(&app->J, memory_order_seq_cst) &&
          !atomic_load_explicit(&app->o, memory_order_seq_cst) && app->running && app->captured);
    CHECK(mc_client_loading_screen_allocate(classes, &screen) == MC_CLIENT_LOADING_OK);
    CHECK(mc_client_loading_screen_construct(screen, app, error) == MC_CLIENT_LOADING_OK);
    CHECK(mc_client_loading_screen_inspect(screen, &before));
    /* The old T body takes running=true and reaches qq/null Settings56.
     * Real default J=false instead throws xx at25 before the title store. */
    const mc_client_loading_status first_status =
        mc_client_loading_screen_title(screen, text, error);
    if (first_status != MC_CLIENT_LOADING_STOPPED_ERROR || error->site.pc != 25)
        fprintf(stderr, "real-J default mismatch: status=%d pc=%d running=%d J=%d\n",
                (int)first_status, error->site.pc, (int)app->running,
                (int)atomic_load_explicit(&app->J, memory_order_seq_cst));
    CHECK(first_status == MC_CLIENT_LOADING_STOPPED_ERROR);
    CHECK(error->site.pc == 25 && strcmp(error->site.method, "c") == 0 && error->child.pc == 2 &&
          strcmp(error->child.class_name, "xx") == 0);
    CHECK(mc_client_loading_screen_inspect(screen, &after) && after.fields.c == before.fields.c &&
          after.fields.d == before.fields.d && !after.fields.e && !after.active);
    CHECK(mc_client_loading_screen_progress(screen, 12, error) == MC_CLIENT_LOADING_STOPPED_ERROR &&
          error->site.pc == 25 && strcmp(error->site.method, "a") == 0 &&
          !error->sampled_clock_committed);
    CHECK(mc_client_loading_screen_message(screen, text, error) ==
              MC_CLIENT_LOADING_STOPPED_ERROR &&
          error->site.pc == 25 && strcmp(error->site.method, "d") == 0);
    CHECK(mc_client_loading_screen_inspect(screen, &after) && after.fields.a == before.fields.a &&
          after.fields.c == before.fields.c && after.fields.d == before.fields.d && app->running &&
          app->captured);
    ++cases;

    CHECK(mc_client_loading_screen_allow_stopped(screen, NULL, error) == MC_CLIENT_LOADING_OK &&
          error->stopped_allowed);
    CHECK(mc_client_loading_screen_inspect(screen, &after) && after.fields.e &&
          after.fields.c == before.fields.c && after.last_store_pc == 2);
    app->running = false;
    app->captured = false;
    /* o is a different actual source volatile field and must not affect J4. */
    atomic_store_explicit(&app->o, true, memory_order_seq_cst);
    CHECK(mc_client_loading_screen_title(screen, text, error) == MC_CLIENT_LOADING_OK &&
          error->stopped_allowed);
    CHECK(mc_client_loading_screen_inspect(screen, &after) && after.fields.c == before.fields.c &&
          !app->running && !app->captured && atomic_load_explicit(&app->o, memory_order_seq_cst));
    ++cases;

    /* Explicit source prepared state, independent of both native flags. */
    atomic_store_explicit(&app->J, true, memory_order_seq_cst);
    CHECK(mc_client_loading_screen_title(screen, text, error) == MC_CLIENT_LOADING_NULL_REFERENCE);
    CHECK(error->site.pc == 56 && error->child.pc == 20 &&
          strcmp(error->child.class_name, "qq") == 0);
    CHECK(mc_client_loading_screen_inspect(screen, &after) && after.fields.c == text &&
          after.last_store_pc == 28);
    CHECK(!app->running && !app->captured && atomic_load_explicit(&app->J, memory_order_seq_cst));
    ++cases;

    atomic_store_explicit(&app->J, false, memory_order_seq_cst);
    CHECK(mc_client_loading_screen_start(screen, text, error) == MC_CLIENT_LOADING_STOPPED_ERROR);
    CHECK(error->site.pc == 25 && error->parent.pc == 7 && strcmp(error->parent.method, "a") == 0);
    CHECK(mc_client_loading_screen_inspect(screen, &after) && !after.fields.e &&
          after.last_store_pc == 2);
    ++cases;

    int64_t actual_now = 0;
    CHECK(mc_platform_current_time_millis(&actual_now) == MC_PLATFORM_MILLIS_OK &&
          actual_now >= 20);
    atomic_store_explicit(&app->J, true, memory_order_seq_cst);
    CHECK(mc_client_loading_screen_message(screen, text, error) ==
          MC_CLIENT_LOADING_NULL_REFERENCE);
    CHECK(error->site.pc == 74 && error->child.pc == 20 && error->parent.pc == 38 &&
          error->sampled_clock_committed && strcmp(error->site.method, "a") == 0);
    CHECK(mc_client_loading_screen_inspect(screen, &after) && after.fields.a == text &&
          after.fields.d >= 20 && after.last_store_pc == 46 && !after.active);
    ++cases;

    /* No atomic access to raw->J occurs before real lifetime registration. */
    raw->running = true;
    CHECK(mc_client_loading_screen_allocate(classes, &raw_screen) == MC_CLIENT_LOADING_OK);
    CHECK(mc_client_loading_screen_construct(raw_screen, raw, error) == MC_CLIENT_LOADING_OK);
    CHECK(mc_client_loading_screen_title(raw_screen, text, error) ==
          MC_CLIENT_LOADING_INVALID_PHASE);
    CHECK(error->site.pc == 4 &&
          strcmp(error->operation, "mk App.J native registered-lifetime admission") == 0);
    CHECK(mc_client_loading_screen_inspect(raw_screen, &after) && after.last_store_pc == 30 &&
          !after.active);
    ++cases;

    /* Keep the actual borrowed App allocated while deliberately testing a
     * retired native phase. No worker hold or invented mk App refcount exists. */
    CHECK(mc_client_application_lifetime_retire(app, &owner_error) ==
          MC_CLIENT_APPLICATION_OWNER_OK);
    retired = true;
    app->running = true;
    CHECK(mc_client_loading_screen_title(screen, text, error) == MC_CLIENT_LOADING_INVALID_PHASE &&
          error->site.pc == 4);
    CHECK(mc_client_loading_screen_inspect(screen, &after) && after.fields.c == text &&
          after.last_store_pc == 46);
    ++cases;

    CHECK(mc_client_loading_screen_allocate(classes, &null_screen) == MC_CLIENT_LOADING_OK);
    CHECK(mc_client_loading_screen_construct(null_screen, NULL, error) == MC_CLIENT_LOADING_OK);
    CHECK(mc_client_loading_screen_allow_stopped(null_screen, NULL, error) ==
          MC_CLIENT_LOADING_NULL_REFERENCE);
    CHECK(error->site.pc == 4 && error->parent.pc == 10 && strcmp(error->site.method, "c") == 0);
    CHECK(mc_client_loading_screen_inspect(null_screen, &after) && after.fields.e &&
          after.last_store_pc == 2);
    ++cases;

cleanup:
    if (null_screen != NULL) {
        if (mc_client_loading_screen_release(null_screen) != MC_CLIENT_LOADING_OK)
            result = 1;
        else
            null_screen = NULL;
    }
    if (raw_screen != NULL) {
        if (mc_client_loading_screen_release(raw_screen) != MC_CLIENT_LOADING_OK)
            result = 1;
        else
            raw_screen = NULL;
    }
    if (screen != NULL) {
        if (mc_client_loading_screen_release(screen) != MC_CLIENT_LOADING_OK)
            result = 1;
        else
            screen = NULL;
    }
    if (registered && !retired &&
        mc_client_application_lifetime_retire(app, &owner_error) != MC_CLIENT_APPLICATION_OWNER_OK)
        result = 1;
    mc_player_string_release(text);
    if (classes != NULL && mc_client_loading_classes_release(classes) != MC_CLIENT_LOADING_OK)
        result = 1;
    if (graphics != NULL &&
        mc_client_render_engine_classes_release(graphics) != MC_CLIENT_RENDER_ENGINE_OK)
        result = 1;
    free(error);
    /* Keep a failed native borrow live instead of freeing App under it. A
     * retained failure is already a failing test, not successful cleanup. */
    if (raw_screen == NULL)
        free(raw);
    if (screen == NULL)
        free(app);
    if (result == 0 &&
        printf("Loading source J: %u checks;%u prepared-state cases\n", checks, cases) < 0)
        result = 1;
    return result;
}
