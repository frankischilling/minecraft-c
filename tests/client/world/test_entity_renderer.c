#include "client/render_engine/internal.h"
#include "mc/client/world/entity_renderer.h"
#include "random_profile_internal.h"
#include <SDL3/SDL_init.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Authored genuine CPU owners. No App storage, fake READY child, device,
 * supplied native seed or constructor callback; all controls remain UNRUN. */
static unsigned checks;
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(x)) {                                                                                \
            fprintf(stderr, "EntityRenderer check%u line%d: %s\n", checks, __LINE__, #x);          \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

static uint64_t bits64(double v) {
    uint64_t bits;
    memcpy(&bits, &v, sizeof(bits));
    return bits;
}

static uint32_t bits32(float v) {
    uint32_t bits;
    memcpy(&bits, &v, sizeof(bits));
    return bits;
}

static int owner_prefixes(bool exhausted) {
    mc_java_random_profile *profile = NULL;
    mc_entity_classes *entities = NULL;
    mc_client_render_classes *models = NULL;
    mc_client_render_engine_classes *graphics = NULL;
    mc_client_entity_renderer_error *error = calloc(1, sizeof(*error));
    CHECK(error != NULL);
    CHECK(mc_java_random_profile_create(SIZE_MAX, &profile) == MC_JAVA_RANDOM_PROFILE_OK);
    if (exhausted)
        CHECK(mc_java_random_profile_bind_constructor_inputs(profile, NULL, 0) ==
              MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_entity_classes_create(MC_ENTITY_CLIENT, profile, NULL, &entities) == MC_ENTITY_OK);
    CHECK(mc_client_render_classes_create(entities, profile, &models) == MC_CLIENT_MODEL_OK);
    CHECK(mc_client_render_engine_classes_create(NULL, &graphics) == MC_CLIENT_RENDER_ENGINE_OK);
    const mc_client_entity_renderer_services services = {models, graphics, profile, NULL};
    mc_client_source_px *raw = NULL;
    CHECK(mc_client_entity_renderer_allocate(&services, NULL, &raw, error) ==
          MC_CLIENT_ENTITY_RENDERER_OK);
    mc_client_entity_renderer_state state;
    CHECK(mc_client_entity_renderer_inspect(raw, &state));
    CHECK(state.phase == MC_CLIENT_ENTITY_RENDERER_RAW && !state.px_class_ready &&
          state.holds == 1 && state.last_store_pc == -1 && state.fields.J == NULL &&
          state.pending_ra == NULL);
    CHECK(mc_client_entity_renderer_construct(raw, NULL, error) ==
          MC_CLIENT_ENTITY_RENDERER_INVALID_PHASE);
    CHECK(mc_client_entity_renderer_retire(&raw, error) == MC_CLIENT_ENTITY_RENDERER_OK &&
          raw == NULL);
    CHECK(mc_client_entity_renderer_class_active_use(models, error) ==
          MC_CLIENT_ENTITY_RENDERER_OK);
    bool reached[12] = {false};
    const size_t count = exhausted ? 1u : 12u;
    for (size_t attempt = 0; attempt < count; ++attempt) {
        const size_t quota = exhausted ? SIZE_MAX : attempt;
        mc_client_entity_renderer_options options = mc_client_entity_renderer_default_options();
        options.arena.max_constructor_allocations = quota;
        mc_client_source_px *owner = NULL;
        const uint64_t before = atomic_load_explicit(&profile->uniquifier, memory_order_relaxed);
        CHECK(mc_client_entity_renderer_allocate(&services, &options, &owner, error) ==
              MC_CLIENT_ENTITY_RENDERER_OK);
        CHECK(mc_client_render_classes_free(models) == MC_CLIENT_MODEL_BUSY);
        const mc_client_entity_renderer_status status =
            mc_client_entity_renderer_construct(owner, NULL, error);
        const uint64_t after = atomic_load_explicit(&profile->uniquifier, memory_order_relaxed);
        CHECK(mc_client_entity_renderer_inspect(owner, &state));
        CHECK(state.phase == MC_CLIENT_ENTITY_RENDERER_FAILED && state.px_class_ready &&
              !state.px_a && state.px_b == 0 && state.fields.j == NULL && state.fields.c == NULL);
        if (exhausted) {
            CHECK(status == MC_CLIENT_ENTITY_RENDERER_RANDOM_FAILED && error->px_pc == 169 &&
                  error->random == MC_JAVA_RANDOM_PROFILE_RANGE && state.last_store_pc == 161 &&
                  state.fields.J == NULL && state.pending_random != NULL &&
                  after == before * UINT64_C(1181783497276652981));
        } else if (quota < 6) {
            static const int32_t pcs[6] = {15, 26, 37, 48, 59, 70};
            static const int32_t prior[6] = {11, 22, 33, 44, 55, 66};
            CHECK(status == MC_CLIENT_ENTITY_RENDERER_LIMIT && error->site.pc == pcs[quota] &&
                  state.last_store_pc == prior[quota] && state.fields.J == NULL && after == before);
            for (size_t i = 0; i < 6; ++i)
                CHECK((state.fields.filters[i] != NULL) == (i < quota));
            reached[quota] = true;
        } else if (quota == 6) {
            CHECK(status == MC_CLIENT_ENTITY_RENDERER_LIMIT && error->site.pc == 165 &&
                  state.last_store_pc == 161 && after == before);
            reached[quota] = true;
        } else {
            CHECK(after == before * UINT64_C(1181783497276652981) && state.fields.J != NULL &&
                  state.fields.J->state <= UINT64_C(0xffffffffffff) &&
                  !state.fields.J->have_next_next_gaussian &&
                  bits64(state.fields.J->next_next_gaussian) == 0);
            if (quota <= 8) {
                CHECK(status == MC_CLIENT_ENTITY_RENDERER_GRAPHICS_FAILED && error->px_pc == 193 &&
                      state.last_store_pc == 187 && state.fields.f == NULL);
                CHECK(strcmp(error->graphics.site.class_name, "ge") == 0);
            } else if (quota == 9) {
                CHECK(status == MC_CLIENT_ENTITY_RENDERER_LIMIT && error->site.pc == 205 &&
                      state.last_store_pc == 201 && state.pending_ra == NULL);
            } else if (quota == 10) {
                CHECK(status == MC_CLIENT_ENTITY_RENDERER_LIMIT && error->site.pc == 41 &&
                      state.ra_last_store_pc == 37 && state.ra_blocks != NULL &&
                      state.pending_iy == NULL);
            } else {
                CHECK(status == MC_CLIENT_ENTITY_RENDERER_NULL_REFERENCE && error->ra_pc == 46 &&
                      state.last_store_pc == 201 && state.ra_last_store_pc == 37 &&
                      state.pending_iy != NULL && state.iy_last_store_pc == -1);
                const mc_client_render_blocks *cv = state.ra_blocks;
                CHECK(cv != NULL && cv->c == NULL && cv->d == -1 && !cv->e && !cv->f && cv->b &&
                      cv->O == 1);
                CHECK(state.ra_b == NULL && state.ra_g == -1 && bits32(state.ra_c) == 0 &&
                      bits32(state.ra_d) == 0);
                CHECK(state.fields.f->capacity == 16 && state.fields.f->position == 0 &&
                      state.fields.f->limit == 16 && state.fields.f->bytes->capacity == 64);
                for (size_t i = 0; i < 64; ++i)
                    CHECK(state.fields.f->bytes->data[i] == 0);
            }
            reached[quota] = true;
        }
        if (quota >= 6) {
            for (size_t i = 0; i < 6; ++i) {
                CHECK(state.fields.filters[i] != NULL && bits32(state.fields.filters[i]->a) == 0 &&
                      bits32(state.fields.filters[i]->b) == 0 &&
                      bits32(state.fields.filters[i]->c) == 0);
                for (size_t j = 0; j < i; ++j)
                    CHECK(state.fields.filters[i] != state.fields.filters[j]);
            }
            CHECK(state.fields.t == 4.0f && state.fields.u == 4.0f && !state.fields.D &&
                  state.fields.E == 1.0 && bits64(state.fields.F) == 0 &&
                  bits64(state.fields.G) == 0);
        }
        if (state.ra_occurrence != NULL) {
            mc_client_held_item *external = state.ra_occurrence;
            const mc_client_source_ra *identity = mc_client_held_item_identity(external);
            CHECK(identity == state.pending_ra);
            CHECK(mc_client_held_item_retain(external, error) == MC_CLIENT_ENTITY_RENDERER_OK);
            CHECK(mc_client_entity_renderer_retire(&owner, error) ==
                      MC_CLIENT_ENTITY_RENDERER_BUSY &&
                  owner != NULL);
            CHECK(mc_client_held_item_identity(external) == identity);
            CHECK(mc_client_held_item_release(&external, error) == MC_CLIENT_ENTITY_RENDERER_OK &&
                  external == NULL);
        }
        CHECK(mc_client_entity_renderer_construct(owner, NULL, error) ==
              MC_CLIENT_ENTITY_RENDERER_INVALID_PHASE);
        mc_client_source_px *other = owner;
        CHECK(mc_client_entity_renderer_retain(other, error) == MC_CLIENT_ENTITY_RENDERER_OK);
        CHECK(mc_client_entity_renderer_retire(&owner, error) == MC_CLIENT_ENTITY_RENDERER_BUSY &&
              owner == other);
        CHECK(mc_client_entity_renderer_release(&other, error) == MC_CLIENT_ENTITY_RENDERER_OK &&
              other == NULL);
        CHECK(mc_client_entity_renderer_retire(&owner, error) == MC_CLIENT_ENTITY_RENDERER_OK &&
              owner == NULL);
    }
    if (!exhausted)
        for (size_t i = 0; i < 12; ++i)
            CHECK(reached[i]);
    CHECK(SDL_WasInit(0) == 0);
    mc_client_render_engine_class_state ge;
    CHECK(mc_client_render_engine_classes_inspect(graphics, &ge) && !ge.ji_ready &&
          ge.native_resource_count == 0 && !ge.resource_retirement_started);
    CHECK(mc_client_render_engine_classes_release(graphics) == MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(mc_client_render_classes_free(models) == MC_CLIENT_MODEL_OK);
    CHECK(mc_entity_classes_free(entities) == MC_ENTITY_OK);
    mc_java_random_profile_free(profile);
    free(error);
    return 0;
}

int main(int argc, char **argv) {
    if (argc != 1) {
        (void)argv;
        fputs("EntityRenderer input: no arguments expected\n", stderr);
        return 2;
    }
    CHECK(owner_prefixes(false) == 0);
    CHECK(owner_prefixes(true) == 0);
    printf("EntityRenderer constructor contracts: %u checks\n", checks);
    return 0;
}
