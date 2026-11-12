/* Genuine App/PX startup and GAME World/Player construction. Prepared source
 * fields bound the finite outer-method comparisons; no whole frame is claimed. */
int mc_hand_startup_fixture_main(int argc, char **argv);
#define main mc_hand_startup_fixture_main
#include "../startup/test_application_startup.c"
#undef main

#include "client/display/camera_surface.h"
#include "client/display/fog_surface.h"
#include "client/display/world_frame_surface.h"
#include "client/render/manager/internal.h"
#include "client/render_engine/internal.h"
#include "client/render_engine/texture_lookup.h"
#include "client/world/entity_renderer/first_person_hand_internal.h"
#include "client/world/entity_renderer/held_item_draw_internal.h"
#include "client/world/entity_renderer/internal.h"
#include "entity/player/player_internal.h"
#include "mc/client/render/model_draw.h"
#include "mc/client/world/held_item_draw.h"
#include "mc/client/world/held_item_tick.h"
#include "world/runtime/chunk_provider_internal.h"
#include "world/runtime/runtime_internal.h"

#include <SDL3/SDL.h>
#include <inttypes.h>

typedef struct hand_world {
    mc_world_runtime_classes *classes;
    mc_world_regions *regions;
    mc_world_store *store;
    mc_world_info_name *name;
    mc_world_provider *dimension;
    mc_world_runtime *world;
    mc_chunk_state *chunks[4];
    mc_chunk_provider_fields provider;
    mc_entity *player;
} hand_world;

