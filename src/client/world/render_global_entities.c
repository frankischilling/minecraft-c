#include "mc/client/world/render_global_entities.h"

#include "../../entity/entity_internal.h"
#include "../application_state.h"
#include "../render/interpolation_internal.h"
#include "../render/living_draw_internal.h"
#include "../render/manager/internal.h"
#include "../render/model_graph_internal.h"
#include "../render/tile_entity_renderer/internal.h"
#include "../render_engine/internal.h"
#include "mc/client/settings/game_settings.h"
#include "mc/core/math/math.h"
#include "mc/entity/living/sleeping.h"
#include "mc/entity/render_distance.h"
#include "render_global_internal.h"

#include <limits.h>
#include <stdlib.h>

static const char entities_descriptor[] = "(Lbt;Lyn;F)V";

struct mc_client_render_global_entities_call {
    mc_client_render_global_entities_state state;
    mc_client_render_global_entities_services services;
    mc_client_render_global_error failure, cleanup;
    mc_client_tile_renderer_error tiles, tiles_cleanup;
    mc_client_manager_error manager, manager_cleanup;
    mc_entity_render_distance_error distance;
    mc_entity_sleeping_error sleeping;
    mc_client_culling_error culling;
    mc_world_runtime *worlds[16];
    size_t world_count;
    mc_world_runtime *list_world;
    mc_entity *current;
    mc_client_render_manager *draw_th;
    mc_client_tile_renderer_dispatcher *draw_ll;
    mc_client_render_manager_draw_call *draw;
    mc_client_manager_draw_error draw_failure, draw_cleanup;
    unsigned int draw_invoke_pc;
};

static bool fail(mc_client_render_global_entities_call *c, mc_client_render_global_status status,
                 const char *operation, unsigned int pc, bool native_policy) {
    c->state.source_pc = pc;
    mc_render_global_source_fail(&c->failure, status, operation, "n", "a", entities_descriptor, pc,
                                 native_policy);
    return false;
}

static mc_client_render_global_status tile_status(mc_client_tile_renderer_status status) {
    switch (status) {
        case MC_CLIENT_TILE_RENDERER_BUSY:
            return MC_CLIENT_RENDER_GLOBAL_BUSY;
        case MC_CLIENT_TILE_RENDERER_LIMIT:
            return MC_CLIENT_RENDER_GLOBAL_LIMIT;
        case MC_CLIENT_TILE_RENDERER_ALLOCATION_FAILED:
            return MC_CLIENT_RENDER_GLOBAL_ALLOCATION_FAILED;
        case MC_CLIENT_TILE_RENDERER_NULL_REFERENCE:
            return MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE;
        case MC_CLIENT_TILE_RENDERER_UNSUPPORTED_FP:
        case MC_CLIENT_TILE_RENDERER_UNSUPPORTED_NUMERIC:
            return MC_CLIENT_RENDER_GLOBAL_UNSUPPORTED_FP;
        default:
            return MC_CLIENT_RENDER_GLOBAL_INVALID_PHASE;
    }
}

static mc_client_render_global_status manager_status(mc_client_manager_status status) {
    switch (status) {
        case MC_CLIENT_MANAGER_BUSY:
            return MC_CLIENT_RENDER_GLOBAL_BUSY;
        case MC_CLIENT_MANAGER_LIMIT:
            return MC_CLIENT_RENDER_GLOBAL_LIMIT;
        case MC_CLIENT_MANAGER_ALLOCATION_FAILED:
            return MC_CLIENT_RENDER_GLOBAL_ALLOCATION_FAILED;
        case MC_CLIENT_MANAGER_NULL_REFERENCE:
            return MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE;
        case MC_CLIENT_MANAGER_UNSUPPORTED_FP:
        case MC_CLIENT_MANAGER_UNSUPPORTED_NUMERIC:
            return MC_CLIENT_RENDER_GLOBAL_UNSUPPORTED_FP;
        case MC_CLIENT_MANAGER_UNIMPLEMENTED_DEPENDENCY:
            return MC_CLIENT_RENDER_GLOBAL_UNIMPLEMENTED_DEPENDENCY;
        default:
            return MC_CLIENT_RENDER_GLOBAL_INVALID_PHASE;
    }
}

