#include "../application_access.h"
#include "../application_state.h"
#include "source_screen_initialize_internal.h"
#include "source_world_selection_internal.h"

#include <limits.h>
#include <string.h>

static const char da_desc[] = "(Lnet/minecraft/client/Minecraft;II)V";

static mc_client_screen_site site(bool da, unsigned int pc) {
    return (mc_client_screen_site){da ? "da" : "fu", da ? "a" : "b", da ? da_desc : "()V", pc};
}

static bool failure(mc_client_screen_initialization *r, mc_client_screen_status status,
                    const char *operation, bool da, unsigned int pc, bool native) {
    mc_screen_fail(&r->failure.screen, status, operation, da ? "da" : "fu", da ? "a" : "b",
                   da ? da_desc : "()V", pc);
    r->failure.screen.native_policy = native;
    if (da)
        r->da_pc = pc;
    else
        r->fu_pc = pc;
    return false;
}

static void store(mc_client_screen_initialization *r, bool da, unsigned int pc) {
    r->stores[r->store_count++] = site(da, pc);
    if (da)
        r->da_pc = pc;
    else
        r->fu_pc = pc;
}

static bool calendar_ok(mc_client_screen_initialization *r, mc_platform_calendar_status status,
                        unsigned int pc) {
    r->fu_pc = pc;
    if (status == MC_PLATFORM_CALENDAR_OK)
        return true;
    return failure(r, MC_CLIENT_SCREEN_CALENDAR_FAILED, "fu actual Calendar/Date child", false, pc,
                   true);
}

static bool literal(mc_client_source_da *o, mc_client_screen_initialization *r, const char *ascii,
                    unsigned int pc, mc_player_string **out) {
    r->fu_pc = pc;
    if (mc_screen_literal_locked(o->classes, ascii, out, &r->failure.screen))
        return true;
    r->failure.screen.site = site(false, pc);
    return false;
}

static bool calendar_and_holiday(mc_client_source_fu *menu, mc_client_screen_initialization *r) {
    if (!calendar_ok(r, mc_platform_calendar_default_create(&r->calendar, &r->failure.calendar), 0))
        return false;
    r->fu_pc = 5;
    r->date_allocated = true;
    if (!calendar_ok(r, mc_platform_date_now(&r->date, &r->failure.calendar), 9))
        return false;
    if (!calendar_ok(r, mc_platform_calendar_set_time(r->calendar, &r->date, &r->failure.calendar),
                     12))
        return false;
    static const unsigned int month_pc[] = {17, 48, 78, 109}, day_pc[] = {29, 60, 90, 120},
                              literal_pc[] = {38, 68, 99, 128}, store_pc[] = {40, 70, 101, 130};
    static const int32_t month_value[] = {11, 6, 12, 1}, day_value[] = {9, 1, 24, 1};
    static const char *const text[] = {"Happy birthday, ez!", "Happy birthday, Notch!",
                                       "Merry X-mas!", "Happy new year!"};
    for (size_t i = 0; i < 4; ++i) {
        int32_t month = 0, day = 0;
        if (!calendar_ok(r, mc_platform_calendar_get(r->calendar, 2, &month, &r->failure.calendar),
                         month_pc[i]))
            return false;
        if (mc_java_i32_add(month, 1) != month_value[i])
            continue;
        if (!calendar_ok(r, mc_platform_calendar_get(r->calendar, 5, &day, &r->failure.calendar),
                         day_pc[i]))
            return false;
        if (day != day_value[i])
            continue;
        mc_player_string *value = NULL;
        if (!literal(&menu->base, r, text[i], literal_pc[i], &value))
            return false;
        menu->fields.j = value;
        store(r, false, store_pc[i]);
        break;
    }
    return true;
}

