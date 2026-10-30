#include "source_screen_transitions.h"
#include "../application_state.h"
#include "../startup/application_startup_internal.h"
#include "../startup/application_startup_menu.h"
#include "../world/load_world.h"
#include "display_screen.h"
#include "mc/client/controller/singleplayer.h"
#include "mc/client/statistics/writer.h"
#include "mc/core/java/java.h"
#include "mc/statistics/registry/registry.h"
#include "mc/world/runtime/construction.h"
#include "source_pause_screen_internal.h"
#include "source_world_list.h"
#include "source_world_selection_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct screen_node {
    struct screen_node *next;
    mc_client_source_rq *rq;
    mc_client_source_oz *pause;
    mc_client_display_screen_call *display;
    mc_client_world_load_call *load;
} screen_node;

struct mc_client_screen_transitions {
    client_application *app;
    mc_client_screen_classes *classes;
    mc_client_source_da *initial;
    screen_node *nodes;
    size_t node_count, screens, calls;
    bool active, retiring, slot_handed_off;
    bool terminal_prepare_selected, terminal_prepared;
    mc_client_singleplayer_controller *controller;
    mc_client_world_load_classes *world_classes;
    mc_client_world_load_call *last_load;
    mc_world_info_name *fallback_filename, *fallback_display;
    mc_client_save_error save;
    mc_singleplayer_controller_error controller_error;
    mc_client_screen_error failure;
    mc_client_stats_error statistics;
    mc_registry_error registry;
};

static mc_client_screen_status fail(mc_client_screen_transitions *r, mc_client_screen_status status,
                                    const char *operation, const char *type, const char *method,
                                    const char *descriptor, unsigned int pc) {
    mc_screen_fail(&r->failure, status, operation, type, method, descriptor, pc);
    r->failure.native_policy = true;
    return status;
}

static bool enter(mc_client_screen_transitions *r, mc_client_screen_error *e) {
    if (e == NULL)
        return false;
    if (r == NULL) {
        mc_screen_fail(e, MC_CLIENT_SCREEN_INVALID_ARGUMENT, "native screen transition receiver",
                       NULL, NULL, NULL, 0);
        return false;
    }
    if (r->active || r->retiring) {
        mc_screen_fail(e, MC_CLIENT_SCREEN_BUSY, "screen transition active/retiring", NULL, NULL,
                       NULL, 0);
        return false;
    }
    r->failure = (mc_client_screen_error){0};
    r->active = true;
    return true;
}

static mc_client_screen_status leave(mc_client_screen_transitions *r, mc_client_screen_error *e) {
    r->active = false;
    if (r->failure.status == MC_CLIENT_SCREEN_OK)
        r->failure.source_returned = true;
    *e = r->failure;
    return e->status;
}

static screen_node *node(mc_client_screen_transitions *r) {
    if (r->node_count >= 4096) {
        fail(r, MC_CLIENT_SCREEN_LIMIT, "native retained transition-record bound", NULL, NULL, NULL,
             0);
        return NULL;
    }
    screen_node *n = calloc(1, sizeof(*n));
    if (n == NULL) {
        fail(r, MC_CLIENT_SCREEN_ALLOCATION_FAILED, "native transition-record allocation", NULL,
             NULL, NULL, 0);
        return NULL;
    }
    n->next = r->nodes;
    r->nodes = n;
    ++r->node_count;
    return n;
}

static mc_client_screen_status world_status(mc_client_world_load_status status) {
    switch (status) {
        case MC_CLIENT_WORLD_LOAD_OK:
            return MC_CLIENT_SCREEN_OK;
        case MC_CLIENT_WORLD_LOAD_BUSY:
            return MC_CLIENT_SCREEN_BUSY;
        case MC_CLIENT_WORLD_LOAD_LIMIT:
            return MC_CLIENT_SCREEN_LIMIT;
        case MC_CLIENT_WORLD_LOAD_ALLOCATION:
            return MC_CLIENT_SCREEN_ALLOCATION_FAILED;
        case MC_CLIENT_WORLD_LOAD_ARGUMENT:
            return MC_CLIENT_SCREEN_INVALID_ARGUMENT;
        case MC_CLIENT_WORLD_LOAD_STATE:
            return MC_CLIENT_SCREEN_INVALID_PHASE;
        case MC_CLIENT_WORLD_LOAD_NULL_REFERENCE:
            return MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE;
        case MC_CLIENT_WORLD_LOAD_UNIMPLEMENTED:
            return MC_CLIENT_SCREEN_UNIMPLEMENTED_DEPENDENCY;
        default:
            return MC_CLIENT_SCREEN_APP_FAILED;
    }
}

