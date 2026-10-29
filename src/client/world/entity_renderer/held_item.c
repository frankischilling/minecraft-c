#include "../../application_state.h"
#include "internal.h"

bool entity_renderer_ra_construct(entity_renderer_context *o, mc_client_source_ra *ra,
                                  client_application *app) {
    ra->phase = MC_CLIENT_ENTITY_RENDERER_CONSTRUCTING;
    ra->b = NULL;
    ra->last_store = 6;
    ra->c = 0.0f;
    ra->last_store = 11;
    ra->d = 0.0f;
    ra->last_store = 16;
    o->failure->ra_pc = 20;
    mc_client_model_graph graph = {o->services->models,
                                   &ra->allocations,
                                   &ra->bytes,
                                   o->options->model_bytes,
                                   &o->failure->model,
                                   NULL,
                                   NULL};
    ra->e = mc_client_model_graph_blocks(&graph, "ra", 20);
    if (ra->e == NULL) {
        entity_renderer_fail(o->failure, MC_CLIENT_ENTITY_RENDERER_MODEL_FAILED,
                             "actual ra cv constructor", "ra", 20, true);
        goto failed;
    }
    ra->last_store = 27;
    ra->g = -1;
    ra->last_store = 32;
    ra->a = app;
    ra->last_store = 37;
    o->failure->ra_pc = 41;
    ra->pending_iy = entity_renderer_context_allocate(o, sizeof(*ra->pending_iy), "ra", 41);
    if (ra->pending_iy == NULL)
        goto failed;
    mc_client_source_iy *iy = ra->pending_iy;
    iy->last_store = -1;
    o->failure->ra_pc = 46;
    if (app == NULL) {
        entity_renderer_fail(o->failure, MC_CLIENT_ENTITY_RENDERER_NULL_REFERENCE,
                             "actual ra Minecraft.q receiver", "ra", 46, false);
        goto failed;
    }
    /* Actual separate argument loads46/50/54, not a px-entry snapshot. Native
     * occurrences protect the captured q/p through the entered child/cleanup. */
    mc_client_bitmap_font *font = app->font_renderer;
    if (font != NULL) {
        o->failure->font = mc_client_bitmap_font_retain(font);
        if (o->failure->font != MC_CLIENT_BITMAP_FONT_OK) {
            entity_renderer_fail(o->failure, MC_CLIENT_ENTITY_RENDERER_BUSY,
                                 "ra captured q native hold", "ra", 46, true);
            goto failed;
        }
        iy->retained_font = font;
    }
    o->failure->ra_pc = 50;
    mc_game_settings *settings = app->settings;
    o->failure->ra_pc = 54;
    mc_client_render_engine *engine = app->render_engine;
    if (engine != NULL) {
        o->failure->owner = mc_client_render_engine_retain(engine);
        if (o->failure->owner != MC_CLIENT_RENDER_ENGINE_OK) {
            entity_renderer_fail(o->failure, MC_CLIENT_ENTITY_RENDERER_BUSY,
                                 "ra captured p native hold", "ra", 54, true);
            goto failed;
        }
        iy->retained_engine = engine;
    }
    o->failure->ra_pc = 57;
    if (!entity_renderer_iy_construct(o, iy, settings))
        goto failed;
    ra->f = iy;
    ra->last_store = 60;
    ra->phase = MC_CLIENT_ENTITY_RENDERER_READY;
    return true;
failed:
    ra->phase = MC_CLIENT_ENTITY_RENDERER_FAILED;
    return false;
}

bool entity_renderer_ra_retire(entity_renderer_context *o, mc_client_source_ra *ra,
                               mc_client_entity_renderer_error *e) {
    ra->phase = MC_CLIENT_ENTITY_RENDERER_RETIRING;
    if (ra->pending_iy != NULL && !entity_renderer_iy_retire(ra->pending_iy, e))
        return false;
    ra->pending_iy = NULL;
    ra->f = NULL;
    mc_item_stack_release(ra->b_occurrence);
    ra->b_occurrence = NULL;
    ra->b = NULL; /* Native borrowed view ends, with no source field rollback. */
    mc_client_model_graph graph = {o->services->models,
                                   &ra->allocations,
                                   &ra->bytes,
                                   o->options->model_bytes,
                                   &e->model,
                                   NULL,
                                   NULL};
    if (mc_client_model_graph_retire(&graph, &e->model) != MC_CLIENT_MODEL_OK) {
        entity_renderer_fail(e, MC_CLIENT_ENTITY_RENDERER_BUSY,
                             "ra actual cv ledger native cleanup", NULL, -1, true);
        return false;
    }
    ra->e = NULL;
    return true;
}