static bool label(mc_client_source_da *o, mc_client_screen_initialization *r, const char *key,
                  unsigned int literal_pc, unsigned int invoke_pc, mc_client_screen_text *out) {
    mc_player_string *actual_key = NULL;
    if (!literal(o, r, key, literal_pc, &actual_key))
        return false;
    r->fu_pc = invoke_pc;
    if (r->translation == NULL)
        return failure(r, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE, "fu actual nh.a String receiver",
                       false, invoke_pc, false);
    mc_player_string_view key_view = {0};
    if (!mc_player_string_read(actual_key, &key_view))
        return failure(r, MC_CLIENT_SCREEN_STRING_FAILED, "fu canonical key String view", false,
                       invoke_pc, true);
    const mc_registry_string *value = NULL;
    r->failure.screen.registry.status = mc_translation_property(
        r->translation, key_view.units, key_view.length, &value, &r->failure.screen.registry);
    if (r->failure.screen.registry.status != MC_REGISTRY_OK)
        return failure(r, MC_CLIENT_SCREEN_REGISTRY_FAILED, "fu actual nh property lookup", false,
                       invoke_pc, true);
    *out = value == NULL ? (mc_client_screen_text){MC_CLIENT_SCREEN_TEXT_PLAYER, actual_key, NULL}
                         : (mc_client_screen_text){MC_CLIENT_SCREEN_TEXT_REGISTRY, NULL, value};
    return true;
}

static bool list_add(mc_client_source_da *o, mc_client_screen_initialization *r,
                     mc_client_screen_list *list, mc_client_source_ke *button, unsigned int pc) {
    r->fu_pc = pc;
    if (list == NULL)
        return failure(r, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE, "fu actual List.add receiver",
                       false, pc, false);
    /* JDK add(E) putfield modCount7 precedes array/size13/17 and helper20. */
    list->mod_count = mc_java_i32_add(list->mod_count, 1);
    if (list->size >= (size_t)INT32_MAX)
        return failure(r, MC_CLIENT_SCREEN_LIMIT, "native ArrayList size limit after modCount",
                       false, pc, true);
    if (list->size == list->capacity) {
        size_t capacity = list->capacity == 0 ? 10u : list->capacity + list->capacity / 2u;
        if (capacity <= list->size)
            capacity = list->size + 1u;
        if (capacity > (size_t)INT32_MAX || capacity > SIZE_MAX / sizeof(*list->entries))
            return failure(r, MC_CLIENT_SCREEN_LIMIT,
                           "native ArrayList growth range after modCount", false, pc, true);
        void **entries = mc_screen_allocate(&o->memory, capacity * sizeof(*entries));
        if (entries == NULL) {
            failure(r, o->memory.allocation_status, "actual ArrayList native growth", false, pc,
                    true);
            r->failure.caller = site(false, pc);
            r->failure.screen.site = (mc_client_screen_site){
                "java/util/ArrayList", "add", "(Ljava/lang/Object;[Ljava/lang/Object;I)V", 7};
            r->failure.screen.parent_site =
                (mc_client_screen_site){"java/util/ArrayList", "add", "(Ljava/lang/Object;)Z", 20};
            return false;
        }
        for (size_t i = 0; i < list->size; ++i)
            entries[i] = list->entries[i];
        list->entries = entries;
        list->capacity = capacity;
    }
    if (!mc_screen_button_hold_locked(button, &r->failure.screen)) {
        r->failure.caller = site(false, pc);
        return false;
    }
    list->entries[list->size] = button; /* private add aastore14 before size19 */
    ++list->size;
    return true; /* Actual ArrayList.add returns true23/24; source caller pops it. */
}

static bool add_button(mc_client_source_fu *menu, mc_client_screen_initialization *r,
                       unsigned int list_pc, unsigned int allocation_pc, unsigned int width_pc,
                       int32_t id, int32_t x_offset, int32_t y_offset, int32_t width,
                       bool default_size, const char *key, unsigned int literal_pc,
                       unsigned int lookup_pc, unsigned int constructor_pc, unsigned int add_pc,
                       bool publish_l) {
    mc_client_source_da *o = &menu->base;
    r->fu_pc = list_pc;
    mc_client_screen_list *list = (mc_client_screen_list *)o->fields.e; /* held outer receiver */
    r->fu_pc = allocation_pc;
    mc_client_source_ke *button = mc_screen_button_allocate_locked(o->classes, &r->failure.screen);
    if (button == NULL) {
        r->failure.caller = site(false, allocation_pc);
        return false;
    }
    r->buttons[r->button_count++] = button;
    r->fu_pc = width_pc;
    int32_t half = 0;
    if (!mc_java_i32_div(o->fields.c, 2, &half))
        return failure(r, MC_CLIENT_SCREEN_INVALID_ARGUMENT, "native fixed-divisor width kernel",
                       false, width_pc, true); /* live c, not cached App dimensions */
    const int32_t x = mc_java_i32_add(half, x_offset), y = mc_java_i32_add(r->row_origin, y_offset);
    mc_client_screen_text text;
    if (!label(o, r, key, literal_pc, lookup_pc, &text))
        return false;
    r->fu_pc = constructor_pc;
    if (!mc_screen_button_construct_locked(button, id, x, y, width, 20, text, &r->failure.screen)) {
        if (default_size)
            r->failure.screen.parent_site =
                (mc_client_screen_site){"ke", "<init>", "(IIILjava/lang/String;)V", 11};
        r->failure.caller = site(false, constructor_pc);
        return false;
    }
    if (publish_l) {
        r->fu_pc = 217;
        if (!mc_screen_button_hold_locked(button, &r->failure.screen)) {
            r->failure.caller = site(false, 217);
            return false;
        }
        r->replaced_l = menu->fields.l; /* move its old independent native occurrence */
        menu->fields.l = button;
        store(r, false, 217);
    }
    return list_add(o, r, list, button, add_pc);
}

