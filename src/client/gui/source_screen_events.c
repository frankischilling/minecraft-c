#include "source_screen_events.h"
#include "../application_state.h"
#include "../display/source_surface.h"
#include "../display/staged.h"
#include "../startup/application_startup.h"
#include "mc/core/java/java.h"
#include "source_screen_internal.h"
#include "source_screen_transitions.h"

#include <fenv.h>
#include <float.h>
#include <limits.h>

static const char mouse_desc[] = "(III)V";

static mc_client_screen_status fail(mc_client_screen_event_error *e, mc_client_screen_status status,
                                    const char *operation, const char *owner, const char *method,
                                    const char *descriptor, unsigned int pc, bool native) {
    mc_screen_fail(&e->screen, status, operation, owner, method, descriptor, pc);
    e->screen.native_policy = native;
    return status;
}

static bool admit(mc_client_source_da *o, mc_client_screen_event_error *e) {
    if (o == NULL) {
        fail(e, MC_CLIENT_SCREEN_INVALID_ARGUMENT, "screen event receiver", NULL, NULL, NULL, 0,
             true);
        return false;
    }
    if (!mc_screen_lock(o->classes, &e->screen))
        return false;
    if (o->classes->retiring || o->phase != MC_CLIENT_SCREEN_READY || o->selected_cleanup != NULL ||
        o->active_calls != 0) {
        fail(e, MC_CLIENT_SCREEN_BUSY, "screen active/retiring event admission", NULL, NULL, NULL,
             0, true);
        (void)mtx_unlock(&o->classes->mutex);
        return false;
    }
    ++o->active_calls;
    o->transitions = NULL;
    e->pending_receiver = o;
    return true;
}

static bool transition(mc_client_source_da *o, mc_client_source_ke *button,
                       mc_client_screen_event_report *r, mc_client_screen_event_error *e) {
    mc_client_screen_transitions *router = o->transitions;
    mc_client_screen_classes *classes = o->classes;
    (void)mtx_unlock(&classes->mutex);
    const mc_client_screen_status status =
        mc_client_screen_transitions_button(router, o, button, &e->screen);
    if (mtx_lock(&classes->mutex) != thrd_success) {
        e->monitor_lost = true;
        fail(e, MC_CLIENT_SCREEN_THREAD_FAILED, "screen source-call monitor reacquisition", NULL,
             NULL, NULL, 0, true);
        return false;
    }
    if (status == MC_CLIENT_SCREEN_OK)
        r->pending = (mc_client_screen_site){0};
    return status == MC_CLIENT_SCREEN_OK;
}

static bool button_hit(mc_client_source_ke *button, int32_t x, int32_t y, bool *out,
                       mc_client_screen_event_error *e) {
    if (button == NULL) {
        fail(e, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE, "actual ke.c null receiver", "da", "a",
             mouse_desc, 45, false);
        return false;
    }
    if (button->phase != MC_CLIENT_SCREEN_READY) {
        fail(e, MC_CLIENT_SCREEN_BUSY, "ke.c native live receiver", "ke", "c",
             "(Lnet/minecraft/client/Minecraft;II)Z", 0, true);
        return false;
    }
    const mc_client_screen_ke_fields *f = &button->fields;
    *out = f->g && x >= f->c && y >= f->d && x < mc_java_i32_add(f->c, f->a) &&
           y < mc_java_i32_add(f->d, f->b);
    return true;
}

mc_client_screen_status mc_client_screen_button_hit(mc_client_source_ke *button, int32_t x,
                                                    int32_t y, bool *out,
                                                    mc_client_screen_error *error) {
    mc_client_screen_event_error e = {0};
    if (button == NULL || out == NULL)
        fail(&e, MC_CLIENT_SCREEN_INVALID_ARGUMENT, "ke.c native outputs", NULL, NULL, NULL, 0,
             true);
    else if (mc_screen_lock(button->classes, &e.screen)) {
        bool result = false;
        if (button_hit(button, x, y, &result, &e)) {
            *out = result;
            e.screen.source_returned = true;
        }
        (void)mtx_unlock(&button->classes->mutex);
    }
    return mc_screen_publish(&e.screen, error);
}