static bool world_failure(mc_client_screen_transitions *r, mc_client_world_load_status status,
                          const mc_client_world_load_call *call) {
    mc_client_world_load_state state;
    if (call == NULL || !mc_client_world_load_inspect(call, &state) || state.failure == NULL) {
        fail(r, world_status(status), "native World-load invocation admission", NULL, NULL, NULL,
             0);
        return false;
    }
    const mc_client_world_load_error *error = state.failure;
    const mc_client_world_load_site *site =
        error->child.class_name == NULL ? &error->site : &error->child;
    fail(r, world_status(status), error->operation, site->class_name, site->method,
         site->descriptor, site->pc < 0 ? 0u : (unsigned int)site->pc);
    r->failure.native_policy = error->native_policy;
    r->failure.parent_site = (mc_client_screen_site){"rq", "e", "(I)V", 80};
    return false;
}

static bool display(mc_client_screen_transitions *r, mc_client_source_da *target, const char *type,
                    const char *method, const char *descriptor, unsigned int pc) {
    screen_node *n = node(r);
    if (n == NULL)
        return false;
    ++r->calls;
    const mc_client_display_screen_status status =
        mc_client_display_screen_run(r->app, r->classes, target, &n->display);
    if (n->display != NULL && n->display->r_stored) {
        /* Native slot authority follows the actual new r106 publication,
         * including a failure after that store. Historical startup stores stay. */
        r->slot_handed_off = true;
        r->app->startup->menu->slot_handed_off = true;
    }
    if (status != MC_CLIENT_DISPLAY_SCREEN_OK) {
        fail(r,
             status == MC_CLIENT_DISPLAY_SCREEN_BUSY ? MC_CLIENT_SCREEN_BUSY
             : status == MC_CLIENT_DISPLAY_SCREEN_UNIMPLEMENTED
                 ? MC_CLIENT_SCREEN_UNIMPLEMENTED_DEPENDENCY
             : status == MC_CLIENT_DISPLAY_SCREEN_NULL_REFERENCE
                 ? MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE
                 : MC_CLIENT_SCREEN_APP_FAILED,
             n->display == NULL ? "actual displayGuiScreen native allocation"
                                : n->display->operation,
             type, method, descriptor, pc);
        r->failure.native_policy = status != MC_CLIENT_DISPLAY_SCREEN_NULL_REFERENCE;
        if (n->display != NULL)
            r->failure.parent_site =
                n->display->child.class_name != NULL ? n->display->child : n->display->site;
        return false;
    }
    if (n->display->fallback == NULL) {
        const mc_client_display_screen_status retired =
            mc_client_display_screen_retire(&n->display);
        if (retired != MC_CLIENT_DISPLAY_SCREEN_OK) {
            fail(r, MC_CLIENT_SCREEN_BUSY, "completed display-call native retirement", type, method,
                 descriptor, pc);
            return false;
        }
    }
    return true;
}