static bool menu_body(mc_client_source_fu *menu, mc_client_screen_initialization *r) {
    mc_client_source_da *o = &menu->base;
    if (!calendar_and_holiday(menu, r))
        return false;
    r->fu_pc = 133;
    if (o->classes->registry == NULL)
        return failure(r, MC_CLIENT_SCREEN_UNIMPLEMENTED_DEPENDENCY,
                       "fu canonical nh class namespace missing", false, 133, true);
    r->failure.screen.registry.status = mc_registry_classes_initialize_translation_singleton(
        o->classes->registry, &r->failure.screen.registry);
    if (r->failure.screen.registry.status != MC_REGISTRY_OK) {
        const mc_registry_status status = r->failure.screen.registry.status;
        return failure(
            r, MC_CLIENT_SCREEN_REGISTRY_FAILED, "fu actual nh.a singleton active use", false, 133,
            status != MC_REGISTRY_NULL_REFERENCE && status != MC_REGISTRY_ILLEGAL_ARGUMENT &&
                status != MC_REGISTRY_INDEX_OUT_OF_RANGE);
    }
    mc_registry_translation_view translation = {0};
    r->failure.screen.registry.status = mc_registry_classes_inspect_translation(
        o->classes->registry, &translation, &r->failure.screen.registry);
    if (r->failure.screen.registry.status != MC_REGISTRY_OK)
        return failure(r, MC_CLIENT_SCREEN_REGISTRY_FAILED, "fu actual nh singleton field read",
                       false, 133, true);
    r->translation = translation.singleton_a;
    r->fu_pc = 138;
    int32_t quarter = 0;
    if (!mc_java_i32_div(o->fields.d, 4, &quarter))
        return failure(r, MC_CLIENT_SCREEN_INVALID_ARGUMENT, "native fixed-divisor row kernel",
                       false, 142, true);
    r->row_origin = mc_java_i32_add(quarter, 48);
    if (!add_button(menu, r, 149, 152, 158, 1, -100, 0, 200, true, "menu.singleplayer", 169, 171,
                    174, 177, false))
        return false;
    if (!add_button(menu, r, 184, 188, 194, 2, -100, 24, 200, true, "menu.multiplayer", 208, 210,
                    213, 220, true))
        return false;
    if (!add_button(menu, r, 227, 230, 236, 3, -100, 48, 200, true, "menu.mods", 250, 252, 255, 258,
                    false))
        return false;
    r->fu_pc = 265;
    client_application *applet_receiver = o->fields.b;
    r->fu_pc = 268;
    if (applet_receiver == NULL)
        return failure(r, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE, "fu actual App.n receiver", false,
                       268, false);
    r->applet_branch = applet_receiver->n; /* actual late getfield268 */
    if (r->applet_branch) {
        if (!add_button(menu, r, 275, 278, 284, 0, -100, 72, 200, true, "menu.options", 298, 300,
                        303, 306, false))
            return false;
    } else {
        if (!add_button(menu, r, 316, 319, 325, 0, -100, 84, 98, false, "menu.options", 346, 348,
                        351, 354, false))
            return false;
        if (!add_button(menu, r, 361, 364, 370, 4, 2, 84, 98, false, "menu.quit", 390, 392, 395,
                        398, false))
            return false;
    }
    r->fu_pc = 405;
    client_application *session_receiver = o->fields.b;
    r->fu_pc = 408;
    if (session_receiver == NULL)
        return failure(r, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE, "fu actual App.k receiver", false,
                       408, false);
    r->session_present = session_receiver->session != NULL;
    if (!r->session_present) {
        r->fu_pc = 415;
        mc_client_source_ke *button = menu->fields.l;
        r->fu_pc = 419;
        if (button == NULL)
            return failure(r, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE,
                           "fu actual multiplayer Button.g receiver", false, 419, false);
        button->fields.g = false;
        store(r, false, 419);
    }
    r->fu_pc = 422;
    return true;
}

