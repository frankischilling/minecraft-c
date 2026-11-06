#include "source_screen_transitions.h"
#include "../../entity/player/player_internal.h"
#include "../application_state.h"
#include "../runtime/bootstrap_terminal_internal.h"
#include "../startup/application_startup_internal.h"
#include "../startup/application_startup_menu.h"
#include "../world/load_world.h"
#include "../world/load_world_terminal_internal.h"
#include "../world/respawn.h"
#include "display_screen.h"
#include "mc/client/controller/singleplayer.h"
#include "mc/client/statistics/writer.h"
#include "mc/core/java/java.h"
#include "mc/statistics/registry/registry.h"
#include "mc/world/runtime/construction.h"
#include "source_create_world_internal.h"
#include "source_inventory_screen_internal.h"
#include "source_pause_screen_internal.h"
#include "source_screen_transition_receipts_internal.h"
#include "source_screen_transitions_terminal_internal.h"
#include "source_workbench_screen_internal.h"
#include "source_world_list.h"
#include "source_world_selection_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct screen_node {
    struct screen_node *next;
    mc_client_source_rq *rq;
    mc_client_source_oz *pause;
    mc_client_source_fu *menu;
    mc_client_source_fj *create;
    mc_client_source_ue *inventory;
    mc_client_source_oo *workbench;
    mc_entity *constructor_player; /* Native occurrence of actual DC invocation receiver. */
    mc_world_info_name *create_folder, *create_name;
    mc_client_display_screen_call *display;
    mc_client_world_load_call *load;
    mc_client_respawn_call *respawn;
    mc_client_respawn_error respawn_error, respawn_cleanup;
    mc_client_respawn_state completed_respawn;
    bool completed_respawn_captured;
    mc_client_singleplayer_controller *controller; /* Candidate or displaced owning occurrence. */
    mc_client_singleplayer_controller **load_controller_slot; /* Actual owning slot, borrowed. */
} screen_node;

struct mc_client_screen_transitions {
    client_application *app;
    mc_client_screen_classes *classes;
    mc_client_source_da *initial;
    screen_node *nodes, *controller_nodes;
    size_t node_count, screens, calls;
    bool active, retiring, slot_handed_off;
    bool terminal_prepare_selected, terminal_prepared;
    const mc_client_bootstrap_terminal_scope *terminal_root; /* Exact borrowed root authority. */
    mc_client_singleplayer_controller *controller;
    mc_client_world_load_classes *world_classes;
    mc_client_world_load_call *last_load;
    screen_node *last_respawn_node;
    mc_client_display_screen_call *last_display_call;
    screen_node *last_load_node;
    mc_client_world_load_call *leave_owner; /* Actual retained invocation owning world_leave. */
    mc_client_world_load_error world_leave;
    size_t leave_calls;
    bool world_leave_retained;
    mc_world_info_name *fallback_filename, *fallback_display;
    mc_client_save_error save;
    mc_singleplayer_controller_error controller_error;
    mc_client_screen_error failure;
    mc_screen_transition_receipts receipts;
    mc_client_stats_error statistics;
    mc_registry_error registry;
};

bool mc_screen_transition_initialization_capture(mc_screen_transition_receipts *r,
                                                 mc_client_screen_error *error,
                                                 const mc_client_screen_init_error *owner) {
    if (r == NULL || error == NULL || owner == NULL ||
        (error->initialization != owner &&
         !(error->initialization == &r->initialization && r->initialization_source == owner)))
        return false;
    r->initialization = *owner;
    r->initialization_source = owner;
    if (r->inventory_source != NULL && r->initialization.screen.inventory == r->inventory_source)
        r->initialization.screen.inventory = &r->inventory;
    error->initialization = &r->initialization;
    return true;
}

void mc_screen_transition_initialization_after(mc_screen_transition_receipts *r,
                                               const mc_client_screen_init_error *owner) {
    if (owner != NULL) {
        r->initialization = *owner;
        if (r->inventory_source != NULL &&
            r->initialization.screen.inventory == r->inventory_source)
            r->initialization.screen.inventory = &r->inventory;
    } else if (r->initialization.screen.initialization == r->initialization_source)
        r->initialization.screen.initialization = NULL;
    r->initialization_source = owner;
}

bool mc_screen_transition_inventory_capture(mc_screen_transition_receipts *r,
                                            mc_client_screen_error *error,
                                            const mc_client_inventory_screen_error *owner) {
    if (r == NULL || error == NULL || owner == NULL)
        return false;
    const bool direct = error->inventory == owner ||
                        (error->inventory == &r->inventory && r->inventory_source == owner);
    const bool nested = error->initialization != NULL &&
                        (error->initialization->screen.inventory == owner ||
                         (error->initialization->screen.inventory == &r->inventory &&
                          r->inventory_source == owner));
    if (!direct && !nested)
        return false;
    if (nested && error->initialization != &r->initialization)
        (void)mc_screen_transition_initialization_capture(r, error, error->initialization);
    r->inventory = *owner;
    r->inventory_source = owner;
    if (direct)
        error->inventory = &r->inventory;
    if (nested)
        r->initialization.screen.inventory = &r->inventory;
    return true;
}

static void expire_container_receipt(mc_player_container_error *error) {
    error->drop.publication = NULL;
    error->drop.statistic = NULL;
}

