/* Native component controls with real constructors/World/DN, independent
 * constructor entropy and actual layer1 draw. The authored World is separate
 * from ordinary saved-World input qualification. */
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#define MC_ITEM_PARTICLE_MAIN mc_digging_inherited_fixture_main
#include "../../../src/client/runtime/gameplay_input.h"
#include "../test_item_particle.c"

static int digging_controls(ip_fixture *f) {
    mc_registry_classes *registry = f->runtime.services.registry;
    const mc_block *dirt = NULL;
    CHECK(mc_registry_block_get(registry, 3, &dirt, NULL) == MC_REGISTRY_OK && dirt != NULL);
    const uint64_t world_random = f->world->fields.r->state;
    const size_t before = f->runtime.services.entities->live_entities;
    mc_java_random expected = *f->effects->random;
    int32_t faces[64];
    for (size_t i = 0; i < 64; ++i)
        CHECK(mc_java_random_next_bounded(&expected, 6, &faces[i]));
    CHECK(mc_client_effect_renderer_destroy_block(f->effects, registry, f->math, 8, 66, 12, 3, 0,
                                                  f->effects_error) == MC_EFFECT_RENDERER_OK);
    const mc_effect_block_state *work = f->effects_error->block;
    CHECK(work != NULL && work->returned && !work->hit && work->source_pc == 184 &&
          work->constructed == 64 && work->appended == 64 && work->random_calls == 64 &&
          work->candidate == NULL && f->effects->random->state == expected.state &&
          f->world->fields.r->state == world_random &&
          f->runtime.services.entities->live_entities == before + 64 &&
          f->effects->layers[1]->size == 64);
    /* A second real producer call rejects the pending first invocation. Its
     * gameplay capture may release its own DN occurrence, but must not clean
     * the original caller's pending receipt or consume any particle effects. */
    mc_client_gameplay_input *rejected = calloc(1, sizeof(*rejected));
    CHECK(rejected != NULL);
    const void *pending_work = f->effects->block_call;
    const size_t producer_holds = f->effects->holds;
    CHECK(mc_client_effect_renderer_retain(f->effects) == MC_EFFECT_RENDERER_OK);
    rejected->hit_renderer = f->effects;
    CHECK(mc_client_effect_renderer_hit_block(f->effects, registry, f->math, 8, 63, 12, 1,
                                              &rejected->hit_effect) == MC_EFFECT_RENDERER_BUSY &&
          rejected->hit_effect.block == NULL && f->effects->block_call == pending_work);
    rejected->hit_block_work = rejected->hit_effect.block != NULL;
    CHECK(mc_client_gameplay_input_retire(rejected) && rejected->hit_renderer == NULL &&
          f->effects->holds == producer_holds && f->effects->block_call == pending_work &&
          f->effects->random->state == expected.state && f->effects->layers[1]->size == 64 &&
          work->returned && work->constructed == 64 && work->appended == 64);
    free(rejected);
    for (size_t i = 0; i < 64; ++i) {
        mc_entity *particle = mc_client_effect_renderer_particle(f->effects, 1, i);
        mc_particle_snapshot state;
        int32_t layer = 0;
        CHECK(particle != NULL && mc_particle_read(particle, &state) &&
              state.phase == MC_ENTITY_READY && state.item_pc == 80 &&
              state.digging_block == dirt && state.digging_face == faces[i] &&
              work->destruction_faces[i] == faces[i] && state.particle.i == .6f &&
              state.particle.j == .6f && state.particle.k == .6f &&
              state.particle.b == dirt->fields.bm &&
              mc_digging_particle_layer(particle, &layer) == MC_ENTITY_OK && layer == 1);
        const double x = 8. + ((double)(i / 16) + .5) / 4.;
        const double y = 66. + ((double)((i / 4) % 4) + .5) / 4.;
        const double z = 12. + ((double)(i % 4) + .5) / 4.;
        CHECK(particle->base.x == x && particle->base.y == y && particle->base.z == z &&
              state.particle.g >= .5f && state.particle.g <= 1.f &&
              state.particle.h == dirt->fields.bz);
    }
    CHECK(mc_client_effect_renderer_tick_in_context(f->effects, registry, NULL, f->effects_error) ==
          MC_EFFECT_RENDERER_BUSY);
    CHECK(mc_client_effect_renderer_block_cleanup(f->effects, f->effects_error) ==
          MC_EFFECT_RENDERER_OK);
    CHECK(mc_client_effect_renderer_tick_in_context(f->effects, registry, NULL, f->effects_error) ==
          MC_EFFECT_RENDERER_OK);
    CHECK(f->effects->layers[1]->size == 64);
    for (size_t i = 0; i < 64; ++i) {
        mc_particle_snapshot state;
        CHECK(mc_particle_read(mc_client_effect_renderer_particle(f->effects, 1, i), &state) &&
              state.particle.e == 1 && state.tick_pc == 147 && !state.tick_active);
    }
    CHECK(ip_frame(f) == 0);
    const mc_effect_draw_services services = {
        .graphics = f->runtime.graphics, .math = f->math, .display = f->display};
    CHECK(mc_client_effect_renderer_sprites_begin(f->effects, f->camera, .375f, &services, &f->call,
                                                  f->draw_error) == MC_EFFECT_DRAW_OK);
    mc_effect_draw_state drawn;
    CHECK(mc_client_effect_renderer_draw_inspect(f->call, &drawn) && drawn.returned &&
          drawn.visited == 64 && drawn.item_particle_returned &&
          drawn.item_vertices_returned == 4 && drawn.particle_pc == 361 &&
          drawn.item_u1 > drawn.item_u0 && drawn.item_v1 > drawn.item_v0);
    CHECK(mc_client_effect_renderer_draw_retire(&f->call, f->draw_error) == MC_EFFECT_DRAW_OK);
    CHECK(mc_client_effect_renderer_change_world(f->effects, f->world, f->effects_error) ==
          MC_EFFECT_RENDERER_OK);
    CHECK(f->runtime.services.entities->live_entities == before);
    for (int32_t face = 0; face < 6; ++face) {
        expected = *f->effects->random;
        double samples[3];
        for (size_t axis = 0; axis < 3; ++axis)
            samples[axis] = mc_java_random_next_double(&expected);
        CHECK(mc_client_effect_renderer_hit_block(f->effects, registry, f->math, 8, 63, 12, face,
                                                  f->effects_error) == MC_EFFECT_RENDERER_OK);
        work = f->effects_error->block;
        CHECK(work != NULL && work->hit && work->returned && work->source_pc == 323 &&
              work->block_id == 3 && work->metadata == 0 && work->constructed == 1 &&
              work->appended == 1 && work->random_calls == 3 && work->face == face &&
              f->effects->random->state == expected.state && f->effects->layers[1]->size == 1);
        mc_entity *hit = mc_client_effect_renderer_particle(f->effects, 1, 0);
        CHECK(hit != NULL && hit->base.width == .2f * .6f && hit->base.height == .2f * .6f &&
              hit->particle->digging_face == face && hit->particle->digging_block == dirt);
        for (size_t axis = 0; axis < 3; ++axis)
            CHECK(work->hit_random[axis] == samples[axis]);
        const int axis = face < 2 ? 1 : face < 4 ? 2 : 0;
        const double coordinate = axis == 0 ? 8. : axis == 1 ? 63. : 12.;
        const double expected_edge =
            (face % 2) == 0 ? coordinate - (double).1f : (coordinate + 1.) + (double).1f;
        CHECK(work->position[axis] == expected_edge);
        CHECK(mc_client_effect_renderer_block_cleanup(f->effects, f->effects_error) ==
                  MC_EFFECT_RENDERER_OK &&
              mc_client_effect_renderer_change_world(f->effects, f->world, f->effects_error) ==
                  MC_EFFECT_RENDERER_OK);
    }
    /* ID0's real source return must consume no DN Random or Entity ID. */
    const uint64_t own = f->effects->random->state;
    const int32_t next = f->runtime.services.entities->next_id;
    CHECK(mc_client_effect_renderer_destroy_block(f->effects, NULL, NULL, 0, 0, 0, 0, 0,
                                                  f->effects_error) == MC_EFFECT_RENDERER_OK &&
          f->effects_error->block->source_pc == 5 && f->effects_error->block->constructed == 0 &&
          f->effects->random->state == own && f->runtime.services.entities->next_id == next);
    CHECK(mc_client_effect_renderer_block_cleanup(f->effects, f->effects_error) ==
          MC_EFFECT_RENDERER_OK);
    /* A real null Block fails after XW construction, before QM icon storage. */
    CHECK(mc_digging_particle_allocate(f->runtime.services.entities, &f->particle, NULL) ==
          MC_ENTITY_OK);
    CHECK(mc_digging_particle_construct(f->particle, f->world, registry, f->math, 8, 66, 12, 0, 0,
                                        0, NULL, 0, 0, f->particle_error) != MC_ENTITY_OK &&
          f->particle_error->source_pc == 33 && f->particle_error->source_exception &&
          f->particle->phase == MC_ENTITY_FAILED_CONSTRUCTION &&
          f->particle_error->constructor_random.math_count == 5 &&
          f->particle_error->constructor_random.own_count == 4);
    mc_entity_release(f->particle);
    f->particle = NULL;
    CHECK(f->runtime.services.entities->live_entities == before);
    return 0;
}