static int hand_world_open(hand_world *w, fixture *runtime, client_application *app,
                           mc_math_classes *math, const char *directory) {
    CHECK(mc_world_runtime_classes_create(MC_WORLD_INFO_CLIENT, runtime->services.profile,
                                          mc_math_classes_table(math), &w->classes,
                                          NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_classes_bind_registry(w->classes, runtime->services.registry, NULL) ==
          MC_REGISTRY_OK);
    CHECK(mc_world_runtime_classes_bind_entities(w->classes, runtime->services.entities) ==
          MC_ENTITY_OK);
    CHECK(mc_world_regions_create(NULL, &w->regions, NULL) == MC_WORLD_STORAGE_OK);
    uint8_t ids[256];
    for (size_t i = 0; i < sizeof(ids); ++i)
        ids[i] = (uint8_t)i;
    const mc_world_store_options options = {.session_token = 6381,
                                            .block_map = ids,
                                            .block_map_length = sizeof(ids),
                                            .limits = mc_world_store_default_limits()};
    CHECK(mc_world_store_open(directory, w->regions, &options, &w->store, NULL) ==
          MC_WORLD_STORAGE_OK);
    static const uint16_t title[] = {'H', 'a', 'n', 'd'};
    CHECK(mc_world_info_name_copy((mc_nbt_string){title, 4}, SIZE_MAX, &w->name) ==
          MC_WORLD_INFO_OK);
    CHECK(mc_world_provider_create(0, NULL, &w->dimension, NULL) == MC_WORLD_PROVIDER_OK);
    mc_world_runtime_construction_limits limits = mc_world_runtime_default_construction_limits();
    limits.max_provider_chunks = 4;
    CHECK(mc_world_runtime_allocate_world(MC_WORLD_RUNTIME_CLIENT_DIRECT_METADATA, w->classes,
                                          &limits, &w->world, NULL) == MC_WORLD_STORAGE_OK);
    const mc_world_runtime_world_arguments args = {
        .store = w->store, .name = w->name, .seed = 23, .dimension = w->dimension};
    CHECK(mc_world_runtime_construct_world(w->world, &args, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_chunk_provider_bind_fields(w->world->fields.v, &w->provider, NULL) ==
          MC_WORLD_STORAGE_OK);
    for (int32_t x = 0; x < 2; ++x)
        for (int32_t z = 0; z < 2; ++z) {
            const size_t index = (size_t)(x * 2 + z);
            CHECK(mc_world_runtime_get(w->world, x, z, &w->chunks[index]) == MC_CHUNK_ACQUIRE_OK &&
                  w->chunks[index] != NULL && w->chunks[index]->terrain_populated);
        }
    CHECK(mc_local_player_allocate(runtime->services.entities, &w->player, NULL) == MC_ENTITY_OK);
    CHECK(mc_local_player_construct(w->player, w->world, app, app->session, 0, NULL, NULL) ==
          MC_PLAYER_OK);
    CHECK(mc_entity_set_position(w->player, 8.5, 64.5, 8.5) == MC_ENTITY_OK);
    app->i = app->h = w->player;
    app->active_world = w->world;
    return 0;
}

static int hand_world_close(hand_world *w, client_application *app) {
    app->i = app->h = NULL;
    app->active_world = NULL;
    mc_entity_release(w->player);
    CHECK(mc_world_runtime_retire_entity_containers(w->world, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_chunk_pending_clear(w->provider.pending) == MC_CHUNK_ACQUIRE_OK);
    CHECK(mc_chunk_cache_init(w->provider.cache, w->provider.cache->entries,
                              w->provider.cache->capacity, w->provider.cache->insertions,
                              w->provider.cache->insertion_capacity));
    mc_world_provider_unbind(w->dimension);
    CHECK(mc_world_runtime_retire_world(w->world, NULL) == MC_WORLD_STORAGE_OK);
    for (size_t i = 0; i < 4; ++i)
        CHECK(mc_world_store_retire_chunk(w->store, w->chunks[i], NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_free(w->world, NULL) == MC_WORLD_STORAGE_OK);
    mc_world_provider_release(w->dimension);
    CHECK(mc_world_store_free_checked(&w->store, NULL) == MC_WORLD_STORAGE_OK);
    mc_world_regions_free(w->regions);
    mc_world_info_name_release(w->name);
    CHECK(mc_world_runtime_classes_free(w->classes, NULL) == MC_WORLD_STORAGE_OK);
    return 0;
}

static uint32_t hand_bits(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static int hand_prepare(client_application *app, mc_entity *player, unsigned int mode,
                        unsigned int bob, unsigned int anaglyph) {
    mc_game_settings_state options;
    CHECK(mc_game_settings_read_state(app->settings, &options));
    options.values.third_person_view = mode == 0; /* Actual kv.A. */
    options.values.hide_gui = mode == 2;          /* Actual kv.z. */
    options.values.view_bobbing = bob != 0;
    options.values.anaglyph = anaglyph != 0;
    CHECK(mc_game_settings_set_values(app->settings, &options.values, NULL) == MC_GAME_SETTINGS_OK);
    mc_living_entity_fields *living = &player->local_player->fields.living;
    living->Y = 20;
    living->aa = 7;
    living->ab = 10;
    living->ac = 60;
    living->ad = 0;
    living->af = 2;
    living->ag = -1;
    player->base.bi = 1.125f;
    player->base.bj = 1.5f;
    mc_player_fields *fields = &player->local_player->fields.player;
    fields->h = .1f;
    fields->i = .175f;
    fields->u = mode == 1;
    CHECK(mc_client_display_source_matrix_mode(app->display, GL_PROJECTION, NULL));
    CHECK(mc_client_display_source_load_identity(app->display, NULL));
    CHECK(mc_client_display_source_scale(app->display, 1.25, .75, 1.5, NULL));
    CHECK(mc_client_display_source_matrix_mode(app->display, GL_MODELVIEW, NULL));
    CHECK(mc_client_display_source_load_identity(app->display, NULL));
    CHECK(mc_client_display_source_translate(app->display, 5, 6, 7, NULL));
    CHECK(mc_client_display_source_scale(app->display, 2, 3, 4, NULL));
    return 0;
}

static int hand_outer_profiles(client_application *app, mc_entity *player, mc_math_classes *math) {
    mc_client_source_px *px = app->t;
    CHECK(px != NULL && px->phase == MC_CLIENT_ENTITY_RENDERER_READY && px->fields.c != NULL);
    mc_client_source_ra *actual_ra = px->fields.c;
    px->fields.c = NULL; /* Same explicit source null operand as the original fixture. */
    mc_client_first_person_hand_call *call = NULL;
    mc_client_first_person_hand_state state;
    mc_client_first_person_hand_error error;
    const float partials[] = {.375f, 1.0f};
    unsigned int index = 0;
    for (unsigned int mode = 0; mode < 4; ++mode)
        for (unsigned int p = 0; p < 2; ++p)
            for (unsigned int bob = 0; bob < 2; ++bob)
                for (unsigned int anaglyph = 0; anaglyph < 2; ++anaglyph)
                    for (unsigned int eye = 0; eye < 2; ++eye) {
                        CHECK(hand_prepare(app, player, mode, bob, anaglyph) == 0);
                        px->active = true;
                        const mc_client_first_person_hand_status status =
                            entity_renderer_first_person_hand_entered(px, math, partials[p],
                                                                      (int32_t)eye, &call, &error);
                        px->active = false;
                        CHECK(mc_client_first_person_hand_inspect(call, &state));
                        CHECK((mode < 2 && status == MC_CLIENT_FIRST_PERSON_HAND_OK &&
                               state.returned) ||
                              (mode >= 2 && status == MC_CLIENT_FIRST_PERSON_HAND_SOURCE_NULL &&
                               !state.returned && error.site.pc == (mode == 2 ? 137 : 100)));
                        CHECK(state.modelview_reset && state.matrix_pushed &&
                              state.matrix_popped == (mode != 3) && state.hurt_calls == 1 &&
                              state.bob_calls == (bob ? (mode < 2 ? 2u : 1u) : 0u));
                        int32_t matrix_mode;
                        size_t model_depth, projection_depth;
                        CHECK(mc_client_display_camera_stack_inspect(
                            app->display, &matrix_mode, &model_depth, &projection_depth));
                        CHECK(model_depth == (mode == 3 ? 2u : 1u) && projection_depth == 1 &&
                              matrix_mode == GL_MODELVIEW);
                        float projection[16], modelview[16];
                        CHECK(mc_client_display_camera_matrices_inspect(app->display, projection,
                                                                        modelview));
                        printf("HAND_OUTER %u %u %u %u %u %u %s %zu %d", index++, mode, p, bob,
                               anaglyph, eye, state.returned ? "OK" : "NULL", model_depth,
                               matrix_mode);
                        for (size_t i = 0; i < 16; ++i)
                            printf(" %08" PRIx32, hand_bits(modelview[i]));
                        for (size_t i = 0; i < 16; ++i)
                            printf(" %08" PRIx32, hand_bits(projection[i]));
                        putchar('\n');
                        CHECK(mc_client_first_person_hand_retire(&call, &error) ==
                                  MC_CLIENT_FIRST_PERSON_HAND_OK &&
                              call == NULL);
                        size_t after_model_depth, after_projection_depth;
                        CHECK(mc_client_display_camera_stack_inspect(app->display, &matrix_mode,
                                                                     &after_model_depth,
                                                                     &after_projection_depth));
                        CHECK(after_model_depth == model_depth &&
                              after_projection_depth == projection_depth);
                        if (model_depth == 2)
                            CHECK(mc_client_display_source_pop(
                                app->display, NULL)); /* Fixture preparation only. */
                    }
    px->fields.c = actual_ra;
    CHECK(index == 64);
    return 0;
}

static int hand_arm_profiles(fixture *runtime, client_application *app, mc_math_classes *math) {
    mc_client_render_manager *manager = app->t->services.models->th->a;
    CHECK(manager != NULL);
    mc_client_managed_renderer *renderer = NULL;
    for (size_t i = 0; i < manager->allocated_renderers; ++i)
        if (strcmp(manager->renderers[i]->source_class, "ds") == 0) {
            CHECK(renderer == NULL);
            renderer = manager->renderers[i];
        }
    CHECK(renderer != NULL && renderer->phase == MC_CLIENT_MODEL_READY &&
          renderer->own_model != NULL && renderer->own_model->fields.d != NULL);
    mc_client_biped_model *model = &renderer->own_model->fields;
    mc_client_model_part *part = model->d;
    CHECK(!part->n);
    int32_t first_list = 0;
    for (unsigned int repeat = 0; repeat < 2; ++repeat) {
        CHECK(mc_client_display_source_matrix_mode(app->display, GL_MODELVIEW, NULL));
        CHECK(mc_client_display_source_load_identity(app->display, NULL));
        CHECK(mc_client_display_source_color(app->display, .125f, .25f, .5f, .75f, NULL));
        CHECK(mc_client_display_source_cull_test(app->display, false, NULL));
        CHECK(mc_client_display_source_blend_enable(app->display, false, NULL));
        CHECK(mc_client_display_source_fog_enable(app->display, false, NULL));
        CHECK(mc_client_display_source_lighting_enable(app->display, false, NULL));
        mc_client_nw_state nw;
        CHECK(mc_client_nw_inspect(runtime->graphics, &nw) && nw.fields.g != NULL &&
              nw.fields.B > 0 && nw.fields.e != NULL && !nw.fields.w);
        /* Same explicit input preparation as the original, once before the
         * complete six-face compilation. Duplicate normal words carry between faces. */
        memset((void *)nw.fields.g, 0, (size_t)nw.fields.B * sizeof(*nw.fields.g));
        ((mc_client_direct_int_buffer *)(void *)nw.fields.e)->position = 0;
        CHECK(mc_entity_vector_pool_reset(runtime->services.entities, NULL) == MC_ENTITY_OK);
        mc_client_model_draw_error error;
        mc_math_class_error math_error;
        CHECK(entity_renderer_ds_arm_draw(renderer, math, runtime->graphics,
                                          runtime->services.entities, app->display, &error,
                                          &math_error));
        CHECK(error.returned && error.faces == (repeat == 0 ? 6u : 0u) &&
              error.vertices == (repeat == 0 ? 24u : 0u));
        mc_client_source_list_state list;
        CHECK(mc_client_display_list_inspect(app->display, part->o, &list) && list.defined &&
              !list.building && list.draws == 6 && list.vertices == 36);
        int32_t mode;
        size_t depth, projection_depth;
        CHECK(
            mc_client_display_camera_stack_inspect(app->display, &mode, &depth, &projection_depth));
        float projection[16], modelview[16];
        CHECK(mc_client_display_camera_matrices_inspect(app->display, projection, modelview));
        mc_entity_vector_snapshot vectors;
        CHECK(mc_entity_vector_get_snapshot(runtime->services.entities, &vectors) == MC_ENTITY_OK);
        printf("ARM_PART %u %u %u %u %zu %d", repeat, part->n ? 1u : 0u, list.defined ? 1u : 0u,
               repeat && first_list == part->o ? 1u : 0u, depth, vectors.cursor);
        for (size_t i = 0; i < 16; ++i)
            printf(" %08" PRIx32, hand_bits(modelview[i]));
        const float pose[] = {model->m, part->a, part->b, part->c, part->d, part->e, part->f};
        for (size_t i = 0; i < 7; ++i)
            printf(" %08" PRIx32, hand_bits(pose[i]));
        putchar('\n');
        if (repeat == 0) {
            first_list = part->o;
            for (size_t face = 0; face < 6; ++face) {
                mc_client_source_batch_view batch;
                CHECK(mc_client_display_list_batch(app->display, part->o, face, &batch) &&
                      batch.vertices == 6 && batch.byte_count == 6u * 8u * sizeof(uint32_t));
                printf("ARM_WORDS %zu %d ", face, batch.vertices);
                for (size_t vertex = 0; vertex < 6; ++vertex)
                    for (size_t component = 0; component < 7; ++component) {
                        uint32_t word;
                        memcpy(&word, batch.bytes + (vertex * 8u + component) * sizeof(word),
                               sizeof(word));
                        printf("%08" PRIx32, word);
                    }
                putchar('\n');
            }
        }
    }
    for (unsigned int mask = 0; mask < 16; ++mask) {
        model->j = (mask & 1u) != 0;
        model->k = (mask & 2u) != 0;
        model->l = (mask & 4u) != 0;
        model->n = (mask & 8u) != 0;
        CHECK(mc_entity_vector_pool_reset(runtime->services.entities, NULL) == MC_ENTITY_OK);
        mc_client_model_draw_error error;
        mc_math_class_error math_error;
        CHECK(entity_renderer_ds_arm_draw(renderer, math, runtime->graphics,
                                          runtime->services.entities, app->display, &error,
                                          &math_error));
        mc_entity_vector_snapshot vectors;
        CHECK(mc_entity_vector_get_snapshot(runtime->services.entities, &vectors) == MC_ENTITY_OK);
        printf("ARM_POSE %u %d", mask, vectors.cursor);
        const mc_client_model_part *parts[] = {model->a, model->b, model->c, model->d, model->e,
                                               model->f, model->g, model->h, model->i};
        for (size_t i = 0; i < 9; ++i) {
            const mc_client_model_part *p = parts[i];
            const float values[] = {p->a, p->b, p->c, p->d, p->e, p->f};
            for (size_t j = 0; j < 6; ++j)
                printf(" %08" PRIx32, hand_bits(values[j]));
        }
        putchar('\n');
    }
    return 0;
}

static int hand_pending_guards(mc_client_held_item *owner, mc_math_classes *math) {
    mc_client_entity_renderer_error owner_error;
    const size_t holds = owner->holds;
    CHECK(mc_client_held_item_retain(owner, &owner_error) == MC_CLIENT_ENTITY_RENDERER_BUSY &&
          owner->holds == holds);
    mc_client_held_item *occurrence = owner;
    CHECK(mc_client_held_item_release(&occurrence, &owner_error) ==
              MC_CLIENT_ENTITY_RENDERER_BUSY &&
          occurrence == owner && owner->holds == holds);
    CHECK(mc_client_held_item_retire(&occurrence, &owner_error) == MC_CLIENT_ENTITY_RENDERER_BUSY &&
          occurrence == owner && owner->holds == holds);
    mc_client_held_item_tick_call *tick = NULL;
    mc_client_held_item_tick_error tick_error;
    CHECK(mc_client_held_item_tick_begin(owner, &tick, &tick_error) ==
              MC_CLIENT_HELD_ITEM_TICK_BUSY &&
          tick == NULL);
    mc_client_held_item_draw_call *draw = NULL;
    mc_client_held_item_draw_error draw_error;
    CHECK(mc_client_held_item_draw_begin(owner, math, .375f, false, &draw, &draw_error) ==
              MC_CLIENT_HELD_ITEM_DRAW_BUSY &&
          draw == NULL);
    return 0;
}

static int hand_full_profiles(fixture *runtime, client_application *app, hand_world *world,
                              mc_math_classes *math) {
    mc_client_source_px *px = app->t;
    CHECK(mc_entity_set_position(world->player, 8.5, 200.5, 8.5) == MC_ENTITY_OK);
    world->player->base.yaw = 33;
    world->player->base.aU = 12;
    world->player->base.aT = -17;
    world->player->base.aV = 3;
    world->player->local_player->fields.living.W = .1f;
    world->player->local_player->fields.living.X = .5f;
    px->fields.c->c = .75f;
    px->fields.c->d = .25f;
    CHECK(world->world->live.time != NULL && world->chunks[0]->data != NULL);
    *world->world->live.time = 6000;
    world->world->fields.f = 0;
    /* The same explicit air/Sky15/Block0 source operands as the original. */
    CHECK(mc_chunk_nibbles_set(world->chunks[0]->data->skylight, MC_CHUNK_NIBBLE_BYTES, 8, 127, 8,
                               15));
    const float partials[] = {.375f, 1.0f};
    for (unsigned int overlays = 0; overlays < 2; ++overlays)
        for (unsigned int p = 0; p < 2; ++p) {
            CHECK(mc_client_display_source_matrix_mode(app->display, GL_MODELVIEW, NULL));
            CHECK(mc_client_display_source_load_identity(app->display, NULL));
            CHECK(mc_client_display_source_color(app->display, .125f, .25f, .5f, .75f, NULL));
            CHECK(mc_client_display_source_alpha_test(app->display, true, NULL));
            CHECK(mc_client_display_source_rescale_normal(app->display, false, NULL));
            CHECK(mc_client_display_source_lighting_enable(app->display, false, NULL));
            CHECK(mc_client_display_source_light_enable(app->display, 0, false, NULL));
            CHECK(mc_client_display_source_light_enable(app->display, 1, false, NULL));
            CHECK(mc_client_display_source_color_material_enable(app->display, false, NULL));
            CHECK(mc_client_display_source_fog_enable(app->display, false, NULL));
            mc_client_nw_state nw;
            CHECK(mc_client_nw_inspect(runtime->graphics, &nw) && nw.fields.g != NULL &&
                  nw.fields.e != NULL && !nw.fields.w);
            memset((void *)nw.fields.g, 0, (size_t)nw.fields.B * sizeof(*nw.fields.g));
            ((mc_client_direct_int_buffer *)(void *)nw.fields.e)->position = 0;
            CHECK(mc_entity_vector_pool_reset(runtime->services.entities, NULL) == MC_ENTITY_OK);
            mc_client_held_item_draw_call *call = NULL;
            mc_client_held_item_draw_error error;
            const mc_client_held_item_draw_status status = mc_client_held_item_draw_begin(
                px->pending_ra, math, partials[p], overlays != 0, &call, &error);
            if (status != MC_CLIENT_HELD_ITEM_DRAW_OK)
                fprintf(stderr, "Actual RA %s.%s:%d %s\n", error.site.class_name, error.site.method,
                        error.site.pc, error.operation);
            mc_client_held_item_draw_state state;
            CHECK(status == MC_CLIENT_HELD_ITEM_DRAW_OK &&
                  mc_client_held_item_draw_inspect(call, &state) && state.returned);
            CHECK(hand_pending_guards(px->pending_ra, math) == 0);
            CHECK(overlays
                      ? state.alpha_restored && !state.burning && !state.suffocating && !state.water
                      : state.skin_returned && state.renderer_returned && state.pose_returned &&
                            state.arm_returned && state.brightness == 1.0f);
            float current_color[4], projection[16], modelview[16];
            /* Same original query order, including Mesa's attribute commit. */
            CHECK(mc_client_display_source_current_attribute(app->display, GL_CURRENT_COLOR,
                                                             current_color, 4, NULL));
            CHECK(mc_client_display_camera_matrices_inspect(app->display, projection, modelview));
            int32_t mode;
            size_t depth, projection_depth;
            CHECK(mc_client_display_camera_stack_inspect(app->display, &mode, &depth,
                                                         &projection_depth));
            mc_entity_vector_snapshot vectors;
            CHECK(mc_entity_vector_get_snapshot(runtime->services.entities, &vectors) ==
                  MC_ENTITY_OK);
            mc_client_source_draw_state draw;
            mc_client_source_fog_state fog;
            CHECK(mc_client_display_source_draw_inspect(app->display, &draw));
            CHECK(mc_client_display_source_fog_inspect(app->display, &fog));
            CHECK(mc_client_nw_inspect(runtime->graphics, &nw));
            printf("HAND_DRAW %u %u %zu %d", overlays, p, depth, vectors.cursor);
            for (size_t i = 0; i < 16; ++i)
                printf(" %08" PRIx32, hand_bits(modelview[i]));
            for (size_t i = 0; i < 4; ++i)
                printf(" %08" PRIx32, hand_bits(current_color[i]));
            printf(" %u %u %u %u %u %u %zu\n", draw.alpha_test ? 1u : 0u,
                   fog.rescale_normal_enabled ? 1u : 0u, fog.lighting_enabled ? 1u : 0u,
                   fog.lights[0] ? 1u : 0u, fog.lights[1] ? 1u : 0u, fog.color_material ? 1u : 0u,
                   nw.fields.e->position);
            CHECK(mc_client_held_item_draw_retire(&call, &error) == MC_CLIENT_HELD_ITEM_DRAW_OK);
        }
    return 0;
}

static int hand_real_children(client_application *app, mc_entity *player, mc_math_classes *math,
                              const char *screenshot) {
    mc_client_source_px *px = app->t;
    mc_client_first_person_hand_call *call = NULL;
    mc_client_first_person_hand_state state;
    mc_client_first_person_hand_error error;
    /* Actual z/hide-GUI skips the held arm, but the dry overlay body still runs. */
    CHECK(hand_prepare(app, player, 2, 1, 0) == 0);
    px->active = true;
    mc_client_first_person_hand_status status =
        entity_renderer_first_person_hand_entered(px, math, .375f, 0, &call, &error);
    px->active = false;
    if (status != MC_CLIENT_FIRST_PERSON_HAND_OK)
        fprintf(stderr, "Actual hand overlays %s.%s:%d %s\n", error.site.class_name,
                error.site.method, error.site.pc, error.operation);
    CHECK(status == MC_CLIENT_FIRST_PERSON_HAND_OK &&
          mc_client_first_person_hand_inspect(call, &state) && state.returned &&
          state.overlays_returned && !state.held_returned && state.hurt_calls == 2 &&
          state.bob_calls == 2 && state.overlays != NULL && state.overlays->returned &&
          state.overlays->alpha_restored && !state.overlays->burning &&
          !state.overlays->suffocating && !state.overlays->water);
    CHECK(mc_client_first_person_hand_retire(&call, &error) == MC_CLIENT_FIRST_PERSON_HAND_OK);
    /* Default view must enter the real arm/item renderer rather than skip it. */
    CHECK(hand_prepare(app, player, 3, 0, 0) == 0);
    CHECK(mc_entity_set_position(player, 8.5, 200.5, 8.5) == MC_ENTITY_OK);
    player->local_player->fields.living.aa = 0;
    player->local_player->fields.living.W = 0;
    player->local_player->fields.living.X = 0;
    player->base.yaw = player->base.aT = player->base.aU = player->base.aV = 0;
    px->fields.c->c = 1.0f;
    px->fields.c->d = 1.0f;
    CHECK(mc_client_display_source_viewport(app->display, app->d, app->e, NULL));
    CHECK(mc_client_display_source_matrix_mode(app->display, GL_PROJECTION, NULL));
    CHECK(mc_client_display_source_load_identity(app->display, NULL));
    CHECK(mc_client_display_source_perspective(app->display, 70.0f, (float)app->d / (float)app->e,
                                               .05f, 128, NULL));
    CHECK(mc_client_display_source_matrix_mode(app->display, GL_MODELVIEW, NULL));
    CHECK(mc_client_display_source_color_mask(app->display, true, true, true, true, NULL));
    CHECK(mc_client_display_source_cull_test(app->display, false, NULL));
    CHECK(mc_client_display_source_texture_enable(app->display, true, NULL));
    CHECK(mc_client_display_source_alpha_test(app->display, true, NULL));
    CHECK(mc_client_display_source_alpha_function(app->display, GL_GREATER, .1f, NULL));
    CHECK(mc_client_display_source_blend_enable(app->display, false, NULL));
    CHECK(mc_client_display_source_fog_enable(app->display, false, NULL));
    CHECK(mc_client_display_source_depth_mask(app->display, true, NULL));
    CHECK(mc_client_display_source_depth_test(app->display, true, NULL));
    CHECK(mc_client_display_source_clear_depth(app->display, 1, NULL));
    CHECK(mc_client_display_source_clear_color(app->display, 0, 0, 0, 1, NULL));
    CHECK(mc_client_display_source_clear(app->display, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT,
                                         NULL));
    mc_client_capture before = {0}, after = {0};
    CHECK(mc_client_display_capture_rgba(app->display, &before, NULL));
    px->active = true;
    status = entity_renderer_first_person_hand_entered(px, math, .375f, 0, &call, &error);
    px->active = false;
    CHECK(mc_client_first_person_hand_inspect(call, &state) && state.held_item != NULL &&
          state.held_item->player_captured && state.held_item->brightness_returned &&
          state.held_item->empty_hand && state.held_item->swing_returned);
    printf("HAND_DEFAULT %d %u %u %u\n", (int)status, state.source_pc, state.held_item->source_pc,
           state.returned ? 1u : 0u);
    if (status != MC_CLIENT_FIRST_PERSON_HAND_OK)
        fprintf(stderr, "Actual hand default %s.%s:%d %s\n", error.site.class_name,
                error.site.method, error.site.pc, error.operation);
    CHECK(status == MC_CLIENT_FIRST_PERSON_HAND_OK && state.returned && state.held_returned &&
          state.overlays_returned && state.held_item->returned && state.held_item->skin_returned &&
          state.held_item->renderer_returned && state.held_item->pose_returned &&
          state.held_item->arm_returned);
    CHECK(mc_client_display_capture_rgba(app->display, &after, NULL));
    CHECK(before.width == after.width && before.height == after.height &&
          before.stride == after.stride);
    size_t changed = 0, purple = 0;
    bool background_black = true;
    for (int32_t y = 0; y < after.height; ++y)
        for (int32_t x = 0; x < after.width; ++x) {
            const size_t offset = (size_t)y * after.stride + (size_t)x * 4;
            const uint8_t *pixel = after.pixels + offset;
            background_black = background_black && before.pixels[offset] == 0 &&
                               before.pixels[offset + 1] == 0 && before.pixels[offset + 2] == 0;
            if (pixel[0] || pixel[1] || pixel[2])
                ++changed;
            if (pixel[0] > pixel[1] && pixel[2] > pixel[1])
                ++purple;
        }
    printf("HAND_PIXELS %zu %zu %d %d\n", changed, purple, after.width, after.height);
    CHECK(background_black && changed >= 64 && purple >= 64);
    SDL_Surface *surface = SDL_CreateSurfaceFrom(after.width, after.height, SDL_PIXELFORMAT_RGBA32,
                                                 after.pixels, (int)after.stride);
    CHECK(surface != NULL && SDL_SavePNG(surface, screenshot));
    SDL_DestroySurface(surface);
    mc_client_capture_free(&before);
    mc_client_capture_free(&after);
    CHECK(mc_client_first_person_hand_retire(&call, &error) == MC_CLIENT_FIRST_PERSON_HAND_OK);
    int32_t mode;
    size_t depth, projection_depth;
    CHECK(mc_client_display_camera_stack_inspect(app->display, &mode, &depth, &projection_depth));
    CHECK(depth == 1 && projection_depth == 1 && mode == GL_MODELVIEW);
    return 0;
}

static int hand_failure_controls(client_application *app, hand_world *world,
                                 mc_math_classes *math) {
    mc_client_held_item *owner = app->t->pending_ra;
    mc_client_texture_lookup_error *lookup = calloc(1, sizeof(*lookup));
    CHECK(lookup != NULL);
    mc_client_texture texture;
    CHECK(mc_client_render_engine_texture(app->render_engine, app->display, NULL,
                                          (mc_texture_pack_text){NULL, 0, true}, &texture,
                                          lookup) == MC_CLIENT_LOOKUP_NULL_REFERENCE);
    CHECK(mc_client_texture_lookup_pending(app->render_engine));
    mc_client_held_item_draw_call *call = NULL;
    mc_client_held_item_draw_error error;
    CHECK(mc_client_held_item_draw_begin(owner, math, .375f, false, &call, &error) !=
          MC_CLIENT_HELD_ITEM_DRAW_OK);
    mc_client_held_item_draw_state state;
    CHECK(mc_client_held_item_draw_inspect(call, &state) && !state.returned &&
          state.source_pc == 1275 && state.failure->texture != NULL &&
          state.failure->texture->status == MC_CLIENT_LOOKUP_BUSY);
    CHECK(hand_pending_guards(owner, math) == 0);
    int32_t mode;
    size_t depth, projection_depth;
    CHECK(mc_client_display_camera_stack_inspect(app->display, &mode, &depth, &projection_depth) &&
          depth == 2 && projection_depth == 1);
    float projection[16], reached[16], retired[16];
    CHECK(mc_client_display_camera_matrices_inspect(app->display, projection, reached));
    CHECK(mc_client_held_item_draw_retire(&call, &error) == MC_CLIENT_HELD_ITEM_DRAW_OK &&
          call == NULL && mc_client_texture_lookup_pending(app->render_engine));
    CHECK(mc_client_display_camera_matrices_inspect(app->display, projection, retired) &&
          memcmp(reached, retired, sizeof(reached)) == 0);
    CHECK(mc_client_display_camera_stack_inspect(app->display, &mode, &depth, &projection_depth) &&
          depth == 2);
    CHECK(mc_client_render_engine_retire_lookup(app->render_engine, lookup) ==
              MC_CLIENT_LOOKUP_OK &&
          !mc_client_texture_lookup_pending(app->render_engine));
    free(lookup);
    CHECK(mc_client_display_source_pop(app->display, NULL)); /* Next fixture's preparation. */
    puts("HAND_CONTROL foreign_lookup_preserved");

    CHECK(mc_entity_set_position(world->player, 8.5, 64.5, 8.5) == MC_ENTITY_OK);
    CHECK(world->chunks[0]->data != NULL && world->chunks[0]->data->blocks != NULL);
    uint8_t *block = &world->chunks[0]->data->blocks[8u * 2048u + 8u * 128u + 64u];
    CHECK(*block == 0);
    for (unsigned int failure = 0; failure < 3; ++failure) {
        /* Explicit source inputs: actual fire counter, solid stone, still water. */
        world->player->base.bv = failure == 0 ? 1 : 0;
        *block = failure == 1 ? 1u : failure == 2 ? 9u : 0u;
        CHECK(mc_client_display_source_alpha_test(app->display, true, NULL));
        CHECK(mc_client_held_item_draw_begin(owner, math, .375f, true, &call, &error) ==
              MC_CLIENT_HELD_ITEM_DRAW_DEPENDENCY);
        CHECK(mc_client_held_item_draw_inspect(call, &state) && !state.returned &&
              state.alpha_disabled && !state.alpha_restored &&
              state.source_pc == (failure == 0   ? 28u
                                  : failure == 1 ? 67u
                                                 : 384u) &&
              (failure == 0   ? state.burning
               : failure == 1 ? state.suffocating
                              : state.water));
        CHECK(mc_client_held_item_draw_retire(&call, &error) == MC_CLIENT_HELD_ITEM_DRAW_OK);
        mc_client_source_draw_state draw;
        CHECK(mc_client_display_source_draw_inspect(app->display, &draw) && !draw.alpha_test);
        CHECK(mc_client_display_camera_stack_inspect(app->display, &mode, &depth,
                                                     &projection_depth) &&
              depth == 1 && projection_depth == 1);
    }
    world->player->base.bv = 0;
    *block = 0;
    puts("HAND_CONTROL overlay_dependencies_preserve_alpha");
    return 0;
}

static int hand_device(const char *resources, const char *assets, const char *directory,
                       const char *screenshot, bool arm_profile) {
    fixture runtime;
    startup_fixture startup;
    CHECK(fixture_open(&runtime, resources) == 0 && startup_services(&startup) == 0 &&
          startup_icons(&startup, assets) == 0);
    client_application *app = NULL;
    CHECK(actual_app(&runtime, false, &app) == 0 && statistics_session(app) == 0);
    mc_client_application_startup *attempt = NULL;
    mc_client_startup_options options = mc_client_application_startup_default_options();
    options.hidden = true;
    CHECK(mc_client_application_startup_allocate(app, &startup.services, &options, &attempt,
                                                 startup.error) == MC_CLIENT_STARTUP_OK);
    CHECK(mc_client_application_run_prefix(attempt, startup.error) ==
              MC_CLIENT_STARTUP_UNIMPLEMENTED &&
          startup.error->caller.pc == 32);
    CHECK(statistics_retire_resources(attempt) == 0);
    mc_math_classes *math = NULL;
    CHECK(mc_math_classes_create(SIZE_MAX, &math, NULL) == MC_MATH_CLASS_OK &&
          mc_math_classes_ensure(math, NULL) == MC_MATH_CLASS_OK);
    hand_world world = {0};
    CHECK(hand_world_open(&world, &runtime, app, math, directory) == 0);
    if (arm_profile)
        CHECK(hand_arm_profiles(&runtime, app, math) == 0);
    else {
        CHECK(hand_outer_profiles(app, world.player, math) == 0);
        CHECK(hand_full_profiles(&runtime, app, &world, math) == 0);
        CHECK(hand_real_children(app, world.player, math, screenshot) == 0);
        CHECK(hand_failure_controls(app, &world, math) == 0);
    }
    CHECK(hand_world_close(&world, app) == 0);
    CHECK(mc_math_classes_release(&math, NULL) == MC_MATH_CLASS_OK);
    mc_client_render_texture_error *terminal = calloc(1, sizeof(*terminal));
    CHECK(terminal != NULL);
    mc_client_render_manager_classes *manager = attempt->manager_classes;
    CHECK(mc_client_application_startup_retire(&attempt, startup.error) == MC_CLIENT_STARTUP_BUSY);
    CHECK(mc_client_render_manager_retire_hands(manager, &startup.error->manager) ==
          MC_CLIENT_MANAGER_OK);
    CHECK(mc_client_render_manager_classes_retire(&manager, &startup.error->manager) ==
          MC_CLIENT_MANAGER_OK);
    CHECK(mc_client_application_startup_retire(&attempt, startup.error) == MC_CLIENT_STARTUP_BUSY);
    CHECK(mc_client_render_engine_retire_resources(runtime.graphics, terminal) ==
          MC_CLIENT_RENDER_TEXTURE_OK);
    free(terminal);
    CHECK(mc_client_application_startup_retire(&attempt, startup.error) == MC_CLIENT_STARTUP_OK &&
          attempt == NULL);
    CHECK(statistics_end_app(&runtime, &startup, &app) == 0);
    return 0;
}

int main(int argc, char **argv) {
    CHECK(argc == 6 &&
          hand_device(argv[1], argv[2], argv[3], argv[4], strcmp(argv[5], "arm") == 0) == 0);
    printf("First-person hand: %u checks; %s phase\n", checks, argv[5]);
    return 0;
}
