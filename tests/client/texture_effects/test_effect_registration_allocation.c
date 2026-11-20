#include <stdbool.h>
#include <stdlib.h>
static bool effect_reallocation_fails;
static size_t effect_reallocation_calls;

static void *effect_test_reallocate(void *memory, size_t bytes) {
    ++effect_reallocation_calls;
    return effect_reallocation_fails ? NULL : realloc(memory, bytes);
}

#define realloc effect_test_reallocate
#include "../../../src/client/render_engine/effects.c"
#undef realloc
#define MC_TEXTURE_EFFECT_REGISTRATION_MAIN mc_effect_unused_registration_fixture_main
#include "test_effect_registration.c"
#undef MC_TEXTURE_EFFECT_REGISTRATION_MAIN

static bool allocation_case(fx_effects *f) {
    mc_client_render_engine_classes *classes = NULL;
    mc_client_default_font *font = NULL;
    mc_client_render_engine *engine = NULL;
    NEED(mc_client_render_engine_classes_create(NULL, &classes) == MC_CLIENT_RENDER_ENGINE_OK);
    NEED(mc_client_default_font_create(32u * 1024u * 1024u, &font) == MC_CLIENT_FONT_OK);
    NEED(mc_client_render_engine_allocate(classes, font, NULL, &engine) ==
         MC_CLIENT_RENDER_ENGINE_OK);
    NEED(mc_client_render_engine_construct(engine, NULL, NULL, NULL) == MC_CLIENT_RENDER_ENGINE_OK);
    mc_client_effect_registration_error *e = calloc(1, sizeof(*e));
    NEED(e != NULL);
    const mc_texture_effect_services service = services(f);
    mc_client_texture_effect *fire = NULL;
    NEED(mc_client_texture_effect_allocate(&service, MC_TEXTURE_EFFECT_FIRE, NULL, &fire,
                                           &e->child) == MC_TEXTURE_EFFECT_OK);
    NEED(mc_client_texture_effect_construct(fire, NULL, 0, &e->child) == MC_TEXTURE_EFFECT_OK);
    mc_texture_effect_state before, after;
    NEED(mc_client_texture_effect_inspect(fire, &before));
    effect_reallocation_fails = true;
    NEED(!mc_client_render_engine_register_effect(engine, fire, 0, e));
    effect_reallocation_fails = false;
    NEED(effect_reallocation_calls == 1 && e->status == MC_TEXTURE_EFFECT_ALLOCATION_FAILED &&
         e->ji_pc == 5 && !e->appended);
    NEED(engine->fields.g->count == 0 && engine->fields.g->entries == NULL);
    NEED(mc_client_texture_effect_inspect(fire, &after) &&
         before.native_holds == after.native_holds && after.last_store_pc == before.last_store_pc);
    NEED(mc_client_render_engine_register_effect(engine, fire, 0, e) && e->source_returned);
    NEED(mc_client_render_engine_retire_effects(engine, e));
    NEED(mc_client_texture_effect_release(&fire, &e->child) == MC_TEXTURE_EFFECT_OK);
    NEED(mc_client_render_engine_release(engine) == MC_CLIENT_RENDER_ENGINE_OK);
    NEED(mc_client_default_font_release(font) == MC_CLIENT_FONT_OK);
    NEED(mc_client_render_engine_classes_release(classes) == MC_CLIENT_RENDER_ENGINE_OK);
    free(e);
    return true;
}

int main(int argc, char **argv) {
    if (argc != 2)
        return 2;
    fx_effects f = {0};
    if (!prepare(&f, argv[1]) || !allocation_case(&f))
        return 1;
    if (mc_registry_classes_free(f.registry, NULL) != MC_REGISTRY_OK)
        return 1;
    mc_java_random_profile_free(f.profile);
    mc_materials_destroy(f.materials);
    if (mc_resource_files_free(f.files) != MC_REGISTRY_OK)
        return 1;
    free(f.trig);
    printf("Actual g allocation controls: %u\n", checks);
    return 0;
}