static bool tile_failure(mc_client_render_global_entities_call *c, unsigned int pc) {
    fail(c, tile_status(c->tiles.status), c->tiles.operation, pc, c->tiles.native_policy);
    c->failure.tile_renderer = &c->tiles;
    c->failure.parent = (mc_client_graphics_site){"n", "a", entities_descriptor, (int32_t)pc};
    c->failure.child = (mc_client_graphics_site){c->tiles.source_class, c->tiles.source_method,
                                                 c->tiles.source_descriptor, c->tiles.pc};
    return false;
}

static bool manager_failure(mc_client_render_global_entities_call *c, unsigned int pc) {
    fail(c, manager_status(c->manager.status), c->manager.operation, pc, c->manager.native_policy);
    c->failure.manager = c->manager;
    c->failure.parent = (mc_client_graphics_site){"n", "a", entities_descriptor, (int32_t)pc};
    c->failure.child = (mc_client_graphics_site){c->manager.source_class, c->manager.source_method,
                                                 c->manager.source_descriptor, c->manager.pc};
    return false;
}

static void draw_views_expired(mc_client_render_global_entities_call *c) {
    c->state.draw = NULL;
    c->failure.manager_draw = NULL;
    c->cleanup.manager_draw = NULL;
    c->draw_failure.entity = c->draw_cleanup.entity = NULL;
    c->draw_failure.living = c->draw_cleanup.living = NULL;
    c->draw_failure.owner.font = c->draw_cleanup.owner.font = NULL;
    c->draw_failure.owner.acquisition = c->draw_cleanup.owner.acquisition = NULL;
}

static bool retire_draw(mc_client_render_global_entities_call *c,
                        mc_client_render_global_error *failure) {
    if (c->draw == NULL)
        return true;
    const mc_client_manager_status status =
        mc_client_render_manager_draw_retire(&c->draw, &c->draw_cleanup);
    c->state.draw = c->draw;
    if (status != MC_CLIENT_MANAGER_OK) {
        mc_render_global_source_fail(failure, MC_CLIENT_RENDER_GLOBAL_CLEANUP_FAILED,
                                     c->draw_cleanup.operation == NULL
                                         ? "N nested TH draw native retirement"
                                         : c->draw_cleanup.operation,
                                     "n", "a", entities_descriptor, c->draw_invoke_pc, true);
        failure->manager_draw = &c->draw_cleanup;
        failure->parent =
            (mc_client_graphics_site){"n", "a", entities_descriptor, (int32_t)c->draw_invoke_pc};
        failure->child = c->draw_cleanup.site;
        return false;
    }
    ++c->state.draws_retired;
    if (c->state.last_draw.valid && c->state.last_draw.attempt == c->state.draws_attempted)
        c->state.last_draw.retired = true;
    if (c->state.first_entity_draw.valid &&
        c->state.first_entity_draw.attempt == c->state.draws_attempted)
        c->state.first_entity_draw.retired = true;
    draw_views_expired(c);
    return true;
}

static bool release_draw_receiver(mc_client_render_global_entities_call *c,
                                  mc_client_render_global_error *failure) {
    if (c->draw_th == NULL ||
        mc_client_render_manager_release(&c->draw_th, &c->manager_cleanup) == MC_CLIENT_MANAGER_OK)
        return true;
    failure->manager = c->manager_cleanup;
    mc_render_global_source_fail(failure, MC_CLIENT_RENDER_GLOBAL_CLEANUP_FAILED,
                                 "N captured draw TH receiver retirement", "n", "a",
                                 entities_descriptor, c->draw_invoke_pc, true);
    return false;
}

static mc_client_render_global_model_receipt
model_snapshot(const mc_client_model_runtime_state *state) {
    return (mc_client_render_global_model_receipt){
        .identity = (uintptr_t)state->model,
        .class_name = state->runtime_class,
        .source_pc = state->site.pc < 0 ? 0u : (unsigned int)state->site.pc,
        .part_calls = state->part_calls,
        .part_returns = state->part_returns,
        .compiled_parts = state->compiled_parts,
        .faces = state->faces,
        .vertices = state->vertices,
        .returned = state->returned};
}

static void model_receipt(mc_client_render_global_draw_receipt *receipt,
                          const mc_client_model_runtime_state *state) {
    receipt->model_parts_compiled += state->compiled_parts;
    receipt->model_faces += state->faces;
    receipt->model_vertices += state->vertices;
}