mc_client_screen_status mc_client_screen_transitions_create(client_application *app,
                                                            mc_client_screen_transitions **out,
                                                            mc_client_screen_error *e) {
    if (e == NULL)
        return MC_CLIENT_SCREEN_INVALID_ARGUMENT;
    *e = (mc_client_screen_error){0};
    if (app == NULL || out == NULL || *out != NULL || app->startup == NULL ||
        app->constructor_phase != MC_CLIENT_APP_READY || app->screen_transitions != NULL ||
        app->startup->phase == MC_CLIENT_STARTUP_RETIRING || app->startup->menu == NULL ||
        !app->startup->menu->source_startup_returned || app->r == NULL ||
        app->r->kind != MC_CLIENT_SCREEN_MAIN_MENU ||
        app->r != mc_client_screen_menu_base(app->startup->menu->candidate) ||
        app->startup->menu->slot_handed_off)
        return mc_screen_fail(e, MC_CLIENT_SCREEN_INVALID_PHASE,
                              "native completed startup screen graph", NULL, NULL, NULL, 0);
    mc_client_screen_transitions *r = calloc(1, sizeof(*r));
    if (r == NULL)
        return mc_screen_fail(e, MC_CLIENT_SCREEN_ALLOCATION_FAILED, "native screen graph", NULL,
                              NULL, NULL, 0);
    r->app = app;
    r->classes = app->r->classes;
    r->initial = app->r;
    const mc_client_screen_status status = mc_client_screen_classes_retain(r->classes, e);
    if (status != MC_CLIENT_SCREEN_OK) {
        free(r);
        return status;
    }
    const mc_client_world_load_status world =
        mc_client_world_load_classes_create(app, &r->world_classes);
    if (world != MC_CLIENT_WORLD_LOAD_OK) {
        /* The retained native graph is returned for checked cleanup even if a
         * namespace association fails after acquiring an earlier occurrence. */
        *out = r;
        app->screen_transitions = r;
        return mc_screen_fail(e, world_status(world), "native World-load class association", NULL,
                              NULL, NULL, 0);
    }
    *out = r;
    app->screen_transitions = r;
    return MC_CLIENT_SCREEN_OK;
}

static bool open_world_selection(mc_client_screen_transitions *r, mc_client_source_da *parent) {
    screen_node *n = node(r);
    if (n == NULL)
        return false;
    if (mc_client_screen_world_selection_allocate(r->classes, NULL, &n->rq, &r->failure) !=
        MC_CLIENT_SCREEN_OK)
        return false;
    ++r->screens;
    if (mc_client_screen_world_selection_construct(n->rq, parent, &r->failure) !=
        MC_CLIENT_SCREEN_OK)
        return false;
    return display(r, mc_client_screen_world_selection_base(n->rq), "fu", "a", "(Lke;)V", 49);
}

static bool focus(mc_client_screen_transitions *r, const char *type, const char *method,
                  const char *descriptor, unsigned int pc) {
    screen_node *n = node(r);
    if (n == NULL)
        return false;
    ++r->calls;
    const mc_client_display_screen_status status =
        mc_client_display_focus_run(r->app, r->classes, &n->display);
    if (status != MC_CLIENT_DISPLAY_SCREEN_OK) {
        fail(r,
             status == MC_CLIENT_DISPLAY_SCREEN_NULL_REFERENCE
                 ? MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE
                 : MC_CLIENT_SCREEN_APP_FAILED,
             n->display == NULL ? "actual focus native admission" : n->display->operation, type,
             method, descriptor, pc);
        if (n->display != NULL) {
            r->failure.native_policy = status != MC_CLIENT_DISPLAY_SCREEN_NULL_REFERENCE;
            r->failure.parent_site =
                n->display->child.class_name == NULL ? n->display->site : n->display->child;
        }
        return false;
    }
    if (mc_client_display_screen_retire(&n->display) != MC_CLIENT_DISPLAY_SCREEN_OK) {
        fail(r, MC_CLIENT_SCREEN_BUSY, "actual focus native retirement", type, method, descriptor,
             pc);
        return false;
    }
    return true;
}