static bool action(mc_client_source_da *o, mc_client_source_ke *button,
                   mc_client_screen_event_report *r, mc_client_screen_event_error *e) {
    if (o->kind == MC_CLIENT_SCREEN_WORLD_SELECTION) {
        r->button = button;
        r->button_id = button->fields.f;
        ++r->actions;
        r->action = button->fields.f == 0   ? MC_CLIENT_SCREEN_ACTION_CANCEL
                    : button->fields.f == 1 ? MC_CLIENT_SCREEN_ACTION_SELECT_WORLD
                    : button->fields.f == 2 ? MC_CLIENT_SCREEN_ACTION_DELETE_WORLD
                    : button->fields.f == 3 ? MC_CLIENT_SCREEN_ACTION_CREATE_WORLD
                    : button->fields.f == 6 ? MC_CLIENT_SCREEN_ACTION_RENAME_WORLD
                                            : MC_CLIENT_SCREEN_ACTION_NONE;
        if (o->transitions != NULL)
            return transition(o, button, r, e);
        fail(e, MC_CLIENT_SCREEN_UNIMPLEMENTED_DEPENDENCY,
             "RQ requires actual screen transition ownership", "rq", "a", "(Lke;)V", 0, true);
        return false;
    }
    if (!(o->kind == MC_CLIENT_SCREEN_MAIN_MENU))
        return true; /* Actual da.a(ke) is return0. */
    const int32_t id = button->fields.f;
    unsigned int pc = 0;
    switch (id) {
        case 0:
            r->action = MC_CLIENT_SCREEN_ACTION_OPTIONS;
            pc = 11;
            break;
        case 1:
            r->action = MC_CLIENT_SCREEN_ACTION_SINGLEPLAYER;
            pc = 41;
            break;
        case 2:
            r->action = MC_CLIENT_SCREEN_ACTION_MULTIPLAYER;
            pc = 64;
            break;
        case 3:
            r->action = MC_CLIENT_SCREEN_ACTION_TEXTURE_PACKS;
            pc = 87;
            break;
        case 4:
            r->action = MC_CLIENT_SCREEN_ACTION_QUIT;
            pc = 110;
            break;
        default:
            return true;
    }
    r->button = button;
    r->button_id = id;
    ++r->actions;
    r->pending = (mc_client_screen_site){"fu", "a", "(Lke;)V", pc};
    if (id == 1 && o->transitions != NULL)
        return transition(o, button, r, e);
    if (id != 4) {
        fail(e, MC_CLIENT_SCREEN_UNIMPLEMENTED_DEPENDENCY,
             id == 0   ? "actual options co allocation"
             : id == 1 ? "actual world-selection rq allocation"
             : id == 2 ? "actual multiplayer lq allocation"
                       : "actual texture-pack ft allocation",
             "fu", "a", "(Lke;)V", pc, true);
        e->screen.parent_site = (mc_client_screen_site){"da", "a", mouse_desc, 74};
        return false;
    }
    if (o->fields.b == NULL) {
        fail(e, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE, "fu quit null App receiver", "fu", "a",
             "(Lke;)V", 110, false);
        return false;
    }
    e->application_status = (int)mc_client_application_source_stop(o->fields.b, NULL);
    if (e->application_status != (int)MC_CLIENT_STARTUP_OK) {
        fail(e, MC_CLIENT_SCREEN_APP_FAILED, "actual Minecraft.f stop child", "fu", "a", "(Lke;)V",
             110, true);
        return false;
    }
    r->quit_returned = true;
    r->pending = (mc_client_screen_site){0};
    return true;
}