static void draw_receipt(mc_client_render_global_entities_call *c,
                         const mc_client_manager_draw_state *state) {
    mc_client_render_global_draw_receipt receipt = {
        .valid = true,
        .returned = state->returned,
        .post_only = state->post_only,
        .primary_returned = state->primary_returned,
        .post_draw_returned = state->post_draw_returned,
        .attempt = c->state.draws_attempted,
        .n_invoke_pc = c->draw_invoke_pc,
        .th_source_pc = state->source_pc,
        .actor_identity = (uintptr_t)state->actor,
        .actor_class_identity = (uintptr_t)state->actor_class,
        .renderer_identity = (uintptr_t)state->renderer_identity,
        .actor_kind = -1,
        .renderer_class_name = state->renderer == NULL ? NULL : state->renderer->source_class};
    mc_entity_class_literal_view actor_class;
    if (mc_entity_class_literal_inspect(state->actor_class, &actor_class) == MC_ENTITY_OK) {
        receipt.actor_kind = (int32_t)actor_class.kind;
        receipt.actor_class_name = actor_class.client_name;
    }
    ++c->state.draws_returned;
    if (c->draw_invoke_pc == 329)
        ++c->state.weather_draws_returned;
    else
        ++c->state.entity_draws_returned;
    if (state->living != NULL) {
        const mc_client_living_draw_state *living = state->living;
        receipt.living_source_pc = living->source_pc;
        receipt.primary_event_returned = living->primary_event_returned;
        receipt.primary_model = model_snapshot(&living->primary_render);
        model_receipt(&receipt, &living->primary_render);
        for (size_t i = 0; i < 4; ++i) {
            receipt.pass_attempted[i] = living->pass_attempted[i];
            receipt.pass_result[i] = living->pass_result[i];
            receipt.pass_render_returned[i] = living->pass_render_returned[i];
            if (living->pass_render_returned[i]) {
                if (receipt.secondary_model.identity == 0)
                    receipt.secondary_model = model_snapshot(&living->pass_render[i]);
                model_receipt(&receipt, &living->pass_render[i]);
            }
        }
    }
    if (state->post_draw != NULL) {
        const mc_client_living_draw_state *post = state->post_draw;
        receipt.post_draw_source_pc = post->source_pc;
        receipt.shadow_quads = post->shadow_quads;
        receipt.shadow_vertices = post->shadow_vertices;
        receipt.shadow_block_queries = post->shadow_block_queries;
        receipt.shadow_light_queries = post->shadow_light_queries;
        receipt.shadow_normal_queries = post->shadow_normal_queries;
        receipt.shadow_brightness_queries = post->shadow_brightness_queries;
    }
    c->state.model_parts_compiled += receipt.model_parts_compiled;
    c->state.model_faces += receipt.model_faces;
    c->state.model_vertices += receipt.model_vertices;
    c->state.shadow_quads += receipt.shadow_quads;
    c->state.last_draw = receipt;
    if (c->draw_invoke_pc == 516 && !c->state.first_entity_draw.valid)
        c->state.first_entity_draw = receipt;
}

static client_application *app(mc_client_render_global_entities_call *c, unsigned int pc) {
    client_application *value = c->state.owner->fields.t;
    if (value == NULL)
        fail(c, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE, "actual N App field receiver", pc, false);
    return value;
}