mc_client_screen_status mc_client_screen_transitions_pause(mc_client_screen_transitions *r,
                                                           mc_client_screen_error *e) {
    if (!enter(r, e))
        return r == NULL || e == NULL ? MC_CLIENT_SCREEN_INVALID_ARGUMENT : e->status;
    if (r->app->r == NULL) {
        screen_node *n = node(r);
        if (n != NULL) {
            if (mc_client_screen_pause_allocate(r->classes, NULL, &n->pause, &r->failure) ==
                MC_CLIENT_SCREEN_OK) {
                ++r->screens;
                if (mc_client_screen_pause_construct(n->pause, &r->failure) == MC_CLIENT_SCREEN_OK)
                    (void)display(r, mc_client_screen_pause_base(n->pause),
                                  "net/minecraft/client/Minecraft", "i", "()V", 16);
                else
                    r->failure.parent_site =
                        (mc_client_screen_site){"net/minecraft/client/Minecraft", "i", "()V", 13};
            } else
                r->failure.parent_site =
                    (mc_client_screen_site){"net/minecraft/client/Minecraft", "i", "()V", 9};
        }
    }
    return leave(r, e);
}

mc_client_screen_status mc_client_screen_transitions_close(mc_client_screen_transitions *r,
                                                           mc_client_source_da *screen,
                                                           mc_client_screen_error *e) {
    if (!enter(r, e))
        return r == NULL || e == NULL ? MC_CLIENT_SCREEN_INVALID_ARGUMENT : e->status;
    if (screen == NULL || screen->classes != r->classes || screen->fields.b != r->app)
        fail(r, MC_CLIENT_SCREEN_INVALID_ARGUMENT, "actual close source identities", NULL, NULL,
             NULL, 0);
    else if (display(r, NULL, "da", "a", "(CI)V", 10))
        (void)focus(r, "da", "a", "(CI)V", 17);
    return leave(r, e);
}

static void pause_action(mc_client_screen_transitions *r, mc_client_source_ke *button) {
    const int32_t id = button->fields.f;
    if (id == 0 || id == 5 || id == 6) {
        fail(r, MC_CLIENT_SCREEN_UNIMPLEMENTED_DEPENDENCY,
             id == 0   ? "actual Options co construction"
             : id == 5 ? "actual Achievements xm construction"
                       : "actual Statistics dv construction",
             "oz", "a", "(Lke;)V",
             id == 0   ? 11u
             : id == 5 ? 128u
                       : 158u);
        return;
    }
    if (id == 4) {
        if (display(r, NULL, "oz", "a", "(Lke;)V", 106))
            (void)focus(r, "oz", "a", "(Lke;)V", 113);
        return;
    }
    if (id != 1)
        return; /* All five independent source ID tests fail. */
    /* Actual App.I41 receiver is captured before jl.j active use44. */
    mc_client_stats_writer *writer = r->app->stats_writer;
    if (mc_registry_classes_initialize_statistics(r->app->registry, &r->registry) !=
        MC_REGISTRY_OK) {
        fail(r, MC_CLIENT_SCREEN_REGISTRY_FAILED, "oz actual jl active use", "oz", "a", "(Lke;)V",
             44);
        r->failure.registry = r->registry;
        return;
    }
    mc_registry_statistic_view statistics;
    if (mc_registry_classes_inspect_statistics(r->app->registry, &statistics, &r->registry) !=
            MC_REGISTRY_OK ||
        statistics.named == NULL) {
        fail(r, MC_CLIENT_SCREEN_REGISTRY_FAILED, "oz actual jl.j field", "oz", "a", "(Lke;)V", 44);
        r->failure.registry = r->registry;
        return;
    }
    if (writer == NULL) {
        fail(r, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE, "oz actual xi.a null receiver", "oz", "a",
             "(Lke;)V", 48);
        r->failure.native_policy = false;
        return;
    }
    if (mc_client_stats_writer_increment(writer, statistics.named[4], 1, &r->statistics) !=
        MC_CLIENT_STATS_OK) {
        fail(r, MC_CLIENT_SCREEN_APP_FAILED, "oz actual leave-game statistic increment", "oz", "a",
             "(Lke;)V", 48);
        r->failure.parent_site =
            (mc_client_screen_site){r->statistics.class_name, r->statistics.method,
                                    r->statistics.descriptor, r->statistics.bytecode};
        return;
    }
    if (r->app->active_world != NULL) {
        mc_world_runtime_fields fields;
        if (mc_world_runtime_fields_get(r->app->active_world, &fields) != MC_WORLD_STORAGE_OK) {
            fail(r, MC_CLIENT_SCREEN_APP_FAILED, "oz actual Minecraft.l World.B read", "oz", "a",
                 "(Lke;)V", 55);
            return;
        }
        if (fields.B) {
            fail(r, MC_CLIENT_SCREEN_UNIMPLEMENTED_DEPENDENCY,
                 "actual remote World disconnect dispatch", "oz", "a", "(Lke;)V", 68);
            return;
        }
    }
    fail(r, MC_CLIENT_SCREEN_UNIMPLEMENTED_DEPENDENCY, "actual Minecraft.a(World) detach call",
         "oz", "a", "(Lke;)V", 76);
}