static bool press(mc_client_source_da *o, int32_t x, int32_t y, int32_t which,
                  mc_client_screen_event_report *r, mc_client_screen_event_error *e) {
    if (which != 0)
        return true;
    ++r->presses;
    mc_client_screen_list *list = (mc_client_screen_list *)o->fields.e;
    if (list == NULL) {
        fail(e, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE, "da press null List.size receiver", "da",
             "a", mouse_desc, 13, false);
        return false;
    }
    for (size_t index = 0; index < list->size; ++index) {
        mc_client_source_ke *button = list->entries[index];
        bool hit = false;
        if (!button_hit(button, x, y, &hit, e))
            return false;
        if (!hit)
            continue;
        if (!mc_screen_button_hold_locked(button, &e->screen))
            return false;
        o->selected_cleanup = o->fields.a;
        o->fields.a = button; /* Actual source selected button store54. */
        if (!mc_screen_button_release_locked(&o->selected_cleanup, &e->screen))
            return false;
        if (o->fields.b == NULL || o->fields.b->B == NULL) {
            fail(e, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE, "da click null sound receiver", "da",
                 "a", mouse_desc, o->fields.b == NULL ? 61u : 68u, false);
            return false;
        }
        static const uint16_t click[] = {'r', 'a', 'n', 'd', 'o', 'm',
                                         '.', 'c', 'l', 'i', 'c', 'k'};
        const mc_sound_status sound = mc_sound_manager_ui(
            o->fields.b->B, (mc_sound_text){click, sizeof(click) / sizeof(click[0]), false}, 1.0f,
            1.0f, &e->sound_report, &e->sound);
        if (sound != MC_SOUND_OK) {
            fail(e, MC_CLIENT_SCREEN_SOUND_FAILED, "da actual random.click sound child", "da", "a",
                 mouse_desc, 68, sound != MC_SOUND_SOURCE_EXCEPTION);
            return false;
        }
        ++r->click_calls;
        if (!action(o, button, r, e))
            return false;
    }
    return true;
}

static bool release_button(mc_client_source_da *o, int32_t which, mc_client_screen_event_report *r,
                           mc_client_screen_event_error *e) {
    if (o->fields.a == NULL || which != 0)
        return true;
    /* Actual ke.a(II) is return0; it reads neither coordinates nor fields. */
    ++r->releases;
    o->selected_cleanup = o->fields.a;
    o->fields.a = NULL; /* Actual da.b(III) putfield22, after ke.a17 returns. */
    return mc_screen_button_release_locked(&o->selected_cleanup, &e->screen);
}

static bool mouse(mc_client_source_da *o, const mc_client_sdk_mouse_event *event,
                  mc_client_screen_event_report *r, mc_client_screen_event_error *e) {
    const bool down = event->pressed;
    if (o->fields.b == NULL) {
        fail(e, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE, "da mouse null App width receiver", "da",
             "f", "()V", down ? 18u : 71u, false);
        return false;
    }
    int32_t x = 0, y = 0;
    if (!mc_java_i32_div(mc_java_i32_mul(event->x, o->fields.c), o->fields.b->d, &x)) {
        fail(e, MC_CLIENT_SCREEN_SOURCE_ARITHMETIC, "da mouse width division by zero", "da", "f",
             "()V", down ? 21u : 74u, false);
        return false;
    }
    if (!mc_java_i32_div(mc_java_i32_mul(event->y, o->fields.d), o->fields.b->e, &y)) {
        fail(e, MC_CLIENT_SCREEN_SOURCE_ARITHMETIC, "da mouse height division by zero", "da", "f",
             "()V", down ? 42u : 95u, false);
        return false;
    }
    y = mc_java_i32_sub(mc_java_i32_sub(o->fields.d, y), 1);
    r->mouse_x = x;
    r->mouse_y = y;
    return down ? press(o, x, y, event->button, r, e) : release_button(o, event->button, r, e);
}

