#include "../../../src/client/particles/effect_renderer_internal.h"
#include "../../../src/entity/entity_internal.h"
#include "../../../src/entity/particles/particle_internal.h"
#include "../../../src/world/runtime/runtime_internal.h"
#include "mc/world/runtime/entities.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned int checks;
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition);                        \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

static bool terminal_case(mc_client_effect_renderer *renderer, mc_effect_renderer_status expected) {
    bool result = false;
    mc_client_effect_renderer saved;
    mc_entity_classes saved_classes;
    mc_effect_particle_list *list_identities[4] = {0};
    mc_effect_particle_list lists[4];
    mc_entity **elements[4] = {0};
    size_t *particle_holds[4] = {0};
    mc_java_random random;
    memcpy(&saved, renderer, sizeof(saved));
    memcpy(&saved_classes, renderer->classes, sizeof(saved_classes));
    if (renderer->random != NULL)
        memcpy(&random, renderer->random, sizeof(random));
    const size_t world_bindings =
        renderer->world == NULL ? 0 : renderer->world->persistent_bindings;
    mc_world_storage_error world_error;
    if (renderer->world != NULL)
        memcpy(&world_error, &renderer->world->last_error, sizeof(world_error));
    if (renderer->layers != NULL) {
        memcpy(list_identities, renderer->layers, sizeof(list_identities));
        for (size_t i = 0; i < 4; ++i) {
            const mc_effect_particle_list *list = renderer->layers[i];
            if (list == NULL)
                continue;
            memcpy(&lists[i], list, sizeof(lists[i]));
            if (list->capacity != 0) {
                elements[i] = malloc(list->capacity * sizeof(*list->elements));
                CHECK(elements[i] != NULL);
                memcpy(elements[i], list->elements, list->capacity * sizeof(*list->elements));
            }
            if (list->size != 0) {
                particle_holds[i] = malloc(list->size * sizeof(*particle_holds[i]));
                CHECK(particle_holds[i] != NULL);
                for (size_t j = 0; j < list->size; ++j)
                    particle_holds[i][j] = list->elements[j]->holds;
            }
        }
    }
    mc_effect_renderer_terminal_view view, sentinel;
    mc_effect_renderer_error error;
    memset(&view, 0xa5, sizeof(view));
    memcpy(&sentinel, &view, sizeof(sentinel));
    memset(&error, 0xa5, sizeof(error));
    CHECK(mc_client_effect_renderer_terminal_inspect(renderer, &view, &error) == expected &&
          error.status == expected && error.source_method == NULL &&
          error.source_descriptor == NULL && error.source_pc == 0 && error.index == SIZE_MAX);
    if (expected == MC_EFFECT_RENDERER_OK) {
        CHECK(view.classes == renderer->classes && view.render_engine == renderer->render_engine &&
              view.world_slot == &renderer->world && *view.world_slot == renderer->world &&
              view.holds == renderer->holds && error.operation == NULL && error.layer == SIZE_MAX);
    } else {
        CHECK(memcmp(&sentinel, &view, sizeof(view)) == 0);
        if (expected == MC_EFFECT_RENDERER_UNSUPPORTED_PARTICLE)
            CHECK(error.layer == 3 && renderer->layers[3]->size != 0);
    }
    if (expected == MC_EFFECT_RENDERER_OK)
        view = (mc_effect_renderer_terminal_view){0};
    memcpy(&sentinel, &view, sizeof(sentinel));
    CHECK(mc_client_effect_renderer_terminal_inspect(renderer, &view, NULL) == expected);
    if (expected == MC_EFFECT_RENDERER_OK)
        CHECK(view.classes == renderer->classes && view.render_engine == renderer->render_engine &&
              view.world_slot == &renderer->world && *view.world_slot == renderer->world &&
              view.holds == renderer->holds);
    else
        CHECK(memcmp(&sentinel, &view, sizeof(view)) == 0);
    CHECK(mc_client_effect_renderer_terminal_inspect(renderer, NULL, &error) ==
              MC_EFFECT_RENDERER_INVALID_ARGUMENT &&
          error.status == MC_EFFECT_RENDERER_INVALID_ARGUMENT);
    memcpy(&sentinel, &view, sizeof(sentinel));
    CHECK(mc_client_effect_renderer_terminal_inspect(NULL, &view, &error) ==
              MC_EFFECT_RENDERER_INVALID_ARGUMENT &&
          error.status == MC_EFFECT_RENDERER_INVALID_ARGUMENT &&
          memcmp(&sentinel, &view, sizeof(view)) == 0);
    CHECK(mc_client_effect_renderer_terminal_inspect(NULL, &view, NULL) ==
              MC_EFFECT_RENDERER_INVALID_ARGUMENT &&
          memcmp(&sentinel, &view, sizeof(view)) == 0);
    CHECK(mc_client_effect_renderer_terminal_inspect(renderer, NULL, NULL) ==
          MC_EFFECT_RENDERER_INVALID_ARGUMENT);
    CHECK(memcmp(&saved, renderer, sizeof(saved)) == 0 &&
          memcmp(&saved_classes, renderer->classes, sizeof(saved_classes)) == 0);
    if (renderer->random != NULL)
        CHECK(memcmp(&random, renderer->random, sizeof(random)) == 0);
    if (renderer->world != NULL)
        CHECK(renderer->world->persistent_bindings == world_bindings &&
              memcmp(&world_error, &renderer->world->last_error, sizeof(world_error)) == 0);
    if (renderer->layers != NULL) {
        CHECK(memcmp(list_identities, renderer->layers, sizeof(list_identities)) == 0);
        for (size_t i = 0; i < 4; ++i) {
            const mc_effect_particle_list *list = renderer->layers[i];
            if (list == NULL)
                continue;
            CHECK(memcmp(&lists[i], list, sizeof(lists[i])) == 0);
            if (list->capacity != 0)
                CHECK(memcmp(elements[i], list->elements,
                             list->capacity * sizeof(*list->elements)) == 0);
            for (size_t j = 0; j < list->size; ++j)
                CHECK(particle_holds[i][j] == list->elements[j]->holds);
        }
    }
    result = true;
cleanup:
    for (size_t i = 0; i < 4; ++i) {
        free(elements[i]);
        free(particle_holds[i]);
    }
    return result;
}