static bool indexed_text(mc_client_screen_transitions *r, mc_nbt_string prefix, bool space,
                         int32_t number, mc_world_info_name **slot) {
    char decimal[16];
    const int count = snprintf(decimal, sizeof(decimal), "%d", number);
    if (count < 0 || (size_t)count >= sizeof(decimal) || prefix.length > 1048576u - 32u)
        return false;
    const size_t length = prefix.length + (space ? 1u : 0u) + (size_t)count;
    uint16_t *units = malloc((length == 0 ? 1 : length) * sizeof(*units));
    if (units == NULL)
        return false;
    if (prefix.length != 0)
        memcpy(units, prefix.units, prefix.length * sizeof(*units));
    size_t at = prefix.length;
    if (space)
        units[at++] = ' ';
    for (int i = 0; i < count; ++i)
        units[at++] = (unsigned char)decimal[i];
    mc_world_info_name *value = NULL;
    const mc_world_info_status status =
        mc_world_info_name_copy((mc_nbt_string){units, length}, 4u * 1024u * 1024u, &value);
    free(units);
    if (status != MC_WORLD_INFO_OK)
        return false;
    mc_world_info_name_release(*slot);
    *slot = value;
    (void)r;
    return true;
}

static bool select_body(mc_client_screen_transitions *r, mc_client_source_rq *rq, int32_t index) {
    if (!display(r, NULL, "rq", "e", "(I)V", 5))
        return false;
    if (rq->fields.l)
        return true;     /* Actual loading guard returns15 AFTER the display call. */
    rq->fields.l = true; /* Actual putfield18. */
    if (r->controller != NULL || r->app->controller != NULL) {
        fail(r, MC_CLIENT_SCREEN_BUSY, "native earlier controller occurrence", "rq", "e", "(I)V",
             25);
        return false;
    }
    mc_singleplayer_controller_status status = mc_client_singleplayer_controller_allocate(
        r->app->entities, r->app->profile, 0, &r->controller, &r->controller_error);
    if (status == MC_SINGLEPLAYER_CONTROLLER_OK)
        status = mc_client_singleplayer_controller_construct(r->controller, r->app,
                                                             &r->controller_error);
    if (status != MC_SINGLEPLAYER_CONTROLLER_OK) {
        fail(r, MC_CLIENT_SCREEN_APP_FAILED, "actual os constructor child", "rq", "e", "(I)V", 33);
        r->failure.parent_site = (mc_client_screen_site){
            r->controller_error.site.class_name, r->controller_error.site.method,
            r->controller_error.site.descriptor, r->controller_error.site.pc};
        return false;
    }
    r->app->controller = r->controller; /* Actual source ob/os identity at c36. */
    if (rq->fields.n == NULL) {
        fail(r, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE, "actual RQ selected List.get receiver",
             "rq", "c", "(I)Ljava/lang/String;", 5);
        return false;
    }
    mc_client_save_selection *row =
        index < 0 ? NULL : mc_client_save_list_at(rq->fields.n, (size_t)index);
    if (row == NULL) {
        fail(r, MC_CLIENT_SCREEN_SOURCE_INDEX, "actual RQ selected List.get index", "rq", "c",
             "(I)Ljava/lang/String;", 5);
        return false;
    }
    mc_client_save_selection_fields fields;
    if (mc_client_save_selection_read(row, &fields, &r->save) != MC_CLIENT_SAVE_OK) {
        fail(r, MC_CLIENT_SCREEN_APP_FAILED, "actual selected vb fields", "rq", "c",
             "(I)Ljava/lang/String;", 13);
        return false;
    }
    if (fields.filename == NULL) {
        static const uint16_t world[] = {'W', 'o', 'r', 'l', 'd'};
        if (!indexed_text(r, (mc_nbt_string){world, 5}, false, index, &r->fallback_filename)) {
            fail(r, MC_CLIENT_SCREEN_ALLOCATION_FAILED, "actual selected filename String", "rq",
                 "e", "(I)V", 68);
            return false;
        }
    }
    if (fields.display_name == NULL || mc_world_info_name_text(fields.display_name).length == 0) {
        mc_player_string_view prefix;
        if (!mc_client_screen_text_read(rq->fields.p, &prefix) ||
            !indexed_text(r, (mc_nbt_string){prefix.units, prefix.length}, true,
                          mc_java_i32_add(index, 1), &r->fallback_display)) {
            fail(r, MC_CLIENT_SCREEN_ALLOCATION_FAILED, "actual selected display-name String", "rq",
                 "d", "(I)Ljava/lang/String;", 64);
            return false;
        }
    }
    screen_node *invocation = node(r);
    if (invocation == NULL)
        return false;
    const mc_client_world_load_status loaded = mc_client_world_load_begin(
        r->world_classes, fields.filename == NULL ? r->fallback_filename : fields.filename,
        fields.display_name == NULL || mc_world_info_name_text(fields.display_name).length == 0
            ? r->fallback_display
            : fields.display_name,
        0, &invocation->load);
    r->last_load = invocation->load;
    if (loaded != MC_CLIENT_WORLD_LOAD_OK)
        return world_failure(r, loaded, invocation->load);
    return display(r, NULL, "rq", "e", "(I)V", 88);
}