static bool list_clear(mc_client_source_da *o, mc_client_screen_initialization *r) {
    r->da_pc = 39;
    mc_client_screen_list *list = (mc_client_screen_list *)o->fields.e;
    if (list == NULL)
        return failure(r, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE, "da actual List.clear receiver",
                       true, 39, false);
    /* Every occupied e element has a separate retained construction record.
     * This preflight rejects only corrupted/foreign native representations. */
    for (size_t i = 0; i < list->size; ++i) {
        mc_client_source_ke *button = list->entries[i];
        if (button != NULL && (button->classes != o->classes || button->holds < 2))
            return failure(r, MC_CLIENT_SCREEN_INVALID_PHASE, "da native List occurrence invariant",
                           true, 39, true);
    }
    list->mod_count = mc_java_i32_add(list->mod_count, 1); /* clear putfield7, including empty */
    const size_t old_size = list->size;
    list->size = 0; /* clear putfield23, before null slots35 */
    for (size_t i = 0; i < old_size; ++i) {
        mc_client_source_ke *button = list->entries[i];
        list->entries[i] = NULL;
        if (!mc_screen_button_release_locked(&button, &r->failure.screen)) {
            r->failure.caller = site(true, 39);
            return false;
        }
    }
    return true;
}

static bool da_body(mc_client_source_da *o, mc_client_screen_initialization *r, int32_t width,
                    int32_t height) {
    r->da_pc = 1;
    r->particles =
        mc_screen_particles_allocate_locked(o->classes, &o->memory.options, &r->failure.screen);
    if (r->particles == NULL) {
        r->failure.caller = site(true, 1);
        return false;
    }
    r->da_pc = 6;
    if (!mc_screen_particles_construct_locked(r->particles, r->app, &r->failure.screen)) {
        r->failure.caller = site(true, 6);
        return false;
    }
    o->fields.h = r->particles;
    store(r, true, 9);
    o->fields.b = r->app;
    store(r, true, 14);
    r->da_pc = 19;
    if (r->app == NULL)
        return failure(r, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE, "da actual App.q receiver", true,
                       19, false);
    mc_client_bitmap_font *font =
        r->app->font_renderer; /* ONE actual q read at19; nullable value */
    if (font != NULL) {
        r->failure.font.status = mc_client_bitmap_font_retain(font);
        if (r->failure.font.status != MC_CLIENT_BITMAP_FONT_OK)
            return failure(r,
                           r->failure.font.status == MC_CLIENT_BITMAP_FONT_BUSY
                               ? MC_CLIENT_SCREEN_BUSY
                               : MC_CLIENT_SCREEN_FONT_FAILED,
                           "da actual captured Font native hold", true, 22, true);
        r->font = font;
    }
    o->fields.g = font;
    store(r, true, 22);
    o->fields.c = width;
    store(r, true, 27);
    o->fields.d = height;
    store(r, true, 32);
    if (!list_clear(o, r))
        return false;
    r->da_pc = 45;
    if (o->kind == MC_CLIENT_SCREEN_MAIN_MENU) {
        if (!menu_body((mc_client_source_fu *)o, r)) {
            r->failure.entry = site(true, 45);
            return false;
        }
    } else if (o->kind == MC_CLIENT_SCREEN_WORLD_SELECTION) {
        if (!mc_screen_world_selection_initialize_locked((mc_client_source_rq *)o,
                                                         &r->failure.screen)) {
            r->failure.entry = site(true, 45);
            r->failure.caller = site(true, 45);
            return false;
        }
    } /* Actual da.b()V is return0. No generic successful callback. */
    r->da_pc = 48;
    return true;
}

static mc_client_screen_status publish(const mc_client_screen_init_error *e,
                                       mc_client_screen_init_error *out) {
    if (out != NULL)
        *out = *e;
    return e->screen.status;
}