static bool tile_bind(mc_client_render_global_entities_call *c,
                      const mc_client_render_global_entities_services *services) {
    c->state.source_pc = 18;
    if (services == NULL || services->models == NULL || services->world_classes == NULL ||
        services->registry == NULL || services->profile == NULL ||
        services->models->profile != services->profile)
        return fail(c, MC_CLIENT_RENDER_GLOBAL_INVALID_ARGUMENT,
                    "N reached LL native namespace services", 18, true);
    c->services = *services;
    mc_client_tile_renderer_classes *classes = NULL;
    if (mc_client_tile_entity_renderer_classes_existing(services->models, services->registry,
                                                        services->world_classes, &classes,
                                                        &c->tiles) != MC_CLIENT_TILE_RENDERER_OK)
        return tile_failure(c, 18);
    if (classes == NULL && mc_client_tile_renderer_classes_bind(
                               services->models, services->world_classes, services->profile, NULL,
                               &classes, &c->tiles) != MC_CLIENT_TILE_RENDERER_OK)
        return tile_failure(c, 18);
    mc_client_tile_renderer_consumer *consumer = &c->state.owner->tile_consumer;
    if (!consumer->attached) {
        if (mc_client_tile_renderer_consumer_attach(classes, consumer, &c->tiles) !=
            MC_CLIENT_TILE_RENDERER_OK)
            return tile_failure(c, 18);
    } else if (consumer->classes != classes) {
        return fail(c, MC_CLIENT_RENDER_GLOBAL_INVALID_PHASE,
                    "N actual LL consumer association changed", 18, true);
    }
    mc_client_tile_renderer_dispatcher *receiver = NULL;
    if (mc_client_tile_renderer_static_get(classes, &receiver, &c->tiles) !=
        MC_CLIENT_TILE_RENDERER_OK)
        return tile_failure(c, 18);
    if (receiver != NULL && mc_client_tile_renderer_dispatcher_retain(receiver, &c->tiles) !=
                                MC_CLIENT_TILE_RENDERER_OK)
        return tile_failure(c, 18);
    c->state.ll = receiver;
    c->state.source_pc = 22;
    mc_world_runtime *world = c->state.owner->fields.k;
    c->state.source_pc = 26;
    mc_client_render_engine *engine = c->state.owner->fields.l;
    client_application *source_app = app(c, 33);
    if (source_app == NULL)
        return false;
    mc_client_bitmap_font *font = source_app->font_renderer;
    source_app = app(c, 40);
    if (source_app == NULL)
        return false;
    mc_entity *living = source_app->i;
    c->state.source_pc = 44;
    if (mc_client_tile_renderer_bind_frame(receiver, world, engine, font, living, c->state.partial,
                                           &c->tiles) != MC_CLIENT_TILE_RENDERER_OK)
        return tile_failure(c, 44);
    c->state.ll_returned = true;
    return true;
}

static bool manager_bind(mc_client_render_global_entities_call *c) {
    c->state.source_pc = 47;
    mc_client_render_manager_classes *classes = NULL;
    if (mc_client_render_manager_classes_existing(c->services.models, c->services.registry,
                                                  c->services.profile, &classes,
                                                  &c->manager) != MC_CLIENT_MANAGER_OK)
        return manager_failure(c, 47);
    if (classes == NULL || classes != c->services.manager_classes)
        return fail(c, MC_CLIENT_RENDER_GLOBAL_INVALID_PHASE,
                    "N reached actual TH native association", 47, true);
    mc_client_manager_consumer *consumer = &c->state.owner->entity_manager_consumer;
    if (!consumer->attached) {
        if (mc_client_render_manager_consumer_attach(classes, consumer, &c->manager) !=
            MC_CLIENT_MANAGER_OK)
            return manager_failure(c, 47);
    } else if (consumer->classes != classes) {
        return fail(c, MC_CLIENT_RENDER_GLOBAL_INVALID_PHASE,
                    "N actual TH consumer association changed", 47, true);
    }
    mc_client_render_manager *receiver = NULL;
    if (mc_client_render_manager_static_get(classes, &receiver, &c->manager) !=
        MC_CLIENT_MANAGER_OK)
        return manager_failure(c, 47);
    if (receiver != NULL &&
        mc_client_render_manager_retain(receiver, &c->manager) != MC_CLIENT_MANAGER_OK)
        return manager_failure(c, 47);
    c->state.th = receiver;
    c->state.source_pc = 51;
    mc_world_runtime *world = c->state.owner->fields.k;
    c->state.source_pc = 55;
    mc_client_render_engine *engine = c->state.owner->fields.l;
    client_application *source_app = app(c, 62);
    if (source_app == NULL)
        return false;
    mc_client_bitmap_font *font = source_app->font_renderer;
    source_app = app(c, 69);
    if (source_app == NULL)
        return false;
    mc_entity *living = source_app->i;
    source_app = app(c, 76);
    if (source_app == NULL)
        return false;
    mc_game_settings *settings = source_app->settings;
    c->state.source_pc = 80;
    if (mc_client_render_manager_bind_frame(receiver, c->services.math, world, engine, font, living,
                                            settings, c->state.partial,
                                            &c->manager) != MC_CLIENT_MANAGER_OK)
        return manager_failure(c, 80);
    c->state.th_returned = true;
    return true;
}

#include "render_global_entities/body.inc"