mc_client_screen_status mc_client_screen_transitions_select(mc_client_screen_transitions *r,
                                                            mc_client_source_rq *rq, int32_t index,
                                                            mc_client_screen_error *e) {
    if (!enter(r, e))
        return r == NULL || e == NULL ? MC_CLIENT_SCREEN_INVALID_ARGUMENT : e->status;
    if (rq == NULL || rq->base.classes != r->classes || rq->base.phase != MC_CLIENT_SCREEN_READY ||
        rq->base.fields.b != r->app)
        fail(r, MC_CLIENT_SCREEN_INVALID_PHASE, "native selected RQ identity", NULL, NULL, NULL, 0);
    else
        (void)select_body(r, rq, index);
    return leave(r, e);
}

mc_client_screen_status mc_client_screen_transitions_button(mc_client_screen_transitions *r,
                                                            mc_client_source_da *screen,
                                                            mc_client_source_ke *button,
                                                            mc_client_screen_error *e) {
    if (!enter(r, e))
        return r == NULL || e == NULL ? MC_CLIENT_SCREEN_INVALID_ARGUMENT : e->status;
    if (screen == NULL || button == NULL || screen->classes != r->classes ||
        button->classes != r->classes || screen->fields.b != r->app)
        fail(r, MC_CLIENT_SCREEN_INVALID_ARGUMENT, "native transition source identities", NULL,
             NULL, NULL, 0);
    else if (screen->kind == MC_CLIENT_SCREEN_MAIN_MENU) {
        const int32_t id = button->fields.f;
        if (id == 1)
            (void)open_world_selection(r, screen);
        else if (id == 0 || id == 2 || id == 3)
            fail(r, MC_CLIENT_SCREEN_UNIMPLEMENTED_DEPENDENCY,
                 "actual main-menu destination construction", "fu", "a", "(Lke;)V",
                 id == 0   ? 11u
                 : id == 2 ? 64u
                           : 87u);
        else if (id == 4 && mc_client_application_source_stop(r->app, NULL) != MC_CLIENT_STARTUP_OK)
            fail(r, MC_CLIENT_SCREEN_APP_FAILED, "actual source stop", "fu", "a", "(Lke;)V", 110);
    } else if (screen->kind == MC_CLIENT_SCREEN_PAUSE) {
        pause_action(r, button);
    } else if (screen->kind == MC_CLIENT_SCREEN_WORLD_SELECTION) {
        mc_client_source_rq *rq = (mc_client_source_rq *)screen;
        const int32_t id = button->fields.f;
        if (!button->fields.g) { /* Actual rq.a returns7. */
        } else if (id == 0)
            (void)display(r, rq->fields.a, "rq", "a", "(Lke;)V", 226);
        else if (id == 1)
            (void)select_body(r, rq, rq->fields.m);
        else if (id == 2 || id == 3 || id == 6)
            fail(r, MC_CLIENT_SCREEN_UNIMPLEMENTED_DEPENDENCY,
                 id == 2   ? "actual delete-confirmation preparation"
                 : id == 3 ? "actual create-world fj construction"
                           : "actual rename-world jk construction",
                 "rq", "a", "(Lke;)V",
                 id == 2   ? 21u
                 : id == 3 ? 162u
                           : 189u);
        else
            (void)mc_client_world_list_button(rq->fields.o, button, &r->failure);
    }
    return leave(r, e);
}

