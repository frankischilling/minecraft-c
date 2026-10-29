#include "application_construction_internal.h"
#include "mc/block/registry/runtime.h"
#include <stdlib.h>
#include <string.h>

static mc_client_application_construction_status
cleanup_failed(mc_client_application_construction *m, mc_client_application_construction_error *e,
               mc_client_application_child child, bool busy, const char *operation,
               mc_client_application_construction_error *out) {
    const mc_client_application_construction_status status =
        busy ? MC_CLIENT_APP_BUSY : MC_CLIENT_APP_CHILD_FAILED;
    application_constructor_error(e, status, child, -1, operation);
    application_constructor_copy_error(out, e);
    m->retirement_active = false;
    return status;
}

mc_client_application_construction_status
mc_client_application_constructor_retire(client_application **slot,
                                         mc_client_application_construction_error *out) {
    if (out != NULL) {
        memset(out, 0, sizeof(*out));
        out->minecraft_pc = -1;
    }
    if (slot == NULL)
        return application_constructor_error(out, MC_CLIENT_APP_INVALID_ARGUMENT,
                                             MC_CLIENT_APP_CHILD_NONE, -1,
                                             "actual App retirement slot");
    client_application *app = *slot;
    if (app == NULL) {
        if (out != NULL) {
            memset(out, 0, sizeof(*out));
            out->minecraft_pc = -1;
        }
        return MC_CLIENT_APP_OK;
    }
    /* The startup record can retain failed unpublished children that borrow
     * this actual storage. Reject before changing phase/stopping kg/freeing App. */
    if (app->startup != NULL)
        return application_constructor_error(out, MC_CLIENT_APP_BUSY, MC_CLIENT_APP_CHILD_NONE, -1,
                                             "actual App startup record still attached");
    if (mc_client_application_resource_thread_borrows_pending(app))
        return application_constructor_error(out, MC_CLIENT_APP_BUSY, MC_CLIENT_APP_CHILD_NONE, -1,
                                             "actual resource thread still borrows App");
    if (mc_client_application_screen_borrows_pending(app))
        return application_constructor_error(out, MC_CLIENT_APP_BUSY, MC_CLIENT_APP_CHILD_NONE, -1,
                                             "actual screen or GUI particles still borrow App");
    if (mc_client_application_effect_borrows_pending(app))
        return application_constructor_error(out, MC_CLIENT_APP_BUSY, MC_CLIENT_APP_CHILD_NONE, -1,
                                             "actual compass/watch effects still borrow App");
    mc_client_application_construction *m = app->construction;
    if (m == NULL)
        return application_constructor_error(
            out, MC_CLIENT_APP_INVALID_PHASE, MC_CLIENT_APP_CHILD_NONE, -1,
            "component frontend is not a source constructor owner");
    if (app->constructor_phase == MC_CLIENT_APP_CONSTRUCTING || m->retirement_active ||
        app->session_constructing || app->stats_writer_constructing ||
        app->water_effect_constructing || app->effect_renderer_constructing ||
        app->loading_screen_constructing || app->font_renderer_constructing ||
        app->statistics_retiring)
        return application_constructor_error(out, MC_CLIENT_APP_BUSY, MC_CLIENT_APP_CHILD_NONE, -1,
                                             "actual App constructor active");
    m->retirement_active = true;
    app->constructor_phase = MC_CLIENT_APP_RETIRING;
    mc_client_application_construction_error *e = &m->retirement;
    memset(e, 0, sizeof(*e));
    e->minecraft_pc = -1;
    /* Native cancellation/join does not write either source volatile field. */
    if (m->worker != NULL) {
        const mc_client_application_owner_status status =
            mc_client_timer_hack_retire(&m->worker, &e->cause.owner);
        if (status != MC_CLIENT_APPLICATION_OWNER_OK)
            return cleanup_failed(m, e, MC_CLIENT_APP_CHILD_WORKER,
                                  status == MC_CLIENT_APPLICATION_OWNER_BUSY,
                                  "actual kg native stop/join/retirement", out);
    }
    /* These are later source owners/callers, not constructor children. Require
     * their genuine separate retirement rather than leak them or infer an
     * inspection session/renderer as the actual source graph. */
    if (app->display != NULL || app->splash.units != NULL || app->active_world != NULL ||
        app->h != NULL || app->i != NULL || app->controller != NULL ||
        app->effect_renderer != NULL || app->render_engine != NULL || app->font_renderer != NULL ||
        app->stats_writer != NULL || app->session != NULL || app->game_directory != NULL ||
        app->diagnostic_game_directory != NULL || app->settings != NULL || app->D != NULL ||
        app->g != NULL || app->v != NULL || app->t != NULL || app->U != NULL || app->C != NULL ||
        app->aa != NULL || app->S != NULL || app->r != NULL || app->l != NULL || app->ab != NULL ||
        app->y != NULL || app->stats_writer_retained != NULL ||
        app->effect_renderer_retained != NULL || app->font_renderer_retained != NULL ||
        app->session_classes != NULL || app->world_session != NULL || app->world_view != NULL ||
        app->main_menu != NULL || app->assets != NULL || app->files != NULL ||
        app->materials != NULL || app->world_classes != NULL || app->trig != NULL ||
        app->screen_transitions != NULL) {
        application_constructor_error(e, MC_CLIENT_APP_UNIMPLEMENTED, MC_CLIENT_APP_CHILD_PLATFORM,
                                      -1,
                                      "later source owners require their own native retirement");
        application_constructor_copy_error(out, e);
        m->retirement_active = false;
        return MC_CLIENT_APP_UNIMPLEMENTED;
    }
    /* A retained mk really borrows this App. Keep our occurrence until it can
     * be final-released; dropping it while external holds remain would dangle b. */
    if (app->loading_screen_retained != NULL) {
        mc_client_loading_state state;
        if (!mc_client_loading_screen_inspect(app->loading_screen_retained, &state) ||
            state.holds != 1 || state.active) {
            e->cause.loading.status = MC_CLIENT_LOADING_BUSY;
            return cleanup_failed(m, e, MC_CLIENT_APP_CHILD_LOADING, true,
                                  "actual mk App borrow still live", out);
        }
        const mc_client_loading_status status =
            mc_client_loading_screen_release(app->loading_screen_retained);
        if (status != MC_CLIENT_LOADING_OK) {
            e->cause.loading.status = status;
            return cleanup_failed(m, e, MC_CLIENT_APP_CHILD_LOADING,
                                  status == MC_CLIENT_LOADING_BUSY, "actual mk retirement", out);
        }
        app->loading_screen_retained = NULL;
        app->loading_screen = NULL; /* Native source view ended on RETIRING. */
    }
    if (m->notice_candidate != NULL || app->achievement_notice != NULL) {
        mc_client_achievement_notice **owner =
            m->notice_candidate != NULL ? &m->notice_candidate : &app->achievement_notice;
        const mc_client_notice_status status =
            mc_client_achievement_notice_free(owner, &e->cause.notice);
        if (status != MC_CLIENT_NOTICE_OK)
            return cleanup_failed(m, e, MC_CLIENT_APP_CHILD_NOTICE, status == MC_CLIENT_NOTICE_BUSY,
                                  "actual vu retained owner retirement", out);
    }
    if (m->biped_candidate != NULL || app->x != NULL) {
        mc_client_biped_owner **owner = m->biped_candidate != NULL ? &m->biped_candidate : &app->x;
        const mc_client_model_status status = mc_client_biped_release(owner, &e->cause.biped);
        if (status != MC_CLIENT_MODEL_OK)
            return cleanup_failed(m, e, MC_CLIENT_APP_CHILD_BIPED, status == MC_CLIENT_MODEL_BUSY,
                                  "actual distinct fh occurrence retirement", out);
    }
    if (m->sound_candidate != NULL || app->B != NULL) {
        mc_sound_manager **owner = m->sound_candidate != NULL ? &m->sound_candidate : &app->B;
        const mc_sound_status status = mc_sound_manager_retire(owner, &e->cause.sound_raw);
        if (status != MC_SOUND_OK)
            return cleanup_failed(m, e, MC_CLIENT_APP_CHILD_SOUND, status == MC_SOUND_BUSY,
                                  "actual yo candidate/manager retirement", out);
    }
    if (app->water_effect_retained != NULL) {
        const mc_water_texture_status status =
            mc_client_water_texture_retire(&app->water_effect_retained, &e->cause.water);
        if (status != MC_WATER_TEXTURE_OK)
            return cleanup_failed(m, e, MC_CLIENT_APP_CHILD_WATER, status == MC_WATER_TEXTURE_BUSY,
                                  "actual vs retirement", out);
        app->water_effect = NULL;
    }
    if (m->lava_candidate != NULL || app->ae != NULL) {
        mc_client_lava_texture_effect **owner =
            m->lava_candidate != NULL ? &m->lava_candidate : &app->ae;
        const mc_lava_texture_status status = mc_client_lava_texture_retire(owner, &e->cause.lava);
        if (status != MC_LAVA_TEXTURE_OK)
            return cleanup_failed(m, e, MC_CLIENT_APP_CHILD_LAVA, status == MC_LAVA_TEXTURE_BUSY,
                                  "actual cg retirement", out);
    }
    if (m->timer_candidate != NULL || app->timer != NULL) {
        mc_timer **owner = m->timer_candidate != NULL ? &m->timer_candidate : &app->timer;
        const mc_timer_construction_status status = mc_timer_retire(owner, &e->cause.timer);
        if (status != MC_TIMER_CONSTRUCTION_OK)
            return cleanup_failed(m, e, MC_CLIENT_APP_CHILD_TIMER,
                                  status == MC_TIMER_CONSTRUCTION_BUSY, "actual py retirement",
                                  out);
    }
    if (app->K != NULL) {
        mc_player_string_release(app->K);
        app->K = NULL;
    }
    if (m->gui_hold) {
        const mc_client_loading_status status = mc_client_loading_classes_release(m->gui);
        if (status != MC_CLIENT_LOADING_OK) {
            e->cause.loading.status = status;
            return cleanup_failed(m, e, MC_CLIENT_APP_CHILD_LOADING,
                                  status == MC_CLIENT_LOADING_BUSY,
                                  "actual GUI class occurrence retirement", out);
        }
        m->gui_hold = false;
        m->gui = NULL;
    }
    if (m->render_hold) {
        e->cause.model_status = mc_client_render_classes_release(app->render_classes);
        if (e->cause.model_status != MC_CLIENT_MODEL_OK)
            return cleanup_failed(m, e, MC_CLIENT_APP_CHILD_RENDER_CLASSES,
                                  e->cause.model_status == MC_CLIENT_MODEL_BUSY,
                                  "actual render class occurrence retirement", out);
        m->render_hold = false;
        app->render_classes = NULL;
    }
    if (m->entity_hold) {
        e->cause.entity_status = mc_entity_classes_release(app->entities);
        if (e->cause.entity_status != MC_ENTITY_OK)
            return cleanup_failed(m, e, MC_CLIENT_APP_CHILD_ENTITY,
                                  e->cause.entity_status == MC_ENTITY_BUSY,
                                  "actual Entity class occurrence retirement", out);
        m->entity_hold = false;
        app->entities = NULL;
    }
    if (m->registry_hold) {
        const mc_registry_status status =
            mc_registry_classes_release(app->registry, &e->cause.registry);
        if (status != MC_REGISTRY_OK)
            return cleanup_failed(m, e, MC_CLIENT_APP_CHILD_REGISTRY, status == MC_REGISTRY_BUSY,
                                  "actual Registry occurrence retirement", out);
        m->registry_hold = false;
        app->registry = NULL;
    }
    /* No remaining cleanup needs MC class data. End exactly one non-counted
     * namespace borrow; retries do not double-drop it. Current static a may
     * still hold this actual App until the caller's terminal namespace release. */
    if (app->source_namespace_borrowed &&
        mc_client_application_namespace_detach(app, &e->cause.owner) !=
            MC_CLIENT_APPLICATION_OWNER_OK)
        return cleanup_failed(m, e, MC_CLIENT_APP_CHILD_CLASS, true,
                              "actual MC namespace borrow retirement", out);
    const mc_client_application_owner_status status =
        mc_client_application_lifetime_retire(app, &e->cause.owner);
    if (status != MC_CLIENT_APPLICATION_OWNER_OK)
        return cleanup_failed(m, e, MC_CLIENT_APP_CHILD_CLASS,
                              status == MC_CLIENT_APPLICATION_OWNER_BUSY,
                              "actual App worker/static lifetime retirement", out);
    free(m);
    free(app);
    *slot = NULL;
    if (out != NULL) {
        memset(out, 0, sizeof(*out));
        out->minecraft_pc = -1;
    }
    return MC_CLIENT_APP_OK;
}