int main(void) {
    int result = 1;
    mc_java_random_profile *profile = NULL;
    mc_entity_classes *classes = NULL, *server = NULL;
    mc_client_effect_renderer *renderer = NULL, *failed = NULL;
    mc_entity *actor = NULL, *a = NULL, *b = NULL;
    mc_effect_renderer_error error;
    mc_effect_renderer_snapshot snapshot;
    mc_world_runtime_classes *world_classes = NULL;
    mc_world_runtime *world = NULL;
    mc_math_trig *trig = NULL;
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
    CHECK(terminal_case(renderer, MC_EFFECT_RENDERER_INVALID_PHASE));
    CHECK(mc_client_effect_renderer_tick(renderer, &error) == MC_EFFECT_RENDERER_INVALID_PHASE);
    CHECK(mc_client_effect_renderer_construct(renderer, NULL, NULL, &error) ==
          MC_EFFECT_RENDERER_OK);
    CHECK(mc_client_effect_renderer_read(renderer, &snapshot) &&
          snapshot.phase == MC_EFFECT_RENDERER_READY && snapshot.constructor_pc == 63 &&
          snapshot.layers != NULL && snapshot.random != NULL && snapshot.world == NULL &&
          snapshot.render_engine == NULL && classes->entity_phase == MC_ENTITY_CLASS_COLD);
    CHECK(terminal_case(renderer, MC_EFFECT_RENDERER_OK));
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
    CHECK(terminal_case(renderer, MC_EFFECT_RENDERER_UNSUPPORTED_PARTICLE) && a->holds == 2 &&
          actor->holds == 4 && a->particle->pickup.p == 0 && !a->base.be);
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
    CHECK(renderer->layers[3]->capacity != 0 && terminal_case(renderer, MC_EFFECT_RENDERER_OK));
    /* A genuine RAW World is valid for the reached dn World binding. These
     * controls do not construct World fields or run a World source body. */
    trig = malloc(sizeof(*trig));
    CHECK(trig != NULL && mc_math_trig_init(trig));
    CHECK(mc_world_runtime_classes_create(MC_WORLD_INFO_CLIENT, profile, trig, &world_classes,
                                          NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_classes_bind_entities(world_classes, classes) == MC_ENTITY_OK);
    CHECK(mc_world_runtime_allocate_world(MC_WORLD_RUNTIME_CLIENT_DIRECT_METADATA, world_classes,
                                          NULL, &world, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(world->construction.phase == MC_WORLD_RUNTIME_RAW && world->persistent_bindings == 0);
    CHECK(mc_client_effect_renderer_change_world(renderer, world, &error) ==
              MC_EFFECT_RENDERER_OK &&
          renderer->world == world && world->persistent_bindings == 1);
    CHECK(terminal_case(renderer, MC_EFFECT_RENDERER_OK) && world->persistent_bindings == 1);
    CHECK(mc_client_effect_renderer_retain(renderer) == MC_EFFECT_RENDERER_OK);
    CHECK(renderer->holds == 2 && terminal_case(renderer, MC_EFFECT_RENDERER_OK));
    CHECK(mc_client_effect_renderer_release(renderer) == MC_EFFECT_RENDERER_OK &&
          renderer->holds == 1);
    CHECK(mc_client_effect_renderer_change_world(renderer, NULL, &error) == MC_EFFECT_RENDERER_OK &&
          world->persistent_bindings == 0 && terminal_case(renderer, MC_EFFECT_RENDERER_OK));
    CHECK(mc_world_runtime_free(world, NULL) == MC_WORLD_STORAGE_OK);
    world = NULL;
    CHECK(mc_world_runtime_classes_free(world_classes, NULL) == MC_WORLD_STORAGE_OK);
    world_classes = NULL;
    free(trig);
    trig = NULL;
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
    CHECK(terminal_case(renderer, MC_EFFECT_RENDERER_OK));
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
        CHECK(terminal_case(failed, MC_EFFECT_RENDERER_INVALID_PHASE));
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
    CHECK(terminal_case(failed, MC_EFFECT_RENDERER_INVALID_PHASE));
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
    if (mc_world_runtime_free(world, NULL) != MC_WORLD_STORAGE_OK ||
        mc_world_runtime_classes_free(world_classes, NULL) != MC_WORLD_STORAGE_OK)
        result = 1;
    free(trig);
    if (mc_entity_classes_free(classes) != MC_ENTITY_OK ||
        mc_entity_classes_free(server) != MC_ENTITY_OK)
        result = 1;
    mc_java_random_profile_free(profile);
    printf("EffectRenderer: %u checks, %s\n", checks, result == 0 ? "pass" : "fail");
    return result;
}