bool mc_client_screen_transitions_inspect(const mc_client_screen_transitions *r,
                                          mc_client_screen_transition_state *out) {
    if (r == NULL || out == NULL)
        return false;
    *out = (mc_client_screen_transition_state){.active = r->active,
                                               .retiring = r->retiring,
                                               .slot_handed_off = r->slot_handed_off,
                                               .screens = r->screens,
                                               .calls = r->calls,
                                               .initial_screen = r->initial,
                                               .failure = &r->failure,
                                               .world_load = r->last_load,
                                               .terminal_prepare_selected =
                                                   r->terminal_prepare_selected,
                                               .terminal_prepared = r->terminal_prepared};
    return true;
}

mc_client_screen_classes *
mc_client_screen_transitions_classes(const mc_client_screen_transitions *r) {
    return r == NULL ? NULL : r->classes;
}

mc_client_screen_status
mc_client_screen_transitions_terminal_prepare(mc_client_screen_transitions *r,
                                              mc_client_screen_error *e) {
    if (e == NULL)
        return MC_CLIENT_SCREEN_INVALID_ARGUMENT;
    *e = (mc_client_screen_error){0};
    if (r == NULL)
        return mc_screen_fail(e, MC_CLIENT_SCREEN_INVALID_ARGUMENT,
                              "native screen terminal preparation receiver", NULL, NULL, NULL, 0);
    if (r->active)
        return mc_screen_fail(e, MC_CLIENT_SCREEN_BUSY, "active native screen terminal preparation",
                              NULL, NULL, NULL, 0);
    if (r->retiring && !r->terminal_prepare_selected)
        return mc_screen_fail(e, MC_CLIENT_SCREEN_BUSY, "legacy screen retirement already entered",
                              NULL, NULL, NULL, 0);
    if (r->world_classes != NULL) {
        const mc_client_world_load_status ended =
            mc_client_world_load_classes_terminal_end(r->world_classes);
        if (ended != MC_CLIENT_WORLD_LOAD_OK)
            return mc_screen_fail(e, world_status(ended),
                                  "World-load class terminal admission ending", NULL, NULL, NULL,
                                  0);
    }
    r->retiring = r->terminal_prepare_selected = true;
    if (r->terminal_prepared)
        return MC_CLIENT_SCREEN_OK;
    r->active = true;
    for (screen_node *n = r->nodes; n != NULL; n = n->next) {
        if (n->load == NULL)
            continue;
        const mc_client_world_load_status prepared = mc_client_world_load_terminal_prepare(n->load);
        if (prepared != MC_CLIENT_WORLD_LOAD_OK) {
            mc_client_world_load_state state;
            const char *operation = "retained World-load terminal preparation";
            if (mc_client_world_load_inspect(n->load, &state) && state.cleanup != NULL &&
                state.cleanup->status != MC_CLIENT_WORLD_LOAD_OK &&
                state.cleanup->operation != NULL)
                operation = state.cleanup->operation;
            r->active = false;
            return mc_screen_fail(e, world_status(prepared), operation, NULL, NULL, NULL, 0);
        }
    }
    r->terminal_prepared = true;
    r->active = false;
    return MC_CLIENT_SCREEN_OK;
}