static bool key(mc_client_source_da *o, const mc_client_sdk_key_event *event,
                mc_client_screen_event_report *r, mc_client_screen_event_error *e) {
    r->key = event->key;
    r->character =
        (uint16_t)event->character; /* Actual Keyboard.getEventCharacter char narrowing. */
    if (!event->pressed)
        return true;
    if (event->key == 87) {
        r->action = MC_CLIENT_SCREEN_ACTION_FULLSCREEN;
        r->pending = (mc_client_screen_site){"da", "g", "()V", 18};
        fail(e, MC_CLIENT_SCREEN_UNIMPLEMENTED_DEPENDENCY, "actual Minecraft.j fullscreen child",
             "da", "g", "()V", 18, true);
        return false;
    }
    if (!(o->kind == MC_CLIENT_SCREEN_MAIN_MENU) && event->key == 1) {
        r->action = MC_CLIENT_SCREEN_ACTION_CLOSE;
        r->pending = (mc_client_screen_site){"da", "a", "(CI)V", 10};
        fail(e, MC_CLIENT_SCREEN_UNIMPLEMENTED_DEPENDENCY,
             "actual base-screen close and focus transition", "da", "a", "(CI)V", 10, true);
        return false;
    }
    return true; /* fu.a(CI)V is return0; da ignores non-Escape keys. */
}

static mc_client_screen_status finish(mc_client_source_da *o, bool returned,
                                      mc_client_screen_event_report *r,
                                      mc_client_screen_event_error *e) {
    if (e->monitor_lost)
        return e->screen.status;
    if (returned)
        e->screen.source_returned = r->source_returned = true;
    --o->active_calls;
    o->transitions = NULL;
    e->pending_receiver = NULL;
    (void)mtx_unlock(&o->classes->mutex);
    return e->screen.status;
}

mc_client_screen_status mc_client_screen_mouse_event(mc_client_source_da *o,
                                                     const mc_client_sdk_mouse_event *event,
                                                     mc_client_screen_event_report *r,
                                                     mc_client_screen_event_error *e) {
    if (e == NULL || r == NULL || event == NULL)
        return MC_CLIENT_SCREEN_INVALID_ARGUMENT;
    *e = (mc_client_screen_event_error){0};
    *r = (mc_client_screen_event_report){0};
    if (!admit(o, e))
        return e->screen.status;
    r->mouse_events = 1;
    return finish(o, mouse(o, event, r, e), r, e);
}

mc_client_screen_status mc_client_screen_key_event(mc_client_source_da *o,
                                                   const mc_client_sdk_key_event *event,
                                                   mc_client_screen_event_report *r,
                                                   mc_client_screen_event_error *e) {
    if (e == NULL || r == NULL || event == NULL)
        return MC_CLIENT_SCREEN_INVALID_ARGUMENT;
    *e = (mc_client_screen_event_error){0};
    *r = (mc_client_screen_event_report){0};
    if (!admit(o, e))
        return e->screen.status;
    r->keyboard_events = 1;
    return finish(o, key(o, event, r, e), r, e);
}

mc_client_screen_status mc_client_screen_handle_input_with_transitions(
    mc_client_source_da *o, mc_client_sdk_input *input, mc_client_screen_transitions *router,
    mc_client_screen_event_report *r, mc_client_screen_event_error *e) {
    if (e == NULL || r == NULL)
        return MC_CLIENT_SCREEN_INVALID_ARGUMENT;
    *e = (mc_client_screen_event_error){0};
    *r = (mc_client_screen_event_report){0};
    if (!admit(o, e))
        return e->screen.status;
    o->transitions = router;
    bool returned = false;
    for (;;) {
        bool next = false;
        mc_client_sdk_mouse_event event;
        if (mc_client_sdk_mouse_next(input, &next, &event, &e->input) != MC_CLIENT_SDK_INPUT_OK) {
            fail(e, MC_CLIENT_SCREEN_INPUT_FAILED, "da actual Mouse.next child", "da", "e", "()V",
                 0, true);
            goto done;
        }
        if (!next)
            break;
        ++r->mouse_events;
        if (!mouse(o, &event, r, e))
            goto done;
    }
    for (;;) {
        bool next = false;
        mc_client_sdk_key_event event;
        if (mc_client_sdk_keyboard_next(input, &next, &event, &e->input) !=
            MC_CLIENT_SDK_INPUT_OK) {
            fail(e, MC_CLIENT_SCREEN_INPUT_FAILED, "da actual Keyboard.next child", "da", "e",
                 "()V", 13, true);
            goto done;
        }
        if (!next)
            break;
        ++r->keyboard_events;
        if (!key(o, &event, r, e))
            goto done;
    }
    returned = true;
done:
    return finish(o, returned, r, e);
}

