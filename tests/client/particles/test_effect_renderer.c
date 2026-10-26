#include "../../../src/client/particles/effect_renderer_internal.h"
#include "../../../src/entity/entity_internal.h"
#include "../../../src/entity/particles/particle_internal.h"

#include <stdio.h>

static unsigned int checks;
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition);                        \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

int main(void) {
    int result = 1;
    mc_java_random_profile *profile = NULL;
    mc_entity_classes *classes = NULL, *server = NULL;
    mc_client_effect_renderer *renderer = NULL, *failed = NULL;
    mc_entity *actor = NULL, *a = NULL, *b = NULL;
    mc_effect_renderer_error error;
    mc_effect_renderer_snapshot snapshot;
    CHECK(mc_java_random_profile_create(65536, &profile) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_entity_classes_create(MC_ENTITY_CLIENT, profile, NULL, &classes) == MC_ENTITY_OK);
    CHECK(mc_entity_classes_create(MC_ENTITY_SERVER, profile, NULL, &server) == MC_ENTITY_OK);
    CHECK(mc_client_effect_renderer_allocate(server, NULL, &renderer, &error) ==
              MC_EFFECT_RENDERER_INVALID_ARGUMENT &&
          renderer == NULL);
    const mc_effect_renderer_options no_memory = {0};
    CHECK(mc_client_effect_renderer_allocate(classes, &no_memory, &renderer, &error) ==
              MC_EFFECT_RENDERER_LIMIT &&
          renderer == NULL);
    CHECK(mc_client_effect_renderer_allocate(classes, NULL, &renderer, &error) ==
          MC_EFFECT_RENDERER_OK);
    CHECK(mc_client_effect_renderer_read(renderer, &snapshot) &&
          snapshot.phase == MC_EFFECT_RENDERER_RAW && snapshot.layers == NULL &&
          snapshot.random == NULL && snapshot.world == NULL && classes->borrowers == 1 &&
          classes->entity_phase == MC_ENTITY_CLASS_COLD && classes->next_id == 0);
    CHECK(mc_client_effect_renderer_tick(renderer, &error) == MC_EFFECT_RENDERER_INVALID_PHASE);
    CHECK(mc_client_effect_renderer_construct(renderer, NULL, NULL, &error) ==
          MC_EFFECT_RENDERER_OK);
    CHECK(mc_client_effect_renderer_read(renderer, &snapshot) &&
          snapshot.phase == MC_EFFECT_RENDERER_READY && snapshot.constructor_pc == 63 &&
          snapshot.layers != NULL && snapshot.random != NULL && snapshot.world == NULL &&
          snapshot.render_engine == NULL && classes->entity_phase == MC_ENTITY_CLASS_COLD);
    for (size_t i = 0; i < 4; ++i) {
        CHECK(snapshot.lists[i] != NULL && snapshot.sizes[i] == 0 && snapshot.capacities[i] == 0);
        for (size_t j = 0; j < i; ++j)
            CHECK(snapshot.lists[i] != snapshot.lists[j]);
    }
    const size_t complete_bytes = snapshot.allocation_bytes;
    CHECK(complete_bytes == sizeof(*renderer) + 4 * sizeof(*renderer->layers) +
                                sizeof(*renderer->random) + 4 * sizeof(mc_effect_particle_list));
    CHECK(mc_client_effect_renderer_construct(renderer, NULL, NULL, &error) ==
          MC_EFFECT_RENDERER_INVALID_PHASE);
    CHECK(mc_client_effect_renderer_add(renderer, NULL, &error) ==
              MC_EFFECT_RENDERER_NULL_REFERENCE &&
          error.source_pc == 1);
    CHECK(mc_falling_entity_allocate(classes, &actor, NULL) == MC_ENTITY_OK);
    CHECK(mc_falling_entity_construct_world(actor, NULL, NULL) == MC_ENTITY_OK);
    CHECK(mc_client_effect_renderer_add(renderer, actor, &error) ==
              MC_EFFECT_RENDERER_UNSUPPORTED_PARTICLE &&
          error.source_pc == 1);
    CHECK(mc_pickup_particle_allocate(classes, &a, NULL) == MC_ENTITY_OK);
    CHECK(mc_client_effect_renderer_add(renderer, a, &error) == MC_EFFECT_RENDERER_INVALID_PHASE);
    CHECK(mc_pickup_particle_construct(a, NULL, actor, actor, -.5f, NULL) == MC_ENTITY_OK);
    CHECK(actor->holds == 3);
    CHECK(mc_pickup_particle_allocate(classes, &b, NULL) == MC_ENTITY_OK);
    CHECK(mc_pickup_particle_construct(b, NULL, actor, NULL, -.5f, NULL) == MC_ENTITY_OK);
    CHECK(actor->holds == 4);
    CHECK(mc_client_effect_renderer_add(renderer, a, &error) == MC_EFFECT_RENDERER_OK &&
          a->holds == 2);
    CHECK(mc_client_effect_renderer_particle(renderer, 3, 0) == a &&
          mc_client_effect_renderer_particle(renderer, 4, 0) == NULL &&
          mc_client_effect_renderer_particle(renderer, 3, 1) == NULL);
    CHECK(mc_client_effect_renderer_tick(renderer, &error) == MC_EFFECT_RENDERER_OK &&
          a->particle->pickup.p == 1 && !a->base.be && renderer->layers[3]->size == 1);
    CHECK(mc_client_effect_renderer_add(renderer, b, &error) == MC_EFFECT_RENDERER_OK);
    CHECK(mc_client_effect_renderer_tick(renderer, &error) == MC_EFFECT_RENDERER_OK &&
          a->particle->pickup.p == 2 && b->particle->pickup.p == 1 &&
          renderer->layers[3]->size == 2);
    CHECK(mc_client_effect_renderer_tick(renderer, &error) == MC_EFFECT_RENDERER_OK &&
          a->particle->pickup.p == 3 && a->base.be && a->holds == 1 && b->particle->pickup.p == 2 &&
          !b->base.be && renderer->layers[3]->size == 1 &&
          mc_client_effect_renderer_particle(renderer, 3, 0) == b);
    CHECK(mc_client_effect_renderer_tick(renderer, &error) == MC_EFFECT_RENDERER_OK && b->base.be &&
          b->holds == 1 && renderer->layers[3]->size == 0);
    /* Reset private native fixture state before exercising the source cap. */
    a->particle->pickup.p = b->particle->pickup.p = 0;
    a->base.be = b->base.be = false;
    for (size_t i = 0; i < 4000; ++i)
        CHECK(mc_client_effect_renderer_add(renderer, a, &error) == MC_EFFECT_RENDERER_OK);
    CHECK(renderer->layers[3]->size == 4000 && a->holds == 4001 && b->holds == 1);
    CHECK(mc_client_effect_renderer_add(renderer, b, &error) == MC_EFFECT_RENDERER_OK &&
          renderer->layers[3]->size == 4000 && a->holds == 4000 && b->holds == 2);
    for (size_t i = 0; i < 3999; ++i)
        CHECK(mc_client_effect_renderer_particle(renderer, 3, i) == a);
    CHECK(mc_client_effect_renderer_particle(renderer, 3, 3999) == b);
    /* Removing the current index must not skip the next occurrence. The real
     * em override kills a on its third visit; later occurrences remain dead. */
    CHECK(mc_client_effect_renderer_tick(renderer, &error) == MC_EFFECT_RENDERER_OK &&
          a->particle->pickup.p == 3999 && a->base.be && a->holds == 3 &&
          b->particle->pickup.p == 1 && !b->base.be && renderer->layers[3]->size == 3);
    CHECK(mc_client_effect_renderer_particle(renderer, 3, 0) == a &&
          mc_client_effect_renderer_particle(renderer, 3, 1) == a &&
          mc_client_effect_renderer_particle(renderer, 3, 2) == b);
    /* World-clear keeps the four source List identities and backing capacity. */
    CHECK(mc_client_effect_renderer_read(renderer, &snapshot));
    CHECK(mc_client_effect_renderer_change_world(renderer, NULL, &error) == MC_EFFECT_RENDERER_OK &&
          a->holds == 1 && b->holds == 1);
    for (size_t i = 0; i < 4; ++i)
        CHECK(snapshot.lists[i] == renderer->layers[i] && renderer->layers[i]->size == 0 &&
              snapshot.capacities[i] == renderer->layers[i]->capacity);
    /* A List growth failure preserves its exact prior contents. A reached
     * retain failure keeps the capacity growth but stores no occurrence. */
    renderer->max_allocation_bytes = renderer->allocation_bytes;
    const size_t held = b->holds;
    b->holds = SIZE_MAX;
    CHECK(mc_client_effect_renderer_add(renderer, b, &error) == MC_EFFECT_RENDERER_ENTITY_FAILED &&
          error.source_pc == 42 && renderer->layers[3]->size == 0);
    b->holds = held;
    renderer->max_allocation_bytes = 16u * 1024u * 1024u;
    /* Saturated cap eviction precedes the failed new native occurrence hold. */
    for (size_t i = 0; i < 4000; ++i)
        CHECK(mc_client_effect_renderer_add(renderer, a, &error) == MC_EFFECT_RENDERER_OK);
    b->holds = SIZE_MAX;
    CHECK(mc_client_effect_renderer_add(renderer, b, &error) == MC_EFFECT_RENDERER_ENTITY_FAILED &&
          error.source_pc == 42 && renderer->layers[3]->size == 3999 && a->holds == 4000);
    b->holds = held;
    CHECK(mc_client_effect_renderer_change_world(renderer, NULL, &error) == MC_EFFECT_RENDERER_OK &&
          a->holds == 1);
    renderer->entered = true;
    CHECK(mc_client_effect_renderer_tick(renderer, &error) == MC_EFFECT_RENDERER_BUSY &&
          mc_client_effect_renderer_retain(renderer) == MC_EFFECT_RENDERER_BUSY &&
          mc_client_effect_renderer_free(&renderer) == MC_EFFECT_RENDERER_BUSY);
    renderer->entered = false;
    CHECK(mc_client_effect_renderer_retain(renderer) == MC_EFFECT_RENDERER_OK &&
          mc_client_effect_renderer_free(&renderer) == MC_EFFECT_RENDERER_BUSY &&
          mc_client_effect_renderer_release(renderer) == MC_EFFECT_RENDERER_OK);
    /* Constructor allocation policies fail at each genuine allocation site,
     * retaining exactly the already-stored List/Random prefix. */
    for (size_t lists = 0; lists < 4; ++lists) {
        const mc_effect_renderer_options options = {
            sizeof(*renderer) + 4 * sizeof(*renderer->layers) + sizeof(*renderer->random) +
            lists * sizeof(mc_effect_particle_list)};
        CHECK(mc_client_effect_renderer_allocate(classes, &options, &failed, &error) ==
              MC_EFFECT_RENDERER_OK);
        CHECK(mc_client_effect_renderer_construct(failed, NULL, NULL, &error) ==
                  MC_EFFECT_RENDERER_LIMIT &&
              error.source_pc == 49 && error.layer == lists &&
              failed->phase == MC_EFFECT_RENDERER_FAILED_CONSTRUCTION && failed->random != NULL &&
              failed->constructor_pc == (lists == 0 ? 34u : 56u));
        for (size_t i = 0; i < 4; ++i)
            CHECK((failed->layers[i] != NULL) == (i < lists));
        CHECK(mc_client_effect_renderer_construct(failed, NULL, NULL, &error) ==
              MC_EFFECT_RENDERER_INVALID_PHASE);
        CHECK(mc_client_effect_renderer_free(&failed) == MC_EFFECT_RENDERER_OK && failed == NULL);
    }
    const mc_effect_renderer_options raw_budget = {sizeof(*renderer)};
    CHECK(mc_client_effect_renderer_allocate(classes, &raw_budget, &failed, &error) ==
          MC_EFFECT_RENDERER_OK);
    CHECK(mc_client_effect_renderer_construct(failed, NULL, NULL, &error) ==
              MC_EFFECT_RENDERER_LIMIT &&
          error.source_pc == 6 && failed->layers == NULL && failed->constructor_pc == 1);
    CHECK(mc_client_effect_renderer_free(&failed) == MC_EFFECT_RENDERER_OK);
    const mc_effect_renderer_options array_budget = {sizeof(*renderer) +
                                                     4 * sizeof(*renderer->layers)};
    CHECK(mc_client_effect_renderer_allocate(classes, &array_budget, &failed, &error) ==
          MC_EFFECT_RENDERER_OK);
    CHECK(mc_client_effect_renderer_construct(failed, NULL, NULL, &error) ==
              MC_EFFECT_RENDERER_LIMIT &&
          error.source_pc == 13 && failed->layers != NULL && failed->random == NULL &&
          failed->constructor_pc == 9);
    CHECK(mc_client_effect_renderer_free(&failed) == MC_EFFECT_RENDERER_OK);
    /* Retire the caller's actors first: the listed particle keeps both actual
     * actor references alive until the last manager occurrence is detached. */
    CHECK(mc_client_effect_renderer_add(renderer, a, &error) == MC_EFFECT_RENDERER_OK);
    mc_entity_release(b);
    b = NULL;
    mc_entity_release(actor);
    actor = NULL;
    mc_entity_release(a);
    a = NULL;
    CHECK(mc_client_effect_renderer_free(&renderer) == MC_EFFECT_RENDERER_OK && renderer == NULL &&
          classes->live_entities == 0 && classes->borrowers == 0);
    result = 0;
cleanup:
    (void)mc_client_effect_renderer_free(&failed);
    (void)mc_client_effect_renderer_free(&renderer);
    mc_entity_release(a);
    mc_entity_release(b);
    mc_entity_release(actor);
    if (mc_entity_classes_free(classes) != MC_ENTITY_OK ||
        mc_entity_classes_free(server) != MC_ENTITY_OK)
        result = 1;
    mc_java_random_profile_free(profile);
    printf("EffectRenderer: %u checks, %s\n", checks, result == 0 ? "pass" : "fail");
    return result;
}