static bool body(mc_client_render_global_entities_call *c,
                 const mc_client_render_global_entities_services *services) {
    mc_client_render_global *n = c->state.owner;
    c->state.source_pc = 1;
    c->state.initial_delay = n->fields.I;
    c->state.remaining_delay = c->state.initial_delay;
    if (c->state.initial_delay > 0) {
        c->state.source_pc = 9;
        n->fields.I = mc_java_i32_sub(n->fields.I, 1);
        c->state.last_store_pc = 14;
        c->state.remaining_delay = n->fields.I;
        c->state.source_pc = 17;
        c->state.delayed = c->state.returned = true;
        return true;
    }
    if (!tile_bind(c, services) || !manager_bind(c) || !origins(c) || !entities(c) || !tiles(c))
        return false;
    c->state.source_pc = 569;
    c->state.returned = true;
    return true;
}

mc_client_render_global_status mc_client_render_global_entities_begin(
    mc_client_render_global *n, mc_entity_vector *eye, mc_client_source_frustum *frustum,
    float partial, const mc_client_render_global_entities_services *services,
    mc_client_render_global_entities_call **out, mc_client_render_global_error *error) {
    mc_client_render_global_error local = {0};
    if (error == NULL)
        error = &local;
    *error = (mc_client_render_global_error){0};
    if (n == NULL || out == NULL || *out != NULL)
        return mc_render_global_source_fail(error, MC_CLIENT_RENDER_GLOBAL_INVALID_ARGUMENT,
                                            "N entities native owner/output", NULL, NULL, NULL, 0,
                                            true);
    mc_client_render_global_world_state attachment;
    const bool blocked = n->world_call != NULL &&
                         (!mc_client_render_global_world_inspect(n->world_call, &attachment) ||
                          attachment.active || !attachment.source_returned);
    if (n->active || n->world_retiring || blocked || n->sky_call != NULL ||
        n->clouds_call != NULL || n->cull_call != NULL || n->update_call != NULL ||
        n->submit_call != NULL || n->entities_call != NULL ||
        n->phase != MC_CLIENT_RENDER_GLOBAL_READY)
        return mc_render_global_source_fail(error, MC_CLIENT_RENDER_GLOBAL_BUSY,
                                            "N entities entered/pending/retiring owner", NULL, NULL,
                                            NULL, 0, true);
    mc_client_render_global_entities_call *c = calloc(1, sizeof(*c));
    if (c == NULL)
        return mc_render_global_source_fail(error, MC_CLIENT_RENDER_GLOBAL_ALLOCATION_FAILED,
                                            "N entities native call allocation", NULL, NULL, NULL,
                                            0, true);
    if (mc_client_render_global_retain(n, error) != MC_CLIENT_RENDER_GLOBAL_OK) {
        free(c);
        return error->status;
    }
    c->state = (mc_client_render_global_entities_state){.owner = n,
                                                        .eye = eye,
                                                        .frustum = frustum,
                                                        .partial = partial,
                                                        .failure = &c->failure,
                                                        .cleanup = &c->cleanup};
    n->entities_call = c;
    *out = c;
    c->state.active = n->active = true;
    (void)body(c, services);
    c->state.active = n->active = false;
    *error = c->failure;
    return error->status;
}

bool mc_client_render_global_entities_inspect(const mc_client_render_global_entities_call *c,
                                              mc_client_render_global_entities_state *out) {
    if (c == NULL || out == NULL || c->state.active)
        return false;
    *out = c->state;
    return true;
}