mc_client_screen_status mc_client_screen_handle_input(mc_client_source_da *o,
                                                      mc_client_sdk_input *input,
                                                      mc_client_screen_event_report *r,
                                                      mc_client_screen_event_error *e) {
    return mc_client_screen_handle_input_with_transitions(o, input, NULL, r, e);
}

mc_client_screen_status mc_client_screen_event_retire(mc_client_screen_event_error *e) {
    if (e == NULL)
        return MC_CLIENT_SCREEN_INVALID_ARGUMENT;
    mc_client_source_da *o = e->pending_receiver;
    if (o == NULL)
        return MC_CLIENT_SCREEN_OK;
    if (!e->monitor_lost)
        return MC_CLIENT_SCREEN_BUSY; /* The source invocation has not returned yet. */
    mc_client_screen_error lock_error = {0};
    if (!mc_screen_lock(o->classes, &lock_error))
        return lock_error.status;
    --o->active_calls;
    o->transitions = NULL;
    e->pending_receiver = NULL;
    e->monitor_lost = false;
    (void)mtx_unlock(&o->classes->mutex);
    return MC_CLIENT_SCREEN_OK;
}

mc_client_screen_status mc_client_screen_tick(mc_client_source_da *o,
                                              mc_client_screen_event_error *e) {
    if (e == NULL)
        return MC_CLIENT_SCREEN_INVALID_ARGUMENT;
    *e = (mc_client_screen_event_error){0};
    if (!admit(o, e))
        return e->screen.status;
    if (o->kind == MC_CLIENT_SCREEN_MAIN_MENU) {
        if (FLT_RADIX != 2 || FLT_MANT_DIG != 24 || sizeof(float) != 4 ||
            fegetround() != FE_TONEAREST)
            fail(e, MC_CLIENT_SCREEN_UNIMPLEMENTED_DEPENDENCY, "fu binary32 arithmetic profile",
                 "fu", "a", "()V", 6, true);
        else {
            mc_client_source_fu *menu = (mc_client_source_fu *)o;
            volatile float next = menu->fields.i + 1.0f;
            menu->fields.i = next; /* Actual fu.a putfield7. */
            e->screen.source_returned = true;
        }
    } else
        e->screen.source_returned = true;
    --o->active_calls;
    e->pending_receiver = NULL;
    (void)mtx_unlock(&o->classes->mutex);
    return e->screen.status;
}

mc_client_screen_status mc_client_screen_particles_tick(mc_client_source_du *o,
                                                        mc_client_screen_event_error *e) {
    if (e == NULL)
        return MC_CLIENT_SCREEN_INVALID_ARGUMENT;
    *e = (mc_client_screen_event_error){0};
    if (o == NULL)
        return fail(e, MC_CLIENT_SCREEN_INVALID_ARGUMENT, "du tick receiver", NULL, NULL, NULL, 0,
                    true);
    if (!mc_screen_lock(o->classes, &e->screen))
        return e->screen.status;
    if (o->phase != MC_CLIENT_SCREEN_READY || o->classes->retiring)
        fail(e, MC_CLIENT_SCREEN_BUSY, "du tick active/retiring owner", NULL, NULL, NULL, 0, true);
    else if (o->a == NULL)
        fail(e, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE, "du tick null List.size receiver", "du",
             "a", "()V", 7, false);
    else if (o->a->size != 0)
        fail(e,
             o->a->entries[0] == NULL ? MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE
                                      : MC_CLIENT_SCREEN_UNIMPLEMENTED_DEPENDENCY,
             "actual first kw.a tick child", "du", "a", "()V", 30, o->a->entries[0] != NULL);
    else
        e->screen.source_returned = true;
    (void)mtx_unlock(&o->classes->mutex);
    return e->screen.status;
}