int main(int argc, char **argv) {
    CHECK(argc == 4);
    ip_fixture *f = calloc(1, sizeof(*f));
    CHECK(f != NULL);
    CHECK(ip_open(f, argv[1], argv[2], argv[3]) == 0);
    f->particle_error = calloc(1, sizeof(*f->particle_error));
    f->tick_error = calloc(1, sizeof(*f->tick_error));
    f->effects_error = calloc(1, sizeof(*f->effects_error));
    CHECK(f->particle_error && f->tick_error && f->effects_error);
    CHECK(ip_construct(f, MC_ENTITY_CLASS_KIND_COW, &f->camera) == 0 &&
          ip_pose(f->camera, 8.5, 65, 12.5, 30, 15) == 0);
    f->app->i = f->camera;
    bool published = false;
    CHECK(mc_world_runtime_insert_entity(f->world, f->camera, &published, NULL) ==
              MC_WORLD_STORAGE_OK &&
          published);
    CHECK(mc_client_effect_renderer_allocate(f->runtime.services.entities, NULL, &f->effects,
                                             f->effects_error) == MC_EFFECT_RENDERER_OK &&
          mc_client_effect_renderer_construct(f->effects, f->world, f->app->render_engine,
                                              f->effects_error) == MC_EFFECT_RENDERER_OK);
    f->app->effect_renderer = f->effects;
    CHECK(ip_n_open(f) == 0 && ip_nw_prepare(f) == 0 && digging_controls(f) == 0);
    CHECK(ip_close(f) == 0);
    free(f);
    printf("Digging particles: %u checks; genuine QM/XW/World/DN\n", checks);
    return 0;
}