static mc_client_screen_status begin(mc_client_source_da *o, client_application *app, int32_t width,
                                     int32_t height, bool da, mc_client_screen_init_error *out) {
    mc_client_screen_error early = {0};
    if (out != NULL)
        memset(out, 0, sizeof(*out));
    if (o == NULL)
        mc_screen_fail(&early, MC_CLIENT_SCREEN_INVALID_ARGUMENT, "screen initialize receiver",
                       NULL, NULL, NULL, 0);
    else if (mc_screen_lock(o->classes, &early)) {
        if (o->phase != MC_CLIENT_SCREEN_READY || o->classes->retiring || o->active_calls != 0 ||
            (!da && !(o->kind == MC_CLIENT_SCREEN_MAIN_MENU)))
            mc_screen_fail(&early, MC_CLIENT_SCREEN_INVALID_PHASE,
                           "actual initialized screen native admission", NULL, NULL, NULL, 0);
        else {
            mc_client_screen_initialization *r = mc_screen_allocate(&o->memory, sizeof(*r));
            if (r == NULL)
                mc_screen_fail(&early, o->memory.allocation_status,
                               "retained screen initialization record", NULL, NULL, NULL, 0);
            else {
                r->da_entry = da;
                r->phase = MC_CLIENT_SCREEN_INIT_ACTIVE;
                r->app = app;
                if (o->last_initialization != NULL)
                    o->last_initialization->next = r;
                else
                    o->initializations = r;
                o->last_initialization = r;
                ++o->initialization_count;
                bool success = true;
                if (da) {
                    if (!mc_client_application_screen_borrow(app, &r->app_counted))
                        success = failure(r, MC_CLIENT_SCREEN_APP_FAILED,
                                          "da native argument screen borrow", true, 0, true);
                    else
                        r->app_held = true;
                    if (success)
                        success = da_body(o, r, width, height);
                } else
                    success = menu_body((mc_client_source_fu *)o, r);
                r->phase = success ? MC_CLIENT_SCREEN_INIT_RETURNED : MC_CLIENT_SCREEN_INIT_FAILED;
                r->failure.screen.source_returned = success;
                r->failure.screen.initialization = NULL;
                (void)mtx_unlock(&o->classes->mutex);
                return publish(&r->failure, out);
            }
        }
        (void)mtx_unlock(&o->classes->mutex);
    }
    if (out != NULL)
        out->screen = early;
    return early.status;
}

mc_client_screen_status mc_client_screen_initialize(mc_client_source_da *o, client_application *app,
                                                    int32_t width, int32_t height,
                                                    mc_client_screen_init_error *out) {
    return begin(o, app, width, height, true, out);
}

mc_client_screen_status mc_client_screen_menu_initialize(mc_client_source_fu *o,
                                                         mc_client_screen_init_error *out) {
    return begin(o == NULL ? NULL : &o->base, NULL, 0, 0, false, out);
}

static bool cleanup_failed(mc_client_screen_initialization *r, mc_client_screen_error *out,
                           mc_client_screen_status status, const char *operation) {
    mc_screen_fail(&r->cleanup.screen, status, operation, NULL, NULL, NULL, 0);
    *out = r->cleanup.screen;
    out->initialization = &r->cleanup;
    return false;
}