void mc_screen_transition_inventory_after(mc_screen_transition_receipts *r,
                                          const mc_client_inventory_screen_error *owner) {
    if (owner != NULL)
        r->inventory = *owner; /* Checked BUSY may already have consumed earlier children. */
    else {
        /* These four fields contain only this Screen's owned adapter views.
         * Successful Screen retirement proves every such child has ended. */
        expire_container_receipt(&r->inventory.container);
        expire_container_receipt(&r->inventory.cleanup_container);
        expire_container_receipt(&r->inventory.inherited_close);
        expire_container_receipt(&r->inventory.direct_close);
    }
    r->inventory_source = owner;
}

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
    r->receipts.inventory_source = NULL;
    r->receipts.initialization_source = NULL;
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
    if (r->node_count == SIZE_MAX) {
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
    r->last_display_call = n->display;
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
        if (status == MC_CLIENT_DISPLAY_SCREEN_NULL_REFERENCE) {
            r->failure.source_exception = true;
            r->failure.exception_class = "java/lang/NullPointerException";
        }
        if (n->display != NULL) {
            r->failure.parent_site =
                n->display->child.class_name != NULL ? n->display->child : n->display->site;
            const mc_client_screen_error *child =
                n->display->screen.status != MC_CLIENT_SCREEN_OK ? &n->display->screen
                : n->display->initialization.screen.status != MC_CLIENT_SCREEN_OK
                    ? &n->display->initialization.screen
                    : NULL;
            if (child != NULL) {
                r->failure.native_policy = child->native_policy;
                r->failure.source_exception = child->source_exception;
                r->failure.source_error = child->source_error;
                r->failure.exception_class = child->exception_class;
                r->failure.inventory = child->inventory;
                r->failure.registry = child->registry;
                if (n->display->initialization.screen.status != MC_CLIENT_SCREEN_OK)
                    r->failure.initialization = &n->display->initialization;
            }
        }
        return false;
    }
    if (n->display->fallback == NULL && n->display->death == NULL) {
        const mc_client_display_screen_status retired =
            mc_client_display_screen_retire(&n->display);
        if (retired != MC_CLIENT_DISPLAY_SCREEN_OK) {
            fail(r, MC_CLIENT_SCREEN_BUSY, "completed display-call native retirement", type, method,
                 descriptor, pc);
            return false;
        }
        r->last_display_call = NULL;
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

static bool open_create_world(mc_client_screen_transitions *r, mc_client_source_da *parent) {
    screen_node *n = node(r);
    if (n == NULL)
        return false;
    if (mc_client_screen_create_world_allocate(r->classes, NULL, &n->create, &r->failure) !=
        MC_CLIENT_SCREEN_OK)
        return false;
    ++r->screens;
    if (mc_client_screen_create_world_construct(n->create, parent, &r->failure) !=
        MC_CLIENT_SCREEN_OK)
        return false;
    return display(r, mc_client_screen_create_world_base(n->create), "rq", "a", "(Lke;)V", 165);
}

static bool create_text(mc_client_screen_transitions *r, mc_client_screen_text text,
                        mc_world_info_name **out, unsigned int pc) {
    if (text.kind == MC_CLIENT_SCREEN_TEXT_NULL)
        return true;
    mc_player_string_view value;
    if (!mc_client_screen_text_read(text, &value) ||
        mc_world_info_name_copy((mc_nbt_string){value.units, value.length}, SIZE_MAX, out) !=
            MC_WORLD_INFO_OK) {
        fail(r, MC_CLIENT_SCREEN_STRING_FAILED, "FJ native World-call String occurrence", "fj", "a",
             "(Lke;)V", pc);
        return false;
    }
    return true;
}

static bool create_action(mc_client_screen_transitions *r, mc_client_source_fj *fj,
                          mc_client_source_ke *button) {
    if (!button->fields.g)
        return true;
    const int32_t id = button->fields.f;
    if (id == 1)
        return display(r, fj->fields.a, "fj", "a", "(Lke;)V", 24);
    if (id != 0)
        return true;
    if (!display(r, NULL, "fj", "a", "(Lke;)V", 42))
        return false;
    if (fj->fields.m)
        return true;
    fj->fields.m = true;
    fj->last_pc = 55;
    int64_t seed;
    if (!mc_screen_lock(r->classes, &r->failure))
        return false;
    const bool seeded = mc_screen_create_world_seed_locked(fj, &seed, &r->failure);
    mtx_unlock(&r->classes->mutex);
    if (!seeded)
        return false;
    mc_client_world_load_state previous = {0};
    const bool reload =
        r->last_load != NULL && mc_client_world_load_inspect(r->last_load, &previous) &&
        previous.leave.source_returned && !previous.leave.failed && previous.leave.i_cleared &&
        previous.leave.f_cleared && previous.leave.h_cleared;
    if (r->app->controller != r->controller || (r->controller != NULL && !reload)) {
        fail(r, MC_CLIENT_SCREEN_BUSY, "native earlier controller occurrence", "fj", "a", "(Lke;)V",
             119);
        return false;
    }
    screen_node *n = node(r);
    if (n == NULL)
        return false;
    mc_singleplayer_controller_status status = mc_client_singleplayer_controller_allocate(
        r->app->entities, r->app->profile, 0, &n->controller, &r->controller_error);
    if (status == MC_SINGLEPLAYER_CONTROLLER_OK)
        status = mc_client_singleplayer_controller_construct(n->controller, r->app,
                                                             &r->controller_error);
    if (status != MC_SINGLEPLAYER_CONTROLLER_OK) {
        fail(r, MC_CLIENT_SCREEN_APP_FAILED, "actual FJ OS constructor child", "fj", "a", "(Lke;)V",
             127);
        r->failure.parent_site = (mc_client_screen_site){
            r->controller_error.site.class_name, r->controller_error.site.method,
            r->controller_error.site.descriptor, r->controller_error.site.pc};
        return false;
    }
    mc_client_singleplayer_controller *candidate = n->controller;
    n->controller = r->controller;
    if (r->last_load_node != NULL)
        r->last_load_node->load_controller_slot = &n->controller;
    r->controller = candidate;
    r->app->controller = candidate;
    fj->last_pc = 130;
    if (!create_text(r, fj->fields.l, &n->create_folder, 138))
        return false;
    if (fj->fields.i == NULL) {
        fail(r, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE, "FJ actual RO.a name null receiver", "fj",
             "a", "(Lke;)V", 145);
        r->failure.native_policy = false;
        return false;
    }
    if (!create_text(r, fj->fields.i->fields.h, &n->create_name, 145))
        return false;
    fj->last_pc = 149;
    const mc_client_world_load_status loaded =
        reload ? mc_client_world_load_begin_after(r->last_load, n->create_folder, n->create_name,
                                                  seed, &n->load)
               : mc_client_world_load_begin(r->world_classes, n->create_folder, n->create_name,
                                            seed, &n->load);
    if (n->load != NULL) {
        n->load_controller_slot = &r->controller;
        r->last_load = n->load;
        r->last_load_node = n;
    }
    if (loaded != MC_CLIENT_WORLD_LOAD_OK) {
        (void)world_failure(r, loaded, n->load);
        r->failure.parent_site = (mc_client_screen_site){"fj", "a", "(Lke;)V", 149};
        return false;
    }
    if (!display(r, NULL, "fj", "a", "(Lke;)V", 157))
        return false;
    fj->last_pc = 160;
    return true;
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

mc_client_screen_status mc_client_screen_transitions_death(mc_client_screen_transitions *r,
                                                           mc_client_screen_error *e) {
    if (!enter(r, e))
        return r == NULL || e == NULL ? MC_CLIENT_SCREEN_INVALID_ARGUMENT : e->status;
    (void)display(r, NULL, "net/minecraft/client/Minecraft", "k", "()V", 177);
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

mc_client_screen_status mc_client_screen_transitions_inventory(mc_client_screen_transitions *r,
                                                               mc_client_screen_error *e) {
    if (!enter(r, e))
        return r == NULL || e == NULL ? MC_CLIENT_SCREEN_INVALID_ARGUMENT : e->status;
    screen_node *n = node(r);
    if (n != NULL) {
        if (mc_client_screen_inventory_allocate(r->classes, r->app->render_classes, NULL,
                                                &n->inventory,
                                                &r->failure) == MC_CLIENT_SCREEN_OK) {
            ++r->screens;
            mc_entity *player = r->app->h; /* Actual constructor operand read at783. */
            if (mc_client_screen_inventory_construct(n->inventory, player, &r->failure) ==
                MC_CLIENT_SCREEN_OK)
                (void)display(r, mc_client_screen_inventory_base(n->inventory),
                              "net/minecraft/client/Minecraft", "k", "()V", 789);
            else
                r->failure.parent_site =
                    (mc_client_screen_site){"net/minecraft/client/Minecraft", "k", "()V", 786};
        } else
            r->failure.parent_site =
                (mc_client_screen_site){"net/minecraft/client/Minecraft", "k", "()V", 778};
    }
    return leave(r, e);
}

mc_client_screen_status mc_client_screen_transitions_workbench(mc_client_screen_transitions *r,
                                                               mc_entity *player, int32_t x,
                                                               int32_t y, int32_t z,
                                                               mc_client_screen_error *e) {
    if (!enter(r, e))
        return r == NULL || e == NULL ? MC_CLIENT_SCREEN_INVALID_ARGUMENT : e->status;
    if (player == NULL) {
        fail(r, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE, "DC.a null actual receiver", "dc", "a",
             "(III)V", 1);
        r->failure.native_policy = false;
        r->failure.source_exception = true;
        r->failure.exception_class = "java/lang/NullPointerException";
    } else if (!mc_entity_is_kind(player, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER) ||
               player->local_player == NULL)
        fail(r, MC_CLIENT_SCREEN_INVALID_ARGUMENT, "DC.a actual LocalPlayer source storage", "dc",
             "a", "(III)V", 1);
    else {
        screen_node *n = node(r);
        if (n != NULL) {
            if (mc_entity_retain(player) != MC_ENTITY_OK)
                fail(r, MC_CLIENT_SCREEN_BUSY, "DC.a native invocation Player occurrence", NULL,
                     NULL, NULL, 0);
            else {
                n->constructor_player = player;
                /* The actual receiver at1 is captured before NEW OO4. Its
                 * NULL is dereferenced only by the final display invoke22. */
                client_application *actual = player->local_player->fields.minecraft;
                if (mc_client_screen_workbench_allocate(r->classes, r->app->render_classes, NULL,
                                                        &n->workbench,
                                                        &r->failure) != MC_CLIENT_SCREEN_OK)
                    r->failure.parent_site = (mc_client_screen_site){"dc", "a", "(III)V", 4};
                else {
                    ++r->screens;
                    mc_inventory_player *inventory = player->local_player->fields.player.c;
                    mc_world_runtime *world = player->base.world;
                    if (mc_client_screen_workbench_construct(n->workbench, inventory, world, x, y,
                                                             z, &r->failure) != MC_CLIENT_SCREEN_OK)
                        r->failure.parent_site = (mc_client_screen_site){"dc", "a", "(III)V", 19};
                    else if (actual == NULL) {
                        fail(r, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE,
                             "DC.a null captured Minecraft display receiver", "dc", "a", "(III)V",
                             22);
                        r->failure.native_policy = false;
                        r->failure.source_exception = true;
                        r->failure.exception_class = "java/lang/NullPointerException";
                    } else if (actual != r->app)
                        fail(r, MC_CLIENT_SCREEN_INVALID_ARGUMENT,
                             "DC.a actual captured Minecraft router identity", "dc", "a", "(III)V",
                             22);
                    else
                        (void)display(r, mc_client_screen_workbench_base(n->workbench), "dc", "a",
                                      "(III)V", 22);
                }
            }
        }
    }
    return leave(r, e);
}

mc_client_screen_status mc_client_screen_transitions_player_close(mc_client_screen_transitions *r,
                                                                  mc_entity *player,
                                                                  mc_client_screen_error *e) {
    if (!enter(r, e))
        return r == NULL || e == NULL ? MC_CLIENT_SCREEN_INVALID_ARGUMENT : e->status;
    if (player == NULL || player->local_player == NULL ||
        !mc_entity_is_kind(player, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER))
        fail(r, MC_CLIENT_SCREEN_INVALID_ARGUMENT, "DC.r native concrete receiver", NULL, NULL,
             NULL, 0);
    else if (mc_entity_retain(player) != MC_ENTITY_OK)
        fail(r, MC_CLIENT_SCREEN_BUSY, "DC.r native Player occurrence", NULL, NULL, NULL, 0);
    else {
        mc_player_container_error restored;
        if (mc_player_container_restore_default(player, &restored) != MC_ENTITY_OK) {
            fail(r,
                 restored.source_exception ? MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE
                                           : MC_CLIENT_SCREEN_APP_FAILED,
                 restored.operation, restored.site.class_name, restored.site.method,
                 restored.site.descriptor, restored.site.pc);
            r->failure.native_policy = restored.native_policy;
            r->failure.source_exception = restored.source_exception;
            r->failure.source_error = restored.source_error;
            r->failure.exception_class = restored.exception_class;
            r->failure.parent_site = (mc_client_screen_site){"dc", "r", "()V", 1};
            mc_entity_release(player);
            return leave(r, e);
        }
        client_application *actual = player->local_player->fields.minecraft; /* DC.b5. */
        if (actual == NULL) {
            fail(r, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE, "DC.r null Minecraft display receiver",
                 "dc", "r", "()V", 9);
            r->failure.native_policy = false;
            r->failure.source_exception = true;
            r->failure.exception_class = "java/lang/NullPointerException";
        } else if (actual != r->app)
            fail(r, MC_CLIENT_SCREEN_INVALID_ARGUMENT, "DC.r actual Minecraft router identity",
                 "dc", "r", "()V", 9);
        else
            (void)display(r, NULL, "dc", "r", "()V", 9);
        mc_entity_release(player);
    }
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
    if (r->last_load == NULL) {
        fail(r, MC_CLIENT_SCREEN_UNIMPLEMENTED_DEPENDENCY, "actual selected World leave owner",
             "oz", "a", "(Lke;)V", 76);
        return;
    }
    mc_client_world_load_state prior = {0};
    (void)mc_client_world_load_inspect(r->last_load, &prior);
    ++r->leave_calls;
    r->leave_owner = r->last_load;
    const mc_client_world_load_status left =
        mc_client_world_load_leave(r->last_load, &r->world_leave);
    mc_client_world_load_state current = {0};
    r->world_leave_retained = mc_client_world_load_inspect(r->last_load, &current) &&
                              current.leave.calls > prior.leave.calls;
    if (left != MC_CLIENT_WORLD_LOAD_OK) {
        const mc_client_world_load_error *error = &r->world_leave;
        const mc_client_world_load_site *site =
            error->child.class_name == NULL ? &error->site : &error->child;
        fail(r, world_status(left), error->operation, site->class_name, site->method,
             site->descriptor, site->pc < 0 ? 0u : (unsigned int)site->pc);
        r->failure.native_policy = error->native_policy;
        r->failure.parent_site = (mc_client_screen_site){"oz", "a", "(Lke;)V", 76};
        return;
    }
    /* Actual NEW fu83/constructor87, owned independently of startup's FU. */
    screen_node *n = node(r);
    if (n == NULL)
        return;
    if (mc_client_screen_menu_allocate(r->classes, NULL, &n->menu, &r->failure) !=
        MC_CLIENT_SCREEN_OK)
        return;
    ++r->screens;
    if (mc_client_screen_menu_construct(n->menu, &r->failure) != MC_CLIENT_SCREEN_OK)
        return;
    (void)display(r, mc_client_screen_menu_base(n->menu), "oz", "a", "(Lke;)V", 90);
}

static mc_client_screen_status respawn_status(mc_client_respawn_status status) {
    switch (status) {
        case MC_CLIENT_RESPAWN_OK:
            return MC_CLIENT_SCREEN_OK;
        case MC_CLIENT_RESPAWN_ARGUMENT:
            return MC_CLIENT_SCREEN_INVALID_ARGUMENT;
        case MC_CLIENT_RESPAWN_BUSY:
            return MC_CLIENT_SCREEN_BUSY;
        case MC_CLIENT_RESPAWN_LIMIT:
            return MC_CLIENT_SCREEN_LIMIT;
        case MC_CLIENT_RESPAWN_ALLOCATION:
            return MC_CLIENT_SCREEN_ALLOCATION_FAILED;
        case MC_CLIENT_RESPAWN_NULL_REFERENCE:
            return MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE;
        case MC_CLIENT_RESPAWN_UNIMPLEMENTED:
            return MC_CLIENT_SCREEN_UNIMPLEMENTED_DEPENDENCY;
        default:
            return MC_CLIENT_SCREEN_APP_FAILED;
    }
}

static void death_action(mc_client_screen_transitions *r, mc_client_source_ke *button) {
    /* The id-zero test has no body. The surviving CH decisions read id at8
     * and34 independently, including after the first action changes App.r. */
    if (button->fields.f == 1) {
        screen_node *n = node(r);
        if (n == NULL)
            return;
        mc_entity *player = r->app->h; /* Actual captured Minecraft.h19 receiver. */
        const mc_client_respawn_status status =
            mc_client_respawn_player_begin(player, r->last_load, &n->respawn, &n->respawn_error);
        r->last_respawn_node = n;
        if (status != MC_CLIENT_RESPAWN_OK) {
            const mc_client_respawn_error *error = &n->respawn_error;
            const mc_client_world_load_site *site =
                error->child.class_name == NULL ? &error->site : &error->child;
            fail(r, respawn_status(status), error->operation, site->class_name, site->method,
                 site->descriptor, site->pc < 0 ? 0u : (unsigned int)site->pc);
            r->failure.native_policy = error->native_policy;
            r->failure.parent_site = (mc_client_screen_site){"ch", "a", "(Lke;)V", 22};
            return;
        }
        if (!mc_client_respawn_inspect(n->respawn, &n->completed_respawn)) {
            fail(r, MC_CLIENT_SCREEN_APP_FAILED, "ch completed respawn native receipt", "ch", "a",
                 "(Lke;)V", 22);
            return;
        }
        n->completed_respawn.failure = n->completed_respawn.cleanup = NULL;
        n->completed_respawn_captured = true;
        /* The actual source invocation returned. End its native Load lease
         * before CH's independent next display call and any later Respawn. */
        const mc_client_respawn_status retired =
            mc_client_respawn_retire(&n->respawn, &n->respawn_cleanup);
        if (retired != MC_CLIENT_RESPAWN_OK) {
            const mc_client_respawn_error *error = &n->respawn_cleanup;
            fail(r, respawn_status(retired), error->operation, "ch", "a", "(Lke;)V", 22);
            r->failure.native_policy = true;
            return;
        }
        n->respawn_error = (mc_client_respawn_error){0};
        if (!display(r, NULL, "ch", "a", "(Lke;)V", 30))
            return;
    }
    if (button->fields.f != 2)
        return;
    if (r->last_load == NULL) {
        fail(r, MC_CLIENT_SCREEN_UNIMPLEMENTED_DEPENDENCY, "ch actual selected World leave owner",
             "ch", "a", "(Lke;)V", 46);
        return;
    }
    mc_client_world_load_state prior = {0};
    (void)mc_client_world_load_inspect(r->last_load, &prior);
    ++r->leave_calls;
    r->leave_owner = r->last_load;
    const mc_client_world_load_status left =
        mc_client_world_load_leave(r->last_load, &r->world_leave);
    mc_client_world_load_state current = {0};
    r->world_leave_retained = mc_client_world_load_inspect(r->last_load, &current) &&
                              current.leave.calls > prior.leave.calls;
    if (left != MC_CLIENT_WORLD_LOAD_OK) {
        const mc_client_world_load_error *error = &r->world_leave;
        const mc_client_world_load_site *site =
            error->child.class_name == NULL ? &error->site : &error->child;
        fail(r, world_status(left), error->operation, site->class_name, site->method,
             site->descriptor, site->pc < 0 ? 0u : (unsigned int)site->pc);
        r->failure.native_policy = error->native_policy;
        r->failure.parent_site = (mc_client_screen_site){"ch", "a", "(Lke;)V", 46};
        return;
    }
    screen_node *n = node(r); /* Actual NEW fu53 after the complete World leave. */
    if (n == NULL)
        return;
    if (mc_client_screen_menu_allocate(r->classes, NULL, &n->menu, &r->failure) !=
        MC_CLIENT_SCREEN_OK)
        return;
    ++r->screens;
    if (mc_client_screen_menu_construct(n->menu, &r->failure) != MC_CLIENT_SCREEN_OK)
        return;
    (void)display(r, mc_client_screen_menu_base(n->menu), "ch", "a", "(Lke;)V", 60);
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
    mc_client_world_load_state previous = {0};
    const bool reload =
        r->last_load != NULL && mc_client_world_load_inspect(r->last_load, &previous) &&
        previous.leave.source_returned && !previous.leave.failed && previous.leave.i_cleared &&
        previous.leave.f_cleared && previous.leave.h_cleared;
    if (r->app->controller != r->controller || (r->controller != NULL && !reload)) {
        fail(r, MC_CLIENT_SCREEN_BUSY, "native earlier controller occurrence", "rq", "e", "(I)V",
             25);
        return false;
    }
    screen_node *invocation = node(r);
    if (invocation == NULL)
        return false;
    mc_singleplayer_controller_status status = mc_client_singleplayer_controller_allocate(
        r->app->entities, r->app->profile, 0, &invocation->controller, &r->controller_error);
    if (status == MC_SINGLEPLAYER_CONTROLLER_OK)
        status = mc_client_singleplayer_controller_construct(invocation->controller, r->app,
                                                             &r->controller_error);
    if (status != MC_SINGLEPLAYER_CONTROLLER_OK) {
        fail(r, MC_CLIENT_SCREEN_APP_FAILED, "actual os constructor child", "rq", "e", "(I)V", 33);
        r->failure.parent_site = (mc_client_screen_site){
            r->controller_error.site.class_name, r->controller_error.site.method,
            r->controller_error.site.descriptor, r->controller_error.site.pc};
        return false;
    }
    mc_client_singleplayer_controller *candidate = invocation->controller;
    invocation->controller = r->controller;
    if (r->last_load_node != NULL)
        r->last_load_node->load_controller_slot = &invocation->controller;
    r->controller = candidate;
    r->app->controller = candidate; /* Actual source ob/os replacement at c36. */
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
    mc_world_info_name *folder = fields.filename == NULL ? r->fallback_filename : fields.filename;
    mc_world_info_name *name =
        fields.display_name == NULL || mc_world_info_name_text(fields.display_name).length == 0
            ? r->fallback_display
            : fields.display_name;
    const mc_client_world_load_status loaded =
        reload ? mc_client_world_load_begin_after(r->last_load, folder, name, 0, &invocation->load)
               : mc_client_world_load_begin(r->world_classes, folder, name, 0, &invocation->load);
    if (invocation->load != NULL) {
        invocation->load_controller_slot = &r->controller;
        r->last_load = invocation->load;
        r->last_load_node = invocation;
    }
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
    } else if (screen->kind == MC_CLIENT_SCREEN_DEATH) {
        death_action(r, button);
    } else if (screen->kind == MC_CLIENT_SCREEN_WORLD_SELECTION) {
        mc_client_source_rq *rq = (mc_client_source_rq *)screen;
        const int32_t id = button->fields.f;
        if (!button->fields.g) { /* Actual rq.a returns7. */
        } else if (id == 0)
            (void)display(r, rq->fields.a, "rq", "a", "(Lke;)V", 226);
        else if (id == 1)
            (void)select_body(r, rq, rq->fields.m);
        else if (id == 3)
            (void)open_create_world(r, screen);
        else if (id == 2 || id == 6)
            fail(r, MC_CLIENT_SCREEN_UNIMPLEMENTED_DEPENDENCY,
                 id == 2 ? "actual delete-confirmation preparation"
                         : "actual rename-world jk construction",
                 "rq", "a", "(Lke;)V", id == 2 ? 21u : 189u);
        else
            (void)mc_client_world_list_button(rq->fields.o, button, &r->failure);
    } else if (screen->kind == MC_CLIENT_SCREEN_CREATE_WORLD)
        (void)create_action(r, (mc_client_source_fj *)screen, button);
    return leave(r, e);
}

bool mc_client_screen_transitions_inspect(const mc_client_screen_transitions *r,
                                          mc_client_screen_transition_state *out) {
    if (r == NULL || out == NULL)
        return false;
    const mc_client_world_load_error *world_leave = r->leave_calls == 0 ? NULL : &r->world_leave;
    mc_client_world_load_state load;
    if (r->world_leave_retained && mc_client_world_load_inspect(r->leave_owner, &load))
        world_leave = load.leave.failure; /* Fresh view after any child cleanup. */
    *out = (mc_client_screen_transition_state){
        .active = r->active,
        .retiring = r->retiring,
        .slot_handed_off = r->slot_handed_off,
        .screens = r->screens,
        .calls = r->calls,
        .initial_screen = r->initial,
        .failure = &r->failure,
        .display_call = r->last_display_call,
        .world_load = r->last_load,
        .world_leave = world_leave,
        .leave_calls = r->leave_calls,
        .respawn = r->last_respawn_node == NULL ? NULL : r->last_respawn_node->respawn,
        .respawn_error = r->last_respawn_node == NULL || r->last_respawn_node->respawn == NULL
                             ? NULL
                             : &r->last_respawn_node->respawn_error,
        .completed_respawn =
            r->last_respawn_node == NULL || !r->last_respawn_node->completed_respawn_captured
                ? NULL
                : &r->last_respawn_node->completed_respawn,
        .terminal_prepare_selected = r->terminal_prepare_selected,
        .terminal_prepared = r->terminal_prepared};
    return true;
}

mc_client_screen_classes *
mc_client_screen_transitions_classes(const mc_client_screen_transitions *r) {
    return r == NULL ? NULL : r->classes;
}

static bool prepare_inventory_screen(mc_client_screen_transitions *r, mc_client_source_id *screen,
                                     mc_client_screen_error *e) {
    const bool copied =
        mc_screen_transition_inventory_capture(&r->receipts, &r->failure, &screen->detail);
    const bool returned = mc_screen_container_terminal_prepare_locked(screen, e);
    if (copied)
        mc_screen_transition_inventory_after(&r->receipts, &screen->detail);
    return returned;
}

static bool inventory_terminal_prefix(mc_client_screen_transitions *r, mc_client_screen_error *e) {
    if (!mc_screen_lock(r->classes, e))
        return false;
    bool ready = true;
    for (screen_node *n = r->nodes; n != NULL; n = n->next) {
        if ((n->inventory != NULL && !prepare_inventory_screen(r, &n->inventory->container, e)) ||
            (n->workbench != NULL && !prepare_inventory_screen(r, &n->workbench->container, e))) {
            ready = false;
            break;
        }
        if (n->constructor_player != NULL) {
            mc_entity_release(n->constructor_player);
            n->constructor_player = NULL;
        }
    }
    mtx_unlock(&r->classes->mutex);
    return ready;
}

static bool root_failure(mc_client_screen_error *e, const char *operation) {
    mc_screen_fail(e, MC_CLIENT_SCREEN_BUSY, operation, NULL, NULL, NULL, 0);
    e->native_policy = true;
    return false;
}

static bool root_epoch_ledger(mc_client_screen_transitions *r, mc_client_screen_error *e) {
    size_t nodes = 0, loads = 0;
    const mc_client_world_load_call *newer = NULL;
    for (const screen_node *n = r->nodes; n != NULL; n = n->next) {
        if (nodes++ == r->node_count)
            return root_failure(e, "OO terminal complete transition node ledger");
        if (n->load == NULL)
            continue;
        if (newer == NULL && (n != r->last_load_node || n->load != r->last_load))
            return root_failure(e, "OO terminal actual latest Load owner");
        mc_world_storage_error error;
        if (mc_client_world_load_terminal_owned_follows(n->load, newer, &error) !=
            MC_WORLD_STORAGE_OK)
            return root_failure(e, error.operation);
        newer = n->load;
        ++loads;
    }
    if (nodes != r->node_count || loads == 0)
        return root_failure(e, "OO terminal represented retained epoch graph");
    mc_world_storage_error error;
    if (mc_client_world_load_classes_terminal_validate(r->world_classes, r->app, loads, &error) !=
        MC_WORLD_STORAGE_OK)
        return root_failure(e, error.operation);
    return true;
}

static bool root_screen_epoch(mc_client_screen_transitions *r, const screen_node *owner,
                              mc_client_world_load_state *out, mc_client_screen_error *e) {
    const mc_client_source_id *screen = &owner->workbench->container;
    if (owner->constructor_player == NULL ||
        owner->constructor_player != screen->constructor_player ||
        screen->base.classes != r->classes || screen->base.fields.b != r->app)
        return root_failure(e, "OO terminal actual DC constructor and App occurrence");
    size_t matches = 0;
    for (const screen_node *n = r->nodes; n != NULL; n = n->next) {
        if (n->load == NULL)
            continue;
        mc_client_world_load_state state;
        if (!mc_client_world_load_inspect(n->load, &state))
            return root_failure(e, "OO terminal actual retained Load inspection");
        if (state.player != screen->constructor_player || state.world != screen->constructor_world)
            continue;
        if (state.app != r->app || state.active || !state.source_returned ||
            !state.world_constructed || !state.world_published || !state.player_published ||
            !state.input_published || !state.view_published || state.player == NULL ||
            state.world == NULL)
            return root_failure(e, "OO terminal completed selected epoch Player and World");
        *out = state;
        ++matches;
    }
    return matches == 1 || root_failure(e, "OO terminal unique actual Player World epoch");
}

static bool root_inventory_terminal_prefix(mc_client_screen_transitions *r,
                                           mc_client_screen_error *e) {
    if (!mc_screen_lock(r->classes, e))
        return false;
    bool ready = false, live = false;
    size_t nodes = 0;
    for (const screen_node *n = r->nodes; n != NULL; n = n->next) {
        if (nodes++ == r->node_count) {
            (void)root_failure(e, "OO terminal bounded transition node ledger");
            goto done;
        }
        if (n->workbench != NULL) {
            mc_client_source_id *screen = &n->workbench->container;
            mc_client_screen_error probe = {0};
            if (screen->terminal_handoff_done ||
                !mc_screen_container_retirement_admit_locked(screen, &probe))
                live = true;
        }
    }
    if (nodes != r->node_count) {
        (void)root_failure(e, "OO terminal complete transition node ledger");
        goto done;
    }
    if (live && !root_epoch_ledger(r, e))
        goto done;
    /* Known retained UE captures must end before the exact AA/IT/IX proof.
     * No caller supplies a guessed allowance for those old AA holds. */
    for (screen_node *n = r->nodes; n != NULL; n = n->next)
        if (n->inventory != NULL && !prepare_inventory_screen(r, &n->inventory->container, e))
            goto done;
    /* Likewise, old closed/failed OO owners still bind that same IX. Keep
     * genuinely published ITs for the separate exact-root handoff below. */
    for (screen_node *n = r->nodes; n != NULL; n = n->next) {
        if (n->workbench == NULL) {
            if (n->constructor_player != NULL) {
                mc_entity_release(n->constructor_player);
                n->constructor_player = NULL;
            }
            continue;
        }
        mc_client_source_id *screen = &n->workbench->container;
        mc_client_screen_error probe = {0};
        if (screen->terminal_handoff_done ||
            !mc_screen_container_retirement_admit_locked(screen, &probe)) {
            continue;
        }
        if (!prepare_inventory_screen(r, screen, e))
            goto done;
        if (n->constructor_player != NULL) {
            mc_entity_release(n->constructor_player);
            n->constructor_player = NULL;
        }
    }
    for (screen_node *n = r->nodes; n != NULL; n = n->next) {
        if (n->workbench == NULL)
            continue;
        mc_client_source_id *screen = &n->workbench->container;
        if (!screen->terminal_handoff_done && !screen->container_owned)
            continue;
        mc_client_world_load_state epoch = {0};
        if (!screen->terminal_handoff_done && !root_screen_epoch(r, n, &epoch, e))
            goto done;
        const bool copied =
            mc_screen_transition_inventory_capture(&r->receipts, &r->failure, &screen->detail);
        const bool prepared = mc_screen_container_terminal_handoff_locked(
            screen, r->terminal_root, epoch.player, r->app->registry, epoch.world, e);
        if (copied)
            mc_screen_transition_inventory_after(&r->receipts, &screen->detail);
        if (!prepared)
            goto done;
        if (n->constructor_player != NULL) {
            mc_entity_release(n->constructor_player);
            n->constructor_player = NULL;
        }
    }
    ready = true;
done:
    mtx_unlock(&r->classes->mutex);
    return ready;
}

static bool retire_respawns(mc_client_screen_transitions *r, mc_client_screen_error *e) {
    for (screen_node *n = r->nodes; n != NULL; n = n->next) {
        const mc_client_respawn_status status =
            mc_client_respawn_retire(&n->respawn, &n->respawn_cleanup);
        if (status != MC_CLIENT_RESPAWN_OK) {
            mc_screen_fail(
                e, respawn_status(status), n->respawn_cleanup.operation,
                n->respawn_cleanup.site.class_name, n->respawn_cleanup.site.method,
                n->respawn_cleanup.site.descriptor,
                n->respawn_cleanup.site.pc < 0 ? 0u : (unsigned int)n->respawn_cleanup.site.pc);
            return false;
        }
        if (r->last_respawn_node == n)
            r->last_respawn_node = NULL;
    }
    return true;
}

static mc_client_screen_status terminal_prepare(mc_client_screen_transitions *r,
                                                const mc_client_bootstrap_terminal_scope *scope,
                                                mc_client_screen_error *e) {
    if (e == NULL)
        return MC_CLIENT_SCREEN_INVALID_ARGUMENT;
    *e = (mc_client_screen_error){0};
    if (r == NULL)
        return mc_screen_fail(e, MC_CLIENT_SCREEN_INVALID_ARGUMENT,
                              "native screen terminal preparation receiver", NULL, NULL, NULL, 0);
    if (scope != NULL &&
        (!mc_client_bootstrap_terminal_scope_matches(scope, r->app) ||
         r->app->screen_transitions != r || r->classes == NULL || r->world_classes == NULL ||
         (r->terminal_root != NULL && r->terminal_root != scope))) {
        (void)root_failure(e, "screen terminal actual bootstrap root authority");
        return e->status;
    }
    if (r->active)
        return mc_screen_fail(e, MC_CLIENT_SCREEN_BUSY, "active native screen terminal preparation",
                              NULL, NULL, NULL, 0);
    if (r->retiring && !r->terminal_prepare_selected)
        return mc_screen_fail(e, MC_CLIENT_SCREEN_BUSY, "legacy screen retirement already entered",
                              NULL, NULL, NULL, 0);
    if (!retire_respawns(r, e))
        return e->status;
    if (r->world_classes != NULL) {
        const mc_client_world_load_status ended =
            mc_client_world_load_classes_terminal_end(r->world_classes);
        if (ended != MC_CLIENT_WORLD_LOAD_OK)
            return mc_screen_fail(e, world_status(ended),
                                  "World-load class terminal admission ending", NULL, NULL, NULL,
                                  0);
    }
    r->retiring = r->terminal_prepare_selected = true;
    if (scope != NULL)
        r->terminal_root = scope;
    if (r->terminal_prepared)
        return MC_CLIENT_SCREEN_OK;
    r->active = true;
    if (!(scope == NULL ? inventory_terminal_prefix(r, e) : root_inventory_terminal_prefix(r, e))) {
        r->active = false;
        return e->status;
    }
    for (screen_node *n = r->nodes; n != NULL; n = n->next) {
        if (n->load != NULL &&
            mc_client_world_load_terminal_records(n->load) != MC_CLIENT_WORLD_LOAD_OK) {
            r->active = false;
            return mc_screen_fail(e, MC_CLIENT_SCREEN_BUSY,
                                  "all epoch native call records before N terminal detach", NULL,
                                  NULL, NULL, 0);
        }
    }
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

mc_client_screen_status
mc_client_screen_transitions_terminal_prepare(mc_client_screen_transitions *r,
                                              mc_client_screen_error *e) {
    return terminal_prepare(r, NULL, e);
}

mc_client_screen_status
mc_client_screen_transitions_terminal_prepare_root(mc_client_screen_transitions *r,
                                                   const mc_client_bootstrap_terminal_scope *scope,
                                                   mc_client_screen_error *e) {
    if (scope == NULL) {
        if (e == NULL)
            return MC_CLIENT_SCREEN_INVALID_ARGUMENT;
        (void)root_failure(e, "screen terminal selected bootstrap scope required");
        return e->status;
    }
    return terminal_prepare(r, scope, e);
}

static bool inventory_add(mc_world_terminal_inventory *sum,
                          const mc_world_terminal_inventory *one) {
#define ADD(field)                                                                                 \
    do {                                                                                           \
        if (one->field > SIZE_MAX - sum->field)                                                    \
            return false;                                                                          \
        sum->field += one->field;                                                                  \
    } while (0)
    ADD(entities);
    ADD(tile_entities);
    ADD(entity_occurrences);
    ADD(tile_entity_occurrences);
    ADD(expected_world_bindings);
    ADD(paths);
    ADD(path_occurrences);
    ADD(path_arenas);
#undef ADD
    sum->piston_discard_required |= one->piston_discard_required;
    return true;
}

mc_world_storage_status mc_client_screen_transitions_terminal_admit(
    const mc_client_screen_transitions *r, const mc_world_terminal_limits *limits,
    mc_world_terminal_inventory *out, mc_world_storage_error *error) {
    if (r == NULL || out == NULL) {
        if (error != NULL)
            *error =
                (mc_world_storage_error){MC_WORLD_STORAGE_INVALID_ARGUMENT,
                                         "screen terminal admission owner and output required", 0};
        return MC_WORLD_STORAGE_INVALID_ARGUMENT;
    }
    if (r->active || !r->terminal_prepared || !r->terminal_prepare_selected || !r->retiring ||
        r->world_classes == NULL || r->last_load == NULL || r->controller == NULL) {
        const mc_world_storage_status status =
            r->active ? MC_WORLD_STORAGE_BUSY : MC_WORLD_STORAGE_UNSUPPORTED_STATE;
        if (error != NULL)
            *error = (mc_world_storage_error){
                status,
                r->active ? "screen terminal admission operation entered"
                          : "screen terminal admission prepared selected-load scope",
                0};
        return status;
    }
    mc_client_terminal_storage_scope scope;
    mc_world_storage_status status =
        mc_client_screen_transitions_terminal_classify(r, &scope, error);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    mc_world_terminal_inventory sum = {0};
    if (scope != MC_CLIENT_TERMINAL_SELECTED_LOAD) {
        if (error != NULL)
            *error = (mc_world_storage_error){
                MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                "screen terminal epoch admission requires actual selected storage", 0};
        return MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    }
    /* All World audits remain passive; scratch is released between retained
     * real epochs. No earlier World is destroyed if a later audit refuses. */
    for (const screen_node *n = r->nodes; n != NULL; n = n->next) {
        if (n->load == NULL)
            continue;
        status = mc_client_world_load_terminal_classify(n->load, r->world_classes, &scope, error);
        if (status != MC_WORLD_STORAGE_OK)
            return status;
        if (scope == MC_CLIENT_TERMINAL_EMPTY_STORAGE)
            continue;
        mc_client_world_load_state state;
        if (!mc_client_world_load_inspect(n->load, &state))
            return MC_WORLD_STORAGE_BUSY;
        mc_client_singleplayer_controller *const *slot = n->load_controller_slot;
        if (slot == NULL || *slot != state.controller) {
            if (error != NULL)
                *error = (mc_world_storage_error){
                    MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                    "screen terminal exact epoch controller owning slot", 0};
            return MC_WORLD_STORAGE_UNSUPPORTED_STATE;
        }
        mc_world_terminal_inventory one;
        status = mc_client_world_load_terminal_admit(n->load, slot, limits, &one, error);
        if (status != MC_WORLD_STORAGE_OK)
            return status;
        if (!inventory_add(&sum, &one)) {
            if (error != NULL)
                *error = (mc_world_storage_error){MC_WORLD_STORAGE_LIMIT,
                                                  "screen terminal epoch inventory sum", 0};
            return MC_WORLD_STORAGE_LIMIT;
        }
    }
    *out = sum;
    if (error != NULL)
        *error = (mc_world_storage_error){0};
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status terminal_classification_result(mc_world_storage_error *error,
                                                              mc_world_storage_status status,
                                                              const char *operation) {
    if (error != NULL)
        *error = (mc_world_storage_error){status, operation, 0};
    return status;
}

mc_world_storage_status
mc_client_screen_transitions_terminal_classify(const mc_client_screen_transitions *r,
                                               mc_client_terminal_storage_scope *out,
                                               mc_world_storage_error *error) {
    if (r == NULL || out == NULL)
        return terminal_classification_result(
            error, MC_WORLD_STORAGE_INVALID_ARGUMENT,
            "screen terminal classification owner/output required");
    if (r->active)
        return terminal_classification_result(error, MC_WORLD_STORAGE_BUSY,
                                              "screen terminal classification operation entered");
    if (!r->terminal_prepared || !r->terminal_prepare_selected || !r->retiring || r->app == NULL ||
        r->app->screen_transitions != r || r->classes == NULL || r->world_classes == NULL ||
        r->app->controller != r->controller)
        return terminal_classification_result(
            error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
            "screen terminal classification prepared owner scope");
    size_t nodes = 0, loads = 0, selected = 0;
    bool last_load_found = r->last_load == NULL;
    const mc_client_world_load_call *newer = NULL;
    for (const screen_node *n = r->nodes; n != NULL; n = n->next) {
        if (nodes == r->node_count)
            return terminal_classification_result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                                  "screen terminal classification node ledger");
        ++nodes;
        if (n->load == NULL)
            continue;
        /* Nodes and Load links share their actual newest-to-oldest order.
         * Reciprocal links reject missing, repeated or reordered owning slots
         * without rescanning the preceding screen history for each Load. */
        const mc_world_storage_status ordered =
            mc_client_world_load_terminal_follows(n->load, newer, error);
        if (ordered != MC_WORLD_STORAGE_OK)
            return ordered;
        if (newer == NULL && (n->load != r->last_load || n != r->last_load_node))
            return terminal_classification_result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                                  "screen terminal latest Load owner");
        newer = n->load;
        ++loads; /* loads <= nodes <= node_count, checked before each node. */
        if (n->load == r->last_load)
            last_load_found = true;
        mc_client_terminal_storage_scope scope;
        const mc_world_storage_status status =
            mc_client_world_load_terminal_classify(n->load, r->world_classes, &scope, error);
        if (status != MC_WORLD_STORAGE_OK)
            return status;
        if (scope == MC_CLIENT_TERMINAL_SELECTED_LOAD) {
            ++selected;
        } else if (scope != MC_CLIENT_TERMINAL_EMPTY_STORAGE)
            return terminal_classification_result(
                error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                "screen terminal classification unknown Load scope");
    }
    if (nodes != r->node_count || !last_load_found ||
        (r->last_load == NULL && r->last_load_node != NULL))
        return terminal_classification_result(
            error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
            "screen terminal classification complete node ledger");
    const mc_world_storage_status status =
        mc_client_world_load_classes_terminal_validate(r->world_classes, r->app, loads, error);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    if (selected != 0) {
        if (r->controller == NULL)
            return terminal_classification_result(
                error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                "screen terminal selected epoch controller scope");
        *out = MC_CLIENT_TERMINAL_SELECTED_LOAD;
    } else
        *out = MC_CLIENT_TERMINAL_EMPTY_STORAGE;
    return terminal_classification_result(error, MC_WORLD_STORAGE_OK, NULL);
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
    if (!retire_respawns(r, e))
        return e->status;
    if (!inventory_terminal_prefix(r, e))
        return e->status;
    while (r->nodes != NULL) {
        screen_node *n = r->nodes;
        mc_client_world_load_call *load = n->load;
        const mc_client_world_load_status loaded = mc_client_world_load_retire(&n->load);
        if (loaded != MC_CLIENT_WORLD_LOAD_OK)
            return mc_screen_fail(e, world_status(loaded), "retained actual World-load prefix",
                                  NULL, NULL, NULL, 0);
        if (load != NULL && r->last_load == load) {
            r->last_load = NULL;
            r->last_load_node = NULL;
        }
        n->load_controller_slot = NULL;
        if (load != NULL && r->leave_owner == load) {
            r->leave_owner = NULL;
            r->world_leave = (mc_client_world_load_error){0};
            r->leave_calls = 0;
            r->world_leave_retained = false;
        }
        const bool copied_initialization = mc_screen_transition_initialization_capture(
            &r->receipts, &r->failure, n->display == NULL ? NULL : &n->display->initialization);
        const bool last_display =
            r->last_display_call != NULL && r->last_display_call == n->display;
        const mc_client_display_screen_status display_status =
            mc_client_display_screen_retire(&n->display);
        if (copied_initialization)
            mc_screen_transition_initialization_after(
                &r->receipts, n->display == NULL ? NULL : &n->display->initialization);
        if (display_status != MC_CLIENT_DISPLAY_SCREEN_OK)
            return mc_screen_fail(e, MC_CLIENT_SCREEN_BUSY, "retained actual display prefix", NULL,
                                  NULL, NULL, 0);
        if (last_display)
            r->last_display_call = NULL;
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
        base = mc_client_screen_workbench_base(n->workbench);
        const bool copied_workbench = mc_screen_transition_inventory_capture(
            &r->receipts, &r->failure,
            n->workbench == NULL ? NULL : &n->workbench->container.detail);
        const mc_client_screen_status workbench =
            mc_client_screen_workbench_retire(&n->workbench, e);
        if (copied_workbench)
            mc_screen_transition_inventory_after(
                &r->receipts, n->workbench == NULL ? NULL : &n->workbench->container.detail);
        if (workbench != MC_CLIENT_SCREEN_OK)
            return workbench;
        if (base != NULL && r->app->r == base)
            r->app->r = NULL;
        base = mc_client_screen_inventory_base(n->inventory);
        const bool copied_inventory = mc_screen_transition_inventory_capture(
            &r->receipts, &r->failure,
            n->inventory == NULL ? NULL : &n->inventory->container.detail);
        const mc_client_screen_status inventory =
            mc_client_screen_inventory_retire(&n->inventory, e);
        if (copied_inventory)
            mc_screen_transition_inventory_after(
                &r->receipts, n->inventory == NULL ? NULL : &n->inventory->container.detail);
        if (inventory != MC_CLIENT_SCREEN_OK)
            return inventory;
        if (base != NULL && r->app->r == base)
            r->app->r = NULL;
        base = mc_client_screen_create_world_base(n->create);
        const mc_client_screen_status create = mc_client_screen_create_world_retire(&n->create, e);
        if (create != MC_CLIENT_SCREEN_OK)
            return create;
        if (base != NULL && r->app->r == base)
            r->app->r = NULL;
        base = mc_client_screen_menu_base(n->menu);
        const mc_client_screen_status menu = mc_client_screen_menu_retire(&n->menu, e);
        if (menu != MC_CLIENT_SCREEN_OK)
            return menu;
        if (base != NULL && r->app->r == base)
            r->app->r = NULL;
        mc_world_info_name_release(n->create_folder);
        mc_world_info_name_release(n->create_name);
        n->create_folder = n->create_name = NULL;
        r->nodes = n->next;
        if (n->controller != NULL) {
            n->next = r->controller_nodes;
            r->controller_nodes = n; /* Same native record, no cleanup allocation. */
        } else
            free(n);
    }
    while (r->controller_nodes != NULL) {
        screen_node *n = r->controller_nodes;
        if (mc_client_singleplayer_controller_free(&n->controller, &r->controller_error) !=
            MC_SINGLEPLAYER_CONTROLLER_OK)
            return mc_screen_fail(e, MC_CLIENT_SCREEN_BUSY,
                                  "displaced/candidate actual controller retirement", NULL, NULL,
                                  NULL, 0);
        r->controller_nodes = n->next;
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