mc_client_render_global_status
mc_client_render_global_entities_retire(mc_client_render_global_entities_call **slot,
                                        mc_client_render_global_error *error) {
    mc_client_render_global_error local = {0};
    if (error == NULL)
        error = &local;
    *error = (mc_client_render_global_error){0};
    if (slot == NULL)
        return mc_render_global_source_fail(error, MC_CLIENT_RENDER_GLOBAL_INVALID_ARGUMENT,
                                            "N entities native retirement slot", NULL, NULL, NULL,
                                            0, true);
    mc_client_render_global_entities_call *c = *slot;
    if (c == NULL)
        return MC_CLIENT_RENDER_GLOBAL_OK;
    if (c->state.active || c->state.owner->active || c->state.owner->entities_call != c)
        return mc_render_global_source_fail(error, MC_CLIENT_RENDER_GLOBAL_BUSY,
                                            "N entities active/changed retirement", NULL, NULL,
                                            NULL, 0, true);
    c->state.retiring = true;
    /* A nested TH owner keeps its producers and pending native rights until
     * this exact child has drained. No parent occurrence is consumed first. */
    if (!retire_draw(c, &c->cleanup)) {
        *error = c->cleanup;
        return error->status;
    }
    release_current(c);
    if (!release_draw_receiver(c, &c->cleanup)) {
        *error = c->cleanup;
        return error->status;
    }
    if (c->draw_ll != NULL && mc_client_tile_renderer_dispatcher_release(
                                  &c->draw_ll, &c->tiles_cleanup) != MC_CLIENT_TILE_RENDERER_OK) {
        c->cleanup.tile_renderer = &c->tiles_cleanup;
        mc_render_global_source_fail(&c->cleanup, MC_CLIENT_RENDER_GLOBAL_CLEANUP_FAILED,
                                     "N captured draw LL receiver retirement", NULL, NULL, NULL, 0,
                                     true);
        *error = c->cleanup;
        return error->status;
    }
    if (c->state.th != NULL && mc_client_render_manager_release(
                                   &c->state.th, &c->manager_cleanup) != MC_CLIENT_MANAGER_OK) {
        c->cleanup.manager = c->manager_cleanup;
        mc_render_global_source_fail(&c->cleanup, MC_CLIENT_RENDER_GLOBAL_CLEANUP_FAILED,
                                     "N captured TH receiver retirement", NULL, NULL, NULL, 0,
                                     true);
        *error = c->cleanup;
        return error->status;
    }
    c->failure.manager.font = NULL;
    c->failure.manager.acquisition = NULL;
    if (c->state.ll != NULL && mc_client_tile_renderer_dispatcher_release(
                                   &c->state.ll, &c->tiles_cleanup) != MC_CLIENT_TILE_RENDERER_OK) {
        c->cleanup.tile_renderer = &c->tiles_cleanup;
        mc_render_global_source_fail(&c->cleanup, MC_CLIENT_RENDER_GLOBAL_CLEANUP_FAILED,
                                     "N captured LL receiver retirement", NULL, NULL, NULL, 0,
                                     true);
        *error = c->cleanup;
        return error->status;
    }
    while (c->world_count != 0) {
        const size_t index = c->world_count - 1;
        if (mc_world_runtime_render_entities_release_lease(c->worlds[index], &c->cleanup.world) !=
            MC_WORLD_STORAGE_OK) {
            mc_render_global_source_fail(&c->cleanup, MC_CLIENT_RENDER_GLOBAL_CLEANUP_FAILED,
                                         "N captured World List lease retirement", NULL, NULL, NULL,
                                         0, true);
            *error = c->cleanup;
            return error->status;
        }
        c->worlds[index] = NULL;
        --c->world_count;
    }
    c->state.entities_list = NULL;
    c->list_world = NULL;
    mc_client_render_global *owner = c->state.owner;
    owner->entities_call = NULL;
    if (mc_client_render_global_release(&c->state.owner, &c->cleanup) !=
        MC_CLIENT_RENDER_GLOBAL_OK) {
        owner->entities_call = c;
        *error = c->cleanup;
        return error->status;
    }
    free(c);
    *slot = NULL;
    return MC_CLIENT_RENDER_GLOBAL_OK;
}

mc_client_render_global_status
mc_render_global_entities_end_consumers(mc_client_render_global *n,
                                        mc_client_render_global_error *error) {
    if (mc_client_tile_renderer_consumer_detach(&n->tile_consumer, &n->tile_consumer_cleanup) !=
        MC_CLIENT_TILE_RENDERER_OK) {
        error->tile_renderer = &n->tile_consumer_cleanup;
        return mc_render_global_source_fail(error, MC_CLIENT_RENDER_GLOBAL_BUSY,
                                            "actual N LL consumer ending", NULL, NULL, NULL, 0,
                                            true);
    }
    if (mc_client_render_manager_consumer_detach(
            &n->entity_manager_consumer, &n->entity_manager_cleanup) != MC_CLIENT_MANAGER_OK) {
        error->manager = n->entity_manager_cleanup;
        return mc_render_global_source_fail(error, MC_CLIENT_RENDER_GLOBAL_BUSY,
                                            "actual N TH consumer ending", NULL, NULL, NULL, 0,
                                            true);
    }
    return MC_CLIENT_RENDER_GLOBAL_OK;
}