bool mc_screen_initializations_prepare_locked(mc_client_source_da *o, mc_client_screen_error *e) {
    if (!o->native_field_occurrences_retired) {
        if ((o->kind == MC_CLIENT_SCREEN_MAIN_MENU) &&
            !mc_screen_button_release_locked(&((mc_client_source_fu *)o)->fields.l, e))
            return false;
        mc_client_screen_list *list = (mc_client_screen_list *)o->fields.e;
        if (list != NULL) {
            list->native_live = false; /* source/native views revoked by RETIRING */
            for (size_t i = 0; i < list->size; ++i) {
                mc_client_source_ke *button = list->entries[i];
                if (!mc_screen_button_release_locked(&button, e))
                    return false;
                list->entries[i] = NULL; /* retired native occurrence, NOT source clear/modCount */
            }
        }
        o->native_field_occurrences_retired = true;
    }
    for (mc_client_screen_initialization *r = o->initializations; r != NULL; r = r->next) {
        if (r->children_retired)
            continue;
        r->phase = MC_CLIENT_SCREEN_INIT_RETIRING;
        memset(&r->cleanup, 0, sizeof(r->cleanup));
        if (r->font != NULL) {
            r->cleanup.font.status = mc_client_bitmap_font_release(&r->font, &r->cleanup.font);
            if (r->cleanup.font.status != MC_CLIENT_BITMAP_FONT_OK)
                return cleanup_failed(r, e,
                                      r->cleanup.font.status == MC_CLIENT_BITMAP_FONT_BUSY
                                          ? MC_CLIENT_SCREEN_BUSY
                                          : MC_CLIENT_SCREEN_FONT_FAILED,
                                      "screen retained Font native retirement");
        }
        if (!mc_screen_particles_release_locked(&r->particles, &r->cleanup.screen))
            return cleanup_failed(r, e, r->cleanup.screen.status, "screen retained du retirement");
        if (!mc_screen_button_release_locked(&r->replaced_l, &r->cleanup.screen))
            return cleanup_failed(r, e, r->cleanup.screen.status,
                                  "screen replaced multiplayer occurrence");
        for (unsigned int i = 0; i < r->button_count; ++i)
            if (!mc_screen_button_release_locked(&r->buttons[i], &r->cleanup.screen))
                return cleanup_failed(r, e, r->cleanup.screen.status,
                                      "screen retained Button retirement");
        r->cleanup.calendar.status =
            mc_platform_calendar_retire(&r->calendar, &r->cleanup.calendar);
        if (r->cleanup.calendar.status != MC_PLATFORM_CALENDAR_OK)
            return cleanup_failed(r, e, MC_CLIENT_SCREEN_CALENDAR_FAILED,
                                  "screen Calendar native retirement");
        r->children_retired = true;
    }
    return true;
}

bool mc_screen_initializations_release_apps_locked(mc_client_source_da *o,
                                                   mc_client_screen_error *e) {
    for (mc_client_screen_initialization *r = o->initializations; r != NULL; r = r->next) {
        if (r->app_held) {
            if (!mc_client_application_screen_release(r->app, r->app_counted))
                return cleanup_failed(r, e, MC_CLIENT_SCREEN_APP_FAILED,
                                      "screen record native App release");
            r->app_held = false;
            r->app_counted = false;
        }
        r->phase = MC_CLIENT_SCREEN_INIT_RETIRED;
    }
    return true;
}

bool mc_client_screen_initialization_inspect(mc_client_source_da *o, size_t index,
                                             mc_client_screen_init_state *out) {
    mc_client_screen_error e = {0};
    if (o == NULL || out == NULL || !mc_screen_lock(o->classes, &e))
        return false;
    mc_client_screen_initialization *r = o->initializations;
    for (size_t i = 0; r != NULL && i < index; ++i)
        r = r->next;
    if (r != NULL) {
        *out = (mc_client_screen_init_state){0};
        out->phase = r->phase;
        out->da_entry = r->da_entry;
        out->app_held = r->app_held;
        out->app_counted = r->app_counted;
        out->date_allocated = r->date_allocated;
        out->calendar_live = r->calendar != NULL;
        out->app_argument = r->app_held ? r->app : NULL;
        out->held_particles = r->particles;
        out->held_font = r->font;
        out->calendar = r->calendar;
        out->date = r->date;
        out->da_pc = r->da_pc;
        out->fu_pc = r->fu_pc;
        out->store_count = r->store_count;
        out->button_count = r->button_count;
        memcpy(out->stores, r->stores, sizeof(out->stores));
        for (unsigned int i = 0; i < 5; ++i)
            out->buttons[i] = r->buttons[i];
        out->replaced_l = r->replaced_l;
        out->translation = r->translation;
        out->applet_branch = r->applet_branch;
        out->session_present = r->session_present;
        out->row_origin = r->row_origin;
        out->failure = &r->failure;
        out->cleanup_failure = &r->cleanup;
    }
    (void)mtx_unlock(&o->classes->mutex);
    return r != NULL;
}

bool mc_client_screen_list_inspect(const mc_client_screen_list *o,
                                   mc_client_screen_list_state *out) {
    if (o == NULL || out == NULL)
        return false;
    *out = (mc_client_screen_list_state){o->size, o->capacity, o->mod_count, o->native_live};
    return true;
}

bool mc_client_screen_list_button(const mc_client_screen_list *o, size_t index,
                                  const mc_client_source_ke **out) {
    if (o == NULL || out == NULL || !o->native_live || o->kind != MC_SCREEN_LIST_BUTTONS ||
        index >= o->size)
        return false;
    *out = o->entries[index];
    return true;
}