mc_client_screen_status mc_client_screen_transitions_retire(mc_client_screen_transitions **slot,
                                                            mc_client_screen_error *e) {
    if (slot == NULL || e == NULL)
        return MC_CLIENT_SCREEN_INVALID_ARGUMENT;
    *e = (mc_client_screen_error){0};
    mc_client_screen_transitions *r = *slot;
    if (r == NULL)
        return MC_CLIENT_SCREEN_OK;
    if (r->active)
        return mc_screen_fail(e, MC_CLIENT_SCREEN_BUSY, "active native screen graph", NULL, NULL,
                              NULL, 0);
    r->retiring = true;
    while (r->nodes != NULL) {
        screen_node *n = r->nodes;
        mc_client_world_load_call *load = n->load;
        const mc_client_world_load_status loaded = mc_client_world_load_retire(&n->load);
        if (loaded != MC_CLIENT_WORLD_LOAD_OK)
            return mc_screen_fail(e, world_status(loaded), "retained actual World-load prefix",
                                  NULL, NULL, NULL, 0);
        if (load != NULL && r->last_load == load)
            r->last_load = NULL;
        if (mc_client_display_screen_retire(&n->display) != MC_CLIENT_DISPLAY_SCREEN_OK)
            return mc_screen_fail(e, MC_CLIENT_SCREEN_BUSY, "retained actual display prefix", NULL,
                                  NULL, NULL, 0);
        mc_client_source_da *base = mc_client_screen_world_selection_base(n->rq);
        const mc_client_screen_status status = mc_client_screen_world_selection_retire(&n->rq, e);
        if (status != MC_CLIENT_SCREEN_OK)
            return status;
        if (base != NULL && r->app->r == base)
            r->app->r = NULL; /* Native source view ended after consuming its owner. */
        base = mc_client_screen_pause_base(n->pause);
        const mc_client_screen_status pause = mc_client_screen_pause_retire(&n->pause, e);
        if (pause != MC_CLIENT_SCREEN_OK)
            return pause;
        if (base != NULL && r->app->r == base)
            r->app->r = NULL;
        r->nodes = n->next;
        free(n);
    }
    if (r->controller != NULL) {
        const bool published = r->app->controller == r->controller;
        if (mc_client_singleplayer_controller_free(&r->controller, &r->controller_error) !=
            MC_SINGLEPLAYER_CONTROLLER_OK)
            return mc_screen_fail(e, MC_CLIENT_SCREEN_BUSY, "actual controller native retirement",
                                  NULL, NULL, NULL, 0);
        if (published)
            r->app->controller = NULL; /* Consumed native source receiver view. */
    }
    if (r->slot_handed_off)
        r->app->r = NULL; /* End the runtime's remaining initial-screen slot borrow. */
    const mc_client_world_load_status world =
        mc_client_world_load_classes_retire(&r->world_classes);
    if (world != MC_CLIENT_WORLD_LOAD_OK)
        return mc_screen_fail(e, world_status(world), "native World-load namespace retirement",
                              NULL, NULL, NULL, 0);
    const mc_client_screen_status status = mc_client_screen_classes_release(&r->classes, e);
    if (status != MC_CLIENT_SCREEN_OK)
        return status;
    mc_world_info_name_release(r->fallback_filename);
    mc_world_info_name_release(r->fallback_display);
    r->app->screen_transitions = NULL;
    free(r);
    *slot = NULL;
    return MC_CLIENT_SCREEN_OK;
}