mc_client_screen_status mc_client_screen_particles_draw(mc_client_source_du *o, float partial_tick,
                                                        mc_client_screen_event_error *e) {
    (void)partial_tick; /* Empty source loop does not evaluate kw interpolation. */
    if (e == NULL)
        return MC_CLIENT_SCREEN_INVALID_ARGUMENT;
    *e = (mc_client_screen_event_error){0};
    if (o == NULL)
        return fail(e, MC_CLIENT_SCREEN_INVALID_ARGUMENT, "du draw receiver", NULL, NULL, NULL, 0,
                    true);
    if (!mc_screen_lock(o->classes, &e->screen))
        return e->screen.status;
    if (o->phase != MC_CLIENT_SCREEN_READY || o->classes->retiring) {
        fail(e, MC_CLIENT_SCREEN_BUSY, "du draw active/retiring owner", NULL, NULL, NULL, 0, true);
        goto done;
    }
    if (o->b == NULL) {
        fail(e, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE, "du draw null App.p receiver", "du", "a",
             "(F)V", 4, false);
        goto done;
    }
    mc_client_render_engine *bind_engine = o->b->render_engine;   /* Actual first p4 read. */
    mc_client_render_engine *lookup_engine = o->b->render_engine; /* Independent p11 read. */
    mc_player_string *literal = NULL;
    if (!mc_screen_literal_locked(o->classes, "/gui/particles.png", &literal, &e->screen))
        goto done;
    if (lookup_engine == NULL) {
        fail(e, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE, "du draw null texture lookup receiver",
             "du", "a", "(F)V", 16, false);
        goto done;
    }
    mc_player_string_view key;
    if (!mc_player_string_read(literal, &key)) {
        fail(e, MC_CLIENT_SCREEN_STRING_FAILED, "du particle texture literal view", "du", "a",
             "(F)V", 14, true);
        goto done;
    }
    mc_client_texture id = 0;
    const mc_client_texture_lookup_status lookup = mc_client_render_engine_texture(
        lookup_engine, o->b->display, NULL, (mc_texture_pack_text){key.units, key.length, false},
        &id, &e->lookup);
    if (lookup != MC_CLIENT_LOOKUP_OK) {
        fail(e,
             lookup == MC_CLIENT_LOOKUP_UNIMPLEMENTED ? MC_CLIENT_SCREEN_UNIMPLEMENTED_DEPENDENCY
                                                      : MC_CLIENT_SCREEN_TEXTURE_FAILED,
             "du actual ji.b(String) child", "du", "a", "(F)V", 16, e->lookup.native_policy);
        goto done;
    }
    if (bind_engine == NULL) {
        fail(e, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE, "du draw null numeric bind receiver", "du",
             "a", "(F)V", 19, false);
        goto done;
    }
    if (id <= (uint32_t)INT32_MAX &&
        !mc_client_display_source_bind_id(o->b->display, id, &e->display)) {
        fail(e, MC_CLIENT_SCREEN_TEXTURE_FAILED, "du actual ji.b(int) GL bind", "du", "a", "(F)V",
             19, true);
        goto done;
    }
    if (o->a == NULL)
        fail(e, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE, "du draw null List.size receiver", "du",
             "a", "(F)V", 29, false);
    else if (o->a->size != 0)
        fail(e,
             o->a->entries[0] == NULL ? MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE
                                      : MC_CLIENT_SCREEN_UNIMPLEMENTED_DEPENDENCY,
             "actual first kw interpolation fields", "du", "a", "(F)V", 52,
             o->a->entries[0] != NULL);
    else
        e->screen.source_returned = true;
done:
    (void)mtx_unlock(&o->classes->mutex);
    return e->screen.status;
}
